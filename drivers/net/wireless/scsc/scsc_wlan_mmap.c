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
#include "debug.h"

#define N_MINORS	1
#define SCSC_WLAN_MAX_SIZE	4*1024*1024

static struct class *scsc_wlan_mmap_class;
static struct cdev scsc_wlan_mmap_dev[N_MINORS];
static dev_t dev_num;

static DEFINE_MUTEX(scsc_wlan_lock);
static void *scsc_wlan_mmap_buf;
static size_t scsc_wlan_mmap_size;

static int scsc_wlan_mmap_open(struct inode *inode, struct file *filp)
{
	SLSI_INFO_NODEV("scsc_wlan_mmap_open\n");
	return 0;
}

static int scsc_wlan_mmap_release(struct inode *inode, struct file *filp)
{
	SLSI_INFO_NODEV("scsc_wlan_mmap_release\n");
	return 0;
}


static int scsc_wlan_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	/* Ignore the vma->vm_pgoff , we are forcing mmap to
	 * start on offset 0*/
	unsigned long offset = 0;
	unsigned long page, pos;

	mutex_lock(&scsc_wlan_lock);
	if (size > scsc_wlan_mmap_size)
		goto error;
	if (offset > scsc_wlan_mmap_size - size)
		goto error;

	if (scsc_wlan_mmap_buf == NULL) {
		SLSI_ERR_NODEV("scsc_wlan_mmap_buf is NULL\n");
		goto error;
	}

	pos = (unsigned long)scsc_wlan_mmap_buf + offset;

	SLSI_INFO_NODEV("scsc_wlan_mmap size:%lu offset %ld\n", size, offset);

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
			goto error;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	mutex_unlock(&scsc_wlan_lock);
	return 0;
error:
	mutex_unlock(&scsc_wlan_lock);
	return -EAGAIN;
}

int scsc_wlan_mmap_set_buffer(void *buf, size_t sz)
{
	mutex_lock(&scsc_wlan_lock);
	if (sz > SCSC_WLAN_MAX_SIZE) {
		SLSI_ERR_NODEV("Size %zu exceeds %d\n", sz, SCSC_WLAN_MAX_SIZE);
		scsc_wlan_mmap_buf = NULL;
		scsc_wlan_mmap_size = 0;
		mutex_unlock(&scsc_wlan_lock);
		return -EIO;
	}

	SLSI_INFO_NODEV("scsc_wlan_mmap_set_buffer size:%zu\n", sz);
	scsc_wlan_mmap_buf = buf;
	scsc_wlan_mmap_size = sz;
	mutex_unlock(&scsc_wlan_lock);

	return 0;
}

static const struct file_operations scsc_wlan_mmap_fops = {
	.owner          = THIS_MODULE,
	.open           = scsc_wlan_mmap_open,
	.mmap           = scsc_wlan_mmap,
	.release        = scsc_wlan_mmap_release,
};

int scsc_wlan_mmap_create(void)
{
	struct device *dev;
	int i;
	int ret;
	dev_t curr_dev;

	/* Request the kernel for N_MINOR devices */
	ret = alloc_chrdev_region(&dev_num, 0, N_MINORS, "scsc_wlan_mmap");
	if (ret) {
		SLSI_ERR_NODEV("alloc_chrdev_region failed");
		goto error;
	}

	/* Create a class : appears at /sys/class */
	scsc_wlan_mmap_class = class_create(THIS_MODULE, "scsc_wlan_mmap_class");
	if (IS_ERR(scsc_wlan_mmap_class)) {
		ret = PTR_ERR(scsc_wlan_mmap_class);
		goto error_class;
	}

	/* Initialize and create each of the device(cdev) */
	for (i = 0; i < N_MINORS; i++) {
		curr_dev = MKDEV(MAJOR(dev_num), i);
		/* Associate the cdev with a set of file_operations */
		cdev_init(&scsc_wlan_mmap_dev[i], &scsc_wlan_mmap_fops);
		ret = cdev_add(&scsc_wlan_mmap_dev[i], curr_dev, 1);
		if (ret) {
			SLSI_ERR_NODEV("cdev_add failed");
			continue;
		}

		scsc_wlan_mmap_dev[i].owner = THIS_MODULE;
		/* Build up the current device number. To be used further */
		dev = device_create(scsc_wlan_mmap_class, NULL, curr_dev, NULL, "scsc_wlan_mmap_%d", i);
		if (IS_ERR(dev)) {
			SLSI_ERR_NODEV("device_create failed");
			ret = PTR_ERR(dev);
			cdev_del(&scsc_wlan_mmap_dev[i]);
			continue;
		}
	}

	return 0;

error_class:
	unregister_chrdev_region(dev_num, N_MINORS);
error:
	return 0;
}

int scsc_wlan_mmap_destroy(void)
{
	int i;
	int major = MAJOR(dev_num);
	dev_t curr_dev;

	for (i = 0; i < N_MINORS; i++) {
		curr_dev = MKDEV(major, i);
		cdev_del(&scsc_wlan_mmap_dev[i]);
		device_destroy(scsc_wlan_mmap_class, dev_num);
	}
	class_destroy(scsc_wlan_mmap_class);
	unregister_chrdev_region(dev_num, N_MINORS);
	return 0;
}

