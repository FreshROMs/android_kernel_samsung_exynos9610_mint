/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 *****************************************************************************/
/* Implements */
#include "scsc_wifilogger_ring_pktfate.h"

/* Uses */
#include <stdarg.h>
#include "scsc_wifilogger_internal.h"

static bool pktfate_monitor_started;

static wifi_tx_report txr;
static wifi_rx_report rxr;
static struct scsc_wlog_ring *fate_ring_tx;
static struct scsc_wlog_ring *fate_ring_rx;

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
#include "scsc_wifilogger_debugfs.h"
#include "scsc_wifilogger.h"

static struct scsc_wlog_debugfs_info di_tx, di_rx;

#ifdef CONFIG_SCSC_WIFILOGGER_TEST
#include <linux/uaccess.h>

static ssize_t dfs_read_fates(struct file *filp, char __user *ubuf,
			      size_t count, loff_t *f_pos)
{
	size_t got_sz, n_provided_fates = 0, n_requested_fates;
	struct scsc_ring_test_object *rto;
	wifi_tx_report *tx_report_bufs = NULL;
	wifi_rx_report *rx_report_bufs = NULL;
	void *srcbuf = NULL;

	if (!filp->private_data)
		return -EINVAL;
	rto = filp->private_data;

	if (!strncmp(rto->r->st.name, WLOGGER_RFATE_TX_NAME, RING_NAME_SZ - 1)) {
		n_requested_fates = count / sizeof(wifi_tx_report);
		tx_report_bufs = vmalloc(sizeof(wifi_tx_report) * n_requested_fates);
		if (tx_report_bufs)
			scsc_wifi_get_tx_pkt_fates(tx_report_bufs, n_requested_fates,
						   &n_provided_fates, false);
		got_sz = sizeof(wifi_tx_report) * n_provided_fates;
		srcbuf =  tx_report_bufs;
	} else {
		n_requested_fates = count / sizeof(wifi_rx_report);
		rx_report_bufs = vmalloc(sizeof(wifi_rx_report) * n_requested_fates);
		if (rx_report_bufs)
		scsc_wifi_get_rx_pkt_fates(rx_report_bufs, n_requested_fates,
					   &n_provided_fates, false);
		got_sz = sizeof(wifi_rx_report) * n_provided_fates;
		srcbuf = rx_report_bufs;
	}
	SCSC_TAG_DEBUG(WLOG, "Ring '%s'...asked for %d fates....GOT %d\n",
		       rto->r->st.name, n_requested_fates, n_provided_fates);
	if (copy_to_user(ubuf, srcbuf, got_sz))
		return -EFAULT;
	*f_pos += got_sz;

	return got_sz;
}

const struct file_operations get_fates_fops = {
	.owner = THIS_MODULE,
	.open = dfs_open,
	.read = dfs_read_fates,
	.release = dfs_release,
};
#endif /* CONFIG_SCSC_WIFILOGGER_TEST */

#endif /* CONFIG_SCSC_WIFILOGGER_DEBUGFS */

bool is_pktfate_monitor_started(void)
{
	return pktfate_monitor_started;
}
EXPORT_SYMBOL(is_pktfate_monitor_started);

bool scsc_wifilogger_ring_pktfate_init(void)
{
	struct scsc_wlog_ring *r = NULL;
#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	struct scsc_ring_test_object *rto_tx, *rto_rx;
#endif

	r = scsc_wlog_ring_create(WLOGGER_RFATE_TX_NAME,
				  RING_BUFFER_ENTRY_FLAGS_HAS_BINARY,
				  ENTRY_TYPE_PKT, 32768 * 4,
				  WIFI_LOGGER_PACKET_FATE_SUPPORTED,
				  NULL, NULL, NULL);
	if (!r) {
		SCSC_TAG_ERR(WLOG, "Failed to CREATE WiFiLogger ring: %s\n",
			     WLOGGER_RFATE_TX_NAME);
		return false;
	}
	fate_ring_tx = r;

	r = scsc_wlog_ring_create(WLOGGER_RFATE_RX_NAME,
				  RING_BUFFER_ENTRY_FLAGS_HAS_BINARY,
				  ENTRY_TYPE_PKT, 32768 * 4,
				  WIFI_LOGGER_PACKET_FATE_SUPPORTED,
				  NULL, NULL, NULL);
	if (!r) {
		SCSC_TAG_ERR(WLOG, "Failed to CREATE WiFiLogger ring: %s\n",
			     WLOGGER_RFATE_RX_NAME);
		scsc_wlog_ring_destroy(fate_ring_tx);
		return false;
	}
	fate_ring_rx = r;

	if (!scsc_wifilogger_register_ring(fate_ring_tx)) {
		SCSC_TAG_ERR(WLOG, "Failed to REGISTER WiFiLogger ring: %s\n",
			     fate_ring_tx->st.name);
		scsc_wlog_ring_destroy(fate_ring_tx);
		scsc_wlog_ring_destroy(fate_ring_rx);
		return false;
	}
	if (!scsc_wifilogger_register_ring(fate_ring_rx)) {
		SCSC_TAG_ERR(WLOG, "Failed to REGISTER WiFiLogger ring: %s\n",
			     fate_ring_rx->st.name);
		scsc_wlog_ring_destroy(fate_ring_tx);
		scsc_wlog_ring_destroy(fate_ring_rx);
		return false;
	}

	// Just in case framework invokes with min_data_size != 0
	scsc_wlog_ring_set_drop_on_full(fate_ring_tx);
	scsc_wlog_ring_set_drop_on_full(fate_ring_rx);

#ifdef CONFIG_SCSC_WIFILOGGER_DEBUGFS
	/* The test object is shared between all the debugfs entries
	 * belonging to the same ring.
	 */
	rto_tx = init_ring_test_object(fate_ring_tx);
	if (rto_tx) {
		scsc_register_common_debugfs_entries(fate_ring_tx->st.name, rto_tx, &di_tx);
#ifdef CONFIG_SCSC_WIFILOGGER_TEST
		scsc_wlog_register_debugfs_entry(fate_ring_tx->st.name, "get_fates",
						 &get_fates_fops, rto_tx, &di_tx);
#endif
	}

	rto_rx = init_ring_test_object(fate_ring_rx);
	if (rto_rx) {
		scsc_register_common_debugfs_entries(fate_ring_rx->st.name, rto_rx, &di_rx);
#ifdef CONFIG_SCSC_WIFILOGGER_TEST
		scsc_wlog_register_debugfs_entry(fate_ring_rx->st.name, "get_fates",
						 &get_fates_fops, rto_rx, &di_rx);
#endif
	}
#endif

	return true;
}

void scsc_wifilogger_ring_pktfate_start_monitoring(void)
{
	if (!fate_ring_rx || !fate_ring_tx)
		return;

	/* Just in case */
	scsc_wlog_flush_ring(fate_ring_tx);
	scsc_wlog_start_logging(fate_ring_tx, WLOG_DEBUG, 0, 0, 0);
	scsc_wlog_flush_ring(fate_ring_rx);
	scsc_wlog_start_logging(fate_ring_rx, WLOG_DEBUG, 0, 0, 0);
	pktfate_monitor_started = true;

	SCSC_TAG_INFO(WLOG, "PacketFate monitor started.\n");
}

/**** Producer API ******/
void scsc_wifilogger_ring_pktfate_new_assoc(void)
{
	SCSC_TAG_INFO(WLOG, "New Association started...flushing PacketFate rings.\n");
	scsc_wlog_flush_ring(fate_ring_tx);
	scsc_wlog_flush_ring(fate_ring_rx);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_pktfate_new_assoc);

void scsc_wifilogger_ring_pktfate_get_fates(int fate, void *report_bufs,
					    size_t n_requested_fates,
					    size_t *n_provided_fates,
					    bool is_user)
{
	struct scsc_wlog_ring *r;
	u32 n_req_fates = (u32)n_requested_fates;
	size_t blen;

	r = (fate == TX_FATE) ? fate_ring_tx : fate_ring_rx;
	if (fate == TX_FATE)
		blen = sizeof(wifi_tx_report) * n_req_fates;
	else
		blen = sizeof(wifi_rx_report) * n_req_fates;
	scsc_wlog_read_max_records(r, report_bufs, blen, &n_req_fates, is_user);
	*n_provided_fates = n_req_fates;

	SCSC_TAG_INFO(WLOG, "[%s]:: GET %s pkt_fates -- Requested:%zd  -  Got:%zd\n",
		      r->st.name,
		      (fate == TX_FATE) ? "TX" : "RX", n_requested_fates, *n_provided_fates);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_pktfate_get_fates);

/**
 * Here we're Just saving the egressing ETH frame for now with the
 * provided initial fate (which is fixed to TX_PKT_FATE_DRV_QUEUED).
 * In a full pktfate implementation as required by WiFi-Logger we
 * should track down this eth frame using the host_tag and then
 * account for the final fate of the frame looking at Debugging
 * Information Element provided in subsequent UnidataTx.confirm, BUT
 * such confirm as of now does NOT provide any Debugging Element NOR
 * any additional interesting information related to the packet fates
 * defined.
 */
void scsc_wifilogger_ring_pktfate_log_tx_frame(wifi_tx_packet_fate fate,
					       u16 htag, void *frame,
					       size_t len, bool ma_unitdata)
{
	if (len > MAX_FRAME_LEN_ETHERNET) {
		SCSC_TAG_WARNING(WLOG, "pktfate TX:: dropped unplausible length frame.\n");
		return;
	}

	if (ma_unitdata)
		len = len <= MAX_UNITDATA_LOGGED_SZ ? len : MAX_UNITDATA_LOGGED_SZ;

	txr.fate = fate;
	txr.frame_inf.payload_type = FRAME_TYPE_ETHERNET_II;
	txr.frame_inf.frame_len = len;
	txr.frame_inf.driver_timestamp_usec = ktime_to_ns(ktime_get_boottime());
	txr.frame_inf.firmware_timestamp_usec = 0;
	memcpy(&txr.frame_inf.frame_content, frame, len);
	//TODO MD5 checksum using Kernel Crypto API
	memset(&txr.md5_prefix, 0x00, MD5_PREFIX_LEN);
	/**
	 * We have to waste a lot of space storing the frame in a full-sized
	 * frame_content array, even if the frame size is much smaller, because
	 * the wifi-logger API (get_tx/rx_pkt_fates) reports multiple of struct
	 * wifi_tx/rx_packet_fates and does NOT return the effectively read bytes.
	 * The real size (that we cannot use) being:
	 *
	 *	real_txr_sz = sizeof(txr) - sizeof(txr.frame_inf.frame_content) + len;
	 *
	 * frame_len field is anyway provided to recognize the actual end of frame.
	 */
	scsc_wlog_write_record(fate_ring_tx, NULL, 0, &txr, sizeof(txr), WLOG_DEBUG, 0);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_pktfate_log_tx_frame);

void scsc_wifilogger_ring_pktfate_log_rx_frame(wifi_rx_packet_fate fate, u16 du_desc,
					       void *frame, size_t len,  bool ma_unitdata)
{
	if ((du_desc == SCSC_DUD_ETHERNET_FRAME && len > MAX_FRAME_LEN_ETHERNET) ||
	    (du_desc == SCSC_DUD_80211_FRAME && len > MAX_FRAME_LEN_80211_MGMT)) {
		SCSC_TAG_WARNING(WLOG, "pktfate RX:: dropped unplausible length frame.\n");
		return;
	}

	if (ma_unitdata)
		len = len <= MAX_UNITDATA_LOGGED_SZ ? len : MAX_UNITDATA_LOGGED_SZ;

	rxr.fate = fate;
	rxr.frame_inf.payload_type = du_desc == SCSC_DUD_ETHERNET_FRAME ? FRAME_TYPE_ETHERNET_II :
				    (du_desc == SCSC_DUD_80211_FRAME ? FRAME_TYPE_80211_MGMT : FRAME_TYPE_UNKNOWN);
	rxr.frame_inf.frame_len = len;
	rxr.frame_inf.driver_timestamp_usec = ktime_to_ns(ktime_get_boottime());
	rxr.frame_inf.firmware_timestamp_usec = 0;
	memcpy(&rxr.frame_inf.frame_content, frame, len);
	//TODO MD5 checksum using Kernel Crypto API
	memset(&rxr.md5_prefix, 0x00, MD5_PREFIX_LEN);
	/**
	 * We have to waste a lot of space storing the frame in a full-sized
	 * frame_content array, even if the frame size is much smaller, because
	 * the wifi-logger API (get_tx/rx_pkt_fates) reports multiple of struct
	 * wifi_tx/rx_packet_fates and does NOT return the effectively read bytes.
	 * The real size (that we cannot use) being:
	 *
	 *	real_txr_sz = sizeof(txr) - sizeof(txr.frame_inf.frame_content) + len;
	 *
	 * frame_len field is anyway provided to recognize the actual end of frame.
	 */
	scsc_wlog_write_record(fate_ring_rx, NULL, 0, &rxr, sizeof(rxr), WLOG_DEBUG, 0);
}
EXPORT_SYMBOL(scsc_wifilogger_ring_pktfate_log_rx_frame);
