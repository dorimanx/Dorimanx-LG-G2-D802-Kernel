/*
* LGE MOCA Driver for MOCA application
*
* Youngjin Park <jin.park@lge.com>
*
* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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
*/

#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/msm_smd_pkt.h>
#include <linux/poll.h>
#include <asm/ioctls.h>
#include <linux/wakelock.h>

#include <mach/msm_smd.h>
#include <mach/subsystem_restart.h>
#include <mach/msm_ipc_logging.h>

#include "smd_private.h"

#define NUM_LGE_MOCA_PORTS 1

#define LGE_MOCA_DEVICE_NAME "lge_moca"
#define PDRIVER_NAME_MAX_SIZE 32
#define MOCA_MODULE_NAME		"MOCA"

#define WAKELOCK_TIMEOUT (2*HZ)

struct lge_moca_dev {
	struct cdev cdev;
	struct device *devicep;
	void *pil;
	char pdriver_name[PDRIVER_NAME_MAX_SIZE];
	struct platform_driver driver;

	struct smd_channel *ch;
	struct mutex ch_lock;
	struct mutex rx_lock;
	struct mutex tx_lock;
	wait_queue_head_t ch_read_wait_queue;
	wait_queue_head_t ch_write_wait_queue;
	wait_queue_head_t ch_opened_wait_queue;

	int i;
	int ref_cnt;

	int blocking_write;
	int is_open;
	int poll_mode;
	unsigned ch_size;
	uint open_modem_wait;

	int has_reset;
	int do_reset_notification;
	struct completion ch_allocated;
	struct wake_lock pa_wake_lock;		/* Packet Arrival Wake lock*/
	struct work_struct packet_arrival_work;
	struct spinlock pa_spinlock;
	int wakelock_locked;
} *lge_moca_devp[NUM_LGE_MOCA_PORTS];

struct class *lge_moca_classp;
static dev_t lge_moca_number;

static char *lge_moca_dev_name[] = {
	"lge_moca_drv0",
};

static char *smd_ch_name[] = {
	"DATA30",
};

static uint32_t smd_ch_edge[] = {
	SMD_APPS_MODEM,
};

static ssize_t lge_moca_open_timeout_store(struct device *d,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	int i;
	unsigned long tmp;
	for (i = 0; i < NUM_LGE_MOCA_PORTS; ++i) {
		if (lge_moca_devp[i]->devicep == d)
			break;
	}
	if (i >= NUM_LGE_MOCA_PORTS) {
		pr_err("%s: unable to match device to valid smd_pkt port\n",
			__func__);
		return -EINVAL;
	}
	if (!strict_strtoul(buf, 10, &tmp)) {
		lge_moca_devp[i]->open_modem_wait = tmp;
		return n;
	} else {
		pr_err("%s: unable to convert: %s to an int\n", __func__,
			buf);
		return -EINVAL;
	}
}

static ssize_t lge_moca_open_timeout_show(struct device *d,
				 struct device_attribute *attr,
				 char *buf)
{
	int i;
	for (i = 0; i < NUM_LGE_MOCA_PORTS; ++i) {
		if (lge_moca_devp[i]->devicep == d)
			break;
	}
	if (i >= NUM_LGE_MOCA_PORTS) {
		pr_err("%s: unable to match device to valid smd_pkt port\n",
			__func__);
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n",
			lge_moca_devp[i]->open_modem_wait);
}

static DEVICE_ATTR(open_timeout, 0664, lge_moca_open_timeout_show,
	lge_moca_open_timeout_store);

static int lge_moca_notify_reset(struct lge_moca_dev *lge_moca_devp)
{
	lge_moca_devp->do_reset_notification = 0;

	return -ENETRESET;
}

static void lge_moca_clean_and_signal(struct lge_moca_dev *lge_moca_devp)
{
	lge_moca_devp->do_reset_notification = 1;
	lge_moca_devp->has_reset = 1;

	lge_moca_devp->is_open = 0;

	wake_up(&lge_moca_devp->ch_read_wait_queue);
	wake_up(&lge_moca_devp->ch_write_wait_queue);
	wake_up_interruptible(&lge_moca_devp->ch_opened_wait_queue);
}

static int lge_moca_dummy_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NUM_LGE_MOCA_PORTS; i++) {
		if (smd_ch_edge[i] == pdev->id
		    && !strncmp(pdev->name, smd_ch_name[i],
				SMD_MAX_CH_NAME_LEN)
		    && lge_moca_devp[i]->driver.probe) {
			complete_all(&lge_moca_devp[i]->ch_allocated);
			break;
		}
	}
	return 0;
}

static void lge_moca_packet_arrival_worker(struct work_struct *work)
{
	struct lge_moca_dev *lge_moca_devp;
	unsigned long flags;

	lge_moca_devp = container_of(work, struct lge_moca_dev,
				    packet_arrival_work);
	mutex_lock(&lge_moca_devp->ch_lock);
	spin_lock_irqsave(&lge_moca_devp->pa_spinlock, flags);
	if (lge_moca_devp->ch && lge_moca_devp->wakelock_locked) {
		wake_lock_timeout(&lge_moca_devp->pa_wake_lock,
				  WAKELOCK_TIMEOUT);
	}
	spin_unlock_irqrestore(&lge_moca_devp->pa_spinlock, flags);
	mutex_unlock(&lge_moca_devp->ch_lock);
}

static void lge_moca_check_and_wakeup_reader(struct lge_moca_dev
	*lge_moca_devp)
{
	int sz;
	unsigned long flags;

	if (!lge_moca_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return;
	}

	if (!lge_moca_devp->ch) {
		pr_err("%s on a closed lge_moca_dev id:%d\n",
			__func__, lge_moca_devp->i);
		return;
	}

	sz = smd_cur_packet_size(lge_moca_devp->ch);
	if (sz == 0)
		return;

	if (!smd_read_avail(lge_moca_devp->ch))
		return;

	/* here we have a packet of size sz ready */
	spin_lock_irqsave(&lge_moca_devp->pa_spinlock, flags);
	wake_lock(&lge_moca_devp->pa_wake_lock);
	lge_moca_devp->wakelock_locked = 1;
	spin_unlock_irqrestore(&lge_moca_devp->pa_spinlock, flags);
	wake_up(&lge_moca_devp->ch_read_wait_queue);
	schedule_work(&lge_moca_devp->packet_arrival_work);
}

static void lge_moca_check_and_wakeup_writer(struct lge_moca_dev
	*lge_moca_devp)
{
	int sz;

	if (!lge_moca_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return;
	}

	if (!lge_moca_devp->ch) {
		pr_err("%s on a closed lge_moca_dev id:%d\n",
			__func__, lge_moca_devp->i);
		return;
	}

	sz = smd_write_avail(lge_moca_devp->ch);
	if (sz) {
		smd_disable_read_intr(lge_moca_devp->ch);
		wake_up(&lge_moca_devp->ch_write_wait_queue);
	}
}

static void lge_moca_ch_notify(void *priv, unsigned event)
{
	struct lge_moca_dev *lge_moca_devp = priv;

	if (lge_moca_devp->ch == 0) {
		pr_err("%s on a closed lge_moca_dev id:%d\n",
			__func__, lge_moca_devp->i);
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA: {
		lge_moca_check_and_wakeup_reader(lge_moca_devp);
		if (lge_moca_devp->blocking_write)
			lge_moca_check_and_wakeup_writer(lge_moca_devp);
		break;
	}
	case SMD_EVENT_OPEN:
		lge_moca_devp->has_reset = 0;
		lge_moca_devp->is_open = 1;
		wake_up_interruptible(&lge_moca_devp->ch_opened_wait_queue);
		break;
	case SMD_EVENT_CLOSE:
		lge_moca_devp->is_open = 0;
		/* put port into reset state */
		lge_moca_clean_and_signal(lge_moca_devp);
		break;
	}
}

static long lge_moca_ioctl(struct file *file, unsigned int cmd,
					     unsigned long arg)
{
	int ret;
	struct lge_moca_dev *lge_moca_devp;

	lge_moca_devp = file->private_data;
	if (!lge_moca_devp)
		return -EINVAL;

	mutex_lock(&lge_moca_devp->ch_lock);
	switch (cmd) {
	case TIOCMGET:
		ret = smd_tiocmget(lge_moca_devp->ch);
		break;
	case TIOCMSET:
		ret = smd_tiocmset(lge_moca_devp->ch, arg, ~arg);
		break;
	case SMD_PKT_IOCTL_BLOCKING_WRITE:
		ret = get_user(lge_moca_devp->blocking_write, (int *)arg);
		break;
	default:
		pr_err("%s: Unrecognized ioctl command %d\n", __func__, cmd);
		ret = -1;
	}
	mutex_unlock(&lge_moca_devp->ch_lock);

	return ret;
}

static unsigned int lge_moca_poll(struct file *file, poll_table *wait)
{
	struct lge_moca_dev *lge_moca_devp;
	unsigned int mask = 0;

	lge_moca_devp = file->private_data;
	if (!lge_moca_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return POLLERR;
	}

	lge_moca_devp->poll_mode = 1;
	poll_wait(file, &lge_moca_devp->ch_read_wait_queue, wait);
	mutex_lock(&lge_moca_devp->ch_lock);
	if (lge_moca_devp->has_reset || !lge_moca_devp->ch) {
		mutex_unlock(&lge_moca_devp->ch_lock);
		return POLLERR;
	}

	if (smd_read_avail(lge_moca_devp->ch))
		mask |= POLLIN | POLLRDNORM;

	mutex_unlock(&lge_moca_devp->ch_lock);

	return mask;
}

ssize_t lge_moca_write(struct file *file,
		       const char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int r = 0, bytes_written;
	struct lge_moca_dev *lge_moca_devp;
	DEFINE_WAIT(write_wait);

	lge_moca_devp = file->private_data;

	usleep(1000);
	if (!lge_moca_devp) {
		pr_err("%s on NULL lge_moca_dev\n", __func__);
		return -EINVAL;
	}

	if (!lge_moca_devp->ch) {
		pr_err("%s on a closed lge_moca_dev id:%d\n",
			__func__, lge_moca_devp->i);
		return -EINVAL;
	}

	if (lge_moca_devp->do_reset_notification || lge_moca_devp->has_reset) {
		/* notify client that a reset occurred */
		return lge_moca_notify_reset(lge_moca_devp);
	}

	mutex_lock(&lge_moca_devp->tx_lock);
	if (!lge_moca_devp->blocking_write) {
		if (smd_write_avail(lge_moca_devp->ch) < count) {
			pr_err("%s: Not enough space in lge_moca_dev id:%d\n",
				   __func__, lge_moca_devp->i);
			mutex_unlock(&lge_moca_devp->tx_lock);
			return -ENOMEM;
		}
	}

	r = smd_write_start(lge_moca_devp->ch, count);
	if (r < 0) {
		mutex_unlock(&lge_moca_devp->tx_lock);
		pr_err("%s: Error:%d in lge_moca_dev id:%d @ smd_write_start\n",
			__func__, r, lge_moca_devp->i);
		return r;
	}

	bytes_written = 0;
	do {
		prepare_to_wait(&lge_moca_devp->ch_write_wait_queue,
				&write_wait, TASK_UNINTERRUPTIBLE);
		if (!smd_write_avail(lge_moca_devp->ch) &&
		    !lge_moca_devp->has_reset) {
			smd_enable_read_intr(lge_moca_devp->ch);
			schedule();
		}
		finish_wait(&lge_moca_devp->ch_write_wait_queue, &write_wait);
		smd_disable_read_intr(lge_moca_devp->ch);

		if (lge_moca_devp->has_reset) {
			mutex_unlock(&lge_moca_devp->tx_lock);
			return lge_moca_notify_reset(lge_moca_devp);
		} else {
			r = smd_write_segment(lge_moca_devp->ch,
					      (void *)(buf + bytes_written),
					      (count - bytes_written), 1);
			if (r < 0) {
				mutex_unlock(&lge_moca_devp->tx_lock);
				if (lge_moca_devp->has_reset)
					return lge_moca_notify_reset(
					lge_moca_devp);

				pr_err("%s on lge_moca_dev id:%d failed r:%d\n",
					__func__, lge_moca_devp->i, r);
				return r;
			}
			bytes_written += r;
		}
	} while (bytes_written != count);
	smd_write_end(lge_moca_devp->ch);
	mutex_unlock(&lge_moca_devp->tx_lock);

#ifdef MOCA_DEBUG
	pr_info(MOCA_MODULE_NAME ": %s: count = %d\n", __func__, count);
#endif

	return count;
}

ssize_t lge_moca_read(struct file *file,
		       char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int r;
	int bytes_read;
	int pkt_size;
	struct lge_moca_dev *lge_moca_devp;
	unsigned long flags;

	lge_moca_devp = file->private_data;

	if (!lge_moca_devp) {
		pr_err("%s on NULL lge_moca_dev\n", __func__);
		return -EINVAL;
	}

	if (!lge_moca_devp->ch) {
		pr_err("%s on a closed lge_moca_dev id:%d\n",
			__func__, lge_moca_devp->i);
		return -EINVAL;
	}

	if (lge_moca_devp->do_reset_notification) {
		/* notify client that a reset occurred */
		return lge_moca_notify_reset(lge_moca_devp);
	}

wait_for_packet:
	r = wait_event_interruptible(lge_moca_devp->ch_read_wait_queue,
				     !lge_moca_devp->ch ||
				     (smd_cur_packet_size(lge_moca_devp->ch) > 0
				      && smd_read_avail(lge_moca_devp->ch)) ||
				     lge_moca_devp->has_reset);

	mutex_lock(&lge_moca_devp->rx_lock);
	if (lge_moca_devp->has_reset) {
		mutex_unlock(&lge_moca_devp->rx_lock);
		return lge_moca_notify_reset(lge_moca_devp);
	}

	if (!lge_moca_devp->ch) {
		mutex_unlock(&lge_moca_devp->rx_lock);
		pr_err("%s on a closed lge_moca_dev id:%d\n",
			__func__, lge_moca_devp->i);
		return -EINVAL;
	}

	if (r < 0) {
		mutex_unlock(&lge_moca_devp->rx_lock);
		/* qualify error message */
		if (r != -ERESTARTSYS) {
			/* we get this anytime a signal comes in */
			pr_err("%s: wait_event_interruptible on lge_moca_dev"
			       " id:%d ret %i\n",
				__func__, lge_moca_devp->i, r);
		}
		return r;
	}

	/* Here we have a whole packet waiting for us */
	pkt_size = smd_cur_packet_size(lge_moca_devp->ch);

	if (!pkt_size) {
		pr_err("%s: No data on lge_moca_dev id:%d, False wakeup\n",
			__func__, lge_moca_devp->i);
		mutex_unlock(&lge_moca_devp->rx_lock);
		goto wait_for_packet;
	}

	if (pkt_size > count) {
		pr_err("%s: failure on lge_moca_dev id: %d - packet size %d"
		       " > buffer size %d,", __func__, lge_moca_devp->i,
			pkt_size, count);
		mutex_unlock(&lge_moca_devp->rx_lock);
		return -ETOOSMALL;
	}

	bytes_read = 0;
	do {
		r = smd_read_user_buffer(lge_moca_devp->ch,
					 (buf + bytes_read),
					 (pkt_size - bytes_read));
		if (r < 0) {
			mutex_unlock(&lge_moca_devp->rx_lock);
			if (lge_moca_devp->has_reset)
				return lge_moca_notify_reset(lge_moca_devp);

			pr_err("%s Error while reading %d\n", __func__, r);
			return r;
		}
		bytes_read += r;
		if (pkt_size != bytes_read)
			wait_event(lge_moca_devp->ch_read_wait_queue,
				   smd_read_avail(lge_moca_devp->ch) ||
				   lge_moca_devp->has_reset);
		if (lge_moca_devp->has_reset) {
			mutex_unlock(&lge_moca_devp->rx_lock);
			return lge_moca_notify_reset(lge_moca_devp);
		}
	} while (pkt_size != bytes_read);
	mutex_unlock(&lge_moca_devp->rx_lock);

	mutex_lock(&lge_moca_devp->ch_lock);
	spin_lock_irqsave(&lge_moca_devp->pa_spinlock, flags);
	if (lge_moca_devp->poll_mode &&
	    !smd_cur_packet_size(lge_moca_devp->ch)) {
		wake_unlock(&lge_moca_devp->pa_wake_lock);
		lge_moca_devp->wakelock_locked = 0;
		lge_moca_devp->poll_mode = 0;
	}
	spin_unlock_irqrestore(&lge_moca_devp->pa_spinlock, flags);
	mutex_unlock(&lge_moca_devp->ch_lock);

	/* check and wakeup read threads waiting on this device */
	lge_moca_check_and_wakeup_reader(lge_moca_devp);

#ifdef MOCA_DEBUG
	pr_info(MOCA_MODULE_NAME ": %s: bytes_read = %d\n",
	__func__, bytes_read);
#endif

	return bytes_read;
}

int lge_moca_open(struct inode *inode, struct file *file)
{
	int r = 0;
	struct lge_moca_dev *lge_moca_devp;
	const char *peripheral = NULL;

	lge_moca_devp = container_of(inode->i_cdev, struct lge_moca_dev, cdev);

	if (!lge_moca_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return -EINVAL;
	}

	file->private_data = lge_moca_devp;

	mutex_lock(&lge_moca_devp->ch_lock);
	if (lge_moca_devp->ch == 0) {
		wake_lock_init(&lge_moca_devp->pa_wake_lock, WAKE_LOCK_SUSPEND,
				lge_moca_dev_name[lge_moca_devp->i]);
		INIT_WORK(&lge_moca_devp->packet_arrival_work,
				lge_moca_packet_arrival_worker);
		init_completion(&lge_moca_devp->ch_allocated);
		lge_moca_devp->driver.probe = lge_moca_dummy_probe;
		scnprintf(lge_moca_devp->pdriver_name, PDRIVER_NAME_MAX_SIZE,
			  "%s", smd_ch_name[lge_moca_devp->i]);
		lge_moca_devp->driver.driver.name = lge_moca_devp->pdriver_name;
		lge_moca_devp->driver.driver.owner = THIS_MODULE;
		r = platform_driver_register(&lge_moca_devp->driver);
		if (r) {
			pr_err("%s: %s Platform driver reg. failed\n",
				__func__, smd_ch_name[lge_moca_devp->i]);
			goto out;
		}

		peripheral = smd_edge_to_subsystem(
				smd_ch_edge[lge_moca_devp->i]);
		if (peripheral) {
			lge_moca_devp->pil = subsystem_get(peripheral);
			if (IS_ERR(lge_moca_devp->pil)) {
				r = PTR_ERR(lge_moca_devp->pil);
				pr_err("%s failed on lge_moca_dev id:%d"
					"- subsystem_get failed for %s\n",
					__func__, lge_moca_devp->i, peripheral);
				/*
				 * Sleep inorder to reduce the frequency of
				 * retry by user-space modules and to avoid
				 * possible watchdog bite.
				 */
				msleep((lge_moca_devp->open_modem_wait * 1000));
				goto release_pd;
			}

			/*
			 * Wait for a packet channel to be allocated so we know
			 * the modem is ready enough.
			 */
			if (lge_moca_devp->open_modem_wait) {
				r = wait_for_completion_interruptible_timeout(
					&lge_moca_devp->ch_allocated,
					msecs_to_jiffies(
						lge_moca_devp->open_modem_wait
							 * 1000));
				if (r == 0)
					r = -ETIMEDOUT;
				if (r < 0) {
					pr_err("%s: wait on lge_moca_dev id:%d"
					       " allocation failed rc:%d\n",
						__func__, lge_moca_devp->i, r);
					goto release_pil;
				}
			}
		}

		r = smd_named_open_on_edge(smd_ch_name[lge_moca_devp->i],
					   smd_ch_edge[lge_moca_devp->i],
					   &lge_moca_devp->ch,
					   lge_moca_devp,
					   lge_moca_ch_notify);
		if (r < 0) {
			pr_err("%s: %s open failed %d\n", __func__,
			       smd_ch_name[lge_moca_devp->i], r);
			goto release_pil;
		}

		r = wait_event_interruptible_timeout(
				lge_moca_devp->ch_opened_wait_queue,
				lge_moca_devp->is_open, (2 * HZ));
		if (r == 0) {
			r = -ETIMEDOUT;
			/* close the ch to sync smd's state with lge_moca */
			smd_close(lge_moca_devp->ch);
			lge_moca_devp->ch = NULL;
		}

		if (r < 0) {
			pr_err("%s: wait on lge_moca_dev id:%d"
				" OPEN event failed"
			       " rc:%d\n", __func__, lge_moca_devp->i, r);
		} else if (!lge_moca_devp->is_open) {
			pr_err("%s: Invalid OPEN event on lge_moca_dev id:%d\n",
				__func__, lge_moca_devp->i);
			r = -ENODEV;
		} else {
			smd_disable_read_intr(lge_moca_devp->ch);
			lge_moca_devp->ch_size =
				smd_write_avail(lge_moca_devp->ch);
			r = 0;
			lge_moca_devp->ref_cnt++;
		}
	} else {
		lge_moca_devp->ref_cnt++;
	}
release_pil:
	if (peripheral && (r < 0))
		subsystem_put(lge_moca_devp->pil);

release_pd:
	if (r < 0) {
		platform_driver_unregister(&lge_moca_devp->driver);
		lge_moca_devp->driver.probe = NULL;
	}
out:
	if (!lge_moca_devp->ch)
		wake_lock_destroy(&lge_moca_devp->pa_wake_lock);

	mutex_unlock(&lge_moca_devp->ch_lock);

#ifdef MOCA_DEBUG
	pr_info(MOCA_MODULE_NAME ": %s: MOCA device open\n", __func__);
#endif

	return r;
}

int lge_moca_release(struct inode *inode, struct file *file)
{
	int r = 0;
	struct lge_moca_dev *lge_moca_devp = file->private_data;

	if (!lge_moca_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&lge_moca_devp->ch_lock);
	mutex_lock(&lge_moca_devp->rx_lock);
	mutex_lock(&lge_moca_devp->tx_lock);
	if (lge_moca_devp->ref_cnt > 0)
		lge_moca_devp->ref_cnt--;

	if (lge_moca_devp->ch != 0 && lge_moca_devp->ref_cnt == 0) {
		lge_moca_clean_and_signal(lge_moca_devp);
		r = smd_close(lge_moca_devp->ch);
		lge_moca_devp->ch = 0;
		lge_moca_devp->blocking_write = 0;
		lge_moca_devp->poll_mode = 0;
		platform_driver_unregister(&lge_moca_devp->driver);
		lge_moca_devp->driver.probe = NULL;
		if (lge_moca_devp->pil)
			subsystem_put(lge_moca_devp->pil);
		lge_moca_devp->has_reset = 0;
		lge_moca_devp->do_reset_notification = 0;
		lge_moca_devp->wakelock_locked = 0;
		wake_lock_destroy(&lge_moca_devp->pa_wake_lock);
	}
	mutex_unlock(&lge_moca_devp->tx_lock);
	mutex_unlock(&lge_moca_devp->rx_lock);
	mutex_unlock(&lge_moca_devp->ch_lock);

#ifdef MOCA_DEBUG
	pr_info(MOCA_MODULE_NAME ": %s: MOCA device release\n", __func__);
#endif

	return r;
}

static const struct file_operations lge_moca_fops = {
	.owner = THIS_MODULE,
	.open = lge_moca_open,
	.release = lge_moca_release,
	.read = lge_moca_read,
	.write = lge_moca_write,
	.poll = lge_moca_poll,
	.unlocked_ioctl = lge_moca_ioctl,
};

static int __init lge_moca_drv_init(void)
{
	int i;
	int r;

	r = alloc_chrdev_region(&lge_moca_number,
			       0,
			       NUM_LGE_MOCA_PORTS,
			       LGE_MOCA_DEVICE_NAME);
	if (IS_ERR_VALUE(r)) {
		pr_err("%s: alloc_chrdev_region() failed ret:%i\n",
		       __func__, r);
		goto error0;
	}

	lge_moca_classp = class_create(THIS_MODULE, LGE_MOCA_DEVICE_NAME);
	if (IS_ERR(lge_moca_classp)) {
		pr_err("%s: class_create() failed ENOMEM\n", __func__);
		r = -ENOMEM;
		goto error1;
	}

	for (i = 0; i < NUM_LGE_MOCA_PORTS; ++i) {
		lge_moca_devp[i] = kzalloc(sizeof(struct lge_moca_dev),
					 GFP_KERNEL);
		if (IS_ERR(lge_moca_devp[i])) {
			pr_err("%s: kzalloc() failed for smd_pkt_dev id:%d\n",
				__func__, i);
			r = -ENOMEM;
			goto error2;
		}

		lge_moca_devp[i]->i = i;

		init_waitqueue_head(&lge_moca_devp[i]->ch_read_wait_queue);
		init_waitqueue_head(&lge_moca_devp[i]->ch_write_wait_queue);
		lge_moca_devp[i]->is_open = 0;
		lge_moca_devp[i]->poll_mode = 0;
		lge_moca_devp[i]->wakelock_locked = 0;
		lge_moca_devp[i]->open_modem_wait = 10;
		init_waitqueue_head(&lge_moca_devp[i]->ch_opened_wait_queue);

		spin_lock_init(&lge_moca_devp[i]->pa_spinlock);
		mutex_init(&lge_moca_devp[i]->ch_lock);
		mutex_init(&lge_moca_devp[i]->rx_lock);
		mutex_init(&lge_moca_devp[i]->tx_lock);

		cdev_init(&lge_moca_devp[i]->cdev, &lge_moca_fops);
		lge_moca_devp[i]->cdev.owner = THIS_MODULE;

		r = cdev_add(&lge_moca_devp[i]->cdev,
			     (lge_moca_number + i),
			     1);

		if (IS_ERR_VALUE(r)) {
			pr_err("%s: cdev_add() failed for smd_pkt_dev id:%d"
			       " ret:%i\n", __func__, i, r);
			kfree(lge_moca_devp[i]);
			goto error2;
		}

		lge_moca_devp[i]->devicep =
			device_create(lge_moca_classp,
				      NULL,
				      (lge_moca_number + i),
				      NULL,
				      lge_moca_dev_name[i]);

		if (IS_ERR(lge_moca_devp[i]->devicep)) {
			pr_err("%s: device_create() failed for lge_moca_dev"
			       " id:%d\n", __func__, i);
			r = -ENOMEM;
			cdev_del(&lge_moca_devp[i]->cdev);
			kfree(lge_moca_devp[i]);
			goto error2;
		}

		if (device_create_file(lge_moca_devp[i]->devicep,
					&dev_attr_open_timeout))
			pr_err("%s: unable to create device attr for"
			       " lge_moca_dev id:%d\n", __func__, i);
	}

	return 0;

 error2:
	if (i > 0) {
		while (--i >= 0) {
			cdev_del(&lge_moca_devp[i]->cdev);
			kfree(lge_moca_devp[i]);
			device_destroy(lge_moca_classp,
				       MKDEV(MAJOR(lge_moca_number), i));
		}
	}

	class_destroy(lge_moca_classp);
 error1:
	unregister_chrdev_region(MAJOR(lge_moca_number), NUM_LGE_MOCA_PORTS);
 error0:
	return r;
}

static void __exit lge_moca_cleanup(void)
{
	int i;

	for (i = 0; i < NUM_LGE_MOCA_PORTS; ++i) {
		cdev_del(&lge_moca_devp[i]->cdev);
		kfree(lge_moca_devp[i]);
		device_destroy(lge_moca_classp,
			       MKDEV(MAJOR(lge_moca_number), i));
	}

	class_destroy(lge_moca_classp);

	unregister_chrdev_region(MAJOR(lge_moca_number), NUM_LGE_MOCA_PORTS);
}

module_init(lge_moca_drv_init);
module_exit(lge_moca_cleanup);

MODULE_DESCRIPTION("LGE MOCA DRV");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Youngjin Park <jin.park@lge.com>");
