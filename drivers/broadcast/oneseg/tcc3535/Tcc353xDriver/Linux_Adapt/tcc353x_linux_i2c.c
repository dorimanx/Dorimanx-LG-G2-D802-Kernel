/****************************************************************************
 *   FileName    : tcc353x_linux_i2c.c
 *   Description : tcc353x i2c function for linux
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited to re-
distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS" and nothing contained in this source code shall 
constitute any express or implied warranty of any kind, including without limitation, any warranty 
of merchantability, fitness for a particular purpose or non-infringement of any patent, copyright 
or other third party intellectual property right. No warranty is made, express or implied, 
regarding the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, out of 
or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************/

#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#include "tcc353x_common.h"
#include "tcpal_os.h"

#define MAX_I2C_BURST   (1024*8)

struct i2c_client *TcpalI2cClient = NULL;
I32S Tcc353xI2cClose(I32S _moduleIndex);

static I32U gI2cHanleInit0 = 0;
static I32U gI2cHanleInit1 = 0;
static I32U gI2cHanleInited = 0;
static I08U gI2cChipAddr[4];
static I08U I2cBuffer[(MAX_I2C_BURST+4)+32] __cacheline_aligned;

extern struct i2c_client *TCC_GET_I2C_DRIVER(void);

static I32S Tcc353xI2cSetup(I32S _moduleIndex)
{
	if (_moduleIndex >= 2) {
		TcpalPrintErr((I08S *) "Not supported, moduleidx=%d\n",
			      _moduleIndex);
		return TCC353X_RETURN_FAIL;
	}

	TcpalI2cClient = TCC_GET_I2C_DRIVER();	
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xI2cOpen(I32S _moduleIndex)
{
	I32S ret;
	ret = TCC353X_RETURN_FAIL;
	
	/* exception handling */
	if (_moduleIndex == 0) {
		if (gI2cHanleInit0 != 0 && gI2cHanleInit1 == 0)
			Tcc353xI2cClose(_moduleIndex);
	} else {
		if (gI2cHanleInit1 != 0 && gI2cHanleInit0 == 0)
			Tcc353xI2cClose(_moduleIndex);
	}

	/* normal process */
	if (_moduleIndex == 0)
		gI2cHanleInit0 = 1;
	else
		gI2cHanleInit1 = 1;

	if (gI2cHanleInited != 0) {
		return TCC353X_RETURN_SUCCESS;
	}

	gI2cHanleInited = 1;

	TcpalMemset(&gI2cChipAddr[_moduleIndex], 0x00, 4);
	ret = Tcc353xI2cSetup(_moduleIndex);

	/* need reset */

	return ret;
}

I32S Tcc353xI2cClose(I32S _moduleIndex)
{
	if (_moduleIndex == 0)
		gI2cHanleInit0 = 0;
	else
		gI2cHanleInit1 = 0;

	if (gI2cHanleInit0 == 0 && gI2cHanleInit1 == 0) {
		gI2cHanleInited = 0;
		TcpalPrintLog((I08S *)"TcpalI2cClient :0x%X\n", (unsigned int)TcpalI2cClient);
	}
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xAdaptI2CWriteEx(I08U raddr, I08U* txdata, I32S length)
{
	I32S rc;
	I32S cMax, remain;
	I32S i;
	struct i2c_msg msg;

	TcpalMemset (&msg, 0x00, sizeof(struct i2c_msg));

	cMax = length / MAX_I2C_BURST;
	remain = length % MAX_I2C_BURST;

	msg.addr = TcpalI2cClient->addr;
	msg.flags = 0;
	msg.len = MAX_I2C_BURST+1;
	msg.buf = (unsigned char*)I2cBuffer;

	I2cBuffer[0] = (unsigned char) (raddr);

	for (i = 0; i < cMax; i++) {
		TcpalMemcpy(&I2cBuffer[1], &txdata[i * MAX_I2C_BURST],
			    MAX_I2C_BURST);

		msg.len = MAX_I2C_BURST+1;
		msg.buf = (unsigned char*)I2cBuffer;

		rc = i2c_transfer(TcpalI2cClient->adapter, &msg, 1);
		if(rc < 0)
		{
			TcpalPrintErr((I08S *)"fail rc = (%d) addr =(0x%X) data=0x%02x\n", 
				(int)rc, (unsigned int)TcpalI2cClient->addr, (unsigned int)txdata[1]);
		
			return TCC353X_RETURN_FAIL;
		}
	}

	if (remain) {
		TcpalMemcpy(&I2cBuffer[1],
			    &txdata[cMax * MAX_I2C_BURST], remain);

		msg.len = remain+1;
		msg.buf = (unsigned char*)I2cBuffer;

		rc = i2c_transfer(TcpalI2cClient->adapter, &msg, 1);
		if(rc < 0)
		{
			TcpalPrintErr((I08S *)"fail rc = (%d) addr =(0x%X) data=0x%02x\n", 
				(int)rc, (unsigned int)TcpalI2cClient->addr, (unsigned int)txdata[1]);
		
			return TCC353X_RETURN_FAIL;
		}
	}

	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xAdaptI2CReadEx(I08U raddr, I08U *rxdata, I32S length)
{
	I32S rc;
	I32S cMax, remain;
	I32S i;
	struct i2c_msg msgs[2];

	TcpalMemset (&msgs[0], 0x00, sizeof(struct i2c_msg)*2);

	cMax = length / MAX_I2C_BURST;
	remain = length % MAX_I2C_BURST;

	msgs[0].addr	= TcpalI2cClient->addr;
	msgs[0].flags = 0;
	msgs[0].len   = 1;
	msgs[0].buf   = (unsigned char*)&raddr;

	msgs[1].addr	= TcpalI2cClient->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len   = length;
	msgs[1].buf   = (unsigned char*)rxdata;

	for (i = 0; i < cMax; i++) {
		msgs[1].len   = MAX_I2C_BURST;
		msgs[1].buf   = (unsigned char*)rxdata + i*MAX_I2C_BURST;

		rc = i2c_transfer(TcpalI2cClient->adapter, msgs, 2);
		if(rc < 0)
		{
			TcpalPrintErr((I08S *)"failed! rc =(%d),%x \n", 
				(int)rc, (unsigned int)TcpalI2cClient->addr);
		
			return TCC353X_RETURN_FAIL;
		}
	}

	if (remain) {
		msgs[1].len   = remain;
		msgs[1].buf   = (unsigned char*)rxdata + cMax*MAX_I2C_BURST;

		rc = i2c_transfer(TcpalI2cClient->adapter, msgs, 2);
		if(rc < 0)
		{
			TcpalPrintErr((I08S *)"failed! rc =(%d),%x \n", 
				(int)rc, (unsigned int)TcpalI2cClient->addr);
		
			return TCC353X_RETURN_FAIL;
		}
	}

	return TCC353X_RETURN_SUCCESS;
};

