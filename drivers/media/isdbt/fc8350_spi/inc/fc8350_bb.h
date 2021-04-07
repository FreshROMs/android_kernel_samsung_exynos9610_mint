/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350_bb.h
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
#ifndef __FC8350_BB__
#define __FC8350_BB__

#ifdef __cplusplus
extern "C" {
#endif

extern s32 fc8350_reset(HANDLE handle, DEVICEID devid);
extern s32 fc8350_probe(HANDLE handle, DEVICEID devid);
extern s32 fc8350_init(HANDLE handle, DEVICEID devid);
extern s32 fc8350_deinit(HANDLE handle, DEVICEID devid);
extern s32 fc8350_scan_status(HANDLE handle, DEVICEID devid);
extern s32 fc8350_set_broadcast_mode(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast);
extern s32 fc8350_set_core_clk(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast, u32 freq);

#ifdef __cplusplus
}
#endif

#endif /* __FC8350_BB__ */

