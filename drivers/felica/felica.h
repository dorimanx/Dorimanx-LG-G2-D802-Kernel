/*
 *  felica.h
 *
 */
#ifndef __FELICA_H__
#define __FELICA_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  INCLUDE FILES FOR MODULE
 *
 */

#include <linux/list.h>

#include "felica_common.h"

/*
 *  DEFINE
 *
 */

#define IOCTL_FELICA_MAGIC 'T'
#define IOCTL_FELICA_CMD_AVAILABLE 0x1B

enum
{
  IOCTL_MAGIC_0 = 0,
  IOCTL_MAGIC_1,
  IOCTL_MAGIC_MAX,
};

#define IOCTL_FELICA_AVAILABLE  _IO(IOCTL_FELICA_MAGIC, IOCTL_FELICA_CMD_AVAILABLE)

#ifdef __cplusplus
}
#endif

#endif // __FELICA_H__
