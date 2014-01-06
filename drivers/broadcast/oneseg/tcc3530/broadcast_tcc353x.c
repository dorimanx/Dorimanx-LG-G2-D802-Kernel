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

static const char *id = "TCC";
static struct msm_xo_voter *xo_handle_tcc;

/*#define _NOT_USE_WAKE_LOCK_*/

struct broadcast_tcc3530_ctrl_data
{
	int			pwr_state;
	struct wake_lock	wake_lock;
	struct spi_device	*spi_dev;
};

static struct broadcast_tcc3530_ctrl_data  IsdbCtrlInfo;
//static unsigned int user_stop_flg = 0;
//static unsigned int mdelay_in_flg = 0;
int broadcast_dmb_drv_start(void);

struct spi_device *TCC_GET_SPI_DRIVER(void)
{
	return IsdbCtrlInfo.spi_dev;
}

int tcc353x_power_on(void)
{
	int rc;
	
	if(IsdbCtrlInfo.pwr_state != 1)
	{
#ifndef _NOT_USE_WAKE_LOCK_
		wake_lock(&IsdbCtrlInfo.wake_lock);
#endif
		TchalPowerOnDevice();

		rc = msm_xo_mode_vote(xo_handle_tcc, MSM_XO_MODE_ON);
		if(rc < 0) {
			pr_err("Configuring MSM_XO_MODE_ON failed (%d)\n", rc);
			msm_xo_put(xo_handle_tcc);
			return FALSE;
		}

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
		if(xo_handle_tcc != NULL) {
		    msm_xo_mode_vote(xo_handle_tcc, MSM_XO_MODE_OFF);
		}
		
		TcpalPrintStatus((I08S *)"Isdb_tcc3530_power_off\n");
		TchalPowerDownDevice();
	}

#ifndef _NOT_USE_WAKE_LOCK_
	wake_unlock(&IsdbCtrlInfo.wake_lock);
#endif
	IsdbCtrlInfo.pwr_state = 0;

	return OK;
}

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

	return rc;
}

static int broadcast_Isdb_spi_remove(struct spi_device *spi)
{
	int rc = 0;
	
	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
	TcpalUnRegisterIrqHandler();
#ifndef _NOT_USE_WAKE_LOCK_
	wake_lock_destroy(&IsdbCtrlInfo.wake_lock);
#endif
	memset((unsigned char*)&IsdbCtrlInfo, 0x0, sizeof(struct broadcast_tcc3530_ctrl_data));
	TcpalDeleteSemaphore(&Tcc353xDrvSem);
	TcpalDeleteSemaphore(&Tcc353xStreamSema);
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
	rc =  spi_register_driver(&broadcast_Isdb_driver);
	TcpalPrintStatus((I08S *)"broadcast_add_driver rc = (%d)\n", rc);
	return rc;
}

static void __exit broadcast_dmb_drv_exit(void)
{

	spi_unregister_driver(&broadcast_Isdb_driver);
}

module_init(broadcast_dmb_drv_init);
module_exit(broadcast_dmb_drv_exit);
MODULE_DESCRIPTION("broadcast_dmb_drv_init");
MODULE_LICENSE("TELECHIPS");
