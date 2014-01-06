/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fci_oal.c
 
 Description : OS Adaptation Layer
 
 History : 
 ----------------------------------------------------------------------
 2009/09/13 	jason		initial
*******************************************************************************/

#include "../inc/fci_types.h"
#include "../inc/broadcast_fc8050.h"

void PRINTF(HANDLE hDevice, char *fmt,...)
{

}

int msWait(int ms)
{
	return tdmb_fc8050_mdelay(ms);
}

void msMustWait(int ms)
{
	tdmb_fc8050_Must_mdelay(ms);
}

