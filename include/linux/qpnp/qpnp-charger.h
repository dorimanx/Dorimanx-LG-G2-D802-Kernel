/*
 * Copyright (C) 2013 LG Electronics
 * G2 Task BSP powerteam.  <G2-Task-Power@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __QPNP_CHARGER_H_
#define __QPNP_CHARGER_H_

#ifdef CONFIG_LGE_PM
extern int32_t qpnp_charger_is_ready(void);
#if defined(CONFIG_MACH_MSM8974_VU3_KR) || defined(CONFIG_MACH_MSM8974_G2_KDDI)
extern int32_t external_qpnp_enable_charging(bool enable);
#endif
#endif

#endif
