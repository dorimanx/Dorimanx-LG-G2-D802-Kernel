#ifndef	__KERNEL__
#define	__KERNEL__
#endif

#include <linux/miscdevice.h>
#include <linux/module.h>	/* module */
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>	/* printk */
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

#include <linux/kernel.h>	/* printk */
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/sched.h>	/* task_struct */
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/delay.h>	/* sleep */
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/errno.h>

#include "broadcast_mmbi_chk_dev.h"

#define BROADCAST_MMBI_CHK_DEV_NUM_DEVS 	1

static struct class *broadcast_mmbi_chk_dev_class;
static dev_t broadcast_mmbi_chk_dev;

struct broadcast_mmbi_chk_dev_chdevice 
{
	struct cdev cdev;
	struct device *dev;
	wait_queue_head_t wq_read;
	void *cookie;
};

static struct broadcast_mmbi_chk_dev_chdevice mmbi_chk_dev;

static ioctl_monitor_info_t g_monitor_info = {0,};

static mmbi_chk_dev_uim_info_t g_uim_info;

static mmbi_chk_dev_antenna_info_t g_antenna_info = {0,};


/************************************************************************
 * open() system call.
 * [User Interface]
 *     int    open( const char "/dev/broadcast_chkdev0", O_RDWR );
 */
/**
	open System Call.
*/

static int broadcast_mmbi_chk_dev_open(struct inode *inode, struct file *filp)
{

	printk("broadcast_mmbi_chk_dev_open!!!\n");
	return 0;
}

/************************************************************************
 * release() .... close() system call.
 * [User Interface]
 *     int    close( int fd );
 */
/**
	close System Call.
*/

static int broadcast_mmbi_chk_dev_close(struct inode *inode, struct file *filp)
{
	printk("broadcast_mmbi_chk_dev_close!!!\n");
	return 0;
}

/************************************************************************
 * read() system call.
 * [User Interface]
 *     ssize_t    read( struct file *filp, char __user *buff, size_t count, loff_t *offp );
 */
/**
	read System Call.
*/

static ssize_t broadcast_mmbi_chk_dev_read(struct file *filp, char *buf, size_t count, loff_t * f_pos)
{
	printk("broadcast_mmbi_chk_dev_read!!!\n");
	return 0;
}

/************************************************************************
 * write() system call.
 * [User Interface]
 *     ssize_t    write( struct file *filp, char __user *buff, size_t count, loff_t *offp );
 */
/**
	write System Call.
*/
static int broadcast_mmbi_chk_dev_write(struct file *filp, const char __user * buf, size_t count, loff_t * f_pos)
{
	printk("broadcast_mmbi_chk_dev_write!!!\n");

	return 0;
}

/**
	System Call driver open.
*/
static int broadcast_mmbi_chk_dev_driver_open(unsigned long arg)
{
	ioctl_monitor_info_t *user_info = (ioctl_monitor_info_t *) arg;
	ioctl_monitor_info_t system_info;//
	size_t tmpsize = sizeof(ioctl_monitor_info_t);

	printk("broadcast_mmbi_chk_dev_driver_open!!!\n");

	g_monitor_info.channel = -1;
	g_monitor_info.rf_mode = -1;
	g_monitor_info.drv_openning = -1;
	memset(&g_monitor_info.sig_info, 0x00, sizeof(g_monitor_info.sig_info));

	if (copy_from_user(&system_info, (void *)user_info, tmpsize)) {
		printk("copy_from_user failed. (len:%d)\n",tmpsize);
		return -EFAULT;
	}

	g_monitor_info.drv_openning = 0;
	
	return 0;
}

/**
	System Call. driver close
*/
static int broadcast_mmbi_chk_dev_driver_close(unsigned long arg)
{
	//ioctl_monitor_info_t *user_info = (ioctl_monitor_info_t *) arg;
	//ioctl_monitor_info_t system_info;//
	//size_t tmpsize = sizeof(ioctl_monitor_info_t);

	printk("broadcast_mmbi_chk_dev_write!!!\n");

	g_monitor_info.channel = -1;
	g_monitor_info.rf_mode = -1;
	g_monitor_info.drv_openning = -1;
	memset(&g_monitor_info.sig_info, 0x00, sizeof(g_monitor_info.sig_info));
	
	return 0;
}

/**
	System Call. driver rf setting
*/
static int broadcast_mmbi_chk_dev_set_rf(unsigned long arg)
{
	ioctl_monitor_info_t *user_info = (ioctl_monitor_info_t *) arg;
	ioctl_monitor_info_t system_info;//
	size_t tmpsize = sizeof(ioctl_monitor_info_t);

	if (copy_from_user(&system_info, (void *)user_info, tmpsize)) {
		printk("copy_from_user failed. (len:%d)\n",tmpsize);
		return -EFAULT;
	}

	g_monitor_info.rf_mode = system_info.rf_mode;
	
	printk("broadcast_mmbi_chk_dev_set_rf %d !!!\n", g_monitor_info.rf_mode);
	
	return 0;
}


/**
	System Call. driver ch setting
*/
static int broadcast_mmbi_chk_dev_set_channel(unsigned long arg)
{
	ioctl_monitor_info_t *user_info = (ioctl_monitor_info_t *) arg;
	ioctl_monitor_info_t system_info;//
	size_t tmpsize = sizeof(ioctl_monitor_info_t);
	
	if (copy_from_user(&system_info, (void *)user_info, tmpsize)) {
		printk("copy_from_user failed. (len:%d)\n",tmpsize);
		return -EFAULT;
	}

	g_monitor_info.channel = system_info.channel;
	
	printk("broadcast_mmbi_chk_dev_set_channel %d!!!\n", g_monitor_info.channel);
	
	return 0;
}

/**
	System Call. driver monitor app. start
*/
static int broadcast_mmbi_chk_dev_set_monitor_app(unsigned long arg)
{
	ioctl_monitor_info_t *user_info = (ioctl_monitor_info_t *) arg;
	ioctl_monitor_info_t system_info;//
	size_t tmpsize = sizeof(ioctl_monitor_info_t);
	
	if (copy_from_user(&system_info, (void *)user_info, tmpsize)) {
		printk("copy_from_user failed. (len:%d)\n",tmpsize);
		return -EFAULT;
	}

	g_monitor_info.monitor_app = system_info.monitor_app;
	
	//printk("broadcast_mmbi_chk_dev_set_monitor_app %d!!!\n", g_monitor_info.monitor_app);
	
	return 0;
}


/**
	System Call. driver sig_info setting
*/
static int broadcast_mmbi_chk_dev_set_sig_info(unsigned long arg)
{
	ioctl_monitor_info_t *user_info = (ioctl_monitor_info_t *) arg;
	ioctl_monitor_info_t system_info;//
	size_t tmpsize = sizeof(ioctl_monitor_info_t);

	if (copy_from_user(&system_info, (void *)user_info, tmpsize)) {
		printk("copy_from_user failed. (len:%d)\n",tmpsize);
		return -EFAULT;
	}
	
	//printk("broadcast_mmbi_chk_dev_set_sig_info!!!\n");

	memcpy(&g_monitor_info.sig_info, &system_info.sig_info, sizeof(ioctl_sig_info_t));
	
	return 0;
}

/**
	System Call. driver sig_info getting
*/
static int broadcast_mmbi_chk_dev_get_sig_info(unsigned long arg)
{
	//ioctl_monitor_info_t *user_info = (ioctl_monitor_info_t *) arg;
	//ioctl_monitor_info_t system_info;//
	size_t tmpsize = sizeof(ioctl_monitor_info_t);

	//printk("broadcast_mmbi_chk_dev_get_sig_info!!!\n");

	//if (copy_from_user(&system_info, (void *)arg, tmpsize)) {
	//	printk("copy_from_user failed. (len:%d)\n",tmpsize);
	//	return -EFAULT;
	//}

	if (copy_to_user((void *)arg, (void *)&g_monitor_info, tmpsize)) 
	{
		return -EFAULT;
	}
	
	return 0;
}

static int broadcast_mmbi_chk_dev_set_uim_info(unsigned long arg)
{
	mmbi_chk_dev_uim_info_t *uim_info = (mmbi_chk_dev_uim_info_t *) arg;
	mmbi_chk_dev_uim_info_t uim_local_info;//
	unsigned char iccid_str[21];
	size_t tmpsize = sizeof(mmbi_chk_dev_uim_info_t);

	if (copy_from_user(&uim_local_info, (void *)uim_info, tmpsize)) {
		printk(KERN_DEBUG"copy_from_user failed. (len:%d)\n",tmpsize);
		return -EFAULT;
	}
	
	memcpy(g_uim_info.iccid, &uim_local_info.iccid, 20);

	memset(iccid_str, 0x00, 21);

	memcpy(iccid_str, g_uim_info.iccid, 20);

	printk(KERN_DEBUG"mmbi iccid info [%s]\n", iccid_str);
	
	return 0;
}

static int broadcast_mmbi_chk_dev_get_uim_info(unsigned long arg)
{
	if(copy_to_user((void*)arg, (void*)&g_uim_info, sizeof(mmbi_chk_dev_uim_info_t)))
	{
		return -EFAULT;
	}

	return 0;
}

//For Auto switching Antenna //0->default Retractable, 1->auto switching
static int broadcast_mmbi_chk_dev_set_antenna_info(unsigned long arg)
{
	mmbi_chk_dev_antenna_info_t *antenna_info = (mmbi_chk_dev_antenna_info_t *) arg;
	mmbi_chk_dev_antenna_info_t antenna_local_info;
	size_t tmpsize = sizeof(mmbi_chk_dev_antenna_info_t);

	if (copy_from_user(&antenna_local_info, (void *)antenna_info, tmpsize)) {
		printk(KERN_DEBUG"copy_from_user failed. (len:%d)\n",tmpsize);
		return -EFAULT;
	}
	
	g_antenna_info.antenna_info = antenna_local_info.antenna_info;
#ifdef MMB_CHK_DEV_EAR_ANTENNA
	isdbt_hw_set_antenna_mode(g_antenna_info.antenna_info);
#endif
	printk(KERN_DEBUG"mmbi antenna info [%d]\n", g_antenna_info.antenna_info);
	
	return 0;
}

/************************************************************************
 * ioctl() system call.
 * [User Interface]
 *     int    ioctl( int fd, unsigned long cmd, ... );
 *     int    ioctl( struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg );
 */
/**
	ioctl System Call..
*/
static long broadcast_mmbi_chk_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rtn = 0;
	
	switch (cmd) 
	{
		case IOCTL_DRV_OPEN:	
			rtn = broadcast_mmbi_chk_dev_driver_open(arg);
			break;

		case IOCTL_DRV_CLOSE:	
			rtn = broadcast_mmbi_chk_dev_driver_close(arg);
			break;

		case IOCTL_SET_RF_BAND:	
			rtn = broadcast_mmbi_chk_dev_set_rf(arg);
			break;

		case IOCTL_SET_CHANNEL:	
			rtn = broadcast_mmbi_chk_dev_set_channel(arg);	
			break;

		case IOCTL_SET_MONITOR:	
			rtn = broadcast_mmbi_chk_dev_set_sig_info(arg);	
			break;

		case IOCTL_GET_MONITOR:	
			rtn = broadcast_mmbi_chk_dev_get_sig_info(arg);
			break;

		case IOCTL_SET_MONITOR_APP:	
			rtn = broadcast_mmbi_chk_dev_set_monitor_app(arg);
			break;

		case IOCTL_SET_UIM_INFO:
			rtn = broadcast_mmbi_chk_dev_set_uim_info(arg);
			break;

		case IOCTL_GET_UIM_INFO:
			rtn = broadcast_mmbi_chk_dev_get_uim_info(arg);
			break;

		case IOCTL_SET_ANTENNA_INFO:
			rtn = broadcast_mmbi_chk_dev_set_antenna_info(arg);
			break;

		default:
			printk("broadcast_mmbi_chk_dev_ioctl : Illegal command operation. cmd[ 0x%04x ].\n", cmd);
		return -EBADRQC;
		break;
	}
	return rtn;
}

static struct file_operations broadcast_mmbi_chk_dev_fops = {
	owner:	THIS_MODULE,		/* owner */
	read:	broadcast_mmbi_chk_dev_read,		/* read() system call entry */
	write:	broadcast_mmbi_chk_dev_write,		/* write() system call entry */
	unlocked_ioctl:	broadcast_mmbi_chk_dev_ioctl,	/* unlocked_ioctl() system call entry */
	open:	broadcast_mmbi_chk_dev_open,		/* open() system call entry */
	release:broadcast_mmbi_chk_dev_close,		/* close() system call entry */
};

// LGE
static int broadcast_mmbi_chk_dev_device_init(struct broadcast_mmbi_chk_dev_chdevice *pbroadcast, int index)
{
	int rc;

	cdev_init(&pbroadcast->cdev, &broadcast_mmbi_chk_dev_fops);

	pbroadcast->cdev.owner = THIS_MODULE;

	rc = cdev_add(&pbroadcast->cdev, broadcast_mmbi_chk_dev, 1);

	pbroadcast->dev = device_create(broadcast_mmbi_chk_dev_class, NULL, MKDEV(MAJOR(broadcast_mmbi_chk_dev), 0),
					 NULL, "broadcast_chkdev%d", index);

	printk("broadcast_mmbi_chk_dev_device_init add add%d broadcast_mmbi_chk_dev = %d \n", rc, MKDEV(MAJOR(broadcast_mmbi_chk_dev), 0));
	
	if (IS_ERR(pbroadcast->dev)) {
		rc = PTR_ERR(pbroadcast->dev);
		pr_err("device_create failed: %d\n", rc);
		rc = -1;
	}
	
	printk("broadcast_mmbi_device_init start %d\n", rc);

	return rc;
}

int broadcast_mmbi_chk_dev_drv_start(void)
{
	struct broadcast_mmbi_chk_dev_chdevice *pbroadcast = NULL;
	int rc = -ENODEV;
	
	if (!broadcast_mmbi_chk_dev_class) {

		broadcast_mmbi_chk_dev_class = class_create(THIS_MODULE, "broadcast_mmbi_chkdev");
		if (IS_ERR(broadcast_mmbi_chk_dev_class)) {
			rc = PTR_ERR(broadcast_mmbi_chk_dev_class);
			pr_err("broadcast_mmbi_chk_dev_class: create device class failed: %d\n",
				rc);
			return rc;
		}

		rc = alloc_chrdev_region(&broadcast_mmbi_chk_dev, 0, BROADCAST_MMBI_CHK_DEV_NUM_DEVS, "broadcast_mmbi_chkdev");
		printk("broadcast_mmbi_chk_dev_drv_start add add%d broadcast_mmbi_chk_dev = %d \n", rc, broadcast_mmbi_chk_dev);
		if (rc < 0) {
			pr_err("broadcast_mmbi_chkdev class: failed to allocate chrdev: %d\n",
				rc);
			return rc;
		}
	}

	pbroadcast = &mmbi_chk_dev;
	
	rc = broadcast_mmbi_chk_dev_device_init(pbroadcast, 0);
	if (rc < 0) {
		return rc;
	}
	
	printk("broadcast_mmbi_chk_dev_drv_start start %d\n", rc);

	return rc;
}
EXPORT_SYMBOL(broadcast_mmbi_chk_dev_drv_start);


/************************************************************************
 * initialization module
 *  called by insmod function.
 */
/**
	__init System Call. (called by Kernel.)
*/
static int __init proc_init_module(void)
{
	broadcast_mmbi_chk_dev_drv_start();
	return 0;
}

/************************************************************************
 * cleanup module.
 *  called  by rmmod function.
 */
/**
	__exit System Call. (called by Kernel.)
	It is called as an end processing module.
*/
static void __exit proc_cleanup_module(void)
{

}

/* Declare entry and exit functions */
module_init(proc_init_module);
module_exit(proc_cleanup_module);
/* End of Program */

