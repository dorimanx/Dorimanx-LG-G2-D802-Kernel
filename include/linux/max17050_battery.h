/*
 *  Copyright (C) 2012 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17050_BATTERY_H_
#define __MAX17050_BATTERY_H_

struct max17050_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
};

#define MAX17050_STATUS				0x00
#define MAX17050_V_ALRT_THRESHOLD	0x01
#define MAX17050_T_ALRT_THRESHOLD	0x02
#define MAX17050_SOC_ALRT_THRESHOLD	0x03
#define MAX17050_AT_RATE			0x04
#define MAX17050_REM_CAP_REP		0x05
#define MAX17050_SOC_REP			0x06
#define MAX17050_AGE				0x07
#define MAX17050_TEMPERATURE		0x08
#define MAX17050_V_CELL				0x09
#define MAX17050_CURRENT			0x0A
#define MAX17050_AVERAGE_CURRENT	0x0B
#define MAX17050_SOC_MIX			0x0D
#define MAX17050_SOC_AV				0x0E
#define MAX17050_REM_CAP_MIX		0x0F
#define MAX17050_FULL_CAP			0x10
#define MAX17050_TTE				0x11
#define MAX17050_Q_RESIDUAL_00		0x12
#define MAX17050_FULL_SOC_THR		0x13
#define MAX17050_AVERAGE_TEMP		0x16
#define MAX17050_CYCLES				0x17
#define MAX17050_DESIGN_CAP			0x18
#define MAX17050_AVERAGE_V_CELL		0x19
#define MAX17050_MAX_MIN_TEMP		0x1A
#define MAX17050_MAX_MIN_VOLTAGE	0x1B
#define MAX17050_MAX_MIN_CURRENT	0x1C
#define MAX17050_CONFIG				0x1D
#define MAX17050_I_CHG_TERM			0x1E
#define MAX17050_REM_CAP_AV			0x1F
#define MAX17050_VERSION			0x21
#define MAX17050_Q_RESIDUAL_10		0x22
#define MAX17050_FULL_CAP_NOM		0x23
#define MAX17050_TEMP_NOM			0x24
#define MAX17050_TEMP_LIM			0x25
#define MAX17050_AIN				0x27
#define MAX17050_LEARN_CFG			0x28
#define MAX17050_FILTER_CFG			0x29
#define MAX17050_RELAX_CFG			0x2A
#define MAX17050_MISC_CFG			0x2B
#define MAX17050_T_GAIN				0x2C
#define MAX17050_T_OFF				0x2D
#define MAX17050_C_GAIN				0x2E
#define MAX17050_C_OFF				0x2F
#define MAX17050_Q_RESIDUAL_20		0x32
#define MAX17050_I_AVG_EMPTY		0x36
#define MAX17050_F_CTC				0x37
#define MAX17050_RCOMP_0			0x38
#define MAX17050_TEMP_CO			0x39
#define MAX17050_V_EMPTY			0x3A
#define MAX17050_F_STAT				0x3D
#define MAX17050_TIMER				0x3E
#define MAX17050_SHDN_TIMER			0x3F
#define MAX17050_Q_RESIDUAL_30		0x42
#define MAX17050_D_QACC				0x45
#define MAX17050_D_PACC				0x46
#define MAX17050_QH					0x4D
#define MAX17050_V_FOCV				0xFB
#define MAX17050_SOC_VF				0xFF
#define BATT_ID         13
#define BATT_ID_PULLUP  14

#ifdef CONFIG_LGE_PM
int max17050_get_battery_capacity_percent(void);
int max17050_get_battery_mvolts(void);
int max17050_suspend_get_mvolts(void);
int max17050_read_battery_age(void);
int max17050_get_battery_age(void);
int max17050_get_battery_current(void);
int max17050_get_soc_for_charging_complete_at_cmd(void);
bool max17050_write_battery_temp(int battery_temp);
bool max17050_write_temp(int battery_temp);
bool max17050_battery_full_info_print(void);
void max17050_initial_quickstart_check(void);
#endif

#endif
