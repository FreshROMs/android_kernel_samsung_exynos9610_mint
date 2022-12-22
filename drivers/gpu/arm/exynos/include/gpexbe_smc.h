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

#ifndef _GPEXBE_SMC_H_
#define _GPEXBE_SMC_H_

/**
 * gpexbe_smc_protection_enable() - enter secure mode using exynos_smc
 *
 * Used by gpexbe_secure module
 *
 * Return: 0 on success
 */
int gpexbe_smc_protection_enable(void);

/**
 * gpexbe_smc_protection_disable() - exit secure mode using exynos_smc
 *
 * Used by gpexbe_secure module
 *
 * Return: 0 on success
 */
int gpexbe_smc_protection_disable(void);

/**
 * gpexbe_smc_notify_power_on() - Notify smc whenever GPU is powered on
 *
 * Needed by some SOC (used if MALI_EXYNOS_SECURE_SMC_NOTIFY_GPU is enabled)
 *
 */
void gpexbe_smc_notify_power_on(void);

/**
 * gpexbe_smc_notify_power_off() - Notify smc whenever GPU is powered off
 *
 * Needed by some SOC (used if MALI_EXYNOS_SECURE_SMC_NOTIFY_GPU is enabled)
 */
void gpexbe_smc_notify_power_off(void);

/**
 * gpexbe_smc_init() - initializes gpexbe_smc backend module
 *
 * Return: 0 on success
 */
int gpexbe_smc_init(void);

/**
 * gpexbe_smc_term() - terminates gpexbe_smc backend module
 *
 * Return: 0 on success
 */
void gpexbe_smc_term(void);

#endif
