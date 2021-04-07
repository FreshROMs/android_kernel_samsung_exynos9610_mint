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

#include "fimc-is-devicemgr.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-subdev-ctrl.h"
#include "fimc-is-config.h"
#include "fimc-is-param.h"
#include "fimc-is-video.h"
#include "fimc-is-type.h"

void fimc_is_ischain_3aa_stripe_cfg(struct fimc_is_subdev *subdev,
		struct fimc_is_frame *frame,
		u32 *full_w, u32 *full_h,
		struct fimc_is_crop *incrop,
		struct fimc_is_crop *otcrop)
{
	u32 stripe_x, stripe_w;

	/* Input crop configuration */
	if (!frame->stripe_info.region_id) {
		/**
		 * Left region
		 * The stripe width should be in h_pix_num.
		 */
		stripe_x = otcrop->x;
		stripe_w = ALIGN_DOWN(frame->stripe_info.in.h_pix_num - stripe_x, STRIPE_WIDTH_ALIGN);
		*full_w = frame->stripe_info.in.h_pix_num;

		frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / otcrop->w;
		frame->stripe_info.in.h_pix_num = stripe_w + stripe_x;
	} else {
		/* Right region */
		stripe_x = 0;
		stripe_w = otcrop->w - (frame->stripe_info.in.h_pix_num - otcrop->x);
		*full_w = *full_w - frame->stripe_info.in.h_pix_num;
	}

	/* Add stripe processing horizontal margin into each region. */
	stripe_w += STRIPE_MARGIN_WIDTH;
	*full_w += STRIPE_MARGIN_WIDTH;

	incrop->x = stripe_x;
	incrop->y = otcrop->y;
	incrop->w = stripe_w;
	incrop->h = otcrop->h;

	/* Output crop configuration */
	if (!frame->stripe_info.region_id) {
		/* Left region */
		stripe_x = 0;
		stripe_w = frame->stripe_info.in.h_pix_num - otcrop->x;

		frame->stripe_info.out.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / otcrop->w;
		frame->stripe_info.out.h_pix_num = stripe_w;
	} else {
		/* Right region */
		stripe_x = 0;
		stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num;
	}

	/* Add stripe processing horizontal margin into each region. */
	stripe_w += STRIPE_MARGIN_WIDTH;

	otcrop->x = stripe_x;
	otcrop->y = 0;
	otcrop->w = stripe_w;

	mdbg_pframe("stripe_in_crop[%d][%d, %d, %d, %d, %d, %d]\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, *full_w, *full_h);
	mdbg_pframe("stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
}

static int fimc_is_ischain_3aa_cfg(struct fimc_is_subdev *leader,
	void *device_data,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_group *head;
	struct fimc_is_group *group;
	struct fimc_is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_input *dma_input;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_ischain *device;
	u32 hw_format = DMA_INPUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INPUT_BIT_WIDTH_16BIT;
	struct fimc_is_crop incrop_cfg, otcrop_cfg;
	u32 in_width, in_height;

	device = (struct fimc_is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	group = &device->group_3aa;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	ret = fimc_is_sensor_g_module(device->sensor, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		queue = GET_SUBDEV_QUEUE(leader);
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

		hw_format = queue->framecfg.format->hw_format;
		hw_bitwidth = queue->framecfg.format->hw_bitwidth; /* memory width per pixel */
	}

	/* Configure Conrtol */
	if (!frame) {
		control = fimc_is_itf_g_param(device, NULL, PARAM_3AA_CONTROL);
		if (test_bit(FIMC_IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}
		*lindex |= LOWBIT_OF(PARAM_3AA_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_3AA_CONTROL);
		(*indexes)++;
	}

	/*+  * When 3AA receives the OTF out data from sensor,+  * it should refer the sensor size for input full size.+	 */
	head = GET_HEAD_GROUP_IN_DEVICE(FIMC_IS_DEVICE_ISCHAIN, group);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state)) {
		in_width = fimc_is_sensor_g_bns_width(device->sensor);
		in_height = fimc_is_sensor_g_bns_height(device->sensor);
	} else {
		in_width = leader->input.width;
		in_height = leader->input.height;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		fimc_is_ischain_3aa_stripe_cfg(leader, frame,
				&in_width, &in_height,
				&incrop_cfg, &otcrop_cfg);

	/*
	 * bayer crop = bcrop1 + bcrop3
	 * hal should set full size input including cac margin
	 * and then full size is decreased as cac margin by driver internally
	 * size of 3AP take over only setting for BDS
	 */

	otf_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_OTF_INPUT);
	if (group->head->id == GROUP_ID_PAF0 || group->head->id == GROUP_ID_PAF1)
		otf_input->source = OTF_INPUT_PAF_RDMA_PATH;
	otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;	/* in Ramen, 3AA has only OTF input */
	otf_input->width = in_width;
	otf_input->height = in_height;
	otf_input->format = OTF_INPUT_FORMAT_BAYER;
	otf_input->bitwidth = OTF_INPUT_BIT_WIDTH_12BIT;
	otf_input->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	otf_input->bayer_crop_offset_x = incrop_cfg.x;
	otf_input->bayer_crop_offset_y = incrop_cfg.y;
	otf_input->bayer_crop_width = incrop_cfg.w;
	otf_input->bayer_crop_height = incrop_cfg.h;
	*lindex |= LOWBIT_OF(PARAM_3AA_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_OTF_INPUT);
	(*indexes)++;

	dma_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA1_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = module->bitwidth - 1; /* msb zero padding by HW constraint */
	dma_input->order = DMA_INPUT_ORDER_GR_BG;
	dma_input->plane = 1;
	dma_input->width = leader->input.width;
	dma_input->height = leader->input.height;
#if defined(ENABLE_3AA_DMA_CROP)
	dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
#else
	dma_input->dma_crop_offset = 0;
	dma_input->dma_crop_width = leader->input.width;
	dma_input->dma_crop_height = leader->input.height;
	dma_input->bayer_crop_offset_x = incrop_cfg.x;
	dma_input->bayer_crop_offset_y = incrop_cfg.y;
#endif
	dma_input->bayer_crop_width = incrop_cfg.w;
	dma_input->bayer_crop_height = incrop_cfg.h;
	dma_input->orientation = DMA_INPUT_ORIENTATION_NORMAL;
#ifdef ENABLE_REMOSAIC_CAPTURE_WITH_ROTATION
	/* if rotation remosaic frame reprocessing is doing, set "CCW(1)" as default,
	 * TODO: get orientation value through interface if needed
	 */
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)
		&& (frame && CHK_REMOSAIC_SCN(frame->shot->ctl.aa.captureIntent))) {
		if (frame->shot_ext->remosaic_rotation)
			dma_input->orientation = DMA_INPUT_ORIENTATION_CCW;
		else
			dma_input->orientation = DMA_INPUT_ORIENTATION_NORMAL;
		msrinfo("DMA rotate(%d) for REMOSAIC\n", device, leader, frame, dma_input->orientation);
	}
#endif
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA1_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA1_INPUT);
	(*indexes)++;

	/* if 3aap is not start then otf output should be enabled */
	otf_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_OTF_OUTPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
#ifdef USE_3AA_CROP_AFTER_BDS
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state) && (otf_input->source != OTF_INPUT_PAF_RDMA_PATH)) {
		otf_output->width = otcrop_cfg.w;
		otf_output->height = otcrop_cfg.h;
		otf_output->crop_enable = 0;
	} else {
		otf_output->width = incrop_cfg.w;
		otf_output->height = incrop_cfg.h;
		otf_output->crop_offset_x = otcrop_cfg.x;
		otf_output->crop_offset_y = otcrop_cfg.y;
		otf_output->crop_width = otcrop_cfg.w;
		otf_output->crop_height = otcrop_cfg.h;
		otf_output->crop_enable = 1;
	}
#else
	otf_output->width = otcrop_cfg.w;
	otf_output->height = otcrop_cfg.h;
	otf_output->crop_enable = 0;
#endif
	otf_output->format = OTF_OUTPUT_FORMAT_BAYER;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_OUTPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_3AA_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_OTF_OUTPUT);
	(*indexes)++;

	stripe_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_STRIPE_INPUT);
	if (frame && frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		stripe_input->overlap_width = STRIPE_MARGIN_WIDTH;
		stripe_input->full_width = leader->input.width;
		stripe_input->full_height = leader->input.height;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->overlap_width = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}
	*lindex |= LOWBIT_OF(PARAM_3AA_STRIPE_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_STRIPE_INPUT);
	(*indexes)++;

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int fimc_is_ischain_3aa_tag(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct taa_param *taa_param;
	struct fimc_is_crop inparm, otparm;
	struct fimc_is_crop *incrop, *otcrop;
	struct fimc_is_subdev *leader;
	struct fimc_is_device_ischain *device;
	u32 lindex, hindex, indexes;

	device = (struct fimc_is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "3AA TAG\n", device);

	incrop = (struct fimc_is_crop *)node->input.cropRegion;
	otcrop = (struct fimc_is_crop *)node->output.cropRegion;

	group = &device->group_3aa;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	taa_param = &device->is_region->parameter.taa;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = taa_param->otf_input.bayer_crop_offset_x;
		inparm.y = taa_param->otf_input.bayer_crop_offset_y;
		inparm.w = taa_param->otf_input.bayer_crop_width;
		inparm.h = taa_param->otf_input.bayer_crop_height;
	} else {
		inparm.x = taa_param->vdma1_input.bayer_crop_offset_x;
		inparm.y = taa_param->vdma1_input.bayer_crop_offset_y;
		inparm.w = taa_param->vdma1_input.bayer_crop_width;
		inparm.h = taa_param->vdma1_input.bayer_crop_height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (test_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state)) {
#ifdef	USE_3AA_CROP_AFTER_BDS
		otparm.x = taa_param->otf_output.crop_offset_x;
		otparm.y = taa_param->otf_output.crop_offset_y;
		otparm.w = taa_param->otf_output.crop_width;
		otparm.h = taa_param->otf_output.crop_height;
#else
		otparm.x = 0;
		otparm.y = 0;
		otparm.w = taa_param->otf_output.width;
		otparm.h = taa_param->otf_output.height;
#endif
	} else {
		otparm.x = otcrop->x;
		otparm.y = otcrop->y;
		otparm.w = otcrop->w;
		otparm.h = otcrop->h;
	}

	if (IS_NULL_CROP(otcrop)) {
		msrwarn("ot_crop [%d, %d, %d, %d] -> [%d, %d, %d, %d]\n", device, subdev, frame,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h,
			otparm.x, otparm.y, otparm.w, otparm.h);
		*otcrop = otparm;
	}

	if (!COMPARE_CROP(incrop, &inparm) ||
		!COMPARE_CROP(otcrop, &otparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		!atomic_read(&group->head->scount) ||
		test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = fimc_is_ischain_3aa_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("fimc_is_ischain_3aa_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			incrop->x, incrop->y, incrop->w, incrop->h);
		msrinfo("ot_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	}

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_subdev_ops fimc_is_subdev_3aa_ops = {
	.bypass			= NULL,
	.cfg			= fimc_is_ischain_3aa_cfg,
	.tag			= fimc_is_ischain_3aa_tag,
};
