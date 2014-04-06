/* Copyright (c) 2013 LGE Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c/bq24192_charger.h>
#include <linux/power_supply.h>
#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <mach/board_lge.h>
#include <linux/max17048_battery.h>
#include <linux/qpnp/qpnp-adc.h>
#include "../../arch/arm/mach-msm/smd_private.h"
#include <linux/usb/otg.h>
#include "../usb/dwc3/dwc3_otg.h"
#include "../usb/dwc3/core.h"
#include <linux/reboot.h>
#include <linux/switch.h>
#include <linux/qpnp-misc.h>

#ifdef CONFIG_WIRELESS_CHARGER
#ifdef CONFIG_BQ51053B_CHARGER
#include <linux/power/bq51053b_charger.h>
#endif
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#include <mach/lge_charging_scenario.h>
#define MONITOR_BATTEMP_POLLING_PERIOD          (60*HZ)
#endif

#if defined(CONFIG_MACH_MSM8974_B1_KR)
#include <linux/qpnp/qpnp-temp-alarm.h>
#endif
#ifdef CONFIG_ZERO_WAIT
#include <linux/zwait.h>
#endif

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

/* Register definitions */
#define BQ00_INPUT_SRC_CONT_REG              0X00
#define BQ01_PWR_ON_CONF_REG                 0X01
#define BQ02_CHARGE_CUR_CONT_REG             0X02
#define BQ03_PRE_CHARGE_TERM_CUR_REG         0X03
#define BQ04_CHARGE_VOLT_CONT_REG            0X04
#define BQ05_CHARGE_TERM_TIMER_CONT_REG      0X05
#define BQ06_IR_COMP_THERM_CONT_REG          0X06
#define BQ07_MISC_OPERATION_CONT_REG         0X07
#define BQ08_SYSTEM_STATUS_REG               0X08
#define BQ09_FAULT_REG                       0X09
#define BQ0A_VENDOR_PART_REV_STATUS_REG      0X0A

/* BQ00 Input Source Control Register MASK */
#define VINDPM_MASK 		(BIT(6)|BIT(5)|BIT(4)|BIT(3))
#define IINLIM_MASK 		(BIT(2)|BIT(1)|BIT(0))

/* BQ01 Power-On Configuration  Register MASK */
#define RESET_REG_MASK		BIT(7)
#define CHG_CONFIG_MASK 	(BIT(5)|BIT(4))
#define OTG_ENABLE_MASK		BIT(5)

//#define SYSTEM_MIN_VOLTAGE_MASK    0x0E
#define SYS_MIN_VOL_MASK	(BIT(3)|BIT(2)|BIT(1))
#define BOOST_LIM 		BIT(0)

/* BQ02 Charge Current Control Register MASK */
#define ICHG_MASK 		(BIT(7)|BIT(6)|BIT(5)|BIT(4)|BIT(3)|BIT(2))
#define FORCE_20PCT_MASK	BIT(0)

/* BQ03 Pre-Charge, Termination Current Control Register MASK */
#define IPRECHG_MASK 		(BIT(7)|BIT(6)|BIT(5)|BIT(4))
#define ITERM_MASK		(BIT(3)|BIT(2)|BIT(1)|BIT(0))

/* BQ04 Charge Voltage Control Register MASK */
#define CHG_VOLTAGE_LIMIT_MASK 	(BIT(7)|BIT(6)|BIT(5)|BIT(4)|BIT(3)|BIT(2))
#define BATLOWV_MASK 		BIT(1)
#define VRECHG_MASK 		BIT(0)

/* BQ05 Charge Termination, Timer-Control Register MASK */
#define EN_CHG_TERM_MASK 	BIT(7)
#define I2C_TIMER_MASK          (BIT(5)|BIT(4))
#define EN_CHG_TIMER_MASK	BIT(3)
#define CHG_TIMER_MASK 		(BIT(2)|BIT(1))

/* BQ06 IR Compensation, Thermal Regulation Control Register MASK */
#define IR_COMP_R_MASK		(BIT(7)|BIT(6)|BIT(5))
#define IR_COMP_VCLAMP_MASK 	(BIT(4)|BIT(3)|BIT(2))

/* BQ07 Misc-Operation Control Register MASK */
#define BATFET_DISABLE_MASK 	BIT(5)

/* BQ08 SYSTEM_STATUS_REG Mask */
#define VBUS_STAT_MASK 		(BIT(7)|BIT(6))
#define PRE_CHARGE_MASK 	BIT(4)
#define FAST_CHARGE_MASK 	BIT(5)
#define CHRG_STAT_MASK		(FAST_CHARGE_MASK|PRE_CHARGE_MASK)
#define DPM_STAT_MASK		BIT(3)
#define PG_STAT_MASK		BIT(2)
#define THERM_STAT_MASK 	BIT(1)
#define VSYS_STAT_MASK 		BIT(0)

/* BQ09 FAULT_REG Mask */
#define CHRG_FAULT_MASK 	(BIT(5)|BIT(4))

#define LT_CABLE_56K		6
#define LT_CABLE_130K		7
#define LT_CABLE_910K		11

#define I2C_SUSPEND_WORKAROUND 1

enum bq24192_chg_status {
	BQ_CHG_STATUS_NONE 		= 0,
	BQ_CHG_STATUS_PRE_CHARGE	= 1,
	BQ_CHG_STATUS_FAST_CHARGE 	= 2,
	BQ_CHG_STATUS_FULL 		= 3,
	BQ_CHG_STATUS_EXCEPTION		= 4,
};

static const char * const bq24192_chg_status[] = {
	"none",
	"pre-charge",
	"fast-charge",
	"full"
	"exception"
};

struct bq24192_chip {
	struct i2c_client  *client;
	struct dentry  *dent;
	struct switch_dev batt_removed;

#ifdef CONFIG_MACH_MSM8974_B1_KR
	int input_current_ma;
	int prev_input_current_ma;
	int thermal_mitigation;
#endif
	int chg_current_ma;
	int term_current_ma;
	int vbat_max_mv;
	int pre_chg_current_ma;
	int sys_vmin_mv;
	int vin_limit_mv;
	int int_gpio;
#if defined(CONFIG_MACH_MSM8974_B1_KR)
	int ext_chg_en;
	int otg_en;
#endif
	int irq;
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	int wlc_present;
#endif
	int usb_present;
	int usb_online;
	int ac_present;
	int ac_online;
	int chg_type;
	int charging_disabled;
	int full_design;
	bool chg_timeout;
	int icl_vbus_mv;
	int icl_idx;
	bool icl_first;
	int icl_fail_cnt;
	int set_icl_idx;
	struct wake_lock icl_wake_lock;
	struct delayed_work input_limit_work;
	struct delayed_work input_limit_exception_work;
	enum bq24192_chg_status	chg_status;

	struct delayed_work  irq_work;
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	struct delayed_work		battemp_work;
	struct wake_lock		lcs_wake_lock;
	enum   lge_btm_states	btm_state;
	int pseudo_ui_chg;
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	int chg_current_te;
	bool force_ichg_20pct;
#endif
#endif
	struct wake_lock  chg_wake_lock;
	struct power_supply  *usb_psy;
	struct power_supply  ac_psy;
	struct power_supply  batt_psy;
#ifdef CONFIG_LGE_CURRENTNOW
	struct power_supply  *cn_psy;
#endif
	struct wake_lock uevent_wake_lock;

	int  set_chg_current_ma;
	struct wake_lock battgone_wake_lock;
	struct wake_lock chg_timeout_lock;

#if I2C_SUSPEND_WORKAROUND
	struct delayed_work check_suspended_work;
	int suspended;
#endif
};


#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
int last_batt_temp;
int last_batt_current;
#endif

#ifdef CONFIG_WIRELESS_CHARGER
static int wireless_charging;
#endif

static struct bq24192_chip *the_chip;

struct pseudo_batt_info_type pseudo_batt_info = {
	.mode = 0,
};

struct debug_reg {
	char  *name;
	u8  reg;
};

#define BQ24192_DEBUG_REG(x, y) {#x#y, y##_REG}

static struct debug_reg bq24192_debug_regs[] = {
	BQ24192_DEBUG_REG(00_, BQ00_INPUT_SRC_CONT),
	BQ24192_DEBUG_REG(01_, BQ01_PWR_ON_CONF),
	BQ24192_DEBUG_REG(02_, BQ02_CHARGE_CUR_CONT),
	BQ24192_DEBUG_REG(03_, BQ03_PRE_CHARGE_TERM_CUR),
	BQ24192_DEBUG_REG(04_, BQ04_CHARGE_VOLT_CONT),
	BQ24192_DEBUG_REG(05_, BQ05_CHARGE_TERM_TIMER_CONT),
	BQ24192_DEBUG_REG(06_, BQ06_IR_COMP_THERM_CONT),
	BQ24192_DEBUG_REG(07_, BQ07_MISC_OPERATION_CONT),
	BQ24192_DEBUG_REG(08_, BQ08_SYSTEM_STATUS),
	BQ24192_DEBUG_REG(09_, BQ09_FAULT),
	BQ24192_DEBUG_REG(0A_, BQ0A_VENDOR_PART_REV_STATUS),
};

int32_t bq24192_is_ready(void)
{
	struct bq24192_chip *chip = the_chip;

	if (!chip)
		return -EPROBE_DEFER;
	return 0;
}
EXPORT_SYMBOL(bq24192_is_ready);

static unsigned int cable_type;
static unsigned int cable_smem_size;
static unsigned int factory_mode;

static bool is_factory_cable(void)
{
	unsigned int cable_info;
	cable_info = lge_pm_get_cable_type();

	if ((cable_info == CABLE_56K ||
		cable_info == CABLE_130K ||
		cable_info == CABLE_910K) ||
		(cable_type == LT_CABLE_56K ||
		cable_type == LT_CABLE_130K ||
		cable_type == LT_CABLE_910K))
		return true;
	else
		return false;
}

static bool is_factory_cable_130k(void)
{
	unsigned int cable_info;
	cable_info = lge_pm_get_cable_type();

	if (cable_info == CABLE_130K ||
		cable_type == LT_CABLE_130K)
		return true;
	else
		return false;
}

static int bq24192_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev,
			"i2c read fail: can't read from %02x: %d\n",
			reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int bq24192_write_reg(struct i2c_client *client, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int bq24192_masked_write(struct i2c_client *client, int reg,
			       u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = bq24192_read_reg(client, reg, &temp);
	if (rc) {
		pr_err("bq24192_read_reg failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	temp &= ~mask;
	temp |= val & mask;

	rc = bq24192_write_reg(client, reg, temp);
	if (rc) {
		pr_err("bq24192_write failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}

	return 0;
}

static void bq24192_reginfo(void)
{
	int i;
	int cnt = ARRAY_SIZE(bq24192_debug_regs);
	u8 val[cnt];

	if(!the_chip)
		return;

	for (i = 0; i < cnt; i++)
		bq24192_read_reg(the_chip->client,
			bq24192_debug_regs[i].reg, &val[i]);

	pr_info("0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		val[0],val[1],val[2],val[3],val[4],
		val[5],val[6],val[7],val[8],val[9]);
}

struct input_ma_limit_entry {
	int  icl_ma;
	u8  value;
};

static struct input_ma_limit_entry icl_ma_table[] = {
	{100, 0x00},
	{150, 0x01},
	{500, 0x02},
	{900, 0x03},
	{1200, 0x04},
	{1500, 0x05},
	{2000, 0x06},
	{3000, 0x07},
};

#define INPUT_CURRENT_LIMIT_MIN_MA  100
#define INPUT_CURRENT_LIMIT_MAX_MA  3000
#if defined (CONFIG_MACH_MSM8974_Z_US) || defined (CONFIG_MACH_MSM8974_B1_KR)
#define INPUT_CURRENT_LIMIT_TA 1500
#else
#define INPUT_CURRENT_LIMIT_TA 2000
#endif
#define INPUT_CURRENT_LIMIT_FACTORY 1500
#define INPUT_CURRENT_LIMIT_USB20 500
#define INPUT_CURRENT_LIMIT_USB30 900
static int bq24192_set_input_i_limit(struct bq24192_chip *chip, int ma)
{
	int i;
	u8 temp;

	if (ma < INPUT_CURRENT_LIMIT_MIN_MA
			|| ma > INPUT_CURRENT_LIMIT_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", ma);
		return -EINVAL;
	}

	for (i = ARRAY_SIZE(icl_ma_table) - 1; i >= 0; i--) {
		if (icl_ma_table[i].icl_ma == ma)
			break;
	}

	if (i < 0) {
		pr_err("can't find %d in icl_ma_table. Use min.\n", ma);
		i = 0;
	}

	temp = icl_ma_table[i].value;

	pr_info("input current limit=%d setting 0x%02x\n", ma, temp);
	return bq24192_masked_write(chip->client, BQ00_INPUT_SRC_CONT_REG,
			IINLIM_MASK, temp);
}

#define IBAT_MAX_MA  4532
#define IBAT_MIN_MA  500
#define IBAT_STEP_MA  64
#define IBAT_DEFAULT  2048
static int bq24192_set_ibat_max(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	int set_ibat = 0;

	if (ma < IBAT_MIN_MA || ma > IBAT_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - IBAT_MIN_MA)/IBAT_STEP_MA;
	set_ibat = reg_val * IBAT_STEP_MA + IBAT_MIN_MA;
	reg_val = reg_val << 2;
	chip->set_chg_current_ma = set_ibat;
	pr_info("req_ibat = %d set_ibat = %d reg_val = 0x%02x\n",
				ma, set_ibat, reg_val);

	return bq24192_masked_write(chip->client, BQ02_CHARGE_CUR_CONT_REG,
			ICHG_MASK, reg_val);
}

#define VIN_LIMIT_MIN_MV  3880
#define VIN_LIMIT_MAX_MV  5080
#define VIN_LIMIT_STEP_MV  80
static int bq24192_set_input_vin_limit(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_vin = 0;

	if (mv < VIN_LIMIT_MIN_MV || mv > VIN_LIMIT_MAX_MV) {
		pr_err("bad mV=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VIN_LIMIT_MIN_MV)/VIN_LIMIT_STEP_MV;
	set_vin = reg_val * VIN_LIMIT_STEP_MV + VIN_LIMIT_MIN_MV;
	reg_val = reg_val << 3;

	pr_info("req_vin = %d set_vin = %d reg_val = 0x%02x\n",
				mv, set_vin, reg_val);

	return bq24192_masked_write(chip->client, BQ00_INPUT_SRC_CONT_REG,
			VINDPM_MASK, reg_val);
}

#define VBAT_MAX_MV  4400
#define VBAT_MIN_MV  3504
#define VBAT_STEP_MV  16
static int bq24192_set_vbat_max(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_vbat = 0;

	if (mv < VBAT_MIN_MV || mv > VBAT_MAX_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VBAT_MIN_MV)/VBAT_STEP_MV;
	set_vbat = reg_val * VBAT_STEP_MV + VBAT_MIN_MV;
	reg_val = reg_val << 2;

	pr_info("req_vbat = %d set_vbat = %d reg_val = 0x%02x\n",
				mv, set_vbat, reg_val);

	return bq24192_masked_write(chip->client, BQ04_CHARGE_VOLT_CONT_REG,
			CHG_VOLTAGE_LIMIT_MASK, reg_val);
}

#define SYSTEM_VMIN_LOW_MV  3000
#define SYSTEM_VMIN_HIGH_MV  3700
#define SYSTEM_VMIN_STEP_MV  100
static int bq24192_set_system_vmin(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_vmin = 0;

	if (mv < SYSTEM_VMIN_LOW_MV || mv > SYSTEM_VMIN_HIGH_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - SYSTEM_VMIN_LOW_MV)/SYSTEM_VMIN_STEP_MV;
	set_vmin = reg_val * SYSTEM_VMIN_STEP_MV + SYSTEM_VMIN_LOW_MV;
	reg_val = reg_val << 1;

	pr_info("req_vmin = %d set_vmin = %d reg_val = 0x%02x\n",
				mv, set_vmin, reg_val);

	return bq24192_masked_write(chip->client, BQ01_PWR_ON_CONF_REG,
			SYS_MIN_VOL_MASK, reg_val);
}

#define IPRECHG_MIN_MA  128
#define IPRECHG_MAX_MA  2048
#define IPRECHG_STEP_MA  128
static int bq24192_set_prechg_i_limit(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (ma < IPRECHG_MIN_MA || ma > IPRECHG_MAX_MA) {
		pr_err("bad ma=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - IPRECHG_MIN_MA)/IPRECHG_STEP_MA;
	set_ma = reg_val * IPRECHG_STEP_MA + IPRECHG_MIN_MA;
	reg_val = reg_val << 4;

	pr_info("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24192_masked_write(chip->client, BQ03_PRE_CHARGE_TERM_CUR_REG,
			IPRECHG_MASK, reg_val);
}

#define ITERM_MIN_MA  128
#define ITERM_MAX_MA  2048
#define ITERM_STEP_MA  128
static int bq24192_set_term_current(struct bq24192_chip *chip, int ma)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (ma < ITERM_MIN_MA || ma > ITERM_MAX_MA) {
		pr_err("bad mv=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - ITERM_MIN_MA)/ITERM_STEP_MA;
	set_ma = reg_val * ITERM_STEP_MA + ITERM_MIN_MA;

	pr_info("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24192_masked_write(chip->client, BQ03_PRE_CHARGE_TERM_CUR_REG,
			ITERM_MASK, reg_val);
}

#define EN_TIMER_SHIFT 3
static int bq24192_set_chg_timer(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << EN_TIMER_SHIFT);

	pr_info("enable=%d\n", enable);

	ret = bq24192_masked_write(chip->client, BQ05_CHARGE_TERM_TIMER_CONT_REG,
						EN_CHG_TIMER_MASK, val);
	if (ret) {
		pr_err("failed to set chg safety timer ret=%d\n", ret);
		return ret;
	}

	return 0;
}

#define CHG_TIMEOUT_SHIFT 1
static int bq24192_set_chg_timeout(struct bq24192_chip *chip)
{
	u8 reg_val = 1;

	pr_info("req_chg_timeout set_h = 8hrs\n");

	chip->chg_timeout = false;

	reg_val = reg_val << CHG_TIMEOUT_SHIFT;

	return bq24192_masked_write(chip->client, BQ05_CHARGE_TERM_TIMER_CONT_REG,
			CHG_TIMER_MASK, reg_val);
}

#define EN_CHG_TERM_SHIFT 7
static int bq24192_set_chg_term(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << EN_CHG_TERM_SHIFT);

	pr_info("enable=%d\n", enable);

	ret = bq24192_masked_write(chip->client, BQ05_CHARGE_TERM_TIMER_CONT_REG,
						EN_CHG_TERM_MASK, val);
	if (ret) {
		pr_err("failed to disable chg term  ret=%d\n", ret);
		return ret;
	}

	return 0;

}

#define IRCOMP_R_MIN_MOHM  0
#define IRCOMP_R_MAX_MOHM  70
#define IRCOMP_R_50_MOHM   50
#define IRCOMP_R_STEP_MOHM 10
static int bq24192_set_ir_comp_resister(struct bq24192_chip *chip, int mohm)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (mohm < IRCOMP_R_MIN_MOHM
			|| mohm > IRCOMP_R_MAX_MOHM) {
		pr_err("bad r=%d asked to set\n", mohm);
		return -EINVAL;
	}

	reg_val = (mohm - IRCOMP_R_MIN_MOHM)/IRCOMP_R_STEP_MOHM;
	set_ma = reg_val * IRCOMP_R_STEP_MOHM + IRCOMP_R_MIN_MOHM;
	reg_val = reg_val << 5;

	pr_info("req_r = %d set_r = %d reg_val = 0x%02x\n",
				mohm, set_ma, reg_val);

	return bq24192_masked_write(chip->client, BQ06_IR_COMP_THERM_CONT_REG,
			IR_COMP_R_MASK, reg_val);
}

#define IRCOMP_VCLAMP_MIN_MV  0
#define IRCOMP_VCLAMP_MAX_MV  112
#define IRCOMP_VCLAMP_STEP_MV  16
static int bq24192_set_vclamp_mv(struct bq24192_chip *chip, int mv)
{
	u8 reg_val = 0;
	int set_ma = 0;

	if (mv < IRCOMP_VCLAMP_MIN_MV
			|| mv > IRCOMP_VCLAMP_MAX_MV) {
		pr_err("bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - IRCOMP_VCLAMP_MIN_MV)/IRCOMP_VCLAMP_STEP_MV;
	set_ma = reg_val * IRCOMP_VCLAMP_STEP_MV + IRCOMP_VCLAMP_MIN_MV;
	reg_val = reg_val << 2;

	pr_info("req_mv = %d set_mv = %d reg_val = 0x%02x\n",
				mv, set_ma, reg_val);

	return bq24192_masked_write(chip->client, BQ06_IR_COMP_THERM_CONT_REG,
			IR_COMP_VCLAMP_MASK, reg_val);
}

#define NOT_INIT_VBUS_UV 5000000
static int bq24192_get_usbin_adc(void)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE

/* LIMIT: Include ONLY A1, B1, Vu3, Z models used MSM8974 AA/AB */
#ifdef CONFIG_ADC_READY_CHECK_JB
	struct qpnp_vadc_result results;
	int rc = 0;

	if (qpnp_vadc_is_ready() == 0) {
		rc = qpnp_vadc_read_lge(USBIN, &results);
		if (rc) {
			pr_err("Unable to read usbin adc rc=%d\n", rc);
			return NOT_INIT_VBUS_UV;
		}
		else {
			pr_debug("DC_IN voltage: %lld\n", results.physical);
			return results.physical;
		}
	} else {
		pr_err("vadc is not ready yet.\n");
		return NOT_INIT_VBUS_UV;
	}
#else
	/* MUST BE IMPLEMENT :
	 * After MSM8974 AC and later version(PMIC combination change),
	 * ADC AMUX of PMICs are separated in each dual PMIC.
	 *
	 * Ref.
	 * qpnp-adc-voltage.c : *qpnp_get_vadc(), qpnp_vadc_read().
	 * qpnp-charger.c     : new implementation by QCT.
	 */
	return NOT_INIT_VBUS_UV;
#endif
#else
	pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return NOT_INIT_VBUS_UV;
#endif
}

static bool bq24192_is_charger_present(struct bq24192_chip *chip)
{
	int ret = 0, vol = 0;
	u8 sys_status, power_good;
	bool power_ok;

	ret = bq24192_read_reg(chip->client, BQ08_SYSTEM_STATUS_REG, &sys_status);
	if (ret) {
		pr_err("failed to read BQ08_SYSTEM_STATUS_REG ret=%d\n", ret);
		return false;
	}

	power_good = (sys_status & PG_STAT_MASK);
	sys_status &= VBUS_STAT_MASK;

	if((power_good==0)&&(sys_status==0 || sys_status==0xC0)) {
		power_ok = false;
		pr_info("DC is missing.\n");
	} else {
		power_ok = true;
		vol = bq24192_get_usbin_adc();
		pr_info("DC is present. DC_IN vol=%d\n", vol);
	}

	return power_ok;
}

#ifdef CONFIG_BATFET_FORCE_CTRL
#define BATFET_DISABLE_SHIFT  5
static int bq24192_force_disable_batfet(struct bq24192_chip *chip, bool disable)
{
	int ret;
	u8 val = (u8)(!!disable << BATFET_DISABLE_SHIFT);

	pr_info("disable=%d\n", disable);

	ret = bq24192_masked_write(chip->client, BQ07_MISC_OPERATION_CONT_REG,
						BATFET_DISABLE_MASK, val);
	if (ret) {
		pr_err("failed to set BATFET Disable ret=%d\n", ret);
		return ret;
	}

	return 0;
}
#endif

static int bq24192_force_ichg_decrease(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable);

	pr_info("enable=%d\n", enable);

	ret = bq24192_masked_write(chip->client, BQ02_CHARGE_CUR_CONT_REG,
			FORCE_20PCT_MASK, val);
	if (ret) {
		pr_err("failed to set FORCE_20PCT ret=%d\n", ret);
		return ret;
	}

	return 0;
}

#define CHG_ENABLE_SHIFT  4
static int bq24192_enable_charging(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << CHG_ENABLE_SHIFT);

	pr_info("enable=%d\n", enable);

	if (chip->chg_timeout) {
		pr_err("charging timeout state, never enabel charging\n");
		return 0;
	}

	ret = bq24192_masked_write(chip->client, BQ01_PWR_ON_CONF_REG,
						CHG_CONFIG_MASK, val);
	if (ret) {
		pr_err("failed to set CHG_CONFIG ret=%d\n", ret);
		return ret;
	}

	chip->charging_disabled = !enable;

	return 0;
}

int32_t external_bq24192_enable_charging(bool enable)
{
	int ret;

	pr_debug("enable=%d.\n", enable);

	ret = bq24192_enable_charging(the_chip, enable);
	if (ret) {
		pr_err("Failed to set CHG_ENABLE_BIT ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int bq24192_get_prop_batt_present(struct bq24192_chip *chip)
{
	int temp = 0;
	bool batt_present;

#if 0//def CONFIG_QPNP_MISC
	if(!bq24192_pmic_batt_present())
		batt_present = 0;
	else
		batt_present = 1;
#else
	// Must be verified!
	temp = bq24192_get_batt_temp_origin();

	if(temp <= -300 || temp >= 790) {
		pr_err("battery missing(%d). \n", temp);
		batt_present = 0;
	} else
		batt_present = 1;
#endif
	pr_debug("bq24192_get_prop_batt_present present=%d\n",
		batt_present ? 1 : 0);

	if (pseudo_batt_info.mode) {
		return 1;
	}
	return batt_present ? 1 : 0;
}

static void bq24192_chg_timeout(bool chg_en)
{
	int ret;

	pr_info("charge safety timer expired!\n");
	cancel_delayed_work_sync(&the_chip->battemp_work);

	wake_lock(&the_chip->chg_timeout_lock);

	ret = bq24192_enable_charging(the_chip, chg_en);
	if (ret)
		pr_err("Failed to set CHG_CONFIG_MASK ret=%d\n", ret);

	the_chip->chg_timeout = true;

	schedule_delayed_work(&the_chip->battemp_work,
		MONITOR_BATTEMP_POLLING_PERIOD);
}

static int bootcompleted;
static int
bq24192_set_bootcompleted(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	pr_info(" %d\n",bootcompleted);

	return 0;
}
module_param_call(bootcompleted, bq24192_set_bootcompleted,
	param_get_uint, &bootcompleted, 0644);

#if defined(CONFIG_MACH_MSM8974_B1_KR)
static void bq24192_batt_remove_insert_cb(int batt_present)
{
	int batt_temp = 0;
	int usb_present = 0;
	bool ftm_cable = false;

	wake_lock(&the_chip->battgone_wake_lock);

	batt_present = bq24192_get_prop_batt_present(the_chip);
	batt_temp = bq24192_get_batt_temp_origin()/10;
	usb_present = bq24192_is_charger_present(the_chip);
	ftm_cable = is_factory_cable();

	printk(KERN_ERR "[PM] batt_present=%d, batt_temp=%d, usb_present=%d, ftm_cable=%d\n", batt_present, batt_temp, usb_present, ftm_cable);

	if ( ( batt_present == 0 ) && ( batt_temp <= -30 || batt_temp >= 79 ) && ( ftm_cable == 0 ) && ( usb_present == 1 ) )
	{
		printk(KERN_ERR "[PM] Now reset as scenario!\n");

		bq24192_enable_charging(the_chip, 0);
		switch_set_state(&the_chip->batt_removed, 1);
		power_supply_changed(&the_chip->batt_psy);

		msleep(5000);

		kernel_restart("oem-11");
		wake_unlock(&the_chip->battgone_wake_lock);
	} else {
		printk(KERN_ERR "[PM] Do not power off.\n");
		wake_unlock(&the_chip->battgone_wake_lock);
	}
}
#endif

struct current_limit_entry {
	int input_limit;
	int chg_limit;
};

static struct current_limit_entry adap_tbl[] = {
#if defined(CONFIG_MACH_MSM8974_Z_US)
	{1200, 1024},
	{1500, 1500},
#elif defined(CONFIG_MACH_MSM8974_B1_KR)
	{1200, 1024},
	{1500, 1400},
#else
	{1200, 1024},
	{2000, 1600},
#endif
};

static void bq24192_input_limit_worker(struct work_struct *work)
{
	struct bq24192_chip *chip = container_of(work, struct bq24192_chip,
						input_limit_work.work);
	int vbus_mv = 0;

	vbus_mv = bq24192_get_usbin_adc();
	vbus_mv = vbus_mv/1000;

	pr_info("vbus_mv = %d\n", vbus_mv);

	if (chip->icl_first && chip->icl_idx > 0) {
		chip->icl_fail_cnt++;
		if (chip->icl_fail_cnt >= ARRAY_SIZE(adap_tbl))
			vbus_mv = chip->icl_vbus_mv;
		else
			chip->icl_idx = 0;
	}
	chip->icl_first = false;

	if (vbus_mv > chip->icl_vbus_mv
			&& chip->icl_idx < (ARRAY_SIZE(adap_tbl) - 1)) {
		chip->icl_idx++;
		bq24192_set_input_i_limit(chip,
				adap_tbl[chip->icl_idx].input_limit);
		bq24192_set_ibat_max(chip,
				adap_tbl[chip->icl_idx].chg_limit);
		schedule_delayed_work(&chip->input_limit_work,
					msecs_to_jiffies(500));
	} else {
		if (chip->icl_idx > 0 && vbus_mv <= chip->icl_vbus_mv)
			chip->icl_idx--;

		bq24192_set_input_i_limit(chip,
				adap_tbl[chip->icl_idx].input_limit);
		bq24192_set_ibat_max(chip,
				adap_tbl[chip->icl_idx].chg_limit);

		chip->set_icl_idx = chip->icl_idx;

		pr_info("optimal input i limit = %d chg limit = %d\n",
					adap_tbl[chip->icl_idx].input_limit,
					adap_tbl[chip->icl_idx].chg_limit);
		chip->icl_idx = 0;
		chip->icl_fail_cnt = 0;
		wake_unlock(&chip->icl_wake_lock);
	}
}

static void bq24192_input_limit_exception_worker(struct work_struct *work)
{
	struct bq24192_chip *chip = container_of(work, struct bq24192_chip,
						input_limit_exception_work.work);
	int usb_present = bq24192_is_charger_present(chip);
	if (chip->usb_present ^ usb_present) {
		chip->usb_present = usb_present;
		pr_info("notify vbus to usb_present=%d\n", usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
		power_supply_changed(chip->usb_psy);
	}
}

static void bq24192_irq_worker(struct work_struct *work)
{
	struct bq24192_chip *chip =
		container_of(work, struct bq24192_chip, irq_work.work);
	u8 reg08, reg09;
	int ret = 0, usb_present = 0;
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	int wlc_present =0;
#endif

	ret = bq24192_read_reg(chip->client, BQ08_SYSTEM_STATUS_REG, &reg08);
	if (ret)
		return;
	ret = bq24192_read_reg(chip->client, BQ09_FAULT_REG, &reg09);
	if (ret)
		return;

	pr_info("08:0x%02X, 09:0x%02X\n",reg08, reg09);
	if ((reg08 & VBUS_STAT_MASK) == VBUS_STAT_MASK)
		pr_info("otg detection!\n");
	else if (reg08 & BIT(7))
		pr_info("adapter port detected!\n");
	else if (reg08 & BIT(6))
		pr_info("usb host detected!\n");
	if ((reg08 & CHRG_STAT_MASK) == CHRG_STAT_MASK)
		pr_info("charging done!\n");
	else if (reg08 & FAST_CHARGE_MASK)
		pr_info("fast charging!\n");
	else if (reg08 & PRE_CHARGE_MASK)
		pr_info("pre-charging!\n");
	else
		pr_info("not charging!\n");
	if (reg08 & DPM_STAT_MASK)
		pr_info("dpm detected!\n");
	if (reg08 & PG_STAT_MASK)
		pr_info("power good!\n");
	if (reg08 & THERM_STAT_MASK)
		pr_info("thermal regulation!\n");	
	if (reg08 & VSYS_STAT_MASK)
		pr_info("vsysmin regulation! battery is too low!\n");

	if (reg09 & BIT(6))
		pr_info("vbus ocp/ovp!\n");
	if ((reg09 & CHRG_FAULT_MASK) == CHRG_FAULT_MASK)
		bq24192_chg_timeout(0);
	else if (reg09 & BIT(5))
		pr_info("thermal shutdown!\n");
	else if (reg09 & BIT(4))
		pr_info("input fault!\n");
	if (reg09 & BIT(3))
		pr_info("battery ovp!\n");

#if !defined(CONFIG_MACH_MSM8974_B1_KR)
	if (!bq24192_get_prop_batt_present(chip)) {
		bool charger = false;
		bool ftm_cable = is_factory_cable();

		wake_lock_timeout(&chip->battgone_wake_lock, HZ*10);
		msleep(2000); // makes top-half i2c time margin
		charger = bq24192_is_charger_present(chip);
		pr_info("battery removed %d-%d\n",charger, ftm_cable);
		if (charger && !ftm_cable) {
			cancel_delayed_work(&chip->irq_work);
			bq24192_enable_charging(chip, 0);
			switch_set_state(&chip->batt_removed, 1);
			power_supply_changed(&chip->batt_psy);
		}
	}
#endif

	usb_present = bq24192_is_charger_present(chip);
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	wlc_present = wireless_charging || is_wireless_charger_plugged();
	if ((chip->usb_present ^ usb_present) || (chip->wlc_present ^ wlc_present)){
		chip->wlc_present = wlc_present;
#else
	if (chip->usb_present ^ usb_present) {
#endif
#ifdef CONFIG_ZERO_WAIT
		zw_psy_irq_handler(usb_present);
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		cancel_delayed_work_sync(&chip->battemp_work);
		schedule_delayed_work(&chip->battemp_work, HZ*1);
		if (!usb_present &&
			wake_lock_active(&chip->lcs_wake_lock))
			wake_unlock(&chip->lcs_wake_lock);
#endif
		if (wake_lock_active(&chip->icl_wake_lock))
			wake_unlock(&chip->icl_wake_lock);
		cancel_delayed_work_sync(&chip->input_limit_work);
		chip->set_icl_idx = 0;
		cancel_delayed_work_sync(&chip->input_limit_exception_work);

		wake_lock_timeout(&chip->uevent_wake_lock, HZ*2);
		if (usb_present == 0 && chip->icl_idx > 0 && bootcompleted) {
			pr_info("input_limit_worker exception\n");
			schedule_delayed_work(&chip->input_limit_exception_work,
				msecs_to_jiffies(1500));
		}
		else {
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
			if(wlc_present){
		  		pr_err("[WLC] set usb_present to 0 \n");
		  		chip->usb_present = 0;
			} else {
				chip->usb_present = usb_present;
			}
#else
			chip->usb_present = usb_present;
#endif
			pr_info("notify vbus to usb_present=%d\n", usb_present);
			power_supply_set_present(chip->usb_psy, chip->usb_present);
		}
	}

	power_supply_changed(chip->usb_psy);
}

static irqreturn_t bq24192_irq(int irq, void *dev_id)
{
	struct bq24192_chip *chip = dev_id;

#if I2C_SUSPEND_WORKAROUND
	schedule_delayed_work(&chip->check_suspended_work, msecs_to_jiffies(100));
#else
	schedule_delayed_work(&chip->irq_work, msecs_to_jiffies(100));
#endif

	return IRQ_HANDLED;
}

#if I2C_SUSPEND_WORKAROUND
static void bq24192_check_suspended_worker(struct work_struct *work)
{
	struct bq24192_chip *chip = container_of(work, struct bq24192_chip, check_suspended_work.work);

	if (chip->suspended)
	{
		printk("bq24192 suspended. try i2c operation after 100ms.\n");
		schedule_delayed_work(&chip->check_suspended_work, msecs_to_jiffies(100));
	}
	else
	{
		printk("bq24192 resume. do bq24192_irq.\n");
		schedule_delayed_work(&chip->irq_work, 0);
	}
}
#endif

static int set_reg(void *data, u64 val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = bq24192_write_reg(client, addr, (u8) val);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	u32 addr = (u32) data;
	u8 temp;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = bq24192_read_reg(client, addr, &temp);
	if (ret < 0)
		return ret;

	*val = temp;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

#define OTG_ENABLE_SHIFT  5
static int bq24192_enable_otg(struct bq24192_chip *chip, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << OTG_ENABLE_SHIFT);

	pr_info("otg enable = %d\n", enable);

	ret = bq24192_masked_write(chip->client, BQ01_PWR_ON_CONF_REG,
					OTG_ENABLE_MASK, val);
	if (ret) {
		pr_err("failed to set OTG_ENABLE_MASK rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static bool bq24192_is_otg_mode(struct bq24192_chip *chip)
{
	u8 temp;
	int ret;

	ret = bq24192_read_reg(chip->client, BQ01_PWR_ON_CONF_REG, &temp);
	if (ret) {
		pr_err("failed to read OTG_ENABLE_MASK rc=%d\n", ret);
		return false;
	}

	return !!(temp & OTG_ENABLE_MASK);
}

static bool bq24192_is_usb_2_0(void)
{
	struct usb_phy *otg_xceiv;
	struct dwc3_otg *dotg;
	bool usb20 = true;

	otg_xceiv = usb_get_transceiver();
	if (!otg_xceiv) {
		pr_err("Failed to get usb transceiver.\n");
		return -ENODEV;
	}

	dotg = container_of(otg_xceiv->otg, struct dwc3_otg, otg);
	if (!dotg) {
		pr_err("Failed to get otg driver data.\n");
		return -ENODEV;
	}

	if (dotg->charger->chg_type != DWC3_DCP_CHARGER) {
		if (dotg->dwc->speed == DWC3_DCFG_SUPERSPEED)
			usb20 = false; 	/* USB3.0 */
	}

	return usb20;
}

static char *bq24192_power_supplied_to[] = {
	"battery",
};

static enum power_supply_property bq24192_power_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static enum power_supply_property bq24192_batt_power_props[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_PSEUDO_BATT,
	POWER_SUPPLY_PROP_EXT_PWR_CHECK,
};


static int bq24192_get_prop_charge_type(struct bq24192_chip *chip)
{
	int ret = 0;
	u8 sys_status;
	enum bq24192_chg_status status;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	ret = bq24192_read_reg(chip->client, BQ08_SYSTEM_STATUS_REG, &sys_status);
	if (ret) {
		pr_err("fail to read BQ08_SYSTEM_STATUS_REG. ret=%d\n", ret);
		chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		goto exception_handling;
	}

	sys_status &= CHRG_STAT_MASK;
	if(sys_status==0x10){
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		status = BQ_CHG_STATUS_PRE_CHARGE;
	}
	else if(sys_status==0x20){
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		status = BQ_CHG_STATUS_FAST_CHARGE;
	}
	else if(sys_status==0x30){
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		status = BQ_CHG_STATUS_FULL;
	}
	else {
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		status = BQ_CHG_STATUS_NONE;
	}
	pr_debug("bq-chg-status (%d=%s).\n", status, bq24192_chg_status[status]);

	if (chip->chg_status != status) {
		if (status == BQ_CHG_STATUS_NONE
			|| status == BQ_CHG_STATUS_FULL) {
			pr_debug("Charging stopped.\n");
#ifdef CONFIG_BQ51053B_CHARGER
			if(chip->wlc_present && ((sys_status & FAST_CHARGE_MASK) && (sys_status & PRE_CHARGE_MASK)))
				wireless_charging_completed();
#endif
			wake_unlock(&chip->chg_wake_lock);
		} else {
			pr_debug("Charging started.\n");
			wake_lock(&chip->chg_wake_lock);
		}
		chip->chg_status = status;
	}

	return chg_type;

exception_handling:
	chip->chg_status = BQ_CHG_STATUS_EXCEPTION;
	if (wake_lock_active(&chip->chg_wake_lock)) {
		pr_err("exception_handling : unlock chg_wake_lock.\n");
		wake_unlock(&chip->chg_wake_lock);
	}
	return chg_type;

}

static int bq24192_get_prop_batt_health(struct bq24192_chip *chip)
{
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	if (chip->btm_state == BTM_HEALTH_OVERHEAT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (chip->btm_state == BTM_HEALTH_COLD)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
#else
	int batt_temp;
	batt_temp = bq24192_get_prop_batt_temp(chip);

	if (batt_temp >= 550)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (batt_temp <= -100)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
#endif
}

#define DEFAULT_VOLTAGE		4000000
static int bq24192_get_prop_batt_voltage_now(void)
{
#ifdef CONFIG_MAX17048_FUELGAUGE
	int voltage = 0;
	voltage = max17048_get_voltage() * 1000;
	return voltage;
#else
	pr_err("CONFIG_MAX17048_FUELGAUGE is not defined.\n");
	return DEFAULT_VOLTAGE;
#endif
}

#define DEFAULT_TEMP		250
int bq24192_get_batt_temp_origin(void)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE

/* LIMIT: Include ONLY A1, B1, Vu3, Z models used MSM8974 AA/AB */
#ifdef CONFIG_ADC_READY_CHECK_JB
	int rc = 0;
	struct qpnp_vadc_result results;

	if (qpnp_vadc_is_ready() == 0) {
		rc = qpnp_vadc_read_lge(LR_MUX1_BATT_THERM, &results);
		if (rc) {
			pr_debug("Unable to read batt temperature rc=%d\n", rc);
			pr_debug("Report last_bat_temp %d again\n", last_batt_temp);
			return last_batt_temp;
		} else {
			pr_debug("get_bat_temp %d %lld\n", results.adc_code, results.physical);
			last_batt_temp =(int)results.physical;
			return (int)results.physical;
		}
	} else {
		pr_err("vadc is not ready yet. report default temperature\n");
		return DEFAULT_TEMP;
	}
#else
	/* MUST BE IMPLEMENT :
	 * After MSM8974 AC and later version(PMIC combination change),
	 * ADC AMUX of PMICs are separated in each dual PMIC.
	 *
	 * Ref.
	 * qpnp-adc-voltage.c : *qpnp_get_vadc(), qpnp_vadc_read().
	 * qpnp-charger.c     : new implementation by QCT.
	 */
	return DEFAULT_TEMP;
#endif
#else
	pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return DEFAULT_TEMP;
#endif
}
EXPORT_SYMBOL(bq24192_get_batt_temp_origin);

static int bq24192_get_prop_batt_temp(struct bq24192_chip *chip)
{
	if (pseudo_batt_info.mode) {
		pr_debug("battery fake mode : %d \n", pseudo_batt_info.mode);
		return pseudo_batt_info.temp * 10;
	} else if (is_factory_cable()) {
		pr_debug("factory cable : %d \n", DEFAULT_TEMP / 10);
		return DEFAULT_TEMP;
	}

	/* Approach adc channel for read batt temp' */
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	return bq24192_get_batt_temp_origin();
#else
	pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return DEFAULT_TEMP;
#endif
}


#define DEFAULT_CAPACITY	50
static int bq24192_get_prop_batt_capacity(struct bq24192_chip *chip)
{
#ifdef CONFIG_MAX17048_FUELGAUGE
	return max17048_get_capacity();
#else
	pr_err("CONFIG_MAX17048_FUELGAUGE is not defined.\n");
	return DEFAULT_CAPACITY;
#endif
}


#define DEFAULT_CURRENT		200000
static int bq24192_get_prop_batt_current_now(struct bq24192_chip *chip)
{
#ifdef CONFIG_LGE_CURRENTNOW
	int batt_current = 0;
	union power_supply_propval ret = {0,};

	if (!bq24192_get_prop_batt_present(chip))
		return DEFAULT_CURRENT;

	chip->cn_psy = power_supply_get_by_name("cn");
	if (!chip->cn_psy) {		
		return DEFAULT_CURRENT;
	} else {
		chip->cn_psy->get_property(chip->cn_psy,
			POWER_SUPPLY_PROP_VIRT_ENABLE_BMS, &ret);
		if(!ret.intval)
			return DEFAULT_CURRENT;
		chip->cn_psy->get_property(chip->cn_psy,
			POWER_SUPPLY_PROP_VIRT_CURRENT_NOW, &ret);
		batt_current = (int)(0-ret.intval);
		last_batt_current = batt_current;
		return batt_current;
	}
#elif defined(CONFIG_SENSORS_QPNP_ADC_CURRENT)
#ifdef CONFIG_ADC_READY_CHECK_JB
	struct qpnp_iadc_result result;
	int batt_current = 0;
	int ret;

	if (qpnp_iadc_is_ready()) {
		pr_err("qpnp_iadc is not ready!\n");
		return DEFAULT_CURRENT;
	}

	ret = qpnp_iadc_read_lge(EXTERNAL_RSENSE, &result);
	if (ret) {
		pr_err("failed to read qpnp_iadc\n");
		return DEFAULT_CURRENT;
	}

	batt_current = result.result_ua;
	pr_info("battery_current= %d\n", batt_current);
	return batt_current;
#else /* CONFIG_ADC_READY_CHECK_JB */
	return DEFAULT_CURRENT;
#endif /* CONFIG_ADC_READY_CHECK_JB */
#else /*                       */
	return DEFAULT_CURRENT;
#endif /*                       */
}

#define DEFAULT_FULL_DESIGN	2500
static int bq24192_get_prop_batt_full_design(struct bq24192_chip *chip)
{
#ifdef CONFIG_MAX17048_FUELGAUGE
	return max17048_get_fulldesign();
#else
	pr_err("CONFIG_MAX17048_FUELGAUGE is not defined.\n");
	return DEFAULT_FULL_DESIGN;
#endif
}

static int bq24192_get_prop_batt_status(struct bq24192_chip *chip)
{
	int chg_type = bq24192_get_prop_charge_type(chip);
	int batt_present = bq24192_get_prop_batt_present(chip);
	int capacity = bq24192_get_prop_batt_capacity(chip);

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	if (((chip->usb_present) || (chip->wlc_present)) && (chip->pseudo_ui_chg))
		return POWER_SUPPLY_STATUS_CHARGING;
#else
	if (chip->usb_present && chip->pseudo_ui_chg)
		return POWER_SUPPLY_STATUS_CHARGING;
#endif
#endif

	if (chg_type == POWER_SUPPLY_CHARGE_TYPE_UNKNOWN ||
		chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE) {
		if (chip->chg_status == BQ_CHG_STATUS_FULL)
			return POWER_SUPPLY_STATUS_FULL;
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
		else if ((chip->usb_present) || (chip->wlc_present))
#else
		else if (chip->usb_present)
#endif
			return POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			return POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (capacity >= 100 && batt_present)
		return POWER_SUPPLY_STATUS_FULL;

	if (chg_type == POWER_SUPPLY_CHARGE_TYPE_TRICKLE ||
		chg_type == POWER_SUPPLY_CHARGE_TYPE_FAST)
		return POWER_SUPPLY_STATUS_CHARGING;

	return POWER_SUPPLY_STATUS_DISCHARGING;
}


static int bq24192_batt_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
					struct bq24192_chip, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24192_get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq24192_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq24192_get_prop_batt_health(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq24192_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 4350 * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 4350 * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq24192_get_prop_batt_voltage_now();
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bq24192_get_prop_batt_temp(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (pseudo_batt_info.mode) {
			val->intval = pseudo_batt_info.capacity;
			break;
		}
		val->intval = bq24192_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq24192_get_prop_batt_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = bq24192_get_prop_batt_full_design(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (chip->charging_disabled) {
			val->intval = 0;
			break;
		}
#ifdef CONFIG_WIRELESS_CHARGER
#ifdef CONFIG_BQ51053B_CHARGER
		val->intval = chip->ac_online | chip->usb_online
				| chip->wlc_present;
#else
		val->intval = chip->ac_online | chip->usb_online
				| wireless_charging;
#endif
#else
		val->intval = chip->ac_online | chip->usb_online;
#endif
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		/*                                                    
                                                       
                                                            
                                                     
   */
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PSEUDO_BATT:
		val->intval = pseudo_batt_info.mode;
		break;
	case POWER_SUPPLY_PROP_EXT_PWR_CHECK:
		val->intval = lge_pm_get_cable_type();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


#ifdef CONFIG_WIRELESS_CHARGER
int set_wireless_power_supply_control(int value)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
#ifdef CONFIG_BQ51053B_CHARGER
	pr_err(" called value : %d\n", value);
#endif
	wireless_charging = value;
#ifndef CONFIG_BQ51053B_CHARGER
	power_supply_changed(&the_chip->batt_psy);
#endif
	return 0;
}
EXPORT_SYMBOL(set_wireless_power_supply_control);

/*W/R function to alternate bq24192_irq_worker n the specific case of WLC insersion or removal
  *case1. WLC PAD is detected to USB until WLC driver probe is finished completely
  *case2. BQ24192 IRQ does not happen sometimes when USB is inserted on WLC PAD  */
#ifdef CONFIG_BQ51053B_CHARGER
void set_usb_present(int value)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return;
	}

	the_chip->wlc_present = !value;
	the_chip->usb_present = value;
	wake_lock_timeout(&the_chip->uevent_wake_lock, HZ*2);
	power_supply_set_present(the_chip->usb_psy, value);
	return;
}
EXPORT_SYMBOL(set_usb_present);

int get_usb_present(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return 0;
	}

	return the_chip->usb_present;
}
EXPORT_SYMBOL(get_usb_present);

bool external_bq24192_is_charger_present(void)
{
	bool power_ok;

	if (!the_chip) {
		pr_err("called before init\n");
		return 0;
	}

	power_ok = bq24192_is_charger_present(the_chip);

	if (power_ok)
		pr_err("DC is present.\n");
	else
		pr_err("DC is missing.\n");

	return power_ok;

}
EXPORT_SYMBOL(external_bq24192_is_charger_present);
#endif

#endif


static int bq24192_batt_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
					struct bq24192_chip, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		bq24192_enable_charging(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		/*                                                    
                                                       
                                                            
                                                     
   */
		break;
	default:
		return -EINVAL;
	}

	power_supply_changed(&chip->batt_psy);
	return 0;
}


static int
bq24192_batt_power_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		return 1;
	default:
		break;
	}

	return 0;
}

static void bq24192_batt_external_power_changed(struct power_supply *psy)
{
	struct bq24192_chip *chip = container_of(psy,
					struct bq24192_chip, batt_psy);
	union power_supply_propval ret = {0,};

	pr_debug("\n");

	chip->usb_psy->get_property(chip->usb_psy,
			  POWER_SUPPLY_PROP_ONLINE, &ret);
	chip->usb_online = ret.intval;

	if (is_factory_cable()) {
		if (is_factory_cable_130k()) {
			pr_info("factory cable detected(130k) iLimit 500mA\n");
			bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_USB20);
		} else {
			pr_info("factory cable detected  iLimit 1500mA\n");
			bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_FACTORY);
		}
		bq24192_set_ibat_max(chip, IBAT_MIN_MA);
	} else if (chip->usb_online &&
			bq24192_is_charger_present(chip)) {
		bool usb20 = bq24192_is_usb_2_0();
		if(usb20)
			bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_USB20);
		else
			bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_USB30);
		bq24192_set_ibat_max(chip, chip->chg_current_ma);
		pr_info("usb is online! i_limt = %d\n", (usb20?500:900));
	} 
#ifdef CONFIG_MACH_MSM8974_B1_KR
	 else if (chip->ac_online &&
			bq24192_is_charger_present(chip) && !chip->thermal_mitigation) {
		chip->icl_first = true;
		bq24192_set_input_i_limit(chip, adap_tbl[0].input_limit);
		bq24192_set_ibat_max(chip, adap_tbl[0].chg_limit);
		wake_lock(&chip->icl_wake_lock);
		schedule_delayed_work(&chip->input_limit_work,
					msecs_to_jiffies(200));
		pr_info("ac is online! i_limit = %d\n",	adap_tbl[0].input_limit);
	} else if (chip->ac_online &&
			bq24192_is_charger_present(chip) && chip->thermal_mitigation) {
		bq24192_set_input_i_limit(chip,chip->input_current_ma);
		pr_info("ac is online! thermal mitigated i_limit = %d\n", chip->thermal_mitigation);
	}
#else
	else if (chip->ac_online &&
			bq24192_is_charger_present(chip)) {
		chip->icl_first = true;
		bq24192_set_input_i_limit(chip, adap_tbl[0].input_limit);
		bq24192_set_ibat_max(chip, adap_tbl[0].chg_limit);
		wake_lock(&chip->icl_wake_lock);
		schedule_delayed_work(&chip->input_limit_work,
					msecs_to_jiffies(200));
		pr_info("ac is online! i_limit = %d\n",	adap_tbl[0].input_limit);
	}
#endif
#ifdef CONFIG_WIRELESS_CHARGER
#ifdef CONFIG_BQ51053B_CHARGER
	else if (wireless_charging) {
		/*Prepare Wireless Charging */
		if(is_wireless_charger_plugged()) {
			bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_USB30);
			pr_err("[WLC] Set inuput limit HC mode\n");

			bq24192_set_ibat_max(chip, IBAT_MIN_MA);
			pr_err("[WLC] Set current HC mode and 500mA : %d\n"
					,chip->wlc_present);
		} else {
			pr_err("[WLC] GPIO SWING\n");
		}
	}
#endif
#endif
	
	chip->usb_psy->get_property(chip->usb_psy,
			  POWER_SUPPLY_PROP_SCOPE, &ret);

	if (ret.intval) {
		pr_info("usb host mode = %d\n", ret.intval);
		if ((ret.intval == POWER_SUPPLY_SCOPE_SYSTEM)
					&& !bq24192_is_otg_mode(chip)) {
			bq24192_enable_otg(chip, true);
#if defined(CONFIG_MACH_MSM8974_B1_KR)
			gpio_set_value(chip->otg_en, 1);
			pr_info("%s : otg_en = %d\n", __func__, chip->otg_en);
#endif
		}
		else if ((ret.intval == POWER_SUPPLY_SCOPE_DEVICE)
					&& bq24192_is_otg_mode(chip)) {
			bq24192_enable_otg(chip, false);
#if defined(CONFIG_MACH_MSM8974_B1_KR)
			gpio_set_value(chip->otg_en, 0);
			pr_info("%s : otg_en = %d\n", __func__, chip->otg_en);
#endif
		}
	}

	power_supply_changed(&chip->batt_psy);
}


static int bq24192_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
					struct bq24192_chip,
					ac_psy);

	switch (psp) {

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->set_chg_current_ma;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->ac_present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->ac_online;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq24192_get_prop_charge_type(chip);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24192_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
						struct bq24192_chip,
						ac_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		chip->ac_present = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		chip->ac_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		//bq24192_set_ibat_max(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(&chip->ac_psy);
	return 0;
}

static int
bq24192_power_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		return 1;
	default:
		break;
	}

	return 0;
}

static ssize_t at_chg_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r;
	bool b_chg_ok = false;
	int chg_type;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	chg_type = bq24192_get_prop_charge_type(the_chip);
	if (chg_type != POWER_SUPPLY_CHARGE_TYPE_NONE) {
		b_chg_ok = true;
		r = snprintf(buf, 3, "%d\n", b_chg_ok);
		pr_info("[Diag] true ! buf = %s, charging=1\n", buf);
	} else {
		b_chg_ok = false;
		r = snprintf(buf, 3, "%d\n", b_chg_ok);
		pr_info("[Diag] false ! buf = %s, charging=0\n", buf);
	}

	return r;
}

static ssize_t at_chg_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;

	if (!count) {
		pr_err("[Diag] count 0 error\n");
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (strncmp(buf, "0", 1) == 0) {
		/* stop charging */
		pr_info("[Diag] stop charging start\n");
		ret = bq24192_enable_charging(the_chip, false);

	} else if (strncmp(buf, "1", 1) == 0) {
		/* start charging */
		pr_info("[Diag] start charging start\n");
		ret = bq24192_enable_charging(the_chip, true);
	}

	if (ret)
		return -EINVAL;

	return 1;
}

static ssize_t at_chg_complete_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int guage_level = 0;
	int r = 0;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	guage_level = bq24192_get_prop_batt_capacity(the_chip);

	if (guage_level == 100) {
		r = snprintf(buf, 3, "%d\n", 0);
		pr_info("[Diag] buf = %s, gauge==100\n", buf);
	} else {
		r = snprintf(buf, 3, "%d\n", 1);
		pr_info("[Diag] buf = %s, gauge<=100\n", buf);
	}

	return r;
}

static ssize_t at_chg_complete_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;

	if (!count) {
		pr_err("[Diag] count 0 error\n");
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (strncmp(buf, "0", 1) == 0) {
		/* charging not complete */
		pr_info("[Diag] charging not complete start\n");
		ret = bq24192_enable_charging(the_chip, true);
	} else if (strncmp(buf, "1", 1) == 0) {
		/* charging complete */
		pr_info("[Diag] charging complete start\n");
		ret = bq24192_enable_charging(the_chip, false);
	}

	if (ret)
		return -EINVAL;

	return 1;
}

static ssize_t at_pmic_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool pm_reset = true;

	msleep(3000); /* for waiting return values of testmode */

	machine_restart(NULL);

	r = snprintf(buf, 3, "%d\n", pm_reset);

	return r;
}
static ssize_t at_otg_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int otg_mode;
	int r = 0;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	otg_mode = bq24192_is_otg_mode(the_chip);
	if(otg_mode) {
		otg_mode = 1;
		r = snprintf(buf, 3, "%d\n", otg_mode);
		pr_info("[Diag] true ! buf = %s, OTG Enabled\n", buf);
	}
	else {
		otg_mode = 0;
		r = snprintf(buf, 3, "%d\n", otg_mode);
		pr_info("[Diag] false ! buf = %s, OTG Disabled\n", buf);
	}
	return r;
}

static ssize_t at_otg_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;

	if (!count) {
		pr_err("[Diag] count 0 error\n");
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (strncmp(buf, "0", 1) == 0) {
		pr_info("[Diag] OTG Disable start\n");
		if(bq24192_is_otg_mode(the_chip))
			ret = bq24192_enable_otg(the_chip, false);

	} else if (strncmp(buf, "1", 1) == 0) {
		pr_info("[Diag] OTG Enable start\n");
		if(!bq24192_is_otg_mode(the_chip))
			ret = bq24192_enable_otg(the_chip, true);
	}

	if(ret)
		return -EINVAL;
	return 1;
}
DEVICE_ATTR(at_charge, 0644, at_chg_status_show, at_chg_status_store);
DEVICE_ATTR(at_chcomp, 0644, at_chg_complete_show, at_chg_complete_store);
DEVICE_ATTR(at_pmrst, 0640, at_pmic_reset_show, NULL);
DEVICE_ATTR(at_otg, 0644, at_otg_status_show, at_otg_status_store);

int pseudo_batt_set(struct pseudo_batt_info_type *info)
{
	struct bq24192_chip *chip = the_chip;
	pr_err("pseudo_batt_set\n");
	pseudo_batt_info.mode = info->mode;
	pseudo_batt_info.id = info->id;
	pseudo_batt_info.therm = info->therm;
	pseudo_batt_info.temp = info->temp;
	pseudo_batt_info.volt = info->volt;
	pseudo_batt_info.capacity = info->capacity;
	pseudo_batt_info.charging = info->charging;

	power_supply_changed(&chip->batt_psy);

	return 0;
}
EXPORT_SYMBOL(pseudo_batt_set);

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
struct charging_current_ma_entry {
	int	ibat;
	int 	adjust_ibat;
};
static struct charging_current_ma_entry adjust_ibat_ma_table[] = {
	{100, 500},
	{200, 1000},
	{300, 1500},
	{400, 2000},
};
static int bq24192_set_adjust_ibat(struct bq24192_chip *chip, int ma)
{
	int set_ibat = 0;
	bool is_charger = bq24192_is_charger_present(chip);

	if (!is_charger)
		return 0;

	if (chip->ac_online && ma < IBAT_MIN_MA) {
		int i;
		for (i = 0; i < ARRAY_SIZE(adjust_ibat_ma_table); i++) {
			if (adjust_ibat_ma_table[i].ibat == ma) {
				set_ibat = adjust_ibat_ma_table[i].adjust_ibat;
				bq24192_set_ibat_max(chip, set_ibat);
				if (!chip->force_ichg_20pct) {
					bq24192_force_ichg_decrease(chip, 1);
					chip->force_ichg_20pct = 1;
				}
				break;
			}
		}
	}  else {
		if (chip->ac_online && ma >= IBAT_MIN_MA) {
			if(ma == chip->chg_current_ma)
				set_ibat = adap_tbl[chip->set_icl_idx].chg_limit;
			 else
				set_ibat = ma;
			bq24192_set_ibat_max(chip, set_ibat);
			if (chip->force_ichg_20pct) {
				bq24192_force_ichg_decrease(chip, 0);
				chip->force_ichg_20pct = 0;
			}
		}

		if (chip->usb_online && chip->force_ichg_20pct) {
			bq24192_force_ichg_decrease(chip, 0);
			chip->force_ichg_20pct = 0;
			return 0;
		}
	}

	pr_err("thermal-engine set_ibat = %d force_ichg_20pct = %d\n",
			set_ibat, chip->force_ichg_20pct);
	return 0;
}
#endif

static int bq24192_thermal_mitigation;
static int
bq24192_set_thermal_chg_current_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	if (!the_chip) {
		pr_err("called before init\n");
		return ret;
	}

	if (is_factory_cable()) {
		pr_err("plugged factory cable\n");
		return 0;
	}

#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	pr_err("thermal-engine set chg current to %d\n",
			bq24192_thermal_mitigation);

	the_chip->chg_current_te = bq24192_thermal_mitigation;
#ifdef CONFIG_MACH_MSM8974_B1_KR
	if(the_chip->chg_current_te < 1500) {
		pr_info("thermal mitigation! %d\n", the_chip->chg_current_te);
		the_chip->thermal_mitigation = 1;
		}
	else {
		pr_info("thermal mitigation CLEAR! %d\n", the_chip->chg_current_te);
		the_chip->thermal_mitigation = 0;
		}
#endif
	cancel_delayed_work_sync(&the_chip->battemp_work);
	schedule_delayed_work(&the_chip->battemp_work, HZ*1);
#else
	pr_err("thermal-engine chg current control not enabled\n");
#endif
	return 0;
}
module_param_call(bq24192_thermal_mitigation, bq24192_set_thermal_chg_current_set,
	param_get_uint, &bq24192_thermal_mitigation, 0644);

static int temp_before = 0;
static void bq24192_monitor_batt_temp(struct work_struct *work)
{
	struct bq24192_chip *chip =
		container_of(work, struct bq24192_chip, battemp_work.work);
	struct charging_info req;
	struct charging_rsp res;
	bool is_changed = false;
	union power_supply_propval ret = {0,};

	if (chip->chg_timeout) {
		int ret;

		pr_err("escape charging timeout, charging enable and unlocked\n");
		chip->chg_timeout = false;
		ret = bq24192_enable_charging(chip, true);
		if (ret)
			pr_err("Failed to set CHG_CONFIG ret=%d\n", ret);

		wake_unlock(&chip->chg_timeout_lock);
	}

	chip->batt_psy.get_property(&(chip->batt_psy),
			  POWER_SUPPLY_PROP_TEMP, &ret);
	req.batt_temp = ret.intval / 10;

	chip->batt_psy.get_property(&(chip->batt_psy),
			  POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
	req.batt_volt = ret.intval;

	chip->batt_psy.get_property(&(chip->batt_psy),
			  POWER_SUPPLY_PROP_CURRENT_NOW, &ret);
	req.current_now = ret.intval / 1000;

#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	req.chg_current_ma = chip->chg_current_ma;
	req.chg_current_te = chip->chg_current_te;
#ifdef CONFIG_MACH_MSM8974_B1_KR
	chip->input_current_ma = chip->chg_current_te;
	if(chip->prev_input_current_ma != chip->input_current_ma) {
		pr_info("Input current(%d) was setted!!\n", chip->chg_current_te);
		bq24192_set_input_i_limit(chip, chip->input_current_ma);
		chip->prev_input_current_ma = chip->input_current_ma;
		}
#endif
#endif

	req.is_charger = bq24192_is_charger_present(chip);

	lge_monitor_batt_temp(req, &res);

	if (((res.change_lvl != STS_CHE_NONE) && req.is_charger) ||
		(res.force_update == true)) {
		if (res.change_lvl == STS_CHE_NORMAL_TO_DECCUR ||
			(res.force_update == true && res.state == CHG_BATT_DECCUR_STATE &&
			res.dc_current != DC_CURRENT_DEF
#if defined(CONFIG_MACH_MSM8974_Z_SPR) || defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_B1_KR)
			&& res.change_lvl != STS_CHE_STPCHG_TO_DECCUR
#endif
			)) {
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			bq24192_set_adjust_ibat(chip, res.dc_current);
#else
			bq24192_set_ibat_max(chip, res.dc_current);
#endif
		} else if (res.change_lvl == STS_CHE_NORMAL_TO_STPCHG ||
			(res.force_update == true &&
			res.state == CHG_BATT_STPCHG_STATE)) {
			wake_lock(&chip->lcs_wake_lock);
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			bq24192_set_adjust_ibat(chip, chip->chg_current_ma);
#else
			bq24192_set_ibat_max(chip, chip->chg_current_ma);
#endif
			bq24192_enable_charging(chip, !res.disable_chg);
		} else if (res.change_lvl == STS_CHE_DECCUR_TO_NORAML) {
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			bq24192_set_adjust_ibat(chip,res.dc_current);
#else
			bq24192_set_ibat_max(chip,chip->chg_current_ma);
#endif
		} else if (res.change_lvl == STS_CHE_DECCUR_TO_STPCHG) {
			wake_lock(&chip->lcs_wake_lock);
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			bq24192_set_adjust_ibat(chip,chip->chg_current_ma);
#endif
			bq24192_enable_charging(chip, !res.disable_chg);
		} else if (res.change_lvl == STS_CHE_STPCHG_TO_NORMAL) {
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			bq24192_set_adjust_ibat(chip,res.dc_current);
#endif
			bq24192_enable_charging(chip, !res.disable_chg);
			wake_unlock(&chip->lcs_wake_lock);
		}
#if defined(CONFIG_MACH_MSM8974_Z_SPR) || defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_B1_KR)
		else if (res.change_lvl == STS_CHE_STPCHG_TO_DECCUR) {
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			bq24192_set_adjust_ibat(chip,res.dc_current);
#endif
			bq24192_enable_charging(chip, !res.disable_chg);
			wake_unlock(&chip->lcs_wake_lock);
		}
#endif
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
		else if (res.force_update == true && res.state == CHG_BATT_NORMAL_STATE &&
			res.dc_current != DC_CURRENT_DEF) {
			bq24192_set_adjust_ibat(chip,res.dc_current);
		}
#endif
		//chip->pseudo_ui_chg = res.pseudo_chg_ui;
	}

	if (chip->pseudo_ui_chg ^ res.pseudo_chg_ui) {
		is_changed = true;
		chip->pseudo_ui_chg = res.pseudo_chg_ui;
	}

	if (chip->btm_state ^ res.btm_state) {
		is_changed = true;
		chip->btm_state = res.btm_state;
	}

	if (temp_before != req.batt_temp) {
		is_changed = true;
		temp_before = req.batt_temp;
	}

	if(is_changed == true)
		power_supply_changed(&chip->batt_psy);

	bq24192_reginfo();

	schedule_delayed_work(&chip->battemp_work,
		MONITOR_BATTEMP_POLLING_PERIOD);
}
#endif

static ssize_t batt_removed_print_name(struct switch_dev *sdev, char *buf){
	return sprintf(buf, "%s\n","battery_removed");
}

static ssize_t batt_removed_print_state(struct switch_dev *sdev, char *buf){
	return sprintf(buf, "%d\n",sdev->state);
}

static int bq24192_create_debugfs_entries(struct bq24192_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir(BQ24192_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("bq24192 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(bq24192_debug_regs) ; i++) {
		char *name = bq24192_debug_regs[i].name;
		u32 reg = bq24192_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, chip->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static void bq24192_remove_debugfs_entries(struct bq24192_chip *chip)
{
	if (chip->dent)
		debugfs_remove_recursive(chip->dent);
}

static int __devinit bq24192_init_batt_psy(struct bq24192_chip *chip)
{
	int ret;

	chip->batt_psy.name = "battery";
	chip->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.properties = bq24192_batt_power_props;
	chip->batt_psy.num_properties =
					ARRAY_SIZE(bq24192_batt_power_props);
	chip->batt_psy.get_property = bq24192_batt_power_get_property;
	chip->batt_psy.set_property = bq24192_batt_power_set_property;
	chip->batt_psy.property_is_writeable =
					bq24192_batt_power_property_is_writeable;
	chip->batt_psy.external_power_changed =
					bq24192_batt_external_power_changed;

	ret = power_supply_register(&chip->client->dev,
				&chip->batt_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int __devinit bq24192_init_ac_psy(struct bq24192_chip *chip)
{
	int ret = 0;

	chip->ac_psy.name = "ac";
	chip->ac_psy.type = POWER_SUPPLY_TYPE_MAINS;
	chip->ac_psy.supplied_to = bq24192_power_supplied_to;
	chip->ac_psy.num_supplicants = ARRAY_SIZE(bq24192_power_supplied_to);
	chip->ac_psy.properties = bq24192_power_props;
	chip->ac_psy.num_properties = ARRAY_SIZE(bq24192_power_props);
	chip->ac_psy.get_property = bq24192_power_get_property;
	chip->ac_psy.set_property = bq24192_power_set_property;
	chip->ac_psy.property_is_writeable =
				bq24192_power_property_is_writeable;
	ret = power_supply_register(&chip->client->dev,
				&chip->ac_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int bq24192_hw_init(struct bq24192_chip *chip)
{
	int ret = 0;

	ret = bq24192_set_input_vin_limit(chip, chip->vin_limit_mv);
	if (ret) {
		pr_err("failed to set input voltage limit\n");
		return ret;
	}

	ret = bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_TA);
	if (ret) {
		pr_err("failed to set input current limit\n");
		return ret;
	}

	ret = bq24192_set_system_vmin(chip, chip->sys_vmin_mv);
	if (ret) {
		pr_err("failed to set system min voltage\n");
		return ret;
	}

	ret = bq24192_set_ibat_max(chip, chip->chg_current_ma);
	if (ret) {
		pr_err("failed to set charging current\n");
		return ret;
	}

	ret = bq24192_force_ichg_decrease(chip, 0);
	if (ret) {
		pr_err("failed to set charging current as reg[ICHG] programmed\n");
		return ret;
	}

	ret = bq24192_set_prechg_i_limit(chip, chip->pre_chg_current_ma);
	if (ret) {
		pr_err("failed to set pre-charge current\n");
		return ret;
	}

	ret = bq24192_set_term_current(chip, chip->term_current_ma);
	if (ret) {
		pr_err("failed to set charge termination current\n");
		return ret;
	}

	ret = bq24192_set_vbat_max(chip, chip->vbat_max_mv);
	if (ret) {
		pr_err("failed to set vbat max\n");
		return ret;
	}

	if (factory_mode) {
		ret = bq24192_set_chg_term(chip, 0);
		if (ret) {
			pr_err("failed to disable chg termination\n");
			return ret;
		}
	}
	else {
		ret = bq24192_set_chg_term(chip, 1);
		if (ret) {
			pr_err("failed to enable chg termination\n");
			return ret;
		}

		ret = bq24192_set_chg_timer(chip, 1);
		if (ret) {
			pr_err("failed to enable chg safety timer\n");
			return ret;
		}
	}

	ret = bq24192_set_chg_timeout(chip);
	if (ret) {
		pr_err("Failed to set CHG_TIMEOUT rc=%d\n", ret);
		return ret;
	}

	ret = bq24192_set_ir_comp_resister(chip, IRCOMP_R_50_MOHM);
	if (ret) {
		pr_err("failed to set ir compensation resister\n");
		return ret;
	}

	ret = bq24192_set_vclamp_mv(chip, IRCOMP_VCLAMP_MAX_MV);
	if (ret) {
		pr_err("failed to set ir vclamp voltage\n");
		return ret;
	}

	return 0;
}

static int bq24192_parse_dt(struct device_node *dev_node,
			   struct bq24192_chip *chip)
{
	int ret = 0;

	chip->int_gpio =
		of_get_named_gpio(dev_node, "ti,int-gpio", 0);
	pr_debug("int_gpio = %d.\n", chip->int_gpio);
	if (chip->int_gpio < 0) {
		pr_err("failed to get int-gpio.\n");
		ret = chip->int_gpio;
		goto out;
	}
#if defined(CONFIG_MACH_MSM8974_B1_KR)
	chip->ext_chg_en = of_get_named_gpio(dev_node, "ti,ext-chg-en-gpio", 0);
	pr_info("ext_chg_en = %d\n", chip->ext_chg_en);
	if(chip->ext_chg_en < 0) {
		pr_err("failed to get ext_chg_en\n");
		ret = chip->ext_chg_en;
		goto out;
	}
	chip->otg_en = of_get_named_gpio(dev_node, "ti,otg-en-gpio", 0);
	pr_info("otg_en = %d\n", chip->otg_en);
	if(chip->otg_en < 0) {
		pr_err("failed to get otg_en\n");
		ret = chip->otg_en;
		goto out;
	}
#endif

	ret = of_property_read_u32(dev_node, "ti,chg-current-ma",
				   &(chip->chg_current_ma));
#ifndef CONFIG_MACH_MSM8974_B1_KR
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	chip->chg_current_te = chip->chg_current_ma;
	chip->force_ichg_20pct = 0;
#endif
#endif
	pr_debug("bq24192 chg_current_ma = %d.\n",
			chip->chg_current_ma);
	if (ret) {
		pr_err("Unable to read chg-current-ma.\n");
		return ret;
	}

#ifdef CONFIG_MACH_MSM8974_B1_KR
		ret = of_property_read_u32(dev_node, "ti,input-current-ma",
					   &(chip->input_current_ma));
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
		chip->chg_current_te = chip->input_current_ma;
#endif
		pr_debug("bq24192 input_current_ma = %d.\n",
				chip->input_current_ma);
		if (ret) {
			pr_err("Unable to read input-current-ma.\n");
			return ret;
		}
#endif

	ret = of_property_read_u32(dev_node, "ti,term-current-ma",
				   &(chip->term_current_ma));
	pr_debug("bq24192 term_current_ma = %d.\n",
				chip->term_current_ma);
	if (ret) {
		pr_err("Unable to read term-current-ma.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,vbat-max-mv",
				   &chip->vbat_max_mv);
	pr_debug("bq24192 vbat_max_mv = %d.\n",
				chip->vbat_max_mv);
	if (ret) {
		pr_err("Unable to read vbat-max-mv.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,pre-chg-current-ma",
				   &chip->pre_chg_current_ma);
	pr_debug("bq24192 pre_chg_current_ma = %d.\n",
				chip->pre_chg_current_ma);
	if (ret) {
		pr_err("Unable to read pre-chg-current-ma.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,sys-vmin-mv",
				   &chip->sys_vmin_mv);
	pr_debug("bq24192 sys_vmin_mv = %d.\n",
				chip->sys_vmin_mv);
	if (ret) {
		pr_err("Unable to read sys-vmin-mv.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,vin-limit-mv",
				   &chip->vin_limit_mv);
	pr_debug("bq24192 vin_limit_mv = %d.\n",
				chip->vin_limit_mv);
	if (ret) {
		pr_err("Unable to read vin-limit-mv.\n");
		return ret;
	}

	ret = of_property_read_u32(dev_node, "ti,icl-vbus-mv",
				   &chip->icl_vbus_mv);
	if (ret) {
		pr_err("Unable to read icl threshod voltage.\n");
		return ret;
	}

out:
	return ret;
}

int lge_get_sbl_cable_type(void)
{
	int ret_cable_type = 0;
	unsigned int *p_cable_type = (unsigned int *)(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size));

	if (p_cable_type)
		ret_cable_type = *p_cable_type;
	else
		ret_cable_type = 0;

	return ret_cable_type;
}
EXPORT_SYMBOL(lge_get_sbl_cable_type);

static int bq24192_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	const struct bq24192_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	struct bq24192_chip *chip;
	int ret = 0;

	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size));

	if (p_cable_type)
		cable_type = *p_cable_type;
	else
		cable_type = 0;	

	if (cable_type == LT_CABLE_56K || cable_type == LT_CABLE_130K ||
					cable_type == LT_CABLE_910K)
		factory_mode = 1;
	 else
		factory_mode = 0;

	pr_info("cable_type is = %d factory_mode = %d\n", 
		cable_type, factory_mode);

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("i2c func fail.\n");
		return -EIO;
	}

	chip = kzalloc(sizeof(struct bq24192_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("failed to alloc memory\n");
		return -ENOMEM;
	}

	chip->client = client;

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		pr_err("usb supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	get_cable_data_from_dt(dev_node);

	if (dev_node) {
		ret = bq24192_parse_dt(dev_node, chip);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto error;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			pr_err("no platform data.\n");
			return -EINVAL;
		}

		chip->int_gpio = pdata->int_gpio;
#if defined(CONFIG_MACH_MSM8974_B1_KR)
		chip->ext_chg_en = pdata->ext_chg_en;
		chip->otg_en = pdata->otg_en;
#endif
		chip->chg_current_ma = pdata->chg_current_ma;
		chip->term_current_ma = pdata->term_current_ma;
		chip->vbat_max_mv = pdata->vbat_max_mv;
		chip->pre_chg_current_ma = pdata->pre_chg_current_ma;
		chip->sys_vmin_mv = pdata->sys_vmin_mv;
		chip->vin_limit_mv = pdata->vin_limit_mv;
		chip->icl_vbus_mv = pdata->icl_vbus_mv;
	}
	chip->set_chg_current_ma = chip->chg_current_ma;

	ret =  gpio_request_one(chip->int_gpio, GPIOF_DIR_IN,
			"bq24192_int");
	if (ret) {
		pr_err("failed to request int_gpio\n");
		goto error;
	}
	chip->irq = gpio_to_irq(chip->int_gpio);
	pr_debug("int_gpio irq#=%d.\n", chip->irq);

#if defined(CONFIG_MACH_MSM8974_B1_KR)
	ret = gpio_request_one(chip->ext_chg_en, GPIOF_DIR_OUT | GPIOF_INIT_LOW,
			"bq24192_en");
	if (ret) {
		pr_err("failed to request bq24192_en\n");
		goto error;
	}
	ret = gpio_request(chip->otg_en, "otg_en");
	if (ret) {
		printk("otg_en gpio_request failed for %d ret=%d\n",
			   chip->otg_en, ret);
		goto error;
	}
	gpio_direction_output(chip->otg_en, 0);
#endif

	i2c_set_clientdata(client, chip);

	ret = bq24192_hw_init(chip);
	if (ret) {
		pr_err("bq24192_hwinit failed.ret=%d\n",ret);
		goto err_hw_init;
	}

	wake_lock_init(&chip->chg_wake_lock,
		       WAKE_LOCK_SUSPEND, BQ24192_NAME);
	wake_lock_init(&chip->uevent_wake_lock,
		       WAKE_LOCK_SUSPEND, "bq24192_chg_uevent");
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_init(&chip->lcs_wake_lock,
			WAKE_LOCK_SUSPEND, "LGE charging scenario");
#endif
	wake_lock_init(&chip->battgone_wake_lock,
		       WAKE_LOCK_SUSPEND, "batt removed");
	wake_lock_init(&chip->chg_timeout_lock,
			       WAKE_LOCK_SUSPEND, "chg timeout");
	wake_lock_init(&chip->icl_wake_lock,
			       WAKE_LOCK_SUSPEND, "icl_wake_lock");
	chip->batt_removed.name = "battery_removed";
	chip->batt_removed.state = 0; /*if batt is removed, state will be set to 1 */
	chip->batt_removed.print_name = batt_removed_print_name;
	chip->batt_removed.print_state = batt_removed_print_state;

	ret = switch_dev_register(&chip->batt_removed);
	if (ret < 0) {
		pr_err("Failed to register switch device, battery_removed\n");
	}

	ret = bq24192_init_batt_psy(chip);
	if (ret) {
		pr_err("bq24192_init_batt_psy failed ret=%d\n", ret);
		goto err_init_batt_psy;
	}

	ret = bq24192_init_ac_psy(chip);
	if (ret) {
		pr_err("bq24192_init_ac_psy failed ret=%d\n", ret);
		goto err_init_ac_psy;
	}

	the_chip = chip;

	if (is_factory_cable()) {		
		if (is_factory_cable_130k()) {
			pr_info("factory cable detected(130k) iLimit 500mA\n");
			bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_USB20);
		} else {
			pr_info("factory cable detected  iLimit 1500mA\n");
			bq24192_set_input_i_limit(chip, INPUT_CURRENT_LIMIT_FACTORY);
			bq24192_force_ichg_decrease(chip, 1);
		}		
		bq24192_set_ibat_max(chip, IBAT_MIN_MA);
	}

	ret = bq24192_create_debugfs_entries(chip);
	if (ret) {
		pr_err("bq24192_create_debugfs_entries failed ret=%d\n", ret);
		goto err_debugfs;
	}

	INIT_DELAYED_WORK(&chip->irq_work, bq24192_irq_worker);
#if I2C_SUSPEND_WORKAROUND
	INIT_DELAYED_WORK(&chip->check_suspended_work, bq24192_check_suspended_worker);
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	INIT_DELAYED_WORK(&chip->battemp_work, bq24192_monitor_batt_temp);
#endif
	INIT_DELAYED_WORK(&chip->input_limit_work, bq24192_input_limit_worker);
	INIT_DELAYED_WORK(&chip->input_limit_exception_work,
		bq24192_input_limit_exception_worker);

	if (chip->irq) {
		ret = request_irq(chip->irq, bq24192_irq,
				#if defined (CONFIG_MACH_MSM8974_Z_OPEN_COM)
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				#else
				IRQF_TRIGGER_FALLING,
				#endif
				"bq24192_irq", chip);
		if (ret) {
			pr_err("request_irq %d failed\n", chip->irq);
			goto err_req_irq;
		}
		enable_irq_wake(chip->irq);
	}

	chip->usb_present = bq24192_is_charger_present(chip);
	power_supply_set_present(chip->usb_psy,chip->usb_present);

	bq24192_enable_charging(chip, 1);

	ret = device_create_file(&client->dev, &dev_attr_at_charge);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_charge creation failed: %d\n",
				__func__, ret);
		ret = -ENODEV;
		goto err_at_charge;
	}

	ret = device_create_file(&client->dev, &dev_attr_at_chcomp);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_chcomp creation failed: %d\n",
				__func__, ret);
		ret = -ENODEV;
		goto err_at_chcomp;
	}

	ret = device_create_file(&client->dev, &dev_attr_at_pmrst);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_pmrst;
	}

	ret = device_create_file(&client->dev, &dev_attr_at_otg);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_otg;
	}

#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	last_batt_temp = DEFAULT_TEMP;
	last_batt_current = DEFAULT_CURRENT;
#endif

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	schedule_delayed_work(&chip->battemp_work,
		MONITOR_BATTEMP_POLLING_PERIOD / 3);
#endif
	schedule_delayed_work(&chip->irq_work, msecs_to_jiffies(2000));
#ifdef CONFIG_ZERO_WAIT
	ret = zw_psy_wakeup_source_register(&chip->chg_wake_lock.ws);
	if (ret < 0)
		goto err_zw_ws_register;
#endif
#if defined(CONFIG_MACH_MSM8974_B1_KR)
	qpnp_batif_regist_batt_present(&bq24192_batt_remove_insert_cb);
#endif
	pr_info("probe success\n");

	return 0;
#ifdef CONFIG_ZERO_WAIT
err_zw_ws_register:
	device_remove_file(&client->dev, &dev_attr_at_otg);
#endif
err_at_otg:
	device_remove_file(&client->dev, &dev_attr_at_pmrst);
err_at_pmrst:
	device_remove_file(&client->dev, &dev_attr_at_chcomp);
err_at_chcomp:
	device_remove_file(&client->dev, &dev_attr_at_charge);
err_at_charge:
err_req_irq:
	bq24192_remove_debugfs_entries(chip);
err_debugfs:
	power_supply_unregister(&chip->ac_psy);
err_init_ac_psy:
	power_supply_unregister(&chip->batt_psy);
err_init_batt_psy:
	wake_lock_destroy(&chip->icl_wake_lock);
	wake_lock_destroy(&chip->chg_wake_lock);
	wake_lock_destroy(&chip->uevent_wake_lock);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_destroy(&chip->lcs_wake_lock);
#endif
	wake_lock_destroy(&chip->battgone_wake_lock);
	wake_lock_destroy(&chip->chg_timeout_lock);
err_hw_init:
	if (chip->int_gpio)
		gpio_free(chip->int_gpio);
error:
	kfree(chip);
	pr_info("fail to probe\n");
	return ret;

}

static int bq24192_remove(struct i2c_client *client)
{
	struct bq24192_chip *chip = i2c_get_clientdata(client);
#ifdef CONFIG_ZERO_WAIT
	zw_psy_wakeup_source_unregister(&chip->chg_wake_lock.ws);
#endif
	bq24192_remove_debugfs_entries(chip);
	device_remove_file(&client->dev, &dev_attr_at_charge);
	device_remove_file(&client->dev, &dev_attr_at_chcomp);
	device_remove_file(&client->dev, &dev_attr_at_pmrst);
	device_remove_file(&client->dev, &dev_attr_at_otg);

	wake_lock_destroy(&chip->chg_wake_lock);
	wake_lock_destroy(&chip->uevent_wake_lock);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_destroy(&chip->lcs_wake_lock);
#endif
	wake_lock_destroy(&chip->battgone_wake_lock);
#if defined(CONFIG_MACH_MSM8974_B1_KR)
	qpnp_batif_unregist_batt_present(0);
#endif
	wake_lock_destroy(&chip->icl_wake_lock);

	power_supply_unregister(&chip->ac_psy);
	power_supply_unregister(&chip->batt_psy);

	if (chip->irq)
		free_irq(chip->irq, chip);
	if (chip->int_gpio)
		gpio_free(chip->int_gpio);
#if defined(CONFIG_MACH_MSM8974_B1_KR)
	if (chip->otg_en)
		gpio_free(chip->otg_en);
#endif

	kfree(chip);
	return 0;
}

#ifdef CONFIG_LGE_PM
static int bq24192_suspend(struct device *dev)
{
	struct bq24192_chip *chip = dev_get_drvdata(dev);

	if (!chip) {
		pr_err(KERN_ERR "%s: called before init\n", __func__);
		return 0;
	}
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	cancel_delayed_work_sync(&chip->battemp_work);
#endif

#if I2C_SUSPEND_WORKAROUND
	chip->suspended = 1;
#endif

	return 0;
}

static int bq24192_resume(struct device *dev)
{
	struct bq24192_chip *chip = dev_get_drvdata(dev);

	if (!chip) {
		pr_err(KERN_ERR "%s: called before init\n", __func__);
		return 0;
	}

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	schedule_delayed_work(&chip->battemp_work, HZ*10);
#endif

#if I2C_SUSPEND_WORKAROUND
	chip->suspended = 0;
#endif

	return 0;
}

static const struct dev_pm_ops bq24192_pm_ops = {
	.suspend	= bq24192_suspend,
	.resume		= bq24192_resume,
};
#endif

static const struct i2c_device_id bq24192_id[] = {
	{BQ24192_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq24192_id);

static const struct of_device_id bq24192_match[] = {
	{ .compatible = "ti,bq24192-charger", },
	{ },
};

static struct i2c_driver bq24192_driver = {
	.driver	= {
			.name	= BQ24192_NAME,
			.owner	= THIS_MODULE,
			.of_match_table = of_match_ptr(bq24192_match),
#ifdef CONFIG_LGE_PM
			.pm		= &bq24192_pm_ops,
#endif
	},
	.probe		= bq24192_probe,
	.remove		= bq24192_remove,
	.id_table	= bq24192_id,
};

static int __init bq24192_init(void)
{
	return i2c_add_driver(&bq24192_driver);
}
module_init(bq24192_init);

static void __exit bq24192_exit(void)
{
	return i2c_del_driver(&bq24192_driver);
}
module_exit(bq24192_exit);

MODULE_DESCRIPTION("Driver for BQ24192 charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" BQ24192_NAME);
