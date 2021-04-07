/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-device-ischain.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-subdev-ctrl.h"
#include "fimc-is-config.h"
#include "fimc-is-param.h"
#include "fimc-is-video.h"
#include "fimc-is-type.h"

void fimc_is_ischain_3ac_stripe_cfg(struct fimc_is_subdev *subdev,
		struct fimc_is_frame *ldr_frame,
		struct fimc_is_crop *otcrop,
		struct fimc_is_fmt *fmt)
{
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	unsigned long flags;
	u32 stripe_x, stripe_w, dma_offset = 0;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr)
		return;

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		/* Output crop & WDMA offset configuration */
		if (!ldr_frame->stripe_info.region_id) {
			/* Left region */
			stripe_x = otcrop->x;
			stripe_w = ldr_frame->stripe_info.out.h_pix_num;

			frame->stripe_info.out.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / otcrop->w;
			frame->stripe_info.out.h_pix_num = stripe_w;
		} else {
			/* Right region */
			stripe_x = 0;
			stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num + STRIPE_MARGIN_WIDTH;

			/* Add horizontal & vertical DMA offset */
			dma_offset = frame->stripe_info.out.h_pix_num - STRIPE_MARGIN_WIDTH;
			dma_offset = dma_offset * fmt->bitsperpixel[0] / BITS_PER_BYTE;
		}

		otcrop->x = stripe_x;
		otcrop->w = stripe_w;

		frame->dvaddr_buffer[0] += dma_offset;

		mdbg_pframe("stripe_ot_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, ldr_frame,
				ldr_frame->stripe_info.region_id,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
}

static int fimc_is_ischain_3ac_cfg(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	return 0;
}

static int fimc_is_ischain_3ac_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct taa_param *taa_param,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;
	struct fimc_is_module_enum *module;
	u32 hw_format, hw_bitwidth, hw_order;
	struct fimc_is_crop otcrop_cfg;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	otcrop_cfg = *otcrop;

	hw_format = queue->framecfg.format->hw_format;
	hw_order = queue->framecfg.format->hw_order;
	hw_bitwidth = queue->framecfg.format->hw_bitwidth; /* memory width per pixel */

	ret = fimc_is_sensor_g_module(device->sensor, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		fimc_is_ischain_3ac_stripe_cfg(subdev,
				frame,
				&otcrop_cfg,
				queue->framecfg.format);

	if ((otcrop_cfg.w > taa_param->otf_input.bayer_crop_width) ||
		(otcrop_cfg.h > taa_param->otf_input.bayer_crop_height)) {
		merr("3ac output size is invalid((%d, %d) != (%d, %d))", device,
			otcrop_cfg.w,
			otcrop_cfg.h,
			taa_param->otf_input.bayer_crop_width,
			taa_param->otf_input.bayer_crop_height);
		ret = -EINVAL;
		goto p_err;
	}

	if (otcrop_cfg.x || otcrop_cfg.y) {
		mwarn("crop pos(%d, %d) is ignored", device, otcrop_cfg.x, otcrop_cfg.y);
		otcrop_cfg.x = 0;
		otcrop_cfg.y = 0;
	}

	dma_output = fimc_is_itf_g_param(device, frame, subdev->param_dma_ot);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = hw_format;
	dma_output->order = hw_order;
	dma_output->bitwidth = hw_bitwidth;

	/* HACK: for reverse raw capture, forcely use the 10bit unpacked, not 12bit unpacked */
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
			hw_bitwidth == DMA_OUTPUT_BIT_WIDTH_16BIT)
		dma_output->msb = MSB_OF_DNG_DMA_OUT;
	else
		dma_output->msb = MSB_OF_3AA_DMA_OUT;

	dma_output->width = otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;
	dma_output->crop_enable = 0;

	/*
	 * Stride control by Driver.
	 * It should be cotrolled by Driver only when
	 * the DMA out width (otcrop_cfg.w) is smaller than full width (otcrop->w).
	 * stride_plane0 = 0: Stride control by DDK.
	 */
	dma_output->stride_plane0 = (otcrop_cfg.w < otcrop->w) ? otcrop->w : 0;

	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;

	subdev->output.crop = *otcrop;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_3ac_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;

	mdbgd_ischain("%s\n", device, __func__);

	dma_output = fimc_is_itf_g_param(device, frame, subdev->param_dma_ot);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_3ac_tag(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct taa_param *taa_param;
	struct fimc_is_crop *otcrop, otparm;
	struct fimc_is_device_ischain *device;
	u32 lindex, hindex, indexes;
	u32 pixelformat = 0;

	device = (struct fimc_is_device_ischain *)device_data;

	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!subdev);
	FIMC_BUG(!GET_SUBDEV_QUEUE(subdev));
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!ldr_frame->shot);

	mdbgs_ischain(4, "3AAC TAG(request %d)\n", device, node->request);

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	taa_param = &device->is_region->parameter.taa;
	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!queue->framecfg.format) {
		merr("format is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	pixelformat = queue->framecfg.format->pixelformat;

	if (node->request) {
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = taa_param->otf_input.bayer_crop_width;
		otparm.h = taa_param->otf_input.bayer_crop_height;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			CHECK_STRIPE_CFG(&ldr_frame->stripe_info) ||
			!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = fimc_is_ischain_3ac_start(device,
				subdev,
				ldr_frame,
				queue,
				taa_param,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3ac_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			otcrop->w,
			otcrop->h,
			ldr_frame->txcTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_3ac_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3ac_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		ldr_frame->txcTargetAddress[0] = 0;
		ldr_frame->txcTargetAddress[1] = 0;
		ldr_frame->txcTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_subdev_ops fimc_is_subdev_3ac_ops = {
	.bypass			= NULL,
	.cfg			= fimc_is_ischain_3ac_cfg,
	.tag			= fimc_is_ischain_3ac_tag,
};
