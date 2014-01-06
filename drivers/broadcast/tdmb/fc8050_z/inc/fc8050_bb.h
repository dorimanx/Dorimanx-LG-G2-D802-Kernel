/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_bb.h
 
 Description : baseband header file
 
 History : 
 ----------------------------------------------------------------------
 2009/09/14 	jason		initial
*******************************************************************************/

#ifndef __FC8050_BB_H__
#define __FC8050_BB_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int  fc8050_reset(HANDLE hDevice);
extern int  fc8050_probe(HANDLE hDevice);
extern int  fc8050_init(HANDLE hDevice);
extern int  fc8050_deinit(HANDLE hDevice);

extern int  fc8050_scan_status(HANDLE hDevice);
extern int  fc8050_channel_select(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId);
extern int  fc8050_video_select(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId, fci_u8 cdiId);
extern int  fc8050_audio_select(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId);
extern int  fc8050_data_select(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId);

extern int  fc8050_channel_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId);
extern int  fc8050_video_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId, fci_u8 cdiId);
extern int  fc8050_audio_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId);
extern int  fc8050_data_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId);

#ifdef __cplusplus
}
#endif

#endif 		// __FC8050_BB_H__
