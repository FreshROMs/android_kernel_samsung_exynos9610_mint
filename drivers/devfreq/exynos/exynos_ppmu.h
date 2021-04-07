/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS PPMU header
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DEVFREQ_EXYNOS_PPMU_H
#define __DEVFREQ_EXYNOS_PPMU_H __FILE__

#include <linux/ktime.h>

struct ppmu_data {
	u64 ccnt;
	u64 pmcnt0;
	u64 pmcnt1;
	u64 pmcnt2;
	u64 pmcnt3;
};

#define exynos_read_ppmu(a, ...) do {} while(0)
#define exynos_init_ppmu(a, ...) do {} while(0)
#define exynos_exit_ppmu(a, ...) do {} while(0)
#define exynos_reset_ppmu(a, ...) do {} while(0)
#define exynos_start_ppmu(a, ...) do {} while(0)
#define exynos_stop_ppmu(a, ...) do {} while(0)

#endif /* __DEVFREQ_EXYNOS_PPMU_H */
