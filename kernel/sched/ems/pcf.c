/*
 * Performance CPU Finder
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpuidle.h>
#include <trace/events/ems.h>

#include "ems.h"
#include "../sched.h"

/*
 * Currently, PCF is composed of a selection algorithm based on distributed
 * processing, for example, selecting idle cpu or cpu with biggest spare
 * capacity. Although the current algorithm may suffice, it is necessary to
 * examine a selection algorithm considering cache hot and migration cost.
 */
int select_perf_cpu(struct eco_env *eenv)
{
	int cpu;
	unsigned long best_perf_cap_orig = 0;
	unsigned long best_perf_util = ULONG_MAX;
	unsigned long best_wake_util = ULONG_MAX;
	unsigned long best_active_util = ULONG_MAX;
	unsigned long max_spare_cap = 0;
	unsigned int min_exit_latency = UINT_MAX;
	int best_perf_cpu = -1;
	int best_active_cpu = -1;
	int backup_cpu = -1;

	rcu_read_lock();

	for_each_cpu_and(cpu, tsk_cpus_allowed(eenv->p), cpu_active_mask) {
		unsigned long capacity_orig = capacity_orig_of(cpu);
		unsigned long new_util, spare_cap, wake_util = cpu_util_without(cpu, eenv->p);

		new_util = wake_util + eenv->task_util;
		new_util = max(new_util, eenv->min_util);

		/* Skip over-capacity cpu */
		if (lbt_util_bring_overutilize(cpu, new_util))
			continue;

		/*
		 * A) Find best performance cpu.
		 *
		 * If the impact of cache hot and migration cost are excluded,
		 * distributed processing is the best way to achieve performance.
		 * To maximize performance, the idle cpu with the highest
		 * performance is selected first. If there are more than two idle
		 * cpus with the highest performance, choose the cpu with the
		 * shallowest idle state for fast reactivity.
		 */
		if (idle_cpu(cpu)) {
			struct cpuidle_state *idle;

			/* find biggest capacity cpu */
			if (capacity_orig < best_perf_cap_orig)
				continue;

			idle = idle_get_state(cpu_rq(cpu));

			if (!idle) {
				best_perf_cpu = cpu;
				continue;
			}

			/* find shallowest idle state cpu */
			if (capacity_orig == best_perf_cap_orig &&
				idle->exit_latency > min_exit_latency)
				continue;

			/* if same cstate, select lower util */
			if (capacity_orig == best_perf_cap_orig &&
				idle->exit_latency == min_exit_latency &&
				wake_util >= best_perf_util)
				continue;

			/* Keep track of best idle CPU */
			best_perf_cap_orig = capacity_orig;
			best_perf_util = wake_util;
			min_exit_latency = idle->exit_latency;
			best_perf_cpu = cpu;
			continue;
		}

		if (cpu_selected(best_perf_cpu))
			continue;

		/*
		 * B) Find backup performance cpu.
		 *
		 * Backup cpu also adopts distributed processing. In the absence
		 * of idle cpu, it is difficult to expect reactivity, so select
		 * the cpu with the biggest spare capacity to handle the most
		 * computations. Since a high performance cpu has a large capacity,
		 * cpu having a high performance is likely to be selected.
		 */
		spare_cap = capacity_orig - new_util;

		if (capacity_curr_of(cpu) > new_util &&
			spare_cap > max_spare_cap) {
			max_spare_cap = spare_cap;
			best_active_cpu = cpu;
			continue;
		}
		
		if (wake_util > best_wake_util)
			continue;

		if (new_util > best_active_util)
			continue;

		best_wake_util = wake_util;
		best_active_util = new_util;
		backup_cpu = cpu;
	}

	rcu_read_unlock();

	if (!cpu_selected(best_active_cpu))
		best_active_cpu = backup_cpu;

	trace_ems_select_perf_cpu(eenv->p, best_perf_cpu, best_active_cpu);

	if (best_perf_cpu == -1)
		return best_active_cpu;

	return best_perf_cpu;
}
