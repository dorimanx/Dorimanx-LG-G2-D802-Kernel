/****************************************************************************
 *   FileName    : tcc353x_isdb.c
 *   Description : isdb Function
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

#include "tcc353x_isdb.h"
#include "tcc353x_command_control.h"
#include "tcc353x_register_control.h"
#include "tcpal_os.h"

extern I32S Tcc353xSetStreamFormat(I32S _moduleIndex,
			    Tcc353xStreamFormat_t * _streamFormat);

#define MAX_PID_FILTER_CNT 	32
extern Tcc353xHandle_t Tcc353xHandle[TCC353X_MAX][TCC353X_DIVERSITY_MAX];

static I32S Tcc353xFlushPidFilterTable (I32S _moduleIndex)
{
	I32U input[16];

	Tcc353xHandle[_moduleIndex][0].numOfPidTable = 0;
	TcpalMemset (&Tcc353xHandle[_moduleIndex][0].pidTable[0], 
		     0x00, sizeof(I32U)*MAX_PID_FILTER_CNT);

	/* Reset Pid Filtering values to disable */
	TcpalMemset (&input[0], 0x00, sizeof(I32U)*16);
	Tcc353xMiscWriteExIncrease(_moduleIndex, 0, MISC_OP_REG_CTRL, 
			   TC3XREG_OP_PID00, 
			   &input[0], 16);

	return TCC353X_RETURN_SUCCESS;
}

static I32S checkPidTable(I32S _moduleIndex, I32U _pid)
{
	I32S findEmptySlot = -1;
	I32S i;
	Tcc353xHandle_t *h;

	h = (Tcc353xHandle_t *)(&Tcc353xHandle[_moduleIndex][0]);

	for (i=0; i<MAX_PID_FILTER_CNT; i++)
	{
		if(findEmptySlot == -1)
			if ((h->pidTable[i] & 0x8000) == 0)
			    	findEmptySlot = i;

		if(((h->pidTable[i]&0x1FFF) == _pid) && 
		   ((h->pidTable[i]&0x8000) != 0))
			return -1;
	}

	if(findEmptySlot != -1)
		return -1;
	else
		return findEmptySlot;
}

static I32S findPidTable(I32S _moduleIndex, I32U _pid)
{
	Tcc353xHandle_t *h;
	I32S i;

	h = (Tcc353xHandle_t *)(&Tcc353xHandle[_moduleIndex][0]);

	for (i=0; i<MAX_PID_FILTER_CNT; i++)
	{
		if(((h->pidTable[i]&0x1FFF) == _pid) && 
		   ((h->pidTable[i]&0x8000) != 0))
			return i;
	}

	return -1;
}

I32S Tcc353xEnablePidFiltering(I32S _moduleIndex, I32U _flag)
{
	Tcc353xStreamFormat_t streamFormat;
	TcpalMemcpy(&streamFormat, 
		    &Tcc353xHandle[_moduleIndex][0].
		    streamFormat, sizeof(Tcc353xStreamFormat_t));

	if(_flag) {
		if (streamFormat.pidFilterEnable)
			return TCC353X_RETURN_FAIL;

		streamFormat.pidFilterEnable = 1;
		Tcc353xSetStreamFormat(_moduleIndex, &streamFormat);
	}
	else {
		if (streamFormat.pidFilterEnable) {
			streamFormat.pidFilterEnable = 0;
			Tcc353xSetStreamFormat(_moduleIndex, 
					       &streamFormat);
		}
	    	Tcc353xFlushPidFilterTable (_moduleIndex);
	}

	return TCC353X_RETURN_SUCCESS;
}


void Tcc353xInitIsdbProcess(Tcc353xHandle_t * _handle)
{
	I32U input[2];

	/* disable pid filtering */
	Tcc353xEnablePidFiltering (_handle->moduleIndex, 0);

	/* Init CAS PID */
	TcpalMemset (&input[0], 0x00, sizeof(I32U)*2);
	Tcc353xMiscWriteExIncrease(_handle->moduleIndex, 0, MISC_OP_REG_CTRL, 
			   TC3XREG_OP_CAS_PID0100, &input[0], 2);
}

I32S Tcc353xAddPidsFiltering (I32S _moduleIndex, Tcc353xpidTable_t *
			      _pidTableControl)
{
	I32U i;
	I32S slot;
	I32U data;
	I32U idx;
	Tcc353xHandle_t *h;

	h = (Tcc353xHandle_t *)(&Tcc353xHandle[_moduleIndex][0]);

	if (!h->streamFormat.pidFilterEnable)
		Tcc353xEnablePidFiltering(_moduleIndex, 1);


	if (h->numOfPidTable + _pidTableControl->numberOfPid > 
	    MAX_PID_FILTER_CNT) {
		TcpalPrintErr((I08S *)"[TCC353X] MAX PID count over\n");
		return TCC353X_RETURN_FAIL;
	}

	for (i=0; i<_pidTableControl->numberOfPid; i++)	{
		/* check exist pid & get empty slot */
		slot = checkPidTable (_moduleIndex, _pidTableControl->Pid[i]);

		if(slot == -1)	{
			TcpalPrintLog((I08S *)"[TCC353X] PID[0x%x] Exist\n",
				_pidTableControl->Pid[i]);
		}
		else	{
			h->pidTable[slot]= _pidTableControl->Pid[i] & 0x8000;
			h->numOfPidTable++;

			TcpalPrintLog((I08S *)"[TCC353X] Add PID[0x%x]\n",
				_pidTableControl->Pid[i]);
			/* apply pid table to register */
			idx = ((I32U)(slot)>>1);

			if(slot & 0x01)
				data = h->pidTable[slot-1] | 
					    (h->pidTable[slot]<<16);
			else
				data = h->pidTable[slot] | 
				           (h->pidTable[slot+1]<<16);

			Tcc353xMiscWrite(_moduleIndex, 0,
					 MISC_OP_REG_CTRL,
					 (I08U)(TC3XREG_OP_PID00 + idx),
					 data);
		}
	}
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xRemovePidsFiltering (I32S _moduleIndex, Tcc353xpidTable_t *
				 _pidTableControl)
{
	I32U i;
	I32S slot;
	I32U data;
	I32U idx;
	Tcc353xHandle_t *h;

	h = (Tcc353xHandle_t *)(&Tcc353xHandle[_moduleIndex][0]);

	if (h->numOfPidTable < _pidTableControl->numberOfPid) {
		TcpalPrintErr((I08S *)"[TCC353X] PID count underrun\n");
		return TCC353X_RETURN_FAIL;
	}

	for (i=0; i<_pidTableControl->numberOfPid; i++)	{
		/* check exist pid & get empty slot */
		slot = findPidTable (_moduleIndex, _pidTableControl->Pid[i]);

		if(slot == -1)	{
			TcpalPrintLog((I08S *)"[TCC353X] PID[0x%x] None\n",
				_pidTableControl->Pid[i]);
		} else if (slot>31) {
			TcpalPrintLog((I08S *)"[TCC353X] PID[0x%x] None\n",
				_pidTableControl->Pid[i]);
		} else {
			h->pidTable[slot]= 0x00;
			h->numOfPidTable--;

			TcpalPrintLog((I08S *)"[TCC353X] Remove PID[0x%x]\n",
				_pidTableControl->Pid[i]);
			/* apply pid table to register */
			idx = ((I32U)(slot)>>1);

			if((slot & 0x01) && (slot>0))
				data = h->pidTable[slot-1] | 
					    (h->pidTable[slot]<<16);
			else if (slot<31)
				data = h->pidTable[slot] | 
					   (h->pidTable[slot+1]<<16);
			else
				data = 0;

			Tcc353xMiscWrite(_moduleIndex, 0,
					 MISC_OP_REG_CTRL,
					 (I08U)(TC3XREG_OP_PID00 + idx),
					 data);
		}
	}
	return TCC353X_RETURN_SUCCESS;
}

I32S Tcc353xGetpidTable(I32S _moduleIndex,
			       Tcc353xpidTable_t * _pidTableControl)
{
	I32S i, idx;
	Tcc353xHandle_t * h;

	h = (Tcc353xHandle_t *)(&Tcc353xHandle[_moduleIndex][0]);

	if (Tcc353xHandle[_moduleIndex][0].handleOpen == 0)
		return TCC353X_RETURN_FAIL_INVALID_HANDLE;

	idx = 0;
	TcpalMemset(_pidTableControl, 0x00, sizeof(Tcc353xpidTable_t));
	_pidTableControl->numberOfPid = h->numOfPidTable;
	
	for (i = 0; i < 32; i++) {
		if (h->pidTable[i] & 0x8000) {
			_pidTableControl->Pid[idx] = 
			    (h->pidTable[i] & 0x1FFF);
			idx++;
		}
	}

	return TCC353X_RETURN_SUCCESS;
}
