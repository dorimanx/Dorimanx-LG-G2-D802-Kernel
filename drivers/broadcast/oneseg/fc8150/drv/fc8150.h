/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : bbm.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
*******************************************************************************/

#ifndef __ISDBT_H__
#define __ISDBT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/list.h>

#include "fci_types.h"
#include "fci_ringbuffer.h"

#define CTL_TYPE 0
#define TS_TYPE 1

#define ISDBT_IOC_MAGIC	'I'
//#define IOCTL_MAXNR		18
#define IOCTL_MAXNR		44

typedef struct
{
	unsigned short 	lock;	/*baseband lock state 					(1: Lock, 0:Unlock)*/
	unsigned short	cn;		/*Signal Level (C/N) 					(0 ~ 28)*/
	unsigned int		ber; 	/*Bit Error rate 						(0 ~ 100000)*/
	unsigned int		per;  	/*Packet Error rate  					(0 ~ 100000)*/
	unsigned short	agc;  	/*Auto Gain control  					(0 ~ 255)*/
	int 				rssi;  	/*Received Signal Strength Indication  	(0 ~ -99)*/
	unsigned short	ErrTSP;
	unsigned short	TotalTSP;
	unsigned int		Num;
	unsigned int		Exp;
	unsigned int		mode;
} IOCTL_ISDBT_SIGNAL_INFO;

#define IOCTL_ISDBT_POWER_ON			_IO(ISDBT_IOC_MAGIC,10)
#define IOCTL_ISDBT_POWER_OFF			_IO(ISDBT_IOC_MAGIC, 11)
#define IOCTL_ISDBT_SCAN_FREQ			_IOW(ISDBT_IOC_MAGIC,12, unsigned int)
#define IOCTL_ISDBT_SET_FREQ			_IOW(ISDBT_IOC_MAGIC,13, unsigned int)
#define IOCTL_ISDBT_GET_LOCK_STATUS    	_IOR(ISDBT_IOC_MAGIC,14, unsigned int)
#define IOCTL_ISDBT_GET_SIGNAL_INFO		_IOR(ISDBT_IOC_MAGIC,15, IOCTL_ISDBT_SIGNAL_INFO)
#define IOCTL_ISDBT_START_TS			_IO(ISDBT_IOC_MAGIC,16)
#define IOCTL_ISDBT_STOP_TS				_IO(ISDBT_IOC_MAGIC,17)

#define LGE_BROADCAST_DMB_IOCTL_MAGIC 'I'

#define LGE_BROADCAST_DMB_IOCTL_ON \
	_IO(LGE_BROADCAST_DMB_IOCTL_MAGIC, 30)

#define LGE_BROADCAST_DMB_IOCTL_OFF \
	_IO(LGE_BROADCAST_DMB_IOCTL_MAGIC, 31)

#define LGE_BROADCAST_DMB_IOCTL_OPEN \
	_IO(LGE_BROADCAST_DMB_IOCTL_MAGIC, 32)

#define LGE_BROADCAST_DMB_IOCTL_CLOSE \
	_IO(LGE_BROADCAST_DMB_IOCTL_MAGIC, 33)
	
#define LGE_BROADCAST_DMB_IOCTL_SET_CH \
	_IOW(LGE_BROADCAST_DMB_IOCTL_MAGIC, 35, struct broadcast_dmb_set_ch_info*)

#define LGE_BROADCAST_DMB_IOCTL_RESYNC \
	_IOW(LGE_BROADCAST_DMB_IOCTL_MAGIC, 36, int)

#define LGE_BROADCAST_DMB_IOCTL_DETECT_SYNC \
	_IOR(LGE_BROADCAST_DMB_IOCTL_MAGIC, 37, int*)

#define LGE_BROADCAST_DMB_IOCTL_GET_SIG_INFO \
	_IOR(LGE_BROADCAST_DMB_IOCTL_MAGIC, 38, struct broadcast_dmb_sig_info*)

#define LGE_BROADCAST_DMB_IOCTL_GET_CH_INFO \
	_IOR(LGE_BROADCAST_DMB_IOCTL_MAGIC, 39, struct broadcast_dmb_ch_info*)

#define LGE_BROADCAST_DMB_IOCTL_RESET_CH \
	_IO(LGE_BROADCAST_DMB_IOCTL_MAGIC, 40)

#define LGE_BROADCAST_DMB_IOCTL_USER_STOP \
	_IOW(LGE_BROADCAST_DMB_IOCTL_MAGIC, 41, int*)

#define LGE_BROADCAST_DMB_IOCTL_GET_DMB_DATA \
	_IOW(LGE_BROADCAST_DMB_IOCTL_MAGIC, 42, struct broadcast_dmb_data_info*)

#define LGE_BROADCAST_DMB_IOCTL_SELECT_ANTENNA \
	_IOW(LGE_BROADCAST_DMB_IOCTL_MAGIC, 43, int*)


struct broadcast_dmb_set_ch_info
{
	unsigned int	mode;
	unsigned int	ch_num;
	unsigned int	sub_ch_id;
};

typedef struct
{
	unsigned int dab_ok;
	unsigned int msc_ber;
	unsigned int sync_lock;
	unsigned int afc_ok;
	unsigned int cir;
	unsigned int fic_ber;
	unsigned int tp_lock;
	unsigned int sch_ber;
	unsigned int tp_err_cnt;
	unsigned int va_ber;
	unsigned int srv_state_flag;
	unsigned int agc;
	unsigned int cn;
	unsigned int antenna_level;
}tdmb_sig_info;

typedef struct
{
	int lock;	/*baseband lock state 					(1: Lock, 0:Unlock)*/
	int cn;		/*Signal Level (C/N) 					(0 ~ 28)*/
	int ber; 	/*Bit Error rate 						(0 ~ 100000)*/
	int per;  	/*Packet Error rate  					(0 ~ 100000)*/
	int agc;  	/*Auto Gain control  					(0 ~ 255)*/
	int rssi;  	/*Received Signal Strength Indication  	(0 ~ -99)*/
	int ErrTSP;
	int TotalTSP;
	int antenna_level;
	int Num;
	int Exp;
	int mode;
}oneseg_sig_info;

struct broadcast_dmb_sig_info
{
	union 
	{
		tdmb_sig_info tdmb_info;
		oneseg_sig_info oneseg_info;
	}info;
};

struct broadcast_dmb_ch_info
{
	unsigned char*	ch_buf;
	unsigned int	buf_len;
};

struct broadcast_dmb_data_info
{
	unsigned char*	data_buf;
	unsigned int	data_buf_size;
	unsigned int	copied_size;
	unsigned int	packet_cnt;
};

typedef struct
{
	unsigned short	reserved;
	unsigned char	subch_id;
	unsigned short	size;
	unsigned char	data_type:7;
	unsigned char	ack_bit:1;
} DMB_BB_HEADER_TYPE;

typedef enum
{
	DMB_BB_DATA_TS,
	DMB_BB_DATA_DAB,
	DMB_BB_DATA_PACK,
	DMB_BB_DATA_FIC,
	DMB_BB_DATA_FIDC
} DMB_BB_DATA_TYPE;

enum
{
	LGE_BROADCAST_OPMODE_DAB = 1,
	LGE_BROADCAST_OPMODE_DMB = 2,
	LGE_BROADCAST_OPMODE_VISUAL =3,
	LGE_BROADCAST_OPMODE_DATA,
	LGE_BROADCAST_OPMODE_ENSQUERY = 6,	/*           */
	LGE_BROADCAST_OPMODE_SERVICE_MAX
};

typedef enum {
	ISDBT_POWERON       = 0,
	ISDBT_POWEROFF	    = 1,
	ISDBT_DATAREAD		= 2
} ISDBT_MODE;
			
typedef struct {
	HANDLE				*hInit;
	struct list_head		hList;
	struct fci_ringbuffer		RingBuffer;
       	u8				*buf;
	u8				isdbttype;
} ISDBT_OPEN_INFO_T;

typedef struct {
	struct list_head		hHead;		
} ISDBT_INIT_INFO_T;

#ifdef __cplusplus
}
#endif

#endif // __ISDBT_H__

