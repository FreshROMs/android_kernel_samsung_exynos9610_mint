/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL3 Monitor support
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_EL3_MON_H
#define __EXYNOS_EL3_MON_H

/* Error code */
#define EXYNOS_ERROR_TZASC_WRONG_REGION		(-1)

#define EXYNOS_ERROR_ALREADY_INITIALIZED	(1)
#define EXYNOS_ERROR_NOT_VALID_ADDRESS		(0x1000)

/* Parameters for TZPC of power domains */
#define EXYNOS_GET_IN_PD_DOWN			(0)
#define EXYNOS_WAKEUP_PD_DOWN			(1)

#define EXYNOS_PD_RUNTIME_PM			(0)
#define EXYNOS_PD_CONNECTIVITY			(1)

/* Connectivity sub system */
#define EXYNOS_GNSS				(0)
#define EXYNOS_WLBT				(1)
#define EXYNOS_SHUB				(u64)(2)
/* Target to set */
#define EXYNOS_SET_CONN_TZPC			(0)

/* Mask */
#define SHIFT_CONN_IP				(32)
#define MASK_CONN_IP_TARGET			(0xFFFFFFFF)

#ifndef __ASSEMBLY__
enum __error_flag {
	CONN_NO_ERROR = 0,
	CONN_ALREADY_SET,
	CONN_INVALID_TARGET,
	CONN_INVALID_CONN_IP
};

int exynos_pd_tz_save(unsigned int addr);
int exynos_pd_tz_restore(unsigned int addr);

int exynos_conn_tz_save(unsigned int addr);
int exynos_conn_tz_restore(unsigned int addr);
#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_EL3_MON_H */
