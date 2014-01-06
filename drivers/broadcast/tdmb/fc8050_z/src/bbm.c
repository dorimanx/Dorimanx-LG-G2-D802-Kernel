/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : bbm.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fci_tun.h"
#include "../inc/fc8050_regs.h"
#include "../inc/fc8050_bb.h"
#include "../inc/fci_hal.h"
#include "../inc/fc8050_isr.h"

int BBM_RESET(HANDLE hDevice)
{
	int res;

	res = fc8050_reset(hDevice);

	return res;
}

int BBM_PROBE(HANDLE hDevice)
{
	int res;

	res = fc8050_probe(hDevice);

	return res;
}

int BBM_INIT(HANDLE hDevice)
{
	int res;

	res = fc8050_init(hDevice);

	return res;
}

int BBM_DEINIT(HANDLE hDevice)
{
	int res;

	res = fc8050_deinit(hDevice);

	return res;
}

int BBM_READ(HANDLE hDevice, fci_u16 addr, fci_u8 *data)
{
	int res;

	res = bbm_read(hDevice, addr, data);

	return res;
}

int BBM_BYTE_READ(HANDLE hDevice, fci_u16 addr, fci_u8 *data)
{
	int res;

	res = bbm_byte_read(hDevice, addr, data);

	return res;
}

int BBM_WORD_READ(HANDLE hDevice, fci_u16 addr, fci_u16 *data)
{
	int res;

	res = bbm_word_read(hDevice, addr, data);

	return res;
}

int BBM_LONG_READ(HANDLE hDevice, fci_u16 addr, fci_u32 *data)
{
	int res;

	res = bbm_long_read(hDevice, addr, data);

	return res;
}

int BBM_BULK_READ(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 size)
{
	int res;

	res = bbm_bulk_read(hDevice, addr, data, size);

	return res;
}

int BBM_DATA(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 size)
{
	int res;

	res = bbm_data(hDevice, addr, data, size);

	return res;
}

int BBM_WRITE(HANDLE hDevice, fci_u16 addr, fci_u8 data)
{
	int res;

	res = bbm_write(hDevice, addr, data);

	return res;
}

int BBM_BYTE_WRITE(HANDLE hDevice, fci_u16 addr, fci_u8 data)
{
	int res;

	res = bbm_byte_write(hDevice, addr, data);

	return res;
}

int BBM_WORD_WRITE(HANDLE hDevice, fci_u16 addr, fci_u16 data)
{
	int res;

	res = bbm_word_write(hDevice, addr, data);

	return res;
}

int BBM_LONG_WRITE(HANDLE hDevice, fci_u16 addr, fci_u32 data)
{
	int res;

	res = bbm_long_write(hDevice, addr, data);

	return res;
}

int BBM_BULK_WRITE(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 size)
{
	int res;

	res = bbm_bulk_write(hDevice, addr, data, size);

	return res;
}

int BBM_TUNER_READ(HANDLE hDevice, fci_u8 addr, fci_u8 alen, fci_u8 *buffer, fci_u8 len)
{
	int res;

	res = tuner_i2c_read(hDevice, addr, alen, buffer, len);

	return res;
}

int BBM_TUNER_WRITE(HANDLE hDevice, fci_u8 addr, fci_u8 alen, fci_u8 *buffer, fci_u8 len)
{
	int res;

	res = tuner_i2c_write(hDevice, addr, alen, buffer, len);

	return res;
}

int BBM_TUNER_SET_FREQ(HANDLE hDevice, fci_u32 freq)
{
	int res = BBM_OK;

	res = tuner_set_freq(hDevice, freq);

	return res;
}

int BBM_TUNER_SELECT(HANDLE hDevice, fci_u32 product, fci_u32 band)
{
	int res = BBM_OK;

	res = tuner_select(hDevice, product, band);

	return res;
}

int BBM_TUNER_GET_RSSI(HANDLE hDevice, fci_s32 *rssi)
{
	int res = BBM_OK;

	res = tuner_get_rssi(hDevice, rssi);

	return res;
}

int BBM_SCAN_STATUS(HANDLE hDevice)
{
	int res = BBM_OK;

	res = fc8050_scan_status(hDevice);

	return res;
}

int BBM_CHANNEL_SELECT(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId)
{
	int res;

	res = fc8050_channel_select(hDevice, subChId, svcChId);

	return res;
}

int BBM_VIDEO_SELECT(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId, fci_u8 cdiId)
{
	int res;

	res = fc8050_video_select(hDevice, subChId, svcChId, cdiId);

	return res;
}

int BBM_AUDIO_SELECT(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId)
{
	int res;

	res = fc8050_audio_select(hDevice, subChId, svcChId);

	return res;
}

int BBM_DATA_SELECT(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId)
{
	int res;

	res = fc8050_data_select(hDevice, subChId, svcChId);

	return res;
}

int BBM_CHANNEL_DESELECT(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId)
{
	int res;

	res = fc8050_channel_deselect(hDevice, subChId, svcChId);

	return res;
}

int BBM_VIDEO_DESELECT(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId, fci_u8 cdiId)
{
	int res;

	res = fc8050_video_deselect(hDevice, subChId, svcChId, cdiId);

	return res;
}

int BBM_AUDIO_DESELECT(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId)
{
	int res;

	res = fc8050_audio_deselect(hDevice, subChId, svcChId);

	return res;
}

int BBM_DATA_DESELECT(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId)
{
	int res;

	res = fc8050_data_deselect(hDevice, subChId, svcChId);

	return res;
}

void BBM_ISR(HANDLE hDevice)
{
	fc8050_isr(hDevice);
}

int BBM_HOSTIF_SELECT(HANDLE hDevice, fci_u8 hostif)
{
	int res = BBM_NOK;

	res = bbm_hostif_select(hDevice, hostif);

	return res;
}

int BBM_HOSTIF_DESELECT(HANDLE hDevice)
{
	int res = BBM_NOK;

	res = bbm_hostif_deselect(hDevice);

	return res;
}

int BBM_FIC_CALLBACK_REGISTER(fci_u32 userdata, int (*callback)(fci_u32 userdata, fci_u8 *data, int length))
{
	gFicUserData = userdata;
	pFicCallback = callback;

	return BBM_OK;
}

int BBM_MSC_CALLBACK_REGISTER(fci_u32 userdata, int (*callback)(fci_u32 userdata, fci_u8 subChId, fci_u8 *data, int length))
{
	gMscUserData = userdata;
	pMscCallback = callback;

	return BBM_OK;
}

int BBM_FIC_CALLBACK_DEREGISTER(HANDLE hDevice)
{
	gFicUserData = 0;
	pFicCallback = NULL;

	return BBM_OK;
}

int BBM_MSC_CALLBACK_DEREGISTER(HANDLE hDevice)
{
	gMscUserData = 0;
	pMscCallback = NULL;

	return BBM_OK;
}

