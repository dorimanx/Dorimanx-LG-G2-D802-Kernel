/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved

 File name : fc8080_demux.h

 Description : baseband header file

 History :
 ----------------------------------------------------------------------

*******************************************************************************/

#ifndef __FC8080_DEMUX_H__
#define __FC8080_DEMUX_H__

#ifdef __cplusplus
extern "C" {
#endif
#define TSIF_LGE_IF

typedef enum ts_data_type
{
	TS_DAT_FIC,
	TS_DAT_VIDEO_I,
	TS_DAT_VIDEO_II,
	TS_DAT_NVIDEO
} ts_data_type;

int fc8080_demux(fci_u8* data, fci_u32 length);
int fc8080_demux_fic_callback_register(fci_u32 userdata, int (*callback)(fci_u32 userdata, fci_u8 *data, int length));
int fc8080_demux_msc_callback_register(fci_u32 userdata, int (*callback)(fci_u32 userdata, fci_u8 subChId, fci_u8 *data, int length));
int fc8080_demux_select_video(fci_u8 subChID, fci_u8 cdiId);
int fc8080_demux_select_channel(fci_u8 subChId, fci_u8 svcChId);
int fc8080_demux_deselect_video(fci_u8 subChId, fci_u8 cdiId);
int fc8080_demux_deselect_channel(fci_u8 subChId, fci_u8 svcChId);
#ifdef TSIF_LGE_IF
fci_u32 fc8080_get_ts_datalen(ts_data_type type, fci_u8 ch, fci_u32 *len);
fci_u32 fc8080_get_multi_data(ts_data_type type, fci_u8 ch, fci_u8* buf, fci_u8* chid);
#endif
#ifdef __cplusplus
}
#endif

#endif // __FC8050_DEMUX_H__
