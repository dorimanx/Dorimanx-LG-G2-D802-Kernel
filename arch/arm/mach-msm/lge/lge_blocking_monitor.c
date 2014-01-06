/*
 * arch/arm/mach-msm/lge/lge_block_monitor.c
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
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/current.h>

#include <mach/lge_blocking_monitor.h>

#define MAX_BLOCKING_MONITOR_NUMBER	10

struct blocking_monitor {
	int id;
	const char *name;
	struct timer_list timer;
	struct task_struct *task;
};

static struct blocking_monitor *bl_monitor[MAX_BLOCKING_MONITOR_NUMBER];

static void dump_blocking_callstack(unsigned long nr)
{
	struct blocking_monitor *bl_monitor = (struct blocking_monitor *)nr;

	pr_err("Start blocking callstack dump\n");
	pr_err("Blocking callstack name is %s\n", bl_monitor->name);
	show_stack(bl_monitor->task, NULL);
	pr_err("End blocking callstack dump\n");
}

int start_monitor_blocking(int id, unsigned long expires)
{
	if (id < 0 || MAX_BLOCKING_MONITOR_NUMBER <= id)
		return -EINVAL;

	if (bl_monitor[id] == 0)
		return -EINVAL;

	bl_monitor[id]->timer.expires = expires;
	bl_monitor[id]->task = (struct task_struct *)current;
	bl_monitor[id]->timer.data = (unsigned long)bl_monitor[id];

	add_timer(&bl_monitor[id]->timer);

	return 0;
}

int end_monitor_blocking(int id)
{
	if (id < 0 || MAX_BLOCKING_MONITOR_NUMBER <= id)
		return -EINVAL;

	if (bl_monitor[id] == 0)
		return -EINVAL;

	del_timer(&bl_monitor[id]->timer);

	return 0;
}

int create_blocking_monitor(const char *name)
{
	int i;

	for (i = 0; i < MAX_BLOCKING_MONITOR_NUMBER; ++i) {
		if (bl_monitor[i] == 0)
			break;
	}

	if (i == MAX_BLOCKING_MONITOR_NUMBER)
		return -EINVAL;

	bl_monitor[i] = kmalloc(sizeof(struct blocking_monitor), GFP_KERNEL);

	bl_monitor[i]->id = i;
	bl_monitor[i]->name = name;
	bl_monitor[i]->timer.function = dump_blocking_callstack;

	init_timer(&bl_monitor[i]->timer);

	return bl_monitor[i]->id;
}

int remove_blocking_monitor(int id)
{
	if (id < 0 || MAX_BLOCKING_MONITOR_NUMBER <= id)
		return -EINVAL;

	if (bl_monitor[id] == 0)
		return -EINVAL;

	del_timer(&bl_monitor[id]->timer);
	kfree(bl_monitor[id]);
	bl_monitor[id] = 0;

	return 0;
}
