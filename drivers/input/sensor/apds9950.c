/*
*  apds9950.c - Linux kernel modules for ambient light + color(RGB) + proximity sensor
*
*  Copyright (C) 2012 Lee Kai Koon <kai-koon.lee@avagotech.com>
*  Copyright (C) 2012 Avago Technologies
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c/apds9950.h>
#ifdef CONFIG_OF
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#endif

#define APDS9950_DRV_NAME	"apds9950"
#define DRIVER_VERSION		"1.0.0"

#define ABS_LIGHT	0x29	/* added to support LIGHT - light sensor */
#define ABS_CCT		0x2A	/* newly added to support CCT - RGB */

#define APDS9950_HAL_USE_SYS_ENABLE

/*#define APDS9950_INT		IRQ_EINT(8) */

#define APDS9950_PS_DETECTION_THRESHOLD		600
#define APDS9950_PS_HSYTERESIS_THRESHOLD	500
#define APDS9950_PS_PULSE_NUMBER		8

#define APDS9950_ALS_THRESHOLD_HSYTERESIS	20	/* 20 = 20% */

#define APDS9950_LUX_GA1	137  /* no cover glass - for Fluorescent Daylight 6500K */
#define APDS9950_LUX_GA2	54   /* no cover glass - for Incandescent 2600K */
#define APDS9950_LUX_GA3	138  /* no cover glass - for Fluorescent Warm White 2700K */
#define APDS9950_CCT_GA1	909  /* no cover glass - for Fluorescent Daylight 6500K */
#define APDS9950_CCT_GA2	381  /* no cover glass - for Incandescent 2600K */
#define APDS9950_CCT_GA3	411  /* no cover glass - for Fluorescent Warm White 2700K */


/* Change History
*
* 1.0.0	Funcamental Functions of APDS-9950
*
*/

#define APDS9950_IOCTL_PS_ENABLE		1
#define APDS9950_IOCTL_PS_GET_ENABLE		2
#define APDS9950_IOCTL_PS_GET_PDATA		3	/* pdata */
#define APDS9950_IOCTL_PS_POLL_DELAY		4
#define APDS9950_IOCTL_ALS_ENABLE		5
#define APDS9950_IOCTL_ALS_GET_ENABLE		6
#define APDS9950_IOCTL_ALS_GET_CDATA		7	/* cdata */
#define APDS9950_IOCTL_ALS_GET_RDATA		8	/* rdata */
#define APDS9950_IOCTL_ALS_GET_GDATA		9	/* gdata */
#define APDS9950_IOCTL_ALS_GET_BDATA		10	/* bdata */
#define APDS9950_IOCTL_ALS_POLL_DELAY		11

#define APDS9950_DISABLE_PS			0
#define APDS9950_ENABLE_PS			1

#define APDS9950_DISABLE_ALS			0
#define APDS9950_ENABLE_ALS_WITH_INT		1
#define APDS9950_ENABLE_ALS_NO_INT		2

#define APDS9950_ALS_POLL_SLOW			0	/* 1 Hz (1s) */
#define APDS9950_ALS_POLL_MEDIUM		1	/* 10 Hz (100ms) */
#define APDS9950_ALS_POLL_FAST			2	/* 20 Hz (50ms) */

/*
* Defines
*/

#define APDS9950_ENABLE_REG	0x00
#define APDS9950_ATIME_REG	0x01
#define APDS9950_PTIME_REG	0x02
#define APDS9950_WTIME_REG	0x03
#define APDS9950_AILTL_REG	0x04
#define APDS9950_AILTH_REG	0x05
#define APDS9950_AIHTL_REG	0x06
#define APDS9950_AIHTH_REG	0x07
#define APDS9950_PILTL_REG	0x08
#define APDS9950_PILTH_REG	0x09
#define APDS9950_PIHTL_REG	0x0A
#define APDS9950_PIHTH_REG	0x0B
#define APDS9950_PERS_REG	0x0C
#define APDS9950_CONFIG_REG	0x0D
#define APDS9950_PPCOUNT_REG	0x0E
#define APDS9950_CONTROL_REG	0x0F
#define APDS9950_REV_REG	0x11
#define APDS9950_ID_REG		0x12
#define APDS9950_STATUS_REG	0x13
#define APDS9950_CDATAL_REG	0x14
#define APDS9950_CDATAH_REG	0x15
#define APDS9950_RDATAL_REG	0x16
#define APDS9950_RDATAH_REG	0x17
#define APDS9950_GDATAL_REG	0x18
#define APDS9950_GDATAH_REG	0x19
#define APDS9950_BDATAL_REG	0x1A
#define APDS9950_BDATAH_REG	0x1B
#define APDS9950_PDATAL_REG	0x1C
#define APDS9950_PDATAH_REG	0x1D

#define CMD_BYTE		0x80
#define CMD_WORD		0xA0
#define CMD_SPECIAL		0xE0

#define CMD_CLR_PS_INT		0xE5
#define CMD_CLR_ALS_INT		0xE6
#define CMD_CLR_PS_ALS_INT	0xE7


/* Register Value define : ATIME */
#define APDS9950_100MS_ADC_TIME	0xD6  /* 100.8ms integration time */
#define APDS9950_50MS_ADC_TIME	0xEB  /* 50.4ms integration time */
#define APDS9950_27MS_ADC_TIME	0xF6  /* 24ms integration time */

/* Register Value define : PERS */
#define APDS9950_PPERS_0	0x00  /* Every proximity ADC cycle */
#define APDS9950_PPERS_1	0x10  /* 1 consecutive proximity value out of range */
#define APDS9950_PPERS_2	0x20  /* 2 consecutive proximity value out of range */
#define APDS9950_PPERS_3	0x30  /* 3 consecutive proximity value out of range */
#define APDS9950_PPERS_4	0x40  /* 4 consecutive proximity value out of range */
#define APDS9950_PPERS_5	0x50  /* 5 consecutive proximity value out of range */
#define APDS9950_PPERS_6	0x60  /* 6 consecutive proximity value out of range */
#define APDS9950_PPERS_7	0x70  /* 7 consecutive proximity value out of range */
#define APDS9950_PPERS_8	0x80  /* 8 consecutive proximity value out of range */
#define APDS9950_PPERS_9	0x90  /* 9 consecutive proximity value out of range */
#define APDS9950_PPERS_10	0xA0  /* 10 consecutive proximity value out of range */
#define APDS9950_PPERS_11	0xB0  /* 11 consecutive proximity value out of range */
#define APDS9950_PPERS_12	0xC0  /* 12 consecutive proximity value out of range */
#define APDS9950_PPERS_13	0xD0  /* 13 consecutive proximity value out of range */
#define APDS9950_PPERS_14	0xE0  /* 14 consecutive proximity value out of range */
#define APDS9950_PPERS_15	0xF0  /* 15 consecutive proximity value out of range */

#define APDS9950_APERS_0	0x00  /* Every ADC cycle */
#define APDS9950_APERS_1	0x01  /* 1 consecutive proximity value out of range */
#define APDS9950_APERS_2	0x02  /* 2 consecutive proximity value out of range */
#define APDS9950_APERS_3	0x03  /* 3 consecutive proximity value out of range */
#define APDS9950_APERS_5	0x04  /* 5 consecutive proximity value out of range */
#define APDS9950_APERS_10	0x05  /* 10 consecutive proximity value out of range */
#define APDS9950_APERS_15	0x06  /* 15 consecutive proximity value out of range */
#define APDS9950_APERS_20	0x07  /* 20 consecutive proximity value out of range */
#define APDS9950_APERS_25	0x08  /* 25 consecutive proximity value out of range */
#define APDS9950_APERS_30	0x09  /* 30 consecutive proximity value out of range */
#define APDS9950_APERS_35	0x0A  /* 35 consecutive proximity value out of range */
#define APDS9950_APERS_40	0x0B  /* 40 consecutive proximity value out of range */
#define APDS9950_APERS_45	0x0C  /* 45 consecutive proximity value out of range */
#define APDS9950_APERS_50	0x0D  /* 50 consecutive proximity value out of range */
#define APDS9950_APERS_55	0x0E  /* 55 consecutive proximity value out of range */
#define APDS9950_APERS_60	0x0F  /* 60 consecutive proximity value out of range */

/* Register Value define : CONTROL */
#define APDS9950_AGAIN_1X	0x00  /* 1X ALS GAIN */
#define APDS9950_AGAIN_4X	0x01  /* 4X ALS GAIN */
#define APDS9950_AGAIN_16X	0x02  /* 16X ALS GAIN */
#define APDS9950_AGAIN_64X	0x03  /* 64X ALS GAIN */

#define APDS9950_PRX_IR_DIOD	0x20  /* Proximity uses CH1 diode */

#define APDS9950_PGAIN_1X	0x00  /* PS GAIN 1X */
#define APDS9950_PGAIN_2X	0x04  /* PS GAIN 2X */
#define APDS9950_PGAIN_4X	0x08  /* PS GAIN 4X */
#define APDS9950_PGAIN_8X	0x0C  /* PS GAIN 8X */

#define APDS9950_PDRVIE_100MA	0x00  /* PS 100mA LED drive */
#define APDS9950_PDRVIE_50MA	0x40  /* PS 50mA LED drive */
#define APDS9950_PDRVIE_25MA	0x80  /* PS 25mA LED drive */
#define APDS9950_PDRVIE_12_5MA	0xC0  /* PS 12.5mA LED drive */


typedef enum {
    APDS9950_ALS_RES_24MS  = 0,    /* 24ms integration time */
    APDS9950_ALS_RES_50MS  = 1,    /* 50ms integration time */
    APDS9950_ALS_RES_100MS = 2	   /* 100ms integration time */
} apds9950_als_res_e;

typedef enum {
    APDS9950_ALS_GAIN_1X    = 0,    /* 1x AGAIN */
    APDS9950_ALS_GAIN_4X    = 1,    /* 4x AGAIN */
    APDS9950_ALS_GAIN_16X   = 2,    /* 16x AGAIN */
    APDS9950_ALS_GAIN_64X   = 3     /* 64x AGAIN */
} apds9950_als_gain_e;

/*
* Structs
*/

struct apds9950_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct delayed_work	dwork;		/* for PS interrupt */
	struct delayed_work	als_dwork;	/* for ALS polling */
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;

	struct apds9950_platform_data *platform_data;


	int irq;

	unsigned int enable;
	unsigned int atime;
	unsigned int ptime;
	unsigned int wtime;
	unsigned int ailt;
	unsigned int aiht;
	unsigned int pilt;
	unsigned int piht;
	unsigned int pers;
	unsigned int config;
	unsigned int ppcount;
	unsigned int control;
	unsigned int cdata;
	unsigned int rdata;
	unsigned int gdata;
	unsigned int bdata;
	unsigned int pdata;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;
	unsigned int enable_als_sensor;

	/* PS parameters */
	unsigned int ps_threshold;
	unsigned int ps_hysteresis_threshold;	/* always lower than ps_threshold */
	unsigned int ps_detection;		/* 0 = near-to-far; 1 = far-to-near */
	unsigned int ps_data;			/* to store PS data */

	/* ALS parameters */
	unsigned int als_threshold_l;	/* low threshold */
	unsigned int als_threshold_h;	/* high threshold */
	unsigned int als_data;		/* to store ALS data */
	int als_prev_lux;		/* to store previous lux value */

	unsigned int als_gain;		/* needed for Lux calculation */
	unsigned int als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int als_atime_index;	/* storage for als integratiion time */
	unsigned int als_again_index;	/* storage for als GAIN */

	unsigned int cct;		/* color temperature */
	unsigned int lux;		/* lux */

};

static struct apds9950_data *pdev_data;

/*
* Global data
*/
static struct i2c_client *apds9950_i2c_client; /* global i2c_client to support ioctl */
static struct workqueue_struct *apds9950_workqueue;

static unsigned char apds9950_als_atime_tb[] = { 0xF6, 0xEB, 0xD6 };
static unsigned short apds9950_als_integration_tb[] = {2400, 5040, 10080}; /* DO NOT use beyond 100.8ms */
static unsigned short apds9950_als_res_tb[] = { 10240, 21504, 43008 };
static unsigned char apds9950_als_again_tb[] = { 1, 4, 16, 64 };
static unsigned char apds9950_als_again_bit_tb[] = { 0x00, 0x01, 0x02, 0x03 };

#ifndef CONFIG_SENSOR_APDS9950

/* ensure the coefficients do not exceed 9999 */
static int RGB_COE_X[3] = {91, -352, -1560}; /* {0.090599, 6.026495, -4.93002}; */
static int RGB_COE_Y[3] = {6026, 6300, 3697}; /* {-0.35177, 6.30048, -4.54057}; */
static int RGB_COE_Z[3] = { -4930, -4541, -113}; /*{-1.55953, 3.69696, -0.11315}; */

static int RGB_CIE_N1 = 332; /* 0.332; */
static int RGB_CIE_N2 = 186; /* 0.1858; */

static int RGB_CIE_CCT1 = 449; /* 449.0; */
static int RGB_CIE_CCT2 = 3525; /* 3525.0; */
static int RGB_CIE_CCT3 = 6823; /* 6823.3; */
static int RGB_CIE_CCT4 = 5520; /* 5520.33; */

#endif

/*
* Management functions
*/

static int apds9950_set_command(struct i2c_client *client, int command)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;
	int clearInt;

	if (command == 0)
		clearInt = CMD_CLR_PS_INT;
	else if (command == 1)
		clearInt = CMD_CLR_ALS_INT;
	else
		clearInt = CMD_CLR_PS_ALS_INT;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte(client, clearInt);
	mutex_unlock(&data->update_lock);

	return ret;
}

static int apds9950_set_enable(struct i2c_client *client, int enable)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_ENABLE_REG, enable);
	mutex_unlock(&data->update_lock);

	data->enable = enable;

	return ret;
}

static int apds9950_set_atime(struct i2c_client *client, int atime)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_ATIME_REG, atime);
	mutex_unlock(&data->update_lock);

	data->atime = atime;

	return ret;
}

static int apds9950_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_PTIME_REG, ptime);
	mutex_unlock(&data->update_lock);

	data->ptime = ptime;

	return ret;
}

static int apds9950_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_WTIME_REG, wtime);
	mutex_unlock(&data->update_lock);

	data->wtime = wtime;

	return ret;
}

static int apds9950_set_ailt(struct i2c_client *client, int threshold)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_AILTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->ailt = threshold;

	return ret;
}

static int apds9950_set_aiht(struct i2c_client *client, int threshold)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_AIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->aiht = threshold;

	return ret;
}

static int apds9950_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PILTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->pilt = threshold;

	return ret;
}

static int apds9950_set_piht(struct i2c_client *client, int threshold)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->piht = threshold;

	return ret;
}

static int apds9950_set_pers(struct i2c_client *client, int pers)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_PERS_REG, pers);
	mutex_unlock(&data->update_lock);

	data->pers = pers;

	return ret;
}

static int apds9950_set_config(struct i2c_client *client, int config)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_CONFIG_REG, config);
	mutex_unlock(&data->update_lock);

	data->config = config;

	return ret;
}

static int apds9950_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_PPCOUNT_REG, ppcount);
	mutex_unlock(&data->update_lock);

	data->ppcount = ppcount;

	return ret;
}

static int apds9950_set_control(struct i2c_client *client, int control)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_CONTROL_REG, control);
	mutex_unlock(&data->update_lock);

	data->control = control;

	return ret;
}

static int LuxCalculation(struct i2c_client *client)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
#ifdef CONFIG_SENSOR_APDS9950
	struct apds9950_platform_data *platform_data = client->dev.platform_data;
#endif
	int X1, Y1, Z1;
	int x1, y1, z1;
	int n;
	int lux;
	int cct;
	int sum_XYZ = 0;


#ifdef CONFIG_SENSOR_APDS9950
	X1 = (platform_data->RGB_COE_X[0] * data->rdata) + (platform_data->RGB_COE_X[1] * data->gdata) + (platform_data->RGB_COE_X[2] * data->bdata);
	Y1 = (platform_data->RGB_COE_Y[0] * data->rdata) + (platform_data->RGB_COE_Y[1] * data->gdata) + (platform_data->RGB_COE_Y[2] * data->bdata);
	Z1 = (platform_data->RGB_COE_Z[0] * data->rdata) + (platform_data->RGB_COE_Z[1] * data->gdata) + (platform_data->RGB_COE_Z[2] * data->bdata);
#else
	X1 = (RGB_COE_X[0] * data->rdata) + (RGB_COE_X[1] * data->gdata) + (RGB_COE_X[2] * data->bdata);
	Y1 = (RGB_COE_Y[0] * data->rdata) + (RGB_COE_Y[1] * data->gdata) + (RGB_COE_Y[2] * data->bdata);
	Z1 = (RGB_COE_Z[0] * data->rdata) + (RGB_COE_Z[1] * data->gdata) + (RGB_COE_Z[2] * data->bdata);
#endif

	if ((X1 == 0) && (Y1 == 0) && (Z1 == 0)) {
		x1 = y1 = z1 = 0;
	} else {
		sum_XYZ = (X1 + Y1 + Z1) / 1000;	/* scale down */
		if (sum_XYZ > 0) {
			if (((X1 + Y1 + Z1) % 1000) >= 500)
				sum_XYZ++;
		} else {
			if (((X1 + Y1 + Z1) % 1000) <= -500)
				sum_XYZ--;
		}

		x1 = X1 / sum_XYZ;
		y1 = Y1 / sum_XYZ;
		z1 = Z1 / sum_XYZ;
	}

#ifdef CONFIG_SENSOR_APDS9950
	if (data->cdata > 10) {
		n = (((x1 - platform_data->RGB_CIE_N1) * 1000) / (platform_data->RGB_CIE_N2 - y1));
		cct = (((platform_data->RGB_CIE_CCT1 * (n * n * n)) / 1000000000) + ((platform_data->RGB_CIE_CCT2 * (n * n)) / 1000000) + ((platform_data->RGB_CIE_CCT3 * n) / 1000) + platform_data->RGB_CIE_CCT4);
	} else {
		n = 0;
		cct = 0;
	}
#else
	if (data->cdata > 10) {

		n = ((x1 - RGB_CIE_N1) * 1000) / (RGB_CIE_N2 - y1);
		cct = (((RGB_CIE_CCT1 * (n * n * n)) / 1000000000) + ((RGB_CIE_CCT2 * (n * n)) / 1000000) + ((RGB_CIE_CCT3 * n) / 1000) + RGB_CIE_CCT4);
	} else {
		n = 0;
		cct = 0;
	}
#endif

	lux = (Y1 / (apds9950_als_integration_tb[data->als_atime_index] * apds9950_als_again_tb[data->als_again_index])) * 10080;
	data->lux = lux / 1000;

	data->cct = cct;

	/*if( (data->rdata/data->cdata)<0.6 ){ */

	if (data->cdata > 0) {
		if (((data->rdata * 100) / data->cdata) > 65) {	/* Incandescent 2600K */
			data->cct = (data->cct * APDS9950_CCT_GA2) / 1000;
			data->lux = (data->lux * APDS9950_LUX_GA2) / 1000;
		} else if (((data->rdata * 100) / data->cdata) > 45) { /* Fluorescent Warm White 2700K */
			data->cct = (data->cct * APDS9950_CCT_GA3) / 1000;
			data->lux = (data->lux * APDS9950_LUX_GA3) / 1000;
		} else { /* Fluorescent Daylight 6500K */
			data->cct = (data->cct * APDS9950_CCT_GA1) / 100;
			data->lux = (data->lux * APDS9950_LUX_GA1) / 100;
		}
	}


	printk("%s cct = %d, lux = %d, data->cct = %d, data->lux = %d\n", __func__, cct, lux, data->cct, data->lux);

	return 0;

}

static void apds9950_change_ps_threshold(struct i2c_client *client)
{
	struct apds9950_data *data = i2c_get_clientdata(client);

	data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_PDATAL_REG);

	if ((data->ps_data > data->pilt) && (data->ps_data >= data->piht)) {
		/* far-to-near detected */
		data->ps_detection = 1;

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* FAR-to-NEAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PILTL_REG, data->ps_hysteresis_threshold);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PIHTL_REG, 1023);

		data->pilt = data->ps_hysteresis_threshold;
		data->piht = 1023;

		printk("far-to-near detected\n");
	} else if ((data->ps_data <= data->pilt) && (data->ps_data < data->piht)) {
		/* near-to-far detected */
		data->ps_detection = 0;

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PIHTL_REG, data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		printk("near-to-far detected\n");
	}
}

static void apds9950_change_als_threshold(struct i2c_client *client)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	unsigned char change_again = 0;
	unsigned char control_data = 0;

	data->cdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_CDATAL_REG);
	data->rdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_RDATAL_REG);
	data->gdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_GDATAL_REG);
	data->bdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_BDATAL_REG);

	LuxCalculation(client);

	if (data->lux >= 0) {
		data->lux = data->lux < 30000 ? data->lux : 30000;
		data->als_prev_lux = data->lux;
	}

	if (data->cct >= 0) {
		data->cct = data->cct < 10000 ? data->cct : 10000;
	}

	printk("cct=%d, lux=%d cdata=%d rdata=%d gdata=%d bdata=%d pdata=%d again=%d\n",
		data->cct, data->lux, data->cdata, data->rdata, data->gdata,
		data->bdata, data->pdata, apds9950_als_again_tb[data->als_again_index]);

	/* check PS under sunlight */
	if ((data->ps_detection == 1) && (data->cdata > (75 * (1024 * (256 - apds9950_als_atime_tb[data->als_atime_index]))) / 100)) {	/* PS was previously in far-to-near condition */
		/* need to inform input event as there will be no interrupt from the PS */
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PIHTL_REG, data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */

		printk("apds_9950_proximity_handler = FAR\n");
	}

	data->als_data = data->cdata;

	data->als_threshold_l = (data->als_data * (100 - APDS9950_ALS_THRESHOLD_HSYTERESIS)) / 100;
	data->als_threshold_h = (data->als_data * (100 + APDS9950_ALS_THRESHOLD_HSYTERESIS)) / 100;

	if (data->als_threshold_h >= apds9950_als_res_tb[data->als_atime_index]) {
		data->als_threshold_h = apds9950_als_res_tb[data->als_atime_index];
	}

	if (data->als_data >= (apds9950_als_res_tb[data->als_atime_index] * 99) / 100) {
		/* lower AGAIN if possible */
		if (data->als_again_index != APDS9950_ALS_GAIN_1X) {
			data->als_again_index--;
			change_again = 1;
		} else {
			input_report_abs(data->input_dev_als, ABS_LIGHT, data->lux); /* report lux level */
			input_report_abs(data->input_dev_als, ABS_CCT, data->cct); /* report color temperature cct */
			input_sync(data->input_dev_als);
		}
	} else if (data->als_data <= (apds9950_als_res_tb[data->als_atime_index] * 1) / 100) {
		/* increase AGAIN if possible */
		if (data->als_again_index != APDS9950_ALS_GAIN_64X) {
			data->als_again_index++;
			change_again = 1;
		} else {
			input_report_abs(data->input_dev_als, ABS_LIGHT, data->lux); /* report lux level */
			input_report_abs(data->input_dev_als, ABS_CCT, data->cct); /* report color temperature cct */
			input_sync(data->input_dev_als);
		}
	} else {
		input_report_abs(data->input_dev_als, ABS_LIGHT, data->lux); /* report lux level */
		input_report_abs(data->input_dev_als, ABS_CCT, data->cct); /* report color temperature cct */
		input_sync(data->input_dev_als);
	}

	if (change_again) {
		control_data = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9950_CONTROL_REG);
		control_data = control_data & 0xFC;
		control_data = control_data | apds9950_als_again_bit_tb[data->als_again_index];
		i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_CONTROL_REG, control_data);
	}

	i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_AILTL_REG, data->als_threshold_l);
	i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_AIHTL_REG, data->als_threshold_h);

}



static void apds9950_reschedule_work(struct apds9950_data *data,
				     unsigned long delay)
{
	/*
	* If work is already scheduled then subsequent schedules will not
	* change the scheduled time that's why we have to cancel it first.
	*/
	__cancel_delayed_work(&data->dwork);
	queue_delayed_work(apds9950_workqueue, &data->dwork, delay);
}


/* ALS polling routine */
static void apds9950_als_polling_work_handler(struct work_struct *work)
{
	struct apds9950_data *data = container_of(work, struct apds9950_data, als_dwork.work);
	struct i2c_client *client = data->client;
	unsigned char change_again = 0;
	unsigned char control_data = 0;

	if (data->enable_als_sensor != APDS9950_ENABLE_ALS_NO_INT)
		return;

	data->cdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_CDATAL_REG);
	data->rdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_RDATAL_REG);
	data->gdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_GDATAL_REG);
	data->bdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_BDATAL_REG);
	data->pdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_PDATAL_REG);

	LuxCalculation(client);

	if (data->lux >= 0) {
		data->lux = data->lux < 30000 ? data->lux : 30000;
		data->als_prev_lux = data->lux;
	}

	if (data->cct >= 0) {
		data->cct = data->cct < 10000 ? data->cct : 10000;
	}

	printk("cct=%d, lux=%d cdata=%d rdata=%d gdata=%d bdata=%d pdata=%d again=%d\n",
	       data->cct, data->lux, data->cdata, data->rdata, data->gdata,
	       data->bdata, data->pdata, apds9950_als_again_tb[data->als_again_index]);

	/* check PS under sunlight */
	if ((data->ps_detection == 1) && (data->cdata > (apds9950_als_res_tb[data->als_atime_index] * 95) / 100)) {	/* PS was previously in far-to-near condition */
		/* need to inform input event as there will be no interrupt from the PS */
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9950_PIHTL_REG, data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */

		printk("apds_9950_proximity_handler = FAR\n");
	}

	data->als_data = data->cdata;

	if (data->als_data >= (apds9950_als_res_tb[data->als_atime_index] * 99) / 100) {
		/* lower AGAIN if possible */
		if (data->als_again_index != APDS9950_ALS_GAIN_1X) {
			data->als_again_index--;
			change_again = 1;
		}
	} else if (data->als_data <= (apds9950_als_res_tb[data->als_atime_index] * 1) / 100) {
		/* increase AGAIN if possible */
		if (data->als_again_index != APDS9950_ALS_GAIN_64X) {
			data->als_again_index++;
			change_again = 1;
		}
	} else {
		input_report_abs(data->input_dev_als, ABS_LIGHT, data->lux); /* report lux level */
		input_report_abs(data->input_dev_als, ABS_CCT, data->cct); /* report color temperature cct */
		input_sync(data->input_dev_als);
	}


	if (change_again) {
		control_data = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9950_CONTROL_REG);
		control_data = control_data & 0xFC;
		control_data = control_data | apds9950_als_again_bit_tb[data->als_again_index];
		i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_CONTROL_REG, control_data);
	}

	queue_delayed_work(apds9950_workqueue, &data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	/* restart timer */
}

/* PS interrupt routine */
static void apds9950_work_handler(struct work_struct *work)
{
	struct apds9950_data *data = container_of(work, struct apds9950_data, dwork.work);
	struct i2c_client *client = data->client;
	int status;
	int cdata;
	int enable;

	status = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9950_STATUS_REG);
	enable = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9950_ENABLE_REG);

	i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_ENABLE_REG, 1);	/* disable 9950's ADC first */

	printk("status = %x\n", status);

	if ((status & enable & 0x30) == 0x30) {
		/* both PS and ALS are interrupted */
		apds9950_change_als_threshold(client);

		cdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_CDATAL_REG);
		if (cdata < (75 * (apds9950_als_res_tb[data->als_atime_index])) / 100)
			apds9950_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1) {
				apds9950_change_ps_threshold(client);
			} else {
				printk("Triggered by background ambient noise\n");
			}
		}

		apds9950_set_command(client, 2);	/* 2 = CMD_CLR_PS_ALS_INT */
	} else if ((status & enable & 0x20) == 0x20) {
		/* only PS is interrupted */

		/* check if this is triggered by background ambient noise */
		cdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_CDATAL_REG);
		if (cdata < (75 * (apds9950_als_res_tb[data->als_atime_index])) / 100)
			apds9950_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1) {
				apds9950_change_ps_threshold(client);
			} else {
				printk("Triggered by background ambient noise\n");
			}
		}

		apds9950_set_command(client, 0);	/* 0 = CMD_CLR_PS_INT */
	} else if ((status & enable & 0x10) == 0x10) {
		/* only ALS is interrupted */
		apds9950_change_als_threshold(client);

		apds9950_set_command(client, 1);	/* 1 = CMD_CLR_ALS_INT */
	}

	i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9950_ENABLE_REG, data->enable);
}

/* assume this is ISR */
static irqreturn_t apds9950_interrupt(int vec, void *info)
{
	struct i2c_client *client = (struct i2c_client *)info;
	struct apds9950_data *data = i2c_get_clientdata(client);

	printk("==> apds9950_interrupt\n");
	apds9950_reschedule_work(data, 0);

	return IRQ_HANDLED;
}

/*
* IOCTL support
*/


static int apds9950_enable_als_sensor(struct i2c_client *client, int val)
{
	struct apds9950_data *data = i2c_get_clientdata(client);

	printk("%s: enable als sensor (%d)\n", __func__, val);

	if ((val != APDS9950_DISABLE_ALS) && (val != APDS9950_ENABLE_ALS_WITH_INT) && (val != APDS9950_ENABLE_ALS_NO_INT)) {
		printk("%s: enable als sensor=%d\n", __func__, val);
		return -1;
	}

	if ((val == APDS9950_ENABLE_ALS_WITH_INT) || (val == APDS9950_ENABLE_ALS_NO_INT)) {
		/* turn on light  sensor */
		if (data->enable_als_sensor == 0) {

			data->enable_als_sensor = val;

			apds9950_set_enable(client, 0); /* Power Off */

			if (data->enable_als_sensor == APDS9950_ENABLE_ALS_NO_INT) {
				if (data->enable_ps_sensor) {
					apds9950_set_enable(client, 0x2F);	 /* Enable PS with interrupt */
				} else {
					apds9950_set_enable(client, 0x0B);	 /* no interrupt*/
				}


				/*
				* If work is already scheduled then subsequent schedules will not
				* change the scheduled time that's why we have to cancel it first.
				*/
				__cancel_delayed_work(&data->als_dwork);
				flush_delayed_work(&data->als_dwork);
				queue_delayed_work(apds9950_workqueue, &data->als_dwork, msecs_to_jiffies(data->als_poll_delay));

			} else {	/* als with int */
				apds9950_set_ailt(client, 0xFFFF);	/* force first ALS interrupt in order to get environment reading */
				apds9950_set_aiht(client, 0);

				if (data->enable_ps_sensor) {
					apds9950_set_enable(client, 0x3F);	 /* Enable both ALS and PS with interrupt */
				} else {
					apds9950_set_enable(client, 0x1B);	 /* only enable light sensor with interrupt*/
				}


				/*
				* If work is already scheduled then subsequent schedules will not
				* change the scheduled time that's why we have to cancel it first.
				*/
				__cancel_delayed_work(&data->als_dwork);
				flush_delayed_work(&data->als_dwork);
			}
		}
	} else {
		/*turn off light sensor */
		/* what if the p sensor is active? */
		data->enable_als_sensor = APDS9950_DISABLE_ALS;

		if (data->enable_ps_sensor) {
			apds9950_set_enable(client, 0); /* Power Off */

			apds9950_set_piht(client, 0);
			apds9950_set_piht(client, APDS9950_PS_DETECTION_THRESHOLD);

			apds9950_set_enable(client, 0x2D);	 /* only enable prox sensor with interrupt */
		} else {
			apds9950_set_enable(client, 0);
		}

		/*
		* If work is already scheduled then subsequent schedules will not
		* change the scheduled time that's why we have to cancel it first.
		*/
		__cancel_delayed_work(&data->als_dwork);
		flush_delayed_work(&data->als_dwork);
	}

	return 0;
}

static int apds9950_set_als_poll_delay(struct i2c_client *client, unsigned int val)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	int ret;
	int atime_index = 0;

	printk("%s : %d\n", __func__, val);

	if ((val != APDS9950_ALS_POLL_SLOW) && (val != APDS9950_ALS_POLL_MEDIUM) && (val != APDS9950_ALS_POLL_FAST)) {
		printk("%s:invalid value=%d\n", __func__, val);
		return -1;
	}

	if (val == APDS9950_ALS_POLL_FAST) {
		data->als_poll_delay = 50;		/* 50ms */
		atime_index = APDS9950_ALS_RES_24MS;
	} else if (val == APDS9950_ALS_POLL_MEDIUM) {
		data->als_poll_delay = 100;		/* 100ms */
		atime_index = APDS9950_ALS_RES_50MS;
	} else {	/* APDS9950_ALS_POLL_SLOW */
		data->als_poll_delay = 1000;		/* 100ms */
		atime_index = APDS9950_ALS_RES_100MS;
	}

	ret = apds9950_set_atime(client, apds9950_als_atime_tb[atime_index]);
	if (ret >= 0) {
		data->als_atime_index = atime_index;
		printk("poll delay %d, atime_index %d\n", data->als_poll_delay, data->als_atime_index);
	} else
		return -1;


	/*
	* If work is already scheduled then subsequent schedules will not
	* change the scheduled time that's why we have to cancel it first.
	*/
	__cancel_delayed_work(&data->als_dwork);
	flush_delayed_work(&data->als_dwork);
	queue_delayed_work(apds9950_workqueue, &data->als_dwork, msecs_to_jiffies(data->als_poll_delay));

	return 0;
}

static int apds9950_enable_ps_sensor(struct i2c_client *client, int val)
{
	struct apds9950_data *data = i2c_get_clientdata(client);

	printk("enable ps senosr (%d)\n", val);

	if ((val != APDS9950_DISABLE_PS) && (val != APDS9950_ENABLE_PS)) {
		printk("%s:invalid value=%d\n", __func__, val);
		return -1;
	}

	/*	apds9950_init_client(client); */

	if (val == APDS9950_ENABLE_PS) {
		/*turn on p sensor */
		if (data->enable_ps_sensor == 0) {

			data->enable_ps_sensor = APDS9950_ENABLE_PS;

			apds9950_set_enable(client, 0); /* Power Off */

			apds9950_set_pilt(client, 0);		/* init threshold for proximity */
			apds9950_set_piht(client, APDS9950_PS_DETECTION_THRESHOLD);

			if (data->enable_als_sensor == APDS9950_DISABLE_ALS) {
				apds9950_set_enable(client, 0x2D);	 /* only enable PS interrupt */
			} else if (data->enable_als_sensor == APDS9950_ENABLE_ALS_WITH_INT) {
				apds9950_set_enable(client, 0x3F);	 /* enable ALS and PS interrupt */
			} else { /* APDS9950_ENABLE_ALS_NO_INT */
				apds9950_set_enable(client, 0x2F);	 /* enable PS interrupt only */
			}
		}
	} else {
		/*turn off p sensor - kk 25 Apr 2011 we can't turn off the entire sensor, the light sensor may be needed by HAL */
		data->enable_ps_sensor = APDS9950_DISABLE_PS;
		if (data->enable_als_sensor == APDS9950_ENABLE_ALS_NO_INT) {
			apds9950_set_enable(client, 0x0B);	 /* no ALS interrupt */


			/*
			* If work is already scheduled then subsequent schedules will not
			* change the scheduled time that's why we have to cancel it first.
			*/
			__cancel_delayed_work(&data->als_dwork);
			flush_delayed_work(&data->als_dwork);
			queue_delayed_work(apds9950_workqueue, &data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	/* 100ms */

		} else if (data->enable_als_sensor == APDS9950_ENABLE_ALS_WITH_INT) {
			/* reconfigute light sensor setting */
			apds9950_set_enable(client, 0); /* Power Off */
			apds9950_set_ailt(client, 0xFFFF);	/* Force ALS interrupt */
			apds9950_set_aiht(client, 0);

			apds9950_set_enable(client, 0x13);	 /* enable ALS interrupt */
		} else {	/* APDS9950_DISABLE_ALS */
			apds9950_set_enable(client, 0);


			/*
			* If work is already scheduled then subsequent schedules will not
			* change the scheduled time that's why we have to cancel it first.
			*/
			__cancel_delayed_work(&data->als_dwork);
			flush_delayed_work(&data->als_dwork);
		}
	}

	return 0;
}

static int apds9950_ps_open(struct inode *inode, struct file *file)
{
	/* printk("apds9950_ps_open\n"); */
	return 0;
}

static int apds9950_ps_release(struct inode *inode, struct file *file)
{
	/* printk("apds9950_ps_release\n"); */
	return 0;
}

static long apds9950_ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct apds9950_data *data;
	struct i2c_client *client;
	int enable;
	int ret = -1;

	if (arg == 0)
		return -1;

	if (apds9950_i2c_client == NULL) {
		printk("apds9950_ps_ioctl error: i2c driver not installed\n");
		return -EFAULT;
	}

	client = apds9950_i2c_client;
	data = i2c_get_clientdata(apds9950_i2c_client);

	switch (cmd) {
	case APDS9950_IOCTL_PS_ENABLE:
		if (copy_from_user(&enable, (void __user *)arg, sizeof(enable))) {
			printk("apds9950_ps_ioctl: copy_from_user failed\n");
			return -EFAULT;
		}

		ret = apds9950_enable_ps_sensor(client, enable);
		if (ret < 0) {
			return ret;
		}
		break;

	case APDS9950_IOCTL_PS_GET_ENABLE:
		if (copy_to_user((void __user *)arg, &data->enable_ps_sensor, sizeof(data->enable_ps_sensor))) {
			printk("apds9950_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}

		break;

	case APDS9950_IOCTL_PS_GET_PDATA:
		data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_PDATAL_REG);

		if (copy_to_user((void __user *)arg, &data->ps_data, sizeof(data->ps_data))) {
			printk("apds9950_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	default:
		break;
	}


	return 0;
}

static int apds9950_als_open(struct inode *inode, struct file *file)
{
	/* printk("apds9950_als_open\n"); */
	return 0;
}

static int apds9950_als_release(struct inode *inode, struct file *file)
{
	/* printk("apds9950_als_release\n"); */
	return 0;
}

static long apds9950_als_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct apds9950_data *data;
	struct i2c_client *client;
	int enable;
	int ret = -1;
	unsigned int delay;

	if (arg == 0)
		return -1;

	if (apds9950_i2c_client == NULL) {
		printk("apds9950_als_ioctl error: i2c driver not installed\n");
		return -EFAULT;
	}

	client = apds9950_i2c_client;
	data = i2c_get_clientdata(apds9950_i2c_client);

	switch (cmd) {
	case APDS9950_IOCTL_ALS_ENABLE:
		if (copy_from_user(&enable, (void __user *)arg, sizeof(enable))) {
			printk("apds9950_als_ioctl: copy_from_user failed\n");
			return -EFAULT;
		}

		ret = apds9950_enable_als_sensor(client, enable);
		if (ret < 0) {
			return ret;
		}
		break;

	case APDS9950_IOCTL_ALS_POLL_DELAY:
		if (data->enable_als_sensor == APDS9950_ENABLE_ALS_NO_INT) {
			if (copy_from_user(&delay, (void __user *)arg, sizeof(delay))) {
				printk("apds9950_als_ioctl: copy_to_user failed\n");
				return -EFAULT;
			}

			ret = apds9950_set_als_poll_delay (client, delay);
			if (ret < 0) {
				return ret;
			}
		} else {
			printk("apds9950_als_ioctl: als is not in polling mode!\n");
			return -EFAULT;
		}
		break;

	case APDS9950_IOCTL_ALS_GET_ENABLE:
		if (copy_to_user((void __user *)arg, &data->enable_als_sensor, sizeof(data->enable_als_sensor))) {
			printk("apds9950_als_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	case APDS9950_IOCTL_ALS_GET_CDATA:
		data->als_data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_CDATAL_REG);

		if (copy_to_user((void __user *)arg, &data->als_data, sizeof(data->als_data))) {
			printk("apds9950_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	case APDS9950_IOCTL_ALS_GET_RDATA:
		data->als_data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_RDATAL_REG);

		if (copy_to_user((void __user *)arg, &data->als_data, sizeof(data->als_data))) {
			printk("apds9950_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	case APDS9950_IOCTL_ALS_GET_GDATA:
		data->als_data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_GDATAL_REG);

		if (copy_to_user((void __user *)arg, &data->als_data, sizeof(data->als_data))) {
			printk("apds9950_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	case APDS9950_IOCTL_ALS_GET_BDATA:
		data->als_data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_BDATAL_REG);

		if (copy_to_user((void __user *)arg, &data->als_data, sizeof(data->als_data))) {
			printk("apds9950_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	default:
		break;
	}

	return 0;
}

/*
* SysFS support
*/

static ssize_t apds9950_show_cdata(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct apds9950_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	int cdata;

	mutex_lock(&data->update_lock);
	cdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_CDATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", cdata);
}

static DEVICE_ATTR(cdata, S_IRUGO,
		   apds9950_show_cdata, NULL);

static ssize_t apds9950_show_rdata(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9950_data *data = i2c_get_clientdata(client);
	int rdata;

	mutex_lock(&data->update_lock);
	rdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_RDATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", rdata);
}

static DEVICE_ATTR(rdata, S_IRUGO,
		   apds9950_show_rdata, NULL);

static ssize_t apds9950_show_gdata(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9950_data *data = i2c_get_clientdata(client);
	int gdata;

	mutex_lock(&data->update_lock);
	gdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_GDATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", gdata);
}

static DEVICE_ATTR(gdata, S_IRUGO,
		   apds9950_show_gdata, NULL);

static ssize_t apds9950_show_bdata(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9950_data *data = i2c_get_clientdata(client);
	int bdata;

	mutex_lock(&data->update_lock);
	bdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_BDATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", bdata);
}

static DEVICE_ATTR(bdata, S_IRUGO,
		   apds9950_show_bdata, NULL);

static ssize_t apds9950_show_pdata(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct apds9950_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	int pdata;

	mutex_lock(&data->update_lock);
	pdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9950_PDATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", pdata);
}

static DEVICE_ATTR(pdata, S_IRUGO,
		   apds9950_show_pdata, NULL);

static ssize_t apds9950_show_proximity_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct apds9950_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t apds9950_store_proximity_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct apds9950_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	unsigned long val = simple_strtoul(buf, NULL, 10);

	printk("%s: enable ps senosr ( %ld)\n", __func__, val);

	if ((val != APDS9950_DISABLE_PS) && (val != APDS9950_ENABLE_PS)) {
		printk("**%s:store invalid value=%ld\n", __func__, val);
		return count;
	}

	apds9950_enable_ps_sensor(client, val);

	return count;
}

static DEVICE_ATTR(proximity_enable, S_IWUGO | S_IRUGO,
		   apds9950_show_proximity_enable, apds9950_store_proximity_enable);

static ssize_t apds9950_show_light_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct apds9950_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enable_als_sensor);
}

static ssize_t apds9950_store_light_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct apds9950_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	unsigned long val = simple_strtoul(buf, NULL, 10);

	printk("%s: enable als sensor ( %ld)\n", __func__, val);

	if ((val != APDS9950_DISABLE_ALS) && (val != APDS9950_ENABLE_ALS_WITH_INT) && (val != APDS9950_ENABLE_ALS_NO_INT)) {
		printk("**%s: store invalid valeu=%ld\n", __func__, val);
		return count;
	}

	apds9950_enable_als_sensor(client, val);

	return count;
}

static DEVICE_ATTR(light_enable, S_IWUGO | S_IRUGO,
		   apds9950_show_light_enable, apds9950_store_light_enable);

static ssize_t apds9950_show_light_poll_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct apds9950_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->als_poll_delay * 1000);	/* return in micro-second */
}

static ssize_t apds9950_store_light_poll_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct apds9950_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	unsigned long val = simple_strtoul(buf, NULL, 10);

	if (data->enable_als_sensor == APDS9950_ENABLE_ALS_NO_INT) {
		apds9950_set_als_poll_delay(client, val);
	} else {

		return 0;
	}

	return count;
}

static DEVICE_ATTR(light_poll_delay, S_IWUSR | S_IRUGO,
		   apds9950_show_light_poll_delay, apds9950_store_light_poll_delay);

static struct attribute *apds9950_attributes[] = {
	&dev_attr_cdata.attr,
	&dev_attr_rdata.attr,
	&dev_attr_gdata.attr,
	&dev_attr_bdata.attr,
	&dev_attr_pdata.attr,
	&dev_attr_proximity_enable.attr,
	&dev_attr_light_enable.attr,
	&dev_attr_light_poll_delay.attr,
	NULL
};

static const struct attribute_group apds9950_attr_group = {
	.attrs = apds9950_attributes,
};

static struct file_operations apds9950_ps_fops = {
	.owner = THIS_MODULE,
	.open = apds9950_ps_open,
	.release = apds9950_ps_release,
	.unlocked_ioctl = apds9950_ps_ioctl,
};

static struct miscdevice apds9950_ps_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "apds9950_ps_dev",
	.fops = &apds9950_ps_fops,
};

static struct file_operations apds9950_als_fops = {
	.owner = THIS_MODULE,
	.open = apds9950_als_open,
	.release = apds9950_als_release,
	.unlocked_ioctl = apds9950_als_ioctl,
};

static struct miscdevice apds9950_als_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "apds9950_als_dev",
	.fops = &apds9950_als_fops,
};

/*
* Initialization function
*/

static int apds9950_init_client(struct i2c_client *client)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
#ifdef CONFIG_SENSOR_APDS9950
	struct apds9950_platform_data *platform_data = client->dev.platform_data;
#endif
	int err;
	int id;

	err = apds9950_set_enable(client, 0);

	if (err < 0)
		return err;

	id = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9950_ID_REG);
	if (id == 0x69) {
		printk("APDS-9950\n");
	} else {
		printk("Not APDS-9950\n");
		return -EIO;
	}

	err = apds9950_set_atime(client, apds9950_als_atime_tb[data->als_atime_index]);	/* 100.64ms ALS integration time */
	if (err < 0)
		return err;

	err = apds9950_set_ptime(client, 0xFF);	/* 2.4ms Prox integration time */
	if (err < 0)
		return err;

	err = apds9950_set_wtime(client, 0xF6);	/* 24ms Wait time */
	if (err < 0)
		return err;

#ifdef CONFIG_SENSOR_APDS9950
	err = apds9950_set_ppcount(client, platform_data->ppcount);
#else
	err = apds9950_set_ppcount(client, APDS9950_PS_PULSE_NUMBER);
#endif
	if (err < 0)
		return err;

	err = apds9950_set_config(client, 0);		/* no long wait */
	if (err < 0)
		return err;

	err = apds9950_set_control(client, APDS9950_PDRVIE_100MA | APDS9950_PRX_IR_DIOD | APDS9950_PGAIN_1X | apds9950_als_again_bit_tb[data->als_again_index]);
	if (err < 0)
		return err;

	err = apds9950_set_pilt(client, 0);		/* init threshold for proximity */
	if (err < 0)
		return err;

	err = apds9950_set_piht(client, APDS9950_PS_DETECTION_THRESHOLD);
	if (err < 0)
		return err;

	err = apds9950_set_ailt(client, 0xFFFF);	/* force first ALS interrupt to get the environment reading */
	if (err < 0)
		return err;

	err = apds9950_set_aiht(client, 0);
	if (err < 0)
		return err;

	err = apds9950_set_pers(client, APDS9950_PPERS_2 | APDS9950_APERS_2);	/* 2 consecutive Interrupt persistence */
	if (err < 0)
		return err;

	/* sensor is in disabled mode but all the configurations are preset */

	return 0;
}


#ifdef CONFIG_PM
static int apds9950_suspend(struct device *dev)
{
	struct apds9950_platform_data *pdata = pdev_data->platform_data;

	pr_info("%s\n", __func__);

	if (pdata->power_on)
		pdata->power_on(false);

	return 0;
}

static int apds9950_resume(struct device *dev)
{
	struct apds9950_platform_data *pdata = pdev_data->platform_data;

	pr_info("%s\n", __func__);

	if (pdata->power_on)
		pdata->power_on(true);
	return 0;
}

#else

#define apds9950_suspend	NULL
#define apds9950_resume		NULL

#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
	       regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int sensor_regulator_configure(struct apds9950_data *data, bool on)
{
	struct i2c_client *client = data->client;
	struct apds9950_platform_data *pdata = data->platform_data;
	int rc;

	if (on == false)
		goto hw_shutdown;

	pdata->vcc_ana = regulator_get(&client->dev, "Avago,vdd_ana");
	if (IS_ERR(pdata->vcc_ana)) {
		rc = PTR_ERR(pdata->vcc_ana);
		dev_err(&client->dev,
			"Regulator get failed vcc_ana rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->vcc_ana) > 0) {
		rc = regulator_set_voltage(pdata->vcc_ana, AVDD_VTG_MIN_UV,
					   AVDD_VTG_MAX_UV);
		if (rc) {
			dev_err(&client->dev,
				"regulator set_vtg failed rc=%d\n", rc);
			goto error_set_vtg_vcc_ana;
		}
	}
	if (pdata->digital_pwr_regulator) {
		pdata->vcc_dig = regulator_get(&client->dev, "Avago,vddio_dig");
		if (IS_ERR(pdata->vcc_dig)) {
			rc = PTR_ERR(pdata->vcc_dig);
			dev_err(&client->dev,
				"Regulator get dig failed rc=%d\n", rc);
			goto error_get_vtg_vcc_dig;
		}

		if (regulator_count_voltages(pdata->vcc_dig) > 0) {
			rc = regulator_set_voltage(pdata->vcc_dig,
						   VDDIO_VTG_DIG_MIN_UV, VDDIO_VTG_DIG_MAX_UV);
			if (rc) {
				dev_err(&client->dev,
					"regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_vcc_dig;
			}
		}
	}
	if (pdata->i2c_pull_up) {
		pdata->vcc_i2c = regulator_get(&client->dev, "Avago,vddio_i2c");
		if (IS_ERR(pdata->vcc_i2c)) {
			rc = PTR_ERR(pdata->vcc_i2c);
			dev_err(&client->dev,
				"Regulator get failed rc=%d\n", rc);
			goto error_get_vtg_i2c;
		}
		if (regulator_count_voltages(pdata->vcc_i2c) > 0) {
			rc = regulator_set_voltage(pdata->vcc_i2c,
						   VDDIO_I2C_VTG_MIN_UV, VDDIO_I2C_VTG_MAX_UV);
			if (rc) {
				dev_err(&client->dev,
					"regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_i2c;
			}
		}
	}

	return 0;

error_set_vtg_i2c:
	regulator_put(pdata->vcc_i2c);
error_get_vtg_i2c:
	if (pdata->digital_pwr_regulator)
		if (regulator_count_voltages(pdata->vcc_dig) > 0)
			regulator_set_voltage(pdata->vcc_dig, 0,
					      VDDIO_VTG_DIG_MAX_UV);
error_set_vtg_vcc_dig:
	if (pdata->digital_pwr_regulator)
		regulator_put(pdata->vcc_dig);
error_get_vtg_vcc_dig:
	if (regulator_count_voltages(pdata->vcc_ana) > 0)
		regulator_set_voltage(pdata->vcc_ana, 0, AVDD_VTG_MAX_UV);
error_set_vtg_vcc_ana:
	regulator_put(pdata->vcc_ana);
	return rc;

hw_shutdown:
	if (regulator_count_voltages(pdata->vcc_ana) > 0)
		regulator_set_voltage(pdata->vcc_ana, 0, AVDD_VTG_MAX_UV);
	regulator_put(pdata->vcc_ana);
	if (pdata->digital_pwr_regulator) {
		if (regulator_count_voltages(pdata->vcc_dig) > 0)
			regulator_set_voltage(pdata->vcc_dig, 0,
					      VDDIO_VTG_DIG_MAX_UV);
		regulator_put(pdata->vcc_dig);
	}
	if (pdata->i2c_pull_up) {
		if (regulator_count_voltages(pdata->vcc_i2c) > 0)
			regulator_set_voltage(pdata->vcc_i2c, 0,
					      VDDIO_I2C_VTG_MAX_UV);
		regulator_put(pdata->vcc_i2c);
	}
	return 0;
}


static int sensor_regulator_power_on(struct apds9950_data *data, bool on)
{
	struct i2c_client *client = data->client;
	struct apds9950_platform_data *pdata = data->platform_data;
	int rc;

	if (on == false)
		goto power_off;

	rc = reg_set_optimum_mode_check(pdata->vcc_ana, AVDD_ACTIVE_LOAD_UA);
	if (rc < 0) {
		dev_err(&client->dev,
			"Regulator vcc_ana set_opt failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(pdata->vcc_ana);
	if (rc) {
		dev_err(&client->dev,
			"Regulator vcc_ana enable failed rc=%d\n", rc);
		goto error_reg_en_vcc_ana;
	}

	if (pdata->digital_pwr_regulator) {
		rc = reg_set_optimum_mode_check(pdata->vcc_dig,
						VDDIO_ACTIVE_LOAD_DIG_UA);
		if (rc < 0) {
			dev_err(&client->dev,
				"Regulator vcc_dig set_opt failed rc=%d\n",
				rc);
			goto error_reg_opt_vcc_dig;
		}

		rc = regulator_enable(pdata->vcc_dig);
		if (rc) {
			dev_err(&client->dev,
				"Regulator vcc_dig enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_dig;
		}
	}

	if (pdata->i2c_pull_up) {
		rc = reg_set_optimum_mode_check(pdata->vcc_i2c, VDDIO_I2C_LOAD_UA);
		if (rc < 0) {
			dev_err(&client->dev,
				"Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(pdata->vcc_i2c);
		if (rc) {
			dev_err(&client->dev,
				"Regulator vcc_i2c enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_i2c;
		}
	}

	msleep(130);

	return 0;

error_reg_en_vcc_i2c:
	if (pdata->i2c_pull_up)
		reg_set_optimum_mode_check(pdata->vcc_i2c, 0);
error_reg_opt_i2c:
	if (pdata->digital_pwr_regulator)
		regulator_disable(pdata->vcc_dig);
error_reg_en_vcc_dig:
	if (pdata->digital_pwr_regulator)
		reg_set_optimum_mode_check(pdata->vcc_dig, 0);
error_reg_opt_vcc_dig:
	regulator_disable(pdata->vcc_ana);
error_reg_en_vcc_ana:
	reg_set_optimum_mode_check(pdata->vcc_ana, 0);
	return rc;

power_off:
	reg_set_optimum_mode_check(pdata->vcc_ana, 0);
	regulator_disable(pdata->vcc_ana);
	if (pdata->digital_pwr_regulator) {
		reg_set_optimum_mode_check(pdata->vcc_dig, 0);
		regulator_disable(pdata->vcc_dig);
	}
	if (pdata->i2c_pull_up) {
		reg_set_optimum_mode_check(pdata->vcc_i2c, 0);
		regulator_disable(pdata->vcc_i2c);
	}
	msleep(50);
	return 0;
}

static int sensor_platform_hw_power_on(bool on)
{
	sensor_regulator_power_on(pdev_data, on);

	return 0;
}

static int sensor_platform_hw_init(void)
{
	struct i2c_client *client = pdev_data->client;
	struct apds9950_data *data = pdev_data;
	int error;

	error = sensor_regulator_configure(data, true);

	if (gpio_is_valid(data->platform_data->irq_gpio)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(data->platform_data->irq_gpio, "apds9950_irq_gpio");
		if (error) {
			dev_err(&client->dev, "unable to request gpio [%d]\n",
				data->platform_data->irq_gpio);
		}
		error = gpio_direction_input(data->platform_data->irq_gpio);
		if (error) {
			dev_err(&client->dev,
				"unable to set direction for gpio [%d]\n",
				data->platform_data->irq_gpio);
		}
		data->irq = client->irq = gpio_to_irq(data->platform_data->irq_gpio);
	} else {
		dev_err(&client->dev, "irq gpio not provided\n");
	}
	return 0;
}

static void sensor_platform_hw_exit(void)
{
	struct apds9950_data *data = pdev_data;

	sensor_regulator_configure(data, false);

	if (gpio_is_valid(data->platform_data->irq_gpio))
		gpio_free(data->platform_data->irq_gpio);
}

/* test - tmp value */
/* ensure the coefficients do not exceed 9999 */
static int RGB_COE_X[3] = {91, -352, -1560}; /* {0.090599, 6.026495, -4.93002}; */
static int RGB_COE_Y[3] = {6026, 6300, 3697}; /* {-0.35177, 6.30048, -4.54057}; */
static int RGB_COE_Z[3] = { -4930, -4541, -113}; /*{-1.55953, 3.69696, -0.11315}; */

static int sensor_parse_dt(struct device *dev, struct apds9950_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int icnt;

	/* regulator info */
	pdata->i2c_pull_up = of_property_read_bool(np, "Avago,i2c-pull-up");
	pdata->digital_pwr_regulator = of_property_read_bool(np,
				       "Ashai,dig-reg-support");

	/* reset, irq gpio info */
	pdata->irq_gpio = of_get_named_gpio_flags(np, "Avago,irq-gpio",
			  0, &pdata->irq_gpio_flags);

	/* set functions of platform data */
	pdata->init = sensor_platform_hw_init;
	pdata->exit = sensor_platform_hw_exit;
	pdata->power_on = sensor_platform_hw_power_on;

	/* TODO : DTS */
	pdata->ppcount = 12;
	for (icnt = 0; icnt < 3; icnt++) {
		pdata->RGB_COE_X[icnt] = RGB_COE_X[icnt];
		pdata->RGB_COE_Y[icnt] = RGB_COE_Y[icnt];
		pdata->RGB_COE_Z[icnt] = RGB_COE_Z[icnt];
	}
	return 0;
}
#endif


/*
* I2C init/probing/exit functions
*/

static struct i2c_driver apds9950_driver;
static int __devinit apds9950_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds9950_data *data;
	struct apds9950_platform_data *platform_data;

	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	/* Allocate memory for driver data */
	pdev_data = data = kzalloc(sizeof(struct apds9950_data), GFP_KERNEL);
	if (!pdev_data) {
		dev_err(&client->dev, "%s: memory allocation failed.", __func__);
		return -ENOMEM;
	}

	/***** Set layout information *****/
	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
					     sizeof(struct apds9950_platform_data), GFP_KERNEL);
		if (!platform_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		pdev_data->platform_data = platform_data;
		client->dev.platform_data = platform_data;
		err = sensor_parse_dt(&client->dev, platform_data);
		if (err)
			return err;
	} else {
		platform_data = client->dev.platform_data;

	}

	data->client = client;
	apds9950_i2c_client = client;

	/* h/w initialization */
	if (platform_data->init)
		err = platform_data->init();

	if (platform_data->power_on)
		err = platform_data->power_on(true);

	i2c_set_clientdata(client, data);

	data->enable = 0;	/* default mode is standard */
	data->ps_threshold = APDS9950_PS_DETECTION_THRESHOLD;
	data->ps_hysteresis_threshold = APDS9950_PS_HSYTERESIS_THRESHOLD;
	data->ps_detection = 0;	/* default to no detection */
	data->enable_als_sensor = 0;	/* default to 0 */
	data->enable_ps_sensor = 0;	/* default to 0 */
	data->als_poll_delay = 100;	/* default to 100ms */
	data->als_atime_index = APDS9950_ALS_RES_100MS;	/* 100ms ATIME */
	data->als_again_index = APDS9950_ALS_GAIN_1X;	/* 1x AGAIN */
	data->als_prev_lux = 0;

	mutex_init(&data->update_lock);

	if (request_irq(data->irq, apds9950_interrupt, IRQF_TRIGGER_FALLING,
			APDS9950_DRV_NAME, (void *)client)) {
		printk("%s Could not allocate APDS9950_INT !\n", __func__);
	}

	/* kernel 3.0.15 */
	irq_set_irq_wake(client->irq, 1);
	/* kernel 2.6.36 */
	/* set_irq_wake(client->irq, 1); */

	INIT_DELAYED_WORK(&data->dwork, apds9950_work_handler);

	INIT_DELAYED_WORK(&data->als_dwork, apds9950_als_polling_work_handler);

	printk("%s interrupt is hooked\n", __func__);

	/* Initialize the APDS9950 chip */
	err = apds9950_init_client(client);
	if (err)
		goto exit_kfree;

	/* Register to Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		err = -ENOMEM;
		printk("Failed to allocate input device als\n");
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		printk("Failed to allocate input device ps\n");
		goto exit_free_dev_als;
	}

	set_bit(EV_ABS, data->input_dev_als->evbit);
	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_als, ABS_LIGHT, 0, 30000, 0, 0);	/* lux */
	input_set_abs_params(data->input_dev_als, ABS_CCT, 0, 10000, 0, 0); /* color temperature cct */
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 1, 0, 0);

	data->input_dev_als->name = "light";
	data->input_dev_ps->name = "proximity";

	/* Register device sysfs */
	data->input_dev_als->dev.parent = &data->client->dev;
	data->input_dev_ps->dev.parent = &data->client->dev;

	input_set_drvdata(data->input_dev_als, data);
	input_set_drvdata(data->input_dev_ps, data);

	dev_set_drvdata(&data->input_dev_als->dev, data);
	dev_set_drvdata(&data->input_dev_ps->dev, data);

	err = input_register_device(data->input_dev_als);
	if (err) {
		err = -ENOMEM;
		printk("Unable to register input device als: %s\n",
		       data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		printk("Unable to register input device ps: %s\n",
		       data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}

	/* Register sysfs hooks */
	dev_set_drvdata(&client->dev, data);
	err = sysfs_create_group(&client->dev.kobj, &apds9950_attr_group);
	if (err)
		goto exit_unregister_dev_ps;

	/* Register for sensor ioctl */
	err = misc_register(&apds9950_ps_device);
	if (err) {
		printk("Unalbe to register ps ioctl: %d", err);
		goto exit_remove_sysfs_group;
	}

	err = device_create_file(&data->input_dev_ps->dev, &dev_attr_proximity_enable);
	err = device_create_file(&data->input_dev_ps->dev, &dev_attr_pdata);
	if (err) {
		printk("Unalbe to Proximity device_create_file: %d", err);
		goto exit_remove_sysfs_group;
	}

	err = misc_register(&apds9950_als_device);
	if (err) {
		printk("Unalbe to register als ioctl: %d", err);
		goto exit_unregister_ps_ioctl;
	}
	err = device_create_file(&data->input_dev_als->dev, &dev_attr_light_enable);
	err = device_create_file(&data->input_dev_als->dev, &dev_attr_light_poll_delay);
	err = device_create_file(&data->input_dev_als->dev, &dev_attr_cdata);
	if (err) {
		printk("Unalbe to Light device_create_file: %d", err);
		goto exit_remove_sysfs_group;
	}

	printk("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);

	return 0;

exit_unregister_ps_ioctl:
	misc_deregister(&apds9950_ps_device);
exit_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &apds9950_attr_group);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_als);
exit_free_dev_ps:
exit_free_dev_als:
exit_free_irq:
	free_irq(client->irq, data);
exit_kfree:
	if (platform_data->power_on)
		platform_data->power_on(false);
	if (platform_data->exit)
		platform_data->exit();

	kfree(data);
exit:
	return err;
}

static int __devexit apds9950_remove(struct i2c_client *client)
{
	struct apds9950_data *data = i2c_get_clientdata(client);
	struct apds9950_platform_data *pdata = data->platform_data;

	__cancel_delayed_work(&data->dwork);
	__cancel_delayed_work(&data->als_dwork);

	/* Power down the device */
	apds9950_set_enable(client, 0);

	misc_deregister(&apds9950_als_device);
	misc_deregister(&apds9950_ps_device);

	sysfs_remove_group(&client->dev.kobj, &apds9950_attr_group);

	device_remove_file(&data->input_dev_ps->dev, &dev_attr_proximity_enable);
	device_remove_file(&data->input_dev_ps->dev, &dev_attr_pdata);

	device_remove_file(&data->input_dev_als->dev, &dev_attr_light_enable);
	device_remove_file(&data->input_dev_als->dev, &dev_attr_light_poll_delay);
	device_remove_file(&data->input_dev_als->dev, &dev_attr_cdata);

	input_unregister_device(data->input_dev_ps);
	input_unregister_device(data->input_dev_als);

	free_irq(client->irq, data);

	if (pdata->power_on)
		pdata->power_on(false);

	if (pdata->exit)
		pdata->exit();

	kfree(data);

	return 0;
}


static const struct i2c_device_id apds9950_id[] = {
	{ "apds9950", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, apds9950_id);

#ifdef CONFIG_OF
static struct of_device_id apds9950_match_table[] = {
	{ .compatible = "Avago,apds9950",},
	{ },
};
#else
#define apds9950_match_table NULL
#endif

static const struct dev_pm_ops apds9950_pm_ops = {
#ifdef CONFIG_PM
	.suspend	= apds9950_suspend,
	.resume 	= apds9950_resume,
#endif
};

static struct i2c_driver apds9950_driver = {
	.driver = {
		.name = "apds9950",
		.owner	= THIS_MODULE,
		.of_match_table = apds9950_match_table,
#ifdef CONFIG_PM
		.pm = &apds9950_pm_ops,
#endif
	},
	.probe		= apds9950_probe,
	.remove 	= __devexit_p(apds9950_remove),
	.id_table = apds9950_id,
};

static int __init apds9950_init(void)
{
	printk(KERN_INFO "APDS9950 Proximity&Ambient driver: initialize.");

	apds9950_workqueue = create_workqueue("proximity_als");

	if (!apds9950_workqueue)
		return -ENOMEM;

	return i2c_add_driver(&apds9950_driver);
}

static void __exit apds9950_exit(void)
{
	printk(KERN_INFO "APDS9950 Proximity&Ambient driver: release.");

	if (apds9950_workqueue)
		destroy_workqueue(apds9950_workqueue);

	apds9950_workqueue = NULL;

	i2c_del_driver(&apds9950_driver);
}

module_init(apds9950_init);
module_exit(apds9950_exit);

MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS9950 ambient light + color+ proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

