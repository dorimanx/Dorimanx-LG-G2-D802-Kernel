/****************************************************************************
 *   FileName    : tcc353x_isdb.h
 *   Description : isdb Function
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

#ifndef __TCC353X_ISDB_H__
#define __TCC353X_ISDB_H__

#include "tcc353x_defines.h"

void Tcc353xInitIsdbProcess(Tcc353xHandle_t * _handle);
I32S Tcc353xGetpidTable(I32S _moduleIndex,
			       Tcc353xpidTable_t * _pidTableControl);
I32S Tcc353xEnablePidFiltering(I32S _moduleIndex, I32U _flag);
I32S Tcc353xRemovePidsFiltering (I32S _moduleIndex, Tcc353xpidTable_t *
				      _pidTableControl);
I32S Tcc353xAddPidsFiltering (I32S _moduleIndex, Tcc353xpidTable_t *
			      _pidTableControl);

#endif
