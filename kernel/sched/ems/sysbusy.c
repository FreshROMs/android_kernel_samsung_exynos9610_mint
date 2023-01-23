/*
 * Scheduling in busy system feature for Exynos Mobile Scheduler (EMS)
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Choonghoon Park <choong.park@samsung.com>
 */

#include <linux/ems.h>

#include "../sched.h"
#include "../tune.h"
#include "ems.h"

#include <trace/events/ems.h>

struct sysbusy {
	raw_spinlock_t lock;
	u64 last_update_time;
	u64 monitor_interval;
	u64 release_start_time;
	enum sysbusy_state state;
	struct work_struct work;
} sysbusy;

struct somac_env {
	struct rq		*dst_rq;
	int			dst_cpu;
	struct rq		*src_rq;
	int			src_cpu;
	struct task_struct	*target_task;
};
DEFINE_PER_CPU(struct somac_env, somac_env);

struct sysbusy_stat {
	int count;
	u64 last_time;
	u64 time_in_state;
} sysbusy_stats[NUM_OF_SYSBUSY_STATE];

struct somac_ready {
	int run;
	struct task_struct *p;
	struct cpumask cpus;
} somac_ready;

static int sysbusy_initialized;

/******************************************************************************
 * sysbusy notify                                                             *
 ******************************************************************************/
static RAW_NOTIFIER_HEAD(sysbusy_chain);

static int sysbusy_notify(int next_state, int event)
{
	int ret;

	ret = raw_notifier_call_chain(&sysbusy_chain, event, &next_state);

	return notifier_to_errno(ret);
}

int sysbusy_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&sysbusy_chain, nb);
}
EXPORT_SYMBOL_GPL(sysbusy_register_notifier);

int sysbusy_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&sysbusy_chain, nb);
}
EXPORT_SYMBOL_GPL(sysbusy_unregister_notifier);

/******************************************************************************
 *			            SOMAC			              *
 ******************************************************************************/
static DEFINE_SPINLOCK(somac_ready_lock);

int is_somac_ready(struct tp_env *env)
{
	struct cpumask mask;

	if (env->p != somac_ready.p)
		return 0;

	cpumask_clear(&mask);

	cpumask_and(&env->cpus_allowed, cpu_active_mask, &somac_ready.cpus);
	cpumask_and(&env->cpus_allowed, &env->cpus_allowed, env->p->cpus_ptr);
	if (cpumask_empty(&env->cpus_allowed))
		return 0;

	cpumask_set_cpu(cpumask_last(&env->cpus_allowed), &mask);
	cpumask_and(&env->cpus_allowed, &env->cpus_allowed, &mask);

	return 1;
}

static void update_somac_ready(struct tp_env *env)
{
	unsigned long flags;

	if (!somac_ready.run)
		return;

	if (env->cgroup_idx != CGROUP_TOPAPP ||
		env->p->wake_q_count != NR_CPUS)
		return;

	spin_lock_irqsave(&somac_ready_lock, flags);
	if (!somac_ready.p || env->p->pid < somac_ready.p->pid)
		somac_ready.p = env->p;
	spin_unlock_irqrestore(&somac_ready_lock, flags);
}

int sysbusy_on_somac(void)
{
	return sysbusy.state == SYSBUSY_SOMAC;
}

static int decision_somac_task(struct task_struct *p)
{
	if (p == somac_ready.p)
		return 0;

	return schedtune_task_group_idx(p) == CGROUP_TOPAPP;
}

static struct task_struct *pick_somac_task(struct rq *rq)
{
	struct task_struct *p, *heaviest_task = NULL;
	unsigned long util, max_util = 0;
	int task_count = 0;

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (decision_somac_task(p)) {
			util = ml_task_util(p);
			if (util > max_util) {
				heaviest_task = p;
				max_util = util;
			}
		}

		if (++task_count >= TRACK_TASK_COUNT)
			break;
	}

	return heaviest_task;
}

static bool can_move_task(struct task_struct *p, struct somac_env *env)
{
	struct rq *src_rq = env->src_rq;
	int src_cpu = env->src_cpu;

	if (p->exit_state)
		return false;

	if (unlikely(src_rq != task_rq(p)))
		return false;

	if (unlikely(src_cpu != smp_processor_id()))
		return false;

	if (src_rq->nr_running <= 1)
		return false;

	if (!cpumask_test_cpu(env->dst_cpu, p->cpus_ptr))
		return false;

	if (task_running(env->src_rq, p))
		return false;

	return true;
}

static int
move_somac_task(struct task_struct *target, struct somac_env *env)
{
	struct list_head *tasks = &env->src_rq->cfs_tasks;
	struct task_struct *p, *n;

	list_for_each_entry_safe(p, n, tasks, se.group_node) {
		if (p != target)
			continue;

		p->on_rq = TASK_ON_RQ_MIGRATING;
		deactivate_task(env->src_rq, p, 0);
		set_task_cpu(p, env->dst_cpu);

		activate_task(env->dst_rq, p, 0);
		p->on_rq = TASK_ON_RQ_QUEUED;
		check_preempt_curr(env->dst_rq, p, 0);

		return 1;
	}

	return 0;
}

static int somac_cpu_stop(void *data)
{
	struct somac_env *env = data;
	struct rq *src_rq, *dst_rq;
	struct task_struct *p;
	int src_cpu, dst_cpu;

	/* Initialize environment data */
	src_rq = env->src_rq;
	dst_rq = env->dst_rq = cpu_rq(env->dst_cpu);
	src_cpu = env->src_cpu = env->src_rq->cpu;
	dst_cpu = env->dst_cpu;
	p = env->target_task;

	if (!cpumask_test_cpu(src_cpu, cpu_active_mask) ||
	    !cpumask_test_cpu(dst_cpu, cpu_active_mask))
		return -1;

	raw_spin_lock_irq(&src_rq->lock);

	/* Check task can be migrated */
	if (!can_move_task(p, env))
		goto out_unlock;

	BUG_ON(src_rq == dst_rq);

	/* Move task from source to destination */
	double_lock_balance(src_rq, dst_rq);
	if (move_somac_task(p, env)) {
		trace_sysbusy_somac(p, ml_task_util(p), src_cpu, dst_cpu);
	}
	double_unlock_balance(src_rq, dst_rq);

out_unlock:

	src_rq->active_balance = 0;

	raw_spin_unlock_irq(&src_rq->lock);
	put_task_struct(p);

	return 0;
}

static unsigned long last_somac;
static int somac_interval = 1; /* 1tick = 4ms */
static DEFINE_SPINLOCK(somac_lock);
DEFINE_PER_CPU(struct cpu_stop_work, somac_work);

static int __somac_tasks(int src_cpu, int dst_cpu,
				struct rq *src_rq, struct rq *dst_rq)
{
	struct somac_env *env = &per_cpu(somac_env, src_cpu);
	unsigned long flags;
	struct task_struct *p;

	if (!cpumask_test_cpu(src_cpu, cpu_active_mask) ||
	    !cpumask_test_cpu(dst_cpu, cpu_active_mask))
		return -1;

	raw_spin_lock_irqsave(&src_rq->lock, flags);
	if (src_rq->active_balance) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return -1;
	}

	if (!src_rq->cfs.curr) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return -1;
	}

	p = pick_somac_task(src_rq);
	if (!p) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return -1;
	}

	get_task_struct(p);

	env->dst_cpu = dst_cpu;
	env->src_rq = src_rq;
	env->target_task = p;

	src_rq->active_balance = 1;

	raw_spin_unlock_irqrestore(&src_rq->lock, flags);

	stop_one_cpu_nowait(src_cpu, somac_cpu_stop, env,
			&per_cpu(somac_work, src_cpu));

	return 0;
}

void somac_tasks(void)
{
	int slow_cpu, fast_cpu, cpu;
	unsigned long now = jiffies;
	struct rq *slow_rq, *fast_rq;
	int busy_cpu = -1, idle_cpu = -1;
	unsigned long flags;

	if (!sysbusy_initialized)
		return;

	if (!spin_trylock_irqsave(&somac_lock, flags))
		return;

	raw_spin_lock(&sysbusy.lock);
	if (!sysbusy_on_somac()) {
		raw_spin_unlock(&sysbusy.lock);
		goto unlock;
	}
	raw_spin_unlock(&sysbusy.lock);

	if (now < last_somac + somac_interval)
		goto unlock;

	if (cpumask_weight(cpu_active_mask) != NR_CPUS)
		goto unlock;

	for_each_online_cpu(cpu) {
		if (cpu_rq(cpu)->nr_running > 1)
			busy_cpu = cpu;
		if (!cpu_rq(cpu)->nr_running)
			idle_cpu = cpu;
	}

	if (cpu_selected(busy_cpu) && cpu_selected(idle_cpu)) {
		slow_rq = cpu_rq(busy_cpu);
		fast_rq = cpu_rq(idle_cpu);

		/* MIGRATE TASK BUSY -> IDLE */
		__somac_tasks(busy_cpu, idle_cpu, slow_rq, fast_rq);
		goto out;
	}

	for_each_cpu(cpu, cpu_coregroup_mask(0)) {
		slow_cpu = cpu;
		fast_cpu = cpu + 4;

		slow_rq = cpu_rq(slow_cpu);
		fast_rq = cpu_rq(fast_cpu);

		/* MIGRATE TASK SLOW -> FAST */
		if (__somac_tasks(slow_cpu, fast_cpu, slow_rq, fast_rq))
			continue;

		slow_cpu = (fast_cpu - 1) % 4;
		slow_rq = cpu_rq(slow_cpu);

		/* MIGRATE TASK FAST -> SLOW */
		__somac_tasks(fast_cpu, slow_cpu, fast_rq, slow_rq);
	}

out:
	last_somac = now;
unlock:
	spin_unlock_irqrestore(&somac_lock, flags);
}

/******************************************************************************
 *			       sysbusy schedule			              *
 ******************************************************************************/
int sysbusy_activated(void)
{
	return sysbusy.state > SYSBUSY_STATE0;
}

static unsigned long
get_remained_task_util(int cpu, struct tp_env *env)
{
	struct task_struct *p, *excluded = env->p;
	struct rq *rq = cpu_rq(cpu);
	unsigned long util = capacity_orig_of(cpu), util_sum = 0;
	int task_count = 0;
	unsigned long flags;
	int need_rq_lock = !excluded->on_rq || cpu != task_cpu(excluded);

	if (need_rq_lock)
		raw_spin_lock_irqsave(&rq->lock, flags);

	if (!rq->cfs.curr) {
		util = 0;
		goto unlock;
	}

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (p == excluded)
			continue;

		util = ml_task_util(p);
		util_sum += util;

		if (++task_count >= TRACK_TASK_COUNT)
			break;
	}

unlock:
	if (need_rq_lock)
		raw_spin_unlock_irqrestore(&rq->lock, flags);

	return min_t(unsigned long, util_sum, capacity_orig_of(cpu));
}

static int sysbusy_find_fastest_cpu(struct tp_env *env)
{
	struct cpumask mask;
	int cpu;

	cpumask_and(&mask, cpu_fastest_mask(), &env->cpus_allowed);
	if (cpumask_empty(&mask))
		cpu = cpumask_last(&env->cpus_allowed);
	else
		cpu = cpumask_any_distribute(&mask);

	if (!cpu_rq(cpu)->nr_running)
		return cpu;

	return check_busy(env->cpu_stat[cpu].util_wo,
				env->cpu_stat[cpu].cap_orig) ? -1 : cpu;
}

static int sysbusy_find_min_util_cpu(struct tp_env *env)
{
	int cpu, min_cpu = -1;
	unsigned long min_util = INT_MAX;

	for_each_cpu(cpu, &env->cpus_allowed) {
		unsigned long cpu_util;

		cpu_util = env->cpu_stat[cpu].util_wo;

		if (cpu == task_cpu(env->p) && !cpu_util)
			cpu_util = get_remained_task_util(cpu, env);

		/* find min util cpu with rt util */
		if (cpu_util <= min_util) {
			min_util = cpu_util;
			min_cpu = cpu;
		}
	}

	return min_cpu;
}

static enum sysbusy_state determine_sysbusy_state(void)
{
	struct system_profile_data system_data;

	get_system_sched_data(&system_data);

	/* Determine STATE2 or STATE3 */
	if (check_busy(system_data.heavy_task_util_sum, et_get_max_capacity())) {
		/* Determine STATE3 */
		if (system_data.misfit_task_count > (NR_CPUS / 2))
			return SYSBUSY_STATE3;

		return SYSBUSY_STATE2;
	}

	/* Determine STATE1 */
	if (system_data.busy_cpu_count >= 1) {
		if (check_busy(system_data.heavy_task_util_sum, system_data.cpu_util_sum) || 
			system_data.misfit_task_count >= 1) {
			return SYSBUSY_STATE1;
		}
	}

	/* not sysbusy or no heavy task */
	return SYSBUSY_STATE0;
}

static void
update_sysbusy_stat(int old_state, int next_state, unsigned long now)
{
	sysbusy_stats[old_state].count++;
	sysbusy_stats[old_state].time_in_state +=
		now - sysbusy_stats[old_state].last_time;

	sysbusy_stats[next_state].last_time = now;
}

static void change_sysbusy_state(int next_state, unsigned long now)
{
	int old_state = sysbusy.state;
	struct sysbusy_param *param;
	unsigned long flags;

	if (old_state == next_state) {
		sysbusy.release_start_time = 0;
		return;
	}

	param = &sysbusy_params[old_state];
	if (!test_bit(next_state, &param->allowed_next_state))
		return;

	/* release sysbusy */
	if (next_state == SYSBUSY_STATE0) {
		if (!sysbusy.release_start_time)
			sysbusy.release_start_time = now;

		if (now < sysbusy.release_start_time + param->release_duration)
			return;

		spin_lock_irqsave(&somac_ready_lock, flags);
		somac_ready.p = NULL;
		spin_unlock_irqrestore(&somac_ready_lock, flags);
	}

	sysbusy.monitor_interval = sysbusy_params[next_state].monitor_interval;
	sysbusy.release_start_time = 0;
	sysbusy.state = next_state;

	schedule_work(&sysbusy.work);
	update_sysbusy_stat(old_state, next_state, now);
	trace_sysbusy_state(old_state, next_state);
}

void monitor_sysbusy(void)
{
	enum sysbusy_state next_state;
	unsigned long now = jiffies;
	unsigned long flags;

	if (!sysbusy_initialized)
		return;

	if (!raw_spin_trylock_irqsave(&sysbusy.lock, flags))
		return;

	if (now < sysbusy.last_update_time + sysbusy.monitor_interval) {
		raw_spin_unlock_irqrestore(&sysbusy.lock, flags);
		return;
	}

	sysbusy.last_update_time = now;

	next_state = determine_sysbusy_state();

	change_sysbusy_state(next_state, now);
	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);
}

int sysbusy_schedule(struct tp_env *env)
{
	int target_cpu = -1;

	if (!sysbusy_initialized)
		return target_cpu;

	switch (sysbusy.state) {
	case SYSBUSY_STATE1:
		if (is_heavy_task_util(env->task_util))
			target_cpu = sysbusy_find_fastest_cpu(env);
		break;
	case SYSBUSY_STATE2:
		target_cpu = sysbusy_find_min_util_cpu(env);
		break;
	case SYSBUSY_STATE3:
		if (is_heavy_task_util(env->task_util)) {
			if (cpumask_test_cpu(task_cpu(env->p), &env->cpus_allowed))
				target_cpu = task_cpu(env->p);
		}
		else
			target_cpu = sysbusy_find_min_util_cpu(env);

		update_somac_ready(env);
		break;
	case SYSBUSY_STATE0:
	default:
		;
	}

	return target_cpu;
}

/**********************************************************************
 *			    SYSFS support			      *
 **********************************************************************/
static struct kobject *sysbusy_kobj;

static ssize_t show_somac_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", somac_interval);
}

static ssize_t store_somac_interval(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	somac_interval = input;

	return count;
}

static struct kobj_attribute somac_interval_attr =
__ATTR(somac_interval, 0644, show_somac_interval, store_somac_interval);

static ssize_t show_sysbusy_stat(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, count = 0;

	for (i = SYSBUSY_STATE0; i < NUM_OF_SYSBUSY_STATE; i++)
		count += snprintf(buf + count, PAGE_SIZE - count,
				"[state%d] count:%d time_in_state=%ums\n",
				i, sysbusy_stats[i].count,
				jiffies_to_msecs(sysbusy_stats[i].time_in_state));

	return count;
}

static struct kobj_attribute sysbusy_stat_attr =
__ATTR(sysbusy_stat, 0644, show_sysbusy_stat, NULL);

static ssize_t show_somac_ready_run(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", somac_ready.run);
}

static ssize_t store_somac_ready_run(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	somac_ready.run = input;

	return count;
}

static struct kobj_attribute somac_ready_run_attr =
__ATTR(somac_ready_run, 0644, show_somac_ready_run, store_somac_ready_run);

static ssize_t show_somac_ready_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%*pbl\n", cpumask_pr_args(&somac_ready.cpus));
}

#define STR_LEN (6)
static ssize_t store_somac_ready_cpus(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char str[STR_LEN];

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%s", str))
		return -EINVAL;

	cpulist_parse(buf, &somac_ready.cpus);

	return count;
}

static struct kobj_attribute somac_ready_cpus_attr =
__ATTR(somac_ready_cpus, 0644, show_somac_ready_cpus, store_somac_ready_cpus);

int sysbusy_sysfs_init(void)
{
	int ret;

	sysbusy_kobj = kobject_create_and_add("sysbusy", ems_kobj);
	if (!sysbusy_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		sysbusy_initialized = 1;
		return -EINVAL;
	}

	ret = sysfs_create_file(sysbusy_kobj, &somac_interval_attr.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	ret = sysfs_create_file(sysbusy_kobj, &sysbusy_stat_attr.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	ret = sysfs_create_file(sysbusy_kobj, &somac_ready_run_attr.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	ret = sysfs_create_file(sysbusy_kobj, &somac_ready_cpus_attr.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	sysbusy_initialized = 1;
	return ret;
}

/**********************************************************************
 *			    Initialization			      *
 **********************************************************************/
static void sysbusy_boost_fn(struct work_struct *work)
{
	int cur_state;
	unsigned long flags;

	raw_spin_lock_irqsave(&sysbusy.lock, flags);
	cur_state = sysbusy.state;
	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);

	if (cur_state > SYSBUSY_STATE0) {
		/*
		 * if sysbusy is activated, notifies whether it is
		 * okay to trigger sysbusy boost.
		 */
		if (sysbusy_notify(cur_state, SYSBUSY_CHECK_BOOST))
			return;
	}

	sysbusy_notify(cur_state, SYSBUSY_STATE_CHANGE);
}

static int __init sysbusy_parse_dt(struct device_node *dn)
{
	const char *buf;

	if (of_property_read_u32(dn, "somac-ready-run", &somac_ready.run))
		return -ENODATA;

	if (of_property_read_string(dn, "somac-ready-cpus", &buf))
		return -ENODATA;

	cpulist_parse(buf, &somac_ready.cpus);

	return 0;
}

int sysbusy_init(void)
{
	int ret;
	struct device_node *dn;

	sysbusy_initialized = 0;

	raw_spin_lock_init(&sysbusy.lock);
	INIT_WORK(&sysbusy.work, sysbusy_boost_fn);

	dn = of_find_node_by_path("/cpus/ems/sysbusy");
	if (!dn) {
		pr_err("failed to find device node for sysbusy\n");
		return -EINVAL;
	}

	ret = sysbusy_parse_dt(dn);
	if (ret)
		pr_err("failed to parse from dt for sysbusy\n");

	return ret;
}
