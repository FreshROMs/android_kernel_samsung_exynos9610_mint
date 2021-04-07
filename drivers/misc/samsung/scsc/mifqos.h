/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __MIFQOS_H
#define __MIFQOS_H

#include <linux/mutex.h>
#include <scsc/scsc_mx.h>

struct scsc_mif_abs;
struct mifqos;

int mifqos_init(struct mifqos *qos, struct scsc_mif_abs *mif);
int mifqos_set_affinity_cpu(struct mifqos *qos, u8 cpu);
int mifqos_add_request(struct mifqos *qos, enum scsc_service_id id, enum scsc_qos_config config);
int mifqos_update_request(struct mifqos *qos, enum scsc_service_id id, enum scsc_qos_config config);
int mifqos_remove_request(struct mifqos *qos, enum scsc_service_id id);
int mifqos_list(struct mifqos *qos);
int mifqos_deinit(struct mifqos *qos);

struct scsc_mifqos_request;

struct mifqos {
	bool                 qos_in_use[SCSC_SERVICE_TOTAL];
	struct mutex         lock;
	struct scsc_mif_abs  *mif;
	struct scsc_mifqos_request qos_req[SCSC_SERVICE_TOTAL];
};
#endif
