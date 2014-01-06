/*
* Copyright(c) 2013, LGE Inc. All rights reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#ifndef __BQ24192_CHARGER_H__
#define __BQ24192_CHARGER_H__

#define BQ24192_NAME  "bq24192"

struct bq24192_platform_data {
	int int_gpio;
	int chg_current_ma;
	int term_current_ma;
	int vbat_max_mv;
	int pre_chg_current_ma;
	int sys_vmin_mv;
	int vin_limit_mv;
	int icl_vbus_mv;
};

extern int32_t bq24192_is_ready(void);
extern int32_t external_bq24192_enable_charging(bool enable);
extern int bq24192_get_batt_temp_origin(void);
#endif
