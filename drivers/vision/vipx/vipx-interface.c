/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>

#include "vipx-log.h"
#include "vipx-device.h"
#include "vipx-binary.h"
#include "vipx-queue.h"
#include "vipx-graphmgr.h"
#include "vipx-memory.h"
#include "vipx-mailbox.h"
#include "vipx-message.h"
#include "vipx-kernel-binary.h"
#include "vipx-system.h"
#include "vipx-interface.h"

static inline void __vipx_interface_set_reply(struct vipx_interface *itf,
		unsigned int gidx)
{
	vipx_enter();
	itf->reply[gidx].valid = 1;
	wake_up(&itf->reply_wq);
	vipx_leave();
}

static inline void __vipx_interface_clear_reply(struct vipx_interface *itf,
		unsigned int gidx)
{
	vipx_enter();
	itf->reply[gidx].valid = 0;
	vipx_leave();
}

static int __vipx_interface_send_interrupt(struct vipx_interface *itf)
{
	int ret;
	struct vipx_system *sys;
	int try_count = 100;
	unsigned int val;

	vipx_enter();
	sys = itf->system;

	/* Check interrupt clear */
	while (try_count) {
		val = sys->ctrl_ops->get_irq(sys, IRQ1_TO_DEVICE);
		if (!val)
			break;

		vipx_warn("irq1(0x%x) is not yet clear (%u)\n", val, try_count);
		udelay(1);
		try_count--;
	}

	if (val) {
		ret = -EBUSY;
		vipx_err("Failed to send interrupt(0x%x)\n", val);
		goto p_err;
	}

	sys->ctrl_ops->set_irq(sys, IRQ1_TO_DEVICE, 0x100);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_hw_wait_bootup(struct vipx_interface *itf)
{
	int ret;
	int try_cnt = 1000;

	vipx_enter();
	while (!test_bit(VIPX_ITF_STATE_BOOTUP, &itf->state)) {
		if (!try_cnt)
			break;

		udelay(100);
		try_cnt--;
	}

	if (!test_bit(VIPX_ITF_STATE_BOOTUP, &itf->state)) {
		vipx_debug_dump_debug_regs();
		ret = -ETIMEDOUT;
		vipx_err("Failed to boot CM7 (%d)\n", ret);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_hw_config(struct vipx_interface *itf, struct vipx_task *itask)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

int vipx_hw_process(struct vipx_interface *itf, struct vipx_task *itask)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

int vipx_hw_create(struct vipx_interface *itf, struct vipx_task *itask)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

int vipx_hw_destroy(struct vipx_interface *itf, struct vipx_task *itask)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

int vipx_hw_init(struct vipx_interface *itf, struct vipx_task *itask)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

int vipx_hw_deinit(struct vipx_interface *itf, struct vipx_task *itask)
{
	vipx_enter();
	vipx_leave();
	return 0;
}

static int __vipx_interface_wait_mailbox_reply(struct vipx_interface *itf,
		struct vipx_task *itask)
{
	int ret;
	long timeout;

	vipx_enter();
	timeout = wait_event_timeout(itf->reply_wq,
			(itask->message == VIPX_TASK_DONE),
			msecs_to_jiffies(itf->wait_time));
	if (!timeout) {
		ret = -ETIMEDOUT;
		vipx_err("wait time(%u ms) is expired (%u)\n",
				itf->wait_time, itask->id);
		vipx_debug_dump_debug_regs();
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int __vipx_interface_send_mailbox(struct vipx_interface *itf,
	struct vipx_task *itask)
{
	int ret;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_mailbox_ctrl *mctrl;
	void *msg;
	unsigned long size, type;
	unsigned long flags, process_flags;

	vipx_enter();
	itaskmgr = &itf->taskmgr;
	mctrl = itf->mctrl;

	msg = (void *)itask->param1;
	size = itask->param2;
	type = itask->param3;

	spin_lock_irqsave(&itf->process_barrier, process_flags);
	itf->process = itask;

	ret = vipx_mailbox_check_full(mctrl, type, MAILBOX_WAIT);
	if (ret) {
		vipx_err("mailbox is full(%lu/%u)\n", type, itask->id);
		vipx_mailbox_dump(mctrl);
		goto p_err_process;
	}

	ret = vipx_mailbox_write(mctrl, type, msg, size);
	if (ret) {
		vipx_err("Failed to write to mailbox(%lu/%u)\n",
				type, itask->id);
		goto p_err_process;
	}

	ret = __vipx_interface_send_interrupt(itf);
	if (ret) {
		vipx_err("Failed to send interrupt(%lu/%u)\n",
				type, itask->id);
		goto p_err_process;
	}

	vipx_time_get_timestamp(&itask->time, TIMESTAMP_START);
	spin_unlock_irqrestore(&itf->process_barrier, process_flags);

	spin_lock_irqsave(&itaskmgr->slock, flags);
	vipx_task_trans_req_to_pro(itaskmgr, itask);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);

	ret = __vipx_interface_wait_mailbox_reply(itf, itask);
	if (ret)
		vipx_mailbox_dump(mctrl);

	spin_lock_irqsave(&itaskmgr->slock, flags);
	vipx_task_trans_pro_to_com(itaskmgr, itask);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);

	spin_lock_irqsave(&itf->process_barrier, process_flags);
	itf->process = NULL;
	spin_unlock_irqrestore(&itf->process_barrier, process_flags);

	vipx_leave();
	return ret;
p_err_process:
	spin_lock_irqsave(&itaskmgr->slock, flags);
	vipx_task_trans_req_to_com(itaskmgr, itask);
	spin_unlock_irqrestore(&itaskmgr->slock, flags);

	itf->process = NULL;
	spin_unlock_irqrestore(&itf->process_barrier, process_flags);
	return ret;
}

int vipx_hw_load_graph(struct vipx_interface *itf, struct vipx_task *itask)
{
	int ret;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_h2d_message msg;
	struct vipx_h2d_load_graph_req *req;
	struct vipx_common_graph_info *ginfo;
	int idx;

	vipx_enter();
	itaskmgr = &itf->taskmgr;

	msg.head.mbox_version = MAILBOX_VERSION;
	msg.head.msg_version = MESSAGE_VERSION;
	msg.head.msg_type = NORMAL_MSG;
	msg.head.msg_size = sizeof(*req);
	msg.head.trans_id = itask->id;
	msg.head.msg_id = H2D_LOAD_GRAPH_REQ;

	req = &msg.body.load_graph_req;
	ginfo = (struct vipx_common_graph_info *)itask->param0;
	req->graph_info = *ginfo;

	itask->param1 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = NORMAL_MSG;

	vipx_dbg("[%s] load graph (firmware)\n", __func__);
	for (idx = 0; idx < sizeof(*ginfo) >> 2; ++idx)
		vipx_dbg("[%3d] %#10x\n", idx, ((int *)ginfo)[idx]);

	ret = __vipx_interface_send_mailbox(itf, itask);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_hw_execute_graph(struct vipx_interface *itf, struct vipx_task *itask)
{
	int ret;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_h2d_message msg;
	struct vipx_h2d_execute_req *req;
	struct vipx_common_execute_info *einfo;
	struct vipx_graph_model *gmodel;
	struct vipx_kernel_binary *kbin, *temp;
	int idx = 0;
	int *debug_addr;
	int iter;

	vipx_enter();
	itaskmgr = &itf->taskmgr;

	msg.head.mbox_version = MAILBOX_VERSION;
	msg.head.msg_version = MESSAGE_VERSION;
	msg.head.msg_type = NORMAL_MSG;
	msg.head.msg_size = sizeof(*req);
	msg.head.trans_id = itask->id;
	msg.head.msg_id = H2D_EXECUTE_REQ;

	req = &msg.body.execute_req;
	einfo = (struct vipx_common_execute_info *)itask->param0;
	req->execute_info = *einfo;

	gmodel = (struct vipx_graph_model *)itask->param1;
	req->num_kernel_bin = gmodel->kbin_count;
	//TODO check max count
	list_for_each_entry_safe(kbin, temp, &gmodel->kbin_list, glist) {
		req->kernel_bin[idx].addr = kbin->buffer.dvaddr;
		req->kernel_bin[idx].size = kbin->buffer.size;
		idx++;
	}

	itask->param1 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = NORMAL_MSG;

	vipx_dbg("[%s] execute graph (firmware)\n", __func__);
	vipx_dbg("num_kernel_bin : %d\n", req->num_kernel_bin);
	for (idx = 0; idx < req->num_kernel_bin; ++idx) {
		vipx_dbg("[%3d] kernel addr : %#10x\n",
				idx, req->kernel_bin[idx].addr);
		vipx_dbg("[%3d] kernel size : %#10x\n",
				idx, req->kernel_bin[idx].size);
	}

	vipx_dbg("gid : %#x\n", einfo->gid);
	vipx_dbg("macro_sc_offset : %u\n", einfo->macro_sg_offset);
	vipx_dbg("num_input : %u\n", einfo->num_input);
	for (idx = 0; idx < einfo->num_input; ++idx) {
		debug_addr = (int *)&einfo->input[idx];
		for (iter = 0; iter < sizeof(einfo->input[0]) >> 2; ++iter)
			vipx_dbg("[%3d][%3d] %#10x\n",
					idx, iter, debug_addr[iter]);
	}

	vipx_dbg("num_output : %u\n", einfo->num_output);
	for (idx = 0; idx < einfo->num_output; ++idx) {
		debug_addr = (int *)&einfo->output[idx];
		for (iter = 0; iter < sizeof(einfo->output[0]) >> 2; ++iter)
			vipx_dbg("[%3d][%3d] %#10x\n",
					idx, iter, debug_addr[iter]);
	}

	vipx_dbg("user_para_size(not use) : %u\n", einfo->user_para_size);
	debug_addr = (int *)&einfo->user_param_buffer;
	for (idx = 0; idx < sizeof(einfo->user_param_buffer) >> 2; ++idx)
		vipx_dbg("[%3d] %#10x\n", idx, debug_addr[idx]);

	ret = __vipx_interface_send_mailbox(itf, itask);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_hw_unload_graph(struct vipx_interface *itf, struct vipx_task *itask)
{
	int ret;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_h2d_message msg;
	struct vipx_h2d_unload_graph_req *req;
	struct vipx_common_graph_info *ginfo;

	vipx_enter();
	itaskmgr = &itf->taskmgr;

	msg.head.mbox_version = MAILBOX_VERSION;
	msg.head.msg_version = MESSAGE_VERSION;
	msg.head.msg_type = NORMAL_MSG;
	msg.head.msg_size = sizeof(*req);
	msg.head.trans_id = itask->id;
	msg.head.msg_id = H2D_UNLOAD_GRAPH_REQ;

	req = &msg.body.unload_graph_req;
	ginfo = (struct vipx_common_graph_info *)itask->param0;
	req->gid = ginfo->gid;

	itask->param1 = (unsigned long)&msg;
	itask->param2 = sizeof(msg);
	itask->param3 = NORMAL_MSG;

	vipx_dbg("[%s] unload graph (firmware)\n", __func__);
	vipx_dbg("gid : %#x\n", req->gid);

	ret = __vipx_interface_send_mailbox(itf, itask);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_interface_work_list_queue_free(
		struct vipx_work_list *work_list, struct vipx_work *work)
{
	unsigned long flags;

	vipx_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	list_add_tail(&work->list, &work_list->free_head);
	work_list->free_cnt++;
	spin_unlock_irqrestore(&work_list->slock, flags);
	vipx_leave();
}

static struct vipx_work *__vipx_interface_work_list_dequeue_free(
		struct vipx_work_list *work_list)
{
	unsigned long flags;
	struct vipx_work *work;

	vipx_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	if (work_list->free_cnt) {
		work = list_first_entry(&work_list->free_head, struct vipx_work,
				list);
		list_del(&work->list);
		work_list->free_cnt--;
	} else {
		work = NULL;
		vipx_err("free work list empty\n");
	}
	spin_unlock_irqrestore(&work_list->slock, flags);

	vipx_leave();
	return work;
}

static void __vipx_interface_work_list_queue_reply(
		struct vipx_work_list *work_list, struct vipx_work *work)
{
	unsigned long flags;

	vipx_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	list_add_tail(&work->list, &work_list->reply_head);
	work_list->reply_cnt++;
	spin_unlock_irqrestore(&work_list->slock, flags);
	vipx_leave();
}

static struct vipx_work *__vipx_interface_work_list_dequeue_reply(
		struct vipx_work_list *work_list)
{
	unsigned long flags;
	struct vipx_work *work;

	vipx_enter();
	spin_lock_irqsave(&work_list->slock, flags);
	if (work_list->reply_cnt) {
		work = list_first_entry(&work_list->reply_head,
				struct vipx_work, list);
		list_del(&work->list);
		work_list->reply_cnt--;
	} else {
		work = NULL;
	}
	spin_unlock_irqrestore(&work_list->slock, flags);

	vipx_leave();
	return work;
}

static void __vipx_interface_work_list_init(struct vipx_work_list *work_list,
		unsigned int count)
{
	unsigned int idx;

	vipx_enter();
	spin_lock_init(&work_list->slock);
	INIT_LIST_HEAD(&work_list->free_head);
	work_list->free_cnt = 0;
	INIT_LIST_HEAD(&work_list->reply_head);
	work_list->reply_cnt = 0;

	for (idx = 0; idx < count; ++idx)
		__vipx_interface_work_list_queue_free(work_list,
				&work_list->work[idx]);

	vipx_leave();
}

static void __vipx_interface_isr(void *data)
{
	struct vipx_interface *itf;
	struct vipx_mailbox_ctrl *mctrl;
	struct vipx_work_list *work_list;
	struct work_struct *work_queue;
	struct vipx_work *work;
	struct vipx_d2h_message rsp;
	unsigned long flags;

	vipx_enter();
	itf = (struct vipx_interface *)data;
	mctrl = itf->mctrl;
	work_list = &itf->work_list;
	work_queue = &itf->work_queue;

	while (!vipx_mailbox_check_reply(mctrl, URGENT_MSG, 0)) {
		vipx_mailbox_read(mctrl, URGENT_MSG, &rsp);


		work = __vipx_interface_work_list_dequeue_free(work_list);
		if (work) {
			/* TODO */
			work->id = rsp.head.trans_id;
			work->message = 0;
			work->param0 = 0;
			work->param1 = 0;
			work->param2 = 0;
			work->param3 = 0;

			__vipx_interface_work_list_queue_reply(work_list, work);

			if (!work_pending(work_queue))
				schedule_work(work_queue);
		}
	}

	while (!vipx_mailbox_check_reply(mctrl, NORMAL_MSG, 0)) {
		vipx_mailbox_read(mctrl, NORMAL_MSG, &rsp);

		if (rsp.head.msg_id == D2H_BOOTUP_NTF) {
			if (rsp.body.bootup_ntf.result == 0) {
				vipx_info("CM7 bootup complete (%u)\n",
						rsp.head.trans_id);
				set_bit(VIPX_ITF_STATE_BOOTUP, &itf->state);
			} else {
				/* TODO process boot fail */
				vipx_err("CM7 bootup failed (%u,%d)\n",
						rsp.head.trans_id,
						rsp.body.bootup_ntf.result);
			}
			break;
		} else if (rsp.head.msg_id == D2H_LOAD_GRAPH_RSP) {
			spin_lock_irqsave(&itf->process_barrier, flags);
			if (!itf->process) {
				vipx_err("process task is empty\n");
				spin_unlock_irqrestore(&itf->process_barrier,
						flags);
				continue;
			}

			vipx_dbg("D2H_LOAD_GRAPH_RSP : %x\n",
					rsp.body.load_graph_rsp.result);
			vipx_dbg("D2H_LOAD_GRAPH_RSP(graph_id) : %d\n",
					rsp.body.load_graph_rsp.gid);
			vipx_time_get_timestamp(&itf->process->time,
					TIMESTAMP_END);
			itf->process->message = VIPX_TASK_DONE;
			wake_up(&itf->reply_wq);
			spin_unlock_irqrestore(&itf->process_barrier, flags);
			continue;
		} else if (rsp.head.msg_id == D2H_EXECUTE_RSP) {
			spin_lock_irqsave(&itf->process_barrier, flags);
			if (!itf->process) {
				vipx_err("process task is empty\n");
				spin_unlock_irqrestore(&itf->process_barrier,
						flags);
				continue;
			}

			vipx_dbg("D2H_EXECUTE_RSP : %x\n",
					rsp.body.execute_rsp.result);
			vipx_dbg("D2H_EXECUTE_RSP(graph_id) : %d\n",
					rsp.body.execute_rsp.gid);
			vipx_dbg("D2H_EXECUTE_RSP(macro) : %d\n",
					rsp.body.execute_rsp.macro_sgid);
			vipx_time_get_timestamp(&itf->process->time,
					TIMESTAMP_END);
			itf->process->message = VIPX_TASK_DONE;
			wake_up(&itf->reply_wq);
			spin_unlock_irqrestore(&itf->process_barrier, flags);
			continue;
		} else if (rsp.head.msg_id == D2H_UNLOAD_GRAPH_RSP) {
			spin_lock_irqsave(&itf->process_barrier, flags);
			if (!itf->process) {
				vipx_err("process task is empty\n");
				spin_unlock_irqrestore(&itf->process_barrier,
						flags);
				continue;
			}

			vipx_dbg("D2H_UNLOAD_GRAPH_RSP : %x\n",
					rsp.body.unload_graph_rsp.result);
			vipx_dbg("D2H_UNLOAD_GRAPH_RSP(graph_id) : %d\n",
					rsp.body.unload_graph_rsp.gid);
			vipx_time_get_timestamp(&itf->process->time,
					TIMESTAMP_END);
			itf->process->message = VIPX_TASK_DONE;
			wake_up(&itf->reply_wq);
			spin_unlock_irqrestore(&itf->process_barrier, flags);
			continue;
		}
		work = __vipx_interface_work_list_dequeue_free(work_list);
		if (work) {
			/* TODO */
			work->id = rsp.head.trans_id;
			work->message = 0;
			work->param0 = 0;
			work->param1 = 0;
			work->param2 = 0;
			work->param3 = 0;

			__vipx_interface_work_list_queue_reply(work_list, work);

			if (!work_pending(work_queue))
				schedule_work(work_queue);
		}
	}

	vipx_leave();
}

static irqreturn_t vipx_interface_isr0(int irq, void *data)
{
	struct vipx_interface *itf;
	struct vipx_system *sys;
	unsigned int val;

	vipx_enter();
	itf = (struct vipx_interface *)data;
	sys = itf->system;

	val = sys->ctrl_ops->get_irq(sys, IRQ_FROM_DEVICE);
	if (val & 0x1) {
		val &= ~(0x1);
		sys->ctrl_ops->clear_irq(sys, IRQ_FROM_DEVICE, val);

		__vipx_interface_isr(data);
	}

	vipx_leave();
	return IRQ_HANDLED;
}

static irqreturn_t vipx_interface_isr1(int irq, void *data)
{
	struct vipx_interface *itf;
	struct vipx_system *sys;
	unsigned int val;

	vipx_enter();
	itf = (struct vipx_interface *)data;
	sys = itf->system;

	val = sys->ctrl_ops->get_irq(sys, IRQ_FROM_DEVICE);
	if (val & (0x1 << 0x1)) {
		val &= ~(0x1 << 0x1);
		sys->ctrl_ops->clear_irq(sys, IRQ_FROM_DEVICE, val);

		__vipx_interface_isr(data);
	}

	vipx_leave();
	return IRQ_HANDLED;
}

int vipx_interface_start(struct vipx_interface *itf)
{
	vipx_enter();
	set_bit(VIPX_ITF_STATE_START, &itf->state);

	vipx_leave();
	return 0;
}

static void __vipx_interface_cleanup(struct vipx_interface *itf)
{
	struct vipx_taskmgr *taskmgr;
	int task_count;

	unsigned long flags;
	unsigned int idx;
	struct vipx_task *task;

	vipx_enter();
	taskmgr = &itf->taskmgr;

	task_count = taskmgr->req_cnt + taskmgr->pro_cnt + taskmgr->com_cnt;
	if (task_count) {
		vipx_warn("count of task manager is not zero (%u/%u/%u)\n",
				taskmgr->req_cnt, taskmgr->pro_cnt,
				taskmgr->com_cnt);
		/* TODO remove debug log */
		vipx_task_print_all(&itf->taskmgr);

		spin_lock_irqsave(&taskmgr->slock, flags);

		for (idx = 1; idx < taskmgr->tot_cnt; ++idx) {
			task = &taskmgr->task[idx];
			if (task->state == VIPX_TASK_STATE_FREE)
				continue;

			vipx_task_trans_any_to_fre(taskmgr, task);
			task_count--;
		}

		spin_unlock_irqrestore(&taskmgr->slock, flags);
	}
}

int vipx_interface_stop(struct vipx_interface *itf)
{
	vipx_enter();
	__vipx_interface_cleanup(itf);
	clear_bit(VIPX_ITF_STATE_BOOTUP, &itf->state);
	clear_bit(VIPX_ITF_STATE_START, &itf->state);

	vipx_leave();
	return 0;
}

int vipx_interface_open(struct vipx_interface *itf, void *mbox)
{
	int idx;

	vipx_enter();
	itf->mctrl = mbox;
	vipx_mailbox_init(itf->mctrl);

	itf->process = NULL;
	for (idx = 0; idx < VIPX_MAX_GRAPH; ++idx) {
		itf->request[idx] = NULL;
		itf->reply[idx].valid = 0;
	}

	itf->done_cnt = 0;
	itf->state = 0;
	set_bit(VIPX_ITF_STATE_OPEN, &itf->state);

	vipx_leave();
	return 0;
}

int vipx_interface_close(struct vipx_interface *itf)
{
	vipx_enter();
	clear_bit(VIPX_ITF_STATE_OPEN, &itf->state);
	itf->mctrl = NULL;

	vipx_leave();
	return 0;
}

static void vipx_interface_work_reply_func(struct work_struct *data)
{
	struct vipx_interface *itf;
	struct vipx_taskmgr *itaskmgr;
	struct vipx_work_list *work_list;
	struct vipx_work *work;
	struct vipx_task *itask;
	unsigned int gidx;
	unsigned long flags;

	vipx_enter();
	itf = container_of(data, struct vipx_interface, work_queue);
	itaskmgr = &itf->taskmgr;
	work_list = &itf->work_list;

	while (true) {
		work = __vipx_interface_work_list_dequeue_reply(work_list);
		if (!work)
			break;

		if (work->id >= VIPX_MAX_TASK) {
			vipx_err("work id is invalid (%d)\n", work->id);
			goto p_end;
		}

		itask = &itaskmgr->task[work->id];

		if (itask->state != VIPX_TASK_STATE_PROCESS) {
			vipx_err("task(%u/%u/%lu) state(%u) is invalid\n",
					itask->message, itask->index,
					itask->param1, itask->state);
			vipx_err("work(%u/%u/%u)\n", work->id,
					work->param0, work->param1);
			vipx_task_print_all(&itf->taskmgr);
			goto p_end;
		}

		switch (itask->message) {
		case VIPX_TASK_INIT:
		case VIPX_TASK_DEINIT:
		case VIPX_TASK_CREATE:
		case VIPX_TASK_DESTROY:
		case VIPX_TASK_ALLOCATE:
		case VIPX_TASK_REQUEST:
			gidx = itask->param1;
			itf->reply[gidx] = *work;
			__vipx_interface_set_reply(itf, gidx);
			break;
		case VIPX_TASK_PROCESS:
			spin_lock_irqsave(&itaskmgr->slock, flags);
			vipx_task_trans_pro_to_com(itaskmgr, itask);
			spin_unlock_irqrestore(&itaskmgr->slock, flags);

			itask->param2 = 0;
			itask->param3 = 0;
			vipx_graphmgr_queue(itf->cookie, itask);
			itf->done_cnt++;
			break;
		default:
			vipx_err("unresolved task message(%d) have arrived\n",
					work->message);
			break;
		}
p_end:
		__vipx_interface_work_list_queue_free(work_list, work);
	};
	vipx_leave();
}

int vipx_interface_probe(struct vipx_system *sys)
{
	int ret;
	struct platform_device *pdev;
	struct vipx_interface *itf;
	struct device *dev;
	struct vipx_taskmgr *taskmgr;

	vipx_enter();
	pdev = to_platform_device(sys->dev);
	itf = &sys->interface;
	itf->system = sys;
	dev = &pdev->dev;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		vipx_err("platform_get_irq(0) is fail (%d)\n", ret);
		goto p_err_irq0;
	}

	itf->irq[REG_SS1] = ret;

	ret = platform_get_irq(pdev, 1);
	if (ret < 0) {
		vipx_err("platform_get_irq(1) is fail (%d)\n", ret);
		goto p_err_irq1;
	}

	itf->irq[REG_SS2] = ret;

	ret = devm_request_irq(dev, itf->irq[REG_SS1], vipx_interface_isr0,
			0, dev_name(dev), itf);
	if (ret) {
		vipx_err("devm_request_irq(0) is fail (%d)\n", ret);
		goto p_err_req_irq0;
	}

	ret = devm_request_irq(dev, itf->irq[REG_SS2], vipx_interface_isr1,
			0, dev_name(dev), itf);
	if (ret) {
		vipx_err("devm_request_irq(1) is fail (%d)\n", ret);
		goto p_err_req_irq1;
	}

	itf->regs = sys->reg_ss[REG_SS1];

	taskmgr = &itf->taskmgr;
	spin_lock_init(&taskmgr->slock);
	ret = vipx_task_init(taskmgr, VIPX_MAX_GRAPH, itf);
	if (ret)
		goto p_err_taskmgr;

	itf->cookie = &sys->graphmgr;
	itf->state = 0;

	init_waitqueue_head(&itf->reply_wq);
	spin_lock_init(&itf->process_barrier);

	INIT_WORK(&itf->work_queue, vipx_interface_work_reply_func);
	__vipx_interface_work_list_init(&itf->work_list, VIPX_WORK_MAX_COUNT);

	itf->wait_time = VIPX_RESPONSE_TIMEOUT;

	vipx_leave();
	return 0;
p_err_taskmgr:
	devm_free_irq(dev, itf->irq[REG_SS2], itf);
p_err_req_irq1:
	devm_free_irq(dev, itf->irq[REG_SS1], itf);
p_err_req_irq0:
p_err_irq1:
p_err_irq0:
	return ret;
}

void vipx_interface_remove(struct vipx_interface *itf)
{
	vipx_task_deinit(&itf->taskmgr);
	devm_free_irq(itf->system->dev, itf->irq[REG_SS2], itf);
	devm_free_irq(itf->system->dev, itf->irq[REG_SS1], itf);
}
