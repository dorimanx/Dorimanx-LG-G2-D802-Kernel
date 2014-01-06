/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_rf.c                                            */
/*    Description : Rf Function                                             */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include "tcc353x_rf.h"
#include "tcc353x_command_control.h"
#include "tcpal_os.h"

/* extern function */
I64U Tcc353xDiv64(I64U x, I64U y);

#define SCALE       22
#define FIXED(x)    (x<<SCALE)
#define MUL(A,B)    ((A*B)>>SCALE)
#define DIV(A,B)    (Tcc353xDiv64((A<<SCALE), B))

#ifndef Bit0
#define Bit31       0x80000000
#define Bit30       0x40000000
#define Bit29       0x20000000
#define Bit28       0x10000000
#define Bit27       0x08000000
#define Bit26       0x04000000
#define Bit25       0x02000000
#define Bit24       0x01000000
#define Bit23       0x00800000
#define Bit22       0x00400000
#define Bit21       0x00200000
#define Bit20       0x00100000
#define Bit19       0x00080000
#define Bit18       0x00040000
#define Bit17       0x00020000
#define Bit16       0x00010000
#define Bit15       0x00008000
#define Bit14       0x00004000
#define Bit13       0x00002000
#define Bit12       0x00001000
#define Bit11       0x00000800
#define Bit10       0x00000400
#define Bit9        0x00000200
#define Bit8        0x00000100
#define Bit7        0x00000080
#define Bit6        0x00000040
#define Bit5        0x00000020
#define Bit4        0x00000010
#define Bit3        0x00000008
#define Bit2        0x00000004
#define Bit1        0x00000002
#define Bit0        0x00000001
#define BitNONE     0x00000000
#endif

#ifndef BITSET
#define BITSET(X, MASK)             ( (X) |= (I32U)(MASK) )
#endif
#ifndef BITCLR
#define BITCLR(X, MASK)             ( (X) &= ~((I32U)(MASK)) )
#endif

#define _IDX_ADDRESS_		0
#define _IDX_ACCESSABLE_	1
#define _IDX_FM_		2
#define _IDX_VHF_		3
#define _IDX_UHF_		4

#define _RFREG_CNT_	0x10
#define _RFREG_FORM_	5

extern Tcc353xHandle_t Tcc353xHandle[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
I32U Tcc353xRfRegSRVersion[_RFREG_CNT_][_RFREG_FORM_] = {
	/* TCC353XRFREGSRVERSION[ADDRESSINDEX][ADDRESS, ACCESS_ABLE, FM, VHF, UHF] */
	{0x00, 0, 0x35300000, 0x35300000, 0x35300000},
	{0x01, 1, 0x80000000, 0x80000000, 0x80000000},
	{0x02, 0, 0x00020000, 0x00020000, 0x00020000},
	{0x03, 0, 0x00000000, 0x00000000, 0x00000000},
	{0x04, 1, 0x00020003, 0x00020005, 0x00020006},
	{0x05, 1, 0x5435343D, 0x5435343D, 0x5435343D},
	{0x06, 1, 0x55555555, 0x55555555, 0x55555555},
	{0x07, 1, 0x64202F15, 0x64202F13, 0x64202F11},
	{0x08, 1, 0x80000000, 0x80000000, 0x80000000},
	{0x09, 1, 0x00868939, 0x00864989, 0x00768989},
	{0x0A, 1, 0x13773300, 0x13773300, 0x09772900},
	{0x0B, 1, 0x961B0D04, 0x961B0D04, 0x961B0D04},
	{0x0C, 1, 0x040404E0, 0x040404E0, 0x040404E0},
	{0x0D, 1, 0x021DDFFD, 0x021DDFFD, 0x021DDFFD},
	{0x0E, 1, 0x7F603048, 0x7F602860, 0x7F043060},
	{0x0F, 1, 0x0000007F, 0x0000007F, 0x0000007F} 
};

I32S Tcc353xRfBandIndex[TCC353X_MAX][TCC353X_DIVERSITY_MAX];

static I32S Tcc353xRfBandChange(I32S _moduleIndex, I32S _diversityIndex, 
				I32S _bandIndex)
{
	I32U i = 0;
	I32U tcc353xRfReg[_RFREG_CNT_][_RFREG_FORM_];
	I08U addressArray[0x10];
	I32U dataArray[0x10];
	I32U arraySize = 0;

	if (_bandIndex != _IDX_FM_ &&  _bandIndex != _IDX_VHF_ 
	    && _bandIndex != _IDX_UHF_)
	{
		Tcc353xRfBandIndex[_moduleIndex][_diversityIndex] = -1;
		return TCC353X_RETURN_FAIL;
	}

	/* check old band index */
	/* if same, skip */
	if (_bandIndex == Tcc353xRfBandIndex[_moduleIndex][_diversityIndex])
		return TCC353X_RETURN_SUCCESS;

	Tcc353xRfBandIndex[_moduleIndex][_diversityIndex] = _bandIndex;

	TcpalMemcpy(&tcc353xRfReg, &Tcc353xRfRegSRVersion,
		    sizeof(tcc353xRfReg));

	for (i = 0; i < _RFREG_CNT_; i++) {
		if (tcc353xRfReg[i][_IDX_ACCESSABLE_])
		{
			addressArray[arraySize] = (I08U)(i);
			dataArray[arraySize] = tcc353xRfReg[i][_bandIndex];
			arraySize++;
		}
	}

	Tcc353xRfWriteEx(_moduleIndex, _diversityIndex, &addressArray[0], 
			 &dataArray[0], arraySize);

	if(_bandIndex == _IDX_FM_)
		TcpalPrintLog((I08S *) "[TCC353X] Band Switch to FM\n");
	else if(_bandIndex == _IDX_VHF_)
		TcpalPrintLog((I08S *) "[TCC353X] Band Switch to VHF\n");
	else
		TcpalPrintLog((I08S *) "[TCC353X] Band Switch to UHF\n");
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xRfInit(I32S _moduleIndex, I32S _diversityIndex)
{
	I32U RfId = 0x00;

	/* set rf band unknown */
	Tcc353xRfBandIndex[_moduleIndex][_diversityIndex] = -1;

	Tcc353xRfRead(_moduleIndex, _diversityIndex, 0x00, &RfId);
	if (RfId != 0x35300000) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] RF ID Read Error [%d][%d][0x%x]\n",
			      _moduleIndex, _diversityIndex, RfId);
		return TCC353X_RETURN_FAIL;
	} else {
		TcpalPrintLog((I08S *) 
			      "[TCC353X] RF ID Read OK.[%d][%d][0x%x]\n",
			      _moduleIndex, _diversityIndex, RfId);
	}

	/* default rf band UHF */
	Tcc353xRfBandChange(_moduleIndex, _diversityIndex, _IDX_UHF_);
	return TCC353X_RETURN_SUCCESS;
}

void Tcc353xRfTune(I32S _moduleIndex, I32S _diversityIndex, I32S _freq_khz,
		   I32S _bw_khz, I32S _oscClk,
		   Tcc353xTuneOptions * _tuneOption)
{
	I64U N, F;
	I64U Flo, VCO_DIV, FOffset, Fvco, FR;
	I64U N_int, intF;
	I64U fXtal, fpfd, f_freq_khz;
	I32U OSCCLK;
	I08U RCNT_RDC = 0;
	I08U REG_VCO_DIV;
	I32U Icp;
	I32S segmentsNum = 13;
	I08U i;
	I32U maxFmFreq = 138000;
	I32U maxVhfFreq = 276000;
	I32U rfReg[0x10];
	I32U bandIdx = _IDX_UHF_;
	I64U pllMode = 2;
	I32U tcc353xRfReg[_RFREG_CNT_][_RFREG_FORM_];
	I08U addressArray[0x10];
	I32U dataArray[0x10];
	I32U arraySize = 0;
	I32U use_lowif = 0;
	I32U use_seg = 13;

	maxFmFreq = 138000;
	maxVhfFreq = 276000;

	TcpalMemcpy(&tcc353xRfReg, &Tcc353xRfRegSRVersion,
		    sizeof(tcc353xRfReg));

	if ((I32U)(_freq_khz) < maxFmFreq)	/* fm */
		bandIdx = _IDX_FM_;
	else if ((I32U)(_freq_khz) < maxVhfFreq)	/* vhf */
		bandIdx = _IDX_VHF_;
	else			/* uhf */
		bandIdx = _IDX_UHF_;

	Tcc353xRfBandChange(_moduleIndex, _diversityIndex, bandIdx);

	/* copy to sub reg */
	for (i = 0; i < _RFREG_CNT_; i++)
		rfReg[i] = tcc353xRfReg[i][bandIdx];

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		if (_tuneOption->rfIfType != TCC353X_ZERO_IF)
			_freq_khz += 1000;
		segmentsNum = 1;
		break;
	case TCC353X_ISDBTSB_1SEG:
	case TCC353X_ISDBTSB_1_OF_3SEG:
		if (_tuneOption->rfIfType != TCC353X_ZERO_IF)
			_freq_khz += 1000;
		break;
	case TCC353X_ISDBTSB_3SEG:
		if (_tuneOption->rfIfType != TCC353X_ZERO_IF)
			_freq_khz += 1000;
		segmentsNum = 3;
		break;
	case TCC353X_ISDBT_13SEG:
		segmentsNum = 13;
		break;
	case TCC353X_ISDBTMM:
		/* reserved */
		if (_tuneOption->rfIfType != TCC353X_ZERO_IF) {
			if(_tuneOption->tmmSet != A_1st_13Seg &&
			_tuneOption->tmmSet != A_2nd_13Seg &&
			_tuneOption->tmmSet != B_1st_13Seg &&
			_tuneOption->tmmSet != B_2nd_13Seg &&
			_tuneOption->tmmSet != C_1st_13Seg &&
			_tuneOption->tmmSet != C_2nd_13Seg &&
			_tuneOption->tmmSet != UserDefine_Tmm13Seg)
			/* 1seg : + 500khz, 3seg : +1000khz */
				_freq_khz += 1000;
		}
		break;
	default:
		/* default full segment */
		segmentsNum = 13;
		break;
	}

	OSCCLK = _oscClk;
	fXtal = OSCCLK;
	f_freq_khz = _freq_khz;

	/* Calculate PLL */
	if (f_freq_khz < 65000) {
		VCO_DIV = 48;
		REG_VCO_DIV = 0x07;
		pllMode = 2;
	}

	else if (f_freq_khz < 90000) {
		VCO_DIV = 32;
		REG_VCO_DIV = 0x06;
		pllMode = 2;
	}

	else if (f_freq_khz < 138000) {
		VCO_DIV = 24;
		REG_VCO_DIV = 0x05;
		pllMode = 2;
	}

	else if (f_freq_khz < 182000) {
		VCO_DIV = 16;
		REG_VCO_DIV = 0x04;
		pllMode = 2;
	}

	else if (f_freq_khz < 276000) {
		VCO_DIV = 12;
		REG_VCO_DIV = 0x03;
		pllMode = 2;
	}

	else if (f_freq_khz < 366000) {
		VCO_DIV = 8;
		REG_VCO_DIV = 0x02;
		pllMode = 2;
	}

	else if (f_freq_khz < 580000) {
		VCO_DIV = 6;
		REG_VCO_DIV = 0x01;
		pllMode = 2;
	}

	else {
		VCO_DIV = 4;
		REG_VCO_DIV = 0x00;
		pllMode = 3;
	}

	FOffset = 0;
	FR = 1;
	/* fdfd = fxtal/FR */
	fpfd = DIV(fXtal, FR);
	fpfd = (fpfd >> SCALE);

	Flo = f_freq_khz - FOffset;
	Fvco = Flo * VCO_DIV;

	/* N = Fvco / (pllMode*fpfd); */
	N = DIV(Fvco, pllMode * fpfd);
	N_int = (N >> SCALE) << SCALE;

	/* F = ((Fvco / 2) / fpfd - (double) N_int) * (2 << 21); */
	F = ((N - N_int) * (2 << 21)) >> SCALE;
	N_int = (N_int >> SCALE);

	intF = F;
	RCNT_RDC = (I08U) (FR);
	Icp = 4 << 24;

	/* also disable auto rst */
	BITCLR(tcc353xRfReg[0x05][bandIdx], 0x07000001);
	BITCLR(tcc353xRfReg[0x07][bandIdx], 0x0000007F);
	BITCLR(tcc353xRfReg[0x08][bandIdx], 0x3FFFFFFF);

	rfReg[0x05] = ((tcc353xRfReg[0x05][bandIdx] & 0xF8FFFFFF) | Icp);
	rfReg[0x07] = tcc353xRfReg[0x07][bandIdx] & 0xFFFFFF80;

	if (pllMode == 2)
		rfReg[0x07] |= RCNT_RDC << 4 | (REG_VCO_DIV & 0x07);
	else
		rfReg[0x07] |=
		    RCNT_RDC << 4 | 1 << 3 | (REG_VCO_DIV & 0x07);

	rfReg[0x08] =(I32U)
	    (tcc353xRfReg[0x08][bandIdx] | (N_int & 0xFF) |
	     ((intF & 0x3FFFFF) << 8));

	/* setting 0x04 Reg */
	if(bandIdx == _IDX_UHF_)
		rfReg[0x04] = ((rfReg[0x04] & 0xFFFFFFF8) | 0x06);
	else if(bandIdx == _IDX_VHF_)
		rfReg[0x04] = ((rfReg[0x04] & 0xFFFFFFF8) | 0x05);
	else
		rfReg[0x04] = ((rfReg[0x04] & 0xFFFFFFF8) | 0x03);

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
	case TCC353X_ISDBTSB_1SEG:
	case TCC353X_ISDBTSB_1_OF_3SEG:
		use_seg = 1;
		if (_tuneOption->rfIfType == TCC353X_LOW_IF)
			use_lowif = 1;
		else
			use_lowif = 0;
		break;
	case TCC353X_ISDBTSB_3SEG:
		use_seg = 3;
		if (_tuneOption->rfIfType == TCC353X_LOW_IF)
			use_lowif = 1;
		else
			use_lowif = 0;
		break;
	case TCC353X_ISDBT_13SEG:
		use_seg = 13;
		use_lowif = 0;
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == A_1st_13Seg ||
		    _tuneOption->tmmSet == A_2nd_13Seg ||
		    _tuneOption->tmmSet == B_1st_13Seg ||
		    _tuneOption->tmmSet == B_2nd_13Seg ||
		    _tuneOption->tmmSet == C_1st_13Seg ||
		    _tuneOption->tmmSet == C_2nd_13Seg ||
		    _tuneOption->tmmSet == UserDefine_Tmm13Seg) {
		    use_seg = 13;
		    use_lowif = 0;
		} else {
			use_seg = 1;
			if (_tuneOption->rfIfType == TCC353X_LOW_IF)
				use_lowif = 1;
			else
				use_lowif = 0;
		}
		break;
	default:
		break;
	}

	if(use_lowif)
		rfReg[0x04] = ((rfReg[0x04] & 0xFFFFF9FF) | 0x0200);
	else
		rfReg[0x04] = (rfReg[0x04] & 0xFFFFF9FF);

	/* setting 0x0C Reg */
	BITCLR(rfReg[0x0C], 0x3F00);
	if (use_seg==13)
		BITSET(rfReg[0x0C], 0x400);
	else if (use_seg==1)	/* rcv as 3seg */
		BITSET(rfReg[0x0C], 0x200);
	else
		BITSET(rfReg[0x0C], 0x200);

	BITCLR(rfReg[0x0C], 0x000F4004);
	if(use_lowif)
		BITSET(rfReg[0x0C], 0x000F4004);
	else
		BITSET(rfReg[0x0C], 0x00040000);

	BITCLR(rfReg[0x0C], 0x80);
	if((use_lowif==1)||(use_seg==13))
		BITSET(rfReg[0x0C], 0x80);

	/* tunning 0x09 register address */
	if(_tuneOption->segmentType == TCC353X_ISDBTMM) {
		rfReg[0x09] = 0x00861989;
	}
	else
	{
		if(bandIdx == _IDX_UHF_) {
			if(_freq_khz >= 773143)
				rfReg[0x09] = 0x00068989;
			else if(_freq_khz >= 737143)
				rfReg[0x09] = 0x00168989;
			else if(_freq_khz >= 683143)
				rfReg[0x09] = 0x00268989;
			else if(_freq_khz >= 623143)
				rfReg[0x09] = 0x00368989;
			else if(_freq_khz >= 563143)
				rfReg[0x09] = 0x00468989;
			else if(_freq_khz >= 527143)
				rfReg[0x09] = 0x00568989;
			else if(_freq_khz >= 497143)
				rfReg[0x09] = 0x00668989;
			else
				rfReg[0x09] = 0x00768989;
		} else if (bandIdx == _IDX_VHF_) {
			if(_freq_khz >= 207143)
				rfReg[0x09] = 0x00863989;
			else if(_freq_khz >= 191143)
				rfReg[0x09] = 0x00864989;
			else if(_freq_khz >= 185143)
				rfReg[0x09] = 0x00865989;
			else
				rfReg[0x09] = 0x00866989;
		}
	}

	/* caution!! set 0x08 reg after set 0x05 */
	/* 0x05 Auto Rst -> 0
	 * 0x08 RST PLL -> 1->0
	 */
	addressArray[0] = 0x04;
	addressArray[1] = 0x05;
	addressArray[2] = 0x07;
	addressArray[3] = 0x08;
	addressArray[4] = 0x09;
	addressArray[5] = 0x0C;

	BITSET(rfReg[0x08], 0x80000000);

	dataArray[0] = rfReg[0x04];
	dataArray[1] = rfReg[0x05];
	dataArray[2] = rfReg[0x07];
	dataArray[3] = rfReg[0x08];
	dataArray[4] = rfReg[0x09];
	dataArray[5] = rfReg[0x0C];

	arraySize = 6;

	Tcc353xRfWriteEx(_moduleIndex, _diversityIndex, &addressArray[0],
		         &dataArray[0], arraySize);

	BITCLR(rfReg[0x08], 0x80000000);
	Tcc353xRfWrite(_moduleIndex, _diversityIndex, 0x08, rfReg[0x08]);

}

I64U Tcc353xDiv64(I64U x, I64U y)
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
