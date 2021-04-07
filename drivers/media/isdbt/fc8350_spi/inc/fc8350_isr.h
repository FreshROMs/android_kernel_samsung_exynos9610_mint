/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350_isr.h
 *
 *	Description : API header of ISDB-T baseband module
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
 ******************************************************************************/
#ifndef __FC8350_ISR__
#define __FC8350_ISR__

#ifdef __cplusplus
extern "C" {
#endif

extern void *fc8350_ts_user_data;
extern s32 (*fc8350_ts_callback)(void *userdata
	, u8 bufid, u8 *data, s32 length);
extern void fc8350_isr(HANDLE handle);

#ifdef __cplusplus
}
#endif
#endif /* __FC8350_ISR__ */

