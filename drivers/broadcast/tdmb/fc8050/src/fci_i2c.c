/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fci_i2c.c
 
 Description : fci i2c driver
 
 History : 
 ----------------------------------------------------------------------
 2009/09/11 	jason		initial
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fci_oal.h"
#include "../inc/fc8050_regs.h"
#include "../inc/fci_hal.h"

#define I2CSTAT_TIP 		0x02	/* Tip bit */
#define I2CSTAT_NACK		0x80	/* Nack bit */

#define I2C_TIMEOUT 		1	/* 1 second */

#define I2C_CR_STA		0x80
#define I2C_CR_STO		0x40
#define I2C_CR_RD		0x20
#define I2C_CR_WR		0x10
#define I2C_CR_NACK 		0x08
#define I2C_CR_IACK 		0x01

#define I2C_WRITE		0
#define I2C_READ		1

#define I2C_OK			0
#define I2C_NOK 		1
#define I2C_NACK		2
#define I2C_NOK_LA		3		/* Lost arbitration */
#define I2C_NOK_TOUT		4		/* time out */

static int WaitForXfer (HANDLE hDevice)
{
	int i;
	int res = I2C_OK;
	fci_u8 status;

	i = I2C_TIMEOUT * 10000;
	// wait for transfer complete
	do {
		bbm_read(hDevice, BBM_I2C_SR, &status);
		i--;
	} while ((i > 0) && (status & I2CSTAT_TIP));

	// check time out or nack
	if(status & I2CSTAT_TIP) {
		res = I2C_NOK_TOUT;
	} else {
		bbm_read(hDevice, BBM_I2C_SR, &status);
		if(status & I2CSTAT_NACK) res = I2C_NACK;
		else res = I2C_OK;
	}

	return res;
}

static int fci_i2c_transfer (HANDLE hDevice, fci_u8 cmd_type, fci_u8 chip, fci_u8 addr[], fci_u8 addr_len, fci_u8 data[], fci_u8 data_len)
{
	int i;
	int result = I2C_OK;

	switch (cmd_type) {
		case I2C_WRITE:
			bbm_write(hDevice, BBM_I2C_TXR, chip | cmd_type);
			bbm_write(hDevice, BBM_I2C_CR, I2C_CR_STA | I2C_CR_WR /*0x90*/);
			result = WaitForXfer(hDevice);
			if(result != I2C_OK) return result;

			if (addr && addr_len) {
				i = 0;
				while ((i < addr_len) && (result == I2C_OK)) {
					bbm_write(hDevice, BBM_I2C_TXR, addr[i]);
					bbm_write(hDevice, BBM_I2C_CR, I2C_CR_WR /*0x10*/);
					result = WaitForXfer(hDevice);
					if(result != I2C_OK) return result;
					i++;
				}
			}

			i = 0;
			while ((i < data_len) && (result == I2C_OK)) {
				bbm_write(hDevice, BBM_I2C_TXR, data[i]);
				bbm_write(hDevice, BBM_I2C_CR, I2C_CR_WR /*0x10*/);
				result = WaitForXfer(hDevice);
				if(result != I2C_OK) return result;
				i++;
			}

			bbm_write(hDevice, BBM_I2C_CR, I2C_CR_STO /*0x40*/);
			result = WaitForXfer(hDevice);
			if(result != I2C_OK) return result;
			break;
		case I2C_READ:
			if (addr && addr_len) {
				bbm_write(hDevice, BBM_I2C_TXR, chip | I2C_WRITE);
				bbm_write(hDevice, BBM_I2C_CR, I2C_CR_STA | I2C_CR_WR /*0x90*/); // send start
				result = WaitForXfer(hDevice);
				if(result != I2C_OK) {
					return result;
				}

				i = 0;
				while ((i < addr_len) && (result == I2C_OK)) {
					bbm_write(hDevice, BBM_I2C_TXR, addr[i]);
					bbm_write(hDevice, BBM_I2C_CR, I2C_CR_WR /*0x10*/);
					result = WaitForXfer(hDevice);
					if(result != I2C_OK) {
						return result;
					}
					i++;
				}
			}

			bbm_write(hDevice, BBM_I2C_TXR, chip | I2C_READ);
			bbm_write(hDevice, BBM_I2C_CR, I2C_CR_STA | I2C_CR_WR /*0x90*/); // resend start
			result = WaitForXfer(hDevice);
			if(result != I2C_OK) {
				return result;
			}

			i = 0;
			while ((i < data_len) && (result == I2C_OK)) {
				if (i == data_len - 1) {
					bbm_write(hDevice, BBM_I2C_CR, I2C_CR_RD|I2C_CR_NACK/*0x28*/);	// No Ack Read
					result = WaitForXfer(hDevice);
					if((result != I2C_NACK) && (result != I2C_OK)){
						PRINTF(hDevice, "NACK4-0[%02x]\n\r", result);
						return result;
					}
				} else {
					bbm_write(hDevice, BBM_I2C_CR, I2C_CR_RD /*0x20*/);	// Ack Read
					result = WaitForXfer(hDevice);
					if(result != I2C_OK){
						PRINTF(hDevice, "NACK4-1[%02x]\n\r", result);
						return result;
					}
				}
				bbm_read(hDevice, BBM_I2C_RXR, &data[i]);
				i++;
			}

			bbm_write(hDevice, BBM_I2C_CR, I2C_CR_STO /*0x40*/);		// send stop
			result = WaitForXfer(hDevice);
			if((result != I2C_NACK) && (result != I2C_OK)) {
				PRINTF(hDevice, "NACK5[%02X]\n\r", result);
				return result;
			}
			break;
		default:
			return I2C_NOK;
	}

	return I2C_OK;
}

int fci_i2c_init (HANDLE hDevice, int speed, int slaveaddr)
{
	fci_u16 pr, rpr =0;

	pr = (fci_u16)((4800/speed) -1); 
	//pr=400;
	bbm_word_write(hDevice, BBM_I2C_PR, pr);

	bbm_word_read(hDevice, BBM_I2C_PR, &rpr);
	if(pr != rpr) {
		return BBM_NOK;
	}

	//i2c master core enable & interrupt enable
	bbm_write(hDevice, BBM_I2C_CTR, 0xC0);

	return BBM_OK;
}

int fci_i2c_read(HANDLE hDevice, fci_u8 chip, fci_u8 addr, fci_u8 alen, fci_u8 *data, fci_u8 len)
{
	int ret;
	fci_u8 tmp[4] = {0xcc, 0xcc, 0xcc, 0xcc};

	ret = fci_i2c_transfer(hDevice, I2C_READ, chip << 1, &addr, alen, &tmp[0], len);
	if(ret != I2C_OK) {
		PRINTF(hDevice, "fci_i2c_read() result=%d, addr = %x, data=%x\n\r", ret, addr, *data);
		return ret;
	}

	*data = tmp[0];

	return ret;
}

int fci_i2c_write(HANDLE hDevice, fci_u8 chip, fci_u8 addr, fci_u8 alen, fci_u8 *data, fci_u8 len)
{
	int ret;
	fci_u8 *paddr = &addr;

	ret = fci_i2c_transfer(hDevice, I2C_WRITE, chip << 1, paddr, alen, data, len);
	if(ret != I2C_OK) {
		PRINTF(hDevice, "fci_i2c_write() result=%d, addr= %x, data=%x\n\r", ret, addr, data);
	}

	return ret;
}
