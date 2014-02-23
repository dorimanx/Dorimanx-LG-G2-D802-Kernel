/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_lna_control.c                                   */
/*    Description : sample source for lna control                           */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include "tcc353x_lna_control.h"
#include "tcpal_os.h"
#include "tcc353x_user_defines.h"
#include "tcc353x_api.h"
#include "tcc353x_monitoring.h"

#if defined (_USE_LNA_CONTROL_)

I32S Lna_Gain_Status[4][4] = {
	{
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL,
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL
	},
	{
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL,
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL
	},
	{
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL,
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL
	},
	{
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL,
	ENUM_NO_LNA_GAIN_CONTROL,ENUM_NO_LNA_GAIN_CONTROL
	}
};

I32U LNA_Gain2LowCmdCount[4][4] = {
	{0,0,0,0},	{0,0,0,0},	{0,0,0,0},	{0,0,0,0}
};

I32U LNA_Gain2HighCmdCount[4][4] = {
	{0,0,0,0},	{0,0,0,0},	{0,0,0,0},	{0,0,0,0}
};

I32S LnaHigh2LowThreshold = DEF_LNA_GAIN_HIGH_2_LOW_THR;
I32S LnaLow2HighThreshold = DEF_LNA_GAIN_LOW_2_HIGH_THR;
I32U LnaControlThrCount = DEF_LNA_CONTROL_COUNT_THR;

static I32S Tcc353xApiLnaControlSub(I32S _moduleIndex, I32S _diversityIndex, 
			  I32U _gainMode)
{
#define GPIO_LNA_PON			11
#define GPIO_MMBI_ELNA_EN		8

	I32S ret = TCC353X_RETURN_SUCCESS;

	if(_gainMode==ENUM_LNA_GAIN_HIGH) {
		/* lna control - high gain */
		/* high gain : PON 1, EN 0   low gain : PON 1, EN 1 */
		ret = Tcc353xApiSetGpioControl(_moduleIndex, _diversityIndex, 
					 GPIO_LNA_PON, 1);
		if(ret!=TCC353X_RETURN_SUCCESS)
			return ret;
		ret = Tcc353xApiSetGpioControl(_moduleIndex, _diversityIndex, 
					 GPIO_MMBI_ELNA_EN, 0);
		if(ret!=TCC353X_RETURN_SUCCESS)
			return ret;
		TcpalPrintStatus ((I08S *)"[TCC353X] LNA HighGain Controled\n");
	} else {
		/* lna control - low gain */
		/* high gain : PON 1, EN 0   low gain : PON 1, EN 1 */
		ret = Tcc353xApiSetGpioControl(_moduleIndex, _diversityIndex, 
					 GPIO_LNA_PON, 1);
		if(ret!=TCC353X_RETURN_SUCCESS)
			return ret;
		ret = Tcc353xApiSetGpioControl(_moduleIndex, _diversityIndex, 
					 GPIO_MMBI_ELNA_EN, 1);
		if(ret!=TCC353X_RETURN_SUCCESS)
			return ret;
		TcpalPrintStatus ((I08S *)"[TCC353X] LNA LowGain Controled\n");
	}
	Lna_Gain_Status[_moduleIndex][_diversityIndex] = _gainMode;

	return ret;
}

void Tcc353xApiLnaControlInit(I32S _moduleIndex, I32S _diversityIndex, 
			      I32S _initStatus)
{
	I32S ret;
	ret = Tcc353xApiLnaControlSub(_moduleIndex, _diversityIndex, 
				_initStatus);

	Lna_Gain_Status[_moduleIndex][_diversityIndex] = _initStatus;
	LNA_Gain2HighCmdCount[_moduleIndex][_diversityIndex] = 0;
	LNA_Gain2LowCmdCount[_moduleIndex][_diversityIndex] = 0;
}


I32S Tcc353xApiLnaControl(I32S _moduleIndex, I32S _diversityIndex, Tcc353xStatus_t *_st)
{
	I32S currRSSI;
	I32S gainValue = 0;
	I32S ret = TCC353X_RETURN_SUCCESS;

	gainValue = Lna_Gain_Status[_moduleIndex][_diversityIndex];

	currRSSI = _st->status.rssi.currentValue;
	currRSSI = (currRSSI/100);

	if(gainValue==ENUM_LNA_GAIN_HIGH && 
	   currRSSI>LnaHigh2LowThreshold) {
		LNA_Gain2LowCmdCount[_moduleIndex][_diversityIndex]++;
		LNA_Gain2HighCmdCount[_moduleIndex][_diversityIndex] = 0;
		TcpalPrintStatus((I08S *)"[TCC353X] RSSI [%d] LowCmd[%d]\n",
				 currRSSI, 
				 LNA_Gain2LowCmdCount[_moduleIndex]
				 [_diversityIndex]);
	} else if(gainValue==ENUM_LNA_GAIN_LOW && 
		  currRSSI<LnaLow2HighThreshold) {
		LNA_Gain2HighCmdCount[_moduleIndex][_diversityIndex]++;
		LNA_Gain2LowCmdCount[_moduleIndex][_diversityIndex] = 0;
		TcpalPrintStatus((I08S *)"[TCC353X] RSSI [%d] HighCmd[%d]\n",
				 currRSSI, 
				 LNA_Gain2HighCmdCount[_moduleIndex]
				 [_diversityIndex]);
	} else {
		TcpalPrintStatus((I08S *)"[TCC353X] RSSI [%d]\n",currRSSI);
		LNA_Gain2LowCmdCount[_moduleIndex][_diversityIndex] = 0;
		LNA_Gain2HighCmdCount[_moduleIndex][_diversityIndex] = 0;
	}

	if(LNA_Gain2LowCmdCount[_moduleIndex][_diversityIndex]>=
	   LnaControlThrCount) {
		LNA_Gain2LowCmdCount[_moduleIndex][_diversityIndex] = 0;
		Tcc353xApiLnaControlSub(_moduleIndex,_diversityIndex,
					ENUM_LNA_GAIN_LOW);
	} else if(LNA_Gain2HighCmdCount[_moduleIndex][_diversityIndex]>=
	 	LnaControlThrCount) {
		LNA_Gain2HighCmdCount[_moduleIndex][_diversityIndex] = 0;
		Tcc353xApiLnaControlSub(_moduleIndex,_diversityIndex,
					ENUM_LNA_GAIN_HIGH);
	}

	TcpalPrintStatus((I08S *)"[TCC353X] Current LNAGain[%d]\n",
			 Lna_Gain_Status[_moduleIndex][_diversityIndex]);
	return ret;
}
#endif
