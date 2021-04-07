/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_LOG_H__
#define __VIPX_LOG_H__

#include <linux/kernel.h>

#include "vipx-config.h"
#include "vipx-debug.h"

#define vipx_err(fmt, args...) \
	pr_err("[VIPx][ERR](%d):" fmt, __LINE__, ##args)

#define vipx_warn(fmt, args...) \
	pr_warn("[VIPx][WRN](%d):" fmt, __LINE__, ##args)

#define vipx_info(fmt, args...) \
	pr_info("[VIPx]:" fmt, ##args)

#define vipx_dbg(fmt, args...)						\
do {									\
	if (vipx_debug_log_enable)					\
		pr_info("[VIPx][DBG](%d):" fmt, __LINE__, ##args);	\
} while (0)

#if defined(DEBUG_LOG_CALL_TREE)
#define vipx_enter()		vipx_info("[%s] enter\n", __func__)
#define vipx_leave()		vipx_info("[%s] leave\n", __func__)
#define vipx_check()		vipx_info("[%s] check\n", __func__)
#else
#define vipx_enter()
#define vipx_leave()
#define vipx_check()
#endif

#endif
