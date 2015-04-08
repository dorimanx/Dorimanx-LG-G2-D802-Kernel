/*
 * earjack debugger trigger
 *
 * Copyright (C) 2012 LGE, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/of_gpio.h>

#include <mach/gpiomux.h>
#include <mach/board_lge.h>

struct earjack_debugger_device {
	int gpio;
	int irq;
	int saved_detect;
	int (*set_uart_console)(int enable);
};

struct earjack_debugger_platform_data {
	int gpio_trigger;
};

extern int msm_serial_set_uart_console(int enable);

static int earjack_debugger_detected(void *dev)
{
//                                                                                                                                                                                                
#if defined (CONFIG_MACH_MSM8974_VU3_KR) || defined (CONFIG_MACH_MSM8974_Z_US) || defined(CONFIG_MACH_MSM8974_Z_CA)
	printk("earjack debugger is detected on VU3 but do not allow to use it!");
	return 0;
#else
	struct earjack_debugger_device *adev = dev;
	return !!gpio_get_value(adev->gpio);
#endif
//                                                                                                                                                                                              
}

static irqreturn_t earjack_debugger_irq_handler(int irq, void *_dev)
{
	struct earjack_debugger_device *adev = _dev;
	unsigned int um = lge_get_uart_mode();
	int detect;

	/*             
                                                        
                                    
  */
	msleep(400);
	/*              */

	detect = earjack_debugger_detected(adev);


	if (detect) {
		pr_debug("%s() : in!!\n", __func__);
		if (um & UART_MODE_INIT_BMSK) {
			adev->set_uart_console(1);
		} else {
			lge_set_uart_mode(um | UART_MODE_EN_BMSK);
		}
	} else {
		pr_debug("%s() : out!!\n", __func__);
		if (um & UART_MODE_INIT_BMSK) {
			adev->set_uart_console(0);
		} else {
			lge_set_uart_mode(um & ~UART_MODE_EN_BMSK);
		}
	}

	return IRQ_HANDLED;
}

static void earjack_debugger_parse_dt(struct device *dev,
		struct earjack_debugger_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->gpio_trigger = of_get_named_gpio_flags(np, "serial,irq-gpio", 0, NULL);
}

static int __devinit earjack_debugger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct earjack_debugger_device *adev;
	struct earjack_debugger_platform_data *pdata;
	unsigned int um = 0;


	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct earjack_debugger_platform_data),
				GFP_KERNEL);
		if (pdata == NULL) {
			pr_err("%s: no pdata\n", __func__);
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;
		earjack_debugger_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}
	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENOMEM;
	}

	adev = kzalloc(sizeof(struct earjack_debugger_device), GFP_KERNEL);
	if (!adev) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	adev->gpio = pdata->gpio_trigger;
	adev->irq = gpio_to_irq(pdata->gpio_trigger);
	adev->set_uart_console = msm_serial_set_uart_console;

	platform_set_drvdata(pdev, adev);

	ret = gpio_request_one(adev->gpio, GPIOF_IN,
			"gpio_earjack_debugger");
	if (ret < 0) {
		pr_err("%s: failed to request gpio %d\n", __func__,
				adev->gpio);
		goto err_gpio_request;
	}

	ret = request_threaded_irq(adev->irq, NULL, earjack_debugger_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"earjack_debugger_trigger", adev);
	if (ret < 0) {
		pr_err("%s: failed to request irq\n", __func__);
		goto err_request_irq;
	}

	um = lge_get_uart_mode();
	if (earjack_debugger_detected(adev))
		adev->set_uart_console(1);

	pr_info("earjack debugger probed\n");

	return ret;

err_request_irq:
	gpio_free(adev->gpio);
err_gpio_request:
	kfree(adev);

	return ret;
}

static int __devexit earjack_debugger_remove(struct platform_device *pdev)
{
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	free_irq(adev->irq, adev);
	gpio_free(adev->gpio);
	kfree(adev);

	return 0;
}

static void earjack_debugger_shutdown(struct platform_device *pdev)
{
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	disable_irq(adev->irq);
}

static int earjack_debugger_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	disable_irq(adev->irq);

	return 0;
}

static int earjack_debugger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	enable_irq(adev->irq);

	return 0;
}

static const struct dev_pm_ops earjack_debugger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(earjack_debugger_suspend,
			earjack_debugger_resume)
};

#ifdef CONFIG_OF
static struct of_device_id earjack_debugger_match_table[] = {
	{ .compatible = "serial,earjack-debugger", },
	{ },
};
#endif

static struct platform_driver earjack_debugger_driver = {
	.probe = earjack_debugger_probe,
	.remove = __devexit_p(earjack_debugger_remove),
	.shutdown = earjack_debugger_shutdown,
	.driver = {
		.name = "earjack-debugger",
		.pm = &earjack_debugger_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = earjack_debugger_match_table,
#endif
	},
};

static int __init earjack_debugger_init(void)
{
	if (lge_get_uart_mode() &
			(UART_MODE_ALWAYS_ON_BMSK | UART_MODE_ALWAYS_OFF_BMSK))
		return -ENODEV;

	return platform_driver_register(&earjack_debugger_driver);
}

static void __exit earjack_debugger_exit(void)
{
	platform_driver_unregister(&earjack_debugger_driver);
}

module_init(earjack_debugger_init);
module_exit(earjack_debugger_exit);

