/****************************************************************************
 *   FileName    : tcc353x_Interrupt_Process.c
 *   Description : Interrupt process
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

#include "tcc353x_common.h"
#include "tcc353x_api.h"
#include "tcpal_os.h"
#include "tcc353x_user_defines.h"

#define USE_LGE_RING_BUFFER

#if defined (USE_LGE_RING_BUFFER) && !defined (_I2C_STS_)
extern void mbt_dataring_create(unsigned int* buffer_id, int len);
extern void mbt_dataring_destroy(unsigned int* buffer_id);
extern int mbt_dataring_empty(unsigned int buffer_id);
extern int mbt_dataring_free(unsigned int buffer_id);
extern int mbt_dataring_avail(unsigned int buffer_id);
extern void mbt_dataring_flush(unsigned int buffer_id);
extern int mbt_dataring_read(unsigned int buffer_id, char * buf, int len);
extern int mbt_dataring_write(unsigned int buffer_id, const char * buf, int len);
#endif

I08U Tcc353xStreamData[TCC353X_STREAM_BUFFER_SIZE + 188];
#ifndef USE_LGE_RING_BUFFER
I32U Tcc353xStreamRp[4] = {0,0,0,0};
I32U Tcc353xStreamWp[4] = {0,0,0,0};
I32U Tcc353xStreamBufferedSize[4] = {0,0,0,0};
I32U Tcc353xStreamFlushFlag[4] = {0,0,0,0};
#else
unsigned int RingBufId = 0;
#endif
extern TcpalSemaphore_t Tcc353xStreamSema;

/* for overflow test */
#define _DBG_CHK_OVERFLOW_CNT_
I32U gOverflowcnt = 0;
I32U gDbgIsrCnt = 0;

#ifndef USE_LGE_RING_BUFFER
void Tcc353xStreamBufferInit(I32S _moduleIndex)
{
	TcpalSemaphoreLock(&Tcc353xStreamSema);
	Tcc353xStreamRp[_moduleIndex] = 0;
	Tcc353xStreamWp[_moduleIndex] = 0;
	Tcc353xStreamBufferedSize[_moduleIndex] = 0;
	Tcc353xStreamFlushFlag[_moduleIndex] = 0;
	TcpalSemaphoreUnLock(&Tcc353xStreamSema);
 }

void Tcc353xStreamBufferClose(I32S _moduleIndex)
{
	TcpalSemaphoreLock(&Tcc353xStreamSema);
	Tcc353xStreamRp[_moduleIndex] = 0;
	Tcc353xStreamWp[_moduleIndex] = 0;
	Tcc353xStreamBufferedSize[_moduleIndex] = 0;
	Tcc353xStreamFlushFlag[_moduleIndex] = 0;
	TcpalSemaphoreUnLock(&Tcc353xStreamSema);
}

void Tcc353xStreamBufferReset(I32S _moduleIndex)
{
	TcpalSemaphoreLock(&Tcc353xStreamSema);
	Tcc353xStreamRp[_moduleIndex] = 0;
	Tcc353xStreamWp[_moduleIndex] = 0;
	Tcc353xStreamBufferedSize[_moduleIndex] = 0;
	Tcc353xStreamFlushFlag[_moduleIndex] = 0;
	TcpalSemaphoreUnLock(&Tcc353xStreamSema);
}

void Tcc353xStreamBufferFlush(I32S _moduleIndex)
{
	TcpalPrintStatus((I08S *) "[TCC353X] StreamBufferFlushing\n");
	TcpalSemaphoreLock(&Tcc353xStreamSema);
	Tcc353xStreamRp[_moduleIndex] = 0;
	Tcc353xStreamWp[_moduleIndex] = 0;
	Tcc353xStreamBufferedSize[_moduleIndex] = 0;
	Tcc353xStreamFlushFlag[_moduleIndex] = 1;
	TcpalSemaphoreUnLock(&Tcc353xStreamSema);
}
#else
void Tcc353xStreamBufferInit(I32S _moduleIndex)
{
#if defined (USE_LGE_RING_BUFFER) && !defined (_I2C_STS_)
	mbt_dataring_create(&RingBufId, TCC353X_STREAM_BUFFER_SIZE);
#endif
}

void Tcc353xStreamBufferClose(I32S _moduleIndex)
{
#if defined (USE_LGE_RING_BUFFER) && !defined (_I2C_STS_)
	mbt_dataring_destroy(&RingBufId);
#endif
}

void Tcc353xStreamBufferReset(I32S _moduleIndex)
{

}

void Tcc353xStreamBufferFlush(I32S _moduleIndex)
{
#if defined (USE_LGE_RING_BUFFER) && !defined (_I2C_STS_)
	mbt_dataring_flush(RingBufId);
#endif
}
#endif

#ifndef USE_LGE_RING_BUFFER
I32U Tcc353xGetStreamBuffer(I32S _moduleIndex, I08U * _buff, I32U _size)
{
	I32U tsSize = 0;
	I32U totalSize = 0;
	I32U rp;
	I32U wp;
	I32U nextRp;

	TcpalSemaphoreLock(&Tcc353xStreamSema);
	rp = Tcc353xStreamRp[_moduleIndex];
	wp = Tcc353xStreamWp[_moduleIndex];
	Tcc353xStreamFlushFlag[_moduleIndex] = 0;
	TcpalSemaphoreUnLock(&Tcc353xStreamSema);

	if(rp > wp)
		totalSize = wp + (TCC353X_STREAM_BUFFER_SIZE - rp);
	else if (rp < wp)
		totalSize = wp - rp;
	else 
		totalSize = 0;

	if(_size > totalSize)
		tsSize = totalSize;
	else
		tsSize = _size;
	tsSize = tsSize - (tsSize%188);

	if (tsSize<188) {
		return 0;
	}

	nextRp = ((rp+tsSize)%TCC353X_STREAM_BUFFER_SIZE);
	
	if(rp+tsSize>TCC353X_STREAM_BUFFER_SIZE) {	/* read twice */
		I32U first;
		I32U remain;
		first = TCC353X_STREAM_BUFFER_SIZE - rp;
		remain = nextRp;

		TcpalMemcpy (&_buff[0], &Tcc353xStreamData[rp], first);
		TcpalMemcpy (&_buff[first], &Tcc353xStreamData[0], nextRp);
	} else {
		TcpalMemcpy (&_buff[0], &Tcc353xStreamData[rp], tsSize);
	}

	TcpalSemaphoreLock(&Tcc353xStreamSema);
	if(Tcc353xStreamFlushFlag[_moduleIndex]!=0) {
		/* no update read pointer & no push data */
		Tcc353xStreamFlushFlag[_moduleIndex] = 0;
		tsSize = 0;
	} else {
		Tcc353xStreamRp[_moduleIndex] = nextRp;
		if(Tcc353xStreamBufferedSize[0]>=tsSize)
			Tcc353xStreamBufferedSize[0] -= tsSize;
	}
	TcpalSemaphoreUnLock(&Tcc353xStreamSema);
	return tsSize;
}
#else
I32U Tcc353xGetStreamBuffer(I32S _moduleIndex, I08U * _buff, I32U _size)
{
#if defined (USE_LGE_RING_BUFFER) && !defined (_I2C_STS_)
	I32U tsSize = 0;
	I32U totalSize = 0;

	totalSize = mbt_dataring_avail(RingBufId);

	if (totalSize < 188) {
		return 0;
	}

	if(_size > totalSize) {
		tsSize = totalSize;
	} else {
		tsSize = _size;
	}

	tsSize = mbt_dataring_read(RingBufId, _buff, tsSize);

	return tsSize;
#else
	return 0;
#endif
}

#endif
I32U Tcc353xInterruptProcess(void)
{
	I32U ret = 0;
	I08U irqStatus = 0;
	I32S moduleIndex = 0;
	I32U totalSize = 0;
	I08U data = 0x00;

	/* Read BB Interrupt Status */
	Tcc353xApiGetIrqStatus(moduleIndex, &irqStatus);

#if defined (_I2C_STS_)
	totalSize = 0;
	data = 0x00;

	/* Tcc353x IRQ Clear */
	Tcc353xApiIrqClear(moduleIndex, irqStatus);
	ret = 0; /* return no data */
#else
	/* Stream Interrupt */
	if (irqStatus&0x01) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] FIFO overflow[0x%02X] flush!!!\n",
			      irqStatus);

		/* IRQ Disable - Prevent additional interrupt signal */
		data = 0x00;
		Tcc353xApiRegisterWrite(0,0, 0x03, &data, 1);

		/* Tcc353x IRQ Clear */
		Tcc353xApiIrqClear(moduleIndex, irqStatus);
		Tcc353xApiInterruptBuffClr(moduleIndex);
		gOverflowcnt ++;
		ret = 0;
	} else {
		/* Tcc353x IRQ Clear */
		Tcc353xApiIrqClear(moduleIndex, irqStatus);
		Tcc353xApiGetFifoStatus(moduleIndex, &totalSize);
		ret = totalSize;
		if(ret>=150*188)
			TcpalPrintErr((I08S *)
			      "[TCC353X] FIFO stat size[%d]\n",
			      ret);
	}

	gDbgIsrCnt++;

	if(gDbgIsrCnt>100) {
		gDbgIsrCnt = 0;
#ifdef _DBG_CHK_OVERFLOW_CNT_
		TcpalPrintStatus((I08S *)
				  "[TCC353X] CurrOverflow Cnt %d\n",
				  gOverflowcnt);
#endif
	}
#endif

	return ret;
}

#ifndef USE_LGE_RING_BUFFER
void Tcc353xInterruptGetStream(I32U _fifoSize)
{
	I32S moduleIndex = 0;
	I32U totalSize = 0;
	I32U wp;

	totalSize = _fifoSize - (_fifoSize%188);

	//[Fix Start]align TS size to use DMA only mode - 20121228 hyewon.eum@lge.com
	//TcpalPrintErr((I08S *) "Tcc353xInterruptGetStream size[%d]\n", totalSize);

	totalSize = (totalSize/188/4)*188*4;

	if(totalSize > 188 * 87)
		totalSize = 188 * 84;
	//[Fix End]align TS size to use DMA only mode - 20121228 hyewon.eum@lge.com

	if(totalSize>=188) {
		I32U nextwp;

		if(Tcc353xStreamBufferedSize[moduleIndex]+totalSize > TCC353X_STREAM_BUFFER_SIZE)
			Tcc353xStreamBufferFlush (moduleIndex);

		TcpalSemaphoreLock(&Tcc353xStreamSema);
		wp = Tcc353xStreamWp[moduleIndex];
		TcpalSemaphoreUnLock(&Tcc353xStreamSema);

		nextwp = ((wp+totalSize)%TCC353X_STREAM_BUFFER_SIZE);
		
		if(wp+totalSize>TCC353X_STREAM_BUFFER_SIZE) {	/* read twice */
			I32U first;
			first = TCC353X_STREAM_BUFFER_SIZE - wp;
			Tcc353xApiStreamRead(moduleIndex,
					     &Tcc353xStreamData[wp],
					     first);
			Tcc353xApiStreamRead(moduleIndex,
					     &Tcc353xStreamData[0],
					     nextwp);
		} else {
			Tcc353xApiStreamRead(moduleIndex,
					     &Tcc353xStreamData[wp],
					     totalSize);
		}

		TcpalSemaphoreLock(&Tcc353xStreamSema);
		Tcc353xStreamWp[moduleIndex] = nextwp;
		Tcc353xStreamBufferedSize[moduleIndex] += totalSize;
		TcpalSemaphoreUnLock(&Tcc353xStreamSema);

		if(Tcc353xStreamData[wp]!=0x47) {
			TcpalPrintErr((I08S *) "[TCC353X] SyncByte Error! [0x%02x]\n",
				     Tcc353xStreamData[wp]);
			TcpalPrintErr((I08S *) "[TCC353X] Buff Flush for SyncByte matching\n");
			Tcc353xApiInterruptBuffClr(moduleIndex);
		}
	}
}
#else
void Tcc353xInterruptGetStream(I32U _fifoSize)
{
#if defined (USE_LGE_RING_BUFFER) && !defined (_I2C_STS_)
#define _MAX_TS_READ_SIZE_	(15792) /* 188*84 */
	I32U totalSize = 0;
	I32U freeSize = 0;
	I32U writeSize = 0;
	I32U remainSize = 0;
	I32U i;
	I32U readSizes[2] = {0,0};

	totalSize = _fifoSize - (_fifoSize%188);

	//[Fix Start]align TS size to use DMA only mode - 20121228 hyewon.eum@lge.com
	//TcpalPrintErr((I08S *) "Tcc353xInterruptGetStream size[%d]\n", totalSize);

	totalSize = (totalSize/188/4)*188*4;

	//if(totalSize > 188 * 87)
	if(totalSize > _MAX_TS_READ_SIZE_) {
		remainSize = totalSize - _MAX_TS_READ_SIZE_;
		remainSize = (remainSize/188/4)*188*4;
		if(remainSize> _MAX_TS_READ_SIZE_)
			remainSize = _MAX_TS_READ_SIZE_;
		totalSize = _MAX_TS_READ_SIZE_;
	} else {
		remainSize = 0;
	}
	readSizes[0] = totalSize;
	readSizes[1] = remainSize;

	//[Fix End]align TS size to use DMA only mode - 20121228 hyewon.eum@lge.com

	for(i=0; i<2; i++) {
		if(readSizes[i]>=188) {
			Tcc353xApiStreamRead(0,
					     &Tcc353xStreamData[0],
					     readSizes[i]);

			if(Tcc353xStreamData[0]!=0x47) {
				TcpalPrintErr((I08S *) "[TCC353X] SyncByte Error! [0x%02x]\n",
					     Tcc353xStreamData[0]);
				TcpalPrintErr((I08S *) "[TCC353X] Buff Flush for SyncByte matching\n");
			} else {
				freeSize = mbt_dataring_free(RingBufId);

				if(freeSize >= readSizes[i]) {
					writeSize = mbt_dataring_write(RingBufId, &Tcc353xStreamData[0], readSizes[i]);
					if(writeSize < 0) {
						TcpalPrintErr((I08S *) "[TCC353X] Ring Buffer Error!\n");
					}
				}
			}
		}
	}
#else
	return;
#endif
}
#endif
