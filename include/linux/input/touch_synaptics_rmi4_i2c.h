/* lge/include/touch_synaptics_rmi4_i2c.h
 *
 * Copyright (C) 2011 LGE, Inc.
 *
 * Author: hyesung.shin@lge.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __TOUCH_SYNAPTICS_RMI4_I2C_H
#define __TOUCH_SYNAPTICS_RMI4_I2C_H
#include <linux/earlysuspend.h>

#define MAX_NUM_OF_BUTTON			4   
#define MAX_NUM_OF_FINGER			10

struct synaptics_ts_platform_data {
	bool use_irq;
	unsigned long irqflags;
	unsigned short i2c_sda_gpio;
	unsigned short i2c_scl_gpio;
	unsigned short i2c_int_gpio;
	int (*power)(int on, bool log_on);
	unsigned short ic_booting_delay;		/* ms */
	unsigned long report_period;			/* ns */
	unsigned char num_of_finger;
	unsigned char num_of_button;
	unsigned short button[MAX_NUM_OF_BUTTON];
	int x_max;
	int y_max;
	unsigned char fw_ver;
	unsigned int palm_threshold;
	unsigned int delta_pos_threshold;
};

typedef struct {
	u8 query_base;
	u8 command_base;
	u8 control_base;
	u8 data_base;
	u8 int_source_count;
	u8 id;
} ts_function_descriptor;

typedef struct {
	unsigned int pos_x[MAX_NUM_OF_FINGER];
	unsigned int pos_y[MAX_NUM_OF_FINGER];
	unsigned char pressure[MAX_NUM_OF_FINGER];
} ts_finger_data;

struct synaptics_ts_timestamp {
	u64 start;
	u64 end;
	u64 result_t;
	unsigned long rem;
	atomic_t ready;
};

/* Device data structure */
struct synaptics_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct synaptics_ts_platform_data *pdata;
	bool is_downloading;		/* avoid power off during F/W upgrade */
	bool is_suspended;			/* avoid power off during F/W upgrade */
	unsigned int button_width;
	char button_prestate[MAX_NUM_OF_BUTTON];
	char finger_prestate[MAX_NUM_OF_FINGER];
	bool ic_init;
	bool is_probed;
	bool melt_mode;					/* for Ghost finger defense - melt mode status */
	unsigned int idle_lock_distance;		/* for Ghost finger defense - lock screen drag distance */
	atomic_t interrupt_handled;
	ts_function_descriptor common_dsc;
	ts_function_descriptor finger_dsc;
	ts_function_descriptor button_dsc;
	unsigned char int_status_reg_asb0_bit;
	unsigned char int_status_reg_button_bit;
	unsigned char curr_int_mask;
	struct hrtimer timer;
	struct delayed_work work;
	struct delayed_work button_lock_work;
	struct early_suspend early_suspend;
	struct synaptics_ts_timestamp int_delay;
	ts_finger_data pre_ts_data;
	char fw_rev;
	char manufcturer_id;
	char product_id[11];
	char fw_path[256];
	char fw_test;
#if 1//J1_BSP_TOUCH
	char fw_config_id[5];
	u8 button_data_type;
#endif
};

/* Debug Mask setting */
#define SYNAPTICS_RMI4_I2C_DEBUG_PRINT   (1)
#define SYNAPTICS_RMI4_I2C_ERROR_PRINT   (1)
#define SYNAPTICS_RMI4_I2C_INFO_PRINT   (1)

#if defined(SYNAPTICS_RMI4_I2C_INFO_PRINT)
#define SYNAPTICS_INFO_MSG(fmt, args...) \
			printk(KERN_INFO "[Touch] " fmt, ##args);
#else
#define SYNAPTICS_INFO_MSG(fmt, args...)     {};
#endif

#if defined(SYNAPTICS_RMI4_I2C_DEBUG_PRINT)
#define SYNAPTICS_DEBUG_MSG(fmt, args...) \
			printk(KERN_INFO "[Touch D] [%s %d] " \
				fmt, __FUNCTION__, __LINE__, ##args);
#else
#define SYNAPTICS_DEBUG_MSG(fmt, args...)     {};
#endif

#if defined(SYNAPTICS_RMI4_I2C_ERROR_PRINT)
#define SYNAPTICS_ERR_MSG(fmt, args...) \
			printk(KERN_ERR "[Touch E] [%s %d] " \
				fmt, __FUNCTION__, __LINE__, ##args);
#else
#define SYNAPTICS_ERR_MSG(fmt, args...)     {};
#endif

/* SYNAPTICS_RMI4_I2C Debug mask value
 * usage: echo [debug_mask] > /sys/module/touch_synaptics_rmi4_i2c/parameters/debug_mask
 * All			: 8191 (0x1FFF)
 * No msg		: 32
 * default		: 0
 */
enum {
	SYNAPTICS_RMI4_I2C_DEBUG_NONE				= 0,
	SYNAPTICS_RMI4_I2C_DEBUG_FUNC_TRACE			= 1U << 0,	/* 1 */
	SYNAPTICS_RMI4_I2C_DEBUG_INT_STATUS			= 1U << 1,	/* 2 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_STATUS		= 1U << 2,	/* 4 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_POSITION	= 1U << 3,	/* 8 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_REG			= 1U << 4,	/* 16 */
	SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_STATUS		= 1U << 5,	/* 32 */
	SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_REG			= 1U << 6,	/* 64 */
	SYNAPTICS_RMI4_I2C_DEBUG_INT_INTERVAL		= 1U << 7,	/* 128 */
	SYNAPTICS_RMI4_I2C_DEBUG_INT_ISR_DELAY		= 1U << 8,	/* 256 */
	SYNAPTICS_RMI4_I2C_DEBUG_FINGER_HANDLE_TIME	= 1U << 9,	/* 512 */
	SYNAPTICS_RMI4_I2C_DEBUG_BUTTON_HANDLE_TIME	= 1U << 10,	/* 1024 */
	SYNAPTICS_RMI4_I2C_DEBUG_UPGRADE_DELAY		= 1U << 11,	/* 2048 */
	SYNAPTICS_DEBUG_FW_UPGRADE					= 1U << 12,	/* 4096 */
};

#define DESCRIPTION_TABLE_START	0xe9

/* Test define for f/w upgrade sanity */
//#define TEST_BOOTING_TIME_FW_FORCE_UPGRADE
//#define TEST_WRONG_CHIPSET_FW_FORCE_UPGRADE
#endif
