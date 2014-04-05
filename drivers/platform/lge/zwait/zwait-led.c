/*
 * LG Zero-Wait LED
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

#include <leds.h>	/* drivers/leds/ */

struct zw_led {
	struct list_head entry;
	struct led_classdev *cdev;
};

static LIST_HEAD(zw_led_list);
static DEFINE_MUTEX(zw_led_list_mtx);

void zw_led_turn_off(void)
{
	struct zw_led *data;

	mutex_lock(&zw_led_list_mtx);
	list_for_each_entry(data, &zw_led_list, entry) {
		led_trigger_remove(data->cdev);
		led_set_brightness(data->cdev, LED_OFF);
	}
	mutex_unlock(&zw_led_list_mtx);
}

int zw_led_register(struct led_classdev *cdev)
{
	struct zw_led *data;

	if (cdev == NULL)
		return -EINVAL;

	data = kzalloc(sizeof(struct led_classdev), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->cdev = cdev;

	mutex_lock(&zw_led_list_mtx);
	list_add_tail(&data->entry, &zw_led_list);
	mutex_unlock(&zw_led_list_mtx);

	return 0;
}

void zw_led_unregister(struct led_classdev *cdev)
{
	struct zw_led *data;

	if (cdev == NULL)
		return;

	mutex_lock(&zw_led_list_mtx);
	list_for_each_entry(data, &zw_led_list, entry) {
		if (data->cdev == cdev) {
			list_del(&data->entry);
			data->cdev = NULL;
			kfree(data);
			break;
		}
	}
	mutex_unlock(&zw_led_list_mtx);
}
