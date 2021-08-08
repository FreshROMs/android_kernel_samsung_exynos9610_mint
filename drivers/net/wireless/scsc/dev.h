/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_DEVICE_H__
#define __SLSI_DEVICE_H__

#include <linux/init.h>
#include <linux/device.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ratelimit.h>
#include <linux/ip.h>

#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/nl80211.h>
#include <linux/wireless.h>
#include <linux/proc_fs.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/nl80211.h>

#include <scsc/scsc_mx.h>

#include "fapi.h"
#include "const.h"
#include "utils.h"
#include "hip.h"
#include "log_clients.h"
#include "src_sink.h"
#include "scsc_wifi_fcq.h"
#include "scsc_wifi_cm_if.h"
#include "hip4.h"
#include "nl80211_vendor.h"
#include "traffic_monitor.h"
#include "reg_info.h"

#define FAPI_MAJOR_VERSION(v) (((v) >> 8) & 0xFF)
#define FAPI_MINOR_VERSION(v) ((v) & 0xFF)

/* Modes for CMDGETBSSINFO and CMDGETSTAINFO */
#define SLSI_80211_MODE_11B 0
#define SLSI_80211_MODE_11G 1
#define SLSI_80211_MODE_11N 2
#define SLSI_80211_MODE_11A 3
#define SLSI_80211_MODE_11AC 4
#define SLSI_80211_MODE_11AX 5

#define SLSI_FW_API_RATE_HT_SELECTOR_FIELD  0xc000
#define SLSI_FW_API_RATE_NON_HT_SELECTED    0x4000
#define SLSI_FW_API_RATE_HT_SELECTED        0x8000
#define SLSI_FW_API_RATE_VHT_SELECTED       0xc000
#define SLSI_FW_API_RATE_HE_SELECTED        0x10000

#define SLSI_FW_API_RATE_VHT_MCS_FIELD      0x000F
#define SLSI_FW_API_RATE_HT_MCS_FIELD       0x003F
#define SLSI_FW_API_RATE_INDEX_FIELD        0x1fff
#define SLSI_FW_API_RATE_VHT_NSS_FIELD      0x0070
#define SLSI_FW_API_RATE_HT_NSS_FIELD       0x0040

#define SLSI_FW_API_RATE_BW_FIELD           0x0600
#define SLSI_FW_API_RATE_BW_40MHZ           0x0200
#define SLSI_FW_API_RATE_BW_20MHZ           0x0000

#define SLSI_FW_API_RATE_SGI                0x0100
#define SLSI_FW_API_RATE_GF                 0x0080
#define SLSI_FW_API_RATE_11AX_GI_POSN       17
#define SLSI_FW_API_RATE_11AX_GI            (0x3 << SLSI_FW_API_RATE_11AX_GI_POSN)
#define SLSI_FW_API_GET_11AX_GI(val)        ((val & SLSI_FW_API_RATE_11AX_GI) >> SLSI_FW_API_RATE_11AX_GI_POSN)

#define SLSI_HOSTSTATE_LCD_ACTIVE         0x0001
#define SLSI_HOSTSTATE_CELLULAR_ACTIVE    0x0002
#define SLSI_HOSTSTATE_SAR_ACTIVE         0x0004
#define SLSI_HOSTSTATE_GRIP_ACTIVE        0x0040
#define SLSI_HOSTSTATE_LOW_LATENCY_ACTIVE 0x0080
#define SLSI_HOST_TAG_ARP_MASK            BIT(15)
#define SLSI_ARP_UNPAUSE_THRESHOLD        4
/* RTT ID: : A value (1-7) for identifying the RTT activity being requested. */
#define SLSI_MIN_RTT_ID  1
#define SLSI_MAX_RTT_ID  7

#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
#define SLSI_MAX_ARP_SEND_FRAME  8
#endif

/* indices: 3= BW20->idx_0, BW40->idx_1, BW80->idx_2.
 *             2= noSGI->idx_0, SGI->idx_1
 *             10= mcs index
 * rate units 100kbps
 * This table for single stream Nss=1and does not include 160MHz BW and 80+80MHz BW.
 */
static const u16 slsi_rates_table[3][2][10] = {
	{         /* BW20 */
		{ /* no SGI */
			65, 130, 195, 260, 390, 520, 585, 650, 780, 0
		},
		{       /* SGI */
			72, 144, 217, 289, 433, 578, 650, 722, 867, 0
		}
	},
	{         /* BW40 */
		{ /* no SGI */
			135, 270, 405, 540, 810, 1080, 1215, 1350, 1620, 1800
		},
		{       /* SGI */
			150, 300, 450, 600, 900, 1200, 1350, 1500, 1800, 2000
		}
	},
	{         /* BW80 */
		{ /* no SGI */
			293, 585, 878, 1170, 1755, 2340, 2633, 2925, 3510, 3900
		},
		{       /* SGI */
			325, 650, 975, 1300, 1950, 2600, 2925, 3250, 3900, 4333
		}
	}
};

static const u16 slsi_he_rates_table_2x2[4][12][3] = {
	{       /* HE 20 MHZ */
		{ 17, 16, 15 }, { 34, 33, 29 }, { 52, 49, 44 }, { 69, 65, 59 },
		{ 103, 98, 88 }, { 138, 130, 117 }, { 155, 146, 132 },
		{ 172, 163, 146 }, { 206, 195, 176 }, { 229, 217, 195 },
		{ 258, 244, 219 }, { 287, 271, 244 }
	},
	{       /* HE 40 MHz  */
		{ 34, 33, 29 }, { 69, 65, 59 }, { 103, 98, 88 }, { 138, 130, 117 },
		{ 206, 195, 178 }, { 275, 260, 234 }, { 310, 292, 263 },
		{ 334, 325, 293 }, { 413, 390, 351 }, { 459, 433, 390 },
		{ 516, 488, 439 }, { 574, 542, 488 }
	},
	{       /* He 80 MHz  */
		{ 72, 68, 61 }, { 144, 136, 133 }, { 216, 204, 184 },
		{ 288, 272, 245 }, { 432, 408, 368 }, { 576, 544, 490 },
		{ 649, 613, 551 }, { 721, 681, 613 }, { 864, 817, 735 },
		{ 961, 907, 816 }, { 1081, 1021, 919 }, { 1201, 1134, 1021 }
	},
	{       /* HE 160 MHz / 80+80 MHz */
		{ 144, 136, 123 }, { 288, 272, 245 }, { 432, 408, 368 },
		{ 576, 544, 490 }, { 865, 817, 735 }, { 1153, 1089, 980 },
		{ 1297, 1225, 1103 }, { 1441, 1361, 1225 }, { 1729, 1633, 1470 },
		{ 1921, 1815, 1633 }, { 2162, 2042, 1838 }, { 2402, 2268, 2042 }
	}
};

/* MSDU subframe Header */
struct msduhdr {
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	__be16		len;			/* MSDU Subframe length */
	unsigned char	dsap;			/* DSAP field - SNAP 0xaa */
	unsigned char	ssap;			/* SSAP field - SNAP 0xaa */
	unsigned char	ui;			/* Control field: U, func UI - 0x03 */
	unsigned char	oui[3];			/* Organization Code - 0x000000 */
	__be16		type;			/* Type - 0x0800 (IPv4)*/
} __attribute__((packed));

static inline void ethr_ii_to_subframe_msdu(struct sk_buff *skb)
{
	struct ethhdr *ehdr;
	struct msduhdr msduh;

	ehdr = eth_hdr(skb);
	ether_addr_copy(msduh.h_dest, ehdr->h_dest);
	ether_addr_copy(msduh.h_source, ehdr->h_source);
	/* adjust packet length */
	msduh.len = cpu_to_be16(skb->len - 6);
	msduh.dsap = 0xaa;
	msduh.ssap = 0xaa;
	msduh.ui   = 0x03;
	memset(msduh.oui, 0x0, 3);
	msduh.type = ehdr->h_proto;
	(void)skb_push(skb, sizeof(struct msduhdr) - sizeof(struct ethhdr));
	/* update SKB mac_header to point to start of MSDU header */
	skb->mac_header -= (sizeof(struct msduhdr) - sizeof(struct ethhdr));
	memcpy(skb->data, &msduh, sizeof(struct msduhdr));
}

#define SLSI_TX_PROCESS_ID_MIN       (0xC001)
#define SLSI_TX_PROCESS_ID_MAX       (0xCF00)
#define SLSI_TX_PROCESS_ID_UDI_MIN   (0xCF01)
#define SLSI_TX_PROCESS_ID_UDI_MAX   (0xCFFE)

/* There are no wakelocks in kernel/supplicant/hostapd.
 * So keep the platform active for some time after receiving any data packet.
 * This timeout value can be fine-tuned based on the test results.
 */
#define SLSI_RX_WAKELOCK_TIME (200)
#define MAX_BA_BUFFER_SIZE 64
#define NUM_BA_SESSIONS_PER_PEER 8
#define SLSI_NCHO_MAX_CHANNEL_LIST 20
#define SLSI_MAX_CHANNEL_LIST 20
#define SLSI_MAX_RX_BA_SESSIONS (8)
#define SLSI_STA_ACTION_FRAME_BITMAP (SLSI_ACTION_FRAME_PUBLIC | SLSI_ACTION_FRAME_WMM | SLSI_ACTION_FRAME_WNM |\
				      SLSI_ACTION_FRAME_QOS | SLSI_ACTION_FRAME_PROTECTED_DUAL |\
				      SLSI_ACTION_FRAME_RADIO_MEASUREMENT)
#define SLSI_STA_ACTION_FRAME_SUSPEND_BITMAP (SLSI_ACTION_FRAME_PUBLIC | SLSI_ACTION_FRAME_WMM | SLSI_ACTION_FRAME_WNM |\
				      SLSI_ACTION_FRAME_QOS | SLSI_ACTION_FRAME_PROTECTED_DUAL)

/* Default value for MIB SLSI_PSID_UNIFI_DISCONNECT_TIMEOUT + 1 sec*/
#define SLSI_DEFAULT_AP_DISCONNECT_IND_TIMEOUT 3000

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0))
#define WLAN_EID_VHT_CAPABILITY 191
#define WLAN_EID_VHT_OPERATION 192
#endif

#define NUM_COUNTRY             (300)

#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
#define SLSI_MUTEX_INIT(slsi_mutex__) \
	do { \
		(slsi_mutex__).owner = NULL; \
		mutex_init(&(slsi_mutex__).mutex); \
		(slsi_mutex__).valid = true; \
	} while (0)

#define SLSI_MUTEX_LOCK(slsi_mutex_to_lock) \
	do { \
		(slsi_mutex_to_lock).line_no_before = __LINE__; \
		(slsi_mutex_to_lock).file_name_before = __FILE__; \
		mutex_lock(&(slsi_mutex_to_lock).mutex); \
		(slsi_mutex_to_lock).owner = current; \
		(slsi_mutex_to_lock).line_no_after = __LINE__; \
		(slsi_mutex_to_lock).file_name_after = __FILE__; \
		(slsi_mutex_to_lock).function = __func__; \
	} while (0)

#define SLSI_MUTEX_UNLOCK(slsi_mutex_to_unlock) \
	do { \
		(slsi_mutex_to_unlock).owner = NULL; \
		mutex_unlock(&(slsi_mutex_to_unlock).mutex); \
	} while (0)
#define SLSI_MUTEX_IS_LOCKED(slsi_mutex__) mutex_is_locked(&(slsi_mutex__).mutex)

struct slsi_mutex {
	bool               valid;
	u32                line_no_before;
	const u8           *file_name_before;
	/* a std mutex */
	struct mutex       mutex;
	u32                line_no_after;
	const u8           *file_name_after;
	const u8           *function;
	struct task_struct *owner;
};

#else
#define SLSI_MUTEX_INIT(mutex__)           mutex_init(&(mutex__))
#define SLSI_MUTEX_LOCK(mutex_to_lock)     mutex_lock(&(mutex_to_lock))
#define SLSI_MUTEX_UNLOCK(mutex_to_unlock) mutex_unlock(&(mutex_to_unlock))
#define SLSI_MUTEX_IS_LOCKED(mutex__)      mutex_is_locked(&(mutex__))
#endif

#define OS_UNUSED_PARAMETER(x) ((void)(x))

#define SLSI_HOST_TAG_TRAFFIC_QUEUE(htag) (htag & 0x00000003)

/* For each mlme-req a mlme-cfm is expected to be received from the
 * firmware. The host is not allowed to send another mlme-req until
 * the mlme-cfm is received.
 *
 * However there are also instances where we need to wait for an mlme-ind
 * following a mlme-req/cfm exchange. One example of this is the disconnect
 * sequence:
 * mlme-disconnect-req - host requests disconnection
 * mlme-disconnect-cfm - firmware accepts disconnection request but hasn't
 *                       disconnected yet.
 * mlme-disconnect-ind - firmware reports final result of disconnection
 *
 * Assuming that waiting for the mlme-ind following on from the mlme-req/cfm
 * is ok.
 */
struct slsi_sig_send {
	/* a std spinlock */
	spinlock_t        send_signal_lock;
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	struct slsi_mutex mutex;
#else
	/* a std mutex */
	struct mutex      mutex;
#endif
	struct completion completion;

	u16               process_id;
	u16               req_id;
	u16               cfm_id;
	u16               ind_id;
	struct sk_buff    *cfm;
	struct sk_buff    *ind;
	struct sk_buff    *mib_error;
};

static inline void slsi_sig_send_init(struct slsi_sig_send *sig_send)
{
	spin_lock_init(&sig_send->send_signal_lock);
	sig_send->req_id = 0;
	sig_send->cfm_id = 0;
	sig_send->process_id = SLSI_TX_PROCESS_ID_MIN;
	SLSI_MUTEX_INIT(sig_send->mutex);
	init_completion(&sig_send->completion);
}

struct slsi_ba_frame_desc {
	bool           active;
	struct sk_buff *signal;
	u16            sn;
};

struct slsi_ba_session_rx {
	bool                      active;
	bool                      used;
	void                      *vif;
	struct slsi_ba_frame_desc buffer[MAX_BA_BUFFER_SIZE];
	u16                       buffer_size;
	u16                       occupied_slots;
	u16                       expected_sn;
	u16                       start_sn;
	u16                       highest_received_sn;
	bool                      closing;
	bool                      trigger_ba_after_ssn;
	u8                        tid;

	/* Aging timer parameters */
	bool                      timer_on;
	struct timer_list         ba_age_timer;
	struct net_device         *dev;
};

#define SLSI_TID_MAX    (16)
#define SLSI_AMPDU_F_INITIATED      (0x0001)
#define SLSI_AMPDU_F_CREATED        (0x0002)
#define SLSI_AMPDU_F_OPERATIONAL    (0x0004)

#define SLSI_SCAN_HW_ID       0
#define SLSI_SCAN_SCHED_ID    1
#define SLSI_SCAN_MAX         3

#define SLSI_SCAN_SSID_MAP_MAX         10 /* Arbitrary value */
#define SLSI_SCAN_SSID_MAP_EXPIRY_AGE  2  /* If hidden bss not found these many scan cycles, remove map. Arbitrary value*/
#define SLSI_FW_SCAN_DONE_TIMEOUT_MSEC (20 * 1000)
#define MAX_CHANNEL_COUNT              40

#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
enum slsi_bss_security {
	SLSI_BSS_SECURED_NO = BIT(0),
	SLSI_BSS_SECURED_PSK = BIT(1),
	SLSI_BSS_SECURED_1x = BIT(2),
	SLSI_BSS_SECURED_SAE = BIT(3)
};

struct slsi_ssid_info {
	struct list_head list;
	struct cfg80211_ssid ssid;
	struct list_head bssid_list;
	u8 akm_type;
};

struct slsi_bssid_info {
	struct list_head list;
	u8 bssid[ETH_ALEN];
	u16 freq;
	int rssi;
	bool connect_attempted;
};

struct slsi_bssid_blacklist_info {
	struct list_head list;
	u8 bssid[ETH_ALEN];
	int end_time;
};
#endif

struct slsi_scan_result {
	u8 bssid[ETH_ALEN];
	u8 hidden;
	int rssi;
	struct sk_buff *probe_resp;
	struct sk_buff *beacon;
	struct slsi_scan_result *next;
	int band;
	u8 ssid[32];
	u8 ssid_length;
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	u8 akm_type;
#endif
};

/* Per Interface Scan Data
 * Access protected by: cfg80211_lock
 */
struct slsi_scan {
	/* When a Scan is running this not NULL. */
	struct cfg80211_scan_request       *scan_req;
	struct slsi_acs_request            *acs_request;
	struct cfg80211_sched_scan_request *sched_req;
	bool                               requeue_timeout_work;

	/* Indicates if the scan req is blocking. i.e, waiting until scan_done_ind received */
	bool                               is_blocking_scan;

	struct slsi_scan_result *scan_results; /* head for scan_results list*/
};

struct slsi_ssid_map {
	u8 bssid[ETH_ALEN];
	u8 ssid[32];
	u8 ssid_len;
	u8 age;
	int band;
};

#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
struct slsi_enhanced_arp_counters {
	u16 arp_req_count_from_netdev;
	u16 arp_req_count_to_lower_mac;
	u16 arp_req_rx_count_by_lower_mac;
	u16 arp_req_count_tx_success;
	u16 arp_rsp_rx_count_by_lower_mac;
	u16 arp_rsp_rx_count_by_upper_mac;
	u16 arp_rsp_count_to_netdev;
	u16 arp_rsp_count_out_of_order_drop;
	u16 ap_link_active;
	bool is_duplicate_addr_detected;
};
#endif

struct slsi_peer {
	/* Flag MUST be set last when creating a record and immediately when removing.
	 * Otherwise another process could test the flag and start using the data.
	 */
	bool                           valid;
	u8                             address[ETH_ALEN];

	/* Presently connected_state is used only for AP/GO mode*/
	u8                             connected_state;
	u16                            aid;
	/* Presently is_wps is used only in P2P GO mode */
	bool                           is_wps;
	u16                            capabilities;
	bool                           qos_enabled;
	u8                             queueset;
	struct scsc_wifi_fcq_data_qset data_qs;
	struct scsc_wifi_fcq_ctrl_q    ctrl_q;

	bool                           authorized;
	bool                           pairwise_key_set;

	/* Needed for STA/AP VIF */
	struct sk_buff                 *assoc_ie;
	struct sk_buff_head            buffered_frames;
	/* Needed for STA VIF */
	struct sk_buff                 *assoc_resp_ie;

	/* bitmask that keeps the status of acm bit for each AC
	 * bit 7  6  5  4  3  2  1  0
	 *     |  |  |  |  |  |  |  |
	 *     vo vo vi vi be bk bk be
	 */
	u8 wmm_acm;
	/* bitmask that keeps the status of tspec establishment for each priority
	 * bit 7  6  5  4  3  2  1  0
	 *     |  |  |  |  |  |  |  |
	 *     p7 p6 p5 p4 p3 p2 p1 p0
	 */
	u8 tspec_established;
	u8 uapsd;

	/* TODO_HARDMAC:
	 * Q: Can we obtain stats from the firmware?
	 *    Yes - then this is NOT needed and we can just get from the firmware when requested.
	 *    No -  How much can we get from the PSCHED?
	 */
	struct station_info       sinfo;
	/* rate limit for peer sinfo mib reads  */
	struct ratelimit_state    sinfo_mib_get_rs;
	struct slsi_ba_session_rx *ba_session_rx[NUM_BA_SESSIONS_PER_PEER];

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	/* qos map configured at peer end*/
	bool	 qos_map_set;
	struct cfg80211_qos_map qos_map;
#endif
	u16 ndl_vif;
	u8 ndp_count;

};

/* Used to update vif type on vif deactivation indicating vif is no longer available */
#define SLSI_VIFTYPE_UNSPECIFIED   0xFFFF

struct slsi_vif_mgmt_tx {
	u64      cookie;    /* Cookie assigned by Host for the tx mgmt frame */
	u16      host_tag;  /* Host tag for the tx mgmt frame */
	const u8 *buf;      /* Buffer - Mgmt frame requested for tx */
	size_t   buf_len;   /* Buffer length */
	u8       exp_frame; /* Next expected Public action frame subtype from peer */
};

struct slsi_wmm_ac {
	u8  aci_aifsn;
	u8  ecw;
	u16 txop_limit;
}  __packed;

/* struct slsi_wmm_parameter_element
 *
 * eid - Vendor Specific
 * len - Remaining Length of IE
 * oui - Microsoft OUI
 * oui_type - WMM
 * oui_subtype - Param IE
 * version - 1
 * qos_info - Qos
 * reserved -
 * ac - BE,BK,VI,VO
 */
struct slsi_wmm_parameter_element {
	u8                 eid;
	u8                 len;
	u8                 oui[3];
	u8                 oui_type;
	u8                 oui_subtype;
	u8                 version;
	u8                 qos_info;
	u8                 reserved;
	struct slsi_wmm_ac ac[4];
} __packed;

#define SLSI_MIN_FILTER_ID  0x80 /* Start of filter range reserved for host */

/* for AP */
#define SLSI_AP_ALL_IPV6_PKTS_FILTER_ID  0x80

/* filter IDs for filters installed by driver */
#ifdef CONFIG_SCSC_WLAN_BLOCK_IPV6

enum slsi_filter_id {
	SLSI_LOCAL_ARP_FILTER_ID = SLSI_MIN_FILTER_ID,				/* 0x80 */
	SLSI_ALL_BC_MC_FILTER_ID,						/* 0x81 */
	SLSI_PROXY_ARP_FILTER_ID,						/* 0x82 */
	SLSI_ALL_IPV6_PKTS_FILTER_ID,						/* 0x83 */
#ifndef CONFIG_SCSC_WLAN_DISABLE_NAT_KA
	SLSI_NAT_IPSEC_FILTER_ID,						/* 0x84 */
#endif
#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
	SLSI_OPT_OUT_ALL_FILTER_ID,						/* 0x85 */
	SLSI_OPT_IN_TCP4_FILTER_ID,						/* 0x86 */
	SLSI_OPT_IN_TCP6_FILTER_ID,						/* 0x87 */
#endif
	SLSI_MULTI_TO_UNICAST_IPV4_ID,						/* 0x85 / 0x88*/
	SLSI_ALL_ARP_FILTER_ID,							/* 0x86 / 0x89 */
	SLSI_REGD_MC_FILTER_ID,							/* 0x87 / 0x8a */
};
#else

/* for STA */
enum slsi_filter_id {
	SLSI_LOCAL_ARP_FILTER_ID = SLSI_MIN_FILTER_ID,				/* 0x80 */
	SLSI_ALL_BC_MC_FILTER_ID,						/* 0x81 */
	SLSI_PROXY_ARP_FILTER_ID,						/* 0x82 */
	SLSI_LOCAL_NS_FILTER_ID,						/* 0x83 */
	SLSI_PROXY_ARP_NA_FILTER_ID,						/* 0x84 */
#ifndef CONFIG_SCSC_WLAN_DISABLE_NAT_KA
	SLSI_NAT_IPSEC_FILTER_ID,						/* 0x85 */
#endif
#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
	SLSI_OPT_OUT_ALL_FILTER_ID,						/* 0x86 */
	SLSI_OPT_IN_TCP4_FILTER_ID,						/* 0x87 */
	SLSI_OPT_IN_TCP6_FILTER_ID,						/* 0x88 */
#endif
	SLSI_MULTI_TO_UNICAST_IPV4_ID,						/* 0x86 / 0x89 */
	SLSI_MULTI_TO_UNICAST_IPv6_ID,						/* 0x87 / 0x8a */
	SLSI_ALL_ARP_FILTER_ID,							/* 0x88 / 0x8b */
	SLSI_REGD_MC_FILTER_ID,							/* 0x89 / 0x8c */
};

#endif

#define SLSI_MAX_PKT_FILTERS       16

#ifndef CONFIG_SCSC_WLAN_DISABLE_NAT_KA
/* default config */
#define SLSI_MC_ADDR_ENTRY_MAX  (SLSI_MIN_FILTER_ID + SLSI_MAX_PKT_FILTERS - SLSI_REGD_MC_FILTER_ID)
#else
#define SLSI_MC_ADDR_ENTRY_MAX  (SLSI_MIN_FILTER_ID + SLSI_MAX_PKT_FILTERS - SLSI_REGD_MC_FILTER_ID + 1)
#endif

/* Values for vif_status field
 *
 * Used to indicate the status of an activated VIF, to help resolve
 * conflicting activities with indications from the firmware eg.
 * cfg80211 triggers a disconnection before a STA completes its
 * connection to an AP.
 */
#define SLSI_VIF_STATUS_UNSPECIFIED   0
#define SLSI_VIF_STATUS_CONNECTING    1
#define SLSI_VIF_STATUS_CONNECTED     2
#define SLSI_VIF_STATUS_DISCONNECTING 3

/*From wifi_offload.h (N_AVAIL_ID=3)*/
#define SLSI_MAX_KEEPALIVE_ID 3

struct slsi_last_connected_bss {
	u8                                           address[ETH_ALEN];
	int                                           antenna_mode;
	int                                           rssi;
	int                                           mode;
	int                                           passpoint_version;
	int                                           snr;
	int                                           noise_level;
	u16                                          bandwidth;
	u16                                          roaming_count;
	u16                                          channel_freq;
	u16                                          tx_data_rate;
	u8                                            roaming_akm;
	u8                                            kv;
	u32                                          kvie;
	bool                                         mimo_used;
	u8                                           ssid[IEEE80211_MAX_SSID_LEN + 1];
	u8                                           ssid_len;
};

enum slsi_wpa3_auth_state {
	SLSI_WPA3_PREAUTH,
	SLSI_WPA3_AUTHENTICATING,
	SLSI_WPA3_AUTHENTICATED
};

struct slsi_vif_sta {
	/* Only valid when the VIF is activated */
	u8                      vif_status;
	bool                    is_wps;
	u16                     eap_hosttag;
	u16                     m4_host_tag;
	u16                     keepalive_host_tag[SLSI_MAX_KEEPALIVE_ID];

	struct sk_buff          *roam_mlme_procedure_started_ind;

	/* This id is used to find out which response  (connect resp/roamed resp/reassoc resp)
	 * is to be sent once M4 is transmitted successfully
	 */
	u16                     resp_id;
	bool                    gratuitous_arp_needed;

	/* regd multicast address*/
	u8                      regd_mc_addr_count;
	u8                      regd_mc_addr[SLSI_MC_ADDR_ENTRY_MAX][ETH_ALEN];
	struct slsi_spinlock    regd_mc_addr_lock;

	bool                    group_key_set;
	bool			wep_key_set;
	struct sk_buff          *mlme_scan_ind_skb;
	bool                    roam_in_progress;
	int                     tdls_peer_sta_records;
	bool                    tdls_enabled;
	struct cfg80211_bss     *sta_bss;
	u8                      *assoc_req_add_info_elem;
	int                     assoc_req_add_info_elem_len;

	/* List of seen ESS and Freq associated with them */
	struct list_head        network_map;
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	struct list_head        ssid_info;
	struct list_head        blacklist_head;
#endif

	struct slsi_wmm_ac wmm_ac[4];
	bool                    nd_offload_enabled;
	unsigned long           data_rate_mbps;
	unsigned long           max_rate_mbps;

	/*This structure is used to store last disconnected bss info and valid even when vif is deactivated. */
	struct slsi_last_connected_bss last_connected_bss;
	struct cfg80211_crypto_settings crypto;

	/* Variable to indicate if roamed_ind needs to be dropped in driver, to maintain roam synchronization. */
	atomic_t                drop_roamed_ind;
	u8                      *vendor_disconnect_ies;
	int                     vendor_disconnect_ies_len;
	u8                      bssid[ETH_ALEN];
	u8                      ssid[IEEE80211_MAX_SSID_LEN];
	u8                      ssid_len;
#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
	u8                      *rsn_ie;
	int                     rsn_ie_len;
#endif

	/* Storing channel bitmap to use it for setting cached channels */
	u16                     channels_24_ghz;
	u32                     channels_5_ghz;
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	bool                    drv_bss_selection;

	/* save connection parameters in order to retry connection */
	struct                         cfg80211_connect_params sme;
	const u8                       *connected_bssid;
	const u8                       *connected_ssid;
	int                            connected_ssid_len;
	u8                             akm_type;
	int                            beacon_int;
	enum slsi_wpa3_auth_state      wpa3_auth_state;
	u32                            action_frame_bmap;
	u32                            action_frame_suspend_bmap;
#endif
};

struct slsi_vif_unsync {
	struct delayed_work roc_expiry_work;   /* Work on ROC duration expiry */
	struct delayed_work del_vif_work;      /* Work on unsync vif retention timeout */
	struct delayed_work hs2_del_vif_work;  /* Work on HS2 unsync vif retention timeout */
	struct delayed_work unset_channel_expiry_work;  /*unset channel after a timer */
	u64                 roc_cookie;        /* Cookie id for ROC */
	u8                  *probe_rsp_ies;    /* Probe response IEs to be configured in firmware */
	size_t              probe_rsp_ies_len; /* Probe response IE length */
	bool                ies_changed;       /* To indicate if Probe Response IEs have changed from that previously stored */
	bool                listen_offload;    /* To indicate if Listen Offload is started */
	bool                slsi_p2p_continuous_fullscan;
};

struct slsi_last_disconnected_sta {
	u8 address[ETH_ALEN];
	u32 rx_retry_packets;
	u32 rx_bc_mc_packets;
	u16 capabilities;
	int bandwidth;
	int antenna_mode;
	int rssi;
	int mode;
	u16 tx_data_rate;
	bool mimo_used;
	u16 reason;
	int support_mode;
};

struct slsi_vif_ap {
	struct slsi_wmm_parameter_element wmm_ie;
	struct slsi_last_disconnected_sta last_disconnected_sta;
	u8                                *cache_wmm_ie;
	u8                                *cache_wpa_ie;
	u8                                *add_info_ies;
	size_t                            wmm_ie_len;
	size_t                            wpa_ie_len;
	size_t                            add_info_ies_len;
	bool                              p2p_gc_keys_set;       /* Used in GO mode to identify that a CLI has connected after WPA2 handshake */
	bool                              privacy;               /* Used for port enabling based on the open/secured AP configuration */
	bool                              qos_enabled;
	int                               beacon_interval;       /* Beacon interval in AP/GO mode */
	int                               mode;
	bool                              non_ht_bss_present;    /* Non HT BSS observed in HT20 OBSS scan */
	struct scsc_wifi_fcq_data_qset    group_data_qs;
	u32                               cipher;
	u16                               channel_freq;
	u8                                ssid[IEEE80211_MAX_SSID_LEN];
	u8                                ssid_len;
#ifdef CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
	struct cfg80211_acl_data          *acl_data_blacklist;
#endif
};

struct slsi_nan_ndl_info {
	u8 peer_nmi[ETH_ALEN];
	s8 ndp_count;
};

struct slsi_nan_discovery_info {
	u8 peer_addr[ETH_ALEN];
	u8 session_id;
	u8 match_id;
	struct slsi_nan_discovery_info *next;
};

enum ndp_slot_status {
	ndp_slot_status_free,
	ndp_slot_status_in_use,
	ndp_slot_status_terminating,
};

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
struct slsi_vif_nan {
	struct slsi_hal_nan_config_req config;
	u32 service_id_map;
	u32 followup_id_map;
	u32 ndp_instance_id_map;
	u32 next_service_id;
	u32 next_ndp_instance_id;
	struct slsi_nan_ndl_info ndl_list[SLSI_NAN_MAX_NDP_INSTANCES];
	u8  ndp_ndi[SLSI_NAN_MAX_NDP_INSTANCES][ETH_ALEN];
	u16 ndp_instance_id2ndl_vif[SLSI_NAN_MAX_NDP_INSTANCES];
	u16 ndp_local_ndp_instance_id[SLSI_NAN_MAX_NDP_INSTANCES];
	enum ndp_slot_status ndp_state[SLSI_NAN_MAX_NDP_INSTANCES];
	/* followup_trans_id_map[index][]
	 * each index(row) has 2 columns, column 0 has matchId
	 * and cloumn 1 has transactionId
	 */
	u16 followup_trans_id_map[SLSI_NAN_MAX_HOST_FOLLOWUP_REQ][2];
	struct completion ndp_delay;

	u8 disable_cluster_merge;
	u16 nan_sdf_flags[SLSI_NAN_MAX_SERVICE_ID + 1];
	/* ndp_count is used by nan data vif */
	u8 ndp_count;
	/* fields used for nan stats/status*/
	u8 local_nmi[ETH_ALEN];
	u8 cluster_id[ETH_ALEN];
	u32 operating_channel[2];
	u8 role;
	u8 state; /* 1 -> nan on; 0 -> nan off */
	u8 master_pref_value;
	u8 amr;
	u8 hopcount;
	u32 random_mac_interval_sec;
	u8 matchid;
	struct slsi_nan_discovery_info *disc_info;
	unsigned long ndp_start_time;
};
#endif

#define TCP_ACK_SUPPRESSION_RECORDS_MAX				16
#define TCP_ACK_SUPPRESSION_RECORD_UNUSED_TIMEOUT	10 /* in seconds */

#define TCP_ACK_SUPPRESSION_OPTIONS_OFFSET	20
#define TCP_ACK_SUPPRESSION_OPTION_EOL		0
#define TCP_ACK_SUPPRESSION_OPTION_NOP		1
#define TCP_ACK_SUPPRESSION_OPTION_MSS		2
#define TCP_ACK_SUPPRESSION_OPTION_WINDOW	3
#define TCP_ACK_SUPPRESSION_OPTION_SACK		5

#define SLSI_IS_VIF_CHANNEL_5G(ndev_vif) (((ndev_vif)->chan) ? ((ndev_vif)->chan->hw_value > 14) : 0)

struct slsi_tcp_ack_s {
	u32 daddr;
	u32 dport;
	u32 saddr;
	u32 sport;
	struct sk_buff_head list;
	u8 window_multiplier;
	u16 mss;
	u32 ack_seq;
	u16 slow_start_count;
	u8 count;
	u8 max;
	u8 age;
	struct timer_list timer;
	bool state;
	ktime_t last_sent;
	bool tcp_slow_start;

	/* TCP session throughput monitor */
	u16		hysteresis;
	u32		last_tcp_rate;
	ktime_t last_sample_time;
	u32		last_ack_seq;
	u64		num_bytes;
#ifdef CONFIG_SCSC_WLAN_HIP4_PROFILING
	u8 stream_id;
	u8 rx_window_scale;
#endif
	struct netdev_vif *ndev_vif;
};

struct slsi_tcp_ack_stats {
	u32 tack_acks;
	u32 tack_suppressed;
	u32 tack_sent;
	u32 tack_max;
	u32 tack_timeout;
	u32 tack_dacks;
	u32 tack_sacks;
	u32 tack_delay_acks;
	u32 tack_low_window;
	u32 tack_nocache;
	u32 tack_norecord;
	u32 tack_hasdata;
	u32 tack_psh;
	u32 tack_dropped;
	u32 tack_ktime;
	u32 tack_lastrecord;
	u32 tack_searchrecord;
	u32 tack_ece;
};

#define	SLSI_NETIF_SET_TID_OFF		0 /* change of TID is off */
#define	SLSI_NETIF_SET_TID_ALL_UDP	1 /* change TID for all UDP packets */

struct slsi_netif_set_tid_attr {
	u8 mode;
	u32 uid;
	u8 tid;
};

struct netdev_vif {
	struct slsi_dev             *sdev;
	struct wireless_dev         wdev;
	atomic_t                    is_registered;         /* Has the net dev been registered */
	bool                        is_available;          /* Has the net dev been opened AND is usable */
	bool                        is_fw_test;            /* Is the device in use as a test device via UDI */
#if defined(CONFIG_SLSI_WLAN_STA_FWD_BEACON) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
	bool                        is_wips_running;
#endif
	/* Structure can be accessed by cfg80211 ops, procfs/ioctls and as a result
	 * of receiving MLME indications e.g. MLME-CONNECT-IND that can affect the
	 * status of the interface eg. STA connect failure will delete the VIF.
	 */
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	struct slsi_mutex           vif_mutex;
#else
	/* a std mutex */
	struct mutex                vif_mutex;
#endif
	struct slsi_sig_send        sig_wait;
#ifndef CONFIG_SCSC_WLAN_RX_NAPI
	struct slsi_skb_work        rx_data;
#endif
	struct slsi_skb_work        rx_mlme;
	u16                         ifnum;
	enum nl80211_iftype         iftype;
	enum nl80211_channel_type   channel_type;
	struct ieee80211_channel    *chan;
	u16 driver_channel;
	bool drv_in_p2p_procedure;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	struct cfg80211_chan_def    *chandef;
	struct cfg80211_chan_def    chandef_saved;
#endif

	/* NOTE: The Address is a __be32
	 * It needs converting to pass to the FW
	 * But not for the Arp or trace %pI4
	 */
	__be32                      ipaddress;

#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	struct in6_addr             ipv6address;
	struct slsi_spinlock        ipv6addr_lock;
#endif
	struct net_device_stats     stats;
	u32                         rx_packets[SLSI_LLS_AC_MAX];
	u32                         tx_packets[SLSI_LLS_AC_MAX];
	u32                         tx_no_ack[SLSI_LLS_AC_MAX];
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	struct slsi_mutex           scan_mutex;
	struct slsi_mutex           scan_result_mutex;
#else
	/* a std mutex */
	struct mutex                scan_mutex;
	struct mutex                scan_result_mutex;

#endif
	struct slsi_scan            scan[SLSI_SCAN_MAX];

	struct slsi_src_sink_params src_sink_params;
	u16                         power_mode;
	u16                         set_power_mode;

	bool                        activated;      /* VIF is created in firmware and ready to use */
	u16                         vif_type;
	struct slsi_spinlock        peer_lock;
	int                         peer_sta_records;
	struct slsi_peer            *peer_sta_record[SLSI_ADHOC_PEER_CONNECTIONS_MAX];

	/* Used to populate the cfg80211 station_info structure generation variable.
	 * This number should increase every time the list of stations changes
	 * i.e. when a station is added or removed, so that userspace can tell
	 * whether it got a consistent snapshot.
	 */
	int                         cfg80211_sinfo_generation;

	/* Block Ack MPDU Re-order */
	struct slsi_spinlock        ba_lock;
	struct sk_buff_head         ba_complete;
	atomic_t                    ba_flush;

	u64                         mgmt_tx_cookie; /* Cookie id for mgmt tx */
	struct slsi_vif_mgmt_tx     mgmt_tx_data;
	struct delayed_work         scan_timeout_work;     /* Work on scan timeout */
	bool                        delete_probe_req_ies;    /* Delete probe request stored at  probe_req_ies, if
							      * connected for WAP2 at mlme_del_vif or in all cases
							      * if STA
							      */
	u8                          *probe_req_ies;
	size_t                      probe_req_ie_len;

	struct slsi_vif_unsync      unsync;
	struct slsi_vif_sta         sta;
	struct slsi_vif_ap          ap;
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	struct slsi_vif_nan         nan;
#endif
	bool			    mgmt_tx_gas_frame;
	u8			    gas_frame_mac_addr[ETH_ALEN];
	bool                        acs;
	/* TCP ack suppression. */
	struct slsi_spinlock tcp_ack_lock;
	struct slsi_tcp_ack_s *last_tcp_ack;
	struct slsi_tcp_ack_s ack_suppression[TCP_ACK_SUPPRESSION_RECORDS_MAX];
	struct slsi_tcp_ack_stats tcp_ack_stats;
	/* traffic monitor */
	ktime_t last_timer_time;
	u32 report_time;
	u32 num_bytes_tx_per_timer;
	u32 num_bytes_rx_per_timer;
	u32 num_bytes_tx_per_sec;
	u32 num_bytes_rx_per_sec;
	u32 throughput_tx;
	u32 throughput_rx;
	u32 throughput_tx_bps;
	u32 throughput_rx_bps;
#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
	bool enhanced_arp_detect_enabled;
	struct slsi_enhanced_arp_counters enhanced_arp_stats;
	u8 target_ip_addr[4];
	int enhanced_arp_host_tag[SLSI_MAX_ARP_SEND_FRAME];
#endif
#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
	struct cfg80211_ap_settings backup_settings;
#endif
#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
	atomic_t                   arp_tx_count;
#endif
	struct work_struct         sched_scan_stop_wk;
	u8 traffic_mon_state;
	struct work_struct traffic_mon_work;
	struct work_struct         set_multicast_filter_work;
	struct slsi_netif_set_tid_attr set_tid_attr;
	struct work_struct         update_pkt_filter_work;
	bool is_opt_out_packet;
};

struct slsi_802_11d_reg_domain {
	u8                         *countrylist;
	struct ieee80211_regdomain *regdomain;
	int                        country_len;
};

struct slsi_apf_capabilities {
	u16 version;
	u16 max_length;
};

struct slsi_ioctl_args {
	int arg_count;
	u8  *args[];
};

struct slsi_roam_scan_channels {
	int n;
	u8  channels[SLSI_MAX_CHANNEL_LIST];
};

struct slsi_dev_config {
	/* Supported Freq Band (Dynamic)
	 * Set via the freq_band procfs
	 */
#define SLSI_FREQ_BAND_AUTO 0
#define SLSI_FREQ_BAND_5GHZ 1
#define SLSI_FREQ_BAND_2GHZ 2
	int                             supported_band;

	struct ieee80211_supported_band *band_5G;
	struct ieee80211_supported_band *band_2G;

	/* current user suspend mode
	 * Set via the suspend_mode procfs
	 * 0 : not suspended
	 * 1 : suspended
	 */
	int                                     user_suspend_mode;

	/* Rx filtering rule
	 * Set via the rx_filter_num procfs
	 * 0: Unicast, 1: Broadcast, 2:Multicast IPv4, 3: Multicast IPv6
	 */
	int                                     rx_filter_num;

	/* Rx filter rule enabled
	 * Set  via the rx_filter_start & rx_filter_stop procfs
	 */
	bool                                    rx_filter_rule_started;

	/* AP Auto channel Selection */
#define SLSI_NO_OF_SCAN_CHANLS_FOR_AUTO_CHAN_MAX 14
	int                                     ap_auto_chan;

	/*QoS capability for a non-AP Station*/
	int                                     qos_info;
#ifdef CONFIG_SCSC_WLAN_WES_NCHO
	/* NCHO OKC mode */
	int                                     okc_mode;

	/*NCHO WES mode */
	int                                     wes_mode;

	int                                     roam_scan_mode;

	int                                     dfs_scan_mode;

	int                                     ncho_mode;

	/*WES mode roam scan channels*/
	struct slsi_roam_scan_channels          wes_roam_scan_list;
#endif
	struct slsi_802_11d_reg_domain          domain_info;

	struct slsi_roam_scan_channels          legacy_roam_scan_list;

	int                                     ap_disconnect_ind_timeout;

	u8                                      host_state;

	int                                     rssi_boost_5g;
	int                                     rssi_boost_2g;
	bool                                    disable_ch12_ch13;
	bool                                    fw_enhanced_arp_detect_supported;
	bool                                    fw_apf_supported;
	struct                                  slsi_apf_capabilities apf_cap;
};

#define SLSI_DEVICE_STATE_ATTACHING 0
#define SLSI_DEVICE_STATE_STOPPED   1
#define SLSI_DEVICE_STATE_STARTING  2
#define SLSI_DEVICE_STATE_STARTED   3
#define SLSI_DEVICE_STATE_STOPPING  4

#define SLSI_NET_INDEX_WLAN 1
#define SLSI_NET_INDEX_P2P  2
#define SLSI_NET_INDEX_P2PX_SWLAN 3
#define SLSI_NET_INDEX_NAN  4

/* States used during P2P operations */
enum slsi_p2p_states {
	P2P_IDLE_NO_VIF,        /* Initial state - Unsync vif is not present */
	P2P_IDLE_VIF_ACTIVE,    /* Unsync vif is present but no P2P procedure in progress */
	P2P_SCANNING,           /* P2P SOCIAL channel (1,6,11) scan in progress. Not used for P2P full scan */
	P2P_LISTENING,          /* P2P Listen (ROC) in progress */
	P2P_ACTION_FRAME_TX_RX, /* P2P Action frame Tx in progress or waiting for a peer action frame Rx (i.e. in response to the Tx frame) */
	P2P_GROUP_FORMED_CLI,   /* P2P Group Formed - CLI role */
	P2P_GROUP_FORMED_GO,    /* P2P Group Formed - GO role */
	/* NOTE: In P2P_LISTENING state if frame transmission is requested to driver then a peer response is ideally NOT expected.
	 * This is an assumption based on the fact that FIND would be stopped prior to group formation/connection.
	 * If driver were to receive a peer frame in P2P_LISTENING state then it would most probably be a REQUEST frame and the supplicant would respond to it.
	 * Hence the driver should get only RESPONSE frames for transmission in P2P_LISTENING state.
	 */
};

enum slsi_wlan_state {
	WLAN_UNSYNC_NO_VIF = 0,           /* Initial state - Unsync vif is not present */
	WLAN_UNSYNC_VIF_ACTIVE,           /* Unsync vif is activated but no wlan procedure in progress */
	WLAN_UNSYNC_VIF_TX                /* Unsync vif is activated and wlan procedure in progress */
};

/* Wakelock timeouts */
#define SLSI_WAKELOCK_TIME_MSEC_EAPOL (1000)

struct slsi_chip_info_mib {
	u16 chip_version;
};

struct slsi_plat_info_mib {
	u16 plat_build;
};

/* P2P States in text format for debug purposes */
static inline char *slsi_p2p_state_text(u8 state)
{
	switch (state) {
	case P2P_IDLE_NO_VIF:
		return "P2P_IDLE_NO_VIF";
	case P2P_IDLE_VIF_ACTIVE:
		return "P2P_IDLE_VIF_ACTIVE";
	case P2P_SCANNING:
		return "P2P_SCANNING";
	case P2P_LISTENING:
		return "P2P_LISTENING";
	case P2P_ACTION_FRAME_TX_RX:
		return "P2P_ACTION_FRAME_TX_RX";
	case P2P_GROUP_FORMED_CLI:
		return "P2P_GROUP_FORMED_CLI";
	case P2P_GROUP_FORMED_GO:
		return "P2P_GROUP_FORMED_GO";
	default:
		return "UNKNOWN";
	}
}

#define SLSI_WLAN_MAX_HCF_PLATFORM_LEN	   (128)

struct slsi_dev_mib_info {
	char                       *mib_file_name;
	unsigned int               mib_hash;

	/* Cached File MIB Configuration values from User Space */
	u8                         *mib_data;
	u32                         mib_len;
	char			   platform[SLSI_WLAN_MAX_HCF_PLATFORM_LEN];
};

#define SLSI_WLAN_MAX_MIB_FILE     2 /* Number of WLAN HCFs to load */

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
struct slsi_dev_mib_collect_file {
	char		file_name[32];
	u16		len;
	u8		*data;
} __packed;

struct slsi_dev_mib_collect {
	bool		enabled;
	/* Serialize writers/readers */
	spinlock_t	in_collection;
	char			num_files;
	/* +1 represents local_mib */
	struct slsi_dev_mib_collect_file  file[SLSI_WLAN_MAX_MIB_FILE + 1];
};

#endif

struct slsi_dev {
	/* Devices */
	struct device              *dev;
	struct wiphy               *wiphy;

	struct slsi_hip            hip;       /* HIP bookkeeping block */
	struct slsi_hip4           hip4_inst; /* The handler to parse to HIP */

	struct scsc_wifi_cm_if     cm_if;     /* cm_if bookkeeping block */
	struct scsc_mx             *maxwell_core;
	struct scsc_service_client mx_wlan_client;
	struct scsc_service        *service;
	struct slsi_chip_info_mib  chip_info_mib;
	struct slsi_plat_info_mib  plat_info_mib;
	u16                        reg_dom_version;

#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	struct slsi_mutex          netdev_add_remove_mutex;
#else
	/* a std mutex */
	struct mutex               netdev_add_remove_mutex;
#endif
	/* mutex to protect dynamic netdev removal */
	struct mutex               netdev_remove_mutex;
	int                        netdev_up_count;
	struct net_device          __rcu *netdev[CONFIG_SCSC_WLAN_MAX_INTERFACES + 1];               /* 0 is reserved */
	struct net_device          __rcu *netdev_ap;
	u8                         netdev_addresses[CONFIG_SCSC_WLAN_MAX_INTERFACES + 1][ETH_ALEN];  /* 0 is reserved */
	bool                       require_vif_delete[CONFIG_SCSC_WLAN_MAX_INTERFACES + 1];
	int                        device_state;

	/* traffic mon */
	u32				           agg_dev_throughput_tx;
	u32				           agg_dev_throughput_rx;

	/* BoT */
	atomic_t                   in_pause_state;
	struct work_struct recovery_work_on_stop;   /* Work on failure_reset recovery*/
#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
	struct work_struct recovery_work;   /* Work on subsystem_reset recovery*/
	struct work_struct recovery_work_on_start;   /* Work on chip recovery*/
#endif
	/* Locking used to control Starting and stopping the chip */
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	struct slsi_mutex          start_stop_mutex;
#else
	/* a std mutex */
	struct mutex               start_stop_mutex;
#endif
	/* UDI Logging */
	struct slsi_log_clients    log_clients;
	void                       *uf_cdev;

	/* ProcFS */
	int                        procfs_instance;
	struct proc_dir_entry      *procfs_dir;

	/* Configuration */
	u8                         hw_addr[ETH_ALEN];
	struct slsi_dev_mib_info   mib[SLSI_WLAN_MAX_MIB_FILE];
	struct slsi_dev_mib_info   local_mib;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	struct slsi_dev_mib_collect  collect_mib;
#endif
	char                       *maddr_file_name;
	bool                       *term_udi_users;   /* Try to terminate UDI users during unload */
	int                        *sig_wait_cfm_timeout;

#ifdef CONFIG_SCSC_WLAN_ANDROID
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock			wlan_wl;
	struct scsc_wake_lock			wlan_wl_mlme;
	struct scsc_wake_lock			wlan_wl_ma;
	struct scsc_wake_lock			wlan_wl_roam;
#else
	struct wake_lock                        wlan_wl;
        struct wake_lock                        wlan_wl_mlme;
        struct wake_lock                        wlan_wl_ma;
        struct wake_lock                        wlan_wl_roam;
#endif
#endif
	struct slsi_sig_send       sig_wait;
	struct slsi_skb_work       rx_dbg_sap;
	atomic_t                   tx_host_tag[SLSI_LLS_AC_MAX];
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	struct slsi_mutex          device_config_mutex;
#else
	/* a std mutex */
	struct mutex               device_config_mutex;
#endif
	struct slsi_dev_config     device_config;

	struct notifier_block      inetaddr_notifier;
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	struct notifier_block      inet6addr_notifier;
#endif

	struct workqueue_struct    *device_wq;              /* Driver Workqueue */
	enum slsi_p2p_states       p2p_state;               /* Store current P2P operation */

	enum slsi_wlan_state        wlan_unsync_vif_state; /* Store current sate of unsync wlan vif */

	int                        current_tspec_id;
	int                        tspec_error_code;
	u8                         p2p_group_exp_frame;        /* Next expected Public action frame subtype from peer */
	bool                       initial_scan;

#ifdef CONFIG_SCSC_WLAN_GSCAN_ENABLE
	struct slsi_gscan          *gscan;
	struct slsi_gscan_result   *gscan_hash_table[SLSI_GSCAN_HASH_TABLE_SIZE];
	int                        num_gscan_results;
	int                        buffer_threshold;
	int                        buffer_consumed;
	struct slsi_bucket         bucket[SLSI_GSCAN_MAX_BUCKETS];
	struct list_head           hotlist_results;
	bool                       epno_active;
#endif
#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
	u8                         scan_mac_addr[6];
	bool                       scan_addr_set;
#endif
#ifdef CONFIG_SCSC_WLAN_HIP4_PROFILING
	int                        minor_prof;
#endif
	struct slsi_ba_session_rx  rx_ba_buffer_pool[SLSI_MAX_RX_BA_SESSIONS];
	struct slsi_spinlock       rx_ba_buffer_pool_lock;
	bool			   fail_reported;
	bool			   p2p_certif; /* Set to true to idenitfy p2p_certification testing is going on*/
	bool                       mlme_blocked; /* When true do not send mlme signals to FW */
	atomic_t		   debug_inds;
	int                        recovery_next_state;
	struct completion          recovery_remove_completion;
	struct completion          recovery_stop_completion;
	struct completion          recovery_completed;
	int                        recovery_status;
	struct slsi_ssid_map       ssid_map[SLSI_SCAN_SSID_MAP_MAX];
	bool                       band_5g_supported;
	int                        supported_2g_channels[14];
	int                        supported_5g_channels[25];
	int                        enabled_channel_count;
	bool                       fw_ht_enabled;
	u8                         fw_ht_cap[4]; /* HT capabilities is 21 bytes but host is never intersted in last 17 bytes*/
	bool                       fw_vht_enabled;
	u8                         fw_vht_cap[4];
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	u8                         wifi_sharing_5ghz_channel[8];
	int                        valid_5g_chan[25];
	int                        wifi_sharing_5g_restricted_channels[25];
	int                        num_5g_restricted_channels;
#endif
	bool                       fw_SoftAp_2g_40mhz_enabled;
	bool                       nan_enabled;
	u16                        assoc_result_code; /* Status of latest association in STA mode */
	bool                       allow_switch_40_mhz; /* Used in AP cert to disable HT40 when not configured */
	bool                       allow_switch_80_mhz; /* Used in AP cert to disable VHT when not configured */
#ifdef CONFIG_SCSC_WLAN_AP_INFO_FILE
	/* Parameters in '/data/vendor/conn/.softap.info' */
	bool                       dualband_concurrency;
	u32                        softap_max_client;
#endif
	u32                        fw_dwell_time;
	int                        lls_num_radio;

#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	struct slsi_mutex          logger_mutex;
#else
	/* a std mutex */
	struct mutex               logger_mutex;
#endif
	struct slsi_traffic_mon_clients    traffic_mon_clients;
	/*Structure to map rtt peers for each rtt_id*/
	struct slsi_rtt_id_params           *rtt_id_params[SLSI_MAX_RTT_ID];
	bool                            acs_channel_switched;
	int                        recovery_timeout; /* ms autorecovery completion timeout */
#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
	bool                            fw_mac_randomization_enabled;
#endif

#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
	bool                       enhanced_pkt_filter_enabled;
#endif
	struct reg_database regdb;
	u8 forced_bandwidth;
	bool require_service_close;
	bool                       mac_changed;
#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
	atomic_t                   ctrl_pause_state;
	u16                        fw_max_arp_count;
	atomic_t                   arp_tx_count;
#endif
	u8                         fw_ext_cap_ie[9]; /*extended capability IE length is 9 */
	u32                        fw_ext_cap_ie_len;
	struct slsi_spinlock       netdev_lock;
	int                        home_time;
	int                        home_away_time;
	int                        max_channel_time;
	int                        max_channel_passive_time;
	int                        wlan_service_on;
};

/* Compact representation of channels a ESS has been seen on
 * This is sized correctly for the Channels we currently support,
 * 2.4Ghz Channels 1 - 14
 * 5  Ghz Channels Uni1, Uni2 and Uni3
 */
struct slsi_roaming_network_map_entry {
	struct list_head     list;
	unsigned long        last_seen_jiffies;			/* Timestamp of the last time we saw this ESS */
	struct cfg80211_ssid ssid;						/* SSID of the ESS */
	u8                   initial_bssid[ETH_ALEN];	/* Bssid of the first ap seen in this ESS */
	bool                 only_one_ap_seen;			/* Has more than one AP for this ESS been seen */
	u16                  channels_24_ghz;			/* 2.4 Ghz Channels Bit Map */
	/* 5 Ghz Channels Bit Map
	 * channels_5_ghz & 0x000000FF =  4 Uni1 Channels
	 * channels_5_ghz & 0x00FFFF00 = 15 Uni2 Channels
	 * channels_5_ghz & 0xFF000000 =  5 Uni3 Channels
	 */
	u32                  channels_5_ghz;
	unsigned long        channel_jiffies[39];
};

#define LLC_SNAP_HDR_LEN 8
struct llc_snap_hdr {
	u8 llc_dsap;
	u8 llc_ssap;
	u8 llc_ctrl;
	u8 snap_oui[3];
	u16 snap_type;
} __packed;

void slsi_rx_data_deliver_skb(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, bool ctx_napi);
void slsi_rx_dbg_sap_work(struct work_struct *work);
void slsi_rx_netdev_data_work(struct work_struct *work);
void slsi_rx_netdev_mlme_work(struct work_struct *work);
int slsi_rx_enqueue_netdev_mlme(struct slsi_dev *sdev, struct sk_buff *skb, u16 vif);
struct ieee80211_channel *slsi_rx_scan_pass_to_cfg80211(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_rx_buffered_frames(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer);
int slsi_rx_blocking_signals(struct slsi_dev *sdev, struct sk_buff *skb);
void slsi_scan_complete(struct slsi_dev *sdev, struct net_device *dev, u16 scan_id, bool aborted,
			bool flush_scan_results);
void slsi_tx_pause_queues(struct slsi_dev *sdev);
void slsi_tx_unpause_queues(struct slsi_dev *sdev);
int slsi_tx_control(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_tx_data(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_tx_data_lower(struct slsi_dev *sdev, struct sk_buff *skb);
bool slsi_is_test_mode_enabled(void);
bool slsi_is_rf_test_mode_enabled(void);
int slsi_check_rf_test_mode(void);
void slsi_init_netdev_mac_addr(struct slsi_dev *sdev);
bool slsi_dev_lls_supported(void);
bool slsi_dev_gscan_supported(void);
bool slsi_dev_epno_supported(void);
bool slsi_dev_vo_vi_block_ack(void);
int slsi_dev_get_scan_result_count(void);
bool slsi_dev_llslogs_supported(void);
int slsi_dev_nan_supported(struct slsi_dev *sdev);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
bool slsi_dev_nan_is_ipv6_link_tlv_include(void);
int slsi_get_nan_max_ndp_instances(void);
int slsi_get_nan_max_ndi_ifaces(void);
int slsi_get_nan_ndp_delay(void);
int slsi_get_nan_ndp_max_time(void);
#endif
void slsi_sched_scan_stopped(struct work_struct *work);
bool slsi_dev_rtt_supported(void);

static inline u16 slsi_tx_host_tag(struct slsi_dev *sdev, enum slsi_traffic_q tq)
{
	u16 host_tag = 0;

	/* host_tag:
	 * bit 0,1 = trafficqueue identifier
	 * bit 2-14 = incremental number
	 * So increment by 4 to get bit 2-14 a incremental sequence
	 * bit 15 is used for ARP flow control.
	 */
	host_tag = (u16)atomic_add_return(4, &sdev->tx_host_tag[tq]);
	host_tag &= ~SLSI_HOST_TAG_ARP_MASK;

	return host_tag;
}

static inline u16 slsi_tx_mgmt_host_tag(struct slsi_dev *sdev)
{
	/* Doesn't matter which traffic queue host tag is selected.*/
	return slsi_tx_host_tag(sdev, 0);
}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
static inline struct net_device *slsi_nan_get_netdev_rcu(struct slsi_dev *sdev, struct sk_buff *skb)
{
	u8 bcast_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u8 *src_addr = NULL;
	u8 *dest_addr = NULL;
	u8 idx = 0;

	WARN_ON(!rcu_read_lock_held());

	switch (fapi_get_sigid(skb)) {
	case MA_BLOCKACK_IND:
		{
			if (fapi_get_u16(skb, u.ma_blockack_ind.reason_code) == FAPI_REASONCODE_UNSPECIFIED_REASON) {
				struct ieee80211_bar *bar = (fapi_get_datalen(skb)) ? (struct ieee80211_bar *)fapi_get_data(skb): NULL;

				if (!bar)
					return NULL;
				dest_addr = bar->ra;
			} else {
				struct ieee80211_mgmt *mgmt = (fapi_get_mgmtlen(skb)) ? fapi_get_mgmt(skb): NULL;

				if (!mgmt) {
					dest_addr = bcast_addr;
					src_addr = fapi_get_buff(skb, u.ma_blockack_ind.peer_qsta_address);
				} else {
					dest_addr = mgmt->da;
				}
			}
		}
		break;
	case MA_UNITDATA_IND:
		{
			struct ethhdr *eth_hdr = (struct ethhdr *)fapi_get_data(skb);
			u32 data_len = fapi_get_datalen(skb);

			if (!eth_hdr || data_len < sizeof(*eth_hdr)) {
				WARN(1, "invalid len:%d\n", data_len);
				return NULL;
			}

			src_addr = eth_hdr->h_source;
			dest_addr = eth_hdr->h_dest;
		}
		break;
	default:
		WARN(1, "Unhandled signal: 0x%.4x\n", fapi_get_sigid(skb));
		return NULL;
	}


	for (idx = SLSI_NAN_DATA_IFINDEX_START; idx <= CONFIG_SCSC_WLAN_MAX_INTERFACES; idx++) {
		struct netdev_vif *ndev_vif;
		u8 i = 0;

		if (sdev->netdev[idx]) {
			/*
			 * In NAN, the 1:1 mapping of VIF to Netdevice does not apply.
			 * The same firmware VIF may map to multiple netdevices. So derive
			 * the Netdev here by matching the destination address of packet
			 * to address of the Netdev.
			 *
			 * The rule above can't apply to multicast. So if it is multicast,
			 * loop through the netdevices and it's peer records and find a
			 * match of the source address to derive the netdev.
			 */
			if (dest_addr && is_multicast_ether_addr(dest_addr)) {
				ndev_vif = (struct netdev_vif *)netdev_priv(sdev->netdev[idx]);
				slsi_spinlock_lock(&ndev_vif->peer_lock);
				for (i = 0; i < SLSI_PEER_INDEX_MAX; i++) {
					if (ndev_vif->peer_sta_record[i] &&
					    ndev_vif->peer_sta_record[i]->valid &&
					    src_addr &&
					    ether_addr_equal(ndev_vif->peer_sta_record[i]->address, src_addr)) {
						slsi_spinlock_unlock(&ndev_vif->peer_lock);
						return rcu_dereference(sdev->netdev[idx]);
					}
				}
				slsi_spinlock_unlock(&ndev_vif->peer_lock);
			} else if (dest_addr && ether_addr_equal(dest_addr, sdev->netdev[idx]->dev_addr)) {
				return rcu_dereference(sdev->netdev[idx]);
			}
		}
	}
	return NULL;
}
#endif

static inline struct net_device *slsi_get_netdev_rcu(struct slsi_dev *sdev, u16 ifnum)
{
	WARN_ON(!rcu_read_lock_held());
	if (ifnum > CONFIG_SCSC_WLAN_MAX_INTERFACES) {
		/* WARN(1, "ifnum:%d", ifnum);  WARN() is used like this to avoid Coverity Error */
		return NULL;
	}
	return rcu_dereference(sdev->netdev[ifnum]);
}

static inline struct net_device *slsi_get_netdev_locked(struct slsi_dev *sdev, u16 ifnum)
{
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(sdev->netdev_add_remove_mutex));
	if (ifnum > CONFIG_SCSC_WLAN_MAX_INTERFACES) {
		WARN(1, "ifnum:%d", ifnum); /* WARN() is used like this to avoid Coverity Error */
		return NULL;
	}
	return sdev->netdev[ifnum];
}

static inline struct net_device *slsi_get_netdev(struct slsi_dev *sdev, u16 ifnum)
{
	struct net_device *dev;

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	dev = slsi_get_netdev_locked(sdev, ifnum);
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	return dev;
}

static inline struct net_device *slsi_get_netdev_by_mac_addr(struct slsi_dev *sdev, u8 *mac_addr, int start_idx)
{
	int i;

	if (!start_idx)
		start_idx = 1;
	for (i = start_idx; i < CONFIG_SCSC_WLAN_MAX_INTERFACES + 1; i++) {
		if (sdev->netdev[i] && ether_addr_equal(mac_addr, sdev->netdev[i]->dev_addr))
			return sdev->netdev[i];
	}
	return NULL;
}

static inline struct net_device *slsi_get_netdev_by_mac_addr_locked(struct slsi_dev *sdev, u8 *mac_addr, int start_idx)
{
	struct net_device *dev;

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	dev = slsi_get_netdev_by_mac_addr(sdev, mac_addr, start_idx);
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	return dev;
}

static inline struct net_device *slsi_get_netdev_by_ifname_locked(struct slsi_dev *sdev, u8 *ifname)
{
	int i;

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(sdev->netdev_add_remove_mutex));
	for (i = 1; i < CONFIG_SCSC_WLAN_MAX_INTERFACES + 1; i++) {
		if (sdev->netdev[i] && strcmp(ifname, sdev->netdev[i]->name) == 0)
			return sdev->netdev[i];
	}
	return NULL;
}

static inline struct net_device *slsi_get_netdev_by_ifname(struct slsi_dev *sdev, u8 *ifname)
{
	struct net_device *dev;

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	dev = slsi_get_netdev_by_ifname_locked(sdev, ifname);
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	return dev;
}

static inline int slsi_get_supported_mode(const u8 *peer_ie)
{
	const u8             *peer_ie_data;
	u8                   peer_ie_len;
	int                  i;
	int                  supported_rate;

	peer_ie_len = peer_ie[1];
	peer_ie_data = &peer_ie[2];
	for (i = 0; i < peer_ie_len; i++) {
		supported_rate = ((peer_ie_data[i] & 0x7F) / 2);
		if (supported_rate > 11)
			return SLSI_80211_MODE_11G;
	}
	return SLSI_80211_MODE_11B;
}

/* Names of full mode HCF files */
extern char *slsi_mib_file;
extern char *slsi_mib_file2;

#ifdef CONFIG_SCSC_WLAN_NW_PKT_DROP
void bypass_backlog(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
#endif

#endif
