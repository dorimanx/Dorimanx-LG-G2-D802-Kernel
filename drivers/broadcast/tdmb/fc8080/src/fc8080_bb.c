/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_bb.c

	Description : baseband source file

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
#include "../inc/fci_oal.h"
#include "../inc/fci_hal.h"
#include "../inc/fci_tun.h"
#include "../inc/fc8080_regs.h"
#include <linux/kernel.h>
#define POWER_SAVE_MODE
#undef FEATURE_DEBUG_LOG

#define LOCK_TIME_TICK  5	/* 5ms */
#define SLOCK_MAX_TIME  200
#define FLOCK_MAX_TIME  300
#define DLOCK_MAX_TIME  500

static fci_s32 fc8080_set_xtal(HANDLE handle)
{
#if (FC8080_FREQ_XTAL == 24576)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x04000000);
	bbm_byte_write(handle, BBM_NCO_INV, 0x80);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x80);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x02);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x02);
	bbm_byte_write(handle, BBM_COEF01, 0x0f);
	bbm_byte_write(handle, BBM_COEF02, 0x0d);
	bbm_byte_write(handle, BBM_COEF03, 0x00);
	bbm_byte_write(handle, BBM_COEF04, 0x04);
	bbm_byte_write(handle, BBM_COEF05, 0x03);
	bbm_byte_write(handle, BBM_COEF06, 0x1c);
	bbm_byte_write(handle, BBM_COEF07, 0x19);
	bbm_byte_write(handle, BBM_COEF08, 0x02);
	bbm_byte_write(handle, BBM_COEF09, 0x0c);
	bbm_byte_write(handle, BBM_COEF0A, 0x04);
	bbm_byte_write(handle, BBM_COEF0B, 0x30);
	bbm_byte_write(handle, BBM_COEF0C, 0xed);
	bbm_byte_write(handle, BBM_COEF0D, 0x13);
	bbm_byte_write(handle, BBM_COEF0E, 0x4f);
	bbm_byte_write(handle, BBM_COEF0F, 0x6b);
#elif (FC8080_FREQ_XTAL == 16384)
	/* clock mode */
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x04000000);
	bbm_byte_write(handle, BBM_NCO_INV, 0x80);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x80);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x00);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x02);
	bbm_byte_write(handle, BBM_COEF01, 0x0f);
	bbm_byte_write(handle, BBM_COEF02, 0x0d);
	bbm_byte_write(handle, BBM_COEF03, 0x00);
	bbm_byte_write(handle, BBM_COEF04, 0x04);
	bbm_byte_write(handle, BBM_COEF05, 0x03);
	bbm_byte_write(handle, BBM_COEF06, 0x1c);
	bbm_byte_write(handle, BBM_COEF07, 0x19);
	bbm_byte_write(handle, BBM_COEF08, 0x02);
	bbm_byte_write(handle, BBM_COEF09, 0x0c);
	bbm_byte_write(handle, BBM_COEF0A, 0x04);
	bbm_byte_write(handle, BBM_COEF0B, 0x30);
	bbm_byte_write(handle, BBM_COEF0C, 0xed);
	bbm_byte_write(handle, BBM_COEF0D, 0x13);
	bbm_byte_write(handle, BBM_COEF0E, 0x4f);
	bbm_byte_write(handle, BBM_COEF0F, 0x6b);
#elif (FC8080_FREQ_XTAL == 19200)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x0369d037);
	bbm_byte_write(handle, BBM_NCO_INV, 0x96);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x6d);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x00);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x0e);
	bbm_byte_write(handle, BBM_COEF01, 0x00);
	bbm_byte_write(handle, BBM_COEF02, 0x03);
	bbm_byte_write(handle, BBM_COEF03, 0x03);
	bbm_byte_write(handle, BBM_COEF04, 0x1f);
	bbm_byte_write(handle, BBM_COEF05, 0x1a);
	bbm_byte_write(handle, BBM_COEF06, 0x1b);
	bbm_byte_write(handle, BBM_COEF07, 0x03);
	bbm_byte_write(handle, BBM_COEF08, 0x0a);
	bbm_byte_write(handle, BBM_COEF09, 0x05);
	bbm_byte_write(handle, BBM_COEF0A, 0x37);
	bbm_byte_write(handle, BBM_COEF0B, 0x2d);
	bbm_byte_write(handle, BBM_COEF0C, 0xfa);
	bbm_byte_write(handle, BBM_COEF0D, 0x1f);
	bbm_byte_write(handle, BBM_COEF0E, 0x49);
	bbm_byte_write(handle, BBM_COEF0F, 0x5c);
#elif (FC8080_FREQ_XTAL == 24000)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x02bb0cf8);
	bbm_byte_write(handle, BBM_NCO_INV, 0xbc);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x57);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x00);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x02);
	bbm_byte_write(handle, BBM_COEF01, 0x02);
	bbm_byte_write(handle, BBM_COEF02, 0x00);
	bbm_byte_write(handle, BBM_COEF03, 0x0c);
	bbm_byte_write(handle, BBM_COEF04, 0x1c);
	bbm_byte_write(handle, BBM_COEF05, 0x1f);
	bbm_byte_write(handle, BBM_COEF06, 0x05);
	bbm_byte_write(handle, BBM_COEF07, 0x08);
	bbm_byte_write(handle, BBM_COEF08, 0x04);
	bbm_byte_write(handle, BBM_COEF09, 0x3a);
	bbm_byte_write(handle, BBM_COEF0A, 0x31);
	bbm_byte_write(handle, BBM_COEF0B, 0x34);
	bbm_byte_write(handle, BBM_COEF0C, 0x07);
	bbm_byte_write(handle, BBM_COEF0D, 0x26);
	bbm_byte_write(handle, BBM_COEF0E, 0x42);
	bbm_byte_write(handle, BBM_COEF0F, 0x4e);
#elif (FC8080_FREQ_XTAL == 26000)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x03c7ea98);
	bbm_byte_write(handle, BBM_NCO_INV, 0x87);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x79);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x02);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x0f);
	bbm_byte_write(handle, BBM_COEF01, 0x0d);
	bbm_byte_write(handle, BBM_COEF02, 0x0f);
	bbm_byte_write(handle, BBM_COEF03, 0x03);
	bbm_byte_write(handle, BBM_COEF04, 0x04);
	bbm_byte_write(handle, BBM_COEF05, 0x1f);
	bbm_byte_write(handle, BBM_COEF06, 0x19);
	bbm_byte_write(handle, BBM_COEF07, 0x1c);
	bbm_byte_write(handle, BBM_COEF08, 0x07);
	bbm_byte_write(handle, BBM_COEF09, 0x0b);
	bbm_byte_write(handle, BBM_COEF0A, 0x3f);
	bbm_byte_write(handle, BBM_COEF0B, 0x2d);
	bbm_byte_write(handle, BBM_COEF0C, 0xf2);
	bbm_byte_write(handle, BBM_COEF0D, 0x19);
	bbm_byte_write(handle, BBM_COEF0E, 0x4d);
	bbm_byte_write(handle, BBM_COEF0F, 0x65);
#elif (FC8080_FREQ_XTAL == 27000)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x03a4114b);
	bbm_byte_write(handle, BBM_NCO_INV, 0x8d);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x75);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x02);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x0e);
	bbm_byte_write(handle, BBM_COEF01, 0x0e);
	bbm_byte_write(handle, BBM_COEF02, 0x01);
	bbm_byte_write(handle, BBM_COEF03, 0x04);
	bbm_byte_write(handle, BBM_COEF04, 0x03);
	bbm_byte_write(handle, BBM_COEF05, 0x1d);
	bbm_byte_write(handle, BBM_COEF06, 0x19);
	bbm_byte_write(handle, BBM_COEF07, 0x1f);
	bbm_byte_write(handle, BBM_COEF08, 0x09);
	bbm_byte_write(handle, BBM_COEF09, 0x09);
	bbm_byte_write(handle, BBM_COEF0A, 0x3b);
	bbm_byte_write(handle, BBM_COEF0B, 0x2d);
	bbm_byte_write(handle, BBM_COEF0C, 0xf5);
	bbm_byte_write(handle, BBM_COEF0D, 0x1c);
	bbm_byte_write(handle, BBM_COEF0E, 0x4b);
	bbm_byte_write(handle, BBM_COEF0F, 0x61);
#elif (FC8080_FREQ_XTAL == 27120)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x039ff180);
	bbm_byte_write(handle, BBM_NCO_INV, 0x8d);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x74);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x02);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x0e);
	bbm_byte_write(handle, BBM_COEF01, 0x0e);
	bbm_byte_write(handle, BBM_COEF02, 0x01);
	bbm_byte_write(handle, BBM_COEF03, 0x04);
	bbm_byte_write(handle, BBM_COEF04, 0x03);
	bbm_byte_write(handle, BBM_COEF05, 0x1d);
	bbm_byte_write(handle, BBM_COEF06, 0x19);
	bbm_byte_write(handle, BBM_COEF07, 0x1f);
	bbm_byte_write(handle, BBM_COEF08, 0x09);
	bbm_byte_write(handle, BBM_COEF09, 0x09);
	bbm_byte_write(handle, BBM_COEF0A, 0x3b);
	bbm_byte_write(handle, BBM_COEF0B, 0x2d);
	bbm_byte_write(handle, BBM_COEF0C, 0xf5);
	bbm_byte_write(handle, BBM_COEF0D, 0x1c);
	bbm_byte_write(handle, BBM_COEF0E, 0x4b);
	bbm_byte_write(handle, BBM_COEF0F, 0x61);
#elif (FC8080_FREQ_XTAL == 32000)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x03126eb8);
	bbm_byte_write(handle, BBM_NCO_INV, 0xa7);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x62);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x02);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x0e);
	bbm_byte_write(handle, BBM_COEF01, 0x0e);
	bbm_byte_write(handle, BBM_COEF02, 0x01);
	bbm_byte_write(handle, BBM_COEF03, 0x04);
	bbm_byte_write(handle, BBM_COEF04, 0x03);
	bbm_byte_write(handle, BBM_COEF05, 0x1d);
	bbm_byte_write(handle, BBM_COEF06, 0x19);
	bbm_byte_write(handle, BBM_COEF07, 0x1f);
	bbm_byte_write(handle, BBM_COEF08, 0x09);
	bbm_byte_write(handle, BBM_COEF09, 0x09);
	bbm_byte_write(handle, BBM_COEF0A, 0x3b);
	bbm_byte_write(handle, BBM_COEF0B, 0x3d);
	bbm_byte_write(handle, BBM_COEF0C, 0xf5);
	bbm_byte_write(handle, BBM_COEF0D, 0x1c);
	bbm_byte_write(handle, BBM_COEF0E, 0x4b);
	bbm_byte_write(handle, BBM_COEF0F, 0x61);
#elif (FC8080_FREQ_XTAL == 38400)
	bbm_long_write(handle, BBM_NCO_OFFSET, 0x0369d037);
	bbm_byte_write(handle, BBM_NCO_INV, 0x96);
	bbm_byte_write(handle, BBM_EZ_CONST, 0x6d);
	bbm_byte_write(handle, BBM_CLK_MODE, 0x01);

	/* filter coefficient */
	bbm_byte_write(handle, BBM_COEF00, 0x0e);
	bbm_byte_write(handle, BBM_COEF01, 0x00);
	bbm_byte_write(handle, BBM_COEF02, 0x03);
	bbm_byte_write(handle, BBM_COEF03, 0x03);
	bbm_byte_write(handle, BBM_COEF04, 0x1f);
	bbm_byte_write(handle, BBM_COEF05, 0x1a);
	bbm_byte_write(handle, BBM_COEF06, 0x1b);
	bbm_byte_write(handle, BBM_COEF07, 0x03);
	bbm_byte_write(handle, BBM_COEF08, 0x0a);
	bbm_byte_write(handle, BBM_COEF09, 0x05);
	bbm_byte_write(handle, BBM_COEF0A, 0x37);
	bbm_byte_write(handle, BBM_COEF0B, 0x2d);
	bbm_byte_write(handle, BBM_COEF0C, 0xfa);
	bbm_byte_write(handle, BBM_COEF0D, 0x1f);
	bbm_byte_write(handle, BBM_COEF0E, 0x49);
	bbm_byte_write(handle, BBM_COEF0F, 0x5c);
#endif
	return BBM_OK;
}

fci_s32 fc8080_reset(HANDLE handle)
{
	bbm_write(handle, BBM_MD_RESET, 0xfe);
	ms_must_wait(1);
	bbm_write(handle, BBM_MD_RESET, 0xff);

	return BBM_OK;
}

fci_s32 fc8080_probe(HANDLE handle)
{
	fci_u16 ver;
	bbm_word_read(handle, BBM_CHIP_ID, &ver);
//#ifdef FEATURE_DEBUG_LOG
	printk("fc8080_probe 0x%x\n", ver);
//#endif

	return (ver == 0x8080) ? BBM_OK : BBM_NOK;
}

fci_s32 fc8080_init(HANDLE handle)
{
	fc8080_reset(handle);
	fc8080_set_xtal(handle);

	bbm_write(handle, BBM_LDO_VCTRL, 0x33);
	bbm_write(handle, BBM_XTAL_CCTRL, 0x14);
	bbm_write(handle, BBM_RF_XTAL_EN, 0x0f);
	bbm_write(handle, BBM_ADC_OPMODE, 0x67);

	/*bbm_write(handle, BBM_FIC_CFG_CRC16, 0x03);*/
	bbm_word_write(handle, BBM_OFDM_DET_MAX_THRESHOLD, 0x0a00);
	bbm_write(handle, BBM_FTOFFSET_RANGE, 0x20);
	bbm_write(handle, BBM_AGC530_EN, 0x53);
	bbm_write(handle, BBM_BLOCK_AVG_SIZE_LOCK, 0x49);
	bbm_word_write(handle, BBM_GAIN_CONSTANT, 0x0303);
	bbm_write(handle, BBM_DET_CNT_BOUND, 0x60);
	bbm_write(handle, BBM_UNLOCK_DETECT_EN, 0x00);

	bbm_write(handle, BBM_DCE_CTRL, 0x27);
	bbm_write(handle, BBM_PGA_GAIN_MAX, 0x18);
	bbm_write(handle, BBM_PGA_GAIN_MIN, 0xe8);

#if (FC8080_FREQ_XTAL == 24000)
	bbm_write(handle, BBM_SYNC_MTH, 0x43);
#else
	bbm_write(handle, BBM_SYNC_MTH, 0xc3);
#endif

#if (FC8080_FREQ_XTAL == 16384) || (FC8080_FREQ_XTAL == 24576)
	bbm_write(handle, BBM_SFSYNC_ON, 0x00);
#else
	bbm_write(handle, BBM_SFSYNC_ON, 0x01);
#endif

	bbm_write(handle, BBM_RESYNC_EN, 0x01);
	bbm_write(handle, BBM_RESYNC_AUTO_CONDITION_EN, 0x00);
	bbm_write(handle, BBM_MSC_CFG_SPD, 0xff);

#if defined(POWER_SAVE_MODE)
	bbm_write(handle, BBM_PS0_RF_ENABLE, 0x06);
	bbm_write(handle, BBM_PS1_ADC_ENABLE, 0x07);
	bbm_write(handle, BBM_PS2_BB_ENABLE, 0x07);
	bbm_write(handle, BBM_PS2_BB_ADD_SHIFT, 0x21);
#else
	bbm_write(handle, BBM_PS0_RF_ENABLE, 0x04);
	bbm_write(handle, BBM_PS1_ADC_ENABLE, 0x05);
	bbm_write(handle, BBM_PS2_BB_ENABLE, 0x05);
#endif
	bbm_write(handle, BBM_PS1_ADC_ADD_SHIFT, 0x71);

	bbm_write(handle, BBM_DM_CTRL, 0xa0);
	bbm_write(handle, BBM_OVERRUN_GAP, 0x06);

	bbm_word_write(handle, BBM_BUF_FIC_START, FIC_BUF_START);
	bbm_word_write(handle, BBM_BUF_FIC_END,   FIC_BUF_END);
	bbm_word_write(handle, BBM_BUF_FIC_THR,   FIC_BUF_THR);
	bbm_word_write(handle, BBM_BUF_CH0_START, CH0_BUF_START);
	bbm_word_write(handle, BBM_BUF_CH0_END,   CH0_BUF_END);
	bbm_word_write(handle, BBM_BUF_CH0_THR,   CH0_BUF_THR);
	bbm_word_write(handle, BBM_BUF_CH1_START, CH1_BUF_START);
	bbm_word_write(handle, BBM_BUF_CH1_END,   CH1_BUF_END);
	bbm_word_write(handle, BBM_BUF_CH1_THR,   CH1_BUF_THR);
	bbm_word_write(handle, BBM_BUF_CH2_START, CH2_BUF_START);
	bbm_word_write(handle, BBM_BUF_CH2_END,   CH2_BUF_END);
	bbm_word_write(handle, BBM_BUF_CH2_THR,   CH2_BUF_THR);

	bbm_word_write(handle, BBM_BUF_INT, 0x0107);
	bbm_word_write(handle, BBM_BUF_ENABLE, 0x0100);

#ifdef FC8080_I2C
	bbm_write(handle, BBM_TSO_SELREG, 0xc0);
#else
	bbm_write(handle, BBM_MD_INT_EN, BBM_MF_INT);
	bbm_write(handle, BBM_MD_INT_STATUS, BBM_MF_INT);
#endif

	return BBM_OK;
}

fci_s32 fc8080_deinit(HANDLE handle)
{
	bbm_write(handle, BBM_MD_RESET, 0x00);

	return BBM_OK;
}

fci_s32 fc8080_channel_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_u8 buf_en = 0;

	bbm_read(handle, BBM_BUF_ENABLE, &buf_en);
	bbm_write(handle, BBM_BUF_ENABLE, buf_en | (1 << buf_id));

	bbm_write(handle, BBM_SCH0_SET_IDI + buf_id, 0x40 | subch_id);

	return BBM_OK;
}

fci_s32 fc8080_video_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id, fci_u8 cdi_id)
{
	fci_u16 fec_en = 0;

	if (cdi_id == 0) {
		bbm_write(handle, BBM_FEC_RST, 0x1c);
		bbm_write(handle, BBM_FEC_RST, 0x00);
	}

	bbm_word_read(handle, BBM_MSC_CFG_SCH0, &fec_en);

	if ((fec_en & 0x00ff) && (fec_en & 0xff00) && cdi_id == 0) {
		bbm_write(handle, BBM_FEC_ON, 0x00);
		bbm_write(handle, BBM_MSC_CFG_SCH0 + cdi_id, 0x00);
		bbm_write(handle, BBM_MSC_CFG_SCH0 + cdi_id, fec_en & 0x00ff);
		bbm_write(handle, BBM_FEC_ON, 0x01);
	}

	if (fc8080_channel_select(handle, subch_id, buf_id) != BBM_OK)
		return BBM_NOK;

	bbm_write(handle, BBM_MSC_CFG_SCH0 + cdi_id, 0x40 | subch_id);
	bbm_write(handle, BBM_BUF_CH0_SUBID + buf_id, 0x40 | subch_id);

	return BBM_OK;
}

fci_s32 fc8080_audio_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	if (fc8080_channel_select(handle, subch_id, buf_id) != BBM_OK)
		return BBM_NOK;

	bbm_write(handle, BBM_BUF_CH0_SUBID + buf_id, 0x40 | subch_id);

	return BBM_OK;
}

fci_s32 fc8080_data_select(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	if (fc8080_channel_select(handle, subch_id, buf_id) != BBM_OK)
		return BBM_NOK;

	bbm_write(handle, BBM_BUF_CH0_SUBID + buf_id, 0x40 | subch_id);

	return BBM_OK;
}

fci_s32 fc8080_channel_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	fci_u8 buf_en = 0;

	bbm_read(handle, BBM_BUF_ENABLE, &buf_en);
	bbm_write(handle, BBM_BUF_ENABLE, buf_en & (~(1 << buf_id)));
	bbm_write(handle, BBM_SCH0_SET_IDI + buf_id, 0);

	return BBM_OK;
}

fci_s32 fc8080_video_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id, fci_u8 cdi_id)
{
	fci_u16 fec_en = 0;
	fci_u8 buf_en = 0;

	bbm_write(handle, BBM_BUF_CH0_SUBID + buf_id, 0x00);
	bbm_word_read(handle, BBM_MSC_CFG_SCH0, &fec_en);

	if (!((fec_en & 0xff00) && (fec_en & 0x00ff) && cdi_id == 0))
		bbm_write(handle, BBM_MSC_CFG_SCH0 + cdi_id, 0x00);

	if (fc8080_channel_deselect(handle, subch_id, buf_id) != BBM_OK)
		return BBM_NOK;

	bbm_read(handle, BBM_BUF_CH0_SUBID, &buf_en);

	if (buf_en == 0 && (fec_en & 0xff00) && cdi_id == 1)
		bbm_write(handle, BBM_MSC_CFG_SCH0, 0x00);

	return BBM_OK;
}

fci_s32 fc8080_audio_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	if (fc8080_channel_deselect(handle, subch_id, buf_id) != BBM_OK)
		return BBM_NOK;

	bbm_write(handle, BBM_BUF_CH0_SUBID + buf_id, 0);

	return BBM_OK;
}

fci_s32 fc8080_data_deselect(HANDLE handle, fci_u8 subch_id, fci_u8 buf_id)
{
	if (fc8080_channel_deselect(handle, subch_id, buf_id) != BBM_OK)
		return BBM_NOK;

	bbm_write(handle, BBM_BUF_CH0_SUBID + buf_id, 0);

	return BBM_OK;
}

fci_s32 fc8080_scan_status(HANDLE handle)
{
	fci_s32 i, res = BBM_NOK;
	fci_u8  mode = 0, status = 0, sync_status = 0;
	fci_s32 dlock_cnt;

	bbm_read(handle, BBM_OFDM_DET, &mode);

	if ((mode & 0x01) == 0x01) {
		fci_s32 slock_cnt, flock_cnt;
		slock_cnt = SLOCK_MAX_TIME / LOCK_TIME_TICK;
		flock_cnt = FLOCK_MAX_TIME / LOCK_TIME_TICK;
		dlock_cnt = DLOCK_MAX_TIME / LOCK_TIME_TICK;

		for (i = 0; i < slock_cnt; i++) {
			if(!ms_wait(LOCK_TIME_TICK))
				return BBM_NOK;

			bbm_read(handle, BBM_DETECT_OFDM, &status);
#ifdef FEATURE_DEBUG_LOG
			printk("OFDM Detect Status 0x%x \n", status);
#endif
			if (status & 0x01)
				break;
		}

		if (i == slock_cnt)
			return BBM_NOK;

		if ((status & 0x02) == 0x00)
			return BBM_NOK;

		for (i += 1; i < flock_cnt; i++) {
			if(!ms_wait(LOCK_TIME_TICK))
				return BBM_NOK;
			bbm_read(handle, BBM_SYNC_STAT, &sync_status);
#ifdef FEATURE_DEBUG_LOG
			printk("FRS Status 0x%x \n", sync_status);
#endif

			if (sync_status & 0x01)
				break;
		}

		if (i == flock_cnt)
			return BBM_NOK;

		for (i += 1; i < dlock_cnt; i++) {
			if(!ms_wait(LOCK_TIME_TICK))
				return BBM_NOK;

			bbm_read(handle, BBM_SYNC_STAT, &sync_status);
#ifdef FEATURE_DEBUG_LOG
			printk("Digital Lock Status 0x%x \n", sync_status);
#endif

			if (sync_status & 0x20)
				return BBM_OK;
		}
	} else {
		dlock_cnt = DLOCK_MAX_TIME / LOCK_TIME_TICK;

		for (i = 0; i < dlock_cnt; i++) {
			if(!ms_wait(LOCK_TIME_TICK))
				return BBM_NOK;

			bbm_read(handle, BBM_SYNC_STAT, &sync_status);
			if (sync_status & 0x20)
				return BBM_OK;
		}
	}

	return res;
}
