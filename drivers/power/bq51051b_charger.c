/*
 * BQ51051B Wireless Charging(WLC) control driver
 *
 * Copyright (C) 2012 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

//#define pr_fmt(fmt)	"%s: " fmt, __func__


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/leds-pm8xxx.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/power/bq51051b_charger.h>
#include <linux/i2c/smb349_charger.h>
#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
#include <linux/qpnp/pin.h>
#endif/*                                    */
#include <linux/delay.h>

#define	LGE_WLC_CHARGER_TEMP_SCENARIO_SYSFS
#define I2C_SUSPEND_WORKAROUND 1
#ifdef I2C_SUSPEND_WORKAROUND
extern bool i2c_suspended;
#endif

#ifdef LGE_WLC_CHARGER_TEMP_SCENARIO_SYSFS
int at_batt_temp;
int at_batt_temp_flag;
#endif/*                                   */

#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
enum {
	WLC_TEMP_NORMAL			= 0,
	WLC_TEMP_OVER_45		= 1,
	WLC_TEMP_UNDIER_M10		= 2,
	WLC_TEMP_OVER_60		= 3,
};
#endif/*                                    */

#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
static int wlc_temp_state;
#endif/*                                    */

struct bq51051b_wlc_chip {
	struct device *dev;
	struct power_supply wireless_psy;
	struct delayed_work wireless_interrupt_work;
	struct wake_lock wireless_chip_wake_lock;
	unsigned int chg_ctrl_gpio;
	unsigned int half_chg_ctrl_gpio;
	unsigned int active_n_gpio;
	unsigned int otg_ctrl_gpio;
	unsigned int wlc_ts_mpp;
	unsigned int track_gpio;
	unsigned int wireless_charging;

#if I2C_SUSPEND_WORKAROUND
	struct delayed_work 	check_suspended_work;
	int suspended;
#endif //I2C_SUSPEND_WORKAROUND
};

static const struct platform_device_id bq51051b_id[] = {
	{BQ51051B_WLC_DEV_NAME, 0},
	{},
};

static struct of_device_id bq51051b_match[] = {
	{ .compatible = "ti,bq51051b", },
	{}
};

static struct bq51051b_wlc_chip *the_chip;
static bool wireless_charging;
static bool wireless_charge_done;

static enum power_supply_property pm_power_props_wireless[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int pm_power_get_property_wireless(struct power_supply *psy,
					  enum power_supply_property psp,
					  union power_supply_propval *val)
{
	/* Check if called before init */
	/* todo workaround for below kmsg
	power_supply wireless: driver failed to report `present' property: 4294967274
	if (!the_chip)
		return -EINVAL;
	*/
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wireless_charging;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void wireless_set(struct bq51051b_wlc_chip *chip)
{
	WLC_DBG_INFO("wireless_set\n");

	wake_lock(&chip->wireless_chip_wake_lock);

	wireless_charging = true;
	wireless_charge_done = false;

	power_supply_changed(&chip->wireless_psy);
	set_wireless_power_supply_control(wireless_charging);
}

static void wireless_reset(struct bq51051b_wlc_chip *chip)
{
	WLC_DBG_INFO("wireless_reset\n");

	wireless_charging = false;
	wireless_charge_done = false;

	power_supply_changed(&chip->wireless_psy);
	set_wireless_power_supply_control(wireless_charging);

	wake_lock_timeout(&chip->wireless_chip_wake_lock, HZ*1);
}

static int wireless_charger_is_plugged(struct bq51051b_wlc_chip *chip)
{
	int ret = 0;

	ret = !(gpio_get_value(chip->active_n_gpio));
	/* for debugging */
	if (ret)
		WLC_DBG_INFO("wireless_charger plugged : true\n");
	else
		WLC_DBG_INFO("wireless_charger plugged : false\n");

	return ret;
}

#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
static void wireless_current_control(bool state)
{
	if (state == true)
		gpio_set_value(the_chip->half_chg_ctrl_gpio, 1);
	else
		gpio_set_value(the_chip->half_chg_ctrl_gpio, 0);

	WLC_DBG_INFO("half_current : %d\n", state);
}
#endif/*                                    */

#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
static void wireless_ts_control(bool state)
{
	if (state == true) {
		struct qpnp_pin_cfg wlc_ts_mpp_param = {/* stop charging */
			.mode = 1,
			.master_en = 1,
		};
		qpnp_pin_config(the_chip->wlc_ts_mpp, &wlc_ts_mpp_param);
	} else {
		struct qpnp_pin_cfg wlc_ts_mpp_param = {/* charging */
			.mode = 1,
			.master_en = 0,
		};
		qpnp_pin_config(the_chip->wlc_ts_mpp, &wlc_ts_mpp_param);
	}
	WLC_DBG_INFO("stop charge state : %d\n", state);

	return;
}
#endif/*                                    */

#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
void wireless_temp_satate(int temp_state)
{
	bool stop_charge_state = false;
	bool half_charge_current = false;

	switch (temp_state) {

	case WLC_TEMP_OVER_45:
		half_charge_current = true;
		break;

	case WLC_TEMP_UNDIER_M10:
	case WLC_TEMP_OVER_60:
		stop_charge_state = true;
		break;

	default:
		stop_charge_state = false;
		half_charge_current = false;
		break;
	}
	wireless_ts_control(stop_charge_state);
	wireless_current_control(half_charge_current);

	return;
}
#endif/*                                    */

#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
void wireless_temp_monitor(int batt_temp)
{
	if (at_batt_temp_flag == 1)
		batt_temp = at_batt_temp;

	switch (wlc_temp_state) {

	case WLC_TEMP_NORMAL:
		if (batt_temp > 55)
			wlc_temp_state = WLC_TEMP_OVER_60;
		else if (batt_temp > 45)
			wlc_temp_state = WLC_TEMP_OVER_45;
		else if (batt_temp < -10)
			wlc_temp_state = WLC_TEMP_UNDIER_M10;
		break;

	case WLC_TEMP_OVER_45:
		if (batt_temp > 55)
			wlc_temp_state = WLC_TEMP_OVER_60;
		else if (batt_temp < 42)
			wlc_temp_state = WLC_TEMP_NORMAL;
		break;

	case WLC_TEMP_UNDIER_M10:
		if (batt_temp > -5)
			wlc_temp_state = WLC_TEMP_NORMAL;
		break;

	case WLC_TEMP_OVER_60:
		if (batt_temp < 42)
			wlc_temp_state = WLC_TEMP_NORMAL;
		break;

	default:
		wlc_temp_state = WLC_TEMP_NORMAL;
		break;
	}
	WLC_DBG_INFO("%d -> state:%d\n", batt_temp, wlc_temp_state);

	wireless_temp_satate(wlc_temp_state);

	return;
}
#endif/*                                    */

void wireless_current_ctl(int batt_temp)
{
#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
	wireless_temp_monitor(batt_temp);
#endif/*                                    */
	return;
}
EXPORT_SYMBOL(wireless_current_ctl);

static void wireless_interrupt_worker(struct work_struct *work)
{
	struct bq51051b_wlc_chip *chip =
	    container_of(work, struct bq51051b_wlc_chip,
			 wireless_interrupt_work.work);

	WLC_DBG_INFO("wireless_interrupt_worker\n");

	if (wireless_charger_is_plugged(chip))
		wireless_set(chip);
	else
		wireless_reset(chip);
	return;
}
/* todo to be will modify request_threaded_irq */
static irqreturn_t wireless_interrupt_handler(int irq, void *data)
{
	int chg_state;
	struct bq51051b_wlc_chip *chip = data;

	/* todo delete below 2lines */
	chg_state = wireless_charger_is_plugged(chip);

	WLC_DBG_INFO("\nwireless is plugged state = %d\n\n", chg_state);

#if I2C_SUSPEND_WORKAROUND
	schedule_delayed_work(&chip->check_suspended_work, msecs_to_jiffies(100));
#else
	schedule_delayed_work(&chip->wireless_interrupt_work, 0);
#endif
	return IRQ_HANDLED;
}
#if I2C_SUSPEND_WORKAROUND
static void wlc_check_suspended_worker(struct work_struct *work)
{
        struct bq51051b_wlc_chip *chip =
                container_of(work, struct bq51051b_wlc_chip, check_suspended_work.work);

        if (chip->suspended && i2c_suspended)
	{
		WLC_DBG_INFO("WLC suspended. try i2c operation after 100ms.\n");
		schedule_delayed_work(&chip->check_suspended_work, msecs_to_jiffies(100));
	}
	else
	{
		WLC_DBG_INFO("WLC resumed. .\n");
		schedule_delayed_work(&chip->wireless_interrupt_work, 0);
	}
}
#endif //I2C_SUSPEND_WORKAROUND

#ifdef LGE_WLC_CHARGER_TEMP_SCENARIO_SYSFS
static ssize_t at_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r = 0;

	r = snprintf(buf, 3, "%d\n", at_batt_temp);

	WLC_DBG_INFO("temp_show:%d\n", at_batt_temp);

	return r;
}
static ssize_t at_temp_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	sscanf(buf, "%d %d", &at_batt_temp_flag, &at_batt_temp);
	if (!size)
		return -EINVAL;

	wireless_current_ctl(at_batt_temp);

	WLC_DBG_INFO("temp_store:%d(%d)\n", at_batt_temp, at_batt_temp_flag);

	return size;
}
#endif/*                                   */

#ifdef LGE_WLC_CHARGER_TEMP_SCENARIO_SYSFS
DEVICE_ATTR(at_temp, 0660, at_temp_show, at_temp_store);
#endif/*                                   */

static int __devinit bq51051b_wlc_hw_init(struct bq51051b_wlc_chip *chip)
{
	int ret;
	WLC_DBG_INFO("hw_init");

	/* chg_ctrl_gpio */
	ret =  gpio_request_one(chip->chg_ctrl_gpio, GPIOF_OUT_INIT_HIGH,
			"chg_ctrl_gpio");
	if (ret < 0) {
		pr_err("wlc: chg_ctrl_gpio request failed\n");
		return ret;
	}

	/* half_chg_ctrl_gpio */
	ret =  gpio_request_one(chip->half_chg_ctrl_gpio, GPIOF_OUT_INIT_LOW,
			"half_chg_ctrl_gpio");
	if (ret < 0) {
		pr_err("wlc: half_chg_ctrl_gpio request failed\n");
		return ret;
	}

	/* active_n_gpio pin must be monitoring the bq51051b status */
	ret =  gpio_request_one(chip->active_n_gpio, GPIOF_DIR_IN,
			"active_n_gpio");
	if (ret < 0) {
		pr_err("wlc: active_n_gpio request failed\n");
		goto err_active_n;
	}

	ret = request_irq(gpio_to_irq(chip->active_n_gpio),
			wireless_interrupt_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"wireless_charger", chip);
	if (ret < 0) {
		pr_err("wlc: wireless_charger request irq failed\n");
		goto err_active_n_irq;
	}
	enable_irq_wake(gpio_to_irq(chip->active_n_gpio));

	/* otg_ctrl_gpio */
	ret =  gpio_request_one(chip->otg_ctrl_gpio, GPIOF_OUT_INIT_LOW,
			"otg_ctrl_gpio");
	if (ret < 0) {
		pr_err("wlc: otg_ctrl_gpio request failed\n");
		goto err_otg_ctrl;
	}

	/* wlc_ts_mpp */
	ret =  gpio_request_one(chip->wlc_ts_mpp, GPIOF_OUT_INIT_LOW,
			"wlc_ts_mpp");
	if (ret < 0) {
		pr_err("wlc: wlc_ts_mpp request failed\n");
		goto err_otg_ctrl;
	}
	return 0;

err_otg_ctrl:
	free_irq(gpio_to_irq(chip->active_n_gpio), chip);
err_active_n_irq:
	gpio_free(chip->active_n_gpio);
err_active_n:
	gpio_free(chip->chg_ctrl_gpio);
	gpio_free(chip->half_chg_ctrl_gpio);
	gpio_free(chip->wlc_ts_mpp);

	return ret;

}

static int bq51051b_wlc_resume(struct device *dev)
{
	struct bq51051b_wlc_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("called before init\n");
		return 0;
	}
#if I2C_SUSPEND_WORKAROUND
	chip->suspended = 0;
#endif //I2C_SUSPEND_WORKAROUND
	return 0;
}

static int bq51051b_wlc_suspend(struct device *dev)
{
	struct bq51051b_wlc_chip *chip = dev_get_drvdata(dev);
	if (!chip) {
		pr_err("called before init\n");
		return 0;
	}
#if I2C_SUSPEND_WORKAROUND
		chip->suspended = 1;
#endif //I2C_SUSPEND_WORKAROUND
		return 0;
}

static void bq51051b_parse_dt(struct device *dev,
		struct bq51051b_wlc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->chg_ctrl_gpio = of_get_named_gpio(np, "chg_ctrl_gpio", 0);
	pdata->half_chg_ctrl_gpio =
			of_get_named_gpio(np, "half_chg_ctrl_gpio", 0);
	pdata->active_n_gpio = of_get_named_gpio(np, "active_n_gpio", 0);
	pdata->otg_ctrl_gpio = of_get_named_gpio(np, "otg_ctrl_gpio", 0);
	pdata->wlc_ts_mpp = of_get_named_gpio(np, "wlc_ts_mpp", 0);
	pdata->track_gpio = of_get_named_gpio(np, "track_gpio", 0);
}

/* todo list
  insert touch code
  bms notify
*/
static int __devinit bq51051b_wlc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct bq51051b_wlc_chip *chip;
	struct bq51051b_wlc_platform_data *pdata;
#ifdef LGE_WLC_CHARGER_TEMP_SCENARIO_SYSFS
	int err = 0;
#endif/*                                   */
	WLC_DBG_INFO("probe\n");

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct bq51051b_wlc_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("wlc: missing platform data\n");
			return -ENODEV;
		}
		pdev->dev.platform_data = pdata;
		bq51051b_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}

	chip = kzalloc(sizeof(struct bq51051b_wlc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("wlc: Cannot allocate bq51051b_wlc_chip\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	chip->chg_ctrl_gpio = pdata->chg_ctrl_gpio;
	WLC_DBG_INFO("chg_ctrl_gpio = %d.\n", chip->chg_ctrl_gpio);

	chip->half_chg_ctrl_gpio = pdata->half_chg_ctrl_gpio;
	WLC_DBG_INFO("half_chg_ctrl_gpio = %d.\n", chip->half_chg_ctrl_gpio);

	chip->active_n_gpio = pdata->active_n_gpio;
	WLC_DBG_INFO("active_n_gpio = %d.\n", chip->active_n_gpio);

	chip->otg_ctrl_gpio = pdata->otg_ctrl_gpio;
	WLC_DBG_INFO("otg_ctrl_gpio = %d.\n", chip->otg_ctrl_gpio);

	chip->wlc_ts_mpp = pdata->wlc_ts_mpp;
	WLC_DBG_INFO("wlc_ts_mpp = %d.\n", chip->wlc_ts_mpp);

	chip->track_gpio	= pdata->track_gpio;
	WLC_DBG_INFO("track_gpio = %d.\n", chip->track_gpio);

	rc = bq51051b_wlc_hw_init(chip);
	if (rc) {
		pr_err("wlc: couldn't init hardware rc = %d\n", rc);
		goto free_chip;
	}

	chip->wireless_psy.name = "wireless";
	chip->wireless_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wireless_psy.supplied_to = pm_power_supplied_to;
	chip->wireless_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	chip->wireless_psy.properties = pm_power_props_wireless;
	chip->wireless_psy.num_properties = ARRAY_SIZE(pm_power_props_wireless);
	chip->wireless_psy.get_property = pm_power_get_property_wireless;

	rc = power_supply_register(chip->dev, &chip->wireless_psy);
	if (rc < 0) {
		pr_err("wlc: power_supply_register wireless failed rx = %d\n",
			      rc);
		goto free_chip;
	}

	platform_set_drvdata(pdev, chip);
	the_chip = chip;
#if I2C_SUSPEND_WORKAROUND
	INIT_DELAYED_WORK(&chip->check_suspended_work,wlc_check_suspended_worker);
#endif //I2C_SUSPEND_WORKAROUND

	INIT_DELAYED_WORK(&chip->wireless_interrupt_work, wireless_interrupt_worker);
	wake_lock_init(&chip->wireless_chip_wake_lock, WAKE_LOCK_SUSPEND,
		       "bq51051b_wireless_chip");

#ifdef CONFIG_LGE_WLC_CHARGER_TEMP_SCENARIO
	wlc_temp_state = WLC_TEMP_NORMAL;
#endif/*                                    */

	/* For Booting Wireless_charging and For Power Charging Logo In Wireless Charging */
	if (wireless_charger_is_plugged(chip))
		wireless_set(chip);

#ifdef LGE_WLC_CHARGER_TEMP_SCENARIO_SYSFS
	err = device_create_file(&pdev->dev, &dev_attr_at_temp);
#endif/*                                   */
	WLC_DBG_INFO("probe success\n");
	return 0;

free_chip:
	kfree(chip);
	return rc;
}

static int __devexit bq51051b_wlc_remove(struct platform_device *pdev)
{
	struct bq51051b_wlc_chip *chip = platform_get_drvdata(pdev);

	WLC_DBG_INFO("remove\n");

	wake_lock_destroy(&chip->wireless_chip_wake_lock);
	the_chip = NULL;
	platform_set_drvdata(pdev, NULL);
	power_supply_unregister(&chip->wireless_psy);
	free_irq(gpio_to_irq(chip->active_n_gpio), chip);
	gpio_free(chip->active_n_gpio);
	gpio_free(chip->chg_ctrl_gpio);
	gpio_free(chip->half_chg_ctrl_gpio);
	gpio_free(chip->otg_ctrl_gpio);
	gpio_free(chip->wlc_ts_mpp);
	kfree(chip);
#ifdef LGE_WLC_CHARGER_TEMP_SCENARIO_SYSFS
	device_remove_file(&pdev->dev, &dev_attr_at_temp);
#endif/*                                   */
	return 0;
}

static const struct dev_pm_ops bq51051b_pm_ops = {
	.suspend = bq51051b_wlc_suspend,
	.resume = bq51051b_wlc_resume,
};

static struct platform_driver bq51051b_wlc_driver = {
	.probe = bq51051b_wlc_probe,
	.remove = __devexit_p(bq51051b_wlc_remove),
	.id_table = bq51051b_id,
	.driver = {
		.name = BQ51051B_WLC_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = &bq51051b_pm_ops,
		.of_match_table = bq51051b_match,
	},
};

static int __init bq51051b_wlc_init(void)
{
	return platform_driver_register(&bq51051b_wlc_driver);
}

static void __exit bq51051b_wlc_exit(void)
{
	platform_driver_unregister(&bq51051b_wlc_driver);
}

late_initcall(bq51051b_wlc_init);
module_exit(bq51051b_wlc_exit);

MODULE_AUTHOR("Kyungtae Oh <Kyungtae.oh@lge.com>");
MODULE_DESCRIPTION("BQ51051B Wireless Charger Control Driver");
MODULE_LICENSE("GPL v2");
