/****************************************************************************
 *   FileName    : tcc353x_dpll_19200osc.h
 *   Description : dpll table for 19200 osc
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

#ifndef __TCC353X_DPLL_19200_H__
#define __TCC353X_DPLL_19200_H__

#include "tcc353x_defines.h"

/* PLL SET for OSC 19200 */
#define OSC_192_PLL_ISDB_T_FULLSEG			0xAA86	/* 103.2MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_2A			0xAA86	/* 103.2MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_2B			0xAB86	/* 105.6MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_2C			0xAC86	/* 108.0MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_2D			0xAD86	/* 110.4MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_2E			0xAE86	/* 112.8MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_2F			0xAF86	/* 115.2MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_30			0xB086	/* 117.6MHz */
#define OSC_192_PLL_ISDB_T_FULLSEG_31			0xB186	/* 120.0MHz */

#define OSC_192_PLL_ISDB_T_PARTIAL_1_SEG		0x8F06	/*  38.4MHz */
#define OSC_192_PLL_ISDB_T_PARTIAL_1_SEG_384		0x8F06	/*  38.4MHz */
#define OSC_192_PLL_ISDB_T_PARTIAL_1_SEG_408		0x9006	/*  40.8MHz */
#define OSC_192_PLL_ISDB_T_PARTIAL_1_SEG_432		0x9106	/*  43.2MHz */
#define OSC_192_PLL_ISDB_T_PARTIAL_1_SEG_456		0x9206	/*  45.6MHz */
#define OSC_192_PLL_ISDB_T_PARTIAL_1_SEG_504		0x9406	/*  50.4MHz */
#define OSC_192_PLL_ISDB_T_PARTIAL_1_SEG_528		0x9506	/*  52.8MHz */

#define OSC_192_PLL_ISDB_TMM_FULLSEG			0x9C06	/*  69.6MHz */
#define OSC_192_PLL_ISDB_TMM_FULLSEG_1C			0x9C06	/*  69.6MHz */
#define OSC_192_PLL_ISDB_TMM_FULLSEG_1D			0x9D06	/*  72.0MHz */

#define OSC_192_PLL_ISDB_TMM_PARTIAL_1_SEG		0x8F06	/*  38.4MHz */
#define OSC_192_PLL_ISDB_TMM_PARTIAL_1_SEG_384		0x8F06	/*  38.4MHz */
#define OSC_192_PLL_ISDB_TMM_PARTIAL_1_SEG_456		0x9206	/*  45.6MHz */

#define OSC_192_PLL_ISDB_TSB				0x8F06	/*  38.4MHz */

/* MAX FREQUENCY NUMBER */
#define _OSC_19200_MAX_TMM_1SEG_FREQ_NUM_		22
#define _OSC_19200_MAX_TMM_13SEG_FREQ_NUM_		5
#define _OSC_19200_MAX_TMM_USER_1SEG_FREQ_NUM_		34
#define _OSC_19200_MAX_TMM_USER_13SEG_FREQ_NUM_		34
#define _OSC_19200_MAX_PARTIAL_1SEG_FREQ_NUM_		88
#define _OSC_19200_MAX_FULLSEG_FREQ_NUM_		71

#endif
