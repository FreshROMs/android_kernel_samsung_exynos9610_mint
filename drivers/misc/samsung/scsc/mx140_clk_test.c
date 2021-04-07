/****************************************************************************
 *
 *   Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ****************************************************************************/

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>
#include "scsc_mx_impl.h"

/* char device entry declarations */
static dev_t        mx140_clk_test_dev_t;
static struct class *mx140_clk_test_class;
static struct cdev  *mx140_clk_test_cdev;

/* Call back function registered with 20MHz clock framework */
static void client_cb(void *data, enum mx140_clk20mhz_status event)
{
	switch (event) {
	case MX140_CLK_STARTED:
		SCSC_TAG_INFO(CLK20_TEST, "Event MX140_CLK_STARTED received\n");
		break;
	case MX140_CLK_STOPPED:
		SCSC_TAG_INFO(CLK20_TEST, "Event MX140_CLK_STOPPED received\n");
		break;
	case MX140_CLK_NOT_STARTED:
		SCSC_TAG_INFO(CLK20_TEST, "Event MX140_CLK_NOT_STARTED received\n");
		break;
	case MX140_CLK_NOT_STOPPED:
		SCSC_TAG_INFO(CLK20_TEST, "Event MX140_CLK_NOT_STOPPED received\n");
		break;
	case MX140_CLK_ASYNC_FAIL:
		SCSC_TAG_INFO(CLK20_TEST, "Event MX140_CLK_ASYNC_FAIL received\n");
		break;
	default:
		break;
	}
}

static int mx140_clk_test_dev_open(struct inode *inode, struct file *file)
{
	mx140_clk20mhz_register(client_cb, NULL);
	return 0;
}

static int mx140_clk_test_dev_release(struct inode *inode, struct file *file)
{
	mx140_clk20mhz_unregister();
	return 0;
}


static ssize_t mx140_clk_test_dev_write(struct file *file, const char *data, size_t len, loff_t *offset)
{
	unsigned long count;
	char          str[2]; /* One value and carry return */
	long int      val = 0;

	if (len > 2) {
		SCSC_TAG_ERR(CLK20_TEST, "Incorrect value len %zd\n", len);
		goto error;
	}

	count = copy_from_user(str, data, len);

	str[1] = 0;

	if (kstrtol(str, 10, &val)) {
		SCSC_TAG_ERR(CLK20_TEST, "Invalid value\n");
		goto error;
	}

	if (val == 1)
		mx140_clk20mhz_request();
	else if (val == 0)
		mx140_clk20mhz_release();
	else
		SCSC_TAG_INFO(CLK20_TEST, "val %ld is not valid, 1 - on, 0 - off\n", val);
error:
	return len;
}

static ssize_t mx140_clk_test_dev_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	return length;
}

static const struct file_operations mx140_clk_test_dev_fops = {
	.owner = THIS_MODULE,
	.open = mx140_clk_test_dev_open,
	.read = mx140_clk_test_dev_read,
	.write = mx140_clk_test_dev_write,
	.release = mx140_clk_test_dev_release,
};

/* 20MHz service driver registration */
void mx140_clk_test_init(void)
{
	int ret;

	SCSC_TAG_INFO(CLK20_TEST, "Registering mx140 TEST\n");

	ret = alloc_chrdev_region(&mx140_clk_test_dev_t, 0, 1, "mx140_clk_test-cdev");
	if (ret < 0) {
		SCSC_TAG_ERR(CLK20_TEST, "failed to alloc chrdev region\n");
		goto fail_alloc_chrdev_region;
	}

	mx140_clk_test_cdev = cdev_alloc();
	if (!mx140_clk_test_cdev) {
		ret = -ENOMEM;
		SCSC_TAG_ERR(CLK20_TEST, "failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}

	cdev_init(mx140_clk_test_cdev, &mx140_clk_test_dev_fops);
	ret = cdev_add(mx140_clk_test_cdev, mx140_clk_test_dev_t, 1);
	if (ret < 0) {
		SCSC_TAG_ERR(CLK20_TEST, "failed to add cdev\n");
		goto fail_add_cdev;
	}

	mx140_clk_test_class = class_create(THIS_MODULE, "mx140_clk_test");
	if (!mx140_clk_test_class) {
		ret = -EEXIST;
		SCSC_TAG_ERR(CLK20_TEST, "failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(mx140_clk_test_class, NULL, mx140_clk_test_dev_t, NULL, "mx140_usb_clk_test_%d", MINOR(mx140_clk_test_dev_t))) {
		ret = -EINVAL;
		SCSC_TAG_ERR(CLK20_TEST, "failed to create device\n");
		goto fail_create_device;
	}

	return;
fail_create_device:
	class_destroy(mx140_clk_test_class);
fail_create_class:
	cdev_del(mx140_clk_test_cdev);
fail_add_cdev:
fail_alloc_cdev:
	unregister_chrdev_region(mx140_clk_test_dev_t, 1);
fail_alloc_chrdev_region:
	return;
}

void mx140_clk_test_exit(void)
{
	device_destroy(mx140_clk_test_class, mx140_clk_test_dev_t);
	class_destroy(mx140_clk_test_class);
	cdev_del(mx140_clk_test_cdev);
	unregister_chrdev_region(mx140_clk_test_dev_t, 1);
}
