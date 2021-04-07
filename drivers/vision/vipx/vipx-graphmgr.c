/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched/types.h>

#include "vipx-log.h"
#include "vipx-common-type.h"
#include "vipx-device.h"
#include "vipx-graphmgr.h"

static void __vipx_taskdesc_set_free(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	taskdesc->state = VIPX_TASKDESC_STATE_FREE;
	list_add_tail(&taskdesc->list, &gmgr->tdfre_list);
	gmgr->tdfre_cnt++;
	vipx_leave();
}

static struct vipx_taskdesc *__vipx_taskdesc_get_first_free(
		struct vipx_graphmgr *gmgr)
{
	struct vipx_taskdesc *taskdesc = NULL;

	vipx_enter();
	if (gmgr->tdfre_cnt)
		taskdesc = list_first_entry(&gmgr->tdfre_list,
				struct vipx_taskdesc, list);
	vipx_leave();
	return taskdesc;
}

static void __vipx_taskdesc_set_ready(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	taskdesc->state = VIPX_TASKDESC_STATE_READY;
	list_add_tail(&taskdesc->list, &gmgr->tdrdy_list);
	gmgr->tdrdy_cnt++;
	vipx_leave();
}

static struct vipx_taskdesc *__vipx_taskdesc_pop_ready(
		struct vipx_graphmgr *gmgr)
{
	struct vipx_taskdesc *taskdesc = NULL;

	vipx_enter();
	if (gmgr->tdrdy_cnt) {
		taskdesc = container_of(gmgr->tdrdy_list.next,
				struct vipx_taskdesc, list);

		list_del(&taskdesc->list);
		gmgr->tdrdy_cnt--;
	}
	vipx_leave();
	return taskdesc;
}

static void __vipx_taskdesc_set_request(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	taskdesc->state = VIPX_TASKDESC_STATE_REQUEST;
	list_add_tail(&taskdesc->list, &gmgr->tdreq_list);
	gmgr->tdreq_cnt++;
	vipx_leave();
}

static void __vipx_taskdesc_set_request_sched(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc, struct vipx_taskdesc *next)
{
	vipx_enter();
	taskdesc->state = VIPX_TASKDESC_STATE_REQUEST;
	list_add_tail(&taskdesc->list, &next->list);
	gmgr->tdreq_cnt++;
	vipx_leave();
}

static void __vipx_taskdesc_set_process(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	taskdesc->state = VIPX_TASKDESC_STATE_PROCESS;
	list_add_tail(&taskdesc->list, &gmgr->tdpro_list);
	gmgr->tdpro_cnt++;
	vipx_leave();
}

static void __vipx_taskdesc_set_complete(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	taskdesc->state = VIPX_TASKDESC_STATE_COMPLETE;
	list_add_tail(&taskdesc->list, &gmgr->tdcom_list);
	gmgr->tdcom_cnt++;
	vipx_leave();
}

static void __vipx_taskdesc_trans_fre_to_rdy(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdfre_cnt) {
		vipx_warn("tdfre_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_FREE) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdfre_cnt--;
	__vipx_taskdesc_set_ready(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_rdy_to_fre(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdrdy_cnt) {
		vipx_warn("tdrdy_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_READY) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdrdy_cnt--;
	__vipx_taskdesc_set_free(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_req_to_pro(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdreq_cnt) {
		vipx_warn("tdreq_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_REQUEST) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdreq_cnt--;
	__vipx_taskdesc_set_process(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_req_to_fre(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdreq_cnt) {
		vipx_warn("tdreq_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_REQUEST) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdreq_cnt--;
	__vipx_taskdesc_set_free(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_alc_to_pro(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdalc_cnt) {
		vipx_warn("tdalc_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_ALLOC) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdalc_cnt--;
	__vipx_taskdesc_set_process(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_alc_to_fre(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdalc_cnt) {
		vipx_warn("tdalc_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_ALLOC) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdalc_cnt--;
	__vipx_taskdesc_set_free(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_pro_to_com(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdpro_cnt) {
		vipx_warn("tdpro_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_PROCESS) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdpro_cnt--;
	__vipx_taskdesc_set_complete(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_pro_to_fre(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdpro_cnt) {
		vipx_warn("tdpro_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_PROCESS) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdpro_cnt--;
	__vipx_taskdesc_set_free(gmgr, taskdesc);
	vipx_leave();
}

static void __vipx_taskdesc_trans_com_to_fre(struct vipx_graphmgr *gmgr,
		struct vipx_taskdesc *taskdesc)
{
	vipx_enter();
	if (!gmgr->tdcom_cnt) {
		vipx_warn("tdcom_cnt is zero\n");
		return;
	}

	if (taskdesc->state != VIPX_TASKDESC_STATE_COMPLETE) {
		vipx_warn("state(%x) is invlid\n", taskdesc->state);
		return;
	}

	list_del(&taskdesc->list);
	gmgr->tdcom_cnt--;
	__vipx_taskdesc_set_free(gmgr, taskdesc);
	vipx_leave();
}

static int __vipx_graphmgr_itf_init(struct vipx_interface *itf,
		struct vipx_graph *graph)
{
	return 0;
}

static int __vipx_graphmgr_itf_deinit(struct vipx_interface *itf,
		struct vipx_graph *graph)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static int __vipx_graphmgr_itf_create(struct vipx_interface *itf,
		struct vipx_graph *graph)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static int __vipx_graphmgr_itf_destroy(struct vipx_interface *itf,
		struct vipx_graph *graph)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static int __vipx_graphmgr_itf_config(struct vipx_interface *itf,
		struct vipx_graph *graph)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static int __vipx_graphmgr_itf_process(struct vipx_interface *itf,
		struct vipx_graph *graph, struct vipx_task *task)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static void __vipx_graphmgr_sched(struct vipx_graphmgr *gmgr)
{
	int ret;
	struct vipx_taskdesc *ready_td, *list_td, *temp;
	struct vipx_graph *graph;
	struct vipx_task *task;

	vipx_enter();

	mutex_lock(&gmgr->tdlock);

	while (1) {
		ready_td = __vipx_taskdesc_pop_ready(gmgr);
		if (!ready_td)
			break;

		if (!gmgr->tdreq_cnt) {
			__vipx_taskdesc_set_request(gmgr, ready_td);
			continue;
		}

		list_for_each_entry_safe(list_td, temp, &gmgr->tdreq_list,
				list) {
			if (ready_td->priority > list_td->priority) {
				__vipx_taskdesc_set_request_sched(gmgr,
						ready_td, list_td);
				break;
			}
		}

		if (ready_td->state == VIPX_TASKDESC_STATE_READY)
			__vipx_taskdesc_set_request(gmgr, ready_td);
	}

	list_for_each_entry_safe(list_td, temp, &gmgr->tdreq_list, list) {
		__vipx_taskdesc_trans_req_to_pro(gmgr, list_td);
	}

	mutex_unlock(&gmgr->tdlock);

	list_for_each_entry_safe(list_td, temp, &gmgr->tdpro_list, list) {
		graph = list_td->graph;
		task = list_td->task;

		ret = __vipx_graphmgr_itf_process(gmgr->interface, graph, task);
		if (ret) {
			ret = graph->gops->cancel(graph, task);
			if (ret)
				return;

			list_td->graph = NULL;
			list_td->task = NULL;
			__vipx_taskdesc_trans_pro_to_fre(gmgr, list_td);
			continue;
		}

		__vipx_taskdesc_trans_pro_to_com(gmgr, list_td);
	}

	gmgr->sched_cnt++;
	vipx_leave();
}

static void vipx_graph_thread(struct kthread_work *work)
{
	int ret;
	struct vipx_task *task;
	struct vipx_graph *graph;
	struct vipx_graphmgr *gmgr;
	struct vipx_taskdesc *taskdesc, *temp;

	vipx_enter();
	task = container_of(work, struct vipx_task, work);
	graph = task->owner;
	gmgr = graph->owner;

	switch (task->message) {
	case VIPX_TASK_REQUEST:
		ret = graph->gops->request(graph, task);
		if (ret)
			return;

		taskdesc = __vipx_taskdesc_get_first_free(gmgr);
		if (!taskdesc) {
			vipx_err("taskdesc is NULL\n");
			return;
		}

		task->tdindex = taskdesc->index;
		taskdesc->graph = graph;
		taskdesc->task = task;
		taskdesc->priority = graph->priority;
		__vipx_taskdesc_trans_fre_to_rdy(gmgr, taskdesc);
		break;
	case VIPX_CTRL_STOP:
		list_for_each_entry_safe(taskdesc, temp,
				&gmgr->tdrdy_list, list) {
			if (taskdesc->graph->idx != graph->idx)
				continue;

			ret = graph->gops->cancel(graph, taskdesc->task);
			if (ret)
				return;

			taskdesc->graph = NULL;
			taskdesc->task = NULL;
			__vipx_taskdesc_trans_rdy_to_fre(gmgr, taskdesc);
		}

		mutex_lock(&gmgr->tdlock);

		list_for_each_entry_safe(taskdesc, temp,
				&gmgr->tdreq_list, list) {
			if (taskdesc->graph->idx != graph->idx)
				continue;

			ret = graph->gops->cancel(graph, taskdesc->task);
			if (ret)
				return;

			taskdesc->graph = NULL;
			taskdesc->task = NULL;
			__vipx_taskdesc_trans_req_to_fre(gmgr, taskdesc);
		}

		mutex_unlock(&gmgr->tdlock);

		ret = graph->gops->control(graph, task);
		if (ret)
			return;
		/* TODO check return/break */
		return;
	default:
		vipx_err("message of task is invalid (%d)\n", task->message);
		return;
	}

	__vipx_graphmgr_sched(gmgr);
	vipx_leave();
}

static void vipx_interface_thread(struct kthread_work *work)
{
	int ret;
	struct vipx_task *task, *itask;
	struct vipx_interface *itf;
	struct vipx_graphmgr *gmgr;
	unsigned int taskdesc_index;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_graph *graph;
	struct vipx_taskdesc *taskdesc;
	unsigned long flags;

	vipx_enter();
	itask = container_of(work, struct vipx_task, work);
	itf = itask->owner;
	itaskmgr = &itf->taskmgr;
	gmgr = itf->cookie;

	if (itask->findex >= VIPX_MAX_TASK) {
		vipx_err("task index(%u) is invalid (%u)\n",
				itask->findex, VIPX_MAX_TASK);
		return;
	}

	switch (itask->message) {
	case VIPX_TASK_ALLOCATE:
		taskdesc_index = itask->tdindex;
		if (taskdesc_index >= VIPX_MAX_TASKDESC) {
			vipx_err("taskdesc index(%u) is invalid (%u)\n",
					taskdesc_index, VIPX_MAX_TASKDESC);
			return;
		}

		taskdesc = &gmgr->taskdesc[taskdesc_index];
		if (taskdesc->state != VIPX_TASKDESC_STATE_ALLOC) {
			vipx_err("taskdesc state(%u) is not ALLOC (%u)\n",
					taskdesc->state, taskdesc_index);
			return;
		}

		graph = taskdesc->graph;
		if (!graph) {
			vipx_err("graph is NULL (%u)\n", taskdesc_index);
			return;
		}

		task = taskdesc->task;
		if (!task) {
			vipx_err("task is NULL (%u)\n", taskdesc_index);
			return;
		}

		task->message = itask->message;
		task->param0 = itask->param2;
		task->param1 = itask->param3;

		/* return status check */
		if (task->param0) {
			vipx_err("task allocation failed (%lu, %lu)\n",
					task->param0, task->param1);

			ret = graph->gops->cancel(graph, task);
			if (ret)
				return;

			/* taskdesc cleanup */
			mutex_lock(&gmgr->tdlock);
			taskdesc->graph = NULL;
			taskdesc->task = NULL;
			__vipx_taskdesc_trans_alc_to_fre(gmgr, taskdesc);
			mutex_unlock(&gmgr->tdlock);
		} else {
			/* taskdesc transition */
			mutex_lock(&gmgr->tdlock);
			__vipx_taskdesc_trans_alc_to_pro(gmgr, taskdesc);
			mutex_unlock(&gmgr->tdlock);
		}

		/* itask cleanup */
		spin_lock_irqsave(&itaskmgr->slock, flags);
		vipx_task_trans_com_to_fre(itaskmgr, itask);
		spin_unlock_irqrestore(&itaskmgr->slock, flags);
		break;
	case VIPX_TASK_PROCESS:
		taskdesc_index = itask->tdindex;
		if (taskdesc_index >= VIPX_MAX_TASKDESC) {
			vipx_err("taskdesc index(%u) is invalid (%u)\n",
					taskdesc_index, VIPX_MAX_TASKDESC);
			return;
		}

		taskdesc = &gmgr->taskdesc[taskdesc_index];
		if (taskdesc->state == VIPX_TASKDESC_STATE_FREE) {
			vipx_err("taskdesc(%u) state is FREE\n",
					taskdesc_index);

			/* itask cleanup */
			spin_lock_irqsave(&itaskmgr->slock, flags);
			vipx_task_trans_com_to_fre(itaskmgr, itask);
			spin_unlock_irqrestore(&itaskmgr->slock, flags);
			break;
		}

		if (taskdesc->state != VIPX_TASKDESC_STATE_COMPLETE) {
			vipx_err("taskdesc state is not COMPLETE (%u)\n",
					taskdesc->state);
			return;
		}

		graph = taskdesc->graph;
		if (!graph) {
			vipx_err("graph is NULL (%u)\n", taskdesc_index);
			return;
		}

		task = taskdesc->task;
		if (!task) {
			vipx_err("task is NULL (%u)\n", taskdesc_index);
			return;
		}

		task->message = itask->message;
		task->tdindex = VIPX_MAX_TASKDESC;
		task->param0 = itask->param2;
		task->param1 = itask->param3;

		ret = graph->gops->done(graph, task);
		if (ret)
			return;

		/* taskdesc cleanup */
		taskdesc->graph = NULL;
		taskdesc->task = NULL;
		__vipx_taskdesc_trans_com_to_fre(gmgr, taskdesc);

		/* itask cleanup */
		spin_lock_irqsave(&itaskmgr->slock, flags);
		vipx_task_trans_com_to_fre(itaskmgr, itask);
		spin_unlock_irqrestore(&itaskmgr->slock, flags);
		break;
	default:
		vipx_err("message of task is invalid (%d)\n", itask->message);
		return;
	}

	__vipx_graphmgr_sched(gmgr);
	vipx_leave();
}

void vipx_taskdesc_print(struct vipx_graphmgr *gmgr)
{
	vipx_enter();
	vipx_leave();
}

int vipx_graphmgr_grp_register(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph)
{
	int ret;
	unsigned int index;

	vipx_enter();
	mutex_lock(&gmgr->mlock);
	for (index = 0; index < VIPX_MAX_GRAPH; ++index) {
		if (!gmgr->graph[index]) {
			gmgr->graph[index] = graph;
			graph->idx = index;
			break;
		}
	}
	mutex_unlock(&gmgr->mlock);

	if (index >= VIPX_MAX_GRAPH) {
		ret = -EINVAL;
		vipx_err("graph slot is lack\n");
		goto p_err;
	}

	kthread_init_work(&graph->control.work, vipx_graph_thread);
	for (index = 0; index < VIPX_MAX_TASK; ++index)
		kthread_init_work(&graph->taskmgr.task[index].work,
				vipx_graph_thread);

	graph->global_lock = &gmgr->mlock;
	atomic_inc(&gmgr->active_cnt);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_graphmgr_grp_unregister(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph)
{
	vipx_enter();
	mutex_lock(&gmgr->mlock);
	gmgr->graph[graph->idx] = NULL;
	mutex_unlock(&gmgr->mlock);
	atomic_dec(&gmgr->active_cnt);
	vipx_leave();
	return 0;
}

int vipx_graphmgr_grp_start(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph)
{
	int ret;

	vipx_enter();
	mutex_lock(&gmgr->mlock);
	if (!test_bit(VIPX_GRAPHMGR_ENUM, &gmgr->state)) {
		ret = __vipx_graphmgr_itf_init(gmgr->interface, graph);
		if (ret) {
			mutex_unlock(&gmgr->mlock);
			goto p_err_init;
		}
		set_bit(VIPX_GRAPHMGR_ENUM, &gmgr->state);
	}
	mutex_unlock(&gmgr->mlock);

	ret = __vipx_graphmgr_itf_create(gmgr->interface, graph);
	if (ret)
		goto p_err_create;

	ret = __vipx_graphmgr_itf_config(gmgr->interface, graph);
	if (ret)
		goto p_err_config;

	vipx_leave();
	return 0;
p_err_config:
p_err_create:
p_err_init:
	return ret;
}

int vipx_graphmgr_grp_stop(struct vipx_graphmgr *gmgr,
		struct vipx_graph *graph)
{
	int ret;

	vipx_enter();
	ret = __vipx_graphmgr_itf_destroy(gmgr->interface, graph);
	if (ret)
		goto p_err_destroy;

	mutex_lock(&gmgr->mlock);
	if (test_bit(VIPX_GRAPHMGR_ENUM, &gmgr->state) &&
			atomic_read(&gmgr->active_cnt) == 1) {
		ret = __vipx_graphmgr_itf_deinit(gmgr->interface, graph);
		if (ret) {
			mutex_unlock(&gmgr->mlock);
			goto p_err_deinit;
		}
		clear_bit(VIPX_GRAPHMGR_ENUM, &gmgr->state);
	}
	mutex_unlock(&gmgr->mlock);

	vipx_leave();
	return 0;
p_err_deinit:
p_err_destroy:
	return ret;
}

void vipx_graphmgr_queue(struct vipx_graphmgr *gmgr, struct vipx_task *task)
{
	vipx_enter();
	kthread_queue_work(&gmgr->worker, &task->work);
	vipx_leave();
}

static bool __vipx_graphmgr_check_model_id(struct vipx_graphmgr *gmgr,
		struct vipx_graph_model *gmodel)
{
	unsigned int current_id, id;
	bool same;

	vipx_enter();
	current_id = GET_COMMON_GRAPH_MODEL_ID(gmgr->current_model->id);
	id = GET_COMMON_GRAPH_MODEL_ID(gmodel->id);

	if (current_id == id)
		same = true;
	else
		same = false;

	vipx_leave();
	return same;
}

static int __vipx_graphmgr_itf_load_graph(struct vipx_interface *itf,
		struct vipx_graph_model *gmodel)
{
	int ret;
	unsigned long flags;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_task *itask;

	vipx_enter();
	itaskmgr = &itf->taskmgr;

	spin_lock_irqsave(&itaskmgr->slock, flags);
	itask = vipx_task_pick_fre_to_req(itaskmgr);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);
	if (!itask) {
		ret = -ENOMEM;
		vipx_err("itask is NULL\n");
		goto p_err_pick;
	}

	itask->id = GET_COMMON_GRAPH_MODEL_ID(gmodel->id);
	itask->message = VIPX_TASK_PROCESS;
	itask->param0 = (unsigned long)&gmodel->common_ginfo;

	ret = vipx_hw_load_graph(itf, itask);

	gmodel->time[TIME_LOAD_GRAPH] = itask->time;

	spin_lock_irqsave(&itaskmgr->slock, flags);
	vipx_task_trans_com_to_fre(itaskmgr, itask);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);

	if (ret)
		goto p_err_hw_load_graph;

	vipx_leave();
	return 0;
p_err_hw_load_graph:
p_err_pick:
	return ret;
}

int vipx_graphmgr_register_model(struct vipx_graphmgr *gmgr,
		struct vipx_graph_model *gmodel)
{
	int ret;

	vipx_enter();
	mutex_lock(&gmgr->mlock);
	if (gmgr->current_model) {
		if (!__vipx_graphmgr_check_model_id(gmgr, gmodel)) {
			ret = -EBUSY;
			vipx_err("model is already registered(%#x/%#x)\n",
					gmgr->current_model->id, gmodel->id);
			mutex_unlock(&gmgr->mlock);
			goto p_err;
		}
	}

	gmgr->current_model = gmodel;
	mutex_unlock(&gmgr->mlock);

	ret = __vipx_graphmgr_itf_load_graph(gmgr->interface, gmodel);
	if (ret)
		goto p_err_register;

	vipx_leave();
	return 0;
p_err_register:
	mutex_lock(&gmgr->mlock);
	gmgr->current_model = NULL;
	mutex_unlock(&gmgr->mlock);
p_err:
	return ret;
}

static int __vipx_graphmgr_itf_unload_graph(struct vipx_interface *itf,
		struct vipx_graph_model *gmodel)
{
	int ret;
	unsigned long flags;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_task *itask;

	vipx_enter();
	itaskmgr = &itf->taskmgr;

	spin_lock_irqsave(&itaskmgr->slock, flags);
	itask = vipx_task_pick_fre_to_req(itaskmgr);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);
	if (!itask) {
		ret = -ENOMEM;
		vipx_err("itask is NULL\n");
		goto p_err_pick;
	}

	itask->id = GET_COMMON_GRAPH_MODEL_ID(gmodel->id);
	itask->message = VIPX_TASK_PROCESS;
	itask->param0 = (unsigned long)&gmodel->common_ginfo;

	ret = vipx_hw_unload_graph(itf, itask);

	gmodel->time[TIME_UNLOAD_GRAPH] = itask->time;

	spin_lock_irqsave(&itaskmgr->slock, flags);
	vipx_task_trans_com_to_fre(itaskmgr, itask);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);

	if (ret)
		goto p_err_hw_unload_graph;

	vipx_leave();
	return 0;
p_err_hw_unload_graph:
p_err_pick:
	return ret;
}

int vipx_graphmgr_unregister_model(struct vipx_graphmgr *gmgr,
		struct vipx_graph_model *gmodel)
{
	int ret;
	struct vipx_graph_model *current_gmodel;

	vipx_enter();
	mutex_lock(&gmgr->mlock);
	if (!gmgr->current_model) {
		ret = -EFAULT;
		vipx_err("model of manager is empty (%#x)\n", gmodel->id);
		mutex_unlock(&gmgr->mlock);
		goto p_err;
	}

	if (!__vipx_graphmgr_check_model_id(gmgr, gmodel)) {
		ret = -EINVAL;
		vipx_err("model of manager is different (%#x/%#x)\n",
				gmgr->current_model->id, gmodel->id);
		mutex_unlock(&gmgr->mlock);
		goto p_err;
	}

	current_gmodel = gmgr->current_model;
	gmgr->current_model = NULL;
	mutex_unlock(&gmgr->mlock);

	ret = __vipx_graphmgr_itf_unload_graph(gmgr->interface, current_gmodel);
	if (ret)
		goto p_err_unregister;

	vipx_leave();
	return 0;
p_err_unregister:
p_err:
	return ret;
}

static int __vipx_graphmgr_itf_execute_graph(struct vipx_interface *itf,
		struct vipx_graph_model *gmodel,
		struct vipx_common_execute_info *einfo)
{
	int ret;
	unsigned long flags;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_task *itask;

	vipx_enter();
	itaskmgr = &itf->taskmgr;

	spin_lock_irqsave(&itaskmgr->slock, flags);
	itask = vipx_task_pick_fre_to_req(itaskmgr);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);
	if (!itask) {
		ret = -ENOMEM;
		vipx_err("itask is NULL\n");
		goto p_err_pick;
	}

	itask->id = GET_COMMON_GRAPH_MODEL_ID(gmodel->id);
	itask->message = VIPX_TASK_PROCESS;
	itask->param0 = (unsigned long)einfo;
	itask->param1 = (unsigned long)gmodel;

	ret = vipx_hw_execute_graph(itf, itask);

	gmodel->time[TIME_EXECUTE_GRAPH] = itask->time;

	spin_lock_irqsave(&itaskmgr->slock, flags);
	vipx_task_trans_com_to_fre(itaskmgr, itask);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);

	if (ret)
		goto p_err_hw_load_graph;

	vipx_leave();
	return 0;
p_err_hw_load_graph:
p_err_pick:
	return ret;
}

int vipx_graphmgr_execute_model(struct vipx_graphmgr *gmgr,
		struct vipx_graph_model *gmodel,
		struct vipx_common_execute_info *einfo)
{
	int ret;

	vipx_enter();
	mutex_lock(&gmgr->mlock);
	if (gmgr->current_model) {
		if (!__vipx_graphmgr_check_model_id(gmgr, gmodel)) {
			ret = -EBUSY;
			vipx_err("other model is registered(%#x/%#x)\n",
					gmgr->current_model->id, gmodel->id);
			mutex_unlock(&gmgr->mlock);
			goto p_err;
		}
	}

	gmgr->current_model = gmodel;
	mutex_unlock(&gmgr->mlock);

	ret = __vipx_graphmgr_itf_execute_graph(gmgr->interface, gmodel, einfo);
	if (ret)
		goto p_err_execute;

	vipx_leave();
	return 0;
p_err_execute:
p_err:
	return ret;
}

int vipx_graphmgr_open(struct vipx_graphmgr *gmgr)
{
	int ret;
	char name[30];
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	vipx_enter();
	kthread_init_worker(&gmgr->worker);
	snprintf(name, sizeof(name), "vipx_graph");
	gmgr->graph_task = kthread_run(kthread_worker_fn,
			&gmgr->worker, name);
	if (IS_ERR(gmgr->graph_task)) {
		ret = PTR_ERR(gmgr->graph_task);
		vipx_err("kthread_run is fail\n");
		goto p_err_run;
	}

	ret = sched_setscheduler_nocheck(gmgr->graph_task,
			SCHED_FIFO, &param);
	if (ret) {
		vipx_err("sched_setscheduler_nocheck is fail(%d)\n", ret);
		goto p_err_sched;
	}

	gmgr->current_model = NULL;
	set_bit(VIPX_GRAPHMGR_OPEN, &gmgr->state);
	vipx_leave();
	return 0;
p_err_sched:
	kthread_stop(gmgr->graph_task);
p_err_run:
	return ret;
}

int vipx_graphmgr_close(struct vipx_graphmgr *gmgr)
{
	vipx_enter();
	kthread_stop(gmgr->graph_task);
	clear_bit(VIPX_GRAPHMGR_OPEN, &gmgr->state);
	clear_bit(VIPX_GRAPHMGR_ENUM, &gmgr->state);
	vipx_leave();
	return 0;
}

int vipx_graphmgr_probe(struct vipx_system *sys)
{
	struct vipx_graphmgr *gmgr;
	int index;

	vipx_enter();
	gmgr = &sys->graphmgr;
	gmgr->interface = &sys->interface;

	atomic_set(&gmgr->active_cnt, 0);
	mutex_init(&gmgr->mlock);
	mutex_init(&gmgr->tdlock);

	INIT_LIST_HEAD(&gmgr->tdfre_list);
	INIT_LIST_HEAD(&gmgr->tdrdy_list);
	INIT_LIST_HEAD(&gmgr->tdreq_list);
	INIT_LIST_HEAD(&gmgr->tdalc_list);
	INIT_LIST_HEAD(&gmgr->tdpro_list);
	INIT_LIST_HEAD(&gmgr->tdcom_list);

	for (index = 0; index < VIPX_MAX_TASKDESC; ++index) {
		gmgr->taskdesc[index].index = index;
		__vipx_taskdesc_set_free(gmgr, &gmgr->taskdesc[index]);
	}

	for (index = 0; index < VIPX_MAX_TASK; ++index)
		kthread_init_work(&gmgr->interface->taskmgr.task[index].work,
				vipx_interface_thread);

	return 0;
}

void vipx_graphmgr_remove(struct vipx_graphmgr *gmgr)
{
	mutex_destroy(&gmgr->tdlock);
	mutex_destroy(&gmgr->mlock);
}
