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

#include "vipx-log.h"
#include "vipx-core.h"
#include "vipx-context.h"
#include "vipx-ioctl.h"

#define VS4L_VERTEXIOC_S_GRAPH32	\
	_IOW('V', 0, struct vs4l_graph32)
#define VS4L_VERTEXIOC_S_FORMAT32	\
	_IOW('V', 1, struct vs4l_format_list32)
#define VS4L_VERTEXIOC_S_PARAM32	\
	_IOW('V', 2, struct vs4l_param_list32)
#define VS4L_VERTEXIOC_S_CTRL32		\
	_IOW('V', 3, struct vs4l_ctrl32)
#define VS4L_VERTEXIOC_QBUF32		\
	_IOW('V', 6, struct vs4l_container_list32)
#define VS4L_VERTEXIOC_DQBUF32		\
	_IOW('V', 7, struct vs4l_container_list32)

struct vipx_ioc_load_kernel_binary32 {
	unsigned int			size;
	unsigned int			global_id;
	int				kernel_fd;
	unsigned int			kernel_size;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_unload_kernel_binary32 {
	unsigned int			size;
	unsigned int			global_id;
	int				kernel_fd;
	unsigned int			kernel_size;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_load_graph_info32 {
	unsigned int			size;
	struct vipx_common_graph_info	graph_info;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_unload_graph_info32 {
	unsigned int			size;
	unsigned int			graph_id;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_execute_submodel32 {
	unsigned int			size;
	struct vipx_common_execute_info	execute_info;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

#define VIPX_IOC_LOAD_KERNEL_BINARY32	\
	_IOWR('V', 0, struct vipx_ioc_load_kernel_binary32)
#define VIPX_IOC_UNLOAD_KERNEL_BINARY32	\
	_IOWR('V', 1, struct vipx_ioc_unload_kernel_binary32)
#define VIPX_IOC_LOAD_GRAPH_INFO32	\
	_IOWR('V', 2, struct vipx_ioc_load_graph_info32)
#define VIPX_IOC_UNLOAD_GRAPH_INFO32	\
	_IOWR('V', 3, struct vipx_ioc_unload_graph_info32)
#define VIPX_IOC_EXECUTE_SUBMODEL32	\
	_IOWR('V', 4, struct vipx_ioc_execute_submodel32)

static int __vipx_ioctl_get_load_kernel_binary32(
		struct vipx_ioc_load_kernel_binary *karg,
		struct vipx_ioc_load_kernel_binary32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size) ||
			get_user(karg->global_id, &uarg->global_id) ||
			get_user(karg->kernel_fd, &uarg->kernel_fd) ||
			get_user(karg->kernel_size, &uarg->kernel_size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Load kernel binary(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_load_kernel_binary32(
		struct vipx_ioc_load_kernel_binary *karg,
		struct vipx_ioc_load_kernel_binary32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Load kernel binary(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_unload_kernel_binary32(
		struct vipx_ioc_unload_kernel_binary *karg,
		struct vipx_ioc_unload_kernel_binary32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size) ||
			get_user(karg->global_id, &uarg->global_id) ||
			get_user(karg->kernel_fd, &uarg->kernel_fd) ||
			get_user(karg->kernel_size, &uarg->kernel_size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Unload Kernel Binary(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_unload_kernel_binary32(
		struct vipx_ioc_unload_kernel_binary *karg,
		struct vipx_ioc_unload_kernel_binary32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Unload kernel binary(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_load_graph_info32(
		struct vipx_ioc_load_graph_info *karg,
		struct vipx_ioc_load_graph_info32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Load graph info(32)]\n");
		goto p_err;
	}

	ret = copy_from_user(&karg->graph_info, &uarg->graph_info,
			sizeof(uarg->graph_info));
	if (ret) {
		vipx_err("Copy failed from user [Load graph info(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_load_graph_info32(
		struct vipx_ioc_load_graph_info *karg,
		struct vipx_ioc_load_graph_info32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Load kernel binary(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_unload_graph_info32(
		struct vipx_ioc_unload_graph_info *karg,
		struct vipx_ioc_unload_graph_info32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size) ||
			get_user(karg->graph_id, &uarg->graph_id)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Unload graph info(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_unload_graph_info32(
		struct vipx_ioc_unload_graph_info *karg,
		struct vipx_ioc_unload_graph_info32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Unload graph_info(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_execute_submodel32(
		struct vipx_ioc_execute_submodel *karg,
		struct vipx_ioc_execute_submodel32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Execute submodel(32)]\n");
		goto p_err;
	}

	ret = copy_from_user(&karg->execute_info, &uarg->execute_info,
			sizeof(uarg->execute_info));
	if (ret) {
		vipx_err("Copy failed from user [Execute submodel(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_execute_submodel32(
		struct vipx_ioc_execute_submodel *karg,
		struct vipx_ioc_execute_submodel32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Load kernel binary(32)]\n");
	}
	vipx_leave();
}

long vipx_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct vipx_context *vctx;
	const struct vipx_ioctl_ops *ops;
	union vipx_ioc_arg karg;
	void __user *uarg;

	vipx_enter();
	vctx = file->private_data;
	ops = vctx->core->ioc_ops;
	uarg = compat_ptr(arg);

	switch (cmd) {
	case VIPX_IOC_LOAD_KERNEL_BINARY32:
		ret = __vipx_ioctl_get_load_kernel_binary32(&karg.kernel_bin,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->load_kernel_binary(vctx, &karg.kernel_bin);
		__vipx_ioctl_put_load_kernel_binary32(&karg.kernel_bin, uarg);
		break;
	case VIPX_IOC_UNLOAD_KERNEL_BINARY32:
		ret = __vipx_ioctl_get_unload_kernel_binary32(&karg.unload_kbin,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->unload_kernel_binary(vctx, &karg.unload_kbin);
		__vipx_ioctl_put_unload_kernel_binary32(&karg.unload_kbin,
				uarg);
		break;
	case VIPX_IOC_LOAD_GRAPH_INFO32:
		ret = __vipx_ioctl_get_load_graph_info32(&karg.load_ginfo,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->load_graph_info(vctx, &karg.load_ginfo);
		__vipx_ioctl_put_load_graph_info32(&karg.load_ginfo, uarg);
		break;
	case VIPX_IOC_UNLOAD_GRAPH_INFO32:
		ret = __vipx_ioctl_get_unload_graph_info32(&karg.unload_ginfo,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->unload_graph_info(vctx, &karg.unload_ginfo);
		__vipx_ioctl_put_unload_graph_info32(&karg.unload_ginfo, uarg);
		break;
	case VIPX_IOC_EXECUTE_SUBMODEL32:
		ret = __vipx_ioctl_get_execute_submodel32(&karg.exec, uarg);
		if (ret)
			goto p_err;

		ret = ops->execute_submodel(vctx, &karg.exec);
		__vipx_ioctl_put_execute_submodel32(&karg.exec, uarg);
		break;
	default:
		ret = -EINVAL;
		vipx_err("ioc command(%x) is not supported\n", cmd);
		goto p_err;
	}

	vipx_leave();
p_err:
	return ret;
}
