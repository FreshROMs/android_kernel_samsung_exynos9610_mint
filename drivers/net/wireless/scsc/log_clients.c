/****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "dev.h"
#include "debug.h"
#include "udi.h"
#include "log_clients.h"
#include "unifiio.h"

/* These functions should NOT be called from interrupt context */
/* It is supposed to be called from process context, or
 * NET_TX_SOFTIRQ - with BHs disabled and interrupts disabled
 */
/* Do not sleep */

void slsi_log_clients_log_signal_safe(struct slsi_dev *sdev, struct sk_buff *skb, u32 direction)
{
	struct list_head       *pos, *n;
	struct slsi_log_client *log_client;
	int                    dir = (direction == SLSI_LOG_DIRECTION_FROM_HOST) ? UDI_FROM_HOST : UDI_TO_HOST;

	spin_lock_bh(&sdev->log_clients.log_client_spinlock);
	list_for_each_safe(pos, n, &sdev->log_clients.log_client_list) {
		log_client = list_entry(pos, struct slsi_log_client, q);
		log_client->log_client_cb(log_client, skb, dir);
	}
	spin_unlock_bh(&sdev->log_clients.log_client_spinlock);
}

void slsi_log_clients_init(struct slsi_dev *sdev)
{
	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);
	spin_lock_init(&sdev->log_clients.log_client_spinlock);
}

/* The arg called "filter" will eventually be passed to kfree().
 * - so pass a NULL if you are not doing any filtering
 */
int slsi_log_client_register(struct slsi_dev *sdev, void *log_client_ctx,
			     int (*log_client_cb)(struct slsi_log_client *, struct sk_buff *, int),
			     char *filter, int min_signal_id, int max_signal_id)
{
	struct slsi_log_client *log_client;
	int                    first_in_list = 0;

	first_in_list = list_empty(&sdev->log_clients.log_client_list);
	log_client = kmalloc(sizeof(*log_client), GFP_KERNEL);
	if (log_client == NULL)
		return -ENOMEM;

	log_client->min_signal_id = min_signal_id;
	log_client->max_signal_id = max_signal_id;
	log_client->signal_filter = filter;
	log_client->log_client_ctx = log_client_ctx;
	log_client->log_client_cb = log_client_cb;

	/* Add to tail of log queue */
	spin_lock_bh(&sdev->log_clients.log_client_spinlock);
	list_add_tail(&log_client->q, &sdev->log_clients.log_client_list);
	spin_unlock_bh(&sdev->log_clients.log_client_spinlock);

	return 0;
}

void slsi_log_clients_terminate(struct slsi_dev *sdev)
{
	/* If the driver is configured to try and terminate UDI user space
	 * applications, the following will try to do so.
	 */
	if (*sdev->term_udi_users) {
		int          num_polls_left = 50;
		unsigned int timeout_ms = 4;

		slsi_log_client_msg(sdev, UDI_DRV_UNLOAD_IND, 0, NULL);

		/* Poll until all refs have gone away or timeout */
		while (slsi_check_cdev_refs() && num_polls_left) {
			msleep(timeout_ms);
			num_polls_left--;
		}
	}
}

void slsi_log_client_msg(struct slsi_dev *sdev, u16 event, u32 event_data_length, const u8 *event_data)
{
	struct list_head       *pos, *n;
	struct slsi_log_client *log_client;

	spin_lock_bh(&sdev->log_clients.log_client_spinlock);
	list_for_each_safe(pos, n, &sdev->log_clients.log_client_list) {
		log_client = list_entry(pos, struct slsi_log_client, q);
		spin_unlock_bh(&sdev->log_clients.log_client_spinlock);
		if (slsi_kernel_to_user_space_event(log_client, event, event_data_length, event_data))
			SLSI_WARN(sdev, "Failed to send event(0x%.4X) to UDI client 0x%p\n", event, log_client);
		spin_lock_bh(&sdev->log_clients.log_client_spinlock);
	}
	spin_unlock_bh(&sdev->log_clients.log_client_spinlock);
}

void slsi_log_client_unregister(struct slsi_dev *sdev, void *log_client_ctx)
{
	struct list_head       *pos, *n;
	struct slsi_log_client *log_client;

	spin_lock_bh(&sdev->log_clients.log_client_spinlock);
	list_for_each_safe(pos, n, &sdev->log_clients.log_client_list) {
		log_client = list_entry(pos, struct slsi_log_client, q);
		if (log_client->log_client_ctx == log_client_ctx) {
			kfree(log_client->signal_filter);
			list_del(pos);
			kfree(log_client);
		}
	}
	spin_unlock_bh(&sdev->log_clients.log_client_spinlock);
}
