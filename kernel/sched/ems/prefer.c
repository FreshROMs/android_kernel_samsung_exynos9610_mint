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
struct plist_head kpp_list[BOOSTGROUPS_COUNT];

static bool kpp_en;

bool ib_ems_initialized;
EXPORT_SYMBOL(ib_ems_initialized);

int kpp_status(int grp_idx)
{
	if (unlikely(!kpp_en))
		return 0;

	if (grp_idx >= BOOSTGROUPS_COUNT)
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

	if (grp_idx >= BOOSTGROUPS_COUNT)
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

	for (i = 0; i < BOOSTGROUPS_COUNT; i++)
		plist_head_init(&kpp_list[i]);

	kpp_en = 1;
}

struct prefer_group {
	int			        cgroup;

	unsigned int		light_threshold;
	unsigned int		heavy_threshold;

	struct cpumask		prefer_cpus;
	struct cpumask		light_prefer_cpus;
	struct cpumask		heavy_prefer_cpus;
};

static struct prefer_group *prefer_list;
static int prefer_list_count;

static struct prefer_group *find_prefer_group(int idx)
{
	int i;

	for (i = 0; i < prefer_list_count; i++)
		if (prefer_list[i].cgroup == idx)
			return &prefer_list[i];

	return NULL;
}

void prefer_cpu_get(struct tp_env *env, struct cpumask *mask)
{
	struct prefer_group *pp;
	int idx;

	cpumask_copy(mask, &env->p->cpus_allowed);

	if (!prefer_list)
		return;

	idx = env->cgroup_idx;
	if (idx <= 0)
		return;

	pp = find_prefer_group(idx);
	if (!pp)
		return;

	/* light task util threshold */
	if (env->task_util <= pp->light_threshold) {
		cpumask_and(mask, mask, &pp->light_prefer_cpus);
		return;
	}

	/* heavy task util threshold */
	if (env->task_util >= pp->heavy_threshold) {
		cpumask_and(mask, mask, &pp->heavy_prefer_cpus);
		return;
	}

	/* boosted task case */
	if (env->boosted || env->task_on_top) {
		cpumask_and(mask, mask, &pp->prefer_cpus);
		return;
	}
}

static void __init build_prefer_cpus(void)
{
	struct device_node *dn, *child;
	int index = 0;
	const char *buf;

	dn = of_find_node_by_name(NULL, "ems");
	dn = of_find_node_by_name(dn, "prefer-cpu");
	prefer_list_count = of_get_child_count(dn);

	prefer_list = kcalloc(prefer_list_count, sizeof(struct prefer_group), GFP_KERNEL);
	if (!prefer_list)
		return;

	for_each_child_of_node(dn, child) {
		if (index >= prefer_list_count)
			return;

		of_property_read_u32(child, "cgroup", &prefer_list[index].cgroup);

		if (of_property_read_string(child, "prefer-cpus", &buf))
			goto next;

		cpulist_parse(buf, &prefer_list[index].prefer_cpus);

		/* For light task processing */
		if (of_property_read_u32(child, "light-task-threshold", &prefer_list[index].light_threshold))
			prefer_list[index].light_threshold = 0;

		if (of_property_read_string(dn, "light-prefer-cpus", &buf))
			goto heavy;

		cpulist_parse(buf, &prefer_list[index].light_prefer_cpus);

heavy:
		/* For heavy task processing */
		if (of_property_read_u32(child, "heavy-task-threshold", &prefer_list[index].heavy_threshold))
			prefer_list[index].heavy_threshold = UINT_MAX;

		if (of_property_read_string(dn, "heavy-prefer-cpus", &buf))
			goto next;

		cpulist_parse(buf, &prefer_list[index].heavy_prefer_cpus);

next:
		index++;
	}
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
	for (i = 0; i < BOOSTGROUPS_COUNT; i++)
		ret += snprintf(buf + ret, 20, "%4s=%d\n", task_cgroup_simple_name[i], kpp_status(i));

	ret += snprintf(buf + ret, 10, "\n");

	return ret;
}

static struct kobj_attribute kpp_attr =
__ATTR(kernel_prefer_perf, 0444, show_kpp, NULL);

static ssize_t show_light_task_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, ret = 0;

	for (i = 0; i < prefer_list_count; i++)
		ret += snprintf(buf + ret, 40, "cgroup=%4s light-task-threshold=%d\n",
				task_cgroup_simple_name[prefer_list[i].cgroup],
				prefer_list[i].light_threshold);

	return ret;
}

static ssize_t store_light_task_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int i, cgroup, threshold, ret;

	ret = sscanf(buf, "%d %d", &cgroup, &threshold);
	if (ret != 2)
		return -EINVAL;

	if (cgroup < 0 || threshold < 0)
		return -EINVAL;

	for (i = 0; i < prefer_list_count; i++)
		if (prefer_list[i].cgroup == cgroup)
			prefer_list[i].light_threshold = threshold;

	return count;
}

static struct kobj_attribute light_task_threshold_attr =
__ATTR(light_task_threshold, 0644, show_light_task_threshold, store_light_task_threshold);

static ssize_t show_heavy_task_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, ret = 0;

	for (i = 0; i < prefer_list_count; i++)
		ret += snprintf(buf + ret, 40, "cgroup=%4s heavy-task-threshold=%d\n",
				task_cgroup_simple_name[prefer_list[i].cgroup],
				prefer_list[i].heavy_threshold);

	return ret;
}

static ssize_t store_heavy_task_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int i, cgroup, threshold, ret;

	ret = sscanf(buf, "%d %d", &cgroup, &threshold);
	if (ret != 2)
		return -EINVAL;

	if (cgroup < 0 || threshold < 0)
		return -EINVAL;

	for (i = 0; i < prefer_list_count; i++)
		if (prefer_list[i].cgroup == cgroup)
			prefer_list[i].heavy_threshold = threshold;

	return count;
}

static struct kobj_attribute heavy_task_threshold_attr =
__ATTR(heavy_task_threshold, 0644, show_heavy_task_threshold, store_heavy_task_threshold);

static struct attribute *prefer_attrs[] = {
	&kpp_attr.attr,
	&light_task_threshold_attr.attr,
	&heavy_task_threshold_attr.attr,
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

	build_prefer_cpus();

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

	ib_ems_initialized = true;
	return 0;
}
late_initcall(init_prefer);
