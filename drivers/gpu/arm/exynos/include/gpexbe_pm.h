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

#ifndef _GPEXBE_PM_H_
#define _GPEXBE_PM_H_

/**
 * gpexbe_pm_init() - initializes power management backend module
 *
 * Return: 0 on successs
 */
int gpexbe_pm_init(void);

/**
 * gpexbe_pm_term() - terminates power management backend module
 */
void gpexbe_pm_term(void);

/**
 * gpexbe_pm_access_lock() - acquire pm mutex
 */
void gpexbe_pm_access_lock(void);

/**
 * gpexbe_pm_access_unlock() - release pm mutex
 */
void gpexbe_pm_access_unlock(void);

/**
 * gpexbe_pm_pd_control_up() - power up the GPU for IFPO
 *
 * deprecated since IFPO is not used anymore
 *
 * Return: 0 on success
 */
int gpexbe_pm_pd_control_up(void);

/**
 * gpexbe_pm_pd_control_down() - power down the GPU for IFPO
 *
 * deprecated since IFPO is not used anymore
 *
 * Return: 0 on success
 */
int gpexbe_pm_pd_control_down(void);

/**
 * gpexbe_pm_get_status() - get GPU power status from the PMU
 *
 * Return: 1 if power is ON, 0 if power is OFF
 */
int gpexbe_pm_get_status(void);

/**
 * gpexbe_pm_get_exynos_pm_domain() - get gpu pm domain (for use by TSG module)
 *
 * This function can be deprecated since TSG only checks if gpu pm domain is present
 *
 * Return: pointer to gpu pm domain struct
 */
struct exynos_pm_domain *gpexbe_pm_get_exynos_pm_domain(void);

#endif /* _GPEXBE_PM_H_ */
