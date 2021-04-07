/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_DEBUG_H__
#define __VIPX_DEBUG_H__

#include <linux/timer.h>

#include "vipx-config.h"
#include "vipx-system.h"

struct vipx_device;

enum vipx_debug_state {
	VIPX_DEBUG_STATE_START,
};

struct vipx_debug_log_area {
	int				front;
	int				rear;
	int				line_size;
	int				queue_size;
	int				reserved[4];
	char				queue[0];
};

struct vipx_debug_log {
	struct timer_list		timer;
	struct vipx_debug_log_area	*area;
};

struct vipx_debug {
	unsigned long			state;
	struct vipx_system		*system;
	struct vipx_debug_log		target_log;

	int				log_bin_enable;

	struct dentry			*root;
	struct dentry			*mem;
	struct dentry			*log;
	struct dentry			*log_bin;
	struct dentry			*dvfs;
	struct dentry			*clk;
	struct dentry			*wait_time;
};

extern int vipx_debug_log_enable;

int vipx_debug_write_log_binary(void);
int vipx_debug_dump_debug_regs(void);
void vipx_debug_log_flush(struct vipx_debug *debug);

int vipx_debug_start(struct vipx_debug *debug);
int vipx_debug_stop(struct vipx_debug *debug);
int vipx_debug_open(struct vipx_debug *debug);
int vipx_debug_close(struct vipx_debug *debug);

int vipx_debug_probe(struct vipx_device *device);
void vipx_debug_remove(struct vipx_debug *debug);

#endif
