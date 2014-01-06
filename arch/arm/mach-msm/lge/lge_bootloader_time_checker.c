/*
 * arch/arm/mach-msm/lge/lge_bootloader_time_checker.c
 *
 * Copyright (C) 2013 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <mach/board_lge.h>


#define MODULE_NAME "bootloader_time_checker"

static int dummy_arg;

struct bootloader_time_checker_data {
	uint32_t sbl_time;
	uint32_t lk_time;
};

static struct bootloader_time_checker_data *data;

static int read_sbl_time(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%d", data->sbl_time);
}
module_param_call(sbl_time, NULL, read_sbl_time, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int read_lk_time(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%d", data->lk_time);
}
module_param_call(lk_time, NULL, read_lk_time, &dummy_arg,
		S_IWUSR | S_IRUGO);


static int bootloader_time_checker_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint64_t temp;

	data = kmalloc(sizeof(struct bootloader_time_checker_data),
			GFP_KERNEL);

	get_dt_cn_prop_u32("lge,sbl_delta_time", &data->sbl_time);
	if (data->sbl_time < 0) {
		pr_err("Wroing SBL Time. Please check 'lge,sbl_delta_time' "
				"property\n");
		data->sbl_time = 0;
	}

	get_dt_cn_prop_u64("lge,lk_delta_time", &temp);
	data->lk_time = (uint32_t)temp;
	if (data->lk_time < 0) {
		pr_err("Wroing LK  Time. Please check 'lge,lk_delta_time' "
				"property\n");
		data->lk_time = 0;
	}

	return ret;
}

static int __devexit
bootloader_time_checker_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver bootloader_time_checker_driver = {
	.probe = bootloader_time_checker_probe,
	.remove = __devexit_p(bootloader_time_checker_remove),
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

static struct platform_device bootloader_time_checker_device = {
	.name = MODULE_NAME,
	.dev = {
		.platform_data = NULL,
	}
};

static int __init bootloader_time_checker_init(void)
{
	platform_device_register(&bootloader_time_checker_device);

	return platform_driver_register(&bootloader_time_checker_driver);
}

static void __exit bootloader_time_checker_exit(void)
{
	kfree(data);
	platform_driver_unregister(&bootloader_time_checker_driver);
}

module_init(bootloader_time_checker_init);
module_exit(bootloader_time_checker_exit);

MODULE_DESCRIPTION("LGE bootloader time checker");
MODULE_AUTHOR("SangWoo Park <sangwoo2.park@lge.com>");
MODULE_LICENSE("GPL");
