/****************************************************************************
 *   FileName    : tcc353x_linux_tccspi.c
 *   Description : tcc353x tcspi function for linux
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited to re-
distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS" and nothing contained in this source code shall 
constitute any express or implied warranty of any kind, including without limitation, any warranty 
of merchantability, fitness for a particular purpose or non-infringement of any patent, copyright 
or other third party intellectual property right. No warranty is made, express or implied, 
regarding the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, out of 
or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************/

#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include "tcc353x_common.h"
#include "tcpal_os.h"

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

	if(!spiData->spi_dev || !_size) return TCC353X_RETURN_FAIL;
	if(!_bufferIn && !_bufferOut) return TCC353X_RETURN_FAIL;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spiData->spi_dev, &msg);

	if(ret < 0) 
	    return TCC353X_RETURN_FAIL;
	
	return TCC353X_RETURN_SUCCESS;
}
