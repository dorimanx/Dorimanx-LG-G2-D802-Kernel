/* include/linux/platform_data/hds_max1462x.h
 *
 * Copyright (C) 2012 LG Electronics Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __HDS_MAX1462X_H__
#define __HDS_MAX1462X_H__

struct max1462x_platform_data {
	const char *switch_name;  /* switch device name */
	const char *keypad_name;  /* keypad device name */
	/* key code for hook, volume up, volume down */
	unsigned int key_code;

	unsigned int gpio_mic_en;
	unsigned int gpio_key;
	unsigned int gpio_detect;

	/* callback function which is initialized while probing */
	void (*gpio_set_value_func)(unsigned gpio, int value);
	int (*gpio_get_value_func)(unsigned gpio);

	unsigned int latency_for_key;
};

#endif /* __HDS_MAX1462X_H__ */
