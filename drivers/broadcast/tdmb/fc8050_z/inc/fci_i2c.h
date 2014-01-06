/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fci_i2c.h
 
 Description : fci i2c driver header
 
 History : 
 ----------------------------------------------------------------------
 2009/09/11 	jason		initial
*******************************************************************************/

#ifndef __FCI_I2C_H__
#define __FCI_I2C_H__

#include "fci_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int fci_i2c_init(HANDLE hDevice, int speed, int slaveaddr);
extern int fci_i2c_read(HANDLE hDevice, fci_u8 chip, fci_u8 addr, fci_u8 alen, fci_u8* data, fci_u8 len);
extern int fci_i2c_write(HANDLE hDevice, fci_u8 chip, fci_u8 addr, fci_u8 alen, fci_u8* data, fci_u8 len);

#ifdef __cplusplus
}
#endif

#endif		// __FCI_I2C_H__
