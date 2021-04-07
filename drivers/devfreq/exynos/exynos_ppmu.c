/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PPMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/io.h>

#include "exynos_ppmu.h"

#define PPMU_PMNC                               0x4
#define PPMU_CNTENS                             0x8
#define PPMU_CNT_AUTO                           0x30
#define PPMU_PMCNT0_HIGH			0x004C
#define PPMU_PMCNT1_HIGH			0x0050
#define PPMU_PMCNT2_HIGH			0x0054
#define PPMU_PMCNT3_HIGH			0x0044
#define PPMU_CCNT_HIGH				0x0058
#define PPMU_PMCNT0_LOW				0x0034
#define PPMU_PMCNT1_LOW				0x0038
#define PPMU_PMCNT2_LOW				0x003C
#define PPMU_PMCNT3_LOW				0x0040
#define PPMU_CCNT_LOW				0x0048
#define PPMU_CH_EV0_TYPE                        0x200
#define PPMU_CH_EV1_TYPE                        0x204
#define PPMU_CH_EV2_TYPE                        0x208
#define PPMU_CH_EV3_TYPE                        0x20c
#define PPMU_SM_ID_V                            0x220
#define PPMU_SM_ID_A                            0x224

#define EVENT_RD_ACTIVATED			0x0
#define EVENT_WR_ACTIVATED			0x1
#define EVENT_RD_DATA				0x4
#define EVENT3_RD_DATA				0x4
#define EVENT3_WR_DATA				0x5
#define EVENT3_RW_DATA				0x22


/* 0x1: disable Q channel */
/* auto mode */
#define BIT_REGVALUE		((0x1<<24) | (0x1<<20))
#define BIT_CH_CCNT		(0x1<<31)
#define BIT_CH_PMCNT0		(0x1<<0)
#define BIT_CH_PMCNT1		(0x1<<1)
#define BIT_CH_PMCNT2		(0x1<<2)
#define BIT_CH_PMCNT3		(0x1<<3)
#define BIT_CH_ALL		(BIT_CH_CCNT | BIT_CH_PMCNT0 | \
				 BIT_CH_PMCNT1 | BIT_CH_PMCNT2 | \
				 BIT_CH_PMCNT3)

void exynos_read_ppmu(struct ppmu_data *ppmu, void __iomem *ppmu_base,
		      u32 channel)
{
	if (!channel)
		channel = BIT_CH_ALL;
	if (channel & BIT_CH_CCNT)
		ppmu->ccnt = __raw_readl(ppmu_base + PPMU_CCNT_LOW);
	if (channel & BIT_CH_PMCNT0)
		ppmu->pmcnt0 = __raw_readl(ppmu_base + PPMU_PMCNT0_LOW);
	if (channel & BIT_CH_PMCNT1)
		ppmu->pmcnt1 = __raw_readl(ppmu_base + PPMU_PMCNT1_LOW);
	if (channel & BIT_CH_PMCNT2)
		ppmu->pmcnt2 = __raw_readl(ppmu_base + PPMU_PMCNT2_LOW);
	if (channel & BIT_CH_PMCNT3)
		ppmu->pmcnt3 = __raw_readl(ppmu_base + PPMU_PMCNT3_LOW);
}

void exynos_init_ppmu(void __iomem *ppmu_base, u32 mask_v, u32 mask_a)
{

	__raw_writel(BIT_REGVALUE | 0x6, ppmu_base + PPMU_PMNC);
	/* Count Enable CCNT, PMCNTTx */
	__raw_writel(BIT_CH_ALL, ppmu_base + PPMU_CNTENS);
	__raw_writel(EVENT_RD_ACTIVATED, ppmu_base + PPMU_CH_EV0_TYPE);
	__raw_writel(EVENT_WR_ACTIVATED, ppmu_base + PPMU_CH_EV1_TYPE);
	__raw_writel(EVENT_RD_DATA, ppmu_base + PPMU_CH_EV2_TYPE);
	__raw_writel(EVENT3_WR_DATA, ppmu_base + PPMU_CH_EV3_TYPE);
	if (mask_v) {
		__raw_writel(mask_v, ppmu_base + PPMU_SM_ID_V);
		__raw_writel(mask_a, ppmu_base + PPMU_SM_ID_A);
	}
}

void exynos_reset_ppmu(void __iomem *ppmu_base, u32 channel)
{
	if (!channel)
		channel = BIT_CH_ALL;
	__raw_writel(channel, ppmu_base + PPMU_CNT_AUTO);
}

void exynos_start_ppmu(void __iomem *ppmu_base)
{
	__raw_writel(BIT_REGVALUE | 0x1, ppmu_base + PPMU_PMNC);
}

void exynos_stop_ppmu(void __iomem *ppmu_base)
{
	__raw_writel(BIT_REGVALUE, ppmu_base + PPMU_PMNC);
}

void exynos_exit_ppmu(void __iomem *ppmu_base)
{
	__raw_writel(BIT_REGVALUE, ppmu_base + PPMU_PMNC);
}
