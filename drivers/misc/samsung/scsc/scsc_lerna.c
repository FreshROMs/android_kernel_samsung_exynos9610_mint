/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>  /* Required for copy_to_user. */
#include <linux/completion.h>
#include <linux/atomic.h>

#include <scsc/scsc_logring.h>

#include "mxman.h"
#include "mxmgmt_transport_format.h"  /* Required for MXMGR_MESSAGE_PAYLOAD_SIZE. */

#define DEVICE_NAME "lerna"
#define DEVICE_CLASS "scsc_config"
#define DEVICE_COUNT (1)

static const void *scsc_lerna_pending;
#define SCSC_LERNA_WAIT_TIMEOUT (2000)
static DECLARE_COMPLETION(scsc_lerna_wait);

/**
 * MSMGR_MESSAGE_PAYLOAD_SIZE is not a nice power of 2, so use sizeof(msmgr_message)
 * just for something more aesthetically pleasing.
 */
#define SCSC_LERNA_BUFFER_SIZE (sizeof(struct mxmgr_message))
static uint8_t scsc_lerna_request_buffer[SCSC_LERNA_BUFFER_SIZE];
static uint8_t scsc_lerna_response_buffer[SCSC_LERNA_BUFFER_SIZE];

static dev_t scsc_lerna_device_id;
static struct class *scsc_lerna_class_p;
static struct device *scsc_lerna_device_p;
static struct cdev scsc_lerna_cdev;

static int scsc_lerna_chardev_open(struct inode *inodep, struct file *filep);
static ssize_t scsc_lerna_chardev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
static ssize_t scsc_lerna_chardev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);
static int scsc_lerna_chardev_release(struct inode *inodep, struct file *filep);

static struct file_operations scsc_lerna_fops = {
	.open = scsc_lerna_chardev_open,
	.read = scsc_lerna_chardev_read,
	.write = scsc_lerna_chardev_write,
	.release = scsc_lerna_chardev_release,
};

static atomic_t scsc_lerna_atomic;

struct scsc_lerna_cmd_header {
	uint8_t magic_number;    /* Set to 0x08. */
	uint8_t cid;             /* Action command identifier. */
	uint16_t payload_length; /* Payload length. 0 for value query. */
	uint16_t psid;           /* PSID to query. */
	uint8_t row_index;       /* Row index, or 0 for non-table querying. */
	uint8_t group_index;     /* Group index, or 0 for default (group not assigned). */
};

static int scsc_lerna_chardev_open(struct inode *inodep, struct file *filep)
{
	(void)inodep;
	(void)filep;

	if (atomic_inc_return(&scsc_lerna_atomic) > 1) {
		atomic_dec(&scsc_lerna_atomic);
		/* Someone already has this open. Denied. */
		SCSC_TAG_DEBUG(LERNA, "character device busy, try again later.\n");
		return -EBUSY;
	}

	SCSC_TAG_DEBUG(LERNA, "opening lerna character device.\n");
	return 0;
}

static ssize_t scsc_lerna_chardev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	const struct scsc_lerna_cmd_header *header;
	unsigned long wait_result;
	ssize_t read_count;
	int error_count;

	(void)filep;
	(void)offset;

	wait_result = wait_for_completion_timeout(&scsc_lerna_wait, msecs_to_jiffies(SCSC_LERNA_WAIT_TIMEOUT));
	if (wait_result == 0) {
		SCSC_TAG_ERR(LERNA, "read timeout; firmware not responding, or read without write.\n");
		return -ETIMEDOUT;
	}

	if (!scsc_lerna_pending) {
		/* Pointer is NULL, indicating that a reply hasn't been sent from firmware. */
		SCSC_TAG_DEBUG(LERNA, "pending reply is null.\n");
		return -ENOMSG;
	}

	header = (const struct scsc_lerna_cmd_header *)(scsc_lerna_pending);
	read_count = sizeof(struct scsc_lerna_cmd_header) + header->payload_length;

	/* Make sure there's enough space to read out the buffer. */
	if (len < read_count) {
		SCSC_TAG_ERR(LERNA, "insufficient buffer space supplied for read.\n");
		return -ENOBUFS;
	}

	error_count = copy_to_user(buffer, scsc_lerna_pending, read_count);

	if (error_count) {
		SCSC_TAG_ERR(LERNA, "could not read from lerna character device.\n");
		return -EFAULT;
	}

	SCSC_TAG_DEBUG(LERNA, "read buffer of size: %lu\n", read_count);
	/* Value was read out, and is no longer considered valid. Need to write before another read. */
	scsc_lerna_pending = NULL;
	return read_count;
}

static ssize_t scsc_lerna_chardev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	SCSC_TAG_DEBUG(LERNA, "writing buffer of size: %lu\n", len);
	/* At a minimum, any request (read or write) must include a command header. */
	if (len >= sizeof(struct scsc_lerna_cmd_header)) {
		/* Header at least fits, but maybe a write value wants more... */
		if (len <= SCSC_LERNA_BUFFER_SIZE) {
			if (copy_from_user(scsc_lerna_request_buffer, buffer, len)) {
				SCSC_TAG_ERR(LERNA, "copy_from_user failed.\n");
				return -EFAULT;
			}
			mxman_lerna_send(NULL, scsc_lerna_request_buffer, len);
		} else {
			/* Message size too long, don't write anything. */
			return -EMSGSIZE;
		}
	} else {
		return -EBADR;
	}

	return len;
}

static int scsc_lerna_chardev_release(struct inode *inodep, struct file *filep)
{
	(void)inodep;
	(void)filep;
	if (atomic_read(&scsc_lerna_atomic) == 0) {
		SCSC_TAG_ALERT(LERNA, "character device release without open.\n");
	} else {
		/* Done with the character device, release the lock on it. */
		atomic_dec(&scsc_lerna_atomic);
	}

	SCSC_TAG_DEBUG(LERNA, "lerna character device closed.\n");
	return 0;
}


int scsc_lerna_init(void)
{
	int result;

	/**
	 * Reset important globals to some kind of sane value. This should be done
	 * whenever the module is loaded explicitly to be sure global values haven't
	 * been previously trashed.
	 */
	scsc_lerna_device_id = 0;
	scsc_lerna_class_p = NULL;
	scsc_lerna_device_p = NULL;

	/* Make sure to initialise the atomic used to lock char device access. */
	atomic_set(&scsc_lerna_atomic, 0);

	/**
	 * Allocate device id(s) for the character device. Use alloc_register_chrdev
	 * because this is the new way of doing things, and it will dynamically allocate
	 * a major number. Returns non-zero on failure.
	 */
	result = alloc_chrdev_region(&scsc_lerna_device_id, 0, DEVICE_COUNT, DEVICE_NAME);
	if (result) {
		/* Failure to register char dev, auto fail to initialise module. */
		SCSC_TAG_ALERT(LERNA, "lerna failed to register character device.\n");
		return result;
	}

	scsc_lerna_class_p = class_create(THIS_MODULE, DEVICE_CLASS);
	if (IS_ERR(scsc_lerna_class_p)) {
		/* Could not create class, failure, remember to unregister device id(s). */
		unregister_chrdev_region(scsc_lerna_device_id, DEVICE_COUNT);
		SCSC_TAG_ALERT(LERNA, "lerna failed to create character class.\n");
		return PTR_ERR(scsc_lerna_class_p);
	}

	scsc_lerna_device_p = device_create(scsc_lerna_class_p, NULL, scsc_lerna_device_id, NULL, DEVICE_NAME);
	if (IS_ERR(scsc_lerna_device_p)) {
		class_destroy(scsc_lerna_class_p);
		unregister_chrdev_region(scsc_lerna_device_id, DEVICE_COUNT);
		SCSC_TAG_ALERT(LERNA, "lerna failed to create character device.\n");
		return PTR_ERR(scsc_lerna_device_p);
	}

	/**
	 * At this point, the device is registered, along with class definition. The character device
	 * itself can now be initialised to provide the kernel with callback information for various
	 * actions taken on the device.
	 */
	cdev_init(&scsc_lerna_cdev, &scsc_lerna_fops);
	scsc_lerna_cdev.owner = THIS_MODULE;

	result = cdev_add(&scsc_lerna_cdev, scsc_lerna_device_id, DEVICE_COUNT);
	if (result) {
		/* Failure to add character device to file system. */
		cdev_del(&scsc_lerna_cdev);
		class_destroy(scsc_lerna_class_p);
		unregister_chrdev_region(scsc_lerna_device_id, DEVICE_COUNT);
		SCSC_TAG_ALERT(LERNA, "lerna failed to add character device.\n");
		return result;
	}
	/* At this point, the cdev is live and can be used. */

	SCSC_TAG_INFO(LERNA, "lerna intialisation complete.\n");
	return 0;  /* 0 for module loaded, non-zero for module load failure. */
}

void scsc_lerna_deinit(void)
{
	/* Character device needs deleting. */
	cdev_del(&scsc_lerna_cdev);

	/* Destroy device. */
	device_destroy(scsc_lerna_class_p, scsc_lerna_device_id);

	/* Unregister the device class. Not sure if this means that a register earlier is required. */
	class_unregister(scsc_lerna_class_p);

	/* Destroy created class. Be careful of the order this is called in. */
	class_destroy(scsc_lerna_class_p);

	/**
	 * Don't forget to unregister device id(s). Major number is dynamically allocated,
	 * so the base id is remembered and passed along to the unregister here.
	 */
	unregister_chrdev_region(scsc_lerna_device_id, DEVICE_COUNT);

	SCSC_TAG_INFO(LERNA, "lerna shutdown complete.\n");
}

void scsc_lerna_response(const void *message)
{
	/**
	 * Buffer the response from the firmware so that future messages from firmware
	 * don't overwrite this accidentally. This means async messages are allowed while
	 * waiting for the character device read from userspace, without impacting lerna's
	 * request/response communications.
	 */
	const struct scsc_lerna_cmd_header *header;
	ssize_t read_count;

	if (message != NULL) {
		header = (const struct scsc_lerna_cmd_header *)(message);
		read_count = sizeof(struct scsc_lerna_cmd_header) + header->payload_length;

		if (read_count <= SCSC_LERNA_BUFFER_SIZE) {
			memcpy(scsc_lerna_response_buffer, message, read_count);
			scsc_lerna_pending = scsc_lerna_response_buffer;
		} else {
			SCSC_TAG_DEBUG(LERNA, "readout too large for response buffering.\n");
			/* No response possible, let the userspace application deal with it. */
			scsc_lerna_pending = NULL;
		}
	}
	complete(&scsc_lerna_wait);
}
