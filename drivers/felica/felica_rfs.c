/*
 *  felica_rfs.c
 *
 */

/*
 *  INCLUDE FILES FOR MODULE
 */

#include "felica_rfs.h"
#include "felica_gpio.h"

#include "felica_test.h"

/*
 *  DEFINE
 */
#ifdef FELICA_LED_SUPPORT
enum{
	RFS_LED_OFF = 0,
	RFS_LED_ON,
};
#endif
/*
 *   FUNCTION PROTOTYPE
 */

/*
 *   INTERNAL DEFINITION
 */
#ifdef FELICA_LED_SUPPORT
#if defined(CONFIG_LGE_FELICA_ONLY)
#define FELICA_LED_INTENT "com.nttdocomo.android.felicaremotelock/.LEDService"
#else
#define FELICA_LED_INTENT "com.lge.felicaservice/.LEDService"
#endif
#endif

/*
 *   INTERNAL VARIABLE
 */
static int isopen = 0; // 0 : No open 1 : Open
#ifdef FELICA_LED_SUPPORT
static int isFelicaUsed = 0; /* 2012 02 26 LGE_UPDATE_S For DCM Felica LED Blincking */
#endif
/*
 *   FUNCTION DEFINITION
*/

#ifdef FELICA_LED_SUPPORT

static void felica_rfs_interrupt_work(struct work_struct *data);
#ifdef CONFIG_LGE_FELICA_DCM
static void felica_rfs_open_after_boot_work(struct work_struct *data);
#endif
static DECLARE_DELAYED_WORK(felica_rfs_interrupt, felica_rfs_interrupt_work);
#ifdef CONFIG_LGE_FELICA_DCM
static DECLARE_DELAYED_WORK(felica_rfs_open_after_boot, felica_rfs_open_after_boot_work);
#endif

static int invoke_led_service(void)
{
	int rc = 0;
	int getvalue;
	char *argv_on[] = { "/system/bin/sh", "/system/bin/am", "startservice", "--es", "rfs", "on", "-n", FELICA_LED_INTENT, NULL };
	char *argv_off[] = { "/system/bin/sh", "/system/bin/am", "startservice", "--es", "rfs", "off", "-n", FELICA_LED_INTENT, NULL };

	static char *envp[] = {FELICA_LD_LIBRARY_PATH,FELICA_BOOTCLASSPATH,FELICA_PATH,NULL};

	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] invoke led service ... \n");
	#endif
	getvalue = felica_gpio_read(felica_get_rfs_gpio_num());
	if( isFelicaUsed ==0 && getvalue == GPIO_LOW_VALUE)
	{
    	#ifdef FEATURE_DEBUG_HIGH
		FELICA_DEBUG_MSG("[FELICA_RFS] Felica LED On ... \n");
		#endif
		lock_felica_rfs_wake_lock();
		rc = call_usermodehelper( argv_on[0], argv_on, envp, UMH_WAIT_PROC );
		isFelicaUsed = 1;
	}
	else if( isFelicaUsed ==1 && getvalue == GPIO_HIGH_VALUE)
	{
    	#ifdef FEATURE_DEBUG_HIGH
		FELICA_DEBUG_MSG("[FELICA_RFS] Felica LED Off ... \n");
		#endif
		unlock_felica_rfs_wake_lock();
		rc = call_usermodehelper( argv_off[0], argv_off, envp, UMH_WAIT_PROC );
		isFelicaUsed =0;
	}
	else	{
    	#ifdef FEATURE_DEBUG_MED
		FELICA_DEBUG_MSG("[FELICA_RFS] Felica LED exception case ... do nothing \n");
		FELICA_DEBUG_MSG("[FELICA_RFS] felica_gpio_read = %d , isFelicaUsed =%d \n",getvalue,isFelicaUsed);
		#endif
		unlock_felica_rfs_wake_lock();
#if defined(CONFIG_LGE_FELICA_ONLY)
		FELICA_DEBUG_MSG("[FELICA_RFS] Felica LED ERROR case so LED Off ... \n");
		rc = call_usermodehelper( argv_off[0], argv_off, envp, UMH_WAIT_PROC );
		isFelicaUsed =0;
#endif
	}

	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] invoke_led_service: %d \n", rc);
	#endif
	return rc;
}

static void felica_rfs_interrupt_work(struct work_struct *data)
{
	int rc = 0;

	disable_irq_nosync(gpio_to_irq(felica_get_rfs_gpio_num()));
	usermodehelper_enable();

	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_interrupt_work - start \n");
	#endif

	rc = invoke_led_service();

	if(rc)
	{
      #ifdef FEATURE_DEBUG_HIGH
      FELICA_DEBUG_MSG("[FELICA_RFS] Error - invoke app \n");
	unlock_felica_rfs_wake_lock();
	isFelicaUsed =0;
	  #endif
	}
	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_interrupt_work - end \n");
	#endif

	enable_irq(gpio_to_irq(felica_get_rfs_gpio_num()));
}
irqreturn_t felica_rfs_detect_interrupt(int irq, void *dev_id)
{
	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_detect_interrupt - start irq number : %d\n", irq);
	#endif

	schedule_delayed_work(&felica_rfs_interrupt,0);

	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_detect_interrupt - end \n");
	#endif
	return IRQ_HANDLED;
}
#endif
/*
 * Description: MFC calls this function using open method of FileInputStream class
 * Input: None
 * Output: Success : 0 Fail : Others
 */
static int felica_rfs_open (struct inode *inode, struct file *fp)
{
  int rc = 0;

  if(1 == isopen)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_open - already open \n");
    #endif

    return -1;
  }
  else
  {
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_open - start \n");
    #endif

    isopen = 1;
  }

#ifdef FELICA_LED_SUPPORT
  rc = felica_gpio_open(felica_get_rfs_gpio_num(), GPIO_DIRECTION_IN, GPIO_HIGH_VALUE);
#else
  rc = felica_gpio_open(felica_get_rfs_gpio_num(), GPIO_DIRECTION_IN, GPIO_LOW_VALUE);
#endif

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_open - end \n");
  #endif

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_open - result(%d) \n",result_open_rfs);
  return result_open_rfs;
#else
  return rc;
#endif

}

/*
 * Description: MFC calls this function using read method(int read()) of FileInputStream class
 * Input: None
 * Output: RFS low : 1 RFS high : 0
 */
static ssize_t felica_rfs_read(struct file *fp, char *buf, size_t count, loff_t *pos)
{
  int rc = 0;
  int getvalue = GPIO_LOW_VALUE;


  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_read - start \n");
  #endif

  /* Check error */
	if(NULL == fp)
	{
      #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RFS] ERROR fp is NULL \n");
      #endif
	  return -1;
	}
  
	if(NULL == buf)
	{
      #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RFS] ERROR buf is NULL \n");
      #endif
	  return -1;
	}
  
	if(1 != count)
	{
      #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RFS] ERROR count(%d) \n",count);
      #endif
	  return -1;
	}
  
	if(NULL == pos)
	{
      #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RFS] ERROR pos is NULL \n");
      #endif
	  return -1;
	}

/* Get GPIO value */
  getvalue = felica_gpio_read(felica_get_rfs_gpio_num());
  FELICA_DEBUG_MSG("[FELICA_RFS] RFS GPIO status : %d \n", getvalue);

  if((GPIO_LOW_VALUE != getvalue)&&(GPIO_HIGH_VALUE != getvalue))
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RFS] ERROR - getvalue is out of range \n");
	#endif
    return -1;
  }

/* Copy value to user memory */
  getvalue = getvalue ? GPIO_LOW_VALUE: GPIO_HIGH_VALUE;

  #ifdef FEATURE_DEBUG_MED
  FELICA_DEBUG_MSG("[FELICA_RFS] RFS status : %d \n", getvalue);
  #endif

  rc = copy_to_user((void*)buf, (void*)&getvalue, count);
  if(rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RFS] ERROR -  copy_to_user \n");
	#endif
    return rc;
  }

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_read - end \n");
  #endif

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_read - result(%d) \n",result_read_rfs);
  if(result_read_rfs != -1)
    result_read_rfs = count;

  return result_read_rfs;
#else
    return count;
#endif
}
/*
 * Description: MFC calls this function using close method(int close()) of FileInputStream class
 * Input: None
 * Output: RFS low : 1 RFS high : 0
 */
static int felica_rfs_release (struct inode *inode, struct file *fp)
{
  if(0 == isopen)
  {
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_release - not open \n");
    #endif

    return -1;
  }
  else
  {
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_release - start \n");
    #endif

    isopen = 0;
  }
#ifdef FELICA_LED_SUPPORT
 isFelicaUsed = 0;
#endif

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_release - end \n");
  #endif

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_release - result(%d) \n",result_close_rfs);
  return result_close_rfs;
#else
  return 0;
#endif
}

#ifdef CONFIG_LGE_FELICA_DCM
static void felica_rfs_open_after_boot_work(struct work_struct *data)
{
	int rc = 0;
    #ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_open_after_boot_work - start");
    #endif
	rc = felica_gpio_open(felica_get_rfs_gpio_num(), GPIO_DIRECTION_IN, GPIO_HIGH_VALUE);
    #ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_open_after_boot_work - end");
    #endif
}
#endif

static struct file_operations felica_rfs_fops =
{
  .owner    = THIS_MODULE,
  .open      = felica_rfs_open,
  .read      = felica_rfs_read,
  .release  = felica_rfs_release,
};

static struct miscdevice felica_rfs_device = {
  .minor = MINOR_NUM_FELICA_RFS,
  .name = FELICA_RFS_NAME,
  .fops = &felica_rfs_fops,
};

static int felica_rfs_init(void)
{
  int rc;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_init - start \n");
  #endif

  /* register the device file */
  rc = misc_register(&felica_rfs_device);
  if (rc < 0)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RFS] FAIL!! can not register felica_rfs \n");
	#endif
    return rc;
  }
#ifdef FELICA_LED_SUPPORT
  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] FELICA LED NEW SUPPORT !!\n");
  #endif

  rc= request_irq(gpio_to_irq(felica_get_rfs_gpio_num()), felica_rfs_detect_interrupt, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING|IRQF_NO_SUSPEND , FELICA_RFS_NAME, NULL);
  if (rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RFS] FAIL!! can not request_irq rc =%d\n",rc);
	#endif
    return rc;
  }
   irq_set_irq_wake(gpio_to_irq(felica_get_rfs_gpio_num()),1);

   init_felica_rfs_wake_lock();
#else
  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] FELICA LED NOT SUPPORT !! \n");
  #endif
#endif
  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_init - end \n");
  #endif

#if defined(FELICA_LED_SUPPORT) && defined(CONFIG_LGE_FELICA_DCM)
  schedule_delayed_work(&felica_rfs_open_after_boot, 3000);
#endif

  return 0;
}

static void felica_rfs_exit(void)
{
	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_exit - start \n");
	#endif
#ifdef FELICA_LED_SUPPORT

	free_irq(gpio_to_irq(felica_get_rfs_gpio_num()), NULL);

	destroy_felica_rfs_wake_lock();

#endif
	/* deregister the device file */
	misc_deregister(&felica_rfs_device);
	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_RFS] felica_rfs_exit - end \n");
	#endif
}

module_init(felica_rfs_init);
module_exit(felica_rfs_exit);

MODULE_LICENSE("Dual BSD/GPL");
