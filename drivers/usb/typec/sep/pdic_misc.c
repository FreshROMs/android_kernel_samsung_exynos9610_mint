/*
 *
 * Copyright (C) 2017-2018 Samsung Electronics
 * Author: Wookwang Lee <wookwang.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/usb/typec/pdic_core.h>

static struct ccic_misc_dev *c_dev;

#define MAX_BUF 255
#define NODE_OF_MISC "ccic_misc"
#define CCIC_IOCTL_UVDM _IOWR('C', 0, struct uvdm_data)
#ifdef CONFIG_COMPAT
#define CCIC_IOCTL_UVDM_32 _IOWR('C', 0, struct uvdm_data_32)
#endif

void set_endian(char *src, char *dest, int size)
{
	int i, j;
	int loop;
	int dest_pos;
	int src_pos;

	loop = size / SEC_UVDM_ALIGN;
	loop += (((size % SEC_UVDM_ALIGN) > 0) ? 1:0);

	for (i = 0 ; i < loop ; i++)
		for (j = 0 ; j < SEC_UVDM_ALIGN ; j++) {
			src_pos = SEC_UVDM_ALIGN * i + j;
			dest_pos = SEC_UVDM_ALIGN * i + SEC_UVDM_ALIGN - j - 1;
			dest[dest_pos] = src[src_pos];
		}
}

int get_checksum(char *data, int start_addr, int size)
{
	int checksum = 0;
	int i;

	for (i = 0; i < size; i++)
		checksum += data[start_addr+i];

	return checksum;
}

int set_uvdmset_count(int size)
{
	int ret = 0;

	if (size <= SEC_UVDM_MAXDATA_FIRST)
		ret = 1;
	else {
		ret = ((size-SEC_UVDM_MAXDATA_FIRST) / SEC_UVDM_MAXDATA_NORMAL);
		if (((size-SEC_UVDM_MAXDATA_FIRST) %
			SEC_UVDM_MAXDATA_NORMAL) == 0)
			ret += 1;
		else
			ret += 2;
	}
	return ret;
}

void set_msg_header(void *data, int msg_type, int obj_num)
{
	msg_header_type *msg_hdr;
	uint8_t *SendMSG = (uint8_t *)data;

	msg_hdr = (msg_header_type *)&SendMSG[0];
	msg_hdr->msg_type = msg_type;
	msg_hdr->num_data_objs = obj_num;
	msg_hdr->port_data_role = USBPD_DFP;
}

void set_uvdm_header(void *data, int vid, int vdm_type)
{
	uvdm_header *uvdm_hdr;
	uint8_t *SendMSG = (uint8_t *)data;

	uvdm_hdr = (uvdm_header *)&SendMSG[0];
	uvdm_hdr->vendor_id = SAMSUNG_VENDOR_ID;
	uvdm_hdr->vdm_type = vdm_type;
	uvdm_hdr->vendor_defined = SEC_UVDM_UNSTRUCTURED_VDM;
	uvdm_hdr->BITS.VDM_command = 4; /* from s2mm005 concept */
}

void set_sec_uvdm_header(void *data, int pid, bool data_type, int cmd_type,
		bool dir, int total_set_num, uint8_t received_data)
{
	s_uvdm_header *SEC_UVDM_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_UVDM_HEADER = (s_uvdm_header *)&SendMSG[4];

	SEC_UVDM_HEADER->pid = pid;
	SEC_UVDM_HEADER->data_type = data_type;
	SEC_UVDM_HEADER->cmd_type = cmd_type;
	SEC_UVDM_HEADER->direction = dir;
	SEC_UVDM_HEADER->total_set_num = total_set_num;
	SEC_UVDM_HEADER->data = received_data;

	pr_info("%s : pid=0x%x, data_type=%d, cmd_type=%d, dir=%d\n", __func__,
		SEC_UVDM_HEADER->pid, SEC_UVDM_HEADER->data_type,
		SEC_UVDM_HEADER->cmd_type, SEC_UVDM_HEADER->direction);
}

int get_data_size(int first_set, int remained_data_size)
{
	int ret = 0;

	if (first_set)
		ret = (remained_data_size <= SEC_UVDM_MAXDATA_FIRST) ?
			remained_data_size : SEC_UVDM_MAXDATA_FIRST;
	else
		ret = (remained_data_size <= SEC_UVDM_MAXDATA_NORMAL) ?
			remained_data_size : SEC_UVDM_MAXDATA_NORMAL;

	return ret;
}

void set_sec_uvdm_tx_header(void *data,
		int first_set, int cur_set, int total_size, int remained_size)
{
	s_tx_header *SEC_TX_HAEDER;
	uint8_t *SendMSG = (uint8_t *)data;

	if (first_set)
		SEC_TX_HAEDER = (s_tx_header *)&SendMSG[8];
	else
		SEC_TX_HAEDER = (s_tx_header *)&SendMSG[4];
	SEC_TX_HAEDER->cur_size = get_data_size(first_set, remained_size);
	SEC_TX_HAEDER->total_size = total_size;
	SEC_TX_HAEDER->order_cur_set = cur_set;
}

void set_sec_uvdm_tx_tailer(void *data)
{
	s_tx_tailer *SEC_TX_TAILER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_TX_TAILER = (s_tx_tailer *)&SendMSG[24];
	SEC_TX_TAILER->checksum =
		get_checksum(SendMSG, 4, SEC_UVDM_CHECKSUM_COUNT);
}

void set_sec_uvdm_rx_header(void *data, int cur_num, int cur_set, int ack)
{
	s_rx_header *SEC_RX_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_RX_HEADER = (s_rx_header *)&SendMSG[4];
	SEC_RX_HEADER->order_cur_set = cur_num;
	SEC_RX_HEADER->rcv_data_size = cur_set;
	SEC_RX_HEADER->result_value = ack;
}

struct ccic_misc_dev *get_ccic_misc_dev(void)
{
	if (!c_dev)
		return NULL;
	return c_dev;
}
EXPORT_SYMBOL(get_ccic_misc_dev);

static inline int _lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1)
		return 0;
	else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void _unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static int ccic_misc_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	pr_info("%s + open success\n", __func__);
	if (!c_dev) {
		pr_err("%s - error : c_dev is NULL\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	if (_lock(&c_dev->open_excl)) {
		pr_err("%s - error : device busy\n", __func__);
		ret = -EBUSY;
		goto err;
	}

	/* check if there is some connection */
	if (!c_dev->uvdm_ready()) {
		_unlock(&c_dev->open_excl);
		pr_err("%s - error : uvdm is not ready\n", __func__);
		ret = -EBUSY;
		goto err;
	}

	pr_info("%s - open success\n", __func__);

	return 0;
err:
	return ret;
}

static int ccic_misc_close(struct inode *inode, struct file *file)
{
	if (c_dev)
		_unlock(&c_dev->open_excl);
	c_dev->uvdm_close();
	pr_info("%s - close success\n", __func__);
	return 0;
}

static int send_uvdm_message(void *data, int size)
{
	int ret;

	ret = c_dev->uvdm_write(data, size);
	pr_info("%s - size : %d, ret : %d\n", __func__, size, ret);
	return ret;
}

static int receive_uvdm_message(void *data, int size)
{
	int ret;

	ret = c_dev->uvdm_read(data);
	pr_info("%s - size : %d, ret : %d\n", __func__, size, ret);
	return ret;
}

static long
ccic_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void *buf = NULL;

	if (_lock(&c_dev->ioctl_excl)) {
		pr_err("%s - error : ioctl busy - cmd : %d\n", __func__, cmd);
		ret = -EBUSY;
		goto err2;
	}

	if (!c_dev->uvdm_ready()) {
		pr_err("%s - error : uvdm is not ready\n", __func__);
		ret = -EACCES;
		goto err1;
	}

	switch (cmd) {
	case CCIC_IOCTL_UVDM:
		pr_info("%s - CCIC_IOCTL_UVDM cmd\n", __func__);
		if (copy_from_user(&c_dev->u_data,
				(void __user *) arg, sizeof(struct uvdm_data))) {
			ret = -EIO;
			pr_err("%s - copy_from_user error\n", __func__);
			goto err1;
		}

		buf = kzalloc(MAX_BUF, GFP_KERNEL);
		if (!buf) {
			ret = -EINVAL;
			pr_err("%s - kzalloc error\n", __func__);
			goto err1;
		}

		if (c_dev->u_data.size > MAX_BUF) {
			ret = -ENOMEM;
			pr_err("%s - user data size is %d error\n",
					__func__, c_dev->u_data.size);
			goto err;
		}

		if (c_dev->u_data.dir == DIR_OUT) {
			if (copy_from_user(buf, c_dev->u_data.pData, c_dev->u_data.size)) {
				ret = -EIO;
				pr_err("%s - copy_from_user error\n", __func__);
				goto err;
			}
			ret = send_uvdm_message(buf, c_dev->u_data.size);
			if (ret < 0) {
				pr_err("%s - send_uvdm_message error\n", __func__);
				ret = -EINVAL;
				goto err;
			}
		} else {
			ret = receive_uvdm_message(buf, c_dev->u_data.size);
			if (ret < 0) {
				pr_err("%s - receive_uvdm_message error\n", __func__);
				ret = -EINVAL;
				goto err;
			}
			if (copy_to_user((void __user *)c_dev->u_data.pData,
					buf, ret)) {
				ret = -EIO;
				pr_err("%s - copy_to_user error\n", __func__);
				goto err;
			}
		}
		break;
#ifdef CONFIG_COMPAT
	case CCIC_IOCTL_UVDM_32:
		pr_info("%s - CCIC_IOCTL_UVDM_32 cmd\n", __func__);
		if (copy_from_user(&c_dev->u_data_32, compat_ptr(arg),
				sizeof(struct uvdm_data_32))) {
			ret = -EIO;
			pr_err("%s - copy_from_user error\n", __func__);
			goto err1;
		}

		buf = kzalloc(MAX_BUF, GFP_KERNEL);
		if (!buf) {
			ret = -EINVAL;
			pr_err("%s - kzalloc error\n", __func__);
			goto err1;
		}

		if (c_dev->u_data_32.size > MAX_BUF) {
			ret = -ENOMEM;
			pr_err("%s - user data size is %d error\n", __func__, c_dev->u_data_32.size);
			goto err;
		}

		if (c_dev->u_data_32.dir == DIR_OUT) {
			if (copy_from_user(buf, compat_ptr(c_dev->u_data_32.pData), c_dev->u_data_32.size)) {
				ret = -EIO;
				pr_err("%s - copy_from_user error\n", __func__);
				goto err;
			}
			ret = send_uvdm_message(buf, c_dev->u_data_32.size);
			if (ret < 0) {
				pr_err("%s - send_uvdm_message error\n", __func__);
				ret = -EINVAL;
				goto err;
			}
		} else {
			ret = receive_uvdm_message(buf, c_dev->u_data_32.size);
			if (ret < 0) {
				pr_err("%s - receive_uvdm_message error\n", __func__);
				ret = -EINVAL;
				goto err;
			}
			if (copy_to_user(compat_ptr(c_dev->u_data_32.pData),
						buf, ret)) {
				ret = -EIO;
				pr_err("%s - copy_to_user error\n", __func__);
				goto err;
			}
		}
		break;
#endif
	default:
		pr_err("%s - unknown ioctl cmd : %d\n", __func__, cmd);
		ret = -ENOIOCTLCMD;
		goto err;
	}
err:
	kfree(buf);
err1:
	_unlock(&c_dev->ioctl_excl);
err2:
	return ret;
}

#ifdef CONFIG_COMPAT
static long
ccic_misc_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	pr_info("%s - cmd : %d\n", __func__, cmd);
	ret = ccic_misc_ioctl(file, cmd, (unsigned long)compat_ptr(arg));

	return ret;
}
#endif

static const struct file_operations ccic_misc_fops = {
	.owner		= THIS_MODULE,
	.open		= ccic_misc_open,
	.release	= ccic_misc_close,
	.llseek		= no_llseek,
	.unlocked_ioctl = ccic_misc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ccic_misc_compat_ioctl,
#endif
};

static struct miscdevice ccic_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name	= NODE_OF_MISC,
	.fops	= &ccic_misc_fops,
};

int ccic_misc_init(pccic_data_t pccic_data)
{
	int ret = 0;

	ret = misc_register(&ccic_misc_device);
	if (ret) {
		pr_err("%s - return error : %d\n", __func__, ret);
		goto err;
	}

	c_dev = kzalloc(sizeof(struct ccic_misc_dev), GFP_KERNEL);
	if (!c_dev) {
		ret = -ENOMEM;
		pr_err("%s - kzalloc failed : %d\n", __func__, ret);
		goto err1;
	}
	atomic_set(&c_dev->open_excl, 0);
	atomic_set(&c_dev->ioctl_excl, 0);

	if (pccic_data)
		pccic_data->misc_dev = c_dev;

	pr_info("%s - register success\n", __func__);
	return 0;
err1:
	misc_deregister(&ccic_misc_device);
err:
	return ret;
}
EXPORT_SYMBOL(ccic_misc_init);

void ccic_misc_exit(void)
{
	pr_info("%s() called\n", __func__);
	if (!c_dev)
		return;
	kfree(c_dev);
	misc_deregister(&ccic_misc_device);
}
EXPORT_SYMBOL(ccic_misc_exit);
