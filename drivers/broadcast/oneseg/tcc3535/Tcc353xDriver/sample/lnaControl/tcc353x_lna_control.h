/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_lna_control.h                                   */
/*    Description : sample source for lna control                           */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#ifndef __TCC353X_LNA_CONTROL_H__
#define __TCC353X_LNA_CONTROL_H__

#ifdef __cplusplus
extern    "C"
{
#endif

#include "tcc353x_common.h"
#include "tcc353x_user_defines.h"
#include "tcc353x_monitoring.h"

#if defined (_USE_LNA_CONTROL_)
void Tcc353xApiLnaControlInit(I32S _moduleIndex, I32S _diversityIndex, 
			      I32S _initStatus);
I32S Tcc353xApiLnaControl(I32S _moduleIndex, I32S _diversityIndex, 
			  Tcc353xStatus_t *_st);
#endif

#ifdef __cplusplus
};
#endif

#endif
