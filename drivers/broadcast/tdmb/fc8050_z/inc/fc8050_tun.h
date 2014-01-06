/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_tun.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/09/11 	jason		initial
*******************************************************************************/

#ifndef __FC8050_TUNER__
#define __FC8050_TUNER__

#ifdef __cplusplus
extern "C" {
#endif

extern int fc8050_tuner_init(HANDLE hDevice, band_type band);
extern int fc8050_set_freq(HANDLE hDevice, band_type band, fci_u32 f_lo );
extern int fc8050_get_rssi(HANDLE hDevice, int *rssi);

#ifdef __cplusplus
}
#endif

#endif // __FC8050_TUNER__

