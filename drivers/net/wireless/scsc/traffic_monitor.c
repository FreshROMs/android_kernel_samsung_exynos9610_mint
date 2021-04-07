/****************************************************************************
 *
 * Copyright (c) 2012 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/ktime.h>
#include "dev.h"
#include "debug.h"
#include "traffic_monitor.h"

struct slsi_traffic_mon_client_entry {
	struct list_head q;
	void *client_ctx;
	u32 throughput;
	u32 state;
	u32 hysteresis;
	u32 mode;
	u32 mid_tput;
	u32 high_tput;
	void (*traffic_mon_client_cb)(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx);
};

static inline void traffic_mon_invoke_client_callback(struct slsi_dev *sdev, u32 tput_tx, u32 tput_rx)
{
	struct list_head       *pos, *n;
	struct slsi_traffic_mon_client_entry *traffic_client;

	list_for_each_safe(pos, n, &sdev->traffic_mon_clients.client_list) {
		traffic_client = list_entry(pos, struct slsi_traffic_mon_client_entry, q);

		if (traffic_client->mode == TRAFFIC_MON_CLIENT_MODE_PERIODIC) {
			if (traffic_client->traffic_mon_client_cb)
				traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_NONE, tput_tx, tput_rx);
		} else if (traffic_client->mode == TRAFFIC_MON_CLIENT_MODE_EVENTS) {
			if ((traffic_client->high_tput) && ((tput_tx + tput_rx) > traffic_client->high_tput)) {
				if (traffic_client->state != TRAFFIC_MON_CLIENT_STATE_HIGH &&
				   (traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_HIGH)) {
					SLSI_DBG1(sdev, SLSI_HIP, "notify traffic event (tput:%u, state:%u --> HIGH)\n", (tput_tx + tput_rx), traffic_client->state);
					traffic_client->hysteresis = 0;
					traffic_client->state = TRAFFIC_MON_CLIENT_STATE_HIGH;

					if (traffic_client->traffic_mon_client_cb)
						traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_HIGH, tput_tx, tput_rx);
					}
			} else if ((traffic_client->mid_tput) && ((tput_tx + tput_rx) > traffic_client->mid_tput)) {
				if (traffic_client->state != TRAFFIC_MON_CLIENT_STATE_MID) {
					if ((traffic_client->state == TRAFFIC_MON_CLIENT_STATE_LOW && (traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_HIGH)) ||
						(traffic_client->state == TRAFFIC_MON_CLIENT_STATE_HIGH && (traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_LOW))) {
						SLSI_DBG1(sdev, SLSI_HIP, "notify traffic event (tput:%u, state:%u --> MID)\n", (tput_tx + tput_rx), traffic_client->state);
						traffic_client->hysteresis = 0;
						traffic_client->state = TRAFFIC_MON_CLIENT_STATE_MID;
						if (traffic_client->traffic_mon_client_cb)
							traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_MID, tput_tx, tput_rx);
					}
				}
			} else if (traffic_client->state != TRAFFIC_MON_CLIENT_STATE_LOW &&
					(traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_LOW)) {
				SLSI_DBG1(sdev, SLSI_HIP, "notify traffic event (tput:%u, state:%u --> LOW\n", (tput_tx + tput_rx), traffic_client->state);
				traffic_client->hysteresis = 0;
				traffic_client->state = TRAFFIC_MON_CLIENT_STATE_LOW;
				if (traffic_client->traffic_mon_client_cb)
					traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_LOW, tput_tx, tput_rx);
			}
		}
		traffic_client->throughput = (tput_tx + tput_rx);
	}
}

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
static void traffic_mon_timer(struct timer_list *t)
#else
static void traffic_mon_timer(unsigned long data)
#endif
{
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	struct slsi_traffic_mon_clients *clients = from_timer(clients, t, timer);
	struct slsi_dev *sdev = container_of(clients, typeof(*sdev), traffic_mon_clients);
#else
	struct slsi_dev *sdev = (struct slsi_dev *)data;
#endif
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	bool stop_monitor;
	u8 i;
	u32 tput_rx = 0;
	u32 tput_tx = 0;

	if (!sdev) {
		SLSI_ERR_NODEV("invalid sdev\n");
		return;
	}

	spin_lock_bh(&sdev->traffic_mon_clients.lock);

	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
		dev = sdev->netdev[i];
		if (dev) {
			ndev_vif = netdev_priv(dev);

			if (ndev_vif) {
				u32 time_in_ms = 0;

				/* the Timer is jiffies based so resolution is not High and it may
				 * be off by a few ms. So to accurately measure the throughput find
				 * the time diff between last timer and this one
				 */
				time_in_ms = ktime_to_ms(ktime_sub(ktime_get(), ndev_vif->last_timer_time));

				/* the Timer may be any value but it still needs to calculate the
				 * throughput over a period of 1 second
				 */
				ndev_vif->num_bytes_rx_per_sec += ndev_vif->num_bytes_rx_per_timer;
				ndev_vif->num_bytes_tx_per_sec += ndev_vif->num_bytes_tx_per_timer;
				ndev_vif->report_time += time_in_ms;
				if (ndev_vif->report_time >= 1000) {
					ndev_vif->throughput_rx_bps = (ndev_vif->num_bytes_rx_per_sec * 8 / ndev_vif->report_time) * 1000;
					ndev_vif->throughput_tx_bps = (ndev_vif->num_bytes_tx_per_sec * 8 / ndev_vif->report_time) * 1000;
					ndev_vif->num_bytes_rx_per_sec = 0;
					ndev_vif->num_bytes_tx_per_sec = 0;
					ndev_vif->report_time = 0;
				}

				/* throughput per timer interval is measured but extrapolated to 1 sec */
				ndev_vif->throughput_tx = (ndev_vif->num_bytes_tx_per_timer * 8 / time_in_ms) * 1000;
				ndev_vif->throughput_rx = (ndev_vif->num_bytes_rx_per_timer * 8 / time_in_ms) * 1000;

				ndev_vif->num_bytes_tx_per_timer = 0;
				ndev_vif->num_bytes_rx_per_timer = 0;
				ndev_vif->last_timer_time = ktime_get();
				tput_tx += ndev_vif->throughput_tx;
				tput_rx += ndev_vif->throughput_rx;
			}
		}
	}

	traffic_mon_invoke_client_callback(sdev, tput_tx, tput_rx);
	stop_monitor = list_empty(&sdev->traffic_mon_clients.client_list);

	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	if (!stop_monitor)
		mod_timer(&sdev->traffic_mon_clients.timer, jiffies + msecs_to_jiffies(SLSI_TRAFFIC_MON_TIMER_PERIOD));
}

inline void slsi_traffic_mon_event_rx(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	/* Apply a correction to length to exclude IP and transport header.
	 * Can either peek into packets to derive the exact payload size
	 * or apply a rough correction to roughly calculate the throughput.
	 * rough correction  is applied with a number inbetween IP header (20 bytes) +
	 * UDP header (8 bytes) or TCP header (can be 20 bytes to 60 bytes) i.e. 40
	 */
	if (skb->len >= 40)
		ndev_vif->num_bytes_rx_per_timer += (skb->len - 40);
	else
		ndev_vif->num_bytes_rx_per_timer += skb->len;
}

inline void slsi_traffic_mon_event_tx(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_skb_cb *cb = slsi_skb_cb_get(skb);

	if ((skb->len - cb->sig_length) >= 40)
		ndev_vif->num_bytes_tx_per_timer += ((skb->len - 40) - cb->sig_length);
	else
		ndev_vif->num_bytes_tx_per_timer += (skb->len - cb->sig_length);
}

u8 slsi_traffic_mon_is_running(struct slsi_dev *sdev)
{
	u8 is_running = 0;

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	if (!list_empty(&sdev->traffic_mon_clients.client_list))
		is_running = 1;
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	return is_running;
}

int slsi_traffic_mon_client_register(
	struct slsi_dev *sdev,
	void *client_ctx,
	u32 mode,
	u32 mid_tput,
	u32 high_tput,
	void (*traffic_mon_client_cb)(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx))
{
	struct slsi_traffic_mon_client_entry *traffic_mon_client;
	bool start_monitor;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	u8 i;

	if (!client_ctx) {
		SLSI_ERR(sdev, "A client context must be provided\n");
		return -EINVAL;
	}

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	SLSI_DBG1(sdev, SLSI_HIP, "client:%p, mode:%u, mid_tput:%u, high_tput:%u\n", client_ctx, mode, mid_tput, high_tput);
	start_monitor = list_empty(&sdev->traffic_mon_clients.client_list);
	traffic_mon_client = kmalloc(sizeof(*traffic_mon_client), GFP_ATOMIC);
	if (!traffic_mon_client) {
		SLSI_ERR(sdev, "could not allocate memory for Monitor client\n");
		spin_unlock_bh(&sdev->traffic_mon_clients.lock);
		return -ENOMEM;
	}

	traffic_mon_client->client_ctx = client_ctx;
	traffic_mon_client->state = TRAFFIC_MON_CLIENT_STATE_LOW;
	traffic_mon_client->hysteresis = 0;
	traffic_mon_client->mode = mode;
	traffic_mon_client->mid_tput = mid_tput;
	traffic_mon_client->high_tput = high_tput;
	traffic_mon_client->traffic_mon_client_cb = traffic_mon_client_cb;

	/* Add to tail of monitor clients queue */
	list_add_tail(&traffic_mon_client->q, &sdev->traffic_mon_clients.client_list);

	if (start_monitor) {
		/* reset counters before starting Timer */
		for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
			dev = sdev->netdev[i];
			if (dev) {
				ndev_vif = netdev_priv(dev);

				if (ndev_vif) {
					ndev_vif->throughput_tx = 0;
					ndev_vif->throughput_rx = 0;
					ndev_vif->num_bytes_tx_per_timer = 0;
					ndev_vif->num_bytes_rx_per_timer = 0;
					ndev_vif->last_timer_time = ktime_get();
					ndev_vif->num_bytes_rx_per_sec = 0;
					ndev_vif->num_bytes_tx_per_sec = 0;
					ndev_vif->throughput_rx_bps = 0;
					ndev_vif->throughput_tx_bps = 0;
					ndev_vif->report_time = 0;
				}
			}
		}
		mod_timer(&sdev->traffic_mon_clients.timer, jiffies + msecs_to_jiffies(SLSI_TRAFFIC_MON_TIMER_PERIOD));
	}

	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	return 0;
}

void slsi_traffic_mon_client_unregister(struct slsi_dev *sdev, void *client_ctx)
{
	struct list_head       *pos, *n;
	struct slsi_traffic_mon_client_entry *traffic_mon_client;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	u8 i;

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	SLSI_DBG1(sdev, SLSI_HIP, "client: %p\n", client_ctx);
	list_for_each_safe(pos, n, &sdev->traffic_mon_clients.client_list) {
		traffic_mon_client = list_entry(pos, struct slsi_traffic_mon_client_entry, q);
		if (traffic_mon_client->client_ctx == client_ctx) {
			SLSI_DBG1(sdev, SLSI_HIP, "delete: %p\n", traffic_mon_client->client_ctx);
			list_del(pos);
			kfree(traffic_mon_client);
		}
	}

	if (list_empty(&sdev->traffic_mon_clients.client_list)) {
		/* reset counters */
		for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
			dev = sdev->netdev[i];
			if (dev) {
				ndev_vif = netdev_priv(dev);

				if (ndev_vif) {
					ndev_vif->throughput_tx = 0;
					ndev_vif->throughput_rx = 0;
					ndev_vif->num_bytes_tx_per_timer = 0;
					ndev_vif->num_bytes_rx_per_timer = 0;
					ndev_vif->num_bytes_rx_per_sec = 0;
					ndev_vif->num_bytes_tx_per_sec = 0;
					ndev_vif->throughput_rx_bps = 0;
					ndev_vif->throughput_tx_bps = 0;
					ndev_vif->report_time = 0;
				}
			}
		}
		spin_unlock_bh(&sdev->traffic_mon_clients.lock);
		del_timer_sync(&sdev->traffic_mon_clients.timer);
		spin_lock_bh(&sdev->traffic_mon_clients.lock);
	}
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
}

void slsi_traffic_mon_clients_init(struct slsi_dev *sdev)
{
	if (!sdev) {
		SLSI_ERR_NODEV("invalid sdev\n");
		return;
	}
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	timer_setup(&sdev->traffic_mon_clients.timer, traffic_mon_timer, 0);
#else
	setup_timer(&sdev->traffic_mon_clients.timer, traffic_mon_timer, (unsigned long)sdev);
#endif
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	spin_lock_init(&sdev->traffic_mon_clients.lock);
}

void slsi_traffic_mon_clients_deinit(struct slsi_dev *sdev)
{
	struct list_head       *pos, *n;
	struct slsi_traffic_mon_client_entry *traffic_mon_client;

	if (!sdev) {
		SLSI_ERR_NODEV("invalid sdev\n");
		return;
	}

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	list_for_each_safe(pos, n, &sdev->traffic_mon_clients.client_list) {
		traffic_mon_client = list_entry(pos, struct slsi_traffic_mon_client_entry, q);
		SLSI_DBG1(sdev, SLSI_HIP, "delete: %p\n", traffic_mon_client->client_ctx);
		list_del(pos);
		kfree(traffic_mon_client);
	}
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	del_timer_sync(&sdev->traffic_mon_clients.timer);
}
