/*
 *  snfc_intu_poll.h
 *  
 */
#ifndef __SNFC_INTU_POLL_H__
#define __SNFC_INTU_POLL_H__

/*
 *  Include header files
 *  
 */
#include "snfc_common.h"
#include "snfc_gpio.h"

int snfc_intu_probe(struct device_node *np);
void snfc_intu_remove(void);

#endif
