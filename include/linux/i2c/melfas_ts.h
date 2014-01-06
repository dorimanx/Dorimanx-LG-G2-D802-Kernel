/*
 * include/linux/melfas_ts.h - platform data structure for MCS Series sensor
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

#ifndef _LINUX_MELFAS_TS_H
#define _LINUX_MELFAS_TS_H

#define MELFAS_TS_NAME "melfas-ts"

#define MAX_NUM_OF_BUTTON			3

struct melfas_tsi_platform_data {
	int gpio_scl;
	int gpio_sda;
	int i2c_int_gpio;
	int (*power_enable)(int en, bool log_en);
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

#endif /* _LINUX_MELFAS_TS_H */
