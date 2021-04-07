/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/ion_exynos.h>

#include "vipx-log.h"
#include "vipx-device.h"
#include "vipx-core.h"
#include "vipx-context.h"
#include "vipx-queue.h"

static struct vipx_format_type vipx_fmts[] = {
	{
		.name		= "RGB",
		.colorspace	= VS4L_DF_IMAGE_RGB,
		.planes		= 1,
		.bitsperpixel	= { 24 }
	}, {
		.name		= "ARGB",
		.colorspace	= VS4L_DF_IMAGE_RGBX,
		.planes		= 1,
		.bitsperpixel	= { 32 }
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.colorspace	= VS4L_DF_IMAGE_NV12,
		.planes		= 2,
		.bitsperpixel	= { 8, 8 }
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.colorspace	= VS4L_DF_IMAGE_NV21,
		.planes		= 2,
		.bitsperpixel	= { 8, 8 }
	}, {
		.name		= "YUV 4:2:0 planar, Y/Cr/Cb",
		.colorspace	= VS4L_DF_IMAGE_YV12,
		.planes		= 3,
		.bitsperpixel	= { 8, 4, 4 }
	}, {
		.name		= "YUV 4:2:0 planar, Y/Cb/Cr",
		.colorspace	= VS4L_DF_IMAGE_I420,
		.planes		= 3,
		.bitsperpixel	= { 8, 4, 4 }
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.colorspace	= VS4L_DF_IMAGE_I422,
		.planes		= 3,
		.bitsperpixel	= { 8, 4, 4 }
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.colorspace	= VS4L_DF_IMAGE_YUYV,
		.planes		= 1,
		.bitsperpixel	= { 16 }
	}, {
		.name		= "YUV 4:4:4 packed, YCbCr",
		.colorspace	= VS4L_DF_IMAGE_YUV4,
		.planes		= 1,
		.bitsperpixel	= { 24 }
	}, {
		.name		= "VX unsigned 8 bit",
		.colorspace	= VS4L_DF_IMAGE_U8,
		.planes		= 1,
		.bitsperpixel	= { 8 }
	}, {
		.name		= "VX unsigned 16 bit",
		.colorspace	= VS4L_DF_IMAGE_U16,
		.planes		= 1,
		.bitsperpixel	= { 16 }
	}, {
		.name		= "VX unsigned 32 bit",
		.colorspace	= VS4L_DF_IMAGE_U32,
		.planes		= 1,
		.bitsperpixel	= { 32 }
	}, {
		.name		= "VX signed 16 bit",
		.colorspace	= VS4L_DF_IMAGE_S16,
		.planes		= 1,
		.bitsperpixel	= { 16 }
	}, {
		.name		= "VX signed 32 bit",
		.colorspace	= VS4L_DF_IMAGE_S32,
		.planes		= 1,
		.bitsperpixel	= { 32 }
	}
};

static struct vipx_format_type *__vipx_queue_find_format(unsigned int format)
{
	int ret;
	unsigned int idx;
	struct vipx_format_type *fmt = NULL;

	vipx_enter();
	for (idx = 0; idx < ARRAY_SIZE(vipx_fmts); ++idx) {
		if (vipx_fmts[idx].colorspace == format) {
			fmt = &vipx_fmts[idx];
			break;
		}
	}

	if (!fmt) {
		ret = -EINVAL;
		vipx_err("Vision format is invalid (%u)\n", format);
		goto p_err;
	}

	vipx_leave();
	return fmt;
p_err:
	return ERR_PTR(ret);
}

static int __vipx_queue_plane_size(struct vipx_buffer_format *format)
{
	int ret;
	unsigned int plane;
	struct vipx_format_type *fmt;
	unsigned int width, height;

	vipx_enter();
	fmt = format->fmt;
	if (fmt->planes > VIPX_MAX_PLANES) {
		ret = -EINVAL;
		vipx_err("planes(%u) is too big (%u)\n",
				fmt->planes, VIPX_MAX_PLANES);
		goto p_err;
	}

	for (plane = 0; plane < fmt->planes; ++plane) {
		width = format->width *
			fmt->bitsperpixel[plane] / BITS_PER_BYTE;
		height = format->height *
			fmt->bitsperpixel[plane] / BITS_PER_BYTE;
		format->size[plane] = width * height;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int __vipx_queue_wait_for_done(struct vipx_queue *queue)
{
	int ret;
	/* TODO: check wait time */
	unsigned long wait_time = 10000;

	vipx_enter();
	if (!queue->streaming) {
		ret = -EINVAL;
		vipx_err("queue is not streaming status\n");
		goto p_err;
	}

	if (!list_empty(&queue->done_list))
		return 0;

	mutex_unlock(queue->lock);

	ret = wait_event_interruptible_timeout(queue->done_wq,
			!list_empty(&queue->done_list) || !queue->streaming,
			msecs_to_jiffies(wait_time));
	if (!ret) {
		ret = -ETIMEDOUT;
		vipx_err("Wait time(%lu ms) is over\n", wait_time);
	} else if (ret > 0) {
		ret = 0;
	} else {
		vipx_err("Waiting has ended abnormaly (%d)\n", ret);
	}

	mutex_lock(queue->lock);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static struct vipx_bundle *__vipx_queue_get_done_bundle(
		struct vipx_queue *queue,
		struct vs4l_container_list *clist)
{
	int ret;
	struct vipx_bundle *bundle;
	unsigned long flags;

	vipx_enter();
	/* Wait for at least one buffer to become available on the done_list */
	ret = __vipx_queue_wait_for_done(queue);
	if (ret)
		goto p_err;

	/*
	 * Driver's lock has been held since we last verified that done_list
	 * is not empty, so no need for another list_empty(done_list) check.
	 */
	spin_lock_irqsave(&queue->done_lock, flags);

	bundle = list_first_entry(&queue->done_list, struct vipx_bundle,
			done_entry);

	list_del(&bundle->done_entry);
	atomic_dec(&queue->done_count);

	spin_unlock_irqrestore(&queue->done_lock, flags);

	vipx_leave();
	return bundle;
p_err:
	return ERR_PTR(ret);
}

static void __vipx_queue_fill_vs4l_buffer(struct vipx_bundle *bundle,
		struct vs4l_container_list *kclist)
{
	struct vipx_container_list *vclist;

	vipx_enter();
	vclist = &bundle->clist;
	kclist->flags &= ~(1 << VS4L_CL_FLAG_TIMESTAMP);
	kclist->flags &= ~(1 << VS4L_CL_FLAG_PREPARE);
	kclist->flags &= ~(1 << VS4L_CL_FLAG_INVALID);
	kclist->flags &= ~(1 << VS4L_CL_FLAG_DONE);

	if (test_bit(VS4L_CL_FLAG_TIMESTAMP, &vclist->flags)) {
		if (sizeof(kclist->timestamp) == sizeof(vclist->timestamp)) {
			kclist->flags |= (1 << VS4L_CL_FLAG_TIMESTAMP);
			memcpy(kclist->timestamp, vclist->timestamp,
					sizeof(vclist->timestamp));
		} else {
			vipx_warn("timestamp of clist is different (%zu/%zu)\n",
					sizeof(kclist->timestamp),
					sizeof(vclist->timestamp));
		}
	}

	if (test_bit(VS4L_CL_FLAG_PREPARE, &bundle->flags))
		kclist->flags |= (1 << VS4L_CL_FLAG_PREPARE);

	if (test_bit(VS4L_CL_FLAG_INVALID, &bundle->flags))
		kclist->flags |= (1 << VS4L_CL_FLAG_INVALID);

	if (test_bit(VS4L_CL_FLAG_DONE, &bundle->flags))
		kclist->flags |= (1 << VS4L_CL_FLAG_DONE);

	kclist->index = vclist->index;
	kclist->id = vclist->id;
	vipx_leave();
}

static int __vipx_queue_map_dmabuf(struct vipx_queue *queue,
		struct vipx_buffer *buf, size_t size)
{
	int ret;
	struct vipx_core *core;
	struct vipx_memory *mem;

	vipx_enter();
	core = queue->qlist->vctx->core;
	mem = &core->system->memory;

	buf->size = size;
	ret = mem->mops->map_dmabuf(mem, buf);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_queue_unmap_dmabuf(struct vipx_queue *queue,
		struct vipx_buffer *buf)
{
	struct vipx_core *core;
	struct vipx_memory *mem;

	vipx_enter();
	core = queue->qlist->vctx->core;
	mem = &core->system->memory;
	mem->mops->unmap_dmabuf(mem, buf);
	vipx_leave();
}

static int __vipx_queue_bundle_prepare(struct vipx_queue *queue,
		struct vipx_bundle *bundle)
{
	int ret;
	struct vipx_container *con;
	struct vipx_buffer *buf;
	struct vipx_buffer_format *fmt;
	unsigned int c_cnt, count, b_cnt;

	vipx_enter();
	if (test_bit(VS4L_CL_FLAG_PREPARE, &bundle->flags))
		return 0;

	con = bundle->clist.containers;
	for (c_cnt = 0; c_cnt < bundle->clist.count; ++c_cnt) {
		switch (con[c_cnt].type) {
		case VS4L_BUFFER_LIST:
		case VS4L_BUFFER_ROI:
		case VS4L_BUFFER_PYRAMID:
			count = con[c_cnt].count;
			break;
		default:
			ret = -EINVAL;
			vipx_err("container type is invalid (%u)\n",
					con[c_cnt].type);
			goto p_err;
		}

		fmt = con[c_cnt].format;
		switch (con[c_cnt].memory) {
		case VS4L_MEMORY_DMABUF:
			buf = con[c_cnt].buffers;
			for (b_cnt = 0; b_cnt < count; ++b_cnt) {
				ret = __vipx_queue_map_dmabuf(queue,
						&buf[b_cnt],
						fmt->size[b_cnt]);
				if (ret)
					goto p_err;
			}
			break;
		default:
			ret = -EINVAL;
			vipx_err("container memory type is invalid (%u)\n",
					con[c_cnt].memory);
			goto p_err;
		}
	}
	set_bit(VS4L_CL_FLAG_PREPARE, &bundle->flags);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int __vipx_queue_bundle_unprepare(struct vipx_queue *queue,
		struct vipx_bundle *bundle)
{
	int ret;
	struct vipx_container *con;
	struct vipx_buffer *buf;
	unsigned int c_cnt, count, b_cnt;

	vipx_enter();
	if (!test_bit(VS4L_CL_FLAG_PREPARE, &bundle->flags))
		goto p_err;

	con = bundle->clist.containers;
	for (c_cnt = 0; c_cnt < bundle->clist.count; ++c_cnt) {
		switch (con[c_cnt].type) {
		case VS4L_BUFFER_LIST:
		case VS4L_BUFFER_ROI:
		case VS4L_BUFFER_PYRAMID:
			count = con[c_cnt].count;
			break;
		default:
			ret = -EINVAL;
			vipx_err("container type is invalid (%u)\n",
					con[c_cnt].type);
			goto p_err;
		}

		switch (con[c_cnt].memory) {
		case VS4L_MEMORY_DMABUF:
			buf = con[c_cnt].buffers;
			for (b_cnt = 0; b_cnt < count; ++b_cnt)
				__vipx_queue_unmap_dmabuf(queue, &buf[b_cnt]);
			break;
		default:
			ret = -EINVAL;
			vipx_err("container memory type is invalid (%u)\n",
					con[c_cnt].memory);
			goto p_err;
		}
	}

	clear_bit(VS4L_CL_FLAG_PREPARE, &bundle->flags);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int __vipx_queue_bundle_check(struct vipx_bundle *bundle,
		struct vs4l_container_list *kclist)
{
	int ret;
	struct vs4l_container *kcon;
	struct vs4l_buffer *kbuf;
	struct vipx_container_list *vclist;
	struct vipx_container *vcon;
	struct vipx_buffer *vbuf;
	unsigned int c_cnt, b_cnt;

	vipx_enter();
	vclist = &bundle->clist;

	if (vclist->direction != kclist->direction) {
		ret = -EINVAL;
		vipx_err("direction of clist is different (%u/%u)\n",
				vclist->direction, kclist->direction);
		goto p_err;
	}

	if (vclist->index != kclist->index) {
		ret = -EINVAL;
		vipx_err("index of clist is different (%u/%u)\n",
				vclist->index, kclist->index);
		goto p_err;
	}

	if (vclist->count != kclist->count) {
		ret = -EINVAL;
		vipx_err("container count of clist is differnet (%u/%u)\n",
				vclist->count, kclist->count);
		goto p_err;
	}

	vclist->id = kclist->id;
	vclist->flags = kclist->flags;

	vcon = vclist->containers;
	kcon = kclist->containers;
	for (c_cnt = 0; c_cnt < vclist->count; ++c_cnt) {
		if (vcon[c_cnt].target != kcon[c_cnt].target) {
			ret = -EINVAL;
			vipx_err("target of container is different (%u/%u)\n",
					vcon[c_cnt].target, kcon[c_cnt].target);
			goto p_err;
		}

		if (vcon[c_cnt].count != kcon[c_cnt].count) {
			ret = -EINVAL;
			vipx_err("count of container is different (%u/%u)\n",
					vcon[c_cnt].count, kcon[c_cnt].count);
			goto p_err;
		}

		vbuf = vcon[c_cnt].buffers;
		kbuf = kcon[c_cnt].buffers;
		for (b_cnt = 0; b_cnt < vcon[c_cnt].count; ++b_cnt) {
			vbuf[b_cnt].roi = kbuf[b_cnt].roi;
			if (vbuf[b_cnt].m.fd != kbuf[b_cnt].m.fd) {
				ret = -EINVAL;
				vipx_err("fd of buffer is different (%d/%d)\n",
						vbuf[b_cnt].m.fd,
						kbuf[b_cnt].m.fd);
				goto p_err;
			}
		}
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int __vipx_queue_bundle_alloc(struct vipx_queue *queue,
		struct vs4l_container_list *kclist)
{
	int ret;
	struct vs4l_container *kcon;
	struct vs4l_buffer *kbuf;
	struct vipx_bundle *bundle;
	struct vipx_container_list *vclist;
	struct vipx_container *vcon;
	struct vipx_buffer *vbuf;
	size_t alloc_size;
	char *ptr;
	unsigned int c_cnt, f_cnt, b_cnt;

	vipx_enter();
	/* All memory allocation and pointer value assignment */
	kcon = kclist->containers;
	alloc_size = sizeof(*bundle);
	for (c_cnt = 0; c_cnt < kclist->count; ++c_cnt)
		alloc_size += sizeof(*vcon) + sizeof(*vbuf) * kcon[c_cnt].count;

	bundle = kzalloc(alloc_size, GFP_KERNEL);
	if (!bundle) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc bundle (%zu)\n", alloc_size);
		goto p_err_alloc;
	}

	vclist = &bundle->clist;
	ptr = (char *)bundle + sizeof(*bundle);
	vclist->containers = (struct vipx_container *)ptr;
	ptr += sizeof(*vcon) * kclist->count;
	for (c_cnt = 0; c_cnt < kclist->count; ++c_cnt) {
		vclist->containers[c_cnt].buffers = (struct vipx_buffer *)ptr;
		ptr += sizeof(*vbuf) * kcon[c_cnt].count;
	}

	vclist->direction = kclist->direction;
	vclist->id = kclist->id;
	vclist->index = kclist->index;
	vclist->flags = kclist->flags;
	vclist->count = kclist->count;

	if (sizeof(vclist->user_params) != sizeof(kclist->user_params)) {
		ret = -EINVAL;
		vipx_err("user params size is invalid (%zu/%zu)\n",
				sizeof(vclist->user_params),
				sizeof(kclist->user_params));
		goto p_err_param;
	}
	memcpy(vclist->user_params, kclist->user_params,
			sizeof(vclist->user_params));

	vcon = vclist->containers;
	for (c_cnt = 0; c_cnt < kclist->count; ++c_cnt) {
		vcon[c_cnt].type = kcon[c_cnt].type;
		vcon[c_cnt].target = kcon[c_cnt].target;
		vcon[c_cnt].memory = kcon[c_cnt].memory;
		if (sizeof(vcon[c_cnt].reserved) !=
				sizeof(kcon[c_cnt].reserved)) {
			ret = -EINVAL;
			vipx_err("container reserve is invalid (%zu/%zu)\n",
					sizeof(vcon[c_cnt].reserved),
					sizeof(kcon[c_cnt].reserved));
			goto p_err_param;
		}
		memcpy(vcon->reserved, kcon[c_cnt].reserved,
				sizeof(vcon->reserved));
		vcon[c_cnt].count = kcon[c_cnt].count;

		for (f_cnt = 0; f_cnt < queue->flist.count; ++f_cnt) {
			if (vcon[c_cnt].target ==
					queue->flist.formats[f_cnt].target) {
				vcon[c_cnt].format =
					&queue->flist.formats[f_cnt];
				break;
			}
		}

		if (!vcon[c_cnt].format) {
			ret = -EINVAL;
			vipx_err("Format(%u) is not set\n", vcon[c_cnt].target);
			goto p_err_param;
		}

		vbuf = vcon[c_cnt].buffers;
		kbuf = kcon[c_cnt].buffers;
		for (b_cnt = 0; b_cnt < kcon[c_cnt].count; ++b_cnt) {
			vbuf[b_cnt].roi = kbuf[b_cnt].roi;
			vbuf[b_cnt].m.fd = kbuf[b_cnt].m.fd;
		}
	}

	queue->bufs[kclist->index] = bundle;
	queue->num_buffers++;

	vipx_leave();
	return 0;
p_err_param:
	kfree(bundle);
p_err_alloc:
	return ret;
}

static void __vipx_queue_bundle_free(struct vipx_queue *queue,
		struct vipx_bundle *bundle)
{
	vipx_enter();
	queue->num_buffers--;
	queue->bufs[bundle->clist.index] = NULL;
	kfree(bundle);
	vipx_leave();
}

static int __vipx_queue_s_format(struct vipx_queue *queue,
		struct vs4l_format_list *flist)
{
	int ret;
	unsigned int idx;
	struct vipx_buffer_format *vformat;
	struct vs4l_format *kformat;
	struct vipx_format_type *fmt;

	vipx_enter();
	vformat = kcalloc(flist->count, sizeof(*vformat), GFP_KERNEL);
	if (!vformat) {
		ret = -ENOMEM;
		vipx_err("Failed to allocate vformats\n");
		goto p_err_alloc;
	}
	queue->flist.count = flist->count;
	queue->flist.formats = vformat;
	kformat = flist->formats;

	for (idx = 0; idx < flist->count; ++idx) {
		fmt = __vipx_queue_find_format(kformat[idx].format);
		if (IS_ERR(fmt)) {
			ret = PTR_ERR(fmt);
			goto p_err_find;
		}

		vformat[idx].fmt = fmt;
		vformat[idx].colorspace = kformat[idx].format;
		vformat[idx].target = kformat[idx].target;
		vformat[idx].plane = kformat[idx].plane;
		vformat[idx].width = kformat[idx].width;
		vformat[idx].height = kformat[idx].height;

		ret = __vipx_queue_plane_size(&vformat[idx]);
		if (ret)
			goto p_err_plane;
	}

	vipx_leave();
	return 0;
p_err_plane:
p_err_find:
	kfree(vformat);
p_err_alloc:
	return ret;
}

static int __vipx_queue_qbuf(struct vipx_queue *queue,
		struct vs4l_container_list *clist)
{
	int ret;
	struct vipx_bundle *bundle;
	struct vipx_container *con;
	struct vipx_buffer *buf;
	int dma_dir;
	unsigned int c_cnt, b_cnt;

	vipx_enter();
	if (clist->index >= VIPX_MAX_BUFFER) {
		ret = -EINVAL;
		vipx_err("clist index is out of range (%u/%u)\n",
				clist->index, VIPX_MAX_BUFFER);
		goto p_err;
	}

	if (queue->bufs[clist->index]) {
		bundle = queue->bufs[clist->index];
		ret = __vipx_queue_bundle_check(bundle, clist);
		if (ret)
			goto p_err;
	} else {
		ret = __vipx_queue_bundle_alloc(queue, clist);
		if (ret)
			goto p_err;

		bundle = queue->bufs[clist->index];
	}

	if (bundle->state != VIPX_BUNDLE_STATE_DEQUEUED) {
		ret = -EINVAL;
		vipx_err("bundle is already in use (%u)\n", bundle->state);
		goto p_err;
	}

	ret = __vipx_queue_bundle_prepare(queue, bundle);
	if (ret)
		goto p_err;

	if (queue->direction == VS4L_DIRECTION_OT)
		dma_dir = DMA_FROM_DEVICE;
	else
		dma_dir = DMA_TO_DEVICE;

	/* TODO: check sync */
	con = bundle->clist.containers;
	for (c_cnt = 0; c_cnt < bundle->clist.count; ++c_cnt) {
		buf = con[c_cnt].buffers;
		for (b_cnt = 0; b_cnt < con[c_cnt].count; ++b_cnt)
			__dma_map_area(buf[b_cnt].kvaddr, buf[b_cnt].size,
					dma_dir);
	}

	/*
	 * Add to the queued buffers list, a buffer will stay on it until
	 * dequeued in dqbuf.
	 */
	list_add_tail(&bundle->queued_entry, &queue->queued_list);
	bundle->state = VIPX_BUNDLE_STATE_QUEUED;
	atomic_inc(&queue->queued_count);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_queue_process(struct vipx_queue *queue,
		struct vipx_bundle *bundle)
{
	vipx_enter();
	bundle->state = VIPX_BUNDLE_STATE_PROCESS;
	list_add_tail(&bundle->process_entry, &queue->process_list);
	atomic_inc(&queue->process_count);
	vipx_leave();
}

static int __vipx_queue_dqbuf(struct vipx_queue *queue,
		struct vs4l_container_list *clist)
{
	int ret;
	struct vipx_bundle *bundle;

	vipx_enter();
	if (queue->direction != clist->direction) {
		ret = -EINVAL;
		vipx_err("direction of queue is different (%u/%u)\n",
				queue->direction, clist->direction);
		goto p_err;
	}

	bundle = __vipx_queue_get_done_bundle(queue, clist);
	if (IS_ERR(bundle)) {
		ret = PTR_ERR(bundle);
		goto p_err;
	}

	if (bundle->state != VIPX_BUNDLE_STATE_DONE) {
		ret = -EINVAL;
		vipx_err("bundle state is not done (%u)\n", bundle->state);
		goto p_err;
	}

	/* Fill buffer information for the userspace */
	__vipx_queue_fill_vs4l_buffer(bundle, clist);

	bundle->state = VIPX_BUNDLE_STATE_DEQUEUED;
	list_del(&bundle->queued_entry);
	atomic_dec(&queue->queued_count);

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int __vipx_queue_start(struct vipx_queue *queue)
{
	vipx_enter();
	queue->streaming = 1;
	vipx_leave();
	return 0;
}

static int __vipx_queue_stop(struct vipx_queue *queue)
{
	int ret;
	struct vipx_bundle **bundle;
	unsigned int idx;

	vipx_enter();
	queue->streaming = 0;
	wake_up_all(&queue->done_wq);

	if (atomic_read(&queue->queued_count)) {
		ret = -EINVAL;
		vipx_err("queued list is not empty (%d)\n",
				atomic_read(&queue->queued_count));
		goto p_err;
	}

	if (atomic_read(&queue->process_count)) {
		ret = -EINVAL;
		vipx_err("process list is not empty (%d)\n",
				atomic_read(&queue->process_count));
		goto p_err;
	}

	if (atomic_read(&queue->done_count)) {
		ret = -EINVAL;
		vipx_err("done list is not empty (%d)\n",
				atomic_read(&queue->done_count));
		goto p_err;
	}

	bundle = queue->bufs;
	for (idx = 0; queue->num_buffers && idx < VIPX_MAX_BUFFER; ++idx) {
		if (!bundle[idx])
			continue;

		ret = __vipx_queue_bundle_unprepare(queue, bundle[idx]);
		if (ret)
			goto p_err;

		__vipx_queue_bundle_free(queue, bundle[idx]);
	}

	if (queue->num_buffers) {
		ret = -EINVAL;
		vipx_err("bundle leak issued (%u)\n", queue->num_buffers);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_queue_poll(struct vipx_queue_list *qlist, struct file *file,
		struct poll_table_struct *poll)
{
	int ret = 0;
	unsigned long events;
	struct vipx_queue *inq, *outq;

	vipx_enter();
	events = poll_requested_events(poll);

	inq = &qlist->in_queue;
	outq = &qlist->out_queue;

	if (events & POLLIN) {
		if (list_empty(&inq->done_list))
			poll_wait(file, &inq->done_wq, poll);

		if (list_empty(&inq->done_list))
			ret |= POLLIN | POLLWRNORM;
	}

	if (events & POLLOUT) {
		if (list_empty(&outq->done_list))
			poll_wait(file, &outq->done_wq, poll);

		if (list_empty(&outq->done_list))
			ret |= POLLOUT | POLLWRNORM;
	}

	vipx_leave();
	return ret;
}

static int vipx_queue_set_graph(struct vipx_queue_list *qlist,
		struct vs4l_graph *ginfo)
{
	int ret;

	vipx_enter();
	ret = qlist->gops->set_graph(qlist->vctx->graph, ginfo);
	if (ret)
		goto p_err_gops;

	vipx_leave();
	return 0;
p_err_gops:
	return ret;
}

static int vipx_queue_set_format(struct vipx_queue_list *qlist,
		struct vs4l_format_list *flist)
{
	int ret;
	struct vipx_queue *queue;

	vipx_enter();
	if (flist->direction == VS4L_DIRECTION_IN)
		queue = &qlist->in_queue;
	else
		queue = &qlist->out_queue;

	ret = qlist->gops->set_format(qlist->vctx->graph, flist);
	if (ret)
		goto p_err_gops;

	ret = __vipx_queue_s_format(queue, flist);
	if (ret)
		goto p_err_s_format;

	vipx_leave();
	return 0;
p_err_s_format:
p_err_gops:
	return ret;
}

static int vipx_queue_set_param(struct vipx_queue_list *qlist,
		struct vs4l_param_list *plist)
{
	int ret;

	vipx_enter();
	ret = qlist->gops->set_param(qlist->vctx->graph, plist);
	if (ret)
		goto p_err_gops;

	vipx_leave();
	return 0;
p_err_gops:
	return ret;
}

static int vipx_queue_set_ctrl(struct vipx_queue_list *qlist,
		struct vs4l_ctrl *ctrl)
{
	int ret;

	vipx_enter();
	switch (ctrl->ctrl) {
	default:
		ret = -EINVAL;
		vipx_err("vs4l control is invalid(%d)\n", ctrl->ctrl);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_queue_qbuf(struct vipx_queue_list *qlist,
		struct vs4l_container_list *clist)
{
	int ret;
	struct vipx_queue *inq, *outq, *queue;
	struct vipx_bundle *invb, *otvb;

	vipx_enter();
	inq = &qlist->in_queue;
	outq = &qlist->out_queue;

	if (clist->direction == VS4L_DIRECTION_IN)
		queue = inq;
	else
		queue = outq;

	ret = __vipx_queue_qbuf(queue, clist);
	if (ret)
		goto p_err_qbuf;

	if (list_empty(&inq->queued_list) || list_empty(&outq->queued_list))
		return 0;

	invb = list_first_entry(&inq->queued_list,
			struct vipx_bundle, queued_entry);
	otvb = list_first_entry(&outq->queued_list,
			struct vipx_bundle, queued_entry);

	__vipx_queue_process(inq, invb);
	__vipx_queue_process(outq, otvb);

	ret = qlist->gops->queue(qlist->vctx->graph,
			&invb->clist, &otvb->clist);
	if (ret)
		goto p_err_gops;

	vipx_leave();
	return 0;
p_err_gops:
p_err_qbuf:
	return ret;
}

static int vipx_queue_dqbuf(struct vipx_queue_list *qlist,
		struct vs4l_container_list *clist)
{
	int ret;
	struct vipx_queue *queue;
	struct vipx_bundle *bundle;

	vipx_enter();
	if (clist->direction == VS4L_DIRECTION_IN)
		queue = &qlist->in_queue;
	else
		queue = &qlist->out_queue;

	ret = __vipx_queue_dqbuf(queue, clist);
	if (ret)
		goto p_err_dqbuf;

	if (clist->index >= VIPX_MAX_BUFFER) {
		ret = -EINVAL;
		vipx_err("clist index is out of range (%u/%u)\n",
				clist->index, VIPX_MAX_BUFFER);
		goto p_err_index;
	}

	bundle = queue->bufs[clist->index];
	if (!bundle) {
		ret = -EINVAL;
		vipx_err("bundle(%u) is NULL\n", clist->index);
		goto p_err_bundle;
	}

	if (bundle->clist.index != clist->index) {
		ret = -EINVAL;
		vipx_err("index of clist is different (%u/%u)\n",
				bundle->clist.index, clist->index);
		goto p_err_bundle_index;
	}

	ret = qlist->gops->deque(qlist->vctx->graph, &bundle->clist);
	if (ret)
		goto p_err_gops;

	vipx_leave();
	return 0;
p_err_gops:
p_err_bundle_index:
p_err_bundle:
p_err_index:
p_err_dqbuf:
	return ret;
}

static int vipx_queue_streamon(struct vipx_queue_list *qlist)
{
	int ret;
	struct vipx_queue *inq, *outq;

	vipx_enter();
	inq = &qlist->in_queue;
	outq = &qlist->out_queue;

	__vipx_queue_start(inq);
	__vipx_queue_start(outq);

	ret = qlist->gops->start(qlist->vctx->graph);
	if (ret)
		goto p_err_gops;

	vipx_leave();
	return 0;
p_err_gops:
	return ret;
}

static int vipx_queue_streamoff(struct vipx_queue_list *qlist)
{
	int ret;
	struct vipx_queue *inq, *outq;

	vipx_enter();
	inq = &qlist->in_queue;
	outq = &qlist->out_queue;

	ret = qlist->gops->stop(qlist->vctx->graph);
	if (ret)
		goto p_err_gops;

	__vipx_queue_stop(inq);
	__vipx_queue_stop(outq);

	vipx_leave();
	return 0;
p_err_gops:
	return ret;
}

const struct vipx_context_qops vipx_context_qops = {
	.poll		= vipx_queue_poll,
	.set_graph	= vipx_queue_set_graph,
	.set_format	= vipx_queue_set_format,
	.set_param	= vipx_queue_set_param,
	.set_ctrl	= vipx_queue_set_ctrl,
	.qbuf		= vipx_queue_qbuf,
	.dqbuf		= vipx_queue_dqbuf,
	.streamon	= vipx_queue_streamon,
	.streamoff	= vipx_queue_streamoff
};

static void __vipx_queue_done(struct vipx_queue *queue,
		struct vipx_bundle *bundle)
{
	int dma_dir;
	struct vipx_container *con;
	struct vipx_buffer *buf;
	unsigned int c_cnt, b_cnt;
	unsigned long flags;

	vipx_enter();
	if (queue->direction != bundle->clist.direction) {
		vipx_err("direction of queue is different (%u/%u)\n",
				queue->direction, bundle->clist.direction);
	}

	if (queue->direction == VS4L_DIRECTION_OT)
		dma_dir = DMA_FROM_DEVICE;
	else
		dma_dir = DMA_TO_DEVICE;

	/* TODO: check sync */
	con = bundle->clist.containers;
	for (c_cnt = 0; c_cnt < bundle->clist.count; ++c_cnt) {
		buf = con[c_cnt].buffers;
		for (b_cnt = 0; b_cnt < con[c_cnt].count; ++b_cnt)
			__dma_map_area(buf[b_cnt].kvaddr, buf[b_cnt].size,
					dma_dir);
	}

	spin_lock_irqsave(&queue->done_lock, flags);

	list_del(&bundle->process_entry);
	atomic_dec(&queue->process_count);

	bundle->state = VIPX_BUNDLE_STATE_DONE;
	list_add_tail(&bundle->done_entry, &queue->done_list);
	atomic_inc(&queue->done_count);

	spin_unlock_irqrestore(&queue->done_lock, flags);
	wake_up(&queue->done_wq);
	vipx_leave();
}

void vipx_queue_done(struct vipx_queue_list *qlist,
		struct vipx_container_list *incl,
		struct vipx_container_list *otcl,
		unsigned long flags)
{
	struct vipx_queue *inq, *outq;
	struct vipx_bundle *invb, *otvb;

	vipx_enter();
	inq = &qlist->in_queue;
	outq = &qlist->out_queue;

	if (!list_empty(&outq->process_list)) {
		otvb = container_of(otcl, struct vipx_bundle, clist);
		if (otvb->state == VIPX_BUNDLE_STATE_PROCESS) {
			otvb->flags |= flags;
			__vipx_queue_done(outq, otvb);
		} else {
			vipx_err("out-bundle state(%d) is not process\n",
					otvb->state);
		}
	} else {
		vipx_err("out-queue is empty\n");
	}

	if (!list_empty(&inq->process_list)) {
		invb = container_of(incl, struct vipx_bundle, clist);
		if (invb->state == VIPX_BUNDLE_STATE_PROCESS) {
			invb->flags |= flags;
			__vipx_queue_done(inq, invb);
		} else {
			vipx_err("in-bundle state(%u) is not process\n",
					invb->state);
		}
	} else {
		vipx_err("in-queue is empty\n");
	}

	vipx_leave();
}

static void __vipx_queue_init(struct vipx_context *vctx,
		struct vipx_queue *queue, unsigned int direction)
{
	vipx_enter();
	queue->direction = direction;
	queue->streaming = 0;
	queue->lock = &vctx->lock;

	INIT_LIST_HEAD(&queue->queued_list);
	atomic_set(&queue->queued_count, 0);
	INIT_LIST_HEAD(&queue->process_list);
	atomic_set(&queue->process_count, 0);
	INIT_LIST_HEAD(&queue->done_list);
	atomic_set(&queue->done_count, 0);

	spin_lock_init(&queue->done_lock);
	init_waitqueue_head(&queue->done_wq);

	queue->flist.count = 0;
	queue->flist.formats = NULL;
	queue->num_buffers = 0;
	queue->qlist = &vctx->queue_list;
	vipx_leave();
}

int vipx_queue_init(struct vipx_context *vctx)
{
	struct vipx_queue_list *qlist;
	struct vipx_queue *inq, *outq;

	vipx_enter();
	vctx->queue_ops = &vipx_context_qops;

	qlist = &vctx->queue_list;
	qlist->vctx = vctx;
	qlist->gops = &vipx_queue_gops;

	inq = &qlist->in_queue;
	outq = &qlist->out_queue;

	__vipx_queue_init(vctx, inq, VS4L_DIRECTION_IN);
	__vipx_queue_init(vctx, outq, VS4L_DIRECTION_OT);

	vipx_leave();
	return 0;
}
