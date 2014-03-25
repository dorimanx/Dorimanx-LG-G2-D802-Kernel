#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/module.h>
#include <mach/board_lge.h>

#include <mach/gpio.h>

#include <linux/time.h>
//#include <stdlib.h>	// taew00k.kang


#include "fc8150_console.h"
#include "bbm.h"
#include "fci_oal.h"
#include "fci_tun.h"
#include "fc8150_regs.h"
#include "fc8150_isr.h"
#include "fci_hal.h"
#ifdef USE_PM8941_XO_A2
#include <linux/clk.h>
struct clk *clk;
#endif /* USE_PM8941_XO_A2 */

ISDBT_INIT_INFO_T *hInit;

int isdbt_open (struct inode *inode, struct file *filp);
long isdbt_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
int isdbt_release (struct inode *inode, struct file *filp);
ssize_t isdbt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);

#define RING_BUFFER_SIZE	(128 * 1024)  // kmalloc max 128k

//GPIO(RESET & INTRRUPT) Setting
#define FC8150_NAME		"isdbt"

#define GPIO_ISDBT_IRQ 77
#define GPIO_ISDBT_PWR_EN 76
#define GPIO_ISDBT_RST 75

//#define GPIO_ISDBT_PWR_EN 85
//#define GPIO_ISDBT_RST 1

void isdbt_hw_setting(void)
{
#if defined(CONFIG_MACH_MSM8974_G2_KDDI)
	{
		use_pm8941_xo_a2_192000 = 0;
		bbm_xtal_freq = 26000;
		PRINTF(hInit, "[1seg] A1-KDDI : %d, xtal_freq : %d\n",use_pm8941_xo_a2_192000, bbm_xtal_freq);
	}
#elif defined(CONFIG_MACH_MSM8974_G2_DCM)
	if (lge_get_board_revno() >= HW_REV_D) {
		use_pm8941_xo_a2_192000 = 1;
		bbm_xtal_freq = 19200;
		PRINTF(hInit, "[1seg] A1-DCM rev.D or later version: %d, xtal_freq : %d\n",use_pm8941_xo_a2_192000, bbm_xtal_freq);
	}
	else {
		use_pm8941_xo_a2_192000 = 0;
		bbm_xtal_freq = 26000;
		PRINTF(hInit, "[1seg] A1-DCM rev.C : %d, xtal_freq : %d\n",use_pm8941_xo_a2_192000, bbm_xtal_freq);
	}
#endif

	gpio_request(GPIO_ISDBT_PWR_EN, "ISDBT_EN");
	udelay(50);
	gpio_direction_output(GPIO_ISDBT_PWR_EN, 0);

	if(gpio_request(GPIO_ISDBT_IRQ, "ISDBT_IRQ_INT"))
		PRINTF(0,"ISDBT_IRQ_INT Port request error!!!\n");

	gpio_direction_input(GPIO_ISDBT_IRQ);

	gpio_request(GPIO_ISDBT_RST, "ISDBT_RST");
	udelay(50);
	gpio_direction_output(GPIO_ISDBT_RST, 1);
}

//POWER_ON & HW_RESET & INTERRUPT_CLEAR
void isdbt_hw_init(void)
{
	PRINTF(0, "isdbt_hw_init \n");
#if defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI)
	if (use_pm8941_xo_a2_192000) {
		if ( !IS_ERR_OR_NULL(clk) )
		{
			int ret = -1;
			ret = clk_prepare_enable(clk);
			if (ret) {
				PRINTF(0,"[1seg] LGE_BROADCAST_DMB_IOCTL_ON enable clock error!!!\n");
				return BBM_NOK;
			}
		}
	}
#endif

	gpio_set_value(GPIO_ISDBT_RST, 1);
	gpio_set_value(GPIO_ISDBT_PWR_EN, 1);
	mdelay(1);
	gpio_set_value(GPIO_ISDBT_RST, 0);
	mdelay(1);
	gpio_set_value(GPIO_ISDBT_RST, 1);
}

//POWER_OFF
void isdbt_hw_deinit(void)
{
	gpio_set_value(GPIO_ISDBT_PWR_EN, 0);
#if defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI)
	if (use_pm8941_xo_a2_192000) {
		if ( !IS_ERR_OR_NULL(clk) )
		{
			clk_disable_unprepare(clk);
		}
	}
#endif
}

static DECLARE_WAIT_QUEUE_HEAD(isdbt_isr_wait);

static u8 isdbt_isr_sig=0;
static struct task_struct *isdbt_kthread = NULL;

#if 0
struct timeval mytime;
u32 isr_time;
u32 read_time;
#endif
static irqreturn_t isdbt_irq(int irq, void *dev_id)
{
	#if 0
	do_gettimeofday(&mytime);
	isr_time = mytime.tv_usec;
	#endif

	isdbt_isr_sig=1;
	wake_up_interruptible(&isdbt_isr_wait);

	return IRQ_HANDLED;
}

int data_callback(u32 hDevice, u8 *data, int len)
{
	ISDBT_INIT_INFO_T *hInit;
	struct list_head *temp;
	//int i;

	hInit = (ISDBT_INIT_INFO_T *)hDevice;

	#if 0
	do_gettimeofday(&mytime);
	read_time = mytime.tv_usec;

	if((read_time-isr_time)>9000){
		PRINTF(hInit, "isr_time : %ld, read_time : %ld, ac_time : %ld\n", isr_time, read_time, (read_time-isr_time));
		return 0;
	}
	#endif
	#if 0
	for(i=0;i<len;i+=188)
	{
		if(data[i]!=0x47)
			data[i+1]=0x80;
	}
	#endif
	list_for_each(temp, &(hInit->hHead))
	{
		ISDBT_OPEN_INFO_T *hOpen;

		hOpen = list_entry(temp, ISDBT_OPEN_INFO_T, hList);

		if(hOpen->isdbttype == TS_TYPE)
		{
			if(fci_ringbuffer_free(&hOpen->RingBuffer) < (len+2) )
			{
				//PRINTF(hDevice, "f");
				return 0;
			}

			FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len >> 8);
			FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len & 0xff);

			fci_ringbuffer_write(&hOpen->RingBuffer, data, len);

			wake_up_interruptible(&(hOpen->RingBuffer.queue));
		}
	}

	return 0;
}

static int isdbt_thread(void *hDevice)
{
	static DEFINE_MUTEX(thread_lock);

	ISDBT_INIT_INFO_T *hInit = (ISDBT_INIT_INFO_T *)hDevice;
	
	// hyewon.eum changed for better play 1seg with a bad dtv tuner chip fc8101 2011-06-08
	//set_user_nice(current, -20);
	
	PRINTF(hInit, "isdbt_kthread enter\n");

	BBM_TS_CALLBACK_REGISTER((u32)hInit, data_callback);
	
	while(1)
	{
		wait_event_interruptible(isdbt_isr_wait, isdbt_isr_sig || kthread_should_stop());
		
		isdbt_isr_sig=0;
		
		BBM_ISR(hInit);
	
		if (kthread_should_stop())
			break;
	}

	BBM_TS_CALLBACK_DEREGISTER();
	
	PRINTF(hInit, "isdbt_kthread exit\n");

	return 0;
}

static struct file_operations isdbt_fops = 
{
	.owner		= THIS_MODULE,
	.unlocked_ioctl		= isdbt_ioctl,
	.open		= isdbt_open,
	.read		= isdbt_read,
	.release	= isdbt_release,
};

static struct miscdevice fc8150_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = FC8150_NAME,
    .fops = &isdbt_fops,
};

int isdbt_open (struct inode *inode, struct file *filp)
{
	ISDBT_OPEN_INFO_T *hOpen;

	PRINTF(hInit, "isdbt open\n");

	hOpen = (ISDBT_OPEN_INFO_T *)kmalloc(sizeof(ISDBT_OPEN_INFO_T), GFP_KERNEL);

	hOpen->buf = (u8 *)kmalloc(RING_BUFFER_SIZE, GFP_KERNEL);
	hOpen->isdbttype = 0;

	list_add(&(hOpen->hList), &(hInit->hHead));

	hOpen->hInit = (HANDLE *)hInit;

	if(hOpen->buf == NULL)
	{
		PRINTF(hInit, "ring buffer malloc error\n");
		return -ENOMEM;
	}

	fci_ringbuffer_init(&hOpen->RingBuffer, hOpen->buf, RING_BUFFER_SIZE);

	filp->private_data = hOpen;

	return 0;
}

ssize_t isdbt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	s32 avail;
	s32 non_blocking = filp->f_flags & O_NONBLOCK;
	ISDBT_OPEN_INFO_T *hOpen = (ISDBT_OPEN_INFO_T*)filp->private_data;
	struct fci_ringbuffer *cibuf = &hOpen->RingBuffer;
	ssize_t len;

	if (!cibuf->data || !count)
	{
		//PRINTF(hInit, " return 0\n");
                return 0;
	}
	
	if (non_blocking && (fci_ringbuffer_empty(cibuf)))
	{
		//PRINTF(hInit, "return EWOULDBLOCK\n");
		return -EWOULDBLOCK;
	}
	#if 0
	if (wait_event_interruptible(cibuf->queue, !fci_ringbuffer_empty(cibuf)))
	{
		PRINTF(hInit, "return ERESTARTSYS\n");
	 	return -ERESTARTSYS;
	}
	#endif
	avail = fci_ringbuffer_avail(cibuf);
	
	if (avail < 4)
	{
		//PRINTF(hInit, "return 00\n");
		return 0;
	}
	
	len = FCI_RINGBUFFER_PEEK(cibuf, 0) << 8;
	len |= FCI_RINGBUFFER_PEEK(cibuf, 1);
	
	if (avail < len + 2 || count < len)
	{
		PRINTF(hInit, "return EINVAL\n");		
		return -EINVAL;
	}
	
	FCI_RINGBUFFER_SKIP(cibuf, 2);

	return fci_ringbuffer_read_user(cibuf, buf, len);
}

int isdbt_release (struct inode *inode, struct file *filp)
{
	ISDBT_OPEN_INFO_T *hOpen;

	hOpen = filp->private_data;

	hOpen->isdbttype = 0;

	list_del(&(hOpen->hList));

	kfree(hOpen->buf);
	kfree(hOpen);

	return 0;
}

long isdbt_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	s32 res = BBM_NOK;
	s32 err = 0;
	s32 size = 0;
	ISDBT_OPEN_INFO_T *hOpen;
	//char *freq = "performance";	// taew00k.kang
	//static int fd = 0;		// taew00k.kang

	ioctl_info info;

	if(_IOC_TYPE(cmd) != IOCTL_MAGIC) 
		return -EINVAL;
	if(_IOC_NR(cmd) >= IOCTL_MAXNR) 
		return -EINVAL;

	hOpen = filp->private_data;

	size = _IOC_SIZE(cmd);
	PRINTF(0, "console isdbt_ioctl  0x%x\n", cmd);	

	switch(cmd) 
	{
		case IOCTL_ISDBT_RESET:
			res = BBM_RESET(hInit);
			break;
		case IOCTL_ISDBT_INIT:
			res = BBM_I2C_INIT(hInit, FCI_I2C_TYPE);
			res |= BBM_INIT(hInit);
			break;
		case IOCTL_ISDBT_BYTE_READ:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_BYTE_READ(hInit, (u16)info.buff[0], (u8 *)(&info.buff[1]));
			err |= copy_to_user((void *)arg, (void *)&info, size);
			break;
		case IOCTL_ISDBT_WORD_READ:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_WORD_READ(hInit, (u16)info.buff[0], (u16 *)(&info.buff[1]));
			err |= copy_to_user((void *)arg, (void *)&info, size);
			break;
		case IOCTL_ISDBT_LONG_READ:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_LONG_READ(hInit, (u16)info.buff[0], (u32 *)(&info.buff[1]));
			err |= copy_to_user((void *)arg, (void *)&info, size);
			break;
		case IOCTL_ISDBT_BULK_READ:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_BULK_READ(hInit, (u16)info.buff[0], (u8 *)(&info.buff[2]), info.buff[1]);
			err |= copy_to_user((void *)arg, (void *)&info, size);
			break;
		case IOCTL_ISDBT_BYTE_WRITE:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_BYTE_WRITE(hInit, (u16)info.buff[0], (u8)info.buff[1]);
			break;
		case IOCTL_ISDBT_WORD_WRITE:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_WORD_WRITE(hInit, (u16)info.buff[0], (u16)info.buff[1]);
			break;
		case IOCTL_ISDBT_LONG_WRITE:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_LONG_WRITE(hInit, (u16)info.buff[0], (u32)info.buff[1]);
			break;
		case IOCTL_ISDBT_BULK_WRITE:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_BULK_WRITE(hInit, (u16)info.buff[0], (u8 *)(&info.buff[2]), info.buff[1]);
			break;
		case IOCTL_ISDBT_TUNER_READ:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_TUNER_READ(hInit, (u8)info.buff[0], (u8)info.buff[1],  (u8 *)(&info.buff[3]), (u8)info.buff[2]);
			err |= copy_to_user((void *)arg, (void *)&info, size);
			break;
		case IOCTL_ISDBT_TUNER_WRITE:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_TUNER_WRITE(hInit, (u8)info.buff[0], (u8)info.buff[1], (u8 *)(&info.buff[3]), (u8)info.buff[2]);
			break;
		case IOCTL_ISDBT_TUNER_SET_FREQ:
		{
			u32 f_rf;
			err = copy_from_user((void *)&info, (void *)arg, size);
			f_rf = ((u32)info.buff[0]- 13) * 6000  + 473143;
			res = BBM_TUNER_SET_FREQ(hInit, f_rf);
			res |= BBM_SCAN_STATUS(hInit);
		}
			break;
		case IOCTL_ISDBT_TUNER_SELECT:
			err = copy_from_user((void *)&info, (void *)arg, size);
			res = BBM_TUNER_SELECT(hInit, (u32)info.buff[0], 0);
			break;
		case IOCTL_ISDBT_TS_START:
			hOpen->isdbttype = TS_TYPE;
			break;
		case IOCTL_ISDBT_TS_STOP:
			hOpen->isdbttype = 0;
			break;
		case IOCTL_ISDBT_POWER_ON:
			PRINTF(0, "IOCTL_ISDBT_POWER_ON \n");	
			isdbt_hw_init();
			break;
		case IOCTL_ISDBT_POWER_OFF:
			isdbt_hw_deinit();
			/*freq = "ondemand";
			fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",O_WRONLY);
			write(fd, freq, strlen(freq));
			close(fd);*/
			//system("echo ondemand >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"); 

			break;
		default:
			PRINTF(hInit, "isdbt ioctl error!\n");
			res = BBM_NOK;
			break;
	}
	
	if(err < 0)
	{
		PRINTF(hInit, "copy to/from user fail : %d", err);
		res = BBM_NOK;
	}
	return res; 
}

int isdbt_init(void)
{
	s32 res;

	PRINTF(hInit, "isdbt_init\n");

	res = misc_register(&fc8150_misc_device);

	if(res < 0)
	{
		PRINTF(hInit, "isdbt init fail : %d\n", res);
		return res;
	}

	isdbt_hw_setting();

	isdbt_hw_init();

	hInit = (ISDBT_INIT_INFO_T *)kmalloc(sizeof(ISDBT_INIT_INFO_T), GFP_KERNEL);

	res = BBM_HOSTIF_SELECT(hInit, BBM_SPI);
	
	if(res)
		PRINTF(hInit, "isdbt host interface select fail!\n");

	isdbt_hw_deinit();
#if 1

	if (!isdbt_kthread)
	{
		
		PRINTF(hInit, "kthread run\n");
		isdbt_kthread = kthread_run(isdbt_thread, (void*)hInit, "isdbt_thread");
	}
	
	res = request_irq(gpio_to_irq(GPIO_ISDBT_IRQ), isdbt_irq, IRQF_DISABLED | IRQF_TRIGGER_FALLING, FC8150_NAME, NULL);
	if(res) 
	{
		PRINTF(hInit, "dmb rquest irq fail : %d\n", res);
	}
#endif
	INIT_LIST_HEAD(&(hInit->hHead));

	return 0;
}

void isdbt_exit(void)
{
	PRINTF(hInit, "isdbt isdbt_exit \n");

	free_irq(GPIO_ISDBT_IRQ, NULL);
	
	kthread_stop(isdbt_kthread);
	isdbt_kthread = NULL;

	BBM_HOSTIF_DESELECT(hInit);

	isdbt_hw_deinit();
	
	misc_deregister(&fc8150_misc_device);

	kfree(hInit);
}

module_init(isdbt_init);
module_exit(isdbt_exit);

MODULE_LICENSE("Dual BSD/GPL");

