/****************************************************************************
 *   FileName    : tcpal_android.c
 *   Description : OS glue Function
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
#include <linux/delay.h>
#include <linux/slab.h>
#include "tcc353x_common.h"


/* Telechips Android (Android Platform Layer) */
I08S Tcc353xDebugStr[1024];

/* For Debug Print */
I32S TcpalPrintLog(const I08S * _fmt, ...)
{
	va_list ap;

	va_start(ap, _fmt);
	vsprintf(Tcc353xDebugStr, _fmt, ap);
	va_end(ap);

	printk(KERN_INFO"%s", Tcc353xDebugStr);
	return TCC353X_RETURN_SUCCESS;
}

I32S TcpalPrintErr(const I08S * _fmt, ...)
{
	va_list ap;

	va_start(ap, _fmt);
	vsprintf(Tcc353xDebugStr, _fmt, ap);
	va_end(ap);

	printk(KERN_ERR"%s", Tcc353xDebugStr);
	return TCC353X_RETURN_SUCCESS;
}

I32S TcpalPrintStatus(const I08S * _fmt, ...)
{
	va_list ap;

	va_start(ap, _fmt);
	vsprintf(Tcc353xDebugStr, _fmt, ap);
	va_end(ap);

	printk(KERN_DEBUG"%s", Tcc353xDebugStr);
	return TCC353X_RETURN_SUCCESS;
}

/* For TimeCheck */
#define MAX_TIMECNT 0xFFFFFFFFFFFFFFFFLL
TcpalTime_t TcpalGetCurrentTimeCount_ms(void)
{
	TcpalTime_t tickcount = 0;
	struct timeval tv;

	do_gettimeofday(&tv);
	tickcount = (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return tickcount;
}

TcpalTime_t TcpalGetTimeIntervalCount_ms(TcpalTime_t _startTimeCount)
{
	TcpalTime_t count = 0;

	if (TcpalGetCurrentTimeCount_ms() >= _startTimeCount)
		count = TcpalGetCurrentTimeCount_ms() - _startTimeCount;
	else
		count =
		    ((MAX_TIMECNT - _startTimeCount) +
		     TcpalGetCurrentTimeCount_ms() + 1);
	return count;
}

/* for sleep */
void TcpalmSleep(I32S _ms)
{
	msleep(_ms);
}

void TcpalmDelay(I32S _ms)
{
	I32S i;
	for(i=0; i<_ms; i++)
		mdelay(1);
}

void TcpaluSleep(I32S _us)
{
	//usleep(_us);
}

/* for memory allocation, free, set */
void *TcpalMemset(void *_dest, I32U _data, I32U _cnt)
{
	void *ptr = NULL;
	if (_dest == NULL)
		ptr = NULL;
	else
		ptr = memset(_dest, _data, _cnt);
	return ptr;
}

void *TcpalMemcpy(void *_dest, const void *_src, I32U _cnt)
{
	void *ptr = NULL;
	if ((_dest == NULL) || (_src == NULL))
		ptr = NULL;
	else
		ptr = memcpy(_dest, _src, _cnt);
	return ptr;
}

/* For Semaphore */
#define MAX_MUTEX_POOL 15
static struct mutex MutexPool[MAX_MUTEX_POOL];
static I32U MutexAssignd[MAX_MUTEX_POOL] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static I32S TcpalGetSemaphoreAddress(void)
{
	I32S i;
	for(i=0; i<MAX_MUTEX_POOL; i++) {
		if(MutexAssignd[i] == 0) {
			MutexAssignd[i] = (I32U)(&MutexPool[i]);
			return i;
		}
	}
	return (-1);
}

static I32S TcpalFreeSemaphoreAddress(I32U _address)
{
	I32S i;
	for(i=0; i<MAX_MUTEX_POOL; i++) {
		if(MutexAssignd[i] == _address) {
			MutexAssignd[i] = 0;
			return i;
		}
	}
	return (-1);
}

I32S TcpalCreateSemaphore(TcpalSemaphore_t * _semaphore, I08S * _name,
			  I32U _initialCount)
{
	struct mutex *lock;
	I32S index;

	index = TcpalGetSemaphoreAddress();
	if(index<0) {
		TcpalPrintErr((I08S *)"######## Mutex Get Fail :%d \n", (int)index);
		return TCC353X_RETURN_FAIL;
	}

	lock = &MutexPool[index];
	mutex_init(lock);
	
	TcpalPrintLog((I08S *)"######## MutexC %s [%d] \n", _name, (int)(lock));
	
	*_semaphore = (TcpalSemaphore_t)lock;
	return TCC353X_RETURN_SUCCESS;
}

I32S TcpalDeleteSemaphore(TcpalSemaphore_t * _semaphore)
{
	struct mutex *lock = (struct mutex*)*_semaphore;
	I32U address;
	I32S index;
	
	if(lock == NULL) 
		return TCC353X_RETURN_FAIL;

	address = (I32U)(lock);
	index = TcpalFreeSemaphoreAddress(address);
	if(index < 0) 
	{ 
		TcpalPrintErr((I08S *)"####### Mutex Delete Fail :%d \n", (int)index);
		return TCC353X_RETURN_FAIL;
	}
	
	TcpalPrintLog((I08S *)"######## MutexR [%d] \n", (int)(lock));
	
	mutex_destroy(lock);
	*_semaphore = 0;
	return TCC353X_RETURN_SUCCESS;
}

I32S TcpalSemaphoreLock(TcpalSemaphore_t * _semaphore)
{
	struct mutex *lock = (struct mutex*)*_semaphore;

	if(lock == NULL) {
		TcpalPrintErr((I08S *)"semaphore lock error\n");
		return TCC353X_RETURN_FAIL;
	}
	mutex_lock(lock);
	return TCC353X_RETURN_SUCCESS;
}

I32S TcpalSemaphoreUnLock(TcpalSemaphore_t * _semaphore)
{
	struct mutex *lock = (struct mutex*)*_semaphore;

	if(lock == NULL) {
		TcpalPrintErr((I08S *)"semaphore unlock error\n");
		return TCC353X_RETURN_FAIL;
	}

	mutex_unlock(lock);
	return TCC353X_RETURN_SUCCESS;
}
