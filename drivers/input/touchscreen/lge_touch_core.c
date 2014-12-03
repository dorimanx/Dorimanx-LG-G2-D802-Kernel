/* Lge_touch_core.c
 *
 * Copyright (C) 2011 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
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


#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#if defined(CONFIG_FB)
#include <linux/lcd_notify.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/jiffies.h>
#include <linux/sysdev.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/version.h>

#include <asm/atomic.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <mach/board.h>
#include <linux/regulator/consumer.h>
#include <linux/zwait.h>

#include <linux/input/lge_touch_core.h>
#define CUST_G2_TOUCH_WAKEUP_GESTURE
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
#include <linux/wakelock.h>
#include <linux/mutex.h>

static int touch_gesture_enable = 0;
static struct wake_lock touch_wake_lock;
static struct mutex i2c_suspend_lock;
#ifndef CONFIG_LGE_SECURITY_KNOCK_ON
static void touch_double_tap_wakeup_enable(struct lge_touch_data *ts);
#endif
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
static void touch_multi_tap_wakeup_enable(struct lge_touch_data *ts, int value);
void send_uevent_lpwg(int type);
#endif
#endif

#ifdef CUST_G2_TOUCH
static int f54_show_raw_data = 0;
struct i2c_client *ds4_i2c_client;
extern void SCAN_PDT(void);
extern int F54Test(int input);
extern char f54_wlog_buf[6000];
int f54_window_crack_check_mode = 0;
int f54_window_crack = 0;
static int ts_suspend = 0;
int thermal_status = 0;
extern int touch_thermal_mode;
#endif

#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
static int knockon_wakeup_uevent_reporting = 0;
#endif

#if defined(Z_GLOVE_TOUCH_SUPPORT)
static int set_glove_finger_enable(struct lge_touch_data *ts, int onoff);
static int glove_finger_enable = 0;
#endif
#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI)&& !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
int ime_drumming_status = 0;
int keyguard_status = 0;
#endif

#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
int quick_cover_status = 0;
#endif
struct touch_device_driver*	touch_device_func;
struct workqueue_struct*	touch_wq;

struct lge_touch_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct lge_touch_data *ts, char *buf);
	ssize_t (*store)(struct lge_touch_data *ts, const char *buf, size_t count);
};

#define LGE_TOUCH_ATTR(_name, _mode, _show, _store)	\
struct lge_touch_attribute lge_touch_attr_##_name = __ATTR(_name, _mode, _show, _store)

/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/lge_touch_core/parameters/debug_mask
 */
#if defined(A1_only)
u32 touch_debug_mask = DEBUG_BASE_INFO | DEBUG_NOISE;
#else
u32 touch_debug_mask = DEBUG_BASE_INFO;
#endif
module_param_named(debug_mask, touch_debug_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

#ifdef LGE_TOUCH_TIME_DEBUG
/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/lge_touch_core/parameters/time_debug_mask
 */
u32 touch_time_debug_mask = DEBUG_TIME_PROFILE_NONE;
module_param_named(time_debug_mask, touch_time_debug_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

struct timeval t_debug[TIME_PROFILE_MAX];
#endif

#ifdef CUST_G2_TOUCH
#define get_time_interval(a,b) a>=b ? a-b : 1000000+a-b
struct timeval t_ex_debug[TIME_EX_PROFILE_MAX];
#endif

#define MAX_RETRY_COUNT			3
#define MAX_GHOST_CHECK_COUNT	3
#define GHOST_EDGE_ZONE_INTERVAL	40
#define IRQ_WAKE_CHECK    0

#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
#define TOUCH_BUTTON_ENABLE_Y_POSITION 1899
#endif

#if defined(CONFIG_FB)
static int lcd_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void touch_early_suspend(struct early_suspend *h);
static void touch_late_resume(struct early_suspend *h);
#endif

/* Auto Test interface for some model */
struct lge_touch_data *touch_test_dev = NULL;
EXPORT_SYMBOL(touch_test_dev);
static void release_all_ts_event(struct lge_touch_data *ts);
/* Jitter Filter
 *
 */
#define jitter_abs(x)		(x > 0 ? x : -x)
#define jitter_sub(x, y)	(x > y ? x - y : y - x)

int ts_charger_plug = 0;
int ts_charger_type = 0;
static void safety_reset(struct lge_touch_data *ts);
static int touch_ic_init(struct lge_touch_data *ts);
static struct hrtimer hr_touch_trigger_timer;
#define MS_TO_NS(x)	(x * 1E6L)

static bool touch_irq_wake = 0;
static int touch_enable_irq_wake(struct lge_touch_data *ts)
{
	int ret = 0;
	if(IRQ_WAKE_CHECK)
		BUG_ON(!mutex_is_locked(&ts->irq_work_mutex));
	if(!touch_irq_wake){
		touch_irq_wake = 1;
		ret= enable_irq_wake(ts->client->irq);
	}
	else
		TOUCH_INFO_MSG("touch_enable_irq_wake!!!\n");
	return ret;
}
static int touch_disable_irq_wake(struct lge_touch_data *ts)
{
	int ret = 0;
	if(IRQ_WAKE_CHECK)
		BUG_ON(!mutex_is_locked(&ts->irq_work_mutex));
	if(touch_irq_wake){
		touch_irq_wake = 0;
		ret = disable_irq_wake(ts->client->irq);
	}
	else
		TOUCH_INFO_MSG("touch_disable_irq_wake!!!\n");
	return ret;
}

#ifdef CUST_G2_TOUCH
void check_touch_xo_therm(int mode){
	if(mode){
		thermal_status = 1;
		TOUCH_INFO_MSG("The Temperature is above 50 degree. Change touch FW !! \n");
#if defined(A1_only) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
		queue_delayed_work(touch_wq, &touch_test_dev->work_thermal, msecs_to_jiffies(10));
#endif
	} else{
		thermal_status = 0;
		TOUCH_INFO_MSG("The Temperature is below 47 degree. Change touch FW !! \n");
#if defined(A1_only) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
		queue_delayed_work(touch_wq, &touch_test_dev->work_thermal, msecs_to_jiffies(10));
#endif
	}
}

static void change_fw_func(struct work_struct *work_thermal) {
	struct lge_touch_data *ts = container_of(to_delayed_work(work_thermal), struct lge_touch_data, work_thermal);
	u8 min_peak[2] = {0};

	if (ts->curr_pwr_state == POWER_OFF)
		return;

	if(thermal_status){
#ifdef CONFIG_LGE_Z_TOUCHSCREEN
		min_peak[0] = 0x11;
		min_peak[1] = 0x3c;

		if (touch_i2c_write(ts->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0){
			TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
		} else {
			TOUCH_INFO_MSG("%s : set min peak 60 \n", __func__);
		}
#else
		switch(ts->fw_info.fw_setting.curr_touch_vendor) {
			case TOUCH_VENDOR_TPK:
				min_peak[0] = 0x0d;
				min_peak[1] = 0x3c;

				if(ts_charger_plug == 0){
					if (touch_i2c_write(ts->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0){
						TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
					} else
						TOUCH_INFO_MSG("%s : TPK set min peak 60 \n", __func__);
				}
				break;
			case TOUCH_VENDOR_LGIT:
				min_peak[0] = 0x0d;
#ifdef CONFIG_MACH_MSM8974_G2_KDDI
                min_peak[1] = 0x28;
#else
				min_peak[1] = 0x20;
#endif
				if(ts_charger_plug == 0){
					if (touch_i2c_write(ts->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0){
						TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
					} else
						TOUCH_INFO_MSG("%s : LGIT set min peak 40 \n", __func__);
				}
				break;
			default:
				break;
		}
#endif
	} else {
#ifdef CONFIG_LGE_Z_TOUCHSCREEN
		min_peak[0] = 0x11;
		min_peak[1] = 0x13;

		if (touch_i2c_write(ts->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0){
			TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
		} else {
			TOUCH_INFO_MSG("%s : set min peak 19 \n", __func__);
		}
#else
		switch(ts->fw_info.fw_setting.curr_touch_vendor) {
			case TOUCH_VENDOR_TPK:
				min_peak[0] = 0x0d;
				min_peak[1] = 0x0f;

				if(ts_charger_plug == 0){
					if (touch_i2c_write(ts->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0){
						TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
					} else
						TOUCH_INFO_MSG("%s : TPK set min peak 15 \n", __func__);
				}
				break;
			case TOUCH_VENDOR_LGIT:
				min_peak[0] = 0x0d;
				min_peak[1] = 0x0f;

				if(ts_charger_plug == 0){
					if (touch_i2c_write(ts->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0){
						TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
					} else
						TOUCH_INFO_MSG("%s : LGIT set min peak 15 \n", __func__);
				}
				break;
			default:
				break;
		}
#endif
	}
}

#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
static void change_ime_drumming_func(struct work_struct *work_ime_drumming) {
	struct lge_touch_data *ts = container_of(to_delayed_work(work_ime_drumming), struct lge_touch_data, work_ime_drumming);
#ifdef CONFIG_LGE_Z_TOUCHSCREEN
	u8 small_finger_amp_th[2] = {0};
	if (ts->curr_pwr_state == POWER_OFF)
		return;
	
	if((ts_charger_plug) && (keyguard_status == 9) && ime_drumming_status) {
		small_finger_amp_th[0]=0x67;
		small_finger_amp_th[1]=0x39;
		if(touch_i2c_write(ts->client, SMALL_FINGER_AMPLITUDE_THRESHOLD_REG, 2, small_finger_amp_th) < 0) {
			TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
		} else {
			TOUCH_INFO_MSG("%s 0x39\n", __func__);
		}
	}else{
		small_finger_amp_th[0]=0x67;
		small_finger_amp_th[1]=0x31;
		if(touch_i2c_write(ts->client, SMALL_FINGER_AMPLITUDE_THRESHOLD_REG, 2, small_finger_amp_th) < 0) {
			TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
		} else {
			TOUCH_INFO_MSG("%s 0x31\n", __func__);
		}
	}
	return;
#else
	u8 udata[5] = {0};
	/* udata[0] = Noise Floor						- panel dependent
	   udata[1] = Minimum Peak Amplitude			- xo_therm dependent
	   udata[2] = Peak Merge Threshold 				- need to be modified for IME drumming issue
	   udata[3] = Drumming Acceleration Threshold	- need to be modified for IME drumming issue
	   udata[4] = Minimum Drumming Separation		- need to be modified for IME drumming issue
	*/

	if (ts->curr_pwr_state == POWER_OFF)
		return;

	switch(ts->fw_info.fw_setting.curr_touch_vendor) {
		case TOUCH_VENDOR_TPK:
			udata[0] = 0x0d;

			if(thermal_status == 1 || ts_charger_plug == 1) {
				udata[1] = 0x3c;
			} else {
				udata[1] = 0x0f;
			}
			break;

		case TOUCH_VENDOR_LGIT:
			udata[0] = 0x0d;

			if(thermal_status == 1 || ts_charger_plug == 1) {
				udata[1] = 0x28;
			} else {
				udata[1] = 0x0f;
			}
			break;

		default:
			break;
	}

	if(ime_drumming_status) {
		udata[2] = 0xc0;
		udata[3] = 0x08;
		udata[4] = 0x01;

		if(touch_i2c_write(ts->client, DRUMMING_THRESH_N_DISTANCE_REG, sizeof(udata), udata) < 0) {
			TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
		} else {
			TOUCH_INFO_MSG("%s : Peak Merge Threshold            : 192\n", __func__);
			TOUCH_INFO_MSG("%s : Drumming Acceleration Threshold : 1\n", __func__);
			TOUCH_INFO_MSG("%s : Min Drumming Distance           : 1\n", __func__);
		}
	} else {
		udata[2] = 0x80;
		udata[3] = 0x0f;
		udata[4] = 0x0a;

		if(touch_i2c_write(ts->client, DRUMMING_THRESH_N_DISTANCE_REG, sizeof(udata), udata) < 0) {
			TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
		} else {
			TOUCH_INFO_MSG("%s : Peak Merge Threshold            : 128 (default)\n", __func__);
			TOUCH_INFO_MSG("%s : Drumming Acceleration Threshold :  15 (default)\n", __func__);
			TOUCH_INFO_MSG("%s : Min Drumming Distance           :  10 (default)\n", __func__);
		}
	}
#endif
}
#endif
#endif

static enum hrtimer_restart touch_trigger_timer_handler(struct hrtimer *timer)
{
	if (touch_test_dev && touch_test_dev->pdata->role->ghost_detection_enable) {
		if(touch_test_dev->gd_ctrl.init_data.ta_reset==1 && atomic_read(&touch_test_dev->device_init) == 1)
		{
			touch_test_dev->gd_ctrl.init_data.ta_reset = 2;
			atomic_inc(&touch_test_dev->next_work);
			queue_work(touch_wq, &touch_test_dev->work);
		}
	}
	return HRTIMER_NORESTART;
}

void trigger_baseline_state_machine(int plug_in, int type)
{
#if defined(A1_only)
	u8 buf=0;
#if !defined(CONFIG_MACH_MSM8974_G2_KDDI) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM)  && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	u8 min_peak[2] = {0};
#endif
#endif

	if (touch_test_dev && touch_test_dev->pdata->role->ghost_detection_enable) {

	    if(plug_in == 0 || plug_in == 1)
	    {
			if(touch_test_dev->curr_pwr_state == POWER_ON && !touch_test_dev->fw_info.fw_upgrade.is_downloading) {
				if(plug_in ==0){
#if defined(A1_only)
					if(touch_i2c_read(touch_test_dev->client, CHARGER_CONNECTED_REG, 1, &buf) < 0) {
						TOUCH_ERR_MSG("CHARGER_CONNECTED_REG read fail");
						return;
					} else {
						buf = buf & 0xDF;
						if(touch_i2c_write_byte(touch_test_dev->client, CHARGER_CONNECTED_REG, buf) < 0) {
							TOUCH_ERR_MSG("CHARGER_CONNECTED_REG write fail");
							return;
						}
					}
#if !defined(CONFIG_MACH_MSM8974_G2_KDDI) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM)  && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
					switch(touch_test_dev->fw_info.fw_setting.curr_touch_vendor) {
						case TOUCH_VENDOR_TPK:
							min_peak[0] = 0x0d;
							min_peak[1] = 0x0f;

							if(thermal_status == 0){
								if (touch_i2c_write(touch_test_dev->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0){
									TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
								} else
									TOUCH_INFO_MSG("%s : TPK set min peak 15 \n", __func__);
							}
							break;
						case TOUCH_VENDOR_LGIT:
							min_peak[0] = 0x0d;
							min_peak[1] = 0x0f;

							if(thermal_status == 0){
								if (touch_i2c_write(touch_test_dev->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0) {
									TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
								} else
									TOUCH_INFO_MSG("%s : LGIT set min peak 15 \n", __func__);
							}
							break;
						default:
							break;
					}
#endif
#endif
				} else if(plug_in ==1){
#if defined(A1_only)
					if(touch_i2c_read(touch_test_dev->client, CHARGER_CONNECTED_REG, 1, &buf) < 0) {
						TOUCH_ERR_MSG("CHARGER_CONNECTED_REG read fail");
						return;
					} else {
						buf = buf | 0x20;
						if(touch_i2c_write_byte(touch_test_dev->client, CHARGER_CONNECTED_REG, buf) < 0) {
							TOUCH_ERR_MSG("CHARGER_CONNECTED_REG write fail");
							return;
						}
					}
#if !defined(CONFIG_MACH_MSM8974_G2_KDDI) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM)  && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
					switch(touch_test_dev->fw_info.fw_setting.curr_touch_vendor) {
						case TOUCH_VENDOR_TPK:
							min_peak[0] = 0x0d;
							min_peak[1] = 0x3c;

							if(thermal_status == 0){
								if (touch_i2c_write(touch_test_dev->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0) {
									TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
								} else
									TOUCH_INFO_MSG("%s : TPK set min peak 60 \n", __func__);
							}
							break;
						case TOUCH_VENDOR_LGIT:
							min_peak[0] = 0x0d;
							min_peak[1] = 0x28;

							if(thermal_status == 0){
								if (touch_i2c_write(touch_test_dev->client, MINIMUM_PEAK_AMPLITUDE_REG, sizeof(min_peak), min_peak) < 0) {
									TOUCH_ERR_MSG("%s : Touch i2c write fail !! \n", __func__);
								} else
									TOUCH_INFO_MSG("%s : LGIT set min peak 40 \n", __func__);
							}
							break;
						default:
							break;
					}
#endif
#endif
				}
			}
			ts_charger_type = type;
			TOUCH_INFO_MSG(" trigger_baseline_state_machine = %d type = %d \n", plug_in, type);
			ts_charger_plug = plug_in;

			if( touch_test_dev->gd_ctrl.init_data.ta_reset==0 && plug_in ==1){
				touch_test_dev->gd_ctrl.init_data.ta_reset = 1;

				hrtimer_start(&hr_touch_trigger_timer, ktime_set(0, MS_TO_NS(1000)), HRTIMER_MODE_REL);
			}
		}
	}
}

bool chk_time_interval(struct timeval t_aft, struct timeval t_bef, int t_val)
{
	if( t_aft.tv_sec - t_bef.tv_sec == 0 ) {
		if((get_time_interval(t_aft.tv_usec, t_bef.tv_usec)) <= t_val)
			return true;
	} else if( t_aft.tv_sec - t_bef.tv_sec == 1 ) {
		if( t_aft.tv_usec + 1000000 - t_bef.tv_usec <= t_val)
			return true;
	}

	return false;
}

int ghost_detect_solution(struct lge_touch_data *ts)
{
	int i = 0;
	bool is_long_press = false;
	static int long_press_cnt;
	unsigned int ghost_detect_time = 50000;

#if defined(A1_only)
	if(ts->ts_data.total_num && !ts->gd_ctrl.init_data.start_palm) {
		ts->gd_ctrl.init_data.palm_num = 0;
		for(i=0; i < ts->pdata->caps->max_id; i++){
			if (ts->ts_data.curr_data[i].status == FINGER_PRESSED &&
				ts->ts_data.curr_data[i].object == PALM &&
				ts->ts_data.curr_data[i].pressure > 254) {
				ts->gd_ctrl.init_data.palm_num++;
				memcpy(&ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data, &ts->ts_data.curr_data[i], sizeof(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data));
			}
		}

		if(ts->gd_ctrl.init_data.palm_num == 1){
			ts->gd_ctrl.init_data.start_palm = 1;
			ts->gd_ctrl.init_data.palm_press_count = 0;
			ts->gd_ctrl.init_data.palm_num = 0;
			do_gettimeofday(&t_ex_debug[TIME_EX_PRESSURE_MAX_START_TIME]);
		}
	} else {
		if(!ts->ts_data.total_num) {
			ts->gd_ctrl.init_data.start_palm = 0;
			ts->gd_ctrl.init_data.palm_press_count = 0;
			ts->gd_ctrl.init_data.palm_num = 0;
			memset(&ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data, 0x0, sizeof(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data));
		} else {
			do_gettimeofday(&t_ex_debug[TIME_EX_CURR_INT_TIME]);
			ts->gd_ctrl.init_data.palm_num = 0;
			for(i=0; i < ts->pdata->caps->max_id; i++){
				if (ts->ts_data.curr_data[i].status == FINGER_PRESSED &&
					ts->ts_data.curr_data[i].object == PALM &&
					ts->ts_data.curr_data[i].pressure > 254) {
					ts->gd_ctrl.init_data.palm_num++;
				}
			}

			if(ts->gd_ctrl.init_data.palm_num == 1) {
				if( t_ex_debug[TIME_EX_CURR_INT_TIME].tv_sec - t_ex_debug[TIME_EX_PRESSURE_MAX_START_TIME].tv_sec <= 3) {
					for(i=0; i < ts->pdata->caps->max_id; i++) {
						if(ts->ts_data.curr_data[i].status == FINGER_PRESSED &&
						   ts->ts_data.curr_data[i].object == PALM &&
						   ts->ts_data.curr_data[i].pressure > 254
						) {
							if(jitter_sub(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data.x_position,ts->ts_data.curr_data[i].x_position)<=20 &&
						       jitter_sub(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data.y_position,ts->ts_data.curr_data[i].y_position)<=20) {
									ts->gd_ctrl.init_data.palm_press_count++;
							}
							memcpy(&ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data, &ts->ts_data.curr_data[i], sizeof(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data));
							break;
						}
					}

					if(ts->gd_ctrl.init_data.palm_press_count > 270) {
						ts->gd_ctrl.init_data.start_palm = 0;
						ts->gd_ctrl.init_data.palm_press_count = 0;
						ts->gd_ctrl.init_data.palm_num = 0;
						memset(&ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data, 0x0, sizeof(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data));
						TOUCH_INFO_MSG("GHOST_PALM_LONG_PRESS\n");
						return -EIO;
					}
				} else {
					ts->gd_ctrl.init_data.start_palm = 0;
					ts->gd_ctrl.init_data.palm_press_count = 0;
					ts->gd_ctrl.init_data.palm_num = 0;
					memset(&ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data, 0x0, sizeof(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data));
				}
			} else {
				ts->gd_ctrl.init_data.start_palm = 0;
				ts->gd_ctrl.init_data.palm_press_count = 0;
				ts->gd_ctrl.init_data.palm_num = 0;
				memset(&ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data, 0x0, sizeof(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data));
			}
		}
	}
#endif

	if(ts->gd_ctrl.init_data.mask & GHOST_FIRST_IRQ) {
		if(ts->gd_ctrl.is_resume) {
			ts->gd_ctrl.is_resume = 0;
			do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_INT_TIME]);

			if(chk_time_interval(t_ex_debug[TIME_EX_FIRST_INT_TIME], t_ex_debug[TIME_EX_INIT_TIME], 200000) &&
				ts->ts_data.total_num) {
				TOUCH_INFO_MSG("GHOST_FIRST_IRQ\n");
				ts->gd_ctrl.init_data.ghost_detection = 1;
			}
		}
	}

	if(ts->gd_ctrl.init_data.mask & GHOST_LONG_PRESS) {
		do_gettimeofday(&t_ex_debug[TIME_EX_CURR_INT_TIME]);
		if( t_ex_debug[TIME_EX_CURR_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec >= 10) {
			if(ts->pdata->role->report_mode == REDUCED_REPORT_MODE) {
				if(touch_device_func->ic_ctrl) {
					if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_REPORT_MODE, ts->pdata->role->report_mode) < 0){
						TOUCH_ERR_MSG("IC_CTRL_REPORT_MODE handling fail\n");
						return 0;
					}
				}
			}
		} else {
			if (!ts->ts_data.total_num) long_press_cnt = 0;

			for (i = 0; i < ts->pdata->caps->max_id; i++) {
				if (ts->ts_data.curr_data[i].status == FINGER_PRESSED) {
					if (ts->ts_data.prev_data[i].status == FINGER_PRESSED &&
						jitter_sub(ts->ts_data.prev_data[i].x_position,ts->ts_data.curr_data[i].x_position) < 10 &&
						jitter_sub(ts->ts_data.prev_data[i].y_position,ts->ts_data.curr_data[i].y_position) < 10) {
						is_long_press = true;
					}
				}
			}

			if (is_long_press) long_press_cnt++;
			else long_press_cnt = 0;

			if (long_press_cnt > 700) {
				long_press_cnt = 0;
				TOUCH_INFO_MSG("GHOST_LONG_PRESS\n");
				goto out_need_to_rebase;
			}
		}
	}

	if(ts->gd_ctrl.init_data.mask & GHOST_CALL_STATE) {
		if(ts->gf_ctrl.incoming_call && (ts->ts_data.total_num > 1)) {
			TOUCH_INFO_MSG("GHOST_CALL_STATE\n");
			goto out_need_to_rebase;
		}
	}

	if(ts->gd_ctrl.init_data.mask & GHOST_TA_DEBOUNCE) {
		if (ts_charger_plug) {
			ghost_detect_time = 70000;
			if(ts->ts_data.total_num > 1 &&
			   ts->ts_data.total_num > ts->ts_data.prev_total_num &&
			   ts->gd_ctrl.init_data.ta_debouncing_count < 2) {
				for(i=0; i < ts->pdata->caps->max_id; i++){
					if ( ts->ts_data.prev_data[i].status == FINGER_RELEASED &&
						ts->ts_data.curr_data[i].status == FINGER_PRESSED){
						TOUCH_INFO_MSG("BOUNCING id=%d!!\n", i);
						ts->gd_ctrl.init_data.ta_debouncing_count++;
						memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));
						return -EAGAIN;
					}
				}
			} else {
				ts->gd_ctrl.init_data.ta_debouncing_count = 0;
			}
		} else {
			ghost_detect_time = 60000;
			if(ts->ts_data.total_num > 1 &&
			   ts->ts_data.total_num > ts->ts_data.prev_total_num &&
			   ts->gd_ctrl.init_data.ta_debouncing_count < 1) {
				for(i=0; i < ts->pdata->caps->max_id; i++){
					if ( ts->ts_data.prev_data[i].status == FINGER_RELEASED &&
						ts->ts_data.curr_data[i].status == FINGER_PRESSED){
						TOUCH_INFO_MSG("BOUNCING id=%d!!\n", i);
						ts->gd_ctrl.init_data.ta_debouncing_count++;
						memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));
						return -EAGAIN;
					}
				}
			} else {
				ts->gd_ctrl.init_data.ta_debouncing_count = 0;
			}
		}
	}

	if(ts->gd_ctrl.init_data.mask & GHOST_PRESSURE) {
		if(ts->ts_data.total_num) {
			for (i = 0; i < ts->pdata->caps->max_id; i++) {
				if (ts->ts_data.curr_data[i].status == FINGER_PRESSED &&
					ts->ts_data.curr_data[i].pressure == 0) {
					TOUCH_INFO_MSG("GHOST_PRESSURE\n");
					ts->gd_ctrl.init_data.ghost_detection = 1;
					break;
				}
			}
		}
	}

	if(ts->gd_ctrl.init_data.mask & GHOST_TA_RESET) {
		if(ts->gd_ctrl.init_data.ta_reset==2) {
			goto out_need_to_rebase;
		}
	}

	if ((ts->ts_data.state != TOUCH_ABS_LOCK) &&(ts->ts_data.total_num)){

		if (ts->ts_data.prev_total_num != ts->ts_data.total_num)
		{
			if (ts->ts_data.prev_total_num <= ts->ts_data.total_num)
			{
				if (ts->gf_ctrl.stage == GHOST_STAGE_CLEAR || (ts->gf_ctrl.stage | GHOST_STAGE_1) || ts->gf_ctrl.stage == GHOST_STAGE_4)
					ts->ts_data.state = TOUCH_BUTTON_LOCK;

				for(i=0; i < ts->pdata->caps->max_id; i++){
					if (ts->ts_data.curr_data[i].status == FINGER_PRESSED
							&& ts->ts_data.prev_data[i].status == FINGER_RELEASED) {
						break;
					}
				}

				if ( i < ts->pdata->caps->max_id )
				{
					memcpy(&t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval));
					do_gettimeofday(&t_ex_debug[TIME_EX_CURR_PRESS_TIME]);

					if ( ts->ts_data.curr_data[i].x_position > 0 && ts->ts_data.curr_data[i].x_position < ts->pdata->caps->x_max &&
						1<= ts->ts_data.prev_total_num && 1<= ts->ts_data.total_num && jitter_sub(ts->gd_ctrl.init_data.ts_prev_finger_press_data.x_position,ts->ts_data.curr_data[i].x_position)<=10 && jitter_sub(ts->gd_ctrl.init_data.ts_prev_finger_press_data.y_position,ts->ts_data.curr_data[i].y_position)<=10 )
					{
						if(chk_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], ghost_detect_time)) {
							ts->gd_ctrl.init_data.ghost_detection = 1;
							ts->gd_ctrl.init_data.ghost_detection_count++;
						}
					}
					else if (ts->ts_data.curr_data[i].x_position > 0 && ts->ts_data.curr_data[i].x_position < ts->pdata->caps->x_max &&
							ts->ts_data.prev_total_num==0 && ts->ts_data.total_num==1 && jitter_sub(ts->gd_ctrl.init_data.ts_prev_finger_press_data.x_position,ts->ts_data.curr_data[i].x_position)<=10 && jitter_sub(ts->gd_ctrl.init_data.ts_prev_finger_press_data.y_position,ts->ts_data.curr_data[i].y_position)<=10 )
					{
						if(chk_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], ghost_detect_time)) {
							ts->gd_ctrl.init_data.ghost_detection = 1;
						}
					}
					else if ( 5 < jitter_sub(ts->ts_data.prev_total_num,ts->ts_data.total_num) )
					{
						 ts->gd_ctrl.init_data.ghost_detection = 1;
					}
					else; //do not anything

					memcpy(&ts->gd_ctrl.init_data.ts_prev_finger_press_data, &ts->ts_data.curr_data[i], sizeof(ts->gd_ctrl.init_data.ts_prev_finger_press_data));
				}
			}else{
					memcpy(&t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval));
					do_gettimeofday(&t_ex_debug[TIME_EX_CURR_INT_TIME]);

					if(chk_time_interval(t_ex_debug[TIME_EX_CURR_INT_TIME], t_ex_debug[TIME_EX_PREV_PRESS_TIME], 10999)) {
						ts->gd_ctrl.init_data.finger_subtraction_check_count++;
					} else {
						ts->gd_ctrl.init_data.finger_subtraction_check_count = 0;
					}

					if(4<ts->gd_ctrl.init_data.finger_subtraction_check_count){
						ts->gd_ctrl.init_data.finger_subtraction_check_count = 0;
						TOUCH_INFO_MSG("need_to_rebase finger_subtraction!!! \n");
						/* rebase is disabled. see TD 41871*/
						//goto out_need_to_rebase;
					}
			}
		}

	}else if (!ts->ts_data.total_num){
			ts->gd_ctrl.init_data.finger_subtraction_check_count = 0;
	}

	if(ts->gd_ctrl.init_data.mask & GHOST_BUTTON) {
		if (ts->ts_data.state != TOUCH_BUTTON_LOCK) {
			if (ts->work_sync_err_cnt > 0
					&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
				/* Do nothing */
			} else {
				if (ts->gd_ctrl.init_data.button_press_count ==0)
					do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME]);
				else
					do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME]);

				ts->gd_ctrl.init_data.button_press_count++;

				if (6 <= ts->gd_ctrl.init_data.button_press_count)
				{
					if(chk_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME], t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME], 100000)) {
						TOUCH_INFO_MSG("GHOST_BUTTON\n");
						goto out_need_to_rebase;
					} else {
						ts->gd_ctrl.init_data.button_press_count = 0;
					}
				} else {
					if(!chk_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME], t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME], 100000)) {
						ts->gd_ctrl.init_data.button_press_count = 0;
					}
				}
			}
		}
	}

	if(ts->gd_ctrl.init_data.mask & GHOST_EDGE_ZONE) {
		if(ts->ts_data.total_num > 0 && ts->gd_ctrl.init_data.in_edge_zone_id < 0){
			for(i=0; i < ts->pdata->caps->max_id; i++){
				if(ts->ts_data.curr_data[i].status == FINGER_PRESSED
					&& ts->ts_data.curr_data[i].pressure > 0
					&& ts->ts_data.curr_data[i].pressure < 45
					&& ((ts->ts_data.curr_data[i].x_position <= GHOST_EDGE_ZONE_INTERVAL && ts->ts_data.curr_data[i].x_position >= 0)
					|| (ts->ts_data.curr_data[i].x_position <= ts->pdata->caps->x_max && ts->ts_data.curr_data[i].x_position >= (ts->pdata->caps->x_max - GHOST_EDGE_ZONE_INTERVAL))))
				{
					do_gettimeofday(&t_ex_debug[TIME_EX_GHOST_EDGE_ZONE_START_TIME]);
					ts->gd_ctrl.init_data.in_edge_zone_id = i;
					ts->gd_ctrl.init_data.in_edge_zone_x_pos= ts->ts_data.curr_data[i].x_position;
					ts->gd_ctrl.init_data.in_edge_zone_y_pos= ts->ts_data.curr_data[i].y_position;
					//TOUCH_INFO_MSG("GHOST_in_EDGE_ZONE_START\n");
					break;
				}
			}
		} else if(ts->gd_ctrl.init_data.in_edge_zone_id >= 0){
			if(ts->ts_data.total_num > 0
				&& ts->ts_data.curr_data[ts->gd_ctrl.init_data.in_edge_zone_id].pressure > 0
				&& ts->ts_data.curr_data[ts->gd_ctrl.init_data.in_edge_zone_id].pressure < 45
				&& ts->ts_data.curr_data[ts->gd_ctrl.init_data.in_edge_zone_id].status == FINGER_PRESSED
				&& ts->ts_data.prev_total_num <= ts->ts_data.total_num
				&& (jitter_sub(ts->ts_data.curr_data[ts->gd_ctrl.init_data.in_edge_zone_id].x_position,ts->gd_ctrl.init_data.in_edge_zone_x_pos) < 25)
				&& (jitter_sub(ts->ts_data.curr_data[ts->gd_ctrl.init_data.in_edge_zone_id].y_position,ts->gd_ctrl.init_data.in_edge_zone_y_pos) < 25))
			{
				do_gettimeofday(&t_ex_debug[TIME_EX_GHOST_EDGE_ZONE_END_TIME]);
				if(!chk_time_interval(t_ex_debug[TIME_EX_GHOST_EDGE_ZONE_END_TIME], t_ex_debug[TIME_EX_GHOST_EDGE_ZONE_START_TIME], 1500000)) {
					TOUCH_INFO_MSG("GHOST_in_EDGE_ZONE!!!\n");
					goto out_need_to_rebase;
				}
				else
				{
					for(i=0; i < ts->pdata->caps->max_id; i++){
						if(ts->gd_ctrl.init_data.in_edge_zone_id != i
							&& ts->ts_data.curr_data[i].status == FINGER_PRESSED
							&& (ts->ts_data.curr_data[i].x_position > GHOST_EDGE_ZONE_INTERVAL && ts->ts_data.curr_data[i].x_position < (ts->pdata->caps->x_max - GHOST_EDGE_ZONE_INTERVAL))){
							ts->gd_ctrl.init_data.in_edge_zone_id = -1;
							ts->gd_ctrl.init_data.in_edge_zone_x_pos= -1;
							ts->gd_ctrl.init_data.in_edge_zone_y_pos= -1;
							//TOUCH_INFO_MSG("NOT_in_EDGE_ZONE_2\n");
							break;
						}
					}
				}
			}
			else
			{
				ts->gd_ctrl.init_data.in_edge_zone_id = -1;
				ts->gd_ctrl.init_data.in_edge_zone_x_pos= -1;
				ts->gd_ctrl.init_data.in_edge_zone_y_pos= -1;
				//TOUCH_INFO_MSG("NOT_in_EDGE_ZONE_1\n");
			}
		}
	}

	if(ts->gd_ctrl.init_data.ghost_detection && ts->ts_data.total_num == 0 && ts->ts_data.palm == 0) {
		TOUCH_INFO_MSG("GHOST REBASE 1\n");
		goto out_need_to_rebase;
	} else if (ts->gd_ctrl.init_data.ghost_detection && 3 <= ts->gd_ctrl.init_data.ghost_detection_count && ts->ts_data.palm == 0) {
		TOUCH_INFO_MSG("GHOST REBASE 2\n");
		goto out_need_to_rebase;
	}

	return 0;

out_need_to_rebase:
	ts->gd_ctrl.init_data.rebase_count++;
	ts->gd_ctrl.init_data.ta_reset = 0;
	ts->gd_ctrl.init_data.ghost_detection = 0;
	ts->gd_ctrl.init_data.ghost_detection_count = 0;
	memset(&ts->gd_ctrl.init_data.ts_prev_finger_press_data, 0x0, sizeof(ts->gd_ctrl.init_data.ts_prev_finger_press_data));
	ts->gd_ctrl.init_data.button_press_count = 0;
	ts->gd_ctrl.init_data.in_edge_zone_id = -1;
	ts->gd_ctrl.init_data.in_edge_zone_x_pos = -1;
	ts->gd_ctrl.init_data.in_edge_zone_y_pos= -1;
	ts->gd_ctrl.init_data.start_palm = 0;
	ts->gd_ctrl.init_data.palm_press_count = 0;
	ts->gd_ctrl.init_data.palm_num = 0;
	memset(&ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data, 0x0, sizeof(ts->gd_ctrl.init_data.ts_prev_palm_finger_press_data));

	if(ts->gd_ctrl.init_data.rebase_count==1) {
		do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME]);

		if((t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec) <= 3) {
			TOUCH_INFO_MSG("GHOST in 3 sec. Init\n");
			return -EIO;
		}
	} else {
		do_gettimeofday(&t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME]);

		if(((t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec) <= 5))
		{
				TOUCH_INFO_MSG("GHOST in 5 sec. Init\n");
				return -EIO;
		} else {
			ts->gd_ctrl.init_data.rebase_count = 1;
			memcpy(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME], &t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME], sizeof(struct timeval));
		}
	}
	release_all_ts_event(ts);
	memset(&ts->ts_data, 0, sizeof(ts->ts_data));
	if(touch_device_func->ic_ctrl){
		if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0){
			TOUCH_ERR_MSG("IC_CTRL_REBASE handling fail\n");
		}
	}
	msleep(50);
	if(ts->gd_ctrl.init_data.mask & GHOST_TA_RESET) {
		if(gpio_get_value(ts->pdata->int_pin) != 1){
			touch_device_func->data(ts->client, &ts->ts_data);
		}
	}
	return -EAGAIN;
}

void Send_Touch( unsigned int x, unsigned int y)
{
	if (touch_test_dev)
	{
		/* press */
#if !defined(MT_PROTOCOL_A)
		input_mt_slot(touch_test_dev->input_dev, 0);
		input_mt_report_slot_state(touch_test_dev->input_dev, MT_TOOL_FINGER, true);
#endif	/* !defined(MT_PROTOCOL_A) */
		input_report_abs(touch_test_dev->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_PRESSURE, 1);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(touch_test_dev->input_dev, ABS_MT_WIDTH_MINOR, 1);
#if defined(MT_PROTOCOL_A)
		input_mt_sync(touch_test_dev->input_dev);
#endif	/* defined(MT_PROTOCOL_A) */
		input_sync(touch_test_dev->input_dev);

		/* release */
#if !defined(MT_PROTOCOL_A)
		input_mt_slot(touch_test_dev->input_dev, 0);
		input_mt_report_slot_state(touch_test_dev->input_dev, MT_TOOL_FINGER, false);
#else
		input_mt_sync(touch_test_dev->input_dev);
#endif	/* !defined(MT_PROTOCOL_A) */
		input_sync(touch_test_dev->input_dev);
	}
	else
	{
		TOUCH_ERR_MSG("Touch device not found\n");
	}
}
EXPORT_SYMBOL(Send_Touch);

int get_touch_ts_fw_version(char *fw_ver)
{
	if (touch_test_dev)
	{
		sprintf(fw_ver, "%s", touch_test_dev->fw_info.ic_fw_version);
		return 1;
	}
	else
	{
		return 0;
	}
}
EXPORT_SYMBOL(get_touch_ts_fw_version);


/* set_touch_handle / get_touch_handle
 *
 * Developer can save their object using 'set_touch_handle'.
 * Also, they can restore that using 'get_touch_handle'.
 */
void set_touch_handle(struct i2c_client *client, void* h_touch)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	ts->h_touch = h_touch;
}

void* get_touch_handle(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	return ts->h_touch;
}

/* touch_i2c_read / touch_i2c_write
 *
 * Developer can use these fuctions to communicate with touch_device through I2C.
 */
int touch_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf)
{
	int ret = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	if ((ret = i2c_transfer(client->adapter, msgs, 2)) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error %d\n", ret);
		return -EIO;
	} else
		return 0;
}

int touch_i2c_write(struct i2c_client *client, u8 reg, int len, u8 * buf)
{
	int ret = 0;
	unsigned char send_buf[len + 1];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = len+1,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	memcpy(&send_buf[1], buf, len);

	if ((ret = i2c_transfer(client->adapter, msgs, 1)) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error %d\n", ret);
		return -EIO;
	} else
		return 0;
}

int touch_i2c_write_byte(struct i2c_client *client, u8 reg, u8 data)
{
	int ret = 0;
	unsigned char send_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 2,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	send_buf[1] = (unsigned char)data;

	if ((ret = i2c_transfer(client->adapter, msgs, 1)) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error %d\n", ret);
		return -EIO;
	} else
		return 0;
}

#ifdef LGE_TOUCH_TIME_DEBUG
static void time_profile_result(struct lge_touch_data *ts)
{
	if (touch_time_debug_mask & DEBUG_TIME_PROFILE_ALL) {
		if (t_debug[TIME_INT_INTERVAL].tv_sec == 0
				&& t_debug[TIME_INT_INTERVAL].tv_usec == 0) {
			t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
			t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
		} else {
			TOUCH_INFO_MSG("Interval [%6luus], Total [%6luus], IRQ -> Thread IRQ [%6luus] -> work [%6luus] -> report [%6luus]\n",
				get_time_interval(t_debug[TIME_ISR_START].tv_usec, t_debug[TIME_INT_INTERVAL].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_THREAD_ISR_START].tv_usec, t_debug[TIME_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_START].tv_usec, t_debug[TIME_THREAD_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_WORKQUEUE_START].tv_usec));

			t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
			t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
		}
	} else {
		if (touch_time_debug_mask & DEBUG_TIME_INT_INTERVAL) {
			if (t_debug[TIME_INT_INTERVAL].tv_sec == 0
					&& t_debug[TIME_INT_INTERVAL].tv_usec == 0) {
				t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
				t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
			} else {
				TOUCH_INFO_MSG("Interrupt interval: %6luus\n",
					get_time_interval(t_debug[TIME_ISR_START].tv_usec, t_debug[TIME_INT_INTERVAL].tv_usec));

				t_debug[TIME_INT_INTERVAL].tv_sec = t_debug[TIME_ISR_START].tv_sec;
				t_debug[TIME_INT_INTERVAL].tv_usec = t_debug[TIME_ISR_START].tv_usec;
			}
		}

		if (touch_time_debug_mask & DEBUG_TIME_INT_IRQ_DELAY) {
			TOUCH_INFO_MSG("IRQ -> Thread IRQ : %6luus\n",
				get_time_interval(t_debug[TIME_THREAD_ISR_START].tv_usec, t_debug[TIME_ISR_START].tv_usec));
		}

		if (touch_time_debug_mask & DEBUG_TIME_INT_THREAD_IRQ_DELAY) {
			TOUCH_INFO_MSG("Thread IRQ -> work: %6luus\n",
				get_time_interval(t_debug[TIME_WORKQUEUE_START].tv_usec, t_debug[TIME_THREAD_ISR_START].tv_usec));
		}

		if (touch_time_debug_mask & DEBUG_TIME_DATA_HANDLE) {
			TOUCH_INFO_MSG("work -> report: %6luus\n",
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_WORKQUEUE_START].tv_usec));
		}
	}

	if (!ts->ts_data.total_num) {
		memset(t_debug, 0x0, sizeof(t_debug));
	}

}
#endif

/* touch_asb_input_report
 *
 * finger status report
 */
static int touch_asb_input_report(struct lge_touch_data *ts, int status)
{
	u16 id = 0;
	u8 total = 0;

	if (status == FINGER_PRESSED) {
		for (id = 0; id < ts->pdata->caps->max_id; id++) {
			if (ts->pdata->role->key_type == TOUCH_SOFT_KEY
					&& (ts->ts_data.curr_data[id].y_position
						>= ts->pdata->caps->y_button_boundary))
				continue;

#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
			if (unlikely(quick_cover_status)) {
				if (unlikely(ts->ts_data.curr_data[id].y_position > 540 || ts->ts_data.curr_data[id].x_position < 170
						|| ts->ts_data.curr_data[id].x_position > 550)) {
						ts->ts_data.curr_data[id].status = FINGER_RELEASED;
				}
			}
#endif

			if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
#if !defined(MT_PROTOCOL_A)
				input_mt_slot(ts->input_dev, id);
#ifdef CUST_G2_TOUCH
				switch(ts->ts_data.curr_data[id].object) {
					case FINGER:
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
						break;
					case PALM:
						if(ts->ts_data.large_palm_status == PALM_PRESSED || ts->ts_data.large_palm_status == PALM_MOVE) {
							TOUCH_DEBUG_MSG("id=%d, large palm pressed>>>>>>>>>>>>>>>>>>\n", id);
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_LARGEPALM, true);
						} else {
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_PALM, true);
						}
						break;
					default:
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
						break;
				}
#endif
#endif	/* !defined(MT_PROTOCOL_A) */
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						ts->ts_data.curr_data[id].x_position);

				/* When a user's finger cross the boundary (from key to LCD),
				    a ABS-event will change its y-position to edge of LCD, automatically.*/
				if(ts->pdata->role->key_type == TOUCH_SOFT_KEY
						&& ts->ts_data.curr_data[id].y_position < ts->pdata->caps->y_button_boundary
						&& ts->ts_data.prev_data[id].y_position > ts->pdata->caps->y_button_boundary
						&& ts->ts_data.prev_button.key_code != KEY_NULL)
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->pdata->caps->y_button_boundary);
				else
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						ts->ts_data.curr_data[id].y_position);

#ifdef CUST_G2_TOUCH
				if (ts->pdata->caps->is_pressure_supported){
					if(ts->ts_data.curr_data[id].object == PALM)
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 255);
					else if(ts->ts_data.curr_data[id].pressure == 255){
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 254);
					}
					else {
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								ts->ts_data.curr_data[id].pressure);
					}
				}
#else
				if (ts->pdata->caps->is_pressure_supported){
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
							ts->ts_data.curr_data[id].pressure);
				}
#endif

				if (ts->pdata->caps->is_width_supported) {
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
									 ts->ts_data.curr_data[id].width_major);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR,
									 ts->ts_data.curr_data[id].width_minor);
					input_report_abs(ts->input_dev, ABS_MT_ORIENTATION,
									 ts->ts_data.curr_data[id].width_orientation);
				}
				if (ts->pdata->caps->is_id_supported)
					input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);

#if defined(MT_PROTOCOL_A)
				input_mt_sync(ts->input_dev);
#endif	/* defined(MT_PROTOCOL_A) */

				total++;

				if (unlikely(touch_debug_mask & DEBUG_ABS))
					TOUCH_INFO_MSG("<%d> pos[%4d,%4d] o_t[%d] w_m[%2d] w_n[%2d] w_o[%2d] p[%3d]\n",
							ts->pdata->caps->is_id_supported? id : 0,
							ts->ts_data.curr_data[id].x_position,
							ts->ts_data.curr_data[id].y_position,
							ts->ts_data.curr_data[id].object,
							ts->pdata->caps->is_width_supported?
							ts->ts_data.curr_data[id].width_major: 0,
							ts->pdata->caps->is_width_supported?
							ts->ts_data.curr_data[id].width_minor: 0,
							ts->pdata->caps->is_width_supported?
							ts->ts_data.curr_data[id].width_orientation: 0,
							ts->pdata->caps->is_pressure_supported ?
							ts->ts_data.curr_data[id].pressure : 0);
			} else {
#if !defined(MT_PROTOCOL_A)	/* Protocol B type only */
				/* release handling */
				if (ts->pdata->role->key_type != TOUCH_SOFT_KEY
						&& ts->ts_data.prev_data[id].status == FINGER_PRESSED) {
					input_mt_slot(ts->input_dev, id);
#ifdef CUST_G2_TOUCH
					switch(ts->ts_data.prev_data[id].object) {
						case FINGER:
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
							break;
						case PALM:
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_PALM, false);
							break;
						default:
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
							break;
					}
#endif
				}
#endif
			}
		}
	} else if (status == FINGER_RELEASED) {
#if defined(MT_PROTOCOL_A)
		input_mt_sync(ts->input_dev);
#else	/* MT_PROTOCOL_B */
		for (id = 0; id < ts->pdata->caps->max_id; id++) {
			if (ts->ts_data.prev_data[id].status == FINGER_PRESSED) {
				input_mt_slot(ts->input_dev, id);
#ifdef CUST_G2_TOUCH
				switch(ts->ts_data.prev_data[id].object) {
					case FINGER:
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
						break;
					case PALM:
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_PALM, false);
						break;
					default:
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
						break;
				}
#endif
			}
		}
#endif	/* defined(MT_PROTOCOL_A) */
	}

	return total;
}

/* release_all_ts_event
 *
 * When system enters suspend-state,
 * if user press touch-panel, release them automatically.
 */
static void release_all_ts_event(struct lge_touch_data *ts)
{
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY){
		if (ts->ts_data.prev_total_num) {
			touch_asb_input_report(ts, FINGER_RELEASED);

			if (likely(touch_debug_mask & (DEBUG_ABS | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("touch finger position released\n");
		}

		if(ts->ts_data.prev_button.state == BUTTON_PRESSED) {
			input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

			if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("Touch KEY[%d] is released\n", ts->ts_data.prev_button.key_code);
		}
	}
	else if (ts->pdata->role->key_type == VIRTUAL_KEY){
		if (ts->ts_data.prev_total_num) {
			touch_asb_input_report(ts, FINGER_RELEASED);

			if (likely(touch_debug_mask & (DEBUG_ABS | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("touch finger position released\n");
		}
	}
	else if (ts->pdata->role->key_type == TOUCH_SOFT_KEY){
		if (ts->ts_data.state == ABS_PRESS) {
			touch_asb_input_report(ts, FINGER_RELEASED);

			if (likely(touch_debug_mask & (DEBUG_ABS | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("touch finger position released\n");
		} else if (ts->ts_data.state == BUTTON_PRESS) {
			input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

			if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("Touch KEY[%d] is released\n", ts->ts_data.prev_button.key_code);
		}
	}

	ts->ts_data.prev_total_num = 0;

	if (ts->pdata->role->jitter_filter_enable) {
		ts->jitter_filter.id_mask = 0;
		memset(ts->jitter_filter.his_data, 0, sizeof(ts->jitter_filter.his_data));
	}
	if(ts->pdata->role->accuracy_filter_enable) {
		memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));
		ts->accuracy_filter.finish_filter = 0;
		ts->accuracy_filter.prev_total_num = 0;
	}
	input_sync(ts->input_dev);
}

/* touch_power_cntl
 *
 * 1. POWER_ON
 * 2. POWER_OFF
 * 3. POWER_SLEEP
 * 4. POWER_WAKE
 */
static int touch_power_cntl(struct lge_touch_data *ts, int onoff)
{
	int ret = 0;

	if (touch_device_func->power == NULL) {
		TOUCH_INFO_MSG("There is no specific power control function\n");
		return -1;
	}

	switch (onoff) {
	case POWER_ON:
		ret = touch_device_func->power(ts->client, POWER_ON);
		if (ret < 0) {
			TOUCH_ERR_MSG("power on failed\n");
		} else
			ts->curr_pwr_state = POWER_ON;
		TOUCH_INFO_MSG("touch power on\n");

		break;
	case POWER_OFF:
		atomic_set(&ts->device_init, 0);
		release_all_ts_event(ts);
		ret = touch_device_func->power(ts->client, POWER_OFF);
		if (ret < 0) {
			TOUCH_ERR_MSG("power off failed\n");
		} else
			ts->curr_pwr_state = POWER_OFF;
		TOUCH_INFO_MSG("touch power off\n");

		msleep(ts->pdata->role->reset_delay);

		break;
	case POWER_SLEEP:
		ret = touch_device_func->power(ts->client, POWER_SLEEP);
		if (ret < 0) {
			TOUCH_ERR_MSG("power sleep failed\n");
		} else
			ts->curr_pwr_state = POWER_SLEEP;

		break;
	case POWER_WAKE:
		ret = touch_device_func->power(ts->client, POWER_WAKE);
		if (ret < 0) {
			TOUCH_ERR_MSG("power wake failed\n");
		} else
			ts->curr_pwr_state = POWER_WAKE;

		break;
	default:
		break;
	}

	if (unlikely(touch_debug_mask & DEBUG_POWER))
		if (ret >= 0)
			TOUCH_INFO_MSG("%s: power_state[%d]", __FUNCTION__, ts->curr_pwr_state);

	return ret;
}

/* safety_reset
 *
 * 1. disable irq/timer.
 * 2. turn off the power.
 * 3. turn on the power.
 * 4. sleep (booting_delay)ms, usually 400ms(synaptics).
 * 5. enable irq/timer.
 *
 * After 'safety_reset', we should call 'touch_init'.
 */
static void safety_reset(struct lge_touch_data *ts)
{
	if (!ts->pdata->role->operation_mode)
		hrtimer_cancel(&ts->timer);

#ifdef CUST_G2_TOUCH
	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}
#endif

	release_all_ts_event(ts);

	touch_power_cntl(ts, POWER_OFF);
	touch_power_cntl(ts, POWER_ON);

#ifdef CUST_G2_TOUCH
	if((ts_suspend==1) && (touch_gesture_enable==0))
		touch_power_cntl(ts, POWER_OFF);
#endif

	msleep(ts->pdata->role->booting_delay);

	if (!ts->pdata->role->operation_mode)
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	return;
}

/* touch_ic_init
 *
 * initialize the device_IC and variables.
 */
static int touch_ic_init(struct lge_touch_data *ts)
{
	int next_work = 0;
	ts->int_pin_state = 0;

#ifdef CUST_G2_TOUCH
	touch_thermal_mode = 0;
	thermal_status = 0;

	if (ts->curr_pwr_state == POWER_OFF)
		return 0;
#endif

	if(unlikely(ts->ic_init_err_cnt >= MAX_RETRY_COUNT)){
		TOUCH_ERR_MSG("Init Failed: Irq-pin has some unknown problems\n");
		goto err_out_critical;
	}

	atomic_set(&ts->next_work, 0);
	atomic_set(&ts->device_init, 1);

	if (touch_device_func->init == NULL) {
		TOUCH_INFO_MSG("There is no specific IC init function\n");
		goto err_out_critical;
	}

	if (touch_device_func->init(ts->client, &ts->fw_info) < 0) {
		TOUCH_ERR_MSG("specific device initialization fail\n");
		goto err_out_retry;
	}
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
#if !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM)  && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_B){
#else
	{
#endif
		if(touch_device_func->ic_ctrl) {
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_DOUBLE_TAP_WAKEUP_MODE, 0) < 0){
				TOUCH_ERR_MSG("IC_CTRL_DOUBLE_TAP_WAKEUP_MODE handling fail\n");
				goto err_out_retry;
			}
		}
	}
#endif

	/* Interrupt pin check after IC init - avoid Touch lockup */
	if(ts->pdata->role->operation_mode == INTERRUPT_MODE){
		ts->int_pin_state = gpio_get_value(ts->pdata->int_pin);
		next_work = atomic_read(&ts->next_work);

		if(unlikely(ts->int_pin_state != 1 && next_work <= 0)){
			TOUCH_INFO_MSG("WARN: Interrupt pin is low - next_work: %d, try_count: %d]\n",
					next_work, ts->ic_init_err_cnt);
			goto err_out_retry;
		}
	}

#if defined(Z_GLOVE_TOUCH_SUPPORT)
	if (set_glove_finger_enable(ts, glove_finger_enable) < 0) {
		TOUCH_ERR_MSG("set_glove_finger_enable error (%s)\n", __func__);
	}
#endif

#ifdef CUST_G2_TOUCH
	if (ts->pdata->role->ghost_detection_enable) {
		memset(&ts->gd_ctrl.init_data, 0, sizeof(ts->gd_ctrl.init_data));
#if defined(A1_only)
		ts->gd_ctrl.init_data.mask = GHOST_LONG_PRESS | GHOST_FIRST_IRQ | GHOST_PRESSURE | GHOST_TA_DEBOUNCE | GHOST_EDGE_ZONE;
#elif defined(CONFIG_LGE_Z_TOUCHSCREEN)
		ts->gd_ctrl.init_data.mask = GHOST_LONG_PRESS | GHOST_FIRST_IRQ | GHOST_PRESSURE | GHOST_EDGE_ZONE;
#else
		ts->gd_ctrl.init_data.mask = GHOST_LONG_PRESS | GHOST_FIRST_IRQ | GHOST_PRESSURE | GHOST_BUTTON | GHOST_TA_RESET;
#endif
		do_gettimeofday(&t_ex_debug[TIME_EX_INIT_TIME]);

		if(ts->gd_ctrl.init_data.mask & GHOST_LONG_PRESS) {
			if(ts->pdata->role->report_mode == REDUCED_REPORT_MODE) {
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_REPORT_MODE, CONTINUOUS_REPORT_MODE) < 0) {
					TOUCH_ERR_MSG("IC_CTRL_REPORT_MODE handling fail\n");
					goto err_out_retry;
				}
			}
		}
	}
#endif

	ts->gf_ctrl.count = 0;
	ts->gf_ctrl.ghost_check_count = 0;
	ts->gf_ctrl.saved_x = -1;
	ts->gf_ctrl.saved_y = -1;
	ts->gd_ctrl.init_data.in_edge_zone_id = -1;
	ts->gd_ctrl.init_data.in_edge_zone_x_pos= -1;
	ts->gd_ctrl.init_data.in_edge_zone_y_pos= -1;

	if(ts->gf_ctrl.probe){
		ts->gf_ctrl.stage = GHOST_STAGE_1;
		if(touch_device_func->ic_ctrl){
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0){
				TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
				goto err_out_retry;
			}
		}
	}
	else{
		if(ts->gf_ctrl.stage & GHOST_STAGE_2) {
			ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2 | GHOST_STAGE_4;
			ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
			if(touch_device_func->ic_ctrl){
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0){
					TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
					goto err_out_retry;
				}
			}
		}
		else {
			ts->gf_ctrl.stage = GHOST_STAGE_3;
			if(touch_device_func->ic_ctrl){
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0){
					TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
					goto err_out_retry;
				}
			}
		}
	}

	if (unlikely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_GHOST))){
		TOUCH_INFO_MSG("%s %s(%s): FW ver[%s], force[%d]\n",
		        ts->pdata->maker, ts->fw_info.ic_fw_identifier,
		        ts->pdata->role->operation_mode?"Interrupt mode":"Polling mode",
		        ts->fw_info.ic_fw_version, ts->fw_info.fw_upgrade.fw_force_upgrade);
		TOUCH_INFO_MSG("irq_pin[%d] next_work[%d] ghost_stage[0x%x]\n",
				ts->int_pin_state, next_work, ts->gf_ctrl.stage);
	}

	ts->gf_ctrl.probe = 0;

	memset(&ts->ts_data, 0, sizeof(ts->ts_data));
	memset(&ts->fw_info.fw_upgrade, 0, sizeof(ts->fw_info.fw_upgrade));
	ts->ic_init_err_cnt = 0;

	ts->jitter_filter.id_mask = 0;
	memset(ts->jitter_filter.his_data, 0, sizeof(ts->jitter_filter.his_data));
	memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));

	ts->accuracy_filter.prev_total_num = 0;
	ts->accuracy_filter.finish_filter = 0;

	return 0;

err_out_retry:
	atomic_set(&ts->device_init, 0);
	ts->ic_init_err_cnt++;
	safety_reset(ts);
	queue_delayed_work(touch_wq, &ts->work_init, msecs_to_jiffies(10));
	return 0;

err_out_critical:
	touch_power_cntl(ts, POWER_OFF);
	ts->ic_init_err_cnt = 0;

	return -1;
}

/* ghost_finger_solution
 *
 * GHOST_STAGE_1
 * - melt_mode.
 * - If user press and release their finger in 1 sec, STAGE_1 will be cleared. --> STAGE_2
 * - If there is no key-guard, ghost_finger_solution is finished.
 *
 * GHOST_STAGE_2
 * - no_melt_mode
 * - if user use multi-finger, stage will be changed to STAGE_1
 *   (We assume that ghost-finger occured)
 * - if key-guard is unlocked, STAGE_2 is cleared. --> STAGE_3
 *
 * GHOST_STAGE_3
 * - when user release their finger, device re-scan the baseline.
 * - Then, GHOST_STAGE3 is cleared and ghost_finger_solution is finished.
 */
#define ghost_sub(x, y)	(x > y ? x - y : y - x)

int ghost_finger_solution(struct lge_touch_data *ts)
{
	u8	id = 0;

	for (id = 0; id < ts->pdata->caps->max_id; id++){
		if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
			break;
		}
	}

	if(ts->gf_ctrl.stage & GHOST_STAGE_1){
		if(ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm == 0){
			if(ts->gf_ctrl.count < ts->gf_ctrl.min_count || ts->gf_ctrl.count >= ts->gf_ctrl.max_count){
				if(ts->gf_ctrl.stage & GHOST_STAGE_2)
					ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
				else
					ts->gf_ctrl.ghost_check_count = 0;
			}
			else{
				if(ghost_sub(ts->gf_ctrl.saved_x, ts->gf_ctrl.saved_last_x) > ts->gf_ctrl.max_moved ||
				   ghost_sub(ts->gf_ctrl.saved_y, ts->gf_ctrl.saved_last_y) > ts->gf_ctrl.max_moved)
					ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT;
				else
					ts->gf_ctrl.ghost_check_count++;

				if (unlikely(touch_debug_mask & DEBUG_GHOST))
					TOUCH_INFO_MSG("ghost_stage_1: delta[%d/%d/%d]\n",
						ghost_sub(ts->gf_ctrl.saved_x, ts->gf_ctrl.saved_last_x),
						ghost_sub(ts->gf_ctrl.saved_y, ts->gf_ctrl.saved_last_y),
						ts->gf_ctrl.max_moved);
			}

			if (unlikely(touch_debug_mask & DEBUG_GHOST || touch_debug_mask & DEBUG_BASE_INFO))
				TOUCH_INFO_MSG("ghost_stage_1: ghost_check_count+[0x%x]\n", ts->gf_ctrl.ghost_check_count);

			if(ts->gf_ctrl.ghost_check_count >= MAX_GHOST_CHECK_COUNT){
				ts->gf_ctrl.ghost_check_count = 0;
				if(touch_device_func->ic_ctrl){
					if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0)
						return -1;
				}
				ts->gf_ctrl.stage &= ~GHOST_STAGE_1;
				if (unlikely(touch_debug_mask & DEBUG_GHOST|| touch_debug_mask & DEBUG_BASE_INFO))
					TOUCH_INFO_MSG("ghost_stage_1: cleared[0x%x]\n", ts->gf_ctrl.stage);
				if(!ts->gf_ctrl.stage){
					if (unlikely(touch_debug_mask & DEBUG_GHOST))
						TOUCH_INFO_MSG("ghost_stage_finished. (NON-KEYGUARD)\n");
				}
			}
			ts->gf_ctrl.count = 0;
			ts->gf_ctrl.saved_x = -1;
			ts->gf_ctrl.saved_y = -1;
		}
		else if(ts->ts_data.total_num == 1 && ts->ts_data.curr_button.state == 0
				&& id == 0 && ts->ts_data.palm == 0){
			if(ts->gf_ctrl.saved_x == -1 && ts->gf_ctrl.saved_x == -1){
				ts->gf_ctrl.saved_x = ts->ts_data.curr_data[id].x_position;
				ts->gf_ctrl.saved_y = ts->ts_data.curr_data[id].y_position;
			}
			ts->gf_ctrl.count++;
			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_1: int_count[%d/%d]\n", ts->gf_ctrl.count, ts->gf_ctrl.max_count);
		}
		else {
			if (unlikely(touch_debug_mask & DEBUG_GHOST) && ts->gf_ctrl.count != ts->gf_ctrl.max_count)
				TOUCH_INFO_MSG("ghost_stage_1: Not good condition. total[%d] button[%d] id[%d] palm[%d]\n",
						ts->ts_data.total_num, ts->ts_data.curr_button.state,
						id, ts->ts_data.palm);
			ts->gf_ctrl.count = ts->gf_ctrl.max_count;
		}
	}
	else if(ts->gf_ctrl.stage & GHOST_STAGE_2){
		if(ts->ts_data.total_num > 1 || (ts->ts_data.total_num == 1 && ts->ts_data.curr_button.state)){
			ts->gf_ctrl.stage |= GHOST_STAGE_1;
			ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
			ts->gf_ctrl.count = 0;
			ts->gf_ctrl.saved_x = -1;
			ts->gf_ctrl.saved_y = -1;
			if(touch_device_func->ic_ctrl){
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0)
					return -1;
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0)
					return -1;
			}
			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_2: multi_finger. return to ghost_stage_1[0x%x]\n", ts->gf_ctrl.stage);
		}
	}
	else if(ts->gf_ctrl.stage & GHOST_STAGE_3){
		if(ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm == 0){
			ts->gf_ctrl.ghost_check_count++;

			if (unlikely(touch_debug_mask & DEBUG_GHOST || touch_debug_mask & DEBUG_BASE_INFO))
				TOUCH_INFO_MSG("ghost_stage_3: ghost_check_count+[0x%x]\n", ts->gf_ctrl.ghost_check_count);

			if(ts->gf_ctrl.ghost_check_count >= MAX_GHOST_CHECK_COUNT){
				ts->gf_ctrl.stage &= ~GHOST_STAGE_3;
				if (unlikely(touch_debug_mask & DEBUG_GHOST || touch_debug_mask & DEBUG_BASE_INFO))
					TOUCH_INFO_MSG("ghost_stage_3: cleared[0x%x]\n", ts->gf_ctrl.stage);
				if(!ts->gf_ctrl.stage){
					if (unlikely(touch_debug_mask & DEBUG_GHOST))
						TOUCH_INFO_MSG("ghost_stage_finished. (NON-KEYGUARD)\n");
				}
			}
		}
		else if(ts->ts_data.total_num == 1 && ts->ts_data.curr_button.state == 0
				&& id == 0 && ts->ts_data.palm == 0);
		else{
			ts->gf_ctrl.stage &= ~GHOST_STAGE_3;
			ts->gf_ctrl.stage |= GHOST_STAGE_1;
			ts->gf_ctrl.ghost_check_count = 0;
			ts->gf_ctrl.count = 0;
			ts->gf_ctrl.saved_x = -1;
			ts->gf_ctrl.saved_y = -1;
			if (touch_device_func->ic_ctrl) {
				if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0)
					return -1;
			}

			if (unlikely(touch_debug_mask & DEBUG_GHOST)){
				TOUCH_INFO_MSG("ghost_stage_3: Not good condition. total[%d] button[%d] id[%d] palm[%d]\n",
						ts->ts_data.total_num, ts->ts_data.curr_button.state,
						id, ts->ts_data.palm);
				TOUCH_INFO_MSG("ghost_stage_3: return to ghost_stage_1[0x%x]\n", ts->gf_ctrl.stage);
			}
		}
	}
	else if(ts->gf_ctrl.stage & GHOST_STAGE_4){
		if(ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm == 0){
			if(touch_device_func->ic_ctrl){
				if (touch_device_func->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0)
					return -1;
			}
			ts->gf_ctrl.stage = GHOST_STAGE_CLEAR;
			if (unlikely(touch_debug_mask & DEBUG_GHOST || touch_debug_mask & DEBUG_BASE_INFO))
				TOUCH_INFO_MSG("ghost_stage_4: cleared[0x%x]\n", ts->gf_ctrl.stage);
			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_finished. (KEYGUARD)\n");
		}

		if (unlikely(touch_debug_mask & DEBUG_GHOST) && ts->ts_data.palm != 0)
			TOUCH_INFO_MSG("ghost_stage_4: palm[%d]\n", ts->ts_data.palm);
	}

	ts->gf_ctrl.saved_last_x = ts->ts_data.curr_data[id].x_position;
	ts->gf_ctrl.saved_last_y = ts->ts_data.curr_data[id].y_position;

	return 0;
}

/* Jitter Filter
 *
 */
#ifdef CUST_G2_TOUCH
//do nothing
#else
#define jitter_abs(x)		(x > 0 ? x : -x)
#define jitter_sub(x, y)	(x > y ? x - y : y - x)
#endif

static u16 check_boundary(int x, int max){
	if(x < 0)
		return 0;
	else if(x > max)
		return (u16)max;
	else
		return (u16)x;
}

static int check_direction(int x){
	if(x > 0)
		return 1;
	else if(x < 0)
		return -1;
	else
		return 0;
}

int accuracy_filter_func(struct lge_touch_data *ts)
{
	int delta_x = 0;
	int delta_y = 0;
	u8	id = 0;

	for (id = 0; id < ts->pdata->caps->max_id; id++){
		if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
			break;
		}
	}

	delta_x = (int)ts->accuracy_filter.his_data.x - (int)ts->ts_data.curr_data[0].x_position;
	delta_y = (int)ts->accuracy_filter.his_data.y - (int)ts->ts_data.curr_data[0].y_position;

	// main algorithm
	if (ts->accuracy_filter.finish_filter){
		if(delta_x || delta_y){
			ts->accuracy_filter.his_data.axis_x += check_direction(delta_x);
			ts->accuracy_filter.his_data.axis_y += check_direction(delta_y);
			ts->accuracy_filter.his_data.count++;
		}

		if(((ts->ts_data.curr_data[0].pressure > ts->accuracy_filter.pen_pressure
		     && jitter_sub(ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure) > ts->accuracy_filter.ignore_pressure_gap)
		        || ts->ts_data.curr_data[0].pressure > ts->accuracy_filter.max_pressure)
		    && !(ts->accuracy_filter.his_data.count > ts->accuracy_filter.time_to_max_pressure
		         && (jitter_abs(ts->accuracy_filter.his_data.axis_x) == ts->accuracy_filter.his_data.count
		             || jitter_abs(ts->accuracy_filter.his_data.axis_y) == ts->accuracy_filter.his_data.count))){
				ts->accuracy_filter.his_data.mod_x += delta_x;
				ts->accuracy_filter.his_data.mod_y += delta_y;
		}
	}

	// finish the accuracy_filter
	if(ts->accuracy_filter.finish_filter == 1 &&
	   (ts->accuracy_filter.his_data.count > ts->accuracy_filter.touch_max_count || ts->ts_data.total_num != 1 || id != 0
	   || jitter_abs(delta_x) > ts->accuracy_filter.delta_max || jitter_abs(delta_y) > ts->accuracy_filter.delta_max
	   || jitter_abs(ts->accuracy_filter.his_data.axis_x) > ts->accuracy_filter.direction_count || jitter_abs(ts->accuracy_filter.his_data.axis_y) > ts->accuracy_filter.direction_count)){
		ts->accuracy_filter.finish_filter = 0;
		memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));
	}

	// start the accuracy_filter
	if(ts->accuracy_filter.finish_filter == 0 && ts->ts_data.total_num == 1
	   && ts->accuracy_filter.prev_total_num == 0 && id == 0){
		ts->accuracy_filter.finish_filter = 1;
		memset(&ts->accuracy_filter.his_data, 0, sizeof(ts->accuracy_filter.his_data));
	}

	if(unlikely(touch_debug_mask & DEBUG_ACCURACY)){
		TOUCH_INFO_MSG("AccuracyFilter: <%d> pos[%4d,%4d] new[%4d,%4d] his[%4d,%4d] delta[%3d,%3d] mod[%3d,%3d] p[%d,%3d,%3d] axis[%2d,%2d] count[%2d/%2d] total_num[%d,%d] finish[%d]\n",
				ts->ts_data.curr_data[0].id, ts->ts_data.curr_data[0].x_position, ts->ts_data.curr_data[0].y_position,
				check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->caps->x_max),
				check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->caps->y_max),
				ts->accuracy_filter.his_data.x, ts->accuracy_filter.his_data.y,
				delta_x, delta_y,
				ts->accuracy_filter.his_data.mod_x, ts->accuracy_filter.his_data.mod_y,
				jitter_sub(ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure) > ts->accuracy_filter.ignore_pressure_gap,
				ts->ts_data.curr_data[0].pressure, ts->accuracy_filter.his_data.pressure,
				ts->accuracy_filter.his_data.axis_x, ts->accuracy_filter.his_data.axis_y,
				ts->accuracy_filter.his_data.count, ts->accuracy_filter.touch_max_count,
				ts->accuracy_filter.prev_total_num, ts->ts_data.total_num, ts->accuracy_filter.finish_filter);
	}

	ts->accuracy_filter.his_data.x = ts->ts_data.curr_data[0].x_position;
	ts->accuracy_filter.his_data.y = ts->ts_data.curr_data[0].y_position;
	ts->accuracy_filter.his_data.pressure = ts->ts_data.curr_data[0].pressure;
	ts->accuracy_filter.prev_total_num = ts->ts_data.total_num;

	if(ts->ts_data.total_num){
		ts->ts_data.curr_data[0].x_position
			= check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->caps->x_max);
		ts->ts_data.curr_data[0].y_position
			= check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->caps->y_max);
	}

	return 0;
}
EXPORT_SYMBOL(accuracy_filter_func);

int jitter_filter_func(struct lge_touch_data *ts)
{
	u16 id = 0;
	int jitter_count = 0;
	u16 new_id_mask = 0;
	u16 bit_mask = 0;
	u16 bit_id = 1;
	int curr_ratio = ts->pdata->role->jitter_curr_ratio;

	if(!ts->ts_data.total_num) {
		ts->jitter_filter.id_mask = 0;
		memset(ts->jitter_filter.his_data, 0, sizeof(ts->jitter_filter.his_data));
		return 0;
	}

	for (id = 0; id < ts->pdata->caps->max_id; id++){
		if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
			u16 width = ts->ts_data.curr_data[id].width_major;
			new_id_mask |= (1 << id);

			if(ts->jitter_filter.id_mask & (1 << id)){
				int delta_x, delta_y;
				int f_jitter = (128 - curr_ratio)*width;
				int adjust_x = 0;
				int adjust_y = 0;
				int adj_ratio = 0;
				char adj_mode = 0;

				if (ts->jitter_filter.adjust_margin > 0) {
					adjust_x = (int)ts->ts_data.curr_data[id].x_position
								- (int)ts->jitter_filter.his_data[id].x;
					adjust_y = (int)ts->ts_data.curr_data[id].y_position
								- (int)ts->jitter_filter.his_data[id].y;
				}

				ts->ts_data.curr_data[id].x_position =
						(ts->ts_data.curr_data[id].x_position + ts->jitter_filter.his_data[id].x) >> 1;
				ts->ts_data.curr_data[id].y_position =
						(ts->ts_data.curr_data[id].y_position + ts->jitter_filter.his_data[id].y) >> 1;

				if (ts->jitter_filter.adjust_margin > 0) {
					adj_ratio = (((width + 1) << 6) / (curr_ratio + 1))
							+ (ts->jitter_filter.adjust_margin >> 3);
					if (jitter_abs(adjust_x) > ts->jitter_filter.adjust_margin
							|| jitter_abs(adjust_x) > adj_ratio
							|| jitter_abs(adjust_y) > ts->jitter_filter.adjust_margin
							|| jitter_abs(adjust_y) > adj_ratio) {
						adjust_x = (int)ts->ts_data.curr_data[id].x_position + (adjust_x >> 2);
						ts->ts_data.curr_data[id].x_position = check_boundary(adjust_x, ts->pdata->caps->x_max);

						adjust_y = (int)ts->ts_data.curr_data[id].y_position + (adjust_y >> 2);
						ts->ts_data.curr_data[id].y_position = check_boundary(adjust_y, ts->pdata->caps->y_max);

						adj_mode = 1;
					}
				}

				delta_x = (int)ts->ts_data.curr_data[id].x_position - (int)ts->jitter_filter.his_data[id].x;
				delta_y = (int)ts->ts_data.curr_data[id].y_position - (int)ts->jitter_filter.his_data[id].y;

				ts->jitter_filter.his_data[id].delta_x = delta_x * curr_ratio
						+ ((ts->jitter_filter.his_data[id].delta_x * (128 - curr_ratio)) >> 7);
				ts->jitter_filter.his_data[id].delta_y = delta_y * curr_ratio
						+ ((ts->jitter_filter.his_data[id].delta_y * (128 - curr_ratio)) >> 7);

				if(unlikely(touch_debug_mask & DEBUG_JITTER)){
					TOUCH_INFO_MSG("JitterFilter[%s]: <%d> p[%d,%d] h_p[%d,%d] w[%d] a_r[%d] d[%d,%d] h_d[%d,%d] f_j[%d]\n",
							adj_mode?"fast":"norm", id, ts->ts_data.curr_data[id].x_position, ts->ts_data.curr_data[id].y_position,
							ts->jitter_filter.his_data[id].x, ts->jitter_filter.his_data[id].y,
							width, adj_ratio, delta_x, delta_y,
							ts->jitter_filter.his_data[id].delta_x, ts->jitter_filter.his_data[id].delta_y, f_jitter);
				}

				if(jitter_abs(ts->jitter_filter.his_data[id].delta_x) <= f_jitter &&
				   jitter_abs(ts->jitter_filter.his_data[id].delta_y) <= f_jitter)
					jitter_count++;
			}
		}
	}

	bit_mask = ts->jitter_filter.id_mask ^ new_id_mask;

	if (ts->pdata->caps->max_id > 10){
			if(unlikely(touch_debug_mask & DEBUG_JITTER))
				TOUCH_INFO_MSG("JitterFilter: Buffer overflow Max_id[%d]\n", ts->pdata->caps->max_id);
			return -1;
	} else {
		for (id = 0, bit_id = 1; id < ts->pdata->caps->max_id; id++){
			if ((ts->jitter_filter.id_mask & bit_id) && !(new_id_mask & bit_id)){
				if(unlikely(touch_debug_mask & DEBUG_JITTER))
					TOUCH_INFO_MSG("JitterFilter: released - id[%d] mask[0x%x]\n", bit_id, ts->jitter_filter.id_mask);
				memset(&ts->jitter_filter.his_data[id], 0, sizeof(ts->jitter_filter.his_data[id]));
			}
			bit_id = bit_id << 1;
		}
	}

	for (id = 0; id < ts->pdata->caps->max_id; id++){
		if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
			ts->jitter_filter.his_data[id].pressure = ts->ts_data.curr_data[id].pressure;
		}
	}

	if (!bit_mask && ts->ts_data.total_num && ts->ts_data.total_num == jitter_count && ts->ts_data.total_num == ts->ts_data.prev_total_num){
		if(unlikely(touch_debug_mask & DEBUG_JITTER))
			TOUCH_INFO_MSG("JitterFilter: ignored - jitter_count[%d] total_num[%d] prev_total_num[%d] bitmask[0x%x]\n",
					jitter_count, ts->ts_data.total_num, ts->ts_data.prev_total_num, bit_mask);
		return -1;
	}

	for (id = 0; id < ts->pdata->caps->max_id; id++){
		if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
			ts->jitter_filter.his_data[id].x = ts->ts_data.curr_data[id].x_position;
			ts->jitter_filter.his_data[id].y = ts->ts_data.curr_data[id].y_position;
		}
	}

	ts->jitter_filter.id_mask = new_id_mask;

	return 0;
}
EXPORT_SYMBOL(jitter_filter_func);

/* touch_init_func
 *
 * In order to reduce the booting-time,
 * we used delayed_work_queue instead of msleep or mdelay.
 */
static void touch_init_func(struct work_struct *work_init)
{
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_init), struct lge_touch_data, work_init);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	/* Specific device initialization */
	touch_ic_init(ts);
}

#ifdef CUST_G2_TOUCH
static ssize_t show_f54(struct lge_touch_data *ts, char *buf);

static struct sys_device lge_touch_sys_device;

#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
char *touch_wakeup_gesture[2] = { "TOUCH_GESTURE_WAKEUP=WAKEUP", NULL };
static void touch_gesture_wakeup_func(struct work_struct *work_gesture_wakeup)
{
	u8 buf = 0;
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
#if defined(A1_only)
	int A1_vendor = 0;
#endif
#endif
	struct lge_touch_data *ts =
		container_of(to_delayed_work(work_gesture_wakeup), struct lge_touch_data, work_gesture_wakeup);

	if (ts->fw_info.fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("touch_gesture_wakeup is not executed\n");
		return;
	}
	mutex_lock(&ts->irq_work_mutex);
	mutex_lock(&i2c_suspend_lock);

	if (unlikely(touch_i2c_read(ts->client, 0x05, 1, &buf) < 0)) {
		mutex_unlock(&i2c_suspend_lock);
		mutex_unlock(&ts->irq_work_mutex);
		TOUCH_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
#ifndef CONFIG_LGE_SECURITY_KNOCK_ON
		wake_unlock(&touch_wake_lock);
#endif
		return ; // fail
	}

#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
	if((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10)<50){
#endif
#if defined(A1_only)
#if defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	switch(ts->fw_info.fw_setting.ic_chip_rev) {
							case TOUCH_PANEL_G1F_LGIT:
								A1_vendor = 17;
								break;
							case TOUCH_PANEL_G1F_SSUNTEL:
								A1_vendor = 9;
								break;
							default:
								break;
	}
#else
	switch(touch_test_dev->fw_info.fw_setting.curr_touch_vendor) {
							case TOUCH_VENDOR_TPK:
							   A1_vendor = 14;
							   break;
						   case TOUCH_VENDOR_LGIT:
							   A1_vendor = 70;
							   break;
						   default:
							   break;
	}
#endif	
	if ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10)<A1_vendor) {
#endif
		if(buf !=0x04){
			mutex_unlock(&i2c_suspend_lock);
			mutex_unlock(&ts->irq_work_mutex);
			TOUCH_INFO_MSG("exception handling INTERRUPT_STATUS_REG %x\n", buf);
			return;
		}
	} else {
		if((buf!=0x40)&&(buf!=0x04)){
			mutex_unlock(&i2c_suspend_lock);
			mutex_unlock(&ts->irq_work_mutex);
			TOUCH_INFO_MSG("exception handling INTERRUPT_STATUS_REG %x\n", buf);
			return;
		}
	}

	if (touch_gesture_enable != LPWG_DOUBLE_TAP) {
		if (touch_device_func->data(ts->client, &ts->ts_data) < 0)
			TOUCH_ERR_MSG("touch_gesture_wakeup_func get data fail\n");
	}
#endif
	mutex_unlock(&i2c_suspend_lock);
	mutex_unlock(&ts->irq_work_mutex);

	TOUCH_INFO_MSG("INTERRUPT_STATUS_REG %x\n", buf);
#ifndef CONFIG_LGE_SECURITY_KNOCK_ON
	wake_lock_timeout(&touch_wake_lock, msecs_to_jiffies(3000));
#endif
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	TOUCH_INFO_MSG("knock on reporting? %d\n", knockon_wakeup_uevent_reporting);
	if (knockon_wakeup_uevent_reporting!=1) {
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
		if (buf & 0x40) {
			send_uevent_lpwg(touch_gesture_enable);
			knockon_wakeup_uevent_reporting = 1;
		} else {
			wake_unlock(&touch_wake_lock);
		}
#else
		if (buf & 0x04) {
			kobject_uevent_env(&lge_touch_sys_device.kobj, KOBJ_CHANGE, touch_wakeup_gesture);
			knockon_wakeup_uevent_reporting = 1;
		} else {
			wake_unlock(&touch_wake_lock);
		}
#endif
	}
#else
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
	TOUCH_INFO_MSG("ic_fw_version = %d\n",(int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10));
#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
	if ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10)<50) {
#endif
#if defined(A1_only)
#if defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	switch(ts->fw_info.fw_setting.ic_chip_rev) {
						 case TOUCH_PANEL_G1F_LGIT:
							A1_vendor = 17;
							break;
						case TOUCH_PANEL_G1F_SSUNTEL:
							A1_vendor = 9;
							break;
						default:
							break;
	}
#else
	switch(touch_test_dev->fw_info.fw_setting.curr_touch_vendor) {
						 case TOUCH_VENDOR_TPK:
						 	A1_vendor = 14;
							break;
						case TOUCH_VENDOR_LGIT:
							A1_vendor = 70;
							break;
						default:
							break;
 	}
#endif
	if ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10)<A1_vendor) {
#endif
		if (buf == 0x04) {
			kobject_uevent_env(&lge_touch_sys_device.kobj, KOBJ_CHANGE, touch_wakeup_gesture);
		}
	} else {
		if ((buf == 0x40) && (touch_gesture_enable == LPWG_DOUBLE_TAP)) {
			send_uevent_lpwg(touch_gesture_enable);
		}
	}
#else
	if (buf & 0x04) {
		kobject_uevent_env(&lge_touch_sys_device.kobj, KOBJ_CHANGE, touch_wakeup_gesture);
	} else {
		wake_unlock(&touch_wake_lock);
	}
#endif
#endif
}
#endif

static void touch_f54_func(struct work_struct *work_f54)
{
	struct lge_touch_data *ts =
		container_of(to_delayed_work(work_f54), struct lge_touch_data, work_f54);

	char buf[1000]= {0,};

	if (ts->fw_info.fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("touch_f54_func is not executed\n");
		return;
	}
	f54_window_crack_check_mode = 1;
	TOUCH_INFO_MSG("touch_f54_func start\n");
	show_f54(ts,buf);
	if(f54_window_crack) {
		char *window_crack[2] = { "TOUCH_WINDOW_STATE=CRACK", NULL };
		kobject_uevent_env(&lge_touch_sys_device.kobj, KOBJ_CHANGE, window_crack);
	}
	TOUCH_INFO_MSG("touch_f54_func end\n");
	f54_window_crack_check_mode = 0;
}
#endif

static void touch_lock_func(struct work_struct *work_touch_lock)
{
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_touch_lock), struct lge_touch_data, work_touch_lock);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	ts->ts_data.state = DO_NOT_ANYTHING;
}

/* check_log_finger_changed
 *
 * Check finger state change for Debug
 */
static void check_log_finger_changed(struct lge_touch_data *ts, u8 total_num)
{
	u16 tmp_p = 0;
	u16 tmp_r = 0;
	u16 id = 0;

	if (ts->ts_data.prev_total_num != total_num) {
		/* Finger added */
		if (ts->ts_data.prev_total_num <= total_num) {
			for(id=0; id < ts->pdata->caps->max_id; id++){
				if (ts->ts_data.curr_data[id].status == FINGER_PRESSED
						&& ts->ts_data.prev_data[id].status == FINGER_RELEASED) {
					break;
				}
			}
#ifdef ISIS_L2
			TOUCH_INFO_MSG("%d finger pressed : <%d> x[****] y[****] z[%3d]\n",
					total_num, id,
					ts->ts_data.curr_data[id].pressure);
#else
			TOUCH_INFO_MSG("%d finger pressed : <%d> x[%4d] y[%4d] z[%3d]\n",
					total_num, id,
					ts->ts_data.curr_data[id].x_position,
					ts->ts_data.curr_data[id].y_position,
					ts->ts_data.curr_data[id].pressure);
#endif
		} else {
		/* Finger subtracted */
			TOUCH_INFO_MSG("%d finger pressed\n", total_num);
		}
	} if (ts->ts_data.prev_total_num == total_num && total_num == 1) {
		/* Finger changed at one finger status - IC bug check */
		for (id = 0, tmp_p = 0; id < ts->pdata->caps->max_id; id++){
			/* find pressed */
			if (ts->ts_data.curr_data[id].status == FINGER_PRESSED
					&& ts->ts_data.prev_data[id].status == FINGER_RELEASED) {
				tmp_p = id;
			}
			/* find released */
			if (ts->ts_data.curr_data[id].status == FINGER_RELEASED
					&& ts->ts_data.prev_data[id].status == FINGER_PRESSED) {
				tmp_r = id;
			}
		}

		if (tmp_p != tmp_r
				&& (ts->ts_data.curr_data[tmp_p].status
						!= ts->ts_data.prev_data[tmp_p].status)) {
#ifdef ISIS_L2
			TOUCH_INFO_MSG("%d finger changed : <%d -> %d> x[****] y[****] z[%3d]\n",
					total_num, tmp_r, tmp_p,
					ts->ts_data.curr_data[tmp_p].pressure);
#else
#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
			if (likely(!quick_cover_status)) {
#endif
			TOUCH_INFO_MSG("%d finger changed : <%d -> %d> x[%4d] y[%4d] z[%3d]\n",
						total_num, tmp_r, tmp_p,
						ts->ts_data.curr_data[tmp_p].x_position,
						ts->ts_data.curr_data[tmp_p].y_position,
						ts->ts_data.curr_data[tmp_p].pressure);
#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
			}
#endif
#endif
		}
	}
}

/* check_log_finger_released
 *
 * Check finger state change for Debug
 */
static void check_log_finger_released(struct lge_touch_data *ts)
{
	u16 id = 0;

	for (id = 0; id < ts->pdata->caps->max_id; id++) {
		if (ts->ts_data.prev_data[id].status == FINGER_PRESSED) {
			break;
		}
	}

#ifdef ISIS_L2
	TOUCH_INFO_MSG("touch_release[%s] : <%d> x[****] y[****]\n",
			ts->ts_data.palm?"Palm":"", id);
#else
	TOUCH_INFO_MSG("touch_release[%s] : <%d> x[%4d] y[%4d]\n",
			ts->ts_data.palm?"Palm":"", id,
			ts->ts_data.prev_data[id].x_position,
			ts->ts_data.prev_data[id].y_position);
#endif
}

/* touch_work_pre_proc
 *
 * Pre-process work at touch_work
 */
static int touch_work_pre_proc(struct lge_touch_data *ts)
{
#ifdef CUST_G2_TOUCH
	int ret = 0;
#endif

	atomic_dec(&ts->next_work);
	ts->ts_data.total_num = 0;
	ts->int_pin_state = 0;

	if(unlikely(ts->work_sync_err_cnt >= MAX_RETRY_COUNT)){
		TOUCH_ERR_MSG("Work Sync Failed: Irq-pin has some unknown problems\n");
		return -EIO;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_START]);
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

#ifdef CUST_G2_TOUCH
	if ((ret = touch_device_func->data(ts->client, &ts->ts_data)) < 0) {
		TOUCH_ERR_MSG("get data fail %d\n", ret);
		return ret;
	}
#else
	if (touch_device_func->data(ts->client, &ts->ts_data) < 0) {
		TOUCH_ERR_MSG("get data fail\n");
		return -EIO;
	}
#endif

	if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE))
		ts->int_pin_state = gpio_get_value(ts->pdata->int_pin);

	/* Ghost finger solution */
	if (likely(ts->pdata->role->ghost_finger_solution_enable) && unlikely(ts->gf_ctrl.stage)){
		if(ghost_finger_solution(ts)){
			TOUCH_ERR_MSG("ghost_finger_solution was failed\n");
			return -EIO;
		}
	}

	/* Ghost Detect Solution */
	if (ts->pdata->role->ghost_detection_enable){
		if((ret = ghost_detect_solution(ts)) < 0)
			return ret;
	}

	/* Accuracy Solution */
	if (likely(ts->pdata->role->accuracy_filter_enable)){
		if (accuracy_filter_func(ts) < 0)
			return -EAGAIN;;
	}

	/* Jitter Solution */
	if (likely(ts->pdata->role->jitter_filter_enable)){
		if (jitter_filter_func(ts) < 0)
			return -EAGAIN;;
	}

	return 0;
}

/* touch_work_post_proc
 *
 * Post-process work at touch_work
 */
static void touch_work_post_proc(struct lge_touch_data *ts, int post_proc)
{
	int next_work = 0;

	if (post_proc >= WORK_POST_MAX)
		return;

	switch (post_proc) {
	case WORK_POST_OUT:
		if(likely(ts->pdata->role->operation_mode == INTERRUPT_MODE)){
			next_work = atomic_read(&ts->next_work);

			if(unlikely(ts->int_pin_state != 1 && next_work <= 0)){
				TOUCH_INFO_MSG("WARN: Interrupt pin is low - next_work: %d, try_count: %d]\n",
						next_work, ts->work_sync_err_cnt);
				post_proc = WORK_POST_ERR_RETRY;
				break;
			}
		}

#ifdef LGE_TOUCH_TIME_DEBUG
		do_gettimeofday(&t_debug[TIME_WORKQUEUE_END]);
		if (next_work)
			memset(t_debug, 0x0, sizeof(t_debug));
		time_profile_result(ts);
#endif

		ts->work_sync_err_cnt = 0;
		post_proc = WORK_POST_COMPLATE;
		break;

#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	case WORK_POST_OUT_IGNORE_INT:
#ifdef LGE_TOUCH_TIME_DEBUG
		do_gettimeofday(&t_debug[TIME_WORKQUEUE_END]);
		if (next_work)
			memset(t_debug, 0x0, sizeof(t_debug));
		time_profile_result(ts);
#endif
		ts->work_sync_err_cnt = 0;
		post_proc = WORK_POST_COMPLATE;
		break;
#endif

	case WORK_POST_ERR_RETRY:
		ts->work_sync_err_cnt++;
		atomic_inc(&ts->next_work);
		queue_work(touch_wq, &ts->work);
		post_proc = WORK_POST_COMPLATE;
		break;

	case WORK_POST_ERR_CIRTICAL:
		ts->work_sync_err_cnt = 0;
		safety_reset(ts);
		touch_ic_init(ts);
		post_proc = WORK_POST_COMPLATE;
		break;

	default:
		post_proc = WORK_POST_COMPLATE;
		break;
	}

	if (post_proc != WORK_POST_COMPLATE)
		touch_work_post_proc(ts, post_proc);
}

/* touch_work_func_a
 *
 * HARD_TOUCH_KEY
 */
static void touch_work_func_a(struct work_struct *work)
{
	struct lge_touch_data *ts =
			container_of(work, struct lge_touch_data, work);
	u8 report_enable = 0;
	int ret = 0;

	mutex_lock(&ts->irq_work_mutex);

	ret = touch_work_pre_proc(ts);
	if (ret == -EIO)
		goto err_out_critical;
	else if (ret == -EAGAIN)
		goto out;
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	else if (ret == -IGNORE_INTERRUPT)
		goto out_ignore_interrupt;
#endif
	/* Finger handle */
	if (ts->ts_data.state != TOUCH_ABS_LOCK) {
		if (!ts->ts_data.total_num) {
			touch_asb_input_report(ts, FINGER_RELEASED);
			report_enable = 1;

			queue_delayed_work(touch_wq, &ts->work_touch_lock, msecs_to_jiffies(200));

			if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS))) {
				if (ts->ts_data.prev_total_num)
					check_log_finger_released(ts);
			}

			ts->ts_data.prev_total_num = 0;

			/* Reset previous finger position data */
			memset(&ts->ts_data.prev_data, 0x0, sizeof(ts->ts_data.prev_data));
		} else if (ts->ts_data.total_num <= ts->pdata->caps->max_id) {
			cancel_delayed_work_sync(&ts->work_touch_lock);

			if (ts->gf_ctrl.stage == GHOST_STAGE_CLEAR || (ts->gf_ctrl.stage | GHOST_STAGE_1) || ts->gf_ctrl.stage == GHOST_STAGE_4){
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
				/* VU3 Touch : Setting touch button enable area where bottom of display area.*/
				ts->ts_data.state = DO_NOT_ANYTHING;
				if(ts->ts_data.total_num == 1 && ts->ts_data.curr_data[0].y_position <= TOUCH_BUTTON_ENABLE_Y_POSITION)
#endif
					ts->ts_data.state = TOUCH_BUTTON_LOCK;

			}
			/* key button cancel */
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
			if((ts->ts_data.prev_button.state == BUTTON_PRESSED  && ts->ts_data.curr_button.state == BUTTON_RELEASED &&ts->ts_data.state == TOUCH_BUTTON_LOCK)
				|| (ts->ts_data.prev_button.state == BUTTON_PRESSED  && ts->ts_data.state == TOUCH_BUTTON_LOCK)) {
#else
			if(ts->ts_data.prev_button.state == BUTTON_PRESSED && ts->ts_data.state == TOUCH_BUTTON_LOCK) {
#endif
				input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_CANCLED);
				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is canceled\n",
							ts->ts_data.prev_button.key_code);

				memset(&ts->ts_data.prev_button, 0x0, sizeof(ts->ts_data.prev_button));
			}

			if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS)))
				check_log_finger_changed(ts, ts->ts_data.total_num);

			ts->ts_data.prev_total_num = ts->ts_data.total_num;

			touch_asb_input_report(ts, FINGER_PRESSED);
			report_enable = 1;

			memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(ts->ts_data.curr_data));
		}

		/* Reset finger position data */
		memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));

		if (report_enable)
			input_sync(ts->input_dev);
	}

	/* Button handle */
	if (ts->ts_data.state != TOUCH_BUTTON_LOCK) {
		/* do not check when there is no pressed button at error case
		 * 	- if you check it, sometimes touch is locked becuase button pressed via IC error.
		 */
		if (ts->work_sync_err_cnt > 0
				&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
			/* Do nothing */
		} else {
			report_enable = 0;

			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
				TOUCH_INFO_MSG("Cur. button -code: %d state: %d, Prev. button -code: %d state: %d\n",
						ts->ts_data.curr_button.key_code,
						ts->ts_data.curr_button.state,
						ts->ts_data.prev_button.key_code,
						ts->ts_data.prev_button.state);

			if (ts->ts_data.curr_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
				/* button pressed */
				cancel_delayed_work_sync(&ts->work_touch_lock);

				input_report_key(ts->input_dev, ts->ts_data.curr_button.key_code, BUTTON_PRESSED);

				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is pressed\n",
							ts->ts_data.curr_button.key_code);

				memcpy(&ts->ts_data.prev_button, &ts->ts_data.curr_button,
						sizeof(ts->ts_data.curr_button));

				report_enable = 1;
			}else if (ts->ts_data.curr_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.key_code != ts->ts_data.curr_button.key_code) {
				/* exception case - multi press button handle */
				queue_delayed_work(touch_wq, &ts->work_touch_lock, msecs_to_jiffies(200));

				/* release previous pressed button */
				input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

				ts->ts_data.prev_button.state = BUTTON_RELEASED;

				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is released\n",
							ts->ts_data.prev_button.key_code);

				report_enable = 1;
			} else if (ts->ts_data.curr_button.state == BUTTON_RELEASED /* button released */
					&& ts->ts_data.prev_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.key_code == ts->ts_data.curr_button.key_code) {
				/* button release */
				input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

				if (likely(touch_debug_mask & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("Touch KEY[%d] is released\n",
							ts->ts_data.prev_button.key_code);

				memset(&ts->ts_data.prev_button, 0x0, sizeof(ts->ts_data.prev_button));
				memset(&ts->ts_data.curr_button, 0x0, sizeof(ts->ts_data.curr_button));

				report_enable = 1;
			}

			if (report_enable)
				input_sync(ts->input_dev);
		}
	}
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
out_ignore_interrupt:
	touch_work_post_proc(ts, WORK_POST_OUT_IGNORE_INT);
	mutex_unlock(&ts->irq_work_mutex);
	return;
#endif
out:
	touch_work_post_proc(ts, WORK_POST_OUT);
	mutex_unlock(&ts->irq_work_mutex);
	return;

err_out_critical:
	touch_work_post_proc(ts, WORK_POST_ERR_CIRTICAL);
	mutex_unlock(&ts->irq_work_mutex);
	return;

}

static bool is_in_section(struct rect rt, u16 x, u16 y)
{
	return x >= rt.left && x <= rt.right && y >= rt.top && y <= rt.bottom;
}

static u16 find_button(const struct t_data data, const struct section_info sc)
{
	int i;

	if (is_in_section(sc.panel, data.x_position, data.y_position))
		return KEY_PANEL;

	for(i=0; i<sc.b_num; i++){
		if (is_in_section(sc.button[i], data.x_position, data.y_position))
			return sc.b_name[i];
	}

	return KEY_BOUNDARY;
}

static bool check_cancel(u16 button, u16 x, u16 y, const struct section_info sc)
{
	int i;

	for(i=0; i<sc.b_num; i++){
		if (sc.b_name[i] == button)
			break;
	}

	if (i < sc.b_num){
		if (is_in_section(sc.button_cancel[i], x, y))
			return false;
	}

	return true;
}

/* touch_work_func_b
 *
 * SOFT_TOUCH_KEY
 */
static void touch_work_func_b(struct work_struct *work)
{
	struct lge_touch_data *ts =
			container_of(work, struct lge_touch_data, work);

	u8  i;
	u8 op_mode = OP_NULL;
	u16 tmp_button = KEY_NULL;
	u16 id = 0;
	int ret = 0;

	mutex_lock(&ts->irq_work_mutex);
	ret = touch_work_pre_proc(ts);
	if (ret == -EIO)
		goto err_out_critical;
	else if (ret == -EAGAIN)
		goto out;

	if (ts->ts_data.total_num == 0)
		op_mode = OP_RELEASE;
	else if (ts->ts_data.state == TOUCH_BUTTON_LOCK || ts->ts_data.state == TOUCH_ABS_LOCK)
		op_mode = OP_LOCK;
	else if (ts->ts_data.total_num == 1)
		op_mode = OP_SINGLE;
	else
		op_mode = OP_MULTI;

	switch(op_mode){
	case OP_RELEASE:
		if (ts->ts_data.prev_button.key_code == KEY_PANEL || ts->ts_data.prev_button.key_code == KEY_BOUNDARY
		    || ts->ts_data.prev_button.key_code == KEY_NULL)
			ts->ts_data.state = ABS_RELEASE;
		else
			ts->ts_data.state = BUTTON_RELEASE;

		ts->ts_data.curr_button.key_code = KEY_NULL;
		ts->ts_data.prev_total_num = 0;
		break;

	case OP_SINGLE:
		for (id = 0; id < ts->pdata->caps->max_id; id++){
			if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
				break;
			}
		}

		tmp_button = find_button(ts->ts_data.curr_data[id], ts->st_info);
		if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_INFO_MSG("button_now [%d]\n", tmp_button);

		if (ts->ts_data.prev_button.key_code != KEY_NULL && ts->ts_data.prev_button.key_code != KEY_BOUNDARY){
			if (ts->ts_data.prev_button.key_code == KEY_PANEL){
				if (ts->ts_data.prev_button.key_code != tmp_button)
					ts->ts_data.state = ABS_RELEASE;
				else
					ts->ts_data.state = ABS_PRESS;
			}
			else{
				if (check_cancel(ts->ts_data.prev_button.key_code, ts->ts_data.curr_data[id].x_position,
						ts->ts_data.curr_data[id].y_position, ts->st_info))
					ts->ts_data.state = BUTTON_CANCEL;
				else
					ts->ts_data.state = DO_NOT_ANYTHING;
			}
		}
		else{
			if (tmp_button == KEY_PANEL || tmp_button == KEY_BOUNDARY)
				ts->ts_data.state = ABS_PRESS;
			else
				ts->ts_data.state = BUTTON_PRESS;
		}

		if (ts->ts_data.state == ABS_PRESS || ts->ts_data.state == BUTTON_PRESS)
			ts->ts_data.curr_button.key_code = tmp_button;
		else if (ts->ts_data.state == BUTTON_RELEASE ||
				ts->ts_data.state == BUTTON_CANCEL || ts->ts_data.state == ABS_RELEASE)
			ts->ts_data.curr_button.key_code = KEY_NULL;
		break;

	case OP_MULTI:
		if (ts->ts_data.prev_button.key_code && ts->ts_data.prev_button.key_code != KEY_PANEL
				&& ts->ts_data.prev_button.key_code != KEY_BOUNDARY)
			ts->ts_data.state = BUTTON_CANCEL;
		else
			ts->ts_data.state = ABS_PRESS;
		ts->ts_data.curr_button.key_code = KEY_PANEL;
		break;

	case OP_LOCK:
		for (id = 0; id < ts->pdata->caps->max_id; id++){
			if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
				if (ts->ts_data.curr_data[id].y_position < ts->pdata->caps->y_button_boundary){
					ts->ts_data.curr_button.key_code = KEY_PANEL;
					ts->ts_data.state = ABS_PRESS;
				}
			}
		}
		break;

	default:
		break;
	}

	if (unlikely(touch_debug_mask & (DEBUG_ABS |DEBUG_BUTTON)))
		TOUCH_INFO_MSG("op_mode[%d] state[%d]\n", op_mode, ts->ts_data.state);

	switch(ts->ts_data.state){
	case ABS_PRESS:
abs_report:
		i=0;

		i = touch_asb_input_report(ts, FINGER_PRESSED);

		if (!i) {
			touch_asb_input_report(ts, FINGER_RELEASED);
			ts->ts_data.prev_total_num = 0;
		}
		else {
			if (likely(touch_debug_mask & DEBUG_ABS))
				check_log_finger_changed(ts, i);

			ts->ts_data.prev_total_num = i;
		}
		break;
	case ABS_RELEASE:
		touch_asb_input_report(ts, FINGER_RELEASED);
		break;
	case BUTTON_PRESS:
		input_report_key(ts->input_dev, ts->ts_data.curr_button.key_code, BUTTON_PRESSED);
			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_INFO_MSG("Touch KEY[%d] is pressed\n", ts->ts_data.curr_button.key_code);
		break;
	case BUTTON_RELEASE:
		input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);
			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_INFO_MSG("Touch KEY[%d] is released\n", ts->ts_data.prev_button.key_code);
		break;
	case BUTTON_CANCEL:
		input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_CANCLED);
		if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_INFO_MSG("Touch KEY[%d] is canceled\n", ts->ts_data.prev_button.key_code);
		if (ts->ts_data.curr_data[0].y_position < ts->pdata->caps->y_button_boundary){
			input_sync(ts->input_dev);
			goto abs_report;
		}
		break;
	case TOUCH_BUTTON_LOCK:
	case TOUCH_ABS_LOCK:
		goto out;
		break;
	default:
		break;
	}

	input_sync(ts->input_dev);

	if (likely(touch_debug_mask & DEBUG_BASE_INFO)){
		if (ts->ts_data.state == ABS_RELEASE)
			check_log_finger_released(ts);
		if(ts->ts_data.state == BUTTON_RELEASE)
			TOUCH_INFO_MSG("touch_release : button[%d]\n", ts->ts_data.prev_button.key_code);
	}

	if (op_mode == OP_SINGLE && ts->ts_data.state == ABS_RELEASE)
			ts->ts_data.state = TOUCH_ABS_LOCK;

	if (ts->ts_data.state == BUTTON_CANCEL)
		ts->ts_data.state = TOUCH_BUTTON_LOCK;

	memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(ts->ts_data.curr_data));
	memcpy(&ts->ts_data.prev_button, &ts->ts_data.curr_button, sizeof(ts->ts_data.curr_button));

out:
	touch_work_post_proc(ts, WORK_POST_OUT);
	mutex_unlock(&ts->irq_work_mutex);
	return;

err_out_critical:
	touch_work_post_proc(ts, WORK_POST_ERR_CIRTICAL);
	mutex_unlock(&ts->irq_work_mutex);
	return;
}

static void touch_work_func_c(struct work_struct *work)
{
	struct lge_touch_data *ts =
			container_of(work, struct lge_touch_data, work);
	u8 report_enable = 0;
	int ret = 0;

	mutex_lock(&ts->irq_work_mutex);
	ret = touch_work_pre_proc(ts);
	if (ret == -EIO)
		goto err_out_critical;
	else if (ret == -EAGAIN)
		goto out;
#ifdef CUST_G2_TOUCH
	else if (ret == -IGNORE_INTERRUPT)
		goto out1;
#endif

	if (!ts->ts_data.total_num) {
		touch_asb_input_report(ts, FINGER_RELEASED);
		report_enable = 1;

		if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS))) {
			if (ts->ts_data.prev_total_num)
				check_log_finger_released(ts);
		}

		ts->ts_data.prev_total_num = 0;

#ifdef CUST_G2_TOUCH
		/* Reset previous finger position data */
		memset(&ts->ts_data.prev_data, 0x0, sizeof(ts->ts_data.prev_data));
#endif
	} else if (ts->ts_data.total_num <= ts->pdata->caps->max_id) {
		if (likely(touch_debug_mask & (DEBUG_BASE_INFO | DEBUG_ABS)))
			check_log_finger_changed(ts, ts->ts_data.total_num);

		ts->ts_data.prev_total_num = ts->ts_data.total_num;

		touch_asb_input_report(ts, FINGER_PRESSED);
		report_enable = 1;

		memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(ts->ts_data.curr_data));
	}

	/* Reset finger position data */
	memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));

	if (report_enable)
		input_sync(ts->input_dev);

out:
	touch_work_post_proc(ts, WORK_POST_OUT);
out1:
	mutex_unlock(&ts->irq_work_mutex);
	return;

err_out_critical:
	touch_work_post_proc(ts, WORK_POST_ERR_CIRTICAL);
	mutex_unlock(&ts->irq_work_mutex);
	return;
}


/* touch_fw_upgrade_func
 *
 * it used to upgrade the firmware of touch IC.
 */
static void touch_fw_upgrade_func(struct work_struct *work_fw_upgrade)
{
	struct lge_touch_data *ts =
			container_of(work_fw_upgrade, struct lge_touch_data, work_fw_upgrade);
	u8	saved_state = ts->curr_pwr_state;

#if defined(CONFIG_MACH_MSM8974_G2_KDDI)
    if(lge_get_board_revno() <= HW_REV_B){
        TOUCH_INFO_MSG("HW_REV : %d, Touch FW Update skipped ", lge_get_board_revno());
        goto out;
    }
#endif
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func->fw_upgrade == NULL) {
		TOUCH_INFO_MSG("There is no specific firmware upgrade function\n");
		goto out;
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("IC identifier[%s] fw_version[%s:%s] : force[%d]\n",
				ts->fw_info.ic_fw_identifier, ts->fw_info.ic_fw_version,
		ts->fw_info.syna_img_fw_version, ts->fw_info.fw_upgrade.fw_force_upgrade);

	ts->fw_info.fw_upgrade.is_downloading = UNDER_DOWNLOADING;

#ifdef CUST_G2_TOUCH
	if(ts->fw_info.fw_force_rework) {
		TOUCH_INFO_MSG("FW-upgrade Force Rework.\n");
	} else {
		TOUCH_INFO_MSG("ic %s img %s\n",ts->fw_info.ic_fw_version,ts->fw_info.syna_img_fw_version);
#ifdef CONFIG_LGE_Z_TOUCHSCREEN
		if (strncmp(ts->fw_info.ic_fw_identifier, "PLG185", 6)) {
			TOUCH_INFO_MSG("There is no IC of Touch. FW-upgrade is not executed\n");
			goto out;
		}

		if( ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >=
			 (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
			 && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
			TOUCH_INFO_MSG("FW-upgrade is not executed\n");
			goto out;
		}

		if (ts->fw_info.ic_fw_version[0] != 'E' && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
			TOUCH_INFO_MSG("FW-upgrade is not executed : ic_fw_version is not official version\n");
			goto out;
		}
#else
#ifdef CONFIG_LGE_VU3_TOUCHSCREEN
		if (strncmp(ts->fw_info.ic_fw_identifier, "PLG238", 6)) {
			TOUCH_INFO_MSG("There is no IC of Touch. FW-upgrade is not executed\n");
			goto out;
		}

		if( (int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) < 4
			&& !ts->fw_info.fw_upgrade.fw_force_upgrade) {
			TOUCH_INFO_MSG("Old touch panel FW-upgrade is not executed\n");
			goto out;
		} else{
			TOUCH_INFO_MSG("New touch panel..\n");
		}

		if( ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >=
			 (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
			 && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
			TOUCH_INFO_MSG("FW-upgrade is not executed\n");
			goto out;
		}
#else
#if defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
if ((!strncmp(ts->fw_info.ic_fw_identifier, "PLG208", 6)) || (!strncmp(ts->fw_info.ic_fw_identifier, "PLG244", 6))
		|| (!strncmp(ts->fw_info.ic_fw_identifier, "PLG270", 6))) {
	TOUCH_INFO_MSG("ic_fw_version = %d\n",(int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10));
	TOUCH_INFO_MSG("syna_img_fw_version = %d\n",(int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10));

	if(ts->fw_info.fw_setting.ic_chip_rev == 2) {
		/* SSUNTEL G1F ic_chip_rev = 2, touch id = 1.8V */
		TOUCH_INFO_MSG("ic_chip_rev = 2, touch id = 1.8V\n");

		if( (int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >= 16) {
			TOUCH_INFO_MSG("Touch IC has wrong FW, SSUNTEL G1F FW-upgrade is executed\n");
		} else if( (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10) > 16) {
			TOUCH_INFO_MSG("Touch img is not for SSUNTEL G1F, FW-upgrade is not executed!\n");
			goto out;
		} else{
			TOUCH_INFO_MSG("SSUNTEL G1F FW-upgrade is checking...\n");
			if( ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >=
				 (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
				 && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
				TOUCH_INFO_MSG("SSUNTEL G1F FW-upgrade is not executed\n");
				goto out;
			} else {
				TOUCH_INFO_MSG("SSUNTEL G1F FW-upgrade is executed\n");
			}
		}
	} else if (ts->fw_info.fw_setting.ic_chip_rev == 1){
		/* LGIT G1F ic_chip_rev = 1, touch id = 0V */
		TOUCH_INFO_MSG("ic_chip_rev = 1, touch id = 0V\n");

		if( (int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) > 50) {
			TOUCH_INFO_MSG("Touch IC has wrong FW, LGIT G1F FW-upgrade is executed\n");
		} else if( (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10) > 50) {
			TOUCH_INFO_MSG("Touch img is not for LGIT G1F, FW-upgrade is not executed\n");
			goto out;
		} else{
			TOUCH_INFO_MSG("LGIT G1F FW-upgrade is checking...\n");
			if( ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >=
				 (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
				 && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
				TOUCH_INFO_MSG("LGIT G1F FW-upgrade is not executed\n");
				goto out;
			} else {
				TOUCH_INFO_MSG("LGIT G1F FW-upgrade is executed\n");
			}
		}
	} else if (ts->fw_info.fw_setting.ic_chip_rev == 3){
		/* LGIT G2 Hybrid  ic_chip_rev = 3, touch id = 1.8V */
		TOUCH_INFO_MSG("ic_chip_rev = 3, touch id = 1.8V\n");

		if( (int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) < 50) {
			TOUCH_INFO_MSG("Touch IC has wrong FW, LGIT G2 FW-upgrade is executed\n");
		} else if( (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10) < 50) {
			TOUCH_INFO_MSG("Touch img is not for LGIT G1F, FW-upgrade is not executed\n");
			goto out;
		} else{
			TOUCH_INFO_MSG("LGIT G2 Hybrid FW-upgrade is checking...\n");
			if( ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >=
				 (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
				 && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
				TOUCH_INFO_MSG("LGIT G1F FW-upgrade is not executed\n");
				goto out;
			} else {
				TOUCH_INFO_MSG("LGIT G1F FW-upgrade is executed\n");
			}
		}
	} else {
		TOUCH_INFO_MSG("ic_chip_rev is invalid..., FW-upgrade is Skipped!\n");
		goto out;
	}
} else {
	TOUCH_INFO_MSG("There is no Touch IC or Empty Touch IC\n");
	TOUCH_INFO_MSG("ic_fw_version = %d\n",(int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10));
	TOUCH_INFO_MSG("syna_img_fw_version = %d\n",(int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10));

	if(!strncmp(ts->fw_info.ic_fw_identifier, "s3404", 5)) {
		TOUCH_INFO_MSG("Touch IC is initial mode!! FW-upgrade is executed\n");
	} else if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_A) {
		TOUCH_INFO_MSG("IC F/W Revision is not REV B. FW-upgrade is not executed\n");
		goto out;
	} else if(strncmp(ts->fw_info.ic_fw_version, ts->fw_info.syna_img_fw_version, 1)){
		switch(ts->fw_info.fw_setting.curr_touch_vendor) {
			case TOUCH_VENDOR_LGIT:
				TOUCH_INFO_MSG("Panel changed [%s -> %s]!! FW-upgrade is executed\n", "TPK", "LGIT");
				break;
			case TOUCH_VENDOR_TPK:
				TOUCH_INFO_MSG("Panel changed [%s -> %s]!! FW-upgrade is executed\n", "LGIT", "TPK");
				break;
			default:
				TOUCH_INFO_MSG("Panel changed [unknown]!! FW-upgrade is executed\n");
				break;
		}
	} else if( ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >=
		 (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
		 && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
		TOUCH_INFO_MSG("FW-upgrade is not executed\n");
		goto out;
	} else {
		TOUCH_INFO_MSG("FW-upgrade is executed\n");
	}
}
#else
		if(!strncmp(ts->fw_info.ic_fw_identifier, "s3404", 5)) {
			TOUCH_INFO_MSG("Touch IC is initial mode!! FW-upgrade is executed\n");
		} else if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_A) {
		    TOUCH_INFO_MSG("IC F/W Revision is not REV B. FW-upgrade is not executed\n");
		    goto out;
		} else if(strncmp(ts->fw_info.ic_fw_version, ts->fw_info.syna_img_fw_version, 1)){
			switch(ts->fw_info.fw_setting.curr_touch_vendor) {
				case TOUCH_VENDOR_LGIT:
					TOUCH_INFO_MSG("Panel changed [%s -> %s]!! FW-upgrade is executed\n", "TPK", "LGIT");
					break;
				case TOUCH_VENDOR_TPK:
					TOUCH_INFO_MSG("Panel changed [%s -> %s]!! FW-upgrade is executed\n", "LGIT", "TPK");
					break;
				default:
					TOUCH_INFO_MSG("Panel changed [unknown]!! FW-upgrade is executed\n");
					break;
			}
		} else if( ((int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) >=
			 (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
			 && !ts->fw_info.fw_upgrade.fw_force_upgrade) {
			TOUCH_INFO_MSG("FW-upgrade is not executed\n");
			goto out;
		} else {
			TOUCH_INFO_MSG("FW-upgrade is executed\n");
		}
#endif	//#ifdef CONFIG_MACH_MSM8974_G2_OPEN_COM
#endif	//#ifdef CONFIG_MACH_MSM8974_VU3_KR
#endif	//#ifdef CONFIG_LGE_Z_TOUCHSCREEN
	}
#else
	if ((!strcmp(ts->pdata->fw_version, ts->fw_info.ic_fw_version)
				|| ts->pdata->fw_version == NULL)
			&& !ts->fw_info.fw_upgrade.fw_force_upgrade){
		TOUCH_INFO_MSG("FW-upgrade is not executed\n");
		goto out;
	}
#endif

	if (ts->curr_pwr_state == POWER_ON || ts->curr_pwr_state == POWER_WAKE) {
		if (ts->pdata->role->operation_mode)
			mutex_lock(&ts->irq_work_mutex);
		else
			hrtimer_cancel(&ts->timer);
	}
#ifdef CUST_G2_TOUCH
	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}
#endif

	if (ts->curr_pwr_state == POWER_OFF) {
		touch_power_cntl(ts, POWER_ON);
		msleep(ts->pdata->role->booting_delay);
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W upgrade - Start\n");

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_START]);
#endif

	if (touch_device_func->fw_upgrade(ts->client, &ts->fw_info) < 0) {
		TOUCH_ERR_MSG("Firmware upgrade was failed\n");
		if (ts->pdata->role->operation_mode)
			mutex_unlock(&ts->irq_work_mutex);
		else
			hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

		goto err_out;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);
#endif

	touch_power_cntl(ts, POWER_OFF);

	if (saved_state != POWER_OFF) {
		touch_power_cntl(ts, POWER_ON);
		msleep(ts->pdata->role->booting_delay);

		if (ts->pdata->role->operation_mode)
			mutex_unlock(&ts->irq_work_mutex);
		else
			hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

		touch_ic_init(ts);

		if(saved_state == POWER_WAKE || saved_state == POWER_SLEEP)
			touch_power_cntl(ts, saved_state);
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE |DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W upgrade - Finish\n");

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);

	if (touch_time_debug_mask & DEBUG_TIME_FW_UPGRADE
			|| touch_time_debug_mask & DEBUG_TIME_PROFILE_ALL) {
		TOUCH_INFO_MSG("FW upgrade time is under %3lu.%06lusec\n",
				get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_sec, t_debug[TIME_FW_UPGRADE_START].tv_sec),
				get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_usec, t_debug[TIME_FW_UPGRADE_START].tv_usec));
	}
#endif

	goto out;

err_out:
	mutex_lock(&ts->irq_work_mutex);
	safety_reset(ts);
	mutex_unlock(&ts->irq_work_mutex);
	touch_ic_init(ts);

out:
	memset(&ts->fw_info.fw_upgrade, 0, sizeof(ts->fw_info.fw_upgrade));
	ts->fw_info.fw_force_rework = false;
	return;
}

/* touch_irq_handler
 *
 * When Interrupt occurs, it will be called before touch_thread_irq_handler.
 *
 * return
 * IRQ_HANDLED: touch_thread_irq_handler will not be called.
 * IRQ_WAKE_THREAD: touch_thread_irq_handler will be called.
 */
static irqreturn_t touch_irq_handler(int irq, void *dev_id)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

	if (unlikely(atomic_read(&ts->device_init) != 1))
		return IRQ_HANDLED;

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_ISR_START]);
#endif

	atomic_inc(&ts->next_work);
	return IRQ_WAKE_THREAD;
}

/* touch_thread_irq_handler
 *
 * 1. disable irq.
 * 2. enqueue the new work.
 * 3. enalbe irq.
 */
static irqreturn_t touch_thread_irq_handler(int irq, void *dev_id)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_THREAD_ISR_START]);
#endif

#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
	if(ts_suspend && touch_gesture_enable){
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
		wake_lock_timeout(&touch_wake_lock, msecs_to_jiffies(3000));
#else
		wake_lock_timeout(&touch_wake_lock, msecs_to_jiffies(1000));
#endif
		TOUCH_INFO_MSG("gesture wakeup\n");
#ifndef CONFIG_MACH_MSM8974_G2_KR
#ifdef I2C_SUSPEND_WORKAROUND
		queue_delayed_work(touch_wq, &ts->check_suspended_work, 0);
#else
		queue_delayed_work(touch_wq, &ts->work_gesture_wakeup,
				msecs_to_jiffies(0));
#endif
#else
		queue_delayed_work(touch_wq, &ts->work_gesture_wakeup,
				msecs_to_jiffies(0));
#endif
		return IRQ_HANDLED;
	}
#endif
#ifdef CONFIG_MACH_MSM8974_G2_KR
#ifdef I2C_SUSPEND_WORKAROUND
	queue_delayed_work(touch_wq, &ts->check_suspended_work, 0);
#else
	queue_work(touch_wq, &ts->work);
#endif
#else
	queue_work(touch_wq, &ts->work);
#endif

	return IRQ_HANDLED;
}

/* touch_timer_handler
 *
 * it will be called when timer interrupt occurs.
 */
static enum hrtimer_restart touch_timer_handler(struct hrtimer *timer)
{
	struct lge_touch_data *ts =
			container_of(timer, struct lge_touch_data, timer);

	atomic_inc(&ts->next_work);
	queue_work(touch_wq, &ts->work);
	hrtimer_start(&ts->timer,
			ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

#ifdef I2C_SUSPEND_WORKAROUND
static void synaptics_touch_check_suspended_worker(struct work_struct *check_suspended_work)
{
	struct lge_touch_data *ts =
		container_of(to_delayed_work(check_suspended_work), struct lge_touch_data, check_suspended_work);
	if (atmel_touch_i2c_suspended){
		dev_err(&ts->client->dev, "lge_touch touch suspended. try i2c operation after 10ms.\n");
		queue_delayed_work(touch_wq, &ts->check_suspended_work, msecs_to_jiffies(10));
		return;
	} else {
		dev_dbg(&ts->client->dev, "lge_touch touch resume. do touch work.\n");
#ifndef CONFIG_MACH_MSM8974_G2_KR
		queue_delayed_work(touch_wq, &ts->work_gesture_wakeup, msecs_to_jiffies(0));
#else
		queue_work(touch_wq, &ts->work);
#endif
		return;
	}
}
#endif

/* check_platform_data
 *
 * check-list
 * 1. Null Pointer
 * 2. lcd, touch screen size
 * 3. button support
 * 4. operation mode
 * 5. power module
 * 6. report period
 */
static int check_platform_data(struct touch_platform_data *pdata)
{
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (!pdata)
		return -1;

	if (!pdata->caps || !pdata->role || !pdata->pwr)
		return -1;

	if (!pdata->caps->lcd_x || !pdata->caps->lcd_y || !pdata->caps->x_max || !pdata->caps->y_max) {
		TOUCH_ERR_MSG("lcd_x, lcd_y, x_max, y_max are should be defined\n");
		return -1;
	}

	if (pdata->caps->button_support) {
		if (!pdata->role->key_type) {
			TOUCH_ERR_MSG("button_support = 1, but key_type is not defined\n");
			return -1;
		}

		if (!pdata->caps->y_button_boundary) {
			if (pdata->role->key_type == TOUCH_HARD_KEY)
				pdata->caps->y_button_boundary = pdata->caps->y_max;
			else
				pdata->caps->y_button_boundary =
						(pdata->caps->lcd_y * pdata->caps->x_max) / pdata->caps->lcd_x;
		}

		if (pdata->caps->button_margin < 0 || pdata->caps->button_margin > 49) {
			pdata->caps->button_margin = 10;
			TOUCH_ERR_MSG("0 < button_margin < 49, button_margin is set 10 by force\n");
		}
	}

	if (!pdata->role->operation_mode) {
		if (!pdata->role->report_period) {
			TOUCH_ERR_MSG("polling mode needs report_period\n");
			return -1;
		}
	}

	if (pdata->role->suspend_pwr == POWER_OFF || pdata->role->suspend_pwr == POWER_SLEEP){
		if (pdata->role->suspend_pwr == POWER_OFF)
			pdata->role->resume_pwr = POWER_ON;
		else
			pdata->role->resume_pwr = POWER_WAKE;
	}
	else {
		TOUCH_ERR_MSG("suspend_pwr = POWER_OFF or POWER_SLEEP\n");
	}

	if (pdata->pwr->use_regulator) {
		if (!pdata->pwr->vdd[0] || !pdata->pwr->vio[0]) {
			TOUCH_ERR_MSG("you should assign the name of vdd and vio\n");
			return -1;
		}
	}
	else{
		if (!pdata->pwr->power) {
			TOUCH_ERR_MSG("you should assign the power-control-function\n");
			return -1;
		}
	}

	if (pdata->role->report_period == 0)
		pdata->role->report_period = 12500000;

	if (pdata->caps->max_id > MAX_FINGER)
		pdata->caps->max_id = MAX_FINGER;

	return 0;
}

/* get_section
 *
 * it calculates the area of touch-key, automatically.
 */
void get_section(struct section_info* sc, struct touch_platform_data *pdata)
{
	int i;

	sc->panel.left = 0;
	sc->panel.right = pdata->caps->x_max;
	sc->panel.top = 0;
	sc->panel.bottom = pdata->caps->y_button_boundary;

	sc->b_width  = pdata->caps->x_max / pdata->caps->number_of_button;
	sc->b_margin = sc->b_width * pdata->caps->button_margin / 100;
	sc->b_inner_width = sc->b_width - (2*sc->b_margin);
	sc->b_height = pdata->caps->y_max - pdata->caps->y_button_boundary;
	sc->b_num = pdata->caps->number_of_button;

	for(i=0; i<sc->b_num; i++){
		sc->button[i].left = i * (pdata->caps->x_max / pdata->caps->number_of_button) + sc->b_margin;
		sc->button[i].right = sc->button[i].left + sc->b_inner_width;
		sc->button[i].top = pdata->caps->y_button_boundary + 1;
		sc->button[i].bottom = pdata->caps->y_max;

		sc->button_cancel[i].left = sc->button[i].left - (2*sc->b_margin) >= 0 ?
				sc->button[i].left - (2*sc->b_margin) : 0;
		sc->button_cancel[i].right = sc->button[i].right + (2*sc->b_margin) <= pdata->caps->x_max ?
				sc->button[i].right + (2*sc->b_margin) : pdata->caps->x_max;
		sc->button_cancel[i].top = sc->button[i].top;
		sc->button_cancel[i].bottom = sc->button[i].bottom;

		sc->b_name[i] = pdata->caps->button_name[i];
	}
}

/* Sysfs
 *
 * For debugging easily, we added some sysfs.
 */
static ssize_t show_platform_data(struct lge_touch_data *ts, char *buf)
{
	struct touch_platform_data *pdata = ts->pdata;
	int ret = 0;

	ret = sprintf(buf, "====== Platform data ======\n");
	ret += sprintf(buf+ret, "int_pin[%d] reset_pin[%d]\n", pdata->int_pin, pdata->reset_pin);
	ret += sprintf(buf+ret, "caps:\n");
	ret += sprintf(buf+ret, "\tbutton_support        = %d\n", pdata->caps->button_support);
	ret += sprintf(buf+ret, "\ty_button_boundary     = %d\n", pdata->caps->y_button_boundary);
	ret += sprintf(buf+ret, "\tbutton_margin         = %d\n", pdata->caps->button_margin);
	ret += sprintf(buf+ret, "\tnumber_of_button      = %d\n", pdata->caps->number_of_button);
	ret += sprintf(buf+ret, "\tbutton_name           = %d, %d, %d, %d\n", pdata->caps->button_name[0],
			pdata->caps->button_name[1], pdata->caps->button_name[2], pdata->caps->button_name[3]);
	ret += sprintf(buf+ret, "\tis_width_supported    = %d\n", pdata->caps->is_width_supported);
	ret += sprintf(buf+ret, "\tis_pressure_supported = %d\n", pdata->caps->is_pressure_supported);
	ret += sprintf(buf+ret, "\tis_id_supported       = %d\n", pdata->caps->is_id_supported);
	ret += sprintf(buf+ret, "\tmax_width             = %d\n", pdata->caps->max_width);
	ret += sprintf(buf+ret, "\tmax_pressure          = %d\n", pdata->caps->max_pressure);
	ret += sprintf(buf+ret, "\tmax_id                = %d\n", pdata->caps->max_id);
	ret += sprintf(buf+ret, "\tx_max                 = %d\n", pdata->caps->x_max);
	ret += sprintf(buf+ret, "\ty_max                 = %d\n", pdata->caps->y_max);
	ret += sprintf(buf+ret, "\tlcd_x                 = %d\n", pdata->caps->lcd_x);
	ret += sprintf(buf+ret, "\tlcd_y                 = %d\n", pdata->caps->lcd_y);
	ret += sprintf(buf+ret, "role:\n");
	ret += sprintf(buf+ret, "\toperation_mode        = %d\n", pdata->role->operation_mode);
	ret += sprintf(buf+ret, "\tkey_type              = %d\n", pdata->role->key_type);
	ret += sprintf(buf+ret, "\treport_mode           = %d\n", pdata->role->report_mode);
	ret += sprintf(buf+ret, "\tdelta_pos_threshold   = %d\n", pdata->role->delta_pos_threshold);
	ret += sprintf(buf+ret, "\torientation           = %d\n", pdata->role->orientation);
	ret += sprintf(buf+ret, "\treport_period         = %d\n", pdata->role->report_period);
	ret += sprintf(buf+ret, "\tbooting_delay         = %d\n", pdata->role->booting_delay);
	ret += sprintf(buf+ret, "\treset_delay           = %d\n", pdata->role->reset_delay);
	ret += sprintf(buf+ret, "\tsuspend_pwr           = %d\n", pdata->role->suspend_pwr);
	ret += sprintf(buf+ret, "\tresume_pwr            = %d\n", pdata->role->resume_pwr);
	ret += sprintf(buf+ret, "\tjitter_filter_enable            = %d\n", pdata->role->jitter_filter_enable);
	ret += sprintf(buf+ret, "\tjitter_curr_ratio            = %d\n", pdata->role->jitter_curr_ratio);
	ret += sprintf(buf+ret, "\taccuracy_filter_enable            = %d\n", pdata->role->accuracy_filter_enable);
	ret += sprintf(buf+ret, "\tirqflags              = 0x%lx\n", pdata->role->irqflags);
#ifdef CUST_G2_TOUCH
	ret += sprintf(buf+ret, "\tghost_detection_enable= %d\n", pdata->role->ghost_detection_enable);
#endif
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
	ret += sprintf(buf+ret, "\tdouble_tap_wakeup_mode= %d\n", touch_gesture_enable);
#endif
	ret += sprintf(buf+ret, "pwr:\n");
	ret += sprintf(buf+ret, "\tuse_regulator         = %d\n", pdata->pwr->use_regulator);
	ret += sprintf(buf+ret, "\tvdd                   = %s\n", pdata->pwr->vdd);
	ret += sprintf(buf+ret, "\tvdd_voltage           = %d\n", pdata->pwr->vdd_voltage);
	ret += sprintf(buf+ret, "\tvio                   = %s\n", pdata->pwr->vio);
	ret += sprintf(buf+ret, "\tvio_voltage           = %d\n", pdata->pwr->vio_voltage);
	ret += sprintf(buf+ret, "\tpower                 = %s\n", pdata->pwr->power ? "YES" : "NO");
	return ret;
}

/* show_fw_info
 *
 * show only the firmware information
 */
static ssize_t show_fw_info(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "\n====== Firmware Info ======\n");
	ret += sprintf(buf+ret, "ic_fw_identifier           = %s\n", ts->fw_info.ic_fw_identifier);
	ret += sprintf(buf+ret, "ic_fw_version              = %s\n", ts->fw_info.ic_fw_version);
#ifdef CUST_G2_TOUCH
	ret += sprintf(buf+ret, "img_fw_product_id          = %s\n", ts->fw_info.syna_img_fw_product_id);
	ret += sprintf(buf+ret, "img_fw_version             = %s\n", ts->fw_info.syna_img_fw_version);
	ret += sprintf(buf+ret, "ic_chip_rev                = %u\n", ts->fw_info.fw_setting.ic_chip_rev);
	ret += sprintf(buf+ret, "bootloader_fw_ver          = %d\n", ts->fw_info.fw_setting.bootloader_fw_ver);
#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
	ret += sprintf(buf+ret, "customer_family			= %d\n", ts->fw_info.fw_setting.customer_family);
#endif
	ret += sprintf(buf+ret, "lcd_panel_type(boot)       = %d\n", lge_get_panel_maker());
	ret += sprintf(buf+ret, "lcd_panel_type(current)    = %d\n", get_panel_maker_id());
#endif

	return ret;
}

/* store_fw_upgrade
 *
 * User can upgrade firmware, anytime, using this module.
 * Also, user can use both binary-img(SDcard) and header-file(Kernel image).
 */
static ssize_t store_fw_upgrade(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value = 0;
	int repeat = 0;
	char path[256] = {0};

	sscanf(buf, "%d %s", &value, path);

	printk(KERN_INFO "\n");
	TOUCH_INFO_MSG("Firmware image path: %s\n", path[0] != 0 ? path : "Internal");

	if (value) {
		for(repeat = 0; repeat < value; repeat++) {
			/* sync for n-th repeat test */
			while(ts->fw_info.fw_upgrade.is_downloading);

			msleep(ts->pdata->role->booting_delay * 2);
			printk(KERN_INFO "\n");
			TOUCH_INFO_MSG("Firmware image upgrade: No.%d", repeat+1);

			/* for n-th repeat test - because ts->fw_info.fw_upgrade is setted 0 after FW upgrade */
			memcpy(ts->fw_info.fw_upgrade.fw_path, path, sizeof(ts->fw_info.fw_upgrade.fw_path)-1);

			/* set downloading flag for sync for n-th test */
			ts->fw_info.fw_upgrade.is_downloading = UNDER_DOWNLOADING;
			ts->fw_info.fw_upgrade.fw_force_upgrade = 1;

			queue_work(touch_wq, &ts->work_fw_upgrade);
		}

		/* sync for fw_upgrade test */
		while(ts->fw_info.fw_upgrade.is_downloading);
	}

	return count;
}

/* show_fw_ver
 *
 * show only firmware version.
 * It will be used for AT-COMMAND
 */
static ssize_t show_fw_ver(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%s\n", ts->fw_info.ic_fw_version);
	return ret;
}

/* show_section_info
 *
 * User can check the information of touch-key-area, using this module.
 */
static ssize_t show_section_info(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	int i;

	ret = sprintf(buf, "====== Section Info ======\n");

	ret += sprintf(buf+ret, "Panel = [%4d,%4d,%4d,%4d]\n", ts->st_info.panel.left,
			ts->st_info.panel.right, ts->st_info.panel.top, ts->st_info.panel.bottom);

	if (ts->pdata->role->key_type == TOUCH_SOFT_KEY) {
		for(i=0; i<ts->st_info.b_num; i++){
			ret += sprintf(buf+ret, "Button[%4d] = [%4d,%4d,%4d,%4d]\n",
					ts->st_info.b_name[i], ts->st_info.button[i].left,
					ts->st_info.button[i].right, ts->st_info.button[i].top, ts->st_info.button[i].bottom);
		}
		for(i=0; i<ts->st_info.b_num; i++){
			ret += sprintf(buf+ret, "Button_cancel[%4d] = [%4d,%4d,%4d,%4d]\n",
					ts->st_info.b_name[i], ts->st_info.button_cancel[i].left,
					ts->st_info.button_cancel[i].right, ts->st_info.button_cancel[i].top,
					ts->st_info.button_cancel[i].bottom);
		}
		ret += sprintf(buf+ret, "button_width        = %d\n", ts->st_info.b_width);
		ret += sprintf(buf+ret, "button_height       = %d\n", ts->st_info.b_height);
		ret += sprintf(buf+ret, "button_inner_width  = %d\n", ts->st_info.b_inner_width);
		ret += sprintf(buf+ret, "button_margin       = %d\n", ts->st_info.b_margin);
		ret += sprintf(buf+ret, "button_number       = %d\n", ts->st_info.b_num);
	}

	return ret;
}

/* store_ts_reset
 *
 * Reset the touch IC.
 */
static ssize_t store_ts_reset(struct lge_touch_data *ts, const char *buf, size_t count)
{
	unsigned char string[5];
	u8 saved_state = ts->curr_pwr_state;
	int ret = 0;

	sscanf(buf, "%s", string);

	if (ts->pdata->role->operation_mode)
		mutex_lock(&ts->irq_work_mutex);
	else
		hrtimer_cancel(&ts->timer);

#ifdef CUST_G2_TOUCH
	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}
#endif

	cancel_work_sync(&ts->work);
	cancel_delayed_work_sync(&ts->work_init);
#ifdef CUST_G2_TOUCH
	cancel_delayed_work_sync(&ts->work_f54);
#endif
#ifdef I2C_SUSPEND_WORKAROUND
	cancel_delayed_work_sync(&ts->check_suspended_work);
#endif
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	release_all_ts_event(ts);

	if (saved_state == POWER_ON || saved_state == POWER_WAKE) {
		if (!strncmp(string, "soft", 4)) {
			if(touch_device_func->ic_ctrl)
				touch_device_func->ic_ctrl(ts->client, IC_CTRL_RESET_CMD, 0);
			else
				TOUCH_INFO_MSG("There is no specific IC control function\n");
		} else if (!strncmp(string, "pin", 3)) {
			if (ts->pdata->reset_pin > 0) {
				gpio_set_value(ts->pdata->reset_pin, 0);
				msleep(ts->pdata->role->reset_delay);
				gpio_set_value(ts->pdata->reset_pin, 1);
			} else
				TOUCH_INFO_MSG("There is no reset pin\n");
		} else if (!strncmp(string, "vdd", 3)) {
			touch_power_cntl(ts, POWER_OFF);
			touch_power_cntl(ts, POWER_ON);
#ifdef CUST_G2_TOUCH
		} else if (!strncmp(string, "off", 3)) {
			saved_state = POWER_OFF;
			touch_power_cntl(ts, POWER_OFF);
#endif
		} else {
			TOUCH_INFO_MSG("Usage: echo [soft | pin | vdd] > ts_reset\n");
			TOUCH_INFO_MSG(" - soft : reset using IC register setting\n");
			TOUCH_INFO_MSG(" - soft : reset using reset pin\n");
			TOUCH_INFO_MSG(" - hard : reset using VDD\n");
		}

		if (ret < 0) {
			TOUCH_ERR_MSG("reset fail\n");
		}else {
			atomic_set(&ts->device_init, 0);
		}

		msleep(ts->pdata->role->booting_delay);

	} else
		TOUCH_INFO_MSG("Touch is suspend state. Don't need reset\n");

	if (ts->pdata->role->operation_mode)
		mutex_unlock(&ts->irq_work_mutex);
	else
		hrtimer_start(&ts->timer,
				ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	if (saved_state == POWER_ON || saved_state == POWER_WAKE)
		touch_ic_init(ts);

	return count;
}

/* ic_register_ctrl
 *
 * User can see any register of touch_IC
 */
static ssize_t ic_register_ctrl(struct lge_touch_data *ts, const char *buf, size_t count)
{
	unsigned char string[6];
	int reg = 0;
	int value = 0;
	int ret = 0;
	u32 write_data;
#ifdef CUST_G2_TOUCH
	int i = 0;
	int offset;
	unsigned char *r_mem = NULL;
#endif
	int page = 0;
#ifdef CUST_G2_TOUCH
	sscanf(buf, "%s %d %d %d %d", string, &page, &reg, &value, &offset);
#else
	sscanf(buf, "%s %d %d %d", string, &page, &reg, &value);
#endif

	if(touch_device_func->ic_ctrl) {
		if(ts->curr_pwr_state == POWER_ON || ts->curr_pwr_state == POWER_WAKE){
			if (unlikely(touch_i2c_write_byte(ts->client, 0xFF, page) < 0)) {
				TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
				return -EIO;
			}

			if (!strncmp(string, "read", 4)) {
				do {
					ret = touch_device_func->ic_ctrl(ts->client, IC_CTRL_READ, reg);
					if(ret >= 0) {
						TOUCH_INFO_MSG("register[0x%x] = 0x%x\n", reg, ret);
					} else {
						TOUCH_INFO_MSG("cannot read register[0x%x]\n", reg);
					}
					reg++;
				} while (--value > 0);
#ifdef CUST_G2_TOUCH
			} else if(!strncmp(string, "rall", 4)) {
				r_mem = kzalloc(sizeof(char) * value, GFP_KERNEL);
				if (r_mem == NULL) {
					TOUCH_ERR_MSG("%d bytes allocation fail!", value);
				} else {
					if (touch_i2c_read(ts->client, reg, value, r_mem) < 0) {
						TOUCH_ERR_MSG("%d bytes read fail!", value);
					} else {
						do {
							TOUCH_INFO_MSG("register[0x%x+%d] = 0x%x\n", reg, i, *(r_mem+i));
							i++;
						} while (--value > 0);
					}
					if(r_mem != NULL) kfree(r_mem);
				}
#endif
			} else if (!strncmp(string, "write", 4)) {
				write_data = ((0xFF & reg) << 8) | (0xFF & value);
				ret = touch_device_func->ic_ctrl(ts->client, IC_CTRL_WRITE, write_data);
				if(ret >= 0) {
					TOUCH_INFO_MSG("register[0x%x] is set to 0x%x\n", reg, value);
				} else {
					TOUCH_INFO_MSG("cannot write register[0x%x]\n", reg);
				}
			} else if (!strncmp(string, "wall", 4)) {
				r_mem = kzalloc(sizeof(char) * (offset+1), GFP_KERNEL);

				if (r_mem == NULL) {
					TOUCH_ERR_MSG("%d bytes allocation fail!", (offset+1));
				} else {
					if (touch_i2c_read(ts->client, reg, (offset+1), r_mem) < 0) {
						TOUCH_ERR_MSG("%d bytes read fail!", (offset+1));
					} else {
						*(r_mem+offset) = value;
						ret = touch_i2c_write(ts->client, reg, (offset+1), r_mem);
						if(ret >= 0) {
							do {
								TOUCH_INFO_MSG("register[0x%x+%d] is set to 0x%x\n", reg, i, *(r_mem+i));
								i++;
							} while (--offset >= 0);
						} else {
							TOUCH_INFO_MSG("cannot write register[0x%x] ~ [0x%x+%d]\n", reg, reg, offset);
						}
						if(r_mem != NULL) kfree(r_mem);
					}
				}
			} else{
#ifdef CUST_G2_TOUCH
				TOUCH_INFO_MSG("Usage: echo [read | rall | write | wall] reg_num value offset > ic_rw\n");
#else
				TOUCH_INFO_MSG("Usage: echo [read | write] reg_num value > ic_rw\n");
#endif
				TOUCH_INFO_MSG(" - reg_num : register address\n");
				TOUCH_INFO_MSG(" - value [read] : number of register starting form reg_num\n");
#ifdef CUST_G2_TOUCH
				TOUCH_INFO_MSG(" - value [rall] : number of register starting form reg_num\n");
#endif
				TOUCH_INFO_MSG(" - value [write] : set value into reg_num\n");
			}
			if (unlikely(touch_i2c_write_byte(ts->client, 0xFF, 0x00) < 0)) {
				TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
				return -EIO;
			}
		}
		else
			TOUCH_INFO_MSG("state=[suspend]. we cannot use I2C, now\n");
	} else
		TOUCH_INFO_MSG("There is no specific IC control function\n");

	return count;
}

/* store_keyguard_info
 *
 * This function is related with Keyguard in framework.
 * We can prevent the ghost-finger problem, using this function.
 * If you need more information, see the 'ghost_finger_solution' function.
 */
static ssize_t store_keyguard_info(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU) && defined(CONFIG_MACH_MSM8974_G2_KR)) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
	if(value == KEYGUARD_ENABLE) {
		ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2 | GHOST_STAGE_4;
		keyguard_status = ts->gf_ctrl.stage;
	}
	else if(value == KEYGUARD_RESERVED) {
		ts->gf_ctrl.stage &= ~GHOST_STAGE_2;
		keyguard_status = ts->gf_ctrl.stage;

		if(ime_drumming_status) {
			queue_delayed_work(touch_wq, &ts->work_ime_drumming, msecs_to_jiffies(10));
		}
	}
#else
	if(value == KEYGUARD_ENABLE)
		ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2 | GHOST_STAGE_4;
	else if(value == KEYGUARD_RESERVED)
		ts->gf_ctrl.stage &= ~GHOST_STAGE_2;
#endif
	if (touch_debug_mask & DEBUG_GHOST || touch_debug_mask & DEBUG_BASE_INFO){
		TOUCH_INFO_MSG("ghost_stage [0x%x]\n", ts->gf_ctrl.stage);
		if(value == KEYGUARD_RESERVED)
			TOUCH_INFO_MSG("ghost_stage2 : cleared[0x%x]\n", ts->gf_ctrl.stage);
	}

	return count;
}

/* show_virtual_key
 *
 * /sys/devices/virtual/input/virtualkeys
 * for use the virtual_key supported by Google
 *
 * 0x01:key_code:center_x:center_y:x_width:y_width
 * 0x01 = start_point
 */
static ssize_t show_virtual_key(struct lge_touch_data *ts, char *buf)
{
	int i=0;
	int ret = 0;

	u32 center_x = (ts->pdata->caps->x_max / (ts->pdata->caps->number_of_button * 2));
	u32 center_y = (ts->pdata->caps->y_button_boundary + (ts->st_info.b_height / 2));

	/* Register sysfs for virtualkeys*/
	if (ts->pdata->caps->button_support && ts->pdata->role->key_type == VIRTUAL_KEY) {

		for(i=0; i<ts->pdata->caps->number_of_button; i++) {
			if (i)
				ret += sprintf(buf+ret, ":");

			ret += sprintf(buf+ret, "0x01:%d:%d:%d:%d:%d", ts->pdata->caps->button_name[i],
						center_x * (i*2 + 1), center_y,
						ts->st_info.b_inner_width, ts->st_info.b_height);
		}
		ret += sprintf(buf+ret, "\n");
		return ret;
	} else
		return -ENOSYS;

}

static ssize_t store_jitter_solution(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	memset(&ts->jitter_filter, 0, sizeof(ts->jitter_filter));

	ret = sscanf(buf, "%d %d",
				&ts->pdata->role->jitter_filter_enable,
				&ts->jitter_filter.adjust_margin);

	return count;
}

static ssize_t store_accuracy_solution(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	memset(&ts->accuracy_filter, 0, sizeof(ts->accuracy_filter));

	ret = sscanf(buf, "%d %d %d %d %d %d %d %d",
				&ts->pdata->role->accuracy_filter_enable,
				&ts->accuracy_filter.ignore_pressure_gap,
				&ts->accuracy_filter.delta_max,
				&ts->accuracy_filter.touch_max_count,
				&ts->accuracy_filter.max_pressure,
				&ts->accuracy_filter.direction_count,
				&ts->accuracy_filter.time_to_max_pressure,
				&ts->accuracy_filter.pen_pressure);

	return count;
}


/* store_incoming_call
 *
 * This function is related with Keyguard in framework.
 * We can prevent the ghost-finger problem, using this function.
 * If you need more information, see the 'ghost_finger_solution' function.
 */
static ssize_t store_incoming_call(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	if(value == INCOMIMG_CALL_TOUCH)
		ts->gf_ctrl.incoming_call = 1;
	else if(value == INCOMIMG_CALL_RESERVED)
		ts->gf_ctrl.incoming_call = 0;

	if (touch_debug_mask & DEBUG_GHOST){
		TOUCH_INFO_MSG("incoming_call = %x\n", ts->gf_ctrl.incoming_call);
		if(value == INCOMIMG_CALL_RESERVED)
			TOUCH_INFO_MSG("incoming_call : cleared[%x]\n", ts->gf_ctrl.incoming_call);
	}

	return count;
}

/* show_f54
 *
 * Synaptics F54 function
*/
static ssize_t show_f54(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	if ((ts->curr_pwr_state == POWER_ON || ts->curr_pwr_state == POWER_WAKE) && !touch_test_dev->fw_info.fw_upgrade.is_downloading) {
		SCAN_PDT();

		ret = sprintf(buf, "====== F54 Function Info ======\n");

#ifdef CUST_G2_TOUCH
		f54_window_crack = 0;
		if(!ts_suspend)
#endif
		{
			if (!ts->pdata->role->operation_mode)
				hrtimer_cancel(&ts->timer);
		}
		mutex_lock(&ts->irq_work_mutex);
		ret += sprintf(buf+ret, "F54_FullRawCap Test Result: %s", (F54Test('a') > 0) ? "Pass\n" : "Fail\n" );
		if(f54_show_raw_data) ret += sprintf(buf+ret, "%s\n", f54_wlog_buf);

#ifdef CUST_G2_TOUCH
		if(!f54_window_crack_check_mode) {
#endif

			ret += sprintf(buf+ret, "TRex Short Test Result: %s", (F54Test('f') > 0) ? "Pass\n" : "Fail\n" );
			if(f54_show_raw_data) ret += sprintf(buf+ret, "%s\n", f54_wlog_buf);

			ret += sprintf(buf+ret, "TRex Gnd Test Result: %s", (F54Test('e') > 0) ? "Pass\n" : "Fail\n" );
			if(f54_show_raw_data) ret += sprintf(buf+ret, "%s\n", f54_wlog_buf);

			//High Resistance always return fail, you should see raw data.
			ret += sprintf(buf+ret, "High Resistance Test Result: %s", (F54Test('g') > 0) ? "Pass\n" : "Fail\n" );
			if(f54_show_raw_data) ret += sprintf(buf+ret, "%s\n", f54_wlog_buf);
#ifdef CUST_G2_TOUCH
		}
#endif
		if(ts_suspend)

			touch_power_cntl(ts, POWER_OFF);
		else
		{
			if (!ts->pdata->role->operation_mode)
				hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);
			msleep(ts->pdata->role->booting_delay);
			touch_ic_init(ts);
		}
		mutex_unlock(&ts->irq_work_mutex);
	} else {
		ret = sprintf(buf+ret, "state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}

/* store_f54
 *
 * Synaptics F54 function
*/
static ssize_t store_f54(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d", &f54_show_raw_data);

	return count;
}

static ssize_t store_ghost_detection_enable(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;
	int ghost_detection_enable = 0;

	ret = sscanf(buf, "%d",
				&ghost_detection_enable
				);

	ts->pdata->role->ghost_detection_enable = ghost_detection_enable;
	return count;
}

#ifdef CUST_G2_TOUCH
static ssize_t show_f54_window_crack(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d\n", f54_window_crack);
	return ret;
}
#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI)) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
static ssize_t show_ime_drumming_status(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

#if !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU) && defined(CONFIG_MACH_MSM8974_G2_KR)
	u8 udata[5] = {0};

	if (unlikely(touch_i2c_read(ts->client, DRUMMING_THRESH_N_DISTANCE_REG, 5, &udata[0]) < 0)) {
		ret += sprintf(buf+ret, "drumming REG read fail\n");
	}

	ret += sprintf(buf+ret, "%s\n", (ime_drumming_status == IME_ON) ? "IME_ON" : "IME_OFF");
	ret += sprintf(buf+ret, "Noise Floor                     = %2x\n", udata[0]);
	ret += sprintf(buf+ret, "Minimum Peak Amplitude          = %2x\n", udata[1]);
	ret += sprintf(buf+ret, "Peak Merge Threshold            = %2x\n", udata[2]);
	ret += sprintf(buf+ret, "Drumming Acceleration Threshold = %2x\n", udata[3]);
	ret += sprintf(buf+ret, "Minimum Drumming Separation     = %2x\n", udata[4]);
#endif
#ifdef CONFIG_LGE_Z_TOUCHSCREEN
	u8 small_finger_amp_th[2] = {0};

	if (unlikely(touch_i2c_read(ts->client, SMALL_FINGER_AMPLITUDE_THRESHOLD_REG, 2, &small_finger_amp_th[0]) < 0)) {
		ret += sprintf(buf+ret, "SMALL_FINGER_AMPLITUDE_THRESHOLD_REG read fail\n");
	}

	ret += sprintf(buf+ret, "%s\n", (ime_drumming_status == IME_ON) ? "IME_ON" : "IME_OFF");
	ret += sprintf(buf+ret, "Finger Amplitude Threshold  = %2x\n", small_finger_amp_th[0]);
	ret += sprintf(buf+ret, "Small Finger Amplitude Threshold  = %2x\n", small_finger_amp_th[1]);
#endif
	return ret;
}

static ssize_t store_ime_drumming_status(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);
#if !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU) && defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
	if( (value == IME_ON) && (ime_drumming_status == 0) ) {
		ime_drumming_status = 1;
		queue_delayed_work(touch_wq, &touch_test_dev->work_ime_drumming, msecs_to_jiffies(10));
	}
	else if( (value == IME_OFF) && (ime_drumming_status == 1) ) {
		ime_drumming_status = 0;
		queue_delayed_work(touch_wq, &touch_test_dev->work_ime_drumming, msecs_to_jiffies(10));
	}
	else {
		return count;
	}

	TOUCH_INFO_MSG("ime status = %s\n", (ime_drumming_status == IME_ON) ? "IME_ON" : "IME_OFF");
#endif
	return count;
}
#endif
#endif

#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
static ssize_t store_quick_cover_status(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	if( (value == IME_ON) && (quick_cover_status == 0) ) {
		quick_cover_status = 1;
	}
	else if( (value == IME_OFF) && (quick_cover_status == 1) ) {
		quick_cover_status = 0;
	}
	else {
		return count;
	}

	TOUCH_INFO_MSG("quick cover status = %s\n", (quick_cover_status == IME_ON) ? "QUICK_COVER_ON" : "QUICK_COVER_OFF");
	return count;
}
#endif


static ssize_t show_mfts(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	int count = 0;
	u8 product_id[11]={0};
	u8 fw_version[5]={0};
	unsigned char address;
	unsigned char buffer[6] = {0};
	unsigned short SynaF01QueryBase = 0;

	mutex_lock(&ts->irq_work_mutex);
	do {
		if(touch_i2c_write_byte(ts->client, 0xFF, 0x00) == 0)
			break;
		count++;
		msleep(100);
	}while(count < 10);

	if (unlikely(touch_i2c_write_byte(ts->client, 0xFF, 0x00) < 0)) {
		ret = sprintf(buf, "2");
		goto out;
	}

	for (address = 0xe9; address > 0xc0; address = address - 6)
	{
		touch_i2c_read(ts->client, address, 6, buffer);

		if (!buffer[5])
			continue;
		switch (buffer[5])
		{
			case 0x01:
				SynaF01QueryBase = buffer[0];
				break;
		}
	}

	if (unlikely(touch_i2c_read(ts->client, SynaF01QueryBase+11, sizeof(product_id) -1, product_id) < 0)) {
		ret = sprintf(buf, "2");
		goto out;
	}
	TOUCH_INFO_MSG("product_id = %s\n", product_id);

	if(!strncmp(product_id, "s3404", 5)) {
		ret = sprintf(buf, "1");
		goto out;
	} else {
		if (unlikely(touch_i2c_read(ts->client, 0x09, sizeof(fw_version) -1, fw_version) < 0)) {
			ret = sprintf(buf, "2");
			goto out;
		}
		TOUCH_INFO_MSG("fw_version = %s\n", fw_version);

		if(ts->fw_info.fw_setting.curr_touch_vendor != get_panel_maker_id())
			ret = sprintf(buf, "1");
		else if((int)simple_strtoul(&fw_version[1], NULL, 10) < (int)simple_strtoul(&ts->fw_info.syna_img_fw_version[1], NULL, 10))
			ret = sprintf(buf, "1");
		else
			ret = sprintf(buf, "0");
	}

out:
	mutex_unlock(&ts->irq_work_mutex);
	return ret;
}

static ssize_t show_mfts_fw(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	printk(KERN_INFO "\n");
	TOUCH_INFO_MSG("Firmware image path: Internal\n");

	ts->fw_info.fw_setting.prev_touch_vendor = ts->fw_info.fw_setting.curr_touch_vendor;
	ts->fw_info.fw_setting.curr_touch_vendor = get_panel_maker_id();
	TOUCH_INFO_MSG("lcd_maker_id = %d, func: %s\n", ts->fw_info.fw_setting.curr_touch_vendor, __func__);

	/* sync for n-th repeat test */
	while(ts->fw_info.fw_upgrade.is_downloading);

	msleep(ts->pdata->role->booting_delay * 2);
	printk(KERN_INFO "\n");

	/* set downloading flag for sync for n-th test */
	ts->fw_info.fw_upgrade.is_downloading = UNDER_DOWNLOADING;
	ts->fw_info.fw_upgrade.fw_force_upgrade = 1;

	queue_work(touch_wq, &ts->work_fw_upgrade);

	/* sync for fw_upgrade test */
	while(ts->fw_info.fw_upgrade.is_downloading);

	touch_power_cntl(ts, POWER_ON);
	msleep(ts->pdata->role->booting_delay);
	enable_irq(ts->client->irq);
	TOUCH_INFO_MSG("%s : enable_irq !!\n", __func__);
	touch_ic_init(ts);

	return ret;
}

static ssize_t show_mfts_fw_ver(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	int count = 0;
	u8 product_id[11]={0};
	u8 fw_version[5]={0};
	unsigned char address;
	unsigned char buffer[6] = {0};
	unsigned short SynaF01QueryBase = 0;

	mutex_lock(&ts->irq_work_mutex);
	do {
		if(touch_i2c_write_byte(ts->client, 0xFF, 0x00) == 0)
			break;
		count++;
		msleep(100);
	}while(count < 10);

	if (unlikely(touch_i2c_write_byte(ts->client, 0xFF, 0x00) < 0)) {
		ret += sprintf(buf+ret, "page select fail\n");
		goto out;
	}

	for (address = 0xe9; address > 0xc0; address = address - 6)
	{
		touch_i2c_read(ts->client, address, 6, buffer);

		if (!buffer[5])
			continue;
		switch (buffer[5])
		{
			case 0x01:
				SynaF01QueryBase = buffer[0];
				break;
		}
	}

	if (unlikely(touch_i2c_read(ts->client, SynaF01QueryBase+11, sizeof(product_id) -1, product_id) < 0)) {
		ret += sprintf(buf+ret, "read product id fail\n");
		goto out;
	}

	if(!strncmp(product_id, "s3404", 5)) {
		ret += sprintf(buf+ret, "null");
		goto out;
	} else {
		if (unlikely(touch_i2c_read(ts->client, 0x09, sizeof(fw_version) -1, fw_version) < 0)) {
			ret += sprintf(buf+ret, "read firmware version fail\n");
			goto out;
		}
		ret += sprintf(buf+ret, "%s", fw_version);
	}
out:
	mutex_unlock(&ts->irq_work_mutex);
	return ret;
}


static ssize_t show_ts_noise(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	struct timeval t_start, t_cur;

	u8 udata[12] = {0};
	unsigned long cns_sum=0, im_sum=0, cid_im_sum=0, feq_scan_im_sum=0, image_sum=0;
	unsigned long baseline_sum=0, negative_sum=0, positive_sum=0, finger_sum=0, difference_sum=0;
	unsigned int cnt = 0;

	if (ts->curr_pwr_state == POWER_ON || ts->curr_pwr_state == POWER_WAKE) {

		if (!ts->pdata->role->operation_mode)
			hrtimer_cancel(&ts->timer);
		mutex_lock(&ts->irq_work_mutex);

		if (unlikely(touch_i2c_write_byte(ts->client, 0xFF, 0x01) < 0)) {
			ret = sprintf(buf+ret, "PAGE_SELECT_REG write fail\n");
			mutex_unlock(&ts->irq_work_mutex);
			return ret;
		}

		do_gettimeofday(&t_start);

		while(1) {
			if (unlikely(touch_i2c_read(ts->client, 0x08, 1, &udata[0]) < 0)) {
				ret += sprintf(buf+ret, "Current Noise State REG read fail\n");
			}
			cns_sum += udata[0];

			if (unlikely(touch_i2c_read(ts->client, 0x04, 1, &udata[0]) < 0)) {
				ret += sprintf(buf+ret, "Interference Metric REG read fail\n");
			}
			if (unlikely(touch_i2c_read(ts->client, 0x05, 1, &udata[1]) < 0)) {
				ret += sprintf(buf+ret, "Interference Metric REG read fail\n");
			}
			im_sum += (udata[1]<<8)|udata[0];

			if (unlikely(touch_i2c_read(ts->client, 0x09, 2, &udata[0]) < 0)) {
				ret += sprintf(buf+ret, "CID IM REG read fail\n");
			}
			cid_im_sum += (udata[1]<<8)|udata[0];

			if (unlikely(touch_i2c_read(ts->client, 0x0a, 2, &udata[0]) < 0)) {
				ret += sprintf(buf+ret, "Freq Scan IM REG read fail\n");
			}
			feq_scan_im_sum += (udata[1]<<8)|udata[0];

			if (unlikely(touch_i2c_read(ts->client, 0x0d, 12, &udata[0]) < 0)) {
				ret += sprintf(buf+ret, "Image Metric REG read fail\n");
			}
			image_sum		+= (udata[1]<<8)|udata[0];
			baseline_sum	+= (udata[3]<<8)|udata[2];
			negative_sum	+= (udata[5]<<8)|udata[4];
			positive_sum	+= (udata[7]<<8)|udata[6];
			finger_sum		+= (udata[9]<<8)|udata[8];
			difference_sum	+= (udata[11]<<8)|udata[10];

			do_gettimeofday(&t_cur);

			cnt++;

			if(t_cur.tv_sec - t_start.tv_sec > 3) break;
		}

		ret += sprintf(buf+ret, "Test Count : %u\n", cnt);
		ret += sprintf(buf+ret, "Current Noise State : %lu\n", cns_sum/cnt);
		ret += sprintf(buf+ret, "Interference Metric : %lu\n", im_sum/cnt);
		ret += sprintf(buf+ret, "CID IM : %lu\n", cid_im_sum/cnt);
		ret += sprintf(buf+ret, "Freq Scan IM : %lu\n", feq_scan_im_sum/cnt);
		ret += sprintf(buf+ret, "Image Metric : %lu\n", image_sum/cnt);
		ret += sprintf(buf+ret, "Baseline Metric : %lu\n", baseline_sum/cnt);
		ret += sprintf(buf+ret, "Negative Energy : %lu\n", negative_sum/cnt);
		ret += sprintf(buf+ret, "Positive Energy : %lu\n", positive_sum/cnt);
		ret += sprintf(buf+ret, "Finger Metric : %lu\n", finger_sum/cnt);
		ret += sprintf(buf+ret, "Difference : %lu\n", difference_sum/cnt);

		if (unlikely(touch_i2c_write_byte(ts->client, 0xFF, 0x00) < 0)) {
			ret += sprintf(buf+ret, "PAGE_SELECT_REG write fail\n");
			return ret;
		}

		mutex_unlock(&ts->irq_work_mutex);
		if(!ts->pdata->role->operation_mode)
			hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);
	}else {
		ret = sprintf(buf+ret, "state=[suspend]. we cannot use I2C, now. Test Result: Fail\n");
	}

	return ret;
}

#if defined(Z_GLOVE_TOUCH_SUPPORT)
static int set_glove_finger_enable(struct lge_touch_data *ts, int onoff)
{
	int ret = 0;

	if(touch_device_func->ic_ctrl) {
		if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_GLOVE_TOUCH, onoff) < 0){
			TOUCH_ERR_MSG("IC_CTRL_GLOVE_TOUCH handling fail\n");
			return -1;
		}
	}

	return ret;
}

static ssize_t show_glove_finger_enable(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%d\n", glove_finger_enable);
	return ret;
}

static ssize_t store_glove_finger_enable(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d", &glove_finger_enable);
	if (set_glove_finger_enable(ts, glove_finger_enable) < 0) {
		TOUCH_ERR_MSG("set_glove_finger_enable error (%s)\n", __func__);
	}

	return count;
}
#endif

#ifndef CONFIG_LGE_SECURITY_KNOCK_ON
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
static ssize_t store_touch_gesture(struct lge_touch_data *ts, const char *buf, size_t count)
{
#if defined(CONFIG_LGE_Z_TOUCHSCREEN)
	/* temporary until implement N-pjt */
	int value;
	int type = 0;
	int temp_value[4] = {0};

	sscanf(buf, "%d %d %d %d %d", &type, &value, &temp_value[1], &temp_value[2], &temp_value[3]);
	TOUCH_INFO_MSG("store_touch_gesture temp %d %d %d %d %d\n\n", type, value, temp_value[1], temp_value[2], temp_value[3]);
	if(type!=1){
		return count;
	}
#else
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	int value;
	int type = 0;
	int temp_value[4] = {0};

	sscanf(buf, "%d %d %d %d %d", &type, &value, &temp_value[1], &temp_value[2], &temp_value[3]);
	TOUCH_INFO_MSG("store_touch_gesture temp %d %d %d %d %d\n\n", type, value, temp_value[1], temp_value[2], temp_value[3]);
#else
	int value;
	sscanf(buf, "%d", &value);
#endif
#endif

	cancel_delayed_work_sync(&ts->work_f54);

	if (is_zw_mode())
		value = 0;
#if !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_A){
		touch_gesture_enable = 0;
		return count;
	}
#endif
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	if( (int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) < 4){
		touch_gesture_enable = 0;
		return count;
	}
#endif

	mutex_lock(&ts->irq_work_mutex);

#if defined(A1_only)
	if(ts_suspend && (get_smartcover_status() == 1) && (value==touch_gesture_enable)){
		if(touch_device_func->ic_ctrl) {
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_DOUBLE_TAP_WAKEUP_MODE, 1) < 0){
				TOUCH_ERR_MSG("IC_CTRL_DOUBLE_TAP_WAKEUP_MODE handling fail\n");
			}
		}
	}
#endif

	if (value == touch_gesture_enable || !ts_suspend)
		goto no_action;

	if(value == 1) {
		touch_double_tap_wakeup_enable(ts);
	}
	else {
		touch_disable_irq_wake(ts);
		if(ts->curr_pwr_state) {
			disable_irq(ts->client->irq);
			TOUCH_INFO_MSG("%s : disable_irq !!\n", __func__);
			touch_power_cntl(ts, POWER_OFF);
		}
	}
no_action:
	touch_gesture_enable = value;
	if (touch_debug_mask & DEBUG_BASE_INFO)
		TOUCH_INFO_MSG("touch gesture = %x\n", touch_gesture_enable);
	mutex_unlock(&ts->irq_work_mutex);
	return count;

}
#endif
#endif

#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
/* send_uevent
 *
 * It will be used to send u-event to Android-framework.
 */
static struct sys_device lge_touch_sys_device;
void send_uevent(char* string[2])
{
    kobject_uevent_env(&lge_touch_sys_device.kobj, KOBJ_CHANGE, string);
    TOUCH_DEBUG_MSG( "uevent[%s]\n", string[0]);
}

/* send_uevent_lpwg
 *
 * It uses wake-lock in order to prevent entering the sleep-state,
 * during recognition or verification.
 */
#define VALID_LPWG_UEVENT_SIZE 2
static char *lpwg_uevent[VALID_LPWG_UEVENT_SIZE][2] =
{{"TOUCH_GESTURE_WAKEUP=WAKEUP", NULL},
 {"TOUCH_GESTURE_WAKEUP=PASSWORD", NULL}};

void send_uevent_lpwg(int type)
{
        send_uevent(lpwg_uevent[type-1]);
}
EXPORT_SYMBOL(send_uevent_lpwg);
/* update_status
 *
 * Other drivers can notify their status to touch driver.
 * Do not use 'i2c_client' in other function.
 */
struct state_info*  state;
struct i2c_client*  client_only_for_update_status;
void update_status(int code, int value)
{

    if (code == NOTIFY_TA_CONNECTION)
        atomic_set(&state->ta_state, value ? TA_CONNECTED : TA_DISCONNECTED);
    else if (code == NOTIFY_TEMPERATURE_CHANGE)
        atomic_set(&state->temperature_state, value);
    else if (code == NOTIFY_PROXIMITY)
        atomic_set(&state->proximity_state, value ? PROXIMITY_NEAR : PROXIMITY_FAR);
    else if (code == NOTIFY_HALL_IC)
        atomic_set(&state->hallic_state, value ? HALL_COVERED : HALL_NONE);


    TOUCH_DEBUG_MSG("code[%d] value[%d]\n", code, value);
}
EXPORT_SYMBOL(update_status);



/* Sysfs - lpwg_data (Low Power Wake-up Gesture)
 *
 * read : "x1 y1\n x2 y2\n ..."
 * write
 * 1 : ENABLE/DISABLE
 * 2 : LCD SIZE
 * 3 : ACTIVE AREA
 * 4 : TAP COUNT
 */
static struct point lpwg_data[MAX_POINT_SIZE_FOR_LPWG+1];
static ssize_t show_lpwg_data(struct lge_touch_data *ts, char *buf)
{
    int i = 0, ret = 0;
	TOUCH_INFO_MSG("[%s]\n", __func__);
    if (touch_device_func->lpwg) {
        memset(lpwg_data, 0, sizeof(struct point)*MAX_POINT_SIZE_FOR_LPWG);
		TOUCH_DEBUG_MSG("\n");
        touch_device_func->lpwg(ts->client, LPWG_READ, 0, lpwg_data);
        for (i = 0; i < MAX_POINT_SIZE_FOR_LPWG; i++) {
            if (lpwg_data[i].x == -1 && lpwg_data[i].y == -1)
                break;
            ret += sprintf(buf+ret, "%d %d\n", lpwg_data[i].x, lpwg_data[i].y);
        }
    }
#if defined(KNOCKON_MASK)
	TOUCH_INFO_MSG("[%s] buf = XXX\n", __func__);
#else
	TOUCH_INFO_MSG("[%s] buf = %s\n", __func__, buf);
#endif
    return ret;
}

static ssize_t store_lpwg_data(struct lge_touch_data *ts, const char *buf, size_t count)
{
    int reply = 0;

    sscanf(buf, "%d", &reply);
	TOUCH_INFO_MSG("[%s] reply=%d\n", __func__, reply);
    if (touch_device_func->lpwg) {
        touch_device_func->lpwg(ts->client, LPWG_REPLY, reply, NULL);
    }

    atomic_set(&ts->state.uevent_state, UEVENT_IDLE);
    //wake_unlock(&ts->lpwg_wake_lock);

    return count;
}

static void set_wakeup_mode(struct lge_touch_data *ts, int value)
{
	TOUCH_INFO_MSG("[%s] value = %d, touch_gesture_enable=%d\n", __func__, value, touch_gesture_enable);

	cancel_delayed_work_sync(&ts->work_f54);

	mutex_lock(&ts->irq_work_mutex);

	if (value == touch_gesture_enable || !ts_suspend){
		TOUCH_INFO_MSG("[%s] ts_suspend = %d\n", __func__, ts_suspend);
		goto no_action;
	}
	TOUCH_INFO_MSG("[%s] value = %d\n", __func__, value);
	switch (value) {
		case LPWG_DOUBLE_TAP:
		case LPWG_PASSWORD:
		        touch_multi_tap_wakeup_enable(ts, value);
//		        ts->password_enable = 1;
			break;
		default:
			touch_disable_irq_wake(ts);
			if(ts->curr_pwr_state) {
				disable_irq(ts->client->irq);
				TOUCH_INFO_MSG("%s : disable_irq !!\n", __func__);
				touch_power_cntl(ts, POWER_OFF);
			}
			break;
	}

no_action:
	touch_gesture_enable = value;
	if (touch_debug_mask & DEBUG_BASE_INFO)
		TOUCH_INFO_MSG("touch gesture = %x\n", touch_gesture_enable);
	mutex_unlock(&ts->irq_work_mutex);
	return;

}

/* Sysfs - lpwg_notify (Low Power Wake-up Gesture)
 *
 */
#if defined(CONFIG_FB)
static int touch_lcd_suspend(struct device *device);
static int touch_lcd_resume(struct device *device);
#endif
static ssize_t store_lpwg_notify(struct lge_touch_data *ts, const char *buf, size_t count)
{
//    static int suspend = 0;
	int type = 0;
	int value[4] = {0};

	sscanf(buf, "%d %d %d %d %d", &type, &value[0], &value[1], &value[2], &value[3]);
	TOUCH_INFO_MSG("[%s] type = %d, value = %d %d %d %d\n", __func__, type, value[0], value[1], value[2], value[3]);

#ifdef CONFIG_LGE_Z_TOUCHSCREEN
#ifndef CONFIG_MACH_MSM8974_Z_CTC_CN
	if(lge_get_board_revno() < HW_REV_B){
		TOUCH_INFO_MSG("[store_lpwg_notify] HW_REV : %d, Knock skipped ", lge_get_board_revno());
		return count;
	}
#endif
#endif

    if (touch_device_func->lpwg) {
        switch(type){
        case 1 :
			set_wakeup_mode(ts, value[0]);
//            touch_device_func->lpwg(ts->client, LPWG_ENABLE, value[0], NULL);
//			mutex_lock(&ts->irq_work_mutex);
//			if(value[0]) touch_gesture_enable = 1;
//			else touch_gesture_enable = 0;
//			mutex_unlock(&ts->irq_work_mutex);
//			if (suspend)
//				touch_lcd_suspend(&ts->client->dev);
            break;
        case 2 :
			touch_device_func->lpwg(ts->client, LPWG_LCD_X, value[0], NULL);
			touch_device_func->lpwg(ts->client, LPWG_LCD_Y, value[1], NULL);
			break;
        case 3 :
			touch_device_func->lpwg(ts->client, LPWG_ACTIVE_AREA_X1, value[0], NULL);
			touch_device_func->lpwg(ts->client, LPWG_ACTIVE_AREA_X2, value[1], NULL);
			touch_device_func->lpwg(ts->client, LPWG_ACTIVE_AREA_Y1, value[2], NULL);
			touch_device_func->lpwg(ts->client, LPWG_ACTIVE_AREA_Y2, value[3], NULL);
			break;
        case 4 :
			mutex_lock(&ts->irq_work_mutex);
			touch_device_func->lpwg(ts->client, LPWG_TAP_COUNT, value[0], NULL);
			mutex_unlock(&ts->irq_work_mutex);
			break;
        case 6 :
#if 0 //#if defined(CONFIG_FB)
			if(value[0] == 0) {
				suspend = 1;
				if (touch_gesture_enable)
					touch_lcd_suspend(&ts->client->dev);
			} else if(value[0] == 1) {
 				suspend = 0;
				touch_lcd_resume(&ts->client->dev);
			}
#endif
            break;
        default:
            break;
        }
    }
    return count;
}
#endif

static LGE_TOUCH_ATTR(platform_data, S_IRUGO | S_IWUSR, show_platform_data, NULL);
static LGE_TOUCH_ATTR(firmware, S_IRUGO | S_IWUSR, show_fw_info, store_fw_upgrade);
static LGE_TOUCH_ATTR(fw_ver, S_IRUGO | S_IWUSR, show_fw_ver, NULL);
static LGE_TOUCH_ATTR(section, S_IRUGO | S_IWUSR, show_section_info, NULL);
static LGE_TOUCH_ATTR(reset, S_IRUGO | S_IWUSR, NULL, store_ts_reset);
static LGE_TOUCH_ATTR(ic_rw, S_IRUGO | S_IWUSR, NULL, ic_register_ctrl);
static LGE_TOUCH_ATTR(keyguard, S_IRUGO | S_IWUSR, NULL, store_keyguard_info);
static LGE_TOUCH_ATTR(virtualkeys, S_IRUGO | S_IWUSR, show_virtual_key, NULL);
static LGE_TOUCH_ATTR(jitter, S_IRUGO | S_IWUSR, NULL, store_jitter_solution);
static LGE_TOUCH_ATTR(accuracy, S_IRUGO | S_IWUSR, NULL, store_accuracy_solution);
#ifdef CUST_G2_TOUCH
static LGE_TOUCH_ATTR(incoming_call, S_IRUGO | S_IWUSR, NULL, store_incoming_call);
static LGE_TOUCH_ATTR(f54, S_IRUGO | S_IWUSR, show_f54, store_f54);
static LGE_TOUCH_ATTR(ghost_detection_enable, S_IRUGO | S_IWUSR, NULL, store_ghost_detection_enable);
static LGE_TOUCH_ATTR(mfts, S_IRUGO | S_IWUSR, show_mfts, NULL);
static LGE_TOUCH_ATTR(mfts_fw, S_IRUGO | S_IWUSR, show_mfts_fw, NULL);
static LGE_TOUCH_ATTR(mfts_fw_ver, S_IRUGO | S_IWUSR, show_mfts_fw_ver, NULL);
static LGE_TOUCH_ATTR(ts_noise, S_IRUGO | S_IWUSR, show_ts_noise, NULL);
static LGE_TOUCH_ATTR(window_crack, S_IRUGO | S_IWUSR, show_f54_window_crack, NULL);
#ifdef CONFIG_LGE_Z_TOUCHSCREEN
static LGE_TOUCH_ATTR(quick_cover_status, S_IRUGO | S_IWUSR, NULL, store_quick_cover_status);
#endif
#endif
#if defined(Z_GLOVE_TOUCH_SUPPORT)
static LGE_TOUCH_ATTR(glove_finger_enable, S_IRUGO | S_IWUSR, show_glove_finger_enable, store_glove_finger_enable);
#endif
#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI)) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
static LGE_TOUCH_ATTR(ime_status, S_IRUGO | S_IWUSR, show_ime_drumming_status, store_ime_drumming_status);
#endif
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
#ifndef CONFIG_LGE_SECURITY_KNOCK_ON
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN) || defined(CONFIG_LGE_Z_TOUCHSCREEN) || defined(A1_only)
static LGE_TOUCH_ATTR(lpwg_notify, S_IRUGO | S_IWUSR, NULL, store_touch_gesture);
#else
static LGE_TOUCH_ATTR(touch_gesture, S_IRUGO | S_IWUSR, NULL, store_touch_gesture);
#endif
#endif
#endif
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
static LGE_TOUCH_ATTR(lpwg_data, S_IRUGO | S_IWUSR, show_lpwg_data, store_lpwg_data);
static LGE_TOUCH_ATTR(lpwg_notify, S_IRUGO | S_IWUSR, NULL, store_lpwg_notify);
#endif

static struct attribute *lge_touch_attribute_list[] = {
	&lge_touch_attr_platform_data.attr,
	&lge_touch_attr_firmware.attr,
	&lge_touch_attr_fw_ver.attr,
	&lge_touch_attr_section.attr,
	&lge_touch_attr_reset.attr,
	&lge_touch_attr_ic_rw.attr,
	&lge_touch_attr_keyguard.attr,
	&lge_touch_attr_virtualkeys.attr,
	&lge_touch_attr_jitter.attr,
	&lge_touch_attr_accuracy.attr,
#ifdef CUST_G2_TOUCH
	&lge_touch_attr_incoming_call.attr,
	&lge_touch_attr_f54.attr,
	&lge_touch_attr_ghost_detection_enable.attr,
	&lge_touch_attr_mfts.attr,
	&lge_touch_attr_mfts_fw.attr,
	&lge_touch_attr_mfts_fw_ver.attr,
	&lge_touch_attr_ts_noise.attr,
	&lge_touch_attr_window_crack.attr,
#ifdef CONFIG_LGE_Z_TOUCHSCREEN
	&lge_touch_attr_quick_cover_status.attr,
#endif
#endif
#if defined(Z_GLOVE_TOUCH_SUPPORT)
	&lge_touch_attr_glove_finger_enable.attr,
#endif
#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI)) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
	&lge_touch_attr_ime_status.attr,
#endif
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
#ifndef CONFIG_LGE_SECURITY_KNOCK_ON
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN) || defined(CONFIG_LGE_Z_TOUCHSCREEN) || defined(A1_only)
	&lge_touch_attr_lpwg_notify.attr,
#else
	&lge_touch_attr_touch_gesture.attr,
#endif
#endif
#endif
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
	&lge_touch_attr_lpwg_data.attr,
	&lge_touch_attr_lpwg_notify.attr,
#endif
	NULL,
};

/* lge_touch_attr_show / lge_touch_attr_store
 *
 * sysfs bindings for lge_touch
 */
static ssize_t lge_touch_attr_show(struct kobject *lge_touch_kobj, struct attribute *attr,
			     char *buf)
{
	struct lge_touch_data *ts =
			container_of(lge_touch_kobj, struct lge_touch_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->show)
		ret = lge_touch_priv->show(ts, buf);

	return ret;
}

static ssize_t lge_touch_attr_store(struct kobject *lge_touch_kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	struct lge_touch_data *ts =
			container_of(lge_touch_kobj, struct lge_touch_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->store)
		ret = lge_touch_priv->store(ts, buf, count);

	return ret;
}

static const struct sysfs_ops lge_touch_sysfs_ops = {
	.show	= lge_touch_attr_show,
	.store	= lge_touch_attr_store,
};

static struct kobj_type lge_touch_kobj_type = {
	.sysfs_ops		= &lge_touch_sysfs_ops,
	.default_attrs 	= lge_touch_attribute_list,
};

static struct sysdev_class lge_touch_sys_class = {
	.name = LGE_TOUCH_NAME,
};

static struct sys_device lge_touch_sys_device = {
	.id		= 0,
	.cls	= &lge_touch_sys_class,
};

int synaptics_power_on(struct i2c_client *client, int on)
{
	int rc = 0;
	static struct regulator *vreg_l22;
	static struct regulator *vreg_lvs3;

		/* 3.3V_TOUCH_VDD, VREG_L15: 2.75 ~ 3.3 */
		if (!vreg_l22) {
			vreg_l22 = regulator_get(&client->dev, "vdd_ana");
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
			vreg_lvs3 = regulator_get(&client->dev, "vcc_i2c");
			if (IS_ERR(vreg_lvs3)) {
				pr_err("%s: regulator get of pm8941_lvs3 failed (%ld)\n",
						__func__,
					   PTR_ERR(vreg_lvs3));
				rc = PTR_ERR(vreg_lvs3);
				vreg_lvs3 = NULL;
				return rc;
			}
		}

			rc = regulator_set_voltage(vreg_l22, 3300000, 3300000);

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

static int synaptics_parse_dt(struct device *dev, struct touch_platform_data *pdata)
{
	struct device_node *node = dev->of_node;
	struct device_node *pp;
	struct touch_device_caps *caps_info;
	struct touch_operation_role *role_info;
	struct touch_power_module *pwr_info;
	struct property *prop;
	int rc = 0;
	int len = 0;
	unsigned int i;
	const char *temp_string = node->name;
	const char *reg_string = node->name;
	u32 temp_val;

	/* reset, irq gpio info */
	if (node == NULL)
		return -ENODEV;

	pdata->reset_pin = of_get_named_gpio_flags(node, "synaptics,reset-gpio", 0, NULL);
	pdata->int_pin = of_get_named_gpio_flags(node, "synaptics,irq-gpio", 0, NULL);

	rc = of_property_read_string(node, "synaptics,maker", &temp_string);
	if (rc) {
		dev_err(dev, "Looking up %s property in node %s failed",
			"synaptics,maker",
			temp_string);
		return -ENODEV;
	}
	len = strlen(temp_string);
	memcpy(pdata->maker, temp_string, len);

	rc = of_property_read_string(node, "synaptics,fw_version", &temp_string);
	if (rc) {
		dev_err(dev, "Looking up %s property in node %s failed",
			"synaptics,fw_version",
			temp_string);
		return -ENODEV;
	}
	len = strlen(temp_string);

	memcpy(pdata->fw_version, temp_string, len);

	/* Caps */
	pdata->num_caps = 0;
	pp = NULL;
	while ((pp = of_get_next_child(node, pp)))
		pdata->num_caps++;

	if (!pdata->num_caps)
		return 0;

	caps_info = devm_kzalloc(dev, pdata->num_caps * sizeof(struct touch_device_caps), GFP_KERNEL);
	if (!caps_info)
		return -ENOMEM;

	pdata->caps = caps_info;
	for_each_child_of_node(node, pp) {
			rc = of_property_read_u32(pp, "button_support", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read button_support\n");
				return rc;
			} else
				caps_info->button_support =  temp_val;

			rc = of_property_read_u32(pp, "y_button_boundary", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read y_button_boundary\n");
				return rc;
			} else
				caps_info->y_button_boundary =	temp_val;

			rc = of_property_read_u32(pp, "button_margin", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read button_margin\n");
				return rc;
			} else
				caps_info->button_margin =	temp_val;

			rc = of_property_read_u32(pp, "number_of_button", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read number_of_button\n");
				return rc;
			} else
				caps_info->number_of_button =  temp_val;


			prop = of_find_property(pp, "button_name", NULL);
			if(prop && (prop->length == caps_info->number_of_button)){
					const u8 *iprop = prop->value;
					for (i =0; i < prop->length; i++)
						caps_info->button_name[i] = iprop[i];
			}

			rc = of_property_read_u32(pp, "is_width_supported", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read is_width_supported\n");
				return rc;
			} else
				caps_info->is_width_supported =  temp_val;

			rc = of_property_read_u32(pp, "is_pressure_supported", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read is_pressure_supported\n");
				return rc;
			} else
				caps_info->is_pressure_supported =	temp_val;

			rc = of_property_read_u32(pp, "is_id_supported", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read is_id_supported\n");
				return rc;
			} else
				caps_info->is_id_supported =  temp_val;

			rc = of_property_read_u32(pp, "max_width", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read max_width\n");
				return rc;
			} else
				caps_info->max_width =	temp_val;

			rc = of_property_read_u32(pp, "max_pressure", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read max_pressure\n");
				return rc;
			} else
				caps_info->max_pressure =  temp_val;

			rc = of_property_read_u32(pp, "max_id", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read max_id\n");
				return rc;
			} else
				caps_info->max_id =  temp_val;

			rc = of_property_read_u32(pp, "x_max", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read x_max\n");
				return rc;
			} else
				caps_info->x_max =	temp_val;

			rc = of_property_read_u32(pp, "y_max", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read y_max\n");
				return rc;
			} else
				caps_info->y_max =	temp_val;

			rc = of_property_read_u32(pp, "lcd_x", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read lcd_x\n");
				return rc;
			} else
				caps_info->lcd_x =	temp_val;

			rc = of_property_read_u32(pp, "lcd_y", &temp_val);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "Unable to read lcd_y\n");
				return rc;
			} else
				caps_info->lcd_y =	temp_val;

			caps_info++;
	}

	/* Role */
		pdata->num_role = 0;
		pp = NULL;
		while ((pp = of_get_next_child(node, pp)))
			pdata->num_role++;

		if (!pdata->num_role)
			return 0;

		role_info = devm_kzalloc(dev, pdata->num_role * sizeof(struct touch_operation_role), GFP_KERNEL);
		if (!role_info)
			return -ENOMEM;

		pdata->role = role_info;
		for_each_child_of_node(node, pp) {
				rc = of_property_read_u32(pp, "operation_mode", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read operation_mode\n");
					return rc;
				} else
					role_info->operation_mode =  temp_val;

				rc = of_property_read_u32(pp, "key_type", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read key_type\n");
					return rc;
				} else
					role_info->key_type =  temp_val;

				rc = of_property_read_u32(pp, "report_mode", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read report_mode\n");
					return rc;
				} else
					role_info->report_mode =  temp_val;

				rc = of_property_read_u32(pp, "delta_pos_threshold", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read delta_pos_threshold\n");
					return rc;
				} else
					role_info->delta_pos_threshold =  temp_val;

				rc = of_property_read_u32(pp, "orientation", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read orientation\n");
					return rc;
				} else
					role_info->orientation =  temp_val;

				rc = of_property_read_u32(pp, "report_period", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read report_period\n");
					return rc;
				} else
					role_info->report_period =	temp_val;

				rc = of_property_read_u32(pp, "booting_delay", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read booting_delay\n");
					return rc;
				} else
					role_info->booting_delay =	temp_val;

				rc = of_property_read_u32(pp, "reset_delay", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read reset_delay\n");
					return rc;
				} else
					role_info->reset_delay =  temp_val;

				rc = of_property_read_u32(pp, "suspend_pwr", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read suspend_pwr\n");
					return rc;
				} else
					role_info->suspend_pwr =  temp_val;

				rc = of_property_read_u32(pp, "resume_pwr", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read resume_pwr\n");
					return rc;
				} else
					role_info->resume_pwr =  temp_val;

				rc = of_property_read_u32(pp, "jitter_filter_enable", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read jitter_filter_enable\n");
					return rc;
				} else
					role_info->jitter_filter_enable =  temp_val;

				rc = of_property_read_u32(pp, "jitter_curr_ratio", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read jitter_curr_ratio\n");
					return rc;
				} else
					role_info->jitter_curr_ratio =	temp_val;

				rc = of_property_read_u32(pp, "accuracy_filter_enable", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read accuracy_filter_enable\n");
					return rc;
				} else
					role_info->accuracy_filter_enable =  temp_val;

				rc = of_property_read_u32(pp, "ghost_finger_solution_enable", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read ghost_finger_solution_enable\n");
					return rc;
				} else
					role_info->ghost_finger_solution_enable =  temp_val;

				rc = of_property_read_u32(pp, "ghost_detection_enable", &temp_val);
				if (rc && (rc != -EINVAL)) {
					dev_err(dev, "Unable to read ghost_detection_enable\n");
					return rc;
				} else
					role_info->ghost_detection_enable =  temp_val;

				of_property_read_u32(pp, "irqflags", (u32 *)&role_info->irqflags);

				role_info++;
			}

	/* pwr */
		pdata->num_pwr = 0;
		pp = NULL;
		while ((pp = of_get_next_child(node, pp)))
			pdata->num_pwr++;

		if (!pdata->num_pwr)
			return 0;

		pwr_info = devm_kzalloc(dev, pdata->num_pwr * sizeof(struct touch_power_module), GFP_KERNEL);
		if (!pwr_info)
			return -ENOMEM;

		pdata->pwr = pwr_info;


		for_each_child_of_node(node, pp) {

		rc = of_property_read_u32(pp, "use_regulator", &temp_val);
		if (rc) {
			dev_err(dev, "Unable to read operation_mode\n");
			return rc;
		} else
			pwr_info->use_regulator =  temp_val;

		rc = of_property_read_string(pp, "vdd", &reg_string);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Looking up %s property in node %s failed",
				"vdd",
				reg_string);
			return -ENODEV;
		}
		len = strlen(reg_string);
		memcpy(pwr_info->vdd, reg_string, len);

		rc = of_property_read_u32(pp, "vdd_voltage", &temp_val);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read vdd_voltage\n");
			return rc;
		} else
			pwr_info->vdd_voltage =  temp_val;

		rc = of_property_read_string(pp, "vio", &reg_string);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Looking up %s property in node %s failed",
				"vio",
				reg_string);
			return -ENODEV;
		}
		len = strlen(reg_string);
		memcpy(pwr_info->vio, reg_string, len);

		/*rc = of_property_read_u32(pp, "vio_voltage", &temp_val);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read vio_voltage\n");
			return rc;
		} else
			pwr_info->vio_voltage =  temp_val;
		*/
		pwr_info->power = synaptics_power_on;


		pwr_info++;
			}
	return 0;

}

#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
#ifndef CONFIG_LGE_SECURITY_KNOCK_ON
static void touch_double_tap_wakeup_enable(struct lge_touch_data *ts)
{
	if (ts->curr_pwr_state != POWER_OFF)
		touch_power_cntl(ts, POWER_OFF);
	touch_power_cntl(ts, POWER_ON);
	if (touch_enable_irq_wake(ts) != 0){
		TOUCH_INFO_MSG("enable_irq_wake failed\n");
	}
	msleep(ts->pdata->role->booting_delay);
	enable_irq(ts->client->irq);
	TOUCH_INFO_MSG("%s : enable_irq !!\n", __func__);
	touch_ic_init(ts);

	if(touch_device_func->ic_ctrl) {
		if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_DOUBLE_TAP_WAKEUP_MODE, 1) < 0){
			TOUCH_ERR_MSG("IC_CTRL_DOUBLE_TAP_WAKEUP_MODE handling fail\n");
			return;
		}
	}
}
#endif

#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
static void touch_multi_tap_wakeup_enable(struct lge_touch_data *ts, int value)
{
	TOUCH_INFO_MSG("[%s] value=%d\n", __func__, value);
	if (ts->curr_pwr_state != POWER_OFF)
		touch_power_cntl(ts, POWER_OFF);
	touch_power_cntl(ts, POWER_ON);
	if (touch_enable_irq_wake(ts) != 0){
		TOUCH_INFO_MSG("enable_irq_wake failed\n");
	}
	msleep(ts->pdata->role->booting_delay);
	enable_irq(ts->client->irq);
	TOUCH_INFO_MSG("%s : enable_irq !!\n", __func__);
	touch_ic_init(ts);

	if (touch_device_func->ic_ctrl) {
		if (touch_device_func->ic_ctrl(ts->client, IC_CTRL_DOUBLE_TAP_WAKEUP_MODE, value) < 0) {
			TOUCH_ERR_MSG("IC_CTRL_DOUBLE_TAP_WAKEUP_MODE handling fail\n");
			return;
		}
	}
}
#endif
#endif

static int touch_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lge_touch_data *ts;
	int ret = 0;
	int one_sec = 0;
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
	wake_lock_init(&touch_wake_lock, WAKE_LOCK_SUSPEND, "touch_irq");
	mutex_init(&i2c_suspend_lock);
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TOUCH_ERR_MSG("i2c functionality check error\n");
		ret = -EPERM;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct lge_touch_data), GFP_KERNEL);
	if (ts == NULL) {
		TOUCH_ERR_MSG("Can not allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	if (client->dev.of_node) {
		ts->pdata = devm_kzalloc(&client->dev,
			sizeof(struct touch_platform_data), GFP_KERNEL);
		if (!ts->pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_alloc_data_failed;
		}

		ret = synaptics_parse_dt(&client->dev, ts->pdata);
		if (ret)
			goto err_alloc_platformdata_failed;

	} else{
	ts->pdata = client->dev.platform_data;
			   if (!ts->pdata) {
				   ret = -EINVAL;
				   goto err_alloc_data_failed;
			   }
		   }

	ts->client = client;

#ifdef CUST_G2_TOUCH
	ds4_i2c_client = client;
#endif
	ts->fw_info.fw_force_rework = false;

	i2c_set_clientdata(client, ts);

	ret = check_platform_data(ts->pdata);
	if (ret < 0) {
		TOUCH_ERR_MSG("Can not read platform data\n");
		ret = -EINVAL;
		goto err_assign_platform_data;
	}

	one_sec = 1000000 / (ts->pdata->role->report_period/1000);
	ts->ic_init_err_cnt = 0;
	ts->work_sync_err_cnt = 0;

	/* Ghost-Finger solution
	 * max_count : if melt-time > 7-sec, max_count should be changed to 'one_sec * 3'
	 */
	ts->gf_ctrl.min_count = one_sec / 12;
	ts->gf_ctrl.max_count = one_sec * 3;
	ts->gf_ctrl.saved_x = -1;
	ts->gf_ctrl.saved_y = -1;
	ts->gf_ctrl.saved_last_x = 0;
	ts->gf_ctrl.saved_last_y = 0;
	ts->gf_ctrl.max_moved = ts->pdata->caps->x_max / 4;
	ts->gf_ctrl.max_pressure = 255;

	get_section(&ts->st_info, ts->pdata);

	/* Specific device probe */
	if (touch_device_func->probe) {
		ret = touch_device_func->probe(ts);
		if (ret < 0) {
			TOUCH_ERR_MSG("specific device probe fail\n");
			goto err_assign_platform_data;
		}
	}

	/* reset pin setting */
	if (ts->pdata->reset_pin > 0){
		ret = gpio_request(ts->pdata->reset_pin, "touch_reset");
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_reset gpio_request\n");
			goto err_assign_platform_data;
		}
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
		gpio_direction_output(ts->pdata->reset_pin, 0);
		gpio_tlmm_config(GPIO_CFG(ts->pdata->reset_pin, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_6MA), GPIO_CFG_ENABLE);
#else
		gpio_direction_output(ts->pdata->reset_pin, 1);
#endif
	}


	atomic_set(&ts->device_init, 0);

	/* Power on */
	if (touch_power_cntl(ts, POWER_ON) < 0)
		goto err_power_failed;

	msleep(ts->pdata->role->booting_delay);

	/* init work_queue */
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY) {
		INIT_WORK(&ts->work, touch_work_func_a);
		INIT_DELAYED_WORK(&ts->work_touch_lock, touch_lock_func);
	}
	else if (ts->pdata->role->key_type == TOUCH_SOFT_KEY)
		INIT_WORK(&ts->work, touch_work_func_b);
	else
		INIT_WORK(&ts->work, touch_work_func_c);

	INIT_DELAYED_WORK(&ts->work_init, touch_init_func);
	INIT_WORK(&ts->work_fw_upgrade, touch_fw_upgrade_func);
#ifdef CUST_G2_TOUCH
	INIT_DELAYED_WORK(&ts->work_f54, touch_f54_func);
	INIT_DELAYED_WORK(&ts->work_thermal, change_fw_func);
#ifdef I2C_SUSPEND_WORKAROUND
	INIT_DELAYED_WORK(&ts->check_suspended_work, synaptics_touch_check_suspended_worker);
#endif
#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU))  || defined(CONFIG_LGE_Z_TOUCHSCREEN)
	INIT_DELAYED_WORK(&ts->work_ime_drumming, change_ime_drumming_func);
#endif
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
	INIT_DELAYED_WORK(&ts->work_gesture_wakeup, touch_gesture_wakeup_func);
#endif
#endif

	/* input dev setting */
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		TOUCH_ERR_MSG("Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	/* Auto Test interface */
	touch_test_dev = ts;

	input_set_drvdata(ts->input_dev, ts);

	ts->input_dev->name = "touch_dev";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#endif

	if (ts->pdata->caps->button_support && ts->pdata->role->key_type != VIRTUAL_KEY) {
		set_bit(EV_KEY, ts->input_dev->evbit);
		for(ret = 0; ret < ts->pdata->caps->number_of_button; ret++) {
			set_bit(ts->pdata->caps->button_name[ret], ts->input_dev->keybit);
		}
	}

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->pdata->caps->x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
			ts->pdata->caps->y_button_boundary
			? ts->pdata->caps->y_button_boundary
			: ts->pdata->caps->y_max,
			0, 0);
	if (ts->pdata->caps->is_pressure_supported)
		input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, ts->pdata->caps->max_pressure, 0, 0);
	if (ts->pdata->caps->is_width_supported) {
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, ts->pdata->caps->max_width, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MINOR, 0, ts->pdata->caps->max_width, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	}

#if defined(MT_PROTOCOL_A)
	if (ts->pdata->caps->is_id_supported)
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->pdata->caps->max_id, 0, 0);
#else
	input_mt_init_slots(ts->input_dev, ts->pdata->caps->max_id);
#endif

	ret = input_register_device(ts->input_dev);
	if (ret < 0) {
		TOUCH_ERR_MSG("Unable to register %s input device\n",
				ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	/* interrupt mode */
	if (ts->pdata->role->operation_mode) {
		ret = gpio_request(ts->pdata->int_pin, "touch_int");
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_int gpio_request\n");
			goto err_interrupt_failed;
		}
		gpio_direction_input(ts->pdata->int_pin);

		ret = request_threaded_irq(client->irq, touch_irq_handler,
				touch_thread_irq_handler,
				ts->pdata->role->irqflags | IRQF_ONESHOT, client->name, ts);

		if (ret < 0) {
			TOUCH_ERR_MSG("request_irq failed. use polling mode\n");
			goto err_interrupt_failed;
		}
	}
	/* polling mode */
	else{
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = touch_timer_handler;
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);
	}

#ifdef CUST_G2_TOUCH
	if (ts->pdata->role->ghost_detection_enable) {
       hrtimer_init(&hr_touch_trigger_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
       hr_touch_trigger_timer.function = touch_trigger_timer_handler;
	}
#endif

	/* ghost-finger solution */
	ts->gf_ctrl.probe = 1;

	/* get current lcd info from kernel command line */
	ts->fw_info.fw_setting.curr_touch_vendor = lge_get_panel_maker();
	ts->fw_info.fw_setting.prev_touch_vendor = ts->fw_info.fw_setting.curr_touch_vendor;
	TOUCH_INFO_MSG("lcd_maker_id = %d, func: %s\n", ts->fw_info.fw_setting.curr_touch_vendor, __func__);

	mutex_init(&ts->irq_work_mutex);
	/* Specific device initialization */
#ifdef CUST_G2_TOUCH
	disable_irq(ts->client->irq);
	TOUCH_INFO_MSG("%s : disable_irq !!\n", __func__);
	release_all_ts_event(ts);
	touch_ic_init(ts);

#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
#if !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_A){
		touch_gesture_enable = 0;
	}
#endif
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	if( (int)simple_strtoul(&ts->fw_info.ic_fw_version[1], NULL, 10) < 4){
		touch_gesture_enable = 0;
		TOUCH_INFO_MSG("This panel is an old version, Gesture is not supported\n");
	}
#endif
#endif

	enable_irq(ts->client->irq);
	TOUCH_INFO_MSG("%s : enable_irq !!\n", __func__);

	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO) {
		TOUCH_INFO_MSG("lge_get_boot_mode() is LGE_BOOT_MODE_CHARGERLOGO\n");
		disable_irq(ts->client->irq);
		TOUCH_INFO_MSG("%s : disable_irq !!\n", __func__);
		touch_power_cntl(ts, POWER_OFF);
		ts_suspend = 1;
	} else {
		TOUCH_INFO_MSG("lge_get_boot_mode() is not LGE_BOOT_MODE_CHARGERLOGO\n");
	}
#else
	touch_ic_init(ts);
#endif
	/* Firmware Upgrade Check - use thread for booting time reduction */
	if (touch_device_func->fw_upgrade) {
		queue_work(touch_wq, &ts->work_fw_upgrade);
	}

	/* jitter solution */
	if (ts->pdata->role->jitter_filter_enable){
		ts->jitter_filter.adjust_margin = 1000;
	}

	/* accuracy solution */
	if (ts->pdata->role->accuracy_filter_enable){
		ts->accuracy_filter.ignore_pressure_gap = 5;
		ts->accuracy_filter.delta_max = 30;
		ts->accuracy_filter.max_pressure = 255;
		ts->accuracy_filter.time_to_max_pressure = one_sec / 20;
		ts->accuracy_filter.direction_count = one_sec / 10;
		ts->accuracy_filter.touch_max_count = one_sec / 2;
		ts->accuracy_filter.pen_pressure = 35;
	}

#if defined(CONFIG_FB)
	ts->notif.notifier_call = lcd_notifier_callback;
	if((ret = lcd_register_client(&ts->notif)))
		TOUCH_ERR_MSG("Unable to register notifier: %d\n", ret);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = touch_early_suspend;
	ts->early_suspend.resume = touch_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	/* Register sysfs for making fixed communication path to framework layer */
	ret = sysdev_class_register(&lge_touch_sys_class);
	if (ret < 0) {
		TOUCH_ERR_MSG("sysdev_class_register is failed\n");
		goto err_lge_touch_sys_class_register;
	}

	ret = sysdev_register(&lge_touch_sys_device);
	if (ret < 0) {
		TOUCH_ERR_MSG("sysdev_register is failed\n");
		goto err_lge_touch_sys_dev_register;
	}

	ret = kobject_init_and_add(&ts->lge_touch_kobj, &lge_touch_kobj_type,
			ts->input_dev->dev.kobj.parent,
			"%s", LGE_TOUCH_NAME);
	if (ret < 0) {
		TOUCH_ERR_MSG("kobject_init_and_add is failed\n");
		goto err_lge_touch_sysfs_init_and_add;
	}

	if (likely(touch_debug_mask & DEBUG_BASE_INFO))
		TOUCH_INFO_MSG("Touch driver is initialized\n");

#ifdef CUST_G2_TOUCH
#if defined(CONFIG_LGE_Z_TOUCHSCREEN) || defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	TOUCH_INFO_MSG("touch_f54_func is not executed\n");
#else
#if defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	TOUCH_INFO_MSG("G1F : touch_f54_func is not executed\n");
#else
	if(lge_get_boot_mode() == LGE_BOOT_MODE_NORMAL) {
		queue_delayed_work(touch_wq, &ts->work_f54,
				msecs_to_jiffies(ts->pdata->role->booting_delay));
	}
#endif
#endif
#endif

	return 0;

err_lge_touch_sysfs_init_and_add:
	kobject_del(&ts->lge_touch_kobj);
err_lge_touch_sys_dev_register:
	sysdev_unregister(&lge_touch_sys_device);
err_lge_touch_sys_class_register:
	sysdev_class_unregister(&lge_touch_sys_class);
	mutex_destroy(&ts->irq_work_mutex);
	mutex_destroy(&i2c_suspend_lock);
#if defined(CONFIG_FB)
	lcd_unregister_client(&ts->notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif
err_interrupt_failed:
	if (ts->pdata->role->operation_mode)
		free_irq(ts->client->irq, ts);
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	touch_power_cntl(ts, POWER_OFF);
err_power_failed:
err_assign_platform_data:
	kfree(ts);
err_alloc_platformdata_failed:
	kfree(ts->pdata);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int touch_remove(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	/* Specific device remove */
	if (touch_device_func->remove)
		touch_device_func->remove(ts->client);

	/* Power off */
	touch_power_cntl(ts, POWER_OFF);

	kobject_del(&ts->lge_touch_kobj);
	sysdev_unregister(&lge_touch_sys_device);
	sysdev_class_unregister(&lge_touch_sys_class);

#if defined(CONFIG_FB)
	if (lcd_unregister_client(&ts->notif))
		TOUCH_ERR_MSG("Error occurred while unregistering notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	if (ts->pdata->role->operation_mode)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
#ifdef CUST_G2_TOUCH
	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}
#endif
	mutex_destroy(&ts->irq_work_mutex);
	mutex_destroy(&i2c_suspend_lock);

	input_unregister_device(ts->input_dev);
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
	wake_lock_destroy(&touch_wake_lock);
#endif
	kfree(ts);

	return 0;
}

#if defined(CONFIG_FB)
static int touch_lcd_suspend(struct device *device)
{
	struct lge_touch_data *ts =  dev_get_drvdata(device);
#ifdef I2C_SUSPEND_WORKAROUND
	if (!ts) {
		TOUCH_ERR_MSG("Called before init\n");
		return 0;
	}
#endif

#ifdef CUST_G2_TOUCH
	if(!ts->curr_pwr_state) {
		ts_suspend = 1;
		TOUCH_INFO_MSG("touch_suspend is not executed curr_pwr_state\n");
		return 0;
	}
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (ts->fw_info.fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("touch_suspend is not executed\n");
			return 0;
	}

	if (ts->pdata->role->operation_mode){
		disable_irq(ts->client->irq);
		TOUCH_INFO_MSG("%s : disable_irq !!\n", __func__);
	}
	else
		hrtimer_cancel(&ts->timer);

#ifdef CUST_G2_TOUCH
	if (ts->pdata->role->ghost_detection_enable) {
		hrtimer_cancel(&hr_touch_trigger_timer);
	}
#endif
	cancel_work_sync(&ts->work);
	cancel_delayed_work_sync(&ts->work_init);
	cancel_delayed_work_sync(&ts->work_thermal);
#ifdef I2C_SUSPEND_WORKAROUND
	cancel_delayed_work_sync(&ts->check_suspended_work);
#endif
#if (defined(A1_only) && !defined(CONFIG_MACH_MSM8974_G2_KDDI) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) && !defined(CONFIG_MACH_MSM8974_G2_OPT_AU)) || defined(CONFIG_LGE_Z_TOUCHSCREEN)
	cancel_delayed_work_sync(&ts->work_ime_drumming);
#endif
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	release_all_ts_event(ts);

	mutex_lock(&ts->irq_work_mutex);
#ifdef I2C_SUSPEND_WORKAROUND
	dev_dbg(&ts->client->dev, " %s\n", __func__);
#endif
	ts_suspend = 1;
#if defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	knockon_wakeup_uevent_reporting = 0;
#endif
#ifdef CUST_G2_TOUCH
#if defined(CONFIG_LGE_Z_TOUCHSCREEN) || defined(CONFIG_LGE_VU3_TOUCHSCREEN)
	TOUCH_INFO_MSG("touch_f54_func is not executed\n");
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
		if(touch_gesture_enable) {
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
			touch_multi_tap_wakeup_enable(ts, touch_gesture_enable);
#else
			touch_double_tap_wakeup_enable(ts);
#endif
		} else {
			touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
		}
#endif
#else
	if((atomic_read(&touch_test_dev->device_init) == 1)) {
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
		if(touch_gesture_enable) {
#ifdef CONFIG_LGE_SECURITY_KNOCK_ON
			touch_multi_tap_wakeup_enable(ts, touch_gesture_enable);
#else
			touch_double_tap_wakeup_enable(ts);
#endif
#if defined(A1_only)
			if(f54_window_crack)
				f54_window_crack = 2;
#if !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || !defined(CONFIG_MACH_MSM8974_G2_KR)
		} else if (lge_get_boot_mode() != LGE_BOOT_MODE_NORMAL) {
			touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
#endif
#endif
		} else
#endif
#if defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
		{
			TOUCH_INFO_MSG("G1F : touch_f54_func is not executed\n");
			touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
		}
#else
		queue_delayed_work(touch_wq, &ts->work_f54,
				msecs_to_jiffies(10));
#endif
	} else
		touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
#endif
#else
	touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
#endif
	mutex_unlock(&ts->irq_work_mutex);

	return 0;

}

static int touch_lcd_resume(struct device *device)
{
	struct lge_touch_data *ts =  dev_get_drvdata(device);
	int	ret=0;
#ifdef I2C_SUSPEND_WORKAROUND
	if (!ts) {
		TOUCH_ERR_MSG("Called before init\n");
		return 0;
	}
#endif
#ifdef CUST_G2_TOUCH
	cancel_delayed_work_sync(&ts->work_f54);
	if(f54_window_crack) {
		char *window_crack[2] = { "TOUCH_WINDOW_STATE=CRACK", NULL };
		kobject_uevent_env(&lge_touch_sys_device.kobj, KOBJ_CHANGE, window_crack);
		if(f54_window_crack==2)
			f54_window_crack = 0;
		TOUCH_INFO_MSG("Window crack uEvent\n");
	}

	if(ts_suspend == 0)
		return 0;
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	mutex_lock(&ts->irq_work_mutex);
#ifdef I2C_SUSPEND_WORKAROUND
	dev_dbg(&ts->client->dev, " %s\n", __func__);
#endif
	if (ts->fw_info.fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("touch_resume is not executed\n");
		goto failed_out;
	}

#ifdef CUST_G2_TOUCH
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
#if defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
	if(ts->fw_info.fw_setting.ic_chip_rev != 0){
#else
	if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_B){
#endif
		if (touch_disable_irq_wake(ts) != 0){
			TOUCH_INFO_MSG("disable_irq_wake failed\n");
		}

		if(touch_gesture_enable){
			disable_irq(ts->client->irq);
			TOUCH_INFO_MSG("%s : disable_irq !!\n", __func__);
		}
		if (ts->curr_pwr_state != POWER_OFF)
			touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
		touch_power_cntl(ts, ts->pdata->role->resume_pwr);
		msleep(ts->pdata->role->booting_delay);
	} else{
		if(ts->curr_pwr_state) {
			TOUCH_INFO_MSG("touch_resume is not executed curr_pwr_state\n");
			goto done_out;
		}
	}
#else
	if(ts->curr_pwr_state) {
		TOUCH_INFO_MSG("touch_resume is not executed curr_pwr_state\n");
		goto done_out;
	}
#endif
#endif

#if defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
		if(touch_device_func->ic_ctrl) {
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_DOUBLE_TAP_WAKEUP_MODE, 0) < 0){
				TOUCH_ERR_MSG("IC_CTRL_DOUBLE_TAP_WAKEUP_MODE handling fail\n");
				goto failed_out;
			}
		}
#else
	touch_power_cntl(ts, ts->pdata->role->resume_pwr);
#endif
#else
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
	if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_A)
#endif
	touch_power_cntl(ts, ts->pdata->role->resume_pwr);
#ifdef CUST_G2_TOUCH_WAKEUP_GESTURE
	if(ts->fw_info.fw_setting.ic_chip_rev == TOUCH_CHIP_REV_B) {
		if(touch_device_func->ic_ctrl) {
			if(touch_device_func->ic_ctrl(ts->client, IC_CTRL_DOUBLE_TAP_WAKEUP_MODE, 0) < 0){
				TOUCH_ERR_MSG("IC_CTRL_DOUBLE_TAP_WAKEUP_MODE handling fail\n");
				goto failed_out;
			}
		}
	}
#endif
#endif
#ifdef CUST_G2_TOUCH
	if(ts->pdata->role->ghost_detection_enable) {
		ts->gd_ctrl.is_resume = 1;
	}
#endif

	if (ts->pdata->role->operation_mode){
		enable_irq(ts->client->irq);
		TOUCH_INFO_MSG("%s : enable_irq !!\n", __func__);
	}
	else
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	if (ts->pdata->role->resume_pwr == POWER_ON)
		queue_delayed_work(touch_wq, &ts->work_init,
				msecs_to_jiffies(ts->pdata->role->booting_delay));
	else
		queue_delayed_work(touch_wq, &ts->work_init, 0);

done_out:
	ts_suspend = 0;
	ret=1;

failed_out:
	mutex_unlock(&ts->irq_work_mutex);
	return ret;

}

static int lcd_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct lge_touch_data *ts = container_of(self, struct lge_touch_data, notif);

	TOUCH_INFO_MSG("%s: event = %lu\n", __func__, event);

	switch(event){
		case LCD_EVENT_ON_START:
#ifdef CUST_G2_TOUCH
			if(f54_window_crack_check_mode)
				mdelay(200);
#endif
			touch_lcd_resume(&ts->client->dev);
			TOUCH_INFO_MSG("touch_resume\n");
			break;
		case LCD_EVENT_OFF_START:
			touch_lcd_suspend(&ts->client->dev);
			TOUCH_INFO_MSG("touch_suspend\n");
			break;
		default:
			break;
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void touch_early_suspend(struct early_suspend *h)
{
	struct lge_touch_data *ts =
			container_of(h, struct lge_touch_data, early_suspend);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (ts->fw_info.fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("early_suspend is not executed\n");
		return;
	}

	if (ts->pdata->role->operation_mode){
		disable_irq(ts->client->irq);
		TOUCH_INFO_MSG("%s : disable_irq !!\n", __func__);
	}
	else
		hrtimer_cancel(&ts->timer);

	cancel_work_sync(&ts->work);
	cancel_delayed_work_sync(&ts->work_init);
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	release_all_ts_event(ts);

	touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
}

static void touch_late_resume(struct early_suspend *h)
{
	struct lge_touch_data *ts =
			container_of(h, struct lge_touch_data, early_suspend);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (ts->fw_info.fw_upgrade.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("late_resume is not executed\n");
		return;
	}

	touch_power_cntl(ts, ts->pdata->role->resume_pwr);

	if (ts->pdata->role->operation_mode){
		enable_irq(ts->client->irq);
		TOUCH_INFO_MSG("%s : enable_irq !!\n", __func__);
	}
	else
		hrtimer_start(&ts->timer, ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	if (ts->pdata->role->resume_pwr == POWER_ON)
		queue_delayed_work(touch_wq, &ts->work_init,
				msecs_to_jiffies(ts->pdata->role->booting_delay));
	else
		queue_delayed_work(touch_wq, &ts->work_init, 0);
}
#endif

#if defined(CONFIG_PM)
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
static int touch_suspend(struct device *device)
{
	return 0;
}

static int touch_resume(struct device *device)
{
	return 0;
}
#elif defined(CUST_G2_TOUCH_WAKEUP_GESTURE)
static int touch_suspend(struct device *device)
{
	mutex_lock(&i2c_suspend_lock);
		TOUCH_DEBUG_MSG("\n");
	return 0;
}

static int touch_resume(struct device *device)
{
	mutex_unlock(&i2c_suspend_lock);
		TOUCH_DEBUG_MSG("\n");
	return 0;
}

#endif
#endif

static struct of_device_id synaptics_match_table[] = {
	{ .compatible = "synaptics,s3404",},
	{ },
};

static struct i2c_device_id lge_ts_id[] = {
	{LGE_TOUCH_NAME, 0 },
};

#if defined(CONFIG_PM)
static struct dev_pm_ops touch_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))||defined(CUST_G2_TOUCH_WAKEUP_GESTURE)
	.suspend 	= touch_suspend,
	.resume 	= touch_resume,
#endif
};
#endif

static struct i2c_driver lge_touch_driver = {
	.probe   = touch_probe,
	.remove	 = touch_remove,
	.id_table = lge_ts_id,
	.driver	 = {
		.name   = LGE_TOUCH_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = synaptics_match_table,
#if defined(CONFIG_PM)
		.pm		= &touch_pm_ops,
#endif
	},
};

int touch_driver_register(struct touch_device_driver* driver)
{
	int ret = 0;

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func != NULL) {
		TOUCH_ERR_MSG("CANNOT add new touch-driver\n");
		ret = -EMLINK;
		goto err_touch_driver_register;
	}

	touch_device_func = driver;

	touch_wq = create_singlethread_workqueue("touch_wq");
	if (!touch_wq) {
		TOUCH_ERR_MSG("CANNOT create new workqueue\n");
		ret = -ENOMEM;
		goto err_work_queue;
	}

	ret = i2c_add_driver(&lge_touch_driver);
	if (ret < 0) {
		TOUCH_ERR_MSG("FAIL: i2c_add_driver\n");
		goto err_i2c_add_driver;
	}

	return 0;

err_i2c_add_driver:
	destroy_workqueue(touch_wq);
err_work_queue:
err_touch_driver_register:
	return ret;
}

void touch_driver_unregister(void)
{
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	i2c_del_driver(&lge_touch_driver);
	touch_device_func = NULL;

	if (touch_wq)
		destroy_workqueue(touch_wq);
}

