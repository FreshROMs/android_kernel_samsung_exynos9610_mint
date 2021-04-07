/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_QUEUE_H__
#define __VIPX_QUEUE_H__

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/mutex.h>

#include "vipx-config.h"
#include "vs4l.h"
#include "vipx-memory.h"
#include "vipx-graph.h"

struct vipx_context;
struct vipx_queue_list;

struct vipx_format_type {
	char				*name;
	unsigned int			colorspace;
	unsigned int			planes;
	unsigned int			bitsperpixel[VIPX_MAX_PLANES];
};

struct vipx_buffer_format {
	unsigned int			target;
	struct vipx_format_type		*fmt;
	unsigned int			colorspace;
	unsigned int			plane;
	unsigned int			width;
	unsigned int			height;
	unsigned int			size[VIPX_MAX_PLANES];
};

struct vipx_buffer_format_list {
	unsigned int			count;
	struct vipx_buffer_format	*formats;
};

struct vipx_container {
	unsigned int			type;
	unsigned int			target;
	unsigned int			memory;
	unsigned int			reserved[4];
	unsigned int			count;
	struct vipx_buffer		*buffers;
	struct vipx_buffer_format	*format;
};

struct vipx_container_list {
	unsigned int			direction;
	unsigned int			id;
	unsigned int			index;
	unsigned long			flags;
	struct timeval			timestamp[6];
	unsigned int			count;
	unsigned int			user_params[MAX_NUM_OF_USER_PARAMS];
	struct vipx_container		*containers;
};

enum vipx_bundle_state {
	VIPX_BUNDLE_STATE_DEQUEUED,
	VIPX_BUNDLE_STATE_QUEUED,
	VIPX_BUNDLE_STATE_PROCESS,
	VIPX_BUNDLE_STATE_DONE,
};

struct vipx_bundle {
	unsigned long			flags;
	unsigned int			state;
	struct list_head		queued_entry;
	struct list_head		process_entry;
	struct list_head		done_entry;

	struct vipx_container_list	clist;
};

struct vipx_queue {
	unsigned int			direction;
	unsigned int			streaming;
	struct mutex			*lock;

	struct list_head		queued_list;
	atomic_t			queued_count;
	struct list_head		process_list;
	atomic_t			process_count;
	struct list_head		done_list;
	atomic_t			done_count;

	spinlock_t			done_lock;
	wait_queue_head_t		done_wq;

	struct vipx_buffer_format_list	flist;
	struct vipx_bundle		*bufs[VIPX_MAX_BUFFER];
	unsigned int			num_buffers;

	struct vipx_queue_list		*qlist;
};

struct vipx_queue_gops {
	int (*set_graph)(struct vipx_graph *graph, struct vs4l_graph *ginfo);
	int (*set_format)(struct vipx_graph *graph,
			struct vs4l_format_list *flist);
	int (*set_param)(struct vipx_graph *graph,
			struct vs4l_param_list *plist);
	int (*set_ctrl)(struct vipx_graph *graph, struct vs4l_ctrl *ctrl);
	int (*queue)(struct vipx_graph *graph,
			struct vipx_container_list *incl,
			struct vipx_container_list *otcl);
	int (*deque)(struct vipx_graph *graph,
			struct vipx_container_list *clist);
	int (*start)(struct vipx_graph *graph);
	int (*stop)(struct vipx_graph *graph);
};

struct vipx_queue_list {
	struct vipx_queue		in_queue;
	struct vipx_queue		out_queue;
	const struct vipx_queue_gops	*gops;
	struct vipx_context		*vctx;
};

void vipx_queue_done(struct vipx_queue_list *qlist,
		struct vipx_container_list *inclist,
		struct vipx_container_list *otclist,
		unsigned long flags);

int vipx_queue_init(struct vipx_context *vctx);

#endif
