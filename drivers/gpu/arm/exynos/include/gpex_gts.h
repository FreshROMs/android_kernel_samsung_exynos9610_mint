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

#ifndef _GPEX_GTS_H_
#define _GPEX_GTS_H_

#include <linux/device.h>
#include <mali_exynos_ioctl.h>

#define HCM_MODE_A (1 << 0)
#define HCM_MODE_B (1 << 1)
#define HCM_MODE_C (1 << 2)

/**
 * gpex_gts_get_ioctl_gts_info() - get current system info required for gts to work
 * @info: structure to fill in with current system info
 *
 * This function is called via ioctl from user space to pass the system info up to
 * the userspace for further processing
 */
int gpex_gts_get_ioctl_gts_info(struct mali_exynos_ioctl_gts_info *info);

/**
 * gpex_gts_update_jobslot_util() - update gpu job slot utilization data
 * @gpu_active: whether gpu was active or idle
 * @ns_time: amount of time in ns the gpu was active or idle for
 *
 * data collected through this function is used for overall operation of GTS
 */
void gpex_gts_update_jobslot_util(bool gpu_active, u32 ns_time);

/**
 * gpex_gts_set_jobslot_status() - set the jobslot status to active or inactive
 * @is_active: pass true for marking jobslot as active. false otherwise
 *
 * data collected here is used for overall operation of GTS
 */
void gpex_gts_set_jobslot_status(bool is_active);

/**
 * gpex_gts_clear() - clear the jobslot utilization to 0
 */
void gpex_gts_clear(void);

/**
 * gpex_gts_set_hcm_mode() - set the mode of heavy compute mode
 * @hcm_mode_val: hcm mode to set to
 *
 * HCM_MODE_A (1 << 0)
 * HCM_MODE_B (1 << 1)
 * HCM_MODE_C (1 << 2)
 */
void gpex_gts_set_hcm_mode(int hcm_mode_val);

/**
 * gpex_gts_get_hcm_mode() - get the current heavy compute mode
 *
 * Return: current heavy compute mode
 */
int gpex_gts_get_hcm_mode(void);

/**
 * gpex_gts_update_gpu_data() - calculate the values required for gts using current status
 *
 * values calculated here are stored internally, and then are read by user space through ioctl
 * (via gpex_gts_get_ioctl_gts_info function)
 *
 * Return: true if all values have been updated (return value not used currently)
 */
bool gpex_gts_update_gpu_data(void);

/**
 * gpex_gts_init() - initializes gpex_gts module
 * @dev: mali device struct
 *
 * Return: 0 on success
 */
int gpex_gts_init(struct device **dev);

/**
 * gpex_gts_term() - terminates gpex_gts module
 */
void gpex_gts_term(void);

#endif /* _GPEX_GTS_H_ */
