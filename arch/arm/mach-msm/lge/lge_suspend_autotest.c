/*
 * kernel/power/suspend_autotest.c -
 * Suspend to RAM and standby autotest facility.
 *
 * Copyright (c) 2011 LGE, Inc.
 * SungEun Kim <cleaneye.kim@lge.com>
 *
 * base idea of code is from kernel/power/suspend_test.c
 *
 * This file is released under the GPLv2.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/android_alarm.h>
#include <linux/suspend.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>

#define MODULE_NAME "suspend_autotest"

#define TEST_SUSPEND_SECONDS    10
#define TEST_EARLYSUSPEND_SECONDS    1
#define TEST_POWEROFF_SECONDS 	3

static unsigned long suspend_test_start_time;
static struct input_dev *pwr_dev;
static int test_alarm_set;
static struct rtc_device *rtc;
struct wake_lock test_wake_lock;

static void earlysuspend_timer_handler(unsigned long data);
static DEFINE_TIMER(suspend_autotest_timer, earlysuspend_timer_handler, 0, 0);

enum {
	NONE = 0,
	SUSPEND,
	EARLYSUSPEND,
	RESUME,
};

static int test_mode;
static int test_enable;
module_param_named(test_enable, test_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int set_test_mode(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_int(val, kp);

	if (rv)
		return rv;

	if (test_mode > EARLYSUSPEND) {
		test_mode = NONE;
		return 0;
	}

	switch (test_mode) {
	case EARLYSUSPEND:
		if (!wake_lock_active(&test_wake_lock))
			wake_lock(&test_wake_lock);
		break;
	case SUSPEND:
	case NONE:
		if (wake_lock_active(&test_wake_lock))
			wake_unlock(&test_wake_lock);
	}

	pr_info("%s: suspend autotest mode is %d\n", __func__, test_mode);

	return 0;
}

static struct kernel_param_ops test_mode_param_ops = {
	.set = set_test_mode,
	.get = param_get_int,
};

module_param_cb(test_mode, &test_mode_param_ops, &test_mode, 0644);

static void report_pwr_event(void)
{
	input_report_key(pwr_dev, KEY_POWER, 1);
	input_sync(pwr_dev);
	input_report_key(pwr_dev, KEY_POWER, 0);
	input_sync(pwr_dev);
}

static int suspend_autotest_suspend(struct device *dev)
{
	unsigned long now;
	struct rtc_wkalrm alm;
	int status;
	int need_test = 0;
	unsigned long alarm_time;

	/* if test isn't enabled, no need to proceed below code */
	if ((!test_enable) || (test_mode != SUSPEND))
		return 0;

	/* read alarm time */
	status = rtc_read_alarm(rtc, &alm);
	if (status) {
		pr_info("%s: rtc_read_alarm fail\n", __func__);
		return status;
	}

	rtc_tm_to_time(&alm.time, &alarm_time);
	/* if alarm is set already, alarm time should be compared.
	 * if no alarm is set, test alarm can be done.
	 */
	if (alm.enabled)
		need_test = 0;
	else
		need_test = 1;

	status = rtc_read_time(rtc, &alm.time);
	if (status < 0) {
		pr_info("%s: rtc_read_time fail\n", __func__);
		return status;
	}
	rtc_tm_to_time(&alm.time, &now);

	/* if alarm will be expired in TEST_SUSPEND_SECONDS,
	 * don't set test alarm time
	 */
	if (!need_test && alarm_time < (now + TEST_SUSPEND_SECONDS)) {
		pr_info("%s: no setting of test alarm\n", __func__);
		return 0;
	}

	memset(&alm, 0, sizeof alm);
	rtc_time_to_tm(now + TEST_SUSPEND_SECONDS, &alm.time);
	alm.enabled = true;

	status = rtc_set_alarm(rtc, &alm);
	if (status < 0) {
		pr_info("%s: rtc_set_alarm fail\n", __func__);
		return status;
	}

	test_alarm_set = 1;
	pr_info("%s: test alarm will be envoked after about %d sec.\n",
			__func__, TEST_SUSPEND_SECONDS);

	return 0;
}

static int suspend_autotest_resume(struct device *dev)
{
	/* if test isn't enabled, no need to proceed below code */
	if (!test_enable)
		return 0;

	/* check if suspend is envoked by rtc alarm event */
	if (wake_lock_active_name("alarm_rtc") && test_alarm_set) {
		pr_info("%s: test alarm expired and report power event\n",
				__func__);
		report_pwr_event();
	}

	return 0;
}

void suspend_test_start(void)
{
	/* FIXME Use better timebase than "jiffies", ideally a clocksource.
	 * What we want is a hardware counter that will work correctly even
	 * during the irqs-are-off stages of the suspend/resume cycle...
	 */
	suspend_test_start_time = jiffies;
}

void suspend_test_finish(const char *label)
{
	long nj = jiffies - suspend_test_start_time;
	unsigned msec;

	msec = jiffies_to_msecs(abs(nj));
	pr_info("PM: %s took %d.%03d seconds\n", label,
			msec / 1000, msec % 1000);

	/* Warning on suspend means the timer expiring period needs to be
	 * larger -- the system was sooo slooowwww to suspend that the
	 * timer (should have) fired before the system went to sleep!
	 *
	 * Warning on either suspend or resume also means the system
	 * has some performance issues.  The stack dump of a WARN_ON
	 * is more likely to get the right attention than a printk...
	 */
	WARN(msec > (TEST_SUSPEND_SECONDS * 1000),
			"Component: %s, time: %u\n", label, msec);
}

static int __devinit has_wakealarm(struct device *dev, void *name_ptr)
{
	struct rtc_device *candidate = to_rtc_device(dev);

	if (!candidate->ops->set_alarm)
		return 0;
	if (!device_may_wakeup(candidate->dev.parent))
		return 0;

	*(const char **)name_ptr = dev_name(dev);
	return 1;
}

static void earlysuspend_timer_handler(unsigned long data)
{
	if (test_mode != NONE)
		report_pwr_event();

	return;
}

static void suspend_autotest_early_suspend(struct early_suspend *handler)
{
	/* if test isn't enabled, no need to proceed below code */
	if (!test_enable)
		return;

	if (test_mode == EARLYSUSPEND) {
		mod_timer(&suspend_autotest_timer,
				jiffies + (HZ * TEST_EARLYSUSPEND_SECONDS));
		pr_info("%s: test wakeup power event will be passed after about %d sec.\n",
				__func__, TEST_EARLYSUSPEND_SECONDS);
	}
}

static void suspend_autotest_late_resume(struct early_suspend *handler)
{
	/* if test isn't enabled, no need to proceed below code */
	if (!test_enable)
		return;

	switch (test_mode) {
	case EARLYSUSPEND:
		del_timer(&suspend_autotest_timer);
	case SUSPEND:
		mod_timer(&suspend_autotest_timer, jiffies +
					(HZ * TEST_POWEROFF_SECONDS));
		pr_info("%s: test sleep power event will be passed after about %d sec.\n",
				__func__, TEST_POWEROFF_SECONDS);
	}
}

static struct early_suspend suspend_autotest_earlysuspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = suspend_autotest_early_suspend,
	.resume = suspend_autotest_late_resume,
};

static int __devinit suspend_autotest_probe(struct platform_device *pdev)
{
	int err;
	char *pony = NULL;

	pwr_dev = input_allocate_device();
	if (!pwr_dev) {
		pr_err("Can't allocate suspend autotest power button\n");
		err = -ENOMEM;
		goto err_alloc_dev;
	}

	input_set_capability(pwr_dev, EV_KEY, KEY_POWER);
	pwr_dev->name = "suspend_autotest_pwrkey";
	pwr_dev->phys = "suspend_autotest_pwrkey/input0";

	err = input_register_device(pwr_dev);
	if (err) {
		pr_err("%s: input_register_device err=%d\n", __func__, err);
		goto err_input_dev;
	}

	/* RTCs have initialized by now too ... can we use one? */
	class_find_device(rtc_class, NULL, &pony, has_wakealarm);
	if (pony)
		rtc = rtc_class_open(pony);
	if (!rtc) {
		pr_info("%s: rtc class fail\n", __func__);
		goto err_input_dev;
	}

	wake_lock_init(&test_wake_lock, WAKE_LOCK_SUSPEND, "suspend_autotest");
	register_early_suspend(&suspend_autotest_earlysuspend);

	test_mode = NONE;

	return 0;

err_input_dev:
	input_free_device(pwr_dev);
err_alloc_dev:
	return err;
}

static int __devexit suspend_autotest_remove(struct platform_device *pdev)
{
	if (wake_lock_active(&test_wake_lock))
		wake_unlock(&test_wake_lock);
	wake_lock_destroy(&test_wake_lock);

	rtc_class_close(rtc);
	input_unregister_device(pwr_dev);
	input_free_device(pwr_dev);

	return 0;
}

static const struct dev_pm_ops suspend_autotest_dev_pm_ops = {
	.suspend_noirq = suspend_autotest_suspend,
	.resume = suspend_autotest_resume,
};

static struct platform_driver suspend_autotest_driver = {
	.probe = suspend_autotest_probe,
	.remove = suspend_autotest_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &suspend_autotest_dev_pm_ops,
	},
};

static struct platform_device suspend_autotest_device = {
	.name = MODULE_NAME,
	.dev = {
		.platform_data = NULL,
	}
};

static int __init suspend_autotest_init(void)
{
	test_mode = SUSPEND;

	platform_device_register(&suspend_autotest_device);

    return platform_driver_register(&suspend_autotest_driver);
}

static void __exit suspend_autotest_exit(void)
{
	return;
}

late_initcall(suspend_autotest_init);
module_exit(suspend_autotest_exit);

MODULE_DESCRIPTION("Suspend/Resume Auto-Test driver");
MODULE_LICENSE("GPL v2");
