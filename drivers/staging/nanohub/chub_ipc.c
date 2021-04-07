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

#include "chub_ipc.h"

#if defined(CHUB_IPC)
#if defined(SEOS)
#include <seos.h>
#include <errno.h>
#include <cmsis.h>
#elif defined(EMBOS)
#include <Device.h>
#define EINVAL 22
#endif
#include <mailboxDrv.h>
#include <csp_common.h>
#include <csp_printf.h>
#include <string.h>
#include <string.h>
#elif defined(AP_IPC)
#include <linux/delay.h>
#include <linux/io.h>
#include "chub.h"
#endif

/* ap-chub ipc */
struct ipc_area ipc_addr[IPC_REG_MAX];

struct ipc_owner_ctrl {
	enum ipc_direction src;
	void *base;
} ipc_own[IPC_OWN_MAX];

struct ipc_map_area *ipc_map;

#define NAME_PREFIX "nanohub-ipc"

#ifdef PACKET_LOW_DEBUG
#define GET_IPC_REG_STRING(a) (((a) == IPC_REG_IPC_C2A) ? "wt" : "rd")

static char *get_cs_name(enum channel_status cs)
{
	switch (cs) {
	case CS_IDLE:
		return "I";
	case CS_AP_WRITE:
		return "AW";
	case CS_CHUB_RECV:
		return "CR";
	case CS_CHUB_WRITE:
		return "CW";
	case CS_AP_RECV:
		return "AR";
	case CS_MAX:
		break;
	};
	return NULL;
}

void content_disassemble(struct ipc_content *content, enum ipc_region act)
{
	CSP_PRINTF_INFO("[content-%s-%d: status:%s: buf: 0x%x, size: %d]\n",
			GET_IPC_REG_STRING(act), content->num,
			get_cs_name(content->status),
			(unsigned int)content->buf, content->size);
}
#endif

/* ipc address control functions */
void ipc_set_base(void *addr)
{
	ipc_addr[IPC_REG_BL].base = addr;
}

inline void *ipc_get_base(enum ipc_region area)
{
	return ipc_addr[area].base;
}

inline u32 ipc_get_offset(enum ipc_region area)
{
	return ipc_addr[area].offset;
}

inline void *ipc_get_addr(enum ipc_region area, int buf_num)
{
#ifdef CHUB_IPC
	return (void *)((unsigned int)ipc_addr[area].base +
			ipc_addr[area].offset * buf_num);
#else
	return ipc_addr[area].base + ipc_addr[area].offset * buf_num;
#endif
}

u32 ipc_get_chub_mem_size(void)
{
	return ipc_addr[IPC_REG_DUMP].offset;
}

void ipc_set_chub_clk(u32 clk)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->chubclk = clk;
}

u32 ipc_get_chub_clk(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->chubclk;
}

void ipc_set_chub_bootmode(u16 bootmode)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->bootmode = bootmode;
}

u16 ipc_get_chub_bootmode(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->bootmode;
}

void ipc_set_chub_kernel_log(u16 kernel_log)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->kernel_log = kernel_log;
}

u16 ipc_get_chub_kernel_log(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->kernel_log;
}

#if defined(LOCAL_POWERGATE)
u32 *ipc_get_chub_psp(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return &(map->psp);
}

u32 *ipc_get_chub_msp(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return &(map->msp);
}
#endif

void *ipc_get_chub_map(void)
{
	char *sram_base = ipc_get_base(IPC_REG_BL);
	struct chub_bootargs *map = (struct chub_bootargs *)(sram_base + MAP_INFO_OFFSET);

	if (strncmp(OS_UPDT_MAGIC, map->magic, sizeof(OS_UPDT_MAGIC))) {
		CSP_PRINTF_ERROR("%s: %s: %p has wrong magic key: %s -> %s\n",
			NAME_PREFIX, __func__, map, OS_UPDT_MAGIC, map->magic);
		return 0;
	}

	if (map->ipc_version != IPC_VERSION) {
		CSP_PRINTF_ERROR
		    ("%s: %s: ipc_version doesn't match: AP %d, Chub: %d\n",
		     NAME_PREFIX, __func__, IPC_VERSION, map->ipc_version);
		return 0;
	}

	if (sizeof(struct chub_bootargs) > MAP_INFO_MAX_SIZE) {
		CSP_PRINTF_ERROR
		    ("%s: %s: map size bigger than max %d > %d", NAME_PREFIX, __func__,
		    sizeof(struct chub_bootargs), MAP_INFO_MAX_SIZE);
		return 0;
	}

	ipc_addr[IPC_REG_BL_MAP].base = map;
	ipc_addr[IPC_REG_OS].base = sram_base + map->code_start;
	ipc_addr[IPC_REG_SHARED].base = sram_base + map->shared_start;
	ipc_addr[IPC_REG_IPC].base = sram_base + map->ipc_start;
	ipc_addr[IPC_REG_RAM].base = sram_base + map->ram_start;
	ipc_addr[IPC_REG_DUMP].base = sram_base + map->dump_start;
	ipc_addr[IPC_REG_BL].offset = map->bl_end - map->bl_start;
	ipc_addr[IPC_REG_OS].offset = map->code_end - map->code_start;
	ipc_addr[IPC_REG_SHARED].offset = map->shared_end - map->shared_start;
	ipc_addr[IPC_REG_IPC].offset = map->ipc_end - map->ipc_start;
	ipc_addr[IPC_REG_RAM].offset = map->ram_end - map->ram_start;
	ipc_addr[IPC_REG_DUMP].offset = map->dump_end - map->dump_start;

	ipc_map = ipc_addr[IPC_REG_IPC].base;
	ipc_map->logbuf.size =
	    ipc_addr[IPC_REG_IPC].offset - sizeof(struct ipc_map_area) - CHUB_PERSISTBUF_SIZE;

	ipc_addr[IPC_REG_IPC_EVT_A2C].base = &ipc_map->evt[IPC_EVT_A2C].data;
	ipc_addr[IPC_REG_IPC_EVT_A2C].offset = sizeof(struct ipc_evt);
	ipc_addr[IPC_REG_IPC_EVT_A2C_CTRL].base =
	    &ipc_map->evt[IPC_EVT_A2C].ctrl;
	ipc_addr[IPC_REG_IPC_EVT_A2C_CTRL].offset = 0;
	ipc_addr[IPC_REG_IPC_EVT_C2A].base = &ipc_map->evt[IPC_EVT_C2A].data;
	ipc_addr[IPC_REG_IPC_EVT_C2A].offset = sizeof(struct ipc_evt);
	ipc_addr[IPC_REG_IPC_EVT_C2A_CTRL].base =
	    &ipc_map->evt[IPC_EVT_C2A].ctrl;
	ipc_addr[IPC_REG_IPC_EVT_C2A_CTRL].offset = 0;
	ipc_addr[IPC_REG_IPC_C2A].base = &ipc_map->data[IPC_DATA_C2A];
	ipc_addr[IPC_REG_IPC_A2C].base = &ipc_map->data[IPC_DATA_A2C];
	ipc_addr[IPC_REG_IPC_C2A].offset = sizeof(struct ipc_buf);
	ipc_addr[IPC_REG_IPC_A2C].offset = sizeof(struct ipc_buf);

	ipc_addr[IPC_REG_LOG].base = &ipc_map->logbuf.buf;
	ipc_addr[IPC_REG_LOG].offset = ipc_map->logbuf.size;
	ipc_addr[IPC_REG_PERSISTBUF].base = ipc_addr[IPC_REG_LOG].base + ipc_addr[IPC_REG_LOG].offset;
	ipc_addr[IPC_REG_PERSISTBUF].offset = CHUB_PERSISTBUF_SIZE;

	if (((u32)ipc_addr[IPC_REG_PERSISTBUF].base + ipc_addr[IPC_REG_PERSISTBUF].offset) >
			((u32)ipc_addr[IPC_REG_IPC].base + ipc_addr[IPC_REG_IPC].offset))
		CSP_PRINTF_INFO("%s: %s: wrong persistbuf addr:%p, %d, ipc_end:0x%x\n",
			NAME_PREFIX, __func__,
			ipc_addr[IPC_REG_PERSISTBUF].base, ipc_addr[IPC_REG_PERSISTBUF].offset, map->ipc_end);

	CSP_PRINTF_INFO
	    ("%s: contexthub map information(v%u)\n bl(%p %d)\n os(%p %d)\n ipc(%p %d)\n ram(%p %d)\n shared(%p %d)\n dump(%p %d)\n",
	     NAME_PREFIX, map->ipc_version,
	     ipc_addr[IPC_REG_BL].base, ipc_addr[IPC_REG_BL].offset,
	     ipc_addr[IPC_REG_OS].base, ipc_addr[IPC_REG_OS].offset,
	     ipc_addr[IPC_REG_IPC].base, ipc_addr[IPC_REG_IPC].offset,
	     ipc_addr[IPC_REG_RAM].base, ipc_addr[IPC_REG_RAM].offset,
	     ipc_addr[IPC_REG_SHARED].base, ipc_addr[IPC_REG_SHARED].offset,
	     ipc_addr[IPC_REG_DUMP].base, ipc_addr[IPC_REG_DUMP].offset);

	CSP_PRINTF_INFO
		("%s: ipc_map information\n ipc(%p %d)\n data_c2a(%p %d)\n data_a2c(%p %d)\n evt_c2a(%p %d)\n evt_a2c(%p %d)\n log(%p %d)\n persistbuf(%p %d)\n",
		NAME_PREFIX, ipc_get_base(IPC_REG_IPC), ipc_get_offset(IPC_REG_IPC),
		ipc_get_base(IPC_REG_IPC_C2A), ipc_get_offset(IPC_REG_IPC_C2A),
		ipc_get_base(IPC_REG_IPC_A2C), ipc_get_offset(IPC_REG_IPC_A2C),
		ipc_get_base(IPC_REG_IPC_EVT_C2A), ipc_get_offset(IPC_REG_IPC_EVT_C2A),
		ipc_get_base(IPC_REG_IPC_EVT_A2C), ipc_get_offset(IPC_REG_IPC_EVT_A2C),
		ipc_get_base(IPC_REG_LOG), ipc_get_offset(IPC_REG_LOG),
		ipc_get_base(IPC_REG_PERSISTBUF), ipc_get_offset(IPC_REG_PERSISTBUF));

	CSP_PRINTF_INFO
		("%s: ipc_map data_ch:size:%d on %d channel. evt_ch:%d\n",
			NAME_PREFIX, PACKET_SIZE_MAX, IPC_CH_BUF_NUM, IPC_EVT_NUM);
#ifdef SEOS
	if (PACKET_SIZE_MAX < NANOHUB_PACKET_SIZE_MAX)
		CSP_PRINTF_ERROR("%s: %d should be bigger than %d\n", NAME_PREFIX, PACKET_SIZE_MAX, NANOHUB_PACKET_SIZE_MAX);
#endif

	return ipc_map;
}

#ifdef CHUB_IPC
#define DISABLE_IRQ() __disable_irq();
#define ENABLE_IRQ() __enable_irq();
static inline void busywait(u32 ms)
{
	msleep(ms);
}
#else /* AP IPC doesn't need it */
#define DISABLE_IRQ() do {} while(0)
#define ENABLE_IRQ() do {} while(0)
static inline void busywait(u32 ms)
{
	(void)ms;
	cpu_relax();
}
#endif

static inline bool __ipc_queue_empty(struct ipc_buf *ipc_data)
{
	return (__raw_readl(&ipc_data->eq) == ipc_data->dq);
}

static inline bool __ipc_queue_full(struct ipc_buf *ipc_data)
{
	return (((ipc_data->eq + 1) % IPC_CH_BUF_NUM) == __raw_readl(&ipc_data->dq));
}

static inline bool __ipc_queue_index_check(struct ipc_buf *ipc_data)
{
	return ((ipc_data->eq >= IPC_CH_BUF_NUM) || (ipc_data->dq >= IPC_CH_BUF_NUM));
}


int ipc_write_data(enum ipc_data_list dir, void *tx, u16 length)
{
	int ret = 0;
	enum ipc_region reg = (dir == IPC_DATA_C2A) ? IPC_REG_IPC_C2A : IPC_REG_IPC_A2C;
	struct ipc_buf *ipc_data = ipc_get_base(reg);

	if (length <= PACKET_SIZE_MAX) {
		if (__ipc_queue_index_check(ipc_data)) {
			CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
			return -EINVAL;
		}

		DISABLE_IRQ();
		if (!__ipc_queue_full(ipc_data)) {
			struct ipc_channel_buf *ipc;

			ipc = &ipc_data->ch[ipc_data->eq];
			ipc->size = length;
#ifdef AP_IPC
			memcpy_toio(ipc->buf, tx, length);
#else
			memcpy(ipc->buf, tx, length);
#endif
			ipc_data->eq = (ipc_data->eq + 1) % IPC_CH_BUF_NUM;
		} else {
			CSP_PRINTF_INFO("%s: %s: is full\n", NAME_PREFIX, __func__);
			ret = -EINVAL;
		}
		ENABLE_IRQ();
	} else {
		CSP_PRINTF_INFO("%s: %s: invalid size:%d\n",
			NAME_PREFIX, __func__, length);
		ret = -EINVAL;
	}

	if (!ret) {
		enum ipc_evt_list evtq = (dir == IPC_DATA_C2A) ? IPC_EVT_C2A : IPC_EVT_A2C;

		ret = ipc_add_evt(evtq, IRQ_EVT_CH0);
		if (ret)
			CSP_PRINTF_INFO("%s: %s: fail by add_evt\n",
			NAME_PREFIX, __func__);
	} else {
		CSP_PRINTF_INFO("%s: %s: error: eq:%d, dq:%d\n",
			NAME_PREFIX, __func__, ipc_data->eq, ipc_data->dq);
		ipc_dump();
	}
	return ret;
}

void *ipc_read_data(enum ipc_data_list dir, u32 *len)
{
	enum ipc_region reg = (dir == IPC_DATA_C2A) ? IPC_REG_IPC_C2A : IPC_REG_IPC_A2C;
	struct ipc_buf *ipc_data = ipc_get_base(reg);
	void *buf = NULL;

	if (__ipc_queue_index_check(ipc_data)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return NULL;
	}

	DISABLE_IRQ();
	if (!__ipc_queue_empty(ipc_data)) {
		struct ipc_channel_buf *ipc;

		ipc = &ipc_data->ch[ipc_data->dq];
		*len = ipc->size;
		ipc_data->dq = (ipc_data->dq + 1) % IPC_CH_BUF_NUM;
		buf = ipc->buf;
	}
	ENABLE_IRQ();
	return buf;
}

static void ipc_print_databuf(void)
{
	struct ipc_buf *ipc_data = ipc_get_base(IPC_REG_IPC_A2C);

	CSP_PRINTF_INFO("%s: a2c: eq:%d dq:%d full:%d empty:%d\n",
		NAME_PREFIX, ipc_data->eq, ipc_data->dq, ipc_data->full, ipc_data->empty);

	ipc_data = ipc_get_base(IPC_REG_IPC_C2A);

	CSP_PRINTF_INFO("%s: c2a: eq:%d dq:%d full:%d empty:%d\n",
		NAME_PREFIX, ipc_data->eq, ipc_data->dq, ipc_data->full, ipc_data->empty);
}

static void ipc_print_logbuf(void)
{
	struct ipc_logbuf *logbuf = &ipc_map->logbuf;

	CSP_PRINTF_INFO("%s: token:%d, eq:%d, dq:%d, size:%d, full:%d\n",
		NAME_PREFIX, logbuf->token, logbuf->eq, logbuf->dq, logbuf->size, logbuf->full);
}

int ipc_check_reset_valid()
{
	int i;
	int ret = 0;
	struct ipc_map_area *map = ipc_get_base(IPC_REG_IPC);

 	for (i = 0; i < IPC_DATA_MAX; i++)
		if (map->data[i].dq || map->data[i].eq ||
			map->data[i].full || (map->data[i].empty != 1)) {
			CSP_PRINTF_INFO("%s: %s: ipc_data_%s invalid: eq:%d, dq:%d, full:%d, empty:%d\n",
			NAME_PREFIX, __func__, i ? "a2c" : "c2a",
			map->data[i].eq,
			map->data[i].dq,
			map->data[i].full,
			map->data[i].empty);
			ret = -EINVAL;
		}

 	for (i = 0; i < IPC_EVT_MAX; i++)
		if (map->evt[i].ctrl.eq || map->evt[i].ctrl.dq ||
			map->evt[i].ctrl.full || (map->evt[i].ctrl.empty != 1)) {
			CSP_PRINTF_INFO("%s: %s: ipc_evt_%s invalid: eq:%d, dq:%d, full:%d, empty:%d\n",
			NAME_PREFIX, __func__, i ? "a2c" : "c2a",
			map->evt[i].ctrl.eq,
			map->evt[i].ctrl.eq,
			map->evt[i].ctrl.full,
			map->evt[i].ctrl.empty);
			ret = -EINVAL;
		}

	return ret;
}

void ipc_init(void)
{
	int i, j;

	if (!ipc_map)
		CSP_PRINTF_ERROR("%s: ipc_map is NULL.\n", __func__);

	for (i = 0; i < IPC_DATA_MAX; i++) {
		ipc_map->data[i].eq = 0;
		ipc_map->data[i].dq = 0;
		ipc_map->data[i].full = 0;
		ipc_map->data[i].empty = 1;
	}

	ipc_hw_clear_all_int_pend_reg(AP);

	for (j = 0; j < IPC_EVT_MAX; j++) {
		ipc_map->evt[j].ctrl.dq = 0;
		ipc_map->evt[j].ctrl.eq = 0;
		ipc_map->evt[j].ctrl.full = 0;
		ipc_map->evt[j].ctrl.empty = 1;
		ipc_map->evt[j].ctrl.irq = 0;

		for (i = 0; i < IPC_EVT_NUM; i++) {
			ipc_map->evt[j].data[i].evt = IRQ_EVT_INVAL;
			ipc_map->evt[j].data[i].irq = IRQ_EVT_INVAL;
		}
	}
}

/* evt functions */
enum {
	IPC_EVT_DQ,		/* empty */
	IPC_EVT_EQ,		/* fill */
};

#define EVT_Q_INT(i) (((i) == IPC_EVT_NUM) ? 0 : (i))
#define IRQ_EVT_IDX_INT(i) (((i) == IRQ_EVT_END) ? IRQ_EVT_START : (i))

static inline bool __ipc_evt_queue_empty(struct ipc_evt_ctrl *ipc_evt)
{
	return (ipc_evt->eq == ipc_evt->dq);
}

static inline bool __ipc_evt_queue_full(struct ipc_evt_ctrl *ipc_evt)
{
	return (((ipc_evt->eq + 1) % IPC_EVT_NUM) == ipc_evt->dq);
}

static inline bool __ipc_evt_queue_index_check(struct ipc_evt_ctrl *ipc_evt)
{
	return ((ipc_evt->eq >= IPC_EVT_NUM) || (ipc_evt->dq >= IPC_EVT_NUM));
}

struct ipc_evt_buf *ipc_get_evt(enum ipc_evt_list evtq)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
	struct ipc_evt_buf *cur_evt = NULL;

	if (__ipc_evt_queue_index_check(&ipc_evt->ctrl)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return NULL;
	}

	DISABLE_IRQ();
	if (!__ipc_evt_queue_empty(&ipc_evt->ctrl)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.dq];
		cur_evt->status = IPC_EVT_DQ;
		ipc_evt->ctrl.dq = (ipc_evt->ctrl.dq + 1) % IPC_EVT_NUM;
	} else if (__raw_readl(&ipc_evt->ctrl.full)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.dq];
		cur_evt->status = IPC_EVT_DQ;
		ipc_evt->ctrl.dq = (ipc_evt->ctrl.dq + 1) % IPC_EVT_NUM;
		__raw_writel(0, &ipc_evt->ctrl.full);
	}
	ENABLE_IRQ();

	return cur_evt;
}

#define EVT_WAIT_TIME (5)
#define MAX_TRY_CNT (5)

int ipc_add_evt(enum ipc_evt_list evtq, enum irq_evt_chub evt)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
	enum ipc_owner owner = (evtq < IPC_EVT_AP_MAX) ? AP : IPC_OWN_MAX;
	struct ipc_evt_buf *cur_evt = NULL;
	int trycnt = 0;
	u32 pending;

	if (!ipc_evt || (owner != AP)) {
		CSP_PRINTF_ERROR("%s: %s: invalid ipc_evt, owner:%d\n", NAME_PREFIX, __func__, owner);
		return -1;
	}

retry:
	if (__ipc_evt_queue_index_check(&ipc_evt->ctrl)) {
		CSP_PRINTF_ERROR("%s:%s: failed by ipc index corrupt\n", NAME_PREFIX, __func__);
		return -EINVAL;
	}

	DISABLE_IRQ();
	if (!__raw_readl(&ipc_evt->ctrl.full)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.eq];
		if (!cur_evt) {
			CSP_PRINTF_ERROR("%s: invalid cur_evt\n", __func__);
			ENABLE_IRQ();
			return -1;
		}

		/* wait pending clear on irq pend */
		pending = ipc_hw_read_gen_int_status_reg(AP, ipc_evt->ctrl.irq);
		if (pending) {
			CSP_PRINTF_ERROR("%s: %s: irq:%d pending:0x%x->0x%x\n",
				NAME_PREFIX, __func__, ipc_evt->ctrl.irq, pending, ipc_hw_read_int_status_reg(AP));
            /* don't sleep on ap */
			do {
				busywait(EVT_WAIT_TIME);
			} while (ipc_hw_read_gen_int_status_reg(AP, ipc_evt->ctrl.irq) && (trycnt++ < MAX_TRY_CNT));
			CSP_PRINTF_INFO("%s: %s: pending irq wait: pend:%d irq %d during %d times\n",
				NAME_PREFIX, __func__, ipc_hw_read_gen_int_status_reg(AP, ipc_evt->ctrl.irq),
				ipc_evt->ctrl.irq, trycnt);

			if (ipc_hw_read_gen_int_status_reg(AP, ipc_evt->ctrl.irq)) {
				CSP_PRINTF_ERROR("%s: %s: fail to add evt by pending:0x%x\n",
					NAME_PREFIX, __func__, ipc_hw_read_gen_int_status_reg(AP, ipc_evt->ctrl.irq));
				ENABLE_IRQ();
				return -1;
			}
		}
		cur_evt->evt = evt;
		cur_evt->status = IPC_EVT_EQ;
		cur_evt->irq = ipc_evt->ctrl.irq;
		ipc_evt->ctrl.eq = EVT_Q_INT(ipc_evt->ctrl.eq + 1);
		ipc_evt->ctrl.eq = (ipc_evt->ctrl.eq + 1) % IPC_EVT_NUM;
		if (ipc_evt->ctrl.eq == __raw_readl(&ipc_evt->ctrl.dq))
			__raw_writel(1, &ipc_evt->ctrl.full);
	} else {
		ENABLE_IRQ();
		do {
			busywait(EVT_WAIT_TIME);
		} while (ipc_evt->ctrl.full && (trycnt++ < MAX_TRY_CNT));

		if (!__raw_readl(&ipc_evt->ctrl.full)) {
			CSP_PRINTF_INFO("%s: %s: evt %d during %d ms is full\n",
					NAME_PREFIX, __func__, evt, EVT_WAIT_TIME * trycnt);
			goto retry;
		} else {
			CSP_PRINTF_ERROR("%s: %s: fail to add evt by full\n", NAME_PREFIX, __func__);
			ipc_dump();
			return -1;
		}
	}
	ENABLE_IRQ();

	if (owner != IPC_OWN_MAX) {
#if defined(AP_IPC)
		ipc_write_val(AP, sched_clock());
#endif
		if (cur_evt)
			ipc_hw_gen_interrupt(owner, cur_evt->irq);
		else
			return -1;
	}
	return 0;
}

#define IPC_GET_EVT_NAME(a) (((a) == IPC_EVT_A2C) ? "A2C" : "C2A")

void ipc_print_evt(enum ipc_evt_list evtq)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
	int i;

	if (ipc_evt) {
		CSP_PRINTF_INFO("%s: evt(%p)-%s: eq:%d dq:%d full:%d irq:%d\n",
			NAME_PREFIX, ipc_evt, IPC_GET_EVT_NAME(evtq), ipc_evt->ctrl.eq,
			ipc_evt->ctrl.dq, ipc_evt->ctrl.full,
			ipc_evt->ctrl.irq);

		for (i = 0; i < IPC_EVT_NUM; i++) {
			CSP_PRINTF_INFO("%s: evt%d(evt:%d,irq:%d,f:%d)\n",
				NAME_PREFIX, i, ipc_evt->data[i].evt,
				ipc_evt->data[i].irq, ipc_evt->data[i].status);
		}
	} else
		CSP_PRINTF_INFO("%s:%s: invalid evtq\n", NAME_PREFIX, __func__);

}

void ipc_dump(void)
{
	CSP_PRINTF_INFO("%s: %s: a2x event\n", NAME_PREFIX, __func__);
	ipc_print_evt(IPC_EVT_A2C);
	CSP_PRINTF_INFO("%s: %s: c2a event\n", NAME_PREFIX, __func__);
	ipc_print_evt(IPC_EVT_C2A);
	CSP_PRINTF_INFO("%s: %s: data buffer\n", NAME_PREFIX, __func__);
	ipc_print_databuf();
	CSP_PRINTF_INFO("%s: %s: log buffer\n", NAME_PREFIX, __func__);
	ipc_print_logbuf();
}

u32 ipc_logbuf_get_token(void)
{
	__raw_writel(ipc_map->logbuf.token + 1, &ipc_map->logbuf.token);

	return __raw_readl(&ipc_map->logbuf.token);
}

void ipc_logbuf_put_with_char(char ch)
{
	char *logbuf;
	int eqNext;

	if (ipc_map) {
		eqNext = ipc_map->logbuf.eq + 1;

#ifdef IPC_DEBUG
		if (eqNext == ipc_map->logbuf.dq) {
			ipc_write_debug_event(AP, IPC_DEBUG_CHUB_FULL_LOG);
			ipc_add_evt(IPC_EVT_C2A, IRQ_EVT_CHUB_TO_AP_DEBUG);
		}
#endif
		ipc_map->logbuf.token++;
		logbuf = ipc_map->logbuf.buf;

		*(logbuf + ipc_map->logbuf.eq) = ch;

		if (eqNext == ipc_map->logbuf.size)
			ipc_map->logbuf.eq = 0;
		else
			ipc_map->logbuf.eq = eqNext;
	}
}

void ipc_set_owner(enum ipc_owner owner, void *base, enum ipc_direction dir)
{
	ipc_own[owner].base = base;
	ipc_own[owner].src = dir;
}

int ipc_hw_read_int_start_index(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return IRQ_EVT_CHUB_MAX;
	else
		return 0;
}

unsigned int ipc_hw_read_gen_int_status_reg(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base +
				   REG_MAILBOX_INTSR1) & (1 << irq);
	else
		return __raw_readl((char *)ipc_own[owner].base +
				   REG_MAILBOX_INTSR0) & (1 << (irq +
								IRQ_EVT_CHUB_MAX));
}

void ipc_hw_write_shared_reg(enum ipc_owner owner, unsigned int val, int num)
{
	__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_ISSR0 + num * 4);
}

unsigned int ipc_hw_read_shared_reg(enum ipc_owner owner, int num)
{
	return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_ISSR0 + num * 4);
}

unsigned int ipc_hw_read_int_status_reg(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR0);
	else
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR1);
}

unsigned int ipc_hw_read_int_gen_reg(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTGR0);
	else
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTGR1);
}

void ipc_hw_clear_int_pend_reg(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTCR0);
	else
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTCR1);
}

void ipc_hw_clear_all_int_pend_reg(enum ipc_owner owner)
{
	u32 val = 0xffff << ipc_hw_read_int_start_index(AP);
	/* hack: org u32 val = 0xff; */

	if (ipc_own[owner].src)
		__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_INTCR0);
	else
		__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_INTCR1);
}

void ipc_hw_gen_interrupt(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR1);
	else
		__raw_writel(1 << (irq + IRQ_EVT_CHUB_MAX),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR0);
}

void ipc_hw_set_mcuctrl(enum ipc_owner owner, unsigned int val)
{
	__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_MCUCTL);
}

void ipc_hw_mask_all(enum ipc_owner owner, bool mask)
{
	if (mask) {
		ipc_hw_clear_all_int_pend_reg(owner);
		__raw_writel(0xffff0000, (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(0xffff, (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	} else {
		__raw_writel(0x0, (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(0x0, (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	}
}

void ipc_dump_mailbox_sfr(struct mailbox_sfr *mailbox)
{
	mailbox->MCUCTL  = __raw_readl(ipc_own[AP].base + REG_MAILBOX_MCUCTL);
	mailbox->INTGR0	 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTGR0);
	mailbox->INTCR0	 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTCR0);
	mailbox->INTMR0	 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMR0);
	mailbox->INTSR0	 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTSR0);
	mailbox->INTMSR0 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMSR0);
	mailbox->INTGR1	 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTGR1);
	mailbox->INTCR1	 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTCR1);
	mailbox->INTMR1  = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMR1);
	mailbox->INTSR1	 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTSR1);
	mailbox->INTMSR1 = __raw_readl(ipc_own[AP].base + REG_MAILBOX_INTMSR1);

	pr_info("%s: 0x%x/ c2a: 0x%x 0x%x 0x%x 0x%x 0x%x/ a2c: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		__func__, mailbox->MCUCTL,
		mailbox->INTGR0, mailbox->INTCR0, mailbox->INTMR0, mailbox->INTSR0, mailbox->INTMSR0,
		mailbox->INTGR1, mailbox->INTCR1, mailbox->INTMR1, mailbox->INTSR1, mailbox->INTMSR1);
}

void ipc_hw_mask_irq(enum ipc_owner owner, int irq)
{
	int mask;

	if (ipc_own[owner].src) {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(mask | (1 << (irq + IRQ_EVT_CHUB_MAX)),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
	} else {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
		__raw_writel(mask | (1 << irq),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	}
}

void ipc_hw_unmask_irq(enum ipc_owner owner, int irq)
{
	int mask;

	if (ipc_own[owner].src) {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(mask & ~(1 << (irq + IRQ_EVT_CHUB_MAX)),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
	} else {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
		__raw_writel(mask & ~(1 << irq),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	}
}

void ipc_write_debug_event(enum ipc_owner owner, enum ipc_debug_event action)
{
	ipc_map->dbg.event = action;
}

u32 ipc_read_debug_event(enum ipc_owner owner)
{
	return ipc_map->dbg.event;
}

void ipc_write_debug_val(enum ipc_data_list dir, u32 val)
{
	ipc_map->dbg.val[dir] = val;
}

u32 ipc_read_debug_val(enum ipc_data_list dir)
{
	return ipc_map->dbg.val[dir];
}

void ipc_write_val(enum ipc_owner owner, u64 val)
{
	u32 low = val & 0xffffffff;
	u32 high = val >> 32;

	ipc_hw_write_shared_reg(owner, low, SR_DEBUG_VAL_LOW);
	ipc_hw_write_shared_reg(owner, high, SR_DEBUG_VAL_HIGH);
}

u64 ipc_read_val(enum ipc_owner owner)
{
	u32 low = ipc_hw_read_shared_reg(owner, SR_DEBUG_VAL_LOW);
	u64 high = ipc_hw_read_shared_reg(owner, SR_DEBUG_VAL_HIGH);
	u64 val = low | (high << 32);

	return val;
}
