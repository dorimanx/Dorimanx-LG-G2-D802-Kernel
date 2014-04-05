/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved

 File name : fc8080_demux.c

 Description : fc8080 TSIF demux

 History :
 ----------------------------------------------------------------------
 2009/04/09 	woost		initial
*******************************************************************************/

/*
** Include Header File
*/
#include "../inc/fci_types.h"
#include "../inc/fc8080_regs.h"
#include "../inc/fc8080_demux.h"

#include <linux/input.h>

/*============================================================
**	  1.   DEFINITIONS
*============================================================*/

// Sync byte
#define SYNC_MASK_FIC           0x80
#define SYNC_MASK_DM            0x90
#define SYNC_MASK_NVIDEO        0xC0
#define SYNC_MASK_VIDEO         0x47
#define SYNC_MASK_VIDEO1        0xB8
#define SYNC_MASK_APAD          0xA0

// packet indicator
#define PKT_IND_NONE            0x00
#define PKT_IND_END             0x20
#define PKT_IND_CONTINUE        0x40
#define PKT_IND_START           0x80
#define PKT_IND_MASK            0xE0

// data size
#define FIC_DATA_SIZE           (FIC_BUF_LENGTH / 2)
#define MSC_DATA_SIZE           (CH0_BUF_LENGTH / 2)
#define DM_DATA_SIZE            188
#define NV_DATA_SIZE            (CH2_BUF_LENGTH / 2)

// TS service information
typedef struct _TS_FRAME_INFO
{
	fci_u8	ind;
	fci_u16	length;   // current receiving length
	fci_u8	subch_id; // sub ch id
	fci_u8*	buffer;
#ifdef TSIF_LGE_IF
	fci_u8	done;
#endif
} TS_FRAME_INFO;

// TS frame header information
typedef struct _TS_FRAME_HDR_
{
	fci_u8	sync;
	fci_u8	ind;
	fci_u16	length;
	fci_u8*	data;
} TS_FRAME_HDR;

/*============================================================
**	  2.   Variables
*============================================================*/
static int (*pFicCallback)(fci_u32 userdata, fci_u8 *data, int length) = NULL;
static int (*pMscCallback)(fci_u32 userdata, fci_u8 subChId, fci_u8 *data, int length) = NULL;
static fci_u32 gFicUserData = 0, gMscUserData = 0;

static fci_u8 sync_error_cnt = 0;

#ifdef TSIF_LGE_IF
fci_u8 bTSVideo[2][188*64];
fci_u8 bTSFic[188*64];
fci_u8 bTSNVideo[2][188*64];
#else
fci_u8 bTSVideo[2][MSC_DATA_SIZE];
fci_u8 bTSFic[FIC_DATA_SIZE];
fci_u8 bTSNVideo[2][NV_DATA_SIZE];
#endif

fci_u8 g_vd_channel[2] = {
	0xff, 0xff
};
fci_u8 g_nv_channel[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff
};
TS_FRAME_INFO sTSVideo[2] = {
	{
		0, 0, 0xff, (fci_u8*) bTSVideo[0], 0
	},
	{
		0, 0, 0xff, (fci_u8*) bTSVideo[1], 0
	}
};
TS_FRAME_INFO sTSFic = {
	0, 0, 0xff, (fci_u8*) &bTSFic[0], 0
};

TS_FRAME_INFO sTSNVideo[8] = {
	{0, 0, 0xff, NULL, 0},
	{0, 0, 0xff, NULL, 0},
	{0, 0, 0xff, (fci_u8*) bTSNVideo[0], 0},
	{0, 0, 0xff, (fci_u8*) bTSNVideo[1], 0},
	{0, 0, 0xff, NULL, 0},
	{0, 0, 0xff, NULL, 0},
	{0, 0, 0xff, NULL, 0},
	{0, 0, 0xff, NULL, 0}
};

/*============================================================
**	  3.   Function Prototype
*============================================================*/
int ts_fic_gather(fci_u8* data, fci_u32 length)
{
	fci_u16 len;
	TS_FRAME_HDR header;

	header.sync		= data[0];
	header.ind		= data[1];
	header.length	= (data[2] << 8) | data[3];
	header.data		= &data[4];

	// current real length
	len = (header.length > 184) ? 184 : header.length;

	if(header.ind == PKT_IND_START) { // fic start frame
		// discard already data if exist, receive new fic frame
		sTSFic.ind		= header.ind;
		sTSFic.length	= 0;
		memcpy((void*)&sTSFic.buffer[sTSFic.length], header.data, len);
		sTSFic.length	= len;
		return BBM_OK;
	} else if(header.ind == PKT_IND_CONTINUE)	{ // fic continue frame
		if(sTSFic.ind != PKT_IND_START) {
			// discard already data & current receiving data
			sTSFic.ind = PKT_IND_NONE;
		} else {
			// store
			sTSFic.ind = header.ind;
			memcpy((void*)&sTSFic.buffer[sTSFic.length], header.data, len);
			sTSFic.length += len;
		}
		return BBM_OK;
	} else if(header.ind == PKT_IND_END) { // fic end frame
		if(sTSFic.ind != PKT_IND_CONTINUE) {
			// discard alread data & current receiving data
			sTSFic.ind = PKT_IND_NONE;
			return BBM_E_MUX_INDICATOR;
		} else {
			// store
			sTSFic.ind = header.ind;
			memcpy((void*)&sTSFic.buffer[sTSFic.length], header.data, len);
			sTSFic.length += len;
		}
	}

	// send host application
	if((sTSFic.length >= FIC_DATA_SIZE) && (sTSFic.ind == PKT_IND_END)) {
#ifdef TSIF_LGE_IF
		sTSFic.done=1;
#else
		if(pFicCallback)
			(*pFicCallback)(gFicUserData, sTSFic.buffer, sTSFic.length);

		sTSFic.length	= 0;
#endif
		sTSFic.ind		= PKT_IND_NONE;
	}

	return BBM_OK;
}

int ts_nv_gather(fci_u8* data, fci_u32 length) {
	TS_FRAME_HDR header;
	fci_u32 len;
	fci_u8  subch;
	fci_u8  ch;

	header.sync   = data[0];
	header.ind    = data[1];
	header.length = (data[2] << 8) | data[3];
	header.data   = &data[4];

	len = (header.length >= 184) ? 184 : header.length;

	subch = header.sync & 0x3f;
	ch = g_nv_channel[subch];

	if(ch == 0xff)
		return BBM_E_MUX_SUBCHANNEL;

	if(header.ind == PKT_IND_START) {
		sTSNVideo[ch].length   = 0;

		memcpy((void*)&sTSNVideo[ch].buffer[0], header.data, len);
		sTSNVideo[ch].length += len;
		if(sTSNVideo[ch].length < header.length)
			sTSNVideo[ch].ind = header.ind;
		else
			sTSNVideo[ch].ind = PKT_IND_END;
	} else if(header.ind == PKT_IND_CONTINUE) {
		memcpy((void*)&sTSNVideo[ch].buffer[sTSNVideo[ch].length], header.data, len);
		sTSNVideo[ch].length += len;
		sTSNVideo[ch].ind     = header.ind;
		return BBM_OK;
	} else if(header.ind == PKT_IND_END) {
		memcpy((void*)&sTSNVideo[ch].buffer[sTSNVideo[ch].length], header.data, len);
		sTSNVideo[ch].length += len;
		sTSNVideo[ch].ind     = header.ind;
	}

	if((header.ind == PKT_IND_END) || (header.ind == PKT_IND_START && 184 >= sTSNVideo[ch].length)) {
#ifdef TSIF_LGE_IF
		sTSNVideo[ch].done=1;
#else
		if(pMscCallback)
			(*pMscCallback)(gMscUserData, subch, sTSNVideo[ch].buffer, sTSNVideo[ch].length);

		sTSNVideo[ch].length = 0;
#endif
		sTSNVideo[ch].ind    = PKT_IND_NONE;
	}

	return BBM_OK;
}

int ts_dmb_gather(fci_u8* data, fci_u32 length) {
	fci_u8 ch;

	// trace sync
	if(data[0] == SYNC_MASK_VIDEO){
		ch = 0;
	} else if(data[0] == SYNC_MASK_VIDEO1){
		ch = 1;
		data[0] = 0x47;
	} else {
		return BBM_E_MUX_DATA_MASK;
	}

	if(g_vd_channel[ch] == 0xff)
		return BBM_E_MUX_SUBCHANNEL;

	memcpy((void*)(sTSVideo[ch].buffer + sTSVideo[ch].length), data, 188);
	sTSVideo[ch].length += length;

	if(sTSVideo[ch].length >= MSC_DATA_SIZE)
	{
#ifdef TSIF_LGE_IF
		sTSVideo[ch].done=1;
#else

		if(pMscCallback)
			(*pMscCallback)(gMscUserData, g_vd_channel[ch], sTSVideo[ch].buffer, MSC_DATA_SIZE);
		sTSVideo[ch].length = 0;
#endif
	}

	return BBM_OK;
}

int fc8080_demux(fci_u8* data, fci_u32 length) {
	int res = BBM_OK;
	fci_u32 i;
	fci_u8  sync_error = 0;

	for(i = 0; i < length; i += 188)  {
		if(data[i] == SYNC_MASK_FIC) {
			res = ts_fic_gather(&data[i], 188);
		} else if((data[i] == SYNC_MASK_VIDEO) || (data[i] == SYNC_MASK_VIDEO1)) {
			res = ts_dmb_gather(&data[i], 188);
		} else if((data[i] & 0xC0) == 0xC0) {
			res = ts_nv_gather(&data[i], 188);
		} else {
			//PRINTF(" %02X", data[i]);
			sync_error++;
		}
	}

	if(sync_error > 0) {
		sync_error_cnt += sync_error;
		if(sync_error_cnt >= 5)
			return BBM_E_MUX_SYNC;
	} else{
		sync_error_cnt = 0;
	}
	return res;
}

#ifdef TSIF_LGE_IF
fci_u32 fc8080_get_ts_datalen(ts_data_type type, fci_u8 ch, fci_u32 *len)
{
	switch(type)
	{
		case TS_DAT_FIC:
			if(sTSFic.done)
			{
				*len = sTSFic.length;
				return BBM_OK;
			}
		break;

		case TS_DAT_VIDEO_I:
			if(sTSVideo[ch].done)
			{
				*len = sTSVideo[ch].length;
				return BBM_OK;
			}
		break;

		case TS_DAT_NVIDEO:
			if(sTSNVideo[ch].done)
			{
				*len = sTSNVideo[ch].length;
				return BBM_OK;
			}
		break;

		default:
			return BBM_NOK;
			break;
	}

	return BBM_NOK;
}

fci_u32 fc8080_get_multi_data(ts_data_type type, fci_u8 ch, fci_u8* buf, fci_u8* chid)
{
	switch(type)
	{
		case TS_DAT_FIC:
			if(sTSFic.done)
			{
				memcpy((void*)buf, (void*)sTSFic.buffer, sTSFic.length);
				sTSFic.done=0;
				sTSFic.length=0;
				return BBM_OK;
			}
		break;

		case TS_DAT_VIDEO_I:
			if(sTSVideo[ch].done)
			{
				memcpy((void*)buf, (void*)sTSVideo[ch].buffer, sTSVideo[ch].length);
				sTSVideo[ch].done=0;
				sTSVideo[ch].length=0;
				*chid=sTSVideo[ch].subch_id;
				return BBM_OK;
			}
		break;

		case TS_DAT_NVIDEO:
			if(sTSNVideo[ch].done)
			{
				memcpy((void*)buf, (void*)sTSNVideo[ch].buffer, sTSNVideo[ch].length);
				sTSNVideo[ch].done=0;
				sTSNVideo[ch].length=0;
				*chid=sTSNVideo[ch].subch_id;
				return BBM_OK;
			}
		break;

		default:
			return BBM_NOK;
			break;
	}

	return BBM_NOK;
}

#endif

int fc8080_demux_fic_callback_register(fci_u32 userdata, int (*callback)(fci_u32 userdata, fci_u8 *data, int length)) {
	gFicUserData = userdata;
	pFicCallback = callback;
	return BBM_OK;
}

int fc8080_demux_msc_callback_register(fci_u32 userdata, int (*callback)(fci_u32 userdata, fci_u8 subChId, fci_u8 *data, int length)) {
	gMscUserData = userdata;
	pMscCallback = callback;
	return BBM_OK;
}

int fc8080_demux_select_video(fci_u8 subChId, fci_u8 cdiId) {
	g_vd_channel[cdiId] = subChId;
	sTSVideo[cdiId].subch_id= subChId;
	return BBM_OK;
}

int fc8080_demux_select_channel(fci_u8 subChId, fci_u8 svcChId) {
	g_nv_channel[subChId] = svcChId;
	sTSNVideo[svcChId].subch_id= subChId;
	return BBM_OK;
}

int fc8080_demux_deselect_video(fci_u8 subChId, fci_u8 cdiId) {
	if(g_vd_channel[cdiId] == subChId)
	{
		g_vd_channel[cdiId] = 0xff;
		sTSVideo[cdiId].subch_id= 0;
	}
	return BBM_OK;
}

int fc8080_demux_deselect_channel(fci_u8 subChId, fci_u8 svcChId) {
	g_nv_channel[subChId] = 0xff;
	sTSNVideo[svcChId].subch_id= 0;
	return BBM_OK;
}
