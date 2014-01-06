/*
 *  felicai2c.h
 *
 */

#ifndef __FELICA_I2C_H__
#define __FELICA_I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  INCLUDE FILES FOR MODULE
 */

#include <linux/list.h>

#include "felica_common.h"

/*
 *  DEFINE
 */

/*
 *  FUNCTION PROTOTYPE
 */
int felica_i2c_open (void);
int felica_i2c_release (void);
int felica_i2c_read(unsigned char reg, unsigned char *buf, size_t count);
int felica_i2c_write(unsigned char reg, unsigned char *buf, size_t count);
int felica_i2c_set_slave_address (unsigned char slave_address);


#ifdef __cplusplus
}
#endif

#endif // __FELICA_I2C_H__