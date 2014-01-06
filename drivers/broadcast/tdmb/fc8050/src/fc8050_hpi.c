/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_hpi.c
 
 Description : fc8050 host interface
 
 History : 
 ----------------------------------------------------------------------
 2009/09/14 	jason		initial
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fc8050_regs.h"

#define HPIC_READ			0x01	// read command
#define HPIC_WRITE			0x02	// write command
#define HPIC_AINC			0x04	// address increment
#define HPIC_BMODE			0x00	// byte mode
#define HPIC_WMODE                      0x10	// word mode
#define HPIC_LMODE                      0x20	// long mode
#define HPIC_ENDIAN			0x00	// little endian
#define HPIC_CLEAR			0x80	// currently not used

#define BBM_BASE_ADDR       0 
#define BBM_BASE_OFFSET     0

#define FC8050_CMD_REG			(*(volatile fci_u8 *)(BBM_BASE_ADDR + (BBM_COMMAND_REG << BBM_BASE_OFFSET)))
#define FC8050_ADDR_REG			(*(volatile fci_u8 *)(BBM_BASE_ADDR + (BBM_ADDRESS_REG << BBM_BASE_OFFSET)))
#define FC8050_DATA_REG			(*(volatile fci_u8 *)(BBM_BASE_ADDR + (BBM_DATA_REG << BBM_BASE_OFFSET)))

int fc8050_hpi_init(HANDLE hDevice, fci_u16 param1, fci_u16 param2) 
{
	return BBM_OK;
}

int fc8050_hpi_byteread(HANDLE hDevice, fci_u16 addr, fci_u8 *data)
{
	FC8050_CMD_REG = HPIC_READ | HPIC_BMODE | HPIC_ENDIAN;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	*data = FC8050_DATA_REG;

	return BBM_OK;
}

int fc8050_hpi_wordread(HANDLE hDevice, fci_u16 addr, fci_u16 *data)
{
	fci_u8 command = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = HPIC_READ | HPIC_WMODE | HPIC_ENDIAN;

	FC8050_CMD_REG = command;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	*data = FC8050_DATA_REG;
	*data |= FC8050_DATA_REG << 8;

	return BBM_OK;
}

int fc8050_hpi_longread(HANDLE hDevice, fci_u16 addr, fci_u32 *data)
{
	FC8050_CMD_REG = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	*data = FC8050_DATA_REG;
	*data |= FC8050_DATA_REG << 8;
	*data |= FC8050_DATA_REG << 16;
	*data |= FC8050_DATA_REG << 24;

	return BBM_OK;
}

int fc8050_hpi_bulkread(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	fci_s32 i;

	FC8050_CMD_REG = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	for(i=0; i<length; i++) {
		data[i] = FC8050_DATA_REG;
	}

	return BBM_OK;
}

int fc8050_hpi_bytewrite(HANDLE hDevice, fci_u16 addr, fci_u8 data)
{
	FC8050_CMD_REG = HPIC_WRITE | HPIC_BMODE | HPIC_ENDIAN;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	FC8050_DATA_REG = data;

	return BBM_OK;
}

int fc8050_hpi_wordwrite(HANDLE hDevice, fci_u16 addr, fci_u16 data)
{
	fci_u8 command = HPIC_WRITE | HPIC_BMODE | HPIC_ENDIAN | HPIC_AINC;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = HPIC_WRITE | HPIC_WMODE | HPIC_ENDIAN;

	FC8050_CMD_REG = command;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	FC8050_DATA_REG = (data & 0xff);
	FC8050_DATA_REG = (data & 0xff00) >> 8;

	return BBM_OK;
}

int fc8050_hpi_longwrite(HANDLE hDevice, fci_u16 addr, fci_u32 data)
{
	FC8050_CMD_REG = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	FC8050_DATA_REG = (data & 0xff);
	FC8050_DATA_REG = (data & 0xff00) >> 8;
	FC8050_DATA_REG = (data & 0xff0000) >> 16;
	FC8050_DATA_REG = (data & 0xff000000) >> 24;

	return BBM_OK;
}

int fc8050_hpi_bulkwrite(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	fci_s32 i;

	FC8050_CMD_REG = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG = (addr & 0xff00) >> 8;

	for(i = 0; i < length; i++) {
		FC8050_DATA_REG = data[i];
	}

	return BBM_OK;
}

int fc8050_hpi_dataread(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	fci_s32 i;

	FC8050_CMD_REG = HPIC_READ | HPIC_BMODE | HPIC_ENDIAN;

	FC8050_ADDR_REG = (addr & 0xff);
	FC8050_ADDR_REG= (addr & 0xff00) >> 8;

	for(i = 0; i < length; i++) {
		data[i] = FC8050_DATA_REG;
	}

	return BBM_OK;
}

int fc8050_hpi_deinit(HANDLE hDevice)
{
	return BBM_OK;
}
