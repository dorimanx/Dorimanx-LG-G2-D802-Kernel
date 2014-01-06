/* linux/drivers/usb/gadget/u_lgeusb.h
 *
 * Copyright (C) 2011, 2012 LG Electronics Inc.
 * Author : Hyeon H. Park <hyunhui.park@lge.com>
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

#ifndef __U_LGEUSB_H__
#define __U_LGEUSB_H__

enum lgeusb_mode {
	LGEUSB_FACTORY_MODE = 0,
	LGEUSB_ANDROID_MODE,
	LGEUSB_DEFAULT_MODE,
};

#ifdef CONFIG_USB_G_LGE_ANDROID_AUTORUN
int lgeusb_get_autorun_user_mode(void);
int lgeusb_get_model_name(char *);
int lgeusb_get_phone_id(char *);
int lgeusb_get_sw_ver(char *);
int lgeusb_get_sub_ver(char *);

int debug_pmic_register_for_usb(void);
#endif

#endif /* __U_LGEUSB_H__ */
