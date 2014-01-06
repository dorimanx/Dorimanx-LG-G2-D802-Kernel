/*****************************************************************************

	Copyright(c) 2008 LG Electronics Inc. All Rights Reserved

	File name : mbd_dataring.h

	Description :

    Hoistory
	----------------------------------------------------------------------
	Mar. 16, 2009:		inb612		create

*******************************************************************************/ 
#ifndef _MBT_DATARING_H_
#define _MBT_DATARING_H_

void mbt_dataring_create(unsigned int* buffer_id, int len);
void mbt_dataring_destroy(unsigned int* buffer_id);
int mbt_dataring_empty(unsigned int buffer_id);
int mbt_dataring_free(unsigned int buffer_id);
int mbt_dataring_avail(unsigned int buffer_id);
void mbt_dataring_flush(unsigned int buffer_id);
int mbt_dataring_read(unsigned int buffer_id, char * buf, int len);
int mbt_dataring_write(unsigned int buffer_id, const char * buf, int len);
#endif /*_MBT_DATARING_H_*/
