/* drivers/gud/sec-os-ctrl/sec_os_ctrl.c
 *
 * Secure OS control driver for Samsung Exynos
 *
 * Copyright (c) 2014 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/mutex.h>

#define DEFAULT_LITTLE_CORE	1
#define DEFAULT_BIG_CORE	6
#define ASCII_TO_DIGIT_NUM(ascii)	(ascii - '0')

static unsigned int current_core, new_core;
static DEFINE_MUTEX(sec_os_ctrl_lock);

int nq_switch_core(uint32_t cpu);
uint32_t mc_active_core(void);

static struct bus_type sec_os_ctrl_subsys = {
	.name = "sec_os_ctrl",
	.dev_name = "sec_os_ctrl",
};

/* Migrate Secure OS */
static ssize_t migrate_os_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned int core_num = 0;

	/* Select only big or LITTLE */
	if ((buf[0] != 'L') && (buf[0] != 'b')) {
		pr_err("Invalid core number\n");
		return count;
	}

	/* Derive core number */
	core_num = ASCII_TO_DIGIT_NUM(buf[1]);
	if (buf[0] == 'L') {
		if ((buf[1] == 0xA) || (buf[1] == 0x0)) {	/* if LF(Line Feed, 0xA) or NULL(0x0) */
			new_core = DEFAULT_LITTLE_CORE;
		} else if (core_num < 4) {	/* From core 0 to core 3 */
			new_core = core_num;
		} else {
			pr_err("[LITTLE] Enter correct core number(0~3)\n");
			return count;
		}
	} else if (buf[0] == 'b') {
		if ((buf[1] == 0xA) || (buf[1] == 0x0)) {	/* if LF(Line Feed, 0xA) or NULL(0x0) */
			new_core = DEFAULT_BIG_CORE;
		} else if (core_num < 4) {	/* From core 0 to core 3 */
			new_core = core_num + 4;
		} else {
			pr_err("[big] Enter correct core number(0~3)\n");
			return count;
		}
	}
	pr_info("Secure OS will be migrated into core [%d]\n", new_core);

	if (mutex_lock_interruptible(&sec_os_ctrl_lock)) {
		pr_err("Fail to get lock\n");
		return count;
	}
	ret = nq_switch_core(new_core);
	mutex_unlock(&sec_os_ctrl_lock);
	if (ret != 0) {
		pr_err("Secure OS migration is failed!\n");
		pr_err("Return value = %d\n", ret);
		return count;
	}

	return count;
}

/* The current core where Secure OS is on */
static ssize_t current_core_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	current_core = mc_active_core();

	return sprintf(buf, "Secure OS is on core [%c%d]\n",
			(current_core < 4) ? 'L' : 'b', (current_core & 3));
}

static struct kobj_attribute migrate_os_attr =
	__ATTR(migrate_os, 0600, NULL, migrate_os_store);

static struct kobj_attribute current_core_attr =
	__ATTR(current_core, 0600, current_core_show, NULL);

static struct attribute *sec_os_ctrl_sysfs_attrs[] = {
	&migrate_os_attr.attr,
	&current_core_attr.attr,
	NULL,
};

static struct attribute_group sec_os_ctrl_sysfs_group = {
	.attrs = sec_os_ctrl_sysfs_attrs,
};

static const struct attribute_group *sec_os_ctrl_sysfs_groups[] = {
	&sec_os_ctrl_sysfs_group,
	NULL,
};

static int __init sec_os_ctrl_init(void)
{
	return subsys_system_register(&sec_os_ctrl_subsys, sec_os_ctrl_sysfs_groups);
}
late_initcall(sec_os_ctrl_init);
