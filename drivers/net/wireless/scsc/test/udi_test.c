/****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/sysfs.h>
#include <linux/poll.h>
#include <linux/cdev.h>

#include "dev.h"

#include "hip.h"
#include "log_clients.h"
#include "debug.h"
#include "unittest.h"
#include "udi.h"

#include "unifiio.h"

#define UDI_CHAR_DEVICE_NAME "s5e7570unittesthip"
#define UDI_CLASS_NAME "s5e7570unittesthip"

/**
 * Control character device for debug
 * ==================================
 */
#define NUM_CHAR_CLIENTS 1                   /* Number of client programmes on one node. */

#define MAX_MINOR (SLSI_UDI_MINOR_NODES - 1) /* Maximum node number. */
static dev_t        major_number;            /* Major number of device created by system. */
static struct class *class;                  /* Device class. */

struct slsi_test_cdev_client;

struct slsi_cdev {
	int                          minor;
	struct cdev                  cdev;
	struct slsi_test_cdev_client *client[NUM_CHAR_CLIENTS];

	struct slsi_test_dev         *uftestdev;
	struct device                *parent;
};

struct slsi_test_cdev_client {
	struct slsi_cdev    *ufcdev;
	int                 log_enabled;

	/* Flags set for special filtering of ma_packet data */
	u16                 ma_packet_filter_config;

	struct sk_buff_head log_list;
	wait_queue_head_t   log_wq;
};

/**
 * One minor node per phy. In normal driver mode, this may be one.
 * In unit test mode, this may be several.
 */
static struct slsi_cdev *uf_cdevs[SLSI_UDI_MINOR_NODES];

static int udi_log_event(struct slsi_log_client *log_client, struct sk_buff *skb, int dir);
static int send_signal_to_log_filter(struct slsi_log_client *log_client, struct sk_buff *skb, int dir);
static int send_signal_to_inverse_log_filter(struct slsi_log_client *log_client, struct sk_buff *skb, int dir);

static int slsi_test_cdev_open(struct inode *inode, struct file *file)
{
	struct slsi_cdev             *uf_cdev;
	struct slsi_test_cdev_client *client;
	int                          indx;
	int                          minor;

	minor = iminor(inode);
	if (minor > MAX_MINOR) {
		SLSI_ERR_NODEV("minor %d exceeds range\n", minor);
		return -EINVAL;
	}

	uf_cdev = uf_cdevs[minor];
	if (!uf_cdev) {
		SLSI_ERR_NODEV("no cdev instance for minor %d\n", minor);
		return -EINVAL;
	}

	if (!uf_cdev->uftestdev) {
		SLSI_ERR_NODEV("uftestdev not set\n");
		return -EINVAL;
	}

	for (indx = 0; indx < NUM_CHAR_CLIENTS; indx++)
		if (uf_cdev->client[indx] == NULL)
			break;
	if (indx >= NUM_CHAR_CLIENTS) {
		SLSI_ERR_NODEV("already opened\n");
		return -ENOTSUPP;
	}

	client = kmalloc(sizeof(*client), GFP_KERNEL);
	if (client == NULL)
		return -ENOMEM;
	memset(client, 0, sizeof(struct slsi_test_cdev_client));

	/* init other resource */
	skb_queue_head_init(&client->log_list);
	init_waitqueue_head(&client->log_wq);

	client->ufcdev = uf_cdev;
	uf_cdev->client[indx] = client;
	file->private_data = client;

	slsi_test_dev_attach(client->ufcdev->uftestdev);

	return 0;
}

static int slsi_test_cdev_release(struct inode *inode, struct file *filp)
{
	struct slsi_test_cdev_client *client = (void *)filp->private_data;
	struct slsi_cdev             *uf_cdev;
	int                          indx;
	int                          minor;

	minor = iminor(inode);
	if (minor > MAX_MINOR) {
		SLSI_ERR_NODEV("minor %d exceeds range\n", minor);
		return -EINVAL;
	}

	uf_cdev = uf_cdevs[minor];
	if (!uf_cdev) {
		SLSI_ERR_NODEV("no cdev instance for minor %d\n", minor);
		return -EINVAL;
	}

	if (client == NULL)
		return -EINVAL;

	if (!client->ufcdev) {
		SLSI_ERR_NODEV("ufcdev not set\n");
		return -EINVAL;
	}

	if (!client->ufcdev->uftestdev) {
		SLSI_ERR_NODEV("uftestdev not set\n");
		return -EINVAL;
	}

	for (indx = 0; indx < NUM_CHAR_CLIENTS; indx++)
		if (uf_cdev->client[indx] == client)
			break;
	if (indx >= NUM_CHAR_CLIENTS) {
		SLSI_ERR_NODEV("client not found in list\n");
		return -EINVAL;
	}

	if (waitqueue_active(&client->log_wq))
		wake_up_interruptible(&client->log_wq);

	if (client->log_enabled && client->ufcdev->uftestdev->sdev)
		slsi_log_client_unregister(client->ufcdev->uftestdev->sdev, client);

	slsi_test_dev_detach(client->ufcdev->uftestdev);

	skb_queue_purge(&client->log_list);

	/* free other resource */
	kfree(client);

	uf_cdev->client[indx] = NULL;

	return 0;
}

static ssize_t slsi_test_cdev_read(struct file *filp, char *p, size_t len, loff_t *poff)
{
	struct slsi_test_cdev_client *client = (void *)filp->private_data;
	int                          msglen;
	struct sk_buff               *skb;

	SLSI_UNUSED_PARAMETER(poff);

	if (client == NULL)
		return -EINVAL;

	if (!skb_queue_len(&client->log_list)) {
		if (filp->f_flags & O_NONBLOCK)
			return 0;

		/* wait until getting a signal */
		if (wait_event_interruptible(client->log_wq, skb_queue_len(&client->log_list)))
			return -ERESTARTSYS;
	}

	skb = skb_dequeue(&client->log_list);

	msglen = skb->len;
	if (msglen > (s32)len) {
		SLSI_WARN_NODEV("truncated read to %d actual msg len is %lu\n", msglen, (unsigned long int)len);
		msglen = len;
	}

	SLSI_DBG_HEX_NODEV(SLSI_TEST, skb->data, skb->len, "cdev read skb:%p skb->data:%p\n", skb, skb->data);
	if (copy_to_user(p, skb->data, msglen)) {
		SLSI_ERR_NODEV("Failed to copy UDI log to user\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	kfree_skb(skb);
	return msglen;
}

static ssize_t slsi_test_cdev_write(struct file *filp, const char *p, size_t len, loff_t *poff)
{
	struct slsi_test_cdev_client *client;
	struct slsi_test_dev         *uftestdev;
	struct sk_buff               *skb;
	struct slsi_skb_cb        *cb;
	u8                           *data;

	SLSI_UNUSED_PARAMETER(poff);

	client = (void *)filp->private_data;
	if (client == NULL) {
		SLSI_ERR_NODEV("filep private data not set\n");
		return -EINVAL;
	}

	if (!client->ufcdev) {
		SLSI_ERR_NODEV("ufcdev not set\n");
		return -EINVAL;
	}

	uftestdev = client->ufcdev->uftestdev;
	if (!uftestdev) {
		SLSI_ERR_NODEV("uftestdev not set\n");
		return -EINVAL;
	}

	skb = alloc_skb(len, GFP_KERNEL);
	if (!skb) {
		SLSI_WARN_NODEV("error allocating skb (len: %d)\n", len);
		return -ENOMEM;
	}

	data = skb_put(skb, len);
	if (copy_from_user(data, p, len)) {
		SLSI_ERR_NODEV("copy from user failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (skb->len < sizeof(struct fapi_signal_header)) {
		SLSI_ERR_NODEV("Data(%d) too short for a signal\n", skb->len);
		kfree_skb(skb);
		return -EINVAL;
	}

	SLSI_DBG_HEX_NODEV(SLSI_TEST, skb->data, skb->len, "cdev write skb:%p skb->data:%p\n", skb, skb->data);

	/* Intercept some requests */
	if (slsi_test_process_signal(uftestdev, skb))
		return len;

	{
		struct slsi_dev *sdev;

		sdev = uftestdev->sdev;
		if (!sdev) {
			SLSI_ERR_NODEV("sdev not set\n");
			kfree_skb(skb);
			return -EINVAL;
		}

		cb = slsi_skb_cb_init(skb);
		cb->sig_length = fapi_get_expected_size(skb);
		cb->data_length = skb->len;

		if (WARN_ON(slsi_hip_rx(sdev, skb))) {
			kfree_skb(skb);
			return -EINVAL;
		}
	}
	return len;
}

static long slsi_test_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct slsi_test_cdev_client *client = (void *)filp->private_data;
	struct slsi_test_dev         *uftestdev;
	struct slsi_dev              *sdev;
	long                         r = 0;
	int                          int_param;

	if (client == NULL || client->ufcdev == NULL)
		return -EINVAL;

	uftestdev = client->ufcdev->uftestdev;
	if (!uftestdev) {
		SLSI_ERR_NODEV("uftestdev not set\n");
		return -EINVAL;
	}

	sdev = uftestdev->sdev;
	if (!sdev) {
		SLSI_ERR_NODEV("sdev not set\n");
		return -EINVAL;
	}

	FUNC_ENTER_NODEV();

	slsi_wake_lock(&sdev->wlan_wl);

	switch (cmd) {
	case UNIFI_GET_UDI_ENABLE:
		int_param = client->log_enabled;
		put_user(int_param, (int *)arg);
		break;

	case UNIFI_SET_UDI_ENABLE:
		if (get_user(int_param, (int *)arg)) {
			r = -EFAULT;
			break;
		}

		if (int_param) {
			slsi_log_client_register(sdev, client, udi_log_event, NULL, 0, 0);
			client->log_enabled = 1;
		} else {
			slsi_log_client_unregister(sdev, client);
			client->log_enabled = 0;
		}

		break;
	case UNIFI_SET_UDI_LOG_MASK:
	{
		struct unifiio_filter_t filter;
		int                     i;

		/* to minimise load on data path, list is converted here to array indexed by signal number */
		if (copy_from_user(&filter, (void *)arg, sizeof(filter))) {
			SLSI_ERR(sdev, "UNIFI_SET_UDI_LOG_MASK: Failed to copy from userspace\n");
			r = -EFAULT;
			break;
		}
		if (filter.signal_ids_n) {
			char *signal_filter_index;
			int  max;
			int  min;
			int  size = filter.signal_ids_n * sizeof(filter.signal_ids[0]);
			u16  *signal_ids = kmalloc(size, GFP_KERNEL);

			if (!signal_ids) {
				r = -ENOMEM;
				break;
			}

			max = signal_ids[0];
			min = signal_ids[0];

			if (copy_from_user(signal_ids, filter.signal_ids, size)) {
				SLSI_ERR(sdev, "UNIFI_SET_UDI_LOG_MASK: Failed to copy filter from userspace\n");
				kfree(signal_ids);
				r = -EFAULT;
				break;
			}
			/* find maximum and minimum signal id in filter */
			for (i = 0; i < filter.signal_ids_n; i++) {
				if (signal_ids[i] & UDI_MA_UNITDATA_FILTER_ALLOW_MASK) {
					client->ma_packet_filter_config |= signal_ids[i];
					continue;
				}
				if (signal_ids[i] > max)
					max = signal_ids[i];
				else if (signal_ids[i] < min)
					min = signal_ids[i];
			}
			/* and create array only big enough to index the range of signal id specified */
			signal_filter_index = kmalloc(max - min + 1, GFP_KERNEL);
			if (signal_filter_index) {
				memset(signal_filter_index, 0, max - min + 1);
				for (i = 0; i < filter.signal_ids_n; i++) {
					if (signal_ids[i] & UDI_MA_UNITDATA_FILTER_ALLOW_MASK)
						continue;
					signal_filter_index[signal_ids[i] - min] = 1;
				}
				slsi_log_client_unregister(sdev, client);
				slsi_log_client_register(sdev, client,
							 filter.log_listed_flag ? send_signal_to_inverse_log_filter :
							 send_signal_to_log_filter, signal_filter_index, min, max);
			} else {
				r = -ENOMEM;
			}
			kfree(signal_ids);
		}
		break;
	}
	default:
		SLSI_WARN(sdev, "Operation (%d) not supported\n", cmd);
		r = -EINVAL;
	}

	slsi_wake_unlock(&sdev->wlan_wl);
	return r;
}

static unsigned int slsi_test_cdev_poll(struct file *filp, poll_table *wait)
{
	struct slsi_test_cdev_client *client = (void *)filp->private_data;
	unsigned int                 mask = 0;
	int                          ready;

	ready = skb_queue_len(&client->log_list);
	poll_wait(filp, &client->log_wq, wait);
	if (ready)
		mask |= POLLIN | POLLRDNORM;  /* readable */

	return mask;
}

/* we know for sure that there is a filter present in log_client->signal_filter if this function is called.
 * we know this because it is called only through a function pointer that is assigned
 * only when a filter is also set up in the log_client
 */
static int send_signal_to_log_filter(struct slsi_log_client *log_client, struct sk_buff *skb, int dir)
{
	int ret = 0;
	u16 signal_id =  fapi_get_u16(skb, id);

	if (signal_id > log_client->max_signal_id || signal_id < log_client->min_signal_id || !log_client->signal_filter[signal_id - log_client->min_signal_id])
		ret = udi_log_event(log_client, skb, dir);

	return ret;
}

static int send_signal_to_inverse_log_filter(struct slsi_log_client *log_client, struct sk_buff *skb, int dir)
{
	int ret = 0;
	u16 signal_id =  fapi_get_u16(skb, id);

	if (signal_id <= log_client->max_signal_id && signal_id >= log_client->min_signal_id && log_client->signal_filter[signal_id - log_client->min_signal_id])
		ret = udi_log_event(log_client, skb, dir);
	return ret;
}

static int udi_log_event(struct slsi_log_client *log_client, struct sk_buff *skb, int dir)
{
	struct slsi_test_cdev_client *client = log_client->log_client_ctx;
	struct udi_msg_t             msg;
	struct udi_msg_t             *msg_skb;

	if (WARN_ON(client == NULL))
		return -EINVAL;
	if (WARN_ON(skb == NULL))
		return -EINVAL;
	if (WARN_ON(skb->len == 0))
		return -EINVAL;

	skb = skb_copy_expand(skb, sizeof(msg), 0, GFP_ATOMIC);
	if (WARN_ON(!skb))
		return -ENOMEM;

	/* Intercept some requests */
	if (slsi_test_process_signal(client->ufcdev->uftestdev, skb))
		return -ECANCELED;

	if (WARN_ON(skb_headroom(skb) < sizeof(msg)))
		return -ENOMEM;

	msg.length = sizeof(msg) + skb->len;
	msg.timestamp = jiffies_to_msecs(jiffies);
	msg.direction = dir;
	msg.signal_length = fapi_get_siglen(skb);

	msg_skb = (struct udi_msg_t *)skb_push(skb, sizeof(msg));
	*msg_skb = msg;

	skb_queue_tail(&client->log_list, skb);

	/* Wake any waiting user process */
	wake_up_interruptible(&client->log_wq);

	return 0;
}

#define UF_DEVICE_CREATE(_class, _parent, _devno, _priv, _fmt, _args)       \
	device_create(_class, _parent, _devno, _priv, _fmt, _args)

static const struct file_operations slsi_test_cdev_fops = {
	.owner          = THIS_MODULE,
	.open           = slsi_test_cdev_open,
	.release        = slsi_test_cdev_release,
	.read           = slsi_test_cdev_read,
	.write          = slsi_test_cdev_write,
	.unlocked_ioctl = slsi_test_cdev_ioctl,
	.compat_ioctl   = slsi_test_cdev_ioctl,
	.poll           = slsi_test_cdev_poll,
};

#define UF_DEVICE_CREATE(_class, _parent, _devno, _priv, _fmt, _args)       \
	device_create(_class, _parent, _devno, _priv, _fmt, _args)

static int slsi_get_minor(void)
{
	int minor;

	for (minor = 0; minor < SLSI_UDI_MINOR_NODES; minor++)
		if (uf_cdevs[minor] == 0)
			return minor;
	return -1;
}

static int slsi_test_cdev_create(struct slsi_test_dev *uftestdev, struct device *parent)
{
	dev_t            devno;
	int              ret;
	struct slsi_cdev *pdev;
	int              minor;

	FUNC_ENTER_NODEV();
	minor = slsi_get_minor();
	if (minor < 0) {
		SLSI_ERR_NODEV("no minor numbers available\n");
		return -ENOMEM;
	}

	pdev = kmalloc(sizeof(*pdev), GFP_KERNEL);
	if (pdev == NULL)
		return -ENOMEM;
	memset(pdev, 0, sizeof(*pdev));

	cdev_init(&pdev->cdev, &slsi_test_cdev_fops);
	pdev->cdev.owner = THIS_MODULE;
	pdev->minor = minor;
	devno = MKDEV(MAJOR(major_number), minor);
	ret = cdev_add(&pdev->cdev, devno, 1);
	if (ret) {
		SLSI_ERR_NODEV("cdev_add failed with %d for minor %d\n", ret, minor);
		kfree(pdev);
		return ret;
	}

	pdev->uftestdev = uftestdev;
	pdev->parent = parent;
	if (!UF_DEVICE_CREATE(class, pdev->parent, devno, pdev, UDI_CHAR_DEVICE_NAME "%d", minor)) {
		cdev_del(&pdev->cdev);
		kfree(pdev);
		return -EINVAL;
	}
	uftestdev->uf_cdev = (void *)pdev;
	uftestdev->device_minor_number = minor;
	uf_cdevs[minor] = pdev;
	return 0;
}

static void slsi_test_cdev_destroy(struct slsi_test_dev *uftestdev)
{
	struct slsi_cdev *pdev = (struct slsi_cdev *)uftestdev->uf_cdev;

	FUNC_ENTER_NODEV();
	if (!pdev)
		return;
	device_destroy(class, pdev->cdev.dev);
	cdev_del(&pdev->cdev);
	uftestdev->uf_cdev = 0;
	uf_cdevs[pdev->minor] = 0;
	kfree(pdev);
}

static int udi_initialised;

int slsi_test_udi_init(void)
{
	int ret;

	memset(uf_cdevs, 0, sizeof(uf_cdevs));

	/* Allocate two device numbers for each device. */
	ret = alloc_chrdev_region(&major_number, 0, SLSI_UDI_MINOR_NODES, UDI_CLASS_NAME);
	if (ret) {
		SLSI_ERR_NODEV("Failed to add alloc dev numbers: %d\n", ret);
		return ret;
	}

	/* Create a UniFi class */
	class = class_create(THIS_MODULE, UDI_CLASS_NAME);
	if (IS_ERR(class)) {
		SLSI_ERR_NODEV("Failed to create UniFi class\n");
		unregister_chrdev_region(major_number, SLSI_UDI_MINOR_NODES);
		major_number = 0;
		return -EINVAL;
	}

	udi_initialised = 1;

	return 0;
}

int slsi_test_udi_deinit(void)
{
	if (!udi_initialised)
		return -1;
	class_destroy(class);
	unregister_chrdev_region(major_number, SLSI_UDI_MINOR_NODES);
	udi_initialised = 0;
	return 0;
}

int slsi_test_udi_node_init(struct slsi_test_dev *uftestdev, struct device *parent)
{
	FUNC_ENTER_NODEV();
	if (!udi_initialised)
		return -1;
	return slsi_test_cdev_create(uftestdev, parent);
}

int slsi_test_udi_node_reregister(struct slsi_test_dev *uftestdev)
{
	struct slsi_cdev *pdev = uftestdev->uf_cdev;
	int              indx;

	if (uftestdev->sdev)
		for (indx = 0; indx < NUM_CHAR_CLIENTS; indx++)
			if (pdev->client[indx] != NULL && pdev->client[indx]->log_enabled)
				slsi_log_client_register(uftestdev->sdev, pdev->client[indx], udi_log_event, NULL, 0, 0);

	return 0;
}

int slsi_test_udi_node_deinit(struct slsi_test_dev *uftestdev)
{
	FUNC_ENTER_NODEV();
	if (!udi_initialised)
		return -1;
	slsi_test_cdev_destroy(uftestdev);
	return 0;
}
