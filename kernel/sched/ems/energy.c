/*
 * Energy efficient cpu selection
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpufreq.h>
#include <linux/cpuidle.h>
#include <trace/events/ems.h>

#include "ems.h"
#include "../sched.h"
#include "../tune.h"

/*
 * The compute capacity, power consumption at this compute capacity and
 * frequency of state. The cap and power are used to find the energy
 * efficiency cpu, and the frequency is used to create the capacity table.
 */
struct energy_state {
	unsigned long cap;
	unsigned long power;
	unsigned long frequency;
};

/*
 * Each cpu can have its own mips, coefficient and energy table. Generally,
 * cpus in the same frequency domain have the same mips, coefficient and
 * energy table.
 */
struct energy_table {
	unsigned int mips;
	unsigned int coefficient;;

	struct energy_state *states;

	unsigned int nr_states;
};
DEFINE_PER_CPU(struct energy_table, energy_table);

struct cpumask slowest_mask;
struct cpumask fastest_mask;

const struct cpumask *cpu_slowest_mask(void)
{
	return &slowest_mask;
}

const struct cpumask *cpu_fastest_mask(void)
{
	return &fastest_mask;
}

inline bool is_slowest_cpu(int cpu)
{
	return cpumask_test_cpu(cpu, cpu_slowest_mask());
}

inline unsigned int get_cpu_mips(unsigned int cpu)
{
	return per_cpu(energy_table, cpu).mips;
}

unsigned int get_cpu_max_capacity(unsigned int cpu)
{
	struct energy_table *table = &per_cpu(energy_table, cpu);

	/* If energy table wasn't initialized, return 0 as capacity */
	if (!table->states)
		return 0;

	return table->states[table->nr_states - 1].cap;
}

unsigned long get_freq_cap(unsigned int cpu, unsigned long freq)
{
	struct energy_table *table = &per_cpu(energy_table, cpu);
	struct energy_state *state = NULL;
	int i;

	for (i = 0; i < table->nr_states; i++) {
		if (table->states[i].frequency >= freq) {
			state = &table->states[i];
			break;
		}
	}

	if (!state)
		return 0;

	return state->cap;
}

static unsigned long normalized_util(unsigned long util, unsigned long capacity)
{
	if (util >= capacity)
		return SCHED_CAPACITY_SCALE;

	return util << SCHED_CAPACITY_SHIFT / capacity;
}

unsigned int calculate_energy(struct task_struct *p, int target_cpu)
{
	unsigned long util[NR_CPUS] = {0, };
	unsigned int total_energy = 0;
	int cpu;

	/*
	 * 0. Calculate utilization of the entire active cpu when task
	 *    is assigned to target cpu.
	 */
	for_each_cpu(cpu, cpu_active_mask) {
		util[cpu] = cpu_util_without(cpu, p);

		if (unlikely(cpu == target_cpu))
			util[cpu] += task_util_est(p);
	}

	for_each_cpu(cpu, cpu_active_mask) {
		struct energy_table *table;
		unsigned long max_util = 0, util_sum = 0;
		unsigned long capacity;
		int i, cap_idx;

		/* Compute coregroup energy with only one cpu per coregroup */
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		/*
		 * 1. The cpu in the coregroup has same capacity and the
		 *    capacity depends on the cpu that has the biggest
		 *    utilization. Find biggest utilization in the coregroup
		 *    to know what capacity the cpu will have.
		 */
		for_each_cpu(i, cpu_coregroup_mask(cpu))
			if (util[i] > max_util)
				max_util = util[i];

		/*
		 * 2. Find the capacity according to biggest utilization in
		 *    coregroup.
		 */
		table = &per_cpu(energy_table, cpu);
		cap_idx = table->nr_states - 1;
		for (i = 0; i < table->nr_states; i++) {
			if (table->states[i].cap >= max_util) {
				capacity = table->states[i].cap;
				cap_idx = i;
				break;
			}
		}

		/*
		 * 3. Get the utilization sum of coregroup. Since cpu
		 *    utilization of CFS reflects the performance of cpu,
		 *    normalize the utilization to calculate the amount of
		 *    cpu usuage that excludes cpu performance.
		 */
		for_each_cpu(i, cpu_coregroup_mask(cpu)) {
			if (i == task_cpu(p))
				util[i] -= min_t(unsigned long, util[i], task_util_est(p));

			if (i == target_cpu)
				util[i] += task_util_est(p);

			/* utilization with task exceeds max capacity of cpu */
			if (util[i] >= capacity) {
				util_sum += SCHED_CAPACITY_SCALE;
				continue;
			}

			/* normalize cpu utilization */
			util_sum += normalized_util(util[i], capacity);
		}

		/*
		 * 4. compute active energy
		 */
		total_energy += util_sum * table->states[cap_idx].power;
	}

	return total_energy;
}

struct cpu_env {
	int cpu;
	int idle_idx;
	unsigned long util;
	unsigned int exit_latency;
};

static void find_min_util_cpu(struct cpu_env *cenv, struct cpumask *mask, struct eco_env *eenv)
{
	struct cpuidle_state *state = NULL;
	unsigned long min_util = ULONG_MAX;
	int idle_idx = INT_MAX;
	int min_util_cpu = -1;
	bool boosted = (eenv->boost > 0);
	int cpu;

	/* Find energy efficient cpu in each coregroup. */
	for_each_cpu_and(cpu, mask, cpu_active_mask) {
		unsigned long new_util = cpu_util_without(cpu, eenv->p) + eenv->task_util;
		new_util = max(new_util, eenv->min_util);

		/* Skip over-capacity cpu */
		if (lbt_util_bring_overutilize(cpu, new_util))
			continue;

		/* Skip non-preemptible CPUs for non-boosted tasks */
		if (!boosted && !is_slowest_cpu(cpu) &&
			!is_cpu_preemptible(eenv->p, -1, cpu, 0))
			continue;

		/*
		 * Choose min util cpu within coregroup as candidates.
		 * Choosing a min util cpu is most likely to handle
		 * wake-up task without increasing the frequecncy.
		 */
		if (new_util < min_util) {
			min_util = new_util;
			min_util_cpu = cpu;
			idle_idx = idle_get_state_idx(cpu_rq(cpu));
		}
	}

	if (cpu_selected(min_util_cpu))
		state = idle_get_state(cpu_rq(min_util_cpu));

	cenv->cpu = min_util_cpu;
	cenv->util = !cpu_selected(min_util_cpu) ? min_util
		: normalized_util(min_util, capacity_orig_of(min_util_cpu));
	cenv->idle_idx = idle_idx;
	cenv->exit_latency = state ? state->exit_latency : UINT_MAX;
}

static int select_eco_cpu(struct eco_env *eenv)
{
	unsigned long best_util = ULONG_MAX;
	unsigned int best_energy = UINT_MAX;
	unsigned int prev_energy;
	int eco_cpu = eenv->prev_cpu;
	int cpu, best_cpu = -1;
	int best_idle_idx = INT_MAX;
	unsigned int best_exit_latency = UINT_MAX;

	/*
	 * It is meaningless to find an energy cpu when the energy table is
	 * not created or has not been created yet.
	 */
	if (!per_cpu(energy_table, eenv->prev_cpu).nr_states)
		return eenv->prev_cpu;

	for_each_cpu(cpu, cpu_active_mask) {
		struct cpumask mask;
		struct cpu_env cenv;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cpumask_and(&mask, cpu_coregroup_mask(cpu), tsk_cpus_allowed(eenv->p));

		if (cpumask_empty(&mask))
			continue;

		/*
		 * Select the best target, which is expected to consume the
		 * lowest energy among the min util cpu for each coregroup.
		 */
		find_min_util_cpu(&cenv, &mask, eenv);
		if (cpu_selected(cenv.cpu)) {
			unsigned int energy = calculate_energy(eenv->p, cenv.cpu);

			/* 1. find min_energy cpu */
			if (energy < best_energy)
				goto energy_cpu_found;
			if (energy > best_energy)
				continue;

			/* 2. find min_util cpu when energy is same */
			if (cenv.util < best_util)
				goto energy_cpu_found;
			if (cenv.util > best_util)
				continue;

			/* 3. find idle cpu when energy, util are same */
			if (best_idle_idx == -1 && cenv.idle_idx > -1)
				goto energy_cpu_found;

			/* 4. find shallower idle cpu when energy, util, idle-status are same */
			if ((best_idle_idx > -1) && (best_idle_idx > cenv.idle_idx))
				goto energy_cpu_found;
			else if (best_idle_idx < cenv.idle_idx)
				continue;

			/* 5. find cpu with least exit latency */
			if (cenv.exit_latency > best_exit_latency)
				continue;

energy_cpu_found:
			best_energy = energy;
			best_cpu = cenv.cpu;
			best_util = cenv.util;
			best_idle_idx = cenv.idle_idx;
			best_exit_latency = cenv.exit_latency;
		}
	}

	if (!cpu_selected(best_cpu) || best_cpu == eco_cpu)
		return eco_cpu;

	/*
	 * Compare prev cpu to best cpu to determine whether keeping the task
	 * on PREV CPU and sending the task to BEST CPU is beneficial for
	 * energy.
	 * An energy saving is considered meaningful if it reduces the energy
	 * consumption of PREV CPU candidate by at least ~1.56%.
	 */
	prev_energy = calculate_energy(eenv->p, eenv->prev_cpu);
	if (prev_energy - (prev_energy >> 6) > best_energy)
		eco_cpu = best_cpu;

	trace_ems_select_eco_cpu(eenv->p, eco_cpu, eenv->prev_cpu, best_cpu,
			prev_energy, best_energy);

	return eco_cpu;
}

int select_energy_cpu(struct eco_env *eenv, int sd_flag, int sync)
{
	struct sched_domain *sd = NULL;
	int cpu = smp_processor_id();

	if (!sched_feat(ENERGY_AWARE))
		return -1;

	/*
	 * Energy-aware wakeup placement on overutilized cpu is hard to get
	 * energy gain.
	 */
	rcu_read_lock();
	sd = rcu_dereference_sched(cpu_rq(eenv->prev_cpu)->sd);
	if (!sd || sd->shared->overutilized) {
		rcu_read_unlock();
		return -1;
	}
	rcu_read_unlock();

	/*
	 * We cannot do energy-aware wakeup placement sensibly for tasks
	 * with 0 utilization, so let them be placed according to the normal
	 * strategy.
	 */
	if (!task_util(eenv->p))
		return -1;

	if (sysctl_sched_sync_hint_enable && sync)
		if (cpumask_test_cpu(cpu, tsk_cpus_allowed(eenv->p)) &&
		    is_cpu_preemptible(eenv->p, eenv->prev_cpu, cpu, sync))
			return cpu;

	/*
	 * Find eco-friendly target.
	 * After selecting the best cpu according to strategy,
	 * we choose a cpu that is energy efficient compared to prev cpu.
	 */
	return select_eco_cpu(eenv);
}

#ifdef CONFIG_SIMPLIFIED_ENERGY_MODEL
static void
fill_power_table(struct energy_table *table, int table_size,
			unsigned long *f_table, unsigned int *v_table,
			int max_f, int min_f)
{
	int i, index = 0;
	int c = table->coefficient, v;
	unsigned long f, power;

	/* energy table and frequency table are inverted */
	for (i = table_size - 1; i >= 0; i--) {
		if (f_table[i] > max_f || f_table[i] < min_f)
			continue;

		f = f_table[i] / 1000;	/* KHz -> MHz */
		v = v_table[i] / 1000;	/* uV -> mV */

		/*
		 * power = coefficent * frequency * voltage^2
		 */
		power = c * f * v * v;

		/*
		 * Generally, frequency is more than treble figures in MHz and
		 * voltage is also more then treble figures in mV, so the
		 * calculated power is larger than 10^9. For convenience of
		 * calculation, divide the value by 10^9.
		 */
		do_div(power, 1000000000);
		table->states[index].power = power;

		/* save frequency to energy table */
		table->states[index].frequency = f_table[i];
		index++;
	}
}

static void
fill_cap_table(struct energy_table *table, int max_mips, unsigned long max_mips_freq)
{
	int i, m = table->mips;
	unsigned long f;

	for (i = 0; i < table->nr_states; i++) {
		f = table->states[i].frequency;

		/*
		 * capacity = freq/max_freq * mips/max_mips * 1024
		 */
		table->states[i].cap = f * m * 1024 / max_mips_freq / max_mips;
	}
}

static void show_energy_table(struct energy_table *table, int cpu)
{
	int i;

	pr_info("[Energy Table : cpu%d]\n", cpu);
	for (i = 0; i < table->nr_states; i++) {
		pr_info("[%d] .cap=%lu .power=%lu\n", i,
			table->states[i].cap, table->states[i].power);
	}
}

/*
 * Store the original capacity to update the cpu capacity according to the
 * max frequency of cpufreq.
 */
DEFINE_PER_CPU(unsigned long, cpu_orig_scale) = SCHED_CAPACITY_SCALE;

static int sched_cpufreq_policy_callback(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned long cpu_scale, max_scale;
	int cpu;

	if (event != CPUFREQ_NOTIFY)
		return NOTIFY_DONE;

	/*
	 * When policy->max is pressed, the performance of the cpu is constrained.
	 * In the constrained state, the cpu capacity also changes, and the
	 * overutil condition changes accordingly, so the cpu scale is updated
	 * whenever policy is changed.
	 */
	max_scale = (policy->max << SCHED_CAPACITY_SHIFT);
	max_scale /= policy->cpuinfo.max_freq;
	for_each_cpu(cpu, policy->related_cpus) {
		cpu_scale = per_cpu(cpu_orig_scale, cpu) * max_scale;
		cpu_scale = cpu_scale >> SCHED_CAPACITY_SHIFT;
		topology_set_cpu_scale(cpu, cpu_scale);
	}

	return NOTIFY_OK;
}

static struct notifier_block sched_cpufreq_policy_notifier = {
	.notifier_call = sched_cpufreq_policy_callback,
};

static void cpumask_speed_init(void)
{
	int cpu;
	unsigned long min_cap = ULONG_MAX, max_cap = 0;

	cpumask_clear(&slowest_mask);
	cpumask_clear(&fastest_mask);

	for_each_cpu(cpu, cpu_possible_mask) {
		unsigned long cap;

		cap = get_cpu_max_capacity(cpu);
		if (cap < min_cap)
			min_cap = cap;
		if (cap > max_cap)
			max_cap = cap;
	}

	for_each_cpu(cpu, cpu_possible_mask) {
		unsigned long cap;

		cap = get_cpu_max_capacity(cpu);
		if (cap == min_cap) {
			pr_info("cpu%d is_min_cap=%d\n", cpu, 1);
			cpumask_set_cpu(cpu, &slowest_mask);
		}
		if (cap == max_cap) {
			pr_info("cpu%d is_min_cap=%d\n", cpu, 0);
			cpumask_set_cpu(cpu, &fastest_mask);
		}
	}
}

/*
 * Whenever frequency domain is registered, and energy table corresponding to
 * the domain is created. Because cpu in the same frequency domain has the same
 * energy table. Capacity is calculated based on the max frequency of the fastest
 * cpu, so once the frequency domain of the faster cpu is regsitered, capacity
 * is recomputed.
 */
void init_sched_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f)
{
	struct energy_table *table;
	int cpu, i, mips, valid_table_size = 0;
	int max_mips = 0;
	unsigned long max_mips_freq = 0;
	int last_state;

	cpumask_and(cpus, cpus, cpu_possible_mask);
	if (cpumask_empty(cpus))
		return;

	mips = per_cpu(energy_table, cpumask_any(cpus)).mips;
	for_each_cpu(cpu, cpus) {
		/*
		 * All cpus in a frequency domain must have the smae capacity.
		 * Otherwise, it does not create an energy table because it
		 * is likely to be a human error.
		 */
		if (mips != per_cpu(energy_table, cpu).mips) {
			pr_warn("cpu%d has different cpacity!!\n", cpu);
			return;
		}
	}

	/* get size of valid frequency table to allocate energy table */
	for (i = 0; i < table_size; i++) {
		if (f_table[i] > max_f || f_table[i] < min_f)
			continue;

		valid_table_size++;
	}

	/* there is no valid row in the table, energy table is not created */
	if (!valid_table_size)
		return;

	/* allocate memory for energy table and fill power table */
	for_each_cpu(cpu, cpus) {
		table = &per_cpu(energy_table, cpu);
		table->states = kcalloc(valid_table_size,
					sizeof(struct energy_state), GFP_KERNEL);
		if (unlikely(!table->states))
			return;

		table->nr_states = valid_table_size;
		fill_power_table(table, table_size, f_table, v_table, max_f, min_f);
	}

	/*
	 * Find fastest cpu among the cpu to which the energy table is allocated.
	 * The mips and max frequency of fastest cpu are needed to calculate
	 * capacity.
	 */
	for_each_possible_cpu(cpu) {
		table = &per_cpu(energy_table, cpu);
		if (!table->states)
			continue;

		if (table->mips > max_mips) {
			max_mips = table->mips;

			last_state = table->nr_states - 1;
			max_mips_freq = table->states[last_state].frequency;
		}
	}

	/*
	 * Calculate and fill capacity table.
	 * Recalculate the capacity whenever frequency domain changes because
	 * the fastest cpu may have changed and the capacity needs to be
	 * recalculated.
	 */
	for_each_possible_cpu(cpu) {
		struct sched_domain *sd;

		table = &per_cpu(energy_table, cpu);
		if (!table->states)
			continue;

		fill_cap_table(table, max_mips, max_mips_freq);
		show_energy_table(table, cpu);

		last_state = table->nr_states - 1;
		per_cpu(cpu_orig_scale, cpu) = table->states[last_state].cap;
		topology_set_cpu_scale(cpu, table->states[last_state].cap);

		rcu_read_lock();
		for_each_domain(cpu, sd)
			update_group_capacity(sd, cpu);
		rcu_read_unlock();
	}

	topology_update();
	cpumask_speed_init();
}

static int __init init_sched_energy_data(void)
{
	struct device_node *cpu_node, *cpu_phandle;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct energy_table *table;

		cpu_node = of_get_cpu_node(cpu, NULL);
		if (!cpu_node) {
			pr_warn("CPU device node missing for CPU %d\n", cpu);
			return -ENODATA;
		}

		cpu_phandle = of_parse_phandle(cpu_node, "sched-energy-data", 0);
		if (!cpu_phandle) {
			pr_warn("CPU device node has no sched-energy-data\n");
			return -ENODATA;
		}

		table = &per_cpu(energy_table, cpu);
		if (of_property_read_u32(cpu_phandle, "capacity-mips", &table->mips)) {
			pr_warn("No capacity-mips data\n");
			return -ENODATA;
		}

		if (of_property_read_u32(cpu_phandle, "power-coefficient", &table->coefficient)) {
			pr_warn("No power-coefficient data\n");
			return -ENODATA;
		}

		of_node_put(cpu_phandle);
		of_node_put(cpu_node);

		pr_info("cpu%d mips=%d, coefficient=%d\n", cpu, table->mips, table->coefficient);
	}

	cpufreq_register_notifier(&sched_cpufreq_policy_notifier, CPUFREQ_POLICY_NOTIFIER);

	return 0;
}
core_initcall(init_sched_energy_data);
#endif	/* CONFIG_SIMPLIFIED_ENERGY_MODEL */
