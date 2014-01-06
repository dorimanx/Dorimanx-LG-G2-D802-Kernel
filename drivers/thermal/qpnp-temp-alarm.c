/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <linux/qpnp/qpnp-adc.h>

#if defined(CONFIG_MACH_MSM8974_G2_KR)
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#endif

#define QPNP_TM_DRIVER_NAME "qcom,qpnp-temp-alarm"

enum qpnp_tm_registers {
	QPNP_TM_REG_TYPE		= 0x04,
	QPNP_TM_REG_SUBTYPE		= 0x05,
	QPNP_TM_REG_STATUS		= 0x08,
	QPNP_TM_REG_SHUTDOWN_CTRL1	= 0x40,
	QPNP_TM_REG_SHUTDOWN_CTRL2	= 0x42,
	QPNP_TM_REG_ALARM_CTRL		= 0x46,
};

#define QPNP_TM_TYPE			0x09
#define QPNP_TM_SUBTYPE			0x08

#define STATUS_STAGE_MASK		0x03

#define SHUTDOWN_CTRL1_OVERRIDE_STAGE3	0x80
#define SHUTDOWN_CTRL1_OVERRIDE_STAGE2	0x40
#define SHUTDOWN_CTRL1_THRESHOLD_MASK	0x03

#define SHUTDOWN_CTRL2_CLEAR_STAGE3	0x80
#define SHUTDOWN_CTRL2_CLEAR_STAGE2	0x40

#define ALARM_CTRL_FORCE_ENABLE		0x80
#define ALARM_CTRL_FOLLOW_HW_ENABLE	0x01

#define TEMP_STAGE_STEP			20000	/* Stage step: 20.000 C */
#define TEMP_STAGE_HYSTERESIS		2000

#define TEMP_THRESH_MIN			105000	/* Threshold Min: 105 C */
#define TEMP_THRESH_STEP		5000	/* Threshold step: 5 C */

#define THRESH_MIN			0
#define THRESH_MAX			3

/* Trip points from most critical to least critical */
#define TRIP_STAGE3			0
#define TRIP_STAGE2			1
#define TRIP_STAGE1			2
#define TRIP_NUM			3

#if defined(CONFIG_MACH_MSM8974_G2_KR)
#define BATT_IF_PRES_RT_STATUS		BIT(0)
#define BATT_PRES_EN_BIT		BIT(0)
#endif

enum qpnp_tm_adc_type {
	QPNP_TM_ADC_NONE,	/* Estimates temp based on overload level. */
	QPNP_TM_ADC_QPNP_ADC,
};

/*
 * Temperature in millicelcius reported during stage 0 if no ADC is present and
 * no value has been specified via device tree.
 */
#define DEFAULT_NO_ADC_TEMP		37000

struct qpnp_tm_chip {
	struct delayed_work		irq_work;
	struct spmi_device		*spmi_dev;
	struct thermal_zone_device	*tz_dev;
	const char			*tm_name;
	enum qpnp_tm_adc_type		adc_type;
	unsigned long			temperature;
	enum thermal_device_mode	mode;
	unsigned int			thresh;
	unsigned int			stage;
	unsigned int			prev_stage;
	int				irq;
	enum qpnp_vadc_channels		adc_channel;
	u16				base_addr;
	bool				allow_software_override;
#if defined(CONFIG_MACH_MSM8974_G2_KR)
	u16				bat_if_base;
	unsigned int			batt_present_irq;
	struct work_struct		batt_insert_remove_worker;
	struct delayed_work		batt_insert_remove_worker_second;
	struct wake_lock		battgone_wake_lock;
#endif
};

/* Delay between TEMP_STAT IRQ going high and status value changing in ms. */
#define STATUS_REGISTER_DELAY_MS       40

enum pmic_thermal_override_mode {
	SOFTWARE_OVERRIDE_DISABLED = 0,
	SOFTWARE_OVERRIDE_ENABLED,
};

static inline int qpnp_tm_read(struct qpnp_tm_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = spmi_ext_register_readl(chip->spmi_dev->ctrl,
			chip->spmi_dev->sid, chip->base_addr + addr, buf, len);

	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: spmi_ext_register_readl() failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__, chip->spmi_dev->sid, chip->base_addr + addr,
			len, rc);

	return rc;
}

static inline int qpnp_tm_write(struct qpnp_tm_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl,
			chip->spmi_dev->sid, chip->base_addr + addr, buf, len);

	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: spmi_ext_register_writel() failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__, chip->spmi_dev->sid, chip->base_addr + addr,
			len, rc);

	return rc;
}


static inline int qpnp_tm_shutdown_override(struct qpnp_tm_chip *chip,
			    enum pmic_thermal_override_mode mode)
{
	int rc = 0;
	u8 reg;

	if (chip->allow_software_override) {
		reg = chip->thresh & SHUTDOWN_CTRL1_THRESHOLD_MASK;

		if (mode == SOFTWARE_OVERRIDE_ENABLED)
			reg |= SHUTDOWN_CTRL1_OVERRIDE_STAGE2
				| SHUTDOWN_CTRL1_OVERRIDE_STAGE3;

		rc = qpnp_tm_write(chip, QPNP_TM_REG_SHUTDOWN_CTRL1, &reg, 1);
	}

	return rc;
}

static int qpnp_tm_update_temp(struct qpnp_tm_chip *chip)
{
	struct qpnp_vadc_result adc_result;
	int rc;

	rc = qpnp_vadc_read(chip->adc_channel, &adc_result);
	if (!rc)
		chip->temperature = adc_result.physical;
	else
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_vadc_read(%d) failed, rc=%d\n",
			__func__, chip->adc_channel, rc);

	return rc;
}

/*
 * This function initializes the internal temperature value based on only the
 * current thermal stage and threshold.
 */
static int qpnp_tm_init_temp_no_adc(struct qpnp_tm_chip *chip)
{
	int rc;
	u8 reg;

	rc = qpnp_tm_read(chip, QPNP_TM_REG_STATUS, &reg, 1);
	if (rc < 0)
		return rc;

	chip->stage = reg & STATUS_STAGE_MASK;

	if (chip->stage)
		chip->temperature = chip->thresh * TEMP_THRESH_STEP +
			   (chip->stage - 1) * TEMP_STAGE_STEP +
			   TEMP_THRESH_MIN;

	return 0;
}

/*
 * This function updates the internal temperature value based on the
 * current thermal stage and threshold as well as the previous stage
 */
static int qpnp_tm_update_temp_no_adc(struct qpnp_tm_chip *chip)
{
	unsigned int stage;
	int rc;
	u8 reg;

	rc = qpnp_tm_read(chip, QPNP_TM_REG_STATUS, &reg, 1);
	if (rc < 0)
		return rc;

	stage = reg & STATUS_STAGE_MASK;

	if (stage > chip->stage) {
		/* increasing stage, use lower bound */
		chip->temperature = (stage - 1) * TEMP_STAGE_STEP
				+ chip->thresh * TEMP_THRESH_STEP
				+ TEMP_STAGE_HYSTERESIS + TEMP_THRESH_MIN;
	} else if (stage < chip->stage) {
		/* decreasing stage, use upper bound */
		chip->temperature = stage * TEMP_STAGE_STEP
				+ chip->thresh * TEMP_THRESH_STEP
				- TEMP_STAGE_HYSTERESIS + TEMP_THRESH_MIN;
	}

	chip->stage = stage;

	return 0;
}

static int qpnp_tz_get_temp_no_adc(struct thermal_zone_device *thermal,
				     unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int rc;

	if (!temperature)
		return -EINVAL;

	rc = qpnp_tm_update_temp_no_adc(chip);
	if (rc < 0)
		return rc;

	*temperature = chip->temperature;

	return 0;
}

static int qpnp_tz_get_temp_qpnp_adc(struct thermal_zone_device *thermal,
				      unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int rc;

	if (!temperature)
		return -EINVAL;

	rc = qpnp_tm_update_temp(chip);
	if (rc < 0) {
		dev_err(&chip->spmi_dev->dev, "%s: %s: adc read failed, rc = %d\n",
			__func__, chip->tm_name, rc);
		return rc;
	}

	*temperature = chip->temperature;

	return 0;
}

static int qpnp_tz_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct qpnp_tm_chip *chip = thermal->devdata;

	if (!mode)
		return -EINVAL;

	*mode = chip->mode;

	return 0;
}

static int qpnp_tz_set_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode mode)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int rc = 0;

	if (mode != chip->mode) {
		if (mode == THERMAL_DEVICE_ENABLED)
			rc = qpnp_tm_shutdown_override(chip,
				SOFTWARE_OVERRIDE_ENABLED);
		else
			rc = qpnp_tm_shutdown_override(chip,
				SOFTWARE_OVERRIDE_DISABLED);

		chip->mode = mode;
	}

	return rc;
}

static int qpnp_tz_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	if (trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case TRIP_STAGE3:
		*type = THERMAL_TRIP_CRITICAL;
		break;
	case TRIP_STAGE2:
		*type = THERMAL_TRIP_HOT;
		break;
	case TRIP_STAGE1:
		*type = THERMAL_TRIP_HOT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qpnp_tz_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int thresh_temperature;

	if (trip < 0 || !temperature)
		return -EINVAL;

	thresh_temperature = chip->thresh * TEMP_THRESH_STEP + TEMP_THRESH_MIN;

	switch (trip) {
	case TRIP_STAGE3:
		thresh_temperature += 2 * TEMP_STAGE_STEP;
		break;
	case TRIP_STAGE2:
		thresh_temperature += TEMP_STAGE_STEP;
		break;
	case TRIP_STAGE1:
		break;
	default:
		return -EINVAL;
	}

	*temperature = thresh_temperature;

	return 0;
}

static int qpnp_tz_get_crit_temp(struct thermal_zone_device *thermal,
				   unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;

	if (!temperature)
		return -EINVAL;

	*temperature = chip->thresh * TEMP_THRESH_STEP + TEMP_THRESH_MIN +
		2 * TEMP_STAGE_STEP;

	return 0;
}

static struct thermal_zone_device_ops qpnp_thermal_zone_ops_no_adc = {
	.get_temp = qpnp_tz_get_temp_no_adc,
	.get_mode = qpnp_tz_get_mode,
	.set_mode = qpnp_tz_set_mode,
	.get_trip_type = qpnp_tz_get_trip_type,
	.get_trip_temp = qpnp_tz_get_trip_temp,
	.get_crit_temp = qpnp_tz_get_crit_temp,
};

static struct thermal_zone_device_ops qpnp_thermal_zone_ops_qpnp_adc = {
	.get_temp = qpnp_tz_get_temp_qpnp_adc,
	.get_mode = qpnp_tz_get_mode,
	.set_mode = qpnp_tz_set_mode,
	.get_trip_type = qpnp_tz_get_trip_type,
	.get_trip_temp = qpnp_tz_get_trip_temp,
	.get_crit_temp = qpnp_tz_get_crit_temp,
};

static void qpnp_tm_work(struct work_struct *work)
{
	struct delayed_work *dwork
		= container_of(work, struct delayed_work, work);
	struct qpnp_tm_chip *chip
		= container_of(dwork, struct qpnp_tm_chip, irq_work);
	int rc;
	u8 reg;

	if (chip->adc_type == QPNP_TM_ADC_NONE) {
		rc = qpnp_tm_update_temp_no_adc(chip);
		if (rc < 0)
			goto bail;
	} else {
		rc = qpnp_tm_read(chip, QPNP_TM_REG_STATUS, &reg, 1);
		if (rc < 0)
			goto bail;

		chip->stage = reg & STATUS_STAGE_MASK;

		rc = qpnp_tm_update_temp(chip);
		if (rc < 0)
			goto bail;
	}

	if (chip->stage != chip->prev_stage) {
		chip->prev_stage = chip->stage;

		pr_crit("%s: PMIC Temp Alarm - stage=%u, threshold=%u, temperature=%lu mC\n",
			chip->tm_name, chip->stage, chip->thresh,
			chip->temperature);

		thermal_zone_device_update(chip->tz_dev);

		/* Notify user space */
		sysfs_notify(&chip->tz_dev->device.kobj, NULL, "type");
	}

bail:
	return;
}

static irqreturn_t qpnp_tm_isr(int irq, void *data)
{
	struct qpnp_tm_chip *chip = data;

	schedule_delayed_work(&chip->irq_work,
			msecs_to_jiffies(STATUS_REGISTER_DELAY_MS) + 1);

	return IRQ_HANDLED;
}

static int qpnp_tm_init_reg(struct qpnp_tm_chip *chip)
{
	int rc = 0;
	u8 reg;

	if (chip->thresh < THRESH_MIN || chip->thresh > THRESH_MAX) {
		/* Read hardware threshold value if configuration is invalid. */
		rc = qpnp_tm_read(chip, QPNP_TM_REG_SHUTDOWN_CTRL1, &reg, 1);
		if (rc < 0)
			return rc;
		chip->thresh = reg & SHUTDOWN_CTRL1_THRESHOLD_MASK;
	}

	/*
	 * Set threshold and disable software override of stage 2 and 3
	 * shutdowns.
	 */
	reg = chip->thresh & SHUTDOWN_CTRL1_THRESHOLD_MASK;
	rc = qpnp_tm_write(chip, QPNP_TM_REG_SHUTDOWN_CTRL1, &reg, 1);
	if (rc < 0)
		return rc;

	/* Enable the thermal alarm PMIC module in always-on mode. */
	reg = ALARM_CTRL_FORCE_ENABLE;
	rc = qpnp_tm_write(chip, QPNP_TM_REG_ALARM_CTRL, &reg, 1);

	return rc;
}

#if defined(CONFIG_MACH_MSM8974_G2_KR)
#define INT_RT_STS(base)			(base + 0x10)
#define INT_EN_SET(base)			(base + 0x15)
#define SMBB_BAT_IF_SUBTYPE			0x03
#define REG_OFFSET_PERP_SUBTYPE		0x05

static int
qpnp_register_read(struct qpnp_tm_chip *chip, u8 *val,
			u16 base, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi_dev;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, base, val,
					count);
	if (rc) {
		pr_err("SPMI read failed base=0x%02x sid=0x%02x rc=%d\n", base,
				spmi->sid, rc);
		return rc;
	}
	return 0;
}

static int
qpnp_register_write(struct qpnp_tm_chip *chip, u8 *val,
			u16 base, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi_dev;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, base, val,
					count);
	if (rc) {
		pr_err("write failed base=0x%02x sid=0x%02x rc=%d\n",
			base, spmi->sid, rc);
		return rc;
	}

	return 0;
}
static int
qpnp_register_masked_write(struct qpnp_tm_chip *chip, u16 base,
						u8 mask, u8 val, int count)
{
	int rc;
	u8 reg;

	rc = qpnp_register_read(chip, &reg, base, count);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n", base, rc);
		return rc;
	}
	pr_debug("addr = 0x%x read 0x%x\n", base, reg);

	reg &= ~mask;
	reg |= val & mask;

	pr_debug("Writing 0x%x\n", reg);

	rc = qpnp_register_write(chip, &reg, base, count);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n", base, rc);
		return rc;
	}

	return 0;
}

static void (*notify_batt_pres_func_ptr)(int);

int qpnp_batif_regist_batt_present(void (*callback)(int))
{
	pr_err("%p\n", callback);
	notify_batt_pres_func_ptr = callback;
	return 0;
}
EXPORT_SYMBOL_GPL(qpnp_batif_regist_batt_present);

void qpnp_batif_unregist_batt_present(void (*callback)(int))
{
	pr_debug("%p\n", callback);
	notify_batt_pres_func_ptr = NULL;
}
EXPORT_SYMBOL_GPL(qpnp_batif_unregist_batt_present);

static void notify_batt_insert_remove_event(u8 batt_present)
{
	batt_present = !!batt_present;
	if (notify_batt_pres_func_ptr) {
		pr_debug("notifying batt_present\n");
		(*notify_batt_pres_func_ptr) ((int)batt_present);
	} else {
		pr_err("unable to notify batt_present\n");
	}
}

/* will be excute when first irq detected battery missing */
static void batt_insert_remove_work_second(struct work_struct *work)
{
	struct qpnp_tm_chip *chip = container_of(work,
			struct qpnp_tm_chip, batt_insert_remove_worker_second.work);
	int rc;
	u8 batt_pres_rt_sts;

	rc = qpnp_register_read(chip, &batt_pres_rt_sts,
				 INT_RT_STS(chip->bat_if_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->bat_if_base), rc);
		goto fail_or_end;
	}

	pr_err("%s(second) : rt sts = 0x%x / batt present -> 0x%x\n", __func__,
		batt_pres_rt_sts,
		(u8)(batt_pres_rt_sts & BATT_IF_PRES_RT_STATUS));

	batt_pres_rt_sts = batt_pres_rt_sts & BATT_IF_PRES_RT_STATUS;

	/* Enable battery insert / remove irq */
	rc = qpnp_register_masked_write(chip, INT_EN_SET(chip->bat_if_base),
				BATT_PRES_EN_BIT, BATT_PRES_EN_BIT, 1);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->bat_if_base), rc);
		goto fail_or_end;
	}

	notify_batt_insert_remove_event(batt_pres_rt_sts);

fail_or_end:
	wake_unlock(&chip->battgone_wake_lock);
}

static void batt_insert_remove_work(struct work_struct *work)
{
	struct qpnp_tm_chip *chip = container_of(work,
			struct qpnp_tm_chip, batt_insert_remove_worker);
	int rc;
	u8 batt_pres_rt_sts;

	/* wake lock enable for prevent AP suspend. */
	wake_lock(&chip->battgone_wake_lock);

	/* Disable battery insert / remove irq */
	rc = qpnp_register_masked_write(chip, (chip->bat_if_base + 0x16),
				BIT(0), 0x01, 1);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->bat_if_base), rc);
		goto fail_or_end;
	}

	rc = qpnp_register_read(chip, &batt_pres_rt_sts,
				 INT_RT_STS(chip->bat_if_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->bat_if_base), rc);
		goto fail_or_end;
	}

	pr_err("%s(first) : rt sts = 0x%x / batt present -> 0x%x\n", __func__,
		batt_pres_rt_sts,
		(u8)(batt_pres_rt_sts & BATT_IF_PRES_RT_STATUS));

	batt_pres_rt_sts = batt_pres_rt_sts & BATT_IF_PRES_RT_STATUS;

	if (batt_pres_rt_sts) {
		/* batt present, wake lock release */
		pr_err("%s(first) : battery remove/insert irq occured."\
			"but batt present, return\n", __func__);

		/* Enable battery insert / remove irq */
		rc = qpnp_register_masked_write(chip, INT_EN_SET(chip->bat_if_base),
					BATT_PRES_EN_BIT, BATT_PRES_EN_BIT, 1);
		if (rc) {
			pr_err("spmi write failed: addr=%03X, rc=%d\n",
					INT_RT_STS(chip->bat_if_base), rc);
			goto fail_or_end;
		}
	} else {
		/* batt missing work-around, re-check rt status after 3sec */
		schedule_delayed_work(&chip->batt_insert_remove_worker_second, HZ*3);
		return;
	}
fail_or_end:
	wake_unlock(&chip->battgone_wake_lock);
}

static irqreturn_t
qpnp_batif_batt_inserted_removed_irq_handler(int irq, void *_chip)
{
	struct qpnp_tm_chip *chip = _chip;
	pr_err("%s\n", __func__);
	schedule_work(&chip->batt_insert_remove_worker);
	return IRQ_HANDLED;
}
#endif

static int __devinit qpnp_tm_probe(struct spmi_device *spmi)
{
	struct device_node *node;
	struct resource *res;
	struct qpnp_tm_chip *chip;
	struct thermal_zone_device_ops *tz_ops;
	char *tm_name;
	u32 default_temperature;
	int rc = 0;
	u8 raw_type[2], type, subtype;
#if defined(CONFIG_MACH_MSM8974_G2_KR)
	struct spmi_resource *spmi_resource;
#endif
	if (!spmi || !(&spmi->dev) || !spmi->dev.of_node) {
		dev_err(&spmi->dev, "%s: device tree node not found\n",
			__func__);
		return -EINVAL;
	}

	node = spmi->dev.of_node;

	chip = kzalloc(sizeof(struct qpnp_tm_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&spmi->dev, "%s: Can't allocate qpnp_tm_chip\n",
			__func__);
		return -ENOMEM;
	}

	dev_set_drvdata(&spmi->dev, chip);

	res = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing base address\n",
			__func__);
		rc = -EINVAL;
		goto free_chip;
	}
	chip->base_addr	= res->start;
	chip->spmi_dev	= spmi;

	chip->irq = spmi_get_irq(spmi, NULL, 0);
	if (chip->irq < 0) {
		rc = chip->irq;
		dev_err(&spmi->dev, "%s: node is missing irq, rc=%d\n",
			__func__, rc);
		goto free_chip;
	}

	chip->tm_name = of_get_property(node, "label", NULL);
	if (chip->tm_name == NULL) {
		dev_err(&spmi->dev, "%s: node is missing label\n",
			__func__);
		rc = -EINVAL;
		goto free_chip;
	}

	tm_name = kstrdup(chip->tm_name, GFP_KERNEL);
	if (tm_name == NULL) {
		dev_err(&spmi->dev, "%s: could not allocate memory for label\n",
			__func__);
		rc = -ENOMEM;
		goto free_chip;
	}
	chip->tm_name = tm_name;

	INIT_DELAYED_WORK(&chip->irq_work, qpnp_tm_work);

	/* These bindings are optional, so it is okay if they are not found. */
	chip->thresh = THRESH_MAX + 1;
	rc = of_property_read_u32(node, "qcom,threshold-set", &chip->thresh);
	if (!rc && (chip->thresh < THRESH_MIN || chip->thresh > THRESH_MAX))
		dev_err(&spmi->dev, "%s: invalid qcom,threshold-set=%u specified\n",
			__func__, chip->thresh);

	chip->adc_type = QPNP_TM_ADC_NONE;
	rc = of_property_read_u32(node, "qcom,channel-num", &chip->adc_channel);
	if (!rc) {
		if (chip->adc_channel < 0 || chip->adc_channel >= ADC_MAX_NUM) {
			dev_err(&spmi->dev, "%s: invalid qcom,channel-num=%d specified\n",
				__func__, chip->adc_channel);
		} else {
			chip->adc_type = QPNP_TM_ADC_QPNP_ADC;
			rc = qpnp_vadc_is_ready();
			if (rc) {
				/* Probe retry, do not print an error message */
				goto err_cancel_work;
			}
		}
	}

	if (chip->adc_type == QPNP_TM_ADC_QPNP_ADC)
		tz_ops = &qpnp_thermal_zone_ops_qpnp_adc;
	else
		tz_ops = &qpnp_thermal_zone_ops_no_adc;

	chip->allow_software_override
		= of_property_read_bool(node, "qcom,allow-override");

	default_temperature = DEFAULT_NO_ADC_TEMP;
	rc = of_property_read_u32(node, "qcom,default-temp",
					&default_temperature);
	chip->temperature = default_temperature;

	rc = qpnp_tm_read(chip, QPNP_TM_REG_TYPE, raw_type, 2);
	if (rc) {
		dev_err(&spmi->dev, "%s: could not read type register, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}
	type = raw_type[0];
	subtype = raw_type[1];

	if (type != QPNP_TM_TYPE || subtype != QPNP_TM_SUBTYPE) {
		dev_err(&spmi->dev, "%s: invalid type=%02X or subtype=%02X register value\n",
			__func__, type, subtype);
		rc = -ENODEV;
		goto err_cancel_work;
	}

	rc = qpnp_tm_init_reg(chip);
	if (rc) {
		dev_err(&spmi->dev, "%s: qpnp_tm_init_reg() failed, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}

	if (chip->adc_type == QPNP_TM_ADC_NONE) {
		rc = qpnp_tm_init_temp_no_adc(chip);
		if (rc) {
			dev_err(&spmi->dev, "%s: qpnp_tm_init_temp_no_adc() failed, rc=%d\n",
				__func__, rc);
			goto err_cancel_work;
		}
	}

	/* Start in HW control; switch to SW control when user changes mode. */
	chip->mode = THERMAL_DEVICE_DISABLED;
	rc = qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);
	if (rc) {
		dev_err(&spmi->dev, "%s: qpnp_tm_shutdown_override() failed, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}

	chip->tz_dev = thermal_zone_device_register(tm_name, TRIP_NUM, chip,
			tz_ops, 0, 0, 0, 0);
	if (chip->tz_dev == NULL) {
		dev_err(&spmi->dev, "%s: thermal_zone_device_register() failed.\n",
			__func__);
		rc = -ENODEV;
		goto err_cancel_work;
	}

	rc = request_irq(chip->irq, qpnp_tm_isr, IRQF_TRIGGER_RISING, tm_name,
			chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "%s: request_irq(%d) failed: %d\n",
			__func__, chip->irq, rc);
		goto err_free_tz;
	}

#if defined(CONFIG_MACH_MSM8974_G2_KR)
	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("qpnp_chg: spmi resource absent\n");
			goto fail_bat_if;
		}

		res = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(res && res->start)) {
			pr_err("node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			goto fail_bat_if;
		}

		rc = qpnp_register_read(chip, &subtype,
				res->start + REG_OFFSET_PERP_SUBTYPE, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			goto fail_bat_if;
		}

		switch (subtype) {
		case SMBB_BAT_IF_SUBTYPE:
			pr_err("qpnp bat_if block enable for batt insert remove irq.\n");
			chip->bat_if_base = res->start;
			chip->batt_present_irq = spmi_get_irq_byname(chip->spmi_dev,
							spmi_resource, "batt-pres");

			if (chip->batt_present_irq < 0) {
				pr_err("Unable to get batt_present_irq\n");
				goto fail_bat_if;
			}

			rc = devm_request_irq(&(chip->spmi_dev->dev), chip->batt_present_irq,
				qpnp_batif_batt_inserted_removed_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"batt_remove_insert", chip);

			if (rc < 0) {
				pr_err("Can't request %d batt irq for insert, remove : %d\n",
						chip->batt_present_irq, rc);
				goto fail_bat_if;
			}

			rc = qpnp_register_masked_write(chip, INT_EN_SET(chip->bat_if_base),
				BATT_PRES_EN_BIT, BATT_PRES_EN_BIT, 1);

			if (rc) {
				pr_err("failed to enable BAT_IF_INT_EN_SET rc=%d\n", rc);
				goto fail_bat_if;
			}
			enable_irq_wake(chip->batt_present_irq);
			break;
		default:
			break;
		}
	}

	INIT_WORK(&chip->batt_insert_remove_worker, batt_insert_remove_work);
	INIT_DELAYED_WORK(&chip->batt_insert_remove_worker_second, batt_insert_remove_work_second);
	wake_lock_init(&chip->battgone_wake_lock,
				WAKE_LOCK_SUSPEND, "batt removed");
#endif
	return 0;

err_free_tz:
	thermal_zone_device_unregister(chip->tz_dev);
err_cancel_work:
	cancel_delayed_work_sync(&chip->irq_work);
	kfree(chip->tm_name);
free_chip:
	dev_set_drvdata(&spmi->dev, NULL);
	kfree(chip);
	return rc;
#if defined(CONFIG_MACH_MSM8974_G2_KR)
fail_bat_if:
	pr_err("Cannot enable qpnp bat_if block.\n");
	return 0;
#endif
}

static int __devexit qpnp_tm_remove(struct spmi_device *spmi)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(&spmi->dev);

	dev_set_drvdata(&spmi->dev, NULL);
	thermal_zone_device_unregister(chip->tz_dev);
	kfree(chip->tm_name);
	qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);
	free_irq(chip->irq, chip);
	cancel_delayed_work_sync(&chip->irq_work);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
static int qpnp_tm_suspend(struct device *dev)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(dev);

	/* Clear override bits in suspend to allow hardware control */
	qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);

	return 0;
}

static int qpnp_tm_resume(struct device *dev)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(dev);

	/* Override hardware actions so software can control */
	if (chip->mode == THERMAL_DEVICE_ENABLED)
		qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_ENABLED);

	return 0;
}

static const struct dev_pm_ops qpnp_tm_pm_ops = {
	.suspend = qpnp_tm_suspend,
	.resume = qpnp_tm_resume,
};

#define QPNP_TM_PM_OPS	(&qpnp_tm_pm_ops)
#else
#define QPNP_TM_PM_OPS	NULL
#endif

static struct of_device_id qpnp_tm_match_table[] = {
	{ .compatible = QPNP_TM_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id qpnp_tm_id[] = {
	{ QPNP_TM_DRIVER_NAME, 0 },
	{}
};

static struct spmi_driver qpnp_tm_driver = {
	.driver = {
		.name		= QPNP_TM_DRIVER_NAME,
		.of_match_table	= qpnp_tm_match_table,
		.owner		= THIS_MODULE,
		.pm		= QPNP_TM_PM_OPS,
	},
	.probe	  = qpnp_tm_probe,
	.remove	  = __devexit_p(qpnp_tm_remove),
	.id_table = qpnp_tm_id,
};

int __init qpnp_tm_init(void)
{
	return spmi_driver_register(&qpnp_tm_driver);
}

static void __exit qpnp_tm_exit(void)
{
	spmi_driver_unregister(&qpnp_tm_driver);
}

module_init(qpnp_tm_init);
module_exit(qpnp_tm_exit);

MODULE_DESCRIPTION("QPNP PMIC Temperature Alarm driver");
MODULE_LICENSE("GPL v2");
