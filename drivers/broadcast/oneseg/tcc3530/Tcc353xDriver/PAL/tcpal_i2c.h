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

#ifdef __cplusplus
};
#endif

#endif
