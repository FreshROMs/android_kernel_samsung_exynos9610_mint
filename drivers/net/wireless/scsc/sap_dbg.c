/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/types.h>
#include "debug.h"
#include "dev.h"
#include "sap.h"
#include "sap_dbg.h"
#include "hip.h"

#define SUPPORTED_OLD_VERSION   0

static int sap_dbg_version_supported(u16 version);
static int sap_dbg_rx_handler(struct slsi_dev *sdev, struct sk_buff *skb);

static struct sap_api sap_dbg = {
	.sap_class = SAP_DBG,
	.sap_version_supported = sap_dbg_version_supported,
	.sap_handler = sap_dbg_rx_handler,
	.sap_versions = { FAPI_DEBUG_SAP_VERSION, SUPPORTED_OLD_VERSION },
};

static int sap_dbg_version_supported(u16 version)
{
	unsigned int major = SAP_MAJOR(version);
	unsigned int minor = SAP_MINOR(version);
	u8           i = 0;

	SLSI_INFO_NODEV("Reported version: %d.%d\n", major, minor);

	for (i = 0; i < SAP_MAX_VER; i++)
		if (SAP_MAJOR(sap_dbg.sap_versions[i]) == major)
			return 0;

	SLSI_ERR_NODEV("Version %d.%d Not supported\n", major, minor);

	return -EINVAL;
}

static void slsi_rx_debug(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	u16 id = fapi_get_u16(skb, id);

	SLSI_UNUSED_PARAMETER(dev);

	switch (id) {
	case DEBUG_FAULT_IND:
		SLSI_WARN(sdev, "WF_FW_INFO: |cpu %s|id 0x%04X|arg 0x%08X|count %d|timestamp %10u|\n",
			  ((fapi_get_u16(skb, u.debug_fault_ind.cpu) == 0x8000) ? "MAC" :
			   (fapi_get_u16(skb, u.debug_fault_ind.cpu) == 0x4000) ? "PHY" : "???"),
			  fapi_get_u16(skb, u.debug_fault_ind.faultid),
			  fapi_get_u32(skb, u.debug_fault_ind.arg),
			  fapi_get_u16(skb, u.debug_fault_ind.count),
			  fapi_get_u32(skb, u.debug_fault_ind.timestamp));
		break;
	case DEBUG_WORD12IND:
		atomic_inc(&sdev->debug_inds);
		SLSI_DBG4(sdev, SLSI_FW_TEST, "FW DEBUG(id:%d, subid:%d, vif:%d, time:%u) %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X\n",
			  fapi_get_u16(skb, u.debug_word12_ind.module_id),
			  fapi_get_u16(skb, u.debug_word12_ind.module_sub_id),
			  fapi_get_vif(skb),
			  fapi_get_u32(skb, u.debug_word12_ind.timestamp),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[0]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[1]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[2]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[3]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[4]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[5]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[6]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[7]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[8]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[9]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[10]),
			  fapi_get_u16(skb, u.debug_word12_ind.debug_words[11]));
		break;
	default:
		SLSI_DBG1(sdev, SLSI_MLME, "Unhandled Debug Ind: 0x%.4x\n", id);
		break;
	}
	kfree_skb(skb);
}

static int slsi_rx_dbg_sap(struct slsi_dev *sdev, struct sk_buff *skb)
{
	u16 id = fapi_get_u16(skb, id);
	u16 vif = fapi_get_vif(skb);
	struct net_device *dev;

	switch (id) {
	case DEBUG_FAULT_IND:
	case DEBUG_WORD12IND:
	case DEBUG_GENERIC_IND:
		slsi_rx_debug(sdev, NULL, skb);
		break;
	case DEBUG_PKT_SINK_REPORT_IND:
		{
			rcu_read_lock();
			dev = slsi_get_netdev_rcu(sdev, vif);
			if (!dev) {
				rcu_read_unlock();
				kfree_skb(skb);
				break;
			}
			slsi_rx_sink_report(sdev, dev, skb);
			rcu_read_unlock();
			break;
		}
	case DEBUG_PKT_GEN_REPORT_IND:
		{
			rcu_read_lock();
			dev = slsi_get_netdev_rcu(sdev, vif);
			if (!dev) {
				rcu_read_unlock();
				kfree_skb(skb);
				break;
			}
			slsi_rx_gen_report(sdev, dev, skb);
			rcu_read_unlock();
			break;
		}
	default:
		kfree_skb(skb);
		SLSI_ERR(sdev, "Unhandled Ind: 0x%.4x\n", id);
		break;
	}

	return 0;
}

void slsi_rx_dbg_sap_work(struct work_struct *work)
{
	struct slsi_skb_work *w = container_of(work, struct slsi_skb_work, work);
	struct slsi_dev *sdev = w->sdev;
	struct sk_buff *skb = slsi_skb_work_dequeue(w);

	slsi_wake_lock(&sdev->wlan_wl);
	while (skb) {
		slsi_debug_frame(sdev, NULL, skb, "RX");
		slsi_rx_dbg_sap(sdev, skb);
		skb = slsi_skb_work_dequeue(w);
	}
	slsi_wake_unlock(&sdev->wlan_wl);
}

static int sap_dbg_rx_handler(struct slsi_dev *sdev, struct sk_buff *skb)
{
	/* DEBUG SAP has a generic confirm. Theoretically, that
	 * can mean upper layer code can block on the confirm.
	 */
	if (slsi_rx_blocking_signals(sdev, skb) == 0)
		return 0;

	slsi_skb_work_enqueue(&sdev->rx_dbg_sap, skb);
	return 0;
}

int sap_dbg_init(void)
{
	SLSI_INFO_NODEV("Registering SAP\n");

	slsi_hip_sap_register(&sap_dbg);

	return 0;
}

int sap_dbg_deinit(void)
{
	SLSI_INFO_NODEV("Unregistering SAP\n");
	slsi_hip_sap_unregister(&sap_dbg);
	return 0;
}
