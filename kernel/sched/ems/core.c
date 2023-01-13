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

	return schedtune_util_est_en(p) ? max(ue.ewma, ue.enqueued)
					: task_util(p);
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

extern int wake_cap(struct task_struct *p, int cpu, int prev_cpu);
static bool is_cpu_preemptible(struct tp_env *env, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
#ifdef CONFIG_SCHED_TUNE
	struct task_struct *curr = READ_ONCE(rq->curr);
	int curr_adj;

	if (is_slowest_cpu(cpu) || !curr)
		goto skip_ux;

	/* Don't preempt EMS boosted task */
	if (curr->pid && ems_task_boost() == curr->pid)
		return false;

	curr_adj = curr->signal->oom_score_adj;

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
void find_overcap_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;
	unsigned long util = env->task_util;

	/* check if task is misfit - causes all CPUs to be over capacity */
	if (util > (SCHED_CAPACITY_SCALE * MISFIT_TASK_UTIL_RATIO / 100))
		goto misfit;

	/*
	 * Find cpus that becomes over capacity with a given task.
	 *
	 * overcap_cpus = cpu capacity < cpu util + task util
	 */
	for_each_cpu(cpu, &env->cpus_allowed)
		if (env->cpu_stat[cpu].util_with > env->cpu_stat[cpu].cap_orig)
			cpumask_set_cpu(cpu, mask);

	return;

misfit:
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
	cpumask_copy(mask, &env->cpus_allowed);
	for_each_cpu(cpu, &env->cpus_allowed) {
		unsigned long capacity = env->cpu_stat[cpu].cap_orig;

		if (util <= capacity &&
		    !cpu_overutilized(capacity, env->cpu_stat[cpu].util) &&
		    !cpu_rq(cpu)->nr_running)
			cpumask_clear_cpu(cpu, mask);
	}
}

static
void find_free_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;

	/*
	 * Free cpus is used as an alternative when all cpus allowed become
	 * overutilized. Define significantly low-utilized cpus(< 3%) as
	 * free cpu.
	 *
	 * free_cpus = cpu util < 3% of cpu capacity
	 */
	for_each_cpu(cpu, &env->cpus_allowed) {
		int threshold = env->cpu_stat[cpu].cap_orig >> 5;

		if (env->cpu_stat[cpu].util < threshold)
			cpumask_set_cpu(cpu, mask);
	}
}

static void find_busy_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;

	/* only find busy cpus for migrating tasks */
	if (env->wake)
		return;

	/*
	 * If this function is called by ontime migration(env->wake == 0),
	 * it looks for busy cpu to exclude from selection. Utilization of cpu
	 * exceeds 12.5% of cpu capacity, it is defined as busy cpu.
	 * (12.5% : this percentage is heuristically obtained)
	 *
	 * busy_cpus = cpu util >= 12.5% of cpu capacity
	 */
	for_each_cpu(cpu, &env->cpus_allowed) {
		int threshold = env->cpu_stat[cpu].cap_orig >> 3;

		if (env->cpu_stat[cpu].util_with >= threshold)
			cpumask_set_cpu(cpu, mask);
	}
}

static
void find_migration_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;

	/* skip if waking under min util policy */
	if (env->wake && env->sched_policy == SCHED_POLICY_MIN_UTIL)
		return;

	/*
	 * It looks for busy cpu to exclude from selection. If cpu is under
	 * boosted ontime migration, it is defined as migration cpu.
	 *
	 * migration_cpus = cpu with boosted ontime migration
	 */
	for_each_cpu(cpu, &env->cpus_allowed) {
		/* only avoid migration to fast cpus */
		if (is_slowest_cpu(cpu))
			continue;

		if (cpu_rq(cpu)->ontime_boost_migration)
			cpumask_set_cpu(cpu, mask);
	}
}

static
void find_non_preemptible_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;

	/* Allow preemption if task is the 'on-top' or task_boost task */
	if (env->on_top || (env->p->pid && ems_task_boost() == env->p->pid))
		return;

	/* Allow preemption if task is sync and 'prefer-perf' task */
	if (env->sync && env->prefer_perf)
		return;

	for_each_cpu(cpu, &env->cpus_allowed) {
		if (!is_cpu_preemptible(env, cpu))
			cpumask_set_cpu(cpu, mask);
	}

	/* if all cpus are non-preemptible, allow preemption */
	if (cpumask_equal(mask, &env->cpus_allowed))
		cpumask_clear(mask);
}

static
int find_fit_cpus(struct tp_env *env)
{
	struct cpumask fit_cpus;
	struct cpumask overcap_cpus, ontime_fit_cpus, prefer_cpus, busy_cpus, migration_cpus, non_preemptible_cpus, free_cpus;
	int non_overcap_cpu;
	int cpu = smp_processor_id();

	if (EMS_PF_GET(env->p) & EMS_PF_RUNNABLE_BUSY) {
		cpu = find_min_load_cpu(env);
		if (cpu >= 0) {
			cpumask_set_cpu(cpu, &env->fit_cpus);
			goto out;
		}
	}

	/* clear masks */
	cpumask_clear(&env->fit_cpus);
	cpumask_clear(&fit_cpus);
	cpumask_clear(&overcap_cpus);
	cpumask_clear(&ontime_fit_cpus);
	cpumask_clear(&prefer_cpus);
	cpumask_clear(&busy_cpus);
	cpumask_clear(&migration_cpus);
	cpumask_clear(&non_preemptible_cpus);
	cpumask_clear(&free_cpus);

	/*
	 * take a snapshot of cpumask to get fit cpus
	 * - overcap_cpus : overcap cpus
	 * - ontime_fit_cpus : ontime fit cpus
	 * - prefer_cpus : prefer cpu
	 * - busy_cpus : busy cpu for migrating tasks
	 * - migration_cpus : cpu under boosted ontime migration
	 * - non_preemptible_cpus : non preemptible cpu
	 */
	find_overcap_cpus(env, &overcap_cpus);
	ontime_select_fit_cpus(env->p, &ontime_fit_cpus);
	prefer_cpu_get(env, &prefer_cpus);
	find_busy_cpus(env, &busy_cpus);
	find_migration_cpus(env, &migration_cpus);
	find_non_preemptible_cpus(env, &non_preemptible_cpus);

	/*
	 * Exclude overcap cpu from cpus_allowed. If there is only one or no
	 * fit cpus, it does not need to find fit cpus anymore.
	 */
	cpumask_andnot(&fit_cpus, &env->cpus_allowed, &overcap_cpus);
	non_overcap_cpu = cpumask_weight(&fit_cpus);
	if (non_overcap_cpu == 1)
		goto out;

	/*
	 * Select free cpus if cpus allowed are overutilized.
	 * If all cpus allowed are overutilized when it assigns given task
	 * to cpu, select free cpus as an alternative.
	 */
	if (env->wake && unlikely(non_overcap_cpu == 0)) {
		find_free_cpus(env, &free_cpus);
		cpumask_copy(&fit_cpus, &free_cpus);
		if (cpumask_weight(&fit_cpus) <= 1)
			goto out;
	}

	/* Exclude busy cpus from fit_cpus if migrating */
	cpumask_andnot(&fit_cpus, &fit_cpus, &busy_cpus);
	if (cpumask_weight(&fit_cpus) <= 1)
		goto out;

	/* Exclude cpus under boosted ontime migration from fit_cpus */
	cpumask_andnot(&fit_cpus, &fit_cpus, &migration_cpus);
	if (cpumask_weight(&fit_cpus) <= 1)
		goto out;

	/* Exclude non-preemptible cpus from fit_cpus */
	cpumask_andnot(&fit_cpus, &fit_cpus, &non_preemptible_cpus);
	if (cpumask_weight(&fit_cpus) <= 1)
		goto out;

	/* Ensure heavy on-top task is running on fast cpu */
	if ((env->on_top && !is_slowest_cpu(env->start_cpu.cpu)) || (env->p->pid && ems_task_boost() == env->p->pid)) {
		if (cpumask_intersects(&fit_cpus, cpu_fastest_mask())) {
			cpumask_and(&fit_cpus, &fit_cpus, cpu_fastest_mask());
			goto out;
		}
	}

	/* Handle sync flag if waker cpu is preemptible */
	if (sysctl_sched_sync_hint_enable &&
		env->sync && (env->on_top || env->prefer_perf || is_cpu_preemptible(env, cpu))) {
		/*
		 * On 4.14, energy and efficiency calculations fail on waking tasks.
		 * Unless task is placed using perf or min util logic,
		 * wake task on the cpu the selection is running if it is preemptible.
		 */
		if (env->sched_policy < SCHED_POLICY_SEMI_PERF &&
			cpumask_test_cpu(cpu, &env->cpus_allowed)) {

			cpumask_clear(&fit_cpus);
			cpumask_set_cpu(cpu, &fit_cpus);
			goto out;
		}

		if (cpu_rq(cpu)->nr_running == 1 &&
			cpumask_test_cpu(cpu, &fit_cpus)) {
			struct cpumask mask;

			/*
			 * Select this cpu if boost is activated and this
			 * cpu does not belong to slowest cpumask.
			 */
			cpumask_andnot(&mask, cpu_active_mask, cpu_slowest_mask());
			if (cpumask_test_cpu(cpu, &mask)) {
				cpumask_clear(&fit_cpus);
				cpumask_set_cpu(cpu, &fit_cpus);
				goto out;
			}
		}
	}

	/* Pick ontime fit cpus if ontime is applicable */
	if (cpumask_intersects(&fit_cpus, &ontime_fit_cpus))
		cpumask_and(&fit_cpus, &fit_cpus, &ontime_fit_cpus);

	/* Apply prefer cpu if it is applicable */
	if (cpumask_intersects(&fit_cpus, &prefer_cpus))
		cpumask_and(&fit_cpus, &fit_cpus, &prefer_cpus);

out:
	cpumask_copy(&env->fit_cpus, &fit_cpus);

	trace_ems_select_fit_cpus(env->p, env->wake,
		*(unsigned int *)cpumask_bits(&env->fit_cpus),
		*(unsigned int *)cpumask_bits(&env->cpus_allowed),
		*(unsigned int *)cpumask_bits(&ontime_fit_cpus),
		*(unsigned int *)cpumask_bits(&prefer_cpus),
		*(unsigned int *)cpumask_bits(&overcap_cpus),
		*(unsigned int *)cpumask_bits(&busy_cpus),
		*(unsigned int *)cpumask_bits(&migration_cpus),
		*(unsigned int *)cpumask_bits(&non_preemptible_cpus),
		*(unsigned int *)cpumask_bits(&free_cpus));

	return cpumask_weight(&env->fit_cpus);
}

static
void take_util_snapshot(struct tp_env *env)
{
	int cpu;

	/*
	 * We don't agree setting 0 for task util
	 * Because we do better apply active power of task
	 * when get the energy
	 */
	env->task_util = max(task_util_est(env->p), (unsigned long) 1);
	env->min_util = max(boosted_task_util(env->p), (unsigned long) 1);

	/* fill cpu util */
	for_each_cpu_and(cpu, &env->p->cpus_allowed, cpu_active_mask) {
		unsigned long cpu_util = cpu_util_without(cpu, env->p);
		unsigned long new_util = cpu_util + env->task_util;
		new_util = max(new_util, env->min_util);

		env->cpu_stat[cpu].cap_max = capacity_max_of(cpu);
		env->cpu_stat[cpu].cap_orig = capacity_orig_of(cpu);

		env->cpu_stat[cpu].util_wo = cpu_util;
		env->cpu_stat[cpu].util_with = new_util;

		if (cpu_equal(env->src_cpu, cpu))
			env->cpu_stat[cpu].util = new_util;
		else
			env->cpu_stat[cpu].util = cpu_util;

		/* cumulative CPU demand */
		env->cpu_stat[cpu].util_cuml =
			min(env->cpu_stat[cpu].util + env->min_util, capacity_orig_of(cpu));
		if (task_in_cum_window_demand(cpu_rq(cpu), env->p))
			env->cpu_stat[cpu].util_cuml -= env->task_util;

		env->cpu_stat[cpu].idle = idle_cpu(cpu);
	}
}

static
void sched_policy_get(struct tp_env *env)
{
	struct task_struct *p = env->p;
	int policy = schedtune_task_sched_policy(p);

	/* 
	 * Mint additions - scheduling policy changes
	 * 
	 * Adapt to current schedtune setting for tasks with
	 * a. 'on-top' status - SCHED_POLICY_PERF
	 * b. global boost on boot - SCHED_POLICY_PERF
	 * c. task boost - SCHED_POLICY_SEMI_PERF
	 * d. 'prefer-perf' status - SCHED_POLICY_SEMI_PERF
	 * e. global boosted scenario - SCHED_POLICY_SEMI_PERF
	 *
	 */
	if (env->on_top || ems_boot_boost() == EMS_INIT_BOOST) {
		policy = SCHED_POLICY_PERF;
		goto out;
	}

	if (ems_global_boost() && env->cgroup_idx == STUNE_TOPAPP) {
		policy = SCHED_POLICY_PERF;
		goto out;
	}

	if (policy >= SCHED_POLICY_PERF)
		goto out;

	if (env->p->pid && ems_task_boost() == env->p->pid) {
		policy = SCHED_POLICY_SEMI_PERF;
		goto out;
	}

	if (env->prefer_perf || (ems_global_boost() && env->cgroup_idx == STUNE_FOREGROUND) || ems_boot_boost() == EMS_BOOT_BOOST) {
		policy = SCHED_POLICY_SEMI_PERF;
		goto out;
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

	if (policy >= SCHED_POLICY_SEMI_PERF)
		goto out;

	if (policy == SCHED_POLICY_EFF &&
		env->task_util <= SCHED_CAPACITY_SCALE >> 6)
		policy = SCHED_POLICY_ENERGY;

	/*
	 * On 4.14, energy and efficiency calculations
	 * fail when task has no utilization. Place task
	 * using min util strategy if this is the case.
	 */
	if (policy == SCHED_POLICY_ENERGY && !task_util(env->p))
		policy = SCHED_POLICY_MIN_UTIL;

out:
	env->sched_policy = policy;
}

static
bool fast_path_eligible(struct tp_env *env)
{	
	/* Cond 1: Previous CPU should be eligible for placement */
	if (!cpumask_test_cpu(env->src_cpu, &env->cpus_allowed))
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

static
void select_start_cpu(struct tp_env *env) {
	int start_cpu = cpumask_first_and(cpu_slowest_mask(), cpu_active_mask);
	int prefer_perf = max(env->prefer_perf, env->on_top);
	struct cpumask active_fast_mask;

	/* Avoid recommending fast CPUs during idle as these are inactive */
	if (pm_freezing)
		goto done;

	/* Don't select CPU if task is not allowed to be placed on fast CPUs */
	if (!cpumask_intersects(tsk_cpus_allowed(env->p), cpu_fastest_mask()))
		goto done;

	/* Get all active fast CPUs */
	cpumask_and(&active_fast_mask, cpu_fastest_mask(), cpu_active_mask);

	/* Don't select CPU if fast CPUs are unavailable */
	if (cpumask_empty(&active_fast_mask))
		goto done;

	/* Return fast CPU if task is prefer_perf or global boosting */
	if (prefer_perf || ems_global_boost() || ems_boot_boost()) {
		start_cpu = cpumask_first(&active_fast_mask);
		goto done;
	}

done:
	env->start_cpu.cpu = start_cpu;
	env->start_cpu.cap_max = capacity_max_of(start_cpu);
}

/*
 * Return number of CPUs allowed.
 */
static
int find_cpus_allowed(struct tp_env *env)
{
	/*
	 * take a snapshot of cpumask to get CPUs allowed
	 * - active_cpus : cpu_active_mask
	 * - cpus_allowed : p->cpus_allowed
	 */
	cpumask_and(&env->cpus_allowed, cpu_active_mask, tsk_cpus_allowed(env->p));

	/* Return weight of allowed cpus */
	return cpumask_weight(&env->cpus_allowed);
}

static
void get_ready_env(struct tp_env *env)
{
	int cpu, nr_cpus;

	/*
	 * If there is only one or no CPU allowed for a given task,
	 * it does not need to initialize env.
	 */
	if (find_cpus_allowed(env) <= 1)
		return;

	/* Get start CPU for the task */
	select_start_cpu(env);

	/* Check for fast placement eligibility */
	if (fast_path_eligible(env)) {
		/* Task is eligible for fast placement, keep the task on prev cpu */
		cpumask_clear(&env->cpus_allowed);
		cpumask_set_cpu(env->src_cpu, &env->cpus_allowed);
		return;
	}

	/* snapshot util to use same util during core selection */
	take_util_snapshot(env);

	/* snapshot scheduling policy to use in the process */
	sched_policy_get(env);

	/* Find fit cpus */
	nr_cpus = find_fit_cpus(env);
	if (nr_cpus == 0) {
		int prev_cpu = env->src_cpu;

		/* There is no fit cpus, keep the task on prev cpu */
		if (cpumask_test_cpu(prev_cpu, &env->cpus_allowed)) {
			cpumask_clear(&env->cpus_allowed);
			cpumask_set_cpu(prev_cpu, &env->cpus_allowed);
			return;
		}

		/*
		 * But prev cpu is not allowed, copy cpus_allowed to
		 * fit_cpus to find appropriate cpu among cpus_allowed.
		 */
		cpumask_copy(&env->fit_cpus, &env->cpus_allowed);
	} else if (nr_cpus == 1) {
		/* Only one cpu is fit. Select this cpu. */
		cpumask_clear(&env->cpus_allowed);
		cpumask_copy(&env->cpus_allowed, &env->fit_cpus);
		return;
	}

	/* Get available idle cpu count */
	env->idle_cpu_count = 0;

	/* 4.14 requires rcu locking to get idle state */
	if (env->sched_policy == SCHED_POLICY_SEMI_PERF)
		rcu_read_lock();

	for_each_cpu(cpu, &env->fit_cpus) {
		struct cpuidle_state *idle;

		if (env->cpu_stat[cpu].idle) {
			/* Get idle state for semi perf selection */
			if (env->sched_policy == SCHED_POLICY_SEMI_PERF) {	
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

extern void sync_entity_load_avg(struct sched_entity *se);
int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sd_flag, int sync, int wake)
{
	int target_cpu = INVALID_CPU;
	char state[30] = "fail";
	struct tp_env env = {
		.p = p,
		.cgroup_idx = schedtune_task_group_idx(p),
		.src_cpu = prev_cpu,

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

	/* Get ready environment variable */
	get_ready_env(&env);

	/* there is no CPU allowed, give up find new cpu */
	if (cpumask_empty(&env.cpus_allowed)) {
		strcpy(state, "no CPU allowed");
		goto out;
	}

	/* there is only one CPU allowed, no need to find cpu */
	if (cpumask_weight(&env.cpus_allowed) == 1) {
		target_cpu = cpumask_any(&env.cpus_allowed);
		strcpy(state, "one CPU allowed");
		goto out;
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

	target_cpu = find_best_cpu(&env);
	if (cpu_selected(target_cpu))
		goto found_best_cpu;
out:
	trace_ems_wakeup_balance(p, target_cpu, wake, state);
found_best_cpu:
	return target_cpu;
}

int ems_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);

	/* avoid migration if cpu is underutilized */
	if (cpu_active(src_cpu) && is_slowest_cpu(src_cpu) &&
		!cpu_overutilized(capacity_orig_of(src_cpu), cpu_util(src_cpu)))
		return 0;

	/* avoid migration if not ontime */
	if (!ontime_can_migration(p, dst_cpu))
		return 0;

	/* avoid migrating on-top and prefer-perf task to slow cpus */
	if (is_slowest_cpu(dst_cpu) && (schedtune_task_on_top(p) || schedtune_prefer_perf(p)))
		return 0;

	return 1;
}

void ems_tick(void)
{
	// profile_sched_data();

	// mlt_update(cpu_of(rq));

	// stt_update(rq, NULL);

	// frt_update_available_cpus(rq);

	// monitor_sysbusy();
	// somac_tasks();

	ontime_migration();
	// ecs_update();
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
