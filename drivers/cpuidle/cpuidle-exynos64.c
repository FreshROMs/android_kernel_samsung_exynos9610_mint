/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * CPUIDLE driver for exynos 64bit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/psci.h>
#include <linux/cpuidle_profiler.h>

#include <asm/tlbflush.h>
#include <asm/cpuidle.h>
#include <asm/topology.h>

#include <soc/samsung/exynos-cpupm.h>

#include "dt_idle_states.h"

/*
 * Exynos cpuidle driver supports the below idle states
 *
 * IDLE_C1 : WFI(Wait For Interrupt) low-power state
 * IDLE_C2 : Local CPU power gating
 */

/***************************************************************************
 *                           Cpuidle state handler                         *
 ***************************************************************************/
static unsigned int prepare_idle(unsigned int cpu, int index)
{
	unsigned int entry_state = 0;

	if (index > 0) {
		cpu_pm_enter();
		entry_state = exynos_cpu_pm_enter(cpu, index);
	}

	cpuidle_profile_cpu_idle_enter(cpu, index);

	return entry_state;
}

static void post_idle(unsigned int cpu, int index, int fail)
{
	cpuidle_profile_cpu_idle_exit(cpu, index, fail);

	if (!index)
		return;

	exynos_cpu_pm_exit(cpu, fail);
	cpu_pm_exit();
}

static int enter_idle(unsigned int index)
{
	/*
	 * idle state index 0 corresponds to wfi, should never be called
	 * from the cpu_suspend operations
	 */
	if (!index) {
		cpu_do_idle();
		return 0;
	}

	return arm_cpuidle_suspend(index);
}

static int exynos_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int index)
{
	int entry_state, ret = 0;

	entry_state = prepare_idle(dev->cpu, index);

	ret = enter_idle(entry_state);

	post_idle(dev->cpu, index, ret);

	/*
	 * If cpu fail to enter idle, it should not update state usage
	 * count. Driver have to return an error value to
	 * cpuidle_enter_state().
	 */
	if (ret < 0)
		return ret;
	else
		return index;
}

/***************************************************************************
 *                            Define notifier call                         *
 ***************************************************************************/
static int exynos_cpuidle_reboot_notifier(struct notifier_block *this,
				unsigned long event, void *_cmd)
{
	switch (event) {
	case SYSTEM_POWER_OFF:
	case SYS_RESTART:
		cpuidle_pause();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpuidle_reboot_nb = {
	.notifier_call = exynos_cpuidle_reboot_notifier,
};

/***************************************************************************
 *                         Initialize cpuidle driver                       *
 ***************************************************************************/
#define exynos_idle_wfi_state(state)					\
	do {								\
		state.enter = exynos_enter_idle;			\
		state.exit_latency = 1;					\
		state.target_residency = 1;				\
		state.power_usage = UINT_MAX;				\
		strncpy(state.name, "WFI", CPUIDLE_NAME_LEN - 1);	\
		strncpy(state.desc, "c1", CPUIDLE_DESC_LEN - 1);	\
	} while (0)

static struct cpuidle_driver exynos_idle_driver[NR_CPUS];

static const struct of_device_id exynos_idle_state_match[] __initconst = {
	{ .compatible = "exynos,idle-state",
	  .data = exynos_enter_idle },
	{ },
};

static int __init exynos_idle_driver_init(struct cpuidle_driver *drv,
					   struct cpumask *cpumask)
{
	int cpu = cpumask_first(cpumask);

	drv->name = kzalloc(sizeof("exynos_idleX"), GFP_KERNEL);
	if (!drv->name)
		return -ENOMEM;

	scnprintf((char *)drv->name, 12, "exynos_idle%d", cpu);
	drv->owner = THIS_MODULE;
	drv->cpumask = cpumask;
	exynos_idle_wfi_state(drv->states[0]);

	return 0;
}

static int __init exynos_idle_init(void)
{
	int ret, cpu, i;

	for_each_possible_cpu(cpu) {
		ret = exynos_idle_driver_init(&exynos_idle_driver[cpu],
					      topology_sibling_cpumask(cpu));

		if (ret) {
			pr_err("failed to initialize cpuidle driver for cpu%d",
					cpu);
			goto out_unregister;
		}

		/*
		 * Initialize idle states data, starting at index 1.
		 * This driver is DT only, if no DT idle states are detected
		 * (ret == 0) let the driver initialization fail accordingly
		 * since there is no reason to initialize the idle driver
		 * if only wfi is supported.
		 */
		ret = dt_init_idle_driver(&exynos_idle_driver[cpu],
					exynos_idle_state_match, 1);
		if (ret < 0) {
			pr_err("failed to initialize idle state for cpu%d\n", cpu);
			goto out_unregister;
		}

		/*
		 * Call arch CPU operations in order to initialize
		 * idle states suspend back-end specific data
		 */
		ret = arm_cpuidle_init(cpu);
		if (ret) {
			pr_err("failed to initialize idle operation for cpu%d\n", cpu);
			goto out_unregister;
		}

		ret = cpuidle_register(&exynos_idle_driver[cpu], NULL);
		if (ret) {
			pr_err("failed to register cpuidle for cpu%d\n", cpu);
			goto out_unregister;
		}
	}

	cpuidle_profile_cpu_idle_register(&exynos_idle_driver[0]);

	register_reboot_notifier(&exynos_cpuidle_reboot_nb);


	pr_info("Exynos cpuidle driver Initialized\n");

	return 0;

out_unregister:
	for (i = 0; i <= cpu; i++) {
		kfree(exynos_idle_driver[i].name);

		/*
		 * Cpuidle driver of variable "cpu" is always not registered.
		 * "cpu" should not call cpuidle_unregister().
		 */
		if (i < cpu)
			cpuidle_unregister(&exynos_idle_driver[i]);
	}

	return ret;
}
device_initcall(exynos_idle_init);
