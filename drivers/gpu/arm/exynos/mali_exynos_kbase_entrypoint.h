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

#ifndef _MALI_EXYNOS_KBASE_ENTRYPOINTS_H_
#define _MALI_EXYNOS_KBASE_ENTRYPOINTS_H_

#include <mali_kbase.h>

void mali_exynos_update_job_load(struct kbase_jd_atom *katom, ktime_t *end_timestamp);

int mali_exynos_set_pm_state_resume_begin(void);
int mali_exynos_set_pm_state_resume_end(void);

/* LLC + Coherency entry points */
void mali_exynos_coherency_reg_map(void);
void mali_exynos_coherency_reg_unmap(void);
void mali_exynos_coherency_set_coherency_feature(void);
void mali_exynos_llc_set_aruser(void);
void mali_exynos_llc_set_awuser(void);

/* GTS entry points */
void mali_exynos_update_jobslot_util(int slot, bool gpu_active, u32 ns_time);
void mali_exynos_set_jobslot_status(int slot, bool is_active);

/* TSG entry points */
int mali_exynos_set_count(struct kbase_jd_atom *katom, u32 status, bool stop);
int mali_exynos_ioctl(struct kbase_context *kctx, unsigned int cmd, unsigned long arg);

void mali_exynos_update_firstjob_vsync_time(void);
void mali_exynos_update_firstjob_time(void);
void mali_exynos_update_lastjob_time(int slot_nr);
void mali_exynos_update_jobsubmit_time(void);
void mali_exynos_sum_jobs_time(int slot_nr);
void mali_exynos_amigo_interframe_hw_update_eof(void);
void mali_exynos_amigo_interframe_hw_update(void);

/* G3D state reader */
int mali_exynos_get_gpu_power_state(void);

/* cmar boost */
void mali_exynos_set_thread_priority(struct kbase_context *kctx);

/* cmar cpu affinity */
void mali_exynos_set_thread_affinity(void);

/* secure rendering */
int mali_exynos_legacy_jm_enter_protected_mode(struct kbase_device *kbdev);
int mali_exynos_legacy_jm_exit_protected_mode(struct kbase_device *kbdev);
int mali_exynos_legacy_pm_exit_protected_mode(struct kbase_device *kbdev);
struct protected_mode_ops *mali_exynos_get_protected_ops(void);

/* dmabuf */
bool mali_exynos_dmabuf_is_cached(struct dma_buf *dmabuf);

/* gpu_model sysfs */
typedef ssize_t (*sysfs_read_func)(struct device *, struct device_attribute *, char *);
void mali_exynos_sysfs_set_gpu_model_callback(sysfs_read_func show_gpu_model_fn);

/* debug */
void mali_exynos_debug_print_info(struct kbase_device *kbdev);

#endif /* _MALI_EXYNOS_KBASE_ENTRYPOINTS_H_ */
