/****************************************************************************
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_ANT_H
#define __SLSI_KIC_ANT_H

#include <scsc/kic/slsi_kic_prim.h>

/**
 * struct slsi_kic_ant_ops - backend description for ANT service driver ops.
 *
 * This struct is registered by the ANT service driver during initilisation
 * in order provide ANT specific services, which can be used by KIC.
 *
 * All callbacks except where otherwise noted should return 0 on success or a
 * negative error code.
 *
 * @trigger_recovery: Trigger a ANT firmware subsystem recovery. The variable
 *	@type specifies the recovery type.
 */
struct slsi_kic_ant_ops {
	int (*trigger_recovery)(void *priv, enum slsi_kic_test_recovery_type type);
};

#ifdef CONFIG_SAMSUNG_KIC

/**
 * slsi_kic_ant_ops_register - register ant_ops with KIC
 *
 * @priv: Private pointer, which will be included in all calls from KIC.
 * @ant_ops: The ant_ops to register.
 *
 * Returns 0 on success or a negative error code.
 */
int slsi_kic_ant_ops_register(void *priv, struct slsi_kic_ant_ops *ant_ops);

/**
 * slsi_kic_ant_ops_unregister - unregister ant_ops with KIC
 *
 * @ant_ops: The ant_ops to unregister.
 *
 * After this call, no more requests can be made, but the call may sleep to wait
 * for an outstanding request that is being handled.
 */
void slsi_kic_ant_ops_unregister(struct slsi_kic_ant_ops *ant_ops);

#else

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define slsi_kic_ant_ops_register(a, b) \
	do { \
		UNUSED(a); \
		UNUSED(b); \
	} while (0)

#define slsi_kic_ant_ops_unregister(a) UNUSED(a)
#endif /* CONFIG_SAMSUNG_KIC */

#endif /* #ifndef __SLSI_KIC_ANT_H */
