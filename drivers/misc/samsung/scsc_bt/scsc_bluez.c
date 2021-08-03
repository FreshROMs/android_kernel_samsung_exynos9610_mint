/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 * BT BlueZ interface
 *
 ****************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/termios.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include "../../../bluetooth/h4_recv.h"

#include <scsc/scsc_logring.h>

static struct hci_dev      		*hdev;
static struct device 			*dev_ref;
static const struct file_operations 	*bt_fs;
static struct workqueue_struct          *wq;
static struct work_struct		open_worker;
static struct work_struct		close_worker;
static struct work_struct		read_work;
static u8				receive_buffer[1024];
static bool	                 	terminate_read;
static atomic_t 			*error_count_ref;
static wait_queue_head_t       		*read_wait_ref;
static struct file			s_file;

static struct platform_device *slsi_btz_pdev;
static struct rfkill *btz_rfkill;

static const struct h4_recv_pkt scsc_recv_pkts[] = {
	{ H4_RECV_ACL,   .recv = hci_recv_frame },
	{ H4_RECV_EVENT, .recv = hci_recv_frame },
};

static void slsi_bt_fs_read_func(struct work_struct *work)
{
	int ret;
	struct sk_buff *skb = NULL;

	while ((ret = bt_fs->read(&s_file, receive_buffer, sizeof(receive_buffer), NULL)) >= 0) {
		if (terminate_read)
			break;

		if (ret > 0) {
			skb = h4_recv_buf(hdev, skb, receive_buffer,
					ret, scsc_recv_pkts,
					ARRAY_SIZE(scsc_recv_pkts));

			if (IS_ERR(skb)) {
				SCSC_TAG_ERR(BT_COMMON, "corrupted event packet\n");
				hdev->stat.err_rx++;
				break;
			}
			hdev->stat.byte_rx += ret;
		}
	}

	SCSC_TAG_INFO(BT_COMMON, "BT BlueZ: Exiting %s\n", __func__);
}

static int slsi_bt_open(struct hci_dev *hdev)
{
	int err;

	SCSC_TAG_INFO(BT_COMMON, "enter\n");

	err = bt_fs->open(NULL, NULL);

	if (0 == err) {
		terminate_read = false;
		if (wq == NULL) {
			wq = create_singlethread_workqueue("slsi_bt_bluez_wq");
			INIT_WORK(&read_work, slsi_bt_fs_read_func);
		}
		queue_work(wq, &read_work);
	}

	SCSC_TAG_INFO(BT_COMMON, "done\n");

	return err;
}

static int slsi_bt_close(struct hci_dev *hdev)
{
	int ret;

	SCSC_TAG_INFO(BT_COMMON, "terminating reader thread\n");

	terminate_read = true;

	if (error_count_ref != NULL)
		atomic_inc(error_count_ref);

	if (read_wait_ref != NULL)
		wake_up(read_wait_ref);

	cancel_work_sync(&read_work);

	SCSC_TAG_INFO(BT_COMMON, "releasing service\n");

	ret = bt_fs->release(NULL, NULL);

	if (wq != NULL) {
		destroy_workqueue(wq);
		wq = NULL;
	}

	SCSC_TAG_INFO(BT_COMMON, "done\n");

	return ret;
}

static int slsi_bt_flush(struct hci_dev *hdev)
{
	return 0;
}

static int slsi_bt_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
	int ret;

	SCSC_TAG_DEBUG(BT_H4, "sending frame(data=%p, len=%u)\n", skb->data, skb->len);

	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);

	ret = bt_fs->write(NULL, skb->data, skb->len, NULL);
	if (ret >= 0) {
		kfree_skb(skb);

		/* Update HCI stat counters */
		hdev->stat.byte_tx += skb->len;

		switch (hci_skb_pkt_type(skb)) {
			case HCI_COMMAND_PKT:
				hdev->stat.cmd_tx++;
				break;

			case HCI_ACLDATA_PKT:
				hdev->stat.acl_tx++;
				break;

			case HCI_SCODATA_PKT:
				hdev->stat.sco_tx++;
				break;
		}
	}

	return ret;
}

static void slsi_bt_open_worker(struct work_struct *work)
{
	int err;

	if (!hdev) {
		hdev = hci_alloc_dev();
		if (!hdev) {
			SCSC_TAG_ERR(BT_COMMON, "failed to allocate hci device\n");
			return;
		}

		hdev->bus = HCI_VIRTUAL;
		hdev->dev_type = HCI_BREDR;

		SET_HCIDEV_DEV(hdev, dev_ref);

		hdev->open     = slsi_bt_open;
		hdev->close    = slsi_bt_close;
		hdev->flush    = slsi_bt_flush;
		hdev->send     = slsi_bt_send_frame;

		err = hci_register_dev(hdev);
		if (err < 0) {
			SCSC_TAG_ERR(BT_COMMON, "failed to register hci device (err: %d)\n", err);
			hci_free_dev(hdev);
			hdev = NULL;
		}
	}
}

static void slsi_bt_close_worker(struct work_struct *work)
{
	if (hdev) {
		hci_unregister_dev(hdev);
		hci_free_dev(hdev);
		hdev = NULL;
	}
}

void slsi_bt_notify_probe(struct device *dev,
			  const struct file_operations *fs,
			  atomic_t *error_count,
			  wait_queue_head_t *read_wait)
{
	bt_fs           = fs;
	error_count_ref = error_count;
	read_wait_ref   = read_wait;
	dev_ref         = dev;

	SCSC_TAG_INFO(BT_COMMON, "SLSI BT BlueZ probe\n");
}

void slsi_bt_notify_remove(void)
{
	error_count_ref = NULL;
	read_wait_ref   = NULL;
	dev_ref         = NULL;
}

static int slsi_bt_power_control_set_param_cb(const char *buffer,
					      const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(buffer, 0, &value);
	if (!ret) {
		if (value && dev_ref) {
			INIT_WORK(&open_worker, slsi_bt_open_worker);
			schedule_work(&open_worker);
		} else if (0 == value) {
			INIT_WORK(&close_worker, slsi_bt_close_worker);
			schedule_work(&close_worker);
		}
	}

	return ret;
}

static int slsi_bt_power_control_get_param_cb(char *buffer,
					      const struct kernel_param *kp)
{
	return sprintf(buffer, "%u\n", (hdev != NULL ? 1 : 0));
}

static struct kernel_param_ops slsi_bt_power_control_ops = {
	.set = slsi_bt_power_control_set_param_cb,
	.get = slsi_bt_power_control_get_param_cb,
};

module_param_cb(power_control, &slsi_bt_power_control_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(power_control,
		 "Enables/disable BlueZ registration");

static int btz_rfkill_set(void *data, bool blocked)
{
	if (!blocked && dev_ref) {
		INIT_WORK(&open_worker, slsi_bt_open_worker);
		schedule_work(&open_worker);
	} else if (blocked) {
		INIT_WORK(&close_worker, slsi_bt_close_worker);
		schedule_work(&close_worker);
	}

	return 0;
}

static const struct rfkill_ops btz_rfkill_ops = {
       .set_block = btz_rfkill_set,
};

static int __init slsi_bluez_init(void)
{
	int ret;

	slsi_btz_pdev = platform_device_alloc("scsc-bluez", -1);
	if (!slsi_btz_pdev) {
		return -ENOMEM;
	}

	ret = platform_device_add(slsi_btz_pdev);
	if (ret) {
		goto err_slsi_btz_pdev;
	}

	btz_rfkill = rfkill_alloc("scsc-bluez-rfkill", &slsi_btz_pdev->dev,
								RFKILL_TYPE_BLUETOOTH, &btz_rfkill_ops, NULL);
	if (!btz_rfkill) {
		goto err_btz_rfkill_alloc;
	}

	rfkill_init_sw_state(btz_rfkill, 1);

	ret = rfkill_register(btz_rfkill);
	if (ret) {
		goto err_btz_rfkill_reg;
	}

	return 0;

err_btz_rfkill_reg:
	rfkill_destroy(btz_rfkill);

err_btz_rfkill_alloc:
	platform_device_del(slsi_btz_pdev);

err_slsi_btz_pdev:
	platform_device_put(slsi_btz_pdev);

	return ret;
}

static void __exit slsi_bluez_exit(void)
{
	platform_device_unregister(slsi_btz_pdev);
	rfkill_unregister(btz_rfkill);
	rfkill_destroy(btz_rfkill);
}

module_init(slsi_bluez_init);
module_exit(slsi_bluez_exit);

MODULE_DESCRIPTION("SCSC BT Bluez");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
