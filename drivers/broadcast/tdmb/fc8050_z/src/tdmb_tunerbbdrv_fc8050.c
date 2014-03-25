/*****************************************************************************
 Copyright(c) 2009 LG Electronics Inc. All Rights Reserved
 
 File name : Tunerbb_drv_fc8050.c
 
 Description : fc8050 made by FCI Driver Code
 
 History : 
 ----------------------------------------------------------------------
 Dec. 25,  2009 :      FCI release for LG MC
*******************************************************************************/
#include "../inc/fci_tun.h"
#include "../inc/fc8050_regs.h"
#include "../inc/fc8050_demux.h"
#include "../inc/fc8050_isr.h"
#include "../inc/bbm.h"
#include "../inc/fci_types.h"
#include "../inc/tdmb_tunerbbdrv_fc8050def.h"

#include "../inc/broadcast_fc8050.h"

#include <linux/string.h>
/*============================================================
**    1.   DEFINITIONS

*============================================================*/
/* Example of Return value */
#define FC8050_RESULT_ERROR		(int8) 0
#define FC8050_RESULT_SUCCESS	(int8) 1

#define MON_BURST_MODE
#undef FEATURE_FIC_BER
#define LOCK_TIME_TUNING0	/* Fast Channel Scan */

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
#define DAB_SVC_ID 3
#define DAT_SVC_ID 2
#define FEATURE_GET_FIC_POLLING

/* change memcpy mscBuffer -> msc_data -> buffer  to mscBuffer->buffer */
#define NOT_MSCDATA_MULTIPLE_MEMCPY

uint32 	tp_total_cnt=0;

/* -----------------------------------------------------------------------
< HOST Interface FEATURE Usage>
  1. STREAM_TS_UPLOAD  : HOST Interface between MSM and FC8050 is TSIF(I2C)
  2. STREAM_SLAVE_PARALLEL_UPLOAD : HOST Interface between MSM and FC8050 is EBI2
  3. STREAM_SPI_UPLOAD : HOST Interface between MSM and FC8050 is SPI
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

extern int tunerbb_drv_fc8050_fic_cb(uint32 userdata, uint8 *data, int length);
extern int tunerbb_drv_fc8050_msc_cb(uint32 userdata, uint8 subChId, uint8 *data, int length);

extern void fc8050_isr_control(uint8 onoff);
extern void fc8050_isr_interruptclear(void);

/*============================================================
**    4.   Local constant variables
*============================================================*/


/*============================================================
**    5.   Local Typedef
*============================================================*/
typedef enum	fc8050_service_type
{
	FC8050_DAB = 1,
	FC8050_DMB = 2,
	FC8050_VISUAL =3,
	FC8050_DATA,
	FC8050_ENSQUERY = 6,	/*           */
	FC8050_BLT_TEST = 9,
	FC8050_SERVICE_MAX
} fc8050_service_type;

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
fc8050_service_type	serviceType_fci[FC8050_SERVICE_MAX];
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
static uint32 tunerbb_drv_fc8050_get_viterbi_ber(void);
static int8 tunerbb_drv_fc8050_get_sync_status(void);
static uint32 tunerbb_drv_fc8050_get_rs_ber(void);
static int8 tunerbb_drv_fc8050_check_overrun(uint8 op_mode);

int8 tunerbb_drv_fc8050_power_on(void)
{
	return tdmb_fc8050_power_on();
}

int8 tunerbb_drv_fc8050_power_off(void)
{
	is_tdmb_probe = 0;
	return tdmb_fc8050_power_off();
}

int8 tunerbb_drv_fc8050_select_antenna(unsigned int sel)
{
	return tdmb_fc8050_select_antenna(sel);
}

int8 tunerbb_drv_fc8050_reset_ch(void)
{
	//return INTERFACE_RESET_CH(TDMB_RFBB_DEV_ADDR);
	return FC8050_RESULT_SUCCESS;
}

int8 tunerbb_drv_fc8050_re_syncdetector(uint8 op_mode)
{
	return FC8050_RESULT_SUCCESS;
}

int8 tunerbb_drv_fc8050_set_channel(int32 freq_num, uint8 subch_id, uint8 op_mode)
{
	int8 ret_val;

	ret_val = tunerbb_drv_fc8050_multi_set_channel(freq_num, 1, &subch_id, &op_mode);

	return ret_val;
}

void tunerbb_drv_fc8050_set_userstop(int mode)
{
	tdmb_fc8050_set_userstop(mode);
}

int tunerbb_drv_fc8050_is_on(void)
{
	return tdmb_fc8050_tdmb_is_on();
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
/*======================================================= 
    Function 		: tunerbb_drv_fc8050_fic_cb
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
int tunerbb_drv_fc8050_fic_cb(uint32 userdata, uint8 *data, int length)
{
	fic_buffer.address = (uint32)(data);
	fic_buffer.length = length;
	fic_buffer.valid = 1;

	// FC8000 관련 code인 send_fic_int_sig_isr2task() in mbs_dshmain.c를 빼다 보니, 현재는 polling 방식이나 향후 ISR방식으로 적용시 필요하므로 feature를 추가함
#ifndef FEATURE_GET_FIC_POLLING
	send_fic_int_sig_isr2task();
#endif // FEATURE_GET_FIC_POLLING

	return FC8050_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int tunerbb_drv_fc8050_msc_cb(uint32 userdata, uint8 subChId, uint8 *data, int length)
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
int tunerbb_drv_fc8050_msc_cb(uint32 userdata, uint8 subChId, uint8 *data, int length)
{
	TDMB_BB_HEADER_TYPE dmb_header;
	uint16 head_size = 0;

	dmb_header.data_type = (serviceType_fci[0] == FC8050_DAB?TDMB_BB_DATA_DAB:TDMB_BB_DATA_TS);
	dmb_header.size = length;
	dmb_header.subch_id = subChId;
	dmb_header.reserved = 0;//data_sequence_count++;//0xDEAD;
	dmb_header.ack_bit = 0;
	head_size = sizeof(TDMB_BB_HEADER_TYPE);

	/* TEST FOR AV Check  110407 */
	//printk("tunerbb_drv_fc8050_msc_cb data0[0x%x] data1[0x%x] data2[0x%x] data3[0x%x] \n", *(data), *(data+1), *(data+2), *(data+3));
	memcpy(&msc_data[0/*msc_buffer.length*/], &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
	memcpy(&msc_data[head_size], data, length);

	msc_buffer.length = head_size + length;
	msc_buffer.valid=1;

	return FC8050_RESULT_SUCCESS;
}

/*======================================================= 
    Function 		: tunerbb_drv_fc8050_init
    Description		: Initializing the FC8050 Chip after power on
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
int8	tunerbb_drv_fc8050_init(void)
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
	BBM_HOSTIF_SELECT(NULL, BBM_PPI);
#elif defined(STREAM_TS_UPLOAD)
	/* TSIF Specific Code */
	BBM_HOSTIF_SELECT(NULL, BBM_I2C);
#elif defined(STREAM_SPI_UPLOAD)
	/* SPI Specific. Code */
	BBM_HOSTIF_SELECT(NULL, BBM_SPI);
#else
#error code not present
#endif

	BBM_FIC_CALLBACK_REGISTER((fci_u32)NULL, tunerbb_drv_fc8050_fic_cb);
	BBM_MSC_CALLBACK_REGISTER((fci_u32)NULL, tunerbb_drv_fc8050_msc_cb);

	res = BBM_INIT(NULL);
	res |= BBM_PROBE(NULL);

	if(res)
	{
		is_tdmb_probe = 0;
		printk("fc8050 chip id read error , so is_tdmb_probe = %d\n", is_tdmb_probe);
		return FC8050_RESULT_ERROR;
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

	res = BBM_TUNER_SELECT(0, FC8050_TUNER, BAND3_TYPE);

#if 0      //fc8050 <-> Host(MSM) 간의 Interface TEST를 위한 code
/* test */
	for(i=0;i<5000;i++)
	{
//		dog_kick();
		BBM_WRITE(NULL, 0x05, i & 0xff);
		BBM_READ(NULL, 0x05, &data);
		if((i & 0xff) != data)
			printk("FC8000 byte test (0x%x,0x%x)\n", i & 0xff, data);
	}
	for(i=0;i<5000;i++)
	{
		BBM_WORD_WRITE(NULL, 0x0210, i & 0xffff);
		BBM_WORD_READ(NULL, 0x0210, &wdata);
		if((i & 0xffff) != wdata)
			printk("FC8000 word test (0x%x,0x%x)\n", i & 0xffff, wdata);
	}
	for(i=0;i<5000;i++)
	{
		BBM_LONG_WRITE(NULL, 0x0210, i & 0xffffffff);
		BBM_LONG_READ(NULL, 0x0210, &ldata);
		if((i & 0xffffffff) != ldata)
			printk("FC8000 long test (0x%x,0x%x)\n", i & 0xffffffff, ldata);
	}

	data = 0;

	for(i=0;i<5000;i++)
	{
	  temp = i&0xff;
		BBM_TUNER_WRITE(NULL, 0x12, 0x01, &temp, 0x01);
		BBM_TUNER_READ(NULL, 0x12, 0x01, &data, 0x01);
		if((i & 0xff) != data)
			printk("FC8000 tuner test (0x%x,0x%x)\n", i & 0xff, data);
	}
	temp = 0x51;
	BBM_TUNER_WRITE(NULL, 0x12, 0x01, &temp, 0x01 );
/* test */
#endif

	if(res)
	{
		printk("[FC8050] BBM_TUNER_SELECT Error = (%d)\n", res);
		return FC8050_RESULT_ERROR;
	}
	else
		return FC8050_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int8 tunrbb_drv_fc8050_stop(void)
    (1)   Stopping the FC8050 Chip Operation 
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           VOID
---------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8050_stop(void)
{
	uint8 res;

	res=BBM_AUDIO_DESELECT(0, 0, DAB_SVC_ID);
	res|=BBM_VIDEO_DESELECT(0, 0, DMB_SVC_ID, 0);
	res|=BBM_VIDEO_DESELECT(0, 0, DMB_SVC_ID+1, 1);
	res|=BBM_DATA_DESELECT(0, 0, DAT_SVC_ID);

#if !defined(STREAM_TS_UPLOAD)
	memset((void*)&g_chinfo, 0xff, sizeof(g_chinfo));
	memset((void*)&msc_buffer, 0x00, sizeof(DATA_BUFFER));
	memset((void*)&fic_buffer, 0x00, sizeof(DATA_BUFFER));
#endif

	if(res)
		return FC8050_RESULT_ERROR;
	else
		return FC8050_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_control_fic(void)
    (1)   fic interrupt control on/off
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           enable : on/off
---------------------------------------------------------------------------- */
int8 tunerbb_drv_fc8050_control_fic(uint8 enable)
{
	unsigned short mask;

	BBM_WORD_READ(NULL, BBM_BUF_INT, &mask);

	if(enable ==1)
		mask |= 0x100;
	else
		mask &= ~0x100;

	BBM_WORD_WRITE(NULL, BBM_BUF_INT, mask);

	return FC8050_RESULT_SUCCESS;
}


//static fci_u16 tunerbb_drv_fc8050_rserror_count(void)
static fci_u16 tunerbb_drv_fc8050_rserror_count(fci_u16 *nframe)//for dummy channel.
{
	//fci_u32 rt_esum;
	fci_u16 rt_nframe, rt_rserror;
	fci_u8  rs_ctrl=0;

#ifdef MON_BURST_MODE
	uint8 burst[12];

	rs_ctrl = 0x21;
	BBM_WRITE(0, BBM_RS_CONTROL, rs_ctrl);
	BBM_BULK_READ(0, BBM_RS_RT_BER_PER, &burst[0], 4);
	//BBM_LONG_READ(0, BBM_RS_RT_BER_PER, burst[0]);
	rt_nframe = (uint16)(*(uint16*)burst);
	rt_rserror = (uint16)(*((uint16*)burst+1));

	rs_ctrl = 0x1;
	BBM_WRITE(0, BBM_RS_CONTROL, rs_ctrl);
#else
	BBM_READ(0, BBM_RS_CONTROL, &rs_ctrl);
	rs_ctrl |= 0x20;
	BBM_WRITE(0, BBM_RS_CONTROL, rs_ctrl);

	BBM_WORD_READ(0, BBM_RS_RT_BER_PER, &rt_nframe);	//실시간으로 count 되는 frame 수
	//BBM_LONG_READ(0, BBM_RS_RT_ERR_SUM, &rt_esum);
	BBM_WORD_READ(0, BBM_RS_RT_FAIL_CNT, &rt_rserror);

	rs_ctrl &= ~0x20;
	BBM_WRITE(0, BBM_RS_CONTROL, rs_ctrl);
#endif

	*nframe=rt_nframe; //실시간으로 count 되는 frame 수
	return rt_rserror;
}

/*--------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_get_bbinfo(tdmb_status_rsp_type* dmb_bb_info)
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
int8	tunerbb_drv_fc8050_get_ber(struct broadcast_tdmb_sig_info *dmb_bb_info)
{
	uint8 sync_status;
	uint32 tp_err_cnt=0;

	uint16 nframe = 0;

	//fc8050_isr_control(0);

	if(is_tdmb_probe == 0)
	{
		dmb_bb_info->msc_ber = 20000;
		dmb_bb_info->tp_err_cnt = 255;

		printk("is_tdmb_probe 0. so msc_ber is 20000, tp_err_cnt = 255. \n");
		return FC8050_RESULT_SUCCESS;
	}


	tunerbb_drv_fc8050_check_overrun(serviceType_fci[0]);

	dmb_bb_info->msc_ber = tunerbb_drv_fc8050_get_viterbi_ber();

	sync_status = tunerbb_drv_fc8050_get_sync_status();
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

	if(serviceType_fci[0] == FC8050_DMB || serviceType_fci[0] == FC8050_VISUAL)
	{
		tp_err_cnt = tunerbb_drv_fc8050_rserror_count(&nframe); //실시간 frame수 체크

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
		dmb_bb_info->va_ber = tunerbb_drv_fc8050_get_rs_ber();
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
	//fc8050_isr_control(1);

	return FC8050_RESULT_SUCCESS;
}

/*--------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_get_msc_ber(void)
    (1)   Getting the msc ber
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           uint32* pmsc_ber (IN/OUT)
---------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8050_get_msc_ber(uint32* pmsc_ber )
{
	*pmsc_ber = tunerbb_drv_fc8050_get_viterbi_ber();

	return FC8050_RESULT_SUCCESS;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_mulit_set_channel(int32 freq_num, uint8 subch_cnt, uint8 subch_id[ ], uint8 op_mode[ ])
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

           <notice> The size of subch_cnt[ ] and op_mode[ ] is the maximum number being supported by FC8050
--------------------------------------------------------------------------------------- */
// Modified by somesoo 20100730 for removing green block effect
int8	tunerbb_drv_fc8050_multi_set_channel(int32 freq_num, uint8 subch_cnt, uint8 subch_id[ ], uint8 op_mode[ ])
{
	int8 res = BBM_OK;
	int32 freq = 0;
	uint8 dmb_cnt=0;
	int i;
	fc8050_service_type svcType = FC8050_SERVICE_MAX;
	unsigned short mask;

	// Added by somesoo 20100730 for removing green block effect
	fc8050_isr_control(0);

	for(i=0;i<subch_cnt;i++)
	{
		serviceType_fci[i] = op_mode[i];

		if(FC8050_ENSQUERY != op_mode[i])
			tunerbb_drv_fc8050_stop();
		else
			svcType = FC8050_ENSQUERY;
	}

	tunerbb_drv_fc8050_control_fic(0);
	/* Change freq_num(channel num) to frequency */
	freq = tunerbb_drv_convert_chnum_to_freq(freq_num);

	if(freq == 0)
	{
		return FC8050_RESULT_ERROR;
	}

	res = BBM_TUNER_SET_FREQ(0, freq);

	if(res)
	{
		return FC8050_RESULT_ERROR;
	}

	if(svcType == FC8050_ENSQUERY)
	{

#ifdef FEATURE_FIC_BER
		BBM_WRITE(0, BBM_VT_CONTROL, 0x01);
#endif

		if(BBM_SCAN_STATUS(0))
		{
			return FC8050_RESULT_ERROR;
		}
	}

#ifdef FEATURE_FIC_BER
	else
		BBM_WRITE(0, BBM_VT_CONTROL, 0x03);
#endif

	BBM_WORD_READ(NULL, BBM_BUF_ENABLE, &mask);
	mask &= 0x100;

	for(i=0;i<subch_cnt;i++)
	{
		switch(serviceType_fci[i])
		{
			case FC8050_DAB:
				mask |= (1<<DAB_SVC_ID);
				BBM_AUDIO_SELECT(0, subch_id[i],DAB_SVC_ID);
#ifdef STREAM_TS_UPLOAD
				fc8050_demux_select_channel(subch_id[i], DAB_SVC_ID);
#else
				g_chinfo[subch_id[i]]=DAB_SVC_ID;
#endif
				break;
			case FC8050_DMB:
			case FC8050_VISUAL:
			case FC8050_BLT_TEST:
				mask |= (1 << (DMB_SVC_ID+dmb_cnt));
				if(dmb_cnt<2)
				{
					BBM_VIDEO_SELECT(0, subch_id[i], DMB_SVC_ID+dmb_cnt, dmb_cnt);
#ifdef STREAM_TS_UPLOAD
					fc8050_demux_select_video(subch_id[i], DMB_SVC_ID+dmb_cnt);
#else
					g_chinfo[subch_id[i]]=dmb_cnt;
#endif
					dmb_cnt++;
				}
				else
					res=BBM_NOK;
				break;
			case FC8050_DATA:
				mask |= (1<<DAT_SVC_ID);
				BBM_DATA_SELECT(0, subch_id[i], DAT_SVC_ID);
#ifdef STREAM_TS_UPLOAD
				fc8050_demux_select_channel(subch_id[i], DAT_SVC_ID);
#else
				g_chinfo[subch_id[i]]=DAT_SVC_ID;
#endif
				break;
			case FC8050_ENSQUERY:
				tunerbb_drv_fc8050_control_fic(1);
				res = BBM_OK;
				break;
			default:
				res = BBM_NOK;
				break;
		}
	}
	BBM_WORD_WRITE(NULL, BBM_BUF_ENABLE, mask);
	tot_subch_cnt = subch_cnt;

	// Added by somesoo 20100730 for removing green block effect
	if(svcType != FC8050_ENSQUERY)
		fc8050_isr_control(1);

	if(res)
		return FC8050_RESULT_ERROR;
	else
		return FC8050_RESULT_SUCCESS;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_get_fic(uint8* buffer, uint32* buffer_size)
    (1)   Getting the FIC data after calling tunerbb_drv_fc8050_multi_set_channel(freq, 1, ignore, ENSQUERY)
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
int8	tunerbb_drv_fc8050_get_fic(uint8* buffer, uint32* buffer_size  /*, uint8 crc_on_off */)
{
	HANDLE hDevice = NULL;
	fci_u16      mfIntStatus = 0;
	fci_u16      size;

	if(buffer==NULL)
		return FC8050_RESULT_ERROR;

	//BBM_WRITE(hDevice, BBM_COM_INT_ENABLE, 0x00);
	//BBM_WRITE(hDevice, BBM_COM_STATUS_ENABLE, 0x00);
	//tunerbb_drv_fc8050_control_fic(1);

	BBM_WORD_READ(hDevice, BBM_BUF_STATUS, &mfIntStatus);

	//printk("tunerbb_drv_fc8050_get_fic (0x%x)\n", mfIntStatus);
	if(mfIntStatus == 0)
	{
		//tunerbb_drv_fc8050_control_fic(0);
		//BBM_WRITE(hDevice, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
		//BBM_WRITE(hDevice, BBM_COM_STATUS_ENABLE, ENABLE_INT_MASK);
		return FC8050_RESULT_ERROR;
	}

	BBM_WORD_WRITE(hDevice, BBM_BUF_STATUS, mfIntStatus);
	BBM_WORD_WRITE(hDevice, BBM_BUF_STATUS, 0x0000);

	if(mfIntStatus & 0x0100)
	{
		BBM_WORD_READ(hDevice, BBM_BUF_FIC_THR, &size);
		size += 1;
		if(size-1)
		{
			BBM_DATA(hDevice, BBM_COM_FIC_DATA, buffer, size);
			*buffer_size=size;
		}
	}

	//tunerbb_drv_fc8050_control_fic(0);
	//BBM_WRITE(hDevice, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
	//BBM_WRITE(hDevice, BBM_COM_STATUS_ENABLE, ENABLE_INT_MASK);

	if(mfIntStatus & 0x0100)
		return FC8050_RESULT_SUCCESS;
	else
		return FC8050_RESULT_ERROR;

}
#else
int8	tunerbb_drv_fc8050_get_fic(uint8* buffer, uint32* buffer_size  /*, uint8 crc_on_off */)
{
	if(buffer==NULL)
		return FC8050_RESULT_ERROR;

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

	return FC8050_RESULT_SUCCESS;
}
#endif
/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_read_data(uint8* buffer, uint32* buffer_size)
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
int8	tunerbb_drv_fc8050_read_data(uint8* buffer, uint32* buffer_size)
{
	int8 retval = FC8050_RESULT_ERROR;

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

	fc8050_isr(NULL);

	if(msc_buffer.valid && msc_buffer.length)
	{
		*buffer_size = msc_buffer.length;
#ifndef NOT_MSCDATA_MULTIPLE_MEMCPY
		memcpy(buffer, &msc_data[0], msc_buffer.length);
#endif
		retval = FC8050_RESULT_SUCCESS;
	}

	return retval;
}

/*                                                                                     
                                                                                                                   
                                                                                                                           
                                                                                        
                      
                     
                                                                 
                  
                               
                                           
                                
                                                                                                                             
                                 
                                             
                                      
                                                                     

                 
                                                                     
                                          
                            
                                                                                            
                                                   
                                                                                                                         
                                                                                        */
#ifdef STREAM_TS_UPLOAD
int8	tunerbb_drv_fc8050_process_multi_data(uint8 subch_cnt, uint8* input_buf, uint32 input_size, uint32* read_size)
{
	uint32 fic_len, nvideo_len, video_len, data_len;
	uint8 i, header_cnt=0;

	if(input_buf == NULL || read_size == NULL)
	{
		return FC8050_RESULT_ERROR;
	}

	fc8050_demux(input_buf, input_size);

	if(!fc8050_get_ts_datalen(TS_DAT_FIC, 0, &fic_len))
	{
		data_len=fic_len;
		header_cnt++;
	}

	for(i=0;i<2;i++)
	{
		if(!fc8050_get_ts_datalen(TS_DAT_VIDEO_I, i, &nvideo_len))
		{
			data_len+=nvideo_len;
			header_cnt++;
		}
	}

	for(i=0;i<8;i++)
	{
		if(!fc8050_get_ts_datalen(TS_DAT_NVIDEO, i, &video_len))
		{
			data_len+=video_len;
			header_cnt++;
		}
	}

	data_len+=sizeof(TDMB_BB_HEADER_TYPE)*header_cnt;
	*read_size=data_len;

	return FC8050_RESULT_SUCCESS;
}
#else
int8	tunerbb_drv_fc8050_process_multi_data(uint8 subch_cnt, uint8* input_buf, uint32 input_size, uint32* read_size)
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

	return FC8050_RESULT_SUCCESS;
}
#endif

#ifdef STREAM_TS_UPLOAD
/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_get_multi_data(uint8 subch_cnt, uint8* buf_ptr, uint32 buf_size)
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
int8	tunerbb_drv_fc8050_get_multi_data(uint8 subch_cnt, uint8* buf_ptr, uint32 buf_size)
{
	uint32 nDataSize;
	int i;
	TDMB_BB_HEADER_TYPE dmb_header;
	uint32 read_size = 0;

	if(buf_ptr == NULL || buf_size == 0)
	{
		return FC8050_RESULT_ERROR;
	}

	if(!fc8050_get_ts_datalen(TS_DAT_FIC, 0, &nDataSize))
	{
		dmb_header.reserved = 0xFC85;
		dmb_header.data_type = TDMB_BB_DATA_FIC;
		dmb_header.size = nDataSize;
		dmb_header.subch_id = 0;
		dmb_header.ack_bit = 0;
		memcpy(buf_ptr, &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
		buf_ptr += sizeof(TDMB_BB_HEADER_TYPE);
		read_size += sizeof(TDMB_BB_HEADER_TYPE);
		fc8050_get_multi_data(TS_DAT_FIC, 0, buf_ptr, &dmb_header.subch_id);
		buf_ptr += nDataSize;
		read_size += nDataSize;
	}			
	
	for(i=0;i<2;i++)
	{
		if(!fc8050_get_ts_datalen(TS_DAT_VIDEO_I, i, &nDataSize))
		{
			dmb_header.reserved = 0xFC85;
			dmb_header.data_type = TDMB_BB_DATA_TS;
			dmb_header.size = nDataSize;
			dmb_header.subch_id = 0;
			dmb_header.ack_bit = 0;
			memcpy(buf_ptr, &dmb_header, sizeof(TDMB_BB_HEADER_TYPE));
			buf_ptr += sizeof(TDMB_BB_HEADER_TYPE);
			read_size += sizeof(TDMB_BB_HEADER_TYPE);
			fc8050_get_multi_data(TS_DAT_VIDEO_I, i, buf_ptr, &dmb_header.subch_id);
			buf_ptr += nDataSize;
			read_size += nDataSize;
		}
	}

	for(i=0;i<8;i++)
	{
		if(!fc8050_get_ts_datalen(TS_DAT_NVIDEO, i, &nDataSize))
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
			fc8050_get_multi_data(TS_DAT_NVIDEO, i, buf_ptr, &dmb_header.subch_id);
			buf_ptr += nDataSize;
			read_size += nDataSize;
		}
	}

	if(read_size != buf_size)
	{
		return FC8050_RESULT_ERROR;
	}
	else
	{
		return FC8050_RESULT_SUCCESS;
	}
}
#else
int8	tunerbb_drv_fc8050_get_multi_data(uint8 subch_cnt, uint8* buf_ptr, uint32 buf_size)
{
	uint32 nDataSize;
	int i=0;
	TDMB_BB_HEADER_TYPE dmb_header;
	uint32 read_size = 0;
	FCI_HEADER_TYPE header;
	uint8 ch_cnt=0;

	if(buf_ptr == NULL || buf_size == 0)
	{
		return FC8050_RESULT_ERROR;
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
		return FC8050_RESULT_ERROR;
	}
	else
	{
		return FC8050_RESULT_SUCCESS;
	}
}
#endif

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_start_tii(void)
    (1)   Starting TII
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           VOID
--------------------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8050_start_tii(void)
{
	return FC8050_RESULT_ERROR;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_stop_tii(void)
    (1)   Stopping TII
    (2)   Return Value
           Sucess : 1
           Fail : 0 or negative interger (If there is error code)
    (3)   Argument
           VOID
--------------------------------------------------------------------------------------- */
int8	tunerbb_drv_fc8050_stop_tii(void)
{
	return FC8050_RESULT_ERROR;
}

/*-------------------------------------------------------------------------------------
int8 tunerbb_drv_fc8050_check_tii(uint8* main_tii_ptr, uint8* sub_tii_ptr)
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
int8	tunerbb_drv_fc8050_check_tii(uint8* main_tii_ptr, uint8* sub_tii_ptr)
{
	if(( NULL == main_tii_ptr) ||( NULL == sub_tii_ptr))
	{
		return FC8050_RESULT_ERROR;
	}

	*main_tii_ptr = 0xFF;
	*sub_tii_ptr = 0xFF;

	return FC8050_RESULT_ERROR;
}

static uint32 tunerbb_drv_fc8050_get_viterbi_ber(void)	//msc_ber
{
	uint8 vt_ctrl;
	uint32 bper, tbe;
	uint32 ber;

#ifdef MON_BURST_MODE
	uint8 burst[12];

#ifdef FEATURE_FIC_BER
	if(serviceType_fci[0]==FC8050_ENSQUERY)
		vt_ctrl = 0x11;
	else
		vt_ctrl = 0x13;
#else
	vt_ctrl = 0x13;
#endif

	BBM_WRITE(0, BBM_VT_CONTROL, vt_ctrl);
	BBM_BULK_READ(0, BBM_VT_RT_BER_PERIOD, &burst[0], 8);
	bper = (uint32)(*(uint32*)burst);
	tbe = (uint32)(*((uint32*)burst+1));

#ifdef FEATURE_FIC_BER
	if(serviceType_fci[0]==FC8050_ENSQUERY)
		vt_ctrl = 0x1;
	else
		vt_ctrl = 0x3;
#else
	vt_ctrl = 0x3;
#endif

	BBM_WRITE(0, BBM_VT_CONTROL, vt_ctrl);
#else
	BBM_READ(0, BBM_VT_CONTROL, &vt_ctrl);
	vt_ctrl |= 0x10;
	BBM_WRITE(0, BBM_VT_CONTROL, vt_ctrl);

	BBM_LONG_READ(0, BBM_VT_RT_BER_PERIOD, &bper);
	BBM_LONG_READ(0, BBM_VT_RT_ERROR_SUM, &tbe);

	vt_ctrl &= ~0x10;
	BBM_WRITE(0, BBM_VT_CONTROL, vt_ctrl);
#endif

	if(bper == 0)
	{
		//                
		ber = MAX_MSC_BER;
	}
	else if(tbe == 0)
	{
		ber = 0;
	}
	else
	{
		//ber = ((tbe / bper) * 100000);
		//ber = (tbe * 100000) / bper;
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

static int8 tunerbb_drv_fc8050_get_sync_status(void)
{
	uint8 sync_status;

	BBM_READ(0, BBM_SYNC_STATUS, &sync_status);

	return sync_status;
}

static uint32 tunerbb_drv_fc8050_get_rs_ber(void)	//va_ber
{
	uint16 nframe, rserror;
	uint32 esum;
	uint32 ber;

#ifdef MON_BURST_MODE
	uint8 burst[12];

	BBM_BULK_READ(0, BBM_RS_BER_PERIOD, &burst[0], 8);
	nframe = (uint16)(*(uint16*)burst);
	rserror = (uint16)(*((uint16*)burst+1));
	esum = (uint32)(*((uint32*)burst+1));
#else
	BBM_WORD_READ(0, BBM_RS_BER_PERIOD, &nframe);
	BBM_LONG_READ(0, BBM_RS_ERR_SUM, &esum);
	BBM_WORD_READ(0, BBM_RS_FAIL_COUNT, &rserror);
#endif

	if(nframe == 0)
	{
		//                
		ber = MAX_VA_BER;
	}
	else if((esum == 0) && (rserror == 0))
	{
		ber = 0;
	}
	else
	{
		//               
		#if (1)	//include corrected bit
		ber = esum;
		#else	//not include
		ber = 0;
		#endif

		ber += rserror * 9;
		ber /= (nframe + 1) * 204 * 8;
		ber = ber * 100000;
	}

	ber = (ber >= MAX_VA_BER) ? MAX_VA_BER : ber;

	return ber;

}

/*
static fci_u8 ficBuffer[1024];
extern int (*pFicCallback)(fci_u32 userdata, fci_u8 *data, int length);
extern fci_u32 gFicUserData;

void tunerbb_drv_fc8050_process_polling_data()
{
	HANDLE hDevice = NULL;
	fci_u16      mfIntStatus = 0;
	fci_u16      size;
	int i;

	BBM_WRITE(hDevice, BBM_COM_INT_ENABLE, 0x00);
	BBM_WRITE(hDevice, BBM_COM_STATUS_ENABLE, 0x00);

	BBM_WORD_READ(hDevice, BBM_BUF_INT, 0x01ff);
	BBM_WORD_READ(hDevice, BBM_BUF_ENABLE, 0x01ff);

	for(i = 0 ; i < 200 ; i++)
	{
		BBM_WORD_READ(hDevice, BBM_BUF_STATUS, &mfIntStatus);

		if(mfIntStatus)
			break;
	}

	if(mfIntStatus == 0)
	{
		BBM_WORD_READ(hDevice, BBM_BUF_INT, 0x00ff);
		BBM_WORD_READ(hDevice, BBM_BUF_ENABLE, 0x00ff);
		BBM_WRITE(hDevice, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
		BBM_WRITE(hDevice, BBM_COM_STATUS_ENABLE, ENABLE_INT_MASK);
		return;
	}

	BBM_WORD_WRITE(hDevice, BBM_BUF_STATUS, mfIntStatus);
	BBM_WORD_WRITE(hDevice, BBM_BUF_STATUS, 0x0000);

	if(mfIntStatus & 0x0100)
	{
		BBM_WORD_READ(hDevice, BBM_BUF_FIC_THR, &size);
		size += 1;
		if(size-1)
		{
			BBM_DATA(hDevice, BBM_COM_FIC_DATA, &ficBuffer[0], size);

			if(pFicCallback)
			      (*pFicCallback)(gFicUserData, &ficBuffer[0], size);
		}
	}
	BBM_WORD_READ(hDevice, BBM_BUF_INT, 0x00ff);
	BBM_WORD_READ(hDevice, BBM_BUF_ENABLE, 0x00ff);

	BBM_WRITE(hDevice, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
	BBM_WRITE(hDevice, BBM_COM_STATUS_ENABLE, ENABLE_INT_MASK);
}
*/

static int8 tunerbb_drv_fc8050_check_overrun(uint8 op_mode)
{
	uint16 mfoverStatus;

	// Patch for BER monitoring 20111115
	//uint16 buf_set=0;

	uint16 veri_val=0;
	uint8 mask;

	if(op_mode == FC8050_DAB)
	{
		mask = 0x08;
	}
	else if(op_mode == FC8050_DMB || op_mode == FC8050_VISUAL)
	{
		mask = 0x01;
	}
	else
	{
		printk("fc8050 invaild op_mode %d\n", op_mode);
		return FC8050_RESULT_ERROR; /* invaild op_mode */
	}

	// Patch for BER monitoring 20111115
	//BBM_WORD_READ(NULL, BBM_BUF_ENABLE, &buf_set);

	// Patch for BER monitoring 20111115
	//if(buf_set & mask)
	{
		BBM_WORD_READ(NULL, BBM_BUF_OVERRUN, &mfoverStatus);

		if(mfoverStatus & mask)
		{
			BBM_WORD_WRITE(NULL, BBM_BUF_OVERRUN, mfoverStatus);
			BBM_WORD_WRITE(NULL, BBM_BUF_OVERRUN, 0x0000);

			BBM_WORD_READ(NULL, BBM_BUF_ENABLE, &veri_val);
			veri_val &= ~mask;
			BBM_WORD_WRITE(NULL, BBM_BUF_ENABLE, veri_val);
			veri_val |= mask;
			BBM_WORD_WRITE(NULL, BBM_BUF_ENABLE, veri_val);

			fc8050_isr_interruptclear();

			printk("======== FC8050  OvernRun and Buffer Reset Done mask (0x%X) over (0x%X) =======\n", mask,mfoverStatus );
		}
	}

	return FC8050_RESULT_SUCCESS;
}
