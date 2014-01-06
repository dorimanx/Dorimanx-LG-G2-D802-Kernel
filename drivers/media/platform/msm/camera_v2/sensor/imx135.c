/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <mach/board_lge.h>		//to use lge_get_board_revno()

#define IMX135_SENSOR_NAME "imx135"
DEFINE_MSM_MUTEX(imx135_mut);

//                                      
#define CONFIG_IMX135_DEBUG
//                                      

#undef CDBG
#ifdef CONFIG_IMX135_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_sensor_ctrl_t imx135_s_ctrl;

#if defined(CONFIG_MACH_LGE)
/*             
                                                               
                               
                                                                           
                                
 */
#if !(defined(CONFIG_MACH_MSM8974_G2_TMO_US) || defined(CONFIG_MACH_MSM8974_G2_VZW) || defined(CONFIG_MACH_MSM8974_G2_SPR) || defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI) || defined(CONFIG_MACH_MSM8974_G2_CA) || defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_Z_US) || defined(CONFIG_MACH_MSM8974_Z_KDDI) || defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU)|| defined(CONFIG_MACH_MSM8974_G2_OPEN_AME))
static struct msm_sensor_power_setting imx135_power_setting_rev_a[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 0,
	},
	{										//VANA, GPIO 16
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 30,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 30,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};
#endif

static struct msm_sensor_power_setting imx135_power_setting_rev_b[] = {
#if (defined(CONFIG_MACH_MSM8974_Z_KR) && defined(CONFIG_MACH_MSM8974_VU3_KR) && defined(CONFIG_MACH_MSM8974_G2_KDDI) && defined(CONFIG_MACH_MSM8974_Z_KDDI))
	{  /* Set GPIO_RESET to low to disable power on reset*/
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{										//VDIG
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{										//VANA, GPIO 16
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{										//VIO, GPIO 96
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{										//VCM, GPIO 57
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VCM,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
#else
	{  /* Set GPIO_RESET to low to disable power on reset*/
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{	// OIS_RESET
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_OIS_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{										//VDIG
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{										//VANA, GPIO 16
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{										//OIS_LDO_EN, GPIO 145
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_OIS_LDO_EN,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{										//VCM, GPIO 57
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VCM,
		.config_val = GPIO_OUT_HIGH,
		.delay = 3,
	},

	{										//VIO, GPIO 96
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{	// OIS_RESET
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_OIS_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
#endif
};
/*                                                                            */
#endif //                            

static struct v4l2_subdev_info imx135_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id imx135_i2c_id[] = {
	{IMX135_SENSOR_NAME, (kernel_ulong_t)&imx135_s_ctrl},
	{ }
};

static struct i2c_driver imx135_i2c_driver = {
	.id_table = imx135_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = IMX135_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx135_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id imx135_dt_match[] = {
	{.compatible = "qcom,imx135", .data = &imx135_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx135_dt_match);

static struct platform_driver imx135_platform_driver = {
	.driver = {
		.name = "qcom,imx135",
		.owner = THIS_MODULE,
		.of_match_table = imx135_dt_match,
	},
};

static int32_t imx135_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	CDBG("%s E\n", __func__);
	match = of_match_device(imx135_dt_match, &pdev->dev);
/*                                                    */
	if(!match)
	{
	      pr_err(" %s failed ",__func__);
	      return -ENODEV;
       }
/*                                                    */
	rc = msm_sensor_platform_probe(pdev, match->data);
	CDBG("%s: X, rc = %d\n", __func__, rc);
	return rc;
}

static int __init imx135_init_module(void)
{
	int32_t rc = 0;
	CDBG("%s E\n", __func__);

#if defined(CONFIG_MACH_LGE)
#if defined(CONFIG_MACH_MSM8974_G2_TMO_US) || defined(CONFIG_MACH_MSM8974_G2_VZW) || defined(CONFIG_MACH_MSM8974_G2_SPR) || defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI) || defined(CONFIG_MACH_MSM8974_G2_CA) || defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_Z_US) || defined(CONFIG_MACH_MSM8974_Z_KDDI) || defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_OPT_AU) || defined(CONFIG_MACH_MSM8974_G2_OPEN_AME)
/*             
                                          
                               
                            
                                
 */
	switch(lge_get_board_revno()) {
		case HW_REV_A:
		default:
			CDBG("%s: Sensor power is set as TMUS/VZW/SPR/DCM Rev.A\n", __func__);
			imx135_s_ctrl.power_setting_array.power_setting = imx135_power_setting_rev_b;
			imx135_s_ctrl.power_setting_array.size = ARRAY_SIZE(imx135_power_setting_rev_b);
			break;
	}
 //                                                     
#else	/* only for LGU / AT&T */
/*             
                                                               
                               
 */
	switch(lge_get_board_revno()) {
		case HW_REV_A:
			CDBG("%s: Sensor power is set as Rev.A\n", __func__);
			imx135_s_ctrl.power_setting_array.power_setting = imx135_power_setting_rev_a;
			imx135_s_ctrl.power_setting_array.size = ARRAY_SIZE(imx135_power_setting_rev_a);
			break;
		case HW_REV_B:
		default:
			CDBG("%s: Sensor power is set as Rev.B\n", __func__);
			imx135_s_ctrl.power_setting_array.power_setting = imx135_power_setting_rev_b;
			imx135_s_ctrl.power_setting_array.size = ARRAY_SIZE(imx135_power_setting_rev_b);
			break;
	}
/*                                                                            */
#endif //#if defined(CONFIG_MACH_MSM8974_G2_TMO_US) || defined(CONFIG_MACH_MSM8974_G2_VZW) || defined(CONFIG_MACH_MSM8974_G2_SPR)
#endif //                            

	rc = platform_driver_probe(&imx135_platform_driver,
		imx135_platform_probe);
	if (!rc) {
		CDBG("%s: X, rc = %d\n", __func__, rc);
		return rc;
	}
	return i2c_add_driver(&imx135_i2c_driver);
}

static void __exit imx135_exit_module(void)
{
	if (imx135_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx135_s_ctrl);
		platform_driver_unregister(&imx135_platform_driver);
	} else
		i2c_del_driver(&imx135_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t imx135_s_ctrl = {
	.sensor_i2c_client = &imx135_sensor_i2c_client,
/*             
                                                               
                               
 */
//	.power_setting_array.power_setting = imx135_power_setting,
//	.power_setting_array.size = ARRAY_SIZE(imx135_power_setting),
/*                                                                            */
	.msm_sensor_mutex = &imx135_mut,
	.sensor_v4l2_subdev_info = imx135_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx135_subdev_info),
};

module_init(imx135_init_module);
module_exit(imx135_exit_module);
MODULE_DESCRIPTION("imx135");
MODULE_LICENSE("GPL v2");
