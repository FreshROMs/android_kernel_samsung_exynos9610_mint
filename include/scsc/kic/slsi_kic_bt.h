/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_BT_H
#define __SLSI_KIC_BT_H

#include <scsc/kic/slsi_kic_prim.h>

/**
 * struct slsi_kic_bt_ops - backend description for BT service driver ops.
 *
 * This struct is registered by the BT service driver during initilisation
 * in order provide BT specific services, which can be used by KIC.
 *
 * All callbacks except where otherwise noted should return 0 on success or a
 * negative error code.
 *
 * @trigger_recovery: Trigger a BT firmware subsystem recovery. The variable
 *	@type specifies the recovery type.
 */
struct slsi_kic_bt_ops {
	int (*trigger_recovery)(void *priv, enum slsi_kic_test_recovery_type type);
};

#ifdef CONFIG_SAMSUNG_KIC

/**
 * slsi_kic_bt_ops_register - register bt_ops with KIC
 *
 * @priv: Private pointer, which will be included in all calls from KIC.
 * @bt_ops: The bt_ops to register.
 *
 * Returns 0 on success or a negative error code.
 */
int slsi_kic_bt_ops_register(void *priv, struct slsi_kic_bt_ops *bt_ops);

/**
 * slsi_kic_bt_ops_unregister - unregister bt_ops with KIC
 *
 * @bt_ops: The bt_ops to unregister.
 *
 * After this call, no more requests can be made, but the call may sleep to wait
 * for an outstanding request that is being handled.
 */
void slsi_kic_bt_ops_unregister(struct slsi_kic_bt_ops *bt_ops);

#else

#define slsi_kic_bt_ops_register(a, b) \
	do { \
		(void)(a); \
		(void)(b); \
	} while (0)

#define slsi_kic_bt_ops_unregister(a) \
		do { \
			(void)(a); \
		} while (0)

#endif /* CONFIG_SAMSUNG_KIC */

#endif /* #ifndef __SLSI_KIC_BT_H */
