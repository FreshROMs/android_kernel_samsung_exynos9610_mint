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

#ifndef _GPEXBE_CLOCK_H_
#define _GPEXBE_CLOCK_H_

struct freq_volt {
	int freq;
	int volt;
};

/**
 * gpexbe_clock_init() - initializes gpexbe_clock module
 *
 * Initializes based on values set in device tree and values returned by cal-if
 *
 * Return: 0 on success
 */
int gpexbe_clock_init(void);

/**
 * gpexbe_clock_term() - terminates gpexbe_clock module
 */
void gpexbe_clock_term(void);

/**
 * gpexbe_clock_get_rate_asv_table() - constructs freq-volt table
 * @fv_array: array to be filled by function
 * @level_num: number of freq-volt levels to fill
 *
 * Constructs frequency-voltage table by getting the information from cal-if.
 *
 * Return: 0 on success
 */
int gpexbe_clock_get_rate_asv_table(struct freq_volt *fv_array, int level_num);

/**
 * gpexbe_clock_get_level_num() - get number of gpu clock levels
 *
 * Get number of levels in clock table from cal-if.
 * This is independent from number of levels set in mali dvfs table from
 * the device tree.
 *
 * Return: number of clock levels
 */
int gpexbe_clock_get_level_num(void);

/**
 * gpexbe_clock_get_boot_freq() - get gpu boot frequency (HW)
 *
 * Return: gpu boot frequency
 */
int gpexbe_clock_get_boot_freq(void);

/**
 * gpexbe_clock_get_max_freq() - get max gpu frequency (HW)
 *
 * Max GPU HW frequency. This is not affected by SW frequency max locks
 *
 * Return: max HW gpu frequency
 */
int gpexbe_clock_get_max_freq(void);

/**
 * gpexbe_clock_set_rate() - set gpu clock to given value
 * @clk: target gpu clock
 *
 * Return: 0 on success
 */
int gpexbe_clock_set_rate(int clk);

/**
 * gpexbe_clock_get_rate() - get current gpu clock from cal-if
 *
 * Return: current gpu clock
 */
int gpexbe_clock_get_rate(void);

#endif /* _GPEXBE_CLOCK_H_ */
