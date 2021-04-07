/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "slsi_kic_internal.h"

int slsi_kic_cm_ops_register(void *priv, struct slsi_kic_cm_ops *cm_ops)
{
	struct slsi_kic_pdata *kic_inst = slsi_kic_core_get_context();

	if (!kic_inst)
		return -EFAULT;

	mutex_lock(&kic_inst->cm_ops_tuple.ops_mutex);
	memcpy(&kic_inst->cm_ops_tuple.cm_ops, cm_ops, sizeof(struct slsi_kic_cm_ops));
	kic_inst->cm_ops_tuple.priv = priv;
	mutex_unlock(&kic_inst->cm_ops_tuple.ops_mutex);
	return 0;
}
EXPORT_SYMBOL(slsi_kic_cm_ops_register);

void slsi_kic_cm_ops_unregister(struct slsi_kic_cm_ops *cm_ops)
{
	struct slsi_kic_pdata *kic_inst = slsi_kic_core_get_context();

	OS_UNUSED_PARAMETER(cm_ops);

	if (!kic_inst)
		return;

	mutex_lock(&kic_inst->cm_ops_tuple.ops_mutex);
	memset(&kic_inst->cm_ops_tuple.cm_ops, 0, sizeof(struct slsi_kic_cm_ops));
	kic_inst->cm_ops_tuple.priv = NULL;
	mutex_unlock(&kic_inst->cm_ops_tuple.ops_mutex);
}
EXPORT_SYMBOL(slsi_kic_cm_ops_unregister);
