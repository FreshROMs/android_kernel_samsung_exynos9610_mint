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

#ifndef _GPEXBE_UTILIZATION_H_
#define _GPEXBE_UTILIZATION_H_

#include <linux/device.h>
#include <mali_kbase.h>

/**
 * gpexbe_utilization_init() - initializes gpexbe_utilization backend module
 * @dev: mali device structure
 *
 * Return: 0 on success
 */
int gpexbe_utilization_init(struct device **dev);

/**
 * gpexbe_utilization_term() - terminates gpexbe_utilization backend module
 */
void gpexbe_utilization_term(void);

/**
 * gpexbe_utilization_calc_utilization() - calculate and return the current overall GPU utilization
 *
 * Return: GPU utilization from 0 to 100
 */
int gpexbe_utilization_calc_utilization(void);

/**
 * gpexbe_utilization_calculate_compute_ratio() - calculate pure compute and graphics utilization ratio
 *
 * Results are stored within internal structure to be read by other gpex modules
 * Value of 100 means current workload is 100% pure compute (OpenCL)
 * Information on time spend for compute, vertex and fragment jobs are reset after this function call.
 */
void gpexbe_utilization_calculate_compute_ratio(void);

/**
 * gpexbe_utilization_update_job_load() - update current gpu job load information (called from mali_kbase)
 * @katom: pointer to mali katom
 * @end_timestamp: time of katom job completion
 *
 * Information updated by this function is stored in an internal structure for use by other gpexbe_utilization functions
 * This function is called from mali_kbase when a katom is completed by the GPU
 */
void gpexbe_utilization_update_job_load(struct kbase_jd_atom *katom, ktime_t *end_timestamp);

/**
 * gpexbe_utilization_get_compute_job_time() - get time spent by GPU on compute jobs since previous compute ratio calculation
 *
 * Measures time spent on GPU compute jobs.
 * Time spent is reset in gpexbe_utilization_calculate_compute_ratio (for now. This might change in the future)
 */
int gpexbe_utilization_get_compute_job_time(void);

/**
 * gpexbe_utilization_get_vertex_job_time() - get time spent by GPU on vertex jobs since previous compute ratio calculation
 *
 * Measures time spent on GPU vertex jobs.
 * Time spent is reset in gpexbe_utilization_calculate_compute_ratio (for now. This might change in the future)
 */
int gpexbe_utilization_get_vertex_job_time(void);

/**
 * gpexbe_utilization_get_fragment_job_time() - get time spent by GPU on fragment jobs since previous compute ratio calculation
 *
 * Measures time spent on GPU fragment jobs.
 * Time spent is reset in gpexbe_utilization_calculate_compute_ratio (for now. This might change in the future)
 */
int gpexbe_utilization_get_fragment_job_time(void);

/**
 * gpexbe_utilization_get_compute_job_cnt() - get the accumulated compute job cnt since the last call to this function
 *
 * This function also resets the accumulated compute job count to 0
 *
 * Return: accumulated compute job count
 */
int gpexbe_utilization_get_compute_job_cnt(void);

/**
 * gpexbe_utilization_get_vertex_job_cnt() - get the accumulated vertex job cnt since the last call to this function
 *
 * This function also resets the accumulated vertex job count to 0
 *
 * Return: accumulated vertex job count
 */
int gpexbe_utilization_get_vertex_job_cnt(void);
/**
 * gpexbe_utilization_get_fragment_job_cnt() - get the accumulated fragment job cnt since the last call to this function
 *
 * This function also resets the accumulated fragment job count to 0
 *
 * Return: accumulated fragment job count
 */
int gpexbe_utilization_get_fragment_job_cnt(void);

/**
 * gpexbe_utilization_get_utilization() - get current overall GPU utilization (store value)
 *
 * This function returns the stored utilization value from the most recent utilization calculation.
 * It does not calculate the utilization.
 *
 * Return: current GPU utilization value from 0 to 100
 */
int gpexbe_utilization_get_utilization(void);

/**
 * gpexbe_utilization_get_pure_compute_time_rate() - get current compute to graphic job ratio
 *
 * This function returns the stored compute<->graphic job ratio value from the most recent ratio calculation.
 * It does not calculate the ratio.
 *
 * Return: current compute ratio from 0 to 100 (100 is 100% compute job)
 */
int gpexbe_utilization_get_pure_compute_time_rate(void);

#endif /* _GPEXBE_UTILIZATION_H_ */
