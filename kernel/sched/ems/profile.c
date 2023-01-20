/*
 * Scheduling status profiler for Exynos Mobile Scheduler
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd
 * Park Choonghoon <choong.park@samsung.com>
 */

#include <linux/ems.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>

static struct system_profile_data *system_profile_data;
static DEFINE_RWLOCK(profile_sched_lock);

/****************************************************************
 *			External APIs				*
 ****************************************************************/
static u64 last_profile_time;
static int profile_interval = MS_TO_JIFFIES(4); /* 1 tick = 4ms */
int profile_sched_data(void)
{
	unsigned long flags;
	unsigned long rq_flags;
	unsigned long now = jiffies;
	unsigned long cpu_util_sum = 0;
	unsigned long heavy_task_util_sum = 0;
	unsigned long misfit_task_util_sum = 0;
	unsigned long heaviest_task_util = 0;
	int busy_cpu_count = 0;
	int heavy_task_count = 0;
	int misfit_task_count = 0;
	int cpu;

	if (!write_trylock_irqsave(&profile_sched_lock, flags))
		return -EBUSY;

	if (now < last_profile_time + profile_interval)
		goto unlock;

	last_profile_time = now;

	for_each_cpu(cpu, cpu_active_mask) {
		struct rq *rq = cpu_rq(cpu);
		struct task_struct *p;
		unsigned long cpu_util, task_util;
		int track_count;

		/* count busy cpu and add up total cpu util */
		cpu_util = ml_cpu_util(cpu);
		cpu_util_sum += cpu_util;
		if (check_busy(cpu_util, capacity_orig_of(cpu)))
			busy_cpu_count++;

		system_profile_data->cpu_util[cpu] = cpu_util;

		raw_spin_lock_irqsave(&rq->lock, rq_flags);

		if (!rq->cfs.curr)
			goto rq_unlock;

		/* Explictly clear count */
		track_count = 0;

		list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
			task_util = ml_task_util(p);
			if (is_heavy_task_util(task_util)) {
				heavy_task_count++;
				heavy_task_util_sum += task_util;
			}

			if (is_misfit_task_util(task_util)) {
				misfit_task_count++;
				misfit_task_util_sum += task_util;
			}

			if (heaviest_task_util < task_util)
				heaviest_task_util = task_util;
			if (++track_count >= TRACK_TASK_COUNT)
				break;
		}
rq_unlock:
		raw_spin_unlock_irqrestore(&rq->lock, rq_flags);
	}

	trace_ems_profile_tasks(busy_cpu_count, cpu_util_sum,
				heavy_task_count, heavy_task_util_sum,
				misfit_task_count, misfit_task_util_sum);

	/* Fill profile data */
	system_profile_data->busy_cpu_count = busy_cpu_count;
	system_profile_data->heavy_task_count = heavy_task_count;
	system_profile_data->misfit_task_count = misfit_task_count;
	system_profile_data->cpu_util_sum = cpu_util_sum;
	system_profile_data->heavy_task_util_sum = heavy_task_util_sum;
	system_profile_data->misfit_task_util_sum = misfit_task_util_sum;
	system_profile_data->heaviest_task_util = heaviest_task_util;

unlock:
	write_unlock_irqrestore(&profile_sched_lock, flags);

	return 0;
}

/* Caller MUST disable irq before calling this function. */
void get_system_sched_data(struct system_profile_data *data)
{
	read_lock(&profile_sched_lock);
	memcpy(data, system_profile_data, sizeof(struct system_profile_data));
	read_unlock(&profile_sched_lock);
}

/****************************************************************
 *		  sysbusy state change notifier			*
 ****************************************************************/
static int profile_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	profile_interval = sysbusy_params[state].monitor_interval;

	return NOTIFY_OK;
}

static struct notifier_block profile_sysbusy_notifier = {
	.notifier_call = profile_sysbusy_notifier_call,
};

/****************************************************************
 *			Initialization				*
 ****************************************************************/
int profile_sched_init(void)
{
	system_profile_data =
		kzalloc(sizeof(struct system_profile_data), GFP_KERNEL);
	if (!system_profile_data) {
		pr_err("Failed to allocate profile_system_data\n");
		return -ENOMEM;
	}

	sysbusy_register_notifier(&profile_sysbusy_notifier);

	return 0;
}
