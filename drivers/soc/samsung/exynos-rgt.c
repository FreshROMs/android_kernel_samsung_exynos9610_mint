/*
 * Exynos regulator support.
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include "../../regulator/internal.h"
#include <../drivers/soc/samsung/acpm/acpm_ipc.h>

#define EXYNOS_RGT_PREFIX	"EXYNOS-RGT: "

struct exynos_rgt_info {
	struct regulator *rgt;
	struct dentry *reg_dir;
	struct dentry *f_get;
	struct dentry *f_ena;
	struct dentry *f_volt;
	struct file_operations get_fops;
	struct file_operations ena_fops;
	struct file_operations volt_fops;
};

static struct dentry *exynos_rgt_root;
static int num_regulators = 0;

static const char *rdev_get_name(struct regulator_dev *rdev)
{
	if (rdev->desc->name)
		return rdev->desc->name;
	else if (rdev->constraints && rdev->constraints->name)
		return rdev->constraints->name;
	else
		return "";
}

static ssize_t exynos_rgt_get_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct regulator *cons, *rgt = file->private_data;
	const char *dev;
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	ssize_t len, ret = 0;

	if (!buf)
		return -ENOMEM;

	len = snprintf(buf + ret, PAGE_SIZE - ret, "[%s]\t open_count %d\n",
			rdev_get_name(rgt->rdev),
			rgt->rdev->open_count);
	if (len > 0)
		ret += len;

	len = snprintf(buf + ret, PAGE_SIZE - ret, "consumer list ->\n");
	if (len > 0)
		ret += len;

	if (list_empty(&rgt->rdev->consumer_list))
		goto skip;

	list_for_each_entry(cons, &rgt->rdev->consumer_list, list) {
		if (cons && cons->dev && dev_name(cons->dev))
			dev = dev_name(cons->dev);
		else
			dev = "unknown";

		len = snprintf(buf + ret, PAGE_SIZE - ret, "\t [%s]\n", dev);
		if (len > 0)
			ret += len;

		if (ret > PAGE_SIZE) {
			ret = PAGE_SIZE;
			break;
		}
	}
skip:
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static ssize_t exynos_rgt_ena_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct regulator *rgt = file->private_data;
	char buf[80];
	ssize_t ret;

	if (!rgt->rdev->desc->ops->is_enabled) {
		pr_err("There is no is_enabled callback on this regulator\n");
		return -ENODEV;
	}

	ret = snprintf(buf, sizeof(buf), "[%s]\t %s (always_on %d, use_count %d)\n",
			rdev_get_name(rgt->rdev),
			rgt->rdev->desc->ops->is_enabled(rgt->rdev) ? "enabled " : "disabled",
			rgt->rdev->constraints->always_on,
			rgt->rdev->use_count);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t exynos_rgt_ena_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct regulator *rgt = file->private_data;
	char buf[32];
	ssize_t len, ret;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	switch (buf[0]) {
	case '0':
		ret = regulator_disable(rgt);
		if (ret)
			return ret;
		break;
	case '1':
		ret = regulator_enable(rgt);
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	return len;
}

static ssize_t exynos_rgt_volt_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct regulator *cons, *rgt = file->private_data;
	struct regulation_constraints *constraints = rgt->rdev->constraints;
	const char *dev;
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	ssize_t len, ret = 0;

	if (!buf)
		return -ENOMEM;

	len = snprintf(buf + ret, PAGE_SIZE - ret, "[%s]\t curr %4d mV\t constraint min %4d mV, max %4d mV\n",
			rdev_get_name(rgt->rdev),
			regulator_get_voltage(rgt) / 1000,
			constraints->min_uV / 1000,
			constraints->max_uV / 1000);
	if (len > 0)
		ret += len;

	len = snprintf(buf + ret, PAGE_SIZE - ret, "consumer list ->\n");
	if (len > 0)
		ret += len;

	if (list_empty(&rgt->rdev->consumer_list))
		goto skip;

	list_for_each_entry(cons, &rgt->rdev->consumer_list, list) {
		if (cons && cons->dev && dev_name(cons->dev))
			dev = dev_name(cons->dev);
		else
			dev = "unknown";

		len = snprintf(buf + ret, PAGE_SIZE - ret,
				"\t [%s]\t min %4d mV, max %4d mV %s\n",
				dev,
				cons->min_uV / 1000,
				cons->max_uV / 1000,
				cons->min_uV ? "(requested)" : "");
		if (len > 0)
			ret += len;

		if (ret > PAGE_SIZE) {
			ret = PAGE_SIZE;
			break;
		}
	}
skip:
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static ssize_t exynos_rgt_volt_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct regulator *rgt = file->private_data;
	int min_mV, min_uV, max_uV = rgt->rdev->constraints->max_uV;
	char buf[32];
	ssize_t len, ret;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	ret = kstrtos32(buf, 10, &min_mV);
	if (ret)
		return ret;

	min_uV = min_mV * 1000;

	if (min_uV < rgt->rdev->constraints->min_uV || min_uV > max_uV)
		return -EINVAL;

	ret = regulator_set_voltage(rgt, min_uV, max_uV);
	if (ret)
		return ret;

	return len;
}

static const struct file_operations exynos_rgt_get_fops = {
	.open = simple_open,
	.read = exynos_rgt_get_read,
	.llseek = default_llseek,
};

static const struct file_operations exynos_rgt_ena_fops = {
	.open = simple_open,
	.read = exynos_rgt_ena_read,
	.write = exynos_rgt_ena_write,
	.llseek = default_llseek,
};

static const struct file_operations exynos_rgt_volt_fops = {
	.open = simple_open,
	.read = exynos_rgt_volt_read,
	.write = exynos_rgt_volt_write,
	.llseek = default_llseek,
};

static int exynos_rgt_probe(struct platform_device *pdev)
{
	int ret;
	struct exynos_rgt_info *rgt_info;
	struct device_node *regulators_np, *reg_np, *pmic_np;
	int rgt_idx = 0, ipc_ch;
	const char *rgt_name;
	struct regulator_ss_info *rgt_ss;
	struct regulator_desc rdesc;

	regulators_np = of_find_node_by_name(NULL, "regulators");
	if (!regulators_np) {
		pr_err("%s %s: could not find regulators sub-node\n", EXYNOS_RGT_PREFIX, __func__);
		ret = -EINVAL;
		goto err_find_regs;
	}

	while (regulators_np) {
		num_regulators += of_get_child_count(regulators_np);
		regulators_np = of_find_node_by_name(regulators_np, "regulators");
	}

	rgt_info = kzalloc(sizeof(struct exynos_rgt_info) * num_regulators, GFP_KERNEL);
	if (!rgt_info) {
		pr_err("%s %s: could not allocate mem for rgt_info\n", EXYNOS_RGT_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_rgt_info;
	}

	exynos_rgt_root = debugfs_create_dir("exynos-rgt", NULL);
	if (!exynos_rgt_root) {
		pr_err("%s %s: could not create debugfs root dir\n",
				EXYNOS_RGT_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_dbgfs_root;
	}

	regulators_np = of_find_node_by_name(NULL, "regulators");
	while (regulators_np) {
		for_each_child_of_node(regulators_np, reg_np) {
			rgt_name = of_get_property(reg_np, "regulator-name", NULL);
			if (!rgt_name)
				continue;

			rgt_info[rgt_idx].rgt = regulator_get(&pdev->dev, rgt_name);
			if (IS_ERR(rgt_info[rgt_idx].rgt)) {
				pr_err("%s %s: failed to getting regulator %s\n", EXYNOS_RGT_PREFIX, __func__, rgt_name);
				continue;
			}

			rgt_info[rgt_idx].get_fops = exynos_rgt_get_fops;
			rgt_info[rgt_idx].ena_fops = exynos_rgt_ena_fops;
			rgt_info[rgt_idx].volt_fops = exynos_rgt_volt_fops;
			rgt_info[rgt_idx].reg_dir =
				debugfs_create_dir(rgt_name, exynos_rgt_root);
			rgt_info[rgt_idx].f_get =
				debugfs_create_file("get", 0400, rgt_info[rgt_idx].reg_dir,
						rgt_info[rgt_idx].rgt, &rgt_info[rgt_idx].get_fops);
			rgt_info[rgt_idx].f_ena =
				debugfs_create_file("enable", 0600, rgt_info[rgt_idx].reg_dir,
						rgt_info[rgt_idx].rgt, &rgt_info[rgt_idx].ena_fops);
			rgt_info[rgt_idx].f_volt =
				debugfs_create_file("voltage", 0600, rgt_info[rgt_idx].reg_dir,
						rgt_info[rgt_idx].rgt, &rgt_info[rgt_idx].volt_fops);

			pmic_np = of_get_parent(regulators_np);
			ret = of_property_read_u32(pmic_np, "acpm-ipc-channel", &ipc_ch);
			if (!ret) {
				rgt_ss = get_regulator_ss(rgt_idx);
				if (rgt_ss != NULL) {
					rdesc = *(rgt_info[rgt_idx].rgt->rdev->desc);

					snprintf(rgt_ss->name, sizeof(rgt_ss->name), "%s.", rdesc.name);
					rgt_ss->min_uV = rdesc.min_uV;
					rgt_ss->uV_step = rdesc.uV_step;
					rgt_ss->linear_min_sel = rdesc.linear_min_sel;
					rgt_ss->vsel_reg = rdesc.vsel_reg;
				}
			}
			rgt_idx++;
		}
		regulators_np = of_find_node_by_name(regulators_np, "regulators");
	}

	platform_set_drvdata(pdev, rgt_info);

	return 0;

err_dbgfs_root:
	kfree(rgt_info);
err_rgt_info:
err_find_regs:
	return ret;
}

static int exynos_rgt_remove(struct platform_device *pdev)
{
	struct exynos_rgt_info *rgt_info = platform_get_drvdata(pdev);
	int i = 0;

	for (i = 0; i < num_regulators; i++) {
		debugfs_remove_recursive(rgt_info[i].f_volt);
		debugfs_remove_recursive(rgt_info[i].f_ena);
		debugfs_remove_recursive(rgt_info[i].f_get);
		debugfs_remove_recursive(rgt_info[i].reg_dir);
		regulator_put(rgt_info[i].rgt);
	}

	debugfs_remove_recursive(exynos_rgt_root);
	kfree(rgt_info);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id exynos_rgt_match[] = {
	{
		.compatible = "samsung,exynos-rgt",
	},
	{},
};

static struct platform_driver exynos_rgt_drv = {
	.probe		= exynos_rgt_probe,
	.remove		= exynos_rgt_remove,
	.driver		= {
		.name	= "exynos_rgt",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_rgt_match,
	},
};

static int __init exynos_rgt_init(void)
{
	return platform_driver_register(&exynos_rgt_drv);
}
late_initcall(exynos_rgt_init);
