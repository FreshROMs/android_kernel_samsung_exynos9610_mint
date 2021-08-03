/****************************************************************************
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "slsi_kic_internal.h"

#if defined(CONFIG_SCSC_ANT) && defined(CONFIG_SAMSUNG_KIC)
int slsi_kic_ant_ops_register(void *priv, struct slsi_kic_ant_ops *ant_ops)
{
	struct slsi_kic_pdata *kic_inst = slsi_kic_core_get_context();

	if (!kic_inst)
		return -EFAULT;

	mutex_lock(&kic_inst->ant_ops_tuple.ops_mutex);
	memcpy(&kic_inst->ant_ops_tuple.ant_ops, ant_ops, sizeof(struct slsi_kic_ant_ops));
	kic_inst->ant_ops_tuple.priv = priv;
	mutex_unlock(&kic_inst->ant_ops_tuple.ops_mutex);
	return 0;
}
EXPORT_SYMBOL(slsi_kic_ant_ops_register);

void slsi_kic_ant_ops_unregister(struct slsi_kic_ant_ops *ant_ops)
{
	struct slsi_kic_pdata *kic_inst = slsi_kic_core_get_context();

	OS_UNUSED_PARAMETER(ant_ops);

	if (!kic_inst)
		return;

	mutex_lock(&kic_inst->ant_ops_tuple.ops_mutex);
	memset(&kic_inst->ant_ops_tuple.ant_ops, 0, sizeof(struct slsi_kic_ant_ops));
	kic_inst->ant_ops_tuple.priv = NULL;
	mutex_unlock(&kic_inst->ant_ops_tuple.ops_mutex);
}
EXPORT_SYMBOL(slsi_kic_ant_ops_unregister);
#endif
