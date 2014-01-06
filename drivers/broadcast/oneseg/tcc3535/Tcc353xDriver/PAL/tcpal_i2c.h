/****************************************************************************
 *   FileName    : tcpal_i2c.h
 *   Description : OS glue Function
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

#ifndef __TCPAL_I2C_H__
#define __TCPAL_I2C_H__

#include "tcpal_types.h"

#ifdef __cplusplus
extern    "C"
{
#endif

I32S Tcc353xI2cRead(I32S _moduleIndex, I32S _chipAddress,
		    I08U _registerAddr, I08U * _outData, I32S _size);

I32S Tcc353xI2cWrite(I32S _moduleIndex, I32S _chipAddress,
		     I08U _registerAddr, I08U * _inputData, I32S _size);
I32S Tcc353xI2cOpen(I32S _moduleIndex);
I32S Tcc353xI2cClose(I32S _moduleIndex);

#ifdef __cplusplus
};
#endif

#endif
