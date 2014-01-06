/* kernel/power/earlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
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

#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#ifdef CONFIG_MACH_LGE
#include <linux/debugfs.h>
#include <mach/lge_blocking_monitor.h>
#endif

#include "power.h"

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
};
static int debug_mask = DEBUG_USER_STATE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static DEFINE_MUTEX(early_suspend_lock);
static LIST_HEAD(early_suspend_handlers);
static void early_suspend(struct work_struct *work);
static void late_resume(struct work_struct *work);
static DECLARE_WORK(early_suspend_work, early_suspend);
static DECLARE_WORK(late_resume_work, late_resume);
static DEFINE_SPINLOCK(state_lock);
enum {
	SUSPEND_REQUESTED = 0x1,
	SUSPENDED = 0x2,
	SUSPEND_REQUESTED_AND_SUSPENDED = SUSPEND_REQUESTED | SUSPENDED,
};
static int state;

#ifdef CONFIG_MACH_LGE
enum log_resume_step {
	RESUME_KICK = 0,
	RESUME_ENTRY,
	RESUME_EXIT
};

static inline void late_resume_call_chain(struct early_suspend *pos);
static inline void early_suspend_call_chain(struct early_suspend *pos);
static inline void log_resume(enum log_resume_step step);
struct timespec ts_resume_kick;

struct resume_delay {
	int avg;
	int max;
	int count;
};

struct resume_delay resume_delay = {
	.avg = 0,
	.max = 0,
	.count = 0,
};

struct resume_delay resume_total = {
	.avg = 0,
	.max = 0,
	.count = 0,
};

#define WATCHDOG_EXPIRES_TIME		(5 * HZ)
static struct timer_list es_watchdog_timer;

static int early_suspend_monitor_id;
static int late_resume_monitor_id;

#endif

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;
		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
#ifdef CONFIG_MACH_LGE
	handler->resume_avg = 0;
	handler->resume_max = 0;
	handler->resume_count = 0;
	handler->suspend_avg = 0;
	handler->suspend_max = 0;
	handler->suspend_count = 0;
#endif
	if ((state & SUSPENDED) && handler->suspend)
		handler->suspend(handler);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static void early_suspend(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

#ifdef CONFIG_MACH_LGE
	start_monitor_blocking(early_suspend_monitor_id,
		jiffies + usecs_to_jiffies(5000000));

	save_earlysuspend_step(EARLYSUSPEND_START);
#endif
	mutex_lock(&early_suspend_lock);
#ifdef CONFIG_MACH_LGE
	save_earlysuspend_step(EARLYSUSPEND_MUTEXLOCK);
#endif

	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED)
		state |= SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("early_suspend: abort, state %d\n", state);
		mutex_unlock(&early_suspend_lock);
		goto abort;
	}

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: call handlers\n");
#ifdef CONFIG_MACH_LGE
	save_earlysuspend_step(EARLYSUSPEND_CHAINSTART);

	es_watchdog_timer.expires = jiffies + WATCHDOG_EXPIRES_TIME;
	add_timer(&es_watchdog_timer);
#endif
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {
#ifdef CONFIG_MACH_LGE
			char sym[KSYM_SYMBOL_LEN];

			sprint_symbol(sym, (unsigned long)pos->suspend);
			save_earlysuspend_call(sym);
			printk(KERN_INFO"%s: %s\n", __func__, sym);
			early_suspend_call_chain(pos);
#else
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("early_suspend: calling %pf\n", pos->suspend);
			pos->suspend(pos);
#endif
		}
	}
#ifdef CONFIG_MACH_LGE
	del_timer(&es_watchdog_timer);

	save_earlysuspend_call(NULL);

	save_earlysuspend_step(EARLYSUSPEND_CHAINDONE);
#endif
	mutex_unlock(&early_suspend_lock);
#ifdef CONFIG_MACH_LGE
	save_earlysuspend_step(EARLYSUSPEND_MUTEXUNLOCK);
#endif

	suspend_sys_sync_queue();
#ifdef CONFIG_MACH_LGE
	save_earlysuspend_step(EARLYSUSPEND_SYNCDONE);
#endif
abort:
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED_AND_SUSPENDED)
		wake_unlock(&main_wake_lock);
	spin_unlock_irqrestore(&state_lock, irqflags);

#ifdef CONFIG_MACH_LGE
	end_monitor_blocking(early_suspend_monitor_id);
#endif
}

static void late_resume(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

#ifdef CONFIG_MACH_LGE
	start_monitor_blocking(late_resume_monitor_id,
		jiffies + usecs_to_jiffies(5000000));

	log_resume(RESUME_ENTRY);
	save_lateresume_step(LATERESUME_START);
#endif
	mutex_lock(&early_suspend_lock);
#ifdef CONFIG_MACH_LGE
	save_lateresume_step(LATERESUME_MUTEXLOCK);
#endif
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPENDED)
		state &= ~SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("late_resume: abort, state %d\n", state);
		goto abort;
	}
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: call handlers\n");
#ifdef CONFIG_MACH_LGE
	save_lateresume_step(LATERESUME_CHAINSTART);

	es_watchdog_timer.expires = jiffies + WATCHDOG_EXPIRES_TIME;
	add_timer(&es_watchdog_timer);
#endif
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link) {
		if (pos->resume != NULL) {
#ifdef CONFIG_MACH_LGE
			char sym[KSYM_SYMBOL_LEN];

			sprint_symbol(sym, (unsigned long)pos->resume);
			save_lateresume_call(sym);
			printk(KERN_INFO"%s: %s\n", __func__, sym);
			late_resume_call_chain(pos);
#else
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("late_resume: calling %pf\n", pos->resume);
			pos->resume(pos);
#endif
		}
	}
#ifdef CONFIG_MACH_LGE
	del_timer(&es_watchdog_timer);

	log_resume(RESUME_EXIT);
	save_lateresume_call(NULL);
#endif
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: done\n");
#ifdef CONFIG_MACH_LGE
	save_lateresume_step(LATERESUME_CHAINDONE);
#endif
abort:
	mutex_unlock(&early_suspend_lock);
#ifdef CONFIG_MACH_LGE
	save_lateresume_step(LATERESUME_END);
	end_monitor_blocking(late_resume_monitor_id);
#endif
}

void request_suspend_state(suspend_state_t new_state)
{
	unsigned long irqflags;
	int old_sleep;

	spin_lock_irqsave(&state_lock, irqflags);
	old_sleep = state & SUSPEND_REQUESTED;
	if (debug_mask & DEBUG_USER_STATE) {
		struct timespec ts;
		struct rtc_time tm;
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
		pr_info("request_suspend_state: %s (%d->%d) at %lld "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			new_state != PM_SUSPEND_ON ? "sleep" : "wakeup",
			requested_suspend_state, new_state,
			ktime_to_ns(ktime_get()),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	}
	if (!old_sleep && new_state != PM_SUSPEND_ON) {
		state |= SUSPEND_REQUESTED;
#ifdef CONFIG_MACH_LGE
		pr_info("queue early_suspend_work\n");
#endif
		queue_work(suspend_work_queue, &early_suspend_work);
	} else if (old_sleep && new_state == PM_SUSPEND_ON) {
		state &= ~SUSPEND_REQUESTED;
		wake_lock(&main_wake_lock);
#ifdef CONFIG_MACH_LGE
		pr_info("queue late_resume_work\n");
#endif
		queue_work(suspend_work_queue, &late_resume_work);
#ifdef CONFIG_MACH_LGE
		log_resume(RESUME_KICK);
#endif
	}
	requested_suspend_state = new_state;
	spin_unlock_irqrestore(&state_lock, irqflags);
}

suspend_state_t get_suspend_state(void)
{
	return requested_suspend_state;
}

#ifdef CONFIG_DEBUG_FS
#ifdef CONFIG_MACH_LGE
static inline void log_resume(enum log_resume_step step)
{
	int msec;
	struct timespec ts_current, ts_sub;
	struct resume_delay *record;

	if (step == RESUME_KICK) {
		getnstimeofday(&ts_resume_kick);
		return;
	} else if (step == RESUME_ENTRY) {
		record = &resume_delay;
	} else if (step == RESUME_EXIT) {
		record = &resume_total;
	}

	getnstimeofday(&ts_current);
	ts_sub = timespec_sub(ts_current, ts_resume_kick);
	msec = ts_sub.tv_sec + ts_sub.tv_nsec / NSEC_PER_MSEC;

	record->avg = ((record->avg * record->count) + msec);
	++record->count;
	record->avg = record->avg / record->count;

	if (msec > record->max)
		record->max = msec;

	return;
}

static inline void late_resume_call_chain(struct early_suspend *pos)
{
	struct timespec ts_entry, ts_exit, ts_sub;
	int msec;

	getnstimeofday(&ts_entry);
	pos->resume(pos);
	getnstimeofday(&ts_exit);

	ts_sub = timespec_sub(ts_exit, ts_entry);
	msec = ts_sub.tv_sec + ts_sub.tv_nsec / NSEC_PER_MSEC;

	pos->resume_avg = ((pos->resume_avg * pos->resume_count) + msec);
	++pos->resume_count;
	pos->resume_avg = pos->resume_avg / pos->resume_count;

	if (msec > pos->resume_max)
		pos->resume_max = msec;
}

static inline void early_suspend_call_chain(struct early_suspend *pos)
{
	struct timespec ts_entry, ts_exit, ts_sub;
	int msec;

	getnstimeofday(&ts_entry);
	pos->suspend(pos);
	getnstimeofday(&ts_exit);

	ts_sub = timespec_sub(ts_exit, ts_entry);
	msec = ts_sub.tv_sec + ts_sub.tv_nsec / NSEC_PER_MSEC;

	pos->suspend_avg = ((pos->suspend_avg * pos->suspend_count) + msec);
	++pos->suspend_count;
	pos->suspend_avg = pos->suspend_avg / pos->suspend_count;

	if (msec > pos->suspend_max)
		pos->suspend_max = msec;
}

static int earlysuspend_func_time_debug_show(struct seq_file *s, void *data)
{
	struct early_suspend *pos;
	char sym[KSYM_SYMBOL_LEN];

	seq_printf(s, "late_resume total time (msec)\n");
	seq_printf(s, "    avg:%5d    max:%5d    count:%d\n",
			resume_total.avg, resume_total.max, resume_total.count);
	seq_printf(s, "late_resume wq schedule delay(msec)\n");
	seq_printf(s, "    avg:%5d    max:%5d\n",
			resume_delay.avg, resume_delay.max);

	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			sprint_symbol(sym, (unsigned long)pos->suspend);
			seq_printf(s, "suspend: %s\n", sym);
			seq_printf(s, "    avg:%5d    max:%5d\n",
					pos->suspend_avg, pos->suspend_max);
		}
	}

	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->resume != NULL) {
			sprint_symbol(sym, (unsigned long)pos->resume);
			seq_printf(s, "resume: %s\n", sym);
			seq_printf(s, "    avg:%5d    max:%5d\n",
					pos->resume_avg, pos->resume_max);
		}
	}

	return 0;
}

static int earlysuspend_func_time_debug_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, earlysuspend_func_time_debug_show, NULL);
}

static const struct file_operations earlysuspend_func_time_debug_fops = {
	.open       = earlysuspend_func_time_debug_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int __init earlysuspend_func_time_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("earlysuspend_func_time", 0755, NULL, NULL,
			&earlysuspend_func_time_debug_fops);
	if (!d) {
		pr_err("Failed to create earlysuspend_func_time debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(earlysuspend_func_time_debug_init);
#endif
#endif

#ifdef CONFIG_MACH_LGE
static void earlysuspend_watchdog_timer_func(unsigned long nr)
{
	BUG();
}

static int __init earlysuspend_watchdog_timer_init(void)
{
	es_watchdog_timer.function = earlysuspend_watchdog_timer_func;

	init_timer(&es_watchdog_timer);

	early_suspend_monitor_id = create_blocking_monitor("early_suspend");
	if (early_suspend_monitor_id < 0)
		return early_suspend_monitor_id;

	late_resume_monitor_id = create_blocking_monitor("late_resume");
	if (late_resume_monitor_id < 0)
		return late_resume_monitor_id;

	return 0;
}

late_initcall(earlysuspend_watchdog_timer_init);
#endif

