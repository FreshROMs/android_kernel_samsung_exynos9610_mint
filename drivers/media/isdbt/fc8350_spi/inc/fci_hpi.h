/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fci_hpi.h
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
#ifndef __FCI_HPI_H__
#define __FCI_HPI_H__

#ifdef __cplusplus
extern "C" {
#endif

extern s32 fci_hpi_init(HANDLE handle, DEVICEID devid,
		s32 speed, s32 slaveaddr);
extern s32 fci_hpi_read(HANDLE handle, DEVICEID devid,
		u8 chip, u8 addr, u8 alen, u8 *data, u8 len);
extern s32 fci_hpi_write(HANDLE handle, DEVICEID devid,
		u8 chip, u8 addr, u8 alen, u8 *data, u8 len);
extern s32 fci_hpi_deinit(HANDLE handle, DEVICEID devid);

#ifdef __cplusplus
}
#endif

#endif /* __FCI_HPI_H__ */

