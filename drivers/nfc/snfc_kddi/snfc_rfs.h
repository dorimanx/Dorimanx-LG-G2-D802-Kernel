/*
*	sfnc_rfs.h
*/

#ifndef __SNFC_RFS_H__
#define __SNFC_RFS_H__

/*
*	Include header files
*/
#include "snfc_common.h"
#include "snfc_gpio.h"

int snfc_rfs_probe(struct device_node *np);
void snfc_rfs_remove(void);

#endif	//__SNFC_RFS_H__
