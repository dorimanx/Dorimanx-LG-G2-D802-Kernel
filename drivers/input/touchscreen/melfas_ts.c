/* drivers/input/touchscreen/melfas_ts.c
 *
 * Copyright (C) 2010 Melfas, Inc.
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
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c/melfas_ts.h>
#include <linux/gpio.h>

#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_INNOTEK_PANEL_3_5)
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#endif

#ifdef CONFIG_TS_INFO_CLASS
#include "ts_class.h"
#endif

#define MODE_CONTROL                    	0x01
#define TS_READ_START_ADDR              	0x10

#define TS_READ_START_ADDR			0x10
#define TS_READ_VERSION_ADDR			0xF0
#define TS_HW_REVISION_ADDR             	0xF1
#define TS_CORE_VERSION_ADDR            	0xF3
#define TS_PRIVATE_CUSTOM_VERSION_ADDR  	0xF4
#define TS_PUBLIC_CUSTOM_VERSION_ADDR   	0xF5

#define UNIVERSAL_CMD				0xA0
#define UNIVERSAL_CMD_RESULT_SIZE		0xAE
#define UNIVERSAL_CMD_RESULT			0xAF
#define UNIVCMD_ENTER_TEST_MODE			0x40
#define UNIVCMD_TEST_CM_DELTA			0x41
#define UNIVCMD_GET_PIXEL_CM_DELTA		0x42
#define UNIVERSAL_CMD_EXIT			0x4F

#define TS_READ_REGS_LEN			100
#define TS_READ_VERSION_INFO_LEN		6

#define MELFAS_MAX_TOUCH			10  /* ts->pdata->num_of_finger */
#define MELFAS_MAX_BTN				4
#define MELFAS_PACKET_SIZE			6

unsigned short MELFAS_binary_nLength;
unsigned char *MELFAS_binary;
extern unsigned short MELFAS_binary_nLength_1;
extern unsigned char MELFAS_binary_1[];
extern unsigned short MELFAS_binary_nLength_2;
extern unsigned char MELFAS_binary_2[];


#define I2C_RETRY_CNT				10

#define PRESS_KEY				1
#define RELEASE_KEY				0

#define SET_DOWNLOAD_BY_GPIO			1

#define MIP_INPUT_EVENT_PACKET_SIZE		0x0F
#define MIP_INPUT_EVENT_INFORMATION		0x10

#define FW_VERSION_ADDR 	0
#define TS_MAKER_ID         4

/* need to update define value when FW changed */
#define D1LK_TOVIS_VER 0xcb
#define D1LK_LGIT_VER 0x07

/*#include "mms136_download.h"*/
#include "mms136_ISP_download.h"

#define get_time_interval(a, b) (a >= b ? a-b : 1000000 + a - b)
struct timeval t_debug[2];

static volatile int init_values[20];
static volatile int irq_flag;
static volatile int tmp_flag[10];
static volatile int point_of_release;
static volatile int time_diff;
static volatile int pre_keycode;
static volatile int touch_prestate;
static volatile int btn_prestate;

/* ESD mode change for D1LV and D1LK */
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_TOVIS_PANEL)
static volatile int sw_reset_cnt;
static volatile int esd_mode_change;
#endif

static int num_tx_line = 21;
static int num_rx_line = 12;

#if defined(CONFIG_LGE_PM)
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_INNOTEK_PANEL_3_5)
static int ac_change;
#endif
#endif


/* touch screen device */
#include <linux/regulator/consumer.h>

#include <linux/async.h>



#define TS_GPIO_IRQ 5
#define TS_X_MIN          0
#define TS_X_MAX          720
#define TS_Y_MIN          0
#define TS_Y_MAX          1280
#define TS_GPIO_I2C_SDA          6
#define TS_GPIO_I2C_SCL          7


int ts_set_vreg(int on, bool log_en)
{
	int rc = -EINVAL;
	static struct regulator *vreg_l22;
	static struct regulator *vreg_lvs3;

	/* 3.3V_TOUCH_VDD, VREG_L15: 2.75 ~ 3.3 */
	if (!vreg_l22) {
		vreg_l22 = regulator_get(NULL, "8941_l22");
		if (IS_ERR(vreg_l22)) {
			pr_err("%s: regulator get of pm8941_l22 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_l22));
			rc = PTR_ERR(vreg_l22);
			vreg_l22 = NULL;
			return rc;
		}
	}
	/* 1.8V_TOUCH_IO, VREG_L22: 1.7 ~ 2.85 */
	if (!vreg_lvs3) {
		vreg_lvs3 = regulator_get(NULL, "8941_lvs3");
		if (IS_ERR(vreg_lvs3)) {
			pr_err("%s: regulator get of pm8941_lvs3 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_lvs3));
			rc = PTR_ERR(vreg_lvs3);
			vreg_lvs3 = NULL;
			return rc;
		}
	}

       rc = regulator_set_voltage(vreg_l22, 3000000, 3000000);

	if (rc < 0) {
		printk(KERN_INFO "[Touch D] %s: cannot control regulator:%d\n",
		       __func__, rc);
		return rc;
	}

	if (on) {
		printk("[Touch D]touch enable\n");
		regulator_enable(vreg_l22);
		regulator_enable(vreg_lvs3);
	} else {
		printk("[Touch D]touch disable\n");
		regulator_disable(vreg_l22);
		regulator_disable(vreg_lvs3);
	}

	return rc;
}

static struct melfas_tsi_platform_data melfas_ts_pdata = {
	.gpio_scl = 0,/*TS_GPIO_I2C_SCL,*/
	.gpio_sda = 0,/*TS_GPIO_I2C_SDA,*/
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 3,
	.button[0]				= KEY_MENU,
	.button[1]				= KEY_HOMEPAGE,
	.button[2]				= KEY_BACK,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= 1,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
};

static int maker_id_val;

void touch_init(void)
{
	int rc;
	maker_id_val = 0;

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 3, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 3, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#if 	0
	gpio_tlmm_config(GPIO_CFG(MSM_8960_TS_PWR, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	/* gpio init */

	rc = gpio_request(MSM_8960_TS_PWR, "TOUCH_PANEL_PWR");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);
#endif

	rc = gpio_request(TS_MAKER_ID, "TOUCH_PANEL_MAKERID");
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/*  maker_id */
	gpio_direction_input(TS_MAKER_ID);

	/* power on */
	rc = ts_set_vreg(1, false);

	/* read gpio value */
	maker_id_val = gpio_get_value(TS_MAKER_ID);
}


/***************************************************************************
 * Debug Definitions
 ***************************************************************************/
enum {
	MELFAS_TS_DEBUG_PROBE = 1U << 0,
	MELFAS_TS_DEBUG_KEY_EVENT = 1U << 1,
	MELFAS_TS_DEBUG_TOUCH_EVENT = 1U << 2,
	MELFAS_TS_DEBUG_TOUCH_EVENT_ONETIME = 1U << 3,
	MELFAS_TS_DEBUG_EVENT_HANDLER = 1U << 4,
	MELFAS_TS_DEBUG_IRQ_HANDLER = 1U << 5,
	MELFAS_TS_DEBUG_TIME = 1U << 6,
};

static int melfas_ts_debug_mask = 9;

module_param_named(
	debug_mask, melfas_ts_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);
#define MELFAS_TS_DPRINTK(mask, level, message, ...) \
	do { \
		if ((mask) & melfas_ts_debug_mask) \
			printk(level message, ## __VA_ARGS__); \
	} while (0)

#define MELFAS_TS_DEBUG_PRINT_TOUCH_EVENT(temp) \
	do { \
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_TOUCH_EVENT, KERN_INFO, \
			"[TOUCH] %s   %d : x=%d y=%d p=%d \n", \
			temp, i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].pressure); \
		if (tmp_flag[i] == 1) { \
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_TOUCH_EVENT_ONETIME, KERN_INFO, \
			"[TOUCH] %s   %d : x=%d y=%d p=%d \n", \
			temp, i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].pressure); \
			if (!strcmp (temp, "Press")) \
				tmp_flag[i] = 0;\
		} \
	} while (0)

#define MELFAS_TS_DEBUG_PRINT_TIME() \
	do { \
		if (MELFAS_TS_DEBUG_TIME & melfas_ts_debug_mask) { \
			if (t_debug[0].tv_sec == 0	&& t_debug[0].tv_usec == 0) { \
				t_debug[0].tv_sec = t_debug[1].tv_sec; \
				t_debug[0].tv_usec = t_debug[1].tv_usec; \
			} else { \
				printk("Interrupt interval: %6luus\n", get_time_interval(t_debug[1].tv_usec, t_debug[0].tv_usec)); \
				t_debug[0].tv_sec = t_debug[1].tv_sec; \
				t_debug[0].tv_usec = t_debug[1].tv_usec; \
			} \
		} \
	} while (0)

/**************************************************************************/

enum {
	None = 0,
	TOUCH_SCREEN,
	TOUCH_KEY
};

struct muti_touch_info {
	int strength;
	int width;
	int posX;
	int posY;
	int pressure;
	int btn_touch;
};

struct btn_info {
	int key_code;
	int status;
};

struct melfas_ts_data {
#ifdef CONFIG_TS_INFO_CLASS
	struct ts_info_classdev cdev;
#endif
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct melfas_tsi_platform_data *pdata;
	struct delayed_work work;
	uint32_t flags;
	struct work_struct upgrade_work;
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7)
	char group_version[10];
#endif
	int version;
	struct early_suspend early_suspend;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif

/* ESD mode change for D1LV and D1LK */
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_TOVIS_PANEL)
static void melfas_ts_esd_change(struct melfas_ts_data *ts);
#endif

#if defined(CONFIG_LGE_PM)
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_INNOTEK_PANEL_3_5)
static void melfas_ts_noise_mode_change(struct melfas_ts_data *ts);
#endif
#endif

static void release_all_fingers(struct melfas_ts_data *ts);
static void melfas_ts_sw_reset(struct melfas_ts_data *ts);

static struct muti_touch_info g_Mtouch_info[MELFAS_MAX_TOUCH];
static struct btn_info g_btn_info[MELFAS_MAX_BTN];

static int check_abs_time(void)
{
	time_diff = 0;

	if (!point_of_release)
		return 0;

	time_diff = jiffies_to_msecs(jiffies) - point_of_release;
	if (time_diff > 0)
		return time_diff;
	else
		return 0;
}

static void melfas_ts_event_handler(struct melfas_ts_data *ts)
{
	int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN];
	int touchType = 0, touchState = 0, touchID = 0, pressed_type = 0;
	int posX = 0, posY = 0, width = 0, strength = 10;
	int keyID = 0, reportID = 0;
	uint8_t read_num = 0, pressed_count = 0;
	static int is_mix_event;

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_INFO,
			"melfas_ts_event_handler \n");

	if (ts == NULL)
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler TS is NULL\n");

	buf[0] = MIP_INPUT_EVENT_PACKET_SIZE;
	ret = i2c_master_send(ts->client, buf, 1);
	ret = i2c_master_recv(ts->client, &read_num, 1);
	/* touch ic reset for ESD defense  */
	if (ret < 0) {
		melfas_ts_sw_reset(ts);
		goto err_free_irq;
	}

	if (read_num == 0) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler: read number 0 \n");
		goto err_free_irq;
	} else if (read_num > MELFAS_MAX_TOUCH*MELFAS_PACKET_SIZE) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler: read number is out of range\n");
		goto err_free_irq;
	}

	buf[0] = MIP_INPUT_EVENT_INFORMATION;
	ret = i2c_master_send(ts->client, buf, 1);
	ret = i2c_master_recv(ts->client, &buf[0], read_num);

	/* touch ic reset for ESD defense
	     if reportID is -0x0F, meflas touch IC need sw reset */
	reportID = (buf[0] & 0x0F);
	if (reportID == 0x0F) {
/* ESD mode change for D1LV and D1LK */
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_TOVIS_PANEL)
		if (sw_reset_cnt > 20) {
			printk("[TOUCH] esd mode change start!!!!\n");
			melfas_ts_esd_change(ts);
			esd_mode_change = 1;
			sw_reset_cnt = 0;
		} else {
			if (esd_mode_change != 1) {
				printk("ESD 0x0F : ");
				melfas_ts_sw_reset(ts);
				sw_reset_cnt++;
			}
		}
#else
		printk("ESD 0x0F : ");
		melfas_ts_sw_reset(ts);
#endif
		goto err_free_irq;
	}

#if defined(CONFIG_LGE_PM)
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_INNOTEK_PANEL_3_5)
	if (ac_change != pm8921_charger_is_ta_connected()) {
		printk("[TOUCH] TA status change!!!\n");
		melfas_ts_noise_mode_change(ts);
		ac_change = pm8921_charger_is_ta_connected();
	}
#endif
#endif

	for (i = 0; i < read_num; i = i + 6) {
		if (ret < 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler: i2c failed\n");
			goto err_free_irq;
		} else {
			touchType  =  ((buf[i] & 0x60) >> 5);				/* Touch Screen, Touch Key */
			touchState = ((buf[i] & 0x80) == 0x80);				/* touchAction = (buf[0]>>7)&&0x01;*/
			reportID = (buf[i] & 0x0F);					/* Touch Screen -> n.th finger input
											Touch Key -> n.th touch key area. */
			posX = (uint16_t) (buf[i + 1] & 0x0F) << 8 | buf[i + 2];	/* X position (0 ~ 4096, 12 bit) */
			posY = (uint16_t) (buf[i + 1] & 0xF0) << 4 | buf[i + 3];	/* Y position (0 ~ 4096, 12 bit) */
			width = buf[i + 4];
			strength = buf[i + 5];

			if (touchType == TOUCH_KEY)
				keyID = reportID;
			else if (touchType == TOUCH_SCREEN) {
				touchID = reportID-1;
				pressed_type = TOUCH_SCREEN;
			}

			if (touchID > ts->pdata->num_of_finger-1)
				goto err_free_irq;

			if (touchType == TOUCH_SCREEN) {
				g_Mtouch_info[touchID].posX = posX;
				g_Mtouch_info[touchID].posY = posY;
				g_Mtouch_info[touchID].width = width;

				g_Mtouch_info[touchID].strength = strength;
				g_Mtouch_info[touchID].pressure = strength;
				g_Mtouch_info[touchID].btn_touch = touchState;

				if (btn_prestate && touch_prestate == 0) {
					input_report_key(ts->input_dev, pre_keycode, 0xff);
					btn_prestate = 0;
				}
			} else if (touchType == TOUCH_KEY) {
				g_btn_info[keyID].key_code = ts->pdata->button[keyID-1];
				g_btn_info[keyID].status = touchState;

				if (keyID > ts->pdata->num_of_button || keyID == 0) {
					MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR, "Touchkey ID error \n");
				} else if (is_mix_event == 1) {
					input_report_key(ts->input_dev, pre_keycode, 0xff);
					is_mix_event = 0;
					btn_prestate = touchState;
				} else{
					if (touch_prestate) {
						btn_prestate = touchState;
					} else if (check_abs_time() > 0 && check_abs_time() < 100) {
						btn_prestate = touchState;
						point_of_release = 0;
					} else if (btn_prestate != touchState) {
						if (touchState == PRESS_KEY) {
							pre_keycode = ts->pdata->button[keyID-1];
							input_report_key(ts->input_dev, ts->pdata->button[keyID-1], PRESS_KEY);
						} else {
							input_report_key(ts->input_dev, ts->pdata->button[keyID-1], RELEASE_KEY);
						}
						btn_prestate = touchState;
					}
				}

				if ((read_num > 6) && (pressed_type == TOUCH_SCREEN)) {
					if (touchState && (touch_prestate == 0))
						is_mix_event = 1;
					touchType = TOUCH_SCREEN;
				}

				MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_KEY_EVENT, KERN_INFO,
					"melfas_ts_event_handler: keyID : %d, touchState: %d\n",
					keyID, touchState);
				break;
			}

		}
	}

	if (touchType == TOUCH_SCREEN) {
		for (i = 0; i < ts->pdata->num_of_finger; i++) {
			if (g_Mtouch_info[i].btn_touch == -1)
				continue;

			if (g_Mtouch_info[i].btn_touch == 0) {
				g_Mtouch_info[i].btn_touch = -1;
				tmp_flag[i] = 1;
				MELFAS_TS_DEBUG_PRINT_TOUCH_EVENT("Release");
				continue;
			}
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].width);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, g_Mtouch_info[i].strength);
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
			input_mt_sync(ts->input_dev);
			MELFAS_TS_DEBUG_PRINT_TOUCH_EVENT("Press");
			touch_prestate = 1;
			pressed_count++;
		}
		if (pressed_count == 0) {
			input_mt_sync(ts->input_dev);
			touch_prestate = 0;
			point_of_release = jiffies_to_msecs(jiffies);
		}
	}
	input_sync(ts->input_dev);

	MELFAS_TS_DEBUG_PRINT_TIME();
	return;
err_free_irq:
	return;
}

static irqreturn_t melfas_ts_irq_handler(int irq, void *handle)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)handle;

	irq_flag = 1;

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_IRQ_HANDLER, KERN_INFO, "melfas_ts_irq_handler\n");

	if (MELFAS_TS_DEBUG_TIME & melfas_ts_debug_mask)
	    do_gettimeofday(&t_debug[1]);

	melfas_ts_event_handler(ts);

	irq_flag = 0;
	return IRQ_HANDLED;
}

#if defined(CONFIG_USING_TOVIS_PANEL)
static ssize_t
mms136_fw_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	uint8_t verbuf[TS_READ_VERSION_INFO_LEN];
	int len, ret;

	if (gpio_get_value(TS_MAKER_ID) == 0) {	/* Tovis Panel */
		MELFAS_binary_nLength = MELFAS_binary_nLength_1;
		MELFAS_binary = MELFAS_binary_1;
		ts->pdata->fw_ver = D1LK_TOVIS_VER;
	} else { 		/* LGIT Panel */
		MELFAS_binary_nLength = MELFAS_binary_nLength_2;
		MELFAS_binary = MELFAS_binary_2;
		ts->pdata->fw_ver = D1LK_LGIT_VER;
	}

	verbuf[0] = TS_READ_VERSION_ADDR;
	i2c_master_send(ts->client, &verbuf[0], 1);
	i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);
	ts->version = verbuf[FW_VERSION_ADDR];

	if (ts->version != ts->pdata->fw_ver) {
		ts->pdata->power_enable(0, true);
		mdelay(50);

		free_irq(ts->client->irq, ts);

		mms100_download(isp_sys_type, embedded_img);
		ts->pdata->power_enable(0, true);
		ts->pdata->power_enable(1, true);

		msleep(50);
		ret = request_threaded_irq(ts->client->irq, NULL, melfas_ts_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_LOW, ts->client->name, ts);
		if (ret > 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
				"[TOUCH] Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
			ret = -EBUSY;
		}
	}

	verbuf[0] = TS_READ_VERSION_ADDR;
	ret = i2c_master_send(ts->client, &verbuf[0], 1);
	ret = i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);
	ts->version = verbuf[FW_VERSION_ADDR];

	len = snprintf(buf, PAGE_SIZE, "%d\n", ts->version);

	return len;
}
#endif
static ssize_t
mms136_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	uint8_t verbuf[TS_READ_VERSION_INFO_LEN];
	int len;

	verbuf[0] = TS_READ_VERSION_ADDR;
	i2c_master_send(ts->client, &verbuf[0], 1);
	i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);

	ts->version = verbuf[FW_VERSION_ADDR];

	len = snprintf(buf, PAGE_SIZE, "%d\n", ts->version);
	return len;
}

static ssize_t
mms136_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int len;
	len = snprintf(buf, PAGE_SIZE, "\nMMS-136 Device Status\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=============================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "irq num       is %d\n", ts->client->irq);
	len += snprintf(buf + len, PAGE_SIZE - len, "gpio_irq num  is %d(level=%d)\n", ts->pdata->i2c_int_gpio, gpio_get_value(ts->pdata->i2c_int_gpio));
	len += snprintf(buf + len, PAGE_SIZE - len, "gpio_scl num  is %d\n", ts->pdata->gpio_scl);
	len += snprintf(buf + len, PAGE_SIZE - len, "gpio_sda num  is %d\n", ts->pdata->gpio_sda);
	return len;
}

static ssize_t
mms136_fw_upgrade_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd, i = 1, ret;
	uint8_t verbuf[TS_READ_VERSION_INFO_LEN];
	uint8_t read_num = 0;
	uint8_t databuf[TS_READ_REGS_LEN];

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;
	switch (cmd) {
	case 1:
		ts->pdata->power_enable(1, true);
		mdelay(50);
		if (irq_flag == 0) {
			printk("disable_irq_nosync\n");
			disable_irq_nosync(ts->client->irq);
			irq_flag = 1;
		}
		do {
			mdelay(100);
			if (i % 10 == 0) {
				databuf[0] = MIP_INPUT_EVENT_PACKET_SIZE;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &read_num, 1);
				/* touch ic reset for ESD defense  */
				if (ret < 0) {
					melfas_ts_sw_reset(ts);
					continue;
				}

				if (read_num == 0) {
					printk("melfas_ts_event_handler: read number 0 \n");
					continue;
				}
				databuf[0] = MIP_INPUT_EVENT_INFORMATION;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &databuf[0], read_num);
				mdelay(100);
			}
			i++;
		} while (!gpio_get_value(ts->pdata->i2c_int_gpio));
		mms100_download(isc_type, embedded_img);
		ts->pdata->power_enable(0, true);
		ts->pdata->power_enable(1, true);

		verbuf[0] = TS_READ_VERSION_ADDR;
		ret = i2c_master_send(ts->client, &verbuf[0], 1);
		ret = i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);

		ts->version = verbuf[FW_VERSION_ADDR];

		if (irq_flag == 1) {
			printk("enable_irq\n");
			enable_irq(ts->client->irq);
			irq_flag = 0;
		}
		break;
	case 2:
		ts->pdata->power_enable(1, true);
		mdelay(50);
		if (irq_flag == 0) {
			printk("disable_irq_nosync\n");
			disable_irq_nosync(ts->client->irq);
			irq_flag = 1;
		}
		do {
			mdelay(100);
			if (i % 10 == 0) {
				databuf[0] = MIP_INPUT_EVENT_PACKET_SIZE;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &read_num, 1);
				/* touch ic reset for ESD defense  */
				if (ret < 0) {
					melfas_ts_sw_reset(ts);
					continue;
				}

				if (read_num == 0) {
					printk("melfas_ts_event_handler: read number 0 \n");
					continue;
				}
				databuf[0] = MIP_INPUT_EVENT_INFORMATION;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &databuf[0], read_num);
				mdelay(100);
			}
			i++;
		} while (!gpio_get_value(ts->pdata->i2c_int_gpio));
		mms100_download(isc_type, external_img);
		ts->pdata->power_enable(0, true);
		ts->pdata->power_enable(1, true);

		verbuf[0] = TS_READ_VERSION_ADDR;
		ret = i2c_master_send(ts->client, &verbuf[0], 1);
		ret = i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);

		ts->version = verbuf[FW_VERSION_ADDR];

		if (irq_flag == 1) {
			printk("enable_irq\n");
			enable_irq(ts->client->irq);
			irq_flag = 0;
		}
	break;
	default:
		printk(KERN_INFO "usage: echo [1|2] > fw_upgrade\n");
		printk(KERN_INFO "  - 1: firmware upgrade with embedded firmware image\n");
		printk(KERN_INFO "  - 2: firmware upgrade with firmware image in user area\n");
		break;
	}
	return count;
}

static ssize_t
mms136_power_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	switch (cmd) {
	case 1: /* touch power on */
		ts->pdata->power_enable(1, true);
		break;
	case 2: /*touch power off */
		ts->pdata->power_enable(0, true);
		break;
	case 3:
		ts->pdata->power_enable(0, true);
		ts->pdata->power_enable(1, true);
		break;
	default:
		printk(KERN_INFO "usage: echo [1|2|3] > control\n");
		printk(KERN_INFO "  - 1: power on\n");
		printk(KERN_INFO "  - 2: power off\n");
		printk(KERN_INFO "  - 3: power reset\n");
		break;
	}
	return count;
}

static ssize_t
mms136_irq_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd, ret;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	switch (cmd) {
	case 1: /* interrupt pin high */
		ret = gpio_direction_input(ts->pdata->i2c_int_gpio);
		if (ret < 0) {
			printk(KERN_ERR "%s: gpio input direction fail\n", __FUNCTION__);
			break;
		}
		gpio_set_value(ts->pdata->i2c_int_gpio, 1);
		printk(KERN_INFO "MMS-136 INTR GPIO pin high\n");
		break;
	case 2: /* interrupt pin LOW */
		ret = gpio_direction_input(ts->pdata->i2c_int_gpio);
		if (ret < 0) {
			printk(KERN_ERR "%s: gpio input direction fail\n", __FUNCTION__);
			break;
		}
		gpio_set_value(ts->pdata->i2c_int_gpio, 0);
		printk(KERN_INFO "MMS-136 INTR GPIO pin low\n");
		break;
	default:
		printk(KERN_INFO "usage: echo [1|2|3|4] > control\n");
		printk(KERN_INFO "  - 1: interrupt pin high\n");
		printk(KERN_INFO "  - 2: interrupt pin low\n");
		break;
	}
	return count;
}

static ssize_t
mms136_reg_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd, ret, reg_addr, length, i;
	uint8_t reg_buf[TS_READ_REGS_LEN];
	if (sscanf(buf, "%d, 0x%x, %d", &cmd, &reg_addr, &length) != 3)
		return -EINVAL;
	switch (cmd) {
	case 1:
		reg_buf[0] = reg_addr;
		ret = i2c_master_send(ts->client, reg_buf, 1);
		if (ret < 0) {
			printk(KERN_ERR "i2c master send fail\n");
			break;
		}
		ret = i2c_master_recv(ts->client, reg_buf, length);
		if (ret < 0) {
			printk(KERN_ERR "i2c master recv fail\n");
			break;
		}
		for (i = 0; i < length; i++) {
			printk(KERN_INFO "0x%x", reg_buf[i]);
		}
		printk(KERN_INFO "\n 0x%x register read done\n", reg_addr);
		break;
	case 2:
		reg_buf[0] = reg_addr;
		reg_buf[1] = length;
		ret = i2c_master_send(ts->client, reg_buf, 2);
		if (ret < 0) {
			printk(KERN_ERR "i2c master send fail\n");
			break;
		}
		printk(KERN_INFO "\n 0x%x register write done\n", reg_addr);
		break;
	default:
		printk(KERN_INFO "usage: echo [1(read)|2(write)], [reg address], [length|value] > reg_control\n");
		printk(KERN_INFO "  - Register Set or Read\n");
		break;
	}
	return count;
}

static ssize_t
mms136_cmdelta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	char c = '-';
	int len, i, j, t;
	uint8_t write_buf[5];
	uint8_t read_buf[10];
	uint8_t read_size = 0;
	uint16_t cmdata = 0;
	int flag = 0;

	if (irq_flag == 0) {
		printk("disable_irq_nosync\n");
		disable_irq_nosync(ts->client->irq);
		irq_flag = 1;
	}
	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVCMD_ENTER_TEST_MODE;
	i2c_master_send(ts->client, write_buf, 2);

	while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
		flag++;
		if (flag == 30) {
			flag = 0;
			break;
		}
		msleep(100);
	}
	flag = 0;

	write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
	i2c_master_send(ts->client, write_buf, 1);
	i2c_master_recv(ts->client, read_buf, 1);
	printk("TEST MODE ENTER =%d \n", read_buf[0]);

	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVCMD_TEST_CM_DELTA;
	i2c_master_send(ts->client, write_buf, 2);

	while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
		flag++;
		if (flag == 30) {
			flag = 0;
			break;
		}
		msleep(100);
	}
	flag = 0;

	write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
	i2c_master_send(ts->client, write_buf, 1);
	i2c_master_recv(ts->client, read_buf, 1);
	printk("CM DELTA TEST =%d \n", read_buf[0]);

	len = snprintf(buf, PAGE_SIZE, "Touch Firmware Version is %d\n", ts->version);
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%5c", c);
	for (j = 0; j < num_tx_line; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%5d", j);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "------------------------\n");

	/* read touch screen cmdelta */
	for (i = 0; i < num_rx_line ; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%2d : ", i);
		for (j = 0; j < num_tx_line; j++) {
			write_buf[0] = UNIVERSAL_CMD;
			write_buf[1] = UNIVCMD_GET_PIXEL_CM_DELTA;
			write_buf[2] = j;
			write_buf[3] = i;
			i2c_master_send(ts->client, write_buf, 4);

			while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
				flag++;
				if (flag == 100) {
					flag = 0;
					break;
				}
				udelay(100);
			}

			write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
			i2c_master_send(ts->client, write_buf, 1);
			i2c_master_recv(ts->client, &read_size, 1);

			write_buf[0] = UNIVERSAL_CMD_RESULT;
			i2c_master_send(ts->client, write_buf, 1);
			i2c_master_recv(ts->client, read_buf, read_size);

			cmdata = read_buf[1];
			cmdata = ((cmdata << 8) | read_buf[0]);
			len += snprintf(buf + len, PAGE_SIZE - len, "%5d", cmdata);
		}
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	/* read touch key cmdelta */
	len += snprintf(buf + len, PAGE_SIZE - len, "key: ");
	for (t = 0; t < MELFAS_MAX_BTN; t++) /*Model Dependent*/	{
		write_buf[0] = UNIVERSAL_CMD;
		write_buf[1] = 0x4A;
		write_buf[2] = t; /*KEY CH.*/
		write_buf[3] = 0; /*Dummy Info*/
		i2c_master_send(ts->client, write_buf, 4);

		while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
			flag++;
			if (flag == 100) {
				flag = 0;
				break;
			}
			udelay(100);
		}

		write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, &read_size, 1);

		write_buf[0] = UNIVERSAL_CMD_RESULT;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, read_buf, read_size);

		cmdata = read_buf[1];
		cmdata = ((cmdata << 8) | read_buf[0]);
		len += snprintf(buf + len, PAGE_SIZE - len, "%5d", cmdata);
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "\n===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");

	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVERSAL_CMD_EXIT;

	i2c_master_send(ts->client, write_buf, 2);

	if (irq_flag == 1) {
		printk("enable_irq\n");
		enable_irq(ts->client->irq);
		irq_flag = 0;
	}
	return len;
}

static ssize_t
mms136_key_intensity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	char c = '-';
	int len, i, j;
	uint8_t write_buf[10];
	uint8_t read_buf[25];
	int8_t cmdata = 0;

	if (irq_flag == 0) {
		printk("disable_irq_nosync\n");
		disable_irq_nosync(ts->client->irq);
		irq_flag = 1;
	}

	len = snprintf(buf, PAGE_SIZE, "Touch Firmware Version is %d\n", ts->version);
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%5c", c);
	for (j = 0; j < num_tx_line+1; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%5d", j);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "------------------------\n");

	for (i = 0; i < num_rx_line; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%2d : ", i);
		j = 0;
		write_buf[0] = 0xB0;
		write_buf[1] = 0x1A;
		write_buf[2] = j;  /*Exciting CH.*/
		write_buf[3] = i;  /*Sensing CH.*/
		write_buf[4] = 0; /*Reserved*/
		write_buf[5] = 0x08; /*Flag*/
		i2c_master_send(ts->client, write_buf, 6);

		write_buf[0] = 0xBF;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, read_buf, num_tx_line+1);

		for (j = 0; j < ts->pdata->num_of_finger; j++) {
			cmdata = (int8_t) read_buf[j];
			len += snprintf(buf + len, PAGE_SIZE - len, "%5d", cmdata);
		}
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");

	if (irq_flag == 1) {
		printk("enable_irq\n");
		enable_irq(ts->client->irq);
		irq_flag = 0;
	}
	return len;
}


static ssize_t
mms136_intensity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	char c = '-';
	int len, i, j;
	uint8_t write_buf[10];
	uint8_t read_buf[25];
	int8_t cmdata = 0;

	if (irq_flag == 0) {
		printk("disable_irq_nosync\n");
		disable_irq_nosync(ts->client->irq);
		irq_flag = 1;
	}

	len = snprintf(buf, PAGE_SIZE, "Touch Firmware Version is %d\n", ts->version);
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%5c", c);
	for (j = 0; j < num_tx_line+1; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%5d", j);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "------------------------\n");

	for (i = 0; i < num_rx_line; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%2d : ", i);
		j = 0;
		write_buf[0] = 0xB0;
		write_buf[1] = 0x1A;
		write_buf[2] = j;  /*Exciting CH.*/
		write_buf[3] = i;  /*Sensing CH.*/
		write_buf[4] = 0; /*Reserved*/
		write_buf[5] = 0x04; /*Flag*/
		i2c_master_send(ts->client, write_buf, 6);

		write_buf[0] = 0xBF;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, read_buf, num_tx_line+1);

		for (j = 0; j < num_tx_line+1; j++) {
			cmdata = (int8_t) read_buf[j];
			len += snprintf(buf + len, PAGE_SIZE - len, "%5d", cmdata);
		}
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");

	if (irq_flag == 1) {
		printk("enable_irq\n");
		enable_irq(ts->client->irq);
		irq_flag = 0;
	}
	return len;
}

static ssize_t
mms136_all_version_show(struct device *dev, struct device_attribute *attr,
char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret, len;
	uint8_t version_buf[6];

	version_buf[0] = TS_READ_VERSION_ADDR;
	ret = i2c_master_send(ts->client, &version_buf[0], 1);
	ret = i2c_master_recv(ts->client, &version_buf[0], TS_READ_VERSION_INFO_LEN);

	len = snprintf(buf, PAGE_SIZE, "Melfas Version Info\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "============================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "Firmware Version : %d\n", version_buf[0]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Hardware Version : %d\n", version_buf[1]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Compatibility Group : %c\n", version_buf[2]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Core Firmware Version : %d\n", version_buf[3]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Private Custom Version : %d\n", version_buf[4]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Public Custom Version : %d\n", version_buf[5]);
	len += snprintf(buf + len, PAGE_SIZE - len, "============================\n");

	return len;
}
#if defined(CONFIG_USING_TOVIS_PANEL)
static ssize_t
mms136_new_version_show(struct device *dev, struct device_attribute *attr,
char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int len;

	if (gpio_get_value(TS_MAKER_ID) == 0) 	/* Tovis Panel */
		ts->pdata->fw_ver = D1LK_TOVIS_VER;
	 else  		/* LGIT Panel */
		ts->pdata->fw_ver = D1LK_LGIT_VER;

	len = snprintf(buf, PAGE_SIZE, "%d\n", ts->pdata->fw_ver);

	return len;
}
#endif

static ssize_t
set_resolution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd, ret = 0, i;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	if (cmd > 0) {
		printk("[TOUCH] Reinitialize Touch input device\n");
		input_unregister_device(ts->input_dev);
		ts->input_dev = input_allocate_device();

		if (!ts->input_dev) {
			printk("[TOUCH] Not enough memory\n");
			return ret;
		}

		ts->input_dev->name = "melfas-ts";

		ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);

		for (i = 0; i < ts->pdata->num_of_button; i++)
			ts->input_dev->keybit[BIT_WORD(ts->pdata->button[i])] |= BIT_MASK(ts->pdata->button[i]);

		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,  ts->pdata->x_max * cmd, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,  ts->pdata->y_max * cmd, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 40, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH - 1, 0, 0);

		ret = input_register_device(ts->input_dev);
		if (ret) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
					  "[TOUCH] Failed to register device\n");
			return ret;
		}
	} else {
		printk(KERN_INFO "usage: echo [1|2|3|4] > resolution\n");
		printk(KERN_INFO "  - 1: X 1\n");
		printk(KERN_INFO "  - 2: X 2\n");
	}
	return count;
}

static struct device_attribute mms136_device_attrs[] = {
	__ATTR(status,  S_IRUGO | S_IWUSR, mms136_status_show, NULL),
	__ATTR(version, S_IRUGO | S_IWUSR, mms136_version_show, NULL),
	__ATTR(fw_upgrade, S_IRUGO | S_IWUSR, NULL, mms136_fw_upgrade_store),
	__ATTR(power_control, S_IRUGO | S_IWUSR, NULL, mms136_power_control_store),
	__ATTR(irq_control, S_IRUGO | S_IWUSR, NULL, mms136_irq_control_store),
	__ATTR(reg_control, S_IRUGO | S_IWUSR, NULL, mms136_reg_control_store),
	__ATTR(cmdelta, S_IRUGO | S_IWUSR, mms136_cmdelta_show, NULL),
	__ATTR(intensity, S_IRUGO | S_IWUSR, mms136_intensity_show, NULL),
	__ATTR(key_intensity, S_IRUGO | S_IWUSR, mms136_key_intensity_show, NULL),
	__ATTR(all_version, S_IRUGO | S_IWUSR, mms136_all_version_show, NULL),
#if defined(CONFIG_USING_TOVIS_PANEL)
	__ATTR(fw_read, S_IRUGO | S_IWUSR, mms136_fw_read_show, NULL),
	__ATTR(new_version, S_IRUGO | S_IWUSR, mms136_new_version_show, NULL),
#endif
	__ATTR(resolution, S_IRUGO | S_IWUSR, NULL, set_resolution_store),
};

static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data *ts;
	int ret = 0, i;
	int m = 1;                       /* l1a use double resolution */

	uint8_t buf[TS_READ_VERSION_INFO_LEN];
	irq_flag = 0;

#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_TOVIS_PANEL)
	esd_mode_change = 0;
#endif

#if defined(CONFIG_LGE_PM)
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_INNOTEK_PANEL_3_5)
	ac_change = pm8921_charger_is_ta_connected();
#endif
#endif

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] melfas_ts_probe Start!!!\n");

	touch_init();

	for (i = 0; i < 20; i++)
		init_values[i] = 1;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] failed to create a state of melfas-ts\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->pdata = &melfas_ts_pdata;
/*	ts->pdata = client->dev.platform_data;*/

	ts->client = client;
	i2c_set_clientdata(client, ts);

#if defined(CONFIG_USING_TOVIS_PANEL) /* [D1LK] this binary only for Rev.C ITO Sample */
	if (gpio_get_value(TS_MAKER_ID) == 0) {	/* Tovis Panel */  /*by andrew*/
		MELFAS_binary_nLength = MELFAS_binary_nLength_1;
		MELFAS_binary = MELFAS_binary_1;
		ts->pdata->fw_ver = D1LK_TOVIS_VER;
	} else { 		/* LGIT Panel */
		MELFAS_binary_nLength = MELFAS_binary_nLength_2;
		MELFAS_binary = MELFAS_binary_2;
		ts->pdata->fw_ver = D1LK_LGIT_VER;
	}
#elif defined(CONFIG_USING_INNOTEK_PANEL_4_7) /* [D1LV] this binary only for Rev.B */
	buf[0] = TS_READ_VERSION_ADDR;
	ret = i2c_master_send(ts->client, &buf[0], 1);
	ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

	if (buf[2] == 'C') {
		MELFAS_binary_nLength = MELFAS_binary_nLength_1;
		MELFAS_binary = MELFAS_binary_1;
	} else {
		MELFAS_binary_nLength = MELFAS_binary_nLength_2; /* for 'D' */
		MELFAS_binary = MELFAS_binary_2;
		ts->pdata->fw_ver = 0x12;
	}
#elif defined(CONFIG_USING_INNOTEK_PANEL_3_5)
	if (lge_get_board_revno() == HW_REV_A) {
		MELFAS_binary_nLength = MELFAS_binary_nLength_1;
		MELFAS_binary = MELFAS_binary_1;
		ts->pdata->fw_ver = 0x04;
	} else {
		MELFAS_binary_nLength = MELFAS_binary_nLength_2;
		MELFAS_binary = MELFAS_binary_2;
	}
#elif defined(CONFIG_USING_INNOTEK_PANEL_4_3) /* L1A */
	num_tx_line = 19;
	num_rx_line = 11;

	if (gpio_get_value(TS_MAKER_ID)) { /* Dummy Pattern */
		ts->pdata->fw_ver = 0x51;
		MELFAS_binary_nLength = MELFAS_binary_nLength_2;
		MELFAS_binary = MELFAS_binary_2;
	} else {
		ts->pdata->fw_ver = 0x0D;
		MELFAS_binary_nLength = MELFAS_binary_nLength_1;
		MELFAS_binary = MELFAS_binary_1;
	}

	if (lge_get_board_revno() == HW_REV_A) {
		ts->pdata->fw_ver = 65;
	}
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_master_send(ts->client, &buf[0], 1);
		if (ret >= 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] i2c_master_send() ok [%d]\n", ret);
			break;
		} else {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] i2c_master_send() failed[%d]\n", ret);
#ifdef andrew_download
			if (i == I2C_RETRY_CNT-1) {
				ret = mms100_download(check_type, embedded_img); /*check FW status */
				if (ret == 0) {
					ret = mms100_download(isp_type, embedded_img);
					MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[touch] touch fw update success \n");
				} else {
					MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] no touch panel \n");
					return ret ;
				}
			}
#endif
		}
	}

	buf[0] = TS_READ_VERSION_ADDR;
	ret = i2c_master_send(ts->client, &buf[0], 1);
	ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

	printk(KERN_INFO "\n= Melfas Version Info =\n");
	printk(KERN_INFO "Release FW Version :: %d, HW Revision :: %d, HW Compatibility GR :: %c\n", buf[0], buf[1], buf[2]);
	printk(KERN_INFO "Core Version :: %d, Private Custom Version :: %d, Public Custom Version :: %d\n", buf[3], buf[4], buf[5]);
	printk(KERN_INFO "Image's Core Version :: %d, Image's HW Revision :: %d\n\n", MELFAS_binary[29696], MELFAS_binary[29697]);

	ts->version = buf[FW_VERSION_ADDR];

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO,
		"[TOUCH] i2c_master_send() [%d], Add[%d]\n", ret, ts->client->addr);

#if SET_DOWNLOAD_BY_GPIO
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) /* d1lv */
	if (lge_get_board_revno() == HW_REV_C || lge_get_board_revno() == HW_REV_D) {
		if (((buf[0] < ts->pdata->fw_ver) || buf[0] == 0xF0 || buf[0] == 0xFF) && buf[2] != 'B') {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] : download start \n");
			mms100_download(isp_type, embedded_img);

			buf[0] = TS_READ_VERSION_ADDR;
			ret = i2c_master_send(ts->client, &buf[0], 1);
			ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

			ts->version = buf[FW_VERSION_ADDR];
		}
	}
	if (lge_get_board_revno() >= HW_REV_1_0) {
		if (buf[0] < ts->pdata->fw_ver || buf[0] > 0xA0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] : download start \n");
			mms100_download(isp_type, embedded_img);

			buf[0] = TS_READ_VERSION_ADDR;
			ret = i2c_master_send(ts->client, &buf[0], 1);
			ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

			ts->version = buf[FW_VERSION_ADDR];
		}
	}
	memset(ts->group_version, 0, 10*sizeof(char));
	snprintf(ts->group_version, 10, "%c_%d", buf[2], buf[0]);
#elif defined(CONFIG_USING_TOVIS_PANEL) /* d1lk */
/*	if (lge_get_board_revno() >= HW_REV_C) { //Check it later by andrew*/
#ifdef andrew_download
		if (buf[0] < ts->pdata->fw_ver || buf[0] == 0xF0 || buf[0] == 0xF0) {   /* update only Rev C ITO case */
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] : download start \n");
			mms100_download(isp_type, embedded_img);

			buf[0] = TS_READ_VERSION_ADDR;
			ret = i2c_master_send(ts->client, &buf[0], 1);
			ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

			ts->version = buf[FW_VERSION_ADDR];
		}
#endif
/*	} // check it later by andrew*/
#elif defined(CONFIG_USING_INNOTEK_PANEL_4_3) /* L1A */
	if (lge_get_board_revno() >= HW_REV_B) {
		if (buf[0] != ts->pdata->fw_ver) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] : download start \n");
			mms100_download(isc_type, embedded_img);

			buf[0] = TS_READ_VERSION_ADDR;
			ret = i2c_master_send(ts->client, &buf[0], 1);
			ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

			ts->version = buf[FW_VERSION_ADDR];
		}
		if (ts->version > 0x09) {  /* Double Resolution */
			m = 2;
		}
	}
#elif defined(CONFIG_USING_INNOTEK_PANEL_3_5)  /* L0 MPCS */
	if (buf[0] != ts->pdata->fw_ver) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] : download start \n");
		mms100_download(isc_type, embedded_img);

		buf[0] = TS_READ_VERSION_ADDR;
		ret = i2c_master_send(ts->client, &buf[0], 1);
		ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

		ts->version = buf[FW_VERSION_ADDR];
	}
	/* LGE_CHANGE
	 * 2012-06-09, sangyeol.ryu@lge.com
	 * use double resoultion for touch drag performance
	 * in case of over 0x13(touch sw version)
	 */
	if (ts->version >= 0x13) {
		m = 2;
	}
#else
	if (buf[0] != ts->pdata->fw_ver) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] : download start \n");
		mms100_download(isc_type, embedded_img);

		buf[0] = TS_READ_VERSION_ADDR;
		ret = i2c_master_send(ts->client, &buf[0], 1);
		ret = i2c_master_recv(ts->client, &buf[0], TS_READ_VERSION_INFO_LEN);

		ts->version = buf[FW_VERSION_ADDR];
	}
#endif
#endif /* SET_DOWNLOAD_BY_GPIO end */

#if defined(CONFIG_USING_INNOTEK_PANEL_4_7)  /*temporally added for 3 key Rev.A*/
	if (lge_get_board_revno() == HW_REV_A && ts->version == 2) {
			ts->pdata->button[0] = KEY_BACK;
			ts->pdata->button[2] = KEY_MENU;
	}
#endif

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] Not enough memory\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "melfas-ts" ;

	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);

	for (i = 0; i < ts->pdata->num_of_button; i++)
		ts->input_dev->keybit[BIT_WORD(ts->pdata->button[i])] |= BIT_MASK(ts->pdata->button[i]);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,  ts->pdata->x_max * m, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,  ts->pdata->y_max * m, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 40, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH - 1, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
			"[TOUCH] Failed to register device\n");
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}


	if (ts->client->irq) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
			"[TOUCH] trying to request irq: %s-%d\n", ts->client->name, ts->client->irq);
		ret = request_threaded_irq(client->irq, NULL, melfas_ts_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_LOW, ts->client->name, ts);

		if (ret > 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
				"[TOUCH] Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
			ret = -EBUSY;
			goto err_request_irq;
		}
	}

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] succeed to register input device\n");

	for (i = 0; i < ARRAY_SIZE(mms136_device_attrs); i++) {
		ret = device_create_file(&client->dev, &mms136_device_attrs[i]);
		if (ret) {
			goto err_request_irq;
		}
	}

	for (i = 0; i < MELFAS_MAX_TOUCH; i++) {
		g_Mtouch_info[i].btn_touch = -1;
		tmp_flag[i] = 1;
	}

#ifdef CONFIG_TS_INFO_CLASS
	ts->cdev.name = "touchscreen";
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7)
	ts->cdev.version = ts->group_version;
#else
	ts->cdev.version = ts->version;
#endif
	ts->cdev.flags = ts->flags;

	ts_info_classdev_register(&client->dev, &ts->cdev);
#endif

#if CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = melfas_ts_early_suspend;
	ts->early_suspend.resume = melfas_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO,
			"[TOUCH] Start touchscreen. name: %s, irq: %d\n",
			ts->client->name, ts->client->irq);
	return 0;

err_request_irq:
	printk(KERN_ERR "melfas-ts: err_request_irq failed\n");
	free_irq(client->irq, ts);
err_input_register_device_failed:
	printk(KERN_ERR "melfas-ts: err_input_register_device failed\n");
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	printk(KERN_ERR "melfas-ts: err_input_dev_alloc failed\n");
err_alloc_data_failed:
	printk(KERN_ERR "melfas-ts: err_alloc_data failed_\n");
err_check_functionality_failed:
	printk(KERN_ERR "melfas-ts: err_check_functionality failed_\n");

	return ret;
}

/* ESD mode change for D1LV and D1LK */
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_TOVIS_PANEL)
static void melfas_ts_esd_change(struct melfas_ts_data *ts)
{
	int ret;
	uint8_t esd_buf[2];

	esd_buf[0] = 0x01;
	esd_buf[1] = 0x80;

	ret = i2c_master_send(ts->client, esd_buf, 2);
	ret = i2c_master_send(ts->client, esd_buf, 1);
	ret = i2c_master_recv(ts->client, &esd_buf[0], 1);

	if (esd_buf[0] & 0x80)
		printk("esd 2\n");
	else
		printk("esd 1\n");
	msleep(700);
}
#endif

#if defined(CONFIG_LGE_PM)
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_INNOTEK_PANEL_3_5)
static void melfas_ts_noise_mode_change(struct melfas_ts_data *ts)
{
	uint8_t noise_buf[2];

	noise_buf[0] = 0x6f;
	if (pm8921_charger_is_ta_connected() == 0) {
		printk("[TOUCH] ta is disconnected!!\n");
		noise_buf[1] = 0x0;
	} else {
		printk("[TOUCH] ta is connected!!\n");
		noise_buf[1] = 0x55;
	}

	i2c_master_send(ts->client, noise_buf, 2);
}
#endif
#endif

static int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
#ifdef CONFIG_TS_INFO_CLASS
	ts_info_classdev_unregister(&ts->cdev);
#endif
	kfree(ts);
	return 0;
}

static void release_all_fingers(struct melfas_ts_data *ts)
{
	int i;
	for (i = 0; i < ts->pdata->num_of_finger; i++) {
		if (g_Mtouch_info[i].btn_touch < 0)
			continue;

		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].width);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
		input_mt_sync(ts->input_dev);

		g_Mtouch_info[i].posX = 0;
		g_Mtouch_info[i].posY = 0;
		g_Mtouch_info[i].strength = 0;
		g_Mtouch_info[i].btn_touch = -1;
		tmp_flag[i] = 1;
	}
	input_sync(ts->input_dev);
	touch_prestate = 0;
}

static void release_all_keys(struct melfas_ts_data *ts)
{
	int i;

	for (i = 0; i < MELFAS_MAX_BTN; i++) {
		printk("g_btn_info status : %d, key_code : %d\n", g_btn_info[i].status , g_btn_info[i].key_code);
		if (g_btn_info[i].status <= 0)
			continue;
		input_report_key(ts->input_dev, g_btn_info[i].key_code, RELEASE_KEY);
	}
	btn_prestate = RELEASE_KEY;
	input_sync(ts->input_dev);
}

static void melfas_ts_sw_reset(struct melfas_ts_data *ts)
{
	printk(KERN_ERR "[TOUCH] sw reset!!!! \n");

	release_all_fingers(ts);
	release_all_keys(ts);
	ts->pdata->power_enable(0, true);
	ts->pdata->power_enable(1, true);
	msleep(20);

/* ESD mode change for D1LV and D1LK */
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_TOVIS_PANEL)
	if (esd_mode_change) {
		printk("[TOUCH] esd mode change in sw_reset\n");
		melfas_ts_esd_change(ts);
	}
#endif
}

static void melfas_ts_suspend_func(struct melfas_ts_data *ts)
{
	int ret;

	printk(KERN_INFO "[TOUCH] suspend start \n");

	ret = ts->pdata->power_enable(0, true);
	if (ret < 0)
		printk(KERN_ERR "[TOUCH] suspend: i2c_smbus_write_byte_data failed\n");

	/* move release timing */
	release_all_fingers(ts);
	release_all_keys(ts);

	printk(KERN_INFO "[TOUCH] suspend end \n");

}

static void melfas_ts_resume_func(struct melfas_ts_data *ts)
{
	int ret = 0;

	printk(KERN_INFO "[TOUCH] resume start \n");

	ret = ts->pdata->power_enable(1, true);

/* ESD mode change for D1LV and D1LK */
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_TOVIS_PANEL)
	if (esd_mode_change) {
		printk("[TOUCH] esd mode change in resume\n");
		melfas_ts_esd_change(ts);
	}
#endif

#if defined(CONFIG_LGE_PM)
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7) || defined(CONFIG_USING_INNOTEK_PANEL_3_5)
	msleep(400);
	melfas_ts_noise_mode_change(ts);
	ac_change = pm8921_charger_is_ta_connected();
#endif
#endif
	printk(KERN_INFO "[TOUCH] resume end \n");

}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_suspend_func(ts);
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_resume_func(ts);
}
#endif


static struct of_device_id melfas_match_table[] = {
	{ .compatible = "melfas,mms136",},
	{ },
};

static const struct i2c_device_id melfas_ts_id[] = {
	{ MELFAS_TS_NAME, 0 },
	{ }
};

static struct i2c_driver melfas_ts_driver = {
	.driver		= {
		.name	= MELFAS_TS_NAME,
		.of_match_table = melfas_match_table,
	},
	.id_table		= melfas_ts_id,
	.probe		= melfas_ts_probe,
	.remove		= __devexit_p (melfas_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= melfas_ts_suspend,
	.resume		= melfas_ts_resume,
#endif
};

static void async_melfas_ts_init(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&melfas_ts_driver);
	if (ret < 0) {
		printk(KERN_ERR "[TOUCH]failed to i2c_add_driver\n");
	}

	pr_info("%s end\n", __func__);
	return;
}

static int __devinit melfas_ts_init(void)
{
	async_schedule(async_melfas_ts_init, NULL);
	return 0;
}

static void __exit melfas_ts_exit(void)
{
	i2c_del_driver(&melfas_ts_driver);

}

MODULE_DESCRIPTION("Driver for Melfas MTSI Touchscreen Controller");
MODULE_AUTHOR("MinSang, Kim <kimms@melfas.com>");
MODULE_LICENSE("GPL");

module_init(melfas_ts_init);
module_exit(melfas_ts_exit);
