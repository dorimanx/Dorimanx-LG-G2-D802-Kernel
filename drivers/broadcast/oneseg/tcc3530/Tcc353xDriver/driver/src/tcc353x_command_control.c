/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_command_control.c                               */
/*    Description : control command Function                                */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include "tcpal_os.h"
#include "tcc353x_command_control.h"
#include "tcc353x_register_control.h"

#define PHY_BASE_ADDR						(0x80000000)

extern Tcc353xHandle_t Tcc353xHandle[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
extern TcpalSemaphore_t Tcc353xInterfaceSema;

#define SWAP16(x) \
    ((I16U)( \
    (((I16U)(x) & (I16U)0x00ffU) << 8) | \
    (((I16U)(x) & (I16U)0xff00U) >> 8) ))

#define SWAP32(x) \
    ((I32U)( \
    (((I32U)(x) & (I32U)0x000000ffUL) << 24) | \
    (((I32U)(x) & (I32U)0x0000ff00UL) <<  8) | \
    (((I32U)(x) & (I32U)0x00ff0000UL) >>  8) | \
    (((I32U)(x) & (I32U)0xff000000UL) >> 24) ))

/* baseband memory control*/
I32S Tcc353xMemoryRead(I32S _moduleIndex, I32S _diversityIndex,
		       I32U _address, I08U * _data, I32U _size)
{
	I08U inputValue[4];
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];
	if (!(_size > 0))
		return TCC353X_RETURN_FAIL;

	_address |= PHY_BASE_ADDR;
	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	Tcc353xSetRegDmaControl(h,
				TC3XREG_CMDDMA_DMAEN |
				TC3XREG_CMDDMA_READMODE, _UNLOCK_);
	inputValue[0] = (I08U)((_address >> 24) & 0xFF);
	inputValue[1] = (I08U)((_address >> 16) & 0xFF);
	inputValue[2] = (I08U)((_address >> 8) & 0xFF);
	inputValue[3] = (I08U)(_address & 0xFF);
	Tcc353xSetRegDmaSourceAddress(h, &inputValue[0], _UNLOCK_);
	inputValue[0] = (I08U)(((_size >> 2) >> 8) & 0xFF);
	inputValue[1] = (I08U)((_size >> 2) & 0xFF);
	Tcc353xSetRegDmaSize(h, &inputValue[0], _UNLOCK_);
	Tcc353xSetRegDmaStartControl(h,
				     TC3XREG_CMDDMA_START_AUTOCLR |
				     TC3XREG_CMDDMA_INIT_AUTOCLR |
				     TC3XREG_CMDDMA_CRC32INIT_AUTOCLR,
				     _UNLOCK_);
	Tcc353xGetRegDataWindow(h, _data, _size, _UNLOCK_);
	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xMemoryWrite(I32S _moduleIndex, I32S _diversityIndex,
			I32U _address, I08U * _data, I32U _size)
{
	I08U inputValue[4];
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];
	if (!(_size > 0))
		return TCC353X_RETURN_FAIL;

	_address |= PHY_BASE_ADDR;
	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	Tcc353xSetRegDmaControl(h,
				TC3XREG_CMDDMA_DMAEN |
				TC3XREG_CMDDMA_WRITEMODE, _UNLOCK_);
	inputValue[0] = (I08U)((_address >> 24) & 0xFF);
	inputValue[1] = (I08U)((_address >> 16) & 0xFF);
	inputValue[2] = (I08U)((_address >> 8) & 0xFF);
	inputValue[3] = (I08U)(_address & 0xFF);
	Tcc353xSetRegDmaSourceAddress(h, &inputValue[0], _UNLOCK_);
	inputValue[0] = (I08U)(((_size >> 2) >> 8) & 0xFF);
	inputValue[1] = (I08U)((_size >> 2) & 0xFF);
	Tcc353xSetRegDmaSize(h, &inputValue[0], _UNLOCK_);
	Tcc353xSetRegDmaStartControl(h,
				     TC3XREG_CMDDMA_START_AUTOCLR |
				     TC3XREG_CMDDMA_INIT_AUTOCLR |
				     TC3XREG_CMDDMA_CRC32INIT_AUTOCLR,
				     _UNLOCK_);
	Tcc353xSetRegDataWindow(h, _data, _size, _UNLOCK_);
	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xDspAsmWrite(Tcc353xHandle_t * _handle, I08U * _data,
			I32S _size)
{
	I08U inputValue[4];
	I32U codeMemoryAddress = TCC353X_CODEMEMBASE;

	if (_size <= 0)
		return TCC353X_RETURN_FAIL;

	codeMemoryAddress = TCC353X_CODEMEMBASE;

	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	Tcc353xSetRegDmaControl(_handle,
				TC3XREG_CMDDMA_DMAEN |
				TC3XREG_CMDDMA_WRITEMODE |
				TC3XREG_CMDDMA_CRC32EN, _UNLOCK_);
	inputValue[0] = (I08U)((codeMemoryAddress >> 24) & 0xFF);
	inputValue[1] = (I08U)((codeMemoryAddress >> 16) & 0xFF);
	inputValue[2] = (I08U)((codeMemoryAddress >> 8) & 0xFF);
	inputValue[3] = (I08U)(codeMemoryAddress & 0xFF);
	Tcc353xSetRegDmaSourceAddress(_handle, &inputValue[0], _UNLOCK_);
	inputValue[0] = (I08U)(((_size >> 2) >> 8) & 0xFF);
	inputValue[1] = (I08U)((_size >> 2) & 0xFF);
	Tcc353xSetRegDmaSize(_handle, &inputValue[0], _UNLOCK_);
	Tcc353xSetRegDmaStartControl(_handle,
				     TC3XREG_CMDDMA_START_AUTOCLR |
				     TC3XREG_CMDDMA_INIT_AUTOCLR |
				     TC3XREG_CMDDMA_CRC32INIT_AUTOCLR,
				     _UNLOCK_);
	Tcc353xSetRegDataWindow(_handle, _data, _size, _UNLOCK_);
	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}

I32S ReadProcess(Tcc353xHandle_t * _handle, I08U _registerAddr, I32S _size,
		 I08U * _outData, I08U _unlock)
{
	if (_unlock == _LOCK_)
		TcpalSemaphoreLock(&Tcc353xInterfaceSema);

	_handle->Read(_handle->moduleIndex, _handle->currentAddress,
		      _registerAddr, _outData, _size);

	if (_unlock == _LOCK_)
		TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);

	return TCC353X_RETURN_SUCCESS;
}

I32S WriteProcess(Tcc353xHandle_t * _handle, I08U _registerAddr,
		  I08U * _inputData, I32S _size, I08U _unlock)
{
	if (_unlock == _LOCK_)
		TcpalSemaphoreLock(&Tcc353xInterfaceSema);

	_handle->Write(_handle->moduleIndex, _handle->currentAddress,
		       _registerAddr, _inputData, _size);

	if (_unlock == _LOCK_)
		TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);

	return TCC353X_RETURN_SUCCESS;
}

/* communication with API Layer !!!! */

/* baseband register*/
I32S Tcc353xRead(I32S _moduleIndex, I32S _diversityIndex, I08U _address,
		 I08U * _data, I32U _size)
{
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];
	return (Tcc353xGetRegManual(h, _address, _size, &_data[0]));
}

I32S Tcc353xWrite(I32S _moduleIndex, I32S _diversityIndex, I08U _address,
		  I08U * _data, I32U _size)
{
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];
	return (Tcc353xSetRegManual(h, _address, &_data[0], _size));
}

/* RF register */
I32S Tcc353xRfWrite(I32S _moduleIndex, I32S _diversityIndex, I08U _address,
		    I32U _data)
{
	return (Tcc353xMiscWrite
		(_moduleIndex, _diversityIndex, MISC_RF_REG_CTRL, _address,
		 _data));
}

I32S Tcc353xRfWriteEx(I32S _moduleIndex, I32S _diversityIndex, I08U *_address,
		    I32U *_data, I32U _size)
{
	return (Tcc353xMiscWriteEx
		(_moduleIndex, _diversityIndex, MISC_RF_REG_CTRL, _address,
		 _data, _size));
}

I32S Tcc353xRfRead(I32S _moduleIndex, I32S _diversityIndex, I08U _address,
		   I32U * _data)
{
	I32S ret;
	ret =
	    Tcc353xMiscRead(_moduleIndex, _diversityIndex,
			    MISC_RF_REG_CTRL, _address, _data);
	return ret;
}

/* Misc Control */
I32S Tcc353xMiscRead(I32S _moduleIndex, I32S _diversityIndex,
		     I08U _miscConfig, I08U _address, I32U * _data)
{
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];

	TcpalSemaphoreLock(&Tcc353xInterfaceSema);

	Tcc353xSetRegMiscConfig(h, _miscConfig, _UNLOCK_);
	Tcc353xSetRegMiscAddress(h, _address, _UNLOCK_);
	Tcc353xSetRegMiscAction(h, TC3XREG_MISC_ACTION, _UNLOCK_);
	Tcc353xGetRegMiscData(h, (I08U *) (_data), _UNLOCK_);
	_data[0] = SWAP32(_data[0]);
	/* warning : please reset to opcontrol miscconfig0 */
	Tcc353xSetRegMiscConfig(h, 0, _UNLOCK_);

	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xMiscWrite(I32S _moduleIndex, I32S _diversityIndex,
		      I08U _miscConfig, I08U _address, I32U _data)
{
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];

	TcpalSemaphoreLock(&Tcc353xInterfaceSema);

	Tcc353xSetRegMiscConfig(h, _miscConfig | 1, _UNLOCK_);
	Tcc353xSetRegMiscAddress(h, _address, _UNLOCK_);
	_data = SWAP32(_data);
	Tcc353xSetRegMiscData(h, (I08U *) (&_data), _UNLOCK_);
	Tcc353xSetRegMiscAction(h, TC3XREG_MISC_ACTION, _UNLOCK_);
	/* warning : please reset to opcontrol miscconfig0 */
	Tcc353xSetRegMiscConfig(h, 0, _UNLOCK_);

	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}

/* misc wite - increase address */
I32S Tcc353xMiscWriteExIncrease(I32S _moduleIndex, I32S _diversityIndex,
		      I08U _miscConfig, I08U _startAddress, 
		      I32U *_data, I32U _size)
{
	Tcc353xHandle_t *h;
	I32U i;
	I32U temp;
	
	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];

	TcpalSemaphoreLock(&Tcc353xInterfaceSema);

	Tcc353xSetRegMiscConfig(h, _miscConfig | 1, _UNLOCK_);

	for (i = 0; i< _size; i++)	{
		Tcc353xSetRegMiscAddress(h, (I08U)(_startAddress+i), _UNLOCK_);
		temp = _data[i];
		temp = SWAP32(temp);
		Tcc353xSetRegMiscData(h, (I08U *) (&temp), _UNLOCK_);
		Tcc353xSetRegMiscAction(h, TC3XREG_MISC_ACTION, _UNLOCK_);
	}
	/* warning : please reset to opcontrol miscconfig0 */
	Tcc353xSetRegMiscConfig(h, 0, _UNLOCK_);

	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}

/* misc wite - not increase address */
I32S Tcc353xMiscWriteEx(I32S _moduleIndex, I32S _diversityIndex,
		      I08U _miscConfig, I08U *_startAddress, 
		      I32U *_data, I32U _size)
{
	Tcc353xHandle_t *h;
	I32U i;
	I32U temp;
	
	if (Tcc353xHandle[_moduleIndex][_diversityIndex].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][_diversityIndex];

	TcpalSemaphoreLock(&Tcc353xInterfaceSema);

	Tcc353xSetRegMiscConfig(h, _miscConfig | 1, _UNLOCK_);

	for (i = 0; i< _size; i++)	{
		Tcc353xSetRegMiscAddress(h, _startAddress[i], _UNLOCK_);
		temp = _data[i];
		temp = SWAP32(temp);
		Tcc353xSetRegMiscData(h, (I08U *) (&temp), _UNLOCK_);
		Tcc353xSetRegMiscAction(h, TC3XREG_MISC_ACTION, _UNLOCK_);
	}
	/* warning : please reset to opcontrol miscconfig0 */
	Tcc353xSetRegMiscConfig(h, 0, _UNLOCK_);

	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}

/* stream read control */
I32S Tcc353xStreamRead(I32S _moduleIndex, I08U * _data, I32S _size)
{
	I32U address;
	I08U inputValue[4];
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][0];
	if (!(_size > 0))
		return TCC353X_RETURN_FAIL;

	address = 0x3F0A0 | PHY_BASE_ADDR;
	TcpalSemaphoreLock(&Tcc353xInterfaceSema);
	Tcc353xSetRegDmaControl(h,
				TC3XREG_CMDDMA_DMAEN |
				TC3XREG_CMDDMA_ADDRFIX |
				TC3XREG_CMDDMA_READMODE, _UNLOCK_);
	inputValue[0] = (I08U)((address >> 24) & 0xFF);
	inputValue[1] = (I08U)((address >> 16) & 0xFF);
	inputValue[2] = (I08U)((address >> 8) & 0xFF);
	inputValue[3] = (I08U)(address & 0xFF);
	Tcc353xSetRegDmaSourceAddress(h, &inputValue[0], _UNLOCK_);
	inputValue[0] = (I08U)(((_size >> 2) >> 8) & 0xFF);
	inputValue[1] = (I08U)((_size >> 2) & 0xFF);
	Tcc353xSetRegDmaSize(h, &inputValue[0], _UNLOCK_);
	Tcc353xSetRegDmaStartControl(h,
				     TC3XREG_CMDDMA_START_AUTOCLR |
				     TC3XREG_CMDDMA_INIT_AUTOCLR |
				     TC3XREG_CMDDMA_CRC32INIT_AUTOCLR,
				     _UNLOCK_);
	Tcc353xGetRegDataWindow(h, _data, _size, _UNLOCK_);
	TcpalSemaphoreUnLock(&Tcc353xInterfaceSema);
	return TCC353X_RETURN_SUCCESS;
}


/* IRQ releate register*/
I32S Tcc353xReadIrqError(I32S _moduleIndex, I08U * _data)
{
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][0];
	return (Tcc353xGetRegIrqError(h, _data));
}

I32S Tcc353xReadIrqStatus(I32S _moduleIndex, I08U * _data)
{
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][0];
	return (Tcc353xGetRegIrqStatus(h, _data));
}

I32S Tcc353xIrqClear(I32S _moduleIndex, I08U _data)
{
	Tcc353xHandle_t *h;

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][0];
	return (Tcc353xSetRegIrqClear(h,_data));
}

I32S Tcc353xWriteIrqErrorClear(I32S _moduleIndex)
{
	Tcc353xHandle_t *h;	

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	h = &Tcc353xHandle[_moduleIndex][0];
	return (Tcc353xSetRegIrqErrorClear
	(h, 
	TC3XREG_IRQ_STAT_DATAINT | TC3XREG_IRQ_STAT_FIFOAINIT));
}
