/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350_i2c.h
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
#ifndef __FC8350_I2C_H__
#define __FC8350_I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

extern s32 fc8350_i2c_init(HANDLE handle, u16 param1, u16 param2);
extern s32 fc8350_i2c_byteread(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data);
extern s32 fc8350_i2c_wordread(HANDLE handle, DEVICEID devid,
		u16 addr, u16 *data);
extern s32 fc8350_i2c_longread(HANDLE handle, DEVICEID devid,
		u16 addr, u32 *data);
extern s32 fc8350_i2c_bulkread(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u16 length);
extern s32 fc8350_i2c_bytewrite(HANDLE handle, DEVICEID devid,
		u16 addr, u8 data);
extern s32 fc8350_i2c_wordwrite(HANDLE handle, DEVICEID devid,
		u16 addr, u16 data);
extern s32 fc8350_i2c_longwrite(HANDLE handle, DEVICEID devid,
		u16 addr, u32 data);
extern s32 fc8350_i2c_bulkwrite(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u16 length);
extern s32 fc8350_i2c_dataread(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u32 length);
extern s32 fc8350_i2c_deinit(HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __FC8350_I2C_H__ */

