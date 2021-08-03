/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_3830_H
#define __MIF_REG_3830_H

/****************************************************************************
 * This header uses values from Nacho Exynos 3830 User Manual
 * A copy can be found in http://cognidox/SC-508880-SP
 ****************************************************************************/


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

#define WLBT_PBUS_BASE			0x14C00000

// CBUS : APM_BUS
// PBUS : CFG_BUS

/* New WLBT SFRs for MEM config */
#define WLBT_PBUS_D_TZPC_SFR		(WLBT_PBUS_BASE + 0x10000)
#define WLBT_PBUS_BAAW_DBUS		(WLBT_PBUS_BASE + 0x20000)
#define WLBT_PBUS_BAAW_CBUS		(WLBT_PBUS_BASE + 0x30000)
#define WLBT_PBUS_SMAPPER		(WLBT_PBUS_BASE + 0x40000)
#define WLBT_PBUS_SYSREG		(WLBT_PBUS_BASE + 0x50000)
#define WLBT_PBUS_BOOT			(WLBT_PBUS_BASE + 0x60000)

#define VGPIO_TX_MONITOR	0x1700
#define VGPIO_TX_MON_BIT29	BIT(29)

/* Exynos 3830 UM - TODO */
#define WLBT_CONFIGURATION	0x3100
#define LOCAL_PWR_CFG		BIT(0) /* Control power state 0: Power down 1: Power on */

/* Exynos 3830 UM - TODO */
#define WLBT_STATUS		0x3104
#define WLBT_STATUS_BIT0	BIT(0) /* Status 0 : Power down 1 : Power on */

/* Exynos 3830 UM - TODO */
#define WLBT_STATES		0x3108 /* STATES [7:0] States index for debugging
					* 0x00 : Reset
					* 0x10 : Power up
					* 0x80 : Power down
					* */
#define WLBT_OPTION		0x310C
#define WLBT_OPTION_DATA	BIT(3)

/* Exynos 3830 UM - TODO */
#define WLBT_CTRL_NS            0x3110 /* WLBT Control SFR non-secure */
#define WLBT_ACTIVE_CLR         BIT(6)  /* WLBT_ACTIVE_REQ is clear internally on WAKEUP */
#define WLBT_ACTIVE_EN          BIT(5)  /* Enable of WIFI_ACTIVE_REQ */

/* Exynos 3830 UM - TODO */
#define WLBT_CTRL_S             0x3114 /* WLBT Control SFR secure */
#define WLBT_START              BIT(3) /* CP control enable 0: Disable 1: Enable */

/* Exynos 3830 UM - TODO */
#define WLBT_OUT 0x3120
#define SWEEPER_BYPASS		BIT(13) /* SWEEPER bypass mode control(WLBT2AP path) If
					 * this bit is set to 1, SWEEPER is bypass mode.
					 */
#define SWEEPER_PND_CLR_REQ	BIT(7) /* SWEEPER_CLEAN Request. SWPPER is the IP
					* that can clean up hung transaction in the Long hop
					* async Bus Interface, when <SUBSYS> get hung
					* state. 0: Normal 1: SWEEPER CLEAN Requested
					*/

/* Exynos 3830 UM - Exynos3830_12_PMU.docx 1.10.566 */
#define WLBT_IN			0x3124
/* TODO: nacho does not have BUS_READY */
#define BUS_READY		BIT(4) /* BUS ready indication signal when reset released. 0:
					* Normal 1: BUS ready state */
#define PWRDOWN_IND		BIT(2) /* PWRDOWN state indication 0: Normal 1: In the
					* power down state */
#define SWEEPER_PND_CLR_ACK	BIT(0) /* SWEEPER_CLEAN ACK signal. SWPPER is the IP
					* that can clean up hung transaction in the Long hop
					* async Bus Interface, when <SUBSYS> get hung
					* state. 0: Normal 1: SWEEPER CLEAN
					* Acknowledged */
/* Exynos 3830 UM - Exynos3830_12_PMU.docx Page 38 */
#define WLBT_INT_EN		0x3144
#define PWR_REQ_F		BIT(3)
#define TCXO_REQ_F		BIT(5)

/* Exynos 3830 UM - TODO */
#define WLBT_STAT		0x0058
#define WLBT_PWRDN_DONE		BIT(0) /* Check WLBT power-down status.*/
#define WLBT_ACCESS_MIF		BIT(4) /* Check whether WLBT accesses MIF domain */

/* Exynos 3830 UM - TODO */
#define WLBT_DEBUG		0x005C

/* Exynos 3830 UM - TODO */
#define MIF_CTRL		0x3810
#define TCXO_EN			BIT(0) /* XCLKREQ enable 0: Disable 1: Enable */

/* Exynos 3830 UM - TODO */
#define TOP_OUT			0x3920
#define PWRRGTON_CP		BIT(1) /* XPWRRTON_CP contr */

#define WAKEUP_INT_TYPE 0x3948
#define RESETREQ_WLBT   BIT(25) /* Interrupt type 0:Edge, 1:Level */

/* Exynos 3830 UM - TODO */
#define TCXO_BUF_CTRL		0x3B78
#define TCXO_BUF_BIAS_EN_WLBT	BIT(0)

/* New WLBT SFRs for MEM config */

/* end address is exclusive so the ENDx register should be set to the first
 * address that is not accessible through that BAAW.
 *
 * Another very important point to note here is we are using BAAW0 to expose
 * 16MB region, so other BAAWs can be used for other purposes
 */
#define WLBT_DBUS_BAAW_0_START          0x80000000 // Start of DRAM for WLBT R7
#define WLBT_DBUS_BAAW_0_END            0x80C00000 // 12 MB
#define WLBT_DBUS_BAAW_1_START          0xC0000000
#define WLBT_DBUS_BAAW_1_END            0xDFFFFFFF

/* #define WLBT_DBUS_BAAW_2_START       0x80800000
#define WLBT_DBUS_BAAW_2_END            WLBT_DBUS_BAAW_3_START
#define WLBT_DBUS_BAAW_3_START          0x80C00000
#define WLBT_DBUS_BAAW_3_END            WLBT_DBUS_BAAW_4_START
#define WLBT_DBUS_BAAW_4_START          0x81000000
#define WLBT_DBUS_BAAW_4_END            0x813FFFFF */

#define WLBT_BAAW_CON_INIT_DONE		(1 << 31)
#define WLBT_BAAW_CON_EN_WRITE		(1 << 1)
#define WLBT_BAAW_CON_EN_READ		(1 << 0)
#define WLBT_BAAW_ACCESS_CTRL		(WLBT_BAAW_CON_INIT_DONE | WLBT_BAAW_CON_EN_WRITE | WLBT_BAAW_CON_EN_READ)

/* ref Confluence Maxwell152+Memory+Map */
#define WLBT_CBUS_BAAW_0_START          0xA0000000		// CP2WLBT MBOX
#define WLBT_CBUS_BAAW_0_END            0xA000FFFF//WLBT_CBUS_BAAW_1_START
#define WLBT_CBUS_BAAW_1_START          0xA0010000		// MAILBOX_GNSS2WLBT
#define WLBT_CBUS_BAAW_1_END            0xA00CFFFF//WLBT_CBUS_BAAW_6_START		// TODO
#define WLBT_CBUS_BAAW_2_START          0xA0020000		// MAILBOX_APM2WLBT
#define WLBT_CBUS_BAAW_2_END            WLBT_CBUS_BAAW_3_START
#define WLBT_CBUS_BAAW_3_START          0xA0030000		// MAILBOX_AP2WLBT
#define WLBT_CBUS_BAAW_3_END            WLBT_CBUS_BAAW_4_START
#define WLBT_CBUS_BAAW_4_START          0xA0040000		// MAILBOX_WLBT2ABOX
#define WLBT_CBUS_BAAW_4_END            WLBT_CBUS_BAAW_5_START
#define WLBT_CBUS_BAAW_5_START          0xA0050000		// MAILBOX_WLBT2CHUB
#define WLBT_CBUS_BAAW_5_END            WLBT_CBUS_BAAW_6_START
#define WLBT_CBUS_BAAW_6_START          0xA0060000		// GPIO_CMGP
#define WLBT_CBUS_BAAW_6_END            WLBT_CBUS_BAAW_7_START
#define WLBT_CBUS_BAAW_7_START          0xA0070000		// ADC_CMGP_AP
#define WLBT_CBUS_BAAW_7_END            WLBT_CBUS_BAAW_8_START
#define WLBT_CBUS_BAAW_8_START          0xA0080000		// ADC_CMGP_CP
#define WLBT_CBUS_BAAW_8_END            WLBT_CBUS_BAAW_9_START
#define WLBT_CBUS_BAAW_9_START          0xA0090000		// SYSREG_CMGP2WLBT
#define WLBT_CBUS_BAAW_9_END            WLBT_CBUS_BAAW_A_START
#define WLBT_CBUS_BAAW_A_START          0xA00A0000		// USI_CMGP00
#define WLBT_CBUS_BAAW_A_END            WLBT_CBUS_BAAW_B_START
#define WLBT_CBUS_BAAW_B_START          0xA00B0000		// reserved
#define WLBT_CBUS_BAAW_B_END            WLBT_CBUS_BAAW_C_START
#define WLBT_CBUS_BAAW_C_START          0xA00C0000		// USI_CMGP01
#define WLBT_CBUS_BAAW_C_END            WLBT_CBUS_BAAW_D_START
#define WLBT_CBUS_BAAW_D_START          0xA00D0000		// CHUB_SRAM
#define WLBT_CBUS_BAAW_D_END            0xA010FFFF

#define WLBT_PBUS_MBOX_CP2WLBT_BASE	0x11950000
#define WLBT_PBUS_MBOX_GNSS2WLBT_BASE	0x119A0000
#define WLBT_PBUS_MBOX_APM2WLBT_BASE	0x119B0000
#define WLBT_PBUS_MBOX_AP2WLBT_BASE	0x119C0000
#define WLBT_PBUS_MBOX_WLBT2ABOX_BASE	0x119D0000
#define WLBT_PBUS_MBOX_WLBT2CHUB_BASE	0x119E0000
#define WLBT_PBUS_GPIO_CMGP_BASE	0x11C30000
#define WLBT_PBUS_ADC_CMGP_AP_BASE	0x11C40000
#define WLBT_PBUS_ADC_CMGP_CP_BASE	0x11C50000
#define WLBT_PBUS_SYSREG_CMGP2WLBT_BASE	0x11C60000
#define WLBT_PBUS_USI_CMG00_BASE        0x11C70000
#define WLBT_PBUS_USI_CMG01_BASE        0x11D20000
#define WLBT_PBUS_CHUB_BASE             0x10E00000 /* TODO: confirm correct address */

/* CHIP_VERSION_ID SFR (remap block) 0x14c50410
 */
#define CHIP_VERSION_ID_OFFSET          0x410
#define CHIP_VERSION_ID_VER_MASK        0xFFFFFFFF      /* [00:31] Version ID */
#define CHIP_VERSION_ID_IP_PMU          0x0000F000      /* [12:15] PMU ROM Rev */
#define CHIP_VERSION_ID_IP_MINOR        0x000F0000      /* [16:19] Minor Rev */
#define CHIP_VERSION_ID_IP_MAJOR        0x00F00000      /* [20:23] Major Rev */
#define CHIP_VERSION_ID_IP_PMU_SHIFT    12
#define CHIP_VERSION_ID_IP_MINOR_SHIFT  16
#define CHIP_VERSION_ID_IP_MAJOR_SHIFT  20

/* TZASC (TrustZone Address Space Controller) configuration for Katmai onwards */
#define EXYNOS_SET_CONN_TZPC    0
#define SMC_CMD_CONN_IF         (0x82000710)
#endif /* __MIF_REG_3830_H */
