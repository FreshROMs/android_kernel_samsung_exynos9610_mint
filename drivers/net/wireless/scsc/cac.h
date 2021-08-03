/****************************************************************************
 *
 * Copyright (c) 2012 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef CAC_H
#define CAC_H

#include <linux/kernel.h>
#include "dev.h"
#include "debug.h"
#include "mlme.h"
#include "mgt.h"

/* management */
#define WLAN_OUI_CISCO                          0x004096 /* Cisco systems OUI */
#define WLAN_OUI_TYPE_CISCO_EDCA                0x09

#define WMM_OUI_SUBTYPE_INFORMATION_ELEMENT     0
#define WMM_OUI_SUBTYPE_PARAMETER_ELEMENT       1
#define WMM_OUI_SUBTYPE_TSPEC_ELEMENT           2
#define WMM_VERSION                             1
#define WMM_ACTION_CODE_ADDTS_REQ               0
#define WMM_ACTION_CODE_ADDTS_RESP              1
#define WMM_ACTION_CODE_DELTS                   2
#define WMM_ADDTS_STATUS_ADMISSION_ACCEPTED     0
#define WMM_ADDTS_STATUS_INVALID_PARAMETERS     1
/* 2 - Reserved */
#define WMM_ADDTS_STATUS_REFUSED                3
/* 4-255 - Reserved */

/* WMM TSPEC Direction Field Values */
#define WMM_TSPEC_DIRECTION_UPLINK              0
#define WMM_TSPEC_DIRECTION_DOWNLINK            1
/* 2 - Reserved */
#define WMM_TSPEC_DIRECTION_BI_DIRECTIONAL      3

/* WMM TSPEC PSB Field Values */
#define WMM_TSPEC_PSB_UNSPECIFIED               2

#define ADDTS_STATUS_ACCEPTED                   0x00
#define ADDTS_STATUS_INVALID_PARAM              0x01
#define ADDTS_STATUS_REFUSED                    0x03
#define ADDTS_STATUS_DELAY                      0x2F
#define ADDTS_STATUS_UNSPECIFIED                0xC8
#define ADDTS_STATUS_POLICY_CONFIG              0xC9
#define ADDTS_STATUS_ASSOC_DENIED               0xCA
#define ADDTS_STATUS_INVALID_PARAM2             0xCB

#define TSINFO_MASK                             0x00FFFFFF

#define CCX_MAX_NUM_RATES                       8

#define TSID_MIN                                0
#define TSID_MAX                                7

#define TSRS_RATE_PER_UNIT                      500000
#define IEEE80211_HEADER_SIZE                   24

#define MAX_TRANSMIT_MSDU_LIFETIME_NOT_VALID    -1
#define BSS_CCX_DISABLED                        0
#define BSS_CCX_ENABLED                         1

/* Macros for handling unaligned memory accesses */
#define CAC_GET_LE16(a)                         ((u16)(((a)[1] << 8) | (a)[0]))
#define CAC_PUT_LE16(a, val)            \
	do {                    \
		(a)[1] = ((u16)(val)) >> 8;    \
		(a)[0] = ((u16)(val)) & 0xff;    \
	} while (0)
#define CAC_PUT_BE24(a, val)                    \
	do {                            \
		(a)[0] = (u8)((((u32)(val)) >> 16) & 0xff);    \
		(a)[1] = (u8)((((u32)(val)) >> 8) & 0xff);    \
		(a)[2] = (u8)(((u32)(val)) & 0xff);        \
	} while (0)
#define CAC_GET_LE24(a) ((((u32)(a)[2]) << 16) | (((u32)(a)[1]) << 8) | ((u32)(a)[0]))
#define CAC_PUT_LE24(a, val)                    \
	do {                            \
		(a)[2] = (u8)((((u32)(val)) >> 16) & 0xff);    \
		(a)[1] = (u8)((((u32)(val)) >> 8) & 0xff);    \
		(a)[0] = (u8)(((u32)(val)) & 0xff);        \
	} while (0)
#define CAC_GET_LE32(a) ((((u32)(a)[3]) << 24) | (((u32)(a)[2]) << 16) | \
			 (((u32)(a)[1]) << 8) | ((u32)(a)[0]))
#define CAC_PUT_LE32(a, val)                    \
	do {                            \
		(a)[3] = (u8)((((u32)(val)) >> 24) & 0xff);    \
		(a)[2] = (u8)((((u32)(val)) >> 16) & 0xff);    \
		(a)[1] = (u8)((((u32)(val)) >> 8) & 0xff);    \
		(a)[0] = (u8)(((u32)(val)) & 0xff);        \
	} while (0)

#define IEEE80211_FC(type, stype) (u16)(type | stype)

/* WMM TSPEC Element */
struct wmm_tspec_element {
	char eid;         /* 221 = 0xdd */
	u8   length;      /* 6 + 55 = 61 */
	u8   oui[3];      /* 00:50:f2 */
	u8   oui_type;    /* 2 */
	u8   oui_subtype; /* 2 */
	u8   version;     /* 1 */
	/* WMM TSPEC body (55 octets): */
	u8   ts_info[3];
	u16  nominal_msdu_size;
	u16  maximum_msdu_size;
	u32  minimum_service_interval;
	u32  maximum_service_interval;
	u32  inactivity_interval;
	u32  suspension_interval;
	u32  service_start_time;
	u32  minimum_data_rate;
	u32  mean_data_rate;
	u32  peak_data_rate;
	u32  maximum_burst_size;
	u32  delay_bound;
	u32  minimum_phy_rate;
	u16  surplus_bandwidth_allowance;
	u16  medium_time;
} __packed;

#define MSDU_LIFETIME_DEFAULT 512

struct cac_activated_tspec {
	struct wmm_tspec_element tspec;
	int                      ebw;
};

struct tspec_field {
	const char *name;
	int        read_only;
	int        is_tsinfo_field;
	u8         size;
	u32        offset;
};

struct cac_tspec {
	struct cac_tspec         *next;
	int                      id;
	struct wmm_tspec_element tspec;
	u8                      psb_specified;
	int                      ebw;
	int                      accepted;
	u8                       dialog_token;
};

#define OFFSETOF(m) ((size_t)&((struct wmm_tspec_element *)0)->m)

struct wmm_action_hdr {
	u8 category;
	u8 action;
	u8 dialog_token;
	u8 status_code;
} __packed;

struct action_addts_req {
	struct wmm_action_hdr    hdr;
	struct wmm_tspec_element tspec;
} __packed;

struct action_addts_rsp {
	struct wmm_action_hdr hdr;
} __packed;

struct action_delts_req {
	struct wmm_action_hdr    hdr;
	struct wmm_tspec_element tspec;
} __packed;
/* prototypes for public functions */
int cac_ctrl_create_tspec(struct slsi_dev *sdev, char *args);
int cac_ctrl_config_tspec(struct slsi_dev *sdev, char *args);
int cac_ctrl_send_addts(struct slsi_dev *sdev, char *args);
int cac_ctrl_send_delts(struct slsi_dev *sdev, char *args);
int cac_update_local_tspec(struct slsi_dev *sdev, u16 msdu_lifetime, struct wmm_tspec_element *tspec);
int cac_get_active_tspecs(struct cac_activated_tspec **tspecs);
void cac_delete_tspec_list(struct slsi_dev *sdev);
int cac_ctrl_delete_tspec(struct slsi_dev *sdev, char *args);
void cac_rx_wmm_action(struct slsi_dev *sdev, struct net_device *netdev, struct ieee80211_mgmt *data, size_t len);
void cac_update_roam_traffic_params(struct slsi_dev *sdev, struct net_device *dev);
#endif /* CAC_H */
