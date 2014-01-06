/*
 * Copyright (C) 2009 Samsung Electronics
 * Copyright (C) 2012 Nvidia Cooperation
 * Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17048_BATTERY_H_
#define __MAX17048_BATTERY_H_

#ifndef CONFIG_LGE_PM
#include <linux/smb347-charger.h>
#endif

/*#define MAX17048_DEBUG*/
#define CONFIG_MAX17048_SOC_ALERT
#ifdef CONFIG_LGE_PM
/*#define CONFIG_MAX17048_POLLING*/	/*It is just for debugging*/
#if defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_Z_US) || defined(CONFIG_MACH_MSM8974_Z_KDDI)
#define CONFIG_MAX17048_LOW_POLLING
#endif
#endif
#define CONFIG_MAX17048_CUSTOM

#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
enum cell_type{
 LGC_LLL,
 TCD_AAC
};
#define BATT_DS2704_L   32
#define BATT_DS2704_C   48
#define BATT_ISL6296_N  73
#define BATT_ISL6296_L  94
#define BATT_ISL6296_C  105
#endif

struct max17048_battery_model {
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
	int empty;
#endif
#ifdef CONFIG_LGE_PM
	int rcomp;
	int temp_co_hot;
	int temp_co_cold;
	int alert_threshold;
	int alert_gpio;
	int full_design;
#else
	uint8_t rcomp;
	uint8_t soccheck_A;
	uint8_t soccheck_B;
	uint8_t bits;
	uint8_t alert_threshold;
	uint8_t one_percent_alerts;
	uint8_t alert_on_reset;
	uint16_t rcomp_seg;
	uint16_t hibernate;
	uint16_t vreset;
	uint16_t valert;
	uint16_t ocvtest;
#endif
};

struct max17048_platform_data {
	int (*battery_online)(void);
	int (*charging_status)(void);
	int (*charger_online)(void);
};

#ifdef CONFIG_LGE_PM
extern int max17048_get_voltage(void);
extern int max17048_get_capacity(void);
extern int max17048_get_fulldesign(void);
#endif
#endif
