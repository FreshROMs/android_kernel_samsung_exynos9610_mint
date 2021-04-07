/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/types.h>
#include "dev.h"
#include "sap.h"
#include "sap_test.h"
#include "hip.h"

#include "debug.h"

#define SUPPORTED_OLD_VERSION   0

static int sap_test_version_supported(u16 version);
static int sap_test_rx_handler(struct slsi_dev *sdev, struct sk_buff *skb);

static struct sap_api sap_test = {
	.sap_class = SAP_TST,
	.sap_version_supported = sap_test_version_supported,
	.sap_handler = sap_test_rx_handler,
	.sap_versions = { FAPI_TEST_SAP_VERSION, SUPPORTED_OLD_VERSION },
};

static int sap_test_version_supported(u16 version)
{
	unsigned int major = SAP_MAJOR(version);
	unsigned int minor = SAP_MINOR(version);
	u8           i = 0;

	SLSI_INFO_NODEV("Reported version: %d.%d\n", major, minor);

	for (i = 0; i < SAP_MAX_VER; i++)
		if (SAP_MAJOR(sap_test.sap_versions[i]) == major)
			return 0;

	SLSI_ERR_NODEV("Version %d.%d Not supported\n", major, minor);

	return -EINVAL;
}

static int sap_test_rx_handler(struct slsi_dev *sdev, struct sk_buff *skb)
{
	if (slsi_rx_blocking_signals(sdev, skb) == 0)
		return 0;

	SLSI_INFO_NODEV("TEST SAP not implemented\n");
	/* Silently consume the skb */
	kfree_skb(skb);
	/* return success */
	return 0;
}

int sap_test_init(void)
{
	SLSI_INFO_NODEV("Registering SAP\n");
	slsi_hip_sap_register(&sap_test);
	return 0;
}

int sap_test_deinit(void)
{
	SLSI_INFO_NODEV("Unregistering SAP\n");
	slsi_hip_sap_unregister(&sap_test);
	return 0;
}
