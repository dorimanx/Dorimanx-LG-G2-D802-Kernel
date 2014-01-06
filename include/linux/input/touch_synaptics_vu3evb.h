/* include/linux/lge_touch_core.h
 *
 * Copyright (C) 2012 LGE.
 *
 * Writer: yehan.ahn@lge.com, 	hyesung.shin@lge.com
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

#ifndef LGE_TOUCH_SYNAPTICS_H
#define LGE_TOUCH_SYNAPTICS_H

//#define ARRAYED_TOUCH_FW_BIN

#define NUM_OF_EACH_FINGER_DATA_REG		5
#define MAX_NUM_OF_FINGERS				10

#define DESCRIPTION_TABLE_START			0xe9

#define PAGE_SELECT_REG					0xFF		/* Button exists Page 02 */
#define PAGE_MAX_NUM					4			/* number of page register */

#ifndef CUST_G_TOUCH
#define CUST_G_TOUCH
#endif

struct function_descriptor {
	u8 	query_base;
	u8 	command_base;
	u8 	control_base;
	u8 	data_base;
	u8 	int_source_count;
	u8 	id;
};

struct ts_ic_function {
	struct function_descriptor dsc;
	u8 	function_page;
};

struct finger_data {
	u8	finger_status_reg[3];
	u8	finger_reg[MAX_NUM_OF_FINGERS][NUM_OF_EACH_FINGER_DATA_REG];
};

struct button_data {
	u16	key_code;
};

struct cur_touch_data {
	u8	device_status_reg;		/* DEVICE_STATUS_REG */
	u8	interrupt_status_reg;
	u8	button_data_reg;
	struct finger_data	finger;
	struct button_data	button;
};

struct synaptics_ts_fw_info
{
	u8		fw_rev;
	u8		fw_image_rev;
	u8		manufacturer_id;
	u8		product_id[11];
#ifdef CUST_G_TOUCH
	u8		syna_img_product_id[11];
	u8		syna_img_fw_ver[5];
#endif
	u8		fw_image_product_id[11];
	u8		config_id[5];
	u8		image_config_id[5];
	unsigned char	*fw_start;
	unsigned long	fw_size;
};
#ifdef CUST_G_TOUCH
struct interrupt_bit_mask {
	u8 flash;
	u8 status;
	u8 abs;
	u8 button;
};
#endif

struct synaptics_ts_data {
	u8	is_probed;
	struct regulator*	regulator_vdd;
	struct regulator*	regulator_vio;
	struct i2c_client*	client;
	struct touch_platform_data*		pdata;
	struct ts_ic_function	common_fc;
	struct ts_ic_function	finger_fc;
	struct ts_ic_function	button_fc;
	struct ts_ic_function	analog_fc;	/* FIXME: not used in ClearPad3000 serise */
	struct ts_ic_function	flash_fc;
	struct cur_touch_data	ts_data;
	struct synaptics_ts_fw_info	fw_info;
#ifdef CUST_G_TOUCH
	struct interrupt_bit_mask	interrupt_mask;
	u8	ic_panel_type;
#endif
};

#ifdef CUST_G_TOUCH
enum {
	UNKNOWN,
	G_IC7020_GFF, 
	G_IC7020_G2, 
	G_IC3203_G2, 
	G_IC7020_G2_LGIT,
	G_IC7020_G2_TPK,
	GJ_IC7020_GFF_H_PTN,
	GV_IC7020_G2_H_PTN_LGIT,
	GV_IC7020_G2_H_PTN_TPK,
	GK_IC7020_G1F,
	GK_IC7020_GFF_SUNTEL,
	GK_IC7020_GFF_LGIT,
	GK_IC7020_GFF_LGIT_HYBRID,
};
#endif

/* extern function */
extern int FirmwareUpgrade(struct synaptics_ts_data *ts, const char* fw_path);
int synaptics_ts_page_data_read(struct i2c_client *client, u8 page, u8 reg, int size, u8 *data);
int synaptics_ts_page_data_write(struct i2c_client *client, u8 page, u8 reg, int size, u8 *data);
int synaptics_ts_page_data_write_byte(struct i2c_client *client, u8 page, u8 reg, u8 data);

#endif
