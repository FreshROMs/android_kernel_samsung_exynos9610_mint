/*
 * On-time Migration Feature for Exynos Mobile Scheduler (EMS)
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * LEE DAEYEONG <daeyeong.lee@samsung.com>
 */

#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/pm_qos.h>
#include <linux/ems.h>
#include <linux/sched/energy.h>

#include <trace/events/ems.h>

#include "../sched.h"
#include "../tune.h"
#include "./ems.h"

/****************************************************************/
/*			On-time migration			*/
/****************************************************************/
static int ontime_enabled[CGROUP_COUNT];

static int support_ontime(struct task_struct *p)
{
	int group_idx = schedtune_task_group_idx(p);

	return ontime_enabled[group_idx];
}

#define MIN_CAPACITY_CPU	0
#define MAX_CAPACITY_CPU	(NR_CPUS - 1)

/* Structure of ontime migration condition */
struct ontime_cond {
	bool			enabled;

	unsigned long		upper_boundary;
	unsigned long		lower_boundary;

	int			coregroup;
	struct cpumask		cpus;

	struct list_head	list;

	/* kobject for sysfs group */
	struct kobject		kobj;
};
LIST_HEAD(cond_list);

/* Structure of ontime migration environment */
struct ontime_env {
	struct rq		*dst_rq;
	struct rq		*src_rq;
	struct task_struct	*target_task;
	u64			flags;
};
DEFINE_PER_CPU(struct ontime_env, ontime_env);

struct ontime_cond *get_current_cond(int cpu)
{
	struct ontime_cond *curr;

	list_for_each_entry(curr, &cond_list, list) {
		if (cpumask_test_cpu(cpu, &curr->cpus))
			return curr;
	}

	return NULL;
}

static unsigned long get_upper_boundary(int cpu)
{
	struct ontime_cond *curr = get_current_cond(cpu);

	if (curr)
		return curr->upper_boundary;
	else
		return ULONG_MAX;
}

static unsigned long get_lower_boundary(int cpu)
{
	struct ontime_cond *curr = get_current_cond(cpu);

	if (curr)
		return curr->lower_boundary;
	else
		return 0;
}

static bool is_faster_than(int src, int dst)
{
	if (capacity_max_of(src) < capacity_max_of(dst))
		return true;
	else
		return false;
}

static inline int check_migrate_slower(int src, int dst)
{
	if (cpumask_test_cpu(src, cpu_coregroup_mask(dst)))
		return false;

	if (capacity_max_of(src) > capacity_max_of(dst))
		return true;
	else
		return false;
}

void
ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus)
{
	struct ontime_cond *curr;
	int src_cpu = task_cpu(p);
	u32 util = ml_uclamp_task_util(p);
	struct cpumask mask;

	curr = get_current_cond(src_cpu);
	if (!curr)
		goto done;

	/*
	 * If the task belongs to a group that does not support ontime
	 * migration, all active cpus are fit.
	 */
	if (!support_ontime(p)) {
		cpumask_copy(&mask, cpu_active_mask);
		goto done;
	}

	/*
	 * case 1) task util < lower boundary
	 *
	 * If task 'util' is smaller than lower boundary of current domain,
	 * do not target specific cpu because ontime migration is not involved
	 * in down migration. All active cpus are fit.
	 *
	 * fit_cpus = cpu_active_mask
	 */
	if (util < curr->lower_boundary) {
		cpumask_copy(&mask, cpu_active_mask);
		goto done;
	}

	cpumask_clear(&mask);

	/*
	 * case 2) lower boundary <= task util < upper boundary
	 *
	 * If task 'util' is between lower boundary and upper boundary of
	 * current domain, both current and faster domain are fit.
	 *
	 * fit_cpus = current cpus & faster cpus
	 */
	if (util < curr->upper_boundary) {
		cpumask_or(&mask, &mask, &curr->cpus);
		list_for_each_entry(curr, &cond_list, list) {
			int dst_cpu = cpumask_first(&curr->cpus);

			if (is_faster_than(src_cpu, dst_cpu))
				cpumask_or(&mask, &mask, &curr->cpus);
		}

		goto done;
	}

	/*
	 * case 3) task util >= upper boundary
	 *
	 * If task 'util' is greater than boundary of current domain, only
	 * faster domain is fit to gurantee cpu performance.
	 *
	 * fit_cpus = faster cpus
	 */
	list_for_each_entry(curr, &cond_list, list) {
		int dst_cpu = cpumask_first(&curr->cpus);

		if (is_faster_than(src_cpu, dst_cpu))
			cpumask_or(&mask, &mask, &curr->cpus);
	}

done:
	/*
	 * Check if the task is given a mulligan.
	 * If so, remove src_cpu from candidate.
	 */
	if (EMS_PF_GET(p) & EMS_PF_MULLIGAN)
		cpumask_clear_cpu(src_cpu, &mask);

	cpumask_copy(fit_cpus, &mask);
}

static struct task_struct *pick_heavy_task(struct rq *rq)
{
	struct task_struct *p, *heaviest_task = NULL;
	unsigned long util, max_util = 0;
	int task_count = 0;

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (tex_task(p)) {
			heaviest_task = p;
			break;
		}

		if (!support_ontime(p))
			continue;

		/*
		 * Pick the task with the biggest util among tasks whose
		 * util is greater than the upper boundary.
		 */
		util = ml_uclamp_task_util(p);
		if (util < get_upper_boundary(task_cpu(p)))
			continue;

		if (util > max_util) {
			heaviest_task = p;
			max_util = util;
		}

		if (++task_count >= TRACK_TASK_COUNT)
			break;
	}

	return heaviest_task;
}

static
bool can_migrate(struct task_struct *p, struct ontime_env *env)
{
	struct rq *src_rq = env->src_rq, *dst_rq = env->dst_rq;

	if (p->exit_state)
		return false;

	if (!cpu_active(dst_rq->cpu))
		return false;

	if (unlikely(src_rq != task_rq(p)))
		return false;

	if (unlikely(src_rq->cpu != smp_processor_id()))
		return false;

	if (src_rq->nr_running <= 1)
		return false;

	if (!cpumask_test_cpu(dst_rq->cpu, p->cpus_ptr))
		return false;

	if (task_running(src_rq, p))
		return false;

	return true;
}

static void move_task(struct task_struct *p, struct ontime_env *env)
{
	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(env->src_rq, p, 0);
	set_task_cpu(p, env->dst_rq->cpu);

	activate_task(env->dst_rq, p, 0);
	p->on_rq = TASK_ON_RQ_QUEUED;
	check_preempt_curr(env->dst_rq, p, 0);
}

static int move_specific_task(struct task_struct *target, struct ontime_env *env)
{
	struct list_head *tasks = &env->src_rq->cfs_tasks;
	struct task_struct *p, *n;

	list_for_each_entry_safe(p, n, tasks, se.group_node) {
		if (p != target)
			continue;

		move_task(p, env);
		return 1;
	}

	return 0;
}

static int ontime_migration_cpu_stop(void *data)
{
	struct ontime_env *env = data;
	struct rq *src_rq, *dst_rq;
	struct task_struct *p;

	/* Initialize environment data */
	src_rq = env->src_rq;
	dst_rq = env->dst_rq;
	p = env->target_task;

	raw_spin_lock_irq(&src_rq->lock);

	/* Check task can be migrated */
	if (!can_migrate(p, env))
		goto out_unlock;

	BUG_ON(src_rq == dst_rq);

	/* Move task from source to destination */
	double_lock_balance(src_rq, dst_rq);
	if (move_specific_task(p, env)) {
		trace_ems_ontime_migration(p, ml_uclamp_task_util(p),
				src_rq->cpu, dst_rq->cpu, "heavy task");
	}
	double_unlock_balance(src_rq, dst_rq);

out_unlock:
	src_rq->active_balance = 0;

	raw_spin_unlock_irq(&src_rq->lock);
	put_task_struct(p);

	return 0;
}

static bool ontime_check_runnable(struct ontime_env *env, struct rq *rq)
{
	int cpu = cpu_of(rq);

	if (is_busy_cpu(cpu)) {
		env->flags |= EMS_PF_RUNNABLE_BUSY;
		return true;
	}

	return rq->nr_running > 1 &&
		mlt_art_last_value(cpu) == SCHED_CAPACITY_SCALE;
}

static struct rq *ontime_find_mulligan_rq(struct task_struct *target_p,
					  struct ontime_env *env)
{
	struct rq *src_rq = env->src_rq, *dst_rq;
	int dst_cpu;

	/* Set flag to skip src_cpu when iterating candidates */
	EMS_PF_SET(target_p, env->flags);

	/* Find next cpu for this task */
	dst_cpu = ems_select_task_rq_fair(target_p, cpu_of(src_rq), 0, 0);

	/* Clear flag */
	EMS_PF_CLEAR(target_p, env->flags);

	if (dst_cpu < 0 || cpu_of(src_rq) == dst_cpu)
		return NULL;

	dst_rq = cpu_rq(dst_cpu);

	return dst_rq;
}

static int ontime_detach_task(struct task_struct *target_p,
			      struct ontime_env *env)
{
	struct rq *src_rq = env->src_rq, *dst_rq = env->dst_rq;
	int dst_cpu = cpu_of(dst_rq);

	/* Deactivate this task to migrate it */
	deactivate_task(src_rq, target_p, 0);
	set_task_cpu(target_p, dst_cpu);

	/* Returning 1 means a task is detached. */
	return 1;
}

static int ontime_attach_task(struct task_struct *target_p,
			      const struct ontime_env *env)
{
	struct rq *src_rq = env->src_rq, *dst_rq = env->dst_rq;
	struct rq_flags rf;

	rq_lock(dst_rq, &rf);
	activate_task(dst_rq, target_p, 0);
	check_preempt_curr(dst_rq, target_p, 0);
	rq_unlock(dst_rq, &rf);

	trace_ems_ontime_migration(target_p, ml_task_util_est(target_p),
				cpu_of(src_rq), cpu_of(dst_rq), "mulligan");

	/* Returning 1 means a task is attached. */
	return 1;
}

static void ontime_mulligan(void)
{
	struct rq *src_rq = this_rq(), *dst_rq;
	struct list_head *tasks = &src_rq->cfs_tasks;
	struct task_struct *p, *target_p = NULL;
	struct ontime_env local_env = { .src_rq = src_rq, .flags = EMS_PF_MULLIGAN };
	unsigned long flags;
	unsigned long min_util = ULONG_MAX, task_util;

	if (!ontime_check_runnable(&local_env, src_rq))
		return;

	raw_spin_lock_irqsave(&src_rq->lock, flags);

	list_for_each_entry_reverse(p, tasks, se.group_node) {
		/* Current task cannot get mulligan */
		if (&p->se == src_rq->cfs.curr)
			continue;

		task_util = ml_uclamp_task_util(p);

		/*
		 * If this cpu is determined as runnable busy, only tasks which
		 * have utilization under 6.25% of capacity can get mulligan.
		 */
		if ((local_env.flags & EMS_PF_RUNNABLE_BUSY)
		    && task_util >= capacity_orig_of(cpu_of(src_rq)) >> 4)
			continue;

		/* Find min util task */
		if (task_util < min_util) {
			target_p = p;
			min_util = task_util;
		}
	}

	/* No task is given a mulligan */
	if (!target_p) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return;
	}

	dst_rq = ontime_find_mulligan_rq(target_p, &local_env);

	/* No rq exists for the mulligan */
	if (!dst_rq) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return;
	}

	/* Fill ontime_env and check whether the task can be migrated */
	local_env.dst_rq = dst_rq;
	local_env.target_task = target_p;
	if (!can_migrate(target_p, &local_env)) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return;
	}

	/* Finally, the task can be moved! */
	ontime_detach_task(target_p, &local_env);

	raw_spin_unlock_irqrestore(&src_rq->lock, flags);

	/* Let's give a second chance to the task */
	ontime_attach_task(target_p, &local_env);
}

DEFINE_PER_CPU(struct cpu_stop_work, ontime_migration_work);

static void ontime_heavy_migration(void)
{
	struct rq *rq = this_rq();
	struct ontime_env *env = &per_cpu(ontime_env, rq->cpu);
	struct task_struct *p;
	int dst_cpu;
	unsigned long flags;
	unsigned long util;
	struct ontime_cond *curr;

	/*
	 * If this cpu belongs to last domain of ontime, no cpu is
	 * faster than this cpu. Skip ontime migration for heavy task.
	 */
	if (cpumask_test_cpu(rq->cpu, cpu_coregroup_mask(MAX_CAPACITY_CPU)))
		return;

	curr = get_current_cond(rq->cpu);
	if (!curr)
		return;

	/*
	 * No need to traverse rq to find a heavy task
	 * if this CPU utilization is under upper boundary
	 */
	util = ml_cpu_util(cpu_of(rq));
   	util = uclamp_util_with(rq, util, NULL);
	if (util < curr->upper_boundary)
		return;

	raw_spin_lock_irqsave(&rq->lock, flags);

	/*
	 * Ontime migration is not performed when active balance is in progress.
	 */
	if (rq->active_balance) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	/*
	 * No need to migration if source cpu does not have cfs tasks.
	 */
	if (!rq->cfs.curr) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	/*
	 * Pick task to be migrated.
	 * Return NULL if there is no heavy task in rq.
	 */
	p = pick_heavy_task(rq);
	if (!p) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	/* Select destination cpu which the task will be moved */
	dst_cpu = ems_select_task_rq_fair(p, rq->cpu, 0, 0);
	if (dst_cpu < 0 || rq->cpu == dst_cpu) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	get_task_struct(p);

	/* Set environment data */
	env->dst_rq = cpu_rq(dst_cpu);
	env->src_rq = rq;
	env->target_task = p;

	/* Prevent active balance to use stopper for migration */
	rq->active_balance = 1;

	raw_spin_unlock_irqrestore(&rq->lock, flags);

	/* Migrate task through stopper */
	stop_one_cpu_nowait(rq->cpu, ontime_migration_cpu_stop, env,
			&per_cpu(ontime_migration_work, rq->cpu));
}

/****************************************************************/
/*		           	    External APIs				            */
/****************************************************************/
static bool skip_ontime;

void ontime_migration(void)
{
	if (skip_ontime)
		return;

	ontime_mulligan();
	ontime_heavy_migration();
}

int ontime_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);
	unsigned long util;

	if (!support_ontime(p))
		return true;

	/*
	 * Task is heavy enough but load balancer tries to migrate the task to
	 * slower cpu, it does not allow migration.
	 */
	util = ml_task_util(p);
	if (util >= get_lower_boundary(src_cpu) &&
	    check_migrate_slower(src_cpu, dst_cpu)) {
		/*
		 * However, only if the source cpu is overutilized, it allows
		 * migration if the task is not very heavy.
		 * (criteria : task util is under 75% of cpu util)
		 */
		if (cpu_overutilized(src_cpu) &&
			util * 100 < (cpu_util(src_cpu) * 75)) {
			trace_ems_ontime_check_migrate(p, dst_cpu, true, "src overutil");
			return true;
		}

		trace_ems_ontime_check_migrate(p, dst_cpu, false, "migrate to slower");
		return false;
	}

	trace_ems_ontime_check_migrate(p, dst_cpu, false, "normal migration");

	return true;
}

u64 ems_ontime_enabled_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	struct schedtune *st = css_st(css);
	int group_idx;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT)
		return (u64) ontime_enabled[CGROUP_ROOT];

	return (u64) ontime_enabled[group_idx];
}

int ems_ontime_enabled_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled) {
	struct schedtune *st = css_st(css);
	int group_idx;

	if (enabled < 0 || enabled > 1)
		return -EINVAL;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT) {
		ontime_enabled[CGROUP_ROOT] = enabled;
		return 0;
	}

	ontime_enabled[group_idx] = enabled;
	return 0;
}

/****************************************************************/
/*		  sysbusy state change notifier			*/
/****************************************************************/
static int ontime_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	skip_ontime = (state > SYSBUSY_STATE1);

	return NOTIFY_OK;
}

static struct notifier_block ontime_sysbusy_notifier = {
	.notifier_call = ontime_sysbusy_notifier_call,
};

/****************************************************************/
/*				             SYSFS			                	*/
/****************************************************************/
static struct kobject *ontime_kobj;

struct ontime_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define ontime_attr_rw(_name)				\
static struct ontime_attr _name##_attr =		\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define ontime_show(_name)							\
static ssize_t show_##_name(struct kobject *k, char *buf)			\
{										\
	struct ontime_cond *cond = container_of(k, struct ontime_cond, kobj);	\
										\
	return sprintf(buf, "%u\n", (unsigned int)cond->_name);			\
}

#define ontime_store(_name, _type, _max)					\
static ssize_t store_##_name(struct kobject *k, const char *buf, size_t count)	\
{										\
	unsigned int val;							\
	struct ontime_cond *cond = container_of(k, struct ontime_cond, kobj);	\
										\
	if (!sscanf(buf, "%u", &val))						\
		return -EINVAL;							\
										\
	val = val > _max ? _max : val;						\
	cond->_name = (_type)val;						\
										\
	return count;								\
}

ontime_show(upper_boundary);
ontime_show(lower_boundary);
ontime_store(upper_boundary, unsigned long, 1024);
ontime_store(lower_boundary, unsigned long, 1024);
ontime_attr_rw(upper_boundary);
ontime_attr_rw(lower_boundary);

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct ontime_attr *oattr = container_of(at, struct ontime_attr, attr);

	return oattr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
		     const char *buf, size_t count)
{
	struct ontime_attr *oattr = container_of(at, struct ontime_attr, attr);

	return oattr->store(kobj, buf, count);
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
static ssize_t show_ontime_enabled(struct kobject *kobj,
        struct kobj_attribute *attr, char *buf)
{
    int i, ret = 0;

    for (i = 0; i < CGROUP_COUNT; i++)
        ret += snprintf(buf + ret, 40, "cgroup=%4s enabled=%d\n",
                task_cgroup_simple_name[i],
                ontime_enabled[i]);

    ret += sprintf(buf + ret, "\n");
    ret += sprintf(buf + ret, "usage:");
    ret += sprintf(buf + ret, "   # echo <group> <enabled=0/1> > %s\n", "ontime_enabled");
    ret += sprintf(buf + ret, "(group0/1/2/3/4=root/fg/bg/ta/rt)\n");

    return ret;
}

static ssize_t store_ontime_enabled(struct kobject *kobj,
        struct kobj_attribute *attr, const char *buf,
        size_t count)
{
    int i, grp_idx, value, ret;

    ret = sscanf(buf, "%d %d", &grp_idx, &value);
    if (ret < 0)
        return -EINVAL;

    if (grp_idx < 0 || grp_idx > CGROUP_COUNT)
        return -EINVAL;

    if (value < 0 || value > 1)
        return -EINVAL;

    ontime_enabled[i] = value;

    return count;
}

static struct kobj_attribute ontime_enabled_attr =
__ATTR(ontime_enabled, 0644, show_ontime_enabled, store_ontime_enabled);


static const struct sysfs_ops ontime_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct attribute *ontime_attrs[] = {
	&upper_boundary_attr.attr,
	&lower_boundary_attr.attr,
	NULL
};

static struct kobj_type ktype_ontime = {
	.sysfs_ops	= &ontime_sysfs_ops,
	.default_attrs	= ontime_attrs,
};

static int __init ontime_sysfs_init(void)
{
	struct ontime_cond *curr;

	if (list_empty(&cond_list))
		return 0;

	ontime_kobj = kobject_create_and_add("ontime", ems_kobj);
	if (!ontime_kobj)
		goto out;

	if (sysfs_create_file(ontime_kobj, &ontime_enabled_attr.attr))
		pr_warn("%s: failed to create ontime enabled sysfs\n", __func__);

	/* Add ontime sysfs node for each coregroup */
	list_for_each_entry(curr, &cond_list, list) {
		int ret;

		/* If ontime is disabled in this coregroup, do not create sysfs node */
		if (!curr->enabled)
			continue;

		ret = kobject_init_and_add(&curr->kobj, &ktype_ontime,
				ontime_kobj, "coregroup%d", curr->coregroup);
		if (ret)
			goto out;
	}

	return 0;

out:
	pr_err("ONTIME(%s): failed to create sysfs node\n", __func__);
	return -EINVAL;
}

/****************************************************************/
/*			initialization				*/
/****************************************************************/
static inline unsigned long get_boundary(unsigned long capacity, int ratio)
{
	/*
	 * If ratio is negative, migration is disabled
	 * -> threshold == maximum util(1024)
	 */
	if (ratio < 0)
		return SCHED_CAPACITY_SCALE;

	return capacity * ratio / 100;

}

static void __init
parse_ontime(struct device_node *dn, struct ontime_cond *cond, int cnt)
{
	struct device_node *ontime, *coregroup;
	char name[15];
	unsigned long capacity;
	int prop;
	int res = 0;

	ontime = of_get_child_by_name(dn, "ontime");
	if (!ontime)
		goto disable;

	snprintf(name, sizeof(name), "coregroup%d", cnt);
	coregroup = of_get_child_by_name(ontime, name);
	if (!coregroup)
		goto disable;
	cond->coregroup = cnt;

	capacity = capacity_max_of(cpumask_first(&cond->cpus));

	/* If capacity of this coregroup is 0, disable ontime of this coregroup */
	if (capacity == 0)
		goto disable;

	/* If any of ontime parameter isn't, disable ontime of this coregroup */
	res |= of_property_read_s32(coregroup, "upper-boundary", &prop);
	cond->upper_boundary = get_boundary(capacity, prop);

	res |= of_property_read_s32(coregroup, "lower-boundary", &prop);
	cond->lower_boundary = get_boundary(capacity, prop);

	if (res)
		goto disable;

	cond->enabled = true;
	return;

disable:
	pr_err("ONTIME(%s): failed to parse ontime node\n", __func__);
	cond->enabled = false;
	cond->upper_boundary = ULONG_MAX;
	cond->lower_boundary = 0;
}

static int __init init_ontime(void)
{
	struct ontime_cond *cond;
	struct device_node *dn;
	int cpu, cnt = 0;

	INIT_LIST_HEAD(&cond_list);

	dn = of_find_node_by_path("/cpus/ems");
	if (!dn)
		return 0;

	/* Explicitly assigned */
	skip_ontime = false;

	if (!cpumask_equal(cpu_possible_mask, cpu_all_mask))
		return 0;

	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cond = kzalloc(sizeof(struct ontime_cond), GFP_KERNEL);

		cpumask_copy(&cond->cpus, cpu_coregroup_mask(cpu));

		parse_ontime(dn, cond, cnt++);

		list_add_tail(&cond->list, &cond_list);
	}

	ontime_sysfs_init();

	sysbusy_register_notifier(&ontime_sysbusy_notifier);

	of_node_put(dn);
	return 0;
}
late_initcall(init_ontime);
