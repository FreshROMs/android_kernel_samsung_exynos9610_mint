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

/* Implements */
#include <gpex_debug.h>

/* Uses */
#include <gpex_utils.h>
#include <mali_kbase.h>
#include <linux/ktime.h>

struct record {
	int prev_data;
	int new_data;
	struct timespec64 ts;
	int code;
};

#define CLK_HIST_SIZE 8
#define LLC_HIST_SIZE 8
#define BTS_HIST_SIZE 8
#define RTPM_HIST_SIZE 8
#define SUSPEND_HIST_SIZE 8

static const char* hist_name[HIST_TYPE_SIZE] = {"clk", "llc", "bts", "rtpm", "suspend"};
static const int hist_size[HIST_TYPE_SIZE] = {
	CLK_HIST_SIZE, LLC_HIST_SIZE, BTS_HIST_SIZE, RTPM_HIST_SIZE, SUSPEND_HIST_SIZE };
static struct record * hist_arr[HIST_TYPE_SIZE];

static struct _debug_info {
	int hist_idx[HIST_TYPE_SIZE];
	int hist_errors[HIST_TYPE_SIZE];

	struct record clk_hist[CLK_HIST_SIZE];
	struct record llc_hist[LLC_HIST_SIZE];
	struct record bts_hist[BTS_HIST_SIZE];
	struct record rtpm_hist[RTPM_HIST_SIZE];
	struct record suspend_hist[SUSPEND_HIST_SIZE];

	struct device *dev;
} debug_info;

int gpex_debug_init(struct device **dev)
{
	int idx;

	debug_info.dev = *dev;

	hist_arr[0] = debug_info.clk_hist;
	hist_arr[1] = debug_info.llc_hist;
	hist_arr[2] = debug_info.bts_hist;
	hist_arr[3] = debug_info.rtpm_hist;
	hist_arr[4] = debug_info.suspend_hist;

	for (idx = 0; idx < HIST_TYPE_SIZE; idx++) {
		memset(hist_arr[idx], -1, sizeof(*hist_arr[idx]) * hist_size[idx]);
	}

	gpex_utils_get_exynos_context()->debug_info = &debug_info;

	return 0;
}

void gpex_debug_dump_hist(enum hist_type ht)
{
	int idx = debug_info.hist_idx[ht];
	int len = hist_size[ht];

	if (debug_info.dev == NULL)
		return;

	while (len-- > 0) {
		struct record rec = hist_arr[ht][idx];
		dev_warn(debug_info.dev, "%s,%d.%.6d,%d,%d,%d",
				hist_name[ht],
				(int)rec.ts.tv_sec,
				(int)(rec.ts.tv_nsec / 1000),
				rec.prev_data,
				rec.new_data,
				rec.code);

		idx--;
		if (idx < 0)
			idx = hist_size[ht]  - 1;
	}
}

static inline struct record *get_record(enum hist_type ht)
{
	return &hist_arr[ht][debug_info.hist_idx[ht]];
}

void gpex_debug_new_record(enum hist_type ht)
{
	debug_info.hist_idx[ht]++;
	if (debug_info.hist_idx[ht] >= hist_size[ht])
		debug_info.hist_idx[ht] = 0;

	memset(get_record(ht), 0xAA, sizeof(struct record));
}

void gpex_debug_record_time(enum hist_type ht)
{
	ktime_get_ts64(&get_record(ht)->ts);
}

void gpex_debug_record_prev_data(enum hist_type ht, int prev_data)
{
	get_record(ht)->prev_data = prev_data;
}

void gpex_debug_record_new_data(enum hist_type ht, int new_data)
{
	get_record(ht)->new_data = new_data;
}

void gpex_debug_record_code(enum hist_type ht, int code)
{
	get_record(ht)->code = code;
}

void gpex_debug_record(enum hist_type ht, int prev_data, int new_data, int code)
{
	struct record *rec = get_record(ht);

	ktime_get_ts64(&rec->ts);
	rec->prev_data = prev_data;
	rec->new_data = new_data;
	rec->code = code;
}

void gpex_debug_incr_error_cnt(enum hist_type ht)
{
	debug_info.hist_errors[ht]++;
}

void gpex_debug_dump_error_cnt(void)
{
	int idx = 0;

	if (debug_info.dev == NULL)
		return;

	for (idx = 0; idx < HIST_TYPE_SIZE; idx++) {
		dev_warn(debug_info.dev, "%s errors: %d", hist_name[idx], debug_info.hist_errors[idx]);
	}
}
