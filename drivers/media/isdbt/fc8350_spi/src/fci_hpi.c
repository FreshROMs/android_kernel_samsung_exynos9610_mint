/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fci_hpi.c
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
#include "fci_hal.h"

s32 fci_hpi_init(HANDLE handle, DEVICEID devid, s32 speed, s32 slaveaddr)
{
	return BBM_OK;
}

s32 fci_hpi_read(HANDLE handle, DEVICEID devid,
		u8 chip, u8 addr, u8 alen, u8 *data, u8 len)
{
	return bbm_bulk_read(handle, devid, 0xc000 | addr, data, len);
}

s32 fci_hpi_write(HANDLE handle, DEVICEID devid,
		u8 chip, u8 addr, u8 alen, u8 *data, u8 len)
{
	return bbm_bulk_write(handle, devid, 0xc000 | addr, data, len);
}

s32 fci_hpi_deinit(HANDLE handle, DEVICEID devid)
{
	return BBM_OK;
}

