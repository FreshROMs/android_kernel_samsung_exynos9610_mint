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

#include "vipx-log.h"
#include "vipx-io.h"
#include "vipx-mailbox.h"

static inline int __vipx_mailbox_is_empty(struct vipx_mailbox_head *head)
{
	vipx_check();
	return head->wmsg_idx == head->rmsg_idx;
}

static void __vipx_mailbox_dump(struct vipx_mailbox_head *head, void *data)
{
	int idx;

	vipx_enter();
	vipx_info("mailbox haed (%u,%u,%u,%u)\n",
			head->rmsg_idx, head->wmsg_idx,
			head->mbox_size, head->elem_size);

	for (idx = 0; idx < head->mbox_size; ++idx) {
		vipx_info("mailbox data (%d/%u)\n", idx, head->mbox_size - 1);
		print_hex_dump(KERN_INFO, "[VIPx]:", DUMP_PREFIX_OFFSET, 32, 4,
				data + (idx * head->elem_size),
				head->elem_size, false);
	}
	vipx_leave();
}

void vipx_mailbox_dump(struct vipx_mailbox_ctrl *mctrl)
{
	vipx_enter();
	vipx_info("h2d normal mailbox dump\n");
	__vipx_mailbox_dump(&mctrl->h2d_normal_head,
			mctrl->h2d_normal_data);

	vipx_info("h2d urgent mailbox dump\n");
	__vipx_mailbox_dump(&mctrl->h2d_urgent_head,
			&mctrl->h2d_urgent_data);

	vipx_info("d2h normal mailbox dump\n");
	__vipx_mailbox_dump(&mctrl->d2h_normal_head,
			&mctrl->d2h_normal_data);

	vipx_info("d2h urgent mailbox dump\n");
	__vipx_mailbox_dump(&mctrl->d2h_urgent_head,
			&mctrl->d2h_urgent_data);
	vipx_leave();
}

static ssize_t __vipx_mailbox_get_remain_size(struct vipx_mailbox_head *head)
{
	unsigned int rmsg_idx, wmsg_idx, mbox_size;
	unsigned int ridx, widx;
	unsigned int used;

	vipx_check();
	rmsg_idx = head->rmsg_idx;
	wmsg_idx = head->wmsg_idx;
	mbox_size = head->mbox_size;

	ridx = (rmsg_idx >= mbox_size) ? rmsg_idx - mbox_size : rmsg_idx;
	widx = (wmsg_idx >= mbox_size) ? wmsg_idx - mbox_size : wmsg_idx;

	if (widx > ridx)
		used = widx - ridx;
	else if (widx < ridx)
		used = (mbox_size - ridx) + widx;
	else if (wmsg_idx != rmsg_idx)
		used = mbox_size;
	else
		used = 0;

	return mbox_size - used;
}

int vipx_mailbox_check_full(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		int wait)
{
	int ret;
	struct vipx_mailbox_head *head;
	int try_count = 1000;
	ssize_t remain_size;

	vipx_enter();
	if (type == NORMAL_MSG) {
		head = &mctrl->h2d_normal_head;
	} else if (type == URGENT_MSG) {
		head = &mctrl->h2d_urgent_head;
	} else {
		ret = -EINVAL;
		vipx_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	while (try_count) {
		remain_size = __vipx_mailbox_get_remain_size(head);
		if (remain_size > 0) {
			break;
		} else if (remain_size < 0) {
			ret = -EFAULT;
			vipx_err("index of mbox(%u) is invalid (%u/%u/%u/%u)\n",
					type, head->wmsg_idx, head->rmsg_idx,
					head->mbox_size, head->elem_size);
			goto p_err;
		} else if (wait) {
			vipx_warn("mbox(%u) is full (%u/%u/%u/%u/%d)\n",
					type, head->wmsg_idx, head->rmsg_idx,
					head->mbox_size, head->elem_size,
					try_count);
			udelay(10);
			try_count--;
		} else {
			break;
		}
	}

	if (!remain_size) {
		ret = -EBUSY;
		vipx_err("mbox(%u) is full (%u/%u/%u/%u)\n",
				type, head->wmsg_idx, head->rmsg_idx,
				head->mbox_size, head->elem_size);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_mailbox_write(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		void *msg, size_t size)
{
	int ret;
	struct vipx_mailbox_head *head;
	unsigned int wmsg_idx, mbox_size;
	unsigned int widx;
	struct vipx_h2d_message *h2d_msg;
	void *wptr;
	struct vipx_h2d_message *debug;

	vipx_enter();
	if (size > sizeof(struct vipx_h2d_message)) {
		ret = -EINVAL;
		vipx_err("message size(%zu/%zu) for mbox(%u) is invalid\n",
				size, sizeof(struct vipx_h2d_message), type);
		goto p_err;
	}

	if (type == NORMAL_MSG) {
		head = &mctrl->h2d_normal_head;
		h2d_msg = mctrl->h2d_normal_data;
	} else if (type == URGENT_MSG) {
		head = &mctrl->h2d_urgent_head;
		h2d_msg = mctrl->h2d_urgent_data;
	} else {
		ret = -EINVAL;
		vipx_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	wmsg_idx = head->wmsg_idx;
	mbox_size = head->mbox_size;

	widx = (wmsg_idx >= mbox_size) ? wmsg_idx - mbox_size : wmsg_idx;

	wptr = &h2d_msg[widx];
	vipx_io_copy_mem2io(wptr, msg, size);

	widx = wmsg_idx + 1;

	debug = msg;
	vipx_dbg("[MBOX(%d)-W] mbox[v%u/r:%u/w:%u/s:%u/e:%u]\n",
			type, mctrl->mbox_version,
			head->rmsg_idx, head->wmsg_idx,
			mbox_size, head->elem_size);
	vipx_dbg("[MBOX(%d)-W] message[v%u/t:%u/s:%u/tid:%u/mid:%u]\n",
			type, debug->head.msg_version, debug->head.msg_type,
			debug->head.msg_size, debug->head.trans_id,
			debug->head.msg_id);
#ifdef DEBUG_LOG_MAILBOX_DUMP
	for (ret = 0; ret < debug->head.msg_size >> 2; ++ret) {
		vipx_dbg("[MBOX(%d)-W][%4d] %#10x\n",
				type, ret, ((int *)&debug->body)[ret]);
	}
#endif

	head->wmsg_idx = (widx >= (mbox_size << 1)) ?
		widx - (mbox_size << 1) : widx;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_mailbox_check_reply(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		int wait)
{
	int ret;
	struct vipx_mailbox_head *head;
	int try_count = 1000;

	vipx_enter();
	if (type == NORMAL_MSG) {
		head = &mctrl->d2h_normal_head;
	} else if (type == URGENT_MSG) {
		head = &mctrl->d2h_urgent_head;
	} else {
		ret = -EINVAL;
		vipx_err("invalid mbox type (%u)\n", type);
		goto p_err;
	}

	while (try_count) {
		ret = __vipx_mailbox_is_empty(head);
		if (ret) {
			if (!wait)
				break;

			udelay(10);
			try_count--;
		} else {
			break;
		}
	}

	if (ret) {
		ret = -EFAULT;
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_mailbox_read(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		void *msg)
{
	int ret;
	struct vipx_mailbox_head *head;
	unsigned int rmsg_idx, mbox_size;
	unsigned int ridx;
	struct vipx_d2h_message *d2h_msg;
	void *rptr;
	struct vipx_d2h_message *debug;

	vipx_enter();
	if (type == NORMAL_MSG) {
		head = &mctrl->d2h_normal_head;
		d2h_msg = mctrl->d2h_normal_data;
	} else if (type == URGENT_MSG) {
		head = &mctrl->d2h_urgent_head;
		d2h_msg = mctrl->d2h_urgent_data;
	} else {
		ret = -EINVAL;
		vipx_err("invalid mbox type(%u)\n", type);
		goto p_err;
	}

	rmsg_idx = head->rmsg_idx;
	mbox_size = head->mbox_size;

	ridx = (rmsg_idx >= mbox_size) ? rmsg_idx - mbox_size : rmsg_idx;

	rptr = &d2h_msg[ridx];
	vipx_io_copy_io2mem(msg, rptr, sizeof(struct vipx_d2h_message));

	ridx = rmsg_idx + 1;

	debug = msg;
	vipx_dbg("[MBOX(%d)-R] mbox[v%u/r:%u/w:%u/s:%u/e:%u]\n",
			type, mctrl->mbox_version,
			head->rmsg_idx, head->wmsg_idx,
			mbox_size, head->elem_size);
	vipx_dbg("[MBOX(%d)-R] message[v%u/t:%u/s:%u/tid:%u/mid:%u]\n",
			type, debug->head.msg_version, debug->head.msg_type,
			debug->head.msg_size, debug->head.trans_id,
			debug->head.msg_id);
#ifdef DEBUG_LOG_MAILBOX_DUMP
	for (ret = 0; ret < debug->head.msg_size >> 2; ++ret) {
		vipx_dbg("[MBOX(%d)-R][%4d] %#10x\n",
				type, ret, ((int *)&debug->body)[ret]);
	}
#endif

	head->rmsg_idx = (ridx >= (mbox_size << 1)) ?
		ridx - (mbox_size << 1) : ridx;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_mailbox_init(struct vipx_mailbox_head *head,
		unsigned short mbox_size, unsigned short elem_size)
{
	vipx_enter();
	head->rmsg_idx = 0;
	head->wmsg_idx = 0;

	head->mbox_size = mbox_size;
	head->elem_size = elem_size;
	vipx_leave();
}

int vipx_mailbox_init(struct vipx_mailbox_ctrl *mctrl)
{
	vipx_enter();
	mctrl->mbox_version = MAILBOX_VERSION;
	mctrl->msg_version = MESSAGE_VERSION;

	__vipx_mailbox_init(&mctrl->h2d_normal_head, MAX_NORMAL_MSG_COUNT,
			sizeof(mctrl->h2d_normal_data[0]));
	__vipx_mailbox_init(&mctrl->h2d_urgent_head, MAX_URGENT_MSG_COUNT,
			sizeof(mctrl->h2d_urgent_data[0]));
	__vipx_mailbox_init(&mctrl->d2h_normal_head, MAX_NORMAL_MSG_COUNT,
			sizeof(mctrl->d2h_normal_data[0]));
	__vipx_mailbox_init(&mctrl->d2h_urgent_head, MAX_URGENT_MSG_COUNT,
			sizeof(mctrl->d2h_urgent_data[0]));

	vipx_leave();
	return 0;
}

int vipx_mailbox_deinit(struct vipx_mailbox_ctrl *mctrl)
{
	vipx_enter();
	vipx_leave();
	return 0;
}
