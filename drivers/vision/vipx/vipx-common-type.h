/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_COMMON_TYPE_H__
#define __VIPX_COMMON_TYPE_H__

#define MAX_INPUT_NUM			(24)
#define MAX_OUTPUT_NUM			(24)

#define GET_COMMON_GRAPH_MODEL_ID(X)	({	\
	union vipx_common_global_id temp_id;	\
	temp_id.num = (X);			\
	temp_id.head.model_id;			\
})

enum vipx_common_addr_type {
	VIPX_COMMON_V_ADDR,
	VIPX_COMMON_DV_ADDR,
	VIPX_COMMON_FD,
};

enum vipx_common_mem_attr {
	VIPX_COMMON_CACHEABLE,
	VIPX_COMMON_NON_CACHEABLE,
};

enum vipx_common_mem_type {
	VIPX_COMMON_MEM_ION,
	VIPX_COMMON_MEM_MALLOC,
	VIPX_COMMON_MEM_ASHMEM
};

struct vipx_common_mem {
	int				fd;
	unsigned int			iova;
	unsigned int			size;
	unsigned int			offset;
	unsigned char			addr_type;
	unsigned char			mem_attr;
	unsigned char			mem_type;
	unsigned char			reserved[5];
};

union vipx_common_global_id {
	struct {
		unsigned int		head :2;
		unsigned int		target :2;
		unsigned int		model_id :12;
		unsigned int		msg_id :16;
	} head;
	unsigned int			num;
};

struct vipx_common_graph_info {
	struct vipx_common_mem		graph;
	struct vipx_common_mem		temp_buf;
	struct vipx_common_mem		weight;
	struct vipx_common_mem		bias;
	unsigned int			gid;
	unsigned int			user_para_size;
	struct vipx_common_mem		user_param_buffer;
};

struct vipx_common_execute_info {
	unsigned int			gid;
	unsigned int			macro_sg_offset;
	unsigned int			num_input;
	unsigned int			num_output;
	struct vipx_common_mem		input[MAX_INPUT_NUM];
	struct vipx_common_mem		output[MAX_OUTPUT_NUM];
	unsigned int			user_para_size;
	unsigned int			reserved;
	struct vipx_common_mem		user_param_buffer;
};

#endif
