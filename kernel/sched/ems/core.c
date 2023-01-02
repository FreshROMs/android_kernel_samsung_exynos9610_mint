/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpuidle.h>
#include <linux/ems.h>
#include <linux/freezer.h>

#include "ems.h"
#include "../sched.h"
#include "../tune.h"

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

static
void sched_policy_get(struct tp_env *env)
{
	int policy = schedtune_task_sched_policy(p);
	struct task_struct *p = env->p;
	int src_cpu = env->src_cpu;

	/* 
	 * Mint additions - scheduling policy changes
	 * 
	 * Adapt to current schedtune setting for tasks with
	 * a. 'on-top' status - SCHED_POLICY_PERF
	 * b. global boost on boot - SCHED_POLICY_PERF
	 * c. 'prefer-perf' status - SCHED_POLICY_SEMI_PERF
	 * d. global boosted scenario - SCHED_POLICY_SEMI_PERF
	 *
	 */
	if (env->on_top) {
		env->sched_policy = SCHED_POLICY_PERF;
		return;
	}

	if (global_boosted_boot()) {
		env->sched_policy =  SCHED_POLICY_PERF;
		return;
	}

	if (env->prefer_perf && policy < SCHED_POLICY_SEMI_PERF) {
		env->sched_policy = SCHED_POLICY_SEMI_PERF;
		return;
	}

	if (global_boosted() && policy < SCHED_POLICY_SEMI_PERF) {
		env->sched_policy = SCHED_POLICY_SEMI_PERF;
		return;
	}

	/* Return policy at this point if scheduling on perf logic */
	if (policy >= SCHED_POLICY_SEMI_PERF) {
		env->sched_policy = policy;
		return;
	}

	/* Return min util policy if src_cpu is overutil */
	if (cpu_overutilized(capacity_orig_of(src_cpu), cpu_util(src_cpu)) && cpu_rq(src_cpu)->nr_running) {
		env->sched_policy = SCHED_POLICY_MIN_UTIL;
		return;
	}

	/*
	 * Change target tasks' policy to SCHED_POLICY_ENERGY
	 * for power optimization, if
	 * 1) target task's sched_policy is SCHED_POLICY_EFF and
	 *    its utilization is under 1.56% of SCHED_CAPACITY_SCALE.
	 * 2) tasks is worker thread.
	 */
	if (p->flags & PF_WQ_WORKER)
		policy = SCHED_POLICY_ENERGY;

	if (policy == SCHED_POLICY_EFF &&
		env->task_util <= SCHED_CAPACITY_SCALE >> 6)
		policy = SCHED_POLICY_ENERGY;

	/*
	 * On 4.14, energy and efficiency calculations
	 * fail when task has no utilization. Place task
	 * using min util strategy if this is the case.
	 */
	if (policy == SCHED_POLICY_ENERGY && !task_util(env->p)) {
		env->sched_policy = SCHED_POLICY_MIN_UTIL;
		return;
	}

	env->sched_policy = policy;
}

extern int wake_cap(struct task_struct *p, int cpu, int prev_cpu);
static bool is_cpu_preemptible(struct tp_env *env, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
#ifdef CONFIG_SCHED_TUNE
	struct task_struct *curr = READ_ONCE(rq->curr);
	int curr_adj;

	if (is_slowest_cpu(cpu) || !curr)
		goto skip_ux;

	curr_adj = curr->signal->oom_score_adj

	/* Allow preemption if not top-app */
	if (!schedtune_task_top_app(curr))
		goto skip_ux;

	/* Always avoid preempting the app in front of user */
	if (env->p != curr && curr_adj == 0)
		return false;

	/* Check if 'curr' is a prefer-perf top-app task */
	if (schedtune_prefer_perf(curr) > 0)
		return false;

skip_ux:
#endif
	if (env->sync && (rq->nr_running != 1 || wake_cap(env->p, cpu, env->src_cpu)))
		return false;

	return true;
}

static
int select_fit_cpus(struct tp_env *env)
{
	struct cpumask fit_cpus;
	struct cpumask cpus_allowed;
	struct cpumask ontime_fit_cpus, prefer_cpus, overutil_cpus, non_preemptible_cpus, busy_cpus, free_cpus;
	struct task_struct *p = env->p;
	int cpu;

	unsigned long task_util = max(task_util_est(p), (unsigned long) 1);
	unsigned long min_util = boosted_task_util(p);

	/* Check if task is misfit - causes all CPUs to be over capacity */
	int misfit = (task_util > (SCHED_CAPACITY_SCALE * MISFIT_TASK_UTIL_RATIO / 100));

	/* Clear masks */
	cpumask_clear(&env->fit_cpus);
	cpumask_clear(&fit_cpus);
	cpumask_clear(&ontime_fit_cpus);
	cpumask_clear(&prefer_cpus);
	cpumask_clear(&overutil_cpus);
	cpumask_clear(&non_preemptible_cpus);
	cpumask_clear(&busy_cpus);
	cpumask_clear(&free_cpus);

	/*
	 * Make cpus allowed task assignment.
	 * The fit cpus are determined among allowed cpus of task
	 */
	cpumask_and(&cpus_allowed, &env->p->cpus_allowed, cpu_active_mask);
	if (cpumask_empty(&cpus_allowed)) {
		/* no cpus allowed, give up on selecting cpus */
		return 0;
	}

	/* Take snapshot of task and CPU utilization with and without task */
	env->task_util = task_util;
	env->min_util = min_util;

	for_each_cpu(cpu, &cpus_allowed) {
		unsigned long cpu_util = cpu_util_without(cpu, p);
		unsigned long new_util = cpu_util + task_util;
		new_util = max(new_util, min_util);

		env->cpu_stat[cpu].util_wo = cpu_util;
		env->cpu_stat[cpu].util_with = new_util;

		env->cpu_stat[cpu].cap_max = capacity_max_of(cpu);
		env->cpu_stat[cpu].cap_orig = capacity_orig_of(cpu);
		env->cpu_stat[cpu].cap_curr = capacity_curr_of(cpu);

		if (cpu_equal(env->src_cpu, cpu))
			env->cpu_stat[cpu].util = new_util;
		else
			env->cpu_stat[cpu].util = cpu_util;

		/* cumulative CPU demand */
		env->cpu_stat[cpu].util_cuml =
			min(env->cpu_stat[cpu].util + min_util, capacity_orig_of(cpu));
		if (task_in_cum_window_demand(cpu_rq(cpu), env->p))
			env->cpu_stat[cpu].util_cuml -= task_util;

		/* idle cpu check */
		env->cpu_stat[cpu].idle = idle_cpu(cpu);
	}

	/* Get cpus where fits task from ontime migration */
	ontime_select_fit_cpus(p, &ontime_fit_cpus);

	/* Get preferred cpus based on service task algorithm */
	prefer_cpu_get(env, &prefer_cpus);

	/*
	 * Find cpus to be overutilized.
	 * If utilization of cpu with given task exceeds cpu capacity, it is
	 * overutilized, but excludes cpu without running task because cpu is likely
	 * to become idle.
	 *
	 * overutil_cpus = cpu util + task util > cpu capacity
	 */
	for_each_cpu(cpu, &cpus_allowed) {
		/*
		 * A misfit task is likely to cause all cpus to become over capacity,
		 * so it is likely to fail proper scheduling. In case of misfit task,
		 * only cpu that meet the following conditions:
		 *
		 *  - the cpu with big enough capacity that can handle misfit task
		 *  - the cpu not busy (util is under 80% of capacity)
		 *  - the cpu with empty runqueue.
		 *
		 * Other cpus are considered overcapacity.
		 */
		if (misfit) {
			unsigned long capacity = env->cpu_stat[cpu].cap_orig;

			if (task_util > capacity ||
			    cpu_overutilized(capacity, env->cpu_stat[cpu].util_wo) ||
			    cpu_rq(cpu)->nr_running)
				cpumask_clear_cpu(cpu, &overutil_cpus);

			continue;
		}

		if (env->cpu_stat[cpu].util_with >= env->cpu_stat[cpu].cap_orig && cpu_rq(cpu)->nr_running)
			cpumask_set_cpu(cpu, &overutil_cpus);
	}

	/*
	 * Find non-preemptible cpus.
	 * These cpus likely hold latency-sensitive tasks that should not be preempted
	 * by miscellaneous tasks as it may cause UX stutter. Check if cpu holds a
	 * non-preemptible task and avoid placement in them.
	 *
	 * non_preemptible_cpus = cpu holding on-top or prefer-perf(top-app) task excluding prev_cpu
	 */

	/* Allow preemption if task is the 'on-top' task */
	if (env->on_top)
		goto skip_non_preemptible;

	/* Allow preemption if task is sync and 'prefer-perf' task */
	if (env->sync && env->prefer_perf)
		goto skip_non_preemptible;

	for_each_cpu(cpu, &cpus_allowed) {
		if (cpu_equal(cpu, env->src_cpu) || is_cpu_preemptible(env, cpu))
			continue;

		cpumask_set_cpu(cpu, &non_preemptible_cpus);
	}

skip_non_preemptible:
	/*
	 * Find busy cpus.
	 * If this function is called by ontime migration(env->wake == 0),
	 * it looks for busy cpu to exclude from selection. Utilization of cpu
	 * exceeds 12.5% of cpu capacity, it is defined as busy cpu.
	 * (12.5% : this percentage is heuristically obtained)
	 *
	 * busy_cpus = cpu util >= 12.5% of cpu capacity
	 */
	if (!env->wake) {
		for_each_cpu(cpu, &cpus_allowed) {
			int threshold = env->cpu_stat[cpu].cap_orig >> 3;

			if (env->cpu_stat[cpu].util_with >= threshold)
				cpumask_set_cpu(cpu, &busy_cpus);
		}

		goto combine_cpumask;
	}

	/*
	 * Find free cpus.
	 * Free cpus is used as an alternative when all cpus allowed become
	 * overutilized. Define significantly low-utilized cpus(< 3%) as
	 * free cpu.
	 *
	 * free_cpus = cpu util < 3% of cpu capacity
	 */
	if (unlikely(cpumask_equal(&cpus_allowed, &overutil_cpus))) {
		for_each_cpu(cpu, &cpus_allowed) {
			int threshold = env->cpu_stat[cpu].cap_orig >> 5;

			if (env->cpu_stat[cpu].util < threshold)
				cpumask_set_cpu(cpu, &free_cpus);
		}
	}

combine_cpumask:
	/*
	 * To select cpuset where task fits, each cpumask is combined as
	 * below sequence:
	 *
	 * 1) Pick ontime fit cpus if ontime is applicable
	 * 2) Apply prefer cpu if it is applicable
	 * 3) Exclude overutil_cpu from fit cpus.
	 *    The utilization of cpu with given task is overutilized, the cpu
	 *    cannot process the task properly then performance drop. therefore,
	 *    overutil_cpu is excluded.
	 * 4) Exclude non-preemptible cpus from fit cpus
	 *
	 *    fit_cpus = cpus_allowed & ontime_fit_cpus & prefer_cpus & ~overutil_cpus & ~non_preemptible_cpus
	 */
	cpumask_copy(&fit_cpus, &cpus_allowed);
	/* Pick ontime fit cpus if ontime is applicable */
	if (cpumask_intersects(&fit_cpus, &ontime_fit_cpus))
		cpumask_and(&fit_cpus, &fit_cpus, &ontime_fit_cpus);

	/* Apply prefer cpu if it is applicable */
	if (cpumask_intersects(&fit_cpus, &prefer_cpus))
		cpumask_and(&fit_cpus, &fit_cpus, &prefer_cpus);

	cpumask_andnot(&fit_cpus, &fit_cpus, &overutil_cpus);
	if (!cpumask_equal(&cpus_allowed, &non_preemptible_cpus))
		cpumask_andnot(&fit_cpus, &fit_cpus, &non_preemptible_cpus);

	/*
	 * Case: task migration
	 *
	 * 5) Exclude busy cpus if task migration.
	 *    To improve performance, do not select busy cpus in task
	 *    migration.
	 *
	 *    fit_cpus = fit_cpus & ~busy_cpus
	 */
	if (!env->wake) {
		cpumask_andnot(&fit_cpus, &fit_cpus, &busy_cpus);
		goto finish;
	}

	/*
	 * Case: task wakeup
	 *
	 * 5) Select free cpus if cpus allowed are overutilized.
	 *    If all cpus allowed are overutilized when it assigns given task
	 *    to cpu, select free cpus as an alternative.
	 *
	 *    fit_cpus = free_cpus
	 */
	if (unlikely(cpumask_empty(&fit_cpus)))
		cpumask_copy(&fit_cpus, &free_cpus);

	/*
	 * 5) Select cpus allowed if no cpus where fits task.
	 *
	 *    fit_cpus = cpus_allowed
	 */
	if (cpumask_empty(&fit_cpus))
		cpumask_copy(&fit_cpus, &cpus_allowed);

finish:
	cpumask_copy(&env->fit_cpus, &fit_cpus);

	trace_ems_select_fit_cpus(env->p, env->wake,
		*(unsigned int *)cpumask_bits(&env->fit_cpus),
		*(unsigned int *)cpumask_bits(&cpus_allowed),
		*(unsigned int *)cpumask_bits(&ontime_fit_cpus),
		*(unsigned int *)cpumask_bits(&prefer_cpus),
		*(unsigned int *)cpumask_bits(&overutil_cpus),
		*(unsigned int *)cpumask_bits(&non_preemptible_cpus),
		*(unsigned int *)cpumask_bits(&busy_cpus),
		*(unsigned int *)cpumask_bits(&free_cpus));

	return cpumask_weight(&env->fit_cpus);
}

static void get_ready_env(struct tp_env *env)
{
	int cpu;

	if (env->sched_policy == SCHED_POLICY_SEMI_PERF)
		rcu_read_lock();

	/* Get available idle cpu count */
	env->idle_cpu_count = 0;
	for_each_cpu(cpu, &env->fit_cpus) {
		struct cpuidle_state *idle;

		if (env->cpu_stat[cpu].idle) {
			if (env->sched_policy == SCHED_POLICY_SEMI_PERF) {
				/* Get idle state for semi perf selection */
				idle = idle_get_state(cpu_rq(cpu));

				if (idle)
					env->cpu_stat[cpu].exit_latency = idle->exit_latency;
				else
					env->cpu_stat[cpu].exit_latency = 0;
			}

			env->idle_cpu_count++;
		}
	}

	if (env->sched_policy == SCHED_POLICY_SEMI_PERF)
		rcu_read_unlock();
}

static
void select_start_cpu(struct tp_env *env) {
	int start_cpu = cpumask_first_and(cpu_slowest_mask(), cpu_active_mask);
	struct cpumask active_fast_mask;

	int prefer_perf = max(env->prefer_perf, env->on_top);
	int src_cpu = env->src_cpu;

	// Avoid recommending fast CPUs during idle as these are inactive
	if (pm_freezing)
		goto done;

	/* Get all active fast CPUs */
	cpumask_and(&active_fast_mask, cpu_fastest_mask(), cpu_active_mask);

	/* Start with fast CPU if available, task is allowed to be placed, and matches criteria */
	if (!cpumask_empty(&active_fast_mask) && cpumask_intersects(tsk_cpus_allowed(env->p), &active_fast_mask)) {
		/* Return fast CPU if task is prefer_perf or global boosting */
		if (prefer_perf || global_boosted()) {
			start_cpu = cpumask_first(&active_fast_mask);
			goto done;
		}

		/* 
		 * Check if task can be placed on big cluster and
		 * fits slowest CPU. Return fast if overutil.
		 */
		if (task_util_est(env->p) * 100 >= capacity_max_of(src_cpu) * 61) {
			start_cpu = cpumask_first(&active_fast_mask);
			goto done;
		}
	}

done:
	env->start_cpu.cpu = start_cpu;
	env->start_cpu.cap_max = capacity_max_of(start_cpu);
}

static
bool fast_path_eligible(struct tp_env *env)
{	
	/* Cond 1: Previous CPU should be eligible for placement */
	if (!cpumask_test_cpu(env->src_cpu, tsk_cpus_allowed(env->p)))
		return false;

	/* Cond 2: Previous CPU must be active */
	if (!cpu_active(env->src_cpu))
		return false;

	/* Cond 3: Previous CPU capacity must be same as start CPU capacity */
	if (env->start_cpu.cap_max != capacity_max_of(env->src_cpu))
		return false;

	/* Cond 4: Previous CPU should be idle */
	if (!idle_cpu(env->src_cpu))
		return false;

	/* Cond 5: Previous CPU must not be overutilized */
	if (cpu_overutilized(capacity_orig_of(env->src_cpu), cpu_util(env->src_cpu)))
		return false;

	/* Cond 6: Previous CPU must be shallow idle */
	if (idle_get_state_idx(cpu_rq(env->src_cpu)) <= 1) {
		return true;
	}

	return false;
}

extern void sync_entity_load_avg(struct sched_entity *se);
int exynos_select_task_rq(struct task_struct *p, int prev_cpu, int sd_flag, int sync, int wake)
{
	int target_cpu = INVALID_CPU;
	int nr_cpus = 0;
	char state[30] = "fail";
	struct tp_env env = {
		.p = p,
		.cgroup_idx = schedtune_task_group_idx(p),
		.src_cpu = prev_cpu,

		.boost = schedtune_task_boost(p),
		.on_top = schedtune_task_on_top(p),
		.prefer_idle = schedtune_prefer_idle(p),
		.prefer_perf = schedtune_prefer_perf(p),

		.sync = sync,
		.wake = wake,
	};

	/*
	 * Update utilization of waking task to apply "sleep" period
	 * before selecting cpu.
	 */
	if (!(sd_flag & SD_BALANCE_FORK))
		sync_entity_load_avg(&p->se);

	/* Get start CPU for the task */
	select_start_cpu(&env);

	/*
	 * Check for fast placement eligibility - keep on prev_cpu
	 */
	if (fast_path_eligible(&env)) {
		target_cpu = prev_cpu;
		strcpy(state, "fast");
		goto out;
	}

	nr_cpus = select_fit_cpus(&env);
	if (nr_cpus == 0) {
		/*
		 * If there are no fit cpus, give up on choosing rq and keep
		 * the task on the prev cpu
		 */
		target_cpu = prev_cpu;
		strcpy(state, "no fit cpu");
		goto out;
	} else if (nr_cpus == 1) {
		/* Only one cpu is fit. Select this cpu. */
		target_cpu = cpumask_any(&env.fit_cpus);
		strcpy(state, "one fit cpu");
		goto out;
	}

	/* Set scheduling policy */
	sched_policy_get(&env);

	/* Handle sync flag */
	if (sysctl_sched_sync_hint_enable && sync) {
		target_cpu = smp_processor_id();

		/*
		 * On 4.14, energy and efficiency calculations fail on waking tasks.
		 * Unless task is placed using perf or min util logic,
		 * wake task on the cpu the selection is running if it is preemptible.
		 */
		if (env.sched_policy < SCHED_POLICY_SEMI_PERF &&
			cpumask_test_cpu(target_cpu, tsk_cpus_allowed(p)) &&
			is_cpu_preemptible(&env, target_cpu)) {
			strcpy(state, "sync wakeup");
			goto out;
		}

		/*
		 * Select waker cpu if it does not belong to slowest cpumask.
		 */
		if (cpu_rq(target_cpu)->nr_running == 1 &&
			cpumask_test_cpu(target_cpu, &env.fit_cpus)) {
			struct cpumask mask;

			cpumask_andnot(&mask, cpu_active_mask, cpu_slowest_mask());
			if (cpumask_test_cpu(target_cpu, &mask)) {
				strcpy(state, "boosted sync wakeup");
				goto out;
			}
		}
	}

	if (!env.wake) {
		struct cpumask mask;

		/*
		 * 'wake = 0' indicates that running task attempt to migrate to
		 * faster cpu by ontime migration. Therefore, if there are no
		 * fit faster cpus, give up on choosing rq.
		 */
		cpumask_and(&mask, cpu_coregroup_mask(prev_cpu), &env.fit_cpus);
		cpumask_andnot(&env.fit_cpus, &env.fit_cpus, &mask);
		cpumask_andnot(&env.fit_cpus, &env.fit_cpus, cpu_slowest_mask());

		if (!cpumask_weight(&env.fit_cpus)) {
			target_cpu = INVALID_CPU;
			strcpy(state, "no fit for migration");
			goto out;
		}
	}

	/*
	 * Get ready to find best cpu.
	 * Depending on the state of the task, the candidate cpus and C/E
	 * weight are decided.
	 */
	get_ready_env(&env);
	target_cpu = find_best_cpu(&env);
	if (cpu_selected(target_cpu))
		goto found_best_cpu;

out:
	trace_ems_wakeup_balance(p, target_cpu, wake, state);
found_best_cpu:
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
