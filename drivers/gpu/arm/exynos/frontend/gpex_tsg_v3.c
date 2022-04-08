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

#include <gpex_tsg.h>

#include <gpex_utils.h>
#include <gpex_dvfs.h>
#include <gpex_pm.h>
#include <gpex_clock.h>
#include <gpexbe_devicetree.h>
#include <gpexbe_utilization.h>

#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-profiler.h>

#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>

#include "gpex_tsg_internal.h"
#include "gpex_dvfs_internal.h"
#include "gpu_dvfs_governor.h"

#define TABLE_MAX                      (200)
#define MAX_GOVERNOR_NUM               (4)
#define VSYNC_Q_SIZE                   (16)
#define RENDERINGTIME_MAX_TIME         (999999)
#define RENDERINGTIME_MIN_FRAME        (4000)
#define RTP_DEFAULT_FRAMETIME          (16666)
#define DEADLINE_DECON_INUS            (1000)
#define SYSBUSY_FREQ_THRESHOLD         (605000)
#define SYSBUSY_UTIL_THRESHOLD         (60)

static struct _tsg_info tsg;
static struct amigo_interframe_data interframe[TABLE_MAX];

static struct exynos_pm_qos_request exynos_amigo_gpu_min_qos;

static struct freq_qos_request cl1_min_pm_qos;
static struct freq_qos_request cl2_min_pm_qos;

static unsigned int rtp_head;
static unsigned int rtp_tail;
static unsigned int rtp_nrq;
static unsigned int rtp_lastshowidx;
static ktime_t rtp_prev_swaptimestamp;
static ktime_t rtp_lasthw_starttime;
static ktime_t rtp_lasthw_endtime;
static atomic_t rtp_lasthw_totaltime;
static atomic_t rtp_lasthw_read;
static ktime_t rtp_curhw_starttime;
static ktime_t rtp_curhw_endtime;
static ktime_t rtp_curhw_totaltime;
static ktime_t rtp_sum_pre;
static ktime_t rtp_sum_cpu;
static ktime_t rtp_sum_v2s;
static ktime_t rtp_sum_gpu;
static ktime_t rtp_sum_v2f;
static ktime_t rtp_max_pre;
static ktime_t rtp_max_cpu;
static ktime_t rtp_max_v2s;
static ktime_t rtp_max_gpu;
static ktime_t rtp_max_v2f;
static int rtp_last_cputime;
static int rtp_last_gputime;

static ktime_t rtp_vsync_lastsw;
static ktime_t rtp_vsync_curhw;
static ktime_t rtp_vsync_lasthw;
static ktime_t rtp_vsync_prev;
static ktime_t rtp_vsync_interval;
static int rtp_vsync_swapcall_counter;
static int rtp_vsync_frame_counter;
static int rtp_vsync_counter;
static unsigned int rtp_target_frametime;

static int migov_vsync_frame_counter;
static unsigned int migov_vsync_counter;
static ktime_t migov_last_updated_vsync_time;

static unsigned int rtp_targettime_margin;
static unsigned int rtp_workload_margin;
static unsigned int rtp_decon_time;
static int rtp_shortterm_comb_ctrl;
static int rtp_shortterm_comb_ctrl_manual;

static int rtp_powertable_size[NUM_OF_DOMAIN];
static struct amigo_freq_estpower *rtp_powertable[NUM_OF_DOMAIN];
static int rtp_busy_cpuid_prev;
static int rtp_busy_cpuid;
static int rtp_busy_cpuid_next;
static int rtp_cur_freqlv[NUM_OF_DOMAIN];
static int rtp_max_minlock_freq;
static int rtp_prev_minlock_cpu_freq;
static int rtp_prev_minlock_gpu_freq;
static int rtp_last_minlock_cpu_maxfreq;
static int rtp_last_minlock_gpu_maxfreq;

void gpex_tsg_input_nr_acc_cnt(void)
{
	tsg.input_job_nr_acc += tsg.input_job_nr;
}

void gpex_tsg_reset_acc_count(void)
{
	tsg.input_job_nr_acc = 0;
	gpex_tsg_set_queued_time_tick(0, 0);
	gpex_tsg_set_queued_time_tick(1, 0);
}

/* SETTER */
void gpex_tsg_set_migov_mode(int mode)
{
	tsg.migov_mode = mode;
}

void gpex_tsg_set_freq_margin(int margin)
{
	tsg.freq_margin = margin;
}

void gpex_tsg_set_util_history(int idx, int order, int input)
{
	tsg.prediction.util_history[idx][order] = input;
}

void gpex_tsg_set_weight_util(int idx, int input)
{
	tsg.prediction.weight_util[idx] = input;
}

void gpex_tsg_set_weight_freq(int input)
{
	tsg.prediction.weight_freq = input;
}

void gpex_tsg_set_en_signal(bool input)
{
	tsg.prediction.en_signal = input;
}

void gpex_tsg_set_pmqos(bool input)
{
	tsg.is_pm_qos_tsg = input;
}

void gpex_tsg_set_weight_table_idx(int idx, int input)
{
	if (idx == 0)
		tsg.weight_table_idx_0 = input;
	else
		tsg.weight_table_idx_1 = input;
}

void gpex_tsg_set_is_gov_set(int input)
{
	tsg.is_gov_set = input;
}

void gpex_tsg_set_saved_polling_speed(int input)
{
	tsg.migov_saved_polling_speed = input;
}

void gpex_tsg_set_governor_type_init(int input)
{
	tsg.governor_type_init = input;
}

void gpex_tsg_set_amigo_flags(int input)
{
	tsg.amigo_flags = input;
}

void gpex_tsg_set_queued_threshold(int idx, uint32_t input)
{
	tsg.queue.queued_threshold[idx] = input;
}

void gpex_tsg_set_queued_time_tick(int idx, ktime_t input)
{
	tsg.queue.queued_time_tick[idx] = input;
}

void gpex_tsg_set_queued_time(int idx, ktime_t input)
{
	tsg.queue.queued_time[idx] = input;
}

void gpex_tsg_set_queued_last_updated(ktime_t input)
{
	tsg.queue.last_updated = input;
}

void gpex_tsg_set_queue_nr(int type, int variation)
{
	/* GPEX_TSG_QUEUE_JOB/IN/OUT: 0, 1, 2*/
	/* GPEX_TSG_QUEUE_INC/DEC/RST: 3, 4, 5*/

	if (variation == GPEX_TSG_RST)
		atomic_set(&tsg.queue.nr[type], 0);
	else
		(variation == GPEX_TSG_INC) ? atomic_inc(&tsg.queue.nr[type]) :
						    atomic_dec(&tsg.queue.nr[type]);
}

/* GETTER */
int gpex_tsg_get_migov_mode(void)
{
	return tsg.migov_mode;
}

int gpex_tsg_get_freq_margin(void)
{
	return tsg.freq_margin;
}

int gpex_tsg_get_util_history(int idx, int order)
{
	return tsg.prediction.util_history[idx][order];
}

int gpex_tsg_get_weight_util(int idx)
{
	return tsg.prediction.weight_util[idx];
}

int gpex_tsg_get_weight_freq(void)
{
	return tsg.prediction.weight_freq;
}

bool gpex_tsg_get_en_signal(void)
{
	return tsg.prediction.en_signal;
}

bool gpex_tsg_get_pmqos(void)
{
	return tsg.is_pm_qos_tsg;
}

int gpex_tsg_get_weight_table_idx(int idx)
{
	return (idx == 0) ? tsg.weight_table_idx_0 : tsg.weight_table_idx_1;
}

int gpex_tsg_get_is_gov_set(void)
{
	return tsg.is_gov_set;
}

int gpex_tsg_get_saved_polling_speed(void)
{
	return tsg.migov_saved_polling_speed;
}

int gpex_tsg_get_governor_type_init(void)
{
	return tsg.governor_type_init;
}

int gpex_tsg_get_amigo_flags(void)
{
	return tsg.amigo_flags;
}

uint32_t gpex_tsg_get_queued_threshold(int idx)
{
	return tsg.queue.queued_threshold[idx];
}

ktime_t gpex_tsg_get_queued_time_tick(int idx)
{
	return tsg.queue.queued_time_tick[idx];
}

ktime_t gpex_tsg_get_queued_time(int idx)
{
	return tsg.queue.queued_time[idx];
}

ktime_t *gpex_tsg_get_queued_time_array(void)
{
	return tsg.queue.queued_time;
}

ktime_t gpex_tsg_get_queued_last_updated(void)
{
	return tsg.queue.last_updated;
}

int gpex_tsg_get_queue_nr(int type)
{
	return atomic_read(&tsg.queue.nr[type]);
}

struct atomic_notifier_head *gpex_tsg_get_frag_utils_change_notifier_list(void)
{
	return &(tsg.frag_utils_change_notifier_list);
}

int gpex_tsg_notify_frag_utils_change(u32 js0_utils)
{
	return atomic_notifier_call_chain(gpex_tsg_get_frag_utils_change_notifier_list(), js0_utils, NULL);
}

int gpex_tsg_spin_lock(void)
{
	return raw_spin_trylock(&tsg.spinlock);
}

void gpex_tsg_spin_unlock(void)
{
	raw_spin_unlock(&tsg.spinlock);
}

/* Function of Kbase */

static inline bool both_q_active(int in_nr, int out_nr)
{
	return in_nr > 0 && out_nr > 0;
}

static inline bool hw_q_active(int out_nr)
{
	return out_nr > 0;
}

static void accum_queued_time(ktime_t current_time, bool accum0, bool accum1)
{
	ktime_t time_diff = 0;
	time_diff = current_time - gpex_tsg_get_queued_last_updated();

	if (accum0 == true)
		gpex_tsg_set_queued_time_tick(0, gpex_tsg_get_queued_time_tick(0) + time_diff);
	if (accum1 == true)
		gpex_tsg_set_queued_time_tick(1, gpex_tsg_get_queued_time_tick(1) + time_diff);

	gpex_tsg_set_queued_last_updated(current_time);
}

int gpex_tsg_reset_count(int powered)
{
	if (powered)
		return 0;

	gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_JOB, GPEX_TSG_RST);
	gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_IN, GPEX_TSG_RST);
	gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_OUT, GPEX_TSG_RST);
	gpex_tsg_set_queued_time_tick(0, 0);
	gpex_tsg_set_queued_time_tick(1, 0);
	gpex_tsg_set_queued_last_updated(0);

	tsg.input_job_nr = 0;

	return !powered;
}

int gpex_tsg_set_count(u32 status, bool stop)
{
	int prev_out_nr, prev_in_nr;
	int cur_out_nr, cur_in_nr;
	bool need_update = false;
	ktime_t current_time = 0;

	if (gpex_tsg_get_queued_last_updated() == 0)
		gpex_tsg_set_queued_last_updated(ktime_get());

	prev_out_nr = gpex_tsg_get_queue_nr(GPEX_TSG_QUEUE_OUT);
	prev_in_nr = gpex_tsg_get_queue_nr(GPEX_TSG_QUEUE_IN);

	if (stop) {
		gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_IN, GPEX_TSG_INC);
		gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_OUT, GPEX_TSG_DEC);
		tsg.input_job_nr++;
	} else {
		switch (status) {
		case GPEX_TSG_ATOM_STATE_QUEUED:
			gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_IN, GPEX_TSG_INC);
			tsg.input_job_nr++;
			break;
		case GPEX_TSG_ATOM_STATE_IN_JS:
			gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_IN, GPEX_TSG_DEC);
			gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_OUT, GPEX_TSG_INC);
			tsg.input_job_nr--;
			if (tsg.input_job_nr < 0)
				tsg.input_job_nr = 0;
			break;
		case GPEX_TSG_ATOM_STATE_HW_COMPLETED:
			gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_OUT, GPEX_TSG_DEC);
			break;
		default:
			break;
		}
	}

	cur_in_nr = gpex_tsg_get_queue_nr(GPEX_TSG_QUEUE_IN);
	cur_out_nr = gpex_tsg_get_queue_nr(GPEX_TSG_QUEUE_OUT);

	if ((!both_q_active(prev_in_nr, prev_out_nr) && both_q_active(cur_in_nr, cur_out_nr)) ||
	    (!hw_q_active(prev_out_nr) && hw_q_active(cur_out_nr)))
		need_update = true;
	else if ((both_q_active(prev_in_nr, prev_out_nr) &&
		  !both_q_active(cur_in_nr, cur_out_nr)) ||
		 (hw_q_active(prev_out_nr) && !hw_q_active(cur_out_nr)))
		need_update = true;
	else if (prev_out_nr > cur_out_nr) {
		current_time = ktime_get();
		need_update =
			current_time - (gpex_tsg_get_queued_last_updated() > 2) * GPEX_TSG_PERIOD;
	}

	if (need_update) {
		current_time = (current_time == 0) ? ktime_get() : current_time;
		if (gpex_tsg_spin_lock()) {
			accum_queued_time(current_time, both_q_active(prev_in_nr, prev_out_nr),
					  hw_q_active(prev_out_nr));
			gpex_tsg_spin_unlock();
		}
	}

	if ((cur_in_nr + cur_out_nr) < 0)
		gpex_tsg_reset_count(0);

	return 0;
}

void gpex_tsg_update_firstjob_time(void)
{
	tsg.first_job_timestamp = ktime_get_real() / 1000UL;
}

void gpex_tsg_update_lastjob_time(int slot_nr)
{
	if (tsg.js_occupy == 0 || tsg.lastjob_starttimestamp == 0UL)
		tsg.lastjob_starttimestamp = ktime_get_real() / 1000UL;
	tsg.js_occupy |= 1 << slot_nr;
}

void gpex_tsg_update_jobsubmit_time(void)
{
	tsg.last_jobs_time = tsg.sum_jobs_time;
	tsg.lastjob_starttimestamp = 0UL;
	tsg.sum_jobs_time = 0UL;
}

void gpex_tsg_sum_jobs_time(int slot_nr)
{
	ktime_t nowtime = ktime_get_real() / 1000UL;

	if (tsg.first_job_timestamp == 0UL)
		tsg.first_job_timestamp = nowtime;

	tsg.js_occupy &= (slot_nr == 0) ? 2 : 1;

	if (tsg.js_occupy == 0 && tsg.lastjob_starttimestamp > 0UL) {
		tsg.sum_jobs_time += nowtime - tsg.lastjob_starttimestamp;
		tsg.lastjob_starttimestamp = 0UL;
	}
}

void gpex_tsg_stats_get_run_times(u64 *times)
{
	struct amigo_interframe_data last_frame;
	int cur_nrq = rtp_nrq;

	rtp_nrq = 0;
	rtp_sum_pre = 0;
	rtp_sum_cpu = 0;
	rtp_sum_v2s = 0;
	rtp_sum_gpu = 0;
	rtp_sum_v2f = 0;

	rtp_max_pre = 0;
	rtp_max_cpu = 0;
	rtp_max_v2s = 0;
	rtp_max_gpu = 0;
	rtp_max_v2f = 0;

	if (rtp_tail == 0)
		memcpy(&last_frame, &interframe[TABLE_MAX - 1],
				sizeof(struct amigo_interframe_data));
	else
		memcpy(&last_frame, &interframe[rtp_tail - 1],
				sizeof(struct amigo_interframe_data));

	if (cur_nrq > 0 && last_frame.nrq > 0) {
		ktime_t tmp;

		tmp = last_frame.sum_pre / last_frame.nrq;
		times[PREAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_cpu / last_frame.nrq;
		times[CPUAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_v2s / last_frame.nrq;
		times[V2SAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_gpu / last_frame.nrq;
		times[GPUAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_v2f / last_frame.nrq;
		times[V2FAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;

		times[PREMAX] = (last_frame.max_pre > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_pre;
		times[CPUMAX] = (last_frame.max_cpu > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_cpu;
		times[V2SMAX] = (last_frame.max_v2s > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_v2s;
		times[GPUMAX] = (last_frame.max_gpu > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_gpu;
		times[V2FMAX] = (last_frame.max_v2f > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_v2f;

		rtp_vsync_frame_counter = 0;
		rtp_vsync_counter = 0;

	} else {
		int i;

		for (i = 0; i < NUM_OF_TIMEINFO; i++)
			times[i] = 0;
	}

	rtp_last_minlock_cpu_maxfreq = 0;
	rtp_last_minlock_gpu_maxfreq = 0;
}

void gpex_tsg_stats_get_pid_list(u16 *pidlist)
{
#ifndef NUM_OF_PID
#define NUM_OF_PID 20
#endif

	/* TODO: 'memset' will be replaced by the codes came from pamir's pid list table. */
	memset(pidlist, 0, sizeof(u16) * NUM_OF_PID);
}

int gpex_tsg_amigo_interframe_sw_update(ktime_t start, ktime_t end)
{
	struct amigo_interframe_data *dst;
	ktime_t v2stime = 0;
	ktime_t v2ftime = 0;
	ktime_t pretime = (rtp_prev_swaptimestamp < start) ? start - rtp_prev_swaptimestamp : 0;
	ktime_t cputime = end - start;
	ktime_t gputime = tsg.last_jobs_time;

	atomic_set(&rtp_lasthw_read, 1);

	rtp_busy_cpuid = rtp_busy_cpuid_next;
	rtp_prev_swaptimestamp = end;
	rtp_vsync_swapcall_counter++;

	dst = &interframe[rtp_tail];
	rtp_tail = (rtp_tail + 1) % TABLE_MAX;
	if (rtp_tail == rtp_head)
		rtp_head = (rtp_head + 1) % TABLE_MAX;
	if (rtp_tail == rtp_lastshowidx)
		rtp_lastshowidx = (rtp_lastshowidx + 1) % TABLE_MAX;

	dst->sw_vsync = (rtp_vsync_lastsw == 0) ? rtp_vsync_prev : rtp_vsync_lastsw;
	rtp_vsync_lastsw = 0;
	dst->sw_start = start;
	dst->sw_end = end;
	//	dst->sw_total = total;
	v2stime = (dst->sw_vsync < dst->sw_end) ?
	dst->sw_end - dst->sw_vsync : dst->sw_end - rtp_vsync_prev;

	dst->hw_vsync = (rtp_vsync_lasthw == 0) ? rtp_vsync_prev : rtp_vsync_lasthw;
	dst->hw_start = rtp_lasthw_starttime;
	dst->hw_end = rtp_lasthw_endtime;
	dst->hw_total = gputime;
	v2ftime = (dst->hw_vsync < dst->hw_end) ?
	dst->hw_end - dst->hw_vsync : dst->hw_end - rtp_vsync_prev;

	if (rtp_nrq > 128) {
		rtp_nrq = 0;
		rtp_sum_pre = 0;
		rtp_sum_cpu = 0;
		rtp_sum_v2s = 0;
		rtp_sum_gpu = 0;
		rtp_sum_v2f = 0;

		rtp_max_pre = 0;
		rtp_max_cpu = 0;
		rtp_max_v2s = 0;
		rtp_max_gpu = 0;
		rtp_max_v2f = 0;

		rtp_last_minlock_cpu_maxfreq = 0;
		rtp_last_minlock_gpu_maxfreq = 0;
	}
	rtp_nrq++;

	rtp_sum_pre += pretime;
	rtp_sum_cpu += cputime;
	rtp_sum_v2s += v2stime;
	rtp_sum_gpu += gputime;
	rtp_sum_v2f += v2ftime;

	if (rtp_max_pre < pretime)
		rtp_max_pre = pretime;
	if (rtp_max_cpu < cputime)
		rtp_max_cpu = cputime;
	if (rtp_max_v2s < v2stime)
		rtp_max_v2s = v2stime;
	if (rtp_max_gpu < gputime)
		rtp_max_gpu = gputime;
	if (rtp_max_v2f < v2ftime)
		rtp_max_v2f = v2ftime;

	dst->nrq = rtp_nrq;
	dst->sum_pre = rtp_sum_pre;
	dst->sum_cpu = rtp_sum_cpu;
	dst->sum_v2s = rtp_sum_v2s;
	dst->sum_gpu = rtp_sum_gpu;
	dst->sum_v2f = rtp_sum_v2f;

	dst->max_pre = rtp_max_pre;
	dst->max_cpu = rtp_max_cpu;
	dst->max_v2s = rtp_max_v2s;
	dst->max_gpu = rtp_max_gpu;
	dst->max_v2f = rtp_max_v2f;
	dst->cputime = cputime;
	dst->gputime = gputime;
	rtp_last_cputime = cputime;
	rtp_last_gputime = gputime;

	dst->vsync_interval = rtp_vsync_interval;

//	gpex_tsg_amigo_shortterm(dst);

	return 0;
}

int gpex_tsg_amigo_interframe_hw_update_eof(void)
{
	ktime_t start = 0;
	ktime_t end = 0;
	ktime_t diff = ktime_get_real() - ktime_get();
	ktime_t term = 0;
	int time = atomic_read(&rtp_lasthw_totaltime);
	int read = atomic_read(&rtp_lasthw_read);

	atomic_set(&rtp_lasthw_read, 0);

	rtp_lasthw_starttime = (diff + rtp_curhw_starttime)/1000;
	rtp_lasthw_endtime = (diff + end)/1000;
	if (start < rtp_curhw_endtime)
		term = end - rtp_curhw_endtime;
	else
		term = end - start;
	rtp_curhw_totaltime += term;
	if (read == 1 || time < (int)(rtp_curhw_totaltime / 1000)) {
		time = (int)(rtp_curhw_totaltime / 1000);
		atomic_set(&rtp_lasthw_totaltime, time);
	}

	return 0;
}

int gpex_tsg_amigo_interframe_hw_update(void)
{
	ktime_t start = 0;
	ktime_t end = 0;
	ktime_t term = 0;

	if (rtp_curhw_starttime == 0)
		rtp_curhw_starttime = start;
	if (start < rtp_curhw_endtime)
		term = end - rtp_curhw_endtime;
	else
		term = end - start;
	rtp_curhw_totaltime += term;
	rtp_curhw_endtime = end;

	return 0;
}

void gpex_tsg_stats_set_vsync(ktime_t ktime_us)
{
	rtp_vsync_interval = ktime_us - rtp_vsync_prev;
	rtp_vsync_prev = ktime_us;

	if (rtp_vsync_lastsw == 0)
		rtp_vsync_lastsw = ktime_us;
	if (rtp_vsync_curhw == 0)
		rtp_vsync_curhw = ktime_us;

	if (rtp_vsync_swapcall_counter > 0) {
		rtp_vsync_frame_counter++;
		migov_vsync_frame_counter++;
	}
	rtp_vsync_swapcall_counter = 0;
	rtp_vsync_counter++;
	migov_vsync_counter++;
}

void gpex_tsg_stats_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	*nrframe = (s32)migov_vsync_frame_counter;
	*nrvsync = (u64)migov_vsync_counter;
	*delta_ms = (u64)(rtp_vsync_prev - migov_last_updated_vsync_time) / 1000;

	migov_vsync_frame_counter = 0;
	migov_vsync_counter = 0;
	migov_last_updated_vsync_time = rtp_vsync_prev;
}

void gpex_tsg_migov_set_targetframetime(int us)
{
	if (IS_STC_DISABLED)
		return;

	if (us < RENDERINGTIME_MAX_TIME && us > RENDERINGTIME_MIN_FRAME)
		rtp_target_frametime = us;
}

void gpex_tsg_migov_set_targettime_margin(int us)
{
	if (IS_STC_DISABLED)
		return;

	if (us < RENDERINGTIME_MAX_TIME && us > RENDERINGTIME_MIN_FRAME)
		rtp_targettime_margin = us;
}

void gpex_tsg_migov_set_util_margin(int percentage)
{
	if (IS_STC_DISABLED)
		return;

	rtp_workload_margin = percentage;
}

void gpex_tsg_migov_set_decon_time(int us)
{
	if (IS_STC_DISABLED)
		return;

	rtp_decon_time = us;
}

void gpex_tsg_migov_set_comb_ctrl(int val)
{
	if (IS_STC_DISABLED)
		rtp_shortterm_comb_ctrl = -1;
	else if (rtp_shortterm_comb_ctrl_manual == -1)
		rtp_shortterm_comb_ctrl = val;
	else
		rtp_shortterm_comb_ctrl = rtp_shortterm_comb_ctrl_manual;
}

#define COMB_CTRL_OFF_TMAX (9999999)
/* Start current DVFS level, find next DVFS lv met feasible region */
void sdp_get_next_feasible_freqlv(int *nextcpulv, int *nextgpulv)
{
	int targetframetime = rtp_target_frametime - rtp_targettime_margin;
	int tmax = targetframetime + targetframetime - rtp_decon_time;
	int lvsz_cpu = rtp_powertable_size[rtp_busy_cpuid];
	int lvsz_gpu = rtp_powertable_size[MIGOV_GPU];
	int fcpulv = rtp_cur_freqlv[rtp_busy_cpuid];
	int fgpulv = rtp_cur_freqlv[MIGOV_GPU];
	int fcpulvlimit = 0;
	int fgpulvlimit = 0;

	int next_cputime = rtp_last_cputime;
	int next_gputime = rtp_last_gputime;
	long cpu_workload = (long)rtp_powertable[rtp_busy_cpuid][fcpulv].freq
						* (long)rtp_last_cputime * (long)rtp_workload_margin / 100;
	long gpu_workload = (long)rtp_powertable[MIGOV_GPU][fgpulv].freq
						* (long)rtp_last_gputime * (long)rtp_workload_margin / 100;

	/* Move to feasible region */
	while (fcpulv > 0 && next_cputime > targetframetime) {
		fcpulv--;
		next_cputime = (int)(cpu_workload /
			rtp_powertable[rtp_busy_cpuid][fcpulv].freq);
	}

	while (fgpulv > 0 && next_gputime > targetframetime) {
		fgpulv--;
		next_gputime = (int)(gpu_workload /
			rtp_powertable[MIGOV_GPU][fgpulv].freq);
	}
	if (rtp_shortterm_comb_ctrl == 0)
		tmax = COMB_CTRL_OFF_TMAX;
	else {
		if (rtp_shortterm_comb_ctrl < fcpulv)
			fcpulvlimit = fcpulv - rtp_shortterm_comb_ctrl;
		if (rtp_shortterm_comb_ctrl < fgpulv)
			fgpulvlimit = fgpulv - rtp_shortterm_comb_ctrl;
		while ((next_cputime + next_gputime) > tmax) {
			if (fcpulv > fcpulvlimit && next_cputime > next_gputime) {
				fcpulv--;
				next_cputime = (int)(cpu_workload /
					rtp_powertable[rtp_busy_cpuid][fcpulv].freq);
			} else if (fgpulv > fgpulvlimit) {
				fgpulv--;
				next_gputime = (int)(gpu_workload /
					rtp_powertable[MIGOV_GPU][fgpulv].freq);
			} else
				break;
		}
	}

	/* Find min power in feasible region */
	while ((fcpulv + 1) < lvsz_cpu && (fgpulv + 1) < lvsz_gpu) {
		int pcpudec = rtp_powertable[rtp_busy_cpuid][fcpulv + 1].power +
					rtp_powertable[MIGOV_GPU][fgpulv].power;
		int pgpudec = rtp_powertable[rtp_busy_cpuid][fcpulv].power +
					rtp_powertable[MIGOV_GPU][fgpulv + 1].power;
		int est_cputime = (int)(cpu_workload / rtp_powertable[rtp_busy_cpuid][fcpulv + 1].freq);
		int est_gputime = (int)(gpu_workload / rtp_powertable[MIGOV_GPU][fgpulv + 1].freq);

		if (pcpudec < pgpudec && est_cputime < targetframetime && (est_cputime + next_gputime) < tmax) {
			fcpulv++;
			next_cputime = est_cputime;
		} else if (est_gputime < targetframetime && (next_cputime + est_gputime) < tmax) {
			fgpulv++;
			next_gputime = est_gputime;
		} else
			break;
	}

	while ((fcpulv + 1) < lvsz_cpu) {
		int est_cputime = (int)(cpu_workload / rtp_powertable[rtp_busy_cpuid][fcpulv + 1].freq);

		if (est_cputime < targetframetime && (est_cputime + next_gputime) < tmax) {
			fcpulv++;
			next_cputime = est_cputime;
		} else
			break;
	}

	while ((fgpulv + 1) < lvsz_gpu) {
		int est_gputime = (int)(gpu_workload / rtp_powertable[MIGOV_GPU][fgpulv + 1].freq);

		if (est_gputime < targetframetime && (next_cputime + est_gputime) < tmax) {
			fgpulv++;
			next_gputime = est_gputime;
		} else
			break;
	}

	*nextcpulv = fcpulv;
	*nextgpulv = fgpulv;
}

int gpex_tsg_amigo_shortterm(struct amigo_interframe_data *dst)
{
	int ret = 0;

	if ((gpex_tsg_get_is_gov_set() == 1) && (rtp_target_frametime > 0)) {
		int cur_fcpulv = rtp_cur_freqlv[rtp_busy_cpuid];
		int cur_fgpulv = rtp_cur_freqlv[MIGOV_GPU];
		int next_fcpulv = cur_fcpulv;
		int next_fgpulv = cur_fgpulv;

		dst->sdp_next_cpuid = rtp_busy_cpuid;
		dst->sdp_cur_fcpu = rtp_powertable[rtp_busy_cpuid][cur_fcpulv].freq;
		dst->sdp_cur_fgpu = rtp_powertable[MIGOV_GPU][cur_fgpulv].freq;

		if (rtp_shortterm_comb_ctrl >= 0) {
			sdp_get_next_feasible_freqlv(&next_fcpulv, &next_fgpulv);
		} else {
			next_fcpulv = rtp_powertable_size[rtp_busy_cpuid] - 1;
			next_fgpulv = rtp_powertable_size[MIGOV_GPU] - 1;
		}

		dst->sdp_next_fcpu = rtp_powertable[rtp_busy_cpuid][next_fcpulv].freq;
		dst->sdp_next_fgpu = rtp_powertable[MIGOV_GPU][next_fgpulv].freq;

		if (rtp_busy_cpuid == rtp_busy_cpuid_prev && rtp_last_minlock_cpu_maxfreq > dst->sdp_next_fcpu)
			dst->sdp_next_fcpu = rtp_last_minlock_cpu_maxfreq;
		if (rtp_last_minlock_gpu_maxfreq > dst->sdp_next_fgpu)
			dst->sdp_next_fgpu = rtp_last_minlock_gpu_maxfreq;

		if (dst->sdp_next_fgpu > rtp_max_minlock_freq)
			dst->sdp_next_fgpu = rtp_max_minlock_freq;

		if (rtp_prev_minlock_gpu_freq != dst->sdp_next_fgpu) {
//			if (sgpu_dvfs_governor_major_level_check(df, dst->sdp_next_fgpu))
			exynos_pm_qos_update_request(&exynos_amigo_gpu_min_qos,	dst->sdp_next_fgpu);
			rtp_prev_minlock_gpu_freq = dst->sdp_next_fgpu;
		}
		if (rtp_prev_minlock_cpu_freq != dst->sdp_next_fcpu || rtp_busy_cpuid != rtp_busy_cpuid_prev) {
			if (freq_qos_request_active(&cl1_min_pm_qos)) {
				if (rtp_busy_cpuid == MIGOV_CL1)
					freq_qos_update_request(&cl1_min_pm_qos, dst->sdp_next_fcpu);
				else if (rtp_busy_cpuid_prev == MIGOV_CL1)
					freq_qos_update_request(&cl1_min_pm_qos, 0);
				}
			if (freq_qos_request_active(&cl2_min_pm_qos)) {
				if (rtp_busy_cpuid == MIGOV_CL2)
					freq_qos_update_request(&cl2_min_pm_qos, dst->sdp_next_fcpu);
				else if (rtp_busy_cpuid_prev == MIGOV_CL2)
					freq_qos_update_request(&cl2_min_pm_qos, 0);
			}
			rtp_prev_minlock_cpu_freq = dst->sdp_next_fcpu;
			rtp_busy_cpuid_prev = rtp_busy_cpuid;
		}
		ret = dst->sdp_next_fgpu;
	} else {
		if (rtp_prev_minlock_cpu_freq > 0) {
			if (freq_qos_request_active(&cl1_min_pm_qos))
				freq_qos_update_request(&cl1_min_pm_qos, 0);
			if (freq_qos_request_active(&cl2_min_pm_qos))
				freq_qos_update_request(&cl2_min_pm_qos, 0);
			rtp_prev_minlock_cpu_freq = -1;
		}
		if (rtp_prev_minlock_gpu_freq > 0) {
			exynos_pm_qos_update_request(&exynos_amigo_gpu_min_qos, 0);
			rtp_prev_minlock_gpu_freq = -1;
		}
	}
	return ret;
}

static void gpex_tsg_profiler_init(void)
{
	/* GPU Profiler */
	tsg.lastshowidx = 0;
	tsg.nr_q = 0;
	tsg.js_occupy = 0;
}

static void gpex_tsg_context_init(void)
{
	gpex_tsg_set_weight_table_idx(0, gpexbe_devicetree_get_int(gpu_weight_table_idx_0));
	gpex_tsg_set_weight_table_idx(1, gpexbe_devicetree_get_int(gpu_weight_table_idx_1));
	gpex_tsg_set_governor_type_init(gpex_dvfs_get_governor_type());
	gpex_tsg_set_queued_threshold(0, 0);
	gpex_tsg_set_queued_threshold(1, 0);
	gpex_tsg_set_queued_time_tick(0, 0);
	gpex_tsg_set_queued_time_tick(1, 0);
	gpex_tsg_set_queued_time(0, 0);
	gpex_tsg_set_queued_time(1, 0);
	gpex_tsg_set_queued_last_updated(0);
	gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_JOB, GPEX_TSG_RST);
	gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_IN, GPEX_TSG_RST);
	gpex_tsg_set_queue_nr(GPEX_TSG_QUEUE_OUT, GPEX_TSG_RST);
	gpex_tsg_profiler_init();

	tsg.input_job_nr = 0;
	tsg.input_job_nr_acc = 0;
}

struct amigo_interframe_data *gpex_tsg_amigo_get_next_frameinfo(void)
{
	struct amigo_interframe_data *dst;

	if (rtp_head == rtp_tail)
		return NULL;

	dst = &interframe[rtp_head];
	rtp_head = (rtp_head + 1) % TABLE_MAX;

	return dst;
}

int gpex_tsg_amigo_get_target_frametime(void)
{
	return rtp_target_frametime;
}

void gpex_tsg_sdp_set_powertable(int id, int cnt, struct freq_table *table)
{
	int i;

	if (rtp_powertable[id] == NULL || rtp_powertable_size[id] < cnt) {
		if (rtp_powertable[id] != NULL)
			kfree(rtp_powertable[id]);
		rtp_powertable[id] = kcalloc(cnt, sizeof(struct amigo_freq_estpower), GFP_KERNEL);
		rtp_powertable_size[id] = cnt;
	}

	for (i = 0; rtp_powertable[id] != NULL && i < cnt; i++) {
		rtp_powertable[id][i].freq = table[i].freq;
		rtp_powertable[id][i].power = table[i].dyn_cost + table[i].st_cost;
	}
}

void gpex_tsg_sdp_set_busy_domain(int id)
{
	rtp_busy_cpuid_next = id;
}

void gpex_tsg_sdp_set_cur_freqlv(int id, int idx)
{
	rtp_cur_freqlv[id] = idx;
}

int gpex_tsg_stc_config_show(int page_size, char *buf)
{
	int ret;

	ret = snprintf(buf, page_size, "[0]comb = %d / cur=%d\n",
		rtp_shortterm_comb_ctrl_manual, rtp_shortterm_comb_ctrl);
	ret += snprintf(buf + ret, page_size, "[1]workload_margin = %d %%\n", rtp_workload_margin);
	ret += snprintf(buf + ret, page_size, "[2]decon_time = %d ns\n", rtp_decon_time);
	ret += snprintf(buf + ret, page_size, "[3]target_frametime_margin = %d ns\n", rtp_targettime_margin);
	ret += snprintf(buf + ret, page_size, "[4]max min_lock = %d MHz\n", rtp_max_minlock_freq);
	ret += snprintf(buf + ret, page_size, "target_frametime = %d ns\n", rtp_target_frametime);

	return 0;
}

int gpex_tsg_stc_config_store(const char *buf)
{
	int id, val;

	if (sscanf(buf, "%d %d", &id, &val) != 2)
		return -1;

	switch (id) {
	case 0:
		if (val >= -2) {
			rtp_shortterm_comb_ctrl_manual = val;
			if (val != -1)
				rtp_shortterm_comb_ctrl = val;
		}
		break;
	case 1:
		if (val >= 50 && val <= 150)
			rtp_workload_margin = val;
		break;
	case 2:
		if (val >= 0 && val <= 16000)
			rtp_decon_time = val;
		break;
	case 3:
		if (val >= 0 && val <= 16000)
			rtp_targettime_margin = val;
		break;
	case 4:
		if (val >= 0 && val <= 999000)
			rtp_max_minlock_freq = val;
		break;
	}

	return 0;
}

int gpex_tsg_init(struct device **dev)
{
	raw_spin_lock_init(&tsg.spinlock);
	tsg.kbdev = container_of(dev, struct kbase_device, dev);

	gpex_tsg_context_init();
	gpex_tsg_external_init(&tsg);
	gpex_tsg_sysfs_init(&tsg);

	return 0;
}

int gpex_tsg_term(void)
{
	gpex_tsg_sysfs_term();
	gpex_tsg_external_term();
	tsg.kbdev = NULL;

	return 0;
}

