/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fci_oal.c
 *
 *	Description : API source of ISDB-T baseband module
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	History :
 *	----------------------------------------------------------------------
 *******************************************************************************/
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "fci_types.h"

void print_log(HANDLE handle, s8 *fmt, ...)
{
	va_list ap;
	char str[256];

	memset(&str[0], 0, sizeof(str));
	va_start(ap, fmt);
	vsprintf(str, fmt, ap);

	pr_err("[ISDBT] %s", str);

	va_end(ap);
}

void msWait(s32 ms)
{
	mdelay(ms);
}

#if 0
/* Write your own mutual exclusion method */
void OAL_CREATE_SEMAPHORE(void)
{
	/* called in driver initialization */
}

void OAL_DELETE_SEMAPHORE(void)
{
	/* called in driver deinitializaton */
}

void OAL_OBTAIN_SEMAPHORE(void)
{

}

void OAL_RELEASE_SEMAPHORE(void)
{

}
#endif

