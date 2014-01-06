#ifndef __TCPAL_SPI_H__
#define __TCPAL_SPI_H__

#include "tcpal_types.h"

#ifdef __cplusplus
extern    "C"
{
#endif

I32S Tcc353xTccspiRead(I32S _moduleIndex, I32S _chipAddress,
		       I08U _registerAddr, I08U * _outData, I32S _size);

I32S Tcc353xTccspiWrite(I32S _moduleIndex, I32S _chipAddress,
			I08U _registerAddr, I08U * _inputData, I32S _size);

I32S Tcc353xTccspiOpen(I32S _moduleIndex);
I32S Tcc353xTccspiClose(I32S _moduleIndex);

#ifdef __cplusplus
};
#endif

#endif
