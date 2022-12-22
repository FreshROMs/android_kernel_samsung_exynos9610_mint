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

#ifndef _GPEXWA_WAKEUP_CLOCK_WA_H_
#define _GPEXWA_WAKEUP_CLOCK_WA_H_

/**
 * gpexwa_wakeup_clock_suspend() - set the clock to middle gpu clock for suspend
 *
 * This function should be called before going into suspend.
 * middle gpu clock is different for each SOC
 */
void gpexwa_wakeup_clock_suspend(void);

/**
 * gpexwa_wakeup_clock_set() - set the clock to middle gpu clock for rtpm power off
 *
 * This function should be called before going into rtpm power off
 * middle gpu clock is different for each SOC
 */
void gpexwa_wakeup_clock_set(void);

/**
 * gpexwa_wakeup_clock_restore() - set the clock to wakeup clock
 *
 * This function should be called after gpu power on.
 * If the clock before going into suspend or rtpm power off (before wakeup_clock_set/suspend is called)
 * was higher than the middle clock, than that clock value is restored.
 * Otherwise clock is set to middle gpu clock.
 */
void gpexwa_wakeup_clock_restore(void);

#endif /* _GPEXWA_WAKEUP_CLOCK_WA_H_ */
