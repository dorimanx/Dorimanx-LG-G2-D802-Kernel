/**===================================================================
 * Copyright(c) 2009 LG Electronics Inc. All Rights Reserved
 *
 * File Name : broadcast_fc8080.h
 * Description : EDIT HISTORY FOR MODULE
 * This section contains comments describing changes made to the module.
 * Notice that changes are listed in reverse chronological order.
 *
 * when			model		who			what
 * 10.27.2009		android		inb612		Create for Android platform
====================================================================**/
#ifndef _BROADCAST_FC8080_H_
#define _BROADCAST_FC8080_H_
#include "../../broadcast_tdmb_typedef.h"

typedef struct
{
	void		(*tdmb_pwr_on)(void);
	void		(*tdmb_pwr_off)(void);
}broadcast_pwr_func;

struct broadcast_tdmb_data
{
	void (*pwr_on)(void);
	void (*pwr_off)(void);
};


int tdmb_fc8080_power_on(void);
int tdmb_fc8080_power_off(void);
int tdmb_fc8080_select_antenna(unsigned int sel);
int tdmb_fc8080_i2c_write_burst(uint16 waddr, uint8* wdata, int length);
int tdmb_fc8080_i2c_read_burst(uint16 raddr, uint8* rdata, int length);
int tdmb_fc8080_mdelay(int32 ms);
void tdmb_fc8080_Must_mdelay(int32 ms);
void tdmb_fc8080_interrupt_lock(void);
void tdmb_fc8080_interrupt_free(void);
int tdmb_fc8080_spi_write_read(uint8* tx_data, int tx_length, uint8 *rx_data, int rx_length);
void tdmb_fc8080_set_userstop(int mode);
int tdmb_fc8080_tdmb_is_on(void);
#endif

