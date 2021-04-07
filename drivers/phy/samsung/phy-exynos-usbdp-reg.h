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

#ifndef _PHY_EXYNOS_USBDP_REG_H_
#define _PHY_EXYNOS_USBDP_REG_H_

#define USBDP_COMBO_BIT_MASK(_bw)		((1 << _bw) - 1)

#define USBDP_COMBO_REG_MSK(_pos, _B)		(USBDP_COMBO_BIT_MASK(_B) << _pos)
#define USBDP_COMBO_REG_CLR(_pos, _B)		(~(USBDP_COMBO_BIT_MASK(_B) << _pos))
#define USBDP_COMBO_REG_SET(_x, _pos, _B)	((_x & USBDP_COMBO_BIT_MASK(_B)) << _pos)
#define USBDP_COMBO_REG_GET(_x, _pos, _B)	((_x & (USBDP_COMBO_BIT_MASK(_B) << _pos)) >> _pos)

#include "phy-exynos-usbdp-reg-cmn.h"
#include "phy-exynos-usbdp-reg-trsv.h"
#include "phy-exynos-usbdp-reg-dp.h"
#include "phy-exynos-usbdp-reg-pcs.h"

#endif /* _PHY_EXYNOS_USBDP_REG_H_ */
