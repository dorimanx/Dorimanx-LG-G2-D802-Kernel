/*
 *  felicagpio.h
 *
 */

#ifndef __FELICA_GPIO_H__
#define __FELICA_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  INCLUDE FILES FOR MODULE
 */

#include <linux/list.h>
#include <linux/gpio.h>

#include "felica_common.h"
/*
 *  DEFINE
 */

/* common */
enum{
  GPIO_DIRECTION_IN = 0,
  GPIO_DIRECTION_OUT,
};

enum{
  GPIO_LOW_VALUE = 0,
  GPIO_HIGH_VALUE,
};

enum{
  GPIO_CONFIG_ENABLE = 0,
  GPIO_CONFIG_DISABLE,
};

#if defined(CONFIG_LGE_FELICA_KDDI)
/* felica_pon */
#define GPIO_FELICA_PON   74

/* felica_rfs */
#define GPIO_FELICA_RFS   102

/* felica_int */
#define GPIO_FELICA_INT   92

#define GPIO_FELICA_LOCKCONT   89

#define GPIO_NFC_HSEL   59


#elif defined (CONFIG_LGE_FELICA_KDDI_Z)

/* felica_pon */
#define GPIO_FELICA_PON   40

/* felica_rfs */
#define GPIO_FELICA_RFS   59

/* felica_int */
#define GPIO_FELICA_INT   37

#define GPIO_FELICA_LOCKCONT   38

#define GPIO_NFC_HSEL   94

#else
/* felica_pon */
#define GPIO_FELICA_PON   40

/* felica_rfs */
#define GPIO_FELICA_RFS_REV_D  59
#define GPIO_FELICA_RFS   94

/* felica_int */
#define GPIO_FELICA_INT_REV_D   37
#define GPIO_FELICA_INT   38

#define GPIO_FELICA_LOCKCONT_REV_D   38
#define GPIO_FELICA_LOCKCONT   37

#define GPIO_NFC_HSEL_REV_D   94
#define GPIO_NFC_HSEL   59
#endif
/*
 *  FUNCTION PROTOTYPE
 */
int felica_gpio_open(int gpionum, int direction, int value);
void felica_gpio_write(int gpionum, int value);
int felica_gpio_read(int gpionum);
int felica_get_rfs_gpio_num(void);
int felica_get_int_gpio_num(void);

#ifdef __cplusplus
}
#endif

#endif // __FELICA_RFS_H__
