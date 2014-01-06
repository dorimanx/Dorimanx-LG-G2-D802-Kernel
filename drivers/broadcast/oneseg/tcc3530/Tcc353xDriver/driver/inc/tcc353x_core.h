/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_core.h                                          */
/*    Description : core Function                                            */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#ifndef __TCC353X_CORE_H__
#define __TCC353X_CORE_H__

#include "tcc353x_defines.h"
#include "tcc353x_common.h"

typedef struct {
	I08U *coldbootDataPtr;
	I32U coldbootDataSize;
	I08U *daguDataPtr;
	I32U daguDataSize;
	I08U *dintDataPtr;
	I32U dintDataSize;
	I08U *randDataPtr;
	I32U randDataSize;
	I08U *colOrderDataPtr;
	I32U colOrderDataSize;
} Tcc353xBoot_t;

I32S Tcc353xOpen(I32S _moduleIndex, Tcc353xOption_t * _Tcc353xOption);
I32S Tcc353xClose(I32S _moduleIndex);
I32S Tcc353xInit(I32S _moduleIndex, I08U * _coldbootData, I32S _codeSize);
I32S Tcc353xTune(I32S _moduleIndex, I32S _frequency,
		 Tcc353xTuneOptions * _tuneOption, I32S _fastTune);
I32S Tcc353xStreamStopAll(I32S _moduleIndex);
I32S Tcc353xStreamStop(I32S _moduleIndex);
I32S Tcc353xStreamStart(I32S _moduleIndex);
I32S Tcc353xInterruptBuffClr(I32S _moduleIndex);
I32S Tcc353xDetach(I32S _moduleIndex);
void Tcc353xPeripheralOnOff(Tcc353xHandle_t * _handle, I32S _onoff);
I32U Tcc353xGetCoreVersion(void);
I32U Tcc353xSendStartMail(Tcc353xHandle_t * _handle);
I32S Tcc353xMailboxWrite(I32S _moduleIndex, I32S _diversityIndex,
			 I32U _command, I32U * dataArray, I32S wordSize);
I32S Tcc353xMailboxRead(I32S _moduleIndex, I32S _diversityIndex,
			I32U _command, mailbox_t * _mailbox);
I32S DummyFunction0(I32S _moduleIndex, I32S _chipAddress, I08U _inputData,
		    I08U * _outData, I32S _size);
I32S DummyFunction1(I32S _moduleIndex, I32S _chipAddress, I08U _address,
		    I08U * _inputData, I32S _size);
#endif
