/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_WIFI_H
#define __SLSI_KIC_WIFI_H

#include <scsc/kic/slsi_kic_prim.h>

/**
 * struct slsi_kic_wifi_ops - backend description for Wi-Fi service driver ops.
 *
 * This struct is registered by the Wi-Fi service driver during initilisation
 * in order provide Wi-Fi specific services, which can be used by KIC.
 *
 * All callbacks except where otherwise noted should return 0 on success or a
 * negative error code.
 *
 * @trigger_recovery: Trigger a Wi-Fi firmware subsystem recovery. The variable
 *	@type specifies the recovery type.
 */
struct slsi_kic_wifi_ops {
	int (*trigger_recovery)(void *priv, enum slsi_kic_test_recovery_type type);
};

#ifdef CONFIG_SAMSUNG_KIC
/**
 * slsi_kic_wifi_ops_register - register wifi_ops with KIC
 *
 * @priv: Private pointer, which will be included in all calls from KIC.
 * @wifi_ops: The wifi_ops to register.
 *
 * Returns 0 on success or a negative error code.
 */
int slsi_kic_wifi_ops_register(void *priv, struct slsi_kic_wifi_ops *wifi_ops);

/**
 * slsi_kic_wifi_ops_unregister - unregister wifi_ops with KIC
 *
 * @wifi_ops: The wifi_ops to unregister.
 *
 * After this call, no more requests can be made, but the call may sleep to wait
 * for an outstanding request that is being handled.
 */
void slsi_kic_wifi_ops_unregister(struct slsi_kic_wifi_ops *wifi_ops);

#else

#define slsi_kic_wifi_ops_register(a, b) \
	do { \
		(void)(a); \
		(void)(b); \
	} while (0)

#define slsi_kic_wifi_ops_unregister(a) \
		do { \
			(void)(a); \
		} while (0)

#endif /* CONFIG_SAMSUNG_KIC */

#endif /* #ifndef __SLSI_KIC_WIFI_H */
