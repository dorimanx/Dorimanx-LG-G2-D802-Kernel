/****************************************************************************
 *   FileName    : tcc353x_common.h
 *   Description : common values
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

#ifndef __TCC353X_COMMON_H__
#define __TCC353X_COMMON_H__

#ifdef __cplusplus
extern    "C"
{
#endif

#include "tcpal_types.h"

/* Caution : Do not modify this File (Share with Driver Core Library) */

/* max 4diversity and max dual baseband */
#define TCC353X_MAX                 2	/* single, dual */
#define TCC353X_DIVERSITY_MAX       4	/* single, 2~n diversity */

#define GPIO_NUM_RF_SWITCHING_TCC3530		9

/* triple band use tcc3531, tcc3535 */
#define GPIO_NUM_TRIPLE_BAND_RF_SWITCHING1		8
#define GPIO_NUM_TRIPLE_BAND_RF_SWITCHING2		9
#define GPIO_NUM_TRIPLE_BAND_RF_SWITCHING3		10
typedef enum {
	BB_TCC3530 = 0, 	/* 5BD - TMM, UHF 1Segment */
	BB_TCC3531,		/* 8BD - ISDB-T, TMM Full Segment */
	BB_TCC3532,		/* 6BD - UHF 1Segment only */
	BB_TCC3535		/* 5BD - Support FullSeg */
} EnumTcc353xBaseBandName;

typedef enum {
	TCC353X_BOARD_SINGLE = 0,
	TCC353X_BOARD_2DIVERSITY,
	TCC353X_BOARD_3DIVERSITY,
	TCC353X_BOARD_4DIVERSITY,
	TCC353X_BOARD_MAX
} EnumTcc353xBoardType;

typedef enum {
	TCC353X_DIVERSITY_NONE = 0,
	TCC353X_DIVERSITY_MASTER,
	TCC353X_DIVERSITY_MID,
	TCC353X_DIVERSITY_LAST
} EnumTcc353xDiversityPosition;

typedef enum {
	/* tcc3530 : Dual BAND */
	/* tcc3531 : Dual, TRIPLE BAND */
	/* tcc3532 : Dual BAND */
	/* tcc3535 : TRIPLE BAND */
	TCC353X_DUAL_BAND_RF = 0,
	TCC353X_TRIPLE_BAND_RF
} EnumTcc353xRfType;

typedef struct {
	I08U irqMode_0x02;

	I08U irqEn_0x03; 	/*add*/

	I08U initRemap_0x0D; 	/*add*/
	I08U initPC_0x0E; 	/*add*/
	I08U initPC_0x0F; 	/*add*/

	I08U gpioAlt_0x10_07_00;
	I08U gpioAlt_0x10_15_08;
	I08U gpioAlt_0x10_23_16;

	I08U gpioDr_0x11_07_00;
	I08U gpioDr_0x11_15_08;
	I08U gpioDr_0x11_23_16;

	I08U gpioLr_0x12_07_00;
	I08U gpioLr_0x12_15_08;
	I08U gpioLr_0x12_23_16;

	I08U gpioDrv_0x13_07_00;
	I08U gpioDrv_0x13_15_08;
	I08U gpioDrv_0x13_23_16;

	I08U gpioPe_0x14_07_00;
	I08U gpioPe_0x14_15_08;
	I08U gpioPe_0x14_23_16;

	I08U gpioSDrv_0x15_07_00;
	I08U gpioSDrv_0x15_15_08;
	I08U gpioSDrv_0x15_23_16;

	I08U ioMisc_0x16;

	I08U streamDataConfig_0x1B;
	I08U streamDataConfig_0x1C;
	I08U streamDataConfig_0x1D;
	I08U streamDataConfig_0x1E;

	I08U periConfig_0x30;
	I08U periConfig_0x31;
	I08U periConfig_0x32;
	I08U periConfig_0x33;

	I08U bufferConfig_0x4E;
	I08U bufferConfig_0x4F;
	I08U bufferConfig_0x54;
	I08U bufferConfig_0x55;

	I08U bufferConfig_0x50;/*add*/
	I08U bufferConfig_0x51;/*add*/
	I08U bufferConfig_0x52;/*add*/
	I08U bufferConfig_0x53;/*add*/

	I08U bufferConfig_0x58;/*add*/
	I08U bufferConfig_0x59;/*add*/
	I08U bufferConfig_0x5A;/*add*/
	I08U bufferConfig_0x5B;/*add*/

	I08U bufferConfig_0x60;/*add*/
	I08U bufferConfig_0x61;/*add*/
	I08U bufferConfig_0x62;/*add*/
	I08U bufferConfig_0x63;/*add*/

	I08U bufferConfig_0x68;/*add*/
	I08U bufferConfig_0x69;/*add*/
	I08U bufferConfig_0x6A;/*add*/
	I08U bufferConfig_0x6B;/*add*/
} Tcc353xRegisterConfig_t;

typedef struct {
	/* BaseBand Name */
	I08S basebandName;

	/* board type               */
	I08S boardType;

	/* select command interface */
	I08S commandInterface;

	/* select stream interface  */
	I08S streamInterface;

	/* current device address   */
	/* 0xA8    (first)          */
	/* 0xAA    (second)         */
	/* 0xAC    (third)          */
	/* 0xAE    (fourth)         */
	I08U address;

	/* pll option               */
	/* default for isdb-t full seg : 0xA214 */
	/* 0 : use default value in driver src */
	/* others : use this value */
	I16U pll;

	/* osc clk                  */
	I32U oscKhz;

	/* diversity position option */
	I16S diversityPosition;

	/* Interrupt usage option (tccspi-only) */
	I16S useInterrupt;

	/* RF Switching GPIO_N */
	/* 0 : not use, N : use GPIO_N 	*/
	/* example use gpio [23 ,09 , 03, 04] => value is [0x80 02 18]*/
	I32U rfSwitchingGpioN;

	/* RF Switching value of GPIO_N  */
	/* control gpioN - 0 : off, 1: on */
	I32U rfSwitchingVhfLow;
	I32U rfSwitchingVhfHigh;
	I32U rfSwitchingUhf;
	I32U rfSwitchingReserved;

	/* rf type - EnumTcc353xRfType */
	I16S rfType;

	/* register config          */
	Tcc353xRegisterConfig_t *Config;
} Tcc353xOption_t;

typedef enum {
	TCC353X_NOT_USE_SRAMLIKE = 0,	/* can't support yet */
	TCC353X_IF_I2C,
	TCC353X_IF_TCCSPI,
	TCC353X_NOT_USE_SDIO1BIT,	/* can't support yet */
	TCC353X_NOT_USE_SDIO4BIT	/* can't support yet */
} EnumCommandIo;

typedef enum {
	TCC353X_STREAM_IO_MAINIO = 0,
	TCC353X_STREAM_IO_PTS,		/* can't support yet */
	TCC353X_STREAM_IO_STS,
	TCC353X_STREAM_IO_SPIMS,
	TCC353X_STREAM_IO_SPISLV,	/* can't support yet */
	TCC353X_STREAM_IO_HPI_HEADERON,	/* can't support yet */
	TCC353X_STREAM_IO_HPI_HEADEROFF,/* can't support yet */
	TCC353X_STREAM_IO_MAX
} EnumStreamIo;

typedef enum {
	TCC353X_RETURN_FAIL_INVALID_HANDLE = -5,
	TCC353X_RETURN_FAIL_NULL_ACCESS = -4,
	TCC353X_RETURN_FAIL_UNKNOWN = -3,
	TCC353X_RETURN_FAIL_TIMEOUT = -2,
	TCC353X_RETURN_FAIL = -1,
	TCC353X_RETURN_SUCCESS = 0,
	TCC353X_RETURN_FIRST
} EnumReturn;

/* User Command List -------------------------------------------------------*/
typedef enum {
	TCC353X_CMD_NONE = 0,
	TCC353X_CMD_DSP_RESET
} EnumUserCmd;

/* Lock Status -------------------------------------------------------------*/
typedef struct {
	I08U AGC;
	I08U DCEC;
	I08U CTO;
	I08U CFO;
	I08U TMCC;
	I08U EWSFlag;
} IsdbLock_t;

/* Stream format control ----------------------------------------------------*/
typedef struct {
	/* 1:pid filtering enable, others : disable */
	I32U pidFilterEnable;
	/* 1:syncbyte filtering enable, others : disable */
	I32U syncByteFilterEnable;
	/* 1:ts error indicator filtering enable, others : disable */
	I32U tsErrorFilterEnable;
	/* 1:ts error indicator insert enable, others : disable */
	I32U tsErrorInsertEnable;
} Tcc353xStreamFormat_t;

/* PID control -------------------------------------------------------------*/
typedef struct {
	/* 0x00~0x1FFF */
	I32U Pid[32];
	/* number of pid */
	I32U numberOfPid;
} Tcc353xpidTable_t;

/* Mailbox control ---------------------------------------------------------*/
typedef struct {
	I32U cmd;
	I32U word_cnt;
	I32U status;
	I32U data_array[7];
} mailbox_t;

/* Stream ouput Types-------------------------------------------------------*/

#define SRVTYPE_NONE                0x00
#define SRVTYPE_ISDB_TS             0x01

/* TMCC Informations -------------------------------------------------------*/
typedef struct {
	I08U partialReceptionFlag;
	/*
	partialReceptionFlag
	0 : No partial reception
	1 : partial reception available
	*/

	I16U transParammLayerA;
	/*
	transParammLayerA

	[03:00] : Number of Segments
	0 : Reserved
	N : N segment
	14 : Reserved
	15 : unused hierarchical layer
	
	[06:04] : Interleaving Length
	(mode-1/ mode-2 / mode-3)
	0 : 0/0/0
	1 : 4/2/1
	2 : 8/4/2
	3 : 16/8/4
	4 : 32/16/8
	5~6 : Reserved
	7 : unused hierarchical layer

	[09:07] : Convolutional coding rate
	0 : 1/2
	1 : 3/2
	2 : 3/4
	3 : 5/6
	4 : 7/8
	5~6 : Reserved
	7 : unused hierarchical layer

	[12:10] : Carrier modulation scheme
	0 : DQPSK
	1 : QPSK
	2 : 16-QAM
	3 : 64-QAM
	4~6 : Reserved
	7 : unused hierarchical layer
	*/

	I16U transParammLayerB;
	/*
	transParammLayerB
	*/

	I16U transParammLayerC;
	/*
	transParammLayerC
	*/
} transInformation_t;

typedef struct {
	I08U systemId;
	/*
	systemId
	0 : System based on this specification
	1 : System for ISDB-TSB
	*/
	
	I08U transParamSwitch;
	/*
	transParamSwitch
	15 : Normal value
	N : (N+1) frames prior to switching
	*/
	
	I08U startFlagEmergencyAlarm;
	/*
	startFlagEmergencyAlarm
	0 : No startup control
	1 : Startup control available
	*/
	
	transInformation_t currentInfo;
	/*
	Current Information
	*/

	transInformation_t nextInfo;
	/*
	Next Information
	*/
	
	I08U phaseShiftCorrectionValue;
	/*
	phaseShiftCorrectionValue
	Used for ISDB for terrestrial sound broadcasting
	*/
} tmccInfo_t;

/* Tune Options ------------------------------------------------------------*/

typedef enum {
/*
use LOW IF : 
	ISDB-T 1Segment,
	ISDB-TSB 3Segment
	ISDB-TSB 1Segment
use ZERO IF :
	ISDB-T 13Segment , ISDB-TMM 13Segment
*/
	TCC353X_ZERO_IF = 0,
	TCC353X_LOW_IF
} EnumRFType;

typedef enum {
	TCC353X_ISDBT_1_OF_13SEG = 0,
	TCC353X_ISDBT_13SEG,
	TCC353X_ISDBTSB_1SEG,
	TCC353X_ISDBTSB_3SEG,
	TCC353X_ISDBTSB_1_OF_3SEG,
	TCC353X_ISDBTMM
} EnumSegmentType;

typedef enum {
    /* Case A */
    A_1st_1Seg = 0,
    A_2nd_1Seg,
    A_3rd_1Seg,
    A_4th_1Seg,
    A_5th_1Seg,
    A_6th_1Seg,
    A_7th_1Seg,
    A_1st_13Seg,
    A_2nd_13Seg,

    /* Case B */
    B_1st_13Seg,
    B_1st_1Seg,
    B_2nd_1Seg,
    B_3rd_1Seg,
    B_4th_1Seg,
    B_5th_1Seg,
    B_6th_1Seg,
    B_7th_1Seg,
    B_2nd_13Seg,

    /* Case C */
    C_1st_13Seg,
    C_2nd_13Seg,
    C_1st_1Seg,
    C_2nd_1Seg,
    C_3rd_1Seg,
    C_4th_1Seg,
    C_5th_1Seg,
    C_6th_1Seg,
    C_7th_1Seg,

    UserDefine_Tmm1Seg,
    UserDefine_Tmm13Seg
} EnumTmmIdx;

typedef struct {
	/* refer EnumRFType */
	I32S rfIfType;
	/* refer EnumSegmentType */
	I32S segmentType;
	/* ISDB-TMM : refer EnumTmmIdx */
	I32S tmmSet;
	/* ISDB-TSB:Start SubChannel number */
	I32S tsbStartSubChannelNum;

	/* change fifo threshold at cspi only mode 0:don't change*/
	I32U userFifothr;
	/* Bandwidth (MHz) : default 6MHz,  8:8MHz */
	I32U BandwidthMHz;
} Tcc353xTuneOptions;

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
 
#ifdef __cplusplus
	};
#endif

#endif
