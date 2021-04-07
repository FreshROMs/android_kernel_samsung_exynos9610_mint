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

struct gb_qos_request {
	struct plist_node node;
	char *name;
	bool active;
};

#define LEAVE_BAND	0

struct task_band {
	int id;
	pid_t tgid;
	raw_spinlock_t lock;

	struct list_head members;
	int member_count;
	struct cpumask playable_cpus;

	unsigned long util;
	unsigned long last_update_time;
};

#ifdef CONFIG_SCHED_EMS
extern struct kobject *ems_kobj;
extern unsigned int get_cpu_max_capacity(unsigned int cpu);

/* task util initialization */
extern void exynos_init_entity_util_avg(struct sched_entity *se);

/* active balance */
extern int exynos_need_active_balance(enum cpu_idle_type idle,
				struct sched_domain *sd, int src_cpu, int dst_cpu);

/* wakeup balance */
extern int
exynos_wakeup_balance(struct task_struct *p, int prev_cpu, int sd_flag, int sync);

/* ontime migration */
extern void ontime_migration(void);
extern int ontime_can_migration(struct task_struct *p, int cpu);
extern void ontime_update_load_avg(u64 delta, int cpu, unsigned long weight, struct sched_avg *sa);
extern void ontime_new_entity_load(struct task_struct *parent, struct sched_entity *se);
extern void ontime_trace_task_info(struct task_struct *p);

/* load balance trigger */
extern bool lbt_overutilized(int cpu, int level);
extern void update_lbt_overutil(int cpu, unsigned long capacity);

/* global boost */
extern void gb_qos_update_request(struct gb_qos_request *req, u32 new_value);

/* task band */
extern void sync_band(struct task_struct *p, bool join);
extern void newbie_join_band(struct task_struct *newbie);
extern int alloc_bands(void);
extern void update_band(struct task_struct *p, long old_util);
extern int band_playing(struct task_struct *p, int cpu);
#else
static inline void exynos_init_entity_util_avg(struct sched_entity *se) { }

static inline int exynos_need_active_balance(enum cpu_idle_type idle,
				struct sched_domain *sd, int src_cpu, int dst_cpu)
{
	return 0;
}

static inline int
exynos_wakeup_balance(struct task_struct *p, int prev_cpu, int sd_flag, int sync)
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

static inline void gb_qos_update_request(struct gb_qos_request *req, u32 new_value) { }

static inline void sync_band(struct task_struct *p, bool join) { }
static inline void newbie_join_band(struct task_struct *newbie) { }
static inline int alloc_bands(void)
{
	return 0;
}
static inline void update_band(struct task_struct *p, long old_util) { }
static inline int band_playing(struct task_struct *p, int cpu)
{
	return 0;
}
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
