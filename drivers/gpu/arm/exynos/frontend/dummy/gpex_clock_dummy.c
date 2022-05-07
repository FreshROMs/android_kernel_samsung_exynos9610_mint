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

#include <linux/module.h>
#include <gpex_clock.h>

struct clk;
_Bool __clk_is_enabled(struct clk *clk)
{
	return 1;
}
EXPORT_SYMBOL(__clk_is_enabled);

int gpex_clock_init(struct device **dev)
{
	return 0;
}

void gpex_clock_term(void)
{
	return;
}

int gpex_clock_prepare_runtime_off(void)
{
	return 0;
}

int gpex_clock_set(int clk)
{
	return 0;
}

int gpex_clock_lock_clock(gpex_clock_lock_cmd_t lock_command, gpex_clock_lock_type_t lock_type,
			  int clock)
{
	return 0;
}

void gpex_clock_set_user_min_lock_input(int clock)
{
	return;
}

int gpex_clock_get_clock_slow(void)
{
	return 0;
}

int gpex_clock_get_table_idx(int clock)
{
	return 0;
}

int gpex_clock_get_table_idx_cur(void)
{
	return 0;
}

int gpex_clock_get_boot_clock(void)
{
	return 0;
}

int gpex_clock_get_max_clock(void)
{
	return 0;
}

int gpex_clock_get_max_clock_limit(void)
{
	return 0;
}

int gpex_clock_get_min_clock(void)
{
	return 0;
}

int gpex_clock_get_cur_clock(void)
{
	return 0;
}

int gpex_clock_get_min_lock(void)
{
	return 0;
}

int gpex_clock_get_max_lock(void)
{
	return 0;
}

void gpex_clock_mutex_lock(void)
{
	return;
}

void gpex_clock_mutex_unlock(void)
{
	return;
}

int gpex_clock_get_voltage(int clk)
{
	return 0;
}

u64 gpex_clock_get_time(int level)
{
	return 0;
}

u64 gpex_clock_get_time_busy(int level)
{
	return 0;
}

int gpex_clock_get_clock(int level)
{
	return 0;
}
int gpex_get_valid_gpu_clock(int clock, bool is_round_up)
{
	return 0;
}
