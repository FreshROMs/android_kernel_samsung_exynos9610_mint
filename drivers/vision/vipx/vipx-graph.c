/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/delay.h>

#include "vipx-log.h"
#include "vipx-core.h"
#include "vipx-queue.h"
#include "vipx-context.h"
#include "vipx-graphmgr.h"
#include "vipx-graph.h"

#define VIPX_GRAPH_MAX_PRIORITY		(20)
#define VIPX_GRAPH_TIMEOUT		(3 * HZ)

static int __vipx_graph_start(struct vipx_graph *graph)
{
	int ret;

	vipx_enter();
	if (test_bit(VIPX_GRAPH_STATE_START, &graph->state))
		return 0;

	ret = vipx_graphmgr_grp_start(graph->owner, graph);
	if (ret)
		goto p_err;

	set_bit(VIPX_GRAPH_STATE_START, &graph->state);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int __vipx_graph_stop(struct vipx_graph *graph)
{
	unsigned int timeout, retry;
	struct vipx_taskmgr *tmgr;
	struct vipx_task *control;

	vipx_enter();
	if (!test_bit(VIPX_GRAPH_STATE_START, &graph->state))
		return 0;

	tmgr = &graph->taskmgr;
	if (tmgr->req_cnt + tmgr->pre_cnt) {
		control = &graph->control;
		control->message = VIPX_CTRL_STOP;
		vipx_graphmgr_queue(graph->owner, control);
		timeout = wait_event_timeout(graph->control_wq,
			control->message == VIPX_CTRL_STOP_DONE,
			VIPX_GRAPH_TIMEOUT);
		if (!timeout)
			vipx_err("wait time(%u ms) is expired (%u)\n",
					jiffies_to_msecs(VIPX_GRAPH_TIMEOUT),
					graph->idx);
	}

	retry = VIPX_STOP_WAIT_COUNT;
	while (tmgr->req_cnt) {
		if (!retry)
			break;

		vipx_warn("Waiting request count(%u) to be zero (%u/%u)\n",
				tmgr->req_cnt, retry, graph->idx);
		udelay(10);
	}

	if (tmgr->req_cnt)
		vipx_err("request count(%u) is remained (%u)\n",
				tmgr->req_cnt, graph->idx);

	retry = VIPX_STOP_WAIT_COUNT;
	while (tmgr->pre_cnt) {
		if (!retry)
			break;

		vipx_warn("Waiting prepare count(%u) to be zero (%u/%u)\n",
				tmgr->pre_cnt, retry, graph->idx);
		udelay(10);
	}

	if (tmgr->pre_cnt)
		vipx_err("prepare count(%u) is remained (%u)\n",
				tmgr->pre_cnt, graph->idx);

	retry = VIPX_STOP_WAIT_COUNT;
	while (tmgr->pro_cnt) {
		if (!retry)
			break;

		vipx_warn("Waiting process count(%u) to be zero (%u/%u)\n",
				tmgr->pro_cnt, retry, graph->idx);
		udelay(10);
	}

	if (tmgr->pro_cnt)
		vipx_err("process count(%u) is remained (%u)\n",
				tmgr->pro_cnt, graph->idx);

	vipx_graphmgr_grp_stop(graph->owner, graph);
	vipx_task_flush(tmgr);
	clear_bit(VIPX_GRAPH_STATE_START, &graph->state);

	vipx_leave();
	return 0;
}

static int vipx_graph_set_graph(struct vipx_graph *graph,
		struct vs4l_graph *ginfo)
{
	int ret;

	vipx_enter();
	if (test_bit(VIPX_GRAPH_STATE_CONFIG, &graph->state)) {
		ret = -EINVAL;
		vipx_err("graph(%u) is already configured (%lu)\n",
				graph->idx, graph->state);
		goto p_err;
	}

	if (ginfo->priority > VIPX_GRAPH_MAX_PRIORITY) {
		vipx_warn("graph(%u) priority is over (%u/%u)\n",
				graph->idx, ginfo->priority,
				VIPX_GRAPH_MAX_PRIORITY);
		ginfo->priority = VIPX_GRAPH_MAX_PRIORITY;
	}

	graph->uid = ginfo->id;
	graph->flags = ginfo->flags;
	graph->priority = ginfo->priority;

	set_bit(VIPX_GRAPH_STATE_CONFIG, &graph->state);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_set_format(struct vipx_graph *graph,
		struct vs4l_format_list *flist)
{
	int ret;
	struct vipx_format_list *in_flist;
	struct vipx_format_list *ot_flist;
	unsigned int cnt;

	vipx_enter();
	in_flist = &graph->inflist;
	ot_flist = &graph->otflist;

	if (flist->direction == VS4L_DIRECTION_IN) {
		if (in_flist->count != flist->count) {
			kfree(in_flist->formats);

			in_flist->count = flist->count;
			in_flist->formats = kcalloc(in_flist->count,
					sizeof(*in_flist->formats), GFP_KERNEL);
			if (!in_flist->formats) {
				ret = -ENOMEM;
				vipx_err("Failed to alloc in_flist formats\n");
				goto p_err;
			}

			for (cnt = 0; cnt < in_flist->count; ++cnt) {
				in_flist->formats[cnt].format =
					flist->formats[cnt].format;
				in_flist->formats[cnt].plane =
					flist->formats[cnt].plane;
				in_flist->formats[cnt].width =
					flist->formats[cnt].width;
				in_flist->formats[cnt].height =
					flist->formats[cnt].height;
			}
		}
	} else if (flist->direction == VS4L_DIRECTION_OT) {
		if (ot_flist->count != flist->count) {
			kfree(ot_flist->formats);

			ot_flist->count = flist->count;
			ot_flist->formats = kcalloc(ot_flist->count,
					sizeof(*ot_flist->formats), GFP_KERNEL);
			if (!ot_flist->formats) {
				ret = -ENOMEM;
				vipx_err("Failed to alloc ot_flist formats\n");
				goto p_err;
			}

			for (cnt = 0; cnt < ot_flist->count; ++cnt) {
				ot_flist->formats[cnt].format =
					flist->formats[cnt].format;
				ot_flist->formats[cnt].plane =
					flist->formats[cnt].plane;
				ot_flist->formats[cnt].width =
					flist->formats[cnt].width;
				ot_flist->formats[cnt].height =
					flist->formats[cnt].height;
			}
		}
	} else {
		ret = -EINVAL;
		vipx_err("invalid direction (%d)\n", flist->direction);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	kfree(in_flist->formats);
	in_flist->formats = NULL;
	in_flist->count = 0;

	kfree(ot_flist->formats);
	ot_flist->formats = NULL;
	ot_flist->count = 0;
	return ret;
}

static int vipx_graph_set_param(struct vipx_graph *graph,
		struct vs4l_param_list *plist)
{
	vipx_enter();
	set_bit(VIPX_GRAPH_FLAG_UPDATE_PARAM, &graph->flags);
	vipx_leave();
	return 0;
}

static int vipx_graph_set_ctrl(struct vipx_graph *graph,
		struct vs4l_ctrl *ctrl)
{
	vipx_enter();
	vipx_graph_print(graph);
	vipx_leave();
	return 0;
}

static int vipx_graph_queue(struct vipx_graph *graph,
		struct vipx_container_list *incl,
		struct vipx_container_list *otcl)
{
	int ret;
	struct vipx_taskmgr *tmgr;
	struct vipx_task *task;
	unsigned long flags;

	vipx_enter();
	tmgr = &graph->taskmgr;

	if (!test_bit(VIPX_GRAPH_STATE_START, &graph->state)) {
		ret = -EINVAL;
		vipx_err("graph(%u) is not started (%lu)\n",
				graph->idx, graph->state);
		goto p_err;
	}

	if (incl->id != otcl->id) {
		vipx_warn("buffer id is incoincidence (%u/%u)\n",
				incl->id, otcl->id);
		otcl->id = incl->id;
	}

	spin_lock_irqsave(&tmgr->slock, flags);
	task = vipx_task_pick_fre_to_req(tmgr);
	spin_unlock_irqrestore(&tmgr->slock, flags);
	if (!task) {
		ret = -ENOMEM;
		vipx_err("free task is not remained (%u)\n", graph->idx);
		vipx_task_print_all(tmgr);
		goto p_err;
	}

	graph->inhash[incl->index] = task->index;
	graph->othash[otcl->index] = task->index;
	graph->input_cnt++;

	task->id = incl->id;
	task->incl = incl;
	task->otcl = otcl;
	task->message = VIPX_TASK_REQUEST;
	task->param0 = 0;
	task->param1 = 0;
	task->param2 = 0;
	task->param3 = 0;

	vipx_graphmgr_queue(graph->owner, task);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_deque(struct vipx_graph *graph,
		struct vipx_container_list *clist)
{
	int ret;
	struct vipx_taskmgr *tmgr;
	unsigned int tidx;
	struct vipx_task *task;
	unsigned long flags;

	vipx_enter();
	tmgr = &graph->taskmgr;

	if (!test_bit(VIPX_GRAPH_STATE_START, &graph->state)) {
		ret = -EINVAL;
		vipx_err("graph(%u) is not started (%lu)\n",
				graph->idx, graph->state);
		goto p_err;
	}

	if (clist->direction == VS4L_DIRECTION_IN)
		tidx = graph->inhash[clist->index];
	else
		tidx = graph->othash[clist->index];

	if (tidx >= VIPX_MAX_TASK) {
		ret = -EINVAL;
		vipx_err("task index(%u) invalid (%u)\n", tidx, graph->idx);
		goto p_err;
	}

	task = &tmgr->task[tidx];
	if (task->state != VIPX_TASK_STATE_COMPLETE) {
		ret = -EINVAL;
		vipx_err("task(%u) state(%d) is invalid (%u)\n",
				tidx, task->state, graph->idx);
		goto p_err;
	}

	if (clist->direction == VS4L_DIRECTION_IN) {
		if (task->incl != clist) {
			ret = -EINVAL;
			vipx_err("incl ptr is invalid (%u)\n", graph->idx);
			goto p_err;
		}

		graph->inhash[clist->index] = VIPX_MAX_TASK;
		task->incl = NULL;
	} else {
		if (task->otcl != clist) {
			ret = -EINVAL;
			vipx_err("otcl ptr is invalid (%u)\n", graph->idx);
			goto p_err;
		}

		graph->othash[clist->index] = VIPX_MAX_TASK;
		task->otcl = NULL;
	}

	if (task->incl || task->otcl)
		return 0;

	spin_lock_irqsave(&tmgr->slock, flags);
	vipx_task_trans_com_to_fre(tmgr, task);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_start(struct vipx_graph *graph)
{
	int ret;

	vipx_enter();
	ret = __vipx_graph_start(graph);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_stop(struct vipx_graph *graph)
{
	vipx_enter();

	__vipx_graph_stop(graph);

	vipx_leave();
	return 0;
}

const struct vipx_queue_gops vipx_queue_gops = {
	.set_graph	= vipx_graph_set_graph,
	.set_format	= vipx_graph_set_format,
	.set_param	= vipx_graph_set_param,
	.set_ctrl	= vipx_graph_set_ctrl,
	.queue		= vipx_graph_queue,
	.deque		= vipx_graph_deque,
	.start		= vipx_graph_start,
	.stop		= vipx_graph_stop
};

static int vipx_graph_control(struct vipx_graph *graph, struct vipx_task *task)
{
	int ret;
	struct vipx_taskmgr *tmgr;

	vipx_enter();
	tmgr = &graph->taskmgr;

	switch (task->message) {
	case VIPX_CTRL_STOP:
		graph->control.message = VIPX_CTRL_STOP_DONE;
		wake_up(&graph->control_wq);
		break;
	default:
		ret = -EINVAL;
		vipx_err("invalid task message(%u) of graph(%u)\n",
				task->message, graph->idx);
		vipx_task_print_all(tmgr);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_request(struct vipx_graph *graph, struct vipx_task *task)
{
	int ret;
	struct vipx_taskmgr *tmgr;
	unsigned long flags;

	vipx_enter();
	tmgr = &graph->taskmgr;
	if (task->state != VIPX_TASK_STATE_REQUEST) {
		ret = -EINVAL;
		vipx_err("task state(%u) is not REQUEST (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	spin_lock_irqsave(&tmgr->slock, flags);
	vipx_task_trans_req_to_pre(tmgr, task);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_process(struct vipx_graph *graph, struct vipx_task *task)
{
	int ret;
	struct vipx_taskmgr *tmgr;
	unsigned long flags;

	vipx_enter();
	tmgr = &graph->taskmgr;
	if (task->state != VIPX_TASK_STATE_PREPARE) {
		ret = -EINVAL;
		vipx_err("task state(%u) is not PREPARE (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	spin_lock_irqsave(&tmgr->slock, flags);
	vipx_task_trans_pre_to_pro(tmgr, task);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_cancel(struct vipx_graph *graph, struct vipx_task *task)
{
	int ret;
	struct vipx_taskmgr *tmgr;
	struct vipx_queue_list *qlist;
	struct vipx_container_list *incl, *otcl;
	unsigned long result;
	unsigned long flags;

	vipx_enter();
	tmgr = &graph->taskmgr;
	qlist = &graph->vctx->queue_list;
	incl = task->incl;
	otcl = task->otcl;

	if (task->state != VIPX_TASK_STATE_PROCESS) {
		ret = -EINVAL;
		vipx_err("task state(%u) is not PROCESS (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	spin_lock_irqsave(&tmgr->slock, flags);
	vipx_task_trans_pro_to_com(tmgr, task);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	graph->recent = task->id;
	graph->done_cnt++;

	result = 0;
	set_bit(VS4L_CL_FLAG_DONE, &result);
	set_bit(VS4L_CL_FLAG_INVALID, &result);

	vipx_queue_done(qlist, incl, otcl, result);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_done(struct vipx_graph *graph, struct vipx_task *task)
{
	int ret;
	struct vipx_taskmgr *tmgr;
	struct vipx_queue_list *qlist;
	struct vipx_container_list *incl, *otcl;
	unsigned long result;
	unsigned long flags;

	vipx_enter();
	tmgr = &graph->taskmgr;
	qlist = &graph->vctx->queue_list;
	incl = task->incl;
	otcl = task->otcl;

	if (task->state != VIPX_TASK_STATE_PROCESS) {
		ret = -EINVAL;
		vipx_err("task state(%u) is not PROCESS (graph:%u)\n",
				task->state, graph->idx);
		goto p_err;
	}

	spin_lock_irqsave(&tmgr->slock, flags);
	vipx_task_trans_pro_to_com(tmgr, task);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	graph->recent = task->id;
	graph->done_cnt++;

	result = 0;
	if (task->param0) {
		set_bit(VS4L_CL_FLAG_DONE, &result);
		set_bit(VS4L_CL_FLAG_INVALID, &result);
	} else {
		set_bit(VS4L_CL_FLAG_DONE, &result);
	}

	vipx_queue_done(qlist, incl, otcl, result);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_update_param(struct vipx_graph *graph,
		struct vipx_task *task)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static const struct vipx_graph_ops vipx_graph_ops = {
	.control	= vipx_graph_control,
	.request	= vipx_graph_request,
	.process	= vipx_graph_process,
	.cancel		= vipx_graph_cancel,
	.done		= vipx_graph_done,
	.update_param	= vipx_graph_update_param
};

static struct vipx_graph_model *vipx_graph_create_model(
		struct vipx_graph *graph, struct vipx_common_graph_info *ginfo)
{
	int ret;
	struct vipx_graph_model *gmodel;

	vipx_enter();
	gmodel = kzalloc(sizeof(*gmodel), GFP_KERNEL);
	if (!gmodel) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc graph model\n");
		goto p_err_alloc;
	}

	list_add_tail(&gmodel->list, &graph->gmodel_list);
	graph->gmodel_count++;

	gmodel->id = ginfo->gid;
	INIT_LIST_HEAD(&gmodel->kbin_list);

	memcpy(&gmodel->common_ginfo, ginfo, sizeof(gmodel->common_ginfo));

	vipx_leave();
	return gmodel;
p_err_alloc:
	return ERR_PTR(ret);
}

static struct vipx_graph_model *vipx_graph_get_model(struct vipx_graph *graph,
		unsigned int id)
{
	unsigned int model_id = GET_COMMON_GRAPH_MODEL_ID(id);
	struct vipx_graph_model *gmodel, *temp;

	vipx_enter();
	list_for_each_entry_safe(gmodel, temp, &graph->gmodel_list, list) {
		unsigned int gmodel_id = GET_COMMON_GRAPH_MODEL_ID(gmodel->id);

		if (gmodel_id == model_id) {
			vipx_leave();
			return gmodel;
		}
	}

	vipx_err("Failed to get gmodel (%d/%u)\n", id, graph->gmodel_count);
	return ERR_PTR(-EINVAL);
}

static int vipx_graph_destroy_model(struct vipx_graph *graph,
		struct vipx_graph_model *gmodel)
{
	vipx_enter();
	graph->gmodel_count--;
	list_del(&gmodel->list);
	kfree(gmodel);
	vipx_leave();
	return 0;
}

static int vipx_graph_register_model(struct vipx_graph *graph,
		struct vipx_graph_model *gmodel)
{
	int ret;

	vipx_enter();
	ret = vipx_graphmgr_register_model(graph->owner, gmodel);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_unregister_model(struct vipx_graph *graph,
		struct vipx_graph_model *gmodel)
{
	int ret;

	vipx_enter();
	ret = vipx_graphmgr_unregister_model(graph->owner, gmodel);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_graph_start_model(struct vipx_graph *graph,
		struct vipx_graph_model *gmodel)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static int vipx_graph_stop_model(struct vipx_graph *graph,
		struct vipx_graph_model *gmodel)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static int vipx_graph_execute_model(struct vipx_graph *graph,
		struct vipx_graph_model *gmodel,
		struct vipx_common_execute_info *einfo)
{
	int ret;

	vipx_enter();
	ret = vipx_graphmgr_execute_model(graph->owner, gmodel, einfo);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_graph_cleanup_buffer(struct vipx_graph *graph,
		struct vipx_buffer *buf)
{
	struct vipx_memory *mem;

	vipx_enter();
	if (!buf)
		return;

	mem = &graph->vctx->core->system->memory;
	if (buf->mem_attr == VIPX_COMMON_CACHEABLE)
		mem->mops->sync_for_cpu(mem, buf);

	mem->mops->unmap_dmabuf(mem, buf);
	kfree(buf);
	vipx_leave();
}

static void __vipx_graph_cleanup_model(struct vipx_graph *graph)
{
	struct vipx_context *vctx;
	struct vipx_graph_model *gmodel, *temp;

	vipx_enter();

	if (!graph->gmodel_count) {
		vipx_leave();
		return;
	}

	vctx = graph->vctx;
	list_for_each_entry_safe(gmodel, temp, &graph->gmodel_list, list) {
		vctx->graph_ops->unregister_model(graph, gmodel);
		__vipx_graph_cleanup_buffer(graph, gmodel->user_param_buffer);
		__vipx_graph_cleanup_buffer(graph, gmodel->bias);
		__vipx_graph_cleanup_buffer(graph, gmodel->weight);
		__vipx_graph_cleanup_buffer(graph, gmodel->temp_buf);
		__vipx_graph_cleanup_buffer(graph, gmodel->graph);
		vctx->graph_ops->destroy_model(graph, gmodel);
	}

	vipx_leave();
}

const struct vipx_context_gops vipx_context_gops = {
	.create_model		= vipx_graph_create_model,
	.get_model		= vipx_graph_get_model,
	.destroy_model		= vipx_graph_destroy_model,
	.register_model		= vipx_graph_register_model,
	.unregister_model	= vipx_graph_unregister_model,
	.start_model		= vipx_graph_start_model,
	.stop_model		= vipx_graph_stop_model,
	.execute_model		= vipx_graph_execute_model
};

void vipx_graph_print(struct vipx_graph *graph)
{
	vipx_enter();
	vipx_leave();
}

struct vipx_graph *vipx_graph_create(struct vipx_context *vctx,
		void *graphmgr)
{
	int ret;
	struct vipx_graph *graph;
	struct vipx_taskmgr *tmgr;
	unsigned int idx;

	vipx_enter();
	graph = kzalloc(sizeof(*graph), GFP_KERNEL);
	if (!graph) {
		ret = -ENOMEM;
		vipx_err("Failed to allocate graph\n");
		goto p_err_kzalloc;
	}

	ret = vipx_graphmgr_grp_register(graphmgr, graph);
	if (ret)
		goto p_err_grp_register;

	graph->owner = graphmgr;
	graph->gops = &vipx_graph_ops;
	mutex_init(&graph->local_lock);
	graph->control.owner = graph;
	graph->control.message = VIPX_CTRL_NONE;
	init_waitqueue_head(&graph->control_wq);

	tmgr = &graph->taskmgr;
	spin_lock_init(&tmgr->slock);
	ret = vipx_task_init(tmgr, graph->idx, graph);
	if (ret)
		goto p_err_task_init;

	for (idx = 0; idx < VIPX_MAX_TASK; ++idx) {
		graph->inhash[idx] = VIPX_MAX_TASK;
		graph->othash[idx] = VIPX_MAX_TASK;
	}

	INIT_LIST_HEAD(&graph->gmodel_list);
	graph->gmodel_count = 0;

	vctx->graph_ops = &vipx_context_gops;
	graph->vctx = vctx;

	vipx_leave();
	return graph;
p_err_task_init:
p_err_grp_register:
	kfree(graph);
p_err_kzalloc:
	return ERR_PTR(ret);
}

int vipx_graph_destroy(struct vipx_graph *graph)
{
	vipx_enter();
	__vipx_graph_stop(graph);
	__vipx_graph_cleanup_model(graph);
	vipx_graphmgr_grp_unregister(graph->owner, graph);

	kfree(graph->inflist.formats);
	graph->inflist.formats = NULL;
	graph->inflist.count = 0;

	kfree(graph->otflist.formats);
	graph->otflist.formats = NULL;
	graph->otflist.count = 0;

	kfree(graph);
	vipx_leave();
	return 0;
}
