/*
 * DM TTY Driver for LGE DM router
 *
 * Seongmook Yim <seongmook.yim@lge.com>
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

#ifdef CONFIG_LGE_DM_DEV
#include <linux/list.h>
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include <linux/diagchar.h>
#ifdef CONFIG_DIAG_SDIO_PIPE
#include "diagfwd_sdio.h"
#endif
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
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

#include "lg_dm_dev_tty.h"

#define DM_DEV_TTY_IOCTL_MAGIC		'K'
#define DM_DEV_TTY_MODEM_OPEN		_IOWR(DM_DEV_TTY_IOCTL_MAGIC, 0x01, short)
#define DM_DEV_TTY_MODEM_CLOSE		_IOWR(DM_DEV_TTY_IOCTL_MAGIC, 0x02, short)
#define DM_DEV_TTY_MODEM_STATUS 	_IOWR(DM_DEV_TTY_IOCTL_MAGIC, 0x03, short)
#define DM_DEV_TTY_DATA_TO_APP		_IOWR(DM_DEV_TTY_IOCTL_MAGIC, 0x04, short)
#define DM_DEV_TTY_DATA_TO_USB		_IOWR(DM_DEV_TTY_IOCTL_MAGIC, 0x05, short)

#define DM_DEV_TTY_MODULE_NAME		"DM_DEV"
#define MAX_DM_DEV_TTY_DRV		1

#define TRUE 1
#define FALSE 0


#define DM_DEV_TTY_TX_MAX_PACKET_SIZE   9000            /* max size = 9000B */
#define DM_DEV_TTY_RX_MAX_PACKET_SIZE   40000           /* max size = 40000B */

struct dm_dev_tty   *lge_dm_dev_tty;

/* modem_request_packet */
char  *dm_dev_modem_response;
int    dm_dev_modem_response_length;

/* modem_response_packet */
char  *dm_dev_modem_request;
int    dm_dev_modem_request_length;

/* modem chip */
enum {
	Primary_modem_chip = 1,
	Secondary_modem_chip = 2
};

/* TTY Driver status  */
enum {
    DM_DEV_TTY_INITIAL = 0,
    DM_DEV_TTY_REGISTERED =1,
    DM_DEV_TTY_OPEN = 2,
    DM_DEV_TTY_CLOSED = 3,
};

void lge_dm_dev_usb_fn(struct work_struct *work)
{
		usb_diag_write(driver->legacy_ch, driver->write_ptr_svc);
}

/*  Modem_request command */
static int lge_dm_dev_tty_modem_request(const unsigned char *buf, int count)
{
		/* send masks to modem */
		diag_process_hdlc((void *)buf, count);
//		pr_info(DM_DEV_TTY_MODULE_NAME ": %s:"
//		     "count = %d\n",__func__, count);

#if 0
	else {
          pr_info(DM_DEV_TTY_MODULE_NAME ": %s: lge_dm_dev_tty_write"
			"modem_number %d "
                    "error count = %d\n",
			__func__, modem_chip, count);
	}
#endif
	return count;
}


static int lge_dm_dev_tty_modem_response(struct dm_dev_tty *lge_dm_dev_tty_drv,
			const unsigned char *buf, int count)
{
	int num_push = 0;
	int total_push = 0;
	int left = 0;


	if (count == 0)
	{
		pr_info("diag:lge_dm_dev_tty_modem_response / count = %d\n",count);
		return 0;
	}

	left = count;

	do {
	num_push = tty_insert_flip_string(lge_dm_dev_tty_drv->tty_str,
		buf+total_push, left);
	total_push += num_push;
	left -= num_push;
	tty_flip_buffer_push(lge_dm_dev_tty_drv->tty_str);
	} while (left != 0);

	return total_push;
}

static int lge_dm_dev_tty_read_thread(void *data)
{
	int i = 0;
	int clear_read_wakelock;
	struct dm_dev_tty *lge_dm_dev_tty_drv = NULL;

	lge_dm_dev_tty_drv = lge_dm_dev_tty;


	while (1) {

		wait_event_interruptible(lge_dm_dev_tty->waitq,
			lge_dm_dev_tty->set_logging);
		
		mutex_lock(&driver->diagchar_mutex);
		clear_read_wakelock = 0;
		if ((lge_dm_dev_tty->set_logging == 1)
				&& (driver->logging_mode == DM_DEV_MODE)) {
			/* copy android data */
// Add Exception Handling seongmook.Yim 2013.05.15 - s
/*
			for (i = 0; i < driver->poolsize_write_struct;
				i++) {
				if (driver->buf_tbl[i].length > 0) {
					lge_dm_dev_tty_modem_response(
					lge_dm_dev_tty_drv,
					(void *)driver->buf_tbl[i].buf,
					driver->buf_tbl[i].length);

					diagmem_free(driver, (unsigned char *)(driver->buf_tbl[i].buf), POOL_TYPE_HDLC);
					driver->buf_tbl[i].length = 0;
					driver->buf_tbl[i].buf = 0;
				}
*/
// Add Exception Handling seongmook.Yim 2013.05.15 - e
				/* copy modem data */
			for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {// woojin
				struct diag_smd_info *data = &driver->smd_data[i];//woojin

				if (data->in_busy_1 == 1) {
					lge_dm_dev_tty_modem_response(
					lge_dm_dev_tty_drv,
					data->buf_in_1,
					data->write_ptr_1->length);

					if (!driver->real_time_mode) {
					process_lock_on_copy(&data->nrt_lock);
					clear_read_wakelock++;
					}
					
					data->in_busy_1 = 0;
				}

				if (data->in_busy_2 == 1) {
					lge_dm_dev_tty_modem_response(
					lge_dm_dev_tty_drv,
					data->buf_in_2,
					data->write_ptr_2->length);

					if (!driver->real_time_mode) {
					process_lock_on_copy(&data->nrt_lock);
					clear_read_wakelock++;
					}
					
					data->in_busy_2 = 0;
					}
				}

			lge_dm_dev_tty->set_logging = 0;

		for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
			if (driver->smd_data[i].ch)
					queue_work(driver->diag_wq,
						&(driver->smd_data[i].diag_read_smd_work));
				}			
			}
			if (clear_read_wakelock) {
				for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++)
					process_lock_on_copy_complete(
						&driver->smd_data[i].nrt_lock);
			}

		mutex_unlock(&driver->diagchar_mutex);

		if (kthread_should_stop())
			break;
			
		mdelay(1);

	}

	return 0;

}

static void lge_dm_dev_tty_unthrottle(struct tty_struct *tty)
{
	return;
}

static int lge_dm_dev_tty_write_room(struct tty_struct *tty)
{

    return DM_DEV_TTY_TX_MAX_PACKET_SIZE;
}

static int lge_dm_dev_tty_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
  int result;
  struct dm_dev_tty *lge_dm_dev_tty_drv = NULL;

  lge_dm_dev_tty_drv = lge_dm_dev_tty;
  tty->driver_data = lge_dm_dev_tty_drv;
  lge_dm_dev_tty_drv -> tty_str = tty;


	/* check the packet size */
    if(count > DM_DEV_TTY_TX_MAX_PACKET_SIZE)
    {
        pr_info(DM_DEV_TTY_MODULE_NAME ": %s:"
		"lge_dm_dev_tty_write error count = %d\n",
			__func__, count);
		return -EPERM;
	}
    result = lge_dm_dev_tty_modem_request(buf,count);
	return result;

}

static int lge_dm_dev_tty_open(struct tty_struct *tty, struct file *file)
{
  struct dm_dev_tty *lge_dm_dev_tty_drv = NULL;

  pr_info(DM_DEV_TTY_MODULE_NAME ": %s: DEV_TTY device open\n", __func__);
  
  if(!tty)
  {
    pr_err(DM_DEV_TTY_MODULE_NAME ": %s: NULL tty", __func__);
		return -ENODEV;
	}

  lge_dm_dev_tty_drv = lge_dm_dev_tty;

  if(!lge_dm_dev_tty_drv)
  {
    pr_err(DM_DEV_TTY_MODULE_NAME "[mook]:%s:"
        "NULL lge_dm_dev_tty_drv", __func__);
		return -ENODEV;
	}

  tty->driver_data = lge_dm_dev_tty_drv;
  lge_dm_dev_tty_drv->tty_str = tty;

  if(lge_dm_dev_tty_drv->tty_state == DM_DEV_TTY_OPEN)
  {
    pr_err(DM_DEV_TTY_MODULE_NAME "[mook]:%s:"
		"tty is already open", __func__);
		return -EBUSY;
	}

	/* support max = 64KB */
  set_bit(TTY_NO_WRITE_SPLIT, &lge_dm_dev_tty_drv->tty_str->flags);

  lge_dm_dev_tty_drv->tty_ts = kthread_run(lge_dm_dev_tty_read_thread, NULL,"lge_dm_dev_tty_thread");


  lge_dm_dev_tty_drv->tty_state = DM_DEV_TTY_OPEN;


  lge_dm_dev_tty_drv->set_logging = 0;

  lge_dm_dev_tty_drv->dm_dev_wq = create_singlethread_workqueue("dm_dev_wq");
  INIT_WORK(&(lge_dm_dev_tty_drv->dm_dev_usb_work), lge_dm_dev_usb_fn);

	return 0;

}

static void lge_dm_dev_tty_close(struct tty_struct *tty, struct file *file)
{
	struct dm_dev_tty *lge_dm_dev_tty_drv = NULL;

	lge_dm_dev_tty_drv = lge_dm_dev_tty;
	tty->driver_data = lge_dm_dev_tty_drv;
	lge_dm_dev_tty_drv->tty_str = tty;

	clear_bit(TTY_NO_WRITE_SPLIT, &lge_dm_dev_tty_drv->tty_str->flags);

	// pr_err(DM_DEV_TTY_MODULE_NAME ": %s:", __func__);

	if (!tty) {
		pr_err(DM_DEV_TTY_MODULE_NAME "[mook] : %s: NULL tty", __func__);
		return;
	}

	lge_dm_dev_tty_drv = tty->driver_data;

	if (!lge_dm_dev_tty_drv) {
		pr_err(DM_DEV_TTY_MODULE_NAME "[mook] : %s: NULL sdio_tty_drv", __func__);
		return;
	}

	if (lge_dm_dev_tty_drv->tty_state != DM_DEV_TTY_OPEN) {
		pr_err(DM_DEV_TTY_MODULE_NAME "[mook] : %s: DEV_TTY device was not opened\n",
			__func__);
		return;
	}

	lge_dm_dev_tty->set_logging = 1;
	wake_up_interruptible(&lge_dm_dev_tty_drv->waitq);

	kthread_stop(lge_dm_dev_tty_drv->tty_ts);

	lge_dm_dev_tty_drv->tty_state = DM_DEV_TTY_CLOSED;

	pr_info(DM_DEV_TTY_MODULE_NAME ": %s: DEV_TTY device closed\n", __func__);

	cancel_work_sync(&(lge_dm_dev_tty_drv->dm_dev_usb_work));
	destroy_workqueue(lge_dm_dev_tty_drv->dm_dev_wq);


#if 0
		lge_dm_dev_tty->set_logging = 0;
		
		/* change path to USB driver */
		mutex_lock(&driver->diagchar_mutex);
		driver->logging_mode = USB_MODE;
		mutex_unlock(&driver->diagchar_mutex);

// Temporarily disabled - 0627
/*
		if (driver->usb_connected == 0)
			diagfwd_disconnect();
		else
			diagfwd_connect();
*/
// Temporarily disabled - 0627
#endif

	return;
}

static int lge_dm_dev_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
	unsigned long arg)
{
	//short modem_number, result;
	struct dm_dev_tty *lge_dm_dev_tty_drv = NULL;

	//int is_all_closed, 
	int i;

	lge_dm_dev_tty_drv = lge_dm_dev_tty;
	tty->driver_data = lge_dm_dev_tty_drv;
	lge_dm_dev_tty_drv->tty_str = tty;

	if (_IOC_TYPE(cmd) != DM_DEV_TTY_IOCTL_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case DM_DEV_TTY_MODEM_OPEN:

// Temporarily disabled - 0627
//        diagfwd_disconnect();
//		diagfwd_connect();
// Temporarily disabled - 0627

		/* change path to DM DEV */
		mutex_lock(&driver->diagchar_mutex);
		driver->logging_mode = DM_DEV_MODE;
		mutex_unlock(&driver->diagchar_mutex);

		for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {//woojin
		
				driver->smd_data[i].in_busy_1 = 0;
				driver->smd_data[i].in_busy_2 = 0;
		
				/* Poll SMD channels to check for data*/
				if (driver->smd_data[i].ch)
					queue_work(driver->diag_wq,
						&(driver->smd_data[i].
							diag_read_smd_work));

		}// end of for loop

		break;

	case DM_DEV_TTY_MODEM_CLOSE:
        
		lge_dm_dev_tty->set_logging = 0;

        pr_info(DM_DEV_TTY_MODULE_NAME ": %s:"
		     "driver->logging_mode ++ =  %d\n",__func__, driver->logging_mode);

		/* change path to USB driver */
		mutex_lock(&driver->diagchar_mutex);
		driver->logging_mode = USB_MODE;
		mutex_unlock(&driver->diagchar_mutex);
        
        pr_info(DM_DEV_TTY_MODULE_NAME ": %s:"
		     "driver->logging_mode -- =  %d\n",__func__, driver->logging_mode);

// Temporarily disabled - 0627
		if (driver->usb_connected == 0)
			diagfwd_disconnect();
		else
			diagfwd_connect();
// Temporarily disabled - 0627

      break;
		

	default:
		pr_info(DM_DEV_TTY_MODULE_NAME ": %s:"
		"lge_dm_tty_ioctl error\n", __func__);
		break;

	}

	return 0;

}

static const struct tty_operations lge_dm_dev_tty_ops = {
  .open = lge_dm_dev_tty_open,
  .close = lge_dm_dev_tty_close,
  .write = lge_dm_dev_tty_write,
  .write_room = lge_dm_dev_tty_write_room,
  .unthrottle = lge_dm_dev_tty_unthrottle,
  .ioctl = lge_dm_dev_tty_ioctl,
};

static int __init lge_dm_dev_tty_init(void)
{
    int ret = 0;
	struct device *tty_dev;
    struct dm_dev_tty *lge_dm_dev_tty_drv;

    pr_info(DM_DEV_TTY_MODULE_NAME ": %s\n",__func__);

    lge_dm_dev_tty_drv = kzalloc(sizeof(struct dm_dev_tty), GFP_KERNEL);
    if(lge_dm_dev_tty_drv == NULL)
    {
      pr_info(DM_DEV_TTY_MODULE_NAME ": %s:" 
                                     "failed to allocate lge_dm_dev_tty", __func__);
		return 0;
	}

    lge_dm_dev_tty = lge_dm_dev_tty_drv;

    lge_dm_dev_tty_drv -> tty_drv = alloc_tty_driver(MAX_DM_DEV_TTY_DRV);
    if(!lge_dm_dev_tty->tty_drv)
    {
      pr_info(DM_DEV_TTY_MODULE_NAME ": %s: - tty_drv is NULL", __func__);
      kfree(lge_dm_dev_tty_drv);
		return 0;
	}

    lge_dm_dev_tty_drv->tty_drv->name = "lge_dm_dev_tty";
    lge_dm_dev_tty_drv->tty_drv->owner = THIS_MODULE;
    lge_dm_dev_tty_drv->tty_drv->driver_name = "lge_dm_dev_tty";
	/* uses dynamically assigned dev_t values */
    lge_dm_dev_tty_drv->tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
    lge_dm_dev_tty_drv->tty_drv->subtype = SERIAL_TYPE_NORMAL;
    lge_dm_dev_tty_drv->tty_drv->flags = TTY_DRIVER_REAL_RAW
		| TTY_DRIVER_DYNAMIC_DEV
		| TTY_DRIVER_RESET_TERMIOS;

	/* initializing the tty driver */
    lge_dm_dev_tty_drv->tty_drv->init_termios = tty_std_termios;
    lge_dm_dev_tty_drv->tty_drv->init_termios.c_iflag = IGNBRK | IGNPAR;
    lge_dm_dev_tty_drv->tty_drv->init_termios.c_oflag = 0;
    lge_dm_dev_tty_drv->tty_drv->init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    lge_dm_dev_tty_drv->tty_drv->init_termios.c_lflag = 0;

    tty_set_operations(lge_dm_dev_tty_drv->tty_drv, &lge_dm_dev_tty_ops);

    ret = tty_register_driver(lge_dm_dev_tty_drv->tty_drv);

	if (ret) {
        put_tty_driver(lge_dm_dev_tty_drv->tty_drv);
        pr_info(DM_DEV_TTY_MODULE_NAME ": %s:"
		"tty_register_driver() ""failed\n",
			__func__);

        lge_dm_dev_tty_drv->tty_drv = NULL;
        kfree(lge_dm_dev_tty_drv);
		return 0;
	}

    tty_dev = tty_register_device(lge_dm_dev_tty_drv -> tty_drv, 0, NULL);

    if(IS_ERR(tty_dev))
    {
       pr_info(DM_DEV_TTY_MODULE_NAME ": %s:"
       "tty_register_driver() ""failed\n",
			__func__);
       tty_unregister_driver(lge_dm_dev_tty_drv -> tty_drv);
       put_tty_driver(lge_dm_dev_tty_drv -> tty_drv);
       kfree(lge_dm_dev_tty_drv);
		return 0;
	}

    init_waitqueue_head(&lge_dm_dev_tty_drv->waitq);

    lge_dm_dev_tty_drv -> tty_state = DM_DEV_TTY_REGISTERED;

	return 0;

}

static void __exit lge_dm_dev_tty_exit(void)
{
	int ret = 0;
    struct dm_dev_tty *lge_dm_dev_tty_drv = NULL;

    lge_dm_dev_tty_drv = lge_dm_dev_tty;

    if(!lge_dm_dev_tty_drv)
    {
      pr_err(DM_DEV_TTY_MODULE_NAME ":%s:"
          "NULL lge_dm_dev_tty_dev", __func__);
		return;
	}

    if(lge_dm_dev_tty_drv->tty_state != DM_DEV_TTY_INITIAL)
    {
      tty_unregister_device(lge_dm_dev_tty_drv->tty_drv, 0);
      ret = tty_unregister_driver(lge_dm_dev_tty_drv -> tty_drv);

      if(ret)
      {
        pr_err(DM_DEV_TTY_MODULE_NAME ": %s:"
			    "tty_unregister_driver() failed\n", __func__);
		}

      put_tty_driver(lge_dm_dev_tty_drv -> tty_drv);
      lge_dm_dev_tty_drv -> tty_state = DM_DEV_TTY_INITIAL;
      lge_dm_dev_tty_drv -> tty_drv = NULL;
	}

    kfree(lge_dm_dev_tty_drv);

	return;

}

module_init(lge_dm_dev_tty_init);
module_exit(lge_dm_dev_tty_exit);

MODULE_DESCRIPTION("LGE DM DEV TTY");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Seongmook Yim <seongmook.yim@lge.com>");
#endif /*CONFIG_LGE_DM_DEV*/
