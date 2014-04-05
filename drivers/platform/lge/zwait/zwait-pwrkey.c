/*
 * LG Zero-Wait pwrkey
 *
 * Copyright (C) 2013 LG Electronics Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/jiffies.h>
#include <linux/input.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/zwait.h>

#include "zwait.h"

static union zw_pwrkey_info zw_pwrkey;
static struct wakeup_source zw_pwrkey_ws;

static unsigned long pwrkey_long_press_jiffies;
static unsigned int pwrkey_long_press_delay = 1500; /* msecs */

static void zw_pwrkey_work_func(struct work_struct *work)
{
	if (is_suspend_force() || (zw_pwrkey.ptr == NULL))
		return;

	if (zw_pwrkey.u->check_func(zw_pwrkey.u->func_param)) {
		input_report_key(zw_pwrkey.u->input, zw_pwrkey.u->code, 1);
		input_report_key(zw_pwrkey.u->input, zw_pwrkey.u->code, 0);
		input_sync(zw_pwrkey.u->input);
		zw_event_report(ZW_EVENT_PWRKEY);
	}
	__pm_relax(&zw_pwrkey_ws);
}

static DECLARE_DELAYED_WORK(zw_dwork, zw_pwrkey_work_func);

static irqreturn_t zw_pwrkey_state_irq_handler(int irq, void *ptr)
{
	cancel_delayed_work_sync(&zw_dwork);
	if (zw_pwrkey.u->check_func(zw_pwrkey.u->func_param)) {
		schedule_delayed_work(&zw_dwork,
				msecs_to_jiffies(pwrkey_long_press_delay));
		__pm_stay_awake(&zw_pwrkey_ws);
	} else {
		__pm_relax(&zw_pwrkey_ws);
	}

	if (!irqs_disabled())
		local_irq_disable();

	return IRQ_HANDLED;
}

static irqreturn_t zw_pwrkey_press_irq_handler(int irq, void *ptr)
{
	pwrkey_long_press_jiffies =
		jiffies + msecs_to_jiffies(pwrkey_long_press_delay);

	__pm_stay_awake(&zw_pwrkey_ws);

	if (!irqs_disabled())
		local_irq_disable();

	return IRQ_HANDLED;
}

static irqreturn_t zw_pwrkey_release_irq_handler(int irq, void *ptr)
{
	if (is_suspend_force())
		return IRQ_HANDLED;

	if (time_after_eq(jiffies, pwrkey_long_press_jiffies)) {
		input_report_key(zw_pwrkey.s->input, zw_pwrkey.s->code, 1);
		input_report_key(zw_pwrkey.s->input, zw_pwrkey.s->code, 0);
		input_sync(zw_pwrkey.s->input);
		zw_event_report(ZW_EVENT_PWRKEY);
	}
	__pm_relax(&zw_pwrkey_ws);

	return IRQ_HANDLED;
}

static inline void zw_pwrkey_irq_wake_disable_all(void)
{
	switch (zw_pwrkey.data->irq_type) {
	case ZW_PWRKEY_UNITE_IRQ:
		if (zw_pwrkey.u->state_irq >= 0) {
			zw_disable_irq_wake(zw_pwrkey.u->state_irq);
		}

		if (zw_pwrkey.u->bark_irq >= 0) {
			zw_disable_irq_wake(zw_pwrkey.u->bark_irq);
		}
		break;

	case ZW_PWRKEY_SEPERATE_IRQ:
		if (zw_pwrkey.s->press_irq >= 0) {
			zw_disable_irq_wake(zw_pwrkey.s->press_irq);
		}

		if (zw_pwrkey.s->release_irq >= 0) {
			zw_disable_irq_wake(zw_pwrkey.s->release_irq);
		}
		break;

	default:
		break;
	}
}

static inline void zw_pwrkey_irq_wake_enable_all(void)
{
	switch (zw_pwrkey.data->irq_type) {
	case ZW_PWRKEY_UNITE_IRQ:
		if (zw_pwrkey.u->state_irq >= 0) {
			zw_enable_irq_wake(zw_pwrkey.u->state_irq);
		}

		if (zw_pwrkey.u->bark_irq >= 0) {
			zw_enable_irq_wake(zw_pwrkey.u->bark_irq);
		}
		break;

	case ZW_PWRKEY_SEPERATE_IRQ:
		if (zw_pwrkey.s->press_irq >= 0) {
			zw_enable_irq_wake(zw_pwrkey.s->press_irq);
		}

		if (zw_pwrkey.s->release_irq >= 0) {
			zw_enable_irq_wake(zw_pwrkey.s->release_irq);
		}
		break;

	default:
		break;
	}
}

static inline void zw_pwrkey_irq_handler_set(void)
{
	switch (zw_pwrkey.data->irq_type) {
	case ZW_PWRKEY_UNITE_IRQ:
		if (zw_pwrkey.u->state_irq_handler) {
			set_irq_handler(zw_pwrkey.u->state_irq,
					zw_pwrkey_state_irq_handler);
		}
		break;

	case ZW_PWRKEY_SEPERATE_IRQ:
		if (zw_pwrkey.s->press_irq_handler) {
			set_irq_handler(zw_pwrkey.s->press_irq,
					zw_pwrkey_press_irq_handler);
		}

		if (zw_pwrkey.s->release_irq_handler) {
			set_irq_handler(zw_pwrkey.s->release_irq,
					zw_pwrkey_release_irq_handler);
		}
		break;

	default:
		break;
	}
}

static inline void zw_pwrkey_irq_handler_clean(void)
{
	switch (zw_pwrkey.data->irq_type) {
	case ZW_PWRKEY_UNITE_IRQ:
		if (zw_pwrkey.u->state_irq_handler) {
			set_irq_handler(zw_pwrkey.u->state_irq,
					zw_pwrkey.u->state_irq_handler);
		}
		break;

	case ZW_PWRKEY_SEPERATE_IRQ:
		if (zw_pwrkey.s->press_irq_handler) {
			set_irq_handler(zw_pwrkey.s->press_irq,
					zw_pwrkey.s->press_irq_handler);
		}

		if (zw_pwrkey.s->release_irq_handler) {
			set_irq_handler(zw_pwrkey.s->release_irq,
					zw_pwrkey.s->release_irq_handler);
		}
		break;

	default:
		break;
	}
}

static inline void zw_pwrkey_set_wakeup_capable(void)
{
	if (zw_pwrkey.data->wdev)
		device_set_wakeup_capable(zw_pwrkey.data->wdev, true);
}

void zw_pwrkey_set(int state)
{
	switch (state) {
	case ZW_STATE_ON_SYSTEM:
		zw_pwrkey_irq_wake_disable_all();
		break;

	case ZW_STATE_ON_USER:
		zw_pwrkey_irq_handler_set();
		zw_pwrkey_set_wakeup_capable();
		break;

	default:
		break;
	}
}

void zw_pwrkey_clean(void)
{
	zw_pwrkey_irq_handler_clean();
	zw_pwrkey_irq_wake_enable_all();
}

void zw_pwrkey_info_register(void *ptr)
{
	if (zw_pwrkey.ptr != NULL) {
		WARN(1, "%s: zw_pwrkey is already registered!\n", __func__);
		return;
	}

	zw_pwrkey.ptr = ptr;
	wakeup_source_init(&zw_pwrkey_ws, "zw_pwrkey_ws");
}

void zw_pwrkey_info_unregister(void)
{
	if (zw_pwrkey.ptr == NULL) {
		WARN(1, "%s: zw_pwrkey is already unregistered!\n", __func__);
		return;
	}
	zw_pwrkey.ptr = NULL;
	wakeup_source_trash(&zw_pwrkey_ws);
}
