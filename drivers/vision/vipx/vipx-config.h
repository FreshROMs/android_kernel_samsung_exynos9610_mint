/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_CONFIG_H__
#define __VIPX_CONFIG_H__

/* CONFIG - GLOBAL OPTIONS */
#define VIPX_MAX_BUFFER			(16)
#define VIPX_MAX_GRAPH			(32)
#define VIPX_MAX_TASK			VIPX_MAX_BUFFER
#define VIPX_MAX_PLANES			(3)

#define VIPX_MAX_PLANES			(3)
#define VIPX_MAX_TASKDESC		(VIPX_MAX_GRAPH * 2)

#define VIPX_MAX_CONTEXT		(16)

/* CONFIG - PLATFORM CONFIG */
#define VIPX_STOP_WAIT_COUNT		(200)
//#define VIPX_MBOX_EMULATOR

/* CONFIG - DEBUG OPTIONS */
//#define DEBUG_TIME_MEASURE
//#define DEBUG_LOG_IO_WRITE
//#define DEBUG_LOG_DUMP_REGION
//#define DEBUG_LOG_MEMORY
//#define DEBUG_LOG_CALL_TREE
//#define DEBUG_LOG_MAILBOX_DUMP

#endif
