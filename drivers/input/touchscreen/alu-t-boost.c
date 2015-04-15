/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
	Based on cpuboost and created by Alucard24@XDA / @Alucard_24
*/

#define pr_fmt(fmt) "alu_t_boost: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <mach/cpufreq.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/smpboot.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/time.h>

/*
 * debug = 1 will print all
 */
static unsigned int debug = 0;
module_param_named(debug_mask, debug, uint, 0644);

#define dprintk(msg...)		\
do {				\
	if (debug)		\
		pr_info(msg);	\
} while (0)

static struct workqueue_struct *touch_boost_wq;
static struct delayed_work input_boost_rem;
static struct work_struct input_boost_work;

static unsigned int input_boost_freq;
module_param(input_boost_freq, uint, 0644);

static unsigned int input_boost_ms = 40;
module_param(input_boost_ms, uint, 0644);

static unsigned int nr_boost_cpus = 4;
module_param(nr_boost_cpus, uint, 0644);

static u64 last_input_time;

static unsigned int min_input_interval = 150;
module_param(min_input_interval, uint, 0644);

static void do_input_boost_rem(struct work_struct *work)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		dprintk("Removing input boost for CPU%u\n", i);
		set_cpu_min_lock(i, 0);
	}
}

static void do_input_boost(struct work_struct *work)
{
	unsigned int i;
	unsigned nr_cpus = nr_boost_cpus;

	cancel_delayed_work_sync(&input_boost_rem);

	if (nr_cpus <= 0)
		nr_cpus = 1;
	else if (nr_cpus > NR_CPUS)
		nr_cpus = NR_CPUS;

	for (i = 0; i < nr_cpus; i++) {
		struct cpufreq_policy policy;
		unsigned int cur = 0;

		dprintk("Input boost for CPU%u\n", i);
		set_cpu_min_lock(i, input_boost_freq);

		if (cpu_online(i)) {
			cur = cpufreq_quick_get(i);
			if (cur < input_boost_freq && cur > 0) {
				policy.cpu = i;
				cpufreq_driver_target(&policy,
					input_boost_freq, CPUFREQ_RELATION_L);
			}
		}
	}

	mod_delayed_work_on(BOOT_CPU, touch_boost_wq,
			&input_boost_rem,
			msecs_to_jiffies(input_boost_ms));
}

static void touchboost_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	u64 now;

	if (!input_boost_freq)
		return;

	now = ktime_to_us(ktime_get());
	if ((now - last_input_time) < (min_input_interval * USEC_PER_MSEC))
		return;

	if (work_pending(&input_boost_work))
		return;

	dprintk("Input boost for input event.\n");

	queue_work(touch_boost_wq, &input_boost_work);
	last_input_time = ktime_to_us(ktime_get());
}

static int touchboost_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = handler->name;

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void touchboost_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id touchboost_ids[] = {
	/* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	/* touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
	{ },
};

static struct input_handler touchboost_input_handler = {
	.event          = touchboost_input_event,
	.connect        = touchboost_input_connect,
	.disconnect     = touchboost_input_disconnect,
	.name           = "alu_t_boost",
	.id_table       = touchboost_ids,
};

static int touch_boost_init(void)
{
	int ret;

	touch_boost_wq = alloc_workqueue("touch_boost_wq", WQ_HIGHPRI, 0);
	if (!touch_boost_wq)
		return -EFAULT;

	INIT_WORK(&input_boost_work, do_input_boost);
	INIT_DELAYED_WORK(&input_boost_rem, do_input_boost_rem);

	ret = input_register_handler(&touchboost_input_handler);
	if (ret)
		pr_err("Cannot register touchboost input handler.\n");

	return ret;
}
late_initcall(touch_boost_init);
