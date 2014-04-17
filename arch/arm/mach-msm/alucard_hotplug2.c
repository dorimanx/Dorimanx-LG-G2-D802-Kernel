/*
 * Author: Alucard_24@XDA
 *
 * Copyright 2012 Alucard_24@XDA
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
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/msm_tsens.h>
#include "acpuclock.h"

#if defined(CONFIG_POWERSUSPEND) || defined(CONFIG_HAS_EARLYSUSPEND)
#if CONFIG_POWERSUSPEND
#include <linux/powersuspend.h>
#else
#include <linux/earlysuspend.h>
#endif
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

static struct workqueue_struct *alucardhp_wq;
#if 0
static ktime_t time_stamp;
#endif
static struct hotplug_cpuinfo {
	u64 prev_cpu_wall;
	u64 prev_cpu_idle;
	int cpu;
	int hotplugging_rate;
	struct delayed_work work;
};
static DEFINE_MUTEX(alucardhp_mutex);

static DEFINE_PER_CPU(struct hotplug_cpuinfo, od_hotplug_cpuinfo);

static atomic_t suspended = ATOMIC_INIT(0);

static struct hotplug_tuners {
	int hotplug_sampling_rate;
	atomic_t hotplug_enable;
	int cpu_up_rate;
	int cpu_down_rate;
	atomic_t maxcoreslimit;
	atomic_t maxcoreslimit_sleep;
#if 0
	atomic_t core_thermal_enable;
#endif
} hotplug_tuners_ins = {
	.hotplug_sampling_rate = 60,
	.hotplug_enable = ATOMIC_INIT(0),
	.cpu_up_rate = 1,
	.cpu_down_rate = 10,
	.maxcoreslimit = ATOMIC_INIT(NR_CPUS),
	.maxcoreslimit_sleep = ATOMIC_INIT(1),
#if 0
	.core_thermal_enable = ATOMIC_INIT(0),
#endif
};

#if 0
static atomic_t core_thermal_lock = ATOMIC_INIT(0);

static struct core_thermal_data {
	uint32_t sensor_id;
	uint32_t core_limit_temp_degC;
	uint32_t core_temp_hysteresis_degC;
	int core_step;
	int num_cores;
};

static struct core_thermal_data core_thermal_info = {
	.sensor_id = 0,
	.core_limit_temp_degC = 80,
	.core_temp_hysteresis_degC = 10,
	.core_step = 1,
	.num_cores = NR_CPUS,
};
#endif

#define MAX_HOTPLUG_RATE	(40)
#define DOWN_INDEX		(0)
#define UP_INDEX		(1)

static struct runqueue_data {
	unsigned int nr_run_avg;
	int64_t last_time;
	int64_t total_time;
	spinlock_t lock;
};

static struct runqueue_data *rq_data;

static void init_rq_avg_stats(void)
{
	rq_data->nr_run_avg = 0;
	rq_data->last_time = 0;
	rq_data->total_time = 0;

	return;
}

static int __init init_rq_avg(void)
{
	rq_data = kzalloc(sizeof(struct runqueue_data), GFP_KERNEL);
	if (rq_data == NULL) {
		pr_err("%s cannot allocate memory\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int __exit exit_rq_avg(void)
{
	kfree(rq_data);
	return 0;
}

static unsigned int get_nr_run_avg(void)
{
	int64_t time_diff = 0;
	int64_t nr_run = 0;
	unsigned long flags = 0;
	int64_t cur_time;
	unsigned int nr_run_avg;

	cur_time = ktime_to_ns(ktime_get());

	spin_lock_irqsave(&rq_data->lock, flags);

	if (rq_data->last_time == 0)
		rq_data->last_time = cur_time;
	if (rq_data->nr_run_avg == 0)
		rq_data->total_time = 0;

	nr_run = nr_running() * 100;
	time_diff = cur_time - rq_data->last_time;
	do_div(time_diff, 1000 * 1000);

	if (time_diff != 0 && rq_data->total_time != 0) {
		nr_run = (nr_run * time_diff) +
			(rq_data->nr_run_avg * rq_data->total_time);
		do_div(nr_run, rq_data->total_time + time_diff);
	}
	rq_data->nr_run_avg = nr_run;
	rq_data->total_time += time_diff;
	rq_data->last_time = cur_time;

	nr_run_avg = rq_data->nr_run_avg;
	rq_data->nr_run_avg = 0;

	spin_unlock_irqrestore(&rq_data->lock, flags);

	return nr_run_avg;
}

static int hotplug_freq[4][2] = {
	{0, 702000},
	{486000, 702000},
	{486000, 702000},
	{486000, 0}
};
static int hotplug_load[4][2] = {
	{0, 65},
	{30, 65},
	{30, 65},
	{30, 0}
};
static int hotplug_rq[4][2] = {
	{0, 300},
	{300, 300},
	{300, 300},
	{300, 0}
};

#if 0
static inline int do_core_control(int online, int num_cores_limit)
{
	struct tsens_device tsens_dev;
	long temp = 0;
	int ret = 0;

	tsens_dev.sensor_num = core_thermal_info.sensor_id;
	ret = tsens_get_temp(&tsens_dev, &temp);
	if (ret) {
		pr_debug("%s: Unable to read TSENS sensor %d\n",
				KBUILD_MODNAME, tsens_dev.sensor_num);
	}

	if (temp >= core_thermal_info.core_limit_temp_degC) {
		core_thermal_info.num_cores =
				min(max(1, (online -
				core_thermal_info.core_step)),
				NR_CPUS);
		atomic_set(&core_thermal_lock, 1);
	} else if (temp <= (core_thermal_info.core_limit_temp_degC -
				core_thermal_info.core_temp_hysteresis_degC)) {
		core_thermal_info.num_cores = num_cores_limit;
		atomic_set(&core_thermal_lock, 0);
	} else {
		core_thermal_info.num_cores = online;
	}
#if 0
	pr_info("Core Sensor Temp.[%u], Max Cores[%d]\n",
			temp, core_thermal_info.num_cores);
#endif

	return core_thermal_info.num_cores;
}
#endif

static bool hotplug_work_fn(struct hotplug_cpuinfo *this_hotplug_cpuinfo)
{
	struct hotplug_cpuinfo *ref_hotplug_cpuinfo;
	u64 cur_wall_time, cur_idle_time;
	unsigned int wall_time, idle_time;
	int upmaxcoreslimit = 0;
	int up_rate = hotplug_tuners_ins.cpu_up_rate;
	int down_rate = hotplug_tuners_ins.cpu_down_rate;
	int up_load, up_freq, up_rq;
	int down_load, down_freq, down_rq;
	bool check_up = false, check_down = false;
	unsigned int cpu = 0;
	int ref_cpu = -1;
	int cur_load = -1;
	int cur_freq = 0;
	int online_cpus = 0;
	bool online = true;
	unsigned int rq_avg = 0;
	unsigned int sampling_rate = hotplug_tuners_ins.hotplug_sampling_rate;
	int delay, ret = 0;
	int suspend = 0;
#if 0
	int core_thermal_enable = 0;
#endif

	cpu = this_hotplug_cpuinfo->cpu;
	ref_cpu = cpu + 1;
	
	if (!cpu_online(cpu))
		return false;
		
	/* set hotplugging_rate used */
	++this_hotplug_cpuinfo->hotplugging_rate;
	check_up = (this_hotplug_cpuinfo->hotplugging_rate % up_rate == 0);
	check_down = (this_hotplug_cpuinfo->hotplugging_rate % down_rate == 0);

	rq_avg = get_nr_run_avg();

	suspend = atomic_read(&suspended);

	if (suspend)
		upmaxcoreslimit = atomic_read(&hotplug_tuners_ins.maxcoreslimit_sleep);
	else
		upmaxcoreslimit = atomic_read(&hotplug_tuners_ins.maxcoreslimit);

	online_cpus = num_online_cpus();
	
#if 0
	core_thermal_enable = atomic_read(
			&hotplug_tuners_ins.core_thermal_enable);

	if (core_thermal_enable > 0 || atomic_read(&core_thermal_lock) > 0)
		upmaxcoreslimit = do_core_control(online_cpus,
					upmaxcoreslimit);
#endif
	
	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);

	wall_time = (unsigned int)
			(cur_wall_time -
				this_hotplug_cpuinfo->prev_cpu_wall);
	this_hotplug_cpuinfo->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int)
			(cur_idle_time -
				this_hotplug_cpuinfo->prev_cpu_idle);
	this_hotplug_cpuinfo->prev_cpu_idle = cur_idle_time;

	/* if wall_time < idle_time, evaluate cpu load next time */
	if (wall_time >= idle_time) {
		/*
		 * if wall_time is equal to idle_time,
		 * cpu_load is equal to 0
		 */
		cur_load = wall_time > idle_time ? (100 *
			(wall_time - idle_time)) / wall_time : 0;
		
		cur_freq = acpuclk_get_rate(cpu);

		up_load = hotplug_load[cpu][UP_INDEX];
		down_load = hotplug_load[cpu][DOWN_INDEX];
		up_freq = hotplug_freq[cpu][UP_INDEX];
		down_freq = hotplug_freq[cpu][DOWN_INDEX];
		up_rq = hotplug_rq[cpu][UP_INDEX];
		down_rq = hotplug_rq[cpu][DOWN_INDEX];

#if 0
		if (cpu > 0 && online_cpus >
				upmaxcoreslimit) {
				online = false;
				return false;
		}
#endif

		if (check_up
			&& ref_cpu < upmaxcoreslimit
			&& cur_load >= up_load
			&& cur_freq >= up_freq
			&& rq_avg > up_rq) {
				if (!cpu_online(ref_cpu)) {
					ref_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, ref_cpu);
					delay = msecs_to_jiffies(sampling_rate);
					ret = cpu_up(ref_cpu);
					if (!ret) {
						queue_delayed_work_on(ref_cpu, alucardhp_wq, &ref_hotplug_cpuinfo->work, delay);
						pr_info("HOTPLUG IN CPU[%u], cur_load[%d], cur_freq[%d], rq_avg[%d]\n", ref_cpu, cur_load, cur_freq, rq_avg);
					}
				}
		}
		
		if (check_down
				&& cpu > 0
				&& cur_load >= 0) {
			if (cur_load < down_load
					|| (cur_freq <= down_freq
					&& rq_avg <= down_rq)) {
						online = false;
						pr_info("HOTPLUG OUT CPU[%u], cur_load[%d], cur_freq[%d], rq_avg[%d]\n", cpu, cur_load, cur_freq, rq_avg);
			}
		}
	}

	if (this_hotplug_cpuinfo->hotplugging_rate >= max(up_rate, down_rate)) {
		this_hotplug_cpuinfo->hotplugging_rate = 0;
	}
	
	return online;
}

static void __cpuinit hotplug_timer_fn(struct work_struct *work)
{
	struct hotplug_cpuinfo *cpu_hotplug_cpuinfo;
	int delay;
	unsigned int cpu;
	int sampling_rate;
	bool IsActive = false;
	int ret = 0;

	cpu_hotplug_cpuinfo = container_of(work, struct hotplug_cpuinfo, work.work);
	cpu = cpu_hotplug_cpuinfo->cpu;

	sampling_rate = hotplug_tuners_ins.hotplug_sampling_rate;
	delay = msecs_to_jiffies(sampling_rate);

	// CORES CONTROL
	IsActive = hotplug_work_fn(cpu_hotplug_cpuinfo);

	if (IsActive) {
		queue_delayed_work_on(cpu, alucardhp_wq, &cpu_hotplug_cpuinfo->work, delay);
	} else {
		if (cpu > 0) {
			ret = cpu_down(cpu);
			if (!ret) {
				cancel_delayed_work(&cpu_hotplug_cpuinfo->work);
				cpu_hotplug_cpuinfo->hotplugging_rate = 0;
				return;
			}
		}
	}
}

static void hotplug_start()
{
	unsigned int cpu;
	int delay = msecs_to_jiffies(hotplug_tuners_ins.hotplug_sampling_rate);

#if 0
	/* Initiate timer time stamp */
	time_stamp = ktime_get();
#endif

	init_rq_avg_stats();

	get_online_cpus();
	for_each_possible_cpu(cpu) {
		struct hotplug_cpuinfo *cpu_hotplug_cpuinfo;

		cpu_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);

		cpu_hotplug_cpuinfo->prev_cpu_idle = get_cpu_idle_time(cpu,
				&cpu_hotplug_cpuinfo->prev_cpu_wall, 0);

		cpu_hotplug_cpuinfo->cpu = cpu;
		cpu_hotplug_cpuinfo->hotplugging_rate = 0;

		atomic_set(&suspended,0);

		INIT_DEFERRABLE_WORK(&cpu_hotplug_cpuinfo->work, hotplug_timer_fn);

		if (cpu_online(cpu)) {
			queue_delayed_work_on(cpu, alucardhp_wq, &cpu_hotplug_cpuinfo->work,
				delay);
		}
	}
	put_online_cpus();
}

static void hotplug_stop(void)
{
	unsigned int cpu;

	get_online_cpus();
	for_each_possible_cpu(cpu) {
		struct hotplug_cpuinfo *cpu_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);

		cancel_delayed_work_sync(&cpu_hotplug_cpuinfo->work);
	}
	put_online_cpus();
}

#if defined(CONFIG_POWERSUSPEND) || defined(CONFIG_HAS_EARLYSUSPEND)
#ifdef CONFIG_POWERSUSPEND
static void __ref alucard_hotplug_suspend(struct power_suspend *handler)
#else
static void __ref alucard_hotplug_early_suspend(struct early_suspend *handler)
#endif
{
	int i = 0;
	int maxcoreslimit_sleep = 0;

	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) > 0) {
		maxcoreslimit_sleep =
			atomic_read(&hotplug_tuners_ins.maxcoreslimit_sleep);

		/* put rest of the cores to sleep! */
		for (i = num_possible_cpus() - 1; i >=
				maxcoreslimit_sleep; i--) {
			struct hotplug_cpuinfo *this_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, i);
				cancel_delayed_work_sync(&this_hotplug_cpuinfo->work);
				cpu_down(i);
				this_hotplug_cpuinfo->hotplugging_rate = 0;
		}
		atomic_set(&suspended,1);
	}
}

#ifdef CONFIG_POWERSUSPEND
static void __cpuinit alucard_hotplug_resume(struct power_suspend *handler)
#else
static void __cpuinit alucard_hotplug_late_resume(
				struct early_suspend *handler)
#endif
{
	int maxcoreslimit = 0;
	int i = 0;
	int delay = msecs_to_jiffies(hotplug_tuners_ins.hotplug_sampling_rate);

#if 0
	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) > 0 &&
			atomic_read(&core_thermal_lock) == 0) {
#endif
	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) > 0) {
		/* wake up everyone */
		maxcoreslimit = atomic_read(&hotplug_tuners_ins.maxcoreslimit);

		atomic_set(&suspended,0);
		for (i = 1; i < maxcoreslimit; i++) {
			struct hotplug_cpuinfo *this_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, i);
				cancel_delayed_work_sync(&this_hotplug_cpuinfo->work);
				cpu_up(i);
				queue_delayed_work_on(i, alucardhp_wq, &this_hotplug_cpuinfo->work,
														delay);
		}
	}
}

#ifdef CONFIG_POWERSUSPEND
static struct power_suspend alucard_hotplug_power_suspend_driver = {
	.suspend = alucard_hotplug_suspend,
	.resume = alucard_hotplug_resume,
};
#else
static struct early_suspend alucard_hotplug_early_suspend_driver = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 10,
	.suspend = alucard_hotplug_early_suspend,
	.resume = alucard_hotplug_late_resume,
};
#endif
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

#define show_atomic(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", \
			atomic_read(&hotplug_tuners_ins.object));	\
}

#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", \
			hotplug_tuners_ins.object);			\
}

show_one(hotplug_sampling_rate, hotplug_sampling_rate);
show_atomic(hotplug_enable, hotplug_enable);
show_one(cpu_up_rate, cpu_up_rate);
show_one(cpu_down_rate, cpu_down_rate);
show_atomic(maxcoreslimit, maxcoreslimit);
show_atomic(maxcoreslimit_sleep, maxcoreslimit_sleep);
#if 0
show_atomic(core_thermal_enable, core_thermal_enable);
#endif

#define show_hotplug_param(file_name, num_core, up_down)		\
static ssize_t show_##file_name##_##num_core##_##up_down		\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", \
			file_name[num_core - 1][up_down]);		\
}

#define store_hotplug_param(file_name, num_core, up_down)		\
static ssize_t store_##file_name##_##num_core##_##up_down		\
(struct kobject *kobj, struct attribute *attr,				\
	const char *buf, size_t count)					\
{									\
	unsigned int input;						\
	int ret;							\
	ret = sscanf(buf, "%d", &input);				\
	if (ret != 1)							\
		return -EINVAL;						\
	if (input == file_name[num_core - 1][up_down]) {		\
		return count;						\
	}								\
	file_name[num_core - 1][up_down] = input;			\
	return count;							\
}

/* hotplug freq */
show_hotplug_param(hotplug_freq, 1, 1);
show_hotplug_param(hotplug_freq, 2, 0);
#if NR_CPUS >= 4
show_hotplug_param(hotplug_freq, 2, 1);
show_hotplug_param(hotplug_freq, 3, 0);
show_hotplug_param(hotplug_freq, 3, 1);
show_hotplug_param(hotplug_freq, 4, 0);
#endif
/* hotplug load */
show_hotplug_param(hotplug_load, 1, 1);
show_hotplug_param(hotplug_load, 2, 0);
#if NR_CPUS >= 4
show_hotplug_param(hotplug_load, 2, 1);
show_hotplug_param(hotplug_load, 3, 0);
show_hotplug_param(hotplug_load, 3, 1);
show_hotplug_param(hotplug_load, 4, 0);
#endif
/* hotplug rq */
show_hotplug_param(hotplug_rq, 1, 1);
show_hotplug_param(hotplug_rq, 2, 0);
#if NR_CPUS >= 4
show_hotplug_param(hotplug_rq, 2, 1);
show_hotplug_param(hotplug_rq, 3, 0);
show_hotplug_param(hotplug_rq, 3, 1);
show_hotplug_param(hotplug_rq, 4, 0);
#endif

/* hotplug freq */
store_hotplug_param(hotplug_freq, 1, 1);
store_hotplug_param(hotplug_freq, 2, 0);
#if NR_CPUS >= 4
store_hotplug_param(hotplug_freq, 2, 1);
store_hotplug_param(hotplug_freq, 3, 0);
store_hotplug_param(hotplug_freq, 3, 1);
store_hotplug_param(hotplug_freq, 4, 0);
#endif
/* hotplug load */
store_hotplug_param(hotplug_load, 1, 1);
store_hotplug_param(hotplug_load, 2, 0);
#if NR_CPUS >= 4
store_hotplug_param(hotplug_load, 2, 1);
store_hotplug_param(hotplug_load, 3, 0);
store_hotplug_param(hotplug_load, 3, 1);
store_hotplug_param(hotplug_load, 4, 0);
#endif
/* hotplug rq */
store_hotplug_param(hotplug_rq, 1, 1);
store_hotplug_param(hotplug_rq, 2, 0);
#if NR_CPUS >= 4
store_hotplug_param(hotplug_rq, 2, 1);
store_hotplug_param(hotplug_rq, 3, 0);
store_hotplug_param(hotplug_rq, 3, 1);
store_hotplug_param(hotplug_rq, 4, 0);
#endif

define_one_global_rw(hotplug_freq_1_1);
define_one_global_rw(hotplug_freq_2_0);
#if NR_CPUS >= 4
define_one_global_rw(hotplug_freq_2_1);
define_one_global_rw(hotplug_freq_3_0);
define_one_global_rw(hotplug_freq_3_1);
define_one_global_rw(hotplug_freq_4_0);
#endif

define_one_global_rw(hotplug_load_1_1);
define_one_global_rw(hotplug_load_2_0);
#if NR_CPUS >= 4
define_one_global_rw(hotplug_load_2_1);
define_one_global_rw(hotplug_load_3_0);
define_one_global_rw(hotplug_load_3_1);
define_one_global_rw(hotplug_load_4_0);
#endif

define_one_global_rw(hotplug_rq_1_1);
define_one_global_rw(hotplug_rq_2_0);
#if NR_CPUS >= 4
define_one_global_rw(hotplug_rq_2_1);
define_one_global_rw(hotplug_rq_3_0);
define_one_global_rw(hotplug_rq_3_1);
define_one_global_rw(hotplug_rq_4_0);
#endif

static void cpus_hotplugging(bool state) {

	atomic_set(&hotplug_tuners_ins.hotplug_enable, state);

	if (state)
		hotplug_start();
	else
		hotplug_stop();
}

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updaing
 * hotplug_tuners_ins.hotplug_sampling_rate might not be appropriate.
 * For example:
 * if the original sampling_rate was 1 second and the requested new sampling
 * rate is 10 ms because the user needs immediate reaction from ondemand
 * governor, but not sure if higher frequency will be required or not,
 * then, the hotplugging system may change the sampling rate too late;
 * up to 1 second later.
 * Thus, if we are reducing the hotplug sampling rate, we need to make the
 * new value effective immediately.
 */
 
static void update_sampling_rate(unsigned int new_rate)
{
	int cpu;

	hotplug_tuners_ins.hotplug_sampling_rate = new_rate;

	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) > 0) {
		get_online_cpus();
		for_each_online_cpu(cpu) {
			struct hotplug_cpuinfo *cpu_hotplug_cpuinfo;
			unsigned long next_sampling, appointed_at;

			cpu_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);

			if (!delayed_work_pending(&cpu_hotplug_cpuinfo->work)) {
				continue;
			}

			next_sampling  = jiffies + msecs_to_jiffies(new_rate);
			appointed_at = cpu_hotplug_cpuinfo->work.timer.expires;

			if (time_before(next_sampling, appointed_at)) {

				cancel_delayed_work_sync(&cpu_hotplug_cpuinfo->work);

				queue_delayed_work_on(cpu_hotplug_cpuinfo->cpu, alucardhp_wq, &cpu_hotplug_cpuinfo->work, msecs_to_jiffies(new_rate));
			}
		}
		put_online_cpus();
	}
}

/* hotplug_sampling_rate */
static ssize_t store_hotplug_sampling_rate(struct kobject *a,
				struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(input, 10);

	if (input == hotplug_tuners_ins.hotplug_sampling_rate)
		return count;

	update_sampling_rate(input);

	return count;
}

/* hotplug_enable */
static ssize_t store_hotplug_enable(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = input > 0;

	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) == input)
		return count;

	if (input > 0)
		cpus_hotplugging(true);
	else
		cpus_hotplugging(false);

	return count;
}

/* cpu_up_rate */
static ssize_t store_cpu_up_rate(struct kobject *a, struct attribute *b,
				 const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,MAX_HOTPLUG_RATE),1);

	if (input == hotplug_tuners_ins.cpu_up_rate)
		return count;

	hotplug_tuners_ins.cpu_up_rate = input;

	return count;
}

/* cpu_down_rate */
static ssize_t store_cpu_down_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,MAX_HOTPLUG_RATE),1);

	if (input == hotplug_tuners_ins.cpu_down_rate)
		return count;

	hotplug_tuners_ins.cpu_down_rate = input;
	return count;
}

/* maxcoreslimit */
static ssize_t store_maxcoreslimit(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(input > NR_CPUS ? NR_CPUS : input, 1);

	if (atomic_read(&hotplug_tuners_ins.maxcoreslimit) == input)
		return count;

	atomic_set(&hotplug_tuners_ins.maxcoreslimit, input);

	return count;
}

/* maxcoreslimit_sleep */
static ssize_t store_maxcoreslimit_sleep(struct kobject *a,
				struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(input > NR_CPUS ? NR_CPUS : input, 1);

	if (atomic_read(&hotplug_tuners_ins.maxcoreslimit_sleep) == input)
		return count;

	atomic_set(&hotplug_tuners_ins.maxcoreslimit_sleep, input);

	return count;
}

#if 0
/* core_thermal_enable */
static ssize_t store_core_thermal_enable(struct kobject *a,
				struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = input > 0;

	if (atomic_read(&hotplug_tuners_ins.core_thermal_enable) == input)
		return count;

	atomic_set(&hotplug_tuners_ins.core_thermal_enable, input);

	return count;
}
#endif

define_one_global_rw(hotplug_sampling_rate);
define_one_global_rw(hotplug_enable);
define_one_global_rw(cpu_up_rate);
define_one_global_rw(cpu_down_rate);
define_one_global_rw(maxcoreslimit);
define_one_global_rw(maxcoreslimit_sleep);
#if 0
define_one_global_rw(core_thermal_enable);
#endif

static struct attribute *alucard_hotplug_attributes[] = {
	&hotplug_sampling_rate.attr,
	&hotplug_enable.attr,
	&hotplug_freq_1_1.attr,
	&hotplug_freq_2_0.attr,
#if NR_CPUS >= 4
	&hotplug_freq_2_1.attr,
	&hotplug_freq_3_0.attr,
	&hotplug_freq_3_1.attr,
	&hotplug_freq_4_0.attr,
#endif
	&hotplug_load_1_1.attr,
	&hotplug_load_2_0.attr,
#if NR_CPUS >= 4
	&hotplug_load_2_1.attr,
	&hotplug_load_3_0.attr,
	&hotplug_load_3_1.attr,
	&hotplug_load_4_0.attr,
#endif
	&hotplug_rq_1_1.attr,
	&hotplug_rq_2_0.attr,
#if NR_CPUS >= 4
	&hotplug_rq_2_1.attr,
	&hotplug_rq_3_0.attr,
	&hotplug_rq_3_1.attr,
	&hotplug_rq_4_0.attr,
#endif
	&cpu_up_rate.attr,
	&cpu_down_rate.attr,
	&maxcoreslimit.attr,
	&maxcoreslimit_sleep.attr,
#if 0
	&core_thermal_enable.attr,
#endif
	NULL
};

static struct attribute_group alucard_hotplug_attr_group = {
	.attrs = alucard_hotplug_attributes,
	.name = "alucard_hotplug",
};

static int __init alucard_hotplug_init(void)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay;
	int ret;
	unsigned int cpu;

	ret = sysfs_create_group(kernel_kobj, &alucard_hotplug_attr_group);
	if (ret) {
		printk(KERN_ERR "failed at(%d)\n", __LINE__);
		return ret;
	}

#ifdef CONFIG_MACH_LGE
	alucardhp_wq = alloc_workqueue("alucardhp_wq",
				WQ_HIGHPRI | WQ_UNBOUND, 1);
#else
	alucardhp_wq = alloc_workqueue("alucardhp_wq",
				WQ_HIGHPRI | WQ_UNBOUND, 1);
//				WQ_HIGHPRI | WQ_FREEZABLE, 1);
#endif

	if (!alucardhp_wq) {
		printk(KERN_ERR "Failed to create \
				alucardhp workqueue\n");
		return -EFAULT;
	}

	ret = init_rq_avg();
	if (ret) {
		return ret;
	}

#if 0
	/* Initiate timer time stamp */
	time_stamp = ktime_get();
#endif

	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) > 0) {

		hotplug_start();
	}

#if defined(CONFIG_POWERSUSPEND) || defined(CONFIG_HAS_EARLYSUSPEND)
#ifdef CONFIG_POWERSUSPEND
	register_power_suspend(&alucard_hotplug_power_suspend_driver);
#else
	register_early_suspend(&alucard_hotplug_early_suspend_driver);
#endif
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */

	return ret;
}

static void __exit alucard_hotplug_exit(void)
{
#if defined(CONFIG_POWERSUSPEND) || defined(CONFIG_HAS_EARLYSUSPEND)
#ifdef CONFIG_POWERSUSPEND
	unregister_power_suspend(&alucard_hotplug_power_suspend_driver);
#else
	unregister_early_suspend(&alucard_hotplug_early_suspend_driver);
#endif
#endif  /* CONFIG_POWERSUSPEND || CONFIG_HAS_EARLYSUSPEND */
	hotplug_stop();

	exit_rq_avg;

	destroy_workqueue(alucardhp_wq);

	sysfs_remove_group(kernel_kobj, &alucard_hotplug_attr_group);
}
MODULE_AUTHOR("Alucard_24@XDA");
MODULE_DESCRIPTION("'alucard_hotplug' - A cpu hotplug driver for "
	"capable processors");
MODULE_LICENSE("GPL");

late_initcall(alucard_hotplug_init);
late_initexit(alucard_hotplug_exit);
