/* linux/drivers/usb/gadget/u_lgeusb.c
 *
 * Copyright (C) 2011,2012 LG Electronics Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/spmi.h>
#include <mach/board.h>
#ifdef CONFIG_MACH_LGE
#include <mach/board_lge.h>
#endif
#include <linux/platform_data/lge_android_usb.h>

#include "u_lgeusb.h"

static struct mutex lgeusb_lock;

#ifdef CONFIG_USB_G_LGE_ANDROID_AUTORUN
static u16 user_mode;
#endif

/* This length must be same as MAX_STR_LEN in android.c */
#define MAX_SERIAL_NO_LEN 256

#define LGE_VENDOR_ID 	0x1004
#define LGE_PRODUCT_ID 	0x618E
#define LGE_FACTORY_PID	0x6000

/* PMIC USB CHGPTH register */
#define SMBB_USB_CHGPTH_BASE                0x1300
#define SMBB_USB_CHGPTH_USB_CHG_PTH_STS     (SMBB_USB_CHGPTH_BASE + 0x09)
#define SMBB_USB_CHGPTH_INT_RT_STS          (SMBB_USB_CHGPTH_BASE + 0x10)
#define SMBB_DC_CHGPATH_BASE				0x1400
#define SMBB_DC_CHGPATH_DC_CHG_PTH_STS		(SMBB_DC_CHGPATH_BASE + 0x0A)
#define	SMBB_DC_CHGPATH_INT_RT_STS			(SMBB_DC_CHGPATH_BASE + 0x10)

struct lgeusb_dev {
	struct device *dev;
	u16 vendor_id;
	u16 factory_pid;
	u8  iSerialNumber;
	const char *product;
	const char *manufacturer;
	const char *fcomposition;
	enum lgeusb_mode current_mode;

	int (*get_serial_number)(char *serial);
	int (*get_factory_cable)(void);
	bool (*is_factory_mode)(void);
};

static char model_string[32];
static char swver_string[32];
static char subver_string[32];
static char phoneid_string[32];

static struct lgeusb_dev *_lgeusb_dev;

int debug_pmic_register_for_usb(void)
{
	int		rc;
	u8      usb_sts_reg, usb_rt_reg;
	u8		usb_valid;

	struct spmi_controller *ctrl = spmi_busnum_to_ctrl(0);

	if(!ctrl) {
		pr_err("Controller is null!\n");
		return -EINVAL;
	}

	/* read SMBB_USB_CHGPTH_USB_CHG_PTH_STS, SMBB_USB_CHGPTH_INT_RT_STS registers
	*/
	rc = spmi_ext_register_readl(ctrl, 0, SMBB_USB_CHGPTH_USB_CHG_PTH_STS,  &usb_sts_reg, 1);
	if (rc)
	{
		pr_err("[%s] spmi read failed-rc:%d , addr:0x%x", __func__, 
									   rc, SMBB_USB_CHGPTH_USB_CHG_PTH_STS);
		return -EINVAL;
	}
	usb_valid = ((usb_sts_reg & 0xC0) >> 6);	

	rc = spmi_ext_register_readl(ctrl, 0, SMBB_USB_CHGPTH_INT_RT_STS,  &usb_rt_reg, 1);
	if (rc)
	{
		pr_err("[%s] spmi read failed-rc:%d ,addr:0x%x", __func__, 
										rc, SMBB_USB_CHGPTH_INT_RT_STS);
		return -EINVAL;
	}

	pr_info("[SMBB_USB_CHGPTH]sts:0x%x, usb_valid:0x%x, rt:0x%x\n", 
								usb_sts_reg, usb_valid, usb_rt_reg);

	return rc;
}
EXPORT_SYMBOL(debug_pmic_register_for_usb);

/* Belows are borrowed from android gadget's ATTR macros ;) */
#define LGE_ID_ATTR(field, format_string)               \
static ssize_t                              \
lgeusb_ ## field ## _show(struct device *dev, struct device_attribute *attr, \
		char *buf)                      \
{                                   \
	struct lgeusb_dev *usbdev = _lgeusb_dev;		\
	return sprintf(buf, format_string, usbdev->field);      \
}                                   \
static ssize_t                              \
lgeusb_ ## field ## _store(struct device *dev, struct device_attribute *attr, \
		const char *buf, size_t size)                   \
{                                   \
	int value;                              \
	struct lgeusb_dev *usbdev = _lgeusb_dev;	\
	if (sscanf(buf, format_string, &value) == 1) {          \
		usbdev->field = value;              \
		return size;                        \
	}                               \
	return -1;                          \
}                                   \
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, lgeusb_ ## field ## _show, \
		lgeusb_ ## field ## _store);

#define LGE_RDONLY_STRING_ATTR(field, string)               \
static ssize_t                              \
lgeusb_ ## field ## _show(struct device *dev, struct device_attribute *attr,   \
		char *buf)                      \
{                                   \
	struct lgeusb_dev *usbdev = _lgeusb_dev;		\
	return sprintf(buf, "%s", usbdev->string);              \
}                                   \
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, lgeusb_ ## field ## _show, NULL);

#define LGE_STRING_ATTR(field, buffer)               \
static ssize_t                              \
field ## _show(struct device *dev, struct device_attribute *attr,   \
		char *buf)                      \
{                                   \
	return snprintf(buf, PAGE_SIZE, "%s", buffer);          \
}                                   \
static ssize_t                              \
field ## _store(struct device *dev, struct device_attribute *attr,  \
		const char *buf, size_t size)                   \
{                                   \
	if (size >= sizeof(buffer)) \
		return -EINVAL;         \
	if (sscanf(buf, "%31s", buffer) == 1) {            \
		return size;                        \
	}                               \
	return -1;                          \
}                                   \
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

LGE_ID_ATTR(vendor_id, "%04X\n")
LGE_ID_ATTR(factory_pid, "%04X\n")
LGE_ID_ATTR(iSerialNumber, "%d\n")
LGE_RDONLY_STRING_ATTR(product_name, product)
LGE_RDONLY_STRING_ATTR(manufacturer_name, manufacturer)
LGE_RDONLY_STRING_ATTR(fcomposition, fcomposition)
LGE_STRING_ATTR(model_name, model_string)
LGE_STRING_ATTR(sw_version, swver_string)
LGE_STRING_ATTR(sub_version, subver_string)
LGE_STRING_ATTR(phone_id, phoneid_string)

static ssize_t lgeusb_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	int is_factory_cable = 0;
	int ret = 0;

	if (usbdev->get_factory_cable)
		is_factory_cable = usbdev->get_factory_cable();

	mutex_lock(&lgeusb_lock);
	if (is_factory_cable)
		usbdev->current_mode = LGEUSB_FACTORY_MODE;
	else
		usbdev->current_mode = LGEUSB_ANDROID_MODE;
	mutex_unlock(&lgeusb_lock);

	switch (is_factory_cable) {
	case LGEUSB_FACTORY_56K:
		ret = sprintf(buf, "%s\n", "factory_56k");
		break;
	case LGEUSB_FACTORY_130K:
		ret = sprintf(buf, "%s\n", "factory_130k");
		break;
	case LGEUSB_FACTORY_910K:
		ret = sprintf(buf, "%s\n", "factory_910k");
		break;
	default:
		ret = sprintf(buf, "%s\n", "normal");
		break;
	}

	return ret;
}
static DEVICE_ATTR(lge_usb_mode, S_IRUGO | S_IWUSR, lgeusb_mode_show, NULL);

#ifdef CONFIG_USB_G_LGE_ANDROID_AUTORUN
/* To set/get USB user mode to/from user space for autorun */
static int autorun_user_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d", user_mode);
	return ret;
}

static int autorun_user_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	unsigned long tmp;

	ret = strict_strtoul(buf, 10, &tmp);
	if (ret)
		return ret;

	mutex_lock(&lgeusb_lock);
	user_mode = (unsigned int)tmp;
	mutex_unlock(&lgeusb_lock);

	pr_info("autorun user mode : %d\n", user_mode);

	return ret;
}
static DEVICE_ATTR(autorun_user_mode, S_IRUGO | S_IWUSR, autorun_user_mode_show,
		autorun_user_mode_store);

int lgeusb_get_autorun_user_mode(void)
{
	return user_mode;
}
#endif

static struct device_attribute *lge_android_usb_attributes[] = {
	&dev_attr_vendor_id,
	&dev_attr_factory_pid,
	&dev_attr_product_name,
	&dev_attr_manufacturer_name,
	&dev_attr_fcomposition,
	&dev_attr_lge_usb_mode,
	&dev_attr_iSerialNumber,
	&dev_attr_model_name,
	&dev_attr_sw_version,
	&dev_attr_sub_version,
	&dev_attr_phone_id,
#ifdef CONFIG_USB_G_LGE_ANDROID_AUTORUN
	&dev_attr_autorun_user_mode,
#endif
	NULL
};

static int lgeusb_create_device_file(struct lgeusb_dev *dev)
{
	struct device_attribute **attrs = lge_android_usb_attributes;
	struct device_attribute *attr;
	int ret;

	while ((attr = *attrs++)) {
		ret = device_create_file(dev->dev, attr);
		if (ret)
			pr_err("usb: lgeusb: error on creating device file %s\n",
					attr->attr.name);
	}

	return 0;
}

bool lgeusb_is_factory_mode(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	if (usbdev->is_factory_mode)
		return usbdev->is_factory_mode();
	return false;
}

int lgeusb_get_pif_cable(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	if (usbdev->get_factory_cable)
		return usbdev->get_factory_cable();
	return 0;
}

int lgeusb_get_vendor_id(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	return usbdev ? usbdev->vendor_id : -EINVAL;
}

int lgeusb_get_factory_pid(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	return usbdev ? usbdev->factory_pid : -EINVAL;
}

int lgeusb_get_serial_number(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	return usbdev ? usbdev->iSerialNumber : -EINVAL;
}

int lgeusb_get_manufacturer_name(char *manufact_name)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	char *manufact = manufact_name;

	if (!manufact || !usbdev || !usbdev->manufacturer)
		return -EINVAL;

	strlcpy(manufact, usbdev->manufacturer, MAX_SERIAL_NO_LEN - 1);
	pr_debug("lgeusb: manfacturer name %s\n", manufact);
	return 0;
}

int lgeusb_get_product_name(char *prod_name)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	char *prod = prod_name;

	if (!prod || !usbdev || !usbdev->product)
		return -EINVAL;

	strlcpy(prod, usbdev->product, MAX_SERIAL_NO_LEN - 1);
	pr_debug("lgeusb: product name %s\n", prod);
	return 0;
}

int lgeusb_get_factory_composition(char *fcomposition)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	char *fcompo = fcomposition;

	if (!fcomposition || !usbdev || !usbdev->fcomposition)
		return -EINVAL;

	strlcpy(fcompo, usbdev->fcomposition, MAX_SERIAL_NO_LEN - 1);
	pr_debug("lgeusb: factory composition %s\n", fcompo);
	return 0;
}

int lgeusb_get_model_name(char *model)
{
	if (!model || strlen(model) > 15)
		return -EINVAL;

	strlcpy(model, model_string, strlen(model) - 1);
	pr_info("lgeusb: model name %s\n", model);
	return 0;
}

int lgeusb_get_phone_id(char *phoneid)
{
	if (!phoneid || strlen(phoneid) > 15)
		return -EINVAL;

	strlcpy(phoneid, phoneid_string, strlen(phoneid) - 1);
	pr_info("lgeusb: phoneid %s\n", phoneid);
	return 0;
}

int lgeusb_get_sw_ver(char *sw_ver)
{
	if (!sw_ver || strlen(sw_ver) > 15)
		return -EINVAL;

	strlcpy(sw_ver, swver_string, strlen(sw_ver) - 1);
	pr_info("lgeusb: sw version %s\n", sw_ver);
	return 0;
}

int lgeusb_get_sub_ver(char *sub_ver)
{
	if (!sub_ver || strlen(sub_ver) > 15)
		return -EINVAL;

	strlcpy(sub_ver, subver_string, strlen(sub_ver) - 1);
	pr_info("lgeusb: sw sub version %s\n", sub_ver);
	return 0;
}

static int get_factory_cable(void)
{
	struct chg_cable_info info;
	enum lge_boot_mode_type boot_mode;
	int res;

	/* if boot mode is factory,
	 * cable must be factory cable.
	 */
	boot_mode = lge_get_boot_mode();
	switch (boot_mode) {
	case LGE_BOOT_MODE_FACTORY:
	case LGE_BOOT_MODE_PIFBOOT:
		res = LGEUSB_FACTORY_130K;
		goto done;
	case LGE_BOOT_MODE_FACTORY2:
	case LGE_BOOT_MODE_PIFBOOT2:
		res = LGEUSB_FACTORY_56K;
		goto done;
	default:
		break;
	}
	/* get cable infomation */
	res = lge_pm_get_cable_info(&info);
	if (res < 0) {
		pr_err("Error get cable information from PMIC %d\n", res);
		return res;
	}

	switch (info.cable_type) {
	/* It is factory cable */
	case CABLE_56K:
		res = LGEUSB_FACTORY_56K;
		break;
	case CABLE_130K:
		res = LGEUSB_FACTORY_130K;
		break;
	case CABLE_910K:
		res = LGEUSB_FACTORY_910K;
		break;
	/* It is normal cable */
	default:
		res = 0;
		break;
	}

done:
	return res;
}

static bool is_factory_mode(void)
{
	enum lge_boot_mode_type boot_mode;
	bool res;

	boot_mode = lge_get_boot_mode();
	switch (boot_mode) {
	case LGE_BOOT_MODE_FACTORY:
	case LGE_BOOT_MODE_PIFBOOT:
	case LGE_BOOT_MODE_FACTORY2:
	case LGE_BOOT_MODE_PIFBOOT2:
	case LGE_BOOT_MODE_MINIOS:
		res = true;
		break;
	default:
		res = false;
		break;
	}

	return res;
}

struct lge_android_usb_platform_data *lge_android_usb_dt_to_pdata(
		struct platform_device *pdev)
{
	struct device_node *node;
	struct lge_android_usb_platform_data *pdata;

	if (!pdev) {
		pr_err("Error: Null Platform device\n");
		return NULL;
	}

	node = pdev->dev.of_node;
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("unable to allocate platform data\n");
		return NULL;
	}
	of_property_read_u32(node, "lge,lge_android_usb,vendor_id",
			&pdata->vendor_id);
	of_property_read_u32(node, "lge,lge_android_usb,factory_pid",
			&pdata->factory_pid);
	of_property_read_u32(node, "lge,lge_android_usb,iSerialNumber",
			&pdata->iSerialNumber);
	of_property_read_string(node, "lge,lge_android_usb,product_name",
			&pdata->product_name);
	of_property_read_string(node, "lge,lge_android_usb,manufacturer_name",
			&pdata->manufacturer_name);
	of_property_read_string(node, "lge,lge_android_usb,factory_composition",
			&pdata->factory_composition);
	return pdata;
}


static int __devinit lgeusb_probe(struct platform_device *pdev)
{
	struct lge_android_usb_platform_data *pdata;
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	int ret = 0;

	pdata = lge_android_usb_dt_to_pdata(pdev);
	usbdev->dev = &pdev->dev;

	dev_dbg(&pdev->dev, "%s: pdata: %p\n", __func__, pdata);

	if (pdata) {
		if (pdata->vendor_id)
			usbdev->vendor_id = pdata->vendor_id;

		if (pdata->factory_pid)
			usbdev->factory_pid = pdata->factory_pid;

		if (pdata->iSerialNumber)
			usbdev->iSerialNumber = pdata->iSerialNumber;

		if (pdata->product_name)
			usbdev->product = pdata->product_name;

		if (pdata->manufacturer_name)
			usbdev->manufacturer = pdata->manufacturer_name;

		if (pdata->factory_composition)
			usbdev->fcomposition = pdata->factory_composition;

		if (pdata->get_factory_cable)
			usbdev->get_factory_cable = pdata->get_factory_cable;
		else
			usbdev->get_factory_cable = get_factory_cable;

		if (pdata->is_factory_mode)
			usbdev->is_factory_mode = pdata->is_factory_mode;
		else
			usbdev->is_factory_mode = is_factory_mode;

	}

	usbdev->current_mode = LGEUSB_DEFAULT_MODE;
	lgeusb_create_device_file(usbdev);

	return ret;
}

static void android_destroy_device(struct lgeusb_dev *dev)
{
	struct device_attribute **attrs = lge_android_usb_attributes;
	struct device_attribute *attr;

	while ((attr = *attrs++))
		device_remove_file(dev->dev, attr);
}

static int lge_android_remove(struct platform_device *pdev)
{
	struct lgeusb_dev *dev = _lgeusb_dev;

	if (dev) {
		android_destroy_device(dev);
		kfree(dev);
	}

	return 0;
}

static const struct platform_device_id lge_android_id_table[] __devinitconst = {
	{
		.name = "lge_android_usb",
	},
};

static struct of_device_id lge_usb_android_dt_match[] = {
	{   .compatible = "lge,lge-android-usb",
	},
	{}
};

static struct platform_driver lge_android_usb_platform_driver = {
	.driver = {
		.name = "lge_android_usb",
		.of_match_table = lge_usb_android_dt_match,
	},
	.probe = lgeusb_probe,
	.remove = lge_android_remove,
	.id_table = lge_android_id_table,
};

static int __init lgeusb_init(void)
{
	int ret;
	struct lgeusb_dev *dev;

	pr_info("u_lgeusb init\n");
	mutex_init(&lgeusb_lock);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	_lgeusb_dev = dev;

	/* set default vid, pid and factory id. vid and pid will be overrided. */
	dev->vendor_id = LGE_VENDOR_ID;
	dev->factory_pid = LGE_FACTORY_PID;

	ret = platform_driver_register(&lge_android_usb_platform_driver);
	if (ret) {
		pr_err("%s(): Failed to register android"
				"platform driver\n", __func__);
	}

	return ret;
}
module_init(lgeusb_init);

static void __exit lgeusb_cleanup(void)
{
	platform_driver_unregister(&lge_android_usb_platform_driver);
	kfree(_lgeusb_dev);
	_lgeusb_dev = NULL;
}
module_exit(lgeusb_cleanup);
