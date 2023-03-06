/*
 * Services for Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/ems.h>
#include <linux/ems_service.h>
#include <trace/events/ems.h>

#include "../sched.h"
#include "../tune.h"
#include "ems.h"

/**********************************************************************
 *                        Kernel Prefer Perf                          *
 **********************************************************************/
struct plist_head kpp_list[CGROUP_COUNT];

static bool kpp_en;

int kpp_status(int grp_idx)
{
	if (unlikely(!kpp_en))
		return 0;

	if (grp_idx >= CGROUP_COUNT)
		return -EINVAL;

	if (plist_head_empty(&kpp_list[grp_idx]))
		return 0;

	return plist_last(&kpp_list[grp_idx])->prio;
}

static DEFINE_SPINLOCK(kpp_lock);

void kpp_request(int grp_idx, struct kpp *req, int value)
{
	unsigned long flags;

	if (unlikely(!kpp_en))
		return;

	if (grp_idx >= CGROUP_COUNT)
		return;

	if (req->node.prio == value)
		return;

	spin_lock_irqsave(&kpp_lock, flags);

	/*
	 * If the request already added to the list updates the value, remove
	 * the request from the list and add it again.
	 */
	if (req->active)
		plist_del(&req->node, &kpp_list[req->grp_idx]);
	else
		req->active = 1;

	plist_node_init(&req->node, value);
	plist_add(&req->node, &kpp_list[grp_idx]);
	req->grp_idx = grp_idx;

	spin_unlock_irqrestore(&kpp_lock, flags);
}

static void __init init_kpp(void)
{
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		plist_head_init(&kpp_list[i]);

	kpp_en = 1;
}

static char *task_cgroup_simple_name[] = {
    "root",
    "fg",
    "bg",
    "ta",
    "rt",
    "sy",
    "syb",
    "n-h",
    "c-d",
};

static ssize_t show_kpp(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, ret = 0;

	/* shows the prefer_perf value of all schedtune groups */
	for (i = 0; i < CGROUP_COUNT; i++)
		ret += snprintf(buf + ret, 20, "%4s=%d\n", task_cgroup_simple_name[i], kpp_status(i));

	ret += snprintf(buf + ret, 10, "\n");

	return ret;
}

static struct kobj_attribute kpp_attr =
__ATTR(kernel_prefer_perf, 0444, show_kpp, NULL);

static struct attribute *prefer_attrs[] = {
	&kpp_attr.attr,
	NULL,
};

static const struct attribute_group prefer_group = {
	.attrs = prefer_attrs,
};

static struct kobject *prefer_kobj;

static int __init init_prefer(void)
{
	int ret;

	init_kpp();

	prefer_kobj = kobject_create_and_add("prefer", ems_kobj);
	if (!prefer_kobj) {
		pr_err("Fail to create ems prefer kboject\n");
		return -EINVAL;
	}

	ret = sysfs_create_group(prefer_kobj, &prefer_group);
	if (ret) {
		pr_err("Fail to create ems prefer group\n");
		return ret;
	}

	return 0;
}
late_initcall(init_prefer);
