/*
 *  LGE currentnow driver to read current value from external Rsense
 *
 *  Copyright (C) 2013 LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/spmi.h>
#include <linux/ioport.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/qpnp/qpnp-adc.h>

#define CURRENTNOW_DEV_NAME		"lge,currentnow_device"

#define IADC1_BMS_ADC_CH_SEL_CTL	0x48
#define ADC_CH_SEL_MASK			0x7
#define BMS_EN_MASK			0x80
#define CC_READING_RESOLUTION_N		542535
#define CC_READING_RESOLUTION_D		100000
#define QPNP_ADC_GAIN_IDEAL		3291LL
#define IADC_BASE			0x3800
#define BMS_BASE			0x4000
#define BMS_EN_CTL			0x4046
#define BMS1_VSENSE_AVG_DATA0		0x4098
#define RSENSE_MICRO_OHM		10000
#define RATIO_TO_MICRO_A		100

struct cn_chip {
	struct device			*dev;
	struct power_supply		cn_psy;
	struct spmi_device		*spmi;
	int				r_sense_uohm;
	u16				base;
	u16				iadc_base;
	u16				bms_enabled;
};

static enum power_supply_property cn_props[] = {
	POWER_SUPPLY_PROP_VIRT_CURRENT_NOW,
	POWER_SUPPLY_PROP_VIRT_ENABLE_BMS,
};

static int cn_read_wrapper(struct cn_chip *chip, u8 *val,
		u16 addr, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		pr_err("SPMI read failed rc=%d\n", rc);
		return rc;
	}
	return 0;
}

static int cn_write_wrapper(struct cn_chip *chip, u8 *val,
			u16 addr, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		pr_err("SPMI write failed rc=%d\n", rc);
		return rc;
	}
	return 0;
}
static int cn_masked_write_base(struct cn_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = cn_read_wrapper(chip, &reg, addr, 1);
	if (rc) {
		pr_err("read failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = cn_write_wrapper(chip, &reg, addr, 1);
	if (rc) {
		pr_err("write failed addr = %03X, val = %02x,\
				mask = %02x, reg = %02x, rc = %d\n",
				addr, val, mask, reg, rc);
		return rc;
	}
	return 0;
}

static int cn_masked_write_iadc(struct cn_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	return cn_masked_write_base(chip, chip->iadc_base + addr, mask, val);
}

static int cc_reading_to_uv(int16_t reading)
{
	return div_s64(reading * CC_READING_RESOLUTION_N,
					CC_READING_RESOLUTION_D);
}

static s64 cc_adjust_for_gain(s64 uv, uint16_t gain)
{
	s64 result_uv;

	pr_debug("adjusting_uv = %lld\n", uv);
	if (gain == 0) {
		pr_debug("gain is %d, not adjusting\n", gain);
		return uv;
	}
	pr_debug("adjusting by factor: %lld/%hu = %lld%%\n",
			QPNP_ADC_GAIN_IDEAL, gain,
			div_s64(QPNP_ADC_GAIN_IDEAL * 100LL, (s64)gain));

	result_uv = div_s64(uv * QPNP_ADC_GAIN_IDEAL, (s64)gain);
	pr_debug("result_uv = %lld\n", result_uv);
	return result_uv;
}

static int convert_vsense_to_uv(struct cn_chip *chip,
					int16_t reading)
{
	struct qpnp_iadc_calib calibration;

	qpnp_iadc_get_gain_and_offset(&calibration);
	return cc_adjust_for_gain(cc_reading_to_uv(reading),
			calibration.gain_raw - calibration.offset_raw);
}

static int cn_set_property(struct power_supply *psy,
		  enum power_supply_property psp,
		  const union power_supply_propval *val)
{
	int ret;
	struct cn_chip *chip = container_of(psy, struct cn_chip, cn_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VIRT_ENABLE_BMS:
		if (!val->intval)
			ret = cn_masked_write_base(chip, BMS_EN_CTL,
					BMS_EN_MASK, 0x00);
		else
			ret = cn_masked_write_base(chip, BMS_EN_CTL,
					BMS_EN_MASK, 0x80);


		if (ret) {
			chip->bms_enabled = 0;
			pr_err("fail to enable bms\n");
		} else {
			chip->bms_enabled = 1;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cn_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int16_t reg;
	uint result_uv;
	struct cn_chip *chip = container_of(psy, struct cn_chip, cn_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VIRT_CURRENT_NOW:
		cn_read_wrapper(chip, (u8 *)&reg,
				BMS1_VSENSE_AVG_DATA0, 2);
		result_uv = convert_vsense_to_uv(chip, reg);
		val->intval = (int)(result_uv * RATIO_TO_MICRO_A);
		break;
	case POWER_SUPPLY_PROP_VIRT_ENABLE_BMS:
		val->intval = chip->bms_enabled;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static int set_cn_iadc_channel(struct cn_chip *chip)
{
	int rc;

	rc = cn_masked_write_iadc(chip,
			IADC1_BMS_ADC_CH_SEL_CTL,
			ADC_CH_SEL_MASK,
			EXTERNAL_RSENSE);
	if (rc) {
		pr_err("Unable to set IADC1_BMS channel %x to %x: %d\n",
				IADC1_BMS_ADC_CH_SEL_CTL,
				EXTERNAL_RSENSE, rc);
		return rc;
	}
	return 0;
}

int spmi_cn_add_controller(struct cn_chip *chip, struct spmi_device *spmi)
{
	chip->spmi = spmi;
	chip->dev = &(spmi->dev);

	return 0;
}
static int __devinit cn_probe(struct spmi_device *spmi)
{
	int rc;
	struct cn_chip *chip;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);

	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}
	chip->iadc_base = IADC_BASE;
	chip->base = BMS_BASE;
	chip->r_sense_uohm = RSENSE_MICRO_OHM; /* rsense is 10mohm */
	chip->bms_enabled = 0;

	rc = qpnp_iadc_is_ready();
	if (rc) {
		pr_info("iadc not ready: %d, deferring probe\n", rc);
		goto error_read;
	}

	rc = spmi_cn_add_controller(chip, spmi);
	if (rc) {
		pr_info("error registering spmi resource %d\n", rc);
		goto error_read;
	}

	rc = set_cn_iadc_channel(chip);
	if (rc) {
		pr_err("Unable to get iadc selected channel = %d\n", rc);
		goto error_read;
	}
	chip->cn_psy.name		= "cn";
	chip->cn_psy.type		= POWER_SUPPLY_TYPE_BMS;
	chip->cn_psy.get_property	= cn_get_property;
	chip->cn_psy.set_property	= cn_set_property;
	chip->cn_psy.properties		= cn_props;
	chip->cn_psy.num_properties	= ARRAY_SIZE(cn_props);

	rc = power_supply_register(chip->dev, &chip->cn_psy);

	if (rc < 0) {
		pr_err("power_supply_register bms failed rc = %d\n", rc);
		goto unregister_dc;
	}

	return 0;
unregister_dc:
	power_supply_unregister(&chip->cn_psy);
error_read:
	kfree(chip);
	return rc;
}
static struct of_device_id cn_match_table[] = {
	{ .compatible = CURRENTNOW_DEV_NAME},
	{}
};


static int __devexit
cn_remove(struct spmi_device *spmi)
{
	return 0;
}

static int cn_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops cn_pm_ops = {
	.resume		= cn_resume,
};

static struct spmi_driver cn_driver = {
	.probe		= cn_probe,
	.remove		= __devexit_p(cn_remove),
	.driver		= {
		.name		= CURRENTNOW_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= cn_match_table,
		.pm		= &cn_pm_ops,
	},
};

static int __init cn_init(void)
{
	return spmi_driver_register(&cn_driver);
}

static void __exit cn_exit(void)
{
	return spmi_driver_unregister(&cn_driver);
}

module_init(cn_init);
module_exit(cn_exit);

MODULE_DESCRIPTION("currentnow Driver");
MODULE_LICENSE("GPL v2");
