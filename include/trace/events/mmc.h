/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmc

#if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMC_H

#include <linux/tracepoint.h>

TRACE_EVENT(mmc_clk,
		TP_PROTO(char *print_info),

		TP_ARGS(print_info),

		TP_STRUCT__entry(
			__string(print_info, print_info)
		),

		TP_fast_assign(
			__assign_str(print_info, print_info);
		),

		TP_printk("%s",
			__get_str(print_info)
		)
);

DECLARE_EVENT_CLASS(mmc_pm_template,
	TP_PROTO(const char *dev_name, int err, s64 usecs),

	TP_ARGS(dev_name, err, usecs),

	TP_STRUCT__entry(
		__field(s64, usecs)
		__field(int, err)
		__string(dev_name, dev_name)
	),

	TP_fast_assign(
		__entry->usecs = usecs;
		__entry->err = err;
		__assign_str(dev_name, dev_name);
	),

	TP_printk(
		"took %lld usecs, %s err %d",
		__entry->usecs,
		__get_str(dev_name),
		__entry->err
	)
);

DEFINE_EVENT(mmc_pm_template, mmc_suspend_host,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, mmc_resume_host,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

#endif /* if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
