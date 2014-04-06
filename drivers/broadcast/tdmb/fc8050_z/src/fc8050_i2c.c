/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved

 File name : fc8050_i2c.c

 Description : fc8050 host interface

 History :
 ----------------------------------------------------------------------
*******************************************************************************/
#include "../inc/fci_types.h"
#include "../inc/fc8050_regs.h"

#define HPIC_READ			0x01	// read command
#define HPIC_WRITE			0x02	// write command
#define HPIC_AINC			0x04	// address increment
#define HPIC_BMODE			0x00	// byte mode
#define HPIC_WMODE          0x10	// word mode
#define HPIC_LMODE          0x20	// long mode
#define HPIC_ENDIAN			0x00	// little endian
#define HPIC_CLEAR			0x80	// currently not used

static unsigned char DADR       = 0xB0;
static unsigned char IADDR_SIZE = 1;

static int i2c_bulkread(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	return 0;	//               
}

static int i2c_bulkwrite(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	return 0;	//               
}

static int i2c_dataread(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	return i2c_bulkread(hDevice, addr, data, length);
}

int fc8050_i2c_init(HANDLE hDevice, fci_u16 param1, fci_u16 param2)
{
	fci_u8 device_addr        = 0xB0;
	fci_u8 internal_addr_size = 1;

	// set DADR
	DADR = device_addr;

	// set iaddr size
	IADDR_SIZE = internal_addr_size;

	return BBM_OK;
}

int fc8050_i2c_byteread(HANDLE hDevice, fci_u16 addr, fci_u8 *data)
{
	int res;
	fci_u8 command = HPIC_READ | HPIC_BMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkread(hDevice, BBM_DATA_REG, data, 1);

	return res;
}

int fc8050_i2c_wordread(HANDLE hDevice, fci_u16 addr, fci_u16 *data)
{
	int res;
	fci_u8 command = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = HPIC_READ | HPIC_WMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkread(hDevice, BBM_DATA_REG, (fci_u8*)data, 2);

	return res;
}

int fc8050_i2c_longread(HANDLE hDevice, fci_u16 addr, fci_u32 *data)
{
	int res;
	fci_u8 command = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkread(hDevice, BBM_DATA_REG, (fci_u8*)data, 4);

	return res;
}

int fc8050_i2c_bulkread(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	int res;
	fci_u8 command = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkread(hDevice, BBM_DATA_REG, data, length);

	return res;
}

int fc8050_i2c_bytewrite(HANDLE hDevice, fci_u16 addr, fci_u8 data)
{
	int res;
	fci_u8 command = HPIC_WRITE | HPIC_BMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkwrite(hDevice, BBM_DATA_REG, (fci_u8*)&data, 1);

	return res;
}

int fc8050_i2c_wordwrite(HANDLE hDevice, fci_u16 addr, fci_u16 data)
{
	int res;
	fci_u8 command = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = HPIC_WRITE | HPIC_WMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkwrite(hDevice, BBM_DATA_REG, (fci_u8*)&data, 2);

	return res;
}

int fc8050_i2c_longwrite(HANDLE hDevice, fci_u16 addr, fci_u32 data)
{
	int res;
	fci_u8 command = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkwrite(hDevice, BBM_DATA_REG, (fci_u8*)&data, 4);

	return res;
}

int fc8050_i2c_bulkwrite(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	int res;
	fci_u8 command = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_bulkwrite(hDevice, BBM_DATA_REG, data, length);

	return res;
}

int fc8050_i2c_dataread(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	int res;
	fci_u8 command = HPIC_READ | HPIC_BMODE | HPIC_ENDIAN;

	res  = i2c_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= i2c_bulkwrite(hDevice, BBM_ADDRESS_REG, (fci_u8*)&addr, 2);
	res |= i2c_dataread(hDevice, BBM_DATA_REG, data, length);

	return res;
}

int fc8050_i2c_deinit(HANDLE hDevice)
{
	return BBM_OK;
}
