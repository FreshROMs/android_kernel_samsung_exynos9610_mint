/*
 * /drivers/rtc/power-on-alarm.c
 *
 *  Copyright (C) 2015 Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/time.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/alarmtimer.h>
#include <linux/ioctl.h>

struct alarm_timespec {
	char alarm[14];
};

#define ANDROID_ALARM_BASE_CMD(cmd)         (cmd & ~(_IOC(0, 0, 0xf0, 0)))
#define ANDROID_ALARM_SET_ALARM_BOOT	    _IOW('a', 7, struct timespec)
#ifdef CONFIG_COMPAT
#define ANDROID_ALARM_SET_ALARM_BOOT_COMPAT	_IOW('a', 7, \
							struct compat_timespec)
#endif
#define ANDROID_ALARM_SET_ALARM_BOOT_NEW    _IOW('a', 7, struct alarm_timespec)

static long power_on_alarm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rv;
	char bootalarm_data[14];

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_SET_ALARM_BOOT:
	case ANDROID_ALARM_SET_ALARM_BOOT_NEW:
		if (copy_from_user(bootalarm_data, (void __user *)arg, 14)) {
			rv = -EFAULT;
			printk("boot alarm param copy fail\n");
			return rv;
		}
		printk("power_alarm_ioctl:%ld\n",arg);
		rv = alarm_set_alarm_boot(bootalarm_data);
		break;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long power_alarm_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rv;
	char bootalarm_data[14];

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_SET_ALARM_BOOT_COMPAT:
	case ANDROID_ALARM_SET_ALARM_BOOT_NEW:
		if (copy_from_user(bootalarm_data, (void __user *)arg, 14)) {
			rv = -EFAULT;
			printk("boot alarm param copy fail\n");
			return rv;
		}
		printk("power_alarm_compat_ioctl:%ld\n",arg);
		rv = alarm_set_alarm_boot(bootalarm_data);
		break;
	}

	return 0;
}
#endif

static int power_on_alarm_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int power_on_alarm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations power_on_alarm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = power_on_alarm_ioctl,
	.open = power_on_alarm_open,
	.release = power_on_alarm_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = power_alarm_compat_ioctl,
#endif
};

static struct miscdevice power_on_alarm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "power_on_alarm",
	.fops = &power_on_alarm_fops,
};

static int __init power_on_alarm_dev_init(void)
{
	int err;

	err = misc_register(&power_on_alarm_device);
	if (err)
		return err;

	return 0;
}

static void  __exit power_on_alarm_dev_exit(void)
{
	misc_deregister(&power_on_alarm_device);
}

module_init(power_on_alarm_dev_init);
module_exit(power_on_alarm_dev_exit);
