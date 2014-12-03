/*
 * Intelli Hotplug Driver
 *
 * Copyright (c) 2013-2014, Paul Reioux <reioux@gmail.com>
 * Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/kobject.h>
#ifdef CONFIG_LCD_NOTIFY
#include <linux/lcd_notify.h>
#elif defined(CONFIG_POWERSUSPEND)
#include <linux/powersuspend.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/cpufreq.h>

#define INTELLI_PLUG			"intelli_plug"
#define INTELLI_PLUG_MAJOR_VERSION	5
#define INTELLI_PLUG_MINOR_VERSION	2

#define DEF_SAMPLING_MS			30
#define RESUME_SAMPLING_MS		HZ / 10
#define START_DELAY_MS			HZ * 5
#define MIN_INPUT_INTERVAL		150 * 1000L
#define BOOST_LOCK_DUR			500 * 1000L
#define DEFAULT_NR_CPUS_BOOSTED		2
#define DEFAULT_MIN_CPUS_ONLINE		1
#define DEFAULT_MAX_CPUS_ONLINE		NR_CPUS
#define DEFAULT_NR_FSHIFT		DEFAULT_MAX_CPUS_ONLINE - 1
#define DEFAULT_DOWN_LOCK_DUR		1000
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
#define DEFAULT_SUSPEND_DEFER_TIME	10
#endif

#define CAPACITY_RESERVE		50
#if defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_ARCH_APQ8064) || \
defined(CONFIG_ARCH_MSM8974)
#define THREAD_CAPACITY			(339 - CAPACITY_RESERVE)
#elif defined(CONFIG_ARCH_MSM8226) || defined (CONFIG_ARCH_MSM8926) || \
defined (CONFIG_ARCH_MSM8610) || defined (CONFIG_ARCH_MSM8228)
#define THREAD_CAPACITY			(190 - CAPACITY_RESERVE)
#else
#define THREAD_CAPACITY			(250 - CAPACITY_RESERVE)
#endif
#define CPU_NR_THRESHOLD		((THREAD_CAPACITY << 1) + \
					(THREAD_CAPACITY / 2))
#define MULT_FACTOR			4
#define DIV_FACTOR			100000

static u64 last_boost_time, last_input;

static struct delayed_work intelli_plug_work;
static struct work_struct up_down_work;
static struct workqueue_struct *intelliplug_wq;
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
static struct workqueue_struct *susp_wq;
static struct delayed_work suspend_work;
static struct work_struct resume_work;
static struct mutex intelli_plug_mutex;
#ifdef CONFIG_LCD_NOTIFY
static struct notifier_block notif;
#endif
#endif

struct ip_cpu_info {
	unsigned long cpu_nr_running;
};
static DEFINE_PER_CPU(struct ip_cpu_info, ip_info);

/* HotPlug Driver controls */
static atomic_t intelli_plug_active = ATOMIC_INIT(0);
static unsigned int cpus_boosted = DEFAULT_NR_CPUS_BOOSTED;
static unsigned int min_cpus_online = DEFAULT_MIN_CPUS_ONLINE;
static unsigned int max_cpus_online = DEFAULT_MAX_CPUS_ONLINE;
static unsigned int full_mode_profile = 0; /* balance profile */
static unsigned int cpu_nr_run_threshold = CPU_NR_THRESHOLD;

#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
static bool hotplug_suspended = false;
unsigned int suspend_defer_time = DEFAULT_SUSPEND_DEFER_TIME;
static unsigned int min_cpus_online_res = DEFAULT_MIN_CPUS_ONLINE;
static unsigned int max_cpus_online_res = DEFAULT_MAX_CPUS_ONLINE;
/*
 * suspend mode, if set = 1 hotplug will sleep,
 * if set = 0, then hoplug will be active all the time.
 */
static unsigned int hotplug_suspend = 0;
#endif

/* HotPlug Driver Tuning */
static unsigned int target_cpus = 0;
static u64 boost_lock_duration = BOOST_LOCK_DUR;
static unsigned int def_sampling_ms = DEF_SAMPLING_MS;
static unsigned int nr_fshift = DEFAULT_NR_FSHIFT;
static unsigned int nr_run_hysteresis = DEFAULT_MAX_CPUS_ONLINE * 2;
static unsigned int debug_intelli_plug = 1;

#define dprintk(msg...)		\
do {				\
	if (debug_intelli_plug)		\
		pr_info(msg);	\
} while (0)

static unsigned int nr_run_thresholds_balance[] = {
	(THREAD_CAPACITY * 625 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 875 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 1125 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int nr_run_thresholds_performance[] = {
	(THREAD_CAPACITY * 380 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 625 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 875 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int nr_run_thresholds_conservative[] = {
	(THREAD_CAPACITY * 875 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 1625 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 2125 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int nr_run_thresholds_disable[] = {
	0,  0,  0,  UINT_MAX
};

static unsigned int nr_run_thresholds_tri[] = {
	(THREAD_CAPACITY * 625 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 875 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int nr_run_thresholds_eco[] = {
        (THREAD_CAPACITY * 380 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int nr_run_thresholds_strict[] = {
	UINT_MAX
};

static unsigned int *nr_run_profiles[] = {
	nr_run_thresholds_balance,
	nr_run_thresholds_performance,
	nr_run_thresholds_conservative,
	nr_run_thresholds_disable,
	nr_run_thresholds_tri,
	nr_run_thresholds_eco,
	nr_run_thresholds_strict
	};

static unsigned int nr_run_last;
static unsigned int down_lock_dur = DEFAULT_DOWN_LOCK_DUR;

struct down_lock {
	unsigned int locked;
	struct delayed_work lock_rem;
};
static DEFINE_PER_CPU(struct down_lock, lock_info);

static void apply_down_lock(unsigned int cpu)
{
	struct down_lock *dl = &per_cpu(lock_info, cpu);

	dl->locked = 1;
	mod_delayed_work_on(0, intelliplug_wq, &dl->lock_rem,
			      msecs_to_jiffies(down_lock_dur));
}

static void remove_down_lock(struct work_struct *work)
{
	struct down_lock *dl = container_of(work, struct down_lock,
					    lock_rem.work);
	dl->locked = 0;
}

static int check_down_lock(unsigned int cpu)
{
	struct down_lock *dl = &per_cpu(lock_info, cpu);
	return dl->locked;
}

static unsigned int calculate_thread_stats(void)
{
	unsigned int avg_nr_run = avg_nr_running();
	unsigned int nr_run;
	unsigned int threshold_size;
	unsigned int *current_profile;

	threshold_size = max_cpus_online;
	nr_run_hysteresis = max_cpus_online * 2;
	nr_fshift = max_cpus_online - 1;

	for (nr_run = 1; nr_run < threshold_size; nr_run++) {
		unsigned int nr_threshold;
		if (max_cpus_online >= 4)
			current_profile = nr_run_profiles[full_mode_profile];
		else if (max_cpus_online == 3)
			current_profile = nr_run_profiles[4];
		else if (max_cpus_online == 2)
			current_profile = nr_run_profiles[5];
		else
			current_profile = nr_run_profiles[6];

		nr_threshold = current_profile[nr_run - 1];

		if (nr_run_last <= nr_run)
			nr_threshold += nr_run_hysteresis;
		if (avg_nr_run <= (nr_threshold << (FSHIFT - nr_fshift)))
			break;
	}
	nr_run_last = nr_run;

	return nr_run;
}

static void update_per_cpu_stat(void)
{
	unsigned int cpu;
	struct ip_cpu_info *l_ip_info;

	for_each_online_cpu(cpu) {
		l_ip_info = &per_cpu(ip_info, cpu);
		l_ip_info->cpu_nr_running = avg_cpu_nr_running(cpu);
	}
}

static void __ref cpu_up_down_work(struct work_struct *work)
{
	int online_cpus, cpu, l_nr_threshold;
	int target = target_cpus;
	struct ip_cpu_info *l_ip_info;

	if (target < min_cpus_online)
		target = min_cpus_online;
	else if (target > max_cpus_online)
		target = max_cpus_online;

	online_cpus = num_online_cpus();

	if (target < online_cpus) {
		if (online_cpus <= cpus_boosted &&
		    (ktime_to_us(ktime_get()) - last_input <
				boost_lock_duration))
			return;

		update_per_cpu_stat();
		for_each_online_cpu(cpu) {
			if (cpu == 0)
				continue;
			if (check_down_lock(cpu))
				break;
			l_nr_threshold =
				cpu_nr_run_threshold << 1 /
					(num_online_cpus());
			l_ip_info = &per_cpu(ip_info, cpu);
			if (l_ip_info->cpu_nr_running < l_nr_threshold)
				cpu_down(cpu);
			if (target >= num_online_cpus())
				break;
		}
	} else if (target > online_cpus) {
		for_each_cpu_not(cpu, cpu_online_mask) {
			if (cpu == 0)
				continue;
			cpu_up(cpu);
			apply_down_lock(cpu);
			if (target <= num_online_cpus())
				break;
		}
	}
}

static void intelli_plug_work_fn(struct work_struct *work)
{
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	if (hotplug_suspended == true)
		return;
#endif
	target_cpus = calculate_thread_stats();
	queue_work_on(0, intelliplug_wq, &up_down_work);

	if (atomic_read(&intelli_plug_active) == 1)
		mod_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
					msecs_to_jiffies(def_sampling_ms));
}

#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
static void intelli_plug_suspend(struct work_struct *work)
{
	if (hotplug_suspended == false) {
		mutex_lock(&intelli_plug_mutex);
		hotplug_suspended = true;
		min_cpus_online_res = min_cpus_online;
		min_cpus_online = 1;
		max_cpus_online_res = max_cpus_online;
		max_cpus_online = 2;
		mutex_unlock(&intelli_plug_mutex);

		/* Flush hotplug workqueue */
		flush_workqueue(intelliplug_wq);
		cancel_delayed_work_sync(&intelli_plug_work);

		dprintk("%s: suspended!\n", INTELLI_PLUG);
	}
}

static void __ref intelli_plug_resume(struct work_struct *work)
{
	int cpu, required_reschedule = 0, required_wakeup = 0;

	if (hotplug_suspended == true) {
		mutex_lock(&intelli_plug_mutex);
		hotplug_suspended = false;
		min_cpus_online = min_cpus_online_res;
		max_cpus_online = max_cpus_online_res;
		mutex_unlock(&intelli_plug_mutex);
		required_wakeup = 1;
		/* Initiate hotplug work if it was cancelled */
		required_reschedule = 1;
		INIT_DELAYED_WORK(&intelli_plug_work,
				intelli_plug_work_fn);
		dprintk("%s: resumed.\n", INTELLI_PLUG);
	}

	if (required_wakeup) {
		/* Fire up all CPUs */
		for_each_cpu_not(cpu, cpu_online_mask) {
			if (cpu == 0)
				continue;
			cpu_up(cpu);
			apply_down_lock(cpu);
		}
		dprintk("%s: wakeup boosted.\n", INTELLI_PLUG);
	}

	/* Resume hotplug workqueue if required */
	if (required_reschedule)
		mod_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
				      msecs_to_jiffies(RESUME_SAMPLING_MS));
}

#ifdef CONFIG_LCD_NOTIFY
static void __intelli_plug_suspend(void)
#elif defined(CONFIG_POWERSUSPEND)
static void __intelli_plug_suspend(struct power_suspend *handler)
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void __intelli_plug_suspend(struct early_suspend *handler)
#endif
{
	if (atomic_read(&intelli_plug_active) == 0)
		return;

	if (!hotplug_suspend)
		return;

	INIT_DELAYED_WORK(&suspend_work, intelli_plug_suspend);
	mod_delayed_work_on(0, susp_wq, &suspend_work,
				 msecs_to_jiffies(suspend_defer_time * 1000));
}

#ifdef CONFIG_LCD_NOTIFY
static void __ref __intelli_plug_resume(void)
#elif defined(CONFIG_POWERSUSPEND)
static void __ref __intelli_plug_resume(struct power_suspend *handler)
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void __ref __intelli_plug_resume(struct early_suspend *handler)
#endif
{
	int cpu;

	if (atomic_read(&intelli_plug_active) == 0)
		return;

	if (!hotplug_suspend) {
		/* Fire up all CPUs */
		for_each_cpu_not(cpu, cpu_online_mask) {
			if (cpu == 0)
				continue;
			cpu_up(cpu);
			apply_down_lock(cpu);
		}
		dprintk("%s: wakeup boosted.\n", INTELLI_PLUG);

		return;
	}

	flush_workqueue(susp_wq);
	cancel_delayed_work_sync(&suspend_work);
	queue_work_on(0, susp_wq, &resume_work);
}

#ifdef CONFIG_LCD_NOTIFY
static int lcd_notifier_callback(struct notifier_block *nb,
				unsigned long event, void *data)
{
	switch (event) {
	case LCD_EVENT_ON_END:
	case LCD_EVENT_OFF_START:
		break;
	case LCD_EVENT_ON_START:
		__intelli_plug_resume();
		break;
	case LCD_EVENT_OFF_END:
		__intelli_plug_suspend();
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}
#elif defined(CONFIG_POWERSUSPEND) || defined(CONFIG_HAS_EARLYSUSPEND)
#ifdef CONFIG_POWERSUSPEND
static struct power_suspend intelli_plug_power_suspend_driver = {
#else
static struct early_suspend intelli_plug_early_suspend_driver = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 10,
#endif
	.suspend = __intelli_plug_suspend,
	.resume = __intelli_plug_resume,
};
#endif
#endif

static void intelli_plug_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	u64 now;

#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	if (hotplug_suspended == true)
		return;
#else
	if (cpus_boosted == 1)
		return;
#endif

	now = ktime_to_us(ktime_get());
	last_input = now;

	if (now - last_boost_time < MIN_INPUT_INTERVAL)
		return;

	if (num_online_cpus() >= cpus_boosted ||
	    cpus_boosted <= min_cpus_online)
		return;

	target_cpus = cpus_boosted;
	queue_work_on(0, intelliplug_wq, &up_down_work);
	last_boost_time = ktime_to_us(ktime_get());
}

static int intelli_plug_input_connect(struct input_handler *handler,
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
	handle->name = handler->name;

	err = input_register_handle(handle);
	if (err)
		goto err_register;

	err = input_open_device(handle);
	if (err)
		goto err_open;

	return 0;
err_open:
	input_unregister_handle(handle);
err_register:
	kfree(handle);
	return err;
}

static void intelli_plug_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id intelli_plug_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			    BIT_MASK(ABS_MT_POSITION_X) |
			    BIT_MASK(ABS_MT_POSITION_Y) },
	}, /* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			    BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	}, /* touchpad */
	{ },
};

static struct input_handler intelli_plug_input_handler = {
	.event          = intelli_plug_input_event,
	.connect        = intelli_plug_input_connect,
	.disconnect     = intelli_plug_input_disconnect,
	.name           = "intelliplug_handler",
	.id_table       = intelli_plug_ids,
};

static int __ref intelli_plug_start(void)
{
	int cpu, ret = 0;
	struct down_lock *dl;

	intelliplug_wq = alloc_workqueue("intelliplug",
			WQ_HIGHPRI | WQ_FREEZABLE, 0);
	if (!intelliplug_wq) {
		pr_err("%s: Failed to allocate hotplug workqueue\n",
		       INTELLI_PLUG);
		ret = -ENOMEM;
		goto err_out;
	}

#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	susp_wq =
	    alloc_workqueue("intelli_susp_wq", WQ_FREEZABLE, 0);
	if (!susp_wq) {
		pr_err("%s: Failed to allocate suspend workqueue\n",
		       INTELLI_PLUG);
		ret = -ENOMEM;
		goto err_out;
	}
#endif

#ifdef CONFIG_LCD_NOTIFY
	notif.notifier_call = lcd_notifier_callback;
	if (lcd_register_client(&notif) != 0) {
		pr_err("%s: Failed to register LCD notifier callback\n",
			INTELLI_PLUG);
		goto err_dev;
	}
#elif defined(CONFIG_POWERSUSPEND)
	register_power_suspend(&intelli_plug_power_suspend_driver);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&intelli_plug_early_suspend_driver);
#endif

	ret = input_register_handler(&intelli_plug_input_handler);
	if (ret) {
		pr_err("%s: Failed to register input handler: %d\n",
			INTELLI_PLUG, ret);
		goto err_dev;
	}

#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	mutex_init(&intelli_plug_mutex);
#endif

	INIT_WORK(&up_down_work, cpu_up_down_work);
	INIT_DELAYED_WORK(&intelli_plug_work, intelli_plug_work_fn);
	for_each_possible_cpu(cpu) {
		dl = &per_cpu(lock_info, cpu);
		INIT_DELAYED_WORK(&dl->lock_rem, remove_down_lock);
	}
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	INIT_DELAYED_WORK(&suspend_work, intelli_plug_suspend);
	INIT_WORK(&resume_work, intelli_plug_resume);
#endif

	/* Fire up all CPUs */
	for_each_cpu_not(cpu, cpu_online_mask) {
		if (cpu == 0)
			continue;
		cpu_up(cpu);
		apply_down_lock(cpu);
	}

	mod_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
			      START_DELAY_MS);

	return ret;
err_dev:
	destroy_workqueue(intelliplug_wq);
err_out:
	atomic_set(&intelli_plug_active, 0);
	return ret;
}

static void intelli_plug_stop(void)
{
	int cpu;
	struct down_lock *dl;

#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	flush_workqueue(susp_wq);
	cancel_work_sync(&resume_work);
	cancel_delayed_work_sync(&suspend_work);
#endif
	for_each_possible_cpu(cpu) {
		dl = &per_cpu(lock_info, cpu);
		cancel_delayed_work_sync(&dl->lock_rem);
	}
	flush_workqueue(intelliplug_wq);
	cancel_work_sync(&up_down_work);
	cancel_delayed_work_sync(&intelli_plug_work);
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	mutex_destroy(&intelli_plug_mutex);
#ifdef CONFIG_LCD_NOTIFY
	lcd_unregister_client(&notif);
	notif.notifier_call = NULL;
#elif defined(CONFIG_POWERSUSPEND)
	unregister_power_suspend(&intelli_plug_power_suspend_driver);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&intelli_plug_early_suspend_driver);
#endif
#endif

	input_unregister_handler(&intelli_plug_input_handler);
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	destroy_workqueue(susp_wq);
#endif
	destroy_workqueue(intelliplug_wq);
}

static void intelli_plug_active_eval_fn(unsigned int status)
{
	int ret = 0;

	if (status == 1) {
		ret = intelli_plug_start();
		if (ret)
			status = 0;
	} else
		intelli_plug_stop();

	atomic_set(&intelli_plug_active, status);
}

#define show_one(file_name, object)				\
static ssize_t show_##file_name					\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)	\
{								\
	return sprintf(buf, "%u\n", object);			\
}

show_one(cpus_boosted, cpus_boosted);
show_one(min_cpus_online, min_cpus_online);
show_one(max_cpus_online, max_cpus_online);
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
show_one(suspend_defer_time, suspend_defer_time);
show_one(hotplug_suspend, hotplug_suspend);
#endif
show_one(full_mode_profile, full_mode_profile);
show_one(cpu_nr_run_threshold, cpu_nr_run_threshold);
show_one(def_sampling_ms, def_sampling_ms);
show_one(debug_intelli_plug, debug_intelli_plug);
show_one(nr_fshift, nr_fshift);
show_one(nr_run_hysteresis, nr_run_hysteresis);
show_one(down_lock_duration, down_lock_dur);

#define store_one(file_name, object)		\
static ssize_t store_##file_name		\
(struct kobject *kobj,				\
 struct kobj_attribute *attr,			\
 const char *buf, size_t count)			\
{						\
	unsigned int input;			\
	int ret;				\
	ret = sscanf(buf, "%u", &input);	\
	if (ret != 1 || input > 100)		\
		return -EINVAL;			\
	if (input == object) {			\
		return count;			\
	}					\
	object = input;				\
	return count;				\
}

store_one(cpus_boosted, cpus_boosted);
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
store_one(suspend_defer_time, suspend_defer_time);
store_one(hotplug_suspend, hotplug_suspend);
#endif
store_one(full_mode_profile, full_mode_profile);
store_one(cpu_nr_run_threshold, cpu_nr_run_threshold);
store_one(def_sampling_ms, def_sampling_ms);
store_one(debug_intelli_plug, debug_intelli_plug);
store_one(nr_fshift, nr_fshift);
store_one(nr_run_hysteresis, nr_run_hysteresis);
store_one(down_lock_duration, down_lock_dur);

static ssize_t show_intelli_plug_active(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n",
			atomic_read(&intelli_plug_active));
}

static ssize_t store_intelli_plug_active(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	unsigned int input;

	ret = sscanf(buf, "%d", &input);
	if (ret < 0)
		return ret;

	if (input < 0)
		input = 0;
	else if (input > 0)
		input = 1;

	if (input == atomic_read(&intelli_plug_active))
		return count;

	intelli_plug_active_eval_fn(input);

	return count;
}

static ssize_t show_boost_lock_duration(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%llu\n", div_u64(boost_lock_duration, 1000));
}

static ssize_t store_boost_lock_duration(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	u64 val;

	ret = sscanf(buf, "%llu", &val);
	if (ret != 1)
		return -EINVAL;

	boost_lock_duration = val * 1000;

	return count;
}

static ssize_t store_min_cpus_online(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > NR_CPUS)
		return -EINVAL;

	if (max_cpus_online < val)
		max_cpus_online = val;

	min_cpus_online = val;

	return count;
}

static ssize_t store_max_cpus_online(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > NR_CPUS)
		return -EINVAL;

	if (min_cpus_online > val)
		min_cpus_online = val;

	max_cpus_online = val;

	return count;
}

#define KERNEL_ATTR_RW(_name) \
static struct kobj_attribute _name##_attr = \
	__ATTR(_name, 0644, show_##_name, store_##_name)

KERNEL_ATTR_RW(intelli_plug_active);
KERNEL_ATTR_RW(cpus_boosted);
KERNEL_ATTR_RW(min_cpus_online);
KERNEL_ATTR_RW(max_cpus_online);
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
KERNEL_ATTR_RW(suspend_defer_time);
KERNEL_ATTR_RW(hotplug_suspend);
#endif
KERNEL_ATTR_RW(full_mode_profile);
KERNEL_ATTR_RW(cpu_nr_run_threshold);
KERNEL_ATTR_RW(boost_lock_duration);
KERNEL_ATTR_RW(def_sampling_ms);
KERNEL_ATTR_RW(debug_intelli_plug);
KERNEL_ATTR_RW(nr_fshift);
KERNEL_ATTR_RW(nr_run_hysteresis);
KERNEL_ATTR_RW(down_lock_duration);

static struct attribute *intelli_plug_attrs[] = {
	&intelli_plug_active_attr.attr,
	&cpus_boosted_attr.attr,
	&min_cpus_online_attr.attr,
	&max_cpus_online_attr.attr,
#if defined(CONFIG_LCD_NOTIFY) || \
	defined(CONFIG_POWERSUSPEND) || \
	defined(CONFIG_HAS_EARLYSUSPEND)
	&suspend_defer_time_attr.attr,
	&hotplug_suspend_attr.attr,
#endif
	&full_mode_profile_attr.attr,
	&cpu_nr_run_threshold_attr.attr,
	&boost_lock_duration_attr.attr,
	&def_sampling_ms_attr.attr,
	&debug_intelli_plug_attr.attr,
	&nr_fshift_attr.attr,
	&nr_run_hysteresis_attr.attr,
	&down_lock_duration_attr.attr,
	NULL,
};

static struct attribute_group intelli_plug_attr_group = {
	.attrs = intelli_plug_attrs,
	.name = "intelli_plug",
};

static int __init intelli_plug_init(void)
{
	int rc;

	rc = sysfs_create_group(kernel_kobj, &intelli_plug_attr_group);

	pr_info("intelli_plug: version %d.%d\n",
		 INTELLI_PLUG_MAJOR_VERSION,
		 INTELLI_PLUG_MINOR_VERSION);

	if (atomic_read(&intelli_plug_active) == 1)
		intelli_plug_start();

	return 0;
}

static void __exit intelli_plug_exit(void)
{
	if (atomic_read(&intelli_plug_active) == 1)
		intelli_plug_stop();

	sysfs_remove_group(kernel_kobj, &intelli_plug_attr_group);
}

late_initcall(intelli_plug_init);
module_exit(intelli_plug_exit);

MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>, \
		Alucard24, Dorimanx, neobuddy89");
MODULE_DESCRIPTION("'intell_plug' - An intelligent cpu hotplug driver for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPLv2");
