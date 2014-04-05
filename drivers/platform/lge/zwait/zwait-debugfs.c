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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>

/* kernel/irq */
#include <internals.h>

#include "zwait.h"

#define READBUF_MAX	SZ_64K
static char read_buffer[READBUF_MAX];

static struct dentry *zwait_dentry;
static struct dentry *irq_dentry;
static struct dentry *wake_dentry;

static int requested_irqs_show(char *buf, size_t max)
{
	int count = 0;
	int irq;
	unsigned long flags;
	struct irq_desc *desc;

	for (irq = 0; irq < nr_irqs; irq++) {
		desc = irq_get_desc_buslock(irq, &flags,
				IRQ_GET_DESC_CHECK_GLOBAL);
		if (desc == NULL)
			continue;

		if (desc->action != NULL) {
			count += scnprintf(buf + count, max - count,
					"irq = %d \t"
					"depth = %d (%s) \t"
					"wake depth = %d \t"
					"chip name = %s \t"
					"action name = %s\n",
					irq,
					desc->depth,
					desc->depth == 0 ? "enable" : "disable",
					desc->wake_depth,
					desc->irq_data.chip->name,
					desc->action->name);
		}
		irq_put_desc_busunlock(desc, flags);
	}

	return count;
}

static int enabled_irqs_show(char *buf, size_t max)
{
	int count = 0;
	int irq;
	unsigned long flags;
	struct irq_desc *desc;

	for (irq = 0; irq < nr_irqs; irq++) {
		desc = irq_get_desc_buslock(irq, &flags,
				IRQ_GET_DESC_CHECK_GLOBAL);
		if (desc == NULL)
			continue;

		if (desc->action != NULL) {
			if (desc->depth == 0) {
				count += scnprintf(buf + count, max - count,
						"irq = %d \t"
						"depth = %d (enabled) \t"
						"wake depth = %d \t"
						"chip name = %s \t"
						"action name = %s\n",
						irq,
						desc->depth,
						desc->wake_depth,
						desc->irq_data.chip->name,
						desc->action->name);
			}
		}
		irq_put_desc_busunlock(desc, flags);
	}

	return count;
}

static int disabled_irqs_show(char *buf, size_t max)
{
	int count = 0;
	int irq;
	unsigned long flags;
	struct irq_desc *desc;

	for (irq = 0; irq < nr_irqs; irq++) {
		desc = irq_get_desc_buslock(irq, &flags,
				IRQ_GET_DESC_CHECK_GLOBAL);
		if (desc == NULL)
			continue;

		if (desc->action != NULL) {
			if (desc->depth > 0) {
				count += scnprintf(buf + count, max - count,
						"irq = %d \t"
						"depth = %d (disabled) \t"
						"wake depth = %d \t"
						"chip name = %s \t"
						"action name = %s\n",
						irq,
						desc->depth,
						desc->wake_depth,
						desc->irq_data.chip->name,
						desc->action->name);
			}
		}
		irq_put_desc_busunlock(desc, flags);
	}

	return count;
}

static int wake_irqs_show(char *buf, size_t max)
{
	int count = 0;
	int irq;
	unsigned long flags;
	struct irq_desc *desc;

	for (irq = 0; irq < nr_irqs; irq++) {
		desc = irq_get_desc_buslock(irq, &flags,
				IRQ_GET_DESC_CHECK_GLOBAL);
		if (desc == NULL)
			continue;

		if (desc->action != NULL) {
			if ((desc->depth == 0) && (desc->wake_depth > 0)) {
				count += scnprintf(buf + count, max - count,
						"irq = %d \t"
						"depth = %d (enabled) \t"
						"wake depth = %d \t"
						"chip name = %s \t"
						"action name = %s\n",
						irq,
						desc->depth,
						desc->wake_depth,
						desc->irq_data.chip->name,
						desc->action->name);
			}
		}
		irq_put_desc_busunlock(desc, flags);
	}

	for (irq = 0; irq < nr_irqs; irq++) {
		desc = irq_get_desc_buslock(irq, &flags,
				IRQ_GET_DESC_CHECK_GLOBAL);
		if (desc == NULL)
			continue;

		if (desc->action != NULL) {
			if ((desc->depth > 0) && (desc->wake_depth > 0)) {
				count += scnprintf(buf + count, max - count,
						"irq = %d \t"
						"depth = %d (disabled) \t"
						"wake depth = %d \t"
						"chip name = %s \t"
						"action name = %s\n",
						irq,
						desc->depth,
						desc->wake_depth,
						desc->irq_data.chip->name,
						desc->action->name);
			}
		}
		irq_put_desc_busunlock(desc, flags);
	}

	return count;
}

static int wake_dev_show(char *buf, size_t max)
{
	int count = 0;
	struct device *dev;

	device_pm_lock();
	list_for_each_entry(dev, &dpm_list, power.entry) {
		if (device_can_wakeup(dev)) {
			get_device(dev);
			count += scnprintf(buf + count, max - count,
					"dev name = %s\n", dev_name(dev));
			put_device(dev);
		}
	}
	device_pm_unlock();

	return count;
}

static int zw_wakedev_list_show(char *buf, size_t max)
{
	int count = 0;
	struct dpm_zw_wakeup *zw_wakeup;

	device_pm_lock();
	list_for_each_entry(zw_wakeup, &dpm_wakeup_dev_list, entry) {
		get_device(zw_wakeup->dev);
		count += scnprintf(buf + count, max - count,
				"dev name = %s\n", dev_name(zw_wakeup->dev));
		put_device(zw_wakeup->dev);
	}
	device_pm_unlock();

	return count;
}

static int genevt_store(const char *buf, size_t count)
{
	int len;
	char *p;

	p = memchr(buf, '\n', count);
	len = p ? p - buf : count;

	if (strncmp(buf, "pwrkey", len) == 0)
		zw_event_report(ZW_EVENT_PWRKEY);
	else if (strncmp(buf, "charger", len) == 0)
		zw_event_report(ZW_EVENT_CHARGER);
	else
		return -EINVAL;

	return count;
}

extern int dump_wakeup_source_list(char *buf, size_t max, int which);

static int ws_list_show(char *buf, size_t max)
{
	return dump_wakeup_source_list(buf, max, 2);
}

static int ws_active_list_show(char *buf, size_t max)
{
	return dump_wakeup_source_list(buf, max, 1);
}

static int ws_deactive_list_show(char *buf, size_t max)
{
	return dump_wakeup_source_list(buf, max, 0);
}

static ZW_DEBUG_ATTR(request, requested_irqs_show, NULL);
static ZW_DEBUG_ATTR(enable, enabled_irqs_show, NULL);
static ZW_DEBUG_ATTR(disable, disabled_irqs_show, NULL);
static ZW_DEBUG_ATTR(wake, wake_irqs_show, NULL);
static ZW_DEBUG_ATTR(wakedev, wake_dev_show, NULL);
static ZW_DEBUG_ATTR(zw_wakedev_list, zw_wakedev_list_show, NULL);
static ZW_DEBUG_ATTR(genevt, NULL, genevt_store);
static ZW_DEBUG_ATTR(ws_all, ws_list_show, NULL);
static ZW_DEBUG_ATTR(ws_active, ws_active_list_show, NULL);
static ZW_DEBUG_ATTR(ws_deactive, ws_deactive_list_show, NULL);

static int zw_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t zw_debugfs_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	size_t size = 0;
	struct zw_debug_ops *ops = file->private_data;

	if (ops->show == NULL)
		return -EINVAL;

	size = ops->show(read_buffer, READBUF_MAX);
	return simple_read_from_buffer(buf, count, ppos, read_buffer, size);
}

static ssize_t zw_debugfs_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct zw_debug_ops *ops = file->private_data;

	if (ops->store == NULL)
		return -EINVAL;

	return ops->store(buf, count);
}

static const struct file_operations debugfs_fops = {
	.owner = THIS_MODULE,
	.open  = zw_debugfs_open,
	.read  = zw_debugfs_read,
	.write = zw_debugfs_write,
};

static void zw_debugfs_create(const char *name, umode_t mode,
			struct dentry *parent, struct zw_debug_ops *ops)
{
	debugfs_create_file(name, mode, parent, ops, &debugfs_fops);
}

#define zw_debugfs_readonly_create(_name, _dentry, _ops) \
	zw_debugfs_create(_name, 0444, _dentry, _ops)

#define zw_debugfs_writeonly_create(_name, _dentry, _ops) \
	zw_debugfs_create(_name, 0222, _dentry, _ops)

static void irq_debugfs_create(void)
{
	zwait_dentry = debugfs_create_dir("zwait", NULL);
	if (IS_ERR(zwait_dentry))
		goto err_zwait_dentry;

	irq_dentry = debugfs_create_dir("irq", zwait_dentry);
	if (IS_ERR(irq_dentry))
		goto err_irq_dentry;

	wake_dentry = debugfs_create_dir("wake", zwait_dentry);
	if (IS_ERR(wake_dentry))
		goto err_wake_dentry;

	zw_debugfs_writeonly_create("genevt", zwait_dentry, &debug_ops_genevt);

	zw_debugfs_readonly_create("request", irq_dentry, &debug_ops_request);
	zw_debugfs_readonly_create("enable", irq_dentry, &debug_ops_enable);
	zw_debugfs_readonly_create("disable", irq_dentry, &debug_ops_disable);
	zw_debugfs_readonly_create("wake", irq_dentry, &debug_ops_wake);

	zw_debugfs_readonly_create("dev", wake_dentry, &debug_ops_wakedev);
	zw_debugfs_readonly_create("zw_wakedev_list", wake_dentry, &debug_ops_zw_wakedev_list);
	zw_debugfs_readonly_create("ws_all", wake_dentry, &debug_ops_ws_all);
	zw_debugfs_readonly_create("ws_active", wake_dentry, &debug_ops_ws_active);
	zw_debugfs_readonly_create("ws_deactive", wake_dentry, &debug_ops_ws_deactive);

	debugfs_create_symlink("class", zwait_dentry, "/sys/class/zwait");
	debugfs_create_symlink("irq", wake_dentry, "../irq/wake");

	return;

err_wake_dentry:
	debugfs_remove(irq_dentry);
err_irq_dentry:
	debugfs_remove(zwait_dentry);
err_zwait_dentry:
	return;
}

static int __init zwait_debugfs_init(void)
{
	irq_debugfs_create();
	return 0;
}

static void __exit zwait_debugfs_exit(void)
{
	debugfs_remove(irq_dentry);
	debugfs_remove(wake_dentry);
	debugfs_remove(zwait_dentry);
	return;
}

module_init(zwait_debugfs_init);
module_exit(zwait_debugfs_exit);

MODULE_DESCRIPTION("debugfs for LG zero-wait");
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_LICENSE("GPL");
