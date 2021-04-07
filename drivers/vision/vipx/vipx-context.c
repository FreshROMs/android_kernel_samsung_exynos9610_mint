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
#include "vipx-util.h"
#include "vipx-kernel-binary.h"
#include "vipx-memory.h"
#include "vipx-context.h"

static struct vipx_buffer *__vipx_context_create_buffer(
		struct vipx_context *vctx,
		struct vipx_common_mem *common_mem,
		enum dma_data_direction dir)
{
	int ret;
	struct vipx_buffer *buf;
	struct vipx_memory *mem;

	vipx_enter();

	if (unlikely(common_mem->size == 0)) {
		ret = -EINVAL;
		vipx_err("memory must not be zero\n");
		goto p_err_size;
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc buffer\n");
		goto p_err_alloc;
	}

	mem = &vctx->core->system->memory;

	buf->m.fd = common_mem->fd;
	buf->size = common_mem->size;
	buf->addr_type = common_mem->addr_type;
	buf->mem_attr = common_mem->mem_attr;
	buf->mem_type = common_mem->mem_type;
	buf->offset = common_mem->offset;
	buf->direction = dir;

	ret = mem->mops->map_dmabuf(mem, buf);
	if (unlikely(ret))
		goto p_err_map;

	if (buf->mem_attr == VIPX_COMMON_CACHEABLE) {
		ret = mem->mops->sync_for_device(mem, buf);
		if (ret)
			goto p_err_sync;
	}

	common_mem->iova = (unsigned int)buf->dvaddr + buf->offset;

	vipx_leave();
	return buf;
p_err_sync:
	mem->mops->unmap_dmabuf(mem, buf);
p_err_map:
	kfree(buf);
p_err_alloc:
p_err_size:
	return ERR_PTR(ret);
}

static void __vipx_context_destroy_buffer(struct vipx_context *vctx,
		struct vipx_buffer *buf)
{
	struct vipx_memory *mem;

	vipx_enter();
	if (!buf)
		return;

	mem = &vctx->core->system->memory;
	if (buf->mem_attr == VIPX_COMMON_CACHEABLE)
		mem->mops->sync_for_cpu(mem, buf);

	mem->mops->unmap_dmabuf(mem, buf);
	kfree(buf);
	vipx_leave();
}

static int vipx_context_load_kernel_binary(struct vipx_context *vctx,
		struct vipx_ioc_load_kernel_binary *kernel_bin)
{
	int ret;

	vipx_enter();
	vipx_dbg("[%s] load kernel binary (framework)\n", __func__);
	vipx_dbg("global_id   : %#x\n", kernel_bin->global_id);
	vipx_dbg("kernel_fd   : %#x\n", kernel_bin->kernel_fd);
	vipx_dbg("kernel_size : %#x\n", kernel_bin->kernel_size);

	ret = vipx_kernel_binary_add(vctx, kernel_bin->global_id,
			kernel_bin->kernel_fd, kernel_bin->kernel_size);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_context_unload_kernel_binary(struct vipx_context *vctx,
		struct vipx_ioc_unload_kernel_binary *unload_kbin)
{
	int ret;

	vipx_enter();
	vipx_dbg("[%s] unload kernel binary (framework)\n", __func__);
	vipx_dbg("model_id    : %#x\n", unload_kbin->global_id);

	ret = vipx_kernel_binary_unload(vctx, unload_kbin->global_id,
			unload_kbin->kernel_fd, unload_kbin->kernel_size);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_context_load_graph_info(struct vipx_context *vctx,
		struct vipx_ioc_load_graph_info *ginfo)
{
	int ret;
	struct vipx_common_graph_info *common_ginfo;
	struct vipx_graph_model *gmodel;
	struct vipx_common_mem *common_mem;
	struct vipx_buffer *buffer;
	int idx;
	int *debug_addr;

	vipx_enter();
	common_ginfo = &ginfo->graph_info;

	vipx_dbg("[%s] load graph (framework)\n", __func__);
	debug_addr = (int *)common_ginfo;
	for (idx = 0; idx < sizeof(*common_ginfo) >> 2; ++idx)
		vipx_dbg("[%3d] %#10x\n", idx, debug_addr[idx]);

	gmodel = vctx->graph_ops->create_model(vctx->graph, common_ginfo);
	if (IS_ERR(gmodel)) {
		ret = PTR_ERR(gmodel);
		goto p_err_gmodel;
	}

	common_mem = &gmodel->common_ginfo.graph;
	buffer = __vipx_context_create_buffer(vctx, common_mem, DMA_TO_DEVICE);
	if (IS_ERR(buffer)) {
		ret = PTR_ERR(buffer);
		vipx_err("Failed to create buffer for graph (%d)\n", ret);
		goto p_err_graph;
	}
	gmodel->graph = buffer;

	common_mem = &gmodel->common_ginfo.temp_buf;
	if (common_mem->size) {
		buffer = __vipx_context_create_buffer(vctx, common_mem,
				DMA_FROM_DEVICE);
		if (IS_ERR(buffer)) {
			ret = PTR_ERR(buffer);
			vipx_err("Failed to create buffer for temp_buf (%d)\n",
					ret);
			goto p_err_temp_buf;
		}
		gmodel->temp_buf = buffer;
	} else {
		gmodel->temp_buf = NULL;
	}

	common_mem = &gmodel->common_ginfo.weight;
	if (common_mem->size) {
		buffer = __vipx_context_create_buffer(vctx, common_mem,
				DMA_TO_DEVICE);
		if (IS_ERR(buffer)) {
			ret = PTR_ERR(buffer);
			vipx_err("Failed to create buffer for weight (%d)\n",
					ret);
			goto p_err_weight;
		}
		gmodel->weight = buffer;
	} else {
		gmodel->weight = NULL;
	}

	common_mem = &gmodel->common_ginfo.bias;
	if (common_mem->size) {
		buffer = __vipx_context_create_buffer(vctx, common_mem,
				DMA_TO_DEVICE);
		if (IS_ERR(buffer)) {
			ret = PTR_ERR(buffer);
			vipx_err("Failed to create buffer for bias (%d)\n",
					ret);
			goto p_err_bias;
		}
		gmodel->bias = buffer;
	} else {
		gmodel->bias = NULL;
	}

	common_mem = &gmodel->common_ginfo.user_param_buffer;
	if (common_mem->size) {
		buffer = __vipx_context_create_buffer(vctx, common_mem,
				DMA_TO_DEVICE);
		if (IS_ERR(buffer)) {
			ret = PTR_ERR(buffer);
			vipx_err("Failed to create user_param_buffer (%d)\n",
					ret);
			goto p_err_user_param_buffer;
		}
		gmodel->user_param_buffer = buffer;
	} else {
		gmodel->user_param_buffer = NULL;
	}

	ret = vctx->graph_ops->register_model(vctx->graph, gmodel);
	if (ret)
		goto p_err_register_model;

	ginfo->timestamp[0] = gmodel->time[TIME_LOAD_GRAPH].start;
	ginfo->timestamp[1] = gmodel->time[TIME_LOAD_GRAPH].end;

	vipx_leave();
	return 0;
p_err_register_model:
	__vipx_context_destroy_buffer(vctx, gmodel->user_param_buffer);
p_err_user_param_buffer:
	__vipx_context_destroy_buffer(vctx, gmodel->bias);
p_err_bias:
	__vipx_context_destroy_buffer(vctx, gmodel->weight);
p_err_weight:
	__vipx_context_destroy_buffer(vctx, gmodel->temp_buf);
p_err_temp_buf:
	__vipx_context_destroy_buffer(vctx, gmodel->graph);
p_err_graph:
	vctx->graph_ops->destroy_model(vctx->graph, gmodel);
p_err_gmodel:
	return ret;
}

static int vipx_context_unload_graph_info(struct vipx_context *vctx,
		struct vipx_ioc_unload_graph_info *ginfo)
{
	int ret;
	struct vipx_graph_model *gmodel;

	vipx_enter();
	vipx_dbg("[%s] unload graph (framework)\n", __func__);
	vipx_dbg("graph_id : %#x\n", ginfo->graph_id);

	gmodel = vctx->graph_ops->get_model(vctx->graph, ginfo->graph_id);
	if (IS_ERR(gmodel)) {
		ret = PTR_ERR(gmodel);
		goto p_err_get_model;
	}

	vctx->graph_ops->unregister_model(vctx->graph, gmodel);

	ginfo->timestamp[0] = gmodel->time[TIME_UNLOAD_GRAPH].start;
	ginfo->timestamp[1] = gmodel->time[TIME_UNLOAD_GRAPH].end;

	__vipx_context_destroy_buffer(vctx, gmodel->user_param_buffer);
	__vipx_context_destroy_buffer(vctx, gmodel->bias);
	__vipx_context_destroy_buffer(vctx, gmodel->weight);
	__vipx_context_destroy_buffer(vctx, gmodel->temp_buf);
	__vipx_context_destroy_buffer(vctx, gmodel->graph);

	vctx->graph_ops->destroy_model(vctx->graph, gmodel);

	vipx_leave();
	return 0;
p_err_get_model:
	return ret;
}

static int vipx_context_execute_submodel(struct vipx_context *vctx,
		struct vipx_ioc_execute_submodel *execute)
{
	int ret;
	int num_input, num_output;
	struct vipx_common_execute_info *execute_info;
	struct vipx_graph_model *gmodel;
	struct vipx_common_mem *common_mem;
	struct vipx_buffer **buffer;
	int buffer_count = 0, translate_count = 0;
	int idx;
	int *debug_addr;

	vipx_enter();
	execute_info = &execute->execute_info;

	vipx_dbg("[%s] execute (framework)\n", __func__);
	debug_addr = (int *)execute_info;
	for (idx = 0; idx < sizeof(*execute_info) >> 2; ++idx)
		vipx_dbg("[%3d] %#10x\n", idx, debug_addr[idx]);

	num_input = execute_info->num_input;
	if (unlikely(num_input > MAX_INPUT_NUM)) {
		ret = -EINVAL;
		vipx_err("num_input[%d] is more than MAX[%d]\n", num_input,
				MAX_INPUT_NUM);
		goto p_err_num;
	}
	//TODO check plane count of buffer
	buffer_count += num_input;

	num_output = execute_info->num_output;
	if (unlikely(num_output > MAX_OUTPUT_NUM)) {
		ret = -EINVAL;
		vipx_err("num_output[%d] is more than MAX[%d]\n", num_output,
				MAX_INPUT_NUM);
		goto p_err_num;
	}
	//TODO check plane count of buffer
	buffer_count += num_output;

	if (execute_info->user_param_buffer.size)
		buffer_count++;

	gmodel = vctx->graph_ops->get_model(vctx->graph, execute_info->gid);
	if (unlikely(IS_ERR(gmodel))) {
		ret = PTR_ERR(gmodel);
		goto p_err_get_model;
	}

	vipx_kernel_binary_set_gmodel(vctx, gmodel);
	if (!gmodel->kbin_count) {
		ret = -ENODATA;
		vipx_err("No kernel binary to set at graph model(%#x)\n",
				gmodel->id);
		goto p_err_set_model;
	}

	buffer = kmalloc(sizeof(*buffer) * buffer_count, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc buffer for in/out (%d)\n",
				buffer_count);
		goto p_err_alloc;
	}

	//TODO check plane count of buffer
	for (idx = 0; idx < num_input; ++idx) {
		common_mem = &execute_info->input[idx];
		buffer[translate_count] = __vipx_context_create_buffer(vctx,
				common_mem, DMA_TO_DEVICE);
		if (unlikely(IS_ERR(buffer[translate_count]))) {
			ret = PTR_ERR(buffer[translate_count]);
			vipx_err("Failed to translate input(%d/%d,%d/%d,%d)\n",
					idx, num_input,
					translate_count, buffer_count, ret);
			goto p_err_buffer;
		}
		translate_count++;
	}

	//TODO check plane count of buffer
	for (idx = 0; idx < num_output; ++idx) {
		common_mem = &execute_info->output[idx];
		buffer[translate_count] = __vipx_context_create_buffer(vctx,
				common_mem, DMA_FROM_DEVICE);
		if (unlikely(IS_ERR(buffer[translate_count]))) {
			ret = PTR_ERR(buffer[translate_count]);
			vipx_err("Failed to translate output(%d/%d,%d/%d,%d)\n",
					idx, num_output,
					translate_count, buffer_count, ret);
			goto p_err_buffer;
		}
		translate_count++;
	}

	if (execute_info->user_param_buffer.size) {
		common_mem = &execute_info->user_param_buffer;
		buffer[translate_count] = __vipx_context_create_buffer(vctx,
				common_mem, DMA_TO_DEVICE);
		if (unlikely(IS_ERR(buffer[translate_count]))) {
			ret = PTR_ERR(buffer[translate_count]);
			vipx_err("Failed to translate user_param(%d/%d,%d)\n",
					translate_count, buffer_count, ret);
			goto p_err_buffer;
		}
		translate_count++;
	}

	ret = vctx->graph_ops->execute_model(vctx->graph, gmodel, execute_info);
	if (ret)
		goto p_err_execute;

	execute->timestamp[0] = gmodel->time[TIME_EXECUTE_GRAPH].start;
	execute->timestamp[1] = gmodel->time[TIME_EXECUTE_GRAPH].end;

	for (idx = 0; idx < translate_count; ++idx)
		__vipx_context_destroy_buffer(vctx, buffer[idx]);

	kfree(buffer);
	vipx_leave();
	return 0;
p_err_execute:
p_err_buffer:
	for (idx = 0; idx < translate_count; ++idx)
		__vipx_context_destroy_buffer(vctx, buffer[idx]);

	kfree(buffer);
p_err_alloc:
p_err_set_model:
p_err_get_model:
p_err_num:
	return ret;
}

static const struct vipx_context_ops vipx_context_ops = {
	.load_kernel_binary	= vipx_context_load_kernel_binary,
	.unload_kernel_binary	= vipx_context_unload_kernel_binary,
	.load_graph_info	= vipx_context_load_graph_info,
	.unload_graph_info	= vipx_context_unload_graph_info,
	.execute_submodel	= vipx_context_execute_submodel
};

struct vipx_context *vipx_context_create(struct vipx_core *core)
{
	int ret;
	struct vipx_context *vctx;

	vipx_enter();
	vctx = kzalloc(sizeof(*vctx), GFP_KERNEL);
	if (!vctx) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc context\n");
		goto p_err;
	}
	vctx->core = core;

	vctx->idx = vipx_util_bitmap_get_zero_bit(core->vctx_map,
			VIPX_MAX_CONTEXT);
	if (vctx->idx == VIPX_MAX_CONTEXT) {
		ret = -ENOMEM;
		vipx_err("Failed to get idx of context\n");
		goto p_err_bitmap;
	}

	core->vctx_count++;
	list_add_tail(&vctx->list, &core->vctx_list);

	mutex_init(&vctx->lock);

	vctx->vops = &vipx_context_ops;
	vctx->binary_count = 0;
	INIT_LIST_HEAD(&vctx->binary_list);
	spin_lock_init(&vctx->binary_slock);

	vipx_queue_init(vctx);

	vctx->state = BIT(VIPX_CONTEXT_OPEN);
	vipx_leave();
	return vctx;
p_err_bitmap:
	kfree(vctx);
p_err:
	return ERR_PTR(ret);
}

void vipx_context_destroy(struct vipx_context *vctx)
{
	struct vipx_core *core;

	vipx_enter();
	core = vctx->core;

	if (vctx->binary_count)
		vipx_kernel_binary_all_remove(vctx);

	mutex_destroy(&vctx->lock);
	list_del(&vctx->list);
	core->vctx_count--;
	vipx_util_bitmap_clear_bit(core->vctx_map, vctx->idx);
	kfree(vctx);
	vipx_leave();
}
