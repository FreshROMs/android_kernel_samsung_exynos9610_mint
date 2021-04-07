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

#include <linux/uaccess.h>
#include <linux/file.h>

#include "ist40xx.h"
#include "ist40xx_update.h"
#include "ist40xx_misc.h"

#define TSP_CH_UNKNOWN		0
#define TSP_CH_UNUSED		1
#define TSP_CH_USED		2

static u32 ist40xx_frame_buf[IST40XX_MAX_NODE_NUM];
static u32 ist40xx_frame_cdcbuf[IST40XX_MAX_NODE_NUM];
static u32 ist40xx_frame_self_cdcbuf[IST40XX_MAX_SELF_NODE_NUM];
static u32 ist40xx_frame_prox_cdcbuf[IST40XX_MAX_SELF_NODE_NUM];
static u32 ist40xx_frame_lofsbuf[IST40XX_MAX_NODE_NUM];
static u32 ist40xx_frame_cpbuf[IST40XX_MAX_NODE_NUM];
static u32 ist40xx_frame_self_cpbuf[IST40XX_MAX_SELF_NODE_NUM];
static u32 ist40xx_frame_prox_cpbuf[IST40XX_MAX_SELF_NODE_NUM];
int ist40xx_check_valid_ch(struct ist40xx_data *data, int ch_tx, int ch_rx)
{
	TSP_INFO *tsp = &data->tsp_info;

	if ((ch_tx < 0) || (ch_tx >= tsp->ch_num.tx) || (ch_rx < 0) ||
			(ch_rx >= tsp->ch_num.rx))
		return TSP_CH_UNKNOWN;

	return TSP_CH_USED;
}

#define GET_LOFS(n)		((n & (1 << 12)) ? (n | 0xFFFFF000) : n)
int ist40xx_parse_touch_node(struct ist40xx_data *data,
		struct TSP_NODE_BUF *node)
{
	int i;
	u16 *cdc = (u16 *) &node->cdc;
	u16 *self_cdc = (u16 *) &node->self_cdc;
	u16 *prox_cdc = (u16 *) &node->prox_cdc;
	u16 *base = (u16 *) &node->base;
	u16 *self_base = (u16 *) &node->self_base;
	u16 *prox_base = (u16 *) &node->prox_base;
	s16 *lofs = (s16 *) &node->lofs;
	u32 *tmp_cdcbuf = ist40xx_frame_cdcbuf;
	u32 *tmp_self_cdcbuf = ist40xx_frame_self_cdcbuf;
	u32 *tmp_prox_cdcbuf = ist40xx_frame_prox_cdcbuf;
	u32 *tmp_lofsbuf = ist40xx_frame_lofsbuf;

	for (i = 0; i < node->self_len; i++) {
		*self_cdc++ = *tmp_self_cdcbuf & 0xFFF;
		*self_base++ = (*tmp_self_cdcbuf >> 16) & 0xFFF;

		tmp_self_cdcbuf++;
	}

	for (i = 0; i < node->self_len; i++) {
		*prox_cdc++ = *tmp_prox_cdcbuf & 0xFFF;
		*prox_base++ = (*tmp_prox_cdcbuf >> 16) & 0xFFF;

		tmp_prox_cdcbuf++;
	}

	for (i = 0; i < node->len; i++) {
		*cdc++ = *tmp_cdcbuf & 0xFFF;
		*base++ = (*tmp_cdcbuf >> 16) & 0xFFF;

		tmp_cdcbuf++;

		*lofs++ = GET_LOFS(*tmp_lofsbuf);
		tmp_lofsbuf++;
	}

	return 0;
}

int print_touch_node(struct ist40xx_data *data, u8 flag,
		struct TSP_NODE_BUF *node, char *buf)
{
	int i, j;
	int count = 0;
	int val = 0;
	const int msg_len = 128;
	char msg[msg_len];
	TSP_INFO *tsp = &data->tsp_info;

	if (tsp->dir.swap_xy) {
		for (i = 0; i < tsp->ch_num.rx; i++) {
			if (i == 0) {
				count += snprintf(msg, msg_len, "%6s%6s", "P  ", "TX  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.tx; j++) {
					if (flag == NODE_FLAG_CDC) {
						val = (int)node->prox_cdc[j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->prox_base[j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->prox_cdc[j] - node->prox_base[j]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);

				count += snprintf(msg, msg_len, "%6s%6s", "RX  ", "S  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.tx; j++) {
					if (flag == NODE_FLAG_CDC) {
						val = (int)node->self_cdc[j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->self_base[j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->self_cdc[j] - node->self_base[j]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);
			}

			for (j = 0; j < tsp->ch_num.tx; j++) {
				if (j == 0) {
					if (flag == NODE_FLAG_CDC) {
						val = (int)node->prox_cdc[tsp->ch_num.tx + i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->prox_base[tsp->ch_num.tx + i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->prox_cdc[tsp->ch_num. tx + i] -
								node->prox_base[tsp->ch_num.tx + i]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d  ", val);
					strncat(buf, msg, msg_len);

					if (flag == NODE_FLAG_CDC) {
						val = (int)node->self_cdc[tsp->ch_num.tx + i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->self_base[tsp->ch_num.tx + i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->self_cdc[tsp->ch_num. tx + i] -
								node->self_base[tsp->ch_num.tx + i]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d  ", val);
					strncat(buf, msg, msg_len);
				}

				if (flag == NODE_FLAG_CDC) {
					val = (int)node->cdc[(j * tsp->ch_num.rx) + i];
					if (val < 0)
						val = 0;
				} else if (flag == NODE_FLAG_BASE) {
					val = (int)node->base[(j * tsp->ch_num.rx) + i];
					if (val < 0)
						val = 0;
				} else if (flag == NODE_FLAG_DIFF) {
					val = (int)(node->cdc[(j * tsp->ch_num.rx) + i] -
							node->base[(j * tsp->ch_num.rx) + i]);
				} else if (flag == NODE_FLAG_LOFS) {
					val = (int)node->lofs[(j * tsp->ch_num.rx) + i];
				} else {
					return 0;
				}

				if (ist40xx_check_valid_ch(data, j, i) == TSP_CH_USED)
					count += snprintf(msg, msg_len, "%4d ", val);
				else
					count += snprintf(msg, msg_len, "%4d ", 0);

				strncat(buf, msg, msg_len);
			}

			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	} else {
		for (i = 0; i < tsp->ch_num.tx; i++) {
			if (i == 0) {
				count += snprintf(msg, msg_len, "%6s%6s", "P  ", "RX  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.rx; j++) {
					if (flag == NODE_FLAG_CDC) {
						val = (int)node->prox_cdc[tsp->ch_num.tx + j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->prox_base[tsp->ch_num.tx + j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->prox_cdc[tsp->ch_num.tx + j] -
								node->prox_base[tsp->ch_num.tx + j]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);

				count += snprintf(msg, msg_len, "%6s%6s", "TX  ", "S  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.rx; j++) {
					if (flag == NODE_FLAG_CDC) {
						val = (int)node->self_cdc[tsp->ch_num.tx + j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->self_base[tsp->ch_num.tx + j];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->self_cdc[tsp->ch_num.tx + j] -
								node->self_base[tsp->ch_num.tx + j]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);
			}

			for (j = 0; j < tsp->ch_num.rx; j++) {
				if (j == 0) {
					if (flag == NODE_FLAG_CDC) {
						val = (int)node->prox_cdc[i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->prox_base[i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->prox_cdc[i] - node->prox_base[i]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);

					if (flag == NODE_FLAG_CDC) {
						val = (int)node->self_cdc[i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_BASE) {
						val = (int)node->self_base[i];
						if (val < 0)
							val = 0;
					} else if (flag == NODE_FLAG_DIFF) {
						val = (int)(node->self_cdc[i] - node->self_base[i]);
					} else if (flag == NODE_FLAG_LOFS) {
						val = 0;
					} else {
						return 0;
					}
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}

				if (flag == NODE_FLAG_CDC) {
					val = (int)node->cdc[(i * tsp->ch_num.rx) + j];
					if (val < 0)
						val = 0;
				} else if (flag == NODE_FLAG_BASE) {
					val = (int)node->base[(i * tsp->ch_num.rx) + j];
					if (val < 0)
						val = 0;
				} else if (flag == NODE_FLAG_DIFF) {
					val = (int)(node->cdc[(i * tsp->ch_num.rx) + j] -
							node->base[(i * tsp->ch_num.rx) + j]);
				} else if (flag == NODE_FLAG_LOFS) {
					val = (int)node->lofs[(i * tsp->ch_num.rx) + j];
				} else {
					return 0;
				}

				if (ist40xx_check_valid_ch(data, i, j) == TSP_CH_USED)
					count += snprintf(msg, msg_len, "%4d ", val);
				else
					count += snprintf(msg, msg_len, "%4d ", 0);

				strncat(buf, msg, msg_len);
			}

			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	}

	return count;
}

int parse_tsp_node(struct ist40xx_data *data, u8 flag,
		struct TSP_NODE_BUF *node, s16 *buf16, s16 *self_buf16, s16 *prox_buf16)
{
	int i, j;
	s16 val = 0;
	TSP_INFO *tsp = &data->tsp_info;

	if ((flag != NODE_FLAG_CDC) && (flag != NODE_FLAG_BASE) &&
			(flag != NODE_FLAG_DIFF))
		return -EPERM;

	for (i = 0; i < (tsp->ch_num.tx + tsp->ch_num.rx); i++) {
		switch (flag) {
		case NODE_FLAG_CDC:
			val = (s16)node->self_cdc[i];
			if (val < 0)
				val = 0;
			break;
		case NODE_FLAG_BASE:
			val = (s16)node->self_base[i];
			if (val < 0)
				val = 0;
			break;
		case NODE_FLAG_DIFF:
			val = (s16)(node->self_cdc[i] - node->self_base[i]);
			break;
		default:
			val = 0;
			break;
		}

		*self_buf16++ = val;
	}

	for (i = 0; i < (tsp->ch_num.tx + tsp->ch_num.rx); i++) {
		switch (flag) {
		case NODE_FLAG_CDC:
			val = (s16)node->prox_cdc[i];
			if (val < 0)
				val = 0;
			break;
		case NODE_FLAG_BASE:
			val = (s16)node->prox_base[i];
			if (val < 0)
				val = 0;
			break;
		case NODE_FLAG_DIFF:
			val = (s16)(node->prox_cdc[i] - node->prox_base[i]);
			break;
		default:
			val = 0;
			break;
		}

		*prox_buf16++ = val;
	}

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			if (ist40xx_check_valid_ch(data, i, j) == TSP_CH_UNKNOWN)
				continue;

			switch (flag) {
			case NODE_FLAG_CDC:
				val = (s16)node->cdc[(i * tsp->ch_num.rx) + j];
				if (val < 0)
					val = 0;
				break;
			case NODE_FLAG_BASE:
				val = (s16)node->base[(i * tsp->ch_num.rx) + j];
				if (val < 0)
					val = 0;
				break;
			case NODE_FLAG_DIFF:
				val = (s16)(node->cdc[(i * tsp->ch_num.rx) + j] -
						node->base[(i * tsp->ch_num.rx) + j]);
				break;
			default:
				val = 0;
				break;
			}

			if (ist40xx_check_valid_ch(data, i, j) == TSP_CH_UNUSED)
				val = 0;

			*buf16++ = val;
		}
	}

	return 0;
}

int ist40xx_read_touch_node(struct ist40xx_data *data, u8 flag,
		struct TSP_NODE_BUF *node)
{
	int ret;
	u32 addr;
	u32 *tmp_cdcbuf = ist40xx_frame_cdcbuf;
	u32 *tmp_self_cdcbuf = ist40xx_frame_self_cdcbuf;
	u32 *tmp_prox_cdcbuf = ist40xx_frame_prox_cdcbuf;
	u32 *tmp_lofsbuf = ist40xx_frame_lofsbuf;

	ist40xx_disable_irq(data);

	ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
	if (ret)
		goto err_read_node;

	addr = IST40XX_DA_ADDR(data->cdc_addr);
	tsp_info("MTL addr: %x, size: %d\n", addr, node->len);
	ret = ist40xx_burst_read(data->client, addr, tmp_cdcbuf, node->len, true);
	if (ret)
		goto err_read_node;

	addr = IST40XX_DA_ADDR(data->self_cdc_addr);
	tsp_info("SFL addr: %x, size: %d\n", addr, node->self_len);
	ret = ist40xx_burst_read(data->client, addr, tmp_self_cdcbuf,
			node->self_len, true);
	if (ret)
		goto err_read_node;

	addr = IST40XX_DA_ADDR(data->prox_cdc_addr);
	tsp_info("PROX addr: %x, size: %d\n", addr, node->self_len);
	ret = ist40xx_burst_read(data->client, addr, tmp_prox_cdcbuf,
			node->self_len, true);
	if (ret)
		goto err_read_node;

	addr = IST40XX_DA_ADDR(data->cdc_addr) + 0x4000;
	tsp_info("LOFS addr: %x, size: %d\n", addr, node->len);
	ret = ist40xx_burst_read(data->client, addr, tmp_lofsbuf, node->len, true);
	if (ret)
		goto err_read_node;

	ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
	if (ret) {
		ist40xx_reset(data, false);
		ist40xx_start(data);
	}

	ist40xx_enable_irq(data);

	return 0;

err_read_node:
	ist40xx_reset(data, false);
	ist40xx_start(data);
	ist40xx_enable_irq(data);

	return ret;
}

int ist40xx_parse_cp_node(struct ist40xx_data *data, struct TSP_NODE_BUF *node,
		bool ium)
{
	int i;
	int len;
	u32 *tmp_cpbuf = ist40xx_frame_cpbuf;
	u32 *tmp_self_cpbuf = ist40xx_frame_self_cpbuf;
	u32 *tmp_prox_cpbuf = ist40xx_frame_prox_cpbuf;
	u16 *cp;
	u16 *self_cp;
	u16 *prox_cp;

	if (ium) {
		cp = (u16 *)&node->rom_cp;
		self_cp = (u16 *)&node->rom_self_cp;
		prox_cp = (u16 *)&node->rom_prox_cp;
	} else {
		cp = (u16 *)&node->memx_cp;
		self_cp = (u16 *)&node->memx_self_cp;
		prox_cp = (u16 *)&node->memx_prox_cp;
	}

	if (ium) {
		len = node->self_len / 2;
		for (i = 0; i < len; i++) {
			*self_cp++ = *tmp_self_cpbuf & 0x7FF;
			*self_cp++ = (*tmp_self_cpbuf >> 16) & 0x7FF;

			tmp_self_cpbuf++;

			*prox_cp++ = *tmp_prox_cpbuf & 0x7FF;
			*prox_cp++ = (*tmp_prox_cpbuf >> 16) & 0x7FF;

			tmp_prox_cpbuf++;
		}

		if (node->self_len % 2) {
			*self_cp++ = *tmp_self_cpbuf & 0x7FF;
			*prox_cp++ = *tmp_prox_cpbuf & 0x7FF;
		}

		len = node->len / 2;
		for (i = 0; i < len; i++) {
			*cp++ = *tmp_cpbuf & 0x7FF;
			*cp++ = (*tmp_cpbuf >> 16) & 0x7FF;

			tmp_cpbuf++;
		}

		if (node->len % 2)
			*cp++ = *tmp_cpbuf & 0x7FF;
	} else {
		len = node->self_len;
		for (i = 0; i < len; i++) {
			*self_cp++ = *tmp_self_cpbuf & 0x7FF;
			tmp_self_cpbuf++;
		}

		len = node->self_len / 2;
		for (i = 0; i < len; i++) {
			*prox_cp++ = *tmp_prox_cpbuf & 0x7FF;
			*prox_cp++ = (*tmp_prox_cpbuf >> 16) & 0x7FF;

			tmp_prox_cpbuf++;
		}

		if (node->self_len % 2) {
			*prox_cp++ = *tmp_prox_cpbuf & 0x7FF;
		}

		len = node->len;
		for (i = 0; i < len; i++) {
			*cp++ = *tmp_cpbuf & 0x7FF;

			tmp_cpbuf++;
		}
	}

	return 0;
}

int parse_cp_node(struct ist40xx_data *data, struct TSP_NODE_BUF *node,
		s16 *buf16, s16 *self_buf16, s16 *prox_buf16, bool ium)
{
	int i, j;
	s16 val = 0;
	TSP_INFO *tsp = &data->tsp_info;

	for (i = 0; i < (tsp->ch_num.tx + tsp->ch_num.rx); i++) {
		if (ium)
			val = (s16)node->rom_self_cp[i];
		else
			val = (s16)node->memx_self_cp[i];
		if (val < 0)
			val = 0;

		*self_buf16++ = val;
	}

	for (i = 0; i < (tsp->ch_num.tx + tsp->ch_num.rx); i++) {
		if (ium)
			val = (s16)node->rom_prox_cp[i];
		else
			val = (s16)node->memx_prox_cp[i];
		if (val < 0)
			val = 0;

		*prox_buf16++ = val;
	}

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			if (ist40xx_check_valid_ch(data, i, j) != TSP_CH_USED)
				continue;

			if (ium)
				val = (s16)node->rom_cp[(i * tsp->ch_num.rx) + j];
			else
				val = (s16)node->memx_cp[(i * tsp->ch_num.rx) + j];

			if (val < 0)
				val = 0;

			*buf16++ = val;
		}
	}

	return 0;
}

int print_cp_node(struct ist40xx_data *data, struct TSP_NODE_BUF *node,
		char *buf, bool ium)
{
	int i, j;
	int count = 0;
	u32 val = 0;
	const int msg_len = 128;
	char msg[msg_len];
	TSP_INFO *tsp = &data->tsp_info;

	if (tsp->dir.swap_xy) {
		for (i = 0; i < tsp->ch_num.rx; i++) {
			if (i == 0) {
				count += snprintf(msg, msg_len, "%6s%6s", "P  ", "TX  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.tx; j++) {
					if (ium)
						val = node->rom_prox_cp[j];
					else
						val = node->memx_prox_cp[j];
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);

				count += snprintf(msg, msg_len, "%6s%6s", "RX  ", "S  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.tx; j++) {
					if (ium)
						val = node->rom_self_cp[j];
					else
						val = node->memx_self_cp[j];
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);
			}

			for (j = 0; j < tsp->ch_num.tx; j++) {
				if (j == 0) {
					if (ium)
						val = node->rom_prox_cp[tsp->ch_num.tx + i];
					else
						val = node->memx_prox_cp[tsp->ch_num.tx + i];

					if (val < 0)
						val = 0;

					count += snprintf(msg, msg_len, "%4d  ", val);
					strncat(buf, msg, msg_len);

					if (ium)
						val = node->rom_self_cp[tsp->ch_num.tx + i];
					else
						val = node->memx_self_cp[tsp->ch_num.tx + i];

					if (val < 0)
						val = 0;

					count += snprintf(msg, msg_len, "%4d  ", val);
					strncat(buf, msg, msg_len);
				}

				if (ium)
					val = node->rom_cp[(j * tsp->ch_num.rx) + i];
				else
					val = node->memx_cp[(j * tsp->ch_num.rx) + i];

				if (ist40xx_check_valid_ch(data, j, i) == TSP_CH_USED)
					count += snprintf(msg, msg_len, "%4d ", val);
				else
					count += snprintf(msg, msg_len, "%4d ", 0);

				strncat(buf, msg, msg_len);
			}

			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	} else {
		for (i = 0; i < tsp->ch_num.tx; i++) {
			if (i == 0) {
				count += snprintf(msg, msg_len, "%6s%6s", "P  ", "RX  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.rx; j++) {
					if (ium)
						val = node->rom_prox_cp[tsp->ch_num.tx + j];
					else
						val = node->memx_prox_cp[tsp->ch_num.tx + j];
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);

				count += snprintf(msg, msg_len, "%6s%6s", "TX  ", "S  ");
				strncat(buf, msg, msg_len);
				for (j = 0; j < tsp->ch_num.rx; j++) {
					if (ium)
						val = node->rom_self_cp[tsp->ch_num.tx + j];
					else
						val = node->memx_self_cp[tsp->ch_num.tx + j];
					count += snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				count += snprintf(msg, msg_len, "\n\n");
				strncat(buf, msg, msg_len);
			}

			for (j = 0; j < tsp->ch_num.rx; j++) {
				if (j == 0) {
					if (ium)
						val = node->rom_prox_cp[i];
					else
						val = node->memx_prox_cp[i];
					count += snprintf(msg, msg_len, "%4d  ", val);
					strncat(buf, msg, msg_len);

					if (ium)
						val = node->rom_self_cp[i];
					else
						val = node->memx_self_cp[i];
					count += snprintf(msg, msg_len, "%4d  ", val);
					strncat(buf, msg, msg_len);
				}

				if (ium)
					val = node->rom_cp[(i * tsp->ch_num.rx) + j];
				else
					val = node->memx_cp[(i * tsp->ch_num.rx) + j];

				if (ist40xx_check_valid_ch(data, i, j) == TSP_CH_USED)
					count += snprintf(msg, msg_len, "%4d ", val);
				else
					count += snprintf(msg, msg_len, "%4d ", 0);

				strncat(buf, msg, msg_len);
			}

			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	}

	return count;
}

int ist40xx_read_cp_node(struct ist40xx_data *data, struct TSP_NODE_BUF *node,
		bool ium)
{
	int ret = 0;
	u32 addr;
	u32 size;
	u32 *tmp_buf;
	u32 *tmp_cpbuf = ist40xx_frame_cpbuf;
	u32 *tmp_self_cpbuf = ist40xx_frame_self_cpbuf;
	u32 *tmp_prox_cpbuf = ist40xx_frame_prox_cpbuf;

	ist40xx_disable_irq(data);

	if (ium) {
		ist40xx_disable_irq(data);
		ist40xx_reset(data, true);
		ist40xx_isp_enable(data, true);
		data->ignore_delay = true;

		tmp_buf = kzalloc(IST40XX_IUM_SIZE, GFP_KERNEL);
		if (!tmp_buf) {
			data->ignore_delay = false;
			tsp_err("failed to allocate %s\n", __func__);
			return ret;
		}

		tsp_info("IMU Area Read\n");
		ret = ist40xx_ium_read(data, tmp_buf);
		if (ret) {
			data->ignore_delay = false;
			goto err_read_cp;
		}

		ist40xx_isp_enable(data, false);
		data->ignore_delay = false;

		memcpy(tmp_self_cpbuf, tmp_buf, IST40XX_MAX_SELF_NODE_NUM * 2);
		memcpy(tmp_cpbuf, tmp_buf + 0x1C, IST40XX_MAX_NODE_NUM * 2);
		memcpy(tmp_prox_cpbuf, tmp_buf + 0x270, IST40XX_MAX_SELF_NODE_NUM * 2);
	} else {
		ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
		if (ret)
			goto err_read_cp;

		addr = IST40XX_DA_ADDR(data->cp_addr);
		tsp_info("MemX MTL addr: %x, size: %d\n", addr, node->len);
		ret = ist40xx_burst_read(data->client, addr, tmp_cpbuf, node->len,
				true);
		if (ret)
			goto err_read_cp;

		addr = IST40XX_DA_ADDR(data->self_cp_addr);
		tsp_info("MemX SFL addr: %x, size: %d\n", addr, node->self_len);
		ret = ist40xx_burst_read(data->client, addr, tmp_self_cpbuf,
				node->self_len, true);
		if (ret)
			goto err_read_cp;

		addr = IST40XX_DA_ADDR(data->prox_cp_addr);
		size = (node->self_len * sizeof(u16)) / IST40XX_DATA_LEN;
		if ((node->self_len * sizeof(u16)) % IST40XX_DATA_LEN)
			size += 1;
		tsp_info("MemX Prox addr: %x, size: %d\n", addr, node->self_len);
		ret = ist40xx_burst_read(data->client, addr, tmp_prox_cpbuf,
				node->self_len, true);
		if (ret)
			goto err_read_cp;

		ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
		if (ret) {
			ist40xx_reset(data, false);
			ist40xx_start(data);
		}

		ist40xx_enable_irq(data);

		return 0;
	}

err_read_cp:
	ist40xx_reset(data, false);
	ist40xx_start(data);
	ist40xx_enable_irq(data);

	kfree(tmp_buf);  // Prevent CID: 56767
	return ret;

}

/* sysfs: /sys/class/touch/node/refresh */
ssize_t ist40xx_refresh_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;
	u8 flag = NODE_FLAG_CDC | NODE_FLAG_BASE;

	tsp_info("refresh node value\n");
	mutex_lock(&data->lock);
	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		mutex_unlock(&data->lock);
		tsp_err("frame read fail\n");
		return sprintf(buf, "FAIL\n");
	}
	mutex_unlock(&data->lock);

	ist40xx_parse_touch_node(data, &tsp->node);

	return sprintf(buf, "OK\n");
}

/* sysfs: /sys/class/touch/sys/debug_mode */
ssize_t ist40xx_debug_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &enable) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((enable != 0) && (enable != 1)) {
		tsp_err("input data error(%d)\n", enable);
		return size;
	}

	data->debug_mode = enable;

	if (data->status.sys_mode != STATE_POWER_OFF) {
		if (enable)
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SLEEP_MODE_EN << 16) | (IST40XX_DISABLE & 0xFFFF));
		else
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SLEEP_MODE_EN << 16) | (IST40XX_ENABLE & 0xFFFF));
	}

	tsp_info("debug mode %s\n", enable ? "start" : "stop");

	return size;
}

/* sysfs: /sys/class/touch/sys/clb */
ssize_t ist40xx_calib_show(struct device *dev, struct device_attribute *attr,
	   char *buf)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->lock);
	ist40xx_disable_irq(data);

	ist40xx_reset(data, false);
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(data->tdata, CALPOSITION_TESTMODE);
	sec_execute_tclm_package(data->tdata, 0);
	sec_tclm_root_of_cal(data->tdata, CALPOSITION_NONE);
#else
	ist40xx_calibrate(data, 1);
#endif
	mutex_unlock(&data->lock);
	ist40xx_start(data);

	return 0;
}

/* sysfs: /sys/class/touch/sys/clb_result */
ssize_t ist40xx_calib_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int count = 0;
	u32 calib_msg[3];
	struct ist40xx_data *data = dev_get_drvdata(dev);

	ret = ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_S, &calib_msg[0]);
	if (ret) {
		mutex_lock(&data->lock);
		ist40xx_reset(data, false);
		ist40xx_start(data);
		mutex_unlock(&data->lock);
		tsp_warn("Error Read Calibration Result\n");
		count = sprintf(buf, "Error Read Calibration Result\n");
		goto calib_show_end;
	}

	ret = ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_M, &calib_msg[1]);
	if (ret) {
		mutex_lock(&data->lock);
		ist40xx_reset(data, false);
		ist40xx_start(data);
		mutex_unlock(&data->lock);
		tsp_warn("Error Read Calibration Result\n");
		count = sprintf(buf, "Error Read Calibration Result\n");
		goto calib_show_end;
	}

	ret = ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_P, &calib_msg[2]);
	if (ret) {
		mutex_lock(&data->lock);
		ist40xx_reset(data, false);
		ist40xx_start(data);
		mutex_unlock(&data->lock);
		tsp_warn("Error Read Calibration Result\n");
		count = sprintf(buf, "Error Read Calibration Result\n");
		goto calib_show_end;
	}

	count = sprintf(buf, "SLF Calibration Status : %d, Max gap : %d - (%08x)\n"
			"MTL Calibration Status : %d, Max gap : %d - (%08x)\n"
			"PROX Calibration Status : %d, Max gap : %d - (%08x)\n",
			CALIB_TO_STATUS(calib_msg[0]), CALIB_TO_GAP(calib_msg[0]),
			calib_msg[0], CALIB_TO_STATUS(calib_msg[1]),
			CALIB_TO_GAP(calib_msg[1]), calib_msg[1],
			CALIB_TO_STATUS(calib_msg[2]), CALIB_TO_GAP(calib_msg[2]),
			calib_msg[2]);

calib_show_end:

	return count;
}

int print_miscalib_node(struct ist40xx_data *data, char *buf)
{
	int i, j, idx;
	int count = 0;
	const int msg_len = 128;
	char msg[msg_len];
	TSP_INFO *tsp = &data->tsp_info;
	struct TSP_NODE_BUF *node = &tsp->node;

	count = snprintf(msg, msg_len, "[miscal]\n");
	strncat(buf, msg, msg_len);

	if (tsp->dir.swap_xy) {
		for (i = 0; i < tsp->ch_num.rx; i++) {
			for (j = 0; j < tsp->ch_num.tx; j++) {
				if (ist40xx_check_valid_ch(data, j, i) != TSP_CH_USED) {
					count += snprintf(msg, msg_len, "%4d ", 0);
					strncat(buf, msg, msg_len);
					continue;
				}

				idx = (j * tsp->ch_num.rx) + i;

				count += snprintf(msg, msg_len, "%4d ", node->miscal[idx]);
				strncat(buf, msg, msg_len);
			}
			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	} else {
		for (i = 0; i < tsp->ch_num.tx; i++) {
			for (j = 0; j < tsp->ch_num.rx; j++) {
				if (ist40xx_check_valid_ch(data, i, j) != TSP_CH_USED) {
					count += snprintf(msg, msg_len, "%4d ", 0);
					strncat(buf, msg, msg_len);
					continue;
				}

				idx = (i * tsp->ch_num.rx) + j;
				count += snprintf(msg, msg_len, "%4d ", node->miscal[idx]);
				strncat(buf, msg, msg_len);
			}
			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
	}

	return count;
}

/* sysfs: /sys/class/touch/sys/miscal */
ssize_t ist40xx_miscalib_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	int count;
	const int msg_len = 128;
	char msg[msg_len];
	struct ist40xx_data *data = dev_get_drvdata(dev);

	ret = ist40xx_miscalibrate(data);
	if (ret) {
		count = sprintf(buf, "FAIL\n");
		return count;
	}

	count = print_miscalib_node(data, buf);

	count += snprintf(msg, msg_len, "MAX : %d\n",
			data->status.miscalib_result);
	strncat(buf, msg, msg_len);

	return count;
}

/* sysfs: /sys/class/touch/sys/miscal_result */
ssize_t ist40xx_miscalib_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int count;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	ret = ist40xx_miscalibrate(data);
	if (ret) {
		count = sprintf(buf, "FAIL\n");
		return count;
	}

	count = sprintf(buf, "MAX : %d\n", data->status.miscalib_result);

	return count;
}

/* sysfs: /sys/class/touch/sys/errcnt */
ssize_t ist40xx_errcnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int err_cnt;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &err_cnt) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if (err_cnt < 0)
		return size;

	tsp_info("Request reset error count: %d\n", err_cnt);

	data->max_irq_err_cnt = err_cnt;

	return size;
}

/* sysfs: /sys/class/touch/sys/scancnt */
ssize_t ist40xx_scancnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int retry;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &retry) < 0) {
		tsp_err("input data kstrtoint fail\n");
		return size;
	}

	if (retry < 0)
		return size;

	tsp_info("Timer scan count retry: %d\n", retry);

	data->max_scan_retry = retry;

	return size;
}

/* sysfs: /sys/class/touch/sys/timerms */
ssize_t ist40xx_timerms_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ms;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &ms) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((ms < 0) || (ms > 10000))
		return size;

	tsp_info("Timer period ms: %dms\n", ms);

	data->timer_period_ms = ms;

	return size;
}

extern int ist40xx_log_level;
/* sysfs: /sys/class/touch/sys/printk */
ssize_t ist40xx_printk_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int level;

	if (kstrtoint(buf, 10, &level) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((level < DEV_ERR) || (level > DEV_VERB))
		return size;

	tsp_info("prink log level: %d\n", level);

	ist40xx_log_level = level;

	return size;
}

ssize_t ist40xx_printk_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "prink log level: %d\n", ist40xx_log_level);
}

/* sysfs: /sys/class/touch/sys/printk5 */
ssize_t ist40xx_printk5_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	tsp_info("prink log level:%d\n", DEV_DEBUG);

	ist40xx_log_level = DEV_DEBUG;

	return 0;
}

/* sysfs: /sys/class/touch/sys/printk6 */
ssize_t ist40xx_printk6_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	tsp_info("prink log level:%d\n", DEV_VERB);

	ist40xx_log_level = DEV_VERB;

	return 0;
}

/* sysfs: /sys/class/touch/sys/report_rate */
ssize_t ist40xx_report_rate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int rate;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &rate) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if (rate > 0xFFFF)	// over 65.5ms
		return size;

	data->report_rate = rate;
	ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			  ((eHCOM_SET_TIME_ACTIVE << 16) |
			   (data->report_rate & 0xFFFF)));
	tsp_info(" active rate : %dus\n", data->report_rate);

	return size;
}

/* sysfs: /sys/class/touch/sys/idle_rate */
ssize_t ist40xx_idle_scan_rate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int rate;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &rate) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if (rate > 0xFFFF)	// over 65.5ms
		return size;

	data->idle_rate = rate;
	ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			((eHCOM_SET_TIME_IDLE << 16) | (data->idle_rate & 0xFFFF)));
	tsp_info(" idle rate : %dus\n", data->idle_rate);

	return size;
}

/* sysfs: /sys/class/touch/sys/mode_ta */
ssize_t ist40xx_ta_mode_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((mode != 0) && (mode != 1))	// enable/disable
		return size;

	ist40xx_set_ta_mode(mode);

	return size;
}

/* sysfs: /sys/class/touch/sys/mode_call */
ssize_t ist40xx_call_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((mode != 0) && (mode != 1))	// enable/disable
		return size;

	ist40xx_set_call_mode(mode);

	return size;
}

/* sysfs: /sys/class/touch/sys/mode_cover */
ssize_t ist40xx_cover_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((mode != 0) && (mode != 1))	// enable/disable
		return size;

	ist40xx_set_cover_mode(mode);

	return size;
}

/* sysfs: /sys/class/touch/sys/mode_glove */
ssize_t ist40xx_glove_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((mode != 0) && (mode != 1))	// enable/disable
		return size;

	ist40xx_set_glove_mode(mode);

	return size;
}

/* sysfs: /sys/class/touch/sys/mode_sensitibity */
ssize_t ist40xx_sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		tsp_err("%s kstrtoint fail\n", __func__);
		return size;
	}

	if ((mode != 0) && (mode != 1))   // enable/disable
		return size;

	ist40xx_set_sensitivity_mode(mode);

	return size;
}

ssize_t ist40xx_sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 result[5] = { 0, };
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (data->status.sys_mode == STATE_POWER_OFF) {
		tsp_err("%s() error currently power off \n", __func__);
		goto end;
	}

	if (data->noise_mode & NOISE_MODE_SENSITIVITY)
		ist40xx_burst_read(data->client, IST40XX_HIB_INTR_MSG, result, 5, true);

end:
	return sprintf(buf, "%d,%d,%d,%d,%d", result[0], result[1], result[2],
			result[3], result[4]);
}

/* sysfs: /sys/class/touch/sys/ic_inform */
ssize_t ist40xx_read_ic_inform(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);

	ist40xx_print_info(data);

	return 0;
}

#define TUNES_CMD_WRITE			(1)
#define TUNES_CMD_READ			(2)
#define TUNES_CMD_REG_ENTER		(3)
#define TUNES_CMD_REG_EXIT		(4)
#define TUNES_CMD_UPDATE_PARAM		(5)
#define TUNES_CMD_UPDATE_FW		(6)

#define DIRECT_ADDR(n)			(IST40XX_DA_ADDR(n))
#define DIRECT_CMD_WRITE		('w')
#define DIRECT_CMD_READ			('r')
#define DIRECT_BUF_COUNT		(4)

#pragma pack(1)
typedef struct {
	u8 cmd;
	u32 addr;
	u16 len;
} TUNES_INFO;
#pragma pack()

#pragma pack(1)
typedef struct {
	char cmd;
	u32 addr;
	u32 val;
	u32 size;
} DIRECT_INFO;
#pragma pack()

static TUNES_INFO ist40xx_tunes;
static DIRECT_INFO ist40xx_direct;
static bool tunes_cmd_done = false;
static bool ist40xx_reg_mode = false;
/* sysfs: /sys/class/touch/sys/direct */
ssize_t ist40xx_direct_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = -EPERM;
	struct ist40xx_data *data = dev_get_drvdata(dev);
	DIRECT_INFO *direct = (DIRECT_INFO *) &ist40xx_direct;
	char *buffer = NULL;
	char *buf_value;

	if (buf != NULL && size != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("input is empty\n");
			return size;
		}

		direct->cmd = *buf_value;

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &direct->addr) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &direct->val) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL, but don't care\n", __func__);
		} else if (kstrtou32(buf_value, 10, &direct->size) < 0) {
			tsp_err("kstrto32 fail. but don't care.\n");
		}

	} else {
		tsp_err("input buf data is NULL\n");
		return size;
	}

	if (direct->size == 0)
		direct->size = DIRECT_BUF_COUNT;

	tsp_debug("Direct cmd: %c, addr: %x, val: %x\n", direct->cmd, direct->addr,
			direct->val);

	if ((direct->cmd != DIRECT_CMD_WRITE) && (direct->cmd != DIRECT_CMD_READ)) {
		tsp_warn("Direct cmd is not correct!\n");
		return size;
	}

	if (ist40xx_intr_wait(data, 30) < 0)
		return size;

	data->status.event_mode = false;
	if (direct->cmd == DIRECT_CMD_WRITE) {
		ist40xx_cmd_hold(data, IST40XX_ENABLE);
		ist40xx_write_cmd(data, DIRECT_ADDR(direct->addr), direct->val);
		ist40xx_read_reg(data->client, DIRECT_ADDR(direct->addr), &direct->val);
		ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
		if (ret)
			tsp_debug("Direct write fail\n");
		else
			tsp_debug("Direct write addr: %x, val: %x\n", direct->addr,
					direct->val);

	}
	data->status.event_mode = true;

	return size;
}

ssize_t ist40xx_direct_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i, count = 0;
	int ret = 0;
	int len;
	u32 addr;
	u32 *buf32;
	const int msg_len = 256;
	char msg[msg_len];
	struct ist40xx_data *data = dev_get_drvdata(dev);
	DIRECT_INFO *direct = (DIRECT_INFO *) &ist40xx_direct;
	int max_len = direct->size;

	if (direct->cmd != DIRECT_CMD_READ)
		return sprintf(buf, "ex) echo r addr len size(display) > direct\n");

	len = direct->val;
	addr = DIRECT_ADDR(direct->addr);

	if (ist40xx_intr_wait(data, 30) < 0)
		return 0;

	if (data->status.sys_mode == STATE_POWER_OFF)
		return 0;

	data->status.event_mode = false;
	ist40xx_cmd_hold(data, IST40XX_ENABLE);

	buf32 = kzalloc(max_len * sizeof(u32), GFP_KERNEL);
	if (!buf32) {
		tsp_err("failed to allocate %s %d\n", __func__, __LINE__);
		return 0;
	}

	while (len > 0) {
		if (len < max_len)
			max_len = len;

		memset(buf32, 0, max_len * sizeof(u32));
		ret = ist40xx_burst_read(data->client, addr, buf32, max_len, true);
		if (ret) {
			count = sprintf(buf, "I2C Burst read fail\n");
			break;
		}
		addr += (max_len * IST40XX_DATA_LEN);

		for (i = 0; i < max_len; i++) {
			count += snprintf(msg, msg_len, "0x%08x ", buf32[i]);
			strncat(buf, msg, msg_len);
		}
		count += snprintf(msg, msg_len, "\n");
		strncat(buf, msg, msg_len);

		len -= max_len;
	}
	kfree(buf32);

	ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
	if (ret)
		tsp_err("Hold disable fail\n");
	data->status.event_mode = true;

	return count;
}

/* sysfs: /sys/class/touch/tunes/regcmd */
ssize_t tunes_regcmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = -1;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	memcpy(&ist40xx_tunes, buf, sizeof(ist40xx_tunes));
	buf += sizeof(ist40xx_tunes);

	tunes_cmd_done = false;

	switch (ist40xx_tunes.cmd) {
	case TUNES_CMD_WRITE:
		break;
	case TUNES_CMD_READ:
		break;
	case TUNES_CMD_REG_ENTER:
		ist40xx_disable_irq(data);
		/* enter reg access mode */
		ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
		if (ret)
			goto regcmd_fail;

		ist40xx_reg_mode = true;
		break;
	case TUNES_CMD_REG_EXIT:
		/* exit reg access mode */
		ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
		if (ret) {
			ist40xx_reset(data, false);
			ist40xx_start(data);
			goto regcmd_fail;
		}

		ist40xx_reg_mode = false;
		ist40xx_enable_irq(data);
		break;
	default:
		ist40xx_enable_irq(data);
		return size;
	}
	tunes_cmd_done = true;

	return size;

regcmd_fail:
	tsp_err("Tunes regcmd i2c_fail, ret=%d\n", ret);

	return size;
}

ssize_t tunes_regcmd_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int size;

	size = sprintf(buf, "cmd: 0x%02x, addr: 0x%08x, len: 0x%04x\n",
			ist40xx_tunes.cmd, ist40xx_tunes.addr, ist40xx_tunes.len);

	return size;
}

#define MAX_WRITE_LEN   (1)
/* sysfs: /sys/class/touch/tunes/reg */
ssize_t tunes_reg_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret;
	u32 *buf32 = (u32 *) buf;
	int waddr, wcnt = 0, len = 0;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (ist40xx_tunes.cmd != TUNES_CMD_WRITE) {
		tsp_err("error, IST40XX_REG_CMD is not correct!\n");
		return size;
	}

	if (!ist40xx_reg_mode) {
		tsp_err("error, IST40XX_REG_CMD is not ready!\n");
		return size;
	}

	if (!tunes_cmd_done) {
		tsp_err("error, IST40XX_REG_CMD is not ready!\n");
		return size;
	}

	waddr = ist40xx_tunes.addr;
	if (ist40xx_tunes.len >= MAX_WRITE_LEN)
		len = MAX_WRITE_LEN;
	else
		len = ist40xx_tunes.len;

	while (wcnt < ist40xx_tunes.len) {
		ret = ist40xx_write_buf(data->client, waddr, buf32, len);
		if (ret) {
			tsp_err("Tunes regstore i2c_fail, ret=%d\n", ret);
			return size;
		}

		wcnt += len;

		if ((ist40xx_tunes.len - wcnt) < MAX_WRITE_LEN)
			len = ist40xx_tunes.len - wcnt;

		buf32 += MAX_WRITE_LEN;
		waddr += MAX_WRITE_LEN * IST40XX_DATA_LEN;
	}

	tunes_cmd_done = false;

	return size;
}

ssize_t tunes_reg_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	int size = 0;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (ist40xx_tunes.cmd != TUNES_CMD_READ) {
		tsp_err("error, IST40XX_REG_CMD is not correct!\n");
		return 0;
	}

	if (!tunes_cmd_done) {
		tsp_err("error, IST40XX_REG_CMD is not ready!\n");
		return 0;
	}

	ret = ist40xx_burst_read(data->client, ist40xx_tunes.addr, (u32 *) buf,
			ist40xx_tunes.len, false);
	if (ret) {
		tsp_err("Tunes regshow i2c_fail, ret=%d\n", ret);
		return size;
	}

	size = ist40xx_tunes.len * IST40XX_DATA_LEN;

	tunes_cmd_done = false;

	return size;
}

/* sysfs: /sys/class/touch/tunes/adb */
ssize_t tunes_adb_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret;
	char *tmp, *ptr;
	char token[9];
	u32 cmd, addr, len, val;
	int write_len;
	char *buffer = NULL, *buf_value;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	if (buf != NULL && size != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &cmd) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &addr) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &len) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}
	} else {
		tsp_err("input buf data is NULL\n");
		return size;
	}

	switch (cmd) {
	case TUNES_CMD_WRITE:	/* write cmd */
		write_len = 0;
		ptr = (char *)(buf + 15);

		while (write_len < len) {
			memcpy(token, ptr, 8);
			token[8] = 0;
			val = simple_strtoul(token, &tmp, 16);
			ret = ist40xx_write_buf(data->client, addr, &val, 1);
			if (ret) {
				tsp_err("Tunes regstore i2c_fail, ret=%d\n", ret);
				return size;
			}

			ptr += 8;
			write_len++;
			addr += 4;
		}
		break;

	case TUNES_CMD_READ:	/* read cmd */
		ist40xx_tunes.cmd = cmd;
		ist40xx_tunes.addr = addr;
		ist40xx_tunes.len = len;
		break;

	case TUNES_CMD_REG_ENTER:	/* enter */
		ist40xx_disable_irq(data);

		ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
		if (ret < 0)
			goto cmd_fail;

		ist40xx_reg_mode = true;
		break;

	case TUNES_CMD_REG_EXIT:	/* exit */
		if (ist40xx_reg_mode == true) {
			ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
			if (ret < 0) {
				ist40xx_reset(data, false);
				ist40xx_start(data);
				goto cmd_fail;
			}

			ist40xx_reg_mode = false;
			ist40xx_enable_irq(data);
		}
		break;

	default:
		break;
	}

	return size;

cmd_fail:
	tsp_err("Tunes adb i2c_fail\n");
	return size;
}

ssize_t tunes_adb_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	int i, len, size = 0;
	char reg_val[10];
	struct ist40xx_data *data = dev_get_drvdata(dev);

	tsp_info("tunes_adb_show,%08x,%d\n", ist40xx_tunes.addr, ist40xx_tunes.len);

	ret = ist40xx_burst_read(data->client, ist40xx_tunes.addr,
			ist40xx_frame_buf, ist40xx_tunes.len, false);
	if (ret) {
		tsp_err("Tunes adbshow i2c_fail, ret=%d\n", ret);
		return size;
	}

	size = 0;
	buf[0] = 0;
	len = sprintf(reg_val, "%08x", ist40xx_tunes.addr);
	strcat(buf, reg_val);
	size += len;
	for (i = 0; i < ist40xx_tunes.len; i++) {
		len = sprintf(reg_val, "%08x", ist40xx_frame_buf[i]);
		strcat(buf, reg_val);
		size += len;
	}

	return size;
}

/* sysfs: /sys/class/touch/tunes/intr_debug */
ssize_t intr_debug_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);
	char *buffer = NULL, *buf_value;

	if (buf != NULL && size != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &data->intr_debug1_addr) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 10, &data->intr_debug1_size) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}
	} else {
		tsp_err("input buf data is NULL\n");
		return size;
	}

	tsp_info("intr debug1 addr: 0x%x, count: %d\n", data->intr_debug1_addr,
			data->intr_debug1_size);

	return size;
}

ssize_t intr_debug_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int count = 0;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	tsp_info("intr debug1 addr: 0x%x, count: %d\n", data->intr_debug1_addr,
			data->intr_debug1_size);

	count = sprintf(buf, "intr debug1 addr: 0x%x, count: %d\n",
			data->intr_debug1_addr, data->intr_debug1_size);

	return count;
}

/* sysfs: /sys/class/touch/tunes/intr_debug2 */
ssize_t intr_debug2_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);
	char *buffer = NULL, *buf_value;

	if (buf != NULL && size != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &data->intr_debug2_addr) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 10, &data->intr_debug2_size) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}
	} else {
		tsp_err("input buf data is NULL\n");
		return size;
	}

	tsp_info("intr debug2 addr: 0x%x, count: %d\n", data->intr_debug2_addr,
			data->intr_debug2_size);

	return size;
}

ssize_t intr_debug2_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int count = 0;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	tsp_info("intr debug2 addr: 0x%x, count: %d\n", data->intr_debug2_addr,
			data->intr_debug2_size);

	count = sprintf(buf, "intr debug2 addr: 0x%x, count: %d\n",
			data->intr_debug2_addr, data->intr_debug2_size);

	return count;
}

/* sysfs: /sys/class/touch/tunes/intr_debug3 */
ssize_t intr_debug3_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);
	char *buffer = NULL, *buf_value;

	if (buf != NULL && size != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);;
			return size;
		} else if (kstrtou32(buf_value, 16, &data->intr_debug3_addr) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 10, &data->intr_debug3_size) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}
	} else {
		tsp_err("input buf data is NULL\n");
		return size;
	}

	tsp_info("intr debug3 addr: 0x%x, count: %d\n", data->intr_debug3_addr,
			data->intr_debug3_size);

	return size;
}

ssize_t intr_debug3_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int count = 0;
	struct ist40xx_data *data = dev_get_drvdata(dev);

	tsp_info("intr debug3 addr: 0x%x, count: %d\n", data->intr_debug3_addr,
			data->intr_debug2_size);

	count = sprintf(buf, "intr debug3 addr: 0x%x, count: %d\n",
			data->intr_debug3_addr, data->intr_debug3_size);

	return count;
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define MAX_FRAME_COUNT			1024
IST40XX_RING_BUF RecordBuf;
IST40XX_RING_BUF *pRecordBuf;
bool recording_initialize = false;
void ist40xx_recording_init(struct ist40xx_data *data)
{
	if (!recording_initialize)
		pRecordBuf = &RecordBuf;

	pRecordBuf->RingBufCtr = 0;
	pRecordBuf->RingBufInIdx = 0;
	pRecordBuf->RingBufOutIdx = 0;

	data->recording_scancnt = 0;

	recording_initialize = true;
}

u32 ist40xx_get_recording_cnt(void)
{
	if (!recording_initialize)
		return 0;

	return pRecordBuf->RingBufCtr;
}

int ist40xx_recording_get_frame(struct ist40xx_data *data, u32 *frame, u32 cnt)
{
	int i;
	u8 *buf = (u8 *) frame;

	if (!recording_initialize)
		return 0;

	cnt *= IST40XX_DATA_LEN;

	if (pRecordBuf->RingBufCtr < cnt)
		return IST40XX_RINGBUF_NOT_ENOUGH;

	for (i = 0; i < cnt; i++) {
		if (pRecordBuf->RingBufOutIdx == IST40XX_MAX_RINGBUF_SIZE)
			pRecordBuf->RingBufOutIdx = 0;

		*buf++ = (u8) pRecordBuf->LogBuf[pRecordBuf->RingBufOutIdx++];
		pRecordBuf->RingBufCtr--;
	}

	return IST40XX_RINGBUF_NO_ERR;
}

#define REC_ENTER_VALUE			(0xFFFFFFFF)
int ist40xx_recording_put_frame(struct ist40xx_data *data, u32 *frame,
		int frame_cnt)
{
	int i;
	int size = 0;
	u32 *buf32;
	u8 *buf;

	if (!recording_initialize)
		return 0;

	buf32 = kzalloc((frame_cnt + 1) * sizeof(u32), GFP_KERNEL);
	if (!buf32) {
		tsp_err("failed to allocate %s %d\n", __func__, __LINE__);
		return 0;
	}

	for (i = 0; i < frame_cnt; i++) {
		buf32[i] = frame[i];
		size++;
	}
	buf32[size++] = REC_ENTER_VALUE;

	buf = (u8 *) buf32;
	size *= IST40XX_DATA_LEN;

	pRecordBuf->RingBufCtr += size;
	if (pRecordBuf->RingBufCtr > IST40XX_MAX_RINGBUF_SIZE) {
		pRecordBuf->RingBufOutIdx +=
			(pRecordBuf->RingBufCtr - IST40XX_MAX_RINGBUF_SIZE);
		if (pRecordBuf->RingBufOutIdx >= IST40XX_MAX_RINGBUF_SIZE)
			pRecordBuf->RingBufOutIdx -= IST40XX_MAX_RINGBUF_SIZE;

		pRecordBuf->RingBufCtr = IST40XX_MAX_RINGBUF_SIZE;
	}

	for (i = 0; i < size; i++) {
		if (pRecordBuf->RingBufInIdx == IST40XX_MAX_RINGBUF_SIZE)
			pRecordBuf->RingBufInIdx = 0;
		pRecordBuf->LogBuf[pRecordBuf->RingBufInIdx++] = *buf++;
	}

	kfree(buf32);

	return IST40XX_RINGBUF_NO_ERR;
}

/* sysfs: /sys/class/touch/tunes/rec_mode */
ssize_t ist40xx_rec_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int count;
	int mode;
	int delay = 0;
	char header[128];
	char *buffer = NULL, *buf_value;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	snprintf(data->rec_file_name, 128, "/sdcard/%s", IST40XX_REC_FILENAME);

	if (buf != NULL && size != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buffer, 10, &mode) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buffer, 10, &delay) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}
	} else {
		tsp_err("input buf data is NULL\n");
		return size;
	}

	old_fs = get_fs();
	set_fs(get_ds());

	if (mode) {
		fp = filp_open(data->rec_file_name, O_CREAT | O_WRONLY | O_TRUNC, 0);
		if (IS_ERR(fp)) {
			tsp_err("file %s open error:%d\n", data->rec_file_name,
					PTR_ERR(fp));
			goto err_file_open;
		}

		count = snprintf(header, 128, "2 2 %d %d %d %d\n", tsp->node.self_len,
				tsp->ch_num.tx, tsp->ch_num.rx, data->rec_size);

		fp->f_op->write(fp, header, count, &fp->f_pos);
		fput(fp);

		filp_close(fp, NULL);
	}

	data->rec_mode = mode;
	tsp_info("rec mode: %s\n", mode ? "start" : "stop");
	if (mode) {
		data->rec_delay = delay;
		tsp_info("rec delay: %dms\n", data->rec_delay);
	}

	ist40xx_recording_init(data);

	if (data->status.sys_mode != STATE_POWER_OFF) {
		if (data->debug_mode || data->rec_mode)
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SLEEP_MODE_EN << 16) | (IST40XX_DISABLE & 0xFFFF));
		else
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SLEEP_MODE_EN << 16) | (IST40XX_ENABLE & 0xFFFF));

		ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_SET_REC_MODE << 16) | (data->rec_mode & 0xFFFF));

		if (data->rec_mode)
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SET_REC_MODE << 16) | (IST40XX_START_SCAN & 0xFFFF));
	}

err_file_open:
	set_fs(old_fs);

	return size;
}

ssize_t ist40xx_rec_mode_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int count;
	char header[128];
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	old_fs = get_fs();
	set_fs(get_ds());

	if (data->rec_mode) {
		data->rec_mode = 0;
	} else {
		snprintf(data->rec_file_name, 128, "/sdcard/%s", IST40XX_REC_FILENAME);
		fp = filp_open(data->rec_file_name, O_CREAT | O_WRONLY | O_TRUNC, 0);
		if (IS_ERR(fp)) {
			tsp_err("file %s open error:%d\n", data->rec_file_name,
					PTR_ERR(fp));
			goto err_file_open;
		}

		count = snprintf(header, 128, "2 2 %d %d %d %d\n", tsp->node.self_len,
				tsp->ch_num.tx, tsp->ch_num.rx, data->rec_size);

		fp->f_op->write(fp, header, count, &fp->f_pos);
		fput(fp);

		filp_close(fp, NULL);

		data->rec_mode = 1;
	}

	tsp_info("rec mode: %s\n", data->rec_mode ? "start" : "stop");
	if (data->rec_mode)
		tsp_info("rec delay: %dms\n", data->rec_delay);

	ist40xx_recording_init(data);

	if (data->status.sys_mode != STATE_POWER_OFF) {
		if (data->debug_mode || data->rec_mode)
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SLEEP_MODE_EN << 16) | (IST40XX_DISABLE & 0xFFFF));
		else
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SLEEP_MODE_EN << 16) | (IST40XX_ENABLE & 0xFFFF));

		ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_SET_REC_MODE << 16) | (data->rec_mode & 0xFFFF));

		if (data->rec_mode)
			ist40xx_write_cmd(data, IST40XX_HIB_CMD,
					(eHCOM_SET_REC_MODE << 16) | (IST40XX_START_SCAN & 0xFFFF));
	}

err_file_open:
	set_fs(old_fs);

	return sprintf(buf, "%s", data->rec_mode ? "ENABLE" : "DISABLE");
}

/* sysfs: /sys/class/touch/tunes/recording_status */
ssize_t recording_status_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%s", data->rec_mode ? "ENABLE" : "DISABLE");
}

/* sysfs: /sys/class/touch/tunes/change_rec_delay */
ssize_t change_rec_delay_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);

	data->rec_delay += 10;

	if (data->rec_delay > 50)
		data->rec_delay = 0;

	return sprintf(buf, "%d", data->rec_delay);
}

/* sysfs: /sys/class/touch/tunes/rec_delay */
ssize_t rec_delay_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d", data->rec_delay);
}

/* sysfs: /sys/class/touch/tunes/recording_cnt */
ssize_t recording_cnt_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	u32 cnt = ist40xx_get_recording_cnt();

	tsp_verb("recording cnt: %d\n", cnt);

	return sprintf(buf, "%08x", cnt);
}

/* sysfs: /sys/class/touch/tunes/recording_frame */
ssize_t recording_frame_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i;
	int count = 0;
	u32 *frame_data = NULL;
	char msg[10];
	struct ist40xx_data *data = dev_get_drvdata(dev);
	u32 read_frame_cnt = 0;
	u32 frame_cnt = MAX_FRAME_COUNT;

	if (!recording_initialize)
		return count;

	frame_data = kzalloc(MAX_FRAME_COUNT, GFP_KERNEL);
	if (!frame_data) {
		tsp_err("failed to allocate %s %d\n", __func__, __LINE__);
		return count;
	}

	buf[0] = '\0';

	read_frame_cnt = ist40xx_get_recording_cnt();
	if (frame_cnt > read_frame_cnt)
		frame_cnt = read_frame_cnt;

	tsp_verb("num: %d of %d\n", frame_cnt, read_frame_cnt);

	frame_cnt /= IST40XX_DATA_LEN;

	if (ist40xx_recording_get_frame(data, frame_data, frame_cnt)) {
		kfree(frame_data);
		return count;
	}

	for (i = 0; i < frame_cnt; i++) {
		if (frame_data[i] != REC_ENTER_VALUE) {
			count += sprintf(msg, "%08x ", frame_data[i]);
		} else {
			tsp_verb("%08X\n", frame_data[i]);
			count += sprintf(msg, "\n");
		}
		strcat(buf, msg);
	}

	kfree(frame_data);
	return count;
}
#endif

#ifdef TCLM_CONCEPT
static DIRECT_INFO ist40xx_sec_info;
/* sysfs: /sys/class/touch/sys/sec_info */
ssize_t ist40xx_sec_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = -EPERM;
	struct ist40xx_data *data = dev_get_drvdata(dev);
	DIRECT_INFO *sec_info = (DIRECT_INFO *) &ist40xx_sec_info;
	char *buffer = NULL, *buf_value;

	if (buf != NULL && size != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		}
		sec_info->cmd = *buf_value;

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 10, &sec_info->addr) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 16, &sec_info->val) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}

		buf_value = strsep(&buffer, " ");
		if (buf_value == NULL) {
			tsp_err("%s buf_value is NULL error\n", __func__);
			return size;
		} else if (kstrtou32(buf_value, 10, &sec_info->size) < 0) {
			tsp_err("%s kstrto32 fail\n", __func__);
			return size;
		}
	} else {
		tsp_err("input buf data is NULL\n");
		return size;
	}

	if (sec_info->size == 0)
		sec_info->size = DIRECT_BUF_COUNT;

	tsp_debug("Sec info cmd: %c, index: %d, val: %x, display size: %d\n",
		  sec_info->cmd, sec_info->addr, sec_info->val, sec_info->size);

	if ((sec_info->cmd != DIRECT_CMD_WRITE) &&
			(sec_info->cmd != DIRECT_CMD_READ)) {
		tsp_warn("Direct cmd is not correct!\n");
		return size;
	}

	if (ist40xx_intr_wait(data, 30) < 0)
		return size;

	if (sec_info->cmd == DIRECT_CMD_WRITE) {
		ret = ist40xx_write_sec_info(data, sec_info->addr, &sec_info->val, 1);
		if (ret)
			tsp_err("sec info write fail\n");
		else
			tsp_debug("sec_info index: %d, val: %x\n", sec_info->addr,
					sec_info->val);
	}

	return size;
}

ssize_t ist40xx_sec_info_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i, count = 0;
	int ret = 0;
	int len;
	u32 *buf32;
	const int msg_len = 256;
	char msg[msg_len];
	struct ist40xx_data *data = dev_get_drvdata(dev);
	DIRECT_INFO *sec_info = (DIRECT_INFO *) &ist40xx_sec_info;

	if (sec_info->cmd != DIRECT_CMD_READ)
		return sprintf(buf, "ex) echo r idx len size(display) > sec_info\n");

	len = sec_info->val;

	if (ist40xx_intr_wait(data, 30) < 0)
		return 0;

	if (data->status.sys_mode == STATE_POWER_OFF)
		return 0;

	buf32 = kzalloc(len * sizeof(u32), GFP_KERNEL);
	if (!buf32) {
		tsp_err("failed to allocate %s %d\n", __func__, __LINE__);
		return count;
	}

	ret = ist40xx_read_sec_info(data, sec_info->addr, buf32, len);
	if (ret) {
		tsp_err("sec info read fail\n");
		count = sprintf(buf, "sec info read fail\n");
		kfree(buf32);
		return count;
	}

	for (i = 0; i < len; i++) {
		if (((i % sec_info->size) == 0) && (i != 0)) {
			count += snprintf(msg, msg_len, "\n");
			strncat(buf, msg, msg_len);
		}
		count += snprintf(msg, msg_len, "0x%08x ", buf32[i]);
		strncat(buf, msg, msg_len);
	}
	count += snprintf(msg, msg_len, "\n");
	strncat(buf, msg, msg_len);

	kfree(buf32);

	return count;
}
#endif

/* sysfs : node */
static DEVICE_ATTR(refresh, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_refresh_show,
		NULL);

/* sysfs : sys */
static DEVICE_ATTR(debug_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_debug_mode_store);
static DEVICE_ATTR(printk, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_printk_show,
		ist40xx_printk_store);
static DEVICE_ATTR(printk5, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_printk5_show,
		NULL);
static DEVICE_ATTR(printk6, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_printk6_show,
		NULL);
static DEVICE_ATTR(direct, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_direct_show,
		ist40xx_direct_store);
static DEVICE_ATTR(clb, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_calib_show, NULL);
static DEVICE_ATTR(clb_result, S_IRUGO | S_IWUSR | S_IWGRP,
		ist40xx_calib_result_show, NULL);
static DEVICE_ATTR(miscal, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_miscalib_show,
		NULL);
static DEVICE_ATTR(miscal_result, S_IRUGO | S_IWUSR | S_IWGRP,
		ist40xx_miscalib_result_show, NULL);
static DEVICE_ATTR(errcnt, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_errcnt_store);
static DEVICE_ATTR(scancnt, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_scancnt_store);
static DEVICE_ATTR(timerms, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_timerms_store);
static DEVICE_ATTR(report_rate, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_report_rate_store);
static DEVICE_ATTR(idle_rate, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_idle_scan_rate_store);
static DEVICE_ATTR(mode_ta, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_ta_mode_store);
static DEVICE_ATTR(mode_call, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_call_mode_store);
static DEVICE_ATTR(mode_cover, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_cover_mode_store);
static DEVICE_ATTR(mode_glove, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		ist40xx_glove_mode_store);
static DEVICE_ATTR(mode_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
		ist40xx_sensitivity_mode_show, ist40xx_sensitivity_mode_store);
static DEVICE_ATTR(ic_inform, S_IRUGO, ist40xx_read_ic_inform, NULL);
#ifdef TCLM_CONCEPT
static DEVICE_ATTR(sec_info, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_sec_info_show,
		ist40xx_sec_info_store);
#endif

/* sysfs : tunes */
static DEVICE_ATTR(regcmd, S_IRUGO | S_IWUSR | S_IWGRP, tunes_regcmd_show,
		tunes_regcmd_store);
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR | S_IWGRP, tunes_reg_show,
		tunes_reg_store);
static DEVICE_ATTR(adb, S_IRUGO | S_IWUSR | S_IWGRP, tunes_adb_show,
		tunes_adb_store);
static DEVICE_ATTR(intr_debug, S_IRUGO | S_IWUSR | S_IWGRP, intr_debug_show,
		intr_debug_store);
static DEVICE_ATTR(intr_debug2, S_IRUGO | S_IWUSR | S_IWGRP, intr_debug2_show,
		intr_debug2_store);
static DEVICE_ATTR(intr_debug3, S_IRUGO | S_IWUSR | S_IWGRP, intr_debug3_show,
		intr_debug3_store);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static DEVICE_ATTR(rec_mode, S_IRUGO | S_IWUSR | S_IWGRP, ist40xx_rec_mode_show,
		ist40xx_rec_mode_store);
static DEVICE_ATTR(change_rec_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		change_rec_delay_show, NULL);
static DEVICE_ATTR(rec_delay, S_IRUGO | S_IWUSR | S_IWGRP, rec_delay_show,
		NULL);
static DEVICE_ATTR(recording_status, S_IRUGO | S_IWUSR | S_IWGRP,
		recording_status_show, NULL);
static DEVICE_ATTR(recording_cnt, S_IRUGO | S_IWUSR | S_IWGRP,
		recording_cnt_show, NULL);
static DEVICE_ATTR(recording_frame, S_IRUGO | S_IWUSR | S_IWGRP,
		recording_frame_show, NULL);
#endif
static struct attribute *node_attributes[] = {
	&dev_attr_refresh.attr,
	NULL,
};

static struct attribute *sys_attributes[] = {
	&dev_attr_debug_mode.attr,
	&dev_attr_printk.attr,
	&dev_attr_printk5.attr,
	&dev_attr_printk6.attr,
	&dev_attr_direct.attr,
	&dev_attr_clb.attr,
	&dev_attr_clb_result.attr,
	&dev_attr_miscal.attr,
	&dev_attr_miscal_result.attr,
	&dev_attr_errcnt.attr,
	&dev_attr_scancnt.attr,
	&dev_attr_timerms.attr,
	&dev_attr_report_rate.attr,
	&dev_attr_idle_rate.attr,
	&dev_attr_mode_ta.attr,
	&dev_attr_mode_call.attr,
	&dev_attr_mode_cover.attr,
	&dev_attr_mode_glove.attr,
	&dev_attr_mode_sensitivity.attr,
	&dev_attr_ic_inform.attr,
#ifdef TCLM_CONCEPT
	&dev_attr_sec_info.attr,
#endif
	NULL,
};

static struct attribute *tunes_attributes[] = {
	&dev_attr_regcmd.attr,
	&dev_attr_reg.attr,
	&dev_attr_adb.attr,
	&dev_attr_intr_debug.attr,
	&dev_attr_intr_debug2.attr,
	&dev_attr_intr_debug3.attr,
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	&dev_attr_rec_mode.attr,
	&dev_attr_change_rec_delay.attr,
	&dev_attr_rec_delay.attr,
	&dev_attr_recording_cnt.attr,
	&dev_attr_recording_frame.attr,
	&dev_attr_recording_status.attr,
#endif
	NULL,
};

static struct attribute_group node_attr_group = {
	.attrs = node_attributes,
};

static struct attribute_group sys_attr_group = {
	.attrs = sys_attributes,
};

static struct attribute_group tunes_attr_group = {
	.attrs = tunes_attributes,
};

extern struct class *ist40xx_class;
struct device *ist40xx_sys_dev;
struct device *ist40xx_tunes_dev;
struct device *ist40xx_node_dev;
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static ssize_t cp_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int ret;
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		tsp_info("Read CP of MemX\n");
		mutex_lock(&data->lock);
		ret = ist40xx_read_cp_node(data, &tsp->node, false);
		if (ret) {
			mutex_unlock(&data->lock);
			tsp_err("frame read fail\n");
			return sprintf(buf, "FAIL\n");
		}
		mutex_unlock(&data->lock);

		ist40xx_parse_cp_node(data, &tsp->node, false);

		data->node_buf[0] = '\0';
		data->node_cnt = sprintf(data->node_buf, "[cp of memx]\n");
		data->node_cnt += print_cp_node(data, &tsp->node, data->node_buf,
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

static ssize_t cp_info_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int ret;
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		tsp_info("Read CP of Info\n");
		mutex_lock(&data->lock);
		ret = ist40xx_read_cp_node(data, &tsp->node, true);
		if (ret) {
			mutex_unlock(&data->lock);
			tsp_err("frame read fail\n");
			return sprintf(buf, "FAIL\n");
		}
		mutex_unlock(&data->lock);

		ist40xx_parse_cp_node(data, &tsp->node, true);

		data->node_buf[0] = '\0';
		data->node_cnt = sprintf(data->node_buf, "[cp of info]\n");
		data->node_cnt += print_cp_node(data, &tsp->node, data->node_buf, true);
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

static ssize_t cdc_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		data->node_buf[0] = '\0';
		data->node_cnt = sprintf(data->node_buf, "[cdc]\n");
		data->node_cnt += print_touch_node(data, NODE_FLAG_CDC, &tsp->node,
				data->node_buf);
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

static ssize_t base_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		data->node_buf[0] = '\0';
		data->node_cnt = sprintf(data->node_buf, "[base]\n");
		data->node_cnt += print_touch_node(data, NODE_FLAG_BASE, &tsp->node,
				data->node_buf);
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

static ssize_t diff_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		data->node_buf[0] = '\0';
		data->node_cnt = sprintf(data->node_buf, "[diff]\n");
		data->node_cnt += print_touch_node(data, NODE_FLAG_DIFF, &tsp->node,
				data->node_buf);
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

static ssize_t lofs_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size)
{
	int count;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ist40xx_data *data = dev_get_drvdata(dev);
	TSP_INFO *tsp = &data->tsp_info;

	if (off == 0) {
		data->node_buf[0] = '\0';
		data->node_cnt = sprintf(data->node_buf, "[lofs]\n");
		data->node_cnt += print_touch_node(data, NODE_FLAG_LOFS, &tsp->node,
				data->node_buf);
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

static struct bin_attribute bin_cp_attr = {
	.attr = {
		 .name = "cp",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = cp_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_cp_info_attr = {
	.attr = {
		 .name = "cp_info",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = cp_info_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_cdc_attr = {
	.attr = {
		 .name = "cdc",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = cdc_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_base_attr = {
	.attr = {
		 .name = "base",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = base_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_diff_attr = {
	.attr = {
		 .name = "diff",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = diff_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

static struct bin_attribute bin_lofs_attr = {
	.attr = {
		 .name = "lofs",
		 .mode = S_IRUGO,
		 },
	.size = MAX_BUF_SIZE,
	.read = lofs_sysfs_read,
	.write = NULL,
	.mmap = NULL,
};

void ist40xx_init_bin_attribute(void)
{
	if (sysfs_create_bin_file(&ist40xx_node_dev->kobj, &bin_cp_attr))
		tsp_err("Failed to create sysfs cp bin file(%s)!\n", "node");

	if (sysfs_create_bin_file(&ist40xx_node_dev->kobj, &bin_cp_info_attr))
		tsp_err("Failed to create sysfs cp_info bin file(%s)!\n", "node");

	if (sysfs_create_bin_file(&ist40xx_node_dev->kobj, &bin_cdc_attr))
		tsp_err("Failed to create sysfs cdc bin file(%s)!\n", "node");

	if (sysfs_create_bin_file(&ist40xx_node_dev->kobj, &bin_base_attr))
		tsp_err("Failed to create sysfs base bin file(%s)!\n", "node");

	if (sysfs_create_bin_file(&ist40xx_node_dev->kobj, &bin_diff_attr))
		tsp_err("Failed to create sysfs diff bin file(%s)!\n", "node");

	if (sysfs_create_bin_file(&ist40xx_node_dev->kobj, &bin_lofs_attr))
		tsp_err("Failed to create sysfs lofs bin file(%s)!\n", "node");
}
#endif

int ist40xx_init_misc_sysfs(struct ist40xx_data *data)
{
	/* /sys/class/touch/sys */
	ist40xx_sys_dev = device_create(ist40xx_class, NULL, 0, data, "sys");

	/* /sys/class/touch/sys/... */
	if (sysfs_create_group(&ist40xx_sys_dev->kobj, &sys_attr_group))
		tsp_err("Failed to create sysfs group(%s)!\n", "sys");

	/* /sys/class/touch/tunes */
	ist40xx_tunes_dev = device_create(ist40xx_class, NULL, 0, data, "tunes");

	/* /sys/class/touch/tunes/... */
	if (sysfs_create_group(&ist40xx_tunes_dev->kobj, &tunes_attr_group))
		tsp_err("Failed to create sysfs group(%s)!\n", "tunes");

	/* /sys/class/touch/node */
	ist40xx_node_dev = device_create(ist40xx_class, NULL, 0, data, "node");

	/* /sys/class/touch/node/... */
	if (sysfs_create_group(&ist40xx_node_dev->kobj, &node_attr_group))
		tsp_err("Failed to create sysfs group(%s)!\n", "node");

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	/* /sys/class/touch/node/... */
	ist40xx_init_bin_attribute();
#endif

	return 0;
}
