/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/poll.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <scsc/scsc_mx.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include "mx_dbg_sampler.h"
#include "scsc_mif_abs.h"
#include "mxman.h"
#include "scsc_mx_impl.h"
#include "miframman.h"

#include <scsc/scsc_logring.h>

static unsigned int source_addr = 0xd0300028;
module_param(source_addr, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(source_addr, "Relative address of Location to sample (usually a register) - default: 0xd0300028. Loaded at /dev open");

static unsigned int num_bytes = 4;
module_param(num_bytes, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(num_bytes, "Number of significant octets (1,2 or 4) to log (lsbytes from source) - default: 4. Loaded at /dev open");

static unsigned int period_usecs;
module_param(period_usecs, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(period_usecs, "Sampling period. 0 means as fast as possible (powers of 2 only) - default: 0. Loaded at /dev open");

static bool auto_start;
module_param(auto_start, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(auto_start, "Start/stop sampling when service is started/stopped? - default: N. Loaded at /dev open");

static unsigned int buf_len = 512 * 1024;
module_param(buf_len, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(buf_len, "Circular buffer length (octets, 2^n) in bytes - default: 524288. Loaded at /dev open");

static unsigned int kfifo_len = 4 * 1024 * 1024;
module_param(kfifo_len, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(kfifo_len, "Kfifo buffer length (octets, 2^n) in bytes - default: 4194304. Loaded at /dev open");

static bool self_test;
module_param(self_test, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(self_test, "Execute self test by triggering a Kernel thread which writes into shared memory and then calls irg handler - default: N. Loaded at /dev open");

#define DRV_NAME                "mx_dbg_sampler"
#define DEVICE_NAME             "mx_dbg_sampler"

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define VER_MAJOR               0
#define VER_MINOR               0

#define SCSC_MX_DEBUG_NODE      1

#define SCSC_MX_DEBUG_INTERFACES        (5 * (SCSC_MX_DEBUG_NODE))

DECLARE_BITMAP(bitmap_dbg_sampler_minor, SCSC_MX_DEBUG_INTERFACES);

#define NO_ERROR                0
#define BUFFER_OVERFLOW         1
#define KFIFO_ERROR             2
#define KFIFO_FULL              3

struct mx_dbg_sampler_dev {
	/* file pointer */
	struct file                 *filp;
	/* char device */
	struct cdev                 cdev;
	/*device pointer*/
	struct device               *dev;
	/* mx_wlan_client */
	struct scsc_service_client  mx_client;
	/*service pointer*/
	struct scsc_service         *service;
	/*service pointer*/
	scsc_mifram_ref             ref;
	/*mx pointer*/
	struct scsc_mx              *mx;
	/* Associated kfifo */
	DECLARE_KFIFO_PTR(fifo, u8);
	/* Associated read_wait queue.*/
	wait_queue_head_t           read_wait;
	/* Associated debug_buffer */
	struct debug_sampler_config info;
	/* Buffer read index */
	u32                         read_idx;
	/* Device in error */
	u8                          error;
	/* Device node spinlock for IRQ */
	spinlock_t                  spinlock;
	/* Device node mutex for fops */
	struct mutex                mutex;
	/* To profile kfifo num elements */
	u32                         kfifo_max;
	/* Device is in use */
	bool                        in_use;
};

/**
 * SCSC User Space debug sampler interface (singleton)
 */
static struct {
	dev_t                     device;
	struct class              *class_mx_dbg_sampler;
	struct mx_dbg_sampler_dev devs[SCSC_MX_DEBUG_INTERFACES];
} mx_dbg_sampler;

static int recovery_in_progress;

static u8 mx_dbg_sampler_failure_notification(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void)client;
	SCSC_TAG_INFO(MX_SAMPLER, "OK\n");
	return err->level;
}


static bool mx_dbg_sampler_stop_on_failure(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void)client;
	(void)err;
	SCSC_TAG_INFO(MX_SAMPLER, "TODO\n");
	recovery_in_progress = 1;
	return false;
}

static void mx_dbg_sampler_failure_reset(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code)
{
	(void)client;
	(void)level;
	(void)scsc_syserr_code;
	SCSC_TAG_INFO(MX_SAMPLER, "TODO\n");
}

static void mx_wlan_read_process(const void *data, size_t length, struct mx_dbg_sampler_dev *mx_dev)
{
	int  ret;
	void *read_ptr;
	u32  elements;

	/* Adjust lenght for kfifo type (u8)- elements -*/
	elements = length;

	if (mx_dev->filp) {
		/* put string into the fifo */
		if (kfifo_avail(&mx_dev->fifo) >= elements) {
			/* Push values in Fifo*/
			read_ptr = (void *)data + (mx_dev->read_idx & (buf_len - 1));
			ret = kfifo_in(&mx_dev->fifo, read_ptr, elements);
			mx_dev->read_idx += ret;
			if (ret != elements || ret == 0) {
				mx_dev->error = KFIFO_ERROR;
				return;
			}
			ret = kfifo_len(&mx_dev->fifo);
			if (ret > mx_dev->kfifo_max)
				mx_dev->kfifo_max = ret;
		} else {
			/* Mask interrupt to avoid interrupt storm */
			mx_dev->error = KFIFO_FULL;
			return;
		}
		wake_up_interruptible(&mx_dev->read_wait);
	}
	/* Device is closed. Silenty return */
}

static void mx_dbg_sampler_irq_handler(int irq, void *data)
{
	struct mx_dbg_sampler_dev *mx_dev = (struct mx_dbg_sampler_dev *)data;
	struct scsc_service       *service = mx_dev->service;
	u32                       write_ref;
	u32                       data_ref;
	void                      *write_ptr;
	void                      *data_ptr;
	u32                       read_idx;
	u32                       write_idx;
	size_t                    to_read;
	unsigned long             flags;

	spin_lock_irqsave(&mx_dev->spinlock, flags);

	/* check whether service has been released */
	if (!mx_dev->service) {
		spin_unlock_irqrestore(&mx_dev->spinlock, flags);
		return;
	}

	read_idx = mx_dev->read_idx;

	write_ref = mx_dev->info.buffer_info.write_index_offset;
	write_ptr = scsc_mx_service_mif_addr_to_ptr(service, write_ref);
	write_idx = *((u32 *)write_ptr);

	to_read = abs((s32)write_idx - (s32)read_idx);

	/* TODO: Decide whether we need to do the memdump on a workqueue/tasklet or just in the int handler */
	if (to_read > mx_dev->info.buffer_info.buf_len) {
		scsc_service_mifintrbit_bit_clear(service, irq);
		scsc_service_mifintrbit_bit_mask(service, irq);
		mx_dev->error = BUFFER_OVERFLOW;
		goto end;
	}

	data_ref = mx_dev->info.buffer_info.buf_offset;
	data_ptr = scsc_mx_service_mif_addr_to_ptr(service, data_ref);
	mx_wlan_read_process(data_ptr, to_read, mx_dev); /* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(service, irq);
	scsc_service_mifintrbit_bit_unmask(service, irq);
end:
	spin_unlock_irqrestore(&mx_dev->spinlock, flags);

	/* Mask if dev is in error */
	/* We shouldn't be printing out lots of stuff here, but it is in error condition */
	if (mx_dev->error != NO_ERROR) {
		scsc_service_mifintrbit_bit_mask(service, irq);
		if (mx_dev->error == BUFFER_OVERFLOW)
			SCSC_TAG_ERR(MX_SAMPLER, "Error, Buffer Overflow %zu write_idx 0x%x read_idex 0x%x\n", to_read, write_idx, read_idx);
		else if (mx_dev->error == KFIFO_ERROR)
			SCSC_TAG_ERR(MX_SAMPLER, "Error Pushing values in kfifo\n");
		else if (mx_dev->error == KFIFO_FULL)
			SCSC_TAG_ERR(MX_SAMPLER, "Error kfifo is full\n");
	}
}

static struct task_struct *mx_dbg_sampler_task;

#define BULK_DATA       (16 * 1024)
int mx_dbg_sampler_thread(void *data)
{
	struct mx_dbg_sampler_dev *dev = (struct mx_dbg_sampler_dev *)data;
	struct scsc_service       *service = dev->service;
	u32                       write;
	u32                       mem;
	void                      *write_ptr;
	u32                       *mem_ptr;
	u32                       val;
	u32                       i;
	u32                       end;

	while (!kthread_should_stop() && !(dev->error != NO_ERROR)) {
		write = dev->info.buffer_info.write_index_offset;
		write_ptr = scsc_mx_service_mif_addr_to_ptr(service, write);
		val = *((u32 *)write_ptr);
		val += BULK_DATA;
		*((u32 *)write_ptr) = val;

		end = BULK_DATA;


		mem = dev->info.buffer_info.buf_offset;
		mem_ptr = scsc_mx_service_mif_addr_to_ptr(service, mem);

		mem_ptr += dev->read_idx / sizeof(u32);

		for (i = 0; i < end / 4; i++)
			*((u32 *)mem_ptr++) = 0x33323130;

		mx_dbg_sampler_irq_handler(0, dev);
		mdelay(100);
	}
	mx_dbg_sampler_task = NULL;
	return 0;
}

static int mx_dbg_sampler_allocate_resources(struct scsc_service *service, struct mx_dbg_sampler_dev *mx_dev)
{
	scsc_mifram_ref     ref, ref_buffer, ref_index;
	int                 ret = 0;
	struct debug_sampler_align *mem;

	/* Allocate memory */
	ret = scsc_mx_service_mifram_alloc(service, buf_len + sizeof(struct debug_sampler_align), &ref, 64);
	if (ret)
		return -ENOMEM;
	mem = (struct debug_sampler_align *)scsc_mx_service_mif_addr_to_ptr(service, ref);

	/* Allocate interrupt */
	ret = scsc_service_mifintrbit_register_tohost(service, mx_dbg_sampler_irq_handler, mx_dev);
	if (ret < 0) {
		SCSC_TAG_ERR(MX_SAMPLER, "Error allocating interrupt\n");
		scsc_mx_service_mifram_free(service, ref);
		return ret;
	}
	/* Populate the buffer_info */
	mem->config.version = mx_dev->info.version = 0;

	scsc_mx_service_mif_ptr_to_addr(service, &mem->mem, &ref_buffer);
	mem->config.buffer_info.buf_offset = mx_dev->info.buffer_info.buf_offset = ref_buffer;

	mem->config.buffer_info.buf_len = mx_dev->info.buffer_info.buf_len = buf_len;

	scsc_mx_service_mif_ptr_to_addr(service, &mem->index, &ref_index);
	mem->config.buffer_info.write_index_offset =
		mx_dev->info.buffer_info.write_index_offset = ref_index;

	/* Reset write index */
	mem->index = 0;

	mem->config.buffer_info.intr_num = mx_dev->info.buffer_info.intr_num = ret;

	mem->config.sample_spec.source_addr = source_addr;
	mem->config.sample_spec.num_bytes = num_bytes;
	mem->config.sample_spec.period_usecs = period_usecs;
	mem->config.auto_start = auto_start;

	mx_dev->ref = ref;
	/* Reset read index */
	mx_dev->read_idx = 0;

	return 0;
}

static int mx_dbg_sampler_free_resources(struct scsc_service *service, struct mx_dbg_sampler_dev *mx_dev)
{
	if (self_test)
		if (mx_dbg_sampler_task)
			kthread_stop(mx_dbg_sampler_task);

	scsc_service_mifintrbit_unregister_tohost(service,
						  mx_dev->info.buffer_info.intr_num);
	scsc_mx_service_mifram_free(service,
				    mx_dev->ref);
	return 0;
}

int mx_dbg_sampler_open(struct inode *inode, struct file *filp)
{
	struct mx_dbg_sampler_dev *mx_dev;
	int                       ret = 0, r;

	mx_dev = container_of(inode->i_cdev, struct mx_dbg_sampler_dev, cdev);

	if (mutex_lock_interruptible(&mx_dev->mutex))
		return -ERESTARTSYS;

	if (mx_dev->in_use) {
		SCSC_TAG_ERR(MX_SAMPLER, "Device node already opened. Only one instance allowed. Exit\n");
		ret = -EIO;
		goto end;
	}

	if (filp->private_data) {
		SCSC_TAG_ERR(MX_SAMPLER, "Service already started\n");
		ret = -EIO;
		goto end;
	}

	filp->private_data = mx_dev;
	mx_dev->filp = filp;
	/* Clear any remaining error */
	mx_dev->error = NO_ERROR;

	ret = kfifo_alloc(&mx_dev->fifo, kfifo_len, GFP_KERNEL);
	if (ret) {
		SCSC_TAG_ERR(MX_SAMPLER, "kfifo_alloc failed");
		ret = -EIO;
		goto error;
	}

	mx_dev->service = scsc_mx_service_open(mx_dev->mx, SCSC_SERVICE_ID_DBG_SAMPLER, &mx_dev->mx_client, &ret);
	if (!mx_dev->service) {
		SCSC_TAG_ERR(MX_SAMPLER, "Error opening service is NULL\n");
		kfifo_free(&mx_dev->fifo);
		ret = -EIO;
		goto error;
	}
	/* Allocate resources */
	ret = mx_dbg_sampler_allocate_resources(mx_dev->service, mx_dev);
	if (ret) {
		SCSC_TAG_ERR(MX_SAMPLER, "Error Allocating resources\n");
		kfifo_free(&mx_dev->fifo);
		r = scsc_mx_service_close(mx_dev->service);
		if (r)
			SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_close failed %d\n", r);
		goto error;
	}

	ret = scsc_mx_service_start(mx_dev->service, mx_dev->ref);
	if (ret) {
		SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_start failed\n");
		mx_dbg_sampler_free_resources(mx_dev->service, mx_dev);
		kfifo_free(&mx_dev->fifo);
		r = scsc_mx_service_close(mx_dev->service);
		if (r)
			SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_close failed %d\n", r);
		goto error;
	}
	/* WARNING: At this point we may be receiving interrupts from Maxwell */

	/* Trigger the dummy thread to test the functionality */
	if (self_test)
		mx_dbg_sampler_task = kthread_run(mx_dbg_sampler_thread, (void *)mx_dev, "mx_dbg_sampler_thread");

	SCSC_TAG_INFO(MX_SAMPLER, "%s: Sampling....\n", DRV_NAME);
	mx_dev->in_use = true;
	mutex_unlock(&mx_dev->mutex);
	return 0;
error:
	filp->private_data = NULL;
	mx_dev->filp = NULL;
	mx_dev->service = NULL;
end:
	mutex_unlock(&mx_dev->mutex);
	return ret;
}

static ssize_t mx_dbg_sampler_read(struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
	unsigned int              copied;
	int                       ret = 0;
	struct mx_dbg_sampler_dev *mx_dev;

	mx_dev = filp->private_data;

	if (mutex_lock_interruptible(&mx_dev->mutex))
		return -EINTR;

	/* Check whether the device is in error */
	if (mx_dev->error != NO_ERROR) {
		SCSC_TAG_ERR(MX_SAMPLER, "Device in error\n");
		ret = -EIO;
		goto end;
	}

	while (len) {
		if (kfifo_len(&mx_dev->fifo)) {
			ret = kfifo_to_user(&mx_dev->fifo, buf, len, &copied);
			if (!ret)
				ret = copied;
			break;
		}

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		ret = wait_event_interruptible(mx_dev->read_wait,
					       !kfifo_is_empty(&mx_dev->fifo));
		if (ret < 0)
			break;
	}
end:
	mutex_unlock(&mx_dev->mutex);
	return ret;
}

static unsigned mx_dbg_sampler_poll(struct file *filp, poll_table *wait)
{
	struct mx_dbg_sampler_dev *mx_dev;
	int                       ret;

	mx_dev = filp->private_data;

	if (mutex_lock_interruptible(&mx_dev->mutex))
		return -EINTR;

	if (mx_dev->error != NO_ERROR) {
		ret = POLLERR;
		goto end;
	}

	poll_wait(filp, &mx_dev->read_wait, wait);

	if (!kfifo_is_empty(&mx_dev->fifo)) {
		ret = POLLIN | POLLRDNORM;  /* readeable */
		goto end;
	}

	ret = POLLOUT | POLLWRNORM;         /* writable */

end:
	mutex_unlock(&mx_dev->mutex);
	return ret;
}

int mx_dbg_sampler_release(struct inode *inode, struct file *filp)
{
	struct mx_dbg_sampler_dev *mx_dev;
	unsigned long             flags;
	int                       r;

	mx_dev = container_of(inode->i_cdev, struct mx_dbg_sampler_dev, cdev);

	if (mutex_lock_interruptible(&mx_dev->mutex))
		return -EINTR;

	if (mx_dev->filp == NULL) {
		SCSC_TAG_ERR(MX_SAMPLER, "Device already closed\n");
		mutex_unlock(&mx_dev->mutex);
		return -EIO;
	}

	if (mx_dev != filp->private_data) {
		SCSC_TAG_ERR(MX_SAMPLER, "Data mismatch\n");
		mutex_unlock(&mx_dev->mutex);
		return -EIO;
	}

	spin_lock_irqsave(&mx_dev->spinlock, flags);
	filp->private_data = NULL;
	mx_dev->filp = NULL;
	mx_dev->in_use = false;
	kfifo_free(&mx_dev->fifo);
	spin_unlock_irqrestore(&mx_dev->spinlock, flags);

	if (mx_dev->service) {
		r = scsc_mx_service_stop(mx_dev->service);
		if (r)
			SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_stop failed err: %d\n", r);
		mx_dbg_sampler_free_resources(mx_dev->service, mx_dev);
		r = scsc_mx_service_close(mx_dev->service);
		if (r)
			SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_close failed %d\n", r);

		spin_lock_irqsave(&mx_dev->spinlock, flags);
		mx_dev->service = NULL;
		spin_unlock_irqrestore(&mx_dev->spinlock, flags);
	}

	mutex_unlock(&mx_dev->mutex);
	SCSC_TAG_INFO(MX_SAMPLER, "%s: Sampling... end. Kfifo_max = %d\n", DRV_NAME, mx_dev->kfifo_max);
	return 0;
}

static const struct file_operations mx_dbg_sampler_fops = {
	.owner          = THIS_MODULE,
	.open           = mx_dbg_sampler_open,
	.read           = mx_dbg_sampler_read,
	.release        = mx_dbg_sampler_release,
	.poll           = mx_dbg_sampler_poll,
};

void mx_dbg_sampler_probe(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{
	dev_t devn;
	int   ret, i = SCSC_MX_DEBUG_INTERFACES;
	char  dev_name[20];
	long  uid = 0;
	int   minor;
	struct mx_dbg_sampler_dev *mx_dev;

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && !recovery_in_progress) {
		SCSC_TAG_INFO(MX_SAMPLER, "Recovery remove - no recovery in progress\n");
		return;
	}

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && recovery_in_progress) {
		SCSC_TAG_INFO(MX_SAMPLER, "Recovery probe\n");

		while (i--)
			if (mx_dbg_sampler.devs[i].cdev.dev && mx_dbg_sampler.devs[i].mx) {
				mx_dev = &mx_dbg_sampler.devs[i];
				/* This should be never be true - as knod should prevent unloading while
				 * the service (device node) is open */

				mx_dev->service = scsc_mx_service_open(mx_dev->mx, SCSC_SERVICE_ID_DBG_SAMPLER, &mx_dev->mx_client, &ret);
				if (!mx_dev->service) {
					SCSC_TAG_ERR(MX_SAMPLER, "Error opening service is NULL\n");
				} else {
					int r;

					ret = scsc_mx_service_start(mx_dev->service, mx_dev->ref);
					if (ret) {
						SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_start failed\n");
						mx_dbg_sampler_free_resources(mx_dev->service, mx_dev);
						r = scsc_mx_service_close(mx_dev->service);
						if (r)
							SCSC_TAG_ERR(MX_SAMPLER,
							"scsc_mx_service_close failed %d\n", r);
					}
				}
			}
		recovery_in_progress = 0;
	} else {
		/* Search for free minors */
		minor = find_first_zero_bit(bitmap_dbg_sampler_minor, SCSC_MX_DEBUG_INTERFACES);
		if (minor >= SCSC_MX_DEBUG_INTERFACES) {
			SCSC_TAG_ERR(MX_SAMPLER, "minor %d > SCSC_TTY_MINORS\n", minor);
			return;
		}

		devn = MKDEV(MAJOR(mx_dbg_sampler.device), MINOR(minor));

		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "debug_sampler");

		cdev_init(&mx_dbg_sampler.devs[minor].cdev, &mx_dbg_sampler_fops);
		mx_dbg_sampler.devs[minor].cdev.owner = THIS_MODULE;
		mx_dbg_sampler.devs[minor].cdev.ops = &mx_dbg_sampler_fops;

		ret = cdev_add(&mx_dbg_sampler.devs[minor].cdev, devn, 1);
		if (ret) {
			mx_dbg_sampler.devs[minor].cdev.dev = 0;
			mx_dbg_sampler.devs[minor].dev = NULL;
			return;
		}

		mx_dbg_sampler.devs[minor].dev = device_create(mx_dbg_sampler.class_mx_dbg_sampler, NULL, mx_dbg_sampler.devs[minor].cdev.dev, NULL, dev_name);

		if (mx_dbg_sampler.devs[minor].dev == NULL) {
			SCSC_TAG_ERR(MX_SAMPLER, "dev is NULL\n");
			cdev_del(&mx_dbg_sampler.devs[minor].cdev);
			return;
		}

		mx_dbg_sampler.devs[minor].mx = mx;
		mx_dbg_sampler.devs[minor].mx_client.failure_notification = mx_dbg_sampler_failure_notification;
		mx_dbg_sampler.devs[minor].mx_client.stop_on_failure_v2 = mx_dbg_sampler_stop_on_failure;
		mx_dbg_sampler.devs[minor].mx_client.failure_reset_v2 = mx_dbg_sampler_failure_reset;

		mutex_init(&mx_dbg_sampler.devs[minor].mutex);
		spin_lock_init(&mx_dbg_sampler.devs[minor].spinlock);
		mx_dbg_sampler.devs[minor].kfifo_max = 0;

		init_waitqueue_head(&mx_dbg_sampler.devs[minor].read_wait);

		/* Update bit mask */
		set_bit(minor, bitmap_dbg_sampler_minor);

		SCSC_TAG_INFO(MX_SAMPLER, "%s: Ready to start sampling....\n", DRV_NAME);
	}
}

void mx_dbg_sampler_remove(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{
	int i = SCSC_MX_DEBUG_INTERFACES, r;
	struct mx_dbg_sampler_dev *mx_dev;

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && !recovery_in_progress) {
		SCSC_TAG_INFO(MX_SAMPLER, "Recovery remove - no recovery in progress\n");
		return;
	}

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && recovery_in_progress) {
		SCSC_TAG_INFO(MX_SAMPLER, "Recovery remove\n");

		while (i--)
			if (mx_dbg_sampler.devs[i].cdev.dev && mx_dbg_sampler.devs[i].mx) {
				mx_dev = &mx_dbg_sampler.devs[i];
				/* This should be never be true - as knod should prevent unloading while
				 * the service (device node) is open */
				if (mx_dbg_sampler.devs[i].service) {
					r = scsc_mx_service_stop(mx_dev->service);
					if (r)
						SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_stop failed err: %d\n", r);

					r = scsc_mx_service_close(mx_dev->service);
					if (r)
						SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_close failed err: %d\n", r);
				}
			}
	} else {
		while (i--)
			if (mx_dbg_sampler.devs[i].mx == mx) {
				device_destroy(mx_dbg_sampler.class_mx_dbg_sampler, mx_dbg_sampler.devs[i].cdev.dev);
				cdev_del(&mx_dbg_sampler.devs[i].cdev);
				memset(&mx_dbg_sampler.devs[i].cdev, 0, sizeof(struct cdev));
				mx_dbg_sampler.devs[i].mx = NULL;
				clear_bit(i, bitmap_dbg_sampler_minor);
			}
	}
}

/* Test client driver registration */
struct scsc_mx_module_client mx_dbg_sampler_driver = {
	.name = "MX client test driver",
	.probe = mx_dbg_sampler_probe,
	.remove = mx_dbg_sampler_remove,
};

/* Test client driver registration */
static int __init mx_dbg_sampler_init(void)
{
	int ret;

	SCSC_TAG_INFO(MX_SAMPLER, "mx_dbg_sampler INIT; version: %d.%d\n", VER_MAJOR, VER_MINOR);

	ret = alloc_chrdev_region(&mx_dbg_sampler.device, 0, SCSC_MX_DEBUG_INTERFACES, "mx_dbg_sampler_char");
	if (ret)
		goto error;

	mx_dbg_sampler.class_mx_dbg_sampler = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(mx_dbg_sampler.class_mx_dbg_sampler)) {
		SCSC_TAG_ERR(MX_SAMPLER, "mx_dbg_sampler class creation failed\n");
		ret = PTR_ERR(mx_dbg_sampler.class_mx_dbg_sampler);
		goto error_class;
	}

	ret = scsc_mx_module_register_client_module(&mx_dbg_sampler_driver);
	if (ret) {
		SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_module_register_client_module failed: ret=%d\n", ret);
		goto error_reg;
	}

	return 0;

error_reg:
	class_destroy(mx_dbg_sampler.class_mx_dbg_sampler);
error_class:
	unregister_chrdev_region(mx_dbg_sampler.device, SCSC_MX_DEBUG_INTERFACES);
error:
	return ret;
}

/* module level */
static void __exit mx_dbg_sampler_unload(void)
{
	int                       i = SCSC_MX_DEBUG_INTERFACES;
	unsigned long             flags;
	struct mx_dbg_sampler_dev *mx_dev;
	int                       r;

	while (i--)
		if (mx_dbg_sampler.devs[i].cdev.dev && mx_dbg_sampler.devs[i].mx) {
			mx_dev = &mx_dbg_sampler.devs[i];
			/* This should be never be true - as knod should prevent unloading while
			 * the service (device node) is open */
			if (mx_dbg_sampler.devs[i].service) {
				r = scsc_mx_service_stop(mx_dev->service);
				if (r)
					SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_stop failed err: %d\n", r);
				mx_dbg_sampler_free_resources(mx_dev->service, mx_dev);
				r = scsc_mx_service_close(mx_dev->service);
				if (r)
					SCSC_TAG_ERR(MX_SAMPLER, "scsc_mx_service_close failed err: %d\n", r);

				spin_lock_irqsave(&mx_dbg_sampler.devs[i].spinlock, flags);
				mx_dbg_sampler.devs[i].filp = NULL;
				kfifo_free(&mx_dbg_sampler.devs[i].fifo);
				mx_dbg_sampler.devs[i].service = NULL;
				spin_unlock_irqrestore(&mx_dev->spinlock, flags);
			}
			device_destroy(mx_dbg_sampler.class_mx_dbg_sampler, mx_dbg_sampler.devs[i].cdev.dev);
			cdev_del(&mx_dbg_sampler.devs[i].cdev);
			memset(&mx_dbg_sampler.devs[i].cdev, 0, sizeof(struct cdev));
			mx_dbg_sampler.devs[i].mx = NULL;
			clear_bit(i, bitmap_dbg_sampler_minor);
		}
	class_destroy(mx_dbg_sampler.class_mx_dbg_sampler);
	unregister_chrdev_region(mx_dbg_sampler.device, SCSC_MX_DEBUG_INTERFACES);

	SCSC_TAG_INFO(MX_SAMPLER, "mx_dbg_sampler EXIT; version: %d.%d\n", VER_MAJOR, VER_MINOR);
}

module_init(mx_dbg_sampler_init);
module_exit(mx_dbg_sampler_unload);

MODULE_DESCRIPTION("Samsung debug sampler Driver");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
/*
 * MODULE_INFO(version, VER_MAJOR);
 * MODULE_INFO(build, SLSI_BUILD_STRING);
 * MODULE_INFO(release, SLSI_RELEASE_STRING);
 */
