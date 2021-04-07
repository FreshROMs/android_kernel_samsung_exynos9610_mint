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

static int __vipx_ioctl_get_load_kernel_binary(
		struct vipx_ioc_load_kernel_binary *karg,
		struct vipx_ioc_load_kernel_binary __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_from_user(karg, uarg, sizeof(*uarg));
	if (ret) {
		vipx_err("Copy failed [Load kernel binary] (%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_load_kernel_binary(
		struct vipx_ioc_load_kernel_binary *karg,
		struct vipx_ioc_load_kernel_binary __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		vipx_err("Copy failed to user [Load kernel binary]\n");

	vipx_leave();
}

static int __vipx_ioctl_get_unload_kernel_binary(
		struct vipx_ioc_unload_kernel_binary *karg,
		struct vipx_ioc_unload_kernel_binary __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_from_user(karg, uarg, sizeof(*uarg));
	if (ret) {
		vipx_err("Copy failed [Unload kernel binary] (%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_unload_kernel_binary(
		struct vipx_ioc_unload_kernel_binary *karg,
		struct vipx_ioc_unload_kernel_binary __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		vipx_err("Copy failed to user [Unload kernel binary]\n");

	vipx_leave();
}

static int __vipx_ioctl_get_load_graph_info(
		struct vipx_ioc_load_graph_info *karg,
		struct vipx_ioc_load_graph_info __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_from_user(karg, uarg, sizeof(*uarg));
	if (ret) {
		vipx_err("Copy failed [Load graph info] (%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_load_graph_info(
		struct vipx_ioc_load_graph_info *karg,
		struct vipx_ioc_load_graph_info __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		vipx_err("Copy failed to user [Load graph info]\n");

	vipx_leave();
}

static int __vipx_ioctl_get_unload_graph_info(
		struct vipx_ioc_unload_graph_info *karg,
		struct vipx_ioc_unload_graph_info __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_from_user(karg, uarg, sizeof(*uarg));
	if (ret) {
		vipx_err("Copy failed [Unload graph info] (%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_unload_graph_info(
		struct vipx_ioc_unload_graph_info *karg,
		struct vipx_ioc_unload_graph_info __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		vipx_err("Copy failed to user [Unload graph info]\n");

	vipx_leave();
}

static int __vipx_ioctl_get_execute_submodel(
		struct vipx_ioc_execute_submodel *karg,
		struct vipx_ioc_execute_submodel __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_from_user(karg, uarg, sizeof(*uarg));
	if (ret) {
		vipx_err("Copy failed [Execute submodel] (%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_execute_submodel(
		struct vipx_ioc_execute_submodel *karg,
		struct vipx_ioc_execute_submodel __user *uarg)
{
	int ret;

	vipx_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		vipx_err("Copy failed to user [Execute submodel]\n");

	vipx_leave();
}

long vipx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct vipx_context *vctx;
	const struct vipx_ioctl_ops *ops;
	union vipx_ioc_arg karg;
	void __user *uarg;

	vipx_enter();
	vctx = file->private_data;
	ops = vctx->core->ioc_ops;
	uarg = (void __user *)arg;

	switch (cmd) {
	case VIPX_IOC_LOAD_KERNEL_BINARY:
		ret = __vipx_ioctl_get_load_kernel_binary(&karg.kernel_bin,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->load_kernel_binary(vctx, &karg.kernel_bin);
		__vipx_ioctl_put_load_kernel_binary(&karg.kernel_bin, uarg);
		break;
	case VIPX_IOC_UNLOAD_KERNEL_BINARY:
		ret = __vipx_ioctl_get_unload_kernel_binary(&karg.unload_kbin,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->unload_kernel_binary(vctx, &karg.unload_kbin);
		__vipx_ioctl_put_unload_kernel_binary(&karg.unload_kbin, uarg);
		break;
	case VIPX_IOC_LOAD_GRAPH_INFO:
		ret = __vipx_ioctl_get_load_graph_info(&karg.load_ginfo,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->load_graph_info(vctx, &karg.load_ginfo);
		__vipx_ioctl_put_load_graph_info(&karg.load_ginfo, uarg);
		break;
	case VIPX_IOC_UNLOAD_GRAPH_INFO:
		ret = __vipx_ioctl_get_unload_graph_info(&karg.unload_ginfo,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->unload_graph_info(vctx, &karg.unload_ginfo);
		__vipx_ioctl_put_unload_graph_info(&karg.unload_ginfo, uarg);
		break;
	case VIPX_IOC_EXECUTE_SUBMODEL:
		ret = __vipx_ioctl_get_execute_submodel(&karg.exec, uarg);
		if (ret)
			goto p_err;

		ret = ops->execute_submodel(vctx, &karg.exec);
		__vipx_ioctl_put_execute_submodel(&karg.exec, uarg);
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
