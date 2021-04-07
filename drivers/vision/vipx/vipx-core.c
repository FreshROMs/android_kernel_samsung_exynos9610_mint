/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vipx-log.h"
#include "vipx-util.h"
#include "vipx-ioctl.h"
#include "vipx-device.h"
#include "vipx-graph.h"
#include "vipx-context.h"
#include "vipx-core.h"

static int vipx_core_load_kernel_binary(struct vipx_context *vctx,
		struct vipx_ioc_load_kernel_binary *args)
{
	int ret;

	vipx_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vipx_err("Failed to lock for loading kernel binary (%d)\n",
				ret);
		goto p_err_lock;
	}

	ret = vctx->vops->load_kernel_binary(vctx, args);
	if (ret)
		goto p_err_vops;

	mutex_unlock(&vctx->lock);
	args->ret = 0;
	vipx_leave();
	return 0;
p_err_vops:
	mutex_unlock(&vctx->lock);
p_err_lock:
	args->ret = ret;
	vipx_err("Failed to load kernel binary(%d)\n", ret);
	/* return value is included in args->ret */
	return 0;
}

static int vipx_core_unload_kernel_binary(struct vipx_context *vctx,
		struct vipx_ioc_unload_kernel_binary *args)
{
	int ret;

	vipx_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vipx_err("Failed to lock for loading kernel binary (%d)\n",
				ret);
		goto p_err_lock;
	}

	ret = vctx->vops->unload_kernel_binary(vctx, args);
	if (ret)
		goto p_err_vops;

	mutex_unlock(&vctx->lock);
	args->ret = 0;
	vipx_leave();
	return 0;
p_err_vops:
	mutex_unlock(&vctx->lock);
p_err_lock:
	args->ret = ret;
	vipx_err("Failed to unload kernel binary(%d)\n", ret);
	/* return value is included in args->ret */
	return 0;
}

static int vipx_core_load_graph_info(struct vipx_context *vctx,
		struct vipx_ioc_load_graph_info *args)
{
	int ret;

	vipx_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vipx_err("Failed to lock for loadding graph info (%d)\n", ret);
		goto p_err_lock;
	}

	ret = vctx->vops->load_graph_info(vctx, args);
	if (ret)
		goto p_err_vops;

	mutex_unlock(&vctx->lock);
	args->ret = 0;
	vipx_leave();
	return 0;
p_err_vops:
	mutex_unlock(&vctx->lock);
p_err_lock:
	args->ret = ret;
	vipx_err("Failed to load graph(%d)\n", ret);
	/* return value is included in args->ret */
	return 0;
}

static int vipx_core_unload_graph_info(struct vipx_context *vctx,
		struct vipx_ioc_unload_graph_info *args)
{
	int ret;

	vipx_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vipx_err("Failed to lock for unloading graph info (%d)\n", ret);
		goto p_err_lock;
	}

	ret = vctx->vops->unload_graph_info(vctx, args);
	if (ret)
		goto p_err_vops;

	mutex_unlock(&vctx->lock);
	args->ret = 0;
	vipx_leave();
	return 0;
p_err_vops:
	mutex_unlock(&vctx->lock);
p_err_lock:
	args->ret = ret;
	vipx_err("Failed to unload graph(%d)\n", ret);
	/* return value is included in args->ret */
	return 0;
}

static int vipx_core_execute_submodel(struct vipx_context *vctx,
		struct vipx_ioc_execute_submodel *args)
{
	int ret;

	vipx_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vipx_err("Failed to lock for executing submodel (%d)\n", ret);
		goto p_err_lock;
	}

	ret = vctx->vops->execute_submodel(vctx, args);
	if (ret)
		goto p_err_vops;

	mutex_unlock(&vctx->lock);
	args->ret = 0;
	vipx_leave();
	return 0;
p_err_vops:
	mutex_unlock(&vctx->lock);
p_err_lock:
	args->ret = ret;
	vipx_err("Failed to execute submodel(%d)\n", ret);
	/* return value is included in args->ret */
	return 0;
}

const struct vipx_ioctl_ops vipx_core_ioctl_ops = {
	.load_kernel_binary	= vipx_core_load_kernel_binary,
	.unload_kernel_binary	= vipx_core_unload_kernel_binary,
	.load_graph_info	= vipx_core_load_graph_info,
	.unload_graph_info	= vipx_core_unload_graph_info,
	.execute_submodel	= vipx_core_execute_submodel,
};

static int vipx_open(struct inode *inode, struct file *file)
{
	int ret;
	struct miscdevice *miscdev;
	struct vipx_device *vdev;
	struct vipx_core *core;
	struct vipx_context *vctx;
	struct vipx_graph *graph;

	vipx_enter();
	miscdev = file->private_data;
	vdev = dev_get_drvdata(miscdev->parent);
	core = &vdev->core;

	if (mutex_lock_interruptible(&core->lock)) {
		ret = -ERESTARTSYS;
		vipx_err("Failed to lock device lock for open (%d)\n", ret);
		goto p_err_lock;
	}

	ret = vipx_device_open(vdev);
	if (ret)
		goto p_err_device_open;

	ret = vipx_device_start(vdev);
	if (ret)
		goto p_err_device_start;

	vctx = vipx_context_create(core);
	if (IS_ERR(vctx)) {
		ret = PTR_ERR(vctx);
		goto p_err_vctx;
	}

	graph = vipx_graph_create(vctx, &core->system->graphmgr);
	if (IS_ERR(graph)) {
		ret = PTR_ERR(graph);
		goto p_err_graph;
	}
	vctx->graph = graph;

	file->private_data = vctx;

	mutex_unlock(&core->lock);

	vipx_info("The vipx has been successfully opened(count:%d)\n",
			vdev->open_count);

	vipx_leave();
	return 0;
p_err_graph:
p_err_vctx:
	vipx_device_stop(vdev);
p_err_device_start:
	vipx_device_close(vdev);
p_err_device_open:
	mutex_unlock(&core->lock);
	vipx_err("Failed to open the vipx(count:%d)(%d)\n",
			vdev->open_count, ret);
p_err_lock:
	return ret;
}

static int vipx_release(struct inode *inode, struct file *file)
{
	int ret;
	struct vipx_context *vctx;
	struct vipx_core *core;

	vipx_enter();
	vctx = file->private_data;
	core = vctx->core;

	if (mutex_lock_interruptible(&core->lock)) {
		ret = -ERESTARTSYS;
		vipx_err("Failed to lock device lock for release (%d)\n", ret);
		return ret;
	}

	vipx_graph_destroy(vctx->graph);
	vipx_context_destroy(vctx);
	vipx_device_stop(core->device);
	vipx_device_close(core->device);

	mutex_unlock(&core->lock);
	vipx_info("The vipx has been closed(count:%d)\n",
			core->device->open_count);
	vipx_leave();
	return 0;
}

static unsigned int vipx_poll(struct file *file, struct poll_table_struct *poll)
{
	int ret;
	struct vipx_context *vctx;

	vipx_enter();
	vctx = file->private_data;
	if (!(vctx->state & BIT(VIPX_CONTEXT_START))) {
		ret = POLLERR;
		vipx_err("Context state(%x) is not start (%d)\n",
				vctx->state, ret);
		goto p_err;
	}

	ret = vctx->queue_ops->poll(&vctx->queue_list, file, poll);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

const struct file_operations vipx_file_ops = {
	.owner		= THIS_MODULE,
	.open		= vipx_open,
	.release	= vipx_release,
	.poll		= vipx_poll,
	.unlocked_ioctl	= vipx_ioctl,
	.compat_ioctl	= vipx_compat_ioctl
};

/* Top-level data for debugging */
static struct vipx_miscdev *misc_vdev;

int vipx_core_probe(struct vipx_device *vdev)
{
	int ret;
	struct vipx_core *core;

	vipx_enter();
	core = &vdev->core;
	core->device = vdev;
	core->system = &vdev->system;
	misc_vdev = &core->misc_vdev;

	mutex_init(&core->lock);
	core->ioc_ops = &vipx_core_ioctl_ops;

	vipx_util_bitmap_init(core->vctx_map, VIPX_MAX_CONTEXT);
	INIT_LIST_HEAD(&core->vctx_list);
	core->vctx_count = 0;

	core->misc_vdev.miscdev.minor = MISC_DYNAMIC_MINOR;
	core->misc_vdev.miscdev.name = VIPX_DEV_NAME;
	core->misc_vdev.miscdev.fops = &vipx_file_ops;
	core->misc_vdev.miscdev.parent = vdev->dev;

	ret = misc_register(&core->misc_vdev.miscdev);
	if (ret) {
		vipx_err("miscdevice is not registered (%d)\n", ret);
		goto p_err_misc;
	}

	vipx_leave();
	return 0;
p_err_misc:
	mutex_destroy(&core->lock);
	return ret;
}

void vipx_core_remove(struct vipx_core *core)
{
	vipx_enter();
	misc_deregister(&core->misc_vdev.miscdev);
	mutex_destroy(&core->lock);
	vipx_leave();
}
