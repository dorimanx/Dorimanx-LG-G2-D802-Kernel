/*****************************************************************************
 Copyright(c) 2012 FCI Inc. All Rights Reserved
 
 File name : fci_tun.h
 
 Description : tuner control driver
*******************************************************************************/

#ifndef __FCI_TUN_H__
#define __FCI_TUN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "fci_types.h"

typedef enum {
	FCI_I2C_TYPE        = 0,
	FCI_BYPASS_TYPE     = 1
} i2c_type;

typedef enum {
	ISDBT_1_SEG_TYPE    = 2
} band_type;

typedef enum {
	FC8150_TUNER        = 8150
} product_type;

extern int tuner_ctrl_select(HANDLE hDevice, i2c_type type);
extern int tuner_ctrl_deselect(HANDLE hDevice);
extern int tuner_select(HANDLE hDevice, u32 product, u32 band);
extern int tuner_deselect(HANDLE hDevice);

extern int tuner_i2c_read(HANDLE hDevice, u8 addr, u8 alen, u8* data, u8 len);
extern int tuner_i2c_write(HANDLE hDevice, u8 addr, u8 alen, u8* data, u8 len);
extern int tuner_set_freq(HANDLE hDevice, u32 freq);
extern int tuner_get_rssi(HANDLE hDevice, s32 *rssi);

#ifdef __cplusplus
}
#endif

#endif		// __FCI_TUN_H__

