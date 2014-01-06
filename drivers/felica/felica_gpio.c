/*
 *  felicagpio.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include <linux/gpio.h>

#include "felica_gpio.h"



/* Debug message feature */
/* #define FELICA_DEBUG_MSG */

/*
* Description :
* Input :
* Output :
*/
int felica_gpio_open(int gpionum, int direction, int value)
{
  int rc = 0;
  char int_name[30];

  if(direction == GPIO_DIRECTION_IN)
  {
    rc = gpio_tlmm_config(GPIO_CFG(gpionum, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CONFIG_ENABLE);

  if(rc)
  {
    #ifdef FEATURE_DEBUG_HIGH
    FELICA_DEBUG_MSG("[FELICA_GPIO] ERROR - gpio_tlmm_config \n");
	#endif
    return rc;
  }

    if(GPIO_FELICA_INT == gpionum)
  {
      sprintf(int_name, "felica_int_%02d", gpionum);

      gpio_request(gpionum, int_name);
    }
#ifdef FELICA_LED_SUPPORT
	if(GPIO_FELICA_RFS == gpionum)
	{
		sprintf(int_name, "felica_rfs_%02d", gpionum);
		gpio_request(gpionum, int_name);
	}
#endif
    rc = gpio_direction_input((unsigned)gpionum);

    if(rc)
    {
      #ifdef FEATURE_DEBUG_HIGH
      FELICA_DEBUG_MSG("[FELICA_GPIO] ERROR -  gpio_direction_input \n");
	  #endif
      return rc;
    }
  }
  else
  {
    rc = gpio_tlmm_config(GPIO_CFG(gpionum, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CONFIG_ENABLE);

    if(rc)
    {
      #ifdef FEATURE_DEBUG_HIGH
      FELICA_DEBUG_MSG("[FELICA_GPIO] ERROR - gpio_tlmm_config \n");
	  #endif
      return rc;
    }
	if(GPIO_FELICA_PON == gpionum)
	{
		sprintf(int_name, "felica_pon_%02d", gpionum);
		gpio_request(gpionum, int_name);
	}

    rc = gpio_direction_output((unsigned)gpionum, value);

    if(rc)
    {
      #ifdef FEATURE_DEBUG_HIGH
      FELICA_DEBUG_MSG("[FELICA_GPIO] ERROR -  gpio_direction_output \n");
	  #endif
      return rc;
    }
  }

  return rc;
}

/*
* Description :
* Input :
* Output :
*/
void felica_gpio_write(int gpionum, int value)
{
  gpio_set_value(gpionum, value);
}

/*
* Description :
* Input :
* Output :
*/
int felica_gpio_read(int gpionum)
{
  return gpio_get_value(gpionum);
}
/*
* Description :
* Input :
* Output :
*/
int felica_get_rfs_gpio_num(void)
{
/* If it has different GPIO number each HW version, please use this route. */
#ifdef CONFIG_LGE_FELICA_NFC_DCM
  int gpionum = GPIO_FELICA_RFS;

  if (lge_get_board_revno() < HW_REV_D)
  {
    gpionum = GPIO_FELICA_RFS;
  }
  else
  {
    gpionum = GPIO_FELICA_RFS_REV_D;  
  }

	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_GPIO] felica_get_rfs_gpio_num(%d) \n",gpionum);
	#endif
	return gpionum;	
#else
	return GPIO_FELICA_RFS;
#endif
}

/*
* Description :
* Input :
* Output :
*/
int felica_get_int_gpio_num(void)
{
/* If it has different GPIO number each HW version, please use this route. */	
#ifdef CONFIG_LGE_FELICA_NFC_DCM
  int gpionum = GPIO_FELICA_INT;

  if (lge_get_board_revno() < HW_REV_D)
  {
    gpionum = GPIO_FELICA_INT;
  }
  else
  {
    gpionum = GPIO_FELICA_INT_REV_D;  
  }
	#ifdef FEATURE_DEBUG_LOW
	FELICA_DEBUG_MSG("[FELICA_GPIO] felica_get_rws_gpio_num(%d) \n",gpionum);
	#endif
	return gpionum;
#else	
	return GPIO_FELICA_INT;
#endif
}
