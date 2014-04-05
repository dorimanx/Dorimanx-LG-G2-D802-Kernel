/*
 * LG Zero-Wait event
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

#include <linux/module.h>
#include <linux/switch.h>
#include <linux/slab.h>
#include <linux/zwait.h>

static struct switch_dev *sdev;

void zw_event_report(int event)
{
	if (is_zw_mode()) {
		switch_set_state(sdev, event);
		switch_set_state(sdev, ZW_EVENT_NONE);
	}
}

static ssize_t print_event_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(sdev));
}

static int __init zw_event_init(void)
{
	int ret = 0;

	sdev = kzalloc(sizeof(struct switch_dev), GFP_KERNEL);
	if (sdev == NULL)
		return -ENOMEM;

	sdev->name = "zwait";
	sdev->print_state = print_event_state;

	ret = switch_dev_register(sdev);
	if (ret)
		kfree(sdev);

	return ret;
}

static void __exit zw_event_exit(void)
{
	switch_dev_unregister(sdev);
	kfree(sdev);
}

module_init(zw_event_init);
module_exit(zw_event_exit);

MODULE_DESCRIPTION("LG Zero-Wait event");
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_LICENSE("GPL");
