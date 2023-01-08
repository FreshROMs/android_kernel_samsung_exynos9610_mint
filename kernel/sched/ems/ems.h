/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../sched-pelt.h"

#define cpu_selected(cpu)	(cpu >= 0)
#define tsk_cpus_allowed(tsk)   (&(tsk)->cpus_allowed)
#define cpu_equal(a, b)	(unlikely(a == b))

extern struct kobject *ems_kobj;

/* Default CPU placeholder */
#define INVALID_CPU -1

/* Places which use this should consider cpumask_var_t. */
#define VENDOR_NR_CPUS      NR_CPUS

/* task scheduling policy */
#define SCHED_POLICY_EFF 0 // foreground/background
#define SCHED_POLICY_ENERGY 1
#define SCHED_POLICY_SEMI_PERF 2 // top-app/rt/camera-daemon
#define SCHED_POLICY_PERF 3 // prefer-perf/on-top/nnapi-hal
#define SCHED_POLICY_MIN_UTIL 4 // overutil case
#define SCHED_POLICY_UNKNOWN 5

/* structure for task placement environment */
struct tp_env {
    struct task_struct *p;
    int cgroup_idx;
    int src_cpu;

    /* start cpu */
    volatile struct {
        int cpu;
        unsigned int cap_max;
    } start_cpu;

    int sched_policy;
    int sync;
    int wake;

    int on_top;
    int prefer_idle;
    int prefer_perf;

    struct cpumask cpus_allowed;
    struct cpumask fit_cpus;

    unsigned long task_util;
    unsigned long min_util;

    volatile struct {
        unsigned long util;
        unsigned long util_wo;
        unsigned long util_with;
        unsigned long util_cuml;

        unsigned int cap_max;
        unsigned long cap_orig;
        unsigned long cap_curr;

        int idle;
        int exit_latency;
    } cpu_stat[NR_CPUS];

    int idle_cpu_count;
};

#define MISFIT_TASK_UTIL_RATIO  (80)

static inline
int cpu_overutilized(unsigned long capacity, unsigned long util)
{
    return (capacity * 1024) < (util * 1280);
}

static inline
struct task_struct *task_of(struct sched_entity *se)
{
    return container_of(se, struct task_struct, se);
}

extern int global_boosted(void);
extern int global_boosted_boot(void);

#ifdef CONFIG_SCHED_EMS
extern int ontime_can_migration(struct task_struct *p, int cpu);
#else
static inline int ontime_can_migration(struct task_struct *p, int cpu)
{
    return 1;
}
#endif


extern void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern void prefer_cpu_get(struct tp_env *env, struct cpumask *mask);

#if 0
extern unsigned int calculate_energy(struct task_struct *p, int target_cpu);
extern unsigned int calculate_efficiency(struct task_struct *p, int target_cpu);
#endif

extern unsigned int calculate_energy(struct tp_env *env, int target_cpu);
extern unsigned int calculate_efficiency(struct tp_env *env, int target_cpu);

extern int find_best_cpu(struct tp_env *env);

extern unsigned int capacity_max_of(unsigned int cpu);
extern unsigned long capacity_curr_of(int cpu);

extern unsigned long cpu_util_without(int cpu, struct task_struct *p);
extern unsigned long task_util_est(struct task_struct *p);

extern unsigned long boosted_task_util(struct task_struct *p);
extern unsigned long freqvar_st_boost_vector(int cpu);

extern unsigned int get_cpu_mips(unsigned int cpu);
extern unsigned long get_freq_cap(unsigned int cpu, unsigned long freq);

extern void init_part(void);
