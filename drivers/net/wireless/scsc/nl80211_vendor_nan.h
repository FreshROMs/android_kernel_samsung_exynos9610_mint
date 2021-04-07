/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __SLSI_NL80211_VENDOR_NAN_H_
#define __SLSI_NL80211_VENDOR_NAN_H_

#define SLSI_NAN_VIF_TYPE_NDP                       0x00FF

#define SLSI_NAN_TLV_TAG_CONFIGURATION             0x0101
#define SLSI_NAN_TLV_TAG_2G4_BAND_SPECIFIC_CONFIG  0x0102
#define SLSI_NAN_TLV_TAG_5G_BAND_SPECIFIC_CONFIG   0x0103
#define SLSI_NAN_TLV_TAG_SUBSCRIBE_SPECIFIC        0x0104
#define SLSI_NAN_TLV_TAG_DISCOVERY_COMMON_SPECIFIC 0x0105
#define SLSI_NAN_TLV_TAG_SERVICE_NAME              0x0106
#define SLSI_NAN_TLV_TAG_SERVICE_SPECIFIC_INFO     0x0107
#define SLSI_NAN_TLV_TAG_EXT_SERVICE_SPECIFIC_INFO 0x0108
#define SLSI_NAN_TLV_TAG_RX_MATCH_FILTER           0x0109
#define SLSI_NAN_TLV_TAG_TX_MATCH_FILTER           0x010a
#define SLSI_NAN_TLV_TAG_MATCH_FILTER              0x010b
#define SLSI_NAN_TLV_TAG_INTERFACE_ADDRESS_SET     0x010c
#define SLSI_NAN_TLV_TAG_MATCH_IND                 0x010d
#define SLSI_NAN_TLV_TAG_DATA_PATH_SECURITY        0x010e
#define SLSI_NAN_TLV_TAG_APP_INFO                  0x010f
#define SLSI_NAN_TLV_TAG_RANGING                   0x0110
#define SLSI_NAN_TLV_TAG_CONFIG_SUPPLEMENTAL       0x0111
#define SLSI_NAN_TLV_WFA_IPV6_LOCAL_LINK           0x0000
#define SLSI_NAN_TLV_WFA_SERVICE_INFO              0x0001
#define SLSI_NAN_TLV_NAN_RTT_CONFIG                0x0112
#define SLSI_NAN_TLV_NAN_RTT_CONFIG_LEN            0x0010
#define SLSI_NAN_TLV_NAN_RTT_RESULT                0x0113
#define SLSI_NAN_TLV_NAN_RTT_RESULT_LEN            0x0023

#define SLSI_NAN_MAX_SERVICE_ID 16
#define SLSI_NAN_MAX_HOST_FOLLOWUP_REQ 20
#define SLSI_NAN_MAX_NDP_INSTANCES 8
#define SLSI_NAN_DATA_IFINDEX_START 5
#define SLSI_NAN_CLUSTER_MERGE_ENABLE_MASK 0xC0000000
#define SLSI_NAN_CLUSTER_MERGE_DISABLE_MASK 0x80000000
#define SLSI_NAN_MAC_RANDOM_INTERVAL_MASK 0x3fffffff

enum SLSI_NAN_REPLY_ATTRIBUTES {
	NAN_REPLY_ATTR_STATUS_TYPE,
	NAN_REPLY_ATTR_VALUE,
	NAN_REPLY_ATTR_RESPONSE_TYPE,
	NAN_REPLY_ATTR_PUBLISH_SUBSCRIBE_TYPE,
	NAN_REPLY_ATTR_CAP_MAX_CONCURRENT_CLUSTER,
	NAN_REPLY_ATTR_CAP_MAX_PUBLISHES,
	NAN_REPLY_ATTR_CAP_MAX_SUBSCRIBES,
	NAN_REPLY_ATTR_CAP_MAX_SERVICE_NAME_LEN,
	NAN_REPLY_ATTR_CAP_MAX_MATCH_FILTER_LEN,
	NAN_REPLY_ATTR_CAP_MAX_TOTAL_MATCH_FILTER_LEN,
	NAN_REPLY_ATTR_CAP_MAX_SERVICE_SPECIFIC_INFO_LEN,
	NAN_REPLY_ATTR_CAP_MAX_VSA_DATA_LEN,
	NAN_REPLY_ATTR_CAP_MAX_MESH_DATA_LEN,
	NAN_REPLY_ATTR_CAP_MAX_NDI_INTERFACES,
	NAN_REPLY_ATTR_CAP_MAX_NDP_SESSIONS,
	NAN_REPLY_ATTR_CAP_MAX_APP_INFO_LEN,
	NAN_REPLY_ATTR_NDP_INSTANCE_ID,
	NAN_REPLY_ATTR_CAP_MAX_QUEUED_TRANSMIT_FOLLOWUP_MGS,
	NAN_REPLY_ATTR_CAP_MAX_NDP_SUPPORTED_BANDS,
	NAN_REPLY_ATTR_CAP_MAX_CIPHER_SUITES_SUPPORTED,
	NAN_REPLY_ATTR_CAP_MAX_SCID_LEN,
	NAN_REPLY_ATTR_CAP_NDP_SECURITY_SUPPORTED,
	NAN_REPLY_ATTR_CAP_MAX_SDEA_SERVICE_SPECIFIC_INFO_LEN,
	NAN_REPLY_ATTR_CAP_MAX_SUBSCRIBE_ADDRESS,
	NAN_REPLY_ATTR_CAP_NDPE_ATTR_SUPPORTED,
	NAN_REPLY_ATTR_HAL_TRANSACTION_ID
};

enum SLSI_NAN_REQ_ATTRIBUTES {
	NAN_REQ_ATTR_MASTER_PREF = 1,
	NAN_REQ_ATTR_CLUSTER_LOW,
	NAN_REQ_ATTR_CLUSTER_HIGH,
	NAN_REQ_ATTR_HOP_COUNT_LIMIT_VAL,
	NAN_REQ_ATTR_SID_BEACON_VAL,
	NAN_REQ_ATTR_SUPPORT_2G4_VAL,
	NAN_REQ_ATTR_SUPPORT_5G_VAL,
	NAN_REQ_ATTR_RSSI_CLOSE_2G4_VAL,
	NAN_REQ_ATTR_RSSI_MIDDLE_2G4_VAL,
	NAN_REQ_ATTR_RSSI_PROXIMITY_2G4_VAL,
	NAN_REQ_ATTR_BEACONS_2G4_VAL = 11,
	NAN_REQ_ATTR_SDF_2G4_VAL,
	NAN_REQ_ATTR_CHANNEL_2G4_MHZ_VAL,
	NAN_REQ_ATTR_RSSI_PROXIMITY_VAL,
	NAN_REQ_ATTR_RSSI_CLOSE_5G_VAL,
	NAN_REQ_ATTR_RSSI_CLOSE_PROXIMITY_5G_VAL,
	NAN_REQ_ATTR_RSSI_MIDDLE_5G_VAL,
	NAN_REQ_ATTR_RSSI_PROXIMITY_5G_VAL,
	NAN_REQ_ATTR_BEACON_5G_VAL,
	NAN_REQ_ATTR_SDF_5G_VAL,
	NAN_REQ_ATTR_CHANNEL_5G_MHZ_VAL = 21,
	NAN_REQ_ATTR_RSSI_WINDOW_SIZE_VAL,
	NAN_REQ_ATTR_OUI_VAL,
	NAN_REQ_ATTR_MAC_ADDR_VAL,
	NAN_REQ_ATTR_CLUSTER_VAL,
	NAN_REQ_ATTR_SOCIAL_CH_SCAN_DWELL_TIME,
	NAN_REQ_ATTR_SOCIAL_CH_SCAN_PERIOD,
	NAN_REQ_ATTR_RANDOM_FACTOR_FORCE_VAL,
	NAN_REQ_ATTR_HOP_COUNT_FORCE_VAL,
	NAN_REQ_ATTR_CONN_CAPABILITY_PAYLOAD_TX,
	NAN_REQ_ATTR_CONN_CAPABILITY_IBSS = 31,
	NAN_REQ_ATTR_CONN_CAPABILITY_WFD,
	NAN_REQ_ATTR_CONN_CAPABILITY_WFDS,
	NAN_REQ_ATTR_CONN_CAPABILITY_TDLS,
	NAN_REQ_ATTR_CONN_CAPABILITY_MESH,
	NAN_REQ_ATTR_CONN_CAPABILITY_WLAN_INFRA,
	NAN_REQ_ATTR_DISCOVERY_ATTR_NUM_ENTRIES,
	NAN_REQ_ATTR_DISCOVERY_ATTR_VAL,
	NAN_REQ_ATTR_CONN_TYPE,
	NAN_REQ_ATTR_NAN_ROLE,
	NAN_REQ_ATTR_TRANSMIT_FREQ = 41,
	NAN_REQ_ATTR_AVAILABILITY_DURATION,
	NAN_REQ_ATTR_AVAILABILITY_INTERVAL,
	NAN_REQ_ATTR_MESH_ID_LEN,
	NAN_REQ_ATTR_MESH_ID,
	NAN_REQ_ATTR_INFRASTRUCTURE_SSID_LEN,
	NAN_REQ_ATTR_INFRASTRUCTURE_SSID,
	NAN_REQ_ATTR_FURTHER_AVAIL_NUM_ENTRIES,
	NAN_REQ_ATTR_FURTHER_AVAIL_VAL,
	NAN_REQ_ATTR_FURTHER_AVAIL_ENTRY_CTRL,
	NAN_REQ_ATTR_FURTHER_AVAIL_CHAN_CLASS = 51,
	NAN_REQ_ATTR_FURTHER_AVAIL_CHAN,
	NAN_REQ_ATTR_FURTHER_AVAIL_CHAN_MAPID,
	NAN_REQ_ATTR_FURTHER_AVAIL_INTERVAL_BITMAP,
	NAN_REQ_ATTR_PUBLISH_ID,
	NAN_REQ_ATTR_PUBLISH_TTL,
	NAN_REQ_ATTR_PUBLISH_PERIOD,
	NAN_REQ_ATTR_PUBLISH_TYPE,
	NAN_REQ_ATTR_PUBLISH_TX_TYPE,
	NAN_REQ_ATTR_PUBLISH_COUNT,
	NAN_REQ_ATTR_PUBLISH_SERVICE_NAME_LEN = 61,
	NAN_REQ_ATTR_PUBLISH_SERVICE_NAME,
	NAN_REQ_ATTR_PUBLISH_MATCH_ALGO,
	NAN_REQ_ATTR_PUBLISH_SERVICE_INFO_LEN,
	NAN_REQ_ATTR_PUBLISH_SERVICE_INFO,
	NAN_REQ_ATTR_PUBLISH_RX_MATCH_FILTER_LEN,
	NAN_REQ_ATTR_PUBLISH_RX_MATCH_FILTER,
	NAN_REQ_ATTR_PUBLISH_TX_MATCH_FILTER_LEN,
	NAN_REQ_ATTR_PUBLISH_TX_MATCH_FILTER,
	NAN_REQ_ATTR_PUBLISH_RSSI_THRESHOLD_FLAG,
	NAN_REQ_ATTR_PUBLISH_CONN_MAP = 71,
	NAN_REQ_ATTR_PUBLISH_RECV_IND_CFG,
	NAN_REQ_ATTR_SUBSCRIBE_ID,
	NAN_REQ_ATTR_SUBSCRIBE_TTL,
	NAN_REQ_ATTR_SUBSCRIBE_PERIOD,
	NAN_REQ_ATTR_SUBSCRIBE_TYPE,
	NAN_REQ_ATTR_SUBSCRIBE_RESP_FILTER_TYPE,
	NAN_REQ_ATTR_SUBSCRIBE_RESP_INCLUDE,
	NAN_REQ_ATTR_SUBSCRIBE_USE_RESP_FILTER,
	NAN_REQ_ATTR_SUBSCRIBE_SSI_REQUIRED,
	NAN_REQ_ATTR_SUBSCRIBE_MATCH_INDICATOR = 81,
	NAN_REQ_ATTR_SUBSCRIBE_COUNT,
	NAN_REQ_ATTR_SUBSCRIBE_SERVICE_NAME_LEN,
	NAN_REQ_ATTR_SUBSCRIBE_SERVICE_NAME,
	NAN_REQ_ATTR_SUBSCRIBE_SERVICE_INFO_LEN,
	NAN_REQ_ATTR_SUBSCRIBE_SERVICE_INFO,
	NAN_REQ_ATTR_SUBSCRIBE_RX_MATCH_FILTER_LEN,
	NAN_REQ_ATTR_SUBSCRIBE_RX_MATCH_FILTER,
	NAN_REQ_ATTR_SUBSCRIBE_TX_MATCH_FILTER_LEN,
	NAN_REQ_ATTR_SUBSCRIBE_TX_MATCH_FILTER,
	NAN_REQ_ATTR_SUBSCRIBE_RSSI_THRESHOLD_FLAG = 91,
	NAN_REQ_ATTR_SUBSCRIBE_CONN_MAP,
	NAN_REQ_ATTR_SUBSCRIBE_NUM_INTF_ADDR_PRESENT,
	NAN_REQ_ATTR_SUBSCRIBE_INTF_ADDR,
	NAN_REQ_ATTR_SUBSCRIBE_RECV_IND_CFG,
	NAN_REQ_ATTR_FOLLOWUP_ID,
	NAN_REQ_ATTR_FOLLOWUP_REQUESTOR_ID,
	NAN_REQ_ATTR_FOLLOWUP_ADDR,
	NAN_REQ_ATTR_FOLLOWUP_PRIORITY,
	NAN_REQ_ATTR_FOLLOWUP_SERVICE_NAME_LEN,
	NAN_REQ_ATTR_FOLLOWUP_SERVICE_NAME = 101,
	NAN_REQ_ATTR_FOLLOWUP_TX_WINDOW,
	NAN_REQ_ATTR_FOLLOWUP_RECV_IND_CFG,
	NAN_REQ_ATTR_SUBSCRIBE_SID_BEACON_VAL,
	NAN_REQ_ATTR_DW_2G4_INTERVAL,
	NAN_REQ_ATTR_DW_5G_INTERVAL,
	NAN_REQ_ATTR_DISC_MAC_ADDR_RANDOM_INTERVAL,
	NAN_REQ_ATTR_PUBLISH_SDEA_LEN,
	NAN_REQ_ATTR_PUBLISH_SDEA,
	NAN_REQ_ATTR_RANGING_AUTO_RESPONSE,
	NAN_REQ_ATTR_SDEA_PARAM_NDP_TYPE = 111,
	NAN_REQ_ATTR_SDEA_PARAM_SECURITY_CFG,
	NAN_REQ_ATTR_SDEA_PARAM_RANGING_STATE,
	NAN_REQ_ATTR_SDEA_PARAM_RANGE_REPORT,
	NAN_REQ_ATTR_SDEA_PARAM_QOS_CFG,
	NAN_REQ_ATTR_RANGING_CFG_INTERVAL,
	NAN_REQ_ATTR_RANGING_CFG_INDICATION,
	NAN_REQ_ATTR_RANGING_CFG_INGRESS_MM,
	NAN_REQ_ATTR_RANGING_CFG_EGRESS_MM,
	NAN_REQ_ATTR_CIPHER_TYPE,
	NAN_REQ_ATTR_SCID_LEN = 121,
	NAN_REQ_ATTR_SCID,
	NAN_REQ_ATTR_SECURITY_KEY_TYPE,
	NAN_REQ_ATTR_SECURITY_PMK_LEN,
	NAN_REQ_ATTR_SECURITY_PMK,
	NAN_REQ_ATTR_SECURITY_PASSPHRASE_LEN,
	NAN_REQ_ATTR_SECURITY_PASSPHRASE,
	NAN_REQ_ATTR_RANGE_RESPONSE_CFG_PUBLISH_ID,
	NAN_REQ_ATTR_RANGE_RESPONSE_CFG_REQUESTOR_ID,
	NAN_REQ_ATTR_RANGE_RESPONSE_CFG_PEER_ADDR,
	NAN_REQ_ATTR_RANGE_RESPONSE_CFG_RANGING_RESPONSE = 131,
	NAN_REQ_ATTR_REQ_INSTANCE_ID,
	NAN_REQ_ATTR_NDP_INSTANCE_ID,
	NAN_REQ_ATTR_CHAN_REQ_TYPE,
	NAN_REQ_ATTR_CHAN,
	NAN_REQ_ATTR_DATA_INTERFACE_NAME_LEN,
	NAN_REQ_ATTR_DATA_INTERFACE_NAME,
	NAN_REQ_ATTR_APP_INFO_LEN,
	NAN_REQ_ATTR_APP_INFO,
	NAN_REQ_ATTR_SERVICE_NAME_LEN,
	NAN_REQ_ATTR_SERVICE_NAME = 141,
	NAN_REQ_ATTR_NDP_RESPONSE_CODE,
	NAN_REQ_ATTR_USE_NDPE_ATTR,
	NAN_REQ_ATTR_HAL_TRANSACTION_ID,
	NAN_REQ_ATTR_CONFIG_DISC_MAC_ADDR_RANDOM,
	NAN_REQ_ATTR_DISCOVERY_BEACON_INT,
	NAN_REQ_ATTR_NSS,
	NAN_REQ_ATTR_ENABLE_RANGING,
	NAN_REQ_ATTR_DW_EARLY_TERMINATION
};

enum SLSI_NAN_RESP_ATTRIBUTES {
	NAN_RESP_ATTR_MAX_CONCURRENT_NAN_CLUSTERS,
	NAN_RESP_ATTR_MAX_PUBLISHES,
	NAN_RESP_ATTR_MAX_SUBSCRIBES,
	NAN_RESP_ATTR_MAX_SERVICE_NAME_LEN,
	NAN_RESP_ATTR_MAX_MATCH_FILTER_LEN,
	NAN_RESP_ATTR_MAX_TOTAL_MATCH_FILTER_LEN,
	NAN_RESP_ATTR_MAX_SERVICE_SPECIFIC_INFO_LEN,
	NAN_RESP_ATTR_MAX_VSA_DATA_LEN,
	NAN_RESP_ATTR_MAX_MESH_DATA_LEN,
	NAN_RESP_ATTR_MAX_NDI_INTERFACES,
	NAN_RESP_ATTR_MAX_NDP_SESSIONS,
	NAN_RESP_ATTR_MAX_APP_INFO_LEN,
	NAN_RESP_ATTR_SUBSCRIBE_ID,
	NAN_RESP_ATTR_PUBLISH_ID
};

enum SLSI_NAN_EVT_ATTRIBUTES {
	NAN_EVT_ATTR_MATCH_PUBLISH_SUBSCRIBE_ID,
	NAN_EVT_ATTR_MATCH_REQUESTOR_INSTANCE_ID,
	NAN_EVT_ATTR_MATCH_ADDR,
	NAN_EVT_ATTR_MATCH_SERVICE_SPECIFIC_INFO_LEN,
	NAN_EVT_ATTR_MATCH_SERVICE_SPECIFIC_INFO,
	NAN_EVT_ATTR_MATCH_SDF_MATCH_FILTER_LEN,
	NAN_EVT_ATTR_MATCH_SDF_MATCH_FILTER,
	NAN_EVT_ATTR_MATCH_MATCH_OCCURRED_FLAG,
	NAN_EVT_ATTR_MATCH_OUT_OF_RESOURCE_FLAG,
	NAN_EVT_ATTR_MATCH_RSSI_VALUE,
	NAN_EVT_ATTR_MATCH_CONN_CAPABILITY_IS_WFD_SUPPORTED = 10,
	NAN_EVT_ATTR_MATCH_CONN_CAPABILITY_IS_WFDS_SUPPORTED,
	NAN_EVT_ATTR_MATCH_CONN_CAPABILITY_IS_TDLS_SUPPORTED,
	NAN_EVT_ATTR_MATCH_CONN_CAPABILITY_IS_IBSS_SUPPORTED,
	NAN_EVT_ATTR_MATCH_CONN_CAPABILITY_IS_MESH_SUPPORTED,
	NAN_EVT_ATTR_MATCH_CONN_CAPABILITY_WLAN_INFRA_FIELD,
	NAN_EVT_ATTR_MATCH_NUM_RX_DISCOVERY_ATTR,
	NAN_EVT_ATTR_MATCH_RX_DISCOVERY_ATTR,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_TYPE,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_ROLE,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_DURATION = 20,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_AVAIL_INTERVAL_BITMAP,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_MAPID,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_ADDR,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_MESH_ID_LEN,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_MESH_ID,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_INFRASTRUCTURE_SSID_LEN,
	NAN_EVT_ATTR_MATCH_DISC_ATTR_INFRASTRUCTURE_SSID_VAL,
	NAN_EVT_ATTR_MATCH_NUM_CHANS,
	NAN_EVT_ATTR_MATCH_FAMCHAN,
	NAN_EVT_ATTR_MATCH_FAM_ENTRY_CONTROL = 30,
	NAN_EVT_ATTR_MATCH_FAM_CLASS_VAL,
	NAN_EVT_ATTR_MATCH_FAM_CHANNEL,
	NAN_EVT_ATTR_MATCH_FAM_MAPID,
	NAN_EVT_ATTR_MATCH_FAM_AVAIL_INTERVAL_BITMAP,
	NAN_EVT_ATTR_MATCH_CLUSTER_ATTRIBUTE_LEN,
	NAN_EVT_ATTR_MATCH_CLUSTER_ATTRIBUTE,
	NAN_EVT_ATTR_PUBLISH_ID,
	NAN_EVT_ATTR_PUBLISH_REASON,
	NAN_EVT_ATTR_SUBSCRIBE_ID,
	NAN_EVT_ATTR_SUBSCRIBE_REASON = 40,
	NAN_EVT_ATTR_DISABLED_REASON,
	NAN_EVT_ATTR_FOLLOWUP_PUBLISH_SUBSCRIBE_ID,
	NAN_EVT_ATTR_FOLLOWUP_REQUESTOR_INSTANCE_ID,
	NAN_EVT_ATTR_FOLLOWUP_ADDR,
	NAN_EVT_ATTR_FOLLOWUP_DW_OR_FAW,
	NAN_EVT_ATTR_FOLLOWUP_SERVICE_SPECIFIC_INFO_LEN,
	NAN_EVT_ATTR_FOLLOWUP_SERVICE_SPECIFIC_INFO,
	NAN_EVT_ATTR_DISCOVERY_ENGINE_EVT_TYPE,
	NAN_EVT_ATTR_DISCOVERY_ENGINE_MAC_ADDR,
	NAN_EVT_ATTR_DISCOVERY_ENGINE_CLUSTER = 50,
	NAN_EVT_ATTR_SDEA,
	NAN_EVT_ATTR_SDEA_LEN,
	NAN_EVT_ATTR_SCID,
	NAN_EVT_ATTR_SCID_LEN,
	NAN_EVT_ATTR_SDEA_PARAM_CONFIG_NAN_DATA_PATH,
	NAN_EVT_ATTR_SDEA_PARAM_NDP_TYPE,
	NAN_EVT_ATTR_SDEA_PARAM_SECURITY_CONFIG,
	NAN_EVT_ATTR_SDEA_PARAM_RANGE_STATE,
	NAN_EVT_ATTR_SDEA_PARAM_RANGE_REPORT,
	NAN_EVT_ATTR_SDEA_PARAM_QOS_CFG = 60,
	NAN_EVT_ATTR_RANGE_MEASUREMENT_MM,
	NAN_EVT_ATTR_RANGEING_EVENT_TYPE,
	NAN_EVT_ATTR_SECURITY_CIPHER_TYPE,
	NAN_EVT_ATTR_STATUS,
	NAN_EVT_ATTR_SERVICE_INSTANCE_ID,
	NAN_EVT_ATTR_NDP_INSTANCE_ID,
	NAN_EVT_ATTR_NDP_RSP_CODE,
	NAN_EVT_ATTR_STATUS_CODE,
	NAN_EVT_ATTR_CHANNEL_INFO,
	NAN_EVT_ATTR_APP_INFO_LEN = 70,
	NAN_EVT_ATTR_APP_INFO,
	NAN_EVT_ATTR_CHANNEL,
	NAN_EVT_ATTR_CHANNEL_BW,
	NAN_EVT_ATTR_CHANNEL_NSS,
	NAN_EVT_ATTR_HAL_TRANSACTION_ID
};

#define SLSI_FAPI_NAN_CONFIG_PARAM_SID_BEACON 0X0003
#define SLSI_FAPI_NAN_CONFIG_PARAM_2_4_RSSI_CLOSE 0X0004
#define SLSI_FAPI_NAN_CONFIG_PARAM_2_4_RSSI_MIDDLE 0X0005
#define SLSI_FAPI_NAN_CONFIG_PARAM_2_4_RSSI_PROXIMITY 0X0006
#define SLSI_FAPI_NAN_CONFIG_PARAM_BAND_USAGE 0X0007
#define SLSI_FAPI_NAN_CONFIG_PARAM_5_RSSI_CLOSE 0X0008
#define SLSI_FAPI_NAN_CONFIG_PARAM_5_RSSI_MIDDLE 0X0009
#define SLSI_FAPI_NAN_CONFIG_PARAM_5_RSSI_PROXIMITY 0X000A
#define SLSI_FAPI_NAN_CONFIG_PARAM_HOP_COUNT_LIMIT 0X000B
#define SLSI_FAPI_NAN_CONFIG_PARAM_RSSI_WINDOW_SIZE 0X000C
#define SLSI_FAPI_NAN_CONFIG_PARAM_SCAN_PARAMETER_2_4 0X000D
#define SLSI_FAPI_NAN_CONFIG_PARAM_SCAN_PARAMETER_5 0X000E
#define SLSI_FAPI_NAN_CONFIG_PARAM_MASTER_PREFERENCE 0X000F
#define SLSI_FAPI_NAN_CONFIG_PARAM_CONNECTION_CAPAB 0X0010
#define SLSI_FAPI_NAN_CONFIG_PARAM_POST_DISCOVER_PARAM 0X0011
#define SLSI_FAPI_NAN_CONFIG_PARAM_FURTHER_AVAIL_CHANNEL_MAP 0X0012
#define SLSI_FAPI_NAN_CONFIG_PARAM_ADDR_RANDOM_INTERVAL 0X0013
#define SLSI_FAPI_NAN_SERVICE_NAME 0X0020
#define SLSI_FAPI_NAN_SERVICE_SPECIFIC_INFO 0X0021
#define SLSI_FAPI_NAN_RX_MATCH_FILTER 0X0022
#define SLSI_FAPI_NAN_TX_MATCH_FILTER 0X0023
#define SLSI_FAPI_NAN_SDF_MATCH_FILTER 0X0024
#define SLSI_FAPI_NAN_CLUSTER_ATTRIBUTE 0X0025
#define SLSI_FAPI_NAN_INTERFACE_ADDRESS_SET 0X0026
#define SLSI_FAPI_NAN_SDEA 0X0027

#define SLSI_HAL_NAN_MAX_SOCIAL_CHANNELS 3
#define SLSI_HAL_NAN_MAX_SERVICE_NAME_LEN 255
#define SLSI_HAL_NAN_MAX_SERVICE_SPECIFIC_INFO_LEN 1024
#define SLSI_HAL_NAN_MAX_MATCH_FILTER_LEN 255
#define SLSI_HAL_NAN_MAX_SUBSCRIBE_MAX_ADDRESS 42
#define SLSI_HAL_NAN_MAX_POSTDISCOVERY_LEN 5
#define SLSI_HAL_NAN_MAX_SDEA_SERVICE_SPEC_INFO_LEN 1024
#define SLSI_HAL_NAN_DP_MAX_APP_INFO_LEN 512

enum slsi_wifi_hal_nan_status_type {
	/* NAN Protocol Response Codes */
	SLSI_HAL_NAN_STATUS_SUCCESS = 0,
	/*  NAN Discovery Engine/Host driver failures */
	SLSI_HAL_NAN_STATUS_INTERNAL_FAILURE = 1,
	/*  NAN OTA failures */
	SLSI_HAL_NAN_STATUS_PROTOCOL_FAILURE = 2,
	/* if the publish/subscribe id is invalid */
	SLSI_HAL_NAN_STATUS_INVALID_PUBLISH_SUBSCRIBE_ID = 3,
	/* If we run out of resources allocated */
	SLSI_HAL_NAN_STATUS_NO_RESOURCE_AVAILABLE = 4,
	/* if invalid params are passed */
	SLSI_HAL_NAN_STATUS_INVALID_PARAM = 5,
	/*  if the requestor instance id is invalid */
	SLSI_HAL_NAN_STATUS_INVALID_REQUESTOR_INSTANCE_ID = 6,
	/*  if the ndp id is invalid */
	SLSI_HAL_NAN_STATUS_INVALID_NDP_ID = 7,
	/* if NAN is enabled when wifi is turned off */
	SLSI_HAL_NAN_STATUS_NAN_NOT_ALLOWED = 8,
	/* if over the air ack is not received */
	SLSI_HAL_NAN_STATUS_NO_OTA_ACK = 9,
	/* If NAN is already enabled and we are try to re-enable the same */
	SLSI_HAL_NAN_STATUS_ALREADY_ENABLED = 10,
	/* If followup message internal queue is full */
	SLSI_HAL_NAN_STATUS_FOLLOWUP_QUEUE_FULL = 11,
	/* Unsupported concurrency session enabled, NAN disabled notified */
	SLSI_HAL_NAN_STATUS_UNSUPPORTED_CONCURRENCY_NAN_DISABLED = 12
};

enum slsi_nan_status_type {
	/* NAN Protocol Response Codes */
	NAN_STATUS_SUCCESS = 0,
	NAN_STATUS_TIMEOUT = 1,
	NAN_STATUS_DE_FAILURE = 2,
	NAN_STATUS_INVALID_MSG_VERSION = 3,
	NAN_STATUS_INVALID_MSG_LEN = 4,
	NAN_STATUS_INVALID_MSG_ID = 5,
	NAN_STATUS_INVALID_HANDLE = 6,
	NAN_STATUS_NO_SPACE_AVAILABLE = 7,
	NAN_STATUS_INVALID_PUBLISH_TYPE = 8,
	NAN_STATUS_INVALID_TX_TYPE = 9,
	NAN_STATUS_INVALID_MATCH_ALGORITHM = 10,
	NAN_STATUS_DISABLE_IN_PROGRESS = 11,
	NAN_STATUS_INVALID_TLV_LEN = 12,
	NAN_STATUS_INVALID_TLV_TYPE = 13,
	NAN_STATUS_MISSING_TLV_TYPE = 14,
	NAN_STATUS_INVALID_TOTAL_TLVS_LEN = 15,
	NAN_STATUS_INVALID_MATCH_HANDLE = 16,
	NAN_STATUS_INVALID_TLV_VALUE = 17,
	NAN_STATUS_INVALID_TX_PRIORITY = 18,
	NAN_STATUS_INVALID_CONNECTION_MAP = 19,
	NAN_STATUS_INVALID_TCA_ID = 20,
	NAN_STATUS_INVALID_STATS_ID = 21,
	NAN_STATUS_NAN_NOT_ALLOWED = 22,
	NAN_STATUS_NO_OTA_ACK = 23,
	NAN_STATUS_TX_FAIL = 24,
	/* 25-4095 Reserved */
	/* NAN Configuration Response codes */
	NAN_STATUS_INVALID_RSSI_CLOSE_VALUE = 4096,
	NAN_STATUS_INVALID_RSSI_MIDDLE_VALUE = 4097,
	NAN_STATUS_INVALID_HOP_COUNT_LIMIT = 4098,
	NAN_STATUS_INVALID_MASTER_PREFERENCE_VALUE = 4099,
	NAN_STATUS_INVALID_LOW_CLUSTER_ID_VALUE = 4100,
	NAN_STATUS_INVALID_HIGH_CLUSTER_ID_VALUE = 4101,
	NAN_STATUS_INVALID_BACKGROUND_SCAN_PERIOD = 4102,
	NAN_STATUS_INVALID_RSSI_PROXIMITY_VALUE = 4103,
	NAN_STATUS_INVALID_SCAN_CHANNEL = 4104,
	NAN_STATUS_INVALID_POST_NAN_CONNECTIVITY_CAPABILITIES_BITMAP = 4105,
	NAN_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_NUMCHAN_VALUE = 4106,
	NAN_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_DURATION_VALUE = 4107,
	NAN_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_CLASS_VALUE = 4108,
	NAN_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_CHANNEL_VALUE = 4109,
	NAN_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_AVAILABILITY_INTERVAL_BITMAP_VALUE = 4110,
	NAN_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_MAP_ID = 4111,
	NAN_STATUS_INVALID_POST_NAN_DISCOVERY_CONN_TYPE_VALUE = 4112,
	NAN_STATUS_INVALID_POST_NAN_DISCOVERY_DEVICE_ROLE_VALUE = 4113,
	NAN_STATUS_INVALID_POST_NAN_DISCOVERY_DURATION_VALUE = 4114,
	NAN_STATUS_INVALID_POST_NAN_DISCOVERY_BITMAP_VALUE = 4115,
	NAN_STATUS_MISSING_FUTHER_AVAILABILITY_MAP = 4116,
	NAN_STATUS_INVALID_BAND_CONFIG_FLAGS = 4117,
	NAN_STATUS_INVALID_RANDOM_FACTOR_UPDATE_TIME_VALUE = 4118,
	NAN_STATUS_INVALID_ONGOING_SCAN_PERIOD = 4119,
	NAN_STATUS_INVALID_DW_INTERVAL_VALUE = 4120,
	NAN_STATUS_INVALID_DB_INTERVAL_VALUE = 4121,
	/* 4122-8191 RESERVED */
	NAN_TERMINATED_REASON_INVALID = 8192,
	NAN_TERMINATED_REASON_TIMEOUT = 8193,
	NAN_TERMINATED_REASON_USER_REQUEST = 8194,
	NAN_TERMINATED_REASON_FAILURE = 8195,
	NAN_TERMINATED_REASON_COUNT_REACHED = 8196,
	NAN_TERMINATED_REASON_DE_SHUTDOWN = 8197,
	NAN_TERMINATED_REASON_DISABLE_IN_PROGRESS = 8198,
	NAN_TERMINATED_REASON_POST_DISC_ATTR_EXPIRED = 8199,
	NAN_TERMINATED_REASON_POST_DISC_LEN_EXCEEDED = 8200,
	NAN_TERMINATED_REASON_FURTHER_AVAIL_MAP_EMPTY = 8201
};

enum slsi_nan_response_type {
	NAN_RESPONSE_ENABLED                = 0,
	NAN_RESPONSE_DISABLED               = 1,
	NAN_RESPONSE_PUBLISH                = 2,
	NAN_RESPONSE_PUBLISH_CANCEL         = 3,
	NAN_RESPONSE_TRANSMIT_FOLLOWUP      = 4,
	NAN_RESPONSE_SUBSCRIBE              = 5,
	NAN_RESPONSE_SUBSCRIBE_CANCEL       = 6,
	NAN_RESPONSE_STATS                  = 7,
	NAN_RESPONSE_CONFIG                 = 8,
	NAN_RESPONSE_TCA                    = 9,
	NAN_RESPONSE_ERROR                  = 10,
	NAN_RESPONSE_BEACON_SDF_PAYLOAD     = 11,
	NAN_RESPONSE_GET_CAPABILITIES       = 12,
	NAN_DP_INTERFACE_CREATE             = 13,
	NAN_DP_INTERFACE_DELETE             = 14,
	NAN_DP_INITIATOR_RESPONSE           = 15,
	NAN_DP_RESPONDER_RESPONSE           = 16,
	NAN_DP_END                          = 17
};

enum slsi_nan_disc_event_type {
	NAN_EVENT_ID_DISC_MAC_ADDR = 0,
	NAN_EVENT_ID_STARTED_CLUSTER,
	NAN_EVENT_ID_JOINED_CLUSTER
};

enum slsi_nan_data_path_response_code {
	NAN_DP_REQUEST_ACCEPT = 0,
	NAN_DP_REQUEST_REJECT
};

struct slsi_rtt_config;

struct slsi_hal_nan_social_channel_scan_params {
	u8 dwell_time[SLSI_HAL_NAN_MAX_SOCIAL_CHANNELS];
	u16 scan_period[SLSI_HAL_NAN_MAX_SOCIAL_CHANNELS];
};

struct slsi_hal_nan_connectivity_capability {
	u8 payload_transmit_flag;
	u8 is_wfd_supported;
	u8 is_wfds_supported;
	u8 is_tdls_supported;
	u8 is_ibss_supported;
	u8 is_mesh_supported;
	u8 wlan_infra_field;
};

struct slsi_hal_nan_post_discovery_param {
	u8 type; /* NanConnectionType */
	u8 role; /* NanDeviceRole */
	u8 transmit_freq;
	u8 duration; /* NanAvailDuration */
	u32 avail_interval_bitmap;
	u8 addr[ETH_ALEN];
	u16 mesh_id_len;
	u8 mesh_id[32];
	u16 infrastructure_ssid_len;
	u8 infrastructure_ssid_val[32];
};

struct slsi_hal_nan_further_availability_channel {
	/* struct slsi_hal_nan_further_availability_channel*/
	u8 entry_control;
	u8 class_val;
	u8 channel;
	u8 mapid;
	u32 avail_interval_bitmap;
};

struct slsi_hal_nan_further_availability_map {
	u8 numchans;
	struct slsi_hal_nan_further_availability_channel famchan[32];
};

struct slsi_hal_nan_receive_post_discovery {
	u8 type;
	u8 role;
	u8 duration;
	u32 avail_interval_bitmap;
	u8 mapid;
	u8 addr[ETH_ALEN];
	u16 mesh_id_len;
	u8 mesh_id[32];
	u16 infrastructure_ssid_len;
	u8 infrastructure_ssid_val[32];
};

struct slsi_nan_sdea_ctrl_params {
	u8 config_nan_data_path;
	u8 ndp_type;
	u8 security_cfg;
	u8 ranging_state;
	u8 range_report;
	u8 qos_cfg;
};

struct slsi_nan_ranging_cfg {
	u32 ranging_interval_msec;
	u32 config_ranging_indications;
	u32 distance_ingress_mm;
	u32 distance_egress_mm;
};

struct slsi_nan_range_response_cfg {
	u16 publish_id;
	u32 requestor_instance_id;
	u8 peer_addr[ETH_ALEN];
	u8 ranging_response;
};

#define SLSI_NAN_PMK_INFO_LEN 32
#define SLSI_NAN_SECURITY_MAX_PASSPHRASE_LEN 63
#define SLSI_NAN_MAX_SCID_BUF_LEN 1024
struct slsi_nan_security_pmk {
	u32 pmk_len;
	u8 pmk[SLSI_NAN_PMK_INFO_LEN];
};

struct slsi_nan_security_passphrase {
	u32 passphrase_len;
	u8 passphrase[SLSI_NAN_SECURITY_MAX_PASSPHRASE_LEN];
};

struct slsi_nan_security_key_info {
	u8 key_type;
	union {
		struct slsi_nan_security_pmk pmk_info;
		struct slsi_nan_security_passphrase passphrase_info;
	} body;
};

struct slsi_nan_security_info {
	u32 cipher_type;
	u32 scid_len;
	u8 scid[SLSI_NAN_MAX_SCID_BUF_LEN];
	struct slsi_nan_security_key_info key_info;
};

struct slsi_hal_nan_enable_req {
	u16 transaction_id;
	/* Mandatory parameters below */
	u8 master_pref;
	u16 cluster_low;
	u16 cluster_high;

	u8 config_support_5g;
	u8 support_5g_val;
	u8 config_sid_beacon;
	u8 sid_beacon_val;
	u8 config_2dot4g_rssi_close;
	u8 rssi_close_2dot4g_val;

	u8 config_2dot4g_rssi_middle;
	u8 rssi_middle_2dot4g_val;

	u8 config_2dot4g_rssi_proximity;
	u8 rssi_proximity_2dot4g_val;

	u8 config_hop_count_limit;
	u8 hop_count_limit_val;

	u8 config_2dot4g_support;
	u8 support_2dot4g_val;

	u8 config_2dot4g_beacons;
	u8 beacon_2dot4g_val;
	u8 config_2dot4g_sdf;
	u8 sdf_2dot4g_val;
	u8 config_5g_beacons;
	u8 beacon_5g_val;
	u8 config_5g_sdf;
	u8 sdf_5g_val;
	u8 config_5g_rssi_close;
	u8 rssi_close_5g_val;
	u8 config_5g_rssi_middle;
	u8 rssi_middle_5g_val;
	u8 config_5g_rssi_close_proximity;
	u8 rssi_close_proximity_5g_val;
	u8 config_rssi_window_size;
	u8 rssi_window_size_val;
	/* The 24 bit Organizationally Unique ID + the 8 bit Network Id. */
	u8 config_oui;
	u32 oui_val;
	u8 config_intf_addr;
	u8 intf_addr_val[ETH_ALEN];

	u8 config_cluster_attribute_val;
	u8 config_scan_params;
	struct slsi_hal_nan_social_channel_scan_params scan_params_val;
	u8 config_random_factor_force;
	u8 random_factor_force_val;
	u8 config_hop_count_force;
	u8 hop_count_force_val;

	/* channel frequency in MHz to enable Nan on */
	u8 config_24g_channel;
	u32 channel_24g_val;

	u8 config_5g_channel;
	int channel_5g_val;
	u8 config_subscribe_sid_beacon;
	u32 subscribe_sid_beacon_val;

	/*NanConfigDW config_dw*/
	u8 config_2dot4g_dw_band;
	u32 dw_2dot4g_interval_val;
	u8 config_5g_dw_band;
	u32 dw_5g_interval_val;
	u32 disc_mac_addr_rand_interval_sec;
	/*NAN Configuration Supplemental */
	u32 discovery_beacon_interval_ms;
	u32 nss_discovery;
	u32 enable_dw_early_termination;
	u32 enable_ranging;
};

struct slsi_hal_nan_publish_req {
	u16 transaction_id;
	/* id  0 means new publish, any other id is existing publish */
	u16 publish_id;
	/* how many seconds to run for. 0 means forever until canceled */
	u16 ttl;
	/* periodicity of OTA unsolicited publish.
	 * Specified in increments of 500 ms
	 */
	u16 period;
	u8 publish_type;/* 0= unsolicited, solicited = 1, 2= both */
	u8 tx_type; /* 0 = broadcast, 1= unicast  if solicited publish */
	/* number of OTA Publish, 0 means forever until canceled */
	u8 publish_count;
	u16 service_name_len;
	u8 service_name[SLSI_HAL_NAN_MAX_SERVICE_NAME_LEN];
	u8 publish_match_indicator;

	u16 service_specific_info_len;
	u8 service_specific_info[SLSI_HAL_NAN_MAX_SERVICE_SPECIFIC_INFO_LEN];

	u16 rx_match_filter_len;
	u8 rx_match_filter[SLSI_HAL_NAN_MAX_MATCH_FILTER_LEN];

	u16 tx_match_filter_len;
	u8 tx_match_filter[SLSI_HAL_NAN_MAX_MATCH_FILTER_LEN];

	u8 rssi_threshold_flag;

	/* 8-bit bitmap which allows the Host to associate this publish
	 *  with a particular Post-NAN Connectivity attribute
	 *  which has been sent down in a NanConfigureRequest/NanEnableRequest
	 *  message.  If the DE fails to find a configured Post-NAN
	 * connectivity attributes referenced by the bitmap,
	 *  the DE will return an error code to the Host.
	 *  If the Publish is configured to use a Post-NAN Connectivity
	 *  attribute and the Host does not refresh the Post-NAN Connectivity
	 *  attribute the Publish will be canceled and the Host will be sent
	 *  a PublishTerminatedIndication message.
	 */
	u8 connmap;
	/* Set/Enable corresponding bits to disable any
	 * indications that follow a publish.
	 * BIT0 - Disable publish termination indication.
	 * BIT1 - Disable match expired indication.
	 * BIT2 - Disable followUp indication received (OTA).
	 */
	u8 recv_indication_cfg;

	u8 service_responder_policy;
	struct slsi_nan_security_info sec_info;
	struct slsi_nan_sdea_ctrl_params sdea_params;
	struct slsi_nan_ranging_cfg ranging_cfg;
	u8 ranging_auto_response;
	struct slsi_nan_range_response_cfg range_response_cfg;

	u16 sdea_service_specific_info_len;
	u8 sdea_service_specific_info[SLSI_HAL_NAN_MAX_SDEA_SERVICE_SPEC_INFO_LEN];
};

struct slsi_hal_nan_subscribe_req {
	u16 transaction_id;
	/* id 0 means new subscribe, non zero is existing subscribe */
	u16 subscribe_id;
	/* how many seconds to run for. 0 means forever until canceled */
	u16 ttl;
	/* periodicity of OTA Active Subscribe. Units in increments
	 * of 500 ms , 0 = attempt every DW
	 */
	u16 period;

	/* Flag which specifies how the Subscribe request shall be processed. */
	u8 subscribe_type; /* 0 - PASSIVE , 1- ACTIVE */

	/* Flag which specifies on Active Subscribes how the Service Response
	 * Filter attribute is populated.
	 */
	u8 service_response_filter; /* 0 - Bloom Filter, 1 - MAC Addr */

	/* Flag which specifies how the Service Response Filter Include
	 * bit is populated.
	 * 0=Do not respond if in the Address Set, 1= Respond
	 */
	u8 service_response_include;

	/* Flag which specifies if the Service Response Filter
	 * should be used when creating Subscribes.
	 * 0=Do not send the Service Response Filter,1= send
	 */
	u8 use_service_response_filter;

	/* Flag which specifies if the Service Specific Info is needed in
	 *  the Publish message before creating the MatchIndication
	 */
	u8 ssi_required_for_match_indication; /* 0=Not needed, 1= Required */

	/* Field which specifies how matching indication to host is controlled.
	 *  0 - Match and Indicate Once
	 *  1 - Match and Indicate continuous
	 *  2 - Match and Indicate never. This means don't
	 *      indicate match to host.
	 *  3 - Reserved
	 */
	u8 subscribe_match_indicator;

	/* The number of Subscribe Matches which should occur
	 *  before the Subscribe request is automatically terminated.
	 */
	/* If this value is 0 this field is not used by DE.*/
	u8 subscribe_count;

	/* length of service name */
	/* UTF-8 encoded string identifying the service */
	u16 service_name_len;
	u8 service_name[SLSI_HAL_NAN_MAX_SERVICE_NAME_LEN];

	/* Sequence of values which further specify the published service
	 * beyond the service name
	 */
	u16 service_specific_info_len;
	u8 service_specific_info[SLSI_HAL_NAN_MAX_SERVICE_SPECIFIC_INFO_LEN];

	/* Ordered sequence of <length, value> pairs used to filter out
	 * received publish discovery messages.
	 *  This can be sent both for a Passive or an Active Subscribe
	 */
	u16 rx_match_filter_len;
	u8 rx_match_filter[SLSI_HAL_NAN_MAX_MATCH_FILTER_LEN];

	/* Ordered sequence of <length, value> pairs  included in the
	 *  Discovery Frame when an Active Subscribe is used.
	 */
	u16 tx_match_filter_len;
	u8 tx_match_filter[SLSI_HAL_NAN_MAX_MATCH_FILTER_LEN];
	u8 rssi_threshold_flag;

	u8 connmap;
	/* NAN Interface Address, conforming to the format as described in
	 *  8.2.4.3.2 of IEEE Std. 802.11-2012.
	 */
	u8 num_intf_addr_present;
	u8 intf_addr[SLSI_HAL_NAN_MAX_SUBSCRIBE_MAX_ADDRESS][ETH_ALEN];
	/* Set/Enable corresponding bits to disable
	 * indications that follow a subscribe.
	 * BIT0 - Disable subscribe termination indication.
	 * BIT1 - Disable match expired indication.
	 * BIT2 - Disable followUp indication received (OTA).
	 */
	u8 recv_indication_cfg;

	struct slsi_nan_security_info sec_info;
	struct slsi_nan_sdea_ctrl_params sdea_params;
	struct slsi_nan_ranging_cfg ranging_cfg;
	u8 ranging_auto_response;
	struct slsi_nan_range_response_cfg range_response_cfg;

	u16 sdea_service_specific_info_len;
	u8 sdea_service_specific_info[SLSI_HAL_NAN_MAX_SDEA_SERVICE_SPEC_INFO_LEN];
};

struct slsi_hal_nan_transmit_followup_req {
	u16 transaction_id;
	/* Publish or Subscribe Id of an earlier Publish/Subscribe */
	u16 publish_subscribe_id;

	/* This Id is the Requestor Instance that is passed as
	 *  part of earlier MatchInd/FollowupInd message.
	 */
	u32 requestor_instance_id;
	u8 addr[ETH_ALEN]; /* Unicast address */
	u8 priority; /* priority of the request 2=high */
	u8 dw_or_faw; /* 0= send in a DW, 1=send in FAW */

	/* Sequence of values which further specify the published service beyond
	 *  the service name.
	 */
	u16 service_specific_info_len;
	u8 service_specific_info[SLSI_HAL_NAN_MAX_SERVICE_SPECIFIC_INFO_LEN];
	/* Set/Enable corresponding bits to disable
	 * responses after followUp.
	 * BIT0 - Disable followUp response from FW.
	 */
	u8 recv_indication_cfg;

	u16 sdea_service_specific_info_len;
	u8 sdea_service_specific_info[SLSI_HAL_NAN_MAX_SDEA_SERVICE_SPEC_INFO_LEN];
};

struct slsi_hal_nan_config_req {
	u16 transaction_id;
	u8 config_sid_beacon;
	u8 sid_beacon;
	u8 config_rssi_proximity;
	u8 rssi_proximity;
	u8 config_master_pref;
	u8 master_pref;
	/* 1 byte value which defines the RSSI filter threshold.
	 *  Any Service Descriptors received above this value
	 *  that are configured for RSSI filtering will be dropped.
	 *  The rssi values should be specified without sign.
	 *  For eg: -70dBm should be specified as 70.
	 */
	u8 config_5g_rssi_close_proximity;
	u8 rssi_close_proximity_5g_val;
	u8 config_rssi_window_size;
	u16 rssi_window_size_val;
	/* If set to 1, the Discovery Engine will enclose the Cluster
	 *  Attribute only sent in Beacons in a Vendor Specific Attribute
	 *  and transmit in a Service Descriptor Frame.
	 */
	u8 config_cluster_attribute_val;
	u8 config_scan_params;
	struct slsi_hal_nan_social_channel_scan_params scan_params_val;
	/* 1 byte quantity which forces the Random Factor to a particular
	 * value for all transmitted Sync/Discovery beacons
	 */
	u8 config_random_factor_force;
	u8 random_factor_force_val;
	/* 1 byte quantity which forces the HC for all transmitted Sync and
	 *  Discovery Beacon NO matter the real HC being received over the
	 *  air.
	 */
	u8 config_hop_count_force;
	u8 hop_count_force_val;
	/* NAN Post Connectivity Capability */
	u8 config_conn_capability;
	struct slsi_hal_nan_connectivity_capability conn_capability_val;
	/* NAN Post Discover Capability */
	u8 num_config_discovery_attr;
	struct slsi_hal_nan_post_discovery_param discovery_attr_val[SLSI_HAL_NAN_MAX_POSTDISCOVERY_LEN];
	/* NAN Further availability Map */
	u8 config_fam;
	struct slsi_hal_nan_further_availability_map fam_val;

	int channel_5g_val;
	u8 config_subscribe_sid_beacon;
	u32 subscribe_sid_beacon_val;

	/*NanConfigDW config_dw*/
	u8 config_2dot4g_dw_band;
	u32 dw_2dot4g_interval_val;
	u8 config_5g_dw_band;
	u32 dw_5g_interval_val;
	u32 disc_mac_addr_rand_interval_sec;

	/* Values Added from enable Req*/
	u16 cluster_low;
	u16 cluster_high;

	u8 config_support_5g;
	u8 support_5g_val;

	u8 config_2dot4g_rssi_close;
	u8 rssi_close_2dot4g_val;

	u8 config_2dot4g_rssi_middle;
	u8 rssi_middle_2dot4g_val;
	u8 config_hop_count_limit;
	u8 hop_count_limit_val;

	u8 config_2dot4g_support;
	u8 support_2dot4g_val;

	u8 config_2dot4g_beacons;
	u8 beacon_2dot4g_val;
	u8 config_2dot4g_sdf;
	u8 sdf_2dot4g_val;
	u8 config_5g_beacons;
	u8 beacon_5g_val;
	u8 config_5g_sdf;
	u8 sdf_5g_val;
	u8 config_5g_rssi_close;
	u8 rssi_close_5g_val;
	u8 config_5g_rssi_middle;
	u8 rssi_middle_5g_val;

	/* The 24 bit Organizationally Unique ID + the 8 bit Network Id. */
	u8 config_oui;
	u32 oui_val;
	u8 config_intf_addr;
	u8 intf_addr_val[ETH_ALEN];

	/* channel frequency in MHz to enable Nan on */
	u8 config_24g_channel;
	u32 channel_24g_val;
	/*NAN Configuration Supplemental */
	u32 discovery_beacon_interval_ms;
	u32 nss_discovery;
	u32 enable_dw_early_termination;
	u32 enable_ranging;
};

struct slsi_hal_nan_data_path_cfg {
	u8 security_cfg;
	u8 qos_cfg;
};

struct slsi_hal_nan_data_path_app_info {
	u16 ndp_app_info_len;
	u8 ndp_app_info[SLSI_HAL_NAN_DP_MAX_APP_INFO_LEN];
};

struct slsi_hal_nan_data_path_initiator_req {
	u16 transaction_id;
	u32 requestor_instance_id;
	u8 channel_request_type;
	u32 channel;
	u8 peer_disc_mac_addr[ETH_ALEN];
	char ndp_iface[IFNAMSIZ + 1];
	struct slsi_hal_nan_data_path_cfg ndp_cfg;
	struct slsi_hal_nan_data_path_app_info app_info;
	struct slsi_nan_security_info key_info;
	u32 service_name_len;
	u8 service_name[SLSI_HAL_NAN_MAX_SERVICE_NAME_LEN];
};

struct slsi_hal_nan_data_path_indication_response {
	u16 transaction_id;
	u32 ndp_instance_id;
	char ndp_iface[IFNAMSIZ + 1];
	struct slsi_hal_nan_data_path_cfg ndp_cfg;
	struct slsi_hal_nan_data_path_app_info app_info;
	u32 rsp_code;
	struct slsi_nan_security_info key_info;
	u8 service_name_len;
	u8 service_name[SLSI_HAL_NAN_MAX_SERVICE_NAME_LEN];
};

struct slsi_hal_nan_data_end {
	u16 transaction_id;
	u8 num_ndp_instances;
	u32 ndp_instance_id[SLSI_NAN_MAX_NDP_INSTANCES];
};

struct slsi_hal_nan_capabilities {
	u32 max_concurrent_nan_clusters;
	u32 max_publishes;
	u32 max_subscribes;
	u32 max_service_name_len;
	u32 max_match_filter_len;
	u32 max_total_match_filter_len;
	u32 max_service_specific_info_len;
	u32 max_vsa_data_len;
	u32 max_mesh_data_len;
	u32 max_ndi_interfaces;
	u32 max_ndp_sessions;
	u32 max_app_info_len;
	u32 max_queued_transmit_followup_msgs;
	u32 ndp_supported_bands;
	u32 cipher_suites_supported;
	u32 max_scid_len;
	bool is_ndp_security_supported;
	u32 max_sdea_service_specific_info_len;
	u32 max_subscribe_address;
	u32 ndpe_attr_supported;
};

struct slsi_hal_nan_followup_ind {
	u16 publish_subscribe_id;
	u32 requestor_instance_id;
	u8 addr[ETH_ALEN];
	u8 dw_or_faw;
	u16 service_specific_info_len;
	u8 service_specific_info[SLSI_HAL_NAN_MAX_SERVICE_SPECIFIC_INFO_LEN];
	u16 sdea_service_specific_info_len;
	u8 sdea_service_specific_info[SLSI_HAL_NAN_MAX_SDEA_SERVICE_SPEC_INFO_LEN];
};

struct slsi_hal_nan_match_ind {
	u16 publish_subscribe_id;
	u32 requestor_instance_id;
	u8 addr[ETH_ALEN];
	u16 service_specific_info_len;
	u8 service_specific_info[SLSI_HAL_NAN_MAX_SERVICE_SPECIFIC_INFO_LEN];
	u16 sdf_match_filter_len;
	u8 sdf_match_filter[SLSI_HAL_NAN_MAX_MATCH_FILTER_LEN];
	u8 match_occurred_flag;
	u8 out_of_resource_flag;
	u8 rssi_value;
	u8 is_conn_capability_valid;
	struct slsi_hal_nan_connectivity_capability conn_capability;
	u8 num_rx_discovery_attr;
	struct slsi_hal_nan_receive_post_discovery discovery_attr[SLSI_HAL_NAN_MAX_POSTDISCOVERY_LEN];
	u8 num_chans;
	struct slsi_hal_nan_further_availability_channel famchan[32];
	u8 cluster_attribute_len;
	u8 cluster_attribute[32];
	struct slsi_nan_security_info sec_info;
	struct slsi_nan_sdea_ctrl_params peer_sdea_params;
	u32 range_measurement_mm;
	u32 ranging_event_type;
	u16 sdea_service_specific_info_len;
	u8 sdea_service_specific_info[SLSI_HAL_NAN_MAX_SDEA_SERVICE_SPEC_INFO_LEN];
};

struct slsi_hal_nan_channel_info {
	u32 channel;
	u32 bandwidth;
	u32 nss;
};

struct slsi_nan_data_path_request_ind {
	u16 service_instance_id;
	u8 peer_disc_mac_addr[ETH_ALEN];
	u32 ndp_instance_id;
	struct slsi_hal_nan_data_path_cfg ndp_cfg;
	struct slsi_hal_nan_data_path_app_info app_info;
};

#define SLSI_HAL_NAN_MAX_CHANNEL_INFO_SUPPORTED 4
struct slsi_hal_nan_data_path_confirm_ind {
	u32 ndp_instance_id;
	u8 peer_ndi_mac_addr[ETH_ALEN];
	struct slsi_hal_nan_data_path_app_info app_info;
	u32 rsp_code;
	u32 reason_code;
	u32 num_channels;
	struct slsi_hal_nan_channel_info channel_info[SLSI_HAL_NAN_MAX_CHANNEL_INFO_SUPPORTED];
};

struct slsi_hal_nan_data_path_end_ind {
	u8 num_ndp_instances;
	u32 ndp_instance_id[SLSI_NAN_MAX_NDP_INSTANCES];
};

void slsi_nan_event(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_nan_followup_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_nan_service_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_nan_get_mac(struct slsi_dev *sdev, char *nan_mac_addr);
struct net_device *slsi_nan_get_netdev(struct slsi_dev *sdev);
int slsi_nan_enable(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_disable(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_publish(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_publish_cancel(struct wiphy *wiphy, struct wireless_dev *wdev,
			    const void *data, int len);
int slsi_nan_subscribe(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_subscribe_cancel(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_transmit_followup(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_set_config(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_get_capabilities(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
void slsi_nan_send_disabled_event(struct slsi_dev *sdev, struct net_device *dev, u32 reason);
int slsi_nan_data_iface_create(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_data_iface_delete(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_ndp_initiate(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_ndp_respond(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_ndp_end(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len);
int slsi_nan_ndp_new_entry(struct slsi_dev *sdev, struct net_device *dev, u32 ndp_instance_id,
			   u16 ndl_vif_id, u8 *local_ndi, u8 *peer_nmi);
void slsi_nan_ndp_del_entry(struct slsi_dev *sdev, struct net_device *dev, u32 ndp_instance_id, const bool ndl_vif_locked);
void slsi_nan_ndp_setup_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, bool is_req_ind);
void slsi_nan_ndp_requested_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_nan_ndp_termination_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
u32 slsi_nan_get_ndp_from_ndl_local_ndi(struct net_device *dev, u16 ndl_vif_id, u8 *local_ndi);
void slsi_nan_del_peer(struct slsi_dev *sdev, struct net_device *dev, u8 *local_ndi, u16 ndp_instance_id);
void slsi_nan_ndp_termination_handler(struct slsi_dev *sdev, struct net_device *dev, u16 ndp_instance_id, u8 *ndi);
int slsi_nan_push_followup_ids(struct slsi_dev *sdev, struct net_device *dev, u16 match_id, u16 trans_id);
void slsi_nan_pop_followup_ids(struct slsi_dev *sdev, struct net_device *dev, u16 match_id);
void slsi_rx_nan_range_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_send_nan_range_cancel(struct slsi_dev *sdev);
int slsi_send_nan_range_config(struct slsi_dev *sdev, u8 count, struct slsi_rtt_config *nl_rtt_params, int rtt_id);

#endif
