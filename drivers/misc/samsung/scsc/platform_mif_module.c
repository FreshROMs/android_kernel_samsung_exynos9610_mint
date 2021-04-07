/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <scsc/scsc_logring.h>
#include "platform_mif_module.h"
#include "platform_mif.h"

/* Implements */
#include "scsc_mif_abs.h"

/* Variables */
struct mif_abs_node {
	struct list_head    list;
	struct scsc_mif_abs *mif_abs;
};

struct mif_driver_node {
	struct list_head           list;
	struct scsc_mif_abs_driver *driver;
};

struct mif_mmap_node {
	struct list_head            list;
	struct scsc_mif_mmap_driver *driver;
};

static struct platform_mif_module {
	struct list_head mif_abs_list;
	struct list_head mif_driver_list;
	struct list_head mif_mmap_list;
} mif_module = {
	.mif_abs_list = LIST_HEAD_INIT(mif_module.mif_abs_list),
	.mif_driver_list = LIST_HEAD_INIT(mif_module.mif_driver_list),
	.mif_mmap_list = LIST_HEAD_INIT(mif_module.mif_mmap_list),
};

/* Private Functions */

static void platform_mif_module_probe_registered_clients(struct scsc_mif_abs *mif_abs)
{
	struct mif_driver_node *mif_driver_node, *next;
	bool                   driver_registered = false;

	/* Traverse Linked List for each mif_driver node */
	list_for_each_entry_safe(mif_driver_node, next, &mif_module.mif_driver_list, list) {
		mif_driver_node->driver->probe(mif_driver_node->driver, mif_abs);
		driver_registered = true;
	}
}

static int platform_mif_module_probe(struct platform_device *pdev)
{
	struct mif_abs_node *mif_node;
	struct scsc_mif_abs *mif_abs;

	/* TODO: ADD EARLY BOARD INITIALIZATIONS IF REQUIRED */
	/* platform_mif_init(); */

	mif_node = kzalloc(sizeof(*mif_node), GFP_KERNEL);
	if (!mif_node)
		return -ENODEV;

	mif_abs = platform_mif_create(pdev);
	if (!mif_abs) {
		SCSC_TAG_ERR(PLAT_MIF, "Error creating platform interface\n");
		kfree(mif_node);
		return -ENODEV;
	}
	/* Add node */
	mif_node->mif_abs = mif_abs;
	list_add_tail(&mif_node->list, &mif_module.mif_abs_list);
	platform_mif_module_probe_registered_clients(mif_abs);

	return 0;
}

static int platform_mif_module_remove(struct platform_device *pdev)
{
	struct mif_abs_node *mif_node, *next;
	bool                match = false;

	/* Remove node */
	list_for_each_entry_safe(mif_node, next, &mif_module.mif_abs_list, list) {
		if (platform_mif_get_platform_dev(mif_node->mif_abs) == pdev) {
			match = true;
			platform_mif_destroy_platform(pdev, mif_node->mif_abs);
			list_del(&mif_node->list);
			kfree(mif_node);
		}
	}
	if (match == false)
		SCSC_TAG_ERR(PLAT_MIF, "No match for given scsc_mif_abs\n");

	return 0;
}

static int platform_mif_module_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mif_abs_node *mif_node, *next;
	int r;

	SCSC_TAG_INFO(PLAT_MIF, "\n");

	/* Traverse mif_abs list for this platform_device to suspend */
	list_for_each_entry_safe(mif_node, next, &mif_module.mif_abs_list, list) {
		if (platform_mif_get_platform_dev(mif_node->mif_abs) == pdev) {
			/* Signal suspend, client can refuse */
			r = platform_mif_suspend(mif_node->mif_abs);
			if (r) {
				SCSC_TAG_INFO(PLAT_MIF, "%d\n", r);
				return r;
			}
		}
	}
	return 0;
}

static int platform_mif_module_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mif_abs_node *mif_node, *next;

	SCSC_TAG_INFO(PLAT_MIF, "\n");

	/* Traverse mif_abs list for this platform_device to resume */
	list_for_each_entry_safe(mif_node, next, &mif_module.mif_abs_list, list) {
		if (platform_mif_get_platform_dev(mif_node->mif_abs) == pdev) {
			/* Signal resume */
			platform_mif_resume(mif_node->mif_abs);
		}
	}
	return 0;
}

static const struct dev_pm_ops platform_mif_pm_ops = {
	.suspend	= platform_mif_module_suspend,
	.resume		= platform_mif_module_resume,
};

static const struct of_device_id scsc_wifibt[] = {
	{ .compatible = "samsung,scsc_wifibt" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, scsc_wifibt);

static struct platform_driver    platform_mif_driver = {
	.probe  = platform_mif_module_probe,
	.remove = platform_mif_module_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &platform_mif_pm_ops,
		.of_match_table = of_match_ptr(scsc_wifibt),
	},
};

/* Choose when the driver should be probed */
#if 1
module_platform_driver(platform_mif_driver);
#else
static int platform_mif_init(void)
{
	SCSC_TAG_INFO(PLAT_MIF, "register platform driver\n");
	return platform_driver_register(&platform_mif_driver);
}
core_initcall(platform_mif_init);
#endif

/* Public Functions */
void scsc_mif_abs_register(struct scsc_mif_abs_driver *driver)
{
	struct mif_driver_node *mif_driver_node;
	struct mif_abs_node    *mif_node;

	/* Add node in driver linked list */
	mif_driver_node = kzalloc(sizeof(*mif_driver_node), GFP_KERNEL);
	if (!mif_driver_node)
		return;

	mif_driver_node->driver = driver;
	list_add_tail(&mif_driver_node->list, &mif_module.mif_driver_list);

	/* Traverse Linked List for each mif_abs node */
	list_for_each_entry(mif_node, &mif_module.mif_abs_list, list) {
		driver->probe(driver, mif_node->mif_abs);
	}
}
EXPORT_SYMBOL(scsc_mif_abs_register);

void scsc_mif_abs_unregister(struct scsc_mif_abs_driver *driver)
{
	struct mif_driver_node *mif_driver_node, *next;

	/* Traverse Linked List for each mif_driver node */
	list_for_each_entry_safe(mif_driver_node, next, &mif_module.mif_driver_list, list) {
		if (mif_driver_node->driver == driver) {
			list_del(&mif_driver_node->list);
			kfree(mif_driver_node);
		}
	}
}
EXPORT_SYMBOL(scsc_mif_abs_unregister);

/* Register a mmap - debug driver - for this specific transport*/
void scsc_mif_mmap_register(struct scsc_mif_mmap_driver *mmap_driver)
{
	struct mif_mmap_node *mif_mmap_node;
	struct mif_abs_node  *mif_node;

	/* Add node in driver linked list */
	mif_mmap_node = kzalloc(sizeof(*mif_mmap_node), GFP_KERNEL);
	if (!mif_mmap_node)
		return;

	mif_mmap_node->driver = mmap_driver;
	list_add_tail(&mif_mmap_node->list, &mif_module.mif_mmap_list);

	/* Traverse Linked List for each mif_abs node */
	list_for_each_entry(mif_node, &mif_module.mif_abs_list, list) {
		mmap_driver->probe(mmap_driver, mif_node->mif_abs);
	}
}
EXPORT_SYMBOL(scsc_mif_mmap_register);

/* Unregister a mmap - debug driver - for this specific transport*/
void scsc_mif_mmap_unregister(struct scsc_mif_mmap_driver *mmap_driver)
{
	struct mif_mmap_node *mif_mmap_node, *next;

	/* Traverse Linked List for each mif_driver node */
	list_for_each_entry_safe(mif_mmap_node, next, &mif_module.mif_mmap_list, list) {
		if (mif_mmap_node->driver == mmap_driver) {
			list_del(&mif_mmap_node->list);
			kfree(mif_mmap_node);
		}
	}
}
EXPORT_SYMBOL(scsc_mif_mmap_unregister);

MODULE_DESCRIPTION("SCSC Platform device Maxwell MIF abstraction");
MODULE_AUTHOR("SCSC");
MODULE_LICENSE("GPL");
