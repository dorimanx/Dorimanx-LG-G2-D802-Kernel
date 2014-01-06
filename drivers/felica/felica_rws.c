/*
 *  felica_rws.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include "felica_rws.h"
#include "felica_gpio.h"

#include "felica_test.h"

/*
 *  DEFINE
 */
enum{
  RWS_AVAILABLE = 0,
  RWS_NOT_AVAILABLE,
};

/* Debug intent */
/* #define FELICA_INTENT "my.andr.u5/.FeliCaTest" */
#define FELICA_INTENT "com.felicanetworks.mfc/com.felicanetworks.adhoc.AdhocReceiver"

/*
 *    INTERNAL DEFINITION
 */
static int isopen = 0; // 0 : No open 1 : Open


/*
 *    FUNCTION PROTOTYPE
 */
static void felica_int_low_work(struct work_struct *data);
static DECLARE_DELAYED_WORK(felica_int_work, felica_int_low_work);

/*
 *    FUNCTION DEFINITION
 */
static int invoke_felica_apk(void)
{
  char *argv[] = { "/system/bin/sh","/system/bin/am", "start", "-n", FELICA_INTENT, "--activity-clear-top", NULL };

  static char *envp[] = {FELICA_LD_LIBRARY_PATH,FELICA_BOOTCLASSPATH,FELICA_PATH,NULL };
  int rc = 0;

  #ifdef FEATURE_DEBUG_HIGH
  FELICA_DEBUG_MSG("[FELICA_RWS] invoke felica app... \n");
  #endif

  rc = call_usermodehelper( argv[0], argv, envp, UMH_WAIT_EXEC );

  #ifdef FEATURE_DEBUG_HIGH
  FELICA_DEBUG_MSG("[FELICA_RWS] felica app result : %d \n", rc);
  #endif

  return rc;
}

static void felica_int_low_work(struct work_struct *data)
{
  int rc = 0;

  lock_felica_wake_lock();
  disable_irq_nosync(gpio_to_irq(felica_get_int_gpio_num()));

  usermodehelper_enable();

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_int_low_work - start \n");
  #endif

  rc = invoke_felica_apk();

  if(rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RWS] Error - invoke app \n");
	#endif
    unlock_felica_wake_lock();
  }

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_int_low_work - end \n");
  #endif

  enable_irq(gpio_to_irq(felica_get_int_gpio_num()));
}

static irqreturn_t felica_int_low_isr(int irq, void *dev_id)
{
  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_int_low_isr - start \n");
  #endif

  schedule_delayed_work(&felica_int_work,0);

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_int_low_isr - end \n");
  #endif

  return IRQ_HANDLED;
}
/*
* Description : MFC calls this function using close method(void open()) of FileOutputStream class
*               When this fuction is excuted, set PON to Low.
* Input : None
* Output : Success : 0 Fail : Other
*/
static int felica_rws_open (struct inode *inode, struct file *fp)
{
  int rc = 0;

  if(1 == isopen)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_open - already open \n");
    #endif
    return -1;
  }
  else
  {
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_open - start \n");
    #endif
    isopen = 1;
  }

  rc = felica_gpio_open(felica_get_int_gpio_num(), GPIO_DIRECTION_IN, GPIO_HIGH_VALUE);

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_open - end \n");
  #endif

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_open - result(%d) \n",result_open_rws);
  return result_open_rws;
#else
    return rc;
#endif
}

/*
 * Description: MFC calls this function using read method(int read()) of FileInputStream class
 * Input: None
 * Output: INT low : RWS not available INT high : available
 */
static ssize_t felica_rws_read(struct file *fp, char *buf, size_t count, loff_t *pos)
{
  int rc = 0;
  int getvalue = GPIO_HIGH_VALUE;    /* Default status*/

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_read - start \n");
  #endif

  /* Check error */
  if(NULL == fp)
	{
	#ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RWS] ERROR fp is NULL \n");
	#endif
	  return -1;
	}
  
	if(NULL == buf)
	{
	#ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RWS] ERROR buf is NULL \n");
	#endif
	  return -1;
	}
  
	if(1 != count)
	{
	#ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RWS] ERROR count(%d) \n",count);
	#endif
	  return -1;
	}
  
	if(NULL == pos)
	{
	#ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_RWS] ERROR pos is NULL \n");
	#endif
	  return -1;
	}


/* Get GPIO value */
  getvalue = felica_gpio_read(felica_get_int_gpio_num());

  if((GPIO_LOW_VALUE != getvalue)&&(GPIO_HIGH_VALUE != getvalue))
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RFS] ERROR - getvalue is out of range \n");
	#endif
    return -1;
  }

/* Change GPIO value to RWS value */
  getvalue = getvalue ? RWS_AVAILABLE : RWS_NOT_AVAILABLE;

  #ifdef FEATURE_DEBUG_MED
  FELICA_DEBUG_MSG("[FELICA_RWS] RWS status : %d \n", getvalue);
  #endif

/* Copy value to user memory */
  rc = copy_to_user((void *)buf, (void *)&getvalue, count);
  if(rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RWS] ERROR -  copy_to_user \n");
	#endif
    return rc;
  }

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rfs_read - end \n");
  #endif

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rfs_read - result(%d) \n",result_read_rws);
  if(result_read_rws != -1)
    result_read_rws = count;
  return result_read_rws;
#else
  return count;
#endif
}

/*
* Description : MFC calls this function using close method(void close()) of FileOutputStream class
*               When this fuction is excuted, set PON to Low.
* Input : None
* Output : Success : 0 Fail : Other
*/
static int felica_rws_release (struct inode *inode, struct file *fp)
{
  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_release - start \n");
  #endif

  if(0 == isopen)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_release - not open \n");
    #endif

    return -1;
  }
  else
  {
    isopen = 0;

    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_release - end \n");
    #endif
  }

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_release - result(%d) \n",result_close_rws);
  return result_close_rws;
#else
    return 0;
#endif
}

/*
 *    STRUCT DEFINITION
 */


static struct file_operations felica_rws_fops =
{
  .owner    = THIS_MODULE,
  .open      = felica_rws_open,
  .read      = felica_rws_read,
  .release  = felica_rws_release,
};

static struct miscdevice felica_rws_device =
{
  .minor = MINOR_NUM_FELICA_RWS,
  .name = FELICA_RWS_NAME,
  .fops = &felica_rws_fops,
};

static int felica_rws_init(void)
{
  int rc = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_init - start \n");
  #endif

  /* register the device file */
  rc = misc_register(&felica_rws_device);
  if (rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RWS] FAIL!! can not register felica_int \n");
	#endif
    return rc;
  }

  rc= request_irq(gpio_to_irq(felica_get_int_gpio_num()), felica_int_low_isr, IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND, FELICA_RWS_NAME, NULL);

  if (rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_RWS] FAIL!! can not request_irq = %d \n",rc);
	#endif
    return rc;
  }

/* wake up a device from sleep mode by coming up this interrupts */
  irq_set_irq_wake(gpio_to_irq(felica_get_int_gpio_num()),1);

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_RWS] felica_rws_init - end \n");
  #endif

  return 0;
}

static void felica_rws_exit(void)
{
  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_INT] felica_rws_exit - start \n");
  #endif

  free_irq(gpio_to_irq(felica_get_int_gpio_num()), NULL);

  misc_deregister(&felica_rws_device);

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_INT] felica_rws_exit - end \n");
  #endif
}

module_init(felica_rws_init);
module_exit(felica_rws_exit);

MODULE_LICENSE("Dual BSD/GPL");
