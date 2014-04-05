/*
* snfc_gpio.c
*
*/

/*
 *    INCLUDE FILES FOR MODULE
 */

#include "snfc_gpio.h"

#include <linux/gpio.h>
#include <mach/board_lge.h>

int gpio_rfs;


/*
* Description :
* Input :
* Output :
*/
int snfc_gpio_open(int gpionum, int direction, int value)
{
        int rc = 0;
        //char int_name[30];
        unsigned gpio_cfg = 0;

        if(direction == GPIO_DIRECTION_IN)
        {
                gpio_cfg =  SNFC_GPIO_CFG(gpionum, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
                rc = gpio_tlmm_config(gpio_cfg, GPIO_CONFIG_ENABLE);

                if(rc)
                {
                        SNFC_DEBUG_MSG("[snfc] ERROR - gpio_tlmm_config \n");
                        return rc;
                }

                rc = gpio_direction_input((unsigned)gpionum);

                if(rc)
                {
                        SNFC_DEBUG_MSG("[snfc] ERROR -  gpio_direction_input \n");
                        return rc;
                }
                SNFC_DEBUG_MSG("[snfc] set gpio %d input \n",gpionum );
        }
        else
        {
                gpio_cfg =  SNFC_GPIO_CFG(gpionum, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
                rc = gpio_tlmm_config(gpio_cfg, GPIO_CONFIG_ENABLE);

                if(rc)
                {
                        SNFC_DEBUG_MSG("[snfc] ERROR - gpio_tlmm_config \n");
                        return rc;
                }

                rc = gpio_direction_output((unsigned)gpionum, value);

                if(rc)
                {
                        SNFC_DEBUG_MSG("[snfc] ERROR -  gpio_direction_output \n");
                        return rc;
                }
                SNFC_DEBUG_MSG("[snfc] set gpio %d output, %d \n",gpionum,value );
        }
        return rc;
}

/*
* Description :
* Input :
* Output :
*/
void snfc_gpio_write(int gpionum, int value)
{
        gpio_set_value(gpionum, value);
}

/*
* Description :
* Input :
* Output :
*/
int snfc_gpio_read(int gpionum)
{
        return gpio_get_value(gpionum);
}

int snfc_get_rfs_gpio_num(void)
{
/* If it has different GPIO number each HW version, please use this route. */
  if (lge_get_board_revno() <= HW_REV_C)
  {
    return GPIO_SNFC_RFS;
  }
  else
  {
    return GPIO_SNFC_RFS_REV_D;
  }
}

int snfc_get_int_gpio_num(void)
{
/* If it has different GPIO number each HW version, please use this route. */
  if (lge_get_board_revno() <= HW_REV_C)
  {
    return GPIO_SNFC_INT;
  }
  else
  {
    return GPIO_SNFC_INT_REV_D;
  }
}

int snfc_get_intu_gpio_num(void)
{
/* If it has different GPIO number each HW version, please use this route. */
  if (lge_get_board_revno() <= HW_REV_C)
  {
    return GPIO_SNFC_INTU;
  }
  else
  {
    return GPIO_SNFC_INTU_REV_D;
  }
}

int snfc_get_hsel_gpio_num(void)
{
/* If it has different GPIO number each HW version, please use this route. */
  if (lge_get_board_revno() <= HW_REV_C)
  {
    return GPIO_SNFC_HSEL;
  }
  else
  {
    return GPIO_SNFC_HSEL_REV_D;
  }
}

int snfc_get_hvdd_gpio_num(void)
{
/* If it has different GPIO number each HW version, please use this route. */
  if (lge_get_board_revno() <= HW_REV_C)
  {
    return GPIO_SNFC_HVDD;
  }
  else
  {
    return GPIO_SNFC_HVDD_REV_D;
  }
}
