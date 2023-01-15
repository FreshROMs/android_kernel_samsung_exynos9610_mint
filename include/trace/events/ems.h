/*
 *  Copyright (C) 2017 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ems

#if !defined(_TRACE_EMS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EMS_H

#include <linux/sched.h>
#include <linux/tracepoint.h>

struct tp_env;

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_select_fit_cpus,

	TP_PROTO(struct task_struct *p, int wake,
		unsigned int fit_cpus, unsigned int cpus_allowed, unsigned int tex_pinning_cpus, unsigned int ontime_fit_cpus, unsigned int prefer_cpus,
		unsigned int overutil_cpus, unsigned int busy_cpus, unsigned int migration_cpus, unsigned int non_preemptible_cpus, unsigned int free_cpus),

	TP_ARGS(p, wake, fit_cpus, cpus_allowed, tex_pinning_cpus, ontime_fit_cpus, prefer_cpus,
				overutil_cpus, busy_cpus, migration_cpus, non_preemptible_cpus, free_cpus),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		src_cpu			)
		__field(	int,		wake			)
		__field(	unsigned int,	fit_cpus		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	tex_pinning_cpus		)
		__field(	unsigned int,	ontime_fit_cpus		)
		__field(	unsigned int,	prefer_cpus		)
		__field(	unsigned int,	overutil_cpus		)
		__field(	unsigned int,	busy_cpus		)
		__field(	unsigned int,	migration_cpus		)
		__field(	unsigned int,	non_preemptible_cpus		)
		__field(	unsigned int,	free_cpus		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->src_cpu		= task_cpu(p);
		__entry->wake			= wake;
		__entry->fit_cpus		= fit_cpus;
		__entry->cpus_allowed		= cpus_allowed;
		__entry->tex_pinning_cpus	= tex_pinning_cpus;
		__entry->ontime_fit_cpus	= ontime_fit_cpus;
		__entry->prefer_cpus	= prefer_cpus;
		__entry->overutil_cpus		= overutil_cpus;
		__entry->busy_cpus		= busy_cpus;
		__entry->migration_cpus		= migration_cpus;
		__entry->non_preemptible_cpus		= non_preemptible_cpus;
		__entry->free_cpus		= free_cpus;
	),

	TP_printk("comm=%s pid=%d src_cpu=%d wake=%d fit_cpus=%#x cpus_allowed=%#x tex_pinning_cpus=%#x ontime_fit_cpus=%#x prefer_cpus=%#x overutil_cpus=%#x busy_cpus=%#x migration_cpus=%#x non_preemptible_cpus=%#x free_cpus=%#x",
		  __entry->comm, __entry->pid, __entry->src_cpu,  __entry->wake,
		  __entry->fit_cpus, __entry->cpus_allowed, __entry->tex_pinning_cpus, __entry->ontime_fit_cpus, __entry->prefer_cpus,
		  __entry->overutil_cpus, __entry->busy_cpus, __entry->migration_cpus, __entry->non_preemptible_cpus, __entry->free_cpus)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_wakeup_balance,

	TP_PROTO(struct task_struct *p, int target_cpu, int wake, char *state),

	TP_ARGS(p, target_cpu, wake, state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		target_cpu		)
		__field(	int,		wake		)
		__array(	char,		state,		30	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->target_cpu	= target_cpu;
		__entry->wake	= wake;
		memcpy(__entry->state, state, 30);
	),

	TP_printk("comm=%s pid=%d target_cpu=%d wake=%d state=%s",
		  __entry->comm, __entry->pid, __entry->target_cpu, __entry->wake, __entry->state)
);

/*
 * Tracepoint for global boost
 */
TRACE_EVENT(ems_global_boost,

	TP_PROTO(char *name, int boost),

	TP_ARGS(name, boost),

	TP_STRUCT__entry(
		__array(	char,	name,	64	)
		__field(	int,	boost		)
	),

	TP_fast_assign(
		memcpy(__entry->name, name, 64);
		__entry->boost		= boost;
	),

	TP_printk("name=%s global_boost=%d", __entry->name, __entry->boost)
);

/*
 * Tracepoint for ontime migration
 */
TRACE_EVENT(ems_ontime_migration,

	TP_PROTO(struct task_struct *p, unsigned long load,
		int src_cpu, int dst_cpu, char *reason),

	TP_ARGS(p, load, src_cpu, dst_cpu, reason),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	load			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__array( char,		reason,	16		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->load		= load;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		strncpy(__entry->reason, reason, 15);
	),

	TP_printk("comm=%s pid=%d load_avg=%lu src_cpu=%d dst_cpu=%d reason=%s",
		__entry->comm, __entry->pid, __entry->load,
		__entry->src_cpu, __entry->dst_cpu, __entry->reason)
);

/*
 * Tracepoint for accounting ontime load averages for tasks.
 */
TRACE_EVENT(ems_ontime_new_entity_load,

	TP_PROTO(struct task_struct *tsk, struct ml_avg *avg),

	TP_ARGS(tsk, avg),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN		)
		__field( pid_t,		pid				)
		__field( int,		cpu				)
		__field( unsigned long,	load_avg			)
		__field( u64,		load_sum			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= task_cpu(tsk);
		__entry->load_avg		= avg->load_avg;
		__entry->load_sum		= avg->load_sum;
	),
	TP_printk("comm=%s pid=%d cpu=%d load_avg=%lu load_sum=%llu",
		  __entry->comm,
		  __entry->pid,
		  __entry->cpu,
		  __entry->load_avg,
		  (u64)__entry->load_sum)
);

/*
 * Tracepoint for accounting ontime load averages for tasks.
 */
TRACE_EVENT(ems_ontime_load_avg_task,

	TP_PROTO(struct task_struct *tsk, struct ml_avg *avg, int ontime_flag),

	TP_ARGS(tsk, avg, ontime_flag),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN		)
		__field( pid_t,		pid				)
		__field( int,		cpu				)
		__field( unsigned long,	load_avg			)
		__field( u64,		load_sum			)
		__field( int,		ontime_flag			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= task_cpu(tsk);
		__entry->load_avg		= avg->load_avg;
		__entry->load_sum		= avg->load_sum;
		__entry->ontime_flag		= ontime_flag;
	),
	TP_printk("comm=%s pid=%d cpu=%d load_avg=%lu load_sum=%llu ontime_flag=%d",
		  __entry->comm, __entry->pid, __entry->cpu, __entry->load_avg,
		  (u64)__entry->load_sum, __entry->ontime_flag)
);

TRACE_EVENT(ems_ontime_check_migrate,

	TP_PROTO(struct task_struct *tsk, int cpu, int migrate, char *label),

	TP_ARGS(tsk, cpu, migrate, label),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field( int,		cpu			)
		__field( int,		migrate			)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= cpu;
		__entry->migrate		= migrate;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("comm=%s pid=%d target_cpu=%d migrate=%d reason=%s",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->migrate, __entry->label)
);

TRACE_EVENT(ems_lbt_overutilized,

	TP_PROTO(int cpu, int level, unsigned long util, unsigned long capacity, bool overutilized),

	TP_ARGS(cpu, level, util, capacity, overutilized),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		level			)
		__field( unsigned long,	util			)
		__field( unsigned long,	capacity		)
		__field( bool,		overutilized		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->level			= level;
		__entry->util			= util;
		__entry->capacity		= capacity;
		__entry->overutilized		= overutilized;
	),

	TP_printk("cpu=%d level=%d util=%lu capacity=%lu overutilized=%d",
		__entry->cpu, __entry->level, __entry->util,
		__entry->capacity, __entry->overutilized)
);
TRACE_EVENT(ems_cpu_active_ratio_patten,

	TP_PROTO(int cpu, int p_count, int p_avg, int p_stdev),

	TP_ARGS(cpu, p_count, p_avg, p_stdev),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( int,	p_count				)
		__field( int,	p_avg				)
		__field( int,	p_stdev				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->p_count		= p_count;
		__entry->p_avg			= p_avg;
		__entry->p_stdev		= p_stdev;
	),

	TP_printk("cpu=%d p_count=%2d p_avg=%4d p_stdev=%4d",
		__entry->cpu, __entry->p_count, __entry->p_avg, __entry->p_stdev)
);

TRACE_EVENT(ems_cpu_active_ratio,

	TP_PROTO(int cpu, struct part *pa, char *event),

	TP_ARGS(cpu, pa, event),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( u64,	start				)
		__field( int,	recent				)
		__field( int,	last				)
		__field( int,	avg				)
		__field( int,	max				)
		__field( int,	est				)
		__array( char,	event,		64		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->start			= pa->period_start;
		__entry->recent			= pa->active_ratio_recent;
		__entry->last			= pa->hist[pa->hist_idx];
		__entry->avg			= pa->active_ratio_avg;
		__entry->max			= pa->active_ratio_max;
		__entry->est			= pa->active_ratio_est;
		strncpy(__entry->event, event, 63);
	),

	TP_printk("cpu=%d start=%llu recent=%4d last=%4d avg=%4d max=%4d est=%4d event=%s",
		__entry->cpu, __entry->start, __entry->recent, __entry->last,
		__entry->avg, __entry->max, __entry->est, __entry->event)
);

TRACE_EVENT(ems_cpu_active_ratio_util_stat,

	TP_PROTO(int cpu, unsigned long part_util, unsigned long pelt_util),

	TP_ARGS(cpu, part_util, pelt_util),

	TP_STRUCT__entry(
		__field( int,		cpu					)
		__field( unsigned long,	part_util				)
		__field( unsigned long,	pelt_util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->part_util		= part_util;
		__entry->pelt_util		= pelt_util;
	),

	TP_printk("cpu=%d part_util=%lu pelt_util=%lu", __entry->cpu, __entry->part_util, __entry->pelt_util)
);

/*
 * Tracepoint for frequency variant boost
 */

TRACE_EVENT(ems_freqvar_st_boost,

	TP_PROTO(int cpu, int ratio),

	TP_ARGS(cpu, ratio),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ratio			= ratio;
	),

	TP_printk("cpu=%d ratio=%d", __entry->cpu, __entry->ratio)
);
TRACE_EVENT(ems_freqvar_boost,

	TP_PROTO(int cpu, int ratio, unsigned long step_max_util,
			unsigned long util, unsigned long boosted_util),

	TP_ARGS(cpu, ratio, step_max_util, util, boosted_util),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	step_max_util		)
		__field( unsigned long,	util			)
		__field( unsigned long,	boosted_util		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ratio			= ratio;
		__entry->step_max_util		= step_max_util;
		__entry->util			= util;
		__entry->boosted_util		= boosted_util;
	),

	TP_printk("cpu=%d ratio=%d step_max_util=%lu util=%lu boosted_util=%lu",
		  __entry->cpu, __entry->ratio, __entry->step_max_util,
		  __entry->util, __entry->boosted_util)
);
#endif /* _TRACE_EMS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
