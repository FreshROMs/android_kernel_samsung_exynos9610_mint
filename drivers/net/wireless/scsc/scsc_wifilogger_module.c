/******************************************************************************
 *
 *   Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ******************************************************************************/
/**
 * Internal Reference docs for WiFi-Logger subsystem
 *
 *  SC-507043-SW --  Android Wi-Fi Logger architecture
 *  SC-507780-DD --  Android Enhanced Logging
 *		     WiFiLogger Core Driver Requirements and Design
 */
#include "scsc_wifilogger_module.h"

static int __init scsc_wifilogger_module_init(void)
{
	if (scsc_wifilogger_init()) {
		scsc_wifilogger_ring_connectivity_init();
		scsc_wifilogger_ring_wakelock_init();
		scsc_wifilogger_ring_pktfate_init();
#ifdef CONFIG_SCSC_WIFILOGGER_TEST
		scsc_wifilogger_ring_test_init();
#endif
		scsc_wifilogger_fw_alert_init();
	} else {
		SCSC_TAG_ERR(WLOG, "Module init failed\n");
		return -ENOMEM;
	}

	SCSC_TAG_INFO(WLOG, "Wi-Fi Logger subsystem initialized.\n");

	return 0;
}

static void  __exit scsc_wifilogger_module_exit(void)
{
	scsc_wifilogger_destroy();

	SCSC_TAG_INFO(WLOG, "Wi-Fi Logger subsystem unloaded.\n");
}

module_init(scsc_wifilogger_module_init);
module_exit(scsc_wifilogger_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung SLSI");
MODULE_DESCRIPTION("Android Wi-Fi Logger module");
