/*
 * arch/arm/mach-msm/lge/lge_gpio_debug.c
 *
 * Copyright (C) 2013 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/spmi.h>

#include <mach/msm_iomap.h>
#include <mach/gpiomux.h>

#include <mach/board_lge.h>

/* MSM GPIOs */
#define GPIO_CONFIG(gpio)        (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))
#define GPIO_IN_OUT(gpio)        (MSM_TLMM_BASE + 0x1004 + (0x10 * (gpio)))
#define GPIO_INTR_CFG(gpio)      (MSM_TLMM_BASE + 0x1008 + (0x10 * (gpio)))
#define GPIO_INTR_STATUS(gpio)   (MSM_TLMM_BASE + 0x100c + (0x10 * (gpio)))
#define GPIO_DIR_CONN_INTR(intr) (MSM_TLMM_BASE + 0x2800 + (0x04 * (intr)))

static int n_msm_gpio = 0x92;
static char *pull[] = {"NO_PULL", "PULL_DOWN", "KEEPER", "PULL_UP"};

/* PMIC GPIOs */
#define PMIC_GPIO_REG            0xC004
#define PMIC_GPIO_MODE(gpio)     (PMIC_GPIO_REG + 0x3C + (0x100 * (gpio-1)))
#define PMIC_GPIO_INT(gpio)      (PMIC_GPIO_REG + 0x11 + (0x100 * (gpio-1)))

static int n_pmic_gpio = 0x24;
static char *gpio_mode[] = {"IN", "OUT", "IN/OUT", "Reserved"};
static char *gpio_pull[] = {"PULL_UP_30uA", "PULL_UP_1.5uA", "PULL_UP_31.5uA",
	"PULL_UP_1.5uA+30uA","PULL_DOWN_10uA", "NO_PULL", "Reserved", "Reserved"};
static char *gpio_out[] = {"CMOS", "NMOS", "PMOS", "N/A"};
static char *gpio_drv[] = {"Reserved", "Low", "Medium", "High"};

/* PMIC MPPs */
#define PMIC_MPP_REG             0xA004
#define PMIC_MPP_MODE(gpio)      (PMIC_MPP_REG + 0x3C + (0x100 * (gpio-1)))
#define PMIC_MPP_INT(gpio)       (PMIC_MPP_REG + 0x11 + (0x100 * (gpio-1)))

static int n_pmic_mpp = 0x8;
static char *mpp_mode[] = {"D_IN", "D_OUT", "D_IN/OUT",
	"Bidirection", "A_IN", "A_OUT", "Current Sink", "Reserved"};
static char *mpp_pull[] = {"0.6kohm", "10 kohm", "30 kohm", "Open"};

static DEFINE_SPINLOCK(gpios_lock);
static struct dentry *debugfs_base;
static u32 debug_suspend;

void gpio_debug_print(void)
{
	unsigned cfg;
	unsigned out;
	unsigned intr;
	unsigned char d[6];
	unsigned long flags;
	int i = 0;
	struct spmi_controller *ctrl = spmi_busnum_to_ctrl(0);

	if (likely(!debug_suspend))
		return;

	spin_lock_irqsave(&gpios_lock, flags);

	/* MSM GPIOs */
	pr_cont("MSM GPIOs:\n");

	for (i = 0; i < n_msm_gpio; i++) {
		cfg = __raw_readl(GPIO_CONFIG(i));
		out = __raw_readl(GPIO_IN_OUT(i));
		intr = __raw_readl(GPIO_INTR_STATUS(i));

		pr_cont("GPIO[%d]: [FS]0x%x, [DIR]%s, [PULL]%s, [DRV]%dmA",
					i, (cfg&0x3C)>>2, ((cfg&0x200)>>9) ? "OUT" : "IN",
						pull[(cfg&0x3)], (((cfg&0x1C0)>>6)<<1)+2);

		if ((cfg&0x200)>>9)
			pr_cont(", [VAL]%s", ((out>>1)&0x1) ? "HIGH" : "LOW");

		if (intr&0x1)
			pr_cont(", [INT]HIGH");

		pr_cont("\n");
	}

	/* PMIC GPIOs */
	pr_cont("PMIC GPIOs:\n");

	for(i = 1; i < n_pmic_gpio+1; i++) {
		spmi_ext_register_readl(ctrl, 0, PMIC_GPIO_MODE(i), d, 6);

		pr_cont("GPIO[%d]: [DIR]%s, [PULL]%s, [OUT]%s, [DRV]%s",
					i, gpio_mode[(d[0]&0x70)>>4], gpio_pull[d[2]&0x7],
						gpio_out[d[5]&0x30>>4], gpio_drv[d[5]&0x3]);

		spmi_ext_register_readl(ctrl, 0, PMIC_GPIO_INT(i), d, 1);

		if (d[0])
			pr_cont(", [INT]Enable");

		pr_cont("\n");
	}

	/* PMIC MPPs */
	pr_cont("PMIC MPPs:\n");

	for(i = 1; i < n_pmic_mpp+1; i++) {
		spmi_ext_register_readl(ctrl, 0, PMIC_MPP_MODE(i), d, 3);

		pr_cont("MPP[%d]: [DIR]%s, [PULL]%s",
					i, mpp_mode[(d[0]&0x70)>>4], mpp_pull[d[2]&0x7]);

		spmi_ext_register_readl(ctrl, 0, PMIC_MPP_INT(i), d, 1);

		if (d[0])
			pr_cont(", [INT]Enable");

		pr_cont("\n");
	}

	spin_unlock_irqrestore(&gpios_lock, flags);

	return;
}

int __init gpio_debug_init(void)
{
	debugfs_base = debugfs_create_dir("gpios", NULL);

	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_u32("debug_suspend", S_IRUGO | S_IWUSR,
				debugfs_base, &debug_suspend)) {
		debugfs_remove_recursive(debugfs_base);
		return -ENOMEM;
	}

	return 0;
}
late_initcall(gpio_debug_init);
