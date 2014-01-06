/*
 * Copyright LG Electronics (c) 2011
 * All rights reserved.
 * Author: Fred Cho <fred.cho@lge.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/cradle.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <mach/board_lge.h>

#define CARKIT_DETECT_DELAY 200

static int pre_set_flag;
struct pm8xxx_carkit {
	struct switch_dev sdev;
	struct delayed_work work;
	struct device *dev;
	struct wake_lock wake_lock;
	const struct pm8xxx_carkit_platform_data *pdata;
	int deskdock;
	int carkit;
	spinlock_t lock;
	int state;
};

static struct workqueue_struct *carkit_wq;
static struct pm8xxx_carkit *cradle;

static void boot_carkit_det_func(void)
{
	int state;

	if (cradle->pdata->hallic_carkit_detect_pin)
		cradle->carkit = !gpio_get_value(cradle->pdata->hallic_carkit_detect_pin);

	printk("%s : boot carkit === > %d \n", __func__ , cradle->carkit);

	if (cradle->carkit == 1)
		state = CARKIT_DOCKED;
	else
		state = CARKIT_NO_DEV;

	if (cradle->deskdock == 1) {
		printk("%s : [Cradle] carkit ignored - deskdock connected\n", __func__);
	} else {
		cradle->state = state;
		wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
		switch_set_state(&cradle->sdev, cradle->state);
		printk("%s : [Cradle] boot carkit value is %d\n", __func__ , state);
	}
}

static void pm8xxx_carkit_work_func(struct work_struct *work)
{
	int state;
	unsigned long flags;

	if (cradle->pdata->hallic_carkit_detect_pin)
		cradle->carkit = !gpio_get_value_cansleep(cradle->pdata->hallic_carkit_detect_pin);

	printk("%s : carkit === > %d \n", __func__ , cradle->carkit);

	spin_lock_irqsave(&cradle->lock, flags);

	if (cradle->carkit == 1)
		state = CARKIT_DOCKED;
	else
		state = CARKIT_NO_DEV;

	if (cradle->deskdock == 1) {
		spin_unlock_irqrestore(&cradle->lock, flags);
		printk("%s : [Cradle] carkit ignored - deskdock connected\n", __func__);
	} else {
		if (cradle->state != state) {
			cradle->state = state;
			spin_unlock_irqrestore(&cradle->lock, flags);
			wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
			switch_set_state(&cradle->sdev, cradle->state);
			printk("%s : [Cradle] carkit value is %d\n", __func__ , state);
		}
		else {
			spin_unlock_irqrestore(&cradle->lock, flags);
			printk("%s : [Cradle] carkit state is %d (no change)\n", __func__, state);
		}
	}
}

void carkit_set_deskdock(int state)
{
	unsigned long flags;
	int new_state;

	if (&cradle->sdev) {
		spin_lock_irqsave(&cradle->lock, flags);
		cradle->deskdock = state;
		if (cradle->deskdock == 0 && cradle->carkit == 1)
			new_state = CARKIT_DOCKED;
		else
			new_state = state;

		if (cradle->state != new_state) {
			cradle->state = new_state;
			spin_unlock_irqrestore(&cradle->lock, flags);
			wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
			switch_set_state(&cradle->sdev, cradle->state);
			printk("%s : [Cradle] deskdock state is %d\n", __func__ , new_state);
		}
		else {
			spin_unlock_irqrestore(&cradle->lock, flags);
			printk("%s : [Cradle] deskdock state is %d (no change)\n", __func__, new_state);
		}
	} else {
		pre_set_flag = state;
	}
}

int carkit_get_deskdock(void)
{
	if (!cradle)
		return pre_set_flag;

	return cradle->deskdock;
}

static irqreturn_t pm8xxx_carkit_irq_handler(int irq, void *handle)
{
	struct pm8xxx_carkit *cradle_handle = handle;
	printk("carkit irq!!!!\n");
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(CARKIT_DETECT_DELAY+5));
	queue_delayed_work(carkit_wq, &cradle_handle->work, msecs_to_jiffies(CARKIT_DETECT_DELAY));
	return IRQ_HANDLED;
}

static ssize_t
carkit_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_carkit *cradle = dev_get_drvdata(dev);
    len = snprintf(buf, PAGE_SIZE, "carkit : %d\n", cradle->carkit);

	return len;
}

static ssize_t
carkit_sensing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_carkit *cradle = dev_get_drvdata(dev);
    len = snprintf(buf, PAGE_SIZE, "sensing(carkit state) : %d\n", cradle->state);

	return len;
}

static struct device_attribute cradle_device_attrs[] = {
	__ATTR(sensing,  S_IRUGO | S_IWUSR, carkit_sensing_show, NULL),
	__ATTR(carkit, S_IRUGO | S_IWUSR, carkit_show, NULL),
};

static ssize_t cradle_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case 0:
		return sprintf(buf, "UNDOCKED\n");
	case 1:
		return sprintf(buf, "DESKDOCK\n");
	case 2:
		return sprintf(buf, "CARKIT\n");
	}
	return -EINVAL;
}

static void bu52014hfv_parse_dt(struct device *dev,
		struct pm8xxx_carkit_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if ((pdata->hallic_carkit_detect_pin = of_get_named_gpio_flags(np, "hallic-carkit-irq-gpio", 0, NULL)) > 0)
		pdata->hallic_carkit_irq = gpio_to_irq(pdata->hallic_carkit_detect_pin);

	printk("[Hall IC] hallic_carkit_gpio: %d", pdata->hallic_carkit_detect_pin);

	pdata->irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
}

static int __devinit pm8xxx_carkit_probe(struct platform_device *pdev)
{
	int ret, i;
	unsigned int hallic_carkit_gpio_irq = 0;

	struct pm8xxx_carkit_platform_data *pdata;

	printk("[Hall IC] carkit probe\n");

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct pm8xxx_carkit_platform_data),
				GFP_KERNEL);
    	if (pdata == NULL) {
	    	pr_err("%s: no pdata\n", __func__);
		   return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;
		bu52014hfv_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}
	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENOMEM;
	}

	cradle = kzalloc(sizeof(*cradle), GFP_KERNEL);
	if (!cradle)
		return -ENOMEM;

	cradle->pdata	= pdata;

	cradle->sdev.name = "dock";
	cradle->sdev.print_name = cradle_print_name;
	cradle->carkit = 0;

	spin_lock_init(&cradle->lock);

	ret = switch_dev_register(&cradle->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	if (pre_set_flag) {
		carkit_set_deskdock(pre_set_flag);
		cradle->state = pre_set_flag;
	}

	wake_lock_init(&cradle->wake_lock, WAKE_LOCK_SUSPEND, "dock_wakeups");

	INIT_DELAYED_WORK(&cradle->work, pm8xxx_carkit_work_func);

	printk("%s : init cradle\n", __func__);

	/* initialize irq of gpio_hall */
	if (cradle->pdata->hallic_carkit_detect_pin > 0) {
		hallic_carkit_gpio_irq = gpio_to_irq(cradle->pdata->hallic_carkit_detect_pin);
		printk("%s : hall_carkit_gpio_irq = [%d]\n", __func__, hallic_carkit_gpio_irq);
		if (hallic_carkit_gpio_irq < 0) {
			printk("Failed : GPIO TO IRQ \n");
			ret = hallic_carkit_gpio_irq;
			goto err_request_irq;
		}
		ret = request_irq(hallic_carkit_gpio_irq, pm8xxx_carkit_irq_handler, pdata->irq_flags, CARKIT_DEV_NAME, cradle);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, hallic_carkit_gpio_irq, ret);
			goto err_request_irq;
		}

		if (enable_irq_wake(hallic_carkit_gpio_irq) == 0)
			printk("%s :enable_irq_wake Enable(3)\n",__func__);
		else
			printk("%s :enable_irq_wake failed(3)\n",__func__);

		printk("%s : pdata->irq_flags = [%d]\n", __func__,(int)pdata->irq_flags);

		printk("%s :boot_carkit_det_func START\n",__func__);
		boot_carkit_det_func();
	}

	for (i = 0; i < ARRAY_SIZE(cradle_device_attrs); i++) {
		ret = device_create_file(&pdev->dev, &cradle_device_attrs[i]);
		if (ret)
			goto err_request_irq;
	}

	platform_set_drvdata(pdev, cradle);
	return 0;

err_request_irq:
	if (hallic_carkit_gpio_irq)
		free_irq(hallic_carkit_gpio_irq, 0);

err_switch_dev_register:
	switch_dev_unregister(&cradle->sdev);
	kfree(cradle);
	return ret;
}

static int __devexit pm8xxx_carkit_remove(struct platform_device *pdev)
{
	struct pm8xxx_carkit *cradle = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&cradle->work);
	switch_dev_unregister(&cradle->sdev);
	platform_set_drvdata(pdev, NULL);
	kfree(cradle);

	return 0;
}

static int pm8xxx_carkit_suspend(struct device *dev)
{
	return 0;
}

static int pm8xxx_carkit_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops pm8xxx_carkit_pm_ops = {
	.suspend = pm8xxx_carkit_suspend,
	.resume = pm8xxx_carkit_resume,
};

#ifdef CONFIG_OF
static struct of_device_id bu52031nvx_match_table[] = {
	{ .compatible = "rohm,hall-bu52031nvx-carkit", },
	{ },
};
#endif

static struct platform_driver pm8xxx_carkit_driver = {
	.probe		= pm8xxx_carkit_probe,
	.remove		= __devexit_p(pm8xxx_carkit_remove),
	.driver		= {
        .name    = CARKIT_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = bu52031nvx_match_table,
#endif
#ifdef CONFIG_PM
		.pm	= &pm8xxx_carkit_pm_ops,
#endif
	},
};

static int __init pm8xxx_carkit_init(void)
{
	printk(KERN_ERR "[Hall IC] carkit init \n");

	carkit_wq = create_singlethread_workqueue("carkit_wq");
	if (!carkit_wq)
		return -ENOMEM;

	return platform_driver_register(&pm8xxx_carkit_driver);
}
module_init(pm8xxx_carkit_init);

static void __exit pm8xxx_carkit_exit(void)
{
	if (carkit_wq)
		destroy_workqueue(carkit_wq);
	platform_driver_unregister(&pm8xxx_carkit_driver);
}
module_exit(pm8xxx_carkit_exit);

MODULE_ALIAS("platform:" CARKIT_DEV_NAME);
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("pm8xxx carkit driver");
MODULE_LICENSE("GPL");
