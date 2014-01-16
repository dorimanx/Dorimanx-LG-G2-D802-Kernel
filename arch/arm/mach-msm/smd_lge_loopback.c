#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/poll.h>

#include <mach/msm_smd.h>

static int smd_lge_info_mask = 1;
static int smd_lge_dbg_mask ;

#define SMD_LGE_INFO(x...) \
   do { \
		if (smd_lge_info_mask) \
			printk(x); \
	} while (0)

#define SMD_LGE_DBG(x...) \
	do { \
		if (smd_lge_dbg_mask) \
			printk(x); \
		} while (0)

#define SMD_LGE_MAX_SIZE		8192


struct smd_lge_t {
	struct miscdevice misc;

	const char *ch_name;
	char *tx_buff;
	char *rx_buff;
	smd_channel_t *ch;

	int	read_byte;

	struct work_struct read_work;
};

static struct workqueue_struct *smd_lge_wq;
static struct smd_lge_t *psmd_device;
static wait_queue_head_t smd_lge_wait_queue;

static DECLARE_WAIT_QUEUE_HEAD(smd_lge_wait_queue);


static void smd_lge_notify(void *priv, unsigned event)
{
	switch (event) {
	case SMD_EVENT_DATA: {
		int sz;

		if (psmd_device->ch == 0) {
			SMD_LGE_INFO("%s : psmd_device->ch is NULL \n",
				  __func__);
			break;
	}
		sz = smd_cur_packet_size(psmd_device->ch);
		SMD_LGE_DBG("%s : SMD_EVENT_DATA current_packet_size = %d\n",
			    __func__, sz);
		if ((sz > 0) && (sz <= smd_read_avail(psmd_device->ch)))
#if 0
			queue_work(smd_lge_wq, &psmd_device->read_work);
#else
			wake_up(&smd_lge_wait_queue);
#endif
		break;
	}
	case SMD_EVENT_OPEN:
		SMD_LGE_INFO("%s : SMD_EVENT_OPEN\n", __func__);
		break;
	case SMD_EVENT_CLOSE:
		SMD_LGE_INFO("%s : SMD_EVENT_CLOSE\n", __func__);
		break;
	}
}

static int smd_lge_open(struct inode *ip, struct file *fp)
{

	int r = 0;

	SMD_LGE_INFO("%s \n", __func__);
	if (psmd_device->ch == 0)
		r = smd_open(psmd_device->ch_name, &psmd_device->ch,
			psmd_device, smd_lge_notify);

	psmd_device->tx_buff = kmalloc(SMD_LGE_MAX_SIZE, GFP_KERNEL);
	psmd_device->rx_buff = kmalloc(SMD_LGE_MAX_SIZE, GFP_KERNEL);


	return r;
}

static ssize_t smd_lge_write(struct file *fp, const char __user *buf,
			 size_t count, loff_t *pos)
{
	int len = 0, ch_avail = 0, ret = 0;

	SMD_LGE_DBG("%s \n", __func__);

	if (psmd_device->ch == 0) {
		SMD_LGE_INFO("%s : psmd_device->ch is NULL \n", __func__);
		return -EFAULT;
	}

	len = count ;

	if (copy_from_user(psmd_device->tx_buff, buf, count))
		return -EFAULT;

	SMD_LGE_DBG("%s : received len ( %d bytes ) from user \n",
		__func__, count);

	while (len > 0) {
		ch_avail = smd_write_avail(psmd_device->ch);
		SMD_LGE_DBG("%s : ch_avail = %d bytes, len = %d bytes \n",
			__func__, ch_avail, len);

		if (ch_avail < len) {
			ret = smd_write(psmd_device->ch, psmd_device->tx_buff,
				ch_avail);
			len -= ch_avail;
		} else {
			ret = smd_write(psmd_device->ch, psmd_device->tx_buff,
				len);
			len -= len;
		}

	}
	SMD_LGE_DBG("%s : write return value = %d \n", __func__, ret);

	return ret;
}

static ssize_t smd_lge_read(struct file *fp, char __user *buf,
			size_t count, loff_t *pos)
{
	int ret = 0;
	int sz = 0;

	SMD_LGE_DBG("%s \n", __func__);
#if 0
	ret = wait_event_interruptible(smd_lge_wait_queue, \
	    psmd_device->read_byte);

	if (psmd_device->ch == 0) {
		SMD_LGE_INFO("%s : psmd_device->ch is NULL \n", __func__);
		return -1;
	}

	ret = copy_to_user(buf, psmd_device->rx_buff, \
		psmd_device->read_byte);

if (ret > 0)
	SMD_LGE_INFO("%s : copy_to_user Error ret = %d\n",
		__func__, ret);
#else

	while (smd_cur_packet_size(psmd_device->ch) <= 0) {
		ret = wait_event_interruptible(smd_lge_wait_queue,
			smd_read_avail(psmd_device->ch)
			||  smd_cur_packet_size(psmd_device->ch));

		if (ret < 0) {
			/* qualify error message */
			/* we get this anytime a signal comes in */
			if (ret != -ERESTARTSYS)
				SMD_LGE_INFO("ERROR:%s: wait_event_"
					"interruptible %d\n", __func__, ret);
			return ret;
		}
	}


	sz = smd_cur_packet_size(psmd_device->ch);
	psmd_device->read_byte = smd_read(psmd_device->ch,
		 psmd_device->rx_buff, sz);
	SMD_LGE_DBG("%s : smd_read_avail = %d  read_byte %d\n",
		__func__, smd_read_avail(psmd_device->ch),
		psmd_device->read_byte);

	if (psmd_device->read_byte != sz)
		SMD_LGE_DBG("%s : psmd_device->read_byte != read_avail \n",
			__func__);

	ret = copy_to_user(buf, psmd_device->rx_buff, psmd_device->read_byte);

	if (ret > 0)
		SMD_LGE_INFO("%s : copy_to_user Error ret = %d\n",
		__func__, ret);

#endif
	return psmd_device->read_byte;

}

static unsigned int smd_lge_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	SMD_LGE_DBG("%s \n", __func__);

	poll_wait(file, &smd_lge_wait_queue, wait);

	if (smd_read_avail(psmd_device->ch) > 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}


static ssize_t smd_lge_release(struct inode *ip, struct file *fp)
{
	int ret = 0;

	SMD_LGE_INFO("%s \n", __func__);
	if (psmd_device->ch != 0) {
		ret = smd_close(psmd_device->ch);
		psmd_device->ch = 0;
	}

	kfree(psmd_device->tx_buff);
	kfree(psmd_device->rx_buff);

	return ret;
}


static void smd_lge_read_work(struct work_struct *ws)
{
	int sz;
	SMD_LGE_DBG("%s \n", __func__);

	for (;;) {

		if (psmd_device->ch == 0) {
			SMD_LGE_INFO("%s : psmd_device->ch is NULL \n",
				__func__);
			break;
		}
		sz = smd_cur_packet_size(psmd_device->ch);
		SMD_LGE_DBG("%s : current packet size = %d\n", __func__, sz);
		if (sz == 0) {
			SMD_LGE_DBG("%s : current packet size = %d\n",
				__func__, sz);
			break;
		}

		if (sz > smd_read_avail(psmd_device->ch)) {
			SMD_LGE_DBG("%s : current packet size > read_avail \n",
				__func__);
			break;
		}
		SMD_LGE_DBG("%s : smd_read_avail = %d\n",
			__func__, smd_read_avail(psmd_device->ch));

		if (smd_read(psmd_device->ch, psmd_device->rx_buff, sz) != sz) {
			SMD_LGE_DBG("%s : read failed\n", __func__);
			continue;
		}
		psmd_device->read_byte = sz;
		wake_up_interruptible(&smd_lge_wait_queue);
	}
}


static const struct file_operations smd_lge_fops = {
	.owner = THIS_MODULE,
	.read = smd_lge_read,
	.write = smd_lge_write,
	.open = smd_lge_open,
	.release = smd_lge_release,
	.poll = smd_lge_poll,
};

static struct smd_lge_t smd_lge_device = {
#if 0
   .ch_name = "DATA35",
#else
	.ch_name = "sys_mon",
#endif
	.misc = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "smd_lge_test",
		.fops = &smd_lge_fops,
	}
};

static struct dentry *smd_lge_dentry;
static struct dentry *smd_lge_dbg_dentry;

static int smd_lge_dbg_show(struct seq_file *s, void *unused)
{
	if (smd_lge_dbg_mask)
		seq_printf(s, "enabled\n");
	else
		seq_printf(s, "disabled\n");

	return 0;
}

static ssize_t smd_lge_dbg_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[8];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "enable", 6)) {
		/* Do not vote here. Let hsic driver decide when to vote */
		smd_lge_dbg_mask = true;
	} else {
		smd_lge_dbg_mask = false;
	}

	return count;
}


static int smd_lge_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, smd_lge_dbg_show, inode->i_private);
}


static const struct file_operations smd_lge_dbg_fops = {
	.write = smd_lge_dbg_write,
	.open = smd_lge_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int smd_lge_debugfs_init(void)
{
	smd_lge_dentry    = debugfs_create_dir("smd_lge_loopback", 0);
	if (IS_ERR(smd_lge_dentry)) {
		SMD_LGE_INFO("%s : create dir error\n", __func__);
		return -1;
	}

	smd_lge_dbg_dentry   = debugfs_create_file("dbg_on",
		 S_IRUGO | S_IWUSR, smd_lge_dentry,
		 (void *) "dbg_on" , &smd_lge_dbg_fops);

	return 0;
}

static int __init smd_lge_init(void)
{
	int ret;

	SMD_LGE_INFO("%s \n", __func__);
	smd_lge_wq = create_singlethread_workqueue("smd_lge");
	if (smd_lge_wq == 0)
		return -ENOMEM;

	psmd_device = &smd_lge_device;

	INIT_WORK(&psmd_device->read_work, smd_lge_read_work);

	init_waitqueue_head(&smd_lge_wait_queue);

	ret = misc_register(&psmd_device->misc);

	ret = smd_lge_debugfs_init();
	if (ret)
		SMD_LGE_INFO("%s : debugfs file is not available\n", __func__);

	return ret;

}

module_init(smd_lge_init);
MODULE_DESCRIPTION("SMD LGE LOOPBACK");
MODULE_LICENSE("GPL v2");
