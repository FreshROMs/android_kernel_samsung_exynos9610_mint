// SPDX-License-Identifier: GPL-2.0

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

#ifndef _MALI_EXYNOS_DEBUG_H_
#define _MALI_EXYNOS_DEBUG_H_

struct device;

enum hist_type {
	HIST_CLOCK = 0,
	HIST_LLC,
	HIST_BTS,
	HIST_RTPM,
	HIST_SUSPEND,
	HIST_TYPE_SIZE,
};

enum pm_action {
	PM_RUNTIME_GET_SYNC = 1,
	PM_RUNTIME_SUSPEND = 2,
	PM_RUNTIME_RESUME = 3,
};

int gpex_debug_init(struct device **dev);

void gpex_debug_dump_hist(enum hist_type ht);

void gpex_debug_new_record(enum hist_type ht);
void gpex_debug_record_time(enum hist_type ht);
void gpex_debug_record_prev_data(enum hist_type ht, int prev_data);
void gpex_debug_record_new_data(enum hist_type ht, int new_data);
void gpex_debug_record_code(enum hist_type ht, int code);
void gpex_debug_record(enum hist_type ht, int prev_data, int new_data, int code);

void gpex_debug_incr_error_cnt(enum hist_type ht);
void gpex_debug_dump_error_cnt(void);

#endif /* _MALI_EXYNOS_DEBUG_H_ */
