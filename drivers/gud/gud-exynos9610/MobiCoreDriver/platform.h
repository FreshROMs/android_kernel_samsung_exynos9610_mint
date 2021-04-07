/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Header file of MobiCore Driver Kernel Module Platform
 * specific structures
 *
 * Internal structures of the McDrvModule
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

#define IRQ_SPI(x)      (x + 32)

/* MobiCore Interrupt. */
#if defined(CONFIG_SOC_EXYNOS3250) || defined(CONFIG_SOC_EXYNOS3472)
#define MC_INTR_SSIQ	254
#elif defined(CONFIG_SOC_EXYNOS3475) || defined(CONFIG_SOC_EXYNOS5430) || \
	defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7870) || \
	defined(CONFIG_SOC_EXYNOS8890) || defined(CONFIG_SOC_EXYNOS7880) || defined(CONFIG_SOC_EXYNOS8895)
#define MC_INTR_SSIQ	255
#elif defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7580)
#define MC_INTR_SSIQ	246
#endif

/* Enable Runtime Power Management */
#if defined(CONFIG_SOC_EXYNOS3472)
#ifdef CONFIG_PM_RUNTIME
#define MC_PM_RUNTIME
#endif
#endif /* CONFIG_SOC_EXYNOS3472 */

#if !defined(CONFIG_SOC_EXYNOS3472)

#define TBASE_CORE_SWITCHER

#if defined(CONFIG_SOC_EXYNOS3250)
#define COUNT_OF_CPUS 2
#elif defined(CONFIG_SOC_EXYNOS3475)
#define COUNT_OF_CPUS 4
#else
#define COUNT_OF_CPUS 8
#endif

/* Values of MPIDR regs */
#if defined(CONFIG_SOC_EXYNOS3250) || defined(CONFIG_SOC_EXYNOS3475)
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003}
#elif defined(CONFIG_SOC_EXYNOS7580) || defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880)
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003, 0x0100, 0x0101, 0x0102, 0x0103}
#elif defined(CONFIG_SOC_EXYNOS9810)
/* On Cortex A55, bit 24 is used to differentiate
 * between different MPIDR format. So the whole MPIDR
 * must be transmited
 */
#define CPU_IDS {0x81000000, 0x81000100, 0x81000200, 0x81000300, 0x80000100,\
		0x80000101, 0x80000102, 0x80000103}
#elif defined(CONFIG_SOC_EXYNOS7885)
#define CPU_IDS {0x0100, 0x0101, 0x0102, 0x0103, 0x0200, 0x0201, 0x0000, 0x0001}
#elif defined(CONFIG_SOC_EXYNOS9610)
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003, 0x0100, 0x0101, 0x0102, 0x0103}
#elif defined(CONFIG_SOC_EXYNOS9820)
#define CPU_IDS {0x81000000, 0x81000100, 0x81000200, 0x81000300, \
		0x81000400, 0x81000500, 0x80000100, 0x80000101}
#else
#define CPU_IDS {0x0100, 0x0101, 0x0102, 0x0103, 0x0000, 0x0001, 0x0002, 0x0003}
#endif
#endif /* !CONFIG_SOC_EXYNOS3472 */

/* Force usage of xenbus_map_ring_valloc as of Linux v4.1 */
#define MC_XENBUS_MAP_RING_VALLOC_4_1

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

/* Set Parameters for Secure OS Boosting */
#define DEFAULT_LITTLE_CORE		0
#define NONBOOT_LITTLE_CORE		1
#define DEFAULT_BIG_CORE		4
#define MIGRATE_TARGET_CORE     DEFAULT_BIG_CORE

#define MC_INTR_LOCAL_TIMER            (IRQ_SPI(238) + DEFAULT_BIG_CORE)

#define LOCAL_TIMER_PERIOD             50

#define DEFAULT_SECOS_BOOST_TIME       5000
#define MAX_SECOS_BOOST_TIME           600000  /* 600 sec */

#define DUMP_TBASE_HALT_STATUS

#endif /* _MC_DRV_PLATFORM_H_ */
