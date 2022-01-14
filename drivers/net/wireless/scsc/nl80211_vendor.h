/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "nl80211_vendor_nan.h"

#ifndef __SLSI_NL80211_VENDOR_H_
#define __SLSI_NL80211_VENDOR_H_

#define OUI_GOOGLE                                      0x001A11
#define OUI_SAMSUNG                                     0x0000f0
#define SLSI_NL80211_GSCAN_SUBCMD_RANGE_START           0x1000
#define SLSI_NL80211_GSCAN_EVENT_RANGE_START            0x01
#define SLSI_NL80211_RTT_SUBCMD_RANGE_START             0x1100
#define SLSI_NL80211_LOGGING_SUBCMD_RANGE_START         0x1400
#define SLSI_NL80211_NAN_SUBCMD_RANGE_START             0x1500
#define SLSI_NL80211_APF_SUBCMD_RANGE_START             0x1600
#define SLSI_GSCAN_SCAN_ID_START                        0x410
#define SLSI_GSCAN_SCAN_ID_END                          0x500

#define SLSI_GSCAN_MAX_BUCKETS                          (8)
#define SLSI_GSCAN_MAX_CHANNELS                         (16) /* As per gscan.h */
#define SLSI_GSCAN_MAX_HOTLIST_APS                      (64)
#define SLSI_GSCAN_MAX_BUCKETS_PER_GSCAN                (SLSI_GSCAN_MAX_BUCKETS)
#define SLSI_GSCAN_MAX_SCAN_CACHE_SIZE                  (12000)
#define SLSI_GSCAN_MAX_AP_CACHE_PER_SCAN                (16)
#define SLSI_GSCAN_MAX_SCAN_REPORTING_THRESHOLD         (100)
#define SLSI_GSCAN_MAX_SIGNIFICANT_CHANGE_APS           (64)
#define SLSI_GSCAN_MAX_EPNO_SSIDS                       (32)
#define SLSI_GSCAN_MAX_EPNO_HS2_PARAM                   (8) /* Framework is not using this. Tune when needed */

#define SLSI_REPORT_EVENTS_NONE                         (0)
#define SLSI_REPORT_EVENTS_EACH_SCAN                    (1)
#define SLSI_REPORT_EVENTS_FULL_RESULTS                 (2)
#define SLSI_REPORT_EVENTS_NO_BATCH                     (4)

#define SLSI_NL_ATTRIBUTE_U32_LEN                       (NLA_HDRLEN + 4)

#define SLSI_NL_VENDOR_ID_OVERHEAD                      SLSI_NL_ATTRIBUTE_U32_LEN
#define SLSI_NL_VENDOR_SUBCMD_OVERHEAD                  SLSI_NL_ATTRIBUTE_U32_LEN
#define SLSI_NL_VENDOR_DATA_OVERHEAD                    (NLA_HDRLEN)

#define SLSI_NL_VENDOR_REPLY_OVERHEAD                   (SLSI_NL_VENDOR_ID_OVERHEAD + \
							 SLSI_NL_VENDOR_SUBCMD_OVERHEAD + \
							 SLSI_NL_VENDOR_DATA_OVERHEAD)

#define SLSI_GSCAN_RTT_UNSPECIFIED                      (-1)
#define SLSI_GSCAN_HASH_TABLE_SIZE                      (32)
#define SLSI_GSCAN_HASH_KEY_MASK                        (0x1F)
#define SLSI_GSCAN_GET_HASH_KEY(_key)                   (_key & SLSI_GSCAN_HASH_KEY_MASK)

#define SLSI_KEEP_SCAN_RESULT                           (0)
#define SLSI_DISCARD_SCAN_RESULT                        (1)

#define SLSI_GSCAN_MAX_BSSID_PER_IE                     (20)

#define SLSI_LLS_CAPABILITY_QOS          0x00000001     /* set for QOS association */
#define SLSI_LLS_CAPABILITY_PROTECTED    0x00000002     /* set for protected association (802.11 beacon frame control protected bit set)*/
#define SLSI_LLS_CAPABILITY_INTERWORKING 0x00000004     /* set if 802.11 Extended Capabilities element interworking bit is set*/
#define SLSI_LLS_CAPABILITY_HS20         0x00000008     /* set for HS20 association*/
#define SLSI_LLS_CAPABILITY_SSID_UTF8    0x00000010     /* set is 802.11 Extended Capabilities element UTF-8 SSID bit is set*/
#define SLSI_LLS_CAPABILITY_COUNTRY      0x00000020     /* set is 802.11 Country Element is present*/

#define TIMESPEC_TO_US(ts)  (((u64)(ts).tv_sec * USEC_PER_SEC) + (ts).tv_nsec / NSEC_PER_USEC)

/* Feature enums */
#define SLSI_WIFI_HAL_FEATURE_INFRA              0x000001      /* Basic infrastructure mode */
#define SLSI_WIFI_HAL_FEATURE_INFRA_5G           0x000002      /* Support for 5 GHz Band */
#define SLSI_WIFI_HAL_FEATURE_HOTSPOT            0x000004      /* Support for GAS/ANQP */
#define SLSI_WIFI_HAL_FEATURE_P2P                0x000008      /* Wifi-Direct */
#define SLSI_WIFI_HAL_FEATURE_SOFT_AP            0x000010      /* Soft AP */
#define SLSI_WIFI_HAL_FEATURE_GSCAN              0x000020      /* Google-Scan APIs */
#define SLSI_WIFI_HAL_FEATURE_NAN                0x000040      /* Neighbor Awareness Networking */
#define SLSI_WIFI_HAL_FEATURE_D2D_RTT            0x000080      /* Device-to-device RTT */
#define SLSI_WIFI_HAL_FEATURE_D2AP_RTT           0x000100      /* Device-to-AP RTT */
#define SLSI_WIFI_HAL_FEATURE_BATCH_SCAN         0x000200      /* Batched Scan (legacy) */
#define SLSI_WIFI_HAL_FEATURE_PNO                0x000400      /* Preferred network offload */
#define SLSI_WIFI_HAL_FEATURE_ADDITIONAL_STA     0x000800      /* Support for two STAs */
#define SLSI_WIFI_HAL_FEATURE_TDLS               0x001000      /* Tunnel directed link setup */
#define SLSI_WIFI_HAL_FEATURE_TDLS_OFFCHANNEL    0x002000      /* Support for TDLS off channel */
#define SLSI_WIFI_HAL_FEATURE_EPR                0x004000      /* Enhanced power reporting */
#define SLSI_WIFI_HAL_FEATURE_AP_STA             0x008000      /* Support for AP STA Concurrency */
#define SLSI_WIFI_HAL_FEATURE_LINK_LAYER_STATS   0x010000      /* Link layer stats collection */
#define SLSI_WIFI_HAL_FEATURE_LOGGER             0x020000      /* WiFi Logger */
#define SLSI_WIFI_HAL_FEATURE_HAL_EPNO           0x040000      /* WiFi PNO enhanced */
#define SLSI_WIFI_HAL_FEATURE_RSSI_MONITOR       0x080000      /* RSSI Monitor */
#define SLSI_WIFI_HAL_FEATURE_MKEEP_ALIVE        0x100000      /* WiFi mkeep_alive */
#define SLSI_WIFI_HAL_FEATURE_CONFIG_NDO         0x200000      /* ND offload */
#define SLSI_WIFI_HAL_FEATURE_CONTROL_ROAMING    0x800000      /* Enable/Disable firmware roaming macro */
#define SLSI_WIFI_HAL_FEATURE_SCAN_RAND          0x2000000     /* Random MAC & Probe seq */
#define SLSI_WIFI_HAL_FEATURE_LOW_LATENCY        0x40000000    /* Low Latency modes */
#define SLSI_WIFI_HAL_FEATURE_P2P_RAND_MAC       0x80000000    /* Random P2P MAC */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
#define SLSI_ATTRIBUTE_START_VAL 1
#else
#define SLSI_ATTRIBUTE_START_VAL 0
#define SLSI_NL_ATTRIBUTE_COUNTRY_CODE 4
#define SLSI_NL_ATTRIBUTE_LATENCY_MODE 5
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
enum slsi_low_latency_attr {
	SLSI_NL_ATTRIBUTE_LATENCY_MODE = 1,
	SLSI_NL_ATTRIBUTE_LATENCY_MAX
};

enum slsi_country_code_attr {
	SLSI_NL_ATTRIBUTE_COUNTRY_CODE = 1,
	SLSI_NL_ATTRIBUTE_COUNTRY_CODE_MAX
};
#endif

enum slsi_wifi_attr {
	SLSI_NL_ATTRIBUTE_ND_OFFLOAD_VALUE = SLSI_ATTRIBUTE_START_VAL,
	SLSI_NL_ATTRIBUTE_PNO_RANDOM_MAC_OUI,
	SLSI_NL_ATTRIBUTE_MAC_OUI_MAX
};

enum SLSI_APF_ATTRIBUTES {
	SLSI_APF_ATTR_VERSION = 0,
	SLSI_APF_ATTR_MAX_LEN,
	SLSI_APF_ATTR_PROGRAM,
	SLSI_APF_ATTR_PROGRAM_LEN,
	SLSI_APF_ATTR_MAX
};

enum SLSI_ROAM_ATTRIBUTES {
	SLSI_NL_ATTR_MAX_BLACKLIST_SIZE,
	SLSI_NL_ATTR_MAX_WHITELIST_SIZE,
	SLSI_NL_ATTR_ROAM_STATE,
	SLSI_NL_ATTR_ROAM_MAX
};

enum slsi_acs_attr_offload {
	SLSI_ACS_ATTR_CHANNEL_INVALID = 0,
	SLSI_ACS_ATTR_PRIMARY_CHANNEL,
	SLSI_ACS_ATTR_SECONDARY_CHANNEL,
	SLSI_ACS_ATTR_HW_MODE,
	SLSI_ACS_ATTR_HT_ENABLED,
	SLSI_ACS_ATTR_HT40_ENABLED,
	SLSI_ACS_ATTR_VHT_ENABLED,
	SLSI_ACS_ATTR_CHWIDTH,
	SLSI_ACS_ATTR_CH_LIST,
	SLSI_ACS_ATTR_VHT_SEG0_CENTER_CHANNEL,
	SLSI_ACS_ATTR_VHT_SEG1_CENTER_CHANNEL,
	SLSI_ACS_ATTR_FREQ_LIST,
	/* keep last */
	SLSI_ACS_ATTR_AFTER_LAST,
	SLSI_ACS_ATTR_MAX =
	SLSI_ACS_ATTR_AFTER_LAST - 1
};

#ifdef CONFIG_SLSI_WLAN_STA_FWD_BEACON
enum slsi_wips_attr {
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_SSID = 0,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_BSSID,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_CHANNEL,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_BCN_INTERVAL,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_TIME_STAMP1,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_TIME_STAMP2,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_SYS_TIME,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_MAX
};

enum slsi_wips_abort_attr {
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_ABORT = 0,
	SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_ABORT_MAX,
};

enum slsi_forward_beacon_abort_reason {
	SLSI_FORWARD_BEACON_ABORT_REASON_UNSPECIFIED = 0,
	SLSI_FORWARD_BEACON_ABORT_REASON_SCANNING,
	SLSI_FORWARD_BEACON_ABORT_REASON_ROAMING,
	SLSI_FORWARD_BEACON_ABORT_REASON_SUSPENDED,
	SLSI_FORWARD_BEACON_ABORT_REASON_OFFSET = 0x8007,
};
#endif

enum slsi_acs_hw_mode {
	SLSI_ACS_MODE_IEEE80211B,
	SLSI_ACS_MODE_IEEE80211G,
	SLSI_ACS_MODE_IEEE80211A,
	SLSI_ACS_MODE_IEEE80211AD,
	SLSI_ACS_MODE_IEEE80211ANY,
};

enum GSCAN_ATTRIBUTE {
	GSCAN_ATTRIBUTE_NUM_BUCKETS = 10,
	GSCAN_ATTRIBUTE_BASE_PERIOD,
	GSCAN_ATTRIBUTE_BUCKETS_BAND,
	GSCAN_ATTRIBUTE_BUCKET_ID,
	GSCAN_ATTRIBUTE_BUCKET_PERIOD,
	GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS,
	GSCAN_ATTRIBUTE_BUCKET_CHANNELS,
	GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN,
	GSCAN_ATTRIBUTE_REPORT_THRESHOLD,
	GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE,
	GSCAN_ATTRIBUTE_REPORT_THRESHOLD_NUM_SCANS,
	GSCAN_ATTRIBUTE_BAND = GSCAN_ATTRIBUTE_BUCKETS_BAND,

	GSCAN_ATTRIBUTE_ENABLE_FEATURE = 20,
	GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE, /* indicates no more results */
	GSCAN_ATTRIBUTE_REPORT_EVENTS,

	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_NUM_OF_RESULTS = 30,
	GSCAN_ATTRIBUTE_SCAN_RESULTS, /* flat array of wifi_scan_result */
	GSCAN_ATTRIBUTE_NUM_CHANNELS,
	GSCAN_ATTRIBUTE_CHANNEL_LIST,
	GSCAN_ATTRIBUTE_SCAN_ID,
	GSCAN_ATTRIBUTE_SCAN_FLAGS,
	GSCAN_ATTRIBUTE_SCAN_BUCKET_BIT,

	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE = 60,
	GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE,
	GSCAN_ATTRIBUTE_MIN_BREACHING,
	GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_BSSIDS,

	GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT = 70,
	GSCAN_ATTRIBUTE_BUCKET_EXPONENT,
	GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD,

	GSCAN_ATTRIBUTE_NUM_BSSID,
	GSCAN_ATTRIBUTE_BLACKLIST_BSSID,
	GSCAN_ATTRIBUTE_BLACKLIST_FROM_SUPPLICANT,

	GSCAN_ATTRIBUTE_MAX
};

enum epno_ssid_attribute {
	SLSI_ATTRIBUTE_EPNO_MINIMUM_5G_RSSI = SLSI_ATTRIBUTE_START_VAL,
	SLSI_ATTRIBUTE_EPNO_MINIMUM_2G_RSSI,
	SLSI_ATTRIBUTE_EPNO_INITIAL_SCORE_MAX,
	SLSI_ATTRIBUTE_EPNO_CUR_CONN_BONUS,
	SLSI_ATTRIBUTE_EPNO_SAME_NETWORK_BONUS,
	SLSI_ATTRIBUTE_EPNO_SECURE_BONUS,
	SLSI_ATTRIBUTE_EPNO_5G_BONUS,
	SLSI_ATTRIBUTE_EPNO_SSID_NUM,
	SLSI_ATTRIBUTE_EPNO_SSID_LIST,
	SLSI_ATTRIBUTE_EPNO_SSID,
	SLSI_ATTRIBUTE_EPNO_SSID_LEN,
	SLSI_ATTRIBUTE_EPNO_FLAGS,
	SLSI_ATTRIBUTE_EPNO_AUTH,
	SLSI_ATTRIBUTE_EPNO_MAX
};

enum epno_hs_attribute {
	SLSI_ATTRIBUTE_EPNO_HS_PARAM_LIST = SLSI_ATTRIBUTE_START_VAL,
	SLSI_ATTRIBUTE_EPNO_HS_NUM,
	SLSI_ATTRIBUTE_EPNO_HS_ID,
	SLSI_ATTRIBUTE_EPNO_HS_REALM,
	SLSI_ATTRIBUTE_EPNO_HS_CONSORTIUM_IDS,
	SLSI_ATTRIBUTE_EPNO_HS_PLMN,
	SLSI_ATTRIBUTE_EPNO_HS_MAX
};

enum gscan_bucket_attributes {
	GSCAN_ATTRIBUTE_CH_BUCKET_1,
	GSCAN_ATTRIBUTE_CH_BUCKET_2,
	GSCAN_ATTRIBUTE_CH_BUCKET_3,
	GSCAN_ATTRIBUTE_CH_BUCKET_4,
	GSCAN_ATTRIBUTE_CH_BUCKET_5,
	GSCAN_ATTRIBUTE_CH_BUCKET_6,
	GSCAN_ATTRIBUTE_CH_BUCKET_7,
	GSCAN_ATTRIBUTE_CH_BUCKET_8
};

enum wifi_band {
	WIFI_BAND_UNSPECIFIED,
	WIFI_BAND_BG = 1,                       /* 2.4 GHz */
	WIFI_BAND_A = 2,                        /* 5 GHz without DFS */
	WIFI_BAND_A_DFS = 4,                    /* 5 GHz DFS only */
	WIFI_BAND_A_WITH_DFS = 6,               /* 5 GHz with DFS */
	WIFI_BAND_ABG = 3,                      /* 2.4 GHz + 5 GHz; no DFS */
	WIFI_BAND_ABG_WITH_DFS = 7,             /* 2.4 GHz + 5 GHz with DFS */
};

enum wifi_scan_event {
	WIFI_SCAN_RESULTS_AVAILABLE,
	WIFI_SCAN_THRESHOLD_NUM_SCANS,
	WIFI_SCAN_THRESHOLD_PERCENT,
	WIFI_SCAN_FAILED,
};

enum wifi_mkeep_alive_attribute {
	MKEEP_ALIVE_ATTRIBUTE_ID = SLSI_ATTRIBUTE_START_VAL,
	MKEEP_ALIVE_ATTRIBUTE_IP_PKT,
	MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN,
	MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR,
	MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR,
	MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC,
	MKEEP_ALIVE_ATTRIBUTE_MAX
};

enum wifi_rssi_monitor_attr {
	SLSI_RSSI_MONITOR_ATTRIBUTE_MAX_RSSI = SLSI_ATTRIBUTE_START_VAL,
	SLSI_RSSI_MONITOR_ATTRIBUTE_MIN_RSSI,
	SLSI_RSSI_MONITOR_ATTRIBUTE_START,
	SLSI_RSSI_MONITOR_ATTRIBUTE_MAX
};

enum lls_attribute {
	LLS_ATTRIBUTE_SET_MPDU_SIZE_THRESHOLD = 1,
	LLS_ATTRIBUTE_SET_AGGR_STATISTICS_GATHERING,
	LLS_ATTRIBUTE_CLEAR_STOP_REQUEST_MASK,
	LLS_ATTRIBUTE_CLEAR_STOP_REQUEST,
	LLS_ATTRIBUTE_MAX
};

enum slsi_hal_vendor_subcmds {
	SLSI_NL80211_VENDOR_SUBCMD_GET_CAPABILITIES = SLSI_NL80211_GSCAN_SUBCMD_RANGE_START,
	SLSI_NL80211_VENDOR_SUBCMD_GET_VALID_CHANNELS,
	SLSI_NL80211_VENDOR_SUBCMD_ADD_GSCAN,
	SLSI_NL80211_VENDOR_SUBCMD_DEL_GSCAN,
	SLSI_NL80211_VENDOR_SUBCMD_GET_SCAN_RESULTS,
	/*****Deprecated due to fapi updates.Do not remove.************/
	SLSI_NL80211_VENDOR_SUBCMD_SET_BSSID_HOTLIST,
	SLSI_NL80211_VENDOR_SUBCMD_RESET_BSSID_HOTLIST,
	SLSI_NL80211_VENDOR_SUBCMD_GET_HOTLIST_RESULTS,
	SLSI_NL80211_VENDOR_SUBCMD_SET_SIGNIFICANT_CHANGE,
	SLSI_NL80211_VENDOR_SUBCMD_RESET_SIGNIFICANT_CHANGE,
	/********************************************************/
	SLSI_NL80211_VENDOR_SUBCMD_SET_GSCAN_OUI,
	SLSI_NL80211_VENDOR_SUBCMD_SET_NODFS,
	SLSI_NL80211_VENDOR_SUBCMD_START_KEEP_ALIVE_OFFLOAD,
	SLSI_NL80211_VENDOR_SUBCMD_STOP_KEEP_ALIVE_OFFLOAD,
	SLSI_NL80211_VENDOR_SUBCMD_SET_BSSID_BLACKLIST,
	SLSI_NL80211_VENDOR_SUBCMD_SET_EPNO_LIST,
	SLSI_NL80211_VENDOR_SUBCMD_SET_HS_LIST,
	SLSI_NL80211_VENDOR_SUBCMD_RESET_HS_LIST,
	SLSI_NL80211_VENDOR_SUBCMD_SET_RSSI_MONITOR,
	SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_SET_STATS,
	SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_GET_STATS,
	SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_CLEAR_STATS,
	SLSI_NL80211_VENDOR_SUBCMD_GET_FEATURE_SET,
	SLSI_NL80211_VENDOR_SUBCMD_SET_COUNTRY_CODE,
	SLSI_NL80211_VENDOR_SUBCMD_CONFIGURE_ND_OFFLOAD,
	SLSI_NL80211_VENDOR_SUBCMD_GET_ROAMING_CAPABILITIES,
	SLSI_NL80211_VENDOR_SUBCMD_SET_ROAMING_STATE,
	SLSI_NL80211_VENDOR_SUBCMD_SET_LATENCY_MODE,
	SLSI_NL80211_VENDOR_SUBCMD_RTT_GET_CAPABILITIES = SLSI_NL80211_RTT_SUBCMD_RANGE_START,
	SLSI_NL80211_VENDOR_SUBCMD_RTT_RANGE_START,
	SLSI_NL80211_VENDOR_SUBCMD_RTT_RANGE_CANCEL,
	SLSI_NL80211_VENDOR_SUBCMD_START_LOGGING = SLSI_NL80211_LOGGING_SUBCMD_RANGE_START,
	SLSI_NL80211_VENDOR_SUBCMD_TRIGGER_FW_MEM_DUMP,
	SLSI_NL80211_VENDOR_SUBCMD_GET_FW_MEM_DUMP,
	SLSI_NL80211_VENDOR_SUBCMD_GET_VERSION,
	SLSI_NL80211_VENDOR_SUBCMD_GET_RING_STATUS,
	SLSI_NL80211_VENDOR_SUBCMD_GET_RING_DATA,
	SLSI_NL80211_VENDOR_SUBCMD_GET_FEATURE,
	SLSI_NL80211_VENDOR_SUBCMD_RESET_LOGGING,
	SLSI_NL80211_VENDOR_SUBCMD_TRIGGER_DRIVER_MEM_DUMP,
	SLSI_NL80211_VENDOR_SUBCMD_GET_DRIVER_MEM_DUMP,
	SLSI_NL80211_VENDOR_SUBCMD_START_PKT_FATE_MONITORING,
	SLSI_NL80211_VENDOR_SUBCMD_GET_TX_PKT_FATES,
	SLSI_NL80211_VENDOR_SUBCMD_GET_RX_PKT_FATES,
	SLSI_NL80211_VENDOR_SUBCMD_GET_WAKE_REASON_STATS,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_ENABLE = SLSI_NL80211_NAN_SUBCMD_RANGE_START,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_DISABLE,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_PUBLISH,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_PUBLISHCANCEL,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_SUBSCRIBE,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_SUBSCRIBECANCEL,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_TXFOLLOWUP,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_CONFIG,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_CAPABILITIES,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_DATA_INTERFACE_CREATE,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_DATA_INTERFACE_DELETE,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_DATA_REQUEST_INITIATOR,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_DATA_INDICATION_RESPONSE,
	SLSI_NL80211_VENDOR_SUBCMD_NAN_DATA_END,
	SLSI_NL80211_VENDOR_SUBCMD_APF_SET_FILTER = SLSI_NL80211_APF_SUBCMD_RANGE_START,
	SLSI_NL80211_VENDOR_SUBCMD_APF_GET_CAPABILITIES,
	SLSI_NL80211_VENDOR_SUBCMD_APF_READ_FILTER
};

enum slsi_supp_vendor_subcmds {
	SLSI_NL80211_VENDOR_SUBCMD_UNSPEC = 0,
	SLSI_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY,
	SLSI_NL80211_VENDOR_SUBCMD_ACS_INIT,
};

enum slsi_vendor_event_values {
	/**********Deprecated now due to fapi updates.Do not remove*/
	SLSI_NL80211_SIGNIFICANT_CHANGE_EVENT,
	SLSI_NL80211_HOTLIST_AP_FOUND_EVENT,
	/******************************************/
	SLSI_NL80211_SCAN_RESULTS_AVAILABLE_EVENT,
	SLSI_NL80211_FULL_SCAN_RESULT_EVENT,
	SLSI_NL80211_SCAN_EVENT,
	/**********Deprecated now due to fapi updates.Do not remove*/
	SLSI_NL80211_HOTLIST_AP_LOST_EVENT,
	/******************************************/
	SLSI_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH,
	SLSI_NL80211_VENDOR_HANGED_EVENT,
	SLSI_NL80211_EPNO_EVENT,
	SLSI_NL80211_HOTSPOT_MATCH,
	SLSI_NL80211_RSSI_REPORT_EVENT = 10,
	SLSI_NL80211_LOGGER_RING_EVENT,
	SLSI_NL80211_LOGGER_FW_DUMP_EVENT,
	SLSI_NL80211_NAN_RESPONSE_EVENT,
	SLSI_NL80211_NAN_PUBLISH_TERMINATED_EVENT,
	SLSI_NL80211_NAN_MATCH_EVENT,
	SLSI_NL80211_NAN_MATCH_EXPIRED_EVENT,
	SLSI_NL80211_NAN_SUBSCRIBE_TERMINATED_EVENT,
	SLSI_NL80211_NAN_FOLLOWUP_EVENT,
	SLSI_NL80211_NAN_DISCOVERY_ENGINE_EVENT,
	SLSI_NL80211_NAN_DISABLED_EVENT = 20,
	SLSI_NL80211_RTT_RESULT_EVENT,
	SLSI_NL80211_RTT_COMPLETE_EVENT,
	SLSI_NL80211_VENDOR_ACS_EVENT,
	SLSI_NL80211_VENDOR_FORWARD_BEACON,
	SLSI_NL80211_VENDOR_FORWARD_BEACON_ABORT,
	SLSI_NL80211_NAN_TRANSMIT_FOLLOWUP_STATUS,
	SLSI_NAN_EVENT_NDP_REQ,
	SLSI_NAN_EVENT_NDP_CFM,
	SLSI_NAN_EVENT_NDP_END,
	SLSI_NL80211_VENDOR_RCL_CHANNEL_LIST_EVENT = 30
};

enum slsi_lls_interface_mode {
	SLSI_LLS_INTERFACE_STA = 0,
	SLSI_LLS_INTERFACE_SOFTAP = 1,
	SLSI_LLS_INTERFACE_IBSS = 2,
	SLSI_LLS_INTERFACE_P2P_CLIENT = 3,
	SLSI_LLS_INTERFACE_P2P_GO = 4,
	SLSI_LLS_INTERFACE_NAN = 5,
	SLSI_LLS_INTERFACE_MESH = 6,
	SLSI_LLS_INTERFACE_UNKNOWN = -1
};

enum slsi_lls_connection_state {
	SLSI_LLS_DISCONNECTED = 0,
	SLSI_LLS_AUTHENTICATING = 1,
	SLSI_LLS_ASSOCIATING = 2,
	SLSI_LLS_ASSOCIATED = 3,
	SLSI_LLS_EAPOL_STARTED = 4,   /* if done by firmware/driver*/
	SLSI_LLS_EAPOL_COMPLETED = 5, /* if done by firmware/driver*/
};

enum slsi_lls_roam_state {
	SLSI_LLS_ROAMING_IDLE = 0,
	SLSI_LLS_ROAMING_ACTIVE = 1,
};

/* access categories */
enum slsi_lls_traffic_ac {
	SLSI_LLS_AC_VO  = 0,
	SLSI_LLS_AC_VI  = 1,
	SLSI_LLS_AC_BE  = 2,
	SLSI_LLS_AC_BK  = 3,
	SLSI_LLS_AC_MAX = 4,
};

/* channel operating width */
enum slsi_lls_channel_width {
	SLSI_LLS_CHAN_WIDTH_20    = 0,
	SLSI_LLS_CHAN_WIDTH_40    = 1,
	SLSI_LLS_CHAN_WIDTH_80    = 2,
	SLSI_LLS_CHAN_WIDTH_160   = 3,
	SLSI_LLS_CHAN_WIDTH_80P80 = 4,
	SLSI_LLS_CHAN_WIDTH_5     = 5,
	SLSI_LLS_CHAN_WIDTH_10    = 6,
	SLSI_LLS_CHAN_WIDTH_INVALID = -1
};

/* wifi peer type */
enum slsi_lls_peer_type {
	SLSI_LLS_PEER_STA,
	SLSI_LLS_PEER_AP,
	SLSI_LLS_PEER_P2P_GO,
	SLSI_LLS_PEER_P2P_CLIENT,
	SLSI_LLS_PEER_NAN,
	SLSI_LLS_PEER_TDLS,
	SLSI_LLS_PEER_INVALID,
};

/* slsi_enhanced_logging_attributes */
enum slsi_enhanced_logging_attributes {
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_DRIVER_VERSION = SLSI_ATTRIBUTE_START_VAL,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_FW_VERSION,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_ID,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_NAME,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_FLAGS,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_VERBOSE_LEVEL,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_LOG_MAX_INTERVAL,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_LOG_MIN_DATA_SIZE,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_FW_DUMP_LEN,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_FW_DUMP_DATA,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_DATA,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_STATUS,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_NUM,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_DRIVER_DUMP_LEN,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_DRIVER_DUMP_DATA,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_NUM,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_DATA,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_MAX,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_INVALID = 0,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_TOTAL_CMD_EVENT_WAKE,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_CMD_EVENT_WAKE_CNT_PTR,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_CMD_EVENT_WAKE_CNT_SZ,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_TOTAL_DRIVER_FW_LOCAL_WAKE,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_DRIVER_FW_LOCAL_WAKE_CNT_PTR,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_DRIVER_FW_LOCAL_WAKE_CNT_SZ,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_TOTAL_RX_DATA_WAKE,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_RX_UNICAST_CNT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_RX_MULTICAST_CNT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_RX_BROADCAST_CNT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_ICMP_PKT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_ICMP6_PKT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_ICMP6_RA,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_ICMP6_NA,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_ICMP6_NS,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_ICMP4_RX_MULTICAST_CNT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_ICMP6_RX_MULTICAST_CNT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_OTHER_RX_MULTICAST_CNT,
	SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_MAX,
};

enum slsi_rtt_event_attributes {
	SLSI_RTT_EVENT_ATTR_ADDR     = 0,
	SLSI_RTT_EVENT_ATTR_BURST_NUM,
	SLSI_RTT_EVENT_ATTR_MEASUREMENT_NUM,
	SLSI_RTT_EVENT_ATTR_SUCCESS_NUM,
	SLSI_RTT_EVENT_ATTR_NUM_PER_BURST_PEER,
	SLSI_RTT_EVENT_ATTR_STATUS,
	SLSI_RTT_EVENT_ATTR_RETRY_AFTER_DURATION,
	SLSI_RTT_EVENT_ATTR_TYPE,
	SLSI_RTT_EVENT_ATTR_RSSI,
	SLSI_RTT_EVENT_ATTR_RSSI_SPREAD,
	SLSI_RTT_EVENT_ATTR_TX_PREAMBLE,
	SLSI_RTT_EVENT_ATTR_TX_NSS,
	SLSI_RTT_EVENT_ATTR_TX_BW,
	SLSI_RTT_EVENT_ATTR_TX_MCS,
	SLSI_RTT_EVENT_ATTR_TX_RATE,
	SLSI_RTT_EVENT_ATTR_RX_PREAMBLE,
	SLSI_RTT_EVENT_ATTR_RX_NSS,
	SLSI_RTT_EVENT_ATTR_RX_BW,
	SLSI_RTT_EVENT_ATTR_RX_MCS,
	SLSI_RTT_EVENT_ATTR_RX_RATE,
	SLSI_RTT_EVENT_ATTR_RTT,
	SLSI_RTT_EVENT_ATTR_RTT_SD,
	SLSI_RTT_EVENT_ATTR_RTT_SPREAD,
	SLSI_RTT_EVENT_ATTR_DISTANCE_MM,
	SLSI_RTT_EVENT_ATTR_DISTANCE_SD_MM,
	SLSI_RTT_EVENT_ATTR_DISTANCE_SPREAD_MM,
	SLSI_RTT_EVENT_ATTR_TIMESTAMP_US,
	SLSI_RTT_EVENT_ATTR_BURST_DURATION_MSN,
	SLSI_RTT_EVENT_ATTR_NEGOTIATED_BURST_NUM,
	SLSI_RTT_EVENT_ATTR_LCI,
	SLSI_RTT_EVENT_ATTR_LCR,

};

/* RTT peer type */
enum slsi_rtt_peer_type {
	SLSI_RTT_PEER_AP = 0x1,
	SLSI_RTT_PEER_STA,
	SLSI_RTT_PEER_P2P_GO,
	SLSI_RTT_PEER_P2P_CLIENT,
	SLSI_RTT_PEER_NAN,
};

/* RTT Measurement Bandwidth */
enum slsi_wifi_rtt_bw {
	SLSI_WIFI_RTT_BW_5 = 0x01,
	SLSI_WIFI_RTT_BW_10 = 0x02,
	SLSI_WIFI_RTT_BW_20 = 0x04,
	SLSI_WIFI_RTT_BW_40 = 0x08,
	SLSI_WIFI_RTT_BW_80 = 0x10,
	SLSI_WIFI_RTT_BW_160 = 0x20
};

/* RTT Measurement Preamble */
enum slsi_wifi_rtt_preamble {
	SLSI_WIFI_RTT_PREAMBLE_LEGACY = 0x1,
	SLSI_WIFI_RTT_PREAMBLE_HT = 0x2,
	SLSI_WIFI_RTT_PREAMBLE_VHT = 0x4
};

/* RTT Type */
enum slsi_wifi_rtt_type {
	SLSI_RTT_TYPE_1_SIDED = 0x1,
	SLSI_RTT_TYPE_2_SIDED,
};

enum slsi_rtt_attribute {
	SLSI_RTT_ATTRIBUTE_TARGET_CNT = SLSI_ATTRIBUTE_START_VAL,
	SLSI_RTT_ATTRIBUTE_TARGET_INFO,
	SLSI_RTT_ATTRIBUTE_TARGET_MAC,
	SLSI_RTT_ATTRIBUTE_TARGET_TYPE,
	SLSI_RTT_ATTRIBUTE_TARGET_PEER,
	SLSI_RTT_ATTRIBUTE_TARGET_CHAN_FREQ,
	SLSI_RTT_ATTRIBUTE_TARGET_PERIOD,
	SLSI_RTT_ATTRIBUTE_TARGET_NUM_BURST,
	SLSI_RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST,
	SLSI_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM,
	SLSI_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR,
	SLSI_RTT_ATTRIBUTE_TARGET_LCI,
	SLSI_RTT_ATTRIBUTE_TARGET_LCR,
	SLSI_RTT_ATTRIBUTE_TARGET_BURST_DURATION,
	SLSI_RTT_ATTRIBUTE_TARGET_PREAMBLE,
	SLSI_RTT_ATTRIBUTE_TARGET_BW,
	SLSI_RTT_ATTRIBUTE_RESULTS_COMPLETE = 30,
	SLSI_RTT_ATTRIBUTE_RESULTS_PER_TARGET,
	SLSI_RTT_ATTRIBUTE_RESULT_CNT,
	SLSI_RTT_ATTRIBUTE_RESULT,
	SLSI_RTT_ATTRIBUTE_TARGET_ID,
	SLSI_RTT_ATTRIBUTE_MAX
};

/* Ranging status */
enum slsi_wifi_rtt_status {
	SLSI_RTT_STATUS_SUCCESS = 0,
	SLSI_RTT_STATUS_FAILURE,           /* general failure status */
	SLSI_RTT_STATUS_FAIL_NO_RSP,           /* target STA does not respond to request */
	SLSI_RTT_STATUS_FAIL_REJECTED,           /* request rejected. Applies to 2-sided RTT only*/
	SLSI_RTT_STATUS_FAIL_NOT_SCHEDULED_YET,
	SLSI_RTT_STATUS_FAIL_TM_TIMEOUT, /* timing measurement times out */
	SLSI_RTT_STATUS_FAIL_AP_ON_DIFF_CHANNEL, /* Target on different channel, cannot range */
	SLSI_RTT_STATUS_FAIL_NO_CAPABILITY,     /* ranging not supported */
	SLSI_RTT_STATUS_ABORTED,     /* request aborted for unknown reason */
	SLSI_RTT_STATUS_FAIL_INVALID_TS,     /* Invalid T1-T4 timestamp */
	SLSI_RTT_STATUS_FAIL_PROTOCOL,    /* 11mc protocol failed */
	SLSI_RTT_STATUS_FAIL_SCHEDULE,    /* request could not be scheduled */
	SLSI_RTT_STATUS_FAIL_BUSY_TRY_LATER,    /* responder cannot collaborate at time of request */
	SLSI_RTT_STATUS_INVALID_REQ,    /* bad request args */
	SLSI_RTT_STATUS_NO_WIFI,    /* WiFi not enabled */
	SLSI_RTT_STATUS_FAIL_FTM_PARAM_OVERRIDE /* Responder overrides param info, cannot range with new params */
};

/* Format of information elements found in the beacon */
struct slsi_wifi_information_element {
	u8 id;                            /* element identifier */
	u8 len;                           /* number of bytes to follow */
	u8 data[];
};

struct slsi_nl_gscan_capabilities {
	int max_scan_cache_size;
	int max_scan_buckets;
	int max_ap_cache_per_scan;
	int max_rssi_sample_size;
	int max_scan_reporting_threshold;
	int max_hotlist_aps;
	int max_hotlist_ssids;
	int max_significant_wifi_change_aps;
	int max_bssid_history_entries;
	int max_number_epno_networks;
	int max_number_epno_networks_by_ssid;
	int max_number_of_white_listed_ssid;
};

struct slsi_nl_channel_param {
	int channel;
	int dwell_time_ms;
	int passive;         /* 0 => active, 1 => passive scan; ignored for DFS */
};

struct slsi_nl_bucket_param {
	int                          bucket_index;
	enum wifi_band               band;
	int                          period; /* desired period in millisecond */
	u8                           report_events;
	int                          max_period; /* If non-zero: scan period will grow exponentially to a maximum period of max_period */
	int                          exponent;    /* multiplier: new_period = old_period ^ exponent */
	int                          step_count; /* number of scans performed at a given period and until the exponent is applied */
	int                          num_channels;
	struct slsi_nl_channel_param channels[SLSI_GSCAN_MAX_CHANNELS];
};

struct slsi_nl_gscan_param {
	int                         base_period;     /* base timer period in ms */
	int                         max_ap_per_scan; /* number of APs to store in each scan in the BSSID/RSSI history buffer */
	int                         report_threshold_percent; /* when scan_buffer  is this much full, wake up application processor */
	int                         report_threshold_num_scans; /* wake up application processor after these many scans */
	int                         num_buckets;
	struct slsi_nl_bucket_param nl_bucket[SLSI_GSCAN_MAX_BUCKETS];
};

struct slsi_nl_scan_result_param {
	u64 ts;                               /* time since boot (in microsecond) when the result was retrieved */
	u8  ssid[IEEE80211_MAX_SSID_LEN + 1]; /* NULL terminated */
	u8  bssid[6];
	int channel;                          /* channel frequency in MHz */
	int rssi;                             /* in db */
	s64 rtt;                              /* in nanoseconds */
	s64 rtt_sd;                           /* standard deviation in rtt */
	u16 beacon_period;                    /* period advertised in the beacon */
	u16 capability;                       /* capabilities advertised in the beacon */
	u32 ie_length;                        /* size of the ie_data blob */
	u8  ie_data[1];                       /* beacon IE */
};

struct slsi_bucket {
	bool              used;                /* to identify if this entry is free */
	bool              for_change_tracking; /* Indicates if this scan_id is used for change_tracking */
	u8                report_events;       /* this is received from HAL/Framework */
	u16               scan_id;             /* SLSI_GSCAN_SCAN_ID_START + <offset in the array> */
	int               scan_cycle;          /* To find the current scan cycle */
	struct slsi_gscan *gscan;              /* gscan ref in which this bucket belongs */
};

struct slsi_gscan {
	int                         max_ap_per_scan;  /* received from HAL/Framework */
	int                         report_threshold_percent; /* received from HAL/Framework */
	int                         report_threshold_num_scans; /* received from HAL/Framework */
	int                         num_scans;
	int                         num_buckets;      /* received from HAL/Framework */
	struct slsi_nl_bucket_param nl_bucket;        /* store the first bucket params. used in tracking*/
	struct slsi_bucket          *bucket[SLSI_GSCAN_MAX_BUCKETS_PER_GSCAN];
	struct slsi_gscan           *next;
};

struct slsi_gscan_param {
	struct slsi_nl_bucket_param *nl_bucket;
	struct slsi_bucket          *bucket;
};

struct slsi_gscan_result {
	struct slsi_gscan_result         *hnext;
	int                              scan_cycle;
	int                              scan_res_len;
	int                              anqp_length;
	struct slsi_nl_scan_result_param nl_scan_res;
};

struct slsi_epno_ssid_param {
	u16 flags;
	u8  ssid_len;
	u8  ssid[32];
};

struct slsi_epno_param {
	u16 min_5g_rssi;                           /* minimum 5GHz RSSI for a BSSID to be considered */
	u16 min_2g_rssi;                           /* minimum 2.4GHz RSSI for a BSSID to be considered */
	u16 initial_score_max;                     /* maximum score that a network can have before bonuses */
	u8 current_connection_bonus;               /* report current connection bonus only, when there is a
						    * network's score this much higher than the current connection
						    */
	u8 same_network_bonus;                     /* score bonus for all networks with the same network flag */
	u8 secure_bonus;                           /* score bonus for networks that are not open */
	u8 band_5g_bonus;                          /* 5GHz RSSI score bonus (applied to all 5GHz networks) */
	u8 num_networks;                           /* number of wifi_epno_network objects */
	struct slsi_epno_ssid_param epno_ssid[];   /* PNO networks */
};

struct slsi_epno_hs2_param {
	u32 id;                          /* identifier of this network block, report this in event */
	u8  realm[256];                  /* null terminated UTF8 encoded realm, 0 if unspecified */
	s64 roaming_consortium_ids[16];  /* roaming consortium ids to match, 0s if unspecified */
	u8  plmn[3];                     /* mcc/mnc combination as per rules, 0s if unspecified */
};

struct slsi_rssi_monitor_evt {
	s16 rssi;
	u8  bssid[ETH_ALEN];
};

/* channel information */
struct slsi_lls_channel_info {
	enum slsi_lls_channel_width width;   /* channel width (20, 40, 80, 80+80, 160)*/
	int center_freq;   /* primary 20 MHz channel */
	int center_freq0;  /* center frequency (MHz) first segment */
	int center_freq1;  /* center frequency (MHz) second segment */
};

/* channel statistics */
struct slsi_lls_channel_stat {
	struct slsi_lls_channel_info channel;
	u32 on_time;                /* msecs the radio is awake (32 bits number accruing over time) */
	u32 cca_busy_time;          /* msecs the CCA register is busy (32 bits number accruing over time) */
};

/* wifi rate */
struct slsi_lls_rate {
	u32 preamble   :3;   /* 0: OFDM, 1:CCK, 2:HT 3:VHT 4..7 reserved*/
	u32 nss        :2;   /* 0:1x1, 1:2x2, 3:3x3, 4:4x4*/
	u32 bw         :3;   /* 0:20MHz, 1:40Mhz, 2:80Mhz, 3:160Mhz*/
	u32 rate_mcs_idx :8; /* OFDM/CCK rate code mcs index*/
	u32 reserved  :16;   /* reserved*/
	u32 bitrate;         /* units of 100 Kbps*/
};

/* per rate statistics */
struct slsi_lls_rate_stat {
	struct slsi_lls_rate rate;     /* rate information*/
	u32 tx_mpdu;        /* number of successfully transmitted data pkts (ACK rcvd)*/
	u32 rx_mpdu;        /* number of received data pkts*/
	u32 mpdu_lost;      /* number of data packet losses (no ACK)*/
	u32 retries;        /* total number of data pkt retries*/
	u32 retries_short;  /* number of short data pkt retries*/
	u32 retries_long;   /* number of long data pkt retries*/
};

/* radio statistics */
struct slsi_lls_radio_stat {
	int radio;               /* wifi radio (if multiple radio supported)*/
	u32 on_time;                    /* msecs the radio is awake (32 bits number accruing over time)*/
	u32 tx_time;                    /* msecs the radio is transmitting (32 bits number accruing over time)*/
	u32 rx_time;                    /* msecs the radio is in active receive (32 bits number accruing over time)*/
	u32 on_time_scan;               /* msecs the radio is awake due to all scan (32 bits number accruing over time)*/
	u32 on_time_nbd;                /* msecs the radio is awake due to NAN (32 bits number accruing over time)*/
	u32 on_time_gscan;              /* msecs the radio is awake due to G?scan (32 bits number accruing over time)*/
	u32 on_time_roam_scan;          /* msecs the radio is awake due to roam?scan (32 bits number accruing over time)*/
	u32 on_time_pno_scan;           /* msecs the radio is awake due to PNO scan (32 bits number accruing over time)*/
	u32 on_time_hs20;               /* msecs the radio is awake due to HS2.0 scans and GAS exchange (32 bits number accruing over time)*/
	u32 num_channels;               /* number of channels*/
	struct slsi_lls_channel_stat channels[];   /* channel statistics*/
};

struct slsi_lls_interface_link_layer_info {
	enum slsi_lls_interface_mode mode;     /* interface mode*/
	u8   mac_addr[6];                  /* interface mac address (self)*/
	enum slsi_lls_connection_state state;  /* connection state (valid for STA, CLI only)*/
	enum slsi_lls_roam_state roaming;      /* roaming state*/
	u32 capabilities;                  /* WIFI_CAPABILITY_XXX (self)*/
	u8 ssid[33];                       /* null terminated SSID*/
	u8 bssid[6];                       /* bssid*/
	u8 ap_country_str[3];              /* country string advertised by AP*/
	u8 country_str[3];                 /* country string for this association*/
};

struct slsi_lls_bssload_info {
	u16 sta_count;    // station count
	u16 chan_util;    // channel utilization
	u8 PAD[4];
};

/* per peer statistics */
struct slsi_lls_peer_info {
	enum slsi_lls_peer_type type;         /* peer type (AP, TDLS, GO etc.)*/
	u8 peer_mac_address[6];           /* mac address*/
	u32 capabilities;                 /* peer WIFI_CAPABILITY_XXX*/
	struct slsi_lls_bssload_info bssload; /* STA count and CU (Not used) */
	u32 num_rate;                     /* number of rates*/
	struct slsi_lls_rate_stat rate_stats[]; /* per rate statistics, number of entries  = num_rate*/
};

/* Per access category statistics */
struct slsi_lls_wmm_ac_stat {
	enum slsi_lls_traffic_ac ac;      /* access category (VI, VO, BE, BK)*/
	u32 tx_mpdu;                  /* number of successfully transmitted unicast data pkts (ACK rcvd)*/
	u32 rx_mpdu;                  /* number of received unicast data packets*/
	u32 tx_mcast;                 /* number of successfully transmitted multicast data packets*/
	u32 rx_mcast;                 /* number of received multicast data packets*/
	u32 rx_ampdu;                 /* number of received unicast a-mpdus; support of this counter is optional*/
	u32 tx_ampdu;                 /* number of transmitted unicast a-mpdus; support of this counter is optional*/
	u32 mpdu_lost;                /* number of data pkt losses (no ACK)*/
	u32 retries;                  /* total number of data pkt retries*/
	u32 retries_short;            /* number of short data pkt retries*/
	u32 retries_long;             /* number of long data pkt retries*/
	u32 contention_time_min;      /* data pkt min contention time (usecs)*/
	u32 contention_time_max;      /* data pkt max contention time (usecs)*/
	u32 contention_time_avg;      /* data pkt avg contention time (usecs)*/
	u32 contention_num_samples;   /* num of data pkts used for contention statistics*/
};

struct slsi_rx_data_cnt_details {
	int rx_unicast_cnt;     /*Total rx unicast packet which woke up host */
	int rx_multicast_cnt;   /*Total rx multicast packet which woke up host */
	int rx_broadcast_cnt;   /*Total rx broadcast packet which woke up host */
};

struct slsi_rx_wake_pkt_type_classification {
	int icmp_pkt;   /*wake icmp packet count */
	int icmp6_pkt;  /*wake icmp6 packet count */
	int icmp6_ra;   /*wake icmp6 RA packet count */
	int icmp6_na;   /*wake icmp6 NA packet count */
	int icmp6_ns;   /*wake icmp6 NS packet count */
};

struct slsi_rx_multicast_cnt {
	int ipv4_rx_multicast_addr_cnt; /*Rx wake packet was ipv4 multicast */
	int ipv6_rx_multicast_addr_cnt; /*Rx wake packet was ipv6 multicast */
	int other_rx_multicast_addr_cnt;/*Rx wake packet was non-ipv4 and non-ipv6*/
};

/*
 * Structure holding all the driver/firmware wake count reasons.
 *
 * Buffers for the array fields (cmd_event_wake_cnt/driver_fw_local_wake_cnt)
 * are allocated and freed by the framework. The size of each allocated
 * array is indicated by the corresponding |_cnt| field. HAL needs to fill in
 * the corresponding |_used| field to indicate the number of elements used in
 * the array.
 */
struct slsi_wlan_driver_wake_reason_cnt {
	int total_cmd_event_wake;    /* Total count of cmd event wakes */
	int *cmd_event_wake_cnt;     /* Individual wake count array, each index a reason */
	int cmd_event_wake_cnt_sz;   /* Max number of cmd event wake reasons */
	int cmd_event_wake_cnt_used; /* Number of cmd event wake reasons specific to the driver */

	int total_driver_fw_local_wake;    /* Total count of drive/fw wakes, for local reasons */
	int *driver_fw_local_wake_cnt;     /* Individual wake count array, each index a reason */
	int driver_fw_local_wake_cnt_sz;   /* Max number of local driver/fw wake reasons */
	int driver_fw_local_wake_cnt_used; /* Number of local driver/fw wake reasons specific to the driver */

	int total_rx_data_wake;     /* total data rx packets, that woke up host */
	struct slsi_rx_data_cnt_details rx_wake_details;
	struct slsi_rx_wake_pkt_type_classification rx_wake_pkt_classification_info;
	struct slsi_rx_multicast_cnt rx_multicast_wake_pkt_info;
};

/* interface statistics */
struct slsi_lls_iface_stat {
	void *iface;                          /* wifi interface*/
	struct slsi_lls_interface_link_layer_info info;  /* current state of the interface*/
	u32 beacon_rx;                        /* access point beacon received count from connected AP*/
	u64 average_tsf_offset;               /* average beacon offset encountered (beacon_TSF - TBTT)*/
	u32 leaky_ap_detected;                /* indicate that this AP typically leaks packets beyond the driver guard time.*/
	u32 leaky_ap_avg_num_frames_leaked;   /* average number of frame leaked by AP after frame with PM bit set was ACK'ed by AP*/
	u32 leaky_ap_guard_time;
	u32 mgmt_rx;                          /* access point mgmt frames received count from connected AP (including Beacon)*/
	u32 mgmt_action_rx;                   /* action frames received count*/
	u32 mgmt_action_tx;                   /* action frames transmit count*/
	int rssi_mgmt;                        /* access Point Beacon and Management frames RSSI (averaged)*/
	int rssi_data;                        /* access Point Data Frames RSSI (averaged) from connected AP*/
	int rssi_ack;                         /* access Point ACK RSSI (averaged) from connected AP*/
	struct slsi_lls_wmm_ac_stat ac[SLSI_LLS_AC_MAX];     /* per ac data packet statistics*/
	u32 num_peers;                        /* number of peers*/
	struct slsi_lls_peer_info peer_info[]; /* per peer statistics*/
};

enum slsi_wifi_hal_api_return_types {
	WIFI_HAL_SUCCESS = 0,
	WIFI_HAL_ERROR_NONE = 0,
	WIFI_HAL_ERROR_UNKNOWN = -1,
	WIFI_HAL_ERROR_UNINITIALIZED = -2,
	WIFI_HAL_ERROR_NOT_SUPPORTED = -3,
	WIFI_HAL_ERROR_NOT_AVAILABLE = -4,
	WIFI_HAL_ERROR_INVALID_ARGS = -5,
	WIFI_HAL_ERROR_INVALID_REQUEST_ID = -6,
	WIFI_HAL_ERROR_TIMED_OUT = -7,
	WIFI_HAL_ERROR_TOO_MANY_REQUESTS = -8,
	WIFI_HAL_ERROR_OUT_OF_MEMORY = -9
};

struct slsi_rtt_capabilities {
	u8 rtt_one_sided_supported;  /* if 1-sided rtt data collection is supported */
	u8 rtt_ftm_supported;        /* if ftm rtt data collection is supported */
	u8 lci_support;              /* if initiator supports LCI request. Applies to 2-sided RTT */
	u8 lcr_support;              /* if initiator supports LCR request. Applies to 2-sided RTT */
	u8 preamble_support;         /* bit mask indicates what preamble is supported by initiator */
	u8 bw_support;               /* bit mask indicates what BW is supported by initiator */
	u8 responder_supported;      /* if 11mc responder mode is supported */
	u8 mc_version;               /* draft 11mc spec version supported by chip. For instance,
				      *version 4.0 should be 40 and version 4.3 should be 43 etc.
				      */
};

/*Data Structure to store rtt_id and list of mac_addresses which is being processed.*/
struct slsi_rtt_id_params {
	u8  fapi_req_id;
	u32 hal_request_id;
	u8 peer_type;
	u32 peer_count;
	u8 peers[];
};

/* RTT configuration */
struct slsi_rtt_config {
	u8 peer_addr[ETH_ALEN];                 /* peer device mac address */
	u8 rtt_peer;                  /* optional - peer device hint (STA, P2P, AP) */
	u8 rtt_type;            /* 1-sided or 2-sided RTT */
	u16 channel_freq;     /* Required for STA-AP mode, optional for P2P, NBD etc. */
	u16 channel_info;
	u8 burst_period;         /* Time interval between bursts (units: 100 ms). */
					/* Applies to 1-sided and 2-sided RTT multi-burst requests.
					 *Range: 0-31, 0: no preference by initiator (2-sided RTT)
					 */
	u8 num_burst;    /* Total number of RTT bursts to be executed. It will be
			  *specified in the same way as the parameter "Number of
			  *Burst Exponent" found in the FTM frame format. It
			  *applies to both: 1-sided RTT and 2-sided RTT. Valid
			  *values are 0 to 15 as defined in 802.11mc std
			  *0 means single shot
			  *The implication of this parameter on the maximum
			  *number of RTT results is the following:
			  *for 1-sided RTT: max num of RTT results = (2^num_burst)*(num_frames_per_burst)
			  *for 2-sided RTT: max num of RTT results = (2^num_burst)*(num_frames_per_burst - 1)
			  */
	u8 num_frames_per_burst; /* num of frames per burst.
				  *Minimum value = 1, Maximum value = 31
				  *For 2-sided this equals the number of FTM frames
				  *to be attempted in a single burst. This also
				  *equals the number of FTM frames that the
				  *initiator will request that the responder send
				  *in a single frame.
				  */
	u8 num_retries_per_ftmr; /* Maximum number of retries that the initiator can
				  *retry an FTMR frame.
				  *Minimum value = 0, Maximum value = 3
				  */
	u8 burst_duration;       /* Applies to 1-sided and 2-sided RTT. Valid values will
				  *be 2-11 and 15 as specified by the 802.11mc std for
				  *the FTM parameter burst duration. In a multi-burst
				  *request, if responder overrides with larger value,
				  *the initiator will return failure. In a single-burst
				  *request if responder overrides with larger value,
				  *the initiator will sent TMR_STOP to terminate RTT
				  *at the end of the burst_duration it requested.
				  */
	u16 preamble;    /* RTT preamble to be used in the RTT frames */
	u16 bw;                /* RTT BW to be used in the RTT frames */
	u16 LCI_request;              /* 1: request LCI, 0: do not request LCI */
	u16 LCR_request;              /* 1: request LCR, 0: do not request LCR */
};

#define MAX_24G_CHANNELS 14  /*Max number of 2.4G channels*/
#define MAX_5G_CHANNELS 25  /*Max number of 5G channels*/
#define MAX_CHAN_VALUE_ACS (MAX_24G_CHANNELS + MAX_5G_CHANNELS)
#define MAX_AP_THRESHOLD 10  /*Max AP threshold in ACS*/

struct slsi_acs_chan_info {
	u16 chan;
	u8 num_ap;
	u8 num_bss_load_ap;
	u8 total_chan_utilization;
	u8 avg_chan_utilization;
	int rssi_factor;
	int adj_rssi_factor;
};

struct slsi_acs_selected_channels {
	u8 pri_channel;
	u8 sec_channel;
	u8 vht_seg0_center_ch;
	u8 vht_seg1_center_ch;
	u16 ch_width;
	enum slsi_acs_hw_mode hw_mode;
};

struct slsi_acs_request {
	struct slsi_acs_chan_info acs_chan_info[MAX_CHAN_VALUE_ACS];
	u8 hw_mode;
	u16 ch_width;
	u8 ch_list_len;
};

struct slsi_logging_ap_info {
	short score;
	short rssi;
	u8 mac[ETH_ALEN];
	u32 tp_score;
	u32 ch_util;
};

void slsi_nl80211_vendor_init(struct slsi_dev *sdev);
void slsi_nl80211_vendor_deinit(struct slsi_dev *sdev);
u8 slsi_gscan_get_scan_policy(enum wifi_band band);
void slsi_gscan_handle_scan_result(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, u16 scan_id, bool scan_done);
void slsi_gscan_hash_remove(struct slsi_dev *sdev, u8 *mac);
void slsi_rx_significant_change_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_gscan_alloc_buckets(struct slsi_dev *sdev, struct slsi_gscan *gscan, int num_buckets);
int slsi_vendor_event(struct slsi_dev *sdev, int event_id, const void *data, int len);
int slsi_mib_get_gscan_cap(struct slsi_dev *sdev, struct slsi_nl_gscan_capabilities *cap);
void slsi_rx_rssi_report_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_mib_get_apf_cap(struct slsi_dev *sdev, struct net_device *dev);
int slsi_mib_get_rtt_cap(struct slsi_dev *sdev, struct net_device *dev, struct slsi_rtt_capabilities *cap);
void slsi_rx_range_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_rx_range_done_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_tx_rate_calc(struct sk_buff *nl_skb, u16 fw_rate, int res, bool tx_rate);
void slsi_rx_event_log_indication(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
#ifdef CONFIG_SCSC_WLAN_DEBUG
char *slsi_print_event_name(int event_id);
#endif

static inline bool slsi_is_gscan_id(u16 scan_id)
{
	if ((scan_id >= SLSI_GSCAN_SCAN_ID_START) && (scan_id <= SLSI_GSCAN_SCAN_ID_END))
		return true;

	return false;
}

static inline enum slsi_lls_traffic_ac slsi_fapi_to_android_traffic_q(enum slsi_traffic_q fapi_q)
{
	switch (fapi_q) {
	case SLSI_TRAFFIC_Q_BE:
		return SLSI_LLS_AC_BE;
	case SLSI_TRAFFIC_Q_BK:
		return SLSI_LLS_AC_BK;
	case SLSI_TRAFFIC_Q_VI:
		return SLSI_LLS_AC_VI;
	case SLSI_TRAFFIC_Q_VO:
		return SLSI_LLS_AC_VO;
	default:
		return SLSI_LLS_AC_MAX;
	}
}

#endif
