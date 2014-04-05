/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : tuner.c

	Description : tuner control driver source file

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
#include "../inc/fci_hpi.h"
#include "../inc/fci_hal.h"
#include "../inc/fc8080_bb.h"
#include "../inc/fc8080_tun.h"
#include "../inc/fc8080_regs.h"

#if (FC8080_FREQ_XTAL == 16000)
#define CLOCK_RATIO 34359738 /* 33554432 * ((float) 1.024) */
#elif (FC8080_FREQ_XTAL == 16384)
#define CLOCK_RATIO 33554432 /* 33554432 * ((float) 1.) */
#elif (FC8080_FREQ_XTAL == 19200)
#define CLOCK_RATIO 28631996 /* 33554432 * ((float) 0.8533) */
#elif (FC8080_FREQ_XTAL == 24000)
#define CLOCK_RATIO 22347251 /* 33554432 * ((float) 0.666) */
#elif (FC8080_FREQ_XTAL == 24576)
#define CLOCK_RATIO 33554432 /* 33554432 * ((float) 1.) */
#elif (FC8080_FREQ_XTAL == 26000)
#define CLOCK_RATIO 31715649 /* 33554432 * ((float) 0.9452) */
#elif (FC8080_FREQ_XTAL == 27000)
#define CLOCK_RATIO 30541244 /* 33554432 * ((float) 0.9102) */
#elif (FC8080_FREQ_XTAL == 27120)
#define CLOCK_RATIO 30403670 /* 33554432 * ((float) 0.9061) */
#elif (FC8080_FREQ_XTAL == 32000)
#define CLOCK_RATIO 22347251 /* 33554432 * ((float) 0.666) */
#elif (FC8080_FREQ_XTAL == 38400)
#define CLOCK_RATIO 28631996 /* 33554432 * ((float) 0.8533) */
#endif

struct tuner_i2c_driver {
	fci_s32 (*init)(HANDLE handle, fci_s32 speed, fci_s32 slaveaddr);
	fci_s32 (*read)(HANDLE handle, fci_u8 chip, fci_u8 addr , fci_u8 addr_len, fci_u8 *data,
			fci_u8 len);
	fci_s32 (*write)(HANDLE handle, fci_u8 chip, fci_u8 addr , fci_u8 addr_len, fci_u8 *data,
			fci_u8 len);
	fci_s32 (*deinit)(HANDLE handle);
};

struct tuner_driver {
	fci_s32 (*init)(HANDLE handle, enum band_type band);
	fci_s32 (*set_freq)(HANDLE handle, enum band_type band, fci_u32 freq);
	fci_s32 (*get_rssi)(HANDLE handle, fci_s32 *rssi);
	fci_s32 (*deinit)(HANDLE handle);
};

static struct tuner_i2c_driver fcihpi = {
	&fci_hpi_init,
	&fci_hpi_read,
	&fci_hpi_write,
	&fci_hpi_deinit
};

static struct tuner_driver fc8080_tuner = {
	&fc8080_tuner_init,
	&fc8080_set_freq,
	&fc8080_get_rssi,
	&fc8080_tuner_deinit
};

static struct tuner_i2c_driver *tuner_ctrl = &fcihpi;
static struct tuner_driver *tuner = &fc8080_tuner;
static enum band_type tuner_band = BAND3_TYPE;
fci_u8 tuner_addr = 0x00;

fci_s32 tuner_ctrl_select(HANDLE handle, enum i2c_type type)
{
	switch (type) {
	case FCI_HPI_TYPE:
		tuner_ctrl = &fcihpi;
		break;
	default:
		return BBM_E_TN_CTRL_SELECT;
	}

	if (tuner_ctrl->init(handle, 400, 0))
		return BBM_E_TN_CTRL_INIT;

	return BBM_OK;
}

fci_s32 tuner_i2c_read(HANDLE handle, fci_u8 addr, fci_u8 addr_len, fci_u8 *data, fci_u8 len)
{
	if (tuner_ctrl->read(handle, tuner_addr, addr, addr_len, data, len))
		return BBM_E_TN_READ;

	return BBM_OK;
}

fci_s32 tuner_i2c_write(HANDLE handle, fci_u8 addr, fci_u8 addr_len, fci_u8 *data, fci_u8 len)
{
	if (tuner_ctrl->write(handle, tuner_addr, addr, addr_len, data, len))
		return BBM_E_TN_WRITE;

	return BBM_OK;
}

fci_s32 tuner_set_freq(HANDLE handle, fci_u32 freq)
{
	fci_s32 res = BBM_NOK;
	fci_u8 tmp;
#ifdef FC8080_I2C
	fci_u16 buf_en = 0;
#endif

	if (tuner == NULL)
		return BBM_NOK;

#ifdef FC8080_I2C
	bbm_word_read(handle, BBM_BUF_ENABLE, &buf_en);
	bbm_word_write(handle, BBM_BUF_ENABLE, buf_en & 0x0ff);
#endif

	res = tuner->set_freq(handle, tuner_band, freq);
	tmp = (fci_u8) (CLOCK_RATIO / freq);
	bbm_write(handle, BBM_INV_CARRIER_FREQ, tmp);

	fc8080_reset(handle);

#ifdef FC8080_I2C
	bbm_word_write(handle, BBM_BUF_ENABLE, buf_en);
#endif

	if (res != BBM_OK)
		return BBM_NOK;

	return res;
}

fci_s32 tuner_select(HANDLE handle, enum product_type product, enum band_type band)
{
	switch (product) {
	case FC8080_TUNER:
		tuner = &fc8080_tuner;
		tuner_band = (enum band_type) band;
		tuner_addr = 0x00;

		tuner_ctrl_select(handle, FCI_HPI_TYPE);

		bbm_write(handle, BBM_QDD_COMMAND, 0xc4);
		break;
	default:
		return BBM_E_TN_SELECT;
	}

	if (tuner->init == NULL)
		return BBM_E_TN_SELECT;

	if (tuner->init(handle, tuner_band))
		return BBM_E_TN_INIT;

	return BBM_OK;
}

fci_s32 tuner_get_rssi(HANDLE handle, fci_s32 *rssi)
{
	if (tuner->get_rssi(handle, rssi))
		return BBM_E_TN_RSSI;

	return BBM_OK;
}

