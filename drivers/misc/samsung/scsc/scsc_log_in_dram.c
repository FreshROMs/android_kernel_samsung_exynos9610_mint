/****************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include "scsc_mif_abs.h"
#include <scsc/scsc_mx.h>
#include "scsc_log_in_dram.h"

#define DEVICE_NAME "scsc_log_in_dram"
#define N_MINORS 1

struct class *scsc_log_in_dram_class;
struct cdev scsc_log_in_dram_dev[N_MINORS];
dev_t ram_dev_num;

#define SCSC_LOG_MAGIC_STRING "scsc_phy"
#define SCSC_LOG_MAGIC_STRING_SZ 8

static char *scsc_log_in_dram_ptr;

struct scsc_log_in_dram_magic {
	char magic[SCSC_LOG_MAGIC_STRING_SZ];
	phys_addr_t phy_add[MIFRAMMAN_LOG_DRAM_SZ / PAGE_SIZE];
} scsc_log_in_dram_status;

static int scsc_log_in_dram_mmap_open(struct inode *inode, struct file *filp)
{
	pr_info("wlbt: in_dram. scsc_log_in_dram_mmap_open\n");

	return 0;
}

static int scsc_log_in_dram_release(struct inode *inode, struct file *filp)
{
	pr_info("wlbt: in_dram. scsc_log_in_dram_release\n");

	return 0;
}

static int scsc_log_in_dram_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;

	pr_info("wlbt: in_dram. scsc_log_in_dram_mmap size:%lu offset %ld\n",
		size, offset);

	if (size > MIFRAMMAN_LOG_DRAM_SZ)
		return -EINVAL;
	if (offset > MIFRAMMAN_LOG_DRAM_SZ - size)
		return -EINVAL;

	if (!scsc_log_in_dram_ptr) {
		pr_err("wlbt: in_dram. Buffer not preallocated");
		return -EINVAL;
	}

	pos = (unsigned long)scsc_log_in_dram_ptr + offset;

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return 0;
}

static const struct file_operations scsc_log_in_dram_mmap_fops = {
	.owner = THIS_MODULE,
	.open = scsc_log_in_dram_mmap_open,
	.mmap = scsc_log_in_dram_mmap,
	.release = scsc_log_in_dram_release,
};

int scsc_log_in_dram_mmap_create(void)
{
	struct device *dev;
	int i;
	int ret;
	dev_t curr_dev;
	void *virtual_address;

	scsc_log_in_dram_ptr = vzalloc(MIFRAMMAN_LOG_DRAM_SZ);
	if (IS_ERR_OR_NULL(scsc_log_in_dram_ptr)) {
		pr_err("wlbt: in_dram. open allocating scsc_log_in_dram_ptr = %ld\n",
		       PTR_ERR(scsc_log_in_dram_ptr));
		scsc_log_in_dram_ptr = NULL;
		return -ENOMEM;
	}
	/* Request the kernel for N_MINOR devices */
	ret = alloc_chrdev_region(&ram_dev_num, 0, N_MINORS,
				  "scsc_log_in_dram");
	if (ret) {
		pr_err("wlbt: in_dram. alloc_chrdev_region failed");
		goto error;
	}

	/* Create a class : appears at /sys/class */
	scsc_log_in_dram_class =
		class_create(THIS_MODULE, "scsc_log_in_dram_class");
	if (IS_ERR(scsc_log_in_dram_class)) {
		ret = PTR_ERR(scsc_log_in_dram_class);
		goto error_class;
	}

	/* Initialize and create each of the device(cdev) */
	for (i = 0; i < N_MINORS; i++) {
		/* Associate the cdev with a set of file_operations */
		cdev_init(&scsc_log_in_dram_dev[i],
			  &scsc_log_in_dram_mmap_fops);

		ret = cdev_add(&scsc_log_in_dram_dev[i], ram_dev_num, 1);
		if (ret)
			pr_err("wlbt: in_dram. cdev_add failed");

		scsc_log_in_dram_dev[i].owner = THIS_MODULE;
		/* Build up the current device number. To be used further */
		dev = device_create(scsc_log_in_dram_class, NULL, ram_dev_num,
				    NULL, "scsc_log_in_dram_%d", i);
		if (IS_ERR(dev)) {
			pr_err("wlbt: in_dram. device_create failed");
			ret = PTR_ERR(dev);
			cdev_del(&scsc_log_in_dram_dev[i]);
			continue;
		}
		curr_dev = MKDEV(MAJOR(ram_dev_num), MINOR(ram_dev_num) + i);
	}

	memcpy(&scsc_log_in_dram_status.magic, SCSC_LOG_MAGIC_STRING,
	       SCSC_LOG_MAGIC_STRING_SZ);
	for (virtual_address = scsc_log_in_dram_ptr, i = 0;
	     virtual_address <
	     (void *)scsc_log_in_dram_ptr + MIFRAMMAN_LOG_DRAM_SZ;
	     virtual_address += PAGE_SIZE, i++) {
		scsc_log_in_dram_status.phy_add[i] =
			PFN_PHYS(vmalloc_to_pfn(virtual_address));
	}
	pr_info("wlbt: in_dram. Log buffer physical address: %lx first entry: %lx",
		virt_to_phys(&scsc_log_in_dram_status),
		PFN_PHYS(vmalloc_to_pfn(scsc_log_in_dram_ptr)));
	return 0;

error_class:
	unregister_chrdev_region(ram_dev_num, N_MINORS);
error:
	return 0;
}

int scsc_log_in_dram_mmap_destroy(void)
{
	int i;

	pr_info("wlbt: in_dram. Free scsc_log_in_dram_ptr");

	vfree(scsc_log_in_dram_ptr);
	scsc_log_in_dram_ptr = NULL;

	device_destroy(scsc_log_in_dram_class, ram_dev_num);
	for (i = 0; i < N_MINORS; i++)
		cdev_del(&scsc_log_in_dram_dev[i]);
	class_destroy(scsc_log_in_dram_class);
	unregister_chrdev_region(ram_dev_num, N_MINORS);
	return 0;
}
