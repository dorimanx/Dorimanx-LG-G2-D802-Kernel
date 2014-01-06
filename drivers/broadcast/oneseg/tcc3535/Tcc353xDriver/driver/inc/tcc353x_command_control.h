/****************************************************************************
 *   FileName    : tcc353x_command_control.h
 *   Description : control command Function
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

#include "tcc353x_core.h"

#ifndef __TCC353X_COMMAND_CONTROL_H__
#define __TCC353X_COMMAND_CONTROL_H__

#define	_RF_TCC3530_	0

typedef enum {
	MISC_OP_CTRL = 0,
	MISC_RF_REG_CTRL = 2,
	MISC_SDRAM_REG_CTRL = 4,
	MISC_OP_REG_CTRL = 6
} ENUM_MISC_CFG;

I32S Tcc353xMemoryRead(I32S _moduleIndex, I32S _diversityIndex,
		       I32U _address, I08U * _data, I32U _size);
I32S Tcc353xMemoryWrite(I32S _moduleIndex, I32S _diversityIndex,
			I32U _address, I08U * _data, I32U _size);
I32S Tcc353xStreamRead(I32S _moduleIndex, I08U * _data, I32S _size);
I32S Tcc353xDspAsmWrite(Tcc353xHandle_t * _handle, I08U * _data,
			I32S _size);
I32S ReadProcess(Tcc353xHandle_t * _handle, I08U _registerAddr, I32S _size,
		 I08U * _outData, I08U _unlock);
I32S WriteProcess(Tcc353xHandle_t * _handle, I08U _registerAddr,
		  I08U * _inputData, I32S _size, I08U _unlock);
I32S Tcc353xRead(I32S _moduleIndex, I32S _diversityIndex, I08U _address,
		 I08U * _data, I32U _size);
I32S Tcc353xWrite(I32S _moduleIndex, I32S _diversityIndex, I08U _address,
		  I08U * _data, I32U _size);
I32S Tcc353xReadIrqError(I32S _moduleIndex, I08U * _data);
I32S Tcc353xReadIrqStatus(I32S _moduleIndex, I08U * _data);
I32S Tcc353xIrqClear(I32S _moduleIndex, I08U _data);
I32S Tcc353xWriteIrqErrorClear(I32S _moduleIndex);
I32S Tcc353xMiscRead(I32S _moduleIndex, I32S _diversityIndex,
		     I08U _miscConfig, I08U _address, I32U * _data);
I32S Tcc353xMiscWrite(I32S _moduleIndex, I32S _diversityIndex,
		      I08U _miscConfig, I08U _address, I32U _data);
I32S Tcc353xMiscWriteExIncrease(I32S _moduleIndex, I32S _diversityIndex,
		      I08U _miscConfig, I08U _startAddress, 
		      I32U *_data, I32U _size);
I32S Tcc353xMiscWriteEx(I32S _moduleIndex, I32S _diversityIndex,
		      I08U _miscConfig, I08U *_startAddress, 
		      I32U *_data, I32U _size);
#endif
