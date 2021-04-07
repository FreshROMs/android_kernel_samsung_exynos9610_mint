/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __CONTEXTHUB_IPC_H_
#define __CONTEXTHUB_IPC_H_

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/platform_data/nanohub.h>
#include <linux/sched/clock.h>
#include <linux/sched/signal.h>
#include "chub_ipc.h"
#include "chub_log.h"

#define WAIT_TRY_CNT (3)
#define RESET_WAIT_TRY_CNT (10)
#define WAIT_CHUB_MS (100)

/* utils for nanohub main */
#define wait_event_interruptible_timeout_locked(q, cond, tmo)		\
({									\
	long __ret = (tmo);						\
	DEFINE_WAIT(__wait);						\
	if (!(cond)) {							\
		for (;;) {						\
			__wait.flags &= ~WQ_FLAG_EXCLUSIVE;		\
			if (list_empty(&__wait.entry))			\
				__add_wait_queue_entry_tail(&(q), &__wait);	\
			set_current_state(TASK_INTERRUPTIBLE);		\
			if ((cond))					\
				break;					\
			if (signal_pending(current)) {			\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
			spin_unlock_irq(&(q).lock);			\
			__ret = schedule_timeout(__ret);		\
			spin_lock_irq(&(q).lock);			\
			if (!__ret) {					\
				if ((cond))				\
					__ret = 1;			\
				break;					\
			}						\
		}							\
		__set_current_state(TASK_RUNNING);			\
		if (!list_empty(&__wait.entry))				\
			list_del_init(&__wait.entry);			\
		else if (__ret == -ERESTARTSYS &&			\
			 /*reimplementation of wait_abort_exclusive() */\
			 waitqueue_active(&(q)))			\
			__wake_up_locked_key(&(q), TASK_INTERRUPTIBLE,	\
			NULL);						\
	} else {							\
		__ret = 1;						\
	}								\
	__ret;								\
})

#define CHUB_RESET_ENABLE
enum mailbox_event {
	MAILBOX_EVT_UTC_MAX = IPC_DEBUG_UTC_MAX,
	MAILBOX_EVT_DUMP_STATUS = IPC_DEBUG_DUMP_STATUS,
	MAILBOX_EVT_WAKEUP,
	MAILBOX_EVT_WAKEUP_CLR,
	MAILBOX_EVT_ERASE_SHARED,
	MAILBOX_EVT_ENABLE_IRQ,
	MAILBOX_EVT_DISABLE_IRQ,
	MAILBOX_EVT_RESET_EVT_START,
	MAILBOX_EVT_INIT_IPC,
	MAILBOX_EVT_POWER_ON,
	MAILBOX_EVT_CHUB_ALIVE,
	MAILBOX_EVT_SHUTDOWN,
	MAILBOX_EVT_RESET,
	MAILBOX_EVT_MAX,
};

enum chub_status {
	CHUB_ST_NO_POWER,
	CHUB_ST_POWER_ON,
	CHUB_ST_RUN,
	CHUB_ST_SHUTDOWN,
	CHUB_ST_NO_RESPONSE,
	CHUB_ST_ERR,
	CHUB_ST_HANG,
};

struct read_wait {
	atomic_t cnt;
	volatile u32 flag;
	wait_queue_head_t event;
};

struct recv_ctrl {
	unsigned long order;
	volatile unsigned long container[IRQ_EVT_CH_MAX];
};

struct chub_alive {
	unsigned int flag;
	wait_queue_head_t event;
};

#ifdef USE_EXYNOS_LOG
#define CHUB_DBG_DIR "/data/exynos/log/chub"
#else
#define CHUB_DBG_DIR "/data"
#endif

enum chub_err_type {
	CHUB_ERR_NONE,
	CHUB_ERR_ITMON,
	CHUB_ERR_FW_FAULT, /* chub error */
	CHUB_ERR_FW_WDT,
	CHUB_ERR_EVTQ_NO_HW_TRIGGER,
	CHUB_ERR_CHUB_NO_RESPONSE, /* 5 */
	CHUB_ERR_CRITICAL,
	CHUB_ERR_EVTQ_ADD, /* ap error */
	CHUB_ERR_EVTQ_EMTPY,
	CHUB_ERR_READ_FAIL,
	CHUB_ERR_WRITE_FAIL, /* 10 */
	CHUB_ERR_NEED_RESET,
	CHUB_ERR_FW_ERROR = CHUB_ERR_NEED_RESET,
	CHUB_ERR_COMMS_NACK, /* ap comms error */
	CHUB_ERR_COMMS_BUSY,
	CHUB_ERR_COMMS_UNKNOWN,
	CHUB_ERR_COMMS,
	CHUB_ERR_RESET_CNT,
	CHUB_ERR_NANOHUB, /* nanohub dbg error */
	CHUB_ERR_MAX,
};

struct contexthub_baaw_info {
	unsigned int baaw_p_apm_chub_start;
	unsigned int baaw_p_apm_chub_end;
	unsigned int baaw_p_apm_chub_remap;
};

struct pmu_shub {
	int reason;
	unsigned int top_bus_shub;
	unsigned int top_pwr_shub;
	unsigned int logic_reset_shub;
	unsigned int reset_otp_shub;
	unsigned int reset_cmu_shub;
	unsigned int reset_subcpu_shub;
};

#ifdef CONFIG_SENSORS_SSP
struct ssp_data;
#endif

#define CHUB_IRQ_PIN_MAX (5)
struct contexthub_ipc_info {
	u32 cur_err;
	int err_cnt[CHUB_ERR_MAX];
	struct device *dev;
	struct nanohub_data *data;
	struct nanohub_platform_data *pdata;
	wait_queue_head_t wakeup_wait;
	struct work_struct debug_work;
	struct read_wait read_lock;
	struct chub_alive chub_alive_lock;
	void __iomem *sram;
	void __iomem *mailbox;
	void __iomem *chub_dumpgpr;
	void __iomem *chub_baaw;
	void __iomem *pmu_chub_reset;
	void __iomem *pmu_chub_cpu;
	void __iomem *cmu_chub_qch;
	struct contexthub_baaw_info baaw_info;
	struct ipc_map_area *ipc_map;
	struct log_buffer_info *fw_log;
	struct log_buffer_info *dd_log;
	struct LOG_BUFFER *dd_log_buffer;
	unsigned long clkrate;
	atomic_t chub_shutdown;
	atomic_t chub_status;
	atomic_t in_reset;
	atomic_t irq1_apInt;
	atomic_t wakeup_chub;
	atomic_t in_use_ipc;
	int irq_mailbox;
	int irq_wdt;
	bool irq_wdt_disabled;
	int utc_run;
	int powermode;
	int block_reset;
	struct pmu_shub pmu_shub_status;
	struct mailbox_sfr mailbox_sfr_dump;
	bool os_load;
	char os_name[MAX_FILE_LEN];
	struct notifier_block itmon_nb;
	u32 irq_pin_len;
	u32 irq_pins[CHUB_IRQ_PIN_MAX];
	struct wakeup_source ws_reset;
#ifdef CONFIG_CONTEXTHUB_DEBUG
	struct work_struct utc_work;
#endif
#ifdef CONFIG_SENSORS_SSP
	int sensor_ldo_en;
	struct ssp_data *ssp_data;
#endif
};

/*	PMU SHUB REGISTERS	*/
#if defined(CONFIG_SOC_EXYNOS9610)
#define TOP_BUS_SHUB_STATUS		0x2c64
#define TOP_PWR_SHUB_STATUS		0x2ce4
#define LOGIC_RESET_SHUB_STATUS		0x2d84
#define RESET_OTP_SHUB_STATUS		0x2b84
#define RESET_CMU_SHUB_STATUS		0x2a64
#define RESET_SUBCPU_SHUB_STATUS	0x3d24
#endif

/*	PMU CHUB_CPU registers */
#if defined(CONFIG_SOC_EXYNOS9810)
#define REG_CHUB_CPU_STATUS (0x0)
#elif defined(CONFIG_SOC_EXYNOS9610)
#define REG_CHUB_CPU_STATUS (0x4)
#else
/* TODO: Need to check */
#define REG_CHUB_CPU_STATUS (0x0)
#endif
#define REG_CHUB_CPU_STATUS_BIT_STANDBYWFI (28)
#if defined(CONFIG_SOC_EXYNOS9810)
#define REG_CHUB_CPU_OPTION (0x4)
#define ENABLE_SYSRESETREQ BIT(4)
#elif defined(CONFIG_SOC_EXYNOS9610)
#define REG_CHUB_CPU_OPTION (0x8)
#define ENABLE_SYSRESETREQ BIT(9)
#else
/* TODO: Need to check */
#define REG_CHUB_CPU_OPTION (0x0)
#define ENABLE_SYSRESETREQ BIT(0)
#endif
#define REG_CHUB_CPU_DURATION (0x8)

/*	PMU CHUB_RESET registers */
#define REG_CHUB_RESET_CHUB_CONFIGURATION (0x0)
#define REG_CHUB_RESET_CHUB_STATUS (0x4)
#define REG_CHUB_RESET_CHUB_OPTION (0x8)
#if defined(CONFIG_SOC_EXYNOS9810)
#define CHUB_RESET_RELEASE_VALUE (0x10000000)
#elif defined(CONFIG_SOC_EXYNOS9610)
#define CHUB_RESET_RELEASE_VALUE (0x8000)
#else
/* TODO: Need to check */
#define CHUB_RESET_RELEASE_VALUE (0x0)
#endif

/*	CMU CHUB_QCH registers	*/
#if defined(CONFIG_SOC_EXYNOS9610)
#define REG_QCH_CON_CM4_SHUB_QCH (0x8)
#define IGNORE_FORCE_PM_EN BIT(2)
#define CLOCK_REQ BIT(1)
#define ENABLE BIT(0)
#endif

/*	CHUB dump gpr Registers : CHUB BASE + 0x1f000000 */
#define REG_CHUB_DUMPGPR_CTRL (0x0)
#define REG_CHUB_DUMPGPR_PCR  (0x4)
#define REG_CHUB_DUMPGPR_GP0R (0x10)
#define REG_CHUB_DUMPGPR_GP1R (0x14)
#define REG_CHUB_DUMPGPR_GP2R (0x18)
#define REG_CHUB_DUMPGPR_GP3R (0x1c)
#define REG_CHUB_DUMPGPR_GP4R (0x20)
#define REG_CHUB_DUMPGPR_GP5R (0x24)
#define REG_CHUB_DUMPGPR_GP6R (0x28)
#define REG_CHUB_DUMPGPR_GP7R (0x2c)
#define REG_CHUB_DUMPGPR_GP8R (0x30)
#define REG_CHUB_DUMPGPR_GP9R (0x34)
#define REG_CHUB_DUMPGPR_GPAR (0x38)
#define REG_CHUB_DUMPGPR_GPBR (0x3c)
#define REG_CHUB_DUMPGPR_GPCR (0x40)
#define REG_CHUB_DUMPGPR_GPDR (0x44)
#define REG_CHUB_DUMPGPR_GPER (0x48)
#define REG_CHUB_DUMPGPR_GPFR (0x4c)

#define IPC_HW_WRITE_DUMPGPR_CTRL(base, val) \
	__raw_writel((val), (base) + REG_CHUB_DUMPGPR_CTRL)
#define IPC_HW_READ_DUMPGPR_PCR(base) \
	__raw_readl((base) + REG_CHUB_DUMPGPR_PCR)

/*	CHUB BAAW Registers : CHUB BASE + 0x100000 */
#define REG_BAAW_D_CHUB0 (0x0)
#define REG_BAAW_D_CHUB1 (0x4)
#define REG_BAAW_D_CHUB2 (0x8)
#define REG_BAAW_D_CHUB3 (0xc)
#define BAAW_VAL_MAX (4)
#define BAAW_RW_ACCESS_ENABLE 0x80000003

#define IPC_MAX_TIMEOUT (0xffffff)
#define INIT_CHUB_VAL (-1)

#define IPC_HW_WRITE_BAAW_CHUB0(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB0)
#define IPC_HW_WRITE_BAAW_CHUB1(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB1)
#define IPC_HW_WRITE_BAAW_CHUB2(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB2)
#define IPC_HW_WRITE_BAAW_CHUB3(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB3)

int contexthub_ipc_write_event(struct contexthub_ipc_info *data,
				enum mailbox_event event);
int contexthub_ipc_read(struct contexthub_ipc_info *ipc,
				uint8_t *rx, int max_length, int timeout);
int contexthub_ipc_write(struct contexthub_ipc_info *ipc,
				uint8_t *tx, int length, int timeout);
int contexthub_poweron(struct contexthub_ipc_info *data);
int contexthub_download_image(struct contexthub_ipc_info *data, enum ipc_region reg);
int contexthub_reset(struct contexthub_ipc_info *ipc, bool force_load, int dump_id);
int contexthub_wakeup(struct contexthub_ipc_info *data, int evt);
int contexthub_request(struct contexthub_ipc_info *ipc);
void contexthub_release(struct contexthub_ipc_info *ipc);

#ifdef CONFIG_SENSORS_SSP
int contexthub_get_token(struct contexthub_ipc_info *ipc);
void contexthub_put_token(struct contexthub_ipc_info *ipc);
#endif
#endif
