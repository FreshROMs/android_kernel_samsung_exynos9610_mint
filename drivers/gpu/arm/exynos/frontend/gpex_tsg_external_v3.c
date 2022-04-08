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

#include <linux/notifier.h>
#include <linux/ktime.h>

#include <gpex_tsg.h>
#include <gpex_dvfs.h>
#include <gpex_utils.h>
#include <gpex_pm.h>
#include <gpex_ifpo.h>
#include <gpex_clock.h>
#include <gpexbe_pm.h>
#include <gpex_cmar_sched.h>

#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-profiler.h>

#include "gpex_tsg_internal.h"

#define DVFS_TABLE_ROW_MAX 20

static struct _tsg_info *tsg_info;

unsigned long exynos_stats_get_job_state_cnt(void)
{
	return tsg_info->input_job_nr_acc;
}
EXPORT_SYMBOL(exynos_stats_get_job_state_cnt);

int exynos_stats_get_gpu_cur_idx(void)
{
	int i;
	int level = -1;
	int clock = 0;
	int idx_max_clk, idx_min_clk;

	if (gpexbe_pm_get_exynos_pm_domain()) {
		if (gpexbe_pm_get_status() == 1) {
			clock = gpex_clock_get_cur_clock();
		} else {
			GPU_LOG(MALI_EXYNOS_ERROR, "%s: can't get dvfs cur clock\n", __func__);
			clock = 0;
		}
	} else {
		if (gpex_pm_get_status(true) == 1) {
			if (gpex_ifpo_get_mode() && !gpex_ifpo_get_status()) {
				GPU_LOG(MALI_EXYNOS_ERROR, "%s: can't get dvfs cur clock\n",
					__func__);
				clock = 0;
			} else {
				clock = gpex_clock_get_cur_clock();
			}
		}
	}

	idx_max_clk = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	idx_min_clk = gpex_clock_get_table_idx(gpex_clock_get_min_clock());

	if ((idx_max_clk < 0) || (idx_min_clk < 0)) {
		GPU_LOG(MALI_EXYNOS_ERROR,
			"%s: mismatch clock table index. max_clock_level : %d, min_clock_level : %d\n",
			__func__, idx_max_clk, idx_min_clk);
		return -1;
	}

	if (clock == 0)
		return idx_min_clk - idx_max_clk;

	for (i = idx_max_clk; i <= idx_min_clk; i++) {
		if (gpex_clock_get_clock(i) == clock) {
			level = i;
			break;
		}
	}

	return (level - idx_max_clk);
}
EXPORT_SYMBOL(exynos_stats_get_gpu_cur_idx);

int exynos_stats_get_gpu_coeff(void)
{
	int coef = 6144;

	return coef;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_coeff);

uint32_t exynos_stats_get_gpu_table_size(void)
{
	return (gpex_clock_get_table_idx(gpex_clock_get_min_clock()) -
		gpex_clock_get_table_idx(gpex_clock_get_max_clock()) + 1);
}
EXPORT_SYMBOL(exynos_stats_get_gpu_table_size);

static uint32_t freqs[DVFS_TABLE_ROW_MAX];
uint32_t *gpu_dvfs_get_freq_table(void)
{
	int i;
	int idx_max_clk, idx_min_clk;

	idx_max_clk = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	idx_min_clk = gpex_clock_get_table_idx(gpex_clock_get_min_clock());

	if ((idx_max_clk < 0) || (idx_min_clk < 0)) {
		GPU_LOG(MALI_EXYNOS_ERROR,
			"%s: mismatch clock table index. idx_max_clk : %d, idx_min_clk : %d\n",
			__func__, idx_max_clk, idx_min_clk);
		return freqs;
	}

	for (i = idx_max_clk; i <= idx_min_clk; i++)
		freqs[i - idx_max_clk] = (uint32_t)gpex_clock_get_clock(i);

	return freqs;
}
EXPORT_SYMBOL(gpu_dvfs_get_freq_table);

static uint32_t volts[DVFS_TABLE_ROW_MAX];
uint32_t *exynos_stats_get_gpu_volt_table(void)
{
	int i;
	int idx_max_clk, idx_min_clk;

	idx_max_clk = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	idx_min_clk = gpex_clock_get_table_idx(gpex_clock_get_min_clock());

	if ((idx_max_clk < 0) || (idx_min_clk < 0)) {
		GPU_LOG(MALI_EXYNOS_ERROR,
			"%s: mismatch clock table index. idx_max_clk : %d, idx_min_clk : %d\n",
			__func__, idx_max_clk, idx_min_clk);
		return volts;
	}

	for (i = idx_max_clk; i <= idx_min_clk; i++)
		volts[i - idx_max_clk] = (uint32_t)gpex_clock_get_voltage(gpex_clock_get_clock(i));

	return volts;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_volt_table);

static ktime_t time_in_state[DVFS_TABLE_ROW_MAX];
ktime_t tis_last_update;
ktime_t *gpu_dvfs_get_time_in_state(void)
{
	int i;
	int idx_max_clk, idx_min_clk;

	idx_max_clk = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	idx_min_clk = gpex_clock_get_table_idx(gpex_clock_get_min_clock());

	if ((idx_max_clk < 0) || (idx_min_clk < 0)) {
		GPU_LOG(MALI_EXYNOS_ERROR,
			"%s: mismatch clock table index. idx_max_clk : %d, idx_min_clk : %d\n",
			__func__, idx_max_clk, idx_min_clk);
		return time_in_state;
	}

	for (i = idx_max_clk; i <= idx_min_clk; i++) {
		time_in_state[i - idx_max_clk] =
			ms_to_ktime((u64)(gpex_clock_get_time_busy(i) * 4) / 100);
	}

	return time_in_state;
}
EXPORT_SYMBOL(gpu_dvfs_get_time_in_state);

ktime_t gpu_dvfs_get_tis_last_update(void)
{
	return (ktime_t)(gpex_clock_get_time_in_state_last_update());
}
EXPORT_SYMBOL(gpu_dvfs_get_tis_last_update);

int exynos_stats_set_queued_threshold_0(uint32_t threshold)
{
	gpex_tsg_set_queued_threshold(0, threshold);
	return gpex_tsg_get_queued_threshold(0);
}
EXPORT_SYMBOL(exynos_stats_set_queued_threshold_0);

int exynos_stats_set_queued_threshold_1(uint32_t threshold)
{
	gpex_tsg_set_queued_threshold(1, threshold);

	return gpex_tsg_get_queued_threshold(1);
}
EXPORT_SYMBOL(exynos_stats_set_queued_threshold_1);

ktime_t *gpu_dvfs_get_job_queue_count(void)
{
	int i;
	for (i = 0; i < 2; i++) {
		gpex_tsg_set_queued_time(i, gpex_tsg_get_queued_time_tick(i));
	}
	return gpex_tsg_get_queued_time_array();
}
EXPORT_SYMBOL(gpu_dvfs_get_job_queue_count);

ktime_t gpu_dvfs_get_job_queue_last_updated(void)
{
	return gpex_tsg_get_queued_last_updated();
}
EXPORT_SYMBOL(gpu_dvfs_get_job_queue_last_updated);

void exynos_stats_set_gpu_polling_speed(int polling_speed)
{
	gpex_dvfs_set_polling_speed(polling_speed);
}
EXPORT_SYMBOL(exynos_stats_set_gpu_polling_speed);

int exynos_stats_get_gpu_polling_speed(void)
{
	return gpex_dvfs_get_polling_speed();
}
EXPORT_SYMBOL(exynos_stats_get_gpu_polling_speed);

void gpu_dvfs_set_amigo_governor(int mode)
{
	gpex_tsg_set_migov_mode(mode);

	if (mode)
		gpex_cmar_sched_set_forced_sched(1);
	else
		gpex_cmar_sched_set_forced_sched(0);
}
EXPORT_SYMBOL(gpu_dvfs_set_amigo_governor);

void gpu_dvfs_set_freq_margin(int margin)
{
	gpex_tsg_set_freq_margin(margin);
}
EXPORT_SYMBOL(gpu_dvfs_set_freq_margin);

void exynos_stats_get_run_times(u64 *times)
{
	gpex_tsg_stats_get_run_times(times);
}
EXPORT_SYMBOL(exynos_stats_get_run_times);

void exynos_stats_get_pid_list(u16 *pidlist)
{
	gpex_tsg_stats_get_pid_list(pidlist);
}
EXPORT_SYMBOL(exynos_stats_get_pid_list);

void exynos_stats_set_vsync(ktime_t ktime_us)
{
	gpex_tsg_stats_set_vsync(ktime_us);
}
EXPORT_SYMBOL(exynos_stats_set_vsync);

void exynos_stats_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	gpex_tsg_stats_get_frame_info(nrframe, nrvsync, delta_ms);
}
EXPORT_SYMBOL(exynos_stats_get_frame_info);

void exynos_migov_set_targetframetime(int us)
{
	gpex_tsg_migov_set_targetframetime(us);
}
EXPORT_SYMBOL(exynos_migov_set_targetframetime);

void exynos_migov_set_targettime_margin(int us)
{
	gpex_tsg_migov_set_targettime_margin(us);
}
EXPORT_SYMBOL(exynos_migov_set_targettime_margin);

void exynos_migov_set_util_margin(int percentage)
{
	gpex_tsg_migov_set_util_margin(percentage);
}
EXPORT_SYMBOL(exynos_migov_set_util_margin);

void exynos_migov_set_decon_time(int us)
{
	gpex_tsg_migov_set_decon_time(us);
}
EXPORT_SYMBOL(exynos_migov_set_decon_time);

void exynos_migov_set_comb_ctrl(int val)
{
	gpex_tsg_migov_set_comb_ctrl(val);
}
EXPORT_SYMBOL(exynos_migov_set_comb_ctrl);

void exynos_sdp_set_powertable(int id, int cnt, struct freq_table *table)
{
	gpex_tsg_sdp_set_powertable(id, cnt, table);
}
EXPORT_SYMBOL(exynos_sdp_set_powertable);

void exynos_sdp_set_busy_domain(int id)
{
	gpex_tsg_sdp_set_busy_domain(id);
}
EXPORT_SYMBOL(exynos_sdp_set_busy_domain);

void exynos_sdp_set_cur_freqlv(int id, int idx)
{
	gpex_tsg_sdp_set_cur_freqlv(id, idx);
}
EXPORT_SYMBOL(exynos_sdp_set_cur_freqlv);

int exynos_gpu_stc_config_show(int page_size, char *buf)
{
	return gpex_tsg_stc_config_show(page_size, buf);
}
EXPORT_SYMBOL(exynos_gpu_stc_config_show);

int exynos_gpu_stc_config_store(const char *buf)
{
	return gpex_tsg_stc_config_store(buf);
}
EXPORT_SYMBOL(exynos_gpu_stc_config_store);

int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(gpex_tsg_get_frag_utils_change_notifier_list(), nb);
}
EXPORT_SYMBOL(gpu_dvfs_register_utilization_notifier);

int gpu_dvfs_unregister_utilization_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(gpex_tsg_get_frag_utils_change_notifier_list(), nb);
}
EXPORT_SYMBOL(gpu_dvfs_unregister_utilization_notifier);

/* TODO: this sysfs function use external fucntion. */
/* Actually, Using external function in internal module is not ideal with the Refactoring rules */
/* So, if we can modify outer modules such as 'migov, cooling, ...' in the future, */
/* fix it following the rules*/
static ssize_t show_feedback_governor_impl(char *buf)
{
	ssize_t ret = 0;
	int i;
	uint32_t *freqs;
	uint32_t *volts;
	ktime_t *time_in_state;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "feedback governer implementation\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int exynos_stats_get_gpu_table_size(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "     +- int gpu_dvfs_get_step(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n",
			exynos_stats_get_gpu_table_size());
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int exynos_stats_get_gpu_cur_idx(void)\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "     +- int gpu_dvfs_get_cur_level(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- clock=%u\n",
			gpex_clock_get_cur_clock());
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- level=%d\n",
			exynos_stats_get_gpu_cur_idx());
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int exynos_stats_get_gpu_coeff(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- int gpu_dvfs_get_coefficient_value(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n",
			exynos_stats_get_gpu_coeff());
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int *gpu_dvfs_get_freq_table(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- uint32_t *gpu_dvfs_get_freqs(void)\n");
	freqs = gpu_dvfs_get_freq_table();
	for (i = 0; i < exynos_stats_get_gpu_table_size(); i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n", freqs[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int *exynos_stats_get_gpu_volt_table(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- unsigned int *gpu_dvfs_get_volts(void)\n");
	volts = exynos_stats_get_gpu_volt_table();
	for (i = 0; i < exynos_stats_get_gpu_table_size(); i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n", volts[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- ktime_t *gpu_dvfs_get_time_in_state(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- ktime_t *gpu_dvfs_get_time_in_state(void)\n");
	time_in_state = gpu_dvfs_get_time_in_state();
	for (i = 0; i < exynos_stats_get_gpu_table_size(); i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %lld\n", time_in_state[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- ktime_t *gpu_dvfs_get_job_queue_count(void)\n");
	time_in_state = gpu_dvfs_get_job_queue_count();
	for (i = 0; i < 2; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %lld\n", time_in_state[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, " +- queued_threshold_check\n");
	for (i = 0; i < 2; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %lld\n",
				gpex_tsg_get_queued_threshold(i));
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- int exynos_stats_get_gpu_polling_speed(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %d\n",
			exynos_stats_get_gpu_polling_speed());

	return ret;
}
CREATE_SYSFS_DEVICE_READ_FUNCTION(show_feedback_governor_impl);

int gpex_tsg_external_init(struct _tsg_info *_tsg_info)
{
	tsg_info = _tsg_info;
	GPEX_UTILS_SYSFS_DEVICE_FILE_ADD_RO(feedback_governor_impl, show_feedback_governor_impl);
	return 0;
}

int gpex_tsg_external_term(void)
{
	tsg_info = (void *)0;

	return 0;
}
