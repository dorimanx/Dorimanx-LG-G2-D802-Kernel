/*
 *  lge_battery_id.c
 *
 *  LGE Battery Charger Interface Driver
 *
 *  Copyright (C) 2011 LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * This driver is really ugly.
 * I want some smart guy to implement again.. more pretty..
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include <linux/power/lge_battery_id.h>
#include <mach/msm_smsm.h>

struct lge_battery_id_info {
	struct device		*dev;
	uint			id;
	uint			pullup;
	uint			batt_info;
	struct power_supply	psy_batt_id;
};

static enum power_supply_property lge_battery_id_battery_props[] = {
	POWER_SUPPLY_PROP_BATTERY_ID_CHECKER,
};

/*
 * TBD : This function should be more intelligent.
 * Should directly access battery id circuit via 1-Wired.
 */
static int lge_battery_id_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct lge_battery_id_info *info = container_of(psy,
			struct lge_battery_id_info,
			psy_batt_id);

	switch (psp) {
		case POWER_SUPPLY_PROP_BATTERY_ID_CHECKER:
			val->intval = info->batt_info;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static __devinit int lge_battery_id_probe(struct platform_device *pdev)
{
	struct lge_battery_id_platform_data *pdata =
		dev_get_platdata(&pdev->dev);
	struct lge_battery_id_info *info;
	//struct power_supply *psy;
	int ret = 0;
	uint *smem_batt = 0;

	dev_info(&pdev->dev, "%s: LGE Battery ID Checker \n", __func__);

	info = kzalloc(sizeof(struct lge_battery_id_info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "memory error\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;

	info->id = pdata->id;
	info->pullup = pdata->pullup;
	smem_batt = (uint *)smem_alloc(SMEM_BATT_INFO, sizeof(smem_batt));
	if (smem_batt == NULL) {
		pr_err("%s : smem_alloc returns NULL\n",__func__);
		info->batt_info = 0;
	} else {
		info->batt_info = *smem_batt;
	}

	info->psy_batt_id.name		= "battery_id";
	info->psy_batt_id.type		= POWER_SUPPLY_TYPE_BATTERY;
	info->psy_batt_id.get_property	= lge_battery_id_get_property;
	info->psy_batt_id.properties	= lge_battery_id_battery_props;
	info->psy_batt_id.num_properties=
		ARRAY_SIZE(lge_battery_id_battery_props);

	ret = power_supply_register(&pdev->dev, &info->psy_batt_id);
	if (ret) {
		dev_err(&pdev->dev, "failed: power supply register\n");
		goto err_register;
	}

	return ret;

err_register:
	platform_set_drvdata(pdev, NULL);
	kfree(info);
	return ret;
}

static int __devexit lge_battery_id_remove(struct platform_device *pdev)
{
	struct lge_battery_id_info *info = platform_get_drvdata(pdev);

	power_supply_unregister(&info->psy_batt_id);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM)
static int lge_battery_id_suspend(struct device *dev)
{
	//struct lge_battery_id_info *info = dev_get_drvdata(dev);

	return 0;
}

static int lge_battery_id_resume(struct device *dev)
{
	//struct lge_battery_id_info *info = dev_get_drvdata(dev);

	return 0;
}

static const struct dev_pm_ops lge_battery_id_pm_ops = {
	.suspend	= lge_battery_id_suspend,
	.resume		= lge_battery_id_resume,
};
#endif

static struct platform_driver lge_battery_id_driver = {
	.driver = {
		.name   = "lge_battery_id",
		.owner  = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm     = &lge_battery_id_pm_ops,
#endif
	},
	.probe  = lge_battery_id_probe,
	.remove = __devexit_p(lge_battery_id_remove),
};

static int __init lge_battery_id_init(void)
{
	return platform_driver_register(&lge_battery_id_driver);
}

static void __exit lge_battery_id_exit(void)
{
	platform_driver_unregister(&lge_battery_id_driver);
}


module_init(lge_battery_id_init);
module_exit(lge_battery_id_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cowboy");
MODULE_DESCRIPTION("LGE Battery ID Checker");

