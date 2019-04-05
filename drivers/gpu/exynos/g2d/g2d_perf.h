/*
 * linux/drivers/gpu/exynos/g2d/g2d_perf.h
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _G2D_PERF_H_
#define _G2D_PERF_H_

struct g2d_context;
struct g2d_performance_data;

#define perf_index_fmt(layer) \
		((((layer)->layer_attr) & G2D_PERF_LAYER_FMTMASK) >> 4)
#define perf_index_rotate(layer) \
		(((layer)->layer_attr) & G2D_PERF_LAYER_ROTATE)
#define is_perf_frame_colorfill(frame) \
		(((frame)->frame_attr) & G2D_PERF_FRAME_SOLIDCOLORFILL)

#define BTS_PEAK_FPS_RATIO 1667

u32 g2d_calc_device_frequency(struct g2d_device *g2d_dev,
			      struct g2d_performance_data *data);
void g2d_update_performance(struct g2d_device *g2d_dev);

#endif /* _G2D_PERF_H_ */
