/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fci_hal.c
 
 Description : fc8050 host interface
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/bbm.h"
#include "../inc/fci_hal.h"
#include "../inc/fc8050_hpi.h"
#include "../inc/fc8050_spi.h"
#include "../inc/fc8050_ppi.h"
#include "../inc/fc8050_i2c.h"

typedef struct {
	int (*init)(HANDLE hDevice, fci_u16 param1, fci_u16 param2);

	int (*byteread)(HANDLE hDevice, fci_u16 addr, fci_u8  *data);
	int (*wordread)(HANDLE hDevice, fci_u16 addr, fci_u16 *data);
	int (*longread)(HANDLE hDevice, fci_u16 addr, fci_u32 *data);
	int (*bulkread)(HANDLE hDevice, fci_u16 addr, fci_u8  *data, fci_u16 size);

	int (*bytewrite)(HANDLE hDevice, fci_u16 addr, fci_u8  data);
	int (*wordwrite)(HANDLE hDevice, fci_u16 addr, fci_u16 data);
	int (*longwrite)(HANDLE hDevice, fci_u16 addr, fci_u32 data);
	int (*bulkwrite)(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 size);

	int (*dataread)(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 size);

	int (*deinit)(HANDLE hDevice);
} IF_PORT;

static IF_PORT hpiif = {
	&fc8050_hpi_init,

	&fc8050_hpi_byteread,
	&fc8050_hpi_wordread,
	&fc8050_hpi_longread,
	&fc8050_hpi_bulkread,

	&fc8050_hpi_bytewrite,
	&fc8050_hpi_wordwrite,
	&fc8050_hpi_longwrite,
	&fc8050_hpi_bulkwrite,

	&fc8050_hpi_dataread,

	&fc8050_hpi_deinit
};

static IF_PORT spiif = {
	&fc8050_spi_init,

	&fc8050_spi_byteread,
	&fc8050_spi_wordread,
	&fc8050_spi_longread,
	&fc8050_spi_bulkread,

	&fc8050_spi_bytewrite,
	&fc8050_spi_wordwrite,
	&fc8050_spi_longwrite,
	&fc8050_spi_bulkwrite,

	&fc8050_spi_dataread,

	&fc8050_spi_deinit
};

static IF_PORT ppiif = {
	&fc8050_ppi_init,

	&fc8050_ppi_byteread,
	&fc8050_ppi_wordread,
	&fc8050_ppi_longread,
	&fc8050_ppi_bulkread,

	&fc8050_ppi_bytewrite,
	&fc8050_ppi_wordwrite,
	&fc8050_ppi_longwrite,
	&fc8050_ppi_bulkwrite,

	&fc8050_ppi_dataread,

	&fc8050_ppi_deinit
};

static IF_PORT i2cif = {
	&fc8050_i2c_init,

	&fc8050_i2c_byteread,
	&fc8050_i2c_wordread,
	&fc8050_i2c_longread,
	&fc8050_i2c_bulkread,

	&fc8050_i2c_bytewrite,
	&fc8050_i2c_wordwrite,
	&fc8050_i2c_longwrite,
	&fc8050_i2c_bulkwrite,

	&fc8050_i2c_dataread,

	&fc8050_i2c_deinit
};

static IF_PORT *ifport = &hpiif;
static fci_u8 hostif_type = BBM_HPI;

int bbm_hostif_get(HANDLE hDevice, fci_u8 *hostif)
{
	*hostif = hostif_type;

	return BBM_OK;
}

int bbm_hostif_select(HANDLE hDevice, fci_u8 hostif)
{
	hostif_type = hostif;

	switch(hostif) {
		case BBM_HPI:
			ifport = &hpiif;
			break;
		case BBM_SPI:
			ifport = &spiif;
			break;
		case BBM_I2C:
			ifport = &i2cif;
			break;
		case BBM_PPI:
			ifport = &ppiif;
			break;
		default:
			return BBM_E_HOSTIF_SELECT;
	}

	if(ifport->init(hDevice, 0, 0))
		return BBM_E_HOSTIF_INIT;

	return BBM_OK;
}

int bbm_hostif_deselect(HANDLE hDevice)
{
	if(ifport->deinit(hDevice))
		return BBM_NOK;

	ifport = NULL;
	hostif_type = BBM_HPI;

	return BBM_OK;
}

int bbm_read(HANDLE hDevice, fci_u16 addr, fci_u8 *data)
{
	if(ifport->byteread(hDevice, addr, data))
		return BBM_E_BB_REG_READ;
	return BBM_OK;
}

int bbm_byte_read(HANDLE hDevice, fci_u16 addr, fci_u8 *data)
{
	if(ifport->byteread(hDevice, addr, data))
		return BBM_E_BB_REG_READ;
	return BBM_OK;
}

int bbm_word_read(HANDLE hDevice, fci_u16 addr, fci_u16 *data)
{
	if(ifport->wordread(hDevice, addr, data))
		return BBM_E_BB_REG_READ;
	return BBM_OK;
}

int bbm_long_read(HANDLE hDevice, fci_u16 addr, fci_u32 *data)
{
	if(ifport->longread(hDevice, addr, data))
		return BBM_E_BB_REG_READ;
	return BBM_OK;
}

int bbm_bulk_read(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	if(ifport->bulkread(hDevice, addr, data, length))
		return BBM_E_BB_REG_READ;
	return BBM_OK;
}

int bbm_write(HANDLE hDevice, fci_u16 addr, fci_u8 data)
{
	if(ifport->bytewrite(hDevice, addr, data))
		return BBM_E_BB_REG_WRITE;
	return BBM_OK;
}

int bbm_byte_write(HANDLE hDevice, fci_u16 addr, fci_u8 data)
{
	if(ifport->bytewrite(hDevice, addr, data))
		return BBM_E_BB_REG_WRITE;
	return BBM_OK;
}

int bbm_word_write(HANDLE hDevice, fci_u16 addr, fci_u16 data)
{
	if(ifport->wordwrite(hDevice, addr, data))
		return BBM_E_BB_REG_WRITE;
	return BBM_OK;
}

int bbm_long_write(HANDLE hDevice, fci_u16 addr, fci_u32 data)
{
	if(ifport->longwrite(hDevice, addr, data))
		return BBM_E_BB_REG_WRITE;
	return BBM_OK;
}

int bbm_bulk_write(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length)
{
	if(ifport->bulkwrite(hDevice, addr, data, length))
		return BBM_E_BB_REG_WRITE;
	return BBM_OK;
}

int bbm_data(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length)
{
	if(ifport->dataread(hDevice, addr, data, length))
		return BBM_E_BB_REG_WRITE;
	return BBM_OK;
}
