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

#include <mali_kbase.h>
#include <mali_kbase_config_defaults.h>
#include <mali_kbase_pm.h>

/**
 * The number of times the adaptive power off hysteresis timer can fire without
 * an intervening GPU active period before power off hysteresis is set to 0.
 */
#ifndef DEFAULT_PM_ADAPTIVE_HYSTERESIS
#define DEFAULT_PM_ADAPTIVE_HYSTERESIS (2)
#endif

static bool adaptive_shaders_needed(struct kbase_device *kbdev)
{
    return kbase_pm_is_active(kbdev);
}

static bool adaptive_get_core_active(struct kbase_device *kbdev)
{
    return kbase_pm_is_active(kbdev);
}

static void adaptive_init(struct kbase_device *kbdev)
{
    struct kbasep_pm_tick_timer_state *stt = &kbdev->pm.backend.shader_tick_timer;
    struct kbasep_pm_policy_adaptive *data = &kbdev->pm.backend.pm_policy_data.adaptive;

    stt->configured_ticks = stt->default_ticks;
    data->delay = DEFAULT_PM_ADAPTIVE_HYSTERESIS;
}

static void adaptive_term(struct kbase_device *kbdev)
{
    struct kbasep_pm_tick_timer_state *stt = &kbdev->pm.backend.shader_tick_timer;
    stt->configured_ticks = stt->default_ticks;
}

static void adaptive_handle_event(struct kbase_device *kbdev, enum kbase_pm_policy_event event)
{
    u64 threshold;
    struct kbasep_pm_tick_timer_state *stt = &kbdev->pm.backend.shader_tick_timer;
    struct kbasep_pm_policy_adaptive *data = &kbdev->pm.backend.pm_policy_data.adaptive;

    switch (event)
    {
    case KBASE_PM_POLICY_EVENT_IDLE:
        data->last_idle = ktime_get_ns();
        break;

    case KBASE_PM_POLICY_EVENT_POWER_ON:
        threshold = stt->default_ticks * ktime_to_ns(stt->configured_interval);
        if (ktime_get_ns() - data->last_idle < threshold)
            stt->configured_ticks = stt->default_ticks;
        break;

    case KBASE_PM_POLICY_EVENT_TIMER_HIT:
        data->delay = DEFAULT_PM_ADAPTIVE_HYSTERESIS;
        break;

    case KBASE_PM_POLICY_EVENT_TIMER_MISS:
        if (data->delay-- == 0) {
            stt->configured_ticks = 0;
            data->delay = DEFAULT_PM_ADAPTIVE_HYSTERESIS;
        }
        break;
    }
}

/* The struct kbase_pm_policy structure for the adaptive power policy.
 *
 * This is the static structure that defines the adaptive power policy's callback
 * and name.
 */
const struct kbase_pm_policy kbase_pm_adaptive_policy_ops = {
    "adaptive",         /* name */
    adaptive_init,          /* init */
    adaptive_term,          /* term */
    adaptive_shaders_needed,    /* shaders_needed */
    adaptive_get_core_active,   /* get_core_active */
    adaptive_handle_event,      /* handle_event */
    KBASE_PM_POLICY_ID_ADAPTIVE,    /* id */
#if MALI_USE_CSF
    0u,                         /* flags */
#endif
};

KBASE_EXPORT_TEST_API(kbase_pm_adaptive_policy_ops);
