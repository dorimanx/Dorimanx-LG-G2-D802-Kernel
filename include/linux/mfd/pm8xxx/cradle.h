/* include/linux/mfd/pm8xxx/cradle.h
 *
 * Copyright (c) 2011-2012, LG Electronics Inc, All rights reserved.
 * Author: Fred Cho <fred.cho@lge.com>
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

#ifndef __PM8XXX_CRADLE_H__
#define __PM8XXX_CRADLE_H__

#define HALL_IC_DEV_NAME "bu52031nvx"
#define CARKIT_DEV_NAME  "bu52031nvx-carkit"

/* SMART COVER Support */
#define SMARTCOVER_NO_DEV			0
#define SMARTCOVER_POUCH_CLOSED		1
#define SMARTCOVER_POUCH_OPENED		0
#define SMARTCOVER_CAMERA_CLOSED	1
#define SMARTCOVER_CAMERA_OPENED	2
#define SMARTCOVER_CAMERA_VIEW		256

#if defined CONFIG_MACH_MSM8974_VU3_KR
#define SMARTCOVER_PEN_IN			1
#define SMARTCOVER_PEN_OUT			0
#else
#define SMARTCOVER_VIEW_CLOSED		3	// G2 doesn't support
#define SMARTCOVER_VIEW_OPENED		4	// G2 doesn't support
#endif

/* Carkit support */
#ifdef CONFIG_BU52031NVX_CARKIT
#define	CARKIT_NO_DEV				0	// only VZW
#define	CARKIT_DESKDOCK				1	// only VZW
#define CARKIT_DOCKED				2	// only VZW
#endif

#if !defined(CONFIG_MACH_MSM8974_B1_KR) && !defined(CONFIG_LGE_Z_TOUCHSCREEN) && !defined(CONFIG_MACH_MSM8974_VU3_KR)
#define A1_only
#endif

struct pm8xxx_cradle_platform_data {
	int hallic_pouch_detect_pin;
	unsigned int hallic_pouch_irq;
#if defined CONFIG_MACH_MSM8974_VU3_KR
	int hallic_pen_detect_pin;
	unsigned int hallic_pen_irq;
#else
	int hallic_camera_detect_pin;
	unsigned int hallic_camera_irq;
#endif
	unsigned long irq_flags;
};

struct pm8xxx_carkit_platform_data {
	int hallic_carkit_detect_pin;
	unsigned int hallic_carkit_irq;
	unsigned long irq_flags;
};

void cradle_set_deskdock(int state);
int cradle_get_deskdock(void);

#ifdef CONFIG_BU52031NVX_CARKIT
void carkit_set_deskdock(int state);
int carkit_get_deskdock(void);
#endif

#if defined(A1_only)
int get_smartcover_status(void);
#endif

#endif /* __PM8XXX_CRADLE_H__ */
