/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Samsung Graphics 2D driver
 */

#ifndef __EXYNOS_G2D_SECURE_H__
#define __EXYNOS_G2D_SECURE_H__

#include <linux/arm-smccc.h>

static inline int g2d_smc(unsigned long cmd, unsigned long arg0,
			  unsigned long arg1, unsigned long arg2)
{
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	struct arm_smccc_res res;

	arm_smccc_smc(cmd, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return (int)res.a0;
#else
	return 0; /* DRMDRV_OK */
#endif
}

#define SMC_DRM_G2D_CMD_DATA            0x8200202d
#define SMC_PROTECTION_SET		0x82002010

#define G2D_ALWAYS_S 37

#endif /* __EXYNOS_G2D_SECURE_H__ */
