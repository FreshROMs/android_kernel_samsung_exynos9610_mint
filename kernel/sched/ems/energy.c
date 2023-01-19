/*
 * Energy efficient cpu selection
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpufreq.h>
#include <trace/events/ems.h>

#include "ems.h"
#include "../sched.h"

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

/*
 * When choosing cpu considering energy efficiency, decide best cpu and
 * backup cpu according to policy, and then choose cpu which consumes the
 * least energy including prev cpu.
 */
struct eco_env {
	struct task_struct *p;
	int prev_cpu;
};

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
		util[cpu] = cpu_util_wake(cpu, p);

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
			util_sum += (util[i] << SCHED_CAPACITY_SHIFT) / capacity;
		}

		/*
		 * 4. compute active energy
		 */
		total_energy += util_sum * table->states[cap_idx].power;
	}

	return total_energy;
}

static int find_min_util_cpu(struct cpumask *mask, unsigned long task_util)
{
	unsigned long min_util = ULONG_MAX;
	int min_util_cpu = -1;
	int cpu;

	/* Find energy efficient cpu in each coregroup. */
	for_each_cpu_and(cpu, mask, cpu_active_mask) {
		unsigned long capacity_orig = capacity_orig_of(cpu);
		unsigned long util = cpu_util(cpu);

		/* Skip over-capacity cpu */
		if (util + task_util > capacity_orig)
			continue;

		/*
		 * Choose min util cpu within coregroup as candidates.
		 * Choosing a min util cpu is most likely to handle
		 * wake-up task without increasing the frequecncy.
		 */
		if (util < min_util) {
			min_util = util;
			min_util_cpu = cpu;
		}
	}

	return min_util_cpu;
}

static int select_eco_cpu(struct eco_env *eenv)
{
	unsigned long task_util = task_util_est(eenv->p);
	unsigned int best_energy = UINT_MAX;
	unsigned int prev_energy;
	int eco_cpu = eenv->prev_cpu;
	int cpu, best_cpu = -1;

	/*
	 * It is meaningless to find an energy cpu when the energy table is
	 * not created or has not been created yet.
	 */
	if (!per_cpu(energy_table, eenv->prev_cpu).nr_states)
		return eenv->prev_cpu;

	for_each_cpu(cpu, cpu_active_mask) {
		struct cpumask mask;
		int energy_cpu;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cpumask_and(&mask, cpu_coregroup_mask(cpu), tsk_cpus_allowed(eenv->p));
		/*
		 * Checking prev cpu is meaningless, because the energy of prev cpu
		 * will be compared to best cpu at last
		 */
		cpumask_clear_cpu(eenv->prev_cpu, &mask);
		if (cpumask_empty(&mask))
			continue;

		/*
		 * Select the best target, which is expected to consume the
		 * lowest energy among the min util cpu for each coregroup.
		 */
		energy_cpu = find_min_util_cpu(&mask, task_util);
		if (cpu_selected(energy_cpu)) {
			unsigned int energy = calculate_energy(eenv->p, energy_cpu);

			if (energy < best_energy) {
				best_energy = energy;
				best_cpu = energy_cpu;
			}
		}
	}

	if (!cpu_selected(best_cpu))
		return -1;

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

int select_energy_cpu(struct task_struct *p, int prev_cpu, int sd_flag, int sync)
{
	struct sched_domain *sd = NULL;
	int cpu = smp_processor_id();
	struct eco_env eenv = {
		.p = p,
		.prev_cpu = prev_cpu,
	};

	if (!sched_feat(ENERGY_AWARE))
		return -1;

	/*
	 * Energy-aware wakeup placement on overutilized cpu is hard to get
	 * energy gain.
	 */
	rcu_read_lock();
	sd = rcu_dereference_sched(cpu_rq(prev_cpu)->sd);
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
	if (!task_util(p))
		return -1;

	if (sysctl_sched_sync_hint_enable && sync)
		if (cpumask_test_cpu(cpu, &p->cpus_allowed))
			return cpu;

	/*
	 * Find eco-friendly target.
	 * After selecting the best cpu according to strategy,
	 * we choose a cpu that is energy efficient compared to prev cpu.
	 */
	return select_eco_cpu(&eenv);
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
