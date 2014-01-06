/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_api.c                                           */
/*    Description : API Function                                            */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include "tcc353x_register_control.h"
#include "tcc353x_command_control.h"
#include "tcpal_os.h"

extern I32U Tcc353xGetAccessMail(Tcc353xHandle_t * _handle);
extern TcpalSemaphore_t
    Tcc353xOpMailboxSema[TCC353X_MAX][TCC353X_DIVERSITY_MAX];

I32S Tcc353xSetRegManual(Tcc353xHandle_t * _handle, I08U _addr,
			 I08U * _data, I32S _size)
{
	if (_addr == TC3XREG_SYS_EN) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] Can't control System Register!!\n");
		return TCC353X_RETURN_FAIL;
	}
	return (WriteProcess(_handle, _addr, _data, _size, _LOCK_));
}

I32S Tcc353xGetRegManual(Tcc353xHandle_t * _handle, I08U _addr, I32S _size,
			 I08U * _data)
{
	if (_addr == TC3XREG_STREAM_CFG2 || _addr == TC3XREG_STREAM_CFG1) {
		I08U latch = 0x20;
		if (_handle->options.useInterrupt)
			latch = 0x21;
		else
			latch = 0x20;
		WriteProcess(_handle, TC3XREG_STREAM_CFG3, &latch, 1,
			     _LOCK_);
	}
	return (ReadProcess(_handle, _addr, _size, _data, _LOCK_));
}

/* System Control Register */
I32S Tcc353xSetRegSysEnable(Tcc353xHandle_t * _handle, I08U _value)
{
	I32S ret;
	ret = WriteProcess(_handle, TC3XREG_SYS_EN, &_value, 1, _LOCK_);
	_handle->sysEnValue = _value;
	return ret;
}

I32S Tcc353xSetRegSysReset(Tcc353xHandle_t * _handle, I08U _value, 
			   I08U _unlock)
{
	I32S ret;
	ret = WriteProcess(_handle, TC3XREG_SYS_RESET, &_value, 1, _LOCK_);
	return ret;
}

I32S Tcc353xSetRegIrqMode(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_IRQ_MODE, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegIrqEnable(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess(_handle, TC3XREG_IRQ_EN, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegIrqClear(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_IRQ_STAT_CLR, &_value, 1, _LOCK_));
}

I32S Tcc353xGetRegIrqStatus(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (ReadProcess(_handle, TC3XREG_IRQ_STAT_CLR, 1, _data, _LOCK_));
}

I32S Tcc353xSetRegIrqErrorClear(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess(_handle, TC3XREG_IRQ_ERROR, &_value, 1, _LOCK_));
}

I32S Tcc353xGetRegIrqError(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (ReadProcess(_handle, TC3XREG_IRQ_ERROR, 1, _data, _LOCK_));
}

I32S Tcc353xSetRegPll6(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock)
{
	return (WriteProcess(_handle, TC3XREG_PLL_6, &_value, 1, _unlock));
}

I32S Tcc353xSetRegPll7(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock)
{
	return (WriteProcess(_handle, TC3XREG_PLL_7, &_value, 1, _unlock));
}

I32S Tcc353xSetRegPll8(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock)
{
	return (WriteProcess(_handle, TC3XREG_PLL_8, &_value, 1, _unlock));
}

I32S Tcc353xSetRegPll9(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock)
{
	return (WriteProcess(_handle, TC3XREG_PLL_9, &_value, 1, _unlock));
}

I32S Tcc353xSetRegRemap(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_INIT_REMAP, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegRemapPc(Tcc353xHandle_t * _handle, I08U * _data,
			  I32S _size)
{
	/* TC3XREG_INIT_REMAP/TC3XREG_INIT_PC8/TC3XREG_INIT_PC0 */
	return (WriteProcess
		(_handle, TC3XREG_INIT_REMAP, _data, _size, _LOCK_));
}

I32S Tcc353xGetRegProgramId(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (ReadProcess(_handle, TC3XREG_PROGRAMID, 1, _data, _LOCK_));
}

I32S Tcc353xGetRegChipId(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (ReadProcess(_handle, TC3XREG_CHIPID, 1, _data, _LOCK_));
}

I32S Tcc353xSetRegGpioAlt(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_GPIO_ALT, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegGpioDR(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_GPIO_DR, &_value, 1, _LOCK_));
}

I32S Tcc353xGetRegGpioDR(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (ReadProcess(_handle, TC3XREG_GPIO_DR, 1, _data, _LOCK_));
}

I32S Tcc353xSetRegGpioLR(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_GPIO_LR, &_value, 1, _LOCK_));
}

I32S Tcc353xGetRegGpioLR(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (ReadProcess(_handle, TC3XREG_GPIO_LR, 1, _data, _LOCK_));
}

I32S Tcc353xSetRegGpioDRV(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_GPIO_DRV, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegGpioPE(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_GPIO_PE, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegGpiosDRV(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_GPIOS_DRV, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegIoCfgMux(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_IOCFG_MUX, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegIoMISC(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_IO_MISC, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegStreamConfig0(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_STREAM_CFG0, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegStreamConfig1(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_STREAM_CFG1, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegStreamConfig2(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_STREAM_CFG2, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegStreamConfig3(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_STREAM_CFG3, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegStreamConfig(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_STREAM_CFG0, _data, 4, _LOCK_));
}

I32S Tcc353xGetRegStreamData(Tcc353xHandle_t * _handle, I08U * _data,
			     I32S _size)
{
	return (ReadProcess
		(_handle, TC3XREG_STREAM_CFG4 | Bit7, _size, _data,
		 _LOCK_));
}

I32S Tcc353xSetRegDataWindow(Tcc353xHandle_t * _handle, I08U * _data,
			     I32S _size, I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_CMDDMA_DATA_WIND | Bit7, _data, _size,
		 _unlock));
}

I32S Tcc353xGetRegDataWindow(Tcc353xHandle_t * _handle, I08U * _data,
			     I32S _size, I08U _unlock)
{
	return (ReadProcess
		(_handle, TC3XREG_CMDDMA_DATA_WIND | Bit7, _size, _data,
		 _unlock));
}

/* Command DMA Register */

I32S Tcc353xSetRegDmaControl(Tcc353xHandle_t * _handle, I08U _value,
			     I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_CMDDMA_CTRL, &_value, 1, _unlock));
}

I32S Tcc353xSetRegDmaSourceAddress(Tcc353xHandle_t * _handle, I08U * _data,
				   I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_CMDDMA_SADDR_24, _data, 4, _unlock));
}

I32S Tcc353xSetRegDmaSize(Tcc353xHandle_t * _handle, I08U * _data,
			  I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_CMDDMA_SIZE8, _data, 2, _unlock));
}

I32S Tcc353xSetRegDmaStartControl(Tcc353xHandle_t * _handle, I08U _value,
				  I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_CMDDMA_STARTCTRL, &_value, 1, _unlock));
}

I32S Tcc353xGetRegDmaCrc32(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (ReadProcess
		(_handle, TC3XREG_CMDDMA_CRC24, 4, _data, _LOCK_));
}

/* PERIperal for stream data Register */

I32S Tcc353xSetRegPeripheralConfig0(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_PERI_CTRL, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegPeripheralConfig(Tcc353xHandle_t * _handle, I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_PERI_CTRL, _data, 4, _LOCK_));
}

/* MAILBOX Register */

I32S Tcc353xSetRegMailboxControl(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_MAIL_CTRL, &_value, 1, _LOCK_));
}

I32S Tcc353xGetRegMailboxFifoReadStatus(Tcc353xHandle_t * _handle,
					I08U * _data)
{
	I08U latchData = 0x5E;
	WriteProcess(_handle, TC3XREG_MAIL_FIFO_R, &latchData, 1, _LOCK_);
	return (ReadProcess
		(_handle, TC3XREG_MAIL_FIFO_R, 1, _data, _LOCK_));
}

I32S Tcc353xGetRegMailboxFifoWriteStatus(Tcc353xHandle_t * _handle,
					 I08U * _data)
{
	I08U latchData = 0x5E;

	WriteProcess(_handle, TC3XREG_MAIL_FIFO_W, &latchData, 1, _LOCK_);
	return (ReadProcess
		(_handle, TC3XREG_MAIL_FIFO_W, 1, _data, _LOCK_));
}

I32S Tcc353xSetRegMailboxFifoWindow(Tcc353xHandle_t * _handle,
				    I08U * _data, I32S _size)
{
	return (WriteProcess
		(_handle, TC3XREG_MAIL_FIFO_WIND | Bit7, _data, _size,
		 _LOCK_));
}

I32S Tcc353xGetRegMailboxFifoWindow(Tcc353xHandle_t * _handle,
				    I08U * _data, I32S _size)
{
	return (ReadProcess
		(_handle, TC3XREG_MAIL_FIFO_WIND | Bit7, _size, _data,
		 _LOCK_));
}

/* OUTPUT Buffer Management Register */

I32S Tcc353xSetRegOutBufferConfig(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_CFG, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegOutBufferInit(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_INIT, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegOutBufferStartAddressA(Tcc353xHandle_t * _handle,
					 I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_A_SADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferEndAddressA(Tcc353xHandle_t * _handle,
				       I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_A_EADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferStartAddressB(Tcc353xHandle_t * _handle,
					 I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_B_SADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferEndAddressB(Tcc353xHandle_t * _handle,
				       I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_B_EADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferStartAddressC(Tcc353xHandle_t * _handle,
					 I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_C_SADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferEndAddressC(Tcc353xHandle_t * _handle,
				       I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_C_EADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferStartAddressD(Tcc353xHandle_t * _handle,
					 I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_D_SADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferEndAddressD(Tcc353xHandle_t * _handle,
				       I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_D_EADDR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferAFifoThr(Tcc353xHandle_t * _handle,
				    I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_A_FIFO_THR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferBFifoThr(Tcc353xHandle_t * _handle,
				    I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_B_FIFO_THR0, _data, 2, _LOCK_));
}

I32S Tcc353xSetRegOutBufferDFifoThr(Tcc353xHandle_t * _handle,
				    I08U * _data)
{
	return (WriteProcess
		(_handle, TC3XREG_OBUFF_D_FIFO_THR0, _data, 2, _LOCK_));
}

I32S Tcc353xGetRegFifoAStatus(Tcc353xHandle_t * _handle, I08U * _data)
{
	I08U tmp = 0x10;
	WriteProcess (_handle, TC3XREG_OBUFF_INIT, &tmp, 1, _LOCK_);
	ReadProcess
		(_handle, TC3XREG_OBUFF_A_FIFO_STAT0, 1, &_data[0], _LOCK_);
	return (ReadProcess
		(_handle, TC3XREG_OBUFF_A_FIFO_STAT1, 1, &_data[1], _LOCK_));
}

I32S Tcc353xSetRegLdoConfig(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_OP_LDO_CONFIG, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegXtalBias(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_OP_XTAL_BIAS, &_value, 1, _LOCK_));
}

I32S Tcc353xSetRegXtalBiasKey(Tcc353xHandle_t * _handle, I08U _value)
{
	return (WriteProcess
		(_handle, TC3XREG_OP_XTAL_BIAS_KEY, &_value, 1, _LOCK_));
}

I32S Tcc353xGetRegOPStatus(Tcc353xHandle_t * _handle, I08U * _data, 
			   I32U _dataSize, I08U _unlock)
{
	I08U tmp = 1;
	WriteProcess (_handle, TC3XREG_OP_STATUS0, &tmp, 1, _unlock);
	return (ReadProcess
		(_handle, TC3XREG_OP_STATUS1| Bit7, _dataSize, _data, _unlock));
}
I32S Tcc353xSetRegMiscConfig(Tcc353xHandle_t * _handle, I08U _value,
			     I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_MISC_CFG0, &_value, 1, _unlock));
}

I32S Tcc353xSetRegMiscAction(Tcc353xHandle_t * _handle, I08U _value,
			     I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_MISC_CFG1, &_value, 1, _unlock));
}

I32S Tcc353xSetRegMiscAddress(Tcc353xHandle_t * _handle, I08U _value,
			      I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_MISC_CFG2, &_value, 1, _unlock));
}

I32S Tcc353xSetRegMiscData(Tcc353xHandle_t * _handle, I08U * _data,
			   I08U _unlock)
{
	return (WriteProcess
		(_handle, TC3XREG_MISC_CFG3, _data, 4, _unlock));
}

I32S Tcc353xGetRegMiscData(Tcc353xHandle_t * _handle, I08U * _data,
			   I08U _unlock)
{
	return (ReadProcess
		(_handle, TC3XREG_MISC_CFG3, 4, _data, _unlock));
}

I32S Tcc353xGetRegOpDebug(Tcc353xHandle_t * _handle, I08U * _data,
			  I08U _unlock)
{
	I08U tmp = 0x5e;
	WriteProcess(_handle, TC3XREG_OP_DEBUG0, &tmp, 1, _unlock);
	return (ReadProcess
		(_handle, TC3XREG_OP_DEBUG0, 3, _data, _unlock));
}
