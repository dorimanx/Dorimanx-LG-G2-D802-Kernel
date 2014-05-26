/*
 * Author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * Copyright 2012~2014 Paul Reioux
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <linux/cpufreq.h>

#if defined(CONFIG_POWERSUSPEND)
#include <linux/powersuspend.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

#define INTELLI_PLUG_MAJOR_VERSION	3
#define INTELLI_PLUG_MINOR_VERSION	5

#define DEF_SAMPLING_MS			(40)
#define BUSY_SAMPLING_MS		(20)

#define BUSY_PERSISTENCE		10
#define DUAL_CORE_PERSISTENCE		7
#define TRI_CORE_PERSISTENCE		5
#define QUAD_CORE_PERSISTENCE		3

#define CPU_DOWN_FACTOR			2
#define NR_FSHIFT			3

static DEFINE_MUTEX(intelli_plug_mutex);

static struct delayed_work intelli_plug_work;
static struct workqueue_struct *intelliplug_wq;

static unsigned int sampling_time = 10;
static unsigned int sampling_time_on = 10;
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
static unsigned int screen_off_max = UINT_MAX;

/* HotPlug Driver Tuning */
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

static int mp_decision(void)
{
	static bool first_call = true;
	int new_state = 0;
	int nr_cpu_online;
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
	/* pr_info(" rq_deptch = %u", rq_depth); */
	nr_cpu_online = num_online_cpus();

	if (nr_cpu_online) {
		index = (nr_cpu_online - 1) * 2;
		if ((nr_cpu_online < 4) &&
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

	if (eco_mode_active == 1) {
		threshold_size =  ARRAY_SIZE(nr_run_thresholds_eco);
		nr_run_hysteresis = 4;
		nr_fshift = 1;
		if (debug_intelli_plug)
			pr_info("intelliplug: eco mode active!");
	} else if (strict_mode_active == 1) {
		threshold_size =  ARRAY_SIZE(nr_run_thresholds_strict);
		nr_run_hysteresis = 2;
		nr_fshift = 1;
		if (debug_intelli_plug)
			pr_info("intelliplug: strict mode active!");
	} else {
		threshold_size =  ARRAY_SIZE(nr_run_thresholds_full);
		nr_run_hysteresis = 8;
		nr_fshift = 3;
		if (debug_intelli_plug)
			pr_info("intelliplug: full mode active!");
	}

	for (nr_run = 1; nr_run < threshold_size; nr_run++) {
		unsigned int nr_threshold;
		if (eco_mode_active == 1) {
			nr_threshold = nr_run_thresholds_eco[nr_run - 1];
		} else if (strict_mode_active == 1) {
			nr_threshold = nr_run_thresholds_strict[nr_run - 1];
		} else {
			nr_threshold = nr_run_thresholds_full[nr_run - 1];
		}

		if (nr_run_last <= nr_run)
			nr_threshold += nr_run_hysteresis;
		if (avg_nr_run <= (nr_threshold << (FSHIFT - nr_fshift)))
			break;
	}
	nr_run_last = nr_run;

	return nr_run;
}

static void __ref intelli_plug_work_fn(struct work_struct *work)
{
	unsigned int nr_run_stat;
	unsigned int cpu_count = 0;
	unsigned int nr_cpus = 0;

	int decision = 0;
	int i;

	nr_run_stat = calculate_thread_stats();
	if (debug_intelli_plug)
		pr_info("nr_run_stat: %u\n", nr_run_stat);
	cpu_count = nr_run_stat;
	/* detect artificial loads or constant loads
	 * using msm rqstats
	 */
	nr_cpus = num_online_cpus();
	if (!eco_mode_active && !strict_mode_active &&
			(nr_cpus >= 1 && nr_cpus < 4)) {
		decision = mp_decision();
		if (decision) {
			switch (nr_cpus) {
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

	if (!hotplug_suspended) {
		switch (cpu_count) {
		case 1:
			if (persist_count > 0)
				persist_count--;
			if (persist_count == 0) {
				/* take down everyone */
				for (i = 3; i > 0; i--)
					cpu_down(i);
			}
			if (debug_intelli_plug)
				pr_info("case 1: %u\n", persist_count);
			break;
		case 2:
			persist_count = dual_core_persistence;
			if (!decision)
				persist_count = dual_core_persistence /
					cpu_down_factor;
			if (nr_cpus < 2) {
				for (i = 1; i < cpu_count; i++)
					cpu_up(i);
			} else {
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
			if (nr_cpus < 3) {
				for (i = 1; i < cpu_count; i++)
					cpu_up(i);
			} else {
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
			if (nr_cpus < 4)
				for (i = 1; i < cpu_count; i++)
					cpu_up(i);
			if (debug_intelli_plug)
				pr_info("case 4: %u\n", persist_count);
			break;
		default:
			pr_err("Run Stat Error: Bad value %u\n",
					nr_run_stat);
			break;
		}
	} else if (debug_intelli_plug)
		pr_info("intelli_plug is suspened!\n");

	queue_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
		msecs_to_jiffies(sampling_time));
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
static void intelli_plug_early_suspend(struct early_suspend *handler)
#endif
{
	int i = 0;
	int num_of_active_cores = 0;

	if (atomic_read(&intelli_plug_active) == 1) {
		flush_workqueue(intelliplug_wq);
		num_of_active_cores = num_possible_cpus();

		mutex_lock(&intelli_plug_mutex);
		hotplug_suspended = true;
		screen_off_limit(true);
		mutex_unlock(&intelli_plug_mutex);

		/* put rest of the cores to sleep! */
		for (i = num_of_active_cores - 1; i > 0; i--) {
			cpu_down(i);
		}
	}
}

static void wakeup_boost(void)
{
	unsigned int i, ret;
	struct cpufreq_policy policy;

	for_each_online_cpu(i) {

		ret = cpufreq_get_policy(&policy, i);
		if (ret)
			continue;

		policy.cur = policy.max;
		cpufreq_update_policy(i);
	}
}

#ifdef CONFIG_POWERSUSPEND
static void __ref intelli_plug_resume(struct power_suspend *handler)
#else
static void __ref intelli_plug_late_resume(struct early_suspend *handler)
#endif
{
	int num_of_active_cores = 0;
	int i = 0;

	if (atomic_read(&intelli_plug_active) == 1) {
		mutex_lock(&intelli_plug_mutex);
		/* keep cores awake long enough for faster wake up */
		persist_count = busy_persistence;
		hotplug_suspended = false;
		mutex_unlock(&intelli_plug_mutex);

		/* wake up everyone */
		if (eco_mode_active == 1)
			num_of_active_cores = 2;
		else if (strict_mode_active == 1)
			num_of_active_cores = 1;
		else
			num_of_active_cores = num_possible_cpus();

		for (i = 1; i < num_of_active_cores; i++) {
			cpu_up(i);
		}
		screen_off_limit(false);
		wakeup_boost();
	}
}

#ifdef CONFIG_POWERSUSPEND
static struct power_suspend intelli_plug_power_suspend_driver = {
	.suspend = intelli_plug_suspend,
	.resume = intelli_plug_resume,
};
#else
static struct early_suspend intelli_plug_early_suspend_struct_driver = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 10,
	.suspend = intelli_plug_early_suspend,
	.resume = intelli_plug_late_resume,
};
#endif
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

static int __ref intelli_plug_start(void)
{
	intelliplug_wq = create_singlethread_workqueue("intelliplug");

	if (!intelliplug_wq) {
		printk(KERN_ERR "Failed to create intelliplug \
				workqueue\n");
		return -EFAULT;
	}

	sampling_time_on = 10;
	sampling_time = 10;
	hotplug_suspended = false;

	INIT_DELAYED_WORK(&intelli_plug_work, intelli_plug_work_fn);

	queue_delayed_work_on(0, intelliplug_wq, &intelli_plug_work,
			msecs_to_jiffies(sampling_time_on));

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

	cancel_delayed_work_sync(&intelli_plug_work);

	destroy_workqueue(intelliplug_wq);
}

#define show_one(file_name, object)				\
static ssize_t show_##file_name					\
(struct kobject *kobj, struct attribute *attr, char *buf)	\
{								\
	return sprintf(buf, "%u\n", object);			\
}

static ssize_t show_intelli_plug_active(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
			atomic_read(&intelli_plug_active));
}

show_one(eco_mode_active, eco_mode_active);
show_one(strict_mode_active, strict_mode_active);
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
(struct kobject *kobj, struct attribute *attr,	\
	const char *buf, size_t count)		\
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

static void intelli_plug_active_eval_fn(unsigned int status)
{
	int ret = 0;

	if (status == 1) {
		ret = intelli_plug_start();
		if (ret)
			status = 0;
	} else {
		intelli_plug_stop();
	}

	atomic_set(&intelli_plug_active, status);
}

static ssize_t store_intelli_plug_active(struct kobject *kobj,
				struct attribute *attr,
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

static struct kobj_attribute intelli_plug_active_attr =
	__ATTR(intelli_plug_active, 0664, show_intelli_plug_active,
			store_intelli_plug_active);

static struct kobj_attribute eco_mode_active_attr =
	__ATTR(eco_mode_active, 0664, show_eco_mode_active,
			store_eco_mode_active);

static struct kobj_attribute strict_mode_active_attr =
	__ATTR(strict_mode_active, 0664, show_strict_mode_active,
			store_strict_mode_active);

static struct kobj_attribute def_sampling_ms_attr =
	__ATTR(def_sampling_ms, 0664, show_def_sampling_ms,
			store_def_sampling_ms);

static struct kobj_attribute busy_sampling_ms_attr =
	__ATTR(busy_sampling_ms, 0664, show_busy_sampling_ms,
			store_busy_sampling_ms);

static struct kobj_attribute dual_core_persistence_attr =
	__ATTR(dual_core_persistence, 0664, show_dual_core_persistence,
			store_dual_core_persistence);

static struct kobj_attribute tri_core_persistence_attr =
	__ATTR(tri_core_persistence, 0664, show_tri_core_persistence,
			store_tri_core_persistence);

static struct kobj_attribute quad_core_persistence_attr =
	__ATTR(quad_core_persistence, 0664, show_quad_core_persistence,
			store_quad_core_persistence);

static struct kobj_attribute busy_persistence_attr =
	__ATTR(busy_persistence, 0664, show_busy_persistence,
			store_busy_persistence);

static struct kobj_attribute cpu_down_factor_attr =
	__ATTR(cpu_down_factor, 0664, show_cpu_down_factor,
			store_cpu_down_factor);

static struct kobj_attribute debug_intelli_plug_attr =
	__ATTR(debug_intelli_plug, 0664, show_debug_intelli_plug,
			store_debug_intelli_plug);

static struct kobj_attribute nr_fshift_attr =
	__ATTR(nr_fshift, 0664, show_nr_fshift,
			store_nr_fshift);

static struct kobj_attribute nr_run_hysteresis_attr =
	__ATTR(nr_run_hysteresis, 0664, show_nr_run_hysteresis,
			store_nr_run_hysteresis);

static struct kobj_attribute screen_off_max_attr =
	__ATTR(screen_off_max, 0664, show_screen_off_max,
			store_screen_off_max);

static struct attribute *intelli_plug_attrs[] = {
	&intelli_plug_active_attr.attr,
	&eco_mode_active_attr.attr,
	&strict_mode_active_attr.attr,
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
MODULE_AUTHOR("Alucard24 & Dorimanx");
MODULE_DESCRIPTION("'intell_plug' - An intelligent cpu hotplug driver for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

late_initcall(intelli_plug_init);
late_initexit(intelli_plug_exit);
