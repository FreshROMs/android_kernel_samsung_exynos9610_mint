/****************************************************************************
 *
 * Copyright (c) 2012 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <scsc/scsc_logring.h>

#include "hip.h"
#include "debug.h"
#include "procfs.h"
#include "sap.h"
#include "hip4.h"
#ifdef CONFIG_SCSC_SMAPPER
#include "hip4_smapper.h"
#endif

/* SAP implementations container. Local and static to hip */
static struct hip_sap {
	struct sap_api *sap[SAP_TOTAL];
} hip_sap_cont;

/* Register SAP with HIP layer */
int slsi_hip_sap_register(struct sap_api *sap_api)
{
	u8 class = sap_api->sap_class;

	if (class >= SAP_TOTAL)
		return -ENODEV;

	hip_sap_cont.sap[class] = sap_api;

	return 0;
}

/* UNregister SAP with HIP layer */
int slsi_hip_sap_unregister(struct sap_api *sap_api)
{
	u8 class = sap_api->sap_class;

	if (class >= SAP_TOTAL)
		return -ENODEV;

	hip_sap_cont.sap[class] = NULL;

	return 0;
}

int slsi_hip_sap_setup(struct slsi_dev *sdev)
{
	/* Execute callbacks to intorm Supported version */
	u16 version = 0;
	u32 conf_hip4_ver = 0;

	conf_hip4_ver = scsc_wifi_get_hip_config_version(&sdev->hip4_inst.hip_control->init);

	/* We enforce that all the SAPs are registered at this point */
	if ((!hip_sap_cont.sap[SAP_MLME]) || (!hip_sap_cont.sap[SAP_MA]) ||
	    (!hip_sap_cont.sap[SAP_DBG]) || (!hip_sap_cont.sap[SAP_TST]))
		return -ENODEV;

	if (hip_sap_cont.sap[SAP_MLME]->sap_version_supported) {
		if (conf_hip4_ver == 4)
			version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_mlme_ver);
		if (conf_hip4_ver == 5)
			version = scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_mlme_ver);
		if (hip_sap_cont.sap[SAP_MLME]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	if (hip_sap_cont.sap[SAP_MA]->sap_version_supported) {
		if (conf_hip4_ver == 4)
			version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_ma_ver);
		if (conf_hip4_ver == 5)
			version = scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_ma_ver);
		if (hip_sap_cont.sap[SAP_MA]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	if (hip_sap_cont.sap[SAP_DBG]->sap_version_supported) {
		if (conf_hip4_ver == 4)
			version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_debug_ver);
		if (conf_hip4_ver == 5)
			version = scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_debug_ver);
		if (hip_sap_cont.sap[SAP_DBG]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	if (hip_sap_cont.sap[SAP_TST]->sap_version_supported) {
		if (conf_hip4_ver == 4)
			version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_test_ver);
		if (conf_hip4_ver == 5)
			version = scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_test_ver);
		if (hip_sap_cont.sap[SAP_TST]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	/* Success */
	return 0;
}

static int slsi_hip_service_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct slsi_dev *sdev = (struct slsi_dev *)data;
	int i;

	if (!sdev)
		return NOTIFY_BAD;

	/* We enforce that all the SAPs are registered at this point */
	if ((!hip_sap_cont.sap[SAP_MLME]) || (!hip_sap_cont.sap[SAP_MA]) ||
	    (!hip_sap_cont.sap[SAP_DBG]) || (!hip_sap_cont.sap[SAP_TST]))
		return NOTIFY_BAD;

	/* Check whether any sap is interested in the notifications */
	for (i = 0; i < SAP_TOTAL; i++)
		if (hip_sap_cont.sap[i]->sap_notifier) {
			if (hip_sap_cont.sap[i]->sap_notifier(sdev, event))
				return NOTIFY_BAD;
		}

	switch (event) {
	case SCSC_WIFI_STOP:
		SLSI_INFO(sdev, "Freeze HIP4\n");
		mutex_lock(&sdev->hip.hip_mutex);
		hip4_freeze(&sdev->hip4_inst);
		mutex_unlock(&sdev->hip.hip_mutex);
		break;

	case SCSC_WIFI_FAILURE_RESET:
		SLSI_INFO(sdev, "Set HIP4 up again\n");
		mutex_lock(&sdev->hip.hip_mutex);
		hip4_setup(&sdev->hip4_inst);
		mutex_unlock(&sdev->hip.hip_mutex);
		break;

	case SCSC_WIFI_SUSPEND:
		SLSI_INFO(sdev, "Suspend HIP4\n");
		mutex_lock(&sdev->hip.hip_mutex);
		hip4_suspend(&sdev->hip4_inst);
		mutex_unlock(&sdev->hip.hip_mutex);
		break;

	case SCSC_WIFI_RESUME:
		SLSI_INFO(sdev, "Resume HIP4\n");
		mutex_lock(&sdev->hip.hip_mutex);
		hip4_resume(&sdev->hip4_inst);
		mutex_unlock(&sdev->hip.hip_mutex);
		break;

	case SCSC_WIFI_SUBSYSTEM_RESET:
	case SCSC_WIFI_CHIP_READY:
		break;

	default:
		SLSI_INFO(sdev, "Unknown event code %lu\n", event);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cm_nb = {
	.notifier_call = slsi_hip_service_notifier,
};

int slsi_hip_init(struct slsi_dev *sdev, struct device *dev)
{
	SLSI_UNUSED_PARAMETER(dev);

	memset(&sdev->hip4_inst, 0, sizeof(sdev->hip4_inst));

	sdev->hip.sdev = sdev;
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STOPPED);
	mutex_init(&sdev->hip.hip_mutex);

	/* Register with the service notifier to receiver
	 * asynchronus messages such as SCSC_WIFI_STOP(Freeze), SCSC_WIFI_FAILURE_RESET i
	 */
	slsi_wlan_service_notifier_register(&cm_nb);

	return 0;
}

void slsi_hip_deinit(struct slsi_dev *sdev)
{
	slsi_wlan_service_notifier_unregister(&cm_nb);
	mutex_destroy(&sdev->hip.hip_mutex);
}

int slsi_hip_start(struct slsi_dev *sdev)
{
	if (!sdev->maxwell_core) {
		SLSI_ERR(sdev, "Maxwell core does not exist\n");
		return -EINVAL;
	}

	SLSI_DBG4(sdev, SLSI_HIP_INIT_DEINIT, "[1/3]. Update HIP state (SLSI_HIP_STATE_STARTING)\n");
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTING);

	SLSI_DBG4(sdev, SLSI_HIP_INIT_DEINIT, "[2/3]. Initialise HIP\n");
	if (hip4_init(&sdev->hip4_inst)) {
		atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STOPPED);
		SLSI_ERR(sdev, "hip4_init failed\n");
		return -EINVAL;
	}

	SLSI_DBG4(sdev, SLSI_HIP_INIT_DEINIT, "[3/3]. Update HIP state (SLSI_HIP_STATE_STARTED)\n");
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);

	return 0;
}

/* SAP rx proxy */
int slsi_hip_rx(struct slsi_dev *sdev, struct sk_buff *skb)
{
	u16 pid;

	/* We enforce that all the SAPs are registered at this point */
	if ((!hip_sap_cont.sap[SAP_MLME]) || (!hip_sap_cont.sap[SAP_MA]) ||
	    (!hip_sap_cont.sap[SAP_DBG]) || (!hip_sap_cont.sap[SAP_TST]))
		return -ENODEV;

	/* Here we push a copy of the bare RECEIVED skb data also to the
	 * logring as a binary record.
	 * Note that bypassing UDI subsystem as a whole means we are losing:
	 *   UDI filtering / UDI Header INFO / UDI QueuesFrames Throttling /
	 *   UDI Skb Asynchronous processing
	 * We keep split DATA/CTRL path.
	 */
	if (fapi_is_ma(skb))
		SCSC_BIN_TAG_DEBUG(BIN_WIFI_DATA_RX, skb->data, skb->len);
	else
		SCSC_BIN_TAG_DEBUG(BIN_WIFI_CTRL_RX, skb->data, skb->len);
	/* Udi test : If pid in UDI range then pass to UDI and ignore */
	slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb, SLSI_LOG_DIRECTION_TO_HOST);
	pid = fapi_get_u16(skb, receiver_pid);
	if (pid >= SLSI_TX_PROCESS_ID_UDI_MIN && pid <= SLSI_TX_PROCESS_ID_UDI_MAX) {
#ifdef CONFIG_SCSC_SMAPPER
		hip4_smapper_free_mapped_skb(skb);
#endif
		kfree_skb(skb);
		return 0;
	}

	if (fapi_is_ma(skb)) {
		/* It is anomolous to handle the MA_BLOCKACK_IND in the
		 * mlme wq.
		 */
		if (fapi_get_sigid(skb) == MA_BLOCKACK_IND)
			return hip_sap_cont.sap[SAP_MLME]->sap_handler(sdev, skb);
		else
			return hip_sap_cont.sap[SAP_MA]->sap_handler(sdev, skb);
	}

	if (fapi_is_mlme(skb))
		return hip_sap_cont.sap[SAP_MLME]->sap_handler(sdev, skb);

	if (fapi_is_debug(skb))
		return hip_sap_cont.sap[SAP_DBG]->sap_handler(sdev, skb);

	if (fapi_is_test(skb))
		return hip_sap_cont.sap[SAP_TST]->sap_handler(sdev, skb);

	return -EIO;
}

/* Only DATA plane will look at the returning FB to account BoT */
int slsi_hip_tx_done(struct slsi_dev *sdev, u8 vif, u8 peer_index, u8 ac)
{
	return hip_sap_cont.sap[SAP_MA]->sap_txdone(sdev, vif, peer_index, ac);
}

int slsi_hip_setup(struct slsi_dev *sdev)
{
	u32 ret_val;
	mutex_lock(&sdev->hip.hip_mutex);

	/* Setup hip4 after initialization */
	ret_val = hip4_setup(&sdev->hip4_inst);

	mutex_unlock(&sdev->hip.hip_mutex);
	return ret_val;
}

#ifdef CONFIG_SCSC_SMAPPER
int slsi_hip_consume_smapper_entry(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return hip4_smapper_consume_entry(sdev, &sdev->hip4_inst, skb);
}

struct sk_buff *slsi_hip_get_skb_from_smapper(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return hip4_smapper_get_skb(sdev, &sdev->hip4_inst, skb);
}

void *slsi_hip_get_skb_data_from_smapper(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return hip4_smapper_get_skb_data(sdev, &sdev->hip4_inst, skb);
}
#endif

int slsi_hip_stop(struct slsi_dev *sdev)
{
	mutex_lock(&sdev->hip.hip_mutex);

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED) {
		mutex_unlock(&sdev->hip.hip_mutex);
		return 0;
	}
	SLSI_DBG4(sdev, SLSI_HIP_INIT_DEINIT, "Update HIP state (SLSI_HIP_STATE_STOPPING)\n");
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STOPPING);

	hip4_deinit(&sdev->hip4_inst);

	SLSI_DBG4(sdev, SLSI_HIP_INIT_DEINIT, "Update HIP state (SLSI_HIP_STATE_STOPPED)\n");
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STOPPED);

	mutex_unlock(&sdev->hip.hip_mutex);
	return 0;
}
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
void slsi_hip_set_napi_cpu(struct slsi_dev *sdev, u8 napi_cpu)
{
	hip4_set_napi_cpu(&sdev->hip4_inst, napi_cpu);
}
#else
void slsi_hip_reprocess_skipped_data_bh(struct slsi_dev *sdev)
{
	struct slsi_hip4 *hip4 =  &sdev->hip4_inst;

	hip4_sched_wq(hip4);
}
#endif
