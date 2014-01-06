/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/completion.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include <asm/current.h>
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diag_dci.h"

#include <linux/timer.h>
#include "diag_debugfs.h"
#include "diag_masks.h"
#include "diagfwd_bridge.h"

MODULE_DESCRIPTION("Diag Char Driver for MTS");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

/* Identifier for data from MDM */
#define MDM_TOKEN				-1
#define BUSY_CHECK_COUNT		5
#define BUSY_WAITING_TIME_US	(100 * 1000)

static int mts_mdlog_start;

DECLARE_COMPLETION(mts_read_completion);
static int mts_read_waiting;

struct smd_info_busy {
	int in_busy_1;
	int in_busy_2;
};

struct smd_info_busy smd_busy[NUM_SMD_DATA_CHANNELS];

#define COPY_USER_SPACE_OR_EXIT(buf, data, length)		\
do {								\
	if ((count < ret+length) || (copy_to_user(buf,		\
			(void *)&data, length))) {		\
		ret = -EFAULT;					\
		goto exit;					\
	}							\
	ret += length;						\
} while (0)

dev_t dev;
struct cdev *cdev_mts;
struct class *diagchar_mts_class;
extern struct diagchar_dev *driver;

static int diagchar_mts_open(struct inode *inode, struct file *file)
{
	pr_info("diagchar_mts_open\n");
	return 0;
}

static int diagchar_mts_close(struct inode *inode, struct file *file)
{
	pr_info("diagchar_mts_close\n");
	return 0;
}

long diagchar_mts_ioctl(struct file *filp,
			   unsigned int iocmd, unsigned long ioarg)
{
	int success = 0;

	if (iocmd == DIAG_IOCTL_SWITCH_LOGGING) {
		if ((int)ioarg == MEMORY_DEVICE_MODE) {
			mts_mdlog_start = 1;
			success = 1;
		} else if ((int)ioarg == USB_MODE) {
			mts_mdlog_start = 0;
			success = 1;
		} else {
			pr_err("invalid ioarg: %lu\n", ioarg);
			success = -EINVAL;
		}
	} else {
		pr_err("invalid iocmd: %d\n", iocmd);
		success = -EINVAL;
	}

	return success;
}

int wait_mts_read_complete(void)
{
	if (mts_mdlog_start && driver->usb_connected) {
		mutex_lock(&driver->diagchar_mutex);
		mts_read_waiting = 1;
		mutex_unlock(&driver->diagchar_mutex);

		wait_for_completion_timeout(&mts_read_completion, HZ);
		INIT_COMPLETION(mts_read_completion);

		mutex_lock(&driver->diagchar_mutex);
		mts_read_waiting = 0;
		mutex_unlock(&driver->diagchar_mutex);

		return 1;
	} else
		return 0;
}

static int wait_smd_data_empty(void)
{
	int i, j;
	int smd_data_empty = 0;

	i = 0;
	do {
		for (j = 0; j < NUM_SMD_DATA_CHANNELS; j++) {
			struct diag_smd_info *data = &driver->smd_data[j];
			if ((smd_busy[j].in_busy_1 == 1) && (data->in_busy_1 == 0)) {
				smd_data_empty = 1;
				break;
			}
			if ((smd_busy[j].in_busy_2 == 1) && (data->in_busy_2 == 0)) {
				smd_data_empty = 1;
				break;
			}
		}
		usleep(BUSY_WAITING_TIME_US);
	} while (!mts_read_waiting && !smd_data_empty && (++i < BUSY_CHECK_COUNT));

	if (!smd_data_empty) {
		for (j = 0; j < NUM_SMD_DATA_CHANNELS; j++) {
			struct diag_smd_info *data = &driver->smd_data[j];
			if ((smd_busy[j].in_busy_1 == 1) && (data->in_busy_1 == 1))
				data->in_busy_1 = 0;

			if ((smd_busy[j].in_busy_2 == 1) && (data->in_busy_2 == 1))
				data->in_busy_2 = 0;
		}
	}

	return smd_data_empty;
}

static void read_smd_data(void)
{
	int i;

	if (!mts_read_waiting) {
		for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
			if (driver->smd_data[i].ch)
				queue_work(driver->diag_wq,
				&(driver->smd_data[i].diag_read_smd_work));
		}
	}
}

static int smd_data_updated(void)
{
	int i;

	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
		struct diag_smd_info *data = &driver->smd_data[i];
		if (data->in_busy_1 || data->in_busy_2)
			return 1;
	}

	return 0;
}

static int diagchar_mts_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	int index = -1, i = 0, ret = 0;
	int num_data = 0, data_type;

	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid == current->tgid)
			index = i;

	if (index == -1) {
		pr_err("diag: Client PID not found in table");
		return -EINVAL;
	}

	if (driver->usb_connected) {
		if (!wait_smd_data_empty())
			read_smd_data();
	} else {
		read_smd_data();
	}

	i = 0;
	while (!mts_read_waiting && !smd_data_updated()) {
		if (!mts_mdlog_start)
			return -EINVAL;
		usleep(BUSY_WAITING_TIME_US);

		if (++i == BUSY_CHECK_COUNT)
			return ret;
	}

	mutex_lock(&driver->diagchar_mutex);
	driver->data_ready[index] |= USER_SPACE_DATA_TYPE;

	/*Copy the type of data being passed*/
	data_type = driver->data_ready[index] & USER_SPACE_DATA_TYPE;
	COPY_USER_SPACE_OR_EXIT(buf, data_type, 4);
	/* place holder for number of data field */
	ret += 4;

	for (i = 0; i < driver->poolsize_write_struct; i++) {
		if (driver->buf_tbl[i].length > 0) {
#ifdef DIAG_DEBUG
			pr_debug("diag: WRITING the buf address "
			       "and length is %x , %d\n", (unsigned int)
				(driver->buf_tbl[i].buf),
				driver->buf_tbl[i].length);
#endif
			num_data++;
			/* Copy the length of data being passed */
			if (copy_to_user(buf+ret, (void *)&(driver->
					buf_tbl[i].length), 4)) {
				num_data--;
				goto drop;
			}
			ret += 4;

			/* Copy the actual data being passed */
			if (copy_to_user(buf+ret, (void *)driver->
					buf_tbl[i].buf, driver->buf_tbl[i].length)) {
				ret -= 4;
				num_data--;
				goto drop;
			}
			ret += driver->buf_tbl[i].length;
drop:
#ifdef DIAG_DEBUG
			pr_debug("diag: DEQUEUE buf address and"
			       " length is %x,%d\n", (unsigned int)
			       (driver->buf_tbl[i].buf), driver->
			       buf_tbl[i].length);
#endif
			diagmem_free(driver, (unsigned char *)
			(driver->buf_tbl[i].buf), POOL_TYPE_HDLC);
			driver->buf_tbl[i].length = 0;
			driver->buf_tbl[i].buf = 0;
		}
	}

	/* copy modem data */
	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
		struct diag_smd_info *data = &driver->smd_data[i];
		if (data->in_busy_1 == 1) {
			num_data++;
			/*Copy the length of data being passed*/
			COPY_USER_SPACE_OR_EXIT(buf+ret,
				(data->write_ptr_1->length), 4);
			/*Copy the actual data being passed*/
			COPY_USER_SPACE_OR_EXIT(buf+ret,
				*(data->buf_in_1),
				data->write_ptr_1->length);
			if (!driver->usb_connected || mts_read_waiting)
				data->in_busy_1 = 0;
			else
				smd_busy[i].in_busy_1 = data->in_busy_1;
		}
		if (data->in_busy_2 == 1) {
			num_data++;
			/*Copy the length of data being passed*/
			COPY_USER_SPACE_OR_EXIT(buf+ret,
				(data->write_ptr_2->length), 4);
			/*Copy the actual data being passed*/
			COPY_USER_SPACE_OR_EXIT(buf+ret,
				*(data->buf_in_2),
				data->write_ptr_2->length);
			if (!driver->usb_connected || mts_read_waiting)
				data->in_busy_2 = 0;
			else
				smd_busy[i].in_busy_2 = data->in_busy_2;
		}
	}

	/* copy number of data fields */
	COPY_USER_SPACE_OR_EXIT(buf+4, num_data, 4);
	ret -= 4;
	driver->data_ready[index] ^= USER_SPACE_DATA_TYPE;
exit:
	if (mts_read_waiting)
		complete(&mts_read_completion);

	mutex_unlock(&driver->diagchar_mutex);
	return ret;
}

static int diagchar_mts_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int err, pkt_type, token_offset = 0;
	bool remote_data = false;
	unsigned int payload_size;
	void *user_space_data = NULL;

	/* Get the packet type F3/log/event/Pkt response */
	err = copy_from_user((&pkt_type), buf, 4);
	/* First 4 bytes indicate the type of payload - ignore these */
	if (count < 4) {
		pr_err("diag: Client sending short data\n");
		return -EBADMSG;
	}
	payload_size = count - 4;
	if (payload_size > USER_SPACE_DATA) {
		pr_err("diag: Dropping packet, packet payload size crosses 8KB limit. Current payload size %d\n",
				payload_size);
		driver->dropped_count++;
		return -EBADMSG;
	}
	if (pkt_type == DCI_DATA_TYPE) {
		user_space_data = diagmem_alloc(driver, payload_size,
				POOL_TYPE_USER);
		if (!user_space_data) {
			driver->dropped_count++;
			return -ENOMEM;
		}
		err = copy_from_user(user_space_data, buf + 4, payload_size);
		if (err) {
			pr_alert("diag: copy failed for DCI data\n");
			diagmem_free(driver, user_space_data, POOL_TYPE_USER);
			return DIAG_DCI_SEND_DATA_FAIL;
		}
		err = diag_process_dci_transaction(user_space_data,
							payload_size);
		diagmem_free(driver, user_space_data, POOL_TYPE_USER);
		return err;
	}
	if (pkt_type == USER_SPACE_DATA_TYPE) {
		user_space_data = diagmem_alloc(driver, payload_size,
				POOL_TYPE_USER);
		if (!user_space_data) {
			driver->dropped_count++;
			return -ENOMEM;
		}
		err = copy_from_user(user_space_data, buf + 4, payload_size);
		if (err) {
			pr_alert("diag: copy failed for user spave data\n");
			diagmem_free(driver, user_space_data, POOL_TYPE_USER);
			return -EIO;
		}

		/* Check for proc_type */
		if (*(int *)user_space_data == MDM_TOKEN) {
			remote_data = true;
			token_offset = 4;
			payload_size -= 4;
			buf += 4;
		}

		if (!mask_request_validate(user_space_data +
					token_offset)) {
			pr_alert("diag: mask request Invalid\n");
			diagmem_free(driver, user_space_data,                                                       POOL_TYPE_USER);
			return -EFAULT;
		}

		buf = buf + 4;

		/* send masks to 8k now */
		if (!remote_data)
			diag_process_hdlc((void *)
				(user_space_data + token_offset),
				 payload_size);

		diagmem_free(driver, user_space_data, POOL_TYPE_USER);
	}

	return 0;
}

static const struct file_operations diagchar_mts_fops = {
	.owner = THIS_MODULE,
	.read = diagchar_mts_read,
	.write = diagchar_mts_write,
	.unlocked_ioctl = diagchar_mts_ioctl,
	.open = diagchar_mts_open,
	.release = diagchar_mts_close
};

static int diagchar_setup_cdev(dev_t devno)
{
	int err;

	cdev_init(cdev_mts, &diagchar_mts_fops);

	cdev_mts->owner = THIS_MODULE;
	cdev_mts->ops = &diagchar_mts_fops;

	err = cdev_add(cdev_mts, devno, 1);

	if (err) {
		printk(KERN_INFO "diagchar_mts cdev registration failed !\n\n");
		return -1;
	}

	diagchar_mts_class = class_create(THIS_MODULE, "diag_mts");

	if (IS_ERR(diagchar_mts_class)) {
		printk(KERN_ERR "Error creating diagchar class.\n");
		return -1;
	}

	device_create(diagchar_mts_class, NULL, devno, 0, "diag_mts");

	return 0;

}

static int diagchar_cleanup(void)
{
	if (cdev_mts) {
		/* TODO - Check if device exists before deleting */
		device_destroy(diagchar_mts_class, dev);
		cdev_del(cdev_mts);
	}

	if (!IS_ERR(diagchar_mts_class))
		class_destroy(diagchar_mts_class);

	return 0;
}

static int __init diagchar_mts_init(void)
{
	int error;

	error = alloc_chrdev_region(&dev, 0, 1, "diag_mts");
	if (error) {
		printk(KERN_INFO "Major number not allocated\n");
		goto fail;
	}

	cdev_mts = cdev_alloc();

	diagchar_setup_cdev(dev);

	return 0;

fail:
	return -1;
}

static void diagchar_mts_exit(void)
{
	diagchar_cleanup();
}

module_init(diagchar_mts_init);
module_exit(diagchar_mts_exit);
