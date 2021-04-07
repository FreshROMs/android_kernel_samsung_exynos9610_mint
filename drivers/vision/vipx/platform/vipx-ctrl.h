/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_CTRL_H__
#define __VIPX_CTRL_H__

struct vipx_system;

enum {
	REG_SS1,
	REG_SS2,
	REG_MAX
};

struct vipx_reg {
	const unsigned int	offset;
	const char		*name;
};

struct vipx_ctrl_ops {
	int (*reset)(struct vipx_system *sys);
	int (*start)(struct vipx_system *sys);
	int (*get_irq)(struct vipx_system *sys, int direction);
	int (*set_irq)(struct vipx_system *sys, int direction, int val);
	int (*clear_irq)(struct vipx_system *sys, int direction, int val);
	int (*debug_dump)(struct vipx_system *sys);
};

extern const struct vipx_ctrl_ops vipx_ctrl_ops;

#endif
