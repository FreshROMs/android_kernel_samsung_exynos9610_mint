/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_9630_H
#define __MIF_REG_9630_H

/*********************************/
/* PLATFORM register definitions */
/*********************************/
#define NUM_MBOX_PLAT	4
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

#define WLBT_PBUS_BASE					0x14400000

// CBUS : APM_BUS
// PBUS : CFG_BUS

/* New WLBT SFRs for MEM config */
#define WLBT_PBUS_D_TZPC_SFR			(WLBT_PBUS_BASE + 0x10000)
#define WLBT_PBUS_BAAW_DBUS			(WLBT_PBUS_BASE + 0x20000)
#define WLBT_PBUS_BAAW_CBUS			(WLBT_PBUS_BASE + 0x30000)
#define WLBT_PBUS_SMAPPER			(WLBT_PBUS_BASE + 0x40000)
#define WLBT_PBUS_SYSREG			(WLBT_PBUS_BASE + 0x50000)
#define WLBT_PBUS_BOOT				(WLBT_PBUS_BASE + 0x60000)

#define TZPC_PROT0STAT		0x14410200
#define TZPC_PROT0SET		0x14410204

#define PMU_BOOT_RAM_START	(WLBT_PBUS_BOOT + 0x1000)
#define PMU_BOOT_RAM_END	(PMU_BOOT_RAM_START + 0xfff)

/* POWER */
/* Exynos 96300 UM - 46.5.1.59 */
#define VGPIO_TX_MONITOR	0x1700
#define VGPIO_TX_MON_BIT29	BIT(29)

/* Exynos 9630 UM - 9.8.719 */
#define WLBT_CONFIGURATION	0x3300
#define LOCAL_PWR_CFG		BIT(0) /* Control power state 0: Power down 1: Power on */

/* Exynos 9630 UM - 9.8.720 */
#define WLBT_STATUS		0x3304
#define WLBT_STATUS_BIT0	BIT(0) /* Status 0 : Power down 1 : Power on */

/* Exynos 9630 UM - 9.8.721 */
#define WLBT_STATES		0x3308 /* STATES [7:0] States index for debugging
					* 0x00 : Reset
					* 0x10 : Power up
					* 0x80 : Power down
					* */
#define WLBT_STATES_BIT7_0	BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)

/* Exynos 9630 UM - 9.8.722 */
#define WLBT_OPTION		0x330C
#define WLBT_OPTION_DATA	BIT(3)

/* Exynos 9630 UM - 9.8.723 */
#define WLBT_CTRL_NS		0x3310  /* WLBT Control SFR non-secure */
#define WLBT_ACTIVE_CLR		BIT(8)  /* WLBT_ACTIVE_REQ is clear internally on WAKEUP */
#define WLBT_ACTIVE_EN		BIT(7)  /* Enable of WIFI_ACTIVE_REQ */
#define SW_TCXO_REQ		BIT(6)  /* SW TCXO Request register, if MASK_TCXO_REQ
					 *  filed value is 1, This register value control TCXO Request*/
#define MASK_TCXO_REQ		BIT(5)  /* 1:mask TCXO_REQ coming from CP,
					 * 0:enable request source
					 */
#define TCXO_GATE		BIT(4)  /* TCXO gate control 0: TCXO enabled 1: TCXO gated */
#define RTC_OUT_EN		BIT(0)  /* RTC output enable 0:Disable 1:Enable */
/*#define SET_SW_MIF_REQ		BIT(13)*/ /* MIF SLEEP control by SW 1: if MASK_MIF_REQ is
					 * set to HIGH, MIF enters into down state by
					 * SET_SW_MIF_REQ.
					 */
/*#define MASK_MIF_REQ		BIT(12)*/ /* 1:mask MIF_REQ coming from WLBT, 0 : disable */
/*#define RTC_OUT_EN		BIT(10)*/ /* 1:enable, 0 : disable This is enable signal on RTC
					 * CLK(32KHz). This clock can be used as WLBT PMU
					 * clock when WLBT is internal power-down and
					 * TCXO(26MHz) is disable at WLBT side.
					 */


/*------------------------------------*/

//??????#define WLBT_PWRON              BIT(1)
#define WLBT_RESET_SET          BIT(0)  /* WLBT reset assertion control by using
					 * PMU_ALIVE_WLBT.
					 * 0x1: Reset Assertion,
					 * 0x0: Reset Release
					 */
#define WLBT_RESET_REQ_EN       BIT(7)  /* 1:enable, 0:disable  Enable of WLBT_RESET_REQ */
#define WLBT_RESET_REQ_CLR      BIT(8)  /* WLBT_RESET_REQ is clear internally on WAKEUP */
#define MASK_PWR_REQ            BIT(1) /* 1:mask PWR_REQ coming from WLBT, 0 : disable */
#define TCXO_ENABLE_SW		BIT(1) /* 1:enable, 0 : disable This is enable signal on TCXO
					 * clock of WLBT. This signal can decide whether TCXO
					 * clock is active by software when WLBT is internal
					 * power-down or WLBT is in reset state at WLBT side. if
					 * this value is HIGH, TCXO is active regardless of
					 * hardware control
					 */
/* from wlbt_if_S5E7920.h excite code */

/* PMU_ALIVE Bit Field */
/* WLBT_CTRL_NS */
//#define CLEANY_BYPASS_DATA_EN				BIT(16)
//#define SET_SW_MIF_REQ					BIT(13)
//#define MASK_MIF_REQ					BIT(12)
//#define RTC_OUT_EN					BIT(10)
//#define MASK_WLBT_PWRDN_DONE				BIT(9)
//#define WLBT_RESET_REQ_CLR				BIT(8)
//#define	WLBT_RESET_REQ_EN				BIT(7)
//#define	WLBT_ACTIVE_CLR					BIT(6)
//#define WLBT_ACTIVE_EN					BIT(5)
//#define WLBT_RESET_SET					BIT(0)
//#define WLBT_PWRON					BIT(1)

/* WLBT_CTRL_S */
//#define WLBT_START					BIT(0)

/* WLBT_STAT */
//#define WLBT_ACCESS_MIF					BIT(4)
//#define WLBT_PWRDN_DONE					BIT(0)

/* WLBT_DEBUG */
//#define MASK_CLKREQ_WLBT				BIT(8)
//#define EN_PWR_REQ					BIT(5)
//#define EN_WLBT_WAKEUP_REQ				BIT(4)
//#define EN_WLBT_RESET_REQ				BIT(3)
//#define EN_WLBT_ACTIVE					BIT(2)
//#define EN_MIF_REQ					BIT(0)

/* WLBT_BOOT_TEST_RST_CONFIG */
#define WLBT_IRAM_BOOT_OFFSET				(BIT(15) | BIT(14) | BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8))
#define WLBT_IRAM_BOOT_TEST				BIT(5)
#define WLBT_NOREMAP_BOOT_TEST				BIT(4)
#define WLBT2AP_PERI_PROT2				BIT(2)

/* WLBT_QOS */
#define WLBT_AWQOS					(BIT(19) | BIT(18) | BIT(17) | BIT(16))
#define WLBT_ARQOS					(BIT(11) | BIT(10) | BIT(9) | BIT(8))
#define WLBT_QOS_OVERRIDE				BIT(0)

/*------------------------------------*/

/* Exynos 9630 UM - 9.7.724 */
#define WLBT_CTRL_S		0x3314 /* WLBT Control SFR secure */
#define WLBT_START		BIT(3) /* CP control enable 0: Disable 1: Enable */

/* Exynos 9630 UM - 9.7.725 */
#define WLBT_OUT		0x3320
#define INISO_EN		BIT(19)
#define TCXO_ACK		BIT(18)
#define PWR_ACK			BIT(17)
#define INTREQ_ACTIVE		BIT(14)
#define SWEEPER_BYPASS		BIT(13) /* SWEEPER bypass mode control(WLBT2AP path) If
					 * this bit is set to 1, SWEEPER is bypass mode.
					 */

#define SWEEPER_PND_CLR_REQ	BIT(7)  /* SWEEPER_CLEAN Request. SWPPER is the IP
					 * that can clean up hung transaction in the Long hop
					 * async Bus Interface, when <SUBSYS> get hung
					 * state. 0: Normal 1: SWEEPER CLEAN Requested
					 */
/* Exynos 9630 UM - 9.7.726 */
#define WLBT_IN			0x3324
#define SHIFTING_DONE_OTP_BLK	BIT(5) /* OTP shifting controls feedback 0:None 1:Shifting done */
#define BUS_READY		BIT(4) /* BUS ready indication signal when reset released. 0:
					* Normal 1: BUS ready state */
#define PWRDOWN_IND		BIT(2) /* PWRDOWN state indication 0: Normal 1: In the
					* power down state */
#define SWEEPER_PND_CLR_ACK	BIT(0) /* SWEEPER_CLEAN ACK signal. SWPPER is the IP
					* that can clean up hung transaction in the Long hop
					* async Bus Interface, when <SUBSYS> get hung
					* state. 0: Normal 1: SWEEPER CLEAN
					* Acknowledged */

/* Exynos 9630 UM - 9.7.727 */
#define WLBT_INT_IN		0x3340

/* Exynos 9630 UM - 9.7.728 */
#define WLBT_INT_EN		0x3344

/* Exynos 9630 UM - 9.7.729 */
#define WLBT_INT_TYPE		0x3348

/* Exynos 9630 UM - 9.7.730 */
#define WLBT_INT_DIR		0x3348

/* the same bit from 0x3340 ~ 0x3348 */
#define LOCAL_PWR_CFG_R		BIT(0)
#define LOCAL_PWR_CFG_F		BIT(1)
#define PWR_REQ_R		BIT(2)
#define PWR_REQ_F		BIT(3)
#define TCXO_REQ_R		BIT(4)
#define TCXO_REQ_F		BIT(5)

/* Exynos 9630 UM - 9.7.1.10 */
#define WLBT_STAT		0x0058
#define WLBT_PWRDN_DONE		BIT(0) /* Check WLBT power-down status.*/
#define WLBT_ACCESS_MIF		BIT(4) /* Check whether WLBT accesses MIF domain */

/* Exynos 9630 UM - 9.7.1.11 */
#define WLBT_DEBUG		0x005C /* MIF sleep, wakeup debugging control */
/* need to find where have they moved */
//#define EN_MIF_REQ		BIT(0) /* Control MIF_REQ through GPIO_ALIVE. */
//#define EN_WLBT_ACTIVE	BIT(2) /* Control WLBT_ACTIVE through GPIO_ALIVE. */
//#define EN_WLBT_RESET_REQ	BIT(3) /* Control WLBT_RESET_REQ through GPIO_ALIVE. */
#define MASK_CLKREQ_WLBT	BIT(8) /* When this field is set to HIGH, ALIVE ignores
					* CLKREQ from WLBT.
					*/

#define RESET_SEQUENCER_STATUS	0x0504
#define RESET_STATUS_MASK	(BIT(10)|BIT(9)|BIT(8))
#define RESET_STATUS		(5 << 8)

#define PMU_SHARED_PWR_REQ_WLBT_CONTROL_STATUS 0x8008
#define CTRL_STATUS_MASK 0x1

#define CLEANY_BUS_WLBT_CONFIGURATION 0x3b20
#define CLEANY_CFG_MASK 0x1

#define CLEANY_BUS_WLBT_STATUS	0x3B24
#define CLEANY_STATUS_MASK	(BIT(17)|BIT(16))

/* Exynos9630 UM_REV0.31 - 9.7.1.748 */
#define WAKEUP_INT_TYPE 0x3948
#define RESETREQ_WLBT   BIT(18) /* Interrupt type 0:Edge, 1:Level */

/* Exynos 9630 UM - 9.8.763 */
#define SYSTEM_OUT		0x3A20
#define PWRRGTON_CON		BIT(9) /* XPWRRTON_CON control 0: Disable 1: Enable */

/* Exynos 9630 UM - 9.8.812 */
#define TCXO_BUF_CTRL		0x3C10
#define TCXO_BUF_BIAS_EN_WLBT	BIT(2)
#define TCXO_BUF_EN_WLBT	BIT(3)

/* New WLBT SFRs for MEM config */

/* end address is exclusive so the ENDx register should be set to the first
 * address that is not accessible through that BAAW.
 *
 * Another very important point to note here is we are using BAAW0 to expose
 * 16MB region, so other BAAWs can be used for other purposes
 */
#define WLBT_DBUS_BAAW_0_START          0x80000000 // Start of DRAM for WLBT R7
#define WLBT_DBUS_BAAW_0_END            WLBT_DBUS_BAAW_4_START // 16 MB
#define WLBT_DBUS_BAAW_1_START          0x80400000
#define WLBT_DBUS_BAAW_1_END            WLBT_DBUS_BAAW_2_START
#define WLBT_DBUS_BAAW_2_START          0x80800000
#define WLBT_DBUS_BAAW_2_END            WLBT_DBUS_BAAW_3_START
#define WLBT_DBUS_BAAW_3_START          0x80C00000
#define WLBT_DBUS_BAAW_3_END            WLBT_DBUS_BAAW_4_START
#define WLBT_DBUS_BAAW_4_START          0x81000000
#define WLBT_DBUS_BAAW_4_END            0x813FFFFF

#define WLBT_BAAW_CON_INIT_DONE		(1 << 31)
#define WLBT_BAAW_CON_EN_WRITE		(1 << 1)
#define WLBT_BAAW_CON_EN_READ		(1 << 0)
#define WLBT_BAAW_ACCESS_CTRL		(WLBT_BAAW_CON_INIT_DONE | WLBT_BAAW_CON_EN_WRITE | WLBT_BAAW_CON_EN_READ)

/* ref Confluence Maxwell450+Memory+Map */
#define WLBT_CBUS_BAAW_0_START          0xA0000000		// CP2WLBT MBOX
#define WLBT_CBUS_BAAW_0_END            0xA000FFFF

#define WLBT_CBUS_BAAW_1_START          0xA0010000		// GNSS,APM,AP,ABOX,CHUB2WLBT MBOX
#define WLBT_CBUS_BAAW_1_END            0xA005FFFF

#define WLBT_CBUS_BAAW_2_START          0xA0060000		// CMGP SFR GPIO_CMGP_BASE
#define WLBT_CBUS_BAAW_2_END            0xA009FFFF

#define WLBT_CBUS_BAAW_3_START          0xA00A0000		// CMGP SFR SYSREG_CMGP2WLBT_BASE
#define WLBT_CBUS_BAAW_3_END            0xA00CFFFF

#define WLBT_CBUS_BAAW_4_START          0xA00D0000		// CMGP SFR USI_CMG00_BASE
#define WLBT_CBUS_BAAW_4_END            0xA015FFFF

#define WLBT_CBUS_BAAW_5_START          0xA0160000		// CHUB SFR CHUB_USICHUB0_BASE
#define WLBT_CBUS_BAAW_5_END            0xA01BFFFF

#define WLBT_CBUS_BAAW_6_START          0xA01C0000		// CHUB SFR CHUB_BASE
#define WLBT_CBUS_BAAW_6_END            0xA01EFFFF

#define WLBT_PBUS_MBOX_CP2WLBT_BASE     0x10F50000
#define WLBT_PBUS_MBOX_GNSS2WLBT_BASE	0x10FA0000
#define WLBT_PBUS_MBOX_SHUB2WLBT_BASE   0x119A0000
#define WLBT_PBUS_USI_CMG00_BASE        0x11500000
#define WLBT_PBUS_SYSREG_CMGP2WLBT_BASE 0x11490000
#define WLBT_PBUS_GPIO_CMGP_BASE        0x11430000
#define WLBT_PBUS_CHUB_USICHUB0_BASE	0x11B70000
#define WLBT_PBUS_CHUB_BASE             0x11A00000



/* CHIP_VERSION_ID SFR (remap block) 0x14450410
 */
#define CHIP_VERSION_ID_OFFSET          0x410
#define CHIP_VERSION_ID_VER_MASK        0xFFFFFFFF      /* [00:31] Version ID */
#define CHIP_VERSION_ID_IP_PMU          0x0000F000      /* [12:15] PMU ROM Rev */
#define CHIP_VERSION_ID_IP_MINOR        0x000F0000      /* [16:19] Minor Rev */
#define CHIP_VERSION_ID_IP_MAJOR        0x00F00000      /* [20:23] Major Rev */
#define CHIP_VERSION_ID_IP_PMU_SHIFT    12
#define CHIP_VERSION_ID_IP_MINOR_SHIFT  16
#define CHIP_VERSION_ID_IP_MAJOR_SHIFT  20

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

//#define CENTRAL_SEQ_WLBT_STATUS 0x0184        /* 23:16  Check statemachine status */
//#define STATES                  0xff0000

#define SYS_PWR_CFG             BIT(0)
#define SYS_PWR_CFG_2           (BIT(0) | BIT(1))
#define SYS_PWR_CFG_16          BIT(16)

/* TZASC (TrustZone Address Space Controller) configuration for Katmai onwards */
#define EXYNOS_SET_CONN_TZPC    0
#define SMC_CMD_CONN_IF         (0x82000710)
#endif /* __MIF_REG_9630_H */
