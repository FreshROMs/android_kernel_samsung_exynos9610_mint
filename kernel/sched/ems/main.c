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
#include "../tune.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ems.h>

static inline int
check_cpu_capacity(struct rq *rq, struct sched_domain *sd)
{
	return ((rq->cpu_capacity * sd->imbalance_pct) <
				(rq->cpu_capacity_orig * 100));
}

#define lb_sd_parent(sd) \
	(sd->parent && sd->parent->groups != sd->parent->groups->next)

/******************************************************************************
 * common function for ems                                                    *
 ******************************************************************************/
inline int cpu_overutilized(int cpu)
{
    return (capacity_orig_of(cpu) * 1024) < (ml_cpu_util(cpu) * 1280);
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

#define SYSTEMUI_THREAD_NAME "ndroid.systemui"
int ems_task_on_top(struct task_struct *p)
{
	/* No tasks are on top while device is sleeping */
	if (pm_freezing)
		return 0;

	if (p->signal->oom_score_adj != 0 && strncmp(p->comm, SYSTEMUI_THREAD_NAME, 15))
		return 0;

	/* Don't touch kthreads */
	if (p->flags & PF_KTHREAD)
		return 0;

	return schedtune_task_group_idx(p) == CGROUP_TOPAPP;
}

int ems_task_boosted(struct task_struct *p)
{
	/* honor uclamp boost flag */
	if (uclamp_boosted(p))
		return 1;

	/* ems task boost */
	if (p->pid && ems_task_boost() == p->pid)
		return 1;

	return ems_global_task_boost(schedtune_task_group_idx(p));
}

/******************************************************************************
 * main function for ems                                                      *
 ******************************************************************************/
int ems_need_active_balance(enum cpu_idle_type idle, struct sched_domain *sd,
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
			if (ems_lbt_overutilized(src_cpu, level) || ems_global_boost() || ems_boot_boost())
				return 1;
	}

	if ((src_cap * src_imb_pct < dst_cap * dst_imb_pct) &&
			cpu_rq(src_cpu)->cfs.h_nr_running == 1 &&
			ems_lbt_overutilized(src_cpu, level) &&
			!ems_lbt_overutilized(dst_cpu, level)) {
		return 1;
	}

	return unlikely(sd->nr_balance_failed > sd->cache_nice_tries + 2);
}

int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sync, int wake)
{
	int cpu;

	cpu = __ems_select_task_rq_fair(p, prev_cpu, sync, wake);

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
	if (cpu_active(src_cpu) && !cpu_overutilized(src_cpu))
		return 0;

	/* avoid migration if ontime does not allow */
	if (!ontime_can_migrate_task(p, dst_cpu))
		return 0;

	/* avoid migrating on-top task to slow cpu */
	if (ems_task_on_top(p) && et_cpu_slowest(dst_cpu))
		return 0;

	/* avoid migration for tex task */
	if (tex_task(p))
		return 0;

	/* avoid migrating tex supressed task to fast cpu */
	if (tex_suppress_task(p) && cpumask_test_cpu(dst_cpu, cpu_fastest_mask()))
		return 0;

	return 1;
}

void ems_tick(struct rq *rq)
{
	profile_sched_data();

	monitor_sysbusy();
	somac_tasks();

	ontime_migration();
}

void ems_tick_locked(struct rq *rq)
{
	mlt_set_period_start(rq);
	mlt_update_recent(rq);
}

void ems_enqueue_task(struct rq *rq, struct task_struct *p)
{
	mlt_enqueue_task(rq);

	tex_enqueue_task(p, cpu_of(rq));
}

void ems_dequeue_task(struct rq *rq, struct task_struct *p)
{
	mlt_dequeue_task(rq);

	tex_dequeue_task(p, cpu_of(rq));
}

void ems_wakeup_task(struct rq *rq, struct task_struct *p)
{
	mlt_wakeup_task(rq);
}

void ems_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				struct sched_entity **se_ptr, bool *repick,
				bool simple, struct task_struct *prev)
{
    struct task_struct *p = NULL;

    if (!list_empty(ems_qjump_list(rq))) {
        p = ems_qjump_first_entry(ems_qjump_list(rq));
        *p_ptr = p;
        *se_ptr = &p->se;

        list_move_tail(ems_qjump_list(rq), ems_qjump_node(p));
        *repick = true;
    }
}

/* If EMS allows load balancing, return 0 */
int ems_load_balance(struct rq *rq)
{
	if (sysbusy_on_somac())
		return -EBUSY;

	return 0;
}

void ems_post_init_entity_util_avg(struct sched_entity *se)
{
	ntu_apply(se);
}

void ems_fork_init(struct task_struct *p)
{
	ems_qjump_queued(p) = 0;
	INIT_LIST_HEAD(ems_qjump_node(p));
}

int ems_check_preempt_wakeup(struct task_struct *p)
{
	if (p->pid && ems_task_boost() == p->pid)
		return 1;

	if (ems_task_on_top(p))
		return 1;

	return 0;
}

static
void qjump_rq_list_init(void)
{
	int cpu;

	for_each_cpu(cpu, cpu_possible_mask)
		INIT_LIST_HEAD(ems_qjump_list(cpu_rq(cpu)));
}

struct kobject *ems_kobj;

static
int __init init_sysfs(void)
{
	ems_kobj = kobject_create_and_add("ems", kernel_kobj);
	if (!ems_kobj) {
		pr_err("failed to create sysfs for ems\n");
		return -ENOMEM;
	}

	sysbusy_sysfs_init();
	core_init();

	return 0;
}
core_initcall(init_sysfs);

void __init ems_init(void) {

	mlt_init();
	qjump_rq_list_init();
	profile_sched_init();
	sysbusy_init();
}
