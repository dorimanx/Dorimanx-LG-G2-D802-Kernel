/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_monitoring.h                                     */
/*    Description : sample source for monitoring                            */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#ifndef __TCC353X_MONITORING_H__
#define __TCC353X_MONITORING_H__

#ifdef __cplusplus
extern    "C"
{
#endif

#include "tcc353x_common.h"

#define _READ_OPSTATUS_

#define ISDB_MAX_MOV_AVG     4

typedef struct {
	/* common */
	I32U currentValue;
	I32U avgValue;
	I32U count;
	I32U array[ISDB_MAX_MOV_AVG];
	I32U oldValue;
} Tcc353xStatusUnsignedValueSub_t;

typedef struct {
	/* common */
	I32S currentValue;
	I32S avgValue;
	I32S count;
	I32S array[ISDB_MAX_MOV_AVG];
	I32S oldValue;
} Tcc353xStatusSignedValueSub_t;


typedef struct {
	/* status value */
	/* lock status */
	IsdbLock_t isdbLock;
	/* RSSI */
	Tcc353xStatusSignedValueSub_t rssi;
	/* PCBER */
	Tcc353xStatusUnsignedValueSub_t pcber[3];
	/* MER */
	Tcc353xStatusUnsignedValueSub_t mer[3];
	/* SNR */
	Tcc353xStatusUnsignedValueSub_t snr;
	/* VITERBI BER */
	Tcc353xStatusUnsignedValueSub_t viterbiber[3];
	/* TSPER */
	Tcc353xStatusUnsignedValueSub_t tsper[3];
} Tcc353xStatusValue_t;

typedef enum {
	GI_1_4 = 0,
	GI_1_8,
	GI_1_16,
	GI_1_32,
	GI_UNKNOWN
} EnumGI;

typedef enum {
	MODE1 = 0,
	MODE2,
	MODE3,
	MODE_RESERVED
} EnumMode;

typedef enum {
	MOD_DQPSK = 0,
	MOD_QPSK,
	MOD_16QAM,
	MOD_64QAM
} EnumMOD;

typedef enum {
	CR_1_2 = 0,
	CR_2_3,
	CR_3_4,
	CR_5_6,
	CR_7_8,
	CR_RESERVED0,
	CR_RESERVED1,
	CR_NON_HIERARCHICAL
} EnumCR;

typedef struct {
	/* for opstatus */
	I08U cfoLock;
	I08U dataState;
	I08U gi; 	/* refer EnumGI */
	I08U mode; 	/* refer EnumMode */
	I16U syncStatus;
	I16U ResyncCnt;
	I08U sysId;	/* 0:ISDBT, 1:ISDBTsb, others:Reserved*/
	I08U tmccSwitchCnt;	/* 0:1 frames prior to switching ~0x0e:15frame */
	I08U af;	/* 0:No start control, 1:Startup control available */
	I08U pr;	/* 0:No partial receiption, 1: partial reception available */
	I08U AMod;	/* refer EnumMOD */
	I08U ACr;	/* refer EnumCR */
	I08U AIntLen;	/* interleaving length */
	I08U ASegNo;
	I08U BMod;	/* refer EnumMOD */
	I08U BCr;	/* refer EnumCR */
	I08U BIntLen;
	I08U BSegNo;

	I16U APcber;
	I16U BPcber;
	I32U ARsErrorCnt;
	I32U ARsCnt;
	I32U ARsOverCnt;
	I32U BRsErrorCnt;
	I32U BRsCnt;
	I32U BRsOverCnt;

	I16U oldResyncCnt;
	I16U resynced;
} Tcc353xOpStatus_t;

typedef struct {
	/* received status */
	I32U syncStatus;
	I08U rfLoopGain;
	I08U bbLoopGain;
	I32U snrMer;
	I32U lxMer[3];
	I32U pcber[3];
	I32U rsOverCount[3];
	I32U rsPacketCount[3];
	I64U rsErrorCount[3];
	I32U antennaPercent[3];

	/* status value */
	Tcc353xStatusValue_t status;

	/* calculate status */
	I64U oldRsErrorCount[3];
	I32U oldRsOverCount[3];
	I32U oldRsPacketCount[3];
	I32U packetResynced[3];
	I16U reserved0;

	/* for opstatus */
	Tcc353xOpStatus_t opstat;
} Tcc353xStatus_t;

I32S Tcc353xMonitoringApiInit(I32S _moduleIndex, I32S _diversityIndex);
I32S Tcc353xMonitoringApiAntennaPercentage (I32S _moduleIndex, 
				Tcc353xStatus_t * pISDBStatData,
				I32U _InputSize);
I32S Tcc353xMonitoringApiGetStatus(I32S _moduleIndex, I32S _diversityIndex,
				   Tcc353xStatus_t * pISDBStatData);
I32S Tcc353xMonitoringApiGetDbgStatus(I32S _moduleIndex,
				      I32S _diversityIndex,
				      mailbox_t * _mailbox, I32S _count);


#ifdef __cplusplus
};
#endif

#endif
