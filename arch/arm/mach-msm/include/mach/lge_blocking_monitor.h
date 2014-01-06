/* 
 * arch/arm/mach-msm/include/mach/lge/lge_blocking_monitor.h
 *
 * Copyright (C) 2012 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LGE_BLOCKING_MONITOR_H__
#define __LGE_BLOCKING_MONITOR_H__

int start_monitor_blocking(int id, unsigned long expires);
int end_monitor_blocking(int id);
int create_blocking_monitor(const char *name);
int remove_blocking_monitor(int id);

#endif
