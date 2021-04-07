/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_CONTEXT_H__
#define __VIPX_CONTEXT_H__

#include <linux/mutex.h>

#include "vipx-queue.h"
#include "vipx-graph.h"
#include "vipx-ioctl.h"
#include "vipx-core.h"

enum vipx_context_state {
	VIPX_CONTEXT_OPEN,
	VIPX_CONTEXT_GRAPH,
	VIPX_CONTEXT_FORMAT,
	VIPX_CONTEXT_START,
	VIPX_CONTEXT_STOP
};

struct vipx_context;

struct vipx_context_ops {
	int (*load_kernel_binary)(struct vipx_context *vctx,
			struct vipx_ioc_load_kernel_binary *kernel_bin);
	int (*unload_kernel_binary)(struct vipx_context *vctx,
			struct vipx_ioc_unload_kernel_binary *unload_kbin);
	int (*load_graph_info)(struct vipx_context *vctx,
			struct vipx_ioc_load_graph_info *ginfo);
	int (*unload_graph_info)(struct vipx_context *vctx,
			struct vipx_ioc_unload_graph_info *ginfo);
	int (*execute_submodel)(struct vipx_context *vctx,
			struct vipx_ioc_execute_submodel *execute);
};

struct vipx_context_qops {
	int (*poll)(struct vipx_queue_list *qlist,
			struct file *file, struct poll_table_struct *poll);
	int (*set_graph)(struct vipx_queue_list *qlist,
			struct vs4l_graph *ginfo);
	int (*set_format)(struct vipx_queue_list *qlist,
			struct vs4l_format_list *flist);
	int (*set_param)(struct vipx_queue_list *qlist,
			struct vs4l_param_list *plist);
	int (*set_ctrl)(struct vipx_queue_list *qlist,
			struct vs4l_ctrl *ctrl);
	int (*qbuf)(struct vipx_queue_list *qlist,
			struct vs4l_container_list *clist);
	int (*dqbuf)(struct vipx_queue_list *qlist,
			struct vs4l_container_list *clist);
	int (*streamon)(struct vipx_queue_list *qlist);
	int (*streamoff)(struct vipx_queue_list *qlist);
};

struct vipx_context_gops {
	struct vipx_graph_model *(*create_model)(struct vipx_graph *graph,
			struct vipx_common_graph_info *ginfo);
	struct vipx_graph_model *(*get_model)(struct vipx_graph *graph,
			unsigned int id);
	int (*destroy_model)(struct vipx_graph *graph,
			struct vipx_graph_model *gmodel);
	int (*register_model)(struct vipx_graph *graph,
			struct vipx_graph_model *gmodel);
	int (*unregister_model)(struct vipx_graph *graph,
			struct vipx_graph_model *gmodel);
	int (*start_model)(struct vipx_graph *graph,
			struct vipx_graph_model *gmodel);
	int (*stop_model)(struct vipx_graph *graph,
			struct vipx_graph_model *gmodel);
	int (*execute_model)(struct vipx_graph *graph,
			struct vipx_graph_model *gmodel,
			struct vipx_common_execute_info *einfo);
};

struct vipx_context {
	unsigned int			state;
	unsigned int			idx;
	struct list_head		list;
	struct mutex			lock;

	const struct vipx_context_ops	*vops;
	int				binary_count;
	struct list_head		binary_list;
	spinlock_t			binary_slock;
	const struct vipx_context_gops	*graph_ops;

	const struct vipx_context_qops	*queue_ops;
	struct vipx_queue_list		queue_list;

	struct vipx_core		*core;
	struct vipx_graph		*graph;
};

struct vipx_context *vipx_context_create(struct vipx_core *core);
void vipx_context_destroy(struct vipx_context *vctx);

#endif
