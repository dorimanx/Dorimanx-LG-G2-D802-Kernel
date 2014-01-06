/****************************************************************************
 *   FileName    : tcc353x_dpll_38400osc.h
 *   Description : dpll table for 38400 osc
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

#ifndef __TCC353X_DPLL_H__
#define __TCC353X_DPLL_H__

#include "tcc353x_defines.h"

/* PLL SET for OSC 38400 */
#define PLL_ISDB_T_FULLSEG				0xAA8E	/* 103.2MHz */
#define PLL_ISDB_T_FULLSEG_31				0xB18E	/* 120.0MHz */
#define PLL_ISDB_T_FULLSEG_30				0xB08E	/* 117.6MHz */
#define PLL_ISDB_T_FULLSEG_2F				0xAF8E	/* 115.2MHz */
#define PLL_ISDB_T_FULLSEG_2E				0xAE8E	/* 112.8MHz */
#define PLL_ISDB_T_FULLSEG_2D				0xAD8E	/* 110.4MHz */
#define PLL_ISDB_T_FULLSEG_2C				0xAC8E	/* 108.0MHz */
#define PLL_ISDB_T_FULLSEG_2B				0xAB8E	/* 105.6MHz */
#define PLL_ISDB_T_FULLSEG_2A				0xAA8E	/* 103.2MHz */

#define PLL_ISDB_T_PARTIAL_1_SEG			0x8f0E	/*  38.4MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_384			0x8f0E	/*  38.4MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_408			0x900E	/*  40.8MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_416			0x9916	/*  41.6MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_432			0x910E	/*  43.2MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_456			0x920E	/*  45.6MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_464			0x9c16	/*  46.4MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_496			0x9e16	/*  49.6MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_512			0x9f16	/*  51.2MHz */
#define PLL_ISDB_T_PARTIAL_1_SEG_528			0xa016	/*  52.8MHz */

#define PLL_ISDB_TMM_FULLSEG				0xAA16	/*  68.8MHz */

#define PLL_ISDB_TMM_PARTIAL_1_SEG			0x8f0E	/*  38.4MHz */
#define PLL_ISDB_TMM_PARTIAL_1_SEG_384			0x8f0E	/*  38.4MHz */
#define PLL_ISDB_TMM_PARTIAL_1_SEG_456			0x920E	/*  45.6MHz */

#define PLL_ISDB_TSB					0x8F0E  /*  38.4MHz */
#define PLL_ISDB_TSB_1_SEG_384				0x8F0E  /*  38.4MHz */
#define PLL_ISDB_TSB_1_SEG_408				0x900E  /*  40.8MHz */

#define PLL_ISDB_TSB_3_SEG_432				0x900E  /*  40.8MHz */
#define PLL_ISDB_TSB_3_SEG_464				0x9C16  /*  46.4MHz */
#define PLL_ISDB_TSB_3_SEG_456				0x920E  /*  45.6MHz */

/* MAX FREQUENCY NUMBER */
#define _MAX_TSB_1SEG_FREQ_NUM_				12
#define _MAX_TSB_3SEG_FREQ_NUM_				12
#define _MAX_TMM_1SEG_FREQ_NUM_				22
#define _MAX_TMM_13SEG_FREQ_NUM_			5
#define _MAX_TMM_USER_1SEG_FREQ_NUM_			34
#define _MAX_TMM_USER_13SEG_FREQ_NUM_			34
#define _MAX_PARTIAL_1SEG_FREQ_NUM_			99
#define _MAX_FULLSEG_FREQ_NUM_				133

#endif
