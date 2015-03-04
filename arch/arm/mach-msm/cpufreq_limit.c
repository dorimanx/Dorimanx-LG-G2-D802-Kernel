/*
 * MSM CPU Frequency Limiter Driver
 *
 * Copyright (c) 2012-2014, Paul Reioux Faux123 <reioux@gmail.com>
 * Copyright (c) 2014, Dorimanx <yuri@bynet.co.il>
 * Copyright (c) 2014, Pranav Vashi <neobuddy89@gmail.com>
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

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <mach/cpufreq.h>
#include <linux/fb.h>

#define MSM_CPUFREQ_LIMIT_MAJOR		4
#define MSM_CPUFREQ_LIMIT_MINOR		8

#define MSM_LIMIT			"msm_cpufreq_limit"

static struct workqueue_struct *limiter_wq;

#define DEFAULT_SUSPEND_DEFER_TIME	10
#define DEFAULT_SUSPEND_FREQUENCY	0
#define DEFAULT_RESUME_FREQUENCY	2265600

static unsigned int debug = 1;
module_param_named(debug_mask, debug, uint, 0644);

#define dprintk(msg...)		\
do {				\
	if (debug)		\
		pr_info(msg);	\
} while (0)

static struct cpu_limit {
	uint32_t suspend_max_freq;
	uint32_t resume_max_freq;
	unsigned int suspended;
	unsigned int suspend_defer_time;
	struct delayed_work suspend_work;
	struct work_struct resume_work;
	struct mutex msm_limiter_mutex;
	struct notifier_block notif;
} limit = {
	.suspend_max_freq = DEFAULT_SUSPEND_FREQUENCY,
	.resume_max_freq = DEFAULT_RESUME_FREQUENCY,
	.suspended = 0,
	.suspend_defer_time = DEFAULT_SUSPEND_DEFER_TIME,
};

static void msm_limit_suspend(struct work_struct *work)
{
	/* Save current instance */
	limit.resume_max_freq = get_max_lock(0);

	if (!limit.resume_max_freq)
		return;

	mutex_lock(&limit.msm_limiter_mutex);
	limit.suspended = 1;
	mutex_unlock(&limit.msm_limiter_mutex);

	set_max_lock(0, limit.suspend_max_freq);

	dprintk("Limit cpu0 max freq to %d\n",
		limit.suspend_max_freq);
}

static void __ref msm_limit_resume(struct work_struct *work)
{
	/* Do not resume if didnt suspended */
	if (!limit.suspended)
		return;

	mutex_lock(&limit.msm_limiter_mutex);
	limit.suspended = 0;
	mutex_unlock(&limit.msm_limiter_mutex);

	set_max_lock(0, limit.resume_max_freq);

	dprintk("Restore cpu0 max freq to %d\n",
		limit.resume_max_freq);
}

static void __msm_limit_suspend(void)
{
	/* Do not suspend if suspend freq is not available */
	if (limit.suspend_max_freq == 0)
		return;

	INIT_DELAYED_WORK(&limit.suspend_work, msm_limit_suspend);
	mod_delayed_work_on(0, limiter_wq, &limit.suspend_work,
			msecs_to_jiffies(limit.suspend_defer_time * 1000));
}

static void __msm_limit_resume(void)
{
	/* Do not resume if suspend freq is not available */
	if (limit.suspend_max_freq == 0)
		return;

	flush_workqueue(limiter_wq);
	cancel_delayed_work_sync(&limit.suspend_work);
	queue_work_on(0, limiter_wq, &limit.resume_work);
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		switch (*blank) {
			case FB_BLANK_UNBLANK:
				/* display on */
				__msm_limit_resume();
				break;
			case FB_BLANK_POWERDOWN:
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_VSYNC_SUSPEND:
			case FB_BLANK_NORMAL:
				/* display off */
				__msm_limit_suspend();
				break;
		}
	}
	return NOTIFY_OK;
}

static int msm_cpufreq_limit_start(void)
{
	int ret = 0;

	limiter_wq =
	    alloc_workqueue("msm_limiter_wq", WQ_HIGHPRI | WQ_FREEZABLE, 0);
	if (!limiter_wq) {
		pr_err("%s: Failed to allocate limiter workqueue\n",
		       MSM_LIMIT);
		ret = -ENOMEM;
		goto err_out;
	}

	limit.notif.notifier_call = fb_notifier_callback;
	if (fb_register_client(&limit.notif)) {
		pr_err("%s: Failed to register FB notifier callback\n",
			MSM_LIMIT);
		goto err_dev;
	}

	mutex_init(&limit.msm_limiter_mutex);
	INIT_DELAYED_WORK(&limit.suspend_work, msm_limit_suspend);
	INIT_WORK(&limit.resume_work, msm_limit_resume);

	queue_work_on(0, limiter_wq, &limit.resume_work);

	return ret;
err_dev:
	destroy_workqueue(limiter_wq);
err_out:
	return ret;
}

static void msm_cpufreq_limit_stop(void)
{
	limit.suspended = 1;
	flush_workqueue(limiter_wq);
	cancel_work_sync(&limit.resume_work);
	cancel_delayed_work_sync(&limit.suspend_work);
	mutex_destroy(&limit.msm_limiter_mutex);
	fb_unregister_client(&limit.notif);
	limit.notif.notifier_call = NULL;
	destroy_workqueue(limiter_wq);
}

static ssize_t suspend_defer_time_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", limit.suspend_defer_time);
}

static ssize_t suspend_defer_time_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;

	limit.suspend_defer_time = val;

	return count;
}

static ssize_t suspend_max_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", limit.suspend_max_freq);
}

static ssize_t suspend_max_freq_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;

	if (val == 0)
		goto out;

	if (val == limit.suspend_max_freq)
		return count;

	if (val < policy->cpuinfo.min_freq)
		val = policy->cpuinfo.min_freq;
	else if (val > policy->cpuinfo.max_freq)
		val = policy->cpuinfo.max_freq;

out:
	limit.suspend_max_freq = val;

	return count;
}

static ssize_t msm_cpufreq_limit_cpu0_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int cpu = 0;
	return sprintf(buf, "%u\n", get_max_lock(cpu));
}

static ssize_t msm_cpufreq_limit_cpu0_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int cpu = 0;
	unsigned int val;
	int ret;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;

	if (val < 300000 || val > 2803200)
		val = 0;

	set_max_lock(cpu, val);

	return count;
}

static ssize_t msm_cpufreq_limit_cpu1_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int cpu = 1;
	return sprintf(buf, "%u\n", get_max_lock(cpu));
}

static ssize_t msm_cpufreq_limit_cpu1_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int cpu = 1;
	unsigned int val;
	int ret;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;

	if (val < 300000 || val > 2803200)
		val = 0;

	set_max_lock(cpu, val);

	return count;
}

static ssize_t msm_cpufreq_limit_cpu2_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int cpu = 2;
	return sprintf(buf, "%u\n", get_max_lock(cpu));
}

static ssize_t msm_cpufreq_limit_cpu2_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int cpu = 2;
	unsigned int val;
	int ret;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;

	if (val < 300000 || val > 2803200)
		val = 0;

	set_max_lock(cpu, val);

	return count;
}

static ssize_t msm_cpufreq_limit_cpu3_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int cpu = 3;
	return sprintf(buf, "%u\n", get_max_lock(cpu));
}

static ssize_t msm_cpufreq_limit_cpu3_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int cpu = 3;
	unsigned int val;
	int ret;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;

	if (val < 300000 || val > 2803200)
		val = 0;

	set_max_lock(cpu, val);

	return count;
}

static ssize_t msm_cpufreq_limit_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u.%u\n",
			MSM_CPUFREQ_LIMIT_MAJOR, MSM_CPUFREQ_LIMIT_MINOR);
}

static struct kobj_attribute msm_cpufreq_limit_cpu0_attribute =
	__ATTR(cpufreq_limit_cpu0, 0666,
		msm_cpufreq_limit_cpu0_show,
		msm_cpufreq_limit_cpu0_store);

static struct kobj_attribute msm_cpufreq_limit_cpu1_attribute =
	__ATTR(cpufreq_limit_cpu1, 0666,
		msm_cpufreq_limit_cpu1_show,
		msm_cpufreq_limit_cpu1_store);

static struct kobj_attribute msm_cpufreq_limit_cpu2_attribute =
	__ATTR(cpufreq_limit_cpu2, 0666,
		msm_cpufreq_limit_cpu2_show,
		msm_cpufreq_limit_cpu2_store);

static struct kobj_attribute msm_cpufreq_limit_cpu3_attribute =
	__ATTR(cpufreq_limit_cpu3, 0666,
		msm_cpufreq_limit_cpu3_show,
		msm_cpufreq_limit_cpu3_store);

static struct kobj_attribute msm_cpufreq_limit_version_attribute =
	__ATTR(msm_cpufreq_limit_version, 0444,
		msm_cpufreq_limit_version_show,
		NULL);

static struct kobj_attribute suspend_defer_time_attribute =
	__ATTR(suspend_defer_time, 0666,
		suspend_defer_time_show,
		suspend_defer_time_store);

static struct kobj_attribute suspend_max_freq_attribute =
	__ATTR(suspend_max_freq, 0666,
		suspend_max_freq_show,
		suspend_max_freq_store);

static struct attribute *msm_cpufreq_limit_attrs[] =
	{
		&msm_cpufreq_limit_cpu0_attribute.attr,
		&msm_cpufreq_limit_cpu1_attribute.attr,
		&msm_cpufreq_limit_cpu2_attribute.attr,
		&msm_cpufreq_limit_cpu3_attribute.attr,
		&msm_cpufreq_limit_version_attribute.attr,
		&suspend_defer_time_attribute.attr,
		&suspend_max_freq_attribute.attr,
		NULL,
	};

static struct attribute_group msm_cpufreq_limit_attr_group =
	{
		.attrs = msm_cpufreq_limit_attrs,
	};

static struct kobject *msm_cpufreq_limit_kobj;

static int msm_cpufreq_limit_init(void)
{
	int ret;

	msm_cpufreq_limit_kobj =
		kobject_create_and_add(MSM_LIMIT, kernel_kobj);
	if (!msm_cpufreq_limit_kobj) {
		pr_err("%s msm_cpufreq_limit_kobj kobject create failed!\n",
			__func__);
		return -ENOMEM;
        }

	ret = sysfs_create_group(msm_cpufreq_limit_kobj,
			&msm_cpufreq_limit_attr_group);

        if (ret) {
		pr_err("%s msm_cpufreq_limit_kobj create failed!\n",
			__func__);
		goto err_dev;
	}

	msm_cpufreq_limit_start();

	return ret;
err_dev:
	if (msm_cpufreq_limit_kobj != NULL)
		kobject_put(msm_cpufreq_limit_kobj);
	return ret;
}

static void msm_cpufreq_limit_exit(void)
{
	if (msm_cpufreq_limit_kobj != NULL)
		kobject_put(msm_cpufreq_limit_kobj);

	msm_cpufreq_limit_stop();
}

module_init(msm_cpufreq_limit_init);
module_exit(msm_cpufreq_limit_exit);

MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>, \
		Dorimanx <yuri@bynet.co.il>, \
		Pranav Vashi <neobuddy89@gmail.com>");
MODULE_DESCRIPTION("MSM Krait CPU Frequency Limiter Driver");
MODULE_LICENSE("GPL v2");
