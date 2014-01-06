#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include "tcc353x_common.h"
#include "tcpal_os.h"

/*
#define _USE_FF_FOR_ALL_DMA_
*/

I32S Tcc353xTccspiClose(I32S _moduleIndex);

static I32U gTccSpiHanleInit0 = 0;
static I32U gTccSpiHanleInit1 = 0;
static I32U gTccSpiHanleInited = 0;
static I08U gTccSpiChipAddr[4];

struct TcpalTcspiData_t 
{
    spinlock_t spin_lock;
    struct spi_device* spi_dev;
};
static struct TcpalTcspiData_t TcpalTcspiData;
struct spi_device *TCC_GET_SPI_DRIVER(void);
void Tcc353xTccspiInit(void);


static I32S Tcc353xTccspiSetup(I32S _moduleIndex)
{
	struct TcpalTcspiData_t *spiData;

	if (_moduleIndex >= 2) {
		TcpalPrintErr((I08S *) "Not supported, moduleidx=%d\n",
			      _moduleIndex);
		return TCC353X_RETURN_FAIL;
	}

	spiData = &TcpalTcspiData;
	memset(&TcpalTcspiData, 0, sizeof(TcpalTcspiData));

	spiData->spi_dev = TCC_GET_SPI_DRIVER();	
	Tcc353xTccspiInit();

	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xTccspiOpen(I32S _moduleIndex)
{
	I32S ret;
	ret = TCC353X_RETURN_FAIL;
	
	/* exception handling */
	if (_moduleIndex == 0) {
		if (gTccSpiHanleInit0 != 0 && gTccSpiHanleInit1 == 0)
			Tcc353xTccspiClose(_moduleIndex);
	} else {
		if (gTccSpiHanleInit1 != 0 && gTccSpiHanleInit0 == 0)
			Tcc353xTccspiClose(_moduleIndex);
	}

	/* normal process */
	if (_moduleIndex == 0)
		gTccSpiHanleInit0 = 1;
	else
		gTccSpiHanleInit1 = 1;

	if (gTccSpiHanleInited != 0) {
		return TCC353X_RETURN_SUCCESS;
	}

	gTccSpiHanleInited = 1;

	TcpalMemset(&gTccSpiChipAddr[_moduleIndex], 0x00, 4);

	ret = Tcc353xTccspiSetup(_moduleIndex);

	/* need reset */

	return ret;
}

I32S Tcc353xTccspiClose(I32S _moduleIndex)
{
	struct TcpalTcspiData_t *spiData = &TcpalTcspiData;

	if (_moduleIndex == 0)
		gTccSpiHanleInit0 = 0;
	else
		gTccSpiHanleInit1 = 0;

	if (gTccSpiHanleInit0 == 0 && gTccSpiHanleInit1 == 0) {
		gTccSpiHanleInited = 0;
		TcpalPrintStatus((I08S *)"spi_dev :0x%p\n", spiData->spi_dev);
	}
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xAdaptSpiReadWrite (I32S _moduleIndex, I08U * _bufferIn, I08U * _bufferOut,
		  I32S _size, I08U _reservedOption)
{
	int ret = 0;
	struct TcpalTcspiData_t *spiData = &TcpalTcspiData;
	
	struct spi_message msg;
	struct spi_transfer xfer = {
	    .tx_buf = _bufferIn,
	    .rx_buf = _bufferOut,
	    .len = _size,
	    .cs_change = 0,
	};

#ifdef _USE_FF_FOR_ALL_DMA_
	if(_size<8) {
		xfer.len+=48;
		TcpalMemset(&_bufferIn[_size], 0xff, _size+48);
	} else if (_size<16) {
		xfer.len+=40;
		TcpalMemset(&_bufferIn[_size], 0xff, _size+40);
	} else if (_size<24) {
		xfer.len+=32;
		TcpalMemset(&_bufferIn[_size], 0xff, _size+32);
	} else if (_size<32) {
		xfer.len+=24;
		TcpalMemset(&_bufferIn[_size], 0xff, _size+24);
	} else if (_size<40) {
		xfer.len+=16;
		TcpalMemset(&_bufferIn[_size], 0xff, _size+16);
	} else if (_size<48) {
		xfer.len+=8;
		TcpalMemset(&_bufferIn[_size], 0xff, _size+8);
	}	
#endif
	
	if(!spiData->spi_dev || !_size) return TCC353X_RETURN_FAIL;
	if(!_bufferIn && !_bufferOut) return TCC353X_RETURN_FAIL;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spiData->spi_dev, &msg);

	if(ret < 0) 
	    return TCC353X_RETURN_FAIL;
	
	return TCC353X_RETURN_SUCCESS;
}
