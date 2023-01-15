/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpuidle.h>
#include <linux/ems.h>
#include <linux/ems_service.h>
#include <linux/freezer.h>

#include "ems.h"
#include "../sched.h"

#ifndef CONFIG_UCLAMP_TASK_GROUP
#include "../tune.h"
#endif

#define CREATE_TRACE_POINTS
#include <trace/events/ems.h>

/*
 * When a task is dequeued, its estimated utilization should not be update if
 * its util_avg has not been updated at least once.
 * This flag is used to synchronize util_avg updates with util_est updates.
 * We map this information into the LSB bit of the utilization saved at
 * dequeue time (i.e. util_est.dequeued).
 */
#define UTIL_AVG_UNCHANGED 0x1

static
inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, ue.enqueued);
}

unsigned long cpu_util_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return cpu_util(cpu);

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	/* Discount task's blocked util from CPU's util */
	util -= min_t(unsigned int, util, task_util_est(p));

	/*
	 * Covered cases:
	 *
	 * a) if *p is the only task sleeping on this CPU, then:
	 *      cpu_util (== task_util) > util_est (== 0)
	 *    and thus we return:
	 *      cpu_util_wake = (cpu_util - task_util) = 0
	 *
	 * b) if other tasks are SLEEPING on this CPU, which is now exiting
	 *    IDLE, then:
	 *      cpu_util >= task_util
	 *      cpu_util > util_est (== 0)
	 *    and thus we discount *p's blocked utilization to return:
	 *      cpu_util_wake = (cpu_util - task_util) >= 0
	 *
	 * c) if other tasks are RUNNABLE on that CPU and
	 *      util_est > cpu_util
	 *    then we use util_est since it returns a more restrictive
	 *    estimation of the spare capacity on that CPU, by just
	 *    considering the expected utilization of tasks already
	 *    runnable on that CPU.
	 *
	 * Cases a) and b) are covered by the above code, while case c) is
	 * covered by the following code when estimated utilization is
	 * enabled.
	 */
	if (sched_feat(UTIL_EST)) {
		unsigned int estimated =
			READ_ONCE(cfs_rq->avg.util_est.enqueued);

		/*
		 * Despite the following checks we still have a small window
		 * for a possible race, when an execl's select_task_rq_fair()
		 * races with LB's detach_task():
		 *
		 *   detach_task()
		 *     p->on_rq = TASK_ON_RQ_MIGRATING;
		 *     ---------------------------------- A
		 *     deactivate_task()                   \
		 *       dequeue_task()                     + RaceTime
		 *         util_est_dequeue()              /
		 *     ---------------------------------- B
		 *
		 * The additional check on "current == p" it's required to
		 * properly fix the execl regression and it helps in further
		 * reducing the chances for the above race.
		 */
		if (unlikely(task_on_rq_queued(p) || current == p)) {
			estimated -= min_t(unsigned int, estimated,
					   (_task_util_est(p) | UTIL_AVG_UNCHANGED));
		}
		util = max(util, estimated);
	}

	/*
	 * Utilization (estimated) can exceed the CPU capacity, thus let's
	 * clamp to the maximum CPU capacity to ensure consistency with
	 * the cpu_util call.
	 */
	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

static inline int
check_cpu_capacity(struct rq *rq, struct sched_domain *sd)
{
	return ((rq->cpu_capacity * sd->imbalance_pct) <
				(rq->cpu_capacity_orig * 100));
}

#define lb_sd_parent(sd) \
	(sd->parent && sd->parent->groups != sd->parent->groups->next)

int exynos_need_active_balance(enum cpu_idle_type idle, struct sched_domain *sd,
					int src_cpu, int dst_cpu)
{
	unsigned int src_imb_pct = lb_sd_parent(sd) ? sd->imbalance_pct : 1;
	unsigned int dst_imb_pct = lb_sd_parent(sd) ? 100 : 1;
	unsigned long src_cap = capacity_of(src_cpu);
	unsigned long dst_cap = capacity_of(dst_cpu);
	int level = sd->level;

	/* dst_cpu is idle */
	if ((idle != CPU_NOT_IDLE) &&
	    (cpu_rq(src_cpu)->cfs.h_nr_running == 1)) {
		if ((check_cpu_capacity(cpu_rq(src_cpu), sd)) &&
		    (src_cap * sd->imbalance_pct < dst_cap * 100)) {
			return 1;
		}

		/* This domain is top and dst_cpu is bigger than src_cpu*/
		if (!lb_sd_parent(sd) && src_cap < dst_cap)
			if (lbt_overutilized(src_cpu, level) || ems_global_boost() || ems_boot_boost())
				return 1;
	}

	if ((src_cap * src_imb_pct < dst_cap * dst_imb_pct) &&
			cpu_rq(src_cpu)->cfs.h_nr_running == 1 &&
			lbt_overutilized(src_cpu, level) &&
			!lbt_overutilized(dst_cpu, level)) {
		return 1;
	}

	return unlikely(sd->nr_balance_failed > sd->cache_nice_tries + 2);
}

inline int cpu_overutilized(int cpu)
{
#if 0
    return (capacity_of(cpu) * 1024) < (ml_cpu_util(cpu) * 1280);
#else
    return (capacity_orig_of(cpu) * 1024) < (ml_cpu_util(cpu) * 1280);
#endif
}

inline int is_heavy_task_util(unsigned long util)
{
    return util > (SCHED_CAPACITY_SCALE * HEAVY_TASK_UTIL_RATIO / 100);
}

inline int is_misfit_task_util(unsigned long util)
{
    return util > (SCHED_CAPACITY_SCALE * MISFIT_TASK_UTIL_RATIO / 100);
}

inline bool is_busy_cpu(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned long util, runnable, capacity, nr_running;

	cfs_rq = &cpu_rq(cpu)->cfs;

	util = ml_cpu_util(cpu);
    runnable = ml_runnable_load_avg(cpu);
	capacity = capacity_orig_of(cpu);
	nr_running = cpu_rq(cpu)->nr_running;

	return __is_busy_cpu(util, runnable, capacity, nr_running);
}

/******************************************************************************
 * common function for ems                                                    *
 ******************************************************************************/
static const struct sched_class *sched_class_begin;
int get_sched_class_idx(const struct sched_class *class)
{
	const struct sched_class *c;
	int class_idx;

	for (c = sched_class_begin, class_idx = 0;
	     class_idx < NUM_OF_SCHED_CLASS;
	     c--, class_idx++)
		if (c == class)
			break;

	return 1 << class_idx;
}

int cpuctl_task_group_idx(struct task_struct *p)
{
	int idx;
	struct cgroup_subsys_state *css;

	rcu_read_lock();

	/* On 4.14, Android uses 'cpuset' */
	css = task_css(p, cpuset_cgrp_id);
	idx = css->id - 1;

	rcu_read_unlock();

	if (idx >= CGROUP_COUNT)
		return 0;

	return idx;
}

int ems_task_top_app(struct task_struct *p)
{
#ifdef CONFIG_UCLAMP_TASK_GROUP
	/* Don't touch kthreads */
	if (p->flags & PF_KTHREAD)
		return 0;

	return cpuctl_task_group_idx(p) == CGROUP_TOPAPP;
#else
	return schedtune_task_top_app(p);
#endif
}

#define SYSTEMUI_THREAD_NAME "ndroid.systemui"
int ems_task_on_top(struct task_struct *p)
{
#ifdef CONFIG_UCLAMP_TASK_GROUP
	if (p->signal->oom_score_adj != 0 && strncmp(p->comm, SYSTEMUI_THREAD_NAME, 15))
		return 0;

	/* Return if task is not an app */
	if (!is_app(p))
		return 0;

	return ems_task_top_app(p);
#else
	return schedtune_task_on_top(p);
#endif
}

int ems_task_boosted(struct task_struct *p)
{
	int cgroup_idx;

	if (uclamp_boosted(p))
		return 1;

	cgroup_idx = cpuctl_task_group_idx(p);
	return kpp_status(cgroup_idx);
}

/******************************************************************************
 * main function for ems                                                      *
 ******************************************************************************/
int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int sync, int wake)
{
	int cpu;

	cpu = __ems_select_task_rq_fair(p, prev_cpu, sd_flag, sync, wake);

	EMS_CPU(p) = cpu;
	return cpu;
}

int ems_select_fallback_rq(struct task_struct *p)
{
	int target_cpu = EMS_CPU(p);

	if (target_cpu < 0 || !cpu_active(target_cpu))
		target_cpu = -1;

	EMS_CPU(p) = -1;
	return target_cpu;
}

int ems_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);

	/* avoid migration if cpu is underutilized */
	if (cpu_active(src_cpu) && is_slowest_cpu(src_cpu) && !cpu_overutilized(src_cpu))
		return 0;

	/* avoid migration if ontime does not allow */
	if (!ontime_can_migrate_task(p, dst_cpu))
		return 0;

#if 0
	/* avoid migration for tex task */
	if (tex_task(p))
		return 0;
#endif

	/* avoid migrating prefer-perf task to slow cpus */
	if (is_slowest_cpu(dst_cpu) && ems_task_boosted(p))
		return 0;

	return 1;
}

void ems_idle_exit(int cpu, int state)
{
	// mlt_idle_exit(cpu);
}

void ems_idle_enter(int cpu, int *state)
{
	// mlt_idle_enter(cpu, *state);
}

void ems_tick(struct rq *rq)
{
	mlt_set_period_start(rq);

	// profile_sched_data();

	// mlt_update(cpu_of(rq));

	// stt_update(rq, NULL);

	// frt_update_available_cpus(rq);

	// monitor_sysbusy();
	// somac_tasks();

	ontime_migration();
	// ecs_update();
}

void ems_enqueue_task(struct rq *rq, struct task_struct *p)
{
	mlt_enqueue_task(rq);

	// stt_enqueue_task(rq, p);

	// tex_enqueue_task(p, cpu_of(rq));

	// freqboost_enqueue_task(p, cpu_of(rq), flags);
}

void ems_dequeue_task(struct rq *rq, struct task_struct *p)
{
	mlt_dequeue_task(rq);

	// stt_dequeue_task(rq, p);

	// tex_dequeue_task(p, cpu_of(rq));

	// freqboost_dequeue_task(p, cpu_of(rq), flags);
}

void ems_wakeup_task(struct rq *rq, struct task_struct *p)
{
	mlt_wakeup_task(rq);
}

void ems_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				struct sched_entity **se_ptr, bool *repick,
				bool simple, struct task_struct *prev)
{
#if 0
    struct task_struct *p = NULL;

    if (!list_empty(ems_qjump_list(rq))) {
        p = ems_qjump_first_entry(ems_qjump_list(rq));
        *p_ptr = p;
        *se_ptr = &p->se;

        list_move_tail(ems_qjump_list(rq), ems_qjump_node(p));
        *repick = true;
    }
#endif
}

/* If EMS allows load balancing, return 0 */
int ems_load_balance(struct rq *rq)
{
#if 0
	if (sysbusy_on_somac())
		return -EBUSY;
#endif

	return 0;
}

void ems_post_init_entity_util_avg(struct sched_entity *se)
{
	ntu_apply(se);
}

void ems_fork_init(struct task_struct *p)
{
#if 0
	ems_qjump_queued(p) = 0;
	INIT_LIST_HEAD(ems_qjump_node(p));
#endif
}

void ems_schedule(struct task_struct *prev,
		struct task_struct *next, struct rq *rq)
{
#if 0
	int state = RUNNING;

	if (prev == next)
		return;

	if (get_sched_class_idx(next->sched_class) == EMS_SCHED_IDLE)
		state = IDLE_START;
	else if (get_sched_class_idx(prev->sched_class) == EMS_SCHED_IDLE)
		state = IDLE_END;

	mlt_task_switch(rq->cpu, next, state);
#endif
}

int ems_check_preempt_wakeup(struct task_struct *p)
{
	if (p->pid && ems_task_boost() == p->pid)
		return 1;

	if (ems_task_on_top(p))
		return 1;

	return 0;
}

#if 0
static void qjump_rq_list_init(void)
{
	int cpu;

	for_each_cpu(cpu, cpu_possible_mask)
		INIT_LIST_HEAD(ems_qjump_list(cpu_rq(cpu)));
}
#endif

struct kobject *ems_kobj;

static int __init init_sysfs(void)
{
	ems_kobj = kobject_create_and_add("ems", kernel_kobj);
	if (!ems_kobj)
		return -ENOMEM;

	return 0;
}
core_initcall(init_sysfs);

void __init ems_init(void) {
	mlt_init();
}
