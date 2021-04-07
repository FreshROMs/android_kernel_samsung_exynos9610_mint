/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SAP_H__
#define __SAP_H__

/* Number of SAPs */
#define SAP_TOTAL       4

#define SAP_MLME        0
#define SAP_MA          1
#define SAP_DBG         2
#define SAP_TST         3

/* Max number of versions supported */
#define SAP_MAX_VER     2

#define SAP_MAJOR(version)      ((version & 0xff00) >> 8)
#define SAP_MINOR(version)      (version & 0xff)

#define SAP_DRV_SIGNAL_BASE          0xA000
#define SAP_DRV_MA_TO_MLME_DELBA_REQ (SAP_DRV_SIGNAL_BASE + 1)

struct sap_drv_ma_to_mlme_delba_req {
	struct fapi_signal_header header;
	__le16 vif;
	u8     peer_qsta_address[ETH_ALEN];
	__le16 sequence_number;
	__le16 user_priority;
	__le16 reason;
	__le16 direction;
} __attribute__((packed));

struct slsi_dev;
struct sk_buff;

struct sap_api {
	u8  sap_class;
	u16 sap_versions[SAP_MAX_VER];
	int (*sap_version_supported)(u16 version);
	int (*sap_handler)(struct slsi_dev *sdev, struct sk_buff *skb);
	int (*sap_txdone)(struct slsi_dev *sdev, u8 vif, u8 peer_index, u8 ac);
	int (*sap_notifier)(struct slsi_dev *sdev, unsigned long event);
};
#endif
