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
    int enabled[BOOSTGROUPS_COUNT];
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

    /* on-top task is eligible for tex */
    if (ems_task_on_top(p))
        return 1;

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
        spare = env->cpu_stat[cpu].cap_orig - env->cpu_stat[cpu].util_wo;
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

	if (ems_task_on_top(p))
		return 0;

	if (p->sched_class == &fair_sched_class && p->prio <= DEFAULT_PRIO)
		return 0;

	return 1;
}

static void
tex_pinning_fit_cpus(struct tp_env *env, struct cpumask *fit_cpus)
{
    int target = tex_task(env->p);
	int suppress;

    if (target) {
        cpumask_andnot(fit_cpus, &tex.pinning_cpus, &tex.busy_cpus);
        cpumask_and(fit_cpus, fit_cpus, &env->p->cpus_allowed);

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

        return;
    }

	suppress = tex_suppress_task(env->p);
    if (suppress) {
		/* Clear fastest & tex-busy cpus for the supressed task */
		cpumask_andnot(fit_cpus, &env->cpus_allowed, cpu_fastest_mask());
		cpumask_andnot(fit_cpus, fit_cpus, &tex.busy_cpus);

		if (cpumask_empty(fit_cpus))
			cpumask_copy(fit_cpus, &env->cpus_allowed);

		return;
	}

    /* Exclude cpus where priority pinning tasks run */
    cpumask_andnot(fit_cpus, &env->p->cpus_allowed, &tex.busy_cpus);
}

/**********************************************************************
 *  cpuset api for 4.14                                               *
 **********************************************************************/
u64 ems_tex_enabled_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	struct schedtune *st = css_st(css);
	int group_idx = st->idx;

	if (group_idx >= BOOSTGROUPS_COUNT)
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
	if (group_idx >= BOOSTGROUPS_COUNT) {
		tex.enabled[CGROUP_ROOT] = enabled;
		return 0;
	}

	tex.enabled[group_idx] = enabled;
	return 0;
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

u64 ems_qjump_stune_hook_read(struct cgroup_subsys_state *css,
			     struct cftype *cft) {
	return (u64) tex.qjump;
}

int ems_qjump_stune_hook_write(struct cgroup_subsys_state *css,
		             struct cftype *cft, u64 enabled) {

	if (enabled < 0 || enabled > 1)
		return -EINVAL;

	tex.qjump = enabled;
	return 0;
}

/**********************************************************************
 *  SYSFS support                                                     *
 **********************************************************************/
static struct kobject *tex_kobj;

static ssize_t show_qjump_enabled(struct kobject *kobj,
        struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", tex.qjump);
}

static ssize_t store_qjump_enabled(struct kobject *kobj,
        struct kobj_attribute *attr, const char *buf,
        size_t count)
{
    int enabled;

    if (sscanf(buf, "%d", &enabled) != 1)
        return -EINVAL;

    /* ignore if requested mode is out of range */
    if (enabled < 0 || enabled > 1)
        return -EINVAL;

    tex.qjump = enabled;

    return count;
}

static ssize_t show_task_prio(struct kobject *kobj,
        struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", tex.prio);
}

static ssize_t store_task_prio(struct kobject *kobj,
        struct kobj_attribute *attr, const char *buf,
        size_t count)
{
    int prio;

    if (sscanf(buf, "%d", &prio) != 1)
        return -EINVAL;

    /* ignore if requested mode is out of range */
    if (prio < MIN_NICE || prio > MAX_PRIO)
        return -EINVAL;

    tex.prio = prio;

    return count;
}

static struct kobj_attribute qjump_enabled_attr =
__ATTR(qjump_enabled, 0644, show_qjump_enabled, store_qjump_enabled);
static struct kobj_attribute task_prio_attr =
__ATTR(task_prio, 0644, show_task_prio, store_task_prio);

static struct attribute *tex_attrs[] = {
    &qjump_enabled_attr.attr,
    &task_prio_attr.attr,
    NULL,
};

static const struct attribute_group tex_group = {
    .attrs = tex_attrs,
};

static
int __init tex_init(void)
{
	int i;
    int ret;

    tex_kobj = kobject_create_and_add("tex", ems_kobj);
    if (!tex_kobj) {
        pr_err("Failed to create ems tex kboject\n");
        return -EINVAL;
    }

    ret = sysfs_create_group(tex_kobj, &tex_group);
    if (ret) {
        pr_err("Failed to create ems tex group\n");
        return ret;
    }

    cpumask_clear(&tex.pinning_cpus);
    cpumask_copy(&tex.pinning_cpus, cpu_perf_mask);

    cpumask_clear(&tex.busy_cpus);
    tex.qjump = 0;
    tex.suppress = 1;
	for (i = 0; i < BOOSTGROUPS_COUNT; i++)
		tex.enabled[i] = 0;
    tex.prio = 110;

    return 0;
}

/******************************************************************************
 * sched policy                                                               *
 ******************************************************************************/
static int sched_policy[BOOSTGROUPS_COUNT] = {SCHED_POLICY_EFF, };
static char *sched_policy_name[] = {
    "SCHED_POLICY_EFF",
    "SCHED_POLICY_ENERGY",
    "SCHED_POLICY_SEMI_PERF",
    "SCHED_POLICY_PERF",
    "SCHED_POLICY_MIN_UTIL",
    "SCHED_POLICY_UNKNOWN"
};

static
void sched_policy_get(struct tp_env *env)
{
	struct task_struct *p = env->p;
	int policy = sched_policy[env->cgroup_idx];

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
	if (env->task_on_top || ems_boot_boost() == EMS_INIT_BOOST) {
		policy = SCHED_POLICY_PERF;
		goto out;
	}

	if (env->p->pid && ems_task_boost() == env->p->pid) {
		policy = SCHED_POLICY_PERF;
		goto out;
	}

	if (policy >= SCHED_POLICY_PERF)
		goto out;

	if (env->boosted && env->cgroup_idx == CGROUP_TOPAPP) {
		policy = SCHED_POLICY_SEMI_PERF;
		goto out;
	}

	if (ems_boot_boost() == EMS_BOOT_BOOST) {
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
	if (policy == SCHED_POLICY_ENERGY && !env->task_util)
		policy = SCHED_POLICY_MIN_UTIL;

out:
	env->sched_policy = policy;
}

int ems_sched_policy_stune_hook_read(struct seq_file *sf, void *v) {
	struct cgroup_subsys_state *css = seq_css(sf);
	struct schedtune *st = css_st(css);
	int policy = sched_policy[CGROUP_ROOT];
	int group_idx = st->idx;

	if (group_idx < BOOSTGROUPS_COUNT)
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
	if (group_idx >= BOOSTGROUPS_COUNT) {
		sched_policy[CGROUP_ROOT] = policy;
		return 0;
	}

	sched_policy[group_idx] = policy;
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
		if (env->idle_cpu_count && et_cpu_slowest(cpumask_first(cpu_coregroup_mask(cpu)))) {
			min_idle_cpu = find_min_util_cpu(env, &mask, true);
			if (cpu_selected(min_idle_cpu))
				break;
		}

		min_cpu = find_min_util_cpu(env, &mask, false);

		/* Traverse next coregroup if no best cpu is found */
		if (cpu_selected(min_cpu))
			break;
	}

	return cpu_selected(min_idle_cpu) ? min_idle_cpu : min_cpu;
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

		/*
		* Skip processing placement further if we are visiting
		* cpus with lower capacity than start cpu
		*/
		if (pm_freezing || env->cpu_stat[cpu].idle)
			goto skip_cap;

		if (env->cpu_stat[cpu].cap_max < env->start_cpu.cap_max)
			continue;

skip_cap:
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

static
int find_min_load_cpu(struct tp_env *env)
{
	int cpu, min_load_cpu = -1;
	unsigned long load, min_load = ULONG_MAX;

	for_each_cpu(cpu, &env->cpus_allowed) {
		load = env->cpu_stat[cpu].util_with;
		if (load < env->cpu_stat[cpu].cap_orig &&
			load < min_load) {
			min_load = load;
			min_load_cpu = cpu;
		}
	}

	return min_load_cpu;
}

static
void find_overcap_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;
	unsigned long util;

	if (env->sched_policy == SCHED_POLICY_SEMI_PERF)
		return;

	util = env->task_util_clamped;

	/* check if task is misfit - causes all CPUs to be over capacity */
	if (is_misfit_task_util(util))
		goto misfit;

	/*
	 * Find cpus that becomes over capacity with a given task.
	 *
	 * overcap_cpus = cpu capacity < cpu util + task util
	 */
	for_each_cpu(cpu, &env->cpus_allowed)
		if (env->cpu_stat[cpu].util_wo + util > env->cpu_stat[cpu].cap_orig)
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
void find_migration_cpus(struct tp_env *env, struct cpumask *mask)
{
	int cpu;

	/*
	 * It looks for busy cpu to exclude from selection. If cpu is under
	 * boosted ontime migration, it is defined as migration cpu.
	 *
	 * migration_cpus = cpu with boosted ontime migration
	 */
	for_each_cpu(cpu, &env->cpus_allowed) {
		/* only avoid migration to fast cpus */
		if (et_cpu_slowest(cpu))
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
	if (env->task_on_top || (env->p->pid && ems_task_boost() == env->p->pid))
		return;

	/* Allow preemption if task is sync and 'prefer-perf' task */
	if (env->sync && env->boosted)
		return;

	for_each_cpu(cpu, &env->cpus_allowed) {
		if (!cpu_preemptible(env, cpu))
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
	struct cpumask overcap_cpus, tex_pinning_cpus, ontime_fit_cpus, prefer_cpus, busy_cpus, migration_cpus, non_preemptible_cpus;
	int cpu = smp_processor_id();

	/* clear masks */
	cpumask_clear(&env->fit_cpus);
	cpumask_clear(&fit_cpus);
	cpumask_clear(&overcap_cpus);
	cpumask_clear(&tex_pinning_cpus);
	cpumask_clear(&ontime_fit_cpus);
	cpumask_clear(&prefer_cpus);
	cpumask_clear(&busy_cpus);
	cpumask_clear(&migration_cpus);
	cpumask_clear(&non_preemptible_cpus);

	/* find min load cpu for busy mulligan task */
	if (EMS_PF_GET(env->p) & EMS_PF_RUNNABLE_BUSY) {
		cpu = find_min_load_cpu(env);
		if (cpu_selected(cpu)) {
			cpumask_set_cpu(cpu, &fit_cpus);
			goto out;
		}
	}

	/*
	 * take a snapshot of cpumask to get fit cpus
	 * - overcap_cpus : overcap cpus
	 * - tex_pinning_cpus : prio pinning cpus
	 * - ontime_fit_cpus : ontime fit cpus
	 * - prefer_cpus : prefer cpu
	 * - busy_cpus : busy cpu for migrating tasks
	 * - migration_cpus : cpu under boosted ontime migration
	 * - non_preemptible_cpus : non preemptible cpu
	 */
	find_overcap_cpus(env, &overcap_cpus);
	tex_pinning_fit_cpus(env, &tex_pinning_cpus);
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
	if (cpumask_weight(&fit_cpus) <= 1)
		goto out;

	/* Exclude busy cpus from fit_cpus */
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

	if (env->p->pid && ems_task_boost() == env->p->pid) {
		if (cpumask_intersects(&fit_cpus, cpu_fastest_mask())) {
			cpumask_and(&fit_cpus, &fit_cpus, cpu_fastest_mask());
			goto out;
		}
	}

	/* Handle sync flag if waker cpu is preemptible */
	if (sysctl_sched_sync_hint_enable &&
		env->sync && (env->task_on_top || env->boosted || cpu_preemptible(env, cpu))) {
		if (cpu_rq(cpu)->nr_running == 1 && cpumask_test_cpu(cpu, &fit_cpus)) {
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

	/*
	 * Apply priority pinning to fit_cpus. Since priority pinning has a
	 * higher priority than ontime, it does not need to execute sequence
	 * afterwards if there is only one fit cpus.
	 */
	if (cpumask_intersects(&fit_cpus, &tex_pinning_cpus)) {
		cpumask_and(&fit_cpus, &fit_cpus, &tex_pinning_cpus);
		if (cpumask_weight(&fit_cpus) == 1)
			goto out;
	}

	/* Pick ontime fit cpus if ontime is applicable */
	if (cpumask_intersects(&fit_cpus, &ontime_fit_cpus))
		cpumask_and(&fit_cpus, &fit_cpus, &ontime_fit_cpus);

	/* Apply prefer cpu if it is applicable */
	if (cpumask_intersects(&fit_cpus, &prefer_cpus))
		cpumask_and(&fit_cpus, &fit_cpus, &prefer_cpus);

out:
	cpumask_and(&env->fit_cpus, &env->p->cpus_allowed, cpu_active_mask);
	cpumask_and(&env->fit_cpus, &env->fit_cpus, &fit_cpus);

	trace_ems_select_fit_cpus(env->p, env->wake,
		*(unsigned int *)cpumask_bits(&env->fit_cpus),
		*(unsigned int *)cpumask_bits(&env->cpus_allowed),
		*(unsigned int *)cpumask_bits(&tex_pinning_cpus),
		*(unsigned int *)cpumask_bits(&ontime_fit_cpus),
		*(unsigned int *)cpumask_bits(&prefer_cpus),
		*(unsigned int *)cpumask_bits(&overcap_cpus),
		*(unsigned int *)cpumask_bits(&busy_cpus),
		*(unsigned int *)cpumask_bits(&migration_cpus),
		*(unsigned int *)cpumask_bits(&non_preemptible_cpus));

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
	for_each_cpu_and(cpu, &env->p->cpus_allowed, cpu_active_mask) {
		env->cpu_stat[cpu].cap_max = capacity_max_of(cpu);
		env->cpu_stat[cpu].cap_orig = capacity_orig_of(cpu);
		env->cpu_stat[cpu].cap_orig = capacity_curr_of(cpu);

		env->cpu_stat[cpu].util_wo = ml_cpu_util_without(cpu, env->p);
		env->cpu_stat[cpu].util_with = ml_cpu_util_with(env->p, cpu);

		env->cpu_stat[cpu].runnable = ml_runnable_load_avg(cpu);

		if (cpu_equal(env->src_cpu, cpu))
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_with;
		else
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_wo;

		/* cumulative CPU demand */
		env->cpu_stat[cpu].util_cuml =
			min(env->cpu_stat[cpu].util + env->task_util_clamped, capacity_orig_of(cpu));
		if (task_in_cum_window_demand(cpu_rq(cpu), env->p))
			env->cpu_stat[cpu].util_cuml -= env->task_util;

		env->cpu_stat[cpu].nr_running = cpu_rq(cpu)->nr_running;
		env->cpu_stat[cpu].idle = idle_cpu(cpu);
	}
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
	if (cpu_overutilized(env->src_cpu))
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
	int task_boosted = max(env->boosted, env->task_on_top);
	struct cpumask active_fast_mask;

	/* Avoid recommending fast CPUs during idle as these are inactive */
	if (pm_freezing)
		goto done;

	/* Don't select CPU if task is not allowed to be placed on fast CPUs */
	if (!cpumask_intersects(&env->p->cpus_allowed, cpu_fastest_mask()))
		goto done;

	/* Get all active fast CPUs */
	cpumask_and(&active_fast_mask, cpu_fastest_mask(), cpu_active_mask);

	/* Don't select CPU if fast CPUs are unavailable */
	if (cpumask_empty(&active_fast_mask))
		goto done;

	/* Return fast CPU if task is boosted or global boosting */
	if (task_boosted || ems_boot_boost()) {
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
	struct cpumask active_cpus;
	struct cpumask online_cpus;
	struct cpumask cpus_allowed;

	if (is_somac_ready(env))
		goto out;

	/* clear masks */
	cpumask_clear(&env->cpus_allowed);
	cpumask_clear(&active_cpus);
	cpumask_clear(&online_cpus);
	cpumask_clear(&cpus_allowed);

	/*
	 * take a snapshot of cpumask to get CPUs allowed
	 * - active_cpus : cpu_active_mask
	 * - cpus_allowed : p->cpus_allowed
	 */
	cpumask_copy(&active_cpus, cpu_active_mask);
	cpumask_copy(&cpus_allowed, &env->p->cpus_allowed);

	/*
	 * Putting per-cpu kthread on other cpu is not allowed.
	 * It does not have to find cpus allowed in this case.
	 */
	if (is_per_cpu_kthread(env->p)) {
		cpumask_copy(&online_cpus, cpu_online_mask);
		cpumask_and(&env->cpus_allowed, &online_cpus, &cpus_allowed);
		goto out;
	}

	/*
	 * Given task must run on the CPU combined as follows:
	 *	cpu_active_mask
	 */
	cpumask_copy(&env->cpus_allowed, &active_cpus);
	if (cpumask_empty(&env->cpus_allowed))
		goto out;

	/*
	 * Unless task is per-cpu kthread, p->cpus_allowed does not cause a problem
	 * even if it is ignored. Consider p->cpus_allowed as possible, but if it
	 * does not overlap with CPUs allowed made above, ignore it.
	 */
	if (cpumask_intersects(&env->cpus_allowed, &cpus_allowed))
		cpumask_and(&env->cpus_allowed, &env->cpus_allowed, &cpus_allowed);

out:
	/* Return weight of allowed cpus */
	return cpumask_weight(&env->cpus_allowed);
}

static
void get_ready_env(struct tp_env *env)
{
	int cpu, nr_cpus;
	int semi_perf_placement;

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
	semi_perf_placement = (env->sched_policy == SCHED_POLICY_SEMI_PERF);

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
	if (semi_perf_placement)
		rcu_read_lock();

	for_each_cpu(cpu, &env->fit_cpus) {
		struct cpuidle_state *idle;
		env->cpu_stat[cpu].exit_latency = 0;

		if (env->cpu_stat[cpu].idle) {
			/* Get idle state for semi perf selection */
			if (semi_perf_placement) {
				idle = idle_get_state(cpu_rq(cpu));
				if (idle)
					env->cpu_stat[cpu].exit_latency = idle->exit_latency;
			}

			env->idle_cpu_count++;
		}
	}

	if (semi_perf_placement)
		rcu_read_unlock();
}

extern void sync_entity_load_avg(struct sched_entity *se);
inline int __ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sd_flag, int sync, int wake)
{
	int target_cpu = INVALID_CPU;
	char state[30] = "fail";
	struct tp_env env = {
		.p = p,
		.cgroup_idx = schedtune_task_group_idx(p),
		.src_cpu = prev_cpu,
		.task_on_top = ems_task_on_top(p),

		.boosted = ems_task_boosted(p),
		.latency_sensitive = uclamp_latency_sensitive(p),

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

int __init core_init()
{
	tex_init();

	return 0;
}
