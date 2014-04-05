/*
 * Copyright (C) 2013 LG Electronics Inc.
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

#ifndef _ZWAIT_H
#define _ZWAIT_H

#include <linux/zwait.h>
#include <linux/debugfs.h>

/* drivers/base/power */
#include <power.h>

extern void dpm_wakeup_dev_list_set(void);
extern void dpm_wakeup_dev_list_clean(void);
extern void zw_pwrkey_set(int state);
extern void zw_pwrkey_clean(void);
extern void zw_psy_update(void);
extern bool is_suspend_force(void);
extern void zw_power_pm_set(void);
extern void zw_power_pm_clean(void);
extern void zw_led_turn_off(void);
extern void zw_rtc_set(void);
extern void zw_rtc_clean(void);
extern unsigned long get_zw_timeout_delay(void);
extern void set_zw_timeout_delay(unsigned long sec);
extern int get_zw_rtc_retry(void);
extern void set_zw_rtc_retry(int max);
extern int get_zw_rtc_retry_delay(void);
extern void set_zw_rtc_retry_delay(int msec);

struct zw_debug_ops {
	int (*show)(char *, size_t);
	int (*store)(const char *, size_t);
};

#define ZW_SET_FUNCTION(_show, _store) { \
	.show = _show, \
	.store = _store \
}

#define ZW_DEBUG_ATTR(_name, _show, _store) \
	struct zw_debug_ops debug_ops_##_name = ZW_SET_FUNCTION(_show, _store)

#endif /* _ZWAIT_H */
