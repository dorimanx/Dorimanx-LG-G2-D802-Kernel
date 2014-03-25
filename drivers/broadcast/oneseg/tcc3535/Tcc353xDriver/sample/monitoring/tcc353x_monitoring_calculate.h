/****************************************************************************
 *   FileName    : tcc353x_monitoring_calculate.h
 *   Description : sample source for monitoring
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

#ifndef __TCC353X_MONITORING_CALCULATE_H__
#define __TCC353X_MONITORING_CALCULATE_H__

#ifdef __cplusplus
extern    "C"
{
#endif

#include "tcc353x_common.h"
#include "tcc353x_monitoring.h"

#define SCALE_FACTOR 100000

#define _ISDB_MIN_MER_  0
#define _ISDB_MAX_MER_  60

#define _ISDB_MIN_SNR_  0
#define _ISDB_MAX_SNR_  50

#define _ISDB_MIN_PCBER_  0
#define _ISDB_MAX_PCBER_  20000

#define _ISDB_MIN_VITERBIBER_  0
//#define _ISDB_MAX_VITERBIBER_  500
#define _ISDB_MAX_VITERBIBER_  2000


#define _ISDB_MIN_TSPER_  0
#define _ISDB_MAX_TSPER_  50000

#define _ISDB_MIN_RSSI_  (-105)
#define _ISDB_MAX_RSSI_  3

#define VBER_ANTENNA_0		500
#define VBER_ANTENNA_5		420
#define VBER_ANTENNA_10		360
#define VBER_ANTENNA_15		300
#define VBER_ANTENNA_20		200
#define VBER_ANTENNA_25		100
#define VBER_ANTENNA_30		80
#define VBER_ANTENNA_35		60
#define VBER_ANTENNA_40		30
#define VBER_ANTENNA_45		10
#define VBER_ANTENNA_50		8
#define VBER_ANTENNA_55		6
#define VBER_ANTENNA_60		4
#define VBER_ANTENNA_65		1
#define VBER_ANTENNA_70		0
#define VBER_ANTENNA_75		0
#define VBER_ANTENNA_80		0
#define VBER_ANTENNA_85		0
#define VBER_ANTENNA_90		0
#define VBER_ANTENNA_95		0
#define VBER_ANTENNA_100	0

#define PCBER_ANTENNA_70	(1600)
#define PCBER_ANTENNA_75	(800)
#define PCBER_ANTENNA_80	(400)
#define PCBER_ANTENNA_85	(200)
#define PCBER_ANTENNA_90	(100)
#define PCBER_ANTENNA_95	(50)
#define PCBER_ANTENNA_100	(25)

I32U Tcc353xCalculateMer(Tcc353xStatus_t * _dMBStatData, I32U _index);
I32U Tcc353xCalculateSnr(Tcc353xStatus_t * _dMBStatData);
I32U Tcc353xCalculatePcber(Tcc353xStatus_t * _dMBStatData, I32U _index);
I32U Tcc353xCalculateViterbiber(Tcc353xStatus_t * _dMBStatData,
				I32U oldViterbiber, I32U _index);
I32U Tcc353xCalculateTsper(Tcc353xStatus_t * _dMBStatData, I32U oldTsper,
			   I32U _index);
I32S Tcc353xCalculateRssi(I32S _moduleIndex, 
			  I32S _diversityIndex, 
			  Tcc353xStatus_t * _isdbStatusData);
I32U Tcc353xGetUnsignedMovingAvg(I32U * _Array, I32U _input, I32U _count,
				 I32S _maxArray);
I32S Tcc353xGetSignedMovingAvg(I32S * _Array, I32S _input, I32S _count,
			       I32S _maxArray);

#ifdef __cplusplus
};
#endif

#endif
