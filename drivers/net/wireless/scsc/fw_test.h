/****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_FW_TEST_H__
#define __SLSI_FW_TEST_H__

#include "dev.h"

struct slsi_fw_test {
	struct slsi_dev      *sdev;
	bool                 fw_test_enabled;
	struct slsi_skb_work fw_test_work;
	struct slsi_spinlock fw_test_lock;
	struct sk_buff       *mlme_add_vif_req[CONFIG_SCSC_WLAN_MAX_INTERFACES + 1];
	struct sk_buff       *mlme_connect_req[CONFIG_SCSC_WLAN_MAX_INTERFACES + 1];
	struct sk_buff       *mlme_connect_cfm[CONFIG_SCSC_WLAN_MAX_INTERFACES + 1];
	struct sk_buff       *mlme_procedure_started_ind[CONFIG_SCSC_WLAN_MAX_INTERFACES + 1]; /* TODO_HARDMAC : Per AID as well as per vif */
};

void slsi_fw_test_init(struct slsi_dev *sdev, struct slsi_fw_test *fwtest);
void slsi_fw_test_deinit(struct slsi_dev *sdev, struct slsi_fw_test *fwtest);
int slsi_fw_test_signal(struct slsi_dev *sdev, struct slsi_fw_test *fwtest, struct sk_buff *skb);
int slsi_fw_test_signal_with_udi_header(struct slsi_dev *sdev, struct slsi_fw_test *fwtest, struct sk_buff *skb);

#endif /*__SLSI_FW_TEST_H__*/
