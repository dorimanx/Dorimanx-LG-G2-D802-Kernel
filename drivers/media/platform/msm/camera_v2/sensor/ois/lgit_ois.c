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

#define LAST_UPDATE "13-06-26, 9_5M"

#define E2P_FIRST_ADDR 			(0x0900)
#define E2P_DATA_BYTE			(28)
#define CTL_END_ADDR_FOR_E2P_DL	(0x13A8)

#define OIS_START_DL_ADDR		(0xF010)
#define OIS_COMPLETE_DL_ADDR	(0xF006)
#define OIS_READ_STATUS_ADDR	(0x6024)
#define OIS_CHECK_SUM_ADDR		(0xF008)

#define LIMIT_STATUS_POLLING	(10)
#define LIMIT_OIS_ON_RETRY		(5)

static struct ois_i2c_bin_list LGIT_VER2_REL_BIN_DATA = 
{
	.files = 3,
	.entries = 
	{
		{
			.filename = "bu24205_LGIT_rev7_data1.bin",
			.filesize = 0x0894,
			.blocks = 1,
			.addrs = {
				{0x0000,0x0893,0x0000},
				}
		},
		{
			.filename = "bu24205_LGIT_rev7_data2.bin",
			.filesize = 0x00D4,
			.blocks = 1,
			.addrs = {
				{0x0000,0x00D3,0x5400},
				}
		},
		{
			.filename = "bu24205_LGIT_rev7_data3.bin",
			.filesize = 0x0098,
			.blocks = 1,
			.addrs = {
				{0x0000,0x0097,0x1188},
				}
		}
	},
	.checksum = 0x0002F737
};

static struct ois_i2c_bin_list LGIT_VER3_REL_BIN_DATA = 
{
	.files = 3,
	.entries = 
	{
		{
			.filename = "bu24205_LGIT_rev7_1_data1.bin",
			.filesize = 0x0894,
			.blocks = 1,
			.addrs = {
				{0x0000,0x0893,0x0000},
				}
		},
		{
			.filename = "bu24205_LGIT_rev7_1_data2.bin",
			.filesize = 0x00D4,
			.blocks = 1,
			.addrs = {
				{0x0000,0x00D3,0x5400},
				}
		},
		{
			.filename = "bu24205_LGIT_rev7_1_data3.bin",
			.filesize = 0x0098,
			.blocks = 1,
			.addrs = {
				{0x0000,0x0097,0x1188},
				}
		}
	},
	.checksum = 0x0002F836
};

static struct ois_i2c_bin_list LGIT_VER3_CAL_BIN_DATA = 
{
	.files = 1,
	.entries = 
	{
		{
			.filename = "bu24205_LGIT_rev9_5M.bin",
			.filesize = 0x54D4,
			.blocks = 4,
			.addrs = {
				{0x5400,0x54D3,0x5400},
				{0x0000,0x0FAB,0x0000},
				{0x1188,0x1223,0x1188},
				{0x1600,0x1ADB,0x1600}
				}
		}
	},
	.checksum = 0x00063FAB
};

static int lgit2_ois_poll_ready(int limit)
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

int lgit2_bin_download(struct ois_i2c_bin_list bin_list)
{
	int rc = 0;
	int cnt = 0;
	int32_t read_value_32t; 
	
	CDBG("%s\n", __func__);

	/* check OIS ic is alive */
	if (!lgit2_ois_poll_ready(LIMIT_STATUS_POLLING))
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
		printk("%s: no reply \n",__func__);
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
	if (!lgit2_ois_poll_ready(LIMIT_STATUS_POLLING))
	{
			printk("%s: no reply 3\n",__func__);
			rc = OIS_INIT_TIMEOUT;
			goto END;
	}
	
	printk("%s, complete dl FINISHED! \n",__func__);

END:
	return rc;
}

int lgit2_ois_init_cmd(int limit)
{
	int trial = 0;
	
	do{
		RegWriteA(0x6020, 0x01);
		trial++;
	} while(trial<limit && !lgit2_ois_poll_ready(LIMIT_STATUS_POLLING));

	if (trial == limit) { return OIS_INIT_TIMEOUT; }
	
	RegWriteA(0x6023, 0x04);// gyro on
	if (!lgit2_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
	
	return OIS_SUCCESS;
}

static struct msm_ois_fn_t lgit2_ois_func_tbl;

int lgit2_ois_mode(enum ois_mode_t data)
{
	int cur_mode = lgit2_ois_func_tbl.ois_cur_mode;
	printk("%s:%d\n", __func__,data);

	if ( cur_mode == data)
	{
		return OIS_SUCCESS;
	}
	
	if ( cur_mode != OIS_MODE_CENTERING_ONLY)
	{
		RegWriteA(0x6020, 0x01);
		if (!lgit2_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
	}
	
	switch(data)
	{
		case OIS_MODE_PREVIEW_CAPTURE :
		case OIS_MODE_VIDEO : 
			CDBG("%s:%d, %d preview capture \n", __func__,data, cur_mode);
			RegWriteA(0x6021, 0x11);
			RegWriteA(0x6020, 0x02);
			break;
		case OIS_MODE_CAPTURE : 
			CDBG("%s:%d, %d capture \n", __func__,data, cur_mode);
			RegWriteA(0x6021, 0x00); // still mode, pan-off 
			RegWriteA(0x6020, 0x02); // ois on
			break;
		case OIS_MODE_CENTERING_ONLY :
			CDBG("%s:%d, %d centering_only \n", __func__,data, cur_mode);
			break;
		case OIS_MODE_CENTERING_OFF :
			CDBG("%s:%d, %d centering_off \n", __func__,data, cur_mode);
			RegWriteA(0x6020, 0x00); //lens centering off
			if (!lgit2_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
			break;
	}

	lgit2_ois_func_tbl.ois_cur_mode = data;
	
	return OIS_SUCCESS;
}

int32_t	lgit2_ois_on ( enum ois_ver_t ver  )
{
	int32_t rc = OIS_SUCCESS;
	uint16_t cal_ver = 0;
	
	printk("%s, %s\n", __func__,LAST_UPDATE);

	ois_i2c_e2p_read(E2P_FIRST_ADDR+0x1C, &cal_ver, 2);
	printk("%s ver %x\n", __func__,cal_ver);

	switch(cal_ver)
	{
		case 1:
			rc = lgit2_bin_download(LGIT_VER2_REL_BIN_DATA);
			break;
		case 2:
			rc = lgit2_bin_download(LGIT_VER3_CAL_BIN_DATA);
			if (rc < 0)
			{
				rc = lgit2_bin_download(LGIT_VER3_REL_BIN_DATA);
			}
			break;	
		default:
			rc = OIS_INIT_NOT_SUPPORTED;
			break;
	}

	if (rc < 0)
	{
		CDBG("%s: init fail \n", __func__);
		return rc;
	}
	
	rc = lgit2_ois_init_cmd(LIMIT_OIS_ON_RETRY);

	lgit2_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;
	
	CDBG("%s : complete!\n", __func__);	
	return rc;
}

int32_t	lgit2_ois_off(void)
{	
	printk("%s enter\n", __func__);
	
	RegWriteA(0x6020, 0x01);
	if (!lgit2_ois_poll_ready(LIMIT_STATUS_POLLING)) { return OIS_INIT_TIMEOUT; }
	RegWriteA(0x609E, 0x01);
	usleep(100000);
	
	printk("%s exit\n", __func__);
	return OIS_SUCCESS;
}

int lgit2_ois_stat(struct msm_sensor_ois_info_t *ois_stat)
{
	uint8_t buf[2];	
	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider), "LGIT_ROHM");

	// x target read-out request
	RegWriteA(0x6094, 0x04) ; 

	ois_i2c_read_seq(0x6044, buf, 2);
	CDBG("%s gyrox %x%x \n",__func__, buf[0], buf[1]);
	ois_stat->gyro[0] = buf[0]<<8 | buf[1];
	
	ois_i2c_read_seq(0x6096, buf, 2); 
	CDBG("%s targetx %x%x \n",__func__, buf[0], buf[1]);
	ois_stat->target[0] = 0xFFFF & ((int16_t)(buf[0]<<8 | buf[1])*41); // scaled to match hallx 

	ois_i2c_read_seq(0x6058, buf, 1);
	CDBG("%s hallx %x \n",__func__, buf[0]);
	ois_stat->hall[0] = 0xFFFF & (((int8_t)buf[0])*256);	//signed 8bit -> signed 16bit

	// y target read-out request
	RegWriteA(0x6094, 0x05) ; 

	ois_i2c_read_seq(0x6042, buf, 2);
	CDBG("%s gyroy %x%x \n",__func__, buf[0], buf[1]);
	ois_stat->gyro[1] = buf[0]<<8 | buf[1];
	
	ois_i2c_read_seq(0x6096, buf, 2);
	CDBG("%s targety %x%x \n",__func__, buf[0], buf[1]);
	ois_stat->target[1] = 0xFFFF & ((int16_t)(buf[0]<<8 | buf[1])*41); // scaled to match hally 

	ois_i2c_read_seq(0x6059, buf, 1);
	CDBG("%s hally %x \n",__func__, buf[0]);
	ois_stat->hall[1] = 0xFFFF & (((int8_t)buf[0])*256);


	if (ois_stat->hall[0] != 0 || ois_stat->hall[1] != 0 
		|| ois_stat->gyro[0] != 0 || ois_stat->gyro[1] != 0) { // is alive.
		ois_stat->is_stable = 1; 
	} else {
		ois_stat->is_stable = 0; 
	}

	return OIS_SUCCESS;
}

int32_t lgit2_ois_move_lens(int16_t offset_x, int16_t offset_y)
{
	// implement here
	return OIS_FAIL;
}

void lgit2_ois_init(struct msm_ois_ctrl_t *msm_ois_t)
{
	lgit2_ois_func_tbl.ois_on = lgit2_ois_on;
    lgit2_ois_func_tbl.ois_off = lgit2_ois_off;
    lgit2_ois_func_tbl.ois_mode = lgit2_ois_mode;
    lgit2_ois_func_tbl.ois_stat = lgit2_ois_stat;
	lgit2_ois_func_tbl.ois_move_lens = lgit2_ois_move_lens;
 	lgit2_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	msm_ois_t->sid_ois = 0x7C >> 1;
	msm_ois_t->ois_func_tbl = &lgit2_ois_func_tbl;
}


