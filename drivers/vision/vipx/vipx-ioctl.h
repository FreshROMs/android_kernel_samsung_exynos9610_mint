/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_IOCTL_H__
#define __VIPX_IOCTL_H__

#include <linux/fs.h>
#include <linux/uaccess.h>

#include "vs4l.h"
#include "vipx-common-type.h"

#define VIPX_CTRL_VIPX_BASE		(0x00010000)
#define VIPX_CTRL_DUMP			(VIPX_CTRL_VIPX_BASE + 1)
#define VIPX_CTRL_MODE			(VIPX_CTRL_VIPX_BASE + 2)
#define VIPX_CTRL_TEST			(VIPX_CTRL_VIPX_BASE + 3)

struct vipx_context;

struct vipx_ioc_load_kernel_binary {
	unsigned int			size;
	unsigned int			global_id;
	int				kernel_fd;
	unsigned int			kernel_size;
	int				ret;
	struct timespec			timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_unload_kernel_binary {
	unsigned int			size;
	unsigned int			global_id;
	int				kernel_fd;
	unsigned int			kernel_size;
	int				ret;
	struct timespec			timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_load_graph_info {
	unsigned int			size;
	struct vipx_common_graph_info	graph_info;
	int				ret;
	struct timespec			timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_unload_graph_info {
	unsigned int			size;
	unsigned int			graph_id;
	int				ret;
	struct timespec			timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_execute_submodel {
	unsigned int			size;
	struct vipx_common_execute_info	execute_info;
	int				ret;
	struct timespec			timestamp[4];
	int				reserved[2];
};

#define VIPX_IOC_LOAD_KERNEL_BINARY	\
	_IOWR('V', 0, struct vipx_ioc_load_kernel_binary)
#define VIPX_IOC_UNLOAD_KERNEL_BINARY	\
	_IOWR('V', 1, struct vipx_ioc_unload_kernel_binary)
#define VIPX_IOC_LOAD_GRAPH_INFO	\
	_IOWR('V', 2, struct vipx_ioc_load_graph_info)
#define VIPX_IOC_UNLOAD_GRAPH_INFO	\
	_IOWR('V', 3, struct vipx_ioc_unload_graph_info)
#define VIPX_IOC_EXECUTE_SUBMODEL	\
	_IOWR('V', 4, struct vipx_ioc_execute_submodel)

union vipx_ioc_arg {
	struct vipx_ioc_load_kernel_binary	kernel_bin;
	struct vipx_ioc_unload_kernel_binary	unload_kbin;
	struct vipx_ioc_load_graph_info		load_ginfo;
	struct vipx_ioc_unload_graph_info	unload_ginfo;
	struct vipx_ioc_execute_submodel	exec;
};

struct vipx_ioctl_ops {
	int (*load_kernel_binary)(struct vipx_context *vctx,
			struct vipx_ioc_load_kernel_binary *args);
	int (*unload_kernel_binary)(struct vipx_context *vctx,
			struct vipx_ioc_unload_kernel_binary *args);
	int (*load_graph_info)(struct vipx_context *vctx,
			struct vipx_ioc_load_graph_info *args);
	int (*unload_graph_info)(struct vipx_context *vctx,
			struct vipx_ioc_unload_graph_info *args);
	int (*execute_submodel)(struct vipx_context *vctx,
			struct vipx_ioc_execute_submodel *args);
};

long vipx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#if defined(CONFIG_COMPAT)
long vipx_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#else
static inline long vipx_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg) { return 0; };
#endif

#endif
