/*
 * Exynos Mobile Scheduler CPU selection
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <linux/ems.h>
#include <linux/freezer.h>

#include "ems.h"
#include "../sched.h"

#include <trace/events/ems.h>

static inline void
prev_cpu_advantage(unsigned long *cpu_util, unsigned long task_util)
{
	long util = *cpu_util;

	/*
	 * subtract cpu util by 12.5% of task util to give advantage to
	 * prev cpu when computing energy.
	 */
	util -= (task_util >> 3);
	*cpu_util = max(util, (long) 0);
}

int find_min_load_cpu(struct tp_env *env)
{
	int cpu, min_load_cpu = -1;
	unsigned long load, min_load = ULONG_MAX;

	for_each_cpu(cpu, &env->cpus_allowed) {
		load = env->cpu_stat[cpu].util;
		if (load < min_load) {
			min_load = load;
			min_load_cpu = cpu;
		}
	}

	return min_load_cpu;
}

static
int find_min_util_cpu(struct tp_env *env, const struct cpumask *mask, bool among_idle)
{
	int cpu, min_cpu = INVALID_CPU;
	unsigned long min_util = ULONG_MAX;

	for_each_cpu_and(cpu, &env->fit_cpus, mask) {
		unsigned long cpu_util;

		/*
		 * If among_idle is true, find min util cpu among idle cpu.
		 * Skip non-idle cpu.
		 */
		if (among_idle && !env->cpu_stat[cpu].idle)
			continue;

		/*
		* Skip processing placement further if we are visiting
		* cpus with lower capacity than start cpu
		*/
		if (pm_freezing || env->cpu_stat[cpu].idle)
			goto skip_cap;

		if (env->cpu_stat[cpu].cap_max < env->start_cpu.cap_max)
			continue;

skip_cap:
		cpu_util = env->cpu_stat[cpu].util_with;
		if (cpu_equal(env->src_cpu, cpu))
			prev_cpu_advantage(&cpu_util, env->task_util);

		if (cpu_util < min_util) {
			min_util = cpu_util;
			min_cpu = cpu;
		}
	}

	return min_cpu;
}

static
int find_min_util_with_cpu(struct tp_env *env)
{
	int cpu, min_cpu = INVALID_CPU, min_idle_cpu = INVALID_CPU;

	for_each_cpu(cpu, cpu_active_mask) {
		struct cpumask mask;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cpumask_and(&mask, cpu_coregroup_mask(cpu), &env->fit_cpus);
		if (cpumask_empty(&mask))
			continue;

		/* find idle cpu from slowest coregroup for task performance */
		if (env->idle_cpu_count && is_slowest_cpu(cpumask_first(cpu_coregroup_mask(cpu)))) {
			min_idle_cpu = find_min_util_cpu(env, &mask, true);
			if (min_idle_cpu >= 0)
				break;
		}

		min_cpu = find_min_util_cpu(env, &mask, false);

		/* Traverse next coregroup if no best cpu is found */
		if (min_cpu >= 0)
			break;
	}

	return (min_idle_cpu >= 0) ? min_idle_cpu : min_cpu;
}

/******************************************************************************
 * best efficiency cpu selection                                              *
 ******************************************************************************/
static
int __find_best_eff_cpu(struct tp_env *env, bool among_idle)
{
	int cpu, best_cpu = INVALID_CPU;
	unsigned int eff, best_eff = UINT_MAX;

	/*
	 * It is meaningless to find an energy cpu when the energy table is
	 * not created or has not been created yet.
	 */
	if (!get_energy_table_status())
		return find_min_util_cpu(env, &env->fit_cpus, among_idle);

	/* find best efficiency cpu */
	for_each_cpu(cpu, &env->fit_cpus) {

		/*
		 * If among_idle is true, find eff cpu among idle cpu.
		 * Skip non-idle cpu.
		 */
		if (among_idle && !env->cpu_stat[cpu].idle)
			continue;

		/*
		* Skip processing placement further if we are visiting
		* cpus with lower capacity than start cpu
		*/
		if (pm_freezing || env->cpu_stat[cpu].idle)
			goto skip_cap;

		if (env->cpu_stat[cpu].cap_max < env->start_cpu.cap_max)
			continue;

skip_cap:
		eff = calculate_efficiency(env, cpu);

		/*
		 * Give 6.25% margin to prev cpu efficiency.
		 * This margin means migration cost.
		 */
		if (cpu_equal(env->src_cpu, cpu))
			eff -= eff >> 4;

		/* 
		 * On 4.14, efficiency calculations go where:
		 * The LOWER result, the BETTER efficiency.
		 */
		if (eff < best_eff) {
			best_eff = eff;
			best_cpu = cpu;
		}
	}

	return best_cpu;
}

static
int find_best_eff_cpu(struct tp_env *env)
{
	int best_cpu = INVALID_CPU;

	if (env->idle_cpu_count > 0 && env->latency_sensitive)
		best_cpu = __find_best_eff_cpu(env, true);

	if (!cpu_selected(best_cpu))
		best_cpu = __find_best_eff_cpu(env, false);

	return best_cpu;
}

static int
__find_energy_cpu(struct tp_env *env, const struct cpumask *candidates)
{
	int cpu, energy_cpu = INVALID_CPU, min_util = INT_MAX;
	unsigned int min_energy = UINT_MAX;

	/* find energy cpu */
	for_each_cpu(cpu, candidates) {
		int cpu_util = env->cpu_stat[cpu].util_with;
		unsigned int energy;

		/* calculate system energy */
		energy = calculate_energy(env, cpu);

		/* find min_energy cpu */
		if (energy < min_energy)
			goto energy_cpu_found;
		if (energy > min_energy)
			continue;

		/* find min_util cpu if energy is same */
		if (cpu_util >= min_util)
			continue;

energy_cpu_found:
		min_energy = energy;
		energy_cpu = cpu;
		min_util = cpu_util;
	}

	return energy_cpu;
}

static
int find_energy_cpu(struct tp_env *env)
{
	struct cpumask candidates;
	int cpu, min_cpu, energy_cpu, adv_energy_cpu = INVALID_CPU;

	/*
	 * It is meaningless to find an energy cpu when the energy table is
	 * not created or has not been created yet.
	 */
	if (!get_energy_table_status())
		return find_min_util_cpu(env, &env->fit_cpus, false);

	/* set candidates cpu to find energy cpu */
	cpumask_clear(&candidates);

	for_each_cpu(cpu, cpu_active_mask) {
		struct cpumask mask;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cpumask_and(&mask, cpu_coregroup_mask(cpu), &env->fit_cpus);
		if (cpumask_empty(&mask))
			continue;

		min_cpu = find_min_util_cpu(env, &mask, false);
		if (min_cpu >= 0)
			cpumask_set_cpu(min_cpu, &candidates);
	}

	if (cpumask_weight(&candidates) == 1) {
		energy_cpu = cpumask_any(&candidates);
		goto out;
	}

	/* find min energy cpu */
	energy_cpu = __find_energy_cpu(env, &candidates);

	/*
	 * Slowest cpumask is usually the coregroup that includes the boot
	 * processor(cpu0), has low power consumption but also low performance
	 * efficiency. If selected cpu belongs to slowest cpumask and task is
	 * tiny enough not to increase system energy, reselect min energy cpu
	 * among idle cpu within slowest cpumask for faster task processing.
	 * (tiny task criteria = task util < 12.5% of slowest cpu capacity)
	 */
	if (cpumask_test_cpu(energy_cpu, cpu_slowest_mask()) &&
	    env->task_util < (capacity_orig_of(0) >> 3))
		adv_energy_cpu = find_min_util_cpu(env,
					cpu_slowest_mask(), true);

out:
	if (adv_energy_cpu >= 0)
		return adv_energy_cpu;

	return energy_cpu;
}

/******************************************************************************
 * best performance cpu selection                                             *
 ******************************************************************************/
static
int find_max_spare_cpu(struct tp_env *env, bool among_idle)
{
	int cpu, best_cpu = INVALID_CPU;
	unsigned long max_spare_cap = 0;
	unsigned long min_cuml_util = ULONG_MAX;

	/* find maximum spare cpu */
	for_each_cpu(cpu, &env->fit_cpus) {
		unsigned long curr_cap, spare_cap;

		/* among_idle is true, find max spare cpu among idle cpu */
		if (among_idle && !env->cpu_stat[cpu].idle)
			continue;

		curr_cap = env->cpu_stat[cpu].cap_curr;
		spare_cap = curr_cap - env->cpu_stat[cpu].util_with;

		if (max_spare_cap >= spare_cap)
			continue;

		/*
		 * If utilization is the same between active CPUs,
		 * break the ties with cumulative demand,
		 * also prefer lower order cpu.
		 */
		if (spare_cap == max_spare_cap &&
			env->cpu_stat[cpu].util_cuml > min_cuml_util)
			continue;

		min_cuml_util = env->cpu_stat[cpu].util_cuml;
		max_spare_cap = spare_cap;
		best_cpu = cpu;
	}

	return best_cpu;
}

static
int find_best_perf_cpu(struct tp_env *env)
{
	int best_cpu = INVALID_CPU;

	if (env->idle_cpu_count > 0 && env->latency_sensitive)
		best_cpu = find_max_spare_cpu(env, true);

	if (!cpu_selected(best_cpu))
		best_cpu = find_max_spare_cpu(env, false);

	return best_cpu;
}

static
int find_semi_perf_cpu(struct tp_env *env)
{
	int best_idle_cpu = INVALID_CPU;
	int min_exit_lat = UINT_MAX;
	int cpu;
	int max_spare_cap_cpu_ls = env->src_cpu;
	unsigned long max_spare_cap_ls = 0;
	unsigned long min_cuml_util = ULONG_MAX;

	unsigned long spare_cap, util, cpu_cap, target_cap;
	int exit_lat;

	for_each_cpu(cpu, &env->fit_cpus) {
		util = env->cpu_stat[cpu].util_with;
		cpu_cap = env->cpu_stat[cpu].cap_orig;
		spare_cap = cpu_cap - util;
		if (env->cpu_stat[cpu].idle) {
			exit_lat = env->cpu_stat[cpu].exit_latency;

			if (cpu_cap < target_cap)
				continue;

			if (cpu_cap == target_cap &&
				exit_lat > min_exit_lat)
				continue;

			if (exit_lat == min_exit_lat &&
				target_cap == cpu_cap &&
				(best_idle_cpu == env->src_cpu ||
				(cpu != env->src_cpu &&
				env->cpu_stat[cpu].util_cuml > min_cuml_util)))
				continue;

			if (exit_lat)
				min_exit_lat = exit_lat;

			min_cuml_util = env->cpu_stat[cpu].util_cuml;
			target_cap = cpu_cap;
			best_idle_cpu = cpu;
		} else if (spare_cap > max_spare_cap_ls) {
			max_spare_cap_ls = spare_cap;
			max_spare_cap_cpu_ls = cpu;
		}
	}

	if (!env->latency_sensitive && max_spare_cap_cpu_ls >= 0)
		return max_spare_cap_cpu_ls;

	return best_idle_cpu >= 0 ? best_idle_cpu : max_spare_cap_cpu_ls;
}

int find_best_cpu(struct tp_env *env)
{
	char state[30] = "fail";
	int policy = env->sched_policy;
	int best_cpu = INVALID_CPU;

	switch (policy) {
	case SCHED_POLICY_EFF:
		/* Find best efficiency cpu */
		best_cpu = find_best_eff_cpu(env);
		strcpy(state, "best eff");
		break;
	case SCHED_POLICY_ENERGY:
		/* Find lowest energy cpu */
		best_cpu = find_energy_cpu(env);
		strcpy(state, "energy");
		break;
	case SCHED_POLICY_PERF:
		/* Find best performance cpu */
		best_cpu = find_best_perf_cpu(env);
		strcpy(state, "best perf");
		break;
	case SCHED_POLICY_SEMI_PERF:
		/* Find semi performance cpu */
		best_cpu = find_semi_perf_cpu(env);
		strcpy(state, "semi perf");
		break;
	case SCHED_POLICY_MIN_UTIL:
		/* Find cpu with minimum util with task */
		best_cpu = find_min_util_with_cpu(env);
		strcpy(state, "min util");
		break;
	default:
		best_cpu = env->src_cpu;
		strcpy(state, "prev");
	}

	if (cpu_selected(best_cpu))
		goto out;

	/* Keep task on prev cpu if no efficient cpu is found */
	best_cpu = env->src_cpu;
	strcpy(state, "no best cpu");
out:
	trace_ems_wakeup_balance(env->p, best_cpu, env->wake, state);
	return best_cpu;
}
