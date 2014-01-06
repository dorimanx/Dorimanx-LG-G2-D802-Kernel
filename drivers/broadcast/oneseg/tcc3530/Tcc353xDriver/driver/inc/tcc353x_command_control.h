/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_api.c                                           */
/*    Description : API Function                                            */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

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
