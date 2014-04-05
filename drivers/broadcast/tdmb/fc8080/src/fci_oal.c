/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fci_oal.c

	Description : OS Adaptation Layer

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
#include <linux/kernel.h>
#include <linux/delay.h>

#include "../inc/fci_types.h"
#include "../inc/broadcast_fc8080.h"

void print_log(HANDLE handle, char *fmt, ...)
{
	va_list ap;
	char str[256];

	va_start(ap, fmt);
	vsprintf(str, fmt, ap);

	printk(KERN_ERR"%s", str);
	/*printk("%s", str);*/

	va_end(ap);
}

fci_s32 ms_wait(fci_s32 ms)
{
	return tdmb_fc8080_mdelay(ms);
}

void ms_must_wait(fci_s32 ms)
{
	tdmb_fc8080_Must_mdelay(ms);
}

