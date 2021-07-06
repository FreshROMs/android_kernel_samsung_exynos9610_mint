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

/*#include <sound/abox_synchronized_ipc.h>*/
#include <sound/tfa_ext.h>
#include "abox.h"

#define TFADSP_CMD_WRITE    0x1001
#define TFADSP_CMD_READ     0x1002
#define TFADSP_CMD_BLACKBOX 0x1003
#define TFADSP_RES_WRITE    0x2001
#define TFADSP_RES_READ     0x2002
#define TFADSP_RES_BLACKBOX 0x2003
#define TFADSP_SUCCESS              0
#define TFADSP_WRITE_INVALID_STATE  1
#define TFADSP_INVALID_PARAM        2
#define TFADSP_FAILED               3
#define TFADSP_READ_INVALID_STATE   4
#define REALTIME_GEAR_ID    0x7007

#define SMEM_READ           0x80584000
#define SMEM_WRITE          0x80582000
/*#define SMEM_READ           0x80840000*/
/*#define SMEM_WRITE          0x80842000*/

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
char *tfadrv_read_buf = NULL;
char *tfadrv_write_buf = NULL;
char *smem_write_buf = NULL;
char *smem_read_buf = NULL;
int abox_ipc_read_error = 0;
int abox_ipc_write_error = 0;
bool abox_ipc_read_avail = false;
bool abox_ipc_write_avail = false;

static bool abox_ipc_timeout_state = false;

int tfadsp_read(int devidx, int size, void *buf)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct abox_data *aboxdata = abox_get_abox_data();

	ipc_dbg("size = %d", size);

	if(abox_ipc_timeout_state == true) {
		ipc_err("ipc timeout state, skip to tfadsp_read");
	}

	abox_request_cpu_gear(&data->pdev_abox->dev, aboxdata,
					REALTIME_GEAR_ID, 4);

	tfadrv_read_buf = (char *)buf;

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = TFADSP_CMD_READ;
	erap_msg->param.raw.params[1] = size;
	abox_ipc_read_avail = false;
	abox_ipc_read_error = TFADSP_SUCCESS;
	ret = abox_start_ipc_transaction(&data->pdev_abox->dev, IPC_ERAP,
					&msg, sizeof(msg), 1, 1);
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
		abox_ipc_timeout_state = true;
		return 0;
	}
	if (abox_ipc_read_error) {
		ipc_err("error = %d", abox_ipc_read_error);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(tfadsp_read);

int tfadsp_write(int devidx, int size, void *buf)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct abox_data *aboxdata = abox_get_abox_data();

	ipc_dbg("size = %d", size);

	if(abox_ipc_timeout_state == true) {
		ipc_err("ipc timeout state, skip to tfadsp_write");
	}

	abox_request_cpu_gear(&data->pdev_abox->dev, aboxdata,
					REALTIME_GEAR_ID, 4);

	tfadrv_write_buf = (char *)buf;
	smem_write_buf = (char *)abox_addr_to_kernel_addr(aboxdata, SMEM_WRITE);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = TFADSP_CMD_WRITE;
	erap_msg->param.raw.params[1] = size;

	memcpy(smem_write_buf, tfadrv_write_buf, size);
	abox_ipc_write_avail = false;
	abox_ipc_write_error = TFADSP_SUCCESS;

	ret = abox_start_ipc_transaction(&data->pdev_abox->dev, IPC_ERAP,
					&msg, sizeof(msg), 1, 1);
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
		abox_ipc_timeout_state = true;
		return 0;
	}
	if (abox_ipc_write_error) {
		ipc_err("abox_ipc_write_error = %d", abox_ipc_write_error);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(tfadsp_write);

static irqreturn_t abox_synchronized_ipc_handler(int irq,
					void *dev_id, ABOX_IPC_MSG *msg)
{
	struct IPC_ERAP_MSG *erap_msg = &msg->msg.erap;
	struct abox_data *aboxdata = abox_get_abox_data();
	unsigned int res_id = erap_msg->param.raw.params[0];
	unsigned int size = erap_msg->param.raw.params[1];

	switch (irq) {
	case IPC_ERAP:
	switch (erap_msg->msgtype) {
	case REALTIME_EXTRA:
	switch(res_id) {
		case TFADSP_RES_READ:
			smem_read_buf =
			(char *)abox_addr_to_kernel_addr(aboxdata, SMEM_READ);
			memcpy(tfadrv_read_buf, smem_read_buf, size);
			abox_ipc_read_avail = true;
			abox_ipc_read_error = TFADSP_SUCCESS;
			ipc_dbg("TFADSP_CMD_READ DONE size= %d", size);
			if (waitqueue_active(&wq_read))
				wake_up_interruptible(&wq_read);
		break;
		case TFADSP_RES_BLACKBOX:
			smem_read_buf =
			(char *)abox_addr_to_kernel_addr(aboxdata, SMEM_READ);
			memcpy(tfadrv_read_buf, smem_read_buf, size);
			abox_ipc_read_avail = true;
			abox_ipc_read_error = TFADSP_SUCCESS;
			ipc_dbg("TFADSP_CMD_BLACKBOX DONE size= %d", size);
			if (waitqueue_active(&wq_read))
				wake_up_interruptible(&wq_read);
		break;
		case TFADSP_READ_INVALID_STATE:
			abox_ipc_read_avail = true;
			abox_ipc_read_error = TFADSP_READ_INVALID_STATE;
			ipc_err("tfadsp_read() TFADSP_READ_INVALID_STATE");
			if (waitqueue_active(&wq_read))
				wake_up_interruptible(&wq_read);
		break;
		case TFADSP_RES_WRITE:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = TFADSP_SUCCESS;
			ipc_dbg("TFADSP_CMD_WRITE done");
			if (waitqueue_active(&wq_write))
				wake_up_interruptible(&wq_write);
		break;
		case TFADSP_WRITE_INVALID_STATE:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = TFADSP_WRITE_INVALID_STATE;
			ipc_err("tfadsp_write() TFADSP_WRITE_INVALID_STATE");
			if (waitqueue_active(&wq_write))
				wake_up_interruptible(&wq_write);
		break;
		case TFADSP_INVALID_PARAM:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = TFADSP_INVALID_PARAM;
			ipc_err("tfadsp_write() TFADSP_INVALID_PARAM");
			if (waitqueue_active(&wq_write))
				wake_up_interruptible(&wq_write);
		break;
		case TFADSP_FAILED:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = TFADSP_FAILED;
			ipc_err("tfadsp_write() TFADSP_FAILED");
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

	tfa_ext_register((dsp_send_message_t)tfadsp_write,
			(dsp_read_message_t)tfadsp_read, NULL);

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
