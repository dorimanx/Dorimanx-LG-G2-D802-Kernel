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

static struct msm_xo_voter *xo_handle_tcc;

/*#define _NOT_USE_WAKE_LOCK_*/

struct broadcast_tcc3530_ctrl_data
{
	int			pwr_state;
	struct wake_lock	wake_lock;
	struct spi_device	*spi_dev;
	struct i2c_client	*pclient;
};

static struct broadcast_tcc3530_ctrl_data  IsdbCtrlInfo;
int broadcast_dmb_drv_start(void);

struct i2c_client*	TCC_GET_I2C_DRIVER(void)
{
	return IsdbCtrlInfo.pclient;
}

struct spi_device *TCC_GET_SPI_DRIVER(void)
{
	return IsdbCtrlInfo.spi_dev;
}

int tcc353x_power_on(void)
{
	if(IsdbCtrlInfo.pwr_state != 1)
	{
		int rc;
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

	IsdbCtrlInfo.pclient = client;
	//i2c_set_clientdata(client, (void*)&IsdbCtrlInfo.pclient);

	TchalInit();
#ifndef _NOT_USE_WAKE_LOCK_
	wake_lock_init(&IsdbCtrlInfo.wake_lock, WAKE_LOCK_SUSPEND,
					dev_name(&client->dev));	
#endif

	return rc;
}

static int broadcast_Isdb_i2c_remove(struct i2c_client* client)
{
	int rc = 0;

	TcpalPrintStatus((I08S *)"[%s]\n", __func__);
#ifndef _NOT_USE_WAKE_LOCK_
	wake_lock_destroy(&IsdbCtrlInfo.wake_lock);
#endif
	memset((unsigned char*)&IsdbCtrlInfo, 0x0, sizeof(struct broadcast_tcc3530_ctrl_data));
	TcpalDeleteSemaphore(&Tcc353xDrvSem);
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
	TcpalPrintStatus((I08S *)"[%s add i2c driver]\n", __func__);
	rc = i2c_add_driver(&broadcast_Isdb_driver);
	TcpalPrintStatus((I08S *)"broadcast_add_driver rc = (%d)\n", rc);
	return rc;
}

static void __exit broadcast_dmb_drv_exit(void)
{
	i2c_del_driver(&broadcast_Isdb_driver);
}

module_init(broadcast_dmb_drv_init);
module_exit(broadcast_dmb_drv_exit);
MODULE_DESCRIPTION("broadcast_dmb_drv_init");
MODULE_LICENSE("TELECHIPS");
