/*
 * Copyright (C) 2013 LG Electronics
 * G2 Task BSP powerteam.  <G2-Task-Power@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __QPNP_TEMP_ALARM_H_
#define __QPNP_TEMP_ALARM_H_

#ifdef CONFIG_LGE_PM
extern int qpnp_batif_regist_batt_present(void (*callback)(int));
extern void qpnp_batif_unregist_batt_present(void (*callback)(int));
#endif

#endif

