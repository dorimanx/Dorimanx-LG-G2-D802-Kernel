/*
 * Copyright (C) 2011 LG Electronics Inc.
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

#ifndef __LGE_BATTERY_ID_H__
#define __LGE_BATTERY_ID_H__

enum {
	BATT_ID_UNKNOWN         = 0,
	BATT_ID_DS2704_N        = 17,
	BATT_ID_DS2704_L        = 32,
	BATT_ID_DS2704_C        = 48,
	BATT_ID_ISL6296_N       = 73,
	BATT_ID_ISL6296_L       = 94,
	BATT_ID_ISL6296_C       = 105,
};

struct lge_battery_id_platform_data {
	uint id;
	uint pullup;
	uint batt_info;
};
#endif  /*                      */

