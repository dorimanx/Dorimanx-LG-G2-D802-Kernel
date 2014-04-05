/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_isr.c

	Description : fc8080 interrupt service routine source file

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA


	History :
	----------------------------------------------------------------------
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fci_hal.h"
#include "../inc/fc8080_regs.h"
#include "../inc/fc8080_isr.h"

static fci_u8 fic_buffer[768];
static fci_u8 msc_buffer[8192];

fci_s32 (*fic_callback)(fci_u32 userdata, fci_u8 *data, fci_s32 length) = NULL;
fci_s32 (*msc_callback)(fci_u32 userdata, fci_u8 subch_id, fci_u8 *data, fci_s32 length) = NULL;

fci_u32 fic_user_data;
fci_u32 msc_user_data;
extern fci_u32 	tp_total_cnt;

static void fc8080_data(HANDLE handle, fci_u16 status)
{
	fci_u16 size;
	fci_s32 i;

	if (status & 0x0100) {
		bbm_data(handle, BBM_RD_FIC, &fic_buffer[0], FIC_BUF_LENGTH/2);

		if (fic_callback)
			(*fic_callback)(fic_user_data, &fic_buffer[0], FIC_BUF_LENGTH/2);
	}

	for (i = 0; i < 3; i++) {
		fci_u8 subch_id;

		if (!(status & (1 << i)))
			continue;

		bbm_word_read(handle, BBM_BUF_CH0_THR + i * 2, &size);
		size++;

		bbm_read(handle, BBM_BUF_CH0_SUBID + i, &subch_id);
		subch_id &= 0x3f;

		{
			fci_u8 rsSubChId;

			bbm_read(handle, BBM_MSC_CFG_SCH0, &rsSubChId);
			rsSubChId &= 0x3F;

			if(rsSubChId == subch_id)
				tp_total_cnt += size/188;
		}

		bbm_data(handle, (BBM_RD_BUF0 + i), &msc_buffer[0], size);

		if(size > 384)
		{
			if (msc_callback)
				(*msc_callback)(msc_user_data, subch_id, &msc_buffer[4],size);
		}
		else
		{
			if (msc_callback)
				(*msc_callback)(msc_user_data, subch_id, &msc_buffer[0],size);
		}


	}
}

void fc8080_isr(HANDLE handle)
{
	fci_u16 buf_int_status = 0;

	bbm_word_read(handle, BBM_BUF_STATUS, &buf_int_status);

	if (buf_int_status) {
		bbm_word_write(handle, BBM_BUF_STATUS, buf_int_status);
		fc8080_data(handle, buf_int_status);
	}

	#if 0
	buf_int_status = 0;
	bbm_word_read(handle, BBM_BUF_STATUS, &buf_int_status);

	if (buf_int_status) {
		bbm_word_write(handle, BBM_BUF_STATUS, buf_int_status);
		fc8080_data(handle, buf_int_status);
	}
	#endif
}
