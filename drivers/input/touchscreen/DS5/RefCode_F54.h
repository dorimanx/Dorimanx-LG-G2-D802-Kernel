// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Copyright ?2012 Synaptics Incorporated. All rights reserved.
//
// The information in this file is confidential under the terms
// of a non-disclosure agreement with Synaptics and is provided
// AS IS.
//
// The information in this file shall remain the exclusive property
// of Synaptics and may be the subject of Synaptics?patents, in
// whole or part. Synaptics?intellectual property rights in the
// information in this file are not expressly or implicitly licensed
// or otherwise transferred to you as a result of such information
// being made available to you.
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include <linux/kernel.h>	//printk
#include <linux/delay.h>	//msleep
#include <linux/time.h>		//struct timeval t_interval[TIME_PROFILE_MAX];
#include <linux/string.h>	//memset

#if 0
extern const int DefaultTimeout;

int F54Test(int input);
bool switchPage(int page);
void RunQueries(void);
void DeltaImageReport(void);
void RawImageReport(void);
void SensorSpeed(void);
void ADCRange(void);
void TxTxTest(void);
//int RxRxShortTest(void);
void HighResistanceTest(void);
void MaxMinTest(void);
int ImageTest(void);
void DeltaImageReport(void);

void SCAN_PDT(void);
#endif

extern void write_log(char *data);
extern void Read8BitRegisters(unsigned short regAddr, unsigned char *data, int length);
extern void Write8BitRegisters(unsigned short regAddr, unsigned char *data, int length);

