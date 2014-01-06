

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
//#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/interrupt.h>

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h> 		/* wake_lock, unlock */

#include "../broadcast_tdmb_drv_ifdef.h"
#include "broadcast_lg2102.h"

/* external function */
extern void broadcast_tdmb_read_data(void);

/* proto type declare */
static int broadcast_tdmb_lg2102_probe(struct spi_device *spi);
static int broadcast_tdmb_lg2102_remove(struct spi_device *spi);
static int broadcast_tdmb_lg2102_suspend(struct spi_device *spi, pm_message_t mesg);
static int broadcast_tdmb_lg2102_resume(struct spi_device *spi);

#define DMB_EN 			28 //GPIO28
#define DMB_INT_N 		29 //GPIO29
#define DMB_RESET_N 	62 //GPIO62

/************************************************************************/
/* LINUX Driver Setting                                                 */
/************************************************************************/
static uint32 user_stop_flg = 0;
static uint32 mdelay_in_flg = 0;
struct TDMB_LG2102_CTRL
{
	boolean 					TdmbPowerOnState;
	struct i2c_client*			pClient;
	struct spi_device* 		pSpiDevice;
	struct work_struct 		spi_work;
	struct workqueue_struct* 	spi_wq;
	struct mutex				mutex;
	struct wake_lock 			wake_lock;	/* wake_lock,wake_unlock */
	//test
	boolean 					spi_irq_status;
};

//static broadcast_pwr_func pwr_func;

static struct TDMB_LG2102_CTRL TdmbCtrlInfo;

struct i2c_client* INC_GET_I2C_DRIVER(void)
{
	return TdmbCtrlInfo.pClient;
}

void LGD_RW_TEST(void);


void tdmb_lg2102_set_userstop(void)
{
	user_stop_flg = ((mdelay_in_flg == 1)? 1: 0 );
}

int tdmb_lg2102_mdelay(int32 ms)
{
#if 0

	int rc = 1;

	printk("[LG2102] mdelay (IN) ms = (%d)\n", ms);
	mdelay_in_flg = 1;
	msleep(ms);
	if(user_stop_flg == 1)
	{
		rc = 0;
		printk("~~~ tdmb_lg2102_mdelay user stop ~~~~ rc = (%d)\n", rc);
	}
	mdelay_in_flg = 0;
	user_stop_flg = 0;
	printk("[LG2102]mdelay(OUT) ms = (%d)\n", ms);
#else
	int32 wait_loop =0;
	int32 wait_ms = ms;
	int       rc = 1;  /* 0 : false, 1 : ture */

	if(ms > 100)
	{
		wait_loop = (ms /100);   /* 100, 200, 300 more only , Otherwise this must be modified e.g (ms + 40)/50 */
		wait_ms = 100;
	}

	//printk("[LG2102] mdelay (IN) ms = (%d)\n", ms);
	mdelay_in_flg = 1;

	do
	{
		msleep(wait_ms);
		if(user_stop_flg == 1)
		{
			printk("~~~~~~~~ Ustop flag is set so return false ~~~~~~~~\n");
			rc = 0;
			break;
		}
	}while((--wait_loop) > 0);

	mdelay_in_flg = 0;
	user_stop_flg = 0;
	//printk("[LG2102]mdelay(OUT) ms = (%d) ustop_flg = (%d) wait_loop = (%d)\n", ms, user_stop_flg, wait_loop);
#endif	
	return rc;
}

int tdmb_lg2102_power_on(void)
{
	//int rc = 0;
	// DMB_INT = GPIO29
	// DMB_EN = GPIO28(1.2V) , 1.8V_VIO(alyways on)
	// DMB_RESET = GPIO62
	if ( TdmbCtrlInfo.TdmbPowerOnState == FALSE )
	{
		wake_lock(&TdmbCtrlInfo.wake_lock);
		gpio_direction_input(DMB_INT_N);
		gpio_direction_output(DMB_RESET_N, false);
		gpio_direction_output(DMB_EN, true);
		gpio_set_value(DMB_EN, 1);
		gpio_set_value(DMB_RESET_N, 1);			
		udelay(1000); //500us
		gpio_set_value(DMB_RESET_N, 0);
		udelay(1000); //500us
		gpio_set_value(DMB_RESET_N, 1);		
		udelay(1000); //500us
		tdmb_lg2102_interrupt_free();
		TdmbCtrlInfo.TdmbPowerOnState = TRUE;
	}
	else
	{
		printk("tdmb_lg2102_power_on the power already turn on \n");
	}

	printk("tdmb_lg2102_power_on completed \n");

	return TRUE;
}

int tdmb_lg2102_power_off(void)
{
	//int rc = FALSE;

	if ( TdmbCtrlInfo.TdmbPowerOnState == TRUE )
	{
		tdmb_lg2102_interrupt_lock();
		TdmbCtrlInfo.TdmbPowerOnState = FALSE;
		gpio_set_value(DMB_RESET_N, 0);
		gpio_set_value(DMB_EN, 0);
		gpio_direction_output(DMB_INT_N, false);
		gpio_set_value(DMB_INT_N, 0);
		wake_unlock(&TdmbCtrlInfo.wake_lock);
	}
	else
	{
		printk("tdmb_lg2102_power_on the power already turn off \n");
	}	

	printk("tdmb_lg2102_power_off completed \n");
	
	return TRUE;
}

#if 0
 static int tdmb_lg2102_i2c_write(uint8* txdata, int length)
{
	struct i2c_msg msg = 
	{	
		TdmbCtrlInfo.pClient->addr,
		0,
		length,
		txdata 
	};


	if (i2c_transfer( TdmbCtrlInfo.pClient->adapter, &msg, 1) < 0) 
	{
		printk("tdmb lg2102 i2c write failed\n");
		return FALSE;
	}

	//printk("tdmb lg2102 i2c write addr = %x data = %x!! \n",addr, data);
	
	return TRUE;
}
 
int tdmb_lg2102_i2c_write_burst(uint16 waddr, uint8* wdata, int length)
{
 	uint8 *buf;
	int	wlen;

	int rc;

	wlen = length + 2;

	buf = (uint8*)kmalloc( wlen, GFP_KERNEL);

	if((buf == NULL) || ( length <= 0 ))
	{
		printk("tdmb_lg2102_i2c_write_burst buf alloc failed\n");
		return FALSE;
	}

	buf[0] = (waddr>>8)&0xFF;
	buf[1] = (waddr&0xFF);

	memcpy(&buf[2], wdata, length);
 
	rc = tdmb_lg2102_i2c_write(buf, wlen);

	kfree(buf);
		
	return rc;
}

static int tdmb_lg2102_i2c_read( uint16 raddr,	uint8 *rxdata, int length)
{
	uint8	r_addr[2] = {raddr>>8, raddr&0xff};
	//uint8	acBuff[384];
	
	struct i2c_msg msgs[] = 
	{
		{
			.addr   = TdmbCtrlInfo.pClient->addr,
			.flags = 0,
			.len   = 2,
			.buf   = &r_addr[0],
		},
		{
			.addr   = TdmbCtrlInfo.pClient->addr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	
	//memset(acBuff, 0, sizeof(acBuff));

	if (i2c_transfer(TdmbCtrlInfo.pClient->adapter, msgs, 2) < 0) 
	{
		printk("tdmb_lg2102_i2c_read failed! %x \n",TdmbCtrlInfo.pClient->addr);
		return FALSE;
	}
	
	//memcpy(rxdata,acBuff, length);

	
	//printk("tdmb lg2102 i2c read addr = %x data = %x!! \n",addr, data);
	
	return TRUE;
}

int tdmb_lg2102_i2c_read_burst(uint16 raddr, uint8* rdata, int length)
{
	int rc;
 
	rc = tdmb_lg2102_i2c_read(raddr, rdata, length);

	return rc;

}


int tdmb_lg2102_i2c_write16(unsigned short reg, unsigned short val)
{
	unsigned int err;
	unsigned char buf[4] = { reg>>8, reg&0xff, val>>8, val&0xff };
	struct i2c_msg	msg = 
	{	
		TdmbCtrlInfo.pClient->addr,
		0,
		4,
		buf 
	};
	
	if ((err = i2c_transfer( TdmbCtrlInfo.pClient->adapter, &msg, 1)) < 0) 
	{
		dev_err(&TdmbCtrlInfo.pClient->dev, "i2c write error\n");
		err = FALSE;
	}
	else
	{
		//printk(KERN_INFO "tdmb : i2c write ok:addr = %x data = %x\n", reg, val);
		err = TRUE;
	}

	return err;
}


int tdmb_lg2102_i2c_read16(uint16 reg, uint16 *ret)
{

	uint32 err;
	uint8 w_buf[2] = {reg>>8, reg&0xff};	
	uint8 r_buf[2] = {0,0};

	struct i2c_msg msgs[2] = 
	{
		{ TdmbCtrlInfo.pClient->addr, 0, 2, &w_buf[0] },
		{ TdmbCtrlInfo.pClient->addr, I2C_M_RD, 2, &r_buf[0]}
	};

	if ((err = i2c_transfer(TdmbCtrlInfo.pClient->adapter, msgs, 2)) < 0) 
	{
		dev_err(&TdmbCtrlInfo.pClient->dev, "i2c read error\n");
		err = FALSE;
	}
	else
	{
		//printk( "tdmb addr = %x : i2c read ok: data[0] = %x data[1] = %x \n", TdmbCtrlInfo.pClient->addr, r_buf[0], r_buf[1]);
		*ret = r_buf[0]<<8 | r_buf[1];
		//printk( "tdmb : i2c read ok: data = %x\n", *ret);
		err = TRUE;
	}

	return err;
}


void LGD_RW_TEST(void)
{
	unsigned short i = 0;
	unsigned short w_val = 0;
	unsigned short r_val = 0;
	unsigned short err_cnt = 0;

	err_cnt = 0;
	for(i=1;i<11;i++)
	{
		w_val = (i%0xFF);
		tdmb_lg2102_i2c_write16( 0x0a00+ 0x05, w_val);
		tdmb_lg2102_i2c_read16(0x0a00+ 0x05, &r_val );
		if(r_val != w_val)
		{
			err_cnt++;
			printk("w_val:%x, r_val:%x\n", w_val,r_val);
		}
	}
}
#endif

#if 0
static int tdmb_lg2102_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int nRet;
	struct broadcast_tdmb_data *pdata;

	memset(&TdmbCtrlInfo, 0x00, sizeof(struct TDMB_LG2102_CTRL));

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "tdmb_lg2102_i2c_probe: need I2C_FUNC_I2C\n");
		nRet = -ENODEV;
		return nRet;
	}

	TdmbCtrlInfo.pClient 	= client;
	//TdmbCtrlInfo.pId 		= id;
	
	i2c_set_clientdata(client, (void*)&TdmbCtrlInfo);

	pdata = client->dev.platform_data;

	pwr_func.tdmb_pwr_on = pdata->pwr_on;
	pwr_func.tdmb_pwr_off = pdata->pwr_off;

	printk("broadcast_tdmb_lg2102_i2c_probe start \n");
	
	return TRUE;
}

static const struct i2c_device_id tdmb_lg2102_i2c_id[] = {
	{ "lg2102", 0},
	{ },
};

static struct i2c_driver tdmb_lg2102_i2c_driver = {
	.id_table = tdmb_lg2102_i2c_id,
	.probe  = tdmb_lg2102_i2c_probe,
	.driver = {
		.name = "lg2102",
	},
};

static int broadcast_tdmb_lg2102_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc;

	rc = broadcast_tdmb_drv_start();

	printk("broadcast_tdmb_lg2102_probe start %d\n", rc);
#if 0
	rc = i2c_add_driver(&tdmb_lg2102_i2c_driver);
	if (rc < 0)
	{
	  printk("i2c_add_driver fail!! \n");
	  rc = -1;
	}
#endif

	tdmb_lg2102_i2c_probe(client, id);
	printk("broadcast_tdmb_lg2102_probe start \n");

	return rc;

}


static struct i2c_device_id tdmb_lg2102_idtable[] = {
	{ "tdmb_lg2102", 0 },
};


static struct i2c_driver broadcast_tdmb_driver = {
	.probe = broadcast_tdmb_lg2102_probe,
	.id_table = tdmb_lg2102_idtable,
	.driver = {
		.name = "tdmb_lg2102",
		.owner = THIS_MODULE,
	},
};
#endif

int tdmb_lg2102_select_antenna(unsigned int sel)
{
	if(LGE_BROADCAST_TDMB_ANT_TYPE_INTENNA == sel)
	{
		//gpio_set_value(DMB_EAR_ANT_SEL, 0);
	}
	else if(LGE_BROADCAST_TDMB_ANT_TYPE_INTENNA == sel)
	{
		//gpio_set_value(DMB_EAR_ANT_SEL, 1);
	}
	else
	{
		return FALSE;
	}
	
	return TRUE;
}

static struct spi_driver broadcast_tdmb_driver = {
	.probe = broadcast_tdmb_lg2102_probe,
	.remove	= __devexit_p(broadcast_tdmb_lg2102_remove),
	.suspend = broadcast_tdmb_lg2102_suspend,
	.resume  = broadcast_tdmb_lg2102_resume,
	.driver = {
		.name = "tdmb_lg2102",
		.bus	= &spi_bus_type,
		.owner = THIS_MODULE,
	},
};

void tdmb_lg2102_interrupt_lock(void)
{
	if (TdmbCtrlInfo.pSpiDevice == NULL)
	{
		printk("tdmb_lg2102_interrupt_lock fail\n");
	}

	disable_irq(TdmbCtrlInfo.pSpiDevice->irq);
}

void tdmb_lg2102_interrupt_free(void)
{
	if (TdmbCtrlInfo.pSpiDevice == NULL)
	{
		printk("tdmb_lg2102_interrupt_free fail\n");
	}

	enable_irq(TdmbCtrlInfo.pSpiDevice->irq);
}

int tdmb_lg2102_spi_write_read(uint8* tx_data, int tx_length, uint8 *rx_data, int rx_length)
{
	int rc;
	
	struct spi_transfer	t = {
			.tx_buf		= tx_data,
			.rx_buf		= rx_data,
			.len			= tx_length+rx_length,
		};
	struct spi_message	m;	

	if (TdmbCtrlInfo.pSpiDevice == NULL)
	{
		printk("tdmb_lg2102_spi_write_read error txdata=0x%x, length=%d\n", (unsigned int)tx_data, tx_length+rx_length);
	}

	mutex_lock(&TdmbCtrlInfo.mutex);

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	rc = spi_sync(TdmbCtrlInfo.pSpiDevice, &m);
	if ( rc < 0 )
	{
		printk("tdmb_lg2102_spi_read_burst result(%d), actual_len=%d\n",rc, m.actual_length);
	}

	mutex_unlock(&TdmbCtrlInfo.mutex);

	return TRUE;
}

static irqreturn_t broadcast_tdmb_spi_isr(int irq, void *handle)
{
	struct TDMB_LG2102_CTRL* pTdmbInfo;

	pTdmbInfo = (struct TDMB_LG2102_CTRL *)handle;	
	if ( pTdmbInfo && pTdmbInfo->TdmbPowerOnState )
	{
		if (pTdmbInfo->spi_irq_status)
		{
			printk("########### broadcast_tdmb_spi_isr ###########\n");
			printk("######### spi read funtion is so late skip #########\n");
			return IRQ_HANDLED;
		}
		queue_work(pTdmbInfo->spi_wq, &pTdmbInfo->spi_work);    
	}
	else
	{
		printk("broadcast_tdmb_spi_isr is called, but device is off state\n");
	}

	return IRQ_HANDLED; 
}

static void broacast_tdmb_spi_work(struct work_struct *tdmb_work)
{
	struct TDMB_LG2102_CTRL *pTdmbWorkData;

	pTdmbWorkData = container_of(tdmb_work, struct TDMB_LG2102_CTRL, spi_work);
	if ( pTdmbWorkData )
	{
		pTdmbWorkData->spi_irq_status = TRUE;
		broadcast_drv_if_isr();
		pTdmbWorkData->spi_irq_status = FALSE;
		//printk("broacast_tdmb_spi_work is called handle=0x%x\n", (unsigned int)pTdmbWorkData);
	}
}

static int broadcast_tdmb_lg2102_probe(struct spi_device *spi)
{
	int rc;

	TdmbCtrlInfo.pSpiDevice 					= spi;
	TdmbCtrlInfo.pSpiDevice->mode 			= SPI_MODE_0;
	TdmbCtrlInfo.pSpiDevice->bits_per_word 	= 8;
	TdmbCtrlInfo.pSpiDevice->max_speed_hz 	= 6000*1000;
	rc = spi_setup(spi);
	printk("broadcast_tdmb_lg2102_probe spi_setup=%d\n", rc);

	INIT_WORK(&TdmbCtrlInfo.spi_work, broacast_tdmb_spi_work);
	TdmbCtrlInfo.spi_wq = create_singlethread_workqueue("tdmb_spi_wq");
        if(TdmbCtrlInfo.spi_wq == NULL){
		printk("Failed to setup tdmb spi workqueue \n");          
        }

	rc = request_irq(spi->irq, broadcast_tdmb_spi_isr, IRQF_DISABLED | IRQF_TRIGGER_RISING, spi->dev.driver->name, &TdmbCtrlInfo);
	printk("broadcast_tdmb_lg2102_probe request_irq=%d\n", rc);
	//enable_irq_wake(spi->irq);

	// DMB_EN = GPIO28(1.2V) , 1.8V_VIO(alyways on)
	// DMB_RESET = GPIO62
	gpio_direction_output(DMB_RESET_N, false);
	gpio_direction_output(DMB_EN, false);
	gpio_direction_output(DMB_INT_N, false);
	tdmb_lg2102_interrupt_lock();

	mutex_init(&TdmbCtrlInfo.mutex);
	wake_lock_init(&TdmbCtrlInfo.wake_lock,  WAKE_LOCK_SUSPEND, dev_name(&spi->dev));		

	return rc;
}

static int broadcast_tdmb_lg2102_remove(struct spi_device *spi)
{
	printk("broadcast_tdmb_lg2102_remove \n");

	if (TdmbCtrlInfo.spi_wq)
	{
		flush_workqueue(TdmbCtrlInfo.spi_wq);
		destroy_workqueue(TdmbCtrlInfo.spi_wq);
	}

	free_irq(spi->irq, &TdmbCtrlInfo);
	mutex_destroy(&TdmbCtrlInfo.mutex);

	wake_lock_destroy(&TdmbCtrlInfo.wake_lock);

	memset((unsigned char*)&TdmbCtrlInfo, 0x0, sizeof(struct TDMB_LG2102_CTRL));
	return 0;
}

static int broadcast_tdmb_lg2102_suspend(struct spi_device *spi, pm_message_t mesg)
{
	printk("broadcast_tdmb_lg2102_suspend \n");
	return 0;
}

static int broadcast_tdmb_lg2102_resume(struct spi_device *spi)
{
	printk("broadcast_tdmb_lg2102_resume \n");
	return 0;
}

int __devinit broadcast_tdmb_drv_init(void)
{
	int rc;

	rc = broadcast_tdmb_drv_start();
	printk("broadcast_tdmb_lg2102_probe start %d\n", rc);

	return spi_register_driver(&broadcast_tdmb_driver);
}

static void __exit broadcast_tdmb_drv_exit(void)
{
	spi_unregister_driver(&broadcast_tdmb_driver);
}

int broadcast_tdmb_is_on(void)
{
	return (int)TdmbCtrlInfo.TdmbPowerOnState;
}

EXPORT_SYMBOL(broadcast_tdmb_is_on);

module_init(broadcast_tdmb_drv_init);
module_exit(broadcast_tdmb_drv_exit);
MODULE_DESCRIPTION("broadcast_tdmb_drv_init");
MODULE_LICENSE("INC");

