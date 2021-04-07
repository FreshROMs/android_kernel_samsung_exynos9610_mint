/* sound/soc/samsung/abox/abox_synchronized_ipc.c
 *
 * ALSA SoC Audio Layer - Samsung Abox synchronized IPC driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include <sound/sec_synchronized_ipc_richtek.h>
#include "abox.h"

#define REALTIME_GEAR_ID    0x7007

#define TIMEOUT_MS 100
#define DEBUG_SYNCHRONIZED_IPC

#ifdef DEBUG_SYNCHRONIZED_IPC
#define ipc_dbg(format, args...)	\
pr_info("[SYNC_IPC] %s: " format "\n", __func__, ## args)
#else
#define ipc_dbg(format, args...)
#endif /* DEBUG_SYNCHRONIZED_IPC */

#define ipc_err(format, args...)	\
pr_err("[SYNC_IPC] %s: " format "\n", __func__, ## args)


static DECLARE_WAIT_QUEUE_HEAD(wq_read);
static DECLARE_WAIT_QUEUE_HEAD(wq_write);

static struct abox_platform_data *data;
static void *richtek_read_buf = NULL;
static int richtek_read_size = 0;
static int abox_ipc_read_error = 0;
static int abox_ipc_write_error = 0;
static bool abox_ipc_read_avail = false;
static bool abox_ipc_write_avail = false;

int richtek_spm_read(void *buf, int size)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct abox_data *aboxdata = abox_get_abox_data();

	ipc_dbg("size = %d", size);

	abox_request_cpu_gear(&data->pdev_abox->dev, aboxdata,
					REALTIME_GEAR_ID, 4);

	richtek_read_buf = (void *)buf;
	richtek_read_size = size;

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	memcpy(&erap_msg->param.raw, buf, size);
	abox_ipc_read_avail = false;
	abox_ipc_read_error = 0;
	ret = abox_start_ipc_transaction(&data->pdev_abox->dev,
					 IPC_ERAP, &msg, sizeof(msg), 0, 0);
	if (ret < 0) {
		ipc_err("abox_start_ipc_transaction is failed, error=%d", ret);
		/*return -1;*/
	}

	ret = wait_event_interruptible_timeout(wq_read,
			abox_ipc_read_avail, msecs_to_jiffies(TIMEOUT_MS));

	abox_request_cpu_gear(&data->pdev_abox->dev, aboxdata,
					REALTIME_GEAR_ID, 12);

	if (!ret) {
		ipc_err("wait_event timeout");
		return -1;
	}
	if (abox_ipc_read_error) {
		ipc_err("error = %d", abox_ipc_read_error);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(richtek_spm_read);

int richtek_spm_write(const void *buf, int size)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct abox_data *aboxdata = abox_get_abox_data();

	ipc_dbg("size = %d", size);

	abox_request_cpu_gear(&data->pdev_abox->dev, aboxdata,
					REALTIME_GEAR_ID, 4);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	memcpy(&erap_msg->param.raw, buf, size);

	abox_ipc_write_avail = false;
	abox_ipc_write_error = 0;

	ret = abox_start_ipc_transaction(&data->pdev_abox->dev,
					 IPC_ERAP, &msg, sizeof(msg), 0, 0);
	if (ret < 0) {
		ipc_err("abox_start_ipc_transaction is failed, error=%d", ret);
		/*return -1;*/
	}

	ret = wait_event_interruptible_timeout(wq_write,
		abox_ipc_write_avail, msecs_to_jiffies(TIMEOUT_MS));

	abox_request_cpu_gear(&data->pdev_abox->dev, aboxdata,
					REALTIME_GEAR_ID, 12);

	if (!ret) {
		ipc_err("wait_event timeout");
		return -1;
	}
	if (abox_ipc_write_error) {
		ipc_err("abox_ipc_write_error = %d", abox_ipc_write_error);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(richtek_spm_write);

static irqreturn_t abox_synchronized_ipc_handler(int irq,
					void *dev_id, ABOX_IPC_MSG *msg)
{
	struct IPC_ERAP_MSG *erap_msg = &msg->msg.erap;
	/* struct abox_data *aboxdata = abox_get_abox_data(); */
	unsigned int res_id = erap_msg->param.raw.params[0];
	/* unsigned int size = erap_msg->param.raw.params[1]; */

	ipc_dbg("irq[%d], type[%d], cmd[%d]\n", irq, erap_msg->msgtype, res_id);
	switch (irq) {
		case IPC_ERAP:
			switch (erap_msg->msgtype) {
			case REALTIME_EXTRA:
				switch(res_id) {
				/* read */
				case 0:
					memcpy(richtek_read_buf,
					       &erap_msg->param.raw, richtek_read_size);
					abox_ipc_read_avail = true;
					abox_ipc_read_error = 0;
					ipc_dbg("CMD_READ done");
					if (waitqueue_active(&wq_read))
						wake_up_interruptible(&wq_read);
					break;
				/* write */
				case 1:
					abox_ipc_write_avail = true;
					abox_ipc_write_error = 0;
					ipc_dbg("CMD_WRITE done");
					if (waitqueue_active(&wq_write))
						wake_up_interruptible(&wq_write);
					break;
				/* read error */
				case 0xfffffffe:
					abox_ipc_read_avail = true;
					abox_ipc_read_error = 1;
					ipc_dbg("CMD_READ error");
					if (waitqueue_active(&wq_read))
						wake_up_interruptible(&wq_read);
					break;
				/* write error */
				case 0xffffffff:
					abox_ipc_write_avail = true;
					abox_ipc_write_error = 1;
					ipc_dbg("CMD_WRITE error");
					if (waitqueue_active(&wq_write))
						wake_up_interruptible(&wq_write);
					break;
				default:
					ipc_err("unknown response type, RES_ID = %d", res_id);
					break;
				}
				break;
			default:
				ipc_err("unknown message type, msgtype = %d",
								erap_msg->msgtype);
				break;
			}
			break;
		default:
			ipc_err("unknown command, irq = %d", irq);
			break;
	}
	return IRQ_HANDLED;
}

static struct snd_soc_platform_driver abox_synchronized_ipc = {
};

static int samsung_abox_synchronized_ipc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_abox;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "[SYNC_IPC] Failed to allocate memory\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, data);

	np_abox = of_parse_phandle(np, "abox", 0);
	if (!np_abox) {
		dev_err(dev, "[SYNC_IPC] Failed to get abox device node\n");
		return -EPROBE_DEFER;
	}
	data->pdev_abox = of_find_device_by_node(np_abox);
	if (!data->pdev_abox) {
		dev_err(dev, "[SYNC_IPC] Failed to get abox platform device\n");
		return -EPROBE_DEFER;
	}
	data->abox_data = platform_get_drvdata(data->pdev_abox);

	abox_register_irq_handler(&data->pdev_abox->dev, IPC_ERAP,
				abox_synchronized_ipc_handler, pdev);

	return snd_soc_register_platform(&pdev->dev, &abox_synchronized_ipc);
}

static int samsung_abox_synchronized_ipc_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id samsung_abox_synchronized_ipc_match[] = {
	{
		.compatible = "samsung,abox-synchronized-ipc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_synchronized_ipc_match);

static struct platform_driver samsung_abox_synchronized_ipc_driver = {
	.probe  = samsung_abox_synchronized_ipc_probe,
	.remove = samsung_abox_synchronized_ipc_remove,
	.driver = {
	.name = "samsung-abox-synchronized-ipc",
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(samsung_abox_synchronized_ipc_match),
	},
};
module_platform_driver(samsung_abox_synchronized_ipc_driver);

/* Module information */
MODULE_AUTHOR("SeokYoung Jang, <quartz.jang@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC A-Box Synchronized IPC Driver");
MODULE_ALIAS("platform:samsung-abox-synchronized-ipc");
MODULE_LICENSE("GPL");
