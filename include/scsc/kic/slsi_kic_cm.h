/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_CM_H
#define __SLSI_KIC_CM_H

#include <scsc/kic/slsi_kic_prim.h>

/**
 * struct slsi_kic_cm_ops - backend description for Chip Manager (CM) driver ops
 *
 * This struct is registered by the Chip Manager driver during initilisation
 * in order provide CM specific services, which can be used by KIC.
 *
 * All callbacks except where otherwise noted should return 0 on success or a
 * negative error code.
 *
 * @trigger_recovery: Trigger a firmware crash, which requires a full chip
 *	              recovery.
 *	@type specifies the recovery type.
 */
struct slsi_kic_cm_ops {
	int (*trigger_recovery)(void *priv, enum slsi_kic_test_recovery_type type);
};


/**
 * slsi_kic_cm_ops_register - register cm_ops with KIC
 *
 * @priv: Private pointer, which will be included in all calls from KIC.
 * @wifi_ops: The wifi_ops to register.
 *
 * Returns 0 on success or a negative error code.
 */
int slsi_kic_cm_ops_register(void *priv, struct slsi_kic_cm_ops *cm_ops);

/**
 * slsi_kic_cm_ops_unregister - unregister cm_ops with KIC
 *
 * @cm_ops: The cm_ops to unregister.
 *
 * After this call, no more requests can be made, but the call may sleep to wait
 * for an outstanding request that is being handled.
 */
void slsi_kic_cm_ops_unregister(struct slsi_kic_cm_ops *cm_ops);

#endif /* #ifndef __SLSI_KIC_CM_H */
