//==============================================================================
// FujiFlim OIS firmware
//==============================================================================
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <mach/camera2.h>
#include <linux/poll.h> 
#include "msm_ois.h"
#include "msm_ois_i2c.h"

#define LAST_UPDATE "13-08-02, 11B"

#define E2P_FIRST_ADDR 			(0x0710)
#define E2P_DATA_BYTE			(28)
#define CTL_END_ADDR_FOR_E2P_DL	(0x13A8)

#define OIS_START_DL_ADDR		(0xF010)
#define OIS_COMPLETE_DL_ADDR	(0xF006)
#define OIS_READ_STATUS_ADDR	(0x6024)
#define OIS_CHECK_SUM_ADDR		(0xF008)

#define LIMIT_STATUS_POLLING	(10)
#define LIMIT_OIS_ON_RETRY		(5)

static struct ois_i2c_bin_list FF_VERX_REL_BIN_DATA = 
{
	.files = 3,
	.entries = 
	{
		{
			.filename = "DLdata_rev11B_data1.ecl",
			.filesize = 0x0AB8,
			.blocks = 1,
			.addrs = {
				{0x0000,0x0AB7,0x0000},
				}
		},
		{
			.filename = "DLdata_rev11B_data2.ecl",
			.filesize = 0x00D4,
			.blocks = 1,
			.addrs = {
				{0x0000,0x00D3,0x5400},
				}
		},
		{
			.filename = "DLdata_rev11B_data3.ecl",
			.filesize = 0x009C,
			.blocks = 1,
			.addrs = {
				{0x0000,0x009B,0x1188},
				}
		}
	},
	.checksum = 0x000367CE
};

static int fuji_ois_poll_ready(int limit)
{
	uint8_t ois_status;
	int read_byte = 0;

	RegReadA(OIS_READ_STATUS_ADDR, &ois_status); //polling status ready
	read_byte++;

	while((ois_status!=0x01) && (read_byte<limit))
	{
		usleep(2000); //wait 2ms
		RegReadA(OIS_READ_STATUS_ADDR, &ois_status); //polling status ready
		read_byte++;
	}
	
	CDBG("%s, 0x6024 read_byte = %d %d\n",__func__, read_byte, ois_status);
	return ois_status;
}

int fuji_bin_download(struct ois_i2c_bin_list bin_list)
{
	int rc = 0;
	int cnt = 0;
	int32_t read_value_32t;
	
	CDBG("%s\n", __func__);

	/* check OIS ic is alive */
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
	{
		printk("%s: no reply 1\n",__func__);
		rc = OIS_INIT_I2C_ERROR;
		goto END;
	}

	/* Send command ois start dl */
	rc = RegWriteA(OIS_START_DL_ADDR, 0x00);
	
	while (rc < 0 && cnt < LIMIT_STATUS_POLLING)
	{
		usleep(2000);
		rc = RegWriteA(OIS_START_DL_ADDR, 0x00);
		cnt ++;
	}
	
	if (rc < 0) 
	{
		printk("%s: no reply 2\n",__func__);
		rc = OIS_INIT_I2C_ERROR;
		goto END;
	}

	/* OIS program downloading */
	rc = ois_i2c_load_and_write_bin_list(bin_list);
	if (rc < 0)
	{
		goto END;
	}

	/* Check sum value!*/
	RamRead32A( OIS_CHECK_SUM_ADDR , &read_value_32t );
	if(read_value_32t != bin_list.checksum)
	{
		printk("%s: sum = 0x%x\n",__func__, read_value_32t);
		rc = OIS_INIT_CHECKSUM_ERROR;
		goto END;
	}

	rc = ois_i2c_load_and_write_e2prom_data(E2P_FIRST_ADDR, E2P_DATA_BYTE, CTL_END_ADDR_FOR_E2P_DL);
	if (rc < 0)
	{
		goto END;
	}

	/* Send command ois complete dl */
	RegWriteA(OIS_COMPLETE_DL_ADDR, 0x00) ;

	/* Read ois status */
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
	{
		printk("%s: no reply 3\n",__func__);
		rc = OIS_INIT_TIMEOUT;
		goto END;
	}
	
	printk("%s, complete dl FINISHED! \n",__func__);

END:
	return rc;
}

int fuji_ois_write_8bytes(uint16_t addr, uint32_t data_u, uint32_t data_d)
{
	RegWriteA(addr++, 0xFF & (data_u >> 24));
	RegWriteA(addr++, 0xFF & (data_u >> 16));
	RegWriteA(addr++, 0xFF & (data_u >> 8));
	RegWriteA(addr++, 0xFF & (data_u));
	RegWriteA(addr++, 0xFF & (data_d >> 24));
	RegWriteA(addr++, 0xFF & (data_d >> 16));
	RegWriteA(addr++, 0xFF & (data_d >> 8));
	RegWriteA(addr  , 0xFF & (data_d));

	return OIS_SUCCESS;
}

int fuji_ois_init_cmd(int limit)
{
	/*	lens lift routine. currently disabled.
	uint8_t wdata[2];
	wdata[0] = (350 & 0xFFF0)>>4; 
	wdata[1] = ((350 & 0x000F)<<4) | 0b0100 ; 
	ois_i2c_act_write(wdata[0], wdata[1]);
	CDBG("%s, data %x, %x",__func__, wdata[0], wdata[1]);
	usleep(100000); // 100ms sleep
	*/
	
	int trial = 0;
	do{
	RegWriteA(0x6020, 0x01);
	trial++;
	}while(trial<limit && !fuji_ois_poll_ready(LIMIT_STATUS_POLLING));

	if (trial == limit) { return OIS_INIT_TIMEOUT; } // initialize fail

	fuji_ois_write_8bytes(0x6080, 0x504084C3, 0x02FC0000);
	fuji_ois_write_8bytes(0x6080, 0x504088C3, 0xFD040000);
	fuji_ois_write_8bytes(0x6080, 0x505084C3, 0x02FC0000);
	fuji_ois_write_8bytes(0x6080, 0x505088C3, 0xFD040000);

	RegWriteA(0x602C, 0x1B);
	RegWriteA(0x602D, 0x00);
	RegWriteA(0x6023, 0x04);
	RegWriteA(0x6025, 0x40); // Set limit angle

	return OIS_SUCCESS;
}


static struct msm_ois_fn_t fuji_ois_func_tbl;

int32_t fuji_ois_mode(enum ois_mode_t data)
{
	int cur_mode = fuji_ois_func_tbl.ois_cur_mode;
	printk("%s:%d\n", __func__,data);

	if ( cur_mode == data)
	{
		return 0;
	}

	if (cur_mode != OIS_MODE_CENTERING_ONLY)
	{	//go to lens centering mode
		RegWriteA(0x6020, 0x01); 
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
	}

	switch(data)
	{
		case OIS_MODE_PREVIEW_CAPTURE :
		case OIS_MODE_CAPTURE : 
			CDBG("%s:%d, %d preview capture \n", __func__,data, cur_mode);
			RegWriteA(0x6021, 0x10);
			RegWriteA(0x6020, 0x02); 
			if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
			break;
		case OIS_MODE_VIDEO : 
			CDBG("%s:%d, %d capture \n", __func__,data, cur_mode);
			RegWriteA(0x6021, 0x11);
			RegWriteA(0x6020, 0x02);
			if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
			break;
		case OIS_MODE_CENTERING_ONLY :
			CDBG("%s:%d, %d centering_only \n", __func__,data, cur_mode);
			break;
		case OIS_MODE_CENTERING_OFF :
			CDBG("%s:%d, %d centering_off \n", __func__,data, cur_mode);
			RegWriteA(0x6020, 0x00); 
			if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
			break;
	}
	
	fuji_ois_func_tbl.ois_cur_mode = data;
	return 0;
}


int32_t fuji_ois_move_lens(int16_t target_x, int16_t target_y);

int32_t	fuji_ois_on ( enum ois_ver_t ver )
{
	int32_t rc = OIS_SUCCESS;
	printk("%s, %s\n", __func__,LAST_UPDATE);

	rc = fuji_bin_download(FF_VERX_REL_BIN_DATA);
	if (rc < 0)
	{
		CDBG("%s: init fail \n", __func__);
		return rc;
	}
	
	switch(ver)
	{
		case OIS_VER_RELEASE:
		{
			rc = fuji_ois_init_cmd(LIMIT_OIS_ON_RETRY);	
			if (rc < 0)
			{
				CDBG("%s: init fail \n", __func__);
				return rc;
			}

		}
		break;	
		case OIS_VER_DEBUG:
		case OIS_VER_CALIBRATION:
		{	
			rc = fuji_ois_init_cmd(LIMIT_OIS_ON_RETRY);	
			if (rc < 0)
			{
				CDBG("%s: init fail \n", __func__);
				return rc;
			}
			
			RegWriteA(0x6020, 0x01); 
			if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
			fuji_ois_move_lens(0x00,0x00); // force move to center
		}
		break;
	}

	fuji_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;
	//usleep(1000000);

	CDBG("%s : exit!\n", __func__);
	return rc;
}

int32_t	fuji_ois_off(void)
{	
	int16_t i;
	printk("%s enter\n", __func__);

	for (i = 0x01C9; i > 0; i-=46)
	{
		fuji_ois_write_8bytes(0x6080, 0x504084C3, i << 16);		 //high limit xch
		fuji_ois_write_8bytes(0x6080, 0x504088C3, (-i) << 16);   //low limit xch
		fuji_ois_write_8bytes(0x6080, 0x505084C3, i << 16);		 //high limit ych
		fuji_ois_write_8bytes(0x6080, 0x505088C3, (-i) << 16);	 //low limit ych
		usleep(5000);
	}
	
	printk("%s exit\n", __func__);	
	return 0;
}

int16_t fuji_convert_int32(int32_t in)
{
	if (in > 32767) return 32767;
	if (in < -32767) return -32767;
	return 0xFFFF & in;
}

/*
 [Integration note]
  gyro[dps] = gyro[int16] / 125
  gyro[dps*256] = gyro[int16]*256/125

  hall[code] : hall[um] = 46:67.397
  hall[code] = hall[um]*46/67.397
             = hall[um]*256/375
             = hall[um*256]/375             
*/

#define GYRO_SCALE_FACTOR 125
#define HALL_SCALE_FACTOR 375

int32_t fuji_ois_stat(struct msm_sensor_ois_info_t *ois_stat)
{
	int16_t gyro = 0;
		
	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider), "FF_ROHM");

	RamReadA(0x6042, &gyro);
	CDBG("%s gyrox %x \n",__func__, gyro);
	ois_stat->gyro[0] = fuji_convert_int32(((int32_t)gyro)*256/GYRO_SCALE_FACTOR);

	ois_stat->target[0] = 0; //not supported
	
	ois_stat->hall[0] = 1; //not supported

	RamReadA(0x6044, &gyro);
	CDBG("%s gyroy %x \n",__func__, gyro);
	ois_stat->gyro[1] = fuji_convert_int32(((int32_t)gyro)*256/GYRO_SCALE_FACTOR);

	ois_stat->target[1] = 0; //not supported

	ois_stat->hall[1] = 1; //not supported
	
	ois_stat->is_stable = 1; // true
	
	return 0;
}

int32_t fuji_ois_move_lens(int16_t target_x, int16_t target_y)
{
	int8_t hallx =  target_x/HALL_SCALE_FACTOR;
	int8_t hally =  target_y/HALL_SCALE_FACTOR;
	uint8_t result = 0;

	// check ois mode & change to suitable mode
	RegReadA(0x6020, &result);
	if (result != 0x01)
	{
		RegWriteA(0x6020, 0x01); 
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
	}

	printk("%s target : %d, %d\n", __func__, hallx, hally);

	// hallx range -> D2 to 2E (-46, 46)
	RegWriteA(0x6099, 0xFF & hallx); // target x position input
	RegWriteA(0x609A, 0xFF & hally); // target y position input
	RegWriteA(0x6098, 0x01); // order to move.
	
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING*2)) { return OIS_INIT_TIMEOUT; }
	
	RegReadA(0x609B, &result);

	if (result == 0x03) { return  OIS_SUCCESS; }

	printk("%s move fail : %x \n", __func__, result);
	
	return OIS_FAIL;
}


void fuji_ois_init(struct msm_ois_ctrl_t *msm_ois_t)
{
	fuji_ois_func_tbl.ois_on = fuji_ois_on;
    fuji_ois_func_tbl.ois_off = fuji_ois_off;
    fuji_ois_func_tbl.ois_mode = fuji_ois_mode;
    fuji_ois_func_tbl.ois_stat = fuji_ois_stat;
	fuji_ois_func_tbl.ois_move_lens = fuji_ois_move_lens;
    fuji_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;
	msm_ois_t->sid_ois = 0x7C >> 1;
	msm_ois_t->ois_func_tbl = &fuji_ois_func_tbl;
}


