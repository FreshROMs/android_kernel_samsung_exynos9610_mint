/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_MESSAGE_H__
#define __VIPX_MESSAGE_H__

#include "vipx-common-type.h"

#define MESSAGE_VERSION				(1)

#define MAX_NUM_KERNEL				(4)
#define MAX_NUM_TEMP_BUF			(32)
#define MAX_SIZE_USER_PARA			(1024)

enum vipx_h2d_message_id {
	H2D_INIT_REQ,
	H2D_LOAD_GRAPH_REQ,
	H2D_EXECUTE_REQ,
	H2D_UNLOAD_GRAPH_REQ,
	H2D_DEINIT_REQ,
	H2D_MESSAGE_NUM
};

enum vipx_d2h_message_id {
	D2H_INIT_RSP,
	D2H_LOAD_GRAPH_RSP,
	D2H_EXECUTE_RSP,
	D2H_UNLOAD_GRAPH_RSP,
	D2H_DEINIT_RSP,
	D2H_BOOTUP_NTF,
	D2H_MESSAGE_NUM
};

struct vipx_common_device_addr {
	unsigned int				addr;
	unsigned int				size;
};

struct vipx_h2d_init_req {
	struct vipx_common_device_addr		heap;
	struct vipx_common_device_addr		boot_kernal;
};

struct vipx_h2d_load_graph_req {
	struct vipx_common_graph_info		graph_info;
	unsigned char				user_para[MAX_SIZE_USER_PARA];
};

struct vipx_h2d_execute_req {
	unsigned int				num_kernel_bin;
	unsigned int				reserved;
	struct vipx_common_device_addr		kernel_bin[MAX_NUM_KERNEL];
	struct vipx_common_execute_info		execute_info;
	unsigned char				user_para[MAX_SIZE_USER_PARA];
};

struct vipx_h2d_unload_graph_req {
	unsigned int				gid;
	unsigned int				reserved;
};

struct vipx_h2d_deinit_req {
	unsigned int				dummy;
	unsigned int				reserved;
};

struct vipx_d2h_init_rsp {
	int					result;
	unsigned int				reserved;
};

struct vipx_d2h_load_graph_rsp {
	int					result;
	unsigned int				gid;
};

struct vipx_d2h_execute_rsp {
	int					result;
	unsigned int				gid;
	unsigned int				macro_sgid;
	unsigned int				reserved;
};

struct vipx_d2h_unload_graph_rsp {
	int					result;
	unsigned int				gid;
};

struct vipx_d2h_deinit_rsp {
	int					result;
	unsigned int				reserved;
};

struct vipx_d2h_bootup_ntf {
	int					result;
	unsigned int				reserved;
};

struct vipx_common_message_head {
	unsigned int				mbox_version;
	unsigned int				msg_version;
	unsigned int				msg_type;
	unsigned int				msg_size;
	unsigned int				trans_id;
	unsigned int				msg_id;
};

union vipx_h2d_message_body {
	struct vipx_h2d_init_req		init_req;
	struct vipx_h2d_load_graph_req		load_graph_req;
	struct vipx_h2d_execute_req		execute_req;
	struct vipx_h2d_unload_graph_req	unload_graph_req;
	struct vipx_h2d_deinit_req		deinit_req;
};

struct vipx_h2d_message {
	struct vipx_common_message_head		head;
	union vipx_h2d_message_body		body;
};

union vipx_d2h_message_body {
	struct vipx_d2h_bootup_ntf		bootup_ntf;
	struct vipx_d2h_init_rsp		init_rsp;
	struct vipx_d2h_load_graph_rsp		load_graph_rsp;
	struct vipx_d2h_execute_rsp		execute_rsp;
	struct vipx_d2h_unload_graph_rsp	unload_graph_rsp;
	struct vipx_d2h_deinit_rsp		deinit_rsp;
};

struct vipx_d2h_message {
	struct vipx_common_message_head		head;
	union vipx_d2h_message_body		body;
};

#endif
