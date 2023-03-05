/*
 * Global task boosting
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/sched.h>
#include <linux/kobject.h>
#include <linux/ems.h>

#include "ems.h"
#include "../sched.h"
#include "../tune.h"

#include <trace/events/ems.h>

/*
 * Global boost manages each boosting request as a list so that it can support
 * boosting at the device driver level as well as user level. The list management
 * algorithm uses the priority-list same as pm_qos.
 */
static struct plist_head gb_list = PLIST_HEAD_INIT(gb_list);

static int gb_qos_value(void)
{
	if (plist_head_empty(&gb_list))
		return 0;

	return plist_last(&gb_list)->prio;
}

static DEFINE_SPINLOCK(gb_lock);

void gb_qos_update_request(struct gb_qos_request *req, u32 new_value)
{
	unsigned long flags;

	/* ignore if the value does not change */
	if (req->node.prio == new_value)
		return;

	spin_lock_irqsave(&gb_lock, flags);

	/*
	 * If the request already added to the list updates the value, remove
	 * the request from the list and add it again.
	 */
	if (req->active)
		plist_del(&req->node, &gb_list);
	else
		req->active = 1;

	plist_node_init(&req->node, new_value);
	plist_add(&req->node, &gb_list);

	trace_ems_global_boost(req->name, new_value);

	spin_unlock_irqrestore(&gb_lock, flags);
}

/* user level request, it is only set via sysfs */
static struct gb_qos_request gb_req_user =
{
	.name = "gb_req_user",
};

static ssize_t show_global_boost(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct gb_qos_request *req;
	int ret = 0;

	/* show all requests as well as user level */
	plist_for_each_entry(req, &gb_list, node)
		ret += snprintf(buf + ret, 30, "%s : %d\n",
				req->name, req->node.prio);

	return ret;
}

static ssize_t store_global_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	unsigned int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	gb_qos_update_request(&gb_req_user, input);

	return count;
}

static int task_boost;

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

/******************************************************************************
 * global task boost                                                          *
 ******************************************************************************/
static int global_task_boost[CGROUP_COUNT] = {0, };

u64 ems_global_task_boost_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	struct schedtune *st = css_st(css);
	int group_idx = st->idx;

	if (group_idx >= CGROUP_COUNT)
		return (u64) global_task_boost[CGROUP_ROOT];

	return (u64) global_task_boost[group_idx];
}

int ems_global_task_boost_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled) {
	struct schedtune *st = css_st(css);
	int group_idx;

	if (enabled < 0 || enabled > 1)
		return -EINVAL;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT) {
		global_task_boost[CGROUP_ROOT] = enabled;
		return 0;
	}

	global_task_boost[group_idx] = enabled;
	return 0;
}

static struct kobj_attribute global_boost_attr =
__ATTR(global_boost, 0644, show_global_boost, store_global_boost);
static struct kobj_attribute task_boost_attr =
__ATTR(task_boost, 0644, show_task_boost, store_task_boost);

static int __init init_boost_sysfs(void)
{
	int ret;

	ret = sysfs_create_file(ems_kobj, &global_boost_attr.attr);
	if (ret)
		pr_err("%s: failed to create global boost sysfs file\n", __func__);

	ret = sysfs_create_file(ems_kobj, &task_boost_attr.attr);
	if (ret)
		pr_err("%s: failed to create task boost sysfs file\n", __func__);

	return 0;
}
late_initcall(init_boost_sysfs);

/*
 * Returns the biggest value in the global boost list. In the current policy,
 * a value greater than 0 is unconditionally boosting. The size of the value
 * is meaningless.
 */
int ems_boot_boost(void)
{
	u64 now = ktime_to_us(ktime_get());

	/* init boost duration = 60s */
	if (now < 60 * USEC_PER_SEC)
		return EMS_INIT_BOOST;

	/* booting boost duration = 120s */
	if (now < 120 * USEC_PER_SEC)
		return EMS_BOOT_BOOST;

	return 0;
}

int ems_global_boost(void)
{
	return gb_qos_value() > 0;
}

int ems_global_task_boost(int cgroup_idx)
{
	if (!ems_global_boost())
		return 0;

	/* enable for all groups if enabled on root */
	if (global_task_boost[CGROUP_ROOT])
		return 1;

	return global_task_boost[cgroup_idx];
}

int ems_task_boost(void)
{
	return task_boost;
}
