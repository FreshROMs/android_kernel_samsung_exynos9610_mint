/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/stat.h>
#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/vmalloc.h>

#include "ist40xx.h"
#include "ist40xx_update.h"

int ist40xx_isp_read_burst(struct i2c_client *client, u32 addr, u32 *buf32,
		u16 len)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_READ_SIZE / IST40XX_DATA_LEN;
	u16 remain_len = len;

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len)
			max_len = remain_len;

		ret = ist40xx_read_buf(client, addr, buf32, max_len);
		if (ret) {
			input_err(true, &client->dev, "Burst fail, addr: %x\n",
				  addr);
			return ret;
		}

		buf32 += max_len;
		remain_len -= max_len;
	}

	return 0;
}

int ist40xx_isp_write_burst(struct i2c_client *client, u32 addr, u32 *buf32,
		u16 len)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_WRITE_SIZE / IST40XX_DATA_LEN;
	u16 remain_len = len;

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len)
			max_len = remain_len;

		ret = ist40xx_write_buf(client, addr, buf32, max_len);
		if (ret) {
			input_info(true, &client->dev, "Burst fail, addr: %x\n",
				   addr);
			return ret;
		}

		buf32 += max_len;
		remain_len -= max_len;
	}

	return 0;
}

#define IST40XX_ISP_READ_TOTAL_S	(0x01)
#define IST40XX_ISP_READ_TOTAL_B	(0x11)
#define IST40XX_ISP_READ_MAIN_S		(0x02)
#define IST40XX_ISP_READ_MAIN_B		(0x12)
#define IST40XX_ISP_READ_INFO_S		(0x03)
#define IST40XX_ISP_READ_INFO_B		(0x13)
#define IST40XX_ISP_PROG_TOTAL_S	(0x04)
#define IST40XX_ISP_PROG_TOTAL_B	(0x14)
#define IST40XX_ISP_PROG_MAIN_S		(0x05)
#define IST40XX_ISP_PROG_MAIN_B		(0x15)
#define IST40XX_ISP_PROG_INFO_S		(0x06)
#define IST40XX_ISP_PROG_INFO_B		(0x16)
#define IST40XX_ISP_ERASE_BLOCK		(0x07)
#define IST40XX_ISP_ERASE_SECTOR	(0x08)
#define IST40XX_ISP_ERASE_PAGE		(0x09)
#define IST40XX_ISP_ERASE_INFO		(0x0A)
#define IST40XX_ISP_READ_TOTAL_CRC	(0x1B)
#define IST40XX_ISP_READ_MAIN_CRC	(0x1C)
#define IST40XX_ISP_READ_INFO_CRC	(0x1D)
int ist40xx_set_i2c_32bit(struct ist40xx_data *data)
{
	int ret = 0;
	u32 val = 0x003912D8;   // 32bit mode

	ret = ist40xx_write_buf(data->client, rI2C_CTRL, &val, 1);

	return ret;
}

int ist40xx_isp_enable(struct ist40xx_data *data, bool enable)
{
	int ret = 0;
	u32 val = 0;

	if (enable)
		val = 0xDE01;

	ret = ist40xx_write_buf(data->client, rISP_ISP_EN, &val, 1);
	if (ret)
		return ret;

	if (enable) {
		val = 0xC100;

		ret = ist40xx_write_buf(data->client, rISP_ACCESS_MODE, &val, 1);
		if (ret)
			return ret;

		val = 0x00100059;

		ret = ist40xx_write_buf(data->client, rISP_TMODE1, &val, 1);
		if (ret)
			return ret;
	}

	ist40xx_delay(1);

	return ret;
}

int ist40xx_isp_mode(struct ist40xx_data *data, int mode)
{
	int ret = 0;
	u32 val = 0;

	switch (mode) {
	case IST40XX_ISP_READ_TOTAL_S:
		val = 0x8090;
		break;
	case IST40XX_ISP_READ_TOTAL_B:
	case IST40XX_ISP_READ_TOTAL_CRC:
		val = 0x8190;
		break;
	case IST40XX_ISP_READ_MAIN_S:
		val = 0x0090;
		break;
	case IST40XX_ISP_READ_MAIN_B:
	case IST40XX_ISP_READ_MAIN_CRC:
		val = 0x0190;
		break;
	case IST40XX_ISP_READ_INFO_S:
		val = 0x0098;
		break;
	case IST40XX_ISP_READ_INFO_B:
	case IST40XX_ISP_READ_INFO_CRC:
		val = 0x0198;
		break;
	case IST40XX_ISP_PROG_TOTAL_S:
		val = 0x8050;
		break;
	case IST40XX_ISP_PROG_TOTAL_B:
		val = 0x8150;
		break;
	case IST40XX_ISP_PROG_MAIN_S:
		val = 0x0050;
		break;
	case IST40XX_ISP_PROG_MAIN_B:
		val = 0x0150;
		break;
	case IST40XX_ISP_PROG_INFO_S:
		val = 0x0058;
		break;
	case IST40XX_ISP_PROG_INFO_B:
		val = 0x0158;
		break;
	case IST40XX_ISP_ERASE_BLOCK:
		val = 0x0131;
		break;
	case IST40XX_ISP_ERASE_SECTOR:
		val = 0x0132;
		break;
	case IST40XX_ISP_ERASE_PAGE:
		val = 0x0130;
		break;
	case IST40XX_ISP_ERASE_INFO:
		val = 0x0138;
		break;
	default:
		input_err(true, &data->client->dev, "ISP fail, unknown mode\n");
		return -EINVAL;
	}

	ret = ist40xx_write_buf(data->client, rISP_ACCESS_MODE, &val, 1);
	if (ret) {
		input_err(true, &data->client->dev, "ISP fail, rISP_ACCESS_MODE\n");
		return ret;
	}

	return 0;
}

int ist40xx_isp_erase(struct ist40xx_data *data, int mode, u32 index)
{
	int ret = 0;

	input_info(true, &data->client->dev, "%s\n", __func__);

	ret = ist40xx_isp_mode(data, mode);
	if (ret)
		return ret;

	ret = ist40xx_write_buf(data->client, rISP_DIN, &index, 1);
	if (ret) {
		input_err(true, &data->client->dev, "ISP fail, rISP_DIN\n");
		return ret;
	}

	ist40xx_delay(50); // Flash erase time : Mininum 40msec

	return ret;
}

int ist40xx_read_chksum(struct ist40xx_data *data, u32 start_addr,
		u32 end_addr, u32 *chksum)
{
	int ret = 0;
	u32 val = (1 << 28) | (1 << 25) | (1 << 24);

	val |= (end_addr / IST40XX_ADDR_LEN) - 1;

	ret = ist40xx_isp_mode(data, IST40XX_ISP_READ_MAIN_CRC);
	if (ret)
		return ret;

	ret = ist40xx_write_buf(data->client, rISP_ADDRESS, &start_addr, 1);
	if (ret) {
		input_err(true, &data->client->dev, "ISP fail, rISP_ADDRESS (%x)\n",
			  val);
		return ret;
	}

	ret = ist40xx_write_buf(data->client, rISP_AUTO_READ_CTRL, &val, 1);
	if (ret) {
		input_err(true, &data->client->dev, "ISP fail, rISP_AUTO_READ_CTRL (%x)\n",
			  val);
		return ret;
	}

	ist40xx_delay(100);

	ret = ist40xx_read_reg(data->client, rISP_CRC, chksum);
	if (ret) {
		input_err(true, &data->client->dev,  "ISP fail, rISP_CRC (%x)\n",
			  chksum);
		return ret;
	}

	return 0;
}

int ist40xx_isp_program(struct ist40xx_data *data, u32 addr, int mode,
		u32 *buf32, int size)
{
	int ret = 0;

	input_info(true, &data->client->dev, "%s\n", __func__);

	ret = ist40xx_isp_mode(data, mode);
	if (ret)
		return ret;

	ret = ist40xx_write_buf(data->client, rISP_ADDRESS, &addr, 1);
	if (ret) {
		input_err(true, &data->client->dev, "ISP fail, rISP_ADDRESS\n");
		return ret;
	}

	ret = ist40xx_isp_write_burst(data->client, rISP_DIN, buf32, size);

	if (ret) {
		input_err(true, &data->client->dev, "ISP fail, rISP_DIN\n");
		return ret;
	}

	return ret;
}

int ist40xx_isp_read(struct ist40xx_data *data, u32 addr, int mode, u32 *buf32,
		int size)
{
	int ret = 0;
	int i;
	int len = size / IST40XX_ROM_PAGE_SIZE;

	input_info(true, &data->client->dev, "%s\n", __func__);

	ret = ist40xx_isp_mode(data, mode);
	if (ret)
		return ret;

	addr /= IST40XX_ADDR_LEN;
	for (i = 0; i < len; i++) {
		ret = ist40xx_write_buf(data->client, rISP_ADDRESS, &addr, 1);
		if (ret) {
			input_err(true, &data->client->dev, "ISP fail, rISP_ADDRESS\n");
			return ret;
		}

		ret = ist40xx_isp_read_burst(data->client, rISP_DOUT, buf32,
				IST40XX_ROM_PAGE_SIZE / IST40XX_DATA_LEN);
		if (ret) {
			input_err(true, &data->client->dev, "ISP fail, rISP_DOUT\n");
			return ret;
		}

		addr += (IST40XX_ROM_PAGE_SIZE / IST40XX_ADDR_LEN);
		buf32 += (IST40XX_ROM_PAGE_SIZE / IST40XX_DATA_LEN);
	}

	return ret;
}

int ist40xx_write_sec_info(struct ist40xx_data *data, u8 idx, u32 *buf32,
		int len)
{
	int ret = 0;

	ist40xx_disable_irq(data);

	ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
	if (ret)
		goto err_write_sec_info;

	ret = ist40xx_burst_write(data->client,
			IST40XX_DA_ADDR(data->copy_sec_info_addr) +
			(idx * IST40XX_ADDR_LEN), buf32, len);
	if (ret)
		goto err_write_sec_info;

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_SET_SEC_INFO_WRITE << 16) | IST40XX_ENABLE);
	if (ret)
		goto err_write_sec_info;

	ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
	if (ret)
		goto err_write_sec_info;

	ist40xx_enable_irq(data);

	return 0;

err_write_sec_info:
	ist40xx_reset(data, false);
	ist40xx_start(data);
	ist40xx_enable_irq(data);

	return ret;
}

int ist40xx_read_sec_info(struct ist40xx_data *data, u8 idx, u32 *buf32,
		int len)
{
	int ret = 0;

	ist40xx_disable_irq(data);

	ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
	if (ret)
		goto err_read_sec_info;

	ret = ist40xx_burst_read(data->client,
			IST40XX_DA_ADDR(data->sec_info_addr) + (idx * IST40XX_ADDR_LEN),
			buf32, len, true);
	if (ret)
		goto err_read_sec_info;

	ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
	if (ret)
		goto err_read_sec_info;

	ist40xx_enable_irq(data);

	return 0;

err_read_sec_info:
	ist40xx_reset(data, false);
	ist40xx_start(data);
	ist40xx_enable_irq(data);

	return ret;
}

int ist40xx_isp_fw_read(struct ist40xx_data *data, u32 *buf32)
{
	int ret = 0;
	u32 addr = IST40XX_ROM_BASE_ADDR;

	ist40xx_reset(data, true);
	data->ignore_delay = true;

	ret = ist40xx_isp_enable(data, true);
	if (ret)
		return ret;

	ist40xx_isp_read(data, addr, IST40XX_ISP_READ_MAIN_B, buf32,
			IST40XX_ROM_TOTAL_SIZE);

	data->ignore_delay = false;
	ist40xx_isp_enable(data, false);
	ist40xx_reset(data, false);

	return ret;
}

int ist40xx_ium_read(struct ist40xx_data *data, u32 *buf32)
{
	int ret = 0;
	u32 addr = IST40XX_IUM_BASE_ADDR;

	ist40xx_reset(data, true);
	data->ignore_delay = true;

	ret = ist40xx_isp_enable(data, true);
	if (ret)
		return ret;

	ist40xx_isp_read(data, addr, IST40XX_ISP_READ_INFO_B, buf32,
			IST40XX_IUM_SIZE);

	data->ignore_delay = false;
	ist40xx_isp_enable(data, false);
	ist40xx_reset(data, false);

	return ret;
}

int ist40xx_isp_fw_update(struct ist40xx_data *data, const u8 *buf)
{
	int ret = 0;
	u32 addr = IST40XX_ROM_BASE_ADDR;

	input_info(true, &data->client->dev, "%s\n", __func__);

	ist40xx_reset(data, true);
	data->ignore_delay = true;

	ret = ist40xx_isp_enable(data, true);
	if (ret)
		goto isp_fw_update_end;

	ret = ist40xx_isp_erase(data, IST40XX_ISP_ERASE_BLOCK, 0);
	if (ret)
		goto isp_fw_update_end;

	ist40xx_delay(1);

	ret = ist40xx_isp_program(data, addr, IST40XX_ISP_PROG_MAIN_B, (u32 *)buf,
			IST40XX_ROM_TOTAL_SIZE / IST40XX_DATA_LEN);
	if (ret)
		goto isp_fw_update_end;

isp_fw_update_end:
	data->ignore_delay = false;
	ist40xx_isp_enable(data, false);
	ist40xx_reset(data, false);

	return ret;
}

u32 ist40xx_parse_ver(struct ist40xx_data *data, int flag, const u8 *buf)
{
	u32 ver = 0;
	u32 *buf32 = (u32 *)buf;

	if (flag == FLAG_MAIN)
		ver = (u32)buf32[(data->tags.flag_addr + 0x3FC) >> 2];
	else if (flag == FLAG_TEST)
		ver = (u32)buf32[(data->tags.flag_addr + 0x3F4) >> 2];
	else if (flag == FLAG_FW)
		ver = (u32)buf32[(data->tags.cfg_addr + 0x4) >> 2];
	else
		input_err(true, &data->client->dev, "Parsing ver's flag is not corrent!\n");

	return ver;
}

int ist40xx_miscalib_wait(struct ist40xx_data *data)
{
	int cnt = CALIB_WAIT_TIME;
	u32 len = 0;
	TSP_INFO *tsp = &data->tsp_info;

	data->status.miscalib_msg = 0;

	while (cnt-- > 0) {
		ist40xx_delay(100);

		if (data->status.miscalib_msg) {
			input_info(true, &data->client->dev,
				   "miscalibration complete(0x%08X, %d)\n",
				   data->status.miscalib_msg, data->status.miscalib_result);

			len = (tsp->node.len * sizeof(u16)) / IST40XX_DATA_LEN;

			if ((tsp->node.len * sizeof(u16)) % IST40XX_DATA_LEN)
				len += 1;

			ist40xx_burst_read(data->client, IST40XX_DA_ADDR(data->miscal_addr),
					(u32*)&tsp->node.miscal, len, true);

			return 0;
		}
	}

	input_err(true, &data->client->dev, "miscalibration time out\n");

	return -EPERM;
}

int ist40xx_miscalibrate(struct ist40xx_data *data)
{
	int ret = -ENOEXEC;

	input_info(true, &data->client->dev, "*** Miscalibrate %ds ***\n",
		   CALIB_WAIT_TIME / 10);

	data->status.miscalib = 1;

	mutex_lock(&data->lock);

	ist40xx_reset(data, false);

	ist40xx_cmd_miscalibrate(data);

	ret = ist40xx_miscalib_wait(data);

	ist40xx_reset(data, false);
	ist40xx_start(data);
	mutex_unlock(&data->lock);

	data->status.miscalib = 0;

	return ret;
}

int ist40xx_calib_wait(struct ist40xx_data *data)
{
	int cnt = CALIB_WAIT_TIME;

	memset(data->status.calib_msg, 0, sizeof(u32) * IST40XX_MAX_CALIB_SIZE);

	while (cnt-- > 0) {
		ist40xx_delay(100);

		if (data->status.calib_msg[0] && data->status.calib_msg[1] &&
				data->status.calib_msg[2]) {
			input_info(true, &data->client->dev,
				   "SLF Calibration status  : %d, Max gap : %d - (%08x)\n",
				   CALIB_TO_STATUS(data->status.calib_msg[0]),
				   CALIB_TO_GAP(data->status.calib_msg[0]),
				   data->status.calib_msg[0]);

			input_info(true, &data->client->dev,
				   "MTL Calibration status  : %d, Max gap : %d - (%08x)\n",
				   CALIB_TO_STATUS(data->status.calib_msg[1]),
				   CALIB_TO_GAP(data->status.calib_msg[1]),
				   data->status.calib_msg[1]);

			input_info(true, &data->client->dev,
				   "PROX Calibration status : %d, Max gap : %d - (%08x)\n",
				   CALIB_TO_STATUS(data->status.calib_msg[2]),
				   CALIB_TO_GAP(data->status.calib_msg[2]),
				   data->status.calib_msg[2]);

			if ((CALIB_TO_STATUS(data->status.calib_msg[0]) == 0) &&
					(CALIB_TO_STATUS(data->status.calib_msg[1]) == 0) &&
					(CALIB_TO_STATUS(data->status.calib_msg[2]) == 0))
				return 0;
			else
				return -EAGAIN;
		}
	}

	input_err(true, &data->client->dev, "Calibration time out\n");

	return -EPERM;
}

int ist40xx_calibrate(struct ist40xx_data *data, int wait_cnt)
{
	int ret = -ENOEXEC;

	input_info(true, &data->client->dev, "*** Calibrate %ds ***\n",
		   CALIB_WAIT_TIME / 10);

	data->status.calib = 1;
	ist40xx_disable_irq(data);

	while (1) {
		ret = ist40xx_cmd_calibrate(data);
		if (ret)
			continue;

		ist40xx_enable_irq(data);

		ret = ist40xx_calib_wait(data);
		if (!ret)
			break;

		ist40xx_disable_irq(data);

		if (--wait_cnt == 0)
			break;

		ist40xx_reset(data, false);
	}

	ist40xx_disable_irq(data);
	ist40xx_reset(data, false);
	ist40xx_enable_irq(data);

	data->status.calib = 0;

	return ret;
}

int ist40xx_parse_tags(struct ist40xx_data *data, const u8 *buf, const u32 size)
{
	int ret = -EPERM;
	struct ist40xx_tags *tags;

	tags = (struct ist40xx_tags *)(&buf[size - sizeof(struct ist40xx_tags)]);

	if (!strncmp(tags->magic1, IST40XX_TAG_MAGIC, sizeof(tags->magic1)) &&
			!strncmp(tags->magic2, IST40XX_TAG_MAGIC, sizeof(tags->magic2))) {
		data->tags = *tags;

		data->tags.fw_addr -= data->tags.rom_base;
		data->tags.cfg_addr -= data->tags.rom_base;
		data->tags.sensor_addr -= data->tags.rom_base;
		data->tags.flag_addr -= data->tags.rom_base;

		data->fw.index = data->tags.fw_addr;
		data->fw.size = tags->flag_addr - tags->fw_addr + tags->flag_size;
		data->fw.chksum = tags->chksum;

		tsp_verb("Tagts magic1: %s, magic2: %s\n",
			  data->tags.magic1, data->tags.magic2);
		tsp_verb(" rom: %x\n", data->tags.rom_base);
		tsp_verb(" ram: %x\n", data->tags.ram_base);
		tsp_verb(" fw: %x(%x)\n", data->tags.fw_addr, data->tags.fw_size);
		tsp_verb(" cfg: %x(%x)\n", data->tags.cfg_addr, data->tags.cfg_size);
		tsp_verb(" sensor: %x(%x)\n",
			  data->tags.sensor_addr, data->tags.sensor_size);
		tsp_verb(" flag: %x(%x)\n", data->tags.flag_addr, data->tags.flag_size);
		tsp_verb(" chksum: %x\n", data->tags.chksum);
		tsp_verb(" chksum_all: %x\n", data->tags.chksum_all);
		tsp_verb(" build time: %04d/%02d/%02d (%02d:%02d:%02d)\n",
			  data->tags.year, data->tags.month, data->tags.day,
			  data->tags.hour, data->tags.min, data->tags.sec);

		ret = 0;
	}

	return ret;
}

int ist40xx_get_update_info(struct ist40xx_data *data, const u8 *buf,
		const u32 size)
{
	int ret;

	ret = ist40xx_parse_tags(data, buf, size);
	if (ret)
		input_err(true, &data->client->dev, "Cannot find tags of F/W\n");

	return ret;
}

#define TSP_INFO_SWAP_XY	(1 << 0)
#define TSP_INFO_FLIP_X		(1 << 1)
#define TSP_INFO_FLIP_Y		(1 << 2)
u32 ist40xx_info_cal_crc(u32 *buf)
{
	int i;
	u32 chksum32 = 0;

	for (i = 0; i < IST40XX_MAX_CMD_SIZE - 1; i++)
		chksum32 += *buf++;

	return chksum32;
}

int ist40xx_tsp_update_info(struct ist40xx_data *data)
{
	int ret = 0;
	u32 chksum;
	u32 info[IST40XX_MAX_CMD_SIZE];
	u32 tsp_lcd, tsp_swap, tsp_scr, tsp_ch;
	u32 prox_th, baseline_th;
	u32 recording_info;
	u32 algo_info;
	TSP_INFO *tsp = &data->tsp_info;

	data->fw.cur.main_ver = 0;
	data->fw.cur.fw_ver = 0;
	data->fw.cur.test_ver = 0;

	ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
	if (ret)
		return ret;

	ret = ist40xx_burst_read(data->client, IST40XX_DA_ADDR(eHCOM_GET_CHIP_ID),
			&info[0], IST40XX_MAX_CMD_SIZE, true);
	if (ret)
		return ret;

	ret = ist40xx_read_cmd(data, rSYS_CHIPID, &data->chip_id);
	if (ret)
		return ret;

	ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
	if (ret)
		return ret;

	if ((info[IST40XX_CMD_VALUE(eHCOM_GET_CHIP_ID)] != data->chip_id) ||
			(info[IST40XX_CMD_VALUE(eHCOM_GET_CHIP_ID)] == 0) ||
			(info[IST40XX_CMD_VALUE(eHCOM_GET_CHIP_ID)] == 0xFFFF))
		return -EINVAL;

	chksum = ist40xx_info_cal_crc((u32 *)info);
	if (chksum != info[IST40XX_MAX_CMD_SIZE - 1]) {
		input_err(true, &data->client->dev, "info checksum : %08X, %08X\n",
			  chksum,
			  info[IST40XX_MAX_CMD_SIZE - 1]);
		return -EINVAL;
	}

	input_info(true, &data->client->dev, "info read success\n");

	data->fw.cur.main_ver = info[IST40XX_CMD_VALUE(eHCOM_GET_VER_MAIN)];
	data->fw.cur.fw_ver = info[IST40XX_CMD_VALUE(eHCOM_GET_VER_FW)];
	data->fw.cur.test_ver = info[IST40XX_CMD_VALUE(eHCOM_GET_VER_TEST)];
	tsp_lcd = info[IST40XX_CMD_VALUE(eHCOM_GET_LCD_INFO)];
	tsp_ch = info[IST40XX_CMD_VALUE(eHCOM_GET_TSP_INFO)];
	tsp_scr = info[IST40XX_CMD_VALUE(eHCOM_GET_SCR_INFO)];
	tsp_swap = info[IST40XX_CMD_VALUE(eHCOM_GET_SWAP_INFO)];
	baseline_th = info[IST40XX_CMD_VALUE(eHCOM_GET_BASELINE_TH)];
	recording_info = info[IST40XX_CMD_VALUE(eHCOM_GET_REC_INFO_BASE)];
	algo_info = info[IST40XX_CMD_VALUE(eHCOM_GET_ALGOINFO)];
	prox_th = info[IST40XX_CMD_VALUE(eHCOM_GET_PROXIMITY_TH)];

	data->gap_spec = info[IST40XX_CMD_VALUE(eHCOM_GET_GAP_SPEC)] & 0xFFFF;

	data->miscal_addr = info[IST40XX_CMD_VALUE(eHCOM_GET_MISCAL_BASE)];
	data->selftest_tx_addr =
		info[IST40XX_CMD_VALUE(eHCOM_GET_SELFTEST_TX_BASE)];
	data->selftest_rx_addr = data->selftest_tx_addr + 0x8;
	data->selftest_addr = info[IST40XX_CMD_VALUE(eHCOM_GET_SELFTEST_BASE)];
	data->prox_cp_addr = info[IST40XX_CMD_VALUE(eHCOM_GET_PROX_CP_BASE)];
	data->prox_cdc_addr = info[IST40XX_CMD_VALUE(eHCOM_GET_PROX_CDC_BASE)];
	data->sec_info_addr = info[IST40XX_CMD_VALUE(eHCOM_GET_SEC_INFO_BASE)];
	data->copy_sec_info_addr =
		info[IST40XX_CMD_VALUE(eHCOM_GET_COPY_SEC_INFO_BASE)];

	data->self_cdc_addr = rMEMX_CDC_SLF;
	data->cdc_addr = rMEMX_CDC_MTL;

	data->self_cp_addr = rMEMX_CPC_SLF;
	data->cp_addr = rMEMX_CPC_MTL;

	data->rec_addr = recording_info & 0xFFFF;
	data->rec_size = (recording_info >> 16) & 0xFFFF;

	data->algo_addr = algo_info & 0xFFFF;
	data->algo_size = (algo_info >> 16) & 0xFFFF;

	tsp->ch_num.rx = (tsp_ch >> 16) & 0xFFFF;
	tsp->ch_num.tx = tsp_ch & 0xFFFF;

	tsp->node.len = tsp->ch_num.tx * tsp->ch_num.rx;
	tsp->node.self_len = tsp->ch_num.tx + tsp->ch_num.rx;

	tsp->dir.swap_xy = (tsp_swap & TSP_INFO_SWAP_XY ? true : false);
	tsp->dir.flip_x = (tsp_swap & TSP_INFO_FLIP_X ? true : false);
	tsp->dir.flip_y = (tsp_swap & TSP_INFO_FLIP_Y ? true : false);

	tsp->prox_tx_threshold = (prox_th >> 16) & 0xFFFF;
	tsp->prox_rx_threshold = prox_th & 0xFFFF;

	tsp->baseline = (baseline_th >> 16) & 0xFFFF;
	tsp->threshold = baseline_th & 0xFFFF;

	tsp->screen.rx = (tsp_scr >> 16) & 0xFFFF;
	tsp->screen.tx = tsp_scr & 0xFFFF;

	if (tsp->dir.swap_xy) {
		tsp->width = tsp_lcd & 0xFFFF;
		tsp->height = (tsp_lcd >> 16) & 0xFFFF;
	} else {
		tsp->width = (tsp_lcd >> 16) & 0xFFFF;
		tsp->height = tsp_lcd & 0xFFFF;
	}

	return ret;
}

int ist40xx_get_tsp_info(struct ist40xx_data *data)
{
	int ret = 0;
	int retry = 3;

	while (retry--) {
		ret = ist40xx_tsp_update_info(data);
		if (ret == 0) {
			input_info(true, &data->client->dev, "tsp update info success!\n");
			return ret;
		}

		ist40xx_reset(data, false);
	}

	return ret;
}

void ist40xx_print_info(struct ist40xx_data *data)
{
	TSP_INFO *tsp = &data->tsp_info;

	input_info(true, &data->client->dev, "*** TSP info ***\n");
	input_info(true, &data->client->dev, " dir swap: %d, flip x: %d, y: %d\n",
			tsp->dir.swap_xy, tsp->dir.flip_x, tsp->dir.flip_y);
	input_info(true, &data->client->dev, " threshold: %d, baseline: %d\n",
			tsp->threshold, tsp->baseline);
	input_info(true, &data->client->dev," prox threshold tx : %d, rx: %d\n", 
			tsp->prox_tx_threshold, tsp->prox_rx_threshold);
	input_info(true, &data->client->dev, " ch_num tx: %d, rx: %d\n",
			tsp->ch_num.tx, tsp->ch_num.rx);
	input_info(true, &data->client->dev, " screen tx: %d, rx: %d\n",
			tsp->screen.tx, tsp->screen.rx);
	input_info(true, &data->client->dev, " width: %d, height: %d\n",
			tsp->width, tsp->height);
	input_info(true, &data->client->dev, " node len: %d\n", tsp->node.len);
	input_info(true, &data->client->dev, " self node len: %d\n",
			tsp->node.self_len);
	input_info(true, &data->client->dev, " miscal addr: 0x%08X\n",
			data->miscal_addr);
	input_info(true, &data->client->dev, " selftest addr: 0x%08X\n",
			data->selftest_addr);
	input_info(true, &data->client->dev, " selftest tx addr: 0x%08X\n",
			data->selftest_tx_addr);
	input_info(true, &data->client->dev, " selftest rx addr: 0x%08X\n",
			data->selftest_rx_addr);
	input_info(true, &data->client->dev, " prox cp addr: 0x%08X\n",
			data->prox_cp_addr);
	input_info(true, &data->client->dev, " prox cdc addr: 0x%08X\n",
			data->prox_cdc_addr);
	input_info(true, &data->client->dev, " sec info addr: 0x%08X\n",
			data->sec_info_addr);
	input_info(true, &data->client->dev, " copy sec info addr: 0x%08X\n",
			data->copy_sec_info_addr);
	input_info(true, &data->client->dev,
			"IC version main: %x, fw: %x, test: %x\n",
			data->fw.cur.main_ver, data->fw.cur.fw_ver, data->fw.cur.test_ver);
}

#define update_next_step(ret)	{ if (ret) goto end; }
int ist40xx_fw_update(struct ist40xx_data *data, const u8 *buf, int size)
{
	int ret = 0;
	u32 chksum = 0;
	struct ist40xx_fw *fw = &data->fw;
	u32 main_ver = ist40xx_parse_ver(data, FLAG_MAIN, buf);
	u32 fw_ver = ist40xx_parse_ver(data, FLAG_FW, buf);
	u32 test_ver = ist40xx_parse_ver(data, FLAG_TEST, buf);

	input_info(true, &data->client->dev, "*** Firmware update ***\n");
	input_info(true, &data->client->dev,
		   " main: %x, fw: %x, test: %x(addr: 0x%x ~ 0x%x)\n",
		   main_ver, fw_ver, test_ver, fw->index, (fw->index + fw->size));

	data->status.update = 1;
	data->status.update_result = 0;

	ist40xx_disable_irq(data);

	ret = ist40xx_isp_fw_update(data, buf);
	update_next_step(ret);

	ret = ist40xx_read_cmd(data, eHCOM_GET_CRC32, &chksum);
	if ((ret) || (chksum != fw->chksum)) {
		if (ret)
			ist40xx_reset(data, false);

		goto end;
	}

	ret = ist40xx_get_tsp_info(data);
	update_next_step(ret);

end:
	if (ret) {
		data->status.update_result = 1;

		input_err(true, &data->client->dev, "Firmware update Fail!, ret=%d\n",
			  ret);
	} else if (chksum != fw->chksum) {
		data->status.update_result = 1;

		input_err(true, &data->client->dev, "Error CheckSum: %x(%x)\n",
			  chksum, fw->chksum);
		ret = -ENOEXEC;
	}

	ist40xx_enable_irq(data);

	data->status.update = 2;

	return ret;
}

int ist40xx_fw_recovery(struct ist40xx_data *data)
{
	int ret = -EPERM;
	u8 *fw = data->fw.buf;
	int fw_size = data->fw.buf_size;
#ifdef TCLM_CONCEPT
	int rc;
#endif

	if (data->dt_data->bringup == 1) {
		input_info(true, &data->client->dev, "%s skip (bringup 1)\n", __func__);
		return ret;
	}

	ret = ist40xx_get_update_info(data, fw, fw_size);
	if (ret) {
		data->status.update_result = 1;
		return ret;
	}

	data->fw.bin.main_ver = ist40xx_parse_ver(data, FLAG_MAIN, fw);
	data->fw.bin.fw_ver = ist40xx_parse_ver(data, FLAG_FW, fw);
	data->fw.bin.test_ver = ist40xx_parse_ver(data, FLAG_TEST, fw);

	mutex_lock(&data->lock);
	ret = ist40xx_fw_update(data, fw, fw_size);
	if (ret == 0) {
		ist40xx_print_info(data);
#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(data->tdata, CALPOSITION_TESTMODE);

		rc = sec_execute_tclm_package(data->tdata, 0);
		if (rc < 0) {
			input_err(true, &data->client->dev,
				  "%s: sec_execute_tclm_package fail\n", __func__);
		}

		sec_tclm_root_of_cal(data->tdata, CALPOSITION_NONE);
#else
		ist40xx_calibrate(data, 1);
#endif
	}
	mutex_unlock(&data->lock);

	ist40xx_start(data);

	return ret;
}

#define MAIN_VER_MASK		0xFF000000
int ist40xx_check_auto_update(struct ist40xx_data *data)
{
	int ret = 0;
	int retry = IST40XX_MAX_RETRY_CNT;
	u32 chip_id = 0;
	bool tsp_check = false;
	u32 chksum;
	struct ist40xx_fw *fw = &data->fw;

	while (retry--) {
		ist40xx_delay(100);
		ret = ist40xx_read_cmd(data, eHCOM_GET_CHIP_ID, &chip_id);
		if (ret == 0) {
			if (chip_id == IST40XX_CHIP_ID) {
				tsp_check = true;
				break;
			}
		}
		ist40xx_reset(data, false);
	}

	if (!tsp_check) {
		input_err(true, &data->client->dev, "%s fail to id check\n",
			  __func__);
		goto fw_check_end;
	}

	ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_FW_HOLD << 16) | (IST40XX_ENABLE & 0xFFFF));
	ist40xx_delay(20);

	ret = ist40xx_get_tsp_info(data);
	if (ret) {
		input_err(true, &data->client->dev, "%s fail to get tsp info\n",
			  __func__);
		goto fw_check_end;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_FW_HOLD << 16) | (IST40XX_DISABLE & 0xFFFF));
	if (ret) {
		input_err(true, &data->client->dev, "%s fail to disable hold\n",
			  __func__);

		ist40xx_reset(data, false);
	}

	if ((fw->cur.fw_ver > 0) && (fw->cur.fw_ver < 0xFFFFFFFF)) {
		if (((fw->cur.main_ver & MAIN_VER_MASK) == MAIN_VER_MASK) ||
				((fw->cur.main_ver & MAIN_VER_MASK) == 0))
			goto fw_check_end;

		input_info(true, &data->client->dev,
			   "Version compare IC: %x(%x), BIN: %x(%x)\n", fw->cur.fw_ver,
			   fw->cur.main_ver, fw->bin.fw_ver, fw->bin.main_ver);

		/* If FW version is same, check FW checksum */
		if ((fw->cur.main_ver == fw->bin.main_ver) &&
				(fw->cur.fw_ver == fw->bin.fw_ver) && (fw->cur.test_ver == 0)) {
			ret = ist40xx_read_cmd(data, eHCOM_GET_CRC32, &chksum);
			if ((ret) || (chksum != fw->chksum)) {
				data->checksum_result = 1;

				input_err(true, &data->client->dev,
					  "Checksum error, IC: %x, Bin: %x (ret: %d)\n",
					  chksum, fw->chksum, ret);
				goto fw_check_end;
			}
		}

		/*
		 *  fw->cur.main_ver : Main version in TSP IC
		 *  fw->cur.fw_ver : FW version if TSP IC
		 *  fw->bin.main_ver : Main version in FW Binary
		 *  fw->bin.fw_ver : FW version in FW Binary
		 */
		/* If the ver of binary is higher than ver of IC, FW update operate. */

		if ((fw->cur.main_ver >= fw->bin.main_ver) &&
				(fw->cur.fw_ver >= fw->bin.fw_ver))
			return 0;
	}

fw_check_end:
	return -EAGAIN;
}

int ist40xx_auto_bin_update(struct ist40xx_data *data)
{
	int ret = 0;
	int retry = IST40XX_MAX_RETRY_CNT;
	const struct firmware *firmware = NULL;
	struct ist40xx_fw *fw = &data->fw;
#ifdef TCLM_CONCEPT
	int restore_cal = 0;
	int rc = 0;
#endif

	if (data->dt_data->bringup == 1) {
		input_info(true, &data->client->dev, "%s skip (bringup 1)\n", __func__);
		return true;
	}

	ret = request_firmware(&firmware, data->dt_data->fw_path,
			&data->client->dev);
	if (ret) {
		input_err(true, &data->client->dev, "do not request firmware: %d\n",
			  ret);
		return false;
	}

	fw->buf = (u8 *)kmalloc((int)firmware->size, GFP_KERNEL);
	if (!fw->buf) {
		release_firmware(firmware);

		input_err(true, &data->client->dev, "Error allocating memory for firmware.\n");
		return false;
	}

	memcpy(fw->buf, firmware->data, (int)firmware->size);
	fw->buf_size = (u32)firmware->size;

	release_firmware(firmware);

	input_info(true, &data->client->dev, "Firmware %s loaded successfully.\n",
		   data->dt_data->fw_path);

	ret = ist40xx_get_update_info(data, fw->buf, fw->buf_size);
	if (ret)
		return false;

	fw->bin.main_ver = ist40xx_parse_ver(data, FLAG_MAIN, fw->buf);
	fw->bin.fw_ver = ist40xx_parse_ver(data, FLAG_FW, fw->buf);
	fw->bin.test_ver = ist40xx_parse_ver(data, FLAG_TEST, fw->buf);

	input_info(true, &data->client->dev,
		   "IC: %x, Binary ver main: %x, fw: %x, test: %x\n",
		   data->chip_id, fw->bin.main_ver, fw->bin.fw_ver, fw->bin.test_ver);

	mutex_lock(&data->lock);
	ret = ist40xx_check_auto_update(data);
	mutex_unlock(&data->lock);

	/* ret < 0 force update */
	if (ret >= 0) { /* don't firmup case */
		ret = true;
		goto end_update;
	}

	input_info(true, &data->client->dev,
		  "Update version. fw(main, test): %x(%x, %x)-> %x(%x, %x)\n",
		  fw->cur.fw_ver, fw->cur.main_ver, fw->cur.test_ver,
		  fw->bin.fw_ver, fw->bin.main_ver, fw->bin.test_ver);

	mutex_lock(&data->lock);
	while (retry--) {
		ret = ist40xx_fw_update(data, fw->buf, fw->buf_size);
		if (!ret)
			break;
	}
	mutex_unlock(&data->lock);

	if (ret) {
		ret = false;
		goto end_update;
	}

	ret = true;

#ifdef TCLM_CONCEPT
	retry = IST40XX_MAX_RETRY_CNT;
	while (retry--) {
		rc = data->tdata->tclm_read(data->client, SEC_TCLM_NVM_ALL_DATA);
		if (rc >= 0)
			break;
	}
	if (rc < 0) {
		input_err(true, &data->client->dev,
			  "%s: SEC_TCLM_NVM_ALL_DATA i2c read fail\n", __func__);
		goto end_update;
	}

	input_info(true, &data->client->dev, "%s: tune_fix_ver [%04X] afe_base [%04X]\n",
		  __func__, data->tdata->nvdata.tune_fix_ver, data->tdata->afe_base);

	if ((data->tdata->tclm_level > TCLM_LEVEL_CLEAR_NV) &&
			((data->tdata->nvdata.tune_fix_ver == 0xffff) ||
			 (data->tdata->afe_base > data->tdata->nvdata.tune_fix_ver))) {
			/* tune version up case */
		sec_tclm_root_of_cal(data->tdata, CALPOSITION_TUNEUP);
		restore_cal = 1;
	} else if (data->tdata->tclm_level == TCLM_LEVEL_CLEAR_NV) {
		/* firmup case */
		sec_tclm_root_of_cal(data->tdata, CALPOSITION_FIRMUP);
		restore_cal = 1;
	}

	if (restore_cal == 1) {
		input_err(true, &data->client->dev, "%s: RUN OFFSET CALIBRATION\n",
			  __func__);

		rc = sec_execute_tclm_package(data->tdata, 0);
		if (rc < 0) {
			input_err(true, &data->client->dev,
				  "%s: sec_execute_tclm_package fail\n", __func__);
		}
	}
	sec_tclm_root_of_cal(data->tdata, CALPOSITION_NONE);
#else
	mutex_lock(&data->lock);
	ist40xx_calibrate(data, IST40XX_MAX_RETRY_CNT);
	mutex_unlock(&data->lock);
#endif

end_update:

	return ret;
}

#define MAX_FILE_PATH		255
/* sysfs: /sys/class/touch/firmware/firmware */
ssize_t ist40xx_fw_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret;
	int fw_size = 0;
	u8 *fw = NULL;
	u8 *fwbuf = NULL;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FILE_PATH];
	const struct firmware *request_fw = NULL;
	int mode = 0;
	int calib = 1;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d %d", &mode, &calib);

	switch (mode) {
	case MASK_UPDATE_INTERNAL:
		fw = data->fw.buf;
		fw_size = data->fw.buf_size;
		break;

	case MASK_UPDATE_FW:
		ret = request_firmware(&request_fw, IST40XX_FW_NAME,
				&data->client->dev);
		if (ret) {
			data->status.update_result = 1;
			tsp_warn("File not found, %s\n", IST40XX_FW_NAME);
			return size;
		}

		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
		tsp_info("firmware is loaded!!\n");
		break;

	case MASK_UPDATE_SDCARD:
		fwbuf = vzalloc(IST40XX_ROM_TOTAL_SIZE + sizeof(struct ist40xx_tags));
		if (!fwbuf) {
			data->status.update_result = 1;
			tsp_info("failed to fwbuf allocate\n");
			goto err_alloc;
		}

		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST40XX_FW_NAME);
		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			data->status.update_result = 1;
			tsp_info("file %s open error:%d\n", fw_path, PTR_ERR(fp));
			goto err_file_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;

		if ((IST40XX_ROM_TOTAL_SIZE + sizeof(struct ist40xx_tags)) != fsize) {
			data->status.update_result = 1;
			tsp_info("mismatch fw size\n");
			goto err_fw_size;
		}

		nread = vfs_read(fp, (char __user *)fwbuf, fsize, &fp->f_pos);
		if (nread != fsize) {
			data->status.update_result = 1;
			tsp_info("mismatch fw size2\n");
			goto err_fw_size;
		}

		fw = (u8 *)fwbuf;
		fw_size = (u32)fsize;

		filp_close(fp, current->files);
		tsp_info("firmware is loaded!!\n");
		break;

	case MASK_UPDATE_ERASE:
		tsp_info("EEPROM all erase!!\n");
		mutex_lock(&data->lock);
		ist40xx_disable_irq(data);
		ist40xx_reset(data, true);
		data->ignore_delay = true;
		ist40xx_isp_enable(data, true);
		ist40xx_isp_erase(data, IST40XX_ISP_ERASE_BLOCK, 0);
		ist40xx_isp_enable(data, false);
		data->ignore_delay = false;
		ist40xx_reset(data, false);
		ist40xx_start(data);
		ist40xx_enable_irq(data);
		mutex_unlock(&data->lock);

	default:
		return size;
	}

	ret = ist40xx_get_update_info(data, fw, fw_size);
	if (ret) {
		data->status.update_result = 1;
		goto err_get_info;
	}

	mutex_lock(&data->lock);
	ret = ist40xx_fw_update(data, fw, fw_size);
	if (ret == 0) {
		ist40xx_print_info(data);
#ifdef TCLM_CONCEPT
		if (calib) {
			sec_tclm_root_of_cal(data->tdata, CALPOSITION_TESTMODE);
			sec_execute_tclm_package(data->tdata, 0);
			sec_tclm_root_of_cal(data->tdata, CALPOSITION_NONE);
		}
#else
		if (calib)
			ist40xx_calibrate(data, 1);
#endif
	}
	mutex_unlock(&data->lock);

	ist40xx_start(data);

err_get_info:
	if (request_fw != NULL)
		release_firmware(request_fw);

	if (fp) {
err_fw_size:
		filp_close(fp, NULL);
err_file_open:
		set_fs(old_fs);
	}

	if (fwbuf)
		vfree(fwbuf);
err_alloc:

	return size;
}

/* sysfs: /sys/class/touch/firmware/fw_sdcard */
ssize_t ist40xx_fw_sdcard_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	u8 *fwbuf = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FILE_PATH];
	struct ist40xx_data *data = dev_get_drvdata(dev);

	fwbuf = vzalloc(IST40XX_ROM_TOTAL_SIZE + sizeof(struct ist40xx_tags));
	if (!fwbuf) {
		data->status.update_result = 1;
		tsp_info("failed to fwbuf allocate\n");
		goto err_alloc;
	}

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST40XX_FW_NAME);
	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		data->status.update_result = 1;
		tsp_info("file %s open error:%d\n", fw_path, PTR_ERR(fp));
		goto err_file_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	if ((IST40XX_ROM_TOTAL_SIZE + sizeof(struct ist40xx_tags)) != fsize) {
		data->status.update_result = 1;
		tsp_info("mismatch fw size\n");
		goto err_fw_size;
	}

	nread = vfs_read(fp, (char __user *)fwbuf, fsize, &fp->f_pos);
	if (nread != fsize) {
		data->status.update_result = 1;
		tsp_info("mismatch fw size2\n");
		goto err_fw_size;
	}

	filp_close(fp, current->files);
	tsp_info("firmware is loaded!!\n");

	ret = ist40xx_get_update_info(data, fwbuf, fsize);
	if (ret) {
		data->status.update_result = 1;
		goto err_get_info;
	}

	mutex_lock(&data->lock);
	ret = ist40xx_fw_update(data, fwbuf, fsize);
	if (ret == 0)
		ist40xx_print_info(data);
	mutex_unlock(&data->lock);

	ist40xx_start(data);

err_get_info:
err_fw_size:
	filp_close(fp, NULL);
err_file_open:
	set_fs(old_fs);
	vfree(fwbuf);
err_alloc:

	return 0;
}

/* sysfs: /sys/class/touch/firmware/firmware */
ssize_t ist40xx_fw_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	switch (data->status.update) {
	case 1:
		count = sprintf(buf, "Downloading\n");
		break;
	case 2:
		if (data->status.update_result) {
			count = sprintf(buf, "Update fail\n");
		} else {
			count = sprintf(buf, "Update success, ver %x(%x, %x), "
					"SLF status : %d, gap : %d, MTL status : %d, gap : %d, "
					"PROX status : %d, gap : %d\n",
					data->fw.cur.fw_ver, data->fw.cur.main_ver,
					data->fw.cur.test_ver,
					CALIB_TO_STATUS(data->status.calib_msg[0]),
					CALIB_TO_GAP(data->status.calib_msg[0]),
					CALIB_TO_STATUS(data->status.calib_msg[1]),
					CALIB_TO_GAP(data->status.calib_msg[1]),
					CALIB_TO_STATUS(data->status.calib_msg[2]),
					CALIB_TO_GAP(data->status.calib_msg[2]));
		}
		break;
	default:
		if (data->status.update_result)
			count = sprintf(buf, "Update fail\n");
		else
			count = sprintf(buf, "Pass\n");
	}

	return count;
}

/* sysfs: /sys/class/touch/firmware/fw_read */
ssize_t ist40xx_fw_read_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i;
	int ret;
	int count;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	char fw_path[MAX_FILE_PATH];
	u8 *fwbuf = NULL;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	count = sprintf(buf, "Fail\n");  // Prevent CID: 57387
	mutex_lock(&data->lock);
	ist40xx_disable_irq(data);

	fwbuf = vzalloc(IST40XX_ROM_TOTAL_SIZE);
	if (!fwbuf) {
		tsp_info("failed to fwbuf allocate\n");
		goto err_alloc;
	}

	ret = ist40xx_isp_fw_read(data, (u32 *)fwbuf);
	if (ret) {
		count = sprintf(buf, "Fail\n");
		tsp_err("isp fw read fail\n");
		goto err_file_open;
	}

	for (i = 0; i < IST40XX_ROM_TOTAL_SIZE; i += 16) {
		tsp_debug("%07x: %02x %02x %02x %02x %02x %02x %02x %02x "
			  "%02x %02x %02x %02x %02x %02x %02x %02x\n", i,
			  fwbuf[i], fwbuf[i + 1], fwbuf[i + 2], fwbuf[i + 3],
			  fwbuf[i + 4], fwbuf[i + 5], fwbuf[i + 6], fwbuf[i + 7],
			  fwbuf[i + 8], fwbuf[i + 9], fwbuf[i + 10], fwbuf[i + 11],
			  fwbuf[i + 12], fwbuf[i + 13], fwbuf[i + 14], fwbuf[i + 15]);
	}

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST40XX_BIN_NAME);
	fp = filp_open(fw_path, O_CREAT|O_WRONLY|O_TRUNC, 0);
	if (IS_ERR(fp)) {
		count = sprintf(buf, "Fail\n");
		tsp_err("file %s open error:%d\n", fw_path, PTR_ERR(fp));
		goto err_file_open;
	}

	fp->f_op->write(fp, fwbuf, IST40XX_ROM_TOTAL_SIZE, &fp->f_pos);
	fput(fp);

	filp_close(fp, NULL);
	set_fs(old_fs);

	count = sprintf(buf, "OK\n");

err_file_open:
err_alloc:
	vfree(fwbuf);

	ist40xx_enable_irq(data);
	mutex_unlock(&data->lock);

	ist40xx_start(data);

	return count;
}

/* sysfs: /sys/class/touch/firmware/ium_read */
ssize_t ist40xx_ium_read_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i;
	int ret;
	int count;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	char fw_path[MAX_FILE_PATH];
	u8 *fwbuf = NULL;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	count = sprintf(buf, "Fail\n");  // Prevent CID: 57388
	mutex_lock(&data->lock);
	ist40xx_disable_irq(data);

	fwbuf = vzalloc(IST40XX_IUM_SIZE);
	if (!fwbuf) {
		tsp_info("failed to fwbuf allocate\n");
		goto err_alloc;
	}

	ret = ist40xx_ium_read(data, (u32 *)fwbuf);
	if (ret) {
		count = sprintf(buf, "Fail\n");
		tsp_err("isp fw read fail\n");
		goto err_file_open;
	}

	for (i = 0; i < IST40XX_IUM_SIZE; i += 16) {
		tsp_debug("%07x: %02x %02x %02x %02x %02x %02x %02x %02x "
			  "%02x %02x %02x %02x %02x %02x %02x %02x\n", i,
			  fwbuf[i], fwbuf[i + 1], fwbuf[i + 2], fwbuf[i + 3],
			  fwbuf[i + 4], fwbuf[i + 5], fwbuf[i + 6], fwbuf[i + 7],
			  fwbuf[i + 8], fwbuf[i + 9], fwbuf[i + 10], fwbuf[i + 11],
			  fwbuf[i + 12], fwbuf[i + 13], fwbuf[i + 14], fwbuf[i + 15]);
	}

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST40XX_IUM_NAME);
	fp = filp_open(fw_path, O_CREAT|O_WRONLY|O_TRUNC, 0);
	if (IS_ERR(fp)) {
		count = sprintf(buf, "Fail\n");
		tsp_err("file %s open error:%d\n", fw_path, PTR_ERR(fp));
		goto err_file_open;
	}

	fp->f_op->write(fp, fwbuf, IST40XX_IUM_SIZE, &fp->f_pos);
	fput(fp);

	filp_close(fp, NULL);
	set_fs(old_fs);

	count = sprintf(buf, "OK\n");

err_file_open:
err_alloc:
	vfree(fwbuf);

	ist40xx_enable_irq(data);
	mutex_unlock(&data->lock);

	ist40xx_start(data);

	return count;
}

/* sysfs: /sys/class/touch/firmware/version */
ssize_t ist40xx_fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	count = sprintf(buf, "ID: %x, main: %x, fw: %x, test: %x\n",
			data->chip_id, data->fw.cur.main_ver, data->fw.cur.fw_ver,
			data->fw.cur.test_ver);

	{
		char msg[128];
		int ret = 0;

		ret = ist40xx_get_update_info(data, data->fw.buf, data->fw.buf_size);
		if (ret == 0) {
			count += sprintf(msg,
					" Header - main: %x, fw: %x, test: %x\n",
					ist40xx_parse_ver(data, FLAG_MAIN, data->fw.buf),
					ist40xx_parse_ver(data, FLAG_FW, data->fw.buf),
					ist40xx_parse_ver(data, FLAG_TEST, data->fw.buf));
			strcat(buf, msg);
		}
	}

	return count;
}

/* sysfs  */
static DEVICE_ATTR(fw_read, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_fw_read_show,
		NULL);
static DEVICE_ATTR(ium_read, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_ium_read_show,
		NULL);
static DEVICE_ATTR(firmware, S_IRUGO | S_IWUSR | S_IWGRP,
		ist40xx_fw_status_show, ist40xx_fw_store);
static DEVICE_ATTR(fw_sdcard, S_IRUGO | S_IWUSR | S_IWGRP,
		ist40xx_fw_sdcard_show, NULL);
static DEVICE_ATTR(version, S_IRUGO | S_IWUSR | S_IWGRP,
		ist40xx_fw_version_show, NULL);

struct class *ist40xx_class;
struct device *ist40xx_fw_dev;

static struct attribute *fw_attributes[] = {
	&dev_attr_fw_read.attr,
	&dev_attr_ium_read.attr,
	&dev_attr_firmware.attr,
	&dev_attr_fw_sdcard.attr,
	&dev_attr_version.attr,
	NULL,
};

static struct attribute_group fw_attr_group = {
	.attrs = fw_attributes,
};

int ist40xx_init_update_sysfs(struct ist40xx_data *data)
{
	/* /sys/class/touch */
	ist40xx_class = class_create(THIS_MODULE, "touch");

	/* /sys/class/touch/firmware */
	ist40xx_fw_dev = device_create(ist40xx_class, NULL, 0, data, "firmware");

	/* /sys/class/touch/firmware/... */
	if (sysfs_create_group(&ist40xx_fw_dev->kobj, &fw_attr_group))
		tsp_err("Failed to create sysfs group(%s)!\n", "firmware");

	data->status.update = 0;
	data->status.calib = 0;
	data->status.miscalib = 0;
	data->status.update_result = 0;

	return 0;
}
