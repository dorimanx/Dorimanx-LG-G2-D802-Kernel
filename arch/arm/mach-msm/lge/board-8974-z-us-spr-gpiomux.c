/* Copyright (c) 2012-2013, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include <mach/board_lge.h>

#define KS8851_IRQ_GPIO 94
/*                                
                                          
 */
int g_is_tlmm_spare_reg_value;

static struct gpiomux_setting ap2mdm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting mdm2ap_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting mdm2ap_errfatal_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting mdm2ap_pblrdy = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};


static struct gpiomux_setting ap2mdm_soft_reset_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ap2mdm_wakeup = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config mdm_configs[] __initdata = {
	/* AP2MDM_STATUS */
	{
		.gpio = 105,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* MDM2AP_STATUS */
	{
		.gpio = 46,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	/* MDM2AP_ERRFATAL */
	{
		.gpio = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	/* AP2MDM_ERRFATAL */
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_SOFT_RESET, aka AP2MDM_PON_RESET_N */
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_soft_reset_cfg,
		}
	},
	/* AP2MDM_WAKEUP */
	{
		.gpio = 104,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_wakeup,
		}
	},
	/* MDM2AP_PBL_READY*/
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_pblrdy,
		}
	},
};

static struct gpiomux_setting gpio_uart_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting slimbus = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_UP,
	.drv = GPIOMUX_DRV_2MA,
	.func = GPIOMUX_FUNC_GPIO,
};

static struct gpiomux_setting gpio_spi_cs2_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting gpio_spi_susp_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_spi_cs1_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm_eth_configs[] = {
	{
		.gpio = KS8851_IRQ_GPIO,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
		}
	},
};
#endif

static struct gpiomux_setting gpio_suspend_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* O-LOW */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
};

static struct gpiomux_setting gpio_epm_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

/*                                                                    */
#if defined(CONFIG_BCMDHD) || defined(CONFIG_BCMDHD_MODULE)
#else
static struct gpiomux_setting wcnss_5wire_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5wire_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif
/*                                                                    */

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	/*
	 * Please keep I2C GPIOs drive-strength at minimum (2ma). It is a
	 * workaround for HW issue of glitches caused by rapid GPIO current-
	 * change.
	 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting lcd_en_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting lcd_en_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

#ifdef CONFIG_MACH_LGE
#else
static struct gpiomux_setting atmel_resout_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting atmel_resout_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

static struct gpiomux_setting taiko_reset = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting taiko_int = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

#ifndef CONFIG_LGE_IRRC
/* NOT USED: GPIO 86 is used as IRRC_RxD in G2 board */
static struct gpiomux_setting hap_lvl_shft_suspended_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hap_lvl_shft_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct msm_gpiomux_config hap_lvl_shft_config[] __initdata = {
	{
		.gpio = 86,
		.settings = {
			[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,
			[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config,
		},
	},
};
#endif

#if defined(CONFIG_BACKLIGHT_LM3630)
static struct gpiomux_setting lcd_bl_en_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting lcd_bl_en_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting lcd_bl_en_suspend_cfg_rev_c = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
#endif

#ifdef CONFIG_MACH_LGE
#ifdef CONFIG_MAX17048_FUELGAUGE
static struct gpiomux_setting max17048_i2c_sda_config = {
	/* GPIO_2 */
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting max17048_i2c_scl_config = {
	/* GPIO_3 */
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting max17048_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.dir = GPIOMUX_IN,
};
#endif
#if 0
/*           
                                                                                                              
                               
 */
static struct gpiomux_setting touch_id_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting touch_id_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.dir = GPIOMUX_IN,
};
#endif

static struct gpiomux_setting touch_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting touch_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting touch_i2c_act_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting touch_i2c_sus_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting touch_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting touch_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting touch_ldoen_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting touch_ldoen_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.dir = GPIOMUX_IN,
};
#endif

static struct msm_gpiomux_config msm_touch_configs[] __initdata = {
	{
		.gpio      = 8,		/* TOUCH RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &touch_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_reset_sus_cfg,
		},
	},
	{
		.gpio      = 5,		/* TOUCH IRQ */
		.settings = {
			[GPIOMUX_ACTIVE] = &touch_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_int_sus_cfg,
		},
	},

};

#ifdef CONFIG_MACH_LGE
static struct gpiomux_setting hsic_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};
#else /* qmc original */
static struct gpiomux_setting hsic_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

#ifndef CONFIG_MACH_LGE /*                                          */
static struct gpiomux_setting hsic_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif

static struct gpiomux_setting hsic_hub_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

#ifndef CONFIG_MACH_LGE
static struct gpiomux_setting hsic_resume_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting hsic_resume_susp_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif

#ifndef CONFIG_MACH_LGE  /*                                          */
static struct msm_gpiomux_config msm_hsic_configs[] = {
	{
		.gpio = 144,               /*HSIC_STROBE */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 145,               /* HSIC_DATA */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_resume_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_resume_susp_cfg,
		},
	},
};
#endif

static struct msm_gpiomux_config msm_hsic_hub_configs[] = {
	{
		.gpio = 50,               /* HSIC_HUB_INT_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_hub_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
};

static struct gpiomux_setting hall_ic_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_hall_ic_configs_rev_a[] = {
	{
		.gpio = 73,
		.settings = {
			[GPIOMUX_ACTIVE] = &hall_ic_act_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_hall_ic_configs_rev_b[] = {
	{
		.gpio = 102,
		.settings = {
			[GPIOMUX_ACTIVE] = &hall_ic_act_cfg,
		},
	},
	{
		.gpio = 144,
		.settings = {
			[GPIOMUX_ACTIVE] = &hall_ic_act_cfg,
		},
	},
};

static struct gpiomux_setting mhl_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mhl_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting hdmi_suspend_1_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_suspend_2_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_2_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

#if defined(CONFIG_MACH_LGE)
#ifdef CONFIG_MAX17048_FUELGAUGE
static struct msm_gpiomux_config msm_fuel_gauge_configs[] __initdata = {
		{
		.gpio      = 2,		/* BLSP1 QUP1 I2C_DAT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &max17048_i2c_sda_config,
			[GPIOMUX_SUSPENDED] = &max17048_i2c_sda_config,
		},
	},
	{
		.gpio      = 3,		/* BLSP1 QUP1 I2C_CLK */
		.settings = {
			[GPIOMUX_ACTIVE]    = &max17048_i2c_scl_config,
			[GPIOMUX_SUSPENDED] = &max17048_i2c_scl_config,
		},
	},
	{
		.gpio      = 9,		/* FUEL_GAUGE_INT_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &max17048_int_config,
			[GPIOMUX_SUSPENDED] = &max17048_int_config,
		},
	},
};
#endif
#endif

static struct msm_gpiomux_config msm_mhl_configs[] __initdata = {
	{
		/* mhl-sii8334 pwr */
		.gpio = 12,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mhl_suspend_config,
			[GPIOMUX_ACTIVE]    = &mhl_active_1_cfg,
		},
	},
	{
		/* mhl-sii8334 intr */
		.gpio = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mhl_suspend_config,
			[GPIOMUX_ACTIVE]    = &mhl_active_1_cfg,
		},
	},
};


static struct msm_gpiomux_config msm_hdmi_configs[] __initdata = {
	{
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_1_cfg,
		},
	},
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_1_cfg,
		},
	},
	{
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_1_cfg,
		},
	},
	{
		.gpio = 34,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_2_cfg,
		},
	},
};

#ifndef CONFIG_MACH_LGE
static struct gpiomux_setting gpio_uart7_active_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_uart7_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_blsp2_uart7_configs[] __initdata = {
	{
		.gpio	= 41,	/* BLSP2 UART7 TX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
	{
		.gpio	= 42,	/* BLSP2 UART7 RX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
	{
		.gpio	= 43,	/* BLSP2 UART7 CTS */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
	{
		.gpio	= 44,	/* BLSP2 UART7 RFR */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
};
#endif

static struct msm_gpiomux_config msm_rumi_blsp_configs[] __initdata = {
	{
		.gpio      = 45,	/* BLSP2 UART8 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 46,	/* BLSP2 UART8 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
};

#if defined(CONFIG_MACH_LGE)
static struct msm_gpiomux_config msm_lcd_configs_rev_b[] __initdata = {
	{
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
#if defined(CONFIG_BACKLIGHT_LM3630)
	{
		.gpio = 91, /* LCD_BL_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_bl_en_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_bl_en_suspend_cfg,
		},
	},
#endif

};

static struct msm_gpiomux_config msm_lcd_configs_rev_c[] __initdata = {
	{
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
#if defined(CONFIG_BACKLIGHT_LM3630)
	{
		.gpio = 49, /* LCD_BL_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_bl_en_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_bl_en_suspend_cfg_rev_c,
		},
	},
#endif

};

static struct msm_gpiomux_config msm_lcd_configs_rev_d[] __initdata = {
	{
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
#if defined(CONFIG_BACKLIGHT_LM3630)
	{
		.gpio = 49, /* LCD_BL_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_bl_en_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_bl_en_suspend_cfg,
		},
	},
#endif

};
#else
static struct msm_gpiomux_config msm_lcd_configs[] __initdata = {
	{
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
#if defined(CONFIG_BACKLIGHT_LM3630)
	{
		.gpio = 91, /* LCD_BL_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_bl_en_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_bl_en_suspend_cfg,
		},
	},
#endif

};
#endif

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	{
		.gpio      = 0,		/* BLSP1 QUP SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 1,		/* BLSP1 QUP SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 3,		/* BLSP1 QUP SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 9,		/* BLSP1 QUP SPI_CS2A_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_cs2_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 8,		/* BLSP1 QUP SPI_CS1_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_cs1_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
#endif

#ifdef CONFIG_MACH_LGE
	{
		.gpio      = 6,		/* BLSP1 QUP2 I2C_DAT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &touch_i2c_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_i2c_sus_cfg,
		},
	},
	{
		.gpio      = 7,		/* BLSP1 QUP2 I2C_CLK */
		.settings = {
			[GPIOMUX_ACTIVE]    = &touch_i2c_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_i2c_sus_cfg,
		},
	},
#else
	{
		.gpio      = 6,		/* BLSP1 QUP2 I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 7,		/* BLSP1 QUP2 I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
#endif
	{
		.gpio      = 83,		/* BLSP11 QUP I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 84,		/* BLSP11 QUP I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
#if 0
/*           
                                                   
                               
 */
#ifdef CONFIG_MACH_LGE
	{
		.gpio      = 4,			/* BLSP2 UART TX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &touch_id_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_id_sus_cfg,
		},
	},
#else
	{
		.gpio      = 4,			/* BLSP2 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
#endif
#endif	/* #if 0 */
#ifdef CONFIG_MACH_LGE
#else
	{
		.gpio      = 5,			/* BLSP2 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
#endif
#ifdef CONFIG_MACH_LGE
	{
		.gpio      = 0,			/* BLSP2 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 1,			/* BLSP2 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
#endif
#ifdef CONFIG_LGE_IRRC
	{
		.gpio	   = 85,		/* BLSP2 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio	   = 86,		/* BLSP2 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
#endif

/*                                                        */
#ifndef CONFIG_LGE_BLUETOOTH
	{
		.gpio      = 53,		/* BLSP2 QUP4 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio      = 54,		/* BLSP2 QUP4 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio      = 56,		/* BLSP2 QUP4 SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio      = 55,		/* BLSP2 QUP4 SPI_CS0_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
#endif /*                      */
/*                                                        */
	{
		.gpio      = 81,		/* EPM enable */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_epm_config,
		},
	},
};

static struct msm_gpiomux_config msm8974_slimbus_config[] __initdata = {
	{
		.gpio	= 70,		/* slimbus clk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
	{
		.gpio	= 71,		/* slimbus data */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
};

#ifdef CONFIG_SND_FM_RADIO

static struct gpiomux_setting  tert_mi2s_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting  tert_mi2s_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#if 0
static struct gpiomux_setting  fm_radio_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting  fm_radio_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif
static struct msm_gpiomux_config msm8974_tert_mi2s_configs[] __initdata = {
	{
		.gpio	= 74,		/*  mi2s sck */
		.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
	{
		.gpio	= 75,
			.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
	{
		.gpio = 76,
		.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
	{
		.gpio = 77,    /* SD1 -Data 1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
};
#if 0
static struct msm_gpiomux_config fm_radio_configs[] =  {
		{
		.gpio	= 69,		/*  mi2s sck */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fm_radio_sus_cfg,
			[GPIOMUX_ACTIVE] = &fm_radio_act_cfg,
		},
},
};
#endif
#endif

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_1, /*active 1*/ /* 0 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*suspend*/ /* 1 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*i2c suspend*/ /* 2 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 0*/ /* 3 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 0*/ /* 4 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
};

#ifdef CONFIG_MACH_LGE
/*                                                                       */
static struct gpiomux_setting sd_card_det_active_config_over_rev_b = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_sleep_config_over_rev_b = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_sleep_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
#else
static struct gpiomux_setting sd_card_det_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_sleep_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
#endif

#ifdef CONFIG_MACH_LGE
/*                                                            */
static struct msm_gpiomux_config sd_card_det __initdata = {
	.gpio = 95,
	.settings = {
		[GPIOMUX_ACTIVE]    = &sd_card_det_active_config_over_rev_b,
		[GPIOMUX_SUSPENDED] = &sd_card_det_sleep_config_over_rev_b,
	},
};
static struct msm_gpiomux_config sd_card_det_under_rev_a __initdata = {
	.gpio = 62,
	.settings = {
		[GPIOMUX_ACTIVE]    = &sd_card_det_active_config,
		[GPIOMUX_SUSPENDED] = &sd_card_det_sleep_config,
	},
};
#else
static struct msm_gpiomux_config sd_card_det __initdata = {
	.gpio = 62,
	.settings = {
		[GPIOMUX_ACTIVE]    = &sd_card_det_active_config,
		[GPIOMUX_SUSPENDED] = &sd_card_det_sleep_config,
	},
};
#endif

#if defined(CONFIG_MACH_LGE)
/*             
                      
                                  
 */
static struct msm_gpiomux_config msm_sensor_configs[] __initdata = {
	{
		.gpio = 15, /* CAM_MCLK0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 16, /* 13M_VANA / CAM_MCLK1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
	{
		.gpio = 17, /* CAM_MCLK2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 18, /* WEBCAM1_RESET_N / CAM_MCLK3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
	{
		.gpio = 19, /* CCI_I2C_SDA0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 20, /* CCI_I2C_SCL0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 21, /* CCI_I2C_SDA1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 22, /* CCI_I2C_SCL1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 23, /* FLASH_LED_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &touch_ldoen_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_ldoen_sus_cfg,
		},
	},
	{
		.gpio = 24, /* FLASH_LED_NOW */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 25, /* WEBCAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 26, /* CAM_IRQ */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
#ifdef CONFIG_MACH_LGE
	/* NULL - GPIO_27 : used with Motor PWM pin
		GPIO_28 : used with SlimPort IRQ pin */
#else /* QCT original */
	{
		.gpio = 27, /* OIS_SYNC */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},

	},
	{
		.gpio = 28, /* WEBCAM1_STANDBY */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
#endif
	{
		.gpio = 89, /* CAM1_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 4, /* CAM1_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
#if defined(CONFIG_BACKLIGHT_LM3630)
#else
	{
		.gpio = 91, /* CAM2_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
#endif

/*                                                       */
#if defined(CONFIG_NFC_BCM2079X)
#else
	{
		.gpio = 92, /* CAM2_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]	= &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
#endif
/*                                                       */

	{
		.gpio = 57, /* 13M_VCM_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
	{
		.gpio = 96, /* 13M_VIO */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
	{
		.gpio = 30, /* VT_LDO_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
};
#endif /*                              */

#ifndef CONFIG_MACH_LGE
static struct gpiomux_setting pri_auxpcm_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};


static struct gpiomux_setting pri_auxpcm_sus_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8974_pri_auxpcm_configs[] __initdata = {
	{
		.gpio = 65,
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_auxpcm_act_cfg,
		},
	},
	{
		.gpio = 66,
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_auxpcm_act_cfg,
		},
	},
	{
		.gpio = 67,
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_auxpcm_act_cfg,
		},
	},
	{
		.gpio = 68,
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_auxpcm_act_cfg,
		},
	},
};
#endif

/*                                                                    */
#if defined(CONFIG_BCMDHD) || defined(CONFIG_BCMDHD_MODULE)
#else
static struct msm_gpiomux_config wcnss_5wire_interface[] = {
	{
		.gpio = 36,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 37,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 38,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 39,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 40,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
};
#endif
/*                                                                    */

static struct msm_gpiomux_config msm_taiko_config[] __initdata = {
	{
		.gpio	= 63,		/* SYS_RST_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &taiko_reset,
		},
	},
	{
		.gpio	= 72,		/* CDC_INT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &taiko_int,
		},
	},
};
#ifdef CONFIG_SLIMPORT_ANX7808
static struct gpiomux_setting slimport_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config slimport_configs[] __initdata = {
	{
		.gpio      = 28,		/* SLIMPORT IRQ */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimport_int_sus_cfg,
		},
	},

};
#endif

#ifdef CONFIG_MACH_LGE
static struct gpiomux_setting headset_active_cfg_gpio65  = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir  = GPIOMUX_IN,
};

static struct gpiomux_setting headset_active_cfg_gpio64  = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir  = GPIOMUX_IN,
};

static struct msm_gpiomux_config headset_configs[]  = {
	{
		.gpio = 64,
		.settings = {
			[GPIOMUX_ACTIVE]    = &headset_active_cfg_gpio64,
		},
	},

	{
		.gpio = 65,
		.settings = {
			[GPIOMUX_ACTIVE]    = &headset_active_cfg_gpio65,
		},
	},
};

/* sensor GPIO setting for LGPS11, TODO: Need change APDS config*/
#if 0  /*sensor disable */
static struct gpiomux_setting sensor_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir  = GPIOMUX_IN,
};

static struct gpiomux_setting sensor_en_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir  = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config sensor_configs[] __initdata = {
	{
		.gpio      = 87,    /* BLSP12 QUP I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 88,    /* BLSP12 QUP I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 10,    /* BLSP3 QUP I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 11,    /* BLSP3 QUP I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 3,    /* GYRO_DATA_EN */
		.settings = {
			[GPIOMUX_ACTIVE] = &sensor_en_config,
			[GPIOMUX_SUSPENDED] = &sensor_en_config,
		},
	},
	{
		.gpio      = 65,    /*ACCL_INT2 */
		.settings = {
			[GPIOMUX_ACTIVE] = &sensor_int_config,
			[GPIOMUX_SUSPENDED] = &sensor_int_config,
		},
	},
	{
		.gpio      = 66,    /* GYRO_INT2*/
		.settings = {
			[GPIOMUX_ACTIVE] = &sensor_int_config,
			[GPIOMUX_SUSPENDED] = &sensor_int_config,
		},
	},
/*
	{
		.gpio      = 67,    //TODO : COMPASS_DRDY
		.settings = {
			[GPIOMUX_ACTIVE] = &sensor_int_config,
			[GPIOMUX_SUSPENDED] = &sensor_int_config,
		},
	},
*/
	{
		.gpio      = 73,    /* ACCL_INT1*/
		.settings = {
			[GPIOMUX_ACTIVE] = &sensor_int_config,
			[GPIOMUX_SUSPENDED] = &sensor_int_config,
		},
	},
	{
		.gpio      = 74,    /* PROXIMITY_INT */
		.settings = {
			[GPIOMUX_ACTIVE] = &sensor_int_config,
			[GPIOMUX_SUSPENDED] = &sensor_int_config,
		},
	},
	{
		.gpio      = 102,    /* GYRO_int2 (DRDY)*/
		.settings = {
			[GPIOMUX_ACTIVE] = &sensor_int_config,
			[GPIOMUX_SUSPENDED] = &sensor_int_config,
		},
	},

};
#endif
#endif

#if defined(CONFIG_LGE_SM100) || defined(CONFIG_TSPDRV)
static struct gpiomux_setting vibrator_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting vibrator_active_cfg_gpio27 = {
	.func = GPIOMUX_FUNC_6,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting vibrator_active_cfg_gpio60 = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config vibrator_configs[] = {
	{
		.gpio = 27,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_active_cfg_gpio27,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
	{
		.gpio = 60,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_active_cfg_gpio60,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
};
#endif

static struct gpiomux_setting sdc3_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc3_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc3_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

/*                                      
                                                     
                                                                    
*/
/*
static struct gpiomux_setting sdc3_data_1_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};
*/
static struct msm_gpiomux_config msm8974_sdc3_configs[] __initdata = {
	{
		/* DAT3 */
		.gpio      = 35,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* DAT2 */
		.gpio      = 36,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* DAT1 */
		.gpio      = 37,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg, /* sdc3_data_1_suspend_cfg */
		},
	},
	{
		/* DAT0 */
		.gpio      = 38,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* CMD */
		.gpio      = 39,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* CLK */
		.gpio      = 40,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
};

static void msm_gpiomux_sdc3_install(void)
{
	msm_gpiomux_install(msm8974_sdc3_configs,
			    ARRAY_SIZE(msm8974_sdc3_configs));
}

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct gpiomux_setting sdc4_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc4_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc4_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc4_data_1_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm8974_sdc4_configs[] __initdata = {
/*                                                       */
#if defined(CONFIG_NFC_BCM2079X)
#else
	{
		/* DAT3 */
		.gpio	   = 92,
		.settings = {
			[GPIOMUX_ACTIVE]	= &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
#endif
/*                                                       */

	{
		/* DAT2 */
		.gpio      = 94,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		/* DAT1 */
		.gpio      = 95,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_data_1_suspend_cfg,
		},
	},
#if 0	/* Camera will use gpio 96 */
/*             
                      
                                  
 */
	{
		/* DAT0 */
		.gpio      = 96,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
#endif
	{
		/* CMD */
		.gpio      = 91,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		/* CLK */
		.gpio      = 93,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
};

static void msm_gpiomux_sdc4_install(void)
{
/*             
                      
                                  
 */
		msm_gpiomux_install(msm8974_sdc4_configs,
					    ARRAY_SIZE(msm8974_sdc4_configs));
}
#else
static void msm_gpiomux_sdc4_install(void) {}
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

/*                                                        */
#ifdef CONFIG_LGE_BLUETOOTH
static struct gpiomux_setting bt_gpio_uart_active_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE, /* Should be PULL NONE */
};

static struct gpiomux_setting bt_gpio_uart_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE, /* PULL Configuration */
};

static struct gpiomux_setting bt_rfkill_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_rfkill_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_host_wakeup_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting bt_host_wakeup_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting bt_wakeup_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_wakeup_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_pcm_active_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_pcm_suspend_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config bt_msm_blsp_configs[] __initdata = {
	{
		.gpio = 53, /* BLSP2 UART10 TX */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
	{
		.gpio = 54, /* BLSP2 UART10 RX */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
	{
		.gpio = 55, /* BLSP2 UART10 CTS */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
	{
		.gpio = 56, /* BLSP2 UART10 RFR */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
};

static struct msm_gpiomux_config bt_rfkill_configs[] = {
	{
		.gpio = 41,
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_rfkill_active_config,
			[GPIOMUX_SUSPENDED] = &bt_rfkill_suspend_config,
		},
	},
};

static struct msm_gpiomux_config bt_host_wakeup_configs[] __initdata = {
	{
		.gpio = 42,
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_host_wakeup_active_config,
			[GPIOMUX_SUSPENDED] = &bt_host_wakeup_suspend_config,
		},
	},
};

static struct msm_gpiomux_config bt_wakeup_configs[] __initdata = {
	{
		.gpio = 62,
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_wakeup_active_config,
			[GPIOMUX_SUSPENDED] = &bt_wakeup_suspend_config,
		},
	},
};

static struct msm_gpiomux_config bt_pcm_configs[] __initdata = {
	{
		.gpio	   = 79,	/* BT_PCM_CLK */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	},
	{
		.gpio	   = 80,	/* BT_PCM_SYNC */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	},
	{
		.gpio	   = 81,	/* BT_PCM_DIN */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	},
	{
		.gpio	   = 82,	/* BT_PCM_DOUT */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	}
};

static void bluetooth_msm_gpiomux_install(void)
{
    /* UART */
	msm_gpiomux_install(bt_msm_blsp_configs, ARRAY_SIZE(bt_msm_blsp_configs));

    /* RFKILL */
	msm_gpiomux_install(bt_rfkill_configs, ARRAY_SIZE(bt_rfkill_configs));

    /* HOST WAKE-UP */
	msm_gpiomux_install(bt_host_wakeup_configs, ARRAY_SIZE(bt_host_wakeup_configs));

    /* BT WAKE-UP */
	msm_gpiomux_install(bt_wakeup_configs, ARRAY_SIZE(bt_wakeup_configs));

    /* PCM I/F */
	msm_gpiomux_install(bt_pcm_configs, ARRAY_SIZE(bt_pcm_configs));
}
#endif /*                      */
/*                                                        */

/*                                                                            */
#if defined(CONFIG_LGE_BROADCAST_TDMB)
static struct gpiomux_setting gpio_blsp8_spi_active_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_blsp8_spi_suspended_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_broadcast_ctrl_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_broadcast_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm8974_tdmb_configs[] __initdata = {
	{
		.gpio	   = 45,		/* BLSP 2 QUP 1 (BLSP8) SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_blsp8_spi_active_config,
			[GPIOMUX_SUSPENDED] = &gpio_blsp8_spi_suspended_config,
		},
	},
	{
		.gpio	   = 46,		/* BLSP 2 QUP 1 (BLSP8) SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_blsp8_spi_active_config,
			[GPIOMUX_SUSPENDED] = &gpio_blsp8_spi_suspended_config,
		},
	},
	{
		.gpio	   = 47,		/* BLSP 2 QUP 1 (BLSP8) SPI_CS_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_blsp8_spi_active_config,
			[GPIOMUX_SUSPENDED] = &gpio_blsp8_spi_suspended_config,
		},
	},
	{
		.gpio	   = 48,		/* BLSP 2 QUP 1 (BLSP8) SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_blsp8_spi_active_config,
			[GPIOMUX_SUSPENDED] = &gpio_blsp8_spi_suspended_config,
		},
	},
	{
		.gpio	   = 75,		/* DNB_RESET */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_broadcast_ctrl_config,
		},
	},
	{
		.gpio	   = 76,		/* DNB_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_broadcast_ctrl_config,
		},
	},
	{
		.gpio	   = 77,		/* DNB_INT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_broadcast_int_config,
		},
	},
};
#endif /*               */
/*                                                                            */
/*                                                                     */
#ifdef CONFIG_NFC_BCM2079X
static struct gpiomux_setting nfc_bcm2079x_sda_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting nfc_bcm2079x_scl_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting nfc_bcm2079x_ven_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting nfc_bcm2079x_irq_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting nfc_bcm2079x_mode_cfg = {	/* WAKE */
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config msm8974_nfc_configs[] __initdata = {
	{
		/* I2C SDA */
		.gpio      = 2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_bcm2079x_sda_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm2079x_sda_cfg,
		},
	},
	{
		/* I2C SCL */
		.gpio      = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_bcm2079x_scl_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm2079x_scl_cfg,
		},
	},
	{
		/* VEN */
		.gpio      = 94,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_bcm2079x_ven_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm2079x_ven_cfg,
		},
	},
	{
		/* IRQ */
		.gpio      = 59,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_bcm2079x_irq_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm2079x_irq_cfg,
		},
	},
	{
		/* MODE *//* WAKE */
		.gpio      = 92,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_bcm2079x_mode_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm2079x_mode_cfg,
		},
	},
};
#endif
/*                                                                     */
#ifdef CONFIG_LGE_NFC_PN544_C3
static struct gpiomux_setting nfc_pn544_sda_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting nfc_pn544_scl_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting nfc_pn544_ven_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting nfc_pn544_irq_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting nfc_pn544_mode_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config msm8974_nfc_configs[] __initdata = {
	{
		/* I2C SDA */
		.gpio      = 83,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_pn544_sda_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_pn544_sda_cfg,
		},
	},
	{
		/* I2C SCL */
		.gpio      = 84,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_pn544_scl_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_pn544_scl_cfg,
		},
	},
	{
		/* VEN */
		.gpio      = 94,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_pn544_ven_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_pn544_ven_cfg,
		},
	},
	{
		/* IRQ */
		.gpio      = 59,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_pn544_irq_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_pn544_irq_cfg,
		},
	},
	{
		/* MODE *//* WAKE */
		.gpio      = 95,
		.settings = {
			[GPIOMUX_ACTIVE]    = &nfc_pn544_mode_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_pn544_mode_cfg,
		},
	},
};
#endif
/*                                                                     */
/*                                                         */
#if defined(CONFIG_BQ51051B_CHARGER)
static struct gpiomux_setting wlc_track_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting wlc_track_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config wlc_charger_configs[] __initdata = {
	{
		.gpio = 44,
		.settings = {
			[GPIOMUX_ACTIVE] = &wlc_track_act_cfg,
			[GPIOMUX_SUSPENDED] = &wlc_track_sus_cfg,
		},
	},
};
#endif
/*                                                         */

#if defined(CONFIG_USB_LGE_USB3_REDRIVER)
static struct gpiomux_setting usb3_rd_en_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config usb3_rd_en_configs[]  = {
	{
		.gpio = 89, /* USB3_RD_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &usb3_rd_en_cfg,
			[GPIOMUX_SUSPENDED] = &usb3_rd_en_cfg,
		},
	},
};
#endif

#if defined(CONFIG_BQ24192_CHARGER)
static struct gpiomux_setting bq24192_chg_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting bq24192_chg_en_n_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config bq_chg_int_configs[] __initdata = {
	{
		.gpio	   = 24,	/* CHG_INT_BQ */
		.settings = {
			[GPIOMUX_ACTIVE]    = &bq24192_chg_int_config,
			[GPIOMUX_SUSPENDED] = &bq24192_chg_int_config,
		},
	},
};
static struct msm_gpiomux_config bq_chg_en_n_configs[] __initdata = {
	{
		.gpio	   = 23,	/* CHG_EN_N_BQ (rev.d = 89)*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &bq24192_chg_en_n_config,
			[GPIOMUX_SUSPENDED] = &bq24192_chg_en_n_config,
		},
	},
};
#endif

static struct msm_gpiomux_config apq8074_dragonboard_ts_config[] __initdata = {
	{
		/* BLSP1 QUP I2C_DATA */
		.gpio      = 2,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		/* BLSP1 QUP I2C_CLK */
		.gpio      = 3,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
};

void __init msm_8974_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	/*                                
                                           
  */
	if (socinfo_get_version() >= 0x20000) {
		g_is_tlmm_spare_reg_value = 0x7;
		msm_tlmm_misc_reg_write(TLMM_SPARE_REG, 0x7);
	}

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	msm_gpiomux_install(msm_eth_configs, ARRAY_SIZE(msm_eth_configs));
#endif
	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));

#ifndef CONFIG_MACH_LGE
	msm_gpiomux_install(msm_blsp2_uart7_configs,
			ARRAY_SIZE(msm_blsp2_uart7_configs));
#endif

#if defined(CONFIG_MACH_LGE)
#ifdef CONFIG_MAX17048_FUELGAUGE
	/*                           
                                                */
	if (HW_REV_A <= lge_get_board_revno()) {
		msm_gpiomux_install(msm_fuel_gauge_configs,
				ARRAY_SIZE(msm_fuel_gauge_configs));
	}
#endif
#endif
	/*                                                                    */
#if defined(CONFIG_BCMDHD) || defined(CONFIG_BCMDHD_MODULE)
#else
	msm_gpiomux_install(wcnss_5wire_interface,
			ARRAY_SIZE(wcnss_5wire_interface));
#endif
	/*                                                                    */

	msm_gpiomux_install(msm8974_slimbus_config,
			ARRAY_SIZE(msm8974_slimbus_config));

	msm_gpiomux_install(msm_touch_configs, ARRAY_SIZE(msm_touch_configs));

#ifndef CONFIG_LGE_IRRC
	msm_gpiomux_install(hap_lvl_shft_config,
			ARRAY_SIZE(hap_lvl_shft_config));
#endif

#ifndef CONFIG_MACH_LGE
	msm_gpiomux_install(msm_sensor_configs, ARRAY_SIZE(msm_sensor_configs));
#endif

#ifdef CONFIG_MACH_LGE
	/*                                                            */
	if (HW_REV_A < lge_get_board_revno())
		msm_gpiomux_install(&sd_card_det, 1);
	else
		msm_gpiomux_install(&sd_card_det_under_rev_a, 1);
#else
	msm_gpiomux_install(&sd_card_det, 1);
#endif

	if (machine_is_apq8074() && (of_board_is_liquid() || \
				of_board_is_dragonboard()))
		msm_gpiomux_sdc3_install();

	msm_gpiomux_sdc4_install();

	msm_gpiomux_install(msm_taiko_config, ARRAY_SIZE(msm_taiko_config));

#ifndef CONFIG_MACH_LGE
	msm_gpiomux_install(msm_hsic_configs, ARRAY_SIZE(msm_hsic_configs));
#endif
	msm_gpiomux_install(msm_hsic_hub_configs,
			ARRAY_SIZE(msm_hsic_hub_configs));

	msm_gpiomux_install(msm_hdmi_configs, ARRAY_SIZE(msm_hdmi_configs));
	if (of_board_is_fluid())
		msm_gpiomux_install(msm_mhl_configs,
				ARRAY_SIZE(msm_mhl_configs));
#ifndef CONFIG_MACH_LGE
	msm_gpiomux_install(msm8974_pri_auxpcm_configs,
			ARRAY_SIZE(msm8974_pri_auxpcm_configs));
#endif

#if defined(CONFIG_MACH_LGE)
	if (lge_get_board_revno() < HW_REV_C)
		msm_gpiomux_install_nowrite(msm_lcd_configs_rev_b, ARRAY_SIZE(msm_lcd_configs_rev_b));
	else if (lge_get_board_revno() == HW_REV_C)
		msm_gpiomux_install_nowrite(msm_lcd_configs_rev_c, ARRAY_SIZE(msm_lcd_configs_rev_c));
	else
		msm_gpiomux_install_nowrite(msm_lcd_configs_rev_d, ARRAY_SIZE(msm_lcd_configs_rev_d));
#else
	msm_gpiomux_install_nowrite(msm_lcd_configs, ARRAY_SIZE(msm_lcd_configs));
#endif

	if (of_board_is_rumi())
		msm_gpiomux_install(msm_rumi_blsp_configs,
				ARRAY_SIZE(msm_rumi_blsp_configs));

	if (socinfo_get_platform_subtype() == PLATFORM_SUBTYPE_MDM)
		msm_gpiomux_install(mdm_configs,
				ARRAY_SIZE(mdm_configs));

#ifdef CONFIG_SLIMPORT_ANX7808
	msm_gpiomux_install(slimport_configs,
			ARRAY_SIZE(slimport_configs));
#endif
#if defined(CONFIG_MACH_LGE)
	/* if (lge_get_board_revno() > HW_REV_A) */
	msm_gpiomux_install(headset_configs, ARRAY_SIZE(headset_configs));
	/* msm_gpiomux_install(msm_display_configs, ARRAY_SIZE(msm_display_configs)); */
#endif
#if 0 /* disable sensor GPIO setting (enable ADSP)*/
	msm_gpiomux_install(sensor_configs, ARRAY_SIZE(sensor_configs));
#endif
#if defined(CONFIG_LGE_SM100) || defined(CONFIG_TSPDRV)
	msm_gpiomux_install(vibrator_configs, ARRAY_SIZE(vibrator_configs));
#endif

	/*                                                        */
#ifdef CONFIG_LGE_BLUETOOTH
	bluetooth_msm_gpiomux_install();
#endif /*                      */
	/*                                                        */

	/*                                                                            */
#if defined(CONFIG_LGE_BROADCAST_TDMB)
	msm_gpiomux_install(msm8974_tdmb_configs, ARRAY_SIZE(msm8974_tdmb_configs));
#endif /*               */

#if defined(CONFIG_NFC_BCM2079X) || defined(CONFIG_LGE_NFC_PN544_C3)
	msm_gpiomux_install(msm8974_nfc_configs, ARRAY_SIZE(msm8974_nfc_configs));
#endif

	/*                                                         */
#if defined(CONFIG_BQ51051B_CHARGER)
	msm_gpiomux_install(wlc_charger_configs,
			ARRAY_SIZE(wlc_charger_configs));
#endif
	/*                                                         */

	/*             
                       
                                   
  */
	msm_gpiomux_install(msm_sensor_configs, ARRAY_SIZE(msm_sensor_configs));

#if defined(CONFIG_MACH_LGE) /*             */
	switch (lge_get_board_revno()) {
		case HW_REV_A:
			msm_gpiomux_install(msm_hall_ic_configs_rev_a, ARRAY_SIZE(msm_hall_ic_configs_rev_a));
			break;
		case HW_REV_B:
		default:
			msm_gpiomux_install(msm_hall_ic_configs_rev_b, ARRAY_SIZE(msm_hall_ic_configs_rev_b));
			break;
	}
#endif
#if defined(CONFIG_USB_LGE_USB3_REDRIVER)
	if (lge_get_board_revno() < HW_REV_C)
		msm_gpiomux_install(usb3_rd_en_configs, ARRAY_SIZE(usb3_rd_en_configs));
#endif
#ifdef CONFIG_SND_FM_RADIO
	msm_gpiomux_install(msm8974_tert_mi2s_configs, ARRAY_SIZE(msm8974_tert_mi2s_configs));
#endif

#if defined(CONFIG_BQ24192_CHARGER)
	if (lge_get_board_revno() > HW_REV_A) {
		msm_gpiomux_install(bq_chg_int_configs, ARRAY_SIZE(bq_chg_int_configs));
		if (lge_get_board_revno() == HW_REV_D)
			bq_chg_en_n_configs[0].gpio = 89;
		msm_gpiomux_install(bq_chg_en_n_configs, ARRAY_SIZE(bq_chg_en_n_configs));
	}
#endif

	if (of_board_is_dragonboard() && machine_is_apq8074())
		msm_gpiomux_install(apq8074_dragonboard_ts_config,
				ARRAY_SIZE(apq8074_dragonboard_ts_config));
}
