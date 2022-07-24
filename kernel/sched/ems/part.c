/*
 * Periodic Active Ratio Tracking
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/sched.h>
#include <linux/ems_service.h>
#include <linux/ems.h>
#include <trace/events/ems.h>

#include "../tune.h"
#include "../sched.h"
#include "ems.h"

/****************************************************************/
/*		Periodic Active Ratio Tracking			*/
/****************************************************************/
enum {
	PART_POLICY_RECENT = 0,
	PART_POLICY_MAX,
	PART_POLICY_MAX_RECENT_MAX,
	PART_POLICY_LAST,
	PART_POLICY_MAX_RECENT_LAST,
	PART_POLICY_MAX_RECENT_AVG,
	PART_POLICY_INVALID,
};

char *part_policy_name[] = {
	"RECENT",
	"MAX",
	"MAX_RECENT_MAX",
	"LAST",
	"MAX_RECENT_LAST",
	"MAX_RECENT_AVG",
	"INVALID"
};

static __read_mostly unsigned int part_policy_idx = PART_POLICY_MAX_RECENT_LAST;
static __read_mostly u64 period_size = 8 * NSEC_PER_MSEC;
static __read_mostly u64 period_hist_size = 10;
static __read_mostly int high_patten_thres = 700;
static __read_mostly int high_patten_stdev = 200;
static __read_mostly int low_patten_count = 3;
static __read_mostly int low_patten_thres = 1024;
static __read_mostly int low_patten_stdev = 200;

static __read_mostly u64 boost_interval = 16 * NSEC_PER_MSEC;

/********************************************************/
/*		  Helper funcition			*/
/********************************************************/

static inline int inc_hist_idx(int idx)
{
	return (idx + 1) % period_hist_size;
}

static inline void calc_active_ratio_hist(struct part *pa)
{
	int idx;
	int sum = 0, max = 0;
	int p_avg = 0, p_stdev = 0, p_count = 0;
	int patten, diff;

	/* Calculate basic statistics of P.A.R.T */
	for (idx = 0; idx < period_hist_size; idx++) {
		sum += pa->hist[idx];
		max = max(max, pa->hist[idx]);
	}

	pa->active_ratio_avg = sum / period_hist_size;
	pa->active_ratio_max = max;
	pa->active_ratio_est = 0;
	pa->active_ratio_stdev = 0;

	/* Calculate stdev for patten recognition */
	for (idx = 0; idx < period_hist_size; idx += 2) {
		patten = pa->hist[idx] + pa->hist[idx + 1];
		if (patten == 0)
			continue;

		p_avg += patten;
		p_count++;
	}

	if (p_count <= 1) {
		p_avg = 0;
		p_stdev = 0;
		goto out;
	}

	p_avg /= p_count;

	for (idx = 0; idx < period_hist_size; idx += 2) {
		patten = pa->hist[idx] + pa->hist[idx + 1];
		if (patten == 0)
			continue;

		diff = patten - p_avg;
		p_stdev += diff * diff;
	}

	p_stdev /= p_count - 1;
	p_stdev = int_sqrt(p_stdev);

out:
	pa->active_ratio_stdev = p_stdev;
	if (p_count >= low_patten_count &&
			p_avg <= low_patten_thres &&
			p_stdev <= low_patten_stdev)
		pa->active_ratio_est = p_avg / 2;

	trace_ems_cpu_active_ratio_patten(cpu_of(container_of(pa, struct rq, pa)),
			p_count, p_avg, p_stdev);
}

static void update_cpu_active_ratio_hist(struct part *pa, bool full, unsigned int count)
{
	/*
	 * Reflect recent active ratio in the history.
	 */
	pa->hist_idx = inc_hist_idx(pa->hist_idx);
	pa->hist[pa->hist_idx] = pa->active_ratio_recent;

	/*
	 * If count is positive, there are empty/full periods.
	 * These will be reflected in the history.
	 */
	while (count--) {
		pa->hist_idx = inc_hist_idx(pa->hist_idx);
		pa->hist[pa->hist_idx] = full ? SCHED_CAPACITY_SCALE : 0;
	}

	/*
	 * Calculate avg/max active ratio through entire history.
	 */
	calc_active_ratio_hist(pa);
}

static void
__update_cpu_active_ratio(int cpu, struct part *pa, u64 now, int boost)
{
	u64 elapsed = now - pa->period_start;
	unsigned int period_count = 0;

	if (boost) {
		pa->last_boost_time = now;
		return;
	}

	if (pa->last_boost_time &&
	    now > pa->last_boost_time + boost_interval)
		pa->last_boost_time = 0;

	if (pa->running) {
		/*
		 * If 'pa->running' is true, it means that the rq is active
		 * from last_update until now.
		 */
		u64 contributer, remainder;

		/*
		 * If now is in recent period, contributer is from last_updated to now.
		 * Otherwise, it is from last_updated to period_end
		 * and remaining active time will be reflected in the next step.
		 */
		contributer = min(now, pa->period_start + period_size);
		pa->active_sum += contributer - pa->last_updated;
		pa->active_ratio_recent =
			div64_u64(pa->active_sum << SCHED_CAPACITY_SHIFT, period_size);

		/*
		 * If now has passed recent period, calculate full periods and reflect they.
		 */
		period_count = div64_u64_rem(elapsed, period_size, &remainder);
		if (period_count) {
			update_cpu_active_ratio_hist(pa, true, period_count - 1);
			pa->active_sum = remainder;
			pa->active_ratio_recent =
				div64_u64(pa->active_sum << SCHED_CAPACITY_SHIFT, period_size);
		}
	} else {
		/*
		 * If 'pa->running' is false, it means that the rq is idle
		 * from last_update until now.
		 */

		/*
		 * If now has passed recent period, calculate empty periods and reflect they.
		 */
		period_count = div64_u64(elapsed, period_size);
		if (period_count) {
			update_cpu_active_ratio_hist(pa, false, period_count - 1);
			pa->active_ratio_recent = 0;
			pa->active_sum = 0;
		}
	}

	pa->period_start += period_size * period_count;
	pa->last_updated = now;
}

/********************************************************/
/*			External APIs			*/
/********************************************************/
void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type)
{
	struct part *pa = &rq->pa;
	int cpu = cpu_of(rq);
	u64 now = sched_clock_cpu(0);

	if (unlikely(pa->period_start == 0))
		return;

	switch (type) {
	/*
	 * 1) Enqueue
	 * This type is called when the rq is switched from idle to running.
	 * In this time, Update the active ratio for the idle interval
	 * and change the state to running.
	 */
	case EMS_PART_ENQUEUE:
		__update_cpu_active_ratio(cpu, pa, now, 0);

		if (rq->nr_running == 0) {
			pa->running = true;
			trace_ems_cpu_active_ratio(cpu, pa, "enqueue");
		}
		break;
	/*
	 * 2) Dequeue
	 * This type is called when the rq is switched from running to idle.
	 * In this time, Update the active ratio for the running interval
	 * and change the state to not-running.
	 */
	case EMS_PART_DEQUEUE:
		__update_cpu_active_ratio(cpu, pa, now, 0);

		if (rq->nr_running == 1) {
			pa->running = false;
			trace_ems_cpu_active_ratio(cpu, pa, "dequeue");
		}
		break;
	/*
	 * 3) Update
	 * This type is called to update the active ratio during rq is running.
	 */
	case EMS_PART_UPDATE:
		__update_cpu_active_ratio(cpu, pa, now, 0);
		trace_ems_cpu_active_ratio(cpu, pa, "update");
		break;

	case EMS_PART_WAKEUP_NEW:
		__update_cpu_active_ratio(cpu, pa, now, 1);
		trace_ems_cpu_active_ratio(cpu, pa, "new task");
		break;
	}
}

void part_cpu_active_ratio(unsigned long *util, unsigned long *max, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct part *pa = &rq->pa;
	unsigned long pelt_max = *max;
	unsigned long pelt_util = *util;
	int util_ratio = *util * SCHED_CAPACITY_SCALE / *max;
	int demand = 0;

	if (unlikely(pa->period_start == 0))
		return;

	if (pa->last_boost_time && util_ratio < pa->active_ratio_boost) {
		*max = SCHED_CAPACITY_SCALE;
		*util = pa->active_ratio_boost;
		return;
	}

	if (util_ratio > pa->active_ratio_limit)
		return;

	if (!pa->running &&
			(pa->active_ratio_avg < high_patten_thres ||
			 pa->active_ratio_stdev > high_patten_stdev)) {
		*util = 0;
		*max = SCHED_CAPACITY_SCALE;
		return;
	}

	update_cpu_active_ratio(rq, NULL, EMS_PART_UPDATE);

	switch (part_policy_idx) {
	case PART_POLICY_RECENT:
		demand = pa->active_ratio_recent;
		break;
	case PART_POLICY_MAX:
		demand = pa->active_ratio_max;
		break;
	case PART_POLICY_MAX_RECENT_MAX:
		demand = max(pa->active_ratio_recent, pa->active_ratio_max);
		break;
	case PART_POLICY_LAST:
		demand = pa->hist[pa->hist_idx];
		break;
	case PART_POLICY_MAX_RECENT_LAST:
		demand = max(pa->active_ratio_recent, pa->hist[pa->hist_idx]);
		break;
	case PART_POLICY_MAX_RECENT_AVG:
		demand = max(pa->active_ratio_recent, pa->active_ratio_avg);
		break;
	}

	*util = max(demand, pa->active_ratio_est);
	*util = min_t(unsigned long, *util, (unsigned long)pa->active_ratio_limit);
	*max = SCHED_CAPACITY_SCALE;

	if (util_ratio > *util) {
		*util = pelt_util;
		*max = pelt_max;
	}

	trace_ems_cpu_active_ratio_util_stat(cpu, *util, (unsigned long)util_ratio);
}

void set_part_period_start(struct rq *rq)
{
	struct part *pa = &rq->pa;
	u64 now;

	if (likely(pa->period_start))
		return;

	now = sched_clock_cpu(0);
	pa->period_start = now;
	pa->last_updated = now;
}

/********************************************************/
/*			  SYSFS				*/
/********************************************************/
static ssize_t show_part_policy(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%u. %s\n", part_policy_idx,
			part_policy_name[part_policy_idx]);
}

static ssize_t store_part_policy(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	long input;

	if (!sscanf(buf, "%ld", &input))
		return -EINVAL;

	if (input >= PART_POLICY_INVALID || input < 0)
		return -EINVAL;

	part_policy_idx = input;

	return count;
}

static ssize_t show_part_policy_list(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	for (i = 0; i < PART_POLICY_INVALID ; i++)
		len += sprintf(buf + len, "%u. %s\n", i, part_policy_name[i]);

	return len;
}

static ssize_t show_active_ratio_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct part *pa;
	int cpu, len = 0;

	for_each_possible_cpu(cpu) {
		pa = &cpu_rq(cpu)->pa;
		len += sprintf(buf + len, "cpu%d ratio:%3d\n",
				cpu, pa->active_ratio_limit);
	}

	return len;
}

static ssize_t store_active_ratio_limit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	struct part *pa;
	int cpu, ratio, i;

	if (sscanf(buf, "%d %d", &cpu, &ratio) != 2)
		return -EINVAL;

	/* Check cpu is possible */
	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -EINVAL;

	/* Check ratio isn't outrage */
	if (ratio < 0 || ratio > SCHED_CAPACITY_SCALE)
		return -EINVAL;

	for_each_cpu(i, cpu_coregroup_mask(cpu)) {
		pa = &cpu_rq(i)->pa;
		pa->active_ratio_limit = ratio;
	}

	return count;
}

static ssize_t show_active_ratio_boost(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct part *pa;
	int cpu, len = 0;

	for_each_possible_cpu(cpu) {
		pa = &cpu_rq(cpu)->pa;
		len += sprintf(buf + len, "cpu%d ratio:%3d\n",
				cpu, pa->active_ratio_boost);
	}

	return len;
}

static ssize_t store_active_ratio_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	struct part *pa;
	int cpu, ratio, i;

	if (sscanf(buf, "%d %d", &cpu, &ratio) != 2)
		return -EINVAL;

	/* Check cpu is possible */
	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -EINVAL;

	/* Check ratio isn't outrage */
	if (ratio < 0 || ratio > SCHED_CAPACITY_SCALE)
		return -EINVAL;

	for_each_cpu(i, cpu_coregroup_mask(cpu)) {
		pa = &cpu_rq(i)->pa;
		pa->active_ratio_boost = ratio;
	}

	return count;
}

#define show_node_function(_name)					\
static ssize_t show_##_name(struct kobject *kobj,			\
		struct kobj_attribute *attr, char *buf)			\
{									\
	return sprintf(buf, "%d\n", _name);				\
}

#define store_node_function(_name, _max)				\
static ssize_t store_##_name(struct kobject *kobj,			\
		struct kobj_attribute *attr, const char *buf,		\
		size_t count)						\
{									\
	unsigned int input;						\
									\
	if (!sscanf(buf, "%u", &input))					\
		return -EINVAL;						\
									\
	if (input > _max)						\
		return -EINVAL;						\
									\
	_name = input;							\
									\
	return count;							\
}

show_node_function(high_patten_thres);
store_node_function(high_patten_thres, SCHED_CAPACITY_SCALE);
show_node_function(high_patten_stdev);
store_node_function(high_patten_stdev, SCHED_CAPACITY_SCALE);
show_node_function(low_patten_count);
store_node_function(low_patten_count, (period_size / 2));
show_node_function(low_patten_thres);
store_node_function(low_patten_thres, (SCHED_CAPACITY_SCALE * 2));
show_node_function(low_patten_stdev);
store_node_function(low_patten_stdev, SCHED_CAPACITY_SCALE);

static struct kobj_attribute _policy =
__ATTR(policy, 0644, show_part_policy, store_part_policy);
static struct kobj_attribute _policy_list =
__ATTR(policy_list, 0444, show_part_policy_list, NULL);
static struct kobj_attribute _high_patten_thres =
__ATTR(high_patten_thres, 0644, show_high_patten_thres, store_high_patten_thres);
static struct kobj_attribute _high_patten_stdev =
__ATTR(high_patten_stdev, 0644, show_high_patten_stdev, store_high_patten_stdev);
static struct kobj_attribute _low_patten_count =
__ATTR(low_patten_count, 0644, show_low_patten_count, store_low_patten_count);
static struct kobj_attribute _low_patten_thres =
__ATTR(low_patten_thres, 0644, show_low_patten_thres, store_low_patten_thres);
static struct kobj_attribute _low_patten_stdev =
__ATTR(low_patten_stdev, 0644, show_low_patten_stdev, store_low_patten_stdev);
static struct kobj_attribute _active_ratio_limit =
__ATTR(active_ratio_limit, 0644, show_active_ratio_limit, store_active_ratio_limit);
static struct kobj_attribute _active_ratio_boost =
__ATTR(active_ratio_boost, 0644, show_active_ratio_boost, store_active_ratio_boost);

static struct attribute *attrs[] = {
	&_policy.attr,
	&_policy_list.attr,
	&_high_patten_thres.attr,
	&_high_patten_stdev.attr,
	&_low_patten_count.attr,
	&_low_patten_thres.attr,
	&_low_patten_stdev.attr,
	&_active_ratio_limit.attr,
	&_active_ratio_boost.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init init_part_sysfs(void)
{
	struct kobject *kobj;

	kobj = kobject_create_and_add("part", ems_kobj);
	if (!kobj)
		return -EINVAL;

	if (sysfs_create_group(kobj, &attr_group))
		return -EINVAL;

	return 0;
}
late_initcall(init_part_sysfs);

static int __init parse_part(void)
{
	struct device_node *dn, *coregroup;
	char name[15];
	int cpu, cnt = 0, limit = -1, boost = -1;

	dn = of_find_node_by_path("/cpus/ems/part");
	if (!dn)
		return 0;

	for_each_possible_cpu(cpu) {
		struct part *pa = &cpu_rq(cpu)->pa;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			goto skip_parse;

		limit = -1;
		boost = -1;

		snprintf(name, sizeof(name), "coregroup%d", cnt++);
		coregroup = of_get_child_by_name(dn, name);
		if (!coregroup)
			continue;

		of_property_read_s32(coregroup, "active-ratio-limit", &limit);
		of_property_read_s32(coregroup, "active-ratio-boost", &boost);

skip_parse:
		if (limit >= 0)
			pa->active_ratio_limit = SCHED_CAPACITY_SCALE * limit / 100;

		if (boost >= 0)
			pa->active_ratio_boost = SCHED_CAPACITY_SCALE * boost / 100;
	}

	return 0;
}
core_initcall(parse_part);

void __init init_part(void)
{
	int cpu, idx;

	for_each_possible_cpu(cpu) {
		struct part *pa = &cpu_rq(cpu)->pa;

		/* Set by default value */
		pa->running = false;
		pa->active_sum = 0;
		pa->active_ratio_recent = 0;
		pa->hist_idx = 0;
		for (idx = 0; idx < PART_HIST_SIZE_MAX; idx++)
			pa->hist[idx] = 0;

		pa->period_start = 0;
		pa->last_updated = 0;
	}
}
