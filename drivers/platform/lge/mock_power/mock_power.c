/*
 * Mock power driver for testing
 *
 * Copyright (C) 2013 LG Electronics Inc.
 *
 * reference: drivers/power/test_power.c
 *
 * Dynamic module parameter code from the Virtual Battery Driver
 * Copyright (C) 2008 Pylone, Inc.
 * By: Masashi YOKOTA <yokota@pylone.jp>
 * Originally found here:
 * http://downloads.pylone.jp/src/virtual_battery/virtual_battery-0.0.1.tar.bz2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/vermagic.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#define MOCK_POWER_STATE_DISABLE	0
#define MOCK_POWER_STATE_ENABLE		1

enum {
	MOCK_POWER_SUPPLY_AC = 0,
	MOCK_POWER_SUPPLY_USB,
#ifdef CONFIG_WIRELESS_CHARGER
	MOCK_POWER_SUPPLY_WIRELESS,
#endif	/* CONFIG_WIRELESS_CHARGER */
	MOCK_POWER_SUPPLY_BATTERY,
	MOCK_POWER_SUPPLY_MAX
};

struct target_power {
	struct list_head entry;
	struct power_supply *psy;
	size_t backup_num_properties;
	enum power_supply_property *backup_properties;
	int (*backup_get_property)(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val);
};

struct mock_power {
	size_t num_properties;
	enum power_supply_property *properties;
	int (*get_property)(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val);
};

struct mock_property_map {
	int value;
	char const *key;
};


static void battery_capacity_delayed_work_func(struct work_struct *work);
static DECLARE_DELAYED_WORK(battcap_delayed_work, battery_capacity_delayed_work_func);

static struct mock_power mock[MOCK_POWER_SUPPLY_MAX];

static DEFINE_MUTEX(tp_list_mtx);

static struct list_head tp_list[MOCK_POWER_SUPPLY_MAX] = {
	[MOCK_POWER_SUPPLY_AC] =
		LIST_HEAD_INIT(tp_list[MOCK_POWER_SUPPLY_AC]),
	[MOCK_POWER_SUPPLY_USB] =
		LIST_HEAD_INIT(tp_list[MOCK_POWER_SUPPLY_USB]),
#ifdef CONFIG_WIRELESS_CHARGER
	[MOCK_POWER_SUPPLY_WIRELESS] =
		LIST_HEAD_INIT(tp_list[MOCK_POWER_SUPPLY_WIRELESS]),
#endif	/* CONFIG_WIRELESS_CHARGER */
	[MOCK_POWER_SUPPLY_BATTERY] =
		LIST_HEAD_INIT(tp_list[MOCK_POWER_SUPPLY_BATTERY])
};

static enum power_supply_property mock_power_main_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mock_power_battery_props[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	POWER_SUPPLY_PROP_BATTERY_ID_CHECK
#endif
};

static int ac_online = 0;
static int usb_online = 0;
#ifdef CONFIG_WIRELESS_CHARGER
static int wireless_online = 0;
static int keep_wireless = 0;
#endif	/* CONFIG_WIRELESS_CHARGER */
static int keep_ac = 0;
static int keep_usb = 0;
static int keep_battery = 0;
static int battery_status = POWER_SUPPLY_STATUS_DISCHARGING;
static int battery_health = POWER_SUPPLY_HEALTH_GOOD;
static int battery_technology = POWER_SUPPLY_TECHNOLOGY_LION;
static int battery_present = 1;			/* true */
static int battery_voltage = 3600;		/* millivolts */
static int battery_capacity = 50;		/* percentage */
static int battery_capacity_delayed = 0;	/* percentage, delay in msec */
static int mock_power = MOCK_POWER_STATE_DISABLE;
static unsigned long target_jiffies = 0;
static int battcap_delayed_done = 1;

static int *keep_ptr[MOCK_POWER_SUPPLY_MAX] = {
	[MOCK_POWER_SUPPLY_AC]       = &keep_ac,
	[MOCK_POWER_SUPPLY_USB]      = &keep_usb,
#ifdef CONFIG_WIRELESS_CHARGER
	[MOCK_POWER_SUPPLY_WIRELESS] = &keep_wireless,
#endif	/* CONFIG_WIRELESS_CHARGER */
	[MOCK_POWER_SUPPLY_BATTERY]  = &keep_battery
};

static void keep_target_power_supply(int keep, int idx);
static void enable_mock_power_supplies(void);
static void disable_mock_power_supplies(void);

static struct mock_property_map map_online[] = {
	{ 0, "off" },
	{ 1, "on"  },
	{ -1, NULL}
};

static struct mock_property_map map_status[] = {
	{ POWER_SUPPLY_STATUS_CHARGING,     "charging"     },
	{ POWER_SUPPLY_STATUS_DISCHARGING,  "discharging"  },
	{ POWER_SUPPLY_STATUS_NOT_CHARGING, "not-charging" },
	{ POWER_SUPPLY_STATUS_FULL,         "full"         },
	{ -1, NULL }
};

static struct mock_property_map map_health[] = {
	{ POWER_SUPPLY_HEALTH_GOOD,           "good"        },
	{ POWER_SUPPLY_HEALTH_OVERHEAT,       "overheat"    },
	{ POWER_SUPPLY_HEALTH_DEAD,           "dead"        },
	{ POWER_SUPPLY_HEALTH_OVERVOLTAGE,    "overvoltage" },
	{ POWER_SUPPLY_HEALTH_UNSPEC_FAILURE, "failure"     },
	{ -1, NULL }
};

static struct mock_property_map map_technology[] = {
	{ POWER_SUPPLY_TECHNOLOGY_NiMH, "NiMH" },
	{ POWER_SUPPLY_TECHNOLOGY_LION, "LION" },
	{ POWER_SUPPLY_TECHNOLOGY_LIPO, "LIPO" },
	{ POWER_SUPPLY_TECHNOLOGY_LiFe, "LiFe" },
	{ POWER_SUPPLY_TECHNOLOGY_NiCd, "NiCd" },
	{ POWER_SUPPLY_TECHNOLOGY_LiMn, "LiMn" },
	{ -1, NULL }
};

static struct mock_property_map map_present[] = {
	{ 0, "false" },
	{ 1, "true"  },
	{ -1, NULL }
};

static struct mock_property_map map_mock_state[] = {
	{ 0, "disable" },
	{ 1, "enable"  },
	{ -1, NULL }
};

static int map_get_value(struct mock_property_map *map, const char *key,
				int def_val)
{
	char *p;
	int len;
	p = memchr(key, '\n', 256);
	len = p ? p - key : 256;

	while (map->key) {
		if (strncasecmp(map->key, key, len) == 0)
			return map->value;
		map++;
	}

	return def_val;
}

static const char *map_get_key(struct mock_property_map *map, int value,
				const char *def_key)
{
	while (map->key) {
		if (map->value == value)
			return map->key;
		map++;
	}

	return def_key;
}

static inline void mock_power_supply_changed(int idx)
{
	struct target_power *target;

	mutex_lock(&tp_list_mtx);
	list_for_each_entry(target, &tp_list[idx], entry) {
		if (target->psy != NULL)
			power_supply_changed(target->psy);
	}
	mutex_unlock(&tp_list_mtx);
}

static void battery_capacity_delayed_work_func(struct work_struct *work)
{
	if (mock_power) {
		battery_capacity = battery_capacity_delayed;
		mock_power_supply_changed(MOCK_POWER_SUPPLY_BATTERY);
	}

	battcap_delayed_done = 1;
}

static int param_set_ac_online(const char *key, const struct kernel_param *kp)
{
	ac_online = map_get_value(map_online, key, ac_online);
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_AC);
	return 0;
}

static int param_get_ac_online(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_online, ac_online, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_ac_online = {
	.set = param_set_ac_online,
	.get = param_get_ac_online,
};

#define param_check_ac_online(name, p)		__param_check(name, p, void);
module_param(ac_online, ac_online, 0644);
MODULE_PARM_DESC(ac_online, "AC charging state <on|off>");

static int param_set_usb_online(const char *key, const struct kernel_param *kp)
{
	usb_online = map_get_value(map_online, key, usb_online);
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_USB);
	return 0;
}

static int param_get_usb_online(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_online, usb_online, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_usb_online = {
	.set = param_set_usb_online,
	.get = param_get_usb_online,
};

#define param_check_usb_online(name, p)		__param_check(name, p, void);
module_param(usb_online, usb_online, 0644);
MODULE_PARM_DESC(usb_online, "USB charging state <on|off>");

#ifdef CONFIG_WIRELESS_CHARGER
static int param_set_wireless_online(const char *key, const struct kernel_param *kp)
{
	wireless_online = map_get_value(map_online, key, wireless_online);
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_WIRELESS);
	return 0;
}

static int param_get_wireless_online(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_online, wireless_online, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_wireless_online = {
	.set = param_set_wireless_online,
	.get = param_get_wireless_online,
};

#define param_check_wireless_online(name, p)	__param_check(name, p, void);
module_param(wireless_online, wireless_online, 0644);
MODULE_PARM_DESC(wireless_online, "Wireless charging state <on|off>");

static int param_set_keep_wireless(const char *key, const struct kernel_param *kp)
{
	int tmp;

	if (sscanf(key, "%d", &tmp) != 1)
		return -EINVAL;

	if ((tmp != 0) && (tmp != 1)) {
		return -EINVAL;
	}

	keep_target_power_supply(tmp, MOCK_POWER_SUPPLY_WIRELESS);

	return 0;
}

static struct kernel_param_ops param_ops_keep_wireless = {
	.set = param_set_keep_wireless,
	.get = param_get_int,
};

#define param_check_keep_wireless(name, p)	__param_check(name, p, void);
module_param(keep_wireless, keep_wireless, 0644);
MODULE_PARM_DESC(keep_wireless, "Keep Wireless charger <0|1>");
#endif	/* CONFIG_WIRELESS_CHARGER */

static int param_set_keep_ac(const char *key, const struct kernel_param *kp)
{
	int tmp;

	if (sscanf(key, "%d", &tmp) != 1)
		return -EINVAL;

	if ((tmp != 0) && (tmp != 1)) {
		return -EINVAL;
	}

	keep_target_power_supply(tmp, MOCK_POWER_SUPPLY_AC);

	return 0;
}

static struct kernel_param_ops param_ops_keep_ac = {
	.set = param_set_keep_ac,
	.get = param_get_int,
};

#define param_check_keep_ac(name, p)	__param_check(name, p, void);
module_param(keep_ac, keep_ac, 0644);
MODULE_PARM_DESC(keep_ac, "Keep AC charger <0|1>");

static int param_set_keep_usb(const char *key, const struct kernel_param *kp)
{
	int tmp;

	if (sscanf(key, "%d", &tmp) != 1)
		return -EINVAL;

	if ((tmp != 0) && (tmp != 1)) {
		return -EINVAL;
	}

	keep_target_power_supply(tmp, MOCK_POWER_SUPPLY_USB);

	return 0;
}

static struct kernel_param_ops param_ops_keep_usb = {
	.set = param_set_keep_usb,
	.get = param_get_int,
};

#define param_check_keep_usb(name, p)	__param_check(name, p, void);
module_param(keep_usb, keep_usb, 0644);
MODULE_PARM_DESC(keep_usb, "Keep USB charger <0|1>");

static int param_set_keep_battery(const char *key, const struct kernel_param *kp)
{
	int tmp;

	if (sscanf(key, "%d", &tmp) != 1)
		return -EINVAL;

	if ((tmp != 0) && (tmp != 1)) {
		return -EINVAL;
	}

	keep_target_power_supply(tmp, MOCK_POWER_SUPPLY_BATTERY);

	return 0;
}

static struct kernel_param_ops param_ops_keep_battery = {
	.set = param_set_keep_battery,
	.get = param_get_int,
};

#define param_check_keep_battery(name, p)	__param_check(name, p, void);
module_param(keep_battery, keep_battery, 0644);
MODULE_PARM_DESC(keep_battery, "Keep Battery <0|1>");

static int param_set_battery_status(const char *key, const struct kernel_param *kp)
{
	battery_status = map_get_value(map_status, key, battery_status);
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_BATTERY);
	return 0;
}

static int param_get_battery_status(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_status, battery_status, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_battery_status = {
	.set = param_set_battery_status,
	.get = param_get_battery_status,
};

#define param_check_battery_status(name, p)	__param_check(name, p, void);
module_param(battery_status, battery_status, 0644);
MODULE_PARM_DESC(battery_status,
	"battery status <charging|discharging|not-charging|full>");

static int param_set_battery_health(const char *key, const struct kernel_param *kp)
{
	battery_health = map_get_value(map_health, key, battery_health);
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_BATTERY);
	return 0;
}

static int param_get_battery_health(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_health, battery_health, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_battery_health = {
	.set = param_set_battery_health,
	.get = param_get_battery_health,
};

#define param_check_battery_health(name, p)	__param_check(name, p, void);
module_param(battery_health, battery_health, 0644);
MODULE_PARM_DESC(battery_health,
	"battery health state <good|overheat|dead|overvoltage|failure>");

static int param_set_battery_technology(const char *key, const struct kernel_param *kp)
{
	battery_technology = map_get_value(map_technology, key, battery_technology);
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_BATTERY);
	return 0;
}

static int param_get_battery_technology(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_technology, battery_technology, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_battery_technology = {
	.set = param_set_battery_technology,
	.get = param_get_battery_technology,
};

#define param_check_battery_technology(name, p)	__param_check(name, p, void);
module_param(battery_technology, battery_technology, 0644);
MODULE_PARM_DESC(battery_technology,
	"battery technology <NiMH|LION|LIPO|LiFe|NiCd|LiMn>");

static int param_set_battery_present(const char *key, const struct kernel_param *kp)
{
	battery_present = map_get_value(map_present, key, battery_present);
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_BATTERY);
	return 0;
}

static int param_get_battery_present(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_present, battery_present, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_battery_present = {
	.set = param_set_battery_present,
	.get = param_get_battery_present,
};

#define param_check_battery_present(name, p)	__param_check(name, p, void);
module_param(battery_present, battery_present, 0644);
MODULE_PARM_DESC(battery_present,
	"battery presence state <good|overheat|dead|overvoltage|failure>");

static int param_set_battery_voltage(const char *key, const struct kernel_param *kp)
{
	int tmp;

	if (sscanf(key, "%d", &tmp) != 1)
		return -EINVAL;

	battery_voltage = tmp;
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_BATTERY);
	return 0;
}

static struct kernel_param_ops param_ops_battery_voltage = {
	.set = param_set_battery_voltage,
	.get = param_get_int,
};

#define param_check_battery_voltage(name, p)	__param_check(name, p, void);
module_param(battery_voltage, battery_voltage, 0644);
MODULE_PARM_DESC(battery_voltage, "battery voltage (millivolts)");

static int param_set_battery_capacity(const char *key, const struct kernel_param *kp)
{
	int tmp;

	if (sscanf(key, "%d", &tmp) != 1)
		return -EINVAL;

	battery_capacity = tmp;
	if (mock_power)
		mock_power_supply_changed(MOCK_POWER_SUPPLY_BATTERY);
	return 0;
}

static struct kernel_param_ops param_ops_battery_capacity = {
	.set = param_set_battery_capacity,
	.get = param_get_int,
};

#define param_check_battery_capacity(name, p)	__param_check(name, p, void);
module_param(battery_capacity, battery_capacity, 0644);
MODULE_PARM_DESC(battery_capacity, "battery capacity (percentage)");

static int param_set_battery_capacity_delayed(const char *key,
					const struct kernel_param *kp)
{
	int capacity;
	unsigned int delay;	/* in msec */

	if (sscanf(key, "%d %u", &capacity, &delay) != 2)
		return -EINVAL;

	cancel_delayed_work_sync(&battcap_delayed_work);

	battery_capacity_delayed = capacity;
	target_jiffies = msecs_to_jiffies(delay) + jiffies;

	schedule_delayed_work(&battcap_delayed_work, msecs_to_jiffies(delay));
	battcap_delayed_done = 0;

	return 0;
}

static int param_get_battery_capacity_delayed(char *buf,
					const struct kernel_param *kp)
{
	unsigned int delay;	/* in msec */
	unsigned long current_jiffies;

	if (battcap_delayed_done) {
		delay = 0;
	} else {
		current_jiffies = jiffies;

		if (time_after_eq(current_jiffies, target_jiffies)) {
			delay = 0;
		} else {
			delay = jiffies_to_msecs(target_jiffies - current_jiffies);
		}
	}

	sprintf(buf, "%d%s, %u%s", battery_capacity_delayed, "%", delay, "msec");

	return strlen(buf);
}

static struct kernel_param_ops param_ops_battery_capacity_delayed = {
	.set = param_set_battery_capacity_delayed,
	.get = param_get_battery_capacity_delayed,
};

#define param_check_battery_capacity_delayed(name, p) \
	__param_check(name, p, void);
module_param(battery_capacity_delayed, battery_capacity_delayed, 0644);
MODULE_PARM_DESC(battery_capacity_delayed,
	"battery capacity delayed <percentage> <msec>");

static int param_set_mock_power(const char *key, const struct kernel_param *kp)
{
	int tmp;

	tmp = map_get_value(map_mock_state, key, -1);

	switch (tmp) {
	case MOCK_POWER_STATE_ENABLE:
		mock_power = tmp;
		enable_mock_power_supplies();
		break;

	case MOCK_POWER_STATE_DISABLE:
		mock_power = tmp;
		disable_mock_power_supplies();
		break;

	default:
		pr_info("mock_power <enable|disable>\n");
		pr_info(" - enable  : echo enable > mock_power\n");
		pr_info(" - disable : echo disable > mock_power\n");
		return 0;
	}

	return 0;
}

static int param_get_mock_power(char *buf, const struct kernel_param *kp)
{
	strcpy(buf, map_get_key(map_mock_state, mock_power, "unknown\n"));
	return strlen(buf);
}

static struct kernel_param_ops param_ops_mock_power = {
	.set = param_set_mock_power,
	.get = param_get_mock_power,
};

#define param_check_mock_power(name, p)		__param_check(name, p, void);
module_param(mock_power, mock_power, 0644);
MODULE_PARM_DESC(mock_power, "mock power state <disable|enable>");

static inline void __enable_mock_power_supply(int idx)
{
	struct target_power *target;

	mutex_lock(&tp_list_mtx);
	list_for_each_entry(target, &tp_list[idx], entry) {
		if (target->psy != NULL) {
			target->backup_get_property = target->psy->get_property;
			target->backup_properties = target->psy->properties;
			target->backup_num_properties = target->psy->num_properties;

			target->psy->get_property = mock[idx].get_property;
			target->psy->properties = mock[idx].properties;
			target->psy->num_properties = mock[idx].num_properties;
		}
	}
	mutex_unlock(&tp_list_mtx);
}

static inline void __disable_mock_power_supply(int idx)
{
	struct target_power *target;

	mutex_lock(&tp_list_mtx);
	list_for_each_entry(target, &tp_list[idx], entry) {
		if (target->psy != NULL) {
			target->psy->get_property = target->backup_get_property;
			target->psy->properties = target->backup_properties;
			target->psy->num_properties = target->backup_num_properties;

			target->backup_get_property = NULL;
			target->backup_properties = NULL;
			target->backup_num_properties = 0;
		}
	}
	mutex_unlock(&tp_list_mtx);
}

static void keep_target_power_supply(int keep, int idx)
{
	if (*keep_ptr[idx] == keep)
		return;

	*keep_ptr[idx] = keep;

	if (mock_power) {
		if (keep) {
			__disable_mock_power_supply(idx);
		} else {
			__enable_mock_power_supply(idx);
		}
		mock_power_supply_changed(idx);
	}
}

static void enable_mock_power_supplies(void)
{
	int i;

	for (i = 0; i < MOCK_POWER_SUPPLY_MAX; i++) {
		if (*keep_ptr[i] == 0)
			__enable_mock_power_supply(i);
	}

	for (i = 0; i < MOCK_POWER_SUPPLY_MAX; i++) {
		mock_power_supply_changed(i);
	}
}

static void disable_mock_power_supplies(void)
{
	int i;

	for (i = 0; i < MOCK_POWER_SUPPLY_MAX; i++) {
		if (*keep_ptr[i] == 0)
			__disable_mock_power_supply(i);
	}

	for (i = 0; i < MOCK_POWER_SUPPLY_MAX; i++) {
		mock_power_supply_changed(i);
	}
}

static inline int type_to_index(enum power_supply_type type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_MAINS:
		return MOCK_POWER_SUPPLY_AC;

	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_ACA:
		return MOCK_POWER_SUPPLY_USB;

	case POWER_SUPPLY_TYPE_BATTERY:
		return MOCK_POWER_SUPPLY_BATTERY;

#ifdef CONFIG_WIRELESS_CHARGER
	case POWER_SUPPLY_TYPE_WIRELESS:
		return MOCK_POWER_SUPPLY_WIRELESS;
#endif	/* CONFIG_WIRELESS_CHARGER */

	default:
		pr_info("%s: type(%d) is not supported!\n", __func__, type);
		return -1;
	}
}

int mock_power_supply_register(struct power_supply *psy)
{
	int idx;
	struct target_power *target;

	if (psy == NULL)
		return -EINVAL;

	idx = type_to_index(psy->type);
	if (idx < 0)
		return 0;

	target = kzalloc(sizeof(struct target_power), GFP_KERNEL);
	if (target == NULL)
		return -ENOMEM;

	target->psy = psy;
	mutex_lock(&tp_list_mtx);
	list_add_tail(&target->entry, &tp_list[idx]);
	mutex_unlock(&tp_list_mtx);

	return 0;
}

void mock_power_supply_unregister(struct power_supply *psy)
{
	int idx;
	struct target_power *target;

	if (psy == NULL)
		return;

	idx = type_to_index(psy->type);
	if (idx < 0)
		return;

	mutex_lock(&tp_list_mtx);
	list_for_each_entry(target, &tp_list[idx], entry) {
		if (target->psy == psy) {
			target->psy = NULL;
			list_del(&target->entry);
			kfree(target);
			break;
		}
	}
	mutex_unlock(&tp_list_mtx);
}

static inline int get_mock_power_main_property(enum power_supply_property psp,
						union power_supply_propval *val,
						int online)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = online;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int get_mock_power_ac_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	return get_mock_power_main_property(psp, val, ac_online);
}

static int get_mock_power_usb_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	return get_mock_power_main_property(psp, val, usb_online);
}

#ifdef CONFIG_WIRELESS_CHARGER
static int get_mock_power_wireless_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	return get_mock_power_main_property(psp, val, wireless_online);
}
#endif	/* CONFIG_WIRELESS_CHARGER */

static int get_mock_power_battery_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "Mock Battery";
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Linux";
		break;

	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = UTS_RELEASE;
		break;

	case POWER_SUPPLY_PROP_STATUS:
		val->intval = battery_status;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = battery_health;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = battery_technology;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battery_present;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = battery_capacity;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = 26;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_voltage;
		break;

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	case POWER_SUPPLY_PROP_BATTERY_ID_CHECK:
		val->intval = 1;
		break;
#endif

	default:
		return -EINVAL;
	}
	return 0;
}

static int __init mock_power_supply_init(void)
{
	mock[MOCK_POWER_SUPPLY_AC].get_property = get_mock_power_ac_property;
	mock[MOCK_POWER_SUPPLY_AC].properties = mock_power_main_props;
	mock[MOCK_POWER_SUPPLY_AC].num_properties = ARRAY_SIZE(mock_power_main_props);

	mock[MOCK_POWER_SUPPLY_USB].get_property = get_mock_power_usb_property;
	mock[MOCK_POWER_SUPPLY_USB].properties = mock_power_main_props;
	mock[MOCK_POWER_SUPPLY_USB].num_properties = ARRAY_SIZE(mock_power_main_props);

#ifdef CONFIG_WIRELESS_CHARGER
	mock[MOCK_POWER_SUPPLY_WIRELESS].get_property = get_mock_power_wireless_property;
	mock[MOCK_POWER_SUPPLY_WIRELESS].properties = mock_power_main_props;
	mock[MOCK_POWER_SUPPLY_WIRELESS].num_properties = ARRAY_SIZE(mock_power_main_props);
#endif	/* CONFIG_WIRELESS_CHARGER */

	mock[MOCK_POWER_SUPPLY_BATTERY].get_property = get_mock_power_battery_property;
	mock[MOCK_POWER_SUPPLY_BATTERY].properties = mock_power_battery_props;
	mock[MOCK_POWER_SUPPLY_BATTERY].num_properties = ARRAY_SIZE(mock_power_battery_props);

#ifdef CONFIG_MOCK_POWER_DEBUGFS
	debugfs_create_symlink("mock_power", NULL,
		"/sys/module/mock_power/parameters");
#endif

	return 0;
}

static void __exit mock_power_supply_exit(void)
{
	int i;

	for (i = 0; i < MOCK_POWER_SUPPLY_MAX; i++) {
		mock[i].get_property = NULL;
		mock[i].properties = NULL;
		mock[i].num_properties = 0;
	}

	return;
}

module_init(mock_power_supply_init);
module_exit(mock_power_supply_exit);

MODULE_DESCRIPTION("Mock power driver for testing");
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_LICENSE("GPL");
