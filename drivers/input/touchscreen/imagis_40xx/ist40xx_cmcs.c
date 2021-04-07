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

#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/err.h>

#include "ist40xx.h"
#include "ist40xx_cmcs.h"

#define TSP_CH_UNKNOWN		0
#define TSP_CH_UNUSED		1
#define TSP_CH_USED		2

#define CMCS_NOT_READY		0
#define CMCS_READY		1

#define CMCS_TIMEOUT		10000	// unit : msec

int cmcs_ready = CMCS_READY;
u8 *ts_cmcs_bin = NULL;
u32 ts_cmcs_bin_size = 0;
CMCS_BIN_INFO ist40xx_cmcs_bin;
CMCS_BIN_INFO *ts_cmcs = (CMCS_BIN_INFO *) &ist40xx_cmcs_bin;
CMCS_BUF ist40xx_cmcs_buf;
CMCS_BUF *cmcs_buf = (CMCS_BUF *) &ist40xx_cmcs_buf;

int ist40xx_parse_cmcs_bin(const u8 * buf, const u32 size)
{
	int ret = -EPERM;
	int i;
	int idx;
	int node_spec_cnt;

	memcpy(ts_cmcs->magic1, buf, sizeof(ts_cmcs->magic1));
	memcpy(ts_cmcs->magic2, &buf[size - sizeof(ts_cmcs->magic2)],
		   sizeof(ts_cmcs->magic2));

	if (!strncmp(ts_cmcs->magic1, IST40XX_CMCS_MAGIC, sizeof(ts_cmcs->magic1))
			&& !strncmp(ts_cmcs->magic2, IST40XX_CMCS_MAGIC,
				sizeof(ts_cmcs->magic2))) {
		idx = sizeof(ts_cmcs->magic1);

		memcpy(&ts_cmcs->items.cnt, &buf[idx], sizeof(ts_cmcs->items.cnt));
		idx += sizeof(ts_cmcs->items.cnt);
		ts_cmcs->items.item =
			kmalloc(sizeof(struct CMCS_ITEM_INFO) * ts_cmcs->items.cnt,
					GFP_KERNEL);
		if (!ts_cmcs->items.item) {
			tsp_err("%s() failed to allocate\n", __func__);
			ret = -EPERM;
			return ret;
		}

		for (i = 0; i < ts_cmcs->items.cnt; i++) {
			memcpy(&ts_cmcs->items.item[i], &buf[idx],
					sizeof(struct CMCS_ITEM_INFO));
			idx += sizeof(struct CMCS_ITEM_INFO);
		}

		memcpy(&ts_cmcs->cmds.cnt, &buf[idx], sizeof(ts_cmcs->cmds.cnt));
		idx += sizeof(ts_cmcs->cmds.cnt);
		ts_cmcs->cmds.cmd =
			kmalloc(sizeof(struct CMCS_CMD_INFO) * ts_cmcs->cmds.cnt,
					GFP_KERNEL);
		if (!ts_cmcs->cmds.cmd) {
			tsp_err("%s() failed to allocate\n", __func__);
			ret = -EPERM;
			return ret;
		}

		for (i = 0; i < ts_cmcs->cmds.cnt; i++) {
			memcpy(&ts_cmcs->cmds.cmd[i], &buf[idx],
					sizeof(struct CMCS_CMD_INFO));
			idx += sizeof(struct CMCS_CMD_INFO);
		}

		memcpy(&ts_cmcs->spec_slope, &buf[idx], sizeof(ts_cmcs->spec_slope));
		idx += sizeof(ts_cmcs->spec_slope);

		memcpy(&ts_cmcs->spec_cr, &buf[idx], sizeof(ts_cmcs->spec_cr));
		idx += sizeof(ts_cmcs->spec_cr);

		memcpy(&ts_cmcs->spec_cm_hfreq, &buf[idx],
				sizeof(ts_cmcs->spec_cm_hfreq));
		idx += sizeof(ts_cmcs->spec_cm_hfreq);

		memcpy(&ts_cmcs->spec_cm_lfreq, &buf[idx],
				sizeof(ts_cmcs->spec_cm_lfreq));
		idx += sizeof(ts_cmcs->spec_cm_lfreq);

		memcpy(&ts_cmcs->param, &buf[idx], sizeof(ts_cmcs->param));
		idx += sizeof(ts_cmcs->param);

		ts_cmcs->spec_item =
			kmalloc(sizeof(struct CMCS_SPEC_TOTAL) * ts_cmcs->items.cnt,
				GFP_KERNEL);
		if (!ts_cmcs->spec_item) {
			tsp_err("%s() failed to allocate\n", __func__);
			ret = -EPERM;
			return ret;
		}

		for (i = 0; i < ts_cmcs->items.cnt; i++) {
			if (!strcmp(ts_cmcs->items.item[i].spec_type, "N")) {
				memcpy(&node_spec_cnt, &buf[idx], sizeof(node_spec_cnt));
				ts_cmcs->spec_item[i].spec_node.node_cnt = node_spec_cnt;
				idx += sizeof(node_spec_cnt);
				ts_cmcs->spec_item[i].spec_node.buf_min = (u16 *)&buf[idx];
				idx += node_spec_cnt * sizeof(s16);
				ts_cmcs->spec_item[i].spec_node.buf_max = (u16 *)&buf[idx];
				idx += node_spec_cnt * sizeof(s16);
			} else if (!strcmp(ts_cmcs->items.item[i].spec_type, "T")) {
				memcpy(&ts_cmcs->spec_item[i].spec_total, &buf[idx],
						sizeof(struct CMCS_SPEC_TOTAL));
				idx += sizeof(struct CMCS_SPEC_TOTAL);
			}
		}

		ts_cmcs->buf_cmcs = (u8 *) &buf[idx];
		idx += ts_cmcs->param.cmcs_size;

		ts_cmcs->buf_cm_sensor = (u32 *) &buf[idx];
		idx += ts_cmcs->param.cm_sensor1_size + ts_cmcs->param.cm_sensor2_size +
			ts_cmcs->param.cm_sensor3_size;

		ts_cmcs->buf_cs_sensor = (u32 *) &buf[idx];
		idx += ts_cmcs->param.cs_sensor1_size + ts_cmcs->param.cs_sensor2_size +
			ts_cmcs->param.cs_sensor3_size;

		ts_cmcs->buf_jit_sensor = (u32 *) &buf[idx];
		idx += ts_cmcs->param.jit_sensor1_size +
			ts_cmcs->param.jit_sensor2_size + ts_cmcs->param.jit_sensor3_size;

		ts_cmcs->version = *((u32 *) &buf[idx]);

		ret = 0;
	}

	tsp_verb("Magic1: %s, Magic2: %s\n", ts_cmcs->magic1, ts_cmcs->magic2);
	tsp_verb("CmCs ver: %08X\n", ts_cmcs->version);
	tsp_verb(" item(%d)\n", ts_cmcs->items.cnt);
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		tsp_verb(" (%d): %s, 0x%08x, %d, %s, %s\n",
			 i, ts_cmcs->items.item[i].name,
			 ts_cmcs->items.item[i].addr,
			 ts_cmcs->items.item[i].size,
			 ts_cmcs->items.item[i].data_type,
			 ts_cmcs->items.item[i].spec_type);
	}
	tsp_verb(" cmd(%d)\n", ts_cmcs->cmds.cnt);
	for (i = 0; i < ts_cmcs->cmds.cnt; i++)
		tsp_verb(" (%d): 0x%08x, 0x%08x\n", i,
			 ts_cmcs->cmds.cmd[i].addr, ts_cmcs->cmds.cmd[i].value);
	tsp_verb(" param\n");
	tsp_verb("  fw: 0x%08x, %d\n", ts_cmcs->param.cmcs_size_addr,
		 ts_cmcs->param.cmcs_size);
	tsp_verb("  enable: 0x%08x\n", ts_cmcs->param.enable_addr);
	tsp_verb("  checksum: 0x%08x\n", ts_cmcs->param.checksum_addr);
	tsp_verb("  endnotify: 0x%08x\n", ts_cmcs->param.end_notify_addr);
	tsp_verb("  cm sensor1: 0x%08x, %d\n", ts_cmcs->param.sensor1_addr,
		 ts_cmcs->param.cm_sensor1_size);
	tsp_verb("  cm sensor2: 0x%08x, %d\n", ts_cmcs->param.sensor2_addr,
		 ts_cmcs->param.cm_sensor2_size);
	tsp_verb("  cm sensor3: 0x%08x, %d\n", ts_cmcs->param.sensor3_addr,
		 ts_cmcs->param.cm_sensor3_size);
	tsp_verb("  cs sensor1: 0x%08x, %d\n", ts_cmcs->param.sensor1_addr,
		 ts_cmcs->param.cs_sensor1_size);
	tsp_verb("  cs sensor2: 0x%08x, %d\n", ts_cmcs->param.sensor2_addr,
		 ts_cmcs->param.cs_sensor2_size);
	tsp_verb("  cs sensor3: 0x%08x, %d\n", ts_cmcs->param.sensor3_addr,
		 ts_cmcs->param.cs_sensor3_size);
	tsp_verb("  jit sensor1: 0x%08x, %d\n", ts_cmcs->param.sensor1_addr,
		 ts_cmcs->param.jit_sensor1_size);
	tsp_verb("  jit sensor2: 0x%08x, %d\n", ts_cmcs->param.sensor2_addr,
		 ts_cmcs->param.jit_sensor2_size);
	tsp_verb("  jit sensor3: 0x%08x, %d\n", ts_cmcs->param.sensor3_addr,
		 ts_cmcs->param.jit_sensor3_size);
	tsp_verb("  chksum: 0x%08x\n, 0x%08x\n, 0x%08x, 0x%08x\n",
		 ts_cmcs->param.cmcs_chksum, ts_cmcs->param.cm_sensor_chksum,
		 ts_cmcs->param.cs_sensor_chksum,
		 ts_cmcs->param.jit_sensor_chksum);
	tsp_verb("  cs result addr(tx, rx): 0x%08x, 0x%08x\n",
		 ts_cmcs->param.cs_tx_result_addr,
		 ts_cmcs->param.cs_rx_result_addr);
	tsp_verb(" slope(%s)\n", ts_cmcs->spec_slope.name);
	tsp_verb("  x(%d,%d), y(%d,%d), key(%d,%d)\n",
		 ts_cmcs->spec_slope.x_min, ts_cmcs->spec_slope.x_max,
		 ts_cmcs->spec_slope.y_min, ts_cmcs->spec_slope.y_max,
		 ts_cmcs->spec_slope.key_min, ts_cmcs->spec_slope.key_max);
	tsp_verb(" cr: screen(%4d, %4d), key(%4d, %4d)\n",
		 ts_cmcs->spec_cr.screen_min, ts_cmcs->spec_cr.screen_max,
		 ts_cmcs->spec_cr.key_min, ts_cmcs->spec_cr.key_max);
	for (i = 0; i < ts_cmcs->items.cnt; i++) {
		if (!strcmp(ts_cmcs->items.item[i].spec_type, "N")) {
			tsp_verb(" %s\n", ts_cmcs->items.item[i].name);
			tsp_verb(" min: %x, %x, %x, %x\n",
				 ts_cmcs->spec_item[i].spec_node.buf_min[0],
				 ts_cmcs->spec_item[i].spec_node.buf_min[1],
				 ts_cmcs->spec_item[i].spec_node.buf_min[2],
				 ts_cmcs->spec_item[i].spec_node.buf_min[3]);
			tsp_verb(" max: %x, %x, %x, %x\n",
				 ts_cmcs->spec_item[i].spec_node.buf_max[0],
				 ts_cmcs->spec_item[i].spec_node.buf_max[1],
				 ts_cmcs->spec_item[i].spec_node.buf_max[2],
				 ts_cmcs->spec_item[i].spec_node.buf_max[3]);
		} else if (!strcmp(ts_cmcs->items.item[i].spec_type, "T")) {
			tsp_verb(" %s: screen(%4d, %4d), key(%4d, %4d)\n",
				 ts_cmcs->items.item[i].name,
				 ts_cmcs->spec_item[i].spec_total.screen_min,
				 ts_cmcs->spec_item[i].spec_total.screen_max,
				 ts_cmcs->spec_item[i].spec_total.key_min,
				 ts_cmcs->spec_item[i].spec_total.key_max);
		}
	}
	tsp_verb(" cmcs: %x, %x, %x, %x\n", ts_cmcs->buf_cmcs[0],
		 ts_cmcs->buf_cmcs[1], ts_cmcs->buf_cmcs[2],
		 ts_cmcs->buf_cmcs[3]);
	tsp_verb(" cm sensor: %x, %x, %x, %x\n", ts_cmcs->buf_cm_sensor[0],
		 ts_cmcs->buf_cm_sensor[1], ts_cmcs->buf_cm_sensor[2],
		 ts_cmcs->buf_cm_sensor[3]);
	tsp_verb(" cs sensor: %x, %x, %x, %x\n", ts_cmcs->buf_cs_sensor[0],
		 ts_cmcs->buf_cs_sensor[1], ts_cmcs->buf_cs_sensor[2],
		 ts_cmcs->buf_cs_sensor[3]);
	tsp_verb(" jit sensor: %x, %x, %x, %x\n", ts_cmcs->buf_jit_sensor[0],
		 ts_cmcs->buf_jit_sensor[1], ts_cmcs->buf_jit_sensor[2],
		 ts_cmcs->buf_jit_sensor[3]);

	return ret;
}

int ist40xx_get_cmcs_info(const u8 *buf, const u32 size)
{
	int ret;

	cmcs_ready = CMCS_NOT_READY;

	ret = ist40xx_parse_cmcs_bin(buf, size);
	if (ret)
		tsp_warn("Cannot find tags of CMCS, make a bin by 'cmcs2bin.exe'\n");

	return ret;
}

int ist40xx_set_cmcs_fw(struct ist40xx_data *data, CMCS_PARAM param, u32 *buf32)
{
	int ret;
	int len;
	u32 waddr;
	u32 val;

	len = param.cmcs_size / IST40XX_DATA_LEN;
	waddr = IST40XX_DA_ADDR(data->tags.ram_base);
	tsp_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
	tsp_verb("%08x(%d)\n", waddr, len);
	ret = ist40xx_burst_write(data->client, waddr, buf32, len);
	if (ret)
		return ret;

	waddr = IST40XX_DA_ADDR(param.cmcs_size_addr);
	val = param.cmcs_size;
	tsp_verb("size(0x%08x): 0x%08x\n", waddr, val);
	ret = ist40xx_write_cmd(data, waddr, val);
	if (ret)
		return ret;

	tsp_info("cmcs code loaded!\n");

	return 0;
}

int ist40xx_set_cmcs_sensor(struct ist40xx_data *data, CMCS_PARAM param,
		u32 *buf32, int mode)
{
	int ret;
	int len = 0;
	u32 waddr;

	if (mode == CMCS_FLAG_CM) {
		waddr = IST40XX_DA_ADDR(param.sensor1_addr);
		len = (param.cm_sensor1_size / IST40XX_DATA_LEN) - 2;
		buf32 += 2;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cm sensor reg1 loaded!\n");
		}

		buf32 += len;
		waddr = IST40XX_DA_ADDR(param.sensor2_addr);
		len = param.cm_sensor2_size / IST40XX_DATA_LEN;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cm sensor reg2 loaded!\n");
		}

		buf32 += len;
		waddr = IST40XX_DA_ADDR(param.sensor3_addr);
		len = param.cm_sensor3_size / IST40XX_DATA_LEN;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cm sensor reg3 loaded!\n");
		}
	} else if (mode == CMCS_FLAG_CS) {
		waddr = IST40XX_DA_ADDR(param.sensor1_addr);
		len = (param.cs_sensor1_size / IST40XX_DATA_LEN) - 2;
		buf32 += 2;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cs sensor reg1 loaded!\n");
		}

		buf32 += len;
		waddr = IST40XX_DA_ADDR(param.sensor2_addr);
		len = param.cs_sensor2_size / IST40XX_DATA_LEN;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cs sensor reg2 loaded!\n");
		}

		buf32 += len;
		waddr = IST40XX_DA_ADDR(param.sensor3_addr);
		len = param.cs_sensor3_size / IST40XX_DATA_LEN;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("cs sensor reg3 loaded!\n");
		}
	} else if (mode == CMCS_FLAG_CMJIT) {
		waddr = IST40XX_DA_ADDR(param.sensor1_addr);
		len = (param.jit_sensor1_size / IST40XX_DATA_LEN) - 2;
		buf32 += 2;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("jit sensor reg1 loaded!\n");
		}

		buf32 += len;
		waddr = IST40XX_DA_ADDR(param.sensor2_addr);
		len = param.jit_sensor2_size / IST40XX_DATA_LEN;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("jit sensor reg2 loaded!\n");
		}

		buf32 += len;
		waddr = IST40XX_DA_ADDR(param.sensor3_addr);
		len = param.jit_sensor3_size / IST40XX_DATA_LEN;
		tsp_verb("%08x %08x %08x\n", buf32[0], buf32[1], buf32[2]);
		tsp_verb("%08x(%d)\n", waddr, len);

		if (len > 0) {
			ret = ist40xx_burst_write(data->client, waddr, buf32, len);
			if (ret)
				return ret;

			tsp_info("jit sensor reg3 loaded!\n");
		}
	}

	return 0;
}

int ist40xx_set_cmcs_cmd(struct ist40xx_data *data, CMCS_CMD cmds)
{
	int ret;
	int i;
	u32 val;
	u32 waddr;

	for (i = 0; i < cmds.cnt; i++) {
		waddr = IST40XX_DA_ADDR(cmds.cmd[i].addr);
		val = cmds.cmd[i].value;
		ret = ist40xx_write_cmd(data, waddr, val);
		if (ret)
			return ret;
		tsp_verb("cmd%d(0x%08x): 0x%08x\n", i, waddr, val);
	}

	tsp_info("cmcs command loaded!\n");

	return 0;
}

int ist40xx_get_cmcs_buf(struct ist40xx_data *data, const char *mode,
		CMCS_ITEM items, s16 *buf)
{
	int ret = 0;
	int i;
	bool success = false;
	u32 waddr;
	u16 len;
	TSP_INFO *tsp = &data->tsp_info;

	for (i = 0; i < items.cnt; i++) {
		if (!strcmp(items.item[i].name, mode)) {
			waddr = IST40XX_DA_ADDR(items.item[i].addr);
			len = (tsp->node.len * 2) / IST40XX_DATA_LEN;
			if ((tsp->node.len * 2) % IST40XX_DATA_LEN)
				len += 1;
			ret = ist40xx_burst_read(data->client, waddr, (u32 *)buf, len,
					true);
			if (ret)
				return ret;
			tsp_verb("%s() 0x%08x, %d\n", __func__, waddr, len);
			success = true;
		}
	}

	if (success == false) {
		tsp_info("item(%s) doesn't exist!\n", mode);
		return -EPERM;
	}

	return ret;
}

int ist40xx_cmcs_enable_and_wait(struct ist40xx_data *data, CMCS_PARAM param,
		u32 *buf32, u32 mode)
{
	int ret;
	int cnt = CMCS_TIMEOUT / 100;
	u32 waddr;

	ret = ist40xx_set_cmcs_sensor(data, param, buf32, mode);
	if (ret) {
		tsp_info("Test not ready!!\n", mode);
		return ret;
	}

	if ((mode != CMCS_FLAG_CM) && (mode != CMCS_FLAG_CS) &&
			(mode != CMCS_FLAG_CMJIT)) {
		tsp_err("not support test item\n");
		return -EPERM;
	}

	ist40xx_enable_irq(data);
	data->status.event_mode = false;

	data->status.cmcs = 0;
	data->status.cmcs_result = 0;

	waddr = IST40XX_DA_ADDR(ts_cmcs->param.enable_addr);
	ret = ist40xx_write_cmd(data, waddr, mode);
	if (ret)
		return -EPERM;

	while (cnt-- > 0) {
		ist40xx_delay(100);

		if (data->status.cmcs) {
			if (mode == CMCS_FLAG_CM) {
				if (CMCS_MSG(data->status.cmcs) == CM_MSG_VALID) {
					if (data->status.cmcs_result == 0x10)
						cmcs_buf->cm_result = 0;
					else
						cmcs_buf->cm_result = 1;
					goto end;
				}
			} else if (mode == CMCS_FLAG_CS) {
				if (CMCS_MSG(data->status.cmcs) == CS_MSG_VALID) {
					if (data->status.cmcs_result == 0x10)
						cmcs_buf->cs_result = 0;
					else
						cmcs_buf->cs_result = 1;
					goto end;
				}
			} else if (mode == CMCS_FLAG_CMJIT) {
				if (CMCS_MSG(data->status.cmcs) == CMJIT_MSG_VALID) {
					if (data->status.cmcs_result == 0x10)
						cmcs_buf->cm_jit_result = 0;
					else
						cmcs_buf->cm_jit_result = 1;
					goto end;
				}
			}
		}
	}

	ist40xx_disable_irq(data);
	tsp_err("cmcs time out\n");

	return -EPERM;

end:
	ist40xx_disable_irq(data);
	tsp_warn("test end\n");

	return 0;
}

void ist40xx_result_calculate(struct ist40xx_data *data, u32 mode)
{
	int i, j;
	int idx;
	TSP_INFO *tsp = &data->tsp_info;

	if (mode == CMCS_FLAG_CM) {
		cmcs_buf->cm_tx_result[0] = 0;
		cmcs_buf->cm_tx_result[1] = 0;
		cmcs_buf->cm_rx_result[0] = 0;
		cmcs_buf->cm_rx_result[1] = 0;

		for (i = 0; i < tsp->ch_num.tx; i++) {
			for (j = 0; j < tsp->ch_num.rx; j++) {
				idx = (i * tsp->ch_num.rx) + j;
				if ((cmcs_buf->cm[idx] < data->dt_data->cm_min_spec) ||
						(cmcs_buf->cm[idx] > data->dt_data->cm_max_spec)) {
					if (i >= 32)
						cmcs_buf->cm_tx_result[1] |= (1 << (i - 32));
					else
						cmcs_buf->cm_tx_result[0] |= (1 << i);

					if (j >= 32)
						cmcs_buf->cm_rx_result[1] |= (1 << (j - 32));
					else
						cmcs_buf->cm_rx_result[0] |= (1 << j);
				}
			}
		}

		if ((cmcs_buf->cm_tx_result[0] != 0) ||
				(cmcs_buf->cm_tx_result[1] != 0) ||
				(cmcs_buf->cm_rx_result[0] != 0) ||
				(cmcs_buf->cm_rx_result[1] != 0))
			cmcs_buf->cm_result = 1;
		else
			cmcs_buf->cm_result = 0;
	} else if (mode == CMCS_FLAG_CS) {
		if ((cmcs_buf->cs_tx_result[0] != 0) ||
				(cmcs_buf->cs_tx_result[1] != 0) ||
				(cmcs_buf->cs_rx_result[0] != 0) ||
				(cmcs_buf->cs_rx_result[1] != 0))
			cmcs_buf->cs_result = 1;
		else
			cmcs_buf->cs_result = 0;
	}
}

void ist40xx_slope_calculate(struct ist40xx_data *data)
{
	int i, j;
	int idx, next_idx;
	TSP_INFO *tsp = &data->tsp_info;

	cmcs_buf->slope_tx_result[0] = 0;
	cmcs_buf->slope_tx_result[1] = 0;
	cmcs_buf->slope_rx_result[0] = 0;
	cmcs_buf->slope_rx_result[1] = 0;

	cmcs_buf->slope_result = 0;

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;
			next_idx = idx + 1;
			if (j == (tsp->ch_num.rx - 1)) {
				cmcs_buf->slope0[idx] = 0;
			} else {
				if (cmcs_buf->cm[idx] && cmcs_buf->cm[next_idx]) {
					cmcs_buf->slope0[idx] = 100 - DIV_ROUND_CLOSEST(100 * min(cmcs_buf->cm[idx], cmcs_buf->cm[next_idx]), max(cmcs_buf->cm[idx], cmcs_buf->cm[next_idx]));
				} else {
					cmcs_buf->slope0[idx] = 9999;
				}
			}

			next_idx = idx + tsp->ch_num.rx;
			if (i == (tsp->ch_num.tx - 1)) {
				cmcs_buf->slope1[idx] = 0;
			} else {
				if (cmcs_buf->cm[idx] && cmcs_buf->cm[next_idx]) {
					cmcs_buf->slope1[idx] = 100 - DIV_ROUND_CLOSEST(100 * min(cmcs_buf->cm[idx], cmcs_buf->cm[next_idx]), max(cmcs_buf->cm[idx], cmcs_buf->cm[next_idx]));
				} else {
					cmcs_buf->slope1[idx] = 9999;
				}
			}

			cmcs_buf->slope[idx] =
				(cmcs_buf->slope0[idx] > cmcs_buf->slope1[idx]) ?
				cmcs_buf->slope0[idx] : cmcs_buf->slope1[idx];

			if (cmcs_buf->slope[idx] > data->dt_data->cm_spec_gap) {
				if (i >= 32)
					cmcs_buf->slope_tx_result[1] |= (1 << (i - 32));
				else
					cmcs_buf->slope_tx_result[0] |= (1 << i);

				if (j >= 32)
					cmcs_buf->slope_rx_result[1] |= (1 << (j - 32));
				else
					cmcs_buf->slope_rx_result[0] |= (1 << j);
			}
		}
	}

	if ((cmcs_buf->slope_tx_result[0] != 0) ||
			(cmcs_buf->slope_tx_result[1] != 0) ||
			(cmcs_buf->slope_rx_result[0] != 0) ||
			(cmcs_buf->slope_rx_result[1] != 0))
		cmcs_buf->slope_result = 1;
}

#define cmcs_next_step(ret) { if (ret) goto end; ist40xx_delay(20); }
int ist40xx_cmcs_test(struct ist40xx_data *data, u32 mode)
{
	int ret;
	u32 waddr;
	u32 chksum = 0;
	u32 *buf32;

	tsp_info("*** CM/CS test ***\n");

	ist40xx_disable_irq(data);
	ist40xx_reset(data, false);

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_RUN_RAMCODE << 16) | IST40XX_DISABLE);
	cmcs_next_step(ret);

	buf32 = (u32 *)ts_cmcs->buf_cmcs;
	ret = ist40xx_set_cmcs_fw(data, ts_cmcs->param, buf32);
	cmcs_next_step(ret);

	ret = ist40xx_set_cmcs_cmd(data, ts_cmcs->cmds);
	cmcs_next_step(ret);

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_RUN_RAMCODE << 16) | IST40XX_ENABLE);
	cmcs_next_step(ret);

	waddr = IST40XX_DA_ADDR(ts_cmcs->param.checksum_addr);
	ret = ist40xx_read_reg(data->client, waddr, &chksum);
	cmcs_next_step(ret);
	if (chksum != ts_cmcs->param.cmcs_chksum)
		goto end;

	if ((mode & CMCS_FLAG_CM) || (mode & CMCS_FLAG_SLOPE)) {
		tsp_info("CM test\n");
		buf32 = ts_cmcs->buf_cm_sensor;
		memset(cmcs_buf->cm, 0, sizeof(cmcs_buf->cm));
		ret = ist40xx_cmcs_enable_and_wait(data, ts_cmcs->param, buf32,
				CMCS_FLAG_CM);
		cmcs_next_step(ret);

		tsp_info("read cm data\n");
		ret = ist40xx_get_cmcs_buf(data, CMCS_CM, ts_cmcs->items, cmcs_buf->cm);
		cmcs_next_step(ret);

		ist40xx_result_calculate(data, CMCS_FLAG_CM);

		if (mode & CMCS_FLAG_SLOPE) {
			tsp_info("slope calculate\n");
			memset(cmcs_buf->slope, 0, sizeof(cmcs_buf->slope));
			memset(cmcs_buf->slope0, 0, sizeof(cmcs_buf->slope0));
			memset(cmcs_buf->slope1, 0, sizeof(cmcs_buf->slope1));
			ist40xx_slope_calculate(data);
		}
	}

	if (mode & CMCS_FLAG_CS) {
		tsp_info("CS test\n");
		buf32 = ts_cmcs->buf_cs_sensor;
		memset(cmcs_buf->cs, 0, sizeof(cmcs_buf->cs));
		ret = ist40xx_cmcs_enable_and_wait(data, ts_cmcs->param, buf32,
				CMCS_FLAG_CS);
		cmcs_next_step(ret);

		tsp_info("read cs data\n");
		ret = ist40xx_get_cmcs_buf(data, CMCS_CS, ts_cmcs->items, cmcs_buf->cs);
		cmcs_next_step(ret);

		tsp_info("read cs tx data\n");
		ret = ist40xx_burst_read(data->client,
				IST40XX_DA_ADDR(ts_cmcs->param.cs_tx_result_addr),
				cmcs_buf->cs_tx_result, 2, true);
		tsp_info(" tx [0]0x%x, [1]0x%x \n", cmcs_buf->cs_tx_result[0],
				cmcs_buf->cs_tx_result[1]);
		cmcs_next_step(ret);

		tsp_info("read cs rx data\n");
		ret = ist40xx_burst_read(data->client,
				IST40XX_DA_ADDR(ts_cmcs->param.cs_rx_result_addr),
				cmcs_buf->cs_rx_result, 2, true);
		tsp_info(" rx [0]0x%x, [1]0x%x \n", cmcs_buf->cs_rx_result[0],
				cmcs_buf->cs_rx_result[1]);
		cmcs_next_step(ret);

		ist40xx_result_calculate(data, CMCS_FLAG_CS);
	}

	if (mode & CMCS_FLAG_CMJIT) {
		tsp_info("JITTER test\n");
		buf32 = ts_cmcs->buf_jit_sensor;
		memset(cmcs_buf->cm_jit, 0, sizeof(cmcs_buf->cm_jit));
		ret = ist40xx_cmcs_enable_and_wait(data, ts_cmcs->param, buf32,
				CMCS_FLAG_CMJIT);
		cmcs_next_step(ret);

		tsp_info("read jitter data\n");
		ret = ist40xx_get_cmcs_buf(data, CMCS_CMJIT, ts_cmcs->items,
				cmcs_buf->cm_jit);
		cmcs_next_step(ret);
	}

	cmcs_ready = CMCS_READY;
end:
	if (ret)
		tsp_warn("CmCs test Fail!, ret=%d\n", ret);

	ist40xx_reset(data, false);
	ist40xx_enable_irq(data);
	ist40xx_start(data);

	return ret;
}

int check_tsp_type(struct ist40xx_data *data, int tx, int rx)
{
	TSP_INFO *tsp = &data->tsp_info;

	if ((tx >= tsp->ch_num.tx) || (tx < 0) ||
		(rx >= tsp->ch_num.rx) || (rx < 0)) {
		tsp_warn("TSP channel is not correct!! (%d * %d)\n", tx, rx);
		return TSP_CH_UNKNOWN;
	}

	return TSP_CH_USED;
}

int print_cmcs(struct ist40xx_data *data, s16 *buf16, char *buf, bool line)
{
	int i, j;
	int idx;
	int type;
	int count = 0;
	char msg[128];
	TSP_INFO *tsp = &data->tsp_info;

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			type = check_tsp_type(data, i, j);
			idx = (i * tsp->ch_num.rx) + j;
			if (type == TSP_CH_USED)
				count += sprintf(msg, "%5d ", buf16[idx]);
			else
				count += sprintf(msg, "%5d ", 0);

			strcat(buf, msg);
		}

		if (!line) {
			count += sprintf(msg, "\n");
			strcat(buf, msg);
		}
	}

	count += sprintf(msg, "\n");
	strcat(buf, msg);

	return count;
}

#if defined(CONFIG_SEC_FACTORY)
/* sysfs: /sys/class/touch/cmcs/cmcs_binary */
ssize_t ist40xx_cmcs_binary_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if ((ts_cmcs_bin == NULL) || (ts_cmcs_bin_size == 0))
		return sprintf(buf, "Binary is not correct(%d)\n", ts_cmcs_bin_size);

	ret = ist40xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret)
		goto binary_end;

	mutex_lock(&data->lock);
	ret = ist40xx_cmcs_test(data, CMCS_FLAG_ALL);
	mutex_unlock(&data->lock);

binary_end:
	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}
#endif

/* sysfs: /sys/class/touch/cmcs/cmcs_custom */
ssize_t ist40xx_cmcs_custom_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int bin_size = 0;
	u8 *bin = NULL;
	const struct firmware *req_bin = NULL;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	ret = request_firmware(&req_bin, IST40XX_CMCS_NAME, &data->client->dev);
	if (ret)
		return sprintf(buf, "File not found, %s\n", IST40XX_CMCS_NAME);

	bin = (u8 *) req_bin->data;
	bin_size = (u32) req_bin->size;

	ret = ist40xx_get_cmcs_info(bin, bin_size);
	if (ret)
		goto custom_end;

	mutex_lock(&data->lock);
	ret = ist40xx_cmcs_test(data, CMCS_FLAG_ALL);
	mutex_unlock(&data->lock);

custom_end:
	release_firmware(req_bin);

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

#define MAX_FILE_PATH   255
/* sysfs: /sys/class/touch/cmcs/cmcs_sdcard */
ssize_t ist40xx_cmcs_sdcard_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int bin_size = 0;
	u8 *bin = NULL;
	const u8 *buff = 0;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FILE_PATH];
	struct ist40xx_data *data = dev_get_drvdata(dev);

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST40XX_CMCS_NAME);
	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		tsp_info("file %s open error:%d\n", fw_path, PTR_ERR(fp));
		ret = -ENOENT;
		goto err_file_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	buff = kzalloc((size_t) fsize, GFP_KERNEL);
	if (!buff) {
		tsp_info("fail to alloc buffer\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
	if (nread != fsize) {
		tsp_info("mismatch fw size\n");

		goto err_fw_size;
	}

	bin = (u8 *) buff;
	bin_size = (u32) fsize;

	filp_close(fp, current->files);
	set_fs(old_fs);
	tsp_info("firmware is loaded!!\n");

	ret = ist40xx_get_cmcs_info(bin, bin_size);
	if (ret)
		goto sdcard_end;

	mutex_lock(&data->lock);
	ret = ist40xx_cmcs_test(data, CMCS_FLAG_ALL);
	mutex_unlock(&data->lock);

sdcard_end:
err_fw_size:
	if (buff)
		kfree(buff);
err_alloc:
	filp_close(fp, NULL);
err_file_open:
	set_fs(old_fs);

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs : cmcs */
#if defined(CONFIG_SEC_FACTORY)
static DEVICE_ATTR(cmcs_binary, S_IRUGO, ist40xx_cmcs_binary_show, NULL);
#endif
static DEVICE_ATTR(cmcs_custom, S_IRUGO, ist40xx_cmcs_custom_show, NULL);
static DEVICE_ATTR(cmcs_sdcard, S_IRUGO, ist40xx_cmcs_sdcard_show, NULL);

static struct attribute *cmcs_attributes[] = {
#if defined(CONFIG_SEC_FACTORY)
	&dev_attr_cmcs_binary.attr,
#endif
	&dev_attr_cmcs_custom.attr,
	&dev_attr_cmcs_sdcard.attr,
	NULL,
};

static struct attribute_group cmcs_attr_group = {
	.attrs = cmcs_attributes,
};

extern struct class *ist40xx_class;
struct device *ist40xx_cmcs_dev;

static ssize_t cm_jit_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("CMJIT (%d * %d)\n", tsp->ch_num.rx, tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->cm_jit, data->node_buf,
				false);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static ssize_t cm_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("CM (%d * %d)\n", tsp->ch_num.rx, tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->cm, data->node_buf,
				false);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static ssize_t cm_slope_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("CM Slope (%d * %d)\n", tsp->ch_num.rx,
			 tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->slope, data->node_buf,
				false);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static ssize_t cs_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("CS (%d * %d)\n", tsp->ch_num.rx, tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->cs, data->node_buf,
				false);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static ssize_t line_cm_jit_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("Line CM Jit (%d * %d)\n", tsp->ch_num.rx,
			 tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->cm_jit, data->node_buf,
				true);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static ssize_t line_cm_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("Line CM (%d * %d)\n", tsp->ch_num.rx, tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->cm, data->node_buf,
				true);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static ssize_t line_cm_slope_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("Line CM Slope (%d * %d)\n", tsp->ch_num.rx, tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->slope, data->node_buf,
				true);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static ssize_t line_cs_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		if (cmcs_ready == CMCS_NOT_READY) {
			data->node_cnt = 0;
			return sprintf(buf, "CMCS test is not work!!\n");
		}

		tsp_verb("Line CS (%d * %d)\n", tsp->ch_num.rx, tsp->ch_num.tx);

		data->node_buf[0] = '\0';
		data->node_cnt = print_cmcs(data, cmcs_buf->cs, data->node_buf, true);
	}

	if (off >= MAX_BUF_SIZE)
		return 0;

	if (data->node_cnt <= 0)
		return 0;

	if (data->node_cnt < (MAX_BUF_SIZE / 2))
		count = data->node_cnt;
	else
		count = (MAX_BUF_SIZE / 2);

	memcpy(buf, data->node_buf + off, count);
	data->node_cnt -= count;

	return count;
}

static struct bin_attribute bin_cm_jit_attr = {
	.attr = {
		 .name = "cm_jit",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = cm_jit_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_cm_attr = {
	.attr = {
		 .name = "cm",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = cm_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_cm_slope_attr = {
	.attr = {
		 .name = "cm_slope",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = cm_slope_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_cs_attr = {
	.attr = {
		 .name = "cs",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = cs_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_line_cm_jit_attr = {
	.attr = {
		 .name = "line_cm_jit",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = line_cm_jit_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_line_cm_attr = {
	.attr = {
		 .name = "line_cm",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = line_cm_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_line_cm_slope_attr = {
	.attr = {
		 .name = "line_cm_slope",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = line_cm_slope_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_line_cs_attr = {
	.attr = {
		 .name = "line_cs",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = line_cs_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

void ist40xx_init_cmcs_bin_attribute(void)
{
	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_cm_jit_attr))
		tsp_err("Failed to create sysfs cm_jit bin file(%s)!\n", "cmcs");

	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_cm_attr))
		tsp_err("Failed to create sysfs cm bin file(%s)!\n", "cmcs");

	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_cm_slope_attr))
		tsp_err("Failed to create sysfs cm_slope bin file(%s)!\n", "cmcs");

	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_cs_attr))
		tsp_err("Failed to create sysfs cs bin file(%s)!\n", "cmcs");

	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_line_cm_jit_attr))
		tsp_err("Failed to create sysfs line_cm_jit bin file(%s)!\n", "cmcs");

	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_line_cm_attr))
		tsp_err("Failed to create sysfs line_cm bin file(%s)!\n", "cmcs");

	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_line_cm_slope_attr))
		tsp_err("Failed to create sysfs line_cm_slope bin file(%s)!\n", "cmcs");

	if (sysfs_create_bin_file(&ist40xx_cmcs_dev->kobj, &bin_line_cs_attr))
		tsp_err("Failed to create sysfs line_cs bin file(%s)!\n", "cmcs");
}

int ist40xx_init_cmcs_sysfs(struct ist40xx_data *data)
{
	int ret = 0;
	const struct firmware *cmcs_bin = NULL;

	/* /sys/class/touch/cmcs */
	ist40xx_cmcs_dev = device_create(ist40xx_class, NULL, 0, data, "cmcs");

	/* /sys/class/touch/cmcs/... */
	if (sysfs_create_group(&ist40xx_cmcs_dev->kobj, &cmcs_attr_group))
		tsp_err("Failed to create sysfs group(%s)!\n", "cmcs");

	/* /sys/class/touch/cmcs/... */
	ist40xx_init_cmcs_bin_attribute();

	if (data->dt_data->bringup == 1) {
		input_info(true, &data->client->dev, "%s skip (bringup 1)\n", __func__);
		return 0;
	}

	ret = request_firmware(&cmcs_bin, data->dt_data->cmcs_path,
			&data->client->dev);
	if (ret) {
		tsp_err("%s() do not loading cmcs image: %d\n", __func__, ret);
		return -1;
	}

	ts_cmcs_bin_size = cmcs_bin->size;
	ts_cmcs_bin = kzalloc(cmcs_bin->size, GFP_KERNEL);
	if (ts_cmcs_bin)
		memcpy(ts_cmcs_bin, cmcs_bin->data, cmcs_bin->size);
	else
		tsp_err("Failed to allocation cmcs mem\n");

	release_firmware(cmcs_bin);

	if (!ts_cmcs_bin)
		return -ENOMEM;

	return 0;
}
