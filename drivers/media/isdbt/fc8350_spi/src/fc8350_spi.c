/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350_spi.c
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
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "fci_types.h"
#include "fc8350_regs.h"
#include "fci_oal.h"

#define SPI_LEN             0x00 /* or 0x10 */
#define SPI_REG             0x20
#define SPI_THR             0x30
#define SPI_READ            0x40
#define SPI_WRITE           0x00
#define SPI_AINC            0x80

#define DRIVER_NAME "fc8350_spi"
#define TX_DATA_SIZE 36	/* header(4)+PID1(2)+PID2(2)+...+PID16(2) */

struct spi_device *fc8350_spi;
static u8 tx_data[TX_DATA_SIZE];
/* static u8 wdata_buf[32] __cacheline_aligned; */
/* static u8 rdata_buf[65536] __cacheline_aligned; */
static u8 *wdata_buf;
static u8 *rdata_buf;
#ifdef CONFIG_SEC_SPI_BYPASS_DATA
u8 *g_isdbt_ts_buffer;
#endif

static DEFINE_MUTEX(fci_spi_lock);
static int fc8350_spi_probe(struct spi_device *spi)
{
	s32 ret = 0;

	ISDB_PR_ERR("%s spi\n", __func__);

	if (spi == NULL) {
		ISDB_PR_ERR("%s spi == NULL\n", __func__);
		return -1;
	}

	wdata_buf = kzalloc(32, GFP_KERNEL);
	if (wdata_buf == NULL) {
		ISDB_PR_ERR("%s : failed to allocate wdata_buf memory!\n", __func__);
		return -1;
	}

	rdata_buf = kzalloc(65536, GFP_KERNEL);
	if (rdata_buf == NULL) {
		ISDB_PR_ERR("%s : failed to allocate rdata_buf memory!\n", __func__);
		ret = -1;
		goto spi_failed;
	}

#ifdef CONFIG_SEC_SPI_BYPASS_DATA
	g_isdbt_ts_buffer = kzalloc(TS0_32PKT_LENGTH + 4, GFP_KERNEL);
	if (g_isdbt_ts_buffer == NULL) {
		ISDB_PR_ERR("%s : failed to allocate ts_buffer memory!\n", __func__);
		ret = -1;
		goto spi_failed;
	}
#endif

	spi->max_speed_hz = 50000000; /* 52000000 */
	spi->bits_per_word = 8;
	spi->mode =  SPI_MODE_0;

	ret = spi_setup(spi);

	if (ret < 0) {
		ISDB_PR_ERR("%s ERROR ret =%d\n", __func__, ret);
		goto spi_failed;
	}

	fc8350_spi = spi;

	return ret;

spi_failed:
	if (wdata_buf != NULL)
		kfree(wdata_buf);

	if (rdata_buf != NULL)
		kfree(rdata_buf);

#ifdef CONFIG_SEC_SPI_BYPASS_DATA
	if (g_isdbt_ts_buffer != NULL)
		kfree(g_isdbt_ts_buffer);
#endif
	return ret;
}

static int fc8350_spi_remove(struct spi_device *spi)
{
	if (wdata_buf != NULL)
		kfree(wdata_buf);

	if (rdata_buf != NULL)
		kfree(rdata_buf);

#ifdef CONFIG_SEC_SPI_BYPASS_DATA
	if (g_isdbt_ts_buffer != NULL)
		kfree(g_isdbt_ts_buffer);
#endif
	ISDB_PR_ERR("%s\n", __func__);
	return 0;
}

static const struct of_device_id tmm_spi_match_table[] = {
	{   .compatible = "isdbt_spi_comp",
	},
	{}
};

static struct spi_driver fc8350_spi_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = tmm_spi_match_table,
	},
	.probe		= fc8350_spi_probe,
	.remove		= fc8350_spi_remove,
};
#ifdef CONFIG_SEC_SPI_REVERSE_ENDIAN
#define REVERSE32(n) (((((n) & 0xff000000) >> 24) | \
			(((n) & 0xff0000) >> 8)| \
			(((n) & 0xff00) << 8)| \
			(((n) & 0xff) << 24)))

#define REVERSE_ENDIAN(P_BUF, LEN)\
do {\
	int i;\
	int len = (LEN);\
	u32 *ptr = (u32 *)(P_BUF);\
	len = (len + 3) >> 2;\
	for (i = 0; i < len; i++, ptr++)\
		*ptr = REVERSE32(*ptr);\
} while (0);
#endif

static int fc8350_spi_write_then_read(struct spi_device *spi
	, u8 *txbuf, u16 tx_length, u8 *rxbuf, u16 rx_length)
{
	int res = 0;

	struct spi_message	message;
	struct spi_transfer	x;
#ifdef CONFIG_SEC_SPI_REVERSE_ENDIAN
	int reverse_endian = 0;
#endif
	if (spi == NULL) {
		ISDB_PR_ERR("[ERROR] FC8350_SPI Handle Fail...........\n");
		return BBM_NOK;
	}

	spi_message_init(&message);
	memset(&x, 0, sizeof(x));
	spi_message_add_tail(&x, &message);

	memcpy(&wdata_buf[0], txbuf, tx_length);

	x.tx_buf = &wdata_buf[0];
#ifdef CONFIG_SEC_SPI_BYPASS_DATA
	if (rx_length > 188)
		x.rx_buf = rxbuf;
	else
#endif
	x.rx_buf = &rdata_buf[0];
	x.len = tx_length + rx_length;
	x.cs_change = 0;
#if CONFIG_SEC_SPI_BITS_PER_WORD_FOR_DATA > 8
	if (wdata_buf[2] & SPI_THR) {
		x.bits_per_word = CONFIG_SEC_SPI_BITS_PER_WORD_FOR_DATA;
#ifdef CONFIG_SEC_SPI_REVERSE_ENDIAN
		REVERSE_ENDIAN(&wdata_buf[0], tx_length);
		reverse_endian = 1;
#endif
	} else {
		x.bits_per_word = 8;
	}
#endif
	res = spi_sync(spi, &message);


#ifdef CONFIG_SEC_SPI_BYPASS_DATA
	if (rx_length <= 188)
		memcpy(rxbuf, x.rx_buf + tx_length, rx_length);
#else
	memcpy(rxbuf, x.rx_buf + tx_length, rx_length);
#endif

#ifdef CONFIG_SEC_SPI_REVERSE_ENDIAN
	if (reverse_endian)
#ifdef CONFIG_SEC_SPI_BYPASS_DATA
		REVERSE_ENDIAN(&rxbuf[4], rx_length);
#else
		REVERSE_ENDIAN(&rxbuf[0], rx_length);
#endif
#endif
	return res;
}

static s32 spi_bulkread(HANDLE handle, u8 devid,
		u16 addr, u8 command, u8 *data, u16 length)
{
	s32 res;

	tx_data[0] = addr & 0xff;
	tx_data[1] = (addr >> 8) & 0xff;
	tx_data[2] = command | devid;
	tx_data[3] = length & 0xff;

	res = fc8350_spi_write_then_read(fc8350_spi
		, &tx_data[0], 4, data, length);

	if (res) {
		ISDB_PR_ERR("%s fail : %d\n", __func__, res);
		return BBM_NOK;
	}

	return res;
}

static s32 spi_bulkwrite(HANDLE handle, u8 devid,
		u16 addr, u8 command, u8 *data, u16 length)
{
	int i;
	int res;

	tx_data[0] = addr & 0xff;
	tx_data[1] = (addr >> 8) & 0xff;
	tx_data[2] = command | devid;
	tx_data[3] = length & 0xff;

	for (i = 0; i < length; i++) {
		if ((4+i) < TX_DATA_SIZE)
			tx_data[4+i] = data[i];
		else
			ISDB_PR_ERR("%s Error tx_length= %d\n", __func__
				, length);
	}
	res = fc8350_spi_write_then_read(fc8350_spi
		, &tx_data[0], length+4, NULL, 0);

	if (res) {
		ISDB_PR_ERR("%s fail : %d\n", __func__, res);
		return BBM_NOK;
	}

	return res;
}

static s32 spi_dataread(HANDLE handle, u8 devid,
		u16 addr, u8 command, u8 *data, u32 length)
{
	int res;

	tx_data[0] = addr & 0xff;
	tx_data[1] = (addr >> 8) & 0xff;
	tx_data[2] = command | devid;
	tx_data[3] = length & 0xff;

	res = fc8350_spi_write_then_read(fc8350_spi
		, &tx_data[0], 4, data, length);

	if (res) {
		ISDB_PR_ERR("fc8350 %s fail : %d\n", __func__, res);
		return BBM_NOK;
	}

	return res;
}

s32 fc8350_spi_init(HANDLE handle, u16 param1, u16 param2)
{
	int res = 0;

	ISDB_PR_ERR("%s : %d\n", __func__, res);

	res = spi_register_driver(&fc8350_spi_driver);

	if (res) {
		ISDB_PR_ERR("fc8350_spi register fail : %d\n", res);
		return BBM_NOK;
	}

	return res;
}

s32 fc8350_spi_byteread(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
{
	s32 res;
	u8 command = SPI_READ;

	mutex_lock(&fci_spi_lock);
	res = spi_bulkread(handle, (u8) (devid & 0x000f), addr, command,
				data, 1);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_wordread(HANDLE handle, DEVICEID devid, u16 addr, u16 *data)
{
	s32 res;
	u8 command = SPI_READ | SPI_AINC;

	mutex_lock(&fci_spi_lock);
	res = spi_bulkread(handle, (u8) (devid & 0x000f), addr, command,
				(u8 *)data, 2);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_longread(HANDLE handle, DEVICEID devid, u16 addr, u32 *data)
{
	s32 res;
	u8 command = SPI_READ | SPI_AINC;

	mutex_lock(&fci_spi_lock);
	res = spi_bulkread(handle, (u8) (devid & 0x000f), addr, command,
				(u8 *)data, 4);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_bulkread(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u16 length)
{
	s32 res;
	u8 command = SPI_READ | SPI_AINC;

	mutex_lock(&fci_spi_lock);
	res = spi_bulkread(handle, (u8) (devid & 0x000f), addr, command,
				data, length);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_bytewrite(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
{
	s32 res;
	u8 command = SPI_WRITE;

	mutex_lock(&fci_spi_lock);
	if (addr == BBM_DM_DATA) {
		u8 ifcommand;
		u16 ifaddr;
		u8 ifdata;
		if (data != 0) {
			ifcommand = 0xff;
			ifaddr = 0xffff;
			ifdata = 0xff;
		} else {
			ifcommand = 0;
			ifaddr = 0;
			ifdata = 0;
		}
		res = spi_bulkwrite(handle, (u8) (devid & 0x000f), ifaddr
			, ifcommand, &ifdata, 1);
	} else
		res = spi_bulkwrite(handle, (u8) (devid & 0x000f), addr, command,
				(u8 *)&data, 1);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_wordwrite(HANDLE handle, DEVICEID devid, u16 addr, u16 data)
{
	s32 res;
	u8 command = SPI_WRITE | SPI_AINC;

	mutex_lock(&fci_spi_lock);
	res = spi_bulkwrite(handle, (u8) (devid & 0x000f), addr, command,
				(u8 *)&data, 2);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_longwrite(HANDLE handle, DEVICEID devid, u16 addr, u32 data)
{
	s32 res;
	u8 command = SPI_WRITE | SPI_AINC;

	mutex_lock(&fci_spi_lock);
	res = spi_bulkwrite(handle, (u8) (devid & 0x000f), addr, command,
				(u8 *) &data, 4);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_bulkwrite(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u16 length)
{
	s32 res;
	u8 command = SPI_WRITE | SPI_AINC;

	mutex_lock(&fci_spi_lock);
	res = spi_bulkwrite(handle, (u8) (devid & 0x000f), addr, command,
				data, length);
	mutex_unlock(&fci_spi_lock);

	return res;
}

s32 fc8350_spi_dataread(HANDLE handle, DEVICEID devid,
		u16 addr, u8 *data, u32 length)
{
	s32 res;
	u8 command = SPI_READ | SPI_THR;

	mutex_lock(&fci_spi_lock);
	res = spi_dataread(handle, (u8) (devid & 0x000f), addr, command,
				data, length);
	mutex_unlock(&fci_spi_lock);

	ISDB_PR_DBG("fc8350_spi_dataread res = %d, length : %d\n", res, length);
	return res;
}

s32 fc8350_spi_deinit(HANDLE handle)
{
	spi_unregister_driver(&fc8350_spi_driver);
	return BBM_OK;
}

