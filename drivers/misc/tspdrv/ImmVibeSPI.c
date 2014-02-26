/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
** =========================================================================
*/

#include <linux/types.h>
#include <linux/err.h>
#include <mach/msm_iomap.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <linux/clk.h>

#include <linux/regulator/consumer.h>
#include <linux/i2c.h>

#include <linux/lge_sm100.h>
#include <linux/of_gpio.h>
#include <mach/board_lge.h>

/* When use SM100 with GP_CLK
  175Hz motor : 22.4KHz - M=1, N=214 ,
  205Hz motor : 26.24Khz - M=1, N=183 ,
  230Hz motor : 29.4KHZ - M=1, N=163 ,
  */

#define ImmPorting

#ifdef ImmPorting
#define DEVICE_NAME		"lge_sm100"

static struct clk *cam_gp1_clk;
static void __iomem *virt_bases_v = NULL;
#define MMSS_CC_GP1_CMD_RCGR(x) (void __iomem *)(virt_bases_v + (x))

#define REG_WRITEL(value, reg)		writel(value, reg)
#define REG_READL(reg)			readl(reg)

#define MMSS_CC_PWM_SET		0xFD8C3450
#define GCC_GP1_PWM_SET		0xFC401900

#define MMSS_CC_PWM_SIZE	SZ_1K

#define GPIO_LIN_MOTOR_EN 60
#define GPIO_LIN_MOTOR_PWM 27
/*#if defined(CONFIG_MACH_MSM8974_VU3_KR)
#define GPIO_LIN_MOTOR_PWR 145
#endif   */

#define GP_CLK_ID				0 /* gp clk 0 */
#define GP_CLK_M_DEFAULT		1
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
#define GP_CLK_N_DEFAULT		110
#else
#define GP_CLK_N_DEFAULT		92
#endif
#define GP_CLK_D_MAX			GP_CLK_N_DEFAULT
#define GP_CLK_D_HALF			(GP_CLK_N_DEFAULT >> 1)

static int mmss_cc_n_default;
static int mmss_cc_d_max;
static int mmss_cc_d_half;
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
VibeInt8 previous_nForce=0;
#endif

struct timed_vibrator_data {
	atomic_t gp1_clk_flag;
	int amp;
	int vibe_n_value;
	int haptic_en_gpio;
/*#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	int haptic_ldo_gpio;
#endif  */
	int motor_pwm_gpio;
	int vpwr_on;
	struct regulator *vreg_l21;
};
struct timed_vibrator_data vib;
static DEFINE_MUTEX(vib_lock);

#ifdef CONFIG_OF
static void vibrator_parse_dt(struct device *dev, struct timed_vibrator_data *vib_data)
{
	struct device_node *np = dev->of_node;

	of_property_read_u32(np, "syncoam,vpwr-on", &vib_data->vpwr_on);
	INFO_MSG("[sm100] vib->vpwr_on : %d!!\n", vib_data->vpwr_on);

	vib_data->haptic_en_gpio = of_get_named_gpio_flags(np, "syncoam,haptic-pwr-gpio", 0, NULL);
	vib_data->motor_pwm_gpio = of_get_named_gpio_flags(np, "syncoam,motor-pwm-gpio", 0, NULL);
/*#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	vib_data->haptic_ldo_gpio = of_get_named_gpio_flags(np, "syncoam,haptic-ldo-gpio", 0, NULL);
#endif  */

	of_property_read_u32(np, "syncoam,motor-amp", &vib_data->amp);
	of_property_read_u32(np, "syncoam,n-value", &vib_data->vibe_n_value);

	INFO_MSG("[sm100] gpio en : %d, pwm : %d, amp : %d, n_value : %d\n",
		   vib_data->haptic_en_gpio, vib_data->motor_pwm_gpio,
		   vib_data->amp, vib_data->vibe_n_value);
}

static struct of_device_id sm100_match_table[] = {
    { .compatible = "syncoam,sm100",},
    { },
};
#endif

static int vibrator_pwm_set(int enable, int amp, int n_value)
{
	/* TODO: set clk for amp */
	//uint M_VAL = GP_CLK_M_DEFAULT;
	//uint D_VAL = GP_CLK_D_MAX;
	//uint D_INV = 0;                 /* QCT support invert bit for msm8960 */

	//amp = (amp + 127)/2;
	//d_val = ((MMSS_CC_N_DEFAULT * amp) /*>> 7*/);
	uint d_val;
	//d_val = MMSS_CC_D_HALF + (MMSS_CC_N_DEFAULT-1)*amp/256;
	d_val = mmss_cc_d_half + (mmss_cc_n_default-1)*amp/256;

	if (virt_bases_v == NULL)
		virt_bases_v = ioremap(MMSS_CC_PWM_SET, MMSS_CC_PWM_SIZE);

	//DbgOut("amp=%d, n_value=%d\n", amp, n_value);

	if (enable) {
		REG_WRITEL(
			((~(d_val << 1)) & 0xffU),	/* D[7:0] */
			MMSS_CC_GP1_CMD_RCGR(0x10));
		REG_WRITEL(
			(1 << 1U) +	/* ROOT_EN[1] */
			(1),		/* UPDATE[0] */
			MMSS_CC_GP1_CMD_RCGR(0));
	} else {
		REG_WRITEL(
			(0 << 1U) +	/* ROOT_EN[1] */
			(0),		/* UPDATE[0] */
			MMSS_CC_GP1_CMD_RCGR(0));
	}


#if 0
	if (enable) {
		D_VAL = ((GP_CLK_D_MAX * amp) >> 7);
		if (D_VAL > GP_CLK_D_HALF) {
			if (D_VAL == GP_CLK_D_MAX) {      /* Max duty is 99% */
				D_VAL = 2;
			} else {
				D_VAL = GP_CLK_D_MAX - D_VAL;
			}
			D_INV = 1;
		}

		REG_WRITEL(
			(((M_VAL & 0xffU) << 16U) + /* M_VAL[23:16] */
			((~(D_VAL << 1)) & 0xffU)),  /* D_VAL[7:0] */
			GPn_MD_REG(GP_CLK_ID));

		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(1U << 11U) +  /* CLK_ROOT_ENA[11]  : Enable(1) */
			((D_INV & 0x01U) << 10U) +  /* CLK_INV[10]       : Disable(0) */
			(1U << 9U) +   /* CLK_BRANCH_ENA[9] : Enable(1) */
			(1U << 8U) +   /* NMCNTR_EN[8]      : Enable(1) */
			(0U << 7U) +   /* MNCNTR_RST[7]     : Not Active(0) */
			(2U << 5U) +   /* MNCNTR_MODE[6:5]  : Dual-edge mode(2) */
			(3U << 3U) +   /* PRE_DIV_SEL[4:3]  : Div-4 (3) */
			(5U << 0U)),   /* SRC_SEL[2:0]      : CXO (5)  */
			GPn_NS_REG(GP_CLK_ID));
		//DbgOut("GPIO_LIN_MOTOR_PWM is enable with M=%d N=%d D=%d\n", M_VAL, n_value, D_VAL);
	} else {
		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(0U << 11U) +  /* CLK_ROOT_ENA[11]  : Disable(0) */
			(0U << 10U) +  /* CLK_INV[10]	    : Disable(0) */
			(0U << 9U) +	 /* CLK_BRANCH_ENA[9] : Disable(0) */
			(0U << 8U) +   /* NMCNTR_EN[8]      : Disable(0) */
			(0U << 7U) +   /* MNCNTR_RST[7]     : Not Active(0) */
			(2U << 5U) +   /* MNCNTR_MODE[6:5]  : Dual-edge mode(2) */
			(3U << 3U) +   /* PRE_DIV_SEL[4:3]  : Div-4 (3) */
			(5U << 0U)),   /* SRC_SEL[2:0]      : CXO (5)  */
			GPn_NS_REG(GP_CLK_ID));
		//DbgOut("GPIO_LIN_MOTOR_PWM is disalbe \n");
	}
#endif
	return 0;
}

static int android_vibrator_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		INFO_MSG("[sm100] probe : pdev->dev.of_node\n");
		vibrator_parse_dt(&pdev->dev, &vib);
	}

	if (vib.vpwr_on != 1) {
		if (!(vib.vreg_l21)) {
			vib.vreg_l21 = regulator_get(&pdev->dev, "vdd_ana");
			if (IS_ERR(vib.vreg_l21)) {
				pr_err("%s: regulator get of pm8941_l21 failed (%ld)\n",
						__func__, PTR_ERR(vib.vreg_l21));
				vib.vreg_l21 = NULL;
			}
		}
	}

	pdev->dev.init_name = "vibrator";
	printk("[sm100] dev->init_name : %s, dev->kobj : %s\n",
				pdev->dev.init_name, pdev->dev.kobj.name);

	cam_gp1_clk = clk_get(&pdev->dev, "cam_gp1_clk");
#if defined(CONFIG_MACH_MSM8974_G2_KR)
	if(lge_get_board_revno() >= HW_REV_E) {
		mmss_cc_n_default = 92;		/* for 230Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 29813);
	} else {
		mmss_cc_n_default = 54;		/* for 175Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 22222);
	}
#elif defined(CONFIG_MACH_MSM8974_G2_VZW) || defined(CONFIG_MACH_MSM8974_G2_ATT) || defined(CONFIG_MACH_MSM8974_G2_TEL_AU)
	if(lge_get_board_revno() >= HW_REV_D) {
		mmss_cc_n_default = 92;		/* for 230Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 29813);
	} else {
		mmss_cc_n_default = 54;		/* for 175Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 22222);
	}
#elif defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_SPR) || defined(CONFIG_MACH_MSM8974_G2_TMO_US) || defined(CONFIG_MACH_MSM8974_G2_CA) || defined(CONFIG_MACH_MSM8974_G2_OPEN_COM) || defined(CONFIG_MACH_MSM8974_G2_VDF_COM)
	if(lge_get_board_revno() >= HW_REV_C) {
		mmss_cc_n_default = 92;		/* for 230Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 29813);
	} else {
		mmss_cc_n_default = 54;		/* for 175Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 22222);
	}
#elif defined(CONFIG_MACH_MSM8974_VU3_KR)
		mmss_cc_n_default = 110; 	/* for 230Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 29090);
#elif defined(CONFIG_MACH_MSM8974_Z_KR) || defined(CONFIG_MACH_MSM8974_Z_US)
	if(lge_get_board_revno() >= HW_REV_B) {
		mmss_cc_n_default = 82; 	/* for 230Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 29268);
	} else {
		mmss_cc_n_default = 54;		/* for 175Hz motor */
		mmss_cc_d_max = mmss_cc_n_default;
		mmss_cc_d_half = (mmss_cc_n_default >> 1);
		clk_set_rate(cam_gp1_clk, 22222);
	}
#elif defined(CONFIG_MACH_MSM8974_Z_KDDI)
	mmss_cc_n_default = 82;     /* for 230Hz motor */
	mmss_cc_d_max = mmss_cc_n_default;
	mmss_cc_d_half = (mmss_cc_n_default >> 1);
	clk_set_rate(cam_gp1_clk, 29268);
#else
	mmss_cc_n_default = 54;		/* for 175Hz motor */
	mmss_cc_d_max = mmss_cc_n_default;
	mmss_cc_d_half = (mmss_cc_n_default >> 1);
	clk_set_rate(cam_gp1_clk, 22222);
#endif
	atomic_set(&vib.gp1_clk_flag, 0);

	return 0;
}

static int android_vibrator_remove(struct platform_device *pdev)
{
	return 0;
}

static void android_vibrator_shutdown(struct platform_device *pdev)
{
}

static int android_vibrator_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int android_vibrator_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver android_vibrator_driver = {
	.probe = android_vibrator_probe,
	.remove = android_vibrator_remove,
	.shutdown = android_vibrator_shutdown,
	.suspend = android_vibrator_suspend,
	.resume = android_vibrator_resume,
	.driver = {
		.name = DEVICE_NAME,
#ifdef CONFIG_OF
		.of_match_table = sm100_match_table,
#endif
	},
};
#endif

#if 0
#define REG_WRITEL(value, reg)		writel(value, (MSM_CLK_CTL_BASE+reg))
#define REG_READL(reg)			readl((MSM_CLK_CTL_BASE+reg))

#define GPn_MD_REG(n)                           (0x2D00+32*(n))
#define GPn_NS_REG(n)                           (0x2D24+32*(n))
#endif

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

#define PWM_DUTY_MAX    579 /* 13MHz / (579 + 1) = 22.4kHz */

static bool g_bAmpEnabled = false;


/*#ifdef CONFIG_OF

#endif*/

/* Helper functions for SM100 */
static int vibrator_power_set(int enable, struct timed_vibrator_data *vib_data)
{
/*#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	int gpio;

	INFO_MSG("pwr_enable=%d\n", enable);

	if (enable)
		gpio_direction_output(vib_data->haptic_ldo_gpio, 1);
	else
		gpio_direction_output(vib_data->haptic_ldo_gpio, 0);
	gpio = gpio_get_value(vib_data->haptic_ldo_gpio);
	INFO_MSG("Haptic_LDO_GPIO Value : %d\n", gpio);

#else   */
	int rc;

	INFO_MSG("pwr_enable=%d\n", enable);

	mutex_lock(&vib_lock);
	if (vib_data->vpwr_on != 1) {
		if (enable) {
			rc = regulator_enable(vib_data->vreg_l21);
			if (rc < 0)
				pr_err("%s: regulator_enable failed\n", __func__);
		} else {
			if (regulator_is_enabled(vib_data->vreg_l21) > 0) {
				rc = regulator_disable(vib_data->vreg_l21);
				if (rc < 0)
					pr_err("%s: regulator_disable failed\n", __func__);
			}
		}
	}
	mutex_unlock(&vib_lock);
/*#endif */
	return 0;
}

static int vibrator_ic_enable_set(int enable, struct timed_vibrator_data *vib_data)
{
	int gpio;

	INFO_MSG("ic_enable=%d\n", enable);

	if (enable)
		gpio_direction_output(vib_data->haptic_en_gpio, 1);
	else
		gpio_direction_output(vib_data->haptic_en_gpio, 0);

	gpio = gpio_get_value(vib_data->haptic_en_gpio);
	INFO_MSG("Haptic_EN_GPIO Value : %d\n", gpio);

	return 0;
}


/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{

    if (g_bAmpEnabled)
    {

        DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_AmpDisable.\n"));

        vibrator_ic_enable_set(0, &vib);
        vibrator_pwm_set(0, 0, GP_CLK_N_DEFAULT);
        vibrator_power_set(0, &vib);

	if (atomic_read(&vib.gp1_clk_flag) == 1) {
		clk_disable_unprepare(cam_gp1_clk);
		atomic_set(&vib.gp1_clk_flag, 0);
	}

        g_bAmpEnabled = false;
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
		previous_nForce=0;
#endif

    }

    return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
    if (!g_bAmpEnabled)
    {
        DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_AmpEnable.\n"));

	if (atomic_read(&vib.gp1_clk_flag) == 0) {
		clk_prepare_enable(cam_gp1_clk);
		atomic_set(&vib.gp1_clk_flag, 1);
	}

        vibrator_power_set(1, &vib);
        //vibrator_pwm_set(1, 0, GP_CLK_N_DEFAULT);
        vibrator_ic_enable_set(1, &vib);

        g_bAmpEnabled = true;
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
		previous_nForce=127;
#endif

    }

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{

	int rc;
#ifdef ImmPorting
	rc = platform_driver_register(&android_vibrator_driver);
#endif
    DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Initialize.\n"));

	/* GPIO setting for PWR Motor EN in msm8960 */
	/*rc = gpio_request(GPIO_LIN_MOTOR_PWR, "lin_motor_pwr_en");
	if (rc) {
		//DbgOut("GPIO_LIN_MOTOR_PWR %d request failed\n",	GPIO_LIN_MOTOR_PWR);
	return VIBE_E_FAIL;
	}*/


	/* GPIO setting for Motor EN in pmic8921 */
	rc = gpio_request(vib.haptic_en_gpio, "lin_motor_en");
	if (rc) {
		printk("GPIO_LIN_MOTOR_EN %d request failed\n", vib.haptic_en_gpio);
		return VIBE_E_FAIL;
	}

	/* gpio init */
	rc = gpio_request(vib.motor_pwm_gpio, "lin_motor_pwm");
	if (unlikely(rc < 0)) {
		printk("not able to get gpio %d\n", vib.motor_pwm_gpio);
		return VIBE_E_FAIL;
	}
//#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	/* GPIO setting for Motor LDO in VU3 */
	//rc = gpio_request(vib.haptic_ldo_gpio, "lin_motor_ldo_en");
	//if (rc) {
	//	printk("GPIO_LIN_MOTOR_PWR %d request failed\n", vib.haptic_ldo_gpio);
	//	return VIBE_E_FAIL;
	//}
//#endif
	/* GPIO setting for Motor EN in msm8960 */
	/*rc = gpio_request(GPIO_LIN_MOTOR_EN, "lin_motor_en");
	if (rc) {
		printk("GPIO_LIN_MOTOR_EN %d request failed\n", GPIO_LIN_MOTOR_EN);
		return VIBE_E_FAIL;
	}*/

	/* gpio init */
	/*rc = gpio_request(GPIO_LIN_MOTOR_PWM, "lin_motor_pwm");
	if (unlikely(rc < 0))
		printk("GPIO_LIN_MOTOR_PWM %d request failed\n", GPIO_LIN_MOTOR_PWM);
	*/

	vibrator_ic_enable_set(0, &vib);
	vibrator_pwm_set(0, 0, GP_CLK_N_DEFAULT);
	vibrator_power_set(0, &vib);

    g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */

    /*
    ** Disable amp.
    ** If multiple actuators are supported, please make sure to call
    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
    ** input argument).
    */
    ImmVibeSPI_ForceOut_AmpDisable(0);

    return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
    DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Terminate.\n"));

    /*
    ** Disable amp.
    ** If multiple actuators are supported, please make sure to call
    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
    ** input argument).
    */
    ImmVibeSPI_ForceOut_AmpDisable(0);
#ifdef ImmPorting
    platform_driver_unregister(&android_vibrator_driver);
#endif
    return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set PWM duty cycle
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{

    VibeInt8 nForce;

    switch (nOutputSignalBitDepth)
    {
        case 8:
            /* pForceOutputBuffer is expected to contain 1 byte */
            if (nBufferSizeInBytes != 1) return VIBE_E_FAIL;

            nForce = pForceOutputBuffer[0];
            break;
        case 16:
            /* pForceOutputBuffer is expected to contain 2 byte */
            if (nBufferSizeInBytes != 2) return VIBE_E_FAIL;

            /* Map 16-bit value to 8-bit */
            nForce = ((VibeInt16*)pForceOutputBuffer)[0] >> 8;
            break;
        default:
            /* Unexpected bit depth */
            return VIBE_E_FAIL;
    }
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	if(nForce==previous_nForce)
		return VIBE_S_SUCCESS;
	previous_nForce=nForce;
#endif
    if (nForce == 0)
    {
        //vibrator_pwm_set(1, 0, GP_CLK_N_DEFAULT);
        ImmVibeSPI_ForceOut_AmpDisable(nActuatorIndex);

    }
    else
    {
        ImmVibeSPI_ForceOut_AmpEnable(nActuatorIndex);
        vibrator_pwm_set(1, nForce, GP_CLK_N_DEFAULT);
    }

    return VIBE_S_SUCCESS;
}

#if 0
/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
    /* This function is not called for ERM device */

    return VIBE_S_SUCCESS;
}
#endif

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{

    if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

    DbgOut((KERN_DEBUG "ImmVibeSPI_Device_GetName.\n"));
#ifdef ImmPorting
    strncpy(szDevName, "LGE A1", nSize-1);
    szDevName[nSize - 1] = '\0';    /* make sure the string is NULL terminated */
#endif
    return VIBE_S_SUCCESS;
}
