/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 *****************************************************************************/
/* Implements */
#include "scsc_wifilogger_ring_wakelock.h"

/* Uses */
#include "scsc_wifilogger_ring_wakelock_api.h"
#include "scsc_wifilogger_internal.h"

static struct scsc_wlog_ring *the_ring;

u32 wring_lev;
EXPORT_SYMBOL(wring_lev);

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
#include "scsc_wifilogger_debugfs.h"

static struct scsc_wlog_debugfs_info di;

#endif /* CONFIG_SCSC_WIFILOGGER_DEBUGFS */

bool scsc_wifilogger_ring_wakelock_init(void)
{
	struct scsc_wlog_ring *r = NULL;
#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	struct scsc_ring_test_object *rto = NULL;
#endif

	r = scsc_wlog_ring_create(WLOGGER_RWAKELOCK_NAME,
				  RING_BUFFER_ENTRY_FLAGS_HAS_BINARY,
				  ENTRY_TYPE_WAKE_LOCK, 65536,
				  WIFI_LOGGER_WAKE_LOCK_SUPPORTED,
				  NULL, NULL, NULL);

	if (!r) {
		SCSC_TAG_ERR(WLOG, "Failed to CREATE WiFiLogger ring: %s\n",
			     WLOGGER_RWAKELOCK_NAME);
		return false;
	}
	scsc_wlog_register_verbosity_reference(r, &wring_lev);

	if (!scsc_wifilogger_register_ring(r)) {
		SCSC_TAG_ERR(WLOG, "Failed to REGISTER WiFiLogger ring: %s\n",
			     WLOGGER_RWAKELOCK_NAME);
		scsc_wlog_ring_destroy(r);
		return false;
	}
	the_ring = r;

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	rto = init_ring_test_object(the_ring);
	if (rto)
		scsc_register_common_debugfs_entries(the_ring->st.name, rto, &di);
#endif

	return true;
}

/**** Producer API ******/

int scsc_wifilogger_ring_wakelock_action(u32 verbose_level, int status,
					 char *wl_name, int reason)
{
	u64 timestamp;
	struct scsc_wake_lock_event wl_event;

	if (!the_ring)
		return 0;

	timestamp = local_clock();
	SCSC_TAG_DBG4(WLOG, "EL -- WAKELOCK[%s] - status:%d   reason:%d   @0x%x\n",
		      wl_name, status, reason, timestamp);

	wl_event.status = status;
	wl_event.reason = reason;
	return scsc_wlog_write_record(the_ring, wl_name, strlen(wl_name), &wl_event,
				      sizeof(wl_event), verbose_level, timestamp);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_wakelock_action);

