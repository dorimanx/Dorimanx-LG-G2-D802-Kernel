/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_isr.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/09/11 	jason		initial
*******************************************************************************/

#ifndef __FC8050_ISR__
#define __FC8050_ISR__

#ifdef __cplusplus
extern "C" {
#endif

#include "fci_types.h"

extern fci_u32 gFicUserData;
extern fci_u32 gMscUserData;

extern int (*pFicCallback)(fci_u32 userdata, fci_u8 *data, int length);
extern int (*pMscCallback)(fci_u32 userdata, fci_u8 subchid, fci_u8 *data, int length);

extern void fc8050_isr(HANDLE hDevice);

#ifdef __cplusplus
}
#endif

#endif // __FC8050_ISR__

