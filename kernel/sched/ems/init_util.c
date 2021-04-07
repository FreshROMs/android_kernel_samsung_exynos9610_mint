/*
 * task util initialization
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Lakkyung Jung <lakkyung.jung@samsung.com>
 */

#include <linux/sched.h>

#include "ems.h"
#include "../sched.h"

enum {
	TYPE_BASE_CFS_RQ_UTIL = 0,
	TYPE_BASE_INHERIT_PARENT_UTIL,
	TYPE_MAX_NUM,
};

static unsigned long init_util_type = TYPE_BASE_CFS_RQ_UTIL;
static unsigned long init_util_ratio = 25;			/* 25% */

static void base_cfs_rq_util(struct sched_entity *se)
{
	struct cfs_rq *cfs_rq = se->cfs_rq;
	struct sched_avg *sa = &se->avg;
	int cpu = cpu_of(cfs_rq->rq);
	unsigned long cap_org = capacity_orig_of(cpu);
	long cap = (long)(cap_org - cfs_rq->avg.util_avg);

	if (cap > 0) {
		if (cfs_rq->avg.util_avg != 0) {
			sa->util_avg  = cfs_rq->avg.util_avg * se->load.weight;
			sa->util_avg /= (cfs_rq->avg.load_avg + 1);
			sa->util_avg = sa->util_avg << 1;

			if (sa->util_avg > cap)
				sa->util_avg = cap;
		} else {
			sa->util_avg = cap_org * init_util_ratio / 100;
		}
		/*
		 * If we wish to restore tuning via setting initial util,
		 * this is where we should do it.
		 */
		sa->util_sum = (u32)(sa->util_avg * LOAD_AVG_MAX);
	}
}

static void base_inherit_parent_util(struct sched_entity *se)
{
	struct sched_avg *sa = &se->avg;
	struct task_struct *p = current;

	sa->util_avg = p->se.avg.util_avg;
	sa->util_sum = p->se.avg.util_sum;
}

void exynos_init_entity_util_avg(struct sched_entity *se)
{
	int type = init_util_type;

	switch(type) {
	case TYPE_BASE_CFS_RQ_UTIL:
		base_cfs_rq_util(se);
		break;
	case TYPE_BASE_INHERIT_PARENT_UTIL:
		base_inherit_parent_util(se);
		break;
	default:
		pr_info("%s: Not support initial util type %ld\n",
				__func__, init_util_type);
	}
}

static ssize_t show_initial_util_type(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return snprintf(buf, 10, "%ld\n", init_util_type);
}

static ssize_t store_initial_util_type(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf,
                size_t count)
{
        long input;

        if (!sscanf(buf, "%ld", &input))
                return -EINVAL;

        input = input < 0 ? 0 : input;
        input = input >= TYPE_MAX_NUM ? TYPE_MAX_NUM - 1 : input;

        init_util_type = input;

        return count;
}

static ssize_t show_initial_util_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return snprintf(buf, 10, "%ld\n", init_util_ratio);
}

static ssize_t store_initial_util_ratio(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf,
                size_t count)
{
        long input;

        if (!sscanf(buf, "%ld", &input))
                return -EINVAL;

        init_util_ratio = !!input;

        return count;
}

static struct kobj_attribute initial_util_type =
__ATTR(initial_util_type, 0644, show_initial_util_type, store_initial_util_type);

static struct kobj_attribute initial_util_ratio =
__ATTR(initial_util_ratio, 0644, show_initial_util_ratio, store_initial_util_ratio);

static struct attribute *attrs[] = {
	&initial_util_type.attr,
	&initial_util_ratio.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init init_util_sysfs(void)
{
	struct kobject *kobj;

	kobj = kobject_create_and_add("init_util", ems_kobj);
	if (!kobj)
		return -EINVAL;

	if (sysfs_create_group(kobj, &attr_group))
		return -EINVAL;

	return 0;
}
late_initcall(init_util_sysfs);
