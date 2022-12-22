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

#ifndef _GPEX_CMAR_BOOST_H_
#define _GPEX_CMAR_BOOST_H_

struct platform_context;

/**
 * gpex_cmar_boost_set_flag() - request cmar boost action
 * @pctx: platform_context struct
 * @request: CMAR_BOOST_SET_RT or CMAR_BOOST_SET_DEFAULT
 *
 * Return: 0 on success
 */
int gpex_cmar_boost_set_flag(struct platform_context *pctx, int request);

/**
 * gpex_cmar_boost_set_thread_priority() - set thread priority using the last cmar boost request given
 * @pctx: platform_context struct
 */
void gpex_cmar_boost_set_thread_priority(struct platform_context *pctx);

#endif /* _GPEX_CMAR_BOOST_H_ */
