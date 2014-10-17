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

#define LAST_UPDATE "13-11-28, 28B"

#define GET_OLD_MODULE_ID
#define E2P_FIRST_ADDR 			(0x0710)
#define E2P_DATA_BYTE			(28)
#define CTL_END_ADDR_FOR_E2P_DL	(0x13A8)

#define OIS_START_DL_ADDR		(0xF010)
#define OIS_COMPLETE_DL_ADDR	(0xF006)
#define OIS_READ_STATUS_ADDR	(0x6024)
#define OIS_CHECK_SUM_ADDR		(0xF008)

#define LIMIT_STATUS_POLLING	(10)
#define LIMIT_OIS_ON_RETRY		(5)
#define LIMIT_GYRO_CALIBRATION  (5)

#define GYRO_SCALE_FACTOR 262
#define HALL_SCALE_FACTOR 187

static struct ois_i2c_bin_list FF_VERX_REL_BIN_DATA =
{
	.files = 3,
	.entries =
	{
		{
			.filename = "DLdata_rev28B_data1.ecl",
			.filesize = 0x0E48,
			.blocks = 1,
			.addrs = {
				{0x0000,0x0E47,0x0000},
				}
		},
		{
			.filename = "DLdata_rev28B_data2.ecl",
			.filesize = 0x00D4,
			.blocks = 1,
			.addrs = {
				{0x0000,0x00D3,0x5400},
				}
		},
		{
			.filename = "DLdata_rev28B_data3.ecl",
			.filesize = 0x0720,
			.blocks = 1,
			.addrs = {
				{0x0000,0x071F,0x1188},
				}
		}
	},
	.checksum = 0x0005488e
};

int8_t from_signed_byte(uint8_t byte)
{
    byte &= 255;
    if (byte > 127) return byte - 256;
    else
		return byte;
}

int16_t from_signed_word(uint16_t word)
{
    word &= 65535;
    if (word > 32767) return word - 65536;
    else
		return word;
}

static int fuji_ois_poll_ready(int limit)
{
	uint8_t ois_status;
	int read_byte = 0;
	
	/* polling status ready */
	RegReadA(OIS_READ_STATUS_ADDR, &ois_status); 
	read_byte++;

	while ((ois_status != 0x01) && (read_byte < limit)) {
		usleep(1000); /* wait 1ms */
		RegReadA(OIS_READ_STATUS_ADDR, &ois_status); /* polling status ready */
		read_byte++;
	}
	return ois_status;
}

int fuji_bin_download(struct ois_i2c_bin_list bin_list)
{
	int rc = 0;
	int cnt = 0;
	int32_t read_value_32t;

	/* check OIS ic is alive */
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) {
		printk("%s: no reply 1\n", __func__);
		rc = OIS_INIT_I2C_ERROR;
		goto END;
	}

	/* Send command ois start dl */
	rc = RegWriteA(OIS_START_DL_ADDR, 0x00);

	while (rc < 0 && cnt < LIMIT_STATUS_POLLING) {
		usleep(2000);
		rc = RegWriteA(OIS_START_DL_ADDR, 0x00);
		cnt++;
	}

	if (rc < 0) {
		printk("%s: no reply 2\n", __func__);
		rc = OIS_INIT_I2C_ERROR;
		goto END;
	}

	/* OIS program downloading */
	rc = ois_i2c_load_and_write_bin_list(bin_list);
	if (rc < 0)
		goto END;
	
	/* Check sum value!*/
	RamRead32A( OIS_CHECK_SUM_ADDR , &read_value_32t );
	if (read_value_32t != bin_list.checksum) {
		printk("%s: error [0xF008]checksum = 0x%x, bin_checksum 0x%x\n", __func__,
			read_value_32t, bin_list.checksum);
		rc = OIS_INIT_CHECKSUM_ERROR;
		goto END;
	}

	rc = ois_i2c_load_and_write_e2prom_data(E2P_FIRST_ADDR, E2P_DATA_BYTE,
		CTL_END_ADDR_FOR_E2P_DL);
	if (rc < 0)
		goto END;

	/* Send command ois complete dl */
	RegWriteA(OIS_COMPLETE_DL_ADDR, 0x00) ;

	/* Read ois status */
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) {
		printk("%s: no reply 3\n", __func__);
		rc = OIS_INIT_TIMEOUT;
		goto END;
	}

	printk("%s, complete dl FINISHED! \n", __func__);

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
	RegWriteA(addr, 0xFF & (data_d));

	return OIS_SUCCESS;
}

int fuji_ois_init_cmd(int limit, int ver)
{
	uint16_t gyro_intercept_x,gyro_intercept_y = 0;
	int16_t gyro_signed_intercept_x,gyro_signed_intercept_y = 0;
	uint16_t gyro_slope_x,gyro_slope_y = 0;
	int16_t gyro_signed_slope_x,gyro_signed_slope_y = 0;

	int trial = 0;
	uint16_t gyro_temp = 0;
	int16_t gyro_signed_temp = 0;
	int16_t gyro_signed_offset_x = 0, gyro_signed_offset_y = 0;
	
	do {
		RegWriteA(0x6020, 0x01);
		trial++;
	} while (trial < limit && !fuji_ois_poll_ready(LIMIT_STATUS_POLLING));

	if (trial == limit)
		return OIS_INIT_TIMEOUT; /* initialize fail */

	fuji_ois_write_8bytes(0x6080, 0x504084C3, 0x02FC0000);
	fuji_ois_write_8bytes(0x6080, 0x504088C3, 0xFD040000);
	fuji_ois_write_8bytes(0x6080, 0x505084C3, 0x02FC0000);
	fuji_ois_write_8bytes(0x6080, 0x505088C3, 0xFD040000);

	RegWriteA(0x602C, 0x1B);
	RegWriteA(0x602D, 0x00);
	
	/* common, cal 8, init 8 */
	switch (ver) {
	case OIS_VER_CALIBRATION:
	case OIS_VER_DEBUG:
		RegWriteA(0x6023, 0x00); 
		break;
	case OIS_VER_RELEASE:
	default:
		RegWriteA(0x602c, 0x41); 
		RamReadA(0x602D, &gyro_temp);
		
		ois_i2c_e2p_read(0x070A, &gyro_intercept_x, 2);
		ois_i2c_e2p_read(0x070C, &gyro_intercept_y, 2);
		ois_i2c_e2p_read(0x072C, &gyro_slope_x, 2);
		ois_i2c_e2p_read(0x072E, &gyro_slope_y, 2);

		gyro_signed_slope_x = from_signed_word(gyro_slope_x);
		gyro_signed_slope_y = from_signed_word(gyro_slope_y);		
		gyro_signed_temp = from_signed_word(gyro_temp);
		gyro_signed_intercept_x = from_signed_word(gyro_intercept_x);
		gyro_signed_intercept_y = from_signed_word(gyro_intercept_y);

		CDBG("%s [0x070A]gyro_intercept_x 0x%x \n", __func__, gyro_intercept_x);
		CDBG("%s [0x070C]gryo_intercept_y 0x%x \n", __func__, gyro_intercept_y);
		CDBG("%s [0x072C]gyro_slope_x 0x%x \n", __func__, gyro_slope_x);
		CDBG("%s [0x072E]gyro_slope_y 0x%x \n", __func__, gyro_slope_y);
		
		gyro_signed_offset_x = ((gyro_signed_slope_x * gyro_signed_temp) >> 15)
			+ gyro_signed_intercept_x;
		
		CDBG("%s gyro_signed_offset_x 0x%x \n", __func__, gyro_signed_offset_x);
		
		RegWriteA(0x609C,0x00);
		RegWriteA(0x609D, 0xFFFF & (uint16_t)gyro_signed_offset_x);			
		RegWriteA(0x609C,0x01);
		
		gyro_signed_offset_y= ((gyro_signed_slope_y * gyro_signed_temp) >> 15)
			+ gyro_signed_offset_y;
		
		CDBG("%s gyro_signed_offset_y 0x%x \n", __func__, gyro_signed_offset_y);
		
		RegWriteA(0x609D,0xFFFF & (uint16_t)gyro_signed_offset_y);	

		RegWriteA(0x6023, 0x04);
		usleep(200000); /* wait 200ms */

		break;
	}
	return OIS_SUCCESS;
}

int fuji_ois_gyro_calibration(int ver)
{
	uint16_t gyro_offset_x = 0, gyro_offset_y = 0;
	uint16_t gyro_temp = 0;
	int16_t gyro_intercept_x,gyro_intercept_y = 0;
	uint16_t gyro_diff_x = 0, gyro_diff_y = 0;	
	int16_t gyro_signed_diff_x = 0, gyro_signed_diff_y = 0;	
	uint16_t gyro_slope_x = 0,gyro_slope_y = 0;	
	uint16_t gyro_raw_x = 0,gyro_raw_y = 0;
	int16_t gyro_signed_differences_x = 0, gyro_signed_differences_y = 0;
	int16_t gyro_signed_slope_x = 0, gyro_signed_slope_y = 0, gyro_signed_temp = 0;
	int16_t gyro_signed_offset_x = 0, gyro_signed_offset_y = 0;
	int16_t gyro_signed_raw_x = 0, gyro_signed_raw_y = 0;

	CDBG("%s start \n",__func__);

	RegWriteA(0x6088, 0x00);
	usleep(10000);
	if (!fuji_ois_poll_ready(100)) { 		
		CDBG("%s fuji_ois_poll_ready error \n", __func__);
		return OIS_INIT_TIMEOUT;
	}
	
	RamReadA(0x608A, &gyro_offset_x);
    CDBG("%s read [0x608A]gyro_offset_x 0x%x \n", __func__, gyro_offset_x);
	
	RegWriteA(0x6023,0x02);
	RegWriteA(0x602c,0x41);
	RamReadA(0x602d,&gyro_temp);
	
    CDBG("%s read [0x602d]gtemp 0x%x \n", __func__, gyro_temp);	

	RegWriteA(0x6023,0x00);

	RegWriteA(0x6088, 0x01); 
	usleep(10000);
	
	if (!fuji_ois_poll_ready(100)) { 		
		CDBG("%s fuji_ois_poll_ready error \n", __func__);
		return OIS_INIT_TIMEOUT; 
	}
	RamReadA(0x608A, &gyro_offset_y); /* 21 */
	CDBG("%s read [0x608A]gyroy_offset_y 0x%x \n", __func__, gyro_offset_y);
		
	RegWriteA(0x609C, 0x00); /* 22 */

	RamWriteA(0x609D, (uint16_t)(0xFFFF & (gyro_offset_x))); /* 23 */	

	RegWriteA(0x609C, 0x01); /* 24 */
	RamWriteA(0x609D, (uint16_t)(0xFFFF & (gyro_offset_y))); /* 25 */
	usleep(10000);

	CDBG("%s write [0x609D]gyro_offset_x 0x%x [0x609D]gyro_offset_y 0x%x \n",
		__func__, gyro_offset_x, gyro_offset_y);
		
	RegWriteA(0x609C, 0x02); /* 26 */
	RamReadA(0x609D, &gyro_diff_x); /* 27 */

	RegWriteA(0x609C, 0x03);  /* 28 */
	RamReadA(0x609D, &gyro_diff_y);  /* 29 */

	gyro_signed_diff_x = from_signed_word(gyro_diff_x);
	gyro_signed_diff_y = from_signed_word(gyro_diff_y);

	/* 1dps */
	if ((abs(gyro_signed_diff_x) > 262) || (abs(gyro_signed_diff_y) > 262)) {
		CDBG("Gyro Offset Diff Y is FAIL!!!  %d %x\n",gyro_signed_diff_y,
			gyro_signed_diff_y);		
		CDBG("Gyro Offset Diff Y is FAIL!!!  %d %x\n",gyro_signed_diff_y,
			gyro_signed_diff_y);		
		return OIS_FAIL;
	}
	
	CDBG("%s read [0x609D]gyro_signed_diff_x 0x%x [0x609D]gyro_signed_diff_y 0x%x \n",
		__func__, gyro_signed_diff_x, gyro_signed_diff_y);

	ois_i2c_e2p_read(0x072C, &gyro_slope_x, 2);
	ois_i2c_e2p_read(0x072E, &gyro_slope_y, 2);

	gyro_signed_slope_x = from_signed_word(gyro_slope_x);
	gyro_signed_slope_y = from_signed_word(gyro_slope_y);
	gyro_signed_temp = from_signed_word(gyro_temp);
	gyro_signed_offset_x = from_signed_word(gyro_offset_x);
	gyro_signed_offset_y = from_signed_word(gyro_offset_y);

	CDBG("%s gyro_signed_slope_x 0x%x \n", __func__, gyro_signed_slope_x);
	CDBG("%s gyro_signed_slope_y 0x%x \n", __func__, gyro_signed_slope_y);
	CDBG("%s gyro_signed_offset_x 0x%x \n", __func__, gyro_signed_offset_x);
	CDBG("%s gyro_signed_offset_y 0x%x \n", __func__, gyro_signed_offset_y);	
	CDBG("%s gyro_signed_temp 0x%x \n", __func__, gyro_signed_temp);
	
	gyro_intercept_x = 
		gyro_signed_offset_x - ((gyro_signed_slope_x * gyro_signed_temp) >> 15);
	gyro_intercept_y = 
		gyro_signed_offset_y - ((gyro_signed_slope_y * gyro_signed_temp) >> 15);

	CDBG("%s calc gyro_intercept_x 0x%x \n", __func__, gyro_intercept_x);
	CDBG("%s calc gyro_intercept_y 0x%x \n", __func__, gyro_intercept_y);
	
	ois_i2c_e2p_write(0x070A, (uint16_t)(0xFFFF & gyro_intercept_x), 2);	
	usleep(10000);	
	ois_i2c_e2p_write(0x070C, (uint16_t)(0xFFFF & gyro_intercept_y), 2);
	usleep(10000);
	
	/*32 */
	RegWriteA(0x6023, 0x02);
	RegWriteA(0x602c, 0x41);
	RamReadA(0x602D, &gyro_temp);
	CDBG("%s read [0x602D]gyro_temp 0x%x \n", __func__, gyro_temp);
	
	RegWriteA(0x6023, 0x00);
	gyro_signed_temp = from_signed_word(gyro_temp);
	/* 36 */
	gyro_signed_offset_x = ((gyro_signed_slope_x * gyro_signed_temp) >> 15)
		+ gyro_intercept_x;
	gyro_signed_offset_y = ((gyro_signed_slope_y * gyro_signed_temp) >> 15)
		+ gyro_intercept_y;

	CDBG("%s calc gyro_signed_offset_x 0x%x \n", __func__, gyro_signed_offset_x);
	CDBG("%s calc gyro_signed_offset_y 0x%x \n", __func__, gyro_signed_offset_y);

	/* 37 ,38 */
	RamReadA(0x6042, &gyro_raw_x);
	RamReadA(0x6044, &gyro_raw_y);
	/* 40 */

	gyro_signed_raw_x = from_signed_word(gyro_raw_x);
	gyro_signed_raw_y = from_signed_word(gyro_raw_y);

	gyro_signed_differences_x = gyro_signed_offset_x + gyro_signed_raw_x;
	gyro_signed_differences_y = gyro_signed_offset_y - gyro_signed_raw_y;

	CDBG("%s calc gyro_differences_x 0x%x \n", __func__,
		gyro_signed_differences_x);
	CDBG("%s calc gyro_differences_y 0x%x \n", __func__,
		gyro_signed_differences_y);

	if ((abs(gyro_signed_differences_x) > 262) ||
		(abs(gyro_signed_differences_y) > 262)) {
		CDBG("Differences X is FAIL!!! %d %x\n",gyro_signed_differences_x,
			gyro_signed_differences_x);
		CDBG("Differences Y is FAIL!!! %d %x\n",gyro_signed_differences_y,
			gyro_signed_differences_y);
		return OIS_FAIL;
	}

	/* 41a */
	RegWriteA(0x6023, 0x02);
	/* 41 */
	RegWriteA(0x609C, 0x00);
	/* 42 */
	RegWriteA(0x609D, (uint16_t)(0xFFFF & (gyro_signed_offset_x)));
	/* 43 */
	RegWriteA(0x609C, 0x01);
	/* 44 */
	RegWriteA(0x609D, (uint16_t)(0xFFFF & (gyro_signed_offset_y)));
	/* 44a */
	RegWriteA(0x6023, 0x04);

	CDBG("%s end \n",__func__);
	return OIS_SUCCESS;
}

static struct msm_ois_fn_t fuji_ois_func_tbl;

int32_t fuji_ois_mode(enum ois_mode_t data)
{
	int cur_mode = fuji_ois_func_tbl.ois_cur_mode;
	printk("%s:%d\n", __func__,data);

	if (cur_mode == data)
		return 0;

	if (cur_mode != OIS_MODE_CENTERING_ONLY) {	
		/* go to lens centering mode */
		RegWriteA(0x6020, 0x01);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
	}

	switch(data) {
	case OIS_MODE_PREVIEW_CAPTURE:
	case OIS_MODE_CAPTURE:
		CDBG("%s:%d, %d preview capture \n", __func__,data, cur_mode);
		RegWriteA(0x6021, 0x10);
		RegWriteA(0x6020, 0x02);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		break;
	case OIS_MODE_VIDEO:
		CDBG("%s:%d, %d capture \n", __func__,data, cur_mode);
		RegWriteA(0x6021, 0x11);
		RegWriteA(0x6020, 0x02);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		break;
	case OIS_MODE_CENTERING_ONLY:
		CDBG("%s:%d, %d centering_only \n", __func__,data, cur_mode);
		break;
	case OIS_MODE_CENTERING_OFF:
		CDBG("%s:%d, %d centering_off \n", __func__,data, cur_mode);
		RegWriteA(0x6020, 0x00);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		break;
	}

	fuji_ois_func_tbl.ois_cur_mode = data;
	return 0;
}


int32_t fuji_ois_move_lens(int16_t target_x, int16_t target_y);

int32_t	fuji_ois_on (enum ois_ver_t ver)
{
	int32_t rc = OIS_SUCCESS;	
	int retry = 0;
#ifdef GET_OLD_MODULE_ID
	uint16_t ver_module = 0;
#endif	
	printk("%s, %s\n", __func__,LAST_UPDATE);

	rc = fuji_bin_download(FF_VERX_REL_BIN_DATA);
	if (rc < 0)	{
		CDBG("%s: init fail \n", __func__);
		return rc;
	}
	
	rc = fuji_ois_init_cmd(LIMIT_OIS_ON_RETRY,ver);
	if (rc < 0)	{
		CDBG("%s: init fail \n", __func__);
		return rc;
	}

	switch (ver) {
	case OIS_VER_RELEASE:
		break;
	case OIS_VER_CALIBRATION:
	case OIS_VER_DEBUG:
		
		/* RegWriteA(0x6020, 0x01); */
		RegWriteA(0x6021, 0x10);
		/* wait 50ms */
		usleep(50000);
		do {
			rc = fuji_ois_gyro_calibration(ver);
		} while (rc < 0 && retry++ < LIMIT_GYRO_CALIBRATION);

		if (rc < 0) {
			CDBG("%s: gyro cal fail \n", __func__);
			rc = OIS_INIT_GYRO_ADJ_FAIL; /* gyro failed. */
		}
		
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		fuji_ois_move_lens(0x00,0x00); /* force move to center */
		break;
	}

	fuji_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

#ifdef GET_OLD_MODULE_ID	
	ois_i2c_e2p_read(0x0730, &ver_module, 1);
	
	if (ver_module != 0x01)
	{
		CDBG("%s, Old module %x \n", __func__,ver_module);
		rc |= OIS_INIT_OLD_MODULE;
	}	
#endif
	return rc;
}

int32_t	fuji_ois_off(void)
{
	int16_t i;
	
	/* go to lens centering mode */
	RegWriteA(0x6020, 0x01);
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
		return OIS_INIT_TIMEOUT;
		
	for (i = 0x01C9; i > 0; i-= 46) {
		fuji_ois_write_8bytes(0x6080, 0x504084C3, i << 16);		 /*high limit xch */
		fuji_ois_write_8bytes(0x6080, 0x504088C3, (-i) << 16);   /* low limit xch */
		fuji_ois_write_8bytes(0x6080, 0x505084C3, i << 16);		 /* high limit ych */
		fuji_ois_write_8bytes(0x6080, 0x505088C3, (-i) << 16);	 /* low limit ych */
		/* wait 5ms */
		usleep(5000);
	}
	return 0; 
}

int32_t fuji_ois_stat(struct msm_sensor_ois_info_t *ois_stat)
{
	uint8_t	hall_x = 0, hall_y = 0;
	int16_t	hall_signed_x = 0, hall_signed_y = 0;
	uint16_t gyro_diff_x = 0, gyro_diff_y = 0;
	int16_t gyro_signed_diff_x = 0, gyro_signed_diff_y = 0;	
	
	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider), "FF_ROHM");

	/* OIS ON */
	RegWriteA(0x6020, 0x02);
	if (!fuji_ois_poll_ready(5))
		return OIS_INIT_TIMEOUT;

	RegWriteA(0x609C, 0x02); /* 26 */
	RamReadA(0x609D, &gyro_diff_x); /* 27 */

	RegWriteA(0x609C, 0x03); /* 28 */
	RamReadA(0x609D, &gyro_diff_y); /* 29 */

	gyro_signed_diff_x = from_signed_word(gyro_diff_x);
	gyro_signed_diff_y = from_signed_word(gyro_diff_y);

	/*                                                             */
	ois_stat->gyro[0] = (int16_t)gyro_signed_diff_x * 3;
	ois_stat->gyro[1] = (int16_t)gyro_signed_diff_y * 3;
	CDBG("%s gyro_signed_diff_x(0x%x) -> 0x%x \n", __func__,
		gyro_signed_diff_x, ois_stat->gyro[0]);
	CDBG("%s gyro_signed_diff_y(0x%x) -> 0x%x \n", __func__,
		gyro_signed_diff_y, ois_stat->gyro[1]);

	RegReadA(0x6058, &hall_x);
	RegReadA(0x6059, &hall_y);

	/* change to unsigned factor */	
	hall_signed_x = from_signed_byte(hall_x);
			
	/* change to unsigned factor */
	hall_signed_y = from_signed_byte(hall_y);

	/* 10 LSB, max = 10 * 262(up scale)  */
	ois_stat->hall[0] = (-1) * hall_signed_x * 262;
	ois_stat->hall[1] = (-1) * hall_signed_y * 262;
	
	CDBG("%s [0x6058]hall_x 0x%x->0x%x\n", __func__, hall_x, ois_stat->hall[0]);
	CDBG("%s [0x6059]hall_y 0x%x->0x%x\n", __func__, hall_y, ois_stat->hall[1]);
	
	ois_stat->is_stable = 1;

	/* 3 dsp */
	if ((abs(gyro_signed_diff_x) > (262 * 3)) ||
		(abs(gyro_signed_diff_y) > (262 * 3))) {
		CDBG("Gyro Offset Diff X is FAIL!!! %d 0x%x\n", gyro_signed_diff_x,
			gyro_signed_diff_x);
		CDBG("Gyro Offset Diff Y is FAIL!!! %d 0x%x\n", gyro_signed_diff_y,
			gyro_signed_diff_y);
		ois_stat->is_stable = 0;
	}
	
	/* hall_x, hall_y check -10 ~ 10, 10LSB */
	/* will be fuji adjust hall_x, hall_y */
	if ((abs(hall_signed_x) > 10) || (abs(hall_signed_y) > 10)) {
		CDBG("hall_signed_x FAIL!!! %d 0x%x\n", hall_signed_x, hall_signed_x);		
		CDBG("hall_signed_y FAIL!!! %d 0x%x\n", hall_signed_y, hall_signed_y);		
		ois_stat->is_stable = 0;
	}

	ois_stat->target[0] = 0; /* not supported */
	ois_stat->target[1] = 0; /* not supported */

	return 0;
}

int32_t fuji_ois_move_lens(int16_t target_x, int16_t target_y)
{
	int8_t hallx =  target_x / HALL_SCALE_FACTOR;
	int8_t hally =  target_y / HALL_SCALE_FACTOR;
	uint8_t result = 0;

	/* check ois mode & change to suitable mode */
	RegReadA(0x6020, &result);
	if (result != 0x01) {
		RegWriteA(0x6020, 0x01);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
	}

	printk("%s target : %d(0x%x), %d(0x%x)\n", __func__,
		hallx, hallx, hally, hally);

	/* hallx range -> D2 to 2E (-46, 46) */
	RegWriteA(0x6099, 0xFF & hallx); /* target x position input */
	RegWriteA(0x609A, 0xFF & hally); /* target y position input */
	/* wait 100ms */
	usleep(100000);
	RegWriteA(0x6098, 0x01); /* order to move. */
		
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING * 2))
		return OIS_INIT_TIMEOUT;

	RegReadA(0x609B, &result);

	if (result == 0x03)
		return  OIS_SUCCESS;

	printk("%s move fail : 0x%x \n", __func__, result);
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
	msm_ois_t->ois_func_tbl = &fuji_ois_func_tbl;
}


