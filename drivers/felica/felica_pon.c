/*
 *  felica_pon.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include "felica_pon.h"
#include "felica_gpio.h"

#include "felica_test.h"

//#ifdef FELICA_UART_DEBUG
//#include "felica_uart.h"
//#endif
/*
 *  DEFINE
 */

//enable this feature for checking port ready packet when PON was set to HIGH.
//#define FELICA_UART_DEBUG

/*
 *    INTERNAL DEFINITION
 */

static int isopen = 0; // 0 : No open 1 : Open

/*
 *    FUNCTION DEFINITION
 */

/*
* Description : MFC calls this function using close method(void open()) of FileOutputStream class
*               When this fuction is excuted, set PON to Low.
* Input : None
* Output : Success : 0 Fail : Other
*/
static int felica_pon_open (struct inode *inode, struct file *fp)
{
  int rc = 0;

  if(1 == isopen)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_open - already open \n");
    #endif
    return -1;
  }
  else
  {
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_open - start \n");
    #endif
    isopen = 1;
  }

  rc = felica_gpio_open(GPIO_FELICA_PON, GPIO_DIRECTION_OUT, GPIO_LOW_VALUE);

#ifdef FELICA_UART_DEBUG
  mdelay(100);
  felica_uart_open();
#endif

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_open - end \n");
  #endif

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_open - result_open(%d)  \n",result_open_pon);
  return result_open_pon;
#else
  return rc;
#endif
}

/*
* Description : MFC calls this function using write method(void write(int oneByte)) of FileOutputStream class
* Input : PON low : 0 PON high : 1
* Output : Success : 0 Fail : Other
*/
static ssize_t felica_pon_write(struct file *fp, const char *buf, size_t count, loff_t *pos)
{
  int rc = 0;
  int SetValue = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_write - start \n");
  #endif

  //FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_write current_uid : %d \n",current_uid());

  /* Check error */
	if(NULL == fp)
	{
    #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_PON] ERROR fp is NULL \n");
	#endif
	  return -1;
	}
  
	if(NULL == buf)
	{
    #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_PON] ERROR buf is NULL \n");
	#endif
	  return -1;
	}
  
	if(1 != count)
	{
    #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_PON] ERROR count(%d) \n",count);
	#endif
	  return -1;
	}
  
	if(NULL == pos)
	{
    #ifdef FEATURE_DEBUG_HIGH
	  FELICA_DEBUG_MSG("[FELICA_PON] ERROR pos is NULL \n");
	#endif
	  return -1;
	}

  rc = copy_from_user(&SetValue, (void*)buf, count);
  if(rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR - copy_from_user \n");
	#endif
    return rc;
  }

  if((GPIO_LOW_VALUE != SetValue)&&(GPIO_HIGH_VALUE != SetValue))
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR - SetValue is out of range \n");
	#endif
    return -1;
  }
  else if(GPIO_LOW_VALUE != SetValue)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_PON] ========> ON \n");
	#endif
  }
  else if(GPIO_HIGH_VALUE != SetValue)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_PON] <======== OFF \n");
	#endif
  }

  felica_gpio_write(GPIO_FELICA_PON, SetValue);

  mdelay(20);

#ifdef FELICA_UART_DEBUG
  if (SetValue == 1)
  {
    int len, i;
    char buf[256];

    len = felica_uart_read(buf, 256);

    for (i = 0; i < len; i++)
     pr_info("%s: felica_uart_read [%d] 0x%02X\n", __func__, i, buf[i]);
  }
#endif

#ifdef FELICA_FN_DEVICE_TEST
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_write - result_write_pon(%d) \n",result_write_pon);
    return result_write_pon;
#else
    return 1;
#endif
}

/*
* Description : MFC calls this function using close method(void close()) of FileOutputStream class
*               When this fuction is excuted, set PON to Low.
* Input : None
* Output : Success : 0 Fail : Other
*/
static int felica_pon_release (struct inode *inode, struct file *fp)
{
  if(0 == isopen)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_release - not open \n");
    #endif

    return -1;
  }
  else
  {
    #ifdef FEATURE_DEBUG_LOW
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_release - start \n");
    #endif

    isopen = 0;
  }

#ifdef FELICA_UART_DEBUG
  felica_uart_close();
#endif

  felica_gpio_write(GPIO_FELICA_PON, GPIO_LOW_VALUE);

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_release - end \n");
  #endif

#ifdef FELICA_FN_DEVICE_TEST
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_close - result_close_pon(%d)  \n",result_close_pon);
  return result_close_pon;
#else
  return 0;
#endif

}

/*
 *    STRUCT DEFINITION
 */

static struct file_operations felica_pon_fops =
{
  .owner    = THIS_MODULE,
  .open      = felica_pon_open,
  .write    = felica_pon_write,
  .release  = felica_pon_release,
};

static struct miscdevice felica_pon_device =
{
  .minor = MINOR_NUM_FELICA_PON,
  .name = FELICA_PON_NAME,
  .fops = &felica_pon_fops,
};

static int felica_pon_init(void)
{
  int rc = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_init - start \n");
  #endif

  /* register the device file */
  rc = misc_register(&felica_pon_device);
  if (rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR can not register felica_pon \n");
	#endif
    return rc;
  }

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_init - end \n");
  #endif

  return 0;
}

static void felica_pon_exit(void)
{
  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_exit - start \n");
  #endif

  /* deregister the device file */
  misc_deregister(&felica_pon_device);

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_exit - end \n");
  #endif
}

module_init(felica_pon_init);
module_exit(felica_pon_exit);

MODULE_LICENSE("Dual BSD/GPL");
