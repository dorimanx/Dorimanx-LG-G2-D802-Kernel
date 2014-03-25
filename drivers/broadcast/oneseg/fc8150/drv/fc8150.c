#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/module.h>	// add by taew00k.kang 2013-04-23
#include <mach/board_lge.h> //to use lge_get_board_revno(), add by taew00k.kang 2013-04-23

#include "fc8150.h"
#include "bbm.h"
#include "fci_oal.h"
#include "fci_tun.h"
#include "fc8150_regs.h"
#include "fc8150_isr.h"
#include "fci_hal.h"

#include <linux/wakelock.h>

#if defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI)
#include <linux/clk.h>
static struct clk *clk;
static u8 use_pm8941_xo_a2_192000;
extern struct clk *fc8150_spi_get_clk(void);
#endif
u32 bbm_xtal_freq;

ISDBT_INIT_INFO_T *hInit;

u32 totalTS=0;
u32 totalErrTS=0;
unsigned char ch_num = 0;

u8 scan_mode;

ISDBT_MODE driver_mode = ISDBT_POWEROFF;

int isdbt_open (struct inode *inode, struct file *filp);
long isdbt_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
int isdbt_release (struct inode *inode, struct file *filp);
ssize_t isdbt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);

struct wake_lock oneseg_wakelock;
static wait_queue_head_t isdbt_isr_wait;

#define RING_BUFFER_SIZE	(128 * 1024)

//GPIO(RESET & INTRRUPT) Setting
//#define FC8150_NAME		"isdbt"
#define FC8150_NAME		"broadcast1"


#define GPIO_ISDBT_IRQ 77
#define GPIO_ISDBT_PWR_EN 76
#define GPIO_ISDBT_RST 75

static DEFINE_MUTEX(ringbuffer_lock);

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
u8 isdbt_hw_init(void)
{
	int i=0;

	while(driver_mode == ISDBT_DATAREAD)
	{
		msWait(100);
		if(i++>5)
			break;
	}

#if defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI)
	if (use_pm8941_xo_a2_192000) {
		if ( !IS_ERR_OR_NULL(clk) )
		{
			int ret = -1;
			ret = clk_prepare_enable(clk);
			if (ret) {
				PRINTF(0,"[1seg] LGE_BROADCAST_DMB_IOCTL_ON enable clock error 1!!!\n");
				return BBM_NOK;
			}
		}
		else
		{
			int ret = -1;
			clk = fc8150_spi_get_clk();
			PRINTF(0,"[1seg] LGE_BROADCAST_DMB_IOCTL_ON IS_ERR_OR_NULL(clk) first get clk!!!\n");

			if ( !IS_ERR_OR_NULL(clk) )
			{
				ret = clk_prepare_enable(clk);
				if (ret) {
					PRINTF(0,"[1seg] LGE_BROADCAST_DMB_IOCTL_ON enable clock error 2!!!\n");
					return BBM_NOK;
				}
			}
		}
	}
#endif

	//PRINTF(0, "isdbt_hw_init \n");
	gpio_set_value(GPIO_ISDBT_RST, 1);
	gpio_set_value(GPIO_ISDBT_PWR_EN, 1);
	msWait(10);
	gpio_set_value(GPIO_ISDBT_RST, 0);
	msWait(5);
	gpio_set_value(GPIO_ISDBT_RST, 1);
	msWait(2);

	driver_mode = ISDBT_POWERON;
	wake_lock(&oneseg_wakelock);

	return BBM_OK;
}

//POWER_OFF
void isdbt_hw_deinit(void)
{
	driver_mode = ISDBT_POWEROFF;
	gpio_set_value(GPIO_ISDBT_PWR_EN, 0);

	wake_unlock(&oneseg_wakelock);
#if defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI)
	if (use_pm8941_xo_a2_192000) {
		if ( !IS_ERR_OR_NULL(clk) )
		{
			clk_disable_unprepare(clk);
		}
	}
#endif
}

u8 irq_error_cnt;
static u8 isdbt_isr_sig=0;
static struct task_struct *isdbt_kthread = NULL;

static irqreturn_t isdbt_irq(int irq, void *dev_id)
{
	if(driver_mode == ISDBT_POWEROFF) {
		//PRINTF(0, "fc8150 isdbt_irq : abnormal Interrupt occurred fc8150 power off state.cnt : %d\n", irq_error_cnt);
		irq_error_cnt++;		
	}
	else {
		isdbt_isr_sig++;
		wake_up(&isdbt_isr_wait);
	}
	return IRQ_HANDLED;
}

int data_callback(u32 hDevice, u8 *data, int len)
{
	ISDBT_INIT_INFO_T *hInit;
	struct list_head *temp;
	int i;

	totalTS +=(len/188);

	for(i=0;i<len;i+=188)
	{
		if((data[i+1]&0x80)||data[i]!=0x47)
			totalErrTS++;
	}

	hInit = (ISDBT_INIT_INFO_T *)hDevice;

	list_for_each(temp, &(hInit->hHead))
	{
		ISDBT_OPEN_INFO_T *hOpen;

		hOpen = list_entry(temp, ISDBT_OPEN_INFO_T, hList);

		if(hOpen->isdbttype == TS_TYPE)
		{
			mutex_lock(&ringbuffer_lock);
			if(fci_ringbuffer_free(&hOpen->RingBuffer) < len )
			{
				mutex_unlock(&ringbuffer_lock);
				PRINTF(0, "fc8150 data_callback : ring buffer is full\n");
				return 0;
			}

			fci_ringbuffer_write(&hOpen->RingBuffer, data, len);
			wake_up_interruptible(&(hOpen->RingBuffer.queue));

			mutex_unlock(&ringbuffer_lock);
		}
	}

	return 0;
}

static int isdbt_thread(void *hDevice)
{
	ISDBT_INIT_INFO_T *hInit = (ISDBT_INIT_INFO_T *)hDevice;

	set_user_nice(current, -20);

	PRINTF(hInit, "isdbt_kthread enter\n");

	BBM_TS_CALLBACK_REGISTER((u32)hInit, data_callback);

	init_waitqueue_head(&isdbt_isr_wait);

	while(1)
	{
		wait_event_interruptible(isdbt_isr_wait, isdbt_isr_sig || kthread_should_stop());
		if (irq_error_cnt >= 1){
			PRINTF(0, "fc8150 isdbt_irq : abnormal Interrupt occurred fc8150 power off state.cnt : %d\n", irq_error_cnt);
			irq_error_cnt = 0;
		}
		if(driver_mode == ISDBT_POWERON)
		{
			driver_mode = ISDBT_DATAREAD;
			BBM_ISR(hInit);
			driver_mode = ISDBT_POWERON;
		}

		if(isdbt_isr_sig>0)
		{
			isdbt_isr_sig--;
		}

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
	.unlocked_ioctl	= isdbt_ioctl,
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
    ssize_t len, total_len = 0;

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

	mutex_lock(&ringbuffer_lock);

    avail = fci_ringbuffer_avail(cibuf);

	if (count >= avail)
		len = avail;
	else
		len = count - (count % 188);

	total_len = fci_ringbuffer_read_user(cibuf, buf, len);

	mutex_unlock(&ringbuffer_lock);

	return total_len;
}

static  ssize_t ioctl_isdbt_read(ISDBT_OPEN_INFO_T *hOpen  ,void __user *arg)
{
	struct broadcast_dmb_data_info __user* puserdata = (struct broadcast_dmb_data_info  __user*)arg;
	int ret = -ENODEV;
	size_t count;
	DMB_BB_HEADER_TYPE dmb_header;
	static int read_count = 0;
	char *buf;

	s32 avail;
	struct fci_ringbuffer *cibuf = &hOpen->RingBuffer;
	ssize_t len, total_len = 0;

	buf = puserdata->data_buf + sizeof(DMB_BB_HEADER_TYPE);
	count = puserdata->data_buf_size - sizeof(DMB_BB_HEADER_TYPE);
	count = (count/188)*188;

    if (!cibuf->data || !count)
    {
        PRINTF(hInit, " ioctl_isdbt_read return 0\n");
        return 0;
    }

    if ( fci_ringbuffer_empty(cibuf) )
    {
        //PRINTF(hInit, "return fci_ringbuffer_empty EWOULDBLOCK\n");
        return -EWOULDBLOCK;
    }

	mutex_lock(&ringbuffer_lock);
    avail = fci_ringbuffer_avail(cibuf);

	if (count >= avail)
		len = avail;
	else
		len = count - (count % 188);

	total_len = fci_ringbuffer_read_user(cibuf, buf, len);
	mutex_unlock(&ringbuffer_lock);

	dmb_header.data_type = DMB_BB_DATA_TS;
	dmb_header.size = (unsigned short)total_len;
	dmb_header.subch_id = ch_num;//0xFF;
	dmb_header.reserved = read_count++;

	ret = copy_to_user(puserdata->data_buf, &dmb_header, sizeof(DMB_BB_HEADER_TYPE));

	puserdata->copied_size = total_len + sizeof(DMB_BB_HEADER_TYPE);

	return ret;
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

int fc8150_if_test(void)
{
	int res=0;
	int i;
	u16 wdata=0;
	u32 ldata=0;
	u8 data=0;
	u8 temp = 0;

	PRINTF(0, "fc8150_if_test Start!!!\n");
	for(i=0;i<100;i++) {
		BBM_BYTE_WRITE(0, 0xa4, i&0xff);
		BBM_BYTE_READ(0, 0xa4, &data);
		if((i&0xff) != data) {
			PRINTF(0, "fc8150_if_btest!   i=0x%x, data=0x%x\n", i&0xff, data);
			res=1;
		}
	}


	for(i = 0 ; i < 100 ; i++) {
		BBM_WORD_WRITE(0, 0xa4, i&0xffff);
		BBM_WORD_READ(0, 0xa4, &wdata);
		if((i & 0xffff) != wdata) {
			PRINTF(0, "fc8150_if_wtest!   i=0x%x, data=0x%x\n", i&0xffff, wdata);
			res = 1;
		}
	}

	for(i = 0 ; i < 100; i++) {
		BBM_LONG_WRITE(0, 0xa4, i&0xffffffff);
		BBM_LONG_READ(0, 0xa4, &ldata);
		if((i&0xffffffff) != ldata) {
			PRINTF(0, "fc8150_if_ltest!   i=0x%x, data=0x%x\n", i&0xffffffff, ldata);
			res=1;
		}
	}

	for(i=0 ; i < 100 ; i++) {
		temp = i & 0xff;
		BBM_TUNER_WRITE(NULL, 0x52, 0x01, &temp, 0x01);
		BBM_TUNER_READ(NULL, 0x52, 0x01, &data, 0x01);
		if((i & 0xff) != data)
			PRINTF(0, "FC8150 tuner test (0x%x,0x%x)\n", i & 0xff, data);
	}

	PRINTF(0, "fc8150_if_test End!!!\n");

	return res;
}

static int isdbt_sw_lock_check(HANDLE hDevice)
{
	int res = BBM_NOK;
	unsigned char lock_data;

	res = BBM_READ(hDevice, 0x5053, &lock_data);

	if(res)
		return res;

	if(lock_data & 0x01)
		res = BBM_OK;
	else
		res = BBM_NOK;

	return res;
}

void isdbt_get_signal_info(HANDLE hDevice,u16 *lock, u32 *ui32BER, u32 *ui32PER, s32 *rssi, u16 *cn)
{
	struct dm_st {
		u8  start;
		s8  rssi;
		u8  wscn;
		u8  reserved;
		u16 main_rxd_rsps;
		u16 main_err_rsps;
		u32 main_err_bits;
		u32 dmp_rxd_bits;
		u32 dmp_err_bits;
		u16 inter_rxd_rsps;
		u16 inter_err_rsps;
		u32 inter_err_bits;
		u8  lna_code;
		u8  rfvga;
		u8  k;
		u8  csf_gain;
		u8  pga_gain;
		u8  extlna;
		u8  high_current_mode_gain;
		u8  extlna_gain;
	} dm;

    if(isdbt_sw_lock_check(hDevice))
	{
        *lock = 0;
		*ui32BER = 10000;
		*ui32PER = 10000;
		*rssi = -100;
		*cn = 0;

		return;
	}
    else
    {
        *lock = 1;
    }

	BBM_WRITE(hDevice, 0x5000, 0x0e);
	BBM_BULK_READ(hDevice, BBM_DM_DATA, (u8*) &dm + 1, sizeof(dm) - 1);

	if(dm.inter_rxd_rsps)
		*ui32PER = ((dm.inter_err_rsps * 10000) / dm.inter_rxd_rsps);
	else
		*ui32PER = 10000;

	if(dm.dmp_rxd_bits)
		*ui32BER = ((dm.dmp_err_bits * 10000) / dm.dmp_rxd_bits);
	else
		*ui32BER = 10000;

	*rssi = dm.rssi;
	*cn = dm.wscn;

	//PRINTF(hDevice, "[FC8150]LOCK :%d, BER: %d, PER : %d, RSSI : %d, CN : %d\n",lock, *ui32BER, *ui32PER, *rssi, *cn);
}

void isdbt_set_scanmode(HANDLE hDevice, u8 scanmode)
{
	if(scanmode)
	{
		if(!scan_mode)
		{
			BBM_WRITE(hDevice, 0x3040, 0x00);
			BBM_WRITE(hDevice, 0x3004, 0x02);
			BBM_WRITE(hDevice, 0x3006, 0x02);
			BBM_WRITE(hDevice, 0x2020, 0x18);
			BBM_WRITE(hDevice, 0x2021, 0x14);
			BBM_WRITE(hDevice, 0x2022, 0xea);
			BBM_WRITE(hDevice, 0x2082, 0x70);
			BBM_WRITE(hDevice, 0x2083, 0x70);
			BBM_WRITE(hDevice, 0x2084, 0x70);
			BBM_WRITE(hDevice, 0x2085, 0x60);

			scan_mode=1;
			PRINTF(hDevice, "SCAN MODE ON\n");
		}
	}
	else
	{
		if(scan_mode)
		{
			BBM_WRITE(hDevice, 0x3040, 0x27);
			BBM_WRITE(hDevice, 0x3004, 0x04);
			BBM_WRITE(hDevice, 0x3006, 0x04);
			BBM_WRITE(hDevice, 0x2020, 0x10);
			BBM_WRITE(hDevice, 0x2021, 0x0e);
			BBM_WRITE(hDevice, 0x2022, 0x4a);
			BBM_WRITE(hDevice, 0x2082, 0x45);
			BBM_WRITE(hDevice, 0x2083, 0x5f);
			BBM_WRITE(hDevice, 0x2084, 0x37);
			BBM_WRITE(hDevice, 0x2085, 0x30);

			scan_mode=0;
			PRINTF(hDevice, "SCAN MODE OFF\n");
		}
	}

}

void isdbt_isr_check(HANDLE hDevice)
{
	u8 isr_time=0;

	BBM_WRITE(hDevice, BBM_BUF_INT, 0x00);

	while(isr_time < 10) {
		if(!isdbt_isr_sig) {
			break;
		}
		msWait(10);
		isr_time++;
	}

}

long isdbt_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	s32 res = BBM_NOK;

	void __user *argp = (void __user *)arg;

	s32 err = 0;
	s32 size = 0;
	int uData=0;
	ISDBT_OPEN_INFO_T *hOpen;

	IOCTL_ISDBT_SIGNAL_INFO isdbt_signal_info;

	if(_IOC_TYPE(cmd) != ISDBT_IOC_MAGIC)
	{
		return -EINVAL;
	}

	if(_IOC_NR(cmd) >= IOCTL_MAXNR)
	{
		return -EINVAL;
	}

	hOpen = filp->private_data;

	size = _IOC_SIZE(cmd);

	// PRINTF(0, "isdbt_ioctl  0x%x\n", cmd);

	switch(cmd)
	{
		case IOCTL_ISDBT_POWER_ON:
		case LGE_BROADCAST_DMB_IOCTL_ON:
			//PRINTF(0, "[1seg] IOCTL_ISDBT_POWER_ON \n");

			res = isdbt_hw_init();
			res |= BBM_I2C_INIT(hInit, FCI_I2C_TYPE);
			PRINTF(hInit, "[1seg] FC8150 BBM_I2C_INIT res : %d \n", res);

			res |= BBM_PROBE(hInit);
			PRINTF(hInit, "[1seg] FC8150 BBM_PROBE res : %d \n", res);

			if(res) {
				PRINTF(hInit, "[1seg] FC8150 Initialize Fail : %d \n", res);
			//	break;
			}

			res |= BBM_INIT(hInit);
			res |= BBM_TUNER_SELECT(hInit, FC8150_TUNER, 0);
			scan_mode = 0;

			if(res)
			PRINTF(0, "[1seg] IOCTL_ISDBT_POWER_ON FAIL \n");
			else
			PRINTF(0, "[1seg] IOCTL_ISDBT_POWER_OK \n");

			//fc8150_if_test();
			break;
		case IOCTL_ISDBT_POWER_OFF:
		case LGE_BROADCAST_DMB_IOCTL_OFF:

			PRINTF(0, "IOCTL_ISDBT_POWER_OFF \n");
			isdbt_hw_deinit();
			res = BBM_OK;
			break;
		case IOCTL_ISDBT_SCAN_FREQ:
		{
			u32 f_rf;
			err = copy_from_user((void *)&uData, (void *)arg, size);

			f_rf = (uData- 13) * 6000 + 473143;
			//PRINTF(0, "IOCTL_ISDBT_SCAN_FREQ  f_rf : %d\n", f_rf);

			isdbt_set_scanmode(hInit, 1);

			isdbt_isr_check(hInit);
			res = BBM_TUNER_SET_FREQ(hInit, f_rf);
			BBM_WRITE(hInit, BBM_BUF_INT, 0x01);
			res |= BBM_SCAN_STATUS(hInit);
		}
			break;
		case IOCTL_ISDBT_SET_FREQ:
		{
			u32 f_rf;
			totalTS=0;
			totalErrTS=0;

			err = copy_from_user((void *)&uData, (void *)arg, size);
			mutex_lock(&ringbuffer_lock);
			fci_ringbuffer_flush(&hOpen->RingBuffer);
			mutex_unlock(&ringbuffer_lock);
			f_rf = (uData- 13) * 6000 + 473143;
			//PRINTF(0, "IOCTL_ISDBT_SET_FREQ chNum : %d, f_rf : %d\n", uData, f_rf);

			isdbt_set_scanmode(hInit, 0);
			isdbt_isr_check(hInit);
			res = BBM_TUNER_SET_FREQ(hInit, f_rf);
			BBM_WRITE(hInit, BBM_BUF_INT, 0x01);
			res |= BBM_SCAN_STATUS(hInit);
		}
			break;
		case IOCTL_ISDBT_GET_LOCK_STATUS:
			//PRINTF(0, "IOCTL_ISDBT_GET_LOCK_STATUS \n");
			res = isdbt_sw_lock_check(hInit);
			if(res)
				uData=0;
			else
				uData=1;
			err |= copy_to_user((void *)arg, (void *)&uData, size);
			res = BBM_OK;
			break;
		case IOCTL_ISDBT_GET_SIGNAL_INFO:
			isdbt_get_signal_info(hInit, &isdbt_signal_info.lock, &isdbt_signal_info.ber, &isdbt_signal_info.per, &isdbt_signal_info.rssi, &isdbt_signal_info.cn);

			isdbt_signal_info.ErrTSP = totalErrTS;
			isdbt_signal_info.TotalTSP = totalTS;

			totalTS=totalErrTS=0;

			err |= copy_to_user((void *)arg, (void *)&isdbt_signal_info, size);

			res = BBM_OK;

			break;
		case IOCTL_ISDBT_START_TS:
			hOpen->isdbttype = TS_TYPE;
			res = BBM_OK;
			break;
		case IOCTL_ISDBT_STOP_TS:
		case LGE_BROADCAST_DMB_IOCTL_USER_STOP:
			hOpen->isdbttype = 0;
			res = BBM_OK;
			break;

		case LGE_BROADCAST_DMB_IOCTL_SET_CH:
			{
				struct broadcast_dmb_set_ch_info udata;
				u32 f_rf;
				//PRINTF(0, "LGE_BROADCAST_DMB_IOCTL_SET_CH \n");

				if(copy_from_user(&udata, argp, sizeof(struct broadcast_dmb_set_ch_info)))
				{
					PRINTF(0,"broadcast_dmb_set_ch fail!!! \n");
					res = -1;
				}
				else
				{
					f_rf = (udata.ch_num- 13) * 6000 + 473143;
					//PRINTF(0, "IOCTL_ISDBT_SET_FREQ freq:%d, RF:%d\n",udata.ch_num,f_rf);
					if(udata.mode == LGE_BROADCAST_OPMODE_ENSQUERY)
						isdbt_set_scanmode(hInit, 1);
                    else
                        isdbt_set_scanmode(hInit, 0);

					isdbt_isr_check(hInit);
					res = BBM_TUNER_SET_FREQ(hInit, f_rf);
					BBM_WRITE(hInit, BBM_BUF_INT, 0x01);

					if(udata.mode == LGE_BROADCAST_OPMODE_ENSQUERY)
					{
						res |= BBM_SCAN_STATUS(hInit);
						if(res != BBM_OK)
						{
							PRINTF(0, " BBM_SCAN_STATUS  Unlock \n");
							break;
						}
						PRINTF(0, " BBM_SCAN_STATUS : Lock \n");
					}

					// PRINTF(0, "IOCTL_ISDBT_SET_FREQ \n");
					totalTS=0;
					totalErrTS=0;
					ch_num = udata.ch_num;
					mutex_lock(&ringbuffer_lock);
					fci_ringbuffer_flush(&hOpen->RingBuffer);
					mutex_unlock(&ringbuffer_lock);
					hOpen->isdbttype = TS_TYPE;
				}
			}
			break;
		case LGE_BROADCAST_DMB_IOCTL_GET_SIG_INFO:
			{
				struct broadcast_dmb_sig_info udata;
				//PRINTF(0, "LGE_BROADCAST_DMB_IOCTL_GET_SIG_INFO \n");

				isdbt_get_signal_info(hInit, &isdbt_signal_info.lock, &isdbt_signal_info.ber, &isdbt_signal_info.per, &isdbt_signal_info.rssi, &isdbt_signal_info.cn);

				isdbt_signal_info.ErrTSP = totalErrTS;
				isdbt_signal_info.TotalTSP = totalTS;

				totalTS=totalErrTS=0;

				udata.info.oneseg_info.lock = (int)isdbt_signal_info.lock;
				udata.info.oneseg_info.ErrTSP = (int)isdbt_signal_info.ErrTSP;
				udata.info.oneseg_info.TotalTSP = (int)isdbt_signal_info.TotalTSP;

				udata.info.oneseg_info.ber = (int)isdbt_signal_info.ber;
				udata.info.oneseg_info.per = (int)isdbt_signal_info.per;
				udata.info.oneseg_info.rssi = (int)isdbt_signal_info.rssi;
				udata.info.oneseg_info.cn = (int)isdbt_signal_info.cn;
                udata.info.oneseg_info.antenna_level = 0;

				if(copy_to_user((void *)argp, &udata, sizeof(struct broadcast_dmb_sig_info)))
				{
					PRINTF(0,"broadcast_dmb_get_sig_info copy_to_user error!!! \n");
					res = BBM_NOK;
				}
				else
				{
                    PRINTF(0, "LOCK :%d, BER: %d, PER : %d, RSSI : %d, CN : %d\n",
                        udata.info.oneseg_info.lock,
                        udata.info.oneseg_info.ber,
                        udata.info.oneseg_info.per,
                        udata.info.oneseg_info.rssi,
                        udata.info.oneseg_info.cn);

					res = BBM_OK;
				}
			}
			break;

		case LGE_BROADCAST_DMB_IOCTL_GET_DMB_DATA:
			//PRINTF(0, "LGE_BROADCAST_DMB_IOCTL_GET_DMB_DATA \n");
			res = ioctl_isdbt_read(hOpen,argp);
			break;
		case LGE_BROADCAST_DMB_IOCTL_OPEN:
		case LGE_BROADCAST_DMB_IOCTL_CLOSE:
		case LGE_BROADCAST_DMB_IOCTL_RESYNC:
		case LGE_BROADCAST_DMB_IOCTL_DETECT_SYNC:
		case LGE_BROADCAST_DMB_IOCTL_GET_CH_INFO:
		case LGE_BROADCAST_DMB_IOCTL_RESET_CH:
		case LGE_BROADCAST_DMB_IOCTL_SELECT_ANTENNA:
			PRINTF(0, "LGE_BROADCAST_DMB_IOCTL_SKIP \n");
            res = BBM_OK;
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

	PRINTF(hInit, "isdbt_init DRV V1p12 20130701\n");

	res = misc_register(&fc8150_misc_device);

	if(res < 0)
	{
		PRINTF(hInit, "isdbt init fail : %d\n", res);
		return res;
	}

	wake_lock_init(&oneseg_wakelock, WAKE_LOCK_SUSPEND, fc8150_misc_device.name);

	isdbt_hw_setting();

	hInit = (ISDBT_INIT_INFO_T *)kmalloc(sizeof(ISDBT_INIT_INFO_T), GFP_KERNEL);

	res = BBM_HOSTIF_SELECT(hInit, BBM_SPI);

	if(res)
	{
		PRINTF(hInit, "isdbt host interface select fail!\n");
	}

	if (!isdbt_kthread)
	{
		PRINTF(hInit, "kthread run\n");
		isdbt_kthread = kthread_run(isdbt_thread, (void*)hInit, "isdbt_thread");
	}

	res = request_irq(gpio_to_irq(GPIO_ISDBT_IRQ), isdbt_irq, IRQF_DISABLED | IRQF_TRIGGER_FALLING, FC8150_NAME, NULL);

	if(res)
		PRINTF(hInit, "dmb rquest irq fail : %d\n", res);

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
	wake_lock_destroy(&oneseg_wakelock);

}

module_init(isdbt_init);
module_exit(isdbt_exit);

//MODULE_LICENSE("Dual BSD/GPL");
MODULE_LICENSE("GPL v2");

