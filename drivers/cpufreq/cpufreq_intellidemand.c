/*
 *  drivers/cpufreq/cpufreq_intellidemand.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  2013 The Linux Foundation. All rights reserved.
 *            (C)  2013 Paul Reioux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/workqueue.h>
#include <linux/slab.h>

#define INTELLIDEMAND_MAJOR_VERSION    5
#define INTELLIDEMAND_MINOR_VERSION    5

/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

#define DEF_SAMPLING_RATE			(50000)
#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(60)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(3)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(70)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)
#define MIN_FREQUENCY_UP_THRESHOLD		(11)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)
#define MIN_FREQUENCY_DOWN_DIFFERENTIAL		(1)

#define DEF_FREQ_STEP				(25)
#define DEF_STEP_UP_EARLY_HISPEED		(1958400)
#define DEF_STEP_UP_INTERIM_HISPEED		(2265600)
#define DEF_SAMPLING_EARLY_HISPEED_FACTOR	(2)
#define DEF_SAMPLING_INTERIM_HISPEED_FACTOR	(3)

/* PATCH : SMART_UP */
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

#define SMART_UP_PLUS (0)
#define SMART_UP_SLOW_UP_AT_HIGH_FREQ (1)
#define SUP_MAX_STEP (3)
#define SUP_CORE_NUM (4)
#define SUP_SLOW_UP_DUR (5)
#define SUP_SLOW_UP_DUR_DEFAULT (2)

#define SUP_HIGH_SLOW_UP_DUR (5)
#define SUP_FREQ_LEVEL (14)

#if defined(SMART_UP_PLUS)
static unsigned int SUP_THRESHOLD_STEPS[SUP_MAX_STEP] = {85, 90, 95};
static unsigned int SUP_FREQ_STEPS[SUP_MAX_STEP] = {4, 3, 2};
typedef struct{
	unsigned int freq_idx;
	unsigned int freq_value;
} freq_table_idx;
static freq_table_idx pre_freq_idx[SUP_CORE_NUM] = {};

#endif


#if defined(SMART_UP_SLOW_UP_AT_HIGH_FREQ)

#define SUP_SLOW_UP_FREQUENCY			(1728000)
#define SUP_HIGH_SLOW_UP_FREQUENCY		(2265600)
#define SUP_SLOW_UP_LOAD			(75)

typedef struct {
	unsigned int hist_max_load[SUP_SLOW_UP_DUR];
	unsigned int hist_load_cnt;
} history_load;
static void reset_hist(history_load *hist_load);
static history_load hist_load[SUP_CORE_NUM] = {};

typedef struct {
	unsigned int hist_max_load[SUP_HIGH_SLOW_UP_DUR];
	unsigned int hist_load_cnt;
} history_load_high;
static void reset_hist_high(history_load_high *hist_load);
static history_load_high hist_load_high[SUP_CORE_NUM] = {};

#endif


/*
 * The polling frequency of this governor depends on the capability of
 * the processor. Default polling frequency is 1000 times the transition
 * latency of the processor. The governor will work on any processor with
 * transition latency <= 10mS, using appropriate sampling
 * rate.
 * For CPUs with transition latency > 10mS (mostly drivers with CPUFREQ_ETERNAL)
 * this governor will not work.
 * All times here are in uS.
 */
#define MIN_SAMPLING_RATE_RATIO			(2)

static unsigned int min_sampling_rate;

#define LATENCY_MULTIPLIER			(1000)
#define MIN_LATENCY_MULTIPLIER			(100)
#define TRANSITION_LATENCY_LIMIT		(10 * 1000 * 1000)

#define POWERSAVE_BIAS_MAXLEVEL			(1000)
#define POWERSAVE_BIAS_MINLEVEL			(-1000)

static void do_dbs_timer(struct work_struct *work);

/* Sampling types */
enum {DBS_NORMAL_SAMPLE, DBS_SUB_SAMPLE};

struct cpu_dbs_info_s {
	u64 prev_cpu_idle;
	u64 prev_cpu_iowait;
	u64 prev_cpu_wall;
	u64 prev_cpu_nice;
	struct cpufreq_policy *cur_policy;
	struct delayed_work work;
	struct cpufreq_frequency_table *freq_table;
	unsigned int freq_lo;
	unsigned int freq_lo_jiffies;
	unsigned int freq_hi_jiffies;
	unsigned int rate_mult;
	unsigned int prev_load;
	unsigned int max_load;
	int cpu;
	unsigned int sample_type:1;
	unsigned int freq_stay_count;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_dbs_timer invocation. We do not want do_dbs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, id_cpu_dbs_info);

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info);
static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info);

static unsigned int dbs_enable;	/* number of CPUs using this policy */

/*
 * dbs_mutex protects dbs_enable and dbs_info during start/stop.
 */
static DEFINE_MUTEX(dbs_mutex);

static struct workqueue_struct *dbs_wq;

struct dbs_work_struct {
	struct work_struct work;
	unsigned int cpu;
};

static DEFINE_PER_CPU(struct dbs_work_struct, dbs_refresh_work);

static struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int up_threshold;
	unsigned int up_threshold_multi_core;
	unsigned int down_differential;
	unsigned int down_differential_multi_core;
	unsigned int optimal_freq;
	unsigned int up_threshold_any_cpu_load;
	unsigned int sync_freq;
	unsigned int ignore_nice;
	unsigned int sampling_down_factor;
	int			 powersave_bias;
	unsigned int io_is_busy;
	/* 20130711 smart_up */
	unsigned int smart_up;
	unsigned int smart_slow_up_load;
	unsigned int smart_slow_up_freq;
	unsigned int smart_slow_up_dur;
	unsigned int smart_high_slow_up_freq;
	unsigned int smart_high_slow_up_dur;
	unsigned int smart_each_off;
	/* end smart_up */
	unsigned int freq_step;
	unsigned int step_up_early_hispeed;
	unsigned int step_up_interim_hispeed;
	unsigned int sampling_early_factor;
	unsigned int sampling_interim_factor;
	unsigned int two_phase_freq;
} dbs_tuners_ins = {
	.up_threshold_multi_core = DEF_FREQUENCY_UP_THRESHOLD,
	.up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
	.sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
	.down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
	.down_differential_multi_core = MICRO_FREQUENCY_DOWN_DIFFERENTIAL,
	.up_threshold_any_cpu_load = DEF_FREQUENCY_UP_THRESHOLD,
	.ignore_nice = 0,
	.powersave_bias = 0,
	.sync_freq = 1574400,
	.optimal_freq = 1574400,
	/* 20130711 smart_up */
	.smart_up = SMART_UP_PLUS,
	.smart_slow_up_load = SUP_SLOW_UP_LOAD,
	.smart_slow_up_freq = SUP_SLOW_UP_FREQUENCY,
	.smart_slow_up_dur = SUP_SLOW_UP_DUR_DEFAULT,
	.smart_high_slow_up_freq = SUP_HIGH_SLOW_UP_FREQUENCY,
	.smart_high_slow_up_dur = SUP_HIGH_SLOW_UP_DUR,
	.smart_each_off = 0,
	/* end smart_up */
	.freq_step = DEF_FREQ_STEP,
	.step_up_early_hispeed = DEF_STEP_UP_EARLY_HISPEED,
	.step_up_interim_hispeed = DEF_STEP_UP_INTERIM_HISPEED,
	.sampling_early_factor = DEF_SAMPLING_EARLY_HISPEED_FACTOR,
	.sampling_interim_factor = DEF_SAMPLING_INTERIM_HISPEED_FACTOR,
	.two_phase_freq = 0,
	.io_is_busy = 0,
	.sampling_rate = DEF_SAMPLING_RATE,
};

static inline u64 get_cpu_iowait_time(unsigned int cpu, u64 *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_jiffies,
 * freq_lo, and freq_lo_jiffies in percpu area for averaging freqs.
 */
static unsigned int powersave_bias_target(struct cpufreq_policy *policy,
					  unsigned int freq_next,
					  unsigned int relation)
{
	unsigned int freq_req, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	int freq_reduc;
	struct cpu_dbs_info_s *dbs_info = &per_cpu(id_cpu_dbs_info,
						   policy->cpu);

	if (!dbs_info->freq_table) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_next;
	}

	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_next,
			relation, &index);
	freq_req = dbs_info->freq_table[index].frequency;
	freq_reduc = freq_req * dbs_tuners_ins.powersave_bias / 1000;
	freq_avg = freq_req - freq_reduc;

	/* Find freq bounds for freq_avg in freq_table */
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_H, &index);
	freq_lo = dbs_info->freq_table[index].frequency;
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_L, &index);
	freq_hi = dbs_info->freq_table[index].frequency;

	/* Find out how long we have to be in hi and lo freqs */
	if (freq_hi == freq_lo) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_lo;
	}
	jiffies_total = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	jiffies_hi = (freq_avg - freq_lo) * jiffies_total;
	jiffies_hi += ((freq_hi - freq_lo) / 2);
	jiffies_hi /= (freq_hi - freq_lo);
	jiffies_lo = jiffies_total - jiffies_hi;
	dbs_info->freq_lo = freq_lo;
	dbs_info->freq_lo_jiffies = jiffies_lo;
	dbs_info->freq_hi_jiffies = jiffies_hi;
	return freq_hi;
}

static int intellidemand_powersave_bias_setspeed(struct cpufreq_policy *policy,
					    struct cpufreq_policy *altpolicy,
					    int level)
{
	if (level == POWERSAVE_BIAS_MAXLEVEL) {
		/* maximum powersave; set to lowest frequency */
		__cpufreq_driver_target(policy,
			(altpolicy) ? altpolicy->min : policy->min,
			CPUFREQ_RELATION_L);
		return 1;
	} else if (level == POWERSAVE_BIAS_MINLEVEL) {
		/* minimum powersave; set to highest frequency */
		__cpufreq_driver_target(policy,
			(altpolicy) ? altpolicy->max : policy->max,
			CPUFREQ_RELATION_H);
		return 1;
	}
	return 0;
}

static void intellidemand_powersave_bias_init_cpu(int cpu)
{
	struct cpu_dbs_info_s *dbs_info = &per_cpu(id_cpu_dbs_info, cpu);
	dbs_info->freq_table = cpufreq_frequency_get_table(cpu);
	dbs_info->freq_lo = 0;
}

static void intellidemand_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		intellidemand_powersave_bias_init_cpu(i);
	}
}

/************************** sysfs interface ************************/

static ssize_t show_sampling_rate_min(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_sampling_rate);
}

define_one_global_ro(sampling_rate_min);

/* cpufreq_intellidemand Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)	      \
{									\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(io_is_busy, io_is_busy);
show_one(up_threshold, up_threshold);
show_one(up_threshold_multi_core, up_threshold_multi_core);
show_one(down_differential, down_differential);
show_one(sampling_down_factor, sampling_down_factor);
show_one(ignore_nice_load, ignore_nice);
show_one(optimal_freq, optimal_freq);
show_one(up_threshold_any_cpu_load, up_threshold_any_cpu_load);
show_one(sync_freq, sync_freq);
/* 20130711 smart_up */
show_one(smart_up, smart_up);
show_one(smart_slow_up_load, smart_slow_up_load);
show_one(smart_slow_up_freq, smart_slow_up_freq);
show_one(smart_slow_up_dur, smart_slow_up_dur);
show_one(smart_high_slow_up_freq, smart_high_slow_up_freq);
show_one(smart_high_slow_up_dur, smart_high_slow_up_dur);
show_one(smart_each_off, smart_each_off);
/* end smart_up */
show_one(freq_step, freq_step);
show_one(step_up_early_hispeed, step_up_early_hispeed);
show_one(step_up_interim_hispeed, step_up_interim_hispeed);
show_one(sampling_early_factor, sampling_early_factor);
show_one(sampling_interim_factor, sampling_interim_factor);

static ssize_t show_powersave_bias
(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", dbs_tuners_ins.powersave_bias);
}

static int two_phase_freq_array[NR_CPUS] = {[0 ... NR_CPUS-1] = 1958400} ;

static ssize_t show_two_phase_freq
(struct kobject *kobj, struct attribute *attr, char *buf)
{
	int i = 0 ;
	int shift = 0 ;
	char *buf_pos = buf;
	for ( i = 0 ; i < NR_CPUS; i++) {
		shift = sprintf(buf_pos,"%d,",two_phase_freq_array[i]);
		buf_pos += shift;
	}
	*(buf_pos-1) = '\0';
	return strlen(buf);
}

static ssize_t store_two_phase_freq(struct kobject *a, struct attribute *b,
		const char *buf, size_t count)
{

	int ret = 0;
	if (NR_CPUS == 1)
		ret = sscanf(buf,"%u",&two_phase_freq_array[0]);
	else if (NR_CPUS == 2)
		ret = sscanf(buf,"%u,%u",&two_phase_freq_array[0],
				&two_phase_freq_array[1]);
	else if (NR_CPUS == 4)
		ret = sscanf(buf, "%u,%u,%u,%u", &two_phase_freq_array[0],
				&two_phase_freq_array[1],
				&two_phase_freq_array[2],
				&two_phase_freq_array[3]);
	if (ret < NR_CPUS)
		return -EINVAL;

	return count;
}

static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	dbs_tuners_ins.sampling_rate = max(input, min_sampling_rate);

	return count;
}

static ssize_t store_sync_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input <= 960000)
		return count;
	dbs_tuners_ins.sync_freq = input;

	return count;
}

static ssize_t store_io_is_busy(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.io_is_busy = !!input;

	return count;
}

static ssize_t store_optimal_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input <= 960000)
		return count;
	dbs_tuners_ins.optimal_freq = input;

	return count;
}

static ssize_t store_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold = input;

	return count;
}

static ssize_t store_up_threshold_multi_core(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold_multi_core = input;

	return count;
}

static ssize_t store_up_threshold_any_cpu_load(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold_any_cpu_load = input;

	return count;
}

static ssize_t store_down_differential(struct kobject *a, struct attribute *b,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input >= dbs_tuners_ins.up_threshold ||
			input < MIN_FREQUENCY_DOWN_DIFFERENTIAL) {
		return -EINVAL;
	}

	dbs_tuners_ins.down_differential = input;

	return count;
}

static ssize_t store_sampling_down_factor(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	dbs_tuners_ins.sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(id_cpu_dbs_info, j);
		dbs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice_load(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == dbs_tuners_ins.ignore_nice) { /* nothing to do */
		return count;
	}
	dbs_tuners_ins.ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(id_cpu_dbs_info, j);
		dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&dbs_info->prev_cpu_wall,
						dbs_tuners_ins.io_is_busy);
		if (dbs_tuners_ins.ignore_nice)
			dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_powersave_bias(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	int input  = 0;
	int bypass = 0;
	int ret, cpu, reenable_timer, j;
	struct cpu_dbs_info_s *dbs_info;

	struct cpumask cpus_timer_done;
	cpumask_clear(&cpus_timer_done);

	ret = sscanf(buf, "%d", &input);

	if (ret != 1)
		return -EINVAL;

	if (input >= POWERSAVE_BIAS_MAXLEVEL) {
		input  = POWERSAVE_BIAS_MAXLEVEL;
		bypass = 1;
	} else if (input <= POWERSAVE_BIAS_MINLEVEL) {
		input  = POWERSAVE_BIAS_MINLEVEL;
		bypass = 1;
	}

	if (input == dbs_tuners_ins.powersave_bias) {
		/* no change */
		return count;
	}

	reenable_timer = ((dbs_tuners_ins.powersave_bias ==
				POWERSAVE_BIAS_MAXLEVEL) ||
				(dbs_tuners_ins.powersave_bias ==
				POWERSAVE_BIAS_MINLEVEL));

	dbs_tuners_ins.powersave_bias = input;

	get_online_cpus();
	mutex_lock(&dbs_mutex);

	if (!bypass) {
		if (reenable_timer) {
			/* reinstate dbs timer */
			for_each_online_cpu(cpu) {
				if (lock_policy_rwsem_write(cpu) < 0)
					continue;

				dbs_info = &per_cpu(id_cpu_dbs_info, cpu);

				for_each_cpu(j, &cpus_timer_done) {
					if (!dbs_info->cur_policy) {
						pr_err("Dbs policy is NULL\n");
						goto skip_this_cpu;
					}
					if (cpumask_test_cpu(j, dbs_info->
							cur_policy->cpus))
						goto skip_this_cpu;
				}

				cpumask_set_cpu(cpu, &cpus_timer_done);
				if (dbs_info->cur_policy) {
					dbs_timer_exit(dbs_info);
					/* restart dbs timer */
					mutex_lock(&dbs_info->timer_mutex);
					dbs_timer_init(dbs_info);
					mutex_unlock(&dbs_info->timer_mutex);
				}
skip_this_cpu:
				unlock_policy_rwsem_write(cpu);
			}
		}
		intellidemand_powersave_bias_init();
	} else {
		/* running at maximum or minimum frequencies; cancel
		   dbs timer as periodic load sampling is not necessary */
		for_each_online_cpu(cpu) {
			if (lock_policy_rwsem_write(cpu) < 0)
				continue;

			dbs_info = &per_cpu(id_cpu_dbs_info, cpu);

			for_each_cpu(j, &cpus_timer_done) {
				if (!dbs_info->cur_policy) {
					pr_err("Dbs policy is NULL\n");
					goto skip_this_cpu_bypass;
				}
				if (cpumask_test_cpu(j, dbs_info->
							cur_policy->cpus))
					goto skip_this_cpu_bypass;
			}

			cpumask_set_cpu(cpu, &cpus_timer_done);

			if (dbs_info->cur_policy) {
				/* cpu using intellidemand, cancel dbs timer */
				dbs_timer_exit(dbs_info);

				mutex_lock(&dbs_info->timer_mutex);
				intellidemand_powersave_bias_setspeed(
					dbs_info->cur_policy,
					NULL,
					input);
				mutex_unlock(&dbs_info->timer_mutex);
			}
skip_this_cpu_bypass:
			unlock_policy_rwsem_write(cpu);
		}
	}

	mutex_unlock(&dbs_mutex);
	put_online_cpus();

	return count;
}

/* PATCH : SMART_UP */
#if defined(SMART_UP_SLOW_UP_AT_HIGH_FREQ)
static void reset_hist(history_load *hist_load)
{
	int i;

	for (i = 0; i < SUP_SLOW_UP_DUR ; i++)
		hist_load->hist_max_load[i] = 0;

	hist_load->hist_load_cnt = 0;
}


static void reset_hist_high(history_load_high *hist_load)
{	int i;

	for (i = 0; i < SUP_HIGH_SLOW_UP_DUR ; i++)
		hist_load->hist_max_load[i] = 0;

	hist_load->hist_load_cnt = 0;
}

#endif

/* 20130711 smart_up */
static ssize_t store_smart_up(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int i, input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input > 1) {
		input = 1;
	} else if (input < 0) {
		input = 0;
	}

	/* buffer reset */
	for_each_online_cpu(i) {
		reset_hist(&hist_load[i]);
		reset_hist_high(&hist_load_high[i]);
	}
	dbs_tuners_ins.smart_up = input;

	return count;
}

static ssize_t store_smart_slow_up_load(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int i, input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input > 100) {
		input = 100;
	} else if (input < 0) {
		input = 0;
	}

        /* buffer reset */
	for_each_online_cpu(i) {
		reset_hist(&hist_load[i]);
		reset_hist_high(&hist_load_high[i]);
	}
	dbs_tuners_ins.smart_slow_up_load = input;

	return count;
}

static ssize_t store_smart_slow_up_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int i, input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input < 0)
		input = 0;

	/* buffer reset */
	for_each_online_cpu(i) {
		reset_hist(&hist_load[i]);
		reset_hist_high(&hist_load_high[i]);
	}
	dbs_tuners_ins.smart_slow_up_freq = input;

	return count;
}

static ssize_t store_smart_slow_up_dur(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int i, input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input > SUP_SLOW_UP_DUR) {
		input = SUP_SLOW_UP_DUR;
	} else if (input < 1) {
		input = 1;
	}

	/* buffer reset */
	for_each_online_cpu(i) {
		reset_hist(&hist_load[i]);
		reset_hist_high(&hist_load_high[i]);
	}
	dbs_tuners_ins.smart_slow_up_dur = input;

	return count;
}
static ssize_t store_smart_high_slow_up_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	unsigned int i;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input < 0)
		input = 0;
	/* buffer reset */
	for_each_online_cpu(i) {
		reset_hist(&hist_load[i]);
		reset_hist_high(&hist_load_high[i]);
	}
	dbs_tuners_ins.smart_high_slow_up_freq = input;

	return count;
}
static ssize_t store_smart_high_slow_up_dur(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	unsigned int i;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input > SUP_HIGH_SLOW_UP_DUR ) {
		input = SUP_HIGH_SLOW_UP_DUR;
	}else if (input < 1 ) {
		input = 1;
	}
	/* buffer reset */
	for_each_online_cpu(i) {
		reset_hist(&hist_load[i]);
		reset_hist_high(&hist_load_high[i]);
	}
	dbs_tuners_ins.smart_high_slow_up_dur = input;

	return count;
}
static ssize_t store_smart_each_off(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int i, input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input > SUP_CORE_NUM) {
		input = SUP_CORE_NUM;
	} else if (input < 0) {
		input = 0;
	}

	/* buffer reset */
	for_each_online_cpu(i) {
		reset_hist(&hist_load[i]);
		reset_hist_high(&hist_load_high[i]);
	}
	dbs_tuners_ins.smart_each_off = input;

	return count;
}
/* end smart_up */

static ssize_t store_freq_step(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 ||
			input < 0) {
		return -EINVAL;
	}
	dbs_tuners_ins.freq_step = input;

	return count;
}

static ssize_t store_step_up_early_hispeed(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 2265600 ||
			input < 0) {
		return -EINVAL;
	}
	dbs_tuners_ins.step_up_early_hispeed = input;

	return count;
}

static ssize_t store_step_up_interim_hispeed(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > DEF_STEP_UP_INTERIM_HISPEED ||
			input < 0) {
		return -EINVAL;
	}
	dbs_tuners_ins.step_up_interim_hispeed = input;

	return count;
}

static ssize_t store_sampling_early_factor(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input < 1)
		return -EINVAL;
	dbs_tuners_ins.sampling_early_factor = input;

	return count;
}

static ssize_t store_sampling_interim_factor(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input < 1)
		return -EINVAL;
	dbs_tuners_ins.sampling_interim_factor = input;

	return count;
}

define_one_global_rw(sampling_rate);
define_one_global_rw(io_is_busy);
define_one_global_rw(up_threshold);
define_one_global_rw(down_differential);
define_one_global_rw(sampling_down_factor);
define_one_global_rw(ignore_nice_load);
define_one_global_rw(powersave_bias);
define_one_global_rw(up_threshold_multi_core);
define_one_global_rw(optimal_freq);
define_one_global_rw(up_threshold_any_cpu_load);
define_one_global_rw(sync_freq);
/* 20130711 smart_up */
define_one_global_rw(smart_up);
define_one_global_rw(smart_slow_up_load);
define_one_global_rw(smart_slow_up_freq);
define_one_global_rw(smart_slow_up_dur);
define_one_global_rw(smart_high_slow_up_freq);
define_one_global_rw(smart_high_slow_up_dur);
define_one_global_rw(smart_each_off);
/* end smart_up */
define_one_global_rw(freq_step);
define_one_global_rw(step_up_early_hispeed);
define_one_global_rw(step_up_interim_hispeed);
define_one_global_rw(sampling_early_factor);
define_one_global_rw(sampling_interim_factor);
define_one_global_rw(two_phase_freq);

static struct attribute *dbs_attributes[] = {
	&sampling_rate_min.attr,
	&sampling_rate.attr,
	&up_threshold.attr,
	&down_differential.attr,
	&sampling_down_factor.attr,
	&ignore_nice_load.attr,
	&powersave_bias.attr,
	&io_is_busy.attr,
	&up_threshold_multi_core.attr,
	&optimal_freq.attr,
	&up_threshold_any_cpu_load.attr,
	&sync_freq.attr,
	/* 20130711 smart_up */
	&smart_up.attr,
	&smart_slow_up_load.attr,
	&smart_slow_up_freq.attr,
	&smart_slow_up_dur.attr,
	&smart_high_slow_up_freq.attr,
	&smart_high_slow_up_dur.attr,
	&smart_each_off.attr,
	/* end smart_up */
	&freq_step.attr,
	&step_up_early_hispeed.attr,
	&step_up_interim_hispeed.attr,
	&sampling_early_factor.attr,
	&sampling_interim_factor.attr,
	&two_phase_freq.attr,
	NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "intellidemand",
};

/************************** sysfs end ************************/

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	if (dbs_tuners_ins.powersave_bias)
		freq = powersave_bias_target(p, freq, CPUFREQ_RELATION_H);
	else if (p->cur == p->max)
		return;

	__cpufreq_driver_target(p, freq, dbs_tuners_ins.powersave_bias ?
			CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}

static void dbs_check_cpu(struct cpu_dbs_info_s *this_dbs_info)
{

#if defined(SMART_UP_PLUS)
	unsigned int core_j = 0;
#endif

	/* Extrapolated load of this CPU */
	unsigned int load_at_max_freq = 0;
	unsigned int max_load_freq;
	/* Current load across this CPU */
	unsigned int cur_load = 0;
	unsigned int max_load = 0;
	unsigned int max_load_other_cpu = 0;
	struct cpufreq_policy *policy;
	unsigned int j;
	static unsigned int phase = 0;
	static unsigned int counter = 0;
	unsigned int nr_cpus;

	this_dbs_info->freq_lo = 0;
	policy = this_dbs_info->cur_policy;
	if (policy == NULL)
		return;

	/*
	 * Every sampling_rate, we check, if current idle time is less
	 * than 20% (default), then we try to increase frequency
	 * Every sampling_rate, we look for a the lowest
	 * frequency which can sustain the load while keeping idle time over
	 * 30%. If such a frequency exist, we try to decrease to this frequency.
	 *
	 * Any frequency increase takes it to the maximum frequency.
	 * Frequency reduction happens at minimum steps of
	 * 5% (default) of current frequency
	 */

	/* Get Absolute Load - in terms of freq */
	max_load_freq = 0;

	for_each_cpu(j, policy->cpus) {
		struct cpu_dbs_info_s *j_dbs_info;
		u64 cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load_freq;
		int freq_avg;

		j_dbs_info = &per_cpu(id_cpu_dbs_info, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time,
				dbs_tuners_ins.io_is_busy);
		cur_iowait_time = get_cpu_iowait_time(j, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - j_dbs_info->prev_cpu_wall);
		j_dbs_info->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - j_dbs_info->prev_cpu_idle);
		j_dbs_info->prev_cpu_idle = cur_idle_time;

		iowait_time = (unsigned int)
			(cur_iowait_time - j_dbs_info->prev_cpu_iowait);
		j_dbs_info->prev_cpu_iowait = cur_iowait_time;

		if (dbs_tuners_ins.ignore_nice) {
			u64 cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE] -
					 j_dbs_info->prev_cpu_nice;
			/*
			 * Assumption: nice time between sampling periods will
			 * be less than 2^32 jiffies for 32 bit sys
			 */
			cur_nice_jiffies = (unsigned long)
					cputime64_to_jiffies64(cur_nice);

			j_dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
		}

		/*
		 * For the purpose of intellidemand, waiting for disk IO is an
		 * indication that you're performance critical, and not that
		 * the system is actually idle. So subtract the iowait time
		 * from the cpu idle time.
		 */

		if (dbs_tuners_ins.io_is_busy && idle_time >= iowait_time)
			idle_time -= iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		/*
		 * If the CPU had gone completely idle, and a task just woke up
		 * on this CPU now, it would be unfair to calculate 'load' the
		 * usual way for this elapsed time-window, because it will show
		 * near-zero load, irrespective of how CPU intensive the new
		 * task is. This is undesirable for latency-sensitive bursty
		 * workloads.
		 *
		 * To avoid this, we reuse the 'load' from the previous
		 * time-window and give this task a chance to start with a
		 * reasonably high CPU frequency.
		 *
		 * Detecting this situation is easy: the governor's deferrable
		 * timer would not have fired during CPU-idle periods. Hence
		 * an unusually large 'wall_time' indicates this scenario.
		 */
		if (unlikely(wall_time > (2 * dbs_tuners_ins.sampling_rate))) {
			cur_load = j_dbs_info->prev_load;
		} else {
			cur_load = 100 * (wall_time - idle_time) / wall_time;
		}

		if (cur_load > max_load)
			max_load = cur_load;

		j_dbs_info->max_load  = max(cur_load, j_dbs_info->prev_load);
		j_dbs_info->prev_load = cur_load;
		freq_avg = __cpufreq_driver_getavg(policy, j);
		if (policy == NULL)
			return;
		if (freq_avg <= 0)
			freq_avg = policy->cur;

		load_freq = cur_load * freq_avg;
		if (load_freq > max_load_freq)
			max_load_freq = load_freq;

#if defined(SMART_UP_PLUS)
		max_load = cur_load;
		core_j = j;
#endif

	}

	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *j_dbs_info;
		j_dbs_info = &per_cpu(id_cpu_dbs_info, j);

		if (j == policy->cpu)
			continue;

		if (max_load_other_cpu < j_dbs_info->max_load)
			max_load_other_cpu = j_dbs_info->max_load;
	}

	/* calculate the scaled load across CPU */
	load_at_max_freq = (cur_load * policy->cur)/policy->max;

	cpufreq_notify_utilization(policy, cur_load);

/* PATCH : SMART_UP */
	if (dbs_tuners_ins.smart_up && (core_j + 1) >
				dbs_tuners_ins.smart_each_off) {
		if (max_load_freq > SUP_THRESHOLD_STEPS[0] * policy->cur) {
			int smart_up_inc =
				(policy->max - policy->cur) / SUP_FREQ_STEPS[0];
			int freq_next = 0;
			int i = 0;

			/* 20130429 UPDATE */
			int check_idx =  0;
			int check_freq = 0;
			int temp_up_inc =0;

			if (counter < 5) {
				counter++;
				if (counter > 2) {
					phase = 1;
				}
			}

			nr_cpus = num_online_cpus();
			dbs_tuners_ins.two_phase_freq = two_phase_freq_array[nr_cpus-1];
			if (dbs_tuners_ins.two_phase_freq < policy->cur)
				phase = 1;
			if (dbs_tuners_ins.two_phase_freq != 0 && phase == 0) {
				dbs_freq_increase(policy, dbs_tuners_ins.two_phase_freq);
			} else {
				if (policy->cur < policy->max)
					this_dbs_info->rate_mult =
						dbs_tuners_ins.sampling_down_factor;
				dbs_freq_increase(policy, policy->max);
			}

			for (i = (SUP_MAX_STEP - 1); i > 0; i--) {
				if (max_load_freq > SUP_THRESHOLD_STEPS[i]
							* policy->cur) {
					smart_up_inc = (policy->max - policy->cur)
							/ SUP_FREQ_STEPS[i];
					break;
				}
			}

			/* 20130429 UPDATE */
			check_idx =  pre_freq_idx[core_j].freq_idx;
			check_freq = pre_freq_idx[core_j].freq_value;
			if (( check_idx == 0)
					|| (this_dbs_info->freq_table[check_idx].frequency
					!=  policy->cur)) {
				int i = 0;
				for (i =0; i < SUP_FREQ_LEVEL; i ++) {
					if (this_dbs_info->freq_table[i].frequency == policy->cur) {
						pre_freq_idx[core_j].freq_idx = i;
						pre_freq_idx[core_j].freq_value = policy->cur;
						check_idx =  i;
						check_freq = policy->cur;
						break;
					}
				}
			}

			if (check_idx < SUP_FREQ_LEVEL-1) {
				temp_up_inc =
					this_dbs_info->freq_table[check_idx + 1].frequency
					- check_freq;
			}

			if (smart_up_inc < temp_up_inc )
				smart_up_inc = temp_up_inc;

			freq_next = MIN((policy->cur + smart_up_inc), policy->max);

			if (policy->cur >= dbs_tuners_ins.smart_high_slow_up_freq) {
				int idx = hist_load_high[core_j].hist_load_cnt;
				int avg_hist_load = 0;

				if (idx >= dbs_tuners_ins.smart_high_slow_up_dur)
					idx = 0;

				hist_load_high[core_j].hist_max_load[idx] = max_load;
				hist_load_high[core_j].hist_load_cnt = idx + 1;

				/* note : check history_load and get_sum_hist_load */
				if (hist_load_high[core_j].
						hist_max_load[dbs_tuners_ins.smart_high_slow_up_dur - 1] > 0) {
					int sum_hist_load_freq = 0;
					int i = 0;
					for (i = 0; i < dbs_tuners_ins.smart_high_slow_up_dur; i++)
						sum_hist_load_freq +=
							hist_load_high[core_j].hist_max_load[i];

					avg_hist_load = sum_hist_load_freq
								/ dbs_tuners_ins.smart_high_slow_up_dur;

					if (avg_hist_load > dbs_tuners_ins.smart_slow_up_load) {
						reset_hist_high(&hist_load_high[core_j]);
						freq_next = MIN((policy->cur + temp_up_inc), policy->max);
					} else
						freq_next = policy->cur;
				} else {
					freq_next = policy->cur;
				}

			} else if (policy->cur >= dbs_tuners_ins.smart_slow_up_freq ) {
				int idx = hist_load[core_j].hist_load_cnt;
				int avg_hist_load = 0;

				if (idx >= dbs_tuners_ins.smart_slow_up_dur)
					idx = 0;

				hist_load[core_j].hist_max_load[idx] = max_load;
				hist_load[core_j].hist_load_cnt = idx + 1;

				/* note : check history_load and get_sum_hist_load */
				if (hist_load[core_j].
					hist_max_load[dbs_tuners_ins.smart_slow_up_dur - 1] > 0) {
					int sum_hist_load_freq = 0;
					int i = 0;
					for (i = 0; i < dbs_tuners_ins.smart_slow_up_dur; i++)
						sum_hist_load_freq +=
							hist_load[core_j].hist_max_load[i];

					avg_hist_load = sum_hist_load_freq
							/ dbs_tuners_ins.smart_slow_up_dur;

					if (avg_hist_load > dbs_tuners_ins.smart_slow_up_load) {
						reset_hist(&hist_load[core_j]);
						freq_next = MIN((policy->cur + temp_up_inc), policy->max);
					} else
						freq_next = policy->cur;
				} else {
					freq_next = policy->cur;
				}
			} else {
				reset_hist(&hist_load[core_j]);
			}
			if (freq_next == policy->max)
				this_dbs_info->rate_mult =
					dbs_tuners_ins.sampling_down_factor;

			dbs_freq_increase(policy, freq_next);
			return;
		}
	} else {
		/* Check for frequency increase */
		if (max_load_freq > dbs_tuners_ins.up_threshold * policy->cur) {
			int target;
			int inc;

			if (policy->cur < dbs_tuners_ins.step_up_early_hispeed) {
				target = dbs_tuners_ins.step_up_early_hispeed;
			} else if (policy->cur < dbs_tuners_ins.step_up_interim_hispeed) {
				if (policy->cur == dbs_tuners_ins.step_up_early_hispeed) {
					if (this_dbs_info->freq_stay_count <
						dbs_tuners_ins.sampling_early_factor) {
						this_dbs_info->freq_stay_count++;
						return;
					}
				}
				this_dbs_info->freq_stay_count = 1;
				inc = (policy->max * dbs_tuners_ins.freq_step) / 100;
				target = min(dbs_tuners_ins.step_up_interim_hispeed,
					policy->cur + inc);
			} else {
				if (policy->cur == dbs_tuners_ins.step_up_interim_hispeed) {
					if (this_dbs_info->freq_stay_count <
						dbs_tuners_ins.sampling_interim_factor) {
						this_dbs_info->freq_stay_count++;
						return;
					}
				}
				this_dbs_info->freq_stay_count = 1;
				target = policy->max;
			}

			pr_debug("%s: cpu=%d, cur=%d, target=%d\n",
				__func__, policy->cpu, policy->cur, target);

			/* If switching to max speed, apply sampling_down_factor */
			if (target == policy->max)
				this_dbs_info->rate_mult =
					dbs_tuners_ins.sampling_down_factor;

			dbs_freq_increase(policy, target);
			return;
		}
	}
	if (counter > 0) {
		counter--;
		if (counter == 0) {
			phase = 0;
		}
	}

	if (num_online_cpus() > 1) {
		if (max_load_other_cpu >
				dbs_tuners_ins.up_threshold_any_cpu_load) {
			if (policy->cur < dbs_tuners_ins.sync_freq)
				dbs_freq_increase(policy,
						dbs_tuners_ins.sync_freq);
			return;
		}

		if (max_load_freq > dbs_tuners_ins.up_threshold_multi_core *
								policy->cur) {
			if (policy->cur < dbs_tuners_ins.optimal_freq)
				dbs_freq_increase(policy,
						dbs_tuners_ins.optimal_freq);
			return;
		}
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		return;

	/*
	 * The optimal frequency is the frequency that is the lowest that
	 * can support the current CPU usage without triggering the up
	 * policy. To be safe, we focus 10 points under the threshold.
	 */
	if (max_load_freq <
	    (dbs_tuners_ins.up_threshold - dbs_tuners_ins.down_differential) *
			policy->cur) {
		unsigned int freq_next;
		freq_next = max_load_freq /
				(dbs_tuners_ins.up_threshold -
					dbs_tuners_ins.down_differential);

		/* PATCH : SMART_UP */
		if (dbs_tuners_ins.smart_up && (core_j + 1) >
					dbs_tuners_ins.smart_each_off) {
			if (freq_next >= dbs_tuners_ins.smart_high_slow_up_freq) {
				int idx = hist_load_high[core_j].hist_load_cnt;

				if (idx >= dbs_tuners_ins.smart_high_slow_up_dur)
					idx = 0;

				hist_load_high[core_j].hist_max_load[idx] = max_load;
				hist_load_high[core_j].hist_load_cnt = idx + 1;
			} else if (freq_next >= dbs_tuners_ins.smart_slow_up_freq) {
				int idx = hist_load[core_j].hist_load_cnt;

				if (idx >= dbs_tuners_ins.smart_slow_up_dur)
					idx = 0;

				hist_load[core_j].hist_max_load[idx] = max_load;
				hist_load[core_j].hist_load_cnt = idx + 1;

				reset_hist_high(&hist_load_high[core_j]);
			} else if (policy->cur >= dbs_tuners_ins.smart_slow_up_freq) {
				reset_hist(&hist_load[core_j]);
				reset_hist_high(&hist_load_high[core_j]);
			}
		}

		/* No longer fully busy, reset rate_mult */
		this_dbs_info->rate_mult = 1;
		this_dbs_info->freq_stay_count = 1;

		if (num_online_cpus() > 1) {
			if (max_load_other_cpu >
			(dbs_tuners_ins.up_threshold_multi_core -
			dbs_tuners_ins.down_differential) &&
			freq_next < dbs_tuners_ins.sync_freq)
				freq_next = dbs_tuners_ins.sync_freq;

			if (max_load_freq >
					((dbs_tuners_ins.up_threshold_multi_core -
					dbs_tuners_ins.down_differential_multi_core) *
					policy->cur) &&
					freq_next < dbs_tuners_ins.optimal_freq)
				freq_next = dbs_tuners_ins.optimal_freq;

		}
		if (!dbs_tuners_ins.powersave_bias) {
			__cpufreq_driver_target(policy, freq_next,
					CPUFREQ_RELATION_L);
		} else {
			int freq = powersave_bias_target(policy, freq_next,
					CPUFREQ_RELATION_L);
			__cpufreq_driver_target(policy, freq,
				CPUFREQ_RELATION_L);
		}
	}
}

static void do_dbs_timer(struct work_struct *work)
{
	struct cpu_dbs_info_s *dbs_info =
		container_of(work, struct cpu_dbs_info_s, work.work);
	unsigned int cpu = dbs_info->cpu;
	int sample_type = dbs_info->sample_type;
	int delay;

	mutex_lock(&dbs_info->timer_mutex);

	/* Common NORMAL_SAMPLE setup */
	dbs_info->sample_type = DBS_NORMAL_SAMPLE;
	if (!dbs_tuners_ins.powersave_bias ||
	    sample_type == DBS_NORMAL_SAMPLE) {
		dbs_check_cpu(dbs_info);
		if (dbs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			dbs_info->sample_type = DBS_SUB_SAMPLE;
			delay = dbs_info->freq_hi_jiffies;
		} else {
			delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate
				* dbs_info->rate_mult);
		}
	} else {
		__cpufreq_driver_target(dbs_info->cur_policy,
			dbs_info->freq_lo, CPUFREQ_RELATION_H);
		delay = dbs_info->freq_lo_jiffies;
	}
	queue_delayed_work_on(cpu, dbs_wq, &dbs_info->work, delay);
	mutex_unlock(&dbs_info->timer_mutex);
}

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);

	if (num_online_cpus() > 1)
		delay -= jiffies % delay;

	dbs_info->sample_type = DBS_NORMAL_SAMPLE;
	INIT_DEFERRABLE_WORK(&dbs_info->work, do_dbs_timer);
	queue_delayed_work_on(dbs_info->cpu, dbs_wq, &dbs_info->work, delay);
}

static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info)
{
	cancel_delayed_work_sync(&dbs_info->work);
}

static void dbs_refresh_callback(struct work_struct *work)
{
	struct cpufreq_policy *policy;
	struct cpu_dbs_info_s *this_dbs_info;
	struct dbs_work_struct *dbs_work;
	unsigned int cpu;

	dbs_work = container_of(work, struct dbs_work_struct, work);
	cpu = dbs_work->cpu;

	get_online_cpus();

	if (lock_policy_rwsem_write(cpu) < 0)
		goto bail_acq_sema_failed;

	this_dbs_info = &per_cpu(id_cpu_dbs_info, cpu);
	policy = this_dbs_info->cur_policy;
	if (!policy) {
		/* CPU not using ondemand governor */
		goto bail_incorrect_governor;
	}

	if (policy->cur < policy->max) {
		/*
		 * Arch specific cpufreq driver may fail.
		 * Don't update governor frequency upon failure.
		 */
		if (__cpufreq_driver_target(policy, policy->max,
					CPUFREQ_RELATION_L) >= 0)
				policy->cur = policy->max;

		this_dbs_info->prev_cpu_idle = get_cpu_idle_time(cpu,
				&this_dbs_info->prev_cpu_wall, dbs_tuners_ins.io_is_busy);
	}

bail_incorrect_governor:
	unlock_policy_rwsem_write(cpu);

bail_acq_sema_failed:
	put_online_cpus();
	return;
}

static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int j;
	int rc;

	this_dbs_info = &per_cpu(id_cpu_dbs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&dbs_mutex);

		dbs_enable++;
		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			u64 tmp;
			j_dbs_info = &per_cpu(id_cpu_dbs_info, j);
			j_dbs_info->cur_policy = policy;

			j_dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&j_dbs_info->prev_cpu_wall,
						dbs_tuners_ins.io_is_busy);
			tmp = j_dbs_info->prev_cpu_wall -
				j_dbs_info->prev_cpu_idle;
			do_div(tmp, j_dbs_info->prev_cpu_wall);
			j_dbs_info->prev_load = 100 * tmp;
			if (dbs_tuners_ins.ignore_nice)
				j_dbs_info->prev_cpu_nice =
						kcpustat_cpu(j).cpustat[CPUTIME_NICE];
		}
		this_dbs_info->cpu = cpu;
		this_dbs_info->rate_mult = 1;
		this_dbs_info->freq_stay_count = 1;
		intellidemand_powersave_bias_init_cpu(cpu);
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (dbs_enable == 1) {
			unsigned int latency;

			rc = sysfs_create_group(cpufreq_global_kobject,
						&dbs_attr_group);
			if (rc) {
				dbs_enable--;
				mutex_unlock(&dbs_mutex);
				return rc;
			}

			/* policy latency is in nS. Convert it to uS first */
			latency = policy->cpuinfo.transition_latency / 1000;
			if (latency == 0)
				latency = 1;
			/* Bring kernel and HW constraints together */
			min_sampling_rate = max(min_sampling_rate,
					MIN_LATENCY_MULTIPLIER * latency);
			if (latency != 1)
				dbs_tuners_ins.sampling_rate =
					max(dbs_tuners_ins.sampling_rate,
						latency * LATENCY_MULTIPLIER);
			dbs_tuners_ins.io_is_busy = 0;

			if (dbs_tuners_ins.optimal_freq == 0)
				dbs_tuners_ins.optimal_freq = policy->min;

			if (dbs_tuners_ins.sync_freq == 0)
				dbs_tuners_ins.sync_freq = policy->min;
		}
		mutex_unlock(&dbs_mutex);

		mutex_init(&this_dbs_info->timer_mutex);

		if (!intellidemand_powersave_bias_setspeed(
					this_dbs_info->cur_policy,
					NULL,
					dbs_tuners_ins.powersave_bias))
			dbs_timer_init(this_dbs_info);
		break;

	case CPUFREQ_GOV_STOP:
		dbs_timer_exit(this_dbs_info);

		mutex_lock(&dbs_mutex);
		mutex_destroy(&this_dbs_info->timer_mutex);

		dbs_enable--;

		/* If device is being removed, policy is no longer
		 * valid. */
		this_dbs_info->cur_policy = NULL;
		if (!dbs_enable)
			sysfs_remove_group(cpufreq_global_kobject,
					   &dbs_attr_group);

		mutex_unlock(&dbs_mutex);

		break;

	case CPUFREQ_GOV_LIMITS:
		/* If device is being removed, skip set limits */
		if (!this_dbs_info->cur_policy)
			break;
		mutex_lock(&this_dbs_info->timer_mutex);
		if (policy->max < this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(this_dbs_info->cur_policy,
				policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(this_dbs_info->cur_policy,
				policy->min, CPUFREQ_RELATION_L);
		else if (dbs_tuners_ins.powersave_bias != 0)
			intellidemand_powersave_bias_setspeed(
				this_dbs_info->cur_policy,
				policy,
				dbs_tuners_ins.powersave_bias);
		dbs_check_cpu(this_dbs_info);
		mutex_unlock(&this_dbs_info->timer_mutex);
		break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_INTELLIDEMAND
static
#endif
struct cpufreq_governor cpufreq_gov_intellidemand = {
	.name			= "intellidemand",
	.governor		= cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	u64 idle_time;
	unsigned int i;
	int cpu = get_cpu();

	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		dbs_tuners_ins.up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		dbs_tuners_ins.down_differential =
					MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		/* For correct statistics, we need 10 ticks for each measure */
		min_sampling_rate =
			MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(10);
	}

	dbs_wq = alloc_workqueue("intellidemand_dbs_wq", WQ_HIGHPRI, 0);
	if (!dbs_wq) {
		printk(KERN_ERR "Failed to create intellidemand_dbs_wq workqueue\n");
		return -EFAULT;
	}
	for_each_possible_cpu(i) {
		struct cpu_dbs_info_s *this_dbs_info =
			&per_cpu(id_cpu_dbs_info, i);
		struct dbs_work_struct *dbs_work =
			&per_cpu(dbs_refresh_work, i);

		mutex_init(&this_dbs_info->timer_mutex);
		INIT_WORK(&dbs_work->work, dbs_refresh_callback);
		dbs_work->cpu = i;
	}

	return cpufreq_register_governor(&cpufreq_gov_intellidemand);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	unsigned int i;

	cpufreq_unregister_governor(&cpufreq_gov_intellidemand);
	for_each_possible_cpu(i) {
		struct cpu_dbs_info_s *this_dbs_info =
			&per_cpu(id_cpu_dbs_info, i);
		mutex_destroy(&this_dbs_info->timer_mutex);
	}
	destroy_workqueue(dbs_wq);
}

MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_DESCRIPTION("'cpufreq_intellidemand' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_INTELLIDEMAND
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
