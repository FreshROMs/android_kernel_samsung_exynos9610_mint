/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>

#include "scsc_wifilogger_internal.h"

static struct scsc_wifi_logger *wifi_logger;

struct scsc_wifi_logger *scsc_wifilogger_get_handle(void)
{
	return wifi_logger;
}

bool scsc_wifilogger_init(void)
{
	struct scsc_wifi_logger *wl = NULL;

	wl = kzalloc(sizeof(*wl), GFP_KERNEL);
	if (!wl) {
		SCSC_TAG_ERR(WLOG,
			     "Failed to allocate scsc_wifilogger data. Abort.\n");
		return false;
	}

	mutex_init(&wl->lock);
	wl->initialized = true;
	wifi_logger = wl;

	return true;
}

static int scsc_wifilogger_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	struct scsc_wifi_logger *wl = NULL;

	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG, "WiFi Logger NOT initialized !\n");
		return NOTIFY_BAD;
	}

	switch (event) {
	case SCSC_FW_EVENT_FAILURE:
		break;
	case SCSC_FW_EVENT_MOREDUMP_COMPLETE:
	{
		char *panic_record_dump = data;

		SCSC_TAG_INFO(WLOG, "Notification received: MOREDUMP COMPLETED.\n");
		SCSC_TAG_DEBUG(WLOG, "PANIC DUMP RX\n-------------------------\n\n%s\n",
			       panic_record_dump);
		if (wl->on_alert_cb) {
			wl->on_alert_cb(panic_record_dump,
					PANIC_RECORD_DUMP_BUFFER_SZ, 0, wl->on_alert_ctx);
			SCSC_TAG_DEBUG(WLOG, "Alert handler -- processed %d bytes @%p\n",
				       PANIC_RECORD_DUMP_BUFFER_SZ, panic_record_dump);
		}
		ret = NOTIFY_OK;
		break;
	}
	default:
		ret = NOTIFY_BAD;
		break;
	}

	return ret;
}

static struct notifier_block firmware_nb = {
	.notifier_call = scsc_wifilogger_notifier,
};

bool scsc_wifilogger_fw_alert_init(void)
{
	struct scsc_wifi_logger *wl = NULL;

	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG, "WiFi Logger NOT initialized !\n");
		return false;
	}

	wl->features_mask |= WIFI_LOGGER_MEMORY_DUMP_SUPPORTED | WIFI_LOGGER_DRIVER_DUMP_SUPPORTED;
	if (!mxman_register_firmware_notifier(&firmware_nb))
		wl->features_mask |= WIFI_LOGGER_WATCHDOG_TIMER_SUPPORTED;

	return true;
}

unsigned int scsc_wifilogger_get_features(void)
{
	struct scsc_wifi_logger *wl = NULL;

	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG, "WiFi Logger NOT initialized !\n");
		return 0;
	}

	return wl->features_mask;
}

bool scsc_wifilogger_get_rings_status(u32 *num_rings,
				      struct scsc_wifi_ring_buffer_status *status)
{
	int i, j = 0;
	struct scsc_wifi_logger *wl = NULL;

	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG, "WiFi Logger NOT initialized !\n");
		*num_rings = 0;
		return false;
	}

	for (i = 0; i < *num_rings && i < MAX_WIFI_LOGGER_RINGS; i++)
		if (wl->rings[i] && wl->rings[i]->registered)
			scsc_wlog_get_ring_status(wl->rings[i], &status[j++]);
	*num_rings = j;

	return true;
}

struct scsc_wlog_ring *scsc_wifilogger_get_ring_from_name(char *name)
{
	int i;
	struct scsc_wlog_ring *r = NULL;
	struct scsc_wifi_logger *wl = NULL;

	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "WiFi Logger NOT initialized..cannot find ring !\n");
		return r;
	}

	mutex_lock(&wl->lock);
	for (i = 0; i < MAX_WIFI_LOGGER_RINGS; i++) {
		if (wl->rings[i] &&
		    !strncmp(name, wl->rings[i]->st.name, RING_NAME_SZ)) {
			if (wl->rings[i]->initialized &&
			    wl->rings[i]->registered)
				r = wl->rings[i];
			break;
		}
	}
	mutex_unlock(&wl->lock);

	return r;
}

bool scsc_wifilogger_register_ring(struct scsc_wlog_ring *r)
{
	int pos;
	struct scsc_wifi_logger *wl = NULL;

	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "WiFi Logger NOT initialized..cannot register ring !\n");
		return false;
	}
	/**
	 * Calculate ring position in array from unique ring_id:
	 * there can be multiple distinct rings supporting the same
	 * feature....like pkt_fate_tx/rx
	 */
	pos = r->st.ring_id % MAX_WIFI_LOGGER_RINGS;
	mutex_lock(&wl->lock);
	if (wl->rings[pos]) {
		SCSC_TAG_ERR(WLOG,
			     "Ring %s already registered on position %d. Abort\n",
			     wl->rings[pos]->st.name, pos);
		mutex_unlock(&wl->lock);
		return false;
	}
	SCSC_TAG_DEBUG(WLOG, "Registering ring %s as position %d\n",
		       r->st.name, pos);
	wl->rings[pos] = r;
	wl->features_mask |= r->features_mask;
	r->wl = wl;
	r->registered = true;
	mutex_unlock(&wl->lock);
	SCSC_TAG_INFO(WLOG, "Ring '%s' registered\n", r->st.name);

	return true;
}

void scsc_wifilogger_destroy(void)
{
	int i;
	struct scsc_wlog_ring *r = NULL;
	struct scsc_wifi_logger *wl = NULL;

	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "WiFi Logger NOT initialized..cannot destroy!\n");
		return;
	}

	mxman_unregister_firmware_notifier(&firmware_nb);
	/* Remove DebufgFS hooks at first... */
#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	scsc_wifilogger_debugfs_remove_top_dir_recursive();
#endif

	mutex_lock(&wl->lock);
	for (i = 0; i < MAX_WIFI_LOGGER_RINGS; i++) {
		if (wl->rings[i]) {
			r = wl->rings[i];
			scsc_wlog_ring_change_verbosity(r, WLOG_NONE);
			r->registered = false;
			wl->rings[i] = NULL;
			if (r->initialized)
				scsc_wlog_ring_destroy(r);
		}
	}
	wl->features_mask = 0;
	mutex_unlock(&wl->lock);
	kfree(wl);
	wifi_logger = NULL;
}
