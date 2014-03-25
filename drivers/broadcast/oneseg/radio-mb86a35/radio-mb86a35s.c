/**
	@file	radio-mb86a35.c \n
	multimedia tuner module device driver header file. \n
	This file is a header file for multimedia tuner module device driver users.
*/
/* COPYRIGHT FUJITSU SEMICONDUCTOR LIMITED 2011 */
#ifndef	__KERNEL__
#define	__KERNEL__
#endif
#include <linux/miscdevice.h>

#include "radio-mb86a35s-dev.h"

#define BROADCAST_MMBI_NUM_DEVS 	1 /**< support this many devices */
//#define PM_QOS
//#define MMB_WQ_FEATURE

#undef PERFORMANCE_TEST	
#define WORKAROUND_XX

#if 1 //   
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h> 		/* wake_lock, unlock */

#ifdef PM_QOS
//#include <linux/pm_qos_params.h>
#endif /* PM_QOS */

#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include "isdbt_common.h"
#endif

#if 1//   
#define QC_SPI_MAX (188*40)//(188*32)//(188*60)

#ifndef boolean
typedef unsigned char      boolean;
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif

static struct class *broadcast_mmbi_class;
static dev_t broadcast_mmbi_dev;
struct broadcast_mmbi_chdevice 
{
	struct cdev cdev;
	struct device *dev;
	wait_queue_head_t wq_read;
	void *cookie;
};

static struct broadcast_mmbi_chdevice mmbi_dev;

struct ISDBT_MMBI_CTRL
{
	boolean 					IsdbtPowerOnState;
	struct spi_device* 			pSpiDevice;
	struct work_struct 			spi_work;
	struct workqueue_struct* 	spi_wq;
	struct mutex				mutex;
	struct wake_lock 			wake_lock;	/* wake_lock,wake_unlock */
	//test
	boolean 					spi_irq_status;
};

static struct ISDBT_MMBI_CTRL MMBICtrlInfo;

static int mmbi_tuner_drv_open = -1;

#define TX_RX_DATA_BUF_SIZE (8192)

static u8 *wdata_buf;
static u8 *rdata_buf;

//                                                         
//static u8 wx_data_buf[TX_RX_DATA_BUF_SIZE+32] = {0,};
//static u8 rx_data_buf[TX_RX_DATA_BUF_SIZE+32] = {0,};
static u8 wx_data_buf[TX_RX_DATA_BUF_SIZE+32] __cacheline_aligned;
static u8 rx_data_buf[TX_RX_DATA_BUF_SIZE+32] __cacheline_aligned;
//                                                         

static unsigned int buf_id = 0;

#ifdef PM_QOS
static int cpu_state = 0;
static struct pm_qos_request_list mmb_pm_qos_list;
#endif


extern void mbt_dataring_create(unsigned int* buffer_id, int len);
extern void mbt_dataring_destroy(unsigned int* buffer_id);
extern int mbt_dataring_empty(unsigned int buffer_id);
extern int mbt_dataring_free(unsigned int buffer_id);
extern int mbt_dataring_avail(unsigned int buffer_id);
extern void mbt_dataring_flush(unsigned int buffer_id);
extern int mbt_dataring_read(unsigned int buffer_id, char * buf, int len);
extern int mbt_dataring_write(unsigned int buffer_id, const char * buf, int len);
extern void isdbt_hw_setting(void);
extern void isdbt_hw_init(void);
extern void isdbt_hw_deinit(void);
#endif


//                                                  

static int devmajor = NODE_MAJOR;
static char *devname = NODE_PATHNAME;

static char *mmap_area;

static u16 read_size = 0;

static int handler_enable_flag = 0;

static unsigned long map_physicaladdr;

#define		LICENSE			"GPL v2"
#define		AUTHOR			"FUJITSU SEMICONDUCTOR LIMITED"
#define		DESCRIPTION		"FUJITSU SEMICONDUCTOR LIMITED"
#define		VERSION			"1.3"

MODULE_LICENSE(LICENSE);
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_VERSION(VERSION);

/* insmod() Parameter */
static int MB86A35_DEBUG = 0;
static char *mode = NULL;
module_param(mode, charp, S_IRUGO);	/* Executed mode : DEBUG "mb86a35_DEBUG" */

#define	MB86A35_DEF_REG2B		0x08
#define	MB86A35_DEF_REG2E		0x16
#define	MB86A35_DEF_REG30		0x04
#define	MB86A35_DEF_REG31		0x3F
#define	MB86A35_DEF_REG5A		0x3E

static int mb86a35_IOCTL_RF_INIT(mb86a35_cmdcontrol_t * cmdctrl,
				 unsigned int cmd, unsigned long arg);
static int mb86a35_IOCTL_RF_FUSEVALUE(mb86a35_cmdcontrol_t * cmdctrl,
				      unsigned int cmd, unsigned long arg);
static int mb86a35_IOCTL_RF_CALIB_DCOFF(mb86a35_cmdcontrol_t * cmdctrl,
					unsigned int cmd, unsigned long arg);
static int mb86a35_IOCTL_RF_CALIB_CTUNE(mb86a35_cmdcontrol_t * cmdctrl,
					unsigned int cmd, unsigned long arg);

static u8 REG30 = MB86A35_DEF_REG30;

#define	MB86A35_CALB_DCOFF_WAIT		10


static int mb86a35s_spi_rf_recv(unsigned char reg, unsigned char *data, unsigned int size);

/*     */
//extern int isdbt_init(void* dev);

/************************************************************************/
/**
	local udelay. \n

	@param	val		[in] wait value.

*/
void _udelay(int val)
{
	/* if val is 0 when not wait */
	if (val)
		udelay(val);
}
/************************************************************************/
/**
 	internel function. \n
	SPI write. 1byte write format. \n

	@param	reg		[in] register address.
	@param	data		[in] write data address.
	@param	size		[in] write byte size.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.

*/
static int mb86a35s_spi_send(unsigned char reg, unsigned int data, unsigned int size)
{
#if 0
	{
	int res = 0;
	
	struct spi_message	message;
	struct spi_transfer	x;
	
	u8 mode   = MB86A35S_SPI_WRITE_HEADER_1BYTE | size;
	u8 header[6] = {0,};

	header[0] = mode;
	header[1] = reg;
	header[2] = 0x000000FF & data;
	header[3] = (0x0000FF00 & data)>>8;
	header[4] = (0x00FF0000 & data)>>16;
	header[5] = (0xFF000000 & data)>>24;

	spi_message_init(&message);
	memset(&x, 0, sizeof x);

	spi_message_add_tail(&x, &message);
	
	memcpy(&wdata_buf[0], header, 2+size);
	
	x.tx_buf=&wdata_buf[0];
	x.rx_buf=&rdata_buf[0];
	x.len = (2 + size) + 0;
	x.cs_change = 0; // 0 : CS is alternate per transfer, 1 : CS is alternate per 8/16/24/32 bit which is setted in spi_setup
	x.bits_per_word = 8;
	res = spi_sync(MMBICtrlInfo.pSpiDevice, &message);

	//memcpy(rxbuf, x.rx_buf + tx_length, rx_length);
	
	return res;
}
#else
	int rtn = 0;
	struct spi_message	m;
	struct spi_transfer t[1];
	u8 mode   = MB86A35S_SPI_WRITE_HEADER_1BYTE | size;
	u8 header[6] = {0,};
	//u8 _data[3];

	//printk("** reg[%02x], data[%08x], size[%02x]\n", (int)reg, (int)data, (int)size);

	mutex_lock(&MMBICtrlInfo.mutex);
	
	spi_message_init(&m);
	memset(t, 0, sizeof(t));
	//memset(_data, 0, sizeof(_data));

	header[0] = mode;
	header[1] = reg;
	header[2] = 0x000000FF & data;
	header[3] = (0x0000FF00 & data)>>8;
	header[4] = (0x00FF0000 & data)>>16;
	header[5] = (0xFF000000 & data)>>24;
	
	t[0].tx_buf = &header[0];
	t[0].len	= size+2; //header
	t[0].cs_change	  = 0;
	t[0].bits_per_word	  = 8;
	spi_message_add_tail(&t[0], &m);

	//t[1].tx_buf = &reg;
	//t[1].len	  = sizeof(reg);
	//spi_message_add_tail(&t[1], &m);

	//memcpy(_data, &data, size);
	//t[1].tx_buf = &_data;
	//t[1].len	  = size;
	//spi_message_add_tail(&t[1], &m);

	rtn = spi_sync(MMBICtrlInfo.pSpiDevice, &m);
	if (rtn != 0) {
		printk("spi_sync error.[ %d ]\n", rtn);
	}

	mutex_unlock(&MMBICtrlInfo.mutex);

	return rtn;
#endif

}




/************************************************************************/
/**
 	internel function. \n
	SPI read. 1byte read format. \n

	@param	reg		[in] register address.
	@param	data		[out] read data address.
	@param	size		[out] read byte size.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.

*/
static int mb86a35s_spi_recv(unsigned char reg, unsigned char *data, unsigned int size)
{
#if 0

	{
	int res = 0;
	
	struct spi_message	message;
	struct spi_transfer	x;
	
	u8 mode = MB86A35S_SPI_READ_HEADER_1BYTE | size;
	u8 header[6] = {0,};

	header[0] = mode;
	header[1] = reg;
	//header[2] = 0x000000FF & data;
	//header[3] = (0x0000FF00 & data)>>8;
	//header[4] = (0x00FF0000 & data)>>16;
	//header[5] = (0xFF000000 & data)>>24;

	spi_message_init(&message);
	memset(&x, 0, sizeof x);

	spi_message_add_tail(&x, &message);
	
	memcpy(&wdata_buf[0], header, 2);
	
	x.tx_buf=&wdata_buf[0];
	x.rx_buf=&rdata_buf[0];
	x.len = (2) + (size+1);
	x.cs_change = 0; // 0 : CS is alternate per transfer, 1 : CS is alternate per 8/16/24/32 bit which is setted in spi_setup
	x.bits_per_word = 8;
	res = spi_sync(MMBICtrlInfo.pSpiDevice, &message);

	memcpy(data, x.rx_buf + 2 + 1, size);
	
	return res;
	}

#else //FSL OK
	int rtn = 0;
	struct spi_message	m;
	struct spi_transfer t[2];
	u8 mode = MB86A35S_SPI_READ_HEADER_1BYTE | size;
	u8 header[2] = {0,};
	//u8 addr	= 0;
	u8 _data[4];

	mutex_lock(&MMBICtrlInfo.mutex);

	spi_message_init(&m);
	memset(t, 0, sizeof(t));
	memset(_data, 0, sizeof(_data));

	header[0] = mode;
	header[1] = reg;
	t[0].tx_buf = &header[0];
	t[0].len	= sizeof(header);
	t[0].cs_change	  = 0;
	t[0].bits_per_word	  = 8;
	spi_message_add_tail(&t[0], &m);

	//addr = (unsigned char)reg;
	//t[1].tx_buf = &addr;
	//t[1].len	  = sizeof(addr);
	//t[1].cs_change	= 0;
	//spi_message_add_tail(&t[1], &m);

	t[1].rx_buf = _data;
	t[1].len	= size + 1;	/* add receive read header size */
	t[1].cs_change	  = 0;
	t[1].bits_per_word	  = 8;
	spi_message_add_tail(&t[1], &m);

	rtn = spi_sync(MMBICtrlInfo.pSpiDevice, &m);
	if (rtn != 0) {
		printk("spi_sync error.[ %d ]\n", rtn);
		goto end;
	}

	memcpy(data, (_data+1), size);	/* read header skip */


	//printk("** FSL modify reg[%02x], *data[%02x] size[%02x]\n", (int)reg, (int)data[0], (int)size);
	//printk("** FSL modify data0[%02x], data1[%02x] data2[%02x] data3[%02x]\n", (int)_data[0], (int)_data[1], (int)_data[2], (int)_data[3]);

end:
	mutex_unlock(&MMBICtrlInfo.mutex);
	return rtn;
#endif
}



/************************************************************************/
/**
 	internel function. \n
	SPI Stream read. 3byte read format. \n

	@param	reg		[in] register address.
	@param	data		[out] read data address.
	@param	size		[out] read byte size.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.

		+------+
	Header	|XXYYYY|    XX : Code      YYYY : send size
		+------+
		+------+
	Address	|0000XX|    XX : Address
		+------+

*/

//#define TX_RX_DATA_BUF_SIZE (40*1024)
//static u8 *wdata_buf;
//static u8 *rdata_buf;
//static u8 wx_data_buf[TX_RX_DATA_BUF_SIZE+32] ={0,};
//static u8 rx_data_buf[TX_RX_DATA_BUF_SIZE+32] = {0,};

static int mb86a35s_spi_stream_recv(unsigned char reg, unsigned char *data, unsigned int size)
{
#if 1
	{
	int res = 0;
	
	struct spi_message	message;
	struct spi_transfer	x;
	
	u8 mode = MB86A35S_SPI_READ_HEADER_3BYTE;
	u8 header[6] = {0,};

	header[0] = mode;
	header[1] = (u8)((size >> 8) & 0x000000ff);	/* size hi	*/
//	header[1] = 0;	/* size hi	*/
	header[2] = (u8)(size & 0x000000ff);		/* size lo	*/
	header[3] = 0;
	header[4] = 0;
	header[5] = reg;
	
	mutex_lock(&MMBICtrlInfo.mutex);

	spi_message_init(&message);
	memset(&x, 0, sizeof x);

	spi_message_add_tail(&x, &message);
	
	memcpy(&wdata_buf[0], header, sizeof(header));
	
	x.tx_buf=&wdata_buf[0];
	x.rx_buf=&rdata_buf[0];
	x.len = (sizeof(header)) + (size+3);
	x.cs_change = 0; // 0 : CS is alternate per transfer, 1 : CS is alternate per 8/16/24/32 bit which is setted in spi_setup
	x.bits_per_word = 8;
	res = spi_sync(MMBICtrlInfo.pSpiDevice, &message);
	if(res != 0)
		printk("spi_sync error %d", res);		

	memcpy(data, x.rx_buf + (sizeof(header)), (size+3));
	
	mutex_unlock(&MMBICtrlInfo.mutex);	
	
	return res;
	}
#else
#if 1
	int rtn = 0;
	struct spi_message  m;
	struct spi_transfer t[2];
	u8 mode = MB86A35S_SPI_READ_HEADER_3BYTE;
	u8 header[6];
	//u8 addr[3];

#if 0
	unsigned int *lptr;
	int i = 0;
#endif

	mutex_lock(&MMBICtrlInfo.mutex);

	spi_message_init(&m);
	memset(t, 0, sizeof(t));
	memset(header, 0, sizeof(header));
	//memset(addr, 0, sizeof(addr));

	header[0] = mode;
	header[1] = (u8)((size >> 8) & 0x000000ff);	/* size hi	*/
//	header[1] = 0;	/* size hi	*/
	header[2] = (u8)(size & 0x000000ff);		/* size lo	*/
	header[3] = 0;
	header[4] = 0;
	header[5] = reg;

	memcpy(&wdata_buf[0], header, sizeof(header));

	t[0].tx_buf = &wdata_buf[0];
	//t[0].rx_buf = NULL;
	t[0].len    = sizeof(header);
	t[0].cs_change	  = 0;
	spi_message_add_tail(&t[0], &m);

	//addr[2] = (unsigned char)reg;
	//t[1].tx_buf = addr;
	//t[1].rx_buf = NULL;
	//t[1].len    = sizeof(addr);
	//spi_message_add_tail(&t[1], &m);

	//t[1].tx_buf = NULL;
	t[1].rx_buf = data;
	t[1].len    = size + 3; 	/* add receive read header size */
	t[1].cs_change	  = 0;
	spi_message_add_tail(&t[1], &m);

	rtn = spi_sync(MMBICtrlInfo.pSpiDevice, &m);
	if (rtn < 0) {
		ERRPRINT("spi_sync error.[ %d ]\n", rtn);
		goto end;
	}

	mutex_unlock(&MMBICtrlInfo.mutex);

#if 0
	printk(KERN_INFO "Stream status %d heder %02x %02x %02x \n", m.status, data[0], data[1], data[2]);
	printk(KERN_INFO "Buff add  %8.8x\n", (unsigned int)data);

	lptr = (unsigned int *)data;

	for (i = 0; i < (size / 16) + 1; i++) {
		printk(KERN_INFO "Stream data %4.4x: %8.8x %8.8x %8.8x %8.8x\n", i*4, *(lptr+i*4), *(lptr+1+i*4), *(lptr+2+i*4), *(lptr+3+i*4));
	}
	
#endif
	//DBGPRINT(PRINT_LHEADERFMT "** reg[%02x], data [%02x] [%02x] [%02x]\n", PRINT_LHEADER, (int)reg, (int)data[0], (int)data[1], (int)data[2]);
	//DBGPRINT(PRINT_LHEADERFMT "** data [%02x] [%02x] [%02x]\n", PRINT_LHEADER, (int)data[3], (int)data[4], (int)data[5]);
end:
	return rtn;

#else
//	int rtn = 0;
	int res;
	u8 mode = MB86A35S_SPI_READ_HEADER_3BYTE;
	u8 header[6];
	//u8 addr[3];
	
	struct spi_message	message;
	struct spi_transfer	x;

	header[0] = mode;
	header[1] = (u8)((size >> 8) & 0x000000ff);	/* size hi	*/
//	header[1] = 0;	/* size hi	*/
	header[2] = (u8)(size & 0x000000ff);		/* size lo	*/
	header[3] = 0;
	header[4] = 0;
	header[5] = reg;
	
	spi_message_init(&message);
	memset(&x, 0, sizeof(x));

	spi_message_add_tail(&x, &message);
	
	memcpy(&wdata_buf[0], &header[0], 6);
	
	x.tx_buf=&wdata_buf[0];
	x.rx_buf=&rdata_buf[0];
	x.len = 6 + (size +3);
	x.cs_change = 0; // 0 : CS is alternate per transfer, 1 : CS is alternate per 8/16/24/32 bit which is setted in spi_setup
	x.bits_per_word = 8;
	res = spi_sync(mb86a35s_spi_device, &message);

	memcpy(data, x.rx_buf + 6, (size+3));
	
	return res;
#endif
#endif
}

/************************************************************************/
/**
	internal function. \n
	SPI write. 1byte write format.  selected sub-address.\n

	@param	suba		[in] register address. (SUBA)
	@param	reg		[in] register address.
	@param	subd		[in] register address. (SUBD)
	@param	data		[in] write data address.
	@param	size		[in] write byte size.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35s_spi_sub_send(unsigned char suba, unsigned char reg,
				 unsigned char subd, unsigned int data,
				 unsigned int size)
{
	int rtn = 0;

	//DBGPRINT(PRINT_LHEADERFMT "** suba[%02x], subd[%02x], reg[%02x], data[%04x], size[%02x]\n", PRINT_LHEADER, (int)suba, (int)subd, (int)reg, (int)data, (int)size);

	rtn = mb86a35s_spi_send(suba, (data << 8) | reg, MB86A35S_SPI_WRITE_2BYTE);
	if (rtn != 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}
end:
	return rtn;
}

/************************************************************************/
/**
	internal function. \n
	SPI read. 1byte read format.  selected sub-address.\n

	@param	suba		[in] register address. (SUBA)
	@param	reg		[in] register address.
	@param	subd		[in] register address. (SUBD)
	@param	data		[out] read data address.
	@param	size		[in] read byte size.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35s_spi_sub_recv(unsigned char suba, unsigned char reg,
				 unsigned char subd, unsigned char *data,
				 unsigned int size)
{
	int rtn = 0;

	disable_irq(MMBICtrlInfo.pSpiDevice->irq);

	/* send SUBA 	*/
	rtn = mb86a35s_spi_send(suba, reg, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* receive data	*/
	rtn = mb86a35s_spi_recv(subd, data, size);
	if (rtn != 0) {
		ERRPRINT("spi receive error.[ %d ]\n", rtn);
		goto end;
	}

	DBGPRINT(PRINT_LHEADERFMT "** suba[%02x], subd[%02x], reg[%02x], data[%04x]\n", PRINT_LHEADER, (int)suba, (int)subd, (int)reg, (int)data);

end:
	enable_irq(MMBICtrlInfo.pSpiDevice->irq);

	return rtn;
}

/************************************************************************/
/**
 	internel function. \n
	RF-IC SPI write. 1byte write format. \n

	@param	reg		[in] register address.
	@param	data		[in] write data address.
	@param	size		[in] write byte size.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
*/
static int mb86a35s_spi_rf_send(unsigned char reg, unsigned int data, unsigned int size)
{
	int rtn = 0;
	u8  _data = 0;

	DBGPRINT(PRINT_LHEADERFMT "** reg[%02x], data[%04x] size[%02x]\n", PRINT_LHEADER, (int)reg, (int)data, (int)size);

/* Add LSI bug recover	*/

#ifdef WORKAROUND_XX

	/* send data 	*/
	rtn = mb86a35s_spi_send(0x3b, 0x3f, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* send data 	*/
	rtn = mb86a35s_spi_send(0x6d, 0x3f, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* send data 	*/
	rtn = mb86a35s_spi_send(0x28, 0xff, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* send data 	*/
	rtn = mb86a35s_spi_send(0x40, 0xff, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* send data 	*/
	rtn = mb86a35s_spi_send(0x50, 0xff, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* read OFDM data 	*/
	rtn = mb86a35s_spi_recv(reg, &_data, MB86A35S_SPI_READ_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}
#endif

	disable_irq(MMBICtrlInfo.pSpiDevice->irq);

	/* SPIEN RF ON	*/
	rtn = mb86a35s_spi_send(MB86A35S_REG_ADDR_SPIF_EN, MB86A35S_SET_OFDN_EN, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* micro sec wait	*/
	_udelay(MB86A35S_SPI_WAITTIME);

	/* send data 	*/
	rtn = mb86a35s_spi_send(reg, data, size);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* micro sec wait	*/
	_udelay(MB86A35S_SPI_WAITTIME);

	/* SPIEN RF OFF	*/
	rtn = mb86a35s_spi_send(MB86A35S_REG_ADDR_SPIF_EN, MB86A35S_SET_RF_DIS, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

#ifdef WORKAROUND_XX

	/* micro sec wait	*/
	_udelay(MB86A35S_SPI_WAITTIME);

	/* send OFDM data 	*/
	rtn = mb86a35s_spi_send(reg, _data, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}
#endif


end:
	enable_irq(MMBICtrlInfo.pSpiDevice->irq);

	return rtn;
}

/************************************************************************/
/**
 	internel function. \n
	RF-IC SPI read. 1byte read format. \n

	@param	reg		[in] register address.
	@param	data		[out] read data address.
	@param	size		[in] read byte size.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
*/
static int mb86a35s_spi_rf_recv(unsigned char reg, unsigned char *data, unsigned int size)
{
	int rtn = 0;
	u8 suba = MB86A35S_REG_ADDR_RFA;
	u8 subd = MB86A35S_REG_ADDR_RFD;

	disable_irq(MMBICtrlInfo.pSpiDevice->irq);

	/* SPIEN RF ON	*/
	rtn = mb86a35s_spi_send(MB86A35S_REG_ADDR_SPIF_EN, MB86A35S_SET_OFDN_EN, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* micro sec wait	*/
	_udelay(MB86A35S_SPI_WAITTIME);

	/* send SUBA 	*/
	rtn = mb86a35s_spi_send(suba, reg, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	/* micro sec wait	*/
	_udelay(MB86A35S_SPI_WAITTIME);

	/* resev SUBD	*/
	rtn = mb86a35s_spi_recv(subd, data, size);
	if (rtn < 0) {
		ERRPRINT("spi receive error.[ %d ]\n", rtn);
		goto end;
	}

	/* micro sec wait	*/
	_udelay(MB86A35S_SPI_WAITTIME);

	/* SPIEN RF OFF	*/
	rtn = mb86a35s_spi_send(MB86A35S_REG_ADDR_SPIF_EN, MB86A35S_SET_RF_DIS, MB86A35S_SPI_WRITE_1BYTE);
	if (rtn < 0) {
		ERRPRINT("spi send error.[ %d ]\n", rtn);
		goto end;
	}

	DBGPRINT(PRINT_LHEADERFMT "** reg[%02x], data[%02x]\n", PRINT_LHEADER, (int)reg, (int)data[0]);

end:
	enable_irq(MMBICtrlInfo.pSpiDevice->irq);

	return rtn;
}

/************************************************************************/
/**
	internal function. \n
	I2C write. \n

	@param	reg		[in] register address.
	@param	value		[in] write data.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_master_send(unsigned int reg, unsigned int value)
{
	int err = 0;

	DBGPRINT(PRINT_LHEADERFMT "** reg[%02x], value[%04x]\n", PRINT_LHEADER,
		 (int)reg, (int)value);

	err = mb86a35s_spi_send(reg, value, MB86A35S_SPI_WRITE_1BYTE);
	if (err < 0) {
		ERRPRINT("spi register address write error [ %d ]\n", err);
	}

	return err;

#if 0	/* I2C I/F	*/
	unsigned char data[2];
	int wsize = 2;
	int err = 0;

	DBGPRINT(PRINT_LHEADERFMT "** reg[%02x], value[%04x]\n", PRINT_LHEADER,
		 (int)reg, (int)value);

	if (mb86a35_i2c_client != NULL) {
		data[0] = reg;
		data[1] = value;

		err = i2c_master_send(mb86a35_i2c_client, &data[0], wsize);
		if (err < 0) {
			ERRPRINT("register address write error [ %d ]\n", err);
			goto i2c_master_send_return;
		} else if (err != wsize) {
			ERRPRINT("register address send error : "
				 "%d bytes transferred.\n", err);
			err = -EIO;
			goto i2c_master_send_return;
		} else {
			/* normal end */
			err = wsize;
		}
	} else {
		ERRPRINT("mb86a35-i2c not attached. [write]\n");
	}
i2c_master_send_return:

#endif
	return err;
}

/************************************************************************/
/**
	internal function. \n
	I2C read. \n

	@param	reg		[in] register address.
	@param	rbuf		[in] read data buffer address.
	@param	count		[in] read data count.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_master_recv(unsigned char reg, unsigned char *rbuf,
				   size_t count)
{
	int err = 0;
	u8 _data = 0;

	switch (count) {
	case 1:
		err = mb86a35s_spi_recv(reg, rbuf, MB86A35S_SPI_WRITE_1BYTE);
		if (err < 0) {
			ERRPRINT("spi register recv error : %d\n", err);
		}
		break;
	case 2:
		err = mb86a35s_spi_recv(reg, &_data, MB86A35S_SPI_WRITE_1BYTE);
		if (err < 0) {
			ERRPRINT("spi register recv error : %d\n", err);
		}
		rbuf[0] = _data;
		err = mb86a35s_spi_recv(reg++, &_data, MB86A35S_SPI_WRITE_1BYTE);
		if (err < 0) {
			ERRPRINT("spi register recv error : %d\n", err);
		}
		rbuf[1] = _data;
		break;
	case 3:
		err = mb86a35s_spi_recv(reg, &_data, MB86A35S_SPI_WRITE_1BYTE);
		if (err < 0) {
			ERRPRINT("spi register recv error : %d\n", err);
		}
		rbuf[0] = _data;
		err = mb86a35s_spi_recv(reg++, &_data, MB86A35S_SPI_WRITE_1BYTE);
		if (err < 0) {
			ERRPRINT("spi register recv error : %d\n", err);
		}
		rbuf[1] = _data;
		err = mb86a35s_spi_recv(reg++, &_data, MB86A35S_SPI_WRITE_1BYTE);
		if (err < 0) {
			ERRPRINT("spi register recv error : %d\n", err);
		}
		rbuf[2] = _data;
		break;
	default:
		break;
	}

	return err;

#if 0	/* I2C I/F	*/
	unsigned char data = reg;
	int wsize = 1;
	int err = 0;

	if (mb86a35_i2c_client != NULL) {
		err = i2c_master_send(mb86a35_i2c_client, &data, wsize);
		if (err < 0) {
			ERRPRINT("register address write error [ %d ]\n", err);
			goto i2c_read_return;
		} else if (err != wsize) {
			ERRPRINT("register address send error : "
				 "%d bytes transferred.\n", err);
			err = -EIO;
			goto i2c_read_return;
		} else {
			/* normal end */
			err = 0;
		}

		err = i2c_master_recv(mb86a35_i2c_client, rbuf, count);
		if (err < 0) {
			ERRPRINT("register recv error : %d\n", err);
			goto i2c_read_return;
		} else if (err != count) {
			ERRPRINT("register recv error : "
				 "%d bytes transferred.\n", err);
			err = -EIO;
			goto i2c_read_return;
		} else {
			/* normal end */
			err = 0;
		}
	} else {
		ERRPRINT("mb86a35-i2c not attached. [read]\n");
	}

#ifdef	DEBUG
	if (STOPLOG == 0)
#endif
		DBGPRINT(PRINT_LHEADERFMT
			 "** reg[%02x], *rbuf[%08x], count[%d], rbuf[%02x]\n",
			 PRINT_LHEADER, (int)reg, (int)rbuf, (int)count,
			 (int)rbuf[0]);

i2c_read_return:

#endif
	return err;
}

/************************************************************************/
/**
	internal function. \n
	I2C write.  selected sub-address.\n

	@param	suba		[in] register address. (SUBA)
	@param	reg		[in] register address.
	@param	subd		[in] register address. (SUBD)
	@param	value		[in] write data.
 */
static void mb86a35_i2c_slave_send(unsigned char suba, unsigned char reg,
				   unsigned char subd, unsigned int value)
{
	int err = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 "** SUBA[%02x], sreg[%02x], SUBD[%02x], value[%04x]\n",
		 PRINT_LHEADER, (int)suba, (int)reg, (int)subd, (int)value);

	err = mb86a35s_spi_sub_send(suba, reg, subd, value, MB86A35S_SPI_WRITE_1BYTE);
	if (err != 0) {
		ERRPRINT("spi sub register send error : %d\n", err);
	}

#if 0	/* I2C I/F	*/
	DBGPRINT(PRINT_LHEADERFMT
		 "** SUBA[%02x], sreg[%02x], SUBD[%02x], value[%04x]\n",
		 PRINT_LHEADER, (int)suba, (int)reg, (int)subd, (int)value);

	if (mb86a35_i2c_client != NULL) {
		mb86a35_i2c_master_send(suba, reg);

		mb86a35_i2c_master_send(subd, value);
	} else {
		ERRPRINT("mb86a35-i2c not attached. [sub write]\n");
	}

#endif
	return;
}

/************************************************************************/
/**
	internal function.
	I2C read. selected sub-address.\n

	@param	suba		[in] register address. (SUBA)
	@param	reg		[in] register address.
	@param	subd		[in] register address. (SUBD)
	@param	rbuf		[out] read data buffer.
	@param	count		[in] read data count.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_slave_recv(unsigned char suba, unsigned char reg,
				  unsigned char subd, unsigned char *rbuf,
				  size_t count)
{
	unsigned char data[4];
	int indx;
	unsigned char adrreg = reg;
	int err = 0;

	memset(data, 0, sizeof(data));
	for (indx = 0; indx < count; indx++) {
		err = mb86a35s_spi_sub_recv(suba, adrreg, subd, data, MB86A35S_SPI_WRITE_1BYTE);
		if (err != 0) {
			ERRPRINT("spi sub register read error : %d bytes transferred.\n", err);
			goto i2c_slave_recv_return;
		}
		rbuf[indx] = data[0];
		adrreg += 1;
	}
#if 0	/* I2C I/F	*/
	if (mb86a35_i2c_client != NULL) {
		for (indx = 0; indx < count; indx++) {
			mb86a35_i2c_master_send(suba, adrreg);

			err = mb86a35_i2c_master_recv(subd, data, 1);
			if (err != 0) {
				ERRPRINT
				    ("sub-register read error : %d bytes transferred.\n",
				     err);
				goto i2c_slave_recv_return;
			}
			rbuf[indx] = data[0];
			adrreg += 1;
		}
	} else {
		ERRPRINT("mb86a35-i2c not attached. [sub read]\n");
	}
#endif

	DBGPRINT(PRINT_LHEADERFMT
		 "** SUBA[%02x], sreg[%02x], SUBD[%02x], *rbuf[%08x], count[%d], rbuf[%02x]\n",
		 PRINT_LHEADER, (int)suba, (int)reg, (int)subd, (int)rbuf,
		 (int)count, (int)rbuf[0]);

i2c_slave_recv_return:
	return err;
}

/************************************************************************/
/**
	internal function. \n
	I2C write.  selected sub-address.\n
	suported 8 / 16 / 24 bits transfer.

	@param	suba		[in] register address. (SUBA)
	@param	reg		[in] register address.
	@param	subd		[in] register address. (SUBD)
	@param	data		[in] write data address.
	@param	mode		[in] 8 / 16 / 24 bits.
 */
static void mb86a35_i2c_sub_send(unsigned char suba, unsigned char reg,
				 unsigned char subd, unsigned char *data,
				 unsigned char mode)
{
	unsigned int value;
	unsigned int err = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 "** SUBA[%02x], sreg[%02x], SUBD[%02x], data[%08x], mode[%02x]\n",
		 PRINT_LHEADER, (int)suba, (int)reg, (int)subd, (int)data,
		 (int)mode);

	memcpy(&value, data, sizeof(value));
	switch (mode) {
	case PARAM_SPI_MODE_SEND:
		err = mb86a35s_spi_sub_send(suba, reg, subd, value, MB86A35S_SPI_WRITE_1BYTE);
		if (err != 0) {
			ERRPRINT("sub-register write error [ %d ].\n", err);
		}
		break;
	case PARAM_SPI_MODE_SEND_16:
		err = mb86a35s_spi_sub_send(suba, reg, subd, value, MB86A35S_SPI_WRITE_2BYTE);
		if (err != 0) {
			ERRPRINT("sub-register write error [ %d ].\n", err);
		}
		break;
	case PARAM_SPI_MODE_SEND_24:
		err = mb86a35s_spi_sub_send(suba, reg, subd, value, MB86A35S_SPI_WRITE_3BYTE);
		if (err != 0) {
			ERRPRINT("sub-register write error [ %d ].\n", err);
		}
		break;
	}
#if 0	/* I2C I/F	*/
	unsigned char regsubd = subd;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 "** SUBA[%02x], sreg[%02x], SUBD[%02x], data[%08x], mode[%02x]\n",
		 PRINT_LHEADER, (int)suba, (int)reg, (int)subd, (int)data,
		 (int)mode);

	if (mb86a35_i2c_client != NULL) {
		mb86a35_i2c_master_send(suba, reg);

		value = data[0];
		mb86a35_i2c_master_send(regsubd, value);

		switch (mode) {
		case PARAM_I2C_MODE_SEND_16:
			regsubd += 1;
			value = data[1];
			mb86a35_i2c_master_send(regsubd, value);
			break;
		case PARAM_I2C_MODE_SEND_24:
			regsubd += 1;
			value = data[1];
			mb86a35_i2c_master_send(regsubd, value);

			regsubd += 1;
			value = data[2];
			mb86a35_i2c_master_send(regsubd, value);
			break;
		}
	} else {
		ERRPRINT("mb86a35-i2c not attached. [sub write]\n");
	}
#endif
	return;
}

/************************************************************************/
/**
	internal function.
	I2C read. selected sub-address.\n

	@param	suba		[in] register address. (SUBA)
	@param	reg		[in] register address.
	@param	subd		[in] register address. (SUBD)
	@param	rbuf		[out] read data buffer.
	@param	mode		[in] 8 / 16 / 24 bits.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_sub_recv(unsigned char suba, unsigned char reg,
				unsigned char subd, unsigned char *data,
				unsigned char mode)
{
	int err = 0;

	/* 1msec wait	*/
	switch (mode) {
	case PARAM_SPI_MODE_RECV:
		err = mb86a35s_spi_sub_recv(suba, reg, subd, &data[0], MB86A35S_SPI_READ_1BYTE);
		if (err != 0) {
			ERRPRINT("sub-register read error : %d bytes transferred.\n", err);
			goto i2c_sub_recv_return;
		}
		break;
	case PARAM_SPI_MODE_RECV_16:
		err = mb86a35s_spi_sub_recv(suba, reg, subd, &data[0], MB86A35S_SPI_READ_1BYTE);
		if (err != 0) {
			ERRPRINT("sub-register read error : %d bytes transferred.\n", err);
			goto i2c_sub_recv_return;
		}
		err = mb86a35s_spi_sub_recv(suba, reg++, subd, &data[1], MB86A35S_SPI_READ_1BYTE);
		if (err != 0) {
			ERRPRINT("sub-register read error : %d bytes transferred.\n", err);
			goto i2c_sub_recv_return;
		}
		break;
	case PARAM_SPI_MODE_RECV_24:
		err = mb86a35s_spi_sub_recv(suba, reg, subd, &data[0], MB86A35S_SPI_READ_1BYTE);
		if (err != 0) {
			ERRPRINT("sub-register read error : %d bytes transferred.\n", err);
			goto i2c_sub_recv_return;
		}
		err = mb86a35s_spi_sub_recv(suba, reg, subd, &data[1], MB86A35S_SPI_READ_1BYTE);
		if (err != 0) {
			ERRPRINT("sub-register read error : %d bytes transferred.\n", err);
			goto i2c_sub_recv_return;
		}
		err = mb86a35s_spi_sub_recv(suba, reg++, subd, &data[2], MB86A35S_SPI_READ_1BYTE);
		if (err != 0) {
			ERRPRINT("sub-register read error : %d bytes transferred.\n", err);
			goto i2c_sub_recv_return;
		}
		break;
	}
#if 0	/* I2C I/F	*/
	unsigned char regsubd = subd;
	int err = 0;

	if (mb86a35_i2c_client != NULL) {
		mb86a35_i2c_master_send(suba, reg);

		err = mb86a35_i2c_master_recv(regsubd, &data[0], 1);
		if (err != 0) {
			ERRPRINT
			    ("sub-register read error : %d bytes transferred.\n",
			     err);
			goto i2c_sub_recv_return;
		}

		switch (mode) {
		case PARAM_I2C_MODE_RECV_16:
			regsubd += 1;
			err = mb86a35_i2c_master_recv(regsubd, &data[1], 1);
			if (err != 0) {
				ERRPRINT
				    ("sub-register read error : %d bytes transferred.\n",
				     err);
				goto i2c_sub_recv_return;
			}
			break;
		case PARAM_I2C_MODE_RECV_24:
			regsubd += 1;
			err = mb86a35_i2c_master_recv(regsubd, &data[1], 1);
			if (err != 0) {
				ERRPRINT
				    ("sub-register read error : %d bytes transferred.\n",
				     err);
				goto i2c_sub_recv_return;
			}

			regsubd += 1;
			err = mb86a35_i2c_master_recv(regsubd, &data[2], 1);
			if (err != 0) {
				ERRPRINT
				    ("sub-register read error : %d bytes transferred.\n",
				     err);
				goto i2c_sub_recv_return;
			}
			break;
		}
	} else {
		ERRPRINT("mb86a35-i2c not attached. [sub read]\n");
	}
#endif

	DBGPRINT(PRINT_LHEADERFMT
		 "** SUBA[%02x], sreg[%02x], SUBD[%02x], *data[%08x], mode[%02x] : data[%02x:%02x:%02x]\n",
		 PRINT_LHEADER, (int)suba, (int)reg, (int)subd, (int)data, mode,
		 data[0], data[1], data[2]);

i2c_sub_recv_return:
	return err;
}

/************************************************************************/
/**
	internal function.
	RF-IC I2C write data. \n

	@param	reg		[in] RF register address.
	@param	value		[in] write data.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_rf_send(unsigned char reg, unsigned int value)
{
	int err;

	DBGPRINT(PRINT_LHEADERFMT " : RFreg[%02x], value:[%04x]\n",
		 PRINT_LHEADER, (int)reg, (int)value);

	err = mb86a35s_spi_rf_send(reg, value, MB86A35S_SPI_WRITE_1BYTE);
	if (err < 0) {
		ERRPRINT("register RF recv error : %d bytes transferred.\n", err);
		err = -EIO;
	}

#if 0	/* I2C I/F	*/
	struct i2c_adapter *adap = mb86a35_i2c_client->adapter;
	struct i2c_msg msg;
	u8 data[4];
	unsigned char sreg = MB86A35_REG_ADDR_SUBSELECT;
	int err;

	DBGPRINT(PRINT_LHEADERFMT " : RFreg[%02x], value:[%04x]\n",
		 PRINT_LHEADER, (int)reg, (int)value);

	/* RF-IC mode set */
	data[0] = sreg;
	data[1] = 0;
	data[2] = 0;
	msg.addr = mb86a35_i2c_client->addr;
	msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
	msg.len = 2;
	msg.buf = (char *)data;

	err = i2c_transfer(adap, &msg, 1);
	if (err != 1) {
		ERRPRINT("register RF recv error : "
			 "%d bytes transferred.\n", err);
		err = -EIO;
		goto rf_write_return;
	} else {
		/* normal end */
		err = 0;
	}

	/* RF-IC Access */
	data[0] = reg;
	data[1] = value;
	data[2] = 0;
	msg.addr = MB86A35_I2C_RFICADDRESS;
	msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
	msg.len = 2;
	msg.buf = (char *)data;

	err = i2c_transfer(adap, &msg, 1);
	if (err != 1) {
		ERRPRINT("register RF recv error : "
			 "%d bytes transferred.\n", err);
		err = -EIO;
		goto rf_write_return;
	} else {
		/* normal end */
		err = 0;
	}


//	err = i2c_transfer(adap, &msg, 1);
//	if (err != 1) {
//		ERRPRINT("register RF recv error : "
//			 "%d bytes transferred.\n", err);
//		err = -EIO;
//		goto rf_write_return;
//	} else {
//		/* normal end */
//		err = 0;
//	}

	/* RF-IC mode reset */
//	data[0] = sreg;
//	data[1] = 0x01;
//	data[2] = 0;
//	msg.addr = mb86a35_i2c_client->addr;
//	msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
//	msg.len = 2;
//	msg.buf = (char *)data;

//	err = i2c_transfer(adap, &msg, 1);
//	if (err != 1) {
//		ERRPRINT("register RF recv error : "
//			 "%d bytes transferred.\n", err);
//		err = -EIO;
//		goto rf_write_return;
//	} else {
//		/* normal end */
//		err = 0;
//	}

rf_write_return:

#endif
	return err;
}

/************************************************************************/
/**
	internal function. \n
	I2C write. with mask.\n

	@param	reg		[in] register address.
	@param	value		[in] write data.
	@param	I2C_MASK		[in] I2C mask data.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_master_send_mask(unsigned int reg, unsigned int value,
					u8 I2C_MASK, u8 PARAM_MASK)
{
	unsigned int svalue = 0;
	u8 I2C_DATA = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 "** reg[%02x], value[%04x], I2C_MASK[%02x], PARAM_MASK[%02x]\n",
		 PRINT_LHEADER, (int)reg, (int)value, (int)I2C_MASK,
		 (int)PARAM_MASK);

        disable_irq(MMBICtrlInfo.pSpiDevice->irq);

	rtncode = mb86a35_i2c_master_recv(reg, &I2C_DATA, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto i2c_master_send_mask_return;
	}
	svalue = I2C_DATA & I2C_MASK;

	svalue |= (value & PARAM_MASK);
	mb86a35_i2c_master_send(reg, svalue);

i2c_master_send_mask_return:
        enable_irq(MMBICtrlInfo.pSpiDevice->irq);

	return rtncode;
}

/************************************************************************/
/**
	internal function. \n
	I2C write.  selected sub-address.  with mask.\n

	@param	suba		[in] register address. (SUBA)
	@param	reg		[in] register address.
	@param	subd		[in] register address. (SUBD)
	@param	value		[in] write data.
	@param	I2C_MASK		[in] I2C mask data.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_slave_send_mask(unsigned char suba, unsigned char reg,
				       unsigned char subd, unsigned int value,
				       u8 I2C_MASK, u8 PARAM_MASK)
{
	unsigned int svalue = 0;
	u8 I2C_DATA = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 "** SUBA[%02x], sreg[%02x], SUBD[%02x], value[%04x], I2C_MASK[%02x]\n",
		 PRINT_LHEADER, (int)suba, (int)reg, (int)subd, (int)value,
		 (int)I2C_MASK);

        disable_irq(MMBICtrlInfo.pSpiDevice->irq);

	rtncode = mb86a35_i2c_slave_recv(suba, reg, subd, &I2C_DATA, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto i2c_slave_send_mask_return;
	}
	svalue = I2C_DATA & I2C_MASK;

	svalue |= (value & PARAM_MASK);
	mb86a35_i2c_slave_send(suba, reg, subd, svalue);

i2c_slave_send_mask_return:
        enable_irq(MMBICtrlInfo.pSpiDevice->irq);

	return rtncode;
}

/************************************************************************/
/**
	internal function.
	I2C sub area read. \n

	@param	reg		[in] RF register address.
	@param	rbuf		[in] read data buffer address.
	@param	count		[in] read data count.
	@retval	=0	normal return.
	@retval	!=0	The error occurred. The detailed information is set to an errno.
 */
static int mb86a35_i2c_rf_recv(unsigned char reg, unsigned char *rbuf,
			       size_t count)
{
	int err  = 0;
	int indx = 0;

	for (indx = 0; indx < count; indx++) {
		err = mb86a35s_spi_rf_recv(reg+indx, &rbuf[indx], MB86A35S_SPI_WRITE_1BYTE);
		if (err < 0) {
			ERRPRINT("register RF receive error : %d bytes transferred.\n", err);
			err = -EIO;
		}
	}

	DBGPRINT(PRINT_LHEADERFMT
		 " : RFreg[%02x], *rbuf[%08x], count[%d], rbuf[%02x]\n",
		 PRINT_LHEADER, (int)reg, (int)rbuf, (int)count, (int)rbuf[0]);

#if 0	/* I2C I/F	*/
	struct i2c_adapter *adap = mb86a35_i2c_client->adapter;
	struct i2c_msg msg;
	u8 data[4];
	unsigned int value = 0;
	unsigned char sreg = MB86A35_REG_ADDR_SUBSELECT;
	int err;
	int indx;

	for (indx = 0; indx < count; indx++) {
		/* RF-IC mode set */
		data[0] = sreg;
		data[1] = value;
		data[2] = 0;
		msg.addr = mb86a35_i2c_client->addr;
		msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
		msg.len = 2;
		msg.buf = (char *)data;

		err = i2c_transfer(adap, &msg, 1);
		if (err != 1) {
			ERRPRINT("register RF recv error : "
				 "%d bytes transferred.\n", err);
			err = -EIO;
			goto rf_read_return;
		} else {
			/* normal end */
			err = 0;
		}

		/* RF-IC Access *//* Read register address write */
		data[0] = reg + indx;
		data[1] = 0;
		data[2] = 0;
		msg.addr = MB86A35_I2C_RFICADDRESS;
		msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
		msg.len = 1;
		msg.buf = (char *)data;

		err = i2c_transfer(adap, &msg, 1);
		if (err != 1) {
			ERRPRINT("register RF recv error : "
				 "%d bytes transferred.\n", err);
			err = -EIO;
			goto rf_read_return;
		} else {
			/* normal end */
			err = 0;
		}

		/* RF-IC mode set */
		data[0] = sreg;
		data[1] = value;
		data[2] = 0;
		msg.addr = mb86a35_i2c_client->addr;
		msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
		msg.len = 2;
		msg.buf = (char *)data;

		err = i2c_transfer(adap, &msg, 1);
		if (err != 1) {
			ERRPRINT("register RF recv error : "
				 "%d bytes transferred.\n", err);
			err = -EIO;
			goto rf_read_return;
		} else {
			/* normal end */
			err = 0;
		}

		/* RF-IC Access *//* Data read */
		msg.addr = MB86A35_I2C_RFICADDRESS;
		msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
		msg.flags |= I2C_M_RD;
		msg.len = 1;
		msg.buf = (char *)&rbuf[indx];

		err = i2c_transfer(adap, &msg, 1);
		if (err != 1) {
			ERRPRINT("register RF recv error : "
				 "%d bytes transferred.\n", err);
			err = -EIO;
			goto rf_read_return;
		} else {
			/* normal end */
			err = 0;
		}
	}

	/* RF-IC mode reset */
//	data[0] = sreg;
//	data[1] = 0x01;
//	data[2] = 0;
//	msg.addr = mb86a35_i2c_client->addr;
//	msg.flags = mb86a35_i2c_client->flags & I2C_M_TEN;
//	msg.len = 2;
//	msg.buf = (char *)data;

//	err = i2c_transfer(adap, &msg, 1);
//	if (err != 1) {
//		ERRPRINT("register RF recv error : "
//			 "%d bytes transferred.\n", err);
//		err = -EIO;
//		goto rf_read_return;
//	} else {
//		/* normal end */
//		err = 0;
//	}

	DBGPRINT(PRINT_LHEADERFMT
		 " : RFreg[%02x], *rbuf[%08x], count[%d], rbuf[%02x]\n",
		 PRINT_LHEADER, (int)reg, (int)rbuf, (int)count, (int)rbuf[0]);

rf_read_return:

#endif

	return err;
}

/************************************************************************/
/**
	RF-IC Channel setting. \n
	CALBVCOSR controled. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	REG2B		[in] REG2B data.
	@param	REG61		[in] REG61 data.
	@retval	=0	normal return.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
void mb86a35_RF_channel_calbvoscr(mb86a35_cmdcontrol_t * cmdctrl, u8 REG2B,
				  u8 REG61)
{
	unsigned int reg;
	unsigned int value;

	/* CALBVCOSR */
	reg = 0x2B;
	value = REG2B;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x61;
	value = (REG61 | 0x08);
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x2B;
	value = (REG2B | 0x20);
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x61;
	value = REG61;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x2B;
	value = REG2B;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x61;
	value = REG61;
	mb86a35_i2c_rf_send(reg, value);

	return;
}

/************************************************************************/
/**
	RF-IC Channel setting. \n
	A35_FREQ table checking. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	mode		[in] UHF/VHF mode.
	@param	chno		[in] channel number.
	@retval	=NULL	no match channel number.
	@retval	!=NULL	struct pointer of "MB86A35_FREQ".
*/

static
mb86a35_freq_t *mb86a35_RF_channel_tblcheck(u8 mode, u8 chno)
{
  
  //ES2.0
  mb86a35_freq_t *freq = NULL;
  int indx = 0;

	switch (mode) {
		/* UHF */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
		for (indx = 0; mb86a35_freq_UHF[indx].CH != 0xFF; indx++) {
			if (mb86a35_freq_UHF[indx].CH == chno) {
				freq = &mb86a35_freq_UHF[indx];
				goto channel_tblcheck_return;
			}
		}
		freq = NULL;
		break;

		/* VHF */
	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
		for (indx = 0; mb86a35_freq_VHF[indx].CH != 0xFF; indx++) {
			if (mb86a35_freq_VHF[indx].CH == chno) {
				freq = &mb86a35_freq_VHF[indx];
				goto channel_tblcheck_return;
			}
		}
		freq = NULL;
		break;

	default:
		freq = NULL;
		break;
	}

channel_tblcheck_return:
	return freq;
}

static
mb86a35_freq1_t *mb86a35_RF_channel_tblcheck1(u8 mode, u8 chno)
{
  //ES3.0
  mb86a35_freq1_t *freq = NULL;
  int indx = 0;

	switch (mode) {
		/* UHF */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
		for (indx = 0; mb86a35_freq1_UHF[indx].CH != 0xFF; indx++) {
			if (mb86a35_freq1_UHF[indx].CH == chno) {
				freq = &mb86a35_freq1_UHF[indx];
				goto channel_tblcheck1_return;
			}
		}
		freq = NULL;
		break;

		/* VHF */
	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
		for (indx = 0; mb86a35_freq1_VHF[indx].CH != 0xFF; indx++) {
			if (mb86a35_freq1_VHF[indx].CH == chno) {
				freq = &mb86a35_freq1_VHF[indx];
				goto channel_tblcheck1_return;
			}
		}
		freq = NULL;
		break;

	default:
		freq = NULL;
		break;
	}

channel_tblcheck1_return:
	return freq;
}


/************************************************************************/
/**
	internal function. \n
	RF-IC Channel setting. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	mode		[in] UHF/VHF mode.
	@param	chno		[in] channel number.
	@retval	=0	normal return.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_RF_channel(mb86a35_cmdcontrol_t * cmdctrl, u8 mode, u8 chno)
{
  int rtncode_rf = 0;
  unsigned char reg_rf = 0;
  u8 chipid0_rf = 0;
  int rtncode = 0;
  unsigned int reg;
  unsigned int value;
   rtncode_rf = mb86a35_i2c_rf_recv(reg_rf, &chipid0_rf, 1);
   if (rtncode_rf != 0) {
    		rtncode = -EFAULT;
    		goto rf_channel_errreturn;
    }

   printk("mb86a35_RF_channel chipid %d\n", chipid0_rf);

   if (chipid0_rf == 0x03) {    //ES2.0 start

	//int rtncode = 0;
	int loopcnt = 0;
	//unsigned int reg;
	//unsigned int value;

	int swUHF = -1;
	int chselect = -1;
	int varconsel = 0;
	u8 ICONDIVL = 0;
	u8 PLLN = 0;
	u8 PLL_lock = 0;
	u8 REG07 = 0;
	u8 REG25 = 0;
	u8 REG28 = 0;
	u8 REG29 = 0;
	u8 REG2A = 0;
	u8 REG2B = MB86A35_DEF_REG2B;
	u8 REG60 = 0;
	u8 REG61 = 0;
	u8 REG62 = 0;

	mb86a35_freq_t *freq;

#define	PLLN_UHF_13		0x1D
#define	PLLN_UHF_41		0x14
#define	PLLN_VHF		0x1B
#define	PLLF_UHF_13		0x092492
#define	PLLF_UHF_41		0x009249
#define	PLLF_VHF		0x064924

	struct mb86a35_reg22_reg3A {
		u8 REG22;
		u8 REG34;
		u8 REG35;
		u8 REG36;
		u8 REG37;
		u8 REG38;
		u8 REG39;
		u8 REG3A;
	} mb86a35_uhf_regdata[6] = {
		{
		0xFC, 0xC0, 0x80, 0xFF, 0xF8, 0x12, 0xC6, 0x0A}, {
		0xFC, 0x80, 0x80, 0xBF, 0xF8, 0x12, 0xC6, 0x0A}, {
		0xFC, 0x40, 0x40, 0xBF, 0x88, 0x12, 0xC6, 0x0A}, {
		0xF8, 0x40, 0x40, 0xBF, 0x88, 0x12, 0xC6, 0x0A}, {
		0xF0, 0x40, 0x00, 0xBF, 0x88, 0x12, 0xC6, 0x0A}, {
	0xF0, 0x00, 0x00, 0xBF, 0xF8, 0x12, 0xC6, 0x02},};

	struct mb86a35_reg8D_reg3C {
		u8 REG8D;
		u8 REG23;
		u8 REG3A;
		u8 REG3B;
		u8 REG3C;
	} mb86a35_vhf_regdata[4] = {
		{
		0xF0, 0x58, 0x05, 0x4D, 0x14}, {
		0xF0, 0x48, 0x04, 0x3C, 0x13}, {
		0x00, 0x58, 0x02, 0x19, 0x11}, {
	0x00, 0x48, 0x02, 0x19, 0x11},};

	struct mb86a35_varcon {
		u8 VARCON;
		u8 VCOBAND;
		u8 ICCONVCOBUF;
		u8 ICCONBUFUHF;
		u8 PLLS;
		u8 VCOLOADBAND;
	} mb86a35_varcon_regdata[4] = {
		{
		0x00, 0x00, 0x02, 0x07, 0x01, 0x03}, {
		0x03, 0x03, 0x02, 0x07, 0x01, 0x03}, {
		0x00, 0x03, 0x07, 0x00, 0x04, 0x01}, {
	0x00, 0x03, 0x07, 0x00, 0x08, 0x00},};

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,mode:0x%02x,ch_no:0x%02x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)mode, (int)chno);

	switch (mode) {
		/* UHF */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
		swUHF = MB86A35_SELECT_UHF;
		break;

		/* VHF */
	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
		swUHF = MB86A35_SELECT_VHF;
		break;

	default:
		rtncode = -EINVAL;
		goto rf_channel_errreturn;
		break;
	}

	freq = mb86a35_RF_channel_tblcheck(mode, chno);
	if (freq == NULL) {
		rtncode = -EINVAL;
		goto rf_channel_errreturn;
	}
	PLLN = freq->PLLN;
	REG28 = freq->REG28;
	REG29 = freq->REG29;
	REG2A = freq->REG2A;

	loopcnt = 0;
	do {
		printk("mb86a35_RF_channel count %d\n", loopcnt);
		if (swUHF == MB86A35_SELECT_UHF) {
			/* UHF */
			reg = 0x88;
			value = 0x10;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x93;
			value = 0x48;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x86;
			value = 0xFF;
			mb86a35_i2c_rf_send(reg, value);

			if ((chno >= 13) && (chno <= 19))
				chselect = 0;
			if ((chno >= 20) && (chno <= 25))
				chselect = 1;
			if ((chno >= 26) && (chno <= 29))
				chselect = 2;
			if ((chno >= 30) && (chno <= 35))
				chselect = 3;
			if ((chno >= 36) && (chno <= 50))
				chselect = 4;
			if ((chno >= 51) && (chno <= 52))
				chselect = 5;
			if (chselect < 0) {
				rtncode = -EINVAL;
				goto rf_channel_errreturn;
			}

			reg = 0x22;
			value = mb86a35_uhf_regdata[chselect].REG22;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x34;
			value = mb86a35_uhf_regdata[chselect].REG34;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x35;
			value = mb86a35_uhf_regdata[chselect].REG35;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x36;
			value = mb86a35_uhf_regdata[chselect].REG36;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x37;
			value = mb86a35_uhf_regdata[chselect].REG37;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x38;
			value = mb86a35_uhf_regdata[chselect].REG38;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x39;
			value = mb86a35_uhf_regdata[chselect].REG39;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x3A;
			value = mb86a35_uhf_regdata[chselect].REG3A;
			mb86a35_i2c_rf_send(reg, value);
		}

		if (swUHF == MB86A35_SELECT_VHF) {
			reg = 0x88;
			value = 0x00;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x93;
			value = 0x40;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x86;
			value = 0xFF;
			mb86a35_i2c_rf_send(reg, value);

			if ((chno > 0) && (chno < 30))
				chselect = 2;
			else
				chselect = 3;

			reg = 0x8D;
			value = mb86a35_vhf_regdata[chselect].REG8D;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x23;
			value = mb86a35_vhf_regdata[chselect].REG23;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x3A;
			value = mb86a35_vhf_regdata[chselect].REG3A;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x3B;
			value = mb86a35_vhf_regdata[chselect].REG3B;
			mb86a35_i2c_rf_send(reg, value);

			reg = 0x3C;
			value = mb86a35_vhf_regdata[chselect].REG3C;
			mb86a35_i2c_rf_send(reg, value);
		}

		ICONDIVL = 0x00;
		if (swUHF == MB86A35_SELECT_UHF) {
			if ((13 <= chno) && (chno <= 37))
				ICONDIVL = 0x02;
			if ((38 <= chno) && (chno <= 39))
				ICONDIVL = 0x04;
			if ((40 <= chno) && (chno <= 52))
				ICONDIVL = 0x05;
			if ((53 <= chno))
				ICONDIVL = 0x06;
		} else {
			ICONDIVL = 0x00;
		}
		reg = 0x60;
		REG60 = 0x88 | ((ICONDIVL << 4) | ICONDIVL);
		value = REG60;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x61;
		REG61 = ((ICONDIVL << 4) | 0x03);
		value = REG61;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x62;
		REG62 = 0x08 + ICONDIVL;
		value = REG62;
		mb86a35_i2c_rf_send(reg, value);

		REG25 = 0x00;
		if (swUHF == MB86A35_SELECT_UHF) {
			if ((13 <= chno) && (chno <= 52))
				REG25 = 0xB8;
		} else {
			REG25 = 0xE8;
		}

		varconsel = 0;
		if (swUHF == MB86A35_SELECT_UHF) {
			if ((13 <= chno) && (chno <= 34))
				varconsel = 0;
			if ((35 <= chno) && (chno <= 52))
				varconsel = 1;
		} else {
			varconsel = 2;
		}

		reg = 0x25;
		value = REG25 + mb86a35_varcon_regdata[varconsel].VARCON;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x51;
		value =
		    0x03 + (mb86a35_varcon_regdata[varconsel].ICCONVCOBUF << 4);
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x4F;
		value =
		    0x0F + (mb86a35_varcon_regdata[varconsel].ICCONBUFUHF << 4);
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x26;
		value = (mb86a35_varcon_regdata[varconsel].VCOLOADBAND << 6)
		    + (mb86a35_varcon_regdata[varconsel].VCOBAND << 2);
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x27;
		value = PLLN;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x28;
		value = REG28;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x29;
		value = REG29;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x2A;
		value = REG2A;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x8C;
		value = 0x2D;
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x54;
		value = 0x81;
		mb86a35_i2c_rf_send(reg, value);

		/* CALBVCOSR */
		mb86a35_RF_channel_calbvoscr(cmdctrl, REG2B, REG61);

		mdelay(1);

		reg = 0x07;
		rtncode = mb86a35_i2c_rf_recv(reg, &REG07, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto rf_channel_errreturn;
		}

		PLL_lock = REG07 & 0x01;
		if (PLL_lock == 0) {
			/* CALBVCOSR */
			mb86a35_RF_channel_calbvoscr(cmdctrl, REG2B, REG61);

			mdelay(2);

			reg = 0x07;
			rtncode = mb86a35_i2c_rf_recv(reg, &REG07, 1);
			if (rtncode != 0) {
				rtncode = -EFAULT;
				goto rf_channel_errreturn;
			}
			PLL_lock = REG07 & 0x01;
		}
		if (PLL_lock == 1)
			goto rf_channel_norreturn;

		mdelay(1);

	} while (loopcnt++ < MB86A35_RF_CHANNEL_SET_TIMEOUTCOUNT);

	rtncode = -ETIMEDOUT;

   }          //ES2.0 end
   else {     //ES3.0 start

	//int rtncode = 0;
	//unsigned int reg;
	//unsigned int value;

	u32 ui32localOscFreq,ui32RadioFreq ;
	u32 ui32pllFreq,ui32RefereceFreq;
	u8 pui8plls = 1;   //,PLLLOCK=0;
	u32 tmp;
	u8 pll_mul, r_div=8;
	u8 VCOLOADBANDF_I2C = 0;
	
	int swUHF = -1;
	mb86a35_freq1_t *freq;
	
	u32 ui32PLLN, ui32PLLF; 
	u8 VCOBAND=0;
	u8 WR51=0, MAXVCOOUT=0x0B;
	u8 VCORG,REG06,REG86;
	u8 ui8REVID;
	
	ui8REVID  = chipid0_rf & 0x0F;

	DBGPRINT(PRINT_LHEADERFMT
		 " : LGE Test (cmdctrl:0x%08x,mode:0x%02x,ch_no:0x%02x) %d called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)mode, (int)chno, ui8REVID);

	freq = mb86a35_RF_channel_tblcheck1(mode, chno);
	if (freq == NULL) {
		rtncode = -EINVAL;
		goto rf_channel_errreturn;
	}
	
	ui32RadioFreq = freq->FREQ;

	switch (mode) {
		/* UHF */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
		swUHF = MB86A35_SELECT_UHF;
		break;

		/* VHF */
	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
		swUHF = MB86A35_SELECT_VHF;
		break;

	default:
		rtncode = -EINVAL;
		goto rf_channel_errreturn;
		break;
	}

	switch(mode)
	{
		case PARAM_MODE_ISDBT_1UHF:
		case PARAM_MODE_ISDBTMM_1VHF:
		case PARAM_MODE_ISDBTSB_1VHF:

		reg = 0x86;
		rtncode = mb86a35_i2c_rf_recv(reg, &REG86, 1);
		
		if (rtncode != 0) {
    		rtncode = -EFAULT;
    		goto rf_channel_errreturn;
    	}
			if(REG86 == 0x00) ui32localOscFreq = ui32RadioFreq - 352;   //Lower IF type
			else  ui32localOscFreq = ui32RadioFreq + 352;   //Upper IF type

				reg = 0x24;  
				value = 0x01;
				mb86a35_i2c_rf_send(reg, value); 
				//WriteIIC (0x24, 0x01 ); // LIFEN<0> = 1 
				reg = 0x30;  
				value = 0x0E;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x30, 0x0E ); // SWPDUPMIX<5> = 0, SWPDCKSYN<1> = 1,  default : 0x2E
			

			if (swUHF == MB86A35_SELECT_UHF)
			{
				VCOLOADBANDF_I2C = 1;
				reg = 0x39;  
				value = 0xFF;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0xFF );  //ENCLK_UPMIX<7> = 1
			}
			else 
			{
				VCOLOADBANDF_I2C = 3;
				reg = 0x39;  
				value = 0xFF;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0x80 );  //ENCLK_UPMIX<7> = 1
			}
			break;

		case PARAM_MODE_ISDBTSB_3VHF: 

			ui32localOscFreq = ui32RadioFreq;
			
			//if(g_oldAppBW != g_setAppBW) //below setting doesn't need when same AppBW with previous setting.
			//{
				reg = 0x24;  
				value = 0x00;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x24, 0x00 ); // LIFEN<0> = 0
				reg = 0x30;  
				value = 0x2E;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x30, 0x2E ); // SWPDUPMIX<5> = 1, SWPDCKSYN<1> = 1,  default : 0x2E
				
				//g_oldAppBW = g_setAppBW;
				
			//}

			if (swUHF == MB86A35_SELECT_UHF)
			{
				VCOLOADBANDF_I2C = 1;
				reg = 0x39;  
				value = 0x7F;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0x7F );  //ENCLK_UPMIX<7> = 0
			}
			else 
			{
				VCOLOADBANDF_I2C = 3;
				reg = 0x39;  
				value = 0x00;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0x00 );  //ENCLK_UPMIX<7> = 0
			}
			break;

		case PARAM_MODE_ISDBT_13UHF:
		case PARAM_MODE_ISDBTMM_13VHF:
		
			ui32localOscFreq = ui32RadioFreq;
			
			//if(g_oldAppBW != g_setAppBW) //below setting doesn't need when same AppBW with previous setting.
			//{
				//CalbCtuneWR(_ISDBTFULLSEG);
				reg = 0x24;  
				value = 0x00;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x24, 0x00 ); // LIFEN<0> = 0
				reg = 0x30;  
				value = 0x2E;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x30, 0x2E ); // SWPDUPMIX<5> = 1, SWPDCKSYN<1> = 1,  default : 0x2E
				
				//g_oldAppBW = g_setAppBW;
				
			//}

			if (swUHF == MB86A35_SELECT_UHF)
			{
				VCOLOADBANDF_I2C = 1;
				reg = 0x39;  
				value = 0x7F;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0x7F );  //ENCLK_UPMIX<7> = 0
			}
			else 
			{
				VCOLOADBANDF_I2C = 3;
				reg = 0x39;  
				value = 0x00;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0x00 );  //ENCLK_UPMIX<7> = 0
			}
			break;

		default:  //same as Full Seg

			ui32localOscFreq = ui32RadioFreq;
			
			//if(g_oldAppBW != g_setAppBW) //below setting doesn't need when same AppBW with previous setting.
			//{
				//CalbCtuneWR(_ISDBTFULLSEG);
				reg = 0x24;  
				value = 0x00;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x24, 0x00 ); // LIFEN<0> = 0
				reg = 0x30;  
				value = 0x2E;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x30, 0x2E ); // SWPDUPMIX<5> = 1, SWPDCKSYN<1> = 1,  default : 0x2E
				
				//g_oldAppBW = g_setAppBW;
				
			//}

			if (swUHF == MB86A35_SELECT_UHF)
			{
				VCOLOADBANDF_I2C = 1;
				reg = 0x39;  
				value = 0x7F;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0x7F );  //ENCLK_UPMIX<7> = 0
			}
			else 
			{
				VCOLOADBANDF_I2C = 3;
				reg = 0x39;  
				value = 0x00;
				mb86a35_i2c_rf_send(reg, value);
				//WriteIIC (0x39, 0x00 );  //ENCLK_UPMIX<7> = 0
			}
			
			break;
	}

		if(ui8REVID == 1) MAXVCOOUT = 0x0B;   //For CS1 chip Setting.
		// Modified by prajuna 20121028 for MAXVCOOUT {
		/*
		else MAXVCOOUT = 0x05;  //For CS2 chip Setting.
		*/
		//
		else
			MAXVCOOUT = 0x07;	// For CS2 chip setting.
		//
		// Modified by prajuna 20121028 for MAXVCOOUT }
		
		reg = 0x51;
		rtncode = mb86a35_i2c_rf_recv(reg, &WR51, 1);
		if (rtncode != 0) {
    		rtncode = -EFAULT;
    		goto rf_channel_errreturn;
    	}
		WR51 = WR51 & 0x87;
		//WR51 = ReadIIC(0x51) & 0x87;		

		reg = 0x51;  
		value = WR51 | (MAXVCOOUT << 3);
		mb86a35_i2c_rf_send(reg, value);
		//WriteIIC (0x51, WR51 | (MAXVCOOUT << 3)); //VCOLDO_OUTSEL_I2C

		if ((ui32localOscFreq < 816000)&&(ui32localOscFreq >= 760000))		 //WriteIIC (0x53, 0x7F);	 // EN_AUTO_VCOVARCON = 0 ;
		{
			reg = 0x53;  
			value = 0x7F;
			mb86a35_i2c_rf_send(reg, value);
		}
		else if ((ui32localOscFreq < 408000)&&(ui32localOscFreq >= 380000))  //WriteIIC (0x53, 0x7F);  // EN_AUTO_VCOVARCON = 0 ;
		{
			reg = 0x53;  
			value = 0x7F;
			mb86a35_i2c_rf_send(reg, value);
		}
		else if ((ui32localOscFreq < 204000)&&(ui32localOscFreq >= 190000))  //WriteIIC (0x53, 0x7F);  // EN_AUTO_VCOVARCON = 0 ;
		{
			reg = 0x53;  
			value = 0x7F;
			mb86a35_i2c_rf_send(reg, value);
		}
		else if ((ui32localOscFreq < 102000)&&(ui32localOscFreq >= 95000))   //WriteIIC (0x53, 0x7F);  // EN_AUTO_VCOVARCON = 0 ;
		{
			reg = 0x53;  
			value = 0x7F;
			mb86a35_i2c_rf_send(reg, value);
		}
		else //WriteIIC (0x53, 0xFF); // EN_AUTO_VCOVARCON = 1 ;
		{
			reg = 0x53;  
			value = 0xFF;
			mb86a35_i2c_rf_send(reg, value);
		}

		if ((ui32localOscFreq < 928000)&&(ui32localOscFreq >= 464000))  //928~464
		  pui8plls=1;
		else if ((ui32localOscFreq < 464000)&&(ui32localOscFreq>=232000)) //464~232
		  pui8plls=4;
		else if ((ui32localOscFreq < 232000)&&(ui32localOscFreq>=116000)) //232~116
          pui8plls=8;
		else if (ui32localOscFreq<116000) 
		 pui8plls=16;
	
		ui32pllFreq = ui32localOscFreq * pui8plls;
		//ui32pllFreq=LO2PLL_Freq(ui32localOscFreq, &ui8PLLS);
	    
	    
	    if ((ui32localOscFreq < 928000)&&(ui32localOscFreq >= 464000))  //928~464
			VCOBAND = 7;
			else if ((ui32localOscFreq < 464000)&&(ui32localOscFreq>=232000)) //464~232
				VCOBAND = 6;
				else if ((ui32localOscFreq < 232000)&&(ui32localOscFreq>=116000)) //232~116
					VCOBAND = 5;
					else if (ui32localOscFreq<116000) 
						VCOBAND = 4;
		//VCOBAND=GetVCOBAND(ui32localOscFreq);

		ui32RefereceFreq = 32000;
		ui32PLLN = ( ui32pllFreq / ui32RefereceFreq  );		

		tmp=ui32pllFreq-ui32PLLN* (ui32RefereceFreq);
		pll_mul=1;
		r_div=16;
		ui32PLLF = (  (tmp * 1024 * 64) / (ui32RefereceFreq/r_div)  * pll_mul);

		reg = 0x26;  
		value = ((VCOLOADBANDF_I2C<<5) + (VCOBAND<<2));
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x27;  
		value = (ui32PLLN&0xff);
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x28;  
		value = ((ui32PLLF&0x0FF000)>>12);
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x29;  
		value = ((ui32PLLF&0x0FF0)>>4);
		mb86a35_i2c_rf_send(reg, value);

		reg = 0x2a;  
		value = (0x01 + ((ui32PLLF&0x0F)<<4));
		mb86a35_i2c_rf_send(reg, value);

		mdelay(1);
		//Sleep(1); //delay 1ms

		reg = 0x06;
		rtncode = mb86a35_i2c_rf_recv(reg, &REG06, 1);
		if (rtncode != 0) {
    		rtncode = -EFAULT;
    		goto rf_channel_errreturn;
    	}
    	VCORG = REG06;
		//VCORG =  ReadIIC(0x06);


		
		if(ui8REVID == 1)   //For CS1 chip Setting.
		{		

			if(VCORG <= 0x1F) MAXVCOOUT = 0x09;
			else if(VCORG > 0x1F && VCORG <= 0x5F ) MAXVCOOUT = 0x08;	// 2011.6.17 not change
			else if(VCORG > 0x5F && VCORG <= 0x7F ) MAXVCOOUT = 0x08;
			else if(VCORG > 0x7F && VCORG <= 0xAF ) MAXVCOOUT = 0x06;
			else MAXVCOOUT = 0x05;

		}
		// Modified by prajuna 20121028 for MAXVCOOUT {
		/*
		else   //For CS2 chip Setting.
		{
			if(VCORG <= 0x1F) MAXVCOOUT = 0x05;
			else if(VCORG > 0x1F && VCORG <= 0x5F ) MAXVCOOUT = 0x05;   //Value Update due to VCORG margin
			else if(VCORG > 0x5F && VCORG <= 0x7F ) MAXVCOOUT = 0x04;
			else if(VCORG > 0x7F && VCORG <= 0xAF ) MAXVCOOUT = 0x04;
			else MAXVCOOUT = 0x02;
		}
		*/
		//
		else   //For CS2 chip Setting.
		{
			if(VCORG <= 0x1F)
			{
				MAXVCOOUT = 0x07;
			}
			else if(VCORG > 0x1F && VCORG <= 0x5F)
			{
				MAXVCOOUT = 0x06;
			}
			else if(VCORG > 0x5F && VCORG <= 0x7F)
			{
				MAXVCOOUT = 0x06;
			}
			else if(VCORG > 0x7F && VCORG <= 0xAF)
			{
				MAXVCOOUT = 0x04;
			}
			else
			{
				MAXVCOOUT = 0x04;
			}
		}
		//
		// Modified by prajuna 20121028 for MAXVCOOUT }

		reg = 0x51;  
		value = WR51 | (MAXVCOOUT << 3);
		mb86a35_i2c_rf_send(reg, value);
		//WriteIIC (0x51, WR51 | (MAXVCOOUT << 3)); //VCOLDO_OUTSEL_I2C

		//reg = 0x07; 
		//rtncode = mb86a35_i2c_rf_recv(reg, &REG07, 1);
		//if (rtncode != 0) {
    		//rtncode = -EFAULT;
    		//goto rf_channel_errreturn;
    	//}
    	//PLLLOCK = REG07 & 0x01;;
    	//PLLLOCK =  ReadIIC(0x07) & 0x01;

   }          //ES3.0 end

rf_channel_errreturn:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;

rf_channel_norreturn:
	reg = 0x86;
	value = 0xEF;
	mb86a35_i2c_rf_send(reg, value);

	rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	internal function. \n
	ioctl System Call.  IOCTL_RF_CALIB_CTUNE command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	RF		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_CALIB_CTUNE_do(mb86a35_cmdcontrol_t * cmdctrl,
				    unsigned int cmd, ioctl_rf_t * RF)
{
  int rtncode_rf = 0;
  unsigned char reg_rf = 0;
  u8 chipid0_rf = 0;
  int rtncode = 0;
   rtncode_rf = mb86a35_i2c_rf_recv(reg_rf, &chipid0_rf, 1);
   if (rtncode_rf != 0) {
    		rtncode = -EFAULT;
    		goto ch_ctune_do_return;
    }

   if (chipid0_rf == 0x03) {    //ES2.0 start

	//int rtncode = 0;
	unsigned char reg;
	unsigned int value = 0;
	int ctuneid = 0;
	int ctune = 0;
	int normal_cnt = 0x04BACA;
	int rcvar = 0;
	u8 REG08 = 0;
	u8 REG09 = 0;
	u8 REG24 = 0;
	u8 REG3D = 0;
	u8 REG3E = 0;
	u8 REG40 = 0;

	struct mb86a35_ctune {
		u8 BQC;
		u8 STG1_C3_GV0_DEF;
		u8 STG1_C3_GV6_DEF;
		u8 STG1_C3_GV12_DEF;
		u8 STG1_C3_GV18_DEF;
		u8 STG2_C3_GV0_DEF;
		u8 STG2_C3_GV6_DEF;
		u8 STG2_C3_GV12_DEF;
		u8 STG2_C3_GV18_DEF;
		u8 STG3_C3_GV0_DEF;
		u8 STG3_C3_GV6_DEF;
		u8 STG3_C3_GV12_DEF;
		u8 STG3_C3_GV18_DEF;
		u8 STG2_Q_CAL;
		u8 STG3_Q_CAL;
		u8 vBQC_OFFSET;
		u8 LPF_MODE;
	} mb86a35_ctune_regdata[3] = {
		{
		31, 1, 3, 6, 12, 12, 24, 48, 88, 10, 20, 39, 78, 4, 3, 3, 8},
		{
		105, 5, 10, 21, 42, 29, 60, 120, 240, 16, 32, 64, 128,
			    0, 0, 0, 2}, {
	26, 1, 3, 6, 12, 10, 20, 40, 80, 10, 20, 40, 80, 0, 0,
			    0, 2},};
	int vSTG1_C3_GV0 = 0;
	int vSTG1_C3_GV6 = 0;
	int vSTG1_C3_GV12 = 0;
	int vSTG1_C3_GV18 = 0;
	int vSTG2_C3_GV0 = 0;
	int vSTG2_C3_GV6 = 0;
	int vSTG2_C3_GV12 = 0;
	int vSTG2_C3_GV18 = 0;
	int vSTG3_C3_GV0 = 0;
	int vSTG3_C3_GV6 = 0;
	int vSTG3_C3_GV12 = 0;
	int vSTG3_C3_GV18 = 0;
	int vBQC = 0;
	int vBQC_ACR = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)RF);

	switch (RF->mode) {
		/* 13 seg */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBTMM_13VHF:
		ctuneid = 0;
		break;

		/* 1 seg */
	case PARAM_MODE_ISDBT_1UHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
		ctuneid = 1;
		break;

		/* 3 seg */
	case PARAM_MODE_ISDBTSB_3VHF:
		ctuneid = 2;
		break;

	default:
		rtncode = -EINVAL;
		goto ch_ctune_do_return;
		break;
	}

	reg = 0x30;
	value = REG30;
	mb86a35_i2c_rf_send(reg, value);

	/* WAIT */
	mdelay(1);
	/********/

	reg = 0x09;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG09, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ch_ctune_do_return;
	}

	reg = 0x08;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG08, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ch_ctune_do_return;
	}

	ctune = (REG09 & 0xff) + ((REG08 & 0x01) << 8);

	rcvar = normal_cnt / ctune;
	REG3D = 0x30 + ((8 + 18) * rcvar / 1000) - 18;
	reg = 0x3D;
	value = REG3D;
	mb86a35_i2c_rf_send(reg, value);

	REG24 = 0x80 + mb86a35_ctune_regdata[ctuneid].LPF_MODE;
	reg = 0x24;
	value = REG24;
	mb86a35_i2c_rf_send(reg, value);

	vSTG1_C3_GV0 =
	    (mb86a35_ctune_regdata[ctuneid].STG1_C3_GV0_DEF * rcvar) / 1000;
	if (vSTG1_C3_GV0 > 0x0F) {
		vSTG1_C3_GV0 = 0x0F;
	}
	reg = 0x3F;
	value = (0x40 + vSTG1_C3_GV0);
	mb86a35_i2c_rf_send(reg, value);

	vSTG1_C3_GV6 =
	    (mb86a35_ctune_regdata[ctuneid].STG1_C3_GV6_DEF * rcvar) / 1000;
	if (vSTG1_C3_GV6 > 0x1F) {
		vSTG1_C3_GV6 = 0x1F;
	}

	vSTG2_C3_GV12 =
	    (mb86a35_ctune_regdata[ctuneid].STG2_C3_GV12_DEF * rcvar) / 1000;
	if (vSTG2_C3_GV12 > 0x1FF) {
		vSTG2_C3_GV12 = 0x1FF;
	}

	vSTG2_C3_GV18 =
	    (mb86a35_ctune_regdata[ctuneid].STG2_C3_GV18_DEF * rcvar) / 1000;
	if (vSTG2_C3_GV18 > 0x1FF) {
		vSTG2_C3_GV18 = 0x1FF;
	}

	vSTG3_C3_GV18 =
	    (mb86a35_ctune_regdata[ctuneid].STG3_C3_GV18_DEF * rcvar) / 1000;
	if (vSTG3_C3_GV18 > 0x1FF) {
		vSTG3_C3_GV18 = 0x1FF;
	}

	REG40 = ((vSTG1_C3_GV6 & 0x1F) << 3)
	    + ((vSTG2_C3_GV12 & 0x01) << 2)
	    + ((vSTG2_C3_GV18 & 0x01) << 1)
	    + (vSTG3_C3_GV18 & 0x01);
	reg = 0x40;
	value = REG40;
	mb86a35_i2c_rf_send(reg, value);

	vSTG1_C3_GV12 =
	    (mb86a35_ctune_regdata[ctuneid].STG1_C3_GV12_DEF * rcvar) / 1000;
	if (vSTG1_C3_GV12 > 0x3F) {
		vSTG1_C3_GV12 = 0x3F;
	}
	reg = 0x41;
	value = vSTG1_C3_GV12;
	mb86a35_i2c_rf_send(reg, value);

	vSTG1_C3_GV18 =
	    (mb86a35_ctune_regdata[ctuneid].STG1_C3_GV18_DEF * rcvar) / 1000;
	if (vSTG1_C3_GV18 > 0x7F) {
		vSTG1_C3_GV18 = 0x7F;
	}
	reg = 0x42;
	value = vSTG1_C3_GV18;
	mb86a35_i2c_rf_send(reg, value);

	vSTG2_C3_GV0 =
	    (mb86a35_ctune_regdata[ctuneid].STG2_C3_GV0_DEF * rcvar) / 1000;
	if (vSTG2_C3_GV0 > 0x7F) {
		vSTG2_C3_GV0 = 0x7F;
	}
	reg = 0x43;
	value = vSTG2_C3_GV0;
	mb86a35_i2c_rf_send(reg, value);

	vSTG2_C3_GV6 =
	    (mb86a35_ctune_regdata[ctuneid].STG2_C3_GV6_DEF * rcvar) / 1000;
	if (vSTG2_C3_GV6 > 0xFF) {
		vSTG2_C3_GV6 = 0xFF;
	}
	reg = 0x44;
	value = vSTG2_C3_GV6;
	mb86a35_i2c_rf_send(reg, value);

	vSTG3_C3_GV0 =
	    (mb86a35_ctune_regdata[ctuneid].STG3_C3_GV0_DEF * rcvar) / 1000;
	if (vSTG3_C3_GV0 > 0x3F) {
		vSTG3_C3_GV0 = 0x3F;
	}
	reg = 0x45;
	value = vSTG3_C3_GV0;
	mb86a35_i2c_rf_send(reg, value);

	vSTG3_C3_GV6 =
	    (mb86a35_ctune_regdata[ctuneid].STG3_C3_GV6_DEF * rcvar) / 1000;
	if (vSTG3_C3_GV6 > 0x7F) {
		vSTG3_C3_GV6 = 0x7F;
	}
	reg = 0x46;
	value = vSTG3_C3_GV6;
	mb86a35_i2c_rf_send(reg, value);

	vSTG3_C3_GV12 =
	    (mb86a35_ctune_regdata[ctuneid].STG3_C3_GV12_DEF * rcvar) / 1000;
	if (vSTG3_C3_GV12 > 0xFF) {
		vSTG3_C3_GV12 = 0xFF;
	}
	reg = 0x47;
	value = vSTG3_C3_GV12;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x48;
	value = (vSTG2_C3_GV12 >> 1) & 0xFF;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x49;
	value = (vSTG2_C3_GV18 >> 1) & 0xFF;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x4A;
	value = (vSTG3_C3_GV18 >> 1) & 0xFF;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x3E;
	REG3E = (mb86a35_ctune_regdata[ctuneid].STG2_Q_CAL << 3)
	    + (mb86a35_ctune_regdata[ctuneid].STG3_Q_CAL);
	value = REG3E;
	mb86a35_i2c_rf_send(reg, value);

	vBQC = ((mb86a35_ctune_regdata[ctuneid].BQC + 18) * rcvar) / 1000 - 18;
	if (vBQC > 0xFF) {
		vBQC = 0xFF;
	}
	reg = 0x4B;
	value = vBQC;
	mb86a35_i2c_rf_send(reg, value);

	vBQC_ACR = vBQC + mb86a35_ctune_regdata[ctuneid].vBQC_OFFSET;
	if (vBQC_ACR > 0xFF) {
		vBQC_ACR = 0xFF;
	}
	reg = 0x91;
	value = vBQC_ACR;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x30;
	value = (REG30 | 0x08);
	mb86a35_i2c_rf_send(reg, value);

   }          //ES2.0 end
   else {     //ES3.0 start

	//int rtncode = 0;
	unsigned char reg;
	unsigned int value = 0;

	u8 ctune = 0;
	u8 REG12 = 0;
	signed char twos_ctune = 0;
	int rcvar = 0;

    //1seg init
	//u32 CBT_ISDBT1SEG_RSB_3_I2C = 3;
	//u32 CBT_ISDBT1SEG_RSB_5_I2C = 3;
	u32 CBT_ISDBT1SEG_CBI_3_I2C = 103;
	u32 CBT_ISDBT1SEG_CBII_3_I2C = 98;
	u32 CBT_ISDBT1SEG_CBIII_3_I2C = 98;
	u32 CBT_ISDBT1SEG_CBI_5_I2C = 170;
	u32 CBT_ISDBT1SEG_CBII_5_I2C = 64;
	u32 CBT_ISDBT1SEG_CBIII_5_I2C = 196;
	u32 CBT_ISDBT1SEG_CBIV_5_I2C = 75;
	u32 CBT_ISDBT1SEG_CBV_5_I2C = 154;
	u32 CBT_ISDBT1SEG_CB3M_5_I2C = 267;
	u32 CBT_ISDBT1SEG_CB3D_5_I2C = 66;
	u32 CBT_ISDBT1SEG_CB5M_5_I2C = 90;
	u32 CBT_ISDBT1SEG_CB5D_5_I2C = 22;

    //13seg init
	//u32 CBT_ISDBTFULL_RSB_3_I2C = 2;
	//u32 CBT_ISDBTFULL_RSB_5_I2C = 2;
	u32 CBT_ISDBTFULL_CBI_3_I2C = 65;
	u32 CBT_ISDBTFULL_CBII_3_I2C = 65;
	u32 CBT_ISDBTFULL_CBIII_3_I2C = 65;
	u32 CBT_ISDBTFULL_CBI_5_I2C = 120;
	u32 CBT_ISDBTFULL_CBII_5_I2C = 62;
	u32 CBT_ISDBTFULL_CBIII_5_I2C = 120;
	u32 CBT_ISDBTFULL_CBIV_5_I2C = 60;
	u32 CBT_ISDBTFULL_CBV_5_I2C = 117;
	u32 CBT_ISDBTFULL_CB3M_5_I2C = 109;
	u32 CBT_ISDBTFULL_CB3D_5_I2C = 26;
	u32 CBT_ISDBTFULL_CB5M_5_I2C = 39;
	u32 CBT_ISDBTFULL_CB5D_5_I2C = 9;

    //3seg init
	//u32 CBT_ISDBT3SEG_RSB_3_I2C = 3;
	//u32 CBT_ISDBT3SEG_RSB_5_I2C = 3;
	u32 CBT_ISDBT3SEG_CBI_3_I2C = 65;
	u32 CBT_ISDBT3SEG_CBII_3_I2C = 65;
	u32 CBT_ISDBT3SEG_CBIII_3_I2C = 65;
	u32 CBT_ISDBT3SEG_CBI_5_I2C = 124;
	u32 CBT_ISDBT3SEG_CBII_5_I2C = 47;
	u32 CBT_ISDBT3SEG_CBIII_5_I2C = 126;
	u32 CBT_ISDBT3SEG_CBIV_5_I2C = 51;
	u32 CBT_ISDBT3SEG_CBV_5_I2C = 113;
	u32 CBT_ISDBT3SEG_CB3M_5_I2C = 195;
	u32 CBT_ISDBT3SEG_CB3D_5_I2C = 48;
	u32 CBT_ISDBT3SEG_CB5M_5_I2C = 66;
	u32 CBT_ISDBT3SEG_CB5D_5_I2C = 16;

    //1seg tune
	u32 CBT_TUNE_ISDBT1SEG_RSB_3_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_RSB_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBI_3_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBII_3_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBIII_3_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBI_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBII_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBIII_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBIV_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CBV_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CB3M_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CB3D_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CB5M_5_I2C = 0;
	u32 CBT_TUNE_ISDBT1SEG_CB5D_5_I2C = 0;

    //13seg tune
	u32 CBT_TUNE_ISDBTFULL_RSB_3_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_RSB_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBI_3_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBII_3_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBIII_3_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBI_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBII_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBIII_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBIV_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CBV_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CB3M_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CB3D_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CB5M_5_I2C = 0;
	u32 CBT_TUNE_ISDBTFULL_CB5D_5_I2C = 0;

    //3seg tune
	u32 CBT_TUNE_ISDBT3SEG_RSB_3_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_RSB_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBI_3_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBII_3_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBIII_3_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBI_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBII_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBIII_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBIV_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CBV_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CB3M_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CB3D_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CB5M_5_I2C = 0;
	u32 CBT_TUNE_ISDBT3SEG_CB5D_5_I2C = 0;

    //write
	u32 vRSB_3_I2C;
	u32 vRSB_5_I2C;
	u32 vCBI_3_I2C;
	u32 vCBII_3_I2C;
	u32 vCBIII_3_I2C;
	u32 vCBI_5_I2C;
	u32 vCBII_5_I2C;
	u32 vCBIII_5_I2C;
	u32 vCBIV_5_I2C;
	u32 vCBV_5_I2C;
	u32 vCB3M_5_I2C;
	u32 vCB3D_5_I2C;
	u32 vCB5M_5_I2C;
	u32 vCB5D_5_I2C;

	u32 REG3D;
	u32 REG3F;
	u32 REG40;
	u32 REG41;
	u32 REG42;
	u32 REG43;
	u32 REG44;
	u32 REG45;
	u32 REG46;
	u32 REG47;
	u32 REG48;
	u32 REG49;
	u32 REG4A;

	reg = 0x12;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG12, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ch_ctune_do_return;
	}

	ctune = REG12 & 0x1F;

    if(ctune & 0x10)
	{
		twos_ctune = ctune & 0x0F; 
		twos_ctune = (~twos_ctune) + 1;
	}
	else
	{
       twos_ctune = ctune;
	}

	switch (RF->mode) {
		/* 13 seg */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBTMM_13VHF:
		
		CBT_TUNE_ISDBTFULL_RSB_3_I2C = 2;
		CBT_TUNE_ISDBTFULL_RSB_5_I2C = 2;
		
		rcvar = (90 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBI_3_I2C = (int)(CBT_ISDBTFULL_CBI_3_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBI_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBTFULL_CBI_3_I2C = (1 << 7)-1;
	    }
		
		rcvar = (77 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBII_3_I2C = (int)(CBT_ISDBTFULL_CBII_3_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBII_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBTFULL_CBII_3_I2C = (1 << 7)-1;
	    }
		
		rcvar = (77 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBIII_3_I2C = (int)(CBT_ISDBTFULL_CBIII_3_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBIII_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBTFULL_CBIII_3_I2C = (1 << 7)-1;
		}
		
		rcvar = (78 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBI_5_I2C = (int)(CBT_ISDBTFULL_CBI_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBI_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBTFULL_CBI_5_I2C = (1 << 8)-1;
		}
		
		rcvar = (40 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBII_5_I2C = (int)(CBT_ISDBTFULL_CBII_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBII_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBTFULL_CBII_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (118 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBIII_5_I2C = (int)(CBT_ISDBTFULL_CBIII_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBIII_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBTFULL_CBIII_5_I2C = (1 << 8)-1;
		}
		
		rcvar = (48 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBIV_5_I2C = (int)(CBT_ISDBTFULL_CBIV_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBIV_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBTFULL_CBIV_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (75 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CBV_5_I2C = (int)(CBT_ISDBTFULL_CBV_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CBV_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBTFULL_CBV_5_I2C = (1 << 8)-1;
		}
		
		rcvar = (70 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CB3M_5_I2C = (int)(CBT_ISDBTFULL_CB3M_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CB3M_5_I2C > ((1 << 9)-1)) {
			CBT_TUNE_ISDBTFULL_CB3M_5_I2C = (1 << 9)-1;
		}
		
		rcvar = (53 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CB3D_5_I2C = (int)(CBT_ISDBTFULL_CB3D_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CB3D_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBTFULL_CB3D_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (25 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CB5M_5_I2C = (int)(CBT_ISDBTFULL_CB5M_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CB5M_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBTFULL_CB5M_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (6 * twos_ctune) >> 5;
		CBT_TUNE_ISDBTFULL_CB5D_5_I2C = (int)(CBT_ISDBTFULL_CB5D_5_I2C+rcvar);
		if (CBT_TUNE_ISDBTFULL_CB5D_5_I2C > ((1 << 5)-1)) {
			CBT_TUNE_ISDBTFULL_CB5D_5_I2C = (1 << 5)-1;
		}
		
		vRSB_3_I2C = CBT_TUNE_ISDBTFULL_RSB_3_I2C;
		vRSB_5_I2C = CBT_TUNE_ISDBTFULL_RSB_5_I2C;
		vCBI_3_I2C = CBT_TUNE_ISDBTFULL_CBI_3_I2C;
		vCBII_3_I2C = CBT_TUNE_ISDBTFULL_CBII_3_I2C;
		vCBIII_3_I2C = CBT_TUNE_ISDBTFULL_CBIII_3_I2C;
		vCBI_5_I2C = CBT_TUNE_ISDBTFULL_CBI_5_I2C;
		vCBII_5_I2C = CBT_TUNE_ISDBTFULL_CBII_5_I2C;
		vCBIII_5_I2C = CBT_TUNE_ISDBTFULL_CBIII_5_I2C;
		vCBIV_5_I2C = CBT_TUNE_ISDBTFULL_CBIV_5_I2C;
		vCBV_5_I2C = CBT_TUNE_ISDBTFULL_CBV_5_I2C;
		vCB3M_5_I2C = CBT_TUNE_ISDBTFULL_CB3M_5_I2C;
		vCB3D_5_I2C = CBT_TUNE_ISDBTFULL_CB3D_5_I2C;
		vCB5M_5_I2C = CBT_TUNE_ISDBTFULL_CB5M_5_I2C;
		vCB5D_5_I2C = CBT_TUNE_ISDBTFULL_CB5D_5_I2C;
		
		break;

		/* 1 seg */
	case PARAM_MODE_ISDBT_1UHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
		
		CBT_TUNE_ISDBT1SEG_RSB_3_I2C = 3;
		CBT_TUNE_ISDBT1SEG_RSB_5_I2C = 3;
		
		rcvar = (115 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBI_3_I2C = (int)(CBT_ISDBT1SEG_CBI_3_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBI_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT1SEG_CBI_3_I2C = (1 << 7)-1;
		}
		
		rcvar = (98 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBII_3_I2C = (int)(CBT_ISDBT1SEG_CBII_3_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBII_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT1SEG_CBII_3_I2C = (1 << 7)-1;
		}
		
		rcvar = (98 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBIII_3_I2C = (int)(CBT_ISDBT1SEG_CBIII_3_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBIII_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT1SEG_CBIII_3_I2C = (1 << 7)-1;
		}
		
		rcvar = (109 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBI_5_I2C = (int)(CBT_ISDBT1SEG_CBI_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBI_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBT1SEG_CBI_5_I2C = (1 << 8)-1;
		}
		
		rcvar = (42 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBII_5_I2C = (int)(CBT_ISDBT1SEG_CBII_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBII_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT1SEG_CBII_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (166 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBIII_5_I2C = (int)(CBT_ISDBT1SEG_CBIII_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBIII_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBT1SEG_CBIII_5_I2C = (1 << 8)-1;
		}
		
		rcvar = (58 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBIV_5_I2C = (int)(CBT_ISDBT1SEG_CBIV_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBIV_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT1SEG_CBIV_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (99 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CBV_5_I2C = (int)(CBT_ISDBT1SEG_CBV_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CBV_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBT1SEG_CBV_5_I2C = (1 << 8)-1;
		}
		
		rcvar = (171 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CB3M_5_I2C = (int)(CBT_ISDBT1SEG_CB3M_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CB3M_5_I2C > ((1 << 9)-1)) {
			CBT_TUNE_ISDBT1SEG_CB3M_5_I2C = (1 << 9)-1;
		}
		
		rcvar = (43 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CB3D_5_I2C = (int)(CBT_ISDBT1SEG_CB3D_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CB3D_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT1SEG_CB3D_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (58 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CB5M_5_I2C = (int)(CBT_ISDBT1SEG_CB5M_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CB5M_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT1SEG_CB5M_5_I2C = (1 << 7)-1;
		}
		
		rcvar = (15 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT1SEG_CB5D_5_I2C = (int)(CBT_ISDBT1SEG_CB5D_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT1SEG_CB5D_5_I2C > ((1 << 5)-1)) {
			CBT_TUNE_ISDBT1SEG_CB5D_5_I2C = (1 << 5)-1;
		}
		
		vRSB_3_I2C = CBT_TUNE_ISDBT1SEG_RSB_3_I2C;
		vRSB_5_I2C = CBT_TUNE_ISDBT1SEG_RSB_5_I2C;
		vCBI_3_I2C = CBT_TUNE_ISDBT1SEG_CBI_3_I2C;
		vCBII_3_I2C = CBT_TUNE_ISDBT1SEG_CBII_3_I2C;
		vCBIII_3_I2C = CBT_TUNE_ISDBT1SEG_CBIII_3_I2C;
		vCBI_5_I2C = CBT_TUNE_ISDBT1SEG_CBI_5_I2C;
		vCBII_5_I2C = CBT_TUNE_ISDBT1SEG_CBII_5_I2C;
		vCBIII_5_I2C = CBT_TUNE_ISDBT1SEG_CBIII_5_I2C;
		vCBIV_5_I2C = CBT_TUNE_ISDBT1SEG_CBIV_5_I2C;
		vCBV_5_I2C = CBT_TUNE_ISDBT1SEG_CBV_5_I2C;
		vCB3M_5_I2C = CBT_TUNE_ISDBT1SEG_CB3M_5_I2C;
		vCB3D_5_I2C = CBT_TUNE_ISDBT1SEG_CB3D_5_I2C;
		vCB5M_5_I2C = CBT_TUNE_ISDBT1SEG_CB5M_5_I2C;
		vCB5D_5_I2C = CBT_TUNE_ISDBT1SEG_CB5D_5_I2C;
		
		break;

		/* 3 seg */
	case PARAM_MODE_ISDBTSB_3VHF:
		
		CBT_TUNE_ISDBT3SEG_RSB_3_I2C = 3;
		CBT_TUNE_ISDBT3SEG_RSB_5_I2C = 3;
		
		rcvar = (90 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBI_3_I2C = (int)(CBT_ISDBT3SEG_CBI_3_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBI_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT3SEG_CBI_3_I2C = ((1 << 7)-1);
		}
		
		rcvar = (77 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBII_3_I2C = (int)(CBT_ISDBT3SEG_CBII_3_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBII_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT3SEG_CBII_3_I2C = ((1 << 7)-1);
		}
		
		rcvar = (77 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBIII_3_I2C = (int)(CBT_ISDBT3SEG_CBIII_3_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBIII_3_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT3SEG_CBIII_3_I2C = ((1 << 7)-1);
		}
		
		rcvar = (80 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBI_5_I2C = (int)(CBT_ISDBT3SEG_CBI_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBI_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBT3SEG_CBI_5_I2C = ((1 << 8)-1);
		}
		
		rcvar = (30 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBII_5_I2C = (int)(CBT_ISDBT3SEG_CBII_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBII_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT3SEG_CBII_5_I2C = ((1 << 7)-1);
		}
		
		rcvar = (122 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBIII_5_I2C = (int)(CBT_ISDBT3SEG_CBIII_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBIII_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBT3SEG_CBIII_5_I2C = ((1 << 8)-1);
		}
		
		rcvar = (43 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBIV_5_I2C = (int)(CBT_ISDBT3SEG_CBIV_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBIV_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT3SEG_CBIV_5_I2C = ((1 << 7)-1);
		}
		
		rcvar = (73 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CBV_5_I2C = (int)(CBT_ISDBT3SEG_CBV_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CBV_5_I2C > ((1 << 8)-1)) {
			CBT_TUNE_ISDBT3SEG_CBV_5_I2C = ((1 << 8)-1);
		}
		
		rcvar = (126 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CB3M_5_I2C = (int)(CBT_ISDBT3SEG_CB3M_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CB3M_5_I2C > ((1 << 9)-1)) {
			CBT_TUNE_ISDBT3SEG_CB3M_5_I2C = ((1 << 9)-1);
		}
		
		rcvar = (31 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CB3D_5_I2C = (int)(CBT_ISDBT3SEG_CB3D_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CB3D_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT3SEG_CB3D_5_I2C = ((1 << 7)-1);
		}
		
		rcvar = (43 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CB5M_5_I2C = (int)(CBT_ISDBT3SEG_CB5M_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CB5M_5_I2C > ((1 << 7)-1)) {
			CBT_TUNE_ISDBT3SEG_CB5M_5_I2C = ((1 << 7)-1);
		}
		
		rcvar = (11 * twos_ctune) >> 5;
		CBT_TUNE_ISDBT3SEG_CB5D_5_I2C = (int)(CBT_ISDBT3SEG_CB5D_5_I2C+rcvar);
		if (CBT_TUNE_ISDBT3SEG_CB5D_5_I2C > ((1 << 5)-1)) {
			CBT_TUNE_ISDBT3SEG_CB5D_5_I2C = ((1 << 5)-1);
		}
		
		vRSB_3_I2C = CBT_TUNE_ISDBT3SEG_RSB_3_I2C;
		vRSB_5_I2C = CBT_TUNE_ISDBT3SEG_RSB_5_I2C;
		vCBI_3_I2C = CBT_TUNE_ISDBT3SEG_CBI_3_I2C;
		vCBII_3_I2C = CBT_TUNE_ISDBT3SEG_CBII_3_I2C;
		vCBIII_3_I2C = CBT_TUNE_ISDBT3SEG_CBIII_3_I2C;
		vCBI_5_I2C = CBT_TUNE_ISDBT3SEG_CBI_5_I2C;
		vCBII_5_I2C = CBT_TUNE_ISDBT3SEG_CBII_5_I2C;
		vCBIII_5_I2C = CBT_TUNE_ISDBT3SEG_CBIII_5_I2C;
		vCBIV_5_I2C = CBT_TUNE_ISDBT3SEG_CBIV_5_I2C;
		vCBV_5_I2C = CBT_TUNE_ISDBT3SEG_CBV_5_I2C;
		vCB3M_5_I2C = CBT_TUNE_ISDBT3SEG_CB3M_5_I2C;
		vCB3D_5_I2C = CBT_TUNE_ISDBT3SEG_CB3D_5_I2C;
		vCB5M_5_I2C = CBT_TUNE_ISDBT3SEG_CB5M_5_I2C;
		vCB5D_5_I2C = CBT_TUNE_ISDBT3SEG_CB5D_5_I2C;
		
		break;

	default:
		rtncode = -EINVAL;
		goto ch_ctune_do_return;
		break;
	}

	REG3D = (0x30 | (vRSB_3_I2C << 2 )) + vRSB_5_I2C;
	REG3F = vCBI_3_I2C;
	REG40 = vCBII_3_I2C;
	REG41 = vCBIII_3_I2C;
	REG42 = vCBI_5_I2C;
	REG43 = vCBII_5_I2C;
	REG44 = vCBIII_5_I2C;
	REG45 = vCBIV_5_I2C;
	REG46 = vCBV_5_I2C;
	REG47 = (((vCB3M_5_I2C & 0x100) >> 8)<<7) + vCB3D_5_I2C;
	REG48 = (vCB3M_5_I2C) & 0xff;
	REG49 = (vCB5D_5_I2C) & 0x1f;
	REG4A = vCB5M_5_I2C;
	
	reg = 0x3d;
	value = REG3D;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x3f;
	value = REG3F;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x40;
	value = REG40;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x41;
	value = REG41;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x42;
	value = REG42;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x43;
	value = REG43;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x44;
	value = REG44;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x45;
	value = REG45;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x46;
	value = REG46;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x47;
	value = REG47;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x48;
	value = REG48;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x49;
	value = REG49;
	mb86a35_i2c_rf_send(reg, value);
	
	reg = 0x4A;
	value = REG4A;
	mb86a35_i2c_rf_send(reg, value);

  } //ES3.0 end

ch_ctune_do_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RST_SOFT command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RST_SOFT(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	ioctl_reset_t *RESET_user = (ioctl_reset_t *) arg;
	ioctl_reset_t *RESET = &cmdctrl->RESET;
	size_t tmpsize = sizeof(ioctl_reset_t);
	int rtncode = 0;
	unsigned int reg;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (RESET_user == NULL) {
		rtncode = -EINVAL;
		goto reset_return;
	}

	memset(RESET, 0, tmpsize);
	if (copy_from_user(RESET, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto reset_return;
	}

	reg = (int)MB86A35_REG_ADDR_RST;
	if (RESET->RESET == PARAM_RESET_ON) {
		value = (int)0;
	} else if (RESET->RESET == PARAM_RESET_OFF) {
		value =
		    (int)(MB86A35_MASK_RST_I2CREG_RESET |
			  MB86A35_MASK_RST_LOGIC_RESET);
	} else {
		rtncode = -EINVAL;
		goto reset_return;
	}
	mb86a35_i2c_master_send(reg, value);

reset_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RST_SYNC command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RST_SYNC(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	ioctl_reset_t *RESET_user = (ioctl_reset_t *) arg;
	ioctl_reset_t *RESET = &cmdctrl->RESET;
	size_t tmpsize = sizeof(ioctl_reset_t);
	int rtncode = 0;
	unsigned int reg;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (RESET_user == NULL) {
		rtncode = -EINVAL;
		goto sync_return;
	}

	memset(RESET, 0, tmpsize);
	if (copy_from_user(RESET, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto sync_return;
	}

	if ((RESET->STATE_INIT != PARAM_STATE_INIT_ON)
	    && (RESET->STATE_INIT != PARAM_STATE_INIT_OFF)) {
		rtncode = -EINVAL;
		goto sync_return;
	}

	reg = MB86A35_REG_ADDR_STATE_INIT;
	value = RESET->STATE_INIT;
	mb86a35_i2c_master_send(reg, value);

sync_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SET_RECVM command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SET_RECVM(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_init_t *INIT_user = (ioctl_init_t *) arg;
	ioctl_init_t *INIT = &cmdctrl->INIT;
	size_t tmpsize = sizeof(ioctl_init_t);
	int rtncode = 0;
	unsigned int reg;
	unsigned int value;
#define	RECV_PATTERN1	( PARAM_SEG_BAND_DVBT7M | PARAM_SEG_BAND_DVBT8M | PARAM_SEG_BAND_DVBT6M		\
			| PARAM_SEG_BAND_DVBT5M | PARAM_SEG_BAND_ISDB6M | PARAM_SEG_BAND_ISDB7M		\
			| PARAM_SEG_BAND_ISDB8M								\
			| PARAM_SEG_TMM_32 | PARAM_SEG_TMM_16						\
			| PARAM_SEG_MYT_ISDB13S | PARAM_SEG_MYT_DVBT | PARAM_SEG_MYT_ISDB1S | PARAM_SEG_MYT_ISDB3S )
#define	RECV_PATTERN2	( PARAM_LAYERSEL_A | PARAM_LAYERSEL_B | PARAM_LAYERSEL_C )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (INIT_user == NULL) {
		rtncode = -EINVAL;
		goto recvm_return;
	}

	memset(INIT, 0, tmpsize);
	if (copy_from_user(INIT, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto recvm_return;
	}

	if ((INIT->SEGMENT & ~RECV_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto recvm_return;
	}

	if ((INIT->LAYERSEL & ~RECV_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto recvm_return;
	}

	reg = (int)MB86A35_REG_ADDR_SEGMENT;
	value = INIT->SEGMENT;
	mb86a35_i2c_master_send(reg, value);

	reg = (int)MB86A35_REG_ADDR_SPFFT_LAYERSEL;
	value = INIT->LAYERSEL;
	mb86a35_i2c_master_send(reg, value);

recvm_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SET_SPECT command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SET_SPECT(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_init_t *INIT_user = (ioctl_init_t *) arg;
	ioctl_init_t *INIT = &cmdctrl->INIT;
	size_t tmpsize = sizeof(ioctl_init_t);
	int rtncode = 0;
	unsigned int reg;
	unsigned int value;
#define	SET_SPECT_PATTERN1	( PARAM_IQINV_IQINV_NORM | PARAM_IQINV_IQINV_INVT )

	printk("mb86a35_IOCTL_SET_SPECT In\n");

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (INIT_user == NULL) {
		rtncode = -EINVAL;
		goto spect_return;
	}

	memset(INIT, 0, tmpsize);
	if (copy_from_user(INIT, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto spect_return;
	}

	if ((INIT->IQINV & ~SET_SPECT_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto spect_return;
	}

	reg = MB86A35_REG_ADDR_IQINV;
	value = INIT->IQINV;
	rtncode =
	    mb86a35_i2c_master_send_mask(reg, value, MB86A35_I2CMASK_IQINV,
					 MB86A35_MASK_IQINV);
	if (rtncode != 0) {
		goto spect_return;
	}

spect_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	printk("mb86a35_IOCTL_SET_SPECT Out\n");
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SET_IQSET command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SET_IQSET(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_init_t *INIT_user = (ioctl_init_t *) arg;
	ioctl_init_t *INIT = &cmdctrl->INIT;
	size_t tmpsize = sizeof(ioctl_init_t);
	int rtncode = 0;
	unsigned int reg;
	unsigned int value;
#define	SET_IQSET_PATTERN1	( PARAM_CONT_CTRL3_IQSEL_IF | PARAM_CONT_CTRL3_IQSEL_IQ		\
				| PARAM_CONT_CTRL3_IFSEL_16 | PARAM_CONT_CTRL3_IFSEL_32 )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (INIT_user == NULL) {
		rtncode = -EINVAL;
		goto iqset_return;
	}

	memset(INIT, 0, tmpsize);
	if (copy_from_user(INIT, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto iqset_return;
	}

	if ((INIT->CONT_CTRL3 & ~SET_IQSET_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto iqset_return;
	}

	reg = MB86A35_REG_ADDR_CONT_CTRL3;
	value = INIT->CONT_CTRL3;
	rtncode =
	    mb86a35_i2c_master_send_mask(reg, value, MB86A35_I2CMASK_CONT_CTRL3,
					 MB86A35_MASK_CONT_CTRL3);
	if (rtncode != 0) {
		goto iqset_return;
	}

iqset_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SET_ALOG_PDOWN command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SET_ALOG_PDOWN(mb86a35_cmdcontrol_t * cmdctrl,
				 unsigned int cmd, unsigned long arg)
{
	ioctl_init_t *INIT_user = (ioctl_init_t *) arg;
	ioctl_init_t *INIT = &cmdctrl->INIT;
	size_t tmpsize = sizeof(ioctl_init_t);
	int rtncode = 0;
	unsigned int reg;
	unsigned int value;
#define	SET_PDOWN_PATTERN1	( PARAM_MACRO_PDOWN_DACPWDN_OFF | PARAM_MACRO_PDOWN_DACPWDN_ON )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (INIT_user == NULL) {
		rtncode = -EINVAL;
		goto pdown_return;
	}

	memset(INIT, 0, tmpsize);
	if (copy_from_user(INIT, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto pdown_return;
	}

	if ((INIT->MACRO_PDOWN & ~SET_PDOWN_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto pdown_return;
	}

	reg = (int)MB86A35_REG_ADDR_MACRO_PWDN;
	value = INIT->MACRO_PDOWN & MB86A35_MASK_MACRO_PWDN_DACPWDN;
	mb86a35_i2c_master_send(reg, value);

pdown_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SET_ALOG_PDOWN command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SET_SCHANNEL(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			       unsigned long arg)
{
	ioctl_init_t *INIT_user = (ioctl_init_t *) arg;
	ioctl_init_t *INIT = &cmdctrl->INIT;
	size_t tmpsize = sizeof(ioctl_init_t);
	int rtncode = 0;
	unsigned int reg;
	unsigned int value;

	printk("mb86a35_IOCTL_SET_SCHANNEL In\n");

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (INIT_user == NULL) {
		rtncode = -EINVAL;
		goto schannel_return;
	}

	memset(INIT, 0, tmpsize);
	if (copy_from_user(INIT, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto schannel_return;
	}

	if (INIT->FTSEGCNT == PARAM_FTSEGCNT_SEGCNT_ISDB_T) {
		value = 0;
	} else {
		if (INIT->FTSEGCNT <= 41) {
			value = PARAM_FTSEGCNT_SEGCNT_ISDB_Tmm(INIT->FTSEGCNT);
		} else {
			rtncode = -EINVAL;
			goto schannel_return;
		}
	}

	reg = (int)MB86A35_REG_ADDR_FTSEGSFT;
	mb86a35_i2c_master_send(reg, value);

schannel_return:
	printk("mb86a35_IOCTL_SET_SCHANNEL Out\n");
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_AGC command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_AGC(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
		      unsigned long arg)
{
  int rtncode_rf = 0;
  unsigned char reg_rf = 0;
  u8 chipid0_rf = 0;
  int rtncode = 0;
   rtncode_rf = mb86a35_i2c_rf_recv(reg_rf, &chipid0_rf, 1);
   if (rtncode_rf != 0) {
    		rtncode = -EFAULT;
    		goto agc_return;
    }

   if (chipid0_rf == 0x03) {    //ES2.0 start

	ioctl_agc_t *AGC_user = (ioctl_agc_t *) arg;
	ioctl_agc_t *AGC = &cmdctrl->AGC;
	size_t tmpsize = sizeof(ioctl_agc_t);
	u8 DTSL;
	u8 AGAIN;
	u8 VIFREFL;
	u8 IFSAMPLE;
	u8 OUTSAMPLE;
	u8 LEVELTH = 0x00;
	//int rtncode = 0;
	unsigned char reg;
	unsigned char rega = MB86A35_REG_ADDR_SUBADR;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_SUBDAT;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (AGC_user == NULL) {
		rtncode = -EINVAL;
		goto agc_return;
	}

	memset(AGC, 0, tmpsize);
	if (copy_from_user(AGC, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto agc_return;
	}

	switch (AGC->mode) {
	case PARAM_MODE_ISDBT_1UHF:	/* ISDB-T    1seg / UHF */
	case PARAM_MODE_ISDBTMM_1VHF:	/* ISDB-Tmm  1seg / VHF */
	case PARAM_MODE_ISDBTSB_1VHF:	/* ISDB-Tsb  1seg / VHF */
		DTSL = 0x0A;
		AGAIN = 0x78;
		VIFREFL = 0x0A;
		IFSAMPLE = 0x20;
		OUTSAMPLE = 0x20;
		break;
	case PARAM_MODE_ISDBTSB_3VHF:	/* ISDB-Tsb  3seg / VHF */
	case PARAM_MODE_ISDBT_13UHF:	/* ISDB-T   13seg / UHF */
	case PARAM_MODE_ISDBTMM_13VHF:	/* ISDB-Tmm 13seg / VHF */
		DTSL = 0x0F;
		AGAIN = 0x78;
		VIFREFL = 0x12;
		IFSAMPLE = 0x01;
		OUTSAMPLE = 0x01;
		break;
	default:
		rtncode = -EFAULT;
		goto agc_return;
	};

	sreg = MB86A35_REG_SUBR_IFAH;
	value = (AGC->IFA >> 8) & 0x0F;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFAL;
	value = (AGC->IFA) & 0xFF;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFBH;
	value = (AGC->IFB >> 8) & 0x0F;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFBL;
	value = (AGC->IFB) & 0xFF;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_DTS;
	value = DTSL;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFAGCO;
	value = (AGC->IFAGCO);
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_MAXIFAGC;
	value = (AGC->MAXIFAGC);
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_AGAIN;
	value = AGAIN;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREFH;
	value = 0;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREFL;
	value = VIFREFL;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREF2H;
	value = (AGC->VIFREF2 >> 8) & 0x0F;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREF2L;
	value = (AGC->VIFREF2) & 0xFF;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFSAMPLE;
	value = IFSAMPLE;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_OUTSAMPLE;
	value = OUTSAMPLE;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_LEVELTH;
	value = LEVELTH;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	reg = MB86A35_REG_ADDR_IFAGC;
	rtncode = mb86a35_i2c_master_recv(reg, &AGC->IFAGC, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}

	sreg = MB86A35_REG_SUBR_IFAGCDAC;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &AGC->IFAGCDAC, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}

	rtncode = put_user(AGC->IFAGC, (char *)&AGC_user->IFAGC);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}
	rtncode = put_user(AGC->IFAGCDAC, (char *)&AGC_user->IFAGCDAC);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}

   }          //ES2.0 end
   else {     //ES3.0 start


	ioctl_agc_t *AGC_user = (ioctl_agc_t *) arg;
	ioctl_agc_t *AGC = &cmdctrl->AGC;
	size_t tmpsize = sizeof(ioctl_agc_t);
	u8 DTSL;
	u8 AGAIN;
	u8 VIFREFL;
	u8 IFSAMPLE;
	u8 OUTSAMPLE;
	u8 LEVELTH = 0x00;
	//int rtncode = 0;
	unsigned char reg;
	unsigned char rega = MB86A35_REG_ADDR_SUBADR;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_SUBDAT;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (AGC_user == NULL) {
		rtncode = -EINVAL;
		goto agc_return;
	}

	memset(AGC, 0, tmpsize);
	if (copy_from_user(AGC, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto agc_return;
	}

	switch (AGC->mode) {
	case PARAM_MODE_ISDBT_1UHF:	/* ISDB-T    1seg / UHF */
	case PARAM_MODE_ISDBTMM_1VHF:	/* ISDB-Tmm  1seg / VHF */
	case PARAM_MODE_ISDBTSB_1VHF:	/* ISDB-Tsb  1seg / VHF */
		DTSL = 0x0A;
		AGAIN = 0x5D;  //ES3.0 change
		VIFREFL = 0x3C; //ES 3.0 change
		IFSAMPLE = 0x20;
		OUTSAMPLE = 0x20;
		break;
	case PARAM_MODE_ISDBTSB_3VHF:	/* ISDB-Tsb  3seg / VHF */
	case PARAM_MODE_ISDBT_13UHF:	/* ISDB-T   13seg / UHF */
	case PARAM_MODE_ISDBTMM_13VHF:	/* ISDB-Tmm 13seg / VHF */
		DTSL = 0x0D;  //ES3.0 change
		AGAIN = 0x10;  //ES3.0 change
		VIFREFL = 0x40;  //ES3.0 change
		IFSAMPLE = 0x01;
		OUTSAMPLE = 0x01;
		break;
	default:
		rtncode = -EFAULT;
		goto agc_return;
	};

	sreg = MB86A35_REG_SUBR_IFAH;
	value = (AGC->IFA >> 8) & 0x0F;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFAL;
	value = (AGC->IFA) & 0xFF;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFBH;
	value = (AGC->IFB >> 8) & 0x0F;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFBL;
	value = (AGC->IFB) & 0xFF;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_DTS;
	value = DTSL;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFAGCO;
	value = (AGC->IFAGCO);
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_MAXIFAGC;
	value = (AGC->MAXIFAGC);
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_AGAIN;
	value = AGAIN;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREFH;
	value = 0;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREFL;
	value = VIFREFL;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREF2H;
	value = (AGC->VIFREF2 >> 8) & 0x0F;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_VIFREF2L;
	value = (AGC->VIFREF2) & 0xFF;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_IFSAMPLE;
	value = IFSAMPLE;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_OUTSAMPLE;
	value = OUTSAMPLE;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_LEVELTH;
	value = LEVELTH;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	reg = MB86A35_REG_ADDR_IFAGC;
	rtncode = mb86a35_i2c_master_recv(reg, &AGC->IFAGC, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}

	sreg = MB86A35_REG_SUBR_IFAGCDAC;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &AGC->IFAGCDAC, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}

	rtncode = put_user(AGC->IFAGC, (char *)&AGC_user->IFAGC);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}
	rtncode = put_user(AGC->IFAGCDAC, (char *)&AGC_user->IFAGCDAC);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto agc_return;
	}

  } //ES3.0 end

agc_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SYNC command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context. (NULL)
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SYNC(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
		       unsigned long arg)
{
  int rtncode_rf = 0;
  unsigned char reg_rf = 0;
  u8 chipid0_rf = 0;
  int rtncode = 0;
   rtncode_rf = mb86a35_i2c_rf_recv(reg_rf, &chipid0_rf, 1);
   if (rtncode_rf != 0) {
    		rtncode = -EFAULT;
    		goto sync_return;
    }

   if (chipid0_rf == 0x03) {    //ES2.0 start

	//int rtncode = 0;
	unsigned char reg;
	unsigned int value;

	MB86A35_setting_data_t syncdata[] = {
		{0x28, 0xEC}, {0x29, 0x41}, {0x2A, 0x40}, {0x2B, 0x10},
		{0x28, 0xEE}, {0x29, 0xC0}, {0x2A, 0x04}, {0x2B, 0x00},
		{0x28, 0x64}, {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x80},
		{0x3B, 0xD1}, {0x3C, 0xA1},
		{0x3B, 0xD7}, {0x3C, 0x00},
		{0xff, 0xff}
	};
	int indx;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	for (indx = 0; syncdata[indx].address != 0xFF; indx++) {
		reg = (int)syncdata[indx].address;
		value = (int)syncdata[indx].data;
		mb86a35_i2c_master_send(reg, value);
	}

   }          //ES2.0 end
   else {     //ES3.0 start

	ioctl_ofdm_init_t *OFDM_INIT_user = (ioctl_ofdm_init_t *) arg;
	ioctl_ofdm_init_t *OFDM_INIT = &cmdctrl->OFDM_INIT;
	size_t tmpsize = sizeof(ioctl_ofdm_init_t);
	//int rtncode = 0;
	unsigned char reg;
	unsigned int value;
	int indx;
	
		MB86A35_setting_data_t syncdata[] = {
		{0x28, 0xEC}, {0x29, 0x41}, {0x2A, 0x40}, {0x2B, 0x10},
		{0x28, 0xEE}, {0x29, 0xC0}, {0x2A, 0x04}, {0x2B, 0x00},
		{0x28, 0x64}, {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x80},
		{0x3B, 0xD1}, {0x3C, 0xA1},
		{0x3B, 0xD7}, {0x3C, 0x00},
		{0x28, 0x20}, {0x29, 0x56}, {0x2A, 0x24}, {0x2B, 0x4A}, //ES3.0 1seg only
		{0xD0, 0x02},  //ES3.0 1seg only
		{0x3B, 0xC3}, {0x3C, 0x40},  //ES3.0
		{0x28, 0xEF}, {0x29, 0x08}, {0x2A, 0xC0}, {0x2B, 0x04}, //ES3.0
		{0x4F, 0x03},  //ES3.0
		{0x3B, 0xEA}, {0x3C, 0x99},  //ES3.0
		{0x3B, 0x5D}, {0x3C, 0x40},  //ES3.0
		{0x40, 0x4D}, {0x41, 0x22},  //ES3.0
		{0x40, 0x4F}, {0x41, 0x00},  //ES3.0
		
		{0xff, 0xff}
		};
	
		MB86A35_setting_data_t syncdata1[] = {
		{0x28, 0xEC}, {0x29, 0x41}, {0x2A, 0x40}, {0x2B, 0x10},
		{0x28, 0xEE}, {0x29, 0xC0}, {0x2A, 0x04}, {0x2B, 0x00},
		{0x28, 0x64}, {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x80},
		{0x3B, 0xD1}, {0x3C, 0xA1},
		{0x3B, 0xD7}, {0x3C, 0x00},
		//{0x28, 0x20}, {0x29, 0x56}, {0x2A, 0x24}, {0x2B, 0x4A}, //ES3.0 1seg only
		//{0xD0, 0x02},  //ES3.0 1seg only
		{0x3B, 0xC3}, {0x3C, 0x40},  //ES3.0
		{0x28, 0xEF}, {0x29, 0x08}, {0x2A, 0xC0}, {0x2B, 0x04}, //ES3.0
		{0x4F, 0x03},  //ES3.0
		{0x3B, 0xEA}, {0x3C, 0x99},  //ES3.0
		{0x3B, 0x5D}, {0x3C, 0x40},  //ES3.0
		{0x40, 0x4D}, {0x41, 0x22},  //ES3.0
		{0x40, 0x4F}, {0x41, 0x00},  //ES3.0
		
		{0xff, 0xff}
		};

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (OFDM_INIT_user == NULL) {
		rtncode = -EINVAL;
		goto sync_return;
	}

	memset(OFDM_INIT, 0, tmpsize);
	if (copy_from_user(OFDM_INIT, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto sync_return;
	}

	switch (OFDM_INIT->mode) {
	
	case PARAM_MODE_ISDBT_1UHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
		
		DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

		for (indx = 0; syncdata[indx].address != 0xFF; indx++) {
			reg = (int)syncdata[indx].address;
			value = (int)syncdata[indx].data;
			mb86a35_i2c_master_send(reg, value);
		}
				
		break;

	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
		
		DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

		for (indx = 0; syncdata1[indx].address != 0xFF; indx++) {
			reg = (int)syncdata1[indx].address;
			value = (int)syncdata1[indx].data;
			mb86a35_i2c_master_send(reg, value);
		}
		
		break;

	default:
		rtncode = -EINVAL;
		goto sync_return;
		break;
	}
	// Setting for optimization of multipath threshold -- start
	mb86a35_i2c_slave_send(0x3b, 0x1a, 0x3c, 0x90);
	mb86a35_i2c_slave_send(0x3b, 0x3c, 0x3c, 0x90);
	mb86a35_i2c_slave_send(0x3b, 0x50, 0x3c, 0x90);
	mb86a35_i2c_slave_send(0x3b, 0xea, 0x3c, 0x19);
	// Setting for optimization of multipath threshold -- end

	// Setting for optimization of synchronization -- start
	mb86a35_i2c_master_send(0x28, 0x32);
	mb86a35_i2c_master_send(0x29, 0x00);
	mb86a35_i2c_master_send(0x2a, 0x00);
	mb86a35_i2c_master_send(0x2b, 0xb1);

	mb86a35_i2c_master_send(0x28, 0x33);
	mb86a35_i2c_master_send(0x29, 0x00);
	mb86a35_i2c_master_send(0x2a, 0x00);
	mb86a35_i2c_master_send(0x2b, 0xb1);

	mb86a35_i2c_master_send(0x28, 0x34);
	mb86a35_i2c_master_send(0x29, 0x00);
	mb86a35_i2c_master_send(0x2a, 0x00);
	mb86a35_i2c_master_send(0x2b, 0x00);

	mb86a35_i2c_master_send(0x28, 0x64);
	mb86a35_i2c_master_send(0x29, 0x00);
	mb86a35_i2c_master_send(0x2a, 0x00);
	mb86a35_i2c_master_send(0x2b, 0xb1);
	// Setting for optimization of synchronization -- end
  } //ES3.0 end

sync_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_FEC command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context. (NULL)
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_FEC(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
		      unsigned long arg)
{
	int rtncode = 0;
	unsigned char reg;
	unsigned int value;
	MB86A35_setting_data_t fecdata[] = {
		{0x50, 0x38}, {0x51, 0xA0},
		{0xff, 0xff}
	};
	int indx;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	for (indx = 0; fecdata[indx].address != 0xFF; indx++) {
		reg = (int)fecdata[indx].address;
		value = (int)fecdata[indx].data;
		mb86a35_i2c_master_send(reg, value);
	}

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_GPIO_SETUP command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context. (NULL)
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_GPIO_SETUP(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			     unsigned long arg)
{
	ioctl_port_t *GPIO_user = (ioctl_port_t *) arg;
	ioctl_port_t *GPIO = &cmdctrl->GPIO;
	size_t tmpsize = sizeof(ioctl_port_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (GPIO_user == NULL) {
		rtncode = -EINVAL;
		goto gpio_setup_return;
	}

	memset(GPIO, 0, tmpsize);
	if (copy_from_user(GPIO, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto gpio_setup_return;
	}

	if ((GPIO->GPIO_DAT != PARAM_GPIO_DAT_AC_MODE_ACCLK)
	    && (GPIO->GPIO_DAT != PARAM_GPIO_DAT_AC_MODE_ACCLK)) {
		rtncode = -EINVAL;
		goto gpio_setup_return;
	}

	reg = MB86A35_REG_ADDR_GPIO_DAT;
	value = GPIO->GPIO_DAT;
	rtncode =
	    mb86a35_i2c_master_send_mask(reg, value,
					 MB86A35_I2CMASK_GPIO_DAT_ACMODE,
					 MB86A35_MASK_GPIO_DAT_GPIO_DAT);
	if (rtncode != 0) {
		goto gpio_setup_return;
	}

	reg = MB86A35_REG_ADDR_GPIO_OUTSEL;
	value = GPIO->GPIO_OUTSEL;
	mb86a35_i2c_master_send(reg, value);

gpio_setup_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_GPIO_READ command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context. (NULL)
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_GPIO_READ(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_port_t *GPIO_user = (ioctl_port_t *) arg;
	ioctl_port_t *GPIO = &cmdctrl->GPIO;
	int rtncode = 0;
	unsigned char reg;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (GPIO_user == NULL) {
		rtncode = -EINVAL;
		goto gpio_read_return;
	}

	reg = MB86A35_REG_ADDR_GPIO_DAT;
	rtncode = mb86a35_i2c_master_recv(reg, &GPIO->GPIO_DAT, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto gpio_read_return;
	}
	GPIO->GPIO_DAT &= MB86A35_MASK_GPIO_DAT_GPIO_DAT;

	if (copy_to_user(&GPIO_user->GPIO_DAT, &GPIO->GPIO_DAT, 1)) {
		DBGPRINT(PRINT_LHEADERFMT " : copy_to_user error.\n",
			 PRINT_LHEADER);
		rtncode = -EFAULT;
		goto gpio_read_return;
	}

gpio_read_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_GPIO_WRITE command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context. (NULL)
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_GPIO_WRITE(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			     unsigned long arg)
{
	ioctl_port_t *GPIO_user = (ioctl_port_t *) arg;
	ioctl_port_t *GPIO = &cmdctrl->GPIO;
	size_t tmpsize = sizeof(ioctl_port_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (GPIO_user == NULL) {
		rtncode = -EINVAL;
		goto gpio_write_return;
	}

	memset(GPIO, 0, tmpsize);
	if (copy_from_user(GPIO, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto gpio_write_return;
	}
	DBGPRINT(PRINT_LHEADERFMT " : GPIO->GPIO_DAT : 0x%02x).\n",
		 PRINT_LHEADER, (int)GPIO->GPIO_DAT);

	reg = MB86A35_REG_ADDR_GPIO_DAT;
	value = GPIO->GPIO_DAT;
	rtncode = mb86a35_i2c_master_send_mask(reg, value,
					       (MB86A35_I2CMASK_GPIO_DAT |
						MB86A35_MASK_GPIO_DAT_AC_MODE),
					       MB86A35_MASK_GPIO_DAT_GPIO_DAT);
	if (rtncode != 0) {
		goto gpio_write_return;
	}

gpio_write_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_ANALOG command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context. (NULL)
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_ANALOG(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			 unsigned long arg)
{
	ioctl_port_t *GPIO_user = (ioctl_port_t *) arg;
	ioctl_port_t *GPIO = &cmdctrl->GPIO;
	size_t tmpsize = sizeof(ioctl_port_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned int value;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (GPIO_user == NULL) {
		rtncode = -EINVAL;
		goto analog_return;
	}

	memset(GPIO, 0, tmpsize);
	if (copy_from_user(GPIO, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto analog_return;
	}

	reg = MB86A35_REG_ADDR_DACOUT;
	value = GPIO->DACOUT;
	mb86a35_i2c_master_send(reg, value);

analog_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SEQ_GETSTAT command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SEQ_GETSTAT(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			      unsigned long arg)
{
	ioctl_seq_t *SEQ_user = (ioctl_seq_t *) arg;
	ioctl_seq_t *SEQ = &cmdctrl->SEQ;
	size_t tmpsize = sizeof(ioctl_seq_t);
	int rtncode = 0;
	unsigned char reg;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SEQ_user == NULL) {
		rtncode = -EINVAL;
		goto seq_getstate_return;
	}

	memset(SEQ, 0, tmpsize);
	if (copy_from_user(SEQ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto seq_getstate_return;
	}

	reg = MB86A35_REG_ADDR_SYNC_STATE;
	rtncode = mb86a35_i2c_master_recv(reg, &SEQ->SYNC_STATE, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto seq_getstate_return;
	}

	rtncode = put_user(SEQ->SYNC_STATE, (char *)&SEQ_user->SYNC_STATE);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto seq_getstate_return;
	}

	DBGPRINT(PRINT_LHEADERFMT " : SEQ.SYNC_STATE [%x]. \n", PRINT_LHEADER,
		 (int)(SEQ->SYNC_STATE));

seq_getstate_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SEQ_SETMODE command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SEQ_SETMODE(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			      unsigned long arg)
{
	ioctl_seq_t *SEQ_user = (ioctl_seq_t *) arg;
	ioctl_seq_t *SEQ = &cmdctrl->SEQ;
	size_t tmpsize = sizeof(ioctl_seq_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned int value;
	u8 tmpdata = 0;
	

#define	SEQ_SETMODE_PATTERN1	( PARAM_MODED_CTRL_M3G32 | PARAM_MODED_CTRL_M3G16 | PARAM_MODED_CTRL_M3G08 | PARAM_MODED_CTRL_M3G04	\
				| PARAM_MODED_CTRL_M2G32 | PARAM_MODED_CTRL_M2G16 | PARAM_MODED_CTRL_M2G08 | PARAM_MODED_CTRL_M2G04 )
#define	SEQ_SETMODE_PATTERN2	( PARAM_MODED_CTRL_M1G32 | PARAM_MODED_CTRL_M1G16 | PARAM_MODED_CTRL_M1G08 | PARAM_MODED_CTRL_M1G04 )
/*#define	SEQ_SETMODE_PATTERN2	( PARAM_MODED_STAT_MDFIX	\
				| PARAM_MODED_STAT_MODE2 | PARAM_MODED_STAT_MODE3		\
				| PARAM_MODED_STAT_GUARDE14 | PARAM_MODED_STAT_GUARDE18 | PARAM_MODED_STAT_GUARDE116 )
*/
	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SEQ_user == NULL) {
		rtncode = -EINVAL;
		goto seq_setmode_return;
	}

	memset(SEQ, 0, tmpsize);
	if (copy_from_user(SEQ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto seq_setmode_return;
	}

	if ((SEQ->MODED_CTRL & ~SEQ_SETMODE_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto seq_setmode_return;
	}

	if ((SEQ->MODED_CTRL2 & ~SEQ_SETMODE_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto seq_setmode_return;
	}
	
	//if ((SEQ->MODED_STAT & ~SEQ_SETMODE_PATTERN2) != 0) {
	//	rtncode = -EINVAL;
	//	goto seq_setmode_return;
	//} 

	reg = MB86A35_REG_ADDR_MODED_CTRL;
	value = SEQ->MODED_CTRL;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_MODED_CTRL2;
	value = SEQ->MODED_CTRL2;
	mb86a35_i2c_master_send(reg, value);

	//reg = MB86A35_REG_ADDR_MODED_STAT;
	//value = SEQ->MODED_STAT;
	//mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_MODED_STAT;
	switch (SEQ->MODE_DETECT) {
	case PARAM_MODE_DETECT_ON:
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto seq_setmode_return;
		}
		tmpdata = tmpdata & 0xEF;
		mb86a35_i2c_master_send( reg, tmpdata ) ;
		break;

	case PARAM_MODE_DETECT_OFF:
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto seq_setmode_return;
		}
		tmpdata = tmpdata & 0xE0;
		tmpdata = tmpdata | 0x10 | SEQ->MODE | SEQ->GUARD;

		mb86a35_i2c_master_send( reg, tmpdata ) ;
		break;

	default:
		rtncode = -EINVAL;
		goto seq_setmode_return;
		break;
	}

seq_setmode_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SEQ_GETMODE command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SEQ_GETMODE(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			      unsigned long arg)
{
	ioctl_seq_t *SEQ_user = (ioctl_seq_t *) arg;
	ioctl_seq_t *SEQ = &cmdctrl->SEQ;
	size_t tmpsize = sizeof(ioctl_seq_t);
	int rtncode = 0;
	unsigned char reg;
	//unsigned int value;
	u8 tmpdata0, tmpdata1 = 0;


	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SEQ_user == NULL) {
		rtncode = -EINVAL;
		goto seq_getmode_return;
	}

	memset(SEQ, 0, tmpsize);
	if (copy_from_user(SEQ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto seq_getmode_return;
	}

	reg = MB86A35_REG_ADDR_MODED_STAT;
	rtncode = mb86a35_i2c_master_recv(reg, &tmpdata0, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto seq_getmode_return;
	}
	tmpdata1 = tmpdata0 & 0x60;
	if (copy_to_user
		((void *)&SEQ_user->MODED_STAT, (void *)&tmpdata1, 1)) {
		rtncode = -EFAULT;
		goto seq_getmode_return;
	}

        tmpdata1 = tmpdata0 & 0x0c;
//	tmpdata1 = tmpdata1 >> 2;
        if (copy_to_user
                ((void *)&SEQ_user->MODE, (void *)&tmpdata1, 1)) {
                rtncode = -EFAULT;
                goto seq_getmode_return;
        }

        tmpdata1 = tmpdata0 & 0x03;
        if (copy_to_user
                ((void *)&SEQ_user->GUARD, (void *)&tmpdata1, 1)) {
                rtncode = -EFAULT;
                goto seq_getmode_return;
        }

seq_getmode_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SEQ_GETTMCC command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SEQ_GETTMCC(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			      unsigned long arg)
{
	ioctl_seq_t *SEQ_user = (ioctl_seq_t *) arg;
	ioctl_seq_t *SEQ = &cmdctrl->SEQ;
	size_t tmpsize = sizeof(ioctl_seq_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_TMCC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_TMCC_SUBD;
	unsigned int value;
#define	SEQ_GETTMCC_PATTERN1	( PARAM_TMCCREAD_TMCCLOCK_AUTO | PARAM_TMCCREAD_TMCCLOCK_NOAUTO )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SEQ_user == NULL) {
		rtncode = -EINVAL;
		goto seq_gettmcc_return;
	}

	memset(SEQ, 0, tmpsize);
	if (copy_from_user(SEQ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto seq_gettmcc_return;
	}

	if ((SEQ->TMCCREAD & ~SEQ_GETTMCC_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto seq_gettmcc_return;
	}

	sreg = MB86A35_REG_SUBR_TMCCREAD;
	value = (SEQ->TMCCREAD & MB86A35_MASK_TMCCREAD_TMCCLOCK);
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_TMCC0;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &SEQ->TMCC[0], 32);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto seq_gettmcc_return;
	}

	sreg = MB86A35_REG_SUBR_FEC_IN;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &SEQ->FEC_IN, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto seq_gettmcc_return;
	}
	SEQ->FEC_IN &=
	    (MB86A35_MASK_FEC_IN_CORRECT | MB86A35_MASK_FEC_IN_VALID);

	if (copy_to_user(&SEQ_user->TMCC[0], &SEQ->TMCC[0], 32)) {
		DBGPRINT(PRINT_LHEADERFMT " : copy_to_user error.\n",
			 PRINT_LHEADER);
		rtncode = -EFAULT;
		goto seq_gettmcc_return;
	}

	rtncode = put_user(SEQ->FEC_IN, (char *)&SEQ_user->FEC_IN);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto seq_gettmcc_return;
	}

seq_gettmcc_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_BER_MONISTAT command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_BER_MONISTAT(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			       unsigned long arg)
{
	ioctl_ber_moni_t *BER_user = (ioctl_ber_moni_t *) arg;
	ioctl_ber_moni_t *BER = &cmdctrl->BER;
	size_t tmpsize = sizeof(ioctl_ber_moni_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_TMCC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_TMCC_SUBD;
#define	BER_MONISTAT_PATTERN1	( PARAM_S8WAIT_TS8_BER8 | PARAM_S8WAIT_TS9_BER8	\
				| PARAM_S8WAIT_TS8_BER9 | PARAM_S8WAIT_TS9_BER9 )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (BER_user == NULL) {
		rtncode = -EINVAL;
		goto ber_monistat_return;
	}

	memset(BER, 0, tmpsize);
	if (copy_from_user(BER, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ber_monistat_return;
	}

	if ((BER->S8WAIT & ~BER_MONISTAT_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ber_monistat_return;
	}

	sreg = MB86A35_REG_SUBR_S8WAIT;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &BER->S8WAIT, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_monistat_return;
	}
	BER->S8WAIT &= MB86A35_I2CMASK_TMCC_SUB_S8WAIT;

	rtncode = put_user(BER->S8WAIT, (char *)&BER_user->S8WAIT);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_monistat_return;
	}

ber_monistat_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_BER_MONICONFIG command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_BER_MONICONFIG(mb86a35_cmdcontrol_t * cmdctrl,
				 unsigned int cmd, unsigned long arg)
{
	ioctl_ber_moni_t *BER_user = (ioctl_ber_moni_t *) arg;
	ioctl_ber_moni_t *BER = &cmdctrl->BER;
	size_t tmpsize = sizeof(ioctl_ber_moni_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value;
#define	BER_MONICONFIG_PATTERN1	( PARAM_S8WAIT_TS8_BER8 | PARAM_S8WAIT_TS9_BER8		\
				| PARAM_S8WAIT_TS8_BER9 | PARAM_S8WAIT_TS9_BER9 )
#define	BER_MONICONFIG_PATTERN2	( PARAM_VBERXRST_VBERXRSTC_C | PARAM_VBERXRST_VBERXRSTC_E	\
				| PARAM_VBERXRST_VBERXRSTB_C | PARAM_VBERXRST_VBERXRSTB_E	\
				| PARAM_VBERXRST_VBERXRSTA_C | PARAM_VBERXRST_VBERXRSTA_E )
#define	BER_MONICONFIG_PATTERN3	( PARAM_RSBERON_RSBERAUTO_M | PARAM_RSBERON_RSBERAUTO_A		\
				| PARAM_RSBERON_RSBERC_S | PARAM_RSBERON_RSBERC_B		\
				| PARAM_RSBERON_RSBERB_S | PARAM_RSBERON_RSBERB_B		\
				| PARAM_RSBERON_RSBERA_S | PARAM_RSBERON_RSBERA_B )
#define	BER_MONICONFIG_PATTERN4	( PARAM_RSBERXRST_RSBERXRSTC_S | PARAM_RSBERXRST_RSBERXRSTC_B	\
				| PARAM_RSBERXRST_RSBERXRSTB_S | PARAM_RSBERXRST_RSBERXRSTB_B	\
				| PARAM_RSBERXRST_RSBERXRSTA_S | PARAM_RSBERXRST_RSBERXRSTA_B )
#define	BER_MONICONFIG_PATTERN5	( PARAM_RSBERCEFLG_SBERCEFC_E | PARAM_RSBERCEFLG_SBERCEFB_E | PARAM_RSBERCEFLG_SBERCEFA_E )
#define	BER_MONICONFIG_PATTERN6	( PARAM_RSBERTHFLG_RSBERTHRC_R | PARAM_RSBERTHFLG_RSBERTHRB_R | PARAM_RSBERTHFLG_RSBERTHRA_R	\
				| PARAM_RSBERTHFLG_RSBERTHFC_F | PARAM_RSBERTHFLG_RSBERTHFB_F | PARAM_RSBERTHFLG_RSBERTHFA_F )
#define	BER_MONICONFIG_PATTERN7	( PARAM_PEREN_PERENC_F | PARAM_PEREN_PERENB_F | PARAM_PEREN_PERENA_F )
	u32 d32;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (BER_user == NULL) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	memset(BER, 0, tmpsize);
	if (copy_from_user(BER, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ber_moniconfig_return;
	}

	if ((BER->S8WAIT & ~BER_MONICONFIG_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if ((BER->VBERON != PARAM_VBERON_BEFORE) && (BER->VBERON != 0)) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if ((BER->VBERXRST & ~BER_MONICONFIG_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->VBERSETA > 16777215) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->VBERSETB > 16777215) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->VBERSETC > 16777215) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if ((BER->RSBERON & ~BER_MONICONFIG_PATTERN3) != 0) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if ((BER->RSBERXRST & ~BER_MONICONFIG_PATTERN4) != 0) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if ((BER->RSBERCEFLG & ~BER_MONICONFIG_PATTERN5) != 0) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if ((BER->RSBERTHFLG & ~BER_MONICONFIG_PATTERN6) != 0) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->SBERSETA > 65535) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->SBERSETB > 65535) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->SBERSETC > 65535) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if ((BER->PEREN & ~BER_MONICONFIG_PATTERN7) != 0) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->PERSNUMA > 65535) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->PERSNUMB > 65535) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	if (BER->PERSNUMC > 65535) {
		rtncode = -EINVAL;
		goto ber_moniconfig_return;
	}

	sreg = MB86A35_REG_SUBR_VBERSETA0;
	/* VBERSETA - C Setting */
	d32 = BER->VBERSETA;
	value = (d32 >> 16) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	d32 = BER->VBERSETB;
	value = (d32 >> 16) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	d32 = BER->VBERSETC;
	value = (d32 >> 16) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	reg = MB86A35_REG_ADDR_VBERON;
	value = BER->VBERON;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_VBERXRST;
	value = BER->VBERXRST;
	mb86a35_i2c_master_send(reg, value);

	/* SBERSETA - C Setting */
	sreg = MB86A35_REG_SUBR_SBERSETA0;
	d32 = BER->SBERSETA;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	d32 = BER->SBERSETB;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	d32 = BER->SBERSETC;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	reg = MB86A35_REG_ADDR_RSBERON;
	value = BER->RSBERON;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_RSBERTHFLG;
	value = BER->RSBERTHFLG;
	mb86a35_i2c_master_send(reg, value);

	/* PERSNUMA - C Setting */
	sreg = MB86A35_REG_SUBR_PERSNUMA0;
	d32 = BER->PERSNUMA;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	d32 = BER->PERSNUMB;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	d32 = BER->PERSNUMC;
	value = (d32 >> 8) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);
	sreg += 1;
	value = (d32) & 0xff;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_PEREN;
	value = BER->PEREN & 0x0f;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

ber_moniconfig_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_BER_MONISTART command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_BER_MONISTART(mb86a35_cmdcontrol_t * cmdctrl,
				unsigned int cmd, unsigned long arg)
{
	ioctl_ber_moni_t *BER = &cmdctrl->BER;
	int rtncode = 0;
	unsigned char reg;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value;
	u8 VBERON = 0;
	u8 VBERXRST = 0;
	u8 RSBERON = 0;
	u8 PEREN = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	VBERON = BER->VBERON & PARAM_VBERON_BEFORE;
	if (BER->VBERON == PARAM_VBERON_BEFORE) {
		VBERXRST = BER->VBERXRST & (PARAM_VBERXRST_VBERXRSTC_E
					    | PARAM_VBERXRST_VBERXRSTB_E
					    | PARAM_VBERXRST_VBERXRSTA_E);
		BER->VBERXRST_SAVE	= VBERXRST;

		if (VBERXRST != 0) {
			reg = MB86A35_REG_ADDR_VBERXRST;
			value = 0;
			mb86a35_i2c_master_send(reg, value);

			value = VBERXRST;
			mb86a35_i2c_master_send(reg, value);
		}
	}

	RSBERON = BER->RSBERON & (PARAM_RSBERON_RSBERC_B
				  | PARAM_RSBERON_RSBERB_B
				  | PARAM_RSBERON_RSBERA_B);
	if (RSBERON != 0) {
		reg = MB86A35_REG_ADDR_RSBERON;
		RSBERON |= BER->RSBERON & PARAM_RSBERON_RSBERAUTO_A;
		value = RSBERON;
		mb86a35_i2c_master_send(reg, value);

		reg = MB86A35_REG_ADDR_RSBERXRST;
		value = 0;
		mb86a35_i2c_master_send(reg, value);

		value = BER->RSBERXRST;
		mb86a35_i2c_master_send(reg, value);
	}

	PEREN = BER->PEREN & (PARAM_PEREN_PERENC_F
			      | PARAM_PEREN_PERENB_F | PARAM_PEREN_PERENA_F);
	if (PEREN != 0) {
		sreg = MB86A35_REG_SUBR_PERRST;
//		value = PEREN;
		value = BER->PERRST;
		mb86a35_i2c_slave_send(rega, sreg, regd, value);

		value = 0;
		mb86a35_i2c_slave_send(rega, sreg, regd, value);
	}
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_BER_MONIGET command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_BER_MONIGET(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			      unsigned long arg)
{
	ioctl_ber_moni_t *BER_user = (ioctl_ber_moni_t *) arg;
	ioctl_ber_moni_t *BER = &cmdctrl->BER;
	size_t cpysize = (sizeof(u32) * 6) + (sizeof(u16) * 3);
	int rtncode = 0;
	unsigned char reg;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
	u8 tmpdata = 0;
	u8 VBERFLG = 0;
	u8 RSBERCEFLG = 0;
	u8 PERFLG = 0;
	u8 CHECKFLG = 0;
//	int loop = 5;
	int automode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (BER_user == NULL) {
		rtncode = -EINVAL;
		goto ber_moniget_return;
	}

	if ((BER->RSBERON & PARAM_RSBERON_RSBERAUTO_A) ==
	    PARAM_RSBERON_RSBERAUTO_A) {
		/*** WAIT ***/
//		mdelay(1000);

		automode = 1;
	}

	/*********************************************/
	reg = MB86A35_REG_ADDR_RSBERTHFLG;
	rtncode = mb86a35_i2c_master_recv(reg, &BER->RSBERTHFLG, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}
	rtncode = put_user(BER->RSBERTHFLG, (char *)&BER_user->RSBERTHFLG);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}
	
	reg = MB86A35_REG_ADDR_RSBERRST;
	rtncode = mb86a35_i2c_master_recv(reg, &BER->RSBERXRST, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}
	rtncode = put_user(BER->RSBERXRST, (char *)&BER_user->RSBERXRST);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	sreg = MB86A35_REG_SUBR_PEREN;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &BER->PEREN, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}
	rtncode = put_user(BER->PEREN, (char *)&BER_user->PEREN);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	sreg = MB86A35_REG_SUBR_PERRST;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &BER->PERRST, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}
	rtncode = put_user(BER->PERRST, (char *)&BER_user->PERRST);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	/*********************************************/
	CHECKFLG = BER->VBERXRST & (PARAM_VBERXRST_VBERXRSTC_E
				    | PARAM_VBERXRST_VBERXRSTB_E
				    | PARAM_VBERXRST_VBERXRSTA_E);
	reg = MB86A35_REG_ADDR_VBERFLG;
#if 0
	loop = 5;
	while (loop--) {
		rtncode = mb86a35_i2c_master_recv(reg, &BER->VBERFLG, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		if ((BER->VBERFLG & CHECKFLG) == CHECKFLG)
			break;

		mdelay(5);
	}
#endif
	rtncode = mb86a35_i2c_master_recv(reg, &BER->VBERFLG, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	rtncode = put_user(BER->VBERFLG, (char *)&BER_user->VBERFLG);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	if( automode == 1 )	goto moniget_automode;

	CHECKFLG = BER->RSBERON & (PARAM_RSBERON_RSBERC_B
				   | PARAM_RSBERON_RSBERB_B
				   | PARAM_RSBERON_RSBERA_B);
	reg = MB86A35_REG_ADDR_RSBERCEFLG;

#if 0
	loop = 5;
	while (loop--) {
		rtncode = mb86a35_i2c_master_recv(reg, &BER->RSBERCEFLG, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		if ((BER->RSBERCEFLG & CHECKFLG) == CHECKFLG)
			break;

		mdelay(5);
	}
#endif

	rtncode = mb86a35_i2c_master_recv(reg, &BER->RSBERCEFLG, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	rtncode = put_user(BER->RSBERCEFLG, (char *)&BER_user->RSBERCEFLG);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

moniget_automode:
	CHECKFLG = BER->PEREN & (PARAM_PEREN_PERENC_F
				 | PARAM_PEREN_PERENB_F | PARAM_PEREN_PERENA_F);
	sreg = MB86A35_REG_SUBR_PERFLG;
#if 0
	loop = 5;
	while (loop--) {
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &BER->PERFLG, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		if ((BER->PERFLG & CHECKFLG) == CHECKFLG)
			break;

		mdelay(5);
	}
#endif

	rtncode =
	    mb86a35_i2c_slave_recv(rega, sreg, regd, &BER->PERFLG, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	rtncode = put_user(BER->PERFLG, (char *)&BER_user->PERFLG);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	/*********************************************/
	if (BER->VBERFLG & PARAM_VBERFLG_VBERFLGA) {
		BER->VBERDTA = 0;
		reg = MB86A35_REG_ADDR_VBERDTA0;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTA += ((tmpdata & 0x0F) << 16);

		reg = MB86A35_REG_ADDR_VBERDTA1;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTA += (tmpdata << 8);

		reg = MB86A35_REG_ADDR_VBERDTA2;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTA += tmpdata;
	}

	/*********************************************/
	BER->VBERDTB = 0;
	if (BER->VBERFLG & PARAM_VBERFLG_VBERFLGB) {
		reg = MB86A35_REG_ADDR_VBERDTB0;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTB += ((tmpdata & 0x0F) << 16);

		reg = MB86A35_REG_ADDR_VBERDTB1;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTB += (tmpdata << 8);

		reg = MB86A35_REG_ADDR_VBERDTB2;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTB += tmpdata;
	}

	/*********************************************/
	BER->VBERDTC = 0;
	if (BER->VBERFLG & PARAM_VBERFLG_VBERFLGC) {
		reg = MB86A35_REG_ADDR_VBERDTC0;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTC += ((tmpdata & 0x0F) << 16);

		reg = MB86A35_REG_ADDR_VBERDTC1;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTC += (tmpdata << 8);

		reg = MB86A35_REG_ADDR_VBERDTC2;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->VBERDTC += tmpdata;
	}

	/*********************************************/
	BER->RSBERDTA = 0;
	if ((automode == 1)
	|| (BER->RSBERCEFLG & PARAM_RSBERCEFLG_SBERCEFA)) {
		reg = MB86A35_REG_ADDR_RSBERDTA0;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTA += ((tmpdata & 0x0F) << 16);

		reg = MB86A35_REG_ADDR_RSBERDTA1;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTA += (tmpdata << 8);

		reg = MB86A35_REG_ADDR_RSBERDTA2;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTA += tmpdata;
	}

	/*********************************************/
	BER->RSBERDTB = 0;
	if ((automode == 1)
	|| (BER->RSBERCEFLG & PARAM_RSBERCEFLG_SBERCEFB)) {
		reg = MB86A35_REG_ADDR_RSBERDTB0;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTB += ((tmpdata & 0x0F) << 16);

		reg = MB86A35_REG_ADDR_RSBERDTB1;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTB += (tmpdata << 8);

		reg = MB86A35_REG_ADDR_RSBERDTB2;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTB += tmpdata;
	}

	/*********************************************/
	BER->RSBERDTC = 0;
	if ((automode == 1)
	|| (BER->RSBERCEFLG & PARAM_RSBERCEFLG_SBERCEFC)) {
		reg = MB86A35_REG_ADDR_RSBERDTC0;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTC += ((tmpdata & 0x0F) << 16);

		reg = MB86A35_REG_ADDR_RSBERDTC1;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTC += (tmpdata << 8);

		reg = MB86A35_REG_ADDR_RSBERDTC2;
		rtncode = mb86a35_i2c_master_recv(reg, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->RSBERDTC += tmpdata;
	}

	/*********************************************/
	BER->PERERRA = 0;
	if (BER->PERFLG & PARAM_PERFLG_PERFLGA) {
		sreg = MB86A35_REG_SUBR_PERERRA0;
		rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->PERERRA += (tmpdata << 8);

		sreg = MB86A35_REG_SUBR_PERERRA1;
		rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->PERERRA += tmpdata;
	}

	/*********************************************/
	BER->PERERRB = 0;
	if (BER->PERFLG & PARAM_PERFLG_PERFLGB) {
		sreg = MB86A35_REG_SUBR_PERERRB0;
		rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->PERERRB += (tmpdata << 8);

		value = 0;
		sreg = MB86A35_REG_SUBR_PERERRB1;
		rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->PERERRB += tmpdata;
	}

	/*********************************************/
	BER->PERERRC = 0;
	if (BER->PERFLG & PARAM_PERFLG_PERFLGC) {
		sreg = MB86A35_REG_SUBR_PERERRC0;
		rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->PERERRC += (tmpdata << 8);

		value = 0;
		sreg = MB86A35_REG_SUBR_PERERRC1;
		rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &tmpdata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ber_moniget_return;
		}
		BER->PERERRC += tmpdata;
	}

	/*********************************************/
	if (copy_to_user
	    ((void *)&BER_user->VBERDTA, (void *)&BER->VBERDTA, cpysize)) {
		rtncode = -EFAULT;
		goto ber_moniget_return;
	}

	/*********************************************//* Moniter restart */
	VBERFLG = BER->VBERFLG & (PARAM_VBERFLG_VBERFLGC
				  | PARAM_VBERFLG_VBERFLGB
				  | PARAM_VBERFLG_VBERFLGA);
	if (VBERFLG != 0) {
		reg = MB86A35_REG_ADDR_VBERXRST;
		value = 0;
		mb86a35_i2c_master_send(reg, value);

	//	value = VBERFLG;
		value = BER->VBERXRST_SAVE;
		mb86a35_i2c_master_send(reg, value);
	}

	RSBERCEFLG = BER->RSBERCEFLG & (PARAM_RSBERCEFLG_SBERCEFC_E
					| PARAM_RSBERCEFLG_SBERCEFB_E
					| PARAM_RSBERCEFLG_SBERCEFA_E);
	if ( automode == 1 ) {
//		reg = MB86A35_REG_ADDR_RSBERXRST;
//		value = 0;
//		mb86a35_i2c_master_send(reg, value);
//
//		value = ( BER->RSBERON & (PARAM_RSBERON_RSBERC_B
//					  | PARAM_RSBERON_RSBERB_B
//					  | PARAM_RSBERON_RSBERA_B ));
//
//		mb86a35_i2c_master_send(reg, value);
	} else if (RSBERCEFLG != 0) {
		reg = MB86A35_REG_ADDR_RSBERXRST;
		value = ~RSBERCEFLG;
		mb86a35_i2c_master_send(reg, value);

	//	value = RSBERCEFLG;
		value = ( BER->RSBERON & (PARAM_RSBERON_RSBERC_B
					  | PARAM_RSBERON_RSBERB_B
					  | PARAM_RSBERON_RSBERA_B ));

		mb86a35_i2c_master_send(reg, value);
	}

	PERFLG = BER->PERFLG & (PARAM_PERFLG_PERFLGC
				| PARAM_PERFLG_PERFLGB | PARAM_PERFLG_PERFLGA);
	if (PERFLG != 0) {
		sreg = MB86A35_REG_SUBR_PERRST;
		value = PERFLG;
		mb86a35_i2c_slave_send(rega, sreg, regd, value);

		value = 0;
		mb86a35_i2c_slave_send(rega, sreg, regd, value);
	}

ber_moniget_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_BER_MONISTOP command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_BER_MONISTOP(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			       unsigned long arg)
{
	int rtncode = 0;
	unsigned char reg;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value;
	u8 rdata = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	reg = MB86A35_REG_ADDR_RSBERON;
	rtncode = mb86a35_i2c_master_recv(reg, &rdata, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ber_monistop_return;
	}

	reg = MB86A35_REG_ADDR_VBERON;
	value = 0;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_RSBERON;
	value = 0;
	mb86a35_i2c_master_send(reg, value);

	sreg = MB86A35_REG_SUBR_PEREN;
	value = 0;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

ber_monistop_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_TS_START command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_TS_START(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	ioctl_ts_t *TS_user = (ioctl_ts_t *) arg;
	ioctl_ts_t *TS = &cmdctrl->TS;
	size_t tmpsize = sizeof(ioctl_ts_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
#define	TS_START_PATTERN1	( PARAM_RS0_RSEN_OFF | PARAM_RS0_RSEN_ON )
#define	TS_START_PATTERN2	( PARAM_SBER_SCLKSEL_OFF | PARAM_SBER_SCLKSEL_ON | PARAM_SBER_SBERSEL_OFF | PARAM_SBER_SBERSEL_ON	\
				| PARAM_SBER_SPACON_OFF | PARAM_SBER_SPACON_ON | PARAM_SBER_SENON_OFF | PARAM_SBER_SENON_ON		\
				| PARAM_SBER_SLAYER_OFF | PARAM_SBER_SLAYER_A | PARAM_SBER_SLAYER_B | PARAM_SBER_SLAYER_C		\
				| PARAM_SBER_SLAYER_AB | PARAM_SBER_SLAYER_AC | PARAM_SBER_SLAYER_BC | PARAM_SBER_SLAYER_ALL )
#define	TS_START_PATTERN3	( PARAM_SBER2_SCLKSEL_OFF | PARAM_SBER2_SCLKSEL_ON		\
				| PARAM_SBER2_SBERSEL_OFF | PARAM_SBER2_SBERSEL_ON		\
				| PARAM_SBER2_SPACON_OFF  | PARAM_SBER2_SPACON_ON		\
				| PARAM_SBER2_SENON_OFF   | PARAM_SBER2_SENON_ON		\
				| PARAM_SBER2_SLAYER_OFF  | PARAM_SBER2_SLAYER_A )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (TS_user == NULL) {
		rtncode = -EINVAL;
		goto ts_start_return;
	}

	memset(TS, 0, tmpsize);
	if (copy_from_user(TS, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ts_start_return;
	}

	if ((TS->RS0 & ~TS_START_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ts_start_return;
	}

	if ((TS->SBER & ~TS_START_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto ts_start_return;
	}

	if ((TS->SBER2 & ~TS_START_PATTERN3) != 0) {
		rtncode = -EINVAL;
		goto ts_start_return;
	}

	sreg = MB86A35_REG_SUBR_RS0;
	value = TS->RS0;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_RS0,
					MB86A35_MASK_RS0_RSEN);
	if (rtncode != 0) {
		goto ts_start_return;
	}

	sreg = MB86A35_REG_SUBR_SBER;
	value = TS->SBER;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_SBER2;
	value = TS->SBER2;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

ts_start_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_TS_STOP command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_TS_STOP(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			  unsigned long arg)
{
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	sreg = MB86A35_REG_SUBR_SBER;
	value = 0;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_SBER2;
	value = 0;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_TS_CONFIG command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_TS_CONFIG(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_ts_t *TS_user = (ioctl_ts_t *) arg;
	ioctl_ts_t *TS = &cmdctrl->TS;
	size_t tmpsize = sizeof(ioctl_ts_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
	u8 TS_CONFIG_PATTERN5 = 0;
	u8 TS_CONFIG_PATTERN6 = 0;
#define	TS_CONFIG_PATTERN1	( PARAM_TSOUT_TSERRINV_OFF | PARAM_TSOUT_TSERRINV_ON		\
				| PARAM_TSOUT_TSENINV_OFF | PARAM_TSOUT_TSENINV_ON		\
				| PARAM_TSOUT_TSSINV_MSB | PARAM_TSOUT_TSSINV_LSB		\
				| PARAM_TSOUT_TSERRMASK2_OFF | PARAM_TSOUT_TSERRMASK2_ON	\
				| PARAM_TSOUT_TSERRMASK_OFF | PARAM_TSOUT_TSERRMASK_ON )
#define	TS_CONFIG_PATTERN2	( PARAM_TSOUT2_TSERRINV_OFF | PARAM_TSOUT2_TSERRINV_ON		\
				| PARAM_TSOUT2_TSENINV_OFF  | PARAM_TSOUT2_TSENINV_ON		\
				| PARAM_TSOUT2_TSSINV_MSB   | PARAM_TSOUT2_TSSINV_LSB )
#define	TS_CONFIG_PATTERN3	( PARAM_PBER_PLAYER_OFF | PARAM_PBER_PLAYER_A | PARAM_PBER_PLAYER_B | PARAM_PBER_PLAYER_C		\
				| PARAM_PBER_PLAYER_AB | PARAM_PBER_PLAYER_AC | PARAM_PBER_PLAYER_BC | PARAM_PBER_PLAYER_ALL )
#define	TS_CONFIG_PATTERN4	( PARAM_SBER_SCLKSEL_OFF | PARAM_SBER_SCLKSEL_ON | PARAM_SBER_SBERSEL_OFF | PARAM_SBER_SBERSEL_ON	\
				| PARAM_SBER_SPACON_OFF | PARAM_SBER_SPACON_ON | PARAM_SBER_SENON_OFF | PARAM_SBER_SENON_ON		\
				| PARAM_SBER_SLAYER_OFF | PARAM_SBER_SLAYER_A | PARAM_SBER_SLAYER_B | PARAM_SBER_SLAYER_C		\
				| PARAM_SBER_SLAYER_AB | PARAM_SBER_SLAYER_AC | PARAM_SBER_SLAYER_BC | PARAM_SBER_SLAYER_ALL )
#define	TS_CONFIG_PATTERN7	( PARAM_SBER2_SCLKSEL_OFF | PARAM_SBER2_SCLKSEL_ON		\
				| PARAM_SBER2_SBERSEL_OFF | PARAM_SBER2_SBERSEL_ON		\
				| PARAM_SBER2_SPACON_OFF | PARAM_SBER2_SPACON_ON		\
				| PARAM_SBER2_SENON_OFF | PARAM_SBER2_SENON_ON )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (TS_user == NULL) {
		rtncode = -EINVAL;
		goto ts_config_return;
	}

	memset(TS, 0, tmpsize);
	if (copy_from_user(TS, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ts_config_return;
	}

	if ((TS->TSOUT & ~TS_CONFIG_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ts_config_return;
	}

	if ((TS->TSOUT2 & ~TS_CONFIG_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto ts_config_return;
	}

	if ((TS->PBER & ~TS_CONFIG_PATTERN3) != 0) {
		rtncode = -EINVAL;
		goto ts_config_return;
	}

	if ((TS->SBER & ~TS_CONFIG_PATTERN4) != 0) {
		rtncode = -EINVAL;
		goto ts_config_return;
	}

	TS_CONFIG_PATTERN5 = TS->PBER2 & 0x07;
	if ((TS_CONFIG_PATTERN5 != PARAM_PBER2_PLAYER_OFF)
	    && (TS_CONFIG_PATTERN5 != PARAM_PBER2_PLAYER_A)) {
		rtncode = -EINVAL;
		goto ts_config_return;
	}

	TS_CONFIG_PATTERN6 = TS->SBER2 & 0xF8;
	if ((TS_CONFIG_PATTERN6 & ~TS_CONFIG_PATTERN7) != 0) {
		rtncode = -EINVAL;
		goto ts_config_return;
	} else {
		TS_CONFIG_PATTERN6 = TS->SBER2 & 0x07;
		if ((TS_CONFIG_PATTERN6 != PARAM_SBER2_SLAYER_OFF)
		    && (TS_CONFIG_PATTERN6 != PARAM_SBER2_SLAYER_A)) {
			rtncode = -EINVAL;
			goto ts_config_return;
		}
	}

	sreg = MB86A35_REG_SUBR_TSOUT;
	value = TS->TSOUT;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_TSOUT, 0xFF);
	if (rtncode != 0) {
		goto ts_config_return;
	}

	sreg = MB86A35_REG_SUBR_TSOUT2;
	value = TS->TSOUT2;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_PBER;
	value = TS->PBER;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_PBER, 0xFF);
	if (rtncode != 0) {
		goto ts_config_return;
	}

	sreg = MB86A35_REG_SUBR_SBER;
	value = TS->SBER;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_PBER2;
	value = TS->PBER2;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_PBER, 0xFF);
	if (rtncode != 0) {
		goto ts_config_return;
	}

	sreg = MB86A35_REG_SUBR_SBER2;
	value = TS->SBER2;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

ts_config_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_TS_PCLOCK command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_TS_PCLOCK(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_ts_t *TS_user = (ioctl_ts_t *) arg;
	ioctl_ts_t *TS = &cmdctrl->TS;
	size_t tmpsize = sizeof(ioctl_ts_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
#define	TS_PCLOCK_PATTERN1	( PARAM_SBER_SCLKSEL_OFF  | PARAM_SBER_SCLKSEL_ON  )
#define	TS_PCLOCK_PATTERN2	( PARAM_SBER2_SCLKSEL_OFF | PARAM_SBER2_SCLKSEL_ON )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (TS_user == NULL) {
		rtncode = -EINVAL;
		goto ts_pclock_return;
	}

	memset(TS, 0, tmpsize);
	if (copy_from_user(TS, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ts_pclock_return;
	}

	if ((TS->SBER & ~TS_PCLOCK_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ts_pclock_return;
	}

	if ((TS->SBER2 & ~TS_PCLOCK_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto ts_pclock_return;
	}

	sreg = MB86A35_REG_SUBR_SBER;
	value = TS->SBER;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_SBER2;
	value = TS->SBER2;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

ts_pclock_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_TS_OUTMASK command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_TS_OUTMASK(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			     unsigned long arg)
{
	ioctl_ts_t *TS_user = (ioctl_ts_t *) arg;
	ioctl_ts_t *TS = &cmdctrl->TS;
	size_t tmpsize = sizeof(ioctl_ts_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
#define	TS_OUTMASK_PATTERN1	( PARAM_TSMASK0_TSFRMMASK_OFF | PARAM_TSMASK0_TSFRMMASK_ON \
				| PARAM_TSMASK0_TSERRMASK_OFF | PARAM_TSMASK0_TSERRMASK_ON \
				| PARAM_TSMASK0_TSPACMASK_OFF | PARAM_TSMASK0_TSPACMASK_ON \
				| PARAM_TSMASK0_TSENMASK_OFF  | PARAM_TSMASK0_TSENMASK_ON  \
				| PARAM_TSMASK0_TSDTMASK_OFF  | PARAM_TSMASK0_TSDTMASK_ON  \
				| PARAM_TSMASK0_TSCLKMASK_OFF | PARAM_TSMASK0_TSCLKMASK_ON )
#define	TS_OUTMASK_PATTERN2	( PARAM_TSMASK1_TSFRMMASK_OFF | PARAM_TSMASK1_TSFRMMASK_ON \
				| PARAM_TSMASK1_TSERRMASK_OFF | PARAM_TSMASK1_TSERRMASK_ON \
				| PARAM_TSMASK1_TSPACMASK_OFF | PARAM_TSMASK1_TSPACMASK_ON \
				| PARAM_TSMASK1_TSENMASK_OFF  | PARAM_TSMASK1_TSENMASK_ON  \
				| PARAM_TSMASK1_TSDTMASK_OFF  | PARAM_TSMASK1_TSDTMASK_ON  \
				| PARAM_TSMASK1_TSCLKMASK_OFF | PARAM_TSMASK1_TSCLKMASK_ON )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (TS_user == NULL) {
		rtncode = -EINVAL;
		goto ts_outmask_return;
	}

	memset(TS, 0, tmpsize);
	if (copy_from_user(TS, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ts_outmask_return;
	}

	if ((TS->TSMASK0 & ~TS_OUTMASK_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ts_outmask_return;
	}

	if ((TS->TSMASK1 & ~TS_OUTMASK_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto ts_outmask_return;
	}

	sreg = MB86A35_REG_SUBR_TSMASK;
	value = TS->TSMASK0;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_TSMASK0, 0xFF);
	if (rtncode != 0) {
		goto ts_outmask_return;
	}

	sreg = MB86A35_REG_SUBR_TSMASK1;
	value = TS->TSMASK1;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_TSMASK1, 0xFF);
	if (rtncode != 0) {
		goto ts_outmask_return;
	}

ts_outmask_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_TS_INVERT command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_TS_INVERT(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_ts_t *TS_user = (ioctl_ts_t *) arg;
	ioctl_ts_t *TS = &cmdctrl->TS;
	size_t tmpsize = sizeof(ioctl_ts_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
#define	TS_INVERT_PATTERN1	( PARAM_TSOUT_TSERRINV_OFF | PARAM_TSOUT_TSERRINV_ON		\
				| PARAM_TSOUT_TSENINV_OFF | PARAM_TSOUT_TSENINV_ON		\
				| PARAM_TSOUT_TSSINV_MSB | PARAM_TSOUT_TSSINV_LSB		\
				| PARAM_TSOUT_TSERRMASK2_OFF | PARAM_TSOUT_TSERRMASK2_ON	\
				| PARAM_TSOUT_TSERRMASK_OFF | PARAM_TSOUT_TSERRMASK_ON )
#define	TS_INVERT_PATTERN2	( PARAM_TSOUT2_TSERRINV_OFF | PARAM_TSOUT2_TSERRINV_ON		\
				| PARAM_TSOUT2_TSENINV_OFF  | PARAM_TSOUT2_TSENINV_ON		\
				| PARAM_TSOUT2_TSSINV_MSB   | PARAM_TSOUT2_TSSINV_LSB )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (TS_user == NULL) {
		rtncode = -EINVAL;
		goto ts_invert_return;
	}

	memset(TS, 0, tmpsize);
	if (copy_from_user(TS, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ts_invert_return;
	}

	if ((TS->TSOUT & ~TS_INVERT_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ts_invert_return;
	}

	if ((TS->TSOUT2 & ~TS_INVERT_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto ts_invert_return;
	}

	sreg = MB86A35_REG_SUBR_TSOUT;
	value = TS->TSOUT;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_TSOUT, 0xFF);
	if (rtncode != 0) {
		goto ts_invert_return;
	}

	sreg = MB86A35_REG_SUBR_TSOUT2;
	value = TS->TSOUT2;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

ts_invert_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_IRQ_GETREASON command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_IRQ_GETREASON(mb86a35_cmdcontrol_t * cmdctrl,
				unsigned int cmd, unsigned long arg)
{
	ioctl_irq_t *IRQ_user = (ioctl_irq_t *) arg;
	ioctl_irq_t *IRQ = &cmdctrl->IRQ;
	size_t tmpsize = sizeof(ioctl_irq_t);
	int rtncode = 0;
	unsigned char reg;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (IRQ_user == NULL) {
		rtncode = -EINVAL;
		goto irq_getreason_return;
	}
	memset(IRQ, 0, tmpsize);

	if (copy_from_user(IRQ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
		PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto irq_getreason_return;
	}


	reg = MB86A35_REG_ADDR_FECIRQ1;
	rtncode = mb86a35_i2c_master_recv(reg, &IRQ->FECIRQ1, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto irq_getreason_return;
	}
	IRQ->FECIRQ1 &= (MB86A35_MASK_FECIRQ1_TSERRC
			 | MB86A35_MASK_FECIRQ1_TSERRB
			 | MB86A35_MASK_FECIRQ1_TSERRA
			 | MB86A35_MASK_FECIRQ1_EMG
			 | MB86A35_MASK_FECIRQ1_CNT | MB86A35_MASK_FECIRQ1_ILL);

	reg = MB86A35_REG_ADDR_FECIRQ2;
	rtncode = mb86a35_i2c_master_recv(reg, &IRQ->FECIRQ2, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto irq_getreason_return;
	}
	IRQ->FECIRQ2 &= (MB86A35_MASK_FECIRQ2_SBERCEFC
			 | MB86A35_MASK_FECIRQ2_SBERCEFB
			 | MB86A35_MASK_FECIRQ2_SBERCEFA
			 | MB86A35_MASK_FECIRQ2_SBERTHFC
			 | MB86A35_MASK_FECIRQ2_SBERTHFB
			 | MB86A35_MASK_FECIRQ2_SBERTHFA);

	reg = MB86A35_REG_ADDR_TUNER_IRQ;
	rtncode = mb86a35_i2c_master_recv(reg, &IRQ->TUNER_IRQ, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto irq_getreason_return;
	}
	IRQ->TUNER_IRQ &= (MB86A35_TUNER_IRQ_GPIO | MB86A35_TUNER_IRQ_CHEND);

	if ((IRQ->FECIRQ1 | IRQ->FECIRQ2 | IRQ->TUNER_IRQ) != 0) {
		IRQ->irq = PARAM_IRQ_YES;
	}

	if (copy_to_user((void *)IRQ_user, (void *)IRQ, tmpsize)) {
		rtncode = -EFAULT;
		goto irq_getreason_return;
	}

irq_getreason_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_IRQ_SETMASK command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_IRQ_SETMASK(mb86a35_cmdcontrol_t * cmdctrl,
			      unsigned int cmd, unsigned long arg)
{
	ioctl_irq_t *IRQ_user = (ioctl_irq_t *) arg;
	ioctl_irq_t *IRQ = &cmdctrl->IRQ;
	size_t tmpsize = sizeof(ioctl_irq_t);
	unsigned char rega = MB86A35_REG_ADDR_TMCC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_TMCC_SUBD;
	unsigned char reg = 0;
	unsigned int value = 0;
	int rtncode = 0;
#define	IRQ_SETMASK_PATTERN1	( PARAM_TMCC_IRQ_MASK_EMG	\
				| PARAM_TMCC_IRQ_MASK_CNTDWON	\
				| PARAM_TMCC_IRQ_MASK_ILL )
#define	IRQ_SETMASK_PATTERN2	( PARAM_SBER_IRQ_MASK_DTERIRQ	\
				| PARAM_SBER_IRQ_MASK_THMASKC | PARAM_SBER_IRQ_MASK_THMASKB | PARAM_SBER_IRQ_MASK_THMASKA	\
				| PARAM_SBER_IRQ_MASK_CEMASKC | PARAM_SBER_IRQ_MASK_CEMASKB | PARAM_SBER_IRQ_MASK_CEMASKA )
#define	IRQ_SETMASK_PATTERN3	( PARAM_TSERR_IRQ_MASK_C | PARAM_TSERR_IRQ_MASK_B | PARAM_TSERR_IRQ_MASK_A )
#define	IRQ_SETMASK_PATTERN4	( PARAM_TMCC_IRQ_RST_EMG	\
				| PARAM_TMCC_IRQ_RST_CNTDWON	\
				| PARAM_TMCC_IRQ_RST_ILL )
#define	IRQ_SETMASK_PATTERN5	( PARAM_RSBERON_RSBERAUTO_M | PARAM_RSBERON_RSBERAUTO_A	\
				| PARAM_RSBERON_RSBERC_S    | PARAM_RSBERON_RSBERC_B	\
				| PARAM_RSBERON_RSBERB_S    | PARAM_RSBERON_RSBERB_B	\
				| PARAM_RSBERON_RSBERA_S    | PARAM_RSBERON_RSBERA_B )
#define	IRQ_SETMASK_PATTERN6	( PARAM_RSBERRST_SBERXRSTC_R | PARAM_RSBERRST_SBERXRSTC_N	\
				| PARAM_RSBERRST_SBERXRSTB_R | PARAM_RSBERRST_SBERXRSTB_N	\
				| PARAM_RSBERRST_SBERXRSTA_R | PARAM_RSBERRST_SBERXRSTA_N )
#define	IRQ_SETMASK_PATTERN7	( PARAM_RSBERCEFLG_SBERCEFC_E | PARAM_RSBERCEFLG_SBERCEFB_E | PARAM_RSBERCEFLG_SBERCEFA_E )
#define	IRQ_SETMASK_PATTERN8	( PARAM_RSBERTHFLG_SBERTHRC_R | PARAM_RSBERTHFLG_SBERTHRB_R | PARAM_RSBERTHFLG_SBERTHRA_R	\
				| PARAM_RSBERTHFLG_SBERTHFC_F | PARAM_RSBERTHFLG_SBERTHFB_F | PARAM_RSBERTHFLG_SBERTHFA_F )
#define	IRQ_SETMASK_PATTERN9	( PARAM_RSERRFLG_RSERRSTC_R | PARAM_RSERRFLG_RSERRSTB_R | PARAM_RSERRFLG_RSERRSTA_R	\
				| PARAM_RSERRFLG_RSERRC_F   | PARAM_RSERRFLG_RSERRB_F   | PARAM_RSERRFLG_RSERRA_F )
#define	IRQ_SETMASK_PATTERN10	( PARAM_FECIRQ1_RSERRC | PARAM_FECIRQ1_RSERRB | PARAM_FECIRQ1_RSERRA	\
				| PARAM_FECIRQ1_LOCK   | PARAM_FECIRQ1_EMG   | PARAM_FECIRQ1_CNT | PARAM_FECIRQ1_ILL )
#define	IRQ_SETMASK_PATTERN11	( PARAM_XIRQINV_LOW | PARAM_XIRQINV_HIGH )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (IRQ_user == NULL) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	memset(IRQ, 0, tmpsize);
	if (copy_from_user(IRQ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto irq_setmask_return;
	}

	if ((IRQ->TMCC_IRQ_MASK & ~IRQ_SETMASK_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->SBER_IRQ_MASK & ~IRQ_SETMASK_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->TSERR_IRQ_MASK & ~IRQ_SETMASK_PATTERN3) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->TMCC_IRQ_RST & ~IRQ_SETMASK_PATTERN4) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->RSBERON & ~IRQ_SETMASK_PATTERN5) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->RSBERRST & ~IRQ_SETMASK_PATTERN6) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->RSBERCEFLG & ~IRQ_SETMASK_PATTERN7) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->RSBERTHFLG & ~IRQ_SETMASK_PATTERN8) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->RSERRFLG & ~IRQ_SETMASK_PATTERN9) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->FECIRQ1 & ~IRQ_SETMASK_PATTERN10) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	if ((IRQ->XIRQINV & ~IRQ_SETMASK_PATTERN11) != 0) {
		rtncode = -EINVAL;
		goto irq_setmask_return;
	}

	sreg = MB86A35_REG_SUBR_TMCC_IRQ_MASK;
	value = IRQ->TMCC_IRQ_MASK;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_TMCC_IRQ_MASK, 0xFF);
	if (rtncode != 0) {
		goto irq_setmask_return;
	}

	sreg = MB86A35_REG_SUBR_SBER_IRQ_MASK;
	value = IRQ->SBER_IRQ_MASK;
	rtncode =
	    mb86a35_i2c_slave_send_mask(MB86A35_REG_ADDR_FEC_SUBA, sreg, MB86A35_REG_ADDR_FEC_SUBD, value,
					MB86A35_I2CMASK_SBER_IRQ_MASK, 0xFF);
	if (rtncode != 0) {
		goto irq_setmask_return;
	}

	sreg = MB86A35_REG_SUBR_TSERR_IRQ_MASK;
	value = IRQ->TSERR_IRQ_MASK;
	rtncode =
	    mb86a35_i2c_slave_send_mask(MB86A35_REG_ADDR_FEC_SUBA, sreg, MB86A35_REG_ADDR_FEC_SUBD, value,
					MB86A35_I2CMASK_TSERR_IRQ_MASK, 0xFF);
	if (rtncode != 0) {
		goto irq_setmask_return;
	}

	sreg = MB86A35_REG_SUBR_TMCC_IRQ_RST;
	value = IRQ->TMCC_IRQ_RST;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_TMCC_IRQ_RST, 0xFF);
	if (rtncode != 0) {
		goto irq_setmask_return;
	}

	reg = MB86A35_REG_ADDR_RSBERON;
	value = IRQ->RSBERON;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_RSBERRST;
	value = IRQ->RSBERRST;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_RSBERTHFLG;
	value = IRQ->RSBERTHFLG;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_RSERRFLG;
	value = IRQ->RSERRFLG;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_FECIRQ1;
	value = IRQ->FECIRQ1;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_XIRQINV;
	value = IRQ->XIRQINV;
	mb86a35_i2c_master_send(reg, value);

irq_setmask_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_IRQ_TMCCPARAM_SET command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_IRQ_TMCCPARAM_SET(mb86a35_cmdcontrol_t * cmdctrl,
				    unsigned int cmd, unsigned long arg)
{
	ioctl_irq_t *IRQ_user = (ioctl_irq_t *) arg;
	ioctl_irq_t *IRQ = &cmdctrl->IRQ;
	size_t tmpsize = sizeof(ioctl_irq_t);
	unsigned char rega = MB86A35_REG_ADDR_TMCC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_TMCC_SUBD;
	unsigned int value = 0;
	int rtncode = 0;
#define	IRQ_TMCCPARAM_PATTERN1	( PARAM_TMCCCHK_14 | PARAM_TMCCCHK_13 | PARAM_TMCCCHK_12	\
				| PARAM_TMCCCHK_11 | PARAM_TMCCCHK_10 | PARAM_TMCCCHK_9 | PARAM_TMCCCHK_8 )
#define	IRQ_TMCCPARAM_PATTERN2	( PARAM_EMG_INV_RECV | PARAM_EMG_INV_NORECV )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (IRQ_user == NULL) {
		rtncode = -EINVAL;
		goto irq_tmccparam_set_return;
	}

	memset(IRQ, 0, tmpsize);
	if (copy_from_user(IRQ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto irq_tmccparam_set_return;
	}

	if ((IRQ->TMCCCHK_HI & ~IRQ_TMCCPARAM_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto irq_tmccparam_set_return;
	}

	if ((IRQ->TMCCCHK2_HI & ~IRQ_TMCCPARAM_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto irq_tmccparam_set_return;
	}

	if ((IRQ->EMG_INV & ~IRQ_TMCCPARAM_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto irq_tmccparam_set_return;
	}

	sreg = MB86A35_REG_SUBR_TMCCCHK_HI;
	value = IRQ->TMCCCHK_HI;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_TMCCCHK_LO;
	value = IRQ->TMCCCHK_LO;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_TMCCCHK2_HI;
	value = IRQ->TMCCCHK2_HI;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_TMCCCHK2_LO;
	value = IRQ->TMCCCHK2_LO;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_TMCC_IRQ_EMG_INV;
	value = IRQ->EMG_INV;
	rtncode =
	    mb86a35_i2c_slave_send_mask(rega, sreg, regd, value,
					MB86A35_I2CMASK_EMG_INV,
					PARAM_EMG_INV_NORECV);
	if (rtncode != 0) {
		goto irq_tmccparam_set_return;
	}

irq_tmccparam_set_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_IRQ_TMCCPARAM_REASON command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_IRQ_TMCCPARAM_REASON(mb86a35_cmdcontrol_t * cmdctrl,
				       unsigned int cmd, unsigned long arg)
{
	ioctl_irq_t *IRQ_user = (ioctl_irq_t *) arg;
	ioctl_irq_t *IRQ = &cmdctrl->IRQ;
	unsigned char rega = MB86A35_REG_ADDR_TMCC_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_TMCC_SUBD;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (IRQ_user == NULL) {
		rtncode = -EINVAL;
		goto irq_tmccparam_reason_return;
	}

	sreg = MB86A35_REG_SUBR_PCHKOUT0;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &IRQ->PCHKOUT0, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto irq_tmccparam_reason_return;
	}
	IRQ->PCHKOUT0 &= IRQ_TMCCPARAM_PATTERN1;

	sreg = MB86A35_REG_SUBR_PCHKOUT1;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &IRQ->PCHKOUT1, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto irq_tmccparam_reason_return;
	}

	sreg = MB86A35_REG_SUBR_TMCC_IRQ_EMG_INV;
	rtncode = mb86a35_i2c_slave_recv(rega, sreg, regd, &IRQ->EMG_INV, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto irq_tmccparam_reason_return;
	}
	IRQ->EMG_INV &= PARAM_EMG_INV_NORECV;

	if (copy_to_user
	    ((void *)&IRQ_user->PCHKOUT0, (void *)&IRQ->PCHKOUT0, 3)) {
		rtncode = -EFAULT;
		goto irq_tmccparam_reason_return;
	}

irq_tmccparam_reason_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_CN_MONI_CONFIG_START command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_CN_MONI_CONFIG_START(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			  unsigned long arg)
{
	ioctl_cn_moni_t *CN_user = (ioctl_cn_moni_t *) arg;
	ioctl_cn_moni_t *CN = &cmdctrl->CN;
	size_t tmpsize = sizeof(ioctl_cn_moni_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned int value = 0;
	//int indx = 0;
	int mode = 0;
	int loop = 0;
	//u8 flag = 0;
	u8 cncnt = 0;
	//u8 cndata;
	int wait_time = 0;
	//u16 tmpdata = 0;
	//u8 sync_state = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (CN_user == NULL) {
		rtncode = -EINVAL;
		goto cn_moni_return;
	}

	memset(CN, 0, tmpsize);
	if (copy_from_user(CN, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto cn_moni_return;
	}

	if (CN->CNCNT > 15) {
		rtncode = -EINVAL;
		goto cn_moni_return;
	}

	if ((CN->CNCNT2 & ~PARAM_CNCNT2_MODE_MUNL) != 0) {
		rtncode = -EINVAL;
		goto cn_moni_return;
	}

	if ((CN->CNCNT & ~MB86A35_MASK_CNCNT_SYMCOUNT) != 0) {
		rtncode = -EINVAL;
		goto cn_moni_return;
	}

	loop = CN->cn_count;
	mode = CN->CNCNT2 & PARAM_CNCNT2_MODE_MUNL;

	cncnt = (CN->CNCNT & MB86A35_MASK_CNCNT_SYMCOUNT);
	CN->CNCNT = cncnt;
	wait_time = MB86A35_CN_MONI_WAITTIME(cncnt);

	reg = MB86A35_REG_ADDR_CNCNT2;
	value = mode;
	mb86a35_i2c_master_send(reg, value);

	reg = MB86A35_REG_ADDR_CNCNT;
	value = CN->CNCNT;
	mb86a35_i2c_master_send(reg, value);

	/* RST = 1 */
	reg = MB86A35_REG_ADDR_CNCNT;
	value = (CN->CNCNT | MB86A35_CNCNT_RST);
	mb86a35_i2c_master_send(reg, value);

	/* RST = 0 */
	reg = MB86A35_REG_ADDR_CNCNT;
	value = cncnt;
	mb86a35_i2c_master_send(reg, value);

	//for (indx = 0; indx < loop; indx++) {
	//	if (mode == PARAM_CNCNT2_MODE_AUTO) {
			/* PARAM_CNCNT2_MODE_AUTO */
			/*** WAIT ***/
			//mdelay(wait_time);

			//reg = MB86A35_REG_ADDR_CNCNT;
			//value = (CN->CNCNT | MB86A35_CNCNT_LOCK);
			//mb86a35_i2c_master_send(reg, value);
			//reg = MB86A35_REG_ADDR_CNDATHI; /* 20110404 MB86A35 ES2.5 */
			//mb86a35_i2c_master_send(reg, 0);
	//	} else {
			/* PARAM_CNCNT2_MODE_MUNL */
	//		reg = MB86A35_REG_ADDR_CNCNT;
	//		while ((flag & MB86A35_CNCNT_FLG) != MB86A35_CNCNT_FLG) {
	//			rtncode =
	//			    mb86a35_i2c_master_recv(reg, &flag, 1);
	//			if (rtncode != 0) {
	//				rtncode = -EFAULT;
	//				goto cn_moni_return;
	//			}
	//			rtncode =
	//			    mb86a35_i2c_master_recv(MB86A35_REG_ADDR_SYNC_STATE, &sync_state, 1);
			
	//			if (rtncode != 0) {
	//				rtncode = -EFAULT;
	//				goto cn_moni_return;
	//			}
			
	//			if (sync_state < PARAM_SYNC_STATE_FRMSYNC){
	//				rtncode = -EFAULT;
	//				goto cn_moni_return;
	//			}
			
	//		}
	//	}
	//	tmpdata = 0;
	//	reg = MB86A35_REG_ADDR_CNDATHI;
	//	rtncode = mb86a35_i2c_master_recv(reg, &cndata, 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto cn_moni_return;
	//	}
	//	tmpdata = (cndata << 8);
	//	reg = MB86A35_REG_ADDR_CNDATLO;
	//	rtncode = mb86a35_i2c_master_recv(reg, &cndata, 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto cn_moni_return;
	//	}
	//	tmpdata += cndata;
	//	if (mode == PARAM_CNCNT2_MODE_AUTO) {
			/* PARAM_CNCNT2_MODE_AUTO */
	//		reg = MB86A35_REG_ADDR_CNCNT;
	//		value = cncnt;
	//		mb86a35_i2c_master_send(reg, value);
	//	}
	//	if (copy_to_user
	//	    ((void *)&CN_user->CNDATA[indx], (void *)&tmpdata, 2)) {
	//		rtncode = -EFAULT;
	//		goto cn_moni_return;
	//	}
	//	if (mode == PARAM_CNCNT2_MODE_MUNL) {
	//		/* PARAM_CNCNT2_MODE_MUNL */
	//		/* RST = 1 */
	//		reg = MB86A35_REG_ADDR_CNCNT;
	//		value = (cncnt | MB86A35_CNCNT_RST);
	//		mb86a35_i2c_master_send(reg, value);

			/* RST = 0 */
	//		reg = MB86A35_REG_ADDR_CNCNT;
	//		value = cncnt;
	//		mb86a35_i2c_master_send(reg, value);
	//	}
	//}

cn_moni_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_CN_MONI_GET command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_CN_MONI_GET(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			  unsigned long arg)
{
	ioctl_cn_moni_t *CN_user = (ioctl_cn_moni_t *) arg;
	ioctl_cn_moni_t *CN = &cmdctrl->CN;
	size_t tmpsize = sizeof(ioctl_cn_moni_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned int value = 0;
	int indx = 0;
	int mode = 0;
	int loop = 0;
	u8 flag = 0;
	u8 cncnt = 0;
	u8 cndata;
	//int wait_time = 0;
	u16 tmpdata = 0;
	//u8 sync_state = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (CN_user == NULL) {
		rtncode = -EINVAL;
		goto cn_moni_return;
	}

	memset(CN, 0, tmpsize);
	if (copy_from_user(CN, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto cn_moni_return;
	}

	//if (CN->CNCNT > 15) {
	//	rtncode = -EINVAL;
	//	goto cn_moni_return;
	//}

	//if ((CN->CNCNT2 & ~PARAM_CNCNT2_MODE_MUNL) != 0) {
	//	rtncode = -EINVAL;
	//	goto cn_moni_return;
	//}

	//if ((CN->CNCNT & ~MB86A35_MASK_CNCNT_SYMCOUNT) != 0) {
	//	rtncode = -EINVAL;
	//	goto cn_moni_return;
	//}

	loop = CN->cn_count;
	mode = CN->CNCNT2 & PARAM_CNCNT2_MODE_MUNL;

	cncnt = (CN->CNCNT & MB86A35_MASK_CNCNT_SYMCOUNT);
	//CN->CNCNT = cncnt;
	//wait_time = MB86A35_CN_MONI_WAITTIME(cncnt);

	//reg = MB86A35_REG_ADDR_CNCNT2;
	//value = mode;
	//mb86a35_i2c_master_send(reg, value);

	//reg = MB86A35_REG_ADDR_CNCNT;
	//value = CN->CNCNT;
	//mb86a35_i2c_master_send(reg, value);

	/* RST = 1 */
	//reg = MB86A35_REG_ADDR_CNCNT;
	//value = (CN->CNCNT | MB86A35_CNCNT_RST);
	//mb86a35_i2c_master_send(reg, value);

	/* RST = 0 */
	//reg = MB86A35_REG_ADDR_CNCNT;
	//value = cncnt;
	//mb86a35_i2c_master_send(reg, value);

	for (indx = 0; indx < loop; indx++) {
		if (mode == PARAM_CNCNT2_MODE_AUTO) {
			/* PARAM_CNCNT2_MODE_AUTO */
			/*** WAIT ***/
			//mdelay(wait_time);

			//reg = MB86A35_REG_ADDR_CNCNT;
			//value = (CN->CNCNT | MB86A35_CNCNT_LOCK);
			//mb86a35_i2c_master_send(reg, value);
			reg = MB86A35_REG_ADDR_CNDATHI; /* 20110404 MB86A35 ES2.5 */
			mb86a35_i2c_master_send(reg, 0);
		} else {
			/* PARAM_CNCNT2_MODE_MUNL */
			reg = MB86A35_REG_ADDR_CNCNT;
#if 0
		/*	while ((flag & MB86A35_CNCNT_FLG) != MB86A35_CNCNT_FLG) {
				rtncode =
				    mb86a35_i2c_master_recv(reg, &flag, 1);
				if (rtncode != 0) {
					rtncode = -EFAULT;
					goto cn_moni_return;
				}
				rtncode =
				    mb86a35_i2c_master_recv(MB86A35_REG_ADDR_SYNC_STATE, &sync_state, 1);
			
				if (rtncode != 0) {
					rtncode = -EFAULT;
					goto cn_moni_return;
				}
			
				if (sync_state < PARAM_SYNC_STATE_FRMSYNC){
					rtncode = -EFAULT;
					goto cn_moni_return;
				}
			
			}*/
#endif
			rtncode =
			    mb86a35_i2c_master_recv(reg, &flag, 1);
			if (rtncode != 0) {
				rtncode = -EFAULT;
				goto cn_moni_return;
			}

			if ((flag & MB86A35_CNCNT_FLG) != MB86A35_CNCNT_FLG) {
				goto cn_moni_return;
			}

		}
		tmpdata = 0;
		reg = MB86A35_REG_ADDR_CNDATHI;
		rtncode = mb86a35_i2c_master_recv(reg, &cndata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto cn_moni_return;
		}
		tmpdata = (cndata << 8);
		reg = MB86A35_REG_ADDR_CNDATLO;
		rtncode = mb86a35_i2c_master_recv(reg, &cndata, 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto cn_moni_return;
		}
		tmpdata += cndata;
		if (mode == PARAM_CNCNT2_MODE_AUTO) {
			/* PARAM_CNCNT2_MODE_AUTO */
			//reg = MB86A35_REG_ADDR_CNCNT;
			//value = cncnt;
			//mb86a35_i2c_master_send(reg, value);
		}
		if (copy_to_user
		    ((void *)&CN_user->CNDATA[indx], (void *)&tmpdata, 2)) {
			rtncode = -EFAULT;
			goto cn_moni_return;
		}
		if (mode == PARAM_CNCNT2_MODE_MUNL) {
			/* PARAM_CNCNT2_MODE_MUNL */
			/* RST = 1 */
			reg = MB86A35_REG_ADDR_CNCNT;
			value = (cncnt | MB86A35_CNCNT_RST);
			mb86a35_i2c_master_send(reg, value);

			/* RST = 0 */
			reg = MB86A35_REG_ADDR_CNCNT;
			value = cncnt;
			mb86a35_i2c_master_send(reg, value);
		}
	}

cn_moni_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_MER_MONI_CONFIG_START command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_MER_MONI_CONFIG_START(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	ioctl_mer_moni_t *MER_user = (ioctl_mer_moni_t *) arg;
	ioctl_mer_moni_t *MER = &cmdctrl->MER;
	size_t tmpsize = sizeof(ioctl_mer_moni_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg = 0;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
	//int indx = 0;
	int mode = 0;
	int loop = 0;
	int wait_time = 0;
	//u8 flag = 0;
	u8 mercnt = 0;
	//u8 merdataA[4];
	//u8 merdataB[4];
	//u8 merdataC[4];
	//struct mer_data MERDATA;
	//u8 sync_state = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (MER_user == NULL) {
		rtncode = -EINVAL;
		goto mer_moni_return;
	}

	memset(MER, 0, tmpsize);
	if (copy_from_user(MER, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto mer_moni_return;
	}

	if (MER->MERSTEP > 7) {
		rtncode = -EINVAL;
		goto mer_moni_return;
	}

	loop = MER->mer_count;
	mode = MER->MERCTRL & PARAM_MERCTRL_MODE_MUNL;

	mercnt = (MER->MERSTEP & MB86A35_MASK_MERSTEP_SYMCOUNT);
	wait_time = MB86A35_MER_MONI_WAITTIME(mercnt);

	sreg = MB86A35_REG_SUBR_MERCTRL;
	value = mode;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	sreg = MB86A35_REG_SUBR_MERSTEP;
	value = mercnt;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	/* RST = 1 */
	sreg = MB86A35_REG_SUBR_MERCTRL;
	value = (mode | MB86A35_MERCTRL_RST);
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	/* RST = 0 */
	sreg = MB86A35_REG_SUBR_MERCTRL;
	value = mode;
	mb86a35_i2c_slave_send(rega, sreg, regd, value);

	//MER->mer_flag = 1;

	//if (copy_to_user((void *)MER_user, (void *)MER, tmpsize)) {
	//	rtncode = -EFAULT;
	//	goto mer_moni_return;
	//}
	//for (indx = 0; indx < loop; indx++) {
	//	if (mode == PARAM_MERCTRL_MODE_AUTO) {
			/* PARAM_MERCTRL_MODE_AUTO */

			/*** WAIT ***/
	//		mdelay(wait_time);

	//		sreg = MB86A35_REG_SUBR_MERCTRL;
	//		value = MB86A35_MERCTRL_LOCK;
	//		mb86a35_i2c_slave_send(rega, sreg, regd, value);
	//	} else {
			/* PARAM_MERCTRL_MODE_MUNL */
	//		flag = 0;
	//		sreg = MB86A35_REG_SUBR_MEREND;
	//		while ((flag & MB86A35_MEREND_FLG) !=
	//		       MB86A35_MEREND_FLG) {
	//			rtncode =
	//			    mb86a35_i2c_slave_recv(rega, sreg, regd,
	//						   &flag, 1);
	//			if (rtncode != 0) {
	//				rtncode = -EFAULT;
	//				goto mer_moni_return;
	//			}
			
	//			rtncode =
	//			    mb86a35_i2c_master_recv(MB86A35_REG_ADDR_SYNC_STATE, &sync_state, 1);
			
	//			if (rtncode != 0) {
	//				rtncode = -EFAULT;
	//				goto mer_moni_return;
	//			}
			
	//			if (sync_state < PARAM_SYNC_STATE_TMCCERRC){
	//				rtncode = -EFAULT;
	//				goto mer_moni_return;
	//			}			
			
	//		}
	//	}
	//	sreg = MB86A35_REG_SUBR_MERA0;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataA[1], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	sreg = MB86A35_REG_SUBR_MERA1;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataA[2], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	sreg = MB86A35_REG_SUBR_MERA2;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataA[3], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	MERDATA.A =
	//	    ((merdataA[1] << 16) + (merdataA[2] << 8) + merdataA[3]);

	//	sreg = MB86A35_REG_SUBR_MERB0;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataB[1], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	sreg = MB86A35_REG_SUBR_MERB1;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataB[2], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	sreg = MB86A35_REG_SUBR_MERB2;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataB[3], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	MERDATA.B =
	//	    ((merdataB[1] << 16) + (merdataB[2] << 8) + merdataB[3]);

	//	sreg = MB86A35_REG_SUBR_MERC0;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataC[1], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	sreg = MB86A35_REG_SUBR_MERC1;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataC[2], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	sreg = MB86A35_REG_SUBR_MERC2;
	//	rtncode =
	//	    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataC[3], 1);
	//	if (rtncode != 0) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//	MERDATA.C =
	//	    ((merdataC[1] << 16) + (merdataC[2] << 8) + merdataC[3]);

	//	if (mode == PARAM_MERCTRL_MODE_AUTO) {
			/* PARAM_MERCTRL_MODE_AUTO */
	//		sreg = MB86A35_REG_SUBR_MERCTRL;
	//		value = 0;
	//		mb86a35_i2c_slave_send(rega, sreg, regd, value);
	//	}

	//	if (copy_to_user
	//	    ((void *)&MER_user->MER[indx], (void *)&MERDATA,
	//	     sizeof(struct mer_data))) {
	//		rtncode = -EFAULT;
	//		goto mer_moni_return;
	//	}
	//}

mer_moni_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_MER_MONI_GET command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_MER_MONI_GET(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	ioctl_mer_moni_t *MER_user = (ioctl_mer_moni_t *) arg;
	ioctl_mer_moni_t *MER = &cmdctrl->MER;
	size_t tmpsize = sizeof(ioctl_mer_moni_t);
	int rtncode = 0;
	unsigned char rega = MB86A35_REG_ADDR_FEC_SUBA;
	unsigned char sreg = 0;
	unsigned char regd = MB86A35_REG_ADDR_FEC_SUBD;
	unsigned int value = 0;
	int indx = 0;
	int mode = 0;
	int loop = 0;
	//int wait_time = 0;
	u8 flag = 0;
	//u8 mercnt = 0;
	u8 merdataA[4];
	u8 merdataB[4];
	u8 merdataC[4];
	struct mer_data MERDATA;
	//u8 sync_state = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (MER_user == NULL) {
		rtncode = -EINVAL;
		goto mer_moni_return;
	}

	memset(MER, 0, tmpsize);
	if (copy_from_user(MER, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto mer_moni_return;
	}

	if (MER->MERSTEP > 7) {
		rtncode = -EINVAL;
		goto mer_moni_return;
	}

	loop = MER->mer_count;
	mode = MER->MERCTRL & PARAM_MERCTRL_MODE_MUNL;

	//mercnt = (MER->MERSTEP & MB86A35_MASK_MERSTEP_SYMCOUNT);
	//wait_time = MB86A35_MER_MONI_WAITTIME(mercnt);

	//sreg = MB86A35_REG_SUBR_MERCTRL;
	//value = mode;
	//mb86a35_i2c_slave_send(rega, sreg, regd, value);

	//sreg = MB86A35_REG_SUBR_MERSTEP;
	//value = mercnt;
	//mb86a35_i2c_slave_send(rega, sreg, regd, value);

	/* RST = 1 */
	//sreg = MB86A35_REG_SUBR_MERCTRL;
	//value = (mode | MB86A35_MERCTRL_RST);
	//mb86a35_i2c_slave_send(rega, sreg, regd, value);

	/* RST = 0 */
	//sreg = MB86A35_REG_SUBR_MERCTRL;
	//value = mode;
	//mb86a35_i2c_slave_send(rega, sreg, regd, value);

	//MER->mer_flag = 1;

	//if (copy_to_user((void *)MER_user, (void *)MER, tmpsize)) {
	//	rtncode = -EFAULT;
	//	goto mer_moni_return;
	//}
	for (indx = 0; indx < loop; indx++) {
		if (mode == PARAM_MERCTRL_MODE_AUTO) {
			/* PARAM_MERCTRL_MODE_AUTO */

			/*** WAIT ***/
			//mdelay(wait_time);

			//sreg = MB86A35_REG_SUBR_MERCTRL;
			//value = MB86A35_MERCTRL_LOCK;
			//mb86a35_i2c_slave_send(rega, sreg, regd, value);
		} else {
			/* PARAM_MERCTRL_MODE_MUNL */
			flag = 0;
			sreg = MB86A35_REG_SUBR_MEREND;
#if 0
			/*while ((flag & MB86A35_MEREND_FLG) !=
			       MB86A35_MEREND_FLG) {
				rtncode =
				    mb86a35_i2c_slave_recv(rega, sreg, regd,
							   &flag, 1);
				if (rtncode != 0) {
					rtncode = -EFAULT;
					goto mer_moni_return;
				}
			
				rtncode =
				    mb86a35_i2c_master_recv(MB86A35_REG_ADDR_SYNC_STATE, &sync_state, 1);
			
				if (rtncode != 0) {
					rtncode = -EFAULT;
					goto mer_moni_return;
				}
			
				if (sync_state < PARAM_SYNC_STATE_TMCCERRC){
					rtncode = -EFAULT;
					goto mer_moni_return;
				}			
			
			}*/
#endif
			rtncode =
			    mb86a35_i2c_slave_recv(rega, sreg, regd,
							   &flag, 1);
				if (rtncode != 0) {
					rtncode = -EFAULT;
					goto mer_moni_return;
				}
			

			if ((flag & MB86A35_MEREND_FLG) != MB86A35_MEREND_FLG) {
				goto mer_moni_return;
			}

		}
		sreg = MB86A35_REG_SUBR_MERA0;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataA[1], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		sreg = MB86A35_REG_SUBR_MERA1;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataA[2], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		sreg = MB86A35_REG_SUBR_MERA2;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataA[3], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		MERDATA.A =
		    ((merdataA[1] << 16) + (merdataA[2] << 8) + merdataA[3]);

		sreg = MB86A35_REG_SUBR_MERB0;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataB[1], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		sreg = MB86A35_REG_SUBR_MERB1;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataB[2], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		sreg = MB86A35_REG_SUBR_MERB2;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataB[3], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		MERDATA.B =
		    ((merdataB[1] << 16) + (merdataB[2] << 8) + merdataB[3]);

		sreg = MB86A35_REG_SUBR_MERC0;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataC[1], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		sreg = MB86A35_REG_SUBR_MERC1;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataC[2], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		sreg = MB86A35_REG_SUBR_MERC2;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &merdataC[3], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
		MERDATA.C =
		    ((merdataC[1] << 16) + (merdataC[2] << 8) + merdataC[3]);

		if (mode == PARAM_MERCTRL_MODE_AUTO) {
			/* PARAM_MERCTRL_MODE_AUTO */
			//sreg = MB86A35_REG_SUBR_MERCTRL;
			//value = 0;
			//mb86a35_i2c_slave_send(rega, sreg, regd, value);
		}else{	//Manual mode
			/* RST = 1 */
			sreg = MB86A35_REG_SUBR_MERCTRL;
			value = (mode | MB86A35_MERCTRL_RST);
			mb86a35_i2c_slave_send(rega, sreg, regd, value);

			/* RST = 0 */
			sreg = MB86A35_REG_SUBR_MERCTRL;
			value = mode;
			mb86a35_i2c_slave_send(rega, sreg, regd, value);
		}

		if (copy_to_user
		    ((void *)&MER_user->MER[indx], (void *)&MERDATA,
		     sizeof(struct mer_data))) {
			rtncode = -EFAULT;
			goto mer_moni_return;
		}
	}

mer_moni_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_CH_SEARCH command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_CH_SEARCH(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	ioctl_ch_search_t *CHSRH_user = (ioctl_ch_search_t *) arg;
	ioctl_ch_search_t *CHSRH = &cmdctrl->CHSRH;
	size_t tmpsize = sizeof(ioctl_ch_search_t);
	int rtncode = 0;
	unsigned char reg;
	unsigned char rega = MB86A35_REG_ADDR_SEARCH_SUBA;
	unsigned char sreg;
	unsigned char regd = MB86A35_REG_ADDR_SEARCH_SUBD;
	unsigned int value = 0;
	u8 irq = 0;
	u8 SEARCH_END_FLAG = 0;
#define	CH_SEARCH_PATTERN1	( PARAM_TUNER_IRQCTL_GPIO_RST | PARAM_TUNER_IRQCTL_CHEND_RST | PARAM_TUNER_IRQCTL_GPIOMASK | PARAM_TUNER_IRQCTL_CHENDMASK )
#define	CH_SEARCH_PATTERN2	( PARAM_SEARCH_CTRL_SEARCH_AFT_B | PARAM_SEARCH_CTRL_SEARCH_AFT_N | PARAM_SEARCH_CTRL_SEARCH_RST | PARAM_SEARCH_CTRL_SEARCH )

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (CHSRH_user == NULL) {
		rtncode = -EINVAL;
		goto ch_search_return;
	}

	memset(CHSRH, 0, tmpsize);
	if (copy_from_user(CHSRH, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ch_search_return;
	}

	printk("mb86a35_IOCTL_CH_SEARCH channel = %d \n", CHSRH->SEARCH_CHANNEL);

	switch (CHSRH->mode) {
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
//		if ((CHSRH->SEARCH_CHANNEL < 1) & (CHSRH->SEARCH_CHANNEL > 33)) {
		if ((CHSRH->SEARCH_CHANNEL < 13) || (CHSRH->SEARCH_CHANNEL > 62)) {
			rtncode = -EINVAL;
			goto ch_search_return;
		}
		break;

	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
//		if ((CHSRH->SEARCH_CHANNEL < 13) & (CHSRH->SEARCH_CHANNEL > 52)) {
//		if ((CHSRH->SEARCH_CHANNEL < 13) & (CHSRH->SEARCH_CHANNEL > 62)) {
		if ((CHSRH->SEARCH_CHANNEL < 1) || (CHSRH->SEARCH_CHANNEL > 33)) {
			rtncode = -EINVAL;
			goto ch_search_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto ch_search_return;
		break;
	}

	if ((CHSRH->TUNER_IRQCTL & ~CH_SEARCH_PATTERN1) != 0) {
		rtncode = -EINVAL;
		goto ch_search_return;
	}

	if ((CHSRH->SEARCH_CTRL & ~CH_SEARCH_PATTERN2) != 0) {
		rtncode = -EINVAL;
		goto ch_search_return;
	}

	/* IRQ get */
	irq = 0;
	reg = MB86A35_REG_ADDR_TUNER_IRQ;
	rtncode = mb86a35_i2c_master_recv(reg, &irq, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto ch_search_return;
	}

	switch (CHSRH->search_flag) {
	case 0:
		/* SEARCH CHEND wait */
		printk("SEARCH CHEND wait In \n");
		sreg = MB86A35_REG_SUBR_SEARCH_END;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &SEARCH_END_FLAG,
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		while (SEARCH_END_FLAG != 0) {
			/* STOP search */
			reg = MB86A35_REG_ADDR_SEARCH_CTRL;
			value = 0;
			mb86a35_i2c_master_send(reg, value);

			/* WAIT */
			//      msleep( 100 );
			printk("SEARCH CHEND wait while \n");

			sreg = MB86A35_REG_SUBR_SEARCH_END;
			rtncode =
			    mb86a35_i2c_slave_recv(rega, sreg, regd,
						   &SEARCH_END_FLAG, 1);
			if (rtncode != 0) {
				rtncode = -EFAULT;
				goto ch_search_return;
			}
		}

		/* initialize */
		reg = MB86A35_REG_ADDR_TUNER_IRQCTL;
		value = CHSRH->TUNER_IRQCTL;
		if ((irq & MB86A35_TUNER_IRQ_GPIO) == MB86A35_TUNER_IRQ_GPIO) {
			value |= MB86A35_TUNER_IRQCTL_GPIO_RST;
		}
		if ((irq & MB86A35_TUNER_IRQ_CHEND) == MB86A35_TUNER_IRQ_CHEND) {
			value |= MB86A35_TUNER_IRQCTL_CHEND_RST;
		}
		value |= MB86A35_TUNER_IRQCTL_GPIOMASK;
		mb86a35_i2c_master_send(reg, value);

		reg = MB86A35_REG_ADDR_SEARCH_CHANNEL;
		value = CHSRH->SEARCH_CHANNEL;
		mb86a35_i2c_master_send(reg, value);

		/*** RF-IC Setting ***/
		rtncode =
		    mb86a35_RF_channel(cmdctrl, CHSRH->mode,
				       CHSRH->SEARCH_CHANNEL);
		if (rtncode != 0)
			goto ch_search_return;
		/*********************/

		reg = MB86A35_REG_ADDR_CONT_CTRL;
		value = 0;
		mb86a35_i2c_master_send(reg, value);

		/* SEARCH start */
		reg = MB86A35_REG_ADDR_SEARCH_CTRL;
		value = 0;
		mb86a35_i2c_master_send(reg, value);

		value = MB86A35_SEARCH_CTRL_SEARCH;
		mb86a35_i2c_master_send(reg, value);
		/****************/

		CHSRH->search_flag = 1;

		printk("SEARCH CHEND wait Out \n");
		break;

	case 1:
		/* SEARCH CHEND wait */
		sreg = MB86A35_REG_SUBR_SEARCH_END;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &SEARCH_END_FLAG,
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		CHSRH->SEARCH_END = SEARCH_END_FLAG;
		if (SEARCH_END_FLAG == 0)
			break;

		printk("SEARCH CHEND complete \n");

		reg = MB86A35_REG_ADDR_TUNER_IRQCTL;
		value =
		    MB86A35_TUNER_IRQCTL_CHEND_RST |
		    MB86A35_TUNER_IRQCTL_GPIOMASK;
		mb86a35_i2c_master_send(reg, value);

		value = MB86A35_TUNER_IRQCTL_GPIOMASK;
		mb86a35_i2c_master_send(reg, value);

		/* STOP search */
		reg = MB86A35_REG_ADDR_SEARCH_CTRL;
		value = 0;
		mb86a35_i2c_master_send(reg, value);

		/* WAIT */
		//                       

		sreg = MB86A35_REG_SUBR_CHANNEL0;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[0],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL1;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[1],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL2;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[2],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL3;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[3],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL4;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[4],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL5;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[5],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL6;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[6],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL7;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[7],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		sreg = MB86A35_REG_SUBR_CHANNEL8;
		rtncode =
		    mb86a35_i2c_slave_recv(rega, sreg, regd, &CHSRH->CHANNEL[8],
					   1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto ch_search_return;
		}
		CHSRH->SEARCH_END = SEARCH_END_FLAG;

		printk("SEARCH CHEND complete END \n");

		break;
	}
	/* channel information setting */
	if (copy_to_user((void *)CHSRH_user, (void *)CHSRH, tmpsize)) {
		rtncode = -EFAULT;
		goto ch_search_return;
	}

ch_search_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RF_INIT command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_INIT(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			  unsigned long arg)
{
  int rtncode_rf = 0;
  unsigned char reg_rf = 0;
  u8 chipid0_rf = 0;
  int rtncode = 0;
   rtncode_rf = mb86a35_i2c_rf_recv(reg_rf, &chipid0_rf, 1);
   if (rtncode_rf != 0) {
    		rtncode = -EFAULT;
    		goto rf_init_return;
    }

   if (chipid0_rf == 0x03) {    //ES2.0 start

	ioctl_rf_t *RF_user = (ioctl_rf_t *) arg;
	ioctl_rf_t *RF = &cmdctrl->RF;
	size_t tmpsize = sizeof(ioctl_rf_t);
	//int rtncode = 0;
	int indx = 0;
	int swUHF = -1;
	unsigned char reg;
	unsigned int value = 0;
	u8 REG2E = MB86A35_DEF_REG2E;
	u8 REG31 = MB86A35_DEF_REG31;
	u8 REG5A = MB86A35_DEF_REG5A;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	

	if (RF_user == NULL) {
		rtncode = -EINVAL;
		goto rf_init_return;
	}

	memset(RF, 0, tmpsize);
	if (copy_from_user(RF, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto rf_init_return;
	}

	printk("mb86a35_IOCTL_RF_INIT mode = %d \n", RF->mode);

	switch (RF->mode) {
		/* UHF */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
		swUHF = MB86A35_SELECT_UHF;
		break;

		/* VHF */
	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
		swUHF = MB86A35_SELECT_VHF;
		break;

	default:
		rtncode = -EINVAL;
		goto rf_init_return;
		break;
	}

	if (swUHF == MB86A35_SELECT_UHF) {
		for (indx = 0; RF_INIT_UHF_DATA[indx].address != 0xff; indx++) {
			reg = RF_INIT_UHF_DATA[indx].address;
			value = RF_INIT_UHF_DATA[indx].data;
			mb86a35_i2c_rf_send(reg, value);
		}
	} else {
		for (indx = 0; RF_INIT_VHF_DATA[indx].address != 0xff; indx++) {
			reg = RF_INIT_VHF_DATA[indx].address;
			value = RF_INIT_VHF_DATA[indx].data;
			mb86a35_i2c_rf_send(reg, value);
		}
	}

	reg = 0x5A;
	value = REG5A;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x31;
	value = REG31;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x2E;
	value = REG2E;
	mb86a35_i2c_rf_send(reg, value);

	if ((rtncode =
	     mb86a35_IOCTL_RF_FUSEVALUE(cmdctrl, cmd,
					(unsigned long)NULL)) < 0) {
		goto rf_init_return;
	}

	if ((rtncode =
	     mb86a35_IOCTL_RF_CALIB_DCOFF(cmdctrl, cmd,
					  (unsigned long)NULL)) < 0) {
		goto rf_init_return;
	}

	if ((rtncode = mb86a35_IOCTL_RF_CALIB_CTUNE_do(cmdctrl, cmd, RF)) < 0) {
		goto rf_init_return;
	}

   }          //ES2.0 end
   else {     //ES3.0 start

	ioctl_rf_t *RF_user = (ioctl_rf_t *) arg;
	ioctl_rf_t *RF = &cmdctrl->RF;
	size_t tmpsize = sizeof(ioctl_rf_t);
	//int rtncode = 0;
	int indx = 0;
	int swUHF = -1;
	unsigned char reg;
	unsigned int value = 0;
	unsigned char ui8REVID;
	
	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);
	
	if (RF_user == NULL) {
		rtncode = -EINVAL;
		goto rf_init_return;
	}

	memset(RF, 0, tmpsize);
	if (copy_from_user(RF, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto rf_init_return;
	}

	switch (RF->mode) {
		/* UHF */
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
		swUHF = MB86A35_SELECT_UHF;
		break;

		/* VHF */
	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
		swUHF = MB86A35_SELECT_VHF;
		break;

	default:
		rtncode = -EINVAL;
		goto rf_init_return;
		break;
	}
	ui8REVID  = chipid0_rf & 0x0F;

	if (swUHF == MB86A35_SELECT_UHF) {
		if ( ui8REVID == 0x01) {
			for (indx = 0; RF_INIT_UHF_DATA1[indx].address != 0xff; indx++) {
				reg = RF_INIT_UHF_DATA1[indx].address;
				value = RF_INIT_UHF_DATA1[indx].data;
				mb86a35_i2c_rf_send(reg, value);
			}
		}
		else{
			for (indx = 0; RF_INIT_UHF_DATA2[indx].address != 0xff; indx++) {
				reg = RF_INIT_UHF_DATA2[indx].address;
				value = RF_INIT_UHF_DATA2[indx].data;
				mb86a35_i2c_rf_send(reg, value);
			}
		}
	} 
	else {
		if ( ui8REVID == 0x01) {
			for (indx = 0; RF_INIT_VHF_DATA1[indx].address != 0xff; indx++) {
				reg = RF_INIT_VHF_DATA1[indx].address;
				value = RF_INIT_VHF_DATA1[indx].data;
				mb86a35_i2c_rf_send(reg, value);
			}
		}
		else{
			for (indx = 0; RF_INIT_VHF_DATA2[indx].address != 0xff; indx++) {
				reg = RF_INIT_VHF_DATA2[indx].address;
				value = RF_INIT_VHF_DATA2[indx].data;
				mb86a35_i2c_rf_send(reg, value);
			}
		}
	}

	if ((rtncode =
	     mb86a35_IOCTL_RF_CALIB_DCOFF(cmdctrl, cmd,
					  (unsigned long)NULL)) < 0) {
		goto rf_init_return;
	}

	if ((rtncode = mb86a35_IOCTL_RF_CALIB_CTUNE_do(cmdctrl, cmd, RF)) < 0) {
		goto rf_init_return;
	}

  } //ES3.0 end

rf_init_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RF_FUSEVALUE command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_FUSEVALUE(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			       unsigned long arg)
{
	int rtncode = 0;
	int fuseval = 0;
	u8 fuse1 = 0;
	u8 fuse_L = 0;
	u8 fuse_H = 0;
	unsigned char reg;
	unsigned int value = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	reg = 0x2B;
	rtncode = mb86a35_i2c_rf_recv(reg, &fuse1, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_fusevalue_return;
	}

	value = fuse1 | 0x10;
	mb86a35_i2c_rf_send(reg, value);

	udelay(10);

	value = fuse1 & 0xEF;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x13;
	rtncode = mb86a35_i2c_rf_recv(reg, &fuse_L, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_fusevalue_return;
	}

	reg = 0x12;
	rtncode = mb86a35_i2c_rf_recv(reg, &fuse_H, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_fusevalue_return;
	}

	fuseval = fuse_L + (fuse_H << 8);
	fuseval = fuseval & 0x3F;
	if (fuseval > 0) {
		reg = 0x80;
		value = fuseval;
		mb86a35_i2c_rf_send(reg, value);
	}

rf_fusevalue_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RF_CALIB_DCOFF command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_CALIB_DCOFF(mb86a35_cmdcontrol_t * cmdctrl,
				 unsigned int cmd, unsigned long arg)
{

  int rtncode_rf = 0;
  unsigned char reg_rf = 0;
  u8 chipid0_rf = 0;
  int rtncode = 0;
   rtncode_rf = mb86a35_i2c_rf_recv(reg_rf, &chipid0_rf, 1);
   if (rtncode_rf != 0) {
    		rtncode = -EFAULT;
    		goto rf_calib_dcoff_return;
    }

   if (chipid0_rf == 0x03) {    //ES2.0 start

	//int rtncode = 0;
	int val = 0;
	int n = 6;
	unsigned char reg;
	unsigned int value = 0;
	u8 REG0A = 0;
	u8 REG2F = 0;
	u8 REG7F = 0;
	u8 REG80 = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	reg = 0x80;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG80, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	REG80 &= 0x3F;

	reg = 0x2F;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG2F, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	REG2F &= 0xFE;

	reg = 0x30;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG30, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	REG30 &= 0xBF;

	reg = 0x7F;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG7F, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	REG7F &= 0x3F;

	reg = 0x7F;
	value = REG7F;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x89;
	value = 0;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x80;
	value = REG80 | 0x80;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x2F;
	value = REG2F;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x30;
	value = REG30;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x3D;
	value = 0xB8;
	mb86a35_i2c_rf_send(reg, value);

	val = 31;
	reg = 0x8B;
	value = val;
	mb86a35_i2c_rf_send(reg, value);

	/*******************************************************************/
	n = 6;
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 5;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 4;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 3;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 2;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 1;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}
	/*******************************************************************/

	reg = 0x3D;
	value = 0x38;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x80;
	value = REG80;
	mb86a35_i2c_rf_send(reg, value);

   }          //ES2.0 end
   else {     //ES3.0 start

	//int rtncode = 0;
	int val = 0;
	int n = 6;
	unsigned char reg;
	unsigned int value = 0;
	u8 REG0A = 0;
	u8 REG2F = 0;
	u8 REG7F = 0;
	u8 REG80 = 0;
	u8 REG3D = 0;  //ES 3.0 add

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	reg = 0x80;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG80, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	//REG80 &= 0x3F;

	reg = 0x2F;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG2F, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	//REG2F &= 0xFE;

	reg = 0x30;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG30, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	//REG30 &= 0xBF;

	reg = 0x7F;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG7F, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	//REG7F &= 0x3F;

	reg = 0x3D;    // ES3.0 add
	rtncode = mb86a35_i2c_rf_recv(reg, &REG3D, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	//REG3D |= 0x80;

	reg = 0x7F;
	value = REG7F&0x3F;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x89;
	value = 0;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x80;
	value = (REG80 & 0x3F) | 0x80;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x2F;
	value = REG2F&0xFE;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x30;
	value = REG30&0xBF;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x3D;
	value = REG3D | 0x80; // ES3.0 change
	mb86a35_i2c_rf_send(reg, value);

	val = 31;
	reg = 0x8B;
	value = val;
	mb86a35_i2c_rf_send(reg, value);

	/*******************************************************************/
	n = 6;
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 5;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 4;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 3;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 2;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}

	/*******************************************************************/
	n = 1;
	val = val & (0x3F - (1 << (n - 1)));
	reg = 0x8B;
	mb86a35_i2c_rf_send(reg, val);
	/* WAIT */
	mdelay(MB86A35_CALB_DCOFF_WAIT);
	/********/
	reg = 0x0A;
	rtncode = mb86a35_i2c_rf_recv(reg, &REG0A, 1);
	if (rtncode != 0) {
		rtncode = -EFAULT;
		goto rf_calib_dcoff_return;
	}
	reg = 0x8B;
	if (REG0A < 0x80) {
		val = val | (1 << (n - 1));
		mb86a35_i2c_rf_send(reg, val);
	}
	if (REG0A >= 0x80) {
		mb86a35_i2c_rf_send(reg, val);
	}
	/*******************************************************************/

	//ES3.0 change start
	reg = 0x2F;  
	value = REG2F;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x30;  
	value = REG30;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x3D;  
	value = REG3D;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x7F;  
	value = REG7F;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x80;  
	value = REG80;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x89;  
	value = 0x27;
	mb86a35_i2c_rf_send(reg, value);
	//ES3.0 change end

   }          //ES3.0 end

rf_calib_dcoff_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RF_CALIB_CTUNE command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_CALIB_CTUNE(mb86a35_cmdcontrol_t * cmdctrl,
				 unsigned int cmd, unsigned long arg)
{
	ioctl_rf_t *RF_user = (ioctl_rf_t *) arg;
	ioctl_rf_t *RF = &cmdctrl->RF;
	size_t tmpsize = sizeof(ioctl_rf_t);
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (RF_user == NULL) {
		rtncode = -EINVAL;
		goto ch_ctune_return;
	}

	memset(RF, 0, tmpsize);
	if (copy_from_user(RF, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ch_ctune_return;
	}

	if ((rtncode = mb86a35_IOCTL_RF_CALIB_CTUNE_do(cmdctrl, cmd, RF)) < 0) {
		goto ch_ctune_return;
	}

ch_ctune_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RF_CHANNEL( command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_CHANNEL(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			     unsigned long arg)
{
	ioctl_rf_t *RF_user = (ioctl_rf_t *) arg;
	ioctl_rf_t *RF = &cmdctrl->RF;
	size_t tmpsize = sizeof(ioctl_rf_t);
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (RF_user == NULL) {
		rtncode = -EINVAL;
		goto rf_channel_return;
	}

	memset(RF, 0, tmpsize);
	if (copy_from_user(RF, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto rf_channel_return;
	}

	switch (RF->mode) {
	case PARAM_MODE_ISDBT_13UHF:
	case PARAM_MODE_ISDBT_1UHF:
		if ((RF->ch_no < 13) || (RF->ch_no > 62)) {
			rtncode = -EINVAL;
			goto rf_channel_return;
		}
		
		break;

	case PARAM_MODE_ISDBTMM_13VHF:
	case PARAM_MODE_ISDBTMM_1VHF:
	case PARAM_MODE_ISDBTSB_1VHF:
	case PARAM_MODE_ISDBTSB_3VHF:
//		if ((RF->ch_no < 13) & (RF->ch_no > 52)) {
		if ((RF->ch_no < 1) || (RF->ch_no > 33)) {
			rtncode = -EINVAL;
			goto rf_channel_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto rf_channel_return;
		break;
	}

	/*** RF-IC Setting ***/
	rtncode = mb86a35_RF_channel(cmdctrl, RF->mode, RF->ch_no);

rf_channel_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RF_PDOWN( command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_PDOWN(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	unsigned char reg;
	unsigned int value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	reg = 0x2F;
	value = 0xFF;
	mb86a35_i2c_rf_send(reg, value);

	reg = 0x30;
	value = 0xFF;
	mb86a35_i2c_rf_send(reg, value);

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_RF_RSSIMONI command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_RF_RSSIMONI(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			      unsigned long arg)
{
	ioctl_rf_t *RF_user = (ioctl_rf_t *) arg;
	ioctl_rf_t *RF = &cmdctrl->RF;
	size_t readsize = 3;
	int rtncode = 0;
	unsigned int reg;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (RF_user == NULL) {
		rtncode = -EINVAL;
		goto rssimoni_return;
	}

	reg = (int)MB86A35_REG_RFADDR_RFAGC;
	rtncode =
	    mb86a35_i2c_rf_recv(reg, (unsigned char *)&RF->RFAGC, readsize);
	if (rtncode >= 0) {
		RF->LNA &= MB86A35_MASK_LNAGAIN_LNA;
		if (copy_to_user
		    ((void *)&RF_user->RFAGC, (void *)&RF->RFAGC, readsize)) {
			rtncode = -EFAULT;
			goto rssimoni_return;
		} else {
			rtncode = 0;
		}
	}

rssimoni_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_POWERCTRL command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_POWERCTRL(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			    unsigned long arg)
{
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_I2C_MAIN command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_I2C_MAIN(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	ioctl_i2c_t *I2C_user = (ioctl_i2c_t *) arg;
	ioctl_i2c_t *I2C = &cmdctrl->I2C;
	size_t tmpsize = sizeof(ioctl_i2c_t);
	unsigned int reg;
	unsigned int value;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (I2C_user == NULL) {
		rtncode = -EINVAL;
		goto i2c_main_return;
	}

	memset(I2C, 0, tmpsize);
	if (copy_from_user(I2C, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto i2c_main_return;
	}

	switch (I2C->mode) {
	case PARAM_I2C_MODE_SEND:
		reg = I2C->address;
		value = I2C->data[0];
		mb86a35_i2c_master_send(reg, value);
		break;

	case PARAM_I2C_MODE_RECV:
		reg = I2C->address;
		rtncode = mb86a35_i2c_master_recv(reg, &I2C->data[0], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto i2c_main_return;
		}

		if (copy_to_user
		    ((void *)&I2C_user->data[0], (void *)&I2C->data[0], 1)) {
			rtncode = -EFAULT;
			goto i2c_main_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto i2c_main_return;
		break;
	}

i2c_main_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_I2C_SUB command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_I2C_SUB(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			  unsigned long arg)
{
	ioctl_i2c_t *I2C_user = (ioctl_i2c_t *) arg;
	ioctl_i2c_t *I2C = &cmdctrl->I2C;
	size_t tmpsize = sizeof(ioctl_i2c_t);
	u8 data[4];
	unsigned char rega;
	unsigned char regd;
	unsigned char sreg;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (I2C_user == NULL) {
		rtncode = -EINVAL;
		goto i2c_sub_return;
	}

	memset(I2C, 0, tmpsize);
	if (copy_from_user(I2C, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto i2c_sub_return;
	}

	memset(data, 0, sizeof(data));
	switch (I2C->mode) {
	case PARAM_I2C_MODE_SEND_8:
	case PARAM_I2C_MODE_SEND_16:
	case PARAM_I2C_MODE_SEND_24:
		rega = I2C->address_sub;
		regd = I2C->data_sub;
		sreg = I2C->address;
		data[0] = I2C->data[0];
		data[1] = I2C->data[1];
		data[2] = I2C->data[2];
		mb86a35_i2c_sub_send(rega, sreg, regd, &data[0], I2C->mode);
		break;

	case PARAM_I2C_MODE_RECV_8:
	case PARAM_I2C_MODE_RECV_16:
	case PARAM_I2C_MODE_RECV_24:
		rega = I2C->address_sub;
		regd = I2C->data_sub;
		sreg = I2C->address;
		rtncode =
		    mb86a35_i2c_sub_recv(rega, sreg, regd, &I2C->data[0],
					 I2C->mode);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto i2c_sub_return;
		}
		if (copy_to_user
		    ((void *)&I2C_user->data[0], (void *)&I2C->data[0], 3)) {
			rtncode = -EFAULT;
			goto i2c_sub_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto i2c_sub_return;
		break;
	}

i2c_sub_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_I2C_RF command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_I2C_RF(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			 unsigned long arg)
{
	ioctl_i2c_t *I2C_user = (ioctl_i2c_t *) arg;
	ioctl_i2c_t *I2C = &cmdctrl->I2C;
	size_t tmpsize = sizeof(ioctl_i2c_t);
	unsigned char reg;
	unsigned int value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (I2C_user == NULL) {
		rtncode = -EINVAL;
		goto i2c_rf_return;
	}

	memset(I2C, 0, tmpsize);
	if (copy_from_user(I2C, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto i2c_rf_return;
	}

	switch (I2C->mode) {
	case PARAM_I2C_MODE_SEND:
		reg = I2C->address;
		value = I2C->data[0];
		mb86a35_i2c_rf_send(reg, value);
		break;

	case PARAM_I2C_MODE_RECV:
		reg = I2C->address;
		rtncode =
		    mb86a35_i2c_rf_recv(reg, (unsigned char *)&I2C->data[0], 1);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto i2c_rf_return;
		}

		if (copy_to_user
		    ((void *)&I2C_user->data[0], (void *)&I2C->data[0], 1)) {
			rtncode = -EFAULT;
			goto i2c_rf_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto i2c_rf_return;
		break;
	}

i2c_rf_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}


/************************************************************************/
/**
	ioctl System Call.  IOCTL_SPI_MAIN command control. \n
	Device Driver for Multi mode tuner module. (MB86A35S)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SPI_MAIN(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			   unsigned long arg)
{
	ioctl_spi_t *SPI_user = (ioctl_spi_t *) arg;
	ioctl_spi_t *SPI = &cmdctrl->SPI;
	size_t tmpsize = sizeof(ioctl_spi_t);
	unsigned char reg;
	unsigned int value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SPI_user == NULL) {
		rtncode = -EINVAL;
		goto spi_main_return;
	}

	memset(SPI, 0, tmpsize);
	if (copy_from_user(SPI, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto spi_main_return;
	}

	switch (SPI->mode) {
	case PARAM_SPI_MODE_SEND:
		reg = SPI->address;
		value = (uint)SPI->data[0];
		rtncode = mb86a35s_spi_send(reg, value, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_main_return;
		}
		break;

	case PARAM_SPI_MODE_RECV:
		reg = SPI->address;
		rtncode = mb86a35s_spi_recv(reg, SPI->data, MB86A35S_SPI_READ_1BYTE);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_main_return;
		}

		if (copy_to_user
		    ((void *)&SPI_user->data[0], (void *)&SPI->data[0], MB86A35S_SPI_READ_1BYTE)) {
			rtncode = -EFAULT;
			goto spi_main_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto spi_main_return;
		break;
	}

spi_main_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SPI_SUB command control. \n
	Device Driver for Multi mode tuner module. (MB86A35S)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SPI_SUB(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			  unsigned long arg)
{
	ioctl_spi_t *SPI_user = (ioctl_spi_t *) arg;
	ioctl_spi_t *SPI = &cmdctrl->SPI;
	size_t tmpsize = sizeof(ioctl_spi_t);
	unsigned char rega  = 0;
	unsigned char regd  = 0;
	unsigned char sreg  = 0;
	unsigned int  value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SPI_user == NULL) {
		rtncode = -EINVAL;
		goto spi_sub_return;
	}

	memset(SPI, 0, tmpsize);
	if (copy_from_user(SPI, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto spi_sub_return;
	}

	switch (SPI->mode) {
	case PARAM_SPI_MODE_SEND_8:
		rega  = SPI->address_sub;
		regd  = SPI->data_sub;
		sreg  = SPI->address;
		value = (uint)SPI->data[0];
		rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_sub_return;
		}
		break;
	case PARAM_SPI_MODE_SEND_24:
		rega = SPI->address_sub;
		regd = SPI->data_sub;
		sreg = SPI->address;
		memcpy(&value, SPI->data, MB86A35S_SPI_WRITE_3BYTE);
		rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_3BYTE);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_sub_return;
		}
		break;

	case PARAM_SPI_MODE_RECV_8:
		rega = SPI->address_sub;
		regd = SPI->data_sub;
		sreg = SPI->address;
		rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, SPI->data, MB86A35S_SPI_READ_1BYTE);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_sub_return;
		}
		if (copy_to_user
		    ((void *)&SPI_user->data[0], (void *)&SPI->data[0], MB86A35S_SPI_READ_1BYTE)) {
			rtncode = -EFAULT;
			goto spi_sub_return;
		}
		break;
	case PARAM_SPI_MODE_RECV_24:
		rega = SPI->address_sub;
		regd = SPI->data_sub;
		sreg = SPI->address;
		rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, SPI->data, MB86A35S_SPI_READ_3BYTE);
#ifdef WORKAROUND_XX
		/* LSI bug. Multi byte read error. need 2 times read.	*/
		rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, SPI->data, MB86A35S_SPI_READ_3BYTE);
#endif
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_sub_return;
		}
		if (copy_to_user
		    ((void *)&SPI_user->data[0], (void *)&SPI->data[0], MB86A35S_SPI_READ_3BYTE)) {
			rtncode = -EFAULT;
			goto spi_sub_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto spi_sub_return;
		break;
	}

spi_sub_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SPI_RF command control. \n
	Device Driver for Multi mode tuner module. (MB86A35S)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SPI_RF(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			 unsigned long arg)
{
	ioctl_spi_t *SPI_user = (ioctl_spi_t *) arg;
	ioctl_spi_t *SPI = &cmdctrl->SPI;
	size_t tmpsize = sizeof(ioctl_spi_t);
	unsigned char reg   = 0;
	unsigned int  value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SPI_user == NULL) {
		rtncode = -EINVAL;
		goto spi_rf_return;
	}

	memset(SPI, 0, tmpsize);
	if (copy_from_user(SPI, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto spi_rf_return;
	}

	switch (SPI->mode) {
	case PARAM_SPI_MODE_SEND:
		reg = SPI->address;
		value = (uint)SPI->data[0];
		rtncode = mb86a35s_spi_rf_send(reg, value, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_rf_return;
		}
		break;
	case PARAM_SPI_MODE_RECV:
		reg = SPI->address;
		rtncode = mb86a35s_spi_rf_recv(reg, SPI->data, MB86A35S_SPI_READ_1BYTE);
		if (rtncode != 0) {
			rtncode = -EFAULT;
			goto spi_rf_return;
		}

		if (copy_to_user
		    ((void *)&SPI_user->data[0], (void *)&SPI->data[0], MB86A35S_SPI_READ_1BYTE)) {
			rtncode = -EFAULT;
			goto spi_rf_return;
		}
		break;

	default:
		rtncode = -EINVAL;
		goto spi_rf_return;
		break;
	}

spi_rf_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_HRM command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_HRM(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			 unsigned long arg)
{
	ioctl_hrm_t *HRM_user = (ioctl_hrm_t *) arg;
	ioctl_hrm_t *HRM = &cmdctrl->HRM;
	size_t tmpsize = sizeof(ioctl_hrm_t);
	unsigned char reg;
	unsigned int value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);
	printk("mb86a35_IOCTL_HRM In\n");

	if (HRM_user == NULL) {
		rtncode = -EINVAL;
		goto hrm_return;
	}

	memset(HRM, 0, tmpsize);
	if (copy_from_user(HRM, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto hrm_return;
	}

	switch (HRM->hrm) {
	case PARAM_HRM_ON:
		reg = 0x33;
		value = 0x00;
		mb86a35_i2c_rf_send(reg, value);
		break;

	case PARAM_HRM_OFF:
		reg = 0x33;
		value = 0xa0;
		mb86a35_i2c_rf_send(reg, value);
		break;

	default:
		rtncode = -EINVAL;
		goto hrm_return;
		break;
	}

hrm_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	printk("mb86a35_IOCTL_HRM Out\n");
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_LOW_UP_IF command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_LOW_UP_IF(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd,
			 unsigned long arg)
{
	ioctl_low_up_if_t *LOW_UP_IF_user = (ioctl_low_up_if_t *) arg;
	ioctl_low_up_if_t *LOW_UP_IF = &cmdctrl->LOW_UP_IF;
	size_t tmpsize = sizeof(ioctl_low_up_if_t);
	unsigned char reg;
	unsigned int value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (LOW_UP_IF_user == NULL) {
		rtncode = -EINVAL;
		goto low_up_if_return;
	}

	memset(LOW_UP_IF, 0, tmpsize);
	if (copy_from_user(LOW_UP_IF, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto low_up_if_return;
	}

	switch (LOW_UP_IF->mode) {
	case PARAM_MODE_LOWER_IF:
		reg = 0x86;
		value = 0x00;
		mb86a35_i2c_rf_send(reg, value);
		break;

	case PARAM_MODE_UPPER_IF:
		reg = 0x86;
		value = 0x20;
		mb86a35_i2c_rf_send(reg, value);
		break;

	default:
		rtncode = -EINVAL;
		goto low_up_if_return;
		break;
	}

low_up_if_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_SPI_CONFIG command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SPI_CONFIG(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd, unsigned long arg)
			 
{
	ioctl_spi_config_t *SPI_CONFIG_user	= (ioctl_spi_config_t *)arg;
	ioctl_spi_config_t *SPI_CONFIG		= &cmdctrl->SPI_CONFIG;
	size_t tmpsize				= sizeof(ioctl_spi_config_t);
	unsigned int value = 0;
	unsigned char rvalue = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (SPI_CONFIG_user == NULL) {
		rtncode = -EINVAL;
		goto spi_config_return;
	}

	memset(SPI_CONFIG, 0, tmpsize);
	if (copy_from_user(SPI_CONFIG, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n", PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto spi_config_return;
	}

	if (SPI_CONFIG->SPICTRL & ~(PARAM_SPICTRL_STMODE_MASTER | PARAM_SPICTRL_STMODE_SLAVE
				| PARAM_SPICTRL_SPINUM_SINGLE | PARAM_SPICTRL_SPINUM_DUAL)) {
		rtncode = -EINVAL;
		goto spi_config_return;
	}

	if (SPI_CONFIG->IOSEL3 & ~(PARAM_IOSEL3_SPIC_CMOS | PARAM_IOSEL3_SPIC_OPENDRAIN)) {
		rtncode = -EINVAL;
		goto spi_config_return;
	}

	if (SPI_CONFIG->IO_OUTEN & ~(PARAM_IO_OUTEN_HIZ_EN | PARAM_IO_OUTEN_HIZ_DIS)) {
		rtncode = -EINVAL;
		goto spi_config_return;
	}

	/* SPI CASE Setting	*/
	value = SPI_CONFIG->SPICTRL;
	rtncode = mb86a35s_spi_send(MB86A35S_REG_ADDR_SPICTRL, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto spi_config_return;
	}

	/* SPI OUTPUT MODE setting	*/
	value = 0;
	rtncode = mb86a35s_spi_recv(MB86A35S_REG_ADDR_IOSEL3, &rvalue, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto spi_config_return;
	}

	if (SPI_CONFIG->IOSEL3 & PARAM_IOSEL3_SPIC_CMOS) {
		value = rvalue | PARAM_IOSEL3_SPIC_CMOS;
	}
	else {
		value = rvalue & ~(PARAM_IOSEL3_SPIC_CMOS);
	}
	rtncode = mb86a35s_spi_send(MB86A35S_REG_ADDR_IOSEL3, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto spi_config_return;
	}

	/* SPI OUTPUT Hi-z setting	*/
	value = 0;
	rtncode = mb86a35s_spi_recv(MB86A35S_REG_ADDR_IOOUTEN, &rvalue, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto spi_config_return;
	}

	if (SPI_CONFIG->IO_OUTEN & PARAM_IO_OUTEN_HIZ_EN) {
		value = rvalue | PARAM_IO_OUTEN_HIZ_EN;
	}
	else {
		value = rvalue & ~(PARAM_IO_OUTEN_HIZ_EN);
	}
	rtncode = mb86a35s_spi_send(MB86A35S_REG_ADDR_IOOUTEN, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto spi_config_return;
	}

spi_config_return:

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER, rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_STREAM_READ command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_STREAM_READ(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd, unsigned long arg)
{
	ioctl_stream_read_t *STREAM_READ_user	= (ioctl_stream_read_t *)arg;
	ioctl_stream_read_t *STREAM_READ	= &cmdctrl->STREAM_READ;
	size_t tmpsize				= sizeof(ioctl_stream_read_t);
	unsigned char reg = 0;
	unsigned char rega = 0;
	unsigned char sreg = 0;
	unsigned char regd = 0;
	unsigned int value = 0;
	unsigned char rvalue = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (STREAM_READ_user == NULL) {
		rtncode = -EINVAL;
		goto stream_read_return;
	}

	memset(STREAM_READ, 0, tmpsize);
	if (copy_from_user(STREAM_READ, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n", PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto stream_read_return;
	}

	if (STREAM_READ->SBER & ~(PARAM_SBER_TSCLKCTRL_ON | PARAM_SBER_TSCLKCTRL_OFF)) {
		rtncode = -EINVAL;
		goto stream_read_return;
	}

	if (STREAM_READ->NULLCTRL & ~(PARAM_NULLCTRL_PNULL_ON | PARAM_NULLCTRL_PNULL_OFF
				    | PARAM_NULLCTRL_SNULL_ON | PARAM_NULLCTRL_SNULL_OFF)) {
		rtncode = -EINVAL;
		goto stream_read_return;
	}

	/* SPIRF_FREQ set 	*/
	reg = MB86A35S_REG_ADDR_SPIRF_FREQ;
	rtncode = mb86a35s_spi_send(reg, MB86A35S_SPIRF_FREQ, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto stream_read_return;
	}

	/* need!! force setting	*/
	value = 0x02;
	rega = MB86A35S_REG_ADDR_SPI_SUBA;
	regd = MB86A35S_REG_ADDR_SPI_SUBD;
	sreg	= 0x27;
	rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto stream_read_return;
	}

	/* SBER SCLKSEL read	*/
	value = 0;
	rega = MB86A35_REG_ADDR_FEC_SUBA;
	regd = MB86A35_REG_ADDR_FEC_SUBD;
	sreg	= MB86A35_REG_SUBR_SBER;
	rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, &rvalue, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto stream_read_return;
	}

	/* SBER SCLKSEL : set Passive edge	*/
	if (STREAM_READ->SBER & PARAM_SBER_TSCLKCTRL_ON) {
		value = rvalue | PARAM_SBER_TSCLKCTRL_ON;
	}
	else {
		value = rvalue & ~(PARAM_SBER_TSCLKCTRL_ON);
	}
	rega = MB86A35_REG_ADDR_FEC_SUBA;
	regd = MB86A35_REG_ADDR_FEC_SUBD;
	sreg	= MB86A35_REG_SUBR_SBER;
	rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto stream_read_return;
	}

	/* NULLCTRL  read	*/
	value = 0;
	rega = MB86A35_REG_ADDR_FEC_SUBA;
	regd = MB86A35_REG_ADDR_FEC_SUBD;
	sreg	= MB86A35_REG_SUBR_NULLCTRL;
	rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, &rvalue, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto stream_read_return;
	}

	/* Set PNULL	*/
	if (STREAM_READ->NULLCTRL & PARAM_NULLCTRL_PNULL_OFF) {
		value = rvalue | PARAM_NULLCTRL_PNULL_OFF;
	}
	else {
		value = rvalue & ~(PARAM_NULLCTRL_PNULL_OFF);
	}
	/* Set SNULL	*/
	if (STREAM_READ->NULLCTRL & PARAM_NULLCTRL_SNULL_OFF) {
		value = value | PARAM_NULLCTRL_SNULL_OFF;
	}
	else {
		value = value & ~(PARAM_NULLCTRL_SNULL_OFF);
	}
	rega = MB86A35_REG_ADDR_FEC_SUBA;
	regd = MB86A35_REG_ADDR_FEC_SUBD;
	sreg	= MB86A35_REG_SUBR_NULLCTRL;
	rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto stream_read_return;
	}

#if 1 //    
	MMBICtrlInfo.IsdbtPowerOnState = TRUE;
	MMBICtrlInfo.spi_irq_status = FALSE;
//                                                                    
#if 0		
	wake_lock(&MMBICtrlInfo.wake_lock);
#endif
#endif

stream_read_return:

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER, rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_STREAM_READ_CTRL command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	Success.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_STREAM_READ_CTRL(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd, unsigned long arg)
{
	ioctl_stream_read_ctrl_t *STREAM_READ_CTRL_user	= (ioctl_stream_read_ctrl_t *)arg;
	ioctl_stream_read_ctrl_t *STREAM_READ_CTRL	= &cmdctrl->STREAM_READ_CTRL;
	size_t tmpsize					= sizeof(ioctl_stream_read_ctrl_t);
	unsigned char rega = MB86A35S_REG_ADDR_SPI_SUBA;
	unsigned char sreg = 0;
	unsigned char regd = MB86A35S_REG_ADDR_SPI_SUBD;
	unsigned int value = 0;
	unsigned char value_read_low = 0;
	unsigned char value_read_high = 0;
	unsigned char rvalue = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (STREAM_READ_CTRL_user == NULL) {
		rtncode = -EINVAL;
		goto stream_read_ctrl_return;
	}

	memset(STREAM_READ_CTRL, 0, tmpsize);
	if (copy_from_user(STREAM_READ_CTRL, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n", PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto stream_read_ctrl_return;
	}

#ifdef DEBUG
	printk(KERN_INFO "%s: READ_SIZE:%d, BUF:%p\n", __FUNCTION__,
	       STREAM_READ_CTRL->READ_SIZE,
	       STREAM_READ_CTRL->BUF);
#endif

	if (STREAM_READ_CTRL->STREAM_CTRL & ~(PARAM_STREAMREAD_START | PARAM_STREAMREAD_STOP)) {
		rtncode = -EINVAL;
		goto stream_read_ctrl_return;
	}

	/* Stream Read START	*/
	if (STREAM_READ_CTRL->STREAM_CTRL & PARAM_STREAMREAD_START) {

		if (STREAM_READ_CTRL->READ_SIZE > MB86A35S_READ_SIZE_MAX) {
			rtncode = -EINVAL;
			goto stream_read_ctrl_return;
		}
#if 0 //   
		if (STREAM_READ_CTRL->BUF == NULL) {
			rtncode = -EINVAL;
			goto stream_read_ctrl_return;
		}
#endif 
		if (!access_ok(VERIFY_WRITE, STREAM_READ_CTRL->BUF, STREAM_READ_CTRL->READ_SIZE)) {
			rtncode =  -EFAULT;
			goto stream_read_ctrl_return;
		}
#if 0 //   
		if (STREAM_READ_CTRL->BUF_SIZE % STREAM_READ_CTRL->READ_SIZE) {
			rtncode = -EINVAL;
			goto stream_read_ctrl_return;
		}
#endif
		#if 0 //   
		read_size = STREAM_READ_CTRL->READ_SIZE;
		read_buff = STREAM_READ_CTRL->BUF;
		buff_size = STREAM_READ_CTRL->BUF_SIZE;
		#else
		read_size = STREAM_READ_CTRL->READ_SIZE;	//drv. interrupt threshold size
		//read_buff = ;
		//buff_size = STREAM_READ_CTRL->BUF_SIZE;		//

		mbt_dataring_create(&buf_id, read_size*30);
		MMBICtrlInfo.IsdbtPowerOnState = TRUE;
//                                                                    
#if 0		
		wake_lock(&MMBICtrlInfo.wake_lock);
#endif
		//enable_irq(MMBICtrlInfo.pSpiDevice->irq);
		#endif

		mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
		
		/* IRQ Mask read	*/
		value = 0;
		sreg	= MB86A35S_REG_SUBR_SPIS0_IRQMASK;
		rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, &rvalue, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}

		/* Stream read exec	*/
		/* Set threshold value of Stream Read Standby IRQ	*/
		value = (STREAM_READ_CTRL->READ_SIZE >> 8);
		sreg	= MB86A35S_REG_SUBR_RAMDATAH;
		rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}
	
		value = (STREAM_READ_CTRL->READ_SIZE & 0x00ff);
		sreg	= MB86A35S_REG_SUBR_RAMDATAL;
		rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}

		//value = (STREAM_READ_CTRL->READ_SIZE >> 8);
		sreg	= MB86A35S_REG_SUBR_RAMDATAH;
		rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, &value_read_high, MB86A35S_SPI_READ_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}
	
		//value = (STREAM_READ_CTRL->READ_SIZE >> 8);
		sreg	= MB86A35S_REG_SUBR_RAMDATAL;
		rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, &value_read_low, MB86A35S_SPI_READ_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}

		printk("\n TS Start interrupt enable !!!!!! read size check high = %2x low = %2x \n", value_read_high, value_read_low);

		handler_enable_flag = 1;

		/* IRQ Enable */
		value = rvalue & ~(PARAM_IRQMASK_STREAMREAD_OFF);
		sreg	= MB86A35S_REG_SUBR_SPIS0_IRQMASK;
		rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}

		mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE);

		/* IRQ handler enable flag on	*/ 
		
	} 
	/* Stream Read STOP	*/
	else {

		mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!

		/* IRQ handler enable flag off	*/ 
		handler_enable_flag = 0;

		/* IRQ Mask read	*/
		value = 0;
		sreg	= MB86A35S_REG_SUBR_SPIS0_IRQMASK;
		rtncode = mb86a35s_spi_sub_recv(rega, sreg, regd, &rvalue, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}

		/* IRQ Disable */
		value = rvalue | PARAM_IRQMASK_STREAMREAD_OFF;
		sreg	= MB86A35S_REG_SUBR_SPIS0_IRQMASK;
		rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
		if (rtncode != 0) {
			goto stream_read_ctrl_return;
		}
		
		#if 0 //   
		//read_size = 0;
		//read_buff = NULL;
		//buff_size = 0;
		//rbuffp    = NULL;
		#else
		//disable_irq(MMBICtrlInfo.pSpiDevice->irq);
		
		mbt_dataring_destroy(&buf_id);

		MMBICtrlInfo.IsdbtPowerOnState = FALSE;
//                                                                    
#if 0		
		wake_unlock(&MMBICtrlInfo.wake_lock);
#endif
		buf_id = 0;
		#endif

		mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE);

		printk("TS END interrupt disable\n");
		
	}

stream_read_ctrl_return:

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER, rtncode);
	return rtncode;
}

/************************************************************************/
/**
	ioctl System Call.  IOCTL_TS_SETUP command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_TS_SETUP(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd, unsigned long arg)
{
	ioctl_ts_setup_t *TS_SETUP_user	= (ioctl_ts_setup_t *)arg;
	ioctl_ts_setup_t *TS_SETUP	= &cmdctrl->TS_SETUP;
	size_t tmpsize			= sizeof(ioctl_ts_setup_t);
	unsigned char rega = MB86A35S_REG_ADDR_SPI_SUBA;
	unsigned char sreg = 0;
	unsigned char regd = MB86A35S_REG_ADDR_SPI_SUBD;
	unsigned int value = 0;
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);

	if (TS_SETUP_user == NULL) {
		rtncode = -EINVAL;
		goto ts_setup_return;
	}

	memset(TS_SETUP, 0, tmpsize);
	if (copy_from_user(TS_SETUP, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n", PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto ts_setup_return;

	}

	if (TS_SETUP->PACBYTE & ~(PARAM_PACBYTE_204P | PARAM_PACBYTE_188N)) {
		rtncode = -EINVAL;
		goto ts_setup_return;
	}

	if (TS_SETUP->ERRCTRL & ~(PARAM_ERRCTRL_ERROUT | PARAM_ERRCTRL_ERRNOOUT)) {
		rtncode = -EINVAL;
		goto ts_setup_return;
	}

	value = TS_SETUP->PACBYTE;
	sreg	= MB86A35S_REG_SUBR_PACBYTE;
	rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto ts_setup_return;
	}

	value = TS_SETUP->ERRCTRL;
	sreg	= MB86A35S_REG_SUBR_ERRCTRL;
	rtncode = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
	if (rtncode != 0) {
		goto ts_setup_return;
	}

ts_setup_return:

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER, rtncode);
	return rtncode;
}


/************************************************************************/
/**
	ioctl System Call.  IOCTL_SELECT ANTENNA command control. \n
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	cmdctrl		[in,out] driver contolr area pointer to structure "mb86a35_cmdcontrol_t".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	=0	nothing write data data.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_IOCTL_SELECT_ANTENNA(mb86a35_cmdcontrol_t * cmdctrl, unsigned int cmd, unsigned long arg)
{
	ioctl_select_antenna_t *Select_antenna_user = (ioctl_select_antenna_t *) arg;
	ioctl_select_antenna_t *Select_antenna = &cmdctrl->SELECT_ANTENNA;
	size_t tmpsize = sizeof(ioctl_select_antenna_t);
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : (cmdctrl:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)cmdctrl, (int)cmd, (int)arg);


	if (Select_antenna_user == NULL) {
		rtncode = -EINVAL;
		goto select_antenna_return;
	}

	memset(Select_antenna, 0, tmpsize);
	if (copy_from_user(Select_antenna, (void *)arg, tmpsize)) {
		DBGPRINT(PRINT_LHEADERFMT "copy_from_user failed. (len:%d)\n",
			 PRINT_LHEADER, tmpsize);
		rtncode = -EFAULT;
		goto select_antenna_return;
	}

	printk("[mb86a35_IOCTL_SELECT_ANTENNA] antenna mode : %d\n", Select_antenna->mode);

	switch (Select_antenna->mode) {
	case 0:
		gpio_request(DMB_ANT_SEL_P_EAR, "antenna_p");
		gpio_direction_output(DMB_ANT_SEL_P_EAR, 1);
		break;
	case 1:
		gpio_request(DMB_ANT_SEL_P_EAR, "antenna_p");
		gpio_direction_output(DMB_ANT_SEL_P_EAR, 0);
		break;

	default:
		rtncode = -EINVAL;
		goto select_antenna_return;
		break;
	}

select_antenna_return:
	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER, rtncode);
	return rtncode;
}


/************************************************************************
 * open() system call.
 * [User Interface]
 *     int    open( const char "/dev/radio0", O_RDWR );
 */
/**
	open System Call.
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	inode		[in] pointer to structure "inode".
	@param	filp		[in] pointer to structure "filp".
	@retval	>0	file descriptor. (normal return)
	@retval	0	The error occurred. The detailed information is set to an errno.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/
#if 0
	ktime_t isr_time;
	ktime_t workQ_time;
	ktime_t spi_time;
	ktime_t read_done_time;
	
	ktime_t diff1;
	ktime_t diff2;
	ktime_t diff3;
	
	s64 max_diff1 = 0;
	s64 max_diff2 = 0;
	s64 max_diff3 = 0;
#endif
static int mb86a35_open(struct inode *inode, struct file *filp)
{
	//for lge
	//unsigned int majorno, minorno;
	unsigned char *devarea;
	mb86a35_cmdcontrol_t *cmdctrl;
	size_t getmemsize = sizeof(mb86a35_cmdcontrol_t);

	//cpu_state = 0;
#if 0
	max_diff1 = 0;
	max_diff2 = 0;
	max_diff3 = 0;
#endif
	if(mmbi_tuner_drv_open == 0)
	{
		printk("mb86a35 already open busy\n");
		return -EBUSY;
	}

	mmbi_tuner_drv_open  = 0;
	
	printk("mb86a35_open: filp[0x%08x]->f_dentry[0x%08x]->d_inode[0x%08x] private_data = %x.\n",
		 (int)filp, (int)(filp->f_dentry),
		 (int)(filp->f_dentry->d_inode), (int)filp->private_data);
	DBGPRINT(PRINT_LHEADERFMT " : open()  called.\n", PRINT_LHEADER);
	DBGPRINT(PRINT_LHEADERFMT
		 " : filp[0x%08x]->f_dentry[0x%08x]->d_inode[0x%08x].\n",
		 PRINT_LHEADER, (int)filp, (int)(filp->f_dentry),
		 (int)(filp->f_dentry->d_inode));

	//for lge misc character device 
	//majorno = imajor(filp->f_dentry->d_inode);
	//minorno = iminor(filp->f_dentry->d_inode);
	//if ((majorno != NODE_MAJOR) || (minorno != NODE_MINOR)) {
	//	DBGPRINT(PRINT_LHEADERFMT
	//		 " : Illegal Operation. major[ %d ], minor[ %d ].\n",
	//		 PRINT_LHEADER, majorno, minorno);
	//	return -ENODEV;
	//}
	/*
	if ((filp->f_flags & O_ACCMODE) != O_RDWR) {
		DBGPRINT(PRINT_LHEADERFMT
			 " : Illegal Operation mode(flags). flags[ %d ].\n",
			 PRINT_LHEADER, (filp->f_flags & O_ACCMODE));
		return -EINVAL;
	}
	*/
	devarea = (unsigned char *)filp->private_data;
	if (devarea != NULL) {
		DBGPRINT(PRINT_LHEADERFMT
			 " : Used private_data area. private_data[ 0x%08x ].\n",
			 PRINT_LHEADER, (int)devarea);
		printk("mb86a35_open busy\n");
		return -EBUSY;
	}

	devarea = (unsigned char *)kmalloc(getmemsize, GFP_KERNEL);
	if (devarea == NULL) {
		DBGPRINT(PRINT_LHEADERFMT
			 " : kernel memory is short[ 0x%08x ].\n",
			 PRINT_LHEADER, (int)devarea);
		return -ENOMEM;
	}
	memset(devarea, 0, getmemsize);
	cmdctrl = (mb86a35_cmdcontrol_t *) devarea;
	filp->private_data = (void *)devarea;
	
	DBGPRINT(PRINT_LHEADERFMT
		 " : Normal return. file->private_data[ 0x%08x ]\n",
		 PRINT_LHEADER, (int)filp->private_data);

	//lge
//                                                                  
#if 1		
	wake_lock(&MMBICtrlInfo.wake_lock);
#endif
	isdbt_hw_init();

	return 0;
}

/************************************************************************
 * release() .... close() system call.
 * [User Interface]
 *     int    close( int fd );
 */
/**
	close System Call.
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	inode		[in] pointer to structure "inode".
	@param	filp		[in] pointer to structure "filp".
	@retval	0	normal return.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_close(struct inode *inode, struct file *filp)
{
	#if 1 //for lge
	unsigned char *devarea;
	mb86a35_cmdcontrol_t *cmdctrl;

	//disable_irq(MMBICtrlInfo.pSpiDevice->irq);

	mbt_dataring_destroy(&buf_id);
	buf_id = 0;

	isdbt_hw_deinit();

	DBGPRINT(PRINT_LHEADERFMT " : close()  called.\n", PRINT_LHEADER);
	DBGPRINT(PRINT_LHEADERFMT
		 " : filp[0x%08x]->f_dentry[0x%08x]->d_inode[0x%08x].\n",
		 PRINT_LHEADER, (int)filp, (int)(filp->f_dentry),
		 (int)(filp->f_dentry->d_inode));

	devarea = (unsigned char *)filp->private_data;
	if (devarea == NULL) {
		DBGPRINT(PRINT_LHEADERFMT
			 " : Not found private_data area. private_data[ 0x%08x ].\n",
			 PRINT_LHEADER, (int)devarea);
		return -EFAULT;
	}
	cmdctrl = (mb86a35_cmdcontrol_t *) devarea;

	memset(devarea, 0, sizeof(mb86a35_cmdcontrol_t));

	kfree(devarea);

	filp->private_data = NULL;

	mmbi_tuner_drv_open  = -1;
//                                                                  
#if 1		
	wake_unlock(&MMBICtrlInfo.wake_lock);
#endif
	printk("\ncloseclose\n");
	//printk("diff1 = %lld, diff2 = %lld, diff3 = %lld \n", max_diff1, max_diff2, max_diff3);
	#endif
	return 0;
}

/************************************************************************
 * read() system call.
 * [User Interface]
 *     ssize_t    read( struct file *filp, char __user *buff, size_t count, loff_t *offp );
 */
/**
	read System Call.
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	filp		[in] pointer to structure "filp".
	@param	buf		[out] pointer to read data area.
	@param	count		[in] size of read data area.
	@param	f_pos		[in,out] offset of read data area.
	@retval	0	nothing read data.
	@retval	>0	number of bytes read is returned.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
ssize_t mb86a35_read(struct file *filp, char *buf, size_t count, loff_t * f_pos)
{
	int rtncode = 0;
	#if 1 //   
	int avail_data_size = 0;
	int empty = 0;
	unsigned char* read_buf = NULL;

	read_buf = (unsigned char *)kmalloc(count, GFP_KERNEL);
	if ((read_buf == NULL) ||(buf_id == 0)) {
		printk("read error!!! \n");
		return -ENOMEM;
	}
	#endif

	DBGPRINT(PRINT_LHEADERFMT
		 " : read(filp:0x%08x,buf:0x%08x,count:%d,f_pos:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)filp, (int)buf, count, (int)f_pos);

	
#if 1 //   
	empty = mbt_dataring_empty(buf_id);

	avail_data_size = mbt_dataring_avail(buf_id);

	if((empty == 1) || (avail_data_size <= 0))
	{
		//printk("empty!!! \n");
		kfree(read_buf);
		return -1; //non blocking
	}

	if(avail_data_size <= count)
	{
		
		mbt_dataring_read(buf_id, read_buf, avail_data_size);
		if(copy_to_user(buf, read_buf, avail_data_size))
		{
			printk("mb86a35_read copy_to_user fail1");			
		}
		else
		{
			rtncode = avail_data_size;
		}		
	}
	else
	{
		mbt_dataring_read(buf_id, read_buf, count);
		if(copy_to_user(buf, read_buf, count))
		{
			printk("mb86a35_read copy_to_user fail2");			
		}
		else
		{
			rtncode = count;
		}
	}

	kfree(read_buf);
		
#endif

#ifdef	DEBUG_PRINT_MEM
	if (LOGMEM != NULL) {
		int offset = 0;
		LOGCOPYSIZE = strlen(LOGMEM);
		if (copy_to_user(buf, &LOGMEM[offset], LOGCOPYSIZE)) {
			DBGPRINT(PRINT_LHEADERFMT " : copy_to_user error.\n",
				 PRINT_LHEADER);
			rtncode = -EFAULT;
			goto read_return;
		}
		memset(LOGMEM, 0, DEBUG_PRINT_MEM_SIZE);
		rtncode = LOGCOPYSIZE;
		goto read_return;
	}

	

read_return:
#endif


	DBGPRINT(PRINT_LHEADERFMT " : Read Length[ %d ], Return Code[ %d ]\n",
		 PRINT_LHEADER, count, rtncode);
	return rtncode;
}

/************************************************************************
 * write() system call.
 * [User Interface]
 *     ssize_t    write( struct file *filp, char __user *buff, size_t count, loff_t *offp );
 */
/**
	write System Call.
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	filp		[in] pointer to structure "filp".
	@param	buf		[in] pointer to write data area.
	@param	count		[in] size of write data area.
	@param	f_pos		[in,out] offset of write data area.
	@retval	0	nothing write data data.
	@retval	>0	number of bytes write is returned.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

static
int mb86a35_write(struct file *filp, const char __user * buf, size_t count,
		  loff_t * f_pos)
{
	int rtncode = 0;

	DBGPRINT(PRINT_LHEADERFMT
		 " : write(filp:0x%08x,buf:0x%08x,count:%d,f_pos:0x%08x)  called.\n",
		 PRINT_LHEADER, (int)filp, (int)buf, count, (int)f_pos);

	DBGPRINT(PRINT_LHEADERFMT "**** return[ %d ].\n", PRINT_LHEADER,
		 rtncode);
	return rtncode;
}

/************************************************************************
 * ioctl() system call.
 * [User Interface]
 *     int    ioctl( int fd, unsigned long cmd, ... );
 *     int    ioctl( struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg );
 */
/**
	ioctl System Call.
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	inode		[in] pointer to structure "inode".
	@param	filp		[in] pointer to structure "filp".
	@param	cmd		[in] command code.
	@param	arg		[in,out] command argument/context.
	@retval	0	normal return.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/

#if 0//def KERNEL_2635
static
int mb86a35_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
#else//def KERNEL_2638
static
long mb86a35_ioctl(struct file *filp, unsigned int cmd,
		  unsigned long arg)
#endif
{
	mb86a35_cmdcontrol_t *cmdctrl;
	int rtn = 0;

	//DBGPRINT(PRINT_LHEADERFMT
	//	 " : ioctl(inode:0x%08x,filp:0x%08x,cmd:0x%08x,arg:0x%08x)  called.\n",
	//	 PRINT_LHEADER, (int)inode, (int)filp, cmd, (int)arg);

	//                               

	cmdctrl = (mb86a35_cmdcontrol_t *) filp->private_data;
	if (cmdctrl == NULL) {
		DBGPRINT(PRINT_LHEADERFMT
			 " : nothing driver working space. cmdctrl[ 0x%08x ].\n",
			 PRINT_LHEADER, (int)cmdctrl);
		return -EBADF;
	}

	switch (cmd) {
		/* Reset */
	case IOCTL_RST_SOFT:	/* OFDM Soft-reset ON / OFF */
		rtn = mb86a35_IOCTL_RST_SOFT(cmdctrl, cmd, arg);
		break;

	case IOCTL_RST_SYNC:	/* OFDM Sequence reset ON / OFF */
		rtn = mb86a35_IOCTL_RST_SYNC(cmdctrl, cmd, arg);
		break;

		/* Initialize */
	case IOCTL_SET_RECVM:	/* set Received mode */
		rtn = mb86a35_IOCTL_SET_RECVM(cmdctrl, cmd, arg);
		break;

	case IOCTL_SET_SPECT:	/* set Receive spectrum Invert */
		rtn = mb86a35_IOCTL_SET_SPECT(cmdctrl, cmd, arg);
		break;

	case IOCTL_SET_IQSET:	/* set I/Q Input (0x03) */
		rtn = mb86a35_IOCTL_SET_IQSET(cmdctrl, cmd, arg);
		break;

	case IOCTL_SET_ALOG_PDOWN:	/* set Analog Power-down (DAC Power-down) */
		rtn = mb86a35_IOCTL_SET_ALOG_PDOWN(cmdctrl, cmd, arg);
		break;

	case IOCTL_SET_SCHANNEL:	/* set Sub-Channel */
		rtn = mb86a35_IOCTL_SET_SCHANNEL(cmdctrl, cmd, arg);
		break;

		/* AGC */
	case IOCTL_AGC:	/* set AGC(Auto Gain Control) register */
		rtn = mb86a35_IOCTL_AGC(cmdctrl, cmd, arg);
		break;

		/* AGC */
	case IOCTL_OFDM_INIT:	/* set SYNC, set FEC */
		rtn = mb86a35_IOCTL_SYNC(cmdctrl, cmd, arg);
		if (rtn >= 0) {
			rtn = mb86a35_IOCTL_FEC(cmdctrl, cmd, arg);
		}
		break;

		/* GPIO */
	case IOCTL_GPIO_SETUP:	/* GPIO : Digital I/O port control : SETUP */
		rtn = mb86a35_IOCTL_GPIO_SETUP(cmdctrl, cmd, arg);
		break;

	case IOCTL_GPIO_READ:	/* GPIO : Digital I/O port control : READ */
		rtn = mb86a35_IOCTL_GPIO_READ(cmdctrl, cmd, arg);
		break;

	case IOCTL_GPIO_WRITE:	/* GPIO : Digital I/O port control : WRITE */
		rtn = mb86a35_IOCTL_GPIO_WRITE(cmdctrl, cmd, arg);
		break;

	case IOCTL_ANALOG:	/* GPIO : Analog port control */
		rtn = mb86a35_IOCTL_ANALOG(cmdctrl, cmd, arg);
		break;

		/* Sequence control */
	case IOCTL_SEQ_GETSTAT:	/* get Status */
		rtn = mb86a35_IOCTL_SEQ_GETSTAT(cmdctrl, cmd, arg);
		break;
		
	case IOCTL_SEQ_SETMODE:	/* set Mode */
		rtn = mb86a35_IOCTL_SEQ_SETMODE(cmdctrl, cmd, arg);
		break;
		
	case IOCTL_SEQ_GETMODE:	/* get Mode */
		rtn = mb86a35_IOCTL_SEQ_GETMODE(cmdctrl, cmd, arg);
		break;

	case IOCTL_SEQ_GETTMCC:	/* get TMCC Information */
		rtn = mb86a35_IOCTL_SEQ_GETTMCC(cmdctrl, cmd, arg);
		break;

		/* BER Monitor */
	case IOCTL_BER_MONISTAT:	/* set BER Monitor config */
		rtn = mb86a35_IOCTL_BER_MONISTAT(cmdctrl, cmd, arg);
		break;

	case IOCTL_BER_MONICONFIG:	/* BER Monitor configuration */
		rtn = mb86a35_IOCTL_BER_MONICONFIG(cmdctrl, cmd, arg);
		break;

	case IOCTL_BER_MONISTART:	/* BER Monitor start */
		rtn = mb86a35_IOCTL_BER_MONISTART(cmdctrl, cmd, arg);
		break;

	case IOCTL_BER_MONIGET:	/* get BER Monitor information */
		rtn = mb86a35_IOCTL_BER_MONIGET(cmdctrl, cmd, arg);
		break;

	case IOCTL_BER_MONISTOP:	/* BER Monitor STOP */
		rtn = mb86a35_IOCTL_BER_MONISTOP(cmdctrl, cmd, arg);
		break;

		/* TS Output */
	case IOCTL_TS_START:	/* Serial TS Output Start */
		rtn = mb86a35_IOCTL_TS_START(cmdctrl, cmd, arg);
		break;

	case IOCTL_TS_STOP:	/* Serial TS Output OFF */
		rtn = mb86a35_IOCTL_TS_STOP(cmdctrl, cmd, arg);
		break;

	case IOCTL_TS_CONFIG:	/* Serial TS Output Configuration */
		rtn = mb86a35_IOCTL_TS_CONFIG(cmdctrl, cmd, arg);
		break;

	case IOCTL_TS_PCLOCK:	/* set Serial TS Output Clock / Parallel TS Clock */
		rtn = mb86a35_IOCTL_TS_PCLOCK(cmdctrl, cmd, arg);
		break;

	case IOCTL_TS_OUTMASK:	/* set TS Output Mask OFF */
		rtn = mb86a35_IOCTL_TS_OUTMASK(cmdctrl, cmd, arg);
		break;

	case IOCTL_TS_INVERT:	/* TSEN / TSERR Invert */
		rtn = mb86a35_IOCTL_TS_INVERT(cmdctrl, cmd, arg);
		break;

		/* IRQ reason */
	case IOCTL_IRQ_GETREASON:	/* Get IRQ Reason */
		rtn = mb86a35_IOCTL_IRQ_GETREASON(cmdctrl, cmd, arg);
		break;

	case IOCTL_IRQ_SETMASK:	/* IRQ : Interrupt mask control */
		rtn = mb86a35_IOCTL_IRQ_SETMASK(cmdctrl, cmd, arg);
		break;

	case IOCTL_IRQ_TMCCPARAM_SET:	/* IRQ : TMCC parameter Interrupt setting */
		rtn = mb86a35_IOCTL_IRQ_TMCCPARAM_SET(cmdctrl, cmd, arg);
		break;

	case IOCTL_IRQ_TMCCPARAM_REASON:	/* IRQ : TMCC parameter Interrupt, get reason */
		rtn = mb86a35_IOCTL_IRQ_TMCCPARAM_REASON(cmdctrl, cmd, arg);
		break;

		/* C/N Monitor */
//	case IOCTL_CN_MONI:	/* C/N Monitoring (Auto / Manual) */
//		rtn = mb86a35_IOCTL_CN_MONI(cmdctrl, cmd, arg);
//		break;
	case IOCTL_CN_MONI_CONFIG_START:	/* C/N Monitoring Config and Start (Auto / Manual) */
		rtn = mb86a35_IOCTL_CN_MONI_CONFIG_START(cmdctrl, cmd, arg);
		break;
	case IOCTL_CN_MONI_GET:	/* C/N Monitoring Get status (Auto / Manual) */
		rtn = mb86a35_IOCTL_CN_MONI_GET(cmdctrl, cmd, arg);
		break;

		/* MER Monitor */
//	case IOCTL_MER_MONI:	/* MER Monitoring (Auto / Manual) */
//		rtn = mb86a35_IOCTL_MER_MONI(cmdctrl, cmd, arg);
//		break;
	case IOCTL_MER_MONI_CONFIG_START:	/* MER Monitoring Config and Start (Auto / Manual) */
		rtn = mb86a35_IOCTL_MER_MONI_CONFIG_START(cmdctrl, cmd, arg);
		break;
	case IOCTL_MER_MONI_GET:	/* MER Monitoring Get status (Auto / Manual) */
		rtn = mb86a35_IOCTL_MER_MONI_GET(cmdctrl, cmd, arg);
		break;

		/* Hight speed Channel Search */
	case IOCTL_CH_SEARCH:	/* Hight speed Channel Search */
		rtn = mb86a35_IOCTL_CH_SEARCH(cmdctrl, cmd, arg);
		break;

		/* MB86A35(RF) */
		/* RF Controled */
	case IOCTL_RF_INIT:	/* RF Initialize  UHF/VHF RF-IC Register Initialize */
		rtn = mb86a35_IOCTL_RF_INIT(cmdctrl, cmd, arg);
		break;

	case IOCTL_RF_FUSEVALUE:	/* RF FUSE VALUE (p9 5.2) */
		rtn = mb86a35_IOCTL_RF_FUSEVALUE(cmdctrl, cmd, arg);
		break;

	case IOCTL_RF_CALIB_DCOFF:	/* CALIB DCOFFSET (p9 5.3) */
		rtn = mb86a35_IOCTL_RF_CALIB_DCOFF(cmdctrl, cmd, arg);
		break;

	case IOCTL_RF_CALIB_CTUNE:	/* CALIB CTUNE (p12 5.4) */
		rtn = mb86a35_IOCTL_RF_CALIB_CTUNE(cmdctrl, cmd, arg);
		break;

	case IOCTL_RF_CHANNEL:	/* set UHF/VHF Channel (p15 5.5) */
		rtn = mb86a35_IOCTL_RF_CHANNEL(cmdctrl, cmd, arg);
		break;

	case IOCTL_RF_PDOWN:	/* set RF POWER DOWN (p19 5.6) */
		rtn = mb86a35_IOCTL_RF_PDOWN(cmdctrl, cmd, arg);
		break;

	case IOCTL_RF_RSSIMONI:	/* RSSI Monitor (p19 5.7) */
		rtn = mb86a35_IOCTL_RF_RSSIMONI(cmdctrl, cmd, arg);
		break;

		/* Power Control */
	case IOCTL_POWERCTRL:	/* Power Control */
		rtn = mb86a35_IOCTL_POWERCTRL(cmdctrl, cmd, arg);
		break;

		/* I2C Control */
	case IOCTL_I2C_MAIN:	/* I2C : I2C main access */
		rtn = mb86a35_IOCTL_I2C_MAIN(cmdctrl, cmd, arg);
		break;

	case IOCTL_I2C_SUB:	/* I2C : I2C sub access */
		rtn = mb86a35_IOCTL_I2C_SUB(cmdctrl, cmd, arg);
		break;

	case IOCTL_I2C_RF:	/* I2C : I2C RF-IC access */
		rtn = mb86a35_IOCTL_I2C_RF(cmdctrl, cmd, arg);
		break;

		/* SPI Control */
	case IOCTL_SPI_MAIN:	/* SPI : SPI main access */
		rtn = mb86a35_IOCTL_SPI_MAIN(cmdctrl, cmd, arg);
		break;

	case IOCTL_SPI_SUB:	/* SPI : SPI sub access */
		rtn = mb86a35_IOCTL_SPI_SUB(cmdctrl, cmd, arg);
		break;

	case IOCTL_SPI_RF:	/* SPI : SPI RF-IC access */
		rtn = mb86a35_IOCTL_SPI_RF(cmdctrl, cmd, arg);
		break;

	case IOCTL_HRM:	/* HRM : HRM Control */
		rtn = mb86a35_IOCTL_HRM(cmdctrl, cmd, arg);
		break;	

	case IOCTL_LOW_UP_IF:	/* LOWER UPEER IF : LOWER UPEER IF Control */
		rtn = mb86a35_IOCTL_LOW_UP_IF(cmdctrl, cmd, arg);
		break;			
		
	case IOCTL_STREAM_READ:	/* Stream Read */
		rtn = mb86a35_IOCTL_STREAM_READ(cmdctrl, cmd, arg);
		break;			
		
	case IOCTL_STREAM_READ_CTRL:	/* Stream Read Control */
		rtn = mb86a35_IOCTL_STREAM_READ_CTRL(cmdctrl, cmd, arg);
		break;			
		
	case IOCTL_SPI_CONFIG:	/* SPI config setting */
		rtn = mb86a35_IOCTL_SPI_CONFIG(cmdctrl, cmd, arg);
		break;			
		
	case IOCTL_TS_SETUP:	/* TS control setting */
		rtn = mb86a35_IOCTL_TS_SETUP(cmdctrl, cmd, arg);
		break;			
	case IOCTL_SELECT_ANTENNA:	/* Select Antenna Path */
		rtn = mb86a35_IOCTL_SELECT_ANTENNA(cmdctrl, cmd, arg);
		break;
	default:
		DBGPRINT(PRINT_LHEADERFMT
			 " : Illegal command operation. cmd[ 0x%04x ].\n",
			 PRINT_LHEADER, cmd);
		return -EBADRQC;
		break;
	}
	//DBGPRINT(PRINT_LHEADERFMT
	//	 " : ioctl(inode:0x%08x,filp:0x%08x,cmd:0x%08x,arg:0x%08x)  return( %d ).\n",
	//	 PRINT_LHEADER, (int)inode, (int)filp, cmd, (int)arg, rtn);
	//printk("ioctl(cmd:0x%08x)  return( %d ).\n", cmd, rtn);
	return (rtn);
}

static struct file_operations mb86a35_fops = {
	owner:	THIS_MODULE,		/* owner */
	read:	mb86a35_read,		/* read() system call entry */
	write:	mb86a35_write,		/* write() system call entry */
#if 0//def KERNEL_2635
	//ioctl:	mb86a35_ioctl,		/* ioctl() system call entry */
#else//def KERNEL_2638
	unlocked_ioctl:	mb86a35_ioctl,	/* unlocked_ioctl() system call entry */
#endif
	open:	mb86a35_open,		/* open() system call entry */
	release:mb86a35_close,		/* close() system call entry */
};

//    
static int broadcast_mmbi_device_init(struct broadcast_mmbi_chdevice *pbroadcast, int index)
{
	int rc;

	cdev_init(&pbroadcast->cdev, &mb86a35_fops);

	pbroadcast->cdev.owner = THIS_MODULE;

	rc = cdev_add(&pbroadcast->cdev, broadcast_mmbi_dev, 1);

	pbroadcast->dev = device_create(broadcast_mmbi_class, NULL, MKDEV(MAJOR(broadcast_mmbi_dev), 0),
					 NULL, "broadcast%d", index);

	printk("broadcast_mmbi_device_add add add%d broadcast_mmbi_dev = %d \n", rc, MKDEV(MAJOR(broadcast_mmbi_dev), 0));
	
	if (IS_ERR(pbroadcast->dev)) {
		rc = PTR_ERR(pbroadcast->dev);
		pr_err("device_create failed: %d\n", rc);
		rc = -1;
	}
	
	printk("broadcast_mmbi_device_init start %d\n", rc);

	return rc;
}

static int broadcast_mmbi_drv_start(void)
{
	struct broadcast_mmbi_chdevice *pbroadcast = NULL;
	int rc = -ENODEV;
	
	if (!broadcast_mmbi_class) {

		broadcast_mmbi_class = class_create(THIS_MODULE, "broadcast_mmbi");
		if (IS_ERR(broadcast_mmbi_class)) {
			rc = PTR_ERR(broadcast_mmbi_class);
			pr_err("broadcast_mmbi_class: create device class failed: %d\n",
				rc);
			return rc;
		}

		rc = alloc_chrdev_region(&broadcast_mmbi_dev, 0, BROADCAST_MMBI_NUM_DEVS, "broadcast_mmbi");
		printk("broadcast_mmbi_drv_start add add%d broadcast_mmbi_dev = %d \n", rc, broadcast_mmbi_dev);
		if (rc < 0) {
			pr_err("broadcast_class: failed to allocate chrdev: %d\n",
				rc);
			return rc;
		}
	}

	pbroadcast = &mmbi_dev;
	
	rc = broadcast_mmbi_device_init(pbroadcast, 0);
	if (rc < 0) {
		return rc;
	}
	
	printk("broadcast_tdmb_drv_start start %d\n", rc);

	return rc;
}



/************************************************************************
 * irq_handler().
 * [User Interface]
 *     irqreturn_t mb86a35_irq_handler(int irq, void *dev);
 */
/**
	Interrupt handler.
	Device Driver for Multi mode tuner module. (MB86A35)

	@param	irq		[in] requested IRQ.
	@param	dev		[in] pointer to the device data.
	@retval	IRQ_HANDLED	interruput is occured.
*/

static
void mb86a35s_irq_work(struct work_struct *work);
#if 0
//struct timeval mytime;


static irqreturn_t mb86a335s_irq_check_time(int irq, void* dev)
{
	isr_time = ktime_get();
	return IRQ_HANDLED; 
}
#endif



static
irqreturn_t mb86a35s_irq_handler(int irq, void *dev)
{
	struct ISDBT_MMBI_CTRL* pIsdbtInfo;

	pIsdbtInfo = (struct ISDBT_MMBI_CTRL *)dev;	
	if ( pIsdbtInfo && pIsdbtInfo->IsdbtPowerOnState )
	{
		if (/*(pIsdbtInfo->spi_irq_status)||*/(handler_enable_flag == 0))
		{			
//			printk("########### broadcast_tdmb_spi_isr ###########\n");
			printk("######### spi read function is so late skip #########\n");			
			return IRQ_HANDLED;
		}	
//		isr_time = ktime_get();
		//                                                                                
#ifdef MMB_WQ_FEATURE
		queue_work(pIsdbtInfo->spi_wq, &pIsdbtInfo->spi_work);
#else
		mb86a35s_irq_work((struct work_struct *)NULL);
#endif

	}
	else
	{
		printk("isdbt_isr is called, but device is off state\n");
	}

	return IRQ_HANDLED; 
}

/* Because mb86a35s_spi_sub_* might do sleep, workqueue is used. */

static
void mb86a35s_irq_work(struct work_struct *work)
{
	int ret = 0;

	unsigned char rega	= MB86A35S_REG_ADDR_SPI_SUBA;
	unsigned char sreg	= 0;
	unsigned char regd	= MB86A35S_REG_ADDR_SPI_SUBD;
	unsigned char value	= 0;
	u8 *_bufp		= mmap_area;
	
	struct ISDBT_MMBI_CTRL *pIsdbtWorkData = NULL;
	int free_size = 0;
#if 0
	s64 cur_diff1 = 0;
	s64 cur_diff2 = 0;
	s64 cur_diff3 = 0;
#endif
#ifdef PM_QOS
	{
		if(cpu_state == 0)
		{
			struct cpumask cpus;
			printk("%s: Enter  Set CPU Affinity only to cpu0\n", __func__);
			cpumask_clear(&cpus);
			cpumask_set_cpu(0, &cpus);		 
			if (sched_setaffinity(current->pid, &cpus))
				printk("%s: dhd_dpc() set CPU affinity failed\n",__func__);
			else
				printk("%s: dhd_dpc() set CPU affinity Succeed  PID = %d\n",__func__, current->pid);
			cpu_state = 1;

		}
	}
#endif	
#ifdef MMB_WQ_FEATURE
	pIsdbtWorkData = container_of(work, struct ISDBT_MMBI_CTRL, spi_work);
#else
	pIsdbtWorkData = &MMBICtrlInfo;
#endif
#if 0
	workQ_time = ktime_get();

	diff1 = ktime_sub( workQ_time , isr_time );
	cur_diff1 =  ktime_to_ns(diff1)>>10;

	if(max_diff1 == 0)
	{
		max_diff1 = cur_diff1;
	}
	else
	{
		if(cur_diff1 > max_diff1)
		{
			max_diff1 = cur_diff1;
			printk("diff1 = %lld \n", max_diff1);
		}
	}
#endif
#ifdef PM_QOS
	if (pm_qos_request_active(&mmb_pm_qos_list)){
		 pm_qos_update_request(&mmb_pm_qos_list, 20 /*under 100*/);
	}
#endif
	//printk("START\n");

	/* IRQ Disable	*/
	//	value |= PARAM_IRQMASK_STREAMREAD_OFF;
	value	= 0xff;
	sreg	= MB86A35S_REG_SUBR_SPIS0_IRQMASK;
	
	ret = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);

	if (ret != 0) {
		printk(KERN_INFO "mb86a35_irq_handler spi send error. \n");
		mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
		mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
		goto stream_read_ctrl_return;
	}
	
	if((pIsdbtWorkData) && (handler_enable_flag == 1) )
	{
		pIsdbtWorkData->spi_irq_status = TRUE;
		
	//max 4K
		if(read_size >= QC_SPI_MAX)
		{
			int count = 0;
			int remain_size = 0;
			int i;

			count = read_size/(QC_SPI_MAX);
			//count +=1;
			remain_size = read_size%(QC_SPI_MAX);

			for(i=0;i<count;i++)
			{
				/* Stream Data Read below 4K	*/
				_bufp		= mmap_area;
				
				/* Stream Read	*/
				ret = mb86a35s_spi_stream_recv(MB86A35S_REG_ADDR_STREAMREAD, _bufp, QC_SPI_MAX);
				if (ret != 0) {
					printk(KERN_INFO "mb86a35_irq_handler stream recv error 1. \n");
					mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					goto stream_read_ctrl_return;
				}

				/* Stream Read End command	*/
			 	ret = mb86a35s_spi_send(0xa0, 0xff, MB86A35S_SPI_WRITE_1BYTE);
				if (ret != 0) {
					printk(KERN_INFO "mb86a35_irq_handler stream end command error. \n");
					mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					goto stream_read_ctrl_return;
				}
			
				free_size = mbt_dataring_free(buf_id);

				if(free_size >= (QC_SPI_MAX))
				{
					ret = mbt_dataring_write(buf_id, _bufp +3, QC_SPI_MAX); //header skip
					if(ret < 0)
					{
						printk("data ring buffer write fail1 %d \n", ret);

						mbt_dataring_flush(buf_id);
						ret = mbt_dataring_write(buf_id, _bufp +3, QC_SPI_MAX); 
						if(ret < 0)
						{
							mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
							mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
							printk("data ring buffer write retry fail 1 %d \n", ret);
							goto stream_read_ctrl_return;
						}
					}
				}
				else
				{
					printk("buffer full 1\n");
					mbt_dataring_flush(buf_id);
					ret = mbt_dataring_write(buf_id, _bufp +3, QC_SPI_MAX); //header skip
					if(ret < 0)
					{
						printk("data ring buffer write fail2 %d \n", ret);
						mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
						mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
						
					}
				}
				#if 0
				for(j = 0; j < 188*20; j += 188)
				{
					if(_bufp[j+3] != 0x47)
					{
						//*outSize = 0;	// temp patch for high bit stream transfer taew00k.kang 2011-09-02
						printk("[ISDB-T][DtvReadTS]SYNC ERR buf[%d]: 0x%x \n",j,_bufp[j+3]);
						printk("[ISDB-T][DtvReadTS]SYNC ERR buf[%d]: 0x%x \n",j+1,_bufp[j+1+3]);
					}
				}
				#endif

				
			}

			if(remain_size > 0)
			{
				/* Remain Stream Data Read	*/
				_bufp		= mmap_area;
				
				ret = mb86a35s_spi_stream_recv(MB86A35S_REG_ADDR_STREAMREAD, _bufp, remain_size);
				if (ret != 0) {
					printk(KERN_INFO "mb86a35_irq_handler stream recv error 2. \n");
					mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					goto stream_read_ctrl_return;
				}

				
				/* Stream Read End command	*/
				ret = mb86a35s_spi_send(0xa0, 0xff, MB86A35S_SPI_WRITE_1BYTE);
				if (ret != 0) {
					printk(KERN_INFO "mb86a35_irq_handler stream end command error. \n");
					mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					goto stream_read_ctrl_return;
				}
		
				free_size = mbt_dataring_free(buf_id);

				if(free_size >= remain_size)
				{
					ret = mbt_dataring_write(buf_id, _bufp +3, remain_size); //header skip
					if(ret < 0)
					{
						printk("data ring buffer write fail3 %d \n", ret);
						
						mbt_dataring_flush(buf_id);
						ret = mbt_dataring_write(buf_id, _bufp +3, remain_size); 
						if(ret < 0)
						{
							printk("data ring buffer write retry fail 2 %d \n", ret);
							mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
							mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
							goto stream_read_ctrl_return;
						}
						
					}
				}
				else
				{
					printk("buffer full 2\n");
					mbt_dataring_flush(buf_id);
					ret = mbt_dataring_write(buf_id, _bufp +3, remain_size); //header skip
					if(ret < 0)
					{
						printk("data ring buffer write fail4 %d \n", ret);
						mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
						mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					}
				}
				#if 0 // dump test
				for(j = 0; j < remain_size; j += 188)
				{
					if(_bufp[j+3] != 0x47)
					{
						//*outSize = 0;	// temp patch for high bit stream transfer taew00k.kang 2011-09-02
						printk("[ISDB-T][DtvReadTS]SYNC ERR buf[%d]: 0x%x\n",j,_bufp[j+3]);
						printk("[ISDB-T][DtvReadTS]SYNC ERR buf[%d]: 0x%x\n",j+1,_bufp[j+1+3]);
					}
				}
				#endif
			}
		}
		else
		{
			/* Stream Read	*/
			ret = mb86a35s_spi_stream_recv(MB86A35S_REG_ADDR_STREAMREAD, _bufp, read_size);
			if (ret != 0) {
				printk(KERN_INFO "mb86a35_irq_handler stream recv error. \n");
				mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
				mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
				goto stream_read_ctrl_return;
			}

			/* Stream Read End command	*/
			ret = mb86a35s_spi_send(0xa0, 0xff, MB86A35S_SPI_WRITE_1BYTE);
			if (ret != 0) {
				printk(KERN_INFO "mb86a35_irq_handler stream end command error. \n");
				mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
				mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
				goto stream_read_ctrl_return;
			}

			free_size = mbt_dataring_free(buf_id);

			if(free_size >= read_size)
			{
				ret = mbt_dataring_write(buf_id, _bufp +3, read_size); //header+write size skip
				if(ret < 0)
				{
					mbt_dataring_flush(buf_id);
					ret = mbt_dataring_write(buf_id, _bufp +3, read_size); //header skip
					if(ret < 0)
					{
						printk("data ring buffer write retry fail5 %d \n", ret);
						mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
						mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					}
				}
			}
			else
			{
				printk("\nbuffer full\n");
				mbt_dataring_flush(buf_id);
				ret = mbt_dataring_write(buf_id, _bufp +3, read_size); //header+write size skip
				if(ret < 0)
				{
					printk("data ring buffer write fail6 %d \n", ret);
					mb86a35s_spi_send(0x93, 0x01, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
					mb86a35s_spi_send(0x93, 0x00, MB86A35S_SPI_WRITE_1BYTE); //buf reset!!!
				}
			}
		}
	
		pIsdbtWorkData->spi_irq_status = FALSE;
	}
	
stream_read_ctrl_return:

	/* IRQ Enable	*/
	//	value &= ~(PARAM_IRQMASK_STREAMREAD_OFF);
	value	= 0xbf;
	sreg	= MB86A35S_REG_SUBR_SPIS0_IRQMASK;
	ret = mb86a35s_spi_sub_send(rega, sreg, regd, value, MB86A35S_SPI_WRITE_1BYTE);
	if (ret != 0) {
		printk(KERN_INFO "mb86a35_irq_handler spi send error. \n");
	}

	//printk("END\n");
#ifdef PM_QOS
	if (pm_qos_request_active(&mmb_pm_qos_list)){
			pm_qos_update_request(&mmb_pm_qos_list, PM_QOS_DEFAULT_VALUE);
	   }
#endif
#if 0
	read_done_time = ktime_get();

	diff3 = ktime_sub( read_done_time , spi_time );
	cur_diff3 =  ktime_to_ns(diff3)>>10;

	if(max_diff3 == 0)
	{
		max_diff3 = cur_diff3;
	}
	else
	{
		if(cur_diff3 > max_diff3)
		{
			max_diff3 = cur_diff3;
			printk("diff3 = %lld \n", cur_diff3);
		}
	}
#endif
	return;
}

//         
extern int broadcast_mmbi_chk_dev_drv_start(void);
//end
/************************************************************************/
static int __devinit mb86a35s_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	int irq = 0;
	unsigned long irqflags = 0;
	char *name = "SPI XIRQ";

	DBGPRINT(PRINT_LHEADERFMT "Called. \n", PRINT_LHEADER);

	//for lge static dev/drv
	#if 0
	ret = misc_register(&mb86a35s_misc_device);
	//if ((ret = register_chrdev(devmajor, devname, &mb86a35_fops))) {
	if (ret < 0) {
		printk(PRINT_LHEADERFMT
		       " : register_chrdev failed. [ \"%s\" ], ret[ %d ]\n",
		       PRINT_LHEADER, devname, ret);
		return -EBUSY;
	} else {
		printk(PRINT_LHEADERFMT
		       " : register_chrdev Successful. [ \"%s\" ], ret[ %d ]\n",
		       PRINT_LHEADER, devname, ret);
	}
	#endif

	broadcast_mmbi_drv_start();

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret < 0) {
		printk(PRINT_LHEADERFMT " : spi_setup failed. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
		goto ERROR1;
	}

	//mb86a35s_spi_device = spi;

	/* memory alloc steram read buffer area	*/
	mmap_area = (unsigned char *)dma_alloc_coherent(NULL, MB86A35S_READ_SIZE_MAX, (dma_addr_t *)&map_physicaladdr, GFP_KERNEL);
	if (!mmap_area) {
		ret = -ENOMEM;
		printk(PRINT_LHEADERFMT " : dma_malloc_coherent failed. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
		goto ERROR1;
	}

	/* initalize map area	*/
	memset(mmap_area, 0, MB86A35S_READ_SIZE_MAX);

#ifdef PERFORMANCE_TEST
	/* for TEST	*/
	gpio_request(GPIO_SPI_TEST1, name);
	printk(PRINT_LHEADERFMT " : gpio_request success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);

	gpio_export(GPIO_SPI_TEST1, 1);
	printk(PRINT_LHEADERFMT " : gpio_export success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);

	gpio_direction_input(GPIO_SPI_TEST1);
	printk(PRINT_LHEADERFMT " : gpio_direction_input success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);

#endif /* PERFORMANCE_TEST	*/

	ret = gpio_request(GPIO_SPI_XIRQ, name);
	if (ret < 0) {
		printk(PRINT_LHEADERFMT " : gpio_request failed. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
		goto ERROR2;
	}
	else {
		printk(PRINT_LHEADERFMT " : gpio_request success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
	}

	ret = gpio_export(GPIO_SPI_XIRQ, 1);
	if (ret < 0) {
		printk(PRINT_LHEADERFMT " : gpio_export failed. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
		goto ERROR3;
	}
	else {
		printk(PRINT_LHEADERFMT " : gpio_export success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
	}
#if 1
	ret = gpio_direction_input(GPIO_SPI_XIRQ);
	if (ret < 0) {
		printk(PRINT_LHEADERFMT " : gpio_direction_input failed. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
		goto ERROR3;
	}
	else {
		printk(PRINT_LHEADERFMT " : gpio_direction_input success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
	}

	irq = gpio_to_irq(GPIO_SPI_XIRQ);
	if (irq < 0) {
		printk(PRINT_LHEADERFMT " : gpio_to_irq failed. [ \"%s\" ], irq[ %d ]\n", PRINT_LHEADER, devname, irq);
		goto ERROR3;
	}
	else {
		printk(PRINT_LHEADERFMT " : gpio_to_irq success. [ \"%s\" ], irq [ %d ] ret[ %d ]\n", PRINT_LHEADER, devname, irq, ret);
	}

#endif
	
#if 1 //   
	MMBICtrlInfo.pSpiDevice = spi;

	MMBICtrlInfo.pSpiDevice->irq = irq;

#ifdef MMB_WQ_FEATURE
	INIT_WORK(&MMBICtrlInfo.spi_work, mb86a35s_irq_work);
	MMBICtrlInfo.spi_wq = create_singlethread_workqueue("isdbt_spi_wq");
	if(MMBICtrlInfo.spi_wq == NULL){
		printk("Failed to setup MMBICtrlInfo spi workqueue \n");
		return -ENOMEM;
	}
#endif
	irqflags = IRQF_DISABLED | IRQF_TRIGGER_FALLING ;
#ifndef MMB_WQ_FEATURE
	ret = request_threaded_irq(irq, NULL, mb86a35s_irq_handler, irqflags, name, &MMBICtrlInfo);
#else
	ret = request_irq(irq, mb86a35s_irq_handler, irqflags, name, &MMBICtrlInfo);
#endif
	if (ret < 0) {
		printk(PRINT_LHEADERFMT " : request_irq failed. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, irq);
		goto ERROR3;
	}
	else {
		printk(PRINT_LHEADERFMT " : request_irq success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
	}

	if (MMBICtrlInfo.pSpiDevice == NULL)
	{
		printk("MMBICtrlInfo_interrupt_lock fail\n");
	}

	isdbt_hw_setting();

	//disable_irq(MMBICtrlInfo.pSpiDevice->irq);

	mutex_init(&MMBICtrlInfo.mutex);
	wake_lock_init(&MMBICtrlInfo.wake_lock,  WAKE_LOCK_SUSPEND, dev_name(&spi->dev));

	
	//                                                         
	//wdata_buf = (u8*)(((u32)wx_data_buf +31)&~31);
	//rdata_buf = (u8*)(((u32)rx_data_buf +31)&~31);
	wdata_buf = wx_data_buf;
	rdata_buf = rx_data_buf;
	//                                                         

	broadcast_mmbi_chk_dev_drv_start();

#ifdef PM_QOS
	pm_qos_add_request(&mmb_pm_qos_list, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
#endif

#else
	irqflags = IRQF_TRIGGER_FALLING;
	ret = request_irq(irq, mb86a35s_irq_handler, irqflags, name, spi);
	if (ret < 0) {
		printk(PRINT_LHEADERFMT " : request_irq failed. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, irq);
		goto ERROR3;
	}
	else {
		printk(PRINT_LHEADERFMT " : request_irq success. [ \"%s\" ], ret[ %d ]\n", PRINT_LHEADER, devname, ret);
	}

#endif

	return ret;

ERROR3:
	gpio_free(GPIO_SPI_XIRQ);
ERROR2:

#ifdef PERFORMANCE_TEST
	/* for TEST	*/
	gpio_free(GPIO_SPI_TEST1);
#endif
	dma_free_coherent(NULL, MB86A35S_READ_SIZE_MAX, mmap_area, map_physicaladdr);
ERROR1:
	unregister_chrdev(devmajor, devname);
//ERROR0:

	return ret;
}

static int __devexit mb86a35s_spi_remove(struct spi_device *spi)
{
	DBGPRINT(PRINT_LHEADERFMT "Called. \n", PRINT_LHEADER);

	free_irq(gpio_to_irq(GPIO_SPI_XIRQ), spi);

	gpio_free(GPIO_SPI_XIRQ);

#ifdef PERFORMANCE_TEST
	/* for TEST	*/
	gpio_free(GPIO_SPI_TEST1);
#endif
/* end TEST	*/

	//mb86a35s_spi_device = NULL;

	dma_free_coherent(NULL, MB86A35S_READ_SIZE_MAX, mmap_area, map_physicaladdr);

	unregister_chrdev(devmajor, devname);
	printk(PRINT_LHEADERFMT " : unregister_chrdev Successful. [ \"%s\" ]\n", PRINT_LHEADER, devname);
	return 0;
}

static struct spi_driver mb86a35s_spi_driver = {
	.driver = {
		   .name = "mb86a35s",
		   .owner = THIS_MODULE,
		   },
	.probe    = mb86a35s_spi_probe,
	.remove   = mb86a35s_spi_remove,
};

/************************************************************************
 * initialization module
 *  called by insmod function.
 */
/**
	__init System Call. (called by Kernel.)
	Device Driver for Multi mode tuner module. (MB86A35)
	It is called as an initialization processing module.

	@retval	0	normal return.
	@retval	<0	The error occurred. The detailed information is set to an errno.
*/
static
int __init proc_init_module(void)
{
	int ret = 0;

	printk(PRINT_LHEADERFMT "Called.  Module is now loaded.\n",
	       PRINT_LHEADER);

	MB86A35_DEBUG = 0;
#ifdef	DEBUG
	if (mode != NULL) {
		if ((strlen(mode) == strlen("DEBUG"))
		    && (strncmp(mode, "DEBUG", strlen("DEBUG")) == 0)) {
			MB86A35_DEBUG = 1;
		}
	}
#endif
#ifdef	DEBUG_PRINT_MEM
	if (MB86A35_DEBUG == 1) {
		LOGMEM =
		    (unsigned char *)kmalloc(DEBUG_PRINT_MEM_SIZE, GFP_KERNEL);
		if (LOGMEM == NULL) {
			printk(PRINT_ERROR PRINT_LHEADERFMT
			       " : kernel memory is short[ 0x%08x ]. [LOGGING]\n",
			       PRINT_LHEADER, (int)LOGMEM);
			return -ENOMEM;
		}
		memset(LOGMEM, 0, DEBUG_PRINT_MEM_SIZE);
	}
#endif

#if 0
	if ((ret = register_chrdev(devmajor, devname, &mb86a35_fops))) {
		printk(PRINT_LHEADERFMT
		       " : register_chrdev failed. [ \"%s\" ], ret[ %d ]\n",
		       PRINT_LHEADER, devname, ret);
		return -EBUSY;
	} else {
		printk(PRINT_LHEADERFMT
		       " : register_chrdev Successful. [ \"%s\" ], ret[ %d ]\n",
		       PRINT_LHEADER, devname, ret);
	}

	ret = mb86a35_probe(NULL);
#endif

	ret = spi_register_driver(&mb86a35s_spi_driver);
	if (ret < 0) {
		ERRPRINT("failed to register SPI driver: %d", ret);
		return -EBUSY;
	} else {
		printk(PRINT_LHEADERFMT
		       " : spi_register_driver Successful. [ \"%s\" ], ret[ %d ]\n",
		       PRINT_LHEADER, devname, ret);
	}

	printk(PRINT_LHEADERFMT " : loaded  into  kernel.  %s\n",
	       PRINT_LHEADER, ((MB86A35_DEBUG == 1) ? "  [DEBUG]" : ""));

	return ret;
}

/************************************************************************
 * cleanup module.
 *  called  by rmmod function.
 */
/**
	__exit System Call. (called by Kernel.)
	Device Driver for Multi mode tuner module. (MB86A35)
	It is called as an end processing module.
*/

static
void __exit proc_cleanup_module(void)
{

#if 0
	/* mb86a35 removed. */
	mb86a35_remove(NULL);
#endif
	printk(PRINT_LHEADERFMT " : removed from kernel.  %s\n",
	       PRINT_LHEADER, ((MB86A35_DEBUG == 1) ? "  [DEBUG]" : ""));


	spi_unregister_driver(&mb86a35s_spi_driver);
	printk(PRINT_LHEADERFMT
	       " : spi_unregister_driver Successful. [ \"%s\" ]\n",
	       PRINT_LHEADER, devname);

#if 0
	unregister_chrdev(devmajor, devname);
	printk(PRINT_LHEADERFMT
	       " : unregister_chrdev Successful. [ \"%s\" ]\n",
	       PRINT_LHEADER, devname);
#endif

	printk(PRINT_LHEADERFMT "Called.  Module is now unloaded.\n",
	       PRINT_LHEADER);

#ifdef	DEBUG_PRINT_MEM
	if (LOGMEM != NULL) {
		kfree(LOGMEM);

		LOGMEM = NULL;
	}
#endif

}

/* Declare entry and exit functions */
module_init(proc_init_module);
module_exit(proc_cleanup_module);
/* End of Program */
