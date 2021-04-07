/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_7885_H
#define __MIF_REG_7885_H

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
/* Page 1173 datasheet */
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
#define RTC_OUT_EN		BIT(10) /* 1:enable, 0 : disable This is enable signal on RTC
					 * CLK(32KHz). This clock can be used as WIFI PMU
					 * clock when WIFI is internal power-down and
					 * TCXO(26MHz) is disable at WIFI side.*/
#define TCXO_ENABLE_SW		BIT(11) /* 1:enable, 0 : disable This is enable signal on TCXO
					 * clock of WIFI. This signal can decide whether TCXO
					 * clock is active by software when WIFI is internal
					 * power-down or WIFI is in reset state at WIFI side. if
					 * this value is HIGH, TCXO is active regardless of
					 * hardware control */
#define MASK_MIF_REQ		BIT(12) /* 1:mask MIF_REQ comming from WIFI, 0 : disable */
#define SET_SW_MIF_REQ		BIT(13) /* MIF SLEEP control by SW 1: if MASK_MIF_REQ is
					   set to HIGH, MIF enters into down state by
					   SET_SW_MIF_REQ. */
#define SWEEPER_BYPASS_DATA_EN	BIT(16) /* CLEANY bypass mode control(WIFI2AP MEM path)
					   If this bit is set to 1, CLEANY in MIF block starts
					   operation. If this bit is set to 0, CLEANY is bypass
					   mode.*/
#define SFR_SERIALIZER_DUR_DATA2REQ (BIT(20) | BIT(21)) /* Duration between DATA and REQUEST on
							   SFR_SERIALIZER */


#define WIFI_CTRL_S             0x0144 /* WIFI Control SFR secure */
#define WIFI_START              BIT(3) /* WIFI Reset release control  If WIFI_START = 1,
					* WIFI exit from DOWN state and go to UP state.
					* If this field is set to high (WIFI_START = 1)
					* WIFI state can go to UP state. This signal can be
					* auto-clear by DIRECTWR at UP */

#define WIFI_STAT               0x0148
#define WIFI_PWRDN_DONE		BIT(0) /* Check WIFI power-down status.*/
#define WIFI_ACCESS_MIF		BIT(4) /* Check whether WIFI accesses MIF doman */

#define WIFI_DEBUG              0x014c /* MIF sleep, wakeup debugging control */
#define EN_MIF_REQ		BIT(0) /* Control MIF_REQ through GPIO_ALIVE. */
#define EN_WIFI_ACTIVE		BIT(2) /* Control WIFI_ACTIVE through GPIO_ALIVE. */
#define EN_MIF_RESET_REQ	BIT(3) /* Control WIFI_RESET_REQ through GPIO_ALIVE. */
#define MASK_CLKREQ_WIFI	BIT(8) /* When this field is set to HIGH, ALIVE ignores
					* CLKREQ from WIFI.*/

/* TODO: Might be 0x10480000 */
#define PMU_ALIVE_BASE          0x0000
#define PMU_ALIVE_REG(r)        (PMU_ALIVE_BASE + (r))
#define WIFI2AP_MEM_CONFIG0     0x7300 /* MEM_SIZE SECTION_0 */
#define WIFI2AP_MEM_CONFIG1     0x7304 /* BASE ADDRESS SECTION 0*/
#define WIFI2AP_MEM_CONFIG2     0x7308 /* MEM_SIZE SECTION_0 */
#define WIFI2AP_MEM_CONFIG3     0x730C /* BASE ADDRESS SECTION 1*/
#define WIFI2AP_MEM_CONFIG4     0x7310 /* MEM_SIZE SECTION_1 */
#define WIFI2AP_MEM_CONFIG5     0x7314 /* BASE ADDRESS SECTION 0*/
#define WIFI2AP_MIF_ACCESS_WIN0 0x7318 /* ACCESS_CONTROL SFR */
#define WIFI2AP_MIF_ACCESS_WIN1 0x731c /* ACCESS_CONTROL SFR */
#define WIFI2AP_PERI0_ACCESS_WIN0 0x7320 /* ACCESS WINDOW PERI */
#define WIFI2AP_PERI0_ACCESS_WIN1 0x7324 /* ACCESS WINDOW PERI */
#define WIFI2AP_PERI0_ACCESS_WIN2 0x7328 /* ACCESS WINDOW PERI */
#define WIFI2AP_PERI0_ACCESS_WIN3 0x732c /* ACCESS WINDOW PERI */
#define WLBT_BOOT_TEST_RST_CFG  0x7330  /* WLBT_IRAM_BOOT_OFFSET */
					/* WLBT_IRAM_BOOT_TEST */
					/* WLBT2AP_PERI_PROT2 */

/* Power down registers */
#define RESET_AHEAD_WIFI_PWR_REG 0x1360		/* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define CLEANY_BUS_WIFI_SYS_PWR_REG 0x1364	/* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define LOGIC_RESET_WIFI_SYS_PWR_REG 0x1368	/* Control power state in LOWPWR mode 1 - on, 0 - down*/
#define TCXO_GATE_WIFI_SYS_PWR_REG 0x136c	/* Control power state in LOWPWR mode 1 - on, 0 */
#define WIFI_DISABLE_ISO_SYS_PWR_REG 0x1370	/* Control power state in LOWPWR mode 1 - on, 0 */
#define WIFI_RESET_ISO_SYS_PWR_REG 0x1374	/* Control power state in LOWPWR mode 1 - on, 0 */

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

/* TZASC configuration for Katmai onward */
#define WLBT_TZASC			0
#define EXYNOS_SMC_WLBT_TZASC_CMD	0x82000710
#endif /* __MIF_REG_7885_H */
