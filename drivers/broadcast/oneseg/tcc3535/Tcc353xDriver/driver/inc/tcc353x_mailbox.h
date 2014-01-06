/****************************************************************************
 *   FileName    : tcc353x_mailbox.h
 *   Description : mailbox control Function
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

#ifndef __TCC353X_MAILBOX_H__
#define __TCC353X_MAILBOX_H__

#include "tcc353x_common.h"
#include "tcc353x_core.h"

#define MAX_MAILBOX_RETRY	(2)	/* continuously */

#define MB_HOSTMAIL		0x47
#define MB_SLAVEMAIL		0x74

#define MB_ERR_OK		0x00
#define MB_ERR_CMD		0x01
#define MB_ERR_PARA		0x02

#define MBCMD_SYS		(0x00<<11)
#define MBPARA_SYS_START	(MBCMD_SYS | 0x00)
#define MBPARA_SYS_RF_FREQ	(MBCMD_SYS | 0x04)
#define MBPARA_SYS_PAUSE	(MBCMD_SYS | 0x07)
#define MBPARA_SYS_ASM_VER	(MBCMD_SYS | 0xff)

#define MB_TMCC			(0x05<<11)
#define MBPARA_TMCC_RESULT	(MB_TMCC | 0x00)

/*
 *   Mailbox protocol
 *   dir : 8bit
 *   size : 4bit
 *   RW
 *   error/ok : 12bit
 *   cmd : 5bit
 *   param : 3bit
 */

#define MB_CMD_READ	0
#define MB_CMD_WRITE	1

I32S Tcc353xMailboxTxOnly(Tcc353xHandle_t * _handle, I32U cmd,
			  I32U * data_array, I32S word_cnt);
I32S Tcc353xMailboxTxRx(Tcc353xHandle_t * _handle, mailbox_t * p_mailbox,
			I32U cmd, I32U * data_array, I32S word_cnt);
I32U Tcc353xGetAccessMail(Tcc353xHandle_t * _handle);

#endif
