/****************************************************************************
 *   FileName    : tcc353x_monitoring_calculate.c
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

#include "tcc353x_monitoring_calculate.h"
#include "tcpal_os.h"
#include "tcc353x_api.h"
#include "tcc353x_user_defines.h"

extern Tcc353xApiControl_t Tcc353xApiControl[TCC353X_MAX]
    [TCC353X_DIVERSITY_MAX];

static I32U Tcc353xMerTable(I32U _val);

static I64U Tcc353xMonDiv64(I64U x, I64U y);
#define DIV(A,B)    (Tcc353xMonDiv64(A,B))

static I64U Tcc353xMonDiv64(I64U x, I64U y)
{
	I64U a, b, q, counter;

	q = 0;
	if (y != 0) {
		while (x >= y) {
			a = x >> 1;
			b = y;
			counter = 1;
			while (a >= b) {
				b <<= 1;
				counter <<= 1;
			}
			x -= b;
			q += counter;
		}
	}
	return q;
}

I32U Tcc353xCalculateMer(Tcc353xStatus_t * _dMBStatData, I32U _index)
{
	I32U input;
	I32U MER;

	/* mer = 20*log10(lxMer/32) */
	if(_index>2)
		return _ISDB_MIN_MER_;

	input = _dMBStatData->lxMer[_index];

	if (input == 0) {
		MER = _ISDB_MIN_MER_;
	} else {
		MER = Tcc353xMerTable(input);
	}

	if (MER > _ISDB_MAX_MER_)
		MER = _ISDB_MAX_MER_;

	return MER;
}

I32U Tcc353xCalculateSnr(Tcc353xStatus_t * _dMBStatData)
{
	I32U SNR;

	SNR = _dMBStatData->snrMer;
	if(SNR >= 3000)
		SNR = 3000;
	return SNR;
}

I32U Tcc353xCalculatePcber(Tcc353xStatus_t * _dMBStatData, I32U _index)
{
	I32U pcber;
	I64U over;

	if(_index>2)
		return _ISDB_MAX_PCBER_;

	over = (I64U) (_dMBStatData->pcber[_index] * SCALE_FACTOR);
	pcber = (I32U) (over >> 16);

	if (pcber > _ISDB_MAX_PCBER_)
		pcber = (I32U)(_ISDB_MAX_PCBER_);

	return pcber;
}

I32U Tcc353xCalculateViterbiber(Tcc353xStatus_t * _dMBStatData,
				I32U oldViterbiber, I32U _index)
{
	I32U VITERBIBER;
	I64U over;
	I64U under;
	I64U overcnt;
	I64U errorcnt;
	I64U result;

	if(_index>2)
		return _ISDB_MAX_VITERBIBER_;

	if(_dMBStatData->opstat.resynced)
		return _ISDB_MAX_VITERBIBER_;

	if(_index==0) {
		overcnt = (I64U)(_dMBStatData->opstat.ARsOverCnt);
		errorcnt = (I64U)(_dMBStatData->opstat.ARsErrorCnt);
		under = (I64U)(_dMBStatData->opstat.ARsCnt);
	} else if(_index==1) {
		overcnt = (I64U)(_dMBStatData->opstat.BRsOverCnt);
		errorcnt = (I64U)(_dMBStatData->opstat.BRsErrorCnt);
		under = (I64U)(_dMBStatData->opstat.BRsCnt);
	} else {
		return _ISDB_MAX_VITERBIBER_;
	}

	if(under==0)
		return _ISDB_MAX_VITERBIBER_;
/*
	over = (overcnt*8*8 + errorcnt) * SCALE_FACTOR;
	under = under*204*8;
*/
	over = (I64U)(((overcnt <<6)+errorcnt)*SCALE_FACTOR);
	under = (I64U)((under*204)<<3);
	result = DIV(over,under);

	VITERBIBER = (I32U)(result & 0xFFFFFFFF);

	if (VITERBIBER > _ISDB_MAX_VITERBIBER_)
		return _ISDB_MAX_VITERBIBER_;
	return VITERBIBER;
}

I32U Tcc353xCalculateTsper(Tcc353xStatus_t * _dMBStatData, I32U oldTsper,
			   I32U _index)
{
	I32U TSPER;
	I64U over;
	I64U under;
	I64U result;

	if(_index>2)
		return _ISDB_MAX_TSPER_;

	if(_dMBStatData->opstat.resynced)
		return _ISDB_MAX_TSPER_;

	if(_index==0) {
		over = (I64U)(_dMBStatData->opstat.ARsOverCnt);
		under = (I64U)(_dMBStatData->opstat.ARsCnt);
	} else if(_index==1) {
		over = (I64U)(_dMBStatData->opstat.BRsOverCnt);
		under = (I64U)(_dMBStatData->opstat.BRsCnt);
	} else {
		return _ISDB_MAX_TSPER_;
	}

	if(under==0)
		return _ISDB_MAX_TSPER_;

	/*
	TSPER = (I32U)((over * SCALE_FACTOR) / under);
	*/
	result = DIV((over * SCALE_FACTOR),under);
	TSPER = (I32U)(result & 0xFFFFFFFF);

	if (TSPER > _ISDB_MAX_TSPER_)
		return _ISDB_MAX_TSPER_;
	return TSPER;
}

#if defined (_USE_LNA_CONTROL_)
extern I32S Lna_Gain_Status[4][4];
#endif

I32S Tcc353xCalculateRssi(I32S _moduleIndex, I32S _diversityIndex, 
			  Tcc353xStatus_t * _isdbStatusData)
{
	I32S RSSI = _ISDB_MIN_RSSI_;

#if defined (_MODEL_F9J_)
#if defined (_USE_LNA_CONTROL_)
	if(Tcc353xApiControl[0][0].tmmMode == 1) {
		if(Lna_Gain_Status[0][0]==ENUM_LNA_GAIN_HIGH)
			RSSI = (I32S)(
				900 - ((I32S) _isdbStatusData->bbLoopGain) *33
				- ((I32S) _isdbStatusData->rfLoopGain) *19);
		else 
			RSSI = (I32S)(
				3000 - ((I32S) _isdbStatusData->bbLoopGain) *33
				- ((I32S) _isdbStatusData->rfLoopGain) *19);
	} else {
		if(Lna_Gain_Status[0][0]==ENUM_LNA_GAIN_HIGH)
			RSSI = (I32S)(
				100 - ((I32S) _isdbStatusData->bbLoopGain) *33
				- ((I32S) _isdbStatusData->rfLoopGain) *19);
		else 
			RSSI = (I32S)(
				2300 - ((I32S) _isdbStatusData->bbLoopGain) *33
				- ((I32S) _isdbStatusData->rfLoopGain) *19);
	}
#else
	if(Tcc353xApiControl[0][0].tmmMode == 1)
		RSSI = (I32S)(
			900 - ((I32S) _isdbStatusData->bbLoopGain) *33
			- ((I32S) _isdbStatusData->rfLoopGain) *19);
	else
		RSSI = (I32S)(
			100 - ((I32S) _isdbStatusData->bbLoopGain) *33
			- ((I32S) _isdbStatusData->rfLoopGain) *19);
#endif
#elif defined (_MODEL_GV_)
	switch(Tcc353xApiControl[_moduleIndex][0].currentBbName) {
	case BB_TCC3530:
		RSSI = (I32S)(
			1800 - ((I32S) _isdbStatusData->bbLoopGain) *33
			- ((I32S) _isdbStatusData->rfLoopGain) *20);
		break;
	case BB_TCC3531:
	case BB_TCC3532:
		RSSI = (I32S)(
			1450 - ((I32S) _isdbStatusData->bbLoopGain) *33 - 
			((I32S) _isdbStatusData->rfLoopGain) *18);
		break;
	case BB_TCC3535:
		RSSI = (I32S)(
			1800 - ((I32S) _isdbStatusData->bbLoopGain) *33
			- ((I32S) _isdbStatusData->rfLoopGain) *20);
		break;
	default:
		TcpalPrintErr((I08S *) "[TCC353X] No baseband name selected\n");
		break;
	}
#elif defined (_MODEL_TCC3535_)
	switch(Tcc353xApiControl[_moduleIndex][0].currentBbName) {
	case BB_TCC3530:
		RSSI = (I32S)(
			1800 - ((I32S) _isdbStatusData->bbLoopGain) *33
			- ((I32S) _isdbStatusData->rfLoopGain) *20);
		break;
	case BB_TCC3531:
	case BB_TCC3532:
		RSSI = (I32S)(
			1450 - ((I32S) _isdbStatusData->bbLoopGain) *33 - 
			((I32S) _isdbStatusData->rfLoopGain) *18);
		break;
	case BB_TCC3535:
		RSSI = (I32S)(
			1800 - ((I32S) _isdbStatusData->bbLoopGain) *33
			- ((I32S) _isdbStatusData->rfLoopGain) *20);
		break;
	default:
		TcpalPrintErr((I08S *) "[TCC353X] No baseband name selected\n");
		break;
	}
#endif

	TcpalPrintStatus((I08S *)"[TCC353X] RSSI[%d] BB[%d] RF[%d]\n", 
			 RSSI, (I32S) (_isdbStatusData->bbLoopGain),
			 (I32S) (_isdbStatusData->rfLoopGain));

	if (RSSI < -10500)
		RSSI = -10500;
	else if (RSSI > 300)
		RSSI = 300;
	return RSSI;
}

static I32U Tcc353xMerTable(I32U _val)
{
	/* mer = 20*log10(lxMer/32) */

	I32U i, MER = 0;
	I32U mer_table[60] = {
		0x1d,
		0x24,
		0x29,
		0x2e,
		0x33,
		0x39,
		0x40,
		0x48,
		0x51,
		0x5b,
		0x66,
		0x72,
		0x80,
		0x8f,
		0xa1,
		0xb4,
		0xca,
		0xe3,
		0xff,
		0x11e,
		0x140,
		0x168,
		0x193,
		0x1c5,
		0x1fc,
		0x23a,
		0x27f,
		0x2cd,
		0x324,
		0x386,
		0x3f4,
		0x470,
		0x4fa,
		0x596,
		0x644,
		0x708,
		0x7e4,
		0x8da,
		0x9ee,
		0xb25,
		0xc80,
		0xe07,
		0xfbd,
		0x11a9,
		0x13d0,
		0x163b,
		0x18f1,
		0x1bfc,
		0x1f67,
		0x233b,
		0x2788,
		0x2c5b,
		0x31c4,
		0x37d6,
		0x3ea6,
		0x464b,
		0x4edf,
		0x587f,
		0x634b,
		0x6f69
	};

	if (_val < mer_table[0]) {
		MER = 0;
	} else if (_val >= mer_table[59]) {
		MER = 60;
	} else {
		for (i = 0; i < 59; i++) {
			if (_val >= mer_table[i]
			    && _val < mer_table[i + 1]) {
				MER = i;
				break;
			}
		}
	}

	return MER;
}

I32U Tcc353xGetUnsignedMovingAvg(I32U * _Array, I32U _input, I32U _count,
				 I32S _maxArray)
{
	I32S j;
	I32S maxroop = 0;
	I32U sum = 0;

	if(_maxArray<1 || _count==0)
		return 0;

	maxroop = _count;

	TcpalMemcpy(_Array, &_Array[1], (_maxArray - 1) * sizeof(I32U));
	_Array[_maxArray - 1] = _input;

	for (j = 0; j < maxroop; j++) {
		if (_maxArray - j - 1 < 0) {
			break;
		}

		sum += _Array[_maxArray - j - 1];
	}

	return (sum / _count);
}

I32S Tcc353xGetSignedMovingAvg(I32S * _Array, I32S _input, I32S _count,
			       I32S _maxArray)
{
	I32S j;
	I32S maxroop = 0;
	I32S sum = 0;

	if(_maxArray<1 || _count==0)
		return 0;

	maxroop = _count;

	TcpalMemcpy(_Array, &_Array[1], (_maxArray - 1) * sizeof(I32S));
	_Array[_maxArray - 1] = _input;

	for (j = 0; j < maxroop; j++) {
		if (_maxArray - j - 1 < 0) {
			break;
		}

		sum += _Array[_maxArray - j - 1];
	}

	return (sum / _count);
}
