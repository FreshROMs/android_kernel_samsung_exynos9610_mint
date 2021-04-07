/*
 * Copyright (C) 2015 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_ISA1000_H
#define _LINUX_ISA1000_H

#define MAX_TIMEOUT	10000

#define VIB_BUFSIZE	30

struct isa1000_pdata {
	u32 duty;
 	u32 period;
 	u32 max_timeout;
 	u32 pwm_id;
 	u32 pwm_use;
 	int gpio_en;
	int min_value;
	int min_intensity;
	const char* regulator_name;
	const char* vib_type;
};

#endif	/* _LINUX_ISA1000_H */

