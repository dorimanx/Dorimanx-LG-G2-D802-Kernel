/*
 *  lge/com_device/input/max1462x.c
 *
 *  LGE 3.5 PI Headset detection driver using max1462x.
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * Copyright (C) 2009 ~ 2010 LGE, Inc.
 * Author: Lee SungYoung <lsy@lge.com>
 *
 * Copyright (C) 2010 LGE, Inc.
 * Author: Kim Eun Hye <ehgrace.kim@lge.com>
 *
 * Copyright (C) 2011 LGE, Inc.
 * Author: Yoon Gi Souk <gisouk.yoon@lge.com>
 *
 * Copyright (C) 2012 LGE, Inc.
 * Author: Park Gyu Hwa <gyuhwa.park@lge.com>
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

/* Interface is following;
 * source file is
 * android/frameworks/base/services/java/com/android/server/HeadsetObserver.java
 * HEADSET_UEVENT_MATCH = "DEVPATH=/sys/devices/virtual/switch/h2w"
 * HEADSET_STATE_PATH = /sys/class/switch/h2w/state
 * HEADSET_NAME_PATH = /sys/class/switch/h2w/name
 */

#ifdef CONFIG_SWITCH_MAX1462X
#define CONFIG_MAX1462X_USE_LOCAL_WORK_QUEUE
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <linux/platform_data/hds_max1462x.h>
#include <linux/jiffies.h>
#include <linux/spmi.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/qpnp/qpnp-adc.h>
#include <mach/gpiomux.h>

#undef  LGE_HSD_DEBUG_PRINT /*TODO*/
#define LGE_HSD_DEBUG_PRINT /*TODO*/
#undef  LGE_HSD_ERROR_PRINT
#define LGE_HSD_ERROR_PRINT

#define HOOK_MIN		0
#define HOOK_MAX		150000
#define VUP_MIN			150000
#define VUP_MAX			400000
#define VDOWN_MIN		400000
#define VDOWN_MAX		600000

/* TODO */
/* 1. coding for additional excetion case in probe function */
/* 2. additional sleep related/excetional case  */

#if defined(LGE_HSD_DEBUG_PRINT)
#define HSD_DBG(fmt, args...) printk(KERN_INFO "HSD.max1462x[%-18s:%5d]" fmt, __func__, __LINE__, ## args)
#else
#define HSD_DBG(fmt, args...) do {} while (0)
#endif

#if defined(LGE_HSD_ERROR_PRINT)
#define HSD_ERR(fmt, args...) printk(KERN_ERR "HSD.max1462x[%-18s:%5d]" fmt, __func__, __LINE__, ## args)
#else
#define HSD_ERR(fmt, args...) do { } while (0)
#endif

#ifdef CONFIG_MAX1462X_USE_LOCAL_WORK_QUEUE
static struct workqueue_struct *local_max1462x_workqueue;
#endif

static struct wake_lock ear_hook_wake_lock;

struct ear_3button_info_table {
	unsigned int ADC_HEADSET_BUTTON;
	int PERMISS_REANGE_MAX;
	int PERMISS_REANGE_MIN;
	int PRESS_OR_NOT;
};

/* This table is only for J1 */
static struct ear_3button_info_table max1462x_ear_3button_type_data[]={
	{KEY_MEDIA, HOOK_MAX, HOOK_MIN, 0},
	{KEY_VOLUMEUP, VUP_MAX, VUP_MIN, 0},
	{KEY_VOLUMEDOWN, VDOWN_MAX, VDOWN_MIN, 0}
};

struct hsd_info {
	/* function devices provided by this driver */
	struct switch_dev sdev;
	struct input_dev *input;

	/* mutex */
	struct mutex mutex_lock;

	/* h/w configuration : initilized by platform data */

	/* MODE : high, low, high-z */
	unsigned int gpio_mic_en;

	/* SWD : to detect 3pole or 4pole to detect among hook,
	 * volum up or down key */
	unsigned int gpio_key;

	/* DET : to detect jack inserted or not */
	unsigned int gpio_detect;

	/* callback function which is initialized while probing */
	void (*gpio_set_value_func)(unsigned gpio, int value);
	int (*gpio_get_value_func)(unsigned gpio);

	unsigned int latency_for_key;

	unsigned int key_code;	/* KEY_MEDIA, KEY_VOLUMEUP or KEY_VOLUMEDOWN */

	/* irqs */
	unsigned int irq_detect;	/* detect */
	unsigned int irq_key;		/* key */

	/* internal states */
	atomic_t irq_key_enabled;
	atomic_t is_3_pole_or_not;
	atomic_t btn_state;
	atomic_t isdetect;

	/* work for detect_work */
	struct work_struct work;
	struct delayed_work work_for_key_pressed;
	struct delayed_work work_for_key_released;

	unsigned char *pdev_name;
};

enum {
	NO_DEVICE   = 0,
	LGE_HEADSET = (1 << 0),
	LGE_HEADSET_NO_MIC = (1 << 1),
};

enum {
	FALSE = 0,
	TRUE = 1,
};

static ssize_t lge_hsd_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
		case NO_DEVICE:
			return sprintf(buf, "No Device");
		case LGE_HEADSET:
			return sprintf(buf, "Headset");
		case LGE_HEADSET_NO_MIC:
			return sprintf(buf, "Headset");
		default:
			break;
	}
	return -EINVAL;
}

static ssize_t lge_hsd_print_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(sdev));
}

static void spmi_write(u8 value)
{
	struct spmi_controller *ctrl = spmi_busnum_to_ctrl(0);
	int ret = 0;
	u16 addr = 0x0000;
	u8 write_buf = 0x0;
	u8 read_buf = 0x0;
	addr = 0xdc46;
	write_buf = value;

	if (ctrl == NULL) {
		HSD_ERR("spmi_write: spmi_controller is NULL!\n");
		return;
	}
	ret = spmi_ext_register_writel(ctrl, 0, addr, &write_buf,1);
	spmi_ext_register_readl(ctrl, 0, addr,&read_buf,1);
	HSD_DBG("addr:%x,write_buf:%x,read_buf:%x,ret:%d\n",addr, write_buf,read_buf,ret);
}

static void button_pressed(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct hsd_info *hi = container_of(dwork, struct hsd_info, work_for_key_pressed);
	struct qpnp_vadc_result result;
	int acc_read_value = 0;
	int i, rc;
	struct ear_3button_info_table *table;
	int table_size = ARRAY_SIZE(max1462x_ear_3button_type_data);

	if (hi->gpio_get_value_func(hi->gpio_detect)||(atomic_read(&hi->is_3_pole_or_not)))
	{
		HSD_ERR("button_pressed but 4 pole ear jack is plugged out already! just ignore the event.\n");
		return;
	}
	rc = qpnp_vadc_read(P_MUX6_1_1,&result);

	if (rc < 0) {
		if (rc == -ETIMEDOUT) {
			pr_err("[DEBUG] button_pressed : adc read timeout \n");
		} else {
			pr_err("button_pressed: adc read error - %d\n", rc);
		}
	}
	acc_read_value = (int)result.physical;
	pr_info("%s: acc_read_value - %d\n", __func__, acc_read_value);

	for (i = 0; i < table_size; i++) {
		table = &max1462x_ear_3button_type_data[i];
		/* [AUDIO_BSP] 20130110, junday.lee,
		 * include min value '=' for 1 button earjack (ADC value= 0)
		 */
		if ((acc_read_value <= table->PERMISS_REANGE_MAX) &&
				(acc_read_value >= table->PERMISS_REANGE_MIN)) {
			HSD_DBG("button_pressed \n");
			atomic_set(&hi->btn_state, 1);
			switch(table->ADC_HEADSET_BUTTON){
				case  KEY_MEDIA :
					input_report_key(hi->input, KEY_MEDIA, 1);
					pr_info("%s: KEY_MEDIA \n", __func__);
					break;
				case KEY_VOLUMEUP :
					input_report_key(hi->input, KEY_VOLUMEUP, 1);
					pr_info("%s: KEY_VOLUMEUP \n", __func__);
					break;
				case KEY_VOLUMEDOWN :
					input_report_key(hi->input, KEY_VOLUMEDOWN, 1);
					pr_info("%s: KEY_VOLUMDOWN \n", __func__);
					break;
				default:
					break;
			}
			table->PRESS_OR_NOT = 1;
			input_sync(hi->input);
			break;
		}
	}
}

static void button_released(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct hsd_info *hi = container_of(dwork, struct hsd_info, work_for_key_released);
	struct ear_3button_info_table *table;
	int table_size = ARRAY_SIZE(max1462x_ear_3button_type_data);
	int i;

       //
       if (hi->gpio_get_value_func(hi->gpio_detect) && !atomic_read(&hi->btn_state)){
		HSD_ERR("button_released but ear jack is plugged out already! just ignore the event.\n");
		return;
	}

	HSD_DBG("button_released \n");
	for (i = 0; i < table_size; i++) {
		table = &max1462x_ear_3button_type_data[i];
		if (table->PRESS_OR_NOT) {
			atomic_set(&hi->btn_state, 0);
			switch(table->ADC_HEADSET_BUTTON){
				case  KEY_MEDIA :
					input_report_key(hi->input, KEY_MEDIA, 0);
					break;
				case KEY_VOLUMEUP :
					input_report_key(hi->input, KEY_VOLUMEUP, 0);
					break;
				case KEY_VOLUMEDOWN :
					input_report_key(hi->input, KEY_VOLUMEDOWN, 0);
					break;
				default:
					break;
			}
			table->PRESS_OR_NOT = 0;
			input_sync(hi->input);
			break;
		}
	}
}

static void insert_headset(struct hsd_info *hi)
{
	int earjack_type;

	HSD_DBG("insert_headset\n");
	if(atomic_read(&hi->isdetect))
	{
		HSD_DBG("duplicate irq\n");
		return;
	}

	atomic_set(&hi->isdetect,TRUE);
	irq_set_irq_wake(hi->irq_key, 1);
	gpio_direction_output(hi->gpio_mic_en, 1);
#ifdef CONFIG_SWITCH_MAX1462X_WA
	#if defined (CONFIG_MACH_MSM8974_G2_TMO_US) ||defined (CONFIG_MACH_MSM8974_G2_SPR) || defined (CONFIG_MACH_MSM8974_G2_OPEN_COM)
		msleep(600);
		HSD_DBG("insert delay 600\n");
	#else
		msleep(500);
		HSD_DBG("insert delay 500\n");
	#endif
#else
	msleep(40);
	HSD_DBG("insert delay 40\n");
#endif
	/* check if 3-pole or 4-pole
	   1. read gpio_key
	   2. check if 3-pole or 4-pole
	   3-1. NOT regiter irq with gpio_key if 3-pole. complete.
	   3-2. regiter irq with gpio_key if 4-pole
	   4. read MPP6 and decide a pressed key when interrupt occurs */

	earjack_type = hi->gpio_get_value_func(hi->gpio_key);

	if ( earjack_type == 1 ) {

		HSD_DBG("4 polarity earjack\n");

		atomic_set(&hi->is_3_pole_or_not, 0);

		mutex_lock(&hi->mutex_lock);
		switch_set_state(&hi->sdev, LGE_HEADSET);
		mutex_unlock(&hi->mutex_lock);

		if (!atomic_read(&hi->irq_key_enabled)) {
			HSD_DBG("irq_key_enabled = FALSE\n");
			atomic_set(&hi->irq_key_enabled, TRUE);
		}

	} else {
		gpio_direction_output(hi->gpio_mic_en, 0);
		spmi_write(0x00);
		HSD_DBG("3 polarity earjack\n");
		atomic_set(&hi->is_3_pole_or_not, 1);

		mutex_lock(&hi->mutex_lock);
		switch_set_state(&hi->sdev, LGE_HEADSET_NO_MIC);
		mutex_unlock(&hi->mutex_lock);

		irq_set_irq_wake(hi->irq_key, 0);
	}
}

static void remove_headset(struct hsd_info *hi)
{

	HSD_DBG("remove_headset\n");
	if(atomic_read(&hi->is_3_pole_or_not) == 1)
		spmi_write(0x80);
	if(atomic_read(&hi->is_3_pole_or_not) == 0)
		gpio_direction_output(hi->gpio_mic_en, 0);

	atomic_set(&hi->is_3_pole_or_not, 1);
	mutex_lock(&hi->mutex_lock);
	switch_set_state(&hi->sdev, NO_DEVICE);
	mutex_unlock(&hi->mutex_lock);

	if (atomic_read(&hi->irq_key_enabled)) {
		atomic_set(&hi->irq_key_enabled, FALSE);
	}

	if (atomic_read(&hi->btn_state))
#ifdef CONFIG_MAX1462X_USE_LOCAL_WORK_QUEUE
		queue_delayed_work(local_max1462x_workqueue,
				&(hi->work_for_key_released), hi->latency_for_key );
#else
	schedule_delayed_work(&(hi->work_for_key_released),
			hi->latency_for_key );
#endif
	atomic_set(&hi->isdetect,FALSE);
}

static void detect_work(struct work_struct *work)
{
	int state;
	struct hsd_info *hi = container_of(work, struct hsd_info, work);

	HSD_DBG("detect_work\n");

	state = hi->gpio_get_value_func(hi->gpio_detect);

	if (state == 1) {
		if (switch_get_state(&hi->sdev) != NO_DEVICE) {
			HSD_DBG("LGE headset removing\n");
			remove_headset(hi);
		} else {
			HSD_DBG("err_invalid_state remove state = %d\n", state);
		}
	} else {

		if (switch_get_state(&hi->sdev) == NO_DEVICE) {
			HSD_DBG("LGE headset inserting\n");
			insert_headset(hi);
		} else {
			HSD_DBG("err_invalid_state insert state = %d\n", state);
		}
	}
}

static irqreturn_t earjack_det_irq_handler(int irq, void *dev_id)
{
	struct hsd_info *hi = (struct hsd_info *) dev_id;

	wake_lock_timeout(&ear_hook_wake_lock, 2 * HZ);

	HSD_DBG("earjack_det_irq_handler\n");

#ifdef CONFIG_MAX1462X_USE_LOCAL_WORK_QUEUE
	queue_work(local_max1462x_workqueue, &(hi->work));
#else
	schedule_work(&(hi->work));
#endif
	return IRQ_HANDLED;
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	struct hsd_info *hi = (struct hsd_info *) dev_id;

	int value;

	wake_lock_timeout(&ear_hook_wake_lock, 2 * HZ);
	HSD_DBG("button_irq_handler\n");

	value = hi->gpio_get_value_func(hi->gpio_key);

	HSD_DBG("hi->gpio_get_value_func(hi->gpio_key) : %d\n", value);

if(atomic_read(&hi->is_3_pole_or_not) == 0)
{
	if (value)
		queue_delayed_work(local_max1462x_workqueue, &(hi->work_for_key_released), hi->latency_for_key );
	else
		queue_delayed_work(local_max1462x_workqueue, &(hi->work_for_key_pressed), hi->latency_for_key );
}
	return IRQ_HANDLED;
}

static void max1462x_parse_dt(struct device *dev, struct max1462x_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->gpio_detect = of_get_named_gpio_flags(np, "max1462x,gpio_detect", 0, NULL);
	pdata->gpio_key = of_get_named_gpio_flags(np, "max1462x,gpio_key", 0, NULL);
	pdata->gpio_mic_en = of_get_named_gpio_flags(np, "max1462x,gpio_mic_en", 0, NULL);
	pdata->key_code = 0;
	pdata->switch_name = "h2w";
	pdata->keypad_name = "hs_detect";
	pdata->gpio_get_value_func = gpio_get_value;
}

static int lge_hsd_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct max1462x_platform_data *pdata = pdev->dev.platform_data;


	struct hsd_info *hi;

	HSD_DBG("lge_hsd_probe\n");

	hi = kzalloc(sizeof(struct hsd_info), GFP_KERNEL);

	if ( hi == NULL) {
		HSD_ERR("Failed to allloate headset per device info\n");
		return -ENOMEM;
	}

	if(pdev->dev.of_node){
		pdata = devm_kzalloc(&pdev->dev,sizeof(struct max1462x_platform_data),GFP_KERNEL);
		if(!pdata){
			HSD_ERR("Failed to allocate memory\n");
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;

		max1462x_parse_dt(&pdev->dev,pdata);
	} else {
		pdata = devm_kzalloc(&pdev->dev,sizeof(struct max1462x_platform_data),GFP_KERNEL);
		if(!pdata){
			HSD_ERR("Failed to allocate memory\n");
			return -ENOMEM;
		}
		else
			pdata = pdev->dev.platform_data;
	}
	hi->key_code = pdata->key_code;

	platform_set_drvdata(pdev, hi);

	atomic_set(&hi->btn_state, 0);
	atomic_set(&hi->is_3_pole_or_not, 1);
	atomic_set(&hi->irq_key_enabled, FALSE);

	hi->gpio_mic_en = pdata->gpio_mic_en;
	hi->gpio_detect = pdata->gpio_detect;
	hi->gpio_key = pdata->gpio_key;
	hi->gpio_set_value_func = pdata->gpio_set_value_func;
	hi->gpio_get_value_func = pdata->gpio_get_value_func;
#ifdef CONFIG_SWITCH_MAX1462X_WA
	hi->latency_for_key = msecs_to_jiffies(80);
#else
	hi->latency_for_key = msecs_to_jiffies(50); /* convert milli to jiffies */
#endif
	mutex_init(&hi->mutex_lock);
	INIT_WORK(&hi->work, detect_work);
	INIT_DELAYED_WORK(&hi->work_for_key_pressed, button_pressed);
	INIT_DELAYED_WORK(&hi->work_for_key_released, button_released);

	ret = gpio_request(hi->gpio_mic_en, "gpio_mic_en");
	if (ret < 0) {
		HSD_ERR("Failed to configure gpio%d (gpio_mic_en) gpio_request\n", hi->gpio_mic_en);
		goto error_02;
	}

	ret = gpio_direction_output(hi->gpio_mic_en, 0);
	if (ret < 0) {
		HSD_ERR("Failed to configure gpio%d (gpio_mic_en) gpio_direction_input\n", hi->gpio_mic_en);
		goto error_02;
	}
	HSD_DBG("gpio_get_value_cansleep(hi->gpio_mic_en) = %d\n", gpio_get_value_cansleep(hi->gpio_mic_en));

	/* init gpio_detect */
	ret = gpio_request(hi->gpio_detect, "gpio_detect");
	if (ret < 0) {
		HSD_ERR("Failed to configure gpio%d (gpio_det) gpio_request\n", hi->gpio_detect);
		goto error_03;
	}

	ret = gpio_direction_input(hi->gpio_detect);
	if (ret < 0) {
		HSD_ERR("Failed to configure gpio%d (gpio_det) gpio_direction_input\n", hi->gpio_detect);
		goto error_03;
	}

	/*init gpio_key */
	ret = gpio_request(hi->gpio_key, "gpio_key");
	if (ret < 0) {
		HSD_ERR("Failed to configure gpio%d (gpio_key) gpio_request\n", hi->gpio_key);
		goto error_04;
	}

	ret = gpio_direction_input(hi->gpio_key);
	if (ret < 0) {
		HSD_ERR("Failed to configure gpio%d (gpio_key) gpio_direction_input\n", hi->gpio_key);
		goto error_04;
	}


	/* initialize irq of gpio_key */
	hi->irq_key = gpio_to_irq(hi->gpio_key);

	HSD_DBG("hi->irq_key = %d\n", hi->irq_key);

	if (hi->irq_key < 0) {
		HSD_ERR("Failed to get interrupt number\n");
		ret = hi->irq_key;
		goto error_06;
	}
	ret = request_threaded_irq(hi->irq_key, NULL, button_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, pdev->name, hi);
	if (ret) {
		HSD_ERR("failed to request button irq\n");
		goto error_06;
	}

	ret = irq_set_irq_wake(hi->irq_key, 1);
	if (ret < 0) {
		HSD_ERR("Failed to set irq_key interrupt wake\n");
		goto error_06;
	}
	enable_irq(hi->irq_key);

	hi->irq_detect = gpio_to_irq(hi->gpio_detect);
	HSD_DBG("hi->irq_detect = %d\n", hi->irq_detect);

	if (hi->irq_detect < 0) {
		HSD_ERR("Failed to get interrupt number\n");
		ret = hi->irq_detect;
		goto error_07;
	}
	ret = request_threaded_irq(hi->irq_detect, NULL, earjack_det_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, pdev->name, hi);

	if (ret) {
		HSD_ERR("failed to request button irq\n");
		goto error_07;
	}

	ret = irq_set_irq_wake(hi->irq_detect, 1);
	if (ret < 0) {
		HSD_ERR("Failed to set gpio_detect interrupt wake\n");
		goto error_07;
	}
	/* initialize switch device */
	hi->sdev.name = pdata->switch_name;
	hi->sdev.print_state = lge_hsd_print_state;
	hi->sdev.print_name = lge_hsd_print_name;

	ret = switch_dev_register(&hi->sdev);
	if (ret < 0) {
		HSD_ERR("Failed to register switch device\n");
		goto error_08;
	}

	/* initialize input device */
	hi->input = input_allocate_device();
	if (!hi->input) {
		HSD_ERR("Failed to allocate input device\n");
		ret = -ENOMEM;
		goto error_09;
	}

	hi->input->name = pdata->keypad_name;

	hi->input->id.vendor    = 0x0001;
	hi->input->id.product   = 1;
	hi->input->id.version   = 1;

	/* headset tx noise */
	{
		struct qpnp_vadc_result result;
		int acc_read_value = 0;
		int i, rc;
		int count = 3;

		for (i = 0; i < count; i++)
		{
			rc = qpnp_vadc_read(P_MUX6_1_1,&result);

			if (rc < 0)
			{
				if (rc == -ETIMEDOUT) {
					pr_err("[DEBUG]adc read timeout \n");
				} else {
					pr_err("[DEBUG]adc read error - %d\n", rc);
				}
			}
			else
			{
				acc_read_value = (int)result.physical;
				pr_info("%s: acc_read_value - %d\n", __func__, (int)result.physical);
				break;
			}
		}
	}

	set_bit(EV_SYN, hi->input->evbit);
	set_bit(EV_KEY, hi->input->evbit);
	set_bit(EV_SW, hi->input->evbit);
	set_bit(hi->key_code, hi->input->keybit);
	set_bit(SW_HEADPHONE_INSERT, hi->input->swbit);
	set_bit(SW_MICROPHONE_INSERT, hi->input->swbit);
	input_set_capability(hi->input, EV_KEY, KEY_MEDIA);
	input_set_capability(hi->input, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(hi->input, EV_KEY, KEY_VOLUMEDOWN);
	ret = input_register_device(hi->input);
	if (ret) {
		HSD_ERR("Failed to register input device\n");
		goto error_09;
	}

	if (!(hi->gpio_get_value_func(hi->gpio_detect)))

#ifdef CONFIG_MAX1462X_USE_LOCAL_WORK_QUEUE
		/* to detect in initialization with eacjack insertion */
		queue_work(local_max1462x_workqueue, &(hi->work));
#else
	/* to detect in initialization with eacjack insertion */
	schedule_work(&(hi->work));
#endif
	return ret;

error_09:
	input_free_device(hi->input);
error_08:
	switch_dev_unregister(&hi->sdev);
error_07:
	free_irq(hi->irq_detect, 0);
error_06:
	free_irq(hi->irq_key, 0);
error_04:
	gpio_free(hi->gpio_key);
error_03:
	gpio_free(hi->gpio_detect);
error_02:
	gpio_free(hi->gpio_mic_en);
	kfree(hi);
	return ret;
}

static int lge_hsd_remove(struct platform_device *pdev)
{
	struct hsd_info *hi = (struct hsd_info *)platform_get_drvdata(pdev);

	HSD_DBG("lge_hsd_remove\n");

	if (switch_get_state(&hi->sdev))
		remove_headset(hi);

	input_unregister_device(hi->input);
	switch_dev_unregister(&hi->sdev);

	free_irq(hi->irq_key, 0);
	free_irq(hi->irq_detect, 0);
	gpio_free(hi->gpio_detect);
	gpio_free(hi->gpio_key);
	gpio_free(hi->gpio_mic_en);

	mutex_destroy(&hi->mutex_lock);

	kfree(hi);

	return 0;
}

static struct of_device_id max1462x_match_table[] = {
	{ .compatible = "maxim,max1462x",},
	{},
};
static struct platform_driver lge_hsd_driver = {
	.probe          = lge_hsd_probe,
	.remove         = lge_hsd_remove,
	.driver         = {
		.name           = "max1462x",
		.owner          = THIS_MODULE,
		.of_match_table = max1462x_match_table,
	},
};

static int __init lge_hsd_init(void)
{
	int ret;

	HSD_DBG("enter\n");

#ifdef CONFIG_MAX1462X_USE_LOCAL_WORK_QUEUE
	local_max1462x_workqueue = create_workqueue("max1462x") ;
	if(!local_max1462x_workqueue)
		return -ENOMEM;
#endif
	HSD_DBG("wake_lock_init\n");
	wake_lock_init(&ear_hook_wake_lock, WAKE_LOCK_SUSPEND, "ear_hook");

	ret = platform_driver_register(&lge_hsd_driver);
	if (ret) {
		HSD_ERR("Fail to register platform driver\n");
	}

	return ret;
}

static void __exit lge_hsd_exit(void)
{
#ifdef CONFIG_MAX1462X_USE_LOCAL_WORK_QUEUE
	if(local_max1462x_workqueue)
		destroy_workqueue(local_max1462x_workqueue);
	local_max1462x_workqueue = NULL;
#endif

	platform_driver_unregister(&lge_hsd_driver);
	HSD_DBG("lge_hsd_exit\n");
	wake_lock_destroy(&ear_hook_wake_lock);
}

late_initcall_sync(lge_hsd_init);
module_exit(lge_hsd_exit);

MODULE_DESCRIPTION("LGE Headset detection driver (max1462x)");
MODULE_LICENSE("GPL");
#endif
