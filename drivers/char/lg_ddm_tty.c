/*
 * DDM TTY Driver for DDM application
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
*/

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/current.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <mach/msm_smd.h>
#include <mach/subsystem_restart.h>
#include <linux/cdev.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>

#define DDM_TTY_MODULE_NAME		"DDM_TTY"
#define MAX_DDM_TTY_DRV		2

//#define DDM_TTY_DEBUG

struct lge_ddm_tty_info {
	smd_channel_t *ch;
	struct tty_struct *tty;
	struct wake_lock wake_lock;
	int open_count;
	struct tasklet_struct tty_tsklt;
	struct timer_list buf_req_timer;
	struct completion ch_allocated;
	struct platform_driver driver;
	void *pil;
	int in_reset;
	int in_reset_updated;
	int is_open;
	int is_dsmodem_ready;
	wait_queue_head_t ch_opened_wait_queue;
	spinlock_t reset_lock;
	struct lge_ddm_config *smd;
};

static struct lge_ddm_tty_info lge_ddm_tty[MAX_DDM_TTY_DRV];
static struct tty_driver *lge_ddm_tty_driver;

struct lge_ddm_config {
	uint32_t tty_dev_index;
	const char *port_name;
	const char *dev_name;
	uint32_t edge;
};

static struct lge_ddm_config lge_ddm_configs[] = {
	{0, "DATA11", NULL, SMD_APPS_MODEM},
};

#define MAX_CH_NAME_LEN 20 /* includes null char at end */

static DEFINE_MUTEX(lge_ddm_tty_lock);

static uint lge_ddm_tty_modem_wait = 10;
static uint lge_ds_modem_wait = 20;

#define MAX_DDM_TTY_BUF_SIZE 4095

static void lge_ddm_tty_unthrottle(struct tty_struct *tty)
{
	struct lge_ddm_tty_info *info = tty->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&info->reset_lock, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock, flags);
		tasklet_hi_schedule(&info->tty_tsklt);
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock, flags);

	return;
}

static int lge_ddm_tty_write_room(struct tty_struct *tty)
{
	struct lge_ddm_tty_info *info = tty->driver_data;
	return smd_write_avail(info->ch);
}

static int lge_ddm_tty_is_in_reset(struct lge_ddm_tty_info *info)
{
	return info->in_reset;
}

static void lge_ddm_tty_notify(void *priv, unsigned event)
{
	struct lge_ddm_tty_info *info = priv;
	unsigned long flags;

	switch (event) {
	case SMD_EVENT_DATA:
		spin_lock_irqsave(&info->reset_lock, flags);
		if (!info->is_open) {
			spin_unlock_irqrestore(&info->reset_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&info->reset_lock, flags);
		/* There may be clients (tty framework) that are blocked
		 * waiting for space to write data, so if a possible read
		 * interrupt came in wake anyone waiting and disable the
		 * interrupts
		 */
		if (smd_write_avail(info->ch)) {
			smd_disable_read_intr(info->ch);
			if (info->tty)
				wake_up_interruptible(&info->tty->write_wait);
		}
		tasklet_hi_schedule(&info->tty_tsklt);
		break;

	case SMD_EVENT_OPEN:
		spin_lock_irqsave(&info->reset_lock, flags);
		info->in_reset = 0;
		info->in_reset_updated = 1;
		info->is_open = 1;
		wake_up_interruptible(&info->ch_opened_wait_queue);
		spin_unlock_irqrestore(&info->reset_lock, flags);
		break;

	case SMD_EVENT_CLOSE:
		spin_lock_irqsave(&info->reset_lock, flags);
		info->in_reset = 1;
		info->in_reset_updated = 1;
		info->is_open = 0;
		wake_up_interruptible(&info->ch_opened_wait_queue);
		spin_unlock_irqrestore(&info->reset_lock, flags);
		/* schedule task to send TTY_BREAK */
		tasklet_hi_schedule(&info->tty_tsklt);
		break;

	case SMD_EVENT_REOPEN_READY:
		/* smd channel is closed completely */
		spin_lock_irqsave(&info->reset_lock, flags);
		info->in_reset = 1;
		info->in_reset_updated = 1;
		info->is_open = 0;
		wake_up_interruptible(&info->ch_opened_wait_queue);
		spin_unlock_irqrestore(&info->reset_lock, flags);
		break;

	}

	return;

}

static int lge_ddm_tty_write(struct tty_struct *tty, const unsigned char *buf,
	int count)
{

	struct lge_ddm_tty_info *info = tty->driver_data;
	int avail, result = 0;

	/* if we're writing to a packet channel we will
	** never be able to write more data than there
	** is currently space for
	*/
	if (lge_ddm_tty_is_in_reset(info))
		return -ENETRESET;

	avail = smd_write_avail(info->ch);
	/* if no space, we'll have to setup a notification later to wake up the
	 * tty framework when space becomes avaliable
	 */
	if (!avail) {
		smd_enable_read_intr(info->ch);
		return 0;
	}

	if (count > avail)
		count = avail;

	result = smd_write(info->ch, buf, count);

#ifdef DDM_TTY_DEBUG
	pr_info(DDM_TTY_MODULE_NAME ": %s: data = %d\n", __func__, result);
#endif

	return result;

}

static void lge_ddm_tty_read(unsigned long param)
{
	unsigned char *ptr;
	int avail;
	struct lge_ddm_tty_info *info = (struct lge_ddm_tty_info *)param;
	struct tty_struct *tty = info->tty;

	if (!tty)
		return;

	for (;;) {
		if (lge_ddm_tty_is_in_reset(info)) {
			/* signal TTY clients using TTY_BREAK */
			tty_insert_flip_char(tty, 0x00, TTY_BREAK);
			tty_flip_buffer_push(tty);
			break;
		}

		if (test_bit(TTY_THROTTLED, &tty->flags)) break;

		avail = smd_read_avail(info->ch);

		if (avail == 0)
			break;

		if (avail > MAX_DDM_TTY_BUF_SIZE)
			avail = MAX_DDM_TTY_BUF_SIZE;

		avail = tty_prepare_flip_string(tty, &ptr, avail);
		if (avail <= 0) {
			mod_timer(&info->buf_req_timer,
					jiffies + msecs_to_jiffies(30));
			return;
		}

		if (smd_read(info->ch, ptr, avail) != avail) {
			/* shouldn't be possible since we're in interrupt
			** context here and nobody else could 'steal' our
			** characters.
			*/
			printk(KERN_ERR "OOPS - lge_ddm_tty_buffer mismatch?!");
		}

		wake_lock_timeout(&info->wake_lock, HZ / 2);

		tty_flip_buffer_push(tty);

#ifdef DDM_TTY_DEBUG
		pr_info(DDM_TTY_MODULE_NAME ": %s: data = %d\n", __func__, avail);
#endif

	}

	/* XXX only when writable and necessary */
	tty_wakeup(tty);

	return;

}

static int lge_ddm_tty_open(struct tty_struct *tty, struct file *file)
{

	int res = 0;
	struct lge_ddm_tty_info *info;
	const char *peripheral = NULL;
	unsigned int n = tty->index;

	if (n >= MAX_DDM_TTY_DRV || !lge_ddm_tty[n].smd)
		return -ENODEV;

	info = lge_ddm_tty + n;
	
	mutex_lock(&lge_ddm_tty_lock);
	tty->driver_data = info;

	if (info->open_count++ == 0) {
		peripheral = smd_edge_to_subsystem(lge_ddm_tty[n].smd->edge);
	
		if (peripheral) {
			info->pil = subsystem_get(peripheral);
			if (IS_ERR(info->pil)) {
				res = PTR_ERR(info->pil);
				goto out;
			}

			/*
			 * Wait for a channel to be allocated so we know
			 * the modem is ready enough.
			 */
			if (lge_ddm_tty_modem_wait) {
				res = wait_for_completion_interruptible_timeout(
					&info->ch_allocated,
					msecs_to_jiffies(lge_ddm_tty_modem_wait *
									1000));

				if (res == 0) {
					pr_err("Timed out waiting for SMD"
								" channel\n");
					res = -ETIMEDOUT;
					goto release_pil;

				} else if (res < 0) {
					pr_err("Error waiting for SMD channel:"
									" %d\n",
						res);
					goto release_pil;
				}
			
				res = 0;
			}

			/* wait for open ready status in seconds */
			pr_info("%s: checking modem status\n", __func__);
			res = wait_event_interruptible_timeout(
					info->ch_opened_wait_queue,
					info->is_dsmodem_ready, (lge_ds_modem_wait * HZ));

			if (res == 0) {
				res = -ETIMEDOUT;
				pr_err("%s: timeout to wait for %s modem: %d\n",
						__func__, lge_ddm_tty[n].smd->port_name, res);
				goto release_pil;
			}

			if (res < 0) {
				pr_err("%s: timeout to wait for %s modem: %d\n",
						__func__, lge_ddm_tty[n].smd->port_name, res);
				goto release_pil;
			}

			pr_info("%s: modem is OK, open..\n", __func__);
			
		}

		info->tty = tty;

		tasklet_init(&info->tty_tsklt, lge_ddm_tty_read,
				 (unsigned long)info);

		wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND,
				lge_ddm_tty[n].smd->port_name);

		if (!info->ch) {
			res = smd_named_open_on_edge(lge_ddm_tty[n].smd->port_name,
							lge_ddm_tty[n].smd->edge,
							&info->ch, info,
							lge_ddm_tty_notify);

			if (res < 0) {
				pr_err("%s: %s open failed %d\n", __func__,
					lge_ddm_tty[n].smd->port_name, res);
				goto release_pil;
			}

			res = wait_event_interruptible_timeout(
				info->ch_opened_wait_queue,
				info->is_open, (20 * HZ));

			if (res == 0)
				res = -ETIMEDOUT;

			if (res < 0) {
				pr_err("%s: wait for %s smd_open failed %d\n",
					__func__, lge_ddm_tty[n].smd->port_name,
					res);
				goto release_pil;
			}

			res = 0;

		}

	}

	/* support max = 64KB */
	set_bit(TTY_NO_WRITE_SPLIT, &(info->tty)->flags);

#ifdef DDM_TTY_DEBUG
	pr_info(DDM_TTY_MODULE_NAME ": %s: TTY device open\n", __func__);
#endif

release_pil:
	if (res < 0)
		subsystem_put(info->pil);
	else
		smd_disable_read_intr(info->ch);
out:
	mutex_unlock(&lge_ddm_tty_lock);

	return res;

}

static void lge_ddm_tty_close(struct tty_struct *tty, struct file *file)
{

	struct lge_ddm_tty_info *info = tty->driver_data;
	unsigned long flags;
	int res = 0;
	int n = tty->index;

	if (info == 0)
		return;

	clear_bit(TTY_NO_WRITE_SPLIT, &(info->tty)->flags);

	mutex_lock(&lge_ddm_tty_lock);
	if (--info->open_count == 0) {
		spin_lock_irqsave(&info->reset_lock, flags);
		info->is_open = 0;
		spin_unlock_irqrestore(&info->reset_lock, flags);
		if (info->tty) {
			tasklet_kill(&info->tty_tsklt);
			wake_lock_destroy(&info->wake_lock);
			info->tty = 0;
		}

		tty->driver_data = 0;
		del_timer(&info->buf_req_timer);

		if (info->ch) {
			smd_close(info->ch);

			pr_info("%s: waiting to close smd %s completely\n",
					__func__, lge_ddm_tty[n].smd->port_name);

			/* wait for reopen ready status in seconds */
			res = wait_event_interruptible_timeout(
				info->ch_opened_wait_queue,
				!info->is_open, (lge_ds_modem_wait * HZ));

			if (res == 0) {
				/* just in case, remain result value */
				res = -ETIMEDOUT;
				pr_err("%s: timeout to wait for %s smd_close.\
						next smd_open may fail....%d\n",
						__func__, lge_ddm_tty[n].smd->port_name, res);
			}

			if (res < 0) {
				pr_err("%s: wait for %s smd_close failed.\
						next smd_open may fail....%d\n",
						__func__, lge_ddm_tty[n].smd->port_name, res);
			}

			info->ch = 0;
			subsystem_put(info->pil);

		}
	}
	mutex_unlock(&lge_ddm_tty_lock);

#ifdef DDM_TTY_DEBUG
	pr_info(DDM_TTY_MODULE_NAME ": %s: TTY device closed\n", __func__);
#endif

	return;

}

static int lge_ddm_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
	unsigned long arg)
{
	return 0;

}

static const struct tty_operations lge_ddm_tty_ops = {
	.open = lge_ddm_tty_open,
	.close = lge_ddm_tty_close,
	.write = lge_ddm_tty_write,
	.write_room = lge_ddm_tty_write_room,
	.unthrottle = lge_ddm_tty_unthrottle,
	.ioctl = lge_ddm_tty_ioctl,
};

static void lge_ddm_buf_req_retry(unsigned long param)
{
	struct lge_ddm_tty_info *info = (struct lge_ddm_tty_info *)param;
	unsigned long flags;

	spin_lock_irqsave(&info->reset_lock, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock, flags);
		tasklet_hi_schedule(&info->tty_tsklt);
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock, flags);
}

static int lge_ddm_tty_probe(struct platform_device *pdev)
{

	int n;
	int idx;

	for (n = 0; n < ARRAY_SIZE(lge_ddm_configs); ++n) {
		idx = lge_ddm_configs[n].tty_dev_index;

		if (!lge_ddm_configs[n].dev_name)
			continue;

		if (pdev->id == lge_ddm_configs[n].edge &&
			!strncmp(pdev->name, lge_ddm_configs[n].dev_name,
					MAX_CH_NAME_LEN)) {
			complete_all(&lge_ddm_tty[idx].ch_allocated);

			lge_ddm_tty[idx].is_dsmodem_ready = 1;
			wake_up_interruptible(&lge_ddm_tty[idx].ch_opened_wait_queue);
			pr_info("%s: lge_ddm_tty_probe is ok\n", __func__);
			return 0;
		}

	}

	pr_err("%s: unknown device '%s'\n", __func__, pdev->name);

	return -ENODEV;
}

static int __init lge_ddm_tty_init(void)
{
	int ret;
	int n;
	int idx;

	lge_ddm_tty_driver = alloc_tty_driver(MAX_DDM_TTY_DRV);

	if (lge_ddm_tty_driver == 0)
		return -ENOMEM;

	lge_ddm_tty_driver->name = "lge_ddm_tty";
	lge_ddm_tty_driver->owner = THIS_MODULE;
	lge_ddm_tty_driver->driver_name = "lge_ddm_tty_driver";
	lge_ddm_tty_driver->major = 0;
	lge_ddm_tty_driver->minor_start = 0;
	/* uses dynamically assigned dev_t values */
	lge_ddm_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	lge_ddm_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	lge_ddm_tty_driver->flags = TTY_DRIVER_REAL_RAW
		| TTY_DRIVER_DYNAMIC_DEV
		| TTY_DRIVER_RESET_TERMIOS;

	/* initializing the tty driver */
	lge_ddm_tty_driver->init_termios = tty_std_termios;
	lge_ddm_tty_driver->init_termios.c_iflag = IGNBRK | IGNPAR;
	lge_ddm_tty_driver->init_termios.c_oflag = 0;
	lge_ddm_tty_driver->init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	lge_ddm_tty_driver->init_termios.c_lflag = 0;

	tty_set_operations(lge_ddm_tty_driver, &lge_ddm_tty_ops);

	ret = tty_register_driver(lge_ddm_tty_driver);

	if (ret) {
		put_tty_driver(lge_ddm_tty_driver);
		pr_err("%s: driver registration failed %d\n", __func__, ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(lge_ddm_configs); ++n) {
		idx = lge_ddm_configs[n].tty_dev_index;

		if (lge_ddm_configs[n].dev_name == NULL)
			lge_ddm_configs[n].dev_name = lge_ddm_configs[n].port_name;


		tty_register_device(lge_ddm_tty_driver, idx, 0);
		init_completion(&lge_ddm_tty[idx].ch_allocated);

		lge_ddm_tty[idx].driver.probe = lge_ddm_tty_probe;

		lge_ddm_tty[idx].driver.driver.name = lge_ddm_configs[n].dev_name;
		lge_ddm_tty[idx].driver.driver.owner = THIS_MODULE;
		spin_lock_init(&lge_ddm_tty[idx].reset_lock);
		lge_ddm_tty[idx].is_open = 0;

		setup_timer(&lge_ddm_tty[idx].buf_req_timer, lge_ddm_buf_req_retry,
				(unsigned long)&lge_ddm_tty[idx]);
		init_waitqueue_head(&lge_ddm_tty[idx].ch_opened_wait_queue);
		ret = platform_driver_register(&lge_ddm_tty[idx].driver);
		
		if (ret) {
			pr_err("%s: init failed %d\n", __func__, ret);
			lge_ddm_tty[idx].driver.probe = NULL;
			goto out;
		}

		lge_ddm_tty[idx].smd = &lge_ddm_configs[n];
	}
	return 0;

out:
	/* unregister platform devices */
	for (n = 0; n < ARRAY_SIZE(lge_ddm_configs); ++n) {
		idx = lge_ddm_configs[n].tty_dev_index;

		if (lge_ddm_tty[idx].driver.probe) {
			platform_driver_unregister(&lge_ddm_tty[idx].driver);
			tty_unregister_device(lge_ddm_tty_driver, idx);
		}
	}

	tty_unregister_driver(lge_ddm_tty_driver);
	put_tty_driver(lge_ddm_tty_driver);

	return ret;

}

static void __exit lge_ddm_tty_exit(void)
{
	int ret = 0;
	int n;
	int idx;

	for (n = 0; n < ARRAY_SIZE(lge_ddm_configs); ++n) {
		idx = lge_ddm_configs[n].tty_dev_index;

		lge_ddm_tty[idx].driver.probe = NULL;
		platform_driver_unregister(&lge_ddm_tty[idx].driver);

		tty_unregister_device(lge_ddm_tty_driver, 0);

		ret = tty_unregister_driver(lge_ddm_tty_driver);

		if (ret) {
			pr_err(DDM_TTY_MODULE_NAME ": %s: "
			    "tty_unregister_driver() failed\n", __func__);
		}
	}

	put_tty_driver(lge_ddm_tty_driver);
	lge_ddm_tty_driver = NULL;

	return;

}

module_init(lge_ddm_tty_init);
module_exit(lge_ddm_tty_exit);

MODULE_DESCRIPTION("LGE DDM TTY");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Youngjin Park <jin.park@lge.com>");
