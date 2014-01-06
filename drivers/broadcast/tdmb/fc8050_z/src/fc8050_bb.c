/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : bbm.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
                            BB Config 1p0
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/fci_oal.h"
#include "../inc/fci_hal.h"
#include "../inc/fci_tun.h"
#include "../inc/fc8050_regs.h"
#include <linux/kernel.h>
#define POWER_SAVE_MODE
#define MSMCHIP

#define LOCK_TIME_TICK				5	// 5ms
#define SLOCK_MAX_TIME				200
#define FLOCK_MAX_TIME				300
#define DLOCK_MAX_TIME				500

int fc8050_reset(HANDLE hDevice)
{
	bbm_write(hDevice, BBM_COM_RESET, 0xFE);
	msMustWait(1);
	bbm_write(hDevice, BBM_COM_RESET, 0xFF);

	return BBM_OK;
}

int fc8050_probe(HANDLE hDevice)
{
	fci_u16 ver;
	bbm_word_read(hDevice, BBM_QDD_CHIP_IDL, &ver);
	printk("fc8050_probe 0x%x\n", ver);
	return (ver == 0x8050)? BBM_OK : BBM_NOK;
}

int fc8050_set_xtal(HANDLE hDevice) {
#if (FC8050_FREQ_XTAL == 19200)
	//////////////////////////////
	// Default XTAL
	//////////////////////////////
#elif (FC8050_FREQ_XTAL == 16384)
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x0);
	bbm_write(hDevice, BBM_QDD_COEF, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x1);
	bbm_write(hDevice, BBM_QDD_COEF, 0xff);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfd);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF, 0x0);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x5);
	bbm_write(hDevice, BBM_QDD_COEF, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x6);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfc);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x7);
	bbm_write(hDevice, BBM_QDD_COEF, 0xf9);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x8);
	bbm_write(hDevice, BBM_QDD_COEF, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x9);
	bbm_write(hDevice, BBM_QDD_COEF, 0xc);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xa);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xb);
	bbm_write(hDevice, BBM_QDD_COEF, 0xf0);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xc);
	bbm_write(hDevice, BBM_QDD_COEF, 0xed);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xd);
	bbm_write(hDevice, BBM_QDD_COEF, 0x13);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xe);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4f);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xf);
	bbm_write(hDevice, BBM_QDD_COEF, 0x6b);
	bbm_write(hDevice, 0xe8, 0x00);
	bbm_write(hDevice, 0xe9, 0x00);
	bbm_write(hDevice, 0xea, 0x00);
	bbm_write(hDevice, 0xeb, 0x04);
	bbm_write(hDevice, 0xec, 0x80);
	bbm_write(hDevice, 0xed, 0x80);
	bbm_write(hDevice, 0xee, 0x06);
#elif (FC8050_FREQ_XTAL == 24576)
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x0);
	bbm_write(hDevice, BBM_QDD_COEF, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x1);
	bbm_write(hDevice, BBM_QDD_COEF, 0xff);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfd);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF, 0x0);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x5);
	bbm_write(hDevice, BBM_QDD_COEF, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x6);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfc);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x7);
	bbm_write(hDevice, BBM_QDD_COEF, 0xf9);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x8);
	bbm_write(hDevice, BBM_QDD_COEF, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x9);
	bbm_write(hDevice, BBM_QDD_COEF, 0xc);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xa);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xb);
	bbm_write(hDevice, BBM_QDD_COEF, 0xf0);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xc);
	bbm_write(hDevice, BBM_QDD_COEF, 0xed);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xd);
	bbm_write(hDevice, BBM_QDD_COEF, 0x13);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xe);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4f);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xf);
	bbm_write(hDevice, BBM_QDD_COEF, 0x6b);
	bbm_write(hDevice, 0xe8, 0x00);
	bbm_write(hDevice, 0xe9, 0x00);
	bbm_write(hDevice, 0xea, 0x00);
	bbm_write(hDevice, 0xeb, 0x04);
	bbm_write(hDevice, 0xec, 0x80);
	bbm_write(hDevice, 0xed, 0x80);
	bbm_write(hDevice, 0xee, 0x05);
#elif (FC8050_FREQ_XTAL == 27000)
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x0);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfe);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x1);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfe);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF, 0x1);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x5);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfd);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x6);
	bbm_write(hDevice, BBM_QDD_COEF, 0xf9);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x7);
	bbm_write(hDevice, BBM_QDD_COEF, 0xff);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x8);
	bbm_write(hDevice, BBM_QDD_COEF, 0x9);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x9);
	bbm_write(hDevice, BBM_QDD_COEF, 0x9);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xa);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfb);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xb);
	bbm_write(hDevice, BBM_QDD_COEF, 0xed);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xc);
	bbm_write(hDevice, BBM_QDD_COEF, 0xf5);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xd);
	bbm_write(hDevice, BBM_QDD_COEF, 0x1c);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xe);
	bbm_write(hDevice, BBM_QDD_COEF, 0x4b);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xf);
	bbm_write(hDevice, BBM_QDD_COEF, 0x61);
	bbm_write(hDevice, 0xe8, 0x4b);
	bbm_write(hDevice, 0xe9, 0x11);
	bbm_write(hDevice, 0xea, 0xa4);
	bbm_write(hDevice, 0xeb, 0x03);
	bbm_write(hDevice, 0xec, 0x8c);
	bbm_write(hDevice, 0xed, 0x75);
	bbm_write(hDevice, 0xee, 0x05);
#elif (FC8050_FREQ_XTAL == 38400)
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x0);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfe);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x1);
	bbm_write(hDevice, BBM_QDD_COEF, 0x0);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x2);
	bbm_write(hDevice, BBM_QDD_COEF, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x4);
	bbm_write(hDevice, BBM_QDD_COEF, 0xff);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x5);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfa);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x6);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfb);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x7);
	bbm_write(hDevice, BBM_QDD_COEF, 0x3);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x8);
	bbm_write(hDevice, BBM_QDD_COEF, 0xa);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0x9);
	bbm_write(hDevice, BBM_QDD_COEF, 0x5);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xa);
	bbm_write(hDevice, BBM_QDD_COEF, 0xf7);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xb);
	bbm_write(hDevice, BBM_QDD_COEF, 0xed);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xc);
	bbm_write(hDevice, BBM_QDD_COEF, 0xfa);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xd);
	bbm_write(hDevice, BBM_QDD_COEF, 0x1f);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xe);
	bbm_write(hDevice, BBM_QDD_COEF, 0x49);
	bbm_write(hDevice, BBM_QDD_COEF_BANK_SEL, 0xf);
	bbm_write(hDevice, BBM_QDD_COEF, 0x5c);
	bbm_write(hDevice, 0xe8, 0x36);
	bbm_write(hDevice, 0xe9, 0xd0);
	bbm_write(hDevice, 0xea, 0x69);
	bbm_write(hDevice, 0xeb, 0x03);
	bbm_write(hDevice, 0xec, 0x96);
	bbm_write(hDevice, 0xed, 0x6d);
	bbm_write(hDevice, 0xee, 0x04);
#endif
	return BBM_OK;
}

int fc8050_init(HANDLE hDevice)
{
	fci_u8 intMask;

	fc8050_reset(hDevice);
	fc8050_set_xtal(hDevice);

	bbm_write(hDevice, BBM_BUF_MISC_CTRL, 0x19);

	//bbm_write(hDevice, BBM_24M_CLK_EN, 0xff);
	bbm_write(hDevice, BBM_VT_CONTROL, 0x03);
	bbm_word_write(hDevice, BBM_SYNC_CNTRL, 0x0020);
	bbm_write(hDevice, BBM_FIC_CRC_CONTROL, 0x07);
	bbm_write(hDevice, BBM_BUF_TEST_MODE, 0x08);
	bbm_write(hDevice, 0x33c, 0x03);

#ifdef TSIF
	bbm_write(hDevice, BBM_TS_PAUSE, 0x60);
	bbm_write(hDevice, BBM_BUF_STS_CLK_DIV, 0x05);

#ifdef MSMCHIP
	bbm_write(hDevice, BBM_BUF_STS_CTRL, 0xc8);
	bbm_write(hDevice, BBM_TS_SELECT, 0x84);
#else
	bbm_write(hDevice, BBM_BUF_STS_CTRL, 0x02);
	bbm_write(hDevice, BBM_TS_SELECT, 0xc3);
#endif
#endif
	bbm_write(hDevice, BBM_FFT_MODEM_STSH, 0x03);
	bbm_write(hDevice, BBM_DIDP_MODE, 0x01);
	bbm_write(hDevice, BBM_SYNC_DET_CNTRL, 0x01);
	bbm_word_write(hDevice, BBM_SYNC_DET_MAX_THRL, 0x0A00);
	bbm_write(hDevice, BBM_SYNC_DET_MODE_ENABLE, 0x01);
	bbm_write(hDevice, BBM_BUF_CLOCK_EN, 0xff);
	bbm_write(hDevice, BBM_FFT_SCALEV_IFFT, 0xea);
	bbm_write(hDevice, BBM_SYNC_FT_RANGE, 0x20);
	bbm_write(hDevice, BBM_QDD_AGC530_EN, 0x53);
	bbm_write(hDevice, BBM_QDD_BLOCK_AVG_SIZE, 0x48);
	bbm_write(hDevice, BBM_QDD_BLOCK_AVG_SIZE_LOCK, 0x49);
	bbm_word_write(hDevice, BBM_QDD_GAIN_CONSTANT, 0x0303);
	bbm_write(hDevice, BBM_QDD_DET_CNT_BOUND, 0x60);
	bbm_write(hDevice, BBM_QDD_REF_AMPL, 0x00);
	bbm_write(hDevice, BBM_QDD_BW_CTRL_LOCK, 0x50);
	bbm_write(hDevice, BBM_QDD_DC_CTRL, 0x3f);

	//bbm_write(hDevice, BBM_RS_CONTROL, 0x09); /* Send error-indicator set packet to host */
	bbm_write(hDevice, BBM_RS_CONTROL, 0x01); /* Not send error-indicator set packet to host */
	bbm_word_write(hDevice, BBM_RS_BER_PERIOD, 0x14e);

#if defined(POWER_SAVE_MODE)
	bbm_write(hDevice, BBM_DIDP_POWER_OPT0, 0x06);
	bbm_write(hDevice, BBM_DIDP_ADD_N_SHIFT0, 0x41);
	bbm_write(hDevice, BBM_DIDP_POWER_OPT1, 0x06);
	bbm_write(hDevice, BBM_DIDP_ADD_N_SHIFT1, 0xf1);
	bbm_write(hDevice, BBM_DIDP_POWER_OPT2, 0x07);
	bbm_write(hDevice, BBM_FFT_ADC_CONTROL, 0x1c);
#else
	bbm_write(hDevice, BBM_DIDP_POWER_OPT0, 0x04);
	bbm_write(hDevice, BBM_DIDP_ADD_N_SHIFT0, 0x21);
	bbm_write(hDevice, BBM_DIDP_POWER_OPT1, 0x05);
	bbm_write(hDevice, BBM_DIDP_ADD_N_SHIFT1, 0x21);
	bbm_write(hDevice, BBM_DIDP_POWER_OPT2, 0x05);
	bbm_write(hDevice, BBM_FFT_ADC_CONTROL, 0x9c);
#endif

	bbm_word_write(hDevice, BBM_BUF_FIC_START,	FIC_BUF_START);
	bbm_word_write(hDevice, BBM_BUF_FIC_END, 	FIC_BUF_END);
	bbm_word_write(hDevice, BBM_BUF_FIC_THR, 	FIC_BUF_THR);
	bbm_word_write(hDevice, BBM_BUF_CH0_START,	CH0_BUF_START);
	bbm_word_write(hDevice, BBM_BUF_CH0_END, 	CH0_BUF_END);
	bbm_word_write(hDevice, BBM_BUF_CH0_THR, 	CH0_BUF_THR);
	bbm_word_write(hDevice, BBM_BUF_CH1_START,	CH1_BUF_START);
	bbm_word_write(hDevice, BBM_BUF_CH1_END, 	CH1_BUF_END);
	bbm_word_write(hDevice, BBM_BUF_CH1_THR, 	CH1_BUF_THR);
	bbm_word_write(hDevice, BBM_BUF_CH2_START,	CH2_BUF_START);
	bbm_word_write(hDevice, BBM_BUF_CH2_END, 	CH2_BUF_END);
	bbm_word_write(hDevice, BBM_BUF_CH2_THR, 	CH2_BUF_THR);
	bbm_word_write(hDevice, BBM_BUF_CH3_START,	CH3_BUF_START);
	bbm_word_write(hDevice, BBM_BUF_CH3_END, 	CH3_BUF_END);
	bbm_word_write(hDevice, BBM_BUF_CH3_THR, 	CH3_BUF_THR);

	bbm_word_write(hDevice, BBM_BUF_INT, 0x01ff);
	bbm_word_write(hDevice, BBM_BUF_ENABLE, 0x01ff);

	intMask = ENABLE_INT_MASK;
	bbm_write(hDevice, BBM_COM_INT_ENABLE, intMask);
	bbm_write(hDevice, BBM_COM_STATUS_ENABLE, intMask);

	return BBM_OK;
}

int fc8050_deinit(HANDLE hDevice)
{
	bbm_write(hDevice, BBM_COM_RESET, 0x00);

	return BBM_OK;
}

int fc8050_channel_select(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId)
{
//                                      

	bbm_write(hDevice, BBM_DIDP_CH0_SUBCH + svcChId, 0x40 | subChId);

#if 0 //it only works on test stream so we block this routine to reduce delay time when setting channel
	if(fc8050_cu_size_check(hDevice, svcChId, &cuSize)) {
		fc8050_power_save_off(hDevice);
		return BBM_OK;
	}

	if(cuSize >= 672) {
		fc8050_power_save_off(hDevice);
		return BBM_OK;
	}

	fc8050_power_save_on(hDevice);
#endif
	return BBM_OK;
}

int fc8050_video_select(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId, fci_u8 cdiId)
{
	if(fc8050_channel_select(hDevice, subChId,svcChId) != BBM_OK) {
		return BBM_NOK;
	}
	bbm_write(hDevice, BBM_CDI0_SUBCH_EN+cdiId, 0x40 | subChId);
	bbm_write(hDevice, BBM_BUF_CH0_SUBCH+svcChId, 0x40 | subChId);
	return BBM_OK;
}

int fc8050_audio_select(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId)
{
	if(fc8050_channel_select(hDevice, subChId,svcChId) != BBM_OK) {
		return BBM_NOK;
	}
	bbm_write(hDevice, BBM_BUF_CH0_SUBCH+svcChId, 0x40 | subChId);
	return BBM_OK;
}

int fc8050_data_select(HANDLE hDevice, fci_u8 subChId,fci_u8 svcChId)
{
	if(fc8050_channel_select(hDevice, subChId,svcChId) != BBM_OK) {
		return BBM_NOK;
	}
	bbm_write(hDevice, BBM_BUF_CH0_SUBCH+svcChId, 0x40 | subChId);

	return BBM_OK;
}

int fc8050_channel_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId)
{
	int i;

	bbm_write(hDevice, BBM_DIDP_CH0_SUBCH + svcChId, 0);

	for(i = 0; i < 12; i++) {
		bbm_write(hDevice, 0x190 + svcChId * 12 + i, 0);
	}

	return BBM_OK;
}

int fc8050_video_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId, fci_u8 cdiId)
{
	if(fc8050_channel_deselect(hDevice, subChId,svcChId) != BBM_OK) {
		return BBM_NOK;
	}
	bbm_write(hDevice, BBM_BUF_CH0_SUBCH+svcChId, 0x00);
	bbm_write(hDevice, BBM_CDI0_SUBCH_EN+cdiId,   0x00);
	return BBM_OK;
}

int fc8050_audio_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId)
{
	if(fc8050_channel_deselect(hDevice, subChId, svcChId) != BBM_OK) {
		return BBM_NOK;
	}
	bbm_write(hDevice, BBM_BUF_CH0_SUBCH+svcChId, 0);
	return BBM_OK;
}

int fc8050_data_deselect(HANDLE hDevice, fci_u8 subChId, fci_u8 svcChId)
{
	if(fc8050_channel_deselect(hDevice, subChId, svcChId) != BBM_OK) {
		return BBM_NOK;
	}
	bbm_write(hDevice, BBM_BUF_CH0_SUBCH+svcChId, 0);
	return BBM_OK;
}

int fc8050_scan_status(HANDLE hDevice) {
	int i, res = BBM_NOK;
	fci_u8  mode = 0, status = 0, sync_status = 0;
	int slock_cnt, flock_cnt, dlock_cnt;

	bbm_read(hDevice, BBM_SYNC_DET_CNTRL, &mode);

	if((mode & 0x01) == 0x01) {
		slock_cnt = SLOCK_MAX_TIME / LOCK_TIME_TICK;
		flock_cnt = FLOCK_MAX_TIME / LOCK_TIME_TICK;
		dlock_cnt = DLOCK_MAX_TIME / LOCK_TIME_TICK;

		// OFDM Detect
		for(i = 0; i < slock_cnt; i++) {
			if(!msWait(LOCK_TIME_TICK))
				return BBM_NOK;

			bbm_read(hDevice, BBM_SYNC_DET_STATUS, &status);
			printk("OFDM Detect Status 0x%x \n", status);
			if(status & 0x01)
				break;
		}

		if(i == slock_cnt)
			return BBM_NOK;

		if((status & 0x02) == 0x00)
			return BBM_NOK;

		// FRS
		for(i += 1; i < flock_cnt; i++) {
			if(!msWait(LOCK_TIME_TICK))
				return BBM_NOK;

			bbm_read(hDevice, BBM_SYNC_STATUS, &sync_status);
			printk("FRS Status 0x%x \n", status);

			if(sync_status & 0x01)
				break;
		}

		if (i == flock_cnt)
			return BBM_NOK;

		// Digital Lock
		for(i += 1; i < dlock_cnt; i++) {
			if(!msWait(LOCK_TIME_TICK))
				return BBM_NOK;

			bbm_read(hDevice, BBM_SYNC_STATUS, &sync_status);
			printk("Digital Lock Status 0x%x \n", status);

			if(sync_status & 0x20)
				return BBM_OK;
		}
	} else {
		dlock_cnt = DLOCK_MAX_TIME / LOCK_TIME_TICK;

		for(i = 0; i < dlock_cnt; i++) {
			if(!msWait(LOCK_TIME_TICK))
				return BBM_NOK;

			bbm_read(hDevice, BBM_SYNC_STATUS, &sync_status);
			if(sync_status & 0x20) {
				return BBM_OK;
			}
		}
	}

	return res;
}
