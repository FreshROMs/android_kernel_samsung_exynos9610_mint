/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_H
#define __MIF_REG_H

/*********************************/
/* PLATFORM register definitions */
/*********************************/
#define NUM_MBOX_PLAT   8
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
/* R1 [15:0]  - Int TO R4 */
#define INTGR1                  0x01c /* Interrupt Generation Register 1 */
#define INTCR1                  0x020 /* Interrupt Clear Register 1 */
#define INTMR1                  0x024 /* Interrupt Mask Register 1 */
#define INTSR1                  0x028 /* Interrupt Status Register 1 */
#define INTMSR1                 0x02c /* Interrupt Mask Status Register 1 */
/* R2 [15:0]  - Int TO M4 */
#define INTGR2                  0x030 /* Interrupt Generation Register 2 */
#define INTCR2                  0x034 /* Interrupt Clear Register 2 */
#define INTMR2                  0x038 /* Interrupt Mask Register 2 */
#define INTSR2                  0x03c /* Interrupt Status Register 2 */
#define INTMSR2                 0x040 /* Interrupt Mask Status Register 2 */
#define MIF_INIT                0x04c /* MIF_init */
#define IS_VERSION              0x050 /* Version Information Register */
#define ISSR_BASE               0x080 /* IS_Shared_Register Base address */
#define ISSR(r)                 (ISSR_BASE + (4 * (r)))
#define SEMAPHORE_BASE          0x180 /* IS_Shared_Register Base address */
#define SEMAPHORE(r)            (SEMAPHORE_BASE + (4 * (r)))
#define SEMA0CON                0x1c0
#define SEMA0STATE              0x1c8


/* POWER */
/* Page 594 datasheet */
/* Base Address - 0x11C8_0000 */
#define WIFI_CTRL_NS            0x0140 /* WIFI Control SFR non-secure */
#define WIFI_PWRON              BIT(1)
#define WIFI_RESET_SET          BIT(2)
#define WIFI_ACTIVE_EN          BIT(5) /* Enable of WIFI_ACTIVE_REQ */
#define WIFI_ACTIVE_CLR         BIT(6) /* WIFI_ACTIVE_REQ is clear internally on WAKEUP */
#define WIFI_RESET_REQ_EN       BIT(7) /* 1:enable, 0:disable  Enable of WIFI_RESET_REQ */
#define WIFI_RESET_REQ_CLR      BIT(8) /* WIFI_RESET_REQ is clear internally on WAKEUP */
#define MASK_WIFI_PWRDN_DONE    BIT(9) /* 1:mask, 0 : pass RTC clock out enable to WIFI
					* This mask WIFI_PWRDN_DONE come in from WIFI.
					* If MASK_WIFI_PWRDN_DONE = 1, WIFI enter to DOWN
					* state without checking WIFI_PWRDN_DONE*/



#define WIFI_CTRL_S             0x0144 /* WIFI Control SFR secure */
#define WIFI_START              BIT(3) /* WIFI Reset release control  If WIFI_START = 1,
					* WIFI exit from DOWN state and go to UP state.
					* If this field is set to high (WIFI_START = 1)
					* WIFI state can go to UP state. This signal can be
					* auto-clear by DIRECTWR at UP */

#define WIFI_STAT               0x0148 /* Indicate whether WIFI uses MIF domain */
#define WIFI_DEBUG              0x014c /* MIF sleep, wakeup debugging control */
/* Page 1574 datasheet */
#define PMU_ALIVE_BASE          0x0000
#define PMU_ALIVE_REG(r)        (PMU_ALIVE_BASE + (r))
#define WIFI2AP_MEM_CONFIG0     0x0150 /* Control WLBT_MEM_SIZE. */
#define WLBT2AP_MIF_ACCESS_WIN0 0x0154 /* ACCESS_CONTROL_PERI_IP */
#define WLBT2AP_MIF_ACCESS_WIN1 0x0158 /* ACCESS_CONTROL_PERI_IP */
#define WLBT2AP_MIF_ACCESS_WIN2 0x015a /* ACCESS_CONTROL_PERI_IP */
#define WLBT2AP_MIF_ACCESS_WIN3 0x0160 /* ACCESS_CONTROL_PERI_IP */
#define WIFI2AP_MEM_CONFIG1     0x0164 /* Control WLBT_MEM_BA0 */
#define WLBT_BOOT_TEST_RST_CFG  0x0168 /* WLBT_IRAM_BOOT_OFFSET */
					/* WLBT_IRAM_BOOT_TEST */
					/* WLBT2AP_PERI_PROT2 */
#define WLBT2AP_PERI_ACCESS_WIN 0x016c /* WLBT2AP_PERI_ACCESS_END - WLBT2AP_PERI_ACCESS_START */
#define WIFI2AP_MODAPIF_CONFIG  0x0170 /* WLBT2AP_PERI_ACCESS_END - WLBT2AP_PERI_ACCESS_START */
#define WIFI2AP_QOS             0x0170 /* RT */
#define WIFI2AP_MEM_CONFIG2     0x017c /* Control WLBT_MEM_BA1 */
#define WIFI2AP_MEM_CONFIG3     0x0184 /* Control WLBT_ADDR_RNG */

/* Power down registers */
#define RESET_ASB_WIFI_SYS_PWR_REG 0x11f4     /* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define TCXO_GATE_WIFI_SYS_PWR_REG 0x11f0     /* Control power state in LOWPWR mode 1 - on, 0 */
#define LOGIC_RESET_WIFI_SYS_PWR_REG 0x11f8   /* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define CLEANY_BUS_WIFI_SYS_PWR_REG 0x11fc    /* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define CENTRAL_SEQ_WIFI_CONFIGURATION 0x0380 /* bit 16. Decides whether system-level low-power mode
						* is used HIGH: System-level Low-Power mode
						* disabled. LOW: System-level Low-Power mode
						* enabled. When system enters low-power mode,
						* this field is automatically cleared to HIGH. */

#define CENTRAL_SEQ_WIFI_STATUS 0x0384        /* 23:16  Check statemachine status */
#define STATES                  0xff0000

#define SYS_PWR_CFG             BIT(0)
#define SYS_PWR_CFG_2           (BIT(0) | BIT(1))
#define SYS_PWR_CFG_16          BIT(16)


/* CMU registers to request PLL for USB Clock */
#define USBPLL_CON0 0x1000
#define AP2WIFI_USBPLL_REQ	BIT(0)	/* 1: Request PLL, 0: Release PLL */

#define USBPLL_CON1 0x1004 /* */
#define AP2WLBT_USBPLL_WPLL_SEL BIT(0)	/* 1: WLBT, 0: AP */
#define AP2WLBT_USBPLL_WPLL_EN  BIT(1)	/* 1: Enable, 0: Disable */

/***** Interrupts ********
 *
 * - MBOX
 * - WIFI_ACTIVE (pag 553)
 *   comes from BLK_WIFI. Initial value is low and then this value becomes high after WIFI booting. If
 *   some problem occurs within WIFI, WIFI_ACTIVE can be low by WIFI CPU. AP CPU detects that WIFI_ACTIVE is
 *   low after WIFI_ACTIVE is high. And WIFI_ACTIVE detected goes to GIC's interrupt source. At ISR, AP CPU
 *   makes wake source and interrupt clear as setting WIFI_CTRL__WIFI_ACTIVE_CLR. WIFI_ACTIVE_CLR is auto
 *   clear by direct-write function.
 *
 * - WIFI_RESET_REQ (pag 554)
 *   WIFI can request WIFI reset only by WIFI_RESET_REQ. If WIFI_RESET_REQ is asserted, AP PMU detects it as
 *   wakeup source and interrupt source. At ISR, AP CPU makes wakeup source clear as setting
 *   WIFI_CTRL__CP_RESET_REQ_CLR. But, interrupt can be not clear because the interrupt goes to GIC directly
 *   from WIFI. (It use make function within GIC) WIFI_RESET_REQ_CLR is auto clear by direct-write function.
 */

#endif /* __MIF_REG_H */
