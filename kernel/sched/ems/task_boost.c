/*
 * Per-ID task boosting
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/sched.h>
#include <linux/kobject.h>
#include <linux/ems.h>

#include <trace/events/ems.h>

#include "ems.h"

static int task_boost;

int ems_task_boost(void)
{
	return task_boost;
}

static ssize_t show_task_boost(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", task_boost);
}

static ssize_t store_task_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int boost;

	if (sscanf(buf, "%d", &boost) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (boost < 0 || boost >= 32768)
		return -EINVAL;

	task_boost = boost;

	return count;
}

static struct kobj_attribute task_boost_attr =
__ATTR(task_boost, 0644, show_task_boost, store_task_boost);

static int __init init_tboost_sysfs(void)
{
	int ret;

	ret = sysfs_create_file(ems_kobj, &task_boost_attr.attr);
	if (ret)
		pr_err("%s: failed to create sysfs file\n", __func__);

	return 0;
}
late_initcall(init_tboost_sysfs);
