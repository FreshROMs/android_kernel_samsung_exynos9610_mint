
#ifdef CONFIG_SCHED_TUNE
#include <linux/reciprocal_div.h>

/*
 * EAS scheduler tunables for task groups.
 */

/* We hold schedtune boost in effect for at least this long */
#define SCHEDTUNE_BOOST_HOLD_NS 50000000ULL

/*
 * System energy normalization constants
 */
struct target_nrg {
	unsigned long min_power;
	unsigned long max_power;
	struct reciprocal_value rdiv;
};

/* SchdTune tunables for a group of tasks */
struct schedtune {
	/* SchedTune CGroup subsystem */
	struct cgroup_subsys_state css;

	/* Boost group allocated ID */
	int idx;

	/* Boost value for tasks on that SchedTune CGroup */
	int boost;

#ifdef CONFIG_SCHED_EMS
	/* Boost value for heavy tasks on that SchedTune CGroup */
	int heavy_boost;

	/* Boost value for busy tasks on that SchedTune CGroup */
	int busy_boost;
#endif

#ifndef CONFIG_UCLAMP_TASK_GROUP
	/* Hint to bias scheduling of tasks on that SchedTune CGroup
	 * towards idle CPUs */
	int prefer_idle;
#endif
};

/*
 * Maximum number of boost groups to support
 * When per-task boosting is used we still allow only limited number of
 * boost groups for two main reasons:
 * 1. on a real system we usually have only few classes of workloads which
 *    make sense to boost with different values (e.g. background vs foreground
 *    tasks, interactive vs low-priority tasks)
 * 2. a limited number allows for a simpler and more memory/time efficient
 *    implementation especially for the computation of the per-CPU boost
 *    value
 */
#define BOOSTGROUPS_COUNT 8

static inline struct schedtune *css_st(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct schedtune, css) : NULL;
}

static inline struct schedtune *task_schedtune(struct task_struct *tsk)
{
	return css_st(task_css(tsk, schedtune_cgrp_id));
}

static inline struct schedtune *parent_st(struct schedtune *st)
{
	return css_st(st->css.parent);
}

extern inline int schedtune_cpu_margin(unsigned long util, int cpu);
extern inline long schedtune_task_margin(struct task_struct *task);

#ifdef CONFIG_SCHED_EMS
int schedtune_task_group_idx(struct task_struct *tsk);
#endif

int schedtune_cpu_boost(int cpu);
int schedtune_task_boost(struct task_struct *tsk);

#ifdef CONFIG_UCLAMP_TASK_GROUP
#define schedtune_prefer_idle(tsk) 0
#else
int schedtune_prefer_idle(struct task_struct *tsk);
#endif

void schedtune_enqueue_task(struct task_struct *p, int cpu);
void schedtune_dequeue_task(struct task_struct *p, int cpu);

#else /* CONFIG_SCHED_TUNE */
#define schedtune_cpu_margin(util, cpu) 0
#define schedtune_task_margin(tsk) 0

#ifdef CONFIG_SCHED_EMS
#define schedtune_task_group_idx(tsk) 0
#endif

#define schedtune_cpu_boost(cpu)  0
#define schedtune_task_boost(tsk) 0

#define schedtune_prefer_idle(tsk) 0

#define schedtune_enqueue_task(task, cpu) do { } while (0)
#define schedtune_dequeue_task(task, cpu) do { } while (0)

#endif /* CONFIG_SCHED_TUNE */
