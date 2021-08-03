/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef __SCSC_WIFILOGGER_RING_PKTFATE_H__
#define __SCSC_WIFILOGGER_RING_PKTFATE_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <scsc/scsc_logring.h>

#include "scsc_wifilogger_core.h"
#include "scsc_wifilogger.h"

#define	TX_FATE				0
#define	RX_FATE				1
#define WLOGGER_RFATE_TX_NAME		"pkt_fate_tx"
#define WLOGGER_RFATE_RX_NAME		"pkt_fate_rx"
#define MAX_UNITDATA_LOGGED_SZ		100

#define SCSC_DUD_ETHERNET_FRAME		0
#define SCSC_DUD_80211_FRAME		1
#define SCSC_DUD_MLME			2

bool is_pktfate_monitor_started(void);
bool scsc_wifilogger_ring_pktfate_init(void);
void scsc_wifilogger_ring_pktfate_start_monitoring(void);
void scsc_wifilogger_ring_pktfate_new_assoc(void);
void scsc_wifilogger_ring_pktfate_log_tx_frame(wifi_tx_packet_fate fate,
					       u16 htag, void *pkt,
					       size_t len, bool ma_unitdata);
void scsc_wifilogger_ring_pktfate_log_rx_frame(wifi_rx_packet_fate fate, u16 du_desc,
					       void *frame, size_t len, bool ma_unitdata);
void scsc_wifilogger_ring_pktfate_get_fates(int fate, void *report_bufs,
					    size_t n_requested_fates,
					    size_t *n_provided_fates, bool is_user);
#endif /* __SCSC_WIFILOGGER_RING_PKTFATE_H__ */
