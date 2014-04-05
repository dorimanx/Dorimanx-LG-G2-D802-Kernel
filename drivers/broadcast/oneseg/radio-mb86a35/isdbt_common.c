#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/regulator/pmic8058-regulator.h>


#include <asm/io.h>

#include <mach/gpio.h>

#include "isdbt_common.h"

#if 0	// wonjong77.lee [2012-08-22] - isdbt_common.h·Î ÀÌµ¿.
#include "../../../../arch/arm/mach-msm/lge/j1/board-j1.h"

#define DMB_ANT_SEL_P_EAR       	    PM8921_GPIO_PM_TO_SYS(11)
// eric0.kim@lge.com [2012.07.26] - FOR_ACTIVATE_SLEEP_MODE
#define GPIO_ONESEG_INT		       	    PM8921_GPIO_PM_TO_SYS(16)
#endif

static unsigned char isdbt_isr_sig=0;
static struct task_struct *isdbt_kthread = NULL;
static wait_queue_head_t isdbt_isr_wait;
#if 0

static int pm8058_l17_mode = -1;

static int gIsdbtRfMode = ISDBT_DEFAULT_NOTUSE_MODE;

extern int check_ear_state(void);

struct regulator *pm8058_l17; // for ISDB-T 2.8V

static int power_set_for_pm8058_l17(unsigned char onoff)
{
	int rc = -EINVAL;

	if(!pm8058_l17) {
		pm8058_l17 = regulator_get(NULL, "8058_l17");
		if (IS_ERR(pm8058_l17)) {
			pr_err("%s: line: %d, vreg_get failed (%ld)\n",
			__func__, __LINE__, PTR_ERR(pm8058_l17));
			rc = PTR_ERR(pm8058_l17);
			return rc;
		}
	}
	if (onoff)
	{
		rc = regulator_set_voltage(pm8058_l17, 2850000, 2850000);
		if (rc) {
			pr_err("%s: line: %d, unable to set pm8058_l17 voltage to 2.8 V\n",__func__,__LINE__);
			goto vreg_l17_fail;
		}
		rc = regulator_enable(pm8058_l17);
		if (rc) {
			pr_err("%s: line: %d, vreg_enable failed %d\n", __func__, __LINE__, rc);
			goto vreg_l17_fail;
		}

		pm8058_l17_mode = 0;
	}
	else
	{
		if(pm8058_l17_mode == 0)
		{
			rc = regulator_disable(pm8058_l17);
			if (rc) {
				pr_err("%s: line: %d, vreg_disable failed %d\n",__func__, __LINE__, rc);
				goto vreg_l17_fail;
			}

			pm8058_l17_mode = -1;
		}
	}
	printk(KERN_INFO "%s: line: %d\n", __func__, __LINE__);
	return 0;

vreg_l17_fail:
	regulator_put(pm8058_l17);
	pm8058_l17 = NULL;
	return rc;

}
#endif
#if 1
static irqreturn_t isdbt_irq(int irq, void *dev_id)
{
	if(isdbt_kthread)
	{
		//isdbt_isr_sig=1;
		isdbt_isr_sig++;
		//wake_up_interruptible(&isdbt_isr_wait);
		//2011-09-29 hyewon.eum get it from mtv250

		printk("isdbt_irq");
		wake_up(&isdbt_isr_wait);
	}

	return IRQ_HANDLED;
}
#endif
#if 1
static int isdbt_thread(void *hDeviceInfo)
{
	set_user_nice(current, -20);// taew00k.kang 2011-08-30 set priority highest
	
	printk("isdbt_kthread enter\n");

	init_waitqueue_head(&isdbt_isr_wait); //0929
	
	while(1)
	{
		wait_event_interruptible(isdbt_isr_wait, isdbt_isr_sig || kthread_should_stop());
		
		//isdbt_isr_sig=0;
		
		if(isdbt_isr_sig>0)
		{
			isdbt_isr_sig--;
		}
	
		if (kthread_should_stop())
			break;
	}

	printk("isdbt_kthread exit\n");

	return 0;
}

#endif
#if 1 // eric0.kim@lge.com [2012.07.26] - FOR_ACTIVATE_SLEEP_MODE
static unsigned oneseg_config_isr[] = {
	GPIO_CFG(GPIO_MB86A35S_SPIS_XIRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_MB86A35S_SPIS_XIRQ, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
};
#else
static unsigned dmb_config_isr[] = {
	GPIO_CFG(GPIO_MB86A35S_SPIS_XIRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_MB86A35S_SPIS_XIRQ, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA)
};
#endif
void isdbt_hw_setting(void)
{
	gpio_request(GPIO_ISDBT_PWR_EN, "ISDBT_EN");
	udelay(50);
	gpio_direction_output(GPIO_ISDBT_PWR_EN, 0);

	gpio_request(GPIO_ISDBT_RST, "ISDBT_RST");
	udelay(50);
	gpio_direction_output(GPIO_ISDBT_RST, 0);

	//gpio_request(GPIO_ISDBT_ANT_SELECT, "ISDBT_ANT_SEL");
	udelay(50);
	// gpio_direction_output(GPIO_ISDBT_ANT_SELECT, 1);

	gpio_request(DMB_ANT_SEL_P_EAR, "DMB_ANT_SEL_P");

	// gpio_set_value_cansleep(DMB_ANT_SEL_P_EAR, 1); /*PMIC Extended GPIO */

	//gpio_tlmm_config(dmb_config_isr[1],GPIO_CFG_ENABLE);

	
	//gpio_request(GPIO_MB86A35S_SPIS_XIRQ, "ISDBT_ISR");

	//gpio_direction_input(GPIO_MB86A35S_SPIS_XIRQ);

	gpio_set_value_cansleep(DMB_ANT_SEL_P_EAR, 1);
	
	printk("isdbt_hw_init \n");
	//gpio_direction_input(GPIO_MB86A35S_SPIS_XIRQ);
	//udelay(50);
#if 1 //eric0.kim@lge.com [2012.07.26] - FOR_ACTIVATE_SLEEP_MODE
	gpio_direction_output(GPIO_ONESEG_INT, false);
	gpio_set_value(GPIO_ONESEG_INT, 0);
#else
	//gpio_direction_input(GPIO_ISDBT_IRQ);
#endif
	//power_set_for_pm8058_l17(0);
}
EXPORT_SYMBOL(isdbt_hw_setting);


//POWER_ON & HW_RESET & INTERRUPT_CLEAR
void isdbt_hw_init(void)
{
	printk("isdbt_hw_init \n");
#if 1 // eric0.kim@lge.com [2012.07.26] - FOR_ACTIVATE_SLEEP_MODE
      gpio_tlmm_config(oneseg_config_isr[0],GPIO_CFG_ENABLE);
	gpio_direction_input(GPIO_MB86A35S_SPIS_XIRQ);
#else

      //gpio_tlmm_config(dmb_config_isr[0],GPIO_CFG_ENABLE);
	//udelay(10);
	//gpio_direction_input(GPIO_MB86A35S_SPIS_XIRQ);
#endif
	//power_set_for_pm8058_l17(1);
	
	gpio_set_value(GPIO_ISDBT_PWR_EN, 1);
	
	udelay(10);
	gpio_set_value(GPIO_ISDBT_RST, 1);
	udelay(10);
	gpio_set_value(GPIO_ISDBT_RST, 0);
	mdelay(10);
	gpio_set_value(GPIO_ISDBT_RST, 1);

	udelay(10);
	// gpio_set_value(GPIO_ISDBT_ANT_SELECT, 1);
	gpio_set_value_cansleep(DMB_ANT_SEL_P_EAR, 1);

	mdelay(2); //need to wait for 1.5ms in mb86a35s 
}
EXPORT_SYMBOL(isdbt_hw_init);

//POWER_OFF
void isdbt_hw_deinit(void)
{
	gpio_set_value(GPIO_ISDBT_RST, 0);
	mdelay(10);
	gpio_set_value(GPIO_ISDBT_PWR_EN, 0);
	
	//power_set_for_pm8058_l17(0);
       //gpio_tlmm_config(dmb_config_isr[1],GPIO_CFG_ENABLE);
	udelay(10);
#if 1 // eric0.kim@lge.com [2012.07.26] - FOR_ACTIVATE_SLEEP_MODE
	gpio_tlmm_config(oneseg_config_isr[1],GPIO_CFG_ENABLE);
	gpio_direction_output(GPIO_MB86A35S_SPIS_XIRQ, false);
	gpio_set_value(GPIO_MB86A35S_SPIS_XIRQ, 0);
#else
	//gpio_direction_output(GPIO_MB86A35S_SPIS_XIRQ, false);
	//gpio_set_value(GPIO_MB86A35S_SPIS_XIRQ, 0);
#endif
	//gpio_set_value(GPIO_ISDBT_ANT_SELECT, 0);
	gpio_set_value_cansleep(DMB_ANT_SEL_P_EAR, 0);

	printk("isdbt_hw_deinit \n");
}

EXPORT_SYMBOL(isdbt_hw_deinit);


int isdbt_init(void* dev)
{
	
	s32 res = 0;
	struct spi_device *spi_dev;

	printk("isdbt_init\n");
	
	isdbt_hw_setting();

	spi_dev = (struct spi_device *)dev;

	res = request_irq(MSM_GPIO_TO_INT(GPIO_ISDBT_IRQ), isdbt_irq, IRQF_DISABLED | IRQF_TRIGGER_FALLING, 
		spi_dev->dev.driver->name, dev);

	//res = request_irq(MSM_GPIO_TO_INT(GPIO_MB86A35S_SPIS_XIRQ), isdbt_irq, IRQF_DISABLED | IRQF_TRIGGER_FALLING, 
	//	spi_dev->dev.driver->name, dev);
	
	if(res) 
	{
		printk("dmb rquest irq fail : %d\n", res);
	}

	if (!isdbt_kthread)
	{
		
		printk("kthread run\n");
		isdbt_kthread = kthread_run(isdbt_thread, NULL, "isdbt_thread");
	}

	return 0;
}

EXPORT_SYMBOL(isdbt_init);

