/*
 * Copyright (C) 2013 LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_MOCK_POWER_H
#define _LINUX_MOCK_POWER_H

#include <linux/power_supply.h>

#ifdef CONFIG_MOCK_POWER
int mock_power_supply_register(struct power_supply *psy);
void mock_power_supply_unregister(struct power_supply *psy);
#else
static inline int mock_power_supply_register(struct power_supply *psy)
{
	return 0;
}
static inline void mock_power_supply_unregister(struct power_supply *psy)
{
	return;
}
#endif

#endif /* _LINUX_MOCK_POWER_H */
