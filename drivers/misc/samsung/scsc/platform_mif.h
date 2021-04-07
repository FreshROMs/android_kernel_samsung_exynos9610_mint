/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __PLATFORM_MIF_H
#define __PLATFORM_MIF_H
#include "scsc_mif_abs.h"

enum wlbt_irqs {
       PLATFORM_MIF_MBOX,
       PLATFORM_MIF_ALIVE,
       PLATFORM_MIF_WDOG,
#if defined(CONFIG_SOC_EXYNOS9610) || defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS3830) || defined(CONFIG_SOC_S5E9815)
       PLATFORM_MIF_CFG_REQ,
#endif
       /* must be last */
       PLATFORM_MIF_NUM_IRQS
};

struct platform_device;

struct scsc_mif_abs    *platform_mif_create(struct platform_device *pdev);
void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface);
struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface);
struct device          *platform_mif_get_dev(struct scsc_mif_abs *interface);
int platform_mif_suspend(struct scsc_mif_abs *interface);
void platform_mif_resume(struct scsc_mif_abs *interface);

#endif
