/*
 * Copyright (c) 2018 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_CPUPM_H
#define __EXYNOS_CPUPM_H __FILE__

extern int exynos_cpu_pm_enter(int cpu, int index);
extern void exynos_cpu_pm_exit(int cpu, int cancel);

enum {
	POWERMODE_TYPE_CLUSTER = 0,
	POWERMODE_TYPE_SYSTEM,
};

extern void disable_power_mode(int cpu, int type);
extern void enable_power_mode(int cpu, int type);
extern bool exynos_cpuhp_last_cpu(unsigned int cpu);

#ifdef CONFIG_CPU_IDLE
void exynos_update_ip_idle_status(int index, int idle);
int exynos_get_idle_ip_index(const char *name);
extern void disable_power_mode(int cpu, int type);
extern void enable_power_mode(int cpu, int type);
#else
static inline void exynos_update_ip_idle_status(int index, int idle)
{
	return;
}

static inline int exynos_get_idle_ip_index(const char *name)
{
	return 0;
}

static inline void disable_power_mode(int cpu, int type)
{
	return;
}

static inline void enable_power_mode(int cpu, int type)
{
	return;
}
#endif

#endif /* __EXYNOS_CPUPM_H */
