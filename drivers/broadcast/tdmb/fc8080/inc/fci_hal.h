/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_hal.h

	Description : fc8080 host interface header file

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

#ifndef __FCI_HAL_H__
#define __FCI_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

extern fci_s32 bbm_hostif_select(HANDLE handle, fci_u8 hostif);
extern fci_s32 bbm_hostif_deselect(HANDLE handle);
extern fci_s32 bbm_read(HANDLE handle, fci_u16 addr, fci_u8 *data);
extern fci_s32 bbm_byte_read(HANDLE handle, fci_u16 addr, fci_u8 *data);
extern fci_s32 bbm_word_read(HANDLE handle, fci_u16 addr, fci_u16 *data);
extern fci_s32 bbm_long_read(HANDLE handle, fci_u16 addr, fci_u32 *data);
extern fci_s32 bbm_bulk_read(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 length);
extern fci_s32 bbm_write(HANDLE handle, fci_u16 addr, fci_u8 data);
extern fci_s32 bbm_byte_write(HANDLE handle, fci_u16 addr, fci_u8 data);
extern fci_s32 bbm_word_write(HANDLE handle, fci_u16 addr, fci_u16 data);
extern fci_s32 bbm_long_write(HANDLE handle, fci_u16 addr, fci_u32 data);
extern fci_s32 bbm_bulk_write(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 length);
extern fci_s32 bbm_data(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u32 length);

#ifdef __cplusplus
}
#endif

#endif /* __FCI_HAL_H__ */

