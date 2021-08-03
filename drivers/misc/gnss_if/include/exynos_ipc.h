/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __EXYNOS_IPC_H__
#define __EXYNOS_IPC_H__

#include <linux/types.h>
#include "gnss.h"

#define EXYNOS_SINGLE_MASK			(0b11000000)
#define EXYNOS_MULTI_START_MASK	(0b10000000)
#define EXYNOS_MULTI_LAST_MASK		(0b01000000)

#define EXYNOS_START_MASK			0xABCD
#define EXYNOS_START_OFFSET		0
#define EXYNOS_START_SIZE			2

#define EXYNOS_FRAME_SEQ_OFFSET	2
#define EXYNOS_FRAME_SIZE			2

#define EXYNOS_FRAG_CONFIG_OFFSET	4
#define EXYNOS_FRAG_CONFIG_SIZE	2

#define EXYNOS_LEN_OFFSET			6
#define EXYNOS_LEN_SIZE			2

#define EXYNOS_CH_ID_OFFSET		8
#define EXYNOS_CH_SIZE				1

#define EXYNOS_CH_SEQ_OFFSET		9
#define EXYNOS_CH_SEQ_SIZE			1

#define EXYNOS_HEADER_SIZE		12

#define EXYNOS_DATA_LOOPBACK_CHANNEL	82

#define EXYNOS_FMT_NUM		1
#define EXYNOS_RFS_NUM		10

struct __packed frag_config {
	u8 frame_first:1,
	frame_last:1,
	packet_index:6;
	u8 frame_index;
};

/* EXYNOS link-layer header */
struct __packed exynos_link_header {
	u16 seq;
	struct frag_config cfg;
	u16 len;
	u16 reserved_1;
	u8 ch_id;
	u8 ch_seq;
	u16 reserved_2;
};

struct __packed exynos_seq_num {
	u16 frame_cnt;
	u8 ch_cnt[255];
};

struct exynos_frame_data {
	/* Frame length calculated from the length fields */
	unsigned int len;

	/* The length of link layer header */
	unsigned int hdr_len;

	/* The length of received header */
	unsigned int hdr_rcvd;

	/* The length of link layer payload */
	unsigned int pay_len;

	/* The length of received data */
	unsigned int pay_rcvd;

	/* The length of link layer padding */
	unsigned int pad_len;

	/* The length of received padding */
	unsigned int pad_rcvd;

	/* Header buffer */
	u8 hdr[EXYNOS_HEADER_SIZE];
};

static inline bool exynos_start_valid(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_START_OFFSET);

	return cfg == EXYNOS_START_MASK ? true : false;
}

static inline bool exynos_multi_start_valid(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_FRAG_CONFIG_OFFSET);
	return ((cfg >> 8) & EXYNOS_MULTI_START_MASK) == EXYNOS_MULTI_START_MASK;
}

static inline bool exynos_multi_last_valid(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_FRAG_CONFIG_OFFSET);
	return ((cfg >> 8) & EXYNOS_MULTI_LAST_MASK) == EXYNOS_MULTI_LAST_MASK;
}

static inline bool exynos_single_frame(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_FRAG_CONFIG_OFFSET);
	return ((cfg >> 8) & EXYNOS_SINGLE_MASK) == EXYNOS_SINGLE_MASK;
}

static inline u8 exynos_get_ch(u8 *frm)
{
	return frm[EXYNOS_CH_ID_OFFSET];
}

static inline unsigned int exynos_calc_padding_size(unsigned int len)
{
	unsigned int residue = len & 0x3;
	return residue ? (4 - residue) : 0;
}

static inline unsigned int exynos_get_frame_len(u8 *frm)
{
	return (unsigned int)*(u16 *)(frm + EXYNOS_LEN_OFFSET);
}

static inline unsigned int exynos_get_total_len(u8 *frm)
{
	unsigned int len;
	unsigned int pad;

	len = exynos_get_frame_len(frm);
	pad = exynos_calc_padding_size(len) ? exynos_calc_padding_size(len) : 0;
	return len + pad;
}

static inline bool exynos_padding_exist(u8 *frm)
{
	return exynos_calc_padding_size(exynos_get_frame_len(frm)) ? true : false;
}
#endif
