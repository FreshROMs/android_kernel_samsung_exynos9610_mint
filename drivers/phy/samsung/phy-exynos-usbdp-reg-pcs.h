/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Sung-Hyun Na <sunghyun.na@samsung.com>
 *
 * Chip Abstraction Layer for USB PHY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAL_PHY_EXYNOS_USBDP_REG_PCS_H_
#define _CAL_PHY_EXYNOS_USBDP_REG_PCS_H_

#define USBDP_PCSREG_FRONT_END_MODE_VEC		0x0200
#define USBDP_PCSREG_RUN_LENGTH_TH			USBDP_COMBO_REG_MSK(4, 1)
#define USBDP_PCSREG_EN_DRAIN_AFTER_RX_VAL_FALL		USBDP_COMBO_REG_MSK(1, 1)
#define USBDP_PCSREG_EN_REALIGN				USBDP_COMBO_REG_MSK(0, 1)

#define USBDP_PCSREG_DET_COMP_EN_SET		0x0300
#define USBDP_PCSREG_COMP_EN_ASSERT_MASK		USBDP_COMBO_REG_MSK(0, 6)
#define USBDP_PCSREG_COMP_EN_ASSERT_SET(_x)		USBDP_COMBO_REG_SET(_x, 0, 6)
#define USBDP_PCSREG_COMP_EN_ASSERT_GET(_x)		USBDP_COMBO_REG_GET(_R, 0, 6)




#endif /* _CAL_PHY_EXYNOS_USBDP_REG_PCS_H_ */
