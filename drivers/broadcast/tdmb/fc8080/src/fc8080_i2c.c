/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_i2c.c

	Description : i2c interface source file

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
#include <linux/mutex.h>

#include "../inc/fci_types.h"
#include "../inc/fc8080_regs.h"
#include "../inc/fci_oal.h"

#define CHIP_ADDR       0x58

static DEFINE_MUTEX(lock);

static fci_s32 i2c_bulkread(HANDLE handle, fci_u8 chip, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	return BBM_OK;
}

static fci_s32 i2c_bulkwrite(HANDLE handle, fci_u8 chip, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	return BBM_OK;
}

static fci_s32 i2c_dataread(HANDLE handle, fci_u8 chip, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	return i2c_bulkread(handle, chip, addr, data, length);
}

fci_s32 fc8080_i2c_init(HANDLE handle, fci_u16 param1, fci_u16 param2)
{
	/*ts_initialize();*/

	return BBM_OK;
}

fci_s32 fc8080_i2c_byteread(HANDLE handle, fci_u16 addr, fci_u8 *data)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (fci_u8) CHIP_ADDR, addr, data, 1);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_wordread(HANDLE handle, fci_u16 addr, fci_u16 *data)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (fci_u8) CHIP_ADDR, addr, (fci_u8 *) data, 2);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_longread(HANDLE handle, fci_u16 addr, fci_u32 *data)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (fci_u8) CHIP_ADDR, addr, (fci_u8 *) data, 4);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_bulkread(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (fci_u8) CHIP_ADDR, addr, data, length);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_bytewrite(HANDLE handle, fci_u16 addr, fci_u8 data)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (fci_u8) CHIP_ADDR, addr, (fci_u8 *) &data, 1);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_wordwrite(HANDLE handle, fci_u16 addr, fci_u16 data)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (fci_u8) CHIP_ADDR, addr, (fci_u8 *) &data, 2);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_longwrite(HANDLE handle, fci_u16 addr, fci_u32 data)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (fci_u8) CHIP_ADDR, addr, (fci_u8 *) &data, 4);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_bulkwrite(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (fci_u8) CHIP_ADDR, addr, (fci_u8 *) &data, length);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_dataread(HANDLE handle, fci_u16 addr, fci_u8 *data, fci_u32 length)
{
	fci_s32 res;

	mutex_lock(&lock);
	res = i2c_dataread(handle, (fci_u8) CHIP_ADDR, addr, (fci_u8 *) &data, length);
	mutex_unlock(&lock);

	return res;
}

fci_s32 fc8080_i2c_deinit(HANDLE handle)
{
	/*ts_receiver_disable();*/
	return BBM_OK;
}

