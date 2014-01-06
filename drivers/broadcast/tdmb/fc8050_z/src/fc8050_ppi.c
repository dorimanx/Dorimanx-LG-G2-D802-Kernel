/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_ppi.c
 
 Description : fc8050 host interface
 
 History : 
 ----------------------------------------------------------------------
 2009/09/14 	jason		initial
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fc8050_regs.h"

#define BBM_BASE_ADDR				0x00//TDMB_RFBB_BASE_ADDR
#define BBM_BASE_OFFSET 			0x00

#define PPI_BMODE                       0x00
#define PPI_WMODE                       0x10
#define PPI_LMODE                       0x20
#define PPI_READ                        0x40
#define PPI_WRITE                       0x00
#define PPI_AINC                        0x80

#define FC8050_PPI_REG			(*(volatile fci_u8 *)(BBM_BASE_ADDR + (BBM_COMMAND_REG << BBM_BASE_OFFSET)))

int fc8050_ppi_init(HANDLE hDevice, fci_u16 param1, fci_u16 param2)
{

	return BBM_OK;
}

int fc8050_ppi_byteread(HANDLE hDevice, fci_u16 addr, fci_u8 *data)
{
	fci_u16 length = 1;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = PPI_READ | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	*data = FC8050_PPI_REG;

	return BBM_OK;
}

int fc8050_ppi_wordread(HANDLE hDevice, fci_u16 addr, fci_u16 *data)
{
	fci_u16 length = 2;
	fci_u8 command = PPI_AINC | PPI_READ | PPI_BMODE;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = PPI_READ | PPI_WMODE;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = command | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	*data = FC8050_PPI_REG;
	*data |= FC8050_PPI_REG << 8;

	return BBM_OK;
}

int fc8050_ppi_longread(HANDLE hDevice, fci_u16 addr, fci_u32 *data)
{
	fci_u16 length = 4;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = PPI_AINC | PPI_READ | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	*data = FC8050_PPI_REG;
	*data |= FC8050_PPI_REG << 8;
	*data |= FC8050_PPI_REG << 16;
	*data |= FC8050_PPI_REG << 24;

	return BBM_OK;
}

int fc8050_ppi_bulkread(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	int i;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = PPI_AINC | PPI_READ | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	for(i=0; i<length; i++) {
		data[i] = FC8050_PPI_REG;
	}

	return BBM_OK;
}

int fc8050_ppi_bytewrite(HANDLE hDevice, fci_u16 addr, fci_u8 data)
{
	fci_u16 length = 1;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = PPI_WRITE | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	FC8050_PPI_REG = data;

	return BBM_OK;
}

int fc8050_ppi_wordwrite(HANDLE hDevice, fci_u16 addr, fci_u16 data)
{
	fci_u16 length = 2;
	fci_u8 command = PPI_AINC | PPI_WRITE | PPI_BMODE;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = PPI_WRITE | PPI_WMODE;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = command | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	FC8050_PPI_REG = data & 0xff;
	FC8050_PPI_REG = (data & 0xff00) >> 8;

	return BBM_OK;
}

int fc8050_ppi_longwrite(HANDLE hDevice, fci_u16 addr, fci_u32 data)
{
	fci_u16 length = 4;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = PPI_AINC | PPI_WRITE | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	FC8050_PPI_REG = data &  0x000000ff;
	FC8050_PPI_REG = (data & 0x0000ff00) >> 8;
	FC8050_PPI_REG = (data & 0x00ff0000) >> 16;
	FC8050_PPI_REG = (data & 0xff000000) >> 24;

	return BBM_OK;
}

int fc8050_ppi_bulkwrite(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	int i;

	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
	FC8050_PPI_REG = PPI_AINC | PPI_WRITE | ((length & 0x0f00) >> 8);
	FC8050_PPI_REG = length & 0xff;

	for(i=0; i<length; i++) {
		FC8050_PPI_REG = data[i];
	}

	return BBM_OK;
}

int fc8050_ppi_dataread(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	int i, j;
	fci_u16 x, y;

	x = length / 4095;
	y = length % 4095;


	for(i=0; i<x; i++) {
		FC8050_PPI_REG = addr & 0xff;
		FC8050_PPI_REG = (addr & 0xff00) >> 8;
		FC8050_PPI_REG = PPI_READ | ((4095 & 0x0f00) >> 8);
		FC8050_PPI_REG = 4095 & 0xff;

		for(j=0; j<4095; j++) {
			data[4095*i+j] = FC8050_PPI_REG;
		}
	}

	if(y) {
	FC8050_PPI_REG = addr & 0xff;
	FC8050_PPI_REG = (addr & 0xff00) >> 8;
		FC8050_PPI_REG = PPI_READ | ((y & 0x0f00) >> 8);
		FC8050_PPI_REG = y & 0xff;

		for(j=0; j<y; j++) {
			data[4095*x+j] = FC8050_PPI_REG;
		}
	}

	return BBM_OK;
}

int fc8050_ppi_deinit(HANDLE hDevice)
{
	return BBM_OK;
}
