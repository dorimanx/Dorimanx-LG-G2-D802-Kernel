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
#include <linux/power/bq51053b_charger.h>
#include <linux/delay.h>
#if defined(CONFIG_SMB349_CHARGER)
#include <linux/i2c/smb349_charger.h>
#elif defined(CONFIG_BQ24192_CHARGER)
#include <linux/i2c/bq24192_charger.h>
#endif

#define MAX_CHECKING_COUNT	10
#define MAX_USB_CHECKING_COUNT	4

#define I2C_SUSPEND_WORKAROUND 1
#ifdef I2C_SUSPEND_WORKAROUND
extern bool i2c_suspended;
#endif
struct bq51053b_wlc_chip {
	struct device *dev;
	struct power_supply wireless_psy;
	struct delayed_work wireless_interrupt_work;
	struct delayed_work wireless_set_online_work;
	struct delayed_work wireless_set_offline_work;
	struct delayed_work wireless_eoc_work;
	struct wake_lock wireless_chip_wake_lock;
	struct wake_lock wireless_eoc_wake_lock;

	unsigned int wlc_int_gpio;
	unsigned int wlc_full_chg;
#if I2C_SUSPEND_WORKAROUND
	struct delayed_work 	check_suspended_work;
	int suspended;
#endif //I2C_SUSPEND_WORKAROUND
};

static const struct platform_device_id bq51053b_id[] = {
	{BQ51053B_WLC_DEV_NAME, 0},
	{},
};

static struct of_device_id bq51053b_match[] = {
	{ .compatible = "ti,bq51053b", },
	{}
};

static struct bq51053b_wlc_chip *the_chip;
static bool wireless_charging;

static enum power_supply_property pm_power_props_wireless[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};
int checking_insert_cnt = 0;
int checking_usb_insert_cnt = 0;

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

int is_wireless_charger_plugged_internal(struct bq51053b_wlc_chip *chip)
{
	int ret = 0;

/*When WLC power is supplied, wlc_int_gpio is low
 *(Because it is connected to inverse ACOK )*/
	ret = !(gpio_get_value(the_chip->wlc_int_gpio));
	return ret;
}

int is_wireless_charger_plugged(void)
{

	int ret = 0;

	if (!the_chip){
		pr_err("[WLC] %s : bq51053b_wlc_chip does not exist yet\n",__func__);
		return 0;
	}

	ret = is_wireless_charger_plugged_internal(the_chip);
	return ret;

}

EXPORT_SYMBOL(is_wireless_charger_plugged);

/* Update WLC psy to online after checking vbus
  * Sometimes WLC OVP GPIO swing when USB is removed
  * because of remained WLC power on backcover
  * Check every 50msec from 200msec to 700msec */

static void wireless_set_online_work (struct work_struct *work){
	int wlc=0;
	bool vbus =0;
	struct bq51053b_wlc_chip *chip = container_of(work,
		struct bq51053b_wlc_chip,wireless_set_online_work.work);

	wlc = is_wireless_charger_plugged_internal(chip);
#if defined(CONFIG_SMB349_CHARGER)
	vbus = external_smb349_is_charger_present();
#elif defined(CONFIG_BQ24192_CHARGER)
	vbus = external_bq24192_is_charger_present();
#endif

	checking_insert_cnt++;

	if(wlc && vbus){
		pr_err("[WLC] %s : Wireless psy is updated to ONLINE !!!!!\n",__func__);
		wireless_charging = true;
		power_supply_changed(&chip->wireless_psy);
		return;
	}

	if(!wlc || (checking_insert_cnt > MAX_CHECKING_COUNT)){
		pr_err("[WLC] %s : It was Ghost WLC (wlc(%d) vbus(%d))........count : %d\n"
			,__func__,wlc,vbus,checking_insert_cnt);
	}else{
		pr_err("[WLC] %s : Checking Ghost WLC (wlc(%d) vbus(%d)).......count : %d\n"
			,__func__,wlc,vbus,checking_insert_cnt);
		schedule_delayed_work(&chip->wireless_set_online_work,
				round_jiffies_relative(msecs_to_jiffies(50)));
	}
}

/* Update USB psy to online after checking vbus
  * Sometimes SOURCE is switched from WLC to USB
  * without smb349 irq (Charger IRQ)
  * Check every 50msec from 500msec to 700msec */

static void wireless_set_offline_work(struct work_struct *work){
	int wlc=0;
	bool vbus =0;
	int usb_present =0;
	struct bq51053b_wlc_chip *chip = container_of(work,
		struct bq51053b_wlc_chip,wireless_set_offline_work.work);

	wlc = is_wireless_charger_plugged_internal(chip);
#if defined(CONFIG_SMB349_CHARGER)
	vbus = external_smb349_is_charger_present();
#elif defined(CONFIG_BQ24192_CHARGER)
	vbus = external_bq24192_is_charger_present();
#endif
	usb_present = get_usb_present();

	checking_usb_insert_cnt++;

	if(!wlc){
		/*Need to W/R*/
		if(!usb_present && vbus){
			pr_err("[WLC] %s : USB is inserted and No SMB349 IRQ \n",__func__);
			pr_err("[WLC] %s : USB psy is updated to ONLINE !!!\n",__func__);
			set_usb_present(1); /*W/R*/
			if(wake_lock_active(&chip->wireless_chip_wake_lock)){
				pr_err("[WLC] %s : RELEASE wakelock\n",__func__);
				wake_unlock(&chip->wireless_chip_wake_lock);
			}
			return;
		}

		/*Normal Case*/
		if(usb_present || (checking_usb_insert_cnt > MAX_USB_CHECKING_COUNT))
		{
			pr_err("[WLC] %s : USB Already updated or W/O USB (usb(%d) cnt(%d)) \n"
						,__func__,usb_present,checking_usb_insert_cnt);
			if(wake_lock_active(&chip->wireless_chip_wake_lock)){
				pr_err("[WLC] %s : RELEASE wakelock\n",__func__);
				wake_unlock(&chip->wireless_chip_wake_lock);
			}
			return;
		}else{
			pr_err("[WLC] %s : Checking USB ........ count : %d\n \n"
						,__func__,checking_usb_insert_cnt);
			schedule_delayed_work(&chip->wireless_set_offline_work,
					round_jiffies_relative(msecs_to_jiffies(50)));
			return;
		}
	}
}

static void wireless_inserted(struct bq51053b_wlc_chip *chip)
{
	pr_err("[WLC] %s \n",__func__);

	if(!wake_lock_active(&chip->wireless_chip_wake_lock)){
		pr_err("[WLC] %s : ACQAURE wakelock\n",__func__);
		wake_lock(&chip->wireless_chip_wake_lock);
	}

#if defined(CONFIG_SMB349_CHARGER) || defined(CONFIG_BQ24192_CHARGER)
	set_wireless_power_supply_control(true);	/*Notify to Charger driver*/
#endif
	checking_insert_cnt = 0;
	schedule_delayed_work(&chip->wireless_set_online_work,
			round_jiffies_relative(msecs_to_jiffies(200)));
}

static void wireless_removed(struct bq51053b_wlc_chip *chip)
{
	pr_err("[WLC] %s \n",__func__);

#if defined(CONFIG_SMB349_CHARGER) || defined(CONFIG_BQ24192_CHARGER)
	set_wireless_power_supply_control(false);	/*Notify to Charger driver*/
#endif

	checking_usb_insert_cnt = 0;
	pr_err("[WLC] %s : Wireless psy is updated to OFFLINE !!!\n",__func__);
	wireless_charging = false;
	power_supply_changed(&chip->wireless_psy);
	schedule_delayed_work(&chip->wireless_set_offline_work,
			round_jiffies_relative(msecs_to_jiffies(500)));
}

static void wireless_interrupt_worker(struct work_struct *work)
{
	struct bq51053b_wlc_chip *chip =
	    container_of(work, struct bq51053b_wlc_chip,
			 wireless_interrupt_work.work);

	pr_err("[WLC] %s \n",__func__);

	if (is_wireless_charger_plugged_internal(chip))
		wireless_inserted(chip);
	else{
		wireless_removed(chip);
	}
}

static irqreturn_t wireless_interrupt_handler(int irq, void *data)
{
	int chg_state;
	struct bq51053b_wlc_chip *chip = data;

	chg_state = is_wireless_charger_plugged_internal(chip);
	if (chg_state)
		pr_err("[WLC] %s : I'm on the WLC PAD\n",__func__);
	else
		pr_err("[WLC] %s : I'm NOT on the WLC PAD\n",__func__);
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
        struct bq51053b_wlc_chip *chip =
                container_of(work, struct bq51053b_wlc_chip, check_suspended_work.work);

        if (chip->suspended && i2c_suspended)
	{
		pr_info("WLC suspended. try i2c operation after 100ms.\n");
		schedule_delayed_work(&chip->check_suspended_work, msecs_to_jiffies(100));
	}
	else
	{
		pr_info("WLC resumed. .\n");
		schedule_delayed_work(&chip->wireless_interrupt_work, 0);
	}
}
#endif //I2C_SUSPEND_WORKAROUND

static void wireless_eoc_work(struct work_struct *work)
{
	struct bq51053b_wlc_chip *chip = container_of(work,
		struct bq51053b_wlc_chip,wireless_eoc_work.work);

	wake_lock(&chip->wireless_eoc_wake_lock);

	gpio_set_value(chip->wlc_full_chg, 1);
	pr_err("[WLC] %s : Send EPT signal!! \n",__func__);
	msleep(3500);
	gpio_set_value(chip->wlc_full_chg, 0);
	pr_err("[WLC] %s : Re-enable RX!! \n",__func__);

	wake_unlock(&chip->wireless_eoc_wake_lock);
}

int wireless_charging_completed()
{
	pr_err("[WLC] %s \n",__func__);
	schedule_delayed_work(&the_chip->wireless_eoc_work,
			round_jiffies_relative(msecs_to_jiffies(2000)));
	return 1;
}

EXPORT_SYMBOL(wireless_charging_completed);

static int __devinit bq51053b_wlc_hw_init(struct bq51053b_wlc_chip *chip)
{
	int ret;
	pr_err("[WLC] %s \n",__func__);

	/* wlc_int_gpio DIR_IN and register IRQ */
	ret =  gpio_request(chip->wlc_int_gpio, "wlc_int_gpio");
	if (ret < 0) {
		pr_err("[WLC] %s : failed to request gpio wlc_int\n",__func__);
		goto err_request_gpio1_failed;
	}

	ret =  gpio_direction_input(chip->wlc_int_gpio);
	if (ret < 0) {
		pr_err("[WLC] %s : failed to set gpio wlc_int\n",__func__);
		goto err_request_irq_failed;
	}

	ret = request_irq(gpio_to_irq(chip->wlc_int_gpio),
			wireless_interrupt_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"wireless_charger", chip);
	if (ret < 0) {
		pr_err("[WLC] %s : failed to request IRQ\n",__func__);
		goto err_request_irq_failed;
	}

	ret = enable_irq_wake(gpio_to_irq(chip->wlc_int_gpio));
	if (ret < 0) {
		pr_err("[WLC] %s : failed to request weake up IRQ\n",__func__);
		goto err_request_wakeup_irq_failed;
	}

	/* wlc_full_chg DIR_OUT and Low */

	ret =  gpio_request_one(chip->wlc_full_chg, GPIOF_OUT_INIT_LOW,
			"wlc_full_chg");
	if (ret < 0) {
		pr_err("[WLC] %s : failed to request gpio wlc_full_chg\n",__func__);
		goto err_request_gpio2_failed;
	}

	return 0;

err_request_gpio2_failed:
	disable_irq_wake(gpio_to_irq(chip->wlc_int_gpio));
err_request_wakeup_irq_failed:
	free_irq(gpio_to_irq(chip->wlc_int_gpio), NULL);
err_request_irq_failed:
	gpio_free(chip->wlc_int_gpio);
err_request_gpio1_failed:

	return ret;

}

static int bq51053b_wlc_resume(struct device *dev)
{
	struct bq51053b_wlc_chip *chip = dev_get_drvdata(dev);

	if (!chip) {
		pr_err("called before init\n");
		return 0;
	}

#if I2C_SUSPEND_WORKAROUND
		chip->suspended = 0;
#endif //I2C_SUSPEND_WORKAROUND

		return 0;

	return 0;
}

static int bq51053b_wlc_suspend(struct device *dev)
{
	struct bq51053b_wlc_chip *chip = dev_get_drvdata(dev);

	if (!chip) {
		pr_err("called before init\n");
		return 0;
	}

#if I2C_SUSPEND_WORKAROUND
	chip->suspended = 1;
#endif //I2C_SUSPEND_WORKAROUND

	return 0;
}

static void bq51053b_parse_dt(struct device *dev,
		struct bq51053b_wlc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->wlc_int_gpio = of_get_named_gpio(np, "wlc_int_gpio", 0);
	pdata->wlc_full_chg = of_get_named_gpio(np, "wlc_full_chg", 0);

}

static int __devinit bq51053b_wlc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct bq51053b_wlc_chip *chip;
	struct bq51053b_wlc_platform_data *pdata;

	pr_err("[WLC] %s : probe start\n",__func__);

	/*Read platform data from dts file*/

	pdata = devm_kzalloc(&pdev->dev,
					sizeof(struct bq51053b_wlc_platform_data),
					GFP_KERNEL);
	if (!pdata) {
		pr_err("[WLC] %s : missing platform data\n",__func__);
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		pdev->dev.platform_data = pdata;
		bq51053b_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}

	chip = kzalloc(sizeof(struct bq51053b_wlc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("[WLC] %s : Cannot allocate bq51053b_wlc_chip\n",__func__);
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	/*Check charger IC probe is finshed or not , before starting probe*/
#if defined(CONFIG_SMB349_CHARGER)
	rc = smb349_is_ready();
	if (rc)
		goto free_chip;
#elif defined(CONFIG_BQ24192_CHARGER)
	rc = bq24192_is_ready();
	if (rc)
		goto free_chip;
#endif

	/*Set Power Supply type for wlc*/
	chip->wireless_psy.name = "wireless";
	chip->wireless_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wireless_psy.supplied_to = pm_power_supplied_to;
	chip->wireless_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	chip->wireless_psy.properties = pm_power_props_wireless;
	chip->wireless_psy.num_properties = ARRAY_SIZE(pm_power_props_wireless);
	chip->wireless_psy.get_property = pm_power_get_property_wireless;

	rc = power_supply_register(chip->dev, &chip->wireless_psy);
	if (rc < 0) {
		pr_err("[WLC] %s : power_supply_register wireless failed rx = %d\n",
			      __func__,rc);
		goto free_chip;
	}

#if I2C_SUSPEND_WORKAROUND
	INIT_DELAYED_WORK(&chip->check_suspended_work,wlc_check_suspended_worker);
#endif //I2C_SUSPEND_WORKAROUND
	INIT_DELAYED_WORK(&chip->wireless_interrupt_work, wireless_interrupt_worker);
	INIT_DELAYED_WORK(&chip->wireless_set_online_work,wireless_set_online_work);
	INIT_DELAYED_WORK(&chip->wireless_set_offline_work,wireless_set_offline_work);
	INIT_DELAYED_WORK(&chip->wireless_eoc_work,wireless_eoc_work);
	/*Set  Wake lock for wlc*/
	wake_lock_init(&chip->wireless_chip_wake_lock, WAKE_LOCK_SUSPEND,
		       "bq51053b_wireless_chip");
	wake_lock_init(&chip->wireless_eoc_wake_lock, WAKE_LOCK_SUSPEND,
		       "bq51053b_wireless_eoc");

	/*Set GPIO & Enable GPIO IRQ for wlc*/
	chip->wlc_int_gpio = pdata->wlc_int_gpio;
	pr_err("[WLC] %s : wlc_int_gpio = %d.\n",__func__, chip->wlc_int_gpio);

	chip->wlc_full_chg = pdata->wlc_full_chg;
	pr_err("[WLC] %s : wlc_full_chg = %d.\n",__func__, chip->wlc_full_chg);

	rc = bq51053b_wlc_hw_init(chip);
	if (rc) {
		pr_err("[WLC] %s : couldn't init hardware rc = %d\n",__func__, rc);
		goto free_chip;
	}

	platform_set_drvdata(pdev, chip);

	the_chip = chip;

	/* For Booting Wireless_charging and For Power Charging Logo In Wireless Charging */
	if (is_wireless_charger_plugged_internal(chip)){
		pr_err("[WLC] %s : I'm on WLC PAD during booting\n ", __func__);
		set_usb_present(false);
		wireless_inserted(chip);
	}
	/*else
		wireless_removed(chip);*/

	pr_err("[WLC] %s : probe done\n", __func__);

	return 0;

free_chip:
	kfree(chip);
	return rc;
}

static int __devexit bq51053b_wlc_remove(struct platform_device *pdev)
{
	struct bq51053b_wlc_chip *chip = platform_get_drvdata(pdev);

	pr_err("[WLC] %s :remove\n", __func__);
	wake_lock_destroy(&chip->wireless_chip_wake_lock);
	wake_lock_destroy(&chip->wireless_eoc_wake_lock);
	the_chip = NULL;
	platform_set_drvdata(pdev, NULL);
	power_supply_unregister(&chip->wireless_psy);
	free_irq(gpio_to_irq(chip->wlc_int_gpio), chip);
	gpio_free(chip->wlc_int_gpio);
	gpio_free(chip->wlc_full_chg);
	kfree(chip);
	return 0;
}

static const struct dev_pm_ops bq51053b_pm_ops = {
	.suspend = bq51053b_wlc_suspend,
	.resume = bq51053b_wlc_resume,
};

static struct platform_driver bq51053b_wlc_driver = {
	.probe = bq51053b_wlc_probe,
	.remove = __devexit_p(bq51053b_wlc_remove),
	.id_table = bq51053b_id,
	.driver = {
		.name = BQ51053B_WLC_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = &bq51053b_pm_ops,
		.of_match_table = bq51053b_match,
	},
};

static int __init bq51053b_wlc_init(void)
{
	return platform_driver_register(&bq51053b_wlc_driver);
}

static void __exit bq51053b_wlc_exit(void)
{
	platform_driver_unregister(&bq51053b_wlc_driver);
}

late_initcall(bq51053b_wlc_init);
module_exit(bq51053b_wlc_exit);

MODULE_AUTHOR("Kyungtae Oh <Kyungtae.oh@lge.com>");
MODULE_DESCRIPTION("BQ51053B Wireless Charger Control Driver");
MODULE_LICENSE("GPL v2");
