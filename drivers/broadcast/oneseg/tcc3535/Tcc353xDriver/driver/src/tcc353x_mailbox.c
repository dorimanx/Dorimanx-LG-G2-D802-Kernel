/****************************************************************************
 *   FileName    : tcc353x_mailbox.c
 *   Description : mailbox control Function
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited to re-
distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS" and nothing contained in this source code shall 
constitute any express or implied warranty of any kind, including without limitation, any warranty 
of merchantability, fitness for a particular purpose or non-infringement of any patent, copyright 
or other third party intellectual property right. No warranty is made, express or implied, 
regarding the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, out of 
or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************/

#include "tcc353x_mailbox.h"
#include "tcc353x_register_control.h"
#include "tcpal_os.h"

extern TcpalSemaphore_t
    Tcc353xMailboxSema[TCC353X_MAX][TCC353X_DIVERSITY_MAX];
extern TcpalSemaphore_t
    Tcc353xOpMailboxSema[TCC353X_MAX][TCC353X_DIVERSITY_MAX];

#define MAXWAIT_MAILBOX 1000	/* about 1sec */

static I32U Tcc353xMailboxStatus(I32U input, I32U WFlag)
{
	static I32U stat;

	if (WFlag)
		stat = input;
	else
		return stat;

	return stat;
}

static void Tcc353xMailboxTx(Tcc353xHandle_t * _handle, I32S rw_flag,
			     I32U cmd, I32U * data_array, I32U word_cnt,
			     I32S * pmailboxok)
{
	I32U temp;
	I08U op_debug[3];
	I32U i;
	I08U wstat;
	TcpalTime_t CurrTime;

	pmailboxok[0] = 1;
	Tcc353xSetRegMailboxControl(_handle, TC3XREG_MAIL_INIT);

	temp =
	    (MB_HOSTMAIL << 24) | (word_cnt << 20) | (rw_flag << 19) |
	    (MB_ERR_OK << 16) | cmd;

	Tcc353xSetRegMailboxFifoWindow(_handle, (I08U *) (&temp), 4);

	for (i = 0; i < (unsigned int) word_cnt; i++) {
		Tcc353xSetRegMailboxFifoWindow(_handle,
					       (I08U *) (&data_array[i]),
					       4);
	}

	Tcc353xMailboxStatus(cmd, 1);
	Tcc353xSetRegMailboxControl(_handle, TC3XREG_MAIL_HOSTMAILPOST);

	CurrTime = TcpalGetCurrentTimeCount_ms();

	do {
		if (!(_handle->sysEnValue & TC3XREG_SYS_EN_DSP)) {
			/* Exceptional Error */
			TcpalPrintErr((I08S *)
				      "[TCC353X] [Error] MailBox - OP Disabled!!! \n");
			return;
		}

		if (TcpalGetTimeIntervalCount_ms(CurrTime) >
		    MAXWAIT_MAILBOX) {
			Tcc353xGetRegOpDebug(_handle, op_debug, _LOCK_);
			TcpalPrintErr((I08S *)
				      "[TCC353X] [Error] MailBox Write Timeout [Command:0x%x] [OP:0x%02x%02x%02x]!!! \n",
				      cmd, op_debug[0], op_debug[1],
				      op_debug[2]);
			pmailboxok[0] = -1;
			return;
		}
		TcpalmDelay(1);
		Tcc353xGetRegMailboxFifoWriteStatus(_handle, &wstat);
	}
	while (!(wstat & 0x01));
}

static I32S Tcc353xMailboxRx(Tcc353xHandle_t * _handle,
			     mailbox_t * p_mailbox, I32S * pmailboxok)
{
	I08U temp;
	I08U op_debug[3];
	I32S i;
	I32S total_word_num;
	I32S total_byte_num;
	I32U cmd;
	TcpalTime_t CurrTime;

	pmailboxok[0] = 1;

	i = 0;
	CurrTime = TcpalGetCurrentTimeCount_ms();

	do {
		if (!(_handle->sysEnValue & TC3XREG_SYS_EN_DSP)) {
			/* Exceptional Error */
			TcpalPrintErr((I08S *)
				      "[TCC353X] [Error] MailBox - OP Disabled!!! \n");
			return TCC353X_RETURN_FAIL;
		}

		Tcc353xGetRegMailboxFifoReadStatus(_handle, &temp);

		if (TcpalGetTimeIntervalCount_ms(CurrTime) >
		    MAXWAIT_MAILBOX) {
			pmailboxok[0] = -1;
			Tcc353xGetRegOpDebug(_handle, op_debug, _LOCK_);
			TcpalPrintErr((I08S *)
				      "[TCC353X] [Error] MailBox Read Timeout[OP:0x%02x%02x%02x]!!! \n",
				      op_debug[0], op_debug[1],
				      op_debug[2]);
			return TCC353X_RETURN_FAIL;
		}
		TcpalmDelay(1);
	}
	while ((temp & 0xfc) < 3);

	total_byte_num = (temp >> 2) & 0x3f;
	total_word_num = (total_byte_num >> 2);

	/* LSB First */
	Tcc353xGetRegMailboxFifoWindow(_handle, (I08U *) (&cmd), 1 << 2);
	Tcc353xGetRegMailboxFifoWindow(_handle,
				       (I08U
					*) (&p_mailbox->data_array[0]),
				       (total_word_num - 1) << 2);

	/* mark check */
	if ((cmd >> 24) != MB_SLAVEMAIL) {
		I32U mailstat;

		TcpalPrintErr((I08S *)
			      "[TCC353X] Mailbox Error Cmd[0x%x] Total Byte Num[0x%x]\n",
			      cmd, total_byte_num);

		for (i = 0; i < total_word_num - 1; i++) {
			TcpalPrintErr((I08S *)
				      "[TCC353X] data_array[%d][0x%x]\n",
				      i, p_mailbox->data_array[i]);
		}

		mailstat = Tcc353xMailboxStatus(cmd, 0);
		TcpalPrintErr((I08S *)
			      "[TCC353X] [MBERR]Mark Error[0x%x][ori:0x%x]\n",
			      cmd, mailstat);
		TcpalPrintErr((I08S *)
			      "[TCC353X] [MBERR]please check the header.\n");
		pmailboxok[0] = -1;
		return TCC353X_RETURN_FAIL;
	}

	p_mailbox->cmd = cmd & 0xffff;
	p_mailbox->word_cnt = total_word_num - 1;
	p_mailbox->status = ((cmd >> 16) & 0x07);

	if (p_mailbox->status != 0) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] [MBERR] Error Message : 0x%0x[cmd 0x%x][totalnum 0x%x]\n",
			      p_mailbox->status, cmd, total_word_num);
		pmailboxok[0] = -1;
		return TCC353X_RETURN_FAIL;
	}

	return TCC353X_RETURN_SUCCESS;
}

static I32S Tcc353xMailboxTxOnlySub(Tcc353xHandle_t * _handle, I32U cmd,
				    I32U * data_array, I32S word_cnt)
{
	I32S mailboxok;

	TcpalSemaphoreLock(&Tcc353xMailboxSema[_handle->moduleIndex]
			   [_handle->diversityIndex]);
	Tcc353xMailboxTx(_handle, MB_CMD_WRITE, cmd, data_array, word_cnt,
			 &mailboxok);
	TcpalSemaphoreUnLock(&Tcc353xMailboxSema[_handle->moduleIndex]
			     [_handle->diversityIndex]);

	if (mailboxok == -1) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] [ERR] MailboxTX Error.\n");
		return TCC353X_RETURN_FAIL;
	}

	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xMailboxTxOnly(Tcc353xHandle_t * _handle, I32U cmd,
			  I32U * data_array, I32S word_cnt)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_handle->moduleIndex]
			   [_handle->diversityIndex]);
	if (_handle->mailboxErrorCounter > MAX_MAILBOX_RETRY) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] [M] Critical Mailbox Control Error!!! Can't recover!!!\n");
		_handle->mailboxErrorCounter = 0;
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema
				     [_handle->
				      moduleIndex]
				     [_handle->diversityIndex]);
		return TCC353X_RETURN_FAIL;
	}

	ret = Tcc353xMailboxTxOnlySub(_handle, cmd, data_array, word_cnt);

	if (ret == TCC353X_RETURN_FAIL) {
		TcpalPrintErr((I08S *)
				 "[TCC353X] [ERR] MailboxTX [cmd:0x%x]\n",
				 cmd);

		/* one more time set and give up */
		TcpalPrintErr((I08S *) "[TCC353X] [M] Mailbox Retry\n");
		ret =
		    Tcc353xMailboxTxOnlySub(_handle, cmd, data_array,
					    word_cnt);
		if (ret == TCC353X_RETURN_FAIL) {
			TcpalPrintErr((I08S *)
				      "[TCC353X] [ERR] Can't recover mailbox. Please Retune!\n");
			_handle->mailboxErrorCounter = 0;
			TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema
					     [_handle->moduleIndex]
					     [_handle->diversityIndex]);
			return TCC353X_RETURN_FAIL;
		}
	}

	_handle->mailboxErrorCounter = 0;
	TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_handle->moduleIndex]
			     [_handle->diversityIndex]);
	return ret;
}

static I32S Tcc353xMailboxTxRxSub(Tcc353xHandle_t * _handle,
				  mailbox_t * p_mailbox, I32U cmd,
				  I32U * data_array, I32S word_cnt)
{
	I32S ret = TCC353X_RETURN_SUCCESS;
	I32S mailboxok;

	TcpalSemaphoreLock(&Tcc353xMailboxSema[_handle->moduleIndex]
			   [_handle->diversityIndex]);

	Tcc353xMailboxTx(_handle, MB_CMD_READ, cmd, data_array, word_cnt,
			 &mailboxok);

	if (mailboxok == -1) {
		TcpalSemaphoreUnLock(&Tcc353xMailboxSema
				     [_handle->
				      moduleIndex]
				     [_handle->diversityIndex]);
		TcpalPrintErr((I08S *)
			      "[TCC353X] [ERR] MailboxTXRX Error. cmd[%x]\n", cmd);
		return TCC353X_RETURN_FAIL;
	} else {
		ret = Tcc353xMailboxRx(_handle, p_mailbox, &mailboxok);

		if (mailboxok == -1) {
			TcpalSemaphoreUnLock(&Tcc353xMailboxSema
					     [_handle->moduleIndex]
					     [_handle->diversityIndex]);
			TcpalPrintErr((I08S *)
				      "[TCC353X] [ERR] MailboxTXRX Error. cmd[%x]\n", cmd);
			return TCC353X_RETURN_FAIL;
		}
	}
	TcpalSemaphoreUnLock(&Tcc353xMailboxSema[_handle->moduleIndex]
			     [_handle->diversityIndex]);
	return ret;
}

I32S Tcc353xMailboxTxRx(Tcc353xHandle_t * _handle, mailbox_t * p_mailbox,
			I32U cmd, I32U * data_array, I32S word_cnt)
{
	I32S ret = TCC353X_RETURN_SUCCESS;

	TcpalSemaphoreLock(&Tcc353xOpMailboxSema[_handle->moduleIndex]
			   [_handle->diversityIndex]);
	if (_handle->mailboxErrorCounter > MAX_MAILBOX_RETRY) {
		TcpalPrintErr((I08S *)
			      "[TCC353X] [M] Critical Mailbox Control Error!!! Can't recover!!!\n");
		_handle->mailboxErrorCounter = 0;
		TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema
				     [_handle->
				      moduleIndex]
				     [_handle->diversityIndex]);
		return TCC353X_RETURN_FAIL;
	}

	ret =
	    Tcc353xMailboxTxRxSub(_handle, p_mailbox, cmd, data_array,
				  word_cnt);

	if (ret == TCC353X_RETURN_FAIL) {
		TcpalPrintErr((I08S *)
				 "[TCC353X] [ERR] MailboxTXRX [cmd:0x%x]\n",
				 cmd);

		/* one more time set and give up */
		TcpalPrintErr((I08S *) "[TCC353X] [M] Mailbox Retry\n");
		ret =
		    Tcc353xMailboxTxRxSub(_handle, p_mailbox, cmd,
					  data_array, word_cnt);
		if (ret == TCC353X_RETURN_FAIL) {
			TcpalPrintErr((I08S *)
				      "[TCC353X] [ERR] Can't recover mailbox. Please Retune!\n");
			_handle->mailboxErrorCounter = 0;
			TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema
					     [_handle->moduleIndex]
					     [_handle->diversityIndex]);
			return TCC353X_RETURN_FAIL;
		}
	}

	_handle->mailboxErrorCounter = 0;
	TcpalSemaphoreUnLock(&Tcc353xOpMailboxSema[_handle->moduleIndex]
			     [_handle->diversityIndex]);
	return ret;
}

I32U Tcc353xGetAccessMail(Tcc353xHandle_t * _handle)
{
	I32U access_mail;

	Tcc353xGetRegMailboxFifoWindow(_handle, (I08U *) (&access_mail),
				       1 << 2);

	if (access_mail != 0x1ACCE551)
		TcpalPrintLog((I08S *)
			      "[TCC353X] [%d][%d] AccessMail Error[0x%08x]\n",
			      _handle->moduleIndex,
			      _handle->diversityIndex, access_mail);
	return access_mail;
}
