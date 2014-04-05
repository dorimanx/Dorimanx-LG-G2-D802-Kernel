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

#ifndef _LINUX_ZWAIT_H
#define _LINUX_ZWAIT_H

#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/power_supply.h>
#include <linux/rtc.h>
#include <linux/suspend.h>

/* zero wait event */
enum {
	ZW_EVENT_NONE = 0,
	ZW_EVENT_PWRKEY,
	ZW_EVENT_CHARGER,
	ZW_EVENT_MAX
};

/* irq type */
enum {
	ZW_PWRKEY_UNITE_IRQ = 0,
	ZW_PWRKEY_SEPERATE_IRQ,
	ZW_PWRKEY_IRQ_TYPE_MAX
};

/* zero wait state */
enum {
	ZW_STATE_OFF = 0,
	ZW_STATE_ON_SYSTEM,
	ZW_STATE_ON_USER,
	ZW_STATE_MAX
};

struct zw_pwrkey_unite_irq {
	int irq_type;			/* irq type */
	struct input_dev *input;	/* input device */
	struct device *wdev;		/* wakeup device */
	unsigned int code;		/* input code */

	int state_irq;
	irq_handler_t state_irq_handler;
	int (*check_func)(void *param);	/* key press check func */
	void *func_param;		/* parameter of check func */

	/*
	 * NOTE: use belows if need be
	 */
	int bark_irq;
};

struct zw_pwrkey_seperate_irq {
	int irq_type;			/* irq type */
	struct input_dev *input;	/* input device */
	struct device *wdev;		/* wakeup device */
	unsigned int code;		/* input code */

	int press_irq;
	int release_irq;
	irq_handler_t press_irq_handler;
	irq_handler_t release_irq_handler;
};

union zw_pwrkey_info {
	void *ptr;
	struct {
		int irq_type;		 /* irq type */
		struct input_dev *input; /* input device */
		struct device *wdev;	 /* wakeup device */
		unsigned int code;	 /* input code */
	} *data;
	struct zw_pwrkey_unite_irq *u;
	struct zw_pwrkey_seperate_irq *s;
};

#define __INIT_ZW_PWRKEY_UNITE_IRQ { \
	.irq_type = ZW_PWRKEY_UNITE_IRQ, \
	.state_irq = -1, \
	.bark_irq = -1, \
}

#define __INIT_ZW_PWRKEY_SEPERATE_IRQ { \
	.irq_type = ZW_PWRKEY_SEPERATE_IRQ, \
	.press_irq = -1, \
	.release_irq = -1, \
}

#define __DEFINE_ZW_PWRKEY_UNITE_IRQ_INFO(_name) \
	struct zw_pwrkey_unite_irq _name = __INIT_ZW_PWRKEY_UNITE_IRQ

#define __DEFINE_ZW_PWRKEY_SEPERATE_IRQ_INFO(_name) \
	struct zw_pwrkey_seperate_irq _name = __INIT_ZW_PWRKEY_SEPERATE_IRQ

#define DEFINE_ZW_PWRKEY_INFO(_name, _type) \
	__DEFINE_##_type##_INFO(_name)


#ifdef CONFIG_ZERO_WAIT

extern int zw_notifier_chain_register(struct notifier_block *nb, void *ptr);
extern int zw_notifier_chain_unregister(struct notifier_block *nb);
extern int is_zw_mode(void);

/* pwrkey */
extern void zw_pwrkey_info_register(void *ptr);
extern void zw_pwrkey_info_unregister(void);

/* power supply */
extern int zw_psy_wakeup_source_register(struct wakeup_source *ws);
extern void zw_psy_wakeup_source_unregister(struct wakeup_source *ws);
extern int zw_power_supply_register(struct power_supply *psy);
extern void zw_power_supply_unregister(struct power_supply *psy);
extern void zw_psy_irq_handler(int attach);

/* power management */
extern void zw_queue_up_suspend_work(suspend_state_t state);

/* led */
extern int zw_led_register(struct led_classdev *cdev);
extern void zw_led_unregister(struct led_classdev *cdev);
extern int zw_no_charger_in_zwait(void);

/* rtc */
extern int zw_rtc_info_register(struct rtc_device *rtc);
extern void zw_rtc_info_unregister(struct rtc_device *rtc);

#else /* !CONFIG_ZERO_WAIT */

static inline void zw_suspend_state_set(void)
{
	return;
}
static inline int zw_notifier_chain_register(struct notifier_block *nb)
{
	return 0;
}
static inline int zw_notifier_chain_unregister(struct notifier_block *nb)
{
	return 0;
}
static inline int is_zw_mode(void)
{
	return 0;
}

/* pwrkey */
static inline void zw_pwrkey_info_register(void *ptr)
{
	return;
}
static inline void zw_pwrkey_info_unregister(void)
{
	return;
}

/* power supply */
static inline int zw_psy_wakeup_source_register(struct wakeup_source *ws)
{
	return 0;
}
static inline void zw_psy_wakeup_source_unregister(struct wakeup_source *ws)
{
	return;
}
static inline int zw_power_supply_register(struct power_supply *psy)
{
	return 0;
}
static inline void zw_power_supply_unregister(struct power_supply *psy)
{
	return;
}
static inline void zw_psy_irq_handler(int attach)
{
	return;
}

/* power management */
static inline void zw_queue_up_suspend_work(suspend_state_t state)
{
	return;
}

/* led */
static inline int zw_led_register(struct led_classdev *cdev)
{
	return 0;
}
static inline void zw_led_unregister(struct led_classdev *cdev)
{
	return;
}
static inline int zw_no_charger_in_zwait(void)
{
	return 0;
}

/* rtc */
static inline int zw_rtc_info_register(struct rtc_device *rtc)
{
	return 0;
}
static inline void zw_rtc_info_unregister(struct rtc_device *rtc)
{
	return;
}
#endif

#if defined(CONFIG_ZERO_WAIT) && defined(CONFIG_SWITCH)
extern void zw_event_report(int event);
#else
static inline void zw_event_report(int event)
{
	return;
}
#endif

#endif	/* _LINUX_ZWAIT_H */
