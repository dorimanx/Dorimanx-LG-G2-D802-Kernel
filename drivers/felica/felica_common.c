/*
 *  felica_common.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include <linux/wakelock.h>

#include "felica_common.h"

/*
 *  DEFINE
 */

/*
 *    INTERNAL DEFINITION
 */

static struct wake_lock felica_wake_lock;

#ifdef FELICA_LED_SUPPORT
static struct wake_lock felica_rfs_wake_lock;
#endif

/*
 *    FUNCTION DEFINITION
 */

/*
* Description :
* Input : None
* Output :
*/
void lock_felica_wake_lock(void)
{
  wake_lock(&felica_wake_lock);
}
/*
* Description :
* Input : None
* Output :
*/
void unlock_felica_wake_lock(void)
{
  wake_unlock(&felica_wake_lock);
}
/*
* Description :
* Input : None
* Output :
*/
void init_felica_wake_lock(void)
{
  wake_lock_init(&felica_wake_lock, WAKE_LOCK_SUSPEND, "felica");
}
/*
* Description :
* Input : None
* Output :
*/
void destroy_felica_wake_lock(void)
{
  wake_lock_destroy(&felica_wake_lock);
}

/*
* Description :
* Input : None
* Output : true - available
*/
int get_felica_uart_status(void)
{
#if defined(CONFIG_LGE_FELICA_NFC)&& defined(FELICA_NFC_INTERFACE)
  int waitcount = 3;
  int retrycount = 10;
  int rval = FELICA_UART_NOTAVAILABLE;

  _e_snfc_uart_status uart_status = __snfc_uart_control_get_uart_status();
  
  switch(uart_status)
  {
    case UART_STATUS_READY:
    case UART_STATUS_FOR_FELICA:
      rval = FELICA_UART_AVAILABLE;
      break;

    default:
      rval = FELICA_UART_NOTAVAILABLE;
      break;
  }

  if(FELICA_UART_NOTAVAILABLE == rval)
  {
    #ifdef FEATURE_DEBUG_MED
    FELICA_DEBUG_MSG("[FELICA_COMMON] uart can be used - wait 30msec \n");
	#endif
    while(0 < waitcount)
    {
      waitcount--;
      mdelay(10);

      if(UART_STATUS_READY == __snfc_uart_control_get_uart_status())
      {
        #ifdef FEATURE_DEBUG_MED
        FELICA_DEBUG_MSG("[FELICA_COMMON] uart state was changed : avalable \n");
		#endif
        rval = FELICA_UART_AVAILABLE;
        break;
      }
    }
  }

  if(FELICA_UART_AVAILABLE != rval)
  {
    #ifdef FEATURE_DEBUG_MED
    FELICA_DEBUG_MSG("[FELICA_COMMON] uart can be used - wait 1sec \n");
	#endif
    while(0 < retrycount)
    {
      retrycount--;
      mdelay(100);
      waitcount = 3;

      #ifdef FEATURE_DEBUG_MED
      FELICA_DEBUG_MSG("[FELICA_COMMON] uart can be used - wait 30msec during wait 1sec \n");
	  #endif
      while(0 < waitcount)
      {
        waitcount--;
        mdelay(10);
        if(UART_STATUS_READY == __snfc_uart_control_get_uart_status())
        {
          #ifdef FEATURE_DEBUG_MED
          FELICA_DEBUG_MSG("[FELICA_COMMON] uart state was changed : avalable \n");
		  #endif
          rval = FELICA_UART_AVAILABLE;
          break;
        }
      }

      if(FELICA_UART_AVAILABLE == rval)
      {
        break;
      }
    }
  }

  return rval;
#else
  return FELICA_UART_AVAILABLE;
#endif
}

/*
* Description :
* Input : None
* Output :
*/
void set_felica_uart_status(_e_snfc_uart_status uart_status)
{
#if defined(CONFIG_LGE_FELICA_NFC) && defined(FELICA_NFC_INTERFACE)
  #ifdef FEATURE_DEBUG_MED
  FELICA_DEBUG_MSG("[FELICA_COMMON] set_felica_uart_status : %d \n", uart_status);
  #endif
  __snfc_uart_control_set_uart_status(uart_status);
#endif
}


/*
* Description :
* Input : None
* Output : true - available
*/
_e_snfc_i2c_status get_felica_i2c_status(void)
{
#if defined(CONFIG_LGE_FELICA_NFC)&& defined(FELICA_NFC_INTERFACE)
  return __snfc_i2c_control_get_status();
#else
  return I2C_STATUS_NO_USE;
#endif
}

/*
* Description :
* Input : None
* Output :
*/
void set_felica_i2c_status(_e_snfc_i2c_status i2c_status)
{
#if defined(CONFIG_LGE_FELICA_NFC)&& defined(FELICA_NFC_INTERFACE)
  #ifdef FEATURE_DEBUG_MED
  FELICA_DEBUG_MSG("[FELICA_COMMON] set_felica_i2c_status : %d \n", i2c_status);
  #endif
  __snfc_i2c_control_set_status(i2c_status);
#endif
}

#ifdef FELICA_LED_SUPPORT
/*
* Description :
* Input : None
* Output :
*/
void lock_felica_rfs_wake_lock(void)
{
  wake_lock(&felica_rfs_wake_lock);
}
/*
* Description :
* Input : None
* Output :
*/
void unlock_felica_rfs_wake_lock(void)
{
  wake_unlock(&felica_rfs_wake_lock);
}
/*
* Description :
* Input : None
* Output :
*/
void init_felica_rfs_wake_lock(void)
{
  wake_lock_init(&felica_rfs_wake_lock, WAKE_LOCK_SUSPEND, "felica_rfs");
}
/*
* Description :
* Input : None
* Output :
*/
void destroy_felica_rfs_wake_lock(void)
{
  wake_lock_destroy(&felica_rfs_wake_lock);
}
#endif
