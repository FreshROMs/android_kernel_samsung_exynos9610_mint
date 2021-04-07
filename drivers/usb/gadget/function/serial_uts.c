/*
 * serial_uts.c -- USB modem serial driver for UTS
 *
 * Copyright 2008 (C) Samsung Electronics
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/usb/cdc.h>

extern int uts_notify(void *dev, u16 state);


static wait_queue_head_t uts_modem_wait_q;

static unsigned int read_state;
static unsigned int control_line_state;

static void *uts_data;

static int uts_modem_open(struct inode *inode, struct file *file)
{
	read_state = 0;

	return 0;
}

static int uts_modem_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t uts_modem_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int ret = 0;

	if (file->f_flags & O_NONBLOCK)
		return -EAGAIN;

	ret = wait_event_interruptible(uts_modem_wait_q, read_state);
	if (ret)
		return ret;

	if (copy_to_user(buf, &control_line_state, sizeof(u32)))
		return -EFAULT;

	read_state = 0;

	return sizeof(u32);
}

static unsigned int uts_modem_poll(struct file *file, poll_table *wait)
{
	int ret;
	poll_wait(file, &uts_modem_wait_q, wait);

	ret = (read_state ? (POLLIN | POLLRDNORM) : 0);

	return ret;
}

void uts_notify_control_line_state(u32 value)
{
	control_line_state = value;

	read_state = 1;

	wake_up_interruptible(&uts_modem_wait_q);
}
EXPORT_SYMBOL(uts_notify_control_line_state);


#define GS_CDC_UTS_NOTIFY_SERIAL_STATE	_IOW('S', 1, int)
#define GS_IOC_UTS_NOTIFY_DTR_TEST		_IOW('S', 3, int)

static long
uts_modem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	printk(KERN_INFO "uts_modem_ioctl: cmd=0x%x, arg=%lu\n", cmd, arg);

	/* handle ioctls */
	switch (cmd) {
	case GS_CDC_UTS_NOTIFY_SERIAL_STATE:
		uts_notify(uts_data, __constant_cpu_to_le16(arg));
		break;

	case GS_IOC_UTS_NOTIFY_DTR_TEST:
		{
			printk(KERN_ALERT"DUN2 : DTR %d\n", (int)arg);
			uts_notify_control_line_state((int)arg);
			break;
		}

	default:
		printk(KERN_INFO "uts_modem_ioctl: Unknown ioctl cmd(0x%x).\n",
				cmd);
		return -ENOIOCTLCMD;
	}
	return 0;
}

#ifdef CONFIG_COMPAT  // add for 64bit kernel & 32bit platform
static long uts_modem_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	ret = uts_modem_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations modem_fops = {
	.owner		= THIS_MODULE,
	.open		= uts_modem_open,
	.release	= uts_modem_close,
	.read		= uts_modem_read,
	.poll		= uts_modem_poll,
	.llseek		= no_llseek,
	.unlocked_ioctl	= uts_modem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =   uts_modem_compat_ioctl,
#endif	
};

static struct miscdevice uts_modem_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name	= "dun2",
	.fops	= &modem_fops,
};

int uts_modem_register(void *data)
{
	if (data == NULL) {
		printk(KERN_INFO "DUN2 register failed. data is null.\n");
		return -1;
	}

	uts_data = data;

	printk(KERN_INFO "DUN2 is registerd\n");

	return 0;
}
EXPORT_SYMBOL(uts_modem_register);

int uts_modem_misc_register(void)
{
	int ret;
	ret = misc_register(&uts_modem_device);
	if (ret) {
		printk(KERN_ERR "DUN2 register is failed, ret = %d\n", ret);
		return ret;
	}

	init_waitqueue_head(&uts_modem_wait_q);
	return ret;
}
EXPORT_SYMBOL(uts_modem_misc_register);

void uts_modem_unregister(void)
{
	uts_data = NULL;

	uts_notify_control_line_state(0);

	printk(KERN_INFO "DUN2 is unregisterd\n");
}
EXPORT_SYMBOL(uts_modem_unregister);
