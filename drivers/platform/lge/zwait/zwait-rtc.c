/*
 * LG Zero-Wait RTC
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

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/zwait.h>
#include "zwait.h"

struct zw_rtc {
	struct list_head entry;
	struct rtc_device *rtc;
	struct rtc_task *task;
};

static LIST_HEAD(zw_rtc_list);
static DEFINE_MUTEX(zw_rtc_list_mtx);

static unsigned long zw_timeout_delay = (15 * 60UL);	/* secs */
static int zw_rtc_retry = 3;
static int zw_rtc_retry_delay = 200;	/* msecs */

static void zw_rtc_work_func(struct work_struct *work);
static DECLARE_WORK(zw_rtc_work, zw_rtc_work_func);

unsigned long get_zw_timeout_delay(void)
{
	return zw_timeout_delay;
}

void set_zw_timeout_delay(unsigned long sec)
{
	zw_timeout_delay = sec;
}

int get_zw_rtc_retry(void)
{
	return zw_rtc_retry;
}

void set_zw_rtc_retry(int max)
{
	zw_rtc_retry = max;
}

int get_zw_rtc_retry_delay(void)
{
	return zw_rtc_retry_delay;
}

void set_zw_rtc_retry_delay(int msec)
{
	zw_rtc_retry_delay = msec;
}

static void zw_rtc_irq_handler(void *private_data)
{
	schedule_work(&zw_rtc_work);
}

static struct rtc_task zw_rtc_task = {
	.func = zw_rtc_irq_handler,
};

static inline void zw_rtc_task_set(void)
{
	struct zw_rtc *data;
	unsigned long flags;

	mutex_lock(&zw_rtc_list_mtx);
	list_for_each_entry(data, &zw_rtc_list, entry) {
		spin_lock_irqsave(&data->rtc->irq_task_lock, flags);
		data->task = data->rtc->irq_task;
		data->rtc->irq_task = NULL;
		spin_unlock_irqrestore(&data->rtc->irq_task_lock, flags);
	}

	data = list_first_entry(&zw_rtc_list, struct zw_rtc, entry);
	spin_lock_irqsave(&data->rtc->irq_task_lock, flags);
	data->rtc->irq_task = &zw_rtc_task;
	spin_unlock_irqrestore(&data->rtc->irq_task_lock, flags);
	mutex_unlock(&zw_rtc_list_mtx);
}

static inline void zw_rtc_task_clean(void)
{
	struct zw_rtc *data;
	unsigned long flags;

	mutex_lock(&zw_rtc_list_mtx);
	list_for_each_entry(data, &zw_rtc_list, entry) {
		spin_lock_irqsave(&data->rtc->irq_task_lock, flags);
		data->rtc->irq_task = data->task;
		data->task = NULL;
		spin_unlock_irqrestore(&data->rtc->irq_task_lock, flags);
	}
	mutex_unlock(&zw_rtc_list_mtx);
}

static inline void __zw_rtc_read_time(struct rtc_device *rtc,
				struct rtc_time *tm)
{
	int i;

	for (i = 0; i < zw_rtc_retry; i++) {
		if (rtc_read_time(rtc, tm) == 0)
			return;

		pr_err("%s: fail to rtc_read_time() (try = %d)",
			__func__, i + 1);
		if (i < (zw_rtc_retry - 1))
			mdelay(zw_rtc_retry_delay);
	}

	kernel_power_off();
}

static inline void __zw_rtc_set_alarm(struct rtc_device *rtc,
				struct rtc_wkalrm *alarm)
{
	int i;

	for (i = 0; i < zw_rtc_retry; i++) {
		if (rtc_set_alarm(rtc, alarm) == 0)
			return;

		pr_err("%s: fail to rtc_set_alarm() (try = %d)",
			__func__, i + 1);
		if (i < (zw_rtc_retry - 1))
			mdelay(zw_rtc_retry_delay);
	}

	kernel_power_off();
}

static inline void zw_rtc_set_alarm(bool enable)
{
	struct zw_rtc *data;
	struct rtc_wkalrm tmp;
	unsigned long time;

	memset(&tmp, 0, sizeof(struct rtc_wkalrm));

	mutex_lock(&zw_rtc_list_mtx);
	data = list_first_entry(&zw_rtc_list, struct zw_rtc, entry);

	__zw_rtc_read_time(data->rtc, &tmp.time);
	rtc_tm_to_time(&tmp.time, &time);

	if (enable) {
		tmp.enabled = 1;
		time += zw_timeout_delay;
	}

	rtc_time_to_tm(time, &tmp.time);
	__zw_rtc_set_alarm(data->rtc, &tmp);

	mutex_unlock(&zw_rtc_list_mtx);
}

static void zw_rtc_work_func(struct work_struct *work)
{
	pr_info("Timeout in Zero Wait mode!\n");

	zw_rtc_set_alarm(0);
	zw_rtc_task_clean();
	kernel_power_off();
}

void zw_rtc_set(void)
{
	zw_rtc_task_set();
	zw_rtc_set_alarm(1);
}

void zw_rtc_clean(void)
{
	zw_rtc_set_alarm(0);
	zw_rtc_task_clean();
}

int zw_rtc_info_register(struct rtc_device *rtc)
{
	struct zw_rtc *data;

	if (rtc == NULL)
		return -EINVAL;

	data = kzalloc(sizeof(struct zw_rtc), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->rtc = rtc;

	mutex_lock(&zw_rtc_list_mtx);
	list_add_tail(&data->entry, &zw_rtc_list);
	mutex_unlock(&zw_rtc_list_mtx);

	return 0;
}

void zw_rtc_info_unregister(struct rtc_device *rtc)
{
	struct zw_rtc *data;

	if (rtc == NULL)
		return;

	mutex_lock(&zw_rtc_list_mtx);
	list_for_each_entry(data, &zw_rtc_list, entry) {
		if (data->rtc == rtc) {
			list_del(&data->entry);
			data->rtc = NULL;
			data->task = NULL;
			kfree(data);
			break;
		}
	}
	mutex_unlock(&zw_rtc_list_mtx);
}
