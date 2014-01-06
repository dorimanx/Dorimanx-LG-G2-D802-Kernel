/****************************************************************************
 *   FileName    : tcc353x_api.h
 *   Description : API Function
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

#ifndef __TCC353X_API_H__
#define __TCC353X_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tcc353x_common.h"

#define ISDB_AGC_LOCK          	(5)
#define ISDB_AGC_LOCK_P_1 	(15)
#define ISDB_AGC_LOCK_TMM_13   	(5)
#define ISDB_AGC_LOCK_TMM_1   	(15)

#define ISDB_CTO_LOCK          	(35)
#define ISDB_CTO_LOCK_P_1  	(30)
#define ISDB_CTO_LOCK_TMM_13  	(50)
#define ISDB_CTO_LOCK_TMM_1  	(30)
#define ISDB_CTO_RETRY         	(2)

#define ISDB_CFO_LOCK          	(75)
#define ISDB_CFO_LOCK_P_1  	(170)
#define ISDB_CFO_LOCK_TMM_13  	(100)
#define ISDB_CFO_LOCK_TMM_1  	(170)
#define ISDB_CFO_RETRY         	(2)

#define ISDB_TMCC_LOCK         	(730)
#define ISDB_TMCC_LOCK_P_1  	(730)
#define ISDB_TMCC_LOCK_TMM_13  	(800)
#define ISDB_TMCC_LOCK_TMM_1  	(800)
#define ISDB_TMCC_RETRY        	(1)

/*
full seg :	invalid min : 80 ms
1 seg :		invalid min : 90 ms
tmm13 seg :	invalid min : 110 ms
tmm1 seg :	invalid min : 90 ms
*/

typedef enum {
	TCC353X_STATUS_NONE = 0,
	TCC353X_STATUS_CLOSED,
	TCC353X_STATUS_OPEND,
	TCC353X_STATUS_INITED
} EnumTcc353xStatus;

typedef struct {
	I32U status;
	I32U NumberofBaseband;
	I32U tmmMode;
	I32U tmmSegments;
	I32U currentBbName;
} Tcc353xApiControl_t;

I32S Tcc353xApiOpen(I32S _moduleIndex,
		    Tcc353xOption_t * _Tcc353xOption,
		    I32S _optionSize);
I32S Tcc353xApiInit(I32S _moduleIndex, I08U * _coldbootData,
		    I32S _codeSize,
		    Tcc353xStreamFormat_t * _streamFormat);
I32S Tcc353xApiClose(I32S _moduleIndex);
I32S Tcc353xApiChannelSearch(I32S _moduleIndex, I32S _frequency,
			     Tcc353xTuneOptions * _tuneOption);
I32S Tcc353xApiChannelSelect(I32S _moduleIndex, I32S _frequency,
			     Tcc353xTuneOptions * _tuneOption);
I32S Tcc353xApiGetLockStatus(I32S _moduleIndex,
			     I32S _diversityIndex,
			     IsdbLock_t * _isdbLock);
I32S Tcc353xApiGetEwsFlag(I32S _moduleIndex, I32S _diversityIndex);
I32S Tcc353xApiRegisterRead(I32S _moduleIndex,
			    I32S _diversityIndex, I08U _address,
			    I08U * _data, I32U _size);
I32S Tcc353xApiRegisterWrite(I32S _moduleIndex,
			     I32S _diversityIndex, I08U _address,
			     I08U * _data, I32U _size);
I32S Tcc353xApiRfRegisterRead(I32S _moduleIndex,
			      I32S _diversityIndex, I08U _address,
			      I32U * _data);
I32S Tcc353xApiRfRegisterWrite(I32S _moduleIndex,
			       I32S _diversityIndex, I08U _address,
			       I32U _data);
I32S Tcc353xApiSetStreamFormat(I32S _moduleIndex,
			       Tcc353xStreamFormat_t *
			       _streamFormat);
I32S Tcc353xApiAddPids(I32S _moduleIndex, 
		       Tcc353xpidTable_t * _pidTableControl);
I32S Tcc353xApiRemovePids(I32S _moduleIndex, 
			  Tcc353xpidTable_t * _pidTableControl);
I32S Tcc353xApiGetpidTable(I32S _moduleIndex,
				  Tcc353xpidTable_t *
				  _pidTableControl);
I32S Tcc353xApiStreamStop(I32S _moduleIndex);
I32S Tcc353xApiStreamStart(I32S _moduleIndex);
I32S Tcc353xApiStreamRead(I32S _moduleIndex, I08U * _data,
			  I32S _size);
I32S Tcc353xApiMemoryWrite(I32S _moduleIndex, I32S _diversityIndex,
			   I32U _address, I08U * _data,
			   I32U _size);
I32S Tcc353xApiMemoryRead(I32S _moduleIndex, I32S _diversityIndex,
			  I32U _address, I08U * _data, I32U _size);
I32S Tcc353xApiGetIrqError(I32S _moduleIndex, I08U * _data);
I32S Tcc353xApiSetIrqErrorClear(I32S _moduleIndex);
I32S Tcc353xApiGetIrqStatus(I32S _moduleIndex, I08U * _data);
I32S Tcc353xApiIrqClear(I32S _moduleIndex, I08U _data);
I32S Tcc353xApiMailboxWrite(I32S _moduleIndex,
			    I32S _diversityIndex, I32U _command,
			    I32U * _dataArray, I32S wordSize);
I32S Tcc353xApiMailboxRead(I32S _moduleIndex, I32S _diversityIndex,
			   I32U _command, mailbox_t * _mailbox);
I32S Tcc353xApiUserCommand(I32S _moduleIndex, I32S _diversityIndex,
			   I32S _command, void *_param1,
			   void *_param2, void *_param3,
			   void *_param4);
I32S Tcc353xApiCasOpen(I32S _moduleIndex, I32U _casRound,
		       I08U * _systemKey);
I32S Tcc353xApiCasSetPid(I32S _moduleIndex, I32U * _pids,
			 I32U _numberOfPids);
I32S Tcc353xApiCasSetKeyMulti2(I32S _moduleIndex, I32S _parity,
			       I08U * _key, I32S _keyLength,
			       I08U * _initVector,
			       I32S _initVectorLength);
I32S Tcc353xApiGetTMCCInfo(I32S _moduleIndex, I32S _diversityIndex,
			   tmccInfo_t * _tmccInfo);
I32S Tcc353xApiGetFifoStatus(I32S _moduleIndex, I32U *_fifoSize);

void Tcc353xApiParseIsdbSyncStat(IsdbLock_t * _isdbLock, I08U _input);
I32S Tcc353xApiInterruptBuffClr(I32S _moduleIndex);
I32S Tcc353xApiUserLoopStopCmd(I32S _moduleIndex);
I32S Tcc353xApiSetGpioControl(I32S _moduleIndex, I32S _diversityIndex, 
			   I32S _gpioNum, I32S _value);
#ifdef __cplusplus
};
#endif

#endif
