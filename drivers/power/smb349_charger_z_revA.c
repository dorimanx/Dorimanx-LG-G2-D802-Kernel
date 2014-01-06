/* Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
#include <linux/i2c/smb349_charger.h>
#include <linux/power_supply.h>
#include <linux/of_gpio.h>
#include <mach/board_lge.h>
#ifdef CONFIG_MAX17050_FUELGAUGE
#include <linux/max17050_battery.h>
#endif
#include <linux/max17048_battery.h>
#include <linux/qpnp/qpnp-adc.h>
#include "../../arch/arm/mach-msm/smd_private.h"
#include <linux/usb/otg.h>
#include "../usb/dwc3/dwc3_otg.h"
#include "../usb/dwc3/core.h"
#include <linux/reboot.h>

#ifdef CONFIG_WIRELESS_CHARGER
#ifdef CONFIG_BQ51053B_CHARGER
#include <linux/power/bq51053b_charger.h>
#endif
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#include <mach/lge_charging_scenario.h>
#define MONITOR_BATTEMP_POLLING_PERIOD          (60*HZ)
#endif

#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
#include <linux/qpnp/qpnp-temp-alarm.h>
#endif

#define SMB349_MASK(BITS, POS)  ((unsigned char)(((1 << BITS) - 1) << POS))

/* Register definitions */
#define CHG_CURRENT_REG                         0x00
#define CHG_OTHER_CURRENT_REG                   0x01
#define VAR_FUNC_REG                            0x02
#define FLOAT_VOLTAGE_REG                       0x03
#define CHG_CTRL_REG                            0x04
#define STAT_TIMER_REG                          0x05
#define PIN_ENABLE_CTRL_REG                     0x06
#define THERM_CTRL_A_REG                        0x07
#define CTRL_FUNCTIONS_REG                      0x09
#define OTG_TLIM_THERM_CNTRL_REG                0x0A
#define HARD_SOFT_LIMIT_CELL_TEMP_MONITOR_REG   0x0B
#define FAULT_IRQ_REG                           0x0C
#define STATUS_IRQ_REG                          0x0D
#define SYSOK_USB3_SELECT_REG                   0x0E
#define FLEX_CHARGE_REG                         0x10
#define STATUS_INT_REG                          0x11
#define I2C_BUS_SLAVE_ADDR_REG                  0x12
#define CMD_A_REG                               0x30
#define CMD_B_REG                               0x31
#define CMD_C_REG                               0x33
#define IRQ_A_REG                               0x35
#define IRQ_B_REG                               0x36
#define IRQ_C_REG                               0x37
#define IRQ_D_REG                               0x38
#define IRQ_E_REG                               0x39
#define IRQ_F_REG                               0x3A
#define STATUS_A_REG                            0x3B
#define STATUS_B_REG                            0x3C
#define STATUS_C_REG                            0x3D
#define STATUS_D_REG                            0x3E
#define STATUS_E_REG                            0x3F

/* Status bits and masks */
#define CHG_STATUS_MASK                         SMB349_MASK(2, 1)
#define CHG_ENABLE_STATUS_BIT                   BIT(0)

/* Control bits and masks */
#define FAST_CHG_CURRENT_MASK                   SMB349_MASK(4, 4)
#define AC_INPUT_CURRENT_LIMIT_MASK             SMB349_MASK(4, 0)
#define PRE_CHG_CURRENT_MASK                    SMB349_MASK(3, 5)
#define TERMINATION_CURRENT_MASK                SMB349_MASK(3, 2)
#define PRE_CHG_TO_FAST_CHG_THRESH_MASK         SMB349_MASK(2, 6)
#define FLOAT_VOLTAGE_MASK                      SMB349_MASK(6, 0)
#define EN_PIN_CTRL_MASK                        SMB349_MASK(2, 5)
#define OTG_OC_LIMIT_MASK						SMB349_MASK(2, 6)
#define OTG_BATT_UV_MASK       					SMB349_MASK(2, 4)
#define COMPETE_CHG_TIMEOUT_BIT                 SMB349_MASK(2, 2)
#define PRE_CHG_TIMEOUT_BIT                 	SMB349_MASK(2, 0)
#define OTG_CURR_LIMIT_MASK						SMB349_MASK(2, 2)
#define CHG_TIMEOUT_BIT                         BIT(7)
#define USB_OTG_EN_BIT                          BIT(4)
#define CHG_ENABLE_BIT                          BIT(1)
#define VOLATILE_W_PERM_BIT                     BIT(7)
#define USB_SELECTION_BIT                       BIT(1)
#define SYSTEM_FET_ENABLE_BIT                   BIT(7)
#define AUTOMATIC_INPUT_CURR_LIMIT_BIT          BIT(4)
#define AUTOMATIC_POWER_SOURCE_DETECTION_BIT    BIT(2)
#define BATT_OV_END_CHG_BIT                     BIT(1)
#define VCHG_FUNCTION                           BIT(0)
#define CURR_TERM_END_CHG_BIT                   BIT(6)
#define USB_2_3_SEL_BIT                         BIT(2)
#define USB_5_1_MODE_BIT                        BIT(1)
#define USB_HC_MODE_BIT                         BIT(0)
#define USB_CS_BIT                              BIT(4)
#define BATT_MISSING_STATUS_BIT                 BIT(4)
#define INOK_BIT                                BIT(2)
#define MISSING_BATT_BIT                        BIT(1)
#define FAST_CHARGE_SET_BIT                     BIT(6)
#define POWER_OK_BIT                            BIT(0)
#define PRE_CHG_TO_FAST_CHG_THRESH_BIT          BIT(1)
#define BOOST_BACK_PREVENTION_BIT               BIT(5)
#define OTG_BATT_FAIL_UVLO_BIT                  BIT(5)
#define OTG_OVER_CURRENT_LIMIT_BIT              BIT(4)
#define TERM_TAPER_CHG_IRQ_SET_BIT              BIT(4)
#define FAST_CHG_IRQ_SET_BIT                    BIT(3)
#define INPUT_OVER_IRQ_SET_BIT                  BIT(3)
#define INPUT_UNDER_IRQ_SET_BIT                 BIT(2)
#define AICL_COMPLETE_IRQ_SET_BIT               BIT(1)
#define OPTICHG_DET_THR_BIT                     BIT(3)

#ifdef CONFIG_SMB349_VZW_FAST_CHG
#define AICL_COMPLETE_IRQ_BIT					BIT(5)
#define AICL_COMPLETE_STATUS_BIT				BIT(4)
#endif

#define SMB349_CHG_IUSB_MAX_MIN_100	100
#define SMB349_CHG_IUSB_MAX_MIN_MA	200

#define LT_CABLE_56K			6
#define LT_CABLE_130K			7
#define LT_CABLE_910K			11

enum smb349_chg_status {
	SMB_CHG_STATUS_NONE			= 0,
	SMB_CHG_STATUS_PRE_CHARGE	= 1,
	SMB_CHG_STATUS_FAST_CHARGE	= 2,
	SMB_CHG_STATUS_TAPER_CHARGE	= 3,
	SMB_CHG_STATUS_EXCEPTION	= 4,
};

enum otg_current_limit {
	OTG_CURRENT_LIMIT_500		= 0,
	OTG_CURRENT_LIMIT_750,
	OTG_CURRENT_LIMIT_1000,
	OTG_CURRENT_LIMIT_MAX,
};

static enum otg_current_limit otg_limit_status = OTG_CURRENT_LIMIT_500;

#ifdef CONFIG_SMB349_VZW_FAST_CHG
enum vzw_chg_state {
	VZW_NO_CHARGER 				= 0,
	VZW_NORMAL_CHARGING			= 1,
	VZW_NOT_CHARGING			= 2,
	VZW_UNDER_CURRENT_CHARGING	= 3,
};

static enum vzw_chg_state chg_state = VZW_NO_CHARGER;

#define NOT_PRESENT 			0
#define CHARGER_PRESENT 		1
#define UNKNOWN_PRESENT 		2
#define INCOMPAT_PRESENT   		3

static int vzw_chg_present = NOT_PRESENT;

#endif

static const char * const smb349_chg_status[] = {
	"none",
	"pre-charge",
	"fast-charge",
	"taper-charge"
	"exception"
};

struct smb349_struct {
	struct	i2c_client	*client;
	struct dentry		*dent;

	int		chg_current_ma;
	int		term_current_ma;
	int		en_n_gpio;
	int		chg_susp_gpio;
	int		stat_gpio;
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
	int		otg_en_gpio;
	struct wake_lock	battgone_wake_lock;
#endif
	int		irq;

#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	int 		wlc_present;
#endif
	int		usb_present;
	int		usb_online;
	int		ac_present;
	int		ac_online;
	int		chg_type;
	int		charging_disabled;
	int		full_design;
	bool	chg_timeout;

	enum smb349_chg_status	chg_status;

	struct delayed_work		irq_work;
	struct delayed_work		polling_work;
#ifdef CONFIG_MAX17050_FUELGAUGE
	struct delayed_work		max17050_soc_work;
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	struct delayed_work		battemp_work;
	struct wake_lock		lcs_wake_lock;
	enum   lge_btm_states	btm_state;
	int						pseudo_ui_chg;
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	int						chg_current_te;
#endif
#endif
	struct wake_lock		chg_wake_lock;
	struct power_supply		dc_psy;
	struct power_supply		batt_psy;
	struct power_supply		*usb_psy;
	struct wake_lock		uevent_wake_lock;
#ifdef CONFIG_QPNP_BMS
	struct power_supply		*bms_psy;
#endif
};

#ifdef CONFIG_WIRELESS_CHARGER
static int wireless_charging;
#endif

#ifdef CONFIG_MAX17050_FUELGAUGE
/*junnyoung.jang@lge.com 20130326 Add battery condition */
static int g_batt_soc;
static int g_batt_vol;
static int g_batt_age;
static int g_batt_pre_soc;
static int g_batt_pre_vol;
static int g_batt_current;
static int pseudo_batt_age_mode;
#endif

struct chg_ma_limit_entry {
	int fast_chg_ma_limit;
	int ac_input_ma_limit;
	u8  chg_current_value;
};

#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
int batt_temp_old;
int batt_current_old;
#endif

static struct smb349_struct *the_smb349_chg;

struct debug_reg {
	char	*name;
	u8	reg;
};

#define SMB349_DEBUG_REG(x, y) {#x#y, y##_REG}

static struct debug_reg smb349_debug_regs[] = {
	SMB349_DEBUG_REG(00_, CHG_CURRENT),
	SMB349_DEBUG_REG(01_, CHG_OTHER_CURRENT),
	SMB349_DEBUG_REG(02_, VAR_FUNC),
	SMB349_DEBUG_REG(03_, FLOAT_VOLTAGE),
	SMB349_DEBUG_REG(04_, CHG_CTRL),
	SMB349_DEBUG_REG(05_, STAT_TIMER),
	SMB349_DEBUG_REG(06_, PIN_ENABLE_CTRL),
	SMB349_DEBUG_REG(07_, THERM_CTRL_A),
	SMB349_DEBUG_REG(09_, CTRL_FUNCTIONS),
	SMB349_DEBUG_REG(0A_, OTG_TLIM_THERM_CNTRL),
	SMB349_DEBUG_REG(0B_, HARD_SOFT_LIMIT_CELL_TEMP_MONITOR),
	SMB349_DEBUG_REG(0C_, FAULT_IRQ),
	SMB349_DEBUG_REG(0D_, STATUS_IRQ),
	SMB349_DEBUG_REG(0E_, SYSOK_USB3_SELECT),
	SMB349_DEBUG_REG(10_, FLEX_CHARGE),
	SMB349_DEBUG_REG(11_, STATUS_INT),
	SMB349_DEBUG_REG(12_, I2C_BUS_SLAVE_ADDR),
	SMB349_DEBUG_REG(30_, CMD_A),
	SMB349_DEBUG_REG(31_, CMD_B),
	SMB349_DEBUG_REG(33_, CMD_C),
	SMB349_DEBUG_REG(35_, IRQ_A),
	SMB349_DEBUG_REG(36_, IRQ_B),
	SMB349_DEBUG_REG(37_, IRQ_C),
	SMB349_DEBUG_REG(38_, IRQ_D),
	SMB349_DEBUG_REG(39_, IRQ_E),
	SMB349_DEBUG_REG(3A_, IRQ_F),
	SMB349_DEBUG_REG(3B_, STATUS_A),
	SMB349_DEBUG_REG(3C_, STATUS_B),
	SMB349_DEBUG_REG(3D_, STATUS_C),
	SMB349_DEBUG_REG(3E_, STATUS_D),
	SMB349_DEBUG_REG(3F_, STATUS_E),
};

int32_t smb349_is_ready(void)
{
	struct smb349_struct *smb349_chg = the_smb349_chg;

	if (!smb349_chg)
		return -EPROBE_DEFER;
	return 0;
}
EXPORT_SYMBOL(smb349_is_ready);

static unsigned int cable_type;
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

static unsigned int cable_smem_size;

static int smb349_read_reg(struct i2c_client *client, int reg,
				u8 *val)
{
	s32 ret;
	struct smb349_struct *smb349_chg;

	smb349_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_read_byte_data(smb349_chg->client, reg);
	if (ret < 0) {
		dev_err(&smb349_chg->client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int smb349_write_reg(struct i2c_client *client, int reg,
						u8 val)
{
	s32 ret;
	struct smb349_struct *smb349_chg;

	smb349_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_write_byte_data(smb349_chg->client, reg, val);
	if (ret < 0) {
		dev_err(&smb349_chg->client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int smb349_masked_write(struct i2c_client *client, int reg,
		u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = smb349_read_reg(client, reg, &temp);
	if (rc) {
		pr_err("smb349_read_reg failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = smb349_write_reg(client, reg, temp);
	if (rc) {
		pr_err("smb349_write failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	return 0;
}

#ifndef CONFIG_LGE_PM
/* Reserved for future use */
static bool smb349_is_dc_online(struct i2c_client *client)
{
	u8 irq_status_c;
	int ret, chg_status, chg_err, chg_en;

	ret = smb349_read_reg(client, STATUS_C_REG, &irq_status_c);
	if (ret) {
		pr_err("Failed to read STATUS_C_REG rc=%d\n", ret);
		return false;
	}

	chg_status = (irq_status_c & 0x06) >> 1;
	chg_err = irq_status_c & 0x40;
	chg_en = irq_status_c & 0x01;

	if ((chg_status != 0) && (chg_err == 0) && (chg_en == 1))
		return true;

	return false;
}
#endif

static int smb349_get_usbin_adc(void)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
       struct qpnp_vadc_result results;
       int rc = 0;

       if (qpnp_vadc_is_ready() == 0) {
               rc = qpnp_vadc_read(USBIN, &results);
               if (rc) {
                       pr_err("Unable to read usbin adc rc=%d\n", rc);
                       return -100;
               }
               else {
                       pr_debug("SMB DC_IN voltage: %lld\n", results.physical);
                       return results.physical;
               }
       } else {
               pr_err("vadc is not ready yet. report default current_now\n");
               return -200;
       }
#else
       pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
       return -300;
#endif
}

static bool smb349_is_charger_present(struct i2c_client *client)
{
	u8 irq_status_f;
	bool power_ok;
	int ret;
	int voltage;

	/* Normally the device is non-removable and embedded on the board.
	 * Verify that charger is present by getting I2C response.
	 * Status Register B or Interrupt Status Register F
	 */

	ret = smb349_read_reg(client, IRQ_F_REG, &irq_status_f);
	if (ret) {
		pr_err("Failed to read IRQ_STATUS_F_REG rc=%d\n", ret);
		return false;
	}

	power_ok = irq_status_f & 0x01;

	/* Power-ok , IRQ_STATUS_F_REG bit#0 */
	if (power_ok) {
		voltage = smb349_get_usbin_adc();
		pr_err("DC is present. DC_IN volt:%d\n", voltage);
	} else
		pr_err("DC is missing.\n");

	return power_ok;
}
#ifdef CONFIG_MAX17050_FUELGAUGE
bool external_smb349_is_charger_present(void)
{
	return smb349_is_charger_present(the_smb349_chg->client);
}
#endif
static int smb349_get_prop_charge_type(struct smb349_struct *smb349_chg)
{
	u8 status_c;
	enum smb349_chg_status status;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	bool chg_enabled;
	bool charger_err;
	struct i2c_client *client = smb349_chg->client;
	int ret;

	ret = smb349_read_reg(client, STATUS_C_REG, &status_c);
	if (ret) {
		pr_err("failed to read STATUS_C_REG.\n");
		chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		goto exception_handling;
	}

	chg_enabled = (bool) (status_c & 0x01);
	charger_err = (bool) (status_c & (1<<6));

	if (!chg_enabled) {
		pr_debug("Charging not enabled.\n");
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		goto exception_handling;
	}

	if (charger_err) {
		pr_warn("Charger error detected.\n");
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		goto exception_handling;
	}

	status = (status_c >> 1) & 0x3;

	if (status == SMB_CHG_STATUS_NONE)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (status == SMB_CHG_STATUS_FAST_CHARGE) /* constant current */
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (status == SMB_CHG_STATUS_TAPER_CHARGE) /* constant voltage */
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (status == SMB_CHG_STATUS_PRE_CHARGE)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	pr_debug("smb-chg-status=%d=%s.\n", status, smb349_chg_status[status]);

	if (smb349_chg->chg_status != status) { /* Status changed */
		if (status == SMB_CHG_STATUS_NONE) {
			pr_debug("Charging stopped.\n");
			wake_unlock(&smb349_chg->chg_wake_lock);
		} else {
			pr_debug("Charging started.\n");
			wake_lock(&smb349_chg->chg_wake_lock);
		}
		smb349_chg->chg_status = status;
	}

	return chg_type;

exception_handling:
	smb349_chg->chg_status = SMB_CHG_STATUS_EXCEPTION;
	if (wake_lock_active(&smb349_chg->chg_wake_lock)) {
		pr_err("exception_handling : unlock chg_wake_lock.\n");
		wake_unlock(&smb349_chg->chg_wake_lock);
	}
	return chg_type;
}

static int smb349_get_prop_batt_present(struct smb349_struct *smb349_chg)
{
	struct i2c_client *client = smb349_chg->client;
	int ret;
	u8 irq_b;
	bool batt_present;

	ret = smb349_read_reg(client, IRQ_B_REG, &irq_b);
	if (ret) {
		pr_err("failed to read IRQ_B_REG.\n");
		return 1;
	}

	batt_present = !((bool) (irq_b & BATT_MISSING_STATUS_BIT));

	pr_debug("smb349_get_prop_batt_present present=%d\n",
		batt_present ? 1 : 0);

	return batt_present ? 1 : 0;
}

#define DEFAULT_VOLTAGE		4000000
static int get_prop_batt_voltage_now_bms(void)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	int rc = 0;
	struct qpnp_vadc_result results;

	rc = qpnp_vadc_read(VBAT_SNS, &results);
	if (rc) {
		pr_err("Unable to read vbat rc=%d\n", rc);
		return 0;
	}
	return results.physical;
#else
	pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return DEFAULT_VOLTAGE;
#endif
}

#ifdef CONFIG_MAX17050_FUELGAUGE
static int get_prop_batt_voltage_now_max17050(void)
{
#ifdef CONFIG_MAX17050_FUELGAUGE
	int voltage = 0;
	voltage = max17050_get_battery_mvolts() * 1000;
	return voltage;
#else
	pr_err("CONFIG_MAX17050_FUELGAUGE is not defined.\n");
	return DEFAULT_VOLTAGE;
#endif
}
#endif
static int get_prop_batt_voltage_now_max17048(void)
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

#ifdef CONFIG_MAX17050_FUELGAUGE
/*junnyoung.jang@lge.com 20130326 Add battery condition */
void lge_pm_battery_age_update(void)
{
	if (pseudo_batt_age_mode)
		return;

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	if (lge_get_board_revno() > HW_REV_A)
		g_batt_age = max17050_get_battery_age();
	else
		g_batt_age = 100;
#else
	g_batt_age = 100;
#endif
}
int lge_pm_get_battery_age(void)
{
	if (g_batt_age == 0)
		lge_pm_battery_age_update();

	return g_batt_age;
}
int lge_pm_get_battery_condition(void)
{
	int batt_age = lge_pm_get_battery_age();
	int batt_condition = 0;

	if (batt_age == 999)
		/*Error or Uncalculated Battery age.*/
		batt_condition = 0;
	else if (batt_age >= 80)
		/*Very Good Condition*/
		batt_condition = 1;
	else if (batt_age >= 50)
		/*Good Condition*/
		batt_condition = 2;
	else if (batt_age >= 0)
		/*Bad Condition*/
		batt_condition = 3;
	else
		/*Uncalculated Battery age.*/
		batt_condition = 0;

	return batt_condition;
}

static int get_bat_age(void *data, u64 *val)
{
	*val = g_batt_age;
	return 0;
}

static int set_bat_age(void *data, u64 val)
{
	int bat_age;

	bat_age = (int) val;
	if (bat_age == -1) {
		pseudo_batt_age_mode = 0;
		lge_pm_battery_age_update();
	} else {
		pseudo_batt_age_mode = 1;
		g_batt_age = bat_age;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(bat_age_fops, get_bat_age, set_bat_age, "%llu\n");
#endif


#define DEFAULT_TEMP		250
static int smb349_get_prop_batt_temp(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	int rc = 0;
	struct qpnp_vadc_result results;

	if (is_factory_cable()) {
		pr_debug("factory cable : %d \n", DEFAULT_TEMP / 10);
		return DEFAULT_TEMP;
	}

	if (!smb349_get_prop_batt_present(smb349_chg)) {
		pr_err("Battery is missed, report default capacity\n");
		return DEFAULT_TEMP;
	}

	rc = qpnp_vadc_read(LR_MUX1_BATT_THERM, &results);
	if (rc) {
		pr_debug("Unable to read batt temperature rc=%d\n", rc);
		pr_debug("Report last_bat_temp %d again\n", batt_temp_old);
		return batt_temp_old;
	}
	else{
		pr_debug("get_bat_temp %d %lld\n", results.adc_code, results.physical);
		batt_temp_old =(int)results.physical;
		return (int)results.physical;
	}
#else
	pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return DEFAULT_TEMP;
#endif
}

static int smb349_get_prop_batt_health(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	if (smb349_chg->btm_state == BTM_HEALTH_OVERHEAT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (smb349_chg->btm_state == BTM_HEALTH_COLD)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
#else
	int batt_temp;
	batt_temp = smb349_get_prop_batt_temp(smb349_chg);

	/* TODO : implements LGE charing scenario */
	if (batt_temp >= 550)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (batt_temp <= -100)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
#endif
}

#define DEFAULT_CAPACITY	50
static int get_prop_batt_capacity_bms(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_QPNP_BMS
	union power_supply_propval ret = {0,};

	if (!smb349_get_prop_batt_present(smb349_chg)) {
		pr_err("Battery is missed, report default capacity\n");
		return DEFAULT_CAPACITY;
	}

	if (!smb349_chg->bms_psy)
		smb349_chg->bms_psy = power_supply_get_by_name("bms");

	if (smb349_chg->bms_psy) {
		smb349_chg->bms_psy->get_property(smb349_chg->bms_psy,
			  POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	} else {
		pr_debug("BMS supply is not registered.\n");
	}

	/* return default capacity to avoid userspace
	 * from shutting down unecessarily */
	return DEFAULT_CAPACITY;
#else
	pr_err("CONFIG_QPNP_BMS is not defined.\n");
	return DEFAULT_CAPACITY;
#endif
}

#ifdef CONFIG_MAX17050_FUELGAUGE
static int get_prop_batt_capacity_max17050(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_MAX17050_FUELGAUGE
	return max17050_get_battery_capacity_percent();
#else
	pr_err("CONFIG_MAX17050_FUELGAUGE is not defined.\n");
	return DEFAULT_CAPACITY;
#endif
}
#endif
static int get_prop_batt_capacity_max17048(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_MAX17048_FUELGAUGE
	return max17048_get_capacity();
#else
	pr_err("CONFIG_MAX17048_FUELGAUGE is not defined.\n");
	return DEFAULT_CAPACITY;
#endif
}

#define DEFAULT_CURRENT		200000
static int smb349_get_prop_batt_current_now(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	struct qpnp_vadc_result results;
	int rc = 0;
	int current_ma = 0;
	if (!smb349_get_prop_batt_present(smb349_chg)) {
		pr_err("Battery is missed, report default current_now\n");
		return DEFAULT_CURRENT;
	}

	/* SMB349 Vchg connected to PMIC AMUX1,
	 * Indicate Charge Current,
	 * Vchg = Ichg * 0.5ohm.
	 * adc physical result expressed micro-.
	 * will be report default value when vadc is not ready state.
	 */
	if (qpnp_vadc_is_ready() == 0) {
		rc = qpnp_vadc_read(LR_MUX4_AMUX_THM1, &results);
		if (rc) {
			pr_err("Unable to read amux_thm1 rc=%d\n", rc);
			pr_err("Report last_bat_current %d again\n",batt_current_old);
			return batt_current_old;
		}
		else {
			pr_debug("get_bat_current %d %lld\n",
				results.adc_code, results.physical * 2);
			current_ma = (int)(results.physical * 2);
			batt_current_old =current_ma ;
			return current_ma;
		}
	} else {
		pr_err("vadc is not ready yet. report default current_now\n");
		return DEFAULT_CURRENT;
	}
#else
	pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return DEFAULT_CURRENT;
#endif
}

#define DEFAULT_FULL_DESIGN	2500
static int get_prop_batt_full_design_bms(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_QPNP_BMS
	union power_supply_propval ret = {0,};

	if (!smb349_chg->bms_psy)
		smb349_chg->bms_psy = power_supply_get_by_name("bms");

	if (smb349_chg->bms_psy) {
		smb349_chg->bms_psy->get_property(smb349_chg->bms_psy,
			  POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &ret);
		return ret.intval;
	} else {
		pr_debug("BMS supply is not registered.\n");
	}

	return DEFAULT_FULL_DESIGN;
#else
	pr_err("CONFIG_QPNP_BMS is not defined.\n");
	return DEFAULT_FULL_DESIGN;
#endif
}

#ifdef CONFIG_MAX17050_FUELGAUGE
static int get_prop_batt_full_design_max17050(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_MACH_MSM8974_G2_DCM
	return 3000; /* G2_DCM FULL_DESIGN = 3000mAh */
#else
	return DEFAULT_FULL_DESIGN;
#endif
}
#endif

static int get_prop_batt_full_design_max17048(struct smb349_struct *smb349_chg)
{
#ifdef CONFIG_MAX17048_FUELGAUGE
	return max17048_get_fulldesign();
#else
	pr_err("CONFIG_MAX17048_FUELGAUGE is not defined.\n");
	return DEFAULT_FULL_DESIGN;
#endif
}

int (*smb349_get_prop_batt_capacity)(struct smb349_struct *smb349_chg);
int (*smb349_get_prop_batt_voltage_now)(void);
int (*smb349_get_prop_batt_full_design)(struct smb349_struct *smb349_chg);

struct	gauge_ic_func {
	int	(*get_prop_batt_cap_func)(struct smb349_struct *smb349_chg);
	int	(*get_prop_batt_vol_func)(void);
	int	(*get_prop_batt_fdesign_func)(struct smb349_struct *smb349_chg);
};

static struct gauge_ic_func gauge_ic_func_array[GAUGE_IC_TYPE_MAX] = {
	{
		.get_prop_batt_cap_func = get_prop_batt_capacity_max17048,
		.get_prop_batt_vol_func = get_prop_batt_voltage_now_max17048,
		.get_prop_batt_fdesign_func
					= get_prop_batt_full_design_max17048,
	},
#ifdef CONFIG_MAX17050_FUELGAUGE
	{
		.get_prop_batt_cap_func = get_prop_batt_capacity_max17050,
		.get_prop_batt_vol_func = get_prop_batt_voltage_now_max17050,
		.get_prop_batt_fdesign_func
					= get_prop_batt_full_design_max17050,
	},
#endif
	{
		.get_prop_batt_cap_func = get_prop_batt_capacity_bms,
		.get_prop_batt_vol_func = get_prop_batt_voltage_now_bms,
		.get_prop_batt_fdesign_func
					= get_prop_batt_full_design_bms,
	},
};

static int smb349_get_prop_batt_status(struct smb349_struct *smb349_chg)
{
	int chg_type = smb349_get_prop_charge_type(smb349_chg);
	int batt_present = smb349_get_prop_batt_present(smb349_chg);
	int capacity = smb349_get_prop_batt_capacity(smb349_chg);

	if (smb349_chg->usb_present && smb349_chg->pseudo_ui_chg)
		return POWER_SUPPLY_STATUS_CHARGING;

	if (chg_type == POWER_SUPPLY_CHARGE_TYPE_UNKNOWN ||
		chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE) {
#ifdef CONFIG_SMB349_VZW_FAST_CHG
			if ((smb349_chg->usb_present) || (chg_state == VZW_NOT_CHARGING))
#else
			if (smb349_chg->usb_present)
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

#define SMB349_CHG_ENABLE_SHIFT	1
static int smb349_enable_charging(struct smb349_struct *smb349_chg, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << SMB349_CHG_ENABLE_SHIFT); /* active high */

	pr_debug("enable=%d.\n", enable);

	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
						CHG_ENABLE_BIT, val);
	if (ret) {
		pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);
		return ret;
	}

	smb349_chg->charging_disabled = !enable;

	return 0;
}

int32_t external_smb349_enable_charging(bool enable)
{
	int ret;

	pr_debug("enable=%d.\n", enable);

	ret = smb349_enable_charging(the_smb349_chg, enable);

	if (ret) {
		pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
static void smb349_batt_remove_insert_cb(int batt_present)
{
	bool charger = false;
	bool ftm_cable = is_factory_cable();

	if (!the_smb349_chg) {
		pr_err("smb349 device is not exist.\n");
		return;
	}

	/* Here comes into just battery missing status only */
	wake_lock_timeout(&the_smb349_chg->battgone_wake_lock, HZ*10);

	/* makes top-half i2c time margin */
	msleep(2000);
	charger = smb349_is_charger_present(the_smb349_chg->client);

	printk(KERN_ERR "[PM] Battery inserted or removed. status(%d-%d-%d)\n",
		batt_present, charger, ftm_cable ? 1 : 0);
	if (charger && (!ftm_cable)) {
		printk(KERN_ERR "[PM] Now reset as scenario.!!\n");
		power_supply_changed(&the_smb349_chg->batt_psy);

		/* makes logger save time margin */
		msleep(3000);

		/* use oem-11 restart reason for battery remove insert irq */
		kernel_restart("oem-11");
	}
}
#endif

/* for dynamically smb349 irq debugging */
static int smb349_irq_debug;
static int smb349_irq_debug_set(const char *val, struct kernel_param *kp)
{
	int ret;

	if (!the_smb349_chg) {
		pr_err("the_smb349_chg is not initialized\n");
		return -EINVAL;
	}

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	if (smb349_irq_debug == 1) {
		ret = smb349_masked_write(the_smb349_chg->client, FAULT_IRQ_REG,
			INPUT_OVER_IRQ_SET_BIT | INPUT_UNDER_IRQ_SET_BIT,
			INPUT_OVER_IRQ_SET_BIT | INPUT_UNDER_IRQ_SET_BIT);
		if (ret) {
			pr_err("Failed to set fault_irq_reg ret=%d\n", ret);
			return -EINVAL;
		} else {
			pr_info("enable input over under irq\n");
		}
	} else if (smb349_irq_debug == 0) {
		ret = smb349_masked_write(the_smb349_chg->client, FAULT_IRQ_REG,
			INPUT_OVER_IRQ_SET_BIT | INPUT_UNDER_IRQ_SET_BIT, 0);

		if (ret) {
			pr_err("Failed to set fault_irq_reg ret=%d\n", ret);
			return -EINVAL;
		} else {
			pr_info("disable input over under irq, AICL complete irq\n");
		}
	} else {
		pr_err("unknown arguments\n");
		smb349_irq_debug = 0;
		return -EINVAL;
	}

	return 0;
}
module_param_call(smb349_irq_debug, smb349_irq_debug_set, param_get_int,
		&smb349_irq_debug, 0644);

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
static int smb349_thermal_mitigation;
static int
smb349_set_thermal_chg_current_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return ret;
	}
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	pr_err("thermal-engine set chg current to %d\n",
			smb349_thermal_mitigation);
	the_smb349_chg->chg_current_te = smb349_thermal_mitigation;

	cancel_delayed_work_sync(&the_smb349_chg->battemp_work);
	schedule_delayed_work(&the_smb349_chg->battemp_work, HZ*1);
#else
	pr_err("thermal-engine chg current control not enabled\n");
#endif
	return 0;
}
module_param_call(smb349_thermal_mitigation, smb349_set_thermal_chg_current_set,
	param_get_uint, &smb349_thermal_mitigation, 0644);
#endif

struct input_current_ma_limit_entry {
	int	icl_ma;
	u8	value;
};

static struct input_current_ma_limit_entry icl_ma_table[] = {
	{500, 0x0},
	{900, 0x1},
	{1000, 0x2},
	{1100, 0x3},
	{1200, 0x4},
	{1300, 0x5},
	{1500, 0x6},
	{1600, 0x7},
	{1700, 0x8},
	{1800, 0x9},
	{2000, 0xA},
	{2200, 0xB},
	{2400, 0xC},
	{2500, 0xD},
	{3000, 0xE},
	{3500, 0xF},
};

static void smb349_chg_timeout(bool chg_en)
{
	int ret;

	cancel_delayed_work_sync(&the_smb349_chg->battemp_work);

	ret = smb349_enable_charging(the_smb349_chg, chg_en);
	if (ret)
		pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);

	the_smb349_chg->chg_timeout = true;

	schedule_delayed_work(&the_smb349_chg->battemp_work,
		MONITOR_BATTEMP_POLLING_PERIOD);
}

static int smb349_aicl_result(u8 value)
{
	int i;

	for (i = ARRAY_SIZE(icl_ma_table) - 1; i >= 0; i--) {
		if (icl_ma_table[i].value == value)
			break;
	}

	if (i < 0) {
		pr_err("can't find %d in icl_ma_table. Use min.\n", value);
		return -1;
	}

	return icl_ma_table[i].icl_ma;
}
/* When the status bit of a certain condition is read,
 * the corresponding IRQ signal is cleared.
 */
static int smb349_clear_irq(struct i2c_client *client)
{
	u8 val;
	int ret;

	ret = smb349_read_reg(client, IRQ_A_REG, &val);
	if (ret < 0) {
		pr_err("Failed to read IRQ_A_REG rc=%d\n", ret);
		return ret;
	}
	ret = smb349_read_reg(client, IRQ_B_REG, &val);
	if (ret < 0) {
		pr_err("Failed to read IRQ_B_REG rc=%d\n", ret);
		return ret;
	}
	if (smb349_irq_debug && val != 0) {
		pr_info("36h %02X\n", val);
		if (val & BIT(7))
			pr_info("battery over voltage irq\n");
		if (val & BIT(6))
			pr_info("battery over voltage status\n");
		if (val & BIT(5))
			pr_info("missing battery irq\n");
		if (val & BIT(4))
			pr_info("missing battery status\n");
		if (val & BIT(3))
			pr_info("low battery voltage irq\n");
		if (val & BIT(2))
			pr_info("low battery voltage status\n");
		if (val & BIT(1))
			pr_info("pre-to-fast chg irq\n");
		if (val & BIT(0))
			pr_info("pre-to-fast chg status\n");
	} else {
		if (val & BIT(1))
			pr_info("pre-to-fast chg irq\n");
	}

	ret = smb349_read_reg(client, IRQ_C_REG, &val);
	if (ret < 0) {
		pr_err("Failed to read IRQ_C_REG rc=%d\n", ret);
		return ret;
	}
	if (smb349_irq_debug && val != 0) {
		pr_info("37h %02X\n", val);
		if (val & BIT(7))
			pr_info("internal temp irq\n");
		if (val & BIT(6))
			pr_info("internal temp status\n");
		if (val & BIT(5))
			pr_info("re-charge battery thresh irq\n");
		if (val & BIT(4))
			pr_info("re-charge battery thresh status\n");
		if (val & BIT(3))
			pr_info("tapper charging mode irq\n");
		if (val & BIT(2))
			pr_info("tapper charging mode status\n");
		if (val & BIT(1))
			pr_info("term charging current hit irq\n");
		if (val & BIT(0))
			pr_info("term charging current hit status\n");
	} else {
		if (val & BIT(5))
			pr_info("re-charge battery thresh irq\n");
		if (val & BIT(3))
			pr_info("tapper charging mode irq\n");
		if (val & BIT(1))
			pr_info("term charging current hit irq\n");
	}

	ret = smb349_read_reg(client, IRQ_D_REG, &val);
	if (ret < 0) {
		pr_err("Failed to read IRQ_D_REG rc=%d\n", ret);
		return ret;
	}
	if ( (val & (BIT(0) | BIT(2) | BIT(4) | BIT(5))) ) {
		if ( (val & (BIT(0) | BIT(2))) )
			smb349_chg_timeout(0);

		ret = smb349_read_reg(client, STATUS_E_REG, &val);
		if (ret < 0) {
			pr_err("Failed to AICL result rc=%d\n", ret);
			return ret;
		}
		val &= SMB349_MASK(4,0);
		pr_info("AICL result : %dmA(0x%02X)\n",
				smb349_aicl_result(val), val);
	}

	ret = smb349_read_reg(client, IRQ_E_REG, &val);
	if (ret < 0) {
		pr_err("Failed to read IRQ_E_REG rc=%d\n", ret);
		return ret;
	}
	if (smb349_irq_debug && val != 0) {
		pr_info("39h %02X\n", val);
		if (val & BIT(7))
			pr_info("DC-in over voltage IRQ\n");
		if (val & BIT(6))
			pr_info("DC-in over voltage status\n");
		if (val & BIT(5))
			pr_info("DC-in under voltage IRQ\n");
		if (val & BIT(4))
			pr_info("DC-in under voltage status\n");
	}

	ret = smb349_read_reg(client, IRQ_F_REG, &val);
	if (ret < 0) {
		pr_err("Failed to read IRQ_F_REG rc=%d\n", ret);
		return ret;
	}
	if (smb349_irq_debug && val != 0) {
		pr_info("3Ah %02X\n", val);
		if (val & BIT(1))
			pr_info("Power ok irq\n");
		if (val & BIT(0))
			pr_info("Power ok status\n");
	}
	return 0;
}

static int smb349_set_otg_current_limit(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	ret = smb349_masked_write(smb349_chg->client, OTG_TLIM_THERM_CNTRL_REG,
						OTG_CURR_LIMIT_MASK, val);
	if (ret) {
		pr_err("Failed to set OTG_TLIM_THERM_CNTRL_REG rc=%d\n", ret);
		return ret;
	}
	return 0;
}

static void smb349_set_otg_enable_bit(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
			USB_OTG_EN_BIT, val);
	if (ret) {
		pr_err("Failed to set CMD_A_REG rc= %d\n", ret);
		return;
	}

	pr_debug("otg enable bit is set.\n");
}

static int smb349_chg_is_otg_active(struct smb349_struct *smb349_chg)
{
	u8 val;
	int ret;

	ret = smb349_read_reg(smb349_chg->client, CMD_A_REG, &val);
	if (ret) {
		pr_err("Failed to read CMD_A_REG = %d\n", ret);
		return 0;
	}

	if (val & USB_OTG_EN_BIT)
		return 1;

	pr_debug("usb otg disabled.\n");
	return 0;
}

/*
 * Increase otg current limit step by step.
 * 500mA -> 750mA -> 1000mA
 */
static void smb349_change_otg_current_limit(struct smb349_struct *smb349_chg)
{
	int ret;

	if (otg_limit_status == OTG_CURRENT_LIMIT_500) {
		ret = smb349_set_otg_current_limit(smb349_chg, 0x08);
		if (ret) {
			pr_err("Failed to set smb349_set_otg_current_limit rc=%d\n", ret);
			return;
		}
		otg_limit_status = OTG_CURRENT_LIMIT_750;
		pr_info("OTG_CURRENT_LIMIT_750\n");
	} else if (otg_limit_status == OTG_CURRENT_LIMIT_750) {
		ret = smb349_set_otg_current_limit(smb349_chg, 0x0c);
		if (ret) {
			pr_err("Failed to set smb349_set_otg_current_limit rc=%d\n", ret);
			return;
		}
		otg_limit_status = OTG_CURRENT_LIMIT_1000;
		pr_info("OTG_CURRENT_LIMIT_1000\n");
	} else {
		otg_limit_status = OTG_CURRENT_LIMIT_MAX;
		pr_info("OTG_CURRENT_LIMIT_MAX\n");
		return;
	}
	schedule_delayed_work(&smb349_chg->polling_work, msecs_to_jiffies(500));
}

#ifdef CONFIG_MAX17050_FUELGAUGE
static int check_fuel_gauage_update_data(void)
{
	int val;
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	if (lge_get_board_revno() > HW_REV_A) {
		g_batt_vol = max17050_get_battery_mvolts();
		g_batt_soc = max17050_get_battery_capacity_percent();
		g_batt_current = max17050_get_battery_current();
		/* battery age update add */
	}
#else
	{
		g_batt_vol = max17050_get_battery_mvolts();
		g_batt_soc = max17050_get_battery_capacity_percent();
		/* battery age update add */
		pr_info("check_fuel_gauage_update_data: g_batt_soc(%d), g_batt_vol(%d)\n",
			g_batt_soc,  g_batt_vol);
	}
#endif
	lge_pm_battery_age_update();

	if (g_batt_vol != g_batt_pre_vol || g_batt_soc != g_batt_pre_soc)
		val = true;
	else
		val = false;
	g_batt_pre_vol = g_batt_vol;
	g_batt_pre_soc = g_batt_soc;

	return val;

}
#endif

#ifdef CONFIG_SMB349_VZW_FAST_CHG
/*
 * Depending on AICL reult, change the charging status.
 * AICL result < 1600mA : VZW_UNDER_CURRENT_CHARGING
 * AICL result >= 1600mA : VZW_NORMAL_CHARGONG
 */
static void change_vzw_charging_state(struct smb349_struct *smb349_chg)
{
	u8 val;
	int ret;

	ret = smb349_read_reg(smb349_chg->client, STATUS_E_REG, &val);
	if (ret) {
		pr_err("Failed to read STATUS_E_REG = %d\n", ret);
		return;
	}
	if ((val & SMB349_MASK(4, 0)) < 0x07) {
		chg_state = VZW_UNDER_CURRENT_CHARGING;
		pr_info("VZW_UNDER_CURRENT_CHARGING : 0x%x\n", val);
	} else if ((val & SMB349_MASK(4, 0)) >= 0x07) {
		chg_state = VZW_NORMAL_CHARGING;
		pr_info("VZW_NORMAL_CHARGING : 0x%x\n", val);
	}
	vzw_chg_present = CHARGER_PRESENT;

	power_supply_changed(&smb349_chg->batt_psy);
	return;
}
#endif

#ifdef CONFIG_MAX17050_FUELGAUGE
#define MAX17050_SOC_MS    60000
#define MID_MAX17050_SOC_MS 40000
#define LOW_MAX17050_SOC_MS	20000
static void max17050_soc(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct smb349_struct *chip = container_of(dwork,
				struct smb349_struct, max17050_soc_work);

#ifdef CONFIG_MAX17050_FUELGAUGE
	if (check_fuel_gauage_update_data() == true) {
		pr_info("changed power_supply");
		pr_info("batt_soc(%d), batt_vol(%d)\n",
			g_batt_soc, g_batt_vol);
		pr_info("batt_current(%d), batt_age(%d)\n",
			g_batt_current, g_batt_age);
		power_supply_changed(&chip->batt_psy);
	} else {
		pr_info("Not changed power_supply");
		pr_info("batt_soc(%d), batt_vol(%d)\n",
			g_batt_soc, g_batt_vol);
		pr_info("batt_current(%d), batt_age(%d)\n",
			g_batt_current, g_batt_age);
	}
#else
	power_supply_changed(&chip->batt_psy);
#endif

	if (g_batt_soc > 15)
		schedule_delayed_work(&chip->max17050_soc_work,
			      msecs_to_jiffies(MAX17050_SOC_MS));
	else if (g_batt_soc > 5)
		schedule_delayed_work(&chip->max17050_soc_work,
			      msecs_to_jiffies(MID_MAX17050_SOC_MS));
	else
		schedule_delayed_work(&chip->max17050_soc_work,
			      msecs_to_jiffies(LOW_MAX17050_SOC_MS));

}
#endif

#ifdef CONFIG_SMB349_VZW_FAST_CHG
/*
 * Detect VZW incompatible charger.
 * If the capacity of chager is under 500mA, that charger is incompatible.
 * 1. Check the SMB349 'DCIN under-voltage status' five times.
 * 2. Determine the charger is incompatible, if DCIN status bit is changed in ten times.
 */
static int detect_vzw_incombatible_charger(struct i2c_client *client)
{
	u8 val;
	int ret,  count = 0;
	int temp[10] = {0,};

	do{
		ret = smb349_read_reg(client, IRQ_E_REG, &val);
		if (ret) {
			pr_err("Failed to read IRQ_E_REG = %d\n", ret);
			return 0;
		}
		temp[count] = val & SMB349_MASK(1, 0);
		pr_debug("temp[%d] : %d\n", count, temp[count]);
		if (count > 0) {
			if (temp[count-1] != temp[count]) {
				pr_info("VZW_FAST_CHG : INCOMPATIBLE CHARGER\n");
				vzw_chg_present = UNKNOWN_PRESENT;
				return 0;
			}
		}
		count += 1;
		msleep(10);
	}while(count < 10);

	pr_info("VZW_FAST_CHG : NOT INCOMPATIBLE CHARGER\n");
	return 1;
}
#endif

/*
 * Do the IRQ work from a thread context rather than interrupt context.
 * Read status registers to clear interrupt source.
 * Notify the power-supply driver about change detected.
 * Relevant events for start/stop charging:
 * 1. DC insert/remove
 * 2. End-Of-Charging
 * 3. Battery insert/remove
 * 4. Temperture too hot/cold (Do not use.)
 * 5. Charging timeout expired.
 */
static void smb349_irq_worker(struct work_struct *work)
{
	u8 val;
	int ret = 0, usb_present = 0, host_mode;
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	int wlc_present =0;
#endif
	struct smb349_struct *smb349_chg =
		container_of(work, struct smb349_struct, irq_work.work);

	ret = smb349_read_reg(smb349_chg->client, IRQ_F_REG, &val);
	if (ret) {
		pr_err("Failed to read IRQ_F_REG = %d\n", ret);
		return;
	}
	if ((val & OTG_OC_LIMIT_MASK) || (val & OTG_BATT_UV_MASK)) {
		if (val & OTG_OC_LIMIT_MASK) {
			/* otg is disabled, if otg current is over recent current limit */
			pr_info("smb349 OTG over current limit.\n");

			if (otg_limit_status < OTG_CURRENT_LIMIT_1000) {
				smb349_change_otg_current_limit(smb349_chg);
			} else {
   				otg_limit_status = OTG_CURRENT_LIMIT_MAX;
				pr_err("OTG_CURRENT_LIMIT > otg is disabled.\n");
			}

			cancel_delayed_work(&smb349_chg->polling_work);
			host_mode = smb349_chg_is_otg_active(smb349_chg);
			pr_err("smb349_irq_worker triggered: %d host_mode: %d\n",
					usb_present, host_mode);

			if (host_mode) {
				if (otg_limit_status != OTG_CURRENT_LIMIT_MAX) {
					smb349_set_otg_enable_bit(smb349_chg, 0);
					smb349_set_otg_enable_bit(smb349_chg, USB_OTG_EN_BIT);
					schedule_delayed_work(&smb349_chg->polling_work,
							msecs_to_jiffies(500));
				}
				return;
			}
		} else if (val & OTG_BATT_UV_MASK) {
			pr_err("OTG_BATT_UNDER_VOLT > otg is disabled.\n");

			host_mode = smb349_chg_is_otg_active(smb349_chg);
			pr_err("smb349_irq_worker triggered: %d host_mode: %d\n",
					usb_present, host_mode);

			if (host_mode)
				return;
		}
	}

#ifdef CONFIG_SMB349_VZW_FAST_CHG
	/* Detect AICL result */
	ret = smb349_read_reg(smb349_chg->client, IRQ_D_REG, &val);
	if (ret) {
		pr_err("Failed to read IRQ_D_REG = %d\n", ret);
		return;
	}
	pr_err("IRQ_D_REG : 0x%x\n", val);

	if (val & AICL_COMPLETE_STATUS_BIT)
		/* Inserted TA and set AICL */
		change_vzw_charging_state(smb349_chg);
	else if ((val & AICL_COMPLETE_IRQ_BIT) &&
			(!(val & AICL_COMPLETE_STATUS_BIT)))
		/* Removed TA and changed AICL status */
		vzw_chg_present = NOT_PRESENT;
	else if (val == 0x00)
		/* Removed USB OR Incompatible TA */
		vzw_chg_present = NOT_PRESENT;
#endif

	ret = smb349_clear_irq(smb349_chg->client);
	pr_debug("smb349_clear_irq %d.\n", ret);
	if (ret == 0) { /* Cleared ok */
		/* Notify Battery-psy about status changed */
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		if ((vzw_chg_present == NOT_PRESENT) && (chg_state == VZW_NO_CHARGER)) {
			if (!detect_vzw_incombatible_charger(smb349_chg->client))
				usb_present = 1;
			else
				usb_present = smb349_is_charger_present(smb349_chg->client);
		} else if ((vzw_chg_present == INCOMPAT_PRESENT) &&
				(chg_state == VZW_NO_CHARGER)) {
			pr_debug("INCOMPAT CHARGING\n");
			return;
		} else
			usb_present = smb349_is_charger_present(smb349_chg->client);
#else
		usb_present = smb349_is_charger_present(smb349_chg->client);
#endif
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
		wlc_present = wireless_charging || is_wireless_charger_plugged() ;
#endif

		pr_debug("smb349_chg->usb_present = %d, usb_preset = %d.\n",
				smb349_chg->usb_present, usb_present);

#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
		if ((smb349_chg->usb_present ^ usb_present) ||
				(smb349_chg->wlc_present ^ wlc_present) ) {
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
			cancel_delayed_work_sync(&smb349_chg->battemp_work);
			schedule_delayed_work(&smb349_chg->battemp_work, HZ*1);
			if (!usb_present &&
				wake_lock_active(&smb349_chg->lcs_wake_lock))
				wake_unlock(&smb349_chg->lcs_wake_lock);
#endif
			wake_lock_timeout(&smb349_chg->uevent_wake_lock, HZ*2);
			smb349_chg->usb_present = usb_present;
			smb349_chg->wlc_present = wlc_present;

			if(!wlc_present)
				power_supply_set_present(smb349_chg->usb_psy,
						smb349_chg->usb_present);
			else
				power_supply_set_present(smb349_chg->usb_psy,0);
		}
#else
		if (smb349_chg->usb_present ^ usb_present) {
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
			cancel_delayed_work_sync(&smb349_chg->battemp_work);
			schedule_delayed_work(&smb349_chg->battemp_work, HZ*1);
			if (!usb_present &&
				wake_lock_active(&smb349_chg->lcs_wake_lock))
				wake_unlock(&smb349_chg->lcs_wake_lock);
#endif
			wake_lock_timeout(&smb349_chg->uevent_wake_lock, HZ*2);
			smb349_chg->usb_present = usb_present;
			power_supply_set_present(smb349_chg->usb_psy,
				smb349_chg->usb_present);
		}
#endif

		power_supply_changed(smb349_chg->usb_psy);
	}

}

/*
 * The STAT pin is low when charging and high when not charging.
 * When the smb350 start/stop charging the STAT pin triggers an interrupt.
 * Interrupt is triggered on both rising or falling edge.
 */
static irqreturn_t smb349_irq(int irq, void *dev_id)
{
	struct smb349_struct *smb349_chg = dev_id;

	pr_debug("smb349_irq\n");

	/* I2C transfers API should not run in interrupt context */
	schedule_delayed_work(&smb349_chg->irq_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static void smb349_polling_worker(struct work_struct *work)
{
	struct smb349_struct *smb349_chg =
		container_of(work, struct smb349_struct, polling_work.work);

	smb349_change_otg_current_limit(smb349_chg);
}

static enum power_supply_property pm_power_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static enum power_supply_property smb349_batt_power_props[] = {
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
#ifdef CONFIG_MAX17050_FUELGAUGE
/*junnyoung.jang@lge.com 20130326 Add battery condition */
	POWER_SUPPLY_PROP_BATTERY_CONDITION,
	POWER_SUPPLY_PROP_BATTERY_AGE,
#endif
#ifdef CONFIG_SMB349_VZW_FAST_CHG
	POWER_SUPPLY_PROP_VZW_CHG_STATE,
#endif
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int pm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct,
						dc_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = (int)smb349_chg->ac_present;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = smb349_chg->chg_current_ma;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		if (chg_state == VZW_NOT_CHARGING)
			val->intval = 1;
		else
#endif
		val->intval = smb349_chg->ac_online;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb349_get_prop_charge_type(smb349_chg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define SMB349_FAST_CHG_MIN_MA	1000
#define SMB349_FAST_CHG_STEP_MA	200
#define SMB349_FAST_CHG_MAX_MA	4000
#define SMB349_FAST_CHG_SHIFT	4
static int
smb349_chg_current_set(struct smb349_struct *smb349_chg, int chg_current)
{
	u8 temp;

	if ((chg_current < SMB349_FAST_CHG_MIN_MA) ||
		(chg_current >  SMB349_FAST_CHG_MAX_MA)) {
		pr_err("bad mA=%d asked to set\n", chg_current);
		return -EINVAL;
	}

	temp = (chg_current - SMB349_FAST_CHG_MIN_MA)
			/ SMB349_FAST_CHG_STEP_MA;

	temp = temp << SMB349_FAST_CHG_SHIFT;
	pr_debug("fastchg limit=%d setting %02x\n",
				chg_current, temp);
	return smb349_masked_write(smb349_chg->client, CHG_CURRENT_REG,
			FAST_CHG_CURRENT_MASK, temp);
}

#define SMB349_TERM_CURR_MIN_MA		100
#define SMB349_TERM_CURR_STEP_MA	100
#define SMB349_TERM_CURR_MAX_MA		700
#define SMB349_TERM_CURR_SHIFT		2
static int smb349_term_current_set(struct smb349_struct *smb349_chg)
{
	u8 temp;

	if ((smb349_chg->term_current_ma < SMB349_TERM_CURR_MIN_MA) ||
		(smb349_chg->term_current_ma >  SMB349_TERM_CURR_MAX_MA)) {
		pr_err("bad mA=%d asked to set\n", smb349_chg->term_current_ma);
		return -EINVAL;
	}

	temp = (smb349_chg->term_current_ma - SMB349_TERM_CURR_MIN_MA)
			/ SMB349_TERM_CURR_STEP_MA;

	temp -= 1;
	if (temp == 255)
		temp = 6;

	temp = temp << SMB349_TERM_CURR_SHIFT;
	pr_debug("fastchg limit=%d setting %02x\n",
				smb349_chg->term_current_ma, temp);
	return smb349_masked_write(smb349_chg->client, CHG_OTHER_CURRENT_REG,
			TERMINATION_CURRENT_MASK, temp);
}

static int smb349_chg_timeout_set(struct smb349_struct *smb349_chg)
{
	int ret;

	/* Complete timeout 382m, pre-charge timeout 48m */
	ret = smb349_masked_write(smb349_chg->client, STAT_TIMER_REG,
				COMPETE_CHG_TIMEOUT_BIT | PRE_CHG_TIMEOUT_BIT, 0);
	if (ret) {
		pr_err("Failed to set CHG_TIMEOUT_SET rc=%d\n", ret);
		return ret;
	}

	/* set Charge timeout bit */
	ret = smb349_masked_write(smb349_chg->client, STATUS_IRQ_REG,
				CHG_TIMEOUT_BIT, 1);
	if (ret) {
		pr_err("Failed to set CHG_TIMEOUT_BIT rc=%d\n", ret);
		return ret;
	}

	smb349_chg->chg_timeout = false;

	return 0;
}

/*
 * val must be EN_PIN_CTRL_MASK or 0.
 * EN_PIN_CTRL_MASK - pin control, active low
 * 0 - I2C control, active high
 */
static int smb349_set_pin_control(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("smb349_set_pin_control, val = %d\n", val);

	if (val != EN_PIN_CTRL_MASK)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, PIN_ENABLE_CTRL_REG,
						EN_PIN_CTRL_MASK, val);
	if (ret) {
		pr_err("Failed to set EN_PIN_CTRL_MASK rc=%d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * val must be USB_CS_BIT or 0.
 * USB_CS_BIT - pin control
 * 0 - register control
 */
static int smb349_set_usbcs_control(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("smb349_set_usbcs_control, val = %d\n", val);

	if (val != USB_CS_BIT)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, PIN_ENABLE_CTRL_REG,
						USB_CS_BIT, val);
	if (ret) {
		pr_err("Failed to set USB_CS_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_set_float_voltage(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("smb349_set_float_voltage, val = %d\n", val);

	if (val < 0x0 || val > 0x3f) {
		pr_err("Invalid setting value, val = %d\n", val);
		return -EINVAL;
	}

	ret = smb349_masked_write(smb349_chg->client, FLOAT_VOLTAGE_REG,
						FLOAT_VOLTAGE_MASK, val);
	if (ret) {
		pr_err("Failed to set FLOAT_VOLTAGE_REG rc=%d\n", ret);
		return ret;
	}
	return 0;
}

/* ToDo : Must implements & verify hwo to set 500mA or 100mA. */
#define SMB349_USB_5_1_MODE_SHIFT	1
static int smb349_set_usb_5_1_mode(struct smb349_struct *smb349_chg, u8 usb5)
{
	int ret;

	pr_debug("smb349_set_usb_5_1_mode, usb5 = %d\n", usb5);

	usb5 = usb5 << SMB349_USB_5_1_MODE_SHIFT;

	ret = smb349_masked_write(smb349_chg->client, CMD_B_REG,
				USB_5_1_MODE_BIT, usb5);
	if (ret) {
		pr_err("Failed to set USB_5_1_MODE_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_set_irq(struct smb349_struct *smb349_chg)
{
	int ret;

	ret = smb349_masked_write(smb349_chg->client, STATUS_IRQ_REG,
				INOK_BIT | MISSING_BATT_BIT,
				INOK_BIT | MISSING_BATT_BIT);

	if (ret) {
		pr_err("Failed to set INOK | MISSING_BATT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_set_fault_irq(struct smb349_struct *smb349_chg)
{
	int ret;

	ret = smb349_masked_write(smb349_chg->client, FAULT_IRQ_REG,
		OTG_BATT_FAIL_UVLO_BIT | OTG_OVER_CURRENT_LIMIT_BIT | AICL_COMPLETE_IRQ_SET_BIT,
		OTG_BATT_FAIL_UVLO_BIT | OTG_OVER_CURRENT_LIMIT_BIT | AICL_COMPLETE_IRQ_SET_BIT);

	if (ret) {
		pr_err("Failed to set OTG_BATT_FAIL_UVLO | OTG_OVER_CURRENT_LIMIT rc=%d\n", ret);
		return ret;
	}
	return 0;
}

static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	if (!the_smb349_chg) {
		pr_err("the_smb349_chg is not initialized\n");
		return -EAGAIN;
	}

	temp = (u16) val;
	ret = smb349_write_reg(the_smb349_chg->client, addr, temp);

	if (ret) {
		pr_err("smb349_write_reg to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}
static int get_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int ret;
	u8 temp = 0;

	if (!the_smb349_chg) {
		pr_err("the_smb349_chg is not initialized\n");
		return -EAGAIN;
	}

	ret = smb349_read_reg(the_smb349_chg->client, addr, &temp);
	if (ret) {
		pr_err("smb349_read_reg to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}

	*val = temp;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static int smb349_create_debugfs_entries(struct smb349_struct *smb349_chg)
{
	int i;
	smb349_chg->dent = debugfs_create_dir(SMB349_NAME, NULL);
	if (IS_ERR(smb349_chg->dent)) {
		pr_err("smb349 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(smb349_debug_regs) ; i++) {
		char *name = smb349_debug_regs[i].name;
		u32 reg = smb349_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, smb349_chg->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static void remove_debugfs_entries(struct smb349_struct *smb349_chg)
{
	if (smb349_chg->dent)
		debugfs_remove_recursive(smb349_chg->dent);
}

static int smb349_hwinit(struct smb349_struct *smb349_chg)
{
	int ret;

	ret = smb349_write_reg(smb349_chg->client, CMD_A_REG,
			VOLATILE_W_PERM_BIT);
	if (ret) {
		pr_err("Failed to set VOLATILE_W_PERM_BIT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_masked_write(smb349_chg->client, CHG_CTRL_REG,
				CURR_TERM_END_CHG_BIT, 0);
	if (ret) {
		pr_err("Failed to set CURR_TERM_END_CHG_BIT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_chg_current_set(smb349_chg, smb349_chg->chg_current_ma);
	if (ret) {
		pr_err("Failed to set FAST_CHG_CURRENT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_term_current_set(smb349_chg);
	if (ret) {
		pr_err("Failed to set TERM_CURRENT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_chg_timeout_set(smb349_chg);
	if (ret) {
		pr_err("Failed to set CHG_TIMEOUT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_usb_5_1_mode(smb349_chg, 1);
	if (ret) {
		pr_err("Failed to set USB_5_1_MODE rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_irq(smb349_chg);
	if (ret) {
		pr_err("Failed to set smb349_set_irq rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_pin_control(smb349_chg, 0);
	if (ret) {
		pr_err("Failed to set pin control rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_usbcs_control(smb349_chg, 0);
	if (ret) {
		pr_err("Failed to set usbcs control rc=%d\n", ret);
		return ret;
	}

	/* Set Floating Voltage to 4.35v */
	ret = smb349_set_float_voltage(smb349_chg, 0x2d);
	if (ret) {
		pr_err("Failed to set floating voltage rc=%d\n", ret);
		return ret;
	}

	ret = smb349_masked_write(smb349_chg->client, THERM_CTRL_A_REG,
				BOOST_BACK_PREVENTION_BIT, 0);
	if (ret) {
		pr_err("Failed to set boost_back_prevnetion bit rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_fault_irq(smb349_chg);
	if (ret) {
		pr_err("Failed to set smb349_set_fault_irq rc=%d\n", ret);
		return ret;
	}

	/* Charging state change irq enable */
	ret = smb349_masked_write(smb349_chg->client, STATUS_IRQ_REG,
		TERM_TAPER_CHG_IRQ_SET_BIT | FAST_CHG_IRQ_SET_BIT,
		TERM_TAPER_CHG_IRQ_SET_BIT | FAST_CHG_IRQ_SET_BIT);
	if (ret) {
		pr_err("Failed to set status_irq_reg ret=%d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/* It must be called for USB devices only. */
static int
smb349_set_usb_2_3_mode(struct smb349_struct *smb349_chg, bool force_usb2)
{
	int ret;
	u8 usb3 = 0x0;
	struct usb_phy *otg_xceiv;
	struct dwc3_otg *dotg;

	if (!force_usb2) {
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
				usb3 = USB_2_3_SEL_BIT;
		}
	}

	pr_debug("smb349_set_usb_2_3_mode usb3 = %d\n", usb3);

	ret = smb349_masked_write(smb349_chg->client, CMD_B_REG,
						USB_2_3_SEL_BIT, usb3);
	if (ret) {
		pr_err("Failed to set USB_2_3_SEL_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * mode must be USB_HC_MODE_BIT or 0.
 * USB_HC_MODE_BIT - high current mode.
 * 0 - usb mode.
 */
static int smb349_usb_hc_mode(struct smb349_struct *smb349_chg, u8 mode)
{
	int ret;

	pr_debug("smb349_usb_hc_mode mode=%d\n", mode);

	if (mode != USB_HC_MODE_BIT)
		mode = 0;

	ret = smb349_masked_write(smb349_chg->client, CMD_B_REG,
				USB_HC_MODE_BIT, mode);
	if (ret) {
		pr_err("Failed to set USB_HC_MODE_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * FAST_CHARGE_SET_BIT - Allow fast-charge current settings.
 * 0 - Force pre-charge current settings.
 */
static int smb349_set_fast_charge(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("val = %d\n", val);

	if (val != FAST_CHARGE_SET_BIT)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
				FAST_CHARGE_SET_BIT, val);
	if (ret) {
		pr_err("Failed to set FAST_CHARGE_SET_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_set_pre_chg_to_fast_chg_thresh(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("val = %d\n", val);

	if (val != PRE_CHG_TO_FAST_CHG_THRESH_BIT)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, SYSOK_USB3_SELECT_REG,
				PRE_CHG_TO_FAST_CHG_THRESH_BIT, val);
	if (ret) {
		pr_err("Failed to set PRE_CHG_TO_FAST_CHG_THRESH_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

#define SMB349_INPUT_CURRENT_LIMIT_MIN_MA	500
#define SMB349_INPUT_CURRENT_LIMIT_MAX_MA	3500
static int smb349_input_current_limit_set(struct smb349_struct *smb349_chg, int icl_ma)
{
	int i;
	u8 temp;

	if ((icl_ma < SMB349_INPUT_CURRENT_LIMIT_MIN_MA) ||
		(icl_ma >  SMB349_INPUT_CURRENT_LIMIT_MAX_MA)) {
		pr_err("bad mA=%d asked to set\n", icl_ma);
		return -EINVAL;
	}

	for (i = ARRAY_SIZE(icl_ma_table) - 1; i >= 0; i--) {
		if (icl_ma_table[i].icl_ma == icl_ma)
			break;
	}

	if (i < 0) {
		pr_err("can't find %d in icl_ma_table. Use min.\n", icl_ma);
		i = 0;
	}

	temp = icl_ma_table[i].value;

	pr_info("input current limit=%d setting %02x\n", icl_ma, temp);
	return smb349_masked_write(smb349_chg->client, CHG_CURRENT_REG,
			AC_INPUT_CURRENT_LIMIT_MASK, temp);
}

static struct input_current_ma_limit_entry pchg_ma_table[] = {
	{100, 0xC0},
	{200, 0x00},
	{300, 0x20},
	{400, 0x40},
	{500, 0x60},
	{600, 0x80},
	{700, 0xA0},
};

#define SMB349_PRE_CHG_CURRENT_LIMIT_MIN_MA     100
#define SMB349_PRE_CHG_CURRENT_LIMIT_MAX_MA     700
#define SMB349_PRE_CHG_CURRENT_LIMIT_DEFAULT    300
static int
smb349_set_pre_chg_current(struct smb349_struct *smb349_chg, int pchg_ma)
{
	int i;
	u8 temp;

	if ((pchg_ma < SMB349_PRE_CHG_CURRENT_LIMIT_MIN_MA) ||
		(pchg_ma >  SMB349_PRE_CHG_CURRENT_LIMIT_MAX_MA)) {
		pr_err("bad mA=%d asked to set\n", pchg_ma);
		return -EINVAL;
	}

	for (i = ARRAY_SIZE(pchg_ma_table) - 1; i >= 0; i--) {
		if (pchg_ma_table[i].icl_ma <= pchg_ma)
			break;
	}

	if (i < 0) {
		pr_err("can't find %d in pchg_ma_table. Use defult.\n",
			pchg_ma);
		i = 2;
	}

	temp = pchg_ma_table[i].value;

	pr_debug("pre-charing current limit=%d setting %02x\n", pchg_ma, temp);
	return smb349_masked_write(smb349_chg->client, CHG_OTHER_CURRENT_REG,
			PRE_CHG_CURRENT_MASK, temp);
}

#define HC_INPUT_CURR_LIMIT_DEFAULT 3000
#define HC_INPUT_CURR_LIMIT_FACTORY 1500
#define HC_INPUT_CURR_LIMIT_FACTORY_130K 1000

static int smb349_switch_usb_to_charge_mode(struct smb349_struct *smb349_chg)
{
	int ret;

	pr_debug("switch to charge mode\n");

	if (!smb349_chg_is_otg_active(smb349_chg))
		return 0;

	/* enable usb otg */
	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
						USB_OTG_EN_BIT, 0);
	if (ret) {
		pr_err("Failed to turn on usb otg = %d\n", ret);
		return ret;
	}
	smb349_enable_charging(smb349_chg, true);

	ret = smb349_set_otg_current_limit(smb349_chg, 0x04);
	if (ret) {
		pr_err("Failed to set smb349_set_otg_current_limit rc=%d\n", ret);
		return ret;
	}
	otg_limit_status = OTG_CURRENT_LIMIT_500;
	pr_err("OTG_CURRENT_LIMIT_500\n");
	cancel_delayed_work(&smb349_chg->polling_work);

	return 0;
}

static int smb349_switch_usb_to_host_mode(struct smb349_struct *smb349_chg)
{
	int ret;

	pr_debug("switch to host mode\n");

	if (smb349_chg_is_otg_active(smb349_chg))
		return 0;

	smb349_enable_charging(smb349_chg, false);

	/* force usb otg */
	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
						USB_OTG_EN_BIT, USB_OTG_EN_BIT);
	if (ret) {
		pr_err("Failed to turn off usb otg = %d\n", ret);
		return ret;
	}

	return 0;
}

static void smb349_batt_external_power_changed(struct power_supply *psy)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct, batt_psy);
	union power_supply_propval ret = {0,};

	pr_debug("\n");

	smb349_chg->usb_psy->get_property(smb349_chg->usb_psy,
			  POWER_SUPPLY_PROP_SCOPE, &ret);
	if (ret.intval) {
		pr_err("%s : ret.intval=%d.\n", __func__, ret.intval);
		if ((ret.intval == POWER_SUPPLY_SCOPE_SYSTEM)
				&& !smb349_chg_is_otg_active(smb349_chg)) {
			smb349_switch_usb_to_host_mode(smb349_chg);
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
#if defined(CONFIG_MACH_MSM8974_G2_KR)
			if(lge_get_board_revno() >= HW_REV_C)
#elif defined(CONFIG_MACH_MSM8974_VU3_KR)
			if(lge_get_board_revno() >= HW_REV_EVB2)
#endif
			{
				gpio_set_value(smb349_chg->otg_en_gpio, 1);
			}
#endif
			schedule_delayed_work(&smb349_chg->polling_work, msecs_to_jiffies(500));
			return;
		}
		if ((ret.intval == POWER_SUPPLY_SCOPE_DEVICE)
				&& smb349_chg_is_otg_active(smb349_chg)) {
			smb349_switch_usb_to_charge_mode(smb349_chg);
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
#if defined(CONFIG_MACH_MSM8974_G2_KR)
			if(lge_get_board_revno() >= HW_REV_C)
#elif defined(CONFIG_MACH_MSM8974_VU3_KR)
			if(lge_get_board_revno() >= HW_REV_EVB2)
#endif
			{
				gpio_set_value(smb349_chg->otg_en_gpio, 0);
			}
#endif
			return;
		}
	}

	smb349_chg->usb_psy->get_property(smb349_chg->usb_psy,
			  POWER_SUPPLY_PROP_ONLINE, &ret);
	smb349_chg->usb_online = ret.intval;

	if (is_factory_cable() && smb349_is_charger_present(smb349_chg->client)) {
		if (is_factory_cable_130k()) {
			pr_info("Factory cable 130k detected, operate USB2.0 mode\n");
			smb349_usb_hc_mode(smb349_chg, 0);
			smb349_set_usb_2_3_mode(smb349_chg, false);
		} else {
			pr_info("Factory cable detected(not 130k), HC mode set %d\n",
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_input_current_limit_set(smb349_chg,
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_set_pre_chg_to_fast_chg_thresh(smb349_chg,
				PRE_CHG_TO_FAST_CHG_THRESH_BIT);
			smb349_set_fast_charge(smb349_chg, 0);
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
#ifndef CONFIG_LGE_PM
			smb349_chg_usb_suspend_enable(smb349_chg, 0);
#endif
		}
#ifdef CONFIG_SMB349_VZW_FAST_CHG
	} else if ((chg_state == VZW_NO_CHARGER) &&
			(vzw_chg_present == UNKNOWN_PRESENT)) {
		/*Changed SMB349 Charger setting,
		 if unknown incompatible charger is detected. */
		vzw_chg_present = INCOMPAT_PRESENT;
		smb349_usb_hc_mode(smb349_chg, 0);
		smb349_set_usb_5_1_mode(smb349_chg, 0);
		smb349_set_usb_2_3_mode(smb349_chg, true);
		smb349_enable_charging(smb349_chg, false);
		pr_info("VZW_FAST_CHG : incompatible charger\n");
#endif
	} else if (ret.intval &&
			smb349_is_charger_present(smb349_chg->client)) {
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		if (vzw_chg_present == INCOMPAT_PRESENT) {
			/* Don't stop charing, if incompatible USB is detected */
			smb349_set_usb_5_1_mode(smb349_chg, 1);
			smb349_enable_charging(smb349_chg, true);
			pr_info("VZW_FAST_CHG : incompatible USB, charging\n");
		}
#endif
		smb349_usb_hc_mode(smb349_chg, 0);
		smb349_set_usb_2_3_mode(smb349_chg, false);
		/* ToDo : Must implements & verify. */
#ifndef CONFIG_LGE_PM
		smb349_chg->usb_psy->get_property(smb349_chg->usb_psy,
			  POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
		if ((ret.intval / 1000) <= SMB349_CHG_IUSB_MAX_MIN_MA)
			smb349_chg_usb_suspend_enable(smb349_chg, 1);
		else
			smb349_chg_usb_suspend_enable(smb349_chg, 0);
#endif
	} else if (smb349_chg->ac_online &&
				smb349_is_charger_present(smb349_chg->client)) {
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		if (vzw_chg_present == INCOMPAT_PRESENT) {
			/* Stop charing, if incompatible TA is detected */
			chg_state = VZW_NOT_CHARGING;
			pr_info("VZW_FAST_CHG : incompatible TA, stop charging\n");
		} else
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
#else
		smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
		/* ToDo : Must implements & verify. */
#endif
#ifndef CONFIG_LGE_PM
		if ((smb349_chg->current_max / 1000) <=
			SMB349_CHG_IUSB_MAX_MIN_MA)
			smb349_chg_usb_suspend_enable(smb349_chg, 1);
		else
			smb349_chg_usb_suspend_enable(smb349_chg, 0);
#endif
#ifdef CONFIG_WIRELESS_CHARGER
#ifdef CONFIG_BQ51053B_CHARGER
	} else if (wireless_charging){
		if(is_wireless_charger_plugged()){
			smb349_input_current_limit_set(smb349_chg, 900);
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
			pr_err("[WLC] Set inuput limit HC mode\n");

			smb349_set_pre_chg_current(smb349_chg, 500);
			smb349_set_pre_chg_to_fast_chg_thresh(smb349_chg,
				PRE_CHG_TO_FAST_CHG_THRESH_BIT);
			smb349_set_fast_charge(smb349_chg, 0);
			pr_err("[WLC] Set charging current 500mA\n");
		}else{
			pr_err("[WLC] GPIO SWING\n");
		}
		if(smb349_is_charger_present(smb349_chg->client)==false){
			pr_err("[WLC] I'm on WLC PAD, but DC is missing!! \n");
		}
#endif
#endif
	} else {
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		if ((vzw_chg_present == NOT_PRESENT) &&
				(chg_state == VZW_NOT_CHARGING)) {
			/* Removed Incompatible TA */
			smb349_set_usb_5_1_mode(smb349_chg, 1);
			pr_info("VZW_FAST_CHG : Removed incompatible TA\n");
		}
		chg_state = VZW_NO_CHARGER;
#endif
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
		smb349_chg_current_set(smb349_chg, smb349_chg->chg_current_ma);
#endif
		smb349_set_usb_2_3_mode(smb349_chg, true);
		smb349_input_current_limit_set(smb349_chg,
			HC_INPUT_CURR_LIMIT_DEFAULT);
		smb349_set_fast_charge(smb349_chg, FAST_CHARGE_SET_BIT);
		smb349_set_pre_chg_to_fast_chg_thresh(smb349_chg, 0);
		smb349_set_pre_chg_current(smb349_chg,
			SMB349_PRE_CHG_CURRENT_LIMIT_DEFAULT);
		smb349_usb_hc_mode(smb349_chg, 0);
		smb349_enable_charging(smb349_chg, true);
		/* ToDo : Must implements & verify. */
#ifndef CONFIG_LGE_PM
		smb349_chg_usb_suspend_enable(smb349_chg, 0);
#endif
	}

	pr_debug("end of power supply changed\n");
/*#ifndef CONFIG_MAX17050_FUELGAUGE*/
	power_supply_changed(&smb349_chg->batt_psy);
/*#endif*/
}

/* ToDo : Must implements & verify. */
static int smb349_batt_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
#ifdef CONFIG_WIRELESS_CHARGER
#ifndef CONFIG_BQ51053B_CHARGER
		if (wireless_charging) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		}
#endif
#endif
		val->intval = smb349_get_prop_batt_status(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb349_get_prop_charge_type(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb349_get_prop_batt_health(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb349_get_prop_batt_present(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
#else
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
#endif
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 4350 * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 4350 * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = smb349_get_prop_batt_voltage_now();
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = smb349_get_prop_batt_temp(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = smb349_get_prop_batt_capacity(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = smb349_get_prop_batt_current_now(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = smb349_get_prop_batt_full_design(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (smb349_chg->charging_disabled) {
			val->intval = 0;
			break;
		}
#ifdef CONFIG_WIRELESS_CHARGER
		val->intval = smb349_chg->ac_online | smb349_chg->usb_online
		              | wireless_charging;
#else
		val->intval = smb349_chg->ac_online | smb349_chg->usb_online;
#endif
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		/* it makes ibat max set following themral mitigation.
		 * But, SMB349 cannot control ibat current like PMIC.
		 * if LGE charging scenario make charging thermal control,
		 * it is good interface to use LG mitigation level.
		 */
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PSEUDO_BATT:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_EXT_PWR_CHECK:
		val->intval = lge_pm_get_cable_type();
		break;
#ifdef CONFIG_MAX17050_FUELGAUGE
/*junnyoung.jang@lge.com 20130326 Add battery condition */
	case POWER_SUPPLY_PROP_BATTERY_CONDITION:
		val->intval = lge_pm_get_battery_condition();
		break;
	case POWER_SUPPLY_PROP_BATTERY_AGE:
		val->intval = lge_pm_get_battery_age();
		break;
#endif
#ifdef CONFIG_SMB349_VZW_FAST_CHG
	case POWER_SUPPLY_PROP_VZW_CHG_STATE:
		val->intval = chg_state;
		pr_err("VZW_CHG_STATE : %d\n", val->intval);
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_WIRELESS_CHARGER
int set_wireless_power_supply_control(int value)
{
	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return -EINVAL;
	}
#ifdef CONFIG_BQ51053B_CHARGER
	pr_err(" called value : %d\n", value);
#endif
	wireless_charging = value;
#ifndef CONFIG_BQ51053B_CHARGER
	power_supply_changed(&the_smb349_chg->batt_psy);
#endif
	return 0;
}
EXPORT_SYMBOL(set_wireless_power_supply_control);

/*W/R function for booting on WLC PAD
  *WLC  PAD is detected to USB until WLC driver probe is finished completely
  *So we have to check gpio and cancel USB present in the last part of WLC driver probe */
#ifdef CONFIG_BQ51053B_CHARGER
void set_usb_present_false(void)
{
	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return;
	}

	the_smb349_chg->usb_present = false;
	power_supply_set_present(the_smb349_chg->usb_psy,0);
	return;
}
EXPORT_SYMBOL(set_usb_present_false);
#endif

#endif

static int smb349_batt_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smb349_enable_charging(smb349_chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		/* it makes ibat max set following themral mitigation.
		 * But, SMB349 cannot control ibat current like PMIC.
		 * if LGE charging scenario make charging thermal control,
		 * it is good interface to use LG mitigation level.
		 */
		break;
	default:
		return -EINVAL;
	}

	power_supply_changed(&smb349_chg->batt_psy);
	return 0;
}

static int
smb349_batt_power_property_is_writeable(struct power_supply *psy,
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

static int pm_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct,
						dc_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		smb349_chg->ac_present = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		smb349_chg->ac_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		/* SMB329 does not use cable detect current */
		//smb349_chg->chg_current_ma = val->intval;
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(&smb349_chg->dc_psy);
	return 0;
}

static int
smb349_pm_power_property_is_writeable(struct power_supply *psy,
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

static void
smb349_force_fast_to_pre_chg(struct smb349_struct *smb349_chg, int chg_current)
{
       smb349_set_pre_chg_current(smb349_chg, chg_current);
       smb349_set_pre_chg_to_fast_chg_thresh(smb349_chg, PRE_CHG_TO_FAST_CHG_THRESH_BIT);
       smb349_set_fast_charge(smb349_chg, 0);
}

static void
smb349_force_pre_to_fast_chg(struct smb349_struct *smb349_chg)
{
       smb349_set_fast_charge(smb349_chg, FAST_CHARGE_SET_BIT);
       smb349_set_pre_chg_to_fast_chg_thresh(smb349_chg, 0);
       smb349_set_pre_chg_current(smb349_chg, SMB349_PRE_CHG_CURRENT_LIMIT_DEFAULT);
}

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
static void smb349_monitor_batt_temp(struct work_struct *work)
{
	struct smb349_struct *smb349_chg =
		container_of(work, struct smb349_struct, battemp_work.work);
	struct charging_info req;
	struct charging_rsp res;
	union power_supply_propval ret = {0,};

	if (smb349_chg->chg_timeout) {
		int ret;
		ret = smb349_enable_charging(smb349_chg, true);
		if (ret)
			pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);
		else
			smb349_chg->chg_timeout = false;
	}

	smb349_chg->batt_psy.get_property(&(smb349_chg->batt_psy),
			  POWER_SUPPLY_PROP_TEMP, &ret);
	req.batt_temp = ret.intval / 10;

	smb349_chg->batt_psy.get_property(&(smb349_chg->batt_psy),
			  POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
	req.batt_volt = ret.intval;

#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	req.chg_current_ma = smb349_chg->chg_current_ma;
	req.chg_current_te = smb349_chg->chg_current_te;
#endif

	req.is_charger = smb349_is_charger_present(smb349_chg->client);

	lge_monitor_batt_temp(req, &res);

	if (((res.change_lvl != STS_CHE_NONE) && req.is_charger) ||
		(res.force_update == true)) {
		if (res.change_lvl == STS_CHE_NORMAL_TO_DECCUR ||
			(res.force_update == true && res.state == CHG_BATT_DECCUR_STATE &&
			res.dc_current != DC_CURRENT_DEF)) {
			smb349_force_fast_to_pre_chg(smb349_chg, res.dc_current);
		} else if (res.change_lvl == STS_CHE_NORMAL_TO_STPCHG ||
			(res.force_update == true &&
			res.state == CHG_BATT_STPCHG_STATE)) {
			wake_lock(&smb349_chg->lcs_wake_lock);
			smb349_enable_charging(smb349_chg, !res.disable_chg);
			smb349_force_pre_to_fast_chg(smb349_chg);
		} else if (res.change_lvl == STS_CHE_DECCUR_TO_NORAML) {
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			if (res.dc_current < SMB349_FAST_CHG_MIN_MA)
				smb349_force_fast_to_pre_chg(smb349_chg, res.dc_current);
			else {
				smb349_chg_current_set(smb349_chg, res.dc_current);
				smb349_force_pre_to_fast_chg(smb349_chg);
			}
#else
			smb349_force_pre_to_fast_chg(smb349_chg);
#endif
		} else if (res.change_lvl == STS_CHE_DECCUR_TO_STPCHG) {
			wake_lock(&smb349_chg->lcs_wake_lock);
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			smb349_chg_current_set(smb349_chg, smb349_chg->chg_current_ma);
#endif
			smb349_force_pre_to_fast_chg(smb349_chg);
			smb349_enable_charging(smb349_chg, !res.disable_chg);
		} else if (res.change_lvl == STS_CHE_STPCHG_TO_NORMAL) {
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
			if (res.dc_current < SMB349_FAST_CHG_MIN_MA)
				smb349_force_fast_to_pre_chg(smb349_chg, res.dc_current);
			else {
				smb349_chg_current_set(smb349_chg, res.dc_current);
				smb349_force_pre_to_fast_chg(smb349_chg);
			}
#endif
			smb349_enable_charging(smb349_chg, !res.disable_chg);
			wake_unlock(&smb349_chg->lcs_wake_lock);
		}
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
		else if (res.force_update == true && res.state == CHG_BATT_NORMAL_STATE &&
			res.dc_current != DC_CURRENT_DEF) {
			if (res.dc_current < SMB349_FAST_CHG_MIN_MA)
				smb349_force_fast_to_pre_chg(smb349_chg, res.dc_current);
			else {
				smb349_chg_current_set(smb349_chg, res.dc_current);
				smb349_force_pre_to_fast_chg(smb349_chg);
			}
		}
#endif

		smb349_chg->pseudo_ui_chg = res.pseudo_chg_ui;
	}

	power_supply_changed(&smb349_chg->batt_psy);

	schedule_delayed_work(&smb349_chg->battemp_work,
		MONITOR_BATTEMP_POLLING_PERIOD);
}
#endif

static int __devinit smb349_init_batt_psy(struct smb349_struct *smb349_chg)
{
	int ret;

	smb349_chg->batt_psy.name = "battery";
	smb349_chg->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	smb349_chg->batt_psy.properties = smb349_batt_power_props;
	smb349_chg->batt_psy.num_properties =
					ARRAY_SIZE(smb349_batt_power_props);
	smb349_chg->batt_psy.get_property = smb349_batt_power_get_property;
	smb349_chg->batt_psy.set_property = smb349_batt_power_set_property;
	smb349_chg->batt_psy.property_is_writeable =
					smb349_batt_power_property_is_writeable;
	smb349_chg->batt_psy.external_power_changed =
					smb349_batt_external_power_changed;

	ret = power_supply_register(&smb349_chg->client->dev,
				&smb349_chg->batt_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int __devinit smb349_init_ext_chg(struct smb349_struct *smb349_chg)
{
	int ret;

	smb349_chg->dc_psy.name = "ac";
	smb349_chg->dc_psy.type = POWER_SUPPLY_TYPE_MAINS;
	smb349_chg->dc_psy.supplied_to = pm_power_supplied_to;
	smb349_chg->dc_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	smb349_chg->dc_psy.properties = pm_power_props;
	smb349_chg->dc_psy.num_properties = ARRAY_SIZE(pm_power_props);
	smb349_chg->dc_psy.get_property = pm_power_get_property;
	smb349_chg->dc_psy.set_property = pm_power_set_property;
	smb349_chg->dc_psy.property_is_writeable =
					smb349_pm_power_property_is_writeable;
	ret = power_supply_register(&smb349_chg->client->dev,
				&smb349_chg->dc_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int __devinit smb349_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	const struct smb349_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	struct smb349_struct *smb349_chg;
	int ret = 0;
	u8 temp = 0;

	/* STAT pin change on start/stop charging */
	u32 irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size));

	if (p_cable_type)
		cable_type = *p_cable_type;
	else
		cable_type = 0;

	pr_info("cable_type is = %d\n", cable_type);

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("smb349 i2c func fail.\n");
		return -EIO;
	}

	smb349_chg = kzalloc(sizeof(*smb349_chg), GFP_KERNEL);
	if (!smb349_chg) {
		pr_err("smb349 alloc fail.\n");
		return -ENOMEM;
	}

	smb349_chg->client = client;

	smb349_chg->usb_psy = power_supply_get_by_name("usb");
	if (!smb349_chg->usb_psy) {
		pr_err("usb supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto stat_gpio_fail;
	}

	get_cable_data_from_dt(dev_node);

	if (dev_node) {
		smb349_chg->stat_gpio =
			of_get_named_gpio(dev_node, "summit,stat-gpio", 0);
		pr_debug("stat_gpio = %d.\n", smb349_chg->stat_gpio);
		if (smb349_chg->stat_gpio < 0) {
			pr_err("Unable to get named gpio for stat-gpio.\n");
			return smb349_chg->stat_gpio;
		}

#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
#if defined(CONFIG_MACH_MSM8974_G2_KR)
		if(lge_get_board_revno() >= HW_REV_C)
#elif defined(CONFIG_MACH_MSM8974_VU3_KR)
		if(lge_get_board_revno() >= HW_REV_EVB2)
#endif
		{
			smb349_chg->otg_en_gpio =
				of_get_named_gpio(dev_node, "summit,otg-en-gpio", 0);
			if (smb349_chg->otg_en_gpio < 0 && lge_get_laf_mode() != LGE_LAF_MODE_LAF) {
				printk("Unable to get named gpio for otg_en_gpio.\n");
				return smb349_chg->otg_en_gpio;
			}
		}
#endif

#ifndef CONFIG_LGE_PM
		smb349_chg->en_n_gpio =
			of_get_named_gpio(dev_node, "summit,chg-en-n-gpio", 0);
		pr_debug("en_n_gpio = %d.\n", smb349_chg->en_n_gpio);
		if (smb349_chg->en_n_gpio < 0) {
			pr_err("Unable to get named gpio for en_n_gpio.\n");
			return smb349_chg->en_n_gpio;
		}

		smb349_chg->chg_susp_n_gpio =
			of_get_named_gpio(dev_node,
					  "summit,chg-susp-n-gpio", 0);
		pr_debug("chg_susp_n_gpio = %d.\n",
					smb349_chg->chg_susp_n_gpio);
		if (smb349_chg->chg_susp_n_gpio < 0) {
			pr_err("Fail to get named gpio for chg_susp_n_gpio.\n");
			return smb349_chg->chg_susp_n_gpio;
		}
#endif

		ret = of_property_read_u32(dev_node, "summit,chg-current-ma",
					   &(smb349_chg->chg_current_ma));
#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
		smb349_chg->chg_current_te = smb349_chg->chg_current_ma;
#endif
		pr_debug("smb349 chg_current_ma = %d.\n",
					smb349_chg->chg_current_ma);
		if (ret) {
			pr_err("smb349 Unable to read chg_current_ma.\n");
			return ret;
		}

		ret = of_property_read_u32(dev_node, "summit,term-current-ma",
					   &(smb349_chg->term_current_ma));
		pr_debug("smb349 term_current_ma = %d.\n",
					smb349_chg->term_current_ma);
		if (ret) {
			pr_err("smb349 Unable to read term_current_ma.\n");
			return ret;
		}
	} else {
		pdata = client->dev.platform_data;

		if (pdata == NULL) {
			pr_err("smb349 no platform data.\n");
			return -EINVAL;
		}

		smb349_chg->stat_gpio = pdata->stat_gpio;
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
		smb349_chg->otg_en_gpio = pdata->otg_en_gpio;
#endif
#ifndef CONFIG_LGE_PM
		smb349_chg->chg_susp_gpio = pdata->chg_susp_gpio;
		smb349_chg->en_n_gpio = pdata->en_n_gpio;
#endif
		smb349_chg->chg_current_ma = pdata->chg_current_ma;
		smb349_chg->term_current_ma = pdata->term_current_ma;
	}

	ret = gpio_request(smb349_chg->stat_gpio, "smb349_stat");
	if (ret) {
		pr_err("stat_gpio gpio_request failed for %d ret=%d\n",
		       smb349_chg->stat_gpio, ret);
		goto stat_gpio_fail;
	}
	smb349_chg->irq = gpio_to_irq(smb349_chg->stat_gpio);
	pr_debug("stat_gpio irq#=%d.\n", smb349_chg->irq);

#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
#if defined(CONFIG_MACH_MSM8974_G2_KR)
	if(lge_get_board_revno() >= HW_REV_C)
#elif defined(CONFIG_MACH_MSM8974_VU3_KR)
	if(lge_get_board_revno() >= HW_REV_EVB2)
#endif
	{
		ret = gpio_request(smb349_chg->otg_en_gpio, "otg_en");
		if (ret && lge_get_laf_mode() != LGE_LAF_MODE_LAF) {
			printk("otg_en_gpio gpio_request failed for %d ret=%d\n",
				   smb349_chg->otg_en_gpio, ret);
			goto stat_gpio_fail;
		}
		gpio_direction_output(smb349_chg->otg_en_gpio, 0);
	}
#endif

#ifndef CONFIG_LGE_PM
	ret = gpio_request(smb349_chg->chg_susp_gpio, "smb349_suspend");
	if (ret) {
		pr_err("chg_susp_gpio gpio_request failed for %d ret=%d\n",
			smb349_chg->chg_susp_gpio, ret);
		goto chg_susp_gpio_fail;
	}

	ret = gpio_request(smb349_chg->en_n_gpio, "smb349_charger_enable");
	if (ret) {
		pr_err("en_n_gpio gpio_request failed for %d ret=%d\n",
			smb349_chg->en_n_gpio, ret);
		goto en_n_gpio_fail;
	}
#endif

	i2c_set_clientdata(client, smb349_chg);

#ifndef CONFIG_LGE_PM
	/* Control chg_susp_gpio. */
	/* Control en_n_gpio. */
	/* Wait the device to exist shutdown */
#endif

	/* Read I2C_BUS_SLAVE_ADDR_REG */
	ret = smb349_read_reg(client, I2C_BUS_SLAVE_ADDR_REG, &temp);
	if ((ret) || ((temp >> 1) != client->addr)) {
		pr_err("No device.\n");
		ret = -ENODEV;
		goto no_dev_fail;
	}
	pr_debug("I2C_BUS_SLAVE_ADDR_REG.0x%x\n", temp);

	ret = smb349_hwinit(smb349_chg);
	if (ret) {
		pr_err("smb349_hwinit failed.ret=%d\n", ret);
		goto hwinit_fail;
	}

	wake_lock_init(&smb349_chg->chg_wake_lock,
		       WAKE_LOCK_SUSPEND, SMB349_NAME);
	wake_lock_init(&smb349_chg->uevent_wake_lock,
		       WAKE_LOCK_SUSPEND, "smb349_chg_uevent");
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_init(&smb349_chg->lcs_wake_lock,
				WAKE_LOCK_SUSPEND, "LGE charging scenario");
#endif
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
	wake_lock_init(&smb349_chg->battgone_wake_lock,
		       WAKE_LOCK_SUSPEND, "batt removed");
#endif
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	if (HW_REV_A < lge_get_board_revno()) {
		smb349_get_prop_batt_capacity
			= gauge_ic_func_array[MAX17050_TYPE]
						.get_prop_batt_cap_func;
		smb349_get_prop_batt_voltage_now
			= gauge_ic_func_array[MAX17050_TYPE]
						.get_prop_batt_vol_func;
		smb349_get_prop_batt_full_design
			= gauge_ic_func_array[MAX17050_TYPE]
						.get_prop_batt_fdesign_func;
	} else {
		smb349_get_prop_batt_capacity
			= gauge_ic_func_array[MAX17048_TYPE]
						.get_prop_batt_cap_func;
		smb349_get_prop_batt_voltage_now
			= gauge_ic_func_array[MAX17048_TYPE]
						.get_prop_batt_vol_func;
		smb349_get_prop_batt_full_design
			= gauge_ic_func_array[MAX17048_TYPE]
						.get_prop_batt_fdesign_func;
	}
#else
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	if (HW_REV_EVB2<= lge_get_board_revno())
#else
	if (HW_REV_A <= lge_get_board_revno())
#endif
	{
		smb349_get_prop_batt_capacity
			= gauge_ic_func_array[MAX17048_TYPE]
						.get_prop_batt_cap_func;
		smb349_get_prop_batt_voltage_now
			= gauge_ic_func_array[MAX17048_TYPE]
						.get_prop_batt_vol_func;
		smb349_get_prop_batt_full_design
			= gauge_ic_func_array[MAX17048_TYPE]
						.get_prop_batt_fdesign_func;
	} else {
		smb349_get_prop_batt_capacity
			= gauge_ic_func_array[BMS_TYPE]
						.get_prop_batt_cap_func;
		smb349_get_prop_batt_voltage_now
			= gauge_ic_func_array[BMS_TYPE]
						.get_prop_batt_vol_func;
		smb349_get_prop_batt_full_design
			= gauge_ic_func_array[BMS_TYPE]
						.get_prop_batt_fdesign_func;
	}
#endif

	ret = smb349_init_batt_psy(smb349_chg);
	if (ret) {
		pr_err("smb349_init_batt_psy failed.ret=%d\n", ret);
		goto reg_batt_psy_fail;
	}

	ret = smb349_init_ext_chg(smb349_chg);
	if (ret) {
		pr_err("smb349_init_ext_chg failed.ret=%d\n", ret);
		goto reg_ac_psy_fail;
	}

	the_smb349_chg = smb349_chg;

	if (is_factory_cable()) {
		if (is_factory_cable_130k()) {
			pr_info("Factory cable 130k detected, operate USB2.0 mode\n");
			smb349_usb_hc_mode(smb349_chg, 0);
			smb349_set_usb_2_3_mode(smb349_chg, false);
		} else {
			pr_info("Factory cable detected(not 130k), HC mode set %d\n",
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_input_current_limit_set(smb349_chg,
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_set_fast_charge(smb349_chg, 0);
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
		}
	}

	ret = smb349_create_debugfs_entries(smb349_chg);
	if (ret) {
		pr_err("smb349_create_debugfs_entries failed.ret=%d\n", ret);
		goto debugfs_fail;
	}

	INIT_DELAYED_WORK(&smb349_chg->irq_work, smb349_irq_worker);
	INIT_DELAYED_WORK(&smb349_chg->polling_work, smb349_polling_worker);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	INIT_DELAYED_WORK(&smb349_chg->battemp_work, smb349_monitor_batt_temp);
#endif
#ifdef CONFIG_MAX17050_FUELGAUGE
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
		if (HW_REV_A < lge_get_board_revno())
			INIT_DELAYED_WORK(&smb349_chg->max17050_soc_work,
				max17050_soc);
#endif
#endif

	ret = request_irq(smb349_chg->irq, smb349_irq, irq_flags,
			  "smb349_irq", smb349_chg);
	if (ret) {
		pr_err("request_irq %d failed.ret=%d\n", smb349_chg->irq, ret);
		goto irq_fail;
	}

	enable_irq_wake(smb349_chg->irq);

	power_supply_set_present(smb349_chg->usb_psy,
		smb349_is_charger_present(smb349_chg->client));

	smb349_enable_charging(smb349_chg, true);

#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	batt_temp_old= DEFAULT_TEMP;
	batt_current_old = DEFAULT_CURRENT;
#endif
#ifdef CONFIG_MAX17050_FUELGAUGE
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
		if (HW_REV_A < lge_get_board_revno())
			schedule_delayed_work(&smb349_chg->max17050_soc_work,
				msecs_to_jiffies(3000));
#endif
#endif

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	schedule_delayed_work(&smb349_chg->battemp_work,
		MONITOR_BATTEMP_POLLING_PERIOD / 3);
#endif
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
	qpnp_batif_regist_batt_present(&smb349_batt_remove_insert_cb);
#endif
	pr_info("OK to probe SMB349.\n");

	return 0;

irq_fail:
debugfs_fail:
	remove_debugfs_entries(smb349_chg);
reg_ac_psy_fail:
	power_supply_unregister(&smb349_chg->batt_psy);
reg_batt_psy_fail:
	wake_lock_destroy(&smb349_chg->chg_wake_lock);
	wake_lock_destroy(&smb349_chg->uevent_wake_lock);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_destroy(&smb349_chg->lcs_wake_lock);
#endif
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
	wake_lock_destroy(&smb349_chg->battgone_wake_lock);
#endif
hwinit_fail:
no_dev_fail:
#ifndef CONFIG_LGE_PM
en_n_gpio_fail:
	gpio_free(smb349_chg->en_n_gpio);
chg_susp_gpio_fail:
	gpio_free(smb349_chg->chg_susp_gpio);
#endif
	if (smb349_chg->stat_gpio)
		gpio_free(smb349_chg->stat_gpio);
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
#if defined(CONFIG_MACH_MSM8974_G2_KR)
	if(lge_get_board_revno() >= HW_REV_C)
#elif defined(CONFIG_MACH_MSM8974_VU3_KR)
	if(lge_get_board_revno() >= HW_REV_EVB2)
#endif
	{
		if (smb349_chg->otg_en_gpio)
			gpio_free(smb349_chg->otg_en_gpio);
	}
#endif
stat_gpio_fail:
	kfree(smb349_chg);
	smb349_chg = NULL;

	pr_info("Fail to probe SMB349\n");
	return ret;
}

static int __devexit smb349_remove(struct i2c_client *client)
{
#ifndef CONFIG_LGE_PM
	const struct smb349_platform_data *pdata;
#endif
	struct smb349_struct *smb349_chg = i2c_get_clientdata(client);

#ifndef CONFIG_LGE_PM
	pdata = client->dev.platform_data;
#endif
	power_supply_unregister(&smb349_chg->dc_psy);
#ifdef CONFIG_LGE_PM
	power_supply_unregister(&smb349_chg->batt_psy);
	if (smb349_chg->stat_gpio)
		gpio_free(smb349_chg->stat_gpio);
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
#if defined(CONFIG_MACH_MSM8974_G2_KR)
	if(lge_get_board_revno() >= HW_REV_C)
#elif defined(CONFIG_MACH_MSM8974_VU3_KR)
	if(lge_get_board_revno() >= HW_REV_EVB2)
#endif
	{
		if (smb349_chg->otg_en_gpio)
			gpio_free(smb349_chg->otg_en_gpio);
	}
#endif
	if (smb349_chg->irq)
		free_irq(smb349_chg->irq, smb349_chg);
#else
	gpio_free(pdata->en_n_gpio);
	gpio_free(pdata->chg_susp_gpio);
#endif
	remove_debugfs_entries(smb349_chg);

	wake_lock_destroy(&smb349_chg->chg_wake_lock);
	wake_lock_destroy(&smb349_chg->uevent_wake_lock);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_destroy(&smb349_chg->lcs_wake_lock);
#endif
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
	wake_lock_destroy(&smb349_chg->battgone_wake_lock);
	qpnp_batif_unregist_batt_present(0);
#endif

	kfree(smb349_chg);
	smb349_chg = NULL;
	return 0;
}

#ifndef CONFIG_LGE_PM
static int smb349_suspend(struct device *dev)
{
	struct smb349_struct *smb349_chg = dev_get_drvdata(dev);

	pr_err("suspend\n");
	if (smb349_chg->charging)
		return -EBUSY;
#if defined(CONFIG_MAX17050_FUELGAUGE)
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
		if (HW_REV_A < lge_get_board_revno())
			cancel_delayed_work_sync
				(&smb349_chg->max17050_soc_work);
#endif
#endif
	return 0;
}

static int smb349_resume(struct device *dev)
{
	struct smb349_struct *smb349_chg = dev_get_drvdata(dev)

	pr_err("resume\n");
#if defined(CONFIG_MAX17050_FUELGAUGE)
#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	if (HW_REV_A < lge_get_board_revno()) {
		schedule_delayed_work(&smb349_chg->max17050_soc_work,
			msecs_to_jiffies(0));
#endif
#endif
	return 0;
}

static const struct dev_pm_ops smb349_pm_ops = {
	.suspend	= smb349_suspend,
	.resume		= smb349_resume,
};
#endif

static const struct i2c_device_id smb349_id[] = {
	{SMB349_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb349_id);

static const struct of_device_id smb349_match[] = {
	{ .compatible = "summit,smb349-charger", },
	{ },
};

static struct i2c_driver smb349_driver = {
	.driver	= {
		   .name	= SMB349_NAME,
		   .owner	= THIS_MODULE,
#ifdef CONFIG_LGE_PM
		   .of_match_table = of_match_ptr(smb349_match),
#else
		   .pm		= &smb349_pm_ops,
#endif
	},
	.probe		= smb349_probe,
	.remove		= __devexit_p(smb349_remove),
	.id_table	= smb349_id,
};

static int __init smb349_init(void)
{
	return i2c_add_driver(&smb349_driver);
}
module_init(smb349_init);

static void __exit smb349_exit(void)
{
	return i2c_del_driver(&smb349_driver);
}
module_exit(smb349_exit);

MODULE_DESCRIPTION("Driver for SMB349 charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" SMB349_NAME);
