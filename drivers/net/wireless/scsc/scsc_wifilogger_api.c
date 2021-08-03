/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#include <scsc/scsc_mx.h>
#include <scsc/scsc_logring.h>

#include "scsc_wifilogger.h"
#include "scsc_wifilogger_core.h"
#include "scsc_wifilogger_internal.h"
#include "scsc_wifilogger_ring_pktfate.h"

wifi_error scsc_wifi_get_firmware_version(char *buffer, int buffer_size)
{
	SCSC_TAG_DEBUG(WLOG, "\n");
	mxman_get_fw_version(buffer, buffer_size);
	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_firmware_version);

wifi_error scsc_wifi_get_driver_version(char *buffer, int buffer_size)
{
	SCSC_TAG_DEBUG(WLOG, "\n");
	mxman_get_driver_version(buffer, buffer_size);
	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_driver_version);

wifi_error scsc_wifi_get_ring_buffers_status(u32 *num_rings,
					     struct scsc_wifi_ring_buffer_status *status)
{
	SCSC_TAG_DEBUG(WLOG, "\n");
	if (!scsc_wifilogger_get_rings_status(num_rings, status))
		return WIFI_ERROR_UNINITIALIZED;

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_ring_buffers_status);

wifi_error scsc_wifi_get_logger_supported_feature_set(unsigned int *support)
{
	SCSC_TAG_DEBUG(WLOG, "\n");
	if (!support)
		return WIFI_ERROR_INVALID_ARGS;
	*support = scsc_wifilogger_get_features();

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_logger_supported_feature_set);

wifi_error scsc_wifi_set_log_handler(on_ring_buffer_data handler, void *ctx)
{
	struct scsc_wifi_logger *wl = NULL;

	SCSC_TAG_DEBUG(WLOG, "\n");
	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "Cannot register log_handler on UNINITIALIZED WiFi Logger.\n");
		return WIFI_ERROR_UNINITIALIZED;
	}
	if (!handler) {
		SCSC_TAG_ERR(WLOG,
			     "Cannot register NULL log_handler for WiFi Logger.\n");
		return WIFI_ERROR_INVALID_ARGS;
	}

	mutex_lock(&wl->lock);
	if (wl->on_ring_buffer_data_cb) {
		SCSC_TAG_ERR(WLOG,
			     "Log handler already registered...request ignored.\n");
		mutex_unlock(&wl->lock);
		return WIFI_SUCCESS;
	}
	wl->on_ring_buffer_data_cb = handler;
	wl->on_ring_buffer_ctx = ctx;
	mutex_unlock(&wl->lock);

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_set_log_handler);

wifi_error scsc_wifi_reset_log_handler(void)
{
	struct scsc_wifi_logger *wl = NULL;

	SCSC_TAG_DEBUG(WLOG, "\n");
	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "Cannot reset log_handler on UNINITIALIZED WiFi Logger.\n");
		return WIFI_ERROR_UNINITIALIZED;
	}
	mutex_lock(&wl->lock);
	wl->on_ring_buffer_data_cb = NULL;
	wl->on_ring_buffer_ctx = NULL;
	mutex_unlock(&wl->lock);

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_reset_log_handler);

wifi_error scsc_wifi_set_alert_handler(on_alert handler, void *ctx)
{
	struct scsc_wifi_logger *wl = NULL;

	SCSC_TAG_DEBUG(WLOG, "\n");
	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "Cannot register alert_handler on UNINITIALIZED WiFi Logger.\n");
		return WIFI_ERROR_UNINITIALIZED;
	}
	if (!handler) {
		SCSC_TAG_ERR(WLOG,
			     "Cannot register NULL alert_handler for WiFi Logger.\n");
		return WIFI_ERROR_INVALID_ARGS;
	}

	mutex_lock(&wl->lock);
	if (wl->on_alert_cb) {
		SCSC_TAG_ERR(WLOG,
			     "Alert handler already registered...request ignored.\n");
		mutex_unlock(&wl->lock);
		return WIFI_SUCCESS;
	}
	wl->on_alert_cb = handler;
	wl->on_alert_ctx = ctx;
	mutex_unlock(&wl->lock);

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_set_alert_handler);

wifi_error scsc_wifi_reset_alert_handler(void)
{
	struct scsc_wifi_logger *wl = NULL;

	SCSC_TAG_DEBUG(WLOG, "\n");
	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "Cannot reset alert_handler on UNINITIALIZED WiFi Logger.\n");
		return WIFI_ERROR_UNINITIALIZED;
	}
	mutex_lock(&wl->lock);
	wl->on_alert_cb = NULL;
	wl->on_alert_ctx = NULL;
	mutex_unlock(&wl->lock);

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_reset_alert_handler);

wifi_error scsc_wifi_get_ring_data(char *ring_name)
{
	struct scsc_wlog_ring *r;
	struct scsc_wifi_logger *wl = NULL;

	SCSC_TAG_DEBUG(WLOG, "\n");
	wl = scsc_wifilogger_get_handle();
	if (!wl || !wl->initialized) {
		SCSC_TAG_ERR(WLOG,
			     "Cannot drain ring %s on UNINITIALIZED WiFi Logger.\n",
			     ring_name);
		return WIFI_ERROR_UNINITIALIZED;
	}

	mutex_lock(&wl->lock);
	if (!wl->on_ring_buffer_data_cb)
		SCSC_TAG_WARNING(WLOG,
				 "NO log-handler registered. Discarding data while draining ring: %s\n",
				 ring_name);
	mutex_unlock(&wl->lock);

	r = scsc_wifilogger_get_ring_from_name(ring_name);
	if (!r) {
		SCSC_TAG_ERR(WLOG,
			     "Ring %s NOT found. Cannot drain.\n",
			     ring_name);
		return WIFI_ERROR_NOT_AVAILABLE;
	}

	scsc_wlog_drain_whole_ring(r);

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_ring_data);

wifi_error scsc_wifi_start_logging(u32 verbose_level, u32 flags, u32 max_interval_sec,
				   u32 min_data_size, char *ring_name)
{
	struct scsc_wlog_ring *r;

	SCSC_TAG_DEBUG(WLOG, "\n");
	r = scsc_wifilogger_get_ring_from_name(ring_name);
	if (!r) {
		SCSC_TAG_ERR(WLOG,
			     "Ring %s NOT found. Cannot start logging\n",
			     ring_name);
		return WIFI_ERROR_NOT_AVAILABLE;
	}

	return scsc_wlog_start_logging(r, verbose_level, flags,
				       max_interval_sec, min_data_size);
}
EXPORT_SYMBOL(scsc_wifi_start_logging);

wifi_error scsc_wifi_get_firmware_memory_dump(on_firmware_memory_dump handler, void *ctx)
{
	char buf[] = "Full FW memory dump NOT available.\n";

	SCSC_TAG_DEBUG(WLOG, "\n");
	handler(buf, sizeof(buf), ctx);

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_firmware_memory_dump);

wifi_error scsc_wifi_get_driver_memory_dump(on_driver_memory_dump handler, void *ctx)
{
	char buf[] = "Full DRIVER memory dump NOT available.\n";

	SCSC_TAG_DEBUG(WLOG, "\n");
	handler(buf, sizeof(buf), ctx);

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_driver_memory_dump);

wifi_error scsc_wifi_start_pkt_fate_monitoring(void)
{
	SCSC_TAG_DEBUG(WLOG, "\n");
	scsc_wifilogger_ring_pktfate_start_monitoring();

	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_start_pkt_fate_monitoring);

wifi_error scsc_wifi_get_tx_pkt_fates(wifi_tx_report *tx_report_bufs,
				      size_t n_requested_fates,
				      size_t *n_provided_fates, bool is_user)
{
	SCSC_TAG_DEBUG(WLOG, "\n");
	scsc_wifilogger_ring_pktfate_get_fates(TX_FATE, tx_report_bufs,
					       n_requested_fates, n_provided_fates, is_user);
	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_tx_pkt_fates);

wifi_error scsc_wifi_get_rx_pkt_fates(wifi_rx_report *rx_report_bufs,
				      size_t n_requested_fates,
				      size_t *n_provided_fates, bool is_user)
{
	SCSC_TAG_DEBUG(WLOG, "\n");
	scsc_wifilogger_ring_pktfate_get_fates(RX_FATE, rx_report_bufs,
					       n_requested_fates, n_provided_fates, is_user);
	return WIFI_SUCCESS;
}
EXPORT_SYMBOL(scsc_wifi_get_rx_pkt_fates);
