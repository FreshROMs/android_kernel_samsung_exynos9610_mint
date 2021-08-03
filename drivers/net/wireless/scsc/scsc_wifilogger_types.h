/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef _SCSC_WIFILOGGER_TYPES_H_
#define _SCSC_WIFILOGGER_TYPES_H_
/**
 * These types are derived from definitions in:
 *
 *  hardware/libhardware_legacy/include/hardware_legacy/wifi_logger.h
 *  hardware/libhardware_legacy/include/hardware_legacy/wifi_hal.h
 *
 * Descriptive comments are in wifi_logger.h original file.
 * Here we avoided using typedef that are in contrast with Kernel
 * coding style though.
 */
#include <linux/types.h>

#define LOGGER_MAJOR_VERSION    1
#define LOGGER_MINOR_VERSION    0
#define LOGGER_MICRO_VERSION    0

/**
 * Be aware that there can be multiple distinct rings, registered
 * with different names but supporting the same feature: an example
 * being pkt_fate_tx and pkt_fate_rx. Rings are registered by ring_id.
 */
#ifndef CONFIG_SCSC_WIFILOGGER_TEST_RING
#define MAX_WIFI_LOGGER_RINGS	10
#else
#define MAX_WIFI_LOGGER_RINGS	11
#endif

typedef enum {
	WIFI_SUCCESS = 0,
	WIFI_ERROR_NONE = 0,
	WIFI_ERROR_UNKNOWN = -1,
	WIFI_ERROR_UNINITIALIZED = -2,
	WIFI_ERROR_NOT_SUPPORTED = -3,
	WIFI_ERROR_NOT_AVAILABLE = -4,	/* Not available right now, but try later */
	WIFI_ERROR_INVALID_ARGS = -5,
	WIFI_ERROR_INVALID_REQUEST_ID = -6,
	WIFI_ERROR_TIMED_OUT = -7,
	WIFI_ERROR_TOO_MANY_REQUESTS = -8,	/* Too many instances of this request */
	WIFI_ERROR_OUT_OF_MEMORY = -9,
	WIFI_ERROR_BUSY = -10,
} wifi_error;

/* Verbosity */
typedef enum {
	WLOG_NONE = 0,
	WLOG_NORMAL = 1,
	WLOG_LAZY = 2,
	WLOG_DEBUG = 3,
} wlog_verbose_level;

/* Feature set */
enum {
	WIFI_LOGGER_MEMORY_DUMP_SUPPORTED = (1 << (0)),
	WIFI_LOGGER_PER_PACKET_TX_RX_STATUS_SUPPORTED = (1 << (1)),
	WIFI_LOGGER_CONNECT_EVENT_SUPPORTED = (1 << (2)),
	WIFI_LOGGER_POWER_EVENT_SUPPORTED = (1 << (3)),
	WIFI_LOGGER_WAKE_LOCK_SUPPORTED = (1 << (4)),
	WIFI_LOGGER_VERBOSE_SUPPORTED = (1 << (5)),
	WIFI_LOGGER_WATCHDOG_TIMER_SUPPORTED = (1 << (6)),
	WIFI_LOGGER_DRIVER_DUMP_SUPPORTED = (1 << (7)),
	WIFI_LOGGER_PACKET_FATE_SUPPORTED = (1 << (8)),
#ifdef CONFIG_SCSC_WIFILOGGER_TEST
	WIFI_LOGGER_SCSC_TEST_RING_SUPPORTED = (1 << (9)),
#endif
};

enum {
	RING_BUFFER_ENTRY_FLAGS_HAS_BINARY = (1 << (0)),
	RING_BUFFER_ENTRY_FLAGS_HAS_TIMESTAMP = (1 << (1))
};

#define	RING_NAME_SZ	32

struct scsc_wifi_ring_buffer_status {
	u8 name[RING_NAME_SZ];
	u32 flags;
	int ring_id;		/* unique integer representing the ring */
	u32 rb_byte_size;	/* total memory size allocated for the buffer */
	u32 verbose_level;	/* verbose level for ring buffer */
	u32 written_bytes;	/* number of bytes that was written to the
				 * buffer by driver, monotonously increasing
				 * integer
				 */
	u32 read_bytes;		/* number of bytes that was read from the buffer
				 * by user land, monotonously increasing integer
				 */
	u32 written_records;	/* number of records that was written to the
				 * buffer by driver, monotonously increasing
				 * integer
				 */
};

typedef void (*on_ring_buffer_data)(char *ring_name, char *buffer, int buffer_size,
				    struct scsc_wifi_ring_buffer_status *status, void *ctx);
typedef void (*on_alert)(char *buffer, int buffer_size, int err_code, void *ctx);

enum {
	ENTRY_TYPE_CONNECT_EVENT = 1,
	ENTRY_TYPE_PKT,
	ENTRY_TYPE_WAKE_LOCK,
	ENTRY_TYPE_POWER_EVENT,
	ENTRY_TYPE_DATA
};

struct scsc_wifi_ring_buffer_entry {
	u16 entry_size; /* the size of payload excluding the header. */
	u8 flags;
	u8 type;
	u64 timestamp;  /* present if has_timestamp bit is set. */
} __packed;

/* set if binary entries are present */
#define WIFI_RING_BUFFER_FLAG_HAS_BINARY_ENTRIES 0x00000001
/* set if ascii entries are present */
#define WIFI_RING_BUFFER_FLAG_HAS_ASCII_ENTRIES  0x00000002

/* Below events refer to the wifi_connectivity_event ring and shall be supported */
#define WIFI_EVENT_ASSOCIATION_REQUESTED    0  // driver receives association command from kernel
#define WIFI_EVENT_AUTH_COMPLETE            1
#define WIFI_EVENT_ASSOC_COMPLETE           2
#define WIFI_EVENT_FW_AUTH_STARTED          3  // fw event indicating auth frames are sent
#define WIFI_EVENT_FW_ASSOC_STARTED         4  // fw event indicating assoc frames are sent
#define WIFI_EVENT_FW_RE_ASSOC_STARTED      5  // fw event indicating reassoc frames are sent
#define WIFI_EVENT_DRIVER_SCAN_REQUESTED    6
#define WIFI_EVENT_DRIVER_SCAN_RESULT_FOUND 7
#define WIFI_EVENT_DRIVER_SCAN_COMPLETE     8
#define WIFI_EVENT_G_SCAN_STARTED           9
#define WIFI_EVENT_G_SCAN_COMPLETE          10
#define WIFI_EVENT_DISASSOCIATION_REQUESTED 11
#define WIFI_EVENT_RE_ASSOCIATION_REQUESTED 12
#define WIFI_EVENT_ROAM_REQUESTED           13
#define WIFI_EVENT_BEACON_RECEIVED          14  // received beacon from AP (event enabled
						// only in verbose mode)
#define WIFI_EVENT_ROAM_SCAN_STARTED        15  // firmware has triggered a roam scan (not g-scan)
#define WIFI_EVENT_ROAM_SCAN_COMPLETE       16  // firmware has completed a roam scan (not g-scan)
#define WIFI_EVENT_ROAM_SEARCH_STARTED      17  // firmware has started searching for roam
						// candidates (with reason =xx)
#define WIFI_EVENT_ROAM_SEARCH_STOPPED      18  // firmware has stopped searching for roam
// candidates (with reason =xx)
#define WIFI_EVENT_CHANNEL_SWITCH_ANOUNCEMENT     20 // received channel switch anouncement from AP
#define WIFI_EVENT_FW_EAPOL_FRAME_TRANSMIT_START  21 // fw start transmit eapol frame, with
						     // EAPOL index 1-4
#define WIFI_EVENT_FW_EAPOL_FRAME_TRANSMIT_STOP   22 // fw gives up eapol frame, with rate,
						     // success/failure and number retries
#define WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED 23 // kernel queue EAPOL for transmission
							    // in driver with EAPOL index 1-4
#define WIFI_EVENT_FW_EAPOL_FRAME_RECEIVED        24 // with rate, regardless of the fact that
						     // EAPOL frame is accepted or rejected by fw
#define WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED    26 // with rate, and eapol index, driver has
						     // received EAPOL frame and will queue it up
						     // to wpa_supplicant
#define WIFI_EVENT_BLOCK_ACK_NEGOTIATION_COMPLETE 27 // with success/failure, parameters
#define WIFI_EVENT_BT_COEX_BT_SCO_START     28
#define WIFI_EVENT_BT_COEX_BT_SCO_STOP      29
#define WIFI_EVENT_BT_COEX_BT_SCAN_START    30  // for paging/scan etc., when BT starts transmiting
						// twice per BT slot
#define WIFI_EVENT_BT_COEX_BT_SCAN_STOP     31
#define WIFI_EVENT_BT_COEX_BT_HID_START     32
#define WIFI_EVENT_BT_COEX_BT_HID_STOP      33
#define WIFI_EVENT_ROAM_AUTH_STARTED        34  // fw sends auth frame in roaming to next candidate
#define WIFI_EVENT_ROAM_AUTH_COMPLETE       35  // fw receive auth confirm from ap
#define WIFI_EVENT_ROAM_ASSOC_STARTED       36  // firmware sends assoc/reassoc frame in
						// roaming to next candidate
#define WIFI_EVENT_ROAM_ASSOC_COMPLETE      37  // firmware receive assoc/reassoc confirm from ap
#define WIFI_EVENT_G_SCAN_STOP              38  // firmware sends stop G_SCAN
#define WIFI_EVENT_G_SCAN_CYCLE_STARTED     39  // firmware indicates G_SCAN scan cycle started
#define WIFI_EVENT_G_SCAN_CYCLE_COMPLETED   40  // firmware indicates G_SCAN scan cycle completed
#define WIFI_EVENT_G_SCAN_BUCKET_STARTED    41  // firmware indicates G_SCAN scan start
						// for a particular bucket
#define WIFI_EVENT_G_SCAN_BUCKET_COMPLETED  42  // firmware indicates G_SCAN scan completed for
						// for a particular bucket
#define WIFI_EVENT_G_SCAN_RESULTS_AVAILABLE 43  // Event received from firmware about G_SCAN scan
// results being available
#define WIFI_EVENT_G_SCAN_CAPABILITIES      44  // Event received from firmware with G_SCAN
						// capabilities
#define WIFI_EVENT_ROAM_CANDIDATE_FOUND     45  // Event received from firmware when eligible
						// candidate is found
#define WIFI_EVENT_ROAM_SCAN_CONFIG         46  // Event received from firmware when roam scan
						// configuration gets enabled or disabled
#define WIFI_EVENT_AUTH_TIMEOUT             47  // firmware/driver timed out authentication
#define WIFI_EVENT_ASSOC_TIMEOUT            48  // firmware/driver timed out association
#define WIFI_EVENT_MEM_ALLOC_FAILURE        49  // firmware/driver encountered allocation failure
#define WIFI_EVENT_DRIVER_PNO_ADD           50  // driver added a PNO network in firmware
#define WIFI_EVENT_DRIVER_PNO_REMOVE        51  // driver removed a PNO network in firmware
#define WIFI_EVENT_DRIVER_PNO_NETWORK_FOUND 52  // driver received PNO networks
						// found indication from firmware
#define WIFI_EVENT_DRIVER_PNO_SCAN_REQUESTED 53  // driver triggered a scan for PNO networks
#define WIFI_EVENT_DRIVER_PNO_SCAN_RESULT_FOUND 54  // driver received scan results
						    // of PNO networks
#define WIFI_EVENT_DRIVER_PNO_SCAN_COMPLETE 55  // driver updated scan results from
						// PNO networks to cfg80211

/**
 * Parameters of wifi logger events are TLVs
 * Event parameters tags are defined as:
 */
#define WIFI_TAG_VENDOR_SPECIFIC    0   // take a byte stream as parameter
#define WIFI_TAG_BSSID              1   // takes a 6 bytes MAC address as parameter
#define WIFI_TAG_ADDR               2   // takes a 6 bytes MAC address as parameter
#define WIFI_TAG_SSID               3   // takes a 32 bytes SSID address as parameter
#define WIFI_TAG_STATUS             4   // takes an integer as parameter
#define WIFI_TAG_CHANNEL_SPEC       5   // takes one or more wifi_channel_spec as parameter
#define WIFI_TAG_WAKE_LOCK_EVENT    6   // takes a wake_lock_event struct as parameter
#define WIFI_TAG_ADDR1              7   // takes a 6 bytes MAC address as parameter
#define WIFI_TAG_ADDR2              8   // takes a 6 bytes MAC address as parameter
#define WIFI_TAG_ADDR3              9   // takes a 6 bytes MAC address as parameter
#define WIFI_TAG_ADDR4              10  // takes a 6 bytes MAC address as parameter
#define WIFI_TAG_TSF                11  // take a 64 bits TSF value as parameter
#define WIFI_TAG_IE                 12  // take one or more specific 802.11 IEs parameter,
					// IEs are in turn indicated in TLV format as per
					// 802.11 spec
#define WIFI_TAG_INTERFACE          13  // take interface name as parameter
#define WIFI_TAG_REASON_CODE        14  // take a reason code as per 802.11 as parameter
#define WIFI_TAG_RATE_MBPS          15  // take a wifi rate in 0.5 mbps
#define WIFI_TAG_REQUEST_ID         16  // take an integer as parameter
#define WIFI_TAG_BUCKET_ID          17  // take an integer as parameter
#define WIFI_TAG_GSCAN_PARAMS       18  // takes a wifi_scan_cmd_params struct as parameter
#define WIFI_TAG_GSCAN_CAPABILITIES 19  // takes a wifi_gscan_capabilities struct as parameter
#define WIFI_TAG_SCAN_ID            20  // take an integer as parameter
#define WIFI_TAG_RSSI               21  // take an integer as parameter
#define WIFI_TAG_CHANNEL            22  // take an integer as parameter
#define WIFI_TAG_LINK_ID            23  // take an integer as parameter
#define WIFI_TAG_LINK_ROLE          24  // take an integer as parameter
#define WIFI_TAG_LINK_STATE         25  // take an integer as parameter
#define WIFI_TAG_LINK_TYPE          26  // take an integer as parameter
#define WIFI_TAG_TSCO               27  // take an integer as parameter
#define WIFI_TAG_RSCO               28  // take an integer as parameter
#define WIFI_TAG_EAPOL_MESSAGE_TYPE 29  // take an integer as parameter
					// M1-1, M2-2, M3-3, M4-4

struct scsc_tlv_log {
	u16 tag;
	u16 length; // length of value
	u8 value[0];
} __packed;

struct scsc_wifi_ring_buffer_driver_connectivity_event {
	u16 event;
	struct scsc_tlv_log tlvs[0];
} __packed;

/**
 * Ring buffer name for power events ring. note that power event are extremely frequents
 * and thus should be stored in their own ring/file so as not to clobber connectivity events.
 */
struct scsc_wake_lock_event {
	int status;      // 0 taken, 1 released
	int reason;      // reason why this wake lock is taken
	char name[0];    // null terminated
} __packed;

struct scsc_wifi_power_event {
	u16 event;
	struct scsc_tlv_log tlvs[0];
} __packed;

#define PER_PACKET_ENTRY_FLAGS_DIRECTION_TX  1	/* 0: TX, 1: RX */
#define PER_PACKET_ENTRY_FLAGS_TX_SUCCESS    2  /* pkt TX or RX/decrypt successfully */
#define PER_PACKET_ENTRY_FLAGS_80211_HEADER  4  /* full 802.11 header or 802.3 header */
#define PER_PACKET_ENTRY_FLAGS_PROTECTED     8  /* whether packet was encrypted */

struct scsc_wifi_ring_per_packet_status_entry {
	u8 flags;
	u8 tid;
	u16 MCS;
	u8 rssi;
	u8 num_retries;
	u16 last_transmit_rate;
	u16 link_layer_transmit_sequence;
	u64 firmware_entry_timestamp;
	u64 start_contention_timestamp;
	u64 transmit_success_timestamp;
	u8 data[0];
} __packed;

typedef void (*on_driver_memory_dump)(char *buffer, int buffer_size, void *ctx);
typedef void (*on_firmware_memory_dump)(char *buffer, int buffer_size, void *ctx);

/* packet fate logs */

#define MD5_PREFIX_LEN             4
#define MAX_FATE_LOG_LEN           32
#define MAX_FRAME_LEN_ETHERNET     1518
#define MAX_FRAME_LEN_80211_MGMT   2352  // 802.11-2012 Fig. 8-34

typedef enum {
	// Sent over air and ACKed.
	TX_PKT_FATE_ACKED,

	// Sent over air but not ACKed. (Normal for broadcast/multicast.)
	TX_PKT_FATE_SENT,

	// Queued within firmware, but not yet sent over air.
	TX_PKT_FATE_FW_QUEUED,

	// Dropped by firmware as invalid. E.g. bad source address, bad checksum,
	// or invalid for current state.
	TX_PKT_FATE_FW_DROP_INVALID,

	// Dropped by firmware due to lack of buffer space.
	TX_PKT_FATE_FW_DROP_NOBUFS,

	// Dropped by firmware for any other reason. Includes frames that
	// were sent by driver to firmware, but unaccounted for by
	// firmware.
	TX_PKT_FATE_FW_DROP_OTHER,

	// Queued within driver, not yet sent to firmware.
	TX_PKT_FATE_DRV_QUEUED,

	// Dropped by driver as invalid. E.g. bad source address, or
	// invalid for current state.
	TX_PKT_FATE_DRV_DROP_INVALID,

	// Dropped by driver due to lack of buffer space.
	TX_PKT_FATE_DRV_DROP_NOBUFS,

	// Dropped by driver for any other reason.
	TX_PKT_FATE_DRV_DROP_OTHER,
} wifi_tx_packet_fate;

typedef enum {
	// Valid and delivered to network stack (e.g., netif_rx()).
	RX_PKT_FATE_SUCCESS,

	// Queued within firmware, but not yet sent to driver.
	RX_PKT_FATE_FW_QUEUED,

	// Dropped by firmware due to host-programmable filters.
	RX_PKT_FATE_FW_DROP_FILTER,

	// Dropped by firmware as invalid. E.g. bad checksum, decrypt failed,
	// or invalid for current state.
	RX_PKT_FATE_FW_DROP_INVALID,

	// Dropped by firmware due to lack of buffer space.
	RX_PKT_FATE_FW_DROP_NOBUFS,

	// Dropped by firmware for any other reason.
	RX_PKT_FATE_FW_DROP_OTHER,

	// Queued within driver, not yet delivered to network stack.
	RX_PKT_FATE_DRV_QUEUED,

	// Dropped by driver due to filter rules.
	RX_PKT_FATE_DRV_DROP_FILTER,

	// Dropped by driver as invalid. E.g. not permitted in current state.
	RX_PKT_FATE_DRV_DROP_INVALID,

	// Dropped by driver due to lack of buffer space.
	RX_PKT_FATE_DRV_DROP_NOBUFS,

	// Dropped by driver for any other reason.
	RX_PKT_FATE_DRV_DROP_OTHER,
} wifi_rx_packet_fate;

typedef enum {
	FRAME_TYPE_UNKNOWN,
	FRAME_TYPE_ETHERNET_II,
	FRAME_TYPE_80211_MGMT,
} frame_type;

struct scsc_frame_info {
	// The type of MAC-layer frame that this frame_info holds.
	// - For data frames, use FRAME_TYPE_ETHERNET_II.
	// - For management frames, use FRAME_TYPE_80211_MGMT.
	// - If the type of the frame is unknown, use FRAME_TYPE_UNKNOWN.
	frame_type payload_type;

	// The number of bytes included in |frame_content|. If the frame
	// contents are missing (e.g. RX frame dropped in firmware),
	// |frame_len| should be set to 0.
	size_t frame_len;

	// Host clock when this frame was received by the driver (either
	// outbound from the host network stack, or inbound from the
	// firmware).
	// - The timestamp should be taken from a clock which includes time
	//   the host spent suspended (e.g. ktime_get_boottime()).
	// - If no host timestamp is available (e.g. RX frame was dropped in
	//   firmware), this field should be set to 0.
	u32 driver_timestamp_usec;

	// Firmware clock when this frame was received by the firmware
	// (either outbound from the host, or inbound from a remote
	// station).
	// - The timestamp should be taken from a clock which includes time
	//   firmware spent suspended (if applicable).
	// - If no firmware timestamp is available (e.g. TX frame was
	//   dropped by driver), this field should be set to 0.
	// - Consumers of |frame_info| should _not_ assume any
	//   synchronization between driver and firmware clocks.
	u32 firmware_timestamp_usec;

	// Actual frame content.
	// - Should be provided for TX frames originated by the host.
	// - Should be provided for RX frames received by the driver.
	// - Optionally provided for TX frames originated by firmware. (At
	//   discretion of HAL implementation.)
	// - Optionally provided for RX frames dropped in firmware. (At
	//   discretion of HAL implementation.)
	// - If frame content is not provided, |frame_len| should be set
	//   to 0.
	union {
		char ethernet_ii_bytes[MAX_FRAME_LEN_ETHERNET];
		char ieee_80211_mgmt_bytes[MAX_FRAME_LEN_80211_MGMT];
	} frame_content;
};

typedef struct {
	// Prefix of MD5 hash of |frame_inf.frame_content|. If frame
	// content is not provided, prefix of MD5 hash over the same data
	// that would be in frame_content, if frame content were provided.
	char md5_prefix[MD5_PREFIX_LEN];
	wifi_tx_packet_fate fate;
	struct scsc_frame_info frame_inf;
} __packed wifi_tx_report;

typedef struct {
	// Prefix of MD5 hash of |frame_inf.frame_content|. If frame
	// content is not provided, prefix of MD5 hash over the same data
	// that would be in frame_content, if frame content were provided.
	char md5_prefix[MD5_PREFIX_LEN];
	wifi_rx_packet_fate fate;
	struct scsc_frame_info frame_inf;
} __packed wifi_rx_report;
#endif /* _SCSC_WIFILOGGER_TYPES_H_ */
