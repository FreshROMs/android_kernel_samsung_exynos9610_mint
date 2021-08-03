/***************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/version.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>
#include "dev.h"
#include "cfg80211_ops.h"
#include "debug.h"
#include "mgt.h"
#include "mlme.h"
#include "netif.h"
#include "unifiio.h"
#include "mib.h"

#ifdef CONFIG_SCSC_WLAN_ANDROID
#include "scsc_wifilogger_rings.h"
#endif
#include "nl80211_vendor.h"

#define SLSI_MAX_CHAN_2G_BAND          14

/* Ext capab is decided by firmware. But there are certain bits
 * which are set by supplicant. So we set the capab and mask in
 * such way so that supplicant sets only the bits our solution supports
 */

static const u8                    slsi_extended_cap[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8                    slsi_extended_cap_mask[] = {
	0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static uint keep_alive_period = SLSI_P2PGO_KEEP_ALIVE_PERIOD_SEC;
module_param(keep_alive_period, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(keep_alive_period, "default is 10 seconds");

static bool slsi_is_mhs_active(struct slsi_dev *sdev)
{
	struct net_device *mhs_dev = sdev->netdev_ap;
	struct netdev_vif *ndev_vif;
	bool ret;

	if (mhs_dev) {
		ndev_vif = netdev_priv(mhs_dev);
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
		ret = ndev_vif->is_available;
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return ret;
	}

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
struct wireless_dev *slsi_add_virtual_intf(struct wiphy        *wiphy,
					   const char          *name,
					   unsigned char       name_assign_type,
					   enum nl80211_iftype type,
					   struct vif_params   *params)
{
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
struct wireless_dev *slsi_add_virtual_intf(struct wiphy        *wiphy,
					   const char          *name,
					   unsigned char       name_assign_type,
					   enum nl80211_iftype type,
					   u32                 *flags,
					   struct vif_params   *params)
{
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
struct wireless_dev *slsi_add_virtual_intf(struct wiphy        *wiphy,
					   const char          *name,
					   enum nl80211_iftype type,
					   u32                 *flags,
					   struct vif_params   *params)
{
#else
struct net_device *slsi_add_ virtual_intf(struct wiphy        *wiphy,
					 char                *name,
					 enum nl80211_iftype type,
					 u32                 *flags,
					 struct vif_params   *params)
{
#endif

	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 11, 0))
	SLSI_UNUSED_PARAMETER(flags);
#endif
	SLSI_NET_DBG1(dev, SLSI_CFG80211, "Intf name:%s, type:%d, macaddr:%pM\n", name, type, params->macaddr);
	if (slsi_is_mhs_active(sdev)) {
		SLSI_ERR(sdev, "MHS is active. cannot add new interface\n");
		return ERR_PTR(-EOPNOTSUPP);
	}
	dev = slsi_dynamic_interface_create(wiphy, name, type, params);
	if (!dev)
		goto exit_with_error;
	ndev_vif = netdev_priv(dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	return &ndev_vif->wdev;
#else
	return dev;
#endif  /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9)) */

exit_with_error:
	return ERR_PTR(-ENODEV);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
int slsi_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct net_device *dev = wdev->netdev;

#else
int slsi_del_virtual_intf(struct wiphy *wiphy, struct net_device *dev)
{
#endif
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);

	if (WARN_ON(!dev))
		return -EINVAL;

	SLSI_NET_DBG1(dev, SLSI_CFG80211, "Dev name:%s\n", dev->name);

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	slsi_stop_net_dev(sdev, dev);
	slsi_netif_remove_locked(sdev, dev);
	if (dev == sdev->netdev_ap)
		rcu_assign_pointer(sdev->netdev_ap, NULL);
	if (!sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN])
		rcu_assign_pointer(sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN], sdev->netdev_ap);
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
int slsi_change_virtual_intf(struct wiphy *wiphy,
			     struct net_device *dev,
			     enum nl80211_iftype type,
			     struct vif_params *params)
{
#else
int slsi_change_virtual_intf(struct wiphy *wiphy,
			     struct net_device *dev,
			     enum nl80211_iftype type,
			     u32 *flags,
			     struct vif_params *params)
{
#endif

	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               r = 0;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 11, 0))
	SLSI_UNUSED_PARAMETER(flags);
#endif

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG1(dev, SLSI_CFG80211, "type:%u, iftype:%d\n", type, ndev_vif->iftype);

	if (WARN_ON(ndev_vif->activated)) {
		r = -EINVAL;
		goto exit;
	}

	switch (type) {
	case NL80211_IFTYPE_UNSPECIFIED:
	case NL80211_IFTYPE_ADHOC:
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_MONITOR:
		ndev_vif->iftype = type;
		dev->ieee80211_ptr->iftype = type;
		if (params)
			dev->ieee80211_ptr->use_4addr = params->use_4addr;
		break;
	default:
		r = -EINVAL;
		break;
	}

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

int slsi_add_key(struct wiphy *wiphy, struct net_device *dev,
		 u8 key_index, bool pairwise, const u8 *mac_addr,
		 struct key_params *params)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer  *peer = NULL;
	int               r = 0;
	u16               key_type = FAPI_KEYTYPE_GROUP;

	if (WARN_ON(pairwise && !mac_addr))
		return -EINVAL;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "(key_index:%d, pairwise:%d, address:%pM, cipher:0x%.8X, key_len:%d,"
		      "vif_type:%d)\n", key_index, pairwise, mac_addr, params->cipher, params->key_len,
		      ndev_vif->vif_type);

	if (!ndev_vif->activated) {
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "vif not active\n");
		goto exit;
	}

	if (params->cipher == WLAN_CIPHER_SUITE_PMK) {
		r = slsi_mlme_set_pmk(sdev, dev, params->key, params->key_len);
		goto exit;
	}

	if (mac_addr && pairwise) {
		/* All Pairwise Keys will have a peer record. */
		peer = slsi_get_peer_from_mac(sdev, dev, mac_addr);
		if (peer)
			mac_addr = peer->address;
	} else if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		/* Sta Group Key will use the peer address */
		peer = slsi_get_peer_from_qs(sdev, dev, SLSI_STA_PEER_QUEUESET);
		if (peer)
			mac_addr = peer->address;
	} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && !pairwise)
		/* AP Group Key will use the Interface address */
		mac_addr = dev->dev_addr;
	else {
		r = -EINVAL;
		goto exit;
	}

	/*Treat WEP key as pairwise key*/
	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION &&
	    (params->cipher == WLAN_CIPHER_SUITE_WEP40 ||
	     params->cipher == WLAN_CIPHER_SUITE_WEP104) && peer) {
		u8 bc_mac_addr[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

		SLSI_NET_DBG3(dev, SLSI_CFG80211, "WEP Key: store key\n");
		r = slsi_mlme_set_key(sdev, dev, key_index, FAPI_KEYTYPE_WEP, bc_mac_addr, params);
		if (r == FAPI_RESULTCODE_SUCCESS) {
			ndev_vif->sta.wep_key_set = true;
			/* if static ip is set before connection, after setting keys enable powersave. */
			if (ndev_vif->ipaddress)
				slsi_mlme_powermgt(sdev, dev, ndev_vif->set_power_mode);
		} else {
			SLSI_NET_ERR(dev, "Error adding WEP key\n");
		}
		goto exit;
	}

	if (pairwise) {
		key_type = FAPI_KEYTYPE_PAIRWISE;
		if (WARN_ON(!peer)) {
			r = -EINVAL;
			goto exit;
		}
	} else if (params->cipher == WLAN_CIPHER_SUITE_AES_CMAC || params->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_128 ||
				params->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_256) {
		key_type = FAPI_KEYTYPE_IGTK;
	}

	if (WARN(!mac_addr, "mac_addr not defined\n")) {
		r = -EINVAL;
		goto exit;
	}
	if (!(ndev_vif->vif_type == FAPI_VIFTYPE_AP && key_index == 4)) {
		r = slsi_mlme_set_key(sdev, dev, key_index, key_type, mac_addr, params);
		if (r) {
			SLSI_NET_ERR(dev, "error in adding key (key_type: %d)\n", key_type);
			goto exit;
		}
	}

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		ndev_vif->sta.eap_hosttag = 0xFFFF;
		/* if static IP is set before connection, after setting keys enable powersave. */
		if (ndev_vif->ipaddress)
			slsi_mlme_powermgt(sdev, dev, ndev_vif->set_power_mode);
	}

	if (key_type == FAPI_KEYTYPE_GROUP) {
		ndev_vif->sta.group_key_set = true;
		ndev_vif->ap.cipher = params->cipher;
	} else if (key_type == FAPI_KEYTYPE_PAIRWISE) {
		if (peer)
			peer->pairwise_key_set = true;
	}

	if (peer) {
		if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
			if (pairwise) {
				if (params->cipher == WLAN_CIPHER_SUITE_SMS4) { /*WAPI */
					slsi_mlme_connect_resp(sdev, dev);
					slsi_set_packet_filters(sdev, dev);
					slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_CONNECTED);
				}
			}

			if (ndev_vif->sta.gratuitous_arp_needed) {
				ndev_vif->sta.gratuitous_arp_needed = false;
				slsi_send_gratuitous_arp(sdev, dev);
			}
		} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && pairwise) {
			slsi_mlme_connected_resp(sdev, dev, peer->aid);
			slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_CONNECTED);
			peer->connected_state = SLSI_STA_CONN_STATE_CONNECTED;
			if (ndev_vif->iftype == NL80211_IFTYPE_P2P_GO)
				ndev_vif->ap.p2p_gc_keys_set = true;
		}
	}

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

int slsi_del_key(struct wiphy *wiphy, struct net_device *dev,
		 u8 key_index, bool pairwise, const u8 *mac_addr)
{
	SLSI_UNUSED_PARAMETER(wiphy);
	SLSI_UNUSED_PARAMETER(key_index);
	SLSI_UNUSED_PARAMETER(pairwise);
	SLSI_UNUSED_PARAMETER(mac_addr);

	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, WlanLite FW does not support MLME_DELETEKEYS.request\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

int slsi_channel_switch(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_csa_settings *params)
{
	struct netdev_vif        *ndev_vif = netdev_priv(dev);
	struct netdev_vif        *ap_dev_vif;
	struct slsi_dev          *sdev = ndev_vif->sdev;
	struct net_device        *wlan_dev;
	struct netdev_vif        *ndev_sta_vif;
	int                      result = 0;
	u16                      center_freq = 0;
	u16                      chan_info = 0;
	u16                      current_chan_info = 0;
	struct cfg80211_chan_def chandef = params->chandef;
	struct cfg80211_chan_def current_chandef = ndev_vif->chandef_saved;
	struct net_device        *ap_dev = NULL;
	struct ieee80211_channel *chan = chandef.chan;
	int                      width  = chandef.width;

	wlan_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (ndev_vif->iftype != NL80211_IFTYPE_AP) {
		SLSI_NET_ERR(dev, "AP Mode is not active\n");
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return -EPERM;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	ndev_sta_vif = netdev_priv(wlan_dev);
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	if (SLSI_IS_VIF_INDEX_MHS(sdev, ndev_vif)) {
		if (ndev_sta_vif) {
			SLSI_MUTEX_LOCK(ndev_sta_vif->vif_mutex);
			if (ndev_sta_vif->activated && ndev_sta_vif->vif_type == FAPI_VIFTYPE_STATION &&
			    (ndev_sta_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTING ||
			     ndev_sta_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED)) {
				SLSI_NET_ERR(dev, "Sta is in connected state (Chan Switch not allowed in WiFi Sharing mode)\n");
				SLSI_MUTEX_UNLOCK(ndev_sta_vif->vif_mutex);
				return -EPERM;
			}
			SLSI_MUTEX_UNLOCK(ndev_sta_vif->vif_mutex);
		}
	}
#endif

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (current_chandef.chan->center_freq == chan->center_freq) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
		current_chan_info = slsi_get_chann_info(sdev, &current_chandef);
#else
		current_chan_info = slsi_get_chann_info(sdev, ndev_vif->channel_type);
#endif
		if (current_chan_info == slsi_get_chann_info(sdev, &chandef)) {
			SLSI_NET_ERR(dev, "Channel Switch requested on current channel freq-> %u and Chan info %d\n", current_chandef.chan->center_freq, current_chan_info);
			SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
			return -EPERM;
			}
		}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);

	ap_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	chan_info = slsi_get_chann_info(sdev, &chandef);
	center_freq = chan->center_freq;
	if (width != NL80211_CHAN_WIDTH_20_NOHT && width != NL80211_CHAN_WIDTH_20)
		center_freq = slsi_get_center_freq1(sdev, chan_info, center_freq);
	ap_dev_vif = netdev_priv(ap_dev);
	SLSI_MUTEX_LOCK(ap_dev_vif->vif_mutex);
	result = slsi_mlme_channel_switch(sdev, ap_dev, center_freq, chan_info);
	SLSI_MUTEX_UNLOCK(ap_dev_vif->vif_mutex);
	return result;
}

int slsi_get_key(struct wiphy *wiphy, struct net_device *dev,
		 u8 key_index, bool pairwise, const u8 *mac_addr,
		 void *cookie,
		 void (*callback)(void *cookie, struct key_params *))
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct key_params params;

#define SLSI_MAX_KEY_SIZE 8 /*used only for AP case, so WAPI not considered*/
	u8                key_seq[SLSI_MAX_KEY_SIZE] = { 0 };
	int               r = 0;

	SLSI_UNUSED_PARAMETER(mac_addr);

	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, WlanLite FW does not support MLME_GET_KEY_SEQUENCE.request\n");
		return -EOPNOTSUPP;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "(key_index:%d, pairwise:%d, mac_addr:%pM, vif_type:%d)\n", key_index,
		      pairwise, mac_addr, ndev_vif->vif_type);

	if (!ndev_vif->activated) {
		SLSI_NET_ERR(dev, "vif not active\n");
		r = -EINVAL;
		goto exit;
	}

	/* The get_key call is expected only for AP vif with Group Key type */
	if (FAPI_VIFTYPE_AP != ndev_vif->vif_type) {
		SLSI_NET_ERR(dev, "Invalid vif type: %d\n", ndev_vif->vif_type);
		r = -EINVAL;
		goto exit;
	}
	if (pairwise) {
		SLSI_NET_ERR(dev, "Invalid key type\n");
		r = -EINVAL;
		goto exit;
	}

	memset(&params, 0, sizeof(params));
	/* Update params with sequence number, key field would be updated NULL */
	params.key = NULL;
	params.key_len = 0;
	params.cipher = ndev_vif->ap.cipher;
	if (!(ndev_vif->vif_type == FAPI_VIFTYPE_AP && key_index == 4)) {
		r = slsi_mlme_get_key(sdev, dev, key_index, FAPI_KEYTYPE_GROUP, key_seq, &params.seq_len);

		if (!r) {
			params.seq = key_seq;
			callback(cookie, &params);
		}
	}
#undef SLSI_MAX_KEY_SIZE
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

static size_t slsi_strip_wsc_p2p_ie(const u8 *src_ie, size_t src_ie_len, u8 *dest_ie, bool strip_wsc, bool strip_p2p)
{
	const u8 *ie;
	const u8 *next_ie;
	size_t   dest_ie_len = 0;

	if (!dest_ie || !(strip_p2p || strip_wsc))
		return dest_ie_len;

	for (ie = src_ie; (ie - src_ie) < src_ie_len; ie = next_ie) {
		next_ie = ie + ie[1] + 2;

		if (ie[0] == WLAN_EID_VENDOR_SPECIFIC && ie[1] > 4) {
			int          i;
			unsigned int oui = 0;

			for (i = 0; i < 4; i++)
				oui = (oui << 8) | ie[5 - i];

			if (strip_wsc && oui == SLSI_WPS_OUI_PATTERN)
				continue;
			if (strip_p2p && oui == SLSI_P2P_OUI_PATTERN)
				continue;
		}

		if (next_ie - src_ie <= src_ie_len) {
			memcpy(dest_ie + dest_ie_len, ie, ie[1] + 2);
			dest_ie_len += ie[1] + 2;
		}
	}

	return dest_ie_len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
int slsi_scan(struct wiphy                 *wiphy,
	      struct cfg80211_scan_request *request)
{
	struct net_device *dev = request->wdev->netdev;

#else
int slsi_scan(struct wiphy *wiphy, struct net_device *dev,
	      struct cfg80211_scan_request *request)
{
#endif  /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9)) */

	struct netdev_vif         *ndev_vif = netdev_priv(dev);
	struct slsi_dev           *sdev = SDEV_FROM_WIPHY(wiphy);
	u16                       scan_type = FAPI_SCANTYPE_FULL_SCAN;
	int                       r = 0;
	u8                        *scan_ie;
	size_t                    scan_ie_len;
	bool                      strip_wsc = false;
	bool                      strip_p2p = false;
	struct ieee80211_channel  *channels[64];
	int                       i, chan_count = 0;
	bool                      wps_sta = false;

#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
	u8 mac_addr_mask[ETH_ALEN] = {0xFF};
#endif

	if (WARN_ON(!request->wdev))
		return -EINVAL;
	if (WARN_ON(!dev))
		return -EINVAL;

	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_WARN(dev, "not supported in WlanLite mode\n");
		return -EOPNOTSUPP;
	}

	/* Reject scan request if Group Formation is in progress */
	if (sdev->p2p_state == P2P_ACTION_FRAME_TX_RX) {
		SLSI_NET_INFO(dev, "Scan received in P2P Action Frame Tx/Rx state - Reject\n");
		return -EBUSY;
	}

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	if (ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req) {
		SLSI_NET_INFO(dev, "Rejecting scan request as last scan is still running\n");
		r = -EBUSY;
		goto exit;
	}
#if defined(CONFIG_SLSI_WLAN_STA_FWD_BEACON) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
	if (ndev_vif->is_wips_running && (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) &&
	    (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED)) {
		int ret = 0;

		SLSI_NET_DBG3(dev, SLSI_CFG80211, "Scan invokes DRIVER_BCN_ABORT\n");
		ret = slsi_mlme_set_forward_beacon(sdev, dev, FAPI_ACTION_STOP);

		if (!ret) {
			ret = slsi_send_forward_beacon_abort_vendor_event(sdev,
									  SLSI_FORWARD_BEACON_ABORT_REASON_SCANNING);
		}
	}
#endif
	SLSI_NET_DBG3(dev, SLSI_CFG80211, "channels:%d, ssids:%d, ie_len:%d, vif_index:%d\n", request->n_channels,
		      request->n_ssids, (int)request->ie_len, ndev_vif->ifnum);

	for (i = 0; i < request->n_channels; i++)
		channels[i] = request->channels[i];
	chan_count = request->n_channels;

	if (SLSI_IS_VIF_INDEX_WLAN(ndev_vif)) {
		if (chan_count == 1)
			scan_type = FAPI_SCANTYPE_SINGLE_CHANNEL_SCAN;
		else if (sdev->initial_scan) {
			sdev->initial_scan = false;
			scan_type = FAPI_SCANTYPE_INITIAL_SCAN;
		}
		ndev_vif->unsync.slsi_p2p_continuous_fullscan = false;
	}

	/* Update scan timing for P2P social channels scan.*/
	if (request->ie &&
	    cfg80211_find_vendor_ie(WLAN_OUI_WFA, WLAN_OUI_TYPE_WFA_P2P, request->ie, request->ie_len) &&
	    request->ssids && SLSI_IS_P2P_SSID(request->ssids[0].ssid, request->ssids[0].ssid_len)) {
		/* In supplicant during joining procedure the P2P GO scan
		 * with GO's operating channel comes on P2P device. Hence added the
		 * check for n_channels as 1
		 */
		if (!ndev_vif->drv_in_p2p_procedure) {
			if (delayed_work_pending(&ndev_vif->unsync.unset_channel_expiry_work)) {
				cancel_delayed_work(&ndev_vif->unsync.unset_channel_expiry_work);
				slsi_mlme_unset_channel_req(sdev, dev);
				ndev_vif->driver_channel = 0;
				}
		}
		/* If an AP (GO) interface is on, should not convert full scan to
		 * Social scan
		 */
		if (sdev->p2p_state == P2P_GROUP_FORMED_GO)
			ndev_vif->unsync.slsi_p2p_continuous_fullscan = false;

		if (request->n_channels == SLSI_P2P_SOCIAL_CHAN_COUNT || request->n_channels == 1) {
			scan_type = FAPI_SCANTYPE_P2P_SCAN_SOCIAL;
			ndev_vif->unsync.slsi_p2p_continuous_fullscan = false;
		} else if (request->n_channels > SLSI_P2P_SOCIAL_CHAN_COUNT) {
			if (!ndev_vif->unsync.slsi_p2p_continuous_fullscan) {
				scan_type = FAPI_SCANTYPE_P2P_SCAN_FULL;
				ndev_vif->unsync.slsi_p2p_continuous_fullscan = true;
			} else {
				int count = 0, chann = 0;

				scan_type = FAPI_SCANTYPE_P2P_SCAN_SOCIAL;
				ndev_vif->unsync.slsi_p2p_continuous_fullscan = false;
				for (i = 0; i < request->n_channels; i++) {
					chann = channels[i]->hw_value & 0xFF;
					if (chann == 1 || chann == 6 || chann == 11) {
						channels[count] = request->channels[i];
						count++;
					}
				}
				chan_count = count;
			}
		}
	}

#ifdef CONFIG_SCSC_WLAN_DUAL_STATION
	if ((SLSI_IS_VIF_INDEX_WLAN(ndev_vif) || ndev_vif->ifnum == SLSI_NET_INDEX_P2PX_SWLAN) && request->ie) {
#else
	if (SLSI_IS_VIF_INDEX_WLAN(ndev_vif) && request->ie) {
#endif
		const u8 *ie;

		/* Supplicant adds wsc and p2p in Station scan at the end of scan request ie.
		 * for non-wps case remove both wps and p2p IEs
		 * for wps case remove only p2p IE
		 */

		ie = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPS, request->ie, request->ie_len);
		if (ie && ie[1] > SLSI_WPS_REQUEST_TYPE_POS) {
			/* Check whether scan is wps_scan or not, if not a wps_scan set strip_wsc to true
			 * to strip WPS IE else wps_sta to true to disable mac radomization for wps_scan
			 */
			if (ie[SLSI_WPS_REQUEST_TYPE_POS] == SLSI_WPS_REQUEST_TYPE_ENROLEE_INFO_ONLY)
				strip_wsc = true;
			else
				wps_sta = true;
		}

		ie = cfg80211_find_vendor_ie(WLAN_OUI_WFA, WLAN_OUI_TYPE_WFA_P2P, request->ie, request->ie_len);
		if (ie)
			strip_p2p = true;
	}

	if (strip_wsc || strip_p2p) {
		scan_ie = kmalloc(request->ie_len, GFP_KERNEL);
		if (!scan_ie) {
			SLSI_NET_INFO(dev, "Out of memory for scan IEs\n");
			r = -ENOMEM;
			goto exit;
		}
		scan_ie_len = slsi_strip_wsc_p2p_ie(request->ie, request->ie_len, scan_ie, strip_wsc, strip_p2p);
	} else {
		scan_ie = (u8 *)request->ie;
		scan_ie_len = request->ie_len;
	}

	/* Flush out any outstanding single scan timeout work */
	cancel_delayed_work(&ndev_vif->scan_timeout_work);

	ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan = false;
	slsi_purge_scan_results(ndev_vif, SLSI_SCAN_HW_ID);

#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	/* If Supplicant triggers WPS scan on station interface,
	 * mac radomization for scan should be disbaled to avoid WPS overlap.
	 * Firmware also disables Mac Randomization for WPS Scan.
	 */
	if (request->flags & NL80211_SCAN_FLAG_RANDOM_ADDR && !wps_sta) {
		if (sdev->fw_mac_randomization_enabled) {
			memcpy(sdev->scan_mac_addr, request->mac_addr, ETH_ALEN);
			r = slsi_set_mac_randomisation_mask(sdev, request->mac_addr_mask);
			if (!r)
				sdev->scan_addr_set = 1;
		} else {
			SLSI_NET_INFO(dev, "Mac Randomization is not enabled in Firmware\n");
			sdev->scan_addr_set = 0;
		}
	} else
#endif
		if (sdev->scan_addr_set) {
			memset(mac_addr_mask, 0xFF, ETH_ALEN);
			r = slsi_set_mac_randomisation_mask(sdev, mac_addr_mask);
			sdev->scan_addr_set = 0;
		}
#endif

	r = slsi_mlme_add_scan(sdev,
			       dev,
			       scan_type,
			       FAPI_REPORTMODE_REAL_TIME,
			       request->n_ssids,
			       request->ssids,
			       chan_count,
			       channels,
			       NULL,
			       scan_ie,
			       scan_ie_len,
			       ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan);

	if (r != 0) {
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "add_scan error: %d\n", r);
		r = -EIO;
	} else {
		ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = request;

		/* if delayed work is already scheduled, queue delayed work fails. So set
		 * requeue_timeout_work flag to enqueue delayed work in the timeout handler
		 */
		if (queue_delayed_work(sdev->device_wq, &ndev_vif->scan_timeout_work,
				       msecs_to_jiffies(SLSI_FW_SCAN_DONE_TIMEOUT_MSEC)))
			ndev_vif->scan[SLSI_SCAN_HW_ID].requeue_timeout_work = false;
		else
			ndev_vif->scan[SLSI_SCAN_HW_ID].requeue_timeout_work = true;

		/* Update State only for scan in Device role */
		if (SLSI_IS_VIF_INDEX_P2P(ndev_vif) && (!SLSI_IS_P2P_GROUP_STATE(sdev))) {
			if (scan_type == FAPI_SCANTYPE_P2P_SCAN_SOCIAL)
				SLSI_P2P_STATE_CHANGE(sdev, P2P_SCANNING);
		} else if (!SLSI_IS_VIF_INDEX_P2P(ndev_vif) && scan_ie_len) {
			kfree(ndev_vif->probe_req_ies);
			ndev_vif->probe_req_ies = kmalloc(request->ie_len, GFP_KERNEL);
			if (!ndev_vif->probe_req_ies) /* Don't fail, continue as it would still work */
				ndev_vif->probe_req_ie_len = 0;
			else {
				ndev_vif->probe_req_ie_len = scan_ie_len;
				memcpy(ndev_vif->probe_req_ies, scan_ie, scan_ie_len);
			}
		}
	}
	if (strip_p2p || strip_wsc)
		kfree(scan_ie);

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	return r;
}

int slsi_sched_scan_start(struct wiphy                       *wiphy,
			  struct net_device                  *dev,
			  struct cfg80211_sched_scan_request *request)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	int               r;
	u8                *scan_ie;
	size_t            scan_ie_len;
	bool              strip_wsc = false;
	bool              strip_p2p = false;

	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, WlanLite FW does not support MLME_ADD_SCAN.request\n");
		return -EOPNOTSUPP;
	}

	/* Allow sched_scan only on wlan0. For P2PCLI interface, sched_scan might get requested following a
	 * wlan0 scan and its results being shared to sibling interfaces. Reject sched_scan for other interfaces.
	 */
	if (!SLSI_IS_VIF_INDEX_WLAN(ndev_vif)) {
		SLSI_NET_INFO(dev, "Scheduled scan req received on vif %d - Reject\n", ndev_vif->ifnum);
		return -EINVAL;
	}

	/* Unlikely to get a schedule scan while Group formation is in progress.
	 * In case it is requested, it will be rejected.
	 */
	if (sdev->p2p_state == P2P_ACTION_FRAME_TX_RX) {
		SLSI_NET_INFO(dev, "Scheduled scan req received in P2P Action Frame Tx/Rx state - Reject\n");
		return -EBUSY;
	}

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "channels:%d, ssids:%d, ie_len:%d, vif_index:%d\n", request->n_channels,
		      request->n_ssids, (int)request->ie_len, ndev_vif->ifnum);

	if (ndev_vif->scan[SLSI_SCAN_HW_ID].sched_req) {
		r = -EBUSY;
		goto exit;
	}

	if (request->ie) {
		const u8 *ie;
		/* Supplicant adds wsc and p2p in Station scan at the end of scan request ie.
		 * Remove both wps and p2p IEs.
		 * Scheduled scan is not used for wsc, So no need to check for wsc request type
		 */

		ie = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPS, request->ie, request->ie_len);
		if (ie)
			strip_wsc = true;

		ie = cfg80211_find_vendor_ie(WLAN_OUI_WFA, WLAN_OUI_TYPE_WFA_P2P, request->ie, request->ie_len);
		if (ie)
			strip_p2p = true;
	}

	if (strip_wsc || strip_p2p) {
		scan_ie = kmalloc(request->ie_len, GFP_KERNEL);
		if (!scan_ie) {
			SLSI_NET_INFO(dev, "Out of memory for scan IEs\n");
			r = -ENOMEM;
			goto exit;
		}
		scan_ie_len = slsi_strip_wsc_p2p_ie(request->ie, request->ie_len, scan_ie, strip_wsc, strip_p2p);
	} else {
		scan_ie = (u8 *)request->ie;
		scan_ie_len = request->ie_len;
	}

	slsi_purge_scan_results(ndev_vif, SLSI_SCAN_SCHED_ID);
	r = slsi_mlme_add_sched_scan(sdev, dev, request, scan_ie, scan_ie_len);

	ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req = request;
	if (strip_p2p || strip_wsc)
		kfree(scan_ie);

	if (r != 0) {
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "add_scan error: %d\n", r);
		ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req = NULL;
		r = -EIO;
	}

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
int slsi_sched_scan_stop(struct wiphy *wiphy, struct net_device *dev, u64 reqid)
{
#else
int slsi_sched_scan_stop(struct wiphy *wiphy, struct net_device *dev)
{
#endif
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	int               r = 0;

	SLSI_UNUSED_PARAMETER(reqid);
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	SLSI_NET_DBG1(dev, SLSI_CFG80211, "vif_index:%d", ndev_vif->ifnum);
	if (!ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req) {
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "No sched scan req\n");
		goto exit;
	}

	r = slsi_mlme_del_scan(sdev, dev, (ndev_vif->ifnum << 8 | SLSI_SCAN_SCHED_ID), false);

	ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req = NULL;

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	return r;
}

static void slsi_abort_hw_scan(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	SLSI_NET_DBG1(dev, SLSI_CFG80211, "Abort on-going scan, vif_index:%d,"
		      "ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req:%p\n", ndev_vif->ifnum,
		      ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req);

	if (ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req) {
		(void)slsi_mlme_del_scan(sdev, dev, ndev_vif->ifnum << 8 | SLSI_SCAN_HW_ID, false);
		slsi_scan_complete(sdev, dev, SLSI_SCAN_HW_ID, false, false);
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
void slsi_save_connection_params(struct slsi_dev *sdev, struct net_device *dev,
				 struct cfg80211_connect_params *sme,
				 struct ieee80211_channel *channel, const u8 *bssid,
				 u32 action_frame_bmap, u32 action_frame_suspend_bmap)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	memset(&ndev_vif->sta.sme, 0, sizeof(struct cfg80211_connect_params));
	ndev_vif->sta.sme = *sme;

	if (sme->ie && sme->ie_len)
		ndev_vif->sta.sme.ie = slsi_mem_dup((u8 *)sme->ie, sme->ie_len);
	else
		ndev_vif->sta.sme.ie = NULL;

	if (sme->ssid && sme->ssid_len)
		ndev_vif->sta.sme.ssid = slsi_mem_dup((u8 *)sme->ssid, sme->ssid_len);
	else
		ndev_vif->sta.sme.ssid = NULL;

	if (sme->key && sme->key_len)
		ndev_vif->sta.sme.key = slsi_mem_dup((u8 *)sme->key, sme->key_len);
	else
		ndev_vif->sta.sme.key = NULL;

	ndev_vif->sta.action_frame_bmap = action_frame_bmap;
	ndev_vif->sta.action_frame_suspend_bmap = action_frame_suspend_bmap;
	ndev_vif->sta.connected_bssid = slsi_mem_dup((u8 *)bssid, ETH_ALEN);
	ndev_vif->sta.connected_ssid_len = (int)sme->ssid_len;
	ndev_vif->sta.connected_ssid = slsi_mem_dup((u8 *)sme->ssid, ndev_vif->sta.connected_ssid_len);
}
#endif

int slsi_connect(struct wiphy *wiphy, struct net_device *dev,
		 struct cfg80211_connect_params *sme)
{
	struct slsi_dev     *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct netdev_vif   *ndev_p2p_vif;
	u8                  device_address[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	int                 r = 0;
	u16                 capability = WLAN_CAPABILITY_ESS;
	struct slsi_peer    *peer;
	u16                 prev_vif_type;
	u32                 action_frame_bmap;
	u32                 action_frame_suspend_bmap;
	struct net_device   *p2p_dev;
	const u8            *bssid;
	struct ieee80211_channel *channel;
	const u8            *connected_ssid = NULL;
	u8                  peer_address[ETH_ALEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	u16                 center_freq = 0;
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	bool                ap_found = false;
#endif

	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, WlanLite FW does not support MLME_CONNECT.request\n");
		return -EOPNOTSUPP;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	capability = sme->privacy ? IEEE80211_PRIVACY_ON : IEEE80211_PRIVACY_OFF;
#else
	if (sme->privacy)
		capability |= WLAN_CAPABILITY_PRIVACY;
#endif

	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	if (sdev->device_state != SLSI_DEVICE_STATE_STARTED) {
		SLSI_WARN(sdev, "device not started yet (device_state:%d)\n", sdev->device_state);
		SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
		return -EINVAL;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	channel = sme->channel;
	bssid = sme->bssid;
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	ndev_vif->sta.akm_type = slsi_bss_connect_type_get(sdev, sme->ie, sme->ie_len);
	ndev_vif->sta.ssid_len = sme->ssid_len;
	memcpy(ndev_vif->sta.ssid, sme->ssid, sme->ssid_len);
	/* If bssid is not present, check if bssid hint is present, if even hint not present,
	 * select bssid in driver set connect_attempted to true.
	 */
	if (!sme->bssid) {
		ndev_vif->sta.drv_bss_selection = true;
		if (sme->bssid_hint) {
			bssid = sme->bssid_hint;
			channel = sme->channel_hint;
		} else {
			ap_found = slsi_select_ap_for_connection(sdev, dev, &bssid, &channel, false);
		}
	} else {
		ndev_vif->sta.drv_bss_selection = false;
	}
#endif

	/* check if ap is found in the blacklist.
	 * if present in the blacklist return failure
	 */
	r = slsi_is_bssid_in_blacklist(sdev, dev, (u8 *)bssid);
	if (r) {
		SLSI_NET_ERR(dev, "Blacklist bssid not allowed\n");
		goto exit_with_error;
	}

	if (ndev_vif->sta.sta_bss)
		SLSI_ETHER_COPY(peer_address, ndev_vif->sta.sta_bss->bssid);

	center_freq = channel ? channel->center_freq : 0;

	if (bssid)
		SLSI_NET_INFO(dev, "%.*s Freq=%d vifStatus=%d CurrBssid:" MACSTR " NewBssid:" MACSTR " Qinfo:%d ieLen:%d\n",
			      (int)sme->ssid_len, sme->ssid, center_freq, ndev_vif->sta.vif_status,
			      MAC2STR(peer_address), MAC2STR(bssid), sdev->device_config.qos_info, (int)sme->ie_len);
	else
		SLSI_NET_INFO(dev, "%.*s Freq=%d vifStatus=%d CurrBssid:" MACSTR " Qinfo:%d ieLen:%d\n",
			      (int)sme->ssid_len, sme->ssid, center_freq, ndev_vif->sta.vif_status,
			      MAC2STR(peer_address), sdev->device_config.qos_info, (int)sme->ie_len);

#if IS_ENABLED(CONFIG_SCSC_WIFILOGGER)
	SCSC_WLOG_PKTFATE_NEW_ASSOC();
	if (bssid) {
		SCSC_WLOG_DRIVER_EVENT(WLOG_NORMAL, WIFI_EVENT_ASSOCIATION_REQUESTED, 3,
				       WIFI_TAG_BSSID, ETH_ALEN, bssid,
				       WIFI_TAG_SSID, sme->ssid_len, sme->ssid,
				       WIFI_TAG_CHANNEL, sizeof(u16), &center_freq);
				       // ?? WIFI_TAG_VENDOR_SPECIFIC, sizeof(RSSE), RSSE);
	}
#endif

	if (SLSI_IS_HS2_UNSYNC_VIF(ndev_vif)) {
		slsi_wlan_unsync_vif_deactivate(sdev, dev, true);
	} else if (SLSI_IS_VIF_INDEX_P2P(ndev_vif)) {
		SLSI_NET_ERR(dev, "Connect requested on incorrect vif\n");
		goto exit_with_error;
	}

	if (WARN_ON(!sme->ssid))
		goto exit_with_error;

	if (WARN_ON(sme->ssid_len > IEEE80211_MAX_SSID_LEN))
		goto exit_with_error;

	if (bssid) {
		if (SLSI_IS_VIF_INDEX_WLAN(ndev_vif) && sdev->p2p_state == P2P_GROUP_FORMED_CLI) {
			p2p_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2PX_SWLAN);
			if (p2p_dev) {
				ndev_p2p_vif  = netdev_priv(p2p_dev);
				if (ndev_p2p_vif->sta.sta_bss) {
					if (SLSI_ETHER_EQUAL(ndev_p2p_vif->sta.sta_bss->bssid, bssid)) {
						SLSI_NET_ERR(dev, "Connect Request Rejected\n");
						goto exit_with_error;
					}
				}
			}
		}
	}

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION && ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED) {
		/*reassociation*/
		peer = slsi_get_peer_from_qs(sdev, dev, SLSI_STA_PEER_QUEUESET);
		if (WARN_ON(!peer))
			goto exit_with_error;

		if (!sme->bssid) {
			SLSI_NET_ERR(dev, "Require bssid in reassoc but received null\n");
			goto exit_with_error;
		}
		if (!memcmp(peer->address, sme->bssid, ETH_ALEN)) { /*same bssid*/
			r = slsi_mlme_reassociate(sdev, dev);
			if (r) {
				SLSI_NET_ERR(dev, "Failed to reassociate : %d\n", r);
			} else {
				ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
				slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
			}
			goto exit;
		} else { /*different bssid*/
			if (!ndev_vif->sta.sta_bss) {
				SLSI_NET_ERR(dev, "Bss is not stored in ndev_vif sta\n");
				goto exit_with_error;
			}
			connected_ssid = cfg80211_find_ie(WLAN_EID_SSID, ndev_vif->sta.sta_bss->ies->data, ndev_vif->sta.sta_bss->ies->len);

			if (!connected_ssid) {
				SLSI_NET_ERR(dev, "Require ssid in roam but received null\n");
				goto exit_with_error;
			}

			if (!memcmp(&connected_ssid[2], sme->ssid, connected_ssid[1])) { /*same ssid*/
				if (!sme->channel) {
					SLSI_NET_ERR(dev, "Roaming has been rejected, as sme->channel is null\n");
					goto exit_with_error;
				}
				r = slsi_mlme_roam(sdev, dev, sme->bssid, sme->channel->center_freq);
				if (r) {
					SLSI_NET_ERR(dev, "Failed to roam : %d\n", r);
					goto exit_with_error;
				}
				goto exit;
			} else {
				SLSI_NET_ERR(dev, "Connected but received connect to new ESS, without disconnect");
				goto exit_with_error;
			}
		}
	}
	/* Sta started case */
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	if (SLSI_IS_VIF_INDEX_MHS(sdev, ndev_vif))
		if (ndev_vif->iftype == NL80211_IFTYPE_P2P_CLIENT) {
			SLSI_NET_ERR(dev, "Iftype: %d\n", ndev_vif->iftype);
			goto exit_with_error;
		}
#endif /*wifi sharing*/
	if (WARN_ON(ndev_vif->activated)) {
		SLSI_NET_ERR(dev, "Vif is activated: %d\n", ndev_vif->activated);
		goto exit_with_error;
	}
	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION &&
	    ndev_vif->sta.vif_status != SLSI_VIF_STATUS_UNSPECIFIED) {
		SLSI_NET_ERR(dev, "VIF status: %d\n", ndev_vif->sta.vif_status);
		goto exit_with_error;
	}
	prev_vif_type = ndev_vif->vif_type;

	switch (ndev_vif->iftype) {
	case NL80211_IFTYPE_UNSPECIFIED:
	case NL80211_IFTYPE_STATION:
		ndev_vif->iftype = NL80211_IFTYPE_STATION;
		dev->ieee80211_ptr->iftype = NL80211_IFTYPE_STATION;
		action_frame_bmap = SLSI_STA_ACTION_FRAME_BITMAP;
		action_frame_suspend_bmap = SLSI_STA_ACTION_FRAME_SUSPEND_BITMAP;
#ifdef CONFIG_SCSC_WLAN_WES_NCHO
		if (sdev->device_config.wes_mode) {
			action_frame_bmap |= SLSI_ACTION_FRAME_VENDOR_SPEC;
			action_frame_suspend_bmap |= SLSI_ACTION_FRAME_VENDOR_SPEC;
		}
#endif
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
		slsi_p2p_group_start_remove_unsync_vif(sdev);
		p2p_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2P);
		if (p2p_dev)
			SLSI_ETHER_COPY(device_address, p2p_dev->dev_addr);
		action_frame_bmap = SLSI_ACTION_FRAME_PUBLIC;
		action_frame_suspend_bmap = SLSI_ACTION_FRAME_PUBLIC;
		break;
	default:
		SLSI_NET_ERR(dev, "Invalid Device Type: %d\n", ndev_vif->iftype);
		goto exit_with_error;
	}

	/* Initial Roaming checks done - assign vif type */
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
#ifndef CONFIG_SCSC_WLAN_BSS_SELECTION
	channel = sme->channel;
	bssid = sme->bssid;
#endif
	ndev_vif->sta.sta_bss = cfg80211_get_bss(wiphy,
						 channel,
						 bssid,
						 sme->ssid,
						 sme->ssid_len,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
						 IEEE80211_BSS_TYPE_ANY,
#else
						 capability,
#endif
						 capability);
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	if (!sme->bssid && !sme->bssid_hint && !ndev_vif->sta.sta_bss) {
		struct list_head *pos;

		list_for_each(pos, &ndev_vif->sta.ssid_info) {
			struct slsi_ssid_info *ssid_info = list_entry(pos, struct slsi_ssid_info, list);
			struct list_head *pos_bssid;

			if (ssid_info->ssid.ssid_len != ndev_vif->sta.ssid_len ||
			    memcmp(ssid_info->ssid.ssid, &ndev_vif->sta.ssid, ndev_vif->sta.ssid_len) != 0 ||
			    !(ssid_info->akm_type & ndev_vif->sta.akm_type))
				continue;
			list_for_each(pos_bssid, &ssid_info->bssid_list) {
				struct slsi_bssid_info *bssid_info = list_entry(pos_bssid, struct slsi_bssid_info, list);

				if (bssid && !memcmp(bssid_info->bssid, bssid, ETH_ALEN))
					continue;
				ndev_vif->sta.sta_bss = cfg80211_get_bss(wiphy,
									 ieee80211_get_channel(sdev->wiphy,
											       (bssid_info->freq / 2)),
									 bssid_info->bssid,
									 sme->ssid,
									 sme->ssid_len,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
									 IEEE80211_BSS_TYPE_ANY,
#else
									 capability,
#endif
									 capability);
				if (ndev_vif->sta.sta_bss) {
					bssid = bssid_info->bssid;
					channel = ieee80211_get_channel(sdev->wiphy, (bssid_info->freq / 2));
					break;
				}
			}
		}
	}
#endif
	if (!ndev_vif->sta.sta_bss) {
		struct cfg80211_ssid ssid;

		SLSI_NET_DBG3(dev, SLSI_CFG80211, "BSS info is not available - Perform scan\n");
		ssid.ssid_len = sme->ssid_len;
		memcpy(ssid.ssid, sme->ssid, ssid.ssid_len);
		if (!(ssid.ssid_len > 0 && channel)) {
			r = slsi_mlme_connect_scan(sdev, dev, 1, &ssid, channel);
			if (r) {
				SLSI_NET_ERR(dev, "slsi_mlme_connect_scan failed\n");
				goto exit;
			}
			ndev_vif->sta.sta_bss = cfg80211_get_bss(wiphy,
								 channel,
								 bssid,
								 sme->ssid,
								 sme->ssid_len,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
								 IEEE80211_BSS_TYPE_ANY,
#else
								 capability,
#endif
								 capability);
			if (!ndev_vif->sta.sta_bss) {
				if (bssid)
					SLSI_NET_ERR(dev, "cfg80211_get_bss(%.*s, " MACSTR ") Not found\n",
						     (int)sme->ssid_len, sme->ssid, MAC2STR(bssid));
				else
					SLSI_NET_ERR(dev, "cfg80211_get_bss(%.*s) Not found\n",
						     (int)sme->ssid_len, sme->ssid);
				/*Set previous status in case of failure */
				ndev_vif->vif_type = prev_vif_type;
				r = -ENOENT;
				goto exit;
			}
			channel = ndev_vif->sta.sta_bss->channel;
			bssid = ndev_vif->sta.sta_bss->bssid;
		}
	} else {
		channel = ndev_vif->sta.sta_bss->channel;
		bssid = ndev_vif->sta.sta_bss->bssid;
	}

	ndev_vif->channel_type = NL80211_CHAN_NO_HT;
	ndev_vif->chan = channel;
#ifndef CONFIG_SCSC_WLAN_BSS_SELECTION
	ndev_vif->sta.ssid_len = sme->ssid_len;
	memcpy(ndev_vif->sta.ssid, sme->ssid, sme->ssid_len);
#endif
	/* Always check the BSSID is not null during connection
	 * It will cause kernel panic if we access null BSSID.
	 */
	if (bssid)
		SLSI_ETHER_COPY(ndev_vif->sta.bssid, bssid);
	if (slsi_mlme_add_vif(sdev, dev, dev->dev_addr, device_address) != 0) {
		SLSI_NET_ERR(dev, "slsi_mlme_add_vif failed\n");
		goto exit_with_bss;
	}
	if (slsi_vif_activated(sdev, dev) != 0) {
		SLSI_NET_ERR(dev, "slsi_vif_activated failed\n");
		goto exit_with_vif;
	}
	if (slsi_mlme_register_action_frame(sdev, dev, action_frame_bmap, action_frame_suspend_bmap) != 0) {
		SLSI_NET_ERR(dev, "Action frame registration failed for bitmap value 0x%x 0x%x\n", action_frame_bmap, action_frame_suspend_bmap);
		goto exit_with_vif;
	}

	r = slsi_set_boost(sdev, dev);
	if (r != 0)
		SLSI_NET_ERR(dev, "Rssi Boost set failed: %d\n", r);

	/* add_info_elements with Probe Req IEs. Proceed even if confirm fails for add_info as it would
	 * still work if the fw pre-join scan does not include the vendor IEs
	 */
	if (ndev_vif->probe_req_ies) {
		if (ndev_vif->iftype == NL80211_IFTYPE_P2P_CLIENT) {
			if (sme->crypto.wpa_versions == 2)
				ndev_vif->delete_probe_req_ies = true; /* Stored Probe Req can be deleted at vif
									* deletion after WPA2 association
									*/
			else
				/* Retain stored Probe Req at vif deletion until WPA2 connection to allow Probe req */
				ndev_vif->delete_probe_req_ies = false;
		} else {
			ndev_vif->delete_probe_req_ies = true; /* Delete stored Probe Req at vif deletion for STA */
		}
		(void)slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_PROBE_REQUEST, ndev_vif->probe_req_ies,
						  ndev_vif->probe_req_ie_len);
	}

	/* Sometimes netif stack takes more time to initialize and any packet
	 * sent to stack would be dropped. This behavior is random in nature,
	 * so start the netif stack before sending out the connect req, it shall
	 * give enough time to netstack to initialize.
	 */
	netif_carrier_on(dev);
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;

	r = slsi_set_ext_cap(sdev, dev, sme->ie, sme->ie_len, slsi_extended_cap_mask);
	if (r != 0)
		SLSI_NET_ERR(dev, "Failed to set extended capability MIB: %d\n", r);

#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
	if (SLSI_IS_VIF_INDEX_WLAN(ndev_vif)) {
		const u8 *rsn;

		SLSI_NET_DBG1(dev, SLSI_CFG80211, "N AKM Suites: : %1d\n", sme->crypto.n_akm_suites);
		rsn = cfg80211_find_ie(WLAN_EID_RSN, sme->ie, sme->ie_len);
		if (rsn) {
			int pos;

			/* Calculate the position of AKM suite in RSNIE
			 * RSNIE TAG(1 byte) + length(1 byte) + version(2 byte) + Group cipher suite(4 bytes)
			 * pairwise suite count(2 byte) + pairwise suite count * 4 + AKM suite count(2 byte)
			 * pos is the array index not length
			 */
			pos = 7 + 2 + (rsn[8] * 4) + 2;
			ndev_vif->sta.crypto.akm_suites[0] = ((rsn[pos + 4] << 24) | (rsn[pos + 3] << 16) | (rsn[pos + 2] << 8) | (rsn[pos + 1]));
			if ((rsn[pos + 1] == 0x00 && rsn[pos + 2] == 0x0f && rsn[pos + 3] == 0xac) && (rsn[pos + 4] == 0x08 || rsn[pos + 4] == 0x09))
				ndev_vif->sta.crypto.wpa_versions = 3;
			else
				ndev_vif->sta.crypto.wpa_versions = 0;

			ndev_vif->sta.rsn_ie_len = rsn[1];
			if (ndev_vif->sta.rsn_ie) {
				kfree(ndev_vif->sta.rsn_ie);
				ndev_vif->sta.rsn_ie = NULL;
			}
			/* Len+2 because RSN IE TAG and Length */
			ndev_vif->sta.rsn_ie = kmalloc(ndev_vif->sta.rsn_ie_len + 2, GFP_KERNEL);

			/* len+2 because RSNIE TAG and Length */
			if (ndev_vif->sta.rsn_ie)
				memcpy(ndev_vif->sta.rsn_ie, rsn, ndev_vif->sta.rsn_ie_len + 2);
		}
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
		if (ndev_vif->sta.crypto.wpa_versions == 3)
			ndev_vif->sta.wpa3_auth_state = SLSI_WPA3_PREAUTH;
#endif
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "RSN IE: : %1d\n", ndev_vif->sta.crypto.akm_suites[0]);
	}
#endif
	r = slsi_mlme_connect(sdev, dev, sme, channel, bssid);
	if (r != 0) {
		ndev_vif->sta.is_wps = false;
		SLSI_NET_ERR(dev, "connect failed: %d\n", r);
		netif_carrier_off(dev);
		goto exit_with_vif;
	}

	peer = slsi_peer_add(sdev, dev, (u8 *)bssid, SLSI_STA_PEER_QUEUESET + 1);
	ndev_vif->sta.resp_id = 0;
	if (!peer) {
		goto exit_with_error;
	}

#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	if (ndev_vif->sta.drv_bss_selection) {
		slsi_save_connection_params(sdev, dev, sme, channel, bssid,
					    action_frame_bmap, action_frame_suspend_bmap);
		slsi_set_reset_connect_attempted_flag(sdev, dev, bssid);
	}
#endif
	goto exit;

exit_with_vif:
	if (slsi_mlme_del_vif(sdev, dev) != 0)
		SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
	slsi_vif_deactivated(sdev, dev);
exit_with_bss:
	if (ndev_vif->sta.sta_bss) {
		slsi_cfg80211_put_bss(wiphy, ndev_vif->sta.sta_bss);
		ndev_vif->sta.sta_bss = NULL;
	}
exit_with_error:
	r = -EINVAL;
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return r;
}

int slsi_disconnect(struct wiphy *wiphy, struct net_device *dev,
		    u16 reason_code)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer  *peer;
	int               r = 0;

	cancel_work_sync(&ndev_vif->set_multicast_filter_work);
	cancel_work_sync(&ndev_vif->update_pkt_filter_work);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "reason: %d, vif_index = %d, vif_type = %d\n", reason_code,
		      ndev_vif->ifnum, ndev_vif->vif_type);

	/* Assuming that the time it takes the firmware to disconnect is not significant
	 * as this function holds the locks until the MLME-DISCONNECT-IND comes back.
	 * Unless the MLME-DISCONNECT-CFM fails.
	 */
	if (!ndev_vif->activated) {
		r = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
		cfg80211_disconnected(dev, reason_code, NULL, 0, false, GFP_KERNEL);
#else
		cfg80211_disconnected(dev, reason_code, NULL, 0, GFP_KERNEL);
#endif
		SLSI_NET_INFO(dev, "Vif is already Deactivated\n");
		goto exit;
	}

	peer = ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET];

#if IS_ENABLED(CONFIG_SCSC_WIFILOGGER)
	SCSC_WLOG_DRIVER_EVENT(WLOG_NORMAL, WIFI_EVENT_DISASSOCIATION_REQUESTED, 2,
			       WIFI_TAG_BSSID, ETH_ALEN, peer->address,
			       WIFI_TAG_REASON_CODE, sizeof(u16), &reason_code);
#endif

	switch (ndev_vif->vif_type) {
	case FAPI_VIFTYPE_STATION:
	{
		slsi_reset_throughput_stats(dev);
		/* Disconnecting spans several host firmware interactions so track the status
		 * so that the Host can ignore connect related signaling eg. MLME-CONNECT-IND
		 * now that it has triggered a disconnect.
		 */
		ndev_vif->sta.vif_status = SLSI_VIF_STATUS_DISCONNECTING;

		netif_carrier_off(dev);
		if (peer->valid)
			slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);

		/* MLME-DISCONNECT_CFM only means that the firmware has accepted the request it has not yet
		 * disconnected. Completion of the disconnect is indicated by MLME-DISCONNECT-IND, so have
		 * to wait for that before deleting the VIF. Also any new activities eg. connect can not yet
		 * be started on the VIF until the disconnection is completed. So the MLME function also handles
		 * waiting for the MLME-DISCONNECT-IND (if the CFM is successful)
		 */

		r = slsi_mlme_disconnect(sdev, dev, peer->address,  reason_code, true);
		if (r != 0)
			SLSI_NET_ERR(dev, "Disconnection returned with failure\n");
		/* Even if we fail to disconnect cleanly, tidy up. */
		r = slsi_handle_disconnect(sdev, dev, peer->address, 0, NULL, 0);

		break;
	}
	default:
		SLSI_NET_WARN(dev, "Invalid - vif type:%d, device type:%d)\n", ndev_vif->vif_type, ndev_vif->iftype);
		r = -EINVAL;
		break;
	}

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

int slsi_set_default_key(struct wiphy *wiphy, struct net_device *dev,
			 u8 key_index, bool unicast, bool multicast)
{
	SLSI_UNUSED_PARAMETER(wiphy);
	SLSI_UNUSED_PARAMETER(dev);
	SLSI_UNUSED_PARAMETER(key_index);
	SLSI_UNUSED_PARAMETER(unicast);
	SLSI_UNUSED_PARAMETER(multicast);
	/* Key is set in add_key. Nothing to do here */
	return 0;
}

int slsi_config_default_mgmt_key(struct wiphy      *wiphy,
				 struct net_device *dev,
				 u8                key_index)
{
	SLSI_UNUSED_PARAMETER(wiphy);
	SLSI_UNUSED_PARAMETER(key_index);
	SLSI_UNUSED_PARAMETER(dev);

	return 0;
}

int slsi_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	int             r = 0;

	SLSI_DBG1(sdev, SLSI_CFG80211, "slsi_set_wiphy_parms Frag Threshold = %d, RTS Threshold = %d",
		  wiphy->frag_threshold, wiphy->rts_threshold);

	if ((changed & WIPHY_PARAM_FRAG_THRESHOLD) && (wiphy->frag_threshold != -1)) {
		r = slsi_set_uint_mib(sdev, NULL, SLSI_PSID_DOT11_FRAGMENTATION_THRESHOLD, wiphy->frag_threshold);
		if (r != 0) {
			SLSI_ERR(sdev, "Setting FRAG_THRESHOLD failed\n");
			return r;
		}
	}

	if ((changed & WIPHY_PARAM_RTS_THRESHOLD) && (wiphy->rts_threshold != -1)) {
		r = slsi_set_uint_mib(sdev, NULL, SLSI_PSID_DOT11_RTS_THRESHOLD, wiphy->rts_threshold);
		if (r != 0) {
			SLSI_ERR(sdev, "Setting RTS_THRESHOLD failed\n");
			return r;
		}
	}

	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
int slsi_set_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
		      enum nl80211_tx_power_setting type, int mbm)
#else
int slsi_set_tx_power(struct wiphy *wiphy,
		      enum nl80211_tx_power_setting type, int mbm)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9)) */
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	int             r = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	SLSI_UNUSED_PARAMETER(wdev);
	SLSI_UNUSED_PARAMETER(type);
#endif
	SLSI_UNUSED_PARAMETER(mbm);
	SLSI_UNUSED_PARAMETER(sdev);

	r = -EOPNOTSUPP;

	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
int slsi_get_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev, int *dbm)
#else
int slsi_get_tx_power(struct wiphy *wiphy, int *dbm)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9)) */
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	int             r = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	SLSI_UNUSED_PARAMETER(wdev);
#endif
	SLSI_UNUSED_PARAMETER(dbm);
	SLSI_UNUSED_PARAMETER(sdev);

	r = -EOPNOTSUPP;

	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
int slsi_del_station(struct wiphy *wiphy, struct net_device *dev,
		     struct station_del_parameters *del_params)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int slsi_del_station(struct wiphy *wiphy, struct net_device *dev,
		     const u8 *mac)
#else
int slsi_del_station(struct wiphy *wiphy, struct net_device *dev,
		     u8 *mac)
#endif
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer  *peer;
	int               r = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	const u8          *mac = del_params->mac;
#endif

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG1(dev, SLSI_CFG80211, "%pM, vifType:%d, vifIndex:%d vifActivated:%d ap.p2p_gc_keys_set = %d\n",
		      mac, ndev_vif->vif_type, ndev_vif->ifnum, ndev_vif->activated, ndev_vif->ap.p2p_gc_keys_set);

	/* Function is called by cfg80211 before the VIF is added */
	if (!ndev_vif->activated)
		goto exit;

	if (FAPI_VIFTYPE_AP != ndev_vif->vif_type) {
		r = -EINVAL;
		goto exit;
	}
	/* MAC with NULL value will come in case of flushing VLANS . Ignore this.*/
	if (!mac)
		goto exit;
	else if (is_broadcast_ether_addr(mac)) {
		int  i = 0;

		while (i < SLSI_PEER_INDEX_MAX) {
			peer = ndev_vif->peer_sta_record[i];
			if (peer && peer->valid) {
				slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
			}
			++i;
		}

		/* Note AP :: mlme_disconnect_request with broadcast mac address is
		 * not required. Other third party devices don't support this. Conclusively,
		 * BIP support is not present with AP
		 */

		/* Free WPA and WMM IEs if present */
		slsi_clear_cached_ies(&ndev_vif->ap.cache_wpa_ie, &ndev_vif->ap.wpa_ie_len);
		slsi_clear_cached_ies(&ndev_vif->ap.cache_wmm_ie, &ndev_vif->ap.wmm_ie_len);

		netif_carrier_off(dev);

		/* All STA related packets and info should already have been flushed */
		if (slsi_mlme_del_vif(sdev, dev) != 0)
			SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
		slsi_vif_deactivated(sdev, dev);
		ndev_vif->ipaddress = cpu_to_be32(0);

		if (ndev_vif->ap.p2p_gc_keys_set) {
			slsi_wake_unlock(&sdev->wlan_wl);
			ndev_vif->ap.p2p_gc_keys_set = false;
		}
	} else {
		peer = slsi_get_peer_from_mac(sdev, dev, mac);
		if (peer) {  /* To handle race condition when disconnect_req is sent before procedure_strted_ind and before mlme-connected_ind*/
			if (peer->connected_state == SLSI_STA_CONN_STATE_CONNECTING) {
				SLSI_NET_DBG1(dev, SLSI_CFG80211, "SLSI_STA_CONN_STATE_CONNECTING : mlme-disconnect-req dropped at driver\n");
				goto exit;
			}
			if (peer->is_wps) {
				/* To inter-op with Intel STA in P2P cert need to discard the deauth after successful WPS handshake as a P2P GO */
				SLSI_NET_INFO(dev, "DISCONNECT after WPS : mlme-disconnect-req dropped at driver\n");
				goto exit;
			}
			slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
			r = slsi_mlme_disconnect(sdev, dev, peer->address, WLAN_REASON_DEAUTH_LEAVING, true);
			if (r != 0)
				SLSI_NET_ERR(dev, "Disconnection returned with failure\n");
			/* Even if we fail to disconnect cleanly, tidy up. */
			r = slsi_handle_disconnect(sdev, dev, peer->address, WLAN_REASON_DEAUTH_LEAVING, NULL, 0);
		}
	}

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int slsi_get_station(struct wiphy *wiphy, struct net_device *dev,
		     const u8 *mac, struct station_info *sinfo)
#else
int slsi_get_station(struct wiphy *wiphy, struct net_device *dev,
		     u8 *mac, struct station_info *sinfo)
#endif
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer  *peer;
	int               r = 0;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!ndev_vif->activated) {
		r = -EINVAL;
		goto exit;
	}

	peer = slsi_get_peer_from_mac(sdev, dev, mac);
	if (!peer) {
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "%pM : Not Found\n", mac);
		r = -EINVAL;
		goto exit;
	}

	if (((ndev_vif->iftype == NL80211_IFTYPE_STATION && !(ndev_vif->sta.roam_in_progress)) ||
	     ndev_vif->iftype == NL80211_IFTYPE_P2P_CLIENT)) {
		/*Read MIB and fill into the peer.sinfo*/
		r = slsi_mlme_get_sinfo_mib(sdev, dev, peer);
		if (r) {
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "failed to read Station Info Error:%d\n", r);
			goto exit;
		}
	}

	*sinfo = peer->sinfo;
	sinfo->generation = ndev_vif->cfg80211_sinfo_generation;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "%pM, tx:%d, txbytes:%llu, rx:%d, rxbytes:%llu tx_fail:%d tx_retry:%d\n",
				      mac,
				      peer->sinfo.tx_packets,
				      peer->sinfo.tx_bytes,
				      peer->sinfo.rx_packets,
				      peer->sinfo.rx_bytes,
				      peer->sinfo.tx_failed,
				      peer->sinfo.tx_retries);
#else
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "%pM, tx:%d, txbytes:%d, rx:%d, rxbytes:%d  tx_fail:%d tx_retry:%d\n",
				      mac,
				      peer->sinfo.tx_packets,
				      peer->sinfo.tx_bytes,
				      peer->sinfo.rx_packets,
				      peer->sinfo.rx_bytes,
				      peer->sinfo.tx_failed,
				      peer->sinfo.tx_retries);
#endif

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

int slsi_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev,
			bool enabled, int timeout)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               r = -EINVAL;
	u16               pwr_mode = enabled ? FAPI_POWERMANAGEMENTMODE_POWER_SAVE : FAPI_POWERMANAGEMENTMODE_ACTIVE_MODE;

	SLSI_UNUSED_PARAMETER(timeout);
	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, WlanLite FW does not support MLME_POWERMGT.request\n");
		return -EOPNOTSUPP;
	}

	if (slsi_is_rf_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, RF test does not support.\n");
		return -EOPNOTSUPP;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "enabled:%d, vif_type:%d, vif_index:%d\n", enabled, ndev_vif->vif_type,
		      ndev_vif->ifnum);

	if (ndev_vif->activated && ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		ndev_vif->set_power_mode = pwr_mode;
		r = slsi_mlme_powermgt(sdev, dev, pwr_mode);
	} else {
		r = 0;
	}

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 18))
int slsi_tdls_oper(struct wiphy *wiphy, struct net_device *dev, const u8 *peer, enum nl80211_tdls_operation oper)
#else
int slsi_tdls_oper(struct wiphy *wiphy, struct net_device *dev, u8 *peer, enum nl80211_tdls_operation oper)
#endif
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               r = 0;

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "oper:%d, vif_type:%d, vif_index:%d\n", oper, ndev_vif->vif_type,
		      ndev_vif->ifnum);

	if (!(wiphy->flags & WIPHY_FLAG_SUPPORTS_TDLS))
		return -ENOTSUPP;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!ndev_vif->activated || SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif) ||
	    ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED) {
		r = -ENOTSUPP;
		goto exit;
	}

	switch (oper) {
	case NL80211_TDLS_DISCOVERY_REQ:
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "NL80211_TDLS_DISCOVERY_REQ\n");
		r = slsi_mlme_tdls_action(sdev, dev, peer, FAPI_TDLSACTION_DISCOVERY, 0, 0);
		break;
	case NL80211_TDLS_SETUP:
		r = slsi_mlme_tdls_action(sdev, dev, peer, FAPI_TDLSACTION_SETUP, 0, 0);
		break;
	case NL80211_TDLS_TEARDOWN:
		r = slsi_mlme_tdls_action(sdev, dev, peer, FAPI_TDLSACTION_TEARDOWN, 0, 0);
		break;
	default:
		r = -EOPNOTSUPP;
		goto exit;
	}
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
int slsi_set_qos_map(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_qos_map *qos_map)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer  *peer;
	int               r = 0;

	/* Cleaning up is inherently taken care by driver */
	if (!qos_map)
		return r;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!ndev_vif->activated) {
		r = -EINVAL;
		goto exit;
	}

	if (ndev_vif->vif_type != FAPI_VIFTYPE_STATION) {
		r = -EINVAL;
		goto exit;
	}

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "Set QoS Map\n");
	peer = ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET];
	if (!peer || !peer->valid) {
		r = -EINVAL;
		goto exit;
	}

	memcpy(&peer->qos_map, qos_map, sizeof(struct cfg80211_qos_map));
	peer->qos_map_set = true;

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

int slsi_set_monitor_channel(struct wiphy *wiphy, struct cfg80211_chan_def *chandef)
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device *dev;
	struct netdev_vif *ndev_vif;

	SLSI_DBG1(sdev, SLSI_CFG80211, "channel (freq:%u)\n", chandef->chan->center_freq);

	rcu_read_lock();
	dev = slsi_get_netdev_rcu(sdev, SLSI_NET_INDEX_WLAN);
	if (!dev) {
		SLSI_ERR(sdev, "netdev No longer exists\n");
		rcu_read_unlock();
		return -EINVAL;
	}
	ndev_vif = netdev_priv(dev);
	rcu_read_unlock();

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (slsi_test_sap_configure_monitor_mode(sdev, dev, chandef) != 0) {
		SLSI_ERR(sdev, "set Monitor channel failed\n");
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return -EINVAL;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return 0;
}
#endif
int slsi_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	SLSI_UNUSED_PARAMETER(wow);

	return 0;
}

int slsi_resume(struct wiphy *wiphy)
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);

	/* Scheduling the IO thread */
/*	(void)slsi_hip_run_bh(sdev); */
	SLSI_UNUSED_PARAMETER(sdev);

	return 0;
}

int slsi_set_pmksa(struct wiphy *wiphy, struct net_device *dev,
		   struct cfg80211_pmksa *pmksa)
{
#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
	int i = 0;
	struct slsi_dev     *sdev = SDEV_FROM_WIPHY(wiphy);
	u8 *rsnie; /* final RSN IE with the PMKID*/
	u16 rsnie_len;
	int left = 0;
	u16 count = 0;
	int ret = 0;
	int pos = 0;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (ndev_vif->sta.rsn_ie) {
		rsnie_len = ndev_vif->sta.rsn_ie_len;
		rsnie = kmalloc(rsnie_len+2+PMKID_LEN, GFP_KERNEL); /* Length of RSNIE + PMKID */
		if (rsnie) {
			memset(rsnie, 0, rsnie_len+2+PMKID_LEN);
			/* parse the RSN IE and copy PMKID to Required position in RSN IE*/
			left = rsnie_len + 2; //FOR RSN IE TAG and Length

			pos = 4; /* RSN IE ID, LEN, and Version*/
			left -= 4;
			if (left < 4) {
				kfree(rsnie);
				kfree(ndev_vif->sta.rsn_ie);
				ndev_vif->sta.rsn_ie = NULL;
				ret = -EINVAL;
				goto exit;
			}
			pos += RSN_SELECTOR_LEN; /* Group cipher suite */
			left -= RSN_SELECTOR_LEN;

			/* Pairwise and AKM suite count and suite list */
			i = 0;
			while (i < 2) {
				pos += 2;
				left -= 2;
				count = le16_to_cpu(*(u16 *)(&ndev_vif->sta.rsn_ie[pos-2]));
				pos += count * RSN_SELECTOR_LEN;
				left -= count * RSN_SELECTOR_LEN;
				i++;
			}
			pos += 2; /* RSN Capabilities */
			left -= 2;
			count = le16_to_cpu(*(u16 *)(&ndev_vif->sta.rsn_ie[pos]));  /* PMKID count */
			pos += 2; /* PMKID count */
			left -= 2;
			memcpy(rsnie, ndev_vif->sta.rsn_ie, pos);
			rsnie[pos-2] = 1;
			rsnie[pos-1] = 0;
			memcpy(&rsnie[pos], pmksa->pmkid, PMKID_LEN); /* copy PMKID */
			pos += PMKID_LEN;
			if (count) {
				left -= PMKID_LEN;
				memcpy(&rsnie[pos], &ndev_vif->sta.rsn_ie[pos], left);
			} else {
				memcpy(&rsnie[pos], &ndev_vif->sta.rsn_ie[pos-PMKID_LEN], left);
			}
			pos += left;
			rsnie_len = pos;
			rsnie[1] = rsnie_len - 2;
			ret = slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_ASSOCIATION_REQUEST, rsnie, rsnie_len);
			if (ret != 0) {
				SLSI_NET_ERR(dev, "RSN IE with PMKID setting failed\n");
				kfree(rsnie);
				kfree(ndev_vif->sta.rsn_ie);
				ndev_vif->sta.rsn_ie = NULL;
				goto exit;
			}
			kfree(rsnie);
		} else {
			SLSI_NET_ERR(dev, "Out of memory for RSN IE\n");
			ret = -ENOMEM;
			goto exit;
		}
	} else {
		SLSI_NET_ERR(dev, "RSN IE is not present in Station VIF\n");
		ret = -EINVAL;
		goto exit;
	}
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return ret;
#else
	SLSI_UNUSED_PARAMETER(wiphy);
	SLSI_UNUSED_PARAMETER(dev);
	SLSI_UNUSED_PARAMETER(pmksa);
	return 0;

#endif
}

int slsi_del_pmksa(struct wiphy *wiphy, struct net_device *dev,
		   struct cfg80211_pmksa *pmksa)
{
#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
	struct slsi_dev     *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	int ret = 0;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (!ndev_vif->activated) {
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "VIF not activated\n");
		goto exit;
	}
	ret = slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_ASSOCIATION_REQUEST, NULL, 0);
	if (ret != 0) {
		SLSI_NET_ERR(dev, "Clearing PMKID failed\n");
		goto exit;
	}
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return ret;
#else

	SLSI_UNUSED_PARAMETER(wiphy);
	SLSI_UNUSED_PARAMETER(dev);
	SLSI_UNUSED_PARAMETER(pmksa);
	return 0;
#endif
}

int slsi_flush_pmksa(struct wiphy *wiphy, struct net_device *dev)
{
	SLSI_UNUSED_PARAMETER(wiphy);
	SLSI_UNUSED_PARAMETER(dev);
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
int slsi_remain_on_channel(struct wiphy             *wiphy,
			   struct wireless_dev      *wdev,
			   struct ieee80211_channel *chan,
			   unsigned int             duration,
			   u64                      *cookie)
{
	struct net_device *dev = wdev->netdev;

#else
int slsi_remain_on_channel(struct wiphy              *wiphy,
			   struct net_device         *dev,
			   struct ieee80211_channel  *chan,
			   enum nl80211_channel_type channel_type,
			   unsigned int              duration,
			   u64                       *cookie)
{
#endif  /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9)) */
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               r = 0;

	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	if (sdev->device_state != SLSI_DEVICE_STATE_STARTED) {
		SLSI_WARN(sdev, "device not started yet (device_state:%d)\n", sdev->device_state);
		goto exit_with_error;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "channel freq = %d, duration = %d, vif_type = %d, vif_index = %d,"
		      "sdev->p2p_state = %s\n", chan->center_freq, duration, ndev_vif->vif_type, ndev_vif->ifnum,
		      slsi_p2p_state_text(sdev->p2p_state));
	if (!SLSI_IS_VIF_INDEX_P2P(ndev_vif)) {
		SLSI_NET_ERR(dev, "Invalid vif type\n");
		goto exit_with_error;
	}

	if (SLSI_IS_P2P_GROUP_STATE(sdev)) {
		slsi_assign_cookie_id(cookie, &ndev_vif->unsync.roc_cookie);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
		cfg80211_ready_on_channel(wdev, *cookie, chan, duration, GFP_KERNEL);
		cfg80211_remain_on_channel_expired(wdev, *cookie, chan, GFP_KERNEL);
#else
		cfg80211_ready_on_channel(dev, *cookie, chan, channel_type, duration, GFP_KERNEL);
		cfg80211_remain_on_channel_expired(dev, *cookie, chan, channel_type, GFP_KERNEL);
#endif
		goto exit;
	}

	/* Unsync vif will be required, cancel any pending work of its deletion */
	cancel_delayed_work(&ndev_vif->unsync.del_vif_work);
	if (sdev->p2p_state == P2P_ACTION_FRAME_TX_RX)
		queue_delayed_work(sdev->device_wq, &ndev_vif->unsync.del_vif_work,
				   msecs_to_jiffies(duration + SLSI_P2P_UNSYNC_VIF_EXTRA_MSEC));

	/* Ideally, there should not be any ROC work pending. However, supplicant can send back to back ROC in a race scenario as below.
	 * If action frame is received while P2P social scan, the response frame tx is delayed till scan completes. After scan completion,
	 * frame tx is done and ROC is started. Upon frame tx status, supplicant sends another ROC without cancelling the previous one.
	 */
	cancel_delayed_work(&ndev_vif->unsync.roc_expiry_work);

	if (delayed_work_pending(&ndev_vif->unsync.unset_channel_expiry_work))
		cancel_delayed_work(&ndev_vif->unsync.unset_channel_expiry_work);

	/* If action frame tx is in progress and ROC comes, then it would mean action frame tx was done in ROC and
	 * frame tx ind is awaited, don't change state. Also allow back to back ROC in case it comes.
	 */
	if (sdev->p2p_state == P2P_ACTION_FRAME_TX_RX || sdev->p2p_state == P2P_LISTENING) {
		goto exit_with_roc;
	}

	/* Unsync vif activation: Possible P2P state at this point is P2P_IDLE_NO_VIF or P2P_IDLE_VIF_ACTIVE */
	if (sdev->p2p_state == P2P_IDLE_NO_VIF) {
		if (slsi_p2p_vif_activate(sdev, dev, chan, duration, true) != 0)
			goto exit_with_error;
	} else if (sdev->p2p_state == P2P_IDLE_VIF_ACTIVE) {
		/* Configure Probe Response IEs in firmware if they have changed */
		if (ndev_vif->unsync.ies_changed) {
			u16 purpose = FAPI_PURPOSE_PROBE_RESPONSE;

			if (slsi_mlme_add_info_elements(sdev, dev, purpose, ndev_vif->unsync.probe_rsp_ies, ndev_vif->unsync.probe_rsp_ies_len) != 0) {
				SLSI_NET_ERR(dev, "Probe Rsp IEs setting failed\n");
				goto exit_with_vif;
			}
			ndev_vif->unsync.ies_changed = false;
		}
		/* Channel Setting - Don't set if already on same channel */
		if (ndev_vif->driver_channel != chan->hw_value) {
			if (slsi_mlme_set_channel(sdev, dev, chan, SLSI_FW_CHANNEL_DURATION_UNSPECIFIED, 0, 0) != 0) {
				SLSI_NET_ERR(dev, "Channel setting failed\n");
				goto exit_with_vif;
			} else {
				ndev_vif->chan = chan;
				ndev_vif->driver_channel = chan->hw_value;
			}
		}
	} else {
		SLSI_NET_ERR(dev, "Driver in incorrect P2P state (%s)", slsi_p2p_state_text(sdev->p2p_state));
		goto exit_with_error;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 9))
	ndev_vif->channel_type = channel_type;
#endif

	SLSI_P2P_STATE_CHANGE(sdev, P2P_LISTENING);

exit_with_roc:
	/* Cancel remain on channel is sent to the supplicant 10ms before the duration
	 *This is to avoid the race condition of supplicant sending cancel remain on channel and
	 *drv sending cancel_remain on channel because of roc expiry.
	 *This race condition causes delay to the next p2p search
	 */
	queue_delayed_work(sdev->device_wq, &ndev_vif->unsync.roc_expiry_work,
			   msecs_to_jiffies(duration - SLSI_P2P_ROC_EXTRA_MSEC));

	slsi_assign_cookie_id(cookie, &ndev_vif->unsync.roc_cookie);
	SLSI_NET_DBG2(dev, SLSI_CFG80211, "Cookie = 0x%llx\n", *cookie);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	cfg80211_ready_on_channel(wdev, *cookie, chan, duration, GFP_KERNEL);
#else
	cfg80211_ready_on_channel(dev, *cookie, chan, channel_type, duration, GFP_KERNEL);
#endif

	goto exit;

exit_with_vif:
	slsi_p2p_vif_deactivate(sdev, dev, true);
exit_with_error:
	r = -EINVAL;
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
int slsi_cancel_remain_on_channel(struct wiphy        *wiphy,
				  struct wireless_dev *wdev,
				  u64                 cookie)
{
	struct net_device *dev = wdev->netdev;

#else
int slsi_cancel_remain_on_channel(struct wiphy      *wiphy,
				  struct net_device *dev,
				  u64               cookie)
{
#endif  /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9)) */
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	int               r = 0;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "Cookie = 0x%llx, vif_type = %d, vif_index = %d, sdev->p2p_state = %s,"
		      "ndev_vif->ap.p2p_gc_keys_set = %d, ndev_vif->unsync.roc_cookie = 0x%llx\n", cookie,
		      ndev_vif->vif_type, ndev_vif->ifnum, slsi_p2p_state_text(sdev->p2p_state),
		      ndev_vif->ap.p2p_gc_keys_set, ndev_vif->unsync.roc_cookie);

	if (!SLSI_IS_VIF_INDEX_P2P(ndev_vif)) {
		SLSI_NET_ERR(dev, "Invalid vif type\n");
		r = -EINVAL;
		goto exit;
	}

	if (!((sdev->p2p_state == P2P_LISTENING) || (sdev->p2p_state == P2P_ACTION_FRAME_TX_RX))) {
		goto exit;
	}

	if (sdev->p2p_state == P2P_ACTION_FRAME_TX_RX && ndev_vif->mgmt_tx_data.exp_frame != SLSI_PA_INVALID) {
		/* Reset the expected action frame as procedure got completed */
		SLSI_INFO(sdev, "Action frame (%s) was not received\n", slsi_pa_subtype_text(ndev_vif->mgmt_tx_data.exp_frame));
		ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
	}

	cancel_delayed_work(&ndev_vif->unsync.roc_expiry_work);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	cfg80211_remain_on_channel_expired(&ndev_vif->wdev, ndev_vif->unsync.roc_cookie, ndev_vif->chan, GFP_KERNEL);
#else
	cfg80211_remain_on_channel_expired(ndev_vif->wdev.netdev, ndev_vif->unsync.roc_cookie,
					   ndev_vif->chan, ndev_vif->channel_type, GFP_KERNEL);
#endif
		if (!ndev_vif->drv_in_p2p_procedure) {
			if (delayed_work_pending(&ndev_vif->unsync.unset_channel_expiry_work))
				cancel_delayed_work(&ndev_vif->unsync.unset_channel_expiry_work);
			queue_delayed_work(sdev->device_wq, &ndev_vif->unsync.unset_channel_expiry_work,
					   msecs_to_jiffies(SLSI_P2P_UNSET_CHANNEL_EXTRA_MSEC));
		}
	/* Queue work to delete unsync vif */
	slsi_p2p_queue_unsync_vif_del_work(ndev_vif, SLSI_P2P_UNSYNC_VIF_EXTRA_MSEC);
	SLSI_P2P_STATE_CHANGE(sdev, P2P_IDLE_VIF_ACTIVE);

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

int slsi_change_bss(struct wiphy *wiphy, struct net_device *dev,
		    struct bss_parameters *params)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	int               r = 0;

	SLSI_UNUSED_PARAMETER(params);
	SLSI_UNUSED_PARAMETER(sdev);

	return r;
}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 5, 0))
int slsi_set_channel(struct wiphy *wiphy, struct net_device *dev,
		     struct ieee80211_channel *chan,
		     enum nl80211_channel_type channel_type)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               r = 0;

	SLSI_UNUSED_PARAMETER(sdev);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "channel_type:%u, freq:%u, vif_index:%d, vif_type:%d\n", channel_type,
		      chan->center_freq, ndev_vif->ifnum, ndev_vif->vif_type);
	if (WARN_ON(ndev_vif->activated)) {
		r = -EINVAL;
		goto exit;
	}

	ndev_vif->channel_type = channel_type;
	ndev_vif->chan = chan;

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}
#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 5, 0)) */

static void slsi_ap_start_obss_scan(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif)
{
	struct cfg80211_ssid     ssids;
	struct ieee80211_channel *channel;
	int                      n_ssids = 1, n_channels = 1, i;

	SLSI_NET_DBG1(dev, SLSI_CFG80211, "channel %u\n", ndev_vif->chan->hw_value);

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	ssids.ssid_len = 0;
	for (i = 0; i < IEEE80211_MAX_SSID_LEN; i++)
		ssids.ssid[i] = 0x00;   /* Broadcast SSID */

	channel = ieee80211_get_channel(sdev->wiphy, ndev_vif->chan->center_freq);

	ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan = true;
	(void)slsi_mlme_add_scan(sdev,
				 dev,
				 FAPI_SCANTYPE_OBSS_SCAN,
				 FAPI_REPORTMODE_REAL_TIME,
				 n_ssids,
				 &ssids,
				 n_channels,
				 &channel,
				 NULL,
				 NULL, /* No IEs */
				 0,
				 ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan /* Wait for scan_done_ind */);

	slsi_ap_obss_scan_done_ind(dev, ndev_vif);
	ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan = false;
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

static int slsi_ap_start_validate(struct net_device *dev, struct slsi_dev *sdev, struct cfg80211_ap_settings *settings)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (SLSI_IS_VIF_INDEX_P2P(ndev_vif)) {
		SLSI_NET_ERR(dev, "AP start requested on incorrect vif\n");
		goto exit_with_error;
	}

	if (!settings->ssid_len || !settings->ssid) {
		SLSI_NET_ERR(dev, "SSID not provided\n");
		goto exit_with_error;
	}

	if (!settings->beacon.head_len || !settings->beacon.head) {
		SLSI_NET_ERR(dev, "Beacon not provided\n");
		goto exit_with_error;
	}

	if (!settings->beacon_interval) {
		SLSI_NET_ERR(dev, "Beacon Interval not provided\n");
		goto exit_with_error;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	ndev_vif->chandef = &settings->chandef;
	ndev_vif->chan = ndev_vif->chandef->chan;
	ndev_vif->chandef_saved = settings->chandef;
#endif
	if (WARN_ON(!ndev_vif->chan))
		goto exit_with_error;

	if (WARN_ON(ndev_vif->activated))
		goto exit_with_error;

	if (WARN_ON((ndev_vif->iftype != NL80211_IFTYPE_AP) && (ndev_vif->iftype != NL80211_IFTYPE_P2P_GO)))
		goto exit_with_error;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	if ((ndev_vif->chan->hw_value <= 14) && (!sdev->fw_SoftAp_2g_40mhz_enabled) &&
	    (ndev_vif->chandef->width == NL80211_CHAN_WIDTH_40)) {
		SLSI_NET_ERR(dev, "Configuration error: 40 MHz on 2.4 GHz is not supported. Channel_no: %d Channel_width: %d\n", ndev_vif->chan->hw_value, slsi_get_chann_info(sdev, ndev_vif->chandef));
		goto exit_with_error;
	}
#else
	if ((ndev_vif->chan->hw_value <= 14) && (!sdev->fw_SoftAp_2g_40mhz_enabled) &&
	    (ndev_vif->channel_type > NL80211_CHAN_HT20)) {
		SLSI_NET_ERR(dev, "Configuration error: 40 MHz on 2.4 GHz is not supported. Channel_no: %d Channel_width: %d\n", ndev_vif->chan->hw_value, slsi_get_chann_info(sdev, ndev_vif->channel_type));
		goto exit_with_error;
	}
#endif

	return 0;

exit_with_error:
	return -EINVAL;
}

static int slsi_get_max_bw_mhz(struct slsi_dev *sdev, u16 prim_chan_cf)
{
	int i;
	struct ieee80211_regdomain *regd = sdev->device_config.domain_info.regdomain;

	if (!regd) {
		SLSI_WARN(sdev, "NO regdomain info\n");
		return 0;
	}

	for (i = 0; i < regd->n_reg_rules; i++) {
		if ((regd->reg_rules[i].freq_range.start_freq_khz / 1000 <= prim_chan_cf - 10) &&
		    (regd->reg_rules[i].freq_range.end_freq_khz / 1000 >= prim_chan_cf + 10))
			return regd->reg_rules[i].freq_range.max_bandwidth_khz / 1000;
	}

	SLSI_WARN(sdev, "Freq(%d) not found in regdomain\n", prim_chan_cf);
	return 0;
}

#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
void slsi_store_settings_for_recovery(struct cfg80211_ap_settings *settings, struct netdev_vif *ndev_vif)
{
	ndev_vif->backup_settings = *settings;
	if (&ndev_vif->backup_settings == settings)
		return;
	ndev_vif->backup_settings.chandef.chan =
	(struct ieee80211_channel *)slsi_mem_dup((u8 *)settings->chandef.chan, sizeof(struct ieee80211_channel));
	ndev_vif->backup_settings.beacon.head = slsi_mem_dup((u8 *)settings->beacon.head, settings->beacon.head_len);
	ndev_vif->backup_settings.beacon.tail = slsi_mem_dup((u8 *)settings->beacon.tail, settings->beacon.tail_len);
	ndev_vif->backup_settings.beacon.beacon_ies = slsi_mem_dup((u8 *)settings->beacon.beacon_ies,
								   settings->beacon.beacon_ies_len);
	ndev_vif->backup_settings.beacon.proberesp_ies = slsi_mem_dup((u8 *)settings->beacon.proberesp_ies,
								      settings->beacon.proberesp_ies_len);
	ndev_vif->backup_settings.beacon.assocresp_ies = slsi_mem_dup((u8 *)settings->beacon.assocresp_ies,
								      settings->beacon.assocresp_ies_len);
	ndev_vif->backup_settings.beacon.probe_resp = slsi_mem_dup((u8 *)settings->beacon.probe_resp,
								   settings->beacon.probe_resp_len);
	ndev_vif->backup_settings.ssid = slsi_mem_dup((u8 *)settings->ssid, settings->ssid_len);
	if (settings->ht_cap) {
		ndev_vif->backup_settings.ht_cap =
		(struct ieee80211_ht_cap *)slsi_mem_dup((u8 *)settings->ht_cap, sizeof(struct ieee80211_ht_cap));
	}
	if (settings->vht_cap) {
		ndev_vif->backup_settings.vht_cap =
		(struct ieee80211_vht_cap *)slsi_mem_dup((u8 *)settings->vht_cap, sizeof(struct ieee80211_vht_cap));
	}
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
static bool slsi_ap_chandef_vht_ht(struct net_device *dev, struct slsi_dev *sdev, int wifi_sharing_channel_switched)
#else
static bool slsi_ap_chandef_vht_ht(struct net_device *dev, struct slsi_dev *sdev)
#endif
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	bool append_vht_ies = false;

	SLSI_NET_DBG1(dev, SLSI_MLME, "Channel: %d, Maximum bandwidth: %d\n", ndev_vif->chandef->chan->hw_value,
		      slsi_get_max_bw_mhz(sdev, ndev_vif->chandef->chan->center_freq));
	/* 11ac configuration (5GHz and VHT) */
	if (ndev_vif->chandef->chan->hw_value >= 36 && ndev_vif->chandef->chan->hw_value < 165 &&
	    sdev->fw_vht_enabled && sdev->allow_switch_80_mhz &&
	    (slsi_get_max_bw_mhz(sdev, ndev_vif->chandef->chan->center_freq) >= 80)) {
		u16 oper_chan = ndev_vif->chandef->chan->hw_value;

		append_vht_ies = true;
		ndev_vif->chandef->width = NL80211_CHAN_WIDTH_80;

		SLSI_NET_DBG1(dev, SLSI_MLME, "5 GHz- Include VHT\n");
		if (oper_chan >= 36 && oper_chan <= 48)
			ndev_vif->chandef->center_freq1 = ieee80211_channel_to_frequency(42, NL80211_BAND_5GHZ);
		else if (oper_chan >= 149 && oper_chan <= 161)
			ndev_vif->chandef->center_freq1 = ieee80211_channel_to_frequency(155, NL80211_BAND_5GHZ);
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
		/* In wifi sharing case, AP can start on STA channel even though it is DFS channel*/
		if (wifi_sharing_channel_switched == 1) {
			if (oper_chan >= 52 && oper_chan <= 64)
				ndev_vif->chandef->center_freq1 = ieee80211_channel_to_frequency(58,
												 NL80211_BAND_5GHZ);
			else if (oper_chan >= 100 && oper_chan <= 112)
				ndev_vif->chandef->center_freq1 = ieee80211_channel_to_frequency(106,
												 NL80211_BAND_5GHZ);
			else if (oper_chan >= 116 && oper_chan <= 128)
				ndev_vif->chandef->center_freq1 = ieee80211_channel_to_frequency(122,
												 NL80211_BAND_5GHZ);
			else if (oper_chan >= 132 && oper_chan <= 144)
				ndev_vif->chandef->center_freq1 = ieee80211_channel_to_frequency(138,
												 NL80211_BAND_5GHZ);
		}
#endif
	} else if (sdev->fw_ht_enabled && sdev->allow_switch_40_mhz &&
			   slsi_get_max_bw_mhz(sdev, ndev_vif->chandef->chan->center_freq) >= 40 &&
			   ((ndev_vif->chandef->chan->hw_value < 165 && ndev_vif->chandef->chan->hw_value >= 36))) {
		/* HT40 configuration (5GHz/2GHz and HT) */
		u16  oper_chan = ndev_vif->chandef->chan->hw_value;
		u8   bw_40_minus_channels[] = { 40, 48, 153, 161, 5, 6, 7, 8, 9, 10, 11 };
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
		u8 bw_40_minus_dfs_channels[] = { 144, 136, 128, 120, 112, 104, 64, 56 };
#endif
		u8   ch;

		ndev_vif->chandef->width = NL80211_CHAN_WIDTH_40;
		ndev_vif->chandef->center_freq1 =  ndev_vif->chandef->chan->center_freq + 10;
		for (ch = 0; ch < ARRAY_SIZE(bw_40_minus_channels); ch++)
			if (oper_chan == bw_40_minus_channels[ch]) {
				ndev_vif->chandef->center_freq1 =  ndev_vif->chandef->chan->center_freq - 10;
				break;
			}

#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
		if (wifi_sharing_channel_switched == 1) {
			for (ch = 0; ch < ARRAY_SIZE(bw_40_minus_dfs_channels); ch++)
				if (oper_chan == bw_40_minus_dfs_channels[ch]) {
					ndev_vif->chandef->center_freq1 =  ndev_vif->chandef->chan->center_freq - 10;
					break;
				}
		}
#endif
	}
	return append_vht_ies;
}
#endif

int slsi_start_ap(struct wiphy *wiphy, struct net_device *dev,
		  struct cfg80211_ap_settings *settings)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct net_device *wlan_dev;
	u8                device_address[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	int               r = 0;
	const u8          *wpa_ie_pos = NULL;
	int               wpa_ie_len = 0;
	const u8          *wmm_ie_pos = NULL;
	int               wmm_ie_len = 0;
	const u8          *country_ie = NULL;
	char              alpha2[SLSI_COUNTRY_CODE_LEN];
	bool              append_vht_ies = false;
	const u8             *ie;
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	int wifi_sharing_channel_switched = 0;
	struct netdev_vif *ndev_sta_vif;
	int invalid_channel = 0;
#endif
	int skip_indoor_check_for_wifi_sharing = 0;
	u8 *ds_params_ie = NULL;
	struct ieee80211_mgmt  *mgmt;
	u16                    beacon_ie_head_len;
	u8 *ht_operation_ie = NULL;
	struct ieee80211_channel  *channel = NULL;
	int indoor_channel = 0;
	int i;
	u32 chan_flags;
	u16 center_freq;

	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	if (sdev->device_state != SLSI_DEVICE_STATE_STARTED) {
		SLSI_WARN(sdev, "device not started yet (device_state:%d)\n", sdev->device_state);
		r = -EINVAL;
		goto exit_with_start_stop_mutex;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	/* Abort any ongoing wlan scan. */
	wlan_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	if (wlan_dev)
		slsi_abort_hw_scan(sdev, wlan_dev);

	SLSI_NET_DBG1(dev, SLSI_CFG80211, "AP frequency received: %d\n", settings->chandef.chan->center_freq);
	mgmt = (struct ieee80211_mgmt *)settings->beacon.head;
	beacon_ie_head_len = settings->beacon.head_len - ((u8 *)mgmt->u.beacon.variable - (u8 *)mgmt);
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	ndev_sta_vif = netdev_priv(wlan_dev);
	if (SLSI_IS_VIF_INDEX_MHS(sdev, ndev_vif)) {
		if (ndev_sta_vif) {
			SLSI_MUTEX_LOCK(ndev_sta_vif->vif_mutex);
			if ((ndev_sta_vif->activated) && (ndev_sta_vif->vif_type == FAPI_VIFTYPE_STATION) &&
			    (ndev_sta_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTING ||
			     ndev_sta_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED)) {
#ifdef CONFIG_SCSC_WLAN_SINGLE_ANTENNA
				invalid_channel = slsi_get_mhs_ws_chan_vsdb(wiphy, dev, settings, sdev,
									    &wifi_sharing_channel_switched);
#else
				invalid_channel = slsi_get_mhs_ws_chan_rsdb(wiphy, dev, settings, sdev,
									    &wifi_sharing_channel_switched);
#endif
				skip_indoor_check_for_wifi_sharing = 1;
				if (invalid_channel) {
					SLSI_NET_ERR(dev, "Rejecting AP start req at host (invalid channel)\n");
					SLSI_MUTEX_UNLOCK(ndev_sta_vif->vif_mutex);
					r = -EINVAL;
					goto exit_with_vif_mutex;
				}

				SLSI_DBG1(sdev, SLSI_CFG80211, "Station frequency: %d, SoftAP frequency: %d\n",
					  ndev_sta_vif->chan->center_freq, settings->chandef.chan->center_freq);
			}
			SLSI_MUTEX_UNLOCK(ndev_sta_vif->vif_mutex);
		}
	}
#endif

	memset(&ndev_vif->ap, 0, sizeof(ndev_vif->ap));
	/* Initialise all allocated peer structures to remove old data. */
	/*slsi_netif_init_all_peers(sdev, dev);*/

	/* Reg domain changes */
	country_ie = cfg80211_find_ie(WLAN_EID_COUNTRY, settings->beacon.tail, settings->beacon.tail_len);
	if (country_ie) {
		country_ie += 2;
		memcpy(alpha2, country_ie, SLSI_COUNTRY_CODE_LEN);
		if (memcmp(sdev->device_config.domain_info.regdomain->alpha2, alpha2, SLSI_COUNTRY_CODE_LEN - 1) != 0) {
			if (slsi_set_country_update_regd(sdev, alpha2, SLSI_COUNTRY_CODE_LEN) != 0) {
				r = -EINVAL;
				goto exit_with_vif_mutex;
			}
		}
	}
	if (!skip_indoor_check_for_wifi_sharing && sdev->band_5g_supported &&
	    ((settings->chandef.chan->center_freq / 1000) == 5)) {
		channel =  ieee80211_get_channel(sdev->wiphy, settings->chandef.chan->center_freq);
		if (!channel) {
			SLSI_ERR(sdev, "Invalid frequency %d used to start AP. Channel not found\n",
				 settings->chandef.chan->center_freq);
			r = -EINVAL;
			goto exit_with_vif_mutex;
		}
		if (ndev_vif->iftype != NL80211_IFTYPE_P2P_GO) {
			if ((channel->flags) & (IEEE80211_CHAN_INDOOR_ONLY)) {
				chan_flags = (IEEE80211_CHAN_INDOOR_ONLY | IEEE80211_CHAN_RADAR |
					      IEEE80211_CHAN_DISABLED |
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 13)
					      IEEE80211_CHAN_PASSIVE_SCAN
#else
					      IEEE80211_CHAN_NO_IR
#endif
					     );

				for (i = 0; i < wiphy->bands[NL80211_BAND_5GHZ]->n_channels; i++) {
					if (!(wiphy->bands[NL80211_BAND_5GHZ]->channels[i].flags & chan_flags)) {
						center_freq = wiphy->bands[NL80211_BAND_5GHZ]->channels[i].center_freq;
						settings->chandef.chan = ieee80211_get_channel(wiphy, center_freq);
						if (!settings->chandef.chan) {
							SLSI_NET_DBG2(dev, SLSI_MLME, "Invalid chan for frequency %d\n", center_freq);
							continue;
						}
						settings->chandef.center_freq1 = center_freq;
						SLSI_DBG1(sdev, SLSI_CFG80211, "ap valid frequency:%d,chan_flags:%x\n",
							  center_freq,
							  wiphy->bands[NL80211_BAND_5GHZ]->channels[i].flags);
						indoor_channel = 1;
						break;
					}
				}
				if (indoor_channel == 0) {
					SLSI_ERR(sdev, "No valid channel found to start the AP");
					r = -EINVAL;
					goto exit_with_vif_mutex;
				}
			}
		}
	}

	r = slsi_ap_start_validate(dev, sdev, settings);
	if (r != 0)
		goto exit_with_vif_mutex;

	if (ndev_vif->iftype == NL80211_IFTYPE_P2P_GO) {
		struct net_device   *p2p_dev;

		slsi_p2p_group_start_remove_unsync_vif(sdev);
		p2p_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2P);
		SLSI_ETHER_COPY(device_address, p2p_dev->dev_addr);
		if (keep_alive_period != SLSI_P2PGO_KEEP_ALIVE_PERIOD_SEC)
			if (slsi_set_uint_mib(sdev, NULL, SLSI_PSID_UNIFI_MLMEGO_KEEP_ALIVE_TIMEOUT,
					      keep_alive_period) != 0) {
				SLSI_NET_ERR(dev, "P2PGO Keep Alive MIB set failed");
				r = -EINVAL;
				goto exit_with_vif_mutex;
			}
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	append_vht_ies = slsi_ap_chandef_vht_ht(dev, sdev, wifi_sharing_channel_switched);
	if (slsi_check_channelization(sdev, ndev_vif->chandef, wifi_sharing_channel_switched) != 0) {
#else
	append_vht_ies = slsi_ap_chandef_vht_ht(dev, sdev);
	if (slsi_check_channelization(sdev, ndev_vif->chandef, 0) != 0) {
#endif
#else
	if (slsi_check_channelization(sdev, ndev_vif->channel_type) != 0) {
#endif
		r = -EINVAL;
		goto exit_with_vif_mutex;
	}

	if (ndev_vif->iftype == NL80211_IFTYPE_AP) {
		/* Legacy AP */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
		if (ndev_vif->chandef->width == NL80211_CHAN_WIDTH_20)
#else
		if (ndev_vif->channel_type == NL80211_CHAN_HT20)
#endif
			slsi_ap_start_obss_scan(sdev, dev, ndev_vif);
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	if (ndev_vif->chandef->width <= NL80211_CHAN_WIDTH_20) {
		/* Enable LDPC, SGI20 and SGI40 for both SoftAP & P2PGO if firmware supports */
		if (cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, settings->beacon.tail, settings->beacon.tail_len)) {
			u8 enforce_ht_cap1 = sdev->fw_ht_cap[0] & (IEEE80211_HT_CAP_LDPC_CODING |
								  IEEE80211_HT_CAP_SGI_20);
			u8 enforce_ht_cap2 = sdev->fw_ht_cap[1] & (IEEE80211_HT_CAP_RX_STBC >> 8);

			slsi_modify_ies(dev, WLAN_EID_HT_CAPABILITY, (u8 *)settings->beacon.tail,
					settings->beacon.tail_len, 2, enforce_ht_cap1);
			slsi_modify_ies(dev, WLAN_EID_HT_CAPABILITY, (u8 *)settings->beacon.tail,
					settings->beacon.tail_len, 3, enforce_ht_cap2);
		}
	} else if (cfg80211_chandef_valid(ndev_vif->chandef)) {
		u8 *ht_operation_ie;
		u8 sec_chan_offset = 0;
		u8 ch;
		u8 bw_40_minus_channels[] = { 40, 48, 153, 161, 5, 6, 7, 8, 9, 10, 11 };

		ht_operation_ie = (u8 *)cfg80211_find_ie(WLAN_EID_HT_OPERATION, settings->beacon.tail,
							 settings->beacon.tail_len);
		if (!ht_operation_ie) {
			SLSI_NET_ERR(dev, "HT Operation IE is not passed by wpa_supplicant");
			r = -EINVAL;
			goto exit_with_vif_mutex;
		}

		sec_chan_offset = IEEE80211_HT_PARAM_CHA_SEC_ABOVE;
		for (ch = 0; ch < ARRAY_SIZE(bw_40_minus_channels); ch++)
			if (bw_40_minus_channels[ch] == ndev_vif->chandef->chan->hw_value) {
				sec_chan_offset = IEEE80211_HT_PARAM_CHA_SEC_BELOW;
				break;
			}

		/* Change HT Information IE subset 1 */
		ht_operation_ie += 3;
		*(ht_operation_ie) |= sec_chan_offset;
		*(ht_operation_ie) |= IEEE80211_HT_PARAM_CHAN_WIDTH_ANY;

		/* For 80MHz, Enable HT Capabilities : Support 40MHz Channel Width, SGI20 and SGI40
		 * for AP (both softAp as well as P2P GO), if firmware supports.
		 */
		if (cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, settings->beacon.tail,
				     settings->beacon.tail_len)) {
			u8 enforce_ht_cap1 = sdev->fw_ht_cap[0] & (IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
								  IEEE80211_HT_CAP_SGI_20 |
								  IEEE80211_HT_CAP_SGI_40 |
								  IEEE80211_HT_CAP_LDPC_CODING);
			u8 enforce_ht_cap2 = sdev->fw_ht_cap[1] & (IEEE80211_HT_CAP_RX_STBC >> 8);

			slsi_modify_ies(dev, WLAN_EID_HT_CAPABILITY, (u8 *)settings->beacon.tail,
					settings->beacon.tail_len, 2, enforce_ht_cap1);
			slsi_modify_ies(dev, WLAN_EID_HT_CAPABILITY, (u8 *)settings->beacon.tail,
					settings->beacon.tail_len, 3, enforce_ht_cap2);
		}
	}
#endif

	if (indoor_channel == 1
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	|| (wifi_sharing_channel_switched == 1)
#endif
	) {
		slsi_modify_ies_on_channel_switch(dev, settings, ds_params_ie, ht_operation_ie, mgmt, beacon_ie_head_len);
	}
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;

	if (slsi_mlme_add_vif(sdev, dev, dev->dev_addr, device_address) != 0) {
		SLSI_NET_ERR(dev, "slsi_mlme_add_vif failed\n");
		r = -EINVAL;
		goto exit_with_vif_mutex;
	}

	if (slsi_vif_activated(sdev, dev) != 0) {
		SLSI_NET_ERR(dev, "slsi_vif_activated failed\n");
		goto exit_with_vif;
	}

	/* Extract the WMM and WPA IEs from settings->beacon.tail - This is sent in add_info_elements and shouldn't be included in start_req
	 * Cache IEs to be used in later add_info_elements_req. The IEs would be freed during AP stop
	 */
	wpa_ie_pos = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPA, settings->beacon.tail, settings->beacon.tail_len);
	if (wpa_ie_pos) {
		wpa_ie_len = *(wpa_ie_pos + 1) + 2;     /* For 0xdd (1) and Tag Length (1) */
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "WPA IE found: Length = %zu\n", wpa_ie_len);
		SLSI_EC_GOTO(slsi_cache_ies(wpa_ie_pos, wpa_ie_len, &ndev_vif->ap.cache_wpa_ie, &ndev_vif->ap.wpa_ie_len), r, exit_with_vif);
	}

	wmm_ie_pos = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WMM, settings->beacon.tail, settings->beacon.tail_len);
	if (wmm_ie_pos) {
		wmm_ie_len = *(wmm_ie_pos + 1) + 2;
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "WMM IE found: Length = %zu\n", wmm_ie_len);
		SLSI_EC_GOTO(slsi_cache_ies(wmm_ie_pos, wmm_ie_len, &ndev_vif->ap.cache_wmm_ie, &ndev_vif->ap.wmm_ie_len), r, exit_with_vif);
	}

	slsi_clear_cached_ies(&ndev_vif->ap.add_info_ies, &ndev_vif->ap.add_info_ies_len);

	/* Set Vendor specific IEs (WPA, WMM, WPS, P2P) for Beacon, Probe Response and Association Response
	 * The Beacon and Assoc Rsp IEs can include Extended Capability (WLAN_EID_EXT_CAPAB) IE when supported.
	 * Some other IEs (like internetworking, etc) can also come if supported.
	 * The add_info should include only vendor specific IEs and other IEs should be removed if supported in future.
	 */
	if ((wmm_ie_pos) || (wpa_ie_pos) || (settings->beacon.beacon_ies_len > 0 && settings->beacon.beacon_ies)) {
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "Add info elements for beacon\n");
		SLSI_EC_GOTO(slsi_ap_prepare_add_info_ies(ndev_vif, settings->beacon.beacon_ies, settings->beacon.beacon_ies_len), r, exit_with_vif);
		SLSI_EC_GOTO(slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_BEACON, ndev_vif->ap.add_info_ies, ndev_vif->ap.add_info_ies_len), r, exit_with_vif);
		slsi_clear_cached_ies(&ndev_vif->ap.add_info_ies, &ndev_vif->ap.add_info_ies_len);
	}

	if ((wmm_ie_pos) || (wpa_ie_pos) || (settings->beacon.proberesp_ies_len > 0 && settings->beacon.proberesp_ies)) {
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "Add info elements for probe response\n");
		SLSI_EC_GOTO(slsi_ap_prepare_add_info_ies(ndev_vif, settings->beacon.proberesp_ies, settings->beacon.proberesp_ies_len), r, exit_with_vif);
		SLSI_EC_GOTO(slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_PROBE_RESPONSE, ndev_vif->ap.add_info_ies, ndev_vif->ap.add_info_ies_len), r, exit_with_vif);
		slsi_clear_cached_ies(&ndev_vif->ap.add_info_ies, &ndev_vif->ap.add_info_ies_len);
	}

	if ((wmm_ie_pos) || (wpa_ie_pos) || (settings->beacon.assocresp_ies_len > 0 && settings->beacon.assocresp_ies)) {
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "Add info elements for assoc response\n");
		SLSI_EC_GOTO(slsi_ap_prepare_add_info_ies(ndev_vif, settings->beacon.assocresp_ies, settings->beacon.assocresp_ies_len), r, exit_with_vif);
		SLSI_EC_GOTO(slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_ASSOCIATION_RESPONSE, ndev_vif->ap.add_info_ies, ndev_vif->ap.add_info_ies_len), r, exit_with_vif);
		slsi_clear_cached_ies(&ndev_vif->ap.add_info_ies, &ndev_vif->ap.add_info_ies_len);
	}

	if (ndev_vif->iftype == NL80211_IFTYPE_P2P_GO) {
		u32 af_bmap_active = SLSI_ACTION_FRAME_PUBLIC;
		u32 af_bmap_suspended = SLSI_ACTION_FRAME_PUBLIC;

		r = slsi_mlme_register_action_frame(sdev, dev, af_bmap_active, af_bmap_suspended);
		if (r != 0) {
			SLSI_NET_ERR(dev, "slsi_mlme_register_action_frame failed: resultcode = %d\n", r);
			goto exit_with_vif;
		}
	}

	if (append_vht_ies) {
		ndev_vif->ap.mode = SLSI_80211_MODE_11AC;
	} else if (cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, settings->beacon.tail, settings->beacon.tail_len) &&
		cfg80211_find_ie(WLAN_EID_HT_OPERATION, settings->beacon.tail, settings->beacon.tail_len)) {
		ndev_vif->ap.mode = SLSI_80211_MODE_11N;
	} else {
		ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, settings->beacon.tail, settings->beacon.tail_len);
		if (ie)
			ndev_vif->ap.mode = slsi_get_supported_mode(ie);
	}

	r = slsi_mlme_start(sdev, dev, dev->dev_addr, settings, wpa_ie_pos, wmm_ie_pos, append_vht_ies);

	cfg80211_ch_switch_notify(dev, &settings->chandef);

#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	if (r == 0)
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "Soft Ap started on frequency: %d\n",
			      settings->chandef.chan->center_freq);
	if (SLSI_IS_VIF_INDEX_MHS(sdev, ndev_vif))
		ndev_vif->chan = settings->chandef.chan;
#endif
	if (r != 0) {
		SLSI_NET_ERR(dev, "Start ap failed: resultcode = %d frequency = %d\n", r,
			     settings->chandef.chan->center_freq);
		goto exit_with_vif;
	} else if (ndev_vif->iftype == NL80211_IFTYPE_P2P_GO) {
		SLSI_P2P_STATE_CHANGE(sdev, P2P_GROUP_FORMED_GO);
	}
#ifdef CONFIG_SCSC_WLAN_NUM_ANTENNAS
	if (ndev_vif->iftype == NL80211_IFTYPE_AP) {
		/* Don't care results. */
		slsi_mlme_set_num_antennas(dev, 1 /*SISO*/, 0);
	}
#endif
	ndev_vif->ap.beacon_interval = settings->beacon_interval;
	ndev_vif->ap.ssid_len = settings->ssid_len;
	memcpy(ndev_vif->ap.ssid, settings->ssid, settings->ssid_len);

	netif_carrier_on(dev);

	if (ndev_vif->ipaddress != cpu_to_be32(0))
		/* Static IP is assigned already */
		slsi_ip_address_changed(sdev, dev, ndev_vif->ipaddress);

	r = slsi_read_disconnect_ind_timeout(sdev, SLSI_PSID_UNIFI_DISCONNECT_TIMEOUT);
	if (r != 0)
		sdev->device_config.ap_disconnect_ind_timeout = *sdev->sig_wait_cfm_timeout;

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "slsi_read_disconnect_ind_timeout: timeout = %d", sdev->device_config.ap_disconnect_ind_timeout);
	goto exit_with_vif_mutex;
exit_with_vif:
	slsi_clear_cached_ies(&ndev_vif->ap.add_info_ies, &ndev_vif->ap.add_info_ies_len);
	if (slsi_mlme_del_vif(sdev, dev) != 0)
		SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
	slsi_vif_deactivated(sdev, dev);
	r = -EINVAL;
exit_with_vif_mutex:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
exit_with_start_stop_mutex:
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);

#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
	slsi_store_settings_for_recovery(settings, ndev_vif);
#endif
	return r;
}

int slsi_change_beacon(struct wiphy *wiphy, struct net_device *dev,
		       struct cfg80211_beacon_data *info)
{
	SLSI_UNUSED_PARAMETER(info);

	return -EOPNOTSUPP;
}

int slsi_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	slsi_reset_throughput_stats(dev);

	return 0;
}

static int slsi_p2p_group_mgmt_tx(const struct ieee80211_mgmt *mgmt, struct wiphy *wiphy,
				  struct net_device *dev, struct ieee80211_channel *chan,
				  unsigned int wait, const u8 *buf, size_t len,
				  bool dont_wait_for_ack, u64 *cookie)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif;
	struct net_device *netdev;
	int               subtype = slsi_get_public_action_subtype(mgmt);
	int               r = 0;
	u32               host_tag = slsi_tx_mgmt_host_tag(sdev);
	u16               freq = 0;
	u32               dwell_time = SLSI_FORCE_SCHD_ACT_FRAME_MSEC;
	u16               data_unit_desc = FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME;

	if (sdev->p2p_group_exp_frame != SLSI_PA_INVALID) {
		SLSI_NET_ERR(dev, "sdev->p2p_group_exp_frame : %d\n", sdev->p2p_group_exp_frame);
		return -EINVAL;
	}
	netdev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	ndev_vif = netdev_priv(netdev);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_NET_DBG2(dev, SLSI_CFG80211, "Sending Action frame (%s) on p2p group vif (%d), vif_index = %d,"
		      "vif_type = %d, chan->hw_value = %d, ndev_vif->chan->hw_value = %d, wait = %d,"
		      "sdev->p2p_group_exp_frame = %d\n", slsi_pa_subtype_text(subtype), ndev_vif->activated,
		      ndev_vif->ifnum, ndev_vif->vif_type, chan->hw_value, ndev_vif->chan->hw_value, wait,
		      ndev_vif->chan->hw_value);

	if (!((ndev_vif->iftype == NL80211_IFTYPE_P2P_GO) || (ndev_vif->iftype == NL80211_IFTYPE_P2P_CLIENT)))
		goto exit_with_error;

	if (chan->hw_value != ndev_vif->chan->hw_value) {
		freq = SLSI_FREQ_HOST_TO_FW(chan->center_freq);
		dwell_time = wait;
	}

	/* Incase of GO dont wait for resp/cfm packets for go-negotiation.*/
	if (subtype != SLSI_P2P_PA_GO_NEG_RSP)
		sdev->p2p_group_exp_frame = slsi_get_exp_peer_frame_subtype(subtype);

	r = slsi_mlme_send_frame_mgmt(sdev, netdev, buf, len, data_unit_desc, FAPI_MESSAGETYPE_IEEE80211_ACTION, host_tag, freq, dwell_time * 1000, 0);
	if (r)
		goto exit_with_lock;
	slsi_assign_cookie_id(cookie, &ndev_vif->mgmt_tx_cookie);
	r = slsi_set_mgmt_tx_data(ndev_vif, *cookie, host_tag, buf, len);         /* If error then it is returned in exit */
	goto exit_with_lock;

exit_with_error:
	r = -EINVAL;
exit_with_lock:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

/* Handle mgmt_tx callback for P2P modes */
static int slsi_p2p_mgmt_tx(const struct ieee80211_mgmt *mgmt, struct wiphy *wiphy,
			    struct net_device *dev, struct netdev_vif *ndev_vif,
			    struct ieee80211_channel *chan, unsigned int wait,
			    const u8 *buf, size_t len, bool dont_wait_for_ack, u64 *cookie)
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	int             ret = 0;

	if (ieee80211_is_action(mgmt->frame_control)) {
		u16 host_tag = slsi_tx_mgmt_host_tag(sdev);
		int subtype = slsi_get_public_action_subtype(mgmt);
		u8  exp_peer_frame;
		u32 dwell_time = 0;

		SLSI_NET_DBG2(dev, SLSI_CFG80211, "Action frame (%s), unsync_vif_active (%d)\n", slsi_pa_subtype_text(subtype), ndev_vif->activated);

		if (subtype == SLSI_PA_INVALID) {
			SLSI_NET_ERR(dev, "Invalid Action frame subtype\n");
			goto exit_with_error;
		}

		/* Check if unsync vif is available */
		if (sdev->p2p_state == P2P_IDLE_NO_VIF)
			if (slsi_p2p_vif_activate(sdev, dev, chan, wait, false) != 0)
				goto exit_with_error;

			/* Clear Probe Response IEs if vif was already present with a different channel */
		if (ndev_vif->driver_channel != chan->hw_value) {
			if (slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_PROBE_RESPONSE, NULL, 0) != 0)
				SLSI_NET_ERR(dev, "Clearing Probe Response IEs failed for unsync vif\n");
			slsi_unsync_vif_set_probe_rsp_ie(ndev_vif, NULL, 0);

			if (slsi_mlme_set_channel(sdev, dev, chan, SLSI_FW_CHANNEL_DURATION_UNSPECIFIED, 0, 0) != 0)
				goto exit_with_vif;
			else {
				ndev_vif->chan = chan;
				ndev_vif->driver_channel = chan->hw_value;
			}
		}

		/* Check if peer frame response is expected */
		exp_peer_frame = slsi_get_exp_peer_frame_subtype(subtype);

		if (exp_peer_frame != SLSI_PA_INVALID) {
			if ((subtype == SLSI_P2P_PA_GO_NEG_RSP) && (slsi_p2p_get_go_neg_rsp_status(dev, mgmt) != SLSI_P2P_STATUS_CODE_SUCCESS)) {
				SLSI_NET_DBG1(dev, SLSI_CFG80211, "GO_NEG_RSP Tx, peer response not expected\n");
				exp_peer_frame = SLSI_PA_INVALID;
			} else {
				SLSI_NET_DBG1(dev, SLSI_CFG80211, "Peer response expected with action frame (%s)\n",
					      slsi_pa_subtype_text(exp_peer_frame));

				if (ndev_vif->mgmt_tx_data.exp_frame != SLSI_PA_INVALID)
					(void)slsi_set_mgmt_tx_data(ndev_vif, 0, 0, NULL, 0);

				/* Change Force Schedule Duration as peer response is expected */
				if (wait)
					dwell_time = wait;
				else
					dwell_time = SLSI_FORCE_SCHD_ACT_FRAME_MSEC;
			}
		}

		slsi_assign_cookie_id(cookie, &ndev_vif->mgmt_tx_cookie);

		/* Send the action frame, transmission status indication would be received later */
		if (slsi_mlme_send_frame_mgmt(sdev, dev, buf, len, FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME, FAPI_MESSAGETYPE_IEEE80211_ACTION, host_tag, 0, dwell_time * 1000, 0) != 0)
			goto exit_with_vif;
		if (subtype == SLSI_P2P_PA_GO_NEG_CFM)
			ndev_vif->drv_in_p2p_procedure = false;
		else if ((subtype == SLSI_P2P_PA_GO_NEG_REQ) || (subtype == SLSI_P2P_PA_PROV_DISC_REQ))
			ndev_vif->drv_in_p2p_procedure = true;
		/* If multiple frames are requested for tx, only the info of first frame would be stored */
		if (ndev_vif->mgmt_tx_data.host_tag == 0) {
			unsigned int n_wait = 0;

			SLSI_NET_DBG1(dev, SLSI_CFG80211, "Store mgmt frame tx data for cookie = 0x%llx\n", *cookie);

			ret = slsi_set_mgmt_tx_data(ndev_vif, *cookie, host_tag, buf, len);
			if (ret != 0)
				goto exit_with_vif;
			ndev_vif->mgmt_tx_data.exp_frame = exp_peer_frame;

			SLSI_P2P_STATE_CHANGE(sdev, P2P_ACTION_FRAME_TX_RX);
			if ((exp_peer_frame == SLSI_P2P_PA_GO_NEG_RSP) || (exp_peer_frame == SLSI_P2P_PA_GO_NEG_CFM))
				/* Retain vif for larger duration that wpa_supplicant asks to wait,
				 * during GO-Negotiation to allow peer to retry GO neg in bad radio condition.
				 * Some of phones retry GO-Negotiation after 2 seconds
				 */
				n_wait = SLSI_P2P_NEG_PROC_UNSYNC_VIF_RETAIN_DURATION;
			else if (exp_peer_frame != SLSI_PA_INVALID)
				/* If a peer response is expected queue work to retain vif till wait time else the work will be handled in mgmt_tx_cancel_wait */
				n_wait = wait + SLSI_P2P_MGMT_TX_EXTRA_MSEC;
			if (n_wait) {
				SLSI_NET_DBG2(dev, SLSI_CFG80211, "retain unsync vif for duration (%d) msec\n", n_wait);
				slsi_p2p_queue_unsync_vif_del_work(ndev_vif, n_wait);
			}
		} else {
			/* Already a frame Tx is in progress, send immediate tx_status as success. Sending immediate tx status should be ok
			 * as supplicant is in another procedure and so these frames would be mostly only response frames.
			 */
			WARN_ON(sdev->p2p_state != P2P_ACTION_FRAME_TX_RX);

			if (!dont_wait_for_ack) {
				SLSI_NET_DBG1(dev, SLSI_CFG80211, "Send immediate tx_status (cookie = 0x%llx)\n", *cookie);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
				cfg80211_mgmt_tx_status(&ndev_vif->wdev, *cookie, buf, len, true, GFP_KERNEL);
#else
				cfg80211_mgmt_tx_status(dev, *cookie, buf, len, true, GFP_KERNEL);
#endif
			}
		}
		goto exit;
	}

	/* Else send failure for unexpected management frame */
	SLSI_NET_ERR(dev, "Drop Tx frame: Unexpected Management frame\n");
	goto exit_with_error;

exit_with_vif:
	if (sdev->p2p_state != P2P_LISTENING)
		slsi_p2p_vif_deactivate(sdev, dev, true);
exit_with_error:
	ret = -EINVAL;
exit:
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
int     slsi_mgmt_tx_cancel_wait(struct wiphy        *wiphy,
				 struct wireless_dev *wdev,
				 u64                 cookie)
{
	struct net_device *dev = wdev->netdev;

#else
int     slsi_mgmt_tx_cancel_wait(struct wiphy      *wiphy,
				 struct net_device *dev,
				 u64               cookie)
{
#endif          /*  (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)) */
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "iface_num = %d, cookie = 0x%llx, vif_index = %d, vif_type = %d,"
		      "sdev->p2p_state = %d, ndev_vif->mgmt_tx_data.cookie = 0x%llx, sdev->p2p_group_exp_frame = %d,"
		      "sdev->wlan_unsync_vif_state = %d\n", ndev_vif->ifnum, cookie,
		      ndev_vif->vif_type, sdev->p2p_state, ndev_vif->mgmt_tx_data.cookie,
		      sdev->p2p_group_exp_frame, sdev->wlan_unsync_vif_state);

	/* If device was in frame tx_rx state, clear mgmt tx data and change state */
	if ((sdev->p2p_state == P2P_ACTION_FRAME_TX_RX) && (ndev_vif->mgmt_tx_data.cookie == cookie)) {
		if (ndev_vif->mgmt_tx_data.exp_frame != SLSI_PA_INVALID)
			(void)slsi_mlme_reset_dwell_time(sdev, dev);

		(void)slsi_set_mgmt_tx_data(ndev_vif, 0, 0, NULL, 0);
		ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;

		if (delayed_work_pending(&ndev_vif->unsync.roc_expiry_work)) {
			SLSI_P2P_STATE_CHANGE(sdev, P2P_LISTENING);
		} else {
			slsi_p2p_queue_unsync_vif_del_work(ndev_vif, SLSI_P2P_UNSYNC_VIF_EXTRA_MSEC);
			SLSI_P2P_STATE_CHANGE(ndev_vif->sdev, P2P_IDLE_VIF_ACTIVE);
		}
	} else if ((SLSI_IS_P2P_GROUP_STATE(sdev)) && (sdev->p2p_group_exp_frame != SLSI_PA_INVALID)) {
		/* acquire mutex lock if it is not group net dev */
		slsi_clear_offchannel_data(sdev, (!SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif)) ? true : false);
	} else if ((sdev->wlan_unsync_vif_state == WLAN_UNSYNC_VIF_TX) && (ndev_vif->mgmt_tx_data.cookie == cookie)) {
		sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_ACTIVE;
		cancel_delayed_work(&ndev_vif->unsync.hs2_del_vif_work);
		queue_delayed_work(sdev->device_wq, &ndev_vif->unsync.hs2_del_vif_work, msecs_to_jiffies(SLSI_HS2_UNSYNC_VIF_EXTRA_MSEC));
		if (ndev_vif->mgmt_tx_data.exp_frame != SLSI_PA_INVALID) {
			ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
			(void)slsi_mlme_reset_dwell_time(sdev, dev);
		}
	} else if (ndev_vif->activated && ndev_vif->vif_type == FAPI_VIFTYPE_STATION
		   && ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED) {
		if (ndev_vif->mgmt_tx_data.exp_frame != SLSI_PA_INVALID) {
			ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
			(void)slsi_mlme_reset_dwell_time(sdev, dev);
		}
	}

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
void slsi_mgmt_frame_register(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      u16 frame_type, bool reg)
{
	struct net_device *dev = wdev->netdev;

#else
void slsi_mgmt_frame_register(struct wiphy *wiphy,
			      struct net_device *dev,
			      u16 frame_type, bool reg)
{
#endif          /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9)) */
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	SLSI_UNUSED_PARAMETER(frame_type);
	SLSI_UNUSED_PARAMETER(reg);
#endif

	if (WARN_ON(!dev))
		return;

	SLSI_UNUSED_PARAMETER(sdev);
}

int slsi_wlan_mgmt_tx(struct slsi_dev *sdev, struct net_device *dev,
		      struct ieee80211_channel *chan, unsigned int wait,
		      const u8 *buf, size_t len, bool dont_wait_for_ack, u64 *cookie)
{
	u32                   host_tag = slsi_tx_mgmt_host_tag(sdev);
	struct netdev_vif     *ndev_vif = netdev_priv(dev);
	int                   r = 0;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)buf;
	u8                    exp_peer_frame = SLSI_PA_INVALID;
	int                   subtype = SLSI_PA_INVALID;

	if (!ieee80211_is_auth(mgmt->frame_control))
		slsi_wlan_dump_public_action_subtype(sdev, mgmt, true);

	if (ieee80211_is_action(mgmt->frame_control)) {
		subtype = slsi_get_public_action_subtype(mgmt);

		if (subtype != SLSI_PA_INVALID)
			exp_peer_frame = slsi_get_exp_peer_frame_subtype(subtype);
	}

	if (!ndev_vif->activated) {
		if (subtype >= SLSI_PA_GAS_INITIAL_REQ_SUBTYPE && subtype <= SLSI_PA_GAS_COMEBACK_RSP_SUBTYPE) {
			ndev_vif->mgmt_tx_gas_frame = true;
			SLSI_ETHER_COPY(ndev_vif->gas_frame_mac_addr, mgmt->sa);
		} else {
			ndev_vif->mgmt_tx_gas_frame = false;
		}
		r = slsi_wlan_unsync_vif_activate(sdev, dev, chan, wait);
		if (r)
			return r;

		r = slsi_mlme_send_frame_mgmt(sdev, dev, buf, len, FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME, FAPI_MESSAGETYPE_IEEE80211_ACTION, host_tag, 0, wait * 1000, 0);
		if (r)
			goto exit_with_vif;

		sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_TX;
		queue_delayed_work(sdev->device_wq, &ndev_vif->unsync.hs2_del_vif_work, msecs_to_jiffies(wait));
	} else {
		/* vif is active*/
		if (ieee80211_is_auth(mgmt->frame_control)) {
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "Transmit on the current frequency\n");
			r = slsi_mlme_send_frame_mgmt(sdev, dev, buf, len, FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME,
						      FAPI_MESSAGETYPE_IEEE80211_MGMT, host_tag, 0, wait * 1000, 0);
			if (r)
				return r;
		} else if (ndev_vif->vif_type == FAPI_VIFTYPE_UNSYNCHRONISED) {
			if (subtype >= SLSI_PA_GAS_INITIAL_REQ_SUBTYPE && subtype <= SLSI_PA_GAS_COMEBACK_RSP_SUBTYPE) {
				slsi_wlan_unsync_vif_deactivate(sdev, dev, true);
				ndev_vif->mgmt_tx_gas_frame = true;
				SLSI_ETHER_COPY(ndev_vif->gas_frame_mac_addr, mgmt->sa);
				r = slsi_wlan_unsync_vif_activate(sdev, dev, chan, wait);
				if (r)
					return r;
			} else {
				if (ndev_vif->mgmt_tx_gas_frame) {
					slsi_wlan_unsync_vif_deactivate(sdev, dev, true);
					ndev_vif->mgmt_tx_gas_frame = false;
					r = slsi_wlan_unsync_vif_activate(sdev, dev, chan, wait);
					if (r)
						return r;
				}
			}

			cancel_delayed_work(&ndev_vif->unsync.hs2_del_vif_work);
			/*even if we fail to cancel the delayed work, we shall go ahead and send action frames*/
			if (ndev_vif->driver_channel != chan->hw_value) {
				r = slsi_mlme_set_channel(sdev, dev, chan, SLSI_FW_CHANNEL_DURATION_UNSPECIFIED, 0, 0);
				if (r)
					goto exit_with_vif;
				else
					ndev_vif->driver_channel = chan->hw_value;
			}
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "wlan unsync vif is active, send frame on channel freq = %d\n", chan->center_freq);
			r = slsi_mlme_send_frame_mgmt(sdev, dev, buf, len, FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME, FAPI_MESSAGETYPE_IEEE80211_ACTION, host_tag, 0, wait * 1000, 0);
			if (r)
				goto exit_with_vif;
			sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_TX;
			queue_delayed_work(sdev->device_wq, &ndev_vif->unsync.hs2_del_vif_work, msecs_to_jiffies(wait));
		} else if (ndev_vif->chan->hw_value == chan->hw_value) {
			/* Dwell time not provided when sending frames on connected channel. */
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "STA VIF is active on same channel, send frame on channel freq %d\n", chan->center_freq);
			r = slsi_mlme_send_frame_mgmt(sdev, dev, buf, len, FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME, FAPI_MESSAGETYPE_IEEE80211_ACTION, host_tag, 0, 0, 0);
			if (r)
				return r;
		} else {
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "STA VIF is active on a different channel, send frame on channel freq %d\n", chan->center_freq);
			/* Dwell time for GAS (ANQP) request packet set to 100ms if dwell time(wait) is more than 100ms */
			if ((subtype == SLSI_PA_GAS_INITIAL_REQ_SUBTYPE || subtype == SLSI_PA_GAS_COMEBACK_REQ_SUBTYPE) && wait > SLSI_FW_MAX_OFFCHANNEL_DWELL_TIME)
				wait = SLSI_FW_MAX_OFFCHANNEL_DWELL_TIME;
			r = slsi_mlme_send_frame_mgmt(sdev, dev, buf, len, FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME, FAPI_MESSAGETYPE_IEEE80211_ACTION, host_tag, SLSI_FREQ_HOST_TO_FW(chan->center_freq), wait * 1000, 0);
			if (r)
				return r;
		}
	}

	ndev_vif->mgmt_tx_data.exp_frame = exp_peer_frame;
	slsi_assign_cookie_id(cookie, &ndev_vif->mgmt_tx_cookie);
	slsi_set_mgmt_tx_data(ndev_vif, *cookie, host_tag, buf, len);
	return r;

exit_with_vif:
	slsi_wlan_unsync_vif_deactivate(sdev, dev, true);
	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
int slsi_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		 struct cfg80211_mgmt_tx_params *params,
		 u64 *cookie)
{
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
int slsi_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		 struct ieee80211_channel *chan, bool offchan,
		 unsigned int wait, const u8 *buf, size_t len, bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	struct net_device *dev = wdev->netdev;

#else
int slsi_mgmt_tx(struct wiphy *wiphy, struct net_device *dev,
		 struct ieee80211_channel *chan, bool offchan,
		 enum nl80211_channel_type channel_type,
		 bool channel_type_valid, unsigned int wait,
		 const u8 *buf, size_t len, bool no_cck,
		 bool dont_wait_for_ack, u64 *cookie)
{
#endif          /*  (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)) */

	/* Note to explore for AP ::All public action frames which come to host should be handled properly
	 * Additionally, if PMF is negotiated over the link, the host shall not issue "mlme-send-frame.request"
	 * primitive  for action frames before the pairwise keys have been installed in F/W. Presently, for
	 * SoftAP with PMF support, there is no scenario in which slsi_mlme_send_frame will be called for
	 * action frames for VIF TYPE = AP.
	 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	struct net_device           *dev = wdev->netdev;
	struct ieee80211_channel    *chan = params->chan;
	bool                        offchan = params->offchan;
	unsigned int                wait = params->wait;
	const u8                    *buf = params->buf;
	size_t                      len = params->len;
	bool                        no_cck = params->no_cck;
	bool                        dont_wait_for_ack = params->dont_wait_for_ack;
#endif

	struct slsi_dev             *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif           *ndev_vif = netdev_priv(dev);
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *)buf;
	int                         r = 0;

	SLSI_UNUSED_PARAMETER(offchan);
	SLSI_UNUSED_PARAMETER(no_cck);
	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	if (sdev->device_state != SLSI_DEVICE_STATE_STARTED) {
		SLSI_WARN(sdev, "device not started yet (device_state:%d)\n", sdev->device_state);
		r = -EINVAL;
		goto exit;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!(ieee80211_is_auth(mgmt->frame_control))) {
		SLSI_NET_DBG2(dev, SLSI_CFG80211, "Mgmt Frame Tx: iface_num = %d, channel = %d, wait = %d, noAck = %d,"
			      "offchannel = %d, mgmt->frame_control = %d, vif_type = %d\n", ndev_vif->ifnum, chan->hw_value,
			      wait, dont_wait_for_ack, offchan, mgmt->frame_control, ndev_vif->vif_type);
	} else {
		SLSI_INFO(sdev, "Send Auth Frame\n");
	}

	if (!(ieee80211_is_mgmt(mgmt->frame_control))) {
		SLSI_NET_ERR(dev, "Drop Tx frame: Not a Management frame\n");
		r = -EINVAL;
		goto exit;
	}
	if (SLSI_IS_VIF_INDEX_WLAN(ndev_vif) || (ndev_vif->iftype == NL80211_IFTYPE_AP && (ieee80211_is_auth(mgmt->frame_control)))) {
		r = slsi_wlan_mgmt_tx(SDEV_FROM_WIPHY(wiphy), dev, chan, wait, buf, len, dont_wait_for_ack, cookie);
		goto exit;
	}

	/*P2P*/

	/* Drop Probe Responses which can come in P2P Device and P2P Group role */
	if (ieee80211_is_probe_resp(mgmt->frame_control)) {
		/* Ideally supplicant doesn't expect Tx status for Probe Rsp. Send tx status just in case it requests ack */
		if (!dont_wait_for_ack) {
			slsi_assign_cookie_id(cookie, &ndev_vif->mgmt_tx_cookie);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
			cfg80211_mgmt_tx_status(wdev, *cookie, buf, len, true, GFP_KERNEL);
#else
			cfg80211_mgmt_tx_status(dev, *cookie, buf, len, true, GFP_KERNEL);
#endif
		}
		goto exit;
	}

	if (SLSI_IS_VIF_INDEX_P2P(ndev_vif)) {
		struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
		/* Check whether STA scan is running or not. If yes, then abort the STA scan */
		slsi_abort_sta_scan(sdev);
		if (SLSI_IS_P2P_GROUP_STATE(sdev))
			r = slsi_p2p_group_mgmt_tx(mgmt, wiphy, dev, chan, wait, buf, len, dont_wait_for_ack, cookie);
		else
			r = slsi_p2p_mgmt_tx(mgmt, wiphy, dev, ndev_vif, chan, wait, buf, len, dont_wait_for_ack, cookie);
	} else if (SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif))
		if (chan->hw_value == ndev_vif->chan->hw_value) {
			struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
			u16             host_tag = slsi_tx_mgmt_host_tag(sdev);

			r = slsi_mlme_send_frame_mgmt(sdev, dev, buf, len, FAPI_DATAUNITDESCRIPTOR_IEEE802_11_FRAME, FAPI_MESSAGETYPE_IEEE80211_ACTION, host_tag, 0, 0, 0);
			if (r) {
				SLSI_NET_ERR(dev, "Failed to send action frame, r = %d\n", r);
				goto exit;
			}
			slsi_assign_cookie_id(cookie, &ndev_vif->mgmt_tx_cookie);
			r = slsi_set_mgmt_tx_data(ndev_vif, *cookie, host_tag, buf, len);
		}
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return r;
}

/* cw = (2^n -1). But WMM IE needs value n. */
u8 slsi_get_ecw(int cw)
{
	int ecw = 0;

	cw = cw + 1;
	do {
		cw = cw >> 1;
		ecw++;
	} while (cw);
	return ecw - 1;
}

int slsi_set_txq_params(struct wiphy *wiphy, struct net_device *ndev,
			struct ieee80211_txq_params *params)
{
	struct slsi_dev                   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif                 *ndev_vif = netdev_priv(ndev);
	struct slsi_wmm_parameter_element *wmm_ie = &ndev_vif->ap.wmm_ie;
	int                               r = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
	int                               ac = params->ac;
#else
	int                               ac = params->queue;
#endif
	/* Index remapping for AC from nl80211_ac enum to slsi_ac_index_wmm enum (index to be used in the IE).
	 * Kernel version less than 3.5.0 doesn't support nl80211_ac enum hence not using the nl80211_ac enum.
	 * Eg. NL80211_AC_VO (index value 0) would be remapped to AC_VO (index value 3).
	 * Don't change the order of array elements.
	 */
	u8  ac_index_map[4] = { AC_VO, AC_VI, AC_BE, AC_BK };
	int ac_remapped = ac_index_map[ac];

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_NET_DBG2(ndev, SLSI_CFG80211, " ac= %x, ac_remapped = %d aifs = %d, cmin=%x cmax = %x, txop = %x,"
		      "vif_index = %d vif_type = %d", ac, ac_remapped, params->aifs, params->cwmin, params->cwmax,
		      params->txop, ndev_vif->ifnum, ndev_vif->vif_type);

	if (ndev_vif->activated) {
		wmm_ie->ac[ac_remapped].aci_aifsn = (ac_remapped << 5) | (params->aifs & 0x0f);
		wmm_ie->ac[ac_remapped].ecw = ((slsi_get_ecw(params->cwmax)) << 4) | ((slsi_get_ecw(params->cwmin)) & 0x0f);
		wmm_ie->ac[ac_remapped].txop_limit = cpu_to_le16(params->txop);
		if (ac == 3) {
			wmm_ie->eid = SLSI_WLAN_EID_VENDOR_SPECIFIC;
			wmm_ie->len = 24;
			wmm_ie->oui[0] = 0x00;
			wmm_ie->oui[1] = 0x50;
			wmm_ie->oui[2] = 0xf2;
			wmm_ie->oui_type = WLAN_OUI_TYPE_MICROSOFT_WMM;
			wmm_ie->oui_subtype = 1;
			wmm_ie->version = 1;
			wmm_ie->qos_info = 0;
			wmm_ie->reserved = 0;
			r = slsi_mlme_add_info_elements(sdev, ndev, FAPI_PURPOSE_LOCAL, (const u8 *)wmm_ie, sizeof(struct slsi_wmm_parameter_element));
			if (r)
				SLSI_NET_ERR(ndev, "Error sending TX Queue Parameters for AP error = %d", r);
		}
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
int slsi_synchronised_response(struct wiphy *wiphy, struct net_device *dev,
			       struct cfg80211_external_auth_params *params)
{
	struct slsi_dev                   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif                 *ndev_vif = netdev_priv(dev);
	int r;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	r = slsi_mlme_synchronised_response(sdev, dev, params);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
static int slsi_update_ft_ies(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_update_ft_ies_params *ftie)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               r = 0;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		const u8 *keo_ie_pos = NULL;
		u8 *ie_buf = NULL;
		int ie_len = 0;
		int ie_buf_len = 0;

		keo_ie_pos = cfg80211_find_vendor_ie(WLAN_OUI_SAMSUNG, WLAN_OUI_TYPE_SAMSUNG_KEO,
						     ndev_vif->sta.assoc_req_add_info_elem,
						     ndev_vif->sta.assoc_req_add_info_elem_len);
		if (keo_ie_pos) {
			ie_buf_len = ftie->ie_len +
				     ndev_vif->sta.assoc_req_add_info_elem_len -
				     (keo_ie_pos - ndev_vif->sta.assoc_req_add_info_elem);
			ie_buf = kmalloc(ie_buf_len, GFP_KERNEL);
			if (!ie_buf) {
				SLSI_NET_ERR(dev, "kmalloc failed\n");
				SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
				return -ENOMEM;
			}
			ie_len = ftie->ie_len;
			if (ie_buf_len < ie_len) {
				SLSI_NET_ERR(dev, "ft_ie buffer overflow!!\n");
				kfree(ie_buf);
				ie_buf = NULL;
				SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
				return -EINVAL;
			}
			memcpy(ie_buf, ftie->ie, ie_len);
			if ((ie_buf_len - ie_len) >= ((int)keo_ie_pos[1] + 2)) {
				memcpy(&ie_buf[ie_len], keo_ie_pos, ((int)keo_ie_pos[1] + 2));
				ie_len += (keo_ie_pos[1] + 2);
				keo_ie_pos += (keo_ie_pos[1] + 2);
			} else {
				SLSI_NET_ERR(dev, "ie_buf buffer overflow!!\n");
				kfree(ie_buf);
				ie_buf = NULL;
				SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
				return -EINVAL;
			}
			while ((ndev_vif->sta.assoc_req_add_info_elem_len -
			       (keo_ie_pos - ndev_vif->sta.assoc_req_add_info_elem)) > 2) {
				keo_ie_pos = cfg80211_find_vendor_ie(WLAN_OUI_SAMSUNG, WLAN_OUI_TYPE_SAMSUNG_KEO,
								     keo_ie_pos,
								     ndev_vif->sta.assoc_req_add_info_elem_len -
								     (keo_ie_pos - ndev_vif->sta.assoc_req_add_info_elem));
				if (!keo_ie_pos)
					break;
				if ((ie_buf_len - ie_len) >= ((int)keo_ie_pos[1] + 2)) {
					memcpy(&ie_buf[ie_len], keo_ie_pos, (keo_ie_pos[1] + 2));
					ie_len += (keo_ie_pos[1] + 2);
					keo_ie_pos += (keo_ie_pos[1] + 2);
				} else {
					SLSI_NET_ERR(dev, "ie_buf buffer overflow!\n");
					kfree(ie_buf);
					ie_buf = NULL;
					SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
					return -EINVAL;
				}
			}
			r = slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_ASSOCIATION_REQUEST, ie_buf, ie_len);
		} else {
			r = slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_ASSOCIATION_REQUEST, ftie->ie, ftie->ie_len);
		}
		kfree(ie_buf);
		ie_buf = NULL;
	}

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

#ifdef CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
int slsi_set_mac_acl_per_mac(struct wiphy *wiphy, struct net_device *dev,
			     const struct cfg80211_acl_data *params)
{
	struct slsi_dev          *sdev           = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif        *ndev_vif       = netdev_priv(dev);
	int                      r               = 0;
	int                      i               = 0;
	int                      malloc_len      = 0;
	struct cfg80211_acl_data *saved_acl_data = NULL;
	int                      last_index      = 0;
	struct mac_address       zero_addr       = {0};
	bool                     found_flag      = false;

	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, WlanLite FW does not support MLME_SET_ACL.request\n");
		return -EOPNOTSUPP;
	}
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (ndev_vif->vif_type != FAPI_VIFTYPE_AP) {
		SLSI_NET_ERR(dev, "Invalid vif type: %d\n", ndev_vif->vif_type);
		r = -EINVAL;
		goto exit;
	}
	SLSI_NET_DBG2(dev, SLSI_CFG80211, "ACL:: Policy: %d  Number of stations: %d\n", params->acl_policy, params->n_acl_entries);

	if (params->n_acl_entries != 1) {
		SLSI_NET_ERR(dev, "n_acl_entries != 1, No action taken\n");
		goto exit;
	}

	saved_acl_data = ndev_vif->ap.acl_data_blacklist;
	if (!saved_acl_data) {
		if (params->acl_policy == NL80211_ACL_POLICY_DENY_UNLESS_LISTED) {
			SLSI_NET_ERR(dev, "Deletion requested on empty list\n");
			r = -EINVAL;
			goto exit;
		}
		malloc_len = sizeof(struct cfg80211_acl_data) + sizeof(struct mac_address) * SLSI_ACL_MAX_BSSID_COUNT;
		saved_acl_data = kmalloc(malloc_len, GFP_KERNEL);
		if (!saved_acl_data) {
			SLSI_ERR(sdev, "Memory Allocation failure for ACL List");
			r = -ENOMEM;
			goto exit;
		}
		memset(saved_acl_data, 0, malloc_len);
		ndev_vif->ap.acl_data_blacklist = saved_acl_data;
	}

	last_index = saved_acl_data->n_acl_entries;
	if (params->acl_policy == NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED) {	/* Add mac address on the blacklist */
		if (SLSI_ETHER_EQUAL(params->mac_addrs[0].addr, zero_addr.addr)) {
			SLSI_NET_ERR(dev, "Addition of addr 00:00:00:00:00:00 in blacklist\n");
			r = -EINVAL;
			goto exit;
		}
		for (i = 0 ; i < last_index; i++) { /*Check for duplicate entries*/
			if (SLSI_ETHER_EQUAL(saved_acl_data->mac_addrs[i].addr, params->mac_addrs[0].addr)) {
				SLSI_NET_INFO(dev, "Mac addr already present in blacklist\n");
				r = 0;
				goto exit;
			}
		}
		for (i = 0 ; i < last_index + 1 && i < SLSI_ACL_MAX_BSSID_COUNT; i++) {
			if (SLSI_ETHER_EQUAL(saved_acl_data->mac_addrs[i].addr, zero_addr.addr)) {
				SLSI_ETHER_COPY(saved_acl_data->mac_addrs[i].addr, params->mac_addrs[0].addr);
				if (i == last_index)
					last_index = i + 1;
				break;
			}
		}
		if (i == SLSI_ACL_MAX_BSSID_COUNT) {
			SLSI_NET_ERR(dev, "Blacklist is full\n");
			r = -EINVAL;
			goto exit;
		}
	} else if (params->acl_policy == NL80211_ACL_POLICY_DENY_UNLESS_LISTED) {	/* Delete mac address from the blacklist */
		found_flag = false;
		for (i = 0 ; i < last_index; i++) {
			if (SLSI_ETHER_EQUAL(saved_acl_data->mac_addrs[i].addr, params->mac_addrs[0].addr)) {
				SLSI_ETHER_COPY(saved_acl_data->mac_addrs[i].addr, zero_addr.addr);
				found_flag = true;
				if (i == last_index - 1) {
					while (i >= 0 && SLSI_ETHER_EQUAL(saved_acl_data->mac_addrs[i].addr, zero_addr.addr))
						i--;
					last_index = i + 1;
				}
				break;
			}
		}
		if (!found_flag) {
			r = -EINVAL;
			SLSI_NET_ERR(dev, "Deletion requested for addr not present in blacklist");
			goto exit;
		}
	}
	saved_acl_data->n_acl_entries = last_index;
	r = slsi_mlme_set_acl(sdev, dev, ndev_vif->ifnum, saved_acl_data->acl_policy, saved_acl_data->n_acl_entries, saved_acl_data->mac_addrs);
exit:
	if (!last_index) {
		ndev_vif->ap.acl_data_blacklist = NULL;
		kfree(saved_acl_data);
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}
#endif

int slsi_set_mac_acl(struct wiphy *wiphy, struct net_device *dev,
		     const struct cfg80211_acl_data *params)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               r = 0;

	if (slsi_is_test_mode_enabled()) {
		SLSI_NET_INFO(dev, "Skip sending signal, WlanLite FW does not support MLME_SET_ACL.request\n");
		return -EOPNOTSUPP;
	}
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (FAPI_VIFTYPE_AP != ndev_vif->vif_type) {
		SLSI_NET_ERR(dev, "Invalid vif type: %d\n", ndev_vif->vif_type);
		r = -EINVAL;
		goto exit;
	}
	SLSI_NET_DBG2(dev, SLSI_CFG80211, "ACL:: Policy: %d  Number of stations: %d\n", params->acl_policy, params->n_acl_entries);
	r = slsi_mlme_set_acl(sdev, dev, ndev_vif->ifnum, params->acl_policy, params->n_acl_entries, (struct mac_address *)params->mac_addrs);
	if (r != 0)
		SLSI_NET_ERR(dev, "mlme_set_acl_req returned with CFM failure\n");
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}
#endif

static struct cfg80211_ops slsi_ops = {
	.add_virtual_intf = slsi_add_virtual_intf,
	.del_virtual_intf = slsi_del_virtual_intf,
	.change_virtual_intf = slsi_change_virtual_intf,

	.scan = slsi_scan,
	.connect = slsi_connect,
	.disconnect = slsi_disconnect,

	.add_key = slsi_add_key,
	.del_key = slsi_del_key,
	.get_key = slsi_get_key,
	.set_default_key = slsi_set_default_key,
	.set_default_mgmt_key = slsi_config_default_mgmt_key,

	.set_wiphy_params = slsi_set_wiphy_params,

	.del_station = slsi_del_station,
	.get_station = slsi_get_station,
	.set_tx_power = slsi_set_tx_power,
	.get_tx_power = slsi_get_tx_power,
	.set_power_mgmt = slsi_set_power_mgmt,

	.suspend = slsi_suspend,
	.resume = slsi_resume,

	.set_pmksa = slsi_set_pmksa,
	.del_pmksa = slsi_del_pmksa,
	.flush_pmksa = slsi_flush_pmksa,

	.remain_on_channel = slsi_remain_on_channel,
	.cancel_remain_on_channel = slsi_cancel_remain_on_channel,

	.change_bss = slsi_change_bss,
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 5, 0))
	.set_channel = slsi_set_channel,
#endif          /* (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 5, 0)) */

	.start_ap = slsi_start_ap,
	.change_beacon = slsi_change_beacon,
	.stop_ap = slsi_stop_ap,

	.sched_scan_start = slsi_sched_scan_start,
	.sched_scan_stop = slsi_sched_scan_stop,

	.mgmt_frame_register = slsi_mgmt_frame_register,
	.mgmt_tx = slsi_mgmt_tx,
	.mgmt_tx_cancel_wait = slsi_mgmt_tx_cancel_wait,
	.set_txq_params = slsi_set_txq_params,
#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
	.external_auth = slsi_synchronised_response,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#ifdef CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
	.set_mac_acl = slsi_set_mac_acl_per_mac,
#else
	.set_mac_acl = slsi_set_mac_acl,
#endif
	.update_ft_ies = slsi_update_ft_ies,
#endif
	.tdls_oper = slsi_tdls_oper,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	.set_monitor_channel = slsi_set_monitor_channel,
	.set_qos_map = slsi_set_qos_map,
#endif
	.channel_switch = slsi_channel_switch
};

#define RATE_LEGACY(_rate, _hw_value, _flags) { \
		.bitrate = (_rate), \
		.hw_value = (_hw_value), \
		.flags = (_flags), \
}

#define CHAN2G(_freq, _idx)  { \
		.band = NL80211_BAND_2GHZ, \
		.center_freq = (_freq), \
		.hw_value = (_idx), \
		.max_power = 17, \
}

#define CHAN5G(_freq, _idx)  { \
		.band = NL80211_BAND_5GHZ, \
		.center_freq = (_freq), \
		.hw_value = (_idx), \
		.max_power = 17, \
}

static struct ieee80211_channel slsi_2ghz_channels[] = {
	CHAN2G(2412, 1),
	CHAN2G(2417, 2),
	CHAN2G(2422, 3),
	CHAN2G(2427, 4),
	CHAN2G(2432, 5),
	CHAN2G(2437, 6),
	CHAN2G(2442, 7),
	CHAN2G(2447, 8),
	CHAN2G(2452, 9),
	CHAN2G(2457, 10),
	CHAN2G(2462, 11),
	CHAN2G(2467, 12),
	CHAN2G(2472, 13),
	CHAN2G(2484, 14),
};

static struct ieee80211_rate    slsi_11g_rates[] = {
	RATE_LEGACY(10,  1,  0),
	RATE_LEGACY(20,  2,  IEEE80211_RATE_SHORT_PREAMBLE),
	RATE_LEGACY(55,  3,  IEEE80211_RATE_SHORT_PREAMBLE),
	RATE_LEGACY(110, 6,  IEEE80211_RATE_SHORT_PREAMBLE),
	RATE_LEGACY(60,  4,  0),
	RATE_LEGACY(90,  5,  0),
	RATE_LEGACY(120, 7,  0),
	RATE_LEGACY(180, 8,  0),
	RATE_LEGACY(240, 9,  0),
	RATE_LEGACY(360, 10, 0),
	RATE_LEGACY(480, 11, 0),
	RATE_LEGACY(540, 12, 0),
};

static struct ieee80211_channel slsi_5ghz_channels[] = {
	/* _We_ call this UNII 1 */
	CHAN5G(5180, 36),
	CHAN5G(5200, 40),
	CHAN5G(5220, 44),
	CHAN5G(5240, 48),
	/* UNII 2 */
	CHAN5G(5260, 52),
	CHAN5G(5280, 56),
	CHAN5G(5300, 60),
	CHAN5G(5320, 64),
	/* "Middle band" */
	CHAN5G(5500, 100),
	CHAN5G(5520, 104),
	CHAN5G(5540, 108),
	CHAN5G(5560, 112),
	CHAN5G(5580, 116),
	CHAN5G(5600, 120),
	CHAN5G(5620, 124),
	CHAN5G(5640, 128),
	CHAN5G(5660, 132),
	CHAN5G(5680, 136),
	CHAN5G(5700, 140),
	CHAN5G(5720, 144),
	/* UNII 3 */
	CHAN5G(5745, 149),
	CHAN5G(5765, 153),
	CHAN5G(5785, 157),
	CHAN5G(5805, 161),
	CHAN5G(5825, 165),
};

/* note fw_rate_idx_to_host_11a_idx[] below must change if this table changes */

static struct ieee80211_rate       wifi_11a_rates[] = {
	RATE_LEGACY(60,  4,  0),
	RATE_LEGACY(90,  5,  0),
	RATE_LEGACY(120, 7,  0),
	RATE_LEGACY(180, 8,  0),
	RATE_LEGACY(240, 9,  0),
	RATE_LEGACY(360, 10, 0),
	RATE_LEGACY(480, 11, 0),
	RATE_LEGACY(540, 12, 0),
};

static struct ieee80211_sta_ht_cap slsi_ht_cap = {
	.ht_supported       = true,
	.cap                = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
			      IEEE80211_HT_CAP_LDPC_CODING |
			      IEEE80211_HT_CAP_RX_STBC |
			      IEEE80211_HT_CAP_GRN_FLD |
			      IEEE80211_HT_CAP_SGI_20 |
			      IEEE80211_HT_CAP_SGI_40,
	.ampdu_factor       = IEEE80211_HT_MAX_AMPDU_64K,
	.ampdu_density      = IEEE80211_HT_MPDU_DENSITY_4,
	.mcs                = {
		.rx_mask    = { 0xff, 0, },
		.rx_highest = cpu_to_le16(0),
		.tx_params  = 0,
	},
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
struct ieee80211_sta_vht_cap       slsi_vht_cap = {
	.vht_supported = true,
	.cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_7991 |
	       IEEE80211_VHT_CAP_SHORT_GI_80 |
	       IEEE80211_VHT_CAP_RXSTBC_1 |
	       IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE |
	       (5 << IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT),
	.vht_mcs = {
		.rx_mcs_map = cpu_to_le16(0xfffe),
		.rx_highest = cpu_to_le16(0),
		.tx_mcs_map = cpu_to_le16(0xfffe),
		.tx_highest = cpu_to_le16(0),
	},
};
#endif

struct ieee80211_supported_band    slsi_band_2ghz = {
	.channels   = slsi_2ghz_channels,
	.band       = NL80211_BAND_2GHZ,
	.n_channels = ARRAY_SIZE(slsi_2ghz_channels),
	.bitrates   = slsi_11g_rates,
	.n_bitrates = ARRAY_SIZE(slsi_11g_rates),
};

struct ieee80211_supported_band    slsi_band_5ghz = {
	.channels   = slsi_5ghz_channels,
	.band       = NL80211_BAND_5GHZ,
	.n_channels = ARRAY_SIZE(slsi_5ghz_channels),
	.bitrates   = wifi_11a_rates,
	.n_bitrates = ARRAY_SIZE(wifi_11a_rates),
};

static const u32                   slsi_cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	WLAN_CIPHER_SUITE_AES_CMAC,
	WLAN_CIPHER_SUITE_SMS4,
	WLAN_CIPHER_SUITE_PMK,
	WLAN_CIPHER_SUITE_GCMP,
	WLAN_CIPHER_SUITE_GCMP_256,
	WLAN_CIPHER_SUITE_CCMP_256,
	WLAN_CIPHER_SUITE_BIP_GMAC_128,
	WLAN_CIPHER_SUITE_BIP_GMAC_256
};

static const struct ieee80211_txrx_stypes
				   ieee80211_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_AUTH >> 4)
	},
	[NL80211_IFTYPE_STATION] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		      BIT(IEEE80211_STYPE_AUTH >> 4)
	},
	[NL80211_IFTYPE_P2P_GO] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4)
	},
};

/* Interface combinations supported by driver */
static struct ieee80211_iface_limit       iface_limits[] = {
#ifdef CONFIG_SCSC_WLAN_STA_ONLY
	/* Basic STA-only */
	{
		.max = CONFIG_SCSC_WLAN_MAX_INTERFACES,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
#else
	/* AP mode: # AP <= 1 on channel = 1 */
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_AP),
	},
	/* STA and P2P mode: #STA <= 1, #{P2P-client,P2P-GO} <= 1 on two channels */
	/* For P2P, the device mode and group mode is first started as STATION and then changed.
	 * Similarly it is changed to STATION on group removal. Hence set maximum interfaces for STATION.
	 */
	{
		.max = CONFIG_SCSC_WLAN_MAX_INTERFACES,
		.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO),
	},
	/* ADHOC mode: #ADHOC <= 1 on channel = 1 */
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_ADHOC),
	},
#endif
};

static struct ieee80211_regdomain         slsi_regdomain = {
	.reg_rules = {
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
	}
};

static struct ieee80211_iface_combination iface_comb[] = {
	{
		.limits = iface_limits,
		.n_limits = ARRAY_SIZE(iface_limits),
		.num_different_channels = 2,
		.max_interfaces = CONFIG_SCSC_WLAN_MAX_INTERFACES,
	},
};

#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
static struct cfg80211_wowlan slsi_wowlan_config = {
	.any = true,
};
#endif
#endif

struct slsi_dev                           *slsi_cfg80211_new(struct device *dev)
{
	struct wiphy    *wiphy;
	struct slsi_dev *sdev = NULL;

	SLSI_DBG1_NODEV(SLSI_CFG80211, "wiphy_new()\n");
	wiphy = wiphy_new(&slsi_ops, sizeof(struct slsi_dev));
	if (!wiphy) {
		SLSI_ERR_NODEV("wiphy_new() failed");
		return NULL;
	}

	sdev = (struct slsi_dev *)wiphy->priv;

	sdev->wiphy = wiphy;

	set_wiphy_dev(wiphy, dev);

	/* Allow changing of the netns, if NOT set then no changes are allowed */
	wiphy->flags |= WIPHY_FLAG_NETNS_OK;
	wiphy->flags |= WIPHY_FLAG_PS_ON_BY_DEFAULT;
	wiphy->flags |= WIPHY_FLAG_CONTROL_PORT_PROTOCOL;
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
#endif
	wiphy->flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH;
	wiphy->max_num_csa_counters = 0;

	wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME |
			WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD |
			WIPHY_FLAG_AP_UAPSD;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	wiphy->max_acl_mac_addrs = SLSI_AP_PEER_CONNECTIONS_MAX;
#endif

	wiphy->privid = sdev;

	wiphy->interface_modes =
#ifdef CONFIG_SCSC_WLAN_STA_ONLY
		BIT(NL80211_IFTYPE_STATION);
#else
		BIT(NL80211_IFTYPE_P2P_GO) |
		BIT(NL80211_IFTYPE_P2P_CLIENT) |
		BIT(NL80211_IFTYPE_STATION) |
		BIT(NL80211_IFTYPE_AP) |
		BIT(NL80211_IFTYPE_MONITOR) |
		BIT(NL80211_IFTYPE_ADHOC);
#endif
	slsi_band_2ghz.ht_cap = slsi_ht_cap;
	slsi_band_5ghz.ht_cap = slsi_ht_cap;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	slsi_band_5ghz.vht_cap = slsi_vht_cap;
#endif
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	wiphy->bands[NL80211_BAND_2GHZ] = &slsi_band_2ghz;
	wiphy->bands[NL80211_BAND_5GHZ] = &slsi_band_5ghz;

	memset(&sdev->device_config, 0, sizeof(struct slsi_dev_config));
	sdev->device_config.band_5G = &slsi_band_5ghz;
	sdev->device_config.band_2G = &slsi_band_2ghz;
	sdev->device_config.domain_info.regdomain = &slsi_regdomain;

	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	wiphy->max_remain_on_channel_duration = 5000;         /* 5000 msec */

	wiphy->cipher_suites = slsi_cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(slsi_cipher_suites);

	wiphy->extended_capabilities = slsi_extended_cap;
	wiphy->extended_capabilities_mask = slsi_extended_cap_mask;
	wiphy->extended_capabilities_len = ARRAY_SIZE(slsi_extended_cap);

	wiphy->mgmt_stypes = ieee80211_default_mgmt_stypes;

	/* Driver interface combinations */
	wiphy->n_iface_combinations = ARRAY_SIZE(iface_comb);
	wiphy->iface_combinations = iface_comb;

	/* Basic scan parameters */
	wiphy->max_scan_ssids = 10;
	wiphy->max_scan_ie_len = 2048;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 11, 0))
	/* Scheduled scanning support */
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
	/* Parameters for Scheduled Scanning Support */
	wiphy->max_sched_scan_reqs = 1;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#ifdef CONFIG_SCSC_WLAN_EXPONENTIAL_SCHED_SCAN
	wiphy->max_sched_scan_plans = 2;
	wiphy->max_sched_scan_plan_interval = 60;
	wiphy->max_sched_scan_plan_iterations = 3;
#endif
#endif
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_SCHED_SCAN_RELATIVE_RSSI);

	/* Randomize TA of Public Action frames. */
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_MGMT_TX_RANDOM_TA);
#endif

	/* Match the maximum number of SSIDs that could be requested from wpa_supplicant */
	wiphy->max_sched_scan_ssids = 16;

	/* To get a list of SSIDs rather than just the wildcard SSID need to support match sets */
	wiphy->max_match_sets = 16;

	wiphy->max_sched_scan_ie_len = 2048;

#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
	wiphy->wowlan = NULL;
	wiphy->wowlan_config = &slsi_wowlan_config;
#endif
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
	wiphy->regulatory_flags |= (REGULATORY_STRICT_REG |
					REGULATORY_CUSTOM_REG |
					REGULATORY_DISABLE_BEACON_HINTS |
					REGULATORY_COUNTRY_IE_IGNORE);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	wiphy->regulatory_flags |= (REGULATORY_STRICT_REG |
				    REGULATORY_CUSTOM_REG |
				    REGULATORY_DISABLE_BEACON_HINTS);
#endif
#ifndef CONFIG_SCSC_WLAN_STA_ONLY
	/* P2P flags */
	wiphy->flags |= WIPHY_FLAG_OFFCHAN_TX;

	/* Enable Probe response offloading w.r.t WPS and P2P */
	wiphy->probe_resp_offload |=
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2 |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P;

	/* TDLS support */
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS;
#endif
	/* Mac Randomization */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
	wiphy->features |= NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR;
	wiphy->features |= NL80211_FEATURE_SCHED_SCAN_RANDOM_MAC_ADDR;
#endif
#endif
#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
	wiphy->features |= NL80211_FEATURE_SAE;
#endif
#ifdef CONFIG_SCSC_WLAN_HE
	/* 11ax related parameters */
	wiphy->support_mbssid = 1;
	wiphy->support_only_he_mbssid = 1;
#endif
	return sdev;
}

int slsi_cfg80211_register(struct slsi_dev *sdev)
{
	SLSI_DBG1(sdev, SLSI_CFG80211, "wiphy_register()\n");
	return wiphy_register(sdev->wiphy);
}

void slsi_cfg80211_unregister(struct slsi_dev *sdev)
{
#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
	sdev->wiphy->wowlan = NULL;
	sdev->wiphy->wowlan_config = NULL;
#endif
#endif
	SLSI_DBG1(sdev, SLSI_CFG80211, "wiphy_unregister()\n");
	wiphy_unregister(sdev->wiphy);
}

void slsi_cfg80211_free(struct slsi_dev *sdev)
{
	SLSI_DBG1(sdev, SLSI_CFG80211, "wiphy_free()\n");
	wiphy_free(sdev->wiphy);
}

void slsi_cfg80211_update_wiphy(struct slsi_dev *sdev)
{
	/* Band 2G probably be disabled by slsi_band_cfg_update() while factory test or NCHO.
	 * So, we need to make sure that Band 2.4G enabled when initialized. */
	sdev->wiphy->bands[NL80211_BAND_2GHZ] = &slsi_band_2ghz;
	sdev->device_config.band_2G = &slsi_band_2ghz;

	/* update supported Bands */
	if (sdev->band_5g_supported) {
		sdev->wiphy->bands[NL80211_BAND_5GHZ] = &slsi_band_5ghz;
		sdev->device_config.band_5G = &slsi_band_5ghz;
		sdev->device_config.supported_band = SLSI_FREQ_BAND_AUTO;
	} else {
		sdev->wiphy->bands[NL80211_BAND_5GHZ] = NULL;
		sdev->device_config.band_5G = NULL;
		sdev->device_config.supported_band = SLSI_FREQ_BAND_2GHZ;
	}

	/* update HT features */
	if (sdev->fw_ht_enabled) {
		slsi_ht_cap.ht_supported = true;
		slsi_ht_cap.cap = le16_to_cpu(*(u16 *)sdev->fw_ht_cap);
		slsi_ht_cap.ampdu_density = (sdev->fw_ht_cap[2] & IEEE80211_HT_AMPDU_PARM_DENSITY) >> IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT;
		slsi_ht_cap.ampdu_factor = sdev->fw_ht_cap[2] & IEEE80211_HT_AMPDU_PARM_FACTOR;
	} else {
		slsi_ht_cap.ht_supported = false;
	}
	slsi_band_2ghz.ht_cap = slsi_ht_cap;
	slsi_band_5ghz.ht_cap = slsi_ht_cap;

	/* update VHT features */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	if (sdev->fw_vht_enabled) {
		slsi_vht_cap.vht_supported = true;
		slsi_vht_cap.cap = le32_to_cpu(*(u32 *)sdev->fw_vht_cap);
	} else {
		slsi_vht_cap.vht_supported = false;
	}
	slsi_band_5ghz.vht_cap = slsi_vht_cap;
#endif

	SLSI_INFO(sdev, "BANDS SUPPORTED -> 2.4:'%c' 5:'%c'\n", sdev->wiphy->bands[NL80211_BAND_2GHZ] ? 'Y' : 'N',
		  sdev->wiphy->bands[NL80211_BAND_5GHZ] ? 'Y' : 'N');
	SLSI_INFO(sdev, "HT/VHT SUPPORTED -> HT:'%c' VHT:'%c'\n", sdev->fw_ht_enabled ? 'Y' : 'N',
		  sdev->fw_vht_enabled ? 'Y' : 'N');
	SLSI_INFO(sdev, "HT  -> cap:0x%04x ampdu_density:%d ampdu_factor:%d\n", slsi_ht_cap.cap, slsi_ht_cap.ampdu_density, slsi_ht_cap.ampdu_factor);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	SLSI_INFO(sdev, "VHT -> cap:0x%08x\n", slsi_vht_cap.cap);
#endif
}
