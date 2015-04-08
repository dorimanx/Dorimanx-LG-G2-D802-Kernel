/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Added code to work as a standalone intelligent thermal throttling driver
 * for many Qualcomm SOCs by Paul Reioux (Faux123)
 * Merged with MSM Thermal code by Yuri Sh. (Dorimanx)
 * Modifications copyright (c) 2014
 *
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/msm_tsens.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/thermal.h>
#include <mach/rpm-regulator.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/cpufreq.h>
#include <linux/regulator/consumer.h>

#define MAX_RAILS 5

#define DEFAULT_POLLING_MS	250

#ifdef CONFIG_INTELLI_THERMAL_STATS
/* last 3 minutes based on $DEFAULT_POLLING_MS polling cycle */
#define MAX_HISTORY_SZ		((3*60*1000) / DEFAULT_POLLING_MS)

struct msm_thermal_stat_data {
	int32_t temp_history[MAX_HISTORY_SZ];
	uint32_t throttled;
	uint32_t warning;
	uint32_t normal;
};
static struct msm_thermal_stat_data msm_thermal_stats;
static uint32_t hist_index = 0;
#endif

static struct msm_thermal_data msm_thermal_info;

static struct msm_thermal_data_intelli msm_thermal_info_local = {
	.sensor_id = 0,
	.poll_ms = DEFAULT_POLLING_MS,
	.limit_temp_degC = 75,
	.temp_hysteresis_degC = 10,
	.freq_step = 2,
	.freq_control_mask = 0xf,
	.core_limit_temp_degC = 80,
	.core_temp_hysteresis_degC = 5,
	.core_control_mask = 0xe,
};

static struct delayed_work check_temp_work;
bool core_control;

/* dummy parameter for rom thermal and apps */
static bool enabled = 1;

static unsigned int debug_mode = 0;
static uint32_t cpus_offlined;
static DEFINE_MUTEX(core_control_mutex);

/* Always enable Intelli Thermal on boot */
static int intelli_enabled = 1;

static int rails_cnt;
static int psm_rails_cnt;
static int limit_idx;

/*
 * min limit is set to 1190400 Mhz!
 * check your FREQ Table and set corect limit_idx_low freq number.
 */
static int limit_idx_low = 7;

static int limit_idx_high;
static int throttled;
static int max_idx;
static int max_tsens_num;
static bool immediately_limit_stop = false;
static struct cpufreq_frequency_table *table;
static uint32_t usefreq;
static int freq_table_get;
static bool vdd_rstr_enabled;
static bool vdd_rstr_nodes_called;
static bool vdd_rstr_probed;
static bool psm_enabled;
static bool psm_nodes_called;
static bool psm_probed;
static int *tsens_id_map;
static DEFINE_MUTEX(vdd_rstr_mutex);
static DEFINE_MUTEX(psm_mutex);

struct cpu_info {
	uint32_t cpu;
	uint32_t limited_max_freq;
	uint32_t limited_min_freq;
};

/* module parameters */
module_param_named(poll_ms, msm_thermal_info_local.poll_ms, uint, 0664);
module_param_named(limit_temp_degC, msm_thermal_info_local.limit_temp_degC,
			int, 0664);
module_param_named(temp_hysteresis_degC,
			msm_thermal_info_local.temp_hysteresis_degC,
			int, 0664);
module_param_named(freq_step, msm_thermal_info_local.freq_step,
			int, 0664);
module_param_named(immediately_limit_stop, immediately_limit_stop,
			bool, 0664);
module_param_named(core_limit_temp_degC,
			msm_thermal_info_local.core_limit_temp_degC,
			int, 0664);
module_param_named(core_temp_hysteresis_degC,
			msm_thermal_info_local.core_temp_hysteresis_degC,
			int, 0664);
module_param_named(freq_control_mask, msm_thermal_info_local.freq_control_mask,
			uint, 0664);
module_param_named(core_control_mask, msm_thermal_info_local.core_control_mask,
			uint, 0664);

module_param_named(thermal_limit_high, limit_idx_high, int, 0664);
module_param_named(thermal_limit_low, limit_idx_low, int, 0664);

module_param_named(thermal_debug_mode, debug_mode, int, 0664);

static unsigned int safety = 1;
module_param_named(temp_safety, safety, int, 0664);

static unsigned int freq_debug = 0;
module_param_named(freq_limit_debug, freq_debug, uint, 0644);

#define dprintk(msg...)		\
do {				\
	if (freq_debug)		\
		pr_info(msg);	\
} while (0)

struct rail {
	const char *name;
	uint32_t freq_req;
	uint32_t min_level;
	uint32_t num_levels;
	int32_t curr_level;
	uint32_t levels[3];
	struct kobj_attribute value_attr;
	struct kobj_attribute level_attr;
	struct regulator *reg;
	struct attribute_group attr_gp;
};

struct psm_rail {
	const char *name;
	uint8_t init;
	uint8_t mode;
	struct kobj_attribute mode_attr;
	struct rpm_regulator *reg;
	struct attribute_group attr_gp;
};

static struct psm_rail *psm_rails;
static struct rail *rails;
static struct cpu_info cpus[NR_CPUS];

struct vdd_rstr_enable {
	struct kobj_attribute ko_attr;
	uint32_t enabled;
};

/* For SMPS only*/
enum PMIC_SW_MODE {
	PMIC_AUTO_MODE  = RPM_REGULATOR_MODE_AUTO,
	PMIC_IPEAK_MODE = RPM_REGULATOR_MODE_IPEAK,
	PMIC_PWM_MODE   = RPM_REGULATOR_MODE_HPM,
};

#define VDD_RES_RO_ATTRIB(_rail, ko_attr, j, _name) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0444; \
	ko_attr.show = vdd_rstr_reg_##_name##_show; \
	ko_attr.store = NULL; \
	sysfs_attr_init(&ko_attr.attr); \
	_rail.attr_gp.attrs[j] = &ko_attr.attr;

#define VDD_RES_RW_ATTRIB(_rail, ko_attr, j, _name) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0644; \
	ko_attr.show = vdd_rstr_reg_##_name##_show; \
	ko_attr.store = vdd_rstr_reg_##_name##_store; \
	sysfs_attr_init(&ko_attr.attr); \
	_rail.attr_gp.attrs[j] = &ko_attr.attr;

#define VDD_RSTR_ENABLE_FROM_ATTRIBS(attr) \
	(container_of(attr, struct vdd_rstr_enable, ko_attr));

#define VDD_RSTR_REG_VALUE_FROM_ATTRIBS(attr) \
	(container_of(attr, struct rail, value_attr));

#define VDD_RSTR_REG_LEVEL_FROM_ATTRIBS(attr) \
	(container_of(attr, struct rail, level_attr));

#define PSM_RW_ATTRIB(_rail, ko_attr, j, _name) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0644; \
	ko_attr.show = psm_reg_##_name##_show; \
	ko_attr.store = psm_reg_##_name##_store; \
	sysfs_attr_init(&ko_attr.attr); \
	_rail.attr_gp.attrs[j] = &ko_attr.attr;

#define PSM_REG_MODE_FROM_ATTRIBS(attr) \
	(container_of(attr, struct psm_rail, mode_attr));

static int msm_thermal_cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	uint32_t max_freq_req = cpus[policy->cpu].limited_max_freq;
	uint32_t min_freq_req = cpus[policy->cpu].limited_min_freq;

	switch (event) {
	case CPUFREQ_INCOMPATIBLE:
		pr_debug("mitigating CPU%d to freq max: %u min: %u\n",
		policy->cpu, max_freq_req, min_freq_req);

		cpufreq_verify_within_limits(policy, min_freq_req,
				max_freq_req);

		if (max_freq_req < min_freq_req)
			pr_err("Invalid frequency request Max:%u Min:%u\n",
					max_freq_req, min_freq_req);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block msm_thermal_cpufreq_notifier = {
	.notifier_call = msm_thermal_cpufreq_callback,
};

/* If freq table exists, then we can send freq request */
static int check_freq_table(void)
{
	int ret = 0;
	struct cpufreq_frequency_table *table = NULL;

	table = cpufreq_frequency_get_table(0);
	if (!table) {
		pr_debug("error reading cpufreq table\n");
		return -EINVAL;
	}
	freq_table_get = 1;

	return ret;
}

static int update_cpu_freq(int cpu, uint32_t cpu_freq)
{
	int ret = 0;
	struct cpufreq_policy policy;

	ret = msm_cpufreq_set_freq_limits(cpu,
			MSM_CPUFREQ_NO_LIMIT,
			cpu_freq);
	if (ret)
		return ret;

	policy.cpu = cpu;

	cpufreq_driver_target(&policy, cpu_freq, CPUFREQ_RELATION_L);

	return ret;
}

static int update_cpu_min_freq_all(uint32_t min)
{
	uint32_t cpu = 0;
	int ret = 0;

	if (!freq_table_get) {
		ret = check_freq_table();
		if (ret) {
			pr_err("Fail to get freq table. err:%d\n", ret);
			return ret;
		}
	}
	/* If min is larger than allowed max */
	min = min(min, table[limit_idx_high].frequency);

	get_online_cpus();
	for_each_possible_cpu(cpu) {
		cpus[cpu].limited_min_freq = min;
		update_cpu_freq(cpu, min);
	}
	put_online_cpus();

	return ret;
}

static int vdd_restriction_apply_freq(struct rail *r, int level)
{
	int ret = 0;

	if (level == r->curr_level)
		return ret;

	/* level = -1: disable, level = 0,1,2..n: enable */
	if (level == -1) {
		ret = update_cpu_min_freq_all(r->min_level);
		if (ret)
			return ret;
		else
			r->curr_level = -1;
	} else if (level >= 0 && level < (r->num_levels)) {
		ret = update_cpu_min_freq_all(r->levels[level]);
		if (ret)
			return ret;
		else
			r->curr_level = level;
	} else {
		pr_err("level input:%d is not within range\n", level);
		return -EINVAL;
	}

	return ret;
}

static int vdd_restriction_apply_voltage(struct rail *r, int level)
{
	int ret = 0;

	if (r->reg == NULL) {
		pr_err("%s don't have regulator handle. can't apply vdd\n",
				r->name);
		return -EFAULT;
	}
	if (level == r->curr_level)
		return ret;

	/* level = -1: disable, level = 0,1,2..n: enable */
	if (level == -1) {
		ret = regulator_set_voltage(r->reg, r->min_level,
			r->levels[r->num_levels - 1]);
		if (!ret)
			r->curr_level = -1;
		pr_debug("Requested min level for %s. curr level: %d\n",
				r->name, r->curr_level);
	} else if (level >= 0 && level < (r->num_levels)) {
		ret = regulator_set_voltage(r->reg, r->levels[level],
			r->levels[r->num_levels - 1]);
		if (!ret)
			r->curr_level = level;
		pr_debug("Requesting level %d for %s. curr level: %d\n",
			r->levels[level], r->name, r->levels[r->curr_level]);
	} else {
		pr_err("level input:%d is not within range\n", level);
		return -EINVAL;
	}

	return ret;
}

/* Setting all rails the same mode */
static int psm_set_mode_all(int mode)
{
	int i = 0;
	int fail_cnt = 0;
	int ret = 0;

	pr_debug("Requesting PMIC Mode: %d\n", mode);
	for (i = 0; i < psm_rails_cnt; i++) {
		if (psm_rails[i].mode != mode) {
			ret = rpm_regulator_set_mode(psm_rails[i].reg, mode);
			if (ret) {
				pr_err("Cannot set mode:%d for %s. err:%d",
					mode, psm_rails[i].name, ret);
				fail_cnt++;
			} else
				psm_rails[i].mode = mode;
		}
	}

	return fail_cnt ? (-EFAULT) : ret;
}

static int vdd_rstr_en_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vdd_rstr_enable *en = VDD_RSTR_ENABLE_FROM_ATTRIBS(attr);

	return snprintf(buf, PAGE_SIZE, "%d\n", en->enabled);
}

static ssize_t vdd_rstr_en_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int i = 0;
	uint8_t en_cnt = 0;
	uint8_t dis_cnt = 0;
	uint32_t val = 0;
	struct kernel_param kp;
	struct vdd_rstr_enable *en = VDD_RSTR_ENABLE_FROM_ATTRIBS(attr);

	mutex_lock(&vdd_rstr_mutex);
	kp.arg = &val;
	ret = param_set_bool(buf, &kp);
	if (ret) {
		pr_err("Invalid input %s for enabled\n", buf);
		goto done_vdd_rstr_en;
	}

	if ((val == 0) && (en->enabled == 0))
		goto done_vdd_rstr_en;

	for (i = 0; i < rails_cnt; i++) {
		if (rails[i].freq_req == 1 && freq_table_get)
			ret = vdd_restriction_apply_freq(&rails[i],
					(val) ? 0 : -1);
		else
			ret = vdd_restriction_apply_voltage(&rails[i],
			(val) ? 0 : -1);

		/*
		 * Even if fail to set one rail, still try to set the
		 * others. Continue the loop
		 */
		if (ret)
			pr_err("Set vdd restriction for %s failed\n",
					rails[i].name);
		else {
			if (val)
				en_cnt++;
			else
				dis_cnt++;
		}
	}
	/* As long as one rail is enabled, vdd rstr is enabled */
	if (val && en_cnt)
		en->enabled = 1;
	else if (!val && (dis_cnt == rails_cnt))
		en->enabled = 0;
	pr_debug("%s vdd restriction. curr: %d\n",
			(val) ? "Enable" : "Disable", en->enabled);

done_vdd_rstr_en:
	mutex_unlock(&vdd_rstr_mutex);
	return count;
}

static struct vdd_rstr_enable vdd_rstr_en = {
	.ko_attr.attr.name = __stringify(enabled),
	.ko_attr.attr.mode = 0644,
	.ko_attr.show = vdd_rstr_en_show,
	.ko_attr.store = vdd_rstr_en_store,
	.enabled = 1,
};

static struct attribute *vdd_rstr_en_attribs[] = {
	&vdd_rstr_en.ko_attr.attr,
	NULL,
};

static struct attribute_group vdd_rstr_en_attribs_gp = {
	.attrs  = vdd_rstr_en_attribs,
};

static int vdd_rstr_reg_value_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int val = 0;
	struct rail *reg = VDD_RSTR_REG_VALUE_FROM_ATTRIBS(attr);
	/* -1:disabled, -2:fail to get regualtor handle */
	if (reg->curr_level < 0)
		val = reg->curr_level;
	else
		val = reg->levels[reg->curr_level];

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static int vdd_rstr_reg_level_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct rail *reg = VDD_RSTR_REG_LEVEL_FROM_ATTRIBS(attr);
	return snprintf(buf, PAGE_SIZE, "%d\n", reg->curr_level);
}

static ssize_t vdd_rstr_reg_level_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	struct rail *reg = VDD_RSTR_REG_LEVEL_FROM_ATTRIBS(attr);

	mutex_lock(&vdd_rstr_mutex);
	if (vdd_rstr_en.enabled == 0)
		goto done_store_level;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s for level\n", buf);
		goto done_store_level;
	}

	if (val < 0 || val > reg->num_levels - 1) {
		pr_err(" Invalid number %d for level\n", val);
		goto done_store_level;
	}

	if (val != reg->curr_level) {
		if (reg->freq_req == 1 && freq_table_get)
			update_cpu_min_freq_all(reg->levels[val]);
		else {
			ret = vdd_restriction_apply_voltage(reg, val);
			if (ret) {
				pr_err( \
				"Set vdd restriction for regulator %s \
						failed. err:%d\n",
				reg->name, ret);
				goto done_store_level;
			}
		}
		reg->curr_level = val;
		pr_debug("Request level %d for %s\n",
				reg->curr_level, reg->name);
	}

done_store_level:
	mutex_unlock(&vdd_rstr_mutex);
	return count;
}

static int psm_reg_mode_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct psm_rail *reg = PSM_REG_MODE_FROM_ATTRIBS(attr);
	return snprintf(buf, PAGE_SIZE, "%d\n", reg->mode);
}

static ssize_t psm_reg_mode_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct psm_rail *reg = PSM_REG_MODE_FROM_ATTRIBS(attr);

	mutex_lock(&psm_mutex);
	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s for mode\n", buf);
		goto done_psm_store;
	}

	if ((val != PMIC_PWM_MODE) && (val != PMIC_AUTO_MODE)) {
		pr_err("Invalid number %d for mode\n", val);
		goto done_psm_store;
	}

	if (val != reg->mode) {
		ret = rpm_regulator_set_mode(reg->reg, val);
		if (ret) {
			pr_err("Fail to set Mode:%d for %s. err:%d\n",
			val, reg->name, ret);
			goto done_psm_store;
		}
		reg->mode = val;
	}

done_psm_store:
	mutex_unlock(&psm_mutex);
	return count;
}

static int check_sensor_id(int sensor_id)
{
	int i = 0;
	bool hw_id_found = false;
	int ret = 0;

	for (i = 0; i < max_tsens_num; i++) {
		if (sensor_id == tsens_id_map[i]) {
			hw_id_found = true;
			break;
		}
	}
	if (!hw_id_found) {
		pr_err("Invalid sensor hw id:%d\n", sensor_id);
		return -EINVAL;
	}

	return ret;
}

static int create_sensor_id_map(void)
{
	int i = 0;
	int ret = 0;

	tsens_id_map = kzalloc(sizeof(int) * max_tsens_num,
			GFP_KERNEL);
	if (!tsens_id_map) {
		pr_err("Cannot allocate memory for tsens_id_map\n");
		return -ENOMEM;
	}

	for (i = 0; i < max_tsens_num; i++) {
		ret = tsens_get_hw_id_mapping(i, &tsens_id_map[i]);
		/* If return -ENXIO, hw_id is default in sequence */
		if (ret) {
			if (ret == -ENXIO) {
				tsens_id_map[i] = i;
				ret = 0;
			} else {
				pr_err("Failed to get hw id for id:%d.err:%d\n",
						i, ret);
				goto fail;
			}
		}
	}

	return ret;
fail:
	kfree(tsens_id_map);
	return ret;
}

#if 0
/* 1:enable, 0:disable */
static int vdd_restriction_apply_all(int en)
{
	int i = 0;
	int en_cnt = 0;
	int dis_cnt = 0;
	int fail_cnt = 0;
	int ret = 0;

	for (i = 0; i < rails_cnt; i++) {
		if (rails[i].freq_req == 1 && freq_table_get)
			ret = vdd_restriction_apply_freq(&rails[i],
					en ? 0 : -1);
		else
			ret = vdd_restriction_apply_voltage(&rails[i],
					en ? 0 : -1);
		if (ret) {
			pr_err("Failed to %s for %s. err:%d",
					(en) ? "enable" : "disable",
					rails[i].name, ret);
			fail_cnt++;
		} else {
			if (en)
				en_cnt++;
			else
				dis_cnt++;
		}
	}

	/* As long as one rail is enabled, vdd rstr is enabled */
	if (en && en_cnt)
		vdd_rstr_en.enabled = 1;
	else if (!en && (dis_cnt == rails_cnt))
		vdd_rstr_en.enabled = 0;

	/*
	 * Check fail_cnt again to make sure all of the rails are applied
	 * restriction successfully or not
	 */
	if (fail_cnt)
		return -EFAULT;
	return ret;
}
#endif

static int msm_thermal_get_freq_table(void)
{
	int ret = 0;
	int i = 0;

	table = cpufreq_frequency_get_table(0);
	if (table == NULL) {
		pr_err("error reading cpufreq table\n");
		ret = -EINVAL;
		goto fail;
	}

	while (table[i].frequency != CPUFREQ_TABLE_END)
		i++;

	limit_idx_high = limit_idx = max_idx = i - 1;
	BUG_ON(limit_idx_high <= 0 || limit_idx_high <= limit_idx_low);
fail:
	return ret;
}

#ifdef CONFIG_SMP
static void __ref do_core_control(long temp)
{
	int i = 0;
	int ret = 0;

	if (!core_control)
		return;

	/**
	 *  Offline cores starting from the max MPIDR to 1, when above limit,
	 *  The core control mask is non zero and allows the core to be turned
	 *  off.
	 *  The core was not previously offlined by this module
	 *  The core is the next in sequence.
	 *  If the core was online for some reason, even after it was offlined
	 *  by this module, offline it again.
	 *  Online the back on if the temp is below the hysteresis and was
	 *  offlined by this module and not already online.
	 */
	mutex_lock(&core_control_mutex);
	if (msm_thermal_info_local.core_control_mask &&
		temp >= msm_thermal_info_local.core_limit_temp_degC) {
		for (i = num_possible_cpus(); i > 0; i--) {
			if (!(msm_thermal_info_local.core_control_mask &
					BIT(i)))
				continue;
			if (cpus_offlined & BIT(i) && !cpu_online(i))
				continue;
			dprintk("%s: Set Offline: CPU%d Temp: %ld\n",
					KBUILD_MODNAME, i, temp);
			ret = cpu_down(i);
			if (ret)
				pr_err("%s: Error %d offline core %d\n",
					KBUILD_MODNAME, ret, i);
			cpus_offlined |= BIT(i);
			break;
		}
	} else if (msm_thermal_info_local.core_control_mask && cpus_offlined &&
		temp <= (msm_thermal_info_local.core_limit_temp_degC -
			msm_thermal_info_local.core_temp_hysteresis_degC)) {
		for (i = 0; i < num_possible_cpus(); i++) {
			if (!(cpus_offlined & BIT(i)))
				continue;
			cpus_offlined &= ~BIT(i);
			dprintk("%s: Allow Online CPU%d Temp: %ld\n",
					KBUILD_MODNAME, i, temp);
			/*
			 * If this core is already online, then bring up the
			 * next offlined core.
			 */
			if (cpu_online(i))
				continue;
			ret = cpu_up(i);
			if (ret)
				pr_err("Error %d online core %d\n",
						ret, i);
			break;
		}
	}
	mutex_unlock(&core_control_mutex);
}
#else
static void do_core_control(long temp)
{
	return;
}
#endif

#if 0
static int do_vdd_restriction(void)
{
	struct tsens_device tsens_dev;
	long temp = 0;
	int ret = 0;
	int i = 0;
	int dis_cnt = 0;

	if (!vdd_rstr_enabled)
		return ret;

	if (usefreq && !freq_table_get) {
		if (check_freq_table())
			return ret;
	}

	mutex_lock(&vdd_rstr_mutex);
	for (i = 0; i < max_tsens_num; i++) {
		tsens_dev.sensor_num = tsens_id_map[i];
		ret = tsens_get_temp(&tsens_dev, &temp);
		if (ret) {
			pr_debug("%s: Unable to read TSENS sensor %d\n",
					__func__, tsens_dev.sensor_num);
			dis_cnt++;
			continue;
		}
		if (temp <=  msm_thermal_info.vdd_rstr_temp_degC) {
			ret = vdd_restriction_apply_all(1);
			if (ret) {
				pr_err( \
				"Enable vdd rstr for all failed. err:%d\n",
					ret);
				goto exit;
			}
			pr_debug("Enabled Vdd Restriction tsens:%d. Temp:%ld\n",
			thresh[MSM_VDD_RESTRICTION].thresh_list[i].sensor_id,
			temp);
			goto exit;
		} else if (temp > msm_thermal_info.vdd_rstr_temp_hyst_degC)
			dis_cnt++;
	}
	if (dis_cnt == max_tsens_num) {
		ret = vdd_restriction_apply_all(0);
		if (ret) {
			pr_err("Disable vdd rstr for all failed. err:%d\n",
					ret);
			goto exit;
		}
		pr_debug("Disabled Vdd Restriction\n");
	}
exit:
	mutex_unlock(&vdd_rstr_mutex);
	return ret;
}
#endif

static int do_psm(void)
{
	struct tsens_device tsens_dev;
	long temp = 0;
	int ret = 0;
	int i = 0;
	int auto_cnt = 0;

	mutex_lock(&psm_mutex);
	for (i = 0; i < max_tsens_num; i++) {
		tsens_dev.sensor_num = tsens_id_map[i];
		ret = tsens_get_temp(&tsens_dev, &temp);
		if (ret) {
			pr_debug("%s: Unable to read TSENS sensor %d\n",
					__func__, tsens_dev.sensor_num);
			auto_cnt++;
			continue;
		}

		/*
		 * As long as one sensor is above the threshold, set PWM mode
		 * on all rails, and loop stops. Set auto mode when all rails
		 * are below thershold
		 */
		if (temp >  msm_thermal_info.psm_temp_degC) {
			ret = psm_set_mode_all(PMIC_PWM_MODE);
			if (ret) {
				pr_err("Set pwm mode for all failed. err:%d\n",
						ret);
				goto exit;
			}
			break;
		} else if (temp <= msm_thermal_info.psm_temp_hyst_degC)
			auto_cnt++;
	}

	if (auto_cnt == max_tsens_num) {
		ret = psm_set_mode_all(PMIC_AUTO_MODE);
		if (ret) {
			pr_err("Set auto mode for all failed. err:%d\n", ret);
			goto exit;
		}
		pr_debug("Requested PMIC AUTO Mode\n");
	}

exit:
	mutex_unlock(&psm_mutex);
	return ret;
}

static void __ref do_freq_control(long temp)
{
	uint32_t cpu = 0;
	uint32_t max_freq = cpus[cpu].limited_max_freq;

	if (safety == 0) {
		if (msm_thermal_info_local.limit_temp_degC > 83)
			msm_thermal_info_local.limit_temp_degC = 83;
	} else {
		if (msm_thermal_info_local.limit_temp_degC > 78)
			msm_thermal_info_local.limit_temp_degC = 78;
	}

	if (debug_mode == 1)
		printk(KERN_ERR "pre-check do_freq_control temp[%ld], \
				limit_idx[%d], limit_idx_low[%d], \
				limited_idx_high[%d]\n",
				temp, limit_idx, limit_idx_low,
				limit_idx_high);

	if (temp >= msm_thermal_info_local.limit_temp_degC) {
		if (limit_idx == limit_idx_low)
			return;

		limit_idx -= msm_thermal_info_local.freq_step;
		if (limit_idx < limit_idx_low)
			limit_idx = limit_idx_low;
		/* Consider saved policy->max freq */
		max_freq = table[min(max_idx -
					(int)msm_thermal_info_local.freq_step,
					limit_idx)].frequency;
	} else if (temp < msm_thermal_info_local.limit_temp_degC -
			msm_thermal_info_local.temp_hysteresis_degC) {
		if (limit_idx == limit_idx_high)
			return;

		limit_idx += msm_thermal_info_local.freq_step;
		limit_idx_high = min(max_idx, limit_idx_high);
		if ((limit_idx >= limit_idx_high) ||
				immediately_limit_stop == true) {
			limit_idx = limit_idx_high;
			max_freq = MSM_CPUFREQ_NO_LIMIT;
		} else
			max_freq = table[limit_idx].frequency;
	}

	if (debug_mode == 1)
		printk(KERN_ERR "do_freq_control temp[%ld], \
				limit_idx[%d], max_freq[%d], \
				limited_max_freq[%d]\n",
				temp, limit_idx, max_freq,
				cpus[cpu].limited_max_freq);

	if (max_freq == cpus[cpu].limited_max_freq)
		return;

	if (!throttled && max_freq > 0 && max_freq != MSM_CPUFREQ_NO_LIMIT) {
		/*
		 * We're about to throttle cpu maximum freq. Save
		 * current policy->max frequency table index and
		 * set max_freq accordingly.
		 */
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
		max_idx = msm_cpufreq_get_index(policy, policy->max);
		max_freq = table[max_idx -
				msm_thermal_info_local.freq_step].frequency;
		throttled = 1;
	} else if (throttled && (max_freq < 0 ||
				max_freq == MSM_CPUFREQ_NO_LIMIT))
		throttled = 0;

	/* Update new limits */
	get_online_cpus();
	for_each_possible_cpu(cpu) {
		if (!(msm_thermal_info_local.freq_control_mask & BIT(cpu)))
			continue;
		cpus[cpu].limited_max_freq = max_freq;
		update_cpu_freq(cpu, max_freq);
	}
	put_online_cpus();
}

static void __ref check_temp(struct work_struct *work)
{
	static int limit_init;
	struct tsens_device tsens_dev;
	long temp = 0;
	int ret = 0;

	tsens_dev.sensor_num = msm_thermal_info_local.sensor_id;
	ret = tsens_get_temp(&tsens_dev, &temp);
	if (ret) {
		pr_debug("%s: Unable to read TSENS sensor %d\n",
				KBUILD_MODNAME, tsens_dev.sensor_num);
		goto reschedule;
	}

#ifdef CONFIG_INTELLI_THERMAL_STATS
	if (hist_index < MAX_HISTORY_SZ)
		msm_thermal_stats.temp_history[hist_index] = temp;
	else {
		hist_index = 0;
		msm_thermal_stats.temp_history[hist_index] = temp;
	}
	hist_index++;
#endif

	if (!limit_init) {
		ret = msm_thermal_get_freq_table();
		if (ret)
			goto reschedule;
		else
			limit_init = 1;
	}

	do_core_control(temp);
#if 0 /* we dont need this */
	do_vdd_restriction();
#endif
	do_psm();
	do_freq_control(temp);

reschedule:
	if (intelli_enabled)
		schedule_delayed_work(&check_temp_work, msecs_to_jiffies(
					msm_thermal_info_local.poll_ms));
}

static int __ref msm_thermal_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	uint32_t cpu = (uint32_t)hcpu;

	if (action == CPU_UP_PREPARE || action == CPU_UP_PREPARE_FROZEN) {
		if (core_control && (
				msm_thermal_info_local.core_control_mask &
				BIT(cpu)) && (cpus_offlined & BIT(cpu))) {
			if (debug_mode == 1)
				pr_info("%s: Preventing cpu%d from coming \
						online.\n", KBUILD_MODNAME,
						cpu);
			return NOTIFY_BAD;
		}
	}

	if (debug_mode == 1)
		pr_debug("voting for CPU%d to be online\n", cpu);
	return NOTIFY_OK;
}

static struct notifier_block __refdata msm_thermal_cpu_notifier = {
	.notifier_call = msm_thermal_cpu_callback,
};

/*
 * We will reset the cpu frequencies limits here. The core online/offline
 * status will be carried over to the process stopping the msm_thermal, as
 * we dont want to online a core and bring in the thermal issues.
 */
static void __ref disable_msm_thermal(void)
{
	uint32_t cpu = 0;

	/* make sure check_temp is no longer running */
	cancel_delayed_work_sync(&check_temp_work);

	get_online_cpus();
	for_each_possible_cpu(cpu) {
		if (cpus[cpu].limited_max_freq == MSM_CPUFREQ_NO_LIMIT &&
				cpus[cpu].limited_min_freq == 0)
			continue;
		cpus[cpu].limited_max_freq = MSM_CPUFREQ_NO_LIMIT;
		cpus[cpu].limited_min_freq = 0;
		update_cpu_freq(cpu, MSM_CPUFREQ_NO_LIMIT);
	}
	put_online_cpus();
}

static int __ref set_enabled(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	if (*val == '0' || *val == 'n' || *val == 'N') {
		intelli_enabled = 0;
		disable_msm_thermal();
		pr_info("%s: disabled!\n", KBUILD_MODNAME);
	} else {
		if (!intelli_enabled) {
			intelli_enabled = 1;
			schedule_delayed_work(&check_temp_work,
					msecs_to_jiffies(10000));
			pr_info("%s: rescheduling...\n", KBUILD_MODNAME);
		} else
			pr_info("%s: already running...\n \
				if you wish to disable echo N > \
				intelli_enabled\n", KBUILD_MODNAME);
	}
	pr_info("%s: enabled = %d\n", KBUILD_MODNAME, intelli_enabled);
	ret = param_set_bool(val, kp);

	return ret;
}

static struct kernel_param_ops module_ops = {
	.set = set_enabled,
	.get = param_get_bool,
};

module_param_cb(intelli_enabled, &module_ops, &intelli_enabled, 0644);
MODULE_PARM_DESC(intelli_enabled, "enforce thermal limit on cpu");

#ifdef CONFIG_INTELLI_THERMAL_STATS
static ssize_t show_thermal_stats(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{

	int i = 0;
	int tmp = 0;

	/* clear out old stats */
	msm_thermal_stats.throttled = 0;
	msm_thermal_stats.warning = 0;
	msm_thermal_stats.normal = 0;

	for (i = 0; i < MAX_HISTORY_SZ; i++) {
		tmp = msm_thermal_stats.temp_history[i];
		if (tmp >= msm_thermal_info_local.limit_temp_degC)
			msm_thermal_stats.throttled++;
		else if (tmp < msm_thermal_info_local.limit_temp_degC &&
			 tmp >= (msm_thermal_info_local.limit_temp_degC -
				 msm_thermal_info_local.temp_hysteresis_degC))
			msm_thermal_stats.warning++;
		else
			msm_thermal_stats.normal++;
	}
        return snprintf(buf, PAGE_SIZE, "%u %u %u\n",
			msm_thermal_stats.throttled,
			msm_thermal_stats.warning,
			msm_thermal_stats.normal);
}
static __refdata struct kobj_attribute msm_thermal_stat_attr =
__ATTR(statistics, 0444, show_thermal_stats, NULL);

static __refdata struct attribute *msm_thermal_stat_attrs[] = {
        &msm_thermal_stat_attr.attr,
        NULL,
};

static __refdata struct attribute_group msm_thermal_stat_attr_group = {
        .attrs = msm_thermal_stat_attrs,
};

static __init int msm_thermal_add_stat_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *stat_kobj = NULL;
	int ret = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module\n",
			KBUILD_MODNAME);
		ret = -ENOENT;
		goto done_stat_nodes;
	}

	stat_kobj = kobject_create_and_add("thermal_stats", module_kobj);
	if (!stat_kobj) {
		pr_err("%s: cannot create core control kobj\n",
				KBUILD_MODNAME);
		ret = -ENOMEM;
		goto done_stat_nodes;
	}

	ret = sysfs_create_group(stat_kobj, &msm_thermal_stat_attr_group);
	if (ret) {
		pr_err("%s: cannot create group\n", KBUILD_MODNAME);
		goto done_stat_nodes;
	}

	return 0;

done_stat_nodes:
	if (stat_kobj)
		kobject_del(stat_kobj);
	return ret;
}
#endif

#ifdef CONFIG_SMP
/* Call with core_control_mutex locked */
static int __ref update_offline_cores(int val)
{
	uint32_t cpu = 0;
	int ret = 0;

	cpus_offlined = msm_thermal_info_local.core_control_mask & val;
	if (!core_control)
		return 0;

	for_each_possible_cpu(cpu) {
		if (!(cpus_offlined & BIT(cpu)))
			continue;
		if (!cpu_online(cpu))
			continue;
		ret = cpu_down(cpu);
		if (ret)
			pr_err("%s: Unable to offline cpu%d\n",
				KBUILD_MODNAME, cpu);
	}
	return ret;
}
#else
static int update_offline_cores(int val)
{
	return 0;
}
#endif

static ssize_t show_cc_enabled_dummy(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", enabled);
}

static ssize_t __ref store_cc_enabled_dummy(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		pr_err("%s: Invalid input %s\n", KBUILD_MODNAME, buf);

	if (enabled == val)
		return count;

	enabled = val;

	return count;
}

static ssize_t show_cc_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", core_control);
}

static ssize_t __ref store_cc_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	mutex_lock(&core_control_mutex);
	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s. err:%d\n", buf, ret);
		goto done_store_cc;
	}

	if (core_control == !!val)
		goto done_store_cc;

	core_control = !!val;
	if (core_control) {
		pr_info("Core control enabled\n");
		register_cpu_notifier(&msm_thermal_cpu_notifier);
		update_offline_cores(cpus_offlined);
		enabled = 1;
	} else {
		pr_info("Core control disabled\n");
		unregister_cpu_notifier(&msm_thermal_cpu_notifier);
		enabled = 0;
	}

done_store_cc:
	mutex_unlock(&core_control_mutex);
	return count;
}

static ssize_t show_cpus_offlined(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cpus_offlined);
}

static ssize_t __ref store_cpus_offlined(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	uint32_t val = 0;

	mutex_lock(&core_control_mutex);
	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s. err:%d\n", buf, ret);
		goto done_cc;
	}

	if (intelli_enabled) {
		pr_err("Ignoring request; polling thread is enabled.\n");
		goto done_cc;
	}

	if (cpus_offlined == val)
		goto done_cc;

	update_offline_cores(val);
done_cc:
	mutex_unlock(&core_control_mutex);
	return count;
}

static __refdata struct kobj_attribute cc_enabled_attr =
__ATTR(core_control, 0664, show_cc_enabled, store_cc_enabled);

static __refdata struct kobj_attribute cc_enabled_dummy_attr =
__ATTR(enabled, 0664, show_cc_enabled_dummy, store_cc_enabled_dummy);

static __refdata struct kobj_attribute cpus_offlined_attr =
__ATTR(cpus_offlined, 0664, show_cpus_offlined, store_cpus_offlined);

static __refdata struct attribute *cc_attrs[] = {
	&cc_enabled_attr.attr,
	&cc_enabled_dummy_attr.attr,
	&cpus_offlined_attr.attr,
	NULL,
};

static __refdata struct attribute_group cc_attr_group = {
	.attrs = cc_attrs,
};

static __init int msm_thermal_add_cc_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *cc_kobj = NULL;
	int ret = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		ret = -ENOENT;
		goto done_cc_nodes;
	}

	cc_kobj = kobject_create_and_add("core_control", module_kobj);
	if (!cc_kobj) {
		pr_err("cannot create core control kobj\n");
		ret = -ENOMEM;
		goto done_cc_nodes;
	}

	ret = sysfs_create_group(cc_kobj, &cc_attr_group);
	if (ret) {
		pr_err("cannot create sysfs group. err:%d\n", ret);
		goto done_cc_nodes;
	}

	return 0;

done_cc_nodes:
	if (cc_kobj)
		kobject_del(cc_kobj);
	return ret;
}

int __devinit msm_thermal_init(struct msm_thermal_data *pdata)
{
	int ret = 0;
	uint32_t cpu;

	for_each_possible_cpu(cpu) {
		cpus[cpu].cpu = cpu;
		cpus[cpu].limited_max_freq = MSM_CPUFREQ_NO_LIMIT;
#ifdef CONFIG_MACH_LGE
		cpus[cpu].limited_min_freq = 300000;
#else
		cpus[cpu].limited_min_freq = 0;
#endif
	}

	BUG_ON(!pdata);
	tsens_get_max_sensor_num(&max_tsens_num);
	memcpy(&msm_thermal_info, pdata, sizeof(struct msm_thermal_data));

	if (create_sensor_id_map())
		return -EINVAL;
	if (check_sensor_id(msm_thermal_info_local.sensor_id))
		return -EINVAL;

	pr_info("%s: polling enabled!\n", KBUILD_MODNAME);

	if (num_possible_cpus() > 1)
		core_control = 1;

	ret = cpufreq_register_notifier(&msm_thermal_cpufreq_notifier,
			CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		pr_err("cannot register cpufreq notifier. err:%d\n", ret);

	INIT_DELAYED_WORK(&check_temp_work, check_temp);
	if (intelli_enabled)
		schedule_delayed_work(&check_temp_work,
				msecs_to_jiffies(10000));

	if (num_possible_cpus() > 1)
		register_cpu_notifier(&msm_thermal_cpu_notifier);

	return ret;
}

static int vdd_restriction_reg_init(struct platform_device *pdev)
{
	int ret = 0;
	int i;

	for (i = 0; i < rails_cnt; i++) {
		if (rails[i].freq_req == 1) {
			usefreq |= BIT(i);
			check_freq_table();
			/*
			 * Restrict frequency by default until we have made
			 * our first temp reading
			 */
			if (freq_table_get)
				ret = vdd_restriction_apply_freq(&rails[i], 0);
			else
				pr_info("Defer vdd rstr freq init.\n");
		} else {
			rails[i].reg = devm_regulator_get(&pdev->dev,
					rails[i].name);
			if (IS_ERR_OR_NULL(rails[i].reg)) {
				ret = PTR_ERR(rails[i].reg);
				if (ret != -EPROBE_DEFER) {
					pr_err( \
					"could not get regulator: %s. err:%d\n",
					rails[i].name, ret);
					rails[i].reg = NULL;
					rails[i].curr_level = -2;
					return ret;
				}
				pr_info("Defer regulator %s probe\n",
					rails[i].name);
				return ret;
			}
			/*
			 * Restrict votlage by default until we have made
			 * our first temp reading
			 */
			ret = vdd_restriction_apply_voltage(&rails[i], 0);
		}
	}

	return ret;
}

static int psm_reg_init(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	int j = 0;

	for (i = 0; i < psm_rails_cnt; i++) {
		psm_rails[i].reg = rpm_regulator_get(&pdev->dev,
				psm_rails[i].name);
		if (IS_ERR_OR_NULL(psm_rails[i].reg)) {
			ret = PTR_ERR(psm_rails[i].reg);
			if (ret != -EPROBE_DEFER) {
				pr_err("couldn't get rpm regulator %s. err%d\n",
					psm_rails[i].name, ret);
				psm_rails[i].reg = NULL;
				goto psm_reg_exit;
			}
			pr_info("Defer regulator %s probe\n",
					psm_rails[i].name);
			return ret;
		}
		/* Apps default vote for PWM mode */
		psm_rails[i].init = PMIC_PWM_MODE;
		ret = rpm_regulator_set_mode(psm_rails[i].reg,
				psm_rails[i].init);
		if (ret) {
			pr_err("Cannot set PMIC PWM mode. err:%d\n", ret);
			return ret;
		} else
			psm_rails[i].mode = PMIC_PWM_MODE;
	}

	return ret;

psm_reg_exit:
	if (ret) {
		for (j = 0; j < i; j++) {
			if (psm_rails[j].reg != NULL)
				rpm_regulator_put(psm_rails[j].reg);
		}
	}

	return ret;
}

static int msm_thermal_add_vdd_rstr_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *vdd_rstr_kobj = NULL;
	struct kobject *vdd_rstr_reg_kobj[MAX_RAILS] = {0};
	int rc = 0;
	int i = 0;

	if (!vdd_rstr_probed) {
		vdd_rstr_nodes_called = true;
		return rc;
	}

	if (vdd_rstr_probed && rails_cnt == 0)
		return rc;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		rc = -ENOENT;
		goto thermal_sysfs_add_exit;
	}

	vdd_rstr_kobj = kobject_create_and_add("vdd_restriction", module_kobj);
	if (!vdd_rstr_kobj) {
		pr_err("cannot create vdd_restriction kobject\n");
		rc = -ENOMEM;
		goto thermal_sysfs_add_exit;
	}

	rc = sysfs_create_group(vdd_rstr_kobj, &vdd_rstr_en_attribs_gp);
	if (rc) {
		pr_err("cannot create kobject attribute group. err:%d\n", rc);
		rc = -ENOMEM;
		goto thermal_sysfs_add_exit;
	}

	for (i = 0; i < rails_cnt; i++) {
		vdd_rstr_reg_kobj[i] = kobject_create_and_add(rails[i].name,
					vdd_rstr_kobj);
		if (!vdd_rstr_reg_kobj[i]) {
			pr_err("cannot create kobject for %s\n",
					rails[i].name);
			rc = -ENOMEM;
			goto thermal_sysfs_add_exit;
		}

		rails[i].attr_gp.attrs = kzalloc(
				sizeof(struct attribute *) * 3, GFP_KERNEL);
		if (!rails[i].attr_gp.attrs) {
			pr_err("kzalloc failed\n");
			rc = -ENOMEM;
			goto thermal_sysfs_add_exit;
		}

		VDD_RES_RW_ATTRIB(rails[i], rails[i].level_attr, 0, level);
		VDD_RES_RO_ATTRIB(rails[i], rails[i].value_attr, 1, value);
		rails[i].attr_gp.attrs[2] = NULL;

		rc = sysfs_create_group(vdd_rstr_reg_kobj[i],
				&rails[i].attr_gp);
		if (rc) {
			pr_err("cannot create attribute group for %s. err:%d\n",
					rails[i].name, rc);
			goto thermal_sysfs_add_exit;
		}
	}

	return rc;

thermal_sysfs_add_exit:
	if (rc) {
		for (i = 0; i < rails_cnt; i++) {
			kobject_del(vdd_rstr_reg_kobj[i]);
			kfree(rails[i].attr_gp.attrs);
		}
		if (vdd_rstr_kobj)
			kobject_del(vdd_rstr_kobj);
	}
	return rc;
}

static int msm_thermal_add_psm_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *psm_kobj = NULL;
	struct kobject *psm_reg_kobj[MAX_RAILS] = {0};
	int rc = 0;
	int i = 0;

	if (!psm_probed) {
		psm_nodes_called = true;
		return rc;
	}

	if (psm_probed && psm_rails_cnt == 0)
		return rc;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		rc = -ENOENT;
		goto psm_node_exit;
	}

	psm_kobj = kobject_create_and_add("pmic_sw_mode", module_kobj);
	if (!psm_kobj) {
		pr_err("cannot create psm kobject\n");
		rc = -ENOMEM;
		goto psm_node_exit;
	}

	for (i = 0; i < psm_rails_cnt; i++) {
		psm_reg_kobj[i] = kobject_create_and_add(psm_rails[i].name,
					psm_kobj);
		if (!psm_reg_kobj[i]) {
			pr_err("cannot create kobject for %s\n",
					psm_rails[i].name);
			rc = -ENOMEM;
			goto psm_node_exit;
		}
		psm_rails[i].attr_gp.attrs = kzalloc( \
				sizeof(struct attribute *) * 2, GFP_KERNEL);
		if (!psm_rails[i].attr_gp.attrs) {
			pr_err("kzalloc failed\n");
			rc = -ENOMEM;
			goto psm_node_exit;
		}

		PSM_RW_ATTRIB(psm_rails[i], psm_rails[i].mode_attr, 0, mode);
		psm_rails[i].attr_gp.attrs[1] = NULL;

		rc = sysfs_create_group(psm_reg_kobj[i],
				&psm_rails[i].attr_gp);
		if (rc) {
			pr_err("cannot create attribute group for %s. err:%d\n",
					psm_rails[i].name, rc);
			goto psm_node_exit;
		}
	}

	return rc;

psm_node_exit:
	if (rc) {
		for (i = 0; i < psm_rails_cnt; i++) {
			kobject_del(psm_reg_kobj[i]);
			kfree(psm_rails[i].attr_gp.attrs);
		}
		if (psm_kobj)
			kobject_del(psm_kobj);
	}
	return rc;
}

static int probe_vdd_rstr(struct device_node *node,
		struct msm_thermal_data *data, struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	int arr_size;
	char *key = NULL;
	struct device_node *child_node = NULL;

	rails = NULL;

	key = "qcom,vdd-restriction-temp";
	ret = of_property_read_u32(node, key, &data->vdd_rstr_temp_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,vdd-restriction-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->vdd_rstr_temp_hyst_degC);
	if (ret)
		goto read_node_fail;

	for_each_child_of_node(node, child_node) {
		rails_cnt++;
	}

	if (rails_cnt == 0)
		goto read_node_fail;
	if (rails_cnt >= MAX_RAILS) {
		pr_err("Too many rails:%d.\n", rails_cnt);
		return -EFAULT;
	}

	rails = kzalloc(sizeof(struct rail) * rails_cnt,
				GFP_KERNEL);
	if (!rails) {
		pr_err("Fail to allocate memory for rails.\n");
		return -ENOMEM;
	}

	i = 0;
	for_each_child_of_node(node, child_node) {
		key = "qcom,vdd-rstr-reg";
		ret = of_property_read_string(child_node, key, &rails[i].name);
		if (ret)
			goto read_node_fail;

		key = "qcom,levels";
		if (!of_get_property(child_node, key, &arr_size))
			goto read_node_fail;
		rails[i].num_levels = arr_size/sizeof(__be32);
		if (rails[i].num_levels >
			sizeof(rails[i].levels)/sizeof(uint32_t)) {
			pr_err("Array size:%d too large for index:%d\n",
				rails[i].num_levels, i);
			return -EFAULT;
		}
		ret = of_property_read_u32_array(child_node, key,
				rails[i].levels, rails[i].num_levels);
		if (ret)
			goto read_node_fail;

		key = "qcom,freq-req";
		rails[i].freq_req = of_property_read_bool(child_node, key);
		if (rails[i].freq_req)
			rails[i].min_level = 0;
		else {
			key = "qcom,min-level";
			ret = of_property_read_u32(child_node, key,
				&rails[i].min_level);
			if (ret)
				goto read_node_fail;
		}

		rails[i].curr_level = -1;
		rails[i].reg = NULL;
		i++;
	}

	if (rails_cnt) {
		ret = vdd_restriction_reg_init(pdev);
		if (ret) {
			pr_info("%s:Failed to get regulators. \
					KTM continues.\n", __func__);
			goto read_node_fail;
		}
		vdd_rstr_enabled = true;
	}
read_node_fail:
	vdd_rstr_probed = true;
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			__func__, node->full_name, key, ret);
		kfree(rails);
		rails_cnt = 0;
	}
	if (ret == -EPROBE_DEFER)
		vdd_rstr_probed = false;
	return ret;
}

static int probe_psm(struct device_node *node, struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	int ret = 0;
	int j = 0;
	char *key = NULL;

	psm_rails = NULL;

	key = "qcom,pmic-sw-mode-temp";
	ret = of_property_read_u32(node, key, &data->psm_temp_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,pmic-sw-mode-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->psm_temp_hyst_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,pmic-sw-mode-regs";
	psm_rails_cnt = of_property_count_strings(node, key);
	psm_rails = kzalloc(sizeof(struct psm_rail) * psm_rails_cnt,
			GFP_KERNEL);
	if (!psm_rails) {
		pr_err("Fail to allocate memory for psm rails\n");
		psm_rails_cnt = 0;
		return -ENOMEM;
	}

	for (j = 0; j < psm_rails_cnt; j++) {
		ret = of_property_read_string_index(node, key, j,
				&psm_rails[j].name);
		if (ret)
			goto read_node_fail;
	}

	if (psm_rails_cnt) {
		ret = psm_reg_init(pdev);
		if (ret) {
			pr_err("Err regulator init. err:%d. KTM continues.\n",
					ret);
			goto read_node_fail;
		}
		psm_enabled = true;
	}

read_node_fail:
	psm_probed = true;
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			__func__, node->full_name, key, ret);
		kfree(psm_rails);
		psm_rails_cnt = 0;
	}
	if (ret == -EPROBE_DEFER)
		psm_probed = false;
	return ret;
}

static int __devinit msm_thermal_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
	char *key = NULL;
	struct device_node *node = pdev->dev.of_node;
	struct msm_thermal_data data;

	pr_info("%s: msm_thermal_dev_probe begin...\n", KBUILD_MODNAME);

	memset(&data, 0, sizeof(struct msm_thermal_data));

	/*
	 * Probe optional properties below. Call probe_psm before
	 * probe_vdd_rstr because rpm_regulator_get has to be called
	 * before devm_regulator_get
	 */
	ret = probe_psm(node, &data, pdev);
	if (ret == -EPROBE_DEFER)
		goto fail;
	ret = probe_vdd_rstr(node, &data, pdev);
	if (ret == -EPROBE_DEFER)
		goto fail;

	/*
	 * In case sysfs add nodes get called before probe function.
	 * Need to make sure sysfs node is created again
	 */
	if (psm_nodes_called) {
		msm_thermal_add_psm_nodes();
		psm_nodes_called = false;
	}
	if (vdd_rstr_nodes_called) {
		msm_thermal_add_vdd_rstr_nodes();
		vdd_rstr_nodes_called = false;
	}
	ret = msm_thermal_init(&data);

	pr_info("%s: msm_thermal_dev_probe completed!\n", KBUILD_MODNAME);

	return ret;
fail:
	if (ret)
		pr_err("Failed reading node=%s, key=%s. err:%d\n",
			node->full_name, key, ret);

	pr_info("%s: msm_thermal_dev_probe failed!\n", KBUILD_MODNAME);

	return ret;
}

static int msm_thermal_dev_exit(struct platform_device *inp_dev)
{
	pr_info("msm_thermal_dev: removed!\n");
	return 0;
}

static struct of_device_id msm_thermal_match_table[] = {
	{.compatible = "qcom,msm-thermal"},
	{},
};

static struct platform_driver msm_thermal_device_driver = {
	.probe = msm_thermal_dev_probe,
	.driver = {
		.name = "msm-thermal",
		.owner = THIS_MODULE,
		.of_match_table = msm_thermal_match_table,
	},
	.remove = msm_thermal_dev_exit,
};

int __init msm_thermal_device_init(void)
{
	return platform_driver_register(&msm_thermal_device_driver);
}

int __init msm_thermal_late_init(void)
{
	if (num_possible_cpus() > 1)
		msm_thermal_add_cc_nodes();
	msm_thermal_add_psm_nodes();
	msm_thermal_add_vdd_rstr_nodes();

#ifdef CONFIG_INTELLI_THERMAL_STATS
	msm_thermal_add_stat_nodes();
#endif
	return 0;
}
late_initcall(msm_thermal_late_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>, Yuri Sh. <yurboss@gmail.com>, \
		Praveen Chidambaram <pchidamb@codeaurora.org>");
MODULE_DESCRIPTION("intelligent thermal driver for Qualcomm based SOCs");
MODULE_DESCRIPTION("originally from Qualcomm's open source repo");
