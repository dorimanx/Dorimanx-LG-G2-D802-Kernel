/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : bbm.h

	Description : API header file of dmb baseband module

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
	20130422 v1.1.0
	20130613 v1.2.0
	20130704 v1.2.1
	20130710 v1.3.1
	20130716 v1.4.1
	20130806 v1.5.1
*******************************************************************************/

#ifndef __BBM_H__
#define __BBM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "fci_types.h"

#define DRIVER_VER 	"VER 1.5.1"

extern fci_s32 bbm_com_reset(HANDLE handle);
extern fci_s32 bbm_com_probe(HANDLE handle);
extern fci_s32 bbm_com_init(HANDLE handle);
extern fci_s32 bbm_com_deinit(HANDLE handle);
extern fci_s32 bbm_com_read(HANDLE handle, fci_u16 addr, fci_u8 *data);
extern fci_s32 bbm_com_byte_read(HANDLE handle, fci_u16 addr, fci_u8 *data);
extern fci_s32 bbm_com_word_read(HANDLE handle, fci_u16 addr, fci_u16 *data);
extern fci_s32 bbm_com_long_read(HANDLE handle, fci_u16 addr, fci_u32 *data);
extern fci_s32 bbm_com_bulk_read(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 size);
extern fci_s32 bbm_com_data(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u32 size);
extern fci_s32 bbm_com_write(HANDLE handle, fci_u16 addr, fci_u8 data);
extern fci_s32 bbm_com_byte_write(HANDLE handle, fci_u16 addr, fci_u8 data);
extern fci_s32 bbm_com_word_write(HANDLE handle, fci_u16 addr, fci_u16 data);
extern fci_s32 bbm_com_long_write(HANDLE handle, fci_u16 addr, fci_u32 data);
extern fci_s32 bbm_com_bulk_write(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 size);
extern fci_s32 bbm_com_tuner_read(HANDLE handle, fci_u8 addr, fci_u8 addr_len,
				fci_u8 *buffer, fci_u8 len);
extern fci_s32 bbm_com_tuner_write(HANDLE handle, fci_u8 addr, fci_u8 addr_len, fci_u8 *buffer,
				fci_u8 len);
extern fci_s32 bbm_com_tuner_set_freq(HANDLE handle, fci_u32 freq);
extern fci_s32 bbm_com_tuner_select(HANDLE handle, fci_u32 product, fci_u32 band);
extern fci_s32 bbm_com_tuner_get_rssi(HANDLE handle, fci_s32 *rssi);
extern fci_s32 bbm_com_scan_status(HANDLE handle);
extern fci_s32 bbm_com_channel_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id);
extern fci_s32 bbm_com_video_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id,
				fci_u8 cdi_id);
extern fci_s32 bbm_com_audio_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id);
extern fci_s32 bbm_com_data_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id);
extern fci_s32 bbm_com_channel_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id);
extern fci_s32 bbm_com_video_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id,
					fci_u8 cdi_id);
extern fci_s32 bbm_com_audio_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id);
extern fci_s32 bbm_com_data_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id);
extern fci_s32 bbm_com_hostif_select(HANDLE handle, fci_u8 hostif);
extern fci_s32 bbm_com_hostif_deselect(HANDLE handle);
extern fci_s32 bbm_com_fic_callback_register(fci_u32 userdata, fci_s32 (*callback)(
					fci_u32 userdata, fci_u8 *data, fci_s32 length));
extern fci_s32 bbm_com_msc_callback_register(fci_u32 userdata, fci_s32 (*callback)(
					fci_u32 userdata, fci_u8 subch_id, fci_u8 *data,
					fci_s32 length));
extern fci_s32 bbm_com_fic_callback_deregister(HANDLE handle);
extern fci_s32 bbm_com_msc_callback_deregister(HANDLE handle);
extern void bbm_com_isr(HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __BBM_H__ */

