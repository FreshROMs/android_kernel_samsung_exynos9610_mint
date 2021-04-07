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
#include "vipx-time.h"

void vipx_time_get_timestamp(struct vipx_time *time, int opt)
{
	vipx_enter();
	if (opt == TIMESTAMP_START)
		getrawmonotonic(&time->start);
	else if (opt == TIMESTAMP_END)
		getrawmonotonic(&time->end);
	else
		vipx_warn("time opt is invaild(%d)\n", opt);
	vipx_leave();
}

void vipx_time_get_interval(struct vipx_time *time)
{
	time->interval = timespec_sub(time->end, time->start);
}

void vipx_time_print(struct vipx_time *time, const char *f, ...)
{
	char buf[128];
	int len, size;
	va_list args;

	vipx_enter();
	vipx_time_get_interval(time);

	len = snprintf(buf, sizeof(buf), "[%2lu.%09lu sec] ",
			time->interval.tv_sec, time->interval.tv_nsec);
	if (len < 0) {
		vipx_warn("Failed to print time\n");
		return;
	}
	size = len;

	va_start(args, f);
	len = vsnprintf(buf + size, sizeof(buf) - size, f, args);
	if (len > 0)
		size += len;
	va_end(args);
	if (buf[size - 1] != '\n')
		buf[size - 1] = '\n';

	vipx_info("%s", buf);
	vipx_leave();
}
