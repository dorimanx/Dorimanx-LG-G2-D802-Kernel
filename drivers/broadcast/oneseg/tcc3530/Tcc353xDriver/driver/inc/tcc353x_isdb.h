/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_isdb.h                                          */
/*    Description : isdb Function                                           */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

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
