/*
 * LG Zero-Wait power
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

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/zwait.h>

#include "zwait.h"

struct zw_psy {
	struct list_head entry;
	struct power_supply *psy;
};

struct zw_psy_ws {
	struct list_head entry;
	struct wakeup_source *ws;
};

static LIST_HEAD(zw_psy_list);
static LIST_HEAD(zw_psy_ws_list);
static DEFINE_MUTEX(zw_psy_list_mtx);
static DEFINE_MUTEX(zw_psy_ws_list_mtx);

static bool goto_suspend_force = false;

static inline void zw_psy_ws_relax(void)
{
	struct zw_psy_ws *data;

	mutex_lock(&zw_psy_ws_list_mtx);
	list_for_each_entry(data, &zw_psy_ws_list, entry) {
		__pm_relax(data->ws);
	}
	mutex_unlock(&zw_psy_ws_list_mtx);
}

void zw_psy_irq_handler(int attach)
{
	if (is_zw_mode()) {
		if (attach) {
			zw_event_report(ZW_EVENT_CHARGER);
		} else {
			zw_led_turn_off();
			zw_psy_ws_relax();
		}
	}
}

static inline int check_type(enum power_supply_type type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_MAINS:
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_ACA:
	case POWER_SUPPLY_TYPE_BATTERY:
#ifdef CONFIG_WIRELESS_CHARGER
	case POWER_SUPPLY_TYPE_WIRELESS:
#endif
		return 0;

	default:
		pr_err("%s: Not support type(%d)\n", __func__, type);
		return -EINVAL;
	}
}

void zw_psy_update(void)
{
	struct zw_psy *data;

	mutex_lock(&zw_psy_list_mtx);
	list_for_each_entry(data, &zw_psy_list, entry) {
		power_supply_changed(data->psy);
	}
	mutex_unlock(&zw_psy_list_mtx);
}

int zw_psy_wakeup_source_register(struct wakeup_source *ws)
{
	struct zw_psy_ws *data;

	if (ws == NULL)
		return -EINVAL;

	data = kzalloc(sizeof(struct zw_psy_ws), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->ws = ws;

	mutex_lock(&zw_psy_ws_list_mtx);
	list_add_tail(&data->entry, &zw_psy_ws_list);
	mutex_unlock(&zw_psy_ws_list_mtx);

	return 0;
}

void zw_psy_wakeup_source_unregister(struct wakeup_source *ws)
{
	struct zw_psy_ws *data;

	if (ws == NULL)
		return;

	mutex_lock(&zw_psy_ws_list_mtx);
	list_for_each_entry(data, &zw_psy_ws_list, entry) {
		if (data->ws == ws) {
			list_del(&data->entry);
			kfree(data);
			break;
		}
	}
	mutex_unlock(&zw_psy_ws_list_mtx);
}

int zw_power_supply_register(struct power_supply *psy)
{
	struct zw_psy *data;

	if (psy == NULL)
		return -EINVAL;

	if (check_type(psy->type))
		return 0;

	data = kzalloc(sizeof(struct zw_psy), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->psy = psy;

	mutex_lock(&zw_psy_list_mtx);
	list_add_tail(&data->entry, &zw_psy_list);
	mutex_unlock(&zw_psy_list_mtx);

	return 0;
}

void zw_power_supply_unregister(struct power_supply *psy)
{
	struct zw_psy *data;

	if (psy == NULL)
		return;

	if (check_type(psy->type))
		return;

	mutex_lock(&zw_psy_list_mtx);
	list_for_each_entry(data, &zw_psy_list, entry) {
		if (data->psy == psy) {
			list_del(&data->entry);
			kfree(data);
			break;
		}
	}
	mutex_unlock(&zw_psy_list_mtx);
}

static int zw_pm_notifier(struct notifier_block *nb,
			unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		/* check again to ensure */
		if (!power_supply_is_system_supplied()) {
			zw_led_turn_off();
			zw_psy_ws_relax();
		}

		if (goto_suspend_force == true) {
			goto_suspend_force = false;
			return NOTIFY_BAD;
		}
		break;

	default:
		break;
	}
	return NOTIFY_DONE;
}

int zw_no_charger_in_zwait(void)
{
	if (is_zw_mode()) {
		if (!power_supply_is_system_supplied())
			return 1;
	}

	return 0;
}

static struct notifier_block zw_pm_nb = {
	.notifier_call = zw_pm_notifier,
};

bool is_suspend_force(void)
{
	return goto_suspend_force;
}

void zw_power_pm_set(void)
{
	goto_suspend_force = true;
	register_pm_notifier(&zw_pm_nb);
}

void zw_power_pm_clean(void)
{
	goto_suspend_force = false;
	unregister_pm_notifier(&zw_pm_nb);
}

void zw_queue_up_suspend_work(suspend_state_t state)
{
	if (is_zw_mode()) {
		if (goto_suspend_force && (state < PM_SUSPEND_MAX)) {
			pm_suspend(state);
		}
	}
}
