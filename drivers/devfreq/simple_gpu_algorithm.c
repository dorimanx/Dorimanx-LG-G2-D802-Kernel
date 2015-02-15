/*
 * Author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * Copyright 2011~2014 Paul Reioux
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
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/msm_adreno_devfreq.h>

static int default_laziness = 2;
module_param_named(simple_laziness, default_laziness, int, 0664);

static int ramp_up_threshold = 6000;
module_param_named(simple_ramp_threshold, ramp_up_threshold, int, 0664);

int simple_gpu_active = 0;
module_param_named(simple_gpu_activate, simple_gpu_active, int, 0664);

int simple_gpu_debug = 0;
module_param_named(simple_gpu_debug, simple_gpu_debug, int, 0664);

static int laziness;

int simple_gpu_algorithm(int level, struct devfreq_msm_adreno_tz_data *priv)
{
	int val = 0;

	if (simple_gpu_debug == 1)
		pr_info("simple_gpu_debug: level is %d\n", level);

	if (simple_gpu_debug == 2)
		pr_info("simple_gpu_debug: busy_time is %u\n", (unsigned int)priv->bin.busy_time);

	if (simple_gpu_debug == 3)
		pr_info("simple_gpu_debug: bus.num is %d\n", priv->bus.num + 3);

	/* it's currently busy */
	if ((unsigned int)priv->bin.busy_time > ramp_up_threshold) {
		if (level == 0)
			val = 0; /* already maxed, so do nothing */
		else if ((level > 0) && (level <= (priv->bus.num + 3)))
			val = -1; /* bump up to next pwrlevel */
	/* idle case */
	} else {
		if ((level >= 0) && (level < (priv->bus.num + 3)))
			if (laziness > 0) {
				/* hold off for a while */
				laziness--;
				val = 0; /* don't change anything yet */
			} else {
				val = 1; /* above min, lower it */
				/* reset laziness count */
				laziness = default_laziness;
		} else if (level == (priv->bus.num + 3))
			val = 0; /* already @ min, so do nothing */
	}
	if (simple_gpu_debug == 4)
		pr_info("simple_gpu_debug: returning val is %d\n", val);

	return val;
}
EXPORT_SYMBOL(simple_gpu_algorithm);

static int __init simple_gpu_init(void)
{
	return 0;
}
subsys_initcall(simple_gpu_init);

static void __exit simple_gpu_exit(void)
{
	return;
}
module_exit(simple_gpu_exit);

MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_DESCRIPTION("'simple_gpu_algorithm - A Simple user configurable GPU"
	"Control Algorithm for Adreno GPU series");
MODULE_LICENSE("GPL");
