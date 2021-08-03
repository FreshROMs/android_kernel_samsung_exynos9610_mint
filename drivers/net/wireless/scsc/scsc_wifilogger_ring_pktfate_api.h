/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef __SCSC_WIFILOGGER_RING_PKTFATE_API_H__
#define __SCSC_WIFILOGGER_RING_PKTFATE_API_H__

/** Android Enhanced Logging
 *
 *  PKTFATE RING -- Public Producer API
 *
 */

#if IS_ENABLED(CONFIG_SCSC_WIFILOGGER)
#include "scsc_wifilogger_ring_pktfate.h"

#define SCSC_WLOG_PKTFATE_NEW_ASSOC()	\
	do { \
		if (is_pktfate_monitor_started()) \
			scsc_wifilogger_ring_pktfate_new_assoc(); \
	} while (0)

#define SCSC_WLOG_PKTFATE_LOG_TX_DATA_FRAME(htag, frame, flen) \
	do { \
		if (is_pktfate_monitor_started()) \
			scsc_wifilogger_ring_pktfate_log_tx_frame(TX_PKT_FATE_DRV_QUEUED, (htag), \
								  (void *)(frame), (flen), true); \
	} while (0)

#define SCSC_WLOG_PKTFATE_LOG_RX_DATA_FRAME(du_desc, frame, flen) \
	do { \
		if (is_pktfate_monitor_started() && \
		    ((du_desc) == SCSC_DUD_ETHERNET_FRAME || (du_desc) == SCSC_DUD_80211_FRAME)) \
			scsc_wifilogger_ring_pktfate_log_rx_frame(RX_PKT_FATE_DRV_QUEUED, (du_desc), \
								  (void *)(frame), (flen), true); \
	} while (0)

#define SCSC_WLOG_PKTFATE_LOG_TX_CTRL_FRAME(htag, frame, flen) \
	do { \
		if (is_pktfate_monitor_started()) \
			scsc_wifilogger_ring_pktfate_log_tx_frame(TX_PKT_FATE_DRV_QUEUED, (htag), \
								  (void *)(frame), (flen), false); \
	} while (0)

#define SCSC_WLOG_PKTFATE_LOG_RX_CTRL_FRAME(frame, flen) \
	do { \
		if (is_pktfate_monitor_started()) \
			scsc_wifilogger_ring_pktfate_log_rx_frame(RX_PKT_FATE_DRV_QUEUED, SCSC_DUD_MLME, \
								  (void *)(frame), (flen), false); \
	} while (0)

#else

#define SCSC_WLOG_PKTFATE_NEW_ASSOC()						do {} while (0)
#define SCSC_WLOG_PKTFATE_LOG_TX_DATA_FRAME(htag, skb_mac, skb_hlen)		do {} while (0)
#define SCSC_WLOG_PKTFATE_LOG_TX_CTRL_FRAME(htag, skb_mac, skb_hlen)		do {} while (0)
#define	SCSC_WLOG_PKTFATE_LOG_RX_DATA_FRAME(du_desc, skb_mac, skb_hlen)		do {} while (0)
#define SCSC_WLOG_PKTFATE_LOG_RX_CTRL_FRAME(frame, flen)			do {} while (0)

#endif /* CONFIG_SCSC_WIFILOGGER */

#endif /* __SCSC_WIFILOGGER_RING_PKTFATE_API_H__ */
