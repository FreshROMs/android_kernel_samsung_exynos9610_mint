/*****************************************************************************
 *
 * Copyright (c) 2012 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "dev.h"
#include "unittest.h"
#include "debug.h"
#include "hip_bh.h"

int slsi_sm_service_driver_register(void)
{
	int csr_result;

	csr_result = slsi_sdio_func_drv_register();
	if (csr_result != 0) {
		SLSI_ERR_NODEV("Failed to register the pretend SDIO function driver: csrResult=%d\n", csr_result);
		return -EIO;
	}

	return 0;
}

void slsi_sm_service_driver_unregister(void)
{
	slsi_sdio_func_drv_unregister();
}

void slsi_sm_service_failed(struct slsi_dev *sdev, const char *reason)
{
}

bool slsi_is_test_mode_enabled(void)
{
	return false;
}

bool slsi_is_rf_test_mode_enabled(void)
{
	return false;
}

int slsi_check_rf_test_mode(void)
{
	return 0;
}

int slsi_sm_wlan_service_start(struct slsi_dev *sdev)
{
	return 0;
}

int slsi_sm_wlan_service_stop(struct slsi_dev *sdev)
{
	return 0;
}

int slsi_sm_wlan_service_open(struct slsi_dev *sdev)
{
	return 0;
}

int mx140_file_request_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_rel_path, const char *filename)
{
	return 0;
}

void mx140_file_release_conf(struct scsc_mx *mx, const struct firmware *conf)
{
}

void slsi_sm_wlan_service_close(struct slsi_dev *sdev)
{
}
int slsi_sm_recovery_service_stop(struct slsi_dev *sdev)
{
	return 0;
}
int slsi_sm_recovery_service_close(struct slsi_dev *sdev)
{
	return 0;
}
