/*
 * Author: Alex Deddo aka Savoca <adeddo27@gmail.com>
 *
 * Copyright 2014 Alex Deddo
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
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/rq_stats.h>
#include <linux/slab.h>
#include <linux/input.h>

#if CONFIG_POWERSUSPEND
#include <linux/powersuspend.h>
#endif

#define ACM_VERSION_X        1
#define ACM_VERSION_Y        0

static unsigned int acm_active = 0;
module_param(acm_active, uint, 0644);

static unsigned int acm_cores = 4;
module_param(acm_cores, uint, 0644);

static unsigned int sampling_time = 0;

static unsigned int persist_count = 0;
static unsigned int busy_persist_count = 0;

static bool suspended = false;

#define NR_FSHIFT        3
static unsigned int nr_fshift = NR_FSHIFT;
module_param(nr_fshift, uint, 0644);

static unsigned int nr_run_thresholds_acm[] = {
/*         1,  2,  3,  4 - on-line cpus target */
        5,  7,  9,  UINT_MAX /* avg run threads * 2 (e.g., 9 = 2.25 threads) */
        };

static unsigned int nr_run_hysteresis = 4;  /* 0.5 thread */
module_param(nr_run_hysteresis, uint, 0644);

static unsigned int nr_run_last;

static unsigned int NwNs_Threshold[] = { 19, 30,  19,  11,  19,  11, 0,  11};
static unsigned int TwTs_Threshold[] = {140,  0, 140, 190, 140, 190, 0, 190};

static unsigned int calculate_thread_stats(void)
{
	unsigned int avg_nr_run = avg_nr_running();
	unsigned int nr_run;
	unsigned int threshold_size;

	if (acm_active == 1) {
		threshold_size = ARRAY_SIZE(nr_run_thresholds_acm);
		nr_run_hysteresis = 8;
		nr_fshift = 3;

		for (nr_run = 1; nr_run < threshold_size; nr_run++) {
			unsigned int nr_threshold;
			nr_threshold = nr_run_thresholds_acm[nr_run - 1];

			if (nr_run_last <= nr_run)
        	                nr_threshold += nr_run_hysteresis;
        	        if (avg_nr_run <= (nr_threshold << (FSHIFT - nr_fshift)))
        	                break;
		}
		nr_run_last = nr_run;

		return nr_run;
	}
}
		

static void __cpuinit acm_main(struct work_struct *work)
{
	unsigned int nr_run_stat;
	unsigned int cpu_count = 0;
	unsigned int nr_cpus = 0;

	int active_cores;
	int set_cores;
	int i;

	if (acm_active == 1) {
		nr_run_stat = calculate_thread_stats();
		cpu_count = nr_run_stat;
		nr_cpus = num_online_cpus();
	
		active_cores = acm_cores;

		// Wake up all cores so we can bring them back down if necessary.
		set_cores = 4;
			for (i = 1; i < set_cores; i++) {
				cpu_up(i);
			}

		if (active_cores < 1 || active_cores > 4 || active_cores == 4) {
			set_cores = 4;
			for (i = 1; i < set_cores; i++) {
				cpu_up(i);
			}
		} else {
			for (i = active_cores - 1; i > 0; i--) {
				cpu_down(i);
			}
		}
	}
}

int __init acm_init(void)
{
	if (acm_active == 1) {
		pr_info("ACM: version %d.%d by Savoca\n",
			 ACM_VERSION_X,
			 ACM_VERSION_Y);
	}

	return 0;
}

MODULE_AUTHOR("Alex Deddo <adeddo27@gmail.com");
MODULE_DESCRIPTION("A CPU driver for user control of active cores");
MODULE_LICENSE("GPL");
