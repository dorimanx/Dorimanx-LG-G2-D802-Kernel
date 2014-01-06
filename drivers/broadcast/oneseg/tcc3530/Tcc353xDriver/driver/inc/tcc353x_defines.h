/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_defines.h                                       */
/*    Description : defines                                                 */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#ifndef __TCC353X_DEFINES_H__
#define __TCC353X_DEFINES_H__

#include "tcc353x_common.h"

/*
#define USE_SEMI_RF
*/

/* CODE Memory Setting */
#define PHY_BASE_ADDR                       (0x80000000)
#define TCC353X_CODEMEMBASE                 (PHY_BASE_ADDR + 0xC000)

#if defined (_MODEL_F9J_)
#define _SUPPORT_OSC_19200_
#else
#define _SUPPORT_OSC_38400_ /* default */
#endif

typedef struct Tcc353xHandle_t {
	I08U handleOpen;

	I08U moduleIndex;
	I08U diversityIndex;
	I08U currentAddress;
	I08U originalAddress;
	I08U sysEnValue;

	I08U mailboxErrorCounter;
	I08U reserved2;

	I32U dspCodeVersion;
	I32U mainClkKhz;
	Tcc353xOption_t options;
	Tcc353xTuneOptions TuneOptions;
	Tcc353xStreamFormat_t streamFormat;

	/* status */
	I32U tuned;
	I32U streamStarted;

	/* pidTable format [12:0] pid  [15] filter on flag */
	I32U pidTable[32];
	I32U numOfPidTable;
	I32U useDefaultPLL;
	
	I32S(*Read) (I32S _moduleIndex, I32S _chipAddress,
		     I08U _registerAddr, I08U * _outData, I32S _size);
	I32S(*Write) (I32S _moduleIndex, I32S _chipAddress,
		      I08U _registerAddr, I08U * _outData, I32S _size);
} Tcc353xHandle_t;

#endif
