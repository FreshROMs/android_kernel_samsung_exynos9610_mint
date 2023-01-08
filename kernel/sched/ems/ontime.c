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
#define TASK_TRACK_COUNT	5
#define MIN_CAPACITY_CPU	0
#define MAX_CAPACITY_CPU	(NR_CPUS - 1)

#define ontime_of(p)		(&p->se.ontime)

#define cap_scale(v, s)		((v)*(s) >> SCHED_CAPACITY_SHIFT)

#define entity_is_cfs_rq(se)	(se->my_q)
#define entity_is_task(se)	(!se->my_q)

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
	int			dst_cpu;
	struct rq		*src_rq;
	int			src_cpu;
	struct task_struct	*target_task;
	int			boost_migration;
};
DEFINE_PER_CPU(struct ontime_env, ontime_env);

static inline struct sched_entity *se_of(struct sched_avg *sa)
{
	return container_of(sa, struct sched_entity, avg);
}

extern long schedtune_margin(unsigned long capacity, unsigned long signal, long boost);
static inline unsigned long ontime_load_avg(struct task_struct *p)
{
	int boost = schedtune_task_boost(p);
	unsigned long load_avg = ontime_of(p)->avg.load_avg;
	unsigned long capacity;

	if (boost == 0)
		return load_avg;

	capacity = capacity_orig_of(task_cpu(p));
	return load_avg + schedtune_margin(capacity, load_avg, boost);
}

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
	struct cpumask mask;
	int src_cpu = task_cpu(p);
	unsigned long load_avg = ontime_load_avg(p);

	cpumask_clear(&mask);
	cpumask_copy(&mask, cpu_active_mask);

	curr = get_current_cond(src_cpu);
	if (!curr)
		goto done;

	/*
	 * If the task belongs to a group that does not support ontime
	 * migration or task is currently migrating, it can be assigned to all
	 * active cpus without specifying fit cpus.
	 */
	if (!schedtune_ontime_en(p) || ontime_of(p)->migrating)
		goto done;

	/*
	 * case 1) task load_avg < lower boundary
	 *
	 * If task 'load_avg' is smaller than lower boundary of current domain,
	 * do not target specific cpu because ontime migration is not involved
	 * in down migration. All active cpus are fit.
	 *
	 * fit_cpus = cpu_active_mask
	 */
	if (load_avg < curr->lower_boundary)
		goto done;

	cpumask_clear(&mask);

	/*
	 * case 2) lower boundary <= task load_avg < upper boundary
	 *
	 * If task 'load_avg' is between lower boundary and upper boundary of
	 * current domain, both current and faster domain are fit.
	 *
	 * fit_cpus = current cpus & faster cpus
	 */
	if (load_avg < curr->upper_boundary) {
		cpumask_or(&mask, &mask, &curr->cpus);
		list_for_each_entry(curr, &cond_list, list) {
			int dst_cpu = cpumask_first(&curr->cpus);

			if (is_faster_than(src_cpu, dst_cpu))
				cpumask_or(&mask, &mask, &curr->cpus);
		}

		goto masking;
	}

	/*
	 * case 3) task load_avg >= upper boundary
	 *
	 * If task 'load_avg' is greater than boundary of current domain, only
	 * faster domain is fit to gurantee cpu performance.
	 *
	 * fit_cpus = faster cpus
	 */
	list_for_each_entry(curr, &cond_list, list) {
		int dst_cpu = cpumask_first(&curr->cpus);

		if (is_faster_than(src_cpu, dst_cpu))
			cpumask_or(&mask, &mask, &curr->cpus);
	}

masking:
	cpumask_and(&mask, &mask, cpu_active_mask);
done:
	cpumask_clear(fit_cpus);
	cpumask_copy(fit_cpus, &mask);
}

extern struct sched_entity *__pick_next_entity(struct sched_entity *se);
static struct task_struct *
ontime_pick_heavy_task(struct sched_entity *se, int *boost_migration)
{
	struct task_struct *heaviest_task = NULL;
	struct task_struct *p = task_of(se);
	unsigned int max_util_avg = 0;
	unsigned int util_avg = 0;
	int task_count = 0;
	int boosted = !!schedtune_task_on_top(p) || !!schedtune_prefer_perf(p);

	/*
	 * Since current task does not exist in entity list of cfs_rq,
	 * check first that current task is heavy.
	 */
	p = task_of(se);
	if (boosted) {
		*boost_migration = 1;
		return p;
	}
	if (schedtune_ontime_en(p)) {
		util_avg = ontime_load_avg(p);
		if (util_avg >= get_upper_boundary(task_cpu(p))) {
			heaviest_task = p;
			max_util_avg = util_avg;
			*boost_migration = 0;
		}
	}

	se = __pick_first_entity(se->cfs_rq);
	while (se && task_count < TASK_TRACK_COUNT) {
		/* Skip non-task entity */
		if (entity_is_cfs_rq(se))
			goto next_entity;

		p = task_of(se);
		if (schedtune_task_on_top(p) || schedtune_prefer_perf(p)) {
			heaviest_task = p;
			*boost_migration = 1;
			break;
		}

		if (!schedtune_ontime_en(p))
			goto next_entity;

		util_avg = ontime_load_avg(p);
		if (util_avg < get_upper_boundary(task_cpu(p)))
			goto next_entity;

		if (util_avg > max_util_avg) {
			heaviest_task = p;
			max_util_avg = util_avg;
			*boost_migration = 0;
		}

next_entity:
		se = __pick_next_entity(se);
		task_count++;
	}

	return heaviest_task;
}

static bool can_migrate(struct task_struct *p, struct ontime_env *env)
{
	struct rq *src_rq = env->src_rq;
	int src_cpu = env->src_cpu;

	if (ontime_of(p)->migrating == 0)
		return false;

	if (p->exit_state)
		return false;

	if (unlikely(src_rq != task_rq(p)))
		return false;

	if (unlikely(src_cpu != smp_processor_id()))
		return false;

	if (src_rq->nr_running <= 1)
		return false;

	if (!cpumask_test_cpu(env->dst_cpu, tsk_cpus_allowed(p)))
		return false;

	if (task_running(env->src_rq, p))
		return false;

	return true;
}

static void move_task(struct task_struct *p, struct ontime_env *env)
{
	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(env->src_rq, p, 0);
	set_task_cpu(p, env->dst_cpu);

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
	int src_cpu, dst_cpu;
	int boost_migration;

	/* Initialize environment data */
	src_rq = env->src_rq;
	dst_rq = env->dst_rq = cpu_rq(env->dst_cpu);
	src_cpu = env->src_cpu = env->src_rq->cpu;
	dst_cpu = env->dst_cpu;
	p = env->target_task;
	boost_migration = env->boost_migration;

	raw_spin_lock_irq(&src_rq->lock);

	/* Check task can be migrated */
	if (!can_migrate(p, env))
		goto out_unlock;

	BUG_ON(src_rq == dst_rq);

	/* Move task from source to destination */
	double_lock_balance(src_rq, dst_rq);
	if (move_specific_task(p, env)) {
		trace_ems_ontime_migration(p, ontime_of(p)->avg.load_avg,
				src_cpu, dst_cpu, boost_migration);
	}
	double_unlock_balance(src_rq, dst_rq);

out_unlock:
	ontime_of(p)->migrating = 0;

	src_rq->active_balance = 0;
	dst_rq->ontime_migrating = 0;
	dst_rq->ontime_boost_migration = 0;

	raw_spin_unlock_irq(&src_rq->lock);
	put_task_struct(p);

	return 0;
}

static void ontime_update_next_balance(int cpu, struct ontime_avg *oa)
{
	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(MAX_CAPACITY_CPU)))
		return;

	if (oa->load_avg < get_upper_boundary(cpu))
		return;

	/*
	 * Update the next_balance of this cpu because tick is most likely
	 * to occur first in currently running cpu.
	 */
	cpu_rq(smp_processor_id())->next_balance = jiffies;
}

extern u64 decay_load(u64 val, u64 n);
static u32 __accumulate_pelt_segments(u64 periods, u32 d1, u32 d3)
{
	u32 c1, c2, c3 = d3;

	c1 = decay_load((u64)d1, periods);
	c2 = LOAD_AVG_MAX - decay_load(LOAD_AVG_MAX, periods) - 1024;

	return c1 + c2 + c3;
}

/****************************************************************/
/*			External APIs				*/
/****************************************************************/
void ontime_trace_task_info(struct task_struct *p)
{
	trace_ems_ontime_load_avg_task(p, &ontime_of(p)->avg, ontime_of(p)->migrating);
}

DEFINE_PER_CPU(struct cpu_stop_work, ontime_migration_work);
static DEFINE_SPINLOCK(om_lock);

void ontime_migration(void)
{
	int cpu;

	if (!spin_trylock(&om_lock))
		return;

	for_each_cpu(cpu, cpu_active_mask) {
		unsigned long flags;
		struct rq *rq = cpu_rq(cpu);
		struct sched_entity *se;
		struct task_struct *p;
		struct ontime_env *env = &per_cpu(ontime_env, cpu);
		int boost_migration = 0;
		int dst_cpu;

		/* Task in big cores don't be ontime migrated. */
		if (cpumask_test_cpu(cpu, cpu_coregroup_mask(MAX_CAPACITY_CPU)))
			break;

		raw_spin_lock_irqsave(&rq->lock, flags);

		/*
		 * Ontime migration is not performed when active balance
		 * is in progress.
		 */
		if (rq->active_balance) {
			raw_spin_unlock_irqrestore(&rq->lock, flags);
			continue;
		}

		/*
		 * No need to migration if source cpu does not have cfs
		 * tasks.
		 */
		if (!rq->cfs.curr) {
			raw_spin_unlock_irqrestore(&rq->lock, flags);
			continue;
		}

		/* Find task entity if entity is cfs_rq. */
		se = rq->cfs.curr;
		if (entity_is_cfs_rq(se)) {
			struct cfs_rq *cfs_rq = se->my_q;

			while (cfs_rq) {
				se = cfs_rq->curr;
				cfs_rq = se->my_q;
			}
		}

		/*
		 * Pick task to be migrated. Return NULL if there is no
		 * heavy task in rq.
		 */
		p = ontime_pick_heavy_task(se, &boost_migration);
		if (!p) {
			raw_spin_unlock_irqrestore(&rq->lock, flags);
			continue;
		}

		/* Select destination cpu which the task will be moved */
		dst_cpu = exynos_select_task_rq(p, cpu, 0, 0, 0);
		if (dst_cpu < 0 || cpu == dst_cpu) {
			raw_spin_unlock_irqrestore(&rq->lock, flags);
			continue;
		}

		ontime_of(p)->migrating = 1;
		get_task_struct(p);

		/* Set environment data */
		env->dst_cpu = dst_cpu;
		env->src_rq = rq;
		env->target_task = p;
		env->boost_migration = boost_migration;

		/* Prevent active balance to use stopper for migration */
		rq->active_balance = 1;

		cpu_rq(dst_cpu)->ontime_migrating = 1;
		cpu_rq(dst_cpu)->ontime_boost_migration = boost_migration;

		raw_spin_unlock_irqrestore(&rq->lock, flags);

		/* Migrate task through stopper */
		stop_one_cpu_nowait(cpu, ontime_migration_cpu_stop, env,
				&per_cpu(ontime_migration_work, cpu));
	}

	spin_unlock(&om_lock);
}

int ontime_can_migration(struct task_struct *p, int dst_cpu)
{
	unsigned long src_util;
	int src_cpu = task_cpu(p);

	if (!schedtune_ontime_en(p))
		return true;

	if (ontime_of(p)->migrating == 1) {
		trace_ems_ontime_check_migrate(p, dst_cpu, false, "on migrating");
		return false;
	}

	/*
	 * Task is heavy enough but load balancer tries to migrate the task to
	 * slower cpu, it does not allow migration.
	 */
	if (ontime_load_avg(p) >= get_lower_boundary(src_cpu) &&
	    check_migrate_slower(src_cpu, dst_cpu)) {
		/*
		 * However, only if the source cpu is overutilized, it allows
		 * migration if the task is not very heavy.
		 * (criteria : task util is under 75% of cpu util)
		 */
		src_util = cpu_util(src_cpu);
		if (cpu_overutilized(capacity_orig_of(src_cpu), src_util) &&
			task_util_est(p) * 100 < (src_util * 75)) {
			trace_ems_ontime_check_migrate(p, dst_cpu, true, "src overutil");
			return true;
		}

		trace_ems_ontime_check_migrate(p, dst_cpu, false, "migrate to slower");
		return false;
	}

	trace_ems_ontime_check_migrate(p, dst_cpu, false, "normal migration");

	return true;
}

/*
 * ontime_update_load_avg : load tracking for ontime-migration
 *
 * @sa : sched_avg to be updated
 * @delta : elapsed time since last update
 * @period_contrib : amount already accumulated against our next period
 * @scale_freq : scale vector of cpu frequency
 * @scale_cpu : scale vector of cpu capacity
 */
void ontime_update_load_avg(u64 delta, int cpu, unsigned long weight, struct sched_avg *sa)
{
	struct ontime_avg *oa = &se_of(sa)->ontime.avg;
	unsigned long scale_freq, scale_cpu;
	u32 contrib = (u32)delta; /* p == 0 -> delta < 1024 */
	u64 periods;

	scale_freq = arch_scale_freq_capacity(NULL, cpu);
	scale_cpu = arch_scale_cpu_capacity(NULL, cpu);

	delta += oa->period_contrib;
	periods = delta / 1024; /* A period is 1024us (~1ms) */

	if (periods) {
		oa->load_sum = decay_load(oa->load_sum, periods);

		delta %= 1024;
		contrib = __accumulate_pelt_segments(periods,
				1024 - oa->period_contrib, delta);
	}
	oa->period_contrib = delta;

	if (weight) {
		contrib = cap_scale(contrib, scale_freq);
		oa->load_sum += contrib * scale_cpu;
	}

	if (!periods)
		return;

	oa->load_avg = div_u64(oa->load_sum, LOAD_AVG_MAX - 1024 + oa->period_contrib);
	ontime_update_next_balance(cpu, oa);
}

void ontime_new_entity_load(struct task_struct *parent, struct sched_entity *se)
{
	struct ontime_entity *ontime;

	if (entity_is_cfs_rq(se))
		return;

	ontime = &se->ontime;

	ontime->avg.load_sum = ontime_of(parent)->avg.load_sum >> 1;
	ontime->avg.load_avg = ontime_of(parent)->avg.load_avg >> 1;
	ontime->avg.period_contrib = 1023;
	ontime->migrating = 0;

	trace_ems_ontime_new_entity_load(task_of(se), &ontime->avg);
}

/****************************************************************/
/*				SYSFS				*/
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

	of_node_put(dn);
	return 0;
}
late_initcall(init_ontime);
