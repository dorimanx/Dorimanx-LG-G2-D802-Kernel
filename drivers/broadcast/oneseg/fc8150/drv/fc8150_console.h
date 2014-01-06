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

#define MAX_OPEN_NUM 		8

#define IOCTL_MAGIC	't'

typedef struct {
	unsigned long size;
	unsigned long buff[128];
} ioctl_info;

#define IOCTL_MAXNR			24

#define IOCTL_ISDBT_RESET		_IO( IOCTL_MAGIC, 0 )
#define IOCTL_ISDBT_PROBE		_IO( IOCTL_MAGIC, 1 )
#define IOCTL_ISDBT_INIT	 	_IO( IOCTL_MAGIC, 2 )
#define IOCTL_ISDBT_DEINIT	 	_IO( IOCTL_MAGIC, 3 )

#define IOCTL_ISDBT_BYTE_READ 		_IOWR( IOCTL_MAGIC, 4, ioctl_info )
#define IOCTL_ISDBT_WORD_READ 		_IOWR( IOCTL_MAGIC, 5, ioctl_info )
#define IOCTL_ISDBT_LONG_READ 		_IOWR( IOCTL_MAGIC, 6, ioctl_info )
#define IOCTL_ISDBT_BULK_READ 		_IOWR( IOCTL_MAGIC, 7, ioctl_info )

#define IOCTL_ISDBT_BYTE_WRITE 		_IOW( IOCTL_MAGIC, 8, ioctl_info )
#define IOCTL_ISDBT_WORD_WRITE 		_IOW( IOCTL_MAGIC, 9, ioctl_info )
#define IOCTL_ISDBT_LONG_WRITE 		_IOW( IOCTL_MAGIC, 10, ioctl_info )
#define IOCTL_ISDBT_BULK_WRITE 		_IOW( IOCTL_MAGIC, 11, ioctl_info )

#define IOCTL_ISDBT_TUNER_READ	 	_IOWR( IOCTL_MAGIC, 12, ioctl_info )
#define IOCTL_ISDBT_TUNER_WRITE	 	_IOW( IOCTL_MAGIC, 13, ioctl_info )

#define IOCTL_ISDBT_TUNER_SET_FREQ 	_IOW( IOCTL_MAGIC, 14, ioctl_info )
#define IOCTL_ISDBT_TUNER_SELECT 	_IOW( IOCTL_MAGIC, 15, ioctl_info )
#define IOCTL_ISDBT_TUNER_DESELECT 	_IO( IOCTL_MAGIC, 16 )

#define IOCTL_ISDBT_TS_START	 	_IO( IOCTL_MAGIC, 17 )
#define IOCTL_ISDBT_TS_STOP	 	_IO( IOCTL_MAGIC, 18 )

#define IOCTL_ISDBT_TUNER_GET_RSSI 	_IOWR( IOCTL_MAGIC, 19, ioctl_info )

#define IOCTL_ISDBT_HOSTIF_SELECT 	_IOW( IOCTL_MAGIC, 20, ioctl_info )
#define IOCTL_ISDBT_HOSTIF_DESELECT 	_IO( IOCTL_MAGIC, 21 )

#define IOCTL_ISDBT_POWER_ON		_IO( IOCTL_MAGIC, 22 )
#define IOCTL_ISDBT_POWER_OFF		_IO( IOCTL_MAGIC, 23 )

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

