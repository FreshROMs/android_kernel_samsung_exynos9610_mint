/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>

#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>

struct scsc_mx_test {
	/* scsc_service_client has to be the first */
	struct scsc_service_client test_service_client;
	struct scsc_service        *primary_service;
	struct scsc_service        *secondary_service;
	struct scsc_mx             *mx;
	bool                       started;
};

static struct scsc_mx_test *test;

/* First service to start */
static int                 service_id = SCSC_SERVICE_ID_NULL;
module_param(service_id, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(service_id, "ID of service to start, Default 0:NULL, 1:WLAN, 2:BT, 3:ANT, 5:ECHO");

/* Second service to start if != -1 */
static int service_id_2 = -1;
module_param(service_id_2, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(service_id_2, "ID of optional second service to start: Default -1:None, 0:NULL, 1:WLAN, 2:BT, 3:ANT, 5:ECHO");

#ifdef CONFIG_SCSC_MX_ALWAYS_ON
static int auto_start = 2;
#else
static int auto_start;
#endif
module_param(auto_start, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(auto_start, "Start service automatically: 0: disabled, 1: Enabled, 2: Deferred");

/* Delay after probe before starting mx140 when auto_start=2 */
#define SCSC_MX_BOOT_DELAY_MS 30000

static DEFINE_MUTEX(ss_lock);

/* char device entry declarations */
static dev_t        client_test_dev_t;
static struct class *client_test_class;
static struct cdev  *client_test_cdev;

static u8 test_failure_notification(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void) client;
	SCSC_TAG_DEBUG(MXMAN_TEST, "OK\n");
	return err->level;
}


static bool test_stop_on_failure(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void) client;
	(void) err;
	SCSC_TAG_DEBUG(MXMAN_TEST, "OK\n");
	return false;
}

static void test_failure_reset(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code)
{
	(void)client;
	(void)level;
	(void)scsc_syserr_code;
	SCSC_TAG_ERR(MXMAN_TEST, "OK\n");
}


static void stop_close_services(void)
{
	int r;

	mutex_lock(&ss_lock);

	if (!test->started) {
		pr_info("mx140: already stopped\n");
		goto done;
	}

	if (test->primary_service) {
		r = scsc_mx_service_stop(test->primary_service);
		if (r)
			SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_stop(primary_service) failed err: %d\n", r);
		else
			SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_stop(primary_service) OK\n");
		r = scsc_mx_service_close(test->primary_service);
		if (r)
			SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_close(%d) failed err: %d\n", service_id, r);
		else
			SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_close(%d) OK\n", service_id);
		test->primary_service = NULL;
	}

	if (test->secondary_service) {
		r = scsc_mx_service_stop(test->secondary_service);
		if (r)
			SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_stop(secondary_service) failed err: %d\n", r);
		else
			SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_stop(secondary_service) OK\n");
		r = scsc_mx_service_close(test->secondary_service);
		if (r)
			SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_close(%d) failed err: %d\n", service_id_2, r);
		else
			SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_close(%d) OK\n", service_id_2);
		test->secondary_service = NULL;
	}
	test->started = false;
done:
	mutex_unlock(&ss_lock);
}

static bool open_start_services(struct scsc_mx *mx)
{
	struct scsc_service *primary_service;
	struct scsc_service *secondary_service;
	int                 r;
	bool		    ok;

	mutex_lock(&ss_lock);

	if (test->started) {
		pr_info("mx140: already started\n");
		ok = true;
		goto done;
	}

	primary_service = scsc_mx_service_open(mx, service_id, &test->test_service_client, &r);
	if (!primary_service) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_open for primary_service failed %d\n", r);
		ok = false;
		goto done;
	}

	r = scsc_mx_service_start(primary_service, 0);
	if (r) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_start for primary_service failed\n");
		r = scsc_mx_service_close(primary_service);
		if (r)
			SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_close for primary_service %d failed\n", r);
		ok = false;
		goto done;
	}

	test->primary_service = primary_service;

	if (service_id_2 != -1) {
		secondary_service = scsc_mx_service_open(mx, service_id_2, &test->test_service_client, &r);
		if (!secondary_service) {
			SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_open for secondary_service failed %d\n", r);
			r = scsc_mx_service_stop(test->primary_service);
			if (r)
				SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_stop(%d) failed err: %d\n", service_id, r);
			else
				SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_stop(%d) OK\n", service_id);
			r = scsc_mx_service_close(test->primary_service);
			if (r)
				SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_close(%d) failed err: %d\n", service_id, r);
			else
				SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_close(%d) OK\n", service_id);
			ok = false;
			goto done;
		}
		r = scsc_mx_service_start(secondary_service, 0);
		if (r) {
			SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_start for secondary_service failed\n");
			r = scsc_mx_service_close(secondary_service);
			if (r)
				SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_close(%d) failed err: %d\n", service_id, r);
			else
				SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_close(%d) OK\n", service_id);

			r = scsc_mx_service_stop(test->primary_service);
			if (r)
				SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_stop(%d) failed err: %d\n", service_id, r);
			else
				SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_stop(%d) OK\n", service_id);
			r = scsc_mx_service_close(test->primary_service);
			if (r)
				SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_service_close(%d) failed err: %d\n", service_id, r);
			else
				SCSC_TAG_DEBUG(MXMAN_TEST, "scsc_mx_service_close(%d) OK\n", service_id);
			ok = false;
			goto done;
		}
		test->secondary_service = secondary_service;
	}
	test->started = true;
	ok = true;
done:
	mutex_unlock(&ss_lock);
	return ok;
}

static void delay_start_func(struct work_struct *work)
{
	(void)work;

	pr_info("mx140: Start wlbt null service\n");

	if (!test->mx)
		return;

	if (!open_start_services(test->mx))
		pr_err("mx140: Error starting delayed service\n");
}

static DECLARE_DELAYED_WORK(delay_start, delay_start_func);

/* Start the null service after a delay */
static void delay_open_start_services(void)
{
	schedule_delayed_work(&delay_start, msecs_to_jiffies(SCSC_MX_BOOT_DELAY_MS));
}

/* Start service(s) and leave running until module unload */
void client_module_probe(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{
	/* Avoid unused error */
	(void)module_client;

	SCSC_TAG_ERR(MXMAN_TEST, "mx140:\n");

	test = kzalloc(sizeof(*test), GFP_KERNEL);
	if (!test)
		return;

	test->test_service_client.failure_notification = test_failure_notification;
	test->test_service_client.stop_on_failure_v2   = test_stop_on_failure;
	test->test_service_client.failure_reset_v2     = test_failure_reset;
	test->mx = mx;

	switch (auto_start) {
	case 1:
		if (!open_start_services(test->mx)) {
			SCSC_TAG_ERR(MXMAN_TEST, "Error starting service/s\n");
			kfree(test);
			return;
		}
		break;
	case 2:
		pr_info("mx140: delayed auto-start\n");
		delay_open_start_services();
		break;
	default:
		break;
	}

	SCSC_TAG_ERR(MXMAN_TEST, "OK\n");
}

void client_module_remove(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{
	/* Avoid unused error */
	(void)module_client;

	pr_info("mx140: %s\n", __func__);

	if (!test)
		return;
	if (test->mx != mx) {
		SCSC_TAG_ERR(MXMAN_TEST, "test->mx != mx\n");
		return;
	}

	/* Cancel any delayed start attempt */
	cancel_delayed_work_sync(&delay_start);

	stop_close_services();

	/* de-allocate test structure */
	kfree(test);
	SCSC_TAG_DEBUG(MXMAN_TEST, "OK\n");
}


/* Test client driver registration */
static struct scsc_mx_module_client client_test_driver = {
	.name = "MX client test driver",
	.probe = client_module_probe,
	.remove = client_module_remove,
};


static int client_test_dev_open(struct inode *inode, struct file *file)
{
	SCSC_TAG_ERR(MXMAN_TEST, "open client test\n");
	return 0;
}

static ssize_t client_test_dev_write(struct file *file, const char *data, size_t len, loff_t *offset)
{
	unsigned long count;
	char          str[2]; /* One value and carry return */
	long int      val = 0;
	bool          ok = true;

	if (len > 2) {
		SCSC_TAG_ERR(MXMAN_TEST, "Incorrect value len %zd\n", len);
		goto error;
	}

	count = copy_from_user(str, data, len);

	str[1] = 0;

	if (kstrtol(str, 10, &val)) {
		SCSC_TAG_ERR(MXMAN_TEST, "Invalid value\n");
		goto error;
	}

	if (test) {
		if (val) {
			SCSC_TAG_INFO(MXMAN_TEST, "Start services\n");
			ok = open_start_services(test->mx);
		} else {
			SCSC_TAG_INFO(MXMAN_TEST, "Stop services\n");
			stop_close_services();
		}
	} else {
		SCSC_TAG_ERR(MXMAN_TEST, "Test not created\n");
		goto error;
	}
error:
	SCSC_TAG_ERR(MXMAN_TEST, "%s\n", ok ? "OK" : "FAIL");
	return ok ? len : -EIO;
}

static ssize_t client_test_dev_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	return length;
}

static int client_test_dev_release(struct inode *inode, struct file *file)
{
	SCSC_TAG_DEBUG(MXMAN_TEST, "close client test\n");
	return 0;
}

static const struct file_operations client_test_dev_fops = {
	.owner = THIS_MODULE,
	.open = client_test_dev_open,
	.read = client_test_dev_read,
	.write = client_test_dev_write,
	.release = client_test_dev_release,
};

static int __init scsc_client_test_module_init(void)
{
	int r;

	SCSC_TAG_DEBUG(MXMAN_TEST, "mx140:\n");

	r = scsc_mx_module_register_client_module(&client_test_driver);
	if (r) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_module_register_client_module failed: r=%d\n", r);
		return r;
	}

	r = alloc_chrdev_region(&client_test_dev_t, 0, 1, "wlbt-null-service");
	if (r < 0) {
		SCSC_TAG_ERR(MXMAN_TEST, "failed to alloc chrdev region\n");
		goto fail_alloc_chrdev_region;
	}

	client_test_cdev = cdev_alloc();
	if (!client_test_cdev) {
		r = -ENOMEM;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}

	cdev_init(client_test_cdev, &client_test_dev_fops);
	r = cdev_add(client_test_cdev, client_test_dev_t, 1);
	if (r < 0) {
		SCSC_TAG_ERR(MXMAN_TEST, "failed to add cdev\n");
		goto fail_add_cdev;
	}

	client_test_class = class_create(THIS_MODULE, "sample");
	if (!client_test_class) {
		r = -EEXIST;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(client_test_class, NULL, client_test_dev_t, NULL, "mx_client_test_%d", MINOR(client_test_dev_t))) {
		r = -EINVAL;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to create device\n");
		goto fail_create_device;
	}

	return 0;
fail_create_device:
	class_destroy(client_test_class);
fail_create_class:
	cdev_del(client_test_cdev);
fail_add_cdev:
fail_alloc_cdev:
	unregister_chrdev_region(client_test_dev_t, 1);
fail_alloc_chrdev_region:
	return r;
}

static void __exit scsc_client_test_module_exit(void)
{
	SCSC_TAG_DEBUG(MXMAN_TEST, "mx140:\n");
	scsc_mx_module_unregister_client_module(&client_test_driver);
	SCSC_TAG_DEBUG(MXMAN_TEST, "exit\n");

	device_destroy(client_test_class, client_test_dev_t);
	class_destroy(client_test_class);
	cdev_del(client_test_cdev);
	unregister_chrdev_region(client_test_dev_t, 1);
}

late_initcall(scsc_client_test_module_init);
module_exit(scsc_client_test_module_exit);

MODULE_DESCRIPTION("mx140 Client Test Driver");
MODULE_AUTHOR("SCSC");
MODULE_LICENSE("GPL");
