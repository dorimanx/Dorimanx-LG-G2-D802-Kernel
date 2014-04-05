/*
 * Copyright (C) 2012, Kyungtae Oh <kyungtae.oh@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#ifndef __LINUX_POWER_BQ51053B_CHARGER_H__
#define __LINUX_POWER_BQ51053B_CHARGER_H__

#define BQ51053B_WLC_DEV_NAME "bq51053bwlc"

struct bq51053b_wlc_platform_data {
	unsigned int wlc_int_gpio;
	unsigned int wlc_full_chg;
	//unsigned int wireless_charging;
	//unsigned int chg_state_gpio;
	//int (*wlc_is_plugged)(void);
};

extern int is_wireless_charger_plugged(void);
extern int wireless_charging_completed(void);

#endif
