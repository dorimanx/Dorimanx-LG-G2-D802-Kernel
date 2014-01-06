/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : tuner.c
 
 Description : tuner driver
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fci_oal.h"
#include "../inc/fci_hal.h"
#include "../inc/fci_tun.h"
#include "../inc/fci_i2c.h"
#include "../inc/fc8050_regs.h"
#include "../inc/fc8050_bb.h"
#include "../inc/fc8050_tun.h"

#define FC8000_TUNER_ADDR	0x56
#define FC2501_TUNER_ADDR	0x60
#define FC2507_TUNER_ADDR	0x60
#define FC2580_TUNER_ADDR	0x56
#define FC2582_TUNER_ADDR	0x56
#define FC8050_TUNER_ADDR	0x5F

static fci_u8 tuner_addr = FC8050_TUNER_ADDR;
static band_type tuner_band = BAND3_TYPE;

typedef struct {
	int		(*init)(HANDLE hDevice, int speed, int slaveaddr);
	int		(*read)(HANDLE hDevice, fci_u8 chip, fci_u8 addr, fci_u8 alen, fci_u8 *data, fci_u8 len);
	int		(*write)(HANDLE hDevice, fci_u8 chip, fci_u8 addr, fci_u8 alen, fci_u8 *data, fci_u8 len);
} I2C_DRV;

static I2C_DRV fcii2c = {
	&fci_i2c_init,
	&fci_i2c_read,
	&fci_i2c_write
};

typedef struct {
	int		(*init)(HANDLE hDevice, band_type band);
	int		(*set_freq)(HANDLE hDevice, band_type band, fci_u32 f_lo);
	int		(*get_rssi)(HANDLE hDevice, int *rssi);
} TUNER_DRV;

static TUNER_DRV fc8050_tuner = {
	&fc8050_tuner_init,
	&fc8050_set_freq,
	&fc8050_get_rssi
};

static I2C_DRV* tuner_ctrl = &fcii2c;
static TUNER_DRV* tuner = &fc8050_tuner;

int tuner_ctrl_select(HANDLE hDevice, i2c_type type)
{
	switch (type) {
		case FCI_I2C_TYPE:
			tuner_ctrl = &fcii2c;
			break;
		default:
			return BBM_E_TN_CTRL_SELECT;
	}

	if(tuner_ctrl->init(hDevice, 400, 0))
		return BBM_E_TN_CTRL_INIT;
	return BBM_OK;
}

int tuner_i2c_read(HANDLE hDevice, fci_u8 addr, fci_u8 alen, fci_u8 *data, fci_u8 len)
{
	if(tuner_ctrl->read(hDevice, tuner_addr, addr, alen, data, len))
		return BBM_E_TN_REG_READ;
	return BBM_OK;
}

int tuner_i2c_write(HANDLE hDevice, fci_u8 addr, fci_u8 alen, fci_u8 *data, fci_u8 len)
{
	if(tuner_ctrl->write(hDevice, tuner_addr, addr, alen, data, len))
		return BBM_E_TN_REG_WRITE;
	return BBM_OK;
}

int tuner_type(HANDLE hDevice, fci_u32 *type)
{
	*type = tuner_band;

	return BBM_OK;
}

int tuner_set_freq(HANDLE hDevice, fci_u32 freq)
{
	int res = BBM_NOK;

	// check whether tuner is selected or not
	if(tuner == NULL) {
		PRINTF(hDevice, "TUNER NULL ERROR \r\n");
		return BBM_NOK;
	}

	// set frequency & sw reset
	res = tuner->set_freq(hDevice, tuner_band, freq);
	if(res != BBM_OK) {
		PRINTF(hDevice, "TUNER res ERROR \r\n");
		return BBM_NOK;
	}

	fc8050_reset(hDevice);

	return res;
}


int tuner_select(HANDLE hDevice, fci_u32 product, fci_u32 band)
{
	switch(product) {
		case FC8050_TUNER:
			tuner_ctrl_select(hDevice, FCI_I2C_TYPE);
			tuner = &fc8050_tuner;
			tuner_band = (band_type) band;
			tuner_addr = FC8050_TUNER_ADDR;
			break;
		default:
			return BBM_E_TN_SELECT;
	}

	if(tuner == NULL) {
		PRINTF(hDevice, "[ERROR] Can not supported Tuner(%d,%d)\n\r", product, band);
		return BBM_E_TN_SELECT;
	}

	if(tuner->init(hDevice, tuner_band))
		return BBM_E_TN_INIT;
	return BBM_OK;
}

int tuner_get_rssi(HANDLE hDevice, fci_s32 *rssi)
{
	if(tuner->get_rssi(hDevice, rssi))
		return BBM_E_TN_RSSI;
	return BBM_OK;
}
