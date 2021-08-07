/****************************************************************************
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 * Bluetooth Quality of Service
 *
 ****************************************************************************/
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <asm/termios.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos_pm_qos.h>
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <scsc/scsc_logring.h>

#include "scsc_bt_priv.h"

#ifdef CONFIG_SCSC_QOS

#define SCSC_BT_QOS_HIGH_LEVEL 20
#define SCSC_BT_QOS_MED_LEVEL 10

static struct scsc_qos_service qos_service;

static DEFINE_MUTEX(bt_qos_mutex);

static void scsc_bt_qos_work(struct work_struct *data)
{
	mutex_lock(&bt_qos_mutex);
	if (qos_service.enabled) {
		enum scsc_qos_config state = SCSC_QOS_DISABLED;
		u32 level = max(qos_service.number_of_outstanding_acl_packets,
				qos_service.number_of_outstanding_hci_events);

		if (level > SCSC_BT_QOS_HIGH_LEVEL)
			state = SCSC_QOS_MAX;
		else if (level > SCSC_BT_QOS_MED_LEVEL)
			state = SCSC_QOS_MED;

		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth QoS update (Level: %u)\n", level);

		scsc_service_pm_qos_update_request(bt_service.service, state);
	}
	mutex_unlock(&bt_qos_mutex);
}

void scsc_bt_qos_update(uint32_t number_of_outstanding_hci_events,
			uint32_t number_of_outstanding_acl_packets)
{
	if (qos_service.enabled) {
		int current_state = 0;
		int next_state = 0;

		u32 current_level = max(qos_service.number_of_outstanding_acl_packets,
					qos_service.number_of_outstanding_hci_events);
		u32 next_level = max(number_of_outstanding_acl_packets,
				     number_of_outstanding_hci_events);

		/* Calculate current and next PM state */
		if (current_level > SCSC_BT_QOS_HIGH_LEVEL)
			current_state = 2;
		else if (current_level > SCSC_BT_QOS_MED_LEVEL)
			current_state = 1;
		if (next_level > SCSC_BT_QOS_HIGH_LEVEL)
			next_state = 2;
		else if (next_level > SCSC_BT_QOS_MED_LEVEL)
			next_state = 1;

		/* Save current levels */
		qos_service.number_of_outstanding_hci_events = number_of_outstanding_hci_events;
		qos_service.number_of_outstanding_acl_packets = number_of_outstanding_acl_packets;

		/* Update PM QoS settings if state differs */
		if ((next_state > current_state) || (current_state > 0 && next_state == 0))
			schedule_work(&qos_service.work_queue);
	}
}

void scsc_bt_qos_service_stop(void)
{
	/* Ensure no crossing of work and stop */
	mutex_lock(&bt_qos_mutex);
	qos_service.number_of_outstanding_hci_events = 0;
	qos_service.number_of_outstanding_acl_packets = 0;
	if (qos_service.enabled) {
		scsc_service_pm_qos_remove_request(bt_service.service);
		qos_service.enabled = false;
	}
	mutex_unlock(&bt_qos_mutex);
}

void scsc_bt_qos_service_start(void)
{
	if (!scsc_service_pm_qos_add_request(bt_service.service, SCSC_QOS_DISABLED))
		qos_service.enabled = true;
	else
		qos_service.enabled = false;

	INIT_WORK(&qos_service.work_queue, scsc_bt_qos_work);
}

void scsc_bt_qos_service_init(void)
{
	qos_service.enabled = false;
	qos_service.number_of_outstanding_hci_events = 0;
	qos_service.number_of_outstanding_acl_packets = 0;
}

#endif /* CONFIG_SCSC_QOS */
