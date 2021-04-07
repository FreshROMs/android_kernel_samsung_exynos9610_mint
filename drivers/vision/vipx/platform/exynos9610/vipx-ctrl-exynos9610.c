/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>

#include "vipx-log.h"
#include "vipx-system.h"
#include "platform/vipx-ctrl.h"

enum vipx_reg_ss1_id {
	REG_SS1_VERSION_ID,
	REG_SS1_QCHANNEL,
	REG_SS1_IRQ_FROM_DEVICE,
	REG_SS1_IRQ0_TO_DEVICE,
	REG_SS1_IRQ1_TO_DEVICE,
	REG_SS1_GLOBAL_CTRL,
	REG_SS1_CORTEX_CONTROL,
	REG_SS1_CPU_CTRL,
	REG_SS1_PROGRAM_COUNTER,
	REG_SS1_DEBUG0,
	REG_SS1_DEBUG1,
	REG_SS1_DEBUG2,
	REG_SS1_DEBUG3,
	REG_SS1_MAX
};

enum vipx_reg_ss2_id {
	REG_SS2_QCHANNEL,
	REG_SS2_GLOBAL_CTRL,
	REG_SS2_MAX
};

static const struct vipx_reg regs_ss1[] = {
	{0x0000, "SS1_VERSION_ID"},
	{0x0004, "SS1_QCHANNEL"},
	{0x0008, "SS1_IRQ_FROM_DEVICE"},
	{0x000C, "SS1_IRQ0_TO_DEVICE"},
	{0x0010, "SS1_IRQ1_TO_DEVICE"},
	{0x0014, "SS1_GLOBAL_CTRL"},
	{0x0018, "SS1_CORTEX_CONTROL"},
	{0x001C, "SS1_CPU_CTRL"},
	{0x0044, "SS1_PROGRAM_COUNTER"},
	{0x0048, "SS1_DEBUG0"},
	{0x004C, "SS1_DEBUG1"},
	{0x0050, "SS1_DEBUG2"},
	{0x0054, "SS1_DEBUG3"},
};

static const struct vipx_reg regs_ss2[] = {
	{0x0000, "SS2_QCHANNEL"},
	{0x0004, "SS2_GLOBAL_CTRL"},
};

static int vipx_exynos9610_ctrl_reset(struct vipx_system *sys)
{
	void __iomem *ss1, *ss2;
	unsigned int val;

	vipx_enter();
	ss1 = sys->reg_ss[REG_SS1];
	ss2 = sys->reg_ss[REG_SS2];

	/* TODO: check delay */
	val = readl(ss1 + regs_ss1[REG_SS1_GLOBAL_CTRL].offset);
	writel(val | 0xF1, ss1 + regs_ss1[REG_SS1_GLOBAL_CTRL].offset);
	udelay(10);

	val = readl(ss2 + regs_ss2[REG_SS2_GLOBAL_CTRL].offset);
	writel(val | 0xF1, ss2 + regs_ss2[REG_SS2_GLOBAL_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	udelay(10);

	writel(0x0, ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_QCHANNEL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_QCHANNEL].offset);
	udelay(10);

	val = readl(ss2 + regs_ss2[REG_SS2_QCHANNEL].offset);
	writel(val | 0x1, ss2 + regs_ss2[REG_SS2_QCHANNEL].offset);
	udelay(10);

	vipx_leave();
	return 0;
}

static int vipx_exynos9610_ctrl_start(struct vipx_system *sys)
{
	vipx_enter();
	writel(0x0, sys->reg_ss[REG_SS1] +
			regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	vipx_leave();
	return 0;
}

static int vipx_exynos9610_ctrl_get_irq(struct vipx_system *sys, int direction)
{
	unsigned int offset;
	int val;

	vipx_enter();
	if (direction == IRQ0_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ0_TO_DEVICE].offset;
		val = readl(sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ1_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ1_TO_DEVICE].offset;
		val = readl(sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ_FROM_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ_FROM_DEVICE].offset;
		val = readl(sys->reg_ss[REG_SS1] + offset);
	} else {
		val = -EINVAL;
		vipx_err("Failed to get irq due to invalid direction (%d)\n",
				direction);
		goto p_err;
	}

	vipx_leave();
	return val;
p_err:
	return val;
}

static int vipx_exynos9610_ctrl_set_irq(struct vipx_system *sys, int direction,
		int val)
{
	int ret;
	unsigned int offset;

	vipx_enter();
	if (direction == IRQ0_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ0_TO_DEVICE].offset;
		writel(val, sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ1_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ1_TO_DEVICE].offset;
		writel(val, sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ_FROM_DEVICE) {
		ret = -EINVAL;
		vipx_err("Host can't set irq from device (%d)\n", direction);
		goto p_err;
	} else {
		ret = -EINVAL;
		vipx_err("Failed to set irq due to invalid direction (%d)\n",
				direction);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_exynos9610_ctrl_clear_irq(struct vipx_system *sys,
		int direction, int val)
{
	int ret;
	unsigned int offset;

	vipx_enter();
	if (direction == IRQ0_TO_DEVICE || direction == IRQ1_TO_DEVICE) {
		ret = -EINVAL;
		vipx_err("Irq to device must be cleared at device (%d)\n",
				direction);
		goto p_err;
	} else if (direction == IRQ_FROM_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ_FROM_DEVICE].offset;
		writel(val, sys->reg_ss[REG_SS1] + offset);
	} else {
		ret = -EINVAL;
		vipx_err("direction of irq is invalid (%d)\n", direction);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_exynos9610_ctrl_debug_dump(struct vipx_system *sys)
{
	void __iomem *ss1, *ss2;
	int idx;
	unsigned int val;
	const char *name;

	vipx_enter();
	ss1 = sys->reg_ss[REG_SS1];
	ss2 = sys->reg_ss[REG_SS2];

	vipx_info("SS1 SFR Dump (count:%d)\n", REG_SS1_MAX);
	for (idx = 0; idx < REG_SS1_MAX; ++idx) {
		val = readl(ss1 + regs_ss1[idx].offset);
		name = regs_ss1[idx].name;
		vipx_info("[%2d][%20s] 0x%08x\n", idx, name, val);
	}

	vipx_info("SS2 SFR Dump (count:%d)\n", REG_SS2_MAX);
	for (idx = 0; idx < REG_SS2_MAX; ++idx) {
		val = readl(ss2 + regs_ss2[idx].offset);
		name = regs_ss2[idx].name;
		vipx_info("[%2d][%20s] 0x%08x\n", idx, name, val);
	}

	vipx_leave();
	return 0;
}

const struct vipx_ctrl_ops vipx_ctrl_ops = {
	.reset		= vipx_exynos9610_ctrl_reset,
	.start		= vipx_exynos9610_ctrl_start,
	.get_irq	= vipx_exynos9610_ctrl_get_irq,
	.set_irq	= vipx_exynos9610_ctrl_set_irq,
	.clear_irq	= vipx_exynos9610_ctrl_clear_irq,
	.debug_dump	= vipx_exynos9610_ctrl_debug_dump,
};
