#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>        /* copy_to_user */

#include "broadcast_dmb_typedef.h"
#include "broadcast_dmb_drv_ifdef.h"

#define BROADCAST_DMB_NUM_DEVS 	1 /**< support this many devices */
#define DEVICE_NAME "broadcast_isdbt"

static struct class *broadcast_dmb_class;
static dev_t broadcast_dmb_dev;

static int mmbi_tuner_drv_open = -1; /* for MMBI-1seg tuner use competition */

struct broadcast_dmb_chdevice 
{
	struct cdev cdev;
	struct device *dev;
	wait_queue_head_t wq_read;
	void *cookie;
};

static struct broadcast_dmb_chdevice dmb_dev;

static int broadcast_dmb_power_on(void)
{
	int rc;
	rc = broadcast_drv_if_power_on();
	printk(KERN_DEBUG"[1seg]broadcast_dmb_power_on\n");
	return rc;
}

static int broadcast_dmb_power_off(void)
{
	int rc;
	rc = broadcast_drv_if_power_off();
	printk(KERN_DEBUG"[1seg]broadcast_dmb_power_off\n");
	return rc;
}

static int broadcast_dmb_open(void __user *arg)
{
	int rc = ERROR;
	struct broadcast_dmb_init_info udata;

	printk(KERN_DEBUG"[1seg]broadcast_dmb_open[s]\n");

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_open arg is Null\n");
		return ERROR;
	}

	if(copy_from_user(&udata, arg, sizeof(struct broadcast_dmb_init_info)))
	{
		printk(KERN_ERR"broadcast_dmb_open copy_from_user fail!!! \n");
		rc = ERROR;
	}
	else	// success to get broadcast_dmb_init_info from Broadcast Framework HAL
	{
		rc = broadcast_drv_if_open();
		
	}
	printk(KERN_DEBUG"[1seg]broadcast_dmb_open[e]\n");
	return rc;
}

static int broadcast_dmb_close(void)
{
	int rc;
	rc = broadcast_drv_if_close();
	printk(KERN_DEBUG"[1seg]broadcast_dmb_close\n");
	return rc;
}

static int broadcast_dmb_set_channel(void __user *arg)
{
	int rc = ERROR;
	struct broadcast_dmb_set_ch_info udata;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_set_channel arg is Null\n");
		return ERROR;
	}

	if(copy_from_user(&udata, arg, sizeof(struct broadcast_dmb_set_ch_info)))
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_set_ch copy_from_user fail!!! \n");
		rc = ERROR;
	}
	else
	{
		printk(KERN_DEBUG"[1seg]broadcast_dmb_set_channel[s] subChannel[%d] band[%d] channel[%d] mode[%d]\n", 
			udata.subchannel, udata.rf_band, 
			udata.channel, udata.mode);

		rc = broadcast_drv_if_set_channel(&udata);
	}
	printk(KERN_DEBUG"[1seg]broadcast_dmb_set_channel[e]\n");
	return rc;

}

static int broadcast_dmb_resync(void __user *arg)
{
	int rc = ERROR;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_resync arg is Null\n");
		return ERROR;
	}

	printk(KERN_DEBUG"[1seg]broadcast_dmb_resync\n");
	rc = broadcast_drv_if_resync();
	return rc;
}

static int broadcast_dmb_detect_sync(void __user *arg)
{
	int rc = ERROR;	
	struct broadcast_dmb_sync_info udata;

	printk(KERN_DEBUG"[1seg]broadcast_dmb_detect_sync\n");

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_detect_sync arg is Null\n");
		return ERROR;
	}

	if(copy_from_user(&udata, arg, sizeof(struct broadcast_dmb_sync_info)))
	{
		printk(KERN_ERR"broadcast_dmb_detect_sync copy_from_user fail!!! \n");
		rc = ERROR;
	}
	else
	{
		rc = broadcast_drv_if_detect_sync(&udata);

		if(rc == ERROR)	
			return ERROR;

		if(copy_to_user((void *)arg, &udata, sizeof(struct broadcast_dmb_sync_info)))
		{
			printk(KERN_ERR"[1seg]broadcast_dmb_detect_sync copy_to_user error!!! \n");
			rc = ERROR;
		}
		else
		{
			rc = OK;
		}
	}

	return rc;
}

static int broadcast_dmb_get_sig_info(void __user *arg)
{
	int rc = ERROR;
	struct broadcast_dmb_control_info udata;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_get_sig_info arg is Null\n");
		return ERROR;
	}
	
	if(copy_from_user(&udata, arg, sizeof(struct broadcast_dmb_control_info)))
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_get_sig_info copy_from_user fail!!! \n");
		rc = ERROR;
	}
	else
	{
		rc = broadcast_drv_if_get_sig_info(&udata);
	}

	if(rc == ERROR) {
		printk(KERN_ERR"[1seg]broadcast_drv_if_get_sig_info fail!!! \n");
		return ERROR;
	}

	if(copy_to_user((void *)arg, &udata, sizeof(struct broadcast_dmb_control_info)))
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_get_sig_info copy_to_user error!!! \n");
		rc = ERROR;
	}
	else
	{
		rc = OK;
	}
	return rc;
}

static int broadcast_dmb_get_ch_info(void __user *arg)
{
	int rc = ERROR;
	struct broadcast_dmb_ch_info udata;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_get_ch_info arg is Null\n");
		return ERROR;
	}

	if(copy_from_user(&udata, arg, sizeof(struct broadcast_dmb_ch_info)))
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_get_ch_info copy_from_user fail!!! \n");
		rc = ERROR;		
	}
	else
	{
		rc = broadcast_drv_if_get_ch_info(&udata);
	}

	if(copy_to_user((void *)arg, &udata, sizeof(struct broadcast_dmb_ch_info)))
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_get_ch_info copy_to_user error!!! \n");
		rc = ERROR;
	}
	else
	{
		rc = OK;
	}
	printk(KERN_DEBUG"[1seg]broadcast_dmb_get_ch_info\n");
	return rc;
}

static int broadcast_dmb_get_dmb_data(void __user *arg)
{
	int rc = ERROR;
	struct broadcast_dmb_data_info __user* puserdata;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_get_dmb_data arg is Null\n");
		return ERROR;
	}

	puserdata = (struct broadcast_dmb_data_info  __user*)arg;
	rc = broadcast_drv_if_get_dmb_data(puserdata);
	return rc;
}

static int8 broadcast_dmb_reset_ch(void)
{
	int rc;
	printk(KERN_DEBUG"[1seg]broadcast_dmb_reset_ch\n");
	rc = broadcast_drv_if_reset_ch();
	return rc;
}

static int8 broadcast_dmb_user_stop(void __user *arg)
{
	int udata;
	int __user* puser = (int __user*)arg;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_user_stop arg is Null\n");
		return ERROR;
	}

	udata = *puser;
	broadcast_drv_if_user_stop(udata);
	printk(KERN_DEBUG"[1seg]broadcast_dmb_user_stop (%d)\n", udata);
	return OK;
}

static int8 broadcast_dmb_select_antenna(void __user *arg)
{
	int rc = ERROR;
	int udata;
	int __user* puser = (int __user*)arg;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_select_antenna arg is Null\n");
		return ERROR;
	}

	printk(KERN_DEBUG"[1seg]broadcast_dmb_select_antenna\n");
	udata = *puser;
	rc = broadcast_drv_if_select_antenna(udata);	
	return rc;
}

static ssize_t broadcast_dmb_open_control(struct inode *inode, struct file *file)
{
	struct broadcast_dmb_chdevice *the_dev =
	       container_of(inode->i_cdev, struct broadcast_dmb_chdevice, cdev);

	printk(KERN_DEBUG"[1seg]broadcast_dmb_open_control start\n");


	if(mmbi_tuner_drv_open == 0)  /* for MMBI-1seg tuner use competition */
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_open_control tuner is already opened\n");
		return -EBUSY;
	}

	mmbi_tuner_drv_open  = 0; /* for MMBI-1seg tuner use competition */

	file->private_data = the_dev;

	printk(KERN_DEBUG"[1seg]broadcast_dmb_open_control OK\n");
	
	return nonseekable_open(inode, file);
}

static ssize_t broadcast_dmb_read_control(struct file *filep, char *buf, size_t count, loff_t *pos)
{
	ssize_t read_count = 0;

	if(buf==NULL) {
		printk(KERN_ERR"broadcast_dmb_read_control buf is Null\n");
		return 0;
	}

	read_count = broadcast_drv_if_read_control(buf, count);
	return read_count;
}

static int broadcast_dmb_get_mode(void __user *arg)
{
	int rc = ERROR;
	unsigned short udata;

	if(arg==NULL) {
		printk(KERN_ERR"broadcast_dmb_get_mode arg is Null\n");
		return ERROR;
	}
	
	if(copy_from_user(&udata, arg, sizeof(unsigned short)))
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_get_mode copy_from_user fail!!! \n");
		rc = ERROR;
	}
	else
	{
		rc = broadcast_drv_if_get_mode (&udata);
	}

	if(rc == ERROR) {
		printk(KERN_ERR"[1seg]broadcast_dmb_get_mode fail!!! \n");
		return ERROR;
	}

	if(copy_to_user((void *)arg, &udata, sizeof(unsigned short)))
	{
		printk(KERN_ERR"[1seg]broadcast_dmb_get_mode copy_to_user error!!! \n");
		rc = ERROR;
	}
	else
	{
		rc = OK;
	}
	return rc;
}

static long broadcast_dmb_ioctl_control(struct file *filep, unsigned int cmd,	unsigned long arg)
{
	int rc = -EINVAL;
	void __user *argp = (void __user *)arg;

	switch (cmd) 
	{
	case LGE_BROADCAST_DMB_IOCTL_ON:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_ON][s]\n");
		rc = broadcast_dmb_power_on();		
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_ON][e]res:%d\n", rc);
		break;
	case LGE_BROADCAST_DMB_IOCTL_OFF:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_OFF][s]\n");
		rc = broadcast_dmb_power_off();
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_OFF][e]res:%d\n", rc);
		break;
	case LGE_BROADCAST_DMB_IOCTL_OPEN:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_OPEN][s]\n");
		rc = broadcast_dmb_open(argp);
		printk(KERN_DEBUG"LGE_BROADCAST_DMB_IOCTL_OPEN OK %d \n", rc);
		break;
	case LGE_BROADCAST_DMB_IOCTL_CLOSE:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_CLOSE][s]\n");
		broadcast_dmb_close();
		printk(KERN_DEBUG"LGE_BROADCAST_DMB_IOCTL_CLOSE OK \n");
		rc = 0;
		break;
	case LGE_BROADCAST_DMB_IOCTL_SET_CH:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_SET_CH][s]\n");
		rc = broadcast_dmb_set_channel(argp);
		printk(KERN_DEBUG"LGE_BROADCAST_DMB_IOCTL_SET_CH result = %d \n", rc);
		break;
	case LGE_BROADCAST_DMB_IOCTL_RESYNC:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_RESYNC][s]\n");
		rc = broadcast_dmb_resync(argp);
		printk(KERN_DEBUG"LGE_BROADCAST_DMB_IOCTL_RESYNC result = %d \n", rc);
		break;
	case LGE_BROADCAST_DMB_IOCTL_DETECT_SYNC:
		//                                                                                                  
		rc = broadcast_dmb_detect_sync(argp);
		//                                                                           
		break;
	case LGE_BROADCAST_DMB_IOCTL_GET_SIG_INFO:
		//                                                                                                   
		rc = broadcast_dmb_get_sig_info(argp);
		break;
	case LGE_BROADCAST_DMB_IOCTL_GET_CH_INFO:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_GET_CH_INFO][s]\n");
		rc = broadcast_dmb_get_ch_info(argp);
		printk(KERN_DEBUG"LGE_BROADCAST_DMB_IOCTL_GET_CH_INFO result = %d \n", rc);
		break;

	case LGE_BROADCAST_DMB_IOCTL_RESET_CH:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_RESET_CH][s]\n");
		rc = broadcast_dmb_reset_ch();
		printk(KERN_DEBUG"LGE_BROADCAST_DMB_IOCTL_RESET_CH result = %d \n", rc);
		break;
		
	case LGE_BROADCAST_DMB_IOCTL_USER_STOP:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_USER_STOP][s]\n");
		rc = broadcast_dmb_user_stop(argp);
		printk(KERN_DEBUG"LGE_BROADCAST_DMB_IOCTL_USER_STOP !!! \n");
		break;

	case LGE_BROADCAST_DMB_IOCTL_GET_DMB_DATA:
		rc = broadcast_dmb_get_dmb_data(argp);
		break;

	case LGE_BROADCAST_DMB_IOCTL_SELECT_ANTENNA:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_SELECT_ANTENNA][s]\n");
		rc = broadcast_dmb_select_antenna(argp);
		break;

	case LGE_BROADCAST_DMB_IOCTL_START_STREAM:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_START_STREAM][s]\n");
		rc = OK;
		break;
		
	case LGE_BROADCAST_DMB_IOCTL_STOP_STREAM:
		printk(KERN_DEBUG"[1seg][broadcast_dmb_ioctl_control][LGE_BROADCAST_DMB_IOCTL_STOP_STREAM][s]\n");
		rc = OK;
		break;

	case LGE_BROADCAST_DMB_GET_MODE:
		rc = broadcast_dmb_get_mode(argp);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static ssize_t broadcast_dmb_release_control(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG"[1seg]broadcast_dmb_release_control\n");
	mmbi_tuner_drv_open  = -1; /* for MMBI-1seg tuner use competition */
	return 0;
}

static const struct file_operations broadcast_dmb_fops_control = 
{
	.owner = THIS_MODULE,
	.open = broadcast_dmb_open_control,
	.read = broadcast_dmb_read_control,
	.unlocked_ioctl = broadcast_dmb_ioctl_control,
	.release = broadcast_dmb_release_control,
};

static int broadcast_dmb_device_init(struct broadcast_dmb_chdevice *pbroadcast, int index)
{
	int rc;
	rc = OK;	// test code taew00k.kang
	printk(KERN_DEBUG"[1seg]broadcast_dmb_release_control index[%d]\n",index);

	cdev_init(&pbroadcast->cdev, &broadcast_dmb_fops_control);

	pbroadcast->cdev.owner = THIS_MODULE;

	rc = cdev_add(&pbroadcast->cdev, broadcast_dmb_dev, 1);

	pbroadcast->dev = device_create(broadcast_dmb_class, NULL, MKDEV(MAJOR(broadcast_dmb_dev), 0),
					 NULL, DEVICE_NAME);

	printk(KERN_DEBUG"broadcast_dmb_device_add add add%d broadcast_dmb_dev = %d \n", rc, MKDEV(MAJOR(broadcast_dmb_dev), 0));
	
	if (IS_ERR(pbroadcast->dev)) {
		rc = PTR_ERR(pbroadcast->dev);
		pr_err("[1seg]device_create failed: %d\n", rc);
		rc = ERROR;
	}
	
	printk(KERN_DEBUG"broadcast_dmb_device_init start %d\n", rc);

	return rc;
}


int8 broadcast_dmb_blt_power_on(void)
{
	int rc;
	rc = OK;	// test code taew00k.kang
	printk(KERN_DEBUG"[1seg]broadcast_dmb_blt_power_on\n");
	return rc;

}
EXPORT_SYMBOL(broadcast_dmb_blt_power_on);

int8 broadcast_dmb_blt_power_off(void)
{
	int rc;
	rc = OK;	// test code taew00k.kang
	printk(KERN_DEBUG"[1seg]broadcast_dmb_blt_power_off\n");
	return rc;

}
EXPORT_SYMBOL(broadcast_dmb_blt_power_off);

int8 broadcast_dmb_blt_open(void)
{
	int rc;
	rc = OK;	// test code taew00k.kang
	printk(KERN_DEBUG"[1seg]broadcast_dmb_blt_open\n");
	return rc;
}
EXPORT_SYMBOL(broadcast_dmb_blt_open);

int8 broadcast_dmb_blt_close(void)
{
	int rc;
	rc = OK;	// test code taew00k.kang
	printk(KERN_DEBUG"[1seg]broadcast_dmb_blt_close\n");
	return rc;
}
EXPORT_SYMBOL(broadcast_dmb_blt_close);

int8 broadcast_dmb_blt_tune_set_ch(int32 freq_num)
{
	int rc;
	//int32 freq_number = freq_num;
	//uint8 subchannel = 0;
	//uint8 op_mode = 2;
	//rc = broadcast_drv_if_set_channel(freq_number, sub_ch_id, op_mode);
	rc = OK;	// test code taew00k.kang
	printk(KERN_DEBUG"[1seg]broadcast_dmb_blt_tune_set_ch freq_num[%d]\n",freq_num);
	return rc;
}
EXPORT_SYMBOL(broadcast_dmb_blt_tune_set_ch);

int8 broadcast_dmb_blt_get_sig_info(void* sig_info)
{
	int rc;
	struct broadcast_dmb_sig_info udata;

	rc = OK;	// test code taew00k.kang
	printk(KERN_DEBUG"[1seg]broadcast_dmb_blt_get_sig_info\n");

	if(sig_info == NULL)
	{
		return rc;
	}
	memset((void*)&udata, 0x00, sizeof(struct broadcast_dmb_sig_info));

	memcpy(sig_info, (void*)&udata, sizeof(struct broadcast_dmb_sig_info));

	return rc;
}


int broadcast_dmb_drv_start(void)
{
	struct broadcast_dmb_chdevice *pbroadcast = NULL;
	int rc = -ENODEV;
	printk(KERN_DEBUG"[1seg]broadcast_dmb_drv_start\n");
	if (!broadcast_dmb_class) {

		broadcast_dmb_class = class_create(THIS_MODULE, DEVICE_NAME);

		if (IS_ERR(broadcast_dmb_class)) {
			rc = PTR_ERR(broadcast_dmb_class);
			pr_err("broadcast_dmb_class: create device class failed: %d\n",
				rc);
			return rc;
		}

		rc = alloc_chrdev_region(&broadcast_dmb_dev, 0, BROADCAST_DMB_NUM_DEVS, DEVICE_NAME);
		
		printk(KERN_DEBUG"broadcast_dmb_drv_start add add%d broadcast_dmb_dev = %d \n", rc, broadcast_dmb_dev);
		if (rc < 0) {
			pr_err("broadcast_class: failed to allocate chrdev: %d\n",
				rc);
			return rc;
		}
	}

	pbroadcast = &dmb_dev;
	
	rc = broadcast_dmb_device_init(pbroadcast, 0);
	if (rc < 0) {
		return rc;
	}
	
	printk(KERN_DEBUG"broadcast_dmb_drv_start start %d\n", rc);

	return rc;
}

EXPORT_SYMBOL(broadcast_dmb_drv_start);

int broadcast_dmb_get_stop_mode(void)
{
	printk(KERN_DEBUG"[1seg]broadcast_dmb_get_stop_mode\n");
	return 0;
}

EXPORT_SYMBOL(broadcast_dmb_get_stop_mode);

