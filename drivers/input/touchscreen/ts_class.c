/*
 * TOUCH INFO Class Core
 *
 * Copyright (C) 2011 Fred Cho <fred.cho@lge.com>
 *
 * We have sevral interfaces for comunication with framework.
 * Need to unified path for it.
 */

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/export.h>
#include "ts_class.h"

DECLARE_RWSEM(ts_info_list_lock);
EXPORT_SYMBOL_GPL(ts_info_list_lock);

LIST_HEAD(ts_info_list);
EXPORT_SYMBOL_GPL(ts_info_list);

static struct class *ts_info_class;

static ssize_t
ts_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct ts_info_classdev *ts_info_cdev = dev_get_drvdata(dev);

#if defined(CONFIG_TOUCHSCREEN_S3200_SYNAPTICS_TS) || defined(CONFIG_USING_INNOTEK_PANEL_4_7)
	len = snprintf(buf, PAGE_SIZE, "%s\n", ts_info_cdev->version);
#else
	len = snprintf(buf, PAGE_SIZE, "%d\n", ts_info_cdev->version);
#endif
	return len;
}

static struct device_attribute ts_info_class_attrs[] = {
	__ATTR(version, S_IRUGO | S_IWUSR, ts_version_show, NULL),
};

int ts_info_classdev_register(struct device *parent, struct ts_info_classdev *ts_info_cdev)
{
	ts_info_cdev->dev = device_create(ts_info_class, parent, 0, ts_info_cdev, "%s", ts_info_cdev->name);
	if (IS_ERR(ts_info_cdev->dev))
		return PTR_ERR(ts_info_cdev->dev);
	down_write(&ts_info_list_lock);
	list_add_tail(&ts_info_cdev->node, &ts_info_list);
	up_write(&ts_info_list_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(ts_info_classdev_register);

void ts_info_classdev_unregister(struct ts_info_classdev *ts_info_cdev)
{
	device_unregister(ts_info_cdev->dev);
	down_write(&ts_info_list_lock);
	list_del(&ts_info_list);
	up_write(&ts_info_list_lock);
}
EXPORT_SYMBOL_GPL(ts_info_classdev_unregister);

static int __init ts_info_init(void)
{
	printk("%s : ts_info calss create\n", __func__);
	ts_info_class = class_create(THIS_MODULE, "input_info");
	if (IS_ERR(ts_info_class))
		return PTR_ERR(ts_info_class);
	ts_info_class->dev_attrs = ts_info_class_attrs;
	return 0;
}

static void __exit ts_info_exit(void)
{
	class_destroy(ts_info_class);
}

subsys_initcall(ts_info_init);
module_exit(ts_info_exit);

MODULE_AUTHOR("LGE");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TOUCH INFO Class Interface");
