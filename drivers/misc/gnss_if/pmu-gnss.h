/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __EXYNOS_PMU_GNSS_H
#define __EXYNOS_PMU_GNSS_H __FILE__

/* BLK_ALIVE: GNSS related SFRs */
#define EXYNOS_PMU_GNSS_CTRL_NS			0x0040
#define EXYNOS_PMU_GNSS_CTRL_S			0x0044
#define EXYNOS_PMU_GNSS_STAT			0x0048
#define EXYNOS_PMU_GNSS_DEBUG			0x004C
#define EXYNOS_PMU_GNSS2AP_MEM_CONFIG0		0x7250 /* GNSS_MEM_SIZE0 */
#define EXYNOS_PMU_GNSS2AP_MEM_CONFIG1		0x7254 /* GNSS_MEM_BA0 */
#define EXYNOS_PMU_GNSS2AP_MIF_ACCESS_WIN0	0X7260
#define EXYNOS_PMU_GNSS2AP_PERI_ACCESS_WIN0	0x7278
#define EXYNOS_PMU_GNSS2AP_PERI_ACCESS_WIN1	0x727C
#define EXYNOS_PMU_GNSS_BOOT_TEST_RST_CONFIG	0x7290

/* Ramen only? */
#define EXYNOS_PMU_SHARED_PWR_REQ_GNSS_CONTROL	0x800C
#define EXYNOS_PMU_SHARED_TCXO_REQ_GNSS_CONTROL	0x801C
#define EXYNOS_PMU_SHARED_MIF_REQ_GNSS_CONTROL	0x802C

#define EXYNOS_PMU_CENTRAL_SEQ_GNSS_CONFIGURATION	0x02C0
#define EXYNOS_PMU_RESET_AHEAD_GNSS_SYS_PWR_REG	0x1340
#define EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG	0x1344
#define EXYNOS_PMU_LOGIC_RESET_GNSS_SYS_PWR_REG	0x1348
#define EXYNOS_PMU_TCXO_GATE_GNSS_SYS_PWR_REG	0x134C
#define EXYNOS_PMU_GNSS_DISABLE_ISO_SYS_PWR_REG	0X1350
#define EXYNOS_PMU_GNSS_RESET_ISO_SYS_PWR_REG	0X1354

/* GNSS PMU */
/* For EXYNOS_PMU_GNSS_CTRL Register */
#define GNSS_PWRON		BIT(1)
#define GNSS_RESET_SET		BIT(2)
#define GNSS_START		BIT(3)
#define GNSS_ACTIVE_REQ_EN	BIT(5)
#define GNSS_ACTIVE_REQ_CLR	BIT(6)
#define GNSS_RESET_REQ_EN	BIT(7)
#define GNSS_RESET_REQ_CLR	BIT(8)
#define MASK_GNSS_PWRDN_DONE	BIT(9)
#define RTC_OUT_EN		BIT(10)
#define TCXO_ENABLE_SW		BIT(11)
#define MASK_SLEEP_START_REQ	BIT(12)
#define SET_SW_SLEEP_START_REQ	BIT(13)
#define GNSS_WAKEUP_REQ_EN	BIT(14)
#define GNSS_WAKEUP_REQ_CLR	BIT(15)
#define CLEANY_BYPASS_END	BIT(16)
#define SFR_SERIALIZER_DUR_DATA2REQ	GENMASK(21, 20)

#define MEMSIZE_MASK	(GENMASK(19, 0))
#define MEMSIZE_OFFSET	0
#define MEMSIZE_RES	(SZ_4K)

#define MEMBASE_ADDR_SHIFT	12
#define MEMBASE_ADDR_MASK	(GENMASK(23, 0))
#define MEMBASE_ADDR_OFFSET	0

/* Base address in the point of GNSS view */
#define MEMBASE_GNSS_ADDR	(0x60000000)
#define MEMBASE_GNSS_ADDR_2ND	(0xA0000000)

#ifndef CONFIG_ARCH_EXYNOS
/* Exynos PMU API functions are only available when ARCH_EXYNOS is defined.
 * Otherwise, we must hardcode the PMU address for setting the PMU registers.
 */
#define USE_IOREMAP_NOPMU
#endif

enum gnss_mode {
	GNSS_POWER_OFF,
	GNSS_POWER_ON,
	GNSS_RESET,
	NUM_GNSS_MODE,
};

enum gnss_int_clear {
	GNSS_INT_WDT_RESET_CLEAR,
	GNSS_INT_ACTIVE_CLEAR,
	GNSS_INT_WAKEUP_CLEAR,
};

enum gnss_tcxo_mode {
	TCXO_SHARED_MODE = 0,
	TCXO_NON_SHARED_MODE = 1,
};

struct gnss_ctl;

struct gnssctl_pmu_ops {
	int (*init_conf)(struct gnss_ctl *);
	int (*hold_reset)(void);
	int (*release_reset)(void);
	void (*check_status)(void);
	int (*power_on)(enum gnss_mode);
	int (*clear_int)(enum gnss_int_clear);
	int (*change_tcxo_mode)(enum gnss_tcxo_mode);
	int (*req_security)(void);
	void (*req_baaw)(void);
};

void gnss_get_pmu_ops(struct gnss_ctl *);
#endif /* __EXYNOS_PMU_GNSS_H */
