/*
 *  drivers/cpufreq/cpufreq_darkness.c
 *
 *  Copyright (C)  2011 Samsung Electronics co. ltd
 *    ByungChang Cha <bc.cha@samsung.com>
 *
 *  Based on ondemand governor
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Created by Alucard_24@xda
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define MIN_SAMPLING_RATE	10000

/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

static void do_darkness_timer(struct work_struct *work);

struct cpufreq_darkness_cpuinfo {
	u64 prev_cpu_wall;
	u64 prev_cpu_idle;
	struct cpufreq_frequency_table *freq_table;
	struct delayed_work work;
	struct cpufreq_policy *cur_policy;
	int cpu;
	bool governor_enabled;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_dbs_timer invocation. We do not want do_dbs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};
/*
 * mutex that serializes governor limit change with
 * do_darkness_timer invocation. We do not want do_darkness_timer to run
 * when user is changing the governor or limits.
 */
static DEFINE_PER_CPU(struct cpufreq_darkness_cpuinfo, od_darkness_cpuinfo);

static unsigned int darkness_enable;	/* number of CPUs using this policy */
/*
 * darkness_mutex protects darkness_enable in governor start/stop.
 */
static DEFINE_MUTEX(darkness_mutex);

/* darkness tuners */
static struct darkness_tuners {
	unsigned int sampling_rate;
	unsigned int io_is_busy;
} darkness_tuners_ins = {
	.sampling_rate = 60000,
	.io_is_busy = 0,
};

/************************** sysfs interface ************************/

/* cpufreq_darkness Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", darkness_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(io_is_busy, io_is_busy);

/* sampling_rate */
static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(input,10000);

	if (input == darkness_tuners_ins.sampling_rate)
		return count;

	darkness_tuners_ins.sampling_rate = input;

	return count;
}

/* io_is_busy */
static ssize_t store_io_is_busy(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input, j;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == darkness_tuners_ins.io_is_busy)
		return count;

	darkness_tuners_ins.io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpufreq_darkness_cpuinfo *j_darkness_cpuinfo;

		j_darkness_cpuinfo = &per_cpu(od_darkness_cpuinfo, j);

		j_darkness_cpuinfo->prev_cpu_idle = get_cpu_idle_time(j,
			&j_darkness_cpuinfo->prev_cpu_wall, darkness_tuners_ins.io_is_busy);
	}
	return count;
}

define_one_global_rw(sampling_rate);
define_one_global_rw(io_is_busy);

static struct attribute *darkness_attributes[] = {
	&sampling_rate.attr,
	&io_is_busy.attr,
	NULL
};

static struct attribute_group darkness_attr_group = {
	.attrs = darkness_attributes,
	.name = "darkness",
};

/************************** sysfs end ************************/

static void darkness_check_cpu(struct cpufreq_darkness_cpuinfo *this_darkness_cpuinfo)
{
	struct cpufreq_policy *cpu_policy;
	unsigned int min_freq;
	unsigned int max_freq;
	u64 cur_wall_time, cur_idle_time;
	unsigned int wall_time, idle_time;
	unsigned int index = 0;
	unsigned int next_freq = 0;
	int cur_load = -1;
	unsigned int cpu;
	int io_busy = darkness_tuners_ins.io_is_busy;

	cpu = this_darkness_cpuinfo->cpu;
	cpu_policy = this_darkness_cpuinfo->cur_policy;
	if (cpu_policy == NULL)
		return;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, io_busy);

	wall_time = (unsigned int)
			(cur_wall_time - this_darkness_cpuinfo->prev_cpu_wall);
	this_darkness_cpuinfo->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int)
			(cur_idle_time - this_darkness_cpuinfo->prev_cpu_idle);
	this_darkness_cpuinfo->prev_cpu_idle = cur_idle_time;

	/*printk(KERN_ERR "TIMER CPU[%u], wall[%u], idle[%u]\n",cpu, wall_time, idle_time);*/

	if (wall_time >= idle_time) { /*if wall_time < idle_time, evaluate cpu load next time*/
		cur_load = wall_time > idle_time ? (100 * (wall_time - idle_time)) / wall_time : 1;/*if wall_time is equal to idle_time cpu_load is equal to 1*/

		cpufreq_notify_utilization(cpu_policy, cur_load);

		/* Checking Frequency Limit */
		min_freq = cpu_policy->min;
		max_freq = cpu_policy->max;

		/* CPUs Online Scale Frequency*/
		next_freq = max(min(cur_load * (max_freq / 100), max_freq), min_freq);
		cpufreq_frequency_table_target(cpu_policy, this_darkness_cpuinfo->freq_table, next_freq,
			CPUFREQ_RELATION_H, &index);
		if (this_darkness_cpuinfo->freq_table[index].frequency != cpu_policy->cur) {
			cpufreq_frequency_table_target(cpu_policy, this_darkness_cpuinfo->freq_table, next_freq,
				CPUFREQ_RELATION_L, &index);
		} else {
			return;
		}

		next_freq = this_darkness_cpuinfo->freq_table[index].frequency;
		/*printk(KERN_ERR "FREQ CALC.: CPU[%u], load[%d], target freq[%u], cur freq[%u], min freq[%u], max_freq[%u]\n",cpu, cur_load, next_freq, cpu_policy->cur, cpu_policy->min, max_freq);*/
		if (next_freq != cpu_policy->cur) {
			__cpufreq_driver_target(cpu_policy, next_freq, CPUFREQ_RELATION_L);
		}
	}

}

static void do_darkness_timer(struct work_struct *work)
{
	struct cpufreq_darkness_cpuinfo *darkness_cpuinfo;
	int delay;
	unsigned int cpu;

	darkness_cpuinfo =	container_of(work, struct cpufreq_darkness_cpuinfo, work.work);
	cpu = darkness_cpuinfo->cpu;

	mutex_lock(&darkness_cpuinfo->timer_mutex);

	darkness_check_cpu(darkness_cpuinfo);

	delay = usecs_to_jiffies(darkness_tuners_ins.sampling_rate);
	/* We want all CPUs to do sampling nearly on
	 * same jiffy
	 */
	if (num_online_cpus() > 1) {
		delay -= jiffies % delay;
	}

	if (delay > 0)
		mod_delayed_work_on(cpu, system_wq, &darkness_cpuinfo->work, delay);
	else
		mod_delayed_work_on(cpu, system_wq, &darkness_cpuinfo->work, usecs_to_jiffies(MIN_SAMPLING_RATE));

	mutex_unlock(&darkness_cpuinfo->timer_mutex);
}

static int cpufreq_governor_darkness(struct cpufreq_policy *policy,
				unsigned int event)
{
	unsigned int cpu;
	struct cpufreq_darkness_cpuinfo *this_darkness_cpuinfo;
	int rc, delay;
	int io_busy;

	cpu = policy->cpu;
	io_busy = darkness_tuners_ins.io_is_busy;
	this_darkness_cpuinfo = &per_cpu(od_darkness_cpuinfo, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&darkness_mutex);

		this_darkness_cpuinfo->freq_table = cpufreq_frequency_get_table(cpu);

		this_darkness_cpuinfo->cpu = cpu;
		this_darkness_cpuinfo->cur_policy = policy;

		this_darkness_cpuinfo->prev_cpu_idle = get_cpu_idle_time(cpu, &this_darkness_cpuinfo->prev_cpu_wall, io_busy);

		darkness_enable++;
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (darkness_enable == 1) {
			rc = sysfs_create_group(cpufreq_global_kobject,
						&darkness_attr_group);
			if (rc) {
				darkness_enable--;
				mutex_unlock(&darkness_mutex);
				return rc;
			}
		}
		this_darkness_cpuinfo->governor_enabled = true;
		mutex_unlock(&darkness_mutex);

		mutex_init(&this_darkness_cpuinfo->timer_mutex);

		delay = usecs_to_jiffies(darkness_tuners_ins.sampling_rate);
		/* We want all CPUs to do sampling nearly on same jiffy */
		if (num_online_cpus() > 1) {
			delay -= jiffies % delay;
		}

		INIT_DEFERRABLE_WORK(&this_darkness_cpuinfo->work, do_darkness_timer);

		mod_delayed_work_on(this_darkness_cpuinfo->cpu, system_wq, &this_darkness_cpuinfo->work, delay);

		break;

	case CPUFREQ_GOV_STOP:
		cancel_delayed_work_sync(&this_darkness_cpuinfo->work);

		mutex_lock(&darkness_mutex);
		mutex_destroy(&this_darkness_cpuinfo->timer_mutex);

		this_darkness_cpuinfo->governor_enabled = false;

		this_darkness_cpuinfo->cur_policy = NULL;

		darkness_enable--;
		if (!darkness_enable) {
			sysfs_remove_group(cpufreq_global_kobject,
					   &darkness_attr_group);
		}
		mutex_unlock(&darkness_mutex);

		break;

	case CPUFREQ_GOV_LIMITS:
		if (!this_darkness_cpuinfo->cur_policy) {
			pr_debug("Unable to limit cpu freq due to cur_policy == NULL\n");
			return -EPERM;
		}
		mutex_lock(&this_darkness_cpuinfo->timer_mutex);
		if (policy->max < this_darkness_cpuinfo->cur_policy->cur)
			__cpufreq_driver_target(this_darkness_cpuinfo->cur_policy,
				policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_darkness_cpuinfo->cur_policy->cur)
			__cpufreq_driver_target(this_darkness_cpuinfo->cur_policy,
				policy->min, CPUFREQ_RELATION_L);
		mutex_unlock(&this_darkness_cpuinfo->timer_mutex);

		break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_DARKNESS
static
#endif
struct cpufreq_governor cpufreq_gov_darkness = {
	.name                   = "darkness",
	.governor               = cpufreq_governor_darkness,
	.owner                  = THIS_MODULE,
};

static int __init cpufreq_gov_darkness_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_darkness);
}

static void __exit cpufreq_gov_darkness_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_darkness);
}

MODULE_AUTHOR("Alucard24@XDA");
MODULE_DESCRIPTION("'cpufreq_darkness' - A dynamic cpufreq/cpuhotplug governor v4.5 (SnapDragon)");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_DARKNESS
fs_initcall(cpufreq_gov_darkness_init);
#else
module_init(cpufreq_gov_darkness_init);
#endif
module_exit(cpufreq_gov_darkness_exit);
