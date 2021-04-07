/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include <linux/spinlock.h>
#include <asm/atomic.h>
#include "netif.h"

#ifndef __SCSC_WIFI_FCQ_H
#define __SCSC_WIFI_FCQ_H

enum scsc_wifi_fcq_8021x_state {
	SCSC_WIFI_FCQ_8021x_STATE_BLOCKED = 0,
	SCSC_WIFI_FCQ_8021x_STATE_OPEN = 1
};

enum scsc_wifi_fcq_ps_state {
	SCSC_WIFI_FCQ_PS_STATE_POWERSAVE = 1,
	SCSC_WIFI_FCQ_PS_STATE_ACTIVE = 2
};

struct scsc_wifi_fcq_q_stat {
	u32 netq_stops;
	u32 netq_resumes;
	u32 netq_stop_percent;
	u32 netq_stop_time_in_ms;
	ktime_t last_sample_time;
	ktime_t last_stop_time;
	bool netq_state;
};

struct scsc_wifi_fcq_q_header {
	atomic_t                    qmod;
	atomic_t                    qcod;
	u16                         netif_queue_id;
	atomic_t                    active;
	struct scsc_wifi_fcq_q_stat stats;
	ktime_t                     empty_t;
	bool                        can_be_distributed;
	bool                        saturated;
	int			    guard;
};

struct scsc_wifi_fcq_data_q {
	struct scsc_wifi_fcq_q_header  head;
	struct scsc_wifi_fcq_data_qset *qs;
};

struct scsc_wifi_fcq_ctrl_q {
	struct scsc_wifi_fcq_q_header head;
};

enum scsc_wifi_fcq_queue_set_type {
	SCSC_WIFI_FCQ_QUEUE_SET_TYPE_UNICAST = 0,
	SCSC_WIFI_FCQ_QUEUE_SET_TYPE_MULTICAST = 1
};

#define EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION	1

struct scsc_wifi_fcq_data_qset {
	atomic_t                       active;
	enum scsc_wifi_fcq_8021x_state controlled_port_state;
	/* a std spinlock */
	spinlock_t                     cp_lock;

	struct scsc_wifi_fcq_data_q    ac_q[SLSI_NETIF_Q_PER_PEER];
	/* Control AC usage (BE,BK,VI,VO) bitmap*/
	u8                             ac_inuse;
	atomic_t                       smod;
	atomic_t                       scod;

	struct scsc_wifi_fcq_q_stat    stats; /* Stats for smod */
	enum scsc_wifi_fcq_ps_state    peer_ps_state;
	u32                            peer_ps_state_transitions;

#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
	ktime_t                     empty_t;
	bool                        can_be_distributed;
	bool                        in_sleep;
#endif
	bool                        saturated;
	int                         guard;
};

/* Queue and queue set management */
int scsc_wifi_fcq_ctrl_q_init(struct scsc_wifi_fcq_ctrl_q *queue);
void scsc_wifi_fcq_ctrl_q_deinit(struct scsc_wifi_fcq_ctrl_q *queue);

int scsc_wifi_fcq_unicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u8 qs_num, struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer);
int scsc_wifi_fcq_multicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u8 vif);
void scsc_wifi_fcq_qset_deinit(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer);

/* Transmit/receive bookkeeping and smod power save changes / 802.1x handling */
int scsc_wifi_fcq_transmit_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index);
int scsc_wifi_fcq_receive_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index);
int scsc_wifi_fcq_receive_data_no_peer(struct net_device *dev, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index);

void scsc_wifi_fcq_pause_queues(struct slsi_dev *sdev);
#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
void scsc_wifi_pause_arp_q_all_vif(struct slsi_dev *sdev);
void scsc_wifi_unpause_arp_q_all_vif(struct slsi_dev *sdev);
#endif

void scsc_wifi_fcq_unpause_queues(struct slsi_dev *sdev);

int scsc_wifi_fcq_transmit_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue);
int scsc_wifi_fcq_receive_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue);

int scsc_wifi_fcq_update_smod(struct scsc_wifi_fcq_data_qset *qs, enum scsc_wifi_fcq_ps_state peer_ps_state,
			      enum scsc_wifi_fcq_queue_set_type type);
int scsc_wifi_fcq_8021x_port_state(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, enum scsc_wifi_fcq_8021x_state state);

/* Statistics */
int scsc_wifi_fcq_stat_queue(struct scsc_wifi_fcq_q_header *queue,
			     struct scsc_wifi_fcq_q_stat *queue_stat,
			     int *qmod, int *qcod);

int scsc_wifi_fcq_stat_queueset(struct scsc_wifi_fcq_data_qset *queue_set,
				struct scsc_wifi_fcq_q_stat *queue_stat,
				int *smod, int *scod, enum scsc_wifi_fcq_8021x_state *cp_state,
				u32 *peer_ps_state_transitions);

#endif /* #ifndef __SCSC_WIFI_FCQ_H */
