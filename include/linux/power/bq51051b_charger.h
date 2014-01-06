/*
 * Copyright (C) 2012, Kyungtae Oh <kyungtae.oh@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#ifndef __LINUX_POWER_BQ51051B_CHARGER_H__
#define __LINUX_POWER_BQ51051B_CHARGER_H__

#define BQ51051B_WLC_DEV_NAME "bq51051b_wlc"

#define WLC_DEG

#ifdef WLC_DEG /* todo */
#define WLC_DBG_INFO(fmt, args...) \
	pr_info("wlc: %s: " fmt, __func__, ##args)
#define WLC_DBG(fmt, args...) \
	pr_debug("wlc: %s: " fmt, __func__, ##args)
#else
#define WLC_DBG_INFO(fmt, args...)  do { } while(0)
#define WLC_DBG(fmt, arges...)      do { } while(0)
#endif

void wireless_current_ctl(int batt_temp);

struct bq51051b_wlc_platform_data {
	unsigned int chg_ctrl_gpio;
	unsigned int half_chg_ctrl_gpio;
	unsigned int active_n_gpio;
	unsigned int otg_ctrl_gpio;
	unsigned int wlc_ts_mpp;
	unsigned int track_gpio;
	unsigned int wireless_charging;
};
#endif
