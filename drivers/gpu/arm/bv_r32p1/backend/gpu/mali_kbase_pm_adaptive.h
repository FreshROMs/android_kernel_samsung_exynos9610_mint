// SPDX-License-Identifier: GPL-2.0
/*
 *
 * (C) COPYRIGHT 2012-2016, 2018-2020 ARM Limited. All rights reserved.
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
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

/*
 * "Adaptive" power management policy
 *
 * Copyright 2021 Google LLC
 *
 * Author: Sidath Senanayake <sidaths@google.com>
 */

#ifndef MALI_KBASE_PM_ADAPTIVE_H
#define MALI_KBASE_PM_ADAPTIVE_H

/**
 * struct kbasep_pm_policy_adaptive - Private structure for the adaptive policy
 *
 * This contains data that is private to the adaptive power policy.
 *
 * @last_idle: The last time that the GPU went idle.
 * @delay:     The remaining number of timer misses before the power down
 *             hysteresis timer is disabled.
 */
struct kbasep_pm_policy_adaptive {
    u64 last_idle;
    int delay;
};

extern const struct kbase_pm_policy kbase_pm_adaptive_policy_ops;

#endif /* MALI_KBASE_PM_ADAPTIVE_H */
