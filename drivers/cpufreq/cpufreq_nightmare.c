/*
 *  drivers/cpufreq/cpufreq_nightmare.c
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
/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

static void do_nightmare_timer(struct work_struct *work);

struct cpufreq_nightmare_cpuinfo {
	u64 prev_cpu_wall;
	u64 prev_cpu_idle;
	struct cpufreq_frequency_table *freq_table;
	struct delayed_work work;
	struct cpufreq_policy *cur_policy;
#if 0
	ktime_t time_stamp;
#endif
	int cpu;
	unsigned int enable:1;
	/*
	 * mutex that serializes governor limit change with
	 * do_nightmare_timer invocation. We do not want do_nightmare_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};

static DEFINE_PER_CPU(struct cpufreq_nightmare_cpuinfo, od_nightmare_cpuinfo);

static unsigned int nightmare_enable;	/* number of CPUs using this policy */
/*
 * nightmare_mutex protects nightmare_enable in governor start/stop.
 */
static DEFINE_MUTEX(nightmare_mutex);

static struct workqueue_struct *nightmare_wq;

/* nightmare tuners */
static struct nightmare_tuners {
	unsigned int sampling_rate;
	int inc_cpu_load_at_min_freq;
	int inc_cpu_load;
	int dec_cpu_load;
	int freq_for_responsiveness;
	int freq_for_responsiveness_max;
	int freq_up_brake_at_min_freq;
	int freq_up_brake;
	int freq_step_at_min_freq;
	int freq_step;
	int freq_step_dec;
	int freq_step_dec_at_max_freq;
} nightmare_tuners_ins = {
	.sampling_rate = 60000,
	.inc_cpu_load_at_min_freq = 40,
	.inc_cpu_load = 70,
	.dec_cpu_load = 50,
	.freq_for_responsiveness = 2265600,
	.freq_for_responsiveness_max = 2803200,
	.freq_step_at_min_freq = 40,
	.freq_step = 40,
	.freq_up_brake_at_min_freq = 30,
	.freq_up_brake = 30,
	.freq_step_dec = 10,
	.freq_step_dec_at_max_freq = 10,
};

/************************** sysfs interface ************************/

/* cpufreq_nightmare Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", nightmare_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(inc_cpu_load_at_min_freq, inc_cpu_load_at_min_freq);
show_one(inc_cpu_load, inc_cpu_load);
show_one(dec_cpu_load, dec_cpu_load);
show_one(freq_for_responsiveness, freq_for_responsiveness);
show_one(freq_for_responsiveness_max, freq_for_responsiveness_max);
show_one(freq_step_at_min_freq, freq_step_at_min_freq);
show_one(freq_step, freq_step);
show_one(freq_up_brake_at_min_freq, freq_up_brake_at_min_freq);
show_one(freq_up_brake, freq_up_brake);
show_one(freq_step_dec, freq_step_dec);
show_one(freq_step_dec_at_max_freq, freq_step_dec_at_max_freq);

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
	
	if (input == nightmare_tuners_ins.sampling_rate)
		return count;

	nightmare_tuners_ins.sampling_rate = input;

	return count;
}

/* inc_cpu_load_at_min_freq */
static ssize_t store_inc_cpu_load_at_min_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1) {
		return -EINVAL;
	}

	input = min(input,nightmare_tuners_ins.inc_cpu_load);

	if (input == nightmare_tuners_ins.inc_cpu_load_at_min_freq)
		return count;

	nightmare_tuners_ins.inc_cpu_load_at_min_freq = input;

	return count;
}

/* inc_cpu_load */
static ssize_t store_inc_cpu_load(struct kobject *a, struct attribute *b,
					const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == nightmare_tuners_ins.inc_cpu_load)
		return count;

	nightmare_tuners_ins.inc_cpu_load = input;

	return count;
}

/* dec_cpu_load */
static ssize_t store_dec_cpu_load(struct kobject *a, struct attribute *b,
					const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,95),5);

	if (input == nightmare_tuners_ins.dec_cpu_load)
		return count;

	nightmare_tuners_ins.dec_cpu_load = input;

	return count;
}

/* freq_for_responsiveness */
static ssize_t store_freq_for_responsiveness(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	if (input == nightmare_tuners_ins.freq_for_responsiveness)
		return count;

	nightmare_tuners_ins.freq_for_responsiveness = input;

	return count;
}

/* freq_for_responsiveness_max */
static ssize_t store_freq_for_responsiveness_max(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	if (input == nightmare_tuners_ins.freq_for_responsiveness_max)
		return count;

	nightmare_tuners_ins.freq_for_responsiveness_max = input;

	return count;
}

/* freq_step_at_min_freq */
static ssize_t store_freq_step_at_min_freq(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == nightmare_tuners_ins.freq_step_at_min_freq)
		return count;

	nightmare_tuners_ins.freq_step_at_min_freq = input;

	return count;
}

/* freq_step */
static ssize_t store_freq_step(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == nightmare_tuners_ins.freq_step)
		return count;

	nightmare_tuners_ins.freq_step = input;

	return count;
}

/* freq_up_brake_at_min_freq */
static ssize_t store_freq_up_brake_at_min_freq(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == nightmare_tuners_ins.freq_up_brake_at_min_freq) {/* nothing to do */
		return count;
	}

	nightmare_tuners_ins.freq_up_brake_at_min_freq = input;

	return count;
}

/* freq_up_brake */
static ssize_t store_freq_up_brake(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == nightmare_tuners_ins.freq_up_brake) {/* nothing to do */
		return count;
	}

	nightmare_tuners_ins.freq_up_brake = input;

	return count;
}

/* freq_step_dec */
static ssize_t store_freq_step_dec(struct kobject *a, struct attribute *b,
				       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == nightmare_tuners_ins.freq_step_dec) {/* nothing to do */
		return count;
	}

	nightmare_tuners_ins.freq_step_dec = input;

	return count;
}

/* freq_step_dec_at_max_freq */
static ssize_t store_freq_step_dec_at_max_freq(struct kobject *a, struct attribute *b,
				       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == nightmare_tuners_ins.freq_step_dec_at_max_freq) {/* nothing to do */
		return count;
	}

	nightmare_tuners_ins.freq_step_dec_at_max_freq = input;

	return count;
}

define_one_global_rw(sampling_rate);
define_one_global_rw(inc_cpu_load_at_min_freq);
define_one_global_rw(inc_cpu_load);
define_one_global_rw(dec_cpu_load);
define_one_global_rw(freq_for_responsiveness);
define_one_global_rw(freq_for_responsiveness_max);
define_one_global_rw(freq_step_at_min_freq);
define_one_global_rw(freq_step);
define_one_global_rw(freq_up_brake_at_min_freq);
define_one_global_rw(freq_up_brake);
define_one_global_rw(freq_step_dec);
define_one_global_rw(freq_step_dec_at_max_freq);

static struct attribute *nightmare_attributes[] = {
	&sampling_rate.attr,
	&inc_cpu_load_at_min_freq.attr,
	&inc_cpu_load.attr,
	&dec_cpu_load.attr,
	&freq_for_responsiveness.attr,
	&freq_for_responsiveness_max.attr,
	&freq_step_at_min_freq.attr,
	&freq_step.attr,
	&freq_up_brake_at_min_freq.attr,
	&freq_up_brake.attr,
	&freq_step_dec.attr,
	&freq_step_dec_at_max_freq.attr,
	NULL
};

static struct attribute_group nightmare_attr_group = {
	.attrs = nightmare_attributes,
	.name = "nightmare",
};

/************************** sysfs end ************************/

#if 0
/* Will return if we need to evaluate cpu load again or not */
static inline bool need_load_eval(struct cpufreq_nightmare_cpuinfo *this_nightmare_cpuinfo,
		unsigned int sampling_rate)
{
	ktime_t time_now = ktime_get();
	s64 delta_us = ktime_us_delta(time_now, this_nightmare_cpuinfo->time_stamp);

	/* Do nothing if we recently have sampled */
	if (delta_us < (s64)(sampling_rate / 2))
		return false;
	else
		this_nightmare_cpuinfo->time_stamp = time_now;

	return true;
}
#endif

static void nightmare_check_cpu(struct cpufreq_nightmare_cpuinfo *this_nightmare_cpuinfo)
{
	struct cpufreq_policy *cpu_policy;
	unsigned int min_freq;
	unsigned int max_freq;
	unsigned int freq_for_responsiveness = nightmare_tuners_ins.freq_for_responsiveness;
	unsigned int freq_for_responsiveness_max = nightmare_tuners_ins.freq_for_responsiveness_max;
	int dec_cpu_load = nightmare_tuners_ins.dec_cpu_load;
	int inc_cpu_load = nightmare_tuners_ins.inc_cpu_load;
	int freq_step = nightmare_tuners_ins.freq_step;
	int freq_up_brake = nightmare_tuners_ins.freq_up_brake;
	int freq_step_dec = nightmare_tuners_ins.freq_step_dec;
	u64 cur_wall_time, cur_idle_time;
	unsigned int wall_time, idle_time;
	unsigned int index = 0;
	unsigned int tmp_freq = 0;
	unsigned int next_freq = 0;
	int cur_load = -1;
	unsigned int cpu;

	cpu = this_nightmare_cpuinfo->cpu;
	cpu_policy = this_nightmare_cpuinfo->cur_policy;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);

	wall_time = (unsigned int)
			(cur_wall_time - this_nightmare_cpuinfo->prev_cpu_wall);
	this_nightmare_cpuinfo->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int)
			(cur_idle_time - this_nightmare_cpuinfo->prev_cpu_idle);
	this_nightmare_cpuinfo->prev_cpu_idle = cur_idle_time;

	/*printk(KERN_ERR "TIMER CPU[%u], wall[%u], idle[%u]\n",cpu, wall_time, idle_time);*/
	if (wall_time >= idle_time) { /*if wall_time < idle_time, evaluate cpu load next time*/
		cur_load = wall_time > idle_time ? (100 * (wall_time - idle_time)) / wall_time : 1;/*if wall_time is equal to idle_time cpu_load is equal to 1*/

		cpufreq_notify_utilization(cpu_policy, cur_load);

		/* Checking Frequency Limit */
		min_freq = cpu_policy->min;
		max_freq = cpu_policy->max;		
		/* CPUs Online Scale Frequency*/
		if (cpu_policy->cur < freq_for_responsiveness) {
			inc_cpu_load = nightmare_tuners_ins.inc_cpu_load_at_min_freq;
			freq_step = nightmare_tuners_ins.freq_step_at_min_freq;
			freq_up_brake = nightmare_tuners_ins.freq_up_brake_at_min_freq;
		} else if (cpu_policy->cur > freq_for_responsiveness_max) {
			freq_step_dec = nightmare_tuners_ins.freq_step_dec_at_max_freq;
		}		
		/* Check for frequency increase or for frequency decrease */
		if (cur_load >= inc_cpu_load && cpu_policy->cur < max_freq) {
			tmp_freq = max(min((cpu_policy->cur + ((cur_load + freq_step - freq_up_brake == 0 ? 1 : cur_load + freq_step - freq_up_brake) * 3780)), max_freq), min_freq);
		} else if (cur_load < dec_cpu_load && cpu_policy->cur > min_freq) {
			tmp_freq = max(min((cpu_policy->cur - ((100 - cur_load + freq_step_dec == 0 ? 1 : 100 - cur_load + freq_step_dec) * 3780)), max_freq), min_freq);
		} else {
			/* if cpu frequency is already at maximum or minimum or cur_load is between inc_cpu_load and dec_cpu_load var, we don't need to set frequency! */
			return;
		}
		cpufreq_frequency_table_target(cpu_policy, this_nightmare_cpuinfo->freq_table, tmp_freq,
			CPUFREQ_RELATION_L, &index);
	 	next_freq = this_nightmare_cpuinfo->freq_table[index].frequency;
		/*printk(KERN_ERR "FREQ CALC.: CPU[%u], load[%d], target freq[%u], cur freq[%u], min freq[%u], max_freq[%u]\n",cpu, cur_load, next_freq, cpu_policy->cur, cpu_policy->min, max_freq);*/
		if (next_freq != cpu_policy->cur) {
			__cpufreq_driver_target(cpu_policy, next_freq, CPUFREQ_RELATION_L);
		}
	}

}

static void do_nightmare_timer(struct work_struct *work)
{
	struct cpufreq_nightmare_cpuinfo *nightmare_cpuinfo;
	unsigned int sampling_rate;
	int delay;
	unsigned int cpu;

	nightmare_cpuinfo = container_of(work, struct cpufreq_nightmare_cpuinfo, work.work);
	cpu = nightmare_cpuinfo->cpu;

	mutex_lock(&nightmare_cpuinfo->timer_mutex);

	sampling_rate = nightmare_tuners_ins.sampling_rate;
	delay = usecs_to_jiffies(sampling_rate);
	/* We want all CPUs to do sampling nearly on
	 * same jiffy
	 */
	if (num_online_cpus() > 1) {
		delay -= jiffies % delay;
	}

#if 0
	if (need_load_eval(nightmare_cpuinfo, sampling_rate))
#endif
		nightmare_check_cpu(nightmare_cpuinfo);

	queue_delayed_work_on(cpu, nightmare_wq, &nightmare_cpuinfo->work, delay);
	mutex_unlock(&nightmare_cpuinfo->timer_mutex);
}

static int cpufreq_governor_nightmare(struct cpufreq_policy *policy,
				unsigned int event)
{
	unsigned int cpu;
	struct cpufreq_nightmare_cpuinfo *this_nightmare_cpuinfo;
	int rc, delay;

	cpu = policy->cpu;
	this_nightmare_cpuinfo = &per_cpu(od_nightmare_cpuinfo, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&nightmare_mutex);

		this_nightmare_cpuinfo->cur_policy = policy;

		this_nightmare_cpuinfo->prev_cpu_idle = get_cpu_idle_time(cpu, &this_nightmare_cpuinfo->prev_cpu_wall, 0);

		this_nightmare_cpuinfo->freq_table = cpufreq_frequency_get_table(cpu);
		this_nightmare_cpuinfo->cpu = cpu;

		nightmare_enable++;
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (nightmare_enable == 1) {
			rc = sysfs_create_group(cpufreq_global_kobject,
						&nightmare_attr_group);
			if (rc) {
				nightmare_enable--;
				mutex_unlock(&nightmare_mutex);
				return rc;
			}
		}
		mutex_unlock(&nightmare_mutex);

		mutex_init(&this_nightmare_cpuinfo->timer_mutex);

#if 0
		/* Initiate timer time stamp */
		this_nightmare_cpuinfo->time_stamp = ktime_get();
#endif

		delay=usecs_to_jiffies(nightmare_tuners_ins.sampling_rate);
		if (num_online_cpus() > 1) {
			delay -= jiffies % delay;
		}

		this_nightmare_cpuinfo->enable = 1;
		INIT_DEFERRABLE_WORK(&this_nightmare_cpuinfo->work, do_nightmare_timer);
		queue_delayed_work_on(this_nightmare_cpuinfo->cpu, nightmare_wq, &this_nightmare_cpuinfo->work, delay);

		break;

	case CPUFREQ_GOV_STOP:
		cancel_delayed_work_sync(&this_nightmare_cpuinfo->work);

		mutex_lock(&nightmare_mutex);
		mutex_destroy(&this_nightmare_cpuinfo->timer_mutex);

		this_nightmare_cpuinfo->enable = 0;

		nightmare_enable--;
		if (!nightmare_enable) {
			sysfs_remove_group(cpufreq_global_kobject,
					   &nightmare_attr_group);			
		}
		mutex_unlock(&nightmare_mutex);
		
		break;

	case CPUFREQ_GOV_LIMITS:
		if (!this_nightmare_cpuinfo->cur_policy) {
			pr_debug("Unable to limit cpu freq due to cur_policy == NULL\n");
			return -EPERM;
		}
		mutex_lock(&this_nightmare_cpuinfo->timer_mutex);
		if (policy->max < this_nightmare_cpuinfo->cur_policy->cur)
			__cpufreq_driver_target(this_nightmare_cpuinfo->cur_policy,
				policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_nightmare_cpuinfo->cur_policy->cur)
			__cpufreq_driver_target(this_nightmare_cpuinfo->cur_policy,
				policy->min, CPUFREQ_RELATION_L);
		mutex_unlock(&this_nightmare_cpuinfo->timer_mutex);

		break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_NIGHTMARE
static
#endif
struct cpufreq_governor cpufreq_gov_nightmare = {
	.name                   = "nightmare",
	.governor               = cpufreq_governor_nightmare,
	.owner                  = THIS_MODULE,
};

static int __init cpufreq_gov_nightmare_init(void)
{
	nightmare_wq = alloc_workqueue("nightmare_wq", WQ_HIGHPRI, 0);

	if (!nightmare_wq) {
		printk(KERN_ERR "Failed to create nightmare workqueue\n");
		return -EFAULT;
	}

	return cpufreq_register_governor(&cpufreq_gov_nightmare);
}

static void __exit cpufreq_gov_nightmare_exit(void)
{
	destroy_workqueue(nightmare_wq);
	cpufreq_unregister_governor(&cpufreq_gov_nightmare);
}

MODULE_AUTHOR("Alucard24@XDA");
MODULE_DESCRIPTION("'cpufreq_nightmare' - A dynamic cpufreq/cpuhotplug governor v4.1 (SnapDragon)");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_NIGHTMARE
fs_initcall(cpufreq_gov_nightmare_init);
#else
module_init(cpufreq_gov_nightmare_init);
#endif
module_exit(cpufreq_gov_nightmare_exit);
