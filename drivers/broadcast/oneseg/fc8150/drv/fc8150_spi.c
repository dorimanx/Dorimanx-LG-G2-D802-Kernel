/*****************************************************************************
 Copyright(c) 2012 FCI Inc. All Rights Reserved
 
 File name : fc8150_spi.c
 
 Description : fc8150 host interface

*******************************************************************************/
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <mach/board_lge.h>

#include "fci_types.h"
#include "fc8150_regs.h"
#include "fci_oal.h"

#if defined(CONFIG_MACH_MSM8974_G2_DCM)
#include <linux/clk.h>
static struct clk *pm8941_xo_a2_192000_clk = NULL;
#endif

#define SPI_BMODE           0x00
#define SPI_WMODE           0x10
#define SPI_LMODE           0x20
#define SPI_READ            0x40
#define SPI_WRITE           0x00
#define SPI_AINC            0x80
#define CHIPID              (0 << 3)

#define DRIVER_NAME "isdbt"

//#define QUP_GSBI_SPI_USE_DMOV

struct spi_device *fc8150_spi = NULL;

static u8 tx_data[10];

#ifdef QUP_GSBI_SPI_USE_DMOV
#define TX_RX_DATA_BUF_SIZE (8192)

static u8 *wdata_buf;
static u8 *rdata_buf;
static u8 wx_data_buf[TX_RX_DATA_BUF_SIZE+32] ={0,};
static u8 rx_data_buf[TX_RX_DATA_BUF_SIZE+32] = {0,};
#else
static u8 rdata_buf[8192]={0};
static u8 wdata_buf[8192]={0};
#endif

#define BUFFER_SIZE 4<<10
struct spi_message spi_msg;

struct spi_transfer spi_xfer;
u8* 	tx_buf; //This needs to be DMA friendly buffer

static DEFINE_MUTEX(lock);

//#ifdef CONFIG_OF //Open firmware must be defined for dts useage
static struct of_device_id fci_spi_fc8150_table[] = {
{
	.compatible = "fci,fc8150-spi",}, //Compatible node must match dts
	{ },
};
/*
static int spi_test_transfer(struct spi_device *spi)
{
	spi_message_init(&spi_msg);
//	spi_xfer.tx_buf = tx_buf;
	spi_xfer.tx_buf =&wdata_buf[0];
	spi_xfer.len = BUFFER_SIZE;
	spi_xfer.bits_per_word = 8;
	spi_xfer.speed_hz = spi->max_speed_hz;

	spi_message_add_tail(&spi_xfer, &spi_msg);
	return spi_sync(spi, &spi_msg);
}
*/

static int __devinit fc8150_spi_probe(struct spi_device *spi)
{
	s32 ret;
	int irq_gpio = -1;
	int irq;
	int cs;
	int cpha,cpol,cs_high;
	u32 max_speed;


	PRINTF(0, "fc8150_spi_probe\n");
	dev_err(&spi->dev, "[1seg]%s\n", __func__);

	//allocate memory for transfer
	/*
	tx_buf = kmalloc(BUFFER_SIZE, GFP_ATOMIC);
	if(tx_buf == NULL){
	dev_err(&spi->dev, "%s: mem alloc failed\n", __func__);
	return -ENOMEM;
	}
	*/
	//Parse data using dt.
	if(spi->dev.of_node){
		irq_gpio = of_get_named_gpio_flags(spi->dev.of_node, "fci_spi_fc8150,irq-gpio", 0, NULL);
	}
	irq = spi->irq;
	cs = spi->chip_select;
	cpha = ( spi->mode & SPI_CPHA ) ? 1:0;
	cpol = ( spi->mode & SPI_CPOL ) ? 1:0;
	cs_high = ( spi->mode & SPI_CS_HIGH ) ? 1:0;
	max_speed = spi->max_speed_hz;

	dev_err(&spi->dev, "[1seg] gpio [%d] irq [%d] gpio_irq [%d] cs [%x] CPHA [%x] CPOL [%x] CS_HIGH [%x]\n",
		irq_gpio, irq, gpio_to_irq(irq_gpio), cs, cpha, cpol, cs_high);

	dev_err(&spi->dev, "[1seg] Max_speed [%d]\n", max_speed );

	//Once you have a spi_device structure you can do a transfer anytime
	spi->bits_per_word = 8;
	spi->mode =  SPI_MODE_0;
/*

    //spi->max_speed_hz =  10800000; //kernel\arch\arm\mach-msm\clock-8960.c -> clk_tbl_gsbi_qup
	//spi->max_speed_hz =  24000000;
	//spi->max_speed_hz =  8000000;
	spi->bits_per_word = 8;
	spi->mode =  SPI_MODE_0;
*/
	spi->max_speed_hz =  19200000;
	ret = spi_setup(spi);

	if (ret < 0)
		return ret;

	fc8150_spi = spi;

	#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	if (lge_get_board_revno() >= HW_REV_D)
	{
		pm8941_xo_a2_192000_clk = clk_get(&fc8150_spi->dev, "xo");
		if ( IS_ERR(pm8941_xo_a2_192000_clk) )
		{
			int ret = 0;
			
			ret = PTR_ERR(pm8941_xo_a2_192000_clk);
			PRINTF(0,"[1seg] fc8150_spi_probe clk_get error %d!!!\n", ret);
		}
	}
	#endif

	return ret;
}

static int fc8150_spi_remove(struct spi_device *spi)
{

	return 0;
}

static struct spi_driver fc8150_spi_driver = {
	.driver = {
		.name		= "fci_spi_fc8150",//DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = fci_spi_fc8150_table,
	},
	.probe		= fc8150_spi_probe,
	.remove		= __devexit_p(fc8150_spi_remove),
};

static int fc8150_spi_write_then_read(struct spi_device *spi, u8 *txbuf, u16 tx_length, u8 *rxbuf, u16 rx_length)
{
	int res = 0;
	
	struct spi_message	message;
	struct spi_transfer	x;

	spi_message_init(&message);
	memset(&x, 0, sizeof x);

	spi_message_add_tail(&x, &message);
	
	memcpy(&wdata_buf[0], txbuf, tx_length);
	
	x.tx_buf=&wdata_buf[0];
	x.rx_buf=&rdata_buf[0];
	x.len = tx_length + rx_length;
	x.cs_change = 0; // 0 : CS is alternate per transfer, 1 : CS is alternate per 8/16/24/32 bit which is setted in spi_setup
	x.bits_per_word = 8;
	res = spi_sync(spi, &message);
	//PRINTF(0, "fc8150_spi_write_then_read x.len : %d res: %d\n", x.len, res);

	memcpy(rxbuf, x.rx_buf + tx_length, rx_length);
	
	return res;
}

static int spi_bulkread(HANDLE hDevice, u16 addr, u8 command, u8 *data, u16 length)
{
	int res;

	tx_data[0] = addr & 0xff;
	tx_data[1] = (addr >> 8) & 0xff;
	tx_data[2] = (command & 0xf0) | CHIPID | ((length >> 16) & 0x07);
	tx_data[3] = (length >> 8) & 0xff;
	tx_data[4] = length & 0xff;

	res = fc8150_spi_write_then_read(fc8150_spi, &tx_data[0], 5, data, length);

	if(res)
	{
		PRINTF(0, "fc8150_spi_bulkread fail : %d\n", res);
		return BBM_NOK;
	}
	
	return BBM_OK;
}

static int spi_bulkwrite(HANDLE hDevice, u16 addr, u8 command, u8* data, u16 length)
{
	int i;
	int res;

	tx_data[0] = addr & 0xff;
	tx_data[1] = (addr >> 8) & 0xff;
	tx_data[2] = (command & 0xf0) | CHIPID | ((length >> 16) & 0x07);
	tx_data[3] = (length >> 8) & 0xff;
	tx_data[4] = length & 0xff;
	
	for(i=0;i<length;i++)
	{
		tx_data[5+i] = data[i];
	}

	res = fc8150_spi_write_then_read(fc8150_spi, &tx_data[0], length+5, NULL, 0);

	if(res)
	{
		PRINTF(0, "fc8150_spi_bulkwrite fail : %d\n", res);
		return BBM_NOK;
	}

	return BBM_OK;
}

static int spi_dataread(HANDLE hDevice, u16 addr, u8 command, u8* data, u32 length)
{
	int res;
	
	tx_data[0] = addr & 0xff;
	tx_data[1] = (addr >> 8) & 0xff;
	tx_data[2] = (command & 0xf0) | CHIPID | ((length >> 16) & 0x07);
	tx_data[3] = (length >> 8) & 0xff;
	tx_data[4] = length & 0xff;

	res = fc8150_spi_write_then_read(fc8150_spi, &tx_data[0], 5, data, length);
	
	if(res)
	{
		PRINTF(0, "fc8150_spi_dataread fail : %d\n", res);
		return BBM_NOK;
	}

	return BBM_OK;
}

#if defined(CONFIG_MACH_MSM8974_G2_DCM) || defined(CONFIG_MACH_MSM8974_G2_KDDI)
struct clk *fc8150_spi_get_clk(void)
{
	#if defined(CONFIG_MACH_MSM8974_G2_DCM)
	return pm8941_xo_a2_192000_clk;
	#else
	return NULL;
	#endif
}
#endif

int fc8150_spi_init(HANDLE hDevice, u16 param1, u16 param2)
{
	int res = 0;

	PRINTF(0, "fc8150_spi_init : %d\n", res);
	
	res = spi_register_driver(&fc8150_spi_driver);
	
	if(res)
	{
		PRINTF(0, "fc8150_spi register fail : %d\n", res);
		return BBM_NOK;
	}
	
#ifdef QUP_GSBI_SPI_USE_DMOV
	wdata_buf = (u8*)(((u32)wx_data_buf +31)&~31);
	rdata_buf = (u8*)(((u32)rx_data_buf +31)&~31);
#endif
	
	return res;
}

int fc8150_spi_byteread(HANDLE hDevice, u16 addr, u8 *data)
{
	int res;
	u8 command = SPI_READ;

	mutex_lock(&lock);
	res = spi_bulkread(hDevice, addr, command, data, 1);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_wordread(HANDLE hDevice, u16 addr, u16 *data)
{
	int res;
	u8 command = SPI_READ | SPI_AINC;

	mutex_lock(&lock);
	res = spi_bulkread(hDevice, addr, command, (u8*)data, 2);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_longread(HANDLE hDevice, u16 addr, u32 *data)
{
	int res;
	u8 command = SPI_READ | SPI_AINC;

	mutex_lock(&lock);
	res = spi_bulkread(hDevice, addr, command, (u8*)data, 4);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_bulkread(HANDLE hDevice, u16 addr, u8 *data, u16 length)
{
	int res;
	u8 command = SPI_READ | SPI_AINC;

	mutex_lock(&lock);	
	res = spi_bulkread(hDevice, addr, command, data, length);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_bytewrite(HANDLE hDevice, u16 addr, u8 data)
{
	int res;
	u8 command = SPI_WRITE;

	mutex_lock(&lock);	
	res = spi_bulkwrite(hDevice, addr, command, (u8*)&data, 1);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_wordwrite(HANDLE hDevice, u16 addr, u16 data)
{
	int res;
	u8 command = SPI_WRITE | SPI_AINC;

	mutex_lock(&lock);
	res = spi_bulkwrite(hDevice, addr, command, (u8*)&data, 2);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_longwrite(HANDLE hDevice, u16 addr, u32 data)
{
	int res;
	u8 command = SPI_WRITE | SPI_AINC;
	
	mutex_lock(&lock);
	res = spi_bulkwrite(hDevice, addr, command, (u8*)&data, 4);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_bulkwrite(HANDLE hDevice, u16 addr, u8* data, u16 length)
{
	int res;
	u8 command = SPI_WRITE | SPI_AINC;

	mutex_lock(&lock);
	res = spi_bulkwrite(hDevice, addr, command, data, length);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_dataread(HANDLE hDevice, u16 addr, u8* data, u32 length)
{
	int res;
	u8 command = SPI_READ;

	mutex_lock(&lock);
	res = spi_dataread(hDevice, addr, command, data, length);
	mutex_unlock(&lock);
	
	return res;
}

int fc8150_spi_deinit(HANDLE hDevice)
{
	//spi_unregister_driver(&fc8150_spi_driver);
	return BBM_OK;
}

