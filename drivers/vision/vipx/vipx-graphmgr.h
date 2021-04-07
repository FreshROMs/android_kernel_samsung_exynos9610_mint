/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_GRAPHMGR_H___
#define __VIPX_GRAPHMGR_H___

#include <linux/kthread.h>
#include <linux/types.h>

#include "vipx-config.h"
#include "vipx-taskmgr.h"
#include "vipx-graph.h"
#include "vipx-interface.h"

struct vipx_system;

enum vipx_taskdesc_state {
	VIPX_TASKDESC_STATE_FREE,
	VIPX_TASKDESC_STATE_READY,
	VIPX_TASKDESC_STATE_REQUEST,
	VIPX_TASKDESC_STATE_ALLOC,
	VIPX_TASKDESC_STATE_PROCESS,
	VIPX_TASKDESC_STATE_COMPLETE
};

struct vipx_taskdesc {
	struct list_head	list;
	unsigned int		index;
	unsigned int		priority;
	struct vipx_graph	*graph;
	struct vipx_task	*task;
	unsigned int		state;
};

enum vipx_graphmgr_state {
	VIPX_GRAPHMGR_OPEN,
	VIPX_GRAPHMGR_ENUM
};

enum vipx_graphmgr_client {
	VIPX_GRAPHMGR_CLIENT_GRAPH = 1,
	VIPX_GRAPHMGR_CLIENT_INTERFACE
};

struct vipx_graphmgr {
	struct vipx_graph	*graph[VIPX_MAX_GRAPH];
	atomic_t		active_cnt;
	unsigned long		state;
	struct mutex		mlock;

	unsigned int		tick_cnt;
	unsigned int		tick_pos;
	unsigned int		sched_cnt;
	unsigned int		sched_pos;
	struct kthread_worker	worker;
	struct task_struct	*graph_task;

	struct vipx_interface	*interface;

	struct mutex		tdlock;
	struct vipx_taskdesc	taskdesc[VIPX_MAX_TASKDESC];
	struct list_head	tdfre_list;
	struct list_head	tdrdy_list;
	struct list_head	tdreq_list;
	struct list_head	tdalc_list;
	struct list_head	tdpro_list;
	struct list_head	tdcom_list;
	unsigned int		tdfre_cnt;
	unsigned int		tdrdy_cnt;
	unsigned int		tdreq_cnt;
	unsigned int		tdalc_cnt;
	unsigned int		tdpro_cnt;
	unsigned int		tdcom_cnt;

	struct vipx_graph_model	*current_model;
};

void vipx_taskdesc_print(struct vipx_graphmgr *gmgr);
int vipx_graphmgr_grp_register(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph);
int vipx_graphmgr_grp_unregister(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph);
int vipx_graphmgr_grp_start(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph);
int vipx_graphmgr_grp_stop(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph);
void vipx_graphmgr_queue(struct vipx_graphmgr *gmgr, struct vipx_task *task);

int vipx_graphmgr_register_model(struct vipx_graphmgr *gmgr,
		struct vipx_graph_model *gmodel);
int vipx_graphmgr_unregister_model(struct vipx_graphmgr *gmgr,
		struct vipx_graph_model *gmodel);
int vipx_graphmgr_execute_model(struct vipx_graphmgr *gmgr,
		struct vipx_graph_model *gmodel,
		struct vipx_common_execute_info *einfo);

int vipx_graphmgr_open(struct vipx_graphmgr *gmgr);
int vipx_graphmgr_close(struct vipx_graphmgr *gmgr);

int vipx_graphmgr_probe(struct vipx_system *sys);
void vipx_graphmgr_remove(struct vipx_graphmgr *gmgr);

#endif
