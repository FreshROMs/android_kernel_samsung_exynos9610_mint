/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef _SCSC_WIFILOGGER_INTERNAL_H_
#define _SCSC_WIFILOGGER_INTERNAL_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/bitops.h>

#include "scsc_wifilogger_types.h"
#include "scsc_wifilogger_core.h"
#include "scsc_wifilogger_debugfs.h"

struct scsc_wifi_logger {
	bool initialized;
	struct mutex lock;
	unsigned int features_mask;
	struct scsc_wlog_ring *rings[MAX_WIFI_LOGGER_RINGS];

	/**
	 * There is only one log_handler and alert_handler registered
	 * to be used across all rings: moreover just one instance of
	 * these handlers can exist per-ring.
	 */

	/* log_handler callback registered by framework */
	void (*on_ring_buffer_data_cb)(char *ring_name, char *buffer,
				       int buffer_size,
				       struct scsc_wifi_ring_buffer_status *status,
				       void *ctx);
	/* alert_handler callback registered by framework */
	void (*on_alert_cb)(char *buffer, int buffer_size, int err_code, void *ctx);

	void *on_ring_buffer_ctx;
	void *on_alert_ctx;
};

bool scsc_wifilogger_init(void);
void scsc_wifilogger_destroy(void);
bool scsc_wifilogger_fw_alert_init(void);
struct scsc_wifi_logger *scsc_wifilogger_get_handle(void);
unsigned int scsc_wifilogger_get_features(void);
bool scsc_wifilogger_register_ring(struct scsc_wlog_ring *r);
struct scsc_wlog_ring *scsc_wifilogger_get_ring_from_name(char *name);
bool scsc_wifilogger_get_rings_status(u32 *num_rings,
				      struct scsc_wifi_ring_buffer_status *status);

#endif /* _SCSC_WIFILOGGER_INTERNAL_H_ */
