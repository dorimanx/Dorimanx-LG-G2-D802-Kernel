/*  sht21.h - support for the SHT21 Temperature and Humidity Sensor
 *
 *  Copyright (C) 2012 LGE, Inc.
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
 *
 */

#ifdef CONFIG_SENSOR_SHT21

#ifdef CONFIG_OF
/* Analog voltage @2.7 V */
#define AVDD_VTG_MIN_UV		2700000
#define AVDD_VTG_MAX_UV		3300000
#define AVDD_ACTIVE_LOAD_UA	15000

/* Digital voltage @1.8 V */
#define VDDIO_VTG_DIG_MIN_UV	1800000
#define VDDIO_VTG_DIG_MAX_UV	1800000
#define VDDIO_ACTIVE_LOAD_DIG_UA	10000

#define VDDIO_I2C_VTG_MIN_UV	1800000
#define VDDIO_I2C_VTG_MAX_UV	1800000
#define VDDIO_I2C_LOAD_UA 	10000
#endif

struct sht21_platform_data {
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(bool);

#ifdef CONFIG_OF
	bool  i2c_pull_up;
	bool  digital_pwr_regulator;

	unsigned int enable_gpio;
	u32 enable_gpio_flags;

	struct regulator *vcc_ana;
	struct regulator *vcc_dig;
	struct regulator *vcc_i2c;
#endif
};
#endif
