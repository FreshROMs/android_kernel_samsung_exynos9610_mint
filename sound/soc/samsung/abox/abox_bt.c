/* sound/soc/samsung/abox/abox_bt.c
 *
 * ALSA SoC Audio Layer - Samsung Abox SCSC B/T driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/shm_ipc.h>
#include <linux/io.h>
#include <linux/iommu.h>

#include <scsc/api/bt_audio.h>
#include "../../../../drivers/iommu/exynos-iommu.h"

#include "abox.h"
#include "abox_bt.h"

static int abox_bt_sco_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_bt_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	enum bt_sco dir = (enum bt_sco)mc->reg;

	dev_dbg(dev, "%s(%d)\n", __func__, dir);

	ucontrol->value.integer.value[0] = data->active[dir];
	return 0;
}

static int abox_bt_sco_put_ipc(struct device *dev,
		enum bt_sco dir, unsigned int val)
{
	struct device *dev_abox = dev->parent;
	struct abox_bt_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	int ret;

	dev_dbg(dev, "%s(%d, %u)\n", __func__, dir, val);

	if (!data->paddr_bt) {
		dev_warn(dev, "B/T SCO isn't ready yet\n");
		return -EIO;
	}

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_BT_SCO_ENABLE;
	system_msg->param1 = val;
	system_msg->param2 = IOVA_BLUETOOTH;
	system_msg->param3 = dir;
	ret = abox_request_ipc(dev_abox, msg.ipcid, &msg, sizeof(msg), 0, 0);

	data->active[dir] = val;

	return ret;
}

static int abox_bt_sco_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	enum bt_sco dir = (enum bt_sco)mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(%d, %u)\n", __func__, dir, val);

	return abox_bt_sco_put_ipc(dev, dir, val);
}

static const struct snd_kcontrol_new abox_bt_controls[] = {
	SOC_SINGLE_EXT("BT SCO SPK Enable", BT_SCO_SPK, 0, 1, 0,
		abox_bt_sco_get, abox_bt_sco_put),
	SOC_SINGLE_EXT("BT SCO MIC Enable", BT_SCO_MIC, 0, 1, 0,
		abox_bt_sco_get, abox_bt_sco_put),
};

static const struct snd_soc_component_driver abox_bt_cmpnt = {
	.controls	= abox_bt_controls,
	.num_controls	= ARRAY_SIZE(abox_bt_controls),
};

unsigned int abox_bt_get_rate(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	return scsc_bt_audio_get_rate(0);
}

unsigned int abox_bt_get_buf_iova(struct device *dev, int stream)
{
	struct abox_bt_data *data = dev_get_drvdata(dev);
	bool tx = (stream == SNDRV_PCM_STREAM_PLAYBACK);
	phys_addr_t paddr_buf = scsc_bt_audio_get_paddr_buf(tx);

	dev_dbg(dev, "%s\n", __func__);

	if (paddr_buf && data->paddr_bt)
		return IOVA_BLUETOOTH + (paddr_buf - data->paddr_bt);
	else
		return 0;
}

bool abox_bt_active(struct device *dev, int stream)
{
	struct abox_bt_data *data = dev_get_drvdata(dev);
	enum bt_sco dir;

	dev_dbg(dev, "%s\n", __func__);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = BT_SCO_MIC;
	else
		dir = BT_SCO_SPK;

	return data->active[dir];
}

static int abox_bt_iommu_map(struct device *dev, phys_addr_t paddr, size_t size)
{
	struct abox_bt_data *data = dev_get_drvdata(dev);
	struct iommu_domain *domain = data->abox_data->iommu_domain;

	dev_info(dev, "%s(%pa, %#zx)\n", __func__, &paddr, size);

	data->paddr_bt = paddr;
	return iommu_map(domain, IOVA_BLUETOOTH, paddr, size, 0);
}

static void abox_bt_iommu_unmap(struct device *dev, size_t size)
{
	struct abox_bt_data *data = dev_get_drvdata(dev);
	struct iommu_domain *domain = data->abox_data->iommu_domain;

	dev_info(dev, "%s(%#zx)\n", __func__, size);

	iommu_unmap(domain, IOVA_BLUETOOTH, size);
	exynos_sysmmu_tlb_invalidate(domain, IOVA_BLUETOOTH, size);
}

static int samsung_abox_bt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *dev_abox = dev->parent;
	struct abox_data *abox_data = dev_get_drvdata(dev_abox);
	struct abox_bt_data *data;
	struct resource *res;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(dev, data);
	data->dev = dev;
	data->abox_data = abox_data;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mailbox");
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", "mailbox");
		return -EINVAL;
	}
	abox_iommu_map(dev_abox, res->start, res->start, resource_size(res), 0);

	ret = scsc_bt_audio_register(dev, abox_bt_iommu_map,
			abox_bt_iommu_unmap);
	if (ret < 0)
		return -EPROBE_DEFER;

	ret = devm_snd_soc_register_component(dev, &abox_bt_cmpnt, NULL, 0);
	if (ret < 0) {
		dev_err(dev, "component register failed:%d\n", ret);
		return ret;
	}

	abox_data->dev_bt = dev;

	return ret;
}

static int samsung_abox_bt_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	scsc_bt_audio_unregister(dev);

	return 0;
}

static const struct of_device_id samsung_abox_bt_match[] = {
	{
		.compatible = "samsung,abox-bt",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_bt_match);

static struct platform_driver samsung_abox_bt_driver = {
	.probe  = samsung_abox_bt_probe,
	.remove = samsung_abox_bt_remove,
	.driver = {
		.name = "samsung-abox-bt",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_bt_match),
	},
};

module_platform_driver(samsung_abox_bt_driver);

MODULE_AUTHOR("Gyeongtaek Lee, <gt82.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC A-Box SCSC Bluetooth Driver");
MODULE_ALIAS("platform:samsung-abox-bt");
MODULE_LICENSE("GPL");
