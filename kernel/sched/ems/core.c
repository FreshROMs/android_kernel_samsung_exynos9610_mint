/*
 * Exynos Mobile Scheduler CPU selection
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <linux/cpuidle.h>
#include <linux/ems.h>
#include <linux/ems_service.h>
#include <linux/freezer.h>

#include "ems.h"
#include "../sched.h"
#include "../tune.h"

#include <trace/events/ems.h>

/******************************************************************************
 * TEX (Task EXpress)                                                         *
 ******************************************************************************/
struct {
    struct cpumask pinning_cpus;
    struct cpumask busy_cpus;
    int qjump;
    int enabled[CGROUP_COUNT];
    int prio;
	int suppress;
} tex;

int tex_task(struct task_struct *p)
{
    int grp_idx;

    grp_idx = schedtune_task_group_idx(p);
    if (!tex.enabled[grp_idx])
        return 0;

    if (p->sched_class != &fair_sched_class)
        return 0;

    if (!p->pid || ems_task_boost() != p->pid)
        return 0;

    return p->prio <= tex.prio;
}

void tex_enqueue_task(struct task_struct *p, int cpu)
{
    if (!tex_task(p))
        return;

    /*
     * Although the task is a TEX task, it could be happen that qjump is
     * enabled but pinning is not. In that case, DO NOT update busy CPUs
     * because it affects on fit_cpus.
     */
    if (cpumask_test_cpu(cpu, &tex.pinning_cpus))
        cpumask_set_cpu(cpu, &tex.busy_cpus);

    if (tex.qjump) {
        list_add_tail(ems_qjump_node(p), ems_qjump_list(cpu_rq(cpu)));
        ems_qjump_queued(p) = 1;
    }
}

void tex_dequeue_task(struct task_struct *p, int cpu)
{
    /*
     * TEX qjump could be disabled while TEX task is being enqueued.
     * Make sure to delete list node of TEX task from qjump list
     * every time the task is dequeued.
     */
    if (ems_qjump_queued(p)) {
        list_del(ems_qjump_node(p));
        ems_qjump_queued(p) = 0;
    }

    if (!tex_task(p))
        return;

    cpumask_clear_cpu(cpu, &tex.busy_cpus);
}

static void
tex_pinning_schedule(struct tp_env *env, struct cpumask *candidates)
{
    int cpu, max_spare_cpu, max_spare_cpu_idle = -1, max_spare_cpu_active = -1;
    unsigned long spare, max_spare_idle = 0, max_spare_active = 0;

    for_each_cpu(cpu, candidates) {
        spare = capacity_orig_of(cpu) - env->cpu_stat[cpu].util_wo;
        if (env->cpu_stat[cpu].idle) {
            if (spare >= max_spare_idle) {
                max_spare_cpu_idle = cpu;
                max_spare_idle = spare;
            }
        } else {
            if (spare >= max_spare_active) {
                max_spare_cpu_active = cpu;
                max_spare_active = spare;
            }
        }
    }

    max_spare_cpu = cpu_selected(max_spare_cpu_idle) ? max_spare_cpu_idle : max_spare_cpu_active;

    cpumask_clear(candidates);
    cpumask_set_cpu(max_spare_cpu, candidates);
}

int tex_suppress_task(struct task_struct *p)
{
	if (!tex.suppress)
		return 0;

	if (p->sched_class == &fair_sched_class && p->prio <= DEFAULT_PRIO)
		return 0;

	return 1;
}

static void
tex_pinning_fit_cpus(struct tp_env *env, struct cpumask *fit_cpus)
{
    int target = tex_task(env->p);
	int suppress = tex_suppress_task(env->p);

	cpumask_clear(fit_cpus);

	if (target) {
		cpumask_andnot(fit_cpus, &tex.pinning_cpus, &tex.busy_cpus);
		cpumask_and(fit_cpus, fit_cpus, &env->cpus_allowed);

		if (cpumask_empty(fit_cpus)) {
			/*
			 * All priority pinning cpus are occupied by TEX tasks.
			 * Return cpu_active_mask so that priority pinning
			 * has no effect.
			 */
			cpumask_copy(fit_cpus, cpu_active_mask);
		} else {
			/* Pick best cpu among fit_cpus */
			tex_pinning_schedule(env, fit_cpus);
		}
	} else if (suppress) {
		/* Clear fastest & tex-busy cpus for the supressed task */
		cpumask_andnot(fit_cpus, &env->cpus_allowed, cpu_fastest_mask());
		cpumask_andnot(fit_cpus, fit_cpus, &tex.busy_cpus);

		if (cpumask_empty(fit_cpus))
			cpumask_copy(fit_cpus, &env->cpus_allowed);
	} else {
		/* Exclude cpus where priority pinning tasks run */
		cpumask_andnot(fit_cpus, cpu_active_mask,
					&tex.busy_cpus);
	}
}

/**********************************************************************
 *  stune api for 4.14                                                *
 **********************************************************************/
u64 ems_tex_enabled_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	struct schedtune *st = css_st(css);
	int group_idx = st->idx;

	if (group_idx >= CGROUP_COUNT)
		return (u64) tex.enabled[CGROUP_ROOT];

	return (u64) tex.enabled[st->idx];
}

int ems_tex_enabled_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled) {
	struct schedtune *st = css_st(css);
	int group_idx;

	if (enabled < 0 || enabled > 1)
		return -EINVAL;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT) {
		tex.enabled[CGROUP_ROOT] = enabled;
		return 0;
	}

	tex.enabled[group_idx] = enabled;
	return 0;
}

int ems_tex_pinning_cpus_stune_hook_read(struct seq_file *sf, void *v)
{
	seq_printf(sf, "%*pbl\n", cpumask_pr_args(&tex.pinning_cpus));
	return 0;
}

#define STR_LEN (6)
ssize_t ems_tex_pinning_cpus_stune_hook_write(struct kernfs_open_file *of,
				    char *buf, size_t nbytes,
				    loff_t off)
{
	char str[STR_LEN];

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%s", str))
		return -EINVAL;

	cpulist_parse(buf, &tex.pinning_cpus);

	return nbytes;
}

u64 ems_tex_prio_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	return (u64) tex.prio;
}

int ems_tex_prio_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 prio) {

	if (prio < MIN_NICE || prio > MAX_PRIO)
		return -EINVAL;

	tex.prio = prio;
	return 0;
}

u64 ems_tex_suppress_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	return (u64) tex.suppress;
}

int ems_tex_suppress_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled) {

	if (enabled < 0 || enabled > 1)
		return -EINVAL;

	tex.suppress = enabled;
	return 0;
}

/**********************************************************************
 *  SYSFS support                                                     *
 **********************************************************************/
static
int __init tex_init(void)
{
	int i;

    cpumask_clear(&tex.pinning_cpus);
    cpumask_copy(&tex.pinning_cpus, cpu_perf_mask);

    cpumask_clear(&tex.busy_cpus);
    tex.qjump = 0;
    tex.suppress = 1;
	for (i = 0; i < CGROUP_COUNT; i++)
		tex.enabled[i] = 0;
    tex.prio = 110;

    return 0;
}

/******************************************************************************
 * sched policy                                                               *
 ******************************************************************************/
static int sched_policy[CGROUP_COUNT] = {SCHED_POLICY_ENERGY, };
static char *sched_policy_name[] = {
    "SCHED_POLICY_EFF",
    "SCHED_POLICY_ENERGY",
    "SCHED_POLICY_SEMI_PERF",
    "SCHED_POLICY_PERF",
    "SCHED_POLICY_UNKNOWN"
};

static
int sched_policy_get(struct task_struct *p)
{
	int cgroup_idx = schedtune_task_group_idx(p);
	int policy = sched_policy[cgroup_idx];

	/* 
	 * Mint additions - scheduling policy changes
	 * 
	 * Adapt to current schedtune setting for tasks with
	 * a. device freezing status - SCHED_POLICY_ENERGY
	 * b. 'on-top' status - SCHED_POLICY_PERF
	 * c. global boost on init - SCHED_POLICY_PERF
	 * d. task boost - SCHED_POLICY_PERF
	 * e. global boost on boot - SCHED_POLICY_SEMI_PERF
	 * f. 'prefer-perf' status - SCHED_POLICY_SEMI_PERF
	 *
	 */
	if (pm_freezing)
		return SCHED_POLICY_ENERGY;

	if (ems_task_on_top(p) || ems_boot_boost() == EMS_INIT_BOOST)
		return SCHED_POLICY_PERF;

	if (p->pid && ems_task_boost() == p->pid)
		return SCHED_POLICY_PERF;

	if (ems_boot_boost() == EMS_BOOT_BOOST)
		return SCHED_POLICY_SEMI_PERF;

	if (kpp_status(cgroup_idx))
		return SCHED_POLICY_SEMI_PERF;

	/*
	 * Change target tasks' policy to SCHED_POLICY_ENERGY
	 * for power optimization, if
	 * 1) target task's sched_policy is SCHED_POLICY_EFF and
	 *    its utilization is under 1.56% of SCHED_CAPACITY_SCALE.
	 * 2) tasks is worker thread.
	 */
	if (p->flags & PF_WQ_WORKER)
		return SCHED_POLICY_ENERGY;

	if (policy == SCHED_POLICY_EFF &&
		ml_task_util(p) <= SCHED_CAPACITY_SCALE >> 6)
		return SCHED_POLICY_ENERGY;

	return policy;
}

int ems_sched_policy_stune_hook_read(struct seq_file *sf, void *v) {
	struct cgroup_subsys_state *css = seq_css(sf);
	struct schedtune *st = css_st(css);
	int policy = sched_policy[CGROUP_ROOT];
	int group_idx = st->idx;

	if (group_idx < CGROUP_COUNT)
		policy = sched_policy[group_idx];

	seq_printf(sf, "%u. %s\n", policy, sched_policy_name[policy]);

	return 0;
}

int ems_sched_policy_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 policy) {
	struct schedtune *st = css_st(css);
	int group_idx;

	if (policy < SCHED_POLICY_EFF || policy >= SCHED_POLICY_UNKNOWN)
		return -EINVAL;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT) {
		sched_policy[CGROUP_ROOT] = policy;
		return 0;
	}

	sched_policy[group_idx] = policy;
	return 0;
}

/******************************************************************************
 * prefer cpu                                                                 *
 ******************************************************************************/
static struct {
	struct cpumask mask[CGROUP_COUNT];
	struct {
		unsigned long threshold;
		struct cpumask mask;
	} small_task;
} prefer_cpu;

static void prefer_cpu_get(struct tp_env *env, struct cpumask *mask)
{
	cpumask_clear(mask);
	cpumask_copy(mask, cpu_active_mask);

	if (env->boosted) {
		if (env->task_util < prefer_cpu.small_task.threshold) {
			cpumask_and(mask, mask, &prefer_cpu.small_task.mask);
			return;
		}

		cpumask_and(mask, mask, &prefer_cpu.mask[env->cgroup_idx]);
	}
}

int ems_prefer_cpus_stune_hook_read(struct seq_file *sf, void *v)
{
	struct cgroup_subsys_state *css = seq_css(sf);
	struct schedtune *st = css_st(css);
	int group_idx = st->idx;

	if (group_idx < CGROUP_COUNT) {
		seq_printf(sf, "%*pbl\n", cpumask_pr_args(&prefer_cpu.mask[group_idx]));
	} else {
		seq_printf(sf, "%*pbl\n", cpumask_pr_args(&prefer_cpu.mask[CGROUP_ROOT]));
	}

	return 0;
}

ssize_t ems_prefer_cpus_stune_hook_write(struct kernfs_open_file *of,
				    char *buf, size_t nbytes,
				    loff_t off)
{
	struct schedtune *st = css_st(of_css(of));
	char str[STR_LEN];
	int group_idx;

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%s", str))
		return -EINVAL;

	group_idx = st->idx;
	if (group_idx >= CGROUP_COUNT) {
		cpulist_parse(buf, &prefer_cpu.mask[CGROUP_ROOT]);
		return nbytes;
	}

	cpulist_parse(buf, &prefer_cpu.mask[group_idx]);

	return nbytes;
}

u64 ems_small_task_threshold_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	return (u64) prefer_cpu.small_task.threshold;
}

int ems_small_task_threshold_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 threshold) {
	if (threshold < 0 || threshold >= 2147483647)
		return -EINVAL;

	prefer_cpu.small_task.threshold = threshold;
	return 0;
}

int ems_small_task_cpus_stune_hook_read(struct seq_file *sf, void *v)
{
	seq_printf(sf, "%*pbl\n", cpumask_pr_args(&prefer_cpu.small_task.mask));
	return 0;
}

ssize_t ems_small_task_cpus_stune_hook_write(struct kernfs_open_file *of,
				    char *buf, size_t nbytes,
				    loff_t off)
{
	char str[STR_LEN];

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%s", str))
		return -EINVAL;

	cpulist_parse(buf, &prefer_cpu.small_task.mask);

	return nbytes;
}

static
int __init prefer_cpu_init(void)
{
	int i;

	cpumask_clear(&prefer_cpu.small_task.mask);
	
	for (i = 0; i < CGROUP_COUNT; i++) {
		cpumask_clear(&prefer_cpu.mask[i]);

		switch (i) {
		case CGROUP_TOPAPP:
			cpumask_copy(&prefer_cpu.mask[i], cpu_perf_mask);
			break;
		default:
			cpumask_copy(&prefer_cpu.mask[i], cpu_possible_mask);
			break;
		}
	}

	prefer_cpu.small_task.threshold = 50;
	cpumask_copy(&prefer_cpu.small_task.mask, cpu_lp_mask);

	return 0;
}

/******************************************************************************
 * best energy cpu selection                                                  *
 ******************************************************************************/
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
		energy = et_calculate_energy(env, cpu);

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
	if (!ems_get_energy_table_status())
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
		if (cpu_selected(min_cpu))
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
	if (cpu_selected(adv_energy_cpu))
		return adv_energy_cpu;

	return energy_cpu;
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
	if (!ems_get_energy_table_status())
		return find_min_util_cpu(env, &env->fit_cpus, among_idle);

	/* find best efficiency cpu */
	for_each_cpu(cpu, &env->fit_cpus) {

		/*
		 * If among_idle is true, find eff cpu among idle cpu.
		 * Skip non-idle cpu.
		 */
		if (among_idle && !env->cpu_stat[cpu].idle)
			continue;

		eff = et_calculate_efficiency(env, cpu);

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

		curr_cap = capacity_curr_of(cpu);
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
	unsigned long spare_cap, util, cpu_cap, target_cap = 0;
	struct cpuidle_state *idle;

	rcu_read_lock();

	for_each_cpu(cpu, &env->fit_cpus) {
		util = env->cpu_stat[cpu].util_with;
		cpu_cap = env->cpu_stat[cpu].cap_orig;
		spare_cap = cpu_cap - util;
		if (idle_cpu(cpu)) {
			if (cpu_cap < target_cap)
				continue;

			idle = idle_get_state(cpu_rq(cpu));
			if (idle && idle->exit_latency > min_exit_lat &&
				cpu_cap == target_cap)
				continue;

			if (idle && idle->exit_latency == min_exit_lat &&
				target_cap == cpu_cap &&
				(best_idle_cpu == env->src_cpu ||
				(cpu != env->src_cpu &&
				env->cpu_stat[cpu].util_cuml > min_cuml_util)))
				continue;

			if (idle)
				min_exit_lat = idle->exit_latency;
			min_cuml_util = env->cpu_stat[cpu].util_cuml;
			target_cap = cpu_cap;
			best_idle_cpu = cpu;
		} else if (spare_cap > max_spare_cap_ls) {
			max_spare_cap_ls = spare_cap;
			max_spare_cap_cpu_ls = cpu;
		}
	}

	rcu_read_unlock();

	if (!env->latency_sensitive && cpu_selected(max_spare_cap_cpu_ls))
		return max_spare_cap_cpu_ls;

	return cpu_selected(best_idle_cpu) ? best_idle_cpu : max_spare_cap_cpu_ls;
}

/******************************************************************************
 * best cpu selection                                                         *
 ******************************************************************************/
static
int find_best_cpu(struct tp_env *env)
{
	char state[30] = "fail";
	int policy = env->sched_policy;
	int best_cpu = INVALID_CPU;

	/* When sysbusy is detected, do scheduling under other policies */
	if (sysbusy_activated()) {
		best_cpu = sysbusy_schedule(env);
		if (cpu_selected(best_cpu)) {
			strcpy(state, "sysbusy");
			goto out;
		}
	}

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
	}

	if (cpu_selected(best_cpu))
		goto out;

	/* Keep task on prev cpu if no efficient cpu is found */
	best_cpu = env->src_cpu;
	strcpy(state, "no best cpu");

out:
	trace_ems_select_task_rq(env->p, env->task_util, env->task_util_clamped, env->cgroup_idx, best_cpu, env->wake, state);
	return best_cpu;
}

static
int find_min_load_avg_cpu(struct tp_env *env)
{
	int cpu, min_load_avg_cpu = -1;
	unsigned long load_avg, min_load_avg = ULONG_MAX;

	for_each_cpu(cpu, &env->cpus_allowed) {
		load_avg = env->cpu_stat[cpu].load_avg;
		if (load_avg < min_load_avg) {
			min_load_avg = load_avg;
			min_load_avg_cpu = cpu;
		}
	}

	return min_load_avg_cpu;
}

static
void find_overcap_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;
	unsigned long util;

	cpumask_clear(mask);

	/* check if task is misfit - causes all CPUs to be over capacity */
	if (is_misfit_task_util(env->task_util_clamped))
		goto misfit;

	/*
	 * Find cpus that becomes over capacity with a given task.
	 *
	 * overcap_cpus = cpu capacity < cpu util + task util
	 */
	for_each_cpu(cpu, &env->cpus_allowed) {
		util = env->cpu_stat[cpu].util_wo + env->task_util_clamped;
		if (util > env->cpu_stat[cpu].cap_orig)
			cpumask_set_cpu(cpu, mask);
	}

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

		if (env->task_util_clamped <= capacity &&
		    !check_busy(env->cpu_stat[cpu].util_wo, capacity) &&
		    !cpu_rq(cpu)->nr_running)
			cpumask_clear_cpu(cpu, mask);
	}
}

static
void find_busy_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;

	cpumask_clear(mask);

	for_each_cpu(cpu, &env->cpus_allowed) {
		unsigned long util, runnable, nr_running;

		util = env->cpu_stat[cpu].util;
		runnable = env->cpu_stat[cpu].runnable;
		nr_running = env->cpu_stat[cpu].nr_running;

		if (!__is_busy_cpu(util, runnable, env->cpu_stat[cpu].cap_orig, nr_running))
			continue;

		cpumask_set_cpu(cpu, mask);
	}
}

static
int find_fit_cpus(struct tp_env *env)
{
	struct cpumask mask[5];
	int cpu = smp_processor_id();

	/* clear cpumask */
	cpumask_clear(&env->fit_cpus);

	/* find min load cpu for busy task */
	if (EMS_PF_GET(env->p) & EMS_PF_RUNNABLE_BUSY) {
		cpu = find_min_load_avg_cpu(env);
		if (cpu_selected(cpu)) {
			cpumask_set_cpu(cpu, &env->fit_cpus);
			goto out;
		}
	}

	/*
	 * take a snapshot of cpumask to get fit cpus
	 * - mask0 : overcap cpus
	 * - mask1 : prio pinning cpus
	 * - mask2 : ontime fit cpus
	 * - mask3 : prefer cpu
	 * - mask4 : busy cpu
	 */
	find_overcap_cpus(env, &mask[0]);
	tex_pinning_fit_cpus(env, &mask[1]);
	ontime_select_fit_cpus(env->p, &mask[2]);
	prefer_cpu_get(env, &mask[3]);
	find_busy_cpus(env, &mask[4]);

	/*
	 * Exclude overcap cpu from cpus_allowed. If there is only one or no
	 * fit cpus, it does not need to find fit cpus anymore.
	 */
	cpumask_andnot(&env->fit_cpus, &env->cpus_allowed, &mask[0]);
	if (cpumask_weight(&env->fit_cpus) <= 1)
		goto out;

	/* Exclude busy cpus from fit_cpus */
	cpumask_andnot(&env->fit_cpus, &env->fit_cpus, &mask[4]);
	if (cpumask_weight(&env->fit_cpus) <= 1)
		goto out;

	if (cpumask_intersects(&env->fit_cpus, cpu_fastest_mask())) {
		if ((env->p->pid && ems_task_boost() == env->p->pid) || ems_task_on_top(env->p)) {
			cpumask_and(&env->fit_cpus, &env->fit_cpus, cpu_fastest_mask());
			goto out;
		}
	}

	/* Handle sync flag */
	if (sysctl_sched_sync_hint_enable && env->sync) {
		if (cpu_rq(cpu)->nr_running == 1 &&
			cpumask_test_cpu(cpu, &env->fit_cpus)) {
			struct cpumask mask;

			cpumask_andnot(&mask, cpu_active_mask,
						cpu_slowest_mask());

			/*
			 * Select this cpu if boost is activated and this
			 * cpu does not belong to slowest cpumask.
			 */
			if (cpumask_test_cpu(cpu, &mask)) {
				cpumask_clear(&env->fit_cpus);
				cpumask_set_cpu(cpu, &env->fit_cpus);
				goto out;
			}
		}
	}

	/*
	 * Apply priority pinning to fit_cpus. Since priority pinning has a
	 * higher priority than ontime, it does not need to execute sequence
	 * afterwards if there is only one fit cpus.
	 */
	if (cpumask_intersects(&env->fit_cpus, &mask[1])) {
		cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask[1]);
		if (cpumask_weight(&env->fit_cpus) == 1)
			goto out;
	}

	/* Pick ontime fit cpus if ontime is applicable */
	if (cpumask_intersects(&env->fit_cpus, &mask[2]))
		cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask[2]);

	/* Apply prefer cpu if it is applicable */
	if (cpumask_intersects(&env->fit_cpus, &mask[3]))
		cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask[3]);

out:
	trace_ems_select_fit_cpus(env->p, env->wake,
		*(unsigned int *)cpumask_bits(&env->fit_cpus),
		*(unsigned int *)cpumask_bits(&env->cpus_allowed),
		*(unsigned int *)cpumask_bits(&mask[1]),
		*(unsigned int *)cpumask_bits(&mask[2]),
		*(unsigned int *)cpumask_bits(&mask[3]),
		*(unsigned int *)cpumask_bits(&mask[0]),
		*(unsigned int *)cpumask_bits(&mask[4]));

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
	env->task_util = max(ml_task_util(env->p), (unsigned long) 1);
	env->task_util_clamped = max(ml_uclamp_task_util(env->p), (unsigned long) 1);

	/* fill cpu util */
	for_each_cpu(cpu, cpu_active_mask) {
		unsigned long capacity_orig = capacity_orig_of(cpu);

		env->cpu_stat[cpu].cap_orig = capacity_orig;

		env->cpu_stat[cpu].util_wo = min(ml_cpu_util_without(cpu, env->p), capacity_orig);
		env->cpu_stat[cpu].util_with = min(ml_cpu_util_with(env->p, cpu), capacity_orig);

		env->cpu_stat[cpu].runnable = ml_runnable_load_avg(cpu);
		env->cpu_stat[cpu].load_avg = ml_cpu_load_avg(cpu);

		if (cpu == task_cpu(env->p))
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_with;
		else
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_wo;

		/* cumulative CPU demand */
		env->cpu_stat[cpu].util_cuml =
			min(env->cpu_stat[cpu].util + env->task_util_clamped, capacity_orig);
		if (task_in_cum_window_demand(cpu_rq(cpu), env->p))
			env->cpu_stat[cpu].util_cuml -= env->task_util;

		env->cpu_stat[cpu].nr_running = cpu_rq(cpu)->nr_running;
		env->cpu_stat[cpu].idle = idle_cpu(cpu);
	}
}

static
int select_start_cpu(struct tp_env *env) {
	int start_cpu = cpumask_first_and(cpu_slowest_mask(), cpu_active_mask);
	struct cpumask active_fast_mask;

	/* Avoid recommending fast CPUs during idle as these are inactive */
	if (pm_freezing)
		return start_cpu;

	/* Don't select CPU if task is not allowed to be placed on fast CPUs */
	cpumask_and(&active_fast_mask, cpu_fastest_mask(), &env->cpus_allowed);
	if (cpumask_empty(&active_fast_mask))
		return start_cpu;

	/* Return fast CPU if task is boosted or global boosting */
	if (env->boosted || ems_boot_boost())
		start_cpu = cpumask_first(&active_fast_mask);

	return start_cpu;
}

static
bool fast_path_eligible(struct tp_env *env)
{	
	int start_cpu = select_start_cpu(env);

	/* Cond 1: Task should not be under mulligan migration */
	if (EMS_PF_GET(env->p) & EMS_PF_MULLIGAN)
		return false;

	/* Cond 2: Previous CPU should be eligible for placement */
	if (!cpumask_test_cpu(env->src_cpu, &env->cpus_allowed))
		return false;

	/* Cond 3: Previous CPU must be active */
	if (!cpu_active(env->src_cpu))
		return false;

	/* Cond 4: Previous CPU must not be overutilized */
	if (cpu_overutilized(env->src_cpu))
		return false;

	/* Cond 5: Previous CPU capacity must be same as start CPU capacity */
	if (capacity_max_of(start_cpu) != capacity_max_of(env->src_cpu))
		return false;

	/* Cond 6: Previous CPU should be idle */
	if (!idle_cpu(env->src_cpu))
		return false;

	/* Cond 7: Previous CPU must be shallow idle */
	if (idle_get_state_idx(cpu_rq(env->src_cpu)) <= 1) {
		/* final sanity check in case 'cpuset' changes gears */
		if (cpumask_test_cpu(env->src_cpu, &env->cpus_allowed))
			return true;
	}

	return false;
}

/*
 * Return number of CPUs allowed.
 */
static
int find_cpus_allowed(struct tp_env *env)
{
	struct cpumask mask[3];

	/* clear mask */
	cpumask_clear(&env->cpus_allowed);

	if (is_somac_ready(env))
		goto out;

	/*
	 * take a snapshot of cpumask to get CPUs allowed
	 * - mask0 : cpu_active_mask
	 * - mask1 : cpu_online_mask
	 * - mask2 : p->cpus_ptr
	 */
	cpumask_copy(&mask[0], cpu_active_mask);
	cpumask_copy(&mask[1], cpu_online_mask);
	cpumask_copy(&mask[2], env->p->cpus_ptr);

	/*
	 * Putting per-cpu kthread on other cpu is not allowed.
	 * It does not have to find cpus allowed in this case.
	 */
	if (env->per_cpu_kthread) {
		cpumask_and(&env->cpus_allowed, &mask[2], &mask[1]);
		goto out;
	}

	/*
	 * Given task must run on the CPU combined as follows:
	 *	p->cpus_ptr & cpu_active_mask
	 */
	cpumask_and(&env->cpus_allowed, &mask[2], &mask[0]);

out:
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

	/* Check for fast placement eligibility */
	if (fast_path_eligible(env)) {
		/* Task is eligible for fast placement, keep the task on prev cpu */
		cpumask_clear(&env->cpus_allowed);
		cpumask_set_cpu(env->src_cpu, &env->cpus_allowed);
		trace_ems_select_task_rq(env->p, 0, 0, env->cgroup_idx, env->src_cpu, env->wake, "fast placement");
		return;
	}

	/* snapshot util to use same util during core selection */
	take_util_snapshot(env);

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
	for_each_cpu(cpu, &env->fit_cpus)
		if (env->cpu_stat[cpu].idle)
			env->idle_cpu_count++;
}

int __ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sync, int wake)
{
	int target_cpu = INVALID_CPU;
	struct tp_env env = {
		.p = p,
		.src_cpu = task_cpu(p),
		.cgroup_idx = schedtune_task_group_idx(p),
		.per_cpu_kthread = is_per_cpu_kthread(p),
		.sched_policy = sched_policy_get(p),

		.boosted = ems_task_boosted(p),
		.latency_sensitive = uclamp_latency_sensitive(p),

		.sync = sync,
		.wake = wake,
	};

	/* Get ready environment variable */
	get_ready_env(&env);

	/* there is no CPU allowed, give up find new cpu */
	if (cpumask_empty(&env.cpus_allowed)) {
		trace_ems_select_task_rq(p, 0, 0, env.cgroup_idx, target_cpu, wake, "no CPU allowed");
		goto out;
	}

	/* there is only one CPU allowed, no need to find cpu */
	if (cpumask_weight(&env.cpus_allowed) == 1) {
		target_cpu = cpumask_any(&env.cpus_allowed);
		trace_ems_select_task_rq(p, 0, 0, env.cgroup_idx, target_cpu, wake, "one CPU allowed");
		goto out;
	}

	target_cpu = find_best_cpu(&env);
	if (target_cpu < 0 || !cpumask_test_cpu(target_cpu, &env.cpus_allowed)) {
		/* 
		 * There are instances where 'cpuset' contends with EMS' current scheduling
		 * and would change allowed CPUs before we can schedule a given task. It's been
		 * alleviated heavily, but it still happens to a minor extent.
		 *
		 * I seriously have nothing else to do other than this. CFS on 4.14 is just painful.
		 */
		if (cpumask_test_cpu(prev_cpu, &env.cpus_allowed))
			target_cpu = prev_cpu;
		else
			target_cpu = cpumask_any_distribute(&env.cpus_allowed);
	}

out:
	return target_cpu;
}

int __init core_init()
{
	tex_init();
	prefer_cpu_init();

	return 0;
}
