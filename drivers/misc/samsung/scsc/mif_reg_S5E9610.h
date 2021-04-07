/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_9610_H
#define __MIF_REG_9610_H

/*********************************/
/* PLATFORM register definitions */
/*********************************/
#define NUM_MBOX_PLAT	8
#define NUM_SEMAPHORE   12

#define MAILBOX_WLBT_BASE       0x0000
#define MAILBOX_WLBT_REG(r)     (MAILBOX_WLBT_BASE + (r))
#define MCUCTRL                 0x000 /* MCU Controller Register */
/* R0 [31:16]  - Int FROM R4/M4 */
#define INTGR0                  0x008 /* Interrupt Generation Register 0 (r/w) */
#define INTCR0                  0x00C /* Interrupt Clear Register 0 (w) */
#define INTMR0                  0x010 /* Interrupt Mask Register 0 (r/w) */
#define INTSR0                  0x014 /* Interrupt Status Register 0 (r) */
#define INTMSR0                 0x018 /* Interrupt Mask Status Register 0 (r) */
/* R1 [15:0]  - Int TO R4/M4 */
#define INTGR1                  0x01c /* Interrupt Generation Register 1 */
#define INTCR1                  0x020 /* Interrupt Clear Register 1 */
#define INTMR1                  0x024 /* Interrupt Mask Register 1 */
#define INTSR1                  0x028 /* Interrupt Status Register 1 */
#define INTMSR1                 0x02c /* Interrupt Mask Status Register 1 */
#define MIF_INIT                0x04c /* MIF_init */
#define IS_VERSION              0x050 /* Version Information Register */
#define ISSR_BASE               0x080 /* IS_Shared_Register Base address */
#define ISSR(r)                 (ISSR_BASE + (4 * (r)))
#define SEMAPHORE_BASE          0x180 /* IS_Shared_Register Base address */
#define SEMAPHORE(r)            (SEMAPHORE_BASE + (4 * (r)))
#define SEMA0CON                0x1c0
#define SEMA0STATE              0x1c8
#define SEMA1CON                0x1e0
#define SEMA1STATE              0x1e8

/* POWER */
/* Exynos 9610 UM - 9.9.1.16 */
#define WLBT_CTRL_NS            0x0050 /* WLBT Control SFR non-secure */

#define WLBT_PWRON              BIT(1)
#define WLBT_RESET_SET          BIT(0)  /* WLBT reset assertion control by using
					 * PMU_ALIVE_WLBT.
					 * 0x1: Reset Assertion,
					 * 0x0: Reset Release
					 */
#define WLBT_ACTIVE_EN          BIT(5)  /* Enable of WIFI_ACTIVE_REQ */
#define WLBT_ACTIVE_CLR         BIT(6)  /* WLBT_ACTIVE_REQ is clear internally on WAKEUP */
#define WLBT_RESET_REQ_EN       BIT(7)  /* 1:enable, 0:disable  Enable of WLBT_RESET_REQ */
#define WLBT_RESET_REQ_CLR      BIT(8)  /* WLBT_RESET_REQ is clear internally on WAKEUP */
#define MASK_PWR_REQ            BIT(18) /* 1:mask PWR_REQ coming from WLBT, 0 : disable */
#define MASK_TCXO_REQ           BIT(20) /* 1:mask TCXO_REQ coming from CP,
					 * 0:enable request source
					 */

#define RTC_OUT_EN		BIT(10) /* 1:enable, 0 : disable This is enable signal on RTC
					 * CLK(32KHz). This clock can be used as WLBT PMU
					 * clock when WLBT is internal power-down and
					 * TCXO(26MHz) is disable at WLBT side.
					 */
#define TCXO_ENABLE_SW		BIT(11) /* 1:enable, 0 : disable This is enable signal on TCXO
					 * clock of WLBT. This signal can decide whether TCXO
					 * clock is active by software when WLBT is internal
					 * power-down or WLBT is in reset state at WLBT side. if
					 * this value is HIGH, TCXO is active regardless of
					 * hardware control
					 */
#define MASK_MIF_REQ		BIT(12) /* 1:mask MIF_REQ coming from WLBT, 0 : disable */
#define SET_SW_MIF_REQ		BIT(13) /* MIF SLEEP control by SW 1: if MASK_MIF_REQ is
					 * set to HIGH, MIF enters into down state by
					 * SET_SW_MIF_REQ.
					 */
#define SWEEPER_BYPASS_DATA_EN	BIT(16) /* SWEEPER bypass mode control(WLBT2AP path) If
					 * this bit is set to 1, SWEEPER is bypass mode.
					 */

#define WLBT_CTRL_S             0x0054 /* WLBT Control SFR secure */
#define WLBT_START              BIT(0) /* WLBT initial Reset release control
					* If CP_START = 1, PMU_RESET_SEQUENCER_CP
					* starts initial reset release sequence
					* and goes to UP state.
					*/

#define WLBT_STAT               0x0058
#define WLBT_PWRDN_DONE	BIT(0) /* Check WLBT power-down status.*/
#define WLBT_ACCESS_MIF	BIT(4) /* Check whether WLBT accesses MIF doman */

#define WLBT_DEBUG			0x005c /* MIF sleep, wakeup debugging control */
#define EN_MIF_REQ			BIT(0) /* Control MIF_REQ through GPIO_ALIVE. */
#define EN_WLBT_ACTIVE		BIT(2) /* Control WLBT_ACTIVE through GPIO_ALIVE. */
#define EN_WLBT_RESET_REQ	BIT(3) /* Control WLBT_RESET_REQ through GPIO_ALIVE. */
#define MASK_CLKREQ_WLBT	BIT(8) /* When this field is set to HIGH, ALIVE ignores
					* CLKREQ from WLBT.
					*/

/* New WLBT SFRs for MEM config */

/* end address is exclusive so the ENDx register should be set to the first
 * address that is not accessible through that BAAW.
 *
 * Another very important point to note here is we are using BAAW0 to expose
 * 16MB region, so other BAAWs can be used for other purposes
 */
#define WLBT_DBUS_BAAW_0_START          0x80000000
#define WLBT_DBUS_BAAW_0_END            WLBT_DBUS_BAAW_4_START
#define WLBT_DBUS_BAAW_1_START          0x80400000
#define WLBT_DBUS_BAAW_1_END            WLBT_DBUS_BAAW_2_START
#define WLBT_DBUS_BAAW_2_START          0x80800000
#define WLBT_DBUS_BAAW_2_END            WLBT_DBUS_BAAW_3_START
#define WLBT_DBUS_BAAW_3_START          0x80C00000
#define WLBT_DBUS_BAAW_3_END            WLBT_DBUS_BAAW_4_START
#define WLBT_DBUS_BAAW_4_START          0x81000000
#define WLBT_DBUS_BAAW_4_END            0x81400000

#define WLBT_BAAW_CON_INIT_DONE		(1 << 31)
#define WLBT_BAAW_CON_EN_WRITE		(1 << 1)
#define WLBT_BAAW_CON_EN_READ		(1 << 0)
#define WLBT_BAAW_ACCESS_CTRL		(WLBT_BAAW_CON_INIT_DONE | WLBT_BAAW_CON_EN_WRITE | WLBT_BAAW_CON_EN_READ)

#define WLBT_PBUS_BAAW_0_START          0xA0000000
#define WLBT_PBUS_BAAW_0_END            WLBT_PBUS_BAAW_1_START
#define WLBT_PBUS_BAAW_1_START          0xA0010000
#define WLBT_PBUS_BAAW_1_END            WLBT_PBUS_BAAW_2_START
#define WLBT_PBUS_BAAW_2_START          0xA0060000
#define WLBT_PBUS_BAAW_2_END            WLBT_PBUS_BAAW_3_START
#define WLBT_PBUS_BAAW_3_START          0xA0100000
#define WLBT_PBUS_BAAW_3_END            WLBT_PBUS_BAAW_4_START
#define WLBT_PBUS_BAAW_4_START          0xA0110000
#define WLBT_PBUS_BAAW_4_END            WLBT_PBUS_BAAW_5_START
#define WLBT_PBUS_BAAW_5_START          0xA0120000
#define WLBT_PBUS_BAAW_5_END            0xA0160000

#define WLBT_PBUS_MBOX_CP2WLBT_BASE     0x11950000//0xA0000000
#define WLBT_PBUS_MBOX_SHUB2WLBT_BASE   0x119A0000//0xA0010000
#define WLBT_PBUS_USI_CMG00_BASE        0x11D00000//0xA0060000
#define WLBT_PBUS_SYSREG_CMGP2WLBT_BASE 0x11C80000//0xA0100000
#define WLBT_PBUS_GPIO_CMGP_BASE        0x11C20000//0xA0110000
#define WLBT_PBUS_SHUB_BASE             0x11200000//0xA0120000

/* EMA settings overloaded onto CHIP_VERSION_ID SFR
 * (remap block)
 */
#define CHIP_VERSION_ID_VER_MASK	0xffc00000	/* [22:32] Version ID */
#define CHIP_VERSION_ID_EMA_MASK	0x003fffff	/*  [0:21] EMA params */
#define CHIP_VERSION_ID_EMA_VALUE       (BIT(20) | \
					 BIT(18) | \
					 BIT(13) | \
					 BIT(11) | \
					 BIT(5)  | \
					 BIT(2) )

/* Power down registers */
#define RESET_AHEAD_WLBT_SYS_PWR_REG 0x1360	/* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define CLEANY_BUS_WLBT_SYS_PWR_REG  0x1364	/* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define LOGIC_RESET_WLBT_SYS_PWR_REG 0x1368	/* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define TCXO_GATE_WLBT_SYS_PWR_REG   0x136C	/* Control power state in LOWPWR mode 1 - on, 0 */
#define WLBT_DISABLE_ISO_SYS_PWR_REG 0x1370	/* Control power state in LOWPWR mode 1 - on, 0 */
#define WLBT_RESET_ISO_SYS_PWR_REG   0x1374	/* Control power state in LOWPWR mode 1 - on, 0 */

#define CENTRAL_SEQ_WLBT_CONFIGURATION 0x0180  /* bit 16. Decides whether system-level low-power mode
						* is used HIGH: System-level Low-Power mode
						* disabled. LOW: System-level Low-Power mode
						* enabled. When system enters low-power mode,
						* this field is automatically cleared to HIGH.
						*/

#define CENTRAL_SEQ_WLBT_STATUS 0x0184        /* 23:16  Check statemachine status */
#define STATES                  0xff0000

#define SYS_PWR_CFG             BIT(0)
#define SYS_PWR_CFG_2           (BIT(0) | BIT(1))
#define SYS_PWR_CFG_16          BIT(16)

/* TZASC (TrustZone Address Space Controller) configuration for Katmai onwards */
#define EXYNOS_SET_CONN_TZPC    0
//#define SMC_CMD_CONN_IF         0x82000710
#endif /* __MIF_REG_9610_H */
