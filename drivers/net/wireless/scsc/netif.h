/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "dev.h"

#ifndef __SLSI_NETIF_H__
#define __SLSI_NETIF_H__

/* net_device queues
 * ---------------------------------------------
 *	1 Queue for Security frames (EAPOL, WAPI etc)
 *	4 Queues for Broadcast/Multicast when in AP mode.
 *	1 Queue for ARP
 *	4 Queues per Peer
 *
 *	STA/ADHOC
 *	Queues
 *	-------------------------------------------------------------
 *	|    0    |    1    |  2 - 5  |  6  |  7  |  8  |  9  |  10 |
 *	-------------------------------------------------------------
 *	| Eapol   | Discard |   Not   | ARP | AC  | AC  | AC  | AC  |
 *	| Frames  | Queue   |   Used  |     |  0  |  1  |  2  |  3  |
 *	-------------------------------------------------------------
 *
 *	AP
 *	Queues
 *	                                                                        ------------------------------------------------
 *	                                                                        | Peer1 ACs(0 - 4)  | Peer2 ACs(0 - 4)  | ......
 *	------------------------------------------------------------------------------------------------------------------------
 *	|    0   |    1    |    2    |    3    |    4    |    5    |  6  |  7  | 8  | 9  | 10 | 11 | 12 | 13 | 14 | ......
 *	------------------------------------------------------------------------------------------------------------------------
 *	| Eapol  | Discard |B/M Cast |B/M Cast |B/M Cast |B/M Cast | ARP | AC  | AC | AC | AC | AC | AC | AC | AC | ......
 *	| Frames |  Queue  |  AC 0   |  AC 1   |  AC 2   |  AC 3   |     |  0  |  1 |  2 |  3 |  0 |  1 |  2 |  3 | ......
 *	------------------------------------------------------------------------------------------------------------------------
 */

#ifndef ETH_P_PAE
#define ETH_P_PAE 0x888e
#endif
#ifndef ETH_P_WAI
#define ETH_P_WAI 0x88b4
#endif

#define SLSI_NETIF_Q_PRIORITY         0
#define SLSI_NETIF_Q_DISCARD          1
#define SLSI_NETIF_Q_MULTICAST_START  2
#define SLSI_NETIF_Q_ARP              6
#define SLSI_NETIF_Q_PEER_START       7

#define SLSI_NETIF_Q_PER_PEER   4

/* sizeof ma_unitdata_req [36] + pad [30] + pad_words [2]  */
#define SLSI_NETIF_SKB_HEADROOM (68 + 160)
#define SLSI_NETIF_SKB_TAILROOM 0

static inline u16 slsi_netif_get_peer_queue(s16 queueset, s16 ac)
{
	WARN_ON(ac > SLSI_NETIF_Q_PER_PEER);
	return SLSI_NETIF_Q_PEER_START + (queueset * SLSI_NETIF_Q_PER_PEER) + ac;
}

/* queueset is one less than the assigned aid. */
static inline unsigned short slsi_netif_get_qs_from_queue(short queue, short ac)
{
	return ((queue - ac - SLSI_NETIF_Q_PEER_START) / SLSI_NETIF_Q_PER_PEER);
}

static inline u16 slsi_netif_get_multicast_queue(s16 ac)
{
	WARN_ON(ac > SLSI_NETIF_Q_PER_PEER);
	return SLSI_NETIF_Q_MULTICAST_START + ac;
}

#define MAP_QS_TO_AID(qs) (qs + 1)
#define MAP_AID_TO_QS(aid) (aid - 1)

enum slsi_traffic_q {
	SLSI_TRAFFIC_Q_BE = 0,
	SLSI_TRAFFIC_Q_BK,
	SLSI_TRAFFIC_Q_VI,
	SLSI_TRAFFIC_Q_VO,
};

enum slsi_traffic_q slsi_frame_priority_to_ac_queue(u16 priority);
int slsi_ac_to_tids(enum slsi_traffic_q ac, int *tids);

struct slsi_dev;
struct slsi_peer;

int slsi_netif_init(struct slsi_dev *sdev);
/* returns the index or -E<error> code */
int slsi_netif_dynamic_iface_add(struct slsi_dev *sdev, const char *name);
int slsi_netif_register(struct slsi_dev *sdev, struct net_device *dev);
int slsi_netif_register_rtlnl_locked(struct slsi_dev *sdev, struct net_device *dev);
void slsi_netif_remove(struct slsi_dev *sdev, struct net_device *dev);
void slsi_netif_remove_rtlnl_locked(struct slsi_dev *sdev, struct net_device *dev);
void slsi_netif_remove_all(struct slsi_dev *sdev);
void slsi_netif_deinit(struct slsi_dev *sdev);
void slsi_tdls_move_packets(struct slsi_dev *sdev, struct net_device *dev,
			    struct slsi_peer *sta_peer, struct slsi_peer *tdls_peer, bool connection);
void slsi_netif_remove_locked(struct slsi_dev *sdev, struct net_device *dev);
int slsi_netif_add_locked(struct slsi_dev *sdev, const char *name, int ifnum);
int slsi_netif_register_locked(struct slsi_dev *sdev, struct net_device *dev);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
void slsi_net_randomize_nmi_ndi(struct slsi_dev *sdev);
#endif
int slsi_netif_set_tid_config(struct slsi_dev *sdev, struct net_device *dev, u8 mode, u32 uid, u8 tid);
#endif /*__SLSI_NETIF_H__*/
