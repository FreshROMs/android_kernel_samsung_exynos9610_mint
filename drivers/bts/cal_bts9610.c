/* Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - BTS CAL code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cal_bts9610.h"
#include <linux/soc/samsung/exynos-soc.h>

#define LOG(x, ...)					\
({							\
	seq_printf(buf, x, ##__VA_ARGS__);		\
})

#define TREX_CON				0x000
#define TREX_TIMEOUT				0x010
#define TREX_RCON				0x020
#define TREX_WCON				0x040
#define TREX_RBLOCK_UPPER			0x024
#define TREX_WBLOCK_UPPER			0x044
#define TREX_RBLOCK_UPPER_NORMAL		0x028
#define TREX_WBLOCK_UPPER_NORMAL		0x048
#define TREX_RBLOCK_UPPER_FULL			0x02C
#define TREX_WBLOCK_UPPER_FULL			0x04C
#define TREX_RBLOCK_UPPER_BUSY			0x030
#define TREX_WBLOCK_UPPER_BUSY			0x050
#define TREX_RBLOCK_UPPER_MAX			0x034
#define TREX_WBLOCK_UPPER_MAX			0x054

#define QMAX_THRESHOLD_R			0x050
#define QMAX_THRESHOLD_W			0x054

static unsigned int set_mo(unsigned int mo)
{
	if (mo > BTS_MAX_MO || !mo)
		mo = BTS_MAX_MO;
	return mo;
}

static unsigned int set_threshold(unsigned int threshold)
{
	if (threshold > BTS_QMAX_MAX_THRESHOLD || !threshold)
		threshold = BTS_QMAX_MAX_THRESHOLD;
	return threshold;
}

void bts_setqos(void __iomem *base, struct bts_status *stat)
{
	unsigned int tmp_reg = 0;
	bool block_en = false;

	if (!base)
		return;

	if (stat->disable) {
		__raw_writel(0x4000, base + TREX_RCON);
		__raw_writel(0x4000, base + TREX_WCON);
		__raw_writel(0x0, base + TREX_CON);
		return;
	}
	__raw_writel(set_mo(stat->rmo), base + TREX_RBLOCK_UPPER);
	__raw_writel(set_mo(stat->wmo), base + TREX_WBLOCK_UPPER);
	if (stat->max_rmo || stat->max_wmo || stat->full_rmo || stat->full_wmo)
		block_en = true;

	__raw_writel(set_mo(stat->max_rmo), base + TREX_RBLOCK_UPPER_MAX);
	__raw_writel(set_mo(stat->max_wmo), base + TREX_WBLOCK_UPPER_MAX);
	__raw_writel(set_mo(stat->full_rmo), base + TREX_RBLOCK_UPPER_FULL);
	__raw_writel(set_mo(stat->full_wmo), base + TREX_WBLOCK_UPPER_FULL);

	if (stat->timeout_en) {
		if (stat->timeout_r > 0xff)
			stat->timeout_r = 0xff;
		if (stat->timeout_w > 0xff)
			stat->timeout_w = 0xff;
		__raw_writel(stat->timeout_r | (stat->timeout_w << 16),
				base + TREX_TIMEOUT);
	} else {
		__raw_writel(0xff | (0xff << 16), base + TREX_TIMEOUT);
	}

	/* override QoS value */
	tmp_reg |= (1 & !stat->bypass_en) << 8;
	tmp_reg |= (stat->priority & 0xf) << 12;

	/* enable Blocking logic */
	tmp_reg |= (1 & block_en) << 0;
	__raw_writel(tmp_reg, base + TREX_RCON);
	__raw_writel(tmp_reg, base + TREX_WCON);

	__raw_writel(((1 & stat->timeout_en) << 20) | 0x1, base + TREX_CON);
}

void bts_showqos(void __iomem *base, struct seq_file *buf)
{
	if (!base)
		return;

	LOG("CON0x%08X qos(%d,%d)0x%Xr%Xw, wmo: %d, rmo: %d\n",
			 __raw_readl(base + TREX_CON),
			(__raw_readl(base + TREX_RCON) >> 8) & 0x1,
			(__raw_readl(base + TREX_WCON) >> 8) & 0x1,
			(__raw_readl(base + TREX_RCON) >> 12) & 0xf,
			(__raw_readl(base + TREX_WCON) >> 12) & 0xf,
			(__raw_readl(base + TREX_WBLOCK_UPPER)),
			(__raw_readl(base + TREX_RBLOCK_UPPER))
	   );
}

void bts_set_qmax(void __iomem *base, unsigned int r_thsd0,
			unsigned int r_thsd1, unsigned int w_thsd0,
			unsigned int w_thsd1)
{
	unsigned int tmp_reg = 0;

	if (!base)
		return;

	tmp_reg |= set_threshold(r_thsd0);
	tmp_reg |= set_threshold(r_thsd1) << 16;
	__raw_writel(tmp_reg, base + QMAX_THRESHOLD_R);

	tmp_reg = 0;
	tmp_reg |= set_threshold(w_thsd0);
	tmp_reg |= set_threshold(w_thsd1) << 16;
	__raw_writel(tmp_reg, base + QMAX_THRESHOLD_W);
}

void bts_show_qmax(void __iomem *base, struct seq_file *buf)
{
	if (!base)
		return;

	LOG("threshold_r(0x%04x,0x%04x), threshold_w(0x%04x,0x%04x)\n",
		__raw_readl(base + QMAX_THRESHOLD_R) & 0xffff,
		(__raw_readl(base + QMAX_THRESHOLD_R) >> 16) & 0xffff,
		__raw_readl(base + QMAX_THRESHOLD_W) & 0xffff,
		(__raw_readl(base + QMAX_THRESHOLD_W) >> 16) & 0xffff);
}
