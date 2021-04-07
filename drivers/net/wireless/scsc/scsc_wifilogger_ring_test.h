/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
#ifndef __SCSC_WIFILOGGER_RING_TEST_H__
#define __SCSC_WIFILOGGER_RING_TEST_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <scsc/scsc_logring.h>

#include "scsc_wifilogger_core.h"
#include "scsc_wifilogger.h"

#define WLOGGER_RTEST_NAME		"test"

struct tring_hdr {
	u32 seq;
	u64 fake;
} __packed;

bool scsc_wifilogger_ring_test_init(void);

int scsc_wifilogger_ring_test_write(char *buf, size_t blen);
#endif /* __SCSC_WIFILOGGER_RING_TEST_H__ */
