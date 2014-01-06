/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_ppi.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/09/14 	jason		initial
*******************************************************************************/

#ifndef __FC8050_PPI_H__
#define __FC8050_PPI_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int fc8050_ppi_init(HANDLE hDevice, fci_u16 param1, fci_u16 param2);
extern int fc8050_ppi_byteread(HANDLE hDevice, fci_u16 addr, fci_u8 *data);
extern int fc8050_ppi_wordread(HANDLE hDevice, fci_u16 addr, fci_u16 *data);
extern int fc8050_ppi_longread(HANDLE hDevice, fci_u16 addr, fci_u32 *data);
extern int fc8050_ppi_bulkread(HANDLE hDevice, fci_u16 addr, fci_u8 *data, fci_u16 length);
extern int fc8050_ppi_bytewrite(HANDLE hDevice, fci_u16 addr, fci_u8 data);
extern int fc8050_ppi_wordwrite(HANDLE hDevice, fci_u16 addr, fci_u16 data);
extern int fc8050_ppi_longwrite(HANDLE hDevice, fci_u16 addr, fci_u32 data);
extern int fc8050_ppi_bulkwrite(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length);
extern int fc8050_ppi_dataread(HANDLE hDevice, fci_u16 addr, fci_u8* data, fci_u16 length);
extern int fc8050_ppi_deinit(HANDLE hDevice);

#ifdef __cplusplus
}
#endif

#endif // __FC8050_PPI_H__

