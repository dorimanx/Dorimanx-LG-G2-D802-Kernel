/*
 *  felica_test.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include <linux/module.h>
#include <linux/kernel.h>

#include "felica_test.h"

/*
 *    INTERNAL DEFINITION
 */
#define ON 1
#define OFF 0

//int val_uart = 0;
int result_open_uart = 0;
int result_close_uart = 0;
int result_available_uart = 0;
int result_read_uart = 0;
int result_write_uart = 0;

int result_open_pon = 0;
int result_close_pon = 0;
int result_write_pon = 1;

int result_open_cen = 0;
int result_close_cen = 0;
int result_read_cen = 1;

int result_open_rfs = 0;
int result_close_rfs = 0;
int result_read_rfs = 0;

int result_open_rws = 0;
int result_close_rws = 0;
int result_read_rws = 0;


/*
 *    FUNCTION DEFINITION
 */

/*
 * Description:
 * Input:
 * Output:
 */static int felica_test_open (struct inode *inode, struct file *fp)
{
  return 0;
}

/*
 * Description:
 * Input:
 * Output:
 */
static int felica_test_release (struct inode *inode, struct file *fp)
{
  return 0;
}

/*
 * Description:
 * Input:
 * Output:
 */
static ssize_t felica_test_read(struct file *fp, char *buf, size_t count, loff_t *pos)
{
  return 1;
}

/*
 * Description:
 * Input:
 * Output:
 */
static ssize_t felica_test_write(struct file *fp, const char *buf, size_t count, loff_t *pos)
{
  unsigned char case_buf[3] = {0,};
  int rc = -1;

  /* Check error */
  if(NULL == buf)
  {
    FELICA_DEBUG_MSG("[FELICA_TEST] ERROR - start \n");
    return -1;
  }

  /* copy from user data */
  rc = copy_from_user(case_buf, buf, count);
  if(rc)
  {
    FELICA_DEBUG_MSG("[FELICA_TEST] ERROR - copy_from_user \n");
    return -1;
  }

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_TEST] copy_from_user[%d][%d][%d] \n",case_buf[0],case_buf[1],case_buf[2]);
  #endif

  switch(case_buf[0]){
    case FFI:
      switch(case_buf[1]){
        case FN_DEVICE_FT_DISABLE_OPEN_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_open_uart(ON);
          else
            disable_open_uart(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_CLOSE_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_close_uart(ON);
          else
            disable_close_uart(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_AVAILABLE_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_available_uart(ON);
          else
            disable_available_uart(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_READ_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_read_uart(ON);
          else
            disable_read_uart(OFF);
          break;
        }
    break;

    case FFO:
      switch(case_buf[1]){
        case FN_DEVICE_FT_DISABLE_OPEN_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_open_uart(ON);
          else
            disable_open_uart(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_CLOSE_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_close_uart(ON);
          else
            disable_close_uart(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_READ_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_read_uart(ON);
          else
            disable_read_uart(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_WRITE_UART:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_write_uart(ON);
          else
            disable_write_uart(OFF);
          break;
        }
      break;

    case PFO:
      switch(case_buf[1]){
        case FN_DEVICE_FT_DISABLE_OPEN_PON:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_open_pon(ON);
          else
            disable_open_pon(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_CLOSE_PON:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_close_pon(ON);
          else
            disable_close_pon(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_WRITE_PON:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_write_pon(ON);
          else
            disable_write_pon(OFF);
          break;
        }
      break;

    case CFI:
      switch(case_buf[1]){
        case FN_DEVICE_FT_DISABLE_OPEN_CEN:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_open_cen(ON);
          else
            disable_open_cen(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_CLOSE_CEN:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_close_cen(ON);
          else
            disable_close_cen(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_READ_CEN:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_read_cen(ON);
          else
            disable_read_cen(OFF);
          break;
        }
      break;

    case RFI:
      switch(case_buf[1]){
        case FN_DEVICE_FT_DISABLE_OPEN_RFS:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_open_rfs(ON);
          else
            disable_open_rfs(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_CLOSE_RFS:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_close_rfs(ON);
          else
            disable_close_rfs(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_READ_RFS:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_read_rfs(ON);
          else
            disable_read_rfs(OFF);
          break;
        }
      break;

    case RWFI:
      switch(case_buf[1]){
        case FN_DEVICE_FT_DISABLE_OPEN_RWS:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_open_rws(ON);
          else
            disable_open_rws(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_CLOSE_RWS:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_close_rws(ON);
          else
            disable_close_rws(OFF);
          break;

        case FN_DEVICE_FT_DISABLE_READ_RWS:
          if(case_buf[2] == FN_DEVICE_TEST_ON)
            disable_read_rws(ON);
          else
            disable_read_rws(OFF);
          break;
        }
      break;
    }
  return 1;
}

//2 FFI&FFO
/*
 * Description:
 * Input:
 * Output:
 */
void disable_open_uart(bool val)
{
  if(val)
    result_open_uart = -1;
  else
    result_open_uart = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] felica_uart_open_failed - val(%d), result_open_uart(%d)  \n",val,result_open_uart);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_close_uart(bool val)
{
  if(val)
    result_close_uart = -1;
  else
    result_close_uart = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_uart_close - val(%d), result_close_uart(%d)  \n",val,result_close_uart);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_available_uart(bool val)
{
  if(val)
    result_available_uart = -1;
  else
    result_available_uart = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_uart_available - val(%d), result_availbale_uart(%d)  \n",val,result_available_uart);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_read_uart(bool val)
{
  if(val)
    result_read_uart = -1;
  else
    result_read_uart = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_uart_read - val(%d), result_read_uart(%d)  \n",val,result_read_uart);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_write_uart(bool val)
{
  if(val)
    result_write_uart = -1;
  else
    result_write_uart = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_uart_write - val(%d), result_write_uart(%d)  \n",val,result_write_uart);
  #endif
}


//2 PFO
/*
 * Description:
 * Input:
 * Output:
 */
void disable_open_pon(bool val)
{
  if(val)
    result_open_pon = -1;
  else
    result_open_pon = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_pon_open - val(%d), result_open_pon(%d)  \n",val,result_open_pon);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_close_pon(bool val)
{
  if(val)
    result_close_pon = -1;
  else
    result_close_pon = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_pon_close - val(%d), result_close_pon(%d)  \n",val,result_close_pon);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_write_pon(bool val)
{
  if(val)
    result_write_pon = -1;
  else
    result_write_pon = 1;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_pon_write - val(%d), result_write_pon(%d)  \n",val,result_write_pon);
  #endif
}

//2 CFI
/*
 * Description:
 * Input:
 * Output:
 */
void disable_open_cen(bool val)
{
  if(val)
    result_open_cen = -1;
  else
    result_open_cen = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA_CEN] disable_felica_cen_open - val(%d), result_open_cen(%d)  \n",val,result_open_cen);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_close_cen(bool val)
{
  if(val)
    result_close_cen = -1;
  else
    result_close_cen = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_cen_close - val(%d), result_close_cen(%d)  \n",val,result_close_cen);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_read_cen(bool val)
{
  if(val)
    result_read_cen = -1;
  else
    result_read_cen = 1;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_cen_read - val(%d), result_read_cen(%d)  \n",val,result_read_cen);
  #endif
}

//2 RFS
/*
 * Description:
 * Input:
 * Output:
 */
void disable_open_rfs(bool val)
{
  if(val)
    result_open_rfs = -1;
  else
    result_open_rfs = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_rfs_open - val(%d), result_open_rfs(%d)  \n",val,result_open_rfs);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_close_rfs(bool val)
{
  if(val)
    result_close_rfs = -1;
  else
    result_close_rfs = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_rfs_close - val(%d), result_close_rfs(%d)  \n",val,result_close_rfs);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_read_rfs(bool val)
{
  if(val)
    result_read_rfs = -1;
  else
    result_read_rfs = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_rfs_read - val(%d), result_read_rfs(%d)  \n",val,result_read_rfs);
  #endif
}

//2 RWFI
/*
 * Description:
 * Input:
 * Output:
 */
void disable_open_rws(bool val)
{
  if(val)
    result_open_rws = -1;
  else
    result_open_rws = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_rws_open - val(%d), result_open_rws(%d)  \n",val,result_open_rws);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_close_rws(bool val)
{
  if(val)
    result_close_rws = -1;
  else
    result_close_rws = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_rws_close - val(%d), result_close_rws(%d)  \n",val,result_close_rws);
  #endif
}

/*
 * Description:
 * Input:
 * Output:
 */
void disable_read_rws(bool val)
{
  if(val)
    result_read_rws = -1;
  else
    result_read_rws = 0;

  #ifdef FEATURE_DEBUG_LOW
  FELICA_DEBUG_MSG("[FELICA] disable_felica_rws_read - val(%d), result_read_rws(%d)  \n",val,result_read_rws);
  #endif
}


/*
 *    STRUCT DEFINITION
 */


static struct file_operations felica_test_fops =
{
  .owner    = THIS_MODULE,
  .open    = felica_test_open,
  .read    = felica_test_read,
  .write    = felica_test_write,
  .release  = felica_test_release,
};

static struct miscdevice felica_test_device =
{
  .minor = MISC_DYNAMIC_MINOR,
  .name = "felica_test",
  .fops = &felica_test_fops
};

/*
 * Description:
 * Input:
 * Output:
 */
static int felica_test_init(void)
{
  int rc;

  /* register the device file */
  rc = misc_register(&felica_test_device);
  if (rc < 0)
  {
    FELICA_DEBUG_MSG("[FELICA_TEST] FAIL!! can not register felica_cal \n");
    return rc;
  }
  return 0;
}

/*
 * Description:
 * Input:
 * Output:
 */
static void felica_test_exit(void)
{
  /* deregister the device file */
  misc_deregister(&felica_test_device);
}

module_init(felica_test_init);
module_exit(felica_test_exit);

MODULE_LICENSE("Dual BSD/GPL");

