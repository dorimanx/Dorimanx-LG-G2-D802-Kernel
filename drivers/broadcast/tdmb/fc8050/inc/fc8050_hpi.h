/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_hpi.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/09/11 	jason		initial
*******************************************************************************/

#ifndef __FC8050_HPI_H__
#define __FC8050_HPI_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int fc8050_hpi_init(HANDLE hDevice, fci_u16 param1, fci_u16 param2);
extern int fc8050_hpi_byteread(HANDLE hDevice, fci_u16 addr, fci_u8 *data);
extern int fc8050_hpi_wordread(HANDLE hDevice, fci_u16 addr, fci_u16 *data);
extern int fc8050_hpi_longread(HANDLE hDevice, fci_u16 addr, fci_u32 *data);
extern int fc8050_hpi_bulkread(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length);
extern int fc8050_hpi_bytewrite(HANDLE hDevice, fci_u16 addr, fci_u8 data);
extern int fc8050_hpi_wordwrite(HANDLE hDevice, fci_u16 addr, fci_u16 data);
extern int fc8050_hpi_longwrite(HANDLE hDevice, fci_u16 addr, fci_u32 data);
extern int fc8050_hpi_bulkwrite(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length);
extern int fc8050_hpi_dataread(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length);
extern int fc8050_hpi_deinit(HANDLE hDevice);

#ifdef __cplusplus
}
#endif

#endif // __FC8050_HPI_H__

