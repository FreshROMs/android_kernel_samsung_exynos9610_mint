#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "pcie_mif_module.h"
#include "pcie_mif.h"

/* Implements */
#include "scsc_mif_abs.h"

struct mif_abs_node {
	struct list_head    list;
	struct scsc_mif_abs *mif_abs;
};

struct mif_driver_node {
	struct list_head           list;
	struct scsc_mif_abs_driver *driver; /* list of drivers (in practice just the core_module) */
};

struct mif_mmap_node {
	struct list_head            list;
	struct scsc_mif_mmap_driver *driver; /* list of drivers (in practive just the core_module) */
};

static struct pcie_mif_module {
	struct list_head mif_abs_list;
	struct list_head mif_driver_list;
	struct list_head mif_mmap_list;
} mif_module = {
	.mif_abs_list = LIST_HEAD_INIT(mif_module.mif_abs_list),
	.mif_driver_list = LIST_HEAD_INIT(mif_module.mif_driver_list),
	.mif_mmap_list = LIST_HEAD_INIT(mif_module.mif_mmap_list),
};


static const struct pci_device_id pcie_mif_module_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_SAMSUNG_SCSC) },
	{ /*End: all zeroes */ }
};

MODULE_DEVICE_TABLE(pci, pcie_mif_module_tbl);

static void pcie_mif_module_probe_registered_clients(struct scsc_mif_abs *mif_abs)
{
	struct mif_driver_node *mif_driver_node, *next;
	struct device          *dev;
	bool                   driver_registered = false;

	/* Traverse Linked List for each mif_driver node */
	list_for_each_entry_safe(mif_driver_node, next, &mif_module.mif_driver_list, list) {
		SCSC_TAG_INFO(PCIE_MIF, "node %p\n", mif_driver_node);

		dev = pcie_mif_get_dev(mif_abs);
		mif_driver_node->driver->probe(mif_driver_node->driver, mif_abs);
		driver_registered = true;
	}
	if (driver_registered == false)
		SCSC_TAG_INFO(PCIE_MIF, "No mif drivers registered\n");
}

static int pcie_mif_module_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct mif_abs_node *mif_node;
	struct scsc_mif_abs *mif_abs;

	mif_node = kzalloc(sizeof(*mif_node), GFP_KERNEL);
	if (!mif_node)
		return -ENODEV;

	mif_abs = pcie_mif_create(pdev, id);
	if (!mif_abs) {
		SCSC_TAG_INFO(PCIE_MIF, "Error creating PCIe interface\n");
		kfree(mif_node);
		return -ENODEV;
	}
	/* Add node */
	mif_node->mif_abs = mif_abs;
	SCSC_TAG_INFO(PCIE_MIF, "mif_node A %p\n", mif_node);
	list_add_tail(&mif_node->list, &mif_module.mif_abs_list);

	pcie_mif_module_probe_registered_clients(mif_abs);

	return 0;
}

static void pcie_mif_module_remove(struct pci_dev *pdev)
{
	struct mif_abs_node *mif_node, *next;
	bool                match = false;

	/* Remove node */
	list_for_each_entry_safe(mif_node, next, &mif_module.mif_abs_list, list) {
		if (pcie_mif_get_pci_dev(mif_node->mif_abs) == pdev) {
			match = true;
			SCSC_TAG_INFO(PCIE_MIF, "Match, destroy pcie_mif\n");
			pcie_mif_destroy_pcie(pdev, mif_node->mif_abs);
			list_del(&mif_node->list);
			kfree(mif_node);
		}
	}
	if (match == false)
		SCSC_TAG_INFO(PCIE_MIF, "FATAL, no match for given scsc_mif_abs\n");
}

static struct pci_driver scsc_pcie = {
	.name           = DRV_NAME,
	.id_table       = pcie_mif_module_tbl,
	.probe          = pcie_mif_module_probe,
	.remove         = pcie_mif_module_remove,
};

void scsc_mif_abs_register(struct scsc_mif_abs_driver *driver)
{
	struct mif_driver_node *mif_driver_node;
	struct mif_abs_node    *mif_node;
	struct device          *dev;

	/* Add node in driver linked list */
	mif_driver_node = kzalloc(sizeof(*mif_driver_node), GFP_KERNEL);
	if (!mif_driver_node)
		return;

	mif_driver_node->driver = driver;
	list_add_tail(&mif_driver_node->list, &mif_module.mif_driver_list);

	/* Traverse Linked List for each mif_abs node */
	list_for_each_entry(mif_node, &mif_module.mif_abs_list, list) {
		dev = pcie_mif_get_dev(mif_node->mif_abs);
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

	/* Traverse Linked List for each mif_mmap_driver node */
	list_for_each_entry_safe(mif_mmap_node, next, &mif_module.mif_mmap_list, list) {
		if (mif_mmap_node->driver == mmap_driver) {
			list_del(&mif_mmap_node->list);
			kfree(mif_mmap_node);
		}
	}
}
EXPORT_SYMBOL(scsc_mif_mmap_unregister);

module_pci_driver(scsc_pcie);

MODULE_DESCRIPTION("SLSI PCIe mx140 MIF abstraction");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL");
