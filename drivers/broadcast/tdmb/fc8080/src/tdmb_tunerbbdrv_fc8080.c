/*****************************************************************************
 Copyright(c) 2009 LG Electronics Inc. All Rights Reserved

 File name : Tunerbb_drv_fc8080.c

 Description : fc8080 made by FCI Driver Code

 History :
 ----------------------------------------------------------------------
 Dec. 25,  2009 :      FCI release for LG MC
*******************************************************************************/
#include "../inc/fci_tun.h"
#include "../inc/fc8080_regs.h"
#include "../inc/fc8080_demux.h"
#include "../inc/fc8080_isr.h"
#include "../inc/bbm.h"
#include "../inc/fci_types.h"
#include "../inc/tdmb_tunerbbdrv_fc8080def.h"
#include "../inc/fci_oal.h"

#include "../inc/broadcast_fc8080.h"

#include <linux/string.h>
/*============================================================
**    1.   DEFINITIONS

*============================================================*/
/* Example of Return value */
#define FC8080_RESULT_ERROR		(int8) 0
#define FC8080_RESULT_SUCCESS	(int8) 1

#undef FEATURE_FIC_BER
#undef FEATURE_RSSI_DEBUG

//        
#define	FREQ_SEARCH_IN_TABLE		/* Freq conversion in Table Searching */
#define	CH_LOW_NUM		71		/* 7A index 71 for UI */

#define	CH_UPPER_NUM		133		/* 13C index 131 for UI*/

#ifdef FREQ_SEARCH_IN_TABLE

#define	MAX_KOREABAND_FULL_CHANNEL	21

#define	INDEX_KOR_CH_NUM_DEC			0
#define	INDEX_KOR_FREQ_NUM				1
#else
#define	C7A_CEN_FREQ		175280	/* 7A Center Frequency */
#define	ENS_GAP_FREQ 		6000	/* Frequency Gap-Interval between Other Ensemble */
#define	CH_GAP_FREQ 		1728	/* Channel Center frequency interval between channel number */
#define	TDMB_ENS_NUM		7		/* Korea TDMB Ensemble Number 7 ~ 13 */
#endif
//        

#define MAX_MSC_BER			20000
#define MAX_VA_BER			20000

#define DMB_SVC_ID 0
#define DAB_SVC_ID 2
#define DAT_SVC_ID 1
#define FEATURE_GET_FIC_POLLING

/* change memcpy mscBuffer -> msc_data -> buffer  to mscBuffer->buffer */
#define NOT_MSCDATA_MULTIPLE_MEMCPY

uint32 	tp_total_cnt=0;

/* -----------------------------------------------------------------------
< HOST Interface FEATURE Usage>
  1. STREAM_TS_UPLOAD  : HOST Interface between MSM and FC8080 is TSIF(I2C)
  2. STREAM_SLAVE_PARALLEL_UPLOAD : HOST Interface between MSM and FC8080 is EBI2
  3. STREAM_SPI_UPLOAD : HOST Interface between MSM and FC8080 is SPI
  ------------------------------------------------------------------------- */

/*============================================================
**    2.   External Variables
*============================================================*/

/*============================================================
**    3.   External Functions
*============================================================*/


#ifdef STREAM_SLAVE_PARALLEL_UPLOAD
boolean 	send_fic_int_sig_isr2task(void);
#endif

extern int tunerbb_drv_fc8080_fic_cb(uint32 userdata, uint8 *data, int length);
extern int tunerbb_drv_fc8080_msc_cb(uint32 userdata, uint8 subChId, uint8 *data, int length);


/*============================================================
**    4.   Local constant variables
*============================================================*/


/*============================================================
**    5.   Local Typedef
*============================================================*/


typedef struct _BUFFR_TAG
{
	uint8	valid;
	uint32	address;
	uint32	length;
	uint8	subch_id;
}DATA_BUFFER;

typedef struct _HEADER_TYPE
{
	uint16	length;
	uint8	subch_id;
	uint8	svc_id;
}FCI_HEADER_TYPE;

/*============================================================
**    6.   Global Variables
*============================================================*/
fc8080_service_type	serviceType[FC8080_SERVICE_MAX];
fci_u8 tot_subch_cnt=0;
#if !defined(STREAM_TS_UPLOAD)
DATA_BUFFER 	msc_buffer;
DATA_BUFFER 	fic_buffer;
fci_u8 g_chinfo[64];
#ifdef NOT_MSCDATA_MULTIPLE_MEMCPY
fci_u8* msc_data = NULL;
#else
fci_u8 msc_data[188*8*8];
#endif
fci_u8 msc_multi_data[188*8*8];
#endif

/*============================================================
**    7.   Static Variables
*============================================================*/
//       
#ifdef FREQ_SEARCH_IN_TABLE

	static int32 gKOREnsembleFullFreqTbl[MAX_KOREABAND_FULL_CHANNEL][2] =
	{
		{71,175280},{72,177008},{73,178736},{81,181280},{82,183008},{83,184736},
		{91,187280},{92,189008},{93,190736},{101,193280},{102,195008},{103,196736},
		{111,199280},{112,201008},{113,202736},{121,205280},{122,207008},{123,208736}
		,{131,211280},{132,213008},{133,214736}
	};
#endif

static uint16 is_tdmb_probe = 0;

//static uint16 data_sequence_count = 0;
/*============================================================
**    8.   Local Function Prototype
*============================================================*/
static int32 tunerbb_drv_convert_chnum_to_freq(uint32 ch_num);
static uint32 tunerbb_drv_fc8080_get_viterbi_ber(void);
static int8 tunerbb_drv_fc8080_get_sync_status(void);
//static uint32 tunerbb_drv_fc8080_get_rs_ber(void);
static int8 tunerbb_drv_fc8080_check_overrun(uint8 op_mode);
void tunerbb_drv_fc8080_isr_control(fci_u8 onoff);
#ifdef FEATURE_RSSI_DEBUG
void tunerbb_drv_fc8080_get_dm(fci_u32 *mscber, fci_u32 *tp_err, fci_u16 *tpcnt, fci_u32 *vaber, fci_s8 *rssi);
#else
void tunerbb_drv_fc8080_get_dm(fci_u32 *mscber, fci_u32 *tp_err, fci_u16 *tpcnt, fci_u32 *vaber);
#endif

int8 tunerbb_drv_fc8080_power_on(void)
{
	return tdmb_fc8080_power_on();
}

int8 tunerbb_drv_fc8080_power_off(void)
{
	is_tdmb_probe = 0;
	return tdmb_fc8080_power_off();
}

int8 tunerbb_drv_fc8080_select_antenna(unsigned int sel)
{
	return tdmb_fc8080_select_antenna(sel);
}

int8 tunerbb_drv_fc8080_reset_ch(void)
{
	tunerbb_drv_fc8080_stop();

	return FC8080_RESULT_SUCCESS;
}

int8 tunerbb_drv_fc8080_re_syncdetector(uint8 op_mode)
{
	return FC8080_RESULT_SUCCESS;
}

int8 tunerbb_drv_fc8080_set_channel(int32 freq_num, uint8 subch_id, uint8 op_mode)
{
	int8 ret_val;

	ret_val = tunerbb_drv_fc8080_multi_set_channel(freq_num, 1, &subch_id, &op_mode);

	return ret_val;
}

void tunerbb_drv_fc8080_set_userstop(int mode)
{
	tdmb_fc8080_set_userstop(mode);
}

int tunerbb_drv_fc8080_is_on(void)
{
	return tdmb_fc8080_tdmb_is_on();
}

//        
static int32	tunerbb_drv_convert_chnum_to_freq(uint32 ch_num)
{
#ifdef FREQ_SEARCH_IN_TABLE
	int32		loop;
	int32		current_idx = 0;

#else
	uint32		ensemble_idx = (ch_num/10-TDMB_ENS_NUM);
	uint32		subch_idx =(ch_num%10 -1);
#endif

	if((ch_num < CH_LOW_NUM ) || (ch_num > CH_UPPER_NUM))
	{
		return 0;
	}

#ifdef FREQ_SEARCH_IN_TABLE
	for(loop = 0; loop < MAX_KOREABAND_FULL_CHANNEL; loop ++)
	{
		if(gKOREnsembleFullFreqTbl[loop][INDEX_KOR_CH_NUM_DEC] == (int32)ch_num)
		{
			current_idx = loop;
			break;
		}
	}

	if(loop >= MAX_KOREABAND_FULL_CHANNEL)
	{
		return 0;
	}

	return (gKOREnsembleFullFreqTbl[current_idx][INDEX_KOR_FREQ_NUM]);
#else
	return ((C7A_CEN_FREQ +  ENS_GAP_FREQ*ensemble_idx) + (CH_GAP_FREQ *subch_idx ));
#endif
}

//        
#if !defined(STREAM_TS_UPLOAD)
/*=======================================================
    Function 		: tunerbb_drv_fc8080_fic_cb
    Description		: set fic data param after ISR process
    Parameter		:
           uint32 userdata : Not Used
           uint8 *data : fic buffer address
           int length : fic data length
    Return Value	:
			           SUCCESS : 1
			           FAIL : 0 or negative interger (If there is error code)

	when		model	who			edit history
  -------------------------------------------------------
	2010/08/17	MOBIT	somesoo		Code review
======================================================== */
int tunerbb_drv_fc8080_fic_cb(uint32 userdata, uint8 *data, int length)
{
	fic_buffer.address = (uint32)(data);
	fic_buffer.length = length;
	fic_buffer.valid = 1;

	// FC8000 관련 code인 send_fic_int_sig_isr2task() in mbs_dshmain.c를 빼다 보니, 현재는 polling 방식이나 향후 ISR방식으로 적용시 필요하므로 feature를 추가함
#ifndef FEATURE_GET_FIC_POLLING
	send_fic_int_sig_isr2task();
#endif // FEATURE_GET_FIC_POLLING

	return FC8080_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int tunerbb_drv_fc8080_msc_cb(uint32 userdata, uint8 subChId, uint8 *data, int length)
    (1)   set msc data param after ISR process.
    (2)   Return Value
           SUCCESS : 1
           FAIL : 0 or negative interger (If there is error code)
    (3)   Argument
           uint32 userdata : Not Used
           uint8 subChId : Subchannel ID
           uint8 *data : msc buffer address
           int length : msc data length
---------------------------------------------------------------------------- */
int tunerbb_drv_fc8080_msc_cb(uint32 userdata, uint8 subChId, uint8 *data, int length)
{
	TDMB_BB_HEADER_TYPE dmb_header;
	uint16 head_size = 0;

	dmb_header.data_type = (serviceType[0] == FC8080_DAB?TDMB_BB_DATA_DAB:TDMB_BB_DATA_TS);
	dmb_header.size = length;
	dmb_header.subch_id = subChId;
	dmb_header.reserved = 0;//data_sequence_count++;//0xDEAD;
	dmb_header.ack_bit = 0;
	head_size = sizeof(TDMB_BB_HEADER_TYPE);

	/* TEST FOR AV Check  110407 */
	//printk("tunerbb_drv_fc8080_msc_cb data0[0x%x] data1[0x%x] data2[0x%x] data3[0x%x] \n", *(data), *(data+1), *(data+2), *(data+3));
	memcpy(&msc_data[0/*msc_buffer.length*/], &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
	memcpy(&msc_data[head_size], data, length);

	msc_buffer.length = head_size + length;
	msc_buffer.valid=1;

	return FC8080_RESULT_SUCCESS;
}
#endif
/*=======================================================
    Function 		: tunerbb_drv_fc8080_init
    Description		: Initializing the FC8080 Chip after power on
    Parameter		: VOID
    Return Value	:
           SUCCESS : 1
           FAIL : 0 or negative interger (If there is error code)

	when		model	who			edit history
  -------------------------------------------------------
	2010/05/17	MOBIT	prajuna		EBI2 configuration
	2010/05/31	MOBIT	prajuna		Removed test code
	2010/06/09	MOBIT	prajuna		TDMB porting(KB3 Rev. A patch)
	2010/07/15	MOBIT	prajuna		TDMB tuning for QSC
	2010/07/16	MOBIT	somesoo		TDMB tuning for QSC with FCI 최규원 과장
	2010/07/17	MOBIT	somesoo		TDMB porting(VG)
	2010/08/19	MOBIT	prajuna		Code review
	2010/09/10	MOBIT	prajuna		TDMB porting(Aloe)
======================================================== */
int8	tunerbb_drv_fc8080_init(void)
{
	uint8 res;
	/*test*/

	/*
	uint16 i;
	uint32 wdata = 0;
	uint32 ldata = 0;
	uint32 data = 0;
	uint32 temp = 0;
	*/

	/* Common Code */
#if defined(STREAM_SLAVE_PARALLEL_UPLOAD)
	/* EBI2 Specific Code */
	bbm_com_hostif_select(NULL, BBM_PPI);
#elif defined(STREAM_TS_UPLOAD)
	/* TSIF Specific Code */
	bbm_com_hostif_select(NULL, BBM_I2C);
#elif defined(STREAM_SPI_UPLOAD)
	/* SPI Specific. Code */
	bbm_com_hostif_select(NULL, BBM_SPI);
#else
#error code not present
#endif

#if !defined(STREAM_TS_UPLOAD)
	bbm_com_fic_callback_register((fci_u32)NULL, tunerbb_drv_fc8080_fic_cb);
	bbm_com_msc_callback_register((fci_u32)NULL, tunerbb_drv_fc8080_msc_cb);
#endif
	res = bbm_com_init(NULL);
	res |= bbm_com_probe(NULL);

	if(res)
	{
		is_tdmb_probe = 0;
		printk("fc8080 chip id read error , so is_tdmb_probe = %d\n", is_tdmb_probe);
		return FC8080_RESULT_ERROR;
	}
	else
	{
#if !defined(STREAM_TS_UPLOAD)
		memset((void*)&g_chinfo, 0xff, sizeof(g_chinfo));
		memset((void*)&msc_buffer, 0x00, sizeof(DATA_BUFFER));
		memset((void*)&fic_buffer, 0x00, sizeof(DATA_BUFFER));
#endif
	}

	is_tdmb_probe = 1;

	res = bbm_com_tuner_select(0, FC8080_TUNER, BAND3_TYPE);

#if 0      //fc8080 <-> Host(MSM) 간의 Interface TEST를 위한 code
/* test */
	for(i=0;i<5000;i++)
	{
//		dog_kick();
		bbm_com_write(NULL, 0xa4, i & 0xff);
		bbm_com_read(NULL, 0xa4, &data);
		if((i & 0xff) != data)
			printk("FC8080 byte test (0x%x,0x%x)\n", i & 0xff, data);
	}
	for(i=0;i<5000;i++)
	{
		bbm_com_word_write(NULL, 0xa4, i & 0xffff);
		bbm_com_word_read(NULL, 0xa4, &wdata);
		if((i & 0xffff) != wdata)
			printk("FC8080 word test (0x%x,0x%x)\n", i & 0xffff, wdata);
	}
	for(i=0;i<5000;i++)
	{
		bbm_com_long_write(NULL, 0xa4, i & 0xffffffff);
		bbm_com_long_read(NULL, 0xa4, &ldata);
		if((i & 0xffffffff) != ldata)
			printk("FC8080 long test (0x%x,0x%x)\n", i & 0xffffffff, ldata);
	}

	data = 0;

	for(i=0;i<5000;i++)
	{
	  temp = i&0xff;
		bbm_com_tuner_write(NULL, 0x58, 0x01, &temp, 0x01);
		bbm_com_tuner_read(NULL, 0x58, 0x01, &data, 0x01);
		if((i & 0xff) != data)
			printk("FC8080 tuner test (0x%x,0x%x)\n", i & 0xff, data);
	}
/* test */
#endif

	if(res)
	{
		printk("[FC8080] BBM_TUNER_SELECT Error = (%d)\n", res);
		return FC8080_RESULT_ERROR;
	}
	else
		return FC8080_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int8 tunrbb_drv_fc8080_stop(void)
    (1)   Stopping the FC8080 Chip Operation
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           VOID
---------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_stop(void)
{
	uint8 res;

	res=bbm_com_audio_deselect(0, 0, DAB_SVC_ID);
	res|=bbm_com_video_deselect(0, 0, DMB_SVC_ID, 0);
	res|=bbm_com_data_deselect(0, 0, DAT_SVC_ID);

	ms_must_wait(60);

#if !defined(STREAM_TS_UPLOAD)
	memset((void*)&g_chinfo, 0xff, sizeof(g_chinfo));
	memset((void*)&msc_buffer, 0x00, sizeof(DATA_BUFFER));
	memset((void*)&fic_buffer, 0x00, sizeof(DATA_BUFFER));
#endif

	if(res)
		return FC8080_RESULT_ERROR;
	else
		return FC8080_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_control_fic(void)
    (1)   fic interrupt control on/off
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           enable : on/off
---------------------------------------------------------------------------- */
int8 tunerbb_drv_fc8080_control_fic(uint8 enable)
{
	unsigned short mask;

	bbm_com_word_read(NULL, BBM_BUF_INT, &mask);

	if(enable ==1)
		mask |= 0x100;
	else
		mask &= ~0x100;

	bbm_com_word_write(NULL, BBM_BUF_INT, mask);

	return FC8080_RESULT_SUCCESS;
}

#if 0
static fci_u16 tunerbb_drv_fc8080_rserror_count(fci_u16 *nframe)//for dummy channel.
{
	fci_u16 rt_nframe, rt_rserror;

	bbm_com_write(NULL, 0xe01, 0x0f);

	bbm_com_word_read(NULL, 0xe30, &rt_nframe);
	bbm_com_word_read(NULL, 0xe32, &rt_rserror);

	*nframe = rt_nframe; //실시간으로 count 되는 frame 수
	return rt_rserror;
}
#endif

/*--------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_get_bbinfo(tdmb_status_rsp_type* dmb_bb_info)
    (1)   Getting the RF/BB Information
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
            tdmb_status_rsp_type* dmb_bb_info (IN/OUT)
           typedef struct tdmb_status_rsp_status
           {
                 uint32 dab_ok;
                 uint32 msc_ber;
                 uint32 sync_lock;
                 uint32 afc_ok;
                 uint32 cir;
                 uint32 fic_ber;
                 uint32 tp_lock;
                 uint32 sch_ber;
                 uint32 tp_err_cnt;
                 uint32 va_ber;
                 byte   srv_state_flag;
           };

           These paramters are dependent on Information supplied by Device.
---------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_get_ber(struct broadcast_tdmb_sig_info *dmb_bb_info)
{
	uint8 sync_status;
	uint32 tp_err_cnt=0;
#ifdef FEATURE_RSSI_DEBUG
	int8 rssi;
#endif

	uint16 nframe = 0;

	if(is_tdmb_probe == 0)
	{
		dmb_bb_info->msc_ber = 20000;
		dmb_bb_info->tp_err_cnt = 255;

		printk("is_tdmb_probe 0. so msc_ber is 20000, tp_err_cnt = 255. \n");
		return FC8080_RESULT_SUCCESS;
	}


	tunerbb_drv_fc8080_check_overrun(serviceType[0]);

#ifdef FEATURE_RSSI_DEBUG
	tunerbb_drv_fc8080_get_dm(&dmb_bb_info->msc_ber, &tp_err_cnt, &nframe, &dmb_bb_info->va_ber, &rssi);
#else
	tunerbb_drv_fc8080_get_dm(&dmb_bb_info->msc_ber, &tp_err_cnt, &nframe, &dmb_bb_info->va_ber);
#endif

	//dmb_bb_info->msc_ber = tunerbb_drv_fc8080_get_viterbi_ber();

	sync_status = tunerbb_drv_fc8080_get_sync_status();
#ifdef FEATURE_RSSI_DEBUG
	printk("[FC8080] sync_status = 0x%x, msc_ber = %d, tp_err_cnt = %d, nframe = %d, va_ber = %d rssi = %d\n", sync_status, dmb_bb_info->msc_ber, tp_err_cnt, nframe, dmb_bb_info->va_ber, rssi);
#endif

	dmb_bb_info->sync_lock = ((sync_status & 0x10) ? 1 : 0);
	dmb_bb_info->cir = ((sync_status & 0x08) ? 1 : 0);
	dmb_bb_info->afc_ok = (((sync_status & 0x06)==0x06) ? 1 : 0);

	if(dmb_bb_info->cir && dmb_bb_info->sync_lock)
	{
		dmb_bb_info->sch_ber = 1;
		// dab_ok : channel impulse response
		dmb_bb_info->dab_ok 	= 1;
	}
	else
	{
		dmb_bb_info->sch_ber = 0;
		// dab_ok : channel impulse response
		dmb_bb_info->dab_ok 	= 0;
	}

	if(serviceType[0] == FC8080_DMB || serviceType[0] == FC8080_VISUAL)
	{
		//tp_err_cnt = tunerbb_drv_fc8080_rserror_count(&nframe); //실시간 frame수 체크

		if((dmb_bb_info->sync_lock == 0) || (tp_total_cnt == 0))
		{
			dmb_bb_info->tp_err_cnt = 0;
			dmb_bb_info->tp_lock = 0;
		}
		else if(tp_err_cnt == 0)
		{
			dmb_bb_info->tp_err_cnt = 0;
			dmb_bb_info->tp_lock = 1;
		}
		else //if(bb_info.tp_err_cnt > 0)
		{
			dmb_bb_info->tp_err_cnt = (uint32)((tp_err_cnt *1000)/(3*tp_total_cnt));
			dmb_bb_info->tp_lock = 0;
		}

		// initialize information
		tp_total_cnt = 0;
		//dmb_bb_info->va_ber = tunerbb_drv_fc8080_get_rs_ber();
	}
	else
	{
		dmb_bb_info->tp_err_cnt = 0;
		dmb_bb_info->tp_lock	= 0;
		dmb_bb_info->va_ber = 0;
	}

	dmb_bb_info->fic_ber = 0;

	if(dmb_bb_info->msc_ber < 6000)
	{
		dmb_bb_info->antenna_level = 4;
	}
	else if(dmb_bb_info->msc_ber >= 6000 && dmb_bb_info->msc_ber < 8000)
	{
		dmb_bb_info->antenna_level = 3;
	}
	else if(dmb_bb_info->msc_ber >= 8000 && dmb_bb_info->msc_ber < 9000)
	{
		dmb_bb_info->antenna_level = 2;
	}
	else if(dmb_bb_info->msc_ber >= 9000 && dmb_bb_info->msc_ber < 12000)
	{
		dmb_bb_info->antenna_level = 1;
	}
	else if(dmb_bb_info->msc_ber >= 12000)
	{
		dmb_bb_info->antenna_level = 0;
	}

#if 0
	//채널은 잡았으나 (sync_status == 0x3f) frame이 들어오지 않는 경우(nframe == 0) - MBN V-Radio
	if((sync_status==0x3f)&&(nframe==0))
	{
		//antenna level을 0으로 만듬.
		dmb_bb_info->antenna_level = 0;
	}

	//antenna level이 0이면 약전계이므로 5분종료를 위해 dab_ok를 0으로 만듬.
	if(dmb_bb_info->antenna_level == 0)
	{
		dmb_bb_info->dab_ok = 0;
	}
#endif

	return FC8080_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_get_msc_ber(void)
    (1)   Getting the msc ber
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           uint32* pmsc_ber (IN/OUT)
---------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_get_msc_ber(uint32* pmsc_ber )
{
	*pmsc_ber = tunerbb_drv_fc8080_get_viterbi_ber();

	return FC8080_RESULT_SUCCESS;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_mulit_set_channel(int32 freq_num, uint8 subch_cnt, uint8 subch_id[ ], uint8 op_mode[ ])
    (1)   Setting the frequency , subch_id and op_mode.
            This function is used in Single Service and Mulitiple Service
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           int32 freq_num (IN)
                - TDMB Frequency index(e.g 7A(71), 13C(133) etc). Convert frequency if needed
           uint8 subch_cnt (IN)
                - The number of multiple service. This value is 1 in case of Single Service
           uint8 subch_id[ ] (IN)
                - Service Componet Sub-Channel ID
           uint8  op_mode[ ] (IN)
                - Service Operation Mode
                DAB  = 1;
                DMB = 2;
                VISUAL = 3;
                DATA = 4;
                TPEG = 5;
                ENSQUERY = 6

           <notice> The size of subch_cnt[ ] and op_mode[ ] is the maximum number being supported by FC8080
--------------------------------------------------------------------------------------- */
// Modified by somesoo 20100730 for removing green block effect
int8	tunerbb_drv_fc8080_multi_set_channel(int32 freq_num, uint8 subch_cnt, uint8 subch_id[ ], uint8 op_mode[ ])
{
	int8 res = BBM_OK;
	int32 freq = 0;
	uint8 dmb_cnt=0;
	int i;
	fc8080_service_type svcType = FC8080_SERVICE_MAX;
	unsigned short mask = 0;

	bbm_com_word_write(NULL, BBM_BUF_ENABLE, 0x0000);
	tunerbb_drv_fc8080_isr_control(0);

	for(i=0;i<subch_cnt;i++)
	{
		serviceType[i] = op_mode[i];

		if(FC8080_ENSQUERY == op_mode[i])
			svcType = FC8080_ENSQUERY;
	}

	tunerbb_drv_fc8080_control_fic(0);
	/* Change freq_num(channel num) to frequency */
	freq = tunerbb_drv_convert_chnum_to_freq(freq_num);

	if(freq == 0)
	{
		return FC8080_RESULT_ERROR;
	}

	res = bbm_com_tuner_set_freq(0, freq);

	if(res)
	{
		return FC8080_RESULT_ERROR;
	}

	if(svcType == FC8080_ENSQUERY)
	{

#ifdef FEATURE_FIC_BER
		bbm_com_write(0, 0xe12, 0x1f);
#endif

		if(bbm_com_scan_status(0))
		{
			return FC8080_RESULT_ERROR;
		}
	}

#ifdef FEATURE_FIC_BER
	else
		bbm_com_write(0, 0xe12, 0x3f);
#endif

	for(i=0;i<subch_cnt;i++)
	{
		switch(serviceType[i])
		{
			case FC8080_DAB:
				mask |= (1<<DAB_SVC_ID);
				res = bbm_com_audio_select(0, subch_id[i],DAB_SVC_ID);
#ifdef STREAM_TS_UPLOAD
				fc8080_demux_select_channel(subch_id[i], DAB_SVC_ID);
#else
				g_chinfo[subch_id[i]]=DAB_SVC_ID;
#endif
				break;
			case FC8080_DMB:
			case FC8080_VISUAL:
			case FC8080_BLT_TEST:
				mask |= (1 << (DMB_SVC_ID+dmb_cnt));
				if(dmb_cnt<2)
				{
					res = bbm_com_video_select(0, subch_id[i], DMB_SVC_ID+dmb_cnt, dmb_cnt);
#ifdef STREAM_TS_UPLOAD
					fc8080_demux_select_video(subch_id[i], DMB_SVC_ID+dmb_cnt);
#else
					g_chinfo[subch_id[i]]=dmb_cnt;
#endif
					//dmb_cnt++;
				}
				else
					res=BBM_NOK;
				break;
			case FC8080_DATA:
				mask |= (1<<DAT_SVC_ID);
				res = bbm_com_data_select(0, subch_id[i], DAT_SVC_ID);
#ifdef STREAM_TS_UPLOAD
				fc8080_demux_select_channel(subch_id[i], DAT_SVC_ID);
#else
				g_chinfo[subch_id[i]]=DAT_SVC_ID;
#endif
				break;
			case FC8080_ENSQUERY:
				tunerbb_drv_fc8080_control_fic(1);
				mask |= 0x100;
				res = BBM_OK;
				break;
			default:
				res = BBM_NOK;
				break;
		}
	}
	bbm_com_word_write(NULL, BBM_BUF_ENABLE, mask);
	tot_subch_cnt = subch_cnt;

	// Added by somesoo 20100730 for removing green block effect
	if(svcType != FC8080_ENSQUERY)
		tunerbb_drv_fc8080_isr_control(1);

	if(res)
		return FC8080_RESULT_ERROR;
	else
		return FC8080_RESULT_SUCCESS;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_get_fic(uint8* buffer, uint32* buffer_size)
    (1)   Getting the FIC data after calling tunerbb_drv_fc8080_multi_set_channel(freq, 1, ignore, ENSQUERY)
            In case of ENSQUERY, set_channel function must return Channel LOCKING or Not.
            Get_FIC is called in case of LOCKING Success
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           uint8* buffer (IN/OUT)
               - buffer for FIC data
           uint32* buffer_size (IN /OUT)
              - FIC Data Size

        <notice> This function is used in All HOST Interface
--------------------------------------------------------------------------------------- */
#ifdef FEATURE_GET_FIC_POLLING
int8	tunerbb_drv_fc8080_get_fic(uint8* buffer, uint32* buffer_size  /*, uint8 crc_on_off */)
{
	HANDLE hDevice = NULL;
	fci_u16      mfIntStatus = 0;

	if(buffer==NULL)
		return FC8080_RESULT_ERROR;

	bbm_com_word_read(hDevice, BBM_BUF_STATUS, &mfIntStatus);

	if(mfIntStatus == 0)
		return FC8080_RESULT_ERROR;

	bbm_com_word_write(hDevice, BBM_BUF_STATUS, mfIntStatus);

	if(mfIntStatus & 0x0100)
	{
		bbm_com_data(hDevice, BBM_RD_FIC, buffer, FIC_BUF_LENGTH/2);
		*buffer_size = FIC_BUF_LENGTH/2;
	}

	if(mfIntStatus & 0x0100)
		return FC8080_RESULT_SUCCESS;
	else
		return FC8080_RESULT_ERROR;

}
#else
#if !defined(STREAM_TS_UPLOAD)
int8	tunerbb_drv_fc8080_get_fic(uint8* buffer, uint32* buffer_size  /*, uint8 crc_on_off */)
{
	if(buffer==NULL)
		return FC8080_RESULT_ERROR;

	if(fic_buffer.valid && fic_buffer.length)
	{
		*buffer_size = fic_buffer.length;
		memcpy(buffer, (uint8*)fic_buffer.address, fic_buffer.length);
	}
	else
	{
		*buffer_size = 0;
	}

	fic_buffer.valid = 0;

	return FC8080_RESULT_SUCCESS;
}
#endif
#endif
#if !defined(STREAM_TS_UPLOAD)
/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_read_data(uint8* buffer, uint32* buffer_size)
    (1)   Reading MSC or MSC + FIC etc Data.
            This function is used in EBI2 HOST Interface
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           uint8* buffer (IN/OUT)
               - buffer for Data
           uint32* buffer_size (IN /OUT)
              - Data Size

        <notice> This function is used in only EBI2 HOST Interface
--------------------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_read_data(uint8* buffer, uint32* buffer_size)
{
	int8 retval = FC8080_RESULT_ERROR;

	if(buffer == NULL || buffer_size == NULL)
	{
		return retval;
	}

	/* initialize length and valid value before isr routine */
	msc_buffer.valid = 0;
	msc_buffer.length=0;
#ifdef NOT_MSCDATA_MULTIPLE_MEMCPY
	msc_data = buffer;
#endif

	fc8080_isr(NULL);

	if(msc_buffer.valid && msc_buffer.length)
	{
		*buffer_size = msc_buffer.length;
#ifndef NOT_MSCDATA_MULTIPLE_MEMCPY
		memcpy(buffer, &msc_data[0], msc_buffer.length);
#endif
		retval = FC8080_RESULT_SUCCESS;
	}

	return retval;
}
#endif
/*                                                                                     
                                                                                                                   
                                                                                                                           
                                                                                        
                      
                     
                                                                 
                  
                               
                                           
                                
                                                                                                                             
                                 
                                             
                                      
                                                                     

                
                                                                     
                                          
                            
                                                                                            
                                                  
                                                                                                                        
                                                                                        */
#ifdef STREAM_TS_UPLOAD
int8	tunerbb_drv_fc8080_process_multi_data(uint8 subch_cnt, uint8* input_buf, uint32 input_size, uint32* read_size)
{
	uint32 fic_len=0, nvideo_len=0, video_len=0, data_len=0;
	uint8 i, header_cnt=0;

	if(input_buf == NULL || read_size == NULL)
	{
		return FC8080_RESULT_ERROR;
	}

	fc8080_demux(input_buf, input_size);

	if(!fc8080_get_ts_datalen(TS_DAT_FIC, 0, &fic_len))
	{
		data_len=fic_len;
		header_cnt++;
	}

	for(i=0;i<2;i++)
	{
		if(!fc8080_get_ts_datalen(TS_DAT_VIDEO_I, i, &nvideo_len))
		{
			data_len+=nvideo_len;
			header_cnt++;
		}
	}

	for(i=0;i<8;i++)
	{
		if(!fc8080_get_ts_datalen(TS_DAT_NVIDEO, i, &video_len))
		{
			data_len+=video_len;
			header_cnt++;
		}
	}

	data_len+=sizeof(TDMB_BB_HEADER_TYPE)*header_cnt;
	*read_size=data_len;

	return FC8080_RESULT_SUCCESS;
}
#else
int8	tunerbb_drv_fc8080_process_multi_data(uint8 subch_cnt, uint8* input_buf, uint32 input_size, uint32* read_size)
{
	uint32 i=0;
	FCI_HEADER_TYPE header;
	uint8 ch_cnt=0;

	(*read_size) = 0;

	// Modified by FCI 20100309 for DAB error fatal(TD10172)
	// Modified by somesoo 20100831 for DAB error fatal(TD10172)
	for(ch_cnt = 0; ch_cnt<tot_subch_cnt; ch_cnt++)
	{
		memcpy(&header, &input_buf[i], sizeof(FCI_HEADER_TYPE));
		memcpy(&msc_multi_data[i], &input_buf[i], sizeof(FCI_HEADER_TYPE));
		i+=sizeof(FCI_HEADER_TYPE);
		memcpy(&msc_multi_data[i], &input_buf[i], header.length);
		i+=header.length;

		(*read_size) += header.length;
		(*read_size) += sizeof(TDMB_BB_HEADER_TYPE);
	}

	return FC8080_RESULT_SUCCESS;
}
#endif

#ifdef STREAM_TS_UPLOAD
/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_get_multi_data(uint8 subch_cnt, uint8* buf_ptr, uint32 buf_size)
    (1)   Getting the processed data.
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           uint8 subch_cnt
               - Sub-Channel count
           uint8* buf_ptr (IN/OUT)
               - buffer for Data
           uint32 buffer_size (IN)
              - Data Size

        <notice>
             (1) format : dmb_header |data|dmb_header|data
--------------------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_get_multi_data(uint8 subch_cnt, uint8* buf_ptr, uint32 buf_size)
{
	uint32 nDataSize;
	int i;
	TDMB_BB_HEADER_TYPE dmb_header;
	uint32 read_size = 0;

	if(buf_ptr == NULL || buf_size == 0)
	{
		return FC8080_RESULT_ERROR;
	}

	if(!fc8080_get_ts_datalen(TS_DAT_FIC, 0, &nDataSize))
	{
		dmb_header.reserved = 0xFC85;
		dmb_header.data_type = TDMB_BB_DATA_FIC;
		dmb_header.size = nDataSize;
		dmb_header.subch_id = 0;
		dmb_header.ack_bit = 0;
		memcpy(buf_ptr, &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
		buf_ptr += sizeof(TDMB_BB_HEADER_TYPE);
		read_size += sizeof(TDMB_BB_HEADER_TYPE);
		fc8080_get_multi_data(TS_DAT_FIC, 0, buf_ptr, &dmb_header.subch_id);
		buf_ptr += nDataSize;
		read_size += nDataSize;
	}

	for(i=0;i<2;i++)
	{
		if(!fc8080_get_ts_datalen(TS_DAT_VIDEO_I, i, &nDataSize))
		{
			dmb_header.reserved = 0xFC85;
			dmb_header.data_type = TDMB_BB_DATA_TS;
			dmb_header.size = nDataSize;
			dmb_header.subch_id = 0;
			dmb_header.ack_bit = 0;
			memcpy(buf_ptr, &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
			buf_ptr += sizeof(TDMB_BB_HEADER_TYPE);
			read_size += sizeof(TDMB_BB_HEADER_TYPE);
			fc8080_get_multi_data(TS_DAT_VIDEO_I, i, buf_ptr, &dmb_header.subch_id);
			buf_ptr += nDataSize;
			read_size += nDataSize;
		}
	}

	for(i=0;i<8;i++)
	{
		if(!fc8080_get_ts_datalen(TS_DAT_NVIDEO, i, &nDataSize))
		{
			dmb_header.reserved = 0xFC85;
			if(i==DAB_SVC_ID)
				dmb_header.data_type = TDMB_BB_DATA_DAB;
			else if(i==DAT_SVC_ID)
				dmb_header.data_type = TDMB_BB_DATA_PACK;
			dmb_header.size = nDataSize;
			dmb_header.subch_id = 0;
			dmb_header.ack_bit = 0;
			memcpy(buf_ptr, &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
			buf_ptr += sizeof(TDMB_BB_HEADER_TYPE);
			read_size += sizeof(TDMB_BB_HEADER_TYPE);
			fc8080_get_multi_data(TS_DAT_NVIDEO, i, buf_ptr, &dmb_header.subch_id);
			buf_ptr += nDataSize;
			read_size += nDataSize;
		}
	}

	if(read_size != buf_size)
	{
		return FC8080_RESULT_ERROR;
	}
	else
	{
		return FC8080_RESULT_SUCCESS;
	}
}
#else
int8	tunerbb_drv_fc8080_get_multi_data(uint8 subch_cnt, uint8* buf_ptr, uint32 buf_size)
{
	uint32 nDataSize;
	int i=0;
	TDMB_BB_HEADER_TYPE dmb_header;
	uint32 read_size = 0;
	FCI_HEADER_TYPE header;
	uint8 ch_cnt=0;

	if(buf_ptr == NULL || buf_size == 0)
	{
		return FC8080_RESULT_ERROR;
	}

	// Modified by FCI 20100309 for DAB error fatal(TD10172)
	// Modified by somesoo 20100831 for DAB error fatal(TD10172)
	for(ch_cnt = 0; ch_cnt<tot_subch_cnt; ch_cnt++)
	{
		memcpy(&header, &msc_multi_data[i], sizeof(FCI_HEADER_TYPE));

		dmb_header.reserved = 0xFC85;
		if(header.svc_id==DMB_SVC_ID)
			dmb_header.data_type = TDMB_BB_DATA_TS;
		else if(header.svc_id==DAB_SVC_ID)
			dmb_header.data_type = TDMB_BB_DATA_DAB;
		else if(header.svc_id==DAT_SVC_ID)
			dmb_header.data_type = TDMB_BB_DATA_PACK;
		dmb_header.size = header.length;
		dmb_header.subch_id = header.subch_id;
		dmb_header.ack_bit = 0;

		i+=sizeof(FCI_HEADER_TYPE);
		memcpy(buf_ptr, &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
		buf_ptr += sizeof(TDMB_BB_HEADER_TYPE);
		read_size += sizeof(TDMB_BB_HEADER_TYPE);

		memcpy(buf_ptr, &msc_multi_data[i], header.length);
		nDataSize=header.length;
		buf_ptr += nDataSize;
		read_size += nDataSize;
		i+=(header.length+sizeof(TDMB_BB_HEADER_TYPE)-sizeof(FCI_HEADER_TYPE));
	}

	if(read_size != buf_size)
	{
		return FC8080_RESULT_ERROR;
	}
	else
	{
		return FC8080_RESULT_SUCCESS;
	}
}
#endif

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_start_tii(void)
    (1)   Starting TII
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           VOID
--------------------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_start_tii(void)
{
	return FC8080_RESULT_ERROR;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_stop_tii(void)
    (1)   Stopping TII
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           VOID
--------------------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_stop_tii(void)
{
	return FC8080_RESULT_ERROR;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8080_check_tii(uint8* main_tii_ptr, uint8* sub_tii_ptr)
    (1)   Stopping TII
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           uint8* main_tii_ptr
              - Main TII value
           uint8* sub_tii_ptr
              - SUB TII value
--------------------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8080_check_tii(uint8* main_tii_ptr, uint8* sub_tii_ptr)
{
	if(( NULL == main_tii_ptr) ||( NULL == sub_tii_ptr))
	{
		return FC8080_RESULT_ERROR;
	}

	*main_tii_ptr = 0xFF;
	*sub_tii_ptr = 0xFF;

	return FC8080_RESULT_ERROR;
}

#ifdef FEATURE_RSSI_DEBUG
void tunerbb_drv_fc8080_get_dm(fci_u32 *mscber, fci_u32 *tp_err, fci_u16 *tpcnt, fci_u32 *vaber, fci_s8 *rssi)
#else
void tunerbb_drv_fc8080_get_dm(fci_u32 *mscber, fci_u32 *tp_err, fci_u16 *tpcnt, fci_u32 *vaber)
#endif
{
#ifdef FEATURE_RSSI_DEBUG
	fci_s8 rssi_value;
#endif

	struct dm_st {
		fci_u32 start;

		fci_u16 vit_ber_rxd_rsps;
		fci_u16 vit_ber_err_rsps;
		fci_u32 vit_ber_err_bits;

		fci_u32 dmp_ber_rxd_bits;
		fci_u32 dmp_ber_err_bits;

		fci_u16 ber_rxd_rsps;
		fci_u16 ber_err_rsps;
		fci_u32 ber_err_bits;
	};

	struct dm_st dm;

	bbm_com_bulk_read(NULL, BBM_DM, (fci_u8*) &dm, sizeof(dm));

#ifdef FEATURE_RSSI_DEBUG
	rssi_value = (fci_s8) ((dm.ber_err_bits & 0xff000000) >> 24);
#endif

	dm.ber_err_bits &= 0x00ffffff;
	dm.dmp_ber_err_bits &= 0x00ffffff;


	if(dm.dmp_ber_rxd_bits == 0)
		*mscber = MAX_MSC_BER;
	else if(dm.dmp_ber_err_bits == 0)
		*mscber = 0;
	else
	{
		if(dm.dmp_ber_err_bits > 42949)
			*mscber = ((dm.dmp_ber_err_bits * 1000)/dm.dmp_ber_rxd_bits)*100;
		else
			*mscber = (dm.dmp_ber_err_bits*100000)/dm.dmp_ber_rxd_bits;
	}

	*mscber = (*mscber >= MAX_MSC_BER) ? MAX_MSC_BER : *mscber;

	/* ber must bigger than 0 because FactoryTest issue */
	if(*mscber == 0)
	{
		*mscber = 1;
	}

	if(dm.ber_rxd_rsps == 0)
		*vaber = MAX_VA_BER;

	else if((dm.ber_err_bits == 0) && (dm.ber_err_rsps == 0))
	{
		*vaber = 0;
	}
	else
	{
		if(dm.ber_err_bits > 42949)
			*vaber = ((dm.ber_err_bits * 1000)/(dm.ber_rxd_rsps * 204 * 8))*100;
		else
			*vaber = (dm.ber_err_bits*100000)/(dm.ber_rxd_rsps * 204 * 8);
	}

	*vaber = (*vaber >= MAX_VA_BER) ? MAX_VA_BER : *vaber;

	*tp_err = dm.ber_err_rsps;
	*tpcnt = dm.ber_rxd_rsps;
#ifdef FEATURE_RSSI_DEBUG
	*rssi = rssi_value;
#endif
}


static uint32 tunerbb_drv_fc8080_get_viterbi_ber(void)	//msc_ber
{
	uint32 bper, tbe;
	uint32 ber;

	bbm_com_write(NULL, 0xe01, 0x04);

	bbm_com_long_read(NULL, 0xe40, &bper);
	bbm_com_long_read(NULL, 0xe44, &tbe);

	if(bper == 0)
		ber = MAX_MSC_BER;
	else if(tbe == 0)
		ber = 0;
	else
	{
		if(tbe > 42949)
		{
			ber = ((tbe * 1000)/bper)*100;
		}
		else
		{
			ber = (tbe*100000)/bper;
		}
	}

	ber = (ber >= MAX_MSC_BER) ? MAX_MSC_BER : ber;

	/* ber must bigger than 0 because FactoryTest issue */
	if(ber == 0)
	{
		ber = 1;
	}

	return ber;

}

static int8 tunerbb_drv_fc8080_get_sync_status(void)
{
	uint8 sync_status;

	bbm_com_read(0, BBM_SYNC_STAT, &sync_status);

	return sync_status;
}

#if 0
static uint32 tunerbb_drv_fc8080_get_rs_ber(void)	//va_ber
{
	uint16 nframe, rserror;
	uint32 esum;
	uint32 ber;

	bbm_com_write(NULL, 0xe01, 0x01);

	bbm_com_word_read(NULL, 0xe30, &nframe);
	bbm_com_word_read(NULL, 0xe32, &rserror);
	bbm_com_long_read(NULL, 0xe34, &esum);

	if(nframe == 0)
		ber = MAX_VA_BER;

	else if((esum == 0) && (rserror == 0))
	{
		ber = 0;
	}
	else
	{
		if(esum > 42949)
			ber = ((esum * 1000)/(nframe * 204 * 8))*100;
		else
			ber = (esum*100000)/(nframe * 204 * 8);
	}

	ber = (ber >= MAX_VA_BER) ? MAX_VA_BER : ber;

	return ber;

}
#endif

/*
static fci_u8 ficBuffer[1024];
extern int (*pFicCallback)(fci_u32 userdata, fci_u8 *data, int length);
extern fci_u32 gFicUserData;

void tunerbb_drv_fc8080_process_polling_data()
{
	HANDLE hDevice = NULL;
	fci_u16      mfIntStatus = 0;
	fci_u16      size;
	int i;

	bbm_com_write(hDevice, BBM_COM_INT_ENABLE, 0x00);
	bbm_com_write(hDevice, BBM_COM_STATUS_ENABLE, 0x00);

	bbm_com_word_read(hDevice, BBM_BUF_INT, 0x01ff);
	bbm_com_word_read(hDevice, BBM_BUF_ENABLE, 0x01ff);

	for(i = 0 ; i < 200 ; i++)
	{
		bbm_com_word_read(hDevice, BBM_BUF_STATUS, &mfIntStatus);

		if(mfIntStatus)
			break;
	}

	if(mfIntStatus == 0)
	{
		bbm_com_word_read(hDevice, BBM_BUF_INT, 0x00ff);
		bbm_com_word_read(hDevice, BBM_BUF_ENABLE, 0x00ff);
		bbm_com_write(hDevice, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
		bbm_com_write(hDevice, BBM_COM_STATUS_ENABLE, ENABLE_INT_MASK);
		return;
	}

	bbm_com_word_write(hDevice, BBM_BUF_STATUS, mfIntStatus);
	bbm_com_word_write(hDevice, BBM_BUF_STATUS, 0x0000);

	if(mfIntStatus & 0x0100)
	{
		bbm_com_word_read(hDevice, BBM_BUF_FIC_THR, &size);
		size += 1;
		if(size-1)
		{
			bbm_com_data(hDevice, BBM_COM_FIC_DATA, &ficBuffer[0], size);

			if(pFicCallback)
			      (*pFicCallback)(gFicUserData, &ficBuffer[0], size);
		}
	}
	bbm_com_word_read(hDevice, BBM_BUF_INT, 0x00ff);
	bbm_com_word_read(hDevice, BBM_BUF_ENABLE, 0x00ff);

	bbm_com_write(hDevice, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
	bbm_com_write(hDevice, BBM_COM_STATUS_ENABLE, ENABLE_INT_MASK);
}
*/

static int8 tunerbb_drv_fc8080_check_overrun(uint8 op_mode)
{
	uint16 mfoverStatus;
	uint8 mask;

	if(op_mode == FC8080_DAB)
	{
		mask = 0x04;
	}
	else if(op_mode == FC8080_DMB || op_mode == FC8080_VISUAL)
	{
		mask = 0x01;
	}
	else
	{
		//printk("fc8080 invaild op_mode %d\n", op_mode);
		return FC8080_RESULT_ERROR; /* invaild op_mode */
	}

	{
		bbm_com_word_read(NULL, BBM_BUF_OVERRUN, &mfoverStatus);

		if(mfoverStatus & mask)
		{
			bbm_com_word_write(NULL, BBM_BUF_OVERRUN, mfoverStatus);

			printk("======== FC8080  OvernRun and Buffer Reset Done mask (0x%X) over (0x%X) =======\n", mask,mfoverStatus );
		}
	}

	return FC8080_RESULT_SUCCESS;
}

void tunerbb_drv_fc8080_isr_control(fci_u8 onoff)
{
	if(onoff)
		bbm_com_write(0, BBM_MD_INT_EN, BBM_MF_INT);
	else
		bbm_com_write(0, BBM_MD_INT_EN, 0);
}
