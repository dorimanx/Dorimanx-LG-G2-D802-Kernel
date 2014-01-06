/* Copywrite(c) 2011-2012, LGE. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/cpumask.h>
#include <linux/module.h>

static int eco_cpu_num = 4;

static int run_cpu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", num_online_cpus());
}

static int run_cpu_store(struct device *dev, struct device_attribute *attr, const char * buf, size_t count)
{
	return count;
}
DEVICE_ATTR(run_cpu, 0774, run_cpu_show, run_cpu_store);

static int eco_cpu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", eco_cpu_num);
}

static int eco_cpu_store(struct device *dev, struct device_attribute *attr, const char * buf, size_t count)
{
	if (buf[0] > 0x34 || buf[0] < 0x30) {
		printk("%s : cpus_nums should be between 1~4.\n", __func__);
		return count;
	}

	eco_cpu_num = buf[0] - 0x30;
	printk("%s : %s %d\n", __func__, buf, eco_cpu_num);

	kobject_uevent(&dev->kobj, KOBJ_CHANGE);

	return count;
}
DEVICE_ATTR(eco_cpu, 0664, eco_cpu_show, eco_cpu_store);

static int lge_kernel_driver_probe(struct platform_device *pdev)
{
	int err;

	eco_cpu_num = num_possible_cpus();

	err = device_create_file(&pdev->dev, &dev_attr_run_cpu);
	if (err < 0)
		printk("%s : Cannot create the sysfs\n", __func__);

	err = device_create_file(&pdev->dev, &dev_attr_eco_cpu);
	if (err < 0)
		printk("%s : Cannot create the sysfs\n", __func__);

	return 0;
}

static int lge_kernel_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver lge_kernel_driver = {
	.probe = lge_kernel_driver_probe,
	.remove = lge_kernel_driver_remove,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "lge_kernel_driver",
		.owner = THIS_MODULE,
	},
};

static int __init lge_kernel_driver_init(void)
{
	platform_driver_register(&lge_kernel_driver);
	return 0;
}

static void __exit lge_kernel_driver_exit(void)
{
	platform_driver_unregister(&lge_kernel_driver);
}

module_init(lge_kernel_driver_init);

MODULE_DESCRIPTION("LGE KERNEL DRIVER");
MODULE_AUTHOR("TaeHyun Kim <tei.kim@lge.com>");
MODULE_LICENSE("GPL");
