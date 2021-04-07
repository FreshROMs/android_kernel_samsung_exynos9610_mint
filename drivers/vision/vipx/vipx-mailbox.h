/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_MAILBOX_H__
#define __VIPX_MAILBOX_H__

#include "vipx-message.h"

#define MAILBOX_VERSION			(1)
#define MAX_NORMAL_MSG_COUNT		(8)
#define MAX_URGENT_MSG_COUNT		(4)

enum vipx_mailbox_msg_type {
	NORMAL_MSG,
	URGENT_MSG,
};

enum vipx_mailbox_wait {
	MAILBOX_NOWAIT,
	MAILBOX_WAIT,
};

struct vipx_mailbox_head {
	unsigned short			rmsg_idx;
	unsigned short			wmsg_idx;
	unsigned short			mbox_size;
	unsigned short			elem_size;
};

struct vipx_mailbox_ctrl {
	unsigned int			mbox_version;
	unsigned int			msg_version;

	struct vipx_mailbox_head	h2d_normal_head;
	struct vipx_h2d_message		h2d_normal_data[MAX_NORMAL_MSG_COUNT];

	struct vipx_mailbox_head	h2d_urgent_head;
	struct vipx_h2d_message		h2d_urgent_data[MAX_URGENT_MSG_COUNT];

	struct vipx_mailbox_head	d2h_normal_head;
	struct vipx_d2h_message		d2h_normal_data[MAX_NORMAL_MSG_COUNT];

	struct vipx_mailbox_head	d2h_urgent_head;
	struct vipx_d2h_message		d2h_urgent_data[MAX_URGENT_MSG_COUNT];
};

void vipx_mailbox_dump(struct vipx_mailbox_ctrl *mctrl);
int vipx_mailbox_check_full(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		int wait);
int vipx_mailbox_write(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		void *msg, size_t size);

int vipx_mailbox_check_reply(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		int wait);
int vipx_mailbox_read(struct vipx_mailbox_ctrl *mctrl, unsigned int type,
		void *msg);

int vipx_mailbox_init(struct vipx_mailbox_ctrl *mctrl);
int vipx_mailbox_deinit(struct vipx_mailbox_ctrl *mctrl);

#endif
