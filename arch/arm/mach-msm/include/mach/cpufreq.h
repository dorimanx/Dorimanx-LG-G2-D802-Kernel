/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_MACH_CPUFREQ_H
#define __ARCH_ARM_MACH_MSM_MACH_CPUFREQ_H

#define MSM_CPUFREQ_NO_LIMIT 0xFFFFFFFF

#ifdef CONFIG_CPU_FREQ_MSM

extern unsigned int get_cpu_min_lock(unsigned int cpu);
extern void set_cpu_min_lock(unsigned int cpu, int freq);
extern unsigned int get_max_lock(unsigned int cpu);
extern void set_max_lock(unsigned int cpu, unsigned int freq);

#else
static unsigned int get_cpu_min_lock(unsigned int cpu)
{
	return -ENOSYS;
}
static void set_cpu_min_lock(unsigned int cpu, int freq)
{
	return -ENOSYS;
}
static unsigned int get_max_lock(unsigned int cpu)
{
	return -ENOSYS;
}
static void set_max_lock(unsigned int cpu, unsigned int freq)
{
	return -ENOSYS;
}
#endif

void set_speed_pvs_bin(int speed, int pvs);
void get_speed_bin(int *speed);
void get_pvs_bin(int *pvs);

#endif /* __ARCH_ARM_MACH_MSM_MACH_CPUFREQ_H */
