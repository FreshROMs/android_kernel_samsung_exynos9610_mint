/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <scsc/scsc_log_collector.h>
#include "scsc_log_collector_mmap.h"

#define DEVICE_NAME "scsc_log_collector"
#define N_MINORS	1

static struct class *scsc_log_collector_class;
static struct cdev scsc_log_collector_dev[N_MINORS];
static dev_t dev_num;

static int scsc_log_collector_mmap_open(struct inode *inode, struct file *filp)
{
	pr_info("scsc_log_collector_mmap_open\n");
	return 0;
}

static int scsc_log_collector_release(struct inode *inode, struct file *filp)
{
	pr_info("scsc_log_collector_release\n");
	return 0;
}

static int scsc_log_collector_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;
	unsigned char *buf;

	if (size > SCSC_LOG_COLLECT_MAX_SIZE)
		return -EINVAL;
	if (offset > SCSC_LOG_COLLECT_MAX_SIZE - size)
		return -EINVAL;

	buf = scsc_log_collector_get_buffer();
	if (!buf) {
		pr_err("No buffer mapped\n");
		return -ENOMEM;
	}

	pos = (unsigned long)buf + offset;

	pr_info("scsc_log_collector_mmap size:%lu offset %ld\n", size, offset);

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

static const struct file_operations scsc_log_collector_mmap_fops = {
	.owner          = THIS_MODULE,
	.open           = scsc_log_collector_mmap_open,
	.mmap           = scsc_log_collector_mmap,
	.release        = scsc_log_collector_release,
};

int scsc_log_collector_mmap_create(void)
{
	struct device *dev;
	int i;
	int ret;
	dev_t curr_dev;

	/* Request the kernel for N_MINOR devices */
	ret = alloc_chrdev_region(&dev_num, 0, N_MINORS, "scsc_log_collector");
	if (ret) {
		pr_err("alloc_chrdev_region failed");
		goto error;
	}

	/* Create a class : appears at /sys/class */
	scsc_log_collector_class = class_create(THIS_MODULE, "scsc_log_collector_class");
	if (IS_ERR(scsc_log_collector_class)) {
		ret = PTR_ERR(scsc_log_collector_class);
		goto error_class;
	}

	/* Initialize and create each of the device(cdev) */
	for (i = 0; i < N_MINORS; i++) {
		/* Associate the cdev with a set of file_operations */
		cdev_init(&scsc_log_collector_dev[i], &scsc_log_collector_mmap_fops);

		ret = cdev_add(&scsc_log_collector_dev[i], dev_num, 1);
		if (ret)
			pr_err("cdev_add failed");

		scsc_log_collector_dev[i].owner = THIS_MODULE;
		/* Build up the current device number. To be used further */
		dev = device_create(scsc_log_collector_class, NULL, dev_num, NULL, "scsc_log_collector_%d", i);
		if (IS_ERR(dev)) {
			pr_err("device_create failed");
			ret = PTR_ERR(dev);
			cdev_del(&scsc_log_collector_dev[i]);
			continue;
		}
		curr_dev = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);
	}

	return 0;

error_class:
	unregister_chrdev_region(dev_num, N_MINORS);
error:
	return 0;
}

int scsc_log_collector_mmap_destroy(void)
{
	int i;

	device_destroy(scsc_log_collector_class, dev_num);
	for (i = 0; i < N_MINORS; i++)
		cdev_del(&scsc_log_collector_dev[i]);
	class_destroy(scsc_log_collector_class);
	unregister_chrdev_region(dev_num, N_MINORS);
	return 0;
}
