/****************************************************************************
 *   FileName    : tcc353x_api.c
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

#include "tcc353x_common.h"
#include "tcpal_os.h"
#include "tcc353x_api.h"

/* extern functions */
extern I32S Tcc353xOpen(I32S _moduleIndex,
			Tcc353xOption_t * _Tcc353xOption);
extern I32S Tcc353xClose(I32S _moduleIndex);
extern I32S Tcc353xInit(I32S _moduleIndex, I08U * _coldbootData,
			I32S _codeSize);
extern I32S Tcc353xChangeToDiversityMode (I32S _mergeIndex, 
					  Tcc353xOption_t * _Tcc353xOption);
extern I32S Tcc353xChangeToDualMode (I32S _devideIndex, 
				     Tcc353xOption_t * _Tcc353xOption);
extern I32S Tcc353xTune(I32S _moduleIndex, I32S _frequency,
			Tcc353xTuneOptions * _tuneOption, I32S _fastTune);
extern I32S Tcc353xStreamStopAll(I32S _moduleIndex);
extern I32S Tcc353xStreamStart(I32S _moduleIndex);
extern I32S Tcc353xMemoryRead(I32S _moduleIndex, I32S _diversityIndex,
			      I32U _address, I08U * _data, I32U _size);
extern I32S Tcc353xMemoryWrite(I32S _moduleIndex, I32S _diversityIndex,
			       I32U _address, I08U * _data, I32U _size);
extern I32S Tcc353xStreamRead(I32S _moduleIndex, I08U * _data, I32S _size);
extern I32S Tcc353xRead(I32S _moduleIndex, I32S _diversityIndex,
			I08U _address, I08U * _data, I32U _size);
extern I32S Tcc353xWrite(I32S _moduleIndex, I32S _diversityIndex,
			 I08U _address, I08U * _data, I32U _size);
extern I32S Tcc353xRfWrite(I32S _moduleIndex, I32S _diversityIndex,
			   I08U addr, I32U data);
extern I32S Tcc353xRfRead(I32S _moduleIndex, I32S _diversityIndex,
			  I08U addr, I32U * data);
extern I32U Tcc353xGetCoreVersion(void);
extern I32S Tcc353xApiSetStreamFormat(I32S _moduleIndex,
				      Tcc353xStreamFormat_t *
				      _streamFormat);
extern I32S Tcc353xRemovePidsFiltering (I32S _moduleIndex, Tcc353xpidTable_t *
				      _pidTableControl);
extern I32S Tcc353xAddPidsFiltering (I32S _moduleIndex, Tcc353xpidTable_t *
				      _pidTableControl);
extern I32S Tcc353xGetpidTable (I32S _moduleIndex,
				      Tcc353xpidTable_t *
				      _pidTableControl);
extern I32S Tcc353xReadIrqError(I32S _moduleIndex, I08U * _data);
extern I32S Tcc353xWriteIrqErrorClear(I32S _moduleIndex);
extern I32S Tcc353xUserCommand(I32S _moduleIndex, I32S _diversityIndex,
			       I32S _command, void *_param1, void *_param2,
			       void *_param3, void *_param4);
extern I32S Tcc353xSetStreamFormat(I32S _moduleIndex,
				   Tcc353xStreamFormat_t * _streamFormat);
extern I32S Tcc353xCasOpen(I32S _moduleIndex, I32U _casRound,
			   I08U * _systemKey);
extern I32S Tcc353xCasSetPid(I32S _moduleIndex, I32U * _pids,
			     I32U _numberOfPids);
extern I32S Tcc353xCasSetKeyMulti2(I32S _moduleIndex, I32S _parity,
				   I08U * _key, I32S _keyLength,
				   I08U * _initVector,
				   I32S _initVectorLength);
extern I32S Tcc353xReadIrqStatus(I32S _moduleIndex, I08U * _data);
extern I32S Tcc353xIrqClear(I32S _moduleIndex, I08U _data);
extern I32S Tcc353xMailboxWrite(I32S _moduleIndex, I32S _diversityIndex,
				I32U _command, I32U * dataArray,
				I32S wordSize);
extern I32S Tcc353xMailboxRead(I32S _moduleIndex, I32S _diversityIndex,
			       I32U _command, mailbox_t * _mailbox);
extern I32S Tcc353xApiGetTMCCInfo(I32S _moduleIndex, I32S _diversityIndex,
				  tmccInfo_t * _tmccInfo);
extern I32S Tcc35xSelectLayer(I32S _moduleIndex, I32S _layer);
extern I32S Tcc353xGetTMCCInfo(I32S _moduleIndex, I32S _diversityIndex,
			       tmccInfo_t * _tmccInfo);

static I32S Tcc353xApiCheckConditions(I32S _moduleIndex);
static I32S Tcc353xGetLockRegister(I08U _data, IsdbLock_t * _isdbLock);
extern I32S Tcc353xGetOpStatus(I32S _moduleIndex, I32S _diversityIndex,
			   I32U* _opStatusData, I32U _dataSize);
static I32S Tcc353xApiWaitLock(I32S _moduleIndex,
			       Tcc353xTuneOptions * _tuneOption,
			       I32S _fastScan);
extern I32S Tcc353xGetFifoStatus(I32S _moduleIndex, I32U *_fifoSize);
extern I32S Tcc353xInterruptBuffClr(I32S _moduleIndex);
extern I32S Tcc353xSetGpioControl(I32S _moduleIndex, I32S _diversityIndex,
				  I32S _gpioNum, I32S _value);


/* global values */
Tcc353xApiControl_t Tcc353xApiControl[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
/* for prevent null access */
I32U Tcc353xApiControlAddress[TCC353X_MAX] = { 0, 0 };

#define MAX_TMM_FREQ_TABLE_NUM	33
I32S TmmFrequencyTable[MAX_TMM_FREQ_TABLE_NUM] = {
	207857, 208286, 208714, 209143, 209571,
	210000, 210429, 210857, 211286, 211714,
	212143, 212571, 213000, 213429, 213857,
	214286, 214714, 215143, 215571, 216000,
	216429, 216857, 217286, 217714, 218143,
	218571, 219000, 219429, 219857, 220286,
	220714, 221143, 221571
};

I32U userStopCmd[TCC353X_MAX] = { 0, 0 };
I32U workingForFlag[TCC353X_MAX] = { 0, 0 };

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiOpen
 * Description
 * 	Openning tcc353x driver
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * 	_Tcc353xOption : Options for driver setting
 * 	_optionSize : size of option data
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	please complete command interface(i2c/tccspi) setting,
 *	power control before api open 
 ---------------------------------------------------------------------*/
I32S Tcc353xApiOpen(I32S _moduleIndex, Tcc353xOption_t * _Tcc353xOption,
		    I32S _optionSize)
{
	I32U needNumberofOptions = 0;
	I32U NumberofBaseband = 1;
	I32U driverCoreVersion = 0x00;
	I32S ret = TCC353X_RETURN_FAIL;

	if (Tcc353xApiControlAddress[_moduleIndex] != 0) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] Tcc353xApiOpen - already opend\n");
		return TCC353X_RETURN_FAIL;
	}

	if (_optionSize > TCC353X_DIVERSITY_MAX * sizeof(Tcc353xOption_t)) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] Tcc353xApiOpen - Too much optionsize\n");
		return TCC353X_RETURN_FAIL;
	} else {
		switch (_Tcc353xOption[0].boardType) {
		case TCC353X_BOARD_SINGLE:
			needNumberofOptions = sizeof(Tcc353xOption_t);
			NumberofBaseband = 1;
			break;

		case TCC353X_BOARD_2DIVERSITY:
			needNumberofOptions =
			    ((sizeof(Tcc353xOption_t)) << 1);
			NumberofBaseband = 2;
			break;

		case TCC353X_BOARD_3DIVERSITY:
			needNumberofOptions =
			    ((sizeof(Tcc353xOption_t)) * 3);
			NumberofBaseband = 3;
			break;

		case TCC353X_BOARD_4DIVERSITY:
			needNumberofOptions =
			    ((sizeof(Tcc353xOption_t)) << 2);
			NumberofBaseband = 4;
			break;

		default:
			needNumberofOptions = sizeof(Tcc353xOption_t);
			NumberofBaseband = 1;
			break;
		}

		if (needNumberofOptions != _optionSize) {
			TcpalPrintErr((I08S *)
				      "[TCC353X] Tcc353xApiOpen - optionsize mismatch\n");
		} else {
			Tcc353xApiControlAddress[_moduleIndex] =
			    (I32U) (&Tcc353xApiControl[_moduleIndex][0]);
			TcpalMemset(&Tcc353xApiControl[_moduleIndex][0], 0,
				    sizeof(Tcc353xApiControl_t) *
				    TCC353X_DIVERSITY_MAX);
			Tcc353xApiControl[_moduleIndex][0].NumberofBaseband
			    = NumberofBaseband;

			Tcc353xApiControl[_moduleIndex][0].currentBbName =
				_Tcc353xOption[0].basebandName;
			driverCoreVersion = Tcc353xGetCoreVersion();
			TcpalPrintLog((I08S *)
				      "[TCC353X] Driver Core Version [%d. %d. %d]\n",
				      ((driverCoreVersion >> 16) & 0xFF),
				      ((driverCoreVersion >> 8) & 0xFF),
				      (driverCoreVersion & 0xFF));
			ret = Tcc353xOpen(_moduleIndex, _Tcc353xOption);
			Tcc353xApiControl[_moduleIndex][0].status =
			    TCC353X_STATUS_OPEND;
		}
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiInit
 * Description
 * 	Initializing tcc353x driver
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_coldbootData : Data of asm file
 *	_codeSize : Coldbootdata size
 *	_streamFormat : stream format option
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	None
 ---------------------------------------------------------------------*/
I32S Tcc353xApiInit(I32S _moduleIndex, I08U * _coldbootData,
		    I32S _codeSize, Tcc353xStreamFormat_t * _streamFormat)
{
	I32S ret;
	if (Tcc353xApiControl[_moduleIndex][0].status ==
	    TCC353X_STATUS_INITED) {
		TcpalPrintErr((I08S *) "[TCC353X] already initialized\n");
		return TCC353X_RETURN_FAIL;
	}

	ret = Tcc353xInit(_moduleIndex, _coldbootData, _codeSize);
	if (ret != TCC353X_RETURN_SUCCESS)
		return ret;

	Tcc353xApiControl[_moduleIndex][0].status = TCC353X_STATUS_INITED;
	ret = Tcc353xSetStreamFormat(_moduleIndex, _streamFormat);
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiClose
 * Description
 * 	Close Driver
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	None
 ---------------------------------------------------------------------*/
I32S Tcc353xApiClose(I32S _moduleIndex)
{
	I32S ret;

	if (Tcc353xApiControlAddress[_moduleIndex] == 0)
		return TCC353X_RETURN_FAIL;

	ret = Tcc353xClose(_moduleIndex);
	Tcc353xApiControl[_moduleIndex][0].status = TCC353X_STATUS_CLOSED;
	Tcc353xApiControlAddress[_moduleIndex] = 0;

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiChangeToDiversityMode
 * Description
 * 	Change dual mode to diversity mode
 * Parameters
 * 	_mergeIndex : merge index
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * 	_Tcc353xOption : Options for driver setting
 * 	_optionSize : size of option data
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Especially when using 2chips
 ---------------------------------------------------------------------*/
I32S Tcc353xApiChangeToDiversityMode (I32S _mergeIndex, 
				      Tcc353xOption_t * _Tcc353xOption,
				      I32S _optionSize)
{
	I32S i;
	I32S ret;
	I32S numberOfBaseband;

	if (Tcc353xApiControlAddress[1] == 0)
		return TCC353X_RETURN_FAIL;

	numberOfBaseband = Tcc353xApiControl[0][0].NumberofBaseband;
	numberOfBaseband += Tcc353xApiControl[1][0].NumberofBaseband;

	if(_mergeIndex>numberOfBaseband)
		return TCC353X_RETURN_FAIL;

	if(_optionSize != sizeof(Tcc353xOption_t)*numberOfBaseband)
		return TCC353X_RETURN_FAIL;

	ret = Tcc353xChangeToDiversityMode(_mergeIndex, _Tcc353xOption);

	for(i = 0; i<numberOfBaseband-_mergeIndex; i++) {
		Tcc353xApiControl[0][_mergeIndex+i].status = 
						TCC353X_STATUS_INITED;
		Tcc353xApiControl[0][_mergeIndex+i].NumberofBaseband = 
						numberOfBaseband;
	}

	for(i = 0; i<_mergeIndex; i++)
		Tcc353xApiControl[0][i].NumberofBaseband = 0;
	
	Tcc353xApiControl[1][0].status = TCC353X_STATUS_CLOSED;
	Tcc353xApiControlAddress[1] = 0;

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiChangeToDualMode
 * Description
 * 	Change diversity mode to dual mode
 * Parameters
 * 	_devideIndex : devide index
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * 	_Tcc353xOption : Options for driver setting
 * 	_optionSize : size of option data
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Especially when using 2chips
 ---------------------------------------------------------------------*/
I32S Tcc353xApiChangeToDualMode (I32S _devideIndex, 
				 Tcc353xOption_t * _Tcc353xOption,
				 I32S _optionSize)
{
	I32S i;
	I32S ret;
	I32S numberOfBaseband;

	if (Tcc353xApiControlAddress[0] == 0)
		return TCC353X_RETURN_FAIL;

	numberOfBaseband = Tcc353xApiControl[0][0].NumberofBaseband;

	if(_devideIndex>numberOfBaseband)
		return TCC353X_RETURN_FAIL;
	
	if(_optionSize != sizeof(Tcc353xOption_t)*numberOfBaseband)
		return TCC353X_RETURN_FAIL;

	ret = Tcc353xChangeToDualMode(_devideIndex, _Tcc353xOption);

	/* to disable mailbox access */
	for(i = _devideIndex; i<numberOfBaseband; i++)
		Tcc353xApiControl[0][i].status = TCC353X_STATUS_CLOSED;
	for(i = 0; i<_devideIndex; i++)
		Tcc353xApiControl[0][i].NumberofBaseband = _devideIndex;

	for(i = 0; i<numberOfBaseband-_devideIndex; i++) {
		Tcc353xApiControl[1][i].status = TCC353X_STATUS_INITED;
		Tcc353xApiControl[1][i].NumberofBaseband = 
					numberOfBaseband-_devideIndex;
	}

	Tcc353xApiControlAddress[1] =
	    (I32U) (&Tcc353xApiControl[1][0]);

	return ret;
}


/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiCheckConditions
 * Description
 * 	Check api condition for access
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	None
 ---------------------------------------------------------------------*/
static I32S Tcc353xApiCheckConditions(I32S _moduleIndex)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiControlAddress[_moduleIndex] == 0) {
		ret = TCC353X_RETURN_FAIL;
	} else if (Tcc353xApiControl[_moduleIndex][0].status !=
		   TCC353X_STATUS_INITED) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = TCC353X_RETURN_SUCCESS;
	}

	return ret;
}

static I32S Tcc353xGetLockTimeOut(I32S _moduleIndex, I32S _step,
					    I32S _fastScan,
					    Tcc353xTuneOptions *
					    _tuneOption)
{
	TcpalTime_t ret = 0;
	I32U tmmMode = 0;
	I32U segments = 13;

	if(Tcc353xApiControl[_moduleIndex][0].tmmMode == 1) {
		tmmMode = 1;
		segments = Tcc353xApiControl[_moduleIndex][0].tmmSegments;
	}
	else {
		switch (_tuneOption->segmentType) {
		case TCC353X_ISDBT_13SEG:
		case TCC353X_ISDBTSB_1SEG:
		case TCC353X_ISDBTSB_3SEG:
			tmmMode = 0;
			segments = 13;
			break;
		case TCC353X_ISDBTMM:
			tmmMode = 1;
			if (_tuneOption->tmmSet == A_1st_13Seg ||
			    _tuneOption->tmmSet == A_2nd_13Seg ||
			    _tuneOption->tmmSet == B_1st_13Seg ||
			    _tuneOption->tmmSet == B_2nd_13Seg ||
			    _tuneOption->tmmSet == C_1st_13Seg ||
			    _tuneOption->tmmSet == C_2nd_13Seg ||
			    _tuneOption->tmmSet == UserDefine_Tmm13Seg) {
				/*13seg */
				segments = 13;
			} else {
				segments = 1;
			}
			break;
		case TCC353X_ISDBT_1_OF_13SEG:
		case TCC353X_ISDBTSB_1_OF_3SEG:
			tmmMode = 0;
			segments = 1;
			break;
		default:
			tmmMode = 0;
			segments = 13;
			break;
		}
	}

	if (_step == 1) {
		/* cto only */
		if(segments == 13 && tmmMode == 0) {
			/* isdb-t 13seg */
			ret =
			(ISDB_AGC_LOCK + ISDB_CTO_LOCK)
			*ISDB_CTO_RETRY;
		}
		else if(segments == 13 && tmmMode == 1) {
			/* tmm 13 seg */
			ret =
			(ISDB_AGC_LOCK_TMM_13 + ISDB_CTO_LOCK_TMM_13)
			*ISDB_CTO_RETRY;
		}
		else if(segments == 1 && tmmMode == 1) {
			/* tmm 1 seg */
			ret =
			(ISDB_AGC_LOCK_TMM_1 + ISDB_CTO_LOCK_TMM_1)
			*ISDB_CTO_RETRY;
		}
		else {
			/* 1 seg */
			ret =
			(ISDB_AGC_LOCK_P_1 + ISDB_CTO_LOCK_P_1)
			*ISDB_CTO_RETRY;
		}
	} else if (_step==2) {
		/* cfo */
		if(segments == 13 && tmmMode == 0) {
			/* isdb-t 13seg */
			ret =
				(ISDB_AGC_LOCK + 
				ISDB_CTO_LOCK+
				ISDB_CFO_LOCK)
				*ISDB_CFO_RETRY;
		}
		else if(segments == 13 && tmmMode == 1) {
			/* tmm 13 seg */
			ret =
				(ISDB_AGC_LOCK_TMM_13 +
				ISDB_CTO_LOCK_TMM_13 +
				ISDB_CFO_LOCK_TMM_13)
				*ISDB_CFO_RETRY;
		}
		else if(segments == 1 && tmmMode == 1) {
			/* tmm 1 seg */
			ret =
				(ISDB_AGC_LOCK_TMM_1 +
				ISDB_CTO_LOCK_TMM_1 +
				ISDB_CFO_LOCK_TMM_1)
				*ISDB_CFO_RETRY;
		}
		else {
			/* 1 seg */
			ret =
				(ISDB_AGC_LOCK_P_1 +
				ISDB_CTO_LOCK_P_1 +
				ISDB_CFO_LOCK_P_1)
				*ISDB_CFO_RETRY;
		}
	} else {
		/* tmcc */
		if(segments == 13 && tmmMode == 0) {
			/* isdb-t 13seg */
			ret = (ISDB_TMCC_LOCK * ISDB_TMCC_RETRY);
		}
		else if(segments == 13 && tmmMode == 1) {
			/* tmm 13 seg */
			ret = (ISDB_TMCC_LOCK_TMM_13 * ISDB_TMCC_RETRY);
		}
		else if(segments == 1 && tmmMode == 1) {
			/* tmm 1 seg */
			ret = (ISDB_TMCC_LOCK_TMM_1 * ISDB_TMCC_RETRY);
		}
		else {
			/* 1 seg */
			ret = (ISDB_TMCC_LOCK_P_1 * ISDB_TMCC_RETRY);
		}
	}
	return (I32S)(ret);
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiWaitLock
 * Description
 * 	Checking frequency locking status
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_tuneOption : Option of tunning parameter, 
 * 		      refefr Tcc353xTuneOptions
 *	_fastScan : MODE3, 1/8 GI only 
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	None
 ---------------------------------------------------------------------*/
static I32S Tcc353xApiWaitLock(I32S _moduleIndex,
			Tcc353xTuneOptions * _tuneOption, I32S _fastScan)
{
	I32S i, j;
	I08U lockRegister;
	IsdbLock_t lock;
	IsdbLock_t lockSub;
	I32S tickMs;
	I32S tickCount;
	I32S subLockOk;
	TcpalTime_t startTime;
	I32S timeOut;

	if (Tcc353xApiControlAddress[_moduleIndex] == 0) {
		return TCC353X_RETURN_FAIL;
	}

	workingForFlag[_moduleIndex] = 1;

	/* -------------------------------------------
	 * 0. AGC Lock : wait
	 * 1. CTO & CFO Detect
	 * 2. TMCC Detect
	 */

	tickMs = 10;

	timeOut =
	    Tcc353xGetLockTimeOut(_moduleIndex, 1, _fastScan,
				     _tuneOption);
	/* CTO Detect */
	subLockOk = 0;
	tickCount = (I32S) ((timeOut+tickMs-1) / tickMs);

	startTime = TcpalGetCurrentTimeCount_ms();

	for (i = 0; i < tickCount; i++) {
		TcpalmSleep(tickMs);

		if(userStopCmd[_moduleIndex] == 1) {
			userStopCmd[_moduleIndex] = 0;
			workingForFlag[_moduleIndex] = 0;
			TcpalPrintLog ((I08S *)"[TCC353X] user stop cmd! quit for loop!\n");
			return TCC353X_RETURN_FAIL;
		}
		
		TcpalMemset(&lock, 0x00, sizeof(IsdbLock_t));
		for (j = 0; j < (I32S) (Tcc353xApiControl[_moduleIndex]
					[0].NumberofBaseband); j++) {
			Tcc353xApiRegisterRead(_moduleIndex, j, 0x0B,
					       &lockRegister, 1);
			Tcc353xGetLockRegister(lockRegister, &lockSub);
			lock.AGC |= lockSub.AGC;
			lock.DCEC |= lockSub.DCEC;
			lock.CTO |= lockSub.CTO;
			lock.CFO |= lockSub.CFO;
			lock.TMCC |= lockSub.TMCC;
		}

		/* lock Success */
		if (lock.CTO && lock.CFO && lock.TMCC) {
			workingForFlag[_moduleIndex] = 0;
			return TCC353X_RETURN_SUCCESS;
		}

		if (lock.CTO) {
			subLockOk = 1;
			break;
		} else {
			if (TcpalGetTimeIntervalCount_ms(startTime) >
			    timeOut) {
				workingForFlag[_moduleIndex] = 0;
				return TCC353X_RETURN_FAIL;
			}
		}
	}

	if (subLockOk == 0)	{/* lock Fail */
		workingForFlag[_moduleIndex] = 0;
		return TCC353X_RETURN_FAIL;
	}

	/* CFO Detect */
	timeOut =
	    Tcc353xGetLockTimeOut(_moduleIndex, 2, _fastScan,
				     _tuneOption);
	subLockOk = 0;
	tickCount = (I32S) ((timeOut+tickMs-1) / tickMs);

	startTime = TcpalGetCurrentTimeCount_ms();

	for (i = 0; i < tickCount; i++) {
		TcpalmSleep(tickMs);

		if(userStopCmd[_moduleIndex] == 1) {
			userStopCmd[_moduleIndex] = 0;
			workingForFlag[_moduleIndex] = 0;
			TcpalPrintLog ((I08S *)"[TCC353X] user stop cmd! quit for loop!\n");
			return TCC353X_RETURN_FAIL;
		}

		TcpalMemset(&lock, 0x00, sizeof(IsdbLock_t));
		for (j = 0; j < (I32S) (Tcc353xApiControl[_moduleIndex]
					[0].NumberofBaseband); j++) {
			Tcc353xApiRegisterRead(_moduleIndex, j, 0x0B,
					       &lockRegister, 1);
			Tcc353xGetLockRegister(lockRegister, &lockSub);
			lock.AGC |= lockSub.AGC;
			lock.DCEC |= lockSub.DCEC;
			lock.CTO |= lockSub.CTO;
			lock.CFO |= lockSub.CFO;
			lock.TMCC |= lockSub.TMCC;
		}

		/* lock Success */
		if (lock.CTO && lock.CFO && lock.TMCC) {
			workingForFlag[_moduleIndex] = 0;
			return TCC353X_RETURN_SUCCESS;
		}

		if (lock.CTO && lock.CFO) {
			subLockOk = 1;
			break;
		} else {
			if (TcpalGetTimeIntervalCount_ms(startTime) >
			    timeOut) {
				workingForFlag[_moduleIndex] = 0;
				return TCC353X_RETURN_FAIL;
			}
		}
	}

	if (subLockOk == 0)	{/* lock Fail */
		workingForFlag[_moduleIndex] = 0;
		return TCC353X_RETURN_FAIL;
	}
	/* TMCC Detect */
	timeOut =
	    Tcc353xGetLockTimeOut(_moduleIndex, 3, _fastScan,
				     _tuneOption);

	/*subLockOk = 0;*/
	tickCount = (I32S) ((timeOut+tickMs-1) / tickMs);

	startTime = TcpalGetCurrentTimeCount_ms();

	for (i = 0; i < tickCount; i++) {
		TcpalmSleep(tickMs);

		if(userStopCmd[_moduleIndex] == 1) {
			userStopCmd[_moduleIndex] = 0;
			workingForFlag[_moduleIndex] = 0;
			TcpalPrintLog ((I08S *)"[TCC353X] user stop cmd! quit for loop!\n");
			return TCC353X_RETURN_FAIL;
		}

		TcpalMemset(&lock, 0x00, sizeof(IsdbLock_t));
		for (j = 0; j < (I32S) (Tcc353xApiControl[_moduleIndex]
					[0].NumberofBaseband); j++) {
			Tcc353xApiRegisterRead(_moduleIndex, j, 0x0B,
					       &lockRegister, 1);
			Tcc353xGetLockRegister(lockRegister, &lockSub);
			lock.AGC |= lockSub.AGC;
			lock.DCEC |= lockSub.DCEC;
			lock.CTO |= lockSub.CTO;
			lock.CFO |= lockSub.CFO;
			lock.TMCC |= lockSub.TMCC;
		}
		if (lock.CTO && lock.CFO && lock.TMCC) {
			/*subLockOk = 1;*/
			workingForFlag[_moduleIndex] = 0;
			return TCC353X_RETURN_SUCCESS;	/* lock Success */
		} else {
			if (TcpalGetTimeIntervalCount_ms(startTime) >
			    timeOut) {
				workingForFlag[_moduleIndex] = 0;
				return TCC353X_RETURN_FAIL;
			}
		}
	}
	workingForFlag[_moduleIndex] = 0;
	return TCC353X_RETURN_FAIL;
}

/*---------------------------------------------------------------------
 * Tcc353xTuneOptions Guide
 *
 *
 * Zero IF - rfIfType : TCC353X_ZERO_IF
 * Low IF - rfIfType : TCC353X_LOW_IF
 *
 *
 * segmentType
 * - TCC353X_ISDBT_1_OF_13SEG : ISDB-T 1Segment
 * - TCC353X_ISDBT_13SEG : ISDB-T Full Segment
 * - TCC353X_ISDBTSB_1SEG : ISDB-Tsb 1Segment
 * - TCC353X_ISDBTSB_3SEG : ISDB-Tsb 3Segment
 * - TCC353X_ISDBTSB_1_OF_3SEG : ISDB-Tsb 1 of 3Segment
 * - TCC353X_ISDBTMM : ISDB-Tmm
 *
 *
 * tmmSet
 * - CaseA/B/C_(n)th_(x)segment
 *   - Case A : 1seg 1seg 1seg 1seg 1seg 1seg 1seg 13seg 13seg
 *   - Case B : 13seg 1seg 1seg 1seg 1seg 1seg 1seg 1seg 13seg
 *   - Case C : 13seg 13seg 1seg 1seg 1seg 1seg 1seg 1seg 1seg
 *
 *       CASE A             CASE B           CASE C
 * Seg13-1seg_1  |   Seg11-13seg_1 =     Seg11-13seg_1 =
 * Seg11-1seg_2  |   Seg9--13seg_1 |     Seg9--13seg_1 |
 * Seg9--1seg_3  |   Seg7--13seg_1 |     Seg7--13seg_1 |
 * Seg7--1seg_4  |   Seg5--13seg_1 |     Seg5--13seg_1 |
 * Seg5--1seg_5  |   Seg3--13seg_1 |     Seg3--13seg_1 |
 * Seg3--1seg_6  |   Seg1--13seg_1 |     Seg1--13seg_1 |
 * Seg1--1seg_7  =   Seg0--13seg_1 |     Seg0--13seg_1 |
 * Seg11-13seg_1=    Seg2--13seg_1 | =   Seg2--13seg_1 |
 * Seg9--13seg_1|    Seg4--13seg_1 | |   Seg4--13seg_1 |
 * Seg7--13seg_1|    Seg6--13seg_1 | |   Seg6--13seg_1 |
 * Seg5--13seg_1|    Seg8--13seg_1 | |   Seg8--13seg_1 |
 * Seg3--13seg_1|    Seg10-13seg_1 | |   Seg10-13seg_1 |
 * Seg1--13seg_1|    Seg12-13seg_1 = |   Seg12-13seg_1 =
 * Seg0--13seg_1|    Seg13-1seg_1    |   Seg11-13seg_2 =
 * Seg2--13seg_1|    Seg11-1seg_2    |   Seg9--13seg_2 |
 * Seg4--13seg_1|    Seg9--1seg_3    |   Seg7--13seg_2 |
 * Seg6--13seg_1|    Seg7--1seg_4    |   Seg5--13seg_2 |
 * Seg8--13seg_1|    Seg5--1seg_5    |   Seg3--13seg_2 |
 * Seg10-13seg_1|    Seg3--1seg_6    |   Seg1--13seg_2 |
 * Seg12-13seg_1=    Seg1--1seg_7    =   Seg0--13seg_2 |
 * Seg11-13seg_2=    Seg11-13seg_2 =     Seg2--13seg_2 |  =
 * Seg9--13seg_2|    Seg9--13seg_2 |     Seg4--13seg_2 |  |
 * Seg7--13seg_2|    Seg7--13seg_2 |     Seg6--13seg_2 |  |
 * Seg5--13seg_2|    Seg5--13seg_2 |     Seg8--13seg_2 |  |
 * Seg3--13seg_2|    Seg3--13seg_2 |     Seg10-13seg_2 |  |
 * Seg1--13seg_2|    Seg1--13seg_2 |     Seg12-13seg_2 =  |
 * Seg0--13seg_2|    Seg0--13seg_2 |     Seg13-1seg_1     |
 * Seg2--13seg_2|    Seg2--13seg_2 |     Seg11-1seg_2     |
 * Seg4--13seg_2|    Seg4--13seg_2 |     Seg9--1seg_3     |
 * Seg6--13seg_2|    Seg6--13seg_2 |     Seg7--1seg_4     |
 * Seg8--13seg_2|    Seg8--13seg_2 |     Seg5--1seg_5     |
 * Seg10-13seg_2|    Seg10-13seg_2 |     Seg3--1seg_6     |
 * Seg12-13seg_2=    Seg12-13seg_2 =     Seg1--1seg_7     =
 *
 ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiChannelSearch
 * Description
 * 	Channel search
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_frequency : Input frequency (khz)
 *	_tuneOption : Option of tunning parameter, 
 * 		      refefr Tcc353xTuneOptions
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Including lock status check routine
 ---------------------------------------------------------------------*/
I32S Tcc353xApiChannelSearch(I32S _moduleIndex, I32S _frequency,
			     Tcc353xTuneOptions * _tuneOption)
{
	I32S subret;
	I32S ret;

	subret =
	    Tcc353xApiChannelSelect(_moduleIndex, _frequency, _tuneOption);
	if (subret == TCC353X_RETURN_SUCCESS) {
		/* lock check */
		if (Tcc353xApiWaitLock(_moduleIndex, _tuneOption, 0) ==
		    TCC353X_RETURN_SUCCESS)
			ret = TCC353X_RETURN_SUCCESS;
		else
			ret = TCC353X_RETURN_FAIL;
	} else {
		ret = TCC353X_RETURN_FAIL;
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiChannelSelect
 * Description
 * 	Channel select
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_frequency : Input frequency (khz)
 *	_tuneOption : Option of tunning parameter, 
 * 		      refefr Tcc353xTuneOptions
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Not include lock status check routine
 ---------------------------------------------------------------------*/
I32S Tcc353xApiChannelSelect(I32S _moduleIndex, I32S _frequency,
			     Tcc353xTuneOptions * _tuneOption)
{
	I32S subret;
	I32S ret;
	I32S i;
	Tcc353xTuneOptions inputOption;

	TcpalMemcpy (&inputOption, _tuneOption, sizeof(Tcc353xTuneOptions));

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		/* check tmmm frequency and then change tmmm mode & frequency */
		Tcc353xApiControl[_moduleIndex][0].tmmMode = 0;

		if(inputOption.segmentType == TCC353X_ISDBT_13SEG ||
		   inputOption.segmentType == TCC353X_ISDBT_1_OF_13SEG) {
		   	I32S seg;
		   	if(inputOption.segmentType == TCC353X_ISDBT_13SEG)
		   		seg = 13;
		   	else
		   		seg = 1;

			for(i = 0; i<MAX_TMM_FREQ_TABLE_NUM; i++) {
				if(_frequency == TmmFrequencyTable[i]) {
					Tcc353xApiControl[_moduleIndex][0].
						tmmMode = 1;
					Tcc353xApiControl[_moduleIndex][0].
						tmmSegments = seg;

					inputOption.segmentType = 
						TCC353X_ISDBTMM;
					if(seg==1)
						inputOption.tmmSet = 
							UserDefine_Tmm1Seg; 
					else
						inputOption.tmmSet = 
							UserDefine_Tmm13Seg;
					break;
				}
			}
		} else if (inputOption.segmentType == TCC353X_ISDBTMM) {
			Tcc353xApiControl[_moduleIndex][0].tmmMode = 1;

			if (inputOption.tmmSet == A_1st_13Seg ||
			    inputOption.tmmSet == B_2nd_13Seg ||
			    inputOption.tmmSet == C_2nd_13Seg ||
			    inputOption.tmmSet == C_1st_13Seg ||
			    inputOption.tmmSet == UserDefine_Tmm13Seg)
				Tcc353xApiControl[_moduleIndex][0].tmmSegments = 13;
			else 
				Tcc353xApiControl[_moduleIndex][0].tmmSegments = 1;
		} else {
			/* none */
		}

		subret =
		    Tcc353xTune(_moduleIndex, _frequency, &inputOption, 0);
		if (subret == TCC353X_RETURN_SUCCESS) {
			ret = TCC353X_RETURN_SUCCESS;
		} else {
			ret = TCC353X_RETURN_FAIL;
		}
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiGetLockStatus
 * Description
 * 	Get locking status
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 * 	_isdbLock : lock status structure
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiGetLockStatus(I32S _moduleIndex, I32S _diversityIndex,
			     IsdbLock_t * _isdbLock)
{
	I32S ret = TCC353X_RETURN_SUCCESS;
	I08U value;

	if (Tcc353xApiControl[_moduleIndex][0].status <
	    TCC353X_STATUS_OPEND) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xApiRegisterRead(_moduleIndex, _diversityIndex,
					     0x0B, &value, 1);
		if (ret == TCC353X_RETURN_FAIL)
			return ret;

		ret = Tcc353xGetLockRegister(value, _isdbLock);
	}
	return ret;
}


/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiGetEwsFlag
 * Description
 * 	Get EWS Flag
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 * Return value
 * 	TCC353X_RETURN_FAIL : Fail
 *	others : EWS Flag
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiGetEwsFlag(I32S _moduleIndex, I32S _diversityIndex)
{
	I32S ret = TCC353X_RETURN_SUCCESS;
	I08U value;
	IsdbLock_t lock;

	if (Tcc353xApiControl[_moduleIndex][0].status <
	    TCC353X_STATUS_OPEND) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xApiRegisterRead(_moduleIndex, _diversityIndex,
					   0x0B, &value, 1);
		if (ret == TCC353X_RETURN_FAIL)
			return ret;

		Tcc353xGetLockRegister(value, &lock);
		ret = lock.EWSFlag;
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiRegisterRead
 * Description
 * 	Read baseband register
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_address : Register address
 *	_data : Register data
 *	_size : Register size
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiRegisterRead(I32S _moduleIndex, I32S _diversityIndex,
			    I08U _address, I08U * _data, I32U _size)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiControl[_moduleIndex][0].status <
	    TCC353X_STATUS_OPEND) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xRead(_moduleIndex, _diversityIndex, _address,
				_data, _size);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiRegisterWrite
 * Description
 * 	Write baseband register
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_address : Register address
 *	_data : Register data
 *	_size : Register size
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiRegisterWrite(I32S _moduleIndex, I32S _diversityIndex,
			     I08U _address, I08U * _data, I32U _size)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiControl[_moduleIndex][0].status <
	    TCC353X_STATUS_OPEND) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xWrite(_moduleIndex, _diversityIndex, _address,
				 _data, _size);
	}

	return ret;

}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiRfRegisterRead
 * Description
 * 	Read rf register
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_address : Register address
 *	_data : Register data
 *	_size : Register size
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiRfRegisterRead(I32S _moduleIndex, I32S _diversityIndex,
			      I08U _address, I32U * _data)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		Tcc353xRfRead(_moduleIndex, _diversityIndex, _address,
			      _data);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiRfRegisterWrite
 * Description
 * 	Write rf register
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_address : Register address
 *	_data : Register data
 *	_size : Register size
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiRfRegisterWrite(I32S _moduleIndex, I32S _diversityIndex,
			       I08U _address, I32U _data)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		Tcc353xRfWrite(_moduleIndex, _diversityIndex, _address,
			       _data);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiSetStreamFormat
 * Description
 * 	Set stream format
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_streamFormat : stream format option
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Plase set after Tcc353xApiInit
 ---------------------------------------------------------------------*/
I32S Tcc353xApiSetStreamFormat(I32S _moduleIndex,
			       Tcc353xStreamFormat_t * _streamFormat)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xSetStreamFormat(_moduleIndex, _streamFormat);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiAddPids
 * Description
 * 	Add Pid for pid filtering
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_pidTableControl : pid table
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiAddPids(I32S _moduleIndex, Tcc353xpidTable_t *
				      _pidTableControl)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		if (_pidTableControl == NULL)
			return TCC353X_RETURN_FAIL_NULL_ACCESS;

		ret = Tcc353xAddPidsFiltering(_moduleIndex, _pidTableControl);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiRemovePids
 * Description
 * 	Remove pid from pid filter table
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_pidTableControl : pid table
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiRemovePids(I32S _moduleIndex, Tcc353xpidTable_t *
				      _pidTableControl)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		if (_pidTableControl == NULL)
			return TCC353X_RETURN_FAIL_NULL_ACCESS;

		ret = Tcc353xRemovePidsFiltering(_moduleIndex, 
						 _pidTableControl);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiGetpidTable
 * Description
 * 	Get pid tables
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_pidTableControl : pid table option
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiGetpidTable(I32S _moduleIndex,
				  Tcc353xpidTable_t * _pidTableControl)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		if (_pidTableControl == NULL)
			return TCC353X_RETURN_FAIL_NULL_ACCESS;

		ret =
		    Tcc353xGetpidTable(_moduleIndex,
					      _pidTableControl);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiStreamStop
 * Description
 * 	Stopping stream output
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	please use this function before change channel or close
 ---------------------------------------------------------------------*/
I32S Tcc353xApiStreamStop(I32S _moduleIndex)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xStreamStopAll(_moduleIndex);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiStreamStart
 * Description
 * 	Restart stream output
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Do not use this function except for special purpose
 ---------------------------------------------------------------------*/
I32S Tcc353xApiStreamStart(I32S _moduleIndex)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xStreamStart(_moduleIndex);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiStreamRead
 * Description
 * 	Get stream output from memory
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * 	_data : data
 *	_size : size (use threshold value)
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Use this function if use interrupt stream control
 ---------------------------------------------------------------------*/
I32S Tcc353xApiStreamRead(I32S _moduleIndex, I08U * _data, I32S _size)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xStreamRead(_moduleIndex, _data, _size);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiMemoryWrite
 * Description
 * 	Write some data to tcc353x memory
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_address : memory address
 *	_data : data
 *	_size : size
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Do not use this function except for special purpose
 ---------------------------------------------------------------------*/
I32S Tcc353xApiMemoryWrite(I32S _moduleIndex, I32S _diversityIndex,
			   I32U _address, I08U * _data, I32U _size)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xMemoryWrite(_moduleIndex, _diversityIndex,
				       _address, _data, _size);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiMemoryRead
 * Description
 * 	Read some data to tcc353x memory
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_address : memory address
 *	_data : data
 *	_size : size
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Do not use this function except for special purpose
 ---------------------------------------------------------------------*/
I32S Tcc353xApiMemoryRead(I32S _moduleIndex, I32S _diversityIndex,
			  I32U _address, I08U * _data, I32U _size)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xMemoryRead(_moduleIndex, _diversityIndex,
				      _address, _data, _size);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiGetIrqError
 * Description
 * 	Get interrupt error value
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_data : data
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Use this function if use interrupt stream control
 ---------------------------------------------------------------------*/
I32S Tcc353xApiGetIrqError(I32S _moduleIndex, I08U * _data)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xReadIrqError(_moduleIndex, _data);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiSetIrqErrorClear
 * Description
 * 	Clear interrupt error value
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Use this function if use interrupt stream control
 ---------------------------------------------------------------------*/
I32S Tcc353xApiSetIrqErrorClear(I32S _moduleIndex)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xWriteIrqErrorClear(_moduleIndex);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiGetIrqStatus
 * Description
 * 	Get interrupt status value
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_data : data
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Use this function if use interrupt stream control
 ---------------------------------------------------------------------*/
I32S Tcc353xApiGetIrqStatus(I32S _moduleIndex, I08U * _data)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xReadIrqStatus(_moduleIndex, _data);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiIrqClear
 * Description
 * 	Clear irq values
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Use this function if use interrupt stream control
 ---------------------------------------------------------------------*/
I32S Tcc353xApiIrqClear(I32S _moduleIndex, I08U _data)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xIrqClear(_moduleIndex, _data);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiMailboxWrite
 * Description
 * 	Write some values using mailbox
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_command : mailbox command
 *	_dataArray : data
 *	wordSize : data size
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Do not use this function except for special purpose
 ---------------------------------------------------------------------*/
I32S Tcc353xApiMailboxWrite(I32S _moduleIndex, I32S _diversityIndex,
			    I32U _command, I32U * _dataArray,
			    I32S wordSize)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xMailboxWrite(_moduleIndex, _diversityIndex,
					_command, _dataArray, wordSize);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiMailboxRead
 * Description
 * 	Read some values using mailbox
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_command : mailbox command
 *	_mailbox : mailbox structure
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Do not use this function except for special purpose
 ---------------------------------------------------------------------*/
I32S Tcc353xApiMailboxRead(I32S _moduleIndex, I32S _diversityIndex,
			   I32U _command, mailbox_t * _mailbox)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xMailboxRead(_moduleIndex, _diversityIndex,
				       _command, _mailbox);
	}
	return ret;
}


/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiGetTMCCInfo
 * Description
 * 	Get TMCC Informations
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_tmccInfo: tmcc information structure
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiGetTMCCInfo(I32S _moduleIndex, I32S _diversityIndex,
			   tmccInfo_t * _tmccInfo)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret =
		    Tcc353xGetTMCCInfo(_moduleIndex, _diversityIndex,
				       _tmccInfo);
	}
	return ret;
}


/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiCasOpen
 * Description
 * 	initialize cas decoder
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_casRound : cas round value
 *	_systemKey : systemkey
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiCasOpen(I32S _moduleIndex, I32U _casRound,
		       I08U * _systemKey)
{
	if (Tcc353xApiCheckConditions(_moduleIndex) != TCC353X_RETURN_SUCCESS)
		return TCC353X_RETURN_FAIL;

	return (Tcc353xCasOpen(_moduleIndex, _casRound, _systemKey));
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiCasSetPid
 * Description
 * 	Set pid value to decrypt cas
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_pids : pid value
 *	_numberOfPids : number of pid values
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiCasSetPid(I32S _moduleIndex, I32U * _pids,
			 I32U _numberOfPids)
{
	if (Tcc353xApiCheckConditions(_moduleIndex) != TCC353X_RETURN_SUCCESS)
		return TCC353X_RETURN_FAIL;

	return (Tcc353xCasSetPid(_moduleIndex, _pids, _numberOfPids));
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiCasSetKeyMulti2
 * Description
 * 	Set cas key (multi2)
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_parity : parity value odd or even
 *	_key	: key value
 *	_keyLength : length of key value
 *	_initVector : initial vector value
 *	_initVectorLength : length of initial value
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiCasSetKeyMulti2(I32S _moduleIndex, I32S _parity,
			       I08U * _key, I32S _keyLength,
			       I08U * _initVector, I32S _initVectorLength)
{
	if (Tcc353xApiCheckConditions(_moduleIndex) != TCC353X_RETURN_SUCCESS)
		return TCC353X_RETURN_FAIL;

	return (Tcc353xCasSetKeyMulti2
		(_moduleIndex, _parity, _key, _keyLength, _initVector,
		 _initVectorLength));
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiUserCommand
 * Description
 * 	command for special purpose
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_command : command
 *	_paramN : Parameters
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	other user command for special usages
 ---------------------------------------------------------------------*/
I32S Tcc353xApiUserCommand(I32S _moduleIndex, I32S _diversityIndex,
			   I32S _command, void *_param1, void *_param2,
			   void *_param3, void *_param4)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		/* reserved to debugging or patch */
		ret =
		    Tcc353xUserCommand(_moduleIndex, _diversityIndex,
				       _command, _param1, _param2, _param3,
				       _param4);
	}

	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiParseIsdbSyncStat
 * Description
 * 	Parse isdb sync status values
 * Parameters
 * 	_isdbLock : lock status structure
 * 	_input : input value
 * Return value
 * 	None
 * Remark
 * 	
 ---------------------------------------------------------------------*/
void Tcc353xApiParseIsdbSyncStat(IsdbLock_t * _isdbLock, I08U _input)
{
	_isdbLock->AGC = ((_input) & 0x01);
	_isdbLock->DCEC = ((_input >> 1) & 0x01);
	_isdbLock->CTO = ((_input >> 2) & 0x01);
	_isdbLock->CFO = ((_input >> 3) & 0x01);
	_isdbLock->TMCC = ((_input >> 4) & 0x01);
	_isdbLock->EWSFlag = ((_input >> 6) & 0x01);
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xGetLockRegister
 * Description
 * 	Get lock status
 * Parameters
 * 	_data : data
 * 	_isdbLock : lock status structure
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
static I32S Tcc353xGetLockRegister(I08U _data, IsdbLock_t * _isdbLock)
{
	Tcc353xApiParseIsdbSyncStat(_isdbLock, _data);
	return TCC353X_RETURN_SUCCESS;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiOpStatusRead
 * Description
 * 	Read some values using opstatus
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 *	_diversityIndex : Index of diversity
 *		0 : Diversity master
 *		1 : Diversity slave#1
 *		2 : Diversity slave#2
 *		3 : Diversity slave#3
 *	_dataSize : read opstatus data size
 *	_datas : receive data pointer
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	Do not use this function except for special purpose
 ---------------------------------------------------------------------*/
I32S Tcc353xApiOpStatusRead(I32S _moduleIndex, I32S _diversityIndex,
			   I32U _dataSize, I32U * _datas)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xGetOpStatus(_moduleIndex, _diversityIndex,
					   _datas, _dataSize);
	}
	return ret;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiGetFifoStatus
 * Description
 * 	Get fifo status (size)
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * 	_fifoSize : fifosize
 * Return value
 * 	Refer EnumReturn
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiGetFifoStatus(I32S _moduleIndex, I32U *_fifoSize)
{
	if (Tcc353xApiCheckConditions(_moduleIndex) 
	    != TCC353X_RETURN_SUCCESS) {
		_fifoSize[0] = 0;
		return TCC353X_RETURN_FAIL;
	}

	Tcc353xGetFifoStatus(_moduleIndex, _fifoSize);
	return TCC353X_RETURN_SUCCESS;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiInterruptBuffClr
 * Description
 * 	buffer clear when fifo overflow
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiInterruptBuffClr(I32S _moduleIndex)
{
	if (Tcc353xApiCheckConditions(_moduleIndex) != TCC353X_RETURN_SUCCESS)
		return TCC353X_RETURN_FAIL;

	Tcc353xInterruptBuffClr(_moduleIndex);
	return TCC353X_RETURN_SUCCESS;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiUserLoopStopCmd
 * Description
 * 	stopping for loop
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiUserLoopStopCmd(I32S _moduleIndex)
{
	if(workingForFlag[_moduleIndex] == 1)
		userStopCmd[_moduleIndex] = 1;
	else 
		userStopCmd[_moduleIndex] = 0;
	return TCC353X_RETURN_SUCCESS;
}

/*---------------------------------------------------------------------
 * Function name
 * 	Tcc353xApiSetGpioControl
 * Description
 * 	stopping for loop
 * Parameters
 * 	_moduleIndex : Index of module
 *		0 : BaseBand#0 (Single : default, Dual : BaseBand#0)
 * 		1 : BaseBand#1 (Single : Not use, Dual : BaseBand#1)
 * Return value
 * 	
 * Remark
 * 	
 ---------------------------------------------------------------------*/
I32S Tcc353xApiSetGpioControl(I32S _moduleIndex, I32S _diversityIndex, 
			   I32S _gpioNum, I32S _value)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xApiCheckConditions(_moduleIndex) !=
	    TCC353X_RETURN_SUCCESS) {
		ret = TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xSetGpioControl(_moduleIndex, _diversityIndex,
					   _gpioNum, _value);
	}
	return ret;
}

