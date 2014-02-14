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
#include <linux/switch.h>
#include <linux/qpnp-misc.h>

#ifdef CONFIG_WIRELESS_CHARGER
#ifdef CONFIG_BQ51053B_CHARGER
#include <linux/power/bq51053b_charger.h>
#endif
#ifdef CONFIG_BQ51051B_CHARGER
#include <linux/power/bq51051b_charger.h>
#endif
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#include <mach/lge_charging_scenario.h>
#define MONITOR_BATTEMP_POLLING_PERIOD          (60*HZ)
#endif
#ifdef  CONFIG_SMB349_VZW_FAST_CHG
#include <linux/slimport.h>
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
/* for checking boostback issue due to trim bit at A4 chip*/
#define CHIP_MINOR_REG                          0x16
#define CMD_A_REG                               0x30
#define CMD_B_REG                               0x31
#define CMD_C_REG                               0x33
/* for checking A4 or A6 chip*/
#define CHIP_MAJOR_REG                          0x34
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
#define OPTICHG_ENABLE_BIT                      BIT(4)

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
	VZW_USB_DRIVER_UNINSTALLED  = 4,
#ifdef CONFIG_VZW_LLK
	VZW_LLK_NOT_CHARGING        = 5,
#endif
};

static enum vzw_chg_state chg_state = VZW_NO_CHARGER;

#define NOT_PRESENT 			0
#define CHARGER_PRESENT 		1
#define UNKNOWN_PRESENT 		2
#define INCOMPAT_PRESENT 		3
#define USB_PRESENT             4

static int vzw_chg_present = NOT_PRESENT;

#define IS_OPEN_TA 0
#define IS_USB_DRIVER_UNINSTALLED 1
#define IS_USB_DRIVER_INSTALLED   2

static int usb_chg_state = IS_USB_DRIVER_INSTALLED;
#ifdef CONFIG_VZW_LLK
static int temp_state = 0;
#endif
#endif

static const char * const smb349_chg_status[] = {
	"none",
	"pre-charge",
	"fast-charge",
	"taper-charge",
	"exception"
};

enum irqstat_idx {
	IRQSTAT_A	= 0,
	IRQSTAT_B	= 1,
	IRQSTAT_C	= 2,
	IRQSTAT_D	= 3,
	IRQSTAT_E	= 4,
	IRQSTAT_F	= 5,
	IRQSTAT_NUM	= 6,
};

#define SMB349_BOOSTBACK_WORKAROUND 1
#if SMB349_BOOSTBACK_WORKAROUND
#define DISABLE_CHG_INPUTFET BIT(0)
bool   smb349_console_silent;
#define smb349_pr_info(fmt, ...) \
do {\
	if (likely(!smb349_console_silent))\
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__);\
} while (0)
#endif

struct smb349_struct {
	struct	i2c_client	*client;
	struct dentry		*dent;
	struct switch_dev 	batt_removed;

	u8     irqstat[IRQSTAT_NUM];
	struct mutex lock;
#if SMB349_BOOSTBACK_WORKAROUND
	bool   is_bb_work_case;
	struct delayed_work  bb_work;
	struct delayed_work  bb_rechg_work;
	bool   is_rechg_work_trigger;
	bool   global_is_bb_worker_eoc;
	unsigned long irq_trigger_cnt;
#endif
	bool   is_phy_forced_on;

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
	struct wake_lock	chg_timeout_lock;
};

#if SMB349_BOOSTBACK_WORKAROUND
typedef struct {
	int usb_present;
	u8  val;
	int caller;
} smb349_bb_param;
#define BB_WORK_TRIGGER_PARAM_SET(p, u, v, c) \
	p.usb_present = u; \
	p.val = v; \
	p.caller = c;
static void smb349_bb_worker_trigger(struct smb349_struct *smb349_chg,
		smb349_bb_param param);
#endif

#ifdef CONFIG_SMB349_VZW_FAST_CHG
#if SMB349_BOOSTBACK_WORKAROUND
static int count_no_aicl = 0;
#endif
#endif
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

struct pseudo_batt_info_type pseudo_batt_info = {
	.mode = 0,
};


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
	SMB349_DEBUG_REG(16_, CHIP_MINOR),
	SMB349_DEBUG_REG(30_, CMD_A),
	SMB349_DEBUG_REG(31_, CMD_B),
	SMB349_DEBUG_REG(33_, CMD_C),
	SMB349_DEBUG_REG(34_, CHIP_MAJOR),
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

#define I2C_RETRY_COUNT 20
static int smb349_read_reg(struct i2c_client *client, int reg,
				u8 *val)
{
	int i;
	int ret;

	for (i = 0; i < I2C_RETRY_COUNT; i++) {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret >= 0)
			break;
		/* this message will change pr_debug after verifying I2C communication */
		pr_info("i2c read fail try count:%d ret:%d\n", i + 1, ret);
		msleep(20);
	}

	if (ret < 0) {
		dev_err(&client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int smb349_read_block_reg(struct i2c_client *client, u8 reg,
				  u8 length, u8 *val)
{
	int i;
	int ret;

	for (i = 0; i < I2C_RETRY_COUNT; i++) {
		ret = i2c_smbus_read_i2c_block_data(client, reg,
				length, val);
		if (ret >= 0)
			break;
		/* this message will change pr_debug after verifying I2C communication */
		pr_info("i2c read fail try count:%d ret:%d\n", i + 1, ret);
		msleep(20);
	}

	if (ret < 0) {
		dev_err(&client->dev, "failed to block read reg 0x%x: %d\n",
				reg, ret);
		return ret;
	}

	return 0;
}

static int smb349_write_reg(struct i2c_client *client, int reg,
						u8 val)
{
	int i;
	int ret;

	for (i = 0; i < I2C_RETRY_COUNT; i++) {
		ret = i2c_smbus_write_byte_data(client, reg, val);
		if (ret >= 0)
			break;
		/* this message will change pr_debug after verifying I2C communication */
		pr_info("i2c read fail try count:%d ret:%d\n", i + 1, ret);
		msleep(20);
	}

	if (ret < 0) {
		dev_err(&client->dev,
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

static void smb349_irqstat_store(struct smb349_struct *smb349_chg,
		u8 irqstat[IRQSTAT_NUM])
{
	int i;
	mutex_lock(&smb349_chg->lock);
	for (i = 0; i < IRQSTAT_NUM; i++) {
		smb349_chg->irqstat[i] = irqstat[i];
	}
	mutex_unlock(&smb349_chg->lock);
}

static void smb349_irqstat_init(struct smb349_struct *smb349_chg)
{
	int i;
	mutex_lock(&smb349_chg->lock);
	for (i = 0; i < IRQSTAT_NUM; i++) {
		smb349_chg->irqstat[i] = 0;
	}
	mutex_unlock(&smb349_chg->lock);
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
static bool smb349_is_charger_present_rt(struct i2c_client *client)
{
	u8 irq_status_e;
	u8 temp;
	bool power_ok;
	int ret;
	int voltage;
	struct smb349_struct *smb349_chg = i2c_get_clientdata(client);

	if (!smb349_chg) {
		pr_err("failed that smb349_chg is not yet initialized\n");
		goto rt_error;
	}

	/* uses under voltage status bit to detect cable plug or unplug */
	ret = smb349_read_reg(client, IRQ_E_REG, &irq_status_e);
	if (ret) {
		pr_err("Failed to read IRQ_STATUS_F_REG rc=%d\n", ret);
		goto rt_error;
	}

	mutex_lock(&smb349_chg->lock);
	smb349_chg->irqstat[IRQSTAT_E] = irq_status_e;
	mutex_unlock(&smb349_chg->lock);

	/* insert case is 0, remove case is 1 */
	power_ok = !(irq_status_e & 0x01);

	if (power_ok) {
		voltage = smb349_get_usbin_adc();
		pr_err("DC is present. DC_IN volt:%d\n", voltage);
	} else
		pr_err("DC is missing.\n");

	return power_ok;

rt_error:
	mutex_lock(&smb349_chg->lock);
	temp = smb349_chg->irqstat[IRQSTAT_E];
	smb349_chg->irqstat[IRQSTAT_E] = temp | 0x01;
	mutex_unlock(&smb349_chg->lock);
	return false;
}

static bool smb349_is_charger_present(struct i2c_client *client)
{
	u8 irq_status_e;
	bool power_ok;
	int voltage;
	struct smb349_struct *smb349_chg = i2c_get_clientdata(client);

	if (!smb349_chg) {
		pr_err("smb349_chg is not yet initialized\n");
		return false;
	}
/* uses under voltage status bit to detect cable plug or unplug */
	mutex_lock(&smb349_chg->lock);
	irq_status_e = smb349_chg->irqstat[IRQSTAT_E];
	mutex_unlock(&smb349_chg->lock);

	/* insert case is 0, remove case is 1 */
	power_ok = !(irq_status_e & 0x01);

	if (power_ok) {
		voltage = smb349_get_usbin_adc();
#if SMB349_BOOSTBACK_WORKAROUND
		smb349_pr_info("DC is present. DC_IN volt:%d\n", voltage);
#else
		pr_err("DC is present. DC_IN volt:%d\n", voltage);
#endif
	} else
		pr_err("DC is missing.\n");

	return power_ok;
}
#if defined(CONFIG_MAX17050_FUELGAUGE) || defined(CONFIG_VZW_LLK)
bool external_smb349_is_charger_present(void)
{
	return smb349_is_charger_present(the_smb349_chg->client);
}
#endif

static void smb349_pmic_usb_override_wrap(struct smb349_struct *smb349_chg, bool mode)
{
	int ret;

	if (!smb349_chg) {
		pr_err("failed that smb349_chg is null.\n");
		return;
	}

	if (smb349_chg->is_phy_forced_on ^ mode) {
		ret = smb349_pmic_usb_override(mode);
		if (ret) {
			pr_err("failed to smb349_pmic_usb_override(%d), ret:%d\n", mode, ret);
			return;
		}
		smb349_chg->is_phy_forced_on = mode;
	}
}

static int smb349_get_prop_charge_type(struct smb349_struct *smb349_chg)
{
	u8 status_c;
	enum smb349_chg_status status;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	bool chg_enabled;
	bool charger_err;
	struct i2c_client *client = smb349_chg->client;
	int ret;
	union power_supply_propval dwc3_usb_present = {0,};

	ret = smb349_read_reg(client, STATUS_C_REG, &status_c);
	if (ret) {
		pr_err("failed to read STATUS_C_REG. return charge unknown\n");
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
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
		power_supply_changed(&smb349_chg->batt_psy);
		wake_lock_timeout(&smb349_chg->uevent_wake_lock, HZ*2);
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
		if( !smb349_is_charger_present(client) ) {
			smb349_chg->usb_psy->get_property(smb349_chg->usb_psy,
			  POWER_SUPPLY_PROP_PRESENT, &dwc3_usb_present);
			if (dwc3_usb_present.intval == 1 ){
				smb349_chg->usb_present = false;
				smb349_pmic_usb_override_wrap(smb349_chg, false);
				wake_lock_timeout(&smb349_chg->uevent_wake_lock, HZ*2);
				power_supply_set_present(smb349_chg->usb_psy, false);
				pr_err("exception_handling : running w/r to update unplugged state\n");
			}
		}
		pr_err("exception_handling : unlock chg_wake_lock\n");
		power_supply_changed(&smb349_chg->batt_psy);
		wake_lock_timeout(&smb349_chg->uevent_wake_lock, HZ*2);
		wake_unlock(&smb349_chg->chg_wake_lock);
	}
	return chg_type;
}

static int smb349_get_prop_batt_present(struct smb349_struct *smb349_chg)
{
	u8 irq_b;
	bool batt_present;

	mutex_lock(&smb349_chg->lock);
	irq_b = smb349_chg->irqstat[IRQSTAT_B];
	mutex_unlock(&smb349_chg->lock);

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
int smb349_get_batt_temp_origin(void)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	int rc = 0;
	struct qpnp_vadc_result results;

	if(qpnp_vadc_is_ready() == 0) {
		rc = qpnp_vadc_read(LR_MUX1_BATT_THERM, &results);
		if (rc) {
			pr_debug("Unable to read batt temperature rc=%d\n", rc);
			pr_debug("Report last_bat_temp %d again\n", batt_temp_old);
			return batt_temp_old;
		} else {
			pr_debug("get_bat_temp %d %lld\n", results.adc_code, results.physical);
			batt_temp_old =(int)results.physical;
			return (int)results.physical;
		}
	} else {
		pr_err("vadc is not ready yet. report default temperature\n");
		return DEFAULT_TEMP;
	}
#else
	pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return DEFAULT_TEMP;
#endif
}
EXPORT_SYMBOL(smb349_get_batt_temp_origin);

static int smb349_get_prop_batt_temp(struct smb349_struct *smb349_chg)
{
	if (pseudo_batt_info.mode) {
		pr_debug("battery fake mode : %d \n", pseudo_batt_info.mode);
		return pseudo_batt_info.temp * 10;
	} else if (is_factory_cable()) {
		pr_debug("factory cable : %d \n", DEFAULT_TEMP / 10);
		return DEFAULT_TEMP;
	}

	if (!smb349_get_prop_batt_present(smb349_chg)) {
		pr_err("Battery is missed, report default temperature\n");
		return DEFAULT_TEMP;
	}

	/* Approach adc channel for read batt temp' */
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	return smb349_get_batt_temp_origin();
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

#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	if (((smb349_chg->usb_present) ||(smb349_chg->wlc_present))&& (smb349_chg->pseudo_ui_chg))
		return POWER_SUPPLY_STATUS_CHARGING;
#else
	if (smb349_chg->usb_present && smb349_chg->pseudo_ui_chg)
		return POWER_SUPPLY_STATUS_CHARGING;
#endif

	if (chg_type == POWER_SUPPLY_CHARGE_TYPE_UNKNOWN ||
		chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE) {
#ifdef CONFIG_SMB349_VZW_FAST_CHG
			if ((smb349_chg->usb_present) || (chg_state == VZW_NOT_CHARGING)
					|| (chg_state == VZW_USB_DRIVER_UNINSTALLED))
#else
#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
			if ((smb349_chg->usb_present) || (smb349_chg->wlc_present))
#else
			if (smb349_chg->usb_present)
#endif
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
#if SMB349_BOOSTBACK_WORKAROUND
	smb349_bb_param param;
#endif

	pr_debug("enable=%d.\n", enable);

	if (smb349_chg->chg_timeout) {
		pr_err("charging timeout state, never enable charging\n");
		return 0;
	}

	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
						CHG_ENABLE_BIT, val);
	if (ret) {
		pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);
		return ret;
	}

	smb349_chg->charging_disabled = !enable;

#if SMB349_BOOSTBACK_WORKAROUND
	if (smb349_chg->is_bb_work_case) {
		BB_WORK_TRIGGER_PARAM_SET(param, 0, 0x00, 1);
		smb349_bb_worker_trigger(smb349_chg, param);
	}
#endif

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

#if defined(CONFIG_MACH_MSM8974_G2_KR)
bool smb349_get_charger_error(struct smb349_struct *smb349_chg)
{
	int ret = 0;
	u8 status_c = 0;
	bool charger_err = false;

	ret = smb349_read_reg(smb349_chg->client, STATUS_C_REG, &status_c);
	if (ret) {
		pr_err("failed to read STATUS_C_REG.\n");
		return false;
	}

	charger_err = (bool) (status_c & (1<<6));

	if (charger_err)
		return true;
	else
		return false;
}

bool smb349_get_thermal_regulation_status(struct smb349_struct *smb349_chg)
{
	int ret = 0;
	u8 status_a = 0;
	bool thm_regulation = false;

	ret = smb349_read_reg(smb349_chg->client, STATUS_A_REG, &status_a);
	if (ret) {
		pr_err("failed to read STATUS_C_REG.\n");
		return false;
	}

	thm_regulation = (bool) (status_a & (1<<6));

	if (thm_regulation)
		return true;
	else
		return false;
}

static void smb349_batt_remove_insert_cb(int batt_present)
{
	bool charger = false;
	bool ftm_cable = is_factory_cable();
	bool charger_err = false;
	bool thm_regulation = false;

	if (!the_smb349_chg) {
		pr_err("smb349 device is not exist.\n");
		return;
	}

	/* Here comes into just battery missing status only */
	wake_lock(&the_smb349_chg->battgone_wake_lock);

	charger = smb349_is_charger_present(the_smb349_chg->client);
	charger_err = smb349_get_charger_error(the_smb349_chg);
	thm_regulation = smb349_get_thermal_regulation_status(the_smb349_chg);

	printk(KERN_ERR "[PM] Battery status(%d-%d-%d-%d-%d-%d-%d)\n",
		batt_present, charger_err ? 1 : 0, the_smb349_chg->chg_timeout ? 1 : 0,
		thm_regulation ? 1 : 0, charger, ftm_cable ? 1 : 0, pseudo_batt_info.mode);

	/*
	 * Battery missing work-aronnd.
	 * this func' should be excute after first battery missing detected.
	 * when, re-check battery rt status, still,
	 *
	 * 1) Battery missing : power off.
	 *
	 * 2) Battery present : check charger error.
	 *    (abnoraml scene - No charger error)
	 *    (real battery remove/insert during re-check - charger error)
	 *
	 *    - No charger error                : not power off(abnormal scene).
	 *    - Charger error by chg timeout    : not power off(should be restored).
	 *    - Charger error by thermal regul' : not power off(by smb349 thermal).
	 *    - Charger error by others         : power off(assume batt remove).
	 */
	if ((!batt_present ||
		(batt_present && (charger_err && !the_smb349_chg->chg_timeout)) ||
		(batt_present && (charger_err && !thm_regulation))) &&
		charger && (!ftm_cable) && (!pseudo_batt_info.mode)) {
		printk(KERN_ERR "[PM] Now reset as scenario.!!\n");
		switch_set_state(&the_smb349_chg->batt_removed, 1);
		power_supply_changed(&the_smb349_chg->batt_psy);

		/* makes logger save time margin */
		msleep(5000);

		/* use oem-11 restart reason for battery remove insert irq */
		kernel_restart("oem-11");
		wake_unlock(&the_smb349_chg->battgone_wake_lock);
	} else {
		printk(KERN_ERR "[PM] Do not power off.\n");
		wake_unlock(&the_smb349_chg->battgone_wake_lock);
	}
}
#endif
static ssize_t batt_removed_print_name(struct switch_dev *sdev, char *buf){
	return sprintf(buf, "%s\n","battery_removed");
}

static ssize_t batt_removed_print_state(struct switch_dev *sdev, char *buf){
	return sprintf(buf, "%d\n",sdev->state);
}

static ssize_t at_chg_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r;
	bool b_chg_ok = false;
	int chg_type;

	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	chg_type = smb349_get_prop_charge_type(the_smb349_chg);
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

	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (strncmp(buf, "0", 1) == 0) {
		/* stop charging */
		pr_info("[Diag] stop charging start\n");
		ret = smb349_enable_charging(the_smb349_chg, false);

	} else if (strncmp(buf, "1", 1) == 0) {
		/* start charging */
		pr_info("[Diag] start charging start\n");
		ret = smb349_enable_charging(the_smb349_chg, true);
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

	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return -EINVAL;
	}
#if defined(CONFIG_MAX17050_FUELGAUGE)
	guage_level = max17050_get_soc_for_charging_complete_at_cmd();
#else
	guage_level = smb349_get_prop_batt_capacity(the_smb349_chg);
#endif

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

	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (strncmp(buf, "0", 1) == 0) {
		/* charging not complete */
		pr_info("[Diag] charging not complete start\n");
		ret = smb349_enable_charging(the_smb349_chg, true);
	} else if (strncmp(buf, "1", 1) == 0) {
		/* charging complete */
		pr_info("[Diag] charging complete start\n");
		ret = smb349_enable_charging(the_smb349_chg, false);
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
DEVICE_ATTR(at_charge, 0644, at_chg_status_show, at_chg_status_store);
DEVICE_ATTR(at_chcomp, 0644, at_chg_complete_show, at_chg_complete_store);
DEVICE_ATTR(at_pmrst, 0644, at_pmic_reset_show, NULL);

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

		ret = smb349_masked_write(the_smb349_chg->client, STATUS_IRQ_REG,
			TERM_TAPER_CHG_IRQ_SET_BIT | FAST_CHG_IRQ_SET_BIT,
			TERM_TAPER_CHG_IRQ_SET_BIT | FAST_CHG_IRQ_SET_BIT);
		if (ret) {
			pr_err("Failed to set status_irq_reg ret=%d\n", ret);
			return -EINVAL;
		} else {
			pr_info("enable term or tapper chg irq, fast chg irq\n");
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

		ret = smb349_masked_write(the_smb349_chg->client, STATUS_IRQ_REG,
			TERM_TAPER_CHG_IRQ_SET_BIT | FAST_CHG_IRQ_SET_BIT, 0);
		if (ret) {
			pr_err("Failed to set status_irq_reg ret=%d\n", ret);
			return -EINVAL;
		} else {
			pr_info("disable term or tapper chg irq, fast chg irq\n");
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

#define FACTORY_LOWEST_CHG_CURRENT_130K     500
#define FACTORY_LOWEST_CHG_CURRENT_OTHERS   300
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

	if (is_factory_cable()) {
		pr_err("thermal-engine chg current control not permitted\n");
		return 0;
	} else {
		the_smb349_chg->chg_current_te = smb349_thermal_mitigation;

		cancel_delayed_work_sync(&the_smb349_chg->battemp_work);
		schedule_delayed_work(&the_smb349_chg->battemp_work, HZ*1);
	}
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

	if (pseudo_batt_info.mode) {
		pr_err("Fake batt enter charging timeout, disable charging and locked\n");
		wake_lock(&the_smb349_chg->chg_timeout_lock);
	} else {
#if !defined(CONFIG_MACH_MSM8974_G2_VZW) && !defined(CONFIG_MACH_MSM8974_G2_SPR) \
	&& !defined(CONFIG_MACH_MSM8974_G2_TMO_US) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) \
	&& !defined(CONFIG_MACH_MSM8974_G2_ATT) && !defined(CONFIG_MACH_MSM8974_G2_CA)
		pr_err("enter charging timeout, disable charging and locked\n");
		wake_lock(&the_smb349_chg->chg_timeout_lock);
#endif
	}

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

	if ((abs(g_batt_vol - g_batt_pre_vol) >= 50) ||
			g_batt_soc != g_batt_pre_soc) {
		g_batt_pre_vol = g_batt_vol;
		g_batt_pre_soc = g_batt_soc;
		val = true;
	} else
		val = false;

	return val;

}
#endif

#ifdef CONFIG_SMB349_VZW_FAST_CHG
/*
 * Depending on AICL reult, change the charging status.
 * AICL result < 1600mA : VZW_UNDER_CURRENT_CHARGING
 * AICL result >= 1600mA : VZW_NORMAL_CHARGONG
 */
static void vzw_fast_chg_change_state(struct smb349_struct *smb349_chg, u8 val)
{
	if (val < 0x07) {
		chg_state = VZW_UNDER_CURRENT_CHARGING;
		pr_info("VZW_UNDER_CURRENT_CHARGING : 0x%x\n", val);
	} else if (val >= 0x07) {
		chg_state = VZW_NORMAL_CHARGING;
		pr_info("VZW_NORMAL_CHARGING : 0x%x\n", val);
	}
	vzw_chg_present = CHARGER_PRESENT;

	power_supply_changed(&smb349_chg->batt_psy);
	return;
}
#endif

#ifdef CONFIG_MAX17050_FUELGAUGE
#define HEAVY_SYSTEM_LOAD -1000
#define TYPICAL_SYSTEM_LOAD -400
#define MAX17050_SOC_MS    30000
#define MID_MAX17050_SOC_MS 20000
#define LOW_MAX17050_SOC_MS	10000
static void max17050_soc(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct smb349_struct *chip = container_of(dwork,
				struct smb349_struct, max17050_soc_work);

#ifdef CONFIG_MAX17050_FUELGAUGE
	if (check_fuel_gauage_update_data() == true) {
		pr_info("batt_soc(%d), batt_vol(%d), batt_current(%d), batt_age(%d)\n",
			g_batt_soc, g_batt_vol, g_batt_current, g_batt_age);
		power_supply_changed(&chip->batt_psy);
	}
#else
	power_supply_changed(&chip->batt_psy);
#endif

	if (!smb349_is_charger_present(chip->client)) {
		if (g_batt_current < HEAVY_SYSTEM_LOAD)
			schedule_delayed_work(&chip->max17050_soc_work,
					      msecs_to_jiffies(LOW_MAX17050_SOC_MS));
		else if (g_batt_current < TYPICAL_SYSTEM_LOAD) {
			if (g_batt_soc > 5)
				schedule_delayed_work(&chip->max17050_soc_work,
					      msecs_to_jiffies(MID_MAX17050_SOC_MS));
			else
				schedule_delayed_work(&chip->max17050_soc_work,
					      msecs_to_jiffies(LOW_MAX17050_SOC_MS));
		}
		else {
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
	} else {
		if ( g_batt_current > 1000 )
			schedule_delayed_work(&chip->max17050_soc_work,
				      msecs_to_jiffies(LOW_MAX17050_SOC_MS));
		else
			schedule_delayed_work(&chip->max17050_soc_work,
				      msecs_to_jiffies(MID_MAX17050_SOC_MS));
	}

}
#endif

#ifdef CONFIG_SMB349_VZW_FAST_CHG
/*
 * Detect VZW incompatible charger.
 * If the capacity of chager is under 500mA, that charger is incompatible.
 * 1. Check the SMB349 'DCIN under-voltage status' five times.
 * 2. Determine the charger is incompatible, if DCIN status bit is changed in ten times.
 */
static int vzw_fast_chg_detect_incompatible_charger(struct i2c_client *client)
{
	int ret, count = 0, threshold= 4400000, hw_rev = 0;

#ifdef CONFIG_MACH_MSM8974_G2_VZW
	if(lge_get_board_revno() >= HW_REV_E) {
		/*for smb349 A4 version*/
		hw_rev = 1;
		pr_info("HW_REV : %d\n", lge_get_board_revno());
	} else {
		/*for smb349 A6 version*/
		threshold = 3750000;
		pr_info("HW_REV : %d\n", lge_get_board_revno());
	}
#endif

	do{
		ret = smb349_get_usbin_adc();
		if (ret < threshold) {
			pr_info("incompatible charger is detected , DCIN : %d\n", ret);
			if (hw_rev) {
				chg_state = VZW_NOT_CHARGING;
				pr_info("chg_state = %d\n", chg_state);
			}
			return 0;
		}
		count += 1;
		msleep(10);
	}while(count < 5);

	return 1;
}
#endif

#if SMB349_BOOSTBACK_WORKAROUND
#define AICL_INC 1
#define AICL_DEC 0
static int smb349_aicl_dynamic_switch(struct smb349_struct *smb349_chg, int mode)
{
	int ret;
	u8  val;

	ret = smb349_read_reg(smb349_chg->client, VAR_FUNC_REG, &val);
	if (ret) {
		pr_err("smb349_read_reg failed: reg=%03X, ret=%d\n", VAR_FUNC_REG, ret);
		return -100;
	}

	if (mode == AICL_INC) {
		if ( !(val & OPTICHG_DET_THR_BIT) ) {
			val |= OPTICHG_DET_THR_BIT;
			ret = smb349_write_reg(smb349_chg->client, VAR_FUNC_REG, val);
			if (ret) {
				pr_err("smb349_write_reg failed: reg=%03X, val=%02X, ret=%d\n",
						VAR_FUNC_REG, val, ret);
				return -300;
			}
		}
	} else if (mode == AICL_DEC) {
		if ( val & OPTICHG_DET_THR_BIT ) {
			val &= ~OPTICHG_DET_THR_BIT;

			/* in decreasing case, need to toggle aicl disable and disable
			   to recover aicl result */
			val &= ~OPTICHG_ENABLE_BIT;
			ret = smb349_write_reg(smb349_chg->client, VAR_FUNC_REG, val);
			if (ret) {
				pr_err("smb349_write_reg failed: reg=%03X, val=%02X, ret=%d\n",
						VAR_FUNC_REG, val, ret);
				return -400;
			}

			val |= OPTICHG_ENABLE_BIT;
			ret = smb349_write_reg(smb349_chg->client, VAR_FUNC_REG, val);
			if (ret) {
				pr_err("smb349_write_reg failed: reg=%03X, val=%02X, ret=%d\n",
						VAR_FUNC_REG, val, ret);
				return -500;
			}
		}
	} else {
		pr_err("error unknown param mode : %d\n", mode);
		return -200;
	}

	return 0;
}

static void smb349_dump_register(struct smb349_struct *smb349_chg, char *msg)
{
	int i;
	u8 val;

	pr_info("%s\n", msg);
	for (i = 0 ; i < ARRAY_SIZE(smb349_debug_regs) ; i++) {
		smb349_read_reg(smb349_chg->client, smb349_debug_regs[i].reg, &val);
		pr_err("[DUMP] %s: 0x%02X\n", smb349_debug_regs[i].name, val);
	}
}
#define BB_POLLING_PERIOD HZ
#define DISCHARGE_CURRENT 50
static void smb349_bb_worker(struct work_struct *work)
{
	struct smb349_struct *smb349_chg =
		container_of(work, struct smb349_struct, bb_work.work);

	static int irq_chk_delay = 0;
	static unsigned long prev_irq_cnt = 0;
	static bool one_shot_dump = true;

	int chg_current;
	int ret;

	chg_current = smb349_get_prop_batt_current_now(smb349_chg);
	smb349_console_silent = 1;

	if (smb349_chg->irq_trigger_cnt > (ULONG_MAX - 100)) {
		smb349_chg->irq_trigger_cnt = 1;
	}

	if (chg_current < DISCHARGE_CURRENT * 1000) {
		pr_debug("discharging case\n");
		if (!irq_chk_delay) {
			prev_irq_cnt = smb349_chg->irq_trigger_cnt;
		}
		irq_chk_delay++;
		irq_chk_delay = irq_chk_delay % 2;

		smb349_console_silent = 1;
		ret = smb349_aicl_dynamic_switch(smb349_chg, AICL_INC);
		if (ret) {
			pr_err("failed to dynamically change aicl thresh ret : %d\n", ret);
			return;
		}

		msleep(20);

		if (smb349_chg->irq_trigger_cnt == 0 || (!irq_chk_delay
			&& prev_irq_cnt == smb349_chg->irq_trigger_cnt)) {
			if (!smb349_is_charger_present_rt(smb349_chg->client)) {
				pr_err("!!!ERROR FATAL smb349 doesn't occur IRQ prev_cnt:%lu, irq_cnt:%lu unplugged\n",
					prev_irq_cnt, smb349_chg->irq_trigger_cnt);
				smb349_console_silent = 0;
				smb349_dump_register(smb349_chg, "unplugged case before call irq_worker");
				schedule_delayed_work(&smb349_chg->irq_work, 0);
				smb349_dump_register(smb349_chg, "unplugged case after call irq_worker");
				pr_err("stat gpio value : %d\n", gpio_get_value(smb349_chg->stat_gpio));
				smb349_pmic_reg_dump();
			} else {
				if (one_shot_dump) {
					pr_err("!!!ERROR FATAL smb349 doesn't occur IRQ prev_cnt:%lu, irq_cnt:%lu plugged\n",
						prev_irq_cnt, smb349_chg->irq_trigger_cnt);
					smb349_dump_register(smb349_chg, "plugged case");
					pr_err("stat gpio value : %d\n", gpio_get_value(smb349_chg->stat_gpio));
					smb349_pmic_reg_dump();
					one_shot_dump = false;
				}
			}
		}

		ret = smb349_aicl_dynamic_switch(smb349_chg, AICL_DEC);
		if (ret) {
			pr_err("failed to dynamically change aicl thresh ret : %d\n", ret);
			return;
		}
	}

	schedule_delayed_work(&smb349_chg->bb_work, BB_POLLING_PERIOD);
}

static int smb349_get_vbat_adc(void)
{
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
       struct qpnp_vadc_result results;
       int rc = 0;

       if (qpnp_vadc_is_ready() == 0) {
               rc = qpnp_vadc_read(VBAT_SNS, &results);
               if (rc) {
                       pr_err("Unable to read vbat_sns adc rc=%d\n", rc);
                       return -100;
               }
               else {
                       pr_debug("SMB vbat_sns voltage: %lld\n", results.physical);
                       return results.physical;
               }
       } else {
               pr_err("vadc is not ready yet.\n");
               return -200;
       }
#else
       pr_err("CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
       return -300;
#endif
}

/* [smb349_bb_rechg_worker for corner case]
 * When DC_IN voltage is lower than 4.5V at EOC,
 * Charger doesn't re-charge, so need to monitor for rechg */
#define SMB349_BB_RECHG_VOLT_TRESH 4200000
#define SMB349_BB_DCIN_VOLT_TRESH  4500000
#define SMB349_BB_RECHG_POLLING_PERIOD (60 * HZ)
static void smb349_bb_rechg_worker(struct work_struct *work)
{
	struct smb349_struct *smb349_chg =
		container_of(work, struct smb349_struct, bb_rechg_work.work);
	int vbat_volt;
	int dcin_volt;

	vbat_volt = smb349_get_vbat_adc();
	if (vbat_volt < 0) {
		pr_err("failed to read vbat_adc ret:%d\n", vbat_volt);
		vbat_volt = 5000000;
	}
	dcin_volt = smb349_get_usbin_adc();
	if (dcin_volt < 0) {
		pr_err("failed to read dcin_volt ret:%d\n", dcin_volt);
		dcin_volt = 0;
	}
	smb349_chg->is_rechg_work_trigger = false;

	pr_debug("vbat:%d, dcin:%d\n", vbat_volt/1000, dcin_volt/1000);
	if (vbat_volt <= SMB349_BB_RECHG_VOLT_TRESH) { /* rechg case */
		pr_debug("kicking AICL Low\n");
		smb349_aicl_dynamic_switch(smb349_chg, AICL_DEC);
	} else if (dcin_volt < SMB349_BB_DCIN_VOLT_TRESH) {
		pr_debug("worker scheduled\n");
		schedule_delayed_work(&smb349_chg->bb_rechg_work, SMB349_BB_RECHG_POLLING_PERIOD);
		smb349_chg->is_rechg_work_trigger = true;
	}
}

static void smb349_bb_worker_trigger(struct smb349_struct *smb349_chg,
		smb349_bb_param param)
{
	static bool is_bb_worker_running = false;
	static bool is_bb_worker_eoc = false;

	/* below code is for debug msg.
	 * After verifying code, this code will be delete. */
	int dbg_pre_running = is_bb_worker_running;
	int dbg_pre_eoc = is_bb_worker_eoc;
	char *dbg_msg;
	int dbg_state = 0;

	if (param.caller == 0) { /* called from bottom half */
		if (param.usb_present) {
			if (param.val & BIT(1)) { /* Charging TERM */
				dbg_state = 1;
				cancel_delayed_work_sync(&smb349_chg->bb_work);
				smb349_aicl_dynamic_switch(smb349_chg, AICL_INC);
				is_bb_worker_running = false;
				is_bb_worker_eoc = true;
				schedule_delayed_work(&smb349_chg->bb_rechg_work, 0);
			} else if (is_bb_worker_eoc) { /* waiting for Re-chg */
				dbg_state = 2;
				if (param.val & BIT(5)) {
					dbg_state = 3;
					smb349_aicl_dynamic_switch(smb349_chg, AICL_DEC);
					if (!delayed_work_pending(&smb349_chg->bb_work))
						schedule_delayed_work(&smb349_chg->bb_work, BB_POLLING_PERIOD);
					else
						dbg_state = 33;
					is_bb_worker_running = true;
					is_bb_worker_eoc = false;
				}
			} else if (!is_bb_worker_running && !smb349_chg->charging_disabled) {
				dbg_state = 4;
				if (!delayed_work_pending(&smb349_chg->bb_work))
					schedule_delayed_work(&smb349_chg->bb_work, BB_POLLING_PERIOD);
				else
					dbg_state = 44;
				is_bb_worker_running = true;
			} else {
				dbg_state = 5;
			}
		} else { /* usb unplug case */
			dbg_state = 6;
			cancel_delayed_work_sync(&smb349_chg->bb_work);
			smb349_aicl_dynamic_switch(smb349_chg, AICL_DEC);
			is_bb_worker_running = false;
			is_bb_worker_eoc = false;
		}
	} else if (param.caller == 1) { /* called from smb349_enable_charging */
		if (smb349_chg->charging_disabled) { /* charging disable */
			if (is_bb_worker_running) {
				dbg_state = 7;
				cancel_delayed_work_sync(&smb349_chg->bb_work);
				smb349_aicl_dynamic_switch(smb349_chg, AICL_INC);
				is_bb_worker_running = false;
				is_bb_worker_eoc = false;
			} else {
				dbg_state = 8;
			}
		} else { /* charging enable */
			if ( !is_bb_worker_running &&
					smb349_is_charger_present(smb349_chg->client) ) {
				dbg_state = 9;
				smb349_aicl_dynamic_switch(smb349_chg, AICL_DEC);
				is_bb_worker_running = true;
				if (!delayed_work_pending(&smb349_chg->bb_work))
					schedule_delayed_work(&smb349_chg->bb_work, BB_POLLING_PERIOD);
				else
					dbg_state = 99;
			} else {
				dbg_state = 10;
			}
		}
	}

	smb349_chg->global_is_bb_worker_eoc = is_bb_worker_eoc;

	switch (dbg_state) {
	case 1:
		dbg_msg = "term cancel polling from bh"; break;
	case 2:
		dbg_msg = "term state wait rechg irq from bh"; break;
	case 3:
		dbg_msg = "rechg sched pollling from bh"; break;
	case 33:
		dbg_msg = "rechg pending sched pollling from bh"; break;
	case 4:
		dbg_msg = "plug-in sched polling from bh"; break;
	case 44:
		dbg_msg = "plug-in pending sched polling from bh"; break;
	case 5:
		dbg_msg = "already polling triggered from bh"; break;
	case 6:
		dbg_msg = "unplug cancel polling from bh"; break;
	case 7:
		dbg_msg = "chg disable cancel polling"; break;
	case 8:
		dbg_msg = "chg disable during no polling"; break;
	case 9:
		dbg_msg = "chg enable sched polling"; break;
	case 99:
		dbg_msg = "chg enable pending sched polling due to pending"; break;
	case 10:
		dbg_msg = "chg enable running polling or dc not present"; break;
	default :
		dbg_msg = "unknown state"; break;
	}

	if (dbg_state != 5 && dbg_state != 10) {
		pr_info("state<run-eoc>(%d-%d)->(%d-%d), %s\n", dbg_pre_running,
				dbg_pre_eoc, is_bb_worker_running, is_bb_worker_eoc, dbg_msg);
	}
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
#if SMB349_BOOSTBACK_WORKAROUND
	smb349_bb_param param;
#endif

	u8 irqstat[IRQSTAT_NUM];
	/* for clearing IRQ, status register block reads.
	 * 35h - IRQ status A - irqstat[0]
	 * 36h - IRQ status B - irqstat[1]
	 * 37h - IRQ status C - irqstat[2]
	 * 38h - IRQ status D - irqstat[3]
	 * 39h - IRQ status E - irqstat[4]
	 * 3Ah - IRQ status F - irqstat[5]
	 */
	ret = smb349_read_block_reg(smb349_chg->client, IRQ_A_REG, IRQSTAT_NUM, irqstat);
	if (ret) {
		pr_err("Failed to read IRQ status block = %d\n", ret);
		return;
	}

#if SMB349_BOOSTBACK_WORKAROUND
	/* during silent pr_info,
	   have to monitor rechg&term, timeout, dc_in under&over */
	if ( smb349_console_silent && ( (irqstat[IRQSTAT_C] & 0x22 ) ||
			(irqstat[IRQSTAT_D] & 0x0A) || (irqstat[IRQSTAT_E] & 0x0A ) ))
		smb349_console_silent = 0;

	smb349_pr_info("[IRQ 35h~3Ah] A:0x%02X, B:0x%02X, C:0x%02X, D:0x%02X, E:0x%02X, F:0x%02X\n",
		irqstat[0],irqstat[1], irqstat[2], irqstat[3], irqstat[4], irqstat[5]);
#else
	pr_info("[IRQ 35h~3Ah] A:0x%02X, B:0x%02X, C:0x%02X, D:0x%02X, E:0x%02X, F:0x%02X\n",
		irqstat[0],irqstat[1], irqstat[2], irqstat[3], irqstat[4], irqstat[5]);
#endif

	/* store irqstatus */
	smb349_irqstat_store(smb349_chg, irqstat);

	/* 3Ah - IRQ status F - irqstat[5] */
	if ((irqstat[IRQSTAT_F] & OTG_OC_LIMIT_MASK) || (irqstat[IRQSTAT_F] & OTG_BATT_UV_MASK)) {
		if (irqstat[IRQSTAT_F] & OTG_OC_LIMIT_MASK) {
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
		} else if (irqstat[IRQSTAT_F] & OTG_BATT_UV_MASK) {
			pr_err("OTG_BATT_UNDER_VOLT > otg is disabled.\n");

			host_mode = smb349_chg_is_otg_active(smb349_chg);
			pr_err("smb349_irq_worker triggered: %d host_mode: %d\n",
					usb_present, host_mode);

			if (host_mode)
				return;
		}
	}

	/* 38h - IRQ status D - irqstat[3] */
	/* Timeout handling */
	if ( (irqstat[IRQSTAT_D] & (BIT(0) | BIT(2))) ) {
		pr_info("[TIMEOUT] timeout val : 0x%02X\n", irqstat[IRQSTAT_D]);
		smb349_chg_timeout(0);
	}

	if ( irqstat[IRQSTAT_D] & BIT(5) ) {
		ret = smb349_read_reg(smb349_chg->client, STATUS_E_REG, &val);
		if (ret < 0)
			pr_err("Failed to AICL result rc=%d\n", ret);

		val &= SMB349_MASK(4,0);
#if SMB349_BOOSTBACK_WORKAROUND
		smb349_pr_info("[smb349] AICL result : %dmA(0x%02X)\n",
				smb349_aicl_result(val), val);
#else
		pr_info("[smb349] AICL result : %dmA(0x%02X)\n",
				smb349_aicl_result(val), val);
#endif
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		if (!(vzw_chg_present == USB_PRESENT))
			vzw_fast_chg_change_state(smb349_chg, val);
	}
#if SMB349_BOOSTBACK_WORKAROUND
	else if (vzw_chg_present == UNKNOWN_PRESENT)
		count_no_aicl += 1;
#endif
#else
	}
#endif

	if ( !(irqstat[IRQSTAT_E] & 0x01) ) {
#if SMB349_BOOSTBACK_WORKAROUND
		smb349_pr_info("[BH] DC is present. DC_IN volt:%d\n", smb349_get_usbin_adc());
#else
		pr_info("[BH] DC is present. DC_IN volt:%d\n", smb349_get_usbin_adc());
#endif
		usb_present = 1;
	} else {
		pr_info("[BH] DC is missing.\n");
		usb_present = 0;
	}

#if SMB349_BOOSTBACK_WORKAROUND
	if (smb349_chg->is_bb_work_case) {
		BB_WORK_TRIGGER_PARAM_SET(param, usb_present, irqstat[IRQSTAT_C], 0);
		smb349_bb_worker_trigger(smb349_chg, param);
	}
#endif

#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	wlc_present = wireless_charging || is_wireless_charger_plugged() ;
#endif

	pr_debug("smb349_chg->usb_present = %d, usb_preset = %d.\n",
			smb349_chg->usb_present, usb_present);

#if defined(CONFIG_BQ51053B_CHARGER) && defined(CONFIG_WIRELESS_CHARGER)
	/*If below codes are changed ,
	 *we have to update change in set_usb_present()*/
	if ( (smb349_chg->usb_present ^ usb_present) ||
			(smb349_chg->wlc_present ^ wlc_present) ||
			(smb349_chg->chg_status == SMB_CHG_STATUS_EXCEPTION && !usb_present) ) {
		smb349_pmic_usb_override_wrap(smb349_chg, usb_present);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		cancel_delayed_work_sync(&smb349_chg->battemp_work);
		schedule_delayed_work(&smb349_chg->battemp_work, HZ*1);
		if (!usb_present &&
			wake_lock_active(&smb349_chg->lcs_wake_lock))
			wake_unlock(&smb349_chg->lcs_wake_lock);
#endif
		wake_lock_timeout(&smb349_chg->uevent_wake_lock, HZ*2);
		smb349_chg->wlc_present = wlc_present;

		if(wlc_present){
			pr_err("[WLC] set usb_present to 0 \n");
			smb349_chg->usb_present = 0;
		}else{
			smb349_chg->usb_present = usb_present;
		}
			power_supply_set_present(smb349_chg->usb_psy,
					smb349_chg->usb_present);
	}
#else
	if ( smb349_chg->usb_present ^ usb_present ||
		(smb349_chg->chg_status == SMB_CHG_STATUS_EXCEPTION && !usb_present) ) {
		smb349_pmic_usb_override_wrap(smb349_chg, usb_present);
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

/*
 * The STAT pin is low when charging and high when not charging.
 * When the smb350 start/stop charging the STAT pin triggers an interrupt.
 * Interrupt is triggered on both rising or falling edge.
 */
static irqreturn_t smb349_irq(int irq, void *dev_id)
{
	struct smb349_struct *smb349_chg = dev_id;

	pr_debug("smb349_irq\n");

#if SMB349_BOOSTBACK_WORKAROUND
	smb349_chg->irq_trigger_cnt++;
#endif
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

	/* Complete timeout 764m, pre-charge timeout disable */
	ret = smb349_masked_write(smb349_chg->client, STAT_TIMER_REG,
				COMPETE_CHG_TIMEOUT_BIT | PRE_CHG_TIMEOUT_BIT, 0x7);
	if (ret) {
		pr_err("Failed to set CHG_TIMEOUT_SET rc=%d\n", ret);
		return ret;
	}

	/* set Charge timeout bit */
	ret = smb349_masked_write(smb349_chg->client, STATUS_IRQ_REG,
				CHG_TIMEOUT_BIT, 0x80);
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

#if SMB349_BOOSTBACK_WORKAROUND
	ret = smb349_masked_write(smb349_chg->client, STATUS_IRQ_REG,
				TERM_TAPER_CHG_IRQ_SET_BIT | MISSING_BATT_BIT,
				TERM_TAPER_CHG_IRQ_SET_BIT | MISSING_BATT_BIT);
	if (ret) {
		pr_err("Failed to set TERM_TAPER | INOK | MISSING_BATT rc=%d\n", ret);
		return ret;
	} else {
		pr_info("enable irq (term, tapper and rechg for bb) + missing");
	}
#else
	ret = smb349_masked_write(smb349_chg->client, STATUS_IRQ_REG,
				INOK_BIT | MISSING_BATT_BIT,
				INOK_BIT | MISSING_BATT_BIT);
	if (ret) {
		pr_err("Failed to set INOK | MISSING_BATT rc=%d\n", ret);
		return ret;
	} else {
		pr_info("power ok + missing\n");
	}
#endif

	return 0;
}

static int smb349_set_fault_irq(struct smb349_struct *smb349_chg)
{
	int ret;

	ret = smb349_masked_write(smb349_chg->client, FAULT_IRQ_REG,
		OTG_BATT_FAIL_UVLO_BIT | OTG_OVER_CURRENT_LIMIT_BIT |
		AICL_COMPLETE_IRQ_SET_BIT | INPUT_UNDER_IRQ_SET_BIT |
		INPUT_OVER_IRQ_SET_BIT,
		OTG_BATT_FAIL_UVLO_BIT | OTG_OVER_CURRENT_LIMIT_BIT |
		AICL_COMPLETE_IRQ_SET_BIT | INPUT_UNDER_IRQ_SET_BIT |
		INPUT_OVER_IRQ_SET_BIT);

	if (ret) {
		pr_err("Failed to set OTG_BATT_FAIL_UVLO | OTG_OVER_CURRENT_LIMIT rc=%d\n", ret);
		return ret;
	}
	pr_info("This version used to under voltage IRQ.\n");
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

/* @val must be OPTICHG_DET_THR_BIT or 0.
*   OPTICHG_DET_THR_BIT - 4.5V
*   0 - 4.25V
*/
static int smb349_set_optichg_det_thr(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	if (val != OPTICHG_DET_THR_BIT)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, VAR_FUNC_REG,
						OPTICHG_DET_THR_BIT, val);
	pr_info("AICL threshold val : 0x%02X\n", val);
	if (ret) {
		pr_err("Failed to set OPTICHG_DET_THR_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static void smb349_version_check(struct smb349_struct *smb349_chg)
{
	bool is_major_a4 = false;
	bool is_minor_old = false;
	u8   val;
	char *str;
	int  ret;

	ret = smb349_read_reg(smb349_chg->client, CHIP_MAJOR_REG, &val);
	if (ret) {
		pr_err("fail to read chip major number ret:%d\n", ret);
	} else {
		switch ( (val & SMB349_MASK(3, 0)) ) {
			case 0x4:
				str = "A4";
				is_major_a4 = true;
				break;
			case 0x6:
				str = "A6";
				break;
			default:
				str = "unknown";
				break;
		}
		pr_info("chip major number : 0x%02X(%s)\n", val, str);
		/* only check minor number at A4 chip */
		if (is_major_a4) {
			ret = smb349_read_reg(smb349_chg->client, CHIP_MINOR_REG, &val);
			if (ret) {
				pr_err("fail to read chip minor number ret:%d\n", ret);
			} else {
				is_minor_old = val & BIT(5);
				pr_info("chip minor number : 0x%02X(%s)", val,
						is_minor_old? "old" : "new");
			}
		}
	}
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

#ifdef CONFIG_VZW_LLK
	ret = smb349_masked_write(smb349_chg->client, CHG_CTRL_REG,
				BIT(7), BIT(7));
	if (ret) {
		pr_err("Failed to set CURR_TERM_END_CHG_BIT rc=%d\n", ret);
		return ret;
	}
#endif

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
#ifdef CONFIG_SMB349_VZW_FAST_CHG
	ret = smb349_set_usb_5_1_mode(smb349_chg, 0);
	if (ret) {
		pr_err("Failed to set USB_5_1_MODE rc=%d\n", ret);
		return ret;
	}
#else
	ret = smb349_set_usb_5_1_mode(smb349_chg, 1);
	if (ret) {
		pr_err("Failed to set USB_5_1_MODE rc=%d\n", ret);
		return ret;
	}
#endif

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

	/* set AICL detect threshold to 4.25V */
	ret = smb349_set_optichg_det_thr(smb349_chg, 0);
	if (ret) {
		pr_err("Failed to set AICL threshold=%d\n", ret);
		return ret;
	} else {
		pr_info("AICL detect threshold 4.25V setted\n");
	}

#if SMB349_BOOSTBACK_WORKAROUND
	if(smb349_pmic_batt_present()) {
			ret = smb349_masked_write(smb349_chg->client, CTRL_FUNCTIONS_REG,
					DISABLE_CHG_INPUTFET, DISABLE_CHG_INPUTFET);
			if (ret) {
				pr_err("can't set disable_chg_inputfet for BOOSTBACK, reg = %d\n", ret);
				return ret;
			} else {
				smb349_chg->is_bb_work_case	= 1;
				pr_info("Applied modified boostback workaround using discharging\n");
			}
	} else {
		pr_info("NOT Applied modified boostback workaround using discharging\n");
	}
#endif

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

#if SMB349_BOOSTBACK_WORKAROUND
	/* USB uses HC mode for boostback workaround */
	if (smb349_chg->is_bb_work_case) {
		if (usb3) { /* usb 3.0 */
			ret = smb349_masked_write(smb349_chg->client, CHG_CURRENT_REG,
								0x0F, 0x01); /* 900mA */
			if (ret) {
				pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);
				return ret;
			}
		} else { /* usb 2.0 */
			ret = smb349_masked_write(smb349_chg->client, CHG_CURRENT_REG,
								0x0F, 0x00); /* 500mA */
			if (ret) {
				pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);
				return ret;
			}
		}
	}
#endif

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

#if defined(CONFIG_MACH_MSM8974_G2_ATT) || defined(CONFIG_MACH_MSM8974_G2_SPR) || defined(CONFIG_MACH_MSM8974_G2_VZW) || defined(CONFIG_MACH_MSM8974_G2_TMO_US) || defined(CONFIG_MACH_MSM8974_G2_TEL_AU) || defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_CA)
#define HC_INPUT_CURR_LIMIT_DEFAULT 2000
#else
#define HC_INPUT_CURR_LIMIT_DEFAULT 3000
#endif
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

#ifdef CONFIG_VZW_LLK
int32_t vzw_llk_smb349_enable_charging(bool enable)
{
	int ret ;
	struct smb349_struct *smb349_chg = the_smb349_chg;

	pr_debug("enable=%d.\n", enable);

	ret = smb349_enable_charging(the_smb349_chg, enable);

	if (ret) {
		pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);
		return ret;
	}

	if (!enable && !(chg_state == VZW_LLK_NOT_CHARGING)) {
		temp_state = chg_state;
		chg_state = VZW_LLK_NOT_CHARGING;
		pr_info("CHG_STATE : %d\n", chg_state);
	} else if ((chg_state == VZW_LLK_NOT_CHARGING) && enable){
		chg_state = temp_state;
		pr_info("CHG_STATE : %d\n", chg_state);
	}

	power_supply_changed(&smb349_chg->batt_psy);

	return 0;
}
#endif

#ifdef CONFIG_SMB349_VZW_FAST_CHG
extern int lge_usb_config_finish;
extern void send_drv_state_uevent(int usb_drv_state);

void set_vzw_usb_charging_state(int state)
{
	struct smb349_struct *smb349_chg = the_smb349_chg;

	usb_chg_state = state;

	if(!slimport_is_connected()) {
		if (usb_chg_state == IS_OPEN_TA) {
			chg_state = VZW_NOT_CHARGING;
			pr_info("%s : OPEN TA is connected!!", __func__);
		} else if (usb_chg_state == IS_USB_DRIVER_UNINSTALLED) {
			smb349_enable_charging(smb349_chg, false);
			send_drv_state_uevent(0);
			chg_state = VZW_USB_DRIVER_UNINSTALLED;
			pr_info("[USB_DRV] USB DRIVER UNINSTALLED !!\n");
		}
		power_supply_changed(&smb349_chg->batt_psy);
	}
}
EXPORT_SYMBOL(set_vzw_usb_charging_state);

static void vzw_fast_chg_change_usb_charging_state(struct smb349_struct *smb349_chg)
{
	chg_state = VZW_NO_CHARGER;

	if(!slimport_is_connected()) {
		if(lge_usb_config_finish == 0) {
			smb349_enable_charging(smb349_chg, false);
			pr_info("USB cable is connected, but USB is not configured!\n");
		} else if (usb_chg_state == IS_USB_DRIVER_INSTALLED) {
			smb349_set_usb_5_1_mode(smb349_chg, 1);
			smb349_enable_charging(smb349_chg, true);
			pr_info("USB is configured and USB Driver is installed!\n");
#if SMB349_BOOSTBACK_WORKAROUND
			if (smb349_chg->is_bb_work_case)
				smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
			else
				smb349_usb_hc_mode(smb349_chg, 0);
#else
			smb349_usb_hc_mode(smb349_chg, 0);
#endif
		}
	} else {
#if SMB349_BOOSTBACK_WORKAROUND
		if (smb349_chg->is_bb_work_case)
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
		else
			smb349_usb_hc_mode(smb349_chg, 0);
#else
		smb349_usb_hc_mode(smb349_chg, 0);
#endif
	}
}

static void vzw_fast_chg_set_charging(struct smb349_struct *smb349_chg)
{
	/*Changed SMB349 Charger setting,
	 if unknown incompatible charger is detected. */
	if (vzw_chg_present == NOT_PRESENT) {
		smb349_set_usb_5_1_mode(smb349_chg, 1);
		if (!vzw_fast_chg_detect_incompatible_charger(smb349_chg->client)) {
			/* Stop charing, if incompatible TA is detected */
			smb349_usb_hc_mode(smb349_chg, 0);
			smb349_set_usb_5_1_mode(smb349_chg, 0);
			smb349_set_usb_2_3_mode(smb349_chg, true);
			smb349_enable_charging(smb349_chg, false);
			vzw_chg_present = INCOMPAT_PRESENT;
			pr_info("incompatible TA, stop charging.\n");
		} else {
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
			vzw_chg_present = UNKNOWN_PRESENT;
		}
	} else if (vzw_chg_present == INCOMPAT_PRESENT){
		chg_state = VZW_NOT_CHARGING;
		pr_info("chg_state 1 = %d\n", chg_state);
#if SMB349_BOOSTBACK_WORKAROUND
	} else if ((vzw_chg_present == UNKNOWN_PRESENT) && count_no_aicl == 2) {
#else
	} else if (vzw_chg_present == UNKNOWN_PRESENT) {
#endif
		smb349_usb_hc_mode(smb349_chg, 0);
		smb349_set_usb_5_1_mode(smb349_chg, 1);
		smb349_set_usb_2_3_mode(smb349_chg, false);
		smb349_enable_charging(smb349_chg, true);
		chg_state = VZW_UNDER_CURRENT_CHARGING;
		pr_info("chg_state 2 = %d\n", chg_state);
	} else {
		smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
#if SMB349_BOOSTBACK_WORKAROUND
		if (count_no_aicl > 2)
			count_no_aicl = 0;
#endif
		pr_info("chg_state 3 = %d\n", chg_state);
	}
}
#endif

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
			smb349_set_usb_2_3_mode(smb349_chg, false);
#if SMB349_BOOSTBACK_WORKAROUND
			if (smb349_chg->is_bb_work_case)
				smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
			else
				smb349_usb_hc_mode(smb349_chg, 0);
#else
			smb349_usb_hc_mode(smb349_chg, 0);
#endif
		} else {
			pr_info("Factory cable detected(not 130k), HC mode set %d\n",
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_input_current_limit_set(smb349_chg,
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_set_pre_chg_to_fast_chg_thresh(smb349_chg,
				PRE_CHG_TO_FAST_CHG_THRESH_BIT);
			smb349_set_fast_charge(smb349_chg, 0);
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
#ifdef CONFIG_LGE_G2_EMBEDDED_BATT
			smb349_enable_charging(smb349_chg, false);
#endif
#ifndef CONFIG_LGE_PM
			smb349_chg_usb_suspend_enable(smb349_chg, 0);
#endif
		}
	} else if (ret.intval &&
			smb349_is_charger_present(smb349_chg->client)) {
		if (!smb349_chg->is_phy_forced_on)
			smb349_pmic_usb_override_wrap(smb349_chg, true);

		smb349_set_usb_2_3_mode(smb349_chg, false);
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		vzw_chg_present = USB_PRESENT;
		vzw_fast_chg_change_usb_charging_state(smb349_chg);
		pr_info("VZW_CHG_PRESENT = USB_PRESENT\n");
#else
#if SMB349_BOOSTBACK_WORKAROUND
		if (smb349_chg->is_bb_work_case)
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
		else
			smb349_usb_hc_mode(smb349_chg, 0);
#else
		smb349_usb_hc_mode(smb349_chg, 0);
#endif
#endif
#ifdef CONFIG_VZW_LLK
		if (get_prop_batt_capacity_max17048(smb349_chg) > 34) {
			smb349_enable_charging(smb349_chg, false);
			temp_state = chg_state;
			chg_state = VZW_LLK_NOT_CHARGING;
			pr_info("VZW LLK Charging Stop!!, CHG_STATE : %d\n", chg_state);
		}
#endif
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
		vzw_fast_chg_set_charging(smb349_chg);
#else
		smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
		/* ToDo : Must implements & verify. */
#endif
		if (smb349_chg->is_phy_forced_on)
			smb349_pmic_usb_override_wrap(smb349_chg, false);

#ifdef CONFIG_VZW_LLK
		if (((chg_state == 1) || (chg_state == 3)) &&
				(get_prop_batt_capacity_max17048(smb349_chg) > 34)) {
			smb349_enable_charging(smb349_chg, false);
			temp_state = chg_state;
			chg_state = VZW_LLK_NOT_CHARGING;
			pr_info("VZW LLK Charging Stop!!, CHG_STATE : %d\n", chg_state);
		}
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
		/*Prepare Wireless Charging */
		if(is_wireless_charger_plugged()){
			smb349_input_current_limit_set(smb349_chg, 900);
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
			pr_err("[WLC] Set inuput limit HC mode\n");

			smb349_set_pre_chg_current(smb349_chg, 500);
			smb349_set_pre_chg_to_fast_chg_thresh(smb349_chg,
				PRE_CHG_TO_FAST_CHG_THRESH_BIT);
			smb349_set_fast_charge(smb349_chg, 0);
			pr_err("[WLC] Set current HC mode and 500mA : %d\n"
					,smb349_chg->wlc_present);
		}else{
			pr_err("[WLC] GPIO SWING\n");
		}

		if (smb349_chg->is_phy_forced_on)
			smb349_pmic_usb_override_wrap(smb349_chg, false);
#endif
#endif
	} else {
		smb349_chg->chg_timeout = false;
#ifdef CONFIG_SMB349_VZW_FAST_CHG
		smb349_set_usb_5_1_mode(smb349_chg, 0);
		if(!smb349_is_charger_present(smb349_chg->client)){
			lge_usb_config_finish = 0;
			usb_chg_state = IS_USB_DRIVER_UNINSTALLED;
#if SMB349_BOOSTBACK_WORKAROUND
			count_no_aicl = 0;
#endif
		}
		chg_state = VZW_NO_CHARGER;
		vzw_chg_present = NOT_PRESENT;
#ifdef CONFIG_VZW_LLK
		temp_state = 0;
#endif
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
#if SMB349_BOOSTBACK_WORKAROUND
		if (!smb349_chg->global_is_bb_worker_eoc) {
			smb349_enable_charging(smb349_chg, true);
		}
#else
		smb349_enable_charging(smb349_chg, true);
#endif
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
#ifdef CONFIG_BQ51051B_CHARGER
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
		if (pseudo_batt_info.mode) {
			val->intval = pseudo_batt_info.capacity;
			break;
		}
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
#ifdef CONFIG_BQ51053B_CHARGER
		val->intval = smb349_chg->ac_online | smb349_chg->usb_online
				| smb349_chg->wlc_present;
#else
		val->intval = smb349_chg->ac_online | smb349_chg->usb_online
				| wireless_charging;
#endif
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
		val->intval = pseudo_batt_info.mode;
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

/*W/R function to alternate smb349_irq_worker n the specific case of WLC insersion or removal
  *case1. WLC PAD is detected to USB until WLC driver probe is finished completely
  *case2. SMB IRQ does not happen sometimes when USB is inserted on WLC PAD  */
#ifdef CONFIG_BQ51053B_CHARGER
void set_usb_present(int value)
{
	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return;
	}

	the_smb349_chg->wlc_present = !value;
	the_smb349_chg->usb_present = value;
	wake_lock_timeout(&the_smb349_chg->uevent_wake_lock, HZ*2);
	power_supply_set_present(the_smb349_chg->usb_psy,value);
	return;
}
EXPORT_SYMBOL(set_usb_present);

int get_usb_present(void)
{
	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return 0;
	}

	return the_smb349_chg->usb_present;
}
EXPORT_SYMBOL(get_usb_present);

bool external_smb349_is_charger_present(void)
{
	u8 irq_status_e;
	bool power_ok;

	if (!the_smb349_chg) {
		pr_err("called before init\n");
		return 0;
	}

	mutex_lock(&the_smb349_chg->lock);
	irq_status_e = the_smb349_chg->irqstat[IRQSTAT_E];
	mutex_unlock(&the_smb349_chg->lock);

	/* insert case is 0, remove case is 1 */
	power_ok = !(irq_status_e & 0x01);

	if (power_ok)
		pr_err("DC is present.\n");
	else
		pr_err("DC is missing.\n");

	return power_ok;

}
EXPORT_SYMBOL(external_smb349_is_charger_present);
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

int pseudo_batt_set(struct pseudo_batt_info_type *info)
{
	struct smb349_struct *smb349_chg = the_smb349_chg;
	pr_err("pseudo_batt_set\n");
	pseudo_batt_info.mode = info->mode;
	pseudo_batt_info.id = info->id;
	pseudo_batt_info.therm = info->therm;
	pseudo_batt_info.temp = info->temp;
	pseudo_batt_info.volt = info->volt;
	pseudo_batt_info.capacity = info->capacity;
	pseudo_batt_info.charging = info->charging;

	power_supply_changed(&smb349_chg->batt_psy);

	return 0;
}
EXPORT_SYMBOL(pseudo_batt_set);

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

#define SMB349_STATUS_DEBUG 1
#if SMB349_STATUS_DEBUG
static void smb349_status_print(struct smb349_struct *smb349_chg)
{
	struct i2c_client *client = smb349_chg->client;
	int rc;
	enum smb349_chg_status status;
	char chg_status, chg_timeout;
	u8 val_3d, val_38, val_3b, val_35, val_36, val_37, val_31;

	union power_supply_propval ret = {0,};

	/* 3Dh */
	rc = smb349_read_reg(client, STATUS_C_REG, &val_3d);
	if (rc) {
		pr_err("Failed to read STATUS_C_REG rc=%d\n", rc);
		return;
	}
	/* 3Bh */
	rc = smb349_read_reg(client, STATUS_A_REG, &val_3b);
	if (rc) {
		pr_err("Failed to read STATUS_A_REG rc=%d\n", rc);
		return;
	}

	/* 35h(irq status A) ~ 38h(irq status D) read irq status */
	mutex_lock(&smb349_chg->lock);
	val_35 = smb349_chg->irqstat[IRQSTAT_A];
	val_36 = smb349_chg->irqstat[IRQSTAT_B];
	val_37 = smb349_chg->irqstat[IRQSTAT_C];
	val_38 = smb349_chg->irqstat[IRQSTAT_D];
	mutex_unlock(&smb349_chg->lock);

	/* 31h */
	rc = smb349_read_reg(client, CMD_B_REG, &val_31);
	if (rc) {
		pr_err("Failed to read CMD_B_REG rc=%d\n", rc);
		return;
	}
	status = (val_3d >> 1) & 0x3;
	switch (status) {
		case SMB_CHG_STATUS_NONE:
			chg_status = 'N';
			break;
		case SMB_CHG_STATUS_PRE_CHARGE:
			chg_status = 'P';
			break;
		case SMB_CHG_STATUS_FAST_CHARGE:
			chg_status = 'F';
			break;
		case SMB_CHG_STATUS_TAPER_CHARGE:
			chg_status = 'T';
			break;
		default:
			chg_status = 'E';
	}

	if ( (val_38 & BIT(2)) && (val_38 & BIT(0)) )
		chg_timeout = 'B';
	else if ( val_38 & BIT(2) )
		chg_timeout = 'P';
	else if ( val_38 & BIT(0) )
		chg_timeout = 'C';
	else
		chg_timeout = 'N';

	smb349_chg->batt_psy.get_property(&(smb349_chg->batt_psy),
			  POWER_SUPPLY_PROP_CHARGE_TYPE, &ret);

#if 0
	printk(KERN_ERR "[chglog]EN:%d ERR:%d STAT:%c M:%c U:%d EOC:%d RE:%d BL:%c BO:%d BM:%d HOFF:%d TO:%c SYS:%d IT:%d TEMP:0x%02X PSY:[PRE:%d,ON:%d-%d,TYP:%d]\n",
			val_3d & BIT(0)? 1: 0,		/* EN:charging enable */
			val_3d & BIT(6)? 1: 0,		/* ERR:charging error */
			chg_status,			/* STAT:charging status */
			val_31 & BIT(0)? 'H':'U',	/* M:USB,HC mode */
			val_31 & BIT(2)? 3: 2,		/* U:USB2,3 selection */
			val_3d & BIT(5)? 1: 0,		/* EOC: At least one cycle has terminated */
			val_37 & BIT(4)? 1: 0,		/* RE: Re-charge battery thresh status */
			val_3d & BIT(4)? 'L':'H',	/* BL: battery voltage level L:Vbat<2.1V,H:Vbat>2.1V */
			val_36 & BIT(6)? 1: 0,		/* BO: Battery overvoltage status */
			val_36 & BIT(4)? 1: 0,		/* BM: Battery missing status */
			val_3d & BIT(3)? 1: 0,		/* HOFF: charger hold-off status */
			chg_timeout,			/* TO: timeout status P:pre-chg, C:completion, B:P and C, N:not occur */
			val_3b & BIT(7)? 1: 0,		/* SYS: system current more than input current */
			val_37 & BIT(6)? 1: 0,		/* IT: internal temp limit 130C */
			val_35,				/* TEMP: temperature interrupt register 35h dump */
			smb349_chg->usb_present,	/* PRE: usb_present */
			smb349_chg->usb_online, smb349_chg->ac_online, /* ON: power_supply online usb_online-ac_online */
			ret.intval			/* TYP: power_supply charger type*/
		);
#endif
}
#endif

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
static int temp_before = 0;
static void smb349_monitor_batt_temp(struct work_struct *work)
{
	struct smb349_struct *smb349_chg =
		container_of(work, struct smb349_struct, battemp_work.work);
	struct charging_info req;
	struct charging_rsp res;
	bool is_changed = false;
	union power_supply_propval ret = {0,};

	if (smb349_chg->chg_timeout) {
		int ret;

		if (pseudo_batt_info.mode) {
			pr_err("Fake batt escape charging timeout, charging enable and unlocked\n");

			smb349_chg->chg_timeout = false;

			ret = smb349_enable_charging(smb349_chg, true);
			if (ret)
				pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);

			wake_unlock(&smb349_chg->chg_timeout_lock);
		} else {
#if !defined(CONFIG_MACH_MSM8974_G2_VZW) && !defined(CONFIG_MACH_MSM8974_G2_SPR) \
	&& !defined(CONFIG_MACH_MSM8974_G2_TMO_US) && !defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) \
	&& !defined(CONFIG_MACH_MSM8974_G2_ATT) && !defined(CONFIG_MACH_MSM8974_G2_CA)
			pr_err("escape charging timeout, charging enable and unlocked\n");

			smb349_chg->chg_timeout = false;

			ret = smb349_enable_charging(smb349_chg, true);
			if (ret)
				pr_err("Failed to set CHG_ENABLE_BIT rc=%d\n", ret);

			wake_unlock(&smb349_chg->chg_timeout_lock);
#endif
		}
	}

	smb349_chg->batt_psy.get_property(&(smb349_chg->batt_psy),
			  POWER_SUPPLY_PROP_TEMP, &ret);
	req.batt_temp = ret.intval / 10;

	smb349_chg->batt_psy.get_property(&(smb349_chg->batt_psy),
			  POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
	req.batt_volt = ret.intval;

	smb349_chg->batt_psy.get_property(&(smb349_chg->batt_psy),
			  POWER_SUPPLY_PROP_CURRENT_NOW, &ret);
	req.current_now = ret.intval / 1000;

#ifdef CONFIG_LGE_THERMALE_CHG_CONTROL
	req.chg_current_ma = smb349_chg->chg_current_ma;
	req.chg_current_te = smb349_chg->chg_current_te;
#endif

	req.is_charger = smb349_is_charger_present(smb349_chg->client);

	lge_monitor_batt_temp(req, &res);

#if SMB349_STATUS_DEBUG
	smb349_status_print(smb349_chg);
#endif

	if (((res.change_lvl != STS_CHE_NONE) && req.is_charger) ||
		(res.force_update == true)) {
		is_changed = true;

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

	if (smb349_chg->btm_state ^ res.btm_state) {
		is_changed = true;
		smb349_chg->btm_state = res.btm_state;
	}

	if (temp_before != req.batt_temp) {
		is_changed = true;
		temp_before = req.batt_temp;
	}

#if defined(CONFIG_WIRELESS_CHARGER) && defined(CONFIG_BQ51051B_CHARGER)
	if (wireless_charging)
		wireless_current_ctl(req.batt_temp);
#endif

	if(is_changed == true)
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

int lge_get_sbl_cable_type(void)
{
	int ret_cable_type = 0;
	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size));

	if (p_cable_type)
		ret_cable_type = *p_cable_type;
	else
		ret_cable_type = 0;

	return ret_cable_type;
}
EXPORT_SYMBOL(lge_get_sbl_cable_type);

static int __devinit smb349_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	const struct smb349_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	struct smb349_struct *smb349_chg;
	int ret = 0;
	u8 temp = 0;

	/* STAT pin change on start/stop charging */
	u32 irq_flags = IRQF_TRIGGER_FALLING;

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

	mutex_init(&smb349_chg->lock);
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

	/* checking chip version information */
	smb349_version_check(smb349_chg);

	/* initialize irqstat */
	smb349_irqstat_init(smb349_chg);

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

	wake_lock_init(&smb349_chg->chg_timeout_lock,
		       WAKE_LOCK_SUSPEND, "chg timeout");

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

	smb349_chg->batt_removed.name = "battery_removed";
	smb349_chg->batt_removed.state = 0; /*if batt is removed, state will be set to 1 */
	smb349_chg->batt_removed.print_name = batt_removed_print_name;
	smb349_chg->batt_removed.print_state = batt_removed_print_state;

	ret = switch_dev_register(&smb349_chg->batt_removed);
	if (ret < 0) {
		pr_err("Failed to register switch device, battery_removed\n");
	}
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
			smb349_chg->chg_current_ma = FACTORY_LOWEST_CHG_CURRENT_130K;
			smb349_set_usb_2_3_mode(smb349_chg, false);
#if SMB349_BOOSTBACK_WORKAROUND
			if (smb349_chg->is_bb_work_case)
				smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
			else
				smb349_usb_hc_mode(smb349_chg, 0);
#else
			smb349_usb_hc_mode(smb349_chg, 0);
#endif
		} else {
			pr_info("Factory cable detected(not 130k), HC mode set %d\n",
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_chg->chg_current_ma = FACTORY_LOWEST_CHG_CURRENT_OTHERS;
			smb349_input_current_limit_set(smb349_chg,
				HC_INPUT_CURR_LIMIT_FACTORY);
			smb349_set_fast_charge(smb349_chg, 0);
			smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
#ifdef CONFIG_LGE_G2_EMBEDDED_BATT
			smb349_enable_charging(smb349_chg, false);
#endif
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
#if SMB349_BOOSTBACK_WORKAROUND
	INIT_DELAYED_WORK(&smb349_chg->bb_work, smb349_bb_worker);
	INIT_DELAYED_WORK(&smb349_chg->bb_rechg_work, smb349_bb_rechg_worker);
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
		smb349_is_charger_present_rt(smb349_chg->client));

	smb349_enable_charging(smb349_chg, true);

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

err_at_pmrst:
	device_remove_file(&client->dev, &dev_attr_at_chcomp);
err_at_chcomp:
	device_remove_file(&client->dev, &dev_attr_at_charge);
err_at_charge:
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

	wake_lock_destroy(&smb349_chg->chg_timeout_lock);

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
	device_remove_file(&client->dev, &dev_attr_at_charge);
	device_remove_file(&client->dev, &dev_attr_at_chcomp);
	device_remove_file(&client->dev, &dev_attr_at_pmrst);

	wake_lock_destroy(&smb349_chg->chg_wake_lock);
	wake_lock_destroy(&smb349_chg->uevent_wake_lock);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_destroy(&smb349_chg->lcs_wake_lock);
#endif
#if defined(CONFIG_MACH_MSM8974_G2_KR) || defined(CONFIG_MACH_MSM8974_VU3_KR)
	wake_lock_destroy(&smb349_chg->battgone_wake_lock);
	qpnp_batif_unregist_batt_present(0);
#endif

	wake_lock_destroy(&smb349_chg->chg_timeout_lock);

	kfree(smb349_chg);
	smb349_chg = NULL;
	return 0;
}

#define SUSPEND_MODE_BIT BIT(2)
#define SUSPEND_MODE_SHIFT 2
static int smb349_suspend_enable(struct smb349_struct *smb349_chg, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << SUSPEND_MODE_SHIFT); /* active high */

	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
					SUSPEND_MODE_BIT, val);

	if (ret) {
		pr_err("Failed to set suspend_enable val=0x%02X, rc=%d\n", val, ret);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_LGE_PM
static void smb349_shutdown(struct i2c_client *client)
{
	struct smb349_struct *smb349_chg = i2c_get_clientdata(client);

	if (!smb349_chg) {
		pr_err("%s: smb349_chg is NULL\n", __func__);
		return;
	}

	if (smb349_chg_is_otg_active(smb349_chg)) {
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
	}

	if (smb349_pmic_batt_present()) {
	/* sometimes failed to boot after reseting the phone
	 * when using low voltage adapter */
		smb349_pmic_usb_override_wrap(smb349_chg, false);

		smb349_suspend_enable(smb349_chg, true);
		mdelay(100);
		smb349_suspend_enable(smb349_chg, false);
	}
}
#endif

#ifdef CONFIG_LGE_PM
static int smb349_suspend(struct device *dev)
{
	struct smb349_struct *smb349_chg = dev_get_drvdata(dev);

	if (!smb349_chg) {
		pr_err("called before init\n");
		return 0;
	}
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	cancel_delayed_work_sync(&smb349_chg->battemp_work);
#endif

#if SMB349_BOOSTBACK_WORKAROUND
	if (smb349_chg->is_rechg_work_trigger) {
		cancel_delayed_work_sync(&smb349_chg->bb_rechg_work);
	}
#endif

	return 0;
}

static int smb349_resume(struct device *dev)
{
	struct smb349_struct *smb349_chg = dev_get_drvdata(dev);

	if (!smb349_chg) {
		pr_err("called before init\n");
		return 0;
	}

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	schedule_delayed_work(&smb349_chg->battemp_work, HZ*10);
#endif

#if SMB349_BOOSTBACK_WORKAROUND
	if (smb349_chg->is_rechg_work_trigger) {
		schedule_delayed_work(&smb349_chg->bb_rechg_work, 0);
	}
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
		   .pm		= &smb349_pm_ops,
#endif
	},
	.probe		= smb349_probe,
#ifdef CONFIG_LGE_PM
	.shutdown	= smb349_shutdown,
#endif
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
