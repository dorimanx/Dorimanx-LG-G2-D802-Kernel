/****************************************************************************
 *   FileName    : tcpal_os.h
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

#ifndef __TCPAL_OS_H__
#define __TCPAL_OS_H__

#ifdef __cplusplus
extern    "C"
{
#endif

#include "tcpal_types.h"

#define GET2BYTES(x) (*(x)<<8 | *(x+1))
#define GET3BYTES(x) (*(x)<<16 | *(x+1)<<8 | *(x+2))
#define GET4BYTES(x) (*(x)<<24 | *(x+1)<<16 | *(x+2)<<8 | *(x+3))
#define HTONS(A) ((((unsigned int short)(A) & 0xff00) >> 8) | \
                (((unsigned int short)(A) & 0x00ff) << 8))
#define HTONL(A) ((((unsigned long)(A) & 0xff000000) >> 24) | \
                (((unsigned long)(A) & 0x00ff0000) >> 8)  | \
                (((unsigned long)(A) & 0x0000ff00) << 8)  | \
                (((unsigned long)(A) & 0x000000ff) << 24))
#define NTOHS  HTONS
#define NTOHL  HTONL

TCBB_FUNC I32S TcpalPrintLog(const I08S * _fmt, ...);
TCBB_FUNC I32S TcpalPrintErr(const I08S * _fmt, ...);
TCBB_FUNC I32S TcpalPrintStatus(const I08S * _fmt, ...);

/* For TimeCheck */
#define TCPAL_MAX_TIMECNT 0xFFFFFFFFUL
TCBB_FUNC TcpalTime_t TcpalGetCurrentTimeCount_ms(void);
TCBB_FUNC TcpalTime_t TcpalGetTimeIntervalCount_ms(TcpalTime_t
						   _startTimeCount);

/* for sleep */
TCBB_FUNC void TcpalmSleep(I32S _ms);
TCBB_FUNC void TcpaluSleep(I32S _us);
TCBB_FUNC void TcpalmDelay(I32S _ms);

/* for memory allocation, free, set */
TCBB_FUNC void *TcpalMemset(void *_dest, I32U _data, I32U _cnt);
TCBB_FUNC void *TcpalMemcpy(void *_dest, const void *_src, I32U _cnt);

/* For Semaphore */
#define TCPAL_INFINITE_SEMAPHORE  0xFFFFFFFFUL

TCBB_FUNC I32S TcpalCreateSemaphore(TcpalSemaphore_t * _semaphore,
				    I08S * _name, I32U _initialCount);
TCBB_FUNC I32S TcpalDeleteSemaphore(TcpalSemaphore_t * _semaphore);
TCBB_FUNC I32S TcpalSemaphoreLock(TcpalSemaphore_t * _semaphore);
TCBB_FUNC I32S TcpalSemaphoreUnLock(TcpalSemaphore_t * _semaphore);

#ifdef __cplusplus
};
#endif

#endif

