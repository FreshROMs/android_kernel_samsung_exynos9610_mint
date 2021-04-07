/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fci_hal.c
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
#include "fc8350_spi.h"
#include "fc8350_i2c.h"

struct IF_PORT {
	s32 (*init)(HANDLE handle, u16 param1, u16 param2);
	s32 (*byteread)(HANDLE handle, DEVICEID devid, u16 addr, u8  *data);
	s32 (*wordread)(HANDLE handle, DEVICEID devid, u16 addr, u16 *data);
	s32 (*longread)(HANDLE handle, DEVICEID devid, u16 addr, u32 *data);
	s32 (*bulkread)(HANDLE handle, DEVICEID devid,
			u16 addr, u8  *data, u16 length);
	s32 (*bytewrite)(HANDLE handle, DEVICEID devid, u16 addr, u8  data);
	s32 (*wordwrite)(HANDLE handle, DEVICEID devid, u16 addr, u16 data);
	s32 (*longwrite)(HANDLE handle, DEVICEID devid, u16 addr, u32 data);
	s32 (*bulkwrite)(HANDLE handle, DEVICEID devid,
			u16 addr, u8 *data, u16 length);
	s32 (*dataread)(HANDLE handle, DEVICEID devid,
			u16 addr, u8 *data, u32 length);
	s32 (*deinit)(HANDLE handle);
};

static struct IF_PORT spiif = {
	&fc8350_spi_init,
	&fc8350_spi_byteread,
	&fc8350_spi_wordread,
	&fc8350_spi_longread,
	&fc8350_spi_bulkread,
	&fc8350_spi_bytewrite,
	&fc8350_spi_wordwrite,
	&fc8350_spi_longwrite,
	&fc8350_spi_bulkwrite,
	&fc8350_spi_dataread,
	&fc8350_spi_deinit
};

static struct IF_PORT i2cif = {
	&fc8350_i2c_init,
	&fc8350_i2c_byteread,
	&fc8350_i2c_wordread,
	&fc8350_i2c_longread,
	&fc8350_i2c_bulkread,
	&fc8350_i2c_bytewrite,
	&fc8350_i2c_wordwrite,
	&fc8350_i2c_longwrite,
	&fc8350_i2c_bulkwrite,
	&fc8350_i2c_dataread,
	&fc8350_i2c_deinit
};

static struct IF_PORT *ifport = &i2cif;
static u8 hostif_type = BBM_I2C;

s32 bbm_hostif_select(HANDLE handle, u8 hostif)
{
	hostif_type = hostif;

	switch (hostif) {
	case BBM_SPI:
		ifport = &spiif;
		break;
	case BBM_I2C:
		ifport = &i2cif;
		break;
	case BBM_SDIO:
		break;
	default:
		return BBM_E_HOSTIF_SELECT;
	}

	if (ifport->init(handle, 0, 0))
		return BBM_E_HOSTIF_INIT;

	return BBM_OK;
}

s32 bbm_hostif_deselect(HANDLE handle)
{
	if (ifport->deinit(handle))
		return BBM_NOK;

	ifport = &spiif;
	hostif_type = BBM_SPI;

	return BBM_OK;
}

s32 bbm_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
{
	if (ifport->byteread(handle, devid, addr, data))
		return BBM_E_BB_READ;
	return BBM_OK;
}

s32 bbm_byte_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
{
	if (ifport->byteread(handle, devid, addr, data))
		return BBM_E_BB_READ;
	return BBM_OK;
}

s32 bbm_word_read(HANDLE handle, DEVICEID devid, u16 addr, u16 *data)
{
	if (ifport->wordread(handle, devid, addr, data))
		return BBM_E_BB_READ;
	return BBM_OK;
}

s32 bbm_long_read(HANDLE handle, DEVICEID devid, u16 addr, u32 *data)
{
	if (ifport->longread(handle, devid, addr, data))
		return BBM_E_BB_READ;
	return BBM_OK;
}

s32 bbm_bulk_read(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u16 length)
{
	if (ifport->bulkread(handle, devid, addr, data, length))
		return BBM_E_BB_READ;
	return BBM_OK;
}

s32 bbm_write(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
{
	if (ifport->bytewrite(handle, devid, addr, data))
		return BBM_E_BB_WRITE;
	return BBM_OK;
}

s32 bbm_byte_write(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
{
	if (ifport->bytewrite(handle, devid, addr, data))
		return BBM_E_BB_WRITE;
	return BBM_OK;
}

s32 bbm_word_write(HANDLE handle, DEVICEID devid, u16 addr, u16 data)
{
	if (ifport->wordwrite(handle, devid, addr, data))
		return BBM_E_BB_WRITE;
	return BBM_OK;
}

s32 bbm_long_write(HANDLE handle, DEVICEID devid, u16 addr, u32 data)
{
	if (ifport->longwrite(handle, devid, addr, data))
		return BBM_E_BB_WRITE;
	return BBM_OK;
}

s32 bbm_bulk_write(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u16 length)
{
	if (ifport->bulkwrite(handle, devid, addr, data, length))
		return BBM_E_BB_WRITE;
	return BBM_OK;
}

s32 bbm_data(HANDLE handle, DEVICEID devid, u16 addr, u8 *data, u32 length)
{
	if (ifport->dataread(handle, devid, addr, data, length))
		return BBM_E_BB_WRITE;
	return BBM_OK;
}

