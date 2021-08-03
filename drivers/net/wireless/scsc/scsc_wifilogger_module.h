/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/

#ifndef _SCSC_WIFILOGGER_MODULE_H_
#define _SCSC_WIFILOGGER_MODULE_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <scsc/scsc_logring.h>

#include "scsc_wifilogger_internal.h"

#include "scsc_wifilogger_ring_connectivity.h"
#include "scsc_wifilogger_ring_wakelock.h"
#include "scsc_wifilogger_ring_pktfate.h"

#ifdef CONFIG_SCSC_WIFILOGGER_TEST
#include "scsc_wifilogger_ring_test.h"
#endif

#endif /* _SCSC_WIFILOGGER_MODULE_H_  */
