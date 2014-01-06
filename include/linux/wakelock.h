/* include/linux/wakelock.h
 *
 * Copyright (C) 2007-2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_WAKELOCK_H
#define _LINUX_WAKELOCK_H

#include <linux/ktime.h>
#include <linux/device.h>

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_MACH_LGE)
#include <linux/module.h>
#include <linux/kallsyms.h>
#endif

/* A wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the wake_lock
 * prevents a full system suspend.
 */

enum {
	WAKE_LOCK_SUSPEND, /* Prevent suspend */
	WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
	struct wakeup_source ws;
};

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_MACH_LGE)
int wake_lock_active_name(char *name);
#endif

static inline void wake_lock_init(struct wake_lock *lock, int type,
				  const char *name)
{
	wakeup_source_init(&lock->ws, name);
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
	wakeup_source_trash(&lock->ws);
}

static inline void wake_lock(struct wake_lock *lock)
{
	__pm_stay_awake(&lock->ws);
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
	__pm_wakeup_event(&lock->ws, jiffies_to_msecs(timeout));
}

static inline void wake_unlock(struct wake_lock *lock)
{
	__pm_relax(&lock->ws);
}

static inline int wake_lock_active(struct wake_lock *lock)
{
	return lock->ws.active;
}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_MACH_LGE)
enum lateresume_wq_stat_step {
	LATERESUME_START = 1,
	LATERESUME_MUTEXLOCK,
	LATERESUME_CHAINSTART,
	LATERESUME_CHAINDONE,
	LATERESUME_END = 0
};

enum earlysuspend_wq_stat_step {
	EARLYSUSPEND_START = 1,
	EARLYSUSPEND_MUTEXLOCK,
	EARLYSUSPEND_CHAINSTART,
	EARLYSUSPEND_CHAINDONE,
	EARLYSUSPEND_MUTEXUNLOCK,
	EARLYSUSPEND_SYNCDONE,
	EARLYSUSPEND_END = 0
};

enum suspend_wq_stat_step {
	SUSPEND_START = 1,
	SUSPEND_ENTERSUSPEND,
	SUSPEND_EXITSUSPEND,
	SUSPEND_EXITDONE = 0
};

enum suspend_wq_num {
	LATERESUME_WQ = 1,
	EARLYSUSPEND_WQ,
	SUSPEND_WQ
};

struct suspend_wq_stats {
	int lateresume_stat;
	int earlysuspend_stat;
	int suspend_stat;
	int failed_wq;
	char last_lateresume_call[KSYM_SYMBOL_LEN];
	char last_earlysuspend_call[KSYM_SYMBOL_LEN];
};

extern struct suspend_wq_stats suspend_wq_stats;
static inline void save_lateresume_step(int step)
{
	suspend_wq_stats.lateresume_stat = step;
}

static inline void save_earlysuspend_step(int step)
{
	suspend_wq_stats.earlysuspend_stat = step;
}

static inline void save_suspend_step(int step)
{
	suspend_wq_stats.suspend_stat = step;
}

static inline void save_lateresume_call(char *name)
{
	char *call_name = "end_of_lateresume";

	if (name)
		call_name = name;

	strlcpy(suspend_wq_stats.last_lateresume_call,
			call_name,
			sizeof(suspend_wq_stats.last_lateresume_call));
}

static inline void save_earlysuspend_call(char *name)
{
	char *call_name = "end_of_lateresume";
	if (name)
		call_name = name;

	strlcpy(suspend_wq_stats.last_earlysuspend_call,
			call_name,
			sizeof(suspend_wq_stats.last_earlysuspend_call));
}
#endif

#endif
