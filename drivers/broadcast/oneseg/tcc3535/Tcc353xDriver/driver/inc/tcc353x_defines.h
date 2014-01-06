/****************************************************************************
 *   FileName    : tcc353x_defines.h
 *   Description : defines
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

#ifndef __TCC353X_DEFINES_H__
#define __TCC353X_DEFINES_H__

#include "tcc353x_common.h"

/* CODE Memory Setting */
#define PHY_BASE_ADDR                       (0x80000000)
#define TCC353X_CODEMEMBASE                 (PHY_BASE_ADDR + 0xC000)

#define _SUPPORT_OSC_38400_ /* default */
#define _SUPPORT_OSC_19200_

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
