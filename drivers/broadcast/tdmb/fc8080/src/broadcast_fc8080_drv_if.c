#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>        /* copy_to_user */
#include <linux/slab.h>

#include "../inc/tdmb_tunerbbdrv_fc8080def.h"

#define FC8080_USES_STATIC_BUFFER

#define TDMB_MPI_BUF_SIZE 			(188*16*4 + 8)//interrupt size + sizeof(TDMB_BB_HEADER_TYPE)
#define TDMB_MPI_BUF_CHUNK_NUM  	10

static uint8*	gpMPI_Buffer = NULL;
static uint8	gBBBuffer_ridx = 0;
static uint8	gBBBuffer_widx = 0;
static uint32	tdmb_real_read_size[TDMB_MPI_BUF_CHUNK_NUM];
static unsigned int s_opmode = FC8080_SERVICE_MAX;

#ifdef FC8080_USES_STATIC_BUFFER
static uint8	gpMPI_Array[TDMB_MPI_BUF_SIZE*TDMB_MPI_BUF_CHUNK_NUM];
#endif // FC8080_USES_STATIC_BUFFER

int broadcast_drv_if_power_on(void)
{
	int8 res = ERROR;
	boolean retval = FALSE;

	if(gpMPI_Buffer == NULL)
	{
#ifndef FC8080_USES_STATIC_BUFFER
		gpMPI_Buffer = kmalloc(TDMB_MPI_BUF_SIZE*TDMB_MPI_BUF_CHUNK_NUM, GFP_KERNEL);
#else // FC8080_USES_STATIC_BUFFER
		gpMPI_Buffer = (uint8*)&gpMPI_Array[0];
#endif // FC8080_USES_STATIC_BUFFER
	}
//                    
	if(tunerbb_drv_fc8080_is_on() == TRUE)
	{
		printk("tdmb_fc8080_power_on state true\n");

		retval = tunerbb_drv_fc8080_stop();
		retval = tunerbb_drv_fc8080_power_off();

		if(retval == TRUE)
		{
			res = OK;
		}
	}

	retval = tunerbb_drv_fc8080_power_on();

	if(retval == TRUE)
	{
		res = OK;
	}

	//tunerbb_drv_fc8080_set_userstop(1);

	return res;
}

int broadcast_drv_if_power_off(void)
{
	int8 res = ERROR;
	boolean retval = FALSE;

	retval = tunerbb_drv_fc8080_power_off();

	if(retval == TRUE)
	{
		res = OK;
	}
	//tunerbb_drv_fc8080_set_userstop(0);

	return res;
}

int broadcast_drv_if_open(void)
{
	int8 res = ERROR;
	boolean retval = FALSE;

	printk("broadcast_drv_if_open In\n");
	retval = tunerbb_drv_fc8080_init();
	printk("broadcast_drv_if_open  Out\n");
	if(retval == TRUE)
	{
		res = OK;
	}

	return res;
}

int broadcast_drv_if_close(void)
{
	int8 res = ERROR;

	if(tunerbb_drv_fc8080_is_on() == TRUE)
	{
		boolean retval = FALSE;
		printk("tdmb_fc8080_power_on state close-->stop\n");

		retval = tunerbb_drv_fc8080_stop();

		if(retval == TRUE)
		{
			res = OK;
		}
	}

	return res;
}

int broadcast_drv_if_set_channel(unsigned int freq_num, unsigned int subch_id, unsigned int op_mode)
{
	int8 rc = ERROR;
	boolean retval = FALSE;
	s_opmode = op_mode;
	gBBBuffer_ridx = gBBBuffer_widx = 0;

	retval = tunerbb_drv_fc8080_set_channel(freq_num, subch_id, op_mode);
	printk("broadcast_drv_if_set_channel result = (%d)\n", retval);

	if(retval == TRUE)
	{
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

	retval = tunerbb_drv_fc8080_re_syncdetector(op_mode);

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

	retval = tunerbb_drv_fc8080_get_ber(dmb_bb_info);

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
		printk("broadcast_drv_if_get_ch_info argument error\n");
		return rc;
	}

	retval = tunerbb_drv_fc8080_get_fic(buffer, buffer_size);

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

	retval = tunerbb_drv_fc8080_reset_ch();

	if(retval == TRUE)
	{
		res = OK;
	}

	return res;
}

int broadcast_drv_if_user_stop(int mode)
{
	tunerbb_drv_fc8080_set_userstop(mode);
	return OK;
}

int broadcast_drv_if_select_antenna(unsigned int sel)
{
	tunerbb_drv_fc8080_select_antenna(sel);
	return OK;
}

int broadcast_drv_if_isr(void)
{
	uint8* 	read_buffer_ptr 	= NULL;
	uint32 	read_buffer_size 	= 0;

	//printk("fc8080 broadcast_drv_if_isr in ++");

	if(gpMPI_Buffer == NULL)
	{
		printk("gpMPI_FIFO_Buffer== NULL");
		return ERROR;
	}

	// Modified by suyong.han 20110922
	/*
	if(gBBBuffer_ridx == ((gBBBuffer_widx + 1)%TDMB_MPI_BUF_CHUNK_NUM))
	{
		// Added by suyong.han 20110921
		read_buffer_ptr = gpMPI_Buffer + gBBBuffer_widx*TDMB_MPI_BUF_SIZE;
		tunerbb_drv_fc8080_read_data(read_buffer_ptr, &read_buffer_size);

		printk("======================================\n");
		printk("### buffer is full, skip the data (ridx=%d, widx=%d)  ###\n", gBBBuffer_ridx, gBBBuffer_widx);
		printk("======================================\n");

		// Removed by suyong.han 20110921
		//gBBBuffer_ridx = gBBBuffer_widx;

		return ERROR;
	}

	read_buffer_ptr = gpMPI_Buffer + gBBBuffer_widx*TDMB_MPI_BUF_SIZE;
	tunerbb_drv_fc8080_read_data(read_buffer_ptr, &read_buffer_size);
	*/
	read_buffer_ptr = gpMPI_Buffer + gBBBuffer_widx*TDMB_MPI_BUF_SIZE;
#if !defined(STREAM_TS_UPLOAD)
	tunerbb_drv_fc8080_read_data(read_buffer_ptr, &read_buffer_size);
#endif
	if(gBBBuffer_ridx == ((gBBBuffer_widx + 1)%TDMB_MPI_BUF_CHUNK_NUM))
	{
		//printk("======================================\n");
		if(s_opmode != FC8080_BLT_TEST)
		{
			printk("### buffer is full, skip the data (ridx=%d, widx=%d)  ###\n", gBBBuffer_ridx, gBBBuffer_widx);
		}
		//printk("======================================\n");
		return ERROR;
	}

	if(read_buffer_size > 0)
	{
		tdmb_real_read_size[gBBBuffer_widx] = read_buffer_size;
		gBBBuffer_widx = ((gBBBuffer_widx + 1)%TDMB_MPI_BUF_CHUNK_NUM);
		return OK;
	}

	//printk("broadcast_tdmb_read_data, ridx=%d, widx=%d, wsize=%d\n",gBBBuffer_ridx, gBBBuffer_widx,  read_buffer_size);
	//printk("fc8080 broadcast_drv_if_isr out --");

	return ERROR;
}
