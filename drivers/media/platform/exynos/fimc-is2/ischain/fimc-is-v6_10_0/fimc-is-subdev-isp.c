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

void fimc_is_ischain_isp_stripe_cfg(struct fimc_is_subdev *subdev,
		struct fimc_is_frame *frame,
		struct fimc_is_crop *incrop,
		struct fimc_is_crop *otcrop,
		struct fimc_is_fmt *fmt)
{
	struct fimc_is_groupmgr *groupmgr = (struct fimc_is_groupmgr *)frame->groupmgr;
	struct fimc_is_group *group = frame->group;
	struct fimc_is_group *stream_leader = groupmgr->leader[subdev->instance];
#ifdef CHAIN_USE_STRIPE_PROCESSING
	struct camera2_stream *stream = (struct camera2_stream *) frame->shot_ext;
#endif
	u32 region_id = frame->stripe_info.region_id;
	u32 stripe_w, dma_offset= 0;

	/* Input crop configuration */
	if (!region_id) {
		/* Left region */
#ifdef CHAIN_USE_STRIPE_PROCESSING
		if (stream->stripe_h_pix_nums[region_id]) {
			stripe_w = stream->stripe_h_pix_nums[region_id];
		} 
		else
#endif
		{
			stripe_w = ALIGN(incrop->w / frame->stripe_info.region_num, 2);
			stripe_w = ALIGN_DOWN(stripe_w, STRIPE_WIDTH_ALIGN);
		}

		frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
		frame->stripe_info.in.h_pix_num = stripe_w;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;

		/* Consider RDMA offset. */
		if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			if (stream_leader->id == group->id) {
				/**
				 * ISP reads the right region of original bayer image.
				 * Add horizontal DMA offset only.
				 */
				dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
				dma_offset = dma_offset * fmt->bitsperpixel[0] / BITS_PER_BYTE;
				msrwarn("Processed bayer reprocessing is NOT supported by stripe processing",
						subdev, subdev, frame);

			} else {
				/**
				 * ISP reads the right region with stripe margin.
				 * Add horizontal & vertical DMA offset.
				 */
				dma_offset = frame->stripe_info.in.h_pix_num + STRIPE_MARGIN_WIDTH;
				dma_offset = dma_offset * fmt->bitsperpixel[0] / BITS_PER_BYTE;
				dma_offset *= otcrop->h;

			}
		}
	}

	stripe_w += STRIPE_MARGIN_WIDTH;

	incrop->w = stripe_w;

	/**
	 * Output crop configuration.
	 * No crop & scale.
	 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	frame->dvaddr_buffer[0] += dma_offset;

	mdbg_pframe("stripe_in_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, frame,
			region_id, incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	mdbg_pframe("stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			region_id, otcrop->x, otcrop->y, otcrop->w, otcrop->h);
}

static int fimc_is_ischain_isp_cfg(struct fimc_is_subdev *leader,
	void *device_data,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_input *dma_input;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct fimc_is_crop *scc_incrop;
	struct fimc_is_crop *scp_incrop;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_ischain *device;
	u32 hw_format = DMA_INPUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INPUT_BIT_WIDTH_16BIT;
	struct fimc_is_crop incrop_cfg, otcrop_cfg;

	device = (struct fimc_is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	scc_incrop = scp_incrop = incrop;
	group = &device->group_isp;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		if (!queue->framecfg.format) {
			merr("format is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		hw_format = queue->framecfg.format->hw_format;
		hw_bitwidth = queue->framecfg.format->hw_bitwidth; /* memory width per pixel */
	}

	ret = fimc_is_sensor_g_module(device->sensor, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	/* Configure Control */
	if (!frame) {
		control = fimc_is_itf_g_param(device, NULL, PARAM_ISP_CONTROL);
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_ISP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_ISP_CONTROL);
		(*indexes)++;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		fimc_is_ischain_isp_stripe_cfg(leader, frame,
				&incrop_cfg, &otcrop_cfg,
				queue->framecfg.format);

	/* ISP */
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_ISP_OTF_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	otf_input->width = incrop_cfg.w;
	otf_input->height = incrop_cfg.h;
	otf_input->format = OTF_INPUT_FORMAT_BAYER;
	otf_input->bayer_crop_offset_x = 0;
	otf_input->bayer_crop_offset_y = 0;
	otf_input->bayer_crop_width = incrop_cfg.w;
	otf_input->bayer_crop_height = incrop_cfg.h;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_INPUT);
	(*indexes)++;

	dma_input = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA1_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = MSB_OF_3AA_DMA_OUT;
	dma_input->width = incrop_cfg.w;
	dma_input->height = incrop_cfg.h;
	dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
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
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA1_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA1_INPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_OTF_OUTPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
	otf_output->width = incrop_cfg.w;
	otf_output->height = incrop_cfg.h;
	otf_output->format = OTF_YUV_FORMAT;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_OUTPUT);
	(*indexes)++;

#ifdef SOC_DRC
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_DRC_OTF_INPUT);
	otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	otf_input->width = incrop_cfg.w;
	otf_input->height = incrop_cfg.h;
	*lindex |= LOWBIT_OF(PARAM_DRC_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_DRC_OTF_INPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_DRC_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_output->width = incrop_cfg.w;
	otf_output->height = incrop_cfg.h;
	*lindex |= LOWBIT_OF(PARAM_DRC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_DRC_OTF_OUTPUT);
	(*indexes)++;
#endif

	stripe_input = fimc_is_itf_g_param(device, frame, PARAM_ISP_STRIPE_INPUT);
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
	*lindex |= LOWBIT_OF(PARAM_ISP_STRIPE_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_STRIPE_INPUT);
	(*indexes)++;

#ifdef SOC_SCC
	if (group->subdev[ENTRY_SCC]) {
		CALL_SOPS(group->subdev[ENTRY_SCC], cfg, device, frame, scc_incrop, NULL, lindex, hindex, indexes);
		scp_incrop = &device->scc.output.crop;
	}
#endif

#ifdef SOC_SCP
	if (group->subdev[ENTRY_SCP])
		CALL_SOPS(group->subdev[ENTRY_SCP], cfg, device, frame, scp_incrop, NULL, lindex, hindex, indexes);
#endif

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int fimc_is_ischain_isp_tag(struct fimc_is_subdev *subdev,
	void *device_data,
	struct fimc_is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct isp_param *isp_param;
	struct fimc_is_crop inparm;
	struct fimc_is_crop *incrop, *otcrop;
	struct fimc_is_subdev *leader;
	struct fimc_is_device_ischain *device;
	u32 lindex, hindex, indexes;

	device = (struct fimc_is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "ISP TAG\n", device);

	incrop = (struct fimc_is_crop *)node->input.cropRegion;
	otcrop = (struct fimc_is_crop *)node->output.cropRegion;

	group = &device->group_isp;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	isp_param = &device->is_region->parameter.isp;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = isp_param->otf_input.width;
		inparm.h = isp_param->otf_input.height;
	} else {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = isp_param->vdma1_input.width;
		inparm.h = isp_param->vdma1_input.height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		!atomic_read(&group->head->scount) ||
		test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = fimc_is_ischain_isp_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("fimc_is_ischain_isp_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
	}

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_subdev_ops fimc_is_subdev_isp_ops = {
	.bypass			= NULL,
	.cfg			= fimc_is_ischain_isp_cfg,
	.tag			= fimc_is_ischain_isp_tag,
};
