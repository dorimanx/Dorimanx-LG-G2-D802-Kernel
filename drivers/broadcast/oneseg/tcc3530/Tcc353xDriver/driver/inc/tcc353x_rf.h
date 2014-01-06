/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_rf.h                                            */
/*    Description : Rf Function                                             */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#ifndef __TCC353X_RF_H__
#define __TCC353X_RF_H__

#include "tcc353x_common.h"

I32S Tcc353xRfInit(I32S _moduleIndex, I32S _diversityIndex);
void Tcc353xRfTune(I32S _moduleIndex, I32S _diversityIndex, I32S _freq_khz,
		   I32S _bw_khz, I32S _oscClk,
		   Tcc353xTuneOptions * _tuneOption);
I32S Tcc353xRfWrite(I32S _moduleIndex, I32S _diversityIndex, I08U addr,
		    I32U data);
I32S Tcc353xRfWriteEx(I32S _moduleIndex, I32S _diversityIndex, I08U *_address,
		    I32U *_data, I32U _size);
I32S Tcc353xRfRead(I32S _moduleIndex, I32S _diversityIndex, I08U addr,
		   I32U * data);
#endif
