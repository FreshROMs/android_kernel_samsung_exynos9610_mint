/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef _SCSC_WIFILOGGER_H_
#define _SCSC_WIFILOGGER_H_
/**
 * Internal Reference docs for WiFi-Logger subsystem
 *
 *  SC-507043-SW --  Android Wi-Fi Logger architecture
 *  SC-507780-DD --  Android Enhanced Logging
 *		     WiFiLogger Core Driver Requirements and Design
 *
 * This is the CONSUMER API as implemented by scsc_wifilogger driver:
 * the framework and WiFi HAL are the final consumer of WiFi logger provided
 * data but this API is directly used by our driver NetLink layer to
 * configure and start/stop Android Enhanced Logging - WiFi Logger.
 *
 * Workflow is as follows:
 *
 *  - framework invokes wifi_logger.h exported methods implemented by WiFi HAL
 *  - WiFi HAL wifi_logger module translates wifi_logger.h requests into
 *    NetLink vendor messages dispatched to out driver
 *  - our SCSC netlink layer driver translates back NetLink received messages
 *    into invokations of methods exported by this driver into the current
 *    header file
 *  - this driver, manages all the basic ring operations, providing:
 *     + this consumer API used to configure and start/stop the data-consuming
 *       reader-process that pushes data up to the framework through the NetLink
 *       channel
 *     + a producer API that will be used to push data/record into the rings
 *     + all the machinery needed to create and manage multiple rings
 *
 * As a consequence this file's types and methods definitions closely resembles
 * the interface and types defined in:
 *
 * hardware/libhardware_legacy/include/hardware_legacy/wifi_logger.h
 * hardware/libhardware_legacy/include/hardware_legacy/wifi_hal.h
 *
 * Some function arguments, deemed un-needed in the driver layer, have been
 * removed from function prototypes, and all the names have been prefixed
 * with "scsc_".
 *
 * Types' definitions are splitted into scsc_wifilogger_types.h dedicated
 * header since they will be used also by core implementation.
 *
 */
#include "scsc_wifilogger_types.h"

/**
 * API to collect a firmware version string.
 *  - Caller is responsible to allocate / free a buffer to retrieve firmware
 *    version info.
 *  - Max string will be at most 256 bytes.
 */
wifi_error scsc_wifi_get_firmware_version(char *buffer, int buffer_size);

/**
 * API to collect a driver version string.
 *  - Caller is responsible to allocate / free a buffer to retrieve driver
 *    version info.
 *  - Max string will be at most 256 bytes.
 */
wifi_error scsc_wifi_get_driver_version(char *buffer, int buffer_size);

/**
 * API to get the status of all ring buffers supported by driver.
 *  - Caller is responsible to allocate / free ring buffer status.
 *  - Maximum no of ring buffer would be 10.
 */
wifi_error scsc_wifi_get_ring_buffers_status(u32 *num_rings,
					     struct scsc_wifi_ring_buffer_status *status);

/**
 * API to retrieve the current supportive features.
 *  - An integer variable is enough to have bit mapping info by caller.
 */
wifi_error scsc_wifi_get_logger_supported_feature_set(unsigned int *support);


/**
 * API to set/reset the log handler for getting ring data
 *  - Only a single instance of log handler can be instantiated for each
 *   ring buffer.
 */
wifi_error scsc_wifi_set_log_handler(on_ring_buffer_data handler, void *ctx);
wifi_error scsc_wifi_reset_log_handler(void);

/**
 * API to set/reset the alert handler for the alert case in Wi-Fi Chip
 *  - Only a single instance of alert handler can be instantiated.
 */
wifi_error scsc_wifi_set_alert_handler(on_alert handler, void *ctx);
wifi_error scsc_wifi_reset_alert_handler(void);

/* API for framework to indicate driver has to upload and drain all data
 * of a given ring
 */
wifi_error scsc_wifi_get_ring_data(char *ring_name);

/**
 * API to trigger the debug collection.
 *  Unless his API is invoked - logging is not triggered.
 *  - Verbose_level 0 corresponds to no collection,
 *    and it makes log handler stop by no more events from driver.
 *  - Verbose_level 1 correspond to normal log level, with minimal user impact.
 *    This is the default value.
 *  - Verbose_level 2 are enabled when user is lazily trying to reproduce
 *    a problem, wifi performances and power can be impacted but device should
 *    not otherwise be significantly impacted.
 *  - Verbose_level 3+ are used when trying to actively debug a problem.
 *
 * ring_name represent the name of the ring for which data
 *             collection shall start.
 *
 * flags: TBD parameter used to enable/disable specific events
 *               on a ring
 * max_interval: maximum interval in seconds for driver to
 *                invoke on_ring_buffer_data,
 *               ignore if zero
 * min_data_size: minimum data size in buffer for driver to
 *                  invoke on_ring_buffer_data,
 *                ignore if zero
 */
wifi_error scsc_wifi_start_logging(u32 verbose_level, u32 flags, u32 max_interval_sec,
				   u32 min_data_size, char *ring_name);

/**
 * API to collect a firmware memory dump for a given iface by async memdump event.
 *  - Triggered by Alerthandler, esp. when FW problem or FW health check happens
 *  - Caller is responsible to store fw dump data into a local,
 *      e.g., /data/misc/wifi/alertdump-1.bin
 */
wifi_error scsc_wifi_get_firmware_memory_dump(on_firmware_memory_dump handler, void *ctx);

/**
 * API to collect driver state.
 *
 *   Framework will call this API soon before or after (but not
 *   concurrently with) wifi_get_firmware_memory_dump(). Capturing
 *   firmware and driver dumps is intended to help identify
 *   inconsistent state between these components.
 *
 *   - In response to this call, HAL implementation should make one or
 *     more calls to callbacks.on_driver_memory_dump(). Framework will
 *     copy data out of the received |buffer|s, and concatenate the
 *     contents thereof.
 *   - HAL implemention will indicate completion of the driver memory
 *     dump by returning from this call.
 */
wifi_error scsc_wifi_get_driver_memory_dump(on_driver_memory_dump handler, void *ctx);

/**
 * API to start packet fate monitoring.
 *  - Once stared, monitoring should remain active until HAL is unloaded.
 *  - When HAL is unloaded, all packet fate buffers should be cleared.
 */
wifi_error scsc_wifi_start_pkt_fate_monitoring(void);

/**
 *  API to retrieve fates of outbound packets.
 *  - HAL implementation should fill |tx_report_bufs| with fates of
 *    _first_ min(n_requested_fates, actual packets) frames
 *    transmitted for the most recent association. The fate reports
 *    should follow the same order as their respective packets.
 *  - HAL implementation may choose (but is not required) to include
 *    reports for management frames.
 *  - Packets reported by firmware, but not recognized by driver,
 *    should be included.  However, the ordering of the corresponding
 *    reports is at the discretion of HAL implementation.
 *  - Framework may call this API multiple times for the same association.
 *  - Framework will ensure |n_requested_fates <= MAX_FATE_LOG_LEN|.
 *  - Framework will allocate and free the referenced storage.
 *  - is_user - to indicate if buffer passed is a user buffer
 */
wifi_error scsc_wifi_get_tx_pkt_fates(wifi_tx_report *tx_report_bufs,
				      size_t n_requested_fates,
				      size_t *n_provided_fates,
				      bool    is_user);

/**
 *  API to retrieve fates of inbound packets.
 *  - HAL implementation should fill |rx_report_bufs| with fates of
 *    _first_ min(n_requested_fates, actual packets) frames
 *    received for the most recent association. The fate reports
 *    should follow the same order as their respective packets.
 *  - HAL implementation may choose (but is not required) to include
 *    reports for management frames.
 *  - Packets reported by firmware, but not recognized by driver,
 *    should be included.  However, the ordering of the corresponding
 *    reports is at the discretion of HAL implementation.
 *  - Framework may call this API multiple times for the same association.
 *  - Framework will ensure |n_requested_fates <= MAX_FATE_LOG_LEN|.
 *  - Framework will allocate and free the referenced storage.
 *  - is_user - to indicate if buffer passed is a user buffer
 */
wifi_error scsc_wifi_get_rx_pkt_fates(wifi_rx_report *rx_report_bufs,
				      size_t n_requested_fates,
				      size_t *n_provided_fates,
				      bool    is_user);

#endif /*_SCSC_WIFILOGGER_H_*/
