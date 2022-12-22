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

#ifndef _GPEXBE_QOS_H_
#define _GPEXBE_QOS_H_

#include <linux/types.h>

typedef enum {
	PMQOS_MIN = 1 << 0,
	PMQOS_MAX = 1 << 1,
	PMQOS_LITTLE = 1 << 2,
	PMQOS_MIDDLE = 1 << 3,
	PMQOS_BIG = 1 << 4,
	PMQOS_MIF = 1 << 5,
} mali_pmqos_flags;

/**
 * gpexbe_qos_init() - initializes quality of service (qos) backend module
 *
 * Return: 0 on successs
 */
int gpexbe_qos_init(void);

/**
 * gpexbe_qos_term() - terminates quality of service (qos) backend module
 */
void gpexbe_qos_term(void);

/**
 * gpexbe_qos_request_add() - add/registers qos requests to other IPs
 * @type: type of QOS to add (e.g: MIF, LITTLE CPU, MIN, MAX etc)
 *
 * Meant to be called once during initialization process during boot
 */
void gpexbe_qos_request_add(mali_pmqos_flags type);

/**
 * gpexbe_qos_request_remove() - remove/unregisters qos requests to other IPs
 * @type: type of QOS to remove (e.g: MIF, LITTLE CPU, MIN, MAX etc)
 *
 * Meant to be called once during termination process during boot
 */
void gpexbe_qos_request_remove(mali_pmqos_flags type);

/**
 * gpexbe_qos_request_update() - request QOS to other IPs
 * @type: type of QOS request (IP and Min or Max lock)
 * @clock: value of clock to request to target IP
 */
void gpexbe_qos_request_update(mali_pmqos_flags type, s32 clock);

/**
 * gpexbe_qos_request_unset() - unset QOS to other IPs
 * @type: type of QOS request to unset
 */
void gpexbe_qos_request_unset(mali_pmqos_flags type);

#endif /* _GPEXBE_QOS_H_ */
