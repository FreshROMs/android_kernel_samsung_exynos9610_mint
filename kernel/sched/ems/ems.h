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
#define cpu_equal(a, b)	(unlikely(a == b))

extern struct kobject *ems_kobj;

/* Default CPU placeholder */
#define INVALID_CPU -1

/* maximum count of tracking tasks in runqueue */
#define TRACK_TASK_COUNT    5

/* task scheduling policy */
#define SCHED_POLICY_EFF 0 // foreground/background
#define SCHED_POLICY_ENERGY 1
#define SCHED_POLICY_SEMI_PERF 2 // top-app/rt/camera-daemon
#define SCHED_POLICY_PERF 3 // prefer-perf/on-top/nnapi-hal
#define SCHED_POLICY_UNKNOWN 4

/*
 * Vendor data handling for TEX queue jump
 *
 * rq  's ems_qjump_list indices 0, 1    are used
 * task's ems_qjump_flags indices 0, 1, 2 are used
 */
#define ems_qjump_list(rq)      ((struct list_head *)&(rq->ems_qjump_list[0]))
#define ems_qjump_node(task)        ((struct list_head *)&(task->ems_qjump_node[0]))
#define ems_qjump_queued(task)      (task->ems_qjump_queued)

#define ems_qjump_first_entry(list) ({                      \
    void *__mptr = (void *)(list->next);                        \
    (struct task_struct *)(__mptr -                         \
                offsetof(struct task_struct, ems_qjump_node[0])); \
})

/* structure for task placement environment */
struct tp_env {
    struct task_struct *p;
    int cgroup_idx;
    int src_cpu;

    unsigned int sched_policy;
    int per_cpu_kthread;
    int sync;
    int wake;

    int boosted;
    int latency_sensitive;

    struct cpumask cpus_allowed;
    struct cpumask fit_cpus;

    unsigned long task_util;
    unsigned long task_util_clamped;
    unsigned long min_util;

    volatile struct {
        unsigned long util;
        unsigned long util_wo;
        unsigned long util_with;
        unsigned long util_cuml;
        unsigned long runnable;
        unsigned long load_avg;

        unsigned long cap_orig;

        unsigned long nr_running;

        int idle;
        int exit_latency;
    } cpu_stat[NR_CPUS];

    int idle_cpu_count;
};

/* core */
extern int __ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sync, int wake);
extern int ems_task_on_top(struct task_struct *p);
extern int core_init(void);

/* energy table */
extern unsigned int capacity_max_of(unsigned int cpu);
extern unsigned long capacity_curr_of(int cpu);
extern const unsigned int et_get_max_capacity(void);
extern unsigned int et_calculate_efficiency(struct tp_env *env, int target_cpu);
extern unsigned int et_calculate_energy(struct tp_env *env, int target_cpu);
extern unsigned int et_get_cpu_mips(unsigned int cpu);
extern unsigned long et_get_freq_cap(unsigned int cpu, unsigned long freq);

/* multi load */
extern void mlt_set_period_start(struct rq *rq);
extern void mlt_wakeup_task(struct rq *rq);
extern void mlt_dequeue_task(struct rq *rq);
extern void mlt_enqueue_task(struct rq *rq);
extern void mlt_update_recent(struct rq *rq);
extern int mlt_art_last_value(int cpu);
extern bool mlt_art_high_patten(struct mlt *mlt);
extern inline int mlt_art_boost(struct mlt *mlt);
extern inline int mlt_art_boost_limit(struct mlt *mlt);
extern inline int mlt_art_last_boost_time(struct mlt *mlt);
extern void mlt_init(void);
extern void ntu_apply(struct sched_entity *se);
extern unsigned long ml_task_util(struct task_struct *p);
extern unsigned long ml_task_util_est(struct task_struct *p);
extern unsigned long ml_cpu_util(int cpu);
extern unsigned long ml_cpu_util_with(struct task_struct *p, int dst_cpu);
extern unsigned long ml_cpu_util_without(int cpu, struct task_struct *p);
extern unsigned long ml_cpu_load_avg(int cpu);
extern unsigned long ml_runnable_load_avg(int cpu);
extern inline unsigned long ml_uclamp_task_util(struct task_struct *p);

#define MLT_PERIOD_SIZE     (4 * NSEC_PER_MSEC)
#define MLT_PERIOD_COUNT    10

/* ontime migration */
extern int ontime_can_migrate_task(struct task_struct *p, int cpu);
extern void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern void ontime_migration(void);

/*
 * Priority-pinning
 */
extern int tex_task(struct task_struct *p);
extern int tex_suppress_task(struct task_struct *p);
extern void tex_enqueue_task(struct task_struct *p, int cpu);
extern void tex_dequeue_task(struct task_struct *p, int cpu);

/* ems boost */
#define EMS_INIT_BOOST 1
#define EMS_BOOT_BOOST 2

extern int ems_task_boost(void);
extern int ems_boot_boost(void);
extern int ems_global_boost(void);
extern int ems_global_task_boost(int cgroup_idx);

/* profile */
struct system_profile_data {
    int         busy_cpu_count;
    int         heavy_task_count;
    int         misfit_task_count;

    unsigned long       cpu_util_sum;
    unsigned long       heavy_task_util_sum;
    unsigned long       misfit_task_util_sum;
    unsigned long       heaviest_task_util;

    unsigned long       cpu_util[NR_CPUS];
};

#define BUSY_CPU_RATIO (150)
#define HEAVY_TASK_UTIL_RATIO   (40)
#define MISFIT_TASK_UTIL_RATIO  (80)
#define check_busy(util, cap)   ((util * 100) >= (cap * 80))

extern inline int is_heavy_task_util(unsigned long util);
extern inline int is_misfit_task_util(unsigned long util);

extern int profile_sched_init(void);
extern int profile_sched_data(void);
extern void get_system_sched_data(struct system_profile_data *data);

/* Sysbusy */
extern void monitor_sysbusy(void);
extern int sysbusy_schedule(struct tp_env *env);
extern int sysbusy_activated(void);
extern void somac_tasks(void);
extern int sysbusy_on_somac(void);
extern int is_somac_ready(struct tp_env *env);
extern int sysbusy_sysfs_init(void);
extern int sysbusy_init(void);

/******************************************************************************
 * common API                                                                 *
 ******************************************************************************/
extern inline int cpu_overutilized(int cpu);

static inline struct sched_entity *se_of(struct sched_avg *sa)
{
    return container_of(sa, struct sched_entity, avg);
}

static inline
struct task_struct *task_of(struct sched_entity *se)
{
    return container_of(se, struct task_struct, se);
}

static inline bool __is_busy_cpu(unsigned long util,
                 unsigned long runnable,
                 unsigned long capacity,
                 unsigned long nr_running)
{
    if (runnable < capacity)
        return false;

    if (!nr_running)
        return false;

    if (util * BUSY_CPU_RATIO >= runnable * 100)
        return false;

    return true;
}

extern inline bool is_busy_cpu(int cpu);
