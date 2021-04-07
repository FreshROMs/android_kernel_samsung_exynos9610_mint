/******************************************************************************
 *
 * Copyright (c) 2012 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include <linux/kthread.h>

#include "unittest.h"
#include "hip.h"
#include "sap.h"
#include "debug.h"
#include "procfs.h"
#include "hip4.h"

#define SLSI_TEST_DEV_SDIO_BLOCK_SIZE 500
bool hip4_sampler_sample_start_stop_q = true;
struct hip4_hip_control test_hip_control;

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

	if (hip_sap_cont.sap[SAP_MLME]->sap_version_supported) {
		version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_mlme_ver);
		if (hip_sap_cont.sap[SAP_MLME]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	if (hip_sap_cont.sap[SAP_MA]->sap_version_supported) {
		version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_ma_ver);
		if (hip_sap_cont.sap[SAP_MA]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	if (hip_sap_cont.sap[SAP_DBG]->sap_version_supported) {
		version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_debug_ver);
		if (hip_sap_cont.sap[SAP_DBG]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	if (hip_sap_cont.sap[SAP_TST]->sap_version_supported) {
		version = scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_test_ver);
		if (hip_sap_cont.sap[SAP_TST]->sap_version_supported(version))
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	/* Success */
	return 0;
}

/* SAP rx proxy */
int slsi_hip_rx(struct slsi_dev *sdev, struct sk_buff *skb)
{
	u16 pid;

	/* Udi test : If pid in UDI range then pass to UDI and ignore */
	slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb, SLSI_LOG_DIRECTION_TO_HOST);
	pid = fapi_get_u16(skb, receiver_pid);
	if (pid >= SLSI_TX_PROCESS_ID_UDI_MIN && pid <= SLSI_TX_PROCESS_ID_UDI_MAX) {
		kfree_skb(skb);
		return 0;
	}

	if (fapi_is_ma(skb))
		return hip_sap_cont.sap[SAP_MA]->sap_handler(sdev, skb);

	if (fapi_is_mlme(skb))
		return hip_sap_cont.sap[SAP_MLME]->sap_handler(sdev, skb);

	if (fapi_is_debug(skb))
		return hip_sap_cont.sap[SAP_DBG]->sap_handler(sdev, skb);

	if (fapi_is_test(skb))
		return hip_sap_cont.sap[SAP_TST]->sap_handler(sdev, skb);

	return -EIO;
}

/* value used at all levels in the driver */
int slsi_hip_init(struct slsi_dev *sdev, struct device *dev)
{
	SLSI_UNUSED_PARAMETER(dev);

	memset(&sdev->hip, 0, sizeof(sdev->hip));

	sdev->hip.sdev     = sdev;
	mutex_init(&sdev->hip.hip_mutex);

	sdev->hip4_inst.hip_control = &test_hip_control;
	return 0;
}

void slsi_hip_deinit(struct slsi_dev *sdev)
{
	mutex_destroy(&sdev->hip.hip_mutex);
}

int slsi_hip_stop(struct slsi_dev *sdev)
{
	return 0;
}

int hip4_free_ctrl_slots_count(struct slsi_hip4 *hip)
{
	return HIP4_CTL_SLOTS;
}

int scsc_wifi_transmit_frame(struct slsi_hip4 *hip, struct sk_buff *skb, bool ctrl_packet, u8 vif_index, u8 peer_index, u8 priority)
{
	struct slsi_dev *sdev = container_of(hip, struct slsi_dev, hip4_inst);

	slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb, SLSI_LOG_DIRECTION_FROM_HOST);

	consume_skb(skb);

	return 0;
}

void slsi_test_bh_work_f(struct work_struct *work)
{
}

/* ALL Dummies to get UT build through goes below */
bool hip4_sampler_sample_q;
bool hip4_sampler_sample_qref;
bool hip4_sampler_sample_int;
bool hip4_sampler_sample_fapi;
bool hip4_sampler_sample_through;
bool hip4_sampler_sample_start_stop_q;
bool hip4_sampler_sample_mbulk;
bool hip4_sampler_sample_qfull;
bool hip4_sampler_sample_mfull;
bool hip4_sampler_vif;
bool hip4_sampler_bot;
bool hip4_sampler_pkt_tx;

void hip4_sampler_update_record(u32 minor, u8 param1, u8 param2, u8 param3, u8 param4)
{
}

void hip4_sampler_create(struct slsi_dev *sdev, struct scsc_mx *mx)
{
}

void hip4_sampler_destroy(struct slsi_dev *sdev, struct scsc_mx *mx)
{
}

int hip4_sampler_register_hip(struct scsc_mx *mx)
{
	return 0;
}

int scsc_wifi_fcq_ctrl_q_init(struct scsc_wifi_fcq_ctrl_q *queue)
{
	return 0;
}

void scsc_wifi_fcq_ctrl_q_deinit(struct scsc_wifi_fcq_ctrl_q *queue)
{
}

int scsc_wifi_fcq_unicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u8 qs_num, struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer)
{
	return 0;
}

int scsc_wifi_fcq_multicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u8 vif)
{
	return 0;
}

void scsc_wifi_fcq_qset_deinit(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer)
{
}

int scsc_wifi_fcq_transmit_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	return 0;
}

int scsc_wifi_fcq_receive_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	return 0;
}

int scsc_wifi_fcq_receive_data_no_peer(struct net_device *dev, u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	return 0;
}

void scsc_wifi_fcq_pause_queues(struct slsi_dev *sdev)
{
}

void scsc_wifi_fcq_unpause_queues(struct slsi_dev *sdev)
{
}

int scsc_wifi_fcq_transmit_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue)
{
	return 0;
}

int scsc_wifi_fcq_receive_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue)
{
	return 0;
}

int scsc_wifi_fcq_update_smod(struct scsc_wifi_fcq_data_qset *qs, enum scsc_wifi_fcq_ps_state peer_ps_state,
			      enum scsc_wifi_fcq_queue_set_type type)
{
	return 0;
}

int scsc_wifi_fcq_8021x_port_state(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs, enum scsc_wifi_fcq_8021x_state state)
{
	return 0;
}

int scsc_wifi_fcq_stat_queue(struct scsc_wifi_fcq_q_header *queue,
			     struct scsc_wifi_fcq_q_stat *queue_stat,
			     int *qmod, int *qcod)
{
	return 0;
}

int scsc_wifi_fcq_stat_queueset(struct scsc_wifi_fcq_data_qset *queue_set,
				struct scsc_wifi_fcq_q_stat *queue_stat,
				int *smod, int *scod, enum scsc_wifi_fcq_8021x_state *cp_state,
				u32 *peer_ps_state_transitions)
{
	return 0;
}
void slsi_hip_reprocess_skipped_data_bh(struct slsi_dev *sdev)
{

}
