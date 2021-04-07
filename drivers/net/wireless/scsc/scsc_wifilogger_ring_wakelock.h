/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef __SCSC_WIFILOGGER_RING_WAKELOCK_H__
#define __SCSC_WIFILOGGER_RING_WAKELOCK_H__

#include <linux/types.h>
#include <scsc/scsc_logring.h>

#include "scsc_wifilogger_core.h"
#include "scsc_wifilogger.h"

#define WLOGGER_RWAKELOCK_NAME		"wakelock"

extern u32 wring_lev;

int scsc_wifilogger_ring_wakelock_action(u32 verbose_level, int status,
					 char *wl_name, int reason);


bool scsc_wifilogger_ring_wakelock_init(void);
#endif /* __SCSC_WIFILOGGER_RING_WAKELOCK_H__ */

