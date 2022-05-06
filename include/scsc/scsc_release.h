/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef _SCSC_RELEASE_H
#define _SCSC_RELEASE_H

#ifdef CONFIG_SOC_EXYNOS3830
#define SCSC_RELEASE_SOLUTION "mx152"
#elif defined(CONFIG_SOC_EXYNOS9630)
#define SCSC_RELEASE_SOLUTION "mx450"
#elif defined(CONFIG_SOC_EXYNOS9610)
#define SCSC_RELEASE_SOLUTION "mx250"
#elif defined(CONFIG_SOC_EXYNOS7885)
#define SCSC_RELEASE_SOLUTION "mx150"
#elif defined(CONFIG_SOC_S5E9815)
#define SCSC_RELEASE_SOLUTION "mx452"
#else
#define SCSC_RELEASE_SOLUTION "wlbt"
#endif



#define SCSC_RELEASE_PRODUCT 11
#define SCSC_RELEASE_ITERATION 19
#define SCSC_RELEASE_CANDIDATE 0

#define SCSC_RELEASE_POINT 18
#define SCSC_RELEASE_CUSTOMER 0

#endif


