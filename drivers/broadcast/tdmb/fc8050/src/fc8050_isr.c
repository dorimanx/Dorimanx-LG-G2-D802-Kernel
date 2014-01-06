/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_isr.c
 
 Description : fc8050 interrupt service routine
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
*******************************************************************************/
#include "../inc/fci_types.h"
#include "../inc/fci_hal.h"
#include "../inc/fc8050_regs.h"

#ifdef USE_QCT_DMA_LGE
#include <linux/dma-mapping.h>
static fci_u8 ficBuffer[512+4] __cacheline_aligned;
static fci_u8 mscBuffer[8192+4] __cacheline_aligned;
#else
static fci_u8 ficBuffer[512+4];
static fci_u8 mscBuffer[8192+4];
#endif

int (*pFicCallback)(fci_u32 userdata, fci_u8 *data, int length) = NULL;
int (*pMscCallback)(fci_u32 userdata, fci_u8 subchid, fci_u8 *data, int length) = NULL;

fci_u32 gFicUserData;
fci_u32 gMscUserData;

extern fci_u32 	tp_total_cnt;

void fc8050_isr_interruptclear(void)
{
	fci_u8	extIntStatus = 0;
	HANDLE hDevice = NULL;

	bbm_read(hDevice, BBM_COM_INT_STATUS, &extIntStatus);
	bbm_write(hDevice, BBM_COM_INT_STATUS, extIntStatus);
	bbm_write(hDevice, BBM_COM_INT_STATUS, 0x00);
}

void fc8050_isr_control(fci_u8 onoff)
{
	if(onoff)
		bbm_write(0, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
	else
		bbm_write(0, BBM_COM_INT_ENABLE, 0);
}

void fc8050_isr(HANDLE hDevice)
{
	fci_u8	extIntStatus = 0;

	//bbm_write(hDevice, BBM_COM_INT_ENABLE, 0);
	bbm_read(hDevice, BBM_COM_INT_STATUS, &extIntStatus);
	bbm_write(hDevice, BBM_COM_INT_STATUS, extIntStatus);
	bbm_write(hDevice, BBM_COM_INT_STATUS, 0x00);

	if(extIntStatus & BBM_MF_INT) {
		fci_u16	mfIntStatus = 0;
		fci_u16	size;
		int  	i;

		bbm_word_read(hDevice, BBM_BUF_STATUS, &mfIntStatus);
		//bbm_word_write(hDevice, BBM_BUF_STATUS, mfIntStatus);
		//bbm_word_write(hDevice, BBM_BUF_STATUS, 0x0000);

		if(mfIntStatus & 0x0100) {
			bbm_word_read(hDevice, BBM_BUF_FIC_THR, &size);
			size += 1;
			if(size-1) {
				bbm_data(hDevice, BBM_COM_FIC_DATA, &ficBuffer[0], size);

				if(pFicCallback)
					(*pFicCallback)((fci_u32) hDevice, &ficBuffer[0], size);

			} 
		}

		for(i=0; i<8; i++) {
			if(mfIntStatus & (1<<i)) {
				bbm_word_read(hDevice, BBM_BUF_CH0_THR+i*2, &size);
				size += 1;

				if(size-1) {
					fci_u8  subChId;

					bbm_read(hDevice, BBM_BUF_CH0_SUBCH+i, &subChId);
					subChId = subChId & 0x3f;

					{
						fci_u8 rsSubChId;

						bbm_read(hDevice, BBM_CDI0_SUBCH_EN, &rsSubChId);

						rsSubChId &= 0x3F;

						if(rsSubChId == subChId)
							tp_total_cnt += size/188;
					}

					bbm_data(hDevice, (BBM_COM_CH0_DATA+i), &mscBuffer[0], size);

					if(size>384)
					{
						if(pMscCallback)
							(*pMscCallback)(gMscUserData, subChId, &mscBuffer[2], size);
					}
					else
					{
						if(pMscCallback)
							(*pMscCallback)(gMscUserData, subChId, &mscBuffer[0], size);
					}

				}
			}
		}

		bbm_word_write(hDevice, BBM_BUF_STATUS, mfIntStatus);
		bbm_word_write(hDevice, BBM_BUF_STATUS, 0x0000);
	}

	//bbm_write(hDevice, BBM_COM_INT_ENABLE, ENABLE_INT_MASK);
}

