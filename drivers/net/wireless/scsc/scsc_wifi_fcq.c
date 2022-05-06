/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include "scsc_wifi_fcq.h"
#include "debug.h"
#include "dev.h"
#include "hip4_sampler.h"

/* Queues hierarchy and control domains
 *
 *             wlan             p2p                p2pX
 *              |		 |                   |
 *              |		 |                   |
 *              |		 |                   |
 *              |		 |                   |
 *              --------------------------------------
 *                               |
 *                              \                                  Global domain
 *                               |
 *                               |
 *              ----------------------------------------
 *              |		 |         ...          |
 *              |		 |         ...          |          Smod Domain (vid, peer_id)
 *             \                \          ...         \
 *              |		 |         ...          |
 *              |		 |         ...          |
 *        ------------------------------------------------------
 *        |   |   |   |    |   |   |   |   ...     |   |   |   |
 *       \   \   \   \    \   \   \   \    ...    \   \   \   \    Qmod Domain
 *        |   |   |   |    |   |   |   |   ...     |   |   |   |
 *        -----------------------------------------------------
 *       vi  vo  bk  be   vi  vo  bk  be          vi  vo  bk  be
 */

static uint scsc_wifi_fcq_smod = 512;
module_param(scsc_wifi_fcq_smod, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_smod, "Initial value of unicast smod - peer normal (default = 500)");

static uint scsc_wifi_fcq_mcast_smod = 100;
module_param(scsc_wifi_fcq_mcast_smod, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_mcast_smod, "Initial value of multicast smod - peer normal (default = 100)");

static uint scsc_wifi_fcq_smod_power = 4;
module_param(scsc_wifi_fcq_smod_power, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_smod_power, "Initial powersave SMOD value - peer powersave (default = 4)");

static uint scsc_wifi_fcq_mcast_smod_power = 4;
module_param(scsc_wifi_fcq_mcast_smod_power, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_mcast_smod_power, "Initial value of powersave multicast smod - peer normal (default = 4)");

static uint scsc_wifi_fcq_qmod = 512;
module_param(scsc_wifi_fcq_qmod, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_qmod, "Initial value of unicast qmod - peer normal (default = 500)");

static uint scsc_wifi_fcq_mcast_qmod = 100;
module_param(scsc_wifi_fcq_mcast_qmod, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_mcast_qmod, "Initial value of multicast qmod - peer normal (default = 100)");

static uint scsc_wifi_fcq_minimum_smod = 50;
module_param(scsc_wifi_fcq_minimum_smod, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_minimum_smod, "Initial value of minimum smod - peer normal (default = 50)");

static uint scsc_wifi_fcq_distribution_delay_ms;
module_param(scsc_wifi_fcq_distribution_delay_ms, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_wifi_fcq_distribution_delay_ms, "Distribution time in ms (default = 0)");

#define SCSC_WIFI_FCQ_SMOD_RESUME_HYSTERESIS 10
#define SCSC_WIFI_FCQ_QMOD_RESUME_HYSTERESIS 10
#define SCSC_WIFI_FCQ_GMOD_RESUME_HYSTERESIS 30

/* Protection guard before reaching Stop queues. */
#define STOP_GUARD_GMOD		10
#define STOP_GUARD_SMOD		1
#define STOP_GUARD_QMOD		2

/* CLOSE_IN_OVERSHOOT could close the overshooted queues quickly, however could lead to ENOSPC on */
/* a multithreaded environment */
/* #define CLOSE_IN_OVERSHOOT      1 */

/* To optimally start/stop global queues */
struct peers_cache {
	struct list_head list;
	struct net_device *dev;
	int vif;
	int peer_index;
	struct scsc_wifi_fcq_data_qset *qs;
	bool is_unicast;
};

static LIST_HEAD(peers_cache_list);
static DEFINE_SPINLOCK(peers_cache_lock);

/* AC qmod mapping */
/* 0 - indicates not active */
/* > 0 - indicates active and the qmod value */
static u32 ac_q_layout[8][4] = {
	{   0,   0,   0, 1000},
	{   0,   0, 500,  500},
	{   0, 500,   0,  500},
	{   0, 333, 333,  333},
	{ 500,   0,   0,  500},
	{ 333,   0, 333,  333},
	{ 333, 333,   0,  333},
	{ 250, 250, 250,  250},
};

/* Setting ENABLE_QCOD will include a second layer of flow control calculation for a specific queue.
 * #define ENABLE_QCOD 1
 */
#define ENABLE_QCOD 1
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
/* Global debug counters */
#define DOMAINS		3
#define DOMAIN_G	0
#define DOMAIN_S	1
#define DOMAIN_Q	2

#define DIREC		2
#define DIREC_TX	0
#define DIREC_RX	1

#define AC_Q            4

static int td[DIREC][DOMAINS][AC_Q];

static inline void fcq_update_counters(int direction, int domain, int ac)
{
	td[direction][domain][ac] = td[direction][domain][ac] + 1;
}
#endif

/* Setting ENABLE_CTRL_FCQ will include flow control on control queues.
 * #define ENABLE_CTRL_FCQ 1
 */

/* POC */
static int total;

#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
/* Need to track the number of peer in smod */
static int total_in_sleep;
#endif

static inline bool is_gmod_active(struct slsi_dev *sdev)
{
	return atomic_read(&sdev->hip4_inst.hip_priv->gactive);
}

static inline bool is_smod_active(struct scsc_wifi_fcq_data_qset *qs)
{
	return atomic_read(&qs->active);
}

static inline bool is_qmod_active(struct scsc_wifi_fcq_data_q *queue)
{
	return atomic_read(&queue->head.active);
}

static inline bool is_in_pause(struct slsi_dev *sdev)
{
	return atomic_read(&sdev->in_pause_state);
}

static inline void fcq_netq_start_stop_sample(struct scsc_wifi_fcq_q_header *queue, bool new_state)
{
	struct scsc_wifi_fcq_q_stat *queue_stat;
	u32 delta_in_ms = 0;

	if (WARN_ON(!queue))
		return;

	queue_stat = &queue->stats;

	if (ktime_to_ms(queue_stat->last_sample_time) == 0)
		queue_stat->last_sample_time = ktime_get();

	/* make sure it's a transition of state */
	if (queue_stat->netq_state == new_state)
		return;

	queue_stat->netq_state = new_state;

	/* Action on state change
	 * resume queue: update cumulative time for Net queue Stop
	 * stop queue: start sampling Stop time
	 */
	if (new_state)
		queue_stat->netq_stop_time_in_ms += ktime_to_ms(ktime_sub(ktime_get(), queue_stat->last_stop_time));
	else
		queue_stat->last_stop_time = ktime_get();

	/* check time elapsed; if it is more than 1 second since last sample, calculate %ge */
	if (ktime_to_ms(ktime_sub(ktime_get(), queue_stat->last_sample_time)) > 1000) {
		/* Compute the delta time (difference in seconds from the last update) -- should always be > 1s */
		delta_in_ms = ktime_ms_delta(ktime_get(), queue_stat->last_sample_time);

		/* if delta is more than 2 seconds, then it is a stale value from an old sample; ignore */
		if (delta_in_ms < 2000)
			queue_stat->netq_stop_percent = ((queue_stat->netq_stop_time_in_ms * 100) / delta_in_ms);
		else
			queue_stat->netq_stop_percent = 0;

		queue_stat->netq_stop_time_in_ms = 0;
		queue_stat->last_sample_time = ktime_get();
	}
}

/* Should be called from locked context */
static inline void fcq_stop_all_queues(struct slsi_dev *sdev)
{
	int i;
	struct peers_cache *pc_node, *next;
	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		/* Stop queues all queues */
		for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++) {
			SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "fcq_stop_all_queues vif %d peer_index %d ac %d\n", pc_node->vif, pc_node->peer_index, i);
			netif_stop_subqueue(pc_node->dev, pc_node->qs->ac_q[i].head.netif_queue_id);
		}
	}
	spin_unlock_bh(&peers_cache_lock);
}

/* Should be called from locked context */
static inline void fcq_wake_all_queues(struct slsi_dev *sdev)
{
	int i;
	struct peers_cache *pc_node, *next;

	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		/* Wake queues that reported to be active, leave stopped the others. Do not wake queues in pause state */
		for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++) {
			if (is_gmod_active(sdev) && is_smod_active(pc_node->qs) && is_qmod_active(&pc_node->qs->ac_q[i]) && !is_in_pause(sdev)) {
				SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "fcq_wake_all_queues vif %d peer_index %d ac %d\n", pc_node->vif, pc_node->peer_index, i);
				netif_wake_subqueue(pc_node->dev, pc_node->qs->ac_q[i].head.netif_queue_id);
			} else {
				SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "fcq_wake_all_queues vif %d peer_index %d ac %d not woken up!\n", pc_node->vif, pc_node->peer_index, i);
			}
		}
	}
	spin_unlock_bh(&peers_cache_lock);
}

void scsc_wifi_fcq_pause_queues(struct slsi_dev *sdev)
{
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Pause queues\n");
	atomic_set(&sdev->in_pause_state, 1);
	fcq_stop_all_queues(sdev);
}

void scsc_wifi_fcq_unpause_queues(struct slsi_dev *sdev)
{
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Unpause queues\n");
	atomic_set(&sdev->in_pause_state, 0);
	fcq_wake_all_queues(sdev);
}

#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
static inline void fcq_stop_arp_q_all_vif(struct slsi_dev *sdev)
{
	struct peers_cache *pc_node, *next;

	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		netif_stop_subqueue(pc_node->dev, SLSI_NETIF_Q_ARP);
		SLSI_INFO(sdev, "ARP_q stop for %s\n", pc_node->dev->name);
	}
	spin_unlock_bh(&peers_cache_lock);
}

static inline void fcq_wake_arp_q_all_vif(struct slsi_dev *sdev)
{
	struct peers_cache *pc_node, *next;

	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		netif_wake_subqueue(pc_node->dev, SLSI_NETIF_Q_ARP);
		SLSI_INFO(sdev, "ARP_q wake for %s\n", pc_node->dev->name);
	}
	spin_unlock_bh(&peers_cache_lock);
}

void scsc_wifi_pause_arp_q_all_vif(struct slsi_dev *sdev)
{
	if (!sdev)
		return;

	SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Pause ARP queues\n");
	atomic_set(&sdev->ctrl_pause_state, 1);
	fcq_stop_arp_q_all_vif(sdev);
}

void scsc_wifi_unpause_arp_q_all_vif(struct slsi_dev *sdev)
{
	if (!sdev)
		return;

	SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Unpause ARP queues\n");
	atomic_set(&sdev->ctrl_pause_state, 0);
	fcq_wake_arp_q_all_vif(sdev);
}
#endif

#ifdef ENABLE_QCOD
/* Detects AC queues that have stopped and redistributes the qmod
 * returns true if the redistribute succeed (qmod increases)
 * returns false if qmod was not redistributed
 */
static bool fcq_redistribute_qmod_before_stopping(struct scsc_wifi_fcq_data_qset *qs)
{
	struct scsc_wifi_fcq_q_header *queue;
	int i, j;
	u32 *qmod_table;
	u32 val;

	/* Only BE, so skip as nothing could be done */
	if (qs->ac_inuse == 1)
		return false;

	/* Go through the list of possible candidates */
	for (i = 1; i < SLSI_NETIF_Q_PER_PEER; i++) {
		queue = &qs->ac_q[i].head;
		if (queue->can_be_distributed &&
		    (ktime_compare(ktime_get(), ktime_add_ms(queue->empty_t, scsc_wifi_fcq_distribution_delay_ms)) > 0)) {
			/* This queue could be redistributed */
			qs->ac_inuse &= ~(1 << i);
			/* To prevent further reallocation */
			queue->can_be_distributed = false;
			qmod_table = &ac_q_layout[qs->ac_inuse >> 1][0];
			for (j = 0; j < SLSI_NETIF_Q_PER_PEER; j++) {
				queue = &qs->ac_q[j].head;
				val = (atomic_read(&qs->smod) * qmod_table[SLSI_NETIF_Q_PER_PEER - 1 - j]) / 1000;
				SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Detected non-active ac queue (%d). Redistribute qmod[%d] %d qcod %d\n", i, j, val, atomic_read(&queue->qcod));
				atomic_set(&queue->qmod, val);
			}
			return true;
		}
	}

	return false;
}

static void fcq_redistribute_qmod(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u8 peer_index, u8 vif)
{
	int i;
	struct scsc_wifi_fcq_q_header *queue_redis;
	u32 *qmod_table;
	u32 val;

	qmod_table = &ac_q_layout[qs->ac_inuse >> 1][0];
	for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++) {
		queue_redis = &qs->ac_q[i].head;
		val = (atomic_read(&qs->smod) * qmod_table[SLSI_NETIF_Q_PER_PEER - 1 - i]) / 1000;
		atomic_set(&queue_redis->qmod, val);
#ifdef CLOSE_IN_OVERSHOOT
		if (val > 0) {
			/* Stop queue that are overshooted because the new smod/qmod */
			if (atomic_read(&queue_redis->active) && (atomic_read(&queue_redis->qcod) >= (atomic_read(&queue_redis->qmod) - STOP_GUARD_QMOD))) {
				netif_stop_subqueue(dev, queue_redis->netif_queue_id);
				SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Closing overshoot queue for vif: %d peer: %d ac: %d qcod (%d) qmod (%d)\n", vif, peer_index, i, atomic_read(&queue_redis->qcod), atomic_read(&queue_redis->qmod));
				atomic_set(&queue_redis->active, 0);
			}
		}
#endif
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Redistribute new value %d: qmod[%d] %d qcod %d active %d\n", val, i, atomic_read(&queue_redis->qmod), atomic_read(&queue_redis->qcod), atomic_read(&queue_redis->active));
	}
}
#endif

static void fcq_redistribute_smod(struct net_device *dev, struct slsi_dev *sdev, int total_to_distribute)
{
#ifdef CLOSE_IN_OVERSHOOT
	int i;
#endif
	u32 new_smod;
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
	int get_total = 0;
#endif
	struct peers_cache *pc_node, *next;
	struct scsc_wifi_fcq_data_qset *qs_redis;

	/* Redistribute smod - qmod */
	/* Go through the list of nodes and redistribute smod and qmod accordingly */
	/* First, get the nominal smod and divide it by total of peers */
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
	get_total = total_to_distribute - total_in_sleep;
	if (get_total > 0)
		new_smod = scsc_wifi_fcq_smod / get_total;
	/* Use the nominal total in case of failure */
	else
		new_smod = scsc_wifi_fcq_smod / total;
#else
	new_smod = scsc_wifi_fcq_smod / total_to_distribute;
#endif
	/* Saturate if number is lower than certian low level */
	if (new_smod < scsc_wifi_fcq_minimum_smod)
		new_smod = scsc_wifi_fcq_minimum_smod;
	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		if (pc_node->is_unicast) {
			qs_redis = pc_node->qs;
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
			if (qs_redis->in_sleep)
				atomic_set(&qs_redis->smod, 0);
			else
				atomic_set(&qs_redis->smod, new_smod);
#else
			atomic_set(&qs_redis->smod, new_smod);
#endif
#ifdef CLOSE_IN_OVERSHOOT
			/* Stop queues to avoid overshooting if scod > smod */
			if (is_smod_active(qs_redis) && atomic_read(&qs_redis->scod) >= (atomic_read(&qs_redis->smod) - STOP_GUARD_SMOD)) {
				/* Disable the qs that is in overshoot */
				for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++)
					netif_stop_subqueue(dev, qs_redis->ac_q[i].head.netif_queue_id);
				SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Closing overshoot qs for vif %d peer_index %d\n", pc_node->vif, pc_node->peer_index);
				atomic_set(&qs_redis->active, 0);
			}
#endif
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
			SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Redistributed smod = %d for vif %d peer_index %d total %d total_in_sleep %d\n", new_smod, pc_node->vif, pc_node->peer_index, total_to_distribute, total_in_sleep);
#else
			SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Redistributed smod = %d for vif %d peer_index %d total %d\n", new_smod, pc_node->vif, pc_node->peer_index, total_to_distribute);
#endif
			/* Redistribute the qmod */
			fcq_redistribute_qmod(pc_node->dev, pc_node->qs, sdev, pc_node->peer_index, pc_node->vif);
		}
	}
	spin_unlock_bh(&peers_cache_lock);
}

#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
static int fcq_redistribute_smod_before_stopping(struct net_device *dev, struct slsi_dev *sdev, int total_to_distribute)
{
	struct peers_cache *pc_node, *next;

	/* only one peer, skip */
	if (total_to_distribute == 1)
		return false;

	/* Search for nodes that were empty and are candidates to be redistributed */
	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		if (pc_node->is_unicast) {
			if (pc_node->qs->can_be_distributed &&
			    (ktime_compare(ktime_get(), ktime_add_ms(pc_node->qs->empty_t, 5000)) > 0)) {
				pc_node->qs->in_sleep = true;
				total_in_sleep += 1;
				SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Smod qs empty. Can be redistributed for vif %d peer_index %d qs->can_be_distributed %d\n", pc_node->vif, pc_node->peer_index, pc_node->qs->can_be_distributed);
				pc_node->qs->can_be_distributed = false;
				spin_unlock_bh(&peers_cache_lock);
				fcq_redistribute_smod(dev, sdev, total_to_distribute);
				return true;
			}
		}
	}
	spin_unlock_bh(&peers_cache_lock);
	return false;
}
#endif

static int fcq_transmit_gmod_domain(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	int gcod;
	int gmod;

	spin_lock(&sdev->hip4_inst.hip_priv->gbot_lock);

	/* Check first the global domain */
	if (sdev->hip4_inst.hip_priv->saturated) {
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx Global domain. No space. active: %d vif: %d peer: %d ac: %d gcod (%d) gmod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
				atomic_read(&sdev->hip4_inst.hip_priv->gactive), vif, peer_index, priority, atomic_read(&sdev->hip4_inst.hip_priv->gcod), atomic_read(&sdev->hip4_inst.hip_priv->gmod),
				td[DIREC_TX][DOMAIN_G][0], td[DIREC_RX][DOMAIN_G][0], td[DIREC_TX][DOMAIN_G][2], td[DIREC_RX][DOMAIN_G][2], td[DIREC_TX][DOMAIN_G][3], td[DIREC_RX][DOMAIN_G][3]);
		fcq_stop_all_queues(sdev);
#endif
		spin_unlock(&sdev->hip4_inst.hip_priv->gbot_lock);
		return -ENOSPC;
	}

	if (!atomic_read(&sdev->hip4_inst.hip_priv->gactive) && sdev->hip4_inst.hip_priv->guard-- == 0) {
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx Global domain. Saturating Gmod. active: %d vif: %d peer: %d ac: %d gcod (%d) gmod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
				atomic_read(&sdev->hip4_inst.hip_priv->gactive), vif, peer_index, priority, atomic_read(&sdev->hip4_inst.hip_priv->gcod), atomic_read(&sdev->hip4_inst.hip_priv->gmod),
				td[DIREC_TX][DOMAIN_G][0], td[DIREC_RX][DOMAIN_G][0], td[DIREC_TX][DOMAIN_G][2], td[DIREC_RX][DOMAIN_G][2], td[DIREC_TX][DOMAIN_G][3], td[DIREC_RX][DOMAIN_G][3]);
#endif
		sdev->hip4_inst.hip_priv->saturated = true;
	}

	gmod = atomic_read(&sdev->hip4_inst.hip_priv->gmod);
	gcod = atomic_inc_return(&sdev->hip4_inst.hip_priv->gcod);
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
	fcq_update_counters(DIREC_TX, DOMAIN_G, priority);
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "tx: active: %d vif: %d peer: %d ac: %d gcod (%d) gmod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
			atomic_read(&sdev->hip4_inst.hip_priv->gactive), vif, peer_index, priority, gcod, gmod,
			td[DIREC_TX][DOMAIN_G][0], td[DIREC_RX][DOMAIN_G][0], td[DIREC_TX][DOMAIN_G][2], td[DIREC_RX][DOMAIN_G][2], td[DIREC_TX][DOMAIN_G][3], td[DIREC_RX][DOMAIN_G][3]);
#endif
	if (gcod >= (atomic_read(&sdev->hip4_inst.hip_priv->gmod) - STOP_GUARD_GMOD)) {
		fcq_stop_all_queues(sdev);
		if (atomic_read(&sdev->hip4_inst.hip_priv->gactive)) {
			sdev->hip4_inst.hip_priv->guard = STOP_GUARD_GMOD;
			/* if GUARD is zero, saturate inmmediatelly */
			if (sdev->hip4_inst.hip_priv->guard == 0)
				sdev->hip4_inst.hip_priv->saturated = true;
		}
		atomic_set(&sdev->hip4_inst.hip_priv->gactive, 0);
		SCSC_HIP4_SAMPLER_BOT_STOP_Q(sdev->minor_prof, vif, peer_index);
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Global Queues Stopped. gcod (%d) >= gmod (%d) gactive(%d)\n", gcod, gmod, atomic_read(&sdev->hip4_inst.hip_priv->gactive));
	}
	spin_unlock(&sdev->hip4_inst.hip_priv->gbot_lock);

	return 0;
}

/* This function should be called in spinlock(qs) */
static int fcq_transmit_smod_domain(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	int scod;

#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
	if (qs->in_sleep) {
		/* Queue was put in sleep and now has become active, need to redistribute smod */
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Detected activity in sleep Qs active %d vif: %d peer: %d ac: %d\n",
				atomic_read(&qs->active), vif, peer_index, priority);
		/* Before redistributing need to update the redistribution parameters */
		qs->in_sleep = false;
		if (total_in_sleep)
			total_in_sleep -= 1;
		fcq_redistribute_smod(dev, sdev, total);
	}
	/* If we transmit we consider the queue -not- empty */
	qs->can_be_distributed = false;
#endif
	/* Check smod domain */
	if (qs->saturated) {
		int i;

#if IS_ENABLED(CONFIG_SCSC_DEBUG)
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx Smod domain. No space. active %d vif: %d peer: %d ac: %d scod (%d) smod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
				atomic_read(&qs->active), vif, peer_index, priority, atomic_read(&qs->scod), atomic_read(&qs->smod),
				td[DIREC_TX][DOMAIN_S][0], td[DIREC_RX][DOMAIN_S][0], td[DIREC_TX][DOMAIN_S][2], td[DIREC_RX][DOMAIN_S][2], td[DIREC_TX][DOMAIN_S][3], td[DIREC_RX][DOMAIN_S][3]);
#endif

		/* Close subqueues again */
		for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++)
			netif_stop_subqueue(dev, qs->ac_q[i].head.netif_queue_id);

		return -ENOSPC;
	}
	/* Pass the frame until reaching the actual saturation */
	if (!atomic_read(&qs->active) && (qs->guard-- == 0)) {
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx Smod domain. Going into Saturation. active %d vif: %d peer: %d ac: %d scod (%d) smod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
				atomic_read(&qs->active), vif, peer_index, priority, atomic_read(&qs->scod), atomic_read(&qs->smod),
				td[DIREC_TX][DOMAIN_S][0], td[DIREC_RX][DOMAIN_S][0], td[DIREC_TX][DOMAIN_S][2], td[DIREC_RX][DOMAIN_S][2], td[DIREC_TX][DOMAIN_S][3], td[DIREC_RX][DOMAIN_S][3]);
#endif
		qs->saturated = true;
	}
	scod = atomic_inc_return(&qs->scod);
	SCSC_HIP4_SAMPLER_BOT_TX(sdev->minor_prof, vif, peer_index, priority, (atomic_read(&qs->smod) << 16) | (scod & 0xFFFF));
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
	fcq_update_counters(DIREC_TX, DOMAIN_S, priority);
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "tx: active: %d vif: %d peer: %d ac: %d scod (%d) smod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
			atomic_read(&qs->active), vif, peer_index, priority, atomic_read(&qs->scod), atomic_read(&qs->smod),
			td[DIREC_TX][DOMAIN_S][0], td[DIREC_RX][DOMAIN_S][0], td[DIREC_TX][DOMAIN_S][2], td[DIREC_RX][DOMAIN_S][2], td[DIREC_TX][DOMAIN_S][3], td[DIREC_RX][DOMAIN_S][3]);
#endif
	if (scod >= (atomic_read(&qs->smod) - STOP_GUARD_SMOD)) {
		int i;
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
		/* Before closing check whether we could get slots from non used queues */
		if (fcq_redistribute_smod_before_stopping(dev, sdev, total)) {
			SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Skipped Stop vif: %d peer: %d. scod (%d) >= smod (%d)\n", vif, peer_index, atomic_read(&qs->scod), atomic_read(&qs->smod));
			return 0;
		}
#endif
		for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++)
			netif_stop_subqueue(dev, qs->ac_q[i].head.netif_queue_id);

		if (atomic_read(&qs->active)) {
			qs->guard = STOP_GUARD_SMOD;
			/* if GUARD is zero, saturate inmmediatelly */
			if (qs->guard == 0)
				qs->saturated = true;
		}
		atomic_set(&qs->active, 0);
		qs->stats.netq_stops++;
		SCSC_HIP4_SAMPLER_BOT_STOP_Q(sdev->minor_prof, vif, peer_index);
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Smod Queues Stopped vif: %d peer: %d. scod (%d) >= smod (%d)\n", vif, peer_index, atomic_read(&qs->scod), atomic_read(&qs->smod));
	}
	return 0;
}

#if defined(ENABLE_CTRL_FCQ) || defined(ENABLE_QCOD)
/* This function should be called in spinlock(qs) */
static int fcq_transmit_qmod_domain(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u16 priority, u8 peer_index, u8 vif)
{
	struct scsc_wifi_fcq_q_header *queue;
	int qcod;

	queue = &qs->ac_q[priority].head;

	if (!(qs->ac_inuse & (1 << priority))) {
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "New AC detected: %d\n", priority);
		qs->ac_inuse |= (1 << priority);
		fcq_redistribute_qmod(dev, qs, sdev, peer_index, vif);
		queue->can_be_distributed = false;
	}

	if (queue->saturated) {
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx No space in ac: %d\n", priority);
		/* Stop subqueue */
		netif_stop_subqueue(dev, queue->netif_queue_id);
		return -ENOSPC;
	}

	/* Pass the frame until reaching the actual saturation */
	if (!atomic_read(&queue->active) && (queue->guard-- == 0)) {
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx No space in ac: %d Saturated\n", priority);
		queue->saturated = true;
	}

	qcod = atomic_inc_return(&queue->qcod);
	SCSC_HIP4_SAMPLER_BOT_QMOD_TX(sdev->minor_prof, vif, peer_index, priority, (atomic_read(&queue->qmod) << 16) | (qcod & 0xFFFF));
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
	fcq_update_counters(DIREC_TX, DOMAIN_Q, priority);
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "tx: active: %d vif: %d peer: %d ac: %d qcod (%d) qmod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
			atomic_read(&queue->active), vif, peer_index, priority, atomic_read(&queue->qcod), atomic_read(&queue->qmod),
			td[DIREC_TX][DOMAIN_Q][0], td[DIREC_RX][DOMAIN_Q][0], td[DIREC_TX][DOMAIN_Q][2], td[DIREC_RX][DOMAIN_Q][2], td[DIREC_TX][DOMAIN_Q][3], td[DIREC_RX][DOMAIN_Q][3]);
#endif
	if (atomic_read(&queue->active) && qcod >= (atomic_read(&queue->qmod) - STOP_GUARD_QMOD)) {
		/* Before closing check whether we could get slots from non used queues */
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
		if (fcq_redistribute_smod_before_stopping(dev, sdev, total)) {
			SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Skipped Stop vif: %d peer: %d. scod (%d) >= smod (%d)\n", vif, peer_index, atomic_read(&qs->scod), atomic_read(&qs->smod));
			goto skip_stop;
		}
#endif
		if (fcq_redistribute_qmod_before_stopping(qs))
			goto skip_stop;
		SCSC_HIP4_SAMPLER_BOT_QMOD_STOP(sdev->minor_prof, vif, peer_index, priority);
		SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Stop subqueue vif: %d peer: %d ac: %d qcod (%d) qmod (%d)\n", vif, peer_index, priority, atomic_read(&queue->qcod), atomic_read(&queue->qmod));
		if (atomic_read(&queue->active)) {
			queue->guard = STOP_GUARD_QMOD;
			/* if GUARD is zero, saturate inmmediatelly */
			if (queue->guard == 0)
				queue->saturated = true;
		}
		atomic_set(&queue->active, 0);
		netif_stop_subqueue(dev, queue->netif_queue_id);
		queue->stats.netq_stops++;
		fcq_netq_start_stop_sample(queue, 0);
	}
skip_stop:
	return 0;
}
#endif

int scsc_wifi_fcq_transmit_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue)
{
	int rc = 0;

#ifdef ENABLE_CTRL_FCQ
	if (WARN_ON(!dev))
		return -EINVAL;

	if (WARN_ON(!queue))
		return -EINVAL;

	rc = fcq_transmit_qmod_domain(dev, &queue->head);
#endif
	return rc;
}

int scsc_wifi_fcq_transmit_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	int rc;
	struct peers_cache *pc_node, *next;

	if (WARN_ON(!dev))
		return -EINVAL;

	if (WARN_ON(!qs))
		return -EINVAL;

	if (WARN_ON(priority >= ARRAY_SIZE(qs->ac_q)))
		return -EINVAL;

	spin_lock_bh(&qs->cp_lock);
	/* Check caller matches an existing peer record */
	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		if (pc_node->qs == qs && pc_node->peer_index == peer_index &&
		    pc_node->vif == vif && pc_node->dev == dev) {
			spin_unlock_bh(&peers_cache_lock);
			goto found;
		}
	}
	spin_unlock_bh(&peers_cache_lock);
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Packet dropped. Detected incorrect peer record\n");
	spin_unlock_bh(&qs->cp_lock);
	return -EINVAL;
found:
	/* Controlled port is not yet open; so can't send data frame */
	if (qs->controlled_port_state == SCSC_WIFI_FCQ_8021x_STATE_BLOCKED) {
		SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "8021x_STATE_BLOCKED\n");
		spin_unlock_bh(&qs->cp_lock);
		return -EPERM;
	}
	rc = fcq_transmit_gmod_domain(dev, qs, priority, sdev, vif, peer_index);
	if (rc) {
		spin_unlock_bh(&qs->cp_lock);
		return rc;
	}

	rc = fcq_transmit_smod_domain(dev, qs, priority, sdev, vif, peer_index);
	if (rc) {
		/* Queue is full and was not active, so decrement gcod since
		 * this packet won't be transmitted, but the overall gcod
		 * resource is still available. This situation should never
		 * happen if flow control works as expected.
		 */
		atomic_dec(&sdev->hip4_inst.hip_priv->gcod);
		spin_unlock_bh(&qs->cp_lock);
		return rc;
	}

#ifdef ENABLE_QCOD
	rc = fcq_transmit_qmod_domain(dev, qs, sdev, priority, peer_index, vif);
	if (rc) {
		/* Queue is full and was not active, so decrement scod since
		 * this packet won't be transmitted, but the overall scod
		 * resource is still available. This situation should never
		 * happen if flow control works as expected.
		 */
		atomic_dec(&qs->scod);
		atomic_dec(&sdev->hip4_inst.hip_priv->gcod);
		spin_unlock_bh(&qs->cp_lock);
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx scsc_wifi_fcq_transmit_data: Flow control not respected. Packet will be dropped.\n");
		return rc;
	}
#endif

	spin_unlock_bh(&qs->cp_lock);
	return 0;
}

static int fcq_receive_gmod_domain(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u16 priority, u8 peer_index, u8 vif)
{
	int gcod;
	int gmod;
	int gactive;

	spin_lock(&sdev->hip4_inst.hip_priv->gbot_lock);
	/* Decrease first the global domain */
	gmod = atomic_read(&sdev->hip4_inst.hip_priv->gmod);
	gcod = atomic_dec_return(&sdev->hip4_inst.hip_priv->gcod);
	gactive = atomic_read(&sdev->hip4_inst.hip_priv->gactive);
	if (unlikely(gcod < 0)) {
		atomic_set(&sdev->hip4_inst.hip_priv->gcod, 0);
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx scsc_wifi_fcq_receive: gcod is negative. Has been fixed\n");
	}

#if IS_ENABLED(CONFIG_SCSC_DEBUG)
	fcq_update_counters(DIREC_RX, DOMAIN_G, priority);
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "rx: active: %d vif: %d peer: %d ac: %d gcod (%d) gmod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d %s\n",
			gactive, vif, peer_index, priority, gcod, gmod,
			td[DIREC_TX][DOMAIN_G][0], td[DIREC_RX][DOMAIN_G][0], td[DIREC_TX][DOMAIN_G][2], td[DIREC_RX][DOMAIN_G][2], td[DIREC_TX][DOMAIN_G][3], td[DIREC_RX][DOMAIN_G][3], qs ? "" : "NO PEER");
#endif

	if (!is_gmod_active(sdev) && (gcod + SCSC_WIFI_FCQ_GMOD_RESUME_HYSTERESIS / total < gmod)) {
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Global Queues Started. gcod (%d) < gmod (%d)\n", gcod, gmod);
		sdev->hip4_inst.hip_priv->saturated = false;
		atomic_set(&sdev->hip4_inst.hip_priv->gactive, 1);
		fcq_wake_all_queues(sdev);
	}
	spin_unlock(&sdev->hip4_inst.hip_priv->gbot_lock);

	return 0;
}

static int fcq_receive_smod_domain(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u16 priority, u8 peer_index, u8 vif)
{
	int scod;

	scod = atomic_dec_return(&qs->scod);
	if (unlikely(scod < 0)) {
		atomic_set(&qs->scod, 0);
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx scsc_wifi_fcq_receive: scod is negative. Has been fixed\n");
	}

#if IS_ENABLED(CONFIG_SCSC_DEBUG)
	fcq_update_counters(DIREC_RX, DOMAIN_S, priority);
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "rx: active: %d vif: %d peer: %d ac: %d scod (%d) smod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
			atomic_read(&qs->active), vif, peer_index, priority, atomic_read(&qs->scod), atomic_read(&qs->smod),
			td[DIREC_TX][DOMAIN_S][0], td[DIREC_RX][DOMAIN_S][0], td[DIREC_TX][DOMAIN_S][2], td[DIREC_RX][DOMAIN_S][2], td[DIREC_TX][DOMAIN_S][3], td[DIREC_RX][DOMAIN_S][3]);
#endif
	SCSC_HIP4_SAMPLER_BOT_RX(sdev->minor_prof, vif, peer_index, priority, (atomic_read(&qs->smod) << 16) | (scod & 0xFFFF));
	if (!is_smod_active(qs) && (scod + SCSC_WIFI_FCQ_SMOD_RESUME_HYSTERESIS / total < atomic_read(&qs->smod))) {
		int i;
		/* Resume all queues for this peer that were active . Do not wake queues in pause state or closed in upper domains */
		for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++) {
			if (is_gmod_active(sdev) && is_qmod_active(&qs->ac_q[i]) && !is_in_pause(sdev))
				netif_wake_subqueue(dev, qs->ac_q[i].head.netif_queue_id);
			else
				SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "smod wake vif %d peer_index %d ac %d not woken up!\n", vif, peer_index, i);
		}

		SCSC_HIP4_SAMPLER_BOT_START_Q(sdev->minor_prof, vif, peer_index);
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Smod Queues Started vif: %d peer: %d. scod (%d) >= smod (%d)\n", vif, peer_index, atomic_read(&qs->scod), atomic_read(&qs->smod));
		/* Regardless the queue were not woken up, set the qs as active */
		qs->saturated = false;
		atomic_set(&qs->active, 1);
		qs->stats.netq_resumes++;
	}

#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
	if (scod == 0) {
		/* Get the empty time */
		qs->empty_t = ktime_get();
		qs->can_be_distributed = true;
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Qs empty vif: %d peer: %d\n", vif, peer_index);
	} else {
		qs->can_be_distributed = false;
	}
#endif
	return 0;
}

#ifdef ENABLE_QCOD
static int fcq_receive_qmod_domain(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u16 priority, u8 peer_index, u8 vif)
{
	struct scsc_wifi_fcq_q_header *queue;
	int qcod;

	queue = &qs->ac_q[priority].head;

	qcod = atomic_dec_return(&queue->qcod);
	if (unlikely(qcod < 0)) {
		atomic_set(&queue->qcod, 0);
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "xxxxxxxxxxxxxxxxxxxxxxx fcq_receive_qmod_domain: qcod is negative. Has been fixed\n");
	}

#if IS_ENABLED(CONFIG_SCSC_DEBUG)
	fcq_update_counters(DIREC_RX, DOMAIN_Q, priority);
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "rx: active: %d vif: %d peer: %d ac: %d qcod (%d) qmod (%d) betx:%d berx:%d vitx:%d virx:%d votx:%d vorx:%d\n",
			atomic_read(&queue->active), vif, peer_index, priority, atomic_read(&queue->qcod), atomic_read(&queue->qmod),
			td[DIREC_TX][DOMAIN_Q][0], td[DIREC_RX][DOMAIN_Q][0], td[DIREC_TX][DOMAIN_Q][2], td[DIREC_RX][DOMAIN_Q][2], td[DIREC_TX][DOMAIN_Q][3], td[DIREC_RX][DOMAIN_Q][3]);
#endif
	SCSC_HIP4_SAMPLER_BOT_QMOD_RX(sdev->minor_prof, vif, peer_index, priority, (atomic_read(&queue->qmod) << 16) | (qcod & 0xFFFF));

	if (!is_qmod_active(&qs->ac_q[priority]) && ((qcod + SCSC_WIFI_FCQ_QMOD_RESUME_HYSTERESIS / total) < atomic_read(&queue->qmod))) {
		/* Do not wake queues in pause state or closed by other domain */
		if (is_gmod_active(sdev) && is_smod_active(qs) && !is_in_pause(sdev))
			netif_wake_subqueue(dev, queue->netif_queue_id);
		/* Only support a maximum of 16 peers!!!!!!*/
		SCSC_HIP4_SAMPLER_BOT_QMOD_START(sdev->minor_prof, vif, peer_index, priority);
		SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Start subqueue vif: %d peer: %d ac: %d qcod (%d) qmod (%d)\n", vif, peer_index, priority, atomic_read(&queue->qcod), atomic_read(&queue->qmod));
		queue->stats.netq_resumes++;
		/* Regardless the queue was not woken up, set the queue as active */
		queue->saturated = false;
		atomic_set(&queue->active, 1);
		fcq_netq_start_stop_sample(queue, 1);
	}

	/* Ignore priority BE as it it always active */
	if (qcod == 0 && priority != SLSI_TRAFFIC_Q_BE) {
		/* Get the stop time */
		queue->empty_t = ktime_get();
		queue->can_be_distributed = true;
	} else {
		queue->can_be_distributed = false;
	}
	return 0;
}
#endif

int scsc_wifi_fcq_receive_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue)
{
	if (WARN_ON(!dev))
		return -EINVAL;

	if (WARN_ON(!queue))
		return -EINVAL;

#ifdef ENABLE_CTRL_FCQ
	/* return fcq_receive(dev, &queue->head, NULL, 0, 0, 0); */
#endif
	return 0;
}

/* This function is to collect missing returning mbulks from a peer that has dissapeared so the qset is missing */
int scsc_wifi_fcq_receive_data_no_peer(struct net_device *dev, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	fcq_receive_gmod_domain(dev, NULL, sdev, priority, peer_index, vif);
#if IS_ENABLED(CONFIG_SCSC_DEBUG)
	/* Update also S and Q domain */
	fcq_update_counters(DIREC_RX, DOMAIN_S, priority);
	fcq_update_counters(DIREC_RX, DOMAIN_Q, priority);
#endif
	return 0;
}

int scsc_wifi_fcq_receive_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	int rc = 0;

	if (WARN_ON(!dev))
		return -EINVAL;

	if (WARN_ON(!qs))
		return -EINVAL;

	if (WARN_ON(priority >= ARRAY_SIZE(qs->ac_q)))
		return -EINVAL;

	/* The read/modify/write of the scod here needs synchronisation. */
	spin_lock_bh(&qs->cp_lock);

	rc = fcq_receive_gmod_domain(dev, qs, sdev, priority, peer_index, vif);
	if (rc)
		goto end;

	rc = fcq_receive_smod_domain(dev, qs, sdev, priority, peer_index, vif);
	if (rc)
		goto end;

#ifdef ENABLE_QCOD
	rc = fcq_receive_qmod_domain(dev, qs, sdev, priority, peer_index, vif);
#endif
end:
	spin_unlock_bh(&qs->cp_lock);
	return rc;
}

int scsc_wifi_fcq_update_smod(struct scsc_wifi_fcq_data_qset *qs, enum scsc_wifi_fcq_ps_state peer_ps_state,
			      enum scsc_wifi_fcq_queue_set_type type)
{
	if (WARN_ON(!qs))
		return -EINVAL;

	if (peer_ps_state == SCSC_WIFI_FCQ_PS_STATE_POWERSAVE) {
		atomic_set(&qs->smod, type == SCSC_WIFI_FCQ_QUEUE_SET_TYPE_UNICAST ? scsc_wifi_fcq_smod_power : scsc_wifi_fcq_mcast_smod_power);
		qs->peer_ps_state = peer_ps_state;
		qs->peer_ps_state_transitions++;
	} else if (peer_ps_state == SCSC_WIFI_FCQ_PS_STATE_ACTIVE) {
		atomic_set(&qs->smod, type == SCSC_WIFI_FCQ_QUEUE_SET_TYPE_UNICAST ? scsc_wifi_fcq_smod : scsc_wifi_fcq_mcast_smod);
		qs->peer_ps_state = peer_ps_state;
		qs->peer_ps_state_transitions++;
	} else
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Unknown sta_state %d\n",
				peer_ps_state);

	return 0;
}

int scsc_wifi_fcq_8021x_port_state(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, enum scsc_wifi_fcq_8021x_state state)
{
	if (WARN_ON(!dev))
		return -EINTR;

	if (WARN_ON(!qs))
		return -EINVAL;

	spin_lock_bh(&qs->cp_lock);
	qs->controlled_port_state = state;
	spin_unlock_bh(&qs->cp_lock);
	SLSI_NET_DBG1(dev, SLSI_WIFI_FCQ, "802.1x: Queue set 0x%p is %s\n", qs,
		      state == SCSC_WIFI_FCQ_8021x_STATE_OPEN ? "Open" : "Blocked");
	return 0;
}

/**
 * Statistics
 */
int scsc_wifi_fcq_stat_queue(struct scsc_wifi_fcq_q_header *queue,
			     struct scsc_wifi_fcq_q_stat *queue_stat,
			     int *qmod, int *qcod)
{
	if (WARN_ON(!queue) || WARN_ON(!queue_stat) || WARN_ON(!qmod) || WARN_ON(!qmod))
		return -EINTR;

	/* check if the value Net q stop %ge is stale
	 *
	 * As we don't have a timer to monitor the %ge, a stale value can remain
	 * a long time due to lack of transition in Net q start/stop
	 */
	if(ktime_to_ms(ktime_sub(ktime_get(), queue->stats.last_sample_time)) > 2000)
		queue->stats.netq_stop_percent = 0;

	memcpy(queue_stat, &queue->stats, sizeof(struct scsc_wifi_fcq_q_stat));
	*qmod = atomic_read(&queue->qmod);
	*qcod = atomic_read(&queue->qcod);
	return 0;
}

int scsc_wifi_fcq_stat_queueset(struct scsc_wifi_fcq_data_qset *queue_set,
				struct scsc_wifi_fcq_q_stat *queue_stat,
				int *smod, int *scod, enum scsc_wifi_fcq_8021x_state *cp_state,
				u32 *peer_ps_state_transitions)
{
	if (WARN_ON(!queue_set) || WARN_ON(!queue_stat) || WARN_ON(!smod) || WARN_ON(!scod) ||
	    WARN_ON(!cp_state) || WARN_ON(!peer_ps_state_transitions))
		return -EINTR;

	memcpy(queue_stat, &queue_set->stats, sizeof(struct scsc_wifi_fcq_q_stat));
	*peer_ps_state_transitions = queue_set->peer_ps_state_transitions;
	*cp_state = queue_set->controlled_port_state;
	*smod = atomic_read(&queue_set->smod);
	*scod = atomic_read(&queue_set->scod);
	return 0;
}

/**
 * Queue and Queue Set init/deinit
 */
int scsc_wifi_fcq_ctrl_q_init(struct scsc_wifi_fcq_ctrl_q *queue)
{
	if (WARN_ON(!queue))
		return -EINVAL;

	/* Ensure that default qmod doesn't exceed 24 bit */
	if (WARN_ON(scsc_wifi_fcq_qmod >= 0x1000000))
		return -EINVAL;

	atomic_set(&queue->head.qmod, scsc_wifi_fcq_qmod);
	atomic_set(&queue->head.qcod, 0);
	queue->head.netif_queue_id = 0;
	queue->head.stats.netq_stops = 0;
	queue->head.stats.netq_resumes = 0;
	atomic_set(&queue->head.active, 1);

	return 0;
}

void scsc_wifi_fcq_ctrl_q_deinit(struct scsc_wifi_fcq_ctrl_q *queue)
{
	int qcod;

	WARN_ON(!queue);

	qcod = atomic_read(&queue->head.qcod);
	if (qcod != 0)
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Ctrl queue (0x%p) deinit: qcod is %d, netif queue %d\n",
				queue, qcod, queue->head.netif_queue_id);
}

static int fcq_data_q_init(struct net_device *dev, struct slsi_dev *sdev, enum scsc_wifi_fcq_queue_set_type type, struct scsc_wifi_fcq_data_q *queue,
			   struct scsc_wifi_fcq_data_qset *qs, u8 qs_num, s16 ac)
{
	if (WARN_ON(!queue))
		return -EINVAL;

	if (WARN_ON(!qs))
		return -EINVAL;

	/* Ensure that default qmods don't exceed 24 bit */
	if (WARN_ON(scsc_wifi_fcq_qmod >= 0x1000000) || WARN_ON(scsc_wifi_fcq_mcast_qmod >= 0x1000000))
		return -EINVAL;

	atomic_set(&queue->head.qmod, type == SCSC_WIFI_FCQ_QUEUE_SET_TYPE_UNICAST ? scsc_wifi_fcq_qmod : scsc_wifi_fcq_mcast_qmod);

	atomic_set(&queue->head.qcod, 0);
	queue->qs = qs;
	queue->head.netif_queue_id = type == SCSC_WIFI_FCQ_QUEUE_SET_TYPE_UNICAST ?
				     slsi_netif_get_peer_queue(qs_num, ac) : slsi_netif_get_multicast_queue(ac);

	queue->head.stats.netq_stops = 0;
	queue->head.stats.netq_resumes = 0;

	/* TODO: This could generate some ENOSPC if queues are full */
	if (!atomic_read(&sdev->in_pause_state))
		netif_wake_subqueue(dev, queue->head.netif_queue_id);

	queue->head.saturated = false;
	atomic_set(&queue->head.active, 1);
	queue->head.stats.netq_state = 1;
	queue->head.stats.netq_stop_time_in_ms = 0;
	queue->head.stats.netq_stop_percent = 0;

	return 0;
}

static void fcq_data_q_deinit(struct scsc_wifi_fcq_data_q *queue)
{
	int qcod;

	WARN_ON(!queue);

	qcod = atomic_read(&queue->head.qcod);
	if (qcod != 0)
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Data queue (0x%p) deinit: qcod is %d, netif queue %d\n",
				queue, qcod, queue->head.netif_queue_id);
}

static void fcq_qset_init(struct net_device *dev, struct slsi_dev *sdev, enum scsc_wifi_fcq_queue_set_type type, struct scsc_wifi_fcq_data_qset *qs, u8 qs_num)
{
	int                         i;
	struct scsc_wifi_fcq_data_q *queue;

	memset(qs, 0, sizeof(struct scsc_wifi_fcq_data_qset));
	spin_lock_init(&qs->cp_lock);
	atomic_set(&qs->smod, type == SCSC_WIFI_FCQ_QUEUE_SET_TYPE_UNICAST ? scsc_wifi_fcq_smod : scsc_wifi_fcq_mcast_smod);
	atomic_set(&qs->scod, 0);

	spin_lock_bh(&qs->cp_lock);
	qs->peer_ps_state = SCSC_WIFI_FCQ_PS_STATE_ACTIVE;
	qs->saturated = false;
	atomic_set(&qs->active, 1);
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
	qs->in_sleep = false;
	qs->can_be_distributed = false;
#endif
	qs->controlled_port_state = SCSC_WIFI_FCQ_8021x_STATE_BLOCKED;

	/* Queues init */
	for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++) {
		/* Clear all the bits */
		qs->ac_inuse &= ~(1 << i);
		queue = &qs->ac_q[i];
		fcq_data_q_init(dev, sdev, type, queue, qs, qs_num, i);
	}
	/* Give all qmod to BE */
	qs->ac_inuse = 1;
	spin_unlock_bh(&qs->cp_lock);
}

int scsc_wifi_fcq_unicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u8 qs_num, struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer)
{
	struct peers_cache *pc_new_node;

	if (WARN_ON(!qs))
		return -EINVAL;

	/* Ensure that default smod doesn't exceed 24 bit */
	if (WARN_ON(scsc_wifi_fcq_smod >= 0x1000000))
		return -EINVAL;

	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Init unicast queue set 0x%p vif %d peer_index %d\n", qs, vif, peer->aid);
	fcq_qset_init(dev, sdev, SCSC_WIFI_FCQ_QUEUE_SET_TYPE_UNICAST, qs, qs_num);
	SCSC_HIP4_SAMPLER_BOT_ADD(	sdev->minor_prof,
								vif,
								peer->aid,
								(peer->address[3] << 24) | (peer->address[2] << 16) | (peer->address[1] << 8) | (peer->address[0]));
	SCSC_HIP4_SAMPLER_BOT_TX(sdev->minor_prof, vif, peer->aid, 0, (atomic_read(&qs->smod) << 16) | (atomic_read(&qs->scod) & 0xFFFF));
	/* Cache the added peer to optimize the Global start/stop process */
	pc_new_node = kzalloc(sizeof(*pc_new_node), GFP_ATOMIC);
	if (!pc_new_node)
		return -ENOMEM;
	pc_new_node->dev = dev;
	pc_new_node->qs = qs;
	pc_new_node->peer_index = peer->aid;
	pc_new_node->vif = vif;
	pc_new_node->is_unicast = true;

	SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Add new peer qs %p vif %d peer->aid %d\n", qs, vif, peer->aid);
	spin_lock_bh(&peers_cache_lock);
	list_add_tail(&pc_new_node->list, &peers_cache_list);
	spin_unlock_bh(&peers_cache_lock);

	if (total == 0) {
		/* No peers. Reset gcod. */
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
		total_in_sleep = 0;
#endif
		SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "First peer. Reset gcod.\n");
		atomic_set(&sdev->hip4_inst.hip_priv->gcod, 0);
		atomic_set(&sdev->hip4_inst.hip_priv->gactive, 1);
		sdev->hip4_inst.hip_priv->saturated = false;
	}

	total++;
	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Add New peer. Total %d\n", total);
	fcq_redistribute_smod(dev, sdev, total);

	return 0;
}

int scsc_wifi_fcq_multicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u8 vif)
{
	struct peers_cache *pc_node;

	if (WARN_ON(!qs))
		return -EINVAL;

	/* Ensure that default smod doesn't exceed 24 bit */
	if (WARN_ON(scsc_wifi_fcq_mcast_smod >= 0x1000000))
		return -EINVAL;

	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Init multicast queue set 0x%p\n", qs);
	fcq_qset_init(dev, sdev, SCSC_WIFI_FCQ_QUEUE_SET_TYPE_MULTICAST, qs, 0);
	SCSC_HIP4_SAMPLER_BOT_ADD(sdev->minor_prof, vif, 0, 0);
	SCSC_HIP4_SAMPLER_BOT_TX(sdev->minor_prof, vif, 0, 0, (atomic_read(&qs->smod) << 16) | (atomic_read(&qs->scod) & 0xFFFF));

	/* Cache the added peer to optimize the Global start/stop process */
	pc_node = kzalloc(sizeof(*pc_node), GFP_ATOMIC);
	if (!pc_node)
		return -ENOMEM;
	pc_node->dev = dev;
	pc_node->qs = qs;
	pc_node->peer_index = 0;
	pc_node->vif = vif;
	pc_node->is_unicast = false;

	SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Add Multicast Qset %p vif %d peer->aid 0\n", qs, vif);
	spin_lock_bh(&peers_cache_lock);
	list_add_tail(&pc_node->list, &peers_cache_list);
	spin_unlock_bh(&peers_cache_lock);

	fcq_redistribute_qmod(pc_node->dev, pc_node->qs, sdev, pc_node->peer_index, pc_node->vif);

	return 0;
}

void scsc_wifi_fcq_qset_deinit(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer)
{
	struct peers_cache *pc_node, *next;
	int i, scod;
#ifdef CONFIG_SCSC_WLAN_HIP4_PROFILING
	int aid = 0;

	if (peer)
		aid = peer->aid;
#endif

	WARN_ON(!qs);

	if (!qs)
		return;

	scod = atomic_read(&qs->scod);
	if (scod != 0)
		SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Data set (0x%p) deinit: scod is %d\n", qs, scod);

	for (i = 0; i < SLSI_NETIF_Q_PER_PEER; i++)
		fcq_data_q_deinit(&qs->ac_q[i]);

	qs->ac_inuse = 0;
#ifdef EXPERIMENTAL_DYNAMIC_SMOD_ADAPTATION
	/* Remove from total sleep if was in sleep */
	if (qs->in_sleep && total_in_sleep)
		total_in_sleep -= 1;
#endif
	SCSC_HIP4_SAMPLER_BOT_RX(sdev->minor_prof, vif, aid, 0, 0);
	SCSC_HIP4_SAMPLER_BOT_REMOVE(sdev->minor_prof, vif, aid);

	if (peer)
		SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Delete qs %p vif %d peer->aid %d\n", qs, vif, peer->aid);
	else
		SLSI_DBG1_NODEV(SLSI_WIFI_FCQ, "Delete qs %p vif %d Multicast\n", qs, vif);

	spin_lock_bh(&peers_cache_lock);
	list_for_each_entry_safe(pc_node, next, &peers_cache_list, list) {
		if (pc_node->qs == qs) {
			list_del(&pc_node->list);
			kfree(pc_node);
		}
	}
	spin_unlock_bh(&peers_cache_lock);
	/* Only count unicast qs */
	if (total > 0 && peer)
		total--;

	SLSI_DBG4_NODEV(SLSI_WIFI_FCQ, "Del peer. Total %d\n", total);

	if (total == 0)
		return;

	fcq_redistribute_smod(dev, sdev, total);
}
