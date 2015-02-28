/*
 * Fuel gauge driver for Maxim 17050 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2012 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/max17050_battery.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>

#include <linux/delay.h>

/*#include <mach/board_lge.h>*/
#include <linux/module.h>
/*#include "../../lge/include/lg_backup_items.h"*/
#include <mach/msm_smsm.h>
#ifdef CONFIG_LGE_PM
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/qpnp/qpnp-charger.h>
#include <linux/i2c/smb349_charger.h>
#include <mach/board_lge.h>
#endif

#define MAX17050_FULL_DEBUG
/*#define MAX17050_LIGHT_DEBUG
#define MAX17050_CORE_PRINT*/


#ifdef MAX17050_FULL_DEBUG
#define F_bat(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define F_bat(fmt, args...) do {} while (0)
#endif

#ifdef MAX17050_LIGHT_DEBUG
#define L_bat(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define L_bat(fmt, args...) do {} while (0)
#endif

#ifdef MAX17050_CORE_PRINT
#define C_bat(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define C_bat(fmt, args...) do {} while (0)
#endif


static struct i2c_client *max17050_i2c_client;

u16 pre_soc = 100;
u16 real_soc = 100;

struct max17050_chip {
	struct i2c_client *client;
	/*struct power_supply battery;*/
	struct max17050_platform_data *pdata;
};

/*                                                                   */
int lge_power_test_flag_max17050 = 1;
/*                                                                   */

/*static int max17050_access_control_of_flash(void);
bool max17050_count_control(u8 select, u8 count_up);
int max17050_save_bat_info_check(void);*/

static int max17050_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17050_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17050_multi_write_data(struct i2c_client *client,
			int reg, const u8 *values, int length)
{
		int ret;

		ret = i2c_smbus_write_i2c_block_data(client,
				reg, length, values);

		if (ret < 0)
				dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);

		return ret;
}

static int max17050_multi_read_data(struct i2c_client *client,
			int reg, u8 *values, int length)
{
		int ret;

		ret = i2c_smbus_read_i2c_block_data(client,
				reg, length, values);

		if (ret < 0)
				dev_err(&client->dev, "%s: err %d\n",
						__func__, ret);

		return ret;
}


int max17050_get_mvolts(void)
{
	u16 read_reg;
	int vbatt_mv;

	/*if (max17050_nobattery)
		return 3950;*/
	if (max17050_i2c_client == NULL) {
		pr_info("[MAX17050] %s: i2c NULL vbatt = 800 mV\n", __func__);
		return 800;
	}
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_V_CELL);
	if (read_reg < 0)
		return 800;

	vbatt_mv = (read_reg >> 3);
	vbatt_mv = (vbatt_mv * 625) / 1000;
#ifdef MAX17050_DEBUG
	pr_debug("%s: vbatt = %d mV\n", __func__, vbatt_mv);
#endif
	return vbatt_mv;

}

int max17050_suspend_get_mvolts(void)
{
	u16 read_reg;
	int vbatt_mv;

	/*if (max17050_nobattery)
		return 3950;*/
	if (max17050_i2c_client == NULL) {
		pr_info("[MAX17050] %s: i2c NULL vbatt = 3950 mV\n", __func__);
		return 800;
	}

	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_V_CELL);
	if (read_reg < 0)
		return 800;

	vbatt_mv = (read_reg >> 3);
	vbatt_mv = (vbatt_mv * 625) / 1000;

	pr_debug("%s: vbatt = %d mV\n", __func__, vbatt_mv);

	return vbatt_mv;

}


int max17050_get_capacity_percent(void)
{
	int battery_soc = 0;
	int read_reg = 0;

	u8 upper_reg;
	u8 lower_reg;
	if (max17050_i2c_client == NULL) {
		return 80;
	} else {
		read_reg = max17050_read_reg(max17050_i2c_client,
			MAX17050_SOC_REP);

		if (read_reg < 0) {
			pr_err("[MAX17050] %s: i2c Read Fail battery SOC = %d\n",
				__func__, pre_soc);
			return pre_soc;
		}
		upper_reg = (read_reg & 0xFF00)>>8 ;
		lower_reg = (read_reg & 0xFF);
#ifdef MAX17050_DEBUG
		F_bat("%s: read_reg = %X  upper_reg = %X lower_reg = %X\n",
			__func__, read_reg, upper_reg, lower_reg);
#endif
		/* SOC scaling for stable max SOC and changed Cut-off */
		/*Adj SOC = (FG SOC-Emply)/(Full-Empty)*100*/
		/* cut off vol 3.48V : (soc - 1.132%)*100/(94.28% - 1.132%) */
		/* full capacity soc 106.5% , real battery SoC 100.7%*/
		battery_soc = ((upper_reg * 256)+lower_reg)*10/256;
#ifdef MAX17050_DEBUG
		F_bat("%s: battery_soc  = %d\n", __func__, battery_soc);
#endif
		battery_soc = (battery_soc * 100) * 100;
		battery_soc = (battery_soc / 9440) - 1; /* 100 -> 105.8% scailing */

		if (((battery_soc/10) < 1) && ((battery_soc%10) >= 5)) {
			battery_soc = 10;
		}

		battery_soc /= 10;
#ifdef MAX17050_DEBUG
		pr_debug("%s: battery_soc  = %d (upper_reg = %d lower_reg = %d)\n",
			__func__, battery_soc, upper_reg, lower_reg);
#endif
		real_soc = battery_soc;

		if (battery_soc >= 100)
			battery_soc = 100;

		if (battery_soc < 0)
			battery_soc = 0;

	}
		pre_soc = battery_soc;

		return battery_soc;

}

int max17050_get_current(void)
{
	u16 read_reg;
	int ibatt_ma;
	int avg_ibatt_ma;
	u16 sign_bit;

	if (max17050_i2c_client == NULL) {
		pr_info("[MAX17050] %s: i2c NULL", __func__);
		return 999;
	}
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_CURRENT);
	if (read_reg < 0)
		return 999; /*Error Value return.*/

	sign_bit = (read_reg & 0x8000)>>15;

	if (sign_bit == 1)
		ibatt_ma = (15625 * (read_reg  - 65536))/100000;
	else
		ibatt_ma = (15625 * read_reg) / 100000;

	read_reg = max17050_read_reg(max17050_i2c_client,
		MAX17050_AVERAGE_CURRENT);
	if (read_reg < 0)
		return 999;/*Error Value return.*/

	sign_bit = (read_reg & 0x8000)>>15;

	if (sign_bit == 1)
		avg_ibatt_ma = (15625 * (read_reg  - 65536)) / 100000;
	else
		avg_ibatt_ma = (15625 * read_reg) / 100000;
#ifdef MAX17050_DEBUG
	pr_debug("%s: I_batt = %d mA avg_I_batt = %d mA\n",
		__func__, ibatt_ma, avg_ibatt_ma);
#endif
	return avg_ibatt_ma;

}

bool max17050_write_temp(int battery_temp)
{
	u16 ret;
	u16 write_temp;

	battery_temp = battery_temp/10;

	if (battery_temp < 0)
		write_temp = (battery_temp + 256)<<8;
	else
		write_temp = (battery_temp)<<8;

	pr_debug("max17050_write_temp   - battery_temp (%d)\n", battery_temp);

	ret = max17050_write_reg(max17050_i2c_client,
		MAX17050_TEMPERATURE, write_temp);

	if (ret < 0) {
		pr_debug("max17050_write_temp error.\n");
		return 1;
	}

	return 0;
}

int max17050_read_battery_age(void)
{
	u16 read_reg;
	int battery_age;

	if (max17050_i2c_client == NULL) {
		pr_debug("MAX17050] %s: i2c NULL battery age: 800\n", __func__);
		return 800;
	}
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_AGE);
	if (read_reg < 0)
		return 999; /*Error Value return.*/

	battery_age = (read_reg >> 8);
#ifdef MAX17050_DEBUG
	F_bat("%s: battery_age = %d\n", __func__, battery_age);
#endif
	return battery_age;
}

bool max17050_battery_full_info_print(void)
{
	u16 read_reg;
	int battery_age;
	int battery_remain_capacity;
	int battery_time_to_empty_sec;
	int battery_soc;
	int battery_voltage;
	int battery_temp;
	int battery_current;
	int battery_full_cap;

	battery_age = max17050_read_battery_age();

	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_REM_CAP_REP);

	battery_remain_capacity = (5 * read_reg)/10;

	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_FULL_CAP);

	battery_full_cap = (5 * read_reg)/10;

	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_TTE);

	battery_time_to_empty_sec = (5625 * read_reg)/1000;

	battery_soc = max17050_get_capacity_percent();

	battery_voltage = max17050_get_mvolts();

	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_TEMPERATURE);

	battery_temp = (read_reg >> 8);

	if (battery_temp > 127)
		battery_temp = battery_temp - 256;
	else
		battery_temp = battery_temp;

	battery_current = max17050_get_current();

	pr_debug("* -- max17050 battery full info print ---------- *\n");
	pr_debug("battery age = %d %%    remain capacity = %d mAh\n",
		battery_age, battery_remain_capacity);
	pr_debug("battery current = %d mA     Time to Empty = %d min\n",
		battery_current, battery_time_to_empty_sec/60);
	pr_debug("battery SOC = %d %%    Voltage = %d mV\n",
		battery_soc, battery_voltage);
	pr_debug("battery TEMP = %d C   full capacity = %d mAh\n",
		battery_temp, battery_full_cap);
	pr_debug("* ---------------------------------------------- *\n");

	return 0;
}

bool max17050_i2c_write_and_verify(u8 addr, u16 value)
{
	u16 read_reg;

	max17050_write_reg(max17050_i2c_client, addr, value);

	read_reg = max17050_read_reg(max17050_i2c_client, addr);

	if (read_reg == value) {
			F_bat("[MAX17050] %s() Addr = 0x%X,", __func__, addr);
			F_bat(" Value = 0x%X Success\n", value);
			return 1;
	} else {
		pr_debug("[MAX17050] %s() Addr = 0x%X,", __func__, addr);
		pr_debug(" Value = 0x%X Fail to write.", value);
		pr_debug(" Write once more.\n");
			max17050_write_reg(max17050_i2c_client, addr, value);
			return 0;
	}

	return 1;

}


static int max17050_new_custom_model_write(void)
{
	/*u16 ret;*/
	u16 read_reg;
	/*u16 write_reg;*/
	u16 vfsoc;
	u16 full_cap_0;
	u16 rem_cap;
	u16 rep_cap;
	u16 dQ_acc;
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	u16 capacity = 0x17B6;
#else
	u16 capacity = 0x1802;
#endif

	u16 i;

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	u8 custom_model_1[32] = {
		0xC0, 0x9C, 0xC0, 0xA5, 0xF0, 0xAD, 0x30, 0xB6,
		0x90, 0xB9, 0x30, 0xBC, 0x30, 0xBD, 0xB0, 0xBD,
		0xC0, 0xBE, 0xF0, 0xBF, 0x30, 0xC2, 0x70, 0xC7,
		0x40, 0xCA, 0x00, 0xCD, 0x60, 0xD2, 0x00, 0xD8,
		};

	u8 custom_model_2[32] = {
		0x90, 0x00, 0xA0, 0x01, 0xA0, 0x01, 0x00, 0x0D,
		0x00, 0x0E, 0x10, 0x19, 0x20, 0x32, 0x70, 0x11,
		0xC0, 0x10, 0xD0, 0x0C, 0xF0, 0x08, 0xA0, 0x08,
		0xF0, 0x08, 0xF0, 0x06, 0xF0, 0x06, 0xF0, 0x06,
		};
#else
	u8 custom_model_1[32] = {
		0x20, 0xAC, 0x40, 0xAD, 0xF0, 0xB1, 0xE0, 0xB8,
		0x60, 0xBA, 0x90, 0xBB, 0x20, 0xBD, 0x00, 0xBE,
		0xD0, 0xBE, 0xB0, 0xBF, 0x60, 0xC1, 0xD0, 0xC5,
		0xE0, 0xCB, 0xC0, 0xD1, 0xC0, 0xD4, 0x20, 0xD7,
		};

	u8 custom_model_2[32] = {
		0xB0, 0x00, 0x30, 0x06, 0x00, 0x04, 0x70, 0x18,
		0x30, 0x0F, 0xB0, 0x17, 0x00, 0x18, 0x10, 0x22,
		0xA0, 0x0A, 0x90, 0x0D, 0xC0, 0x09, 0xF0, 0x07,
		0xF0, 0x06, 0xA0, 0x09, 0xE0, 0x04, 0xE0, 0x04,
		};
#endif

	u8 custom_model_3[32] = {
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		};

	u8 read_custom_model_1[32];
	u8 read_custom_model_2[32];
	u8 read_custom_model_3[32];

	F_bat("[MAX17050] %s()  Start\n", __func__);

	/*0. Check for POR or Battery Insertion*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_STATUS);
	F_bat("[MAX17050] %s()  MAX17050_STATUS = 0x%X\n", __func__, read_reg);
	if (read_reg == 0) {
		pr_debug("[MAX17050] IC Non-Power-On-Reset state.");
		pr_debug(" Go to max17050_save_bat_info_to_flash.\n");
		return 2; /*Go to save the custom model.*/
	}
	else
		pr_debug("[MAX17050] IC Power On Reset state. Start Custom Model Write.\n");

	/*Skip if the cable is Factory Mode.*/
/*	if ((usb_cable_info == 6) ||
		(usb_cable_info == 7) || (usb_cable_info == 11)) {
		pr_debug("[MAX17050] IC Power-On-Reset state.
		But Skip Custom model in Factory Mode.\n");
		return 2;
	} */

	/*1. Delay 500mS*/
	msleep(500);

	/*1.1 Version Check*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_VERSION);
	F_bat("[MAX17050] %s()  MAX17050_VERSION = 0x%X\n", __func__, read_reg);
	if (read_reg != 0xAC) {
		pr_debug("[MAX17050]  Version Check Error.");
		pr_debug(" Version Check = 0x%x\n", read_reg);
		return 1; /*Version Check Error*/
	}

	/*2. Initialize Configuration*/
	max17050_write_reg(max17050_i2c_client, MAX17050_RELAX_CFG, 0x506B);
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	/* External temp and enable alert function */
	max17050_write_reg(max17050_i2c_client, MAX17050_CONFIG, 0x2104);
#else
	/* External temp */
	max17050_write_reg(max17050_i2c_client, MAX17050_CONFIG, 0x2100);
#endif
	max17050_write_reg(max17050_i2c_client, MAX17050_FILTER_CFG, 0x87A4);
	max17050_write_reg(max17050_i2c_client, MAX17050_LEARN_CFG, 0x2607);
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, 0x0870);
	max17050_write_reg(max17050_i2c_client, MAX17050_FULL_SOC_THR, 0x5F00);

	/*4. Unlock Model Access*/
	max17050_write_reg(max17050_i2c_client, 0x62, 0x0059);
	max17050_write_reg(max17050_i2c_client, 0x63, 0x00C4);

	/*5. Write/Read/Verify the Custom Model*/
	max17050_multi_write_data(max17050_i2c_client,
				0x80, &custom_model_1[0], 32);
	max17050_multi_write_data(max17050_i2c_client,
				0x90, &custom_model_2[0], 32);
	max17050_multi_write_data(max17050_i2c_client,
				0xA0, &custom_model_3[0], 32);

	/*For test only. Read back written-custom model data.*/
	max17050_multi_read_data(max17050_i2c_client,
				0x80, &read_custom_model_1[0], 32);
	max17050_multi_read_data(max17050_i2c_client,
				0x90, &read_custom_model_2[0], 32);
	max17050_multi_read_data(max17050_i2c_client,
				0xA0, &read_custom_model_3[0], 32);

	/*Compare with original one.*/
	for (i = 0 ; i < 32 ; i++) {
		if (read_custom_model_1[i] != custom_model_1[i]) {
			pr_debug("[MAX17050] Custom Model");
			pr_debug(" 1[%d]	Write Error\n", i);
		}
	}

	for (i = 0 ; i < 32 ; i++) {
		if (read_custom_model_2[i] != custom_model_2[i]) {
			pr_debug("[MAX17050] Custom Model");
			pr_debug(" 2[%d]	Write Error\n", i);
		}
	}

	for (i = 0 ; i < 32 ; i++) {
		if (read_custom_model_3[i] != custom_model_3[i]) {
			pr_debug("[MAX17050] Custom Model");
			pr_debug(" 3[%d] Write Error\n", i);
		}
	}
	/*For Test only end.*/

	/*8. Lock Model Access*/
	max17050_write_reg(max17050_i2c_client, 0x62, 0x0000);
	max17050_write_reg(max17050_i2c_client, 0x63, 0x0000);

	/*9. Verify the Model Access is locked.*/
	/*Skip.*/

	/*10. Write Custom Parameters*/
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	max17050_i2c_write_and_verify(MAX17050_RCOMP_0, 0x0048);
	max17050_i2c_write_and_verify(MAX17050_TEMP_CO, 0x1D2A);
	max17050_i2c_write_and_verify(MAX17050_TEMP_NOM, 0x1400);
	max17050_write_reg(max17050_i2c_client, MAX17050_T_GAIN, 0xE932);
	max17050_write_reg(max17050_i2c_client, MAX17050_T_OFF, 0x2381);

	/*Termination Current 400mA*/
	max17050_write_reg(max17050_i2c_client, MAX17050_I_CHG_TERM, 0x0280);
	max17050_i2c_write_and_verify(MAX17050_V_EMPTY, 0xA2DA);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_00, 0x2602);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_10, 0x1A82);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_20, 0x0A04);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_30, 0x0804);
#else
	max17050_i2c_write_and_verify(MAX17050_RCOMP_0, 0x0066);
	max17050_i2c_write_and_verify(MAX17050_TEMP_CO, 0x2436);
	max17050_i2c_write_and_verify(MAX17050_TEMP_NOM, 0x1400);
	max17050_write_reg(max17050_i2c_client, MAX17050_T_GAIN, 0xE932);
	max17050_write_reg(max17050_i2c_client, MAX17050_T_OFF, 0x2381);

	/*Termination Current 150mA*/
	max17050_write_reg(max17050_i2c_client, MAX17050_I_CHG_TERM, 0x03C0);
	max17050_i2c_write_and_verify(MAX17050_V_EMPTY, 0xACDA);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_00, 0x8400);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_10, 0x5A84);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_20, 0x270A);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_30, 0x198A);
#endif

	/*11. Update Full Capacity Parameters*/
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP, capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP, capacity);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM, capacity);

	/*13. Delay at least 350mS*/
	msleep(360);

	/*14. Write VFSOC value to VFSOC 0*/
	vfsoc = max17050_read_reg(max17050_i2c_client, MAX17050_SOC_VF);
	F_bat("[MAX17050] %s()  vfsoc = 0x%X\n", __func__, vfsoc);
	max17050_write_reg(max17050_i2c_client, 0x60, 0x0080);
	max17050_i2c_write_and_verify(0x48, vfsoc);
	max17050_write_reg(max17050_i2c_client, 0x60, 0x0000);

	/*15. Write temperature (default 20 deg C)*/
	max17050_i2c_write_and_verify(MAX17050_TEMPERATURE, 0x1400);

	/*16. Load New Capacity Parameters*/
	full_cap_0 =  max17050_read_reg(max17050_i2c_client, 0x35);
	F_bat("[MAX17050] %s()  full_cap_0 = %d  = 0x%X\n",
		__func__, full_cap_0, full_cap_0);
	rem_cap = (vfsoc * full_cap_0) / 25600;
	F_bat("[MAX17050] %s()  rem_cap = %d  = 0x%X\n",
		__func__, rem_cap, rem_cap);
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_MIX, rem_cap);
	rep_cap = rem_cap;
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP, rep_cap);
	dQ_acc = (capacity / 4);
	max17050_i2c_write_and_verify(MAX17050_D_QACC, dQ_acc);
	max17050_i2c_write_and_verify(MAX17050_D_PACC, 0x3200);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP, capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP, capacity);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM, capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_SOC_REP, vfsoc);

	/*17. Initialization Complete*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_STATUS);
	max17050_i2c_write_and_verify(MAX17050_STATUS, (read_reg & 0xFFFD));

	/*End of the Custom Model step 1.*/
	pr_debug("[MAX17050] End of the max17050_new_custom_model_write.\n");

	F_bat("[MAX17050] %s()  End\n", __func__);
	return 0; /*Success to write.*/

}

static int max17050_force_custom_model_write(void)
{
	/*u16 ret;*/
	u16 read_reg;
	/*u16 write_reg;*/
	u16 vfsoc;
	u16 full_cap_0;
	u16 rem_cap;
	u16 rep_cap;
	u16 dQ_acc;
	u16 capacity = 0x1770;

	u16 i;

	u8 custom_model_1[32] = {
		0x30, 0x7F, 0x10, 0xB6, 0xD0, 0xB7, 0xA0, 0xB9,
		0xC0, 0xBB, 0xA0, 0xBC, 0xE0, 0xBC, 0x30, 0xBD,
		0x90, 0xBD, 0xD0, 0xBE, 0xE0, 0xC1, 0xF0, 0xC3,
		0x10, 0xC6, 0x30, 0xC9, 0x60, 0xCC, 0x90, 0xCF,
		};

	u8 custom_model_2[32] = {
		0xE0, 0x00, 0x70, 0x0E, 0xF0, 0x0D, 0x00, 0x0F,
		0x20, 0x15, 0xF0, 0x46, 0xC0, 0x38, 0x70, 0x19,
		0x00, 0x18, 0x60, 0x0C, 0x20, 0x0D, 0xC0, 0x0C,
		0xE0, 0x07, 0xF0, 0x08, 0xF0, 0x08, 0xF0, 0x08,
		};

	u8 custom_model_3[32] = {
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
		};

	u8 read_custom_model_1[32];
	u8 read_custom_model_2[32];
	u8 read_custom_model_3[32];

	F_bat("[MAX17050] %s()  Start\n", __func__);

	/*0. No Check for POR or Battery Insertion*/
	pr_debug("[MAX17050]Start Force Custom Model Write.\n");

	/*Skip if the cable is Factory Mode.*/
/*	if ((usb_cable_info == 6) ||
		(usb_cable_info == 7) || (usb_cable_info == 11)) {
		pr_debug("[MAX17050] Force Custom Write.
			But Skip Custom model in Factory Mode.\n");
		return 2;
	}*/

	/*1. Delay 500mS*/
	msleep(500);

	/*1.1 Version Check*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_VERSION);
	F_bat("[MAX17050] %s()  MAX17050_VERSION = 0x%X\n", __func__, read_reg);
	/*if (read_reg != 0xAC)
		{
			pr_debug("[MAX17050]  Version Check Error.\n");
			return 1; //Version Check Error
		}*/

	/*2. Initialize Configuration*/
	max17050_write_reg(max17050_i2c_client, MAX17050_RELAX_CFG, 0x506B);
	max17050_write_reg(max17050_i2c_client, MAX17050_CONFIG, 0x2100);
	max17050_write_reg(max17050_i2c_client, MAX17050_FILTER_CFG, 0x87A4);
	max17050_write_reg(max17050_i2c_client, MAX17050_LEARN_CFG, 0x2607);
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, 0x0870);
	max17050_write_reg(max17050_i2c_client, MAX17050_FULL_SOC_THR, 0x5F00);

	/*4. Unlock Model Access*/
	max17050_write_reg(max17050_i2c_client, 0x62, 0x0059);
	max17050_write_reg(max17050_i2c_client, 0x63, 0x00C4);

	/*5. Write/Read/Verify the Custom Model*/
	max17050_multi_write_data(max17050_i2c_client,
	0x80, &custom_model_1[0], 32);
	max17050_multi_write_data(max17050_i2c_client,
		0x90, &custom_model_2[0], 32);
	max17050_multi_write_data(max17050_i2c_client,
		0xA0, &custom_model_3[0], 32);

	/*For test only. Read back written-custom model data.*/
	max17050_multi_read_data(max17050_i2c_client,
	0x80, &read_custom_model_1[0], 32);
	max17050_multi_read_data(max17050_i2c_client,
		0x90, &read_custom_model_2[0], 32);
	max17050_multi_read_data(max17050_i2c_client,
		0xA0, &read_custom_model_3[0], 32);

	/*Compare with original one.*/
	for (i = 0 ; i < 32 ; i++) {
			if (read_custom_model_1[i] != custom_model_1[i]) {
				pr_debug("[MAX17050] Custom Model");
				pr_debug(" 1[%d]	Write Error\n", i);
			}
	}

	for (i = 0 ; i < 32 ; i++) {
			if (read_custom_model_2[i] != custom_model_2[i]) {
				pr_debug("[MAX17050] Custom Model");
				pr_debug(" 2[%d]	Write Error\n", i);
			}
	}

	for (i = 0 ; i < 32 ; i++) {
			if (read_custom_model_3[i] != custom_model_3[i]) {
				pr_debug("[MAX17050] Custom Model");
				pr_debug(" 3[%d]	Write Error\n", i);
			}
	}
	/*For Test only end.*/

	/*8. Lock Model Access*/
	max17050_write_reg(max17050_i2c_client, 0x62, 0x0000);
	max17050_write_reg(max17050_i2c_client, 0x63, 0x0000);

	/*9. Verify the Model Access is locked.*/
	/*Skip.*/

	/*10. Write Custom Parameters*/
	max17050_i2c_write_and_verify(MAX17050_RCOMP_0, 0x0058);
	max17050_i2c_write_and_verify(MAX17050_TEMP_CO, 0x2230);
	max17050_i2c_write_and_verify(MAX17050_TEMP_NOM, 0x1400);
	max17050_write_reg(max17050_i2c_client, MAX17050_T_GAIN, 0xE932);
	max17050_write_reg(max17050_i2c_client, MAX17050_T_OFF, 0x2381);

	/*Termination Current 150mA*/
	max17050_write_reg(max17050_i2c_client, MAX17050_I_CHG_TERM, 0x03C0);
	max17050_i2c_write_and_verify(MAX17050_V_EMPTY, 0xACDA);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_00, 0x9680);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_10, 0x3800);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_20, 0x3805);
	max17050_i2c_write_and_verify(MAX17050_Q_RESIDUAL_30, 0x2513);

	/*11. Update Full Capacity Parameters*/
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP, capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP, capacity);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM, capacity);

	/*13. Delay at least 350mS*/
	msleep(360);

	/*14. Write VFSOC value to VFSOC 0*/
	vfsoc = max17050_read_reg(max17050_i2c_client, MAX17050_SOC_VF);
	F_bat("[MAX17050] %s()  vfsoc = 0x%X\n", __func__, vfsoc);
	max17050_write_reg(max17050_i2c_client, 0x60, 0x0080);
	max17050_i2c_write_and_verify(0x48, vfsoc);
	max17050_write_reg(max17050_i2c_client, 0x60, 0x0000);

	/*15. Write temperature (default 20 deg C)*/
	max17050_i2c_write_and_verify(MAX17050_TEMPERATURE, 0x1400);

	/*16. Load New Capacity Parameters*/
	full_cap_0 =  max17050_read_reg(max17050_i2c_client, 0x35);
	F_bat("[MAX17050] %s()  full_cap_0 = %d  = 0x%X\n",
		__func__, full_cap_0, full_cap_0);
	rem_cap = (vfsoc * full_cap_0) / 25600;
	F_bat("[MAX17050] %s()  rem_cap = %d  = 0x%X\n",
		__func__, rem_cap, rem_cap);
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_MIX, rem_cap);
	rep_cap = rem_cap;
	max17050_i2c_write_and_verify(MAX17050_REM_CAP_REP, rep_cap);
	dQ_acc = (capacity / 4);
	max17050_i2c_write_and_verify(MAX17050_D_QACC, dQ_acc);
	max17050_i2c_write_and_verify(MAX17050_D_PACC, 0x3200);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP, capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_DESIGN_CAP, capacity);
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP_NOM, capacity);
	max17050_write_reg(max17050_i2c_client, MAX17050_SOC_REP, vfsoc);

	/*17. Initialization Complete*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_STATUS);
	max17050_i2c_write_and_verify(MAX17050_STATUS, (read_reg & 0xFFFD));

	/*End of the Custom Model step 1.*/
	pr_debug("[MAX17050] End of the max17050_new_custom_model_write.\n");

	F_bat("[MAX17050] %s()  End\n", __func__);
	return 0; /*Success to write.*/

}

int max17050_battery_exchange_program(void)
{
	int ret = 0;

/*                                          */
	{
		/*Call max17050_new_custom_model_write*/
		ret = max17050_force_custom_model_write();

		if (ret == 2)
			pr_debug("NON-POWER_ON reset. Proceed Booting.\n");
		/*Error occurred. Write once more.*/
		else if (ret == 1)
			max17050_force_custom_model_write();
		/*New Custom model write End.
		Go to max17050_restore_bat_info_from_flash.*/
		else if (ret == 0)
			pr_debug("POWER_ON reset. Custom Model Success.\n");
	}

return 0;

}

bool max17050_quick_start(void)
{
	u16 read_reg;
	u16 write_reg;
	u16 check_reg;
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	u16 capacity = 0x17B6;
#else
	u16 capacity = 0x1802;
#endif

	/*1. Set the QuickStart and Verify bits*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	write_reg = read_reg | 0x1400;
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, write_reg);

	/*2. Verify no memory leaks during Quickstart writing*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	check_reg = read_reg & 0x1000;
	if (check_reg != 0x1000) {
		pr_debug(" [MAX17050] quick_start error !!!!\n");
		return 1;
	}

	/*3. Clean the Verify bit*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	write_reg = read_reg & 0xefff;
	max17050_write_reg(max17050_i2c_client, MAX17050_MISC_CFG, write_reg);

	/*4. Verify no memory leaks during Verify bit clearing*/
	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_MISC_CFG);
	check_reg = read_reg & 0x1000;
	if (check_reg != 0x0000) {
		pr_debug(" [MAX17050] quick_start error !!!!\n");
		return 1;
	}
	/*5. Delay 500ms*/
	msleep(500);

	/*6. Writing and Verify FullCAP Register Value*/
	max17050_i2c_write_and_verify(MAX17050_FULL_CAP, capacity);

	/*7. Delay 500ms*/
	msleep(500);

	return 0;

}

int max17050_get_soc_for_charging_complete_at_cmd()
{

	int guage_level = 0;

/*	pm8921_charger_enable(0);
	pm8921_disable_source_current(1);*/

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	/* Reduce charger source */
	external_smb349_enable_charging(0);
#endif

	pr_debug(" [AT_CMD][at_fuel_guage_level_show] max17050_quick_start\n");
	max17050_quick_start();
	guage_level = max17050_get_capacity_percent();
	if (guage_level != 0) {
		if (guage_level >= 80)
			guage_level = (real_soc * 962) / 1000;
		else
			guage_level = (guage_level * 100) / 100;
	}

	if (guage_level > 100)
		guage_level = 100;
	else if (guage_level < 0)
		guage_level = 0;

/*	pm8921_disable_source_current(0);
	pm8921_charger_enable(1);*/

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	/* Restore charger source */
	external_smb349_enable_charging(1);
#endif

	pr_debug(" [AT_CMD][at_fuel_guage_soc_for_charging_complete]");
	pr_debug(" BATT guage_level = %d ,", guage_level);
	pr_debug(" real_soc = %d\n", real_soc);

	return guage_level;
}
EXPORT_SYMBOL(max17050_get_soc_for_charging_complete_at_cmd);

int max17050_get_battery_mvolts(void)
{
	return max17050_get_mvolts();
}
EXPORT_SYMBOL(max17050_get_battery_mvolts);

u8 at_cmd_buf[2] = {0xff, 0xff};
int max17050_get_battery_capacity_percent(void)
{
	if (at_cmd_buf[0] == 1)
		return at_cmd_buf[1];

	return max17050_get_capacity_percent();
}
EXPORT_SYMBOL(max17050_get_battery_capacity_percent);

int max17050_get_battery_current(void)
{
	return max17050_get_current();
}
EXPORT_SYMBOL(max17050_get_battery_current);

bool max17050_write_battery_temp(int battery_temp)
{
	return max17050_write_temp(battery_temp);
}
EXPORT_SYMBOL(max17050_write_battery_temp);


int max17050_get_battery_age(void)
{
	return max17050_read_battery_age();
}
EXPORT_SYMBOL(max17050_get_battery_age);

/* For max17050 AT cmd */
bool max17050_set_battery_atcmd(int flag, int value)
{
	bool ret;

	u16 soc_read;
	u16 vbat_mv;

	if (flag == 0) {
		/*Call max17050 Quick Start function.*/
		ret = max17050_quick_start();

		if (ret == 0) {
			/*Read and Verify Outputs*/
			soc_read = max17050_get_capacity_percent();
			vbat_mv = max17050_suspend_get_mvolts();
			pr_debug("[MAX17050] max17050_quick_start end");
			pr_debug(" Reset_SOC = %d %% vbat = %d mV\n",
				soc_read, vbat_mv);

			if ((vbat_mv >= 4100) && (soc_read < 91)) {
				at_cmd_buf[0] = 1;
				at_cmd_buf[1] = 100;
				pr_debug("[MAX17050] max17050_quick_start error correction works.\n");
				return 1;
			} else
				at_cmd_buf[0] = 0;
		} else {
				at_cmd_buf[0] = 1;
				at_cmd_buf[1] = 100;
				pr_debug("[MAX17050] max17050_quick_start error correction works.\n");
				return 1;
		}
	} else if (flag == 1) {
		at_cmd_buf[0] = 0;
	}

	return 0;
}

static ssize_t at_fuel_guage_reset_show
	(struct device *dev, struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool ret = 0;

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	/* Reduce charger source */
	external_smb349_enable_charging(0);
#endif

	ret = max17050_set_battery_atcmd(0, 100);  /* Reset the fuel guage IC*/
	if (ret == 1)
		pr_debug("at_fuel_guage_reset_show error.\n");

	r = snprintf(buf, PAGE_SIZE, "%d\n", true);
	/*at_cmd_force_control = TRUE;*/

	msleep(100);

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	/* Restore charger source */
	external_smb349_enable_charging(1);
#endif

	return r;
}

static ssize_t at_fuel_guage_level_show
	(struct device *dev, struct device_attribute *attr, char *buf)
{
	int r = 0;
	int guage_level = 0;

	/*                                                                */
	if (lge_power_test_flag_max17050 == 1) {
		/*pm8921_charger_enable(0);
		pm8921_disable_source_current(1);*/

		pr_debug(" [AT_CMD][at_fuel_guage_level_show]");
		pr_debug(" max17050_quick_start\n");

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
		/* Reduce charger source */
		external_smb349_enable_charging(0);
#endif

		max17050_quick_start();
		guage_level = max17050_get_capacity_percent();
		if (guage_level != 0) {
			if (guage_level >= 80)
				guage_level = (real_soc * 962) / 1000;
			else
				guage_level = (guage_level * 100) / 100;
		}

		if (guage_level > 100)
			guage_level = 100;
		else if (guage_level < 0)
			guage_level = 0;

		pr_debug(" [AT_CMD][at_fuel_guage_level_show]");
		pr_debug(" BATT guage_level = %d ,", guage_level);
		pr_debug(" real_soc = %d\n", real_soc);

		/*pm8921_disable_source_current(0);
		pm8921_charger_enable(1);*/

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
		/* Restore charger source */
		external_smb349_enable_charging(1);
#endif

		return snprintf(buf, PAGE_SIZE, "%d\n", guage_level);
	}
	/*                                                                */
	guage_level = max17050_get_capacity_percent();
	pr_debug(" [AT_CMD][at_fuel_guage_level_show]");
	pr_debug(" not quick start BATT guage_level = %d\n", guage_level);
	r = snprintf(buf, PAGE_SIZE, "%d\n", guage_level);

	return r;
}

static ssize_t at_batt_level_show
	(struct device *dev, struct device_attribute *attr, char *buf)
{
	int r = 0;
	int battery_level = 0;


	/*                                                                */
	if (lge_power_test_flag_max17050 == 1) {
		/*pm8921_charger_enable(0);
		pm8921_disable_source_current(1);*/

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
		/* Reduce charger source */
		external_smb349_enable_charging(0);
#endif

		pr_debug(" [AT_CMD][at_batt_level_show] max17050_quick_start\n");

		max17050_quick_start();
		battery_level =  max17050_get_battery_mvolts();

		/*pm8921_disable_source_current(0);
		pm8921_charger_enable(1);*/

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
		/* Restore charger source */
		external_smb349_enable_charging(1);
#endif

		pr_debug(" [AT_CMD][at_batt_level_show] BATT LVL = %d\n",
				battery_level);

		return snprintf(buf, PAGE_SIZE, "%d\n", battery_level);
	}
	/*                                                                */

	battery_level =  max17050_get_battery_mvolts();
	pr_debug(" [AT_CMD][at_batt_level_show]");
	pr_debug("	not quick start BATT LVL = %d\n", battery_level);

	r = snprintf(buf, PAGE_SIZE, "%d\n", battery_level);

	return r;
}

DEVICE_ATTR(at_fuelrst, 0644, at_fuel_guage_reset_show, NULL);
DEVICE_ATTR(at_fuelval, 0644, at_fuel_guage_level_show, NULL);
DEVICE_ATTR(at_batl, 0644, at_batt_level_show, NULL);

static int  max17050_quickstart_check_chglogo_boot
	(u16 v_cell, u16 v_focv)
{
	u16 result;
	int ret = 0;

	result = v_focv - v_cell;
	pr_info("[MAX17050]chglogoboot-result(%d),v_focv(%d),vcell(%d)\n", result, v_focv, v_cell);

	if(result >= 25 && result <= 100)
	{
		pr_info("[MAX17050] - skip quickstart\n");
	}
	else
	{
		pr_info("[MAX17050] - quick_start !!\n");
		ret = 1;
	}

	return ret;

}
static int  max17050_quickstart_check_normalboot
	(u16 v_cell, u16 v_focv)
{
	u16 result;
	int ret = 0;
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	u16 result2;
	bool power_ok;

	result2 = v_cell - v_focv;
#endif
	result = v_focv - v_cell;
	pr_debug("[MAX17050]normalboot -result (%d),", result);
	pr_debug("  v_focv (%d),  vcell(%d)\n", v_focv, v_cell);

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	power_ok = external_smb349_is_charger_present();
#endif

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	/* For A1 DCM with BT-L7(3000mAh) battery*/
	if (!power_ok) {
		if (v_cell >= 3800) {
			if (result <= 100 || result2 <= 100) {
				F_bat("[MAX17050] skip quickstart more than 3.8V = %d\n", result);
				F_bat("[MAX17050] skip quickstart more than 3.8V = %d\n", result2);
			} else {
				ret = 1;
				F_bat("[MAX17050] quickstart more than 3.8V = %d\n", result);
				F_bat("[MAX17050] quickstart more than 3.8V = %d\n", result2);
			}
		} else {
			if (result <= 120 || result2 <= 120) {
				F_bat("[MAX17050] skip quickstart under 3.8V = %d\n", result);
				F_bat("[MAX17050] skip quickstart under 3.8V = %d\n", result2);
			} else {
				ret = 1;
				F_bat("[MAX17050] quickstart under 3.8V = %d\n", result);
				F_bat("[MAX17050] quickstart under 3.8V = %d\n", result2);
			}
		}
	} else if (power_ok) {
		if (result <= 170 || result2 <= 170) {
			F_bat("[MAX17050] skip quickstart in case of chg result2 = %d\n", result);
			F_bat("[MAX17050] skip quickstart in case of chg result2 = %d\n", result2);
		}
		else {
			F_bat("[MAX17050] quickstart in case of chg result2 = %d\n", result);
			F_bat("[MAX17050] quickstart in case of chg result2 = %d\n", result2);
			ret = 1;
		}
	}
#else
	/* Brought from Vu, GV model. */
	if (v_cell >= 3800) {
		if (result >= 67 && result <= 100) {
			pr_debug("[MAX17047] - skip quickstart\n");
		} else {
			pr_debug("[MAX17047] - quick_start !!\n");
			ret = 1;
		}
	} else {
		if (result >= 72 && result <= 100) {
			pr_debug("[MAX17047]  - skip quickstart\n");
		} else {
			pr_debug("[MAX17047]  - quick_start !!\n");
			ret = 1;
		}
	}
#endif

	return ret;

}


void max17050_initial_quickstart_check(void)
{

	u16 read_reg;
	/*u16 write_reg;*/
	u16 vfocv;
	u16 vcell;

	unsigned int *power_on_status = 0;
	unsigned int smem_size;

	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_V_CELL);
	vcell = (read_reg >> 3);
	vcell = (vcell * 625) / 1000;

	read_reg = max17050_read_reg(max17050_i2c_client, MAX17050_V_FOCV);
	vfocv = (read_reg >> 4);
	vfocv = (vfocv * 125) / 100;


	power_on_status = smem_get_entry(SMEM_POWER_ON_STATUS_INFO, &smem_size);
	if (smem_size == 0 || !power_on_status) {
		pr_err("max17050 subsystem failure reason:");
		pr_err(" (unknown, smem_get_entry failed).\n");
		return;
	}
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	/* for A1 DCM PM8941 */
	if((0xff & *power_on_status) ==0x10)
#else
	/* for G, GV PM8941 */
	if((0xff & *power_on_status) ==0x20)
#endif
	{
		if(max17050_quickstart_check_chglogo_boot(vcell, vfocv))
			F_bat("[MAX17050] chglogo quicksart!!\n");
			/*goto quick_start;*/
	} else {
		if (max17050_quickstart_check_normalboot(vcell, vfocv))
			F_bat("[MAX17050] normalboot quicksart!!\n");
			/*goto quick_start;*/
	}
	return;

/*quick_start:
	max17050_quick_start();
	return;*/
}
EXPORT_SYMBOL(max17050_initial_quickstart_check);

static int __devinit max17050_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17050_chip *chip;
#ifdef CONFIG_LGE_PM
		hw_rev_type rev;
#endif

	int ret = 0;

	/*Test only
	int i = 0;
	u16 read_reg;*/

	F_bat("[MAX17050] %s()  Start\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

#ifdef CONFIG_LGE_PM
			rev = lge_get_board_revno();

			if (rev > HW_REV_A) {
				ret = smb349_is_ready();
			} else if (rev == HW_REV_A) {
				pr_debug("[MAX17050] HW_REV_A not used MAX17050.");
				goto error;
			} else {
				ret = qpnp_charger_is_ready();
			}
			if (ret)
				goto error;
#endif

	chip->client = client;
#ifdef CONFIG_OF
		if (!(&client->dev.of_node)) {
			pr_debug("max17050_probe of_node err.\n");
			goto error;
		}
#else
	chip->pdata = client->dev.platform_data;
#endif

	i2c_set_clientdata(client, chip);

	max17050_i2c_client = client;

	/*Call max17050_new_custom_model_write*/
	ret = max17050_new_custom_model_write();

	if (ret == 2)
		pr_debug("NON-POWER_ON reset. Proceed Booting.\n");
	/*Error occurred. Write once more.*/
	else if (ret == 1)
		max17050_new_custom_model_write();
	/*New Custom model write End.
	Go to max17050_restore_bat_info_from_flash.*/
	else if (ret == 0)
		pr_debug("POWER_ON reset. Custom Model Success.\n");

	ret = device_create_file(&client->dev, &dev_attr_at_fuelrst);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_fuelrst_failed;
	}
	ret = device_create_file(&client->dev, &dev_attr_at_fuelval);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_fuelval_failed;
	}
	ret = device_create_file(&client->dev, &dev_attr_at_batl);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_batl_failed;
	}
	
	//batt_id_pullup_out_low();
	gpio_tlmm_config(GPIO_CFG(BATT_ID, 0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_DISABLE);
	gpio_tlmm_config(GPIO_CFG(BATT_ID_PULLUP, 0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_DISABLE);
	
	F_bat("[MAX17050] %s()  End\n", __func__);

	return 0;
err_create_file_fuelrst_failed:
	device_remove_file(&client->dev, &dev_attr_at_fuelrst);
err_create_file_fuelval_failed:
	device_remove_file(&client->dev, &dev_attr_at_fuelval);
err_create_file_batl_failed:
	device_remove_file(&client->dev, &dev_attr_at_batl);
error:
	kfree(chip);
	return ret;
}

static int __devexit max17050_remove(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_at_fuelrst);
	device_remove_file(&client->dev, &dev_attr_at_fuelval);
	device_remove_file(&client->dev, &dev_attr_at_batl);

	/*power_supply_unregister(&chip->battery);*/
	i2c_set_clientdata(client, NULL);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max17050_match_table[] = {
	{ .compatible = "maxim,max17050", },
	{ },
};
#endif

static const struct i2c_device_id max17050_id[] = {
	{ "max17050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17050_id);

static struct i2c_driver max17050_i2c_driver = {
	.driver	= {
		.name	= "max17050",
#ifdef CONFIG_LGE_PM
		.owner	= THIS_MODULE,
#endif
#ifdef CONFIG_OF
		.of_match_table = max17050_match_table,
#endif
	},
	.probe		= max17050_probe,
	.remove		= __devexit_p(max17050_remove),
	.id_table	= max17050_id,
};

static int __init max17050_init(void)
{
	return i2c_add_driver(&max17050_i2c_driver);
}
module_init(max17050_init);

static void __exit max17050_exit(void)
{
	i2c_del_driver(&max17050_i2c_driver);
}
module_exit(max17050_exit);

MODULE_AUTHOR("LG Power <lge.com>");
MODULE_DESCRIPTION("MAX17050 Fuel Gauge");
MODULE_LICENSE("GPL");
