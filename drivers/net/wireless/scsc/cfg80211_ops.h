/****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_CFG80211_OPS_H__
#define __SLSI_CFG80211_OPS_H__

#include <net/cfg80211.h>

struct slsi_dev;

#define SDEV_FROM_WIPHY(wiphy) ((struct slsi_dev *)(wiphy)->priv)
#define WLAN_CIPHER_SUITE_PMK 0x00904C00

#define SLSI_WPS_REQUEST_TYPE_POS               15
#define SLSI_WPS_REQUEST_TYPE_ENROLEE_INFO_ONLY 0x00
#define SLSI_WPS_OUI_PATTERN                    0x04F25000
#define SLSI_P2P_OUI_PATTERN                    0x099a6f50
#define SLSI_VENDOR_OUI_AND_TYPE_LEN            4
#define PMKID_LEN                               16
#define RSN_SELECTOR_LEN                        4
#ifdef CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
#define SLSI_ACL_MAX_BSSID_COUNT                255
#endif

struct slsi_dev *slsi_cfg80211_new(struct device *dev);
int slsi_cfg80211_register(struct slsi_dev *sdev);
void slsi_cfg80211_unregister(struct slsi_dev *sdev);
void slsi_cfg80211_free(struct slsi_dev *sdev);
void slsi_cfg80211_update_wiphy(struct slsi_dev *sdev);
int slsi_wlan_mgmt_tx(struct slsi_dev *sdev, struct net_device *dev,
		      struct ieee80211_channel *chan, unsigned int wait,
		      const u8 *buf, size_t len, bool dont_wait_for_ack, u64 *cookie);

#endif /*__SLSI_CFG80211_OPS_H__*/
