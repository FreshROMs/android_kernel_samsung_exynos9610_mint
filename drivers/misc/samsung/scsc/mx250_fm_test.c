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
#include <linux/uaccess.h>

#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>
#include "scsc_mx_impl.h"

/* char device entry declarations */
static dev_t        mx250_fm_test_dev_t;
static struct class *mx250_fm_test_class;
static struct cdev  *mx250_fm_test_cdev;


static int mx250_fm_test_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mx250_fm_test_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}


static ssize_t mx250_fm_test_dev_write(struct file *file, const char *data, size_t len, loff_t *offset)
{
	unsigned long count;
	char          str[20]; /* One value and carry return */
	long          val = 0;
	struct wlbt_fm_params params;
	int r;

	count = copy_from_user(str, data, len);

	str[sizeof(str) - 1] = 0;
	if (len < sizeof(str))
		str[len - 1] = 0;

	r = kstrtol(str, 0, &val);
	if (r) {
		SCSC_TAG_ERR(FM_TEST, "parse error %d, l=%zd\n", r, len);
		goto error;
	}

	if (val == 1)
		mx250_fm_request();
	else if (val == 0)
		mx250_fm_release();
	else {
		/* All other values are frequency info */
		params.freq = (u32)val;

		SCSC_TAG_INFO(FM_TEST, "FM freq=%u\n", params.freq);

		mx250_fm_set_params(&params);
	}
error:
	return len;
}

static ssize_t mx250_fm_test_dev_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	return length;
}

static const struct file_operations mx250_fm_test_dev_fops = {
	.owner = THIS_MODULE,
	.open = mx250_fm_test_dev_open,
	.read = mx250_fm_test_dev_read,
	.write = mx250_fm_test_dev_write,
	.release = mx250_fm_test_dev_release,
};

/* FM service driver registration */
void mx250_fm_test_init(void)
{
	int ret;

	SCSC_TAG_INFO(FM_TEST, "Registering mx250 TEST\n");

	ret = alloc_chrdev_region(&mx250_fm_test_dev_t, 0, 1, "mx250_fm_test-cdev");
	if (ret < 0) {
		SCSC_TAG_ERR(FM_TEST, "failed to alloc chrdev region\n");
		goto fail_alloc_chrdev_region;
	}

	mx250_fm_test_cdev = cdev_alloc();
	if (!mx250_fm_test_cdev) {
		ret = -ENOMEM;
		SCSC_TAG_ERR(FM_TEST, "failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}

	cdev_init(mx250_fm_test_cdev, &mx250_fm_test_dev_fops);
	ret = cdev_add(mx250_fm_test_cdev, mx250_fm_test_dev_t, 1);
	if (ret < 0) {
		SCSC_TAG_ERR(FM_TEST, "failed to add cdev\n");
		goto fail_add_cdev;
	}

	mx250_fm_test_class = class_create(THIS_MODULE, "mx250_fm_test");
	if (!mx250_fm_test_class) {
		ret = -EEXIST;
		SCSC_TAG_ERR(FM_TEST, "failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(mx250_fm_test_class, NULL, mx250_fm_test_dev_t, NULL, "mx250_fm_test_%d",
			MINOR(mx250_fm_test_dev_t))) {
		ret = -EINVAL;
		SCSC_TAG_ERR(FM_TEST, "failed to create device\n");
		goto fail_create_device;
	}

	return;
fail_create_device:
	class_destroy(mx250_fm_test_class);
fail_create_class:
	cdev_del(mx250_fm_test_cdev);
fail_add_cdev:
fail_alloc_cdev:
	unregister_chrdev_region(mx250_fm_test_dev_t, 1);
fail_alloc_chrdev_region:
	return;
}

void mx250_fm_test_exit(void)
{
	device_destroy(mx250_fm_test_class, mx250_fm_test_dev_t);
	class_destroy(mx250_fm_test_class);
	cdev_del(mx250_fm_test_cdev);
	unregister_chrdev_region(mx250_fm_test_dev_t, 1);
}
