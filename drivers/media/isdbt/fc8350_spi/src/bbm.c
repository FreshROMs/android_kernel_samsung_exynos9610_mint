/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : bbm.c
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
#include "fci_types.h"
#include "fci_tun.h"
#include "fci_hal.h"
#include "fc8350_bb.h"
#include "fc8350_isr.h"

s32 bbm_com_reset(HANDLE handle, DEVICEID devid)
{
	s32 res;

	res = fc8350_reset(handle, devid);

	return res;
}

s32 bbm_com_probe(HANDLE handle, DEVICEID devid)
{
	s32 res;

	res = fc8350_probe(handle, devid);

	return res;
}

s32 bbm_com_init(HANDLE handle, DEVICEID devid)
{
	s32 res;

	res = fc8350_init(handle, devid);

	return res;
}

s32 bbm_com_deinit(HANDLE handle, DEVICEID devid)
{
	s32 res;

	res = fc8350_deinit(handle, devid);

	return res;
}

s32 bbm_com_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
{
	s32 res;

	res = bbm_read(handle, devid, addr, data);

	return res;
}

s32 bbm_com_byte_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
{
	s32 res;

	res = bbm_byte_read(handle, devid, addr, data);

	return res;
}

s32 bbm_com_word_read(HANDLE handle, DEVICEID devid, u16 addr, u16 *data)
{
	s32 res;

	res = bbm_word_read(handle, devid, addr, data);

	return res;
}

s32 bbm_com_long_read(HANDLE handle, DEVICEID devid, u16 addr, u32 *data)
{
	s32 res;

	res = bbm_long_read(handle, devid, addr, data);

	return res;
}

s32 bbm_com_bulk_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data,
							u16 size)
{
	s32 res;

	res = bbm_bulk_read(handle, devid, addr, data, size);

	return res;
}

s32 bbm_com_data(HANDLE handle, DEVICEID devid, u16 addr, u8 *data, u32 size)
{
	s32 res;

	res = bbm_data(handle, devid, addr, data, size);

	return res;
}

s32 bbm_com_write(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
{
	s32 res;

	res = bbm_write(handle, devid, addr, data);

	return res;
}

s32 bbm_com_byte_write(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
{
	s32 res;

	res = bbm_byte_write(handle, devid, addr, data);

	return res;
}

s32 bbm_com_word_write(HANDLE handle, DEVICEID devid, u16 addr, u16 data)
{
	s32 res;

	res = bbm_word_write(handle, devid, addr, data);

	return res;
}

s32 bbm_com_long_write(HANDLE handle, DEVICEID devid, u16 addr, u32 data)
{
	s32 res;

	res = bbm_long_write(handle, devid, addr, data);

	return res;
}

s32 bbm_com_bulk_write(HANDLE handle, DEVICEID devid, u16 addr, u8 *data,
								u16 size)
{
	s32 res;

	res = bbm_bulk_write(handle, devid, addr, data, size);

	return res;
}

s32 bbm_com_i2c_init(HANDLE handle, u32 type)
{
	s32 res;

	res = tuner_ctrl_select(handle, DIV_BROADCAST, (enum I2C_TYPE) type);

	return res;
}

s32 bbm_com_i2c_deinit(HANDLE handle)
{
	s32 res;

	res = tuner_ctrl_deselect(handle, DIV_BROADCAST);

	return res;
}

s32 bbm_com_tuner_select(HANDLE handle, DEVICEID devid, u32 product,
						u32 brodcast)
{
	s32 res;

	res = tuner_select(handle, devid, product, brodcast);

	return res;
}

s32 bbm_com_tuner_deselect(HANDLE handle, DEVICEID devid)
{
	s32 res;

	res = tuner_deselect(handle, devid);

	return res;
}

s32 bbm_com_tuner_read(HANDLE handle, DEVICEID devid, u8 addr, u8 alen,
		   u8 *buffer, u8 len)
{
	s32 res;

	res = tuner_i2c_read(handle, devid, addr, alen, buffer, len);

	return res;
}

s32 bbm_com_tuner_write(HANDLE handle, DEVICEID devid, u8 addr, u8 alen,
		    u8 *buffer, u8 len)
{
	s32 res;

	res = tuner_i2c_write(handle, devid, addr, alen, buffer, len);

	return res;
}

s32 bbm_com_tuner_set_freq(HANDLE handle, DEVICEID devid, u32 freq, u8 subch)
{
	s32 res;

	res = tuner_set_freq(handle, devid, freq, subch);

	return res;
}

s32 bbm_com_tuner_get_rssi(HANDLE handle, DEVICEID devid, s32 *rssi)
{
	s32 res;

	res = tuner_get_rssi(handle, devid, rssi);

	return res;
}

s32 bbm_com_scan_status(HANDLE handle,  DEVICEID devid)
{
	s32 res;

	res = fc8350_scan_status(handle, devid);

	return res;
}

s32 bbm_com_hostif_select(HANDLE handle, u8 hostif)
{
	s32 res;

	res = bbm_hostif_select(handle, hostif);

	return res;
}

s32 bbm_com_hostif_deselect(HANDLE handle)
{
	s32 res;

	res = bbm_hostif_deselect(handle);

	return res;
}

s32 bbm_com_ts_callback_register(void *userdata,
		s32 (*callback)(void *userdata, u8 bufid, u8 *data, s32 length))
{
	fc8350_ts_user_data = userdata;
	fc8350_ts_callback = callback;

	return BBM_OK;
}

s32 bbm_com_ts_callback_deregister(void)
{
	fc8350_ts_user_data = 0;
	fc8350_ts_callback = NULL;

	return BBM_OK;
}
void bbm_com_isr(HANDLE handle)
{
	fc8350_isr(handle);
}

