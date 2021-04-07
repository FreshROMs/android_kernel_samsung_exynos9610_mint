/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_TIME_H__
#define __VIPX_TIME_H__

#include <linux/ktime.h>

#define TIMESTAMP_START		(1 << 0)
#define TIMESTAMP_END		(1 << 1)

enum vipx_time_measure_point {
	TIME_LOAD_GRAPH,
	TIME_EXECUTE_GRAPH,
	TIME_UNLOAD_GRAPH,
	TIME_COUNT
};

struct vipx_time {
	struct timespec		start;
	struct timespec		end;
	struct timespec		interval;
};

void vipx_time_get_timestamp(struct vipx_time *time, int opt);
void vipx_time_get_interval(struct vipx_time *time);
void vipx_time_print(struct vipx_time *time, const char *f, ...);

#endif
