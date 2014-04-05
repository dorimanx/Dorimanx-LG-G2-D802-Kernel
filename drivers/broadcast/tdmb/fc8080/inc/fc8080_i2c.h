/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_i2c.h

	Description : i2c interface header file

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

#ifndef __FC8080_I2C_H__
#define __FC8080_I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

extern fci_s32 fc8080_i2c_init(HANDLE handle, fci_u16 param1, fci_u16 param2);
extern fci_s32 fc8080_i2c_byteread(HANDLE handle, fci_u16 addr, fci_u8 *data);
extern fci_s32 fc8080_i2c_wordread(HANDLE handle, fci_u16 addr, fci_u16 *data);
extern fci_s32 fc8080_i2c_longread(HANDLE handle, fci_u16 addr, fci_u32 *data);
extern fci_s32 fc8080_i2c_bulkread(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 length);
extern fci_s32 fc8080_i2c_bytewrite(HANDLE handle, fci_u16 addr, fci_u8 data);
extern fci_s32 fc8080_i2c_wordwrite(HANDLE handle, fci_u16 addr, fci_u16 data);
extern fci_s32 fc8080_i2c_longwrite(HANDLE handle, fci_u16 addr, fci_u32 data);
extern fci_s32 fc8080_i2c_bulkwrite(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 length);
extern fci_s32 fc8080_i2c_dataread(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u32 length);
extern fci_s32 fc8080_i2c_deinit(HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __FC8080_I2C_H__ */

