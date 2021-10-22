/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/ems.h>

#define CREATE_TRACE_POINTS
#include <trace/events/ems.h>

#include "ems.h"
#include "../sched.h"
#include "../tune.h"

unsigned long cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

#ifdef CONFIG_SCHED_WALT
	if (likely(!walt_disabled && sysctl_sched_use_walt_cpu_util)) {
		u64 walt_cpu_util = cpu_rq(cpu)->cumulative_runnable_avg;

		walt_cpu_util <<= SCHED_CAPACITY_SHIFT;
		do_div(walt_cpu_util, walt_ravg_window);

		return min_t(unsigned long, walt_cpu_util,
			     capacity_orig_of(cpu));
	}
#endif

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	if (sched_feat(UTIL_EST))
		util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

unsigned long task_util(struct task_struct *p)
{
	if (rt_task(p))
		return p->rt.avg.util_avg;
	else
		return p->se.avg.util_avg;
}

unsigned long cpu_util_wake(int cpu, struct task_struct *p)
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
	if (sched_feat(UTIL_EST))
		util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

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
			if (lbt_overutilized(src_cpu, level) || global_boosted())
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

static int select_proper_cpu(struct task_struct *p, int prev_cpu)
{
	int cpu;
	unsigned long best_min_util = ULONG_MAX;
	int best_cpu = -1;

	for_each_cpu(cpu, cpu_active_mask) {
		int i;

		/* visit each coregroup only once */
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		/* skip if task cannot be assigned to coregroup */
		if (!cpumask_intersects(&p->cpus_allowed, cpu_coregroup_mask(cpu)))
			continue;

		for_each_cpu_and(i, tsk_cpus_allowed(p), cpu_coregroup_mask(cpu)) {
			unsigned long capacity_orig = capacity_orig_of(i);
			unsigned long wake_util, new_util;

			wake_util = cpu_util_wake(i, p);
			new_util = wake_util + task_util_est(p);
			new_util = max(new_util, boosted_task_util(p));

			/* skip over-capacity cpu */
			if (new_util > capacity_orig)
				continue;

			/*
			 * Best target) lowest utilization among lowest-cap cpu
			 *
			 * If the sequence reaches this function, the wakeup task
			 * does not require performance and the prev cpu is over-
			 * utilized, so it should do load balancing without
			 * considering energy side. Therefore, it selects cpu
			 * with smallest cpapacity and the least utilization among
			 * cpu that fits the task.
			 */
			if (best_min_util < new_util)
				continue;

			best_min_util = new_util;
			best_cpu = i;
		}

		/*
		 * if it fails to find the best cpu in this coregroup, visit next
		 * coregroup.
		 */
		if (cpu_selected(best_cpu))
			break;
	}

	trace_ems_select_proper_cpu(p, best_cpu, best_min_util);

	/*
	 * if it fails to find the vest cpu, choosing any cpu is meaningless.
	 * Return prev cpu.
	 */
	return cpu_selected(best_cpu) ? best_cpu : prev_cpu;
}

extern void sync_entity_load_avg(struct sched_entity *se);

int exynos_wakeup_balance(struct task_struct *p, int prev_cpu, int sd_flag, int sync)
{
	int target_cpu = -1;
	char state[30] = "fail";

	/*
	 * Since the utilization of a task is accumulated before sleep, it updates
	 * the utilization to determine which cpu the task will be assigned to.
	 * Exclude new task.
	 */
	if (!(sd_flag & SD_BALANCE_FORK)) {
		unsigned long old_util = task_util(p);

		sync_entity_load_avg(&p->se);
		/* update the band if a large amount of task util is decayed */
		update_band(p, old_util);
	}

	target_cpu = select_service_cpu(p);
	if (cpu_selected(target_cpu)) {
		strcpy(state, "service");
		goto out;
	}

	/*
	 * Priority 1 : ontime task
	 *
	 * If task which has more utilization than threshold wakes up, the task is
	 * classified as "ontime task" and assigned to performance cpu. Conversely,
	 * if heavy task that has been classified as ontime task sleeps for a long
	 * time and utilization becomes small, it is excluded from ontime task and
	 * is no longer guaranteed to operate on performance cpu.
	 *
	 * Ontime task is very sensitive to performance because it is usually the
	 * main task of application. Therefore, it has the highest priority.
	 */
	target_cpu = ontime_task_wakeup(p, sync);
	if (cpu_selected(target_cpu)) {
		strcpy(state, "ontime migration");
		goto out;
	}

	/*
	 * Priority 2 : prefer-perf
	 *
	 * Prefer-perf is a function that operates on cgroup basis managed by
	 * schedtune. When perfer-perf is set to 1, the tasks in the group are
	 * preferentially assigned to the performance cpu.
	 *
	 * It has a high priority because it is a function that is turned on
	 * temporarily in scenario requiring reactivity(touch, app laucning).
	 */
	target_cpu = prefer_perf_cpu(p);
	if (cpu_selected(target_cpu)) {
		strcpy(state, "prefer-perf");
		goto out;
	}

	/*
	 * Priority 3 : task band
	 *
	 * The tasks in a process are likely to interact, and its operations are
	 * sequential and share resources. Therefore, if these tasks are packed and
	 * and assign on a specific cpu or cluster, the latency for interaction
	 * decreases and the reusability of the cache increases, thereby improving
	 * performance.
	 *
	 * The "task band" is a function that groups tasks on a per-process basis
	 * and assigns them to a specific cpu or cluster. If the attribute "band"
	 * of schedtune.cgroup is set to '1', task band operate on this cgroup.
	 */
	target_cpu = band_play_cpu(p);
	if (cpu_selected(target_cpu)) {
		strcpy(state, "task band");
		goto out;
	}

	/*
	 * Priority 4 : global boosting
	 *
	 * Global boost is a function that preferentially assigns all tasks in the
	 * system to the performance cpu. Unlike prefer-perf, which targets only
	 * group tasks, global boost targets all tasks. So, it maximizes performance
	 * cpu utilization.
	 *
	 * Typically, prefer-perf operates on groups that contains UX related tasks,
	 * such as "top-app" or "foreground", so that major tasks are likely to be
	 * assigned to performance cpu. On the other hand, global boost assigns
	 * all tasks to performance cpu, which is not as effective as perfer-perf.
	 * For this reason, global boost has a lower priority than prefer-perf.
	 */
	target_cpu = global_boosting(p);
	if (cpu_selected(target_cpu)) {
		strcpy(state, "global boosting");
		goto out;
	}

	/*
	 * Priority 5 : prefer-idle
	 *
	 * Prefer-idle is a function that operates on cgroup basis managed by
	 * schedtune. When perfer-idle is set to 1, the tasks in the group are
	 * preferentially assigned to the idle cpu.
	 *
	 * Prefer-idle has a smaller performance impact than the above. Therefore
	 * it has a relatively low priority.
	 */
	target_cpu = prefer_idle_cpu(p);
	if (cpu_selected(target_cpu)) {
		strcpy(state, "prefer-idle");
		goto out;
	}

	/*
	 * Priority 6 : energy cpu
	 *
	 * A scheduling scheme based on cpu energy, find the least power consumption
	 * cpu with energy table when assigning task.
	 */
	target_cpu = select_energy_cpu(p, prev_cpu, sd_flag, sync);
	if (cpu_selected(target_cpu)) {
		strcpy(state, "energy cpu");
		goto out;
	}

	/*
	 * Priority 7 : proper cpu
	 *
	 * If the task failed to find a cpu to assign from the above conditions,
	 * it means that assigning task to any cpu does not have performance and
	 * power benefit. In this case, select cpu for balancing cpu utilization.
	 */
	target_cpu = select_proper_cpu(p, prev_cpu);
	if (cpu_selected(target_cpu))
		strcpy(state, "proper cpu");

out:
	trace_ems_wakeup_balance(p, target_cpu, state);
	return target_cpu;
}

struct kobject *ems_kobj;

static int __init init_sysfs(void)
{
	ems_kobj = kobject_create_and_add("ems", kernel_kobj);
	if (!ems_kobj)
		return -ENOMEM;

	return 0;
}
core_initcall(init_sysfs);
