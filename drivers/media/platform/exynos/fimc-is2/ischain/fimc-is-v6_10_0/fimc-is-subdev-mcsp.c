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
#include "fimc-is-subdev-ctrl.h"
#include "fimc-is-config.h"
#include "fimc-is-param.h"
#include "fimc-is-video.h"
#include "fimc-is-type.h"

#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-hw-dvfs.h"

#if defined(CONFIG_VIDEO_EXYNOS_SMFC) && defined(ENABLE_HWFC)
#include "../../../smfc/smfc-sync.h"
#endif

void fimc_is_ischain_mxp_stripe_cfg(struct fimc_is_subdev *subdev,
		struct fimc_is_frame *ldr_frame,
		struct fimc_is_crop *incrop,
		struct fimc_is_crop *otcrop,
		struct fimc_is_frame_cfg *framecfg)
{
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_fmt *fmt = framecfg->format;
	bool x_flip = test_bit(SCALER_FLIP_COMMAND_X_MIRROR, &framecfg->flip);
	unsigned long flags;
	u32 stripe_x, stripe_w;
	long long dma_offset = 0;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr)
		return;

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		/* Input crop configuration */
		if (!ldr_frame->stripe_info.region_id) {
			/* Left region w/o margin */
			stripe_x = incrop->x;
			stripe_w = ldr_frame->stripe_info.in.h_pix_num - stripe_x;

			frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
			frame->stripe_info.in.h_pix_num = stripe_w;
		} else {
			/* Right region w/o margin */
			stripe_x = STRIPE_MARGIN_WIDTH;
			stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;
		}

		incrop->x = stripe_x;
		incrop->w = stripe_w;

		/* Output crop & WDMA offset configuration */
		if (!ldr_frame->stripe_info.region_id) {
			/* Left region */
			stripe_w = ALIGN(otcrop->w * frame->stripe_info.in.h_pix_ratio / STRIPE_RATIO_PRECISION, 2);

			frame->stripe_info.out.h_pix_num = stripe_w;

			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = (otcrop->w - stripe_w) * fmt->bitsperpixel[0] / BITS_PER_BYTE;
		} else {
			/* Right region */
			stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num;

			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = -(stripe_w * fmt->bitsperpixel[0] / BITS_PER_BYTE);
			else
				dma_offset = frame->stripe_info.out.h_pix_num * fmt->bitsperpixel[0] / BITS_PER_BYTE;
		}

		otcrop->w = stripe_w;

		frame->dvaddr_buffer[0] += dma_offset;

		mdbg_pframe("stripe_in_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, ldr_frame,
				ldr_frame->stripe_info.region_id,
				incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
		mdbg_pframe("stripe_ot_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, ldr_frame,
				ldr_frame->stripe_info.region_id,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
}

static int fimc_is_ischain_mxp_adjust_crop(struct fimc_is_device_ischain *device,
	u32 input_crop_w, u32 input_crop_h,
	u32 *output_crop_w, u32 *output_crop_h);

static int fimc_is_ischain_mxp_cfg(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_queue *queue;
	struct fimc_is_fmt *format;
	struct param_mcs_output *mcs_output;
	struct fimc_is_device_ischain *device;
	u32 width, height;
	u32 crange;

	device = (struct fimc_is_device_ischain *)device_data;

	FIMC_BUG(!device);
	FIMC_BUG(!incrop);

	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	format = queue->framecfg.format;
	if (!format) {
		mserr("format is NULL", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	width = queue->framecfg.width;
	height = queue->framecfg.height;
	fimc_is_ischain_mxp_adjust_crop(device, incrop->w, incrop->h, &width, &height);

	if (queue->framecfg.quantization == V4L2_QUANTIZATION_FULL_RANGE) {
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
		msinfo("CRange:W\n", device, subdev);
	} else {
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;
		msinfo("CRange:N\n", device, subdev);
	}

	mcs_output = fimc_is_itf_g_param(device, frame, subdev->param_dma_ot);

	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_ENABLE;
	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->dma_bitwidth = format->hw_bitwidth;
	mcs_output->dma_format = format->hw_format;
	mcs_output->dma_order = format->hw_order;
	mcs_output->plane = format->hw_plane;

	mcs_output->crop_offset_x = incrop->x;
	mcs_output->crop_offset_y = incrop->y;
	mcs_output->crop_width = incrop->w;
	mcs_output->crop_height = incrop->h;

	mcs_output->width = width;
	mcs_output->height = height;
	/* HW spec: stride should be aligned by 16 byte. */
	mcs_output->dma_stride_y = ALIGN(max(width * format->bitsperpixel[0] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[0]), 16);
	mcs_output->dma_stride_c = ALIGN(max(width * format->bitsperpixel[1] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[1]), 16);

	mcs_output->yuv_range = crange;
	mcs_output->flip = (u32)queue->framecfg.flip >> 1; /* Caution: change from bitwise to enum */

#ifdef ENABLE_HWFC
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		mcs_output->hwfc = 1; /* TODO: enum */
	else
		mcs_output->hwfc = 0; /* TODO: enum */
#endif

#ifdef SOC_VRA
	if (device->group_mcs.junction == subdev) {
		struct param_otf_input *otf_input;
		otf_input = fimc_is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
		otf_input->width = width;
		otf_input->height = height;
		*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
		(*indexes)++;
	}
#endif

	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;


p_err:
	return ret;
}

#define MXP_RATIO_UP	(10)
#define MXP_RATIO_DOWN	(32)

static int fimc_is_ischain_mxp_adjust_crop(struct fimc_is_device_ischain *device,
	u32 input_crop_w, u32 input_crop_h,
	u32 *output_crop_w, u32 *output_crop_h)
{
	int changed = 0;

	if (*output_crop_w > input_crop_w * MXP_RATIO_UP) {
		mwarn("Cannot be scaled up beyond %d times(%d -> %d)",
			device, MXP_RATIO_UP, input_crop_h, *output_crop_h);
		*output_crop_h = input_crop_h * MXP_RATIO_UP;
		changed |= 0x01;
	}

	if (*output_crop_h > input_crop_h * MXP_RATIO_UP) {
		mwarn("Cannot be scaled up beyond %d times(%d -> %d)",
			device, MXP_RATIO_UP, input_crop_h, *output_crop_h);
		*output_crop_h = input_crop_h * MXP_RATIO_UP;
		changed |= 0x02;
	}

	if (*output_crop_w < (input_crop_w + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN) {
		mwarn("Cannot be scaled down beyond 1/32 times(%d -> %d)",
			device, input_crop_w, *output_crop_w);
		*output_crop_w = ALIGN((input_crop_w + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN, 2);
		changed |= 0x10;
	}

	if (*output_crop_h < (input_crop_h + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN) {
		mwarn("Cannot be scaled down beyond 1/32 times(%d -> %d)",
			device, input_crop_h, *output_crop_h);
		*output_crop_h = ALIGN((input_crop_h + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN, 2);
		changed |= 0x20;
	}

	return changed;
}

static int fimc_is_ischain_mxp_compare_size(struct fimc_is_device_ischain *device,
	struct mcs_param *mcs_param,
	struct fimc_is_crop *incrop)
{
	int changed = 0;

	if (mcs_param->input.otf_cmd == OTF_INPUT_COMMAND_ENABLE) {
		if (incrop->x + incrop->w > mcs_param->input.width) {
			mwarn("Out of crop width region(%d < %d)",
				device, mcs_param->input.width, incrop->x + incrop->w);
			incrop->x = 0;
			incrop->w = mcs_param->input.width;
			changed |= 0x01;
		}

		if (incrop->y + incrop->h > mcs_param->input.height) {
			mwarn("Out of crop height region(%d < %d)",
				device, mcs_param->input.height, incrop->y + incrop->h);
			incrop->y = 0;
			incrop->h = mcs_param->input.height;
			changed |= 0x02;
		}
	} else {
		if (incrop->x + incrop->w > mcs_param->input.dma_crop_width) {
			mwarn("Out of crop width region(%d < %d)",
				device, mcs_param->input.dma_crop_width, incrop->x + incrop->w);
			incrop->x = 0;
			incrop->w = mcs_param->input.dma_crop_width;
			changed |= 0x01;
		}

		if (incrop->y + incrop->h > mcs_param->input.dma_crop_height) {
			mwarn("Out of crop height region(%d < %d)",
				device, mcs_param->input.dma_crop_height, incrop->y + incrop->h);
			incrop->y = 0;
			incrop->h = mcs_param->input.dma_crop_height;
			changed |= 0x02;
		}
	}

	return changed;
}

static int fimc_is_ischain_mxp_start(struct fimc_is_device_ischain *device,
	struct camera2_node *node,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct mcs_param *mcs_param,
	struct param_mcs_output *mcs_output,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	ulong index,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_fmt *format, *tmp_format;
	struct param_otf_input *otf_input = NULL;
	u32 crange;
#if ANDROID_VERSION >= 100000 /* Over Q */
	u32 flip = mcs_output->flip;
#endif
	struct fimc_is_crop incrop_cfg, otcrop_cfg;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	if (node->pixelformat && format->pixelformat != node->pixelformat) { /* per-frame control for RGB */
		tmp_format = fimc_is_find_format((u32)node->pixelformat, 0);
		if (tmp_format) {
			mdbg_pframe("pixelformat is changed(%c%c%c%c->%c%c%c%c)\n",
				device, subdev, frame,
				(char)((format->pixelformat >> 0) & 0xFF),
				(char)((format->pixelformat >> 8) & 0xFF),
				(char)((format->pixelformat >> 16) & 0xFF),
				(char)((format->pixelformat >> 24) & 0xFF),
				(char)((tmp_format->pixelformat >> 0) & 0xFF),
				(char)((tmp_format->pixelformat >> 8) & 0xFF),
				(char)((tmp_format->pixelformat >> 16) & 0xFF),
				(char)((tmp_format->pixelformat >> 24) & 0xFF));
			queue->framecfg.format = format = tmp_format;
		} else {
			mdbg_pframe("pixelformat is not found(%c%c%c%c)\n",
				device, subdev, frame,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));
		}
	}

#if ANDROID_VERSION >= 100000 /* Over Q */
	if ((index >= PARAM_MCS_OUTPUT0 && index < PARAM_MCS_OUTPUT5) &&
		frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0] != mcs_output->flip) { /* per-frame control for flip */
		flip = frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0];
		queue->framecfg.flip = flip << 1;
		mdbg_pframe("flip is changed(%d->%d)\n",
			device, subdev, frame,
			mcs_output->flip,
			flip);
	}
#endif

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		fimc_is_ischain_mxp_stripe_cfg(subdev, frame,
				&incrop_cfg, &otcrop_cfg,
				&queue->framecfg);

	/* if output DS, skip check a incrop & input mcs param
	 * because, DS input size set to preview port output size
	 */
	if ((index - PARAM_MCS_OUTPUT0) != MCSC_OUTPUT_DS)
		fimc_is_ischain_mxp_compare_size(device, mcs_param, &incrop_cfg);

	fimc_is_ischain_mxp_adjust_crop(device, incrop_cfg.w, incrop_cfg.h, &otcrop_cfg.w, &otcrop_cfg.h);

	if (queue->framecfg.quantization == V4L2_QUANTIZATION_FULL_RANGE) {
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
		mdbg_pframe("CRange:W\n", device, subdev, frame);
	} else {
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;
		mdbg_pframe("CRange:N\n", device, subdev, frame);
	}

	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_ENABLE;
	mcs_output->dma_bitwidth = format->hw_bitwidth;
	mcs_output->dma_format = format->hw_format;
	mcs_output->dma_order = format->hw_order;
	mcs_output->plane = format->hw_plane;

	mcs_output->crop_offset_x = incrop_cfg.x; /* per frame */
	mcs_output->crop_offset_y = incrop_cfg.y; /* per frame */
	mcs_output->crop_width = incrop_cfg.w; /* per frame */
	mcs_output->crop_height = incrop_cfg.h; /* per frame */

	mcs_output->width = otcrop_cfg.w; /* per frame */
	mcs_output->height = otcrop_cfg.h; /* per frame */
	/* HW spec: stride should be aligned by 16 byte. */

	mcs_output->dma_stride_y = ALIGN(max(otcrop->w * format->bitsperpixel[0] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[0]), 16);
	mcs_output->dma_stride_c = ALIGN(max(otcrop->w * format->bitsperpixel[1] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[1]), 16);

	mcs_output->yuv_range = crange;
#if ANDROID_VERSION >= 100000 /* Over Q */
	mcs_output->flip = flip;
#else
	mcs_output->flip = (u32)queue->framecfg.flip >> 1; /* Caution: change from bitwise to enum */
#endif

#ifdef ENABLE_HWFC
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		mcs_output->hwfc = 1; /* TODO: enum */
	else
		mcs_output->hwfc = 0; /* TODO: enum */
#endif

	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

#ifdef SOC_VRA
	if (device->group_mcs.junction == subdev) {
		otf_input = fimc_is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
		otf_input->width = otcrop_cfg.w;
		otf_input->height = otcrop_cfg.h;
		*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
		(*indexes)++;
	}
#endif

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_mxp_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct param_mcs_output *mcs_output,
	ulong index,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;

	mdbgd_ischain("%s\n", device, __func__);

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static void fimc_is_ischain_mxp_otf_enable(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct param_mcs_output *mcs_output,
	ulong index,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
#if !defined(USE_VRA_OTF)
	struct param_mcs_input *input;

	input = fimc_is_itf_g_param(device, frame, PARAM_MCS_INPUT);

	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_ENABLE;

	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->crop_offset_x = 0;
	mcs_output->crop_offset_y = 0;

	if (input->otf_cmd == OTF_INPUT_COMMAND_ENABLE) {
		mcs_output->crop_width = input->width;
		mcs_output->crop_height = input->height;
	} else {
		mcs_output->crop_width = input->dma_crop_width;
		mcs_output->crop_height = input->dma_crop_height;
	}

	/* HACK */
	if (mcs_output->crop_width > 640 && mcs_output->crop_height > 480) {
		mcs_output->width = mcs_output->crop_width;
		mcs_output->height = mcs_output->crop_height;
	} else {
		mcs_output->width = 640;
		mcs_output->height = 480;
	}

	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

	mdbg_pframe("OTF only enable [%d, %d, %d, %d]-->[%d, %d]\n", device, subdev, frame,
		mcs_output->crop_offset_x, mcs_output->crop_offset_y,
		mcs_output->crop_width, mcs_output->crop_height,
		mcs_output->width, mcs_output->height);
#endif
}

static void fimc_is_ischain_mxp_otf_disable(struct fimc_is_device_ischain *device,
		struct fimc_is_subdev *subdev,
		struct fimc_is_frame *frame,
		struct param_mcs_output *mcs_output,
		ulong index,
		u32 *lindex,
		u32 *hindex,
		u32 *indexes)
{
#if !defined(USE_VRA_OTF)
	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_DISABLE;

	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

	mdbg_pframe("OTF only disable\n", device, subdev, frame);
#endif
}

static int fimc_is_ischain_mxp_tag(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct mcs_param *mcs_param;
	struct param_mcs_output *mcs_output;
	struct fimc_is_crop *incrop, *otcrop, inparm, otparm;
	struct fimc_is_device_ischain *device;
	u32 index, lindex, hindex, indexes;
	u32 pixelformat = 0;
	u32 *target_addr;
#if ANDROID_VERSION >= 100000 /* Over Q */
	bool change_flip = false;
#endif
	bool change_pixelformat = false;

	device = (struct fimc_is_device_ischain *)device_data;

	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!subdev);
	FIMC_BUG(!GET_SUBDEV_QUEUE(subdev));
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!ldr_frame->shot);
	FIMC_BUG(!node);

	mdbgs_ischain(4, "MXP TAG(request %d)\n", device, node->request);

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	mcs_param = &device->is_region->parameter.mcs;
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

	switch (node->vid) {
	case FIMC_IS_VIDEO_M0P_NUM:
		index = PARAM_MCS_OUTPUT0;
		target_addr = ldr_frame->sc0TargetAddress;
		break;
	case FIMC_IS_VIDEO_M1P_NUM:
		index = PARAM_MCS_OUTPUT1;
		target_addr = ldr_frame->sc1TargetAddress;
		break;
	case FIMC_IS_VIDEO_M2P_NUM:
		index = PARAM_MCS_OUTPUT2;
		target_addr = ldr_frame->sc2TargetAddress;
		break;
	case FIMC_IS_VIDEO_M3P_NUM:
		index = PARAM_MCS_OUTPUT3;
		target_addr = ldr_frame->sc3TargetAddress;
		break;
	case FIMC_IS_VIDEO_M4P_NUM:
		index = PARAM_MCS_OUTPUT4;
		target_addr = ldr_frame->sc4TargetAddress;
		break;
	case FIMC_IS_VIDEO_M5P_NUM:
		index = PARAM_MCS_OUTPUT5;
		target_addr = ldr_frame->sc5TargetAddress;
		break;
	default:
		mserr("vid(%d) is not matched", device, subdev, node->vid);
		ret = -EINVAL;
		goto p_err;
	}

	mcs_output = fimc_is_itf_g_param(device, ldr_frame, index);

	if (node->request) {
		incrop = (struct fimc_is_crop *)node->input.cropRegion;
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;
		if (node->pixelformat) {
			change_pixelformat = !COMPARE_FORMAT(pixelformat, node->pixelformat);
			pixelformat = node->pixelformat;
		}

#if ANDROID_VERSION >= 100000 /* Over Q */
		if ((index >= PARAM_MCS_OUTPUT0 && index < PARAM_MCS_OUTPUT5) &&
			ldr_frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0] != mcs_output->flip)
			change_flip = true;
#endif

		inparm.x = mcs_output->crop_offset_x;
		inparm.y = mcs_output->crop_offset_y;
		inparm.w = mcs_output->crop_width;
		inparm.h = mcs_output->crop_height;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = mcs_output->width;
		otparm.h = mcs_output->height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(incrop, &inparm) ||
			!COMPARE_CROP(otcrop, &otparm) ||
			CHECK_STRIPE_CFG(&ldr_frame->stripe_info) ||
#if ANDROID_VERSION >= 100000 /* Over Q */
			change_flip ||
#endif
			change_pixelformat ||
			test_bit(FIMC_IS_ISCHAIN_MODE_CHANGED, &device->state) ||
			!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = fimc_is_ischain_mxp_start(device,
				node,
				subdev,
				ldr_frame,
				queue,
				mcs_param,
				mcs_output,
				incrop,
				otcrop,
				index,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_mxp_start is fail(%d)", device, ret);
				goto p_err;
			}
			clear_bit(FIMC_IS_ISCHAIN_MODE_CHANGED, &device->state);

			mdbg_pframe("in_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		/* buf_tag should be set by unit of stride */
		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			mcs_output->dma_stride_y,
			otcrop->h,
			target_addr);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		ret = fimc_is_ischain_mxp_stop(device,
			subdev,
			ldr_frame,
			mcs_output,
			index,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("fimc_is_ischain_mxp_stop is fail(%d)", device, ret);
			goto p_err;
		}

		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state))
			mdbg_pframe(" off\n", device, subdev, ldr_frame);

		if ((node->vid - FIMC_IS_VIDEO_M0P_NUM)
			== (ldr_frame->shot->uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]))
			fimc_is_ischain_mxp_otf_enable(device,
				subdev,
				ldr_frame,
				mcs_output,
				index,
				&lindex,
				&hindex,
				&indexes);
		else if (mcs_output->otf_cmd == OTF_OUTPUT_COMMAND_ENABLE)
			fimc_is_ischain_mxp_otf_disable(device,
				subdev,
				ldr_frame,
				mcs_output,
				index,
				&lindex,
				&hindex,
				&indexes);

		target_addr[0] = 0;
		target_addr[1] = 0;
		target_addr[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

#if defined(CONFIG_VIDEO_EXYNOS_SMFC) && defined(ENABLE_HWFC)
	switch (node->vid) {
	case FIMC_IS_VIDEO_M0P_NUM:
		break;
	case FIMC_IS_VIDEO_M1P_NUM:
		ret = exynos_smfc_wait_done(mcs_output->hwfc);
		break;
	case FIMC_IS_VIDEO_M2P_NUM:
		if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
			ret = exynos_smfc_wait_done(mcs_output->hwfc);
		break;
	case FIMC_IS_VIDEO_M3P_NUM:
		break;
	case FIMC_IS_VIDEO_M4P_NUM:
		break;
	case FIMC_IS_VIDEO_M5P_NUM:
		break;
	default:
		mserr("vid(%d) is not matched", device, subdev, node->vid);
		ret = -EINVAL;
		goto p_err;
	}

	if (ret) {
		mrerr("exynos_smfc_wait_done is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}
#endif


p_err:
	return ret;
}

const struct fimc_is_subdev_ops fimc_is_subdev_mcsp_ops = {
	.bypass			= NULL,
	.cfg			= fimc_is_ischain_mxp_cfg,
	.tag			= fimc_is_ischain_mxp_tag,
};
