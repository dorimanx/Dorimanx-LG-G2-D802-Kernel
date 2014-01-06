#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>        /* copy_to_user */

#include "tdmb_tunerbbdrv_lg2102def.h"

#define TDMB_MPI_BUF_SIZE 			(188*16*2 + 8)//interrupt size + sizeof(TDMB_BB_HEADER_TYPE)
#define TDMB_MPI_BUF_CHUNK_NUM  	10

static uint8*	gpMPI_Buffer = NULL;
static uint8	gBBBuffer_ridx = 0;
static uint8	gBBBuffer_widx = 0;
static uint32	tdmb_real_read_size[TDMB_MPI_BUF_CHUNK_NUM];

int broadcast_drv_if_power_on(void)
{
	int8 res = ERROR;
	boolean retval = FALSE;

	if(gpMPI_Buffer == NULL)
	{
		gpMPI_Buffer = kmalloc(TDMB_MPI_BUF_SIZE*TDMB_MPI_BUF_CHUNK_NUM, GFP_KERNEL);
	}
	
	retval = tunerbb_drv_lg2102_power_on();

	if(retval == TRUE)
	{
		res = OK;
	}
	tunerbb_drv_lg2102_set_userstop();
	
	return res;
}

int broadcast_drv_if_power_off(void)
{
	int8 res = ERROR;
	boolean retval = FALSE;
	
	retval = tunerbb_drv_lg2102_power_off();

	if(retval == TRUE)
	{
		res = OK;
	}
	tunerbb_drv_lg2102_set_userstop();

	return res;
}

int broadcast_drv_if_open(void) 
{
	int8 res = ERROR;
	boolean retval = FALSE;
	
	retval = tunerbb_drv_lg2102_init();

	if(retval == TRUE)
	{
		res = OK;
	}

	return res;
}

int broadcast_drv_if_close(void)
{
	int8 res = ERROR;
	boolean retval = FALSE;

	retval = tunerbb_drv_lg2102_stop();
	
	if(retval == TRUE)
	{
		res = OK;
	}

	return res;
}

int broadcast_drv_if_set_channel(unsigned int freq_num, unsigned int subch_id, unsigned int op_mode)
{	
	int8 rc = ERROR;
	boolean retval = FALSE;

	retval = tunerbb_drv_lg2102_set_channel(freq_num, subch_id, op_mode);
	if(retval == TRUE)
	{
		gBBBuffer_ridx = gBBBuffer_widx = 0;
		rc = OK;
	}

	return rc;
}

int broadcast_drv_if_resync(void)
{
	return 0;
}

int broadcast_drv_if_detect_sync(int op_mode)
{
	int8 rc = ERROR;
	boolean retval = FALSE;

	retval = tunerbb_drv_lg2102_re_syncdetector(op_mode);

	if(retval == TRUE)
	{
		rc = OK;
	}
	return rc;
}

int broadcast_drv_if_get_sig_info(struct broadcast_tdmb_sig_info *dmb_bb_info)
{
	int rc = ERROR;
	boolean retval = FALSE;
	
	retval = tunerbb_drv_lg2102_get_ber(dmb_bb_info);
	
	if(retval == TRUE)
	{
		rc = OK;
	}		
	return rc;
}

int broadcast_drv_if_get_ch_info(char* buffer, unsigned int* buffer_size)
{
	int rc = ERROR;
	boolean retval = FALSE;

	if(buffer == NULL || buffer_size == NULL)
	{
		printk("broadcast_tdmb_get_ch_info argument error\n");
		return rc;
	}

	retval = tunerbb_drv_lg2102_get_fic(buffer, buffer_size, TRUE);
	
	if(retval == TRUE)
	{
		rc = OK;
	}	
	
	return rc;
}

int broadcast_drv_if_get_dmb_data(char** buffer_ptr, unsigned int* buffer_size, unsigned int user_buffer_size)
{
	if(gpMPI_Buffer == NULL)
	{
		printk("gpMPI_FIFO_Buffer == NULL\n");
		return ERROR;
	}

	if(buffer_ptr == NULL || buffer_size == NULL)
	{
		printk(" input arg is null\n");
		return ERROR;
	}

	if(gBBBuffer_ridx == gBBBuffer_widx)
	{
		//printk("broadcast_tdmb_get_dmb_data, data is not ready\n");
		return ERROR;
	}

	if(user_buffer_size < tdmb_real_read_size[gBBBuffer_ridx])
	{
		printk("user buffer is not enough %d", user_buffer_size);
		return ERROR;
	}

	*buffer_ptr	= gpMPI_Buffer + gBBBuffer_ridx * TDMB_MPI_BUF_SIZE;
	*buffer_size = tdmb_real_read_size[gBBBuffer_ridx];

	//printk("broadcast_tdmb_get_dmb_data, read_size %d, total ridx %d, widx %d\n", *buffer_size, gBBBuffer_ridx, gBBBuffer_widx);

	gBBBuffer_ridx = ((gBBBuffer_ridx + 1) % TDMB_MPI_BUF_CHUNK_NUM);

	return OK;
}

int broadcast_drv_if_reset_ch(void)
{
	int8 res = ERROR;
	boolean retval = FALSE;
	
	retval = tunerbb_drv_lg2102_reset_ch();

	if(retval == TRUE)
	{
		res = OK;
	}

	return res;
}

int broadcast_drv_if_user_stop(void)
{
	tunerbb_drv_lg2102_set_userstop( );
	return OK;	
}

int broadcast_drv_if_select_antenna(unsigned int sel)
{
	//tunerbb_drv_lg2102_select_antenna(sel);
	return OK;
}

int broadcast_drv_if_isr(void)
{
	uint8* 	read_buffer_ptr 	= NULL;
	uint32 	read_buffer_size 	= 0;

	if(gpMPI_Buffer== NULL)
	{
		printk("gpMPI_FIFO_Buffer== NULL");
		return ERROR;
	}

	if(gBBBuffer_ridx == ((gBBBuffer_widx + 1)%TDMB_MPI_BUF_CHUNK_NUM))
	{	
		printk("======================================\n");
		printk("### buffer is full, skip the data (ridx=%d, widx=%d)  ###\n", gBBBuffer_ridx, gBBBuffer_widx);
		printk("======================================\n");
		return ERROR;
	}

	read_buffer_ptr = gpMPI_Buffer + gBBBuffer_widx*TDMB_MPI_BUF_SIZE;
	
	tunerbb_drv_lg2102_read_data(read_buffer_ptr, &read_buffer_size);
	tdmb_real_read_size[gBBBuffer_widx] = read_buffer_size;

	/* update write index */
	if ( 0 < read_buffer_size )
	{
		gBBBuffer_widx = ((gBBBuffer_widx + 1)%TDMB_MPI_BUF_CHUNK_NUM);
	}

	//printk("broadcast_tdmb_read_data, ridx=%d, widx=%d, wsize=%d\n",gBBBuffer_ridx, gBBBuffer_widx,  read_buffer_size);
	
	return OK;
}


