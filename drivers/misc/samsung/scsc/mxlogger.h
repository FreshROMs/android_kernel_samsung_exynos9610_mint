/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/**
 * Maxwell mxlogger (Interface)
 *
 * Provides bi-directional communication between the firmware and the
 * host.
 *
 */

#ifndef __MX_LOGGER_H__
#define __MX_LOGGER_H__

#include <linux/types.h>
#include <scsc/scsc_mifram.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include <linux/jiffies.h>

#include "mxmgmt_transport_format.h"

/**
 * ___________________________________________________________________
 * |    Cmd       |    Arg       |   ...  payload (opt)  ...         |
 * -------------------------------------------------------------------
 * <-- uint8_t --><-- uint8_t --><-----  uint8_t[] buffer ----------->
 *
 */

#define	MXLOGGER_RINGS_TMO_US		200000

/* CMD/EVENTS */
#define MM_MXLOGGER_LOGGER_CMD			(0)
#define MM_MXLOGGER_DIRECTION_CMD		(1)
#define MM_MXLOGGER_CONFIG_CMD			(2)
#define MM_MXLOGGER_INITIALIZED_EVT		(3)
#define MM_MXLOGGER_SYNC_RECORD			(4)
#define	MM_MXLOGGER_STARTED_EVT			(5)
#define	MM_MXLOGGER_STOPPED_EVT			(6)
#define	MM_MXLOGGER_COLLECTION_FW_REQ_EVT	(7)

/* ARG - LOGGER */
#define MM_MXLOGGER_LOGGER_ENABLE		(0)
#define MM_MXLOGGER_LOGGER_DISABLE		(1)
#define MM_MXLOGGER_DISABLE_REASON_STOP		(0)
#define MM_MXLOGGER_DISABLE_REASON_COLLECTION	(1)

/* ARG - DIRECTION */
#define MM_MXLOGGER_DIRECTION_DRAM	(0)
#define MM_MXLOGGER_DIRECTION_HOST	(1)

/* ARG - CONFIG TABLE */
#define MM_MXLOGGER_CONFIG_BASE_ADDR	(0)

/* ARG - CONFIG TABLE */
#define MM_MXLOGGER_SYNC_INDEX		(0)

#define MM_MXLOGGER_PAYLOAD_SZ          (MXMGR_MESSAGE_PAYLOAD_SIZE - 2)

/* Exynos3830's reserved memory changed */
#if defined(CONFIG_SOC_EXYNOS3830) || defined(CONFIG_SOC_EXYNOS7885)
#define MXL_INTERNAL_RSV		(24 * 1024)
#define MXL_POOL_SZ			((4 * 1024 * 1024) - MXL_INTERNAL_RSV)
#define MXLOGGER_RSV_COMMON_SZ		(2 * 1024)
#define MXLOGGER_RSV_WLAN_SZ		(4 * 1024)
#define MXLOGGER_RSV_RADIO_SZ		(2 * 1024)
#else
#define MXL_POOL_SZ			(6 * 1024 * 1024)
#define MXLOGGER_RSV_COMMON_SZ		(4 * 1024)
#define MXLOGGER_RSV_WLAN_SZ		(2 * 1024 * 1024)
#define MXLOGGER_RSV_RADIO_SZ		(4 * 1024)
#endif

#define MXLOGGER_SYNC_SIZE		(10 * 1024)
#define MXLOGGER_IMP_SIZE		(102 * 1024)
#define MXLOGGER_RSV_BT_SZ		(4 * 1024)

#define MXLOGGER_TOTAL_FIX_BUF		(MXLOGGER_SYNC_SIZE + MXLOGGER_IMP_SIZE + \
					MXLOGGER_RSV_COMMON_SZ + MXLOGGER_RSV_BT_SZ + \
					MXLOGGER_RSV_WLAN_SZ + MXLOGGER_RSV_RADIO_SZ)

#define MXLOGGER_NON_FIX_BUF_ALIGN	32

#define MXLOGGER_MAGIG_NUMBER		0xcaba0401
#define MXLOGGER_MAJOR			0
#define MXLOGGER_MINOR			0

#define NUM_SYNC_RECORDS		256
#define SYNC_MASK			(NUM_SYNC_RECORDS - 1)

/* Shared memory Layout
 *
 * |-------------------------| CONFIG
 * |    CONFIG AREA          |
 * |    ...                  |
 * |    *bufs     	     |------|
 * |    ....                 |      |
 * |    ....                 |      |
 * |  --------------------   |<-----|
 * |                         |
 * |  loc | sz | state |info |---------------------|
 * |  loc | sz | state |info |---------------------|
 * |  loc | sz | state |info |---------------------|
 * |    ...                  |                     |
 * |-------------------------| Fixed size buffers  |
 * |     SYNC  BUFFER        |<--------------------|
 * |-------------------------| 			   |
 * |     IMPORTANT EVENTS    |<--------------------|
 * |-------------------------|                     |
 * |    Reserved COMMON      |<--------------------|
 * |-------------------------|
 * |    Reserved BT          |
 * |-------------------------|
 * |    Reserved WL          |
 * |-------------------------|
 * |    Reserved RADIO       |
 * |-------------------------| Not fixed size buffers
 * |         MXLOG           |
 * |-------------------------|
 * |         UDI             |
 * |-------------------------|
 * |  Future buffers (TBD)   |
 * |-------------------------|
 * |  Future buffers (TBD)   |
 * |-------------------------|
 */

enum mxlogger_buffers {
	MXLOGGER_FIRST_FIXED_SZ,
	MXLOGGER_SYNC = MXLOGGER_FIRST_FIXED_SZ,
	MXLOGGER_IMP,
	MXLOGGER_RESERVED_COMMON,
	MXLOGGER_RESERVED_BT,
	MXLOGGER_RESERVED_WLAN,
	MXLOGGER_RESERVED_RADIO,
	MXLOGGER_LAST_FIXED_SZ = MXLOGGER_RESERVED_RADIO,
	MXLOGGER_MXLOG,
	MXLOGGER_UDI,
	MXLOGGER_NUM_BUFFERS
};

enum mxlogger_sync_event {
	MXLOGGER_SYN_SUSPEND,
	MXLOGGER_SYN_RESUME,
	MXLOGGER_SYN_TOHOST,
	MXLOGGER_SYN_TORAM,
	MXLOGGER_SYN_LOGCOLLECTION,
};

struct mxlogger_sync_record {
	u64 tv_sec; /* struct timeval.tv_sec */
	u64 tv_usec; /* struct timeval.tv_usec */
	u64 kernel_time; /* ktime_t */
	u32 sync_event; /* type of sync event*/
	u32 fw_time;
	u32 fw_wrap;
	u8  reserved[4];
} __packed;

struct buffer_desc {
	u32 location;			/* Buffer location */
	u32 size;			/* Buffer sz (in bytes) */
	u32 status;			/* buffer status */
	u32 info;			/* buffer info */
} __packed;

struct mxlogger_config {
	u32			magic_number;   /* 0xcaba0401 */
	u32			config_major;	/* Version Major */
	u32			config_minor;	/* Version Minor */
	u32			num_buffers;	/* configured buffers */
	scsc_mifram_ref		bfds_ref;
} __packed;

struct mxlogger_config_area {
	struct mxlogger_config	config;
	struct buffer_desc	bfds[MXLOGGER_NUM_BUFFERS];
	uint8_t	*buffers_start;
} __packed;

struct log_msg_packet {
	uint8_t		msg;		/* cmd or event id */
	uint8_t		arg;
	uint8_t		payload[MM_MXLOGGER_PAYLOAD_SZ];
} __packed;

struct mxlogger {
	bool				initialized;
	bool				configured;
	bool				enabled;
	struct scsc_mx			*mx;
	void				*mem;
	void				*mem_sync_buf;
	uint32_t			msz;
	scsc_mifram_ref			mifram_ref;
	struct mutex			lock;
	struct mxlogger_config_area	*cfg;
	u8				observers;
	u8				sync_buffer_index;
	/* collection variables */
	bool				re_enable;
	struct completion		rings_serialized_ops;
};

int mxlogger_generate_sync_record(struct mxlogger *mxlogger, enum mxlogger_sync_event event);
int mxlogger_dump_shared_memory_to_file(struct mxlogger *mxlogger);
int mxlogger_init(struct scsc_mx *mx, struct mxlogger *mxlogger, uint32_t mem_sz);
void mxlogger_deinit(struct scsc_mx *mx, struct mxlogger *mxlogger);
int mxlogger_start(struct mxlogger *mxlogger);
int mxlogger_register_observer(struct mxlogger *mxlogger, char *name);
int mxlogger_unregister_observer(struct mxlogger *mxlogger, char *name);
int mxlogger_register_global_observer(char *name);
int mxlogger_unregister_global_observer(char *name);
bool mxlogger_set_enabled_status(bool enable);

#define MEM_LAYOUT_CHECK()	\
({				\
	BUILD_BUG_ON((sizeof(struct mxlogger_sync_record) * NUM_SYNC_RECORDS) >  MXLOGGER_SYNC_SIZE); \
	BUILD_BUG_ON((MXLOGGER_TOTAL_FIX_BUF + sizeof(struct mxlogger_config_area))  > MXL_POOL_SZ); \
})

#endif /* __MX_LOGGER_H__ */
