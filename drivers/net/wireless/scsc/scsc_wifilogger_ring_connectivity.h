/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef __SCSC_WIFILOGGER_RING_CONNECTIVITY_H__
#define __SCSC_WIFILOGGER_RING_CONNECTIVITY_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <scsc/scsc_logring.h>

#include "scsc_wifilogger_core.h"
#include "scsc_wifilogger.h"

#define	MAX_TLVS_SZ			1024
#define WLOGGER_RCONNECT_NAME		"connectivity"

/**
 * A local mirror for this ring's current verbose level:
 * avoids func-call and minimizes impact when ring is disabled
 */
extern u32 cring_lev;

bool scsc_wifilogger_ring_connectivity_init(void);

int scsc_wifilogger_ring_connectivity_driver_event(wlog_verbose_level lev,
						   u16 driver_event_id, unsigned int tag_count, ...);

int scsc_wifilogger_ring_connectivity_fw_event(wlog_verbose_level lev, u16 fw_event_id,
					       u64 fw_timestamp, void *fw_bulk_data, size_t fw_blen);

#endif /* __SCSC_WIFILOGGER_RING_CONNECTIVITY_H__ */
