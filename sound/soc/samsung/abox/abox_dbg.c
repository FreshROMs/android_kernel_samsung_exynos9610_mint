/* sound/soc/samsung/abox/abox_dbg.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Debug driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/io.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include "abox_dbg.h"
#include "abox_gic.h"

#define ABOX_DBG_DUMP_MAGIC_SRAM	0x3935303030504D44ull /* DMP00059 */
#define ABOX_DBG_DUMP_MAGIC_DRAM	0x3231303038504D44ull /* DMP80012 */
#define ABOX_DBG_DUMP_MAGIC_SFR		0x5246533030504D44ull /* DMP00SFR */

static struct dentry *abox_dbg_root_dir __read_mostly;

struct dentry *abox_dbg_get_root_dir(void)
{
	pr_debug("%s\n", __func__);

	if (abox_dbg_root_dir == NULL)
		abox_dbg_root_dir = debugfs_create_dir("abox", NULL);

	return abox_dbg_root_dir;
}

void abox_dbg_print_gpr_from_addr(struct device *dev, struct abox_data *data,
		unsigned int *addr)
{
	int i;
	char version[4];

	memcpy(version, &data->calliope_version, sizeof(version));

	dev_info(dev, "========================================\n");
	dev_info(dev, "A-Box CPU register dump (%c%c%c%c)\n",
			version[3], version[2], version[1], version[0]);
	dev_info(dev, "----------------------------------------\n");
	for (i = 0; i <= 14; i++)
		dev_info(dev, "CA7_R%02d        : %08x\n", i, *addr++);
	dev_info(dev, "CA7_PC         : %08x\n", *addr++);
	dev_info(dev, "========================================\n");
}

void abox_dbg_print_gpr(struct device *dev, struct abox_data *data)
{
	int i;
	char version[4];

	memcpy(version, &data->calliope_version, sizeof(version));

	dev_info(dev, "========================================\n");
	dev_info(dev, "A-Box CPU register dump (%c%c%c%c)\n",
			version[3], version[2], version[1], version[0]);
	dev_info(dev, "----------------------------------------\n");
	for (i = 0; i <= 14; i++) {
		dev_info(dev, "CA7_R%02d        : %08x\n", i,
				readl(data->sfr_base + ABOX_CPU_R(i)));
	}
	dev_info(dev, "CA7_PC         : %08x\n",
			readl(data->sfr_base + ABOX_CPU_PC));
	dev_info(dev, "CA7_L2C_STATUS : %08x\n",
			readl(data->sfr_base + ABOX_CPU_L2C_STATUS));
	dev_info(dev, "========================================\n");
}

struct abox_dbg_dump_sram {
	unsigned long long magic;
	char dump[SZ_512K];
} __packed;

struct abox_dbg_dump_dram {
	unsigned long long magic;
	char dump[DRAM_FIRMWARE_SIZE];
} __packed;

struct abox_dbg_dump_sfr {
	unsigned long long magic;
	u32 dump[SZ_64K / sizeof(u32)];
} __packed;

struct abox_dbg_dump {
	struct abox_dbg_dump_sram sram;
	struct abox_dbg_dump_dram dram;
	struct abox_dbg_dump_sfr sfr;
	u32 sfr_gic_gicd[SZ_4K / sizeof(u32)];
	unsigned int gpr[17];
	long long time;
	char reason[SZ_32];
} __packed;

struct abox_dbg_dump_min {
	struct abox_dbg_dump_sram sram;
	struct abox_dbg_dump_sfr sfr;
	u32 sfr_gic_gicd[SZ_4K / sizeof(u32)];
	unsigned int gpr[17];
	long long time;
	char reason[SZ_32];
} __packed;

static struct abox_dbg_dump (*p_abox_dbg_dump)[ABOX_DBG_DUMP_COUNT];
static struct abox_dbg_dump_min (*p_abox_dbg_dump_min)[ABOX_DBG_DUMP_COUNT];
static struct reserved_mem *abox_rmem;

static int __init abox_rmem_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);

	abox_rmem = rmem;
	if (sizeof(*p_abox_dbg_dump) <= abox_rmem->size)
		p_abox_dbg_dump = phys_to_virt(abox_rmem->base);
	else if (sizeof(*p_abox_dbg_dump_min) <= abox_rmem->size)
		p_abox_dbg_dump_min = phys_to_virt(abox_rmem->base);

	return 0;
}

RESERVEDMEM_OF_DECLARE(abox_rmem, "exynos,abox_rmem", abox_rmem_setup);

void abox_dbg_dump_gpr_from_addr(struct device *dev, unsigned int *addr,
		enum abox_dbg_dump_src src, const char *reason)
{
	int i;

	dev_dbg(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[src];

		p_dump->time = sched_clock();
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		for (i = 0; i <= 14; i++)
			p_dump->gpr[i] = *addr++;
		p_dump->gpr[i++] = *addr++;
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[src];

		p_dump->time = sched_clock();
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		for (i = 0; i <= 14; i++)
			p_dump->gpr[i] = *addr++;
		p_dump->gpr[i++] = *addr++;
	}
}

void abox_dbg_dump_gpr(struct device *dev, struct abox_data *data,
		enum abox_dbg_dump_src src, const char *reason)
{
	int i;

	dev_dbg(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[src];

		p_dump->time = sched_clock();
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		for (i = 0; i <= 14; i++)
			p_dump->gpr[i] = readl(data->sfr_base + ABOX_CPU_R(i));
		p_dump->gpr[i++] = readl(data->sfr_base + ABOX_CPU_PC);
		p_dump->gpr[i++] = readl(data->sfr_base + ABOX_CPU_L2C_STATUS);
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[src];

		p_dump->time = sched_clock();
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		for (i = 0; i <= 14; i++)
			p_dump->gpr[i] = readl(data->sfr_base + ABOX_CPU_R(i));
		p_dump->gpr[i++] = readl(data->sfr_base + ABOX_CPU_PC);
		p_dump->gpr[i++] = readl(data->sfr_base + ABOX_CPU_L2C_STATUS);
	}
}

void abox_dbg_dump_mem(struct device *dev, struct abox_data *data,
		enum abox_dbg_dump_src src, const char *reason)
{
	struct abox_gic_data *gic_data = dev_get_drvdata(data->dev_gic);

	dev_dbg(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[src];

		p_dump->time = sched_clock();
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		memcpy_fromio(p_dump->sram.dump, data->sram_base,
				data->sram_size);
		p_dump->sram.magic = ABOX_DBG_DUMP_MAGIC_SRAM;
		memcpy(p_dump->dram.dump, data->dram_base, DRAM_FIRMWARE_SIZE);
		p_dump->dram.magic = ABOX_DBG_DUMP_MAGIC_DRAM;
		memcpy_fromio(p_dump->sfr.dump, data->sfr_base,
				sizeof(p_dump->sfr.dump));
		p_dump->sfr.magic = ABOX_DBG_DUMP_MAGIC_SFR;
		memcpy_fromio(p_dump->sfr_gic_gicd, gic_data->gicd_base,
				sizeof(p_dump->sfr_gic_gicd));
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[src];

		p_dump->time = sched_clock();
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		memcpy_fromio(p_dump->sram.dump, data->sram_base,
				data->sram_size);
		p_dump->sram.magic = ABOX_DBG_DUMP_MAGIC_SRAM;
		memcpy_fromio(p_dump->sfr.dump, data->sfr_base,
				sizeof(p_dump->sfr.dump));
		p_dump->sfr.magic = ABOX_DBG_DUMP_MAGIC_SFR;
		memcpy_fromio(p_dump->sfr_gic_gicd, gic_data->gicd_base,
				sizeof(p_dump->sfr_gic_gicd));
	}
}

void abox_dbg_dump_gpr_mem(struct device *dev, struct abox_data *data,
		enum abox_dbg_dump_src src, const char *reason)
{
	abox_dbg_dump_gpr(dev, data, src, reason);
	abox_dbg_dump_mem(dev, data, src, reason);
}

struct abox_dbg_dump_simple {
	struct abox_dbg_dump_sram sram;
	struct abox_dbg_dump_sfr sfr;
	u32 sfr_gic_gicd[SZ_4K / sizeof(u32)];
	unsigned int gpr[17];
	long long time;
	char reason[SZ_32];
};

static struct abox_dbg_dump_simple abox_dump_simple;

void abox_dbg_dump_simple(struct device *dev, struct abox_data *data,
		const char *reason)
{
	struct abox_gic_data *gic_data = dev_get_drvdata(data->dev_gic);
	int i;

	dev_info(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	abox_dump_simple.time = sched_clock();
	strncpy(abox_dump_simple.reason, reason,
			sizeof(abox_dump_simple.reason) - 1);
	for (i = 0; i <= 14; i++)
		abox_dump_simple.gpr[i] = readl(data->sfr_base + ABOX_CPU_R(i));
	abox_dump_simple.gpr[i++] = readl(data->sfr_base + ABOX_CPU_PC);
	abox_dump_simple.gpr[i++] = readl(data->sfr_base + ABOX_CPU_L2C_STATUS);
	memcpy_fromio(abox_dump_simple.sram.dump, data->sram_base,
			data->sram_size);
	abox_dump_simple.sram.magic = ABOX_DBG_DUMP_MAGIC_SRAM;
	memcpy_fromio(abox_dump_simple.sfr.dump, data->sfr_base,
			sizeof(abox_dump_simple.sfr.dump));
	abox_dump_simple.sfr.magic = ABOX_DBG_DUMP_MAGIC_SFR;
	memcpy_fromio(abox_dump_simple.sfr_gic_gicd, gic_data->gicd_base,
			sizeof(abox_dump_simple.sfr_gic_gicd));
}

static atomic_t abox_error_count = ATOMIC_INIT(0);

void abox_dbg_report_status(struct device *dev, bool ok)
{
	char env[32] = {0,};
	char *envp[2] = {env, NULL};

	dev_info(dev, "%s\n", __func__);

	if (ok)
		atomic_set(&abox_error_count, 0);
	else
		atomic_inc(&abox_error_count);

	snprintf(env, sizeof(env), "ERR_CNT=%d",
			atomic_read(&abox_error_count));
	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
}

int abox_dbg_get_error_count(struct device *dev)
{
	int count = atomic_read(&abox_error_count);

	dev_dbg(dev, "%s: %d\n", __func__, count);

	return count;
}

static ssize_t calliope_sram_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device *dev_abox = dev->parent;

	dev_dbg(dev, "%s(%lld, %zu)\n", __func__, off, size);

	if (pm_runtime_get_if_in_use(dev_abox) > 0) {
		memcpy_fromio(buf, battr->private + off, size);
		pm_runtime_put(dev_abox);
	} else {
		memset(buf, 0x0, size);
	}

	return size;
}

static ssize_t calliope_dram_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	struct device *dev = kobj_to_dev(kobj);

	dev_dbg(dev, "%s(%lld, %zu)\n", __func__, off, size);

	memcpy(buf, battr->private + off, size);
	return size;
}

static ssize_t calliope_priv_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	return calliope_dram_read(file, kobj, battr, buf, off, size);
}

/* size will be updated later */
static BIN_ATTR_RO(calliope_sram, 0);
static BIN_ATTR_RO(calliope_dram, DRAM_FIRMWARE_SIZE);
static BIN_ATTR_RO(calliope_priv, PRIVATE_SIZE);
static struct bin_attribute *calliope_bin_attrs[] = {
	&bin_attr_calliope_sram,
	&bin_attr_calliope_dram,
	&bin_attr_calliope_priv,
};

static ssize_t gpr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct abox_data *data = dev_get_drvdata(dev->parent);
	char version[4];
	char *pbuf = buf;
	int i;

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return -EFAULT;
	}

	memcpy(version, &data->calliope_version, sizeof(version));

	pbuf += sprintf(pbuf, "========================================\n");
	pbuf += sprintf(pbuf, "A-Box CPU register dump (%c%c%c%c)\n",
			version[3], version[2], version[1], version[0]);
	pbuf += sprintf(pbuf, "----------------------------------------\n");
	for (i = 0; i <= 14; i++) {
		pbuf += sprintf(pbuf, "CA7_R%02d        : %08x\n", i,
				readl(data->sfr_base + ABOX_CPU_R(i)));
	}
	pbuf += sprintf(pbuf, "CA7_PC         : %08x\n",
			readl(data->sfr_base + ABOX_CPU_PC));
	pbuf += sprintf(pbuf, "CA7_L2C_STATUS : %08x\n",
			readl(data->sfr_base + ABOX_CPU_L2C_STATUS));
	pbuf += sprintf(pbuf, "========================================\n");

	return pbuf - buf;
}

static DEVICE_ATTR_RO(gpr);

static int samsung_abox_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *abox_dev = dev->parent;
	struct abox_data *data = dev_get_drvdata(abox_dev);
	int i, ret;

	dev_dbg(dev, "%s\n", __func__);

	if (abox_rmem == NULL)
		return -ENOMEM;

	dev_info(dev, "%s(%pa) is mapped on %p with size of %pa\n",
			"dump buffer", &abox_rmem->base,
			phys_to_virt(abox_rmem->base), &abox_rmem->size);
	iommu_map(data->iommu_domain, IOVA_DUMP_BUFFER, abox_rmem->base,
			abox_rmem->size, 0);
	data->dump_base = phys_to_virt(abox_rmem->base);
	data->dump_base_phys = abox_rmem->base;
	ret = device_create_file(dev, &dev_attr_gpr);
	bin_attr_calliope_sram.size = data->sram_size;
	bin_attr_calliope_sram.private = data->sram_base;
	bin_attr_calliope_dram.private = data->dram_base;
	bin_attr_calliope_priv.private = data->priv_base;
	for (i = 0; i < ARRAY_SIZE(calliope_bin_attrs); i++) {
		struct bin_attribute *battr = calliope_bin_attrs[i];

		ret = device_create_bin_file(dev, battr);
		if (ret < 0)
			dev_warn(dev, "Failed to create file: %s\n",
					battr->attr.name);
	}

	return ret;
}

static int samsung_abox_debug_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id samsung_abox_debug_match[] = {
	{
		.compatible = "samsung,abox-debug",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_debug_match);

static struct platform_driver samsung_abox_debug_driver = {
	.probe  = samsung_abox_debug_probe,
	.remove = samsung_abox_debug_remove,
	.driver = {
		.name = "samsung-abox-debug",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_debug_match),
	},
};

module_platform_driver(samsung_abox_debug_driver);

MODULE_AUTHOR("Gyeongtaek Lee, <gt82.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC A-Box Debug Driver");
MODULE_ALIAS("platform:samsung-abox-debug");
MODULE_LICENSE("GPL");
