/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* uses */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <scsc/scsc_logring.h>
#include <linux/bitmap.h>
#include "scsc_mif_abs.h"

/* Implements */
#include "mifqos.h"

int mifqos_init(struct mifqos *qos, struct scsc_mif_abs *mif)
{
	u8 i;

	if (!qos)
		return -EIO;

	SCSC_TAG_INFO(MIF, "Init MIF QoS\n");

	for (i = 0; i < SCSC_SERVICE_TOTAL; i++)
		qos->qos_in_use[i] = false;

	mutex_init(&qos->lock);
	qos->mif = mif;

	return 0;
}

int mifqos_set_affinity_cpu(struct mifqos *qos, u8 cpu)
{
	struct scsc_mif_abs *mif;
	int ret = -ENODEV;

	if (!qos)
		return -EIO;

	mutex_lock(&qos->lock);

	SCSC_TAG_INFO(MIF, "Change CPU affinity to %d\n", cpu);

	mif = qos->mif;

	if (mif->mif_set_affinity_cpu)
		ret = mif->mif_set_affinity_cpu(mif, cpu);

	mutex_unlock(&qos->lock);

	return ret;
}

int mifqos_add_request(struct mifqos *qos, enum scsc_service_id id, enum scsc_qos_config config)
{
	struct scsc_mif_abs *mif;
	struct scsc_mifqos_request *req;
	int ret = 0;

	if (!qos)
		return -EIO;

	mutex_lock(&qos->lock);
	if (qos->qos_in_use[id]) {
		mutex_unlock(&qos->lock);
		return -EIO;
	}

	SCSC_TAG_INFO(MIF, "Service id %d add QoS request %d\n", id, config);

	mif = qos->mif;
	req = &qos->qos_req[id];

	if (mif->mif_pm_qos_add_request)
		ret = mif->mif_pm_qos_add_request(mif, req, config);
	if (ret) {
		mutex_unlock(&qos->lock);
		return ret;
	}
	qos->qos_in_use[id] = true;
	mutex_unlock(&qos->lock);

	return 0;
}

int mifqos_update_request(struct mifqos *qos, enum scsc_service_id id, enum scsc_qos_config config)
{
	struct scsc_mif_abs *mif;
	struct scsc_mifqos_request *req;

	if (!qos)
		return -EIO;

	mutex_lock(&qos->lock);
	if (!qos->qos_in_use[id]) {
		mutex_unlock(&qos->lock);
		return -EIO;
	}

	SCSC_TAG_INFO(MIF, "Service id %d update QoS request %d\n", id, config);

	mif = qos->mif;
	req = &qos->qos_req[id];

	mutex_unlock(&qos->lock);

	if (mif->mif_pm_qos_update_request)
		return mif->mif_pm_qos_update_request(mif, req, config);
	else
		return 0;
}

int mifqos_remove_request(struct mifqos *qos, enum scsc_service_id id)
{
	struct scsc_mif_abs *mif;
	struct scsc_mifqos_request *req;

	if (!qos)
		return -EIO;

	mutex_lock(&qos->lock);
	if (!qos->qos_in_use[id]) {
		mutex_unlock(&qos->lock);
		return -EIO;
	}

	SCSC_TAG_INFO(MIF, "Service id %d remove QoS\n", id);

	mif = qos->mif;
	req = &qos->qos_req[id];

	qos->qos_in_use[id] = false;

	mutex_unlock(&qos->lock);

	if (mif->mif_pm_qos_remove_request)
		return mif->mif_pm_qos_remove_request(mif, req);
	else
		return 0;
}

int mifqos_deinit(struct mifqos *qos)
{
	enum scsc_service_id i;

	SCSC_TAG_INFO(MIF, "Deinit MIF QoS\n");

	for (i = 0; i < SCSC_SERVICE_TOTAL; i++)
		mifqos_remove_request(qos, i);

	return 0;
}

