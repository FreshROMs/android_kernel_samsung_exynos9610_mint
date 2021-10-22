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
#define tsk_cpus_allowed(tsk)	(&(tsk)->cpus_allowed)

extern struct kobject *ems_kobj;

extern int select_service_cpu(struct task_struct *p);
extern int ontime_task_wakeup(struct task_struct *p, int sync);
extern int select_perf_cpu(struct task_struct *p);
extern int global_boosting(struct task_struct *p);
extern int global_boosted(void);
extern int select_energy_cpu(struct task_struct *p, int prev_cpu, int sd_flag, int sync);
extern unsigned int calculate_energy(struct task_struct *p, int target_cpu);
extern int band_play_cpu(struct task_struct *p);

#ifdef CONFIG_SCHED_TUNE
extern int prefer_perf_cpu(struct task_struct *p);
extern int prefer_idle_cpu(struct task_struct *p);
extern int group_balancing(struct task_struct *p);
#else
static inline int prefer_perf_cpu(struct task_struct *p) { return -1; }
static inline int prefer_idle_cpu(struct task_struct *p) { return -1; }
#endif

extern unsigned long cpu_util(int cpu);
extern unsigned long task_util(struct task_struct *p);
extern unsigned long cpu_util_wake(int cpu, struct task_struct *p);
extern unsigned long task_util_est(struct task_struct *p);
extern unsigned int get_cpu_mips(unsigned int cpu);
extern unsigned int get_cpu_max_capacity(unsigned int cpu);

extern unsigned long boosted_task_util(struct task_struct *p);

static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}
