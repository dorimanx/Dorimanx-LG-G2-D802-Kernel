/*
 * MSM Hotplug Driver
 *
 * Copyright (C) 2013 Fluxi <linflux@arcor.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/lcd_notify.h>
#include <linux/input.h>

#define MSM_HOTPLUG		"msm_hotplug"
#define DEFAULT_UPDATE_RATE	HZ / 10
#define START_DELAY		HZ * 20
#define NUM_LOAD_LEVELS		5
#define MIN_INPUT_INTERVAL	150 * 1000L
#define DEFAULT_HISTORY_SIZE	10
#define DEFAULT_DOWN_LOCK_DUR	2500
#define DEFAULT_SUSPEND_FREQ	1036800
#define DEFAULT_NR_CPUS_BOOSTED	2
#define DEFAULT_MIN_CPUS_ONLINE	1
#define DEFAULT_MAX_CPUS_ONLINE	NR_CPUS

static unsigned int debug = 0;
module_param_named(debug_mask, debug, uint, 0644);

#define dprintk(msg...)		\
do { 				\
	if (debug)		\
		pr_info(msg);	\
} while (0)

static struct cpu_hotplug {
	unsigned int suspend_freq;
	atomic_t target_cpus;
	unsigned int min_cpus_online;
	unsigned int max_cpus_online;
	unsigned int cpus_boosted;
	atomic_t down_lock;
	unsigned int down_lock_dur;
	u64 last_input_time;
	struct work_struct up_work;
	struct work_struct down_work;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	struct timer_list lock_timer;
	struct notifier_block notif;
	bool screen_on;
} hotplug = {
	.suspend_freq = DEFAULT_SUSPEND_FREQ,
	.min_cpus_online = DEFAULT_MIN_CPUS_ONLINE,
	.max_cpus_online = DEFAULT_MAX_CPUS_ONLINE,
	.cpus_boosted = DEFAULT_NR_CPUS_BOOSTED,
	.down_lock = ATOMIC_INIT(0),
	.down_lock_dur = DEFAULT_DOWN_LOCK_DUR,
	.screen_on = true
};

static struct workqueue_struct *hotplug_wq;
static struct delayed_work hotplug_work;

static u64 last_input_time;

static struct cpu_stats {
	unsigned int update_rate;
	unsigned int *load_hist;
	unsigned int hist_size;
	unsigned int hist_cnt;
	unsigned int min_cpus;
	unsigned int total_cpus;
	unsigned int online_cpus;
	unsigned int current_load;
} stats = {
	.update_rate = DEFAULT_UPDATE_RATE,
	.hist_size = DEFAULT_HISTORY_SIZE,
	.min_cpus = 1,
	.total_cpus = NR_CPUS
};

static uint32_t limited_max_freq = UINT_MAX;
static uint32_t limited_min_freq;

static int msm_hotplug_cpufreq_callback(struct notifier_block *nfb,
					unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;

	switch (event) {
	case CPUFREQ_INCOMPATIBLE:
		cpufreq_verify_within_limits(policy, limited_min_freq,
					     limited_max_freq);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block msm_hotplug_cpufreq_notifier = {
	.notifier_call = msm_hotplug_cpufreq_callback,
};

extern unsigned int report_load_at_max_freq(void);

static struct cpu_stats *get_load_stats(void)
{
	unsigned int i, j;
	unsigned int load = 0;
	struct cpu_stats *st = &stats;

	st->online_cpus = num_online_cpus();
	st->load_hist[st->hist_cnt] = report_load_at_max_freq();

	for (i = 0, j = st->hist_cnt; i < st->hist_size; i++, j--) {
		load += st->load_hist[j];

		if (j == 0)
			j = st->hist_size;
	}

	if (++st->hist_cnt == st->hist_size)
		st->hist_cnt = 0;

	st->current_load = load / st->hist_size;

	return st;
}
EXPORT_SYMBOL_GPL(get_load_stats);

struct load_thresh_tbl {
	unsigned int up_threshold;
	unsigned int down_threshold;
};

#define LOAD_SCALE(u, d)             \
{                          	     \
		.up_threshold = u,   \
		.down_threshold = d, \
}

static struct load_thresh_tbl load[] = {
	LOAD_SCALE(400, 0),
	LOAD_SCALE(50, 0),
	LOAD_SCALE(100, 40),
	LOAD_SCALE(150, 80),
	LOAD_SCALE(410, 140),
};

static void apply_down_lock(void)
{
	atomic_set(&hotplug.down_lock, 1);
	mod_timer(&hotplug.lock_timer,
		  jiffies + msecs_to_jiffies(hotplug.down_lock_dur));
}
EXPORT_SYMBOL_GPL(apply_down_lock);

static void handle_lock_timer(unsigned long data)
{
	atomic_set(&hotplug.down_lock, 0);
}
EXPORT_SYMBOL_GPL(handle_lock_timer);

static void __ref cpu_up_work(struct work_struct *work)
{
	int cpu;
	int target;

	target = atomic_read(&hotplug.target_cpus);

	for_each_cpu_not(cpu, cpu_online_mask) {
		if (target == num_online_cpus())
			break;
		if (cpu == 0)
			continue;
		cpu_up(cpu);
	}
}
EXPORT_SYMBOL_GPL(cpu_up_work);

static void cpu_down_work(struct work_struct *work)
{
	int cpu;
	int lock, target;

	lock = atomic_read(&hotplug.down_lock);
	if (lock)
		return;

	target = atomic_read(&hotplug.target_cpus);

	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		cpu_down(cpu);
		if (target == num_online_cpus())
			break;
	}
}
EXPORT_SYMBOL_GPL(cpu_down_work);

static void online_cpu(unsigned int target)
{
	atomic_set(&hotplug.target_cpus, target);
	apply_down_lock();
	queue_work_on(0, hotplug_wq, &hotplug.up_work);
}
EXPORT_SYMBOL_GPL(online_cpu);

static void offline_cpu(unsigned int target)
{
	atomic_set(&hotplug.target_cpus, target);
	queue_work_on(0, hotplug_wq, &hotplug.down_work);
}
EXPORT_SYMBOL_GPL(offline_cpu);

static int reschedule_hotplug_work(void)
{
	return queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
				     stats.update_rate);
}
EXPORT_SYMBOL_GPL(reschedule_hotplug_work);

static void msm_hotplug_work(struct work_struct *work)
{
	unsigned int cur_load, online_cpus, target = 0;
	unsigned int i;
	struct cpu_stats *st = get_load_stats();
	struct cpu_hotplug *hp = &hotplug;

	cur_load = st->current_load;
	online_cpus = st->online_cpus;

	/* if nr of cpus locked, break out early */
	if (hp->min_cpus_online == num_possible_cpus()) {
		if (online_cpus != hp->min_cpus_online)
			online_cpu(hp->min_cpus_online);
		goto reschedule;
	} else if (hp->max_cpus_online == st->min_cpus) {
		if (online_cpus != hp->max_cpus_online)
			offline_cpu(hp->max_cpus_online);
		goto reschedule;
	}

	for (i = st->min_cpus; i < NUM_LOAD_LEVELS; i++) {
		if (cur_load <= load[i].up_threshold
		    && cur_load > load[i].down_threshold) {
			target = i;
			break;
		}
	}

	if (target > hp->max_cpus_online)
		target = hp->max_cpus_online;
	else if (target < hp->min_cpus_online)
		target = hp->min_cpus_online;

	if (online_cpus != target) {
		if (target > online_cpus)
			online_cpu(target);
		else if (target < online_cpus)
			offline_cpu(target);
	}

	dprintk("%s: cur_load: %3u online_cpus: %u target: %u\n", MSM_HOTPLUG,
		cur_load, online_cpus, target);

reschedule:
	reschedule_hotplug_work();
}
EXPORT_SYMBOL_GPL(msm_hotplug_work);

static void msm_hotplug_suspend(struct work_struct *work)
{
	unsigned int cpu = 0;

	hotplug.screen_on = 0;

	flush_workqueue(hotplug_wq);
	cancel_delayed_work_sync(&hotplug_work);

	atomic_set(&hotplug.down_lock, 0);
	offline_cpu(stats.min_cpus);

	get_online_cpus();
	for_each_online_cpu(cpu) {
		if (cpufreq_update_policy(cpu))
			pr_info("%s: Unable to update policy for cpu:%d\n",
				MSM_HOTPLUG, cpu);
	}
	put_online_cpus();

	pr_info("%s: Suspend - max freq: %dMHz\n", MSM_HOTPLUG,
		hotplug.suspend_freq / 1000);
}
EXPORT_SYMBOL_GPL(msm_hotplug_suspend);

static void msm_hotplug_resume(struct work_struct *work)
{
	unsigned int cpu = 0;
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return;

	hotplug.screen_on = 1;

	online_cpu(stats.total_cpus);

	get_online_cpus();
	for_each_online_cpu(cpu) {
		if (cpufreq_update_policy(cpu))
			pr_info("%s: Unable to update policy for cpu:%d\n",
				MSM_HOTPLUG, cpu);
	}
	put_online_cpus();

	pr_info("%s: Resume - restore max frequency: %dMHz\n",
		MSM_HOTPLUG, policy->max / 1000);

	reschedule_hotplug_work();
}
EXPORT_SYMBOL_GPL(msm_hotplug_resume);

static int lcd_notifier_callback(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	switch (event) {
	case LCD_EVENT_ON_START:
		schedule_work(&hotplug.resume_work);
		break;
	case LCD_EVENT_OFF_END:
		schedule_work(&hotplug.suspend_work);
		break;
	default:
		break;
	}

	return 0;
}

static void hotplug_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value)
{
	u64 now;

	if (num_online_cpus() >= hotplug.cpus_boosted)
		return;

	now = ktime_to_us(ktime_get());
	if (now - last_input_time < MIN_INPUT_INTERVAL)
		return;

	dprintk("%s: online_cpus: %u boosted\n", MSM_HOTPLUG,
		stats.online_cpus);

	online_cpu(hotplug.cpus_boosted);
	last_input_time = ktime_to_us(ktime_get());
}
EXPORT_SYMBOL_GPL(hotplug_input_event);

static int hotplug_input_connect(struct input_handler *handler,
				 struct input_dev *dev,
				 const struct input_device_id *id)
{
	struct input_handle *handle;
	int err;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "msm-hotplug";

	err = input_register_handle(handle);
	if (err)
		goto err_register;

	err = input_open_device(handle);
	if (err)
		goto err_open;

	return 0;
err_register:
	input_unregister_handle(handle);
err_open:
	kfree(handle);
	return err;
}
EXPORT_SYMBOL_GPL(hotplug_input_connect);

static void hotplug_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}
EXPORT_SYMBOL_GPL(hotplug_input_disconnect);

static const struct input_device_id hotplug_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler hotplug_input_handler = {
	.event		= hotplug_input_event,
	.connect	= hotplug_input_connect,
	.disconnect	= hotplug_input_disconnect,
	.name		= MSM_HOTPLUG,
	.id_table	= hotplug_ids,
};

/************************** sysfs interface ************************/

static ssize_t show_suspend_freq(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", hotplug.suspend_freq);
}

static ssize_t store_suspend_freq(struct device *dev,
				  struct device_attribute *msm_hotplug_attrs,
				  const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int val = 0, cpu = 0;
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return -EINVAL;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < policy->min || val > policy->max)
		return -EINVAL;

	hotplug.suspend_freq = val;

	return ret;
}

static ssize_t show_down_lock_duration(struct device *dev,
				       struct device_attribute
				       *msm_hotplug_attrs, char *buf)
{
	return sprintf(buf, "%u\n", hotplug.down_lock_dur);
}

static ssize_t store_down_lock_duration(struct device *dev,
					struct device_attribute
					*msm_hotplug_attrs, const char *buf,
					size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.suspend_freq = val;

	return ret;
}

static ssize_t show_update_rate(struct device *dev,
				struct device_attribute *msm_hotplug_attrs,
				char *buf)
{
	return sprintf(buf, "%u\n", stats.update_rate);
}

static ssize_t store_update_rate(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	stats.update_rate = val;

	return ret;
}

static ssize_t show_load_levels(struct device *dev,
				struct device_attribute *msm_hotplug_attrs,
				char *buf)
{
	int i, len = 0;

	if (!buf)
		return -EINVAL;

	for (i = 0; i < NUM_LOAD_LEVELS; i++) {
		len += sprintf(buf + len, "%u ", i);
		len += sprintf(buf + len, "%u ", load[i].up_threshold);
		len += sprintf(buf + len, "%u\n", load[i].down_threshold);
	}

	return len;
}

static ssize_t store_load_levels(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 const char *buf, size_t count)
{
	int ret;
	unsigned int val[3];

	ret = sscanf(buf, "%u %u %u", &val[0], &val[1], &val[2]);
	if (ret != ARRAY_SIZE(val))
		return -EINVAL;

	load[val[0]].up_threshold = val[1];
	load[val[0]].down_threshold = val[2];

	return ret;
}

static ssize_t show_history_size(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", stats.hist_size);
}

static ssize_t store_history_size(struct device *dev,
				  struct device_attribute *msm_hotplug_attrs,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct cpu_stats *st = &stats;
	struct cpu_hotplug *hp = &hotplug;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val == 0)
		return -EINVAL;

	flush_workqueue(hotplug_wq);
	cancel_delayed_work_sync(&hotplug_work);

	kfree(st->load_hist);
	st->hist_size = val;

	st->load_hist = kmalloc(sizeof(st->hist_size), GFP_KERNEL);
	if (!st->load_hist)
		return -ENOMEM;

	if (!hp->screen_on)
		reschedule_hotplug_work();

	return ret;
}

static ssize_t show_min_cpus_online(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    char *buf)
{
	return sprintf(buf, "%u\n", hotplug.min_cpus_online);
}

static ssize_t store_min_cpus_online(struct device *dev,
				     struct device_attribute *msm_hotplug_attrs,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct cpu_hotplug *hp = &hotplug;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	if (hp->max_cpus_online < val)
		hp->max_cpus_online = val;
	hp->min_cpus_online = val;
	atomic_set(&hp->down_lock, 0);
	offline_cpu(val);

	return ret;
}

static ssize_t show_max_cpus_online(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    char *buf)
{
	return sprintf(buf, "%u\n",hotplug.max_cpus_online);
}

static ssize_t store_max_cpus_online(struct device *dev,
				     struct device_attribute *msm_hotplug_attrs,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct cpu_hotplug *hp = &hotplug;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	if (hp->min_cpus_online > val)
		hp->min_cpus_online = val;
	hp->max_cpus_online = val;
	atomic_set(&hp->down_lock, 0);
	online_cpu(val);

	return ret;
}

static ssize_t show_cpus_boosted(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", hotplug.cpus_boosted);
}

static ssize_t store_cpus_boosted(struct device *dev,
				  struct device_attribute *msm_hotplug_attrs,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.cpus_boosted = val;

	return ret;
}

static ssize_t show_current_load(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	unsigned int load;

	if (hotplug.screen_on)
		load = 0;
	else
		load = stats.current_load;

	return sprintf(buf, "%u\n", load);
}

static DEVICE_ATTR(suspend_freq, 644, show_suspend_freq, store_suspend_freq);
static DEVICE_ATTR(down_lock_duration, 644, show_down_lock_duration,
		   store_down_lock_duration);
static DEVICE_ATTR(update_rate, 644, show_update_rate, store_update_rate);
static DEVICE_ATTR(load_levels, 644, show_load_levels, store_load_levels);
static DEVICE_ATTR(history_size, 644, show_history_size, store_history_size);
static DEVICE_ATTR(min_cpus_online, 644, show_min_cpus_online,
		   store_min_cpus_online);
static DEVICE_ATTR(max_cpus_online, 644, show_max_cpus_online,
		   store_max_cpus_online);
static DEVICE_ATTR(cpus_boosted, 644, show_cpus_boosted, store_cpus_boosted);
static DEVICE_ATTR(current_load, 444, show_current_load, NULL);

static struct attribute *msm_hotplug_attrs[] = {
	&dev_attr_suspend_freq.attr,
	&dev_attr_down_lock_duration.attr,
	&dev_attr_update_rate.attr,
	&dev_attr_load_levels.attr,
	&dev_attr_history_size.attr,
	&dev_attr_min_cpus_online.attr,
	&dev_attr_max_cpus_online.attr,
	&dev_attr_cpus_boosted.attr,
	&dev_attr_current_load.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = msm_hotplug_attrs,
};

/************************** sysfs end ************************/

static int __init msm_hotplug_init(void)
{
	int ret = 0;
	struct cpu_stats *st = &stats;
	struct cpu_hotplug *hp = &hotplug;

	hotplug_wq = alloc_workqueue("msm_hotplug_wq", WQ_HIGHPRI, 0);
	if (!hotplug_wq) {
		pr_err("%s: Creation of hotplug work failed\n", MSM_HOTPLUG);
		return -ENOMEM;
	}

	ret = cpufreq_register_notifier(&msm_hotplug_cpufreq_notifier,
					CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		pr_err("%s: cannot register cpufreq notifier\n", MSM_HOTPLUG);

	INIT_DELAYED_WORK(&hotplug_work, msm_hotplug_work);
	INIT_WORK(&hp->up_work, cpu_up_work);
	INIT_WORK(&hp->down_work, cpu_down_work);
	INIT_WORK(&hp->suspend_work, msm_hotplug_suspend);
	INIT_WORK(&hp->resume_work, msm_hotplug_resume);

	queue_delayed_work_on(0, hotplug_wq, &hotplug_work, START_DELAY);

	st->load_hist = kmalloc(sizeof(st->hist_size), GFP_KERNEL);
	if (!st->load_hist)
		return -ENOMEM;

	setup_timer(&hp->lock_timer, handle_lock_timer, 0);

	return ret;
}
EXPORT_SYMBOL_GPL(msm_hotplug_init);

late_initcall(msm_hotplug_init);

static struct platform_device msm_hotplug_device = {
	.name = MSM_HOTPLUG,
	.id = -1,
};

static int __init msm_hotplug_device_init(void)
{
	int ret;
	struct kobject *module_kobj;

	ret = platform_device_register(&msm_hotplug_device);
	if (ret) {
		pr_err("%s: Device init failed: %d\n", MSM_HOTPLUG, ret);
		return ret;
	}

	module_kobj = kset_find_obj(module_kset, MSM_HOTPLUG);
	if (!module_kobj) {
		pr_err("%s: Cannot find kobject for module\n", MSM_HOTPLUG);
		return -ENOENT;
	}

	ret = sysfs_create_group(module_kobj, &attr_group);
	if (ret)
		return pr_err("%s: Creation of sysfs: %d\n", MSM_HOTPLUG, ret);

	hotplug.notif.notifier_call = lcd_notifier_callback;
	if (lcd_register_client(&hotplug.notif) != 0)
		pr_err("%s: LCD notifier callback failed\n", __func__);

	ret = input_register_handler(&hotplug_input_handler);
	if (ret)
		return pr_err("%s: Faileds to register input handler: %d\n",
			      MSM_HOTPLUG, ret);

	pr_info("%s: Device init\n", MSM_HOTPLUG);

	return ret;
}
EXPORT_SYMBOL_GPL(msm_hotplug_device_init);

static void __exit msm_hotplug_device_exit(void)
{
	destroy_workqueue(hotplug_wq);
	input_unregister_handler(&hotplug_input_handler);
	del_timer(&hotplug.lock_timer);
	kfree(stats.load_hist);
}
EXPORT_SYMBOL_GPL(msm_hotplug_device_exit);

late_initcall(msm_hotplug_device_init);
module_exit(msm_hotplug_device_exit);

MODULE_AUTHOR("Fluxi <linflux@arcor.de>");
MODULE_DESCRIPTION("MSM Hotplug Driver");
MODULE_LICENSE("GPL");
