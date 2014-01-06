
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "tcpal_os.h"
#include "tcc353x_hal.h"

#ifndef _MODEL_F9J_
#define INCLUDE_LGE_SRC_EAR_ANT_SEL
#endif

#ifdef _MODEL_F9J_
#include "../../../../arch/arm/mach-msm/lge/f9j/board-f9j.h"	/* PM8921_GPIO_PM_TO_SYS() */
#endif

#ifndef _MODEL_F9J_
#define ISDB_EN			85 	/* GPIO 85 */
#endif
#define ISDB_INT_N		77 	/* GPIO 77 */
#define ISDB_RESET_N  		1 	/* GPIO 1 */

#ifdef INCLUDE_LGE_SRC_EAR_ANT_SEL
#define ONESEG_EAR_ANT_SEL_P	PM8921_GPIO_PM_TO_SYS(11)	/* Internel/Ear antenna switch */
#endif

#ifdef _MODEL_F9J_
static struct regulator *reg_s4;	//vdd 1.8V
static int pm8921_s4_mode = -1;

static struct regulator *reg_l10;	//vdd 2.8V
static int pm8921_l10_mode = -1;

static struct regulator *reg_l29;	//1.8v
static int pm8921_l29_mode = -1;

static int power_set_for_pm8921_s4(unsigned char onoff)
{
	int rc = -EINVAL;

	if(!reg_s4) {
		reg_s4 = regulator_get(NULL, "8921_s4");
		if (IS_ERR(reg_s4)) {
			pr_err("%s: line: %d, vreg_get failed (%ld)\n",
			__func__, __LINE__, PTR_ERR(reg_s4));
			rc = PTR_ERR(reg_s4);
			return rc;
		}
	}
	if (onoff)
	{
		rc = regulator_set_voltage(reg_s4, 1800000, 1800000);
		if (rc) {
			pr_err("%s: line: %d, unable to set pm8921_s4 voltage to 1.8 V\n",__func__,__LINE__);
			goto vreg_s4_fail;
		}
		rc = regulator_enable(reg_s4);
		if (rc) {
			pr_err("%s: line: %d, vreg_enable failed %d\n", __func__, __LINE__, rc);
			goto vreg_s4_fail;
		}

		pm8921_s4_mode = 0;
	}
	else
	{
		if(pm8921_s4_mode == 0)
		{
			rc = regulator_disable(reg_s4);
			if (rc) {
				pr_err("%s: line: %d, vreg_disable failed %d\n",__func__, __LINE__, rc);
				goto vreg_s4_fail;
			}

			pm8921_s4_mode = -1;
		}
	}
	printk(KERN_INFO "%s: line: %d\n", __func__, __LINE__);
	return 0;

vreg_s4_fail:
	regulator_put(reg_s4);
	reg_s4 = NULL;
	pm8921_s4_mode = -1;
	return rc;

}

static int power_set_for_pm8921_l10(unsigned char onoff)
{
	int rc = -EINVAL;

	if(!reg_l10) {
		reg_l10 = regulator_get(NULL, "8921_l10");
		if (IS_ERR(reg_l10)) {
			pr_err("%s: line: %d, vreg_get failed (%ld)\n",
			__func__, __LINE__, PTR_ERR(reg_l10));
			rc = PTR_ERR(reg_l10);
			return rc;
		}
	}
	if (onoff)
	{
		rc = regulator_set_voltage(reg_l10, 2800000, 2800000);
		if (rc) {
			pr_err("%s: line: %d, unable to set pm8921_l10 voltage to 2.8 V\n",__func__,__LINE__);
			goto vreg_l10_fail;
		}
		rc = regulator_enable(reg_l10);
		if (rc) {
			pr_err("%s: line: %d, vreg_enable failed %d\n", __func__, __LINE__, rc);
			goto vreg_l10_fail;
		}

		pm8921_l10_mode = 0;
	}
	else
	{
		if(pm8921_l10_mode == 0)
		{
			rc = regulator_disable(reg_l10);
			if (rc) {
				pr_err("%s: line: %d, vreg_disable failed %d\n",__func__, __LINE__, rc);
				goto vreg_l10_fail;
			}

			pm8921_l10_mode = -1;
		}
	}
	printk(KERN_INFO "%s: line: %d\n", __func__, __LINE__);
	return 0;

vreg_l10_fail:
	regulator_put(reg_l10);
	reg_l10 = NULL;
	pm8921_l10_mode = -1;
	return rc;

}

static int power_set_for_pm8921_l29(unsigned char onoff)
{
	int rc = -EINVAL;

	if(!reg_l29) {
		reg_l29 = regulator_get(NULL, "8921_l29");
		if (IS_ERR(reg_l29)) {
			pr_err("%s: line: %d, vreg_get failed (%ld)\n",
			__func__, __LINE__, PTR_ERR(reg_l29));
			rc = PTR_ERR(reg_l29);
			return rc;
		}
	}
	if (onoff)
	{
		rc = regulator_set_voltage(reg_l29, 1800000, 1800000);
		if (rc) {
			pr_err("%s: line: %d, unable to set pm8921_l29 voltage to 1.8 V\n",__func__,__LINE__);
			goto vreg_l29_fail;
		}
		rc = regulator_enable(reg_l29);
		if (rc) {
			pr_err("%s: line: %d, vreg_enable failed %d\n", __func__, __LINE__, rc);
			goto vreg_l29_fail;
		}

		pm8921_l29_mode = 0;
	}
	else
	{
		if(pm8921_l29_mode == 0)
		{
			rc = regulator_disable(reg_l29);
			if (rc) {
				pr_err("%s: line: %d, vreg_disable failed %d\n",__func__, __LINE__, rc);
				goto vreg_l29_fail;
			}

			pm8921_l29_mode = -1;
		}
	}
	printk(KERN_INFO "%s: line: %d\n", __func__, __LINE__);
	return 0;

vreg_l29_fail:
	regulator_put(reg_l29);
	reg_l29 = NULL;
	pm8921_l29_mode = -1;
	return rc;

}


#endif


void TchalInit(void)
{
	gpio_request(ISDB_RESET_N, "ISDB_RESET");
	#ifdef _MODEL_F9J_
	
	#else
	gpio_request(ISDB_EN, "ISDB_EN");
	#endif
	gpio_request(ISDB_INT_N, "ISDB_INT");

#ifdef INCLUDE_LGE_SRC_EAR_ANT_SEL
	/* Internel antenna:OFF, Ear antenna: ON, GPIO11:LOW (Saving power)*/
	gpio_set_value_cansleep(ONESEG_EAR_ANT_SEL_P, 0); /* PMIC Extended GPIO */
#endif

	gpio_direction_output(ISDB_RESET_N, false); 	/* output low */
	#ifdef _MODEL_F9J_
	power_set_for_pm8921_s4(0);
	power_set_for_pm8921_l10(0);
	power_set_for_pm8921_l29(0);
	#else
	gpio_direction_output(ISDB_EN, false); 		/* output low */
	#endif
	gpio_direction_input(ISDB_INT_N); 		/* input */

	TcpalPrintStatus((I08S *)"[%s:%d]\n", __func__, __LINE__);
}

void TchalResetDevice(void)
{
	gpio_set_value(ISDB_RESET_N, 1);		/* high ISDB_RESET_N */
	TcpalmSleep(5);
	gpio_set_value(ISDB_RESET_N, 0);		/* low ISDB_RESET_N */
	TcpalmSleep(5);
	gpio_set_value(ISDB_RESET_N, 1);		/* high ISDB_RESET_N */
	TcpalmSleep(5);

	TcpalPrintStatus((I08S *)"[%s:%d]\n", __func__, __LINE__);
}

void TchalPowerOnDevice(void)
{

#ifdef INCLUDE_LGE_SRC_EAR_ANT_SEL
	/* Internel antenna:ON, Ear antenna: OFF, GPIO11: HIGH (Default: Use Internel Antenna )*/
	gpio_set_value_cansleep(ONESEG_EAR_ANT_SEL_P, 1); /* PMIC Extended GPIO */
#endif
	#ifndef _MODEL_F9J_
	gpio_direction_output(ISDB_EN, false); 		/* output low */
	#endif
	gpio_direction_output(ISDB_RESET_N, false); 	/* output low */

	#ifdef _MODEL_F9J_
	power_set_for_pm8921_s4(1);
	power_set_for_pm8921_l10(1);
	power_set_for_pm8921_l29(1);
	#else
	gpio_set_value(ISDB_EN, 1);			/* high ISDB_EN */
	#endif
	TcpalmSleep(10);
	TchalResetDevice();
	TchalIrqSetup();

	TcpalPrintStatus((I08S *)"[%s:%d]\n", __func__, __LINE__);
}

void TchalPowerDownDevice(void)
{
	gpio_set_value(ISDB_RESET_N, 0);		/* low ISDB_RESET_N */
	TcpalmSleep(5);
	#ifdef _MODEL_F9J_
	power_set_for_pm8921_s4(0);
	power_set_for_pm8921_l10(0);
	power_set_for_pm8921_l29(0);
	#else
	gpio_set_value(ISDB_EN, 0);			/* low ISDB_EN */
	#endif

#ifdef INCLUDE_LGE_SRC_EAR_ANT_SEL
	/* Internel antenna:OFF, Ear antenna: ON, GPIO11:LOW (Saving power)*/
	gpio_set_value_cansleep(ONESEG_EAR_ANT_SEL_P, 0); /* PMIC Extended GPIO */
#endif

	TcpalPrintStatus((I08S *)"[%s:%d]\n", __func__, __LINE__);
}

void TchalIrqSetup(void)
{
	gpio_direction_input(ISDB_INT_N);		/* input mode */
}

