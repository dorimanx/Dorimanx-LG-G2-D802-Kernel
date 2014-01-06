/****************************************************************************
 *   FileName    : tcc353x_rf.h
 *   Description : TCC353X RF control functions
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

#ifndef __TCC353X_RF_H__
#define __TCC353X_RF_H__

#include "tcc353x_common.h"

#define _IDX_ADDRESS_		0
#define _IDX_ACCESSABLE_	1
#define _IDX_FM_		2
#define _IDX_VHF_		3
#define _IDX_UHF_		4

I32S Tcc353xRfInit(I32S _moduleIndex, I32S _diversityIndex);
void Tcc353xRfTune(I32S _moduleIndex, I32S _diversityIndex, I32S _freq_khz,
		   I32S _bw_khz, I32S _oscClk,
		   Tcc353xTuneOptions * _tuneOption);
I32S Tcc353xRfWrite(I32S _moduleIndex, I32S _diversityIndex, I08U addr,
		    I32U data);
I32S Tcc353xRfWriteEx(I32S _moduleIndex, I32S _diversityIndex, I08U *_address,
		    I32U *_data, I32U _size);
I32S Tcc353xRfRead(I32S _moduleIndex, I32S _diversityIndex, I08U addr,
		   I32U * data);
void Tcc353xRfSwitching(I32S _moduleIndex, I32S _diversityIndex,
		        I32S _frequency, Tcc353xOption_t * _option);
#endif
