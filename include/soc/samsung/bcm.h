/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_BCM_H_
#define __EXYNOS_BCM_H_

struct bcm_info;

struct output_data {
	int index;
	u32 rd0;
	u32 rd1;
	u32 rd2;
	u64 rd3;
	u64 rd4;
	u64 rd5;
};


#if defined(CONFIG_EXYNOS_BCM)
int bcm_pd_sync(struct bcm_info *, bool);
struct output_data *bcm_start(const int *);
struct output_data *bcm_stop(const int *);
#else

#define bcm_pd_sync(a, b) do {} while (0)
#define bcm_start(a) do {} while (0)
#define bcm_stop(a) do {} while (0)
#endif

#endif
