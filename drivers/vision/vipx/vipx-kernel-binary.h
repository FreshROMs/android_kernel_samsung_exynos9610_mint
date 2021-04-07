/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_KERNEL_BINARY_H__
#define __VIPX_KERNEL_BINARY_H__

#include "vipx-context.h"
#include "vipx-memory.h"
#include "vipx-graph.h"

struct vipx_kernel_binary {
	unsigned int		global_id;
	struct vipx_buffer	buffer;
	struct list_head	clist;
	struct list_head	glist;

	struct vipx_context	*vctx;
};

int vipx_kernel_binary_set_gmodel(struct vipx_context *vctx,
		struct vipx_graph_model *gmodel);
int vipx_kernel_binary_add(struct vipx_context *vctx, unsigned int id,
		int fd, unsigned int size);
int vipx_kernel_binary_unload(struct vipx_context *vctx, unsigned int id,
		int fd, unsigned int size);
void vipx_kernel_binary_remove(struct vipx_kernel_binary *kbin);
void vipx_kernel_binary_all_remove(struct vipx_context *vctx);

#endif
