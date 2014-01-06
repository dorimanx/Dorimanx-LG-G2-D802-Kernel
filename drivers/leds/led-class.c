/*
 * LED Class Core
 *
 * Copyright (C) 2005 John Lenz <lenz@cs.wisc.edu>
 * Copyright (C) 2005-2007 Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include "leds.h"
#if defined(CONFIG_MACH_LGE)
#include <mach/board_lge.h>
#endif

#define LED_BUFF_SIZE 50

static struct class *leds_class;
#ifdef CONFIG_LEDS_PM8941_EMOTIONAL
extern void make_blink_led_pattern(int rgb, int delay_on, int delay_off);
extern void make_onoff_led_pattern(int rgb);
extern void change_led_pattern(int pattern);
extern void make_input_led_pattern(int patterns[],
			int red_start, int red_length, int red_duty,
			int red_pause, int green_start, int green_length,
			int green_duty,	int green_pause, int blue_start,
			int blue_length, int blue_duty, int blue_pause,
			int red_flag, int green_flag, int blue_flag,
			int period);
extern void set_kpdbl_pattern (int pattern);
static int onoff_rgb = 0;
#endif
static void led_update_brightness(struct led_classdev *led_cdev)
{
	if (led_cdev->brightness_get)
		led_cdev->brightness = led_cdev->brightness_get(led_cdev);
}

static ssize_t led_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	/* no lock needed for this */
	led_update_brightness(led_cdev);

	return snprintf(buf, LED_BUFF_SIZE, "%u\n", led_cdev->brightness);
}

static ssize_t led_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

		if (state == LED_OFF)
			led_trigger_remove(led_cdev);
		led_set_brightness(led_cdev, state);
	}

	return ret;
}

static ssize_t led_max_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	unsigned long state = 0;

	ret = strict_strtoul(buf, 10, &state);
	if (!ret) {
		ret = size;
		if (state > LED_FULL)
			state = LED_FULL;
		led_cdev->max_brightness = state;
		led_set_brightness(led_cdev, led_cdev->brightness);
	}

	return ret;
}

static ssize_t led_max_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return snprintf(buf, LED_BUFF_SIZE, "%u\n", led_cdev->max_brightness);
}

#ifdef CONFIG_MACH_MSM8974_G2_VZW
static int lge_thm_status = 0;
static ssize_t thermald_status_show(struct device *dev,
		                struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n",lge_thm_status);
}

static ssize_t thermald_status_store(struct device *dev,
		                struct device_attribute *attr, const char *buf, size_t size)
{
        struct led_classdev *led_cdev = dev_get_drvdata(dev);
        unsigned long state = 0;
        int rc = 1;

	if (strncmp(buf, "0", 1) == 0)
		lge_thm_status = 0;
	else if (strncmp(buf, "1", 1) == 0) {
		state = LED_FULL;
		led_cdev->max_brightness = state;
		led_set_brightness(led_cdev, led_cdev->brightness);

		lge_thm_status = 1;
	}
	return rc;
}
#endif

static struct device_attribute led_class_attrs[] = {
	__ATTR(brightness, 0644, led_brightness_show, led_brightness_store),
	__ATTR(max_brightness, 0644, led_max_brightness_show,
			led_max_brightness_store),
#ifdef CONFIG_MACH_MSM8974_G2_VZW
	__ATTR(thermald_status, 0644, thermald_status_show, thermald_status_store),
#endif
#ifdef CONFIG_LEDS_TRIGGERS
	__ATTR(trigger, 0644, led_trigger_show, led_trigger_store),
#endif
	__ATTR_NULL,
};

static void led_timer_function(unsigned long data)
{
	struct led_classdev *led_cdev = (void *)data;
	unsigned long brightness;
	unsigned long delay;

	if (!led_cdev->blink_delay_on || !led_cdev->blink_delay_off) {
		led_set_brightness(led_cdev, LED_OFF);
		return;
	}

	brightness = led_get_brightness(led_cdev);
	if (!brightness) {
		/* Time to switch the LED on. */
		brightness = led_cdev->blink_brightness;
		delay = led_cdev->blink_delay_on;
	} else {
		/* Store the current brightness value to be able
		 * to restore it when the delay_off period is over.
		 */
		led_cdev->blink_brightness = brightness;
		brightness = LED_OFF;
		delay = led_cdev->blink_delay_off;
	}

	led_set_brightness(led_cdev, brightness);

	mod_timer(&led_cdev->blink_timer, jiffies + msecs_to_jiffies(delay));
}

/**
 * led_classdev_suspend - suspend an led_classdev.
 * @led_cdev: the led_classdev to suspend.
 */
void led_classdev_suspend(struct led_classdev *led_cdev)
{
	led_cdev->flags |= LED_SUSPENDED;
	led_cdev->brightness_set(led_cdev, 0);
}
EXPORT_SYMBOL_GPL(led_classdev_suspend);

/**
 * led_classdev_resume - resume an led_classdev.
 * @led_cdev: the led_classdev to resume.
 */
void led_classdev_resume(struct led_classdev *led_cdev)
{
	led_cdev->brightness_set(led_cdev, led_cdev->brightness);
	led_cdev->flags &= ~LED_SUSPENDED;
}
EXPORT_SYMBOL_GPL(led_classdev_resume);

static int led_suspend(struct device *dev, pm_message_t state)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (led_cdev->flags & LED_CORE_SUSPENDRESUME)
		led_classdev_suspend(led_cdev);

	return 0;
}

static int led_resume(struct device *dev)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (led_cdev->flags & LED_CORE_SUSPENDRESUME)
		led_classdev_resume(led_cdev);

	return 0;
}

/**
 * led_classdev_register - register a new object of led_classdev class.
 * @parent: The device to register.
 * @led_cdev: the led_classdev structure for this device.
 */
int led_classdev_register(struct device *parent, struct led_classdev *led_cdev)
{
	led_cdev->dev = device_create(leds_class, parent, 0, led_cdev,
				      "%s", led_cdev->name);
	if (IS_ERR(led_cdev->dev))
		return PTR_ERR(led_cdev->dev);

#ifdef CONFIG_LEDS_TRIGGERS
	init_rwsem(&led_cdev->trigger_lock);
#endif
	/* add to the list of leds */
	down_write(&leds_list_lock);
	list_add_tail(&led_cdev->node, &leds_list);
	up_write(&leds_list_lock);

	if (!led_cdev->max_brightness)
		led_cdev->max_brightness = LED_FULL;

	led_update_brightness(led_cdev);

	init_timer(&led_cdev->blink_timer);
	led_cdev->blink_timer.function = led_timer_function;
	led_cdev->blink_timer.data = (unsigned long)led_cdev;

#ifdef CONFIG_LEDS_TRIGGERS
	led_trigger_set_default(led_cdev);
#endif

	printk(KERN_DEBUG "Registered led device: %s\n",
			led_cdev->name);

	return 0;
}
EXPORT_SYMBOL_GPL(led_classdev_register);

#ifdef CONFIG_LEDS_PM8941_EMOTIONAL
static ssize_t get_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
//	count = sprintf(buf,"%d %d\n", pattern_num,pattern_on );

	return count;
}

static ssize_t set_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;
	int pattern_num = 0;

	if (sscanf(buf, "%d", &pattern_num) != 1) {
		printk("[RGB LED] bad arguments ");
	}
	ret = size;

	if(lge_get_boot_mode() <= LGE_BOOT_MODE_CHARGERLOGO) {
		printk("[RGB LED] pattern_num=%d\n", pattern_num);
		if (!pattern_num || pattern_num == 35 || pattern_num == 36 || pattern_num == 1035)
			set_kpdbl_pattern(pattern_num);
		if ((pattern_num != 35)&&(pattern_num != 36))
			change_led_pattern(pattern_num);
	}

	return ret;
}

static DEVICE_ATTR(setting, 0644, get_pattern, set_pattern);

static ssize_t get_input_pattern(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int count = 0;
	return count;
}

static ssize_t set_input_pattern(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;
	int patterns[63];

	int red_start_idx;
	int red_length;
	int red_duty;
	int red_pause_lo;

	int green_start_idx;
	int green_length;
	int green_duty;
	int green_pause_lo;

	int blue_start_idx;
	int blue_length;
	int blue_duty;
	int blue_pause_lo;

	int red_flag;
	int green_flag;
	int blue_flag;
	int period;

	int i = 0;

	if (sscanf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			&patterns[0], &patterns[1], &patterns[2],
			&patterns[3], &patterns[4], &patterns[5],
			&patterns[6], &patterns[7],	&patterns[8],
			&patterns[9], &patterns[10], &patterns[11],
			&patterns[12], &patterns[13], &patterns[14],
			&patterns[15], &patterns[16], &patterns[17],
			&patterns[18], &patterns[19], &patterns[20],
			&patterns[21], &patterns[22], &patterns[23],
			&patterns[24], &patterns[25], &patterns[26],
			&patterns[27], &patterns[28], &patterns[29],
			&patterns[30], &patterns[31], &patterns[32],
			&patterns[33], &patterns[34], &patterns[35],
			&patterns[36], &patterns[37], &patterns[38],
			&patterns[39], &patterns[40], &patterns[41],
			&patterns[42], &patterns[43], &patterns[44],
			&patterns[45], &patterns[46], &patterns[47],
			&patterns[48], &patterns[49], &patterns[50],
			&patterns[51], &patterns[52], &patterns[53],
			&patterns[54], &patterns[55], &patterns[56],
			&patterns[57], &patterns[58], &patterns[59],
			&patterns[60], &patterns[61], &patterns[62],
			&red_start_idx, &red_length, &red_duty,
			&red_pause_lo, &green_start_idx, &green_length,
			&green_duty, &green_pause_lo, &blue_start_idx,
			&blue_length,  &blue_duty,  &blue_pause_lo,
			&red_flag, &green_flag, &blue_flag, &period) != 79){
			printk(KERN_INFO "[RGB LED] bad arguments ");
		}
	ret = size;

	printk("[RGB LED] LUT is \n");
	for (i = 0; i < ARRAY_SIZE(patterns); i++)
		printk(KERN_INFO "%d ", patterns[i]);
	printk("\n");
	printk(KERN_INFO "[RGB LED] [RED] duty_ms:%d, pause_lo:%d, start:%d, length:%d \n",
		   red_duty, red_pause_lo, red_start_idx, red_length);
	printk(KERN_INFO "[RGB LED] [GREEN] duty_ms:%d, pause_lo:%d, start:%d, length:%d \n",
		   green_duty, green_pause_lo, green_start_idx, green_length);
	printk(KERN_INFO "[RGB LED] [BLUE] duty_ms:%d, pause_lo:%d, start:%d, length:%d \n",
		   blue_duty, blue_pause_lo, blue_start_idx, blue_length);
	printk(KERN_INFO "[RGB LED] [FLAG] red:%d, green:%d, blue:%d\n",
		   red_flag, green_flag, blue_flag);
	make_input_led_pattern((int *)&patterns, red_start_idx, red_length,
			red_duty, red_pause_lo, green_start_idx, green_length,
			green_duty, green_pause_lo, blue_start_idx, blue_length,
			blue_duty, blue_pause_lo, red_flag, green_flag,
			blue_flag, period);

	return ret;
}

static DEVICE_ATTR(input_patterns, 0644, get_input_pattern, set_input_pattern);

static ssize_t confirm_blink_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;

//	count = sprintf(buf,"%d %d\n", pattern_num,pattern_on );

	return count;
}


static ssize_t make_blink_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{

	ssize_t ret = -EINVAL;
	int rgb;
	int delay_on;
	int delay_off;

	if (sscanf(buf, "0x%06x,%d,%d", &rgb, &delay_on, &delay_off) != 3)
		printk("[RGB LED] make_blink_pattern() bad arguments ");

	ret = size;

	printk("[RGB LED] make_blink_pattern rgb is %06x\n", rgb);
	printk("[RGB LED] make_blink_pattern delay_on is %d\n", delay_on);
	printk("[RGB LED] make_blink_pattern delay_off %d\n", delay_off);
	make_blink_led_pattern(rgb, delay_on, delay_off);

	return ret;
}

static DEVICE_ATTR(blink_patterns, 0644, confirm_blink_pattern, make_blink_pattern);

static ssize_t confirm_onoff_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, LED_BUFF_SIZE, "0x%06x\n", onoff_rgb);
}

static ssize_t make_onoff_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{

	ssize_t ret = -EINVAL;

	if (sscanf(buf, "0x%06x", &onoff_rgb) != 1)
		printk("[RGB LED] make_onoff_pattern() bad arguments ");

	ret = size;

	//printk("[RGB LED] make_onoff_rgb is %06x\n",rgb);
	make_onoff_led_pattern(onoff_rgb);

	return ret;
}

static DEVICE_ATTR(onoff_patterns, 0644, confirm_onoff_pattern, make_onoff_pattern);


int led_pattern_sysfs_register(void)
{
	struct class *g2_rgb;
	struct device *pattern_sysfs_dev;
	g2_rgb = class_create(THIS_MODULE, "g2_rgb_led");
	if (IS_ERR(g2_rgb)) {
		printk("Failed to create class(g2_rgb_led)!\n");
	}
	pattern_sysfs_dev = device_create(g2_rgb, NULL, 0, NULL, "use_patterns");
	if (IS_ERR(pattern_sysfs_dev))
		return PTR_ERR(pattern_sysfs_dev);

	if (device_create_file(pattern_sysfs_dev, &dev_attr_setting) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_setting.attr.name);

	if (device_create_file(pattern_sysfs_dev, &dev_attr_blink_patterns) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_blink_patterns.attr.name);

	if (device_create_file(pattern_sysfs_dev, &dev_attr_input_patterns) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_input_patterns.attr.name);

	if (device_create_file(pattern_sysfs_dev, &dev_attr_onoff_patterns) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_onoff_patterns.attr.name);

	return 0;
}
EXPORT_SYMBOL_GPL(led_pattern_sysfs_register);
#endif

/**
 * led_classdev_unregister - unregisters a object of led_properties class.
 * @led_cdev: the led device to unregister
 *
 * Unregisters a previously registered via led_classdev_register object.
 */
void led_classdev_unregister(struct led_classdev *led_cdev)
{
#ifdef CONFIG_LEDS_TRIGGERS
	down_write(&led_cdev->trigger_lock);
	if (led_cdev->trigger)
		led_trigger_set(led_cdev, NULL);
	up_write(&led_cdev->trigger_lock);
#endif

	/* Stop blinking */
	led_brightness_set(led_cdev, LED_OFF);

	device_unregister(led_cdev->dev);

	down_write(&leds_list_lock);
	list_del(&led_cdev->node);
	up_write(&leds_list_lock);
}
EXPORT_SYMBOL_GPL(led_classdev_unregister);

static int __init leds_init(void)
{
	leds_class = class_create(THIS_MODULE, "leds");
	if (IS_ERR(leds_class))
		return PTR_ERR(leds_class);
	leds_class->suspend = led_suspend;
	leds_class->resume = led_resume;
	leds_class->dev_attrs = led_class_attrs;
#ifdef CONFIG_LEDS_PM8941_EMOTIONAL
	led_pattern_sysfs_register();
#endif
	return 0;
}

static void __exit leds_exit(void)
{
	class_destroy(leds_class);
}

subsys_initcall(leds_init);
module_exit(leds_exit);

MODULE_AUTHOR("John Lenz, Richard Purdie");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED Class Interface");
