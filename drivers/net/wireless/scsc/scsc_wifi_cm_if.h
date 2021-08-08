/****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SCSC_WIFI_CM_IF_H__
#define __SCSC_WIFI_CM_IF_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/kref.h>

#include <scsc/scsc_mx.h>

#define SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC 8

struct slsi_dev;

/**
 * CM interface States:
 *     STOPPED  : (default) state,
 *     PROBING  :
 *     PROBED   :
 *     STARTING :
 *     STARTED  :
 *     STOPPING :
 *     REMOVING :
 *     REMOVED  :
 *     BLOCKED  :
 */
enum scsc_wifi_cm_if_state {
	SCSC_WIFI_CM_IF_STATE_STOPPED,
	SCSC_WIFI_CM_IF_STATE_PROBING,
	SCSC_WIFI_CM_IF_STATE_PROBED,
	SCSC_WIFI_CM_IF_STATE_STARTING,
	SCSC_WIFI_CM_IF_STATE_STARTED,
	SCSC_WIFI_CM_IF_STATE_STOPPING,
	SCSC_WIFI_CM_IF_STATE_REMOVING,
	SCSC_WIFI_CM_IF_STATE_REMOVED,
	SCSC_WIFI_CM_IF_STATE_BLOCKED
};

/**
 * Notification Events
 *     SCSC_WIFI_STOP : Wifi service should freeze
 *     SCSC_WIFI_FAILURE_RESET : Failure has been handled
 *     SCSC_WIFI_SUSPEND: Host going in to suspend mode
 *     SCSC_WIFI_RESUME: Host resuming
 */
enum scsc_wifi_cm_if_notifier {
	SCSC_WIFI_STOP,
	SCSC_WIFI_FAILURE_RESET,
	SCSC_WIFI_SUSPEND,
	SCSC_WIFI_RESUME,
	SCSC_WIFI_SUBSYSTEM_RESET,
	SCSC_WIFI_CHIP_READY,
	SCSC_MAX_NOTIFIER
};

struct scsc_wifi_cm_if {
	struct slsi_dev *sdev;
	/* a std mutex */
	struct mutex    cm_if_mutex;

	struct kref     kref;

	/* refer to enum scsc_wifi_cm_if_state */
	atomic_t        cm_if_state;

	int       recovery_state;

#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
	atomic_t                 reset_level;
#endif
};

/*********************************** API ************************************/

/**
 * Driver's interface to cm_if
 */
struct slsi_dev *slsi_dev_attach(struct device *dev, struct scsc_mx *core, struct scsc_service_client *mx_wlan_client);
void slsi_dev_detach(struct slsi_dev *sdev);

/**
 * cm_if's interface to driver
 */
int slsi_sm_service_driver_register(void);
void slsi_sm_service_driver_unregister(void);
void slsi_sm_service_failed(struct slsi_dev *sdev, const char *reason);
int slsi_sm_wlan_service_open(struct slsi_dev *sdev);
int slsi_sm_wlan_service_start(struct slsi_dev *sdev);
int slsi_sm_wlan_service_stop(struct slsi_dev *sdev);
void slsi_sm_wlan_service_close(struct slsi_dev *sdev);
int slsi_wlan_service_notifier_register(struct notifier_block *nb);
int slsi_wlan_service_notifier_unregister(struct notifier_block *nb);
int slsi_sm_recovery_service_stop(struct slsi_dev *sdev);
int slsi_sm_recovery_service_close(struct slsi_dev *sdev);
int slsi_sm_recovery_service_open(struct slsi_dev *sdev);
int slsi_sm_recovery_service_start(struct slsi_dev *sdev);

#endif
