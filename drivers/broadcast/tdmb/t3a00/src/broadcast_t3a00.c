#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/pm.h>

#include <linux/spi/spi.h>      /* HOST Interface SPI */
#include <linux/spi/spidev.h> 	/* HOST Interface SPI */
#include <linux/interrupt.h>    /* HOST Interface SPI */

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h> 	/* wake_lock, unlock */

#include "../../broadcast_tdmb_drv_ifdef.h"
#include "../inc/broadcast_t3a00.h"

#include <linux/pm_qos.h> // FEATURE_DMB_USE_PM_QOS
#include <linux/err.h>
#include <linux/of_gpio.h>

#include <linux/clk.h>
#include <mach/msm_bus.h> // FEATURE_DMB_USE_BUS_SCALE

#include <mach/board_lge.h>

/* pmic regulator control */
#include <linux/regulator/consumer.h>
#define FEATURE_DMB_USE_XO		/* XO_OUT_A2 */
#define FEATURE_DMB_USE_BUS_SCALE 

/* SPI Data read using workqueue */
//#define FEATURE_DMB_USE_WORKQUEUE
#define FEATURE_DMB_USE_PM_QOS

/* VU3 DMB GPIOs */
#define DMB_INT_N           77	/* IRQ, IN, LOW_ACTIVE */			
#define DMB_EN              76    /* PWR_EN, OUT, HIGH_ACTIVE */
#define DMB_RESET_N         75     /* RESET, OUT, LOW_ACTIVE */

/* t3a00 driver contrl block */
struct tdmb_t3a00_ctrl_blk 
{
	int 						is_power_on;
	struct spi_device* 			spi_ptr;
#ifdef FEATURE_DMB_USE_WORKQUEUE
	struct work_struct 			spi_work;
	struct workqueue_struct* 	spi_wq;
#endif
	struct mutex				mutex;
	struct wake_lock 			wake_lock;	/* wake_lock,wake_unlock */
	boolean 					irq_status;
	spinlock_t					spin_lock;
#ifdef FEATURE_DMB_USE_XO
	struct clk					*clk;
#endif
#ifdef FEATURE_DMB_USE_BUS_SCALE
	struct platform_device		*pdev;
	struct msm_bus_scale_pdata	*bus_scale_pdata;
	u32							bus_scale_client_id; 
#endif
#ifdef FEATURE_DMB_USE_PM_QOS
	struct pm_qos_request    pm_req_list;
#endif
};

static struct tdmb_t3a00_ctrl_blk t3a00_ctrl_info;
static uint32 user_stop_flg = 0;

static struct regulator *pm8941_power_reg = NULL;

/* ============================================================== */
/*  Internal Functions                                                                                                                */
/* ============================================================== */
static int broadcast_t3a00_probe(struct spi_device *spi);
static int broadcast_t3a00_remove(struct spi_device *spi);
static int broadcast_t3a00_suspend(struct spi_device *spi, pm_message_t mesg);
static int broadcast_t3a00_resume(struct spi_device *spi);

/* For Use VDDIO 1.8V Power */
static int power_set_pm8941_regulator(int onoff);

void tdmb_t3a00_set_userstop(int mode)
{
	user_stop_flg = mode;
}

int tdmb_t3a00_mdelay(int32 ms)
{
	int		rc = 1;  /* 0 : false, 1 : ture */
	int32	wait_loop =0;
	int32	wait_ms = ms;

	if(ms > 100)
	{
		wait_loop = (ms /100);   /* 100, 200, 300 more only , Otherwise this must be modified e.g (ms + 40)/50 */
		wait_ms = 100;
	}

	do
	{
		mdelay(wait_ms);
		if((user_stop_flg == 1) && (wait_loop > 0))
		{
			printk("[TDMB]User-stop flag is set so escape loop ms =(%d)\n", ms);
			rc = 0;
			break;
		}
	}while((--wait_loop) > 0);

	return rc;
}

void tdmb_t3a00_must_mdelay(int32 ms)
{
	mdelay(ms);
}

int tdmb_t3a00_power_on(void)
{
#ifdef FEATURE_DMB_USE_XO
	int rc = FALSE;
#endif
	printk("tdmb_t3a00_power_on \n");
	if ( t3a00_ctrl_info.is_power_on == FALSE )
	{
#ifdef FEATURE_DMB_USE_BUS_SCALE
		msm_bus_scale_client_update_request(t3a00_ctrl_info.bus_scale_client_id, 1); /* expensive call, index:1 is the <84 512 3000 152000> entry */ 
#endif
#ifdef FEATURE_DMB_USE_XO//내부클락 
		if(t3a00_ctrl_info.clk != NULL) {
			rc = clk_prepare_enable(t3a00_ctrl_info.clk);
			if (rc) {
				dev_err(&t3a00_ctrl_info.spi_ptr->dev, "could not enable clock\n");
				return rc;
			}
		}
#endif
#ifdef FEATURE_DMB_USE_PM_QOS
		if(pm_qos_request_active(&t3a00_ctrl_info.pm_req_list)) {
			pm_qos_update_request(&t3a00_ctrl_info.pm_req_list, 20);
		}
#endif
		wake_lock(&t3a00_ctrl_info.wake_lock);
		
		/* t3a00 Power On Sequence */
		/* PMU Enable and RESET Pin Low */
		gpio_set_value(DMB_EN, 0);
		gpio_set_value(DMB_RESET_N, 0);
		udelay(500);

		/* VDD IO 1.8V Enable */
		power_set_pm8941_regulator(1);
		udelay(500);
		
		gpio_set_value(DMB_EN, 1);
		udelay(500);  /* PMIC Clock is already on */

		gpio_set_value(DMB_RESET_N, 1);
		udelay(500);

		gpio_set_value(DMB_RESET_N, 0);
		udelay(500);

		gpio_set_value(DMB_RESET_N, 1);
		udelay(50);

		tdmb_t3a00_interrupt_free();
		t3a00_ctrl_info.is_power_on = TRUE;

	}
	else
	{
		printk("tdmb_t3a00_power_on the power already turn on \n");
	}

	printk("tdmb_t3a00_power_on completed \n");

	return TRUE;

}

int tdmb_t3a00_power_off(void)
{
	if ( t3a00_ctrl_info.is_power_on == TRUE )
	{
		tdmb_t3a00_interrupt_lock();

		t3a00_ctrl_info.is_power_on = FALSE;

		gpio_set_value(DMB_RESET_N, 0);
		udelay(10);
		gpio_set_value(DMB_EN, 0);
		udelay(100);

		/* VDD IO 1.8V Disable */
		power_set_pm8941_regulator(0);

		udelay(200);
		wake_unlock(&t3a00_ctrl_info.wake_lock);

#ifdef FEATURE_DMB_USE_XO
		if(t3a00_ctrl_info.clk != NULL) {
			clk_disable_unprepare(t3a00_ctrl_info.clk);
		}
#endif
#ifdef FEATURE_DMB_USE_BUS_SCALE 
		msm_bus_scale_client_update_request(t3a00_ctrl_info.bus_scale_client_id, 0); /* expensive call, index:0 is the <84 512 0 0> entry */
#endif
#ifdef FEATURE_DMB_USE_PM_QOS
		if(pm_qos_request_active(&t3a00_ctrl_info.pm_req_list)) {
			pm_qos_update_request(&t3a00_ctrl_info.pm_req_list, PM_QOS_DEFAULT_VALUE);
		}
#endif
	}
	else
	{
		printk("tdmb_t3a00_power_on the power already turn off \n");
	}	

	printk("tdmb_t3a00_power_off completed \n");
		
	return TRUE;
}

int tdmb_t3a00_select_antenna(unsigned int sel)
{
	return FALSE;
}

static struct of_device_id tdmb_spi_table[] = {
	{
		.compatible = "inc,tdmb-t3a00",
	},
	{}
};

static struct spi_driver broadcast_tdmb_driver = {
	.probe = broadcast_t3a00_probe,
	.remove = __devexit_p(broadcast_t3a00_remove),
	.suspend = broadcast_t3a00_suspend,
	.resume  = broadcast_t3a00_resume,
	.driver = {
		.name = "tdmb-t3a00",
		.of_match_table = tdmb_spi_table,
		.bus	= &spi_bus_type,
		.owner = THIS_MODULE,
	},
};

void tdmb_t3a00_interrupt_lock(void)
{
	if (t3a00_ctrl_info.spi_ptr == NULL)
	{
		printk("tdmb_t3a00_interrupt_lock fail\n");
		return;
	}

	disable_irq(t3a00_ctrl_info.spi_ptr->irq);
	return;
}

void tdmb_t3a00_interrupt_free(void)
{
	if (t3a00_ctrl_info.spi_ptr == NULL)
	{
		printk("tdmb_t3a00_interrupt_free fail\n");
		return;
	}

	enable_irq(t3a00_ctrl_info.spi_ptr->irq);
	return;
}


int tdmb_t3a00_spi_write_read(uint8* tx_data, int tx_length, uint8 *rx_data, int rx_length)
{
	int rc;
	
	struct spi_transfer	t = {
			.tx_buf		= tx_data,
			.rx_buf		= rx_data,
			.len			= tx_length+rx_length,
			//
			//.cs_change = 0
		};
	struct spi_message	m;	

	//printk("tdmb_t3a00_spi_write_read start\n");

	if (t3a00_ctrl_info.spi_ptr == NULL)
	{
		printk("tdmb_t3a00_spi_write_read error txdata=0x%x, length=%d\n", (unsigned int)tx_data, tx_length+rx_length);
		return FALSE;
	}

	mutex_lock(&t3a00_ctrl_info.mutex);

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	rc = spi_sync(t3a00_ctrl_info.spi_ptr, &m);
	//printk("rc = %d in tdmb_t3a00_spi_write_read\n", rc);

	if ( rc < 0 )
	{
		printk("tdmb_t3a00_spi_read_burst result(%d), actual_len=%d\n",rc, m.actual_length);
	}

	mutex_unlock(&t3a00_ctrl_info.mutex);


	return TRUE;
}


#if 0
void tdmb_rw_test(void)
{
	unsigned short i = 0;
	unsigned short w_val = 0;
	unsigned short r_val = 0;
	unsigned short err_cnt = 0;

	err_cnt = 0;
	for(i=1;i<11;i++)
	{
		w_val = (i%0xFF);
		tdmb_t3a00_spi_write_read( 0x0a00+ 0x05, w_val, 0x0a00+ 0x05, &r_val );
		//tdmb_t3a00_i2c_read16(0x0a00+ 0x05, &r_val );
		if(r_val != w_val)
		{
			err_cnt++;
			printk("w_val:%x, r_val:%x\n", w_val,r_val);
		}
	}
}
#endif

static int power_set_pm8941_regulator(int onoff)
{
	int rc = -1;
	hw_rev_type hw_rev = HW_REV_MAX;

	/* REV_EVB1= 0, REV_EVB2 = 1, REV_A =2, REV_B =3, REV_C = 4 ... */
	hw_rev = lge_get_board_revno();
	
#if defined(CONFIG_MACH_MSM8974_VU3_KR)
	if(hw_rev <= HW_REV_A || hw_rev > HW_REV_C) {
		pm8941_power_reg = NULL;
		printk("[T3A00]VU3 Rev=(%d) does not support PMIC 1.8V\n", hw_rev);
		return rc;
	}
#endif

#if defined(CONFIG_MACH_MSM8974_Z_KR)
	if(hw_rev != HW_REV_C)
	{
		pm8941_power_reg = NULL;
		printk("[T3A00]Z-KOR Rev=(%d) does not support PMIC 1.8V except Rev.C\n", hw_rev);
		return rc;
	}
#endif


	if(!t3a00_ctrl_info.spi_ptr){
		printk("[T3A00]T3A00 Contrl Block invalid\n");
		return rc;
	}

	if(!pm8941_power_reg ) {
		pm8941_power_reg = regulator_get(&t3a00_ctrl_info.spi_ptr->dev, "tdmb-vdd-io");
		if(IS_ERR(pm8941_power_reg)) {
			printk("%s: regulator get of pm8941 power failed in T3A00 (%ld)\n",
				__func__, PTR_ERR(pm8941_power_reg));
				rc = PTR_ERR(pm8941_power_reg);
				pm8941_power_reg = NULL;
				return rc;
		}
	}

	if(onoff){
		rc = regulator_set_voltage(pm8941_power_reg , 1800000, 1800000);
		if(rc < 0 ) {
			printk("[T3A00] %s: cannot control regulator pm8941 1.8V :%d\n", __func__, rc);
			goto vreg_set_fail;
		}
		rc = regulator_enable(pm8941_power_reg);
		if(rc < 0 ) {
			printk("[T3A00] %s: cannot regulator enable %d\n", __func__, rc);
			goto vreg_set_fail;

		}
	}else {
		rc = regulator_disable(pm8941_power_reg);
		if(rc < 0 ) {
			printk("[T3A00] %s: cannot regulator disable %d\n", __func__, rc);
			goto vreg_set_fail;
		}
	}
	printk("[T3A00]pm8941 voltage contorl success mode =(%d)\n", onoff);
	return rc;

vreg_set_fail:
	regulator_put(pm8941_power_reg);
	pm8941_power_reg = NULL;
	return rc;
}


#ifdef FEATURE_DMB_USE_WORKQUEUE
static irqreturn_t broadcast_tdmb_spi_isr(int irq, void *handle)
{
	struct tdmb_t3a00_ctrl_blk* t3a00_info_p;
	unsigned long flag;

	t3a00_info_p = (struct tdmb_t3a00_ctrl_blk *)handle;	
	if ( t3a00_info_p && t3a00_info_p->is_power_on )
	{
		if (t3a00_info_p->irq_status)
		{			
			printk("######### t3a00 spi read funtion is so late skip #########\n");			
			return IRQ_HANDLED;
		}
		spin_lock_irqsave(&t3a00_info_p->spin_lock, flag);
		queue_work(t3a00_info_p->spi_wq, &t3a00_info_p->spi_work);
		spin_unlock_irqrestore(&t3a00_info_p->spin_lock, flag);
	}
	else
	{
		printk("broadcast_tdmb_spi_isr is called, but device is off state\n");
	}

	return IRQ_HANDLED; 
}

static void broacast_tdmb_spi_work(struct work_struct *tdmb_work)
{
	struct tdmb_t3a00_ctrl_blk *t3a00_info_p;

	t3a00_info_p = container_of(tdmb_work, struct tdmb_t3a00_ctrl_blk, spi_work);
	if ( t3a00_info_p )
	{
		t3a00_info_p->irq_status = TRUE;
		broadcast_drv_if_read_data();
		t3a00_info_p->irq_status = FALSE;
	}
	else
	{
		printk("~~~~~~~broadcast_tdmb_spi_work call but t3a00_info_p is NULL ~~~~~~~\n");
	}
}
#else
static irqreturn_t broadcast_tdmb_spi_event_handler(int irq, void *handle)
{
	struct tdmb_t3a00_ctrl_blk *t3a00_info_p;

	t3a00_info_p = (struct tdmb_t3a00_ctrl_blk *)handle;
	if ( t3a00_info_p && t3a00_info_p->is_power_on )
	{
		if (t3a00_info_p->irq_status)
		{
			printk("######### spi read function is so late skip ignore #########\n");
			return IRQ_HANDLED;
		}

		t3a00_info_p->irq_status = TRUE;
		broadcast_drv_if_read_data();
		t3a00_info_p->irq_status = FALSE;
	}
	else
	{
		printk("broadcast_tdmb_spi_isr is called, but device is off state\n");
	}
	return IRQ_HANDLED;
}
#endif

static int tdmb_t3a00_configure_gpios(void)
{
	int rc= 0;

	rc = gpio_request(DMB_EN, "dmb_enable");
	if(rc)
	{
		pr_warning("T3A00 DMB_EN gpio request error\n");
	}
	gpio_direction_output(DMB_EN, 0);  /* output and low */

	rc = gpio_request(DMB_RESET_N, "dmb_reset");
	if(rc)
	{
		pr_warning("T3A00 DMB_RESET gpio request error\n");
	}
	gpio_direction_output(DMB_RESET_N, 0);  /* output and low */

	rc = gpio_request(DMB_INT_N, "dmb_irq");
	if(rc)
	{
		pr_warning("T3A00 DMB_INT_N gpio request error\n");
	}
	gpio_direction_input(DMB_INT_N);


	return 0;
}


static int  broadcast_t3a00_probe(struct spi_device *spi)
{
	int rc;

	if(spi == NULL)
	{
		printk("broadcast_t3a00_probe spi is NULL, so spi can not be set\n");
		return -1;
	}

	t3a00_ctrl_info.spi_ptr 				= spi;
	t3a00_ctrl_info.spi_ptr->mode 			= SPI_MODE_0;
	t3a00_ctrl_info.spi_ptr->bits_per_word 	= 8;
	t3a00_ctrl_info.spi_ptr->max_speed_hz 	= (7400*1000);

#ifdef FEATURE_DMB_USE_BUS_SCALE
	t3a00_ctrl_info.pdev = to_platform_device(&spi->dev);
	t3a00_ctrl_info.bus_scale_pdata = msm_bus_cl_get_pdata(t3a00_ctrl_info.pdev); 
	t3a00_ctrl_info.bus_scale_client_id = msm_bus_scale_register_client(t3a00_ctrl_info.bus_scale_pdata); 
#endif

	rc = spi_setup(spi);
	printk("%s is called and spi_setup\n", "broadcast_t3a00_probe");

#ifdef FEATURE_DMB_USE_XO
 	t3a00_ctrl_info.clk = clk_get(&t3a00_ctrl_info.spi_ptr->dev, "xo");
	if (IS_ERR(t3a00_ctrl_info.clk)) {
		rc = PTR_ERR(t3a00_ctrl_info.clk);
		dev_err(&t3a00_ctrl_info.spi_ptr->dev, "could not get clock\n");
		return rc;
	}

	/* We enable/disable the clock only to assure it works */
	rc = clk_prepare_enable(t3a00_ctrl_info.clk);
	if (rc) {
		dev_err(&t3a00_ctrl_info.spi_ptr->dev, "could not enable clock\n");
		return rc;
	}
	clk_disable_unprepare(t3a00_ctrl_info.clk);
#endif

#ifdef FEATURE_DMB_USE_WORKQUEUE
	INIT_WORK(&t3a00_ctrl_info.spi_work, broacast_tdmb_spi_work);

	t3a00_ctrl_info.spi_wq = create_singlethread_workqueue("tdmb_spi_wq");
	// t3a00_ctrl_info.spi_wq = create_rt_workqueue("tdmb_spi_wq");
	if(t3a00_ctrl_info.spi_wq == NULL){
		printk("Failed to setup tdmb spi workqueue \n");
		return -ENOMEM;
	}
#endif

	tdmb_t3a00_configure_gpios( );
#ifdef FEATURE_DMB_USE_WORKQUEUE
	rc = request_irq(spi->irq, broadcast_tdmb_spi_isr, IRQF_DISABLED | IRQF_TRIGGER_FALLING, spi->dev.driver->name, &t3a00_ctrl_info);
#else
	rc = request_threaded_irq(spi->irq, NULL, broadcast_tdmb_spi_event_handler, IRQF_DISABLED | IRQF_TRIGGER_FALLING,
	                   spi->dev.driver->name, &t3a00_ctrl_info);
#endif
	printk("broadcast_t3a00_probe request_irq=%d\n", rc);

	tdmb_t3a00_interrupt_lock();

	mutex_init(&t3a00_ctrl_info.mutex);

	wake_lock_init(&t3a00_ctrl_info.wake_lock,  WAKE_LOCK_SUSPEND, dev_name(&spi->dev));

	spin_lock_init(&t3a00_ctrl_info.spin_lock);

#ifdef FEATURE_DMB_USE_PM_QOS
	pm_qos_add_request(&t3a00_ctrl_info.pm_req_list, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
#endif
	printk("broadcast_t3a00_probe End. \n");
	
	power_set_pm8941_regulator(1);
	power_set_pm8941_regulator(0);
	return rc;
}

static int broadcast_t3a00_remove(struct spi_device *spi)
{
#ifdef FEATURE_DMB_USE_WORKQUEUE
	if (t3a00_ctrl_info.spi_wq)
	{
		flush_workqueue(t3a00_ctrl_info.spi_wq);
		destroy_workqueue(t3a00_ctrl_info.spi_wq);
	}
#endif

#ifdef FEATURE_DMB_USE_BUS_SCALE 
	msm_bus_scale_unregister_client(t3a00_ctrl_info.bus_scale_client_id); 
#endif
	free_irq(spi->irq, &t3a00_ctrl_info);
	mutex_destroy(&t3a00_ctrl_info.mutex);
	wake_lock_destroy(&t3a00_ctrl_info.wake_lock);

#ifdef FEATURE_DMB_USE_PM_QOS
	pm_qos_remove_request(&t3a00_ctrl_info.pm_req_list);
#endif
	memset((unsigned char*)&t3a00_ctrl_info, 0x0, sizeof(struct tdmb_t3a00_ctrl_blk));
	return 0;
}


static int broadcast_t3a00_suspend(struct spi_device *spi, pm_message_t mesg)
{
	printk("broadcast_t3a00_suspend \n");
	return 0;
}

static int broadcast_t3a00_resume(struct spi_device *spi)
{
	printk("broadcast_t3a00_resume \n");
	return 0;
}

int __devinit broadcast_tdmb_drv_init(void)
{
	int rc = 0;
	printk("%s is called\n", "broadcast_tdmb_drv_init");

//20130516_wonhee.jeong Code for revision separation [START]
#if defined (CONFIG_MACH_MSM8974_Z_KR) 
	if(lge_get_board_revno() == HW_REV_A){
		printk("TDMB T3A00 Not support in MSM8974_Z_KR Rev.A board\n");
		return rc;
	}
#endif
//20130516_wonhee.jeong Code for revision separation [END]
	rc = broadcast_tdmb_drv_start();
	if (rc) {
		printk("broadcast_tdmb_drv_start %s failed to load\n", __func__);
		return rc;
	}
	rc = spi_register_driver(&broadcast_tdmb_driver);
	printk("broadcast_spi_add_driver rc = (%d)\n", rc);

	return rc;
}

static void __exit broadcast_tdmb_drv_exit(void)
{
	spi_unregister_driver(&broadcast_tdmb_driver);
}


/* EXPORT_SYMBOL() : when we use external symbol 
which is not included in current module - over kernel 2.6 */
//EXPORT_SYMBOL(broadcast_tdmb_is_on);

module_init(broadcast_tdmb_drv_init);
module_exit(broadcast_tdmb_drv_exit);
MODULE_DESCRIPTION("broadcast_tdmb_drv_init");
MODULE_LICENSE("INC");
