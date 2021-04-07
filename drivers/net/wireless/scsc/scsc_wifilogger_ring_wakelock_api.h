/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef __SCSC_WIFILOGGER_RING_WAKELOCK_API_H__
#define __SCSC_WIFILOGGER_RING_WAKELOCK_API_H__

/**
 * Android Enhanced Logging
 *
 *  WAKELOCK EVENTS RING -- Public Producer API
 *
 */
enum {
	WL_TAKEN = 0,
	WL_RELEASED
};

enum {
	WL_REASON_TX = 0,
	WL_REASON_RX,
	WL_REASON_ROAM
};

#if IS_ENABLED(CONFIG_SCSC_WIFILOGGER)

#include "scsc_wifilogger_ring_wakelock.h"

#define SCSC_WLOG_WAKELOCK(lev, status, wl_name, reason) \
	do { \
		if (wring_lev && (lev) <= wring_lev) \
			scsc_wifilogger_ring_wakelock_action((lev), (status), (wl_name), (reason)); \
	} while (0)

#else

#define SCSC_WLOG_WAKELOCK(lev, status, wl_name, reason)	do {} while (0)

#endif

#endif /* __SCSC_WIFILOGGER_RING_WAKELOCK_API_H__ */
