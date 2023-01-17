/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
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

#include <linux/plist.h>
#include <linux/sched/idle.h>
#include <linux/sched/topology.h>

enum task_cgroup {
    CGROUP_ROOT,
    CGROUP_FOREGROUND,
    CGROUP_BACKGROUND,
    CGROUP_TOPAPP,
    CGROUP_RT,
    CGROUP_SYSTEM,
    CGROUP_SYSTEM_BACKGROUND,
    CGROUP_NNAPI_HAL,
    CGROUP_CAMERA_DAEMON,
    CGROUP_COUNT,
};

struct gb_qos_request {
	struct plist_node node;
	char *name;
	bool active;
};

#ifdef CONFIG_SCHED_EMS
extern struct kobject *ems_kobj;
extern unsigned int capacity_max_of(unsigned int cpu);

/* task util initialization */
extern void exynos_init_entity_util_avg(struct sched_entity *se);

/* active balance */
extern int exynos_need_active_balance(enum cpu_idle_type idle,
				struct sched_domain *sd, int src_cpu, int dst_cpu);

/* ems api */
extern int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sd_flag, int sync, int wake);
extern int ems_select_fallback_rq(struct task_struct *p);
extern int ems_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ems_idle_exit(int cpu, int state);
extern void ems_idle_enter(int cpu, int *state);
extern void ems_fork_init(struct task_struct *p);
extern void ems_post_init_entity_util_avg(struct sched_entity *se);
extern int ems_check_preempt_wakeup(struct task_struct *p);
extern void ems_init(void);

extern int ems_task_top_app(struct task_struct *p);
extern int ems_task_on_top(struct task_struct *p);
extern int ems_task_boosted(struct task_struct *p);

extern bool energy_initialized;
extern void set_energy_table_status(bool status);
extern bool get_energy_table_status(void);

/* ontime migration */
extern void ml_update_load_avg(u64 delta, int cpu, unsigned long weight, struct sched_avg *sa);
extern void ml_new_entity_load(struct task_struct *parent, struct sched_entity *se);

/* ontime migration */
extern void ontime_trace_task_info(struct task_struct *p);

/* load balance trigger */
extern bool lbt_overutilized(int cpu, int level);
extern void update_lbt_overutil(int cpu, unsigned long capacity);

/* ems boost */
#define EMS_INIT_BOOST 1
#define EMS_BOOT_BOOST 2

extern int ems_task_boost(void);
extern int ems_boot_boost(void);
extern int ems_global_boost(void);
extern void gb_qos_update_request(struct gb_qos_request *req, u32 new_value);

extern const struct cpumask *cpu_slowest_mask(void);
extern const struct cpumask *cpu_fastest_mask(void);
extern inline bool is_slowest_cpu(int cpu);

/* Task EXpress (TEX) */
extern void ems_set_tex_prio(int prio);
extern int ems_get_tex_prio(void);
extern void ems_set_tex_qjump_enabled(int enabled);
extern int ems_get_tex_qjump_enabled(void);

#else
static inline void exynos_init_entity_util_avg(struct sched_entity *se) { }

static inline int exynos_need_active_balance(enum cpu_idle_type idle,
				struct sched_domain *sd, int src_cpu, int dst_cpu)
{
	return 0;
}

static inline int
ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sd_flag, int sync, int wake)
{
	return -1;
}

static inline void ontime_migration(void) { }
static inline int ontime_can_migration(struct task_struct *p, int cpu)
{
	return 1;
}
static inline void ontime_update_load_avg(u64 delta, int cpu, unsigned long weight, struct sched_avg *sa) { }
static inline void ontime_new_entity_load(struct task_struct *p, struct sched_entity *se) { }
static inline void ontime_trace_task_info(struct task_struct *p) { }

static inline bool lbt_overutilized(int cpu, int level)
{
	return false;
}
static inline void update_lbt_overutil(int cpu, unsigned long capacity) { }

static inline int ems_boot_boost(void) { return 0; }
static inline int ems_global_boost(void) { return 0; }
static inline int ems_task_boost(void) { return 0; }
static inline void gb_qos_update_request(struct gb_qos_request *req, u32 new_value) { }

static inline bool is_slowest_cpu(int cpu)
{
	return false;
}
/* P.A.R.T */
static inline void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type) { }
static inline void part_cpu_active_ratio(unsigned long *util, unsigned long *max, int cpu) { }
static inline void set_part_period_start(struct rq *rq) { }
#endif /* CONFIG_SCHED_EMS */

#ifdef CONFIG_SIMPLIFIED_ENERGY_MODEL
extern void init_sched_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f);
#else
static inline void init_sched_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f) { }
#endif

/* Fluid Real Time */
extern unsigned int frt_disable_cpufreq;
