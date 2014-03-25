/*
 *  snfc_uart_collsion_control.c
 *  
 */
 /*
  *    Include header files
  */
#include "snfc_uart_collision_control.h"

/*
*	Define
*/

/*
*	Internal definitions
*/
/*
*	Internal variables
*/
static int isopen_snfcuartcontrol = 0; // 0 : No open 1 : Opend
_e_snfc_uart_status g_uartcollisoncontrol = UART_STATUS_KOTO_OFF;
static int gpio_init = 0;
static int forced_hsel_up_flag=0;
static int forced_pon_up_flag=0;	
static int koto_abnormal=0;
static int autopoll_status = 0;

int snfc_poweroff_flag=0;

extern struct snfc_gp snfc_gpios;
/* 
 *	Function definitions
 */

/*
* Description : open uart collision control
* Input : 
* Output : 
*/
void __snfc_uart_control_set_uart_status(_e_snfc_uart_status uart_status)
{
	_e_snfc_uart_status current_status = g_uartcollisoncontrol;
	
	if(current_status == uart_status)
		return;
	
	g_uartcollisoncontrol = uart_status;
	SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] uart status %d -> %d\n", current_status, g_uartcollisoncontrol );

	return;
}

EXPORT_SYMBOL(__snfc_uart_control_set_uart_status);
/*
* Description : open uart collision control
* Input : 
* Output : 
*/
_e_snfc_uart_status __snfc_uart_control_get_uart_status(void)
{
	return g_uartcollisoncontrol;
}
EXPORT_SYMBOL(__snfc_uart_control_get_uart_status);
/*
* Description : open uart collision control
* Input : 
* Output : 
*/
static int snfc_uart_control_open(struct inode *inode, struct file *fp)
{
    int rc = 0;

    if(isopen_snfcuartcontrol == 1)
    {
        SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_open - already open \n");
        return 0;
    }
    SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_open - start \n");
    isopen_snfcuartcontrol = 1;

    __snfc_uart_control_set_uart_status(UART_STATUS_READY);

        if(gpio_init ==0)
        {
            rc = gpio_request(snfc_gpios.gpio_hvdd,"snfc_hvdd");
            if(rc){
                SNFC_DEBUG_MSG("[snfc_intu_poll] gpio_request snfc_hvdd fail\n");
            }
            snfc_gpio_write(snfc_gpios.gpio_hvdd, GPIO_HIGH_VALUE);

            rc = gpio_request(snfc_gpios.gpio_uicc_con, "snfc_uicc_con");
            {
                SNFC_DEBUG_MSG("[snfc_driver] gpio_request snfc_uicc_con fail\n");
            }
            snfc_gpio_write(snfc_gpios.gpio_uicc_con, GPIO_LOW_VALUE);

            gpio_init = 1;                
        }

    SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_open - end \n");

    return rc;
}

/*
* Description : 
* Input : 
* Output : 
*/
static int snfc_uart_control_release (struct inode *inode, struct file *fp)
{
	if(isopen_snfcuartcontrol == 0)
	{
		SNFC_DEBUG_MSG("[snfc_uart_control] snfc_uart_control_release - not open \n");
		return -1;
	}

	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_release - start \n");

	isopen_snfcuartcontrol = 0;

	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_release - end \n");

	return 0;
}
/*
* Description : 
* Input : 
* Output : 
*/
static long snfc_uart_control_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	//ioctl_buf *k_buf;
	//int i,err;
	int size;
	_e_snfc_uart_status current_status;
	int autopoll_wait_cnt;
	int break_cnt;
	unsigned char write_buf = 0x00/*, read_buf = 0x00*/;
	int rc =0;
	
	size = _IOC_SIZE(cmd);
	SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] snfc_uart_control_ioctl - start,cmd =%d\n", cmd);
		
	current_status = __snfc_uart_control_get_uart_status();
	if( current_status == UART_STATUS_FOR_FELICA )
	{
		SNFC_DEBUG_MSG("[snfc_uart_control] snfc_uart_control_ioctl, UART is used to FeliCa\n");
		return -1;
	}

	__snfc_uart_control_set_uart_status(UART_STATUS_FOR_NFC);	

	switch(cmd)
	{
		case IOCTL_SNFC_START_SETTING :
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_SETTING - start\n");
			if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
				break;
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_HIGH_VALUE);			
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_HIGH_VALUE);
			mdelay(10); 
            SNFC_DEBUG_MSG_LOW("[snfc_uart_control] hsel %d, pon %d\n",
                snfc_gpios.gpio_hsel,snfc_gpios.gpio_pon);
            SNFC_DEBUG_MSG_LOW("[snfc_driver] GPIO_SNFC_PON = %d, GPIO_SNFC_HSEL = %d, GPIO_SNFC_HBDD = %d\n",
                snfc_gpio_read(snfc_gpios.gpio_pon),snfc_gpio_read(snfc_gpios.gpio_hsel),snfc_gpio_read(snfc_gpios.gpio_hvdd)  );             
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_SETTING - end\n");
			break;
			
		case IOCTL_SNFC_START_AUTOPOLL :
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_AUTOPOLL - start\n");
			autopoll_wait_cnt = 0;
			break_cnt = 0;
			if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
				break;
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_HIGH_VALUE);	
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_HIGH_VALUE);

			#ifndef CONFIG_CXD2235AGG_GJ_KDDI	
				//SNFC_DEBUG_MSG("[snfc_uart_control] !!!! RFS disable start !!!!\n");
				disable_irq(gpio_to_irq(snfc_gpios.gpio_rfs));
			#endif
			
			mdelay(10);
			autopoll_status = 1;		
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_AUTOPOLL - end\n");
			break;
			
		case IOCTL_SNFC_START_RW :
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_RW - start\n");
			if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
				break;
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_HIGH_VALUE);				
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_HIGH_VALUE);
			mdelay(10);
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_RW - end\n");
			break;
			
		case IOCTL_SNFC_START_TARGET :
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_TARGET - start\n");
			if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
				break;	
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_HIGH_VALUE);				
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_HIGH_VALUE);
			if(GPIO_HIGH_VALUE == snfc_gpio_read(snfc_gpios.gpio_rfs))
				mdelay(10);
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_TARGET - end\n");
			break;
			
		case IOCTL_SNFC_START_INTU :
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_INTU - start\n");
			if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
				break;
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_HIGH_VALUE);				
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_HIGH_VALUE);
			//mdelay(10);
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_INTU - end\n");
			break;
			
		case IOCTL_SNFC_START_WAITSIMBOOT:
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_WAITSIMBOOT - start\n");
			if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
				break;
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_HIGH_VALUE);	
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_HIGH_VALUE);
			mdelay(10);
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_START_WAITSIMBOOT - end\n");		
			break;
			
		case IOCTL_SNFC_HSEL_UP:
			forced_hsel_up_flag = 1;
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_hsel_up\n");	
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_HIGH_VALUE);	
			break;
			
		case IOCTL_SNFC_HSEL_DOWN:	
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_hsel_down\n");	
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_LOW_VALUE);
			forced_hsel_up_flag = 0;
			if(forced_pon_up_flag == 0 && forced_hsel_up_flag == 0)
				__snfc_uart_control_set_uart_status(UART_STATUS_READY);
			break;
			
		case IOCTL_SNFC_PON_UP:
			forced_pon_up_flag = 1;
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_pon_up\n");	
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_HIGH_VALUE);
			mdelay(10);
			break;
			
		case IOCTL_SNFC_PON_DOWN:	
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_pon_down\n");	
			forced_pon_up_flag = 0;
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_LOW_VALUE);	
			if(forced_pon_up_flag == 0 && forced_hsel_up_flag == 0)
				__snfc_uart_control_set_uart_status(UART_STATUS_READY);
			break;

		case IOCTL_SNFC_BOOT_CEN_HI:		//Kernel init only
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_boot_hi\n");
			SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] CEN = High (UNLOCK) \n");
			write_buf = 0x81; // set unlock  
			//mutex_lock(&nfc_cen_mutex);	
			rc = snfc_i2c_write(0x02, &write_buf, 1);
			//mutex_unlock(&nfc_cen_mutex);   
			break;
			
		case IOCTL_SNFC_BOOT_CEN_LO:	//Kernel init only
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] ioctl_snfc_boot_low\n");
			SNFC_DEBUG_MSG_MIDDLE("[snfc_uart_control] CEN = Low (LOCK) \n");
			write_buf = 0x80; // set lock
			//mutex_lock(&nfc_cen_mutex);
			rc = snfc_i2c_write(0x02, &write_buf, 1);
			//mutex_unlock(&nfc_cen_mutex);  			
			break;
			
		case IOCTL_SNFC_END :
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_END - start\n");
			if(forced_pon_up_flag == 1 || forced_hsel_up_flag == 1)
			{
				SNFC_DEBUG_MSG("[snfc_uart_control] pon & hsel forced up!! pon and/or sel will keep high\n");
				break;
			}
			if(current_status != UART_STATUS_FOR_NFC)
			{
				SNFC_DEBUG_MSG("[snfc_uart_control] IOCTL_SNFC_END, UART is not used to NFC\n");
				//return -2;
			}				
			snfc_gpio_write(snfc_gpios.gpio_hsel, GPIO_LOW_VALUE);
			snfc_gpio_write(snfc_gpios.gpio_pon, GPIO_LOW_VALUE);	
			__snfc_uart_control_set_uart_status(UART_STATUS_READY);
			
			#ifndef CONFIG_CXD2235AGG_GJ_KDDI	
				if(autopoll_status == 1) 
				{
					//SNFC_DEBUG_MSG("[snfc_uart_control] !!!! RFS disable end !!!!\n");
					enable_irq(gpio_to_irq(snfc_gpios.gpio_rfs));
				}
			#endif
			
			autopoll_status = 0;
			SNFC_DEBUG_MSG_LOW("[snfc_uart_control] IOCTL_SNFC_END - end (hsel low)(pon low)\n");
			break;			
			
	}
	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_ioctl - end\n");

	return 0;
}
/*
* Description : 
* Input : 
* Output : 
*/
int snfc_temp_flag = 0;
static int snfc_uart_control_read(struct file *pf, char *pbuf, size_t size, loff_t *pos)
{
	int current_status;
	int rc;
		
	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_read - start \n");
	
	current_status = koto_abnormal;	
	
	rc = copy_to_user((void*)pbuf, (void*)&current_status, size);
	if(rc)
	{
		SNFC_DEBUG_MSG("[snfc_uart_control] ERROR -  copy_to_user \n");
		return rc;
	}

	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_read :koto_abnormal=%d - end \n",koto_abnormal);
	
	return size;
}
/*
* Description : 
* Input : 
* Output : 
*/
static int snfc_uart_control_write(struct file *pf, const char *pbuf, size_t size, loff_t *pos)
{
	int new_status;
	int rc;
	
	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_write - start \n");
	
	rc = copy_from_user(&new_status, (void*)pbuf, size);
	if(rc)
	{
		SNFC_DEBUG_MSG("[snfc_uart_control] ERROR -  copy_to_user \n");
		return rc;
	}
	
	//if(autopoll_status == 1)
		koto_abnormal = new_status;	

	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_write - end:koto_abnormal=%d \n",koto_abnormal);

	return size;
}

static struct file_operations snfc_uart_control_fops = 
{
	.owner    = THIS_MODULE,
	.open     = snfc_uart_control_open,
	.read		= snfc_uart_control_read,
	.write    = snfc_uart_control_write,
	.unlocked_ioctl = snfc_uart_control_ioctl,
	.release  = snfc_uart_control_release,
};

struct miscdevice snfc_uart_control_device = 
{
	.minor = 126,
	.name = "snfc_uart_control",
	.fops = &snfc_uart_control_fops,
};


int snfc_uart_control_probe(struct device_node *np)
{
    int rc;

    snfc_gpios.gpio_hsel = of_get_named_gpio_flags(np, "sony,hsel-gpio", 0, NULL);
    snfc_gpios.gpio_pon = of_get_named_gpio_flags(np, "sony,pon-gpio", 0, NULL);
    snfc_gpios.gpio_hvdd = of_get_named_gpio_flags(np, "sony,hvdd-gpio", 0, NULL);
    snfc_gpios.gpio_uicc_con = of_get_named_gpio_flags(np, "sony,uicc_con", 0, NULL);
    //SNFC_DEBUG_MSG("[snfc_driver] of_get_named_gpio_flags gpio_hsel %d gpio_pon %d gpio_hvdd %d\n",
    //    snfc_gpios.gpio_hsel,snfc_gpios.gpio_pon,snfc_gpios.gpio_hvdd );     

    rc = gpio_request(snfc_gpios.gpio_hsel, "snfc_hsel");
    if (rc)
    {
        SNFC_DEBUG_MSG("[snfc_driver] gpio_request snfc_hsel fail\n");
    }
    rc = gpio_request(snfc_gpios.gpio_pon, "snfc_pon");
    if (rc)
    {
        SNFC_DEBUG_MSG("[snfc_driver] gpio_request snfc_pon fail\n");
    }

    SNFC_DEBUG_MSG_LOW("[snfc_driver] GPIO_SNFC_PON = %d, GPIO_SNFC_HSEL = %d\n",
        snfc_gpio_read(snfc_gpios.gpio_pon),snfc_gpio_read(snfc_gpios.gpio_hsel) ); 

    /* register the device file */
    rc = misc_register(&snfc_uart_control_device);
    if (rc)
    {
        SNFC_DEBUG_MSG("[snfc_driver] FAIL!! can not register snfc_uart_control \n");
        return rc;
    }
    return rc;
}
void snfc_uart_control_remove(void)
{
    /* deregister the device file */
    misc_deregister(&snfc_uart_control_device);
}

/*
* Description : 
* Input : 
* Output : 
*/
#if 0
static int snfc_uart_control_init(void)
{
//	int rc=0;

	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_init - start \n");

	SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_init - end \n");

	return 0;  	
}

/*
* Description : 
* Input : 
* Output : 
*/
static void snfc_uart_control_exit(void)
{
  SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_exit - start \n");

  
  SNFC_DEBUG_MSG_LOW("[snfc_uart_control] snfc_uart_control_exit - end \n");
}

module_init(snfc_uart_control_init);
module_exit(snfc_uart_control_exit);
#endif

MODULE_LICENSE("Dual BSD/GPL");

