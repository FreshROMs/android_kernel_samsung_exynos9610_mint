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
#include "vipx-common-type.h"
#include "vipx-kernel-binary.h"

int vipx_kernel_binary_set_gmodel(struct vipx_context *vctx,
		struct vipx_graph_model *gmodel)
{
	struct vipx_kernel_binary *kbin, *temp, *gbin, *temp2;
	unsigned int kid, gid;
	int kfd, gfd;
	size_t ksize, gsize;
	bool is_duplicate;

	vipx_enter();
	gid = GET_COMMON_GRAPH_MODEL_ID(gmodel->id);
	list_for_each_entry_safe(kbin, temp, &vctx->binary_list, clist) {
		kid = GET_COMMON_GRAPH_MODEL_ID(kbin->global_id);
		if (gid == kid) {
			kfd = kbin->buffer.m.fd;
			ksize = kbin->buffer.size;
			is_duplicate = false;
			list_for_each_entry_safe(gbin, temp2,
					&gmodel->kbin_list, glist) {
				gfd = gbin->buffer.m.fd;
				gsize = gbin->buffer.size;
				if ((kfd == gfd) && (ksize == gsize)) {
					is_duplicate = true;
					break;
				}
			}
			if (!is_duplicate) {
				//TODO check if list cleanup is needed
				list_add_tail(&kbin->glist, &gmodel->kbin_list);
				gmodel->kbin_count++;
			}
		}
	}
	vipx_leave();
	return 0;
}

static int __vipx_kernel_binary_check(struct vipx_context *vctx,
		unsigned int id, int fd, unsigned int size)
{
	unsigned int model_id, kid;
	struct vipx_kernel_binary *kbin, *temp;

	model_id = GET_COMMON_GRAPH_MODEL_ID(id);
	list_for_each_entry_safe(kbin, temp, &vctx->binary_list, clist) {
		kid = GET_COMMON_GRAPH_MODEL_ID(kbin->global_id);
		if ((model_id == kid) &&
				(fd == kbin->buffer.m.fd) &&
				(size == kbin->buffer.size))
			return true;
	}
	return false;
}

int vipx_kernel_binary_add(struct vipx_context *vctx, unsigned int id,
		int fd, unsigned int size)
{
	int ret;
	struct vipx_kernel_binary *kbin;
	struct vipx_memory *mem;
	unsigned long flags;

	vipx_enter();
	ret = __vipx_kernel_binary_check(vctx, id, fd, size);
	if (ret) {
		vipx_leave();
		return 0;
	}

	kbin = kzalloc(sizeof(*kbin), GFP_KERNEL);
	if (!kbin) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc kernel binary\n");
		goto p_err;
	}

	kbin->global_id = id;
	kbin->buffer.m.fd = fd;
	kbin->buffer.size = size;

	mem = &vctx->core->system->memory;
	ret = mem->mops->map_dmabuf(mem, &kbin->buffer);
	if (ret)
		goto p_err_map;

	spin_lock_irqsave(&vctx->binary_slock, flags);
	vctx->binary_count++;
	list_add_tail(&kbin->clist, &vctx->binary_list);
	spin_unlock_irqrestore(&vctx->binary_slock, flags);

	kbin->vctx = vctx;
	vipx_leave();
	return 0;
p_err_map:
	kfree(kbin);
p_err:
	return ret;
}

int vipx_kernel_binary_unload(struct vipx_context *vctx, unsigned int id,
	int fd, unsigned int size)
{
	unsigned int model_id, kid;
	struct vipx_kernel_binary *kbin, *temp;
	bool found;

	vipx_enter();
	found = false;
	model_id = GET_COMMON_GRAPH_MODEL_ID(id);
	list_for_each_entry_safe(kbin, temp, &vctx->binary_list, clist) {
		kid = GET_COMMON_GRAPH_MODEL_ID(kbin->global_id);
		if ((model_id == kid) &&
			(fd == kbin->buffer.m.fd) &&
			(size == kbin->buffer.size)) {
			vipx_kernel_binary_remove(kbin);
			found = true;
			break;
		}
	}
	if (!found) {
		vipx_err("There is no kernel binary to unload\n");
		vipx_leave();
		return -ENOENT;
	}
	vipx_leave();
	return 0;
}

void vipx_kernel_binary_remove(struct vipx_kernel_binary *kbin)
{
	struct vipx_context *vctx;
	unsigned long flags;
	struct vipx_memory *mem;

	vipx_enter();
	vctx = kbin->vctx;

	spin_lock_irqsave(&vctx->binary_slock, flags);
	list_del(&kbin->clist);
	vctx->binary_count--;
	spin_unlock_irqrestore(&vctx->binary_slock, flags);

	mem = &vctx->core->system->memory;
	mem->mops->unmap_dmabuf(mem, &kbin->buffer);

	kfree(kbin);

	vipx_leave();
}

void vipx_kernel_binary_all_remove(struct vipx_context *vctx)
{
	struct vipx_kernel_binary *kbin, *temp;

	vipx_enter();
	list_for_each_entry_safe(kbin, temp, &vctx->binary_list, clist) {
		vipx_kernel_binary_remove(kbin);
	}
	vipx_leave();
}
