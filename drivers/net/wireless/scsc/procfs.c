/*****************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "dev.h"
#include "procfs.h"
#include "debug.h"
#include "mlme.h"
#include "mgt.h"
#include "mib.h"
#include "cac.h"
#include "hip.h"
#include "netif.h"
#include "ioctl.h"
#include "nl80211_vendor.h"

#include "mib.h"

int slsi_procfs_open_file_generic(struct inode *inode, struct file *file)
{
	file->private_data = SLSI_PDE_DATA(inode);
	return 0;
}

#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
static int slsi_printf_mutex_stats(char *buf, const size_t bufsz, const char *printf_padding, struct slsi_mutex *mutex_p)
{
	int        pos = 0;
	const char *filename;
	bool       is_locked;

	if (mutex_p->valid) {
		is_locked = SLSI_MUTEX_IS_LOCKED(*mutex_p);
		pos += scnprintf(buf, bufsz, "INFO: lock:%d\n", is_locked);
		if (is_locked) {
			filename = strrchr(mutex_p->file_name_before, '/');
			if (filename)
				filename++;
			else
				filename = mutex_p->file_name_before;
			pos += scnprintf(buf + pos, bufsz - pos, "\t%sTryingToAcquire:%s:%d\n", printf_padding,
					 filename, mutex_p->line_no_before);
			filename = strrchr(mutex_p->file_name_after, '/');
			if (filename)
				filename++;
			else
				filename = mutex_p->file_name_after;
			pos += scnprintf(buf + pos, bufsz - pos, "\t%sAcquired:%s:%d:%s\n", printf_padding,
					 filename, mutex_p->line_no_after, mutex_p->function);
			pos += scnprintf(buf + pos, bufsz - pos, "\t%sProcessName:%s\n", printf_padding, mutex_p->owner->comm);
		}
	} else {
		pos += scnprintf(buf, bufsz, "NoInit\n");
	}
	return pos;
}

static ssize_t slsi_procfs_mutex_stats_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	char              buf[76 + (200 * CONFIG_SCSC_WLAN_MAX_INTERFACES)];
	int               pos = 0;
	int               i;
	const size_t      bufsz = sizeof(buf);
	struct slsi_dev   *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;

	SLSI_UNUSED_PARAMETER(file);

	pos += scnprintf(buf, bufsz, "sdev\n");
	pos += scnprintf(buf + pos, bufsz - pos, "\tnetdev_add_remove_mutex ");
	pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t", &sdev->netdev_add_remove_mutex);
	pos += scnprintf(buf + pos, bufsz - pos, "\tstart_stop_mutex ");
	pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t", &sdev->start_stop_mutex);
	pos += scnprintf(buf + pos, bufsz - pos, "\tdevice_config_mutex ");
	pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t", &sdev->device_config_mutex);
	pos += scnprintf(buf + pos, bufsz - pos, "\tsig_wait.mutex ");
	pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t", &sdev->sig_wait.mutex);
#ifdef CONFIG_SCSC_WLAN_ENHANCED_LOGGING
	pos += scnprintf(buf + pos, bufsz - pos, "\tlogger_mutex ");
	pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t", &sdev->logger_mutex);
#endif

	for (i = 1; i < CONFIG_SCSC_WLAN_MAX_INTERFACES + 1; i++) {
		pos += scnprintf(buf + pos, bufsz - pos, "netdevvif %d\n", i);
		dev = slsi_get_netdev_locked(sdev, i);
		if (!dev)
			continue;
		ndev_vif = netdev_priv(dev);
		if (ndev_vif->is_available) {
			pos += scnprintf(buf + pos, bufsz - pos, "\tvif_mutex ");
			pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t\t", &ndev_vif->vif_mutex);
			pos += scnprintf(buf + pos, bufsz - pos, "\tsig_wait.mutex ");
			pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t\t", &ndev_vif->sig_wait.mutex);
			pos += scnprintf(buf + pos, bufsz - pos, "\tscan_mutex ");
			pos += slsi_printf_mutex_stats(buf + pos, bufsz - pos, "\t\t", &ndev_vif->scan_mutex);
		} else {
			pos += scnprintf(buf + pos, bufsz - pos, "\tvif UNAVAILABLE\n");
		}
	}
	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}
#endif

static ssize_t slsi_procfs_throughput_stats_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	char              buf[5 * 25];
	int               pos = 0;
	const size_t      bufsz = sizeof(buf);
	struct slsi_dev   *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = {{ SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 3, 0 } },
						 { SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 4, 0 } },
						 { SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 5, 0 } },
						 { SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 25, 0 } },
						 { SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 30, 0 } } };

	SLSI_UNUSED_PARAMETER(file);

	dev = slsi_get_netdev(sdev, 1);
	ndev_vif = netdev_priv(dev);

	if (ndev_vif->activated) {
		mibrsp.dataLength = 15 * ARRAY_SIZE(get_values);
		mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
		if (!mibrsp.data)
			SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		values = slsi_read_mibs(sdev, dev, get_values, ARRAY_SIZE(get_values), &mibrsp);
		if (!values) {
			kfree(mibrsp.data);
			return -EINVAL;
		}
		if (values[0].type != SLSI_MIB_TYPE_UINT)
			SLSI_ERR(sdev, "invalid type. iter:%d", 0); /*bad_fcs_count*/
		if (values[1].type != SLSI_MIB_TYPE_UINT)
			SLSI_ERR(sdev, "invalid type. iter:%d", 1); /*missed_ba_count*/
		if (values[2].type != SLSI_MIB_TYPE_UINT)
			SLSI_ERR(sdev, "invalid type. iter:%d", 2); /*missed_ack_count*/
		if (values[3].type != SLSI_MIB_TYPE_UINT)
			SLSI_ERR(sdev, "invalid type. iter:%d", 3); /*mac_bad_sig_count*/
		if (values[4].type != SLSI_MIB_TYPE_UINT)
			SLSI_ERR(sdev, "invalid type. iter:%d", 4); /*rx_error_count*/

		pos += scnprintf(buf, bufsz, "RX FCS:             %d\n", values[0].u.uintValue);
		pos += scnprintf(buf + pos, bufsz - pos, "RX bad SIG:         %d\n", values[3].u.uintValue);
		pos += scnprintf(buf + pos, bufsz - pos, "RX dot11 error:     %d\n", values[4].u.uintValue);
		pos += scnprintf(buf + pos, bufsz - pos, "TX MPDU no ACK:     %d\n", values[2].u.uintValue);
		pos += scnprintf(buf + pos, bufsz - pos, "TX A-MPDU no ACK:   %d\n", values[1].u.uintValue);

		kfree(values);
		kfree(mibrsp.data);
	} else {
		pos += scnprintf(buf, bufsz, "RX FCS:             %d\n", 0);
		pos += scnprintf(buf + pos, bufsz - pos, "RX bad SIG:         %d\n", 0);
		pos += scnprintf(buf + pos, bufsz - pos, "RX dot11 error:     %d\n", 0);
		pos += scnprintf(buf + pos, bufsz - pos, "TX MPDU no ACK:     %d\n", 0);
		pos += scnprintf(buf + pos, bufsz - pos, "TX A-MPDU no ACK:   %d\n", 0);
	}

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t slsi_procfs_sta_bss_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	char              buf[100];
	int               pos;
	const size_t      bufsz = sizeof(buf);
	struct slsi_dev   *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct cfg80211_bss  *sta_bss;
	int                  channel = 0, center_freq = 0;
	u8                   no_mac[] = {0, 0, 0, 0, 0, 0};
	u8                   *mac_ptr;
	u8                   ssid[33];
	s32                  signal = 0;

	SLSI_UNUSED_PARAMETER(file);

	mac_ptr = no_mac;
	ssid[0] = 0;

	dev = slsi_get_netdev(sdev, 1);
	if (!dev)
		goto exit;

	ndev_vif = netdev_priv(dev);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	sta_bss = ndev_vif->sta.sta_bss;
	if (sta_bss && ndev_vif->vif_type == FAPI_VIFTYPE_STATION &&
	    ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED) {
		const u8 *ssid_ie = cfg80211_find_ie(WLAN_EID_SSID, sta_bss->ies->data, sta_bss->ies->len);

		if (ssid_ie) {
			memcpy(ssid, &ssid_ie[2], ssid_ie[1]);
			ssid[ssid_ie[1]] = 0;
		}

		if (sta_bss->channel) {
			channel = sta_bss->channel->hw_value;
			center_freq = sta_bss->channel->center_freq;
		}
		mac_ptr = sta_bss->bssid;
		signal = sta_bss->signal;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);

exit:
	pos = scnprintf(buf, bufsz, "%pM,%s,%d,%d,%d", mac_ptr, ssid, channel, center_freq, signal);
	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t slsi_procfs_big_data_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	char              buf[100];
	int               pos;
	const size_t      bufsz = sizeof(buf);
	struct slsi_dev   *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev;

	SLSI_UNUSED_PARAMETER(file);
	dev = slsi_get_netdev(sdev, 1);
	if (!dev)
		goto exit;

	pos = slsi_get_sta_info(dev, buf, bufsz);
	if (pos >= 0)
		return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
exit:
	return 0;
}

static int slsi_procfs_status_show(struct seq_file *m, void *v)
{
	struct slsi_dev *sdev = (struct slsi_dev *)m->private;
	const char      *state;
	u32 conf_hip4_ver = 0;
	int i;

	SLSI_UNUSED_PARAMETER(v);

	switch (sdev->device_state) {
	case SLSI_DEVICE_STATE_ATTACHING:
		state = "Attaching";
		break;
	case SLSI_DEVICE_STATE_STOPPED:
		state = "Stopped";
		break;
	case SLSI_DEVICE_STATE_STARTING:
		state = "Starting";
		break;
	case SLSI_DEVICE_STATE_STARTED:
		state = "Started";
		break;
	case SLSI_DEVICE_STATE_STOPPING:
		state = "Stopping";
		break;
	default:
		state = "UNKNOWN";
		break;
	}

	seq_printf(m, "Driver FAPI Version: MA SAP    : %d.%d.%d\n", FAPI_MAJOR_VERSION(FAPI_DATA_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_DATA_SAP_VERSION), FAPI_DATA_SAP_ENG_VERSION);
	seq_printf(m, "Driver FAPI Version: MLME SAP  : %d.%d.%d\n", FAPI_MAJOR_VERSION(FAPI_CONTROL_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_CONTROL_SAP_VERSION), FAPI_CONTROL_SAP_ENG_VERSION);
	seq_printf(m, "Driver FAPI Version: DEBUG SAP : %d.%d.%d\n", FAPI_MAJOR_VERSION(FAPI_DEBUG_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_DEBUG_SAP_VERSION), FAPI_DEBUG_SAP_ENG_VERSION);
	seq_printf(m, "Driver FAPI Version: TEST SAP  : %d.%d.%d\n", FAPI_MAJOR_VERSION(FAPI_TEST_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_TEST_SAP_VERSION), FAPI_TEST_SAP_ENG_VERSION);

	if (atomic_read(&sdev->hip.hip_state) == SLSI_HIP_STATE_STARTED) {
		conf_hip4_ver = scsc_wifi_get_hip_config_version(&sdev->hip4_inst.hip_control->init);
		seq_printf(m, "HIP4 Version  : %d\n", conf_hip4_ver);
		if (conf_hip4_ver == 4) {
			seq_printf(m, "Chip FAPI Version (v4): MA SAP         : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_ma_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_ma_ver)));
			seq_printf(m, "Chip FAPI Version (v4): MLME SAP       : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_mlme_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_mlme_ver)));
			seq_printf(m, "Chip FAPI Version (v4): DEBUG SAP      : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_debug_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_debug_ver)));
			seq_printf(m, "Chip FAPI Version (v4): TEST SAP       : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_test_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_4_u16(&sdev->hip4_inst.hip_control->config_v4, sap_test_ver)));
		} else if (conf_hip4_ver == 5) {
			seq_printf(m, "Chip FAPI Version (v5): MA SAP         : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_ma_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_ma_ver)));
			seq_printf(m, "Chip FAPI Version (v5): MLME SAP       : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_mlme_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_mlme_ver)));
			seq_printf(m, "Chip FAPI Version (v5): DEBUG SAP      : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_debug_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_debug_ver)));
			seq_printf(m, "Chip FAPI Version (v5): TEST SAP       : %d.%d\n",
				   FAPI_MAJOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_test_ver)),
				   FAPI_MINOR_VERSION(scsc_wifi_get_hip_config_version_5_u16(&sdev->hip4_inst.hip_control->config_v5, sap_test_ver)));
		}
	}

#ifdef CONFIG_SCSC_WLAN_DEBUG
	seq_puts(m, "Driver Debug      : Enabled\n");
#else
	seq_puts(m, "Driver Debug      : Disabled\n");
#endif
	seq_printf(m, "Driver State      : %s\n", state);

	seq_printf(m, "HW Version     [MIB] : 0x%.4X (%u)\n", sdev->chip_info_mib.chip_version, sdev->chip_info_mib.chip_version);
	seq_printf(m, "Platform Build [MIB] : 0x%.4X (%u)\n", sdev->plat_info_mib.plat_build, sdev->plat_info_mib.plat_build);
	for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++) {
		seq_printf(m, "Hash         [MIB%2d] : 0x%.4X (%u)\n", i, sdev->mib[i].mib_hash, sdev->mib[i].mib_hash);
		seq_printf(m, "Platform:    [MIB%2d] : %s\n", i, sdev->mib[i].platform);
	}
	seq_printf(m, "Hash           [local_MIB] : 0x%.4X (%u)\n", sdev->local_mib.mib_hash, sdev->local_mib.mib_hash);
	seq_printf(m, "Platform:      [local_MIB] : %s\n", sdev->local_mib.platform);

	return 0;
}

static int slsi_procfs_build_show(struct seq_file *m, void *v)
{
	SLSI_UNUSED_PARAMETER(v);
	seq_printf(m, "FAPI_DATA_SAP_VERSION                                    : %d.%d.%d\n",
		   FAPI_MAJOR_VERSION(FAPI_DATA_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_DATA_SAP_VERSION),
		   FAPI_DATA_SAP_ENG_VERSION);
	seq_printf(m, "FAPI_CONTROL_SAP_VERSION                                    : %d.%d.%d\n",
		   FAPI_MAJOR_VERSION(FAPI_CONTROL_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_CONTROL_SAP_VERSION),
		   FAPI_CONTROL_SAP_ENG_VERSION);
	seq_printf(m, "FAPI_DEBUG_SAP_VERSION                                    : %d.%d.%d\n",
		   FAPI_MAJOR_VERSION(FAPI_DEBUG_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_DEBUG_SAP_VERSION),
		   FAPI_DEBUG_SAP_ENG_VERSION);
	seq_printf(m, "FAPI_TEST_SAP_VERSION                                    : %d.%d.%d\n",
		   FAPI_MAJOR_VERSION(FAPI_TEST_SAP_VERSION),
		   FAPI_MINOR_VERSION(FAPI_TEST_SAP_VERSION),
		   FAPI_TEST_SAP_ENG_VERSION);
	seq_printf(m, "CONFIG_SCSC_WLAN_MAX_INTERFACES                   : %d\n", CONFIG_SCSC_WLAN_MAX_INTERFACES);
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
	seq_puts(m, "CONFIG_SCSC_WLAN_RX_NAPI                          : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_RX_NAPI                          : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_RX_NAPI_GRO
	seq_puts(m, "CONFIG_SCSC_WLAN_RX_NAPI_GRO                      : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_RX_NAPI_GRO                      : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_ANDROID
	seq_puts(m, "CONFIG_SCSC_WLAN_ANDROID                          : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_ANDROID                          : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_GSCAN_ENABLE
	seq_puts(m, "CONFIG_SCSC_WLAN_GSCAN_ENABLE                     : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_GSCAN_ENABLE                     : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
	seq_puts(m, "CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD                 : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD                 : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_PRIORITISE_IMP_FRAMES
	seq_puts(m, "CONFIG_SCSC_WLAN_PRIORITISE_IMP_FRAMES            : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_PRIORITISE_IMP_FRAMES            : n\n");
#endif
	seq_puts(m, "-------------------------------------------------\n");
#ifdef CONFIG_SCSC_WLAN_DEBUG
	seq_puts(m, "CONFIG_SCSC_WLAN_DEBUG                            : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_DEBUG                            : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	seq_puts(m, "CONFIG_SCSC_WLAN_MUTEX_DEBUG                      : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_MUTEX_DEBUG                      : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_ENHANCED_LOGGING
	seq_puts(m, "CONFIG_SCSC_WLAN_ENHANCED_LOGGING                 : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_ENHANCED_LOGGING                 : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	seq_puts(m, "CONFIG_SCSC_WLAN_WIFI_SHARING                     : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_WIFI_SHARING                     : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_SINGLE_ANTENNA
	seq_puts(m, "CONFIG_SCSC_WLAN_SINGLE_ANTENNA                   : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_SINGLE_ANTENNA                   : n\n");
#endif
#ifdef CONFIG_SCSC_AP_INTERFACE_NAME
	seq_printf(m, "CONFIG_SCSC_AP_INTERFACE_NAME                   : %s\n", CONFIG_SCSC_AP_INTERFACE_NAME);
#endif
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	seq_puts(m, "CONFIG_SCSC_WIFI_NAN_ENABLE                       : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WIFI_NAN_ENABLE                       : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA
	seq_puts(m, "CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA            : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA            : n\n");
#endif
#if defined(CONFIG_SLSI_WLAN_STA_FWD_BEACON) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
	seq_puts(m, "CONFIG_SLSI_WLAN_STA_FWD_BEACON                   : y\n");
#else
	seq_puts(m, "CONFIG_SLSI_WLAN_STA_FWD_BEACON                   : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
	seq_puts(m, "CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT          : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT          : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_DYNAMIC_ITO
	seq_puts(m, "CONFIG_SCSC_WLAN_DYNAMIC_ITO                      : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_DYNAMIC_ITO                      : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
	seq_puts(m, "CONFIG_SCSC_WLAN_BSS_SELECTION                      : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_BSS_SELECTION                      : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
	seq_puts(m, "CONFIG_SCSC_WLAN_FAST_RECOVERY                      : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_FAST_RECOVERY                      : n\n");
#endif
#ifdef CONFIG_SCSC_WLAN_AP_AUTO_RECOVERY
	seq_puts(m, "CONFIG_SCSC_WLAN_AP_AUTO_RECOVERY                 : y\n");
#else
	seq_puts(m, "CONFIG_SCSC_WLAN_AP_AUTO_RECOVERY                 : n\n");
#endif


	return 0;
}

static const char *slsi_procfs_vif_type_to_str(u16 type)
{
	switch (type) {
	case FAPI_VIFTYPE_STATION:
		return "STATION";
	case FAPI_VIFTYPE_AP:
		return "AP";
	case FAPI_VIFTYPE_UNSYNCHRONISED:
		return "UNSYNCH";
	default:
		return "?";
	}
}

static int slsi_procfs_vifs_show(struct seq_file *m, void *v)
{
	struct slsi_dev *sdev = (struct slsi_dev *)m->private;
	u16             vif;
	u16             peer_index;

	SLSI_UNUSED_PARAMETER(v);

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	for (vif = 1; vif <= CONFIG_SCSC_WLAN_MAX_INTERFACES; vif++) {
		struct net_device *dev = slsi_get_netdev_locked(sdev, vif);
		struct netdev_vif *ndev_vif;

		if (!dev)
			continue;

		ndev_vif = netdev_priv(dev);
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

		if (!ndev_vif->activated) {
			SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
			continue;
		}
		seq_printf(m, "vif:%d %pM %s\n", vif, dev->dev_addr, slsi_procfs_vif_type_to_str(ndev_vif->vif_type));
		for (peer_index = 0; peer_index < SLSI_ADHOC_PEER_CONNECTIONS_MAX; peer_index++) {
			struct slsi_peer *peer = ndev_vif->peer_sta_record[peer_index];

			if (peer && peer->valid)
				seq_printf(m, "vif:%d %pM peer[%d] %pM \n", vif, dev->dev_addr, peer_index, peer->address);
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	}
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	return 0;
}

static ssize_t slsi_procfs_read_int(struct file *file, char __user *user_buf, size_t count, loff_t *ppos, int value, const char *extra)
{
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	SLSI_UNUSED_PARAMETER(file);

	pos += scnprintf(buf + pos, bufsz - pos, "%d\n", value);
	if (extra)
		pos += scnprintf(buf + pos, bufsz - pos, "%s", extra);
	SLSI_INFO((struct slsi_dev *)file->private_data, "%s", buf);
	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t slsi_procfs_uapsd_write(struct file *file,
				       const char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	struct slsi_dev   *sdev           = file->private_data;
	struct net_device *dev          = NULL;
	int               qos_info      = 0;
	int               offset        = 0;
	char              *read_string;

	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);

	if (!dev) {
		SLSI_ERR(sdev, "Dev not found\n");
		return -EINVAL;
	}

	if (!count)
		return -EINVAL;

	read_string = kmalloc(count + 1, GFP_KERNEL);
	if (!read_string) {
		SLSI_ERR(sdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}
	memset(read_string, 0, (count + 1));

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	offset = slsi_str_to_int(read_string, &qos_info);
	if (!offset) {
		SLSI_ERR(sdev, "qos info : failed to read a numeric value");
		kfree(read_string);
		return -EINVAL;
	}

	/*Store the qos info and use it to set MIB during connection*/
	sdev->device_config.qos_info = qos_info;
	SLSI_DBG1(sdev, SLSI_MLME, "set qos_info:%d\n", sdev->device_config.qos_info);

	kfree(read_string);
	return count;
}

static ssize_t slsi_procfs_ap_cert_disable_ht_vht_write(struct file *file, const char __user *user_buf,
							size_t count, loff_t *ppos)
{
	struct slsi_dev *sdev = file->private_data;
	int offset = 0;
	int width = 0;
	char *read_string;

	if (!count)
		return -EINVAL;

	read_string = kmalloc(count + 1, GFP_KERNEL);
	if (!read_string) {
		SLSI_ERR(sdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}
	memset(read_string, 0, (count + 1));

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	offset = slsi_str_to_int(read_string, &width);
	if (!offset) {
		SLSI_ERR(sdev, "Failed to read a numeric value");
		kfree(read_string);
		return -EINVAL;
	}

	/* Disable default upgrade of corresponding width during AP start */
	if (width == 80)
		sdev->allow_switch_80_mhz = false;
	else if (width == 40)
		sdev->allow_switch_40_mhz = false;

	kfree(read_string);
	return count;
}

static ssize_t slsi_procfs_p2p_certif_write(struct file *file,
					    const char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct slsi_dev   *sdev           = file->private_data;
	char              *read_string;
	int               cert_info      = 0;
	int               offset        = 0;

	read_string = kmalloc(count + 1, GFP_KERNEL);
	if (!read_string) {
		SLSI_ERR(sdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}
	memset(read_string, 0, (count + 1));

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	offset = slsi_str_to_int(read_string, &cert_info);
	if (!offset) {
		SLSI_ERR(sdev, "qos info : failed to read a numeric value");
		kfree(read_string);
		return -EINVAL;
	}
	sdev->p2p_certif = cert_info;
	kfree(read_string);
	return count;
}

static ssize_t slsi_procfs_p2p_certif_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev   *sdev           = file->private_data;
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	SLSI_UNUSED_PARAMETER(file);

	pos += scnprintf(buf + pos, bufsz - pos, "%d\n", sdev->p2p_certif);

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static int slsi_procfs_mac_addr_show(struct seq_file *m, void *v)
{
	struct slsi_dev *sdev = (struct slsi_dev *)m->private;

	SLSI_UNUSED_PARAMETER(v);

	seq_printf(m, "%pM", sdev->hw_addr);
	return 0;
}

static ssize_t slsi_procfs_create_tspec_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev   *sdev = file->private_data;
	static const char *extra_info = "";

	return slsi_procfs_read_int(file, user_buf, count, ppos, sdev->current_tspec_id, extra_info);
}

static ssize_t slsi_procfs_create_tspec_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev *sfdev = (struct slsi_dev *)file->private_data;
	char            *read_string = kmalloc(count + 1, GFP_KERNEL);

	if (!read_string) {
		SLSI_ERR(sfdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}

	if (!count) {
		kfree(read_string);
		return 0;
	}

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	sfdev->current_tspec_id = cac_ctrl_create_tspec(sfdev, read_string);
	if (sfdev->current_tspec_id < 0) {
		SLSI_ERR(sfdev, "create tspec: No parameters or not valid parameters\n");
		kfree(read_string);
		return -EINVAL;
	}
	kfree(read_string);

	return count;
}

static ssize_t slsi_procfs_confg_tspec_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	static const char *extra_info = "Not implemented yet";
	int               value = 10;

	return slsi_procfs_read_int(file, user_buf, count, ppos, value, extra_info);
}

static ssize_t slsi_procfs_confg_tspec_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev *sfdev = (struct slsi_dev *)file->private_data;
	char            *read_string = kmalloc(count + 1, GFP_KERNEL);

	if (!read_string) {
		SLSI_ERR(sfdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}

	if (!count) {
		kfree(read_string);
		return 0;
	}

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	/* to do: call to config_tspec() to configure a tspec field */
	if (cac_ctrl_config_tspec(sfdev, read_string) < 0) {
		SLSI_ERR(sfdev, "config tspec error\n");
		kfree(read_string);
		return -EINVAL;
	}

	kfree(read_string);

	return count;
}

static ssize_t slsi_procfs_send_addts_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev   *sdev = file->private_data;
	static const char *extra_info = "";

	return slsi_procfs_read_int(file, user_buf, count, ppos, sdev->tspec_error_code, extra_info);
}

static ssize_t slsi_procfs_send_addts_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev *sfdev = (struct slsi_dev *)file->private_data;
	char            *read_string = kmalloc(count + 1, GFP_KERNEL);

	if (!read_string) {
		SLSI_ERR(sfdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}

	sfdev->tspec_error_code = -1;
	if (!count) {
		kfree(read_string);
		return 0;
	}

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	/* to do: call to config_tspec() to configure a tspec field */
	if (cac_ctrl_send_addts(sfdev, read_string) < 0) {
		SLSI_ERR(sfdev, "send addts error\n");
		kfree(read_string);
		return -EINVAL;
	}
	kfree(read_string);
	return count;
}

static ssize_t slsi_procfs_send_delts_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev   *sdev = file->private_data;
	static const char *extra_info = "";

	return slsi_procfs_read_int(file, user_buf, count, ppos, sdev->tspec_error_code, extra_info);
}

static ssize_t slsi_procfs_send_delts_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev *sfdev = (struct slsi_dev *)file->private_data;
	char            *read_string = kmalloc(count + 1, GFP_KERNEL);

	if (!read_string) {
		SLSI_ERR(sfdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}

	sfdev->tspec_error_code = -1;

	if (!count) {
		kfree(read_string);
		return 0;
	}

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	/* to do: call to config_tspec() to configure a tspec field */
	if (cac_ctrl_send_delts(sfdev, read_string) < 0) {
		SLSI_ERR(sfdev, "send delts error\n");
		kfree(read_string);
		return -EINVAL;
	}
	kfree(read_string);
	return count;
}

static ssize_t slsi_procfs_del_tspec_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	static const char *extra_info = "Not implemented yet";
	int               value = 10;

	return slsi_procfs_read_int(file, user_buf, count, ppos, value, extra_info);
}

static ssize_t slsi_procfs_del_tspec_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev *sfdev = (struct slsi_dev *)file->private_data;
	char            *read_string = kmalloc(count + 1, GFP_KERNEL);

	if (!read_string) {
		SLSI_ERR(sfdev, "Malloc for read_string failed\n");
		return -ENOMEM;
	}

	if (!count) {
		kfree(read_string);
		return 0;
	}

	simple_write_to_buffer(read_string, count, ppos, user_buf, count);
	read_string[count] = '\0';

	/* to do: call to config_tspec() to configure a tspec field */
	if (cac_ctrl_delete_tspec(sfdev, read_string) < 0) {
		SLSI_ERR(sfdev, "config tspec error\n");
		kfree(read_string);
		return -EINVAL;
	}
	kfree(read_string);
	return count;
}

static ssize_t slsi_procfs_tput_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	struct slsi_dev *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	int i;
	char         buf[256];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
		dev = sdev->netdev[i];
		if (dev) {
			ndev_vif = netdev_priv(dev);
			pos += scnprintf(buf + pos, bufsz - pos, "%s:\t", dev->name);
			if (ndev_vif->throughput_tx_bps < 1000)
				pos += scnprintf(buf + pos, bufsz - pos, "TX:%u bps\t", ndev_vif->throughput_tx_bps);
			else if ((ndev_vif->throughput_tx_bps >= 1000) && (ndev_vif->throughput_tx_bps < (1000 * 1000)))
				pos += scnprintf(buf + pos, bufsz - pos, "TX:%u Kbps\t", (ndev_vif->throughput_tx_bps / 1000));
			else
				pos += scnprintf(buf + pos, bufsz - pos, "TX:%u Mbps\t", (ndev_vif->throughput_tx_bps / (1000 * 1000)));

			if (ndev_vif->throughput_rx_bps < 1000)
				pos += scnprintf(buf + pos, bufsz - pos, "RX:%u bps\n", ndev_vif->throughput_rx_bps);
			else if ((ndev_vif->throughput_rx_bps >= 1000) && (ndev_vif->throughput_rx_bps < (1000 * 1000)))
				pos += scnprintf(buf + pos, bufsz - pos, "RX:%u Kbps\n", (ndev_vif->throughput_rx_bps / 1000));
			else
				pos += scnprintf(buf + pos, bufsz - pos, "RX:%u Mbps\n", (ndev_vif->throughput_rx_bps / (1000 * 1000)));
		}
	}
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t slsi_procfs_tput_write(struct file *file, const char __user *user_buf, size_t len, loff_t *ppos)
{
	struct slsi_dev *sdev = (struct slsi_dev *)file->private_data;
	char			read_buf[2];

	if (!sdev) {
		SLSI_ERR(sdev, "invalid sdev\n");
		return -ENOMEM;
	}

	if (!len || (len > sizeof(read_buf))) {
		SLSI_ERR(sdev, "invalid len\n");
		return -EINVAL;
	}

	simple_write_to_buffer(read_buf, len, ppos, user_buf, len);

	switch (read_buf[0]) {
	case '1':
		if (!slsi_traffic_mon_is_running(sdev)) {
			SLSI_DBG1(sdev, SLSI_HIP, "start Traffic monitor\n");
			slsi_traffic_mon_client_register(sdev, sdev, 0, 0, 0, NULL);
		}
		break;
	case '0':
		SLSI_DBG1(sdev, SLSI_HIP, "stop Traffic monitor\n");
		slsi_traffic_mon_client_unregister(sdev, sdev);
		break;
	default:
		SLSI_DBG1(sdev, SLSI_HIP, "invalid value %c\n", read_buf[0]);
		return -EINVAL;
	}
	return len;
}

static atomic_t fd_opened_count;

void slsi_procfs_inc_node(void)
{
	atomic_inc(&fd_opened_count);
}

void slsi_procfs_dec_node(void)
{
	if (0 == atomic_read(&fd_opened_count)) {
		WARN_ON(1);
		return;
	}
	atomic_dec(&fd_opened_count);
}

static ssize_t slsi_procfs_fd_opened_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	SLSI_UNUSED_PARAMETER(file);

	pos += scnprintf(buf + pos, bufsz - pos, "%d\n", atomic_read(&fd_opened_count));

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static int slsi_procfs_fcq_show(struct seq_file *m, void *v)
{
	struct slsi_dev *sdev = (struct slsi_dev *)m->private;
	int             ac;
	s32             vif, i;

	SLSI_UNUSED_PARAMETER(v);

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	for (vif = 1; vif <= CONFIG_SCSC_WLAN_MAX_INTERFACES; vif++) {
		struct net_device *dev = slsi_get_netdev_locked(sdev, vif);
		struct netdev_vif *ndev_vif;

		if (!dev)
			continue;

		ndev_vif = netdev_priv(dev);
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

		/* Unicast */
		for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
			struct slsi_peer               *peer = ndev_vif->peer_sta_record[i];
			int                            smod = 0, scod = 0, qmod = 0, qcod = 0;
			struct scsc_wifi_fcq_q_stat    queue_stat;
			u32                            peer_ps_state_transitions = 0;
			enum scsc_wifi_fcq_8021x_state cp_state;

			if (!peer || !peer->valid)
				continue;

			if (scsc_wifi_fcq_stat_queueset(&peer->data_qs, &queue_stat, &smod, &scod, &cp_state, &peer_ps_state_transitions) != 0)
				continue;

			seq_printf(m, "Interface: %-12s (vif: %d, type: %s)\n#%d\t|peer: %pM, qs: %2d, smod: %u, scod: %u, net_q_stops:%u, net_q_resumes:%u, PS transitions: %u, Controlled port: %s\n",
				   netdev_name(dev),
				   vif,
				   "UNICAST",
				   i + 1,
				   peer->address,
				   peer->queueset,
				   smod,
				   scod,
				   queue_stat.netq_stops,
				   queue_stat.netq_resumes,
				   peer_ps_state_transitions,
				   cp_state == SCSC_WIFI_FCQ_8021x_STATE_BLOCKED ? "Blocked" : "Opened");

			seq_printf(m, "\t|%8s|%8s|%8s|%8s|%8s|%8s|%8s|\n",
				   "AC index", "qcod", "qmod",
				   "state", "stops", "resumes",
				   "stop_percent");

			for (ac = 0; ac < SLSI_NETIF_Q_PER_PEER; ac++) {
				if (scsc_wifi_fcq_stat_queue(&peer->data_qs.ac_q[ac].head,
							     &queue_stat,
							     &qmod, &qcod) == 0)
					seq_printf(m, "\t|%8d|%8u|%8u|%8u|%8u|%8u|%8u%%\n",
						   ac,
						   qcod,
						   qmod,
						   queue_stat.netq_state,
						   queue_stat.netq_stops,
						   queue_stat.netq_resumes,
						   queue_stat.netq_stop_percent);
				else
					break;
			}
		}

		/* Groupcast */
		if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
			int                            smod = 0, scod = 0, qmod = 0, qcod = 0;
			struct scsc_wifi_fcq_q_stat    queue_stat;
			u32                            peer_ps_state_transitions = 0;
			enum scsc_wifi_fcq_8021x_state cp_state;

			if (scsc_wifi_fcq_stat_queueset(&ndev_vif->ap.group_data_qs, &queue_stat, &smod, &scod, &cp_state, &peer_ps_state_transitions) != 0) {
				SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
				continue;
			}

			seq_printf(m, "|%-12s|%-6d|%-6s|\n%d). smod:%u, scod:%u, netq stops :%u, netq resumes :%u, PS transitions :%u Controlled port :%s\n",
				   netdev_name(dev),
				   vif,
				   "MCAST",
				   i + 1,
				   smod,
				   scod,
				   queue_stat.netq_stops,
				   queue_stat.netq_resumes,
				   peer_ps_state_transitions,
				   cp_state == SCSC_WIFI_FCQ_8021x_STATE_BLOCKED ? "Blocked" : "Opened");

			seq_printf(m, "    |%-12s|%4s|%8s|%8s|%8s|%8s|%10s|%8s|\n",
				   "netdev",
				   "AC index", "qcod", "qmod",
				   "nq_state", "nq_stop", "nq_resume",
				   "tq_state");

			for (ac = 0; ac < SLSI_NETIF_Q_PER_PEER; ac++) {
				if (scsc_wifi_fcq_stat_queue(&ndev_vif->ap.group_data_qs.ac_q[ac].head,
							     &queue_stat,
							     &qmod, &qcod) == 0)
					seq_printf(m, "    |%-12s|%4d|%8u|%8u|%8u|%8u\n",
						   netdev_name(dev),
						   ac,
						   qcod,
						   qmod,
						   queue_stat.netq_stops,
						   queue_stat.netq_resumes);
				else
					break;
			}
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	}

	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	return 0;
}

static int slsi_procfs_tcp_ack_suppression_show(struct seq_file *m, void *v)
{
	struct slsi_dev *sdev = (struct slsi_dev *)m->private;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	int i;

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
		dev = sdev->netdev[i];
		if (dev) {
			ndev_vif = netdev_priv(dev);

			seq_printf(m, "%s: tack_acks=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_acks);
			seq_printf(m, "%s: tack_suppressed=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_suppressed);
			seq_printf(m, "%s: tack_sent=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_sent);
			seq_printf(m, "%s: tack_max=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_max);
			seq_printf(m, "%s: tack_timeout=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_timeout);
			seq_printf(m, "%s: tack_aged=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_ktime);
			seq_printf(m, "%s: tack_dacks=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_dacks);
			seq_printf(m, "%s: tack_sacks=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_sacks);
			seq_printf(m, "%s: tack_delay_acks=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_delay_acks);
			seq_printf(m, "%s: tack_low_window=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_low_window);
			seq_printf(m, "%s: tack_ece=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_ece);
			seq_printf(m, "%s: tack_nocache=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_nocache);
			seq_printf(m, "%s: tack_norecord=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_norecord);
			seq_printf(m, "%s: tack_lastrecord=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_lastrecord);
			seq_printf(m, "%s: tack_searchrecord=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_searchrecord);
			seq_printf(m, "%s: tack_hasdata=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_hasdata);
			seq_printf(m, "%s: tack_psh=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_psh);
			seq_printf(m, "%s: tack_dropped=%u\n", dev->name, ndev_vif->tcp_ack_stats.tack_dropped);

			/* reset stats after it is read */
			memset(&ndev_vif->tcp_ack_stats, 0, sizeof(struct slsi_tcp_ack_stats));
		}
	}
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
	return 0;
}

static ssize_t slsi_procfs_nan_mac_addr_read(struct file *file,	char __user *user_buf, size_t count, loff_t *ppos)
{
	char              buf[20];
	char              nan_mac[ETH_ALEN];
	int               pos = 0;
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	struct slsi_dev  *sdev = (struct slsi_dev *)file->private_data;

	slsi_nan_get_mac(sdev, nan_mac);
#else

	SLSI_UNUSED_PARAMETER(file);
	memset(nan_mac, 0, ETH_ALEN);
#endif
	pos = scnprintf(buf, sizeof(buf), "%pM", nan_mac);
	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
static ssize_t slsi_procfs_nan_info_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	char              buf[300];
	int               pos = 0;
	const size_t      bufsz = sizeof(buf);
	struct slsi_dev   *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev = slsi_nan_get_netdev(sdev);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_vif_nan *nan_data;

	SLSI_UNUSED_PARAMETER(file);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	nan_data = &ndev_vif->nan;
	memset(buf, 0, sizeof(buf));

	pos += scnprintf(buf, bufsz, "NANMACADDRESS,");
	pos += scnprintf(buf + pos, bufsz - pos, "%pM", nan_data->local_nmi);
	pos += scnprintf(buf + pos, bufsz, ",CLUSTERID,");
	pos += scnprintf(buf + pos, bufsz - pos, "%pM", nan_data->cluster_id);
	pos += scnprintf(buf + pos, bufsz, ",OPERATINGCHANNEL,");
	if (nan_data->operating_channel[0])
		pos += scnprintf(buf + pos, bufsz - pos, "%d ", nan_data->operating_channel[0]);
	if (nan_data->operating_channel[1])
		pos += scnprintf(buf + pos, bufsz - pos, "%d", nan_data->operating_channel[1]);
	pos += scnprintf(buf + pos, bufsz, ",ROLE,");
	pos += scnprintf(buf + pos, bufsz - pos, "%d", nan_data->role);
	pos += scnprintf(buf + pos, bufsz, ",STATE,");
	pos += scnprintf(buf + pos, bufsz - pos, "%d", nan_data->state);
	pos += scnprintf(buf + pos, bufsz, ",MASTERPREFVAL,");
	pos += scnprintf(buf + pos, bufsz - pos, "%d", nan_data->master_pref_value);
	pos += scnprintf(buf + pos, bufsz, ",AMR,");
	pos += scnprintf(buf + pos, bufsz - pos, "%d", nan_data->amr);
	pos += scnprintf(buf + pos, bufsz, ",HOPCOUNT,");
	pos += scnprintf(buf + pos, bufsz - pos, "%d", nan_data->hopcount);
	pos += scnprintf(buf + pos, bufsz, ",NMIRANDOMINTERVAL,");
	pos += scnprintf(buf + pos, bufsz - pos, "%d", nan_data->random_mac_interval_sec);

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t slsi_procfs_nan_disable_cluster_merge_write(struct file *file, const char __user *user_buf, size_t len,
							   loff_t *ppos)
{
	struct slsi_dev *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev = slsi_nan_get_netdev(sdev);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	char read_string[3];
	int  val, ret;

	simple_write_to_buffer(read_string, sizeof(read_string), ppos, user_buf, sizeof(read_string) - 1);
	read_string[sizeof(read_string) - 1] = '\0';

	if (strtoint(read_string, &val)) {
		SLSI_ERR(sdev, "invalid input %s\n", read_string);
		ret = -EINVAL;
	} else {
		ndev_vif->nan.disable_cluster_merge = val ? 1 : 0;
		ret = sizeof(read_string) - 1;
	}

	return ret;
}

static ssize_t slsi_procfs_nan_discovery_data_read(struct file *file,  char __user *user_buf, size_t count, loff_t *ppos)
{
	char              buf[500];
	int               pos = 0;
	const size_t      bufsz = sizeof(buf);
	struct slsi_dev   *sdev = (struct slsi_dev *)file->private_data;
	struct net_device *dev = slsi_nan_get_netdev(sdev);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_vif_nan *nan_data;
	struct slsi_nan_discovery_info *traverse;

	SLSI_UNUSED_PARAMETER(file);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	nan_data = &ndev_vif->nan;
	memset(buf, 0, sizeof(buf));
	traverse = nan_data->disc_info;

	while (traverse) {
		pos += scnprintf(buf + pos, bufsz - pos, "%pM,", nan_data->disc_info->peer_addr);
		pos += scnprintf(buf + pos, bufsz - pos, "%d\n", nan_data->disc_info->match_id);
		traverse = traverse->next;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

#endif

static ssize_t slsi_procfs_dscp_mapping_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
#if (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
#define DSCP_MAP "0,8,40,56"
#else
#define DSCP_MAP "24,8,40,56"
#endif
	return simple_read_from_buffer(user_buf, count, ppos, DSCP_MAP, strlen(DSCP_MAP));
}

SLSI_PROCFS_SEQ_FILE_OPS(vifs);
SLSI_PROCFS_SEQ_FILE_OPS(mac_addr);
SLSI_PROCFS_WRITE_FILE_OPS(uapsd);
SLSI_PROCFS_WRITE_FILE_OPS(ap_cert_disable_ht_vht);
SLSI_PROCFS_RW_FILE_OPS(p2p_certif);
SLSI_PROCFS_RW_FILE_OPS(create_tspec);
SLSI_PROCFS_RW_FILE_OPS(confg_tspec);
SLSI_PROCFS_RW_FILE_OPS(send_addts);
SLSI_PROCFS_RW_FILE_OPS(send_delts);
SLSI_PROCFS_RW_FILE_OPS(del_tspec);
SLSI_PROCFS_RW_FILE_OPS(tput);
SLSI_PROCFS_READ_FILE_OPS(fd_opened);
SLSI_PROCFS_SEQ_FILE_OPS(build);
SLSI_PROCFS_SEQ_FILE_OPS(status);
SLSI_PROCFS_SEQ_FILE_OPS(fcq);
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
SLSI_PROCFS_READ_FILE_OPS(mutex_stats);
#endif
SLSI_PROCFS_READ_FILE_OPS(sta_bss);
SLSI_PROCFS_READ_FILE_OPS(big_data);
SLSI_PROCFS_READ_FILE_OPS(throughput_stats);
SLSI_PROCFS_SEQ_FILE_OPS(tcp_ack_suppression);
SLSI_PROCFS_READ_FILE_OPS(nan_mac_addr);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
SLSI_PROCFS_READ_FILE_OPS(nan_info);
SLSI_PROCFS_WRITE_FILE_OPS(nan_disable_cluster_merge);
SLSI_PROCFS_READ_FILE_OPS(nan_discovery_data);

#endif
SLSI_PROCFS_READ_FILE_OPS(dscp_mapping);

int slsi_create_proc_dir(struct slsi_dev *sdev)
{
	char                  dir[16];
	struct proc_dir_entry *parent;

	(void)snprintf(dir, sizeof(dir), "driver/unifi%d", sdev->procfs_instance);
	parent = proc_mkdir(dir, NULL);
	if (parent) {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0))
		parent->data = sdev;
#endif
		sdev->procfs_dir = parent;

		SLSI_PROCFS_SEQ_ADD_FILE(sdev, build, parent, S_IRUSR | S_IRGRP | S_IROTH);
		SLSI_PROCFS_SEQ_ADD_FILE(sdev, status, parent, S_IRUSR | S_IRGRP | S_IROTH);
		SLSI_PROCFS_SEQ_ADD_FILE(sdev, fcq, parent, S_IRUSR | S_IRGRP | S_IROTH);
		SLSI_PROCFS_SEQ_ADD_FILE(sdev, vifs, parent, S_IRUSR | S_IRGRP);
		SLSI_PROCFS_SEQ_ADD_FILE(sdev, mac_addr, parent, S_IRUSR | S_IRGRP | S_IROTH); /*Add S_IROTH permission so that android settings can access it*/
		SLSI_PROCFS_ADD_FILE(sdev, uapsd, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, ap_cert_disable_ht_vht, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, p2p_certif, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, create_tspec, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, confg_tspec, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, send_addts, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, send_delts, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, del_tspec, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, tput, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, fd_opened, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
		SLSI_PROCFS_ADD_FILE(sdev, mutex_stats, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#endif
		SLSI_PROCFS_ADD_FILE(sdev, sta_bss, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, big_data, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, throughput_stats, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_SEQ_ADD_FILE(sdev, tcp_ack_suppression, sdev->procfs_dir, S_IRUSR | S_IRGRP);
		SLSI_PROCFS_ADD_FILE(sdev, nan_mac_addr, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
		SLSI_PROCFS_ADD_FILE(sdev, nan_info, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, nan_disable_cluster_merge, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		SLSI_PROCFS_ADD_FILE(sdev, nan_discovery_data, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#endif
		SLSI_PROCFS_ADD_FILE(sdev, dscp_mapping, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		return 0;
	}

err:
	SLSI_DBG1(sdev, SLSI_HIP, "Failure in creation of proc directories\n");
	return -EINVAL;
}

void slsi_remove_proc_dir(struct slsi_dev *sdev)
{
	if (sdev->procfs_dir) {
		char dir[32];

		SLSI_PROCFS_REMOVE_FILE(build, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(status, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(vifs, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(mac_addr, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(fcq, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(uapsd, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(ap_cert_disable_ht_vht, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(p2p_certif, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(create_tspec, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(confg_tspec, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(send_addts, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(send_delts, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(del_tspec, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(tput, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(fd_opened, sdev->procfs_dir);
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
		SLSI_PROCFS_REMOVE_FILE(mutex_stats, sdev->procfs_dir);
#endif
		SLSI_PROCFS_REMOVE_FILE(sta_bss, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(big_data, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(throughput_stats, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(tcp_ack_suppression, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(nan_mac_addr, sdev->procfs_dir);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
		SLSI_PROCFS_REMOVE_FILE(nan_info, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(nan_disable_cluster_merge, sdev->procfs_dir);
		SLSI_PROCFS_REMOVE_FILE(nan_discovery_data, sdev->procfs_dir);
#endif
		SLSI_PROCFS_REMOVE_FILE(dscp_mapping, sdev->procfs_dir);
		(void)snprintf(dir, sizeof(dir), "driver/unifi%d", sdev->procfs_instance);
		remove_proc_entry(dir, NULL);
		sdev->procfs_dir = NULL;
	}
}
