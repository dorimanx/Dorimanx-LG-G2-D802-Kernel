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
 *
 */

#include "msm_sensor.h"
#define SENSOR_NAME "imx132"
#define PLATFORM_DRIVER_NAME "msm_camera_imx132"
#define imx132_obj imx132_##obj

/* jinw.kim@lge.com, 2013-01-03
 * G2 Sub Camera Bring up(IMX132)
 * Define PMIC gpio for VT LDO
 */
#if defined(CONFIG_MACH_LGE) && !defined(CONFIG_MACH_MSM8974_LGPS11)
#define PM8941_GPIO_10	10
#define VT_CAM_LDO_EN	481	// PM8941_GPIO_PM_TO_SYS is not defined. When parse <&pm8941_gpios 10 0> from msm_sensor_init_vreg_data(), the value 481 is returned.
#endif

/* jinw.kim@lge.com, 2013-01-03
 * G2 Sub Camera Bring up(IMX132)
 * Add function imx132_sensor_power_up&down
 */
#if defined(CONFIG_MACH_LGE)
extern int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);
extern int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);
static struct camera_vreg_t imx132_vreg_vio = {
	.reg_name = "cam_vio",
	.type = REG_VS,
	.min_voltage = 1800000,
	.max_voltage = 1800000,
	.op_mode = 0,
	.delay = 0,
};
static struct regulator *p_vio;
#endif

DEFINE_MUTEX(imx132_mut);
static struct msm_sensor_ctrl_t imx132_s_ctrl;

static struct msm_camera_i2c_reg_conf imx132_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx132_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx132_groupon_settings[] = {
#if 1
	{0x104, 0x01},
#else
	{0x3208, 0x00},
#endif
};

static struct msm_camera_i2c_reg_conf imx132_groupoff_settings[] = {
#if 1
	{0x104, 0x00},
#else
	{0x3208, 0x10},
	{0x3208, 0xA0},
#endif
};

#if 0
static struct msm_camera_i2c_reg_conf imx132_720_settings[] = {
	{0x3800, 0x01},
	{0x3801, 0x4a},
	{0x3802, 0x00},
	{0x3803, 0xba},
	{0x3804, 0x06},
	{0x3805, 0x51+32},
	{0x3806, 0x03},
	{0x3807, 0x8d+24},
	{0x3810, 0x00},
	{0x3811, 0x05},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0d, 0x03},
	{0x3a0e, 0x03},
	{0x4520, 0x00},
	{0x4837, 0x1b},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xff},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x13},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x10},
	{0x3036, 0x04},
	{0x3037, 0x61},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x4800, 0x24},
};

static struct msm_camera_i2c_reg_conf imx132_vga_settings[] = {
	{0x3800, 0x00},
	{0x3801, 0x0c},
	{0x3802, 0x00},
	{0x3803, 0x02},
	{0x3804, 0x07},
	{0x3805, 0x97+32},
	{0x3806, 0x04},
	{0x3807, 0x45+24},
	{0x3810, 0x00},
	{0x3811, 0x03},
	{0x3812, 0x00},
	{0x3813, 0x03},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0d, 0x03},
	{0x3a0e, 0x03},
	{0x4520, 0x00},
	{0x4837, 0x1b},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xff},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x13},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x10},
	{0x3036, 0x04},
	{0x3037, 0x61},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x4800, 0x24},
	{0x3500, 0x00},
	{0x3501, 0x17},
	{0x3502, 0xf0},
	{0x3508, 0x00},
	{0x3509, 0x20},
};

static struct msm_camera_i2c_reg_conf imx132_60fps_settings[] = {
	{0x3718, 0x10},
	{0x3702, 0x18},
	{0x373a, 0x3c},
	{0x3715, 0x01},
	{0x3703, 0x1d},
	{0x3705, 0x0b},
	{0x3730, 0x1f},
	{0x3704, 0x3f},
	{0x3f06, 0x1d},
	{0x371c, 0x00},
	{0x371d, 0x83},
	{0x371e, 0x00},
	{0x371f, 0xb6},
	{0x3708, 0x63},
	{0x3709, 0x52},
	{0x3800, 0x01},
	{0x3801, 0x42},
	{0x3802, 0x00},
	{0x3803, 0x40},
	{0x3804, 0x06},
	{0x3805, 0x61},
	{0x3806, 0x04},
	{0x3807, 0x08},
	{0x3808, 0x02},
	{0x3809, 0x80},
	{0x380a, 0x01},
	{0x380b, 0xe0},
	{0x380c, 0x03},
	{0x380d, 0x0c},
	{0x380e, 0x02},
	{0x380f, 0x00},
	{0x3810, 0x00},
	{0x3811, 0x0f},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x02},
	{0x3a09, 0x67},
	{0x3a0a, 0x02},
	{0x3a0b, 0x00},
	{0x3a0d, 0x00},
	{0x3a0e, 0x00},
	{0x4520, 0x0a},
	{0x4837, 0x29},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xcf},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x07},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x30},
	{0x3036, 0x14},
	{0x3037, 0x21},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x3011, 0x22},
	{0x3a00, 0x58},
};

static struct msm_camera_i2c_reg_conf imx132_90fps_settings[] = {
	{0x3718, 0x10},
	{0x3702, 0x18},
	{0x373a, 0x3c},
	{0x3715, 0x01},
	{0x3703, 0x1d},
	{0x3705, 0x0b},
	{0x3730, 0x1f},
	{0x3704, 0x3f},
	{0x3f06, 0x1d},
	{0x371c, 0x00},
	{0x371d, 0x83},
	{0x371e, 0x00},
	{0x371f, 0xb6},
	{0x3708, 0x63},
	{0x3709, 0x52},
	{0x3800, 0x01},
	{0x3801, 0x42},
	{0x3802, 0x00},
	{0x3803, 0x40},
	{0x3804, 0x06},
	{0x3805, 0x61},
	{0x3806, 0x04},
	{0x3807, 0x08},
	{0x3808, 0x02},
	{0x3809, 0x80},
	{0x380a, 0x01},
	{0x380b, 0xe0},
	{0x380c, 0x03},
	{0x380d, 0x0c},
	{0x380e, 0x02},
	{0x380f, 0x00},
	{0x3810, 0x00},
	{0x3811, 0x0f},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x02},
	{0x3a09, 0x67},
	{0x3a0a, 0x02},
	{0x3a0b, 0x00},
	{0x3a0d, 0x00},
	{0x3a0e, 0x00},
	{0x4520, 0x0a},
	{0x4837, 0x29},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xcf},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x07},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x30},
	{0x3036, 0x1e},
	{0x3037, 0x21},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x3011, 0x22},
	{0x3a00, 0x58},
};

static struct msm_camera_i2c_reg_conf imx132_120fps_settings[] = {
	{0x3718, 0x10},
	{0x3702, 0x18},
	{0x373a, 0x3c},
	{0x3715, 0x01},
	{0x3703, 0x1d},
	{0x3705, 0x0b},
	{0x3730, 0x1f},
	{0x3704, 0x3f},
	{0x3f06, 0x1d},
	{0x371c, 0x00},
	{0x371d, 0x83},
	{0x371e, 0x00},
	{0x371f, 0xb6},
	{0x3708, 0x63},
	{0x3709, 0x52},
	{0x3800, 0x01},
	{0x3801, 0x42},
	{0x3802, 0x00},
	{0x3803, 0x40},
	{0x3804, 0x06},
	{0x3805, 0x61},
	{0x3806, 0x04},
	{0x3807, 0x08},
	{0x3808, 0x02},
	{0x3809, 0x80},
	{0x380a, 0x01},
	{0x380b, 0xe0},
	{0x380c, 0x03},
	{0x380d, 0x0c},
	{0x380e, 0x02},
	{0x380f, 0x00},
	{0x3810, 0x00},
	{0x3811, 0x0f},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x02},
	{0x3a09, 0x67},
	{0x3a0a, 0x02},
	{0x3a0b, 0x00},
	{0x3a0d, 0x00},
	{0x3a0e, 0x00},
	{0x4520, 0x0a},
	{0x4837, 0x29},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xcf},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x07},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x10},
	{0x3036, 0x14},
	{0x3037, 0x21},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x3011, 0x22},
	{0x3a00, 0x58},
};
#endif
static struct msm_camera_i2c_reg_conf imx132_recommend_settings[] = {
#if 1
	/* global setting */
	{0x3087, 0x53},
	{0x308B, 0x5A},
	{0x3094, 0x11},
	{0x309D, 0xA4},
	{0x30AA, 0x01},
	{0x30C6, 0x00},
	{0x30C7, 0x00},
	{0x3118, 0x2F},
	{0x312A, 0x00},
	{0x312B, 0x0B},
	{0x312C, 0x0B},
	{0x312D, 0x13},
	/* black level setting */
#else
	{0x0103, 0x01},
	{0x3718, 0x10},
	{0x3702, 0x24},
	{0x373a, 0x60},
	{0x3715, 0x01},
	{0x3703, 0x2e},
	{0x3705, 0x10},
	{0x3730, 0x30},
	{0x3704, 0x62},
	{0x3f06, 0x3a},
	{0x371c, 0x00},
	{0x371d, 0xc4},
	{0x371e, 0x01},
	{0x371f, 0x0d},
	{0x3708, 0x61},
	{0x3709, 0x12},
#endif
};

static struct msm_camera_i2c_reg_conf imx132_prev_settings[] = {
	//19.2 Mhz settings
	//reg_D Preview Full resolution 2.38M, 1976x1200 - 30fps
	/* read out direction */
	/* PLL setting */
	{0x0305, 0x01},
	{0x0307, 0x2D},
	{0x30A4, 0x02},
	{0x303C, 0x3C},
	/* mode setting */
	{0x0340, 0x05},
	{0x0341, 0x00},
	{0x0342, 0x08},
	{0x0343, 0xC8},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x07},
	{0x0349, 0xB7},
	{0x034A, 0x04},
	{0x034B, 0xAF},
	{0x034C, 0x07},
	{0x034D, 0xB8},
	{0x034E, 0x04},
	{0x034F, 0xB0},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x303D, 0x10},
	{0x303E, 0x5A},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x2F},
	{0x304D, 0x02},
	{0x3064, 0x92},
	{0x306A, 0x10},
	{0x309B, 0x00},
	{0x309E, 0x41},
	{0x30A0, 0x10},
	{0x30A1, 0x0B},
	{0x30B2, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30D8, 0x00},
	{0x30D9, 0x00},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x00},
	{0x3102, 0x0C},
	{0x3103, 0x33},
	{0x3104, 0x18},
	{0x3105, 0x00},
	{0x3106, 0x65},
	{0x3107, 0x00},
	{0x3108, 0x06},
	{0x3109, 0x04},
	{0x310A, 0x04},
	{0x315C, 0x3D},
	{0x315D, 0x3C},
	{0x316E, 0x3E},
	{0x316F, 0x3D},
	{0x3301, 0x01},
	{0x3304, 0x07},
	{0x3305, 0x06},
	{0x3306, 0x19},
	{0x3307, 0x03},
	{0x3308, 0x0F},
	{0x3309, 0x07},
	{0x330A, 0x0C},
	{0x330B, 0x06},
	{0x330C, 0x0B},
	{0x330D, 0x07},
	{0x330E, 0x03},
	{0x3318, 0x61},
	{0x3322, 0x09},
	{0x3342, 0x00},
	{0x3348, 0xE0},
	{0x0101, 0x03},
	// end 19.2 Mhz settings
#if 0 //disable 24 Mhz Settings
#if 1
	//reg_D Preview Full resolution 2.38M, 1976x1200 - 30fps
	/* read out direction */
	{0x0101, 0x03},
	/* PLL setting */
	{0x0305, 0x04},
	{0x0307, 0x87},
	{0x30A4, 0x02},
	{0x303C, 0x4B},
	/* mode setting */
	{0x0340, 0x04},
	{0x0341, 0xD2},
	//{0x0341, 0xCA},
	{0x0342, 0x08},
	{0x0343, 0xC8},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x07},
	{0x0349, 0xB7},
	{0x034A, 0x04},
	{0x034B, 0xAF},
	{0x034C, 0x07},
	{0x034D, 0xB8},
	{0x034E, 0x04},
	{0x034F, 0xB0},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x303D, 0x10},
	{0x303E, 0x5A},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x2F},
	{0x304D, 0x02},
	{0x306A, 0x10},
	{0x309B, 0x00},
	{0x309E, 0x41},
	{0x30A0, 0x10},
	{0x30A1, 0x0B},
	{0x30D5, 0x00},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30DE, 0x00},
	{0x3102, 0x0C},
	{0x3103, 0x33},
	{0x3104, 0x18},
	{0x3106, 0x65},
	{0x315C, 0x3D},
	{0x315D, 0x3C},
	{0x316E, 0x3E},
	{0x316F, 0x3D},
	{0x3318, 0x61},
	{0x3348, 0xE0},
#else
	{0x3800, 0x00},
	{0x3801, 0x02},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x07},
	{0x3805, 0xA1},
	{0x3806, 0x04},
	{0x3807, 0x47},
	{0x3810, 0x00},
	{0x3811, 0x09},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0d, 0x03},
	{0x3a0e, 0x03},
	{0x4520, 0x00},
	{0x4837, 0x1b},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xcf},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x03},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x10},
	{0x3036, 0x1e},
	{0x3037, 0x21},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x4800, 0x24},
#endif
#endif //disable 24 Mhz Settings
};

static struct v4l2_subdev_info imx132_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx132_init_conf[] = {
	{
		&imx132_recommend_settings[0],
		ARRAY_SIZE(imx132_recommend_settings), 0,
		MSM_CAMERA_I2C_BYTE_DATA
	}
};

static struct msm_camera_i2c_conf_array imx132_confs[] = {
	{
		&imx132_prev_settings[0],
		ARRAY_SIZE(imx132_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA
	},
#if 0
	{
		&imx132_vga_settings[0],
		ARRAY_SIZE(imx132_vga_settings), 0, MSM_CAMERA_I2C_BYTE_DATA
	},
	{
		&imx132_720_settings[0],
		ARRAY_SIZE(imx132_720_settings), 0, MSM_CAMERA_I2C_BYTE_DATA
	},
	{
		&imx132_60fps_settings[0],
		ARRAY_SIZE(imx132_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA
	},
	{
		&imx132_90fps_settings[0],
		ARRAY_SIZE(imx132_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA
	},
	{
		&imx132_120fps_settings[0],
		ARRAY_SIZE(imx132_120fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA
	},
#endif
};

static struct msm_sensor_output_info_t imx132_dimensions[] = {
#if 1
	{
		.x_output = 0x7B8, //1976
		.y_output = 0x4B0, // 1200
		.line_length_pclk = 0x8C8,
		.frame_length_lines = 0x500,
		.vt_pixel_clk = 864000000,
		.op_pixel_clk = 864000000,
	},
#else
	{
		.x_output = 0x510, //1296
		.y_output = 0x410, // 1040
		.line_length_pclk = 0x570,
		.frame_length_lines = 0x432,
		.vt_pixel_clk = 45600000,
		.op_pixel_clk = 45600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x78C,
		.y_output = 0x444,
		.line_length_pclk = 0x85c,
		.frame_length_lines = 0x460,
		.vt_pixel_clk = 72000000,
		.op_pixel_clk = 72000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x510,
		.y_output = 0x278,
		.line_length_pclk = 0x85c,
		.frame_length_lines = 0x460,
		.vt_pixel_clk = 72000000,
		.op_pixel_clk = 72000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x298,
		.y_output = 0x1F2,
		.line_length_pclk = 0x85c,
		.frame_length_lines = 0x460,
		.vt_pixel_clk = 72000000,
		.op_pixel_clk = 72000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x280, /* 640 */
		.y_output = 0x1E0, /* 480 */
		.line_length_pclk = 0x30C, /* 780 */
		.frame_length_lines = 0x200, /* 512 */
		.vt_pixel_clk = 24000000,
		.op_pixel_clk = 24000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x280, /* 640 */
		.y_output = 0x1E0, /* 480 */
		.line_length_pclk = 0x30C, /* 780 */
		.frame_length_lines = 0x200, /* 512 */
		.vt_pixel_clk = 36000000,
		.op_pixel_clk = 36000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x280, /* 640 */
		.y_output = 0x1E0, /* 480 */
		.line_length_pclk = 0x30C, /* 780 */
		.frame_length_lines = 0x200, /* 512 */
		.vt_pixel_clk = 48000000,
		.op_pixel_clk = 48000000,
		.binning_factor = 1,
	},
#endif
};

static struct msm_sensor_output_reg_addr_t imx132_reg_addr = {
#if 1
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
#else
	.x_output = 0x3808,
	.y_output = 0x380a,
	.line_length_pclk = 0x380c,
	.frame_length_lines = 0x380e,
#endif
};

static struct msm_sensor_id_info_t imx132_id_info = {
#if 1
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x132,
#else
	.sensor_id_reg_addr = 0x300A,
	.sensor_id = 0x2720,
#endif
};

static struct msm_sensor_exp_gain_info_t imx132_exp_gain_info = {
#if 1
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x204,
	.vert_offset = 3,
#else
	.coarse_int_time_addr = 0x3501,
	.global_gain_addr = 0x3508,
	.vert_offset = 6,
#endif
};

#ifndef CONFIG_MACH_LGE
static enum msm_camera_vreg_name_t imx132_veg_seq[] = {
       CAM_VIO,
       CAM_VANA,
       CAM_VDIG,
};
#endif /* QCT Original Code */

static const struct i2c_device_id imx132_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx132_s_ctrl},
	{ }
};

static struct i2c_driver imx132_i2c_driver = {
	.id_table = imx132_i2c_id,
	.probe = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx132_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};


static const struct of_device_id imx132_dt_match[] = {
	{.compatible = "qcom,imx132", .data = &imx132_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx132_dt_match);

static struct platform_driver imx132_platform_driver = {
	.driver = {
		.name = "qcom,imx132",
		.owner = THIS_MODULE,
		.of_match_table = imx132_dt_match,
	},
};

static int32_t imx132_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(imx132_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init msm_sensor_init_module(void)
{
	int32_t rc = 0;
	rc = platform_driver_probe(&imx132_platform_driver,
			imx132_platform_probe);
	if (!rc)
		return rc;
	return i2c_add_driver(&imx132_i2c_driver);
}

static void __exit msm_sensor_exit_module(void)
{
	if (imx132_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx132_s_ctrl);
		platform_driver_unregister(&imx132_platform_driver);
	} else
		i2c_del_driver(&imx132_i2c_driver);
	return;
}

static struct v4l2_subdev_core_ops imx132_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx132_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx132_subdev_ops = {
	.core = &imx132_subdev_core_ops,
	.video = &imx132_subdev_video_ops,
};

/* jinw.kim@lge.com, 2013-01-03
 * G2 Sub Camera Bring up(IMX132)
 * Add function imx132_sensor_power_up&down
 */
#if defined(CONFIG_MACH_LGE)
static struct msm_cam_clk_info cam_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_cam_clk_info cam_8974_clk_info[] = {
	{"cam_src_clk", 19200000},
	{"cam_clk", -1},
};

int32_t imx132_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	pr_err("%s: E: %s\n", __func__, data->sensor_name);

	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE) {
		msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_RELEASE);
	}

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	if (s_ctrl->sensor_device_type == MSM_SENSOR_I2C_DEVICE)
		msm_cam_clk_enable(dev, cam_8960_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_8960_clk_info), 0);
	else
		msm_cam_clk_enable(dev, cam_8974_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_8974_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
	msm_camera_enable_vreg(dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		(s_ctrl->sensordata->sensor_platform_info->num_vreg) - (s_ctrl->skip_vio),
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		(s_ctrl->sensordata->sensor_platform_info->num_vreg) - (s_ctrl->skip_vio),
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);

	if(!(s_ctrl->skip_vio)) {
		CDBG("%s: Turning off VIO\n", __func__);
		if(p_vio) {
			regulator_disable(p_vio);
			usleep_range(imx132_vreg_vio.delay * 1000, (imx132_vreg_vio.delay * 1000) + 1000);
			regulator_put(p_vio);
	        p_vio = NULL;
		} else {
			pr_err("%s: p_vio is NULL!!!\n", __func__);
		}
	} else {
		pr_info("%s: skip turing off VIO!!!\n", __func__);
	}

/* jinw.kim@lge.com, 2013-01-03
 * G2 Sub Camera Bring up(IMX132)
 * Separated power control
 */
#if defined(CONFIG_MACH_LGE) && !defined(CONFIG_MACH_MSM8974_LGPS11)
	CDBG("%s: Turning off VDIG\n", __func__);
	gpio_direction_output(VT_CAM_LDO_EN, 0);
	gpio_free(VT_CAM_LDO_EN);
#endif

	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;

	pr_err("%s: X\n", __func__);

	return 0;
}

int32_t imx132_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;

	pr_err("%s: E: %s\n", __func__, data->sensor_name);
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

	CDBG("%s: Turning on VIO\n", __func__);

	p_vio = regulator_get(dev, imx132_vreg_vio.reg_name);
	if (IS_ERR(p_vio)) {
			pr_err("%s: %s get failed\n", __func__, imx132_vreg_vio.reg_name);
			p_vio = NULL;
			goto enable_vreg_failed;
	}

	rc = regulator_enable(p_vio);
	if (rc < 0) {
		pr_err("%s: %s enable failed\n", __func__, imx132_vreg_vio.reg_name);
		goto enable_vio_failed;
	}
	usleep_range(imx132_vreg_vio.delay * 1000, (imx132_vreg_vio.delay * 1000) + 1000);

/* jinw.kim@lge.com, 2013-01-03
 * G2 Sub Camera Bring up(IMX132)
 * Separated power control
 */
#if defined(CONFIG_MACH_LGE) && !defined(CONFIG_MACH_MSM8974_LGPS11)
    CDBG("%s: Turning on VDIG\n", __func__);
    rc = gpio_request(VT_CAM_LDO_EN, "VT_CAM_LDO_EN");
    if (rc) {
        pr_err("%s : enable vt_ldo (pm8941_gpio_10) is failed.\n", __func__);
		gpio_free(VT_CAM_LDO_EN);
		goto enable_vio_failed;
    }
    gpio_direction_output(VT_CAM_LDO_EN, 1);
#endif

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->sensor_device_type == MSM_SENSOR_I2C_DEVICE) {
		if (s_ctrl->clk_rate != 0)
			cam_8960_clk_info->clk_rate = s_ctrl->clk_rate;

		rc = msm_cam_clk_enable(dev, cam_8960_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_8960_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: clk enable failed\n", __func__);
			goto enable_clk_failed;
		}
	} else {
		rc = msm_cam_clk_enable(dev, cam_8974_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_8974_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: clk enable failed\n", __func__);
			goto enable_clk_failed;
		}
	}

	if (!s_ctrl->power_seq_delay)
		usleep_range(1000, 2000);
	else if (s_ctrl->power_seq_delay < 20)
		usleep_range((s_ctrl->power_seq_delay * 1000),
			((s_ctrl->power_seq_delay * 1000) + 1000));
	else
		msleep(s_ctrl->power_seq_delay);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE) {
		rc = msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto cci_init_failed;
		}
	}
	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
	pr_err("%s: X\n", __func__);

	return rc;

cci_init_failed:
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->vreg_seq,
			s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 0);
#if defined(CONFIG_MACH_LGE)
enable_vio_failed:
	if(p_vio) {
		regulator_disable(p_vio);
		usleep_range(imx132_vreg_vio.delay * 1000, (imx132_vreg_vio.delay * 1000) + 1000);
		regulator_put(p_vio);
		p_vio = NULL;
	}
#endif
enable_vreg_failed:
	msm_camera_config_vreg(dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
}
#endif /* CONFIG_MACH_LGE */

static int32_t imx132_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, offset;
	uint8_t int_time[3];
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;

	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			fl_lines, MSM_CAMERA_I2C_WORD_DATA);
	int_time[0] = line >> 12;
	int_time[1] = line >> 4;
	int_time[2] = line << 4;
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr-1,
			int_time[0], MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			int_time[1], MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr+1,
			int_time[2], MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
			MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}
static struct msm_sensor_fn_t imx132_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = imx132_write_exp_gain,
	.sensor_write_snapshot_exp_gain = imx132_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
/* jinw.kim@lge.com, 2013-01-03
 * G2 Sub Camera Bring up(IMX132)
 * Add function imx132_sensor_power_up&down
 */
#if defined(CONFIG_MACH_LGE)
	.sensor_power_up = imx132_sensor_power_up,
	.sensor_power_down = imx132_sensor_power_down,
#else
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
#endif
	//.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines2,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t imx132_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx132_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx132_start_settings),
	.stop_stream_conf = imx132_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx132_stop_settings),
	.group_hold_on_conf = imx132_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx132_groupon_settings),
	.group_hold_off_conf = imx132_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(imx132_groupoff_settings),
	.init_settings = &imx132_init_conf[0],
	.init_size = ARRAY_SIZE(imx132_init_conf),
	.mode_settings = &imx132_confs[0],
	.output_settings = &imx132_dimensions[0],
	.num_conf = ARRAY_SIZE(imx132_confs),
};

static struct msm_sensor_ctrl_t imx132_s_ctrl = {
	.msm_sensor_reg = &imx132_regs,
	.sensor_i2c_client = &imx132_sensor_i2c_client,
	.sensor_i2c_addr = 0x6C,
#ifndef CONFIG_MACH_LGE
	.vreg_seq = imx132_veg_seq,
	.num_vreg_seq = ARRAY_SIZE(imx132_veg_seq),
#endif /* QCT Original Code */
	.sensor_output_reg_addr = &imx132_reg_addr,
	.sensor_id_info = &imx132_id_info,
	.sensor_exp_gain_info = &imx132_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &imx132_mut,
	.sensor_i2c_driver = &imx132_i2c_driver,
	.sensor_v4l2_subdev_info = imx132_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx132_subdev_info),
	.sensor_v4l2_subdev_ops = &imx132_subdev_ops,
	.func_tbl = &imx132_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
module_exit(msm_sensor_exit_module);
MODULE_DESCRIPTION("sony 2.4MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
