/****************************************************************************
 *
 * Copyright (c) 2012 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "debug.h"
#include "dev.h"
#include "ba.h"
#include "mgt.h"
#include "sap.h"

/* Timeout (in milliseconds) for frames in MPDU reorder buffer
 *
 * When a frame is out of order, the frame is stored in Reorder buffer.
 * Frames can be released from the buffer, if subsequent frames arrive such that
 * frames can be ordered or when this timeout occurs
 */
static uint ba_mpdu_reorder_age_timeout = 100; /* 100 milliseconds */
module_param(ba_mpdu_reorder_age_timeout, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ba_mpdu_reorder_age_timeout, "Timeout (in ms) before a BA frame in Reorder buffer is passed to upper layers");

static bool ba_out_of_range_delba_enable = 1;
module_param(ba_out_of_range_delba_enable, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ba_out_of_range_delba_enable, "Option to trigger DELBA on out of range frame (1: enable (default), 0: disable)");

#define BA_WINDOW_BOUNDARY 2048

#define SN_TO_INDEX(__ba_session_rx, __sn) (((__sn - __ba_session_rx->start_sn) & 0xFFF) % __ba_session_rx->buffer_size)

#define ADVANCE_EXPECTED_SN(__ba_session_rx) \
	{ \
		__ba_session_rx->expected_sn++; \
		__ba_session_rx->expected_sn &= 0xFFF; \
	}

#define FREE_BUFFER_SLOT(__ba_session_rx, __index) \
	{ \
		__ba_session_rx->occupied_slots--; \
		__ba_session_rx->buffer[__index].active = false; \
	}

#define IS_SN_LESS(sn1, sn2) ((((sn1) - (sn2)) & 0xFFF) > BA_WINDOW_BOUNDARY)

void slsi_rx_ba_init(struct slsi_dev *sdev)
{
	slsi_spinlock_create(&sdev->rx_ba_buffer_pool_lock);
}

static struct slsi_ba_session_rx *slsi_rx_ba_alloc_buffer(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_ba_session_rx *buffer = NULL;
	int i;

	SLSI_NET_DBG3(dev, SLSI_RX_BA, "RX BA buffer pool status: %d,%d,%d,%d,%d,%d,%d,%d\n",
		      sdev->rx_ba_buffer_pool[0].used, sdev->rx_ba_buffer_pool[1].used, sdev->rx_ba_buffer_pool[2].used,
		      sdev->rx_ba_buffer_pool[3].used, sdev->rx_ba_buffer_pool[4].used, sdev->rx_ba_buffer_pool[5].used,
		      sdev->rx_ba_buffer_pool[6].used, sdev->rx_ba_buffer_pool[7].used);

	slsi_spinlock_lock(&sdev->rx_ba_buffer_pool_lock);
	for (i = 0; i < SLSI_MAX_RX_BA_SESSIONS; i++) {
		if (!sdev->rx_ba_buffer_pool[i].used) {
			sdev->rx_ba_buffer_pool[i].used = true;
			buffer = &sdev->rx_ba_buffer_pool[i];
			break;
		}
	}
	slsi_spinlock_unlock(&sdev->rx_ba_buffer_pool_lock);

	if (!buffer)
		SLSI_NET_ERR(dev, "No free RX BA buffer\n");

	return buffer;
}

static void slsi_rx_ba_free_buffer(struct net_device *dev, struct slsi_peer *peer, int tid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	slsi_spinlock_lock(&sdev->rx_ba_buffer_pool_lock);
	if (peer && peer->ba_session_rx[tid]) {
		peer->ba_session_rx[tid]->used = false;
		peer->ba_session_rx[tid] = NULL;
	}
	slsi_spinlock_unlock(&sdev->rx_ba_buffer_pool_lock);

	SLSI_NET_DBG3(dev, SLSI_RX_BA, "RX BA buffer pool status: %d,%d,%d,%d,%d,%d,%d,%d\n",
		      sdev->rx_ba_buffer_pool[0].used, sdev->rx_ba_buffer_pool[1].used, sdev->rx_ba_buffer_pool[2].used,
		      sdev->rx_ba_buffer_pool[3].used, sdev->rx_ba_buffer_pool[4].used, sdev->rx_ba_buffer_pool[5].used,
		      sdev->rx_ba_buffer_pool[6].used, sdev->rx_ba_buffer_pool[7].used);
}

/* This code - slsi_ba_process_complete()
 * is called in the data workqueue context with the
 * netdev_vif mutex held.
 */
void slsi_ba_process_complete(struct net_device *dev, bool ctx_napi)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *skb;

	while ((skb = skb_dequeue(&ndev_vif->ba_complete)) != NULL)
		slsi_rx_data_deliver_skb(ndev_vif->sdev, dev, skb, ctx_napi);
}

static void slsi_ba_signal_process_complete(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	atomic_set(&ndev_vif->ba_flush, 1);
#ifndef CONFIG_SCSC_WLAN_RX_NAPI
	slsi_skb_schedule_work(&ndev_vif->rx_data);
#endif
}

static void ba_add_frame_to_ba_complete(struct net_device *dev, struct slsi_ba_frame_desc *frame_desc)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	skb_queue_tail(&ndev_vif->ba_complete, frame_desc->signal);
}

static void ba_update_expected_sn(struct net_device *dev,
				  struct slsi_ba_session_rx *ba_session_rx, u16 sn)
{
	u32 i, j;
	u16 gap;

	gap = (sn - ba_session_rx->expected_sn) & 0xFFF;
	SLSI_NET_DBG3(dev, SLSI_RX_BA, "Proccess the frames up to new expected_sn = %d gap = %d\n", sn, gap);

	for (j = 0; j < gap && j < ba_session_rx->buffer_size; j++) {
		i = SN_TO_INDEX(ba_session_rx, ba_session_rx->expected_sn);
		SLSI_NET_DBG3(dev, SLSI_RX_BA, "Proccess the slot index = %d\n", i);
		if (ba_session_rx->buffer[i].active) {
			ba_add_frame_to_ba_complete(dev, &ba_session_rx->buffer[i]);
			SLSI_NET_DBG3(dev, SLSI_RX_BA, "Proccess the frame at index = %d expected_sn = %d\n", i, ba_session_rx->expected_sn);
			FREE_BUFFER_SLOT(ba_session_rx, i);
		} else {
			SLSI_NET_DBG3(dev, SLSI_RX_BA, "Empty slot at index = %d\n", i);
		}
		ADVANCE_EXPECTED_SN(ba_session_rx);
	}
	ba_session_rx->expected_sn = sn;
}

static void ba_complete_ready_sequence(struct net_device         *dev,
				       struct slsi_ba_session_rx *ba_session_rx)
{
	int i;

	i = SN_TO_INDEX(ba_session_rx, ba_session_rx->expected_sn);
	while (ba_session_rx->buffer[i].active) {
		ba_add_frame_to_ba_complete(dev, &ba_session_rx->buffer[i]);
		SLSI_NET_DBG4(dev, SLSI_RX_BA, "Completed stored frame (expected_sn=%d) at i = %d\n",
			      ba_session_rx->expected_sn, i);
		FREE_BUFFER_SLOT(ba_session_rx, i);
		ADVANCE_EXPECTED_SN(ba_session_rx);
		i = SN_TO_INDEX(ba_session_rx, ba_session_rx->expected_sn);
	}
}

static void ba_scroll_window(struct net_device *dev,
			     struct slsi_ba_session_rx *ba_session_rx, u16 sn)
{
	if (((sn - ba_session_rx->expected_sn) & 0xFFF) <= BA_WINDOW_BOUNDARY) {
		ba_update_expected_sn(dev, ba_session_rx, sn);
		ba_complete_ready_sequence(dev, ba_session_rx);
	}
}

static void ba_delete_ba_on_old_frame(struct net_device *dev, struct slsi_peer *peer,
			     struct slsi_ba_session_rx *ba_session_rx, u16 sn)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sap_drv_ma_to_mlme_delba_req *delba_req;
	struct sk_buff *skb = NULL;

	SLSI_NET_DBG4(dev, SLSI_RX_BA, "sn=%d\n", sn);
	/* construct a message for MLME */
	skb = alloc_skb(sizeof(struct sap_drv_ma_to_mlme_delba_req), GFP_ATOMIC);

	if (WARN_ON(!skb))
		return;

	ba_session_rx->closing = true;
	slsi_skb_cb_init(skb)->sig_length = sizeof(struct sap_drv_ma_to_mlme_delba_req);
	slsi_skb_cb_get(skb)->data_length = sizeof(struct sap_drv_ma_to_mlme_delba_req);

	delba_req = (struct sap_drv_ma_to_mlme_delba_req *)skb_put(skb, sizeof(struct sap_drv_ma_to_mlme_delba_req));
	delba_req->header.id = cpu_to_le16(SAP_DRV_MA_TO_MLME_DELBA_REQ);
	delba_req->header.receiver_pid = 0;
	delba_req->header.sender_pid = 0;
	delba_req->header.fw_reference = 0;
	delba_req->vif = ndev_vif->ifnum;

	memcpy(delba_req->peer_qsta_address, peer->address, ETH_ALEN);
	delba_req->sequence_number = sn;
	delba_req->user_priority = ba_session_rx->tid;
	delba_req->reason = FAPI_REASONCODE_OUT_OF_RANGE_SEQUENCE_NUMBER;
	delba_req->direction = FAPI_DIRECTION_RECEIVE;

	/* queue the message for MLME SAP */
	slsi_rx_enqueue_netdev_mlme(ndev_vif->sdev, skb, fapi_get_vif(skb));
}

static int ba_consume_frame_or_get_buffer_index(struct net_device *dev, struct slsi_peer *peer,
						struct slsi_ba_session_rx *ba_session_rx, u16 sn, struct slsi_ba_frame_desc *frame_desc, bool *stop_timer)
{
	int i;
	u16 sn_temp;

	*stop_timer = false;

	if (((sn - ba_session_rx->expected_sn) & 0xFFF) <= BA_WINDOW_BOUNDARY) {
		/* Once we are in BA window, set the flag for BA trigger */
		if (!ba_session_rx->trigger_ba_after_ssn)
			ba_session_rx->trigger_ba_after_ssn = true;

		sn_temp = ba_session_rx->expected_sn + ba_session_rx->buffer_size;
		SLSI_NET_DBG4(dev, SLSI_RX_BA, "New frame: sn=%d\n", sn);

		if (!(((sn - sn_temp) & 0xFFF) > BA_WINDOW_BOUNDARY)) {
			u16 new_expected_sn;

			SLSI_NET_DBG2(dev, SLSI_RX_BA, "Frame is out of window\n");
			sn_temp = (sn - ba_session_rx->buffer_size) & 0xFFF;
			if (ba_session_rx->timer_on)
				*stop_timer = true;
			new_expected_sn = (sn_temp + 1) & 0xFFF;
			ba_update_expected_sn(dev, ba_session_rx, new_expected_sn);
		}

		i = -1;
		if (sn == ba_session_rx->expected_sn) {
			SLSI_NET_DBG4(dev, SLSI_RX_BA, "sn = ba_session_rx->expected_sn = %d\n", sn);
			if (ba_session_rx->timer_on)
				*stop_timer = true;
			ADVANCE_EXPECTED_SN(ba_session_rx);
			ba_add_frame_to_ba_complete(dev, frame_desc);
		} else {
			i = SN_TO_INDEX(ba_session_rx, sn);
			SLSI_NET_DBG4(dev, SLSI_RX_BA, "sn (%d) != ba_session_rx->expected_sn(%d), i = %d\n", sn, ba_session_rx->expected_sn, i);
			if (ba_session_rx->buffer[i].active) {
				SLSI_NET_DBG3(dev, SLSI_RX_BA, "free frame at i = %d\n", i);
				i = -1;
#ifdef CONFIG_SCSC_SMAPPER
				hip4_smapper_free_mapped_skb(frame_desc->signal);
#endif
				kfree_skb(frame_desc->signal);
			}
		}
		if (!IS_SN_LESS(sn, ba_session_rx->highest_received_sn))
			ba_session_rx->highest_received_sn = sn;
	} else {
		i = -1;
		if (!ba_session_rx->trigger_ba_after_ssn) {
			SLSI_NET_DBG3(dev, SLSI_RX_BA, "frame before ssn, pass it up: sn=%d\n", sn);
			ba_add_frame_to_ba_complete(dev, frame_desc);
		} else {
			if (slsi_is_tdls_peer(dev, peer)) {
				/* Don't drop old frames in TDLS AMPDU-reordering for interoperability with third party devices.
				 * When the TDLS link is established the peer sends few packets with AP's sequence number.
				 * BA reorder logic updates the expected sequence number. After that peer sends packets with
				 * starting sequence number negotiated in BA (0). But those frames are getting dropped here.
				 * Because of this TCP traffic does not work and TDLS link is getting disconnected.
				 */
				SLSI_NET_DBG1(dev, SLSI_RX_BA, "tdls: forward old frame: sn=%d, expected_sn=%d\n", sn, ba_session_rx->expected_sn);
				ba_add_frame_to_ba_complete(dev, frame_desc);
			} else {
				/* this frame is deemed as old. But it may so happen that the reorder process did not wait long
				 * enough for this frame and moved to new window. So check here that the current frame still lies in
				 * originators transmit window by comparing it with highest sequence number received from originator.
				 *
				 * If it lies in the window pass the frame to next process else pass the frame and initiate DELBA procedure.
				 */
				if (IS_SN_LESS(ba_session_rx->highest_received_sn, ((sn + ba_session_rx->buffer_size) & 0xFFF))) {
					SLSI_NET_DBG4(dev, SLSI_RX_BA, "old frame, but still in window: sn=%d, highest_received_sn=%d\n", sn, ba_session_rx->highest_received_sn);
					ba_add_frame_to_ba_complete(dev, frame_desc);
				} else {
					SLSI_NET_DBG4(dev, SLSI_RX_BA, "old frame, accept but as countermeasure initiate delete BA: sn=%d, expected_sn=%d\n", sn, ba_session_rx->expected_sn);
					ba_add_frame_to_ba_complete(dev, frame_desc);

					if (ba_out_of_range_delba_enable && !ba_session_rx->closing)
						ba_delete_ba_on_old_frame(dev, peer, ba_session_rx, sn);
				}
			}
		}
	}
	return i;
}

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
static void slsi_ba_aging_timeout_handler(struct timer_list *t)
#else
static void slsi_ba_aging_timeout_handler(unsigned long data)
#endif
{
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	struct slsi_ba_session_rx *ba_session_rx = from_timer(ba_session_rx, t, ba_age_timer);
#else
	struct slsi_ba_session_rx *ba_session_rx = (struct slsi_ba_session_rx *)data;
#endif
	u8                        i, j;
	u8                        gap = 1;
	u16                       temp_sn;
	struct net_device         *dev = ba_session_rx->dev;
	struct netdev_vif         *ndev_vif = netdev_priv(dev);

	SLSI_NET_DBG3(dev, SLSI_RX_BA, "\n");

	slsi_spinlock_lock(&ndev_vif->ba_lock);

	ba_session_rx->timer_on = false;

	if (ba_session_rx->active && ba_session_rx->occupied_slots) {
		/* expected sequence has not arrived so start searching from next
		 * sequence number until a frame is available and determine the gap.
		 * Release all the frames upto next hole from the reorder buffer.
		 */
		temp_sn = (ba_session_rx->expected_sn + 1) & 0xFFF;

		for (j = 0; j < ba_session_rx->buffer_size; j++) {
			i = SN_TO_INDEX(ba_session_rx, temp_sn);

			if (ba_session_rx->buffer[i].active) {
				while (gap--)
					ADVANCE_EXPECTED_SN(ba_session_rx);

				SLSI_NET_DBG3(dev, SLSI_RX_BA, "Completed stored frame (expected_sn=%d) at i = %d\n", ba_session_rx->expected_sn, i);
				ba_add_frame_to_ba_complete(dev, &ba_session_rx->buffer[i]);
				FREE_BUFFER_SLOT(ba_session_rx, i);
				ADVANCE_EXPECTED_SN(ba_session_rx);
				ba_complete_ready_sequence(dev, ba_session_rx);
				break;
			}
			/* advance temp sequence number and frame gap */
			temp_sn = (temp_sn + 1) & 0xFFF;
			gap++;
		}

		/* Check for next hole in the buffer, if hole exists create the timer for next missing frame */
		/* do not rearm the timer if BA session is going down */
		if (ba_session_rx->active && ba_session_rx->occupied_slots) {
			SLSI_NET_DBG3(dev, SLSI_RX_BA, "Timer start\n");
			mod_timer(&ba_session_rx->ba_age_timer, jiffies + msecs_to_jiffies(ba_mpdu_reorder_age_timeout));
			ba_session_rx->timer_on = true;
		}
		/* Process the data now marked as completed */
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
		slsi_ba_process_complete(dev, false);
#else
		slsi_ba_signal_process_complete(dev);
#endif
	}
	slsi_spinlock_unlock(&ndev_vif->ba_lock);
}

int slsi_ba_process_frame(struct net_device *dev, struct slsi_peer *peer,
			  struct sk_buff *skb, u16 sequence_number, u16 tid)
{
	struct netdev_vif         *ndev_vif = netdev_priv(dev);
	int                       i;
	struct slsi_ba_session_rx *ba_session_rx = peer->ba_session_rx[tid];
	struct slsi_ba_frame_desc frame_desc;
	bool                      stop_timer = false;

	SLSI_NET_DBG4(dev, SLSI_RX_BA, "Got frame(sn=%d)\n", sequence_number);

	if (WARN_ON(tid > FAPI_PRIORITY_QOS_UP7)) {
		SLSI_NET_ERR(dev, "tid=%d\n", tid);
		return -EINVAL;
	}

	if (!ba_session_rx)
		return -EINVAL;

	slsi_spinlock_lock(&ndev_vif->ba_lock);

	if (!ba_session_rx->active) {
		SLSI_NET_ERR(dev, "No BA session exists\n");
		slsi_spinlock_unlock(&ndev_vif->ba_lock);
		return -EINVAL;
	}

	frame_desc.signal = skb;
	frame_desc.sn = sequence_number;
	frame_desc.active = true;

	i = ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx, sequence_number, &frame_desc, &stop_timer);
	if (i >= 0) {
		SLSI_NET_DBG4(dev, SLSI_RX_BA, "Store frame(sn=%d) at i = %d\n", sequence_number, i);
		ba_session_rx->buffer[i] = frame_desc;
		ba_session_rx->occupied_slots++;
	} else {
		SLSI_NET_DBG4(dev, SLSI_RX_BA, "Frame consumed - sn = %d\n", sequence_number);
	}

	ba_complete_ready_sequence(dev, ba_session_rx);

	/* Timer decision:
	 *
	 * If the timer is not running (timer_on=false)
	 *    Start the timer if there are holes (occupied_slots!=0)
	 *
	 * If the timer is running (timer_on=true)
	 *    Stop the timer if
	 *       There are no holes (occupied_slots=0)
	 *    Restart the timer if
	 *       stop_timer=true and there are holes (occupied_slots!=0)
	 *    Leave the timer running (do nothing) if
	 *       stop_timer=false and there are holes (occupied_slots!=0)
	 */

	if (!ba_session_rx->timer_on) {
		if (ba_session_rx->occupied_slots) {
			stop_timer = false;
			SLSI_NET_DBG4(dev, SLSI_RX_BA, "Timer start\n");
			mod_timer(&ba_session_rx->ba_age_timer, jiffies + msecs_to_jiffies(ba_mpdu_reorder_age_timeout));
			ba_session_rx->timer_on = true;
		}
	} else if (!ba_session_rx->occupied_slots) {
		stop_timer = true;
	} else if (stop_timer) {
		stop_timer = false;
		SLSI_NET_DBG4(dev, SLSI_RX_BA, "Timer restart\n");
		mod_timer(&ba_session_rx->ba_age_timer, jiffies + msecs_to_jiffies(ba_mpdu_reorder_age_timeout));
		ba_session_rx->timer_on = true;
	}

	if (stop_timer) {
		ba_session_rx->timer_on = false;
		slsi_spinlock_unlock(&ndev_vif->ba_lock);
		SLSI_NET_DBG4(dev, SLSI_RX_BA, "Timer stop\n");
		del_timer_sync(&ba_session_rx->ba_age_timer);
	} else {
		slsi_spinlock_unlock(&ndev_vif->ba_lock);
	}
	slsi_ba_signal_process_complete(dev);
	return 0;
}

bool slsi_ba_check(struct slsi_peer *peer, u16 tid)
{
	if (tid > FAPI_PRIORITY_QOS_UP7)
		return false;
	if (!peer->ba_session_rx[tid])
		return false;

	return peer->ba_session_rx[tid]->active;
}

static void __slsi_rx_ba_stop(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx)
{
	u8 i, j;

	SLSI_NET_DBG1(dev, SLSI_RX_BA, "Stopping BA session: tid = %d\n", ba_session_rx->tid);

	if (WARN_ON(!ba_session_rx->active)) {
		SLSI_NET_ERR(dev, "No BA session exists\n");
		return;
	}

	for (i = SN_TO_INDEX(ba_session_rx, ba_session_rx->expected_sn), j = 0;
	     j < ba_session_rx->buffer_size; i++, j++) {
		i %= ba_session_rx->buffer_size;
		if (ba_session_rx->buffer[i].active) {
			ba_add_frame_to_ba_complete(dev, &ba_session_rx->buffer[i]);
			SLSI_NET_DBG3(dev, SLSI_RX_BA, "Completed stored frame at i = %d\n", i);
			FREE_BUFFER_SLOT(ba_session_rx, i);
		}
	}

#ifdef CONFIG_SCSC_WLAN_RX_NAPI
	slsi_ba_process_complete(dev, false);
#else
	 slsi_ba_signal_process_complete(dev);
#endif

	ba_session_rx->active = false;
}

static void slsi_rx_ba_stop_lock_held(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx)
{
	struct netdev_vif		  *ndev_vif = netdev_priv(dev);

	__slsi_rx_ba_stop(dev, ba_session_rx);
	ba_session_rx->timer_on = false;
	slsi_spinlock_unlock(&ndev_vif->ba_lock);
	del_timer_sync(&ba_session_rx->ba_age_timer);
	slsi_spinlock_lock(&ndev_vif->ba_lock);
}

static void slsi_rx_ba_stop_lock_unheld(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx)
{
	struct netdev_vif         *ndev_vif = netdev_priv(dev);

	slsi_spinlock_lock(&ndev_vif->ba_lock);
	__slsi_rx_ba_stop(dev, ba_session_rx);
	ba_session_rx->timer_on = false;
	slsi_spinlock_unlock(&ndev_vif->ba_lock);
	del_timer_sync(&ba_session_rx->ba_age_timer);
}

void slsi_rx_ba_stop_all(struct net_device *dev, struct slsi_peer *peer)
{
	int i;

	for (i = 0; i < NUM_BA_SESSIONS_PER_PEER; i++)
		if (peer->ba_session_rx[i] && peer->ba_session_rx[i]->active) {
			slsi_rx_ba_stop_lock_unheld(dev, peer->ba_session_rx[i]);
			slsi_rx_ba_free_buffer(dev, peer, i);
		}
}

static int slsi_rx_ba_start(struct net_device *dev,
			    struct slsi_ba_session_rx *ba_session_rx,
			    u16 tid, u16 buffer_size, u16 start_sn)
{
	struct netdev_vif         *ndev_vif = netdev_priv(dev);

	SLSI_NET_DBG1(dev, SLSI_RX_BA, "Request to start a new BA session tid=%d buffer_size=%d start_sn=%d\n",
		      tid, buffer_size, start_sn);

	if (WARN_ON((!buffer_size) || (buffer_size > SLSI_BA_BUFFER_SIZE_MAX))) {
		SLSI_NET_ERR(dev, "Invalid window size: buffer_size=%d\n", buffer_size);
		return -EINVAL;
	}

	slsi_spinlock_lock(&ndev_vif->ba_lock);

	if (ba_session_rx->active) {
		SLSI_NET_DBG1(dev, SLSI_RX_BA, "BA session already exists\n");

		if ((ba_session_rx->buffer_size == buffer_size) &&
		    (ba_session_rx->expected_sn == start_sn)) {
			SLSI_NET_DBG1(dev, SLSI_RX_BA,
				      "BA session tid=%d already exists. The parameters match so keep the existing session\n",
				      tid);
			slsi_spinlock_unlock(&ndev_vif->ba_lock);
			return 0;
		}
		SLSI_NET_DBG1(dev, SLSI_RX_BA, "Parameters don't match so stop the existing BA session: tid=%d\n", tid);
		slsi_rx_ba_stop_lock_held(dev, ba_session_rx);
	}

	ba_session_rx->dev = dev;
	ba_session_rx->buffer_size = buffer_size;
	ba_session_rx->start_sn = start_sn;
	ba_session_rx->expected_sn = start_sn;
	ba_session_rx->highest_received_sn = 0;
	ba_session_rx->closing = false;
	ba_session_rx->trigger_ba_after_ssn = false;
	ba_session_rx->tid = tid;
	ba_session_rx->timer_on = false;
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	timer_setup(&ba_session_rx->ba_age_timer, slsi_ba_aging_timeout_handler, 0);
#else
	ba_session_rx->ba_age_timer.function = slsi_ba_aging_timeout_handler;
	ba_session_rx->ba_age_timer.data = (unsigned long)ba_session_rx;
	init_timer(&ba_session_rx->ba_age_timer);
#endif

	ba_session_rx->active = true;
	SLSI_NET_DBG1(dev, SLSI_RX_BA, "Started a new BA session tid=%d buffer_size=%d start_sn=%d\n",
		      tid, buffer_size, start_sn);

	slsi_spinlock_unlock(&ndev_vif->ba_lock);
	slsi_ba_signal_process_complete(dev);

	return 0;
}

void slsi_ba_update_window(struct net_device *dev,
				  struct slsi_ba_session_rx *ba_session_rx, u16 sequence_number)
{
	struct netdev_vif		  *ndev_vif = netdev_priv(dev);

	slsi_spinlock_lock(&ndev_vif->ba_lock);

	if (WARN_ON(!ba_session_rx->active)) {
		SLSI_NET_ERR(dev, "No BA session exists\n");
		slsi_spinlock_unlock(&ndev_vif->ba_lock);
		return;
	}

	ba_scroll_window(dev, ba_session_rx, sequence_number);
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
	slsi_ba_process_complete(dev, false);
#else
	slsi_ba_signal_process_complete(dev);
#endif
	slsi_spinlock_unlock(&ndev_vif->ba_lock);
}

void slsi_handle_blockack(struct net_device *dev, struct slsi_peer *peer,
			  u16 vif, u8 *peer_qsta_address, u16 parameter_set, u16 sequence_number,
			  u16 reason_code, u16 direction)
{
	struct slsi_ba_session_rx *ba_session_rx;
	u16                       user_priority = (parameter_set >> 2) & 0x000F;
	u16                       buffer_size   = (parameter_set >> 6) & 0x03FF;

	SLSI_UNUSED_PARAMETER(vif);
	SLSI_UNUSED_PARAMETER(peer_qsta_address);

	if (WARN_ON(user_priority > FAPI_PRIORITY_QOS_UP7)) {
		SLSI_NET_ERR(dev, "Invalid user_priority=%d\n", user_priority);
		return;
	}

	switch (direction) {
	case FAPI_DIRECTION_TRANSMIT:
		break;
	case FAPI_DIRECTION_RECEIVE:
		ba_session_rx = peer->ba_session_rx[user_priority];

		switch (reason_code) {
		case FAPI_REASONCODE_START:
			if (!peer->ba_session_rx[user_priority])
				peer->ba_session_rx[user_priority] = slsi_rx_ba_alloc_buffer(dev);

			if (peer->ba_session_rx[user_priority])
				if (slsi_rx_ba_start(dev, peer->ba_session_rx[user_priority], user_priority, buffer_size, sequence_number) != 0)
					slsi_rx_ba_free_buffer(dev, peer, user_priority);
			break;
		case FAPI_REASONCODE_END:
			if (ba_session_rx) {
				slsi_rx_ba_stop_lock_unheld(dev, ba_session_rx);
				slsi_rx_ba_free_buffer(dev, peer, user_priority);
			}
			break;
		case FAPI_REASONCODE_UNSPECIFIED_REASON:
			if (ba_session_rx)
				slsi_ba_update_window(dev, ba_session_rx, sequence_number);
			break;
		default:
			SLSI_NET_ERR(dev, "Invalid value: reason_code=%d\n", reason_code);
			break;
		}
		break;
	default:
		SLSI_NET_ERR(dev, "Invalid value: direction=%d\n", direction);
		break;
	}
}
