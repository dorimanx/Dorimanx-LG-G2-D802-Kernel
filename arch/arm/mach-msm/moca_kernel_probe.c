#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#include <linux/jiffies.h>
#include <linux/completion.h>
#include <asm/ioctls.h>
#include <mach/moca_kernel_probe.h>

#define MODULE_NAME "moca_kernel_probe"

#define EVENT_NUM		20		//The number of event time for comparison
#define EVENT_PERIOD	60		//Maximum period to trigger MOCA, which is by secs

#define TIME_ARRAY_MAX	(unsigned int)(EVENT_NUM * 3 / 2)		//Event time log array index 

#define KERNEL_EVENT_NOTI_LEN 		81U
#define KERNEL_IOCTL_MAGIC			'K'
#define KERNEL_IOCTL_MAXNR			0x02
#define KERNEL_EVENT_NOTI			_IOR(KERNEL_IOCTL_MAGIC, 0x01, unsigned int)

long lge_moca_kernel_probe_ioctl(struct file *file, const unsigned int cmd, unsigned long arg);

static struct class *kernel_probe_class;

static unsigned long event_time[TIME_ARRAY_MAX];		//Report jiffies time
static unsigned int top_index;						// Top index of event_time[]
moca_km_enum_type event_type;
struct completion km_ioctl_wait_completion;

bool irq_debug = false;
unsigned int event_count = 0;

struct kernel_probe_context
{
	dev_t 			dev_num;
	struct device 	*dev;
	struct cdev 	*cdev;
};

struct kernel_probe_context *gkernel_probe_context;

const struct file_operations kernel_probe_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lge_moca_kernel_probe_ioctl,
};

void lge_moca_kernel_monitor_init(void)
{
	printk("%s: Init for moca irq monitor\n",__func__);
	// Init for global variables
	memset(event_time, 0, sizeof(event_time));
	top_index = 0;
	event_count = 0;
	event_type = NO_EVENT;
}

long lge_moca_kernel_probe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if(_IOC_TYPE(cmd) != KERNEL_IOCTL_MAGIC) 
	{
		printk("magic err\n");
		return -EINVAL;
	}

	if(_IOC_NR(cmd) >= KERNEL_IOCTL_MAXNR)
	{
		printk("NR err\n");
		return -EINVAL;
	}

	switch(cmd)
	{
		case KERNEL_EVENT_NOTI:
			irq_debug = true;

			printk("[MOCA] %s: Wait for completion_interruptible, event_type = %d, irq_debug = %d \n",__func__, event_type, irq_debug);

			ret = wait_for_completion_interruptible(&km_ioctl_wait_completion);

			if(event_type == IRQ_EVENT)
			{
				irq_debug = false;

				printk("[MOCA] %s: IRQ Event Triggered ", __func__);

				if(copy_to_user((void *)arg, &event_type, sizeof(event_type)))
				{
					ret = -EFAULT;
				}

				lge_moca_kernel_monitor_init();
			}

			init_completion(&km_ioctl_wait_completion);

			break;

		default:
			break;
	}

	return ret;
}

static unsigned long lge_moca_get_local_time_by_secs(void)
{
	struct timespec time;
	struct tm tmresult;
	static int tm_day = 0;

	time = __current_kernel_time();
	time_to_tm(time.tv_sec,sys_tz.tz_minuteswest * 60* (-1),&tmresult);

	if(tm_day != tmresult.tm_mday)
	{
		printk("\n%s: The day is different \n",__func__);
		tm_day = tmresult.tm_mday;
		lge_moca_kernel_monitor_init();
	}

	return (tmresult.tm_sec + tmresult.tm_min * 60 + tmresult.tm_hour * 60 * 60);
}

int lge_moca_report_irq_time(void)
{
	//event_time[top_index] = jiffies_to_msecs(jiffies)/1000;

	event_time[top_index] = lge_moca_get_local_time_by_secs();

	printk("\n%s: current time = %06lu secs \n",__func__, event_time[top_index]);

	top_index = (top_index+1)%TIME_ARRAY_MAX;

	if(event_count < TIME_ARRAY_MAX) // Wait until events to be fulled in buffer
	{
		printk("%s: IRQs are not enough to compare \n",__func__);
		event_count++;
		return 0;
	}
	else
	{
		// Designate index to compare btw them.

		int curr_index = top_index - 1;

		int prev_index = curr_index - EVENT_NUM;

		if(curr_index < 0 ) curr_index += TIME_ARRAY_MAX;

		if(prev_index < 0 ) prev_index += TIME_ARRAY_MAX;

		// Now compare it!

		if(event_time[curr_index] - event_time[prev_index] > EVENT_PERIOD)
		{
			printk("%s: No violated IRQs \n",__func__);
			return 0;
		}
		else
		{
			int i=0;
			int print_index=0;

			printk("%s: Abnormal 57/58 irqs are detected, by comparing %06lu[%d] - %06lu[%d] > %d \n",__func__, event_time[curr_index],curr_index, event_time[prev_index], prev_index, EVENT_PERIOD);

			for(i=0 ; i<TIME_ARRAY_MAX ; i++)
			{
				print_index = (curr_index+i)%TIME_ARRAY_MAX;
				printk("[%d]=%06lu ",print_index, event_time[print_index]);
			}

			printk("\n");

			return 1;
		}
	}
}

void kernel_event_monitor(moca_km_enum_type type)
{
	if(lge_moca_report_irq_time() == 0)
	{
		return;
	}

	event_type = type;
	complete(&km_ioctl_wait_completion);
}

EXPORT_SYMBOL(kernel_event_monitor);

/**
 * Module Init.
 */
static int __init lge_moca_kernel_probe_init(void)
{
	int ret;
	u32 size = (u32)sizeof(struct kernel_probe_context);

	gkernel_probe_context = (void *)kzalloc(size, GFP_KERNEL);
	if (gkernel_probe_context == NULL) {
		printk( " Context kzalloc err.\n");
		return -ENOMEM;
	}

	kernel_probe_class = class_create(THIS_MODULE, MODULE_NAME);

	ret = alloc_chrdev_region(&gkernel_probe_context->dev_num, 0, 1, MODULE_NAME);
	if (ret) {
		printk("alloc_chrdev_region err.\n");
		return -ENODEV;
	}

	gkernel_probe_context->dev = device_create(kernel_probe_class, NULL, gkernel_probe_context->dev_num, gkernel_probe_context, MODULE_NAME);
	if (IS_ERR(gkernel_probe_context->dev)) {
		printk("device_create err.\n");
		return -ENODEV;
	}

	gkernel_probe_context->cdev = cdev_alloc();
	if (gkernel_probe_context->cdev == NULL) {
		printk("cdev_alloc err.\n");
		return -ENODEV;
	}
	cdev_init(gkernel_probe_context->cdev, &kernel_probe_fops);
	gkernel_probe_context->cdev->owner = THIS_MODULE;

	ret = cdev_add(gkernel_probe_context->cdev, gkernel_probe_context->dev_num, 1);
	if (ret)
		printk( "cdev_add err=%d\n", -ret);
	else
		printk( "MOCA Kernel Probe module init OK!!..\n");

	lge_moca_kernel_monitor_init();
	irq_debug = false;

	init_completion(&km_ioctl_wait_completion);

	return ret;
}

/**
 * Module Exit.
 */
static void __exit lge_moca_kernel_probe_exit(void)
{
	cdev_del(gkernel_probe_context->cdev);
	device_destroy(kernel_probe_class, gkernel_probe_context->dev_num);
	unregister_chrdev_region(gkernel_probe_context->dev_num, 1);

	kfree((const void*)gkernel_probe_context);

	printk( "MOCA Kernel Probe Module exit OK!!.\n");
}

module_init(lge_moca_kernel_probe_init);
module_exit(lge_moca_kernel_probe_exit);
