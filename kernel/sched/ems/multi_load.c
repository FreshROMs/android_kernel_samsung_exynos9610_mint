/*
 * Multi-purpose Load tracker
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/sched.h>
#include <linux/ems.h>

#include "../sched.h"
#include "../tune.h"

#include "ems.h"

#include <trace/events/ems.h>

/******************************************************************************
 *                           MULTI LOAD for TASK                              *
 ******************************************************************************/
/*
 * ml_task_util - task util
 *
 * Task utilization. The calculation is the same as the task util of cfs.
 */
unsigned long ml_task_util(struct task_struct *p)
{
	if (rt_task(p))
		return READ_ONCE(p->rt.avg.util_avg);
	else
		return READ_ONCE(p->se.avg.util_avg);
}

#define UTIL_AVG_UNCHANGED 0x1

/*
 * ml_task_util_est - task util with util-est
 *
 * Task utilization with util-est, The calculation is the same as
 * task_util_est of cfs.
 */
static unsigned long _ml_task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return (max(ue.ewma, ue.enqueued) | UTIL_AVG_UNCHANGED);
}

unsigned long ml_task_util_est(struct task_struct *p)
{
	return max(ml_task_util(p), _ml_task_util_est(p));
}

/*
 * ml_uclamp_task_util - clamped task util-est with uclamp
 *
 * Task utilization with util-est clamped with uclamp
 */
inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
#ifdef CONFIG_UCLAMP_TASK
    return clamp(ml_task_util_est(p),
             uclamp_eff_value(p, UCLAMP_MIN),
             uclamp_eff_value(p, UCLAMP_MAX));
#else
    return ml_task_util_est(p);
#endif
}

/******************************************************************************
 *                            MULTI LOAD for CPU                              *
 ******************************************************************************/
/*
 * ml_cpu_util - cpu utilization
 */
unsigned long ml_cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	struct rt_rq *rt_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	rt_rq = &cpu_rq(cpu)->rt;

	util = READ_ONCE(cfs_rq->avg.util_avg);
	util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	/* account rt task usage */
	util += rt_rq->avg.util_avg;

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/*
 * ml_cpu_util_without - cpu utilization without waking task
 */
unsigned long ml_cpu_util_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	struct rt_rq *rt_rq = &cpu_rq(cpu)->rt;
	unsigned int util = READ_ONCE(cfs_rq->avg.util_avg);

	/* account rt task usage */
	util += rt_rq->avg.util_avg;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return util;

	/* Discount task's util from CPU's util */
	lsub_positive(&util, ml_task_util(p));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

/*
 * ml_cpu_util_with - cpu utilization with waking task
 */
unsigned long ml_cpu_util_with(struct task_struct *p, int cpu)
{
	unsigned long util;

	util = ml_cpu_util_without(cpu, p);
	util += max(ml_task_util_est(p), (unsigned long)1);

	return min(util, capacity_orig_of(cpu));
}

/*
 * ml_cpu_load_avg - cpu load_avg
 */
unsigned long ml_cpu_load_avg(int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	return READ_ONCE(cfs_rq->avg.load_avg);
}

/*
 * ml_runnable_load_avg - cpu runnable_load_avg
 */
unsigned long ml_runnable_load_avg(int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	return READ_ONCE(cfs_rq->runnable_load_avg);
}

/******************************************************************************
 *                     New task utilization init                              *
 ******************************************************************************/
static int ntu_ratio[CGROUP_COUNT] = {5, };

/* An entity is a task if it doesn't "own" a runqueue */
#define entity_is_task(se)	(!se->my_q)

void ntu_apply(struct sched_entity *se)
{
	struct cfs_rq *cfs_rq = se->cfs_rq;
	struct sched_avg *sa = &se->avg;
	int cpu = cpu_of(cfs_rq->rq);
	unsigned long cap_org = capacity_orig_of(cpu);
	long cap = (long)(cap_org - cfs_rq->avg.util_avg) / 2;
	int grp_idx = entity_is_task(se) ? schedtune_task_group_idx(task_of(se)): CGROUP_ROOT;
	int ratio = ntu_ratio[grp_idx];

	if (cap > 0) {
		if (cfs_rq->avg.util_avg != 0) {
			sa->util_avg  = cfs_rq->avg.util_avg * se->load.weight;
			sa->util_avg /= (cfs_rq->avg.load_avg + 1);

			if (sa->util_avg > cap)
				sa->util_avg = cap;
		} else {
			sa->util_avg = cap_org * ratio / 100;
		}

		/*
		 * If we wish to restore tuning via setting initial util,
		 * this is where we should do it.
		 */
		sa->util_sum = (u32)(sa->util_avg * LOAD_AVG_MAX);
	}
}

u64 ems_ntu_ratio_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	struct schedtune *st = css_st(css);
	int group_idx;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT)
		return (u64) ntu_ratio[CGROUP_ROOT];

	return (u64) ntu_ratio[group_idx];
}

int ems_ntu_ratio_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 ratio) {
	struct schedtune *st = css_st(css);
	int group_idx;

	if (ratio < 0 || ratio > 100)
		return -EINVAL;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT) {
		ntu_ratio[CGROUP_ROOT] = ratio;
		return 0;
	}

	ntu_ratio[group_idx] = ratio;
	return 0;
}

/******************************************************************************
 *                             Multi load tracking                            *
 ******************************************************************************/
enum {
	MLT_POLICY_RECENT = 0,
	MLT_POLICY_MAX,
	MLT_POLICY_MAX_RECENT_MAX,
	MLT_POLICY_LAST,
	MLT_POLICY_MAX_RECENT_LAST,
	MLT_POLICY_MAX_RECENT_AVG,
	MLT_POLICY_INVALID,
};

char *mlt_policy_name[] = {
	"RECENT",
	"MAX",
	"MAX_RECENT_MAX",
	"LAST",
	"MAX_RECENT_LAST",
	"MAX_RECENT_AVG",
	"INVALID"
};

static __read_mostly unsigned int mlt_policy_idx = MLT_POLICY_MAX_RECENT_LAST;
static __read_mostly int high_patten_thres = 700;
static __read_mostly int high_patten_stdev = 200;
static __read_mostly int low_patten_count = 3;
static __read_mostly int low_patten_thres = 1024;
static __read_mostly int low_patten_stdev = 200;

static __read_mostly u64 boost_interval = 16 * NSEC_PER_MSEC;

/********************************************************/
/*		  Helper funcition			*/
/********************************************************/

static inline void mlt_move_next_period(struct mlt *mlt)
{
	mlt->cur_period = (mlt->cur_period + 1) % MLT_PERIOD_COUNT;
}

static inline
void calc_active_ratio_hist(struct mlt *mlt)
{
	int idx;
	int sum = 0, max = 0;
	int p_avg = 0, p_stdev = 0, p_count = 0;
	int patten, diff;

	/* Calculate basic statistics of P.A.R.T */
	for (idx = 0; idx < MLT_PERIOD_COUNT; idx++) {
		sum += mlt->period[idx];
		max = max(max, mlt->period[idx]);
	}

	mlt->active_ratio_avg = sum / MLT_PERIOD_COUNT;
	mlt->active_ratio_max = max;
	mlt->active_ratio_est = 0;
	mlt->active_ratio_stdev = 0;

	/* Calculate stdev for patten recognition */
	for (idx = 0; idx < MLT_PERIOD_COUNT; idx += 2) {
		patten = mlt->period[idx] + mlt->period[idx + 1];
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

	for (idx = 0; idx < MLT_PERIOD_COUNT; idx += 2) {
		patten = mlt->period[idx] + mlt->period[idx + 1];
		if (patten == 0)
			continue;

		diff = patten - p_avg;
		p_stdev += diff * diff;
	}

	p_stdev /= p_count - 1;
	p_stdev = int_sqrt(p_stdev);

out:
	mlt->active_ratio_stdev = p_stdev;
	if (p_count >= low_patten_count &&
			p_avg <= low_patten_thres &&
			p_stdev <= low_patten_stdev)
		mlt->active_ratio_est = p_avg / 2;

	trace_ems_cpu_active_ratio_patten(cpu_of(container_of(mlt, struct rq, mlt)),
			p_count, p_avg, p_stdev);
}

static void __mlt_update_hist(struct mlt *mlt)
{
	/*
	 * Reflect recent active ratio in the history.
	 */
	mlt_move_next_period(mlt);
	mlt->period[mlt->cur_period] = mlt->active_ratio_recent;
}

static void mlt_update_full_hist(struct mlt *mlt, unsigned int count)
{

	/*
	 * Reflect recent active ratio in the history.
	 */
	__mlt_update_hist(mlt);

	/*
	 * If count is positive, there are empty/full periods.
	 * These will be reflected in the history.
	 */
	while (count--) {
		mlt_move_next_period(mlt);
		mlt->period[mlt->cur_period] = SCHED_CAPACITY_SCALE;
	}

	/*
	 * Calculate avg/max active ratio through entire history.
	 */
	calc_active_ratio_hist(mlt);
}

static void mlt_update_hist(struct mlt *mlt, unsigned int count)
{

	/*
	 * Reflect recent active ratio in the history.
	 */
	__mlt_update_hist(mlt);

	/*
	 * If count is positive, there are empty/full periods.
	 * These will be reflected in the history.
	 */
	while (count--) {
		mlt_move_next_period(mlt);
		mlt->period[mlt->cur_period] = 0;
	}

	/*
	 * Calculate avg/max active ratio through entire history.
	 */
	calc_active_ratio_hist(mlt);
}

static void
mlt_update(int cpu, struct mlt *mlt, u64 now, int boost)
{
	u64 elapsed = now - mlt->period_start;
	unsigned int period_count = 0;

	if (boost) {
		mlt->last_boost_time = now;
		return;
	}

	if (mlt->last_boost_time &&
	    now > mlt->last_boost_time + boost_interval)
		mlt->last_boost_time = 0;

	if (mlt->running) {
		/*
		 * If 'mlt->running' is true, it means that the rq is active
		 * from last_update until now.
		 */
		u64 contributer, remainder;

		/*
		 * If now is in recent period, contributer is from last_updated to now.
		 * Otherwise, it is from last_updated to period_end
		 * and remaining active time will be reflected in the next step.
		 */
		contributer = min(now, mlt->period_start + MLT_PERIOD_SIZE);
		mlt->active_sum += contributer - mlt->last_updated;
		mlt->active_ratio_recent =
			div64_u64(mlt->active_sum << SCHED_CAPACITY_SHIFT, MLT_PERIOD_SIZE);

		/*
		 * If now has passed recent period, calculate full periods and reflect they.
		 */
		period_count = div64_u64_rem(elapsed, MLT_PERIOD_SIZE, &remainder);
		if (period_count) {
			mlt_update_full_hist(mlt, period_count - 1);
			mlt->active_sum = remainder;
			mlt->active_ratio_recent =
				div64_u64(mlt->active_sum << SCHED_CAPACITY_SHIFT, MLT_PERIOD_SIZE);
		}
	} else {
		/*
		 * If 'mlt->running' is false, it means that the rq is idle
		 * from last_update until now.
		 */

		/*
		 * If now has passed recent period, calculate empty periods and reflect they.
		 */
		period_count = div64_u64(elapsed, MLT_PERIOD_SIZE);
		if (period_count) {
			mlt_update_hist(mlt, period_count - 1);
			mlt->active_ratio_recent = 0;
			mlt->active_sum = 0;
		}
	}

	mlt->period_start += MLT_PERIOD_SIZE * period_count;
	mlt->last_updated = now;
}

static inline int
mlt_can_update_art(struct mlt *mlt)
{
	if (unlikely(mlt->period_start == 0))
		return 0;

	return 1;
}

/********************************************************/
/*			External APIs			*/
/********************************************************/
void mlt_enqueue_task(struct rq *rq) {
	struct mlt *mlt = &rq->mlt;
	int cpu = cpu_of(rq);
	u64 now = sched_clock_cpu(0);

	if (!mlt_can_update_art(mlt))
		return;

	/*
	 * This type is called when the rq is switched from idle to running.
	 * In this time, Update the active ratio for the idle interval
	 * and change the state to running.
	 */
	mlt_update(cpu, mlt, now, 0);

	if (rq->nr_running == 0) {
		mlt->running = true;
		trace_ems_cpu_active_ratio(cpu, mlt, "enqueue");
	}
}

void mlt_dequeue_task(struct rq *rq) {
	struct mlt *mlt = &rq->mlt;
	int cpu = cpu_of(rq);
	u64 now = sched_clock_cpu(0);

	if (!mlt_can_update_art(mlt))
		return;

	/*
	 * This type is called when the rq is switched from running to idle.
	 * In this time, Update the active ratio for the running interval
	 * and change the state to not-running.
	 */
	mlt_update(cpu, mlt, now, 0);

	if (rq->nr_running == 1) {
		mlt->running = false;
		trace_ems_cpu_active_ratio(cpu, mlt, "dequeue");
	}
}

void mlt_wakeup_task(struct rq *rq) {
	struct mlt *mlt = &rq->mlt;
	int cpu = cpu_of(rq);
	u64 now = sched_clock_cpu(0);

	if (!mlt_can_update_art(mlt))
		return;

	mlt_update(cpu, mlt, now, 1);
	trace_ems_cpu_active_ratio(cpu, mlt, "new task");
}

void mlt_update_recent(struct rq *rq) {
	struct mlt *mlt = &rq->mlt;
	int cpu = cpu_of(rq);
	u64 now = sched_clock_cpu(0);

	if (!mlt_can_update_art(mlt))
		return;

	/*
	 * This type is called to update the active ratio during rq is running.
	 */
	mlt_update(cpu, mlt, now, 0);
	trace_ems_cpu_active_ratio(cpu, mlt, "update");
}

void mlt_set_period_start(struct rq *rq)
{
	struct mlt *mlt = &rq->mlt;
	u64 now;

	if (likely(mlt->period_start))
		return;

	now = sched_clock_cpu(0);
	mlt->period_start = now;
	mlt->last_updated = now;
}

int mlt_art_last_value(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct mlt *mlt = &rq->mlt;

	return mlt->period[mlt->cur_period];
}

bool mlt_art_high_patten(struct mlt *mlt)
{
	if (!mlt->running &&
			(mlt->active_ratio_avg < high_patten_thres ||
			 mlt->active_ratio_stdev > high_patten_stdev))
		return true;

	return false;
}

inline
int mlt_art_boost(struct mlt *mlt)
{
	return mlt->active_ratio_boost;
}

inline
int mlt_art_boost_limit(struct mlt *mlt)
{
	return mlt->active_ratio_limit;
}

inline
int mlt_art_last_boost_time(struct mlt *mlt)
{
	return mlt->last_boost_time;
}

/********************************************************/
/*			  SYSFS				*/
/********************************************************/
static ssize_t show_mlt_policy(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%u. %s\n", mlt_policy_idx,
			mlt_policy_name[mlt_policy_idx]);
}

static ssize_t store_mlt_policy(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	long input;

	if (!sscanf(buf, "%ld", &input))
		return -EINVAL;

	if (input >= MLT_POLICY_INVALID || input < 0)
		return -EINVAL;

	mlt_policy_idx = input;

	return count;
}

static ssize_t show_mlt_policy_list(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	for (i = 0; i < MLT_POLICY_INVALID ; i++)
		len += sprintf(buf + len, "%u. %s\n", i, mlt_policy_name[i]);

	return len;
}

static ssize_t show_active_ratio_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct mlt *mlt;
	int cpu, len = 0;

	for_each_possible_cpu(cpu) {
		mlt = &cpu_rq(cpu)->mlt;
		len += sprintf(buf + len, "cpu%d ratio:%3d\n",
				cpu, mlt->active_ratio_limit);
	}

	return len;
}

static ssize_t store_active_ratio_limit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	struct mlt *mlt;
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
		mlt = &cpu_rq(i)->mlt;
		mlt->active_ratio_limit = ratio;
	}

	return count;
}

static ssize_t show_active_ratio_boost(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct mlt *mlt;
	int cpu, len = 0;

	for_each_possible_cpu(cpu) {
		mlt = &cpu_rq(cpu)->mlt;
		len += sprintf(buf + len, "cpu%d ratio:%3d\n",
				cpu, mlt->active_ratio_boost);
	}

	return len;
}

static ssize_t store_active_ratio_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	struct mlt *mlt;
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
		mlt = &cpu_rq(i)->mlt;
		mlt->active_ratio_boost = ratio;
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
store_node_function(low_patten_count, (MLT_PERIOD_SIZE / 2));
show_node_function(low_patten_thres);
store_node_function(low_patten_thres, (SCHED_CAPACITY_SCALE * 2));
show_node_function(low_patten_stdev);
store_node_function(low_patten_stdev, SCHED_CAPACITY_SCALE);

static struct kobj_attribute _policy =
__ATTR(policy, 0644, show_mlt_policy, store_mlt_policy);
static struct kobj_attribute _policy_list =
__ATTR(policy_list, 0444, show_mlt_policy_list, NULL);
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

static int __init mlt_init_sysfs(void)
{
	struct kobject *kobj;

	kobj = kobject_create_and_add("mlt", ems_kobj);
	if (!kobj)
		return -EINVAL;

	if (sysfs_create_group(kobj, &attr_group))
		return -EINVAL;

	return 0;
}
late_initcall(mlt_init_sysfs);

static int __init mlt_parse_dt(void)
{
	struct device_node *dn, *coregroup;
	char name[15];
	int cpu, cnt = 0, limit = -1, boost = -1;

	dn = of_find_node_by_path("/cpus/ems/mlt");
	if (!dn)
		return 0;

	for_each_possible_cpu(cpu) {
		struct mlt *mlt = &cpu_rq(cpu)->mlt;

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
			mlt->active_ratio_limit = SCHED_CAPACITY_SCALE * limit / 100;

		if (boost >= 0)
			mlt->active_ratio_boost = SCHED_CAPACITY_SCALE * boost / 100;
	}

	return 0;
}
core_initcall(mlt_parse_dt);

void __init mlt_init(void)
{
	int cpu, idx;

	for_each_possible_cpu(cpu) {
		struct mlt *mlt = &cpu_rq(cpu)->mlt;

		/* Set by default value */
		mlt->running = false;
		mlt->active_sum = 0;
		mlt->active_ratio_recent = 0;
		mlt->cur_period = 0;
		for (idx = 0; idx < MLT_HIST_SIZE_MAX; idx++)
			mlt->period[idx] = 0;

		mlt->period_start = 0;
		mlt->last_updated = 0;
	}
}
