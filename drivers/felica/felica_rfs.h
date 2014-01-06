/*
 *  felica_rfs.h
 *
 */

#ifndef __FELICA_RFS_H__
#define __FELICA_RFS_H__

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
#ifdef FELICA_LED_SUPPORT
irqreturn_t felica_rfs_detect_interrupt(int irq, void *dev_id);
#endif

#ifdef __cplusplus
}
#endif

#endif // __FELICA_RFS_H__
