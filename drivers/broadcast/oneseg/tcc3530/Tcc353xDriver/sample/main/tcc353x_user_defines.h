/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_user_defines.h                                  */
/*    Description : sample user defines                                     */
/*                  this file can modify for some tunings                   */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#ifndef __TCC353X_USER_DEFINES_H__
#define __TCC353X_USER_DEFINES_H__

#define _USE_TMM_CSPI_ONLY_

/* Remap Type, init PC */
#ifndef _USE_TMM_CSPI_ONLY_
/* normal case */
#define TCC353X_REMAP_TYPE 	0x04
#define TCC353X_INIT_PC_H 	0xC0
#define TCC353X_INIT_PC_L 	0x00
#else
/* tmm cspi only case */
#define TCC353X_REMAP_TYPE 	0x03
#define TCC353X_INIT_PC_H 	0xC0
#define TCC353X_INIT_PC_L 	0x00
#endif

/* out buffer config */
#ifndef _USE_TMM_CSPI_ONLY_
/* normal case */
#define TCC353X_BUFF_A_START 	0x00018000
#define TCC353X_BUFF_A_END 	0x00019f93
#define TCC353X_BUFF_B_START 	0x00018000
#define TCC353X_BUFF_B_END 	0x00019f93
#define TCC353X_BUFF_C_START 	0x00018000
#define TCC353X_BUFF_C_END 	0x00019f93
#define TCC353X_BUFF_D_START 	0x00018000
#define TCC353X_BUFF_D_END 	0x00019f93
#else
/* tmm cspi only case */
#define TCC353X_BUFF_A_START 	0x00010000
#define TCC353X_BUFF_A_END 	0x00027F57	/* inside driver fix */
#define TCC353X_BUFF_B_START 	0x00018000
#define TCC353X_BUFF_B_END 	0x00019f93
#define TCC353X_BUFF_C_START 	0x00018000
#define TCC353X_BUFF_C_END 	0x00019f93
#define TCC353X_BUFF_D_START 	0x00018000
#define TCC353X_BUFF_D_END 	0x00019f93
#endif

/* set stream threshold (using interrupt) */
#define TCC353X_STREAM_THRESHOLD_SPISLV	(188*100)
#define TCC353X_STREAM_THRESHOLD_SPISLV_WH	\
	(((TCC353X_STREAM_THRESHOLD_SPISLV>>2)>>8)&0xFF)
#define TCC353X_STREAM_THRESHOLD_SPISLV_WL	\
	((TCC353X_STREAM_THRESHOLD_SPISLV>>2)&0xFF)

/* use large frame buffer for cspi only */

#define TCC353X_USE_STREAM_BUFFERING

#if defined (TCC353X_USE_STREAM_BUFFERING)
/* about 400k : 200ms at 16QAM MAX */
#define TCC353X_STREAM_BUFFER_PKT	(3840)
#define TCC353X_STREAM_BUFFER_SIZE	(188*TCC353X_STREAM_BUFFER_PKT)
#else
#define TCC353X_STREAM_BUFFER_SIZE	(TCC353X_STREAM_THRESHOLD_SPISLV)
#endif

/* set stream threshold (not use interrupt) */
#define TCC353X_STREAM_THRESHOLD	(188*8)
#define TCC353X_STREAM_THRESHOLD_WH	(((TCC353X_STREAM_THRESHOLD>>2)>>8)\
					&0xFF)
#define TCC353X_STREAM_THRESHOLD_WL	((TCC353X_STREAM_THRESHOLD>>2)&0xFF)

/* driving strength for sts (GPIO 04~07) */
/* 0x00 : min , 0xF0 : max */
#define TCC353X_DRV_STR_GPIO_0x13_07_00 	0x00	/* 0xF0 */

/* STS(TSIF) Clk polarity */
#define STS_CLK_POS			0x00
#define STS_CLK_NEG			0x80
#define STS_SYNC_ACT_LOW		0x40
#define STS_SYNC_ACT_HIGH		0x00
#define STS_FRM_ACT_LOW			0x20
#define STS_FRM_ACT_HIGH		0x00

#define STS_POLARITY	(STS_CLK_POS|STS_SYNC_ACT_LOW|STS_FRM_ACT_LOW)
/*
#define STS_POLARITY	(STS_CLK_POS|STS_SYNC_ACT_HIGH|STS_FRM_ACT_HIGH)
*/

/* set stream speed (DLR) */
#define TCC353X_DLR                             1

#endif
