/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_INTERNAL_H
#define __SLSI_KIC_INTERNAL_H

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/semaphore.h>
#include <net/genetlink.h>
#include <linux/time.h>
#include <linux/module.h>

#include <scsc/scsc_logring.h>

#include <scsc/kic/slsi_kic_prim.h>
#include <scsc/kic/slsi_kic_wifi.h>
#include <scsc/kic/slsi_kic_cm.h>
#include <scsc/kic/slsi_kic_bt.h>
#include <scsc/kic/slsi_kic_ant.h>

#define OS_UNUSED_PARAMETER(x) ((void)(x))

/**
 * Core instance
 */
enum slsi_kic_state {
	idle,
	initialised,
	ready
};

struct slsi_kic_service_details {
	struct list_head              proxy_q;
	enum slsi_kic_technology_type tech;
	struct slsi_kic_service_info  info;
};

struct slsi_kic_chip_details {
	struct semaphore proxy_service_list_mutex;
	struct list_head proxy_service_list;
};

struct slsi_kic_wifi_ops_tuple {
	void                     *priv;
	struct slsi_kic_wifi_ops wifi_ops;
	struct mutex             ops_mutex;
};

struct slsi_kic_bt_ops_tuple {
	void                     *priv;
	struct slsi_kic_bt_ops   bt_ops;
	struct mutex             ops_mutex;
};

struct slsi_kic_ant_ops_tuple {
	void                     *priv;
	struct slsi_kic_ant_ops  ant_ops;
	struct mutex             ops_mutex;
};

struct slsi_kic_cm_ops_tuple {
	void                   *priv;
	struct slsi_kic_cm_ops cm_ops;
	struct mutex           ops_mutex;
};

struct slsi_kic_pdata {
	enum slsi_kic_state            state;
	struct slsi_kic_chip_details   chip_details;
	struct slsi_kic_wifi_ops_tuple wifi_ops_tuple;
	struct slsi_kic_cm_ops_tuple   cm_ops_tuple;
	struct slsi_kic_bt_ops_tuple   bt_ops_tuple;
	struct slsi_kic_ant_ops_tuple  ant_ops_tuple;
	uint32_t                       seq;     /* This should *perhaps* be moved to a record struct for
						* each subscription - will look into that during the
						* filtering work. */
};

struct slsi_kic_pdata *slsi_kic_core_get_context(void);

#endif /* #ifndef __SLSI_KIC_INTERNAL_H */
