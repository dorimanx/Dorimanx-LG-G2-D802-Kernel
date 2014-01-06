/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, LGE Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#include <mach/vreg.h>
#include <mach/rpc_server_handset.h>
#include <mach/board.h>

/* keypad */
#include <linux/mfd/pm8xxx/pm8921.h>

/* i2c */
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
#include <linux/input/touch_synaptics_rmi4_i2c.h>
#include <linux/input/lge_touch_core_vu3evb.h>
#endif
#include <mach/board_lge.h>

#if defined(CONFIG_RMI4_I2C)
#include <linux/rmi.h>
#endif

//#include "board-j1.h"
#define NR_MSM_IRQS 288
#define MSM_GPIO_TO_INT(n) (NR_MSM_IRQS + (n))

/* TOUCH GPIOS */
#define SYNAPTICS_TS_I2C_SDA                 	6
#define SYNAPTICS_TS_I2C_SCL                 	7
#define SYNAPTICS_TS_I2C_INT_GPIO            	5
#define SYNAPTICS_TS_I2C_INT_GPIO_GVDCM       	59
#define TOUCH_RESET                             8
#define TOUCH_POWER_EN                          62

#define TOUCH_FW_VERSION                        1

/* touch screen device */
#define APQ8064_GSBI3_QUP_I2C_BUS_ID	2

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
int synaptics_t1320_power_on(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_l15 = NULL;
	static struct regulator *vreg_l22 = NULL;
#if defined(CONFIG_MACH_APQ8064_GVDCM)
	static struct regulator *vreg_l21 = NULL;
#endif
	/* 3.3V_TOUCH_VDD, VREG_L15: 2.75 ~ 3.3 */
	if (!vreg_l15) {
		vreg_l15 = regulator_get(NULL, "8941_l22");
		if (IS_ERR(vreg_l15)) {
			pr_err("%s: regulator get of 8921_l15 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_l15));
			rc = PTR_ERR(vreg_l15);
			vreg_l15 = NULL;
			return rc;
		}
	}
	/* 1.8V_TOUCH_IO, VREG_L22: 1.7 ~ 2.85 */
	if (!vreg_l22) {
		vreg_l22 = regulator_get(NULL, "8941_lvs3");
		if (IS_ERR(vreg_l22)) {
			pr_err("%s: regulator get of 8921_l22 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_l22));
			rc = PTR_ERR(vreg_l22);
			vreg_l22 = NULL;
			return rc;
		}
	}

#if defined(CONFIG_MACH_APQ8064_GVDCM)
        if (lge_get_board_revno() == HW_REV_A) {
		/* 1.8V_TOUCH_IO, VREG_L22: 1.7 ~ 2.85 */
		if (!vreg_l21) {
			vreg_l21 = regulator_get(NULL, "touch_io_temp");
			if (IS_ERR(vreg_l21)) {
				pr_err("%s: regulator get of 8921_l22 failed (%ld)\n",
						__func__,
				       PTR_ERR(vreg_l21));
				rc = PTR_ERR(vreg_l21);
				vreg_l21 = NULL;
				return rc;
			}
		}
	}
#endif
	rc = regulator_set_voltage(vreg_l15, 3300000, 3300000);
	rc |= regulator_set_voltage(vreg_l22, 1800000, 1800000);
#if defined(CONFIG_MACH_APQ8064_GVDCM)
        if (lge_get_board_revno() == HW_REV_A) 
	rc |= regulator_set_voltage(vreg_l21, 1800000, 1800000);
#endif	
	if (rc < 0) {
		printk(KERN_INFO "[Touch D] %s: cannot control regulator\n",
		       __func__);
		return rc;
	}

	if (on) {
		printk("[Touch D]touch enable\n");
		regulator_enable(vreg_l15);
		regulator_enable(vreg_l22);
#if defined(CONFIG_MACH_APQ8064_GVDCM)
     	if (lge_get_board_revno() == HW_REV_A)
		regulator_enable(vreg_l21);
#endif
	} else {
		printk("[Touch D]touch disable\n");
		regulator_disable(vreg_l15);
		regulator_disable(vreg_l22);
#if defined(CONFIG_MACH_APQ8064_GVDCM)
        if (lge_get_board_revno() == HW_REV_A)
		regulator_disable(vreg_l21);
#endif
	}

	return rc;
}

static struct touch_power_module touch_pwr = {
	.use_regulator	= 1,
	.vdd			= "8941_l22",
	.vdd_voltage	= 3300000,
	.vio			= "8941_lvs3",
	.vio_voltage	= 1800000,
	.power			= synaptics_t1320_power_on,
};

static struct touch_device_caps touch_caps = {
	.button_support 			= 1,
	.y_button_boundary			= 0,
#if defined(CUST_G_TOUCH) || defined(CONFIG_MACH_APQ8064_GKATT) || defined(CONFIG_MACH_APQ8064_GVDCM) || defined(CONFIG_MACH_APQ8064_GV_KR)
	.number_of_button 			= 2,
	.button_name 				= {KEY_BACK,KEY_MENU},
#else
	.number_of_button 			= 3,
	.button_name 				= {KEY_BACK,KEY_HOMEPAGE,KEY_MENU},
#endif
	.button_margin				= 0,	
	.is_width_supported 		= 1,
	.is_pressure_supported 		= 1,
	.is_id_supported			= 1,
	.max_width 					= 15,
	.max_pressure 				= 0xFF,
	.max_id						= 10,

#if defined(CUST_G_TOUCH) || defined(CONFIG_MACH_APQ8064_GKATT) || defined(CONFIG_MACH_APQ8064_GVDCM) || defined(CONFIG_MACH_APQ8064_GV_KR)
	.lcd_x						= 1080,
	.lcd_y						= 1920,
#elif defined(CONFIG_MACH_APQ8064_J1D) || defined(CONFIG_MACH_APQ8064_J1KD)
	.lcd_x						= 720,
	.lcd_y						= 1280,
#else
	.lcd_x						= 768,
	.lcd_y						= 1280,
#endif

#if defined(CONFIG_MACH_APQ8064_J1D) || defined(CONFIG_MACH_APQ8064_J1KD)
	.x_max						= 1440,
#else
	.x_max						= 1536,
#endif
	.y_max						= 2560,
};

static struct touch_operation_role touch_role = {
	.operation_mode 		= INTERRUPT_MODE,
	.key_type				= TOUCH_HARD_KEY,
	.report_mode			= REDUCED_REPORT_MODE,
	.delta_pos_threshold 	= 1,
	.orientation 			= 0,
	.report_period			= 10000000,
	.booting_delay 			= 200,
	.reset_delay			= 5,
	.suspend_pwr			= POWER_OFF,
	.resume_pwr				= POWER_ON,
	.jitter_filter_enable	= 0,
	.jitter_curr_ratio		= 30,
	.accuracy_filter_enable = 1,
	.irqflags 				= IRQF_TRIGGER_FALLING,
#ifdef CUST_G_TOUCH
	.show_touches			= 0,
	.pointer_location		= 0,
	.ta_debouncing_count    = 2,
	.ta_debouncing_finger_num  = 2,
#ifdef CONFIG_MACH_APQ8064_GVDCM
	.ghost_detection_enable = 0,
#else
	.ghost_detection_enable = 1,
#endif
	.pen_enable		= 0,
#endif
};

static struct touch_platform_data j1_ts_data = {
	.int_pin	= SYNAPTICS_TS_I2C_INT_GPIO,
	.reset_pin	= TOUCH_RESET,
	.maker		= "Synaptics",
	.fw_version	= "E129",
	.caps		= &touch_caps,
	.role		= &touch_role,
	.pwr		= &touch_pwr,
};

static struct i2c_board_info synaptics_ts_info[] = {
	[0] = {
		I2C_BOARD_INFO(LGE_TOUCH_NAME, 0x20),
		.platform_data = &j1_ts_data,
		.irq = MSM_GPIO_TO_INT(SYNAPTICS_TS_I2C_INT_GPIO),
	},
};
#endif





#if defined(CONFIG_RMI4_I2C) 
struct syna_gpio_data {
	u16 gpio_number;
	char* gpio_name;
};

static int synaptics_touchpad_gpio_setup(void *gpio_data, bool configure)
{

	int rc = -EINVAL;
	static struct regulator *vreg_l15 = NULL;
	static struct regulator *vreg_l22 = NULL;
#if defined(CONFIG_MACH_APQ8064_GVDCM)
	static struct regulator *vreg_l21 = NULL;
#endif

	struct syna_gpio_data *data = gpio_data;
	
	pr_err("%s: [Touch D] S1 \n",__func__);

 #if defined(TOUCH_RESET)
       rc = gpio_request(TOUCH_RESET, "rmi4_reset_pin");
       if (rc) {
       	pr_err("%s: Failed to get rmi4_reset_pin %d. Code: %d.",
       						__func__, TOUCH_RESET, rc);
       	return rc;
       }            
       rc = gpio_direction_output(TOUCH_RESET, 1);
 #endif


	/* 3.3V_TOUCH_VDD, VREG_L15: 2.75 ~ 3.3 */
	if (!vreg_l15) {
		vreg_l15 = regulator_get(NULL, "touch_vdd");
		if (IS_ERR(vreg_l15)) {
			pr_err("%s: regulator get of 8921_l15 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_l15));
			rc = PTR_ERR(vreg_l15);
			vreg_l15 = NULL;
			return rc;
		}
	}
	/* 1.8V_TOUCH_IO, VREG_L22: 1.7 ~ 2.85 */
	if (!vreg_l22) {
		vreg_l22 = regulator_get(NULL, "touch_io");
		if (IS_ERR(vreg_l22)) {
			pr_err("%s: regulator get of 8921_l22 failed (%ld)\n",
					__func__,
			       PTR_ERR(vreg_l22));
			rc = PTR_ERR(vreg_l22);
			vreg_l22 = NULL;
			return rc;
		}
	}

#if defined(CONFIG_MACH_APQ8064_GVDCM)
        if (lge_get_board_revno() == HW_REV_A) {
		/* 1.8V_TOUCH_IO, VREG_L22: 1.7 ~ 2.85 */
		if (!vreg_l21) {
			vreg_l21 = regulator_get(NULL, "touch_io_temp");
			if (IS_ERR(vreg_l21)) {
				pr_err("%s: regulator get of 8921_l22 failed (%ld)\n",
						__func__,
				       PTR_ERR(vreg_l21));
				rc = PTR_ERR(vreg_l21);
				vreg_l21 = NULL;
				return rc;
			}
		}
	}
#endif
	rc = regulator_set_voltage(vreg_l15, 3300000, 3300000);
	rc |= regulator_set_voltage(vreg_l22, 1800000, 1800000);
#if defined(CONFIG_MACH_APQ8064_GVDCM)
        if (lge_get_board_revno() == HW_REV_A) 
	rc |= regulator_set_voltage(vreg_l21, 1800000, 1800000);
#endif	
	if (rc < 0) {
		printk(KERN_INFO "[Touch D] %s: cannot control regulator\n",
		       __func__);
		return rc;
	}

	printk("[Touch D]touch enable\n");
			regulator_enable(vreg_l15);
			regulator_enable(vreg_l22);
#if defined(CONFIG_MACH_APQ8064_GVDCM)
			if (lge_get_board_revno() == HW_REV_A)
			regulator_enable(vreg_l21);
#endif


	if (configure) {
		rc = gpio_request(data->gpio_number, "rmi4_attn");
		if (rc) {
			pr_err("%s: Failed to get attn gpio %d. Code: %d.",
								__func__, data->gpio_number, rc);
			return rc;
		}

		gpio_tlmm_config(GPIO_CFG(data->gpio_number, 0, GPIO_CFG_INPUT,
								GPIO_CFG_PULL_UP, GPIO_CFG_6MA),GPIO_CFG_ENABLE);

		rc = gpio_direction_input(data->gpio_number);
		if (rc) {
				pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
				__func__, data->gpio_number, rc);
				gpio_free(data->gpio_number);
		}
	} else {
			pr_warn("%s: No way to deconfigure gpio %d.",
			__func__, data->gpio_number);
	}

	if(rc < 0){
			printk(KERN_INFO "[Touch D] %s: cannot request GPIO\n", __func__);
			return rc;
	}
	
	printk("[Touch D]synaptics_touchpad_gpio_setup -- \n");

	return rc;
}

#define AXIS_ALIGNMENT { \
	.swap_axes = false, \
	.flip_x = false, \
	.flip_y = false, \
	.clip_X_low  = 0, \
	.clip_Y_low  = 0, \
	.offset_X = 0,  \
	.offset_Y = 0, \
}

#define TM2144_ADDR 0x20
#define TM2144_ATTN 6

static unsigned char tm2144_button_codes[] = {KEY_BACK,KEY_HOMEPAGE,KEY_MENU}; 
static struct rmi_f1a_button_map tm2144_button_map = {
	.nbuttons = ARRAY_SIZE(tm2144_button_codes),
	.map = tm2144_button_codes,
};

static struct syna_gpio_data tm2144_gpiodata = {
	.gpio_number = TM2144_ATTN,
	.gpio_name = "ts_int.gpio_6",
};

static struct rmi_device_platform_data tm2144_platformdata = {
	.driver_name = "rmi_generic",
	.sensor_name = "TM2144",
	.attn_gpio = TM2144_ATTN,
	.attn_polarity = RMI_ATTN_ACTIVE_LOW,
	.gpio_data = &tm2144_gpiodata,
	.gpio_config = synaptics_touchpad_gpio_setup,
	.axis_align = AXIS_ALIGNMENT,
	.f1a_button_map = &tm2144_button_map,
	.reset_delay_ms = 100,
};


static struct i2c_board_info synaptics_ds4_rmi_info[] = {
     [0] = {
         I2C_BOARD_INFO("rmi_i2c", TM2144_ADDR),
        .platform_data = &tm2144_platformdata,
     },
};
#endif


void __init vu3ebv_init_input(void)
{
	printk("[Touch D] %s: NOT DCM KDDI, reg synaptics driver \n", __func__);

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)

#if defined(CONFIG_MACH_APQ8064_GVDCM)
        if(lge_get_board_revno() >= HW_REV_C) {
                synaptics_ts_info[0].irq = MSM_GPIO_TO_INT(SYNAPTICS_TS_I2C_INT_GPIO_GVDCM);
                j1_ts_data.int_pin = SYNAPTICS_TS_I2C_INT_GPIO_GVDCM;
        }
#elif defined(CONFIG_MACH_APQ8064_GV_KR)
        if(lge_get_board_revno() == HW_REV_C) {
                synaptics_ts_info[0].irq = MSM_GPIO_TO_INT(SYNAPTICS_TS_I2C_INT_GPIO_GVDCM);
                j1_ts_data.int_pin = SYNAPTICS_TS_I2C_INT_GPIO_GVDCM;
        }
#endif
	i2c_register_board_info(APQ8064_GSBI3_QUP_I2C_BUS_ID,
				&synaptics_ts_info[0], 1);

#endif

// Wireless Debugging Porting
#if defined(CONFIG_RMI4_I2C) 
sdfdsf
		i2c_register_board_info(APQ8064_GSBI3_QUP_I2C_BUS_ID,
			&synaptics_ds4_rmi_info[0], 1);
#endif

}
