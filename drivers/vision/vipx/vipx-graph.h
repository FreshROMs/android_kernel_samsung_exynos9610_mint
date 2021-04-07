/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_GRAPH_H__
#define __VIPX_GRAPH_H__

#include <linux/mutex.h>
#include <linux/wait.h>

#include "vipx-config.h"
#include "vs4l.h"
#include "vipx-taskmgr.h"
#include "vipx-common-type.h"

struct vips_context;
struct vipx_graph;

enum vipx_graph_state {
	VIPX_GRAPH_STATE_CONFIG,
	VIPX_GRAPH_STATE_HENROLL,
	VIPX_GRAPH_STATE_HMAPPED,
	VIPX_GRAPH_STATE_MMAPPED,
	VIPX_GRAPH_STATE_START,
};

enum vipx_graph_flag {
	VIPX_GRAPH_FLAG_UPDATE_PARAM = VS4L_GRAPH_FLAG_END
};

struct vipx_format {
	unsigned int			format;
	unsigned int			plane;
	unsigned int			width;
	unsigned int			height;
};

struct vipx_format_list {
	unsigned int			count;
	struct vipx_format		*formats;
};

struct vipx_graph_ops {
	int (*control)(struct vipx_graph *graph, struct vipx_task *task);
	int (*request)(struct vipx_graph *graph, struct vipx_task *task);
	int (*process)(struct vipx_graph *graph, struct vipx_task *task);
	int (*cancel)(struct vipx_graph *graph, struct vipx_task *task);
	int (*done)(struct vipx_graph *graph, struct vipx_task *task);
	int (*update_param)(struct vipx_graph *graph, struct vipx_task *task);
};

struct vipx_graph_model {
	unsigned int			id;
	struct list_head		kbin_list;
	unsigned int			kbin_count;
	struct vipx_common_graph_info	common_ginfo;
	struct vipx_buffer		*graph;
	struct vipx_buffer		*temp_buf;
	struct vipx_buffer		*weight;
	struct vipx_buffer		*bias;
	struct vipx_buffer		*user_param_buffer;

	struct list_head		list;
	struct vipx_time		time[TIME_COUNT];
};

struct vipx_graph {
	unsigned int			idx;
	unsigned long			state;
	struct mutex			*global_lock;

	void				*owner;
	const struct vipx_graph_ops	*gops;
	struct mutex			local_lock;
	struct vipx_task		control;
	wait_queue_head_t		control_wq;
	struct vipx_taskmgr		taskmgr;

	unsigned int			uid;
	unsigned long			flags;
	unsigned int			priority;

	struct vipx_format_list		inflist;
	struct vipx_format_list		otflist;

	unsigned int			inhash[VIPX_MAX_TASK];
	unsigned int			othash[VIPX_MAX_TASK];

	struct list_head		gmodel_list;
	unsigned int			gmodel_count;

	/* for debugging */
	unsigned int			input_cnt;
	unsigned int			cancel_cnt;
	unsigned int			done_cnt;
	unsigned int			recent;

	struct vipx_context		*vctx;
};

extern const struct vipx_queue_gops vipx_queue_gops;
extern const struct vipx_vctx_gops vipx_vctx_gops;

void vipx_graph_print(struct vipx_graph *graph);

struct vipx_graph *vipx_graph_create(struct vipx_context *vctx,
		void *graphmgr);
int vipx_graph_destroy(struct vipx_graph *graph);

#endif
