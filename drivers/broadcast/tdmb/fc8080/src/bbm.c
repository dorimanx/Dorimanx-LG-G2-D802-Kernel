/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : bbm.c

	Description : API source file of dmb baseband module

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fci_tun.h"
#include "../inc/fc8080_regs.h"
#include "../inc/fc8080_bb.h"
#include "../inc/fci_hal.h"
#include "../inc/fc8080_isr.h"

fci_s32 bbm_com_reset(HANDLE handle)
{
	fci_s32 res;

	res = fc8080_reset(handle);

	return res;
}

fci_s32 bbm_com_probe(HANDLE handle)
{
	fci_s32 res;

	res = fc8080_probe(handle);

	return res;
}

fci_s32 bbm_com_init(HANDLE handle)
{
	fci_s32 res;

	res = fc8080_init(handle);

	return res;
}

fci_s32 bbm_com_deinit(HANDLE handle)
{
	fci_s32 res;

	res = fc8080_deinit(handle);

	return res;
}

fci_s32 bbm_com_read(HANDLE handle, fci_u16 addr, fci_u8 *data)
{
	fci_s32 res;

	res = bbm_read(handle, addr, data);

	return res;
}

fci_s32 bbm_com_byte_read(HANDLE handle, fci_u16 addr, fci_u8 *data)
{
	fci_s32 res;

	res = bbm_byte_read(handle, addr, data);

	return res;
}

fci_s32 bbm_com_word_read(HANDLE handle, fci_u16 addr, fci_u16 *data)
{
	fci_s32 res;

	res = bbm_word_read(handle, addr, data);

	return res;
}

fci_s32 bbm_com_long_read(HANDLE handle, fci_u16 addr, fci_u32 *data)
{
	fci_s32 res;

	res = bbm_long_read(handle, addr, data);

	return res;
}

fci_s32 bbm_com_bulk_read(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 size)
{
	fci_s32 res;

	res = bbm_bulk_read(handle, addr, data, size);

	return res;
}

fci_s32 bbm_com_data(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u32 size)
{
	fci_s32 res;

	res = bbm_data(handle, addr, data, size);

	return res;
}

fci_s32 bbm_com_write(HANDLE handle, fci_u16 addr, fci_u8 data)
{
	fci_s32 res;

	res = bbm_write(handle, addr, data);

	return res;
}

fci_s32 bbm_com_byte_write(HANDLE handle, fci_u16 addr, fci_u8 data)
{
	fci_s32 res;

	res = bbm_byte_write(handle, addr, data);

	return res;
}

fci_s32 bbm_com_word_write(HANDLE handle, fci_u16 addr, fci_u16 data)
{
	fci_s32 res;

	res = bbm_word_write(handle, addr, data);

	return res;
}

fci_s32 bbm_com_long_write(HANDLE handle, fci_u16 addr, fci_u32 data)
{
	fci_s32 res;

	res = bbm_long_write(handle, addr, data);

	return res;
}

fci_s32 bbm_com_bulk_write(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 size)
{
	fci_s32 res;

	res = bbm_bulk_write(handle, addr, data, size);

	return res;
}

fci_s32 bbm_com_tuner_read(HANDLE handle, fci_u8 addr, fci_u8 addr_len, fci_u8 *buffer,
			fci_u8 len)
{
	fci_s32 res;

	res = tuner_i2c_read(handle, addr, addr_len, buffer, len);

	return res;
}

fci_s32 bbm_com_tuner_write(HANDLE handle, fci_u8 addr, fci_u8 addr_len, fci_u8 *buffer, fci_u8 len)
{
	fci_s32 res;

	res = tuner_i2c_write(handle, addr, addr_len, buffer, len);

	return res;
}

fci_s32 bbm_com_tuner_set_freq(HANDLE handle, fci_u32 freq)
{
	fci_s32 res = BBM_OK;

	res = tuner_set_freq(handle, freq);

	return res;
}

fci_s32 bbm_com_tuner_select(HANDLE handle, fci_u32 product, fci_u32 band)
{
	fci_s32 res = BBM_OK;

	res = tuner_select(handle, product, band);

	return res;
}

fci_s32 bbm_com_tuner_get_rssi(HANDLE handle, fci_s32 *rssi)
{
	fci_s32 res = BBM_OK;

	res = tuner_get_rssi(handle, rssi);

	return res;
}

fci_s32 bbm_com_scan_status(HANDLE handle)
{
	fci_s32 res = BBM_OK;

	res = fc8080_scan_status(handle);

	return res;
}

fci_s32 bbm_com_channel_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_s32 res;

	res = fc8080_channel_select(handle, subch_id, buf_id);

	return res;
}

fci_s32 bbm_com_video_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id, fci_u8 cdi_id)
{
	fci_s32 res;

	res = fc8080_video_select(handle, subch_id, buf_id, cdi_id);

	return res;
}

fci_s32 bbm_com_audio_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_s32 res;

	res = fc8080_audio_select(handle, subch_id, buf_id);

	return res;
}

fci_s32 bbm_com_data_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_s32 res;

	res = fc8080_data_select(handle, subch_id, buf_id);

	return res;
}

fci_s32 bbm_com_channel_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_s32 res;

	res = fc8080_channel_deselect(handle, subch_id, buf_id);

	return res;
}

fci_s32 bbm_com_video_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id, fci_u8 cdi_id)
{
	fci_s32 res;

	res = fc8080_video_deselect(handle, subch_id, buf_id, cdi_id);

	return res;
}

fci_s32 bbm_com_audio_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_s32 res;

	res = fc8080_audio_deselect(handle, subch_id, buf_id);

	return res;
}

fci_s32 bbm_com_data_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_s32 res;

	res = fc8080_data_deselect(handle, subch_id, buf_id);

	return res;
}

void bbm_com_isr(HANDLE handle)
{
	fc8080_isr(handle);
}

fci_s32 bbm_com_hostif_select(HANDLE handle, fci_u8 hostif)
{
	fci_s32 res = BBM_NOK;

	res = bbm_hostif_select(handle, hostif);

	return res;
}

fci_s32 bbm_com_hostif_deselect(HANDLE handle)
{
	fci_s32 res = BBM_NOK;

	res = bbm_hostif_deselect(handle);

	return res;
}

fci_s32 bbm_com_fic_callback_register(fci_u32 userdata, fci_s32 (*callback)(fci_u32 userdata,
					fci_u8 *data, fci_s32 length))
{
	fic_user_data = userdata;
	fic_callback = callback;

	return BBM_OK;
}

fci_s32 bbm_com_msc_callback_register(fci_u32 userdata, fci_s32 (*callback)(fci_u32 userdata,
					fci_u8 subch_id, fci_u8 *data, fci_s32 length))
{
	msc_user_data = userdata;
	msc_callback = callback;

	return BBM_OK;
}

fci_s32 bbm_com_fic_callback_deregister(HANDLE handle)
{
	fic_user_data = 0;
	fic_callback = NULL;

	return BBM_OK;
}

fci_s32 bbm_com_msc_callback_deregister(HANDLE handle)
{
	msc_user_data = 0;
	msc_callback = NULL;

	return BBM_OK;
}

