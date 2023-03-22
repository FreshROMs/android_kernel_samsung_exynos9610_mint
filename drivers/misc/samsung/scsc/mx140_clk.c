/****************************************************************************
 *
 *   Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/wakelock.h>

#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>
#include "scsc_mx_impl.h"

#ifdef CONFIG_SCSC_CLK20MHZ_TEST
#include "mx140_clk_test.h"
#endif

/* Note: define MX140_CLK_VERBOSE_CALLBACKS to get more callbacks when events occur.
 * without this, the only callbacks are failure/success from request()
 */
static int auto_start;
module_param(auto_start, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(auto_start, "Start service automatically: Default 0: disabled, 1: Enabled");

static DEFINE_MUTEX(clk_lock);
static DEFINE_MUTEX(clk_work_lock);

struct workqueue_struct *mx140_clk20mhz_wq;
struct work_struct      mx140_clk20mhz_work;

static int recovery;
static int recovery_pending_stop_close;
#define MX140_SERVICE_RECOVERY_TIMEOUT 20000

/* Static Singleton */
static struct mx140_clk20mhz {
	/* scsc_service_client has to be the first */
	struct scsc_service_client mx140_clk20mhz_service_client;
	struct scsc_service        *mx140_clk20mhz_service;
	struct scsc_mx             *mx;

	atomic_t                   clk_request;
	atomic_t                   maxwell_is_present;
	atomic_t                   mx140_clk20mhz_service_started;
	atomic_t                   request_pending;
	atomic_t                   mx140_clk20mhz_service_failed;

	void                       *data;
	void (*mx140_clk20mhz_client_cb)(void *data, enum mx140_clk20mhz_status event);

	struct proc_dir_entry	   *procfs_ctrl_dir;
	u32			   procfs_ctrl_dir_num;

	struct wake_lock	   clk_wake_lock;
	struct completion          recovery_probe_completion;

} clk20mhz;

static void mx140_clk20mhz_wq_stop(void);
static int mx140_clk20mhz_stop_service(struct scsc_mx *mx);

#ifndef AID_MXPROC
#define AID_MXPROC 0
#endif

static void mx140_clk20mhz_restart(void);

#define MX_CLK20_DIRLEN 128
static const char *procdir_ctrl = "driver/mx140_clk";
static u32 proc_count;

/* Framework for registering proc handlers */
#define MX_CLK20_PROCFS_RW_FILE_OPS(name)                                           \
	static ssize_t mx_procfs_ ## name ## _write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos); \
	static ssize_t                      mx_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations mx_procfs_ ## name ## _fops = { \
		.read = mx_procfs_ ## name ## _read,                        \
		.write = mx_procfs_ ## name ## _write,                      \
		.open = mx_clk20_procfs_generic_open,                     \
		.llseek = generic_file_llseek                                 \
	}
#define MX_CLK20_PROCFS_RO_FILE_OPS(name)                                           \
	static ssize_t                      mx_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations mx_procfs_ ## name ## _fops = { \
		.read = mx_procfs_ ## name ## _read,                        \
		.open = mx_clk20_procfs_generic_open,                                  \
		.llseek = generic_file_llseek                               \
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#define MX_PDE_DATA(inode) PDE_DATA(inode)
#else
#define MX_PDE_DATA(inode) (PDE(inode)->data)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#define MX_CLK20_PROCFS_SET_UID_GID(_entry) \
	do { \
		kuid_t proc_kuid = KUIDT_INIT(AID_MXPROC); \
		kgid_t proc_kgid = KGIDT_INIT(AID_MXPROC); \
		proc_set_user(_entry, proc_kuid, proc_kgid); \
	} while (0)
#else
#define MX_CLK20_PROCFS_SET_UID_GID(entry) \
	do { \
		(entry)->uid = AID_MXPROC; \
		(entry)->gid = AID_MXPROC; \
	} while (0)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#define MX_CLK20_PROCFS_ADD_FILE(_sdev, name, parent, mode)                      \
	do {                                                               \
		struct proc_dir_entry *entry = proc_create_data(# name, mode, parent, &mx_procfs_ ## name ## _fops, _sdev); \
		MX_CLK20_PROCFS_SET_UID_GID(entry);                              \
	} while (0)
#else
#define MX_CLK20_PROCFS_ADD_FILE(_data, name, parent, mode)                      \
	do {                                                               \
		struct proc_dir_entry *entry;                              \
		entry = create_proc_entry(# name, mode, parent);           \
		if (entry) {                                               \
			entry->proc_fops = &mx_procfs_ ## name ## _fops; \
			entry->data = _data;                               \
			MX_CLK20_PROCFS_SET_UID_GID(entry);                      \
		}                                                          \
	} while (0)
#endif

#define MX_CLK20_PROCFS_REMOVE_FILE(name, parent) remove_proc_entry(# name, parent)

/* Open handler */
static int mx_clk20_procfs_generic_open(struct inode *inode, struct file *file)
{
	file->private_data = MX_PDE_DATA(inode);
	return 0;
}

/* No-op */
static ssize_t mx_procfs_restart_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	(void)user_buf;
	(void)count;
	(void)ppos;

	return 0;
}

/* Restart clock service */
static ssize_t mx_procfs_restart_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	(void)file;
	(void)user_buf;
	(void)ppos;

	mx140_clk20mhz_restart();

	SCSC_TAG_INFO(MX_PROC, "OK\n");
	return count;
}

/* Register proc handler */
MX_CLK20_PROCFS_RW_FILE_OPS(restart);

/* Populate proc node */
static int mx140_clk20mhz_create_ctrl_proc_dir(struct mx140_clk20mhz *clk20mhz)
{
	char                  dir[MX_CLK20_DIRLEN];
	struct proc_dir_entry *parent;

	(void)snprintf(dir, sizeof(dir), "%s%d", procdir_ctrl, proc_count);
	parent = proc_mkdir(dir, NULL);
	if (!parent) {
		SCSC_TAG_ERR(MX_PROC, "failed to create proc dir %s\n", procdir_ctrl);
		return -EINVAL;
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0))
	parent->data = clk20mhz;
#endif
	clk20mhz->procfs_ctrl_dir = parent;
	clk20mhz->procfs_ctrl_dir_num = proc_count;
	MX_CLK20_PROCFS_ADD_FILE(clk20mhz, restart, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SCSC_TAG_DEBUG(MX_PROC, "created %s proc dir\n", dir);
	proc_count++;

	return 0;
}

/* Remove proc node */
static void mx140_clk20mhz_remove_ctrl_proc_dir(struct mx140_clk20mhz *clk20mhz)
{
	if (clk20mhz->procfs_ctrl_dir) {
		char dir[MX_CLK20_DIRLEN];

		MX_CLK20_PROCFS_REMOVE_FILE(restart, clk20mhz->procfs_ctrl_dir);
			(void)snprintf(dir, sizeof(dir), "%s%d", procdir_ctrl, clk20mhz->procfs_ctrl_dir_num);
		remove_proc_entry(dir, NULL);
		clk20mhz->procfs_ctrl_dir = NULL;
		proc_count--;
		SCSC_TAG_DEBUG(MX_PROC, "removed %s proc dir\n", dir);
	}
}

/* Maxwell manager has detected a recoverable issue no action needed */
static u8 mx140_clk20mhz_failure_notification(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void) client;
	SCSC_TAG_INFO(CLK20, "\n");
	return err->level;
}

/* Maxwell manager has detected an issue and the service should freeze */
static bool mx140_clk20mhz_stop_on_failure(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void) err;
	atomic_set(&clk20mhz.mx140_clk20mhz_service_failed, 1);

	mutex_lock(&clk_work_lock);
	recovery = 1;
	reinit_completion(&clk20mhz.recovery_probe_completion);
	mutex_unlock(&clk_work_lock);

#ifdef MX140_CLK_VERBOSE_CALLBACKS
	/* If call back is registered, inform the user about an asynchronous failure */
	if (clk20mhz.mx140_clk20mhz_client_cb)
		clk20mhz.mx140_clk20mhz_client_cb(clk20mhz.data, MX140_CLK_ASYNC_FAIL);
#endif

	SCSC_TAG_INFO(CLK20, "\n");

	return false;
}

/* Maxwell manager has handled a failure and the chip has been resat. */
static void mx140_clk20mhz_failure_reset(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code)
{
	(void)level;
	(void)scsc_syserr_code;
	atomic_set(&clk20mhz.mx140_clk20mhz_service_failed, 1);

#ifdef MX140_CLK_VERBOSE_CALLBACKS
	/* If call back is registered, inform the user about an asynchronous failure */
	if (clk20mhz.mx140_clk20mhz_client_cb)
		clk20mhz.mx140_clk20mhz_client_cb(clk20mhz.data, MX140_CLK_ASYNC_FAIL);
#endif
	SCSC_TAG_INFO(CLK20, "\n");
}

static int mx140_clk20mhz_start_service(struct scsc_mx *mx)
{
	int r;

	/* Open the service and get resource pointers */
	clk20mhz.mx140_clk20mhz_service = scsc_mx_service_open(mx, SCSC_SERVICE_ID_CLK20MHZ, &clk20mhz.mx140_clk20mhz_service_client, &r);
	if (!clk20mhz.mx140_clk20mhz_service) {
		SCSC_TAG_ERR(CLK20, "scsc_mx_service_open failed %d\n", r);
		return r;
	}

	/* In case of recovery ensure WLBT has ownership */
	if (atomic_read(&clk20mhz.mx140_clk20mhz_service_failed)) {
		struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(clk20mhz.mx);

		if (!mif)
			goto error;

		if (mif->mif_restart)
			mif->mif_restart(mif);

		atomic_set(&clk20mhz.mx140_clk20mhz_service_failed, 0);
	}

	/* Start service. Will bring-up the chip if the chip is disabled */
	if (scsc_mx_service_start(clk20mhz.mx140_clk20mhz_service, 0)) {
		SCSC_TAG_ERR(CLK20, "scsc_mx_service_start failed\n");
		goto error;
	}

	atomic_set(&clk20mhz.mx140_clk20mhz_service_started, 1);

	/* If call back is registered, inform the user that the service has started */
	if (atomic_read(&clk20mhz.clk_request) && clk20mhz.mx140_clk20mhz_client_cb)
		clk20mhz.mx140_clk20mhz_client_cb(clk20mhz.data, MX140_CLK_STARTED);

	return 0;
error:
	return -EIO;
}

static int mx140_clk20mhz_stop_service(struct scsc_mx *mx)
{
	int r;

	if (!atomic_read(&clk20mhz.mx140_clk20mhz_service_started)) {
		SCSC_TAG_INFO(CLK20, "Service not started\n");
		return -ENODEV;
	}

	/* Stop service. */
	if (scsc_mx_service_stop(clk20mhz.mx140_clk20mhz_service)) {
		SCSC_TAG_ERR(CLK20, "scsc_mx_service_stop failed\n");
#ifdef MX140_CLK_VERBOSE_CALLBACKS
		/* If call back is registered, inform the user that the service has failed to stop */
		if (clk20mhz.mx140_clk20mhz_client_cb)
			clk20mhz.mx140_clk20mhz_client_cb(clk20mhz.data, MX140_CLK_NOT_STOPPED);
		return -EIO;
#endif
	}

	/* Ignore a service_stop timeout above as it's better to try to close */

	/* Close service, if no other service is using Maxwell, chip will turn off */
	r = scsc_mx_service_close(clk20mhz.mx140_clk20mhz_service);
	if (r) {
		SCSC_TAG_ERR(CLK20, "scsc_mx_service_close failed\n");

#ifdef MX140_CLK_VERBOSE_CALLBACKS
		/* If call back is registered, inform the user that the service has failed to close */
		if (clk20mhz.mx140_clk20mhz_client_cb)
			clk20mhz.mx140_clk20mhz_client_cb(clk20mhz.data, MX140_CLK_NOT_STOPPED);
		return -EIO;
#endif
	}

	atomic_set(&clk20mhz.mx140_clk20mhz_service_started, 0);

#ifdef MX140_CLK_VERBOSE_CALLBACKS
	/* If call back is registered, inform the user that the service has stopped */
	if (clk20mhz.mx140_clk20mhz_client_cb)
		clk20mhz.mx140_clk20mhz_client_cb(clk20mhz.data, MX140_CLK_STOPPED);
#endif
	return 0;
}

#define MX140_CLK_TRIES (20)

static void mx140_clk20mhz_work_func(struct work_struct *work)
{
	int i;
	int r = 0;
	enum mx140_clk20mhz_status status;

	mutex_lock(&clk_work_lock);

	for (i = 0; i < MX140_CLK_TRIES; i++) {
		if (atomic_read(&clk20mhz.clk_request) == 0) {
			SCSC_TAG_INFO(CLK20, "mx140_clk20mhz_start_service no longer requested\n");
			recovery = 0;
			mutex_unlock(&clk_work_lock);
			return;
		}

		SCSC_TAG_INFO(CLK20, "Calling mx140_clk20mhz_start_service\n");
		r = mx140_clk20mhz_start_service(clk20mhz.mx);
		switch (r) {
		case 0:
			SCSC_TAG_INFO(CLK20, "mx140_clk20mhz_start_service OK\n");
			recovery = 0;
			mutex_unlock(&clk_work_lock);
			return;
		case -EAGAIN:
			SCSC_TAG_INFO(CLK20, "FW not found because filesystem not mounted yet, retrying...\n");
			msleep(500); /* No FS yet, retry */
			break;
		default:
			SCSC_TAG_INFO(CLK20, "mx140_clk20mhz_start_service failed %d\n", r);
			goto err;
		}
	}
err:
	SCSC_TAG_ERR(CLK20, "Unable to start the 20MHz clock service\n");

	/* Deferred service start failure or timeout.
	 * We assume it'll never manage to start - e.g. bad or missing f/w.
	 */
	if (r) {
		struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(clk20mhz.mx);

		SCSC_TAG_ERR(CLK20, "Deferred start timeout (%d)\n", r);
		atomic_set(&clk20mhz.mx140_clk20mhz_service_failed, 1);

		/* Switch USBPLL ownership to AP so USB may be used */
		if (mif && mif->mif_cleanup)
			mif->mif_cleanup(mif);
	}

	/* If call back is registered, inform the user that the service has failed to start */
	if (atomic_read(&clk20mhz.clk_request) && clk20mhz.mx140_clk20mhz_client_cb) {
		/* The USB/PLL driver has inadequate error handing...
		 * Lie that the start was successful when AP has control
		 */
		status = atomic_read(&clk20mhz.mx140_clk20mhz_service_failed) ? MX140_CLK_STARTED : MX140_CLK_NOT_STARTED;

		/* Also lie that the start was successful when the mx140 driver is halted after f/w panic */
		if (r == -EILSEQ || r == -EPERM)
			status = MX140_CLK_STARTED;

		SCSC_TAG_INFO(CLK20, "cb %d\n", status);
		clk20mhz.mx140_clk20mhz_client_cb(clk20mhz.data, status);
	}
	recovery = 0;
	mutex_unlock(&clk_work_lock);
}

static void mx140_clk20mhz_wq_init(void)
{
	mx140_clk20mhz_wq = create_singlethread_workqueue("mx140_clk20mhz_wq");
	INIT_WORK(&mx140_clk20mhz_work, mx140_clk20mhz_work_func);
}

static void mx140_clk20mhz_wq_stop(void)
{
	cancel_work_sync(&mx140_clk20mhz_work);
	flush_workqueue(mx140_clk20mhz_wq);
}

static void mx140_clk20mhz_wq_deinit(void)
{
	mx140_clk20mhz_wq_stop();
	destroy_workqueue(mx140_clk20mhz_wq);
}

static void mx140_clk20mhz_wq_start(void)
{
	queue_work(mx140_clk20mhz_wq, &mx140_clk20mhz_work);
}

/* Register a callback function to indicate to the (USB) client the status of
 * the clock request
 */
int mx140_clk20mhz_register(void (*client_cb)(void *data, enum mx140_clk20mhz_status event), void *data)
{
	SCSC_TAG_INFO(CLK20, "cb %p, %p\n", client_cb, data);

	mutex_lock(&clk_lock);
	if (clk20mhz.mx140_clk20mhz_client_cb == NULL) {
		SCSC_TAG_INFO(CLK20, "clk20Mhz client registered\n");
		clk20mhz.mx140_clk20mhz_client_cb = client_cb;
		clk20mhz.data = data;
		mutex_unlock(&clk_lock);
		return 0;
	}

	SCSC_TAG_ERR(CLK20, "clk20Mhz client already registered\n");
	mutex_unlock(&clk_lock);
	return -EEXIST;
}
EXPORT_SYMBOL(mx140_clk20mhz_register);

/* Unregister callback function */
void mx140_clk20mhz_unregister(void)
{
	SCSC_TAG_INFO(CLK20, "\n");

	mutex_lock(&clk_lock);
	if (clk20mhz.mx140_clk20mhz_client_cb == NULL) {
		SCSC_TAG_INFO(CLK20, "clk20Mhz client not registered\n");
		mutex_unlock(&clk_lock);
		return;
	}

	clk20mhz.mx140_clk20mhz_client_cb = NULL;
	clk20mhz.data = NULL;
	mutex_unlock(&clk_lock);
}
EXPORT_SYMBOL(mx140_clk20mhz_unregister);

/* Indicate that an external client requires mx140's 20 MHz clock.
 * The Core driver will boot mx140 as required and ensure that the
 * clock remains running.
 *
 * If a callback was installed by register(), do this asynchronously.
 */
int mx140_clk20mhz_request(void)
{
	mutex_lock(&clk_lock);
	atomic_inc(&clk20mhz.clk_request);

	SCSC_TAG_INFO(CLK20, "%d\n", atomic_read(&clk20mhz.clk_request));

	if (!atomic_read(&clk20mhz.maxwell_is_present)) {
		SCSC_TAG_INFO(CLK20, "Maxwell is not present yet, store request\n");
		atomic_set(&clk20mhz.request_pending, 1);
		mutex_unlock(&clk_lock);
		return 0;
	}

	if (recovery) {
		int r;

		mutex_unlock(&clk_lock);
		r = wait_for_completion_timeout(&clk20mhz.recovery_probe_completion,
		  msecs_to_jiffies(MX140_SERVICE_RECOVERY_TIMEOUT));
		mutex_lock(&clk_lock);
		if (r == 0) {
			SCSC_TAG_INFO(CLK20, "recovery_probe_completion timeout - try a start\n");
			mx140_clk20mhz_wq_start();
		}
	} else if (!atomic_read(&clk20mhz.mx140_clk20mhz_service_started))
		mx140_clk20mhz_wq_start();
	else
		SCSC_TAG_INFO(CLK20, "Service already started\n");

	mutex_unlock(&clk_lock);
	return 0;
}
EXPORT_SYMBOL(mx140_clk20mhz_request);

/* Indicate that an external client no requires mx140's 20 MHz clock
 * The Core driver will shut down mx140 if no other services are
 * currently running
 *
 * If a callback was installed by register(), do this asynchronously.
 */
int mx140_clk20mhz_release(void)
{
	int ret = 0;

	mutex_lock(&clk_lock);
	atomic_dec(&clk20mhz.clk_request);
	SCSC_TAG_INFO(CLK20, "%d\n", atomic_read(&clk20mhz.clk_request));

	if (!atomic_read(&clk20mhz.maxwell_is_present)) {
		SCSC_TAG_INFO(CLK20, "Maxwell is released before probe\n");
		if (!atomic_read(&clk20mhz.request_pending)) {
			SCSC_TAG_INFO(CLK20, "Maxwell had request pending. Cancel it\n");
			atomic_set(&clk20mhz.request_pending, 0);
		}
		mutex_unlock(&clk_lock);
		return 0;
	}

	/* Cancel any pending attempt */
	mx140_clk20mhz_wq_stop();

	if (recovery) {
		recovery_pending_stop_close = 1;
	} else {
		ret = mx140_clk20mhz_stop_service(clk20mhz.mx);
		if (ret == -ENODEV) {
			/* Suppress error if it wasn't running */
			ret = 0;
		}
	}

	/* Suppress stop failure if the service is failed */
	if (atomic_read(&clk20mhz.mx140_clk20mhz_service_failed)) {
		SCSC_TAG_INFO(CLK20, "Return OK as control is with AP\n");
		atomic_set(&clk20mhz.mx140_clk20mhz_service_started, 0);
		ret = 0;
	}

	mutex_unlock(&clk_lock);
	return ret;
}
EXPORT_SYMBOL(mx140_clk20mhz_release);

/* Probe callback after platform driver is registered */
void mx140_clk20mhz_probe(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{
	SCSC_TAG_INFO(CLK20, "\n");

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && !recovery) {
		SCSC_TAG_INFO(CLK20, "Ignore probe - no recovery in progress\n");
		return;
	}

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && recovery) {
		SCSC_TAG_INFO(CLK20, "Recovery probe\n");

		/**
		 * If recovery_pending_stop_close is set, then there was a stop
		 * during recovery (could be due to USB cable unplugged) so
		 * recovery should just stop here.
		 * The mx140_clk service has been closed in the remove callback.
		 */
		mutex_lock(&clk_lock);
		if (recovery_pending_stop_close) {
			SCSC_TAG_INFO(CLK20, "Recovery probe - stop during recovery, so don't recover\n");
			recovery_pending_stop_close = 0;
			recovery = 0;
			mutex_unlock(&clk_lock);
			/**
			 * Should there have been a new start request during
			 * recovery (very unlikely), then the complete timeout
			 * will ensure that a start is requested.
			 */
			return;
		}
		mutex_unlock(&clk_lock);

		mutex_lock(&clk_work_lock);
		mx140_clk20mhz_wq_start();
		mutex_unlock(&clk_work_lock);
		complete_all(&clk20mhz.recovery_probe_completion);
	} else {
		SCSC_TAG_INFO(CLK20, "Maxwell probed\n");
		clk20mhz.mx = mx;
		clk20mhz.mx140_clk20mhz_service_client.failure_notification   = mx140_clk20mhz_failure_notification;
		clk20mhz.mx140_clk20mhz_service_client.stop_on_failure_v2   = mx140_clk20mhz_stop_on_failure;
		clk20mhz.mx140_clk20mhz_service_client.failure_reset_v2     = mx140_clk20mhz_failure_reset;

		mx140_clk20mhz_create_ctrl_proc_dir(&clk20mhz);

		mx140_clk20mhz_wq_init();

		atomic_set(&clk20mhz.maxwell_is_present, 1);

		mutex_lock(&clk_work_lock);
		if ((auto_start || atomic_read(&clk20mhz.request_pending))) {
			atomic_set(&clk20mhz.request_pending, 0);
			SCSC_TAG_INFO(CLK20, "start pending service\n");
			mx140_clk20mhz_wq_start();
		}
		mutex_unlock(&clk_work_lock);
	}
}


static void mx140_clk20mhz_restart(void)
{
	int r;
	struct scsc_mif_abs *mif;

	SCSC_TAG_INFO(CLK20, "\n");

	wake_lock(&clk20mhz.clk_wake_lock);

	mutex_lock(&clk_lock);

	if (!atomic_read(&clk20mhz.mx140_clk20mhz_service_started)) {
		SCSC_TAG_INFO(CLK20, "service wasn't started\n");
		goto done;
	}

	mif = scsc_mx_get_mif_abs(clk20mhz.mx);
	if (mif == NULL)
		goto done;

	/* Don't stop the 20 MHz clock service. Leave it running until
	 * WLBT resets due to the service_close().
	 */

	/* Ensure USBPLL is running and owned by AP, to stop USB disconnect */
	if (mif->mif_cleanup)
		mif->mif_cleanup(mif);

	r = scsc_mx_service_close(clk20mhz.mx140_clk20mhz_service);
	if (r) {
		SCSC_TAG_ERR(CLK20, "scsc_mx_service_close failed (%d)\n", r);
		goto done;
	}

	/* ...and restart the 20 MHz clock service */
	clk20mhz.mx140_clk20mhz_service = scsc_mx_service_open(clk20mhz.mx, SCSC_SERVICE_ID_CLK20MHZ, &clk20mhz.mx140_clk20mhz_service_client, &r);
	if (clk20mhz.mx140_clk20mhz_service == NULL) {
		SCSC_TAG_ERR(CLK20, "reopen failed %d\n", r);
		atomic_set(&clk20mhz.mx140_clk20mhz_service_started, 0);
		goto done;
	}

	/* Ensure USBPLL is owned by WLBT again */
	if (mif->mif_restart)
		mif->mif_restart(mif);

	r = scsc_mx_service_start(clk20mhz.mx140_clk20mhz_service, 0);
	if (r) {
		SCSC_TAG_ERR(CLK20, "restart failed %d\n", r);
		r = scsc_mx_service_close(clk20mhz.mx140_clk20mhz_service);
		if (r)
			SCSC_TAG_ERR(CLK20, "scsc_mx_service_close failed %d\n", r);
		atomic_set(&clk20mhz.mx140_clk20mhz_service_started, 0);
		goto done;
	}

	SCSC_TAG_INFO(CLK20, "restarted\n");
done:
	mutex_unlock(&clk_lock);
	wake_unlock(&clk20mhz.clk_wake_lock);
}

/* Remove callback platform driver is unregistered */
void mx140_clk20mhz_remove(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{
	int r;

	mutex_lock(&clk_work_lock);
	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && !recovery) {
		SCSC_TAG_INFO(CLK20, "Ignore recovery remove: Service driver not active\n");
		goto done;
	} else if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && recovery) {
		struct scsc_mif_abs *mif;

		SCSC_TAG_INFO(CLK20, "Recovery remove\n");

		mutex_lock(&clk_lock);
		mx140_clk20mhz_wq_stop();

		mif = scsc_mx_get_mif_abs(clk20mhz.mx);
		if (mif == NULL)
			goto done_local;

		/**
		 * If there's been a stop during recovery ensure that the
		 * mx140_clk service is closed in the mx driver, but do not
		 * touch USBPLL ownership since this will already have been
		 * handled.
		 */
		if (!recovery_pending_stop_close) {
			/* Don't stop the clock service - leave it running until
			 * service_close() resets WLBT.
			 */

			/* Switch ownership of USBPLL to the AP. Ownership
			 * returns to WLBT after recovery completes.
			 */
			if (mif->mif_cleanup)
				mif->mif_cleanup(mif);
		}

		r = scsc_mx_service_close(clk20mhz.mx140_clk20mhz_service);
		if (r)
			SCSC_TAG_INFO(CLK20, "scsc_mx_service_close failed %d\n", r);

		atomic_set(&clk20mhz.mx140_clk20mhz_service_started, 0);
done_local:
		mutex_unlock(&clk_lock);
	} else {
		SCSC_TAG_INFO(CLK20, "Maxwell removed\n");
		mx140_clk20mhz_remove_ctrl_proc_dir(&clk20mhz);
		atomic_set(&clk20mhz.maxwell_is_present, 0);
		mx140_clk20mhz_wq_deinit();
	}

done:
	mutex_unlock(&clk_work_lock);
}

/* 20MHz client driver */
struct scsc_mx_module_client mx140_clk20mhz_driver = {
	.name = "MX 20MHz clock client",
	.probe = mx140_clk20mhz_probe,
	.remove = mx140_clk20mhz_remove,
};

/* 20MHz service driver initialization */
static int __init mx140_clk20mhz_init(void)
{
	int ret;

	SCSC_TAG_INFO(CLK20, "Registering service\n");

	wake_lock_init(&clk20mhz.clk_wake_lock, WAKE_LOCK_SUSPEND, "clk20_wl");
	init_completion(&clk20mhz.recovery_probe_completion);

	atomic_set(&clk20mhz.clk_request, 0);
	atomic_set(&clk20mhz.maxwell_is_present, 0);
	atomic_set(&clk20mhz.mx140_clk20mhz_service_started, 0);
	atomic_set(&clk20mhz.request_pending, 0);
	atomic_set(&clk20mhz.mx140_clk20mhz_service_failed, 0);

	/* Register with Maxwell Framework */
	ret = scsc_mx_module_register_client_module(&mx140_clk20mhz_driver);
	if (ret) {
		SCSC_TAG_ERR(CLK20, "scsc_mx_module_register_client_module failed: r=%d\n", ret);
		return ret;
	}

#ifdef CONFIG_SCSC_CLK20MHZ_TEST
	mx140_clk_test_init();
#endif
	return 0;
}

static void __exit mx140_clk20mhz_exit(void)
{
	scsc_mx_module_unregister_client_module(&mx140_clk20mhz_driver);
#ifdef CONFIG_SCSC_CLK20MHZ_TEST
	mx140_clk_test_exit();
#endif

	complete_all(&clk20mhz.recovery_probe_completion);
	wake_lock_destroy(&clk20mhz.clk_wake_lock);
}

module_init(mx140_clk20mhz_init);
module_exit(mx140_clk20mhz_exit);

MODULE_DESCRIPTION("Samsung Maxwell 20MHz Clock Service");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
