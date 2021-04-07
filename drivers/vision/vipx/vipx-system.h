/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_SYSTEM_H__
#define __VIPX_SYSTEM_H__

#include "platform/vipx-clk.h"
#include "platform/vipx-ctrl.h"
#include "vipx-interface.h"
#include "vipx-pm.h"
#include "vipx-memory.h"
#include "vipx-binary.h"
#include "vipx-graphmgr.h"

struct vipx_device;

struct vipx_system {
	struct device			*dev;
	void __iomem			*reg_ss[REG_MAX];
	resource_size_t			reg_ss_size[REG_MAX];
	void __iomem			*itcm;
	resource_size_t			itcm_size;
	void __iomem			*dtcm;
	resource_size_t			dtcm_size;

	const struct vipx_clk_ops	*clk_ops;
	const struct vipx_ctrl_ops	*ctrl_ops;
	struct pinctrl                  *pinctrl;
	struct vipx_pm			pm;
	struct vipx_memory		memory;
	struct vipx_interface		interface;
	struct vipx_binary		binary;
	struct vipx_graphmgr		graphmgr;

	struct vipx_device		*device;
};

int vipx_system_fw_bootup(struct vipx_system *sys);

int vipx_system_resume(struct vipx_system *sys);
int vipx_system_suspend(struct vipx_system *sys);
int vipx_system_runtime_resume(struct vipx_system *sys);
int vipx_system_runtime_suspend(struct vipx_system *sys);

int vipx_system_start(struct vipx_system *sys);
int vipx_system_stop(struct vipx_system *sys);
int vipx_system_open(struct vipx_system *sys);
int vipx_system_close(struct vipx_system *sys);

int vipx_system_probe(struct vipx_device *device);
void vipx_system_remove(struct vipx_system *sys);

#endif
