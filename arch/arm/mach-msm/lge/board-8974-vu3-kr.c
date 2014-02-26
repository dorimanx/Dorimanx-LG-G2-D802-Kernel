/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>
#include <mach/msm_bus_board.h>
#include "../board-dt.h"
#include "../clock.h"
#include "../devices.h"
#include "../spm.h"
#include "../modem_notifier.h"
#include "../lpm_resources.h"
#include "../platsmp.h"
#include <mach/board_lge.h>

#if defined(CONFIG_LCD_KCAL)
/* LGE_CHANGE_S
* change code for LCD KCAL
* 2013-05-08, seojin.lee@lge.com
*/
#include <linux/module.h>
#include "../../../../drivers/video/msm/mdss/mdss_fb.h"
extern int update_preset_lcdc_lut(void);
#endif // CONFIG_LCD_KCAL

static struct memtype_reserve msm8974_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm8974_paddr_to_memtype(phys_addr_t paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info msm8974_reserve_info __initdata = {
	.memtype_reserve_table = msm8974_reserve_table,
	.paddr_to_memtype = msm8974_paddr_to_memtype,
};

void __init msm_8974_reserve(void)
{
	reserve_info = &msm8974_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8974_reserve_table);
#ifdef CONFIG_MACH_LGE
	of_scan_flat_dt(lge_init_dt_scan_chosen, NULL);
#endif
	msm_reserve();
	lge_reserve();
}

static void __init msm8974_early_memory(void)
{
	reserve_info = &msm8974_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8974_reserve_table);
}

#ifdef CONFIG_LGE_LCD_TUNING
static struct platform_device lcd_misc_device = {
	.name = "lcd_misc_msm",
	.id = 0,
};

void __init lge_add_lcd_misc_devices(void)
{
	platform_device_register(&lcd_misc_device);
}
#endif

#if defined(CONFIG_LCD_KCAL)
/* LGE_CHANGE_S
* change code for LCD KCAL
* 2013-05-08, seojin.lee@lge.com
*/
extern int g_kcal_r;
extern int g_kcal_g;
extern int g_kcal_b;

int kcal_set_values(int kcal_r, int kcal_g, int kcal_b)
{
    //change RGB Limit Value
		int isUpdate = 0;

		int kcal_r_limit = 0;
		int kcal_g_limit = 0;
		int kcal_b_limit = 0;

		g_kcal_r = kcal_r < kcal_r_limit ? kcal_r_limit : kcal_r;
		g_kcal_g = kcal_g < kcal_g_limit ? kcal_g_limit : kcal_g;
		g_kcal_b = kcal_b < kcal_b_limit ? kcal_b_limit : kcal_b;

		if(kcal_r < kcal_r_limit || kcal_g < kcal_g_limit || kcal_b < kcal_b_limit){
			isUpdate = 1;
		}
		if(isUpdate)
		update_preset_lcdc_lut();

	/*
	g_kcal_r = kcal_r;
	g_kcal_g = kcal_g;
	g_kcal_b = kcal_b;
	*/
	return 0;
}

static int kcal_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	*kcal_r = g_kcal_r;
	*kcal_g = g_kcal_g;
	*kcal_b = g_kcal_b;
	return 0;
}

static int kcal_refresh_values(void)
{
	return update_preset_lcdc_lut();
}

static struct kcal_platform_data kcal_pdata = {
	.set_values = kcal_set_values,
	.get_values = kcal_get_values,
	.refresh_display = kcal_refresh_values
};

static struct platform_device kcal_platrom_device = {
	.name   = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};

void __init lge_add_lcd_kcal_devices(void)
{
	pr_info (" KCAL_DEBUG : %s \n", __func__);
	platform_device_register(&kcal_platrom_device);
}
#endif // CONFIG_LCD_KCAL
/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
/* LGE_CHANGE_S, [WiFi][hayun.kim@lge.com], 2013-01-22, Wifi Bring Up */
#if defined (CONFIG_BCMDHD) || defined (CONFIG_BCMDHD_MODULE)
extern void init_bcm_wifi(void);
#endif
/* LGE_CHANGE_E, [WiFi][hayun.kim@lge.com], 2013-01-22, Wifi Bring Up */
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_VU3EVB)
extern void vu3ebv_init_input(void);
#endif

void __init msm8974_add_drivers(void)
{
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_lpmrs_module_init();
	rpm_regulator_smd_driver_init();
	msm_spm_device_init();
	krait_power_init();
	if (of_board_is_rumi())
		msm_clock_init(&msm8974_rumi_clock_init_data);
	else
		msm_clock_init(&msm8974_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
#ifdef CONFIG_LGE_LCD_TUNING
	lge_add_lcd_misc_devices();
#endif
	lge_add_persistent_device();
#ifdef CONFIG_LGE_QFPROM_INTERFACE
	lge_add_qfprom_devices();
#endif
#ifdef CONFIG_LGE_ECO_MODE
	lge_add_lge_kernel_devices();
#endif
/* LGE_CHANGE_S, [WiFi][hayun.kim@lge.com], 2013-01-22, Wifi Bring Up */
#if defined (CONFIG_BCMDHD) || defined (CONFIG_BCMDHD_MODULE)
	init_bcm_wifi();
#endif
/* LGE_CHANGE_E, [WiFi][hayun.kim@lge.com], 2013-01-22, Wifi Bring Up */
#if defined(CONFIG_LCD_KCAL)
/* LGE_CHANGE_S
* change code for LCD KCAL
* 2013-05-08, seojin.lee@lge.com
*/
	lge_add_lcd_kcal_devices();
#endif // CONFIG_LCD_KCAL

#if defined(CONFIG_LGE_PM_BATTERY_ID_CHECKER)
	lge_battery_id_devices();
#endif
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_VU3EVB)
	vu3ebv_init_input();
#endif
}

static struct of_dev_auxdata msm8974_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,ehci-host", 0xF9A55000, \
			"msm_ehci_host", NULL),
	OF_DEV_AUXDATA("qcom,dwc-usb3-msm", 0xF9200000, \
			"msm_dwc3", NULL),
	OF_DEV_AUXDATA("qcom,usb-bam-msm", 0xF9304000, \
			"usb_bam", NULL),
	OF_DEV_AUXDATA("qcom,spi-qup-v2", 0xF9924000, \
			"spi_qsd.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98E4000, \
			"msm_sdcc.4", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98E4900, \
			"msm_sdcc.4", NULL),
	OF_DEV_AUXDATA("qcom,msm-rng", 0xF9BFF000, \
			"msm_rng", NULL),
	OF_DEV_AUXDATA("qcom,qseecom", 0xFE806000, \
			"qseecom", NULL),
	OF_DEV_AUXDATA("qcom,mdss_mdp", 0xFD900000, "mdp.0", NULL),
	OF_DEV_AUXDATA("qcom,msm-tsens", 0xFC4A8000, \
			"msm-tsens", NULL),
	OF_DEV_AUXDATA("qcom,qcedev", 0xFD440000, \
			"qcedev.0", NULL),
	OF_DEV_AUXDATA("qcom,qcrypto", 0xFD440000, \
			"qcrypto.0", NULL),
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, \
			"msm_hsic_host", NULL),
	{}
};

static void __init msm8974_map_io(void)
{
	msm_map_8974_io();
}

void __init msm8974_init(void)
{
	struct of_dev_auxdata *adata = msm8974_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm_8974_init_gpiomux();
	regulator_has_full_constraints();
	board_dt_populate(adata);
	msm8974_add_drivers();
}

void __init msm8974_init_very_early(void)
{
	msm8974_early_memory();
}

static const char *msm8974_dt_match[] __initconst = {
	"qcom,msm8974",
	"qcom,apq8074",
	NULL
};

DT_MACHINE_START(MSM8974_DT, "Qualcomm MSM 8974 (Flattened Device Tree)")
	.map_io = msm8974_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8974_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8974_dt_match,
	.reserve = msm_8974_reserve,
	.init_very_early = msm8974_init_very_early,
	.restart = msm_restart,
	.smp = &msm8974_smp_ops,
MACHINE_END
