// SPDX-License-Identifier: GPL-2.0

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

#ifndef _GPEXWA_INTERACTIVE_BOOST_H_
#define _GPEXWA_INTERACTIVE_BOOST_H_

#if IS_ENABLED(CONFIG_MALI_EXYNOS_INTERACTIVE_BOOST)
int gpexwa_interactive_boost_set(int duration);
int gpexwa_interactive_boost_init(void);
void gpexwa_interactive_boost_term(void);
#else
#define gpexwa_interactive_boost_set(...) 0
#define gpexwa_interactive_boost_init(...) (void)0
#define gpexwa_interactive_boost_term(...) (void)0
#endif

#endif /* _GPEXWA_INTERACTIVE_BOOST_H_ */
