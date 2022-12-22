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

#ifndef _GPEX_CMAR_SCHED_H_
#define _GPEX_CMAR_SCHED_H_

/**
 * gpex_cmar_sched_set_forced_sched() - enable or disable forced schedule
 * @mode: 0 for disable, enable otherwise
 *
 * Return: 0 on success
 */
int gpex_cmar_sched_set_forced_sched(int mode);

/**
 * gpex_cmar_sched_set_affinity() - set cpu affinity depending on current mask
 *
 * Return: 0 on success
 */
int gpex_cmar_sched_set_affinity(void);

/**
 * gpex_cmar_sched_init() - initializes gpex_cmar_sched module
 *
 * Return: 0 on success
 */
int gpex_cmar_sched_init(void);

/**
 * gpex_cmar_sched_term() - terminates gpex_cmar_sched module
 */
void gpex_cmar_sched_term(void);

#endif /* _GPEX_CMAR_SCHED_H_ */
