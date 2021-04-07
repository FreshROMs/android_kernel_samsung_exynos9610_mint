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


#ifndef _MAILBOX_CHUB_IPC_H
#define _MAILBOX_CHUB_IPC_H

#if defined(SEOS) || defined(EMBOS)
#define CHUB_IPC
#else
#define AP_IPC
#endif

#define IPC_VERSION (181024)

#if defined(CHUB_IPC)
#if defined(SEOS)
#include <nanohubPacket.h>
#elif defined(EMBOS)
/* TODO: Add embos */
#define SUPPORT_LOOPBACKTEST
#endif
#include <csp_common.h>
#elif defined(AP_IPC)
#if defined(CONFIG_NANOHUB)
#include "comms.h"
#elif defined(CONFIG_CONTEXTHUB_DRV)
// TODO: Add packet size.. #define PACKET_SIZE_MAX ()
#endif
#endif

#ifndef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX (769) // txBuffer max size 769 : sensor data(512) + debug data(250 + 2) + msg header(5)
//#define PACKET_SIZE_MAX (272) // txBuffer max size 769 : sensor data(512) + debug data(250 + 2) + msg header(5)
#endif

#ifdef LOWLEVEL_DEBUG
#define DEBUG_LEVEL (0)
#else
#if defined(CHUB_IPC)
#define DEBUG_LEVEL (LOG_ERROR)
#elif defined(AP_IPC)
#define DEBUG_LEVEL (KERN_ERR)
#endif
#endif

#ifndef CSP_PRINTF_INFO
#ifdef AP_IPC
#ifdef LOWLEVEL_DEBUG
#define CSP_PRINTF_INFO(fmt, ...) log_printf(fmt, ##__VA_ARGS__)
#define CSP_PRINTF_ERROR(fmt, ...) log_printf(fmt, ##__VA_ARGS__)
#else
#define CSP_PRINTF_INFO(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#define CSP_PRINTF_ERROR(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#endif
#endif
#endif

#ifdef LOWLEVEL_DEBUG
#define DEBUG_PRINT(lv, fmt, ...)	\
	((DEBUG_LEVEL == (0)) ? (CSP_PRINTF_INFO(fmt, ##__VA_ARGS__)) : \
	((DEBUG_LEVEL == (lv)) ? (CSP_PRINTF_INFO(fmt, ##__VA_ARGS__)) : (NULL)))
#else
#define DEBUG_PRINT(level, fmt, ...)
#endif

/* contexthub bootargs */
#define BL_OFFSET		(0x0)
#define MAP_INFO_OFFSET (256)
#define MAP_INFO_MAX_SIZE (128)
#define CHUB_PERSISTBUF_SIZE (96)

#define OS_UPDT_MAGIC	"Nanohub OS"

#define BOOTMODE_COLD       (0x7733)
#define BOOTMODE_PWRGATING  (0x1188)

#define KERNEL_LOG_ON		(0x1)
#define KERNEL_LOG_OFF		(0x0)

#define MAILBOX_REQUEST_KLOG_ON     (0x1)
#define MAILBOX_REQUEST_KLOG_OFF    (0x2)
#define MAILBOX_REQUEST_AP_PREPARE  (0x4)
#define MAILBOX_REQUEST_AP_COMPLETE (0x8)

struct chub_bootargs {
	char magic[16];
	u32 ipc_version;
	u32 bl_start;
	u32 bl_end;
	u32 code_start;
	u32 code_end;
	u32 ipc_start;
	u32 ipc_end;
	u32 ram_start;
	u32 ram_end;
	u32 shared_start;
	u32 shared_end;
	u32 dump_start;
	u32 dump_end;
	u32 chubclk;
	u16 bootmode;
	u16 kernel_log;
#if defined(LOCAL_POWERGATE)
	u32 psp;
	u32 msp;
#endif
};

/* ipc map
 * data channel: AP -> CHUB
 * data channel: CHUB -> AP
 * event channel: AP -> CHUB / ctrl
 * event channel: CHUB -> AP / ctrl
 * logbuf / logbuf_ctrl
 */
#define IPC_BUF_NUM (IRQ_EVT_CH_MAX)
#define IPC_EVT_NUM (30)
#define IPC_LOGBUF_NUM (256)

enum sr_num {
	SR_0 = 0,
	SR_1 = 1,
	SR_2 = 2,
	SR_3 = 3,
};

#define SR_A2C_ADDR SR_0
#define SR_A2C_SIZE SR_1
#define SR_C2A_ADDR SR_2
#define SR_C2A_SIZE SR_3
#define SR_DEBUG_ACTION SR_0
#define SR_DEBUG_VAL_LOW SR_1
#define SR_DEBUG_VAL_HIGH SR_2
#define SR_CHUB_ALIVE SR_3
#define SR_BOOT_MODE SR_0

enum irq_chub {
	IRQ_C2A_START,
	IRQ_C2A_END = 2,
	IRQ_EVT_START,
	IRQ_EVT_END = 15,
	IRQ_CHUB_ALIVE = 15,
	IRQ_INVAL = 0xff,
};

enum irq_evt_chub {
	IRQ_EVT_CH0,		/* data channel */
	IRQ_EVT_CH1,
	IRQ_EVT_CH2,
	IRQ_EVT_CH_MAX,
	IRQ_EVT_A2C_RESET = IRQ_EVT_CH_MAX,
	IRQ_EVT_A2C_WAKEUP,
	IRQ_EVT_A2C_WAKEUP_CLR,
	IRQ_EVT_A2C_SHUTDOWN,
	IRQ_EVT_A2C_LOG,
	IRQ_EVT_A2C_DEBUG,
	IRQ_EVT_C2A_DEBUG = IRQ_EVT_CH_MAX,
	IRQ_EVT_C2A_ASSERT,
	IRQ_EVT_C2A_INT,
	IRQ_EVT_C2A_INTCLR,
	IRQ_EVT_CHUB_EVT_MAX = 15,
	IRQ_EVT_CHUB_ALIVE = IRQ_EVT_CHUB_EVT_MAX,
	IRQ_EVT_CHUB_MAX = 16,	/* max irq number on mailbox */
	IRQ_EVT_INVAL = 0xff,
};

enum ipc_debug_event {
	IPC_DEBUG_UTC_STOP, /* no used. UTC_NONE */
	IPC_DEBUG_UTC_AGING,
	IPC_DEBUG_UTC_WDT,
	IPC_DEBUG_UTC_RTC,
	IPC_DEBUG_UTC_MEM,
	IPC_DEBUG_UTC_TIMER,
	IPC_DEBUG_UTC_GPIO,
	IPC_DEBUG_UTC_SPI,
	IPC_DEBUG_UTC_CMU,
	IPC_DEBUG_UTC_TIME_SYNC,
	IPC_DEBUG_UTC_ASSERT, /* 10 */
	IPC_DEBUG_UTC_FAULT,
	IPC_DEBUG_UTC_CHECK_STATUS,
	IPC_DEBUG_UTC_CHECK_CPU_UTIL,
	IPC_DEBUG_UTC_HEAP_DEBUG,
	IPC_DEBUG_UTC_HANG,
	IPC_DEBUG_UTC_HANG_ITMON,
	IPC_DEBUG_UTC_SENSOR_CHIPID, /* ap request */
	IPC_DEBUG_UTC_IPC_TEST_START,
	IPC_DEBUG_UTC_IPC_TEST_END,
	IPC_DEBUG_DUMP_STATUS,
	IPC_DEBUG_UTC_MAX,
	IPC_DEBUG_NANOHUB_MAX,
	IPC_DEBUG_CHUB_PRINT_LOG, /* chub request */
	IPC_DEBUG_CHUB_FULL_LOG,
	IPC_DEBUG_CHUB_FAULT,
	IPC_DEBUG_CHUB_ASSERT,
	IPC_DEBUG_CHUB_ERROR,
};

enum ipc_region {
	IPC_REG_BL,
	IPC_REG_BL_MAP,
	IPC_REG_OS,
	IPC_REG_IPC,
	IPC_REG_IPC_EVT_A2C,
	IPC_REG_IPC_EVT_A2C_CTRL,
	IPC_REG_IPC_EVT_C2A,
	IPC_REG_IPC_EVT_C2A_CTRL,
	IPC_REG_IPC_A2C,
	IPC_REG_IPC_C2A,
	IPC_REG_SHARED,
	IPC_REG_RAM,
	IPC_REG_LOG,
	IPC_REG_PERSISTBUF,
	IPC_REG_DUMP,
	IPC_REG_MAX,
};

struct ipc_area {
	void *base;
	u32 offset;
};

enum ipc_owner {
	AP,
#if defined(CHUB_IPC)
	APM,
	CP,
	GNSS,
#endif
	IPC_OWN_MAX
};

enum ipc_data_list {
	IPC_DATA_C2A,
	IPC_DATA_A2C,
	IPC_DATA_MAX,
};

enum ipc_evt_list {
	IPC_EVT_C2A,
	IPC_EVT_A2C,
	IPC_EVT_AP_MAX,
	IPC_EVT_MAX = IPC_EVT_AP_MAX
};

enum ipc_packet {
	IPC_ALIVE_HELLO = 0xab,
	IPC_ALIVE_OK = 0xcd,
};

enum ipc_direction {
	IPC_DST,
	IPC_SRC,
};

/* channel status define
 * IDLE_A2C:	100
 * AP_WRITE :		110
 * CHUB_RECV:		101
 * IDLE_C2A:	000
 * CHUB_WRITE:		010
 * AP_RECV:		001
 */
#define CS_OWN_OFFSET (3)
#define CS_AP (0x1)
#define CS_CHUB (0x0)
#define CS_AP_OWN (CS_AP << CS_OWN_OFFSET)
#define CS_CHUB_OWN (CS_CHUB << CS_OWN_OFFSET)
#define CS_WRITE (0x2)
#define CS_RECV (0x1)
#define CS_IPC_REG_CMP (0x3)

enum channel_status {
#ifdef AP_IPC
	CS_IDLE = CS_AP_OWN,
#else
	CS_IDLE = CS_CHUB_OWN,
#endif
	CS_AP_WRITE = CS_AP_OWN | CS_WRITE,
	CS_CHUB_RECV = CS_AP_OWN | CS_RECV,
	CS_CHUB_WRITE = CS_CHUB_OWN | CS_WRITE,
	CS_AP_RECV = CS_CHUB_OWN | CS_RECV,
	CS_MAX = 0xf
};

#define INVAL_CHANNEL (-1)

#if defined(AP_IPC) || defined(EMBOS)
#define HOSTINTF_SENSOR_DATA_MAX    240
#endif

/* event structure */
struct ipc_evt_ctrl {
	u32 eq;
	u32 dq;
	u32 full;
	u32 empty;
	u32 irq;
};

struct ipc_evt_buf {
	u32 evt;
	u32 irq;
	u32 status;
};

struct ipc_evt {
	struct ipc_evt_buf data[IPC_EVT_NUM];
	struct ipc_evt_ctrl ctrl;
};

/* it's from struct HostIntfDataBuffer buf */
struct ipc_log_content {
	u8 pad0;
	u8 length;
	u16 pad1;
	u8 buffer[sizeof(u64) + HOSTINTF_SENSOR_DATA_MAX - sizeof(u32)];
};

struct ipc_logbuf {
	u32 eq;	/* write owner chub (index_writer) */
	u32 dq;	/* read onwer ap (index_reader) */
	u32 size;
	u32 token;
	u32 full;
	char buf[0];
};

#ifndef IPC_DATA_SIZE
#define IPC_DATA_SIZE (4096)
#endif

struct ipc_channel_buf {
	u32 size;
	u8 buf[PACKET_SIZE_MAX];
};

#define IPC_CH_BUF_NUM (16)
struct ipc_buf {
	volatile u32 eq;
	volatile u32 dq;
	volatile u32 full;
	volatile u32 empty;
	struct ipc_channel_buf ch[IPC_CH_BUF_NUM];
};

struct ipc_debug {
	u32 event;
	u32 val[IPC_DATA_MAX];
};

struct ipc_map_area {
	struct ipc_buf data[IPC_DATA_MAX];
	struct ipc_evt evt[IPC_EVT_MAX];
	struct ipc_debug dbg;
	struct ipc_logbuf logbuf;
};

struct mailbox_sfr {
            unsigned int MCUCTL;
            unsigned int INTGR0;
            unsigned int INTCR0;
            unsigned int INTMR0;
            unsigned int INTSR0;
            unsigned int INTMSR0;
            unsigned int INTGR1;
            unsigned int INTCR1;
            unsigned int INTMR1;
            unsigned int INTSR1;
            unsigned int INTMSR1;
};

/*  mailbox Registers */
#define REG_MAILBOX_MCUCTL (0x000)
#define REG_MAILBOX_INTGR0 (0x008)
#define REG_MAILBOX_INTCR0 (0x00C)
#define REG_MAILBOX_INTMR0 (0x010)
#define REG_MAILBOX_INTSR0 (0x014)
#define REG_MAILBOX_INTMSR0 (0x018)
#define REG_MAILBOX_INTGR1 (0x01C)
#define REG_MAILBOX_INTCR1 (0x020)
#define REG_MAILBOX_INTMR1 (0x024)
#define REG_MAILBOX_INTSR1 (0x028)
#define REG_MAILBOX_INTMSR1 (0x02C)

#if defined(AP_IPC)
#if defined(CONFIG_SOC_EXYNOS9810)
#define REG_MAILBOX_VERSION (0x050)
#elif defined(CONFIG_SOC_EXYNOS9610)
#define REG_MAILBOX_VERSION (0x070)
#else
//
//Need to check !!!
//
#define REG_MAILBOX_VERSION (0x0)
#endif
#endif

#define REG_MAILBOX_ISSR0 (0x080)
#define REG_MAILBOX_ISSR1 (0x084)
#define REG_MAILBOX_ISSR2 (0x088)
#define REG_MAILBOX_ISSR3 (0x08C)

#define IPC_HW_READ_STATUS(base) \
	__raw_readl((base) + REG_MAILBOX_INTSR0)
#define IPC_HW_READ_STATUS1(base) \
	__raw_readl((base) + REG_MAILBOX_INTSR1)
#define IPC_HW_READ_PEND(base, irq) \
	(__raw_readl((base) + REG_MAILBOX_INTSR1) & (1 << (irq)))
#define IPC_HW_CLEAR_PEND(base, irq) \
	__raw_writel(1 << (irq), (base) + REG_MAILBOX_INTCR0)
#define IPC_HW_CLEAR_PEND1(base, irq) \
	__raw_writel(1 << (irq), (base) + REG_MAILBOX_INTCR1)
#define IPC_HW_WRITE_SHARED_REG(base, num, data) \
	__raw_writel((data), (base) + REG_MAILBOX_ISSR0 + (num) * 4)
#define IPC_HW_READ_SHARED_REG(base, num) \
	__raw_readl((base) + REG_MAILBOX_ISSR0 + (num) * 4)
#define IPC_HW_GEN_INTERRUPT_GR1(base, num) \
	__raw_writel(1 << (num), (base) + REG_MAILBOX_INTGR1)
#define IPC_HW_GEN_INTERRUPT_GR0(base, num) \
	__raw_writel(1 << ((num) + 16), (base) + REG_MAILBOX_INTGR0)
#define IPC_HW_SET_MCUCTL(base, val) \
	__raw_write32((val), (base) + REG_MAILBOX_MCUCTL)

/* channel ctrl functions */
void ipc_print_channel(void);
int ipc_check_valid(void);
char *ipc_get_cs_name(enum channel_status cs);
void ipc_set_base(void *addr);
void *ipc_get_base(enum ipc_region area);
u32 ipc_get_offset(enum ipc_region area);
void *ipc_get_addr(enum ipc_region area, int buf_num);
int ipc_check_reset_valid(void);
void ipc_init(void);
int ipc_hw_read_int_start_index(enum ipc_owner owner);
/* logbuf functions */
void *ipc_get_logbuf(void);
unsigned int ipc_logbuf_get_token(void);
/* evt functions */
struct ipc_evt_buf *ipc_get_evt(enum ipc_evt_list evt);
int ipc_add_evt(enum ipc_evt_list evt, enum irq_evt_chub irq);
void ipc_print_evt(enum ipc_evt_list evt);
/* mailbox hw access */
void ipc_set_owner(enum ipc_owner owner, void *base, enum ipc_direction dir);
unsigned int ipc_hw_read_gen_int_status_reg(enum ipc_owner owner, int irq);
void ipc_hw_write_shared_reg(enum ipc_owner owner, unsigned int val, int num);
unsigned int ipc_hw_read_shared_reg(enum ipc_owner owner, int num);
unsigned int ipc_hw_read_int_status_reg(enum ipc_owner owner);
unsigned int ipc_hw_read_int_gen_reg(enum ipc_owner owner);
void ipc_hw_clear_int_pend_reg(enum ipc_owner owner, int irq);
void ipc_hw_clear_all_int_pend_reg(enum ipc_owner owner);
void ipc_hw_gen_interrupt(enum ipc_owner owner, int irq);
void ipc_hw_set_mcuctrl(enum ipc_owner owner, unsigned int val);
void ipc_hw_mask_all(enum ipc_owner owner, bool mask);
void ipc_dump_mailbox_sfr(struct mailbox_sfr *mailbox);
void ipc_hw_mask_irq(enum ipc_owner owner, int irq);
void ipc_hw_unmask_irq(enum ipc_owner owner, int irq);
void ipc_logbuf_put_with_char(char ch);
int ipc_logbuf_need_flush(void);
void ipc_write_debug_event(enum ipc_owner owner, enum ipc_debug_event action);
u32 ipc_read_debug_event(enum ipc_owner owner);
void ipc_write_debug_val(enum ipc_data_list dir, u32 val);
u32 ipc_read_debug_val(enum ipc_data_list dir);

void *ipc_get_chub_map(void);
u32 ipc_get_chub_mem_size(void);
u64 ipc_read_val(enum ipc_owner owner);
void ipc_write_val(enum ipc_owner owner, u64 result);
void ipc_set_chub_clk(u32 clk);
u32 ipc_get_chub_clk(void);
void ipc_set_chub_bootmode(u16 bootmode);
u16 ipc_get_chub_bootmode(void);
void ipc_set_chub_kernel_log(u16 kernel_log);
u16 ipc_get_chub_kernel_log(void);
void ipc_dump(void);
#if defined(LOCAL_POWERGATE)
u32 *ipc_get_chub_psp(void);
u32 *ipc_get_chub_msp(void);
#endif

void *ipc_read_data(enum ipc_data_list dir, u32 *len);
int ipc_write_data(enum ipc_data_list dir, void *tx, u16 length);

#endif
