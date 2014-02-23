#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h> 

#include <linux/err.h>
#include <mach/msm_xo.h>

#include "tcpal_os.h"
#include "tcc353x_hal.h"

#include "broadcast_dmb_typedef.h"
#include "broadcast_dmb_drv_ifdef.h"
#include "broadcast_tcc353x.h"

TcpalSemaphore_t Tcc353xDrvSem;
TcpalSemaphore_t Tcc353xStreamSema;
TcpalSemaphore_t Tcc353xLnaControlSema;
#if defined (_MODEL_F9J_)
static struct msm_xo_voter *xo_handle_tcc;
#endif
/*#define _NOT_USE_WAKE_LOCK_*/

struct broadcast_tcc3530_ctrl_data
{
	int			pwr_state;
	struct wake_lock	wake_lock;
	struct spi_device	*spi_dev;
	struct i2c_client	*pclient;
};

static struct broadcast_tcc3530_ctrl_data  IsdbCtrlInfo;
//static unsigned int user_stop_flg = 0;
//static unsigned int mdelay_in_flg = 0;
int broadcast_dmb_drv_start(void);

struct i2c_client*	TCC_GET_I2C_DRIVER(void)
{
	return IsdbCtrlInfo.pclient;
}

struct spi_device *TCC_GET_SPI_DRIVER(void)
{
	return IsdbCtrlInfo.spi_dev;
}

extern void tcc353x_lnaControl_start(void);
extern void tcc353x_lnaControl_stop(void);


int tcc353x_power_on(void)
{
#if defined (_MODEL_F9J_)
	int rc;
#endif	
	if(IsdbCtrlInfo.pwr_state != 1)
	{
#ifndef _NOT_USE_WAKE_LOCK_
		wake_lock(&IsdbCtrlInfo.wake_lock);
#endif
		TchalPowerOnDevice();
#if defined (_MODEL_F9J_)

		rc = msm_xo_mode_vote(xo_handle_tcc, MSM_XO_MODE_ON);
		if(rc < 0) {
			pr_err("Configuring MSM_XO_MODE_ON failed (%d)\n", rc);
			msm_xo_put(xo_handle_tcc);
			return FALSE;
		}
#endif		
	}
	else
	{
		TcpalPrintStatus((I08S *)"aready on!! \n");
	}

	IsdbCtrlInfo.pwr_state = 1;
	return OK;

}


int tcc353x_is_power_on()
{
	return (int)IsdbCtrlInfo.pwr_state;
}

int tcc353x_power_off(void)
{
	if(IsdbCtrlInfo.pwr_state == 0)
	{
		TcpalPrintStatus((I08S *)"Isdb_tcc3530_power is immediately off\n");
		return OK;
	}
	else
	{
		#if defined (_MODEL_F9J_)
		if(xo_handle_tcc != NULL) {
		    msm_xo_mode_vote(xo_handle_tcc, MSM_XO_MODE_OFF);
		}
		#endif
		TcpalPrintStatus((I08S *)"Isdb_tcc3530_power_off\n");
		TchalPowerDownDevice();
	}

#ifndef _NOT_USE_WAKE_LOCK_
	wake_unlock(&IsdbCtrlInfo.wake_lock);
#endif
	IsdbCtrlInfo.pwr_state = 0;

	return OK;
}

#if defined (_I2C_STS_)
static int broadcast_Isdb_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;

	int addr;

	TcpalPrintLog("broadcast_Isdb_i2c_probe client:0x%X\n", (unsigned int)client);
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TcpalPrintErr("need I2C_FUNC_I2C\n");
		return -ENODEV;
	}
	/* taew00k.kang added for Device Tree Structure 2013-06-04 [start] */
	addr = client->addr; //Slave Addr
	pr_err("[1seg] i2c Slaveaddr [%x] \n", addr);
	/* taew00k.kang added for Device Tree Structure 2013-06-04 [end] */

	TcpalCreateSemaphore(&Tcc353xDrvSem,
			     (I08S *) "Tcc353xDriverControlSemaphore", 1);
	TcpalCreateSemaphore(&Tcc353xStreamSema,
			     (I08S *) "StreamSemaphore", 1);
	TcpalCreateSemaphore(&Tcc353xLnaControlSema,
			     (I08S *) "LnaControlSemaphore", 1);

	IsdbCtrlInfo.pclient = client;
	//i2c_set_clientdata(client, (void*)&IsdbCtrlInfo.pclient);

	TchalInit();
	TcpalRegisterIrqHandler();
	TcpalIrqDisable();

#ifndef _NOT_USE_WAKE_LOCK_
	wake_lock_init(&IsdbCtrlInfo.wake_lock, WAKE_LOCK_SUSPEND,
					dev_name(&client->dev));	
#endif

	/* lna control task start */
	tcc353x_lnaControl_start();

	return rc;
}

static int broadcast_Isdb_i2c_remove(struct i2c_client* client)
{
	int rc = 0;

	/* lna control task stop */
	tcc353x_lnaControl_stop();

	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	TcpalUnRegisterIrqHandler();
#ifndef _NOT_USE_WAKE_LOCK_
	wake_lock_destroy(&IsdbCtrlInfo.wake_lock);
#endif
	memset((unsigned char*)&IsdbCtrlInfo, 0x0, sizeof(struct broadcast_tcc3530_ctrl_data));
	TcpalDeleteSemaphore(&Tcc353xDrvSem);
	TcpalDeleteSemaphore(&Tcc353xStreamSema);
	TcpalDeleteSemaphore(&Tcc353xLnaControlSema);
	return rc;
}

static int broadcast_Isdb_i2c_suspend(struct i2c_client* client, pm_message_t mesg)
{
	int rc = 0;
	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	return rc;
}

static int broadcast_Isdb_i2c_resume(struct i2c_client* client)
{
	int rc = 0;
	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	return rc;
}

static const struct i2c_device_id isdbt_tcc3530_id[] = {
/* taew00k.kang added for Device Tree Structure 2013-06-04 [start] */
	{"tcc3535_i2c",	0},
/* taew00k.kang added for Device Tree Structure 2013-06-04 [end] */
	{},
};

MODULE_DEVICE_TABLE(i2c, isdbt_tcc3530_id);


/* taew00k.kang added for Device Tree Structure 2013-06-04 [start] */
static struct of_device_id tcc3535_i2c_table[] = {
{ .compatible = "telechips,tcc3535-i2c",}, //Compatible node must match dts
{ },
};
/* taew00k.kang added for Device Tree Structure 2013-06-04 [end] */

static struct i2c_driver broadcast_Isdb_driver = {
	.driver = {
		.name = "tcc3535_i2c",
		.owner = THIS_MODULE,
		.of_match_table = tcc3535_i2c_table,
	},
	.probe = broadcast_Isdb_i2c_probe,
	.remove	= __devexit_p(broadcast_Isdb_i2c_remove),
	.id_table = isdbt_tcc3530_id,
	.suspend = broadcast_Isdb_i2c_suspend,
	.resume  = broadcast_Isdb_i2c_resume,
};
#else
static int broadcast_Isdb_spi_probe(struct spi_device *spi_dev)
{
	int rc = 0;

	xo_handle_tcc = msm_xo_get(MSM_XO_TCXO_A0, id);
	if(IS_ERR(xo_handle_tcc)) {
		pr_err("Failed to get MSM_XO_TCXO_A2 handle for TDMB (%ld)\n", PTR_ERR(xo_handle_tcc));
		return FALSE;
	}

	TcpalCreateSemaphore(&Tcc353xDrvSem,
			     (I08S *) "Tcc353xDriverControlSemaphore", 1);
	TcpalCreateSemaphore(&Tcc353xStreamSema,
			     (I08S *) "StreamSemaphore", 1);
	TcpalCreateSemaphore(&Tcc353xLnaControlSema,
			     (I08S *) "LnaControlSemaphore", 1);

	spi_dev->mode = SPI_MODE_0;
	spi_dev->bits_per_word = 8;
	spi_dev->max_speed_hz = 32*1000*1000;	/* 32 MHz	*/
	rc = spi_setup(spi_dev);

	IsdbCtrlInfo.spi_dev = spi_dev;
	IsdbCtrlInfo.pwr_state = 0;
	TcpalPrintStatus((I08S *)"spi : %p\n", spi_dev);

	TchalInit();
	TcpalRegisterIrqHandler();
	TcpalIrqDisable();

#ifndef _NOT_USE_WAKE_LOCK_
	wake_lock_init(&IsdbCtrlInfo.wake_lock, WAKE_LOCK_SUSPEND,
					dev_name(&spi_dev->dev));	
#endif

	/* lna control task start */
	tcc353x_lnaControl_start();

	return rc;
}

static int broadcast_Isdb_spi_remove(struct spi_device *spi)
{
	int rc = 0;

	/* lna control task stop */
	tcc353x_lnaControl_stop();

	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	TcpalUnRegisterIrqHandler();
#ifndef _NOT_USE_WAKE_LOCK_
	wake_lock_destroy(&IsdbCtrlInfo.wake_lock);
#endif
	memset((unsigned char*)&IsdbCtrlInfo, 0x0, sizeof(struct broadcast_tcc3530_ctrl_data));
	TcpalDeleteSemaphore(&Tcc353xDrvSem);
	TcpalDeleteSemaphore(&Tcc353xStreamSema);
	TcpalDeleteSemaphore(&Tcc353xLnaControlSema);
	return rc;
}

static int broadcast_Isdb_spi_suspend(struct spi_device *spi, pm_message_t mesg)
{
	int rc = 0;
	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	return rc;
}

static int broadcast_Isdb_spi_resume(struct spi_device *spi)
{
	int rc = 0;
	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	return rc;
}

static struct spi_driver broadcast_Isdb_driver = {
	.probe = broadcast_Isdb_spi_probe,
	.remove	= __devexit_p(broadcast_Isdb_spi_remove),
	.suspend = broadcast_Isdb_spi_suspend,
	.resume  = broadcast_Isdb_spi_resume,
	.driver = {
		.name = "isdbt_tcc3530",
		.bus	= &spi_bus_type,
		.owner = THIS_MODULE,
	},
};
#endif

int __devinit broadcast_dmb_drv_init(void)
{
	int rc;

	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	rc = broadcast_dmb_drv_start();	
	if (rc) 
	{
		TcpalPrintErr((I08S *)"failed to load\n");
		return rc;
	}

#if defined(_I2C_STS_)
	TcpalPrintStatus((I08S *)"[%s add i2c driver]\n", __func__);
	rc = i2c_add_driver(&broadcast_Isdb_driver);
#else
	TcpalPrintStatus((I08S *)"[%s add spi driver]\n", __func__);
	rc =  spi_register_driver(&broadcast_Isdb_driver);
#endif

	TcpalPrintStatus((I08S *)"broadcast_add_driver rc = (%d)\n", rc);
	return rc;
}

static void __exit broadcast_dmb_drv_exit(void)
{
#if defined(_I2C_STS_)
	i2c_del_driver(&broadcast_Isdb_driver);
#else
	spi_unregister_driver(&broadcast_Isdb_driver);
#endif
}

module_init(broadcast_dmb_drv_init);
module_exit(broadcast_dmb_drv_exit);
MODULE_DESCRIPTION("broadcast_dmb_drv_init");
MODULE_LICENSE("TELECHIPS");
