/* Touch_synaptics.c
 *
 * Copyright (C) 2012 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/input/lge_touch_core_vu3evb.h>
#include <linux/input/touch_synaptics_vu3evb.h>
#include <mach/board_lge.h>

#if defined(CONFIG_MACH_APQ8064_GV_KR)
#include "SynaImage_for_GVKR.h"
#elif defined(CUST_G_TOUCH) || defined(CONFIG_MACH_APQ8064_GKATT)
#include "SynaImage_vu3evb.h"
#elif defined(CONFIG_MACH_APQ8064_GVDCM)
#include "SynaImage_for_DCM.h"
#elif defined(CONFIG_MACH_APQ8064_J1D) || defined(CONFIG_MACH_APQ8064_J1KD)
#include "SynaImage_for_GJ.h"
#else
#include "SynaImage.h"
#define G_ONLY
#endif


#include <linux/regulator/machine.h>

/* RMI4 spec from 511-000405-01 Rev.D
 * Function	Purpose									See page
 * $01		RMI Device Control						45
 * $1A		0-D capacitive button sensors			61
 * $05		Image Reporting							68
 * $07		Image Reporting							75
 * $08		BIST									82
 * $09		BIST									87
 * $11		2-D TouchPad sensors					93
 * $19		0-D capacitive button sensors			141
 * $30		GPIO/LEDs								148
 * $31		LEDs									162
 * $34		Flash Memory Management					163
 * $36		Auxiliary ADC							174
 * $54		Test Reporting							176
 */
#define RMI_DEVICE_CONTROL				0x01
#define TOUCHPAD_SENSORS				0x11
#define CAPACITIVE_BUTTON_SENSORS		0x1A
#define GPIO_LEDS						0x30
#define LEDS							0x31
#define ANALOG_CONTROL					0x54
#define TIMER							0x32
#define FLASH_MEMORY_MANAGEMENT			0x34
#define AUXILIARY_ADC					0x36

/* Register Map & Register bit mask
 * - Please check "One time" this map before using this device driver
 */
/* RMI_DEVICE_CONTROL */
#define MANUFACTURER_ID_REG				(ts->common_fc.dsc.query_base)			/* Manufacturer ID */
#define FW_REVISION_REG					(ts->common_fc.dsc.query_base+3)		/* FW revision */
#define PRODUCT_ID_REG					(ts->common_fc.dsc.query_base+11)		/* Product ID */

#define DEVICE_COMMAND_REG				(ts->common_fc.dsc.command_base)

#define DEVICE_CONTROL_REG 				(ts->common_fc.dsc.control_base)		/* Device Control */
#define DEVICE_CONTROL_NORMAL_OP		0x00	/* sleep mode : go to doze mode after 500 ms */
#define DEVICE_CONTROL_SLEEP 			0x01	/* sleep mode : go to sleep */
#define DEVICE_CONTROL_SPECIFIC			0x02	/* sleep mode : go to doze mode after 5 sec */
#define DEVICE_CONTROL_NOSLEEP			0x04
#define DEVICE_CONTROL_CONFIGURED		0x80
#ifdef CUST_G_TOUCH
#define DEVICE_CHARGER_CONNECTED		0x20
#endif

#define INTERRUPT_ENABLE_REG			(ts->common_fc.dsc.control_base+1)		/* Interrupt Enable 0 */

#define DEVICE_STATUS_REG				(ts->common_fc.dsc.data_base)			/* Device Status */
#define DEVICE_FAILURE_MASK				0x03
#define DEVICE_CRC_ERROR_MASK			0x04
#define DEVICE_STATUS_FLASH_PROG		0x40
#define DEVICE_STATUS_UNCONFIGURED		0x80

#define INTERRUPT_STATUS_REG			(ts->common_fc.dsc.data_base+1)		/* Interrupt Status */
#ifdef CUST_G_TOUCH
//do nothing
#else
#define INTERRUPT_MASK_FLASH			0x01
#define INTERRUPT_MASK_ABS0				0x04
#define INTERRUPT_MASK_BUTTON			0x10
#endif

/* TOUCHPAD_SENSORS */
#define FINGER_COMMAND_REG				(ts->finger_fc.dsc.command_base)

#define FINGER_STATE_REG				(ts->finger_fc.dsc.data_base)			/* Finger State */
#define FINGER_DATA_REG_START			(ts->finger_fc.dsc.data_base+3)		/* Finger Data Register */
#define FINGER_STATE_MASK				0x03
#define REG_X_POSITION					0
#define REG_Y_POSITION					1
#define REG_YX_POSITION					2
#define REG_WY_WX						3
#define REG_Z							4
#define TWO_D_EXTEND_STATUS			(ts->finger_fc.dsc.data_base+53)

#define TWO_D_REPORTING_MODE			(ts->finger_fc.dsc.control_base+0)		/* 2D Reporting Mode */
#ifdef CUST_G_TOUCH
#define REPORT_BEYOND_CLIP				0x80
#endif
#define REPORT_MODE_CONTINUOUS			0x00
#define REPORT_MODE_REDUCED				0x01
#define ABS_FILTER						0x08
#define PALM_DETECT_REG 				(ts->finger_fc.dsc.control_base+1)		/* Palm Detect */
#define DELTA_X_THRESH_REG 				(ts->finger_fc.dsc.control_base+2)		/* Delta-X Thresh */
#define DELTA_Y_THRESH_REG 				(ts->finger_fc.dsc.control_base+3)		/* Delta-Y Thresh */
#define SENSOR_MAX_X_POS				(ts->finger_fc.dsc.control_base+6)		/* SensorMaxXPos */
#define SENSOR_MAX_Y_POS				(ts->finger_fc.dsc.control_base+8)		/* SensorMaxYPos */

/* CAPACITIVE_BUTTON_SENSORS */
#define BUTTON_COMMAND_REG				(ts->button_fc.dsc.command_base)
#define BUTTON_DATA_REG					(ts->button_fc.dsc.data_base)			/* Button Data */

#define MAX_NUM_OF_BUTTON				4

/* ANALOG_CONTROL */
#define ANALOG_COMMAND_REG				(ts->analog_fc.dsc.command_base)
#define FORCE_UPDATE					0x04

#define ANALOG_CONTROL_REG				(ts->analog_fc.dsc.control_base)
#define FORCE_FAST_RELAXATION			0x04

#define FAST_RELAXATION_RATE			(ts->analog_fc.dsc.control_base+16)

/* FLASH_MEMORY_MANAGEMENT */
#define FLASH_CONFIG_ID_REG				(ts->flash_fc.dsc.control_base)		/* Flash Control */
#define FLASH_CONTROL_REG				(ts->flash_fc.dsc.data_base+18)
#define FLASH_STATUS_MASK				0xF0

/* Page number */
#define COMMON_PAGE						(ts->common_fc.function_page)
#define FINGER_PAGE						(ts->finger_fc.function_page)
#define BUTTON_PAGE						(ts->button_fc.function_page)
#define ANALOG_PAGE						(ts->analog_fc.function_page)
#define FLASH_PAGE						(ts->flash_fc.function_page)
#define DEFAULT_PAGE					0x00

#ifdef CUST_G_TOUCH
#define SMALL_OBJECT_DETECTION_TUNNING_REG	(ts->finger_fc.dsc.control_base+45) //0x0083
#define SMALL_OBJECT_DETECTION			0x04
#endif

/* Get user-finger-data from register.
 */
#define TS_SNTS_GET_X_POSITION(_high_reg, _low_reg) \
		( ((u16)((_high_reg << 4) & 0x0FF0) | (u16)(_low_reg&0x0F)))
#define TS_SNTS_GET_Y_POSITION(_high_reg, _low_reg) \
		( ((u16)((_high_reg << 4) & 0x0FF0) | (u16)((_low_reg >> 4) & 0x0F)))
#define TS_SNTS_GET_WIDTH_MAJOR(_width) \
		((((_width & 0xF0) >> 4) - (_width & 0x0F)) > 0) ? (_width & 0xF0) >> 4 : _width & 0x0F
#define TS_SNTS_GET_WIDTH_MINOR(_width) \
		((((_width & 0xF0) >> 4) - (_width & 0x0F)) > 0) ? _width & 0x0F : (_width & 0xF0) >> 4
#define TS_SNTS_GET_ORIENTATION(_width) \
		((((_width & 0xF0) >> 4) - (_width & 0x0F)) > 0) ? 0 : 1
#define TS_SNTS_GET_PRESSURE(_pressure) \
		_pressure

/* GET_BIT_MASK & GET_INDEX_FROM_MASK
 *
 * For easily checking the user input.
 * Usually, User use only one or two fingers.
 * However, we should always check all finger-status-register
 * because we can't know the total number of fingers.
 * These Macro will prevent it.
 */
#define GET_BIT_MASK(_finger_status_reg)	\
		(_finger_status_reg[2] & 0x04)<<7 | (_finger_status_reg[2] & 0x01)<<8 |	\
		(_finger_status_reg[1] & 0x40)<<1 | (_finger_status_reg[1] & 0x10)<<2 | \
		(_finger_status_reg[1] & 0x04)<<3 | (_finger_status_reg[1] & 0x01)<<4 |	\
		(_finger_status_reg[0] & 0x40)>>3 | (_finger_status_reg[0] & 0x10)>>2 | \
		(_finger_status_reg[0] & 0x04)>>1 | (_finger_status_reg[0] & 0x01)

#define GET_INDEX_FROM_MASK(_index, _bit_mask, _max_finger)	\
		for(; !((_bit_mask>>_index)&0x01) && _index <= _max_finger; _index++);	\
		if (_index <= _max_finger) _bit_mask &= ~(_bit_mask & (1<<(_index)));

#ifdef CUST_G_TOUCH
u8 pressure_zero = 0;
extern int ts_charger_plug;
extern int ts_charger_type;
#ifdef CUST_G_TOUCH_FREQ_HOPPING
extern int cur_hopping_idx;
int cns_en = 0;
u8 hopping = 0;
#endif
#endif

/* wrapper function for i2c communication - except defalut page
 * if you have to select page for reading or writing, then using this wrapper function */
int synaptics_ts_page_data_read(struct i2c_client *client, u8 page, u8 reg, int size, u8 *data)
{
	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, page) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(client, reg, size, data) < 0)) {
		TOUCH_ERR_MSG("[%dP:%d]register read fail\n", page, reg);
		return -EIO;
	}

	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, DEFAULT_PAGE) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	return 0;
}

int synaptics_ts_page_data_write(struct i2c_client *client, u8 page, u8 reg, int size, u8 *data)
{
	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, page) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_write(client, reg, size, data) < 0)) {
		TOUCH_ERR_MSG("[%dP:%d]register read fail\n", page, reg);
		return -EIO;
	}

	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, DEFAULT_PAGE) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	return 0;
}

int synaptics_ts_page_data_write_byte(struct i2c_client *client, u8 page, u8 reg, u8 data)
{
	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, page) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_write_byte(client, reg, data) < 0)) {
		TOUCH_ERR_MSG("[%dP:%d]register write fail\n", page, reg);
		return -EIO;
	}

	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, DEFAULT_PAGE) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	return 0;
}

int synaptics_ts_get_data(struct i2c_client *client, struct touch_data* data)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);

	u16 touch_finger_bit_mask=0;
	u8 finger_index=0;
	u8 index=0;
	u8 buf=0;
	u8 cnt;
	u8 buf2=0;
	u16 alpha = 0;
	u8 cns = 0;
	u16 im = 0;
	u16 vm = 0;
	u16 aim = 0;
	data->total_num = 0;
#ifdef CUST_G_TOUCH
	pressure_zero = 0;
#ifdef CUST_G_TOUCH_FREQ_HOPPING
	hopping = 0;
#endif
#endif

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (unlikely(touch_i2c_read(client, DEVICE_STATUS_REG,
			sizeof(ts->ts_data.interrupt_status_reg),
			&ts->ts_data.device_status_reg) < 0)) {
		TOUCH_ERR_MSG("DEVICE_STATUS_REG read fail\n");
		goto err_synaptics_getdata;
	}

	/* ESD damage check */
	if ((ts->ts_data.device_status_reg & DEVICE_FAILURE_MASK)== DEVICE_FAILURE_MASK) {
		TOUCH_ERR_MSG("ESD damage occured. Reset Touch IC\n");
		goto err_synaptics_device_damage;
	}

	/* Internal reset check */
	if (((ts->ts_data.device_status_reg & DEVICE_STATUS_UNCONFIGURED) >> 7) == 1) {
		TOUCH_ERR_MSG("Touch IC resetted internally. Reconfigure register setting\n");
		goto err_synaptics_device_damage;
	}

	if (unlikely(touch_i2c_read(client, INTERRUPT_STATUS_REG,
			sizeof(ts->ts_data.interrupt_status_reg),
			&ts->ts_data.interrupt_status_reg) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
		goto err_synaptics_getdata;
	}

	if (unlikely(touch_debug_mask & DEBUG_GET_DATA))
		TOUCH_INFO_MSG("Interrupt_status : 0x%x\n", ts->ts_data.interrupt_status_reg);

#ifdef CUST_G_TOUCH
//do nothing
#else
	/* IC bug Exception handling - Interrupt status reg is 0 when interrupt occur */
	if (ts->ts_data.interrupt_status_reg == 0) {
		TOUCH_ERR_MSG("Interrupt_status reg is 0. Something is wrong in IC\n");
		goto err_synaptics_device_damage;
	}
#endif

	/* Because of ESD damage... */
	if (unlikely(ts->ts_data.interrupt_status_reg & ts->interrupt_mask.flash)){
		TOUCH_ERR_MSG("Impossible Interrupt\n");
		goto err_synaptics_device_damage;
	}

#ifdef CUST_G_TOUCH
	if ( ts->ts_data.interrupt_status_reg == 0x08 || ts->ts_data.interrupt_status_reg == 0x00 ) {
		TOUCH_ERR_MSG("Ignore interrupt. interrupt status reg = 0x%x\n", ts->ts_data.interrupt_status_reg);
		goto ignore_interrupt;
	}
#endif

	/* Finger */
	if (likely(ts->ts_data.interrupt_status_reg & ts->interrupt_mask.abs)) {
		if (unlikely(touch_i2c_read(client, FINGER_STATE_REG,
				sizeof(ts->ts_data.finger.finger_status_reg),
				ts->ts_data.finger.finger_status_reg) < 0)) {
			TOUCH_ERR_MSG("FINGER_STATE_REG read fail\n");
			goto err_synaptics_getdata;
		}

		touch_finger_bit_mask = GET_BIT_MASK(ts->ts_data.finger.finger_status_reg);
		if (unlikely(touch_debug_mask & DEBUG_GET_DATA)) {
			TOUCH_INFO_MSG("Finger_status : 0x%x, 0x%x, 0x%x\n", ts->ts_data.finger.finger_status_reg[0],
					ts->ts_data.finger.finger_status_reg[1], ts->ts_data.finger.finger_status_reg[2]);
			TOUCH_INFO_MSG("Touch_bit_mask: 0x%x\n", touch_finger_bit_mask);
		}

		while(touch_finger_bit_mask) {
			GET_INDEX_FROM_MASK(finger_index, touch_finger_bit_mask, MAX_NUM_OF_FINGERS)
			if (unlikely(touch_i2c_read(ts->client,
					FINGER_DATA_REG_START + (NUM_OF_EACH_FINGER_DATA_REG * finger_index),
					NUM_OF_EACH_FINGER_DATA_REG,
					ts->ts_data.finger.finger_reg[finger_index]) < 0)) {
				TOUCH_ERR_MSG("FINGER_DATA_REG read fail\n");
				goto err_synaptics_getdata;
			}

			data->curr_data[finger_index].id = finger_index;
			data->curr_data[finger_index].x_position =
				TS_SNTS_GET_X_POSITION(ts->ts_data.finger.finger_reg[finger_index][REG_X_POSITION],
									   ts->ts_data.finger.finger_reg[finger_index][REG_YX_POSITION]);
			
			data->curr_data[finger_index].x_position = ts->pdata->caps->x_max - data->curr_data[finger_index].x_position;
				
			data->curr_data[finger_index].y_position =
				TS_SNTS_GET_Y_POSITION(ts->ts_data.finger.finger_reg[finger_index][REG_Y_POSITION],
									   ts->ts_data.finger.finger_reg[finger_index][REG_YX_POSITION]);

			data->curr_data[finger_index].y_position = ts->pdata->caps->y_max - data->curr_data[finger_index].y_position;
			
			data->curr_data[finger_index].width_major = TS_SNTS_GET_WIDTH_MAJOR(ts->ts_data.finger.finger_reg[finger_index][REG_WY_WX]);
			data->curr_data[finger_index].width_minor = TS_SNTS_GET_WIDTH_MINOR(ts->ts_data.finger.finger_reg[finger_index][REG_WY_WX]);
			data->curr_data[finger_index].width_orientation = TS_SNTS_GET_ORIENTATION(ts->ts_data.finger.finger_reg[finger_index][REG_WY_WX]);
			data->curr_data[finger_index].pressure = TS_SNTS_GET_PRESSURE(ts->ts_data.finger.finger_reg[finger_index][REG_Z]);
			data->curr_data[finger_index].status = FINGER_PRESSED;
#ifdef CUST_G_TOUCH
			if(ts->pdata->role->ghost_detection_enable) {
				if(data->curr_data[finger_index].pressure == 0) pressure_zero = 1;
			}
#endif

			if (unlikely(touch_debug_mask & DEBUG_GET_DATA))
				TOUCH_INFO_MSG("<%d> pos(%4d,%4d) w_m[%2d] w_n[%2d] w_o[%2d] p[%2d]\n",
								finger_index, data->curr_data[finger_index].x_position, data->curr_data[finger_index].y_position,
								data->curr_data[finger_index].width_major, data->curr_data[finger_index].width_minor,
								data->curr_data[finger_index].width_orientation, data->curr_data[finger_index].pressure);

			index++;
		}
		data->total_num = index;
		if (unlikely(touch_debug_mask & DEBUG_GET_DATA))
			TOUCH_INFO_MSG("Total_num: %d\n", data->total_num);
	}

	 /* Button */
	if (unlikely(ts->button_fc.dsc.id != 0)) {
		if (likely(ts->ts_data.interrupt_status_reg & ts->interrupt_mask.button)) {
			if (unlikely(synaptics_ts_page_data_read(client, BUTTON_PAGE, BUTTON_DATA_REG,
					sizeof(ts->ts_data.button_data_reg), &ts->ts_data.button_data_reg) < 0)) {
				TOUCH_ERR_MSG("BUTTON_DATA_REG read fail\n");
				goto err_synaptics_getdata;
			}

			if (unlikely(touch_debug_mask & DEBUG_BUTTON))
				TOUCH_DEBUG_MSG("Button register: 0x%x\n", ts->ts_data.button_data_reg);

			if (ts->ts_data.button_data_reg) {
				/* pressed - find first one */
				for (cnt = 0; cnt < ts->pdata->caps->number_of_button; cnt++)
				{
#ifdef CUST_G_TOUCH
					if(ts->ic_panel_type == G_IC3203_G2) {
						if ((ts->ts_data.button_data_reg >> (cnt << 1)) & 0x3) {
							ts->ts_data.button.key_code = ts->pdata->caps->button_name[cnt];
							data->curr_button.key_code = ts->ts_data.button.key_code;
							data->curr_button.state = 1;
							break;
						}
					} else {
						if ((ts->ts_data.button_data_reg >> cnt) & 0x1) {
							ts->ts_data.button.key_code = ts->pdata->caps->button_name[cnt];
							data->curr_button.key_code = ts->ts_data.button.key_code;
							data->curr_button.state = 1;
							break;
						}
					}
#endif
				}
			}else {
				/* release */
				data->curr_button.key_code = ts->ts_data.button.key_code;
				data->curr_button.state = 0;
			}
		}
	}

	/* Palm check */
	if (unlikely(touch_i2c_read(client, TWO_D_EXTEND_STATUS, 1, &buf) < 0)){
	       TOUCH_ERR_MSG("TWO_D_EXTEND_STATUS read fail\n");
	       goto err_synaptics_getdata;
	}
	data->palm = buf & 0x2;

	if( (ts_charger_plug == 1 && (data->prev_total_num != data->total_num)) ||
		(touch_debug_mask & DEBUG_NOISE) ) {
		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x0e, 1, &buf) < 0)) {
			TOUCH_ERR_MSG("Alpha REG read fail\n");
			goto err_synaptics_getdata;
		}
		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x0f, 1, &buf2) < 0)) {
			TOUCH_ERR_MSG("Alpha REG read fail\n");
			goto err_synaptics_getdata;
		}
		alpha = (buf2<<8)|buf;

		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x0D, 1, &cns) < 0)) {
			TOUCH_ERR_MSG("Current Noise State REG read fail\n");
			goto err_synaptics_getdata;
		}
#ifdef CUST_G_TOUCH_FREQ_HOPPING
		if(ts_charger_plug && cns >= 1) {
			cns_en = 1;
			if(cur_hopping_idx != 4){
				buf = 0x84;
				synaptics_ts_page_data_write(client, 0x01, 0x04, 1, &buf);
				cur_hopping_idx = 4;
				hopping = 1;
				TOUCH_INFO_MSG("cur_hopping_idx [ %s ] = %x %x \n", __func__, buf, hopping);
			} else {
				hopping = 0;
			}
		}
#endif
		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x05, 1, &buf) < 0)) {
			TOUCH_ERR_MSG("Interference Metric REG read fail\n");
			goto err_synaptics_getdata;
		}
		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x06, 1, &buf2) < 0)) {
			TOUCH_ERR_MSG("Interference Metric REG read fail\n");
			goto err_synaptics_getdata;
		}
		im = (buf2<<8)|buf;

		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x09, 1, &buf) < 0)) {
			TOUCH_ERR_MSG("Variance Metric REG read fail\n");
			goto err_synaptics_getdata;
		}
		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x0a, 1, &buf2) < 0)) {
			TOUCH_ERR_MSG("Variance Metric REG read fail\n");
			goto err_synaptics_getdata;
		}
		vm = (buf2<<8)|buf;

		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x0b, 1, &buf) < 0)) {
			TOUCH_ERR_MSG("Averaged IM REG read fail\n");
			goto err_synaptics_getdata;
		}
		if (unlikely(synaptics_ts_page_data_read(client, ANALOG_PAGE, 0x0c, 1, &buf2) < 0)) {
			TOUCH_ERR_MSG("Averaged IM REG read fail\n");
			goto err_synaptics_getdata;
		}
		aim = (buf2<<8)|buf;

		TOUCH_INFO_MSG("  A[%5d]   CNS[%d]   IM[%5d]   VM[%5d]   AIM[%5d]\n", alpha, cns, im, vm, aim);
	}

	return 0;

err_synaptics_device_damage:
err_synaptics_getdata:
	return -EIO;
#ifdef CUST_G_TOUCH
ignore_interrupt:
	return -IGNORE_INTERRUPT;
#endif
}

static int read_page_description_table(struct i2c_client* client)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	struct function_descriptor buffer;
	unsigned short u_address = 0;
	unsigned short page_num = 0;

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	memset(&buffer, 0x0, sizeof(struct function_descriptor));
	memset(&ts->common_fc, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->finger_fc, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->button_fc, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->analog_fc, 0x0, sizeof(struct ts_ic_function));
	memset(&ts->flash_fc, 0x0, sizeof(struct ts_ic_function));

	for(page_num = 0; page_num < PAGE_MAX_NUM; page_num++) {
		if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, page_num) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}

		for(u_address = DESCRIPTION_TABLE_START; u_address > 10; u_address -= sizeof(struct function_descriptor)) {
			if (unlikely(touch_i2c_read(client, u_address, sizeof(buffer), (unsigned char *)&buffer) < 0)) {
				TOUCH_ERR_MSG("RMI4 Function Descriptor read fail\n");
				return -EIO;
			}

			if (buffer.id == 0)
				break;

			switch (buffer.id) {
			case RMI_DEVICE_CONTROL:
				ts->common_fc.dsc = buffer;
				ts->common_fc.function_page = page_num;
				break;
			case TOUCHPAD_SENSORS:
				ts->finger_fc.dsc = buffer;
				ts->finger_fc.function_page = page_num;
				break;
			case CAPACITIVE_BUTTON_SENSORS:
				ts->button_fc.dsc = buffer;
				ts->button_fc.function_page = page_num;
				break;
			case ANALOG_CONTROL:
				ts->analog_fc.dsc = buffer;
				ts->analog_fc.function_page = page_num;
				break;
			case FLASH_MEMORY_MANAGEMENT:
				ts->flash_fc.dsc = buffer;
				ts->flash_fc.function_page = page_num;
			default:
				break;
			}
		}
	}

	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, 0x00) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	/* set interrupt mask */
	ts->interrupt_mask.flash = 0x1;
	ts->interrupt_mask.status = 0x2;
#ifdef CUST_G_TOUCH
	ts->interrupt_mask.abs = 0x4;
	ts->interrupt_mask.button = 0x20;
#endif

	if(ts->common_fc.dsc.id == 0 || ts->finger_fc.dsc.id == 0
			|| ts->analog_fc.dsc.id == 0 || ts->flash_fc.dsc.id == 0){
		TOUCH_ERR_MSG("common/finger/analog/flash are not initiailized\n");
		return -EPERM;
	}

	if (touch_debug_mask & DEBUG_BASE_INFO)
		TOUCH_INFO_MSG("common[%dP:0x%02x] finger[%dP:0x%02x] button[%dP:0x%02x] analog[%dP:0x%02x] flash[%dP:0x%02x]\n",
				ts->common_fc.function_page, ts->common_fc.dsc.id,
				ts->finger_fc.function_page, ts->finger_fc.dsc.id,
				ts->button_fc.function_page, ts->button_fc.dsc.id,
				ts->analog_fc.function_page, ts->analog_fc.dsc.id,
				ts->flash_fc.function_page, ts->flash_fc.dsc.id);

	return 0;
}

int get_ic_info(struct synaptics_ts_data* ts, struct touch_fw_info* fw_info)
{
#if defined(ARRAYED_TOUCH_FW_BIN)
	int cnt;
#endif

	u8 device_status = 0;
	u8 flash_control = 0;

	read_page_description_table(ts->client);

	memset(&ts->fw_info, 0, sizeof(struct synaptics_ts_fw_info));

	if (unlikely(touch_i2c_read(ts->client, FW_REVISION_REG,
			sizeof(ts->fw_info.fw_rev), &ts->fw_info.fw_rev) < 0)) {
		TOUCH_ERR_MSG("FW_REVISION_REG read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(ts->client, MANUFACTURER_ID_REG,
			sizeof(ts->fw_info.manufacturer_id), &ts->fw_info.manufacturer_id) < 0)) {
		TOUCH_ERR_MSG("MANUFACTURER_ID_REG read fail\n");
		return -EIO;
	}

	/* Product ID - G:TM2000, GJ:TM2372, GK:PLG124(LGIT G1F), PLG192(SUNTEL GFF) , PLG193(LGIT GFF), PLG207(LGIT GFF HYBRID), GV:PLG121(LGIT), PLG184(TPK) */
	if (unlikely(touch_i2c_read(ts->client, PRODUCT_ID_REG,
			sizeof(ts->fw_info.product_id) - 1, ts->fw_info.product_id) < 0)) {
		TOUCH_ERR_MSG("PRODUCT_ID_REG read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(ts->client, FLASH_CONFIG_ID_REG,
			sizeof(ts->fw_info.config_id) - 1, ts->fw_info.config_id) < 0)) {
		TOUCH_ERR_MSG("FLASH_CONFIG_ID_REG read fail\n");
		return -EIO;
	}

	snprintf(fw_info->ic_fw_identifier, sizeof(fw_info->ic_fw_identifier),
			"%s - %d", ts->fw_info.product_id, ts->fw_info.manufacturer_id);
	snprintf(fw_info->ic_fw_version, sizeof(fw_info->ic_fw_version),
			"%s", ts->fw_info.config_id);
	
#ifdef CUST_G_TOUCH
	if(!strncmp(ts->fw_info.product_id, "DS4 R3.0", 8)) {	//G OLD PANEL
		if(!strncmp(fw_info->ic_fw_version, "0000", 4) || !strncmp(fw_info->ic_fw_version, "S001", 4)) {
			ts->ic_panel_type = G_IC7020_GFF;
			TOUCH_INFO_MSG("IC is 7020, panel is GFF.");
		} else {
			if( fw_info->ic_fw_version[0] == 'E' && (int)simple_strtol(&fw_info->ic_fw_version[1], NULL, 10) < 14) {
				ts->ic_panel_type = G_IC7020_G2;
				TOUCH_INFO_MSG("IC is 7020, panel is G2.");
			} else if( (fw_info->ic_fw_version[0] == 'E' && 
				  	   (int)simple_strtol(&fw_info->ic_fw_version[1], NULL, 10) >= 14 && 
				       (int)simple_strtol(&fw_info->ic_fw_version[1], NULL, 10) < 27) || 
				       fw_info->ic_fw_version[0] == 'T') {
				ts->ic_panel_type = G_IC3203_G2;
				TOUCH_INFO_MSG("IC is 3203, panel is G2.");
			} else {
				ts->ic_panel_type = UNKNOWN;
				TOUCH_INFO_MSG("UNKNOWN OLD PANEL");
			}
		}
	} else if(!strncmp(ts->fw_info.product_id, "TM2000", 6)) {	//G PANEL LGIT
		ts->ic_panel_type = G_IC7020_G2_LGIT;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is G2. LGIT");

		if((fw_info->ic_fw_version[0] == 'E') && 
		   ((int)simple_strtol(&fw_info->ic_fw_version[1], NULL, 10) >= 40)) {
			ts->interrupt_mask.button = 0x10;
		}
	} else if(!strncmp(ts->fw_info.product_id, "TM2369", 6)) {	//G PANEL TPK
		ts->ic_panel_type = G_IC7020_G2_TPK;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is G2. TPK");
		ts->interrupt_mask.button = 0x10;
	} else if(!strncmp(ts->fw_info.product_id, "TM2372", 6)) {	//GJ PANEL
		ts->ic_panel_type = GJ_IC7020_GFF_H_PTN;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is GFF.");

		if((fw_info->ic_fw_version[0] == 'E') &&
		   ((int)simple_strtol(&fw_info->ic_fw_version[1], NULL, 10) >= 2)) {
			ts->interrupt_mask.button = 0x10;
		}
	} else if(!strncmp(ts->fw_info.product_id, "PLG124", 6)) {	//GK PANEL LGIT G1F
		ts->ic_panel_type = GK_IC7020_G1F;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is G1F.");
		ts->interrupt_mask.button = 0x10;
	} else if(!strncmp(ts->fw_info.product_id, "PLG192", 6)) {	//GK PANEL SUNTEL GFF
		ts->ic_panel_type = GK_IC7020_GFF_SUNTEL;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is GFF. SUNTEL");
		ts->interrupt_mask.button = 0x10;
	} else if(!strncmp(ts->fw_info.product_id, "PLG193", 6)) {	//GK PANEL LGIT GFF
		ts->ic_panel_type = GK_IC7020_GFF_LGIT;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is GFF. LGIT");
		ts->interrupt_mask.button = 0x10;
	} else if(!strncmp(ts->fw_info.product_id, "PLG207", 6)) {	//GK PANEL LGIT GFF HYBRID
		ts->ic_panel_type = GK_IC7020_GFF_LGIT_HYBRID;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is GFF. LGIT");
		ts->interrupt_mask.button = 0x10;
	} else if(!strncmp(ts->fw_info.product_id, "PLG121", 6)) {	//GV PANEL
		ts->ic_panel_type = GV_IC7020_G2_H_PTN_LGIT;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is G2. LGIT");
		ts->interrupt_mask.button = 0x10;
	} else if(!strncmp(ts->fw_info.product_id, "PLG184", 6)) {	//GV PANEL TPK
		ts->ic_panel_type = GV_IC7020_G2_H_PTN_TPK;
		TOUCH_INFO_MSG("IC is 7020, H pattern, panel is G2. TPK");
		ts->interrupt_mask.button = 0x10;
	} else if(!strncmp(ts->fw_info.product_id, "S7020", 5)) {	//S7020 PANEL (UNKNOWN PANEL)
		TOUCH_INFO_MSG("UNKNOWN PANEL, Product id is S7020.");
		ts->interrupt_mask.button = 0x10;
	} else {
		TOUCH_INFO_MSG("UNKNOWN PANEL");
	}
#endif


#if defined(ARRAYED_TOUCH_FW_BIN)
	for (cnt = 0; cnt < sizeof(SynaFirmware)/sizeof(SynaFirmware[0]); cnt++) {
		strncpy(ts->fw_info.fw_image_product_id, &SynaFirmware[cnt][16], 10);
		if (!(strncmp(ts->fw_info.product_id , ts->fw_info.fw_image_product_id, 10)))
			break;
	}
	strncpy(ts->fw_info.image_config_id, &SynaFirmware[cnt][0xb100],4);
	ts->fw_info.fw_start = (unsigned char *)&SynaFirmware[cnt][0];
	ts->fw_info.fw_size = sizeof(SynaFirmware[0]);
#else
#ifdef G_ONLY
			switch(ts->ic_panel_type){
				case G_IC7020_GFF:
				case G_IC7020_G2:
				case G_IC3203_G2:
				case G_IC7020_G2_LGIT:
					memcpy(&SynaFirmware[0], &SynaFirmware_TM2000[0], sizeof(SynaFirmware));
					break;
				case G_IC7020_G2_TPK:
					memcpy(&SynaFirmware[0], &SynaFirmware_TM2369[0], sizeof(SynaFirmware));
					break;
				default:
					TOUCH_ERR_MSG("UNKNOWN PANEL. SynaImage set error");
					break;
			}
#endif

#if defined(CUST_G_TOUCH) || defined(CONFIG_MACH_APQ8064_GKATT)
	switch(ts->ic_panel_type){
		case GK_IC7020_G1F:
			memcpy(&SynaFirmware[0], &SynaFirmware_PLG124[0], sizeof(SynaFirmware));
			break;
		case GK_IC7020_GFF_SUNTEL:
			memcpy(&SynaFirmware[0], &SynaFirmware_PLG192[0], sizeof(SynaFirmware));
			break;
		case GK_IC7020_GFF_LGIT:
			memcpy(&SynaFirmware[0], &SynaFirmware_PLG193[0], sizeof(SynaFirmware));
			break;
		case GK_IC7020_GFF_LGIT_HYBRID:
			memcpy(&SynaFirmware[0], &SynaFirmware_PLG207[0], sizeof(SynaFirmware));
			break;
		default:
			TOUCH_ERR_MSG("UNKNOWN PANEL(GK). SynaImage set error");
			break;
	}
#elif defined(CONFIG_MACH_APQ8064_GVDCM)|| defined(CONFIG_MACH_APQ8064_GV_KR)
	switch(ts->ic_panel_type){
		case GV_IC7020_G2_H_PTN_LGIT:
			memcpy(&SynaFirmware[0], &SynaFirmware_PLG121[0], sizeof(SynaFirmware));
			break;
		case GV_IC7020_G2_H_PTN_TPK:
			memcpy(&SynaFirmware[0], &SynaFirmware_PLG184[0], sizeof(SynaFirmware));
			break;
		default:
			TOUCH_ERR_MSG("UNKNOWN PANEL(GV). SynaImage set error");
			break;
	}
#endif

	strncpy(ts->fw_info.fw_image_product_id, &SynaFirmware[16], 10);
	strncpy(ts->fw_info.image_config_id, &SynaFirmware[0xb100],4);
#ifdef CUST_G_TOUCH
	strncpy(fw_info->syna_img_fw_version, &SynaFirmware[0xb100], 4);
	strncpy(fw_info->syna_img_fw_product_id, &SynaFirmware[0x0040], 6);

	strncpy(ts->fw_info.syna_img_product_id, &SynaFirmware[0x0040], 6);
	strncpy(ts->fw_info.syna_img_fw_ver, &SynaFirmware[0xb100],4);
#endif
	ts->fw_info.fw_start = (unsigned char *)&SynaFirmware[0];
	ts->fw_info.fw_size = sizeof(SynaFirmware);
#endif

	ts->fw_info.fw_image_rev = ts->fw_info.fw_start[31];

	if (unlikely(touch_i2c_read(ts->client, FLASH_CONTROL_REG, sizeof(flash_control), &flash_control) < 0)) {
		TOUCH_ERR_MSG("FLASH_CONTROL_REG read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(ts->client, DEVICE_STATUS_REG, sizeof(device_status), &device_status) < 0)) {
		TOUCH_ERR_MSG("DEVICE_STATUS_REG read fail\n");
		return -EIO;
	}

	/* Firmware has a problem, so we should firmware-upgrade */
	if(device_status & DEVICE_STATUS_FLASH_PROG
			|| (device_status & DEVICE_CRC_ERROR_MASK) != 0
			|| (flash_control & FLASH_STATUS_MASK) != 0) {
		TOUCH_ERR_MSG("Firmware has a unknown-problem, so it needs firmware-upgrade.\n");
		TOUCH_ERR_MSG("FLASH_CONTROL[%x] DEVICE_STATUS_REG[%x]\n", (u32)flash_control, (u32)device_status);
		TOUCH_ERR_MSG("FW-upgrade Force Rework.\n");

		/* firmware version info change by force for rework */
		ts->fw_info.fw_rev = 0;
		snprintf(ts->fw_info.config_id, sizeof(ts->fw_info.config_id), "ERR");
#ifdef CUST_G_TOUCH
		fw_info->fw_force_rework = true;
#endif
	}

	return 0;
}

int synaptics_ts_init(struct i2c_client* client, struct touch_fw_info* fw_info)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);

	u8 buf = 0;

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	if (!ts->is_probed)
		if (unlikely(get_ic_info(ts, fw_info) < 0))
			return -EIO;

#ifdef CUST_G_TOUCH
	if(ts_charger_plug==0){
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_NOSLEEP | DEVICE_CONTROL_CONFIGURED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
	} else if(ts_charger_plug==1){
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_NOSLEEP | DEVICE_CONTROL_CONFIGURED | DEVICE_CHARGER_CONNECTED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
#ifdef CUST_G_TOUCH_FREQ_HOPPING
		if (unlikely(synaptics_ts_page_data_read(client, 0x01, 0x04, 1, &buf) < 0)) {
			TOUCH_ERR_MSG("Current Hopping Index read fail\n");
			return -EIO;
		}

		if(buf == 3) cur_hopping_idx = 3;
		else cur_hopping_idx = 4;

		TOUCH_INFO_MSG("cur_hopping_idx [ %s ] = %x\n", __func__, buf);

		switch(ts_charger_type) {
			case 0:
			case 1:
				if(cns_en && cur_hopping_idx != 4){
					buf = 0x84;
					synaptics_ts_page_data_write(client, 0x01, 0x04, 1, &buf);
					cur_hopping_idx = 4;
					TOUCH_INFO_MSG("cur_hopping_idx [ %s ] = %x\n", __func__, buf);
				}
				break;
			default:
				break;
		}
#endif
	}

	if (unlikely(touch_i2c_read(client, DEVICE_CONTROL_REG,	1, &buf) < 0)) {
		TOUCH_ERR_MSG("DEVICE_CONTROL_REG read fail\n");
		return -EIO;
	}
	TOUCH_INFO_MSG("DEVICE CONTROL_REG = %x\n", buf);
#else
	if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
			DEVICE_CONTROL_NOSLEEP | DEVICE_CONTROL_CONFIGURED) < 0)) {
		TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
		return -EIO;
	}
#endif

	if (unlikely(touch_i2c_read(client, INTERRUPT_ENABLE_REG,
			1, &buf) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_ENABLE_REG read fail\n");
		return -EIO;
	}
	if (unlikely(touch_i2c_write_byte(client, INTERRUPT_ENABLE_REG,
			buf | ts->interrupt_mask.abs | ts->interrupt_mask.button) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_ENABLE_REG write fail\n");
		return -EIO;
	}

	if(ts->pdata->role->report_mode == CONTINUOUS_REPORT_MODE) {
#ifdef CUST_G_TOUCH
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				REPORT_BEYOND_CLIP | ABS_FILTER | REPORT_MODE_CONTINUOUS) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}
#else
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				REPORT_MODE_CONTINUOUS) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}
#endif
	} else {	/* REDUCED_REPORT_MODE */
#ifdef CUST_G_TOUCH
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				REPORT_BEYOND_CLIP | ABS_FILTER | REPORT_MODE_REDUCED) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}
#else
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				REPORT_MODE_REDUCED) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}
#endif

		if (unlikely(touch_i2c_write_byte(client, DELTA_X_THRESH_REG,
				ts->pdata->role->delta_pos_threshold) < 0)) {
			TOUCH_ERR_MSG("DELTA_X_THRESH_REG write fail\n");
			return -EIO;
		}
		if (unlikely(touch_i2c_write_byte(client, DELTA_Y_THRESH_REG,
				ts->pdata->role->delta_pos_threshold) < 0)) {
			TOUCH_ERR_MSG("DELTA_Y_THRESH_REG write fail\n");
			return -EIO;
		}
	}

#ifdef CUST_G_TOUCH
	if (unlikely(touch_i2c_read(client, SMALL_OBJECT_DETECTION_TUNNING_REG, 1, &buf) < 0)) {
		TOUCH_ERR_MSG("SMALL_OBJECT_DETECTION_TUNNING_REG read fail\n");
		return -EIO;
	}
	
	if(buf & SMALL_OBJECT_DETECTION) {
		TOUCH_INFO_MSG("Stylus Pen is Enabled\n");
		ts->pdata->role->pen_enable = 1;
	}
#endif

	if (unlikely(touch_i2c_read(client, INTERRUPT_STATUS_REG, 1, &buf) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
		return -EIO;	// it is critical problem because interrupt will not occur.
	}

	if (unlikely(touch_i2c_read(client, FINGER_STATE_REG, sizeof(ts->ts_data.finger.finger_status_reg),
			ts->ts_data.finger.finger_status_reg) < 0)) {
		TOUCH_ERR_MSG("FINGER_STATE_REG read fail\n");
		return -EIO;	// it is critical problem because interrupt will not occur on some FW.
	}

	ts->is_probed = 1;

	return 0;
}

int synaptics_ts_power(struct i2c_client* client, int power_ctrl)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	switch (power_ctrl) {
	case POWER_OFF:
		if (ts->pdata->pwr->use_regulator) {
			regulator_disable(ts->regulator_vio);
			regulator_disable(ts->regulator_vdd);
		}
		else
			ts->pdata->pwr->power(0);

#ifdef CUST_G_TOUCH
		if (ts->pdata->reset_pin > 0) {
			gpio_set_value(ts->pdata->reset_pin, 0);
		}
#endif
		break;
	case POWER_ON:
#ifdef CUST_G_TOUCH
		if (ts->pdata->reset_pin > 0) {
			gpio_set_value(ts->pdata->reset_pin, 1);
		}
#endif
		if (ts->pdata->pwr->use_regulator) {
			regulator_enable(ts->regulator_vdd);
			regulator_enable(ts->regulator_vio);
		}
		else
			ts->pdata->pwr->power(1);

#ifdef CUST_G_TOUCH
		if (ts->pdata->reset_pin > 0) {
			gpio_set_value(ts->pdata->reset_pin, 0);
			msleep(ts->pdata->role->reset_delay);
			gpio_set_value(ts->pdata->reset_pin, 1);
		}
#else
		/* P2 H/W bug fix */
		if (ts->pdata->reset_pin > 0) {
			msleep(10);
			gpio_set_value(ts->pdata->reset_pin, 0);
			msleep(ts->pdata->role->reset_delay);
			gpio_set_value(ts->pdata->reset_pin, 1);
		}
#endif

		break;
	case POWER_SLEEP:
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_SLEEP | DEVICE_CONTROL_CONFIGURED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
		break;
	case POWER_WAKE:
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_NORMAL_OP | DEVICE_CONTROL_CONFIGURED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
		break;
	default:
		return -EIO;
		break;
	}

	return 0;
}

int synaptics_ts_probe(struct i2c_client* client)
{
	struct synaptics_ts_data* ts;
	int ret = 0;

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	ts = kzalloc(sizeof(struct synaptics_ts_data), GFP_KERNEL);
	if (!ts) {
		TOUCH_ERR_MSG("Can not allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	set_touch_handle(client, ts);

	ts->client = client;
	ts->pdata = client->dev.platform_data;

	if (ts->pdata->pwr->use_regulator) {
		ts->regulator_vdd = regulator_get(&client->dev, ts->pdata->pwr->vdd);
		if (IS_ERR(ts->regulator_vdd)) {
			TOUCH_ERR_MSG("FAIL: regulator_get_vdd - %s\n", ts->pdata->pwr->vdd);
			ret = -EPERM;
			goto err_get_vdd_failed;
	}

		ts->regulator_vio = regulator_get(&client->dev, ts->pdata->pwr->vio);
		if (IS_ERR(ts->regulator_vio)) {
			TOUCH_ERR_MSG("FAIL: regulator_get_vio - %s\n", ts->pdata->pwr->vio);
			ret = -EPERM;
			goto err_get_vio_failed;
		}

		if (ts->pdata->pwr->vdd_voltage > 0) {
			ret = regulator_set_voltage(ts->regulator_vdd, ts->pdata->pwr->vdd_voltage, ts->pdata->pwr->vdd_voltage);
			if (ret < 0)
				TOUCH_ERR_MSG("FAIL: VDD voltage setting - (%duV)\n", ts->pdata->pwr->vdd_voltage);
	}
		/* // G2 : vio is not adjustable regulator
		if (ts->pdata->pwr->vio_voltage > 0) {
			ret = regulator_set_voltage(ts->regulator_vio, ts->pdata->pwr->vio_voltage, ts->pdata->pwr->vio_voltage);
			if (ret < 0)
				TOUCH_ERR_MSG("FAIL: VIO voltage setting - (%duV)\n",ts->pdata->pwr->vio_voltage);
		}
		*/
	}

	return ret;

err_get_vio_failed:
	if (ts->pdata->pwr->use_regulator) {
		regulator_put(ts->regulator_vdd);
	}
err_get_vdd_failed:
err_alloc_data_failed:
	kfree(ts);
	return ret;
}

#ifdef CUST_G_TOUCH
int synaptics_ts_resolution(struct i2c_client* client) {
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);

	u8 resolution[2] = {0};

	if(ts->pdata->role->key_type == TOUCH_HARD_KEY) {
		if (unlikely(touch_i2c_read(ts->client, SENSOR_MAX_X_POS, sizeof(resolution), resolution) < 0)) {
			TOUCH_ERR_MSG("SENSOR_MAX_X read fail\n");
			return -EIO;	// it is critical problem because interrupt will not occur.
		}
		TOUCH_INFO_MSG("SENSOR_MAX_X=%d", (int)(resolution[1] << 8 | resolution[0]));
		ts->pdata->caps->x_max = (int)(resolution[1] << 8 | resolution[0]);

		if (unlikely(touch_i2c_read(ts->client, SENSOR_MAX_Y_POS, sizeof(resolution), resolution) < 0)) {
			TOUCH_ERR_MSG("SENSOR_MAX_Y read fail\n");
			return -EIO;	// it is critical problem because interrupt will not occur.
		}
		TOUCH_INFO_MSG("SENSOR_MAX_Y=%d", (int)(resolution[1] << 8 | resolution[0]));
		ts->pdata->caps->y_max = (int)(resolution[1] << 8 | resolution[0]);
	}

	return 0;

}
#endif
void synaptics_ts_remove(struct i2c_client* client)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	if (ts->pdata->pwr->use_regulator) {
		regulator_put(ts->regulator_vio);
		regulator_put(ts->regulator_vdd);
	}

	kfree(ts);
}

int synaptics_ts_fw_upgrade(struct i2c_client* client, struct touch_fw_info* fw_info)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	int ret = 0;

	ts->is_probed = 0;
#if !defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_VU3EVB)
	ret = FirmwareUpgrade(ts, fw_info->fw_upgrade.fw_path);

	/* update IC info */
	if (ret >= 0)
		get_ic_info(ts, fw_info);
#endif
	return ret;
}

#ifdef CUST_G_TOUCH
int synaptics_ts_ic_ctrl(struct i2c_client *client, u8 code, u32 value)
#else
int synaptics_ts_ic_ctrl(struct i2c_client *client, u8 code, u16 value)
#endif
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	u8 buf = 0;

	switch (code)
	{
	case IC_CTRL_BASELINE:
		switch (value)
		{
		case BASELINE_OPEN:
#ifdef CUST_G_TOUCH
			break;
#endif
			if (unlikely(synaptics_ts_page_data_write_byte(client, ANALOG_PAGE,
					ANALOG_CONTROL_REG, FORCE_FAST_RELAXATION) < 0)) {
				TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
				return -EIO;
			}

			msleep(10);

			if (unlikely(synaptics_ts_page_data_write_byte(client, ANALOG_PAGE,
					ANALOG_COMMAND_REG, FORCE_UPDATE) < 0)) {
				TOUCH_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
				return -EIO;
			}

			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("BASELINE_OPEN\n");

			break;
		case BASELINE_FIX:
#ifdef CUST_G_TOUCH
			break;
#endif
			if (unlikely(synaptics_ts_page_data_write_byte(client, ANALOG_PAGE,
					ANALOG_CONTROL_REG, 0x00) < 0)) {
				TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
				return -EIO;
			}

			msleep(10);

			if (unlikely(synaptics_ts_page_data_write_byte(client, ANALOG_PAGE,
					ANALOG_COMMAND_REG, FORCE_UPDATE) < 0)) {
				TOUCH_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
				return -EIO;
			}

			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("BASELINE_FIX\n");

			break;
		case BASELINE_REBASE:
			/* rebase base line */
			if (likely(ts->finger_fc.dsc.id != 0)) {
				if (unlikely(touch_i2c_write_byte(client, FINGER_COMMAND_REG, 0x1) < 0)) {
					TOUCH_ERR_MSG("finger baseline reset command write fail\n");
					return -EIO;
				}
			}
			break;
		default:
			break;
		}
		break;
	case IC_CTRL_READ:
#ifdef CUST_G_TOUCH
		if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, ((value & 0xFF00) >> 8)) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}

		if (touch_i2c_read(client, (value & 0xFF), 1, &buf) < 0) {
			TOUCH_ERR_MSG("IC register read fail\n");
			return -EIO;
		}

		if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, 0x00) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}
#else
		if (touch_i2c_read(client, value, 1, &buf) < 0) {
			TOUCH_ERR_MSG("IC register read fail\n");
			return -EIO;
		}
#endif
		break;
	case IC_CTRL_WRITE:
#ifdef CUST_G_TOUCH
		if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, ((value & 0xFF0000) >> 16)) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}

		if (touch_i2c_write_byte(client, ((value & 0xFF00) >> 8), (value & 0xFF)) < 0) {
			TOUCH_ERR_MSG("IC register write fail\n");
			return -EIO;
		}

		if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, 0x00) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}
#else
		if (touch_i2c_write_byte(client, ((value & 0xFF00) >> 8), (value & 0xFF)) < 0) {
			TOUCH_ERR_MSG("IC register write fail\n");
			return -EIO;
		}
#endif
		break;
	case IC_CTRL_RESET_CMD:
		if (unlikely(touch_i2c_write_byte(client, DEVICE_COMMAND_REG, 0x1) < 0)) {
			TOUCH_ERR_MSG("IC Reset command write fail\n");
			return -EIO;
		}
		break;

	case IC_CTRL_REPORT_MODE:

		switch (value)
		{
			case 0:   // continuous mode
#ifdef CUST_G_TOUCH
			if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
					REPORT_BEYOND_CLIP | ABS_FILTER | REPORT_MODE_CONTINUOUS) < 0)) {
				TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
				return -EIO;
			}
#else
			if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
					REPORT_MODE_CONTINUOUS) < 0)) {
				TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
				return -EIO;
			}
#endif
				break;
			case 1:  // reduced mode
#ifdef CUST_G_TOUCH
			if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
					REPORT_BEYOND_CLIP | ABS_FILTER | REPORT_MODE_REDUCED) < 0)) {
				TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
				return -EIO;
			}
#else
			if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
					REPORT_MODE_REDUCED) < 0)) {
				TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
				return -EIO;
			}
#endif
			default:
				break;
		}
		break;

	default:
		break;
	}

	return buf;
}

struct touch_device_driver synaptics_ts_driver = {
	.probe 	= synaptics_ts_probe,
#ifdef CUST_G_TOUCH
	.resolution = synaptics_ts_resolution,
#endif
	.remove	= synaptics_ts_remove,
	.init  	= synaptics_ts_init,
	.data  	= synaptics_ts_get_data,
	.power 	= synaptics_ts_power,
	.fw_upgrade = synaptics_ts_fw_upgrade,
	.ic_ctrl	= synaptics_ts_ic_ctrl,
};

static int __devinit touch_init(void)
{
	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");
	printk("[Touch] synaptics ts init sdk \n");
	return touch_driver_register(&synaptics_ts_driver);
}

static void __exit touch_exit(void)
{
	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	touch_driver_unregister();
}

module_init(touch_init);
module_exit(touch_exit);

MODULE_AUTHOR("yehan.ahn@lge.com, hyesung.shin@lge.com");
MODULE_DESCRIPTION("LGE Touch Driver");
MODULE_LICENSE("GPL");

