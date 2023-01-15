/*
 * Multi-purpose Load tracker
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/sched.h>
#include <linux/ems.h>

#include "../sched.h"
#ifndef CONFIG_UCLAMP_TASK_GROUP
#include "../tune.h"
#endif
#include "ems.h"

#include <trace/events/ems.h>

/******************************************************************************
 *                           MULTI LOAD for TASK                              *
 ******************************************************************************/
/*
 * ml_task_util - task util
 *
 * Task utilization. The calculation is the same as the task util of cfs.
 */
unsigned long ml_task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

#define UTIL_AVG_UNCHANGED 0x1

/*
 * ml_task_util_est - task util with util-est
 *
 * Task utilization with util-est, The calculation is the same as
 * task_util_est of cfs.
 */
static unsigned long _ml_task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return (max(ue.ewma, ue.enqueued) | UTIL_AVG_UNCHANGED);
}

unsigned long ml_task_util_est(struct task_struct *p)
{
	return max(ml_task_util(p), _ml_task_util_est(p));
}

/*
 * ml_uclamp_task_util - clamped task util-est with uclamp
 *
 * Task utilization with util-est clamped with uclamp
 */
#ifdef CONFIG_UCLAMP_TASK
inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
    return clamp(ml_task_util_est(p),
             uclamp_eff_value(p, UCLAMP_MIN),
             uclamp_eff_value(p, UCLAMP_MAX));
}
#else
inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
    return ml_task_util_est(p);
}
#endif

/*
 * ml_task_load_avg - task ontime load_avg
 */
#ifndef CONFIG_UCLAMP_TASK_GROUP
extern long schedtune_margin(unsigned long capacity, unsigned long signal, long boost);
extern int schedtune_task_boost(struct task_struct *p);
#endif
unsigned long ml_task_load_avg(struct task_struct *p)
{
	unsigned long load_avg = ml_of(p)->avg.load_avg;
#ifdef CONFIG_UCLAMP_TASK_GROUP
	unsigned long util_min = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long util_max = uclamp_eff_value(p, UCLAMP_MAX);

	return clamp(load_avg, util_min, util_max);
#else
	int boost = schedtune_task_boost(p);
	unsigned long capacity;

	if (boost == 0)
		return load_avg;

	capacity = capacity_orig_of(task_cpu(p));
	return load_avg + schedtune_margin(capacity, load_avg, boost);
#endif
}

/******************************************************************************
 *                            MULTI LOAD for CPU                              *
 ******************************************************************************/
/*
 * ml_cpu_util - cpu utilization
 */
unsigned long ml_cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/*
 * ml_cpu_util_without - cpu utilization without waking task
 */
unsigned long ml_cpu_util_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned int util = READ_ONCE(cfs_rq->avg.util_avg);

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return util;

	/* Discount task's util from CPU's util */
	lsub_positive(&util, ml_task_util(p));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

/*
 * ml_cpu_util_with - cpu utilization with waking task
 */
unsigned long ml_cpu_util_with(struct task_struct *p, int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util = READ_ONCE(cfs_rq->avg.util_avg);

	/* Discount task's util from prev CPU's util */
	if (cpu == task_cpu(p))
		lsub_positive(&util, ml_task_util(p));

	util += max(ml_task_util_est(p), (unsigned long)1);

	return min(util, capacity_orig_of(cpu));
}

/*
 * ml_cpu_load_avg - cpu load_avg
 */
unsigned long ml_cpu_load_avg(int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	return cfs_rq->avg.load_avg;
}

/*
 * ml_runnable_load_avg - cpu runnable_load_avg
 */
unsigned long ml_runnable_load_avg(int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	return cfs_rq->runnable_load_avg;
}

extern u64 decay_load(u64 val, u64 n);
static u32 __accumulate_pelt_segments(u64 periods, u32 d1, u32 d3)
{
	u32 c1, c2, c3 = d3;

	c1 = decay_load((u64)d1, periods);
	c2 = LOAD_AVG_MAX - decay_load(LOAD_AVG_MAX, periods) - 1024;

	return c1 + c2 + c3;
}

/*
 * ml_update_load_avg : load tracking for multi-load
 *
 * @sa : sched_avg to be updated
 * @delta : elapsed time since last update
 * @period_contrib : amount already accumulated against our next period
 * @scale_freq : scale vector of cpu frequency
 * @scale_cpu : scale vector of cpu capacity
 */
void ml_update_load_avg(u64 delta, int cpu, unsigned long weight, struct sched_avg *sa)
{
	struct ml_avg *avg = &se_of(sa)->ml.avg;
	unsigned long scale_freq, scale_cpu;
	u32 contrib = (u32)delta; /* p == 0 -> delta < 1024 */
	u64 periods;

	scale_freq = arch_scale_freq_capacity(NULL, cpu);
	scale_cpu = arch_scale_cpu_capacity(NULL, cpu);

	delta += avg->period_contrib;
	periods = delta / 1024; /* A period is 1024us (~1ms) */

	if (periods) {
		avg->load_sum = decay_load(avg->load_sum, periods);

		delta %= 1024;
		contrib = __accumulate_pelt_segments(periods,
				1024 - avg->period_contrib, delta);
	}
	avg->period_contrib = delta;

	if (weight) {
		contrib = cap_scale(contrib, scale_freq);
		avg->load_sum += contrib * scale_cpu;
	}

	if (!periods)
		return;

	avg->load_avg = div_u64(avg->load_sum, LOAD_AVG_MAX - 1024 + avg->period_contrib);
	ontime_update_next_balance(cpu, avg);
}

void ml_new_entity_load(struct task_struct *parent, struct sched_entity *se)
{
	struct ml_entity *ml;

	if (entity_is_cfs_rq(se))
		return;

	ml = &se->ml;

	ml->avg.load_sum = ml_of(parent)->avg.load_sum >> 1;
	ml->avg.load_avg = ml_of(parent)->avg.load_avg >> 1;
	ml->avg.period_contrib = 1023;
	ml->migrating = 0;

	trace_ems_ontime_new_entity_load(task_of(se), &ml->avg);
}

/******************************************************************************
 *                     New task utilization init                              *
 ******************************************************************************/
void ntu_apply(struct sched_entity *se)
{
	struct cfs_rq *cfs_rq = se->cfs_rq;
	struct sched_avg *sa = &se->avg;
	int cpu = cpu_of(cfs_rq->rq);
	unsigned long cap_org = capacity_orig_of(cpu);
	long cap = (long)(cap_org - cfs_rq->avg.util_avg) / 2;
	int ratio = 25;

	if (cap > 0) {
		if (cfs_rq->avg.util_avg != 0) {
			sa->util_avg  = cfs_rq->avg.util_avg * se->load.weight;
			sa->util_avg /= (cfs_rq->avg.load_avg + 1);

			if (sa->util_avg > cap)
				sa->util_avg = cap;
		} else {
			sa->util_avg = cap_org * ratio / 100;
		}

		/*
		 * If we wish to restore tuning via setting initial util,
		 * this is where we should do it.
		 */
		sa->util_sum = (u32)(sa->util_avg * LOAD_AVG_MAX);
	}
}
