/* SPDX-License-Identifier: GPL-2.0 */

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef _MALI_EXYNOS_BTS_BACKEND_H_
#define _MALI_EXYNOS_BTS_BACKEND_H_

#include <linux/device.h>

/**
 * gpexbe_bts_init() - initializes BTS MO backend module
 *
 * Return: 0 on success
 */
int gpexbe_bts_init(void);

/**
 * gpexbe_bts_term() - terminates BTS MO backend module
 */
void gpexbe_bts_term(void);

/**
 * gpexbe_bts_set_bts_mo() - set BTS MO to given value
 * @val: currently only accepts 1 or 0 (enable/disable)
 *
 * For now, BTS MO can only be enabled or disabled to predetermined values set
 * in devicetree.
 *
 * Return: 0 on success
 */
int gpexbe_bts_set_bts_mo(int val);

#endif /* _MALI_EXYNOS_BTS_BACKEND_H_ */
