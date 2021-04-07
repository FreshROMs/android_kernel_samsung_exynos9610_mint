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
#include "vipx-debug.h"
#include "vipx-io.h"

void *vipx_io_copy_mem2io(void *dst, void *src, size_t size)
{
	unsigned char *src8;
	unsigned char *dst8;

	src8 = (unsigned char *)(src);
	dst8 = (unsigned char *)(dst);

	/* first copy byte by byte till the source first alignment
	 * this step is necessary to ensure we do not even try to access
	 * data which is before the source buffer, hence it is not ours.
	 */
	/* complete the left overs */
	while (size--) {
		IOW8(*dst8, *src8);
		dst8++;
		src8++;
	}

	return dst;
}

void *vipx_io_copy_io2mem(void *dst, void *src, size_t size)
{
	unsigned char *src8;
	unsigned char *dst8;

	src8 = (unsigned char *)(src);
	dst8 = (unsigned char *)(dst);

	while (size--) {
		*dst8 = IOR8(*src8);
		dst8++;
		src8++;
	}

	return dst;
}
