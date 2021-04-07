/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_INTERFACE_H__
#define __VIPX_INTERFACE_H__

#include <linux/platform_device.h>
#include <linux/timer.h>

#include "vipx-config.h"
#include "platform/vipx-ctrl.h"
#include "vipx-taskmgr.h"

#define VIPX_WORK_MAX_COUNT		(20)
#define VIPX_WORK_MAX_DATA		(24)
#define VIPX_RESPONSE_TIMEOUT		(10000) /* msecs */

struct vipx_system;

enum vipx_interrupt_direction {
	IRQ0_TO_DEVICE,
	IRQ1_TO_DEVICE,
	IRQ_FROM_DEVICE,
};

enum vipx_work_message {
	VIPX_WORK_MTYPE_BLK,
	VIPX_WORK_MTYPE_NBLK,
};

enum vipx_interface_state {
	VIPX_ITF_STATE_OPEN,
	VIPX_ITF_STATE_BOOTUP,
	VIPX_ITF_STATE_START
};

struct vipx_work {
	struct list_head		list;
	unsigned int			valid;
	unsigned int			id;
	enum vipx_work_message		message;
	unsigned int			param0;
	unsigned int			param1;
	unsigned int			param2;
	unsigned int			param3;
	unsigned char			data[VIPX_WORK_MAX_DATA];
};

struct vipx_work_list {
	struct vipx_work		work[VIPX_WORK_MAX_COUNT];
	spinlock_t			slock;
	struct list_head		free_head;
	unsigned int			free_cnt;
	struct list_head		reply_head;
	unsigned int			reply_cnt;
};

struct vipx_interface {
	int				irq[REG_MAX];
	void __iomem			*regs;
	struct vipx_taskmgr		taskmgr;
	void				*cookie;
	unsigned long			state;

	wait_queue_head_t		reply_wq;
	spinlock_t			process_barrier;
	struct vipx_work_list		work_list;
	struct work_struct		work_queue;

	struct vipx_task		*request[VIPX_MAX_GRAPH];
	struct vipx_task		*process;
	struct vipx_work		reply[VIPX_MAX_GRAPH];
	unsigned int			done_cnt;
	unsigned int			wait_time;

	struct vipx_mailbox_ctrl	*mctrl;
	struct vipx_system		*system;
};

int vipx_hw_wait_bootup(struct vipx_interface *itf);

int vipx_hw_config(struct vipx_interface *itf, struct vipx_task *itask);
int vipx_hw_process(struct vipx_interface *itf, struct vipx_task *itask);
int vipx_hw_create(struct vipx_interface *itf, struct vipx_task *itask);
int vipx_hw_destroy(struct vipx_interface *itf, struct vipx_task *itask);
int vipx_hw_init(struct vipx_interface *itf, struct vipx_task *itask);
int vipx_hw_deinit(struct vipx_interface *itf, struct vipx_task *itask);

int vipx_hw_load_graph(struct vipx_interface *itf, struct vipx_task *itask);
int vipx_hw_unload_graph(struct vipx_interface *itf, struct vipx_task *itask);
int vipx_hw_execute_graph(struct vipx_interface *itf, struct vipx_task *itask);

int vipx_interface_start(struct vipx_interface *itf);
int vipx_interface_stop(struct vipx_interface *itf);
int vipx_interface_open(struct vipx_interface *itf, void *mbox);
int vipx_interface_close(struct vipx_interface *itf);

int vipx_interface_probe(struct vipx_system *sys);
void vipx_interface_remove(struct vipx_interface *itf);

#endif
