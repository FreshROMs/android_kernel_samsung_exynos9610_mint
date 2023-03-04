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

/* support flag-handling for EMS */
#define EMS_PF_GET(task)        (task->ems_flags)
#define EMS_PF_SET(task, value)     (task->ems_flags |= (value))
#define EMS_PF_CLEAR(task, value)   (task->ems_flags &= ~(value))

#define EMS_CPU(task)        (task->ems_assigned_cpu)

#define EMS_PF_MULLIGAN             0x00000001  /* I'm given a mulligan */
#define EMS_PF_RUNNABLE_BUSY        0x00000002  /* Picked from runnable busy cpu */
#define EMS_PF_RT_VICTIM            0x00000003  /* Victim task from FRT */

/*
 * Helpers for converting millisecond timing to jiffy resolution
 */
#define MS_TO_JIFFIES(TIME) ((unsigned long)(TIME) / (MSEC_PER_SEC / HZ))

/*
 * Sysbusy
 */
enum sysbusy_state {
    SYSBUSY_STATE0 = 0,
    SYSBUSY_STATE1,
    SYSBUSY_STATE2,
    SYSBUSY_STATE3,
    NUM_OF_SYSBUSY_STATE,
};

#define SYSBUSY_CHECK_BOOST (0)
#define SYSBUSY_STATE_CHANGE    (1)

/* Sysbusy */
struct sysbusy_param {
    int monitor_interval;       /* tick (1 tick = 4ms) */
    int release_duration;       /* tick (1 tick = 4ms) */
    unsigned long allowed_next_state;
};

#define TICK_SEC    MS_TO_JIFFIES(1000)
#define BUSY_MONITOR_INTERVAL    MS_TO_JIFFIES(100)
static struct sysbusy_param sysbusy_params[] = {
    {
        /* SYSBUSY_STATE0 (sysbusy inactivation) */
        .monitor_interval   = MS_TO_JIFFIES(4),
        .release_duration   = 0,
        .allowed_next_state = (1 << SYSBUSY_STATE1) |
                      (1 << SYSBUSY_STATE2) |
                      (1 << SYSBUSY_STATE3),
    },
    {
        /* 
         * TenSeventy7:
         * Avoid instances where spiking loads, which is very frequent in
         * Android, would cause 'sysbusy' levels 0-1 to switch and trigger quickly.
         * Let the heavy load alleviate first by making STATE1 longer.
         */ 
        /* SYSBUSY_STATE1 */
        .monitor_interval   = MS_TO_JIFFIES(16),
        .release_duration   = MS_TO_JIFFIES(500),
        .allowed_next_state = (1 << SYSBUSY_STATE0) |
                      (1 << SYSBUSY_STATE2) |
                      (1 << SYSBUSY_STATE3),
    },
    {
        /* SYSBUSY_STATE2 */
        .monitor_interval   = BUSY_MONITOR_INTERVAL,
        .release_duration   = TICK_SEC * 3,
        .allowed_next_state = (1 << SYSBUSY_STATE0) |
                      (1 << SYSBUSY_STATE3),
    },
    {
        /* SYSBUSY_STATE3 */
        .monitor_interval   = BUSY_MONITOR_INTERVAL,
        .release_duration   = TICK_SEC * 9,
        .allowed_next_state = (1 << SYSBUSY_STATE0),
    },
};

#define SYSBUSY_SOMAC   SYSBUSY_STATE3

struct rq;

extern inline bool is_per_cpu_kthread(struct task_struct *p);

#ifdef CONFIG_SCHED_EMS
extern struct kobject *ems_kobj;

/* ems api */
extern int ems_need_active_balance(enum cpu_idle_type idle,
                struct sched_domain *sd, int src_cpu, int dst_cpu);
extern int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu, int sync, int wake);
extern int ems_select_fallback_rq(struct task_struct *p);
extern int ems_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ems_tick(struct rq *rq);
extern void ems_tick_locked(struct rq *rq);
extern void ems_enqueue_task(struct rq *rq, struct task_struct *p);
extern void ems_dequeue_task(struct rq *rq, struct task_struct *p);
extern void ems_wakeup_task(struct rq *rq, struct task_struct *p);
extern void ems_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
                struct sched_entity **se_ptr, bool *repick,
                bool simple, struct task_struct *prev);
extern int ems_load_balance(struct rq *rq);
extern void ems_post_init_entity_util_avg(struct sched_entity *se);
extern void ems_fork_init(struct task_struct *p);
extern int ems_check_preempt_wakeup(struct task_struct *p);
extern void ems_init(void);

extern int ems_task_boosted(struct task_struct *p);

extern unsigned int capacity_max_of(unsigned int cpu);

extern void ems_set_energy_table_status(bool status);
extern bool ems_get_energy_table_status(void);

/* load balance trigger */
extern bool ems_lbt_overutilized(int cpu, int level);
extern void ems_lbt_update_overutil(int cpu, unsigned long capacity);

/* ems boost */
#define EMS_INIT_BOOST 1
#define EMS_BOOT_BOOST 2

extern int ems_task_boost(void);
extern int ems_boot_boost(void);
extern int ems_global_boost(void);
extern void gb_qos_update_request(struct gb_qos_request *req, u32 new_value);
extern void ems_gpu_boost_update(s32 gpu_cur_freq);

extern const struct cpumask *cpu_slowest_mask(void);
extern const struct cpumask *cpu_fastest_mask(void);
extern inline bool et_cpu_slowest(int cpu);

extern int sysbusy_register_notifier(struct notifier_block *nb);
extern int sysbusy_unregister_notifier(struct notifier_block *nb);
#else
static inline void gb_qos_update_request(struct gb_qos_request *req, u32 new_value) { }
static inline bool et_cpu_slowest(int cpu)
{
	return false;
}

static inline int sysbusy_register_notifier(struct notifier_block *nb) { return 0; };
static inline int sysbusy_unregister_notifier(struct notifier_block *nb) { return 0; };
#endif /* CONFIG_SCHED_EMS */

#ifdef CONFIG_SIMPLIFIED_ENERGY_MODEL
extern void ems_init_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f);
#else
static inline void ems_init_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f) { }
#endif

/* Fluid Real Time */
extern unsigned int frt_disable_cpufreq;
