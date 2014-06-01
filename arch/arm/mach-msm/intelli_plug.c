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
#include <linux/rq_stats.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/kobject.h>
#if defined(CONFIG_POWERSUSPEND)
#include <linux/powersuspend.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */
#include <linux/cpufreq.h>

#define INTELLI_PLUG_MAJOR_VERSION	3
#define INTELLI_PLUG_MINOR_VERSION	7

#define BUSY_PERSISTENCE		10
#define DUAL_CORE_PERSISTENCE		7
#define TRI_CORE_PERSISTENCE		5
#define QUAD_CORE_PERSISTENCE		3

#define CPU_DOWN_FACTOR			2
#define NR_FSHIFT			3

#define DEF_SAMPLING_MS			100 / 2
#define BUSY_SAMPLING_MS		100 / 5
#define RESUME_SAMPLING_MS		100 / 10
#define START_DELAY_MS			100 * 20
#define MIN_INPUT_INTERVAL		150 * 1000L
#define BOOST_LOCK_DUR			2500 * 1000L

static struct mutex intelli_plug_mutex;
static u64 last_boost_time, last_input;
static unsigned int boosted_cpus = 2;

static struct delayed_work intelli_plug_work;
static struct delayed_work intelli_plug_boost;
static struct workqueue_struct *intelliplug_wq;

static unsigned int sampling_time = 10;
static unsigned int persist_count = 0;
static unsigned int busy_persist_count = 0;
static bool hotplug_suspended = false;

struct ip_cpu_info {
	int cpu;
	unsigned int curr_max;
};

static DEFINE_PER_CPU(struct ip_cpu_info, ip_info);

/* HotPlug Driver controls */
static atomic_t intelli_plug_active = ATOMIC_INIT(0);
static unsigned int eco_mode_active = 0;
static unsigned int strict_mode_active = 0;
static unsigned int wake_boost_active = 0;
static unsigned int touch_boosted_cpus = 2;
static unsigned int screen_off_max = UINT_MAX;

/* HotPlug Driver Tuning */
static u64 boost_lock_duration = BOOST_LOCK_DUR;
static unsigned int def_sampling_ms = DEF_SAMPLING_MS;
static unsigned int busy_sampling_ms = BUSY_SAMPLING_MS;
static unsigned int dual_core_persistence = DUAL_CORE_PERSISTENCE;
static unsigned int tri_core_persistence = TRI_CORE_PERSISTENCE;
static unsigned int quad_core_persistence = QUAD_CORE_PERSISTENCE;
static unsigned int busy_persistence = BUSY_PERSISTENCE;
static unsigned int cpu_down_factor = CPU_DOWN_FACTOR;
static unsigned int nr_fshift = NR_FSHIFT;
static unsigned int nr_run_hysteresis = 4;  /* 0.5 thread */
static unsigned int debug_intelli_plug = 0;

static unsigned int nr_run_thresholds_full[] = {
/*	1,  2,  3,  4 - on-line cpus target */
	5,  7,  9,  UINT_MAX /* avg run threads * 2 (e.g., 9 = 2.25 threads) */
	};

static unsigned int nr_run_thresholds_eco[] = {
/*      1,  2, - on-line cpus target */
        3,  UINT_MAX /* avg run threads * 2 (e.g., 9 = 2.25 threads) */
        };

static unsigned int nr_run_thresholds_strict[] = {
/*	   1, - on-line cpus target */
	UINT_MAX /* avg run threads *2 (e.g., 9 = 2.25 threads) */
	};

static unsigned int nr_run_last;

static unsigned int NwNs_Threshold[] = { 19, 30,  19,  11,  19,  11, 0,  11};
static unsigned int TwTs_Threshold[] = {140,  0, 140, 190, 140, 190, 0, 190};

static int mp_decision(int nr_cpu_online)
{
	static bool first_call = true;
	int new_state = 0;
	int index;
	unsigned int rq_depth;
	static cputime64_t total_time = 0;
	static cputime64_t last_time;
	cputime64_t current_time;
	cputime64_t this_time = 0;

	current_time = ktime_to_ms(ktime_get());
	if (first_call) {
		first_call = false;
	} else {
		this_time = current_time - last_time;
	}
	total_time += this_time;

	rq_depth = rq_info.rq_avg;

	if (nr_cpu_online) {
		index = (nr_cpu_online - 1) * 2;
		if ((nr_cpu_online < NR_CPUS) &&
				(rq_depth >= NwNs_Threshold[index])) {
			if (total_time >= TwTs_Threshold[index]) {
				new_state = 1;
			}
		} else if (rq_depth <= NwNs_Threshold[index+1]) {
			if (total_time >= TwTs_Threshold[index+1] ) {
				new_state = 0;
			}
		} else {
			total_time = 0;
		}
	} else {
		total_time = 0;
	}

	last_time = ktime_to_ms(ktime_get());

	return new_state;
}

static unsigned int calculate_thread_stats(void)
{
	unsigned int avg_nr_run = avg_nr_running();
	unsigned int nr_run;
	unsigned int threshold_size;

	if (strict_mode_active) {
		threshold_size =  ARRAY_SIZE(nr_run_thresholds_strict);
		nr_run_hysteresis = 2;
		nr_fshift = 1;
		if (debug_intelli_plug)
			pr_info("intelliplug: strict mode active!");
	} else if (eco_mode_active) {
		threshold_size =  ARRAY_SIZE(nr_run_thresholds_eco);
		nr_run_hysteresis = 4;
		nr_fshift = 1;
		if (debug_intelli_plug)
			pr_info("intelliplug: eco mode active!");
	} else {
		threshold_size =  ARRAY_SIZE(nr_run_thresholds_full);
		nr_run_hysteresis = 8;
		nr_fshift = 3;
		if (debug_intelli_plug)
			pr_info("intelliplug: full mode active!");
	}

	for (nr_run = 1; nr_run < threshold_size; nr_run++) {
		unsigned int nr_threshold;
		if (strict_mode_active)
			nr_threshold = nr_run_thresholds_strict[nr_run - 1];
		else if (eco_mode_active)
			nr_threshold = nr_run_thresholds_eco[nr_run - 1];
		else
			nr_threshold = nr_run_thresholds_full[nr_run - 1];

		if (nr_run_last <= nr_run)
			nr_threshold += nr_run_hysteresis;
		if (avg_nr_run <= (nr_threshold << (FSHIFT - nr_fshift)))
			break;
	}
	nr_run_last = nr_run;

	return nr_run;
}

static int boost_lock_check(void)
{
	if (num_online_cpus() <= boosted_cpus &&
	    (ktime_to_us(ktime_get()) - last_input < boost_lock_duration))
		return 1;
	return 0;
}

static void __ref intelli_plug_work_fn(struct work_struct *work)
{
	unsigned int nr_run_stat;
	unsigned int cpu_count = 0;
	unsigned int online_cpus = 0;

	int decision = 0;
	int i, boost_locked;

	nr_run_stat = calculate_thread_stats();
	if (debug_intelli_plug)
		pr_info("nr_run_stat: %u\n", nr_run_stat);
	cpu_count = nr_run_stat;
	/* detect artificial loads or constant loads
	 * using msm rqstats
	 */
	online_cpus = num_online_cpus();

	if (!eco_mode_active && !strict_mode_active && online_cpus < NR_CPUS) {
		decision = mp_decision(online_cpus);
		if (decision) {
			switch (online_cpus) {
			case 2:
				cpu_count = 3;
				if (debug_intelli_plug)
					pr_info("nr_run(2) => %u\n",
							nr_run_stat);
				break;
			case 3:
				cpu_count = 4;
				if (debug_intelli_plug)
					pr_info("nr_run(3) => %u\n",
							nr_run_stat);
				break;
			}
		}
	}
	/* it's busy.. lets help it a bit */
	if (cpu_count > 2) {
		if (busy_persist_count == 0) {
			sampling_time = busy_sampling_ms;
			busy_persist_count = busy_persistence;
		}
	} else {
		if (busy_persist_count > 0)
			busy_persist_count--;
		else
			sampling_time = def_sampling_ms;
	}


	if (hotplug_suspended) {
		if (debug_intelli_plug)
			pr_info("intelli_plug is suspended!\n");
		return;
	}

	boost_locked = boost_lock_check();

	switch (cpu_count) {
	case 1:
		if (persist_count > 0)
			persist_count--;
		if (persist_count == 0) {
			/* take down everyone */
			if (!boost_locked)
				for_each_online_cpu(i) {
					if (i == 0)
						continue;
					cpu_down(i);
				}
		}
		if (debug_intelli_plug)
			pr_info("case 1: %u\n", persist_count);
		break;
	case 2:
		persist_count = dual_core_persistence;
		if (!decision)
			persist_count = dual_core_persistence /
				cpu_down_factor;
		if (online_cpus < 2) {
			for (i = 1; i < cpu_count; i++)
				cpu_up(i);
		} else {
			if (!boost_locked)
				for (i = 3; i >  1; i--)
					cpu_down(i);
		}
		if (debug_intelli_plug)
			pr_info("case 2: %u\n", persist_count);
		break;
	case 3:
		persist_count = tri_core_persistence;
		if (!decision)
			persist_count = tri_core_persistence /
				cpu_down_factor;
		if (online_cpus < 3) {
			for (i = 1; i < cpu_count; i++)
				cpu_up(i);
		} else {
			if (!boost_locked)
				for (i = 3; i > 2; i--)
					cpu_down(i);
		}
		if (debug_intelli_plug)
			pr_info("case 3: %u\n", persist_count);
		break;
	case 4:
		persist_count = quad_core_persistence;
		if (!decision)
			persist_count = quad_core_persistence /
				cpu_down_factor;
		if (online_cpus < 4)
			for_each_cpu_not(i, cpu_online_mask) {
				if (i == 0)
					continue;
				cpu_up(i);
			}
		if (debug_intelli_plug)
			pr_info("case 4: %u\n", persist_count);
		break;
	default:
		pr_err("Run Stat Error: Bad value %u\n",
				nr_run_stat);
		break;
	}

	if (atomic_read(&intelli_plug_active) == 1)
		queue_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
					msecs_to_jiffies(sampling_time));
}

static void __ref intelli_plug_boost_fn(struct work_struct *work)
{
	int cpu;	
	u64 now;	

	now = ktime_to_us(ktime_get());
	last_input = now;

	if (now - last_boost_time < MIN_INPUT_INTERVAL)
		return;

	if (eco_mode_active)
		boosted_cpus = 2;
	else
		boosted_cpus = touch_boosted_cpus;

	for_each_cpu_not(cpu, cpu_online_mask) {
		if (boosted_cpus <= num_online_cpus())
			break;
		if (cpu == 0)
			continue;
		cpu_up(cpu);
	}
	last_boost_time = ktime_to_us(ktime_get());
}

static void __ref wakeup_boost(void)
{
	unsigned int cpu, ret;
	struct cpufreq_policy policy;

	for_each_cpu_not(cpu, cpu_online_mask) {
		if (NR_CPUS == num_online_cpus())
			break;
		if (cpu == 0)
			continue;
		cpu_up(cpu);
	}

	for_each_online_cpu(cpu) {
		ret = cpufreq_get_policy(&policy, cpu);
		if (ret)
			continue;
		policy.cur = policy.max;
		cpufreq_update_policy(cpu);
	}
}

#if defined(CONFIG_POWERSUSPEND) || defined(CONFIG_HAS_EARLYSUSPEND)
static void screen_off_limit(bool on)
{
	unsigned int i, ret;
	struct cpufreq_policy policy;
	struct ip_cpu_info *l_ip_info;

	/* not active, so exit */
	if (screen_off_max == UINT_MAX)
		return;

	for_each_online_cpu(i) {

		l_ip_info = &per_cpu(ip_info, i);
		ret = cpufreq_get_policy(&policy, i);
		if (ret)
			continue;

		if (on) {
			/* save current instance */
			l_ip_info->curr_max = policy.max;
			policy.max = screen_off_max;
		} else {
			/* restore */
			policy.max = l_ip_info->curr_max;
		}
		cpufreq_update_policy(i);
	}
}

#ifdef CONFIG_POWERSUSPEND
static void intelli_plug_suspend(struct power_suspend *handler)
#else
static void intelli_plug_suspend(struct early_suspend *handler)
#endif
{
	int cpu = 0;

	if (atomic_read(&intelli_plug_active) == 0)
		return;

	/* flush hotplug workqueue */
	flush_workqueue(intelliplug_wq);
	cancel_delayed_work_sync(&intelli_plug_boost);
	cancel_delayed_work_sync(&intelli_plug_work);

	mutex_lock(&intelli_plug_mutex);
	hotplug_suspended = true;
	screen_off_limit(true);
	mutex_unlock(&intelli_plug_mutex);

	/* put rest of the cores to sleep! */
	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		cpu_down(cpu);
	}
}

#ifdef CONFIG_POWERSUSPEND
static void __ref intelli_plug_resume(struct power_suspend *handler)
#else
static void __ref intelli_plug_resume(struct early_suspend *handler)
#endif
{
	int cpu, num_of_active_cores;

	if (atomic_read(&intelli_plug_active) == 0)
		return;

	mutex_lock(&intelli_plug_mutex);
	/* keep cores awake long enough for faster wake up */
	persist_count = busy_persistence;
	hotplug_suspended = false;
	screen_off_limit(false);
	mutex_unlock(&intelli_plug_mutex);

	if (wake_boost_active)
		/* wake up and boost all cores to max freq */
		wakeup_boost();
	else {
		/* wake up all possible cores */
		if (strict_mode_active)
			num_of_active_cores = 1;
		else if (eco_mode_active)
			num_of_active_cores = 2;
		else
			num_of_active_cores = NR_CPUS;

		for_each_cpu_not(cpu, cpu_online_mask) {
			if (num_of_active_cores <= num_online_cpus())
				break;
			if (cpu == 0)
				continue;
			cpu_up(cpu);
		}
	}

	INIT_DELAYED_WORK(&intelli_plug_work, intelli_plug_work_fn);
	INIT_DELAYED_WORK(&intelli_plug_boost, intelli_plug_boost_fn);

	/* resume hotplug workqueue */
	queue_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
			      RESUME_SAMPLING_MS);
}

#ifdef CONFIG_POWERSUSPEND
static struct power_suspend intelli_plug_power_suspend_driver = {
#else
static struct early_suspend intelli_plug_early_suspend_struct_driver = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 10,
#endif
	.suspend = intelli_plug_suspend,
	.resume = intelli_plug_resume,
};
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

static void intelli_plug_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	if (strict_mode_active || touch_boosted_cpus == 1 || hotplug_suspended)
		return;

	queue_delayed_work_on(0, intelliplug_wq, &intelli_plug_boost,
			      RESUME_SAMPLING_MS);
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

	if (debug_intelli_plug)
		pr_info("%s found and connected!\n", dev->name);

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
	int rc;

	intelliplug_wq = alloc_workqueue("intelliplug", WQ_HIGHPRI | WQ_FREEZABLE, 0);
	rc = input_register_handler(&intelli_plug_input_handler);

	if (!intelliplug_wq) {
		printk(KERN_ERR "Failed to create intelliplug \
				workqueue\n");
		return -EFAULT;
	}

	sampling_time = 10;
	hotplug_suspended = false;

	mutex_init(&intelli_plug_mutex);

	INIT_DELAYED_WORK(&intelli_plug_work, intelli_plug_work_fn);
	INIT_DELAYED_WORK(&intelli_plug_boost, intelli_plug_boost_fn);

	queue_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
			      START_DELAY_MS);

#if defined(CONFIG_POWERSUSPEND)
	register_power_suspend(&intelli_plug_power_suspend_driver);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&intelli_plug_early_suspend_struct_driver);
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

	return 0;
}

static void intelli_plug_stop(void)
{
#if defined(CONFIG_POWERSUSPEND)
	unregister_power_suspend(&intelli_plug_power_suspend_driver);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&intelli_plug_early_suspend_struct_driver);
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

	flush_workqueue(intelliplug_wq);
	cancel_delayed_work_sync(&intelli_plug_boost);
	cancel_delayed_work_sync(&intelli_plug_work);
	mutex_destroy(&intelli_plug_mutex);
	input_unregister_handler(&intelli_plug_input_handler);
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

show_one(eco_mode_active, eco_mode_active);
show_one(strict_mode_active, strict_mode_active);
show_one(wake_boost_active, wake_boost_active);
show_one(touch_boosted_cpus, touch_boosted_cpus);
show_one(def_sampling_ms, def_sampling_ms);
show_one(busy_sampling_ms, busy_sampling_ms);
show_one(dual_core_persistence, dual_core_persistence);
show_one(tri_core_persistence, tri_core_persistence);
show_one(quad_core_persistence, quad_core_persistence);
show_one(busy_persistence, busy_persistence);
show_one(cpu_down_factor, cpu_down_factor);
show_one(debug_intelli_plug, debug_intelli_plug);
show_one(nr_fshift, nr_fshift);
show_one(nr_run_hysteresis, nr_run_hysteresis);
show_one(screen_off_max, screen_off_max);

#define store_one(file_name, object)		\
static ssize_t store_##file_name		\
(struct kobject *kobj, 				\
 struct kobj_attribute *attr, 			\
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

store_one(eco_mode_active, eco_mode_active);
store_one(strict_mode_active, strict_mode_active);
store_one(wake_boost_active, wake_boost_active);
store_one(touch_boosted_cpus, touch_boosted_cpus);
store_one(def_sampling_ms, def_sampling_ms);
store_one(busy_sampling_ms, busy_sampling_ms);
store_one(dual_core_persistence, dual_core_persistence);
store_one(tri_core_persistence, tri_core_persistence);
store_one(quad_core_persistence, quad_core_persistence);
store_one(busy_persistence, busy_persistence);
store_one(cpu_down_factor, cpu_down_factor);
store_one(debug_intelli_plug, debug_intelli_plug);
store_one(nr_fshift, nr_fshift);
store_one(nr_run_hysteresis, nr_run_hysteresis);
store_one(screen_off_max, screen_off_max);

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

	input = min(max(input, 0), 1);

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

#define KERNEL_ATTR_RW(_name) \
static struct kobj_attribute _name##_attr = \
	__ATTR(_name, 0644, show_##_name, store_##_name)

KERNEL_ATTR_RW(intelli_plug_active);
KERNEL_ATTR_RW(eco_mode_active);
KERNEL_ATTR_RW(strict_mode_active);
KERNEL_ATTR_RW(wake_boost_active);
KERNEL_ATTR_RW(touch_boosted_cpus);
KERNEL_ATTR_RW(boost_lock_duration);
KERNEL_ATTR_RW(def_sampling_ms);
KERNEL_ATTR_RW(busy_sampling_ms);
KERNEL_ATTR_RW(dual_core_persistence);
KERNEL_ATTR_RW(tri_core_persistence);
KERNEL_ATTR_RW(quad_core_persistence);
KERNEL_ATTR_RW(busy_persistence);
KERNEL_ATTR_RW(cpu_down_factor);
KERNEL_ATTR_RW(debug_intelli_plug);
KERNEL_ATTR_RW(nr_fshift);
KERNEL_ATTR_RW(nr_run_hysteresis);
KERNEL_ATTR_RW(screen_off_max);

static struct attribute *intelli_plug_attrs[] = {
	&intelli_plug_active_attr.attr,
	&eco_mode_active_attr.attr,
	&strict_mode_active_attr.attr,
	&wake_boost_active_attr.attr,
	&touch_boosted_cpus_attr.attr,
	&boost_lock_duration_attr.attr,
	&def_sampling_ms_attr.attr,
	&busy_sampling_ms_attr.attr,
	&dual_core_persistence_attr.attr,
	&tri_core_persistence_attr.attr,
	&quad_core_persistence_attr.attr,
	&busy_persistence_attr.attr,
	&cpu_down_factor_attr.attr,
	&debug_intelli_plug_attr.attr,
	&nr_fshift_attr.attr,
	&nr_run_hysteresis_attr.attr,
	&screen_off_max_attr.attr,
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

	pr_info("intelli_plug: version %d.%d by faux123\n",
		 INTELLI_PLUG_MAJOR_VERSION,
		 INTELLI_PLUG_MINOR_VERSION);

	if (atomic_read(&intelli_plug_active) == 1)
		intelli_plug_start();

	return 0;
}

static int __exit intelli_plug_exit(void)
{
	if (atomic_read(&intelli_plug_active) == 1)
		intelli_plug_stop();

	sysfs_remove_group(kernel_kobj, &intelli_plug_attr_group);

	return 0;
}

MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_AUTHOR("Alucard24 & Dorimanx & neobuddy89");
MODULE_DESCRIPTION("'intell_plug' - An intelligent cpu hotplug driver for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPLv2");

late_initcall(intelli_plug_init);
late_initexit(intelli_plug_exit);
