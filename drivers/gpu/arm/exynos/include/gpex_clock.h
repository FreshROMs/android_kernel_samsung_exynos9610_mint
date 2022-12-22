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

#ifndef _MALI_EXYNOS_CLOCK_H_
#define _MALI_EXYNOS_CLOCK_H_

#include <mali_kbase.h>
#include <linux/device.h>
typedef enum {
	GPU_CLOCK_MAX_LOCK = 0,
	GPU_CLOCK_MIN_LOCK,
	GPU_CLOCK_MAX_UNLOCK,
	GPU_CLOCK_MIN_UNLOCK,
} gpex_clock_lock_cmd_t;

typedef enum {
	TMU_LOCK = 0,
	SYSFS_LOCK,
	PMQOS_LOCK,
	CLBOOST_LOCK,
	INTERACTIVE_LOCK,
	MM_LOCK,
	NUMBER_LOCK
} gpex_clock_lock_type_t;

/*************************************
 * INIT/TERM
 ************************************/

/**
 * gpex_clock_init() - initializes gpex_clock module
 * @dev: mali device struct
 *
 * Return: 0 on success
 */
int gpex_clock_init(struct device **dev);

/**
 * gpex_clock_term() - terminates gpex_clock module
 */
void gpex_clock_term(void);

/*************************************
 * CALLBACKS
 ************************************/

/**
 * gpex_clock_prepare_runtime_off() - prepare for runtime power off
 *
 * Prepare for runtime off by updating time in state and applying
 * SOC specific work-arounds.
 *
 * Return: 0 on success
 */
int gpex_clock_prepare_runtime_off(void);

/*************************************
 * SETTERS
 ************************************/

/**
 * gpex_clock_set() - set gpu clock and trigger side-effects such as QOS requests
 * @clk: target gpu clock
 *
 * Side-effects include QOS, BTS and LLC toggle depending on GPU clock requested.
 * CLBoost and TSG, GTS are also considered.
 * For full list of side effects, I recommend looking at the code as it can change
 * with further driver development.
 *
 * Return: 0 on success
 */
int gpex_clock_set(int clk);

/**
 * gpex_clock_lock_clock() - set GPU clock max or min lock
 * @lock_command: requested command max/min lock/unlock
 * @lock_type: requested lock type (thermal, sysfs, qos, clboost etc)
 * @clock : lock clock
 *
 * There can be multiple lock types, each being set and unset independently.
 * For example, locking and unlocking max gpu clock using sysfs will not remove
 * max gpu clock lock set by thermal/TMU
 *
 * Return: 0 on success
 */
int gpex_clock_lock_clock(gpex_clock_lock_cmd_t lock_command, gpex_clock_lock_type_t lock_type,
			  int clock);

/*************************************
 * GETTERS
 ************************************/

/**
 * gpex_clock_get_clock_slow() - get current GPU clock (actual read)
 *
 * Checks actual GPU clock instead of relying on stored value in the DDK
 *
 * Return: current GPU clock in Khz
 */
int gpex_clock_get_clock_slow(void);

/**
 * gpex_clock_get_table_idx() - get the array index of the given clock in the clock table
 * @clock: clock to check the index for
 *
 * Higher clocks are stored in lower index
 *
 * Return: array index of the given clock. -1 on error
 */
int gpex_clock_get_table_idx(int clock);

/**
 * gpex_clock_get_boot_clock() - get the GPU boot clock
 *
 * Return: GPU boot clock in Khz
 */
int gpex_clock_get_boot_clock(void);

/**
 * gpex_clock_get_max_clock() - get the GPU max clock set in mali devicetree
 *
 * Return: GPU max clock in Khz (from mali device tree)
 */
int gpex_clock_get_max_clock(void);

/**
 * gpex_clock_get_max_clock_limit() - get the GPU max clock from cal-if (from bl2)
 *
 * Return: GPU max clock in Khz (from cal-if)
 */
int gpex_clock_get_max_clock_limit(void);

/**
 * gpex_clock_get_min_clock() - get the GPU min clock from mali devicetree
 *
 * Return: GPU min clock in Khz (from mali device tree)
 */
int gpex_clock_get_min_clock(void);

/**
 * gpex_clock_get_cur_clock() - get the last known GPU clock (fast)
 *
 * Return: last known GPU clock
 */
int gpex_clock_get_cur_clock(void);

/**
 * gpex_clock_get_min_lock() - get current gpu min lock clock
 *
 * If there are multiple min lock request (say from sysfs and clboost)
 * The highest min lock clock is used.
 *
 * Return: gpu min lock clock
 */
int gpex_clock_get_min_lock(void);

/**
 * gpex_clock_get_max_lock() - get current gpu max lock clock
 *
 * If there are multiple max lock request (say from sysfs and thermal)
 * The lower max lock clock is used.
 *
 * Return: gpu max lock clock
 */
int gpex_clock_get_max_lock(void);

/**
 * gpex_clock_get_voltage() - get voltage associated with given clock
 * @clk: gpu clock to search the voltage for
 * Return: voltage on success. -EINVAL if given clock not found in clock table
 */
int gpex_clock_get_voltage(int clk);

/**
 * gpex_clock_get_time_busy() - get time spent in given clock since last time in state update
 * @level: level in gpu clock table to get time busy for
 *
 * Return: time busy in jiffies
 */
u64 gpex_clock_get_time_busy(int level);

/**
 * gpex_clock_get_time_in_state_last_update() - get the last time when the time in state was updated
 *
 * Return: time in jiffies
 */
u64 gpex_clock_get_time_in_state_last_update(void);

/**
 * gpex_clock_get_clock() - get gpu clock in the given level in the gpu clock table
 * @level: level in gpu clock table
 *
 * Return: gpu clock in Khz
 */
int gpex_clock_get_clock(int level);

/**
 * gpex_get_valid_gpu_clock() - get the closest clock to given clock in the gpu clock table
 * @clock: approx clock to search for
 * @is_round_up: if true, get the higher clock. If false, get the lower clock
 *
 * Return: gpu clock in Khz
 */
int gpex_get_valid_gpu_clock(int clock, bool is_round_up);

/**************************************
 * MUTEX
 *************************************/

/**
 * gpex_clock_mutex_lock() - acquire gpu clock mutex
 */
void gpex_clock_mutex_lock(void);

/**
 * gpex_clock_mutex_unlock() - release gpu clock mutex
 */
void gpex_clock_mutex_unlock(void);

#endif /* _MALI_EXYNOS_CLOCK_H_ */
