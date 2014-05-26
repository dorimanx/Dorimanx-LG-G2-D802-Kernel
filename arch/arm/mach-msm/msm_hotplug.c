/*
 * MSM Hotplug Driver
 *
 * Copyright (c) 2013-2014, Fluxi <linflux@arcor.de>
 * Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/lcd_notify.h>
#include <linux/input.h>
#include <linux/math64.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>

#define MSM_HOTPLUG		"msm_hotplug"
#define HOTPLUG_ENABLED		0
#define DEFAULT_UPDATE_RATE	100 / 10
#define START_DELAY		100 * 20
#define MIN_INPUT_INTERVAL	150 * 1000L
#define DEFAULT_HISTORY_SIZE	10
#define DEFAULT_DOWN_LOCK_DUR	1000
#define DEFAULT_BOOST_LOCK_DUR	4000 * 1000L
#define DEFAULT_NR_CPUS_BOOSTED	1
#define DEFAULT_MIN_CPUS_ONLINE	1
#define DEFAULT_MAX_CPUS_ONLINE	NR_CPUS
#define DEFAULT_FAST_LANE_LOAD	95

static unsigned int debug = 0;
module_param_named(debug_mask, debug, uint, 0644);

#define dprintk(msg...)		\
do { 				\
	if (debug)		\
		pr_info(msg);	\
} while (0)

static struct cpu_hotplug {
	unsigned int msm_enabled;
	unsigned int target_cpus;
	unsigned int min_cpus_online;
	unsigned int max_cpus_online;
	unsigned int cpus_boosted;
	unsigned int offline_load;
	unsigned int down_lock_dur;
	u64 boost_lock_dur;
	u64 last_input;
	unsigned int fast_lane_load;
	struct work_struct up_work;
	struct work_struct down_work;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	struct notifier_block notif;
} hotplug = {
	.msm_enabled = HOTPLUG_ENABLED,
	.min_cpus_online = DEFAULT_MIN_CPUS_ONLINE,
	.max_cpus_online = DEFAULT_MAX_CPUS_ONLINE,
	.cpus_boosted = DEFAULT_NR_CPUS_BOOSTED,
	.down_lock_dur = DEFAULT_DOWN_LOCK_DUR,
	.boost_lock_dur = DEFAULT_BOOST_LOCK_DUR,
	.fast_lane_load = DEFAULT_FAST_LANE_LOAD
};

static struct workqueue_struct *hotplug_wq;
static struct delayed_work hotplug_work;

static u64 last_boost_time;

static struct cpu_stats {
	unsigned int update_rate;
	unsigned int *load_hist;
	unsigned int hist_size;
	unsigned int hist_cnt;
	unsigned int min_cpus;
	unsigned int total_cpus;
	unsigned int online_cpus;
	unsigned int cur_avg_load;
	unsigned int cur_max_load;
	struct mutex stats_mutex;
} stats = {
	.update_rate = DEFAULT_UPDATE_RATE,
	.hist_size = DEFAULT_HISTORY_SIZE,
	.min_cpus = 1,
	.total_cpus = NR_CPUS
};

struct down_lock {
	unsigned int locked;
	struct delayed_work lock_rem;
};

static DEFINE_PER_CPU(struct down_lock, lock_info);

struct cpu_load_data {
	u64 prev_cpu_idle;
	u64 prev_cpu_wall;
	u64 prev_cpu_iowait;
	unsigned int avg_load_maxfreq;
	unsigned int cur_load_maxfreq;
	unsigned int samples;
	unsigned int window_size;
	cpumask_var_t related_cpus;
};

static DEFINE_PER_CPU(struct cpu_load_data, cpuload);

#if 0 /* functions moved to drivers/cpufreq/cpufreq.c */
static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu,
					    cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}
#endif

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu,
					      cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static int update_average_load(unsigned int cpu)
{
	int ret;
	unsigned int idle_time, wall_time, iowait_time;
	unsigned int cur_load, load_max_freq;
	cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
	struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);
	struct cpufreq_policy policy;

	ret = cpufreq_get_policy(&policy, cpu);
	if (ret)
		return -EINVAL;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);
	cur_iowait_time = get_cpu_iowait_time(cpu, &cur_wall_time);

	wall_time = (unsigned int) (cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int) (cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	iowait_time = (unsigned int) (cur_iowait_time - pcpu->prev_cpu_iowait);
	pcpu->prev_cpu_iowait = cur_iowait_time;

	if (idle_time >= iowait_time)
		idle_time -= iowait_time;

	if (unlikely(!wall_time || wall_time < idle_time))
		return 0;

	cur_load = 100 * (wall_time - idle_time) / wall_time;

	/* Calculate the scaled load across cpu */
	load_max_freq = (cur_load * policy.cur) / policy.max;

	if (!pcpu->avg_load_maxfreq) {
		/* This is the first sample in this window */
		pcpu->avg_load_maxfreq = load_max_freq;
		pcpu->window_size = wall_time;
	} else {
		/*
		 * The is already a sample available in this window.
		 * Compute weighted average with prev entry, so that
		 * we get the precise weighted load.
		 */
		pcpu->avg_load_maxfreq =
			((pcpu->avg_load_maxfreq * pcpu->window_size) +
			(load_max_freq * wall_time)) /
			(wall_time + pcpu->window_size);

		pcpu->window_size += wall_time;
	}

	return 0;
}

static unsigned int load_at_max_freq(void)
{
	int cpu;
	unsigned int total_load = 0, max_load = 0;
	struct cpu_load_data *pcpu;

	for_each_online_cpu(cpu) {
		pcpu = &per_cpu(cpuload, cpu);
		update_average_load(cpu);
		total_load += pcpu->avg_load_maxfreq;
		pcpu->cur_load_maxfreq = pcpu->avg_load_maxfreq;
		max_load = max(max_load, pcpu->avg_load_maxfreq);
		pcpu->avg_load_maxfreq = 0;
	}
	stats.cur_max_load = max_load;

	return total_load;
}
static void update_load_stats(void)
{
	unsigned int i, j;
	unsigned int load = 0;

	mutex_lock(&stats.stats_mutex);
	stats.online_cpus = num_online_cpus();

	if (stats.hist_size > 1) {
		stats.load_hist[stats.hist_cnt] = load_at_max_freq();
	} else {
		stats.cur_avg_load = load_at_max_freq();
		mutex_unlock(&stats.stats_mutex);
		return;
	}

	for (i = 0, j = stats.hist_cnt; i < stats.hist_size; i++, j--) {
		load += stats.load_hist[j];

		if (j == 0)
			j = stats.hist_size;
	}

	if (++stats.hist_cnt == stats.hist_size)
		stats.hist_cnt = 0;

	stats.cur_avg_load = load / stats.hist_size;
	mutex_unlock(&stats.stats_mutex);
}

struct loads_tbl {
	unsigned int up_threshold;
	unsigned int down_threshold;
};

#define LOAD_SCALE(u, d)     \
{                            \
	.up_threshold = u,   \
	.down_threshold = d, \
}

static struct loads_tbl loads[] = {
	LOAD_SCALE(400, 0),
	LOAD_SCALE(65, 0),
	LOAD_SCALE(120, 50),
	LOAD_SCALE(190, 100),
	LOAD_SCALE(410, 170),
	LOAD_SCALE(0, 0),
};

static void apply_down_lock(unsigned int cpu)
{
	struct down_lock *dl = &per_cpu(lock_info, cpu);

	dl->locked = 1;
	queue_delayed_work_on(0, hotplug_wq, &dl->lock_rem,
			      msecs_to_jiffies(hotplug.down_lock_dur));
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

static int get_lowest_load_cpu(void)
{
	int cpu, lowest_cpu = 0;
	unsigned int lowest_load = UINT_MAX;
	unsigned int cpu_load[NR_CPUS];
	unsigned int proj_load;
	struct cpu_load_data *pcpu;

	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		pcpu = &per_cpu(cpuload, cpu);
		cpu_load[cpu] = pcpu->cur_load_maxfreq;
		if (cpu_load[cpu] < lowest_load) {
			lowest_load = cpu_load[cpu];
			lowest_cpu = cpu;
		}
	}

	proj_load = stats.cur_avg_load - lowest_load;
	if (proj_load > loads[stats.online_cpus - 1].up_threshold)
		return -EPERM;

	if (hotplug.offline_load && lowest_load >= hotplug.offline_load)
		return -EPERM;

	return lowest_cpu;
}

static void __ref cpu_up_work(struct work_struct *work)
{
	int cpu;
	unsigned int target;

	target = hotplug.target_cpus;

	for_each_cpu_not(cpu, cpu_online_mask) {
		if (target <= num_online_cpus())
			break;
		if (cpu == 0)
			continue;
		cpu_up(cpu);
		apply_down_lock(cpu);
	}
}

static void cpu_down_work(struct work_struct *work)
{
	int cpu, lowest_cpu;
	unsigned int target;

	target = hotplug.target_cpus;

	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		lowest_cpu = get_lowest_load_cpu();
		if (check_down_lock(cpu))
			break;
		cpu_down(lowest_cpu);
		if (target >= num_online_cpus())
			break;
	}
}

static void online_cpu(unsigned int target)
{
	unsigned int online_cpus = num_online_cpus();

	if (!hotplug.msm_enabled)
		return;

	/*
	 * Do not online more CPUs if max_cpus_online reached
	 * and cancel online task if target already achieved.
	 */
	if (target <= online_cpus ||
		online_cpus >= hotplug.max_cpus_online)
		return;

	hotplug.target_cpus = target;
	queue_work_on(0, hotplug_wq, &hotplug.up_work);
}

static void offline_cpu(unsigned int target)
{
	unsigned int online_cpus = num_online_cpus();
	u64 now;

	if (!hotplug.msm_enabled)
		return;

	/*
	 * Do not offline more CPUs if min_cpus_online reached
	 * and cancel offline task if target already achieved.
	 */
	if (target >= online_cpus ||
		online_cpus <= hotplug.min_cpus_online)
		return;

	now = ktime_to_us(ktime_get());
	if (online_cpus <= hotplug.cpus_boosted &&
	    (now - hotplug.last_input < hotplug.boost_lock_dur))
		return;

	hotplug.target_cpus = target;
	queue_work_on(0, hotplug_wq, &hotplug.down_work);
}

static int reschedule_hotplug_work(void)
{
	return queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
				     stats.update_rate);
}

static void msm_hotplug_work(struct work_struct *work)
{
	unsigned int cur_load, online_cpus, target = 0;
	unsigned int i;

	update_load_stats();

	if (stats.cur_max_load >= hotplug.fast_lane_load) {
		/* Enter the fast lane */
		online_cpu(hotplug.max_cpus_online);
		goto reschedule;
	}

	cur_load = stats.cur_avg_load;
	online_cpus = stats.online_cpus;

	/* If number of cpus locked, break out early */
	if (hotplug.min_cpus_online == num_possible_cpus()) {
		if (online_cpus != hotplug.min_cpus_online)
			online_cpu(hotplug.min_cpus_online);
		goto reschedule;
	} else if (hotplug.max_cpus_online == stats.min_cpus) {
		if (online_cpus != hotplug.max_cpus_online)
			offline_cpu(hotplug.max_cpus_online);
		goto reschedule;
	}

	for (i = stats.min_cpus; loads[i].up_threshold; i++) {
		if (cur_load <= loads[i].up_threshold
		    && cur_load > loads[i].down_threshold) {
			target = i;
			break;
		}
	}

	if (target > hotplug.max_cpus_online)
		target = hotplug.max_cpus_online;
	else if (target < hotplug.min_cpus_online)
		target = hotplug.min_cpus_online;

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

static void __ref msm_hotplug_resume_work(struct work_struct *work)
{
	int cpu;

	/* Fire up all CPUs */
	for_each_cpu_not(cpu, cpu_online_mask) {
		if (cpu == 0)
			continue;
		cpu_up(cpu);
		apply_down_lock(cpu);
	}
}

static int lcd_notifier_callback(struct notifier_block *nb,
                                 unsigned long event, void *data)
{
        if (event == LCD_EVENT_ON_START)
		schedule_work(&hotplug.resume_work);

        return 0;
}

static void hotplug_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value)
{
	u64 now = ktime_to_us(ktime_get());

	hotplug.last_input = now;
	if (now - last_boost_time < MIN_INPUT_INTERVAL)
		return;

	if (num_online_cpus() >= hotplug.cpus_boosted ||
		hotplug.cpus_boosted <= hotplug.min_cpus_online)
		return;

	dprintk("%s: online_cpus: %u boosted\n", MSM_HOTPLUG,
		stats.online_cpus);

	online_cpu(hotplug.cpus_boosted);
	last_boost_time = ktime_to_us(ktime_get());
}

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
	handle->name = handler->name;

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

static void hotplug_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

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

static ssize_t show_enable_hotplug(struct device *dev,
				   struct device_attribute *msm_hotplug_attrs,
				   char *buf)
{
	return sprintf(buf, "%u\n", hotplug.msm_enabled);
}

static ssize_t store_enable_hotplug(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    const char *buf, size_t count)
{
	int ret, cpu;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == hotplug.msm_enabled)
		return count;

	hotplug.msm_enabled = val;

	if (hotplug.msm_enabled) {
		reschedule_hotplug_work();
	} else {
		flush_workqueue(hotplug_wq);
		cancel_delayed_work_sync(&hotplug_work);
		for_each_online_cpu(cpu) {
			if (cpu == 0)
				continue;
			cpu_down(cpu);
		}
	}

	return count;
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

	hotplug.down_lock_dur = val;

	return count;
}

static ssize_t show_boost_lock_duration(struct device *dev,
				        struct device_attribute
				        *msm_hotplug_attrs, char *buf)
{
	return sprintf(buf, "%llu\n", div_u64(hotplug.boost_lock_dur, 1000));
}

static ssize_t store_boost_lock_duration(struct device *dev,
					 struct device_attribute
					 *msm_hotplug_attrs, const char *buf,
					 size_t count)
{
	int ret;
	u64 val;

	ret = sscanf(buf, "%llu", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.boost_lock_dur = val * 1000;

	return count;
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

	return count;
}

static ssize_t show_load_levels(struct device *dev,
				struct device_attribute *msm_hotplug_attrs,
				char *buf)
{
	int i, len = 0;

	if (!buf)
		return -EINVAL;

	for (i = 0; loads[i].up_threshold; i++) {
		len += sprintf(buf + len, "%u ", i);
		len += sprintf(buf + len, "%u ", loads[i].up_threshold);
		len += sprintf(buf + len, "%u\n", loads[i].down_threshold);
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
	if (ret != ARRAY_SIZE(val) || val[2] > val[1])
		return -EINVAL;

	loads[val[0]].up_threshold = val[1];
	loads[val[0]].down_threshold = val[2];

	return count;
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

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > 20)
		return -EINVAL;

	flush_workqueue(hotplug_wq);
	cancel_delayed_work_sync(&hotplug_work);

	memset(stats.load_hist, 0, sizeof(stats.load_hist));
	stats.hist_size = val;

	reschedule_hotplug_work();

	return count;
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

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > DEFAULT_MAX_CPUS_ONLINE)
		return -EINVAL;

	if (hotplug.max_cpus_online < val)
		hotplug.max_cpus_online = val;

	hotplug.min_cpus_online = val;

	return count;
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

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > DEFAULT_MAX_CPUS_ONLINE)
		return -EINVAL;

	if (hotplug.min_cpus_online > val)
		hotplug.min_cpus_online = val;

	hotplug.max_cpus_online = val;

	return count;
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
	if (ret != 1 || val < 1 || val > DEFAULT_MAX_CPUS_ONLINE)
		return -EINVAL;

	hotplug.cpus_boosted = val;

	return count;
}

static ssize_t show_offline_load(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", hotplug.offline_load);
}

static ssize_t store_offline_load(struct device *dev,
				  struct device_attribute *msm_hotplug_attrs,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.offline_load = val;

	return count;
}

static ssize_t show_fast_lane_load(struct device *dev,
				   struct device_attribute *msm_hotplug_attrs,
				   char *buf)
{
	return sprintf(buf, "%u\n", hotplug.fast_lane_load);
}

static ssize_t store_fast_lane_load(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.fast_lane_load = val;

	return count;
}

static ssize_t show_current_load(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", stats.cur_avg_load);
}

static DEVICE_ATTR(msm_enabled, 644, show_enable_hotplug, store_enable_hotplug);
static DEVICE_ATTR(down_lock_duration, 644, show_down_lock_duration,
		   store_down_lock_duration);
static DEVICE_ATTR(boost_lock_duration, 644, show_boost_lock_duration,
		   store_boost_lock_duration);
static DEVICE_ATTR(update_rate, 644, show_update_rate, store_update_rate);
static DEVICE_ATTR(load_levels, 644, show_load_levels, store_load_levels);
static DEVICE_ATTR(history_size, 644, show_history_size, store_history_size);
static DEVICE_ATTR(min_cpus_online, 644, show_min_cpus_online,
		   store_min_cpus_online);
static DEVICE_ATTR(max_cpus_online, 644, show_max_cpus_online,
		   store_max_cpus_online);
static DEVICE_ATTR(cpus_boosted, 644, show_cpus_boosted, store_cpus_boosted);
static DEVICE_ATTR(offline_load, 644, show_offline_load, store_offline_load);
static DEVICE_ATTR(fast_lane_load, 644, show_fast_lane_load,
		   store_fast_lane_load);
static DEVICE_ATTR(current_load, 444, show_current_load, NULL);

static struct attribute *msm_hotplug_attrs[] = {
	&dev_attr_msm_enabled.attr,
	&dev_attr_down_lock_duration.attr,
	&dev_attr_boost_lock_duration.attr,
	&dev_attr_update_rate.attr,
	&dev_attr_load_levels.attr,
	&dev_attr_history_size.attr,
	&dev_attr_min_cpus_online.attr,
	&dev_attr_max_cpus_online.attr,
	&dev_attr_cpus_boosted.attr,
	&dev_attr_offline_load.attr,
	&dev_attr_fast_lane_load.attr,
	&dev_attr_current_load.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = msm_hotplug_attrs,
};

/************************** sysfs end ************************/

static int __devinit msm_hotplug_probe(struct platform_device *pdev)
{
	int cpu, ret = 0;
	struct kobject *module_kobj;
	struct down_lock *dl;

	hotplug_wq =
	    alloc_workqueue("msm_hotplug_wq", WQ_HIGHPRI | WQ_FREEZABLE, 0);
	if (!hotplug_wq) {
		pr_err("%s: Failed to allocate hotplug workqueue\n",
		       MSM_HOTPLUG);
		ret = -ENOMEM;
		goto err_out;
	}

	module_kobj = kset_find_obj(module_kset, MSM_HOTPLUG);
	if (!module_kobj) {
		pr_err("%s: Cannot find kobject for module\n", MSM_HOTPLUG);
		goto err_dev;
	}

	ret = sysfs_create_group(module_kobj, &attr_group);
	if (ret) {
		pr_err("%s: Failed to create sysfs: %d\n", MSM_HOTPLUG, ret);
		goto err_dev;
	}

	hotplug.notif.notifier_call = lcd_notifier_callback;
        if (lcd_register_client(&hotplug.notif) != 0) {
                pr_err("%s: Failed to register LCD notifier callback\n",
                       MSM_HOTPLUG);
		goto err_dev;
	}

	ret = input_register_handler(&hotplug_input_handler);
	if (ret) {
		pr_err("%s: Failed to register input handler: %d\n",
		       MSM_HOTPLUG, ret);
		goto err_dev;
	}

	stats.load_hist = kmalloc(sizeof(stats.hist_size), GFP_KERNEL);
	if (!stats.load_hist) {
		pr_err("%s: Failed to allocated memory\n", MSM_HOTPLUG);
		ret = -ENOMEM;
		goto err_dev;
	}

	mutex_init(&stats.stats_mutex);

	INIT_DELAYED_WORK(&hotplug_work, msm_hotplug_work);
	INIT_WORK(&hotplug.up_work, cpu_up_work);
	INIT_WORK(&hotplug.down_work, cpu_down_work);
	INIT_WORK(&hotplug.resume_work, msm_hotplug_resume_work);

	for_each_possible_cpu(cpu) {
		dl = &per_cpu(lock_info, cpu);
		INIT_DELAYED_WORK(&dl->lock_rem, remove_down_lock);
	}

	if (hotplug.msm_enabled)
		queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
				      START_DELAY);

	return ret;
err_dev:
	module_kobj = NULL;
	destroy_workqueue(hotplug_wq);
err_out:
	return ret;
}

static struct platform_device msm_hotplug_device = {
	.name = MSM_HOTPLUG,
	.id = -1,
};

static int msm_hotplug_remove(struct platform_device *pdev)
{
	destroy_workqueue(hotplug_wq);
	input_unregister_handler(&hotplug_input_handler);
	kfree(stats.load_hist);

	return 0;
}

static struct platform_driver msm_hotplug_driver = {
	.probe = msm_hotplug_probe,
	.remove = msm_hotplug_remove,
	.driver = {
		.name = MSM_HOTPLUG,
		.owner = THIS_MODULE,
	},
};

static int __init msm_hotplug_init(void)
{
	int ret;

	ret = platform_driver_register(&msm_hotplug_driver);
	if (ret) {
		pr_err("%s: Driver register failed: %d\n", MSM_HOTPLUG, ret);
		return ret;
	}

	ret = platform_device_register(&msm_hotplug_device);
	if (ret) {
		pr_err("%s: Device register failed: %d\n", MSM_HOTPLUG, ret);
		return ret;
	}

	pr_info("%s: Device init\n", MSM_HOTPLUG);

	return ret;
}

static void __exit msm_hotplug_exit(void)
{
	platform_device_unregister(&msm_hotplug_device);
	platform_driver_unregister(&msm_hotplug_driver);
}

late_initcall(msm_hotplug_init);
module_exit(msm_hotplug_exit);

MODULE_AUTHOR("Fluxi <linflux@arcor.de>");
MODULE_DESCRIPTION("MSM Hotplug Driver");
MODULE_LICENSE("GPLv2");
