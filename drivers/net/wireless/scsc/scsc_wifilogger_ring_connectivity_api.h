/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef __SCSC_WIFILOGGER_RING_CONNECTIVITY_API_H__
#define __SCSC_WIFILOGGER_RING_CONNECTIVITY_API_H__
/** Android Enhanced Logging
 *
 *  CONNECTIVITY RING -- Public Producer API
 *
 *  This ring collects a number of events originated by FW and Driver; given the different
 *  format of the provided payload between FW and Driver the API is splitted in two classes:
 *  once to be invoked on fw events and the other on driver events.
 */

#if IS_ENABLED(CONFIG_SCSC_WIFILOGGER)

#include "scsc_wifilogger_ring_connectivity.h"
/**
 *  DRIVER-Produced Connectivity Events
 *
 *  @lev: chosen verbosity level
 *  @driver_event_id: id of the event reported by the driver
 *  @tag_count: number of TLV-TRIPLETS constituting the variadic portion of the call.
 *		Provided TLV triplets are composed as follows:
 *              - Types(Tag) are defined in scsc_wifilogger_types.h
 *              - Length in bytes of value
 *              - Value is a POINTER to the area holding the Length bytes to copy
 *
 * This function will take care to build the needed inner record-header and push the
 * log material to the related ring, adding also a proper driver-built timestamp.
 *
 * An example invokation on Association Request received at driver:
 *
 *  SCSC_WLOG_DRIVER_EVENT(WLOG_NORMAL, WIFI_EVENT_ASSOCIATION_REQUESTED, 3,
 *			   WIFI_TAG_BSSID, ETH_ALEN, sme->bssid,
 *			   WIFI_TAG_SSID, sme->ssid_len, sme->ssid,
 *			   WIFI_TAG_CHANNEL, sizeof(u16), &sme->channel->center_freq);
 *
 *  BE AWARE THAT tag_count parameeters expects the NUMBER of TRIPLETS
 *  NOT the number of variadic params.
 */
#define SCSC_WLOG_DRIVER_EVENT(lev, driver_event_id, tag_count, tag_args...) \
	do { \
		if (cring_lev && (lev) <= cring_lev) \
			scsc_wifilogger_ring_connectivity_driver_event((lev), (driver_event_id), \
								       (tag_count), tag_args); \
	} while (0)

/**
 *  FW-Produced Connectivity Events
 *
 *  @lev: chosen verbosity level
 *  @fw_event_id: id of the event as provided in the field
 *		  MLME-EVENT-LOG.indication[Event]
 *  @fw_timestamp: timestamp of the event as provided in the field
 *		   MLME-EVENT-LOG.indication[TSF Time]
 *  @fw_bulk_data: the bulk data contained in the MLME signal.
 *		   "The bulk data shall contain TLV encoded parameters for that event"
 *  @fw_blen: the length of the above bulk_data
 *
 * This function will take care to build the needed inner record-header and push the
 * log material to the related ring.
 */
#define	SCSC_WLOG_FW_EVENT(lev, fw_event_id, fw_timestamp, fw_bulk_data, fw_blen) \
	do { \
		if (cring_lev && (lev) <= cring_lev) \
			scsc_wifilogger_ring_connectivity_fw_event((lev), (fw_event_id), (fw_timestamp), \
								   (fw_bulk_data), (fw_blen)); \
	} while (0)
#else

#define	SCSC_WLOG_DRIVER_EVENT(lev, driver_event_id, tag_count, tag_args...)		do {} while (0)
#define	SCSC_WLOG_FW_EVENT(lev, fw_event_id, fw_timestamp, fw_bulk_data, fw_blen)	do {} while (0)

#endif /* CONFIG_SCSC_WIFILOGGER */

#endif /* __SCSC_WIFILOGGER_RING_CONNECTIVITY_API_H__ */
