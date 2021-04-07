/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "debug.h"
#include "mlme.h"
#include "nl80211_vendor_nan.h"

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, val) \
	{ \
		u16 attribute_len = 1; \
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (u8 *)&(val), 1); \
	}

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_U16(req, attribute, val) \
	{ \
		u16 attribute_len = 2; \
		__le16 le16val = cpu_to_le16(val); \
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (u8 *)&le16val, 2); \
	}

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_U32(req, attribute, val) \
	{ \
		u16 attribute_len = 4; \
		__le32 le32val = cpu_to_le32(val);\
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (u8 *)&le32val, 4); \
	}

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_DATA(req, attribute, val, val_len) \
	{ \
		u16 attribute_len = (val_len); \
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (val), (attribute_len)); \
	}

static u32 slsi_mlme_nan_append_tlv(struct sk_buff *req, u16 tag, u16 len, u8 *data)
{
	u8 *p;

	p = fapi_append_data_u16(req, tag);
	p = fapi_append_data_u16(req, len);
	p = fapi_append_data(req, data, len);
	if (p)
		return 0;
	return 1;
}

static u32 slsi_mlme_nan_append_service_info_tlv(struct sk_buff *req, struct slsi_hal_nan_data_path_app_info *app_info)
{
	int pos = 0;
	u16 length;
	int ret = 1;

	while (pos < (app_info->ndp_app_info_len - 3)) {
		length = app_info->ndp_app_info[pos + 2] << 8 | app_info->ndp_app_info[pos + 1];
		if (app_info->ndp_app_info[pos] == 0x01) {
			if ((length + pos + 3) <= app_info->ndp_app_info_len)
				ret = slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_WFA_SERVICE_INFO, length,
							       &app_info->ndp_app_info[pos + 3]);
			break;
		}
		pos = pos + length + 3; //Length is 2 bytes and tag is 1 byte
	}
	return ret;
}

static u16 slsi_mlme_nan_service_info_tlv_length(struct slsi_hal_nan_data_path_app_info *app_info)
{
	int pos = 0;
	u16 length = 0;

		while (pos < (app_info->ndp_app_info_len - 3)) {
			if (app_info->ndp_app_info[pos] == 0x01) {
				length = app_info->ndp_app_info[pos + 2] << 8 | app_info->ndp_app_info[pos + 1];
				break;
			}
			//Length is 2 bytes and tag is 1 byte
			pos = pos + app_info->ndp_app_info[pos + 1] + 3;
		}
	return length;
}

static u32 slsi_mlme_nan_append_config_tlv(struct sk_buff *req, u8 master_pref, u16 include_ps_id, u8 ps_id_count,
					   u16 include_ss_id, u8 ss_id_count, u16 rssi_window, u32 nmi_rand_interval,
					   u16 cluster_merge)
{
	u8 *p;

	p = fapi_append_data_u16(req, SLSI_NAN_TLV_TAG_CONFIGURATION);
	p = fapi_append_data_u16(req, 0x000f);
	p = fapi_append_data_u8(req, master_pref);

	/* publish service ID inclusion in beacon */
	p = fapi_append_data_bool(req, include_ps_id);
	p = fapi_append_data_u8(req, ps_id_count);

	/* subscribe service ID inclusion in beacon */
	p = fapi_append_data_bool(req, include_ss_id);
	p = fapi_append_data_u8(req, ss_id_count);

	p = fapi_append_data_u16(req, rssi_window);
	p = fapi_append_data_u32(req, nmi_rand_interval);
	p = fapi_append_data_u16(req, cluster_merge);

	if (p)
		return 0;
	return 1;
}

static u32 slsi_mlme_nan_append_band_specific_config(struct sk_buff *req, u16 tag, u8 rssi_close, u8 rssi_middle,
						     u8 rssi_proximity, u8 dwell_time, u16 scan_period,
						     u16 use_dw_int_val, u8 dw_interval)
{
	u8 *p;

	p = fapi_append_data_u16(req, tag);
	p = fapi_append_data_u16(req, 0x0009);
	p = fapi_append_data_u8(req, rssi_close);
	p = fapi_append_data_u8(req, rssi_middle);
	p = fapi_append_data_u8(req, rssi_proximity);
	p = fapi_append_data_u8(req, dwell_time);
	p = fapi_append_data_u16(req, scan_period);
	p = fapi_append_data_bool(req, use_dw_int_val);
	p = fapi_append_data_u8(req, dw_interval);
	if (p)
		return 0;
	return 1;
}

static u32 slsi_mlme_nan_append_2g4_band_specific_config(struct sk_buff *req, u8 rssi_close, u8 rssi_middle,
							 u8 rssi_proximity, u8 dwell_time, u16 scan_period,
							 u16 use_dw_int_val, u8 dw_interval)
{
	return slsi_mlme_nan_append_band_specific_config(req, SLSI_NAN_TLV_TAG_2G4_BAND_SPECIFIC_CONFIG, rssi_close,
							 rssi_middle, rssi_proximity, dwell_time, scan_period,
							 use_dw_int_val, dw_interval);
}

static u32 slsi_mlme_nan_append_5g_band_specific_config(struct sk_buff *req, u8 rssi_close, u8 rssi_middle,
							u8 rssi_proximity, u8 dwell_time, u16 scan_period,
							u16 use_dw_int_val, u8 dw_interval)
{
	return slsi_mlme_nan_append_band_specific_config(req, SLSI_NAN_TLV_TAG_5G_BAND_SPECIFIC_CONFIG, rssi_close,
							 rssi_middle, rssi_proximity, dwell_time, scan_period,
							 use_dw_int_val, dw_interval);
}

static u32 slsi_mlme_nan_append_config_supplemental(struct sk_buff *req, u32 db_interval_ms, u32 nss_discovery,
						    u32 enable_dw_early_termination, u32 enable_ranging)
{
	u8 *p;

	p = fapi_append_data_u16(req, SLSI_NAN_TLV_TAG_CONFIG_SUPPLEMENTAL);
	p = fapi_append_data_u16(req, 0x0007);
	p = fapi_append_data_u16(req, db_interval_ms);
	p = fapi_append_data_u8(req, nss_discovery);
	p = fapi_append_data_bool(req, enable_dw_early_termination);
	p = fapi_append_data_bool(req, enable_ranging);
	if (p)
		return 0;
	return 1;
}

static u32 slsi_mlme_nan_append_discovery_config(struct sk_buff *req, u8 sd_type, u8 tx_type, u16 ttl, u16 dw_period,
						 u8 dw_count, u8 disc_match_ind, u16 use_rssi_thres, u16 ranging_req,
						 u16 data_path_req)
{
	u8 *p;

	p = fapi_append_data_u16(req, SLSI_NAN_TLV_TAG_DISCOVERY_COMMON_SPECIFIC);
	p = fapi_append_data_u16(req, 0x000e);
	p = fapi_append_data_u8(req, sd_type);
	p = fapi_append_data_u8(req, tx_type);
	p = fapi_append_data_u16(req, ttl);
	p = fapi_append_data_u16(req, dw_period);
	p = fapi_append_data_u8(req, dw_count);
	p = fapi_append_data_u8(req, disc_match_ind);
	p = fapi_append_data_bool(req, use_rssi_thres);
	p = fapi_append_data_bool(req, ranging_req);
	p = fapi_append_data_bool(req, data_path_req);
	if (p)
		return 0;
	return 1;
}

static u32 slsi_mlme_nan_append_subscribe_specific(struct sk_buff *req, u8 srf_type, u16 respond_if_in_address_set,
						   u16 use_srf, u16 ssi_required_for_match)
{
	u8 *p;

	p = fapi_append_data_u16(req, SLSI_NAN_TLV_TAG_SUBSCRIBE_SPECIFIC);
	p = fapi_append_data_u16(req, 0x0007);
	p = fapi_append_data_u8(req, srf_type);
	p = fapi_append_data_u16(req, respond_if_in_address_set);
	p = fapi_append_data_u16(req, use_srf);
	p = fapi_append_data_u16(req, ssi_required_for_match);
	if (p)
		return 0;
	return 1;
}

static u32 slsi_mlme_nan_append_address_set(struct sk_buff *req, u16 count, u8 *addresses)
{
	if (!count)
		return 0;
	return slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_INTERFACE_ADDRESS_SET, count * 6, addresses);
}

static u32 slsi_mlme_nan_append_service_name(struct sk_buff *req, u16 len, u8 *data)
{
	if (!len)
		return 0;
	return slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_SERVICE_NAME, len, data);
}

static u32 slsi_mlme_nan_append_service_specific_info(struct sk_buff *req, u16 len, u8 *data)
{
	if (!len)
		return 0;
	return slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_SERVICE_SPECIFIC_INFO, len, data);
}

static u32 slsi_mlme_nan_append_ext_service_specific_info(struct sk_buff *req, u16 len, u8 *data)
{
	if (!len)
		return 0;
	return slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_EXT_SERVICE_SPECIFIC_INFO, len, data);
}

static u32 slsi_mlme_nan_append_tx_match_filter(struct sk_buff *req, u16 len, u8 *data)
{
	if (!len)
		return 0;
	return slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_TX_MATCH_FILTER, len, data);
}

static u32 slsi_mlme_nan_append_rx_match_filter(struct sk_buff *req, u16 len, u8 *data)
{
	if (!len)
		return 0;
	return slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_RX_MATCH_FILTER, len, data);
}

static u32 slsi_mlme_nan_append_data_path_sec(struct sk_buff *req, struct slsi_nan_security_info *sec_info)
{
	u8 *p, *len_p;
	u8 *pmk;
	u8 passphrase_len = 0;
	u8 *passphrase;
	u8 sec_type = sec_info->key_info.key_type;
	u8 pmk_len = 0;

	if (sec_info->key_info.key_type == 1) {
		pmk_len = sec_info->key_info.body.pmk_info.pmk_len;
		pmk = sec_info->key_info.body.pmk_info.pmk;
	} else if (sec_info->key_info.key_type == 2) {
		passphrase_len = sec_info->key_info.body.passphrase_info.passphrase_len;
		passphrase = sec_info->key_info.body.passphrase_info.passphrase;
	} else {
		sec_type = 0;
	}

	p = fapi_append_data_u16(req, SLSI_NAN_TLV_TAG_DATA_PATH_SECURITY);
	p = fapi_append_data_u16(req, 0x0002);
	len_p = p;
	p = fapi_append_data_u8(req, sec_type);
	p = fapi_append_data_u8(req, sec_info->cipher_type);
	if (sec_info->key_info.key_type == 1) {
		if (pmk_len)
			p = fapi_append_data(req, pmk, pmk_len);
		if (p && len_p)
			*len_p = 2 + pmk_len;
	} else if (sec_info->key_info.key_type == 2) {
		if (passphrase_len)
			p = fapi_append_data(req, passphrase, passphrase_len);
		if (p && len_p)
			*len_p = 2 + passphrase_len;
	}
	if (!p)
		return 1;
	return 0;
}

static u32 slsi_mlme_nan_append_ranging(struct sk_buff *req, struct slsi_nan_ranging_cfg *ranging_cfg)
{
	u8 *p;

	p = fapi_append_data_u16(req, SLSI_NAN_TLV_TAG_RANGING);
	p = fapi_append_data_u16(req, 0x0009);
	p = fapi_append_data_u32(req, ranging_cfg->ranging_interval_msec);
	p = fapi_append_data_u8(req, ranging_cfg->config_ranging_indications);
	p = fapi_append_data_u16(req, ranging_cfg->distance_ingress_mm);
	p = fapi_append_data_u16(req, ranging_cfg->distance_egress_mm);
	if (p)
		return 0;
	return 1;
}

static int slsi_mlme_nan_append_ipv6_link_tlv(struct sk_buff *req, u8 *local_ndi)
{
	u8 *p;
	u8 interface_identifier[8];

	memcpy(&interface_identifier[0], &local_ndi[0], 3);
	interface_identifier[3] = 0xFF;
	interface_identifier[4] = 0xFE;
	memcpy(&interface_identifier[5], &local_ndi[3], 3);

	p = fapi_append_data_u16(req, SLSI_NAN_TLV_WFA_IPV6_LOCAL_LINK);
	p = fapi_append_data_u16(req, 0x0008);
	p = fapi_append_data(req, interface_identifier, 8);

	if (p)
		return 0;
	return 1;
}

static u32 slsi_mlme_nan_enable_fapi_data(struct netdev_vif *ndev_vif, struct sk_buff *req, struct slsi_hal_nan_enable_req *hal_req,
					  u8 cluster_merge)
{
	u16 publish_id_inc, service_id_inc;
	u8  publish_id_inc_count = 0;
	u8  service_id_inc_count = 0;
	u8  rssi_close, rssi_middle, rssi_proximity;
	u16 rssi_window = hal_req->config_rssi_window_size ? hal_req->rssi_window_size_val : 8;
	u32 ret;

	/* NAN configuration TLV */
	publish_id_inc = hal_req->config_sid_beacon && (hal_req->sid_beacon_val & 0x01);
	if (publish_id_inc)
		publish_id_inc_count = hal_req->sid_beacon_val >> 1;
	service_id_inc = hal_req->config_subscribe_sid_beacon && (hal_req->subscribe_sid_beacon_val & 0x01);
	if (service_id_inc)
		service_id_inc_count = hal_req->subscribe_sid_beacon_val >> 1;
	ret = slsi_mlme_nan_append_config_tlv(req, hal_req->master_pref, publish_id_inc, publish_id_inc_count,
					      service_id_inc, service_id_inc_count, rssi_window,
					      hal_req->disc_mac_addr_rand_interval_sec, cluster_merge);
	if (ret) {
		SLSI_WARN_NODEV("Error append config TLV\n");
		return ret;
	}

	/* 2.4G NAN band specific config TLV*/
	rssi_close = hal_req->config_2dot4g_rssi_close ? hal_req->rssi_close_2dot4g_val : 0;
	rssi_middle = hal_req->config_2dot4g_rssi_middle ? hal_req->rssi_middle_2dot4g_val : 0;
	rssi_proximity = hal_req->config_2dot4g_rssi_proximity ? hal_req->rssi_proximity_2dot4g_val : 0;
	ret = slsi_mlme_nan_append_2g4_band_specific_config(req, rssi_close, rssi_middle, rssi_proximity,
							    hal_req->scan_params_val.dwell_time[0],
							    hal_req->scan_params_val.scan_period[0],
							    (u16)hal_req->config_2dot4g_dw_band,
							    hal_req->dw_2dot4g_interval_val);
	if (ret) {
		SLSI_WARN_NODEV("Error append 2.4G band specific TLV\n");
		return ret;
	}

	/* 5G NAN band specific config TLV*/
	if (hal_req->config_support_5g && hal_req->support_5g_val) {
		rssi_close = hal_req->config_5g_rssi_close ? hal_req->rssi_close_5g_val : 0;
		rssi_middle = hal_req->config_5g_rssi_middle ? hal_req->rssi_middle_5g_val : 0;
		rssi_proximity = hal_req->config_5g_rssi_close_proximity ? hal_req->rssi_close_proximity_5g_val : 0;
		ret = slsi_mlme_nan_append_5g_band_specific_config(req, rssi_close, rssi_middle, rssi_proximity,
								   hal_req->scan_params_val.dwell_time[1],
								   hal_req->scan_params_val.scan_period[1],
								   (u16)hal_req->config_5g_dw_band,
								   hal_req->dw_5g_interval_val);
		if (ret) {
			SLSI_WARN_NODEV("Error append 5G band specific TLV\n");
			return ret;
		}
	}

	ret = slsi_mlme_nan_append_config_supplemental(req, hal_req->discovery_beacon_interval_ms,
						       hal_req->nss_discovery, hal_req->enable_dw_early_termination,
						       hal_req->enable_ranging);
	if (ret) {
		SLSI_WARN_NODEV("Error append configuration supplemental TLV\n");
		return ret;
	}

	return ret;
}

void slsi_mlme_nan_store_config(struct netdev_vif *ndev_vif, struct slsi_hal_nan_enable_req *hal_req)
{
	ndev_vif->nan.config.config_rssi_proximity = hal_req->config_2dot4g_rssi_proximity;
	ndev_vif->nan.config.rssi_close_2dot4g_val = hal_req->rssi_close_2dot4g_val;
	ndev_vif->nan.config.rssi_middle_2dot4g_val = hal_req->rssi_middle_2dot4g_val;
	ndev_vif->nan.config.rssi_proximity = hal_req->rssi_proximity_2dot4g_val;
	ndev_vif->nan.config.scan_params_val.dwell_time[0] =  hal_req->scan_params_val.dwell_time[0];
	ndev_vif->nan.config.scan_params_val.scan_period[0] = hal_req->scan_params_val.scan_period[0];
	ndev_vif->nan.config.config_2dot4g_dw_band = (u16)hal_req->config_2dot4g_dw_band;
	ndev_vif->nan.config.dw_2dot4g_interval_val = hal_req->dw_2dot4g_interval_val;

	ndev_vif->nan.config.config_5g_rssi_close_proximity = hal_req->config_5g_rssi_close_proximity;
	ndev_vif->nan.config.rssi_close_5g_val = hal_req->rssi_close_5g_val;
	ndev_vif->nan.config.rssi_middle_5g_val = hal_req->rssi_middle_5g_val;
	ndev_vif->nan.config.rssi_close_proximity_5g_val = hal_req->rssi_close_proximity_5g_val;
	ndev_vif->nan.config.scan_params_val.dwell_time[1] =  hal_req->scan_params_val.dwell_time[1];
	ndev_vif->nan.config.scan_params_val.scan_period[1] = hal_req->scan_params_val.scan_period[1];
	ndev_vif->nan.config.config_5g_dw_band = (u16)hal_req->config_5g_dw_band;
	ndev_vif->nan.config.dw_5g_interval_val = hal_req->dw_5g_interval_val;
	ndev_vif->nan.state = 1;
}

int slsi_mlme_nan_enable(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_enable_req *hal_req)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_oper_ctrl = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");

	/* mbulk data length = 0x0f + 4 + 2 * (9 + 4) + 0x07 + 4 = 56*/
	req = fapi_alloc(mlme_nan_start_req, MLME_NAN_START_REQ, ndev_vif->ifnum, 56);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	fapi_set_u16(req, u.mlme_nan_start_req.nan_operation_control_flags, nan_oper_ctrl);
	fapi_set_u32(req, u.mlme_nan_start_req.spare_1, hal_req->transaction_id);

	r = slsi_mlme_nan_enable_fapi_data(ndev_vif, req, hal_req, !ndev_vif->nan.disable_cluster_merge);
	if (r) {
		SLSI_NET_ERR(dev, "Failed to construct mbulkdata\n");
		kfree_skb(req);
		return -EINVAL;
	}

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_START_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_start_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_START_CFM(result:0x%04x) ERROR\n",
			     fapi_get_u16(cfm, u.mlme_nan_start_cfm.result_code));
		r = -EINVAL;
	}
	if (!r)
		slsi_mlme_nan_store_config(ndev_vif, hal_req);
	kfree_skb(cfm);
	return r;
}

static u32 slsi_mlme_nan_publish_fapi_data(struct sk_buff *req, struct slsi_hal_nan_publish_req *hal_req)
{
	u32 ret;

	ret = slsi_mlme_nan_append_discovery_config(req, hal_req->publish_type, hal_req->tx_type, hal_req->ttl,
						    hal_req->period, hal_req->publish_count, hal_req->publish_match_indicator,
						    (u16)hal_req->rssi_threshold_flag, (u16)0, (u16)hal_req->sdea_params.config_nan_data_path);
	if (ret) {
		SLSI_WARN_NODEV("Error append disovery config TLV\n");
		return ret;
	}

	if (hal_req->service_name_len) {
		ret = slsi_mlme_nan_append_service_name(req, hal_req->service_name_len, hal_req->service_name);
		if (ret) {
			SLSI_WARN_NODEV("Error append servicename TLV\n");
			return ret;
		}
	}

	if (hal_req->service_specific_info_len) {
		ret = slsi_mlme_nan_append_service_specific_info(req, hal_req->service_specific_info_len,
								 hal_req->service_specific_info);
		if (ret) {
			SLSI_WARN_NODEV("Error append servSpecInfo TLV\n");
			return ret;
		}
	}

	if (hal_req->sdea_service_specific_info_len) {
		ret = slsi_mlme_nan_append_ext_service_specific_info(req, hal_req->sdea_service_specific_info_len,
								     hal_req->sdea_service_specific_info);
		if (ret) {
			SLSI_WARN_NODEV("Error append extServSpecInfo TLV\n");
			return ret;
		}
	}

	if (hal_req->rx_match_filter_len) {
		ret = slsi_mlme_nan_append_rx_match_filter(req, hal_req->rx_match_filter_len, hal_req->rx_match_filter);
		if (ret) {
			SLSI_WARN_NODEV("Error append rx match filter TLV\n");
			return ret;
		}
	}

	if (hal_req->tx_match_filter_len) {
		ret = slsi_mlme_nan_append_tx_match_filter(req, hal_req->tx_match_filter_len, hal_req->tx_match_filter);
		if (ret) {
			SLSI_WARN_NODEV("Error append tx match filter TLV\n");
			return ret;
		}
	}

	ret = slsi_mlme_nan_append_data_path_sec(req, &hal_req->sec_info);
	if (ret) {
		SLSI_WARN_NODEV("Error append datapath sec TLV\n");
		return ret;
	}
	ret = slsi_mlme_nan_append_ranging(req, &hal_req->ranging_cfg);
	if (ret)
		SLSI_WARN_NODEV("Error append ranging config TLV\n");
	return ret;
}

int slsi_mlme_nan_publish(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_publish_req *hal_req,
			  u16 publish_id)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_sdf_flags = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");
	if (hal_req) {
		/* discovery_config_tlv, datapath_sec_tlv, ranging_cfg_tlv */
		u16 length = 18 + 70 + 11;

		length += hal_req->service_name_len ? hal_req->service_name_len + 4 : 0;
		length += hal_req->service_specific_info_len ? hal_req->service_specific_info_len + 4 : 0;
		length += hal_req->rx_match_filter_len ? hal_req->rx_match_filter_len + 4 : 0;
		length += hal_req->tx_match_filter_len ? hal_req->tx_match_filter_len + 4 : 0;
		length += hal_req->sdea_service_specific_info_len ? hal_req->sdea_service_specific_info_len + 4 : 0;

		req = fapi_alloc(mlme_nan_publish_req, MLME_NAN_PUBLISH_REQ, ndev_vif->ifnum, length);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}

		/* Set/Enable corresponding bits to disable any indications
		 * that follow a publish.
		 * BIT0 - Disable publish termination indication.
		 * BIT1 - Disable match expired indication.
		 * BIT2 - Disable followUp indication received (OTA).
		 */
		if (hal_req->recv_indication_cfg & BIT(0))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_PUBLISH_END_EVENT;
		if (hal_req->recv_indication_cfg & BIT(1))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_MATCH_EXPIRED_EVENT;
		if (hal_req->recv_indication_cfg & BIT(2))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_RECEIVED_FOLLOWUP_EVENT;
		/* Store the SDF Flags */
		ndev_vif->nan.nan_sdf_flags[publish_id] = nan_sdf_flags;
	} else {
		req = fapi_alloc(mlme_nan_publish_req, MLME_NAN_PUBLISH_REQ, ndev_vif->ifnum, 0);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}
	}

	fapi_set_u16(req, u.mlme_nan_publish_req.session_id, publish_id);
	fapi_set_u16(req, u.mlme_nan_publish_req.nan_sdf_flags, 0);

	if (hal_req) {
		fapi_set_u32(req, u.mlme_nan_publish_req.spare_1, hal_req->transaction_id);
		r = slsi_mlme_nan_publish_fapi_data(req, hal_req);
		if (r) {
			SLSI_NET_ERR(dev, "Failed to construct mbulkdata\n");
			kfree_skb(req);
			return -EINVAL;
		}
	} else {
		fapi_set_u32(req, u.mlme_nan_publish_req.spare_1, 0);
	}

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_PUBLISH_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_publish_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_PUBLISH_CFM(result:0x%04x) ERROR\n",
			     fapi_get_u16(cfm, u.mlme_nan_publish_cfm.result_code));
		r = -EINVAL;
	}

	if (hal_req && !r)
		ndev_vif->nan.service_id_map |= (u32)BIT(publish_id);
	else
		ndev_vif->nan.service_id_map &= (u32)~BIT(publish_id);
	kfree_skb(cfm);
	return r;
}

static u32 slsi_mlme_nan_subscribe_fapi_data(struct sk_buff *req, struct slsi_hal_nan_subscribe_req *hal_req)
{
	u32 ret;

	ret = slsi_mlme_nan_append_subscribe_specific(req, hal_req->service_response_filter,
						      hal_req->service_response_include,
						      hal_req->use_service_response_filter,
						      hal_req->ssi_required_for_match_indication);
	if (ret) {
		SLSI_WARN_NODEV("Error append subscribe specific TLV\n");
		return ret;
	}

	ret = slsi_mlme_nan_append_discovery_config(req, hal_req->subscribe_type,
						    hal_req->subscribe_type ? 0 : 1, hal_req->ttl,
						    hal_req->period, hal_req->subscribe_count, hal_req->subscribe_match_indicator,
						    hal_req->rssi_threshold_flag, (u16)0, (u16)0);
	if (ret) {
		SLSI_WARN_NODEV("Error append discovery config TLV\n");
		return ret;
	}

	if (hal_req->service_name_len) {
		ret = slsi_mlme_nan_append_service_name(req, hal_req->service_name_len, hal_req->service_name);
		if (ret) {
			SLSI_WARN_NODEV("Error append service name TLV\n");
			return ret;
		}
	}

	if (hal_req->service_specific_info_len) {
		ret = slsi_mlme_nan_append_service_specific_info(req, hal_req->service_specific_info_len,
								 hal_req->service_specific_info);
		if (ret) {
			SLSI_WARN_NODEV("Error append servSpecInfo TLV\n");
			return ret;
		}
	}

	if (hal_req->rx_match_filter_len) {
		ret = slsi_mlme_nan_append_rx_match_filter(req, hal_req->rx_match_filter_len, hal_req->rx_match_filter);
		if (ret) {
			SLSI_WARN_NODEV("Error append rx match filter TLV\n");
			return ret;
		}
	}

	if (hal_req->tx_match_filter_len) {
		ret = slsi_mlme_nan_append_tx_match_filter(req, hal_req->tx_match_filter_len, hal_req->tx_match_filter);
		if (ret) {
			SLSI_WARN_NODEV("Error append tx match filter TLV\n");
			return ret;
		}
	}

	if (hal_req->sdea_service_specific_info_len) {
		ret = slsi_mlme_nan_append_ext_service_specific_info(req, hal_req->sdea_service_specific_info_len,
								     hal_req->sdea_service_specific_info);
		if (ret) {
			SLSI_WARN_NODEV("Error append extServSpecInfo TLV\n");
			return ret;
		}
	}

	ret = slsi_mlme_nan_append_data_path_sec(req, &hal_req->sec_info);
	if (ret) {
		SLSI_WARN_NODEV("Error append datapath sec TLV\n");
		return ret;
	}

	ret = slsi_mlme_nan_append_ranging(req, &hal_req->ranging_cfg);
	if (ret) {
		SLSI_WARN_NODEV("Error append ranging config TLV\n");
		return ret;
	}

	ret = slsi_mlme_nan_append_address_set(req, hal_req->num_intf_addr_present, (u8 *)hal_req->intf_addr);
	if (ret)
		SLSI_WARN_NODEV("Error append address set TLV\n");
	return ret;
}

int slsi_mlme_nan_subscribe(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_subscribe_req *hal_req,
			    u16 subscribe_id)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_sdf_flags = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");
	if (hal_req) {
		/* subscribespecific + discovery + data_path sec + ranging */
		u16 length = 11 + 18 + 70 + 11;

		length += hal_req->service_name_len ? hal_req->service_name_len + 4 : 0;
		length += hal_req->service_specific_info_len ? hal_req->service_specific_info_len + 4 : 0;
		length += hal_req->rx_match_filter_len ? hal_req->rx_match_filter_len + 4 : 0;
		length += hal_req->tx_match_filter_len ? hal_req->tx_match_filter_len + 4 : 0;
		length += hal_req->sdea_service_specific_info_len ? hal_req->sdea_service_specific_info_len + 4 : 0;
		length += hal_req->num_intf_addr_present ? hal_req->num_intf_addr_present * 6 + 4 : 0;

		req = fapi_alloc(mlme_nan_subscribe_req, MLME_NAN_SUBSCRIBE_REQ, ndev_vif->ifnum, length);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}
		/* Set/Enable corresponding bits to disable
		 * indications that follow a subscribe.
		 * BIT0 - Disable subscribe termination indication.
		 * BIT1 - Disable match expired indication.
		 * BIT2 - Disable followUp indication received (OTA).
		 */
		if (hal_req->recv_indication_cfg & BIT(0))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_SUBSCRIBE_END_EVENT;
		if (hal_req->recv_indication_cfg & BIT(1))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_MATCH_EXPIRED_EVENT;
		if (hal_req->recv_indication_cfg & BIT(2))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_RECEIVED_FOLLOWUP_EVENT;
		ndev_vif->nan.nan_sdf_flags[subscribe_id] = nan_sdf_flags;
	} else {
		req = fapi_alloc(mlme_nan_subscribe_req, MLME_NAN_SUBSCRIBE_REQ, ndev_vif->ifnum, 0);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}
	}

	fapi_set_u16(req, u.mlme_nan_subscribe_req.session_id, subscribe_id);
	fapi_set_u16(req, u.mlme_nan_subscribe_req.nan_sdf_flags, 0);
	if (hal_req) {
		fapi_set_u32(req, u.mlme_nan_subscribe_req.spare_1, hal_req->transaction_id);
		r = slsi_mlme_nan_subscribe_fapi_data(req, hal_req);
		if (r) {
			SLSI_NET_ERR(dev, "Failed to construct mbulkdata\n");
			kfree_skb(req);
			return -EINVAL;
		}
	} else {
		fapi_set_u32(req, u.mlme_nan_subscribe_req.spare_1, 0);
	}

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_SUBSCRIBE_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_subscribe_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_SUBSCRIBE_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_nan_subscribe_cfm.result_code));
		r = -EINVAL;
	}

	if (hal_req && !r)
		ndev_vif->nan.service_id_map |= (u32)BIT(subscribe_id);
	else
		ndev_vif->nan.service_id_map &= (u32)~BIT(subscribe_id);
	kfree_skb(cfm);
	return r;
}

static u32 slsi_mlme_nan_followup_fapi_data(struct sk_buff *req, struct slsi_hal_nan_transmit_followup_req *hal_req)
{
	u32 ret;

	ret = slsi_mlme_nan_append_service_specific_info(req, hal_req->service_specific_info_len,
							 hal_req->service_specific_info);
	if (ret) {
		SLSI_WARN_NODEV("Error append service specific info TLV\n");
		return ret;
	}

	ret = slsi_mlme_nan_append_ext_service_specific_info(req, hal_req->sdea_service_specific_info_len,
							     hal_req->sdea_service_specific_info);
	if (ret)
		SLSI_WARN_NODEV("Error append extServSpecInfo TLV\n");
	return ret;
}

int slsi_mlme_nan_tx_followup(struct slsi_dev *sdev, struct net_device *dev,
			      struct slsi_hal_nan_transmit_followup_req *hal_req)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_sdf_flags = 0;
	u16               data_len;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");

	if (slsi_nan_push_followup_ids(sdev, dev, hal_req->requestor_instance_id, hal_req->transaction_id) < 0) {
		SLSI_WARN(sdev, "Host followup_req queue full\n");
		return SLSI_HAL_NAN_STATUS_FOLLOWUP_QUEUE_FULL;
	}

	data_len = hal_req->service_specific_info_len ? (hal_req->service_specific_info_len + 4) : 0;
	data_len = hal_req->sdea_service_specific_info_len ? (hal_req->sdea_service_specific_info_len + 4) : 0;

	/* max possible length for publish attributes: 5*255 */
	req = fapi_alloc(mlme_nan_followup_req, MLME_NAN_FOLLOWUP_REQ, ndev_vif->ifnum, 5 * 255);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	fapi_set_u16(req, u.mlme_nan_followup_req.session_id, hal_req->publish_subscribe_id);
	fapi_set_u16(req, u.mlme_nan_followup_req.match_id, hal_req->requestor_instance_id);
	fapi_set_u16(req, u.mlme_nan_followup_req.nan_sdf_flags, nan_sdf_flags);
	fapi_set_u32(req, u.mlme_nan_followup_req.spare_1, hal_req->transaction_id);

	r = slsi_mlme_nan_followup_fapi_data(req, hal_req);
	if (r) {
		SLSI_NET_ERR(dev, "Failed to construct mbulkdata\n");
		kfree_skb(req);
		return -EINVAL;
	}

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_FOLLOWUP_CFM);
	if (!cfm) {
		slsi_nan_pop_followup_ids(sdev, dev, hal_req->requestor_instance_id);
		return -EIO;
	}

	if (fapi_get_u16(cfm, u.mlme_nan_followup_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		slsi_nan_pop_followup_ids(sdev, dev, hal_req->requestor_instance_id);
		SLSI_NET_ERR(dev, "MLME_NAN_FOLLOWUP_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_nan_followup_cfm.result_code));
		if (fapi_get_u16(cfm, u.mlme_nan_followup_cfm.result_code) ==
		    FAPI_RESULTCODE_TOO_MANY_SIMULTANEOUS_REQUESTS)
			r = SLSI_HAL_NAN_STATUS_FOLLOWUP_QUEUE_FULL;
		else
			r = -EINVAL;
	}

	kfree_skb(cfm);
	return r;
}

static u32 slsi_mlme_nan_config_fapi_data(struct netdev_vif *ndev_vif, struct sk_buff *req, struct slsi_hal_nan_config_req *hal_req, u8 cluster_merge)
{
	u16 rssi_window = hal_req->config_rssi_window_size ? hal_req->rssi_window_size_val : 8;
	u32 ret;
	u8  rssi_close = hal_req->rssi_close_2dot4g_val;
	u8  rssi_middle = hal_req->rssi_middle_2dot4g_val;
	u8  rssi_proximity = 0;

	u16 is_sid_in_beacon = hal_req->config_subscribe_sid_beacon && (hal_req->subscribe_sid_beacon_val & 0x01);
	u8  sid_count_in_beacon = hal_req->config_subscribe_sid_beacon ? hal_req->subscribe_sid_beacon_val >> 1 : 0;

	if (!hal_req->master_pref)
		hal_req->master_pref = ndev_vif->nan.master_pref_value;

	ret = slsi_mlme_nan_append_config_tlv(req, hal_req->master_pref,
					      hal_req->config_sid_beacon && (hal_req->sid_beacon & 0x01),
					      hal_req->config_sid_beacon ? hal_req->sid_beacon >> 1 : 0,
					      is_sid_in_beacon, sid_count_in_beacon, rssi_window,
					      hal_req->disc_mac_addr_rand_interval_sec, cluster_merge);
	if (ret) {
		SLSI_WARN_NODEV("Error append config TLV\n");
		return ret;
	}

	/* 2.4G NAN band specific config*/
	rssi_proximity = hal_req->config_rssi_proximity ? hal_req->rssi_proximity : 0;

	ret = slsi_mlme_nan_append_2g4_band_specific_config(req, rssi_close, rssi_middle, rssi_proximity,
							    hal_req->scan_params_val.dwell_time[0],
							    hal_req->scan_params_val.scan_period[0],
							    hal_req->config_2dot4g_dw_band,
							    hal_req->dw_2dot4g_interval_val);
	if (ret) {
		SLSI_WARN_NODEV("Error append 2.4G band specific TLV\n");
		return ret;
	}

	/* 5G NAN band specific config*/
	rssi_close = hal_req->rssi_close_5g_val;
	rssi_middle = hal_req->rssi_middle_5g_val;
	rssi_proximity = hal_req->config_5g_rssi_close_proximity ? hal_req->rssi_close_proximity_5g_val : 0;

	ret = slsi_mlme_nan_append_5g_band_specific_config(req, rssi_close, rssi_middle, rssi_proximity,
							   hal_req->scan_params_val.dwell_time[1],
							   hal_req->scan_params_val.scan_period[1],
							   hal_req->config_5g_dw_band,
							   hal_req->dw_5g_interval_val);
	if (ret) {
		SLSI_WARN_NODEV("Error append 5G band specific TLV\n");
		return ret;
	}

	ret = slsi_mlme_nan_append_config_supplemental(req, hal_req->discovery_beacon_interval_ms,
						       hal_req->nss_discovery, hal_req->enable_dw_early_termination,
						       hal_req->enable_ranging);
	if (ret) {
		SLSI_WARN_NODEV("Error append configuration supplemental TLV\n");
		return ret;
	}
	return ret;
}

int slsi_mlme_nan_set_config(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_config_req *hal_req)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_oper_ctrl = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");
	/* mbulk data length = 0x0f + 4 + 2 * (9 + 4) + 0x07 + 4 = 56 */
	req = fapi_alloc(mlme_nan_config_req, MLME_NAN_CONFIG_REQ, ndev_vif->ifnum, 56);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	fapi_set_u16(req, u.mlme_nan_config_req.nan_operation_control_flags, nan_oper_ctrl);
	fapi_set_u32(req, u.mlme_nan_config_req.spare_1, hal_req->transaction_id);

	r = slsi_mlme_nan_config_fapi_data(ndev_vif, req, hal_req, !ndev_vif->nan.disable_cluster_merge);
	if (r) {
		SLSI_NET_ERR(dev, "Failed to construct mbulkdata\n");
		kfree_skb(req);
		return -EINVAL;
	}

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_CONFIG_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_config_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_FOLLOWUP_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_nan_config_cfm.result_code));
		r = -EINVAL;
	}

	kfree_skb(cfm);
	return r;
}

static int slsi_mlme_ndp_request_fapi_data(struct sk_buff *req,
					   struct slsi_hal_nan_data_path_initiator_req *hal_req,
					   bool include_ipv6_addr_tlv, bool include_service_info_tlv,
					   u8 *local_ndi)
{
	int ret;

	ret = slsi_mlme_nan_append_data_path_sec(req, &hal_req->key_info);
	if (ret) {
		SLSI_WARN_NODEV("Error data_path sec TLV\n");
		return ret;
	}

	if (hal_req->app_info.ndp_app_info_len) {
		ret = slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_APP_INFO, hal_req->app_info.ndp_app_info_len,
					       hal_req->app_info.ndp_app_info);
		if (ret) {
			SLSI_WARN_NODEV("Error app info TLV\n");
			return ret;
		}
	}
	if (include_service_info_tlv) {
		ret = slsi_mlme_nan_append_service_info_tlv(req, &hal_req->app_info);
		if (ret)
			SLSI_WARN_NODEV("Error Adding Service Info TLV\n");
	}
	if (include_ipv6_addr_tlv) {
		ret = slsi_mlme_nan_append_ipv6_link_tlv(req, local_ndi);
		if (ret)
			SLSI_WARN_NODEV("Error ipv6 link tlv\n");
	}
	if (hal_req->service_name_len) {
		ret = slsi_mlme_nan_append_service_name(req, hal_req->service_name_len, hal_req->service_name);
		if (ret)
			SLSI_WARN_NODEV("Error append servicename TLV\n");
	}
	return ret;
}

int slsi_mlme_ndp_request(struct slsi_dev *sdev, struct net_device *dev,
			  struct slsi_hal_nan_data_path_initiator_req *hal_req, u32 ndp_instance_id, u16 ndl_vif_id)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0, data_len;
	bool              include_ipv6_link_tlv, include_service_info_tlv = 0;
	u8                *local_ndi;
	struct net_device *data_dev;
	struct netdev_vif *ndev_data_vif;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");

	data_dev = slsi_get_netdev_by_ifname(sdev, hal_req->ndp_iface);
	if (!data_dev) {
		SLSI_NET_ERR(dev, "no net_device for %s\n", hal_req->ndp_iface);
		return -EINVAL;
	}
	local_ndi = data_dev->dev_addr;
	ndev_data_vif = netdev_priv(data_dev);

	include_ipv6_link_tlv = slsi_dev_nan_is_ipv6_link_tlv_include();

	data_len = 74; /* for datapath security tlv */
	data_len += hal_req->app_info.ndp_app_info_len ? 4 + hal_req->app_info.ndp_app_info_len : 0;
	data_len += include_ipv6_link_tlv ? 4 + 8 : 0;
	data_len += hal_req->service_name_len ? hal_req->service_name_len + 4 : 0;
	if (hal_req->app_info.ndp_app_info_len) {
		u16 length;

		length = slsi_mlme_nan_service_info_tlv_length(&hal_req->app_info);
		data_len += (length + 1);
		include_service_info_tlv = 1;
	}
	req = fapi_alloc(mlme_ndp_request_req, MLME_NDP_REQUEST_REQ, ndev_vif->ifnum, data_len);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	fapi_set_u16(req, u.mlme_ndp_request_req.ndl_vif_index, ndl_vif_id);
	fapi_set_u16(req, u.mlme_ndp_request_req.match_id, hal_req->requestor_instance_id);
	fapi_set_memcpy(req, u.mlme_ndp_request_req.local_ndp_interface_address, local_ndi);
	fapi_set_memcpy(req, u.mlme_ndp_request_req.peer_nmi, hal_req->peer_disc_mac_addr);
	fapi_set_u16(req, u.mlme_ndp_request_req.ndp_instance_id, ndp_instance_id);
	fapi_set_u32(req, u.mlme_ndp_request_req.spare_1, hal_req->transaction_id);

	r = slsi_mlme_ndp_request_fapi_data(req, hal_req, include_ipv6_link_tlv, include_service_info_tlv, local_ndi);
	if (r) {
		SLSI_NET_ERR(dev, "Failed to construct mbulkdata\n");
		kfree_skb(req);
		return -EINVAL;
	}

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NDP_REQUEST_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_ndp_request_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NDP_REQUEST_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_ndp_request_cfm.result_code));
		r = -EINVAL;
	} else {
		SLSI_MUTEX_LOCK(ndev_data_vif->vif_mutex);
		ndev_data_vif->nan.ndp_count++;
		SLSI_MUTEX_UNLOCK(ndev_data_vif->vif_mutex);
		if (slsi_nan_ndp_new_entry(sdev, dev, ndp_instance_id, ndl_vif_id, local_ndi, hal_req->peer_disc_mac_addr))
			SLSI_NET_ERR(dev, "invalid ndl_vifid:%d ndp_id:%d\n", ndl_vif_id, ndp_instance_id);
	}

	kfree_skb(cfm);
	return r;
}

static int slsi_mlme_ndp_response_fapi_data(struct sk_buff *req,
					    struct slsi_hal_nan_data_path_indication_response *hal_req,
					    bool include_ipv6_link_tlv, bool include_service_info_tlv,
					    u8 *local_ndi)
{
	int ret;

	ret = slsi_mlme_nan_append_data_path_sec(req, &hal_req->key_info);
	if (ret) {
		SLSI_WARN_NODEV("Error data_path sec TLV\n");
		return ret;
	}

	if (hal_req->app_info.ndp_app_info_len) {
		ret = slsi_mlme_nan_append_tlv(req, SLSI_NAN_TLV_TAG_APP_INFO, hal_req->app_info.ndp_app_info_len,
					       hal_req->app_info.ndp_app_info);
		if (ret) {
			SLSI_WARN_NODEV("Error app info TLV\n");
			return ret;
		}
	}
	if (include_service_info_tlv) {
		ret = slsi_mlme_nan_append_service_info_tlv(req, &hal_req->app_info);
		if (ret)
			SLSI_WARN_NODEV("Error Adding Service Info TLV\n");
	}
	if (include_ipv6_link_tlv) {
		ret = slsi_mlme_nan_append_ipv6_link_tlv(req, local_ndi);
		if (ret)
			SLSI_WARN_NODEV("Error ipv6 link tlv\n");
	}
	if (hal_req->service_name_len) {
		ret = slsi_mlme_nan_append_service_name(req, hal_req->service_name_len, hal_req->service_name);
		if (ret)
			SLSI_WARN_NODEV("Error append servicename TLV\n");
	}
	return ret;
}

int slsi_mlme_ndp_response(struct slsi_dev *sdev, struct net_device *dev,
			   struct slsi_hal_nan_data_path_indication_response *hal_req, u16 local_ndp_instance_id)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0, data_len;
	bool              include_ipv6_link_tlv, include_service_info_tlv = 0;
	u8                *local_ndi;
	u16               ndl_vif_id, rsp_code;
	struct net_device *data_dev;
	u8                nomac[ETH_ALEN] = {0, 0, 0, 0, 0, 0};

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");
	data_dev = slsi_get_netdev_by_ifname(sdev, hal_req->ndp_iface);
	if (!data_dev)
		local_ndi = nomac;
	else
		local_ndi = data_dev->dev_addr;

	include_ipv6_link_tlv = slsi_dev_nan_is_ipv6_link_tlv_include();

	data_len = 74; /* for datapath security tlv */
	data_len += hal_req->app_info.ndp_app_info_len ? 4 + hal_req->app_info.ndp_app_info_len : 0;
	data_len += include_ipv6_link_tlv ? 4 + 8 : 0;
	data_len += hal_req->service_name_len ? 4 + hal_req->service_name_len : 0;
	if (hal_req->app_info.ndp_app_info_len) {
		u16 length;

		length = slsi_mlme_nan_service_info_tlv_length(&hal_req->app_info);
		data_len += (length + 1);
		include_service_info_tlv = 1;
	}

	req = fapi_alloc(mlme_ndp_response_req, MLME_NDP_RESPONSE_REQ, ndev_vif->ifnum, data_len);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}
	if (hal_req->ndp_instance_id)
		ndl_vif_id = ndev_vif->nan.ndp_instance_id2ndl_vif[hal_req->ndp_instance_id - 1];
	else
		ndl_vif_id = 0;
	fapi_set_u16(req, u.mlme_ndp_response_req.ndl_vif_index, ndl_vif_id);
	fapi_set_u16(req, u.mlme_ndp_response_req.request_id, local_ndp_instance_id);
	fapi_set_memcpy(req, u.mlme_ndp_response_req.local_ndp_interface_address, local_ndi);
	fapi_set_u16(req, u.mlme_ndp_response_req.ndp_instance_id, hal_req->ndp_instance_id);
	fapi_set_u32(req, u.mlme_ndp_response_req.spare_1, hal_req->transaction_id);

	rsp_code = hal_req->rsp_code == NAN_DP_REQUEST_ACCEPT ? FAPI_REASONCODE_NDP_ACCEPTED :
		   FAPI_REASONCODE_NDP_REJECTED;
	fapi_set_u16(req, u.mlme_ndp_response_req.reason_code, rsp_code);
	r = slsi_mlme_ndp_response_fapi_data(req, hal_req, include_ipv6_link_tlv, include_service_info_tlv, local_ndi);
	if (r) {
		SLSI_NET_ERR(dev, "Failed to construct mbulkdata\n");
		kfree_skb(req);
		return -EINVAL;
	}

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NDP_RESPONSE_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_ndp_response_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NDP_RESPONSE_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_ndp_request_cfm.result_code));
		r = -EINVAL;
		slsi_nan_ndp_del_entry(sdev, dev, hal_req->ndp_instance_id, false);
	} else {
		if (rsp_code == FAPI_REASONCODE_NDP_REJECTED)
			slsi_nan_ndp_del_entry(sdev, dev, hal_req->ndp_instance_id, false);
		/* new ndp entry was made when received mlme-ndp-requested.ind
		 * but local_ndi is decided now.
		 */
		if (hal_req->ndp_instance_id && rsp_code == FAPI_REASONCODE_NDP_ACCEPTED)
			ether_addr_copy(ndev_vif->nan.ndp_ndi[hal_req->ndp_instance_id - 1], local_ndi);
		if (data_dev) {
			struct netdev_vif *ndev_data_vif = netdev_priv(data_dev);

			ndev_data_vif = netdev_priv(data_dev);
			SLSI_MUTEX_LOCK(ndev_data_vif->vif_mutex);
			ndev_data_vif->nan.ndp_count++;
			SLSI_MUTEX_UNLOCK(ndev_data_vif->vif_mutex);
		}
	}

	kfree_skb(cfm);
	return r;
}

int slsi_mlme_ndp_terminate(struct slsi_dev *sdev, struct net_device *dev, u16 ndp_instance_id, u16 transaction_id)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;

	if (ndev_vif->nan.ndp_state[ndp_instance_id - 1] != ndp_slot_status_in_use) {
		slsi_nan_ndp_termination_handler(sdev, dev, ndp_instance_id, ndev_vif->nan.ndp_ndi[ndp_instance_id - 1]);
		return 0;
	}

	req = fapi_alloc(mlme_ndp_terminate_req, MLME_NDP_TERMINATE_REQ, ndev_vif->ifnum, 0);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	fapi_set_u16(req, u.mlme_ndp_terminate_req.ndp_instance_id, ndp_instance_id);
	fapi_set_u32(req, u.mlme_ndp_terminate_req.spare_1, transaction_id);
	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NDP_TERMINATE_CFM);
	if (!cfm) {
		slsi_nan_ndp_termination_handler(sdev, dev, ndp_instance_id, ndev_vif->nan.ndp_ndi[ndp_instance_id - 1]);
		return -EIO;
	}

	if (fapi_get_u16(cfm, u.mlme_ndp_terminate_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NDP_TERMINATE_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_ndp_terminate_cfm.result_code));
		slsi_nan_ndp_termination_handler(sdev, dev, ndp_instance_id, ndev_vif->nan.ndp_ndi[ndp_instance_id - 1]);
	}

	kfree_skb(cfm);
	return 0;
}

int slsi_mlme_nan_range_req(struct slsi_dev *sdev, struct net_device *dev, u8 count,
			    struct slsi_rtt_config *nl_rtt_params)
{
	struct sk_buff *req;
	struct sk_buff *rx;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int r = 0, i;

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
	req = fapi_alloc(mlme_nan_range_req, MLME_NAN_RANGE_REQ, 0, count * (SLSI_NAN_TLV_NAN_RTT_CONFIG_LEN + 4));
	if (!req) {
		SLSI_ERR(sdev, "failed to alloc %zd\n", count * (SLSI_NAN_TLV_NAN_RTT_CONFIG_LEN + 4));
		return -ENOMEM;
	}
	SLSI_DBG2(sdev, SLSI_MLME, "count:%d\n", count);
	/*fill the data */
	fapi_set_u16(req, u.mlme_nan_range_req.vif, ndev_vif->ifnum);
	for (i = 0; i < count; i++) {
		fapi_append_data_u16(req, SLSI_NAN_TLV_NAN_RTT_CONFIG);
		fapi_append_data_u16(req, SLSI_NAN_TLV_NAN_RTT_CONFIG_LEN);
		fapi_append_data(req, nl_rtt_params[i].peer_addr, ETH_ALEN);
		fapi_append_data_u8(req, nl_rtt_params[i].num_frames_per_burst);
		fapi_append_data_u8(req, nl_rtt_params[i].burst_duration);
		fapi_append_data_u8(req, nl_rtt_params[i].num_retries_per_ftmr);
	}
	rx = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_RANGE_CFM);
	if (!rx)
		return -EIO;
	if (fapi_get_u16(rx, u.mlme_nan_range_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_ERR(sdev, "mlme_nan_range_cfm(ERROR:0x%04x)",
			 fapi_get_u16(rx, u.mlme_nan_range_cfm.result_code));
		r = -EINVAL;
	}

	kfree_skb(rx);
	return r;
}

static bool slsi_nan_range_cancel_cfm_validate(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *cfm)
{
	int  result = fapi_get_u16(cfm, u.mlme_nan_range_cancel_cfm.result_code);
	bool r = false;

	SLSI_UNUSED_PARAMETER(sdev);

	if (WARN_ON(!dev))
		goto exit;

	if (result == FAPI_RESULTCODE_SUCCESS)
		r = true;
	else
		SLSI_NET_ERR(dev, "mlme_nan_range_cancel_cfm(result:0x%04x) ERROR\n", result);

exit:
	kfree_skb(cfm);
	return r;
}

int slsi_mlme_nan_range_cancel_req(struct slsi_dev *sdev, struct net_device *dev)
{
	struct sk_buff *req;
	struct sk_buff *rx;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int            r = 0;

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
	/* Alloc data size */
	req = fapi_alloc(mlme_nan_range_cancel_req, MLME_NAN_RANGE_CANCEL_REQ, 0, 0);
	if (!req) {
		SLSI_ERR(sdev, "failed to alloc cancel req\n");
		return -ENOMEM;
	}
	/*fill the data */
	fapi_set_u16(req, u.mlme_nan_range_cancel_req.vif, ndev_vif->ifnum);

	rx = slsi_mlme_req_cfm_ind(sdev, dev, req, MLME_NAN_RANGE_CANCEL_CFM, MLME_NAN_RANGE_IND, slsi_nan_range_cancel_cfm_validate);
	if (!rx) {
		SLSI_NET_ERR(dev, "mlme_nan_range_cancel_cfm() ERROR\n");
		r = -EINVAL;
	}
	kfree_skb(rx);
	return r;
}

