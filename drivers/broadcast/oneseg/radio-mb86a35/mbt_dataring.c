/*****************************************************************************

	Copyright(c) 2008 LG Electronics Inc. All Rights Reserved

	File name : mbt_ringbuffer.c

	Description :

    Hoistory
	----------------------------------------------------------------------
	Mar. 16, 2009:		inb612		create

*******************************************************************************/
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

//#include <mbt_osp.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
//#include <linux/sched.h>
#include <linux/string.h>
//#include <asm/uaccess.h>

#include <mbt_ring.h>
//#include <mbt_dbg.h>

static DEFINE_MUTEX(ringbuffer_lock);

/*============================================================
**    1.   DEFINITIONS
*============================================================*/
#define USE_STATIC_DATA_BUFFER	//static buffer for data ring, because of failure of malloc
#define RING_DATA_BUFFER_SIZE 188*128*30

/*============================================================
**    2.   External Variables
*============================================================*/

/*============================================================
**    3.   External Functions
*============================================================*/

/*============================================================
**    4.   Local constant variables
*============================================================*/

/*============================================================
**    5.   Local Typedef
*============================================================*/

/*============================================================
**    6.   Global Variables
*============================================================*/

/*============================================================
**    7.   Static Variables
*============================================================*/ 
#ifdef USE_STATIC_DATA_BUFFER
static char data_buf[RING_DATA_BUFFER_SIZE] = {0,}; 
#endif

/*============================================================
**    8.   Local Function Prototype
*============================================================*/


/*======================================================= 
    Function 		: mbt_dataring_create
    Description		: Ring Buffer Initialize
    Parameter		: rbuf - structure, len - ring buffer size 
    Return Value	: void

	when            model      		who        edit history
  -------------------------------------------------------
	Mar.16.2009  QDMB 2.0.0    inb612   created
======================================================== */
void mbt_dataring_create(unsigned int *buffer_id, int len)
{
	void *data = NULL;
	MBT_RING *rbuf = NULL;
	//uint32 mutex = 0;

	if((buffer_id == NULL) || (len <= 0))
	{
		return;
	}
	
	//mbd_osp_create_critical_section(&mutex);

	//if(mutex)
	{
		rbuf = (MBT_RING*)kmalloc(sizeof(MBT_RING), GFP_KERNEL);
		if(rbuf == NULL)
		{
			//mbd_osp_leave_critical_section(mutex);
			//mbd_osp_delete_critical_section(mutex);
			printk("mbt_dataring_create fail no memory !!! \n");
			return;
		}

		memset(rbuf, 0x00, sizeof(MBT_RING));
		//rbuf->mutex = &ringbuffer_lock;
		#ifdef USE_STATIC_DATA_BUFFER
		data = &data_buf[0];
		#else
		data = kmalloc(len, GFP_KERNEL);
		#endif
		if(data)
		{
			#ifdef USE_STATIC_DATA_BUFFER
			memset(data, 0x00, RING_DATA_BUFFER_SIZE);
			#else
			memset(data, 0x00, len);
			#endif
		}
		#ifdef USE_STATIC_DATA_BUFFER
		mbt_ring_init(rbuf, data, RING_DATA_BUFFER_SIZE);
		#else
		mbt_ring_init(rbuf, data, len);
		#endif
		*buffer_id = (unsigned int)rbuf;
		//mbd_osp_leave_critical_section(mutex);	
		//mutex_unlock(&ringbuffer_lock);
	}
}
EXPORT_SYMBOL(mbt_dataring_create);


/*======================================================= 
    Function 		: mbt_dataring_destroy
    Description		: Ring Buffer De-initialize
    Parameter		: rbuf - structure 
    Return Value	: void

	when            model      		who        edit history
  -------------------------------------------------------
	Mar.16.2009  QDMB 2.0.0    inb612   created
======================================================== */
void mbt_dataring_destroy(unsigned int * buffer_id)
{
	//void* data = NULL;
	MBT_RING *rbuf = NULL;
	//uint32 mutex = 0;
	
	if(buffer_id == NULL)
	{
		return;
	}

	if(*buffer_id == 0)
	{
		return;
	}

	rbuf = (MBT_RING*)(*buffer_id);	

	//if(rbuf->mutex == 0)
	//{
	//	return;
	//}

	//mutex = rbuf->mutex;
	
	//mbd_osp_enter_critical_section(mutex);
	mutex_lock(&ringbuffer_lock);
	#ifndef USE_STATIC_DATA_BUFFER
	if(rbuf->data != NULL)
	{
		kfree(rbuf->data);
	}
	#endif
	
	kfree((void*)rbuf);
	*buffer_id = 0;
	//mbd_osp_leave_critical_section(mutex);
	//mbd_osp_delete_critical_section(mutex);

	mutex_unlock(&ringbuffer_lock);
}
EXPORT_SYMBOL(mbt_dataring_destroy);

/*======================================================= 
	 Function		 : mbt_dataring_empty
	 Description	 : Ring Buffer empty
	 Parameter	 : rbuf - structure
	 Return Value	 : -1 : error, 0 : no empty, 1: empty
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
int mbt_dataring_empty(unsigned int buffer_id)
{
	if(buffer_id == 0)
	{
		return -1;
	}
	
	return mbt_ring_empty((MBT_RING*)buffer_id);
}
EXPORT_SYMBOL(mbt_dataring_empty);

/*======================================================= 
	 Function		 : mbt_dataring_free
	 Description	 : Ring Buffer free size getting
	 Parameter	 : rbuf - structure
	 Return Value	 : -1 : error, free size 
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
int mbt_dataring_free(unsigned int buffer_id)
{
	if(buffer_id == 0)
	{
		return -1;
	}

	return mbt_ring_free((MBT_RING*)buffer_id);
}
EXPORT_SYMBOL(mbt_dataring_free);

/*======================================================= 
	 Function		 : mbt_dataring_avail
	 Description	 : Ring Buffer available size getting
	 Parameter	 : rbuf - structure
	 Return Value	 : -1 : error, available size
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
int mbt_dataring_avail(unsigned int buffer_id)
{	
	if(buffer_id == 0)
	{
		return -1;
	}

	return mbt_ring_avail((MBT_RING*)buffer_id);
}
EXPORT_SYMBOL(mbt_dataring_avail);

/*======================================================= 
	 Function		 : mbt_dataring_flush
	 Description	 : Ring Buffer flush
	 Parameter	 : rbuf - structure
	 Return Value	 : void
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
void mbt_dataring_flush(unsigned int buffer_id)
{
	MBT_RING *rbuf;
	
	if(buffer_id == 0)
	{
		return;
	}

	rbuf = (MBT_RING*)buffer_id;

	//if(rbuf->mutex == 0)
	//{
	//	return;
	//}

	//mbd_osp_enter_critical_section(rbuf->mutex);
	mutex_lock(&ringbuffer_lock);
	mbt_ring_flush(rbuf);
	//mbd_osp_leave_critical_section(rbuf->mutex);
	mutex_unlock(&ringbuffer_lock);
}
EXPORT_SYMBOL(mbt_dataring_flush);

/*======================================================= 
	 Function		 : mbt_dataring_read
	 Description	 : Ring Buffer data read
	 Parameter	 : rbuf - structure, buf - data buffer to read, len - length
	 Return Value	 : -1 : error, read size
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
int mbt_dataring_read(unsigned int buffer_id, char* buf, int len)
{
	MBT_RING *rbuf;
	
	if((buffer_id == 0)||(buf == NULL)||(len<=0))
	{
		return -1;
	}

	rbuf = (MBT_RING*)buffer_id;

	if(/*(rbuf->mutex == 0) ||*/ (rbuf->data == NULL))
	{
		return -1;
	}

	//mbd_osp_enter_critical_section(rbuf->mutex);
	mutex_lock(&ringbuffer_lock);
	mbt_ring_read(rbuf, (char*)buf, (int)len);
	mutex_unlock(&ringbuffer_lock);
	//mbd_osp_leave_critical_section(rbuf->mutex);
	return len;
}
EXPORT_SYMBOL(mbt_dataring_read);

/*======================================================= 
	 Function		 : mbt_dataring_write
	 Description	 : Ring Buffer data write
	 Parameter	 : rbuf - structure, buf - data buffer to read, len - length
	 Return Value	 : -1 : error, write size
 
	 when			 model			 who		edit history
   -------------------------------------------------------
	 Mar.16.2009  QDMB 2.0.0	inb612	 created
======================================================== */
int mbt_dataring_write(unsigned int buffer_id, const char * buf, int len)
{
	MBT_RING *rbuf;
	
	if((buffer_id == 0)||(buf == NULL) ||(len <= 0))
	{
		return -1;
	}

	rbuf = (MBT_RING*)buffer_id;

	if(/*(rbuf->mutex == 0) ||*/ (rbuf->data == NULL))
	{
		return -1;
	}

	//mbd_osp_enter_critical_section(rbuf->mutex);
	mutex_lock(&ringbuffer_lock);
	mbt_ring_write(rbuf, (char*)buf, (int)len);
	mutex_unlock(&ringbuffer_lock);
	//mbd_osp_leave_critical_section(rbuf->mutex);
	return len;
}
EXPORT_SYMBOL(mbt_dataring_write);

