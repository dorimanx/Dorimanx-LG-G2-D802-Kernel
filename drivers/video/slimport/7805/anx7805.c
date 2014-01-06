#define _ANX7805_C_
#include "sp_tx_drv.h"
#include "sp_tx_reg.h"
#include "anx7805.h"


#ifdef __KEIL51_ENV__

int hardware_power_ctl(int stat)
{
	if(stat == 1) {
#if(HARDWARE_POWER_CHANGE)
	SP_TX_HW_RESET= 1;

	SP_TX_CHIP_PD_CTRL= 0;
	delay_ms(50);
	SP_TX_PWR_V10_CTRL= 1;
	delay_ms(50);
	SP_TX_PWR_V10_CTRL= 0;
	delay_ms(300);
	//De-assert reset after power on
	SP_TX_HW_RESET= 0;
	delay_ms(500);

#else
	SP_TX_HW_RESET= 0;

	SP_TX_CHIP_PD_CTRL= 0;
	delay_ms(1);
	SP_TX_PWR_V10_CTRL= 0;
	delay_ms(2);
	SP_TX_PWR_V10_CTRL= 1;
	delay_ms(20);
	//De-assert reset after power on
	SP_TX_HW_RESET= 1;
	//SP_TX_Hardware_Reset();
#endif
	}else {
#if(HARDWARE_POWER_CHANGE)
	SP_TX_HW_RESET= 1;
	delay_ms(100);
	SP_TX_PWR_V10_CTRL= 1;
	delay_ms(200);
	SP_TX_CHIP_PD_CTRL = 1;
       delay_ms(100);
#else
	SP_TX_HW_RESET= 0;
	delay_ms(1);
	SP_TX_PWR_V10_CTRL= 0;
	delay_ms(5);
	SP_TX_CHIP_PD_CTRL = 1;
       delay_ms(1);
#endif
	}
	
	return 1;
}
int get_cable_detect_status(void){
	return SLIMPORT_CABLE_DETECT ? 1 : 0;
}

#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/ioport.h>
#include <linux/input-polldev.h>
#include <linux/i2c.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/platform_data/slimport_device.h>
#include <asm/types.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <sound/soc.h>
#include <mach/gpio.h>
#include <linux/sched.h>   //wake_up_process()
#include <linux/kthread.h> //kthread_create()\u3001kthread_run()
#include <linux/module.h> 
#include <linux/wait.h>
#include <mach/board.h> 
#include <linux/of_gpio.h>

#undef NULL
#define NULL ((void *)0)


static struct anx7805_platform_data *g_pdata;
struct i2c_client *anx7805_client;

struct anx7805_data {
	struct anx7805_platform_data    *pdata;
	struct delayed_work    work;
	struct workqueue_struct    *anx7805_workqueue;
};

int timer_en_cnt;
int cable_detect_irq=0;
int workqueue_run_flag=0;


int Anx7805_send_msg(unsigned char addr, unsigned char *buf, unsigned short len, unsigned short rw_flag)
{
	int rc;

	anx7805_client->addr = addr;
//	D("%s:anx7805_client->addr = %c \n",__func__, anx7805_client->addr);

	if(rw_flag)
	{
		rc=i2c_smbus_read_byte_data(anx7805_client, *buf);
		*buf = rc;

	} else
	{
		rc=i2c_smbus_write_byte_data(anx7805_client, buf[0], buf[1]);
	}
	delay_ms(1);	
	
	return 0;
}

unsigned char SP_TX_Read_Reg(unsigned char dev,unsigned char offset, unsigned char *d)
{
	unsigned char c;
	int ret;
	c = offset;

//	D("%s start!!\n",__func__);
	ret = Anx7805_send_msg(dev >> 1, &c, 1, 1);
	if(ret < 0){
		D("Colorado_send_msg err!\n");
		return 1;
	}

	*d = c;
	return 0;
}

unsigned char SP_TX_Write_Reg(unsigned char dev,unsigned char offset, unsigned char d)
{
	unsigned char buf[2] = {offset, d};

	return Anx7805_send_msg(dev >> 1, buf, 2, 0);
}

static irqreturn_t cable_detect_isr(int irq,void *dev_id)
{
	//int abc = gpio_get_value(SLIMPORT_CABLE_DETECT);
	struct anx7805_data *anx7805 = dev_id;
	
	D("=========cable_detect_isr==========");
	if(workqueue_run_flag == 0){
		workqueue_run_flag = 1;
		queue_delayed_work(anx7805->anx7805_workqueue, &anx7805->work,0);		
	}
	
	return IRQ_HANDLED;
}

int get_cable_detect_status(void)
{
#ifdef CONFIG_OF
	struct anx7805_platform_data *pdata = g_pdata;
	
		printk(KERN_INFO
		   "7805 : %s  gpio_cbl_det %d \n",
			   __func__,
			   gpio_get_value(pdata->gpio_cbl_det));
		
	return (gpio_get_value(pdata->gpio_cbl_det)==0 ?  0 : 1);
#else
	return (gpio_get_value(PM8941_GPIO_13)==0 ?  0 : 1);
#endif			
}

int hardware_power_ctl(int stat)
{
	int rc =0;
	int ret;

#ifdef CONFIG_OF
	struct anx7805_platform_data *pdata = g_pdata;
#else
	struct anx7805_platform_data *pdata = anx7805_client->dev.platform_data;
#endif
	
	D("%s start!!\n",__func__);

	if (stat) {
		gpio_set_value(pdata->gpio_reset, 0);
		msleep(20);
		gpio_set_value(pdata->gpio_p_dwn, 0);
		msleep(20);
//		pdata->dvdd_power(1);
//		msleep(100);
		gpio_set_value(pdata->gpio_reset, 1);
		msleep(20);
		ret = gpio_get_value(pdata->gpio_p_dwn);
		D("%s: gpio_p_dwn is %d\n",__func__, ret);
		rc = 1;
	}
	else {
		gpio_set_value(pdata->gpio_reset, 0);
		msleep(10);
//		pdata->dvdd_power(0);
//		msleep(10);
		gpio_set_value(pdata->gpio_p_dwn, 1);
		msleep(20);
		ret = gpio_get_value(pdata->gpio_p_dwn);
		D("%s: gpio_p_dwn is %d\n",__func__, ret);
		rc = 1;
	}
		
	return rc;
}

static int anx7805_System_Init(void)
{
    int ret;
		D("%s start!!",__func__);
    ret = SP_CTRL_Chip_Detect();
		
    if(ret<0)
    {
        D("Chip detect error\n");
        return -ENODEV;
    }

    //Chip initialization
    SP_CTRL_Chip_Initial();
    return 0;

}


static const struct i2c_device_id anx7805_register_id[] = {
	{ "anx7805", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, anx7805_register_id);


#ifdef CONFIG_OF
static struct of_device_id anx_match_table[] = {
    { .compatible = "analogix,anx7805",},
    { },
};

#endif


void anx7805_work_func(struct work_struct * work)
{
	struct anx7805_data *td = container_of(work, struct anx7805_data,
								work.work);

#ifndef EYE_TEST

	SP_CTRL_Main_Procss();
	if((sp_tx_system_state == SP_TX_WAIT_SLIMPORT_PLUGIN)
		&&(sp_tx_pd_mode==1))
	{
		//cancel_delayed_work(&work);
		workqueue_run_flag = 0;
		D("cancel_delayed_work");
	}
	
	if(workqueue_run_flag== 1)
		queue_delayed_work(td->anx7805_workqueue, &td->work, msecs_to_jiffies(500));
#endif

}


#ifdef CONFIG_OF
int anx7805_regulator_configure(
	struct device *dev, struct anx7805_platform_data *pdata)
{
	int rc = 0;
/* To do : regulator control after H/W change */
	return rc;
/*
	pdata->avdd_10 = regulator_get(dev, "analogix,vdd_ana");

	if (IS_ERR(pdata->avdd_10)) {
		rc = PTR_ERR(pdata->avdd_10);
		pr_err("%s : Regulator get failed avdd_10 rc=%d\n",
			   __func__, rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->avdd_10) > 0) {
		rc = regulator_set_voltage(pdata->avdd_10, 2850000,
							2850000);
		if (rc) {
			pr_err("%s : Regulator set_vtg failed rc=%d\n",
				   __func__, rc);
			goto error_set_vtg_avdd_10;
		}
	}

	pdata->dvdd_10 = regulator_get(dev, "analogix,vdd_dig");
	if (IS_ERR(pdata->dvdd_10)) {
		rc = PTR_ERR(pdata->dvdd_10);
		pr_err("%s : Regulator get failed dvdd_10 rc=%d\n",
			   __func__, rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->dvdd_10) > 0) {
		rc = regulator_set_voltage(pdata->dvdd_10, 2850000,
							2850000);
		if (rc) {
			pr_err("%s : Regulator set_vtg failed rc=%d\n",
				   __func__, rc);
			goto error_set_vtg_dvdd_10;
		}
	}

	return 0;

error_set_vtg_dvdd_10:
	regulator_put(pdata->dvdd_10);
error_set_vtg_avdd_10:
	regulator_put(pdata->avdd_10);

	return rc;
*/	
}

static int anx7805_parse_dt(
	struct device *dev, struct anx7805_platform_data *pdata)
{
	int rc = 0;
	struct device_node *np = dev->of_node;

	pdata->gpio_p_dwn = of_get_named_gpio_flags(
		np, "analogix,p-dwn-gpio", 0, NULL);

	pdata->gpio_reset = of_get_named_gpio_flags(
		np, "analogix,reset-gpio", 0, NULL);

	pdata->gpio_int = of_get_named_gpio_flags(
		np, "analogix,irq-gpio", 0, NULL);

	pdata->gpio_cbl_det = of_get_named_gpio_flags(
		np, "analogix,cbl-det-gpio", 0, NULL);

	printk(KERN_INFO
		   "%s: gpio p_dwn : %d, reset : %d, irq : %d, gpio_cbl_det %d \n",
		   __func__,
		   pdata->gpio_p_dwn,
		   pdata->gpio_reset,
		   pdata->gpio_int,
		   pdata->gpio_cbl_det);

	if (anx7805_regulator_configure(dev, pdata) < 0) {
		pr_err("%s: parsing dt for anx7805 is failed.\n", __func__);
		return rc;
	}

// connects function nodes which are not provided with dts //
//	pdata->avdd_power = slimport_avdd_power;
//	pdata->dvdd_power = slimport_dvdd_power;

	return rc;
}
#else
static int anx7805_parse_dt(
	struct device *dev, struct anx7805_platform_data *pdata)
{
	return -ENODEV;
}
#endif


static int anx7805_gpio_init_func(struct anx7805_data *anx7805)
{
	int rc = 0;
	
	D("anx7805 init gpio \n");

	rc = gpio_request(anx7805->pdata->gpio_int, "anx7805_int_n"); 
	if(rc)		
		goto err1; 
	else		
		gpio_direction_output(anx7805->pdata->gpio_int, 0);
	
	rc = gpio_request(anx7805->pdata->gpio_reset, "anx7805_reset_n");
	if(rc)		
		goto err2;
	else		
		gpio_direction_output(anx7805->pdata->gpio_reset ,0);
	
	rc =  gpio_request(anx7805->pdata->gpio_cbl_det, "anx7805_cbl_det");
	if(rc)		
		goto err3; 
	else {		
		gpio_direction_input(anx7805->pdata->gpio_cbl_det);
		rc = get_cable_detect_status();
		pr_info("1-get_cable_detect_status return = %d\n", rc); 
	}
	rc = gpio_request(anx7805->pdata->gpio_p_dwn, "anx_p_dwn_ctl");
	if(rc)		
		goto err4;
	else		
		gpio_direction_output(anx7805->pdata->gpio_p_dwn, 0);
	
	gpio_set_value(anx7805->pdata->gpio_reset, 0);
	gpio_set_value(anx7805->pdata->gpio_p_dwn, 1);	
	gpio_set_value(anx7805->pdata->gpio_cbl_det, 0);	
	
	rc = gpio_get_value(anx7805->pdata->gpio_p_dwn);
	pr_info("gpio_p_dwn value = %d\n", rc);
	rc = get_cable_detect_status();
	pr_info("gpio_cbl_det value = %d\n", rc);
	pr_info("ANX7805 gpio init finish\n");
	return 0;

err4:
	gpio_free(anx7805->pdata->gpio_p_dwn);	
err3:
	gpio_free(anx7805->pdata->gpio_cbl_det);
err2:
	gpio_free(anx7805->pdata->gpio_reset);
err1:
	gpio_free(anx7805->pdata->gpio_int);

	return rc;

}


static void anx7805_gpio_free_func(struct anx7805_data *anx7805)
{
	gpio_free(anx7805->pdata->gpio_p_dwn);
	gpio_free(anx7805->pdata->gpio_cbl_det);
	gpio_free(anx7805->pdata->gpio_reset);
	gpio_free(anx7805->pdata->gpio_int);	
}

/* debugging */
static ssize_t
slimport_fake_cbl_det(
	struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	int cmd;
	int rc;

	sscanf(buf, "%d", &cmd);
	switch (cmd) {
	case 0:
		gpio_set_value(g_pdata->gpio_cbl_det, 0);
		break;
	case 1:
		gpio_set_value(g_pdata->gpio_cbl_det, 1);
		break;
	}
	rc = get_cable_detect_status();
	pr_info("gpio_cbl_det value = %d\n", rc);
	return count;
}

static struct device_attribute slimport_device_attrs[] = {
	__ATTR(cbl_det, S_IRUGO | S_IWUSR, NULL, slimport_fake_cbl_det),
};


static int anx7805_i2c_probe(struct i2c_client *client,
																const struct i2c_device_id *id)
{
	int ret = 0;
	int i;
//	bool bdata; 
	struct anx7805_data *anx7805;
	struct anx7805_platform_data *pdata;
		
  D("7805 %s: start!!\n",__func__);      
  
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c bus does not support the anx7805\n", __func__);
		ret = -ENODEV;
		goto probe_fail;
	}
	anx7805 = kzalloc(sizeof(struct anx7805_data), GFP_KERNEL);
	if (!anx7805) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto probe_fail;
	}
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
							 sizeof(struct anx7805_platform_data),
							 GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		client->dev.platform_data = pdata;
/* device tree parsing function call */
		ret = anx7805_parse_dt(&client->dev, pdata);
		if (ret != 0) /* if occurs error */
			return ret;

		anx7805->pdata = pdata;
	} else {
		anx7805->pdata = client->dev.platform_data;
	}

	/* to access global platform data */
	g_pdata = anx7805->pdata;

	memcpy(&anx7805_client, &client, sizeof(client));

	ret = anx7805_gpio_init_func(anx7805);
	if (ret) {
		pr_err("%s: failed to initialize gpio\n", __func__);
		goto probe_fail;
	}

	ret = anx7805_System_Init();

	INIT_DELAYED_WORK(&anx7805->work, anx7805_work_func);

	anx7805->anx7805_workqueue = create_singlethread_workqueue("anx7805_work");
	if (anx7805->anx7805_workqueue == NULL) {
		pr_err("%s: failed to create work queue\n", __func__);
		ret = -ENOMEM;
		goto probe_fail;
	}
	
	client->irq = gpio_to_irq(anx7805->pdata->gpio_cbl_det);
	if (client->irq < 0) {
		pr_err("%s : failed to get gpio irq\n", __func__);
		goto probe_fail;
	}
	
	ret = request_threaded_irq(client->irq, NULL, cable_detect_isr,
					IRQF_TRIGGER_RISING
					| IRQF_TRIGGER_FALLING,
					"anx7805", anx7805);
	
	if (ret  < 0) {
		pr_err("%s : failed to request irq\n", __func__);
		goto probe_fail;
	}		

/*	cable_detect_irq = request_irq(client->irq, cable_detect_isr, IRQF_TRIGGER_RISING
					| IRQF_TRIGGER_FALLING, "cable_detect", &bdata);		
	
	D("%s: request_irq, cable_detect_irq = %d \n",__func__, cable_detect_irq);	
*/	
	
	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++) {
		ret = device_create_file(
			&client->dev, &slimport_device_attrs[i]);
		if (ret) {
			printk(KERN_ERR "anx7808 sysfs register failed\n");
			goto probe_fail;
		}
	}

	return ret;

probe_fail:
	D("anx7805 probe error! \n");
	
	return ret;
}

static int __devexit anx7805_i2c_remove( struct i2c_client *client )
{
	struct anx7805_data *anx7805 = i2c_get_clientdata(client);

	free_irq(client->irq, anx7805);
	anx7805_gpio_free_func(anx7805);
	destroy_workqueue(anx7805->anx7805_workqueue);
	kfree(anx7805);
	return 0;
}

static struct i2c_driver anx7805_i2c_driver = {
    .driver = {
	  .name   = "anx7805",
	  .owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = anx_match_table,
#endif	  
     },
    
    .probe            = anx7805_i2c_probe,
    .remove  					= anx7805_i2c_remove,
    .id_table         = anx7805_register_id,
};

static int __init anx7805_init(void)
{
	
	// need to register i2c driver
	int ret = 0;
	D("%s 7805 init start\n",__func__);
	ret = i2c_add_driver(&anx7805_i2c_driver);
	if (ret < 0)
		D("%s: fail to register anx7805 i2c driver\n", __func__);
	
	return ret;	 
}


static void __exit anx7805_exit(void)
{
	i2c_del_driver(&anx7805_i2c_driver);
}

module_init(anx7805_init);
module_exit(anx7805_exit);

MODULE_DESCRIPTION ("Slimport  transmitter ANX7805 driver");
MODULE_AUTHOR("FeiWang<fwang@analogixsemi.com>");
MODULE_LICENSE("GPL");


#undef _ANX7805_C_


