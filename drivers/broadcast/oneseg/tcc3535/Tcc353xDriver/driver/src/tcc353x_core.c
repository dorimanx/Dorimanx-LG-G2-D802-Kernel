/****************************************************************************
 *   FileName    : tcc353x_core.c
 *   Description : core Function
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

#include "tcpal_os.h"
#include "tcpal_i2c.h"
#include "tcpal_spi.h"
#include "tcc353x_mailbox.h"
#include "tcc353x_core.h"
#include "tcc353x_register_control.h"
#include "tcc353x_rf.h"
#include "tcc353x_isdb.h"
#include "tcc353x_command_control.h"
#include "tcc353x_dpll_38400osc.h"
#include "tcc353x_dpll_19200osc.h"

/* static functions */
static I32S Tcc353xColdbootParserUtil(I08U * pData, I32U size,
				      Tcc353xBoot_t * pBOOTBin);
static void Tcc353xSetGpio(Tcc353xHandle_t * _handle);
static I32S Tcc353xSetPll(I32S _moduleIndex, I32S _deviceIndex, 
			  I32S _flagInterfaceLock, I16U _pllValue);
static I32S Tcc353xChangePll(I32S _moduleIndex, I16U _pllValue);
static void Tcc353xSaveOption(I32S _moduleIndex, I32S _diversityIndex,
			      Tcc353xOption_t * _Tcc353xOption);
static void Tcc353xConnectCommandInterface(I32S _moduleIndex, 
					   I32S _diversityIndex,
					   I08S _commandInterface);
static void Tcc353xSetInterruptControl(Tcc353xHandle_t * _handle);
static void Tcc353xOutBufferConfig(Tcc353xHandle_t * _handle);
static void Tcc353xSetStreamControl(Tcc353xHandle_t * _handle);
static I32S Tcc353xCodeDownload(I32S _moduleIndex, I08U * _coldbootData,
				I32S _codeSize);
static I32S Tcc353xCodeCrcCheck(I32S _moduleIndex, I08U * _coldbootData,
				Tcc353xBoot_t * _boot);
static void Tcc353xGetOpconfigValues(I32S _moduleIndex,
				     I32S _diversityIndex,
				     Tcc353xTuneOptions * _tuneOption,
				     I32U * _opConfig,
				     I32U _frequencyInfo);
static I32U Tcc353xDspRestart(I32S _moduleIndex, I32S _diversityIndex);
static I32S Tcc353xInitBroadcasting(I32S _moduleIndex,
				    I08U * _coldbootData, I32S _codeSize);
static I32S Tcc353xAttach(I32S _moduleIndex,
			  Tcc353xOption_t * _Tcc353xOption);
extern I64U Tcc353xDiv64(I64U x, I64U y);
extern I32S Tcc353xRfInit(I32S _moduleIndex, I32S _diversityIndex);
static I32S Tcc353xStreamStartPrepare(I32S _moduleIndex);
static I32S Tcc353xStreamOn (I32S _moduleIndex);
static I32S Tcc353xDspEpReopenForStreamStart(I32S _moduleIndex, 
					     I32S _diversityIndex);
static I32S Tcc353xSetOpConfig(I32S _moduleIndex, I32S _diversityIndex,
			I32U * op_cfg, I32U _firstFlag);
static I32U Tcc353xSearchDpllValue (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I32U _defaultPll);
static I32U Tcc353xSearchDpllTable (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I64U *_rcStep, 
				    I32U *_adcClkCfg, I32U _defaultPll);
static I32S Tcc353xSendStoppingCommand(I32S _moduleIndex);

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
#define	BITSET(X, MASK)				( (X) |= (I32U)(MASK) )
#endif
#ifndef BITCLR
#define	BITCLR(X, MASK)				( (X) &= ~((I32U)(MASK)) )
#endif

#define SCALE       22
#define FIXED(x)    (x<<SCALE)
#define MUL(A,B)    ((A*B)>>SCALE)
#define DIV(A,B)    (Tcc353xDiv64((A<<SCALE), B))

/*--------------------------------------------------------------------------*/
/*                                                                          */
/* Driver core version 8bit.8bit.8bit                                       */

I32U Tcc353xCoreVersion = ((0<<16) | (1<<8) | (60));

/*                                                                          */
/*--------------------------------------------------------------------------*/

Tcc353xHandle_t Tcc353xHandle[TCC353X_MAX][TCC353X_DIVERSITY_MAX];

TcpalSemaphore_t Tcc353xInterfaceSema;
I32U *pTcc353xInterfaceSema = NULL;

I32U Tcc353xCurrentDiversityCount[TCC353X_MAX];

TcpalSemaphore_t Tcc353xMailboxSema[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
TcpalSemaphore_t Tcc353xOpMailboxSema[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
Tcc353xRegisterConfig_t
    Tcc353xRegisterOptions[TCC353X_MAX][TCC353X_DIVERSITY_MAX];

I32U OriginalOpConfig[TCC353X_MAX][TCC353X_DIVERSITY_MAX][16];
Tcc353xTuneOptions MasterTuneOption;
I32U MasterInputfrequency = 0;

I08S *MailSemaphoreName[4][4] = { 
	{"MailboxSemaphore00", "MailboxSemaphore01",
	"MailboxSemaphore02", "MailboxSemaphore03"},
	{"MailboxSemaphore10", "MailboxSemaphore11",
	"MailboxSemaphore12", "MailboxSemaphore13"},
	{"MailboxSemaphore20", "MailboxSemaphore21",
	"MailboxSemaphore22", "MailboxSemaphore23"},
	{"MailboxSemaphore30", "MailboxSemaphore31",
	"MailboxSemaphore32", "MailboxSemaphore33"}
};
I08S *OPMailboxSemaphoreName[4][4] = { 
	{"OpMailboxSemaphore00", "OpMailboxSemaphore01",
	"OpMailboxSemaphore02", "OpMailboxSemaphore03"},
	{"OpMailboxSemaphore10", "OpMailboxSemaphore11",
	"OpMailboxSemaphore12", "OpMailboxSemaphore13"},
	{"OpMailboxSemaphore20", "OpMailboxSemaphore21",
	"OpMailboxSemaphore22", "OpMailboxSemaphore23"},
	{"OpMailboxSemaphore30", "OpMailboxSemaphore31",
	"OpMailboxSemaphore32", "OpMailboxSemaphore33"}
};

extern I32U DpllTable_TSB_1SEG[];
extern I32U DpllTable_TSB_3SEG[];
extern I32U DpllTable_TMM_1SEG[];
extern I32U DpllTable_TMM_13SEG[];
extern I32U DpllTable_TMM_USER_1SEG[];
extern I32U DpllTable_TMM_USER_13SEG[];
extern I32U DpllTable_Partial1Seg[];
extern I32U DpllTable_FullSeg[];

extern I32U DpllTable_TSB_1SEG_tcc3535[];
extern I32U DpllTable_TSB_3SEG_tcc3535[];
extern I32U DpllTable_TMM_1SEG_tcc3535[];
extern I32U DpllTable_TMM_13SEG_tcc3535[];
extern I32U DpllTable_TMM_USER_1SEG_tcc3535[];
extern I32U DpllTable_TMM_USER_13SEG_tcc3535[];
extern I32U DpllTable_Partial1Seg_tcc3535[];
extern I32U DpllTable_FullSeg_tcc3535[];

extern I32U OSC_19200_DpllTable_TMM_1SEG[];
extern I32U OSC_19200_DpllTable_TMM_13SEG[];
extern I32U OSC_19200_DpllTable_TMM_USER_1SEG[];
extern I32U OSC_19200_DpllTable_TMM_USER_13SEG[];
extern I32U OSC_19200_DpllTable_Partial1Seg[];
extern I32U OSC_19200_DpllTable_FullSeg[];

I32S Tcc353xOpen(I32S _moduleIndex, Tcc353xOption_t * _Tcc353xOption)
{
	return (Tcc353xAttach(_moduleIndex, _Tcc353xOption));
}

I32S Tcc353xClose(I32S _moduleIndex)
{
	I32U i;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	/* interrupt clr and disable */
	if (Tcc353xHandle[_moduleIndex][0].options.useInterrupt) {
		Tcc353xSetRegIrqEnable(&Tcc353xHandle[_moduleIndex][0], 0);
		Tcc353xIrqClear(_moduleIndex, TC3XREG_IRQ_STATCLR_ALL);
	}

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xPeripheralOnOff(&Tcc353xHandle[_moduleIndex][i], 0);
	}
	return (Tcc353xDetach(_moduleIndex));
}

I32S Tcc353xInit(I32S _moduleIndex, I08U * _coldbootData, I32S _codeSize)
{
	I32U i = 0;
	I32S ret = TCC353X_RETURN_SUCCESS;
	mailbox_t MailBox;
	I08U progId;
	I08U version0, version1, version2;
	I08U year, month, date;
	I32U temp;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;


	/* asm download and addressing */
	ret =
	    Tcc353xInitBroadcasting(_moduleIndex, _coldbootData,
				    _codeSize);
	if (ret != TCC353X_RETURN_SUCCESS)
		return ret;

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		I08U xtalbias_value = 7;

		/* stream / int / gpio / etc settings */
		Tcc353xSetStreamControl(&Tcc353xHandle[_moduleIndex][i]);
		Tcc353xSetInterruptControl(&Tcc353xHandle[_moduleIndex]
					   [i]);
		Tcc353xSetGpio(&Tcc353xHandle[_moduleIndex][i]);

		/* Restart System for stablility */
		TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i], 
				       TC3XREG_SYS_EN_OPCLK);
		Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex][i],
				      TC3XREG_SYS_RESET_DSP, _LOCK_);
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i],
				       TC3XREG_SYS_EN_EP |
				       TC3XREG_SYS_EN_DSP |
				       TC3XREG_SYS_EN_OPCLK |
				       TC3XREG_SYS_EN_RF);
		Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][i]);
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

		/* display code binary version */
		/* Get ASM Version */
		Tcc353xMailboxTxRx(&Tcc353xHandle[_moduleIndex][i],
				   &MailBox, MBPARA_SYS_ASM_VER, NULL, 0);

		/* option - change ldo volatage 1.2 to 1.8v */
		/*
		Tcc353xSetRegLdoConfig (&Tcc353xHandle[_moduleIndex][i],
				      0x1C);
		*/

		/* write values to sdram */
		/* sdram controller config */
		if(i==0) {
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 9, 0x56);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 5, 0x8000);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 6, 0x10000f);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0, 0x47482400);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 3, 0xf0);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 2, 0x72);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04,0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x20);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x460);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 0x04, 0x1FFFF);
			Tcc353xMiscWrite(_moduleIndex, i, 
					 MISC_SDRAM_REG_CTRL, 13, 0x80000E00);
		}

		/* Xtal Bias Key Setup */
		/*
		if (Tcc353xCurrentDiversityCount[_moduleIndex] == 1)
			xtalbias_value = 0;
		else
			xtalbias_value = 1;
		*/

		Tcc353xSetRegXtalBias(&Tcc353xHandle[_moduleIndex][i],
				      xtalbias_value);
		Tcc353xSetRegXtalBiasKey(&Tcc353xHandle[_moduleIndex][i],
					 0x5e);

		/* get program id, code version */
		Tcc353xGetRegProgramId(&Tcc353xHandle[_moduleIndex][0],
				       &progId);
		Tcc353xHandle[_moduleIndex][i].dspCodeVersion =
		    MailBox.data_array[0];
		temp = MailBox.data_array[0];

		TcpalPrintLog((I08S *)
			      "[TCC353X] ----------------------\n");
		TcpalPrintLog((I08S *)
			"[TCC353X] Code VersionForm(%d) : 0x%08X\n", i,
			temp);
		version0 = (I08U)((temp>>28) & 0x0F);
		version1 = (I08U)((temp>>24) & 0x0F);
		version2 = (I08U)((temp>>16) & 0xFF);
		year = (I08U)((temp>>9) & 0x3F);
		month = (I08U)((temp>>5) & 0x0F);
		date = (I08U)(temp & 0x1F);

		TcpalPrintLog((I08S *)
			      "[TCC353X] Version (%d.%d.%d)\n", version0, 
			      version1, version2);
		TcpalPrintLog((I08S *)
			      "[TCC353X] Date (%d.%d.%d)\n", year, 
			      month, date);

		version0 = (I08U)((Tcc353xCoreVersion>>16)&0xFF);
		version1 = (I08U)((Tcc353xCoreVersion>>8)&0xFF);
		version2 = (I08U)((Tcc353xCoreVersion)&0xFF);

		TcpalPrintLog((I08S *)
			      "[TCC353X] Device Driver Version (%d.%d.%d)\n", 
			      version0, version1, version2);
		TcpalPrintLog((I08S *)
			      "[TCC353X] ----------------------\n");
	}

	/* rf init */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		ret = Tcc353xRfInit(_moduleIndex, i);
		if(ret!=TCC353X_RETURN_SUCCESS)
			return ret;
	}

	return TCC353X_RETURN_SUCCESS;
}

#if 0
static void Tcc353xSetChangedGpioValue (I32S _moduleIndex, 
				        I32S _diversityIndex,
					Tcc353xRegisterConfig_t *_old, 
					Tcc353xRegisterConfig_t *_curr)
{
	I32S currentMuxConfig = -1;

	/* mux 0 */
	if (_old->gpioAlt_0x10_07_00 != _curr->gpioAlt_0x10_07_00) {
		if (currentMuxConfig != 0)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x00);
		currentMuxConfig = 0;
		Tcc353xSetRegGpioAlt(&Tcc353xHandle
				[_moduleIndex][_diversityIndex],
				_curr->gpioAlt_0x10_07_00);
	}

	if (_old->gpioDr_0x11_07_00 != _curr->gpioDr_0x11_07_00) {
		if (currentMuxConfig != 0)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x00);
		currentMuxConfig = 0;
		Tcc353xSetRegGpioDR(&Tcc353xHandle[_moduleIndex][_diversityIndex],
				    _curr->gpioDr_0x11_07_00);
	}
	
	if (_old->gpioDrv_0x13_07_00 != _curr->gpioDrv_0x13_07_00) {
		if (currentMuxConfig != 0)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x00);
		currentMuxConfig = 0;
		Tcc353xSetRegGpioDRV (&Tcc353xHandle[_moduleIndex][_diversityIndex],
				    _curr->gpioDrv_0x13_07_00);
	}

	/* mux 1 */
	if (_old->gpioAlt_0x10_15_08 != _curr->gpioAlt_0x10_15_08) {
		if (currentMuxConfig != 1)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x01);
		currentMuxConfig = 1;
		Tcc353xSetRegGpioAlt(&Tcc353xHandle[_moduleIndex][_diversityIndex],
				     _curr->gpioAlt_0x10_15_08);
	}

	if (_old->gpioDr_0x11_15_08 != _curr->gpioDr_0x11_15_08) {
		if (currentMuxConfig != 1)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x01);
		currentMuxConfig = 1;
		Tcc353xSetRegGpioDR(&Tcc353xHandle[_moduleIndex][_diversityIndex],
				    _curr->gpioDr_0x11_15_08);
	}

	if (_old->gpioDrv_0x13_15_08 != _curr->gpioDrv_0x13_15_08) {
		if (currentMuxConfig != 1)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x01);
		currentMuxConfig = 1;
		Tcc353xSetRegGpioDRV (&Tcc353xHandle[_moduleIndex][_diversityIndex],
				    _curr->gpioDrv_0x13_15_08);
	}

	/* mux 2 */
	if (_old->gpioAlt_0x10_23_16 != _curr->gpioAlt_0x10_23_16) {
		if (currentMuxConfig != 2)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x02);
		currentMuxConfig = 2;
		Tcc353xSetRegGpioAlt(&Tcc353xHandle[_moduleIndex][_diversityIndex],
				     _curr->gpioAlt_0x10_23_16);
	}

	if (_old->gpioDr_0x11_23_16 != _curr->gpioDr_0x11_23_16) {
		if (currentMuxConfig != 2)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x02);
		currentMuxConfig = 2;
		Tcc353xSetRegGpioDR(&Tcc353xHandle[_moduleIndex][_diversityIndex],
				    _curr->gpioDr_0x11_23_16);
	}

	if (_old->gpioDrv_0x13_23_16 != _curr->gpioDrv_0x13_23_16) {
		if (currentMuxConfig != 2)
			Tcc353xSetRegIoCfgMux(&Tcc353xHandle
				[_moduleIndex][_diversityIndex], 0x02);
		currentMuxConfig = 2;
		Tcc353xSetRegGpioDRV (&Tcc353xHandle[_moduleIndex][_diversityIndex],
				    _curr->gpioDrv_0x13_23_16);
	}
}
#endif

I32S Tcc353xChangeToDiversityMode (I32S _mergeIndex, 
				   Tcc353xOption_t * _Tcc353xOption)
{
#if 1
	return TCC353X_RETURN_FAIL;
#else
	I32S ret = TCC353X_RETURN_SUCCESS;
	I08U chipId;
	Tcc353xRegisterConfig_t oldConfig[2];
	Tcc353xRegisterConfig_t currConfig[2];
	I08U data[4];
	I32U tempDivCnt = 0;
	I32U i;
	
	TcpalPrintLog((I08S *)
		      "[TCC353X] Changing to 2-Diversity Mode! \n");

	/* ---------------------------------------------------	*/
	/* close dual driver !!! 				*/
	/*							*/

	Tcc353xPeripheralOnOff(&Tcc353xHandle[1][0], 0);

	/* link Dummy Function */
	Tcc353xHandle[1][0].Read = DummyFunction0;
	Tcc353xHandle[1][0].Write = DummyFunction1;

	/* Dealloc handles */
	TcpalMemset(&Tcc353xHandle[1][0], 0, sizeof(Tcc353xHandle_t));

	/* interface semaphore only one semaphore */
	/* delete all drivers */
	/* mailbox semaphore */
	TcpalDeleteSemaphore(&Tcc353xMailboxSema[1][0]);
	/* op & mailbox semaphore */
	TcpalDeleteSemaphore(&Tcc353xOpMailboxSema[1][0]);

	/* ---------------------------------------------------	*/
	/* open diversity driver !!! 				*/
	/*							*/

	tempDivCnt = Tcc353xCurrentDiversityCount[1];
	Tcc353xCurrentDiversityCount[0] += Tcc353xCurrentDiversityCount[1];
	Tcc353xCurrentDiversityCount[1] = 0;

	TcpalMemcpy(&oldConfig[0], 
		    &Tcc353xRegisterOptions[0][0],
		    sizeof(Tcc353xRegisterConfig_t));
	
	TcpalMemcpy(&oldConfig[1], 
		    &Tcc353xRegisterOptions[1][0],
		    sizeof(Tcc353xRegisterConfig_t));

	Tcc353xSaveOption(0, 0, &_Tcc353xOption[0]);
	Tcc353xSaveOption(0, 1, &_Tcc353xOption[1]);

	TcpalMemcpy(&currConfig[0], 
		    &Tcc353xRegisterOptions[0][0],
		    sizeof(Tcc353xRegisterConfig_t));

	TcpalMemcpy(&currConfig[1], 
		    &Tcc353xRegisterOptions[0][1],
		    sizeof(Tcc353xRegisterConfig_t));

	Tcc353xHandle[0][1].handleOpen = 1;
	Tcc353xConnectCommandInterface (0, 1, 
		Tcc353xHandle[0][0].
		options.commandInterface);

	TcpalCreateSemaphore(&Tcc353xMailboxSema[0][1],
			     MailSemaphoreName[0][1], 1);
	/* op & mailbox semaphore */
	TcpalCreateSemaphore(&Tcc353xOpMailboxSema[0]
			     [1], OPMailboxSemaphoreName[0][1],
			     1);
	Tcc353xGetRegChipId(&Tcc353xHandle[0][1],
			    &chipId);
	TcpalPrintLog((I08S *) "[TCC353X][0][1] ChipID 0x%02x\n", chipId);

	if (chipId != 0x33)
		ret = TCC353X_RETURN_FAIL;

	/* change BB#0 single to Diversity Master - Set gpio alt(13~21), io misc */
	Tcc353xSetChangedGpioValue (0, 0, &oldConfig[0], &currConfig[0]);
	Tcc353xSetRegIoMISC(&Tcc353xHandle[0][0], currConfig[0].ioMisc_0x16);

	/* change BB#1 single to Diversity Slave - Set gpio alt, io misc */
	/* 				buff config, gpio drv, peripheral */
	Tcc353xSetChangedGpioValue (0, 1, &oldConfig[1], &currConfig[1]);
	Tcc353xSetRegIoMISC(&Tcc353xHandle[0][1], currConfig[1].ioMisc_0x16);
	Tcc353xOutBufferConfig(&Tcc353xHandle[0][1]);
	
	data[0] = currConfig[1].periConfig_0x30;
	data[1] = currConfig[1].periConfig_0x31;
	data[2] = currConfig[1].periConfig_0x32;
	data[3] = currConfig[1].periConfig_0x33;
	Tcc353xSetRegPeripheralConfig(&Tcc353xHandle[0][1], &data[0]);

	/* tune same as master */
	for (i = 0; i < tempDivCnt; i++) {
		I32U opConfig[TCC353X_DIVERSITY_MAX][16];
		Tcc353xRfSwitching(0, i, MasterInputfrequency,
				   &Tcc353xHandle[0][i].options);

		Tcc353xRfTune(0, i, MasterInputfrequency, 6000,
			      Tcc353xHandle[0][i].options.oscKhz, 
			      &MasterTuneOption);

		Tcc353xGetOpconfigValues(0, i, &MasterTuneOption,
					 &opConfig[i][0], 
					 (I32U)(MasterInputfrequency));

		/* op configure it need dsp disable->reset->enable */
		Tcc353xSetOpConfig(0, i,
				   &opConfig[i][0], 1);
	}

	/* dsp disable to enable, ep reset & peripheral enable */
	for (i = 0; i < tempDivCnt; i++) {
		Tcc353xDspEpReopenForStreamStart(0, i);
		Tcc353xSendStartMail(&Tcc353xHandle[0][i]);
	}

	return ret;
#endif
}

I32S Tcc353xChangeToDualMode (I32S _devideIndex, 
			      Tcc353xOption_t * _Tcc353xOption)
{
#if 1
	return TCC353X_RETURN_FAIL;
#else
	I32S ret = TCC353X_RETURN_SUCCESS;
	I32S numberOfDemodule;
	I32S rest;
	I32S i;
	I08U chipId;
	Tcc353xRegisterConfig_t oldConfig[4];
	Tcc353xRegisterConfig_t currConfig[4];
	I08U data[4];

	numberOfDemodule = Tcc353xCurrentDiversityCount[0];
	rest = numberOfDemodule - _devideIndex;
	TcpalPrintLog((I08S *)
		      "[TCC353X] Changing to Dual Mode! \n");

	/* ---------------------------------------------------	*/
	/* close diversity driver !!!				*/
	/*							*/

	/* link Dummy Function */
	for(i=0; i<rest; i++)	{
		Tcc353xHandle[0][_devideIndex+i].Read = DummyFunction0;
		Tcc353xHandle[0][_devideIndex+i].Write = DummyFunction1;
	}

	/* Dealloc handles */
	TcpalMemset(&Tcc353xHandle[0][_devideIndex], 0, 
		    sizeof(Tcc353xHandle_t)*rest);

	/* interface semaphore only one semaphore */
	/* delete all drivers */
	/* mailbox semaphore */
	for(i=0; i<rest; i++)	{
		TcpalDeleteSemaphore(&Tcc353xMailboxSema[0][_devideIndex+i]);
		/* op & mailbox semaphore */
		TcpalDeleteSemaphore(&Tcc353xOpMailboxSema[0][_devideIndex+i]);
	}

	/* ---------------------------------------------------	*/
	/* open Dual driver !!!				*/
	/*							*/

	for(i=0; i<numberOfDemodule; i++)	{
		if(i<_devideIndex)
			Tcc353xCurrentDiversityCount[i] = _devideIndex;
		else
			Tcc353xCurrentDiversityCount[i] = rest;
	}

	for(i=0; i<numberOfDemodule; i++)
		TcpalMemcpy(&oldConfig[i], 
			    &Tcc353xRegisterOptions[0][i],
			    sizeof(Tcc353xRegisterConfig_t));

	for(i=0; i<numberOfDemodule; i++)	{
		if(i<_devideIndex)
			Tcc353xSaveOption(0, 0, &_Tcc353xOption[i]);
		else
			Tcc353xSaveOption(1, 0, &_Tcc353xOption[i]);
	}

	TcpalMemcpy(&currConfig[0], 
		    &Tcc353xRegisterOptions[0][0],
		    sizeof(Tcc353xRegisterConfig_t));

	TcpalMemcpy(&currConfig[1], 
		    &Tcc353xRegisterOptions[1][0],
		    sizeof(Tcc353xRegisterConfig_t));

	Tcc353xHandle[1][0].handleOpen = 1;
	Tcc353xConnectCommandInterface (1, 0, 
		Tcc353xHandle[1][0].
		options.commandInterface);

	TcpalCreateSemaphore(&Tcc353xMailboxSema[1][0],
			     MailSemaphoreName[1][0], 1);
	/* op & mailbox semaphore */
	TcpalCreateSemaphore(&Tcc353xOpMailboxSema[1][0], 
			     OPMailboxSemaphoreName[1][0],
			     1);
	Tcc353xGetRegChipId(&Tcc353xHandle[1][0],
			    &chipId);
	TcpalPrintLog((I08S *) "[TCC353X][BB 1] ChipID [0x%02x]\n", chipId);

	if (chipId != 0x33)
		ret = TCC353X_RETURN_FAIL;

	/* change BB#0 Diversity to Single - Set gpio alt(13~21), io misc */
	Tcc353xSetChangedGpioValue (0, 0, &oldConfig[0], &currConfig[0]);
	Tcc353xSetRegIoMISC(&Tcc353xHandle[0][0], currConfig[0].ioMisc_0x16);

	/* change BB#1 Diversity to Single - Set gpio alt, io misc */
	/*				buff config, gpio drv, peripheral */
	Tcc353xSetChangedGpioValue (1, 0, &oldConfig[1], &currConfig[1]);
	Tcc353xSetRegIoMISC(&Tcc353xHandle[1][0], currConfig[1].ioMisc_0x16);
	Tcc353xOutBufferConfig(&Tcc353xHandle[1][0]);
	
	data[0] = currConfig[1].periConfig_0x30;
	data[1] = currConfig[1].periConfig_0x31;
	data[2] = currConfig[1].periConfig_0x32;
	data[3] = currConfig[1].periConfig_0x33;
	Tcc353xSetRegPeripheralConfig(&Tcc353xHandle[1][0], &data[0]);
	Tcc353xHandle[1][0].useDefaultPLL = 1;

	return ret;
#endif
}

static I32S Tcc353xShiftCenterFrequency(I32S _moduleIndex, I32S _frequency,
					Tcc353xTuneOptions * _tuneOption)
{
	I32S inputFrequency = 0;
	I32S outputFrequency = 0;
	I32S frequencyOffset = 0;

	inputFrequency = _frequency;
	outputFrequency = inputFrequency;

	switch (_tuneOption->tmmSet) {
	case A_1st_1Seg:
		frequencyOffset = -6857;
		break;
	case A_2nd_1Seg:
		frequencyOffset = -6428;
		break;
	case A_3rd_1Seg:
		frequencyOffset = -6000;
		break;
	case A_4th_1Seg:
		frequencyOffset = -5571;
		break;
	case A_5th_1Seg:
		frequencyOffset = -5143;
		break;
	case A_6th_1Seg:
		frequencyOffset = -4714;
		break;
	case A_7th_1Seg:
		frequencyOffset = -4285;
		break;
	case A_1st_13Seg:
		frequencyOffset = -1285;
		break;
	case A_2nd_13Seg:
		frequencyOffset = 4286;
		break;

	case B_1st_13Seg:
		frequencyOffset = -4285;
		break;
	case B_1st_1Seg:
		frequencyOffset = -1285;
		break;
	case B_2nd_1Seg:
		frequencyOffset = -857;
		break;
	case B_3rd_1Seg:
		frequencyOffset = -428;
		break;
	case B_4th_1Seg:
		frequencyOffset = 0;
		break;
	case B_5th_1Seg:
		frequencyOffset = 429;
		break;
	case B_6th_1Seg:
		frequencyOffset = 857;
		break;
	case B_7th_1Seg:
		frequencyOffset = 1286;
		break;
	case B_2nd_13Seg:
		frequencyOffset = 4286;
		break;

	case C_1st_13Seg:
		frequencyOffset = -4285;
		break;
	case C_2nd_13Seg:
		frequencyOffset = 1286;
		break;
	case C_1st_1Seg:
		frequencyOffset = 4286;
		break;
	case C_2nd_1Seg:
		frequencyOffset = 4715;
		break;
	case C_3rd_1Seg:
		frequencyOffset = 5143;
		break;
	case C_4th_1Seg:
		frequencyOffset = 5572;
		break;
	case C_5th_1Seg:
		frequencyOffset = 6000;
		break;
	case C_6th_1Seg:
		frequencyOffset = 6429;
		break;
	case C_7th_1Seg:
		frequencyOffset = 6857;
		break;
	default:
		frequencyOffset = 0;
		break;
	}

	outputFrequency += frequencyOffset;
	return outputFrequency;
}

static I32U Tcc353xSearchDpllValue (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I32U _defaultPll)
{
	I32U i;
	I32U index;
	I32U pll;

	pll = _defaultPll;

	for(i = 0; i<_maxFreqNum; i++)	{
		index = (i*5);
		if(_tables[index] == 0) {
			/* last search, can't search frequency */
			pll = _tables[index+1];
			break;
		}
		if(_tables[index] == _frequencyInfo) {
			pll = _tables[index+1];
			break;
		}
	}

	return pll;
}

static I32S Tcc353xSendStoppingCommand(I32S _moduleIndex)
{
	I32U i;
	
	/* stop mail -> pause mail (for receiving ack) */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		I32U temp = 0;
		I16S j = 0;
		Tcc353xMiscRead(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, &temp);
		temp |= 0x100;
		Tcc353xMiscWrite(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, temp);

		for(j=0; j<300; j++)
		{
			I08U tmpdata = 0;
			Tcc353xGetRegProgramId(&Tcc353xHandle[_moduleIndex][i]
				, &tmpdata);
			if(tmpdata & 0x2)
				break;
			else
				TcpalmDelay(1);
		}
	}
	TcpalmDelay(2);	/* for stability */

	return TCC353X_RETURN_SUCCESS;
}

static I16U Tcc353xGetNewPLL_38400(I32S _moduleIndex, 
    Tcc353xTuneOptions * _tuneOption, I32S _Inputfrequency)
{
	I16U newPll = 0;

#if defined (_SUPPORT_OSC_38400_)
	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		if(_tuneOption->rfIfType==TCC353X_LOW_IF)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_Partial1Seg[0],
					_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					PLL_ISDB_T_PARTIAL_1_SEG));
		else
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_FullSeg[0],
					_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					PLL_ISDB_T_PARTIAL_1_SEG));
		break;
	case TCC353X_ISDBT_13SEG:
		newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_FullSeg[0],
					_MAX_FULLSEG_FREQ_NUM_, 
					PLL_ISDB_T_FULLSEG));
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_USER_13SEG[0],
					_MAX_TMM_USER_13SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_FULLSEG));
		else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_USER_1SEG[0],
					_MAX_TMM_USER_1SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_PARTIAL_1_SEG));
		else if (_tuneOption->tmmSet == A_1st_13Seg||
			 _tuneOption->tmmSet == A_2nd_13Seg||
			 _tuneOption->tmmSet == B_1st_13Seg||
			 _tuneOption->tmmSet == B_2nd_13Seg||
			 _tuneOption->tmmSet == C_1st_13Seg||
			 _tuneOption->tmmSet == C_2nd_13Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_13SEG[0],
					_MAX_TMM_13SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_FULLSEG));
		else
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_1SEG[0],
					_MAX_TMM_1SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_PARTIAL_1_SEG));
		break;
	case TCC353X_ISDBTSB_1SEG:
	case TCC353X_ISDBTSB_1_OF_3SEG:
		newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TSB_1SEG[0],
					_MAX_TSB_1SEG_FREQ_NUM_, 
					PLL_ISDB_TSB));
		break;
	case TCC353X_ISDBTSB_3SEG:
		newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TSB_3SEG[0],
					_MAX_TSB_3SEG_FREQ_NUM_, 
					PLL_ISDB_TSB));
		break;
	default:
		newPll = PLL_ISDB_T_FULLSEG;
		break;
	}
#endif
	return newPll;
}

static I16U Tcc353xGetNewPLL_19200(I32S _moduleIndex, 
    Tcc353xTuneOptions * _tuneOption, I32S _Inputfrequency)
{
	I16U newPll = 0;
	
#if defined (_SUPPORT_OSC_19200_)
	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		if(_tuneOption->rfIfType==TCC353X_LOW_IF)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&OSC_19200_DpllTable_Partial1Seg[0],
					_OSC_19200_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					OSC_192_PLL_ISDB_T_PARTIAL_1_SEG));
		else
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&OSC_19200_DpllTable_FullSeg[0],
					_OSC_19200_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					OSC_192_PLL_ISDB_T_PARTIAL_1_SEG));
		break;
	case TCC353X_ISDBT_13SEG:
		newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&OSC_19200_DpllTable_FullSeg[0],
					_OSC_19200_MAX_FULLSEG_FREQ_NUM_, 
					OSC_192_PLL_ISDB_T_FULLSEG));
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&OSC_19200_DpllTable_TMM_USER_13SEG[0],
					_OSC_19200_MAX_TMM_USER_13SEG_FREQ_NUM_, 
					OSC_192_PLL_ISDB_TMM_FULLSEG));
		else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&OSC_19200_DpllTable_TMM_USER_1SEG[0],
					_OSC_19200_MAX_TMM_USER_1SEG_FREQ_NUM_, 
					OSC_192_PLL_ISDB_TMM_PARTIAL_1_SEG));
		else if (_tuneOption->tmmSet == A_1st_13Seg||
			 _tuneOption->tmmSet == A_2nd_13Seg||
			 _tuneOption->tmmSet == B_1st_13Seg||
			 _tuneOption->tmmSet == B_2nd_13Seg||
			 _tuneOption->tmmSet == C_1st_13Seg||
			 _tuneOption->tmmSet == C_2nd_13Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&OSC_19200_DpllTable_TMM_13SEG[0],
					_OSC_19200_MAX_TMM_13SEG_FREQ_NUM_, 
					OSC_192_PLL_ISDB_TMM_FULLSEG));
		else
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&OSC_19200_DpllTable_TMM_1SEG[0],
					_OSC_19200_MAX_TMM_1SEG_FREQ_NUM_, 
					OSC_192_PLL_ISDB_TMM_PARTIAL_1_SEG));
		break;
	case TCC353X_ISDBTSB_1SEG:
		newPll = OSC_192_PLL_ISDB_TSB;
		break;
	case TCC353X_ISDBTSB_3SEG:
		newPll = OSC_192_PLL_ISDB_TSB;
		break;
	case TCC353X_ISDBTSB_1_OF_3SEG:
		newPll = OSC_192_PLL_ISDB_TSB;
		break;
	default:
		newPll = OSC_192_PLL_ISDB_T_FULLSEG;
		break;
	}
#endif
	return newPll;
}

static I16U Tcc353xGetNewPLL_tcc3535(I32S _moduleIndex, 
    Tcc353xTuneOptions * _tuneOption, I32S _Inputfrequency)
{
	I16U newPll = 0;

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		if(_tuneOption->rfIfType==TCC353X_LOW_IF)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_Partial1Seg_tcc3535[0],
					_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					PLL_ISDB_T_PARTIAL_1_SEG));
		else
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_FullSeg_tcc3535[0],
					_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					PLL_ISDB_T_PARTIAL_1_SEG));
		break;
	case TCC353X_ISDBT_13SEG:
		newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_FullSeg_tcc3535[0],
					_MAX_FULLSEG_FREQ_NUM_, 
					PLL_ISDB_T_FULLSEG));
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_USER_13SEG_tcc3535[0],
					_MAX_TMM_USER_13SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_FULLSEG));
		else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_USER_1SEG_tcc3535[0],
					_MAX_TMM_USER_1SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_PARTIAL_1_SEG));
		else if (_tuneOption->tmmSet == A_1st_13Seg||
			 _tuneOption->tmmSet == A_2nd_13Seg||
			 _tuneOption->tmmSet == B_1st_13Seg||
			 _tuneOption->tmmSet == B_2nd_13Seg||
			 _tuneOption->tmmSet == C_1st_13Seg||
			 _tuneOption->tmmSet == C_2nd_13Seg)
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_13SEG_tcc3535[0],
					_MAX_TMM_13SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_FULLSEG));
		else
			newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TMM_1SEG_tcc3535[0],
					_MAX_TMM_1SEG_FREQ_NUM_, 
					PLL_ISDB_TMM_PARTIAL_1_SEG));
		break;
	case TCC353X_ISDBTSB_1SEG:
	case TCC353X_ISDBTSB_1_OF_3SEG:
		newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TSB_1SEG_tcc3535[0],
					_MAX_TSB_1SEG_FREQ_NUM_, 
					PLL_ISDB_TSB));
		break;
	case TCC353X_ISDBTSB_3SEG:
		newPll = (I16U)(Tcc353xSearchDpllValue (
					(I32U)(_Inputfrequency),
					&DpllTable_TSB_3SEG_tcc3535[0],
					_MAX_TSB_3SEG_FREQ_NUM_, 
					PLL_ISDB_TSB));
		break;
	default:
		newPll = PLL_ISDB_T_FULLSEG;
		break;
	}

	return newPll;
}


I32S Tcc353xTune(I32S _moduleIndex, I32S _frequency,
		 Tcc353xTuneOptions * _tuneOption, I32S _fastTune)
{
	I32U i;
	I16U newPll = 0;
	I32U opConfig[TCC353X_DIVERSITY_MAX][16];
	I32S Inputfrequency = _frequency;
	I32U firstOpconfigWrite = 0;
	TcpalTime_t CurrTime;
	I32U TotalTime;
	I08U bufferEndReg[2];
	I32U bufferEndAddress;

	CurrTime = TcpalGetCurrentTimeCount_ms();

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	/* stopping old channel stream output */

	/* stop mail -> pause mail (for receiving ack) */
	Tcc353xSendStoppingCommand(_moduleIndex);

	if (Tcc353xHandle[_moduleIndex][0].streamStarted) {
		/* stream stop */
		Tcc353xStreamStop(_moduleIndex);
	}

	if(Tcc353xHandle[_moduleIndex][0].options.streamInterface== 
						TCC353X_STREAM_IO_MAINIO) {
		I08U fifothr[2];
		
		/* change buffer A end size */
		switch (_tuneOption->segmentType) {
		case TCC353X_ISDBT_1_OF_13SEG:
		case TCC353X_ISDBTSB_1SEG:
		case TCC353X_ISDBTSB_1_OF_3SEG:
			bufferEndAddress = 0x00019F5B;
			break;
		case TCC353X_ISDBT_13SEG:
		case TCC353X_ISDBTSB_3SEG:
			bufferEndAddress = 0x00027F57;
			break;
		case TCC353X_ISDBTMM:
			if (_tuneOption->tmmSet == A_1st_13Seg ||
			    _tuneOption->tmmSet == A_2nd_13Seg ||
			    _tuneOption->tmmSet == B_1st_13Seg ||
			    _tuneOption->tmmSet == B_2nd_13Seg ||
			    _tuneOption->tmmSet == C_1st_13Seg ||
			    _tuneOption->tmmSet == C_2nd_13Seg ||
			    _tuneOption->tmmSet == UserDefine_Tmm13Seg) {
				/*13seg */
				bufferEndAddress = 0x00027F57;
			} else {
				bufferEndAddress = 0x00019F5B;
			}
			break;
		default:
			bufferEndAddress = 0x00027F57;
			break;
		}
		
		if(_tuneOption->userFifothr) {
			fifothr[0] = (I08U)(((_tuneOption->userFifothr>>2)>>8) 
				     & 0xFF);
			fifothr[1] = (I08U)((_tuneOption->userFifothr>>2) 
				     & 0xFF);
			Tcc353xSetRegOutBufferAFifoThr
				(&Tcc353xHandle[_moduleIndex][0], &fifothr[0]);
		}

		bufferEndReg[0] = (I08U)((bufferEndAddress >> 10) & 0xFF),
		bufferEndReg[1] = (I08U)((bufferEndAddress >> 2) & 0xFF),
		Tcc353xSetRegOutBufferEndAddressA(
			&Tcc353xHandle[_moduleIndex][0], &bufferEndReg[0]);
	}

	TcpalMemcpy(&Tcc353xHandle[_moduleIndex][0].TuneOptions,
		    _tuneOption, sizeof(Tcc353xTuneOptions));

	/* center frequency shift for isdb-tmm */
	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
	case TCC353X_ISDBT_13SEG:
		/* ISDB-T */
		TcpalPrintLog((I08S *)
			      "[TCC353X] [BB %d] Tune frequency [ISDB-T]: %d\n",
			      _moduleIndex, _frequency);
		break;

	case TCC353X_ISDBTSB_1SEG:
	case TCC353X_ISDBTSB_3SEG:
	case TCC353X_ISDBTSB_1_OF_3SEG:
		/* ISDB-Tsb */
		TcpalPrintLog((I08S *)
			      "[TCC353X] [BB %d] Tune frequency [ISDB-Tsb]: %d\n",
			      _moduleIndex, _frequency);
		break;

	case TCC353X_ISDBTMM:
		/* ISDB-TMM */
		Inputfrequency =
		    Tcc353xShiftCenterFrequency(_moduleIndex, _frequency,
						_tuneOption);
		if (Inputfrequency <= 0)
			return TCC353X_RETURN_FAIL;

		TcpalPrintLog((I08S *)
 			      "[TCC353X] [BB %d] Tune frequency [ISDB-TMM] (TMM center frequency shift : %d to %d)\n",
			      _moduleIndex, _frequency, Inputfrequency);
		break;
	default:
		/* ISDB-T */
		TcpalPrintLog((I08S *)
			      "[TCC353X] [BB %d] Tune frequency [ISDB-T]: %d\n",
			      _moduleIndex, _frequency);
		break;
	}

	/* check pll change need or not */
	/* change full seg to partial 1seg or isdb-t <-> tmm */
	if(Tcc353xHandle[_moduleIndex][0].useDefaultPLL == 1)
	{
		if(Tcc353xHandle[_moduleIndex][0].options.basebandName 
		   == BB_TCC3535)
			newPll = Tcc353xGetNewPLL_tcc3535
				    (_moduleIndex, _tuneOption, Inputfrequency);
		else if(Tcc353xHandle[_moduleIndex][0].options.oscKhz == 38400)
			newPll = Tcc353xGetNewPLL_38400
				    (_moduleIndex, _tuneOption, Inputfrequency);
		else if (Tcc353xHandle[_moduleIndex][0].options.oscKhz == 19200)
			newPll = Tcc353xGetNewPLL_19200
				    (_moduleIndex, _tuneOption, Inputfrequency);
		else	/* default 38400 */
			newPll = Tcc353xGetNewPLL_38400
				    (_moduleIndex, _tuneOption, Inputfrequency);

		/* change pll */
		#ifdef TCC79X	/* for 79x muse */
		Tcc353xChangePll(_moduleIndex, newPll);
		#else
		if (newPll != Tcc353xHandle[_moduleIndex][0].options.pll)
			Tcc353xChangePll(_moduleIndex, newPll);
		#endif
	}

	if(!Tcc353xHandle[_moduleIndex][0].tuned)
		firstOpconfigWrite = 1;

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xRfSwitching(_moduleIndex, i, Inputfrequency,
				   &Tcc353xHandle[_moduleIndex]
				   [i].options);

		Tcc353xRfTune(_moduleIndex, i, Inputfrequency, 6000,
			      Tcc353xHandle[_moduleIndex][i].
			      options.oscKhz, _tuneOption);

		Tcc353xGetOpconfigValues(_moduleIndex, i, _tuneOption,
					 &opConfig[i][0], 
					 (I32U)(Inputfrequency));

		/* op configure it need dsp disable->reset->enable */
		Tcc353xSetOpConfig(_moduleIndex, i,
				   &opConfig[i][0], firstOpconfigWrite);
	}

	if(_moduleIndex==0) {
		TcpalMemcpy (&MasterTuneOption, _tuneOption, 
		    sizeof(Tcc353xTuneOptions));
		MasterInputfrequency = Inputfrequency;
	}

	Tcc353xHandle[_moduleIndex][0].tuned = 1;

	/* stream start & dsp reset & send start mail */
	Tcc353xInitIsdbProcess(&Tcc353xHandle[_moduleIndex][0]);
	Tcc353xStreamStartPrepare (_moduleIndex);

	/* dsp disable to enable, ep reset & peripheral enable */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xDspEpReopenForStreamStart(_moduleIndex, i);
		Tcc353xSendStartMail(&Tcc353xHandle[_moduleIndex][i]);
	}
	
	TotalTime = (I32U)((I64U)(TcpalGetTimeIntervalCount_ms(CurrTime)) 
				   & 0xFFFFFFFF);
	TcpalPrintStatus ((I08S *) "[TCC353X] SpendTime [%d]ms\n", TotalTime);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xSetStreamFormat(I32S _moduleIndex,
			    Tcc353xStreamFormat_t * _streamFormat)
{
	I32U data = 0;
	TcpalMemcpy(&Tcc353xHandle[_moduleIndex][0].streamFormat, 
		    _streamFormat, sizeof(Tcc353xStreamFormat_t));

	Tcc353xMiscRead(_moduleIndex, 0, MISC_OP_REG_CTRL,
			TC3XREG_OP_FILTER_CFG, &data);
	data = (data & 0xFFFFFC3F);

	if (_streamFormat->pidFilterEnable)
		BITSET(data, Bit6);
	else
		BITCLR(data, Bit6);

	if (_streamFormat->tsErrorFilterEnable)
		BITSET(data, Bit7);
	else
		BITCLR(data, Bit7);

	if (_streamFormat->syncByteFilterEnable)
		BITSET(data, Bit8);
	else
		BITCLR(data, Bit8);

	if (_streamFormat->tsErrorInsertEnable)
		BITSET(data, Bit9);
	else
		BITCLR(data, Bit9);

	Tcc353xMiscWrite(_moduleIndex, 0, MISC_OP_REG_CTRL,
			 TC3XREG_OP_FILTER_CFG, data);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc35xSelectLayer(I32S _moduleIndex, I32S _layer)
{
	return TCC353X_RETURN_SUCCESS;
}

I32U Tcc353xSendStartMail(Tcc353xHandle_t * _handle)
{
	/* start / stop mail (toggle) for dp stablility */
	mailbox_t mailbox;
	I32S retmail;

	/* any value */
	I32U data = 0x11;

	retmail =
	    Tcc353xMailboxTxRx(_handle, &mailbox, MBPARA_SYS_START, &data,
			       1);
	if (retmail == -1)
		TcpalPrintErr((I08S *)
			      "[TCC353X] Error - Sending Startmail\n");
	return retmail;
}

I32S Tcc353xStreamStopAll(I32S _moduleIndex)
{

	/* stopping stream */
	Tcc353xStreamStop(_moduleIndex);

	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xStreamStop(I32S _moduleIndex)
{
	I08U streamDataConfig_0x1E;
	I08U bufferConfig0;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	/* interrupt clr and disable */
	if (Tcc353xHandle[_moduleIndex][0].options.useInterrupt) {
		Tcc353xSetRegIrqEnable(&Tcc353xHandle[_moduleIndex][0], 0);
		Tcc353xIrqClear(_moduleIndex, TC3XREG_IRQ_STATCLR_ALL);
	}

	/* disable stream data config */
	streamDataConfig_0x1E =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->streamDataConfig_0x1E;
	BITCLR(streamDataConfig_0x1E, TC3XREG_STREAM_DATA_ENABLE);
	Tcc353xSetRegStreamConfig0(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->streamDataConfig_0x1B);
	Tcc353xSetRegStreamConfig3(&Tcc353xHandle[_moduleIndex][0],
				   streamDataConfig_0x1E);

	/* disable buffer */
	bufferConfig0 =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->bufferConfig_0x4E;
	BITCLR(bufferConfig0,
	       TC3XREG_OBUFF_CFG_BUFF_A_EN | TC3XREG_OBUFF_CFG_BUFF_B_EN |
	       TC3XREG_OBUFF_CFG_BUFF_C_EN | TC3XREG_OBUFF_CFG_BUFF_D_EN);
	Tcc353xSetRegOutBufferConfig(&Tcc353xHandle[_moduleIndex][0],
				     bufferConfig0);

	/* peripheral disable clr */
	Tcc353xPeripheralOnOff(&Tcc353xHandle[_moduleIndex][0], 0);

	Tcc353xHandle[_moduleIndex][0].streamStarted = 0;

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xStreamStartPrepare(I32S _moduleIndex)
{
	I08U streamDataConfig_0x1B;
	I08U streamDataConfig_0x1E;
	I08U bufferConfig0;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	Tcc353xHandle[_moduleIndex][0].streamStarted = 1;

	/* buffer init & enable buffer */
	Tcc353xSetRegOutBufferInit(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->bufferConfig_0x4F);

	bufferConfig0 =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->bufferConfig_0x4E;
	BITSET(bufferConfig0, TC3XREG_OBUFF_CFG_BUFF_A_EN);

	Tcc353xSetRegOutBufferConfig(&Tcc353xHandle[_moduleIndex][0],
				     bufferConfig0);

	streamDataConfig_0x1B =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->streamDataConfig_0x1B;
	streamDataConfig_0x1E =
	    Tcc353xHandle[_moduleIndex][0].options.
	    Config->streamDataConfig_0x1E;

	BITSET(streamDataConfig_0x1E, TC3XREG_STREAM_DATA_ENABLE);

	if (Tcc353xHandle[_moduleIndex][0].options.useInterrupt)
		BITSET(streamDataConfig_0x1E,
		       TC3XREG_STREAM_DATA_FIFO_INIT |
		       TC3XREG_STREAM_DATA_FIFO_EN);
	else
		BITSET(streamDataConfig_0x1E,
		       TC3XREG_STREAM_DATA_FIFO_INIT);

	Tcc353xSetRegStreamConfig0(&Tcc353xHandle[_moduleIndex][0],
				   streamDataConfig_0x1B);
	Tcc353xSetRegStreamConfig1(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->streamDataConfig_0x1C);
	Tcc353xSetRegStreamConfig2(&Tcc353xHandle[_moduleIndex][0],
				   Tcc353xHandle[_moduleIndex][0].
				   options.Config->streamDataConfig_0x1D);
	Tcc353xSetRegStreamConfig3(&Tcc353xHandle[_moduleIndex][0],
				   streamDataConfig_0x1E);

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xStreamOn (I32S _moduleIndex)
{
	Tcc353xOption_t *pOptions;
	pOptions = (Tcc353xOption_t *)(&Tcc353xHandle[_moduleIndex][0].options);

	/* peripheral enable clr */
	Tcc353xPeripheralOnOff(&Tcc353xHandle[_moduleIndex][0], 1);

	/* interrupt enable */
	if (pOptions->useInterrupt) {
		I08U irqValue = 0x00;
		if(pOptions->streamInterface == TCC353X_STREAM_IO_MAINIO)
			irqValue = TC3XREG_IRQ_EN_FIFOAINIT |
				   TC3XREG_IRQ_EN_FIFO_OVER;

		/* special case for Emergency alarm */
		/*
		irqValue += TC3XREG_IRQ_EN_OPINT;
		*/
	
		Tcc353xSetRegIrqEnable(&Tcc353xHandle[_moduleIndex][0], 
				       irqValue);
	}

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xDspEpReopenForStreamStart(I32S _moduleIndex, 
					     I32S _diversityIndex)
{
	TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex]
			   [_diversityIndex]);
	
	/* DSP disable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex]
			       [_diversityIndex],
			       TC3XREG_SYS_EN_EP |
			       TC3XREG_SYS_EN_OPCLK |
			       TC3XREG_SYS_EN_RF);
	/* DSP reset */
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex]
			      [_diversityIndex], TC3XREG_SYS_RESET_DSP, 
			      _LOCK_);
	TcpalmDelay(1);

	/* EP reset */
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex]
			      [_diversityIndex], TC3XREG_SYS_RESET_EP, 
			      _LOCK_);

	/* peripheral enable */
	if(_diversityIndex==0)
		Tcc353xStreamOn (_moduleIndex);

	/* DSP Enable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex]
			       [_diversityIndex],
			       TC3XREG_SYS_EN_EP |
			       TC3XREG_SYS_EN_DSP |
			       TC3XREG_SYS_EN_OPCLK |
			       TC3XREG_SYS_EN_RF);

	Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][_diversityIndex]);
	TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex]
			     [_diversityIndex]);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xStreamStart(I32S _moduleIndex)
{
	I32S i;
	/* stream start & dsp reset & send start mail */
	Tcc353xStreamStartPrepare (_moduleIndex);

	/* dsp disable to enable, ep reset & peripheral enable */
	for (i = 0; i < (I32S)Tcc353xCurrentDiversityCount[_moduleIndex]; 
	    i++) {
		Tcc353xDspEpReopenForStreamStart(_moduleIndex, i);
		Tcc353xSendStartMail(&Tcc353xHandle[_moduleIndex][i]);
	}
	
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xInterruptBuffClr(I32S _moduleIndex)
{
	I32U i;

	/* send stop command */
	Tcc353xSendStoppingCommand (_moduleIndex);

	/* stopping stream */
	Tcc353xStreamStop(_moduleIndex);

	/* send opconfig */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		I32U temp = 0;
		Tcc353xMiscRead(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, &temp);
		temp &= ~((I32U)(0x100));
		Tcc353xMiscWrite(_moduleIndex, i, MISC_OP_REG_CTRL, 
				TC3XREG_OP_CFG06, temp);
	}

	/* send start mail */
	Tcc353xStreamStart(_moduleIndex);
	return TCC353X_RETURN_SUCCESS;
}


static I32S Tcc353xAttach(I32S _moduleIndex,
			  Tcc353xOption_t * _Tcc353xOption)
{
	I32U i;
	I32S ret = TCC353X_RETURN_FAIL;
	I08U chipId = 0;
	I08U progId = 0;

	/* init global values */
	switch (_Tcc353xOption[0].boardType) {
	case TCC353X_BOARD_SINGLE:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 1;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [Single Mode]\n");
		break;

	case TCC353X_BOARD_2DIVERSITY:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 2;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [2Diversity Mode]\n");
		break;

	case TCC353X_BOARD_3DIVERSITY:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 3;
		TcpalPrintLog((I08S *)
 			      "[TCC353X] TCC353X Attach Success! [3Diversity Mode]\n");
		break;

	case TCC353X_BOARD_4DIVERSITY:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 4;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [4Diversity Mode]\n");
		break;

	default:
		Tcc353xCurrentDiversityCount[_moduleIndex] = 1;
		TcpalPrintLog((I08S *)
			      "[TCC353X] TCC353X Attach Success! [Single Mode]\n");
		break;
	}

	TcpalMemset(&Tcc353xHandle[_moduleIndex][0], 0,
		    sizeof(Tcc353xHandle_t) *
		    Tcc353xCurrentDiversityCount[_moduleIndex]);

	/* connect command interface function */
	/* set address */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xSaveOption(_moduleIndex, i, &_Tcc353xOption[i]);
		Tcc353xHandle[_moduleIndex][i].handleOpen = 1;
		Tcc353xConnectCommandInterface (_moduleIndex, i, 
			Tcc353xHandle[_moduleIndex][0].
			options.commandInterface);
	}

	/* interface semaphore only one semaphore */
	if (pTcc353xInterfaceSema == NULL) {
		TcpalCreateSemaphore(&Tcc353xInterfaceSema,
				     (I08S *) "InterfaceSemaphore", 1);
		pTcc353xInterfaceSema = &Tcc353xInterfaceSema;
	} else {
		TcpalPrintLog((I08S *)
			      "[TCC353X] - Already exist interface semaphore\n");
	}

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		/* mailbox semaphore */
		TcpalCreateSemaphore(&Tcc353xMailboxSema[_moduleIndex][i],
				     MailSemaphoreName[_moduleIndex][i], 1);
		/* op & mailbox semaphore */
		TcpalCreateSemaphore(&Tcc353xOpMailboxSema[_moduleIndex][i], 
				     OPMailboxSemaphoreName[_moduleIndex][i],
				     1);

		Tcc353xGetRegChipId(&Tcc353xHandle[_moduleIndex][i],
				    &chipId);
		TcpalPrintLog((I08S *) "[TCC353X][%d][%d] ChipID 0x%02x\n",
			      _moduleIndex, i, chipId);
		Tcc353xGetRegProgramId(&Tcc353xHandle[_moduleIndex][i],
				       &progId);
		TcpalPrintLog((I08S *) "[TCC353X][%d][%d] progId 0x%02x\n",
			      _moduleIndex, i, progId);
	}

	switch(Tcc353xHandle[_moduleIndex][0].options.basebandName) {
	case BB_TCC3530:
		TcpalPrintLog((I08S *) "[TCC353X] TCC3530 selected\n");
		break;
	case BB_TCC3531:
		TcpalPrintLog((I08S *) "[TCC353X] TCC3531 selected\n");
		break;
	case BB_TCC3532:
		TcpalPrintLog((I08S *) "[TCC353X] TCC3532 selected\n");
		break;
	case BB_TCC3535:
		TcpalPrintLog((I08S *) "[TCC353X] TCC3535 selected\n");
		break;
	default:
		TcpalPrintErr((I08S *) "[TCC353X] No baseband name selected\n");
		break;
	}

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xHandle_t *h;
		h = &Tcc353xHandle[_moduleIndex][i];

		if(h->options.rfType == TCC353X_TRIPLE_BAND_RF) {
		   	if(h->options.basebandName != BB_TCC3531 && 
		   	   h->options.basebandName != BB_TCC3535 ) {
		   	   	h->options.rfType = TCC353X_DUAL_BAND_RF;
				TcpalPrintLog((I08S *) "[TCC353X] [%dth] Change to Dual band\n",i);
			} else {
				TcpalPrintLog((I08S *) "[TCC353X] [%dth] Triple Band selected\n",i);
			}
		} else {
			if(h->options.basebandName == BB_TCC3535) {
		   	   	h->options.rfType = TCC353X_TRIPLE_BAND_RF;
				TcpalPrintLog((I08S *) "[TCC353X] [%dth] Change to Triple band\n",i);
			} else {
				TcpalPrintLog((I08S *) "[TCC353X] [%dth] Dual Band selected\n",i);
			}
		}
	}

	if (chipId == 0x33)
		ret = TCC353X_RETURN_SUCCESS;
	return ret;
}

I32S Tcc353xDetach(I32S _moduleIndex)
{
	I32U i;
	/* link Dummy Function */
	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xHandle[_moduleIndex][i].Read = DummyFunction0;
		Tcc353xHandle[_moduleIndex][i].Write = DummyFunction1;
	}

	/* Dealloc handles */
	TcpalMemset(&Tcc353xHandle[_moduleIndex][0], 0,
		    sizeof(Tcc353xHandle_t) *
		    Tcc353xCurrentDiversityCount[_moduleIndex]);

	/* for stability Link Dummy Function */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xHandle[_moduleIndex][i].Read = DummyFunction0;
		Tcc353xHandle[_moduleIndex][i].Write = DummyFunction1;
	}
	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);

	/* interface semaphore only one semaphore */
	/* delete all drivers */
	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		/* mailbox semaphore */
		TcpalDeleteSemaphore(&Tcc353xMailboxSema[_moduleIndex][i]);
		/* op & mailbox semaphore */
		TcpalDeleteSemaphore(&Tcc353xOpMailboxSema[_moduleIndex]
				     [i]);
	}

	if (Tcc353xHandle[0][0].handleOpen == 0
	    && Tcc353xHandle[1][0].handleOpen == 0) {
		TcpalDeleteSemaphore(&Tcc353xInterfaceSema);
		pTcc353xInterfaceSema = NULL;

		/* init global values */
		Tcc353xCurrentDiversityCount[_moduleIndex] = 1;
	}

	TcpalPrintLog((I08S *) "[TCC353X] Detach Success!\n");

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xCodeDownload(I32S _moduleIndex, I08U * _coldbootData,
				I32S _codeSize)
{
	I32S coldsize;
	Tcc353xBoot_t boot;
	I32S ret = TCC353X_RETURN_SUCCESS;

	if (Tcc353xColdbootParserUtil(_coldbootData, _codeSize, &boot) ==
	    TCC353X_RETURN_SUCCESS) {
		coldsize = boot.coldbootDataSize - 4;	/* Except CRC Size */
		Tcc353xDspAsmWrite(&Tcc353xHandle[_moduleIndex][0],
				   boot.coldbootDataPtr, coldsize);
		ret =
		    Tcc353xCodeCrcCheck(_moduleIndex, _coldbootData,
					&boot);
	} else {
		ret = TCC353X_RETURN_FAIL;
	}
	return ret;
}

static I32S Tcc353xCodeCrcCheck(I32S _moduleIndex, I08U * _coldbootData,
				Tcc353xBoot_t * _boot)
{
	I32S i;
	I32U destCrc, srcCrc;
	Tcc353xHandle_t *handle;
	I32S count = 1;
	I32S ret = TCC353X_RETURN_SUCCESS;
	I08U data[4];

	count = Tcc353xCurrentDiversityCount[_moduleIndex];

	for (i = count - 1; i >= 0; i--) {
#if !defined (_MODEL_TCC3535_)
		Tcc353xHandle[_moduleIndex][i].currentAddress =
		    Tcc353xHandle[_moduleIndex][i].originalAddress;
#endif
		handle = &Tcc353xHandle[_moduleIndex][i];

		Tcc353xGetRegDmaCrc32(handle, &data[0]);
		destCrc =
		    (data[0] << 24) | (data[1] << 16) | (data[2] << 8) |
		    (data[3]);
		srcCrc =
		    HTONL(GET4BYTES
			  (&_boot->coldbootDataPtr
			   [_boot->coldbootDataSize - 4]));

		if (destCrc == srcCrc) {
			TcpalPrintLog((I08S *)
				      "[TCC353X] [%d][%d] CRC Success!\n",
				      handle->moduleIndex,
				      handle->diversityIndex);
			TcpalPrintLog((I08S *)
				      "[TCC353X] [0x%x][0x%x]\n",
				      srcCrc, destCrc);
		} else {
			TcpalPrintErr((I08S *)
				      "[TCC353X] [%d][%d] CRC Fail!!!!!! \n",
				      _moduleIndex, i);
			TcpalPrintErr((I08S *)
				      "[TCC353X] [0x%x][0x%x]\n",
				      srcCrc, destCrc);
			ret = TCC353X_RETURN_FAIL;
		}
	}

	return ret;
}

static I32S Tcc353xInitBroadcasting(I32S _moduleIndex,
				    I08U * _coldbootData, I32S _codeSize)
{
	I32U i;
	I32S subret;
	I08U remapPc[3];
	I32S broadcastingFlag = 0;
	I16U plls = PLL_ISDB_T_FULLSEG;

	/* broad casting write */
	if (Tcc353xCurrentDiversityCount[_moduleIndex] > 1) {
		broadcastingFlag = 1;
#if !defined (_MODEL_TCC3535_)
		if (Tcc353xHandle[_moduleIndex][0].
		    options.commandInterface == TCC353X_IF_I2C)
			Tcc353xHandle[_moduleIndex][0].currentAddress =
			    0xA0;
		else if (Tcc353xHandle[_moduleIndex][0].
			 options.commandInterface == TCC353X_IF_TCCSPI)
			Tcc353xHandle[_moduleIndex][0].currentAddress =
			    (0xA0 >> 1);
		else
			Tcc353xHandle[_moduleIndex][0].currentAddress =
			    0xA0;
#endif
	}

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++)
		TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	/* ALL Parts Disable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][0], 0);

	/* set pll */
	if(Tcc353xHandle[_moduleIndex][0].options.pll == 0)
		Tcc353xHandle[_moduleIndex][0].useDefaultPLL = 1;
	else
		Tcc353xHandle[_moduleIndex][0].useDefaultPLL = 0;

	if (Tcc353xHandle[_moduleIndex][0].options.oscKhz == 38400)
		plls = PLL_ISDB_T_FULLSEG;
	else if (Tcc353xHandle[_moduleIndex][0].options.oscKhz == 19200)
		plls = OSC_192_PLL_ISDB_T_FULLSEG;
	else
		plls = PLL_ISDB_T_FULLSEG;

	Tcc353xSetPll(_moduleIndex, 0, 0, plls);
	
	/* EP Reset */
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex][0],
			      TC3XREG_SYS_RESET_EP, _LOCK_);
	/* EP Enable */
	Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][0],
			       TC3XREG_SYS_EN_EP);

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++)
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	/* remap */
	Tcc353xSetRegRemap(&Tcc353xHandle[_moduleIndex][0], 0x00);

	/* asm code download and roll-back current address */
	if (_coldbootData == NULL) {
		subret = TCC353X_RETURN_SUCCESS;
#if !defined (_MODEL_TCC3535_)
		if (Tcc353xCurrentDiversityCount[_moduleIndex] > 1) {
			if (Tcc353xHandle[_moduleIndex][0].options.
			    commandInterface == TCC353X_IF_I2C)
				Tcc353xHandle[_moduleIndex]
				    [0].currentAddress =
				    Tcc353xHandle[_moduleIndex]
				    [0].originalAddress;
			else if (Tcc353xHandle[_moduleIndex][0].options.
				 commandInterface == TCC353X_IF_TCCSPI)
				Tcc353xHandle[_moduleIndex]
				    [0].currentAddress =
				    Tcc353xHandle[_moduleIndex]
				    [0].originalAddress;
			else
				Tcc353xHandle[_moduleIndex]
				    [0].currentAddress =
				    Tcc353xHandle[_moduleIndex]
				    [0].originalAddress;
		}
#endif
	} else {
		subret =
		    Tcc353xCodeDownload(_moduleIndex, _coldbootData,
					_codeSize);
	}

	if (subret != TCC353X_RETURN_SUCCESS)
		return TCC353X_RETURN_FAIL;

	for (i = 0; i < Tcc353xCurrentDiversityCount[_moduleIndex]; i++) {
		Tcc353xHandle_t *h;
		h = &Tcc353xHandle[_moduleIndex][i];
		remapPc[0] = h->options.Config->initRemap_0x0D;
		remapPc[1] = h->options.Config->initPC_0x0E;
		remapPc[2] = h->options.Config->initPC_0x0F;
		Tcc353xSetRegRemapPc(h, &remapPc[0], 3);
	}

	return TCC353X_RETURN_SUCCESS;
}

static void Tcc353xSetInterruptControl(Tcc353xHandle_t * _handle)
{
	/* init irq disable */
	Tcc353xSetRegIrqMode(_handle,
			     _handle->options.Config->irqMode_0x02);
	Tcc353xSetRegIrqClear(_handle, TC3XREG_IRQ_STATCLR_ALL);
	if (_handle->options.useInterrupt)
		Tcc353xSetRegIrqEnable(_handle, 0);
}

static void Tcc353xOutBufferConfig(Tcc353xHandle_t * _handle)
{
	I08U data[2];

	data[0] = _handle->options.Config->bufferConfig_0x50;
	data[1] = _handle->options.Config->bufferConfig_0x51;
	Tcc353xSetRegOutBufferStartAddressA(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x52;
	data[1] = _handle->options.Config->bufferConfig_0x53;
	Tcc353xSetRegOutBufferEndAddressA(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x58;
	data[1] = _handle->options.Config->bufferConfig_0x59;
	Tcc353xSetRegOutBufferStartAddressB(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x5A;
	data[1] = _handle->options.Config->bufferConfig_0x5B;
	Tcc353xSetRegOutBufferEndAddressB(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x60;
	data[1] = _handle->options.Config->bufferConfig_0x61;
	Tcc353xSetRegOutBufferStartAddressC(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x62;
	data[1] = _handle->options.Config->bufferConfig_0x63;
	Tcc353xSetRegOutBufferEndAddressC(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x68;
	data[1] = _handle->options.Config->bufferConfig_0x69;
	Tcc353xSetRegOutBufferStartAddressD(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x6A;
	data[1] = _handle->options.Config->bufferConfig_0x6B;
	Tcc353xSetRegOutBufferEndAddressD(_handle, &data[0]);

	data[0] = _handle->options.Config->bufferConfig_0x54;
	data[1] = _handle->options.Config->bufferConfig_0x55;
	Tcc353xSetRegOutBufferAFifoThr(_handle, &data[0]);

	Tcc353xSetRegOutBufferConfig(_handle,
				     _handle->options.
				     Config->bufferConfig_0x4E);
	Tcc353xSetRegOutBufferInit(_handle,
				   _handle->options.
				   Config->bufferConfig_0x4F);
}

static void Tcc353xSetStreamControl(Tcc353xHandle_t * _handle)
{
	I08U data[4];
	I32U streamClkSpeed;
	I32U dlr;

	Tcc353xOutBufferConfig(_handle);

	data[0] = _handle->options.Config->streamDataConfig_0x1B;
	data[1] = _handle->options.Config->streamDataConfig_0x1C;
	data[2] = _handle->options.Config->streamDataConfig_0x1D;
	data[3] = _handle->options.Config->streamDataConfig_0x1E;
	Tcc353xSetRegStreamConfig(_handle, &data[0]);

	data[0] = _handle->options.Config->periConfig_0x30;
	data[1] = _handle->options.Config->periConfig_0x31;
	data[2] = _handle->options.Config->periConfig_0x32;
	data[3] = _handle->options.Config->periConfig_0x33;
	Tcc353xSetRegPeripheralConfig(_handle, &data[0]);

	if ((_handle->options.Config->periConfig_0x30 & 0x30) == 0x10) {
		/* spi ms */
		I64U temp;
		I64U temp2;
		dlr =
		    ((_handle->options.
		      Config->periConfig_0x31 & 0x1C) >> 2);
		temp2 = _handle->mainClkKhz;
		temp = (DIV(temp2, ((1 + dlr) << 1))) >> SCALE;
		streamClkSpeed = (I32U) (temp);
		TcpalPrintLog((I08S *)
			      "[TCC353X] SET SPI Clk : %d khz [DLR : %d]\n",
			      streamClkSpeed, dlr);
	} else if ((_handle->options.Config->periConfig_0x30 & 0x30) ==
		   0x20) {
		/* ts */
		I64U temp;
		I64U temp2;
		dlr = (_handle->options.Config->periConfig_0x31 & 0x07);
		temp2 = _handle->mainClkKhz;
		temp = (DIV(temp2, ((1 + dlr) << 1))) >> SCALE;
		streamClkSpeed = (I32U) (temp);
		TcpalPrintLog((I08S *)
			      "[TCC353X] SET TS Clk : %d khz [DLR : %d]\n",
			      streamClkSpeed, dlr);
	} else {
		;		/*none */
	}

}

void Tcc353xPeripheralOnOff(Tcc353xHandle_t * _handle, I32S _onoff)
{
	if (_onoff)
		Tcc353xSetRegPeripheralConfig0(_handle,
					       _handle->options.
					       Config->periConfig_0x30 |
					       TC3XREG_PERI_EN |
					       TC3XREG_PERI_INIT_AUTOCLR);
	else
		Tcc353xSetRegPeripheralConfig0(_handle,
					       _handle->options.
					       Config->periConfig_0x30 |
					       TC3XREG_PERI_INIT_AUTOCLR);
}

static void Tcc353xSetGpio(Tcc353xHandle_t * _handle)
{
	Tcc353xSetRegIoCfgMux(_handle, 0x00);
	Tcc353xSetRegGpioAlt(_handle,
			     _handle->options.Config->gpioAlt_0x10_07_00);
	Tcc353xSetRegGpioDR(_handle,
			    _handle->options.Config->gpioDr_0x11_07_00);
	Tcc353xSetRegGpioLR(_handle,
			    _handle->options.Config->gpioLr_0x12_07_00);
	Tcc353xSetRegGpioDRV(_handle,
			     _handle->options.Config->gpioDrv_0x13_07_00);
	Tcc353xSetRegGpioPE(_handle,
			    _handle->options.Config->gpioPe_0x14_07_00);
	Tcc353xSetRegGpiosDRV(_handle,
			      _handle->options.
			      Config->gpioSDrv_0x15_07_00);

	Tcc353xSetRegIoCfgMux(_handle, 0x01);
	Tcc353xSetRegGpioAlt(_handle,
			     _handle->options.Config->gpioAlt_0x10_15_08);
	Tcc353xSetRegGpioDR(_handle,
			    _handle->options.Config->gpioDr_0x11_15_08);
	Tcc353xSetRegGpioLR(_handle,
			    _handle->options.Config->gpioLr_0x12_15_08);
	Tcc353xSetRegGpioDRV(_handle,
			     _handle->options.Config->gpioDrv_0x13_15_08);
	Tcc353xSetRegGpioPE(_handle,
			    _handle->options.Config->gpioPe_0x14_15_08);
	Tcc353xSetRegGpiosDRV(_handle,
			      _handle->options.
			      Config->gpioSDrv_0x15_15_08);

	Tcc353xSetRegIoCfgMux(_handle, 0x02);
	Tcc353xSetRegGpioAlt(_handle,
			     _handle->options.Config->gpioAlt_0x10_23_16);
	Tcc353xSetRegGpioDR(_handle,
			    _handle->options.Config->gpioDr_0x11_23_16);
	Tcc353xSetRegGpioLR(_handle,
			    _handle->options.Config->gpioLr_0x12_23_16);
	Tcc353xSetRegGpioDRV(_handle,
			     _handle->options.Config->gpioDrv_0x13_23_16);
	Tcc353xSetRegGpioPE(_handle,
			    _handle->options.Config->gpioPe_0x14_23_16);
	Tcc353xSetRegGpiosDRV(_handle,
			      _handle->options.
			      Config->gpioSDrv_0x15_23_16);

	Tcc353xSetRegIoMISC(_handle, _handle->options.Config->ioMisc_0x16);
}

static I32S Tcc353xSetPll(I32S _moduleIndex, I32S _deviceIndex, 
			  I32S _flagInterfaceLock, I16U _pllValue)
{
	I08U PLL6, PLL7;
	I08U pll_f, pll_m, pll_r, pll_od;
	I32U fout, fvco;
	I08U lockFlag;
	Tcc353xHandle_t *h;

	if (_flagInterfaceLock == 0)
		lockFlag = _LOCK_;
	else
		lockFlag = _UNLOCK_;
	
	h = (Tcc353xHandle_t *)(&Tcc353xHandle[_moduleIndex][_deviceIndex]);

	if(Tcc353xHandle[_moduleIndex][0].useDefaultPLL == 1)
		h->options.pll = _pllValue;

	PLL6 = (h->options.pll >> 8) & 0x007f;
	PLL7 = ((h->options.pll) & 0xFF);

	/* for stablility */
	Tcc353xSetRegPll8(h, 0x28, lockFlag);
	Tcc353xSetRegPll9(h, 0x64, lockFlag);

	Tcc353xSetRegPll6(h, PLL6, lockFlag);
	Tcc353xSetRegPll7(h, PLL7, lockFlag);
	
	Tcc353xSetRegPll6(h, PLL6 | 0x80, lockFlag);
	TcpalmDelay(1);		/* 1ms (orig: 340us) */

	pll_m = ((PLL6 & 0x40) >> 6);
	pll_f = (PLL6 & 0x3f) + 1;
	pll_r = ((PLL7 >> 3)&0x0F) + 1;

	fvco = (I32U) (MUL(h->options.oscKhz,DIV(pll_f, pll_r)));
	pll_od = ((PLL7 & 0x06) >> 1);

	if (pll_od)
		fout = fvco >> pll_od;
	else
		fout = fvco;

	if (pll_m)
		fout = fout >> pll_m;

	h->mainClkKhz = fout;
	TcpalPrintLog((I08S *)"[TCC353X] PLLSet %dkHz\n", h->mainClkKhz);
	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xChangePll(I32S _moduleIndex, I16U _pllValue)
{
	I32S i;

	/* lock all interface */
	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--)
		TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	/* change pll */
	/* slave first for stability */
	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--) {
		/* ALL Parts Disable */
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i], 
				       TC3XREG_SYS_EN_OPCLK);
		TcpalmDelay(1);

		/* dsp reset */
		Tcc353xSetRegSysReset(
			&Tcc353xHandle[_moduleIndex][i], 
			TC3XREG_SYS_RESET_DSP, _UNLOCK_);
		TcpalmDelay(1);
		/* ep reset */
		Tcc353xSetRegSysReset(
			&Tcc353xHandle[_moduleIndex][i], 
			TC3XREG_SYS_RESET_EP, _UNLOCK_);
		/* change pll */
		Tcc353xSetPll(_moduleIndex, i, 1, _pllValue);

		/* DSP Enable */
		Tcc353xSetRegSysEnable(&Tcc353xHandle[_moduleIndex][i],
				       TC3XREG_SYS_EN_EP |
				       TC3XREG_SYS_EN_DSP |
				       TC3XREG_SYS_EN_OPCLK |
				       TC3XREG_SYS_EN_RF);
	}

	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--)
		Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][i]);

	for (i = Tcc353xCurrentDiversityCount[_moduleIndex]-1; i >= 0 ; i--)
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex][i]);

	return TCC353X_RETURN_SUCCESS;
}

static void Tcc353xSaveOption(I32S _moduleIndex, I32S _diversityIndex,
			      Tcc353xOption_t * _Tcc353xOption)
{
	TcpalMemcpy(&Tcc353xHandle[_moduleIndex][_diversityIndex].options,
		    &_Tcc353xOption[0], sizeof(Tcc353xOption_t));
	TcpalMemcpy(&Tcc353xRegisterOptions[_moduleIndex][_diversityIndex],
		    _Tcc353xOption[0].Config,
		    sizeof(Tcc353xRegisterConfig_t));

	Tcc353xHandle[_moduleIndex][_diversityIndex].options.Config =
	    &Tcc353xRegisterOptions[_moduleIndex][_diversityIndex];
	Tcc353xHandle[_moduleIndex][_diversityIndex].moduleIndex =
	    (I08U) _moduleIndex;
	Tcc353xHandle[_moduleIndex][_diversityIndex].diversityIndex = 
	    (I08U)(_diversityIndex);

	switch (Tcc353xHandle[_moduleIndex][_diversityIndex].
		options.commandInterface) {
	case TCC353X_IF_I2C:
		Tcc353xHandle[_moduleIndex][_diversityIndex].currentAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		Tcc353xHandle[_moduleIndex][_diversityIndex].originalAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		break;

	case TCC353X_IF_TCCSPI:
		Tcc353xHandle[_moduleIndex][_diversityIndex].currentAddress =
		    (Tcc353xHandle[_moduleIndex][_diversityIndex].
		     options.address >> 1);
		Tcc353xHandle[_moduleIndex][_diversityIndex].originalAddress =
		    (Tcc353xHandle[_moduleIndex][_diversityIndex].
		     options.address >> 1);
		break;

	default:
		Tcc353xHandle[_moduleIndex][_diversityIndex].currentAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		Tcc353xHandle[_moduleIndex][_diversityIndex].originalAddress =
		    Tcc353xHandle[_moduleIndex][_diversityIndex].
		    options.address;
		TcpalPrintErr((I08S *)
			      "[TCC353X] Driver Can't support your interface yet\n");
		break;
	}
}

static void Tcc353xConnectCommandInterface(I32S _moduleIndex, 
					   I32S _diversityIndex,
					   I08S _commandInterface)
{
	I32U i;
	i = (I32U)(_diversityIndex);

	switch (_commandInterface) {
	case TCC353X_IF_I2C:
		Tcc353xHandle[_moduleIndex][i].Read =
		    Tcc353xI2cRead;
		Tcc353xHandle[_moduleIndex][i].Write =
		    Tcc353xI2cWrite;
		Tcc353xHandle[_moduleIndex][i].currentAddress =
		    Tcc353xHandle[_moduleIndex][i].options.address;
		Tcc353xHandle[_moduleIndex][i].originalAddress =
		    Tcc353xHandle[_moduleIndex][i].options.address;
		TcpalPrintLog((I08S *)
			      "[TCC353X] Interface is I2C\n");
		break;
	
	case TCC353X_IF_TCCSPI:
		Tcc353xHandle[_moduleIndex][i].Read =
		    Tcc353xTccspiRead;
		Tcc353xHandle[_moduleIndex][i].Write =
		    Tcc353xTccspiWrite;
		Tcc353xHandle[_moduleIndex][i].currentAddress =
		    (Tcc353xHandle[_moduleIndex][i].
		     options.address >> 1);
		Tcc353xHandle[_moduleIndex][i].originalAddress =
		    (Tcc353xHandle[_moduleIndex][i].
		     options.address >> 1);
		TcpalPrintLog((I08S *)
			      "[TCC353X] Interface is Tccspi\n");
		break;
	
	default:
		TcpalPrintErr((I08S *)
			      "[TCC353X] Driver Can't support your interface yet\n");
		break;
	}
}


static I32S Tcc353xColdbootParserUtil(I08U * pData, I32U size,
				      Tcc353xBoot_t * pBOOTBin)
{
	I32U idx;
	I32U length;
	I08U *pBin;
	I08U *daguDataPtr;
	I08U *dintDataPtr;
	I08U *randDataPtr;
	I08U *colOrderDataPtr;
	I32U BootSize[5];

	/*
	 * coldboot         0x00000001
	 * dagu             0x00000002
	 * dint             0x00000003
	 * rand             0x00000004
	 * col_order        0x00000005
	 * sizebyte         4byte
	 * data             nbyte
	 */

	TcpalMemset(BootSize, 0, sizeof(I32U) * 5);

	/* cold boot */
	idx = 0;
	if (pData[idx + 3] != 0x01) {
		return TCC353X_RETURN_FAIL;
	}
	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	BootSize[0] = length;
	pBin = &pData[idx];
	idx += length;
	size -= (length + 8);

	/* dagu */
	if (pData[idx + 3] != 0x02) {
		return TCC353X_RETURN_FAIL;
	}
	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	if (length) {
		daguDataPtr = &pData[idx];
		BootSize[1] = length;
		idx += length;
	} else {
		BootSize[1] = 0;
	}
	size -= (length + 8);

	/* dint */
	if (pData[idx + 3] != 0x03) {
		return TCC353X_RETURN_FAIL;
	}
	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	if (length) {
		dintDataPtr = &pData[idx];
		BootSize[2] = length;
		idx += length;
	} else {
		dintDataPtr = NULL;
		BootSize[2] = 0;
	}
	size -= (length + 8);

	/* rand */
	if (pData[idx + 3] != 0x04) {
		return TCC353X_RETURN_FAIL;
	}

	idx += 4;
	length =
	    (pData[idx] << 24) + (pData[idx + 1] << 16) +
	    (pData[idx + 2] << 8) + (pData[idx + 3]);
	idx += 4;

	if (length) {
		randDataPtr = &pData[idx];
		BootSize[3] = length;
		idx += length;
	} else {
		randDataPtr = NULL;
		BootSize[3] = 0;
	}
	size -= (length + 8);

	if (size >= 8) {
		if (pData[idx + 3] != 0x05) {
			return TCC353X_RETURN_FAIL;
		}

		idx += 4;
		length =
		    (pData[idx] << 24) + (pData[idx + 1] << 16) +
		    (pData[idx + 2] << 8) + (pData[idx + 3]);
		idx += 4;

		if (length) {
			colOrderDataPtr = &pData[idx];
			BootSize[4] = length;
			idx += length;
		} else {
			colOrderDataPtr = NULL;
			BootSize[4] = 0;
		}
		size -= (length + 8);
	}

	pBOOTBin->coldbootDataPtr = pBin;
	pBOOTBin->coldbootDataSize = BootSize[0];
	pBOOTBin->daguDataPtr = daguDataPtr;
	pBOOTBin->daguDataSize = BootSize[1];
	pBOOTBin->dintDataPtr = dintDataPtr;
	pBOOTBin->dintDataSize = BootSize[2];
	pBOOTBin->randDataPtr = randDataPtr;
	pBOOTBin->randDataSize = BootSize[3];
	pBOOTBin->colOrderDataPtr = colOrderDataPtr;
	pBOOTBin->colOrderDataSize = BootSize[4];

	return TCC353X_RETURN_SUCCESS;
}

I32U Tcc353xGetCoreVersion()
{
	return Tcc353xCoreVersion;
}

I32S Tcc353xMailboxWrite(I32S _moduleIndex, I32S _diversityIndex,
			 I32U _command, I32U * dataArray, I32S wordSize)
{
	I32S ret = TCC353X_RETURN_SUCCESS;
	ret = Tcc353xMailboxTxOnly(&Tcc353xHandle[_moduleIndex]
				   [_diversityIndex], _command, dataArray,
				   wordSize);
	return ret;
}

I32S Tcc353xMailboxRead(I32S _moduleIndex, I32S _diversityIndex,
			I32U _command, mailbox_t * _mailbox)
{
	I32S ret = TCC353X_RETURN_SUCCESS;
	ret = Tcc353xMailboxTxRx(&Tcc353xHandle[_moduleIndex]
				 [_diversityIndex], _mailbox, _command,
				 NULL, 0);
	return ret;
}

I32S Tcc353xGetOpStatus(I32S _moduleIndex, I32S _diversityIndex,
			   I32U* _opStatusData, I32U _dataSize)
{
	I08U datas[32];
	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	Tcc353xGetRegOPStatus(&Tcc353xHandle[_moduleIndex]
				 [_diversityIndex], &datas[0], _dataSize,
				   _UNLOCK_);
	TcpalMemcpy ((void *)(_opStatusData), (void *)(&datas[0]), 32);
	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);

	return TCC353X_RETURN_SUCCESS;
}


static I32S Tcc353xSetOpConfig(I32S _moduleIndex, I32S _diversityIndex,
			I32U * _opConfig, I32U _firstFlag)
{
	I32S i;
	I08U opconfigAddress[16];
	I32U opconfigValue[16];
	I32S count = 0;

	if(_firstFlag) {
		Tcc353xMiscWriteExIncrease(_moduleIndex, _diversityIndex,
				      MISC_OP_REG_CTRL, TC3XREG_OP_CFG00, 
				      &_opConfig[0], 16);
	}
	else {
		for(i = 0; i<16; i++)	{
			if((OriginalOpConfig[_moduleIndex][_diversityIndex][i]
			   != _opConfig[i]) || (i==6)) {
				opconfigAddress[count] = (I08U)(i);
				opconfigValue[count] = _opConfig[i];
				count++;
			}
		}
		Tcc353xMiscWriteEx(_moduleIndex, _diversityIndex,
				      MISC_OP_REG_CTRL, &opconfigAddress[0], 
				      &opconfigValue[0], count);
	}

	TcpalMemcpy (&OriginalOpConfig[_moduleIndex][_diversityIndex][0], 
		     _opConfig, sizeof(I32U)*16);
	return TCC353X_RETURN_SUCCESS;
}

static I32U Tcc353xSearchDpllTable (I32U _frequencyInfo, I32U *_tables,
				    I32U _maxFreqNum, I64U *_rcStep, 
				    I32U *_adcClkCfg, I32U _defaultPll)
{
	I32U i;
	I32U index;
	I64U data = 0;
	I32U pll;

	pll = _defaultPll;

	for(i = 0; i<_maxFreqNum; i++)	{
		index = (i*5);
		
		if(_tables[index] == 0) {
			/* last search, can't search frequency */
			pll = _tables[index+1];
			break;
		}
		if(_tables[index] == _frequencyInfo) {
			data = _tables[index+2];
			_rcStep[0] = ((data << 32) | _tables[index+3]);
			_adcClkCfg[0] = _tables[index+4];
			pll = _tables[index+1];
			break;
		}
	}

	return pll;
}

static I32S Tcc353xApplySpurSuppression_38400 (I32S _moduleIndex, 
					 Tcc353xTuneOptions *_tuneOption, 
					 I64U *_rcStep, I32U *_adcClkCfg,
					 I32U *_icic, I32U _frequencyInfo)
{
	I32U pllValue;
	/* 0 : not match, 1:partial 1seg match, 13:full seg match */
	I64U changeRcStep = _rcStep[0];
	I32U changeadcClkCfg = _adcClkCfg[0];

	pllValue = Tcc353xHandle[_moduleIndex][0].options.pll;

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		Tcc353xSearchDpllTable (_frequencyInfo, 
					&DpllTable_Partial1Seg[0],
					_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					PLL_ISDB_TMM_PARTIAL_1_SEG);
		break;
	case TCC353X_ISDBT_13SEG:
		pllValue = Tcc353xSearchDpllTable (_frequencyInfo, 
					&DpllTable_FullSeg[0],
					_MAX_FULLSEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					PLL_ISDB_T_FULLSEG);
		
		if (pllValue == PLL_ISDB_TMM_FULLSEG) /* isdb-tmm 13 case */
			_icic[0] = 0; /* icic value change for isdb-tmm*/
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_USER_13SEG[0],
						_MAX_TMM_USER_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_USER_1SEG[0],
						_MAX_TMM_USER_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == A_1st_13Seg||
			 _tuneOption->tmmSet == A_2nd_13Seg||
			 _tuneOption->tmmSet == B_1st_13Seg||
			 _tuneOption->tmmSet == B_2nd_13Seg||
			 _tuneOption->tmmSet == C_1st_13Seg||
			 _tuneOption->tmmSet == C_2nd_13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_13SEG[0],
						_MAX_TMM_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_1SEG[0],
						_MAX_TMM_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		break;
	default:
		return -1;
		break;
	}

	_rcStep[0] = changeRcStep;
	_adcClkCfg[0] = changeadcClkCfg;

	return 0;
}

static I32S Tcc353xApplySpurSuppression_19200 (I32S _moduleIndex, 
					 Tcc353xTuneOptions *_tuneOption, 
					 I64U *_rcStep, I32U *_adcClkCfg,
					 I32U *_icic, I32U _frequencyInfo)
{
	I32U pllValue;
	/* 0 : not match, 1:partial 1seg match, 13:full seg match */
	I64U changeRcStep = _rcStep[0];
	I32U changeadcClkCfg = _adcClkCfg[0];

	pllValue = Tcc353xHandle[_moduleIndex][0].options.pll;

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		Tcc353xSearchDpllTable (_frequencyInfo, 
					&OSC_19200_DpllTable_Partial1Seg[0],
					_OSC_19200_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					OSC_192_PLL_ISDB_TMM_PARTIAL_1_SEG);
		break;
	case TCC353X_ISDBT_13SEG:
		pllValue = Tcc353xSearchDpllTable (_frequencyInfo, 
					&OSC_19200_DpllTable_FullSeg[0],
					_OSC_19200_MAX_FULLSEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					OSC_192_PLL_ISDB_T_FULLSEG);
		
		if (pllValue == OSC_192_PLL_ISDB_TMM_FULLSEG) /* isdb-tmm 13 case */
			_icic[0] = 0; /* icic value change for isdb-tmm*/
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&OSC_19200_DpllTable_TMM_USER_13SEG[0],
						_OSC_19200_MAX_TMM_USER_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						OSC_192_PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&OSC_19200_DpllTable_TMM_USER_1SEG[0],
						_OSC_19200_MAX_TMM_USER_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						OSC_192_PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == A_1st_13Seg||
			 _tuneOption->tmmSet == A_2nd_13Seg||
			 _tuneOption->tmmSet == B_1st_13Seg||
			 _tuneOption->tmmSet == B_2nd_13Seg||
			 _tuneOption->tmmSet == C_1st_13Seg||
			 _tuneOption->tmmSet == C_2nd_13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&OSC_19200_DpllTable_TMM_13SEG[0],
						_OSC_19200_MAX_TMM_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						OSC_192_PLL_ISDB_TMM_FULLSEG);
		else
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&OSC_19200_DpllTable_TMM_1SEG[0],
						_OSC_19200_MAX_TMM_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						OSC_192_PLL_ISDB_TMM_FULLSEG);
		break;
	default:
		return -1;
		break;
	}

	_rcStep[0] = changeRcStep;
	_adcClkCfg[0] = changeadcClkCfg;

	return 0;
}

static I32S Tcc353xApplySpurSuppression_tcc3535 (I32S _moduleIndex, 
					 Tcc353xTuneOptions *_tuneOption, 
					 I64U *_rcStep, I32U *_adcClkCfg,
					 I32U *_icic, I32U _frequencyInfo)
{
	I32U pllValue;
	/* 0 : not match, 1:partial 1seg match, 13:full seg match */
	I64U changeRcStep = _rcStep[0];
	I32U changeadcClkCfg = _adcClkCfg[0];

	pllValue = Tcc353xHandle[_moduleIndex][0].options.pll;

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		Tcc353xSearchDpllTable (_frequencyInfo, 
					&DpllTable_Partial1Seg_tcc3535[0],
					_MAX_PARTIAL_1SEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					PLL_ISDB_TMM_PARTIAL_1_SEG);
		break;
	case TCC353X_ISDBT_13SEG:
		pllValue = Tcc353xSearchDpllTable (_frequencyInfo, 
					&DpllTable_FullSeg_tcc3535[0],
					_MAX_FULLSEG_FREQ_NUM_, 
					&changeRcStep, &changeadcClkCfg,
					PLL_ISDB_T_FULLSEG);
		
		if (pllValue == PLL_ISDB_TMM_FULLSEG) /* isdb-tmm 13 case */
			_icic[0] = 0; /* icic value change for isdb-tmm*/
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == UserDefine_Tmm13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_USER_13SEG_tcc3535[0],
						_MAX_TMM_USER_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == UserDefine_Tmm1Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_USER_1SEG_tcc3535[0],
						_MAX_TMM_USER_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else if (_tuneOption->tmmSet == A_1st_13Seg||
			 _tuneOption->tmmSet == A_2nd_13Seg||
			 _tuneOption->tmmSet == B_1st_13Seg||
			 _tuneOption->tmmSet == B_2nd_13Seg||
			 _tuneOption->tmmSet == C_1st_13Seg||
			 _tuneOption->tmmSet == C_2nd_13Seg)
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_13SEG_tcc3535[0],
						_MAX_TMM_13SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		else
			Tcc353xSearchDpllTable (_frequencyInfo, 
						&DpllTable_TMM_1SEG_tcc3535[0],
						_MAX_TMM_1SEG_FREQ_NUM_, 
						&changeRcStep, &changeadcClkCfg,
						PLL_ISDB_TMM_FULLSEG);
		break;
	default:
		return -1;
		break;
	}

	_rcStep[0] = changeRcStep;
	_adcClkCfg[0] = changeadcClkCfg;

	return 0;
}

static void Tcc353xGetOpconfigValues(I32S _moduleIndex,
				     I32S _diversityIndex,
				     Tcc353xTuneOptions * _tuneOption,
				     I32U * _opConfig, I32U _frequencyInfo)
{
	/* opconfig version higher than 0.0.15 */
	I32U LSEL, TDF_SEL, OU, DIV_CFG, AH, GMODE, TMODE, CT_OM,
	    START_SUB_CH, S_TYPE, S, ICIC, ASE;
	I32U ADC_CLK_CFG, FP_CLK_CFG, DIV_CLK_CFG;
	I32U FP_GLB_CFG, ADC_GLB_CFG;
	I32U DC_CFG;
	I32U OM_MODE, ME, TMCC_SEG_FLAG, CFO_SEG_FLAG;
	I32U AFC_STEP;
	I32U CFO_ER;
	I64U RC_STEP;
	I32U frequencyForm;
	I32U tripleBandRfFlag = 0;
	I32U DIV_NUM_CFG = 0;
	I32U AGC_TR_SPEED = 3;

	if(Tcc353xHandle[_moduleIndex][0].options.rfType == 
	   TCC353X_TRIPLE_BAND_RF)
		tripleBandRfFlag = 1;	/* only tcc3531,tcc3535 case, others don't care */
	else
		tripleBandRfFlag = 0;

	if(Tcc353xCurrentDiversityCount[_moduleIndex] > 2)
		DIV_NUM_CFG = 1;
	else
		DIV_NUM_CFG = 0;


	frequencyForm = ((_frequencyInfo >> 4) & 0xFFFF);
	S = 1;
	ME = 0;
	CT_OM = 1;
	TMODE = 0;
	GMODE = 0;
	AH = 1;
	TDF_SEL = 2;	/* 1seg - low if -> 1 */
	LSEL = 0;
	OM_MODE = 0;
	CFO_ER = 3;
	RC_STEP = 0x2747C9D1F2LL;

	if(Tcc353xHandle[_moduleIndex][0].TuneOptions.BandwidthMHz == 8)
		RC_STEP = 0x0000000329161F9ALL;

	ASE = 1;

	if (Tcc353xHandle[_moduleIndex][0].options.boardType ==
	    TCC353X_BOARD_SINGLE) {
		DIV_CFG = 0x00;
	} else {
		if (Tcc353xHandle[_moduleIndex][_diversityIndex].options.
		    diversityPosition == TCC353X_DIVERSITY_MASTER)
			DIV_CFG = 0x15E;
		else if (Tcc353xHandle[_moduleIndex]
			 [_diversityIndex].options.diversityPosition ==
			 TCC353X_DIVERSITY_MID)
			DIV_CFG = 0x11F;
		else
			DIV_CFG = 0x13D;
	}

	switch (_tuneOption->segmentType) {
	case TCC353X_ISDBT_1_OF_13SEG:
		S_TYPE = 0;
		START_SUB_CH = 21;
		ICIC = 0;	/*ICI cancellation */
		OU = 1;
		break;
	case TCC353X_ISDBT_13SEG:
		S_TYPE = 2;
		START_SUB_CH = 3;
		ICIC = 1;	/*ICI cancellation */
		OU = 0;
		break;
	case TCC353X_ISDBTSB_1SEG:
		S_TYPE = 0;
		/*START_SUB_CH = 3;*/
		START_SUB_CH = _tuneOption->tsbStartSubChannelNum;
		ICIC = 0;	/*ICI cancellation */
		OU = 1;
		break;
	case TCC353X_ISDBTSB_3SEG:
		S_TYPE = 1;
		/*START_SUB_CH = 3;*/
		if(_tuneOption->tsbStartSubChannelNum>=3)
			START_SUB_CH = _tuneOption->tsbStartSubChannelNum-3;
		else 
			START_SUB_CH = 3;
		ICIC = 0;	/*ICI cancellation */
		OU = 0;
		break;
	case TCC353X_ISDBTSB_1_OF_3SEG:
		S_TYPE = 0;
		/*START_SUB_CH = 3;*/
		START_SUB_CH = _tuneOption->tsbStartSubChannelNum;
		ICIC = 0;	/*ICI cancellation */
		OU = 1;
		break;
	case TCC353X_ISDBTMM:
		if (_tuneOption->tmmSet == A_1st_13Seg ||
		    _tuneOption->tmmSet == A_2nd_13Seg ||
		    _tuneOption->tmmSet == B_1st_13Seg ||
		    _tuneOption->tmmSet == B_2nd_13Seg ||
		    _tuneOption->tmmSet == C_1st_13Seg ||
		    _tuneOption->tmmSet == C_2nd_13Seg ||
		    _tuneOption->tmmSet == UserDefine_Tmm13Seg) {
			/*13seg */
			OU = 0;
			S_TYPE = 2;
			START_SUB_CH = 3;
		} else {
			OU = 1;
			S_TYPE = 0;
			if(_tuneOption->tmmSet == UserDefine_Tmm1Seg)
				START_SUB_CH = 21;
			else if (_tuneOption->tmmSet <= A_7th_1Seg)
				START_SUB_CH =
				    (_tuneOption->tmmSet - A_1st_1Seg) * 3;
			else if (_tuneOption->tmmSet <= B_7th_1Seg)
				START_SUB_CH =
				    (_tuneOption->tmmSet - B_1st_1Seg) * 3;
			else
				START_SUB_CH =
				    (_tuneOption->tmmSet - C_1st_1Seg) * 3;
		}
		ICIC = 0;	/*ICI cancellation */
		break;
	default:
		S_TYPE = 2;
		START_SUB_CH = 3;
		ICIC = 0;	/*ICI cancellation */
		OU = 0;
		break;
	}

	if (S_TYPE == 0)
		if(Tcc353xHandle[_moduleIndex][0].TuneOptions.BandwidthMHz == 8)
			ADC_CLK_CFG = 0x27;	/* 1seg, partial 1seg */
		else
			ADC_CLK_CFG = 0x28;
	else if (S_TYPE == 1)
		ADC_CLK_CFG = 0x24;	/* 3seg */
	else
		ADC_CLK_CFG = 0x21;	/* 13seg */

	if(Tcc353xHandle[_moduleIndex][0].options.streamInterface== 
						TCC353X_STREAM_IO_MAINIO) {
		if(S_TYPE == 0)
			OM_MODE = 0x00;
		else if(S_TYPE ==2 && _tuneOption->segmentType ==
							TCC353X_ISDBTMM)
			OM_MODE = 0x1c;
		else if(S_TYPE ==2 && _tuneOption->segmentType ==
							TCC353X_ISDBT_13SEG)
			OM_MODE = 0x1c;
		else if(S_TYPE ==1 && _tuneOption->segmentType ==
							TCC353X_ISDBTSB_3SEG)
			OM_MODE = 0x1c;
		else
			OM_MODE = 0x00;
	}

	FP_CLK_CFG = 0x02;
	DIV_CLK_CFG = 0x02;

	if (Tcc353xHandle[_moduleIndex][0].TuneOptions.rfIfType ==
	    TCC353X_LOW_IF) {
		FP_GLB_CFG = 0x00C9;
		ADC_GLB_CFG = 0x00E1;
		if(Tcc353xHandle[_moduleIndex][0].TuneOptions.BandwidthMHz == 8)
			AFC_STEP = 0x03430001;
		else if(S_TYPE == 1) 
			AFC_STEP = 0x03820001;
		else 
			AFC_STEP = 0x03040001;
	} else {
		FP_GLB_CFG = 0x0309;
		ADC_GLB_CFG = 0x00E2;
		AFC_STEP = 0x00000000;
	}

	DC_CFG = 0x0001969A;

	if (S_TYPE == 0) {
		/* 1seg, partial 1seg */
		TMCC_SEG_FLAG = 0x01;
		CFO_SEG_FLAG = 0x01;
		TDF_SEL = 1;
	} else if (S_TYPE == 1) {
		/* 3seg */
		TMCC_SEG_FLAG = 0x02;
		CFO_SEG_FLAG = 0x02;
		TDF_SEL = 1;
	} else {
		/* 13seg */
		TMCC_SEG_FLAG = 0x1803;
		CFO_SEG_FLAG = 0x404;
	}

	/* Spur - ADC Clock Control */
	if(Tcc353xHandle[_moduleIndex][0].TuneOptions.BandwidthMHz == 8) {
	} else {
		if(Tcc353xHandle[_moduleIndex][0].options.basebandName 
		   == BB_TCC3535)
			Tcc353xApplySpurSuppression_tcc3535(_moduleIndex, 
						    _tuneOption, 
						    &RC_STEP, &ADC_CLK_CFG, 
						    &ICIC, _frequencyInfo);
		else if(Tcc353xHandle[_moduleIndex][0].options.oscKhz == 38400)
			Tcc353xApplySpurSuppression_38400(_moduleIndex, 
						    _tuneOption, 
						    &RC_STEP, &ADC_CLK_CFG, 
						    &ICIC, _frequencyInfo);
		else if (Tcc353xHandle[_moduleIndex][0].options.oscKhz == 19200)
			Tcc353xApplySpurSuppression_19200(_moduleIndex, 
						    _tuneOption, 
						    &RC_STEP, &ADC_CLK_CFG, 
						    &ICIC, _frequencyInfo);
		else
			Tcc353xApplySpurSuppression_38400(_moduleIndex, 
						    _tuneOption, 
						    &RC_STEP, &ADC_CLK_CFG, 
						    &ICIC, _frequencyInfo);
	}

	_opConfig[0] =
	    (LSEL << 30) | (ASE << 29) | (ICIC << 28) | (TDF_SEL << 26) |
	    (OU << 25) | (DIV_CFG << 16) | (AH << 15) | (GMODE << 13) |
	    (TMODE << 11) | (CT_OM << 9) | (START_SUB_CH << 3) | (S_TYPE <<
								  1) | (S);
	_opConfig[1] = 0x368285E5;	/* layer - A only ts resync enable */
	_opConfig[2] =
	   (DIV_NUM_CFG<<31) | (AGC_TR_SPEED<<21) | (tripleBandRfFlag<<20) | 
	   (CFO_ER<<18) | (ADC_CLK_CFG << 12) | 
	   (FP_CLK_CFG << 6) | DIV_CLK_CFG;
	_opConfig[3] = (FP_GLB_CFG << 16) | (ADC_GLB_CFG);
	_opConfig[4] = DC_CFG;
	_opConfig[5] =
	    (OM_MODE << 27) | (ME << 26) | (TMCC_SEG_FLAG << 13) |
	    (CFO_SEG_FLAG);
	_opConfig[6] = ((I32U)((RC_STEP>>32)&0xFF) | (frequencyForm << 16));
	_opConfig[7] = (I32U)(RC_STEP & 0xFFFFFFFF);
	_opConfig[8] = AFC_STEP;
	_opConfig[9] = 0x00000000;
	_opConfig[10] = 0x00000000;
	_opConfig[11] = 0x00000000;
	_opConfig[12] = 0x00000000;
	_opConfig[13] = 0xC2A8FF09;
	_opConfig[14] = 0x01BEFF16;

	switch(Tcc353xHandle[_moduleIndex][0].options.basebandName) {
	case BB_TCC3530:
		if(S_TYPE==2)
			_opConfig[15] = 0x03BEFF42;
		else
			_opConfig[15] = 0x03BEFF43;
		break;
	case BB_TCC3531:
		_opConfig[15] = 0x03BEFF43;
		break;
	case BB_TCC3532:
		_opConfig[15] = 0x03BEFF43;
		break;
	case BB_TCC3535:
		if(S_TYPE==2)
			_opConfig[15] = 0x03BEFF42;
		else
			_opConfig[15] = 0x03BEFF43;
		break;
	default:
		TcpalPrintErr((I08S *) "[TCC353X] No baseband name selected\n");
		break;
	}
}

I32S Tcc353xGetTMCCInfo(I32S _moduleIndex, I32S _diversityIndex,
			tmccInfo_t * _tmccInfo)
{
	mailbox_t mailbox;
	I32S ret = TCC353X_RETURN_SUCCESS;

	ret = Tcc353xMailboxRead(_moduleIndex, _diversityIndex,
				 MBPARA_TMCC_RESULT, &mailbox);

	if (ret != TCC353X_RETURN_SUCCESS)
		return ret;

	_tmccInfo->systemId =
	    (I08U) (((mailbox.data_array[0] >> 10) & 0x03));
	_tmccInfo->transParamSwitch =
	    (I08U) (((mailbox.data_array[0] >> 6) & 0x0F));
	_tmccInfo->startFlagEmergencyAlarm =
	    (I08U) (((mailbox.data_array[0] >> 5) & 0x01));

	_tmccInfo->currentInfo.partialReceptionFlag =
	    (I08U) (((mailbox.data_array[0] >> 4) & 0x01));
	_tmccInfo->currentInfo.transParammLayerA =
	    (I16U) (((mailbox.
		      data_array[1] >> 23) & 0x1FF) |
		    ((mailbox.data_array[0] & 0x0F)
		     << 9));
	_tmccInfo->currentInfo.transParammLayerB =
	    (I16U) (((mailbox.data_array[1]) >> 10) & 0x1FFF);
	_tmccInfo->currentInfo.transParammLayerC =
	    (I16U) (((mailbox.
		      data_array[2] >> 29) & 0x07) |
		    ((mailbox.data_array[1] & 0x3FF)
		     << 3));

	_tmccInfo->nextInfo.partialReceptionFlag =
	    (I08U) (((mailbox.data_array[2] >> 28) & 0x01));
	_tmccInfo->nextInfo.transParammLayerA =
	    (I16U) (((mailbox.data_array[2] >> 15) & 0x1FFF));
	_tmccInfo->nextInfo.transParammLayerB =
	    (I16U) (((mailbox.data_array[2] >> 2) & 0x1FFF));
	_tmccInfo->nextInfo.transParammLayerC =
	    (I16U) (((mailbox.
		      data_array[3] >> 21) & 0x7FF) |
		    ((mailbox.data_array[2] & 0x3)
		     << 11));

	_tmccInfo->phaseShiftCorrectionValue =
	    (I08U) (((mailbox.data_array[3] >> 18) & 0x07));

	return ret;
}

I32S Tcc353xCasOpen(I32S _moduleIndex, I32U _casRound, I08U * _systemKey)
{
	I08U i;
	I32U currentOpFilterConfig = 0x00;
	I32U systemKey[8];

	Tcc353xMiscRead(_moduleIndex, 0, MISC_OP_REG_CTRL,
			TC3XREG_OP_FILTER_CFG, &currentOpFilterConfig);
	currentOpFilterConfig = currentOpFilterConfig & 0x3FF;
	currentOpFilterConfig |= ((((_casRound << 3) - 1) & 0xFFFF) << 16);
	currentOpFilterConfig |= 0x6C00;
	currentOpFilterConfig |= 0x8000;   

	Tcc353xMiscWrite(_moduleIndex, 0, MISC_OP_REG_CTRL,
			 TC3XREG_OP_FILTER_CFG, currentOpFilterConfig);

	for (i = 0; i < 8; i++) {
		int idx;
		idx = (i << 2);
		systemKey[i] = 0;
		systemKey[i] |= (_systemKey[idx + 3] << 24);
		systemKey[i] |= (_systemKey[idx + 2] << 16);
		systemKey[i] |= (_systemKey[idx + 1] << 8);
		systemKey[i] |= (_systemKey[idx]);
	}
	Tcc353xMiscWriteExIncrease(_moduleIndex, 0,
			   MISC_OP_REG_CTRL, TC3XREG_OP_CAS_SYSTEM_KEY0, 
			   &systemKey[0], 8);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xCasSetPid(I32S _moduleIndex, I32U * _pids, I32U _numberOfPids)
{
#if 1
	return TCC353X_RETURN_SUCCESS;	 
#else
	I32U pid01 = 0x3FFF3FFF;
	I32U pid23 = 0x3FFF3FFF;
	I32U casPid[2];

	if (_numberOfPids == 1) {
		pid01 = 0x3FFF0000 | (_pids[0] & 0x1FFF) | 0x8000;
	} else if (_numberOfPids == 2) {
		pid01 =
		    ((_pids[1] & 0x1FFF) << 16) | (_pids[0] &
						   0x1FFF) | 0x80008000;
	} else if (_numberOfPids == 3) {
		pid01 =
		    ((_pids[1] & 0x1FFF) << 16) | (_pids[0] &
						   0x1FFF) | 0x80008000;
		pid23 = 0x3FFF0000 | (_pids[2] & 0x1FFF) | 0x8000;
	} else if (_numberOfPids >= 4) {
		pid01 =
		    ((_pids[1] & 0x1FFF) << 16) | (_pids[0] &
						   0x1FFF) | 0x80008000;
		pid23 =
		    ((_pids[3] & 0x1FFF) << 16) | (_pids[2] &
						   0x1FFF) | 0x80008000;
	} else {
		pid01 = 0x3FFF3FFF;
		pid23 = 0x3FFF3FFF;
	}

	casPid[0] = pid01;
	casPid[1] = pid23;
	Tcc353xMiscWriteExIncrease(_moduleIndex, 0,
			   MISC_OP_REG_CTRL, TC3XREG_OP_CAS_PID0100, 
			   &casPid[0], 2);
	return TCC353X_RETURN_SUCCESS;
#endif
}

I32S Tcc353xCasSetKeyMulti2(I32S _moduleIndex, I32S _parity,
			    I08U * _key, I32S _keyLength,
			    I08U * _initVector, I32S _initVectorLength)
{
	I32U keyLow;
	I32U keyHigh;
	I32U inputKey[2];

	if (_parity != 0) {
		keyHigh =
		    (_key[3] << 24) | (_key[2] << 16) | (_key[1] <<
							 8) | _key[0];
		keyLow =
		    (_key[7] << 24) | (_key[6] << 16) | (_key[5] <<
							 8) | _key[4];;

		inputKey[0] = keyLow;
		inputKey[1] = keyHigh;
		Tcc353xMiscWriteExIncrease(_moduleIndex, 0, MISC_OP_REG_CTRL, 
				   TC3XREG_OP_CAS_PID0O_EVEN_KEY_L, 
				   &inputKey[0], 2);
	} else {
		keyHigh =
		    (_key[3] << 24) | (_key[2] << 16) | (_key[1] <<
							 8) | _key[0];
		keyLow =
		    (_key[7] << 24) | (_key[6] << 16) | (_key[5] <<
							 8) | _key[4];;

		inputKey[0] = keyLow;
		inputKey[1] = keyHigh;
		Tcc353xMiscWriteExIncrease(_moduleIndex, 0, MISC_OP_REG_CTRL, 
				   TC3XREG_OP_CAS_PID0O_ODD_KEY_L, 
				   &inputKey[0], 2);
	}

	if (_initVectorLength != 0) {
		keyHigh = (_initVector[3] << 24) | (_initVector[2] << 16)
		    | (_initVector[1] << 8) | _initVector[0];
		keyLow = (_initVector[7] << 24) | (_initVector[6] << 16)
		    | (_initVector[5] << 8) | _initVector[4];;

		inputKey[0] = keyLow;
		inputKey[1] = keyHigh;
		Tcc353xMiscWriteExIncrease(_moduleIndex, 0, MISC_OP_REG_CTRL, 
				   TC3XREG_OP_CAS_IV0, 
				   &inputKey[0], 2);
	}

	return TCC353X_RETURN_SUCCESS;
}

static I32U Tcc353xDspRestart(I32S _moduleIndex, I32S _diversityIndex)
{
	TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_moduleIndex]
			   [_diversityIndex]);
	Tcc353xSetRegSysReset(&Tcc353xHandle[_moduleIndex]
			      [_diversityIndex], 
			      TC3XREG_SYS_RESET_DSP, _LOCK_);
	Tcc353xGetAccessMail(&Tcc353xHandle[_moduleIndex][_diversityIndex]);
	TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_moduleIndex]
			     [_diversityIndex]);
	TcpalPrintLog((I08S *) "[TCC353X] SYS_RESET(DSP)\n");
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xGetFifoStatus(I32S _moduleIndex, I32U *_fifoSize)
{
	I08U data[2];
	Tcc353xGetRegFifoAStatus(&Tcc353xHandle[_moduleIndex][0], &data[0]);
	_fifoSize[0] = (((data[0]<<8) | data[1])<<2);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xSetGpioControl(I32S _moduleIndex, I32S _diversityIndex,
				  I32S _gpioNum, I32S _value)
{
	I08U mux = 0;
	I08U gpioDr = 0;
	I08U gpioLr = 0;
	I08U maskValue = 0;
	I08U mask[8] = {
		0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
	};

#ifndef BITSET
#define	BITSET(X, MASK)				( (X) |= (I32U)(MASK) )
#endif
#ifndef BITCLR
#define	BITCLR(X, MASK)				( (X) &= ~((I32U)(MASK)) )
#endif
	
	if(_gpioNum<8)
		mux = 0;
	else if(_gpioNum<16)
		mux = 1;
	else if(_gpioNum<24)
		mux = 2;
	else
		return TCC353X_RETURN_FAIL;

	Tcc353xSetRegIoCfgMux(&Tcc353xHandle[_moduleIndex]
			      [_diversityIndex], mux);

	Tcc353xGetRegGpioDR(&Tcc353xHandle[_moduleIndex][_diversityIndex], 
	    &gpioDr);

	Tcc353xGetRegGpioLR(&Tcc353xHandle[_moduleIndex][_diversityIndex], 
	    &gpioLr);

	maskValue = mask[(_gpioNum-(mux*8))&0x07];

	BITSET(gpioDr, maskValue);

	if(_value==0)
		BITCLR(gpioLr, maskValue);
	else
		BITSET(gpioLr, maskValue);

	Tcc353xSetRegGpioDR(&Tcc353xHandle[_moduleIndex][_diversityIndex], 
	    gpioDr);
	Tcc353xSetRegGpioLR(&Tcc353xHandle[_moduleIndex][_diversityIndex], 
	    gpioLr);
	return TCC353X_RETURN_SUCCESS;
}


I32S Tcc353xUserCommand(I32S _moduleIndex, I32S _diversityIndex,
			I32S _command, void *_param1,
			void *_param2, void *_param3, void *_param4)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	switch (_command) {
	case TCC353X_CMD_DSP_RESET:
		Tcc353xDspRestart(_moduleIndex, _diversityIndex);
		break;

	default:
		ret = TCC353X_RETURN_FAIL_UNKNOWN;
		break;
	}

	return ret;
}

/* Dummy functions */

I32S DummyFunction0(I32S _moduleIndex, I32S _chipAddress,
		    I08U _inputData, I08U * _outData, I32S _size)
{
	TcpalPrintLog((I08S *)
		      "[TCC353X] Access dummy function 0\n");
	return TCC353X_RETURN_SUCCESS;
}

I32S DummyFunction1(I32S _moduleIndex, I32S _chipAddress,
		    I08U _address, I08U * _inputData, I32S _size)
{
	TcpalPrintLog((I08S *)
		      "[TCC353X] Access dummy function 1\n");
	return TCC353X_RETURN_SUCCESS;
}
