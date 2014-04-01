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
#include "acpuclock.h"

static struct mutex timer_mutex;

static struct delayed_work alucard_hotplug_work;
static struct work_struct up_work;
static struct work_struct down_work;

static ktime_t time_stamp;

static struct hotplug_cpuinfo {
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_idle;
	int online;
	int up_cpu;
	int up_by_cpu;
};

static DEFINE_PER_CPU(struct hotplug_cpuinfo, od_hotplug_cpuinfo);

static struct hotplug_tuners {
	atomic_t hotplug_sampling_rate;
	atomic_t hotplug_enable;
	atomic_t cpu_up_rate;
	atomic_t cpu_down_rate;
	atomic_t maxcoreslimit;
} hotplug_tuners_ins = {
	.hotplug_sampling_rate = ATOMIC_INIT(60),
	.hotplug_enable = ATOMIC_INIT(0),
	.cpu_up_rate = ATOMIC_INIT(2),
	.cpu_down_rate = ATOMIC_INIT(20),
	.maxcoreslimit = ATOMIC_INIT(NR_CPUS),
};

#define MAX_HOTPLUG_RATE	(40)
#define DOWN_INDEX		(0)
#define UP_INDEX		(1)

#define RQ_AVG_TIMER_RATE	10

static struct runqueue_data {
	unsigned int nr_run_avg;
	unsigned int update_rate;
	int64_t last_time;
	int64_t total_time;
	struct delayed_work work;
	struct workqueue_struct *nr_run_wq;
	spinlock_t lock;
};

static struct runqueue_data *rq_data;
static void rq_work_fn(struct work_struct *work);

static void start_rq_work(void)
{
	rq_data->nr_run_avg = 0;
	rq_data->last_time = 0;
	rq_data->total_time = 0;
	if (rq_data->nr_run_wq == NULL)
		rq_data->nr_run_wq =
			create_singlethread_workqueue("nr_run_avg");

	queue_delayed_work(rq_data->nr_run_wq, &rq_data->work,
			   msecs_to_jiffies(rq_data->update_rate));
	return;
}

static void stop_rq_work(void)
{
	if (rq_data->nr_run_wq)
		cancel_delayed_work(&rq_data->work);
	return;
}

static int __init init_rq_avg(void)
{
	rq_data = kzalloc(sizeof(struct runqueue_data), GFP_KERNEL);
	if (rq_data == NULL) {
		pr_err("%s cannot allocate memory\n", __func__);
		return -ENOMEM;
	}
	spin_lock_init(&rq_data->lock);
	rq_data->update_rate = RQ_AVG_TIMER_RATE;
	INIT_DEFERRABLE_WORK(&rq_data->work, rq_work_fn);

	return 0;
}

static void rq_work_fn(struct work_struct *work)
{
	int64_t time_diff = 0;
	int64_t nr_run = 0;
	unsigned long flags = 0;
	int64_t cur_time;

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

	if (rq_data->update_rate != 0)
		queue_delayed_work(rq_data->nr_run_wq, &rq_data->work,
				   msecs_to_jiffies(rq_data->update_rate));

	spin_unlock_irqrestore(&rq_data->lock, flags);
}

static unsigned int get_nr_run_avg(void)
{
	unsigned int nr_run_avg;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_data->lock, flags);
	nr_run_avg = rq_data->nr_run_avg;
	rq_data->nr_run_avg = 0;
	spin_unlock_irqrestore(&rq_data->lock, flags);

	return nr_run_avg;
}

static unsigned hotplugging_rate = 0;

static atomic_t hotplug_freq[4][2] = {
	{ATOMIC_INIT(0), ATOMIC_INIT(1267200)},
	{ATOMIC_INIT(960000), ATOMIC_INIT(1497600)},
	{ATOMIC_INIT(1036800), ATOMIC_INIT(1728000)},
	{ATOMIC_INIT(1190400), ATOMIC_INIT(0)}
};
static atomic_t hotplug_load[4][2] = {
	{ATOMIC_INIT(0), ATOMIC_INIT(65)},
	{ATOMIC_INIT(30), ATOMIC_INIT(65)},
	{ATOMIC_INIT(30), ATOMIC_INIT(65)},
	{ATOMIC_INIT(30), ATOMIC_INIT(0)}
};
static atomic_t hotplug_rq[4][2] = {
	{ATOMIC_INIT(0), ATOMIC_INIT(300)},
	{ATOMIC_INIT(300), ATOMIC_INIT(300)},
	{ATOMIC_INIT(300), ATOMIC_INIT(300)},
	{ATOMIC_INIT(300), ATOMIC_INIT(0)}
};

#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", atomic_read(&hotplug_tuners_ins.object));		\
}
show_one(hotplug_sampling_rate, hotplug_sampling_rate);
show_one(hotplug_enable, hotplug_enable);
show_one(cpu_up_rate, cpu_up_rate);
show_one(cpu_down_rate, cpu_down_rate);
show_one(maxcoreslimit, maxcoreslimit);

#define show_hotplug_param(file_name, num_core, up_down)		\
static ssize_t show_##file_name##_##num_core##_##up_down		\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", atomic_read(&file_name[num_core - 1][up_down]));	\
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
	if (input == atomic_read(&file_name[num_core - 1][up_down])) {		\
		return count;	\
	}	\
	atomic_set(&file_name[num_core - 1][up_down], input);			\
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
	unsigned int cpu = 0;
	int delay = 0;

	atomic_set(&hotplug_tuners_ins.hotplug_enable, state);

	mutex_lock(&timer_mutex);

	if (state) {
		start_rq_work();
		for_each_possible_cpu(cpu) {
			struct hotplug_cpuinfo *this_hotplug_cpuinfo;
			this_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);

			this_hotplug_cpuinfo->prev_cpu_idle = get_cpu_idle_time(cpu, &this_hotplug_cpuinfo->prev_cpu_wall, 0);
			this_hotplug_cpuinfo->up_cpu = 1;
			this_hotplug_cpuinfo->online = cpu_online(cpu);
			this_hotplug_cpuinfo->up_by_cpu = -1;
		}
		hotplugging_rate = 0;

		/* Initiate timer time stamp */
		time_stamp = ktime_get();

		delay = msecs_to_jiffies(atomic_read(&hotplug_tuners_ins.hotplug_sampling_rate));
		queue_delayed_work_on(0, system_wq, &alucard_hotplug_work, delay);
	} else {
		mutex_unlock(&timer_mutex);
		cancel_delayed_work_sync(&alucard_hotplug_work);
		cancel_work_sync(&up_work);
		cancel_work_sync(&down_work);
		stop_rq_work();
		mutex_lock(&timer_mutex);
	}

	mutex_unlock(&timer_mutex);
}

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updaing
 * hotplug_tuners_ins.hotplug_sampling_rate might not be appropriate. For example,
 * if the original sampling_rate was 1 second and the requested new sampling
 * rate is 10 ms because the user needs immediate reaction from ondemand
 * governor, but not sure if higher frequency will be required or not,
 * then, the hotplugging system may change the sampling rate too late; up to 1 second
 * later. Thus, if we are reducing the hotplug sampling rate, we need to make the
 * new value effective immediately.
 */
static void update_sampling_rate(unsigned int new_rate)
{
	unsigned long next_sampling, appointed_at;

	atomic_set(&hotplug_tuners_ins.hotplug_sampling_rate,new_rate);

	mutex_lock(&timer_mutex);

	if (!delayed_work_pending(&alucard_hotplug_work)) {
		mutex_unlock(&timer_mutex);
		return;
	}

	next_sampling  = jiffies + msecs_to_jiffies(new_rate);
	appointed_at = alucard_hotplug_work.timer.expires;

	if (time_before(next_sampling, appointed_at)) {

		mutex_unlock(&timer_mutex);
		cancel_delayed_work_sync(&alucard_hotplug_work);
		mutex_lock(&timer_mutex);

		queue_delayed_work_on(0, system_wq, &alucard_hotplug_work, msecs_to_jiffies(new_rate));
	}

	mutex_unlock(&timer_mutex);
}

/* hotplug_sampling_rate */
static ssize_t store_hotplug_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(input, 10);

	if (input == atomic_read(&hotplug_tuners_ins.hotplug_sampling_rate))
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

	if (input == atomic_read(&hotplug_tuners_ins.cpu_up_rate))
		return count;

	atomic_set(&hotplug_tuners_ins.cpu_up_rate,input);

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

	if (input == atomic_read(&hotplug_tuners_ins.cpu_down_rate))
		return count;

	atomic_set(&hotplug_tuners_ins.cpu_down_rate,input);
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

define_one_global_rw(hotplug_sampling_rate);
define_one_global_rw(hotplug_enable);
define_one_global_rw(cpu_up_rate);
define_one_global_rw(cpu_down_rate);
define_one_global_rw(maxcoreslimit);

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
	NULL
};

static struct attribute_group alucard_hotplug_attr_group = {
	.attrs = alucard_hotplug_attributes,
	.name = "alucard_hotplug",
};

static void __cpuinit cpu_up_work(struct work_struct *work)
{
	int cpu;

	for_each_cpu_not(cpu, cpu_online_mask) {
		struct hotplug_cpuinfo *this_hotplug_cpuinfo;
		this_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);
		if (this_hotplug_cpuinfo->online == true) {
			cpu_up(cpu);
		}
	}
}

static void __cpuexit cpu_down_work(struct work_struct *work)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct hotplug_cpuinfo *this_hotplug_cpuinfo;
		this_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);
		if (this_hotplug_cpuinfo->online == false && cpu != 0) {
			cpu_down(cpu);
		}
	}
}

static void hotplug_work_fn(struct work_struct *work)
{
	struct hotplug_cpuinfo *ref_hotplug_cpuinfo = NULL;
	int upmaxcoreslimit = 0;
	int up_rate = 0;
	int down_rate = 0;
	bool check_up = false, check_down = false;
	int schedule_down_cpu = 3;
	int schedule_up_cpu = 0;
	unsigned int cpu = 0;
	int online_cpu = 0;
	int offline_cpu = 0;
	int ref_cpu = -1;
	int online_cpus = 0;
	unsigned int rq_avg = 0;
	unsigned int sampling_rate=0;
	int delay;
	int cpus_off[4] = {-1, -1, -1, -1};
	int cpus_on[4] = {-1, -1, -1, -1};
	int idx_off = 0;

	mutex_lock(&timer_mutex);

	sampling_rate = max(10, atomic_read(&hotplug_tuners_ins.hotplug_sampling_rate));
	delay = msecs_to_jiffies(sampling_rate);
	/* set hotplugging_rate used */
	++hotplugging_rate;
	upmaxcoreslimit = atomic_read(&hotplug_tuners_ins.maxcoreslimit);
	up_rate = atomic_read(&hotplug_tuners_ins.cpu_up_rate);
	down_rate = atomic_read(&hotplug_tuners_ins.cpu_down_rate);
	check_up = (hotplugging_rate % up_rate == 0);
	check_down = (hotplugging_rate % down_rate == 0);
	rq_avg = get_nr_run_avg();

	online_cpus = num_online_cpus();

	for_each_cpu_not(cpu, cpu_online_mask) {
		struct hotplug_cpuinfo *this_hotplug_cpuinfo;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int wall_time, idle_time;

		this_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);

		cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);

		wall_time = (unsigned int)
				(cur_wall_time - this_hotplug_cpuinfo->prev_cpu_wall);
		this_hotplug_cpuinfo->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
				(cur_idle_time - this_hotplug_cpuinfo->prev_cpu_idle);
		this_hotplug_cpuinfo->prev_cpu_idle = cur_idle_time;

		cpus_off[idx_off] = cpu;
		++idx_off;
		++schedule_up_cpu;
		--schedule_down_cpu;

		this_hotplug_cpuinfo->online = false;
		this_hotplug_cpuinfo->up_cpu = 1;
		this_hotplug_cpuinfo->up_by_cpu = -1;
	}

	for_each_online_cpu(cpu) {
		struct hotplug_cpuinfo *this_hotplug_cpuinfo;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int wall_time, idle_time;
		int up_load;
		int down_load;
		unsigned int up_freq;
		unsigned int down_freq;
		unsigned int up_rq;
		unsigned int down_rq;
		int cur_load = -1;
		unsigned int cur_freq = 0;

		this_hotplug_cpuinfo =	&per_cpu(od_hotplug_cpuinfo, cpu);

		cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);

		wall_time = (unsigned int)
				(cur_wall_time - this_hotplug_cpuinfo->prev_cpu_wall);
		this_hotplug_cpuinfo->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
				(cur_idle_time - this_hotplug_cpuinfo->prev_cpu_idle);
		this_hotplug_cpuinfo->prev_cpu_idle = cur_idle_time;

		if (wall_time >= idle_time) { /*if wall_time < idle_time, evaluate cpu load next time*/
			cur_load = wall_time > idle_time ? (100 * (wall_time - idle_time)) / wall_time : 0;/*if wall_time is equal to idle_time cpu_load is equal to 0*/

			cur_freq = acpuclk_get_rate(cpu);

			up_load = atomic_read(&hotplug_load[cpu][UP_INDEX]);
			down_load = atomic_read(&hotplug_load[cpu][DOWN_INDEX]);
			up_freq = atomic_read(&hotplug_freq[cpu][UP_INDEX]);
			down_freq = atomic_read(&hotplug_freq[cpu][DOWN_INDEX]);
			up_rq = atomic_read(&hotplug_rq[cpu][UP_INDEX]);
			down_rq = atomic_read(&hotplug_rq[cpu][DOWN_INDEX]);

			/*printk(KERN_ERR "ONLINE CPUS[%u], CPU[%u], cur_freq[%u], cur_load[%d], rq_avg[%u]\n",online_cpus, cpu, cur_freq, cur_load, rq_avg);
			printk(KERN_ERR "U CPU[%u], cur_freq[%u], up_freq[%u], cur_load[%d], up_load[%d], offline_cpu[%d], schedule_up_cpu[%d]\n",cpu, cur_freq, up_freq, cur_load, up_load, offline_cpu, schedule_up_cpu);
			printk(KERN_ERR "D CPU[%u], cur_freq[%u], down_freq[%u], cur_load[%d], down_load[%d], schedule_down_cpu[%d]\n",cpu, cur_freq, down_freq, cur_load, down_load, schedule_down_cpu);*/

			if (check_up
					&& online_cpus < upmaxcoreslimit
					&& this_hotplug_cpuinfo->up_cpu > 0
					&& schedule_up_cpu > 0
					&& cur_load >= up_load
					&& cur_freq >= up_freq
					&& rq_avg > up_rq) {
				if (offline_cpu < idx_off
						&& cpus_off[offline_cpu] > 0) {
					ref_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpus_off[offline_cpu]);
					ref_hotplug_cpuinfo->online = true;
					ref_hotplug_cpuinfo->up_by_cpu = cpu;
					this_hotplug_cpuinfo->up_cpu = 0;
					++offline_cpu;
					--schedule_up_cpu;
				}

			} else if (check_down
					&& cpu > 0
					&& schedule_down_cpu > 0
					&& cur_load >= 0) {
				if (cur_load < down_load
						|| (cur_freq <= down_freq
						&& rq_avg <= down_rq)) {
					ref_cpu = this_hotplug_cpuinfo->up_by_cpu;
					if (ref_cpu >= 0) {
						ref_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, ref_cpu);
						ref_hotplug_cpuinfo->up_cpu = 1;
					}
					this_hotplug_cpuinfo->online = false;
					this_hotplug_cpuinfo->up_cpu = 1;
					this_hotplug_cpuinfo->up_by_cpu = -1;
					cpus_on[online_cpu] = cpu;
					++online_cpu;
					--schedule_down_cpu;
				}
			}
		}
	}
	if (offline_cpu > 0) {
		queue_work_on(0, system_wq, &up_work);
	}

	if (online_cpu > 0) {
		queue_work_on(0, system_wq, &down_work);
	}

	if (hotplugging_rate >= max(up_rate, down_rate)) {
		hotplugging_rate = 0;
	}

	if (num_online_cpus() == 1) {
		ref_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, 0);
		ref_hotplug_cpuinfo->up_cpu = 1;
	}
	queue_delayed_work_on(0, system_wq, &alucard_hotplug_work, delay);

	mutex_unlock(&timer_mutex);
}

static int __init alucard_hotplug_init(void)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay;
	unsigned int cpu;
	int ret;

	ret = sysfs_create_group(kernel_kobj, &alucard_hotplug_attr_group);
	if (ret) {
		printk(KERN_ERR "failed at(%d)\n", __LINE__);
		return ret;
	}

	ret = init_rq_avg();
	if (ret) {
		return ret;
	}

	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) > 0)
		start_rq_work();

	for_each_possible_cpu(cpu) {
		struct hotplug_cpuinfo *this_hotplug_cpuinfo;
		this_hotplug_cpuinfo = &per_cpu(od_hotplug_cpuinfo, cpu);

		this_hotplug_cpuinfo->prev_cpu_idle = get_cpu_idle_time(cpu, &this_hotplug_cpuinfo->prev_cpu_wall, 0);
		this_hotplug_cpuinfo->up_cpu = 1;
		this_hotplug_cpuinfo->online = cpu_online(cpu);
		this_hotplug_cpuinfo->up_by_cpu = -1;
	}
	hotplugging_rate = 0;
	mutex_init(&timer_mutex);

	/* Initiate timer time stamp */
	time_stamp = ktime_get();

	INIT_DELAYED_WORK(&alucard_hotplug_work, hotplug_work_fn);
	INIT_WORK(&up_work, cpu_up_work);
	INIT_WORK(&down_work, cpu_down_work);

	delay = msecs_to_jiffies(atomic_read(&hotplug_tuners_ins.hotplug_sampling_rate));

	if (atomic_read(&hotplug_tuners_ins.hotplug_enable) > 0)
		queue_delayed_work_on(0, system_wq, &alucard_hotplug_work, delay);

	return ret;
}

static void __exit alucard_hotplug_exit(void)
{
	cancel_delayed_work_sync(&alucard_hotplug_work);
	cancel_work_sync(&up_work);
	cancel_work_sync(&down_work);
	stop_rq_work();

	mutex_destroy(&timer_mutex);

	sysfs_remove_group(kernel_kobj,
					   &alucard_hotplug_attr_group);
}
MODULE_AUTHOR("Alucard_24@XDA");
MODULE_DESCRIPTION("'alucard_hotplug' - A cpu hotplug driver for "
	"capable processors");
MODULE_LICENSE("GPL");
module_init(alucard_hotplug_init);
module_exit(alucard_hotplug_exit);
