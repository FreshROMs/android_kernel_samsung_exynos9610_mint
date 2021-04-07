/* linux/arch/arm/mach-exynos/include/mach/secos_booster.h
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* Header file for secure OS booster API
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef __LINUX_SECOS_BOOST_H__
#define __LINUX_SECOS_BOOST_H__

/*
 * Secure OS Boost Policy
 */
enum secos_boost_policy {
	MAX_PERFORMANCE,
	MID_PERFORMANCE,
	MIN_PERFORMANCE,
	STB_PERFORMANCE,
	PERFORMANCE_MAX_CNT,
};

int secos_booster_start(enum secos_boost_policy policy);
int secos_booster_stop(void);

#endif
