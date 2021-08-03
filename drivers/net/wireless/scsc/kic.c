/******************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include <scsc/kic/slsi_kic_wifi.h>
#include "dev.h"
#include "debug.h"
#include "mxman.h"

static int wifi_kic_trigger_recovery(void *priv, enum slsi_kic_test_recovery_type type)
{
	struct slsi_dev *sdev = (struct slsi_dev *)priv;
	char reason[80];

	if (!sdev)
		return -EINVAL;

	if (sdev->device_state != SLSI_DEVICE_STATE_STARTED)
		return -EAGAIN;

	switch (type) {
	case slsi_kic_test_recovery_type_subsystem_panic:
		SLSI_INFO(sdev, "Trigger Wi-Fi firmware subsystem panic\n");
		if (scsc_service_force_panic(sdev->service))
			return -EINVAL;
		return 0;
	case slsi_kic_test_recovery_type_emulate_firmware_no_response:
		SLSI_INFO(sdev, "Trigger Wi-Fi host panic\n");
		snprintf(reason, sizeof(reason), "slsi_kic_test_recovery_type_emulate_firmware_no_response");
		slsi_sm_service_failed(sdev, reason);
		return 0;
	case slsi_kic_test_recovery_type_watch_dog:
	case slsi_kic_test_recovery_type_chip_crash:
	default:
		return -EINVAL;
	}
}

static struct slsi_kic_wifi_ops kic_ops = {
	.trigger_recovery = wifi_kic_trigger_recovery,
};

int wifi_kic_register(struct slsi_dev *sdev)
{
	return slsi_kic_wifi_ops_register((void *)sdev, &kic_ops);
}

void wifi_kic_unregister(void)
{
	return slsi_kic_wifi_ops_unregister(&kic_ops);
}
