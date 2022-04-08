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

#include <linux/ktime.h>
#include <gpex_utils.h>
#include <gpex_tsg.h>

#include <soc/samsung/exynos-profiler.h>

#define DVFS_TABLE_ROW_MAX 9

static ktime_t kt;

int exynos_stats_get_gpu_cur_idx(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_cur_idx);

int exynos_stats_get_gpu_coeff(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_coeff);

uint32_t exynos_stats_get_gpu_table_size(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_table_size);

/* TODO: so far, this stub function is dependant to product, so needs to be diverged for each product */
/* or, just return zero value when GPU Profiler is disabled. */
uint32_t freqs[DVFS_TABLE_ROW_MAX];
uint32_t *gpu_dvfs_get_freq_table(void)
{
	freqs[0] = 858000;
	freqs[1] = 767000;
	freqs[2] = 676000;
	freqs[3] = 585000;
	freqs[4] = 494000;
	freqs[5] = 403000;
	freqs[6] = 312000;
	freqs[7] = 221000;
	freqs[8] = 130000;

	return freqs;
}
EXPORT_SYMBOL(gpu_dvfs_get_freq_table);

uint32_t volts[DVFS_TABLE_ROW_MAX];
uint32_t *exynos_stats_get_gpu_volt_table(void)
{
	return volts;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_volt_table);

ktime_t time_in_state[DVFS_TABLE_ROW_MAX];
ktime_t tis_last_update;
ktime_t *gpu_dvfs_get_time_in_state(void)
{
	return time_in_state;
}
EXPORT_SYMBOL(gpu_dvfs_get_time_in_state);

ktime_t gpu_dvfs_get_tis_last_update(void)
{
	return tis_last_update;
}
EXPORT_SYMBOL(gpu_dvfs_get_tis_last_update);

int gpu_dvfs_get_max_freq(void)
{
	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_get_max_freq);

int gpu_dvfs_get_min_freq(void)
{
	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_get_min_freq);

int exynos_stats_set_queued_threshold_0(unsigned int threshold)
{
	CSTD_UNUSED(threshold);
	return 0;
}
EXPORT_SYMBOL(exynos_stats_set_queued_threshold_0);

int exynos_stats_set_queued_threshold_1(unsigned int threshold)
{
	CSTD_UNUSED(threshold);
	return 0;
}
EXPORT_SYMBOL(exynos_stats_set_queued_threshold_1);

ktime_t *gpu_dvfs_get_job_queue_count(void)
{
	return NULL;
}
EXPORT_SYMBOL(gpu_dvfs_get_job_queue_count);

ktime_t gpu_dvfs_get_job_queue_last_updated(void)
{
	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_get_job_queue_last_updated);

void exynos_stats_set_gpu_polling_speed(int polling_speed)
{
	CSTD_UNUSED(polling_speed);
}
EXPORT_SYMBOL(exynos_stats_set_gpu_polling_speed);

int exynos_stats_get_gpu_polling_speed(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_polling_speed);

void gpu_dvfs_set_amigo_governor(int mode)
{
	CSTD_UNUSED(mode);
}
EXPORT_SYMBOL(gpu_dvfs_set_amigo_governor);

void gpu_dvfs_set_freq_margin(int margin)
{
	CSTD_UNUSED(margin);
}
EXPORT_SYMBOL(gpu_dvfs_set_freq_margin);

void exynos_stats_get_run_times(u64 *times)
{
	CSTD_UNUSED(times);
}
EXPORT_SYMBOL(exynos_stats_get_run_times);

void exynos_stats_get_pid_list(u16 *pidlist)
{
	CSTD_UNUSED(pidlist);
}
EXPORT_SYMBOL(exynos_stats_get_pid_list);

void exynos_stats_set_vsync(ktime_t timestamp)
{
	CSTD_UNUSED(timestamp);
}
EXPORT_SYMBOL(exynos_stats_set_vsync);

void exynos_migov_set_targetframetime(int us)
{
	CSTD_UNUSED(us);
}
EXPORT_SYMBOL(exynos_migov_set_targetframetime);

void exynos_sdp_set_powertable(int id, int cnt, struct freq_table *table)
{
	CSTD_UNUSED(id);
	CSTD_UNUSED(cnt);
	CSTD_UNUSED(table);
}
EXPORT_SYMBOL(exynos_sdp_set_powertable);

void exynos_sdp_set_busy_domain(int id)
{
	CSTD_UNUSED(id);
}
EXPORT_SYMBOL(exynos_sdp_set_busy_domain);

void exynos_sdp_set_cur_freqlv(int id, int idx)
{
	CSTD_UNUSED(idx);
}
EXPORT_SYMBOL(exynos_sdp_set_cur_freqlv);

void exynos_migov_set_targettime_margin(int us)
{
	CSTD_UNUSED(us);
}
EXPORT_SYMBOL(exynos_migov_set_targettime_margin);

void exynos_migov_set_util_margin(int percentage)
{
	CSTD_UNUSED(percentage);
}
EXPORT_SYMBOL(exynos_migov_set_util_margin);

void exynos_migov_set_decon_time(int us)
{
	CSTD_UNUSED(us);
}
EXPORT_SYMBOL(exynos_migov_set_decon_time);

void exynos_migov_set_comb_ctrl(int enable)
{
	CSTD_UNUSED(enable);
}
EXPORT_SYMBOL(exynos_migov_set_comb_ctrl);

int exynos_gpu_stc_config_show(int page_size, char *buf)
{
	CSTD_UNUSED(page_size);
	CSTD_UNUSED(buf);
	return 0;
}
EXPORT_SYMBOL(exynos_gpu_stc_config_show);

int exynos_gpu_stc_config_store(const char *buf)
{
	CSTD_UNUSED(buf);
	return 0;
}
EXPORT_SYMBOL(exynos_gpu_stc_config_store);

int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb)
{
	CSTD_UNUSED(nb);
	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_register_utilization_notifier);

int gpu_dvfs_unregister_utilization_notifier(struct notifier_block *nb)
{
	CSTD_UNUSED(nb);
	return 0;
}
EXPORT_SYMBOL(gpu_dvfs_unregister_utilization_notifier);

void gpex_tsg_set_migov_mode(int mode)
{
	CSTD_UNUSED(mode);
}

void gpex_tsg_set_freq_margin(int margin)
{
	CSTD_UNUSED(margin);
}

void gpex_tsg_set_util_history(int idx, int order, int input)
{
	CSTD_UNUSED(idx);
	CSTD_UNUSED(order);
	CSTD_UNUSED(input);
}

void gpex_tsg_set_weight_util(int idx, int input)
{
	CSTD_UNUSED(idx);
	CSTD_UNUSED(input);
}

void gpex_tsg_set_weight_freq(int input)
{
	CSTD_UNUSED(input);
}

void gpex_tsg_set_en_signal(bool input)
{
	CSTD_UNUSED(input);
}

void gpex_tsg_set_pmqos(bool pm_qos_tsg)
{
	CSTD_UNUSED(pm_qos_tsg);
}

void gpex_tsg_set_weight_table_idx(int idx, int input)
{
	CSTD_UNUSED(idx);
	CSTD_UNUSED(input);
}

void gpex_tsg_set_is_gov_set(int input)
{
	CSTD_UNUSED(input);
}

void gpex_tsg_set_saved_polling_speed(int input)
{
	CSTD_UNUSED(input);
}

void gpex_tsg_set_governor_type_init(int input)
{
	CSTD_UNUSED(input);
}

void gpex_tsg_set_amigo_flags(int input)
{
	CSTD_UNUSED(input);
}

void gpex_tsg_set_queued_threshold(int idx, uint32_t input)
{
	CSTD_UNUSED(idx);
	CSTD_UNUSED(input);
}

void gpex_tsg_set_queued_time_tick(int idx, ktime_t input)
{
	CSTD_UNUSED(idx);
	CSTD_UNUSED(input);
}

void gpex_tsg_set_queued_time(int idx, ktime_t input)
{
	CSTD_UNUSED(idx);
	CSTD_UNUSED(input);
}

void gpex_tsg_set_queued_last_updated(ktime_t input)
{
	CSTD_UNUSED(input);
}

void gpex_tsg_set_queue_nr(int type, int variation)
{
	CSTD_UNUSED(type);
	CSTD_UNUSED(variation);
}

int gpex_tsg_get_migov_mode(void)
{
	return 0;
}

int gpex_tsg_get_freq_margin(void)
{
	return 0;
}

bool gpex_tsg_get_pmqos(void)
{
	return false;
}

int gpex_tsg_get_util_history(int idx, int order)
{
	return 0;
}

int gpex_tsg_get_weight_util(int idx)
{
	return 0;
}

int gpex_tsg_get_weight_freq(void)
{
	return 0;
}

bool gpex_tsg_get_en_signal(void)
{
	return 0;
}

int gpex_tsg_get_weight_table_idx(int idx)
{
	return 0;
}

int gpex_tsg_get_is_gov_set(void)
{
	return 0;
}

int gpex_tsg_get_saved_polling_speed(void)
{
	return 0;
}

int gpex_tsg_get_governor_type_init(void)
{
	return 0;
}

int gpex_tsg_get_amigo_flags(void)
{
	return 0;
}

uint32_t gpex_tsg_get_queued_threshold(int idx)
{
	CSTD_UNUSED(idx);
	return 0;
}

ktime_t gpex_tsg_get_queued_time_tick(int idx)
{
	CSTD_UNUSED(idx);
	return kt;
}

ktime_t gpex_tsg_get_queued_time(int idx)
{
	CSTD_UNUSED(idx);
	return kt;
}

ktime_t *gpex_tsg_get_queued_time_array(void)
{
	return &kt;
}

ktime_t gpex_tsg_get_queued_last_updated(void)
{
	return kt;
}

int gpex_tsg_get_queue_nr(int type)
{
	CSTD_UNUSED(type);
	return 0;
}

struct atomic_notifier_head *gpex_tsg_get_frag_utils_change_notifier_list(void)
{
	return 0;
}

int gpex_tsg_notify_frag_utils_change(u32 js0_utils)
{
	CSTD_UNUSED(js0_utils);
	return 0;
}

int gpex_tsg_spin_lock(void)
{
	return 0;
}

void gpex_tsg_spin_unlock(void)
{
	return;
}

int gpex_tsg_reset_count(int powered)
{
	return 0;
}

int gpex_tsg_set_count(u32 status, bool stop)
{
	return 0;
}

void gpex_tsg_update_firstjob_time(void)
{
}

void gpex_tsg_update_lastjob_time(int slot_nr)
{
	CSTD_UNUSED(slot_nr);
}

void gpex_tsg_update_jobsubmit_time(void)
{
}

void gpex_tsg_sum_jobs_time(int slot_nr)
{
	CSTD_UNUSED(slot_nr);
}

int gpex_tsg_amigo_interframe_sw_update(ktime_t start, ktime_t end)
{
	CSTD_UNUSED(start);
	CSTD_UNUSED(end);

	return 0;
}

int gpex_tsg_amigo_interframe_hw_update_eof(void)
{
	return 0;
}

int gpex_tsg_amigo_interframe_hw_update(void)
{
	return 0;
}

int gpex_tsg_init(struct device **dev)
{
	memset(&kt, 0, sizeof(ktime_t));
	return 0;
}
int gpex_tsg_term(void)
{
	return 0;
}

void gpex_tsg_input_nr_acc_cnt(void)
{
	return;
}

void gpex_tsg_reset_acc_count(void)
{
	return;
}

void exynos_stats_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	CSTD_UNUSED(nrframe);
	CSTD_UNUSED(nrvsync);
	CSTD_UNUSED(delta_ms);
}
EXPORT_SYMBOL(exynos_stats_get_frame_info);
