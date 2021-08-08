/****************************************************************************
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 * Bluetooth Quality of Service
 *
 ****************************************************************************/
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
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
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <scsc/api/bsmhcp.h>
#include <scsc/scsc_logring.h>

#include "scsc_bt_priv.h"

#define SCSC_QOS_OF_TABLE_LENGTH        (12)

struct scsc_qos_of_table {
	u32 firmware_bus_high;
	u32 default_rx_throttle_bus;
	u32 default_rx_throttle_int;
	u32 default_rx_throttle_cluster1;
	u32 medium_rx_throttle_level;
	u32 medium_rx_throttle_bus;
	u32 medium_rx_throttle_int;
	u32 medium_rx_throttle_cluster1;
	u32 high_rx_throttle_level;
	u32 high_rx_throttle_bus;
	u32 high_rx_throttle_int;
	u32 high_rx_throttle_cluster1;
};

static ulong pm_qos_int_value = PM_QOS_DEVICE_THROUGHPUT_DEFAULT_VALUE;
static ulong pm_qos_bus_value = PM_QOS_BUS_THROUGHPUT_DEFAULT_VALUE;
static ulong pm_qos_cluster0_freq_min_value = 0;
static ulong pm_qos_cluster1_freq_min_value = 0;
static struct scsc_qos_service qos_service;
static struct scsc_qos_of_table qos_of_table;

static int pm_qos_set_param_cb(const char *buffer, const struct kernel_param *kp)
{
	int ret = param_set_ulong(buffer, kp);
	if (ret >= 0)
		schedule_work(&qos_service.work_queue);

	return ret;
}

static struct kernel_param_ops pm_qos_ops = {
	.set = pm_qos_set_param_cb,
	.get = param_get_ulong,
};

module_param_cb(pm_qos_int, &pm_qos_ops, &pm_qos_int_value, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_qos_int, "Device throughput");

module_param_cb(pm_qos_bus, &pm_qos_ops, &pm_qos_bus_value, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_qos_bus, "Bus throughput");

module_param_cb(pm_qos_cluster0_freq_min,
		&pm_qos_ops,
		&pm_qos_cluster0_freq_min_value,
		S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_qos_cluster0_freq_min, "Cluster 0 minimum frequency");

module_param_cb(pm_qos_cluster1_freq_min,
		&pm_qos_ops,
		&pm_qos_cluster1_freq_min_value,
		S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pm_qos_cluster1_freq_min, "Cluster 1 minimum frequency");

void scsc_bt_qos_update(uint32_t number_of_outstanding_hci_events,
			uint32_t number_of_outstanding_acl_packets)
{
	if (qos_service.of_table_present) {
		int current_state = 0;
		int next_state = 0;

		u32 current_level = max(qos_service.number_of_outstanding_acl_packets,
					qos_service.number_of_outstanding_hci_events);
		u32 next_level = max(number_of_outstanding_acl_packets,
				     number_of_outstanding_hci_events);

		/* Calculate current and next PM state */
		if (current_level > qos_of_table.high_rx_throttle_level)
			current_state = 2;
		else if (current_level > qos_of_table.medium_rx_throttle_level)
			current_state = 1;
		if (next_level > qos_of_table.high_rx_throttle_level)
			next_state = 2;
		else if (next_level > qos_of_table.medium_rx_throttle_level)
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
	/* Throttle down to minimum on Bluetooth service close */
	qos_service.number_of_outstanding_hci_events = 0;
	qos_service.number_of_outstanding_acl_packets = 0;
	if (qos_service.of_table_present)
		schedule_work(&qos_service.work_queue);
}

static void scsc_bt_qos_work(struct work_struct *data)
{
	u32 pm_qos_int_max = max((u32)pm_qos_int_value, qos_of_table.default_rx_throttle_int);
	u32 pm_qos_bus_max = max((u32)pm_qos_bus_value, qos_of_table.default_rx_throttle_bus);
	u32 pm_qos_cluster1_freq_min = max((u32)pm_qos_cluster1_freq_min_value,
					   qos_of_table.default_rx_throttle_cluster1);
	u32 level = max(qos_service.number_of_outstanding_acl_packets,
			qos_service.number_of_outstanding_hci_events);

	if (level > qos_of_table.high_rx_throttle_level) {
		pm_qos_int_max = max(pm_qos_int_max, qos_of_table.high_rx_throttle_int);
		pm_qos_bus_max = max(pm_qos_bus_max, qos_of_table.high_rx_throttle_bus);
		pm_qos_cluster1_freq_min = max(pm_qos_cluster1_freq_min,
					       qos_of_table.high_rx_throttle_cluster1);
	} else if (level > qos_of_table.medium_rx_throttle_level) {
		pm_qos_int_max = max(pm_qos_int_max, qos_of_table.medium_rx_throttle_int);
		pm_qos_bus_max = max(pm_qos_bus_max, qos_of_table.medium_rx_throttle_bus);
		pm_qos_cluster1_freq_min = max(pm_qos_cluster1_freq_min,
					       qos_of_table.medium_rx_throttle_cluster1);
	}

	SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth QoS update (Level: %u)\n", level);

	pm_qos_update_request(&qos_service.pm_qos_int,
			      pm_qos_int_max);
	pm_qos_update_request(&qos_service.pm_qos_bus,
			      pm_qos_bus_max);
	pm_qos_update_request(&qos_service.pm_qos_cluster0_freq_min,
			      pm_qos_cluster0_freq_min_value);
	pm_qos_update_request(&qos_service.pm_qos_cluster1_freq_min,
			      pm_qos_cluster1_freq_min);
}

void scsc_bt_qos_init(void)
{
	pm_qos_add_request(&qos_service.pm_qos_int,
			   PM_QOS_DEVICE_THROUGHPUT,
			   pm_qos_int_value);
	pm_qos_add_request(&qos_service.pm_qos_bus,
			   PM_QOS_BUS_THROUGHPUT,
			   pm_qos_bus_value);
	pm_qos_add_request(&qos_service.pm_qos_cluster0_freq_min,
			   PM_QOS_CLUSTER0_FREQ_MIN,
			   pm_qos_cluster0_freq_min_value);
	pm_qos_add_request(&qos_service.pm_qos_cluster1_freq_min,
			   PM_QOS_CLUSTER1_FREQ_MIN,
			   pm_qos_cluster1_freq_min_value);

	INIT_WORK(&qos_service.work_queue, scsc_bt_qos_work);
}

void scsc_bt_qos_deinit(void)
{
	pm_qos_remove_request(&qos_service.pm_qos_int);
	pm_qos_remove_request(&qos_service.pm_qos_bus);
	pm_qos_remove_request(&qos_service.pm_qos_cluster0_freq_min);
	pm_qos_remove_request(&qos_service.pm_qos_cluster1_freq_min);
}

static int scsc_bt_qos_probe(struct platform_device *pdev)
{
	int ret = 0;
	int len = of_property_count_u32_elems(pdev->dev.of_node, "bluetooth_qos");

	if (len == SCSC_QOS_OF_TABLE_LENGTH) {
		ret = of_property_read_u32_array(pdev->dev.of_node,
						 "bluetooth_qos",
						 (u32*)&qos_of_table,
						 len);
		qos_service.of_table_present = (ret == 0);
	}

	return 0;
}

static int scsc_bt_qos_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id scsc_bt_qos_match[] = {
	{ .compatible = "samsung,scsc_bt_qos", },
	{},
};
MODULE_DEVICE_TABLE(of, scsc_bt_qos_match);

static struct platform_driver scsc_bt_qos_driver = {
	.probe          = scsc_bt_qos_probe,
	.remove         = scsc_bt_qos_remove,
	.driver         = {
		.name = "scsc_bt_qos",
		.owner = THIS_MODULE,
		.of_match_table = scsc_bt_qos_match,
	}
};
module_platform_driver(scsc_bt_qos_driver);
