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

#ifndef _GPEXBE_NOTIFIER_H_
#define _GPEXBE_NOTIFIER_H_

/**
 * gpexbe_notifier_init() - initializes notifier module
 *
 * Notifier module creates and registers pmqos and thermal notifiers
 *
 * Return: 0 on successs
 */
int gpexbe_notifier_init(void);

/**
 * gpexbe_notifier_term() - terminates notifier module
 */
void gpexbe_notifier_term(void);

#endif /* _GPEXBE_NOTIFIER_H_ */
