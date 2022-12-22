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

#ifndef _MALI_EXYNOS_PLATFORM_H_
#define _MALI_EXYNOS_PLATFORM_H_

#include <linux/device.h>
#include <linux/types.h>

/**
 * gpex_platform_init() - initializes gpex_platform which initializes all other modules
 *
 * This function is called when mali driver is first loaded and it initializes all other
 * mali integration layer modules (gpex, gpexbe and gpexwa) that are enabled on the system.
 *
 * Return: 0 on success
 */
int gpex_platform_init(struct device **dev);

/**
 * gpex_platform_term() - terminates gpex_platform
 */
void gpex_platform_term(void);

#endif /* _MALI_EXYNOS_PLATFORM_H_ */
