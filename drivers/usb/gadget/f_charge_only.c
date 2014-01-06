/*
 * Gadget Driver for LGE Android charge only feature.
 *
 * Copyright (C) 2011 LG Electronics Inc.
 * Author: Hyeon H. Park <hyunhui.park@lge.com>
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

/* This function driver is implemented for charge only feature of
 * LGE Android USB connection mode. This function do nothing about
 * the real USB data transfer. It is for only interface between Android
 * platform and linux android gadget driver.
 * 2011-03-11, hyunhui.park@lge.com
 */

/* Charge only function gadget driver is enumerated as HID interface
 * on host PC. By USB framework change in Android ICS, it needs to preform
 * real usb enumeration process. Charge only function gadget is revised
 * for this. Of course, this driver has no effect on host PC. This driver
 * is based on f_hid.c.
 * 2012-03-07, hyunhui.park@lge.com
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/usb/composite.h>
#include <linux/hid.h>
#include <linux/usb/g_hid.h>

#include "u_lgeusb.h"

static const char f_name[] = "charge_only";

struct charge_only_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	struct usb_request	*req;

	spinlock_t lock;
};

static struct charge_only_dev *_charge_only_dev;

static inline struct charge_only_dev *func_to_codev(struct usb_function *f)
{
	return container_of(f, struct charge_only_dev, function);
}

/* this is baed on qct's charger function driver */
static const u8 the_report_descriptor[] = {
	0x06, 0xA0, 0xFF, 0x09, 0xA5, 0xA1, 0x01, 0x09,
	0xA6, 0x09, 0xA7, 0x15, 0x80, 0x25, 0x7F, 0x75,
	0x08, 0x95, 0x02, 0x81, 0x02, 0x09, 0xA9, 0x15,
	0x80, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x91,
	0x02, 0xC0,
};

static struct usb_interface_descriptor charge_only_interface_desc = {
	.bLength                = sizeof charge_only_interface_desc,
	.bDescriptorType        = USB_DT_INTERFACE,
	/* .bInterfaceNumber    = DYNAMIC */
	.bAlternateSetting      = 0,
	.bNumEndpoints          = 0,
	.bInterfaceClass        = USB_CLASS_HID,
	.bInterfaceSubClass     = 0,
	.bInterfaceProtocol     = 0,
	/* .iInterface      = DYNAMIC */
};

static struct hid_descriptor charge_only_desc = {
	.bLength                = sizeof charge_only_desc,
	.bDescriptorType        = HID_DT_HID,
	.bcdHID                 = 0x0111,
	.bCountryCode           = 0x00,
	.bNumDescriptors        = 0x1,
	.desc[0].bDescriptorType    = HID_DT_REPORT,
	.desc[0].wDescriptorLength  = cpu_to_le16(sizeof(the_report_descriptor)),
};



static struct usb_descriptor_header *charge_only_ss_descriptors[] = {
	(struct usb_descriptor_header *)&charge_only_interface_desc,
	(struct usb_descriptor_header *)&charge_only_desc,
	NULL,
};


static struct usb_descriptor_header *charge_only_hs_descriptors[] = {
	(struct usb_descriptor_header *)&charge_only_interface_desc,
	(struct usb_descriptor_header *)&charge_only_desc,
	NULL,
};


static struct usb_descriptor_header *charge_only_fs_descriptors[] = {
	(struct usb_descriptor_header *)&charge_only_interface_desc,
	(struct usb_descriptor_header *)&charge_only_desc,
	NULL,
};

#define charge_only_FUNC_IDX 0

static struct usb_string charge_only_func_string_defs[] = {
	[charge_only_FUNC_IDX].s = "USB Charger only Interface",
	{},         /* end of list */
};

static struct usb_gadget_strings charge_only_func_string_table = {
	.language   = 0x0409,   /* en-US */
	.strings    = charge_only_func_string_defs,
};

static struct usb_gadget_strings *charge_only_func_strings[] = {
	&charge_only_func_string_table,
	NULL,
};

static int charge_only_function_setup(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	/* struct charge_only_dev	*dev = func_to_codev(f); */
	struct usb_composite_dev  *cdev = f->config->cdev;
	struct usb_request      *req  = cdev->req;
	int status = 0;
	__u16 value, length;

	value   = __le16_to_cpu(ctrl->wValue);
	length  = __le16_to_cpu(ctrl->wLength);

	pr_debug("charge_only_setup crtl_request : bRequestType:0x%x "
			"bRequest:0x%x Value:0x%x\n",
			ctrl->bRequestType, ctrl->bRequest, value);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE) << 8
			| USB_REQ_GET_DESCRIPTOR):
		switch (value >> 8) {
		case HID_DT_REPORT:
			pr_debug("USB_REQ_GET_DESCRIPTOR: REPORT\n");
			length = min_t(unsigned short, length,
					sizeof(the_report_descriptor));
			memcpy(req->buf, the_report_descriptor, length);
			goto respond;
			break;

		default:
			pr_debug("Unknown decriptor request 0x%x\n",
					value >> 8);
			goto stall;
			break;
		}
		break;

	default:
		pr_debug("Unknown request 0x%x\n", ctrl->bRequest);
		goto stall;
		break;
	}

stall:
	return -EOPNOTSUPP;

respond:
	req->zero = 0;
	req->length = length;
	status = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
	if (status < 0)
		pr_err("usb_ep_queue pr_err on ep0 %d\n", value);
	return status;
}

static int
charge_only_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct charge_only_dev	*dev = func_to_codev(f);
	int status;

	dev->cdev = cdev;
	pr_debug("charge_only_function_bind dev: %p\n", dev);

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	charge_only_interface_desc.bInterfaceNumber = status;

	/* copy descriptors */
	f->descriptors = usb_copy_descriptors(charge_only_fs_descriptors);
	if (!f->descriptors)
		goto fail;

	if (gadget_is_dualspeed(c->cdev->gadget)) {
		f->hs_descriptors = usb_copy_descriptors(charge_only_hs_descriptors);
		if (!f->hs_descriptors)
			goto fail;
	}

	if (gadget_is_superspeed(c->cdev->gadget)) {
		f->ss_descriptors = usb_copy_descriptors(charge_only_ss_descriptors);
		if (!f->ss_descriptors)
			goto fail;
	}

	return 0;

fail:
	if (f->ss_descriptors)
		usb_free_descriptors(f->ss_descriptors);
	if (f->hs_descriptors)
		usb_free_descriptors(f->hs_descriptors);
	if (f->descriptors)
		usb_free_descriptors(f->descriptors);
	if (dev->req != NULL) {
		kfree(dev->req->buf);
	}

	return status;

}

static void
charge_only_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	/* free descriptors copies */
    if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->descriptors);

}

static int charge_only_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
    /* Do nothing, but need for gadget driver */
    return 0;
}

static void charge_only_function_disable(struct usb_function *f)
{
    /* Do nothing, but need for gadget driver */
    return;
}

static int charge_only_bind_config(struct usb_configuration *c)
{
	struct charge_only_dev *dev = _charge_only_dev;
	int status;

	pr_debug("charge_only_bind_config\n");

	/* maybe allocate device-global string IDs, and patch descriptors */
	if (charge_only_func_string_defs[charge_only_FUNC_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		charge_only_func_string_defs[charge_only_FUNC_IDX].id = status;
		charge_only_interface_desc.iInterface = status;
	}

	/* As this function is just pseudo interface,
	 * It does not have any descriptors.
	 */
	dev->cdev = c->cdev;
	dev->function.name = f_name;
	dev->function.strings = charge_only_func_strings;
	dev->function.bind = charge_only_function_bind;
	dev->function.unbind = charge_only_function_unbind;
	dev->function.set_alt = charge_only_function_set_alt;
	dev->function.disable = charge_only_function_disable;
	dev->function.setup = charge_only_function_setup;

	return usb_add_function(c, &dev->function);
}

static int charge_only_setup(void)
{
	struct charge_only_dev *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	_charge_only_dev = dev;

	return 0;
}

static void charge_only_cleanup(void)
{
	kfree(_charge_only_dev);
	_charge_only_dev = NULL;
}

#if 0 /* old gb version */
static struct android_usb_function charge_only_function = {
	.name = "charge_only",
	.bind_config = charge_only_bind_config,
};

static int __init init(void)
{
	pr_debug("f_charge_only init\n");
	android_register_function(&charge_only_function);
	return 0;
}
module_init(init);
#endif
