/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_CLK_H__
#define __VIPX_CLK_H__

#include <linux/clk.h>

struct vipx_system;

struct vipx_clk {
	struct clk	*clk;
	const char	*name;
};

struct vipx_clk_ops {
	int (*init)(struct vipx_system *sys);
	void (*deinit)(struct vipx_system *sys);
	int (*on)(struct vipx_system *sys);
	int (*off)(struct vipx_system *sys);
	int (*dump)(struct vipx_system *sys);
	int (*get_count)(struct vipx_system *sys);
	unsigned long (*get_freq)(struct vipx_system *sys, int id);
	const char *(*get_name)(struct vipx_system *sys, int id);
};

extern const struct vipx_clk_ops vipx_clk_ops;

#endif
