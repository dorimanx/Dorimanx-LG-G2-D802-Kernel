/*****************************************************************************

	Copyright(c) 2011 LG Electronics Inc. All Rights Reserved

	File name : mbt_ring.h

	Description :

    Hoistory
	----------------------------------------------------------------------
	Feb. 11. 2011:		hyewon.eum		create

*******************************************************************************/ 
#ifndef _MBT_RING_H_
#define _MBT_RING_H_

/*============================================================
**    1.   DEFINITIONS
*============================================================*/
typedef struct _mbt_ring 
{
	char *data;
	int size;
	int pread;
	int pwrite;
	unsigned int mutex;
}MBT_RING;

/*======================================================= 
    Function 		: mbt_ring_init
    Description		: Ring Buffer Initialize
    Parameter		: rbuf - structure, data - ring buffer, len - ring buffer size 
    Return Value	: void

	when            model      		who        edit history
  -------------------------------------------------------
	Mar.16.2009  QDMB 2.0.0    inb612   created
======================================================== */
static void mbt_ring_init(MBT_RING *rbuf, void *data, int len)
{		
	rbuf->pread=rbuf->pwrite=0;
	rbuf->data=data;
	rbuf->size=len;
}

/*======================================================= 
	 Function		 : mbt_ring_empty
	 Description	 : Ring Buffer empty
	 Parameter	 : rbuf - structure
	 Return Value	 : -1 : error, 0 : no empty, 1: empty
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
static int mbt_ring_empty(MBT_RING *rbuf)
{	
	return (rbuf->pread==rbuf->pwrite);
}

/*======================================================= 
	 Function		 : mbt_ring_free
	 Description	 : Ring Buffer free size getting
	 Parameter	 : rbuf - structure
	 Return Value	 : -1 : error, free size 
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
static int mbt_ring_free(MBT_RING *rbuf)
{
	int free;
	
	free = rbuf->pread - rbuf->pwrite;
	if (free <= 0)
	{
		free += rbuf->size;
	}

	return free-1;
}

/*======================================================= 
	 Function		 : mbt_ring_avail
	 Description	 : Ring Buffer available size getting
	 Parameter	 : rbuf - structure
	 Return Value	 : -1 : error, available size
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
static int mbt_ring_avail(MBT_RING *rbuf)
{
	int avail;

	avail = rbuf->pwrite - rbuf->pread;
	if (avail < 0)
	{
		avail += rbuf->size;
	}

	return avail;
}

/*======================================================= 
	 Function		 : mbt_ring_flush
	 Description	 : Ring Buffer flush
	 Parameter	 : rbuf - structure
	 Return Value	 : void
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
static void mbt_ring_flush(MBT_RING *rbuf)
{
	rbuf->pread = rbuf->pwrite;
}

/*======================================================= 
	 Function		 : mbt_ring_read
	 Description	 : Ring Buffer data read
	 Parameter	 : rbuf - structure, buf - data buffer to read, len - length
	 Return Value	 : -1 : error, read size
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
static int mbt_ring_read(MBT_RING *rbuf, char *buf, int len)
{
	int todo = len;
	int split;
	
	split = (rbuf->pread + len > rbuf->size) ? rbuf->size - rbuf->pread : 0;

	if (split > 0) 
	{
		memcpy(buf,rbuf->data+rbuf->pread,split);

		buf += split;
		todo -= split;
		rbuf->pread = 0;

	}

	memcpy(buf, rbuf->data+rbuf->pread, todo);

	rbuf->pread = (rbuf->pread + todo) % rbuf->size;

	return len;

}

/*======================================================= 
	 Function		 : mbt_ring_write
	 Description	 : Ring Buffer data write
	 Parameter	 : rbuf - structure, buf - data buffer to read, len - length
	 Return Value	 : -1 : error, write size
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
static int mbt_ring_write(MBT_RING *rbuf, const char *buf,int len)
{
	int todo = len;
	int split;

	split = (rbuf->pwrite + len > rbuf->size) ? rbuf->size - rbuf->pwrite : 0;

	if (split > 0) 
	{
		memcpy(rbuf->data+rbuf->pwrite, (void*)buf, split);

		buf += split;
		todo -= split;
		rbuf->pwrite = 0;
	}

	memcpy(rbuf->data+rbuf->pwrite, (void*)buf, todo);	

	rbuf->pwrite = (rbuf->pwrite + todo) % rbuf->size;

	return len;
}

#endif /* _TDMB_RING_H_ */
