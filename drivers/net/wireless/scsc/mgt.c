/******************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include <linux/delay.h>
#include <linux/firmware.h>
#include <scsc/kic/slsi_kic_lib.h>
#include <linux/fs.h>
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#include <linux/soc/samsung/exynos-soc.h>
#endif

#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
#include <linux/if_ether.h>
#include <linux/in.h>
#endif

#include <scsc/scsc_mx.h>
#include <scsc/scsc_release.h>
#include "mgt.h"
#include "ioctl.h"
#include "debug.h"
#include "mlme.h"
#include "netif.h"
#include "utils.h"
#include "udi.h"
#include "log_clients.h"
#ifdef SLSI_TEST_DEV
#include "unittest.h"
#endif
#include "hip.h"
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

#include "procfs.h"
#include "mib.h"
#include "unifiio.h"
#include "ba.h"
#include "scsc_wifi_fcq.h"
#include "cac.h"
#include "cfg80211_ops.h"
#include "nl80211_vendor.h"

#ifdef CONFIG_SCSC_WLBTD
#include "../../../misc/samsung/scsc/scsc_wlbtd.h"
#endif
#if defined(CONFIG_SCSC_WLAN_ENHANCED_BIGDATA) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)

#include "hanged_record.h"
#endif
#define CSR_WIFI_SME_MIB2_HOST_PSID_MASK    0x8000
#define SLSI_DEFAULT_HW_MAC_ADDR    "\x00\x00\x0F\x11\x22\x33"
#define MX_WLAN_FILE_PATH_LEN_MAX (128)
#define SLSI_MIB_REG_RULES_MAX (50)
#define SLSI_MIB_MAX_CLIENT (10)
#define SLSI_REG_PARAM_START_INDEX (1)

/*Requirement-Used for Fast Recovery */
#define HOST_REASONCODE_RECOVERY_REASON    111

#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
/* To do Autogen for this mib later */
#define SLSI_PSID_UNIFI_ARP_OUTSTANDING_MAX 0x0A1E
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

static char *mib_file_t = "wlan_t.hcf";
module_param(mib_file_t, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mib_file_t, "mib data filename");

static char *mib_file2_t = "wlan_t_sw.hcf";
module_param(mib_file2_t, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mib_file2_t, "mib data filename");

/* MAC address override. If set to FF's, then
 * the address is taken from config files or
 * default derived from HW ID.
 */
static char mac_addr_override[] = "ff:ff:ff:ff:ff:ff";
module_param_string(mac_addr, mac_addr_override, sizeof(mac_addr_override), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mac_addr_override, "WLAN MAC address override");

static int slsi_mib_open_file(struct slsi_dev *sdev, struct slsi_dev_mib_info *mib_info, const struct firmware **fw);
static int slsi_mib_close_file(struct slsi_dev *sdev, const struct firmware *e);
static int slsi_mib_download_file(struct slsi_dev *sdev, struct slsi_dev_mib_info *mib_info);
static int slsi_country_to_index(struct slsi_802_11d_reg_domain *domain_info, const char *alpha2);
static int slsi_mib_initial_get(struct slsi_dev *sdev);
static int slsi_hanged_event_count;
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
#define SLSI_MAX_CHAN_5G_BAND 25
#define SLSI_2G_CHANNEL_ONE 2412
#endif

/* MAC address override stored in /sys/wifi/mac_addr */
static ssize_t sysfs_show_macaddr(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf);
static ssize_t sysfs_store_macaddr(struct kobject *kobj, struct kobj_attribute *attr,
				   const char *buf, size_t count);

static struct kobject *wifi_kobj_ref;
static char sysfs_mac_override[] = "ff:ff:ff:ff:ff:ff";
static struct kobj_attribute mac_attr = __ATTR(mac_addr, 0660, sysfs_show_macaddr, sysfs_store_macaddr);

/* Retrieve mac address in sysfs global */
static ssize_t sysfs_show_macaddr(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  char *buf)
{
	return snprintf(buf, sizeof(sysfs_mac_override), "%s", sysfs_mac_override);
}

/* Update mac address in sysfs global */
static ssize_t sysfs_store_macaddr(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf,
				   size_t count)
{
	int r;

	SLSI_INFO_NODEV("Override WLAN MAC address %s\n", buf);

	/* size of macaddr string */
	r = sscanf(buf, "%17s", (char *)&sysfs_mac_override);
	return (r > 0) ? count : 0;
}

/* Register sysfs mac address override */
void slsi_create_sysfs_macaddr(void)
{
#ifndef SLSI_TEST_DEV
	int r;

	wifi_kobj_ref = mxman_wifi_kobject_ref_get();
	pr_info("wifi_kobj_ref: 0x%p\n", wifi_kobj_ref);

	if (wifi_kobj_ref) {
		/* Create sysfs file /sys/wifi/mac_addr */
		r = sysfs_create_file(wifi_kobj_ref, &mac_attr.attr);
		if (r) {
			/* Failed, so clean up dir */
			pr_err("Can't create /sys/wifi/mac_addr\n");
			return;
		}
	} else {
		pr_err("failed to create /sys/wifi/mac_addr\n");
	}
#endif
}

/* Unregister sysfs mac address override */
void slsi_destroy_sysfs_macaddr(void)
{
	if (!wifi_kobj_ref)
		return;

	/* Destroy /sys/wifi/mac_addr file */
	sysfs_remove_file(wifi_kobj_ref, &mac_attr.attr);

	/* Destroy /sys/wifi virtual dir */
	mxman_wifi_kobject_ref_put();
}

void slsi_purge_scan_results_locked(struct netdev_vif *ndev_vif, u16 scan_id)
{
	struct slsi_scan_result *scan_result;
	struct slsi_scan_result *prev = NULL;

	scan_result = ndev_vif->scan[scan_id].scan_results;
	while (scan_result) {
		kfree_skb(scan_result->beacon);
		kfree_skb(scan_result->probe_resp);
		prev = scan_result;
		scan_result = scan_result->next;
		kfree(prev);
	}
	ndev_vif->scan[scan_id].scan_results = NULL;
}

void slsi_purge_scan_results(struct netdev_vif *ndev_vif, u16 scan_id)
{
	SLSI_MUTEX_LOCK(ndev_vif->scan_result_mutex);
	slsi_purge_scan_results_locked(ndev_vif, scan_id);
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_result_mutex);
}

void slsi_purge_blacklist(struct netdev_vif *ndev_vif)
{
	struct list_head *blacklist_pos, *blacklist_q;

	kfree(ndev_vif->acl_data_supplicant);
	ndev_vif->acl_data_supplicant = NULL;

	kfree(ndev_vif->acl_data_hal);
	ndev_vif->acl_data_hal = NULL;

	list_for_each_safe(blacklist_pos, blacklist_q, &ndev_vif->acl_data_fw_list) {
		struct slsi_bssid_blacklist_info *blacklist_info = list_entry(blacklist_pos,
			struct slsi_bssid_blacklist_info, list);

		list_del(blacklist_pos);
		kfree(blacklist_info);
	}
	INIT_LIST_HEAD(&ndev_vif->acl_data_fw_list);
}

struct sk_buff *slsi_dequeue_cached_scan_result(struct slsi_scan *scan, int *count)
{
	struct sk_buff *skb = NULL;
	struct slsi_scan_result *scan_result = scan->scan_results;

	if (scan_result) {
		if (scan_result->beacon) {
			skb = scan_result->beacon;
			scan_result->beacon = NULL;
		} else if (scan_result->probe_resp) {
			skb = scan_result->probe_resp;
			scan_result->probe_resp = NULL;
		} else {
			SLSI_ERR_NODEV("Scan entry with no beacon /probe resp!!\n");
		}

		/*If beacon and probe response indicated above , remove the entry*/
		if (!scan_result->beacon  && !scan_result->probe_resp) {
			scan->scan_results = scan_result->next;
			kfree(scan_result);
			if (count)
				(*count)++;
		}
	}
	return skb;
}

void slsi_get_hw_mac_address(struct slsi_dev *sdev, u8 *addr)
{
#ifndef SLSI_TEST_DEV
	const struct firmware *e = NULL;
	int                   i;
	u32                   u[ETH_ALEN];
	char                  path_name[MX_WLAN_FILE_PATH_LEN_MAX];
	int                   r;
	bool		      valid = false;

	/* Module parameter override */
	r = sscanf(mac_addr_override, "%02X:%02X:%02X:%02X:%02X:%02X", &u[0], &u[1], &u[2], &u[3], &u[4], &u[5]);
	if (r != ETH_ALEN) {
		SLSI_ERR(sdev, "mac_addr modparam set, but format is incorrect (should be e.g. xx:xx:xx:xx:xx:xx)\n");
		goto mac_sysfs;
	}
	for (i = 0; i < ETH_ALEN; i++) {
		if (u[i] != 0xff)
			valid = true;
		addr[i] = u[i] & 0xff;
	}

	/* If the override is valid, use it */
	if (valid) {
		SLSI_INFO(sdev, "MAC address from modparam: " MACSTR "\n", MAC2STR(u));
		return;
	}

	/* Sysfs parameter override */
mac_sysfs:
	r = sscanf(sysfs_mac_override, "%02X:%02X:%02X:%02X:%02X:%02X", &u[0], &u[1], &u[2], &u[3], &u[4], &u[5]);
	if (r != ETH_ALEN) {
		SLSI_ERR(sdev, "mac_addr in sysfs set, but format is incorrect (should be e.g. xx:xx:xx:xx:xx:xx)\n");
		goto mac_file;
	}
	for (i = 0; i < ETH_ALEN; i++) {
		if (u[i] != 0xff)
			valid = true;
		addr[i] = u[i] & 0xff;
	}

	/* If the override is valid, use it */
	if (valid) {
		SLSI_INFO(sdev, "MAC address from sysfs: " MACSTR "\n", MAC2STR(u));
		return;
	}

	/* read mac.txt */
mac_file:
	if (sdev->maddr_file_name) {
		scnprintf(path_name, MX_WLAN_FILE_PATH_LEN_MAX, "wlan/%s", sdev->maddr_file_name);
		SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "MAC address file : %s\n", path_name);

		r = mx140_file_request_device_conf(sdev->maxwell_core, &e, path_name);
		if (r != 0)
			goto mac_efs;

		if (!e) {
			SLSI_ERR(sdev, "mx140_file_request_device_conf() returned succes, but firmware was null\n");
			goto mac_efs;
		}
		r = sscanf(e->data, "%02X:%02X:%02X:%02X:%02X:%02X", &u[0], &u[1], &u[2], &u[3], &u[4], &u[5]);
		mx140_file_release_conf(sdev->maxwell_core, e);
		if (r != ETH_ALEN) {
			SLSI_ERR(sdev, "%s exists, but format is incorrect (should be e.g. xx:xx:xx:xx:xx:xx)\n", path_name);
			goto mac_efs;
		}
		for (i = 0; i < ETH_ALEN; i++)
			addr[i] = u[i] & 0xff;
		SLSI_INFO(sdev, "MAC address loaded from %s: " MACSTR "\n", path_name, MAC2STR(u));
		return;
	}
mac_efs:
#ifdef CONFIG_SCSC_WLAN_MAC_ADDRESS_FILENAME
	r = mx140_request_file(sdev->maxwell_core, CONFIG_SCSC_WLAN_MAC_ADDRESS_FILENAME, &e);
	if (r != 0)
		goto mac_default;
	if (!e) {
		SLSI_ERR(sdev, "mx140_request_file() returned succes, but firmware was null\n");
		goto mac_default;
	}
	r = sscanf(e->data, "%02X:%02X:%02X:%02X:%02X:%02X", &u[0], &u[1], &u[2], &u[3], &u[4], &u[5]);
	if (r != ETH_ALEN) {
		SLSI_ERR(sdev, "%s exists, but format is incorrect (%d) [%20s] (should be e.g. xx:xx:xx:xx:xx:xx)\n",
			 CONFIG_SCSC_WLAN_MAC_ADDRESS_FILENAME, r, e->data);
		goto mac_default;
	}
	for (i = 0; i < ETH_ALEN; i++) {
		if (u[i] != 0xff)
			valid = true;
		addr[i] = u[i] & 0xff;
	}
#endif
	/* If MAC address seems valid, finished */
	if (valid) {
#ifdef CONFIG_SCSC_WLAN_MAC_ADDRESS_FILENAME
		SLSI_INFO(sdev, "MAC address loaded from %s: " MACSTR "\n",
			  CONFIG_SCSC_WLAN_MAC_ADDRESS_FILENAME, MAC2STR(u));
#endif
		/* MAC address read could hold invalid values, try to fix it to normal address */
		if (addr[0] & 0x01) {
			addr[0] = addr[0] & 0xfe;
			SLSI_INFO(sdev, "MAC address invalid, fixed address: " MACSTR "\n", MAC2STR(addr));
		}
		mx140_release_file(sdev->maxwell_core, e);
		return;
	}
mac_default:
	/* This is safe to call, even if the struct firmware handle is NULL */
	mx140_file_release_conf(sdev->maxwell_core, e);

	SLSI_ETHER_COPY(addr, SLSI_DEFAULT_HW_MAC_ADDR);
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
	/* Randomise MAC address from the soc uid */
	addr[3] = (exynos_soc_info.unique_id & 0xFF0000000000) >> 40;
	addr[4] = (exynos_soc_info.unique_id & 0x00FF00000000) >> 32;
	addr[5] = (exynos_soc_info.unique_id & 0x0000FF000000) >> 24;
#endif
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT,
		  "MAC addr file NOT found, using default MAC ADDR: %pM\n", addr);
#else
	/* We use FIXED Mac addresses with the unittest driver */
	struct slsi_test_dev *uftestdev = (struct slsi_test_dev *)sdev->maxwell_core;

	SLSI_ETHER_COPY(addr, uftestdev->hw_addr);
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "Test Device Address: %pM\n", addr);
#endif
}

static void write_wifi_version_info_file(struct slsi_dev *sdev)
{
#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION >= 9)
	char *filepath = "/data/vendor/conn/.wifiver.info";
#else
	char *filepath = "/data/misc/conn/.wifiver.info";
#endif
	char buf[256];
	char build_id_fw[128];
	char build_id_drv[64];

	/* For 5.4 kernel CONFIG_SCSC_WLBTD will be defined so filp_open will not be used */
#ifndef CONFIG_SCSC_WLBTD
	struct file *fp = NULL;

	fp = filp_open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (IS_ERR(fp)) {
		SLSI_WARN(sdev, "version file wasn't found\n");
		return;
	} else if (!fp) {
		SLSI_WARN(sdev, "%s doesn't exist.\n", filepath);
		return;
	}
#endif
#ifndef SLSI_TEST_DEV
	mxman_get_fw_version(build_id_fw, 128);
	mxman_get_driver_version(build_id_drv, 64);
#endif

	/* WARNING:
	 * Please do not change the format of the following string
	 * as it can have fatal consequences.
	 * The framework parser for the version may depend on this
	 * exact formatting.
	 *
	 * Also beware that SCSC_SEP_VERSION will not be defined in AOSP.
	 */
#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION >= 9)
	/* P-OS */
	snprintf(buf, sizeof(buf),
		 "%s\n"	/* drv_ver: already appended by mxman_get_driver_version() */
		 "f/w_ver: %s\n"
		 "hcf_ver_hw: %s\n"
		 "hcf_ver_sw: %s\n"
		 "regDom_ver: %d.%d\n",
		 build_id_drv,
		 build_id_fw,
		 sdev->mib[0].platform,
		 sdev->mib[1].platform,
		 ((sdev->reg_dom_version >> 8) & 0xFF), (sdev->reg_dom_version & 0xFF));
#else
	/* O-OS, or unknown */
	snprintf(buf, sizeof(buf),
		 "%s (f/w_ver: %s)\nregDom_ver: %d.%d\n",
		 build_id_drv,
		 build_id_fw,
		 ((sdev->reg_dom_version >> 8) & 0xFF), (sdev->reg_dom_version & 0xFF));
#endif

/* If SCSC_SEP_VERSION is not known, avoid writing the file, as it could go to the wrong
 * location.
 */
#ifdef SCSC_SEP_VERSION
#ifdef CONFIG_SCSC_WLBTD
	wlbtd_write_file(filepath, buf);
#else
	kernel_write(fp, buf, strlen(buf), 0);
	if (fp)
		filp_close(fp, NULL);
#endif

	SLSI_INFO(sdev, "Succeed to write firmware/host information to .wifiver.info\n");
#else
	SLSI_UNUSED_PARAMETER(filepath);
#endif
}

static void write_m_test_chip_version_file(struct slsi_dev *sdev)
{
#ifdef CONFIG_SCSC_WLBTD
	char *filepath = "/data/vendor/conn/.cid.info";
	char buf[256];

	snprintf(buf, sizeof(buf), "%s\n", SCSC_RELEASE_SOLUTION);

	wlbtd_write_file(filepath, buf);

	SLSI_WARN(sdev, "Wrote chip information to .cid.info\n");
#endif
}


int slsi_start_monitor_mode(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 device_address[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	ndev_vif->vif_type = FAPI_VIFTYPE_MONITOR;

	if (slsi_mlme_add_vif(sdev, dev, dev->dev_addr, device_address) != 0) {
		SLSI_NET_ERR(dev, "add VIF for Monitor mode failed\n");
		ndev_vif->vif_type = SLSI_VIFTYPE_UNSPECIFIED;
		return -EINVAL;
	}

	/* set the link type for the device; it depends on the format of
	 * packet the firmware is going to Pass to Host.
	 *
	 * If the firmware passes MA data in 802.11 frame format, then
	 * dev->type = ARPHRD_IEEE80211;
	 *
	 * If the firmware adds Radio TAP header to MA data,
	 * dev->type = ARPHRD_IEEE80211_RADIOTAP;
	 */
	dev->type = ARPHRD_IEEE80211_RADIOTAP;
	ndev_vif->activated = true;
	return 0;
}

void slsi_stop_monitor_mode(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
	SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "de-activate monitor VIF\n");
	if (slsi_mlme_del_vif(sdev, dev) != 0)
		SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");

	/* set back to ARPHRD_ETHER */
	dev->type = ARPHRD_ETHER;

	slsi_vif_deactivated(sdev, dev);
}

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int slsi_hcf_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	struct slsi_dev *sdev = (struct slsi_dev *)collect_client->prv;
	int ret = 0;
	u8 index = sdev->collect_mib.num_files;
	u8 i;
	u8 *data;

	SLSI_INFO_NODEV("Collecting WLAN HCF\n");

	if (!sdev->collect_mib.enabled)
		SLSI_INFO_NODEV("Collection not enabled\n");

	spin_lock(&sdev->collect_mib.in_collection);
	ret = scsc_log_collector_write(&index, sizeof(char), 1);
	if (ret) {
		spin_unlock(&sdev->collect_mib.in_collection);
		return ret;
	}

	for (i = 0; i < index; i++) {
		SLSI_INFO_NODEV("Collecting WLAN HCF. File %s\n", sdev->collect_mib.file[i].file_name);
		/* Write file name */
		ret = scsc_log_collector_write((char *)&sdev->collect_mib.file[i].file_name, 32, 1);
		if (ret) {
			spin_unlock(&sdev->collect_mib.in_collection);
			return ret;
		}
		/* Write file len */
		ret = scsc_log_collector_write((char *)&sdev->collect_mib.file[i].len, sizeof(u16), 1);
		if (ret) {
			spin_unlock(&sdev->collect_mib.in_collection);
			return ret;
		}
		/* Write data */
		data = sdev->collect_mib.file[i].data;
		if (!data)
			continue;
		ret = scsc_log_collector_write((char *)data, sdev->collect_mib.file[i].len, 1);
		if (ret) {
			spin_unlock(&sdev->collect_mib.in_collection);
			return ret;
		}
	}
	spin_unlock(&sdev->collect_mib.in_collection);

	return ret;
}

/* Collect client registration for HCF file*/
static struct scsc_log_collector_client slsi_hcf_client = {
	.name = "wlan_hcf",
	.type = SCSC_LOG_CHUNK_WLAN_HCF,
	.collect_init = NULL,
	.collect = slsi_hcf_collect,
	.collect_end = NULL,
	.prv = NULL,
};
#endif

int slsi_start(struct slsi_dev *sdev)
{
#ifndef CONFIG_SCSC_DOWNLOAD_FILE
	const struct firmware *fw[SLSI_WLAN_MAX_MIB_FILE] = { NULL, NULL };
#endif
	int  err = 0, r, reg_err = 0, stop_err;
	int i;
	char alpha2[3];
#ifdef CONFIG_SCSC_WLAN_AP_INFO_FILE
	u32 offset = 0;
	struct file *fp = NULL;
#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 9
	char *filepath = "/data/vendor/conn/.softap.info";
#else
	char *filepath = "/data/misc/conn/.softap.info";
#endif
	char buf[512];
#endif
#ifdef CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA
	char *ant_file_path = "/data/vendor/conn/.ant.info";
	char *antenna_file_path = "/data/vendor/wifi/antenna.info";
#endif

	if (WARN_ON(!sdev))
		return -EINVAL;

	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);

	slsi_wake_lock(&sdev->wlan_wl);

	if (sdev->device_state != SLSI_DEVICE_STATE_STOPPED) {
		SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "Device already started: device_state:%d\n", sdev->device_state);
		goto done;
	}

	if (!sdev->mac_changed) {
		slsi_reset_channel_flags(sdev);
		slsi_regd_init(sdev);
	} else {
		sdev->mac_changed = false;
	}

	if (sdev->recovery_status) {
		r = wait_for_completion_timeout(&sdev->recovery_completed,
						msecs_to_jiffies(sdev->recovery_timeout));
		if (r == 0)
			SLSI_INFO(sdev, "recovery_completed timeout\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
		reinit_completion(&sdev->recovery_completed);
#else
		/*This is how the macro is used in the older version.*/
		INIT_COMPLETION(sdev->recovery_completed);
#endif
	}

	sdev->device_state = SLSI_DEVICE_STATE_STARTING;
	sdev->require_service_close = false;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	reinit_completion(&sdev->sig_wait.completion);
#else
	INIT_COMPLETION(sdev->sig_wait.completion);
#endif

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Step [1/2]: Start WLAN service\n");
	SLSI_EC_GOTO(slsi_sm_wlan_service_open(sdev), err, err_done);
	/**
	 * Download MIB data, if any.
	 */
	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Step [2/3]: Send MIB configuration\n");

	sdev->local_mib.mib_hash = 0; /* Reset localmib hash value */
#ifndef SLSI_TEST_DEV
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	spin_lock_init(&sdev->collect_mib.in_collection);
	sdev->collect_mib.num_files = 0;
	sdev->collect_mib.enabled = false;
#endif
#ifndef CONFIG_SCSC_DOWNLOAD_FILE
	/* The "_t" HCF is used in RF Test mode and wlanlite/production test mode */
	if (slsi_is_rf_test_mode_enabled() || slsi_is_test_mode_enabled()) {
		sdev->mib[0].mib_file_name = mib_file_t;
		sdev->mib[1].mib_file_name = mib_file2_t;
	} else {
		sdev->mib[0].mib_file_name = slsi_mib_file;
		sdev->mib[1].mib_file_name = slsi_mib_file2;
	}

	/* Place MIB files in shared memory */
	for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++) {
		err = slsi_mib_open_file(sdev, &sdev->mib[i], &fw[i]);

		/* Only the first file is mandatory */
		if (i == 0 && err) {
			SLSI_ERR(sdev, "mib: Mandatory wlan hcf missing. WLAN will not start (err=%d)\n", err);
			slsi_sm_wlan_service_close(sdev);
			goto err_done;
		}
	}

	err = slsi_sm_wlan_service_start(sdev);
	if (err) {
		SLSI_ERR(sdev, "slsi_sm_wlan_service_start failed: err=%d\n", err);
		for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++)
			slsi_mib_close_file(sdev, fw[i]);
		if (err != -EILSEQ)
			slsi_sm_wlan_service_close(sdev);
		goto err_done;
	}

	for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++)
		slsi_mib_close_file(sdev, fw[i]);
#else
	/* Download main MIB file via mlme_set */
	err = slsi_sm_wlan_service_start(sdev);
	if (err) {
		SLSI_ERR(sdev, "slsi_sm_wlan_service_start failed: err=%d\n", err);
		if (err != -EILSEQ)
			slsi_sm_wlan_service_close(sdev);
		goto err_done;
	}
	SLSI_EC_GOTO(slsi_mib_download_file(sdev, &sdev->mib), err, err_hip_started);
#endif
	/* Always try to download optional localmib file via mlme_set, ignore error */
	(void)slsi_mib_download_file(sdev, &sdev->local_mib);
#endif
	/**
	 * Download MIB data, if any.
	 * Get f/w capabilities and default configuration
	 * configure firmware
	 */
	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	sdev->device_config.rssi_boost_2g = 0;
	sdev->device_config.rssi_boost_5g = 0;
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Step [3/3]: Get MIB configuration\n");
	SLSI_EC_GOTO(slsi_mib_initial_get(sdev), err, err_hip_started);
	SLSI_INFO(sdev, "=== Version info from the [MIB] ===\n");
	SLSI_INFO(sdev, "HW Version : 0x%.4X (%u)\n", sdev->chip_info_mib.chip_version, sdev->chip_info_mib.chip_version);
	SLSI_INFO(sdev, "Platform : 0x%.4X (%u)\n", sdev->plat_info_mib.plat_build, sdev->plat_info_mib.plat_build);
	slsi_cfg80211_update_wiphy(sdev);

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	sdev->device_config.host_state = SLSI_HOSTSTATE_CELLULAR_ACTIVE;
	reg_err = slsi_read_regulatory(sdev);
	if (reg_err) {
		SLSI_INFO(sdev, "Error in reading regulatory!\n");
		/* Get UnifiCountryList */
		err = slsi_read_unifi_countrylist(sdev, SLSI_PSID_UNIFI_COUNTRY_LIST);
		if (err) {
			SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
			goto err_hip_started;
		}
	}
	if (sdev->regdb.regdb_state == SLSI_REG_DB_SET) {
		sdev->reg_dom_version = ((sdev->regdb.db_major_version & 0xFF) << 8) |
					(sdev->regdb.db_minor_version & 0xFF);
	}
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);

	if (!slsi_is_test_mode_enabled()) {
		err = slsi_mlme_set_country(sdev, sdev->device_config.domain_info.regdomain->alpha2);

		if (err < 0)
			goto err_hip_started;
	}

	memcpy(alpha2, sdev->device_config.domain_info.regdomain->alpha2, 2);

	/* unifiDefaultCountry != world_domain */
	if (!(alpha2[0] == '0' && alpha2[1] == '0'))
		/* Read the regulatory params for the country*/
		if (slsi_read_regulatory_rules(sdev, &sdev->device_config.domain_info, alpha2) == 0) {
			slsi_reset_channel_flags(sdev);
			wiphy_apply_custom_regulatory(sdev->wiphy, sdev->device_config.domain_info.regdomain);
		}
	/* Do nothing for unifiDefaultCountry == world_domain */

	/* write .wifiver.info */
	/* Needed for MCD projects only */
	write_wifi_version_info_file(sdev);

	/* write .cid.info */
	write_m_test_chip_version_file(sdev);

#ifdef CONFIG_SCSC_WLAN_AP_INFO_FILE
	/* writing .softap.info in /data/vendor/conn */
	fp = filp_open(filepath, O_WRONLY | O_CREAT, 0644);

	if (!fp)  {
		SLSI_WARN(sdev, "%s doesn't exist\n", filepath);
	} else if (IS_ERR(fp)) {
		SLSI_WARN(sdev, "%s open returned error %d\n", filepath, IS_ERR(fp));
	} else {
		offset = snprintf(buf + offset, sizeof(buf), "#softap.info\n");
		offset += snprintf(buf + offset, sizeof(buf), "DualBandConcurrency=%s\n", sdev->dualband_concurrency ? "yes" : "no");
		offset += snprintf(buf + offset, sizeof(buf), "DualInterface=%s\n", "yes");
		offset += snprintf(buf + offset, sizeof(buf), "5G=%s\n", sdev->band_5g_supported ? "yes" : "no");
		offset += snprintf(buf + offset, sizeof(buf), "maxClient=%d\n", !sdev->softap_max_client ? SLSI_MIB_MAX_CLIENT : sdev->softap_max_client);

		/* following are always supported */
		offset += snprintf(buf + offset, sizeof(buf), "HalFn_setCountryCodeHal=yes\n");
		offset += snprintf(buf + offset, sizeof(buf), "HalFn_getValidChannels=yes\n");
/* If WLBTD is being used which we will be doing for 5.4 kernel project we will use daemon for writing file */
#ifdef CONFIG_SCSC_WLBTD
		wlbtd_write_file(filepath, buf);
#else
		/* Will only be used for old projects before WLBTD was introduced (Android O)*/
		kernel_write(fp, buf, strlen(buf), 0);
#endif
		if (fp)
			filp_close(fp, NULL);

		SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Succeed to write softap information to .softap.info\n");
	}
#endif

#ifdef CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA
	if (slsi_is_rf_test_mode_enabled()) {
		/* reading antenna mode from configured file /data/vendor/conn/.ant.info */
		if (!(slsi_read_preferred_antenna_from_file(sdev, ant_file_path))) {
			/* reading antenna mode from configured file /data/vendor/wifi/antenna.info */
			slsi_read_preferred_antenna_from_file(sdev, antenna_file_path);
		}
	}
#endif

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Register with log collector to collect wlan hcf file */
	slsi_hcf_client.prv = sdev;
	scsc_log_collector_register_client(&slsi_hcf_client);
	sdev->collect_mib.enabled = true;
#endif
	slsi_update_supported_channels_regd_flags(sdev);
	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "---Driver started successfully---\n");
	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	for (i = 0; i < SLSI_MAX_RTT_ID; i++)
		sdev->rtt_id_params[i] = NULL;
#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
	sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_STARTED;
#endif
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);

	slsi_kic_system_event(slsi_kic_system_event_category_initialisation,
			      slsi_kic_system_events_wifi_service_driver_started, GFP_KERNEL);

	slsi_wake_unlock(&sdev->wlan_wl);
	return err;

err_hip_started:
#ifndef SLSI_TEST_DEV
	stop_err = slsi_sm_wlan_service_stop(sdev);
	slsi_hip_stop(sdev);
	if (stop_err != -EUSERS)
		slsi_sm_wlan_service_close(sdev);
#endif

err_done:
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;

done:
	slsi_wake_unlock(&sdev->wlan_wl);

	slsi_kic_system_event(slsi_kic_system_event_category_initialisation,
			      slsi_kic_system_events_wifi_on, GFP_KERNEL);

	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return err;
}

#ifdef CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA
bool slsi_read_preferred_antenna_from_file(struct slsi_dev *sdev, char *antenna_file_path)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	char ant_mode = '0';
	u16 antenna = 0;
	int ret = 0;
	struct file *file_ptr = NULL;

	file_ptr = filp_open(antenna_file_path, O_RDONLY, 0);
	if (!file_ptr || IS_ERR(file_ptr)) {
		SLSI_DBG1(sdev, SLSI_CFG80211, "%s open returned error %d\n", antenna_file_path, IS_ERR(file_ptr));
		return false;
	} else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		ret = kernel_read(file_ptr, &ant_mode, 1, &file_ptr->f_pos);
#else
		ret = kernel_read(file_ptr, file_ptr->f_pos, &ant_mode, 1);
#endif
		if (ret < 0) {
			SLSI_INFO_NODEV("Kernel read error found\n");
			filp_close(file_ptr, NULL);
			return false;
		}
		antenna = ant_mode - '0';
		filp_close(file_ptr, NULL);
		slsi_set_mib_preferred_antenna(sdev, antenna);
		return true;
	}
#else
	return false;
#endif
}
#endif

struct net_device *slsi_dynamic_interface_create(struct wiphy        *wiphy,
					     const char          *name,
					     enum nl80211_iftype type,
					     struct vif_params   *params)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;
	int               err = -EINVAL;
	int               iface;

	SLSI_DBG1(sdev, SLSI_CFG80211, "name:%s\n", name);

	iface = slsi_netif_dynamic_iface_add(sdev, name);
	if (iface < 0)
		return NULL;

	dev = slsi_get_netdev(sdev, iface);
	if (!dev)
		return NULL;

	ndev_vif = netdev_priv(dev);

	err = slsi_netif_register_rtlnl_locked(sdev, dev);
	if (err)
		return NULL;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->iftype = type;
	dev->ieee80211_ptr->iftype = type;
	if (params)
		dev->ieee80211_ptr->use_4addr = params->use_4addr;
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);

	return dev;
}

static void slsi_stop_chip(struct slsi_dev *sdev)
{
#ifndef SLSI_TEST_DEV
	int stop_err;
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	u8 index = sdev->collect_mib.num_files;
	u8 i;
#endif
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(sdev->start_stop_mutex));

	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "netdev_up_count:%d device_state:%d\n", sdev->netdev_up_count, sdev->device_state);
	sdev->mac_changed = false;

	if (sdev->device_state != SLSI_DEVICE_STATE_STARTED)
		return;

	/* Only shutdown on the last device going down. */
	if (sdev->netdev_up_count)
		return;

	complete_all(&sdev->sig_wait.completion);

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	sdev->collect_mib.enabled = false;
	scsc_log_collector_unregister_client(&slsi_hcf_client);
	for (i = 0; i < index; i++)
		kfree(sdev->collect_mib.file[i].data);
#endif

	sdev->device_state = SLSI_DEVICE_STATE_STOPPING;
#ifndef SLSI_TEST_DEV
	stop_err = slsi_sm_wlan_service_stop(sdev);
#else
	slsi_sm_wlan_service_stop(sdev);
#endif
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;

	slsi_hip_stop(sdev);
#ifndef SLSI_TEST_DEV
	if (stop_err != -EUSERS)
		slsi_sm_wlan_service_close(sdev);
#endif
	slsi_kic_system_event(slsi_kic_system_event_category_deinitialisation,
			      slsi_kic_system_events_wifi_service_driver_stopped, GFP_KERNEL);

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	sdev->mlme_blocked = false;

	slsi_kic_system_event(slsi_kic_system_event_category_deinitialisation,
			      slsi_kic_system_events_wifi_off, GFP_KERNEL);

#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
	if (atomic_read(&sdev->arp_tx_count) && atomic_read(&sdev->ctrl_pause_state))
		scsc_wifi_unpause_arp_q_all_vif(sdev);
	atomic_set(&sdev->arp_tx_count, 0);
#endif
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
void slsi_ndl_vif_cleanup(struct slsi_dev *sdev, struct net_device *dev, bool hw_available)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int i;
	struct slsi_peer *peer;
	u32 ndp_instance_id;
	struct net_device *nan_mgmt_dev = slsi_get_netdev_locked(sdev, SLSI_NET_INDEX_NAN);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
	netif_carrier_off(dev);
	slsi_spinlock_lock(&ndev_vif->peer_lock);
	for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
		peer = ndev_vif->peer_sta_record[i];
		while (peer && peer->valid) {
			ndp_instance_id = slsi_nan_get_ndp_from_ndl_local_ndi(nan_mgmt_dev, peer->ndl_vif, dev->dev_addr);
			slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
			if(peer->ndp_count == 1)
				slsi_peer_remove(sdev, dev, peer);
			peer->ndp_count--;
			if (ndev_vif->nan.ndp_count > 0)
				ndev_vif->nan.ndp_count--;
			if (nan_mgmt_dev && ndp_instance_id < SLSI_NAN_MAX_NDP_INSTANCES + 1)
				slsi_nan_ndp_del_entry(sdev, nan_mgmt_dev, ndp_instance_id, true);
		}
	}

	slsi_spinlock_unlock(&ndev_vif->peer_lock);
}
#endif
void slsi_vif_cleanup(struct slsi_dev *sdev, struct net_device *dev, bool hw_available, bool is_recovery)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               i;

	SLSI_NET_DBG3(dev, SLSI_INIT_DEINIT, "clean VIF\n");

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	if (ndev_vif->ifnum >= SLSI_NAN_DATA_IFINDEX_START) {
		slsi_ndl_vif_cleanup(sdev, dev, hw_available);
		return;
	}
#endif
	if (ndev_vif->activated) {
		netif_carrier_off(dev);
		for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
			struct slsi_peer *peer = ndev_vif->peer_sta_record[i];

			if (peer && peer->valid)
				slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
		}

		if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
			bool already_disconnected = false;

			SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Station active: hw_available=%d\n", hw_available);
			if (hw_available) {
				if (ndev_vif->sta.sta_bss) {
					slsi_mlme_disconnect(sdev, dev, ndev_vif->sta.sta_bss->bssid, FAPI_REASONCODE_UNSPECIFIED_REASON, true);
					slsi_handle_disconnect(sdev, dev, ndev_vif->sta.sta_bss->bssid, 0, NULL, 0);
					already_disconnected = true;
				} else {
					if (slsi_mlme_del_vif(sdev, dev) != 0)
						SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
				}
			}
			if (!already_disconnected) {
				SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Calling slsi_vif_deactivated\n");
				slsi_vif_deactivated(sdev, dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
				cfg80211_disconnected(dev, is_recovery ? HOST_REASONCODE_RECOVERY_REASON :
						      FAPI_REASONCODE_UNSPECIFIED_REASON, NULL, 0, false, GFP_ATOMIC);
#else
				cfg80211_disconnected(dev, is_recovery ? HOST_REASONCODE_RECOVERY_REASON :
						      FAPI_REASONCODE_UNSPECIFIED_REASON, NULL, 0, GFP_ATOMIC);
#endif
			}
		} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
			SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "AP active\n");
			if (hw_available) {
				struct slsi_peer *peer;
				int              j = 0;

				while (j < SLSI_PEER_INDEX_MAX) {
					peer = ndev_vif->peer_sta_record[j];
					if (peer && peer->valid)
						slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
					++j;
				}
				if (slsi_mlme_del_vif(sdev, dev) != 0)
					SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
			}
			SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Calling slsi_vif_deactivated\n");
			slsi_vif_deactivated(sdev, dev);

			if (ndev_vif->iftype == NL80211_IFTYPE_P2P_GO)
				SLSI_P2P_STATE_CHANGE(sdev, P2P_IDLE_NO_VIF);
		} else if (ndev_vif->vif_type == FAPI_VIFTYPE_UNSYNCHRONISED) {
			if (SLSI_IS_VIF_INDEX_WLAN(ndev_vif)) {
				slsi_wlan_unsync_vif_deactivate(sdev, dev, hw_available);
			} else {
				SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "P2P active - Deactivate\n");
				slsi_p2p_vif_deactivate(sdev, dev, hw_available);
			}
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
		} else if (ndev_vif->vif_type == FAPI_VIFTYPE_NAN) {
			if (hw_available) {
				if (slsi_mlme_del_vif(sdev, dev) != 0)
					SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
			}
			ndev_vif->activated = false;
			slsi_nan_send_disabled_event(sdev, dev, hw_available ? SLSI_HAL_NAN_STATUS_SUCCESS :
						     SLSI_HAL_NAN_STATUS_INTERNAL_FAILURE);
#endif
		}

		else if (ndev_vif->vif_type == FAPI_VIFTYPE_MONITOR)
			slsi_stop_monitor_mode(sdev, dev);
	}
}

void slsi_sched_scan_stopped(struct work_struct *work)
{
	struct netdev_vif *ndev_vif = container_of(work, struct netdev_vif, sched_scan_stop_wk);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy;
	u64 reqid = 0;
	bool sched_scan_req = false;

	wiphy = sdev->wiphy;
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	if (ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req) {
		reqid = ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req->reqid;
		sched_scan_req = true;
		ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req = NULL;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	if (sched_scan_req) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
		cfg80211_sched_scan_stopped(wiphy, reqid);
#else
		cfg80211_sched_scan_stopped(wiphy);
#endif
	}
}

void slsi_scan_cleanup(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int               i;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	struct cfg80211_scan_info info = {.aborted = false};
#endif

	SLSI_NET_DBG3(dev, SLSI_INIT_DEINIT, "clean scan_data\n");

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	for (i = 0; i < SLSI_SCAN_MAX; i++) {
		if ((ndev_vif->scan[i].scan_req || ndev_vif->scan[i].acs_request) &&
		    !sdev->mlme_blocked)
			slsi_mlme_del_scan(sdev, dev, (ndev_vif->ifnum << 8 | i), false);
		slsi_purge_scan_results(ndev_vif, i);
		if (ndev_vif->scan[i].scan_req && i == SLSI_SCAN_HW_ID) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
			info.aborted = true;
			cfg80211_scan_done(ndev_vif->scan[i].scan_req, &info);
#else
			cfg80211_scan_done(ndev_vif->scan[i].scan_req, true);
#endif
		}

		if (ndev_vif->scan[i].sched_req && i == SLSI_SCAN_SCHED_ID)
			queue_work(sdev->device_wq, &ndev_vif->sched_scan_stop_wk);

		ndev_vif->scan[i].scan_req = NULL;
		kfree(ndev_vif->scan[i].acs_request);
		ndev_vif->scan[i].acs_request = NULL;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

static void slsi_stop_net_dev_locked(struct slsi_dev *sdev, struct net_device *dev, bool hw_available)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "Stopping netdev_up_count=%d, hw_available = %d\n", sdev->netdev_up_count, hw_available);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(sdev->start_stop_mutex));

	if (!ndev_vif->is_available) {
		SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "Not Available\n");
		return;
	}

	if (WARN_ON(!sdev->netdev_up_count)) {
		SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "sdev->netdev_up_count=%d\n", sdev->netdev_up_count);
		return;
	}

	slsi_scan_cleanup(sdev, dev);

	cancel_work_sync(&ndev_vif->set_multicast_filter_work);
	cancel_work_sync(&ndev_vif->update_pkt_filter_work);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	slsi_vif_cleanup(sdev, dev, hw_available, 0);
	slsi_spinlock_lock(&sdev->netdev_lock);
	ndev_vif->is_available = false;
	sdev->netdev_up_count--;
	slsi_spinlock_unlock(&sdev->netdev_lock);
#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
	if (atomic_read(&ndev_vif->arp_tx_count) && atomic_read(&sdev->ctrl_pause_state))
		scsc_wifi_unpause_arp_q_all_vif(sdev);
	atomic_set(&ndev_vif->arp_tx_count, 0);
#endif
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);

	complete_all(&ndev_vif->sig_wait.completion);
	slsi_stop_chip(sdev);
}

/* Called when a net device wants to go DOWN */
void slsi_stop_net_dev(struct slsi_dev *sdev, struct net_device *dev)
{
	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	slsi_stop_net_dev_locked(sdev, dev, sdev->recovery_status ? false : true);
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
}

/* Called when we get sdio_removed */
void slsi_stop(struct slsi_dev *sdev)
{
	struct net_device *dev;
	int               i;

	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "netdev_up_count:%d\n", sdev->netdev_up_count);

	complete_all(&sdev->sig_wait.completion);

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
		dev = slsi_get_netdev_locked(sdev, i);
		if (dev)
			slsi_stop_net_dev_locked(sdev, sdev->netdev[i], false);
	}
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
}

/* MIB download handling */
static u8 *slsi_mib_slice(struct slsi_dev *sdev, const u8 *data, u32 length, u32 *p_parsed_len,
			  u32 *p_mib_slice_len)
{
	const u8 *p = data;
	u8       *mib_slice;
	u32      mib_slice_len = 0;

	SLSI_UNUSED_PARAMETER(sdev);

	if (!length)
		return NULL;

	mib_slice = kmalloc(length + 4, GFP_KERNEL);
	if (!mib_slice)
		return NULL;

	while (length >= 4) {
		u16 psid = SLSI_BUFF_LE_TO_U16(p);
		u16 pslen = (u16)(4 + SLSI_BUFF_LE_TO_U16(&p[2]));

		if (pslen & 0x1)
			pslen++;

		if (psid & CSR_WIFI_SME_MIB2_HOST_PSID_MASK) {
			/* do nothing */
		} else {
			/* SLSI_ERR (sdev, "PSID=0x%04X : FW\n", psid); */
#define CSR_WIFI_HOSTIO_MIB_SET_MAX     (1800)
			if ((mib_slice_len + pslen) > CSR_WIFI_HOSTIO_MIB_SET_MAX)
				break;
			if (pslen > length + 4) {
				SLSI_ERR(sdev, "length %u read from MIB file > space %u - corrupt file?\n", pslen, length + 4);
				mib_slice_len = 0;
				break;
			}
			memcpy(&mib_slice[mib_slice_len], p, pslen);
			mib_slice_len += pslen;
		}
		p += pslen;
		length -= pslen;
	}

	*p_mib_slice_len = mib_slice_len;
	*p_parsed_len = (p - data);

	return mib_slice;
}

/* Extract the platform name string from the HCF file */
static int slsi_mib_get_platform(struct slsi_dev_mib_info *mib_info)
{
	size_t plat_name_len;
	int pos = 0;

	/* The mib_data passed to this function should already
	 * have had its HCF header skipped.
	 *
	 * This is shoehorned into specific PSIDs to allow backward
	 * compatibility, so we must look into the HCF payload
	 * instead of the header :(
	 *
	 * The updated configcmd util guarantees that these keys
	 * will appear first:
	 *
	 * PSIDs:
	 * 0xfffe - 16 bit version ID, value 1.
	 * 0xffff - If version ID=1, holds platform name string.
	 */

	mib_info->platform[0] = '\0';

	/* Sanity - payload long enough for info? */
	if (mib_info->mib_len < 12) {
		SLSI_INFO_NODEV("HCF file too short\n");
		return -EINVAL;				/* file too short */
	}

	if (mib_info->mib_data[pos++] != 0xFE ||	/* Version ID FFFE */
	    mib_info->mib_data[pos++] != 0xFF) {
		SLSI_INFO_NODEV("No HCF version ID\n");
		return -EINVAL;				/* No version ID */
	}
	if (mib_info->mib_data[pos++] != 0x01 ||	/* Len 1, LE */
	    mib_info->mib_data[pos++] != 0x00) {
		SLSI_INFO_NODEV("Bad length\n");
		return -EINVAL;				/* Unknown length */
	}
	if (mib_info->mib_data[pos++] != 0x01 ||	/* Header ID 1, LE */
	    mib_info->mib_data[pos++] != 0x00) {
		SLSI_INFO_NODEV("Bad version ID\n");
		return -EINVAL;				/* Unknown version ID */
	}
	if (mib_info->mib_data[pos++] != 0xFF ||	/* Platform Name FFFF */
	    mib_info->mib_data[pos++] != 0xFF) {
		SLSI_INFO_NODEV("No HCF platform name\n");
		return -EINVAL;				/* No platform name */
	}

	/* Length of platform name */
	plat_name_len = mib_info->mib_data[pos++];
	plat_name_len |= (mib_info->mib_data[pos++] << 16);

	/* Sanity check */
	if (plat_name_len + pos > mib_info->mib_len || plat_name_len < 2) {
		SLSI_ERR_NODEV("Bad HCF FFFF key length %zu\n",
			       plat_name_len);
		return -EINVAL;				/* Implausible length */
	}

	/* Skip vldata header SC-506179-SP. This conveys the
	 * length of the platform string and is 2 or 3 octets long
	 * depending on the length of the string.
	 */
	{
#define SLSI_VLDATA_STRING	0xA0
#define SLSI_VLDATA_LEN		0x17

		u8 vlen_hdr = mib_info->mib_data[pos++];
		u8 vlen_len = vlen_hdr & SLSI_VLDATA_LEN; /* size of length field */

		/* Skip vlen header octet */
		plat_name_len--;

		SLSI_DBG1_NODEV(SLSI_INIT_DEINIT, "vlhdr 0x%x, len %u\n", vlen_hdr, vlen_len);

		/* Is it an octet string type? */
		if (!(vlen_hdr & SLSI_VLDATA_STRING)) {
			SLSI_ERR_NODEV("No string vlen header 0x%x\n", vlen_hdr);
			return -EINVAL;
		}

		/* Handle 1 or 2 octet length field only */
		if (vlen_len > 2) {
			SLSI_ERR_NODEV("Too long octet string header %u\n", vlen_len);
			return -EINVAL;
		}

		/* Skip over the string length field.
		 * Note we just use datalength anyway.
		 */
		pos += vlen_len;
		plat_name_len -= vlen_len;
	}

	/* Limit the platform name to space in driver and read */
	{
		size_t trunc_len = plat_name_len;

		if (trunc_len >= sizeof(mib_info->platform))
			trunc_len = sizeof(mib_info->platform) - 1;

		/* Extract platform name */
		memcpy(mib_info->platform, &mib_info->mib_data[pos], trunc_len);
		mib_info->platform[trunc_len] = '\0';

		/* Print non-truncated string in log now */
		SLSI_INFO_NODEV("MIB platform: %.*s\n", (int)plat_name_len, &mib_info->mib_data[pos]);

		SLSI_DBG1_NODEV(SLSI_INIT_DEINIT, "plat_name_len: %zu + %u\n",
				plat_name_len, (plat_name_len & 1));
	}

	/* Pad string to 16-bit boundary */
	plat_name_len += (plat_name_len & 1);
	pos += plat_name_len;

	/* Advance over the keys we read, FW doesn't need them */
	mib_info->mib_data += pos;
	mib_info->mib_len -= pos;

	SLSI_DBG1_NODEV(SLSI_INIT_DEINIT, "Skip %d octets HCF payload\n", pos);

	return 0;
}

#define MGT_HASH_SIZE_BYTES	2 /* Hash will be contained in a uint32 */
#define MGT_HASH_OFFSET		4
static int slsi_mib_open_file(struct slsi_dev *sdev, struct slsi_dev_mib_info *mib_info, const struct firmware **fw)
{
	int r = -1;
	const struct firmware *e = NULL;
	const char *mib_file_ext;
	char path_name[MX_WLAN_FILE_PATH_LEN_MAX];
	char *mib_file_name = mib_info->mib_file_name;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	u8 index = sdev->collect_mib.num_files;
	u8 *data;
#endif

	if (!mib_file_name || !fw)
		return -EINVAL;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	if (index > SLSI_WLAN_MAX_MIB_FILE) {
		SLSI_ERR(sdev, "collect mib index is invalid:%d\n", index);
		return -EINVAL;
	}
#endif
	mib_info->mib_data = NULL;
	mib_info->mib_len = 0;
	mib_info->mib_hash = 0; /* Reset mib hash value */

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "MIB file - Name : %s\n", mib_file_name);

	/* Use MIB file compatibility mode? */
	mib_file_ext = strrchr(mib_file_name, '.');
	if (!mib_file_ext) {
		SLSI_ERR(sdev, "configuration file name '%s' invalid\n", mib_file_name);
		return -EINVAL;
	}

	/* Build MIB file path from override */
	scnprintf(path_name, MX_WLAN_FILE_PATH_LEN_MAX, "wlan/%s", mib_file_name);
	SLSI_INFO(sdev, "Path to the MIB file : %s\n", path_name);

	r = mx140_file_request_conf(sdev->maxwell_core, &e, "wlan", mib_file_name);
	if (r || (!e)) {
		SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Skip MIB download as file %s is NOT found\n", mib_file_name);
		*fw = e;
		return r;
	}

	mib_info->mib_data = (u8 *)e->data;
	mib_info->mib_len = e->size;

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	spin_lock(&sdev->collect_mib.in_collection);
	memset(&sdev->collect_mib.file[index].file_name, 0, 32);
	memcpy(&sdev->collect_mib.file[index].file_name, mib_file_name, 32);
	sdev->collect_mib.file[index].len = mib_info->mib_len;
	data = kmalloc(mib_info->mib_len, GFP_ATOMIC);
	if (!data) {
		spin_unlock(&sdev->collect_mib.in_collection);
		goto cont;
	}
	memcpy(data, mib_info->mib_data, mib_info->mib_len);
	sdev->collect_mib.file[index].data = data;
	sdev->collect_mib.num_files += 1;
	spin_unlock(&sdev->collect_mib.in_collection);
cont:
#endif
	/* Check MIB file header */
	if (mib_info->mib_len >= 8 &&              /* Room for header */
		/*(sdev->mib_data[6] & 0xF0) == 0x20 && */ /* WLAN subsystem */
		mib_info->mib_data[7] == 1) {      /* First file format */
		int i;

		mib_info->mib_hash = 0;

		for (i = 0; i < MGT_HASH_SIZE_BYTES; i++)
			mib_info->mib_hash = (mib_info->mib_hash << 8) | mib_info->mib_data[i + MGT_HASH_OFFSET];

		SLSI_INFO(sdev, "MIB hash: 0x%.04x\n", mib_info->mib_hash);
		/* All good - skip header and continue */
		mib_info->mib_data += 8;
		mib_info->mib_len -= 8;

		/* Extract platform name if available */
		slsi_mib_get_platform(mib_info);
	} else {
		/* Bad header */
		SLSI_ERR(sdev, "configuration file '%s' has bad header\n", mib_info->mib_file_name);
		mx140_file_release_conf(sdev->maxwell_core, e);
		return -EINVAL;
	}

	*fw = e;
	return 0;
}

static int slsi_mib_close_file(struct slsi_dev *sdev, const struct firmware *e)
{
	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "MIB close %p\n", e);

	if (!e || !sdev)
		return -EIO;

	mx140_file_release_conf(sdev->maxwell_core, e);

	return 0;
}

static int slsi_mib_download_file(struct slsi_dev *sdev, struct slsi_dev_mib_info *mib_info)
{
	int r = -1;
	const struct firmware *e = NULL;
	u8 *mib_slice;
	u32 mib_slice_len, parsed_len;

	r = slsi_mib_open_file(sdev, mib_info, &e);
	if (r)
		return r;
	/**
	 * MIB data should not be larger than CSR_WIFI_HOSTIO_MIB_SET_MAX.
	 * Slice it into smaller ones and download one by one
	 */
	while (mib_info->mib_len > 0) {
		mib_slice = slsi_mib_slice(sdev, mib_info->mib_data, mib_info->mib_len, &parsed_len, &mib_slice_len);
		if (!mib_slice)
			break;
		if (mib_slice_len == 0 || mib_slice_len > mib_info->mib_len) {
			/* Sanity check MIB parsing */
			SLSI_ERR(sdev, "slsi_mib_slice returned implausible %d\n", mib_slice_len);
			r = -EINVAL;
			kfree(mib_slice);
			break;
		}
		r = slsi_mlme_set(sdev, NULL, mib_slice, mib_slice_len);
		kfree(mib_slice);
		if (r != 0)     /* some mib can fail to be set, but continue */
			SLSI_ERR(sdev, "mlme set failed r=0x%x during downloading:'%s'\n",
				 r, mib_info->mib_file_name);

		mib_info->mib_data += parsed_len;
		mib_info->mib_len -= parsed_len;
	}

	slsi_mib_close_file(sdev, e);

	return r;
}

static int slsi_mib_initial_get(struct slsi_dev *sdev)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	int *band = sdev->supported_5g_channels;
	int rx_len = 0;
	int r;
	int i = 0;
	int j = 0;
	int chan_start = 0;
	int chan_count = 0;
	int index = 0;
	int mib_index = 0;
	u16 num_antenna = 0;
	u16 antenna_lw_bit = 0;
	u16 antenna_hg_bit = 0;
	static const struct slsi_mib_get_entry get_values[] = {{ SLSI_PSID_UNIFI_CHIP_VERSION,            { 0, 0 } },
							       { SLSI_PSID_UNIFI_SUPPORTED_CHANNELS,      { 0, 0 } },
							       { SLSI_PSID_UNIFI_HT_ACTIVATED, {0, 0} },
							       { SLSI_PSID_UNIFI_VHT_ACTIVATED, {0, 0} },
							       { SLSI_PSID_UNIFI_HT_CAPABILITIES, {0, 0} },
							       { SLSI_PSID_UNIFI_VHT_CAPABILITIES, {0, 0} },
							       { SLSI_PSID_UNIFI_HARDWARE_PLATFORM, {0, 0} },
							       { SLSI_PSID_UNIFI_REG_DOM_VERSION, {0, 0} },
							       { SLSI_PSID_UNIFI_NAN_ACTIVATED, {0, 0} },
							       { SLSI_PSID_UNIFI_DEFAULT_DWELL_TIME, {0, 0} },
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
							       { SLSI_PSID_UNIFI_WI_FI_SHARING5_GHZ_CHANNEL, {0, 0} },
#endif
#ifdef CONFIG_SCSC_WLAN_AP_INFO_FILE
							       { SLSI_PSID_UNIFI_DUAL_BAND_CONCURRENCY, {0, 0} },
							       { SLSI_PSID_UNIFI_MAX_CLIENT, {0, 0} },
#endif
#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
							       { SLSI_PSID_UNIFI_MAC_ADDRESS_RANDOMISATION, {0, 0} },
#endif
							       { SLSI_PSID_UNIFI_DEFAULT_COUNTRY_WITHOUT_CH12_CH13, {0, 0} },
#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
							       { SLSI_PSID_UNIFI_ARP_DETECT_ACTIVATED, {0, 0} },
#endif
#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
							       { SLSI_PSID_UNIFI_ARP_OUTSTANDING_MAX, {0, 0} },
#endif
							       { SLSI_PSID_UNIFI_APF_ACTIVATED, {0, 0} },
							       { SLSI_PSID_UNIFI_SOFT_AP40_MHZ_ON24G, {0, 0} },
							       { SLSI_PSID_UNIFI_MAX_NUM_ANTENNA_TO_USE, {0, 0} },
							       { SLSI_PSID_UNIFI_EXTENDED_CAPABILITIES, {0, 0} },
							      };/*Check the mibrsp.dataLength when a new mib is added*/

	r = slsi_mib_encode_get_list(&mibreq, sizeof(get_values) / sizeof(struct slsi_mib_get_entry), get_values);
	if (r != SLSI_MIB_STATUS_SUCCESS)
		return -ENOMEM;

	mibrsp.dataLength = 240;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, NULL, mibreq.data, mibreq.dataLength, mibrsp.data, mibrsp.dataLength, &rx_len);
	kfree(mibreq.data);
	if (r == 0) {
		struct slsi_mib_value *values;

		mibrsp.dataLength = (u32)rx_len;

		values = slsi_mib_decode_get_list(&mibrsp, sizeof(get_values) / sizeof(struct slsi_mib_get_entry), get_values);

		if (!values) {
			kfree(mibrsp.data);
			return -EINVAL;
		}

		if (values[mib_index].type != SLSI_MIB_TYPE_NONE) {    /* CHIP_VERSION */
			SLSI_CHECK_TYPE(sdev, values[mib_index].type, SLSI_MIB_TYPE_UINT);
			sdev->chip_info_mib.chip_version = values[mib_index].u.uintValue;
		}

		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {    /* SUPPORTED_CHANNELS */
			SLSI_CHECK_TYPE(sdev, values[mib_index].type, SLSI_MIB_TYPE_OCTET);
			if (values[mib_index].type == SLSI_MIB_TYPE_OCTET) {
#ifdef CONFIG_SCSC_WLAN_DEBUG
				int k = 0;
				int increment = 4; /* increment channel by 4 for 5G and by 1 for 2G */
				int buf_len = 150; /* 150 bytes for 14+25=39 channels and spaces between them */
				char *supported_channels_buffer = NULL;
				int buf_pos = 0;

				supported_channels_buffer = kmalloc(buf_len, GFP_KERNEL);
				if (!supported_channels_buffer)
					return -ENOMEM;
#endif
				sdev->band_5g_supported = 0;
				memset(sdev->supported_2g_channels, 0, sizeof(sdev->supported_2g_channels));
				memset(sdev->supported_5g_channels, 0, sizeof(sdev->supported_5g_channels));
				for (i = 0; i < values[mib_index].u.octetValue.dataLength / 2; i++) {
					/* If any 5GHz channel is supported, update band_5g_supported */
					if ((values[mib_index].u.octetValue.data[i * 2] > 14) &&
					    (values[mib_index].u.octetValue.data[i * 2 + 1] > 0)) {
						sdev->band_5g_supported = 1;
						break;
					}
				}
				for (i = 0; i < values[mib_index].u.octetValue.dataLength; i += 2) {
					chan_start = values[mib_index].u.octetValue.data[i];
					chan_count = values[mib_index].u.octetValue.data[i + 1];
					band = sdev->supported_5g_channels;
#ifdef CONFIG_SCSC_WLAN_DEBUG
					k = 0;
					if (chan_start < 15)
						increment = 1;
					else
						increment = 4;
#endif
					if (chan_start < 15) {
						index = chan_start - 1;
						band = sdev->supported_2g_channels;
					} else if (chan_start >= 36 && chan_start <= 48) {
						index = (chan_start - 36) / 4;
					} else if (chan_start >= 52 && chan_start <= 64) {
						index = ((chan_start - 52) / 4) + 4;
					} else if (chan_start >= 100 && chan_start <= 140) {
						index = ((chan_start - 100) / 4) + 8;
					} else if (chan_start >= 149 && chan_start <= 165) {
						index = ((chan_start - 149) / 4) + 20;
					} else {
						continue;
					}

					for (j = 0; j < chan_count; j++) {
						band[index + j] = 1;
#ifdef CONFIG_SCSC_WLAN_DEBUG
						buf_pos += snprintf(supported_channels_buffer + buf_pos,
								    buf_len - buf_pos, "%d ", (chan_start + k));
						k = k + increment;
#endif
					}
					sdev->enabled_channel_count += chan_count;
				}
#ifdef CONFIG_SCSC_WLAN_DEBUG
				SLSI_DBG1(sdev, SLSI_CFG80211, "Value for Supported Channels mib: %s\n",
					  supported_channels_buffer);
				kfree(supported_channels_buffer);
#endif
			}
		}

		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) /* HT enabled? */
			sdev->fw_ht_enabled = values[mib_index].u.boolValue;
		else
			SLSI_WARN(sdev, "Error reading HT enabled mib\n");
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE)  /* VHT enabled? */
			sdev->fw_vht_enabled = values[mib_index].u.boolValue;
		else
			SLSI_WARN(sdev, "Error reading VHT enabled mib\n");
		if (values[++mib_index].type == SLSI_MIB_TYPE_OCTET) { /* HT capabilities */
			if (values[mib_index].u.octetValue.dataLength >= 4)
				memcpy(&sdev->fw_ht_cap, values[mib_index].u.octetValue.data, 4);
			else
				SLSI_WARN(sdev, "Error reading HT capabilities\n");
		} else {
			SLSI_WARN(sdev, "Error reading HT capabilities\n");
		}
		if (values[++mib_index].type == SLSI_MIB_TYPE_OCTET) { /* VHT capabilities */
			if (values[mib_index].u.octetValue.dataLength >= 4)
				memcpy(&sdev->fw_vht_cap, values[mib_index].u.octetValue.data, 4);
			else
				SLSI_WARN(sdev, "Error reading VHT capabilities\n");
		} else {
			SLSI_WARN(sdev, "Error reading VHT capabilities\n");
		}
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {    /* HARDWARE_PLATFORM */
			SLSI_CHECK_TYPE(sdev, values[mib_index].type, SLSI_MIB_TYPE_UINT);
			sdev->plat_info_mib.plat_build = values[mib_index].u.uintValue;
		} else {
			SLSI_WARN(sdev, "Error reading Hardware platform\n");
		}
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {    /* REG_DOM_VERSION */
			SLSI_CHECK_TYPE(sdev, values[mib_index].type, SLSI_MIB_TYPE_UINT);
			sdev->reg_dom_version = values[mib_index].u.uintValue;
		} else {
			SLSI_WARN(sdev, "Error reading Reg domain version\n");
		}

		/* NAN enabled? */
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {
			sdev->nan_enabled = values[mib_index].u.boolValue;
		} else {
			sdev->nan_enabled = false;
			SLSI_WARN(sdev, "Error reading NAN enabled mib\n");
		}
		SLSI_DBG1(sdev, SLSI_CFG80211, "Value for NAN enabled mib : %d\n", sdev->nan_enabled);

		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) { /* UnifiDefaultDwellTime */
			SLSI_CHECK_TYPE(sdev, values[mib_index].type, SLSI_MIB_TYPE_UINT);
			sdev->fw_dwell_time = (values[mib_index].u.uintValue) * 1024; /* Conveting TU to Microseconds */
		} else {
			SLSI_WARN(sdev, "Error reading UnifiForcedScheduleDuration\n");
		}

#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
		if (values[++mib_index].type == SLSI_MIB_TYPE_OCTET) {  /* 5Ghz Allowed Channels */
			if (values[mib_index].u.octetValue.dataLength >= 8) {
				memcpy(&sdev->wifi_sharing_5ghz_channel, values[mib_index].u.octetValue.data, 8);
				slsi_extract_valid_wifi_sharing_channels(sdev);
			} else {
				SLSI_WARN(sdev, "Error reading 5Ghz Allowed Channels\n");
			}
		} else {
			SLSI_WARN(sdev, "Error reading 5Ghz Allowed Channels\n");
		}
#endif

#ifdef CONFIG_SCSC_WLAN_AP_INFO_FILE
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) /* Dual band concurrency */
			sdev->dualband_concurrency = values[mib_index].u.boolValue;
		else
			SLSI_WARN(sdev, "Error reading dual band concurrency\n");
		if (values[++mib_index].type == SLSI_MIB_TYPE_UINT) /* max client for soft AP */
			sdev->softap_max_client = values[mib_index].u.uintValue;
		else
			SLSI_WARN(sdev, "Error reading SoftAP max client\n");
#endif
#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE)  /* Mac Randomization enable? */
			sdev->fw_mac_randomization_enabled = values[mib_index].u.boolValue;
		else
			SLSI_WARN(sdev, "Error reading Mac Randomization Support\n");
#endif
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {  /* Disable ch12/ch13 */
			sdev->device_config.disable_ch12_ch13 = values[mib_index].u.boolValue;
			SLSI_DBG1(sdev, SLSI_CFG80211, "Value for default country without ch12/13 mib: %d\n",
				  sdev->device_config.disable_ch12_ch13);
		} else {
			SLSI_WARN(sdev, "Error reading default country without ch12/13 mib\n");
		}
#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE)  /* Enhanced Arp Detect Support */
			sdev->device_config.fw_enhanced_arp_detect_supported = values[mib_index].u.boolValue;
		else
			SLSI_DBG2(sdev, SLSI_MLME, "Enhanced Arp Detect is disabled!\n");
#endif
#ifdef CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL
		/* Max ARP support in FW */
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[mib_index].type, SLSI_MIB_TYPE_UINT);
			sdev->fw_max_arp_count = values[mib_index].u.uintValue;
			if (sdev->fw_max_arp_count <= SLSI_ARP_UNPAUSE_THRESHOLD) {
				SLSI_INFO(sdev,
					  "qlen:%d less. NO ArpFlowControl\n",
					  sdev->fw_max_arp_count);
				sdev->fw_max_arp_count = 0;
			}
		} else {
			sdev->fw_max_arp_count = 0;
			SLSI_DBG3(sdev, SLSI_MLME,
				  "NO ARP flow control support in FW\n");
		}
#endif
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE)  /* APF Support */
			sdev->device_config.fw_apf_supported = values[mib_index].u.boolValue;
		else
			SLSI_DBG2(sdev, SLSI_MLME, "APF Support is disabled!\n");

		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE)  /* 40MHz for Soft AP */
			sdev->fw_SoftAp_2g_40mhz_enabled = values[mib_index].u.boolValue;
		else
			SLSI_DBG2(sdev, SLSI_MLME, "40MHz for Soft AP is disabled!\n");
		/* Num of Antenna */
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[mib_index].type, SLSI_MIB_TYPE_UINT);
			num_antenna = values[mib_index].u.uintValue;
			antenna_lw_bit = num_antenna & 0xff;
			antenna_hg_bit = ((num_antenna >> 8) & 0xff);
			sdev->lls_num_radio = (antenna_lw_bit > antenna_hg_bit) ? antenna_lw_bit : antenna_hg_bit;
			SLSI_DBG2(sdev, SLSI_MLME, "sdev->lls_num_radio = %d\n", sdev->lls_num_radio);
		} else
			SLSI_DBG2(sdev, SLSI_MLME, "Failed to read number of antennas\n");
		if (values[++mib_index].type != SLSI_MIB_TYPE_NONE) {
			sdev->fw_ext_cap_ie_len = values[mib_index].u.octetValue.dataLength;
			memset(sdev->fw_ext_cap_ie, 0, sizeof(sdev->fw_ext_cap_ie));
			memcpy(sdev->fw_ext_cap_ie, values[mib_index].u.octetValue.data,
					sdev->fw_ext_cap_ie_len);
		} else
			SLSI_DBG2(sdev, SLSI_MLME, "Failed to read Extended capabilities\n");

		kfree(values);
	}
	kfree(mibrsp.data);

	return r;
}

int slsi_set_mib_roam(struct slsi_dev *dev, struct net_device *ndev, u16 psid, int value)
{
	struct slsi_mib_data mib_data = { 0, NULL };
	int error = SLSI_MIB_STATUS_FAILURE;

	if (slsi_mib_encode_int(&mib_data, psid, value, 0) == SLSI_MIB_STATUS_SUCCESS)
		if (mib_data.dataLength) {
			error = slsi_mlme_set(dev, ndev, mib_data.data, mib_data.dataLength);
			if (error)
				SLSI_ERR(dev, "Err Setting MIB failed. error = %d\n", error);
			kfree(mib_data.data);
		}

	return error;
}

#ifdef CONFIG_SCSC_WLAN_SET_PREFERRED_ANTENNA
int slsi_set_mib_preferred_antenna(struct slsi_dev *dev, u16 value)
{
	struct slsi_mib_data mib_data = { 0, NULL };
	int error = SLSI_MIB_STATUS_FAILURE;

	if (slsi_mib_encode_uint(&mib_data, SLSI_PSID_UNIFI_PREFERRED_ANTENNA_BITMAP,
				 value, 0) == SLSI_MIB_STATUS_SUCCESS)
		if (mib_data.dataLength) {
			error = slsi_mlme_set(dev, NULL, mib_data.data, mib_data.dataLength);
			if (error)
				SLSI_ERR(dev, "Err Setting MIB failed. error = %d\n", error);
			kfree(mib_data.data);
		}

	return error;
}
#endif

void slsi_reset_throughput_stats(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_mib_data mib_data = { 0, NULL };
	int error = SLSI_MIB_STATUS_FAILURE;

	if (slsi_mib_encode_int(&mib_data, SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, 0, 0) == SLSI_MIB_STATUS_SUCCESS)
		if (mib_data.dataLength) {
			error = slsi_mlme_set(sdev, dev, mib_data.data, mib_data.dataLength);
			if (error)
				SLSI_ERR(sdev, "Err Setting MIB failed. error = %d\n", error);
			kfree(mib_data.data);
		}
}

int slsi_get_mib_roam(struct slsi_dev *sdev, u16 psid, int *mib_value)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	int rx_len = 0;
	int r;
	struct slsi_mib_get_entry get_values[] = { { psid, { 0, 0 } } };

	r = slsi_mib_encode_get_list(&mibreq, sizeof(get_values) / sizeof(struct slsi_mib_get_entry), get_values);
	if (r != SLSI_MIB_STATUS_SUCCESS)
		return -ENOMEM;

	mibrsp.dataLength = 64;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, NULL, mibreq.data, mibreq.dataLength, mibrsp.data, mibrsp.dataLength, &rx_len);
	kfree(mibreq.data);

	if (r == 0) {
		struct slsi_mib_value *values;

		mibrsp.dataLength = (u32)rx_len;

		values = slsi_mib_decode_get_list(&mibrsp, sizeof(get_values) / sizeof(struct slsi_mib_get_entry), get_values);

		if (!values) {
			kfree(mibrsp.data);
			return -EINVAL;
		}

		WARN_ON(values[0].type == SLSI_MIB_TYPE_OCTET ||
			values[0].type == SLSI_MIB_TYPE_NONE);

		if (values[0].type == SLSI_MIB_TYPE_INT)
			*mib_value = (int)(values->u.intValue);
		else if (values[0].type == SLSI_MIB_TYPE_UINT)
			*mib_value = (int)(values->u.uintValue);
		else if (values[0].type == SLSI_MIB_TYPE_BOOL)
			*mib_value = (int)(values->u.boolValue);

		SLSI_DBG2(sdev, SLSI_MLME, "MIB value = %d\n", *mib_value);
		kfree(values);
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
	}

	kfree(mibrsp.data);
	return r;
}

#ifdef CONFIG_SCSC_WLAN_GSCAN_ENABLE
int slsi_mib_get_gscan_cap(struct slsi_dev *sdev, struct slsi_nl_gscan_capabilities *cap)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	int rx_len = 0;
	int r;
	static const struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_GOOGLE_MAX_NUMBER_OF_PERIODIC_SCANS, { 0, 0 } },
							       { SLSI_PSID_UNIFI_GOOGLE_MAX_RSSI_SAMPLE_SIZE,            { 0, 0 } },
							       { SLSI_PSID_UNIFI_GOOGLE_MAX_HOTLIST_APS,       { 0, 0 } },
							       { SLSI_PSID_UNIFI_GOOGLE_MAX_SIGNIFICANT_WIFI_CHANGE_APS, { 0, 0 } },
							       { SLSI_PSID_UNIFI_GOOGLE_MAX_BSSID_HISTORY_ENTRIES, { 0, 0 } },};

	r = slsi_mib_encode_get_list(&mibreq, sizeof(get_values) / sizeof(struct slsi_mib_get_entry), get_values);
	if (r != SLSI_MIB_STATUS_SUCCESS)
		return -ENOMEM;

	mibrsp.dataLength = 64;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, NULL, mibreq.data, mibreq.dataLength, mibrsp.data, mibrsp.dataLength, &rx_len);
	kfree(mibreq.data);

	if (r == 0) {
		struct slsi_mib_value *values;

		mibrsp.dataLength = (u32)rx_len;

		values = slsi_mib_decode_get_list(&mibrsp, sizeof(get_values) / sizeof(struct slsi_mib_get_entry), get_values);

		if (!values) {
			kfree(mibrsp.data);
			return -EINVAL;
		}

		if (values[0].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[0].type, SLSI_MIB_TYPE_UINT);
			cap->max_scan_buckets = values[0].u.uintValue;
		}

		if (values[1].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[1].type, SLSI_MIB_TYPE_UINT);
			cap->max_rssi_sample_size = values[1].u.uintValue;
		}

		if (values[2].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[2].type, SLSI_MIB_TYPE_UINT);
			cap->max_hotlist_aps = values[2].u.uintValue;
		}

		if (values[3].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[3].type, SLSI_MIB_TYPE_UINT);
			cap->max_significant_wifi_change_aps = values[3].u.uintValue;
		}

		if (values[4].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[4].type, SLSI_MIB_TYPE_UINT);
			cap->max_bssid_history_entries = values[4].u.uintValue;
		}

		kfree(values);
	}
	kfree(mibrsp.data);
	return r;
}
#endif

int slsi_mib_get_apf_cap(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mib_data                   mibreq = { 0, NULL };
	struct slsi_mib_data                   mibrsp = { 0, NULL };
	struct slsi_mib_value                  *values = NULL;
	int                                    data_length = 0;
	int                                    r = 0;
	static const struct slsi_mib_get_entry get_values[] = {
		{ SLSI_PSID_UNIFI_APF_VERSION, { 0, 0 } },         /* to get the supported APF version*/
		{ SLSI_PSID_UNIFI_APF_MAX_SIZE, { 0, 0 } }         /* to get APF_MAX_SIZE*/
	};

	r = slsi_mib_encode_get_list(&mibreq, (sizeof(get_values) / sizeof(struct slsi_mib_get_entry)),
				     get_values);
	if (r != SLSI_MIB_STATUS_SUCCESS)
		return -ENOMEM;

	/* 15*2 bytes for 2 Mib's */
	mibrsp.dataLength = 30;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_NET_DBG1(dev, SLSI_MLME, "failed to allocate memory\n");
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, dev, mibreq.data, mibreq.dataLength, mibrsp.data,
			  mibrsp.dataLength, &data_length);
	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = (u32)data_length;
		values = slsi_mib_decode_get_list(&mibrsp,
						  (sizeof(get_values) / sizeof(struct slsi_mib_get_entry)), get_values);
		if (!values) {
			SLSI_NET_DBG1(dev, SLSI_MLME, "mib decode list failed\n");
			kfree(mibrsp.data);
			return -ENOMEM;
		}

		if (values[0].type == SLSI_MIB_TYPE_UINT)
			sdev->device_config.apf_cap.version = values[0].u.uintValue; /* supported APF version */
		else
			SLSI_ERR(sdev, "invalid type. index:%d\n", 0);
		if (values[1].type == SLSI_MIB_TYPE_UINT)
			sdev->device_config.apf_cap.max_length = values[1].u.uintValue; /* APF_MAX_LENGTH */
		else
			SLSI_ERR(sdev, "invalid type. index:%d\n", 1);
	} else {
		SLSI_NET_DBG1(dev, SLSI_MLME, "mlme_get_req failed(result:0x%4x)\n", r);
	}

	kfree(mibrsp.data);
	kfree(values);
	return r;
}

int slsi_mib_get_rtt_cap(struct slsi_dev *sdev, struct net_device *dev, struct slsi_rtt_capabilities *cap)
{
	struct slsi_mib_data supported_rtt_capab = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_RTT_CAPABILITIES, { 0, 0 } } };

	mibrsp.dataLength = 64;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		kfree(mibrsp.data);
		return -ENOMEM;
	}

	values = slsi_read_mibs(sdev, dev, get_values, 1, &mibrsp);
	if (!values) {
		kfree(mibrsp.data);
		return -EINVAL;
	}

	if (values[0].type != SLSI_MIB_TYPE_OCTET) {
		SLSI_ERR(sdev, "Invalid type (%d) for SLSI_PSID_UNIFI_RTT_CAPABILITIES", values[0].type);
		kfree(mibrsp.data);
		kfree(values);
		return -EINVAL;
	}
	supported_rtt_capab = values[0].u.octetValue;
	if (!supported_rtt_capab.data) {
		kfree(mibrsp.data);
		kfree(values);
		return -EINVAL;
	}
	cap->rtt_one_sided_supported = supported_rtt_capab.data[0];
	cap->rtt_ftm_supported = supported_rtt_capab.data[1];
	cap->lci_support = supported_rtt_capab.data[2];
	cap->lcr_support = supported_rtt_capab.data[3];
	cap->responder_supported = supported_rtt_capab.data[4];
	cap->preamble_support = supported_rtt_capab.data[5];
	cap->bw_support = supported_rtt_capab.data[6];
	cap->mc_version = supported_rtt_capab.data[7];

	kfree(values);
	kfree(mibrsp.data);
	return 0;
}

struct slsi_peer *slsi_peer_add(struct slsi_dev *sdev, struct net_device *dev, u8 *peer_address, u16 aid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = NULL;
	u16 queueset = 0;

	if (WARN_ON(!aid)) {
		SLSI_NET_ERR(dev, "Invalid aid(0) received\n");
		return NULL;
	}
	queueset = MAP_AID_TO_QS(aid);

	/* MUST only be called from the control path that has acquired the lock */
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (WARN_ON(!ndev_vif->activated))
		return NULL;

	if (!peer_address) {
		SLSI_NET_WARN(dev, "Peer without address\n");
		return NULL;
	}

	peer = slsi_get_peer_from_mac(sdev, dev, peer_address);
	if (peer) {
		if (ndev_vif->sta.tdls_enabled && (peer->queueset == 0)) {
			SLSI_NET_DBG3(dev, SLSI_CFG80211, "TDLS enabled and its station queueset\n");
		} else {
			SLSI_NET_WARN(dev, "Peer (MAC:" MACSTR ") already exists\n", MAC2STR(peer_address));
			return NULL;
		}
	}

	if (slsi_get_peer_from_qs(sdev, dev, queueset)) {
		SLSI_NET_WARN(dev, "Peer (queueset:%d) already exists\n", queueset);
		return NULL;
	}

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "%pM, aid:%d\n", peer_address, aid);

	peer = ndev_vif->peer_sta_record[queueset];
	if (!peer) {
		/* If it reaches here, something has gone wrong */
		SLSI_NET_ERR(dev, "Peer (queueset:%d) is NULL\n", queueset);
		return NULL;
	}

	peer->aid = aid;
	peer->queueset = queueset;
	SLSI_ETHER_COPY(peer->address, peer_address);
	peer->assoc_ie = NULL;
	peer->assoc_resp_ie = NULL;
	peer->is_wps = false;
	peer->connected_state = SLSI_STA_CONN_STATE_DISCONNECTED;
	/* Initialise the Station info */
	slsi_peer_reset_stats(sdev, dev, peer);
	peer->ndp_count = 1;
	ratelimit_state_init(&peer->sinfo_mib_get_rs, SLSI_SINFO_MIB_ACCESS_TIMEOUT, 0);

	if (scsc_wifi_fcq_ctrl_q_init(&peer->ctrl_q) < 0) {
		SLSI_NET_ERR(dev, "scsc_wifi_fcq_ctrl_q_init failed\n");
		return NULL;
	}

#ifdef CONFIG_SCSC_WLAN_DEBUG
	if (scsc_wifi_fcq_unicast_qset_init(dev, &peer->data_qs, peer->queueset, sdev, ndev_vif->ifnum, peer) < 0) {
#else
	if (scsc_wifi_fcq_unicast_qset_init(dev, &peer->data_qs, peer->queueset, sdev, ndev_vif->ifnum, peer) < 0) {
#endif
		SLSI_NET_ERR(dev, "scsc_wifi_fcq_unicast_qset_init failed\n");
		scsc_wifi_fcq_ctrl_q_deinit(&peer->ctrl_q);
		return NULL;
	}

	/* A peer is only valid once all the data is initialised
	 * otherwise a process could check the flag and start to read
	 * uninitialised data.
	 */

	if (ndev_vif->sta.tdls_enabled)
		ndev_vif->sta.tdls_peer_sta_records++;
	else
		ndev_vif->peer_sta_records++;

	ndev_vif->cfg80211_sinfo_generation++;
	skb_queue_head_init(&peer->buffered_frames);

	/* For TDLS this flag will be set while moving the packets from STAQ to TDLSQ */
	/* TODO: changes for moving packets is removed for now. Enable this when these data path changes go in*/
/*	if (!ndev_vif->sta.tdls_enabled)
 *		peer->valid = true;
 */
	peer->valid = true;

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "created station peer %pM AID:%d\n", peer->address, aid);
	return peer;
}

void slsi_peer_reset_stats(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_UNUSED_PARAMETER(sdev);

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "Peer:%pM\n", peer->address);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	memset(&peer->sinfo, 0x00, sizeof(peer->sinfo));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
	peer->sinfo.filled = BIT(NL80211_STA_INFO_RX_BYTES) |
			     BIT(NL80211_STA_INFO_TX_BYTES) |
			     BIT(NL80211_STA_INFO_RX_PACKETS) |
			     BIT(NL80211_STA_INFO_TX_PACKETS) |
			     BIT(NL80211_STA_INFO_RX_DROP_MISC) |
			     BIT(NL80211_STA_INFO_TX_FAILED) |
			     BIT(NL80211_STA_INFO_SIGNAL) |
			     BIT(NL80211_STA_INFO_BSS_PARAM);
#else
	peer->sinfo.filled = STATION_INFO_RX_BYTES |
			     STATION_INFO_TX_BYTES |
			     STATION_INFO_RX_PACKETS |
			     STATION_INFO_TX_PACKETS |
			     STATION_INFO_RX_DROP_MISC |
			     STATION_INFO_TX_FAILED |
			     STATION_INFO_SIGNAL |
			     STATION_INFO_BSS_PARAM;
#endif
}

void slsi_dump_stats(struct net_device *dev)
{
	SLSI_UNUSED_PARAMETER(dev);

	SLSI_INFO_NODEV("slsi_hanged_event_count: %d\n", slsi_hanged_event_count);
}

enum slsi_wlan_vendor_attr_hanged_event {
	SLSI_WLAN_VENDOR_ATTR_HANGED_EVENT_PANIC_CODE = 1,
	SLSI_WLAN_VENDOR_ATTR_HANGED_EVENT_MAX
};

#if defined(CONFIG_SCSC_WLAN_ENHANCED_BIGDATA) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)

/* Substitute NULL CHAR to user passed character */

static void slsi_substitute_null(char *s, const char c, size_t length)
{
	int i;

	for (i = 0; i < length; i++)
		if (s[i] == '\0' || s[i] == ' ' ) s[i] = c;

}

static void slsi_fill_bigdata_record(struct slsi_dev *sdev, struct scsc_hanged_record *hr, char *result, u16 scsc_panic_code, const size_t resultsz)
{
	int length = sizeof(struct scsc_hanged_record);
	int i = 0;
	char version_fw[HANGED_FW_VERSION_SIZE] = {0};
	int num_records;
	/* Define enough to_string buffer to allocate u32 and NULL char */
	char to_hex_string[HANGED_PANIC_RECORD_HEX_SZ + 1];
	scsc_fw_record_t panic_record_buf[HANGED_PANIC_RECORD_COUNT];

	/* Build hanged record information */
	SLSI_INFO(sdev, "Generating Hanged record information [%d] [%d]\n", length, resultsz);
	memset(result, '\0', resultsz);
	memset(hr, ' ', length);
	memcpy(hr->version, HANGED_PANIC_VERSION, sizeof(hr->version));
	mxman_get_fw_version(version_fw, HANGED_FW_VERSION_SIZE);
	snprintf(hr->fw_version, sizeof(hr->fw_version),"%s",version_fw);

	/* Treat ' ' as a one of seperator, so have to remove it */
	slsi_substitute_null(hr->fw_version, '_', sizeof(hr->fw_version));

	snprintf(hr->host_version, sizeof(hr->host_version),"%u.%u.%u.%u.%u", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION, SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);

	/* Format SCSC panic code, snprintf returns NULL character so strip it out by doing a memcpy */
	/* Plain copy of HANGED_OFFSET_DATA */
	memcpy(hr->offset_data, HANGED_OFFSET_DATA, sizeof(HANGED_OFFSET_DATA));
	snprintf(hr->hang_type,sizeof(hr->hang_type), "%2x", scsc_panic_code);

	/* Get HANGED_PANIC records  */
	num_records = scsc_service_get_panic_record(sdev->service, (char *)&panic_record_buf[0], HANGED_PANIC_RECORD_SIZE);
	if (num_records == 0) {
		SLSI_INFO(sdev, "scsc_service_get_panic_record failed - num_records = 0\n");
		memset(&hr->panic_record, '0', HANGED_PANIC_RECORD_SIZE);
	}
	for (i = 0; i < num_records; i++) {
		/* Format SCSC panic code, snprinft returns NULL character so strip it out by doing a memcpy */
		snprintf(to_hex_string, HANGED_PANIC_RECORD_HEX_SZ + 1, HANGED_PANIC_REC_FORMATTING, panic_record_buf[i]);
		memcpy(&hr->panic_record[i * HANGED_PANIC_RECORD_HEX_SZ], to_hex_string, sizeof(to_hex_string));
	}

	snprintf(result, resultsz, "%s 0 0 0 0 0 0 0 0 %s|%s|%s|%s", hr->hang_type,hr->host_version,hr->fw_version, hr->offset_data, hr->panic_record);

	SLSI_INFO(sdev, "Hanged record information string: %s\n", result);
}
#endif

int slsi_send_hanged_vendor_event(struct slsi_dev *sdev, u16 scsc_panic_code)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	struct sk_buff *skb;
#if defined(CONFIG_SCSC_WLAN_ENHANCED_BIGDATA) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
	struct scsc_hanged_record hr;
	char *result;
	int length = sizeof(struct scsc_hanged_record);

	result = kmalloc(length, GFP_KERNEL);
	if (!result) {
		SLSI_ERR_NODEV("Failed to allocate result for vendor hanged event");
		return -ENOMEM;
	}
	slsi_fill_bigdata_record(sdev, &hr, result, scsc_panic_code, length);
#else
	int length = sizeof(scsc_panic_code);
#endif

	slsi_hanged_event_count++;
	SLSI_INFO(sdev, "Sending SLSI_NL80211_VENDOR_HANGED_EVENT , count: %d, reason =0x%2x\n", slsi_hanged_event_count, scsc_panic_code);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, NULL, length, SLSI_NL80211_VENDOR_HANGED_EVENT, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, length, SLSI_NL80211_VENDOR_HANGED_EVENT, GFP_KERNEL);
#endif
	if (!skb) {
		SLSI_ERR_NODEV("Failed to allocate SKB for vendor hanged event");
#if defined(CONFIG_SCSC_WLAN_ENHANCED_BIGDATA) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)

		kfree(result);
#endif
		return -ENOMEM;
	}

#if defined(CONFIG_SCSC_WLAN_ENHANCED_BIGDATA) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
	if (nla_put(skb, SLSI_WLAN_VENDOR_ATTR_HANGED_EVENT_PANIC_CODE, length, result)) {
		kfree(result);
#else
	if (nla_put(skb, SLSI_WLAN_VENDOR_ATTR_HANGED_EVENT_PANIC_CODE, length, &scsc_panic_code)) {
#endif
		SLSI_ERR_NODEV("Failed nla_put for panic code\n");
		kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);

#if defined(CONFIG_SCSC_WLAN_ENHANCED_BIGDATA) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
	kfree(result);
#endif

#endif
	return 0;
}

#if defined(CONFIG_SLSI_WLAN_STA_FWD_BEACON) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
int slsi_send_forward_beacon_vendor_event(struct slsi_dev *sdev, const u8 *ssid, const int ssid_len, const u8 *bssid,
					  u8 channel, const u16 beacon_int, const u64 timestamp, const u64 sys_time)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	struct sk_buff *skb;
	u8 err = 0;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;

	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(dev);

	SLSI_DBG2(sdev, SLSI_CFG80211, "Sending SLSI_NL80211_VENDOR_FORWARD_BEACON\n");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, &ndev_vif->wdev, NLMSG_DEFAULT_SIZE,
					  SLSI_NL80211_VENDOR_FORWARD_BEACON, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, NLMSG_DEFAULT_SIZE,
					  SLSI_NL80211_VENDOR_FORWARD_BEACON, GFP_KERNEL);
#endif
	if (!skb) {
		SLSI_ERR_NODEV("Failed to allocate SKB for vendor forward_beacon event\n");
		return -ENOMEM;
	}

	err |= nla_put(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_SSID, ssid_len, ssid);
	err |= nla_put(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_BSSID, ETH_ALEN, bssid);
	err |= nla_put_u8(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_CHANNEL, channel);
	err |= nla_put_u16(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_BCN_INTERVAL, beacon_int);
	err |= nla_put_u32(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_TIME_STAMP1, (timestamp & 0x00000000FFFFFFFF));
	err |= nla_put_u32(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_TIME_STAMP2,
			   ((timestamp >> 32) & 0x00000000FFFFFFFF));
	err |= nla_put_u64_64bit(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_SYS_TIME, sys_time, 0);

	if (err) {
		SLSI_ERR_NODEV("Failed nla_put for forward_beacon\n");
		kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);

#endif
	return 0;
}

int slsi_send_forward_beacon_abort_vendor_event(struct slsi_dev *sdev, u16 reason_code)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	struct sk_buff *skb;
	u8 err = 0;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;

	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(dev);

	SLSI_INFO(sdev, "Sending SLSI_NL80211_VENDOR_FORWARD_BEACON_ABORT\n");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, &ndev_vif->wdev, sizeof(reason_code),
					  SLSI_NL80211_VENDOR_FORWARD_BEACON_ABORT, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, sizeof(reason_code),
					  SLSI_NL80211_VENDOR_FORWARD_BEACON_ABORT, GFP_KERNEL);
#endif
	if (!skb) {
		SLSI_ERR_NODEV("Failed to allocate SKB for vendor forward_beacon_abort event\n");
		return -ENOMEM;
	}

	err = nla_put_u16(skb, SLSI_WLAN_VENDOR_ATTR_FORWARD_BEACON_ABORT, reason_code);

	if (err) {
		SLSI_ERR_NODEV("Failed nla_put for beacon_recv_abort\n");
		kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
#endif
	return 0;
}
#endif

#ifdef CONFIG_SCSC_WLAN_HANG_TEST
int slsi_test_send_hanged_vendor_event(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_INFO(ndev_vif->sdev, "Test FORCE HANG\n");
	return slsi_send_hanged_vendor_event(ndev_vif->sdev, SCSC_PANIC_CODE_HOST << 15);
}
#endif

int slsi_set_ext_cap(struct slsi_dev *sdev, struct net_device *dev, const u8 *ies, int ie_len, const u8 *ext_cap_mask)
{
	const u8              *ext_capab_ie;
	int                   r = 0;

	ext_capab_ie = cfg80211_find_ie(WLAN_EID_EXT_CAPABILITY, ies, ie_len);
	if (ext_capab_ie) {
		u8 ext_cap_ie_len = ext_capab_ie[1];
		int i = 0;
		bool set_ext_cap = false;

		ext_capab_ie += 2; /* skip the EID and length*/
		for (i = 0; i < ext_cap_ie_len; i++) {
			/* Checking Supplicant's extended capability BITS with driver advertised mask.
			 */
			if ((~ext_cap_mask[i] & ext_capab_ie[i]) && !(~ext_cap_mask[i] & sdev->fw_ext_cap_ie[i])) {
				set_ext_cap = true;
				sdev->fw_ext_cap_ie[i] = sdev->fw_ext_cap_ie[i] | ext_capab_ie[i];
			}
		}
		if (set_ext_cap)
			r = slsi_mlme_set_ext_capab(sdev, dev, sdev->fw_ext_cap_ie, sdev->fw_ext_cap_ie_len);
	}
	return r;
}

static bool slsi_search_ies_for_qos_indicators(struct slsi_dev *sdev, u8 *ies, int ies_len)
{
	SLSI_UNUSED_PARAMETER(sdev);

	if (cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, ies, ies_len)) {
		SLSI_DBG1(sdev, SLSI_CFG80211, "QOS enabled due to WLAN_EID_HT_CAPABILITY\n");
		return true;
	}
	if (cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY, ies, ies_len)) {
		SLSI_DBG1(sdev, SLSI_CFG80211, "QOS enabled due to WLAN_EID_VHT_CAPABILITY\n");
		return true;
	}
	if (cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WMM, ies, ies_len)) {
		SLSI_DBG1(sdev, SLSI_CFG80211, "QOS enabled due to WLAN_OUI_TYPE_MICROSOFT_WMM\n");
		return true;
	}
	return false;
}

void slsi_peer_update_assoc_req(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u16 id = fapi_get_u16(skb, id);

	/* MUST only be called from the control path that has acquired the lock */
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	switch (id) {
	case MLME_CONNECTED_IND:
	case MLME_PROCEDURE_STARTED_IND:
		if (WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_AP &&
			    ndev_vif->vif_type != FAPI_VIFTYPE_STATION)) {
			kfree_skb(skb);
			return;
		}
		break;
	default:
		kfree_skb(skb);
		WARN_ON(1);
		return;
	}

	kfree_skb(peer->assoc_ie);
	peer->assoc_ie = NULL;
	peer->capabilities = 0;
	peer->sinfo.assoc_req_ies = NULL;
	peer->sinfo.assoc_req_ies_len = 0;

	if (fapi_get_datalen(skb)) {
		int mgmt_hdr_len;
		struct ieee80211_mgmt *mgmt = fapi_get_mgmt(skb);
		struct netdev_vif *ndev_vif = netdev_priv(dev);

		/* Update the skb to just point to the frame */
		skb_pull(skb, fapi_get_siglen(skb));

		if (ieee80211_is_assoc_req(mgmt->frame_control)) {
			mgmt_hdr_len = (mgmt->u.assoc_req.variable - (u8 *)mgmt);
			if (ndev_vif->vif_type == FAPI_VIFTYPE_AP)
				peer->capabilities = le16_to_cpu(mgmt->u.assoc_req.capab_info);
		} else if (ieee80211_is_reassoc_req(mgmt->frame_control)) {
			mgmt_hdr_len = (mgmt->u.reassoc_req.variable - (u8 *)mgmt);
			if (ndev_vif->vif_type == FAPI_VIFTYPE_AP)
				peer->capabilities = le16_to_cpu(mgmt->u.reassoc_req.capab_info);
		} else {
			WARN_ON(1);
			kfree_skb(skb);
			return;
		}

		skb_pull(skb, mgmt_hdr_len);

		peer->assoc_ie = skb;
		peer->sinfo.assoc_req_ies = skb->data;
		peer->sinfo.assoc_req_ies_len = skb->len;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0))
		peer->sinfo.filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
		peer->qos_enabled = slsi_search_ies_for_qos_indicators(sdev, skb->data, skb->len);
	}
}

void slsi_peer_update_assoc_rsp(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u16 id = fapi_get_u16(skb, id);

	SLSI_UNUSED_PARAMETER(sdev);

	/* MUST only be called from the control path that has acquired the lock */
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (ndev_vif->vif_type != FAPI_VIFTYPE_STATION)
		goto exit_with_warnon;

	if (id != MLME_CONNECT_IND && id != MLME_ROAMED_IND && id != MLME_REASSOCIATE_IND) {
		SLSI_NET_ERR(dev, "Unexpected id =0x%4x\n", id);
		goto exit_with_warnon;
	}

	kfree_skb(peer->assoc_resp_ie);
	peer->assoc_resp_ie = NULL;
	peer->capabilities = 0;
	if (fapi_get_datalen(skb)) {
		int mgmt_hdr_len;
		struct ieee80211_mgmt *mgmt = fapi_get_mgmt(skb);

		/* Update the skb to just point to the frame */
		skb_pull(skb, fapi_get_siglen(skb));

		if (ieee80211_is_assoc_resp(mgmt->frame_control)) {
			mgmt_hdr_len = (mgmt->u.assoc_resp.variable - (u8 *)mgmt);
			peer->capabilities = le16_to_cpu(mgmt->u.assoc_resp.capab_info);
		} else if (ieee80211_is_reassoc_resp(mgmt->frame_control)) {
			mgmt_hdr_len = (mgmt->u.reassoc_resp.variable - (u8 *)mgmt);
			peer->capabilities = le16_to_cpu(mgmt->u.reassoc_resp.capab_info);
		} else {
			goto exit_with_warnon;
		}

		skb_pull(skb, mgmt_hdr_len);
		peer->assoc_resp_ie = skb;
	}
	return;

exit_with_warnon:
	WARN_ON(1);
	kfree_skb(skb);
}

int slsi_peer_remove(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *buff_frame;

	SLSI_UNUSED_PARAMETER(sdev);

	/* MUST only be called from the control path that has acquired the lock */
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (!peer) {
		SLSI_NET_WARN(dev, "peer=NULL");
		return -EINVAL;
	}

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "%pM\n", peer->address);

	buff_frame = skb_dequeue(&peer->buffered_frames);
	while (buff_frame) {
		SLSI_NET_DBG3(dev, SLSI_MLME, "FLUSHING BUFFERED FRAMES\n");
		kfree_skb(buff_frame);
		buff_frame = skb_dequeue(&peer->buffered_frames);
	}

	/* If TCP packet is enqueued in BA reordering buffer,
	 * this tcp packet can trigger another transmission by tcp_send_ack.
	 * tcp_send_ack eventually calls slsi_net_select_queue which requires peer_lock and causes deadlock on peer_lock.
	 */
	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	slsi_rx_ba_stop_all(dev, peer);
	slsi_spinlock_lock(&ndev_vif->peer_lock);

	/* The information is no longer valid so first update the flag to ensure that
	 * another process doesn't try to use it any more.
	 */
	peer->valid = false;
	peer->is_wps = false;
	peer->connected_state = SLSI_STA_CONN_STATE_DISCONNECTED;

	if (slsi_is_tdls_peer(dev, peer))
		ndev_vif->sta.tdls_peer_sta_records--;
	else
		ndev_vif->peer_sta_records--;

	ndev_vif->cfg80211_sinfo_generation++;

	scsc_wifi_fcq_qset_deinit(dev, &peer->data_qs, sdev, ndev_vif->ifnum, peer);
	scsc_wifi_fcq_ctrl_q_deinit(&peer->ctrl_q);

	kfree_skb(peer->assoc_ie);
	kfree_skb(peer->assoc_resp_ie);
	memset(peer, 0x00, sizeof(*peer));

	return 0;
}

int slsi_vif_activated(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_UNUSED_PARAMETER(sdev);

	/* MUST only be called from the control path that has acquired the lock */
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	/* MUST have cleared any peer records previously */
	WARN_ON(ndev_vif->peer_sta_records);

	if (WARN_ON(ndev_vif->activated))
		return -EALREADY;

	if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		/* Enable the Multicast queue set for AP mode */
		if (scsc_wifi_fcq_multicast_qset_init(dev, &ndev_vif->ap.group_data_qs, sdev, ndev_vif->ifnum) < 0)
			return -EFAULT;

#ifdef CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
		kfree(ndev_vif->ap.acl_data_blacklist);
		ndev_vif->ap.acl_data_blacklist = NULL;
#endif
	}

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		/* MUST have cleared any tdls peer records previously */
		WARN_ON(ndev_vif->sta.tdls_peer_sta_records);

		ndev_vif->sta.tdls_peer_sta_records = 0;
		ndev_vif->sta.tdls_enabled = false;
		ndev_vif->sta.roam_in_progress = false;
		ndev_vif->sta.nd_offload_enabled = true;

		memset(ndev_vif->sta.keepalive_host_tag, 0, sizeof(ndev_vif->sta.keepalive_host_tag));
	}

	ndev_vif->cfg80211_sinfo_generation = 0;
	ndev_vif->peer_sta_records = 0;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
	ndev_vif->set_tid_attr.mode = SLSI_NETIF_SET_TID_OFF;
	ndev_vif->set_tid_attr.uid = 0;
	ndev_vif->set_tid_attr.tid = 0;
	ndev_vif->activated = true;
	return 0;
}

void slsi_vif_deactivated(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif           = netdev_priv(dev);
	int               i                   = 0;

	/* MUST only be called from the control path that has acquired the lock */
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	/* The station type VIF is deactivated when the AP connection is lost */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		ndev_vif->sta.group_key_set = false;
		ndev_vif->sta.wep_key_set = false;
		ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
		memset(ndev_vif->sta.keepalive_host_tag, 0, sizeof(ndev_vif->sta.keepalive_host_tag));

		/* delete the TSPEC entries (if any) if it is a STA vif */
		if (ndev_vif->iftype == NL80211_IFTYPE_STATION)
			cac_delete_tspec_list(sdev);

		if (ndev_vif->sta.tdls_enabled)
			WARN(ndev_vif->sta.tdls_peer_sta_records, "vif:%d, tdls_peer_sta_records:%d", ndev_vif->ifnum, ndev_vif->sta.tdls_peer_sta_records);

		if (ndev_vif->sta.sta_bss) {
			slsi_cfg80211_put_bss(sdev->wiphy, ndev_vif->sta.sta_bss);
			ndev_vif->sta.sta_bss = NULL;
		}
		ndev_vif->sta.tdls_enabled = false;
#if defined(CONFIG_SLSI_WLAN_STA_FWD_BEACON) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
		ndev_vif->is_wips_running = false;
#endif
#ifdef CONFIG_SCSC_WLAN_SAE_CONFIG
		if (ndev_vif->sta.rsn_ie) {
			kfree(ndev_vif->sta.rsn_ie);
			ndev_vif->sta.rsn_ie = NULL;
		}
#endif
	}

	/* MUST be done first to ensure that other code doesn't treat the VIF as still active */
	ndev_vif->activated = false;
#ifndef CONFIG_SCSC_WLAN_RX_NAPI
	skb_queue_purge(&ndev_vif->rx_data.queue);
#endif
	for (i = 0; i < (SLSI_ADHOC_PEER_CONNECTIONS_MAX); i++) {
		struct slsi_peer *peer = ndev_vif->peer_sta_record[i];

		if (peer && peer->valid) {
			if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && peer->assoc_ie)
				cfg80211_del_sta(dev, peer->address, GFP_KERNEL);

			slsi_spinlock_lock(&ndev_vif->peer_lock);
			slsi_peer_remove(sdev, dev, peer);
			slsi_spinlock_unlock(&ndev_vif->peer_lock);
		}
	}

	if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		memset(&ndev_vif->ap.last_disconnected_sta, 0, sizeof(ndev_vif->ap.last_disconnected_sta));
		scsc_wifi_fcq_qset_deinit(dev, &ndev_vif->ap.group_data_qs, sdev, ndev_vif->ifnum, NULL);
#ifdef CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
		kfree(ndev_vif->ap.acl_data_blacklist);
		ndev_vif->ap.acl_data_blacklist = NULL;
#endif
	}

	if ((ndev_vif->iftype == NL80211_IFTYPE_P2P_CLIENT) || (ndev_vif->iftype == NL80211_IFTYPE_P2P_GO)) {
		SLSI_P2P_STATE_CHANGE(sdev, P2P_IDLE_NO_VIF);
		sdev->p2p_group_exp_frame = SLSI_PA_INVALID;
	}

	/* MUST be done last as lots of code is dependent on checking the vif_type */
	ndev_vif->vif_type = SLSI_VIFTYPE_UNSPECIFIED;
	ndev_vif->set_power_mode = FAPI_POWERMANAGEMENTMODE_POWER_SAVE;
	if (slsi_is_rf_test_mode_enabled()) {
		SLSI_NET_ERR(dev, "*#rf# rf test mode set is enabled.\n");
		ndev_vif->set_power_mode = FAPI_POWERMANAGEMENTMODE_ACTIVE_MODE;
	} else {
		ndev_vif->set_power_mode = FAPI_POWERMANAGEMENTMODE_POWER_SAVE;
	}
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;

	/* SHOULD have cleared any peer records */
	WARN(ndev_vif->peer_sta_records, "vif:%d, peer_sta_records:%d", ndev_vif->ifnum, ndev_vif->peer_sta_records);

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		if (ndev_vif->sta.tdls_enabled)
			WARN(ndev_vif->sta.tdls_peer_sta_records, "vif:%d, tdls_peer_sta_records:%d",
			     ndev_vif->ifnum, ndev_vif->sta.tdls_peer_sta_records);

		if (ndev_vif->sta.sta_bss) {
			slsi_cfg80211_put_bss(sdev->wiphy, ndev_vif->sta.sta_bss);
			ndev_vif->sta.sta_bss = NULL;
		}
		ndev_vif->sta.tdls_enabled = false;
	}
}

int slsi_sta_ieee80211_mode(struct net_device *dev, u16 current_bss_channel_frequency)
{
	struct netdev_vif    *ndev_vif = netdev_priv(dev);
	const u8             *ie;

	ie = cfg80211_find_ie(WLAN_EID_VHT_OPERATION, ndev_vif->sta.sta_bss->ies->data,
			      ndev_vif->sta.sta_bss->ies->len);
	if (ie)
		return SLSI_80211_MODE_11AC;

	ie = cfg80211_find_ie(WLAN_EID_HT_OPERATION, ndev_vif->sta.sta_bss->ies->data, ndev_vif->sta.sta_bss->ies->len);
	if (ie)
		return SLSI_80211_MODE_11N;

	if (current_bss_channel_frequency > 5000)
		return  SLSI_80211_MODE_11A;

	ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, ndev_vif->sta.sta_bss->ies->data, ndev_vif->sta.sta_bss->ies->len);
	if (ie)
		return slsi_get_supported_mode(ie);
	return -EINVAL;
}

static int slsi_get_sta_mode(struct net_device *dev, const u8 *last_peer_mac)
{
	struct netdev_vif    *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer    *last_peer;
	const u8             *peer_ie;

	last_peer = slsi_get_peer_from_mac(sdev, dev, last_peer_mac);

	if (!last_peer) {
		SLSI_NET_ERR(dev, "Peer not found\n");
		return -EINVAL;
	}

	ndev_vif->ap.last_disconnected_sta.support_mode = 0;
	if (cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY, last_peer->assoc_ie->data,
			     last_peer->assoc_ie->len))
		ndev_vif->ap.last_disconnected_sta.support_mode = 3;
	else if (cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, last_peer->assoc_ie->data,
				  last_peer->assoc_ie->len))
		ndev_vif->ap.last_disconnected_sta.support_mode = 1;

	if (ndev_vif->ap.mode == SLSI_80211_MODE_11AC) { /*AP supports VHT*/
		peer_ie = cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY, last_peer->assoc_ie->data,
					   last_peer->assoc_ie->len);
		if (peer_ie)
			return SLSI_80211_MODE_11AC;

		peer_ie = cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, last_peer->assoc_ie->data,
					   last_peer->assoc_ie->len);
		if (peer_ie)
			return SLSI_80211_MODE_11N;
		return  SLSI_80211_MODE_11A;
	}
	if (ndev_vif->ap.mode == SLSI_80211_MODE_11N) { /*AP supports HT*/
		peer_ie = cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, last_peer->assoc_ie->data,
					   last_peer->assoc_ie->len);
		if (peer_ie)
			return SLSI_80211_MODE_11N;
		if (ndev_vif->ap.channel_freq > 5000)
			return SLSI_80211_MODE_11A;
		peer_ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, last_peer->assoc_ie->data,
					   last_peer->assoc_ie->len);
		if (peer_ie)
			return slsi_get_supported_mode(peer_ie);
	}

	if (ndev_vif->ap.channel_freq > 5000)
		return SLSI_80211_MODE_11A;

	if (ndev_vif->ap.mode == SLSI_80211_MODE_11G) {	/*AP supports 11g mode */
		peer_ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, last_peer->assoc_ie->data,
					   last_peer->assoc_ie->len);
		if (peer_ie)
			return slsi_get_supported_mode(peer_ie);
	}

	return SLSI_80211_MODE_11B;
}

int slsi_populate_bss_record(struct net_device *dev)
{
	struct netdev_vif        *ndev_vif = netdev_priv(dev);
	struct slsi_dev          *sdev = ndev_vif->sdev;
	struct slsi_mib_data     mibrsp = { 0, NULL };
	struct slsi_mib_value    *values = NULL;
	const u8                 *ie, *ext_capab, *rm_capab, *ext_data, *rm_data, *bss_load;
	int                      ext_capab_ie_len, rm_capab_ie_len;
	bool                     neighbor_report_bit = 0, btm = 0;
	u16                                     fw_tx_rate;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_CURRENT_BSS_CHANNEL_FREQUENCY, { 0, 0 } },
							       { SLSI_PSID_UNIFI_CURRENT_BSS_BANDWIDTH, { 0, 0 } },
							       { SLSI_PSID_UNIFI_CURRENT_BSS_NSS, {0, 0} },
							       { SLSI_PSID_UNIFI_AP_MIMO_USED, {0, 0} },
							       { SLSI_PSID_UNIFI_LAST_BSS_SNR, {0, 0} },
							       { SLSI_PSID_UNIFI_LAST_BSS_RSSI, { 0, 0 } },
							       { SLSI_PSID_UNIFI_ROAMING_COUNT, {0, 0} },
							       { SLSI_PSID_UNIFI_LAST_BSS_TX_DATA_RATE, { 0, 0 } },
							       { SLSI_PSID_UNIFI_ROAMING_AKM, {0, 0} } };

	mibrsp.dataLength = 10 * ARRAY_SIZE(get_values);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes for interface MIBs\n", mibrsp.dataLength);
		return -ENOMEM;
	}

	values = slsi_read_mibs(sdev, dev, get_values, ARRAY_SIZE(get_values), &mibrsp);

	memset(&ndev_vif->sta.last_connected_bss, 0, sizeof(ndev_vif->sta.last_connected_bss));

	if (!values) {
		SLSI_NET_DBG1(dev, SLSI_MLME, "mib decode list failed\n");
		kfree(values);
		kfree(mibrsp.data);
		return -EINVAL;
	}

	/* The Below sequence of reading the BSS Info related Mibs is very important */
	if (values[0].type != SLSI_MIB_TYPE_NONE) {    /* CURRENT_BSS_CHANNEL_FREQUENCY */
		SLSI_CHECK_TYPE(sdev, values[0].type, SLSI_MIB_TYPE_UINT);
		ndev_vif->sta.last_connected_bss.channel_freq = ((values[0].u.uintValue) / 2);
	}

	if (values[1].type != SLSI_MIB_TYPE_NONE) {    /* CURRENT_BSS_BANDWIDTH */
		SLSI_CHECK_TYPE(sdev, values[1].type, SLSI_MIB_TYPE_UINT);
		ndev_vif->sta.last_connected_bss.bandwidth = values[1].u.uintValue;
	}

	if (values[2].type != SLSI_MIB_TYPE_NONE) {    /* CURRENT_BSS_NSS */
		SLSI_CHECK_TYPE(sdev, values[2].type, SLSI_MIB_TYPE_UINT);
		ndev_vif->sta.last_connected_bss.antenna_mode = values[2].u.uintValue;
	}

	if (values[3].type != SLSI_MIB_TYPE_NONE) {    /* AP_MIMO_USED */
		SLSI_CHECK_TYPE(sdev, values[3].type, SLSI_MIB_TYPE_UINT);
		ndev_vif->sta.last_connected_bss.mimo_used = values[3].u.uintValue;
	}

	if (values[4].type != SLSI_MIB_TYPE_NONE) {    /* SNR */
		SLSI_CHECK_TYPE(sdev, values[4].type, SLSI_MIB_TYPE_UINT);
		ndev_vif->sta.last_connected_bss.snr = values[4].u.uintValue;
	}

	if (values[5].type != SLSI_MIB_TYPE_NONE) {    /* RSSI */
		SLSI_CHECK_TYPE(sdev, values[5].type, SLSI_MIB_TYPE_INT);
		ndev_vif->sta.last_connected_bss.rssi = values[5].u.intValue;
	}

	if (values[6].type != SLSI_MIB_TYPE_NONE) {    /* ROAMING_COUNT */
		SLSI_CHECK_TYPE(sdev, values[6].type, SLSI_MIB_TYPE_UINT);
		ndev_vif->sta.last_connected_bss.roaming_count = values[6].u.uintValue;
	}

	if (values[7].type != SLSI_MIB_TYPE_NONE) {    /* TX_DATA_RATE */
		SLSI_CHECK_TYPE(sdev, values[7].type, SLSI_MIB_TYPE_UINT);
		fw_tx_rate = values[7].u.uintValue;
		slsi_decode_fw_rate((u32)fw_tx_rate, NULL,
				    (unsigned long *)(&ndev_vif->sta.last_connected_bss.tx_data_rate));
	}

	if (values[8].type != SLSI_MIB_TYPE_NONE) {    /* ROAMING_AKM */
		SLSI_CHECK_TYPE(sdev, values[8].type, SLSI_MIB_TYPE_UINT);
		ndev_vif->sta.last_connected_bss.roaming_akm = values[8].u.uintValue;
	}

	kfree(values);
	kfree(mibrsp.data);

	if (!ndev_vif->sta.sta_bss) {
		SLSI_WARN(sdev, "Bss missing due to out of order msg from firmware!! Cannot collect Big Data\n");
		return -EINVAL;
	}

	SLSI_ETHER_COPY(ndev_vif->sta.last_connected_bss.address, ndev_vif->sta.sta_bss->bssid);

	ndev_vif->sta.last_connected_bss.mode = slsi_sta_ieee80211_mode(dev,
									   ndev_vif->sta.last_connected_bss.channel_freq);
	if (ndev_vif->sta.last_connected_bss.mode == -EINVAL) {
		SLSI_ERR(sdev, "slsi_get_bss_info : Supported Rates IE is null");
		return -EINVAL;
	}

	ie = cfg80211_find_vendor_ie(WLAN_OUI_WFA, SLSI_WLAN_OUI_TYPE_WFA_HS20_IND,
				     ndev_vif->sta.sta_bss->ies->data, ndev_vif->sta.sta_bss->ies->len);
	if (ie) {
		if ((ie[6] >> 4) == 0)
			ndev_vif->sta.last_connected_bss.passpoint_version = 1;
		else
			ndev_vif->sta.last_connected_bss.passpoint_version = 2;
	}

	ndev_vif->sta.last_connected_bss.noise_level = (ndev_vif->sta.last_connected_bss.rssi -
							   ndev_vif->sta.last_connected_bss.snr);

	ext_capab = cfg80211_find_ie(WLAN_EID_EXT_CAPABILITY, ndev_vif->sta.sta_bss->ies->data,
				     ndev_vif->sta.sta_bss->ies->len);
	rm_capab = cfg80211_find_ie(WLAN_EID_RRM_ENABLED_CAPABILITIES, ndev_vif->sta.sta_bss->ies->data,
				    ndev_vif->sta.sta_bss->ies->len);
	bss_load =  cfg80211_find_ie(WLAN_EID_QBSS_LOAD, ndev_vif->sta.sta_bss->ies->data,
				     ndev_vif->sta.sta_bss->ies->len);

	if (ext_capab) {
		ext_capab_ie_len = ext_capab[1];
		ext_data = &ext_capab[2];
		if ((ext_capab_ie_len >= 2) && (ext_data[1] &
		     SLSI_WLAN_EXT_CAPA1_PROXY_ARP_ENABLED))	/*check bit12 is set or not */
			ndev_vif->sta.last_connected_bss.kvie |= 1 << 1;
		if (ext_capab_ie_len >= 3) {
			if (ext_data[2] & SLSI_WLAN_EXT_CAPA2_TFS_ENABLED) /*check bit16 is set or not */
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 2;
			if (ext_data[2] & SLSI_WLAN_EXT_CAPA2_WNM_SLEEP_ENABLED)	/*check bit17 is set or not */
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 3;
			if (ext_data[2] & SLSI_WLAN_EXT_CAPA2_TIM_ENABLED)  /*check bit18 is set or not */
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 4;
			/*check bit19 is set or not */
			if (ext_data[2] & SLSI_WLAN_EXT_CAPA2_BSS_TRANSISITION_ENABLED) {
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 5;
				btm = 1;
				}
			if (ext_data[2] & SLSI_WLAN_EXT_CAPA2_DMS_ENABLED)  /*check bit20 is set or not */
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 6;
		}
	}
	if (bss_load)
		ndev_vif->sta.last_connected_bss.kvie |= 1;
	if (rm_capab) {
		rm_capab_ie_len = rm_capab[1];
		rm_data = &rm_capab[2];
		if (rm_capab_ie_len >= 1) {
			neighbor_report_bit = SLSI_WLAN_RM_CAPA0_NEIGHBOR_REPORT_ENABLED & rm_data[0];
			if (SLSI_WLAN_RM_CAPA0_LINK_MEASUREMENT_ENABLED & rm_data[0])
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 7;
			if (neighbor_report_bit)
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 8;
			if (SLSI_WLAN_RM_CAPA0_PASSIVE_MODE_ENABLED & rm_data[0])
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 9;
			if (SLSI_WLAN_RM_CAPA0_ACTIVE_MODE_ENABLED & rm_data[0])
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 10;
			if (SLSI_WLAN_RM_CAPA0_TABLE_MODE_ENABLED & rm_data[0])
				ndev_vif->sta.last_connected_bss.kvie |= 1 << 11;
		}
	}
	if (!neighbor_report_bit && !btm && !bss_load)
		ndev_vif->sta.last_connected_bss.kv = 0;
	else if (neighbor_report_bit != 0 && (!btm && !bss_load))
		ndev_vif->sta.last_connected_bss.kv = 1;		/*11k support */
	else if (!neighbor_report_bit && (btm || bss_load))
		ndev_vif->sta.last_connected_bss.kv = 2;		/*11v support */
	else
		ndev_vif->sta.last_connected_bss.kv = 3;		/*11kv support */

	ndev_vif->sta.last_connected_bss.ssid_len = ndev_vif->sta.ssid_len;
	memcpy(ndev_vif->sta.last_connected_bss.ssid, ndev_vif->sta.ssid, ndev_vif->sta.ssid_len);

	return 0;
}

static int slsi_fill_last_disconnected_sta_info(struct slsi_dev *sdev, struct net_device *dev,
						const u8 *last_peer_mac, const u16 reason_code)
{
	int i;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *last_peer;
	struct slsi_mib_data     mibrsp = { 0, NULL };
	struct slsi_mib_value    *values = NULL;
	u16                             fw_tx_rate;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_PEER_BANDWIDTH, { 0, 0 } },
							       { SLSI_PSID_UNIFI_CURRENT_PEER_NSS, {0, 0} },
							       { SLSI_PSID_UNIFI_PEER_RSSI, { 0, 0 } },
							       { SLSI_PSID_UNIFI_PEER_TX_DATA_RATE, { 0, 0 } } };

	SLSI_ETHER_COPY(ndev_vif->ap.last_disconnected_sta.address,
			last_peer_mac);
	ndev_vif->ap.last_disconnected_sta.reason = reason_code;
	ndev_vif->ap.last_disconnected_sta.mode = slsi_get_sta_mode(dev, last_peer_mac);
	last_peer = slsi_get_peer_from_mac(sdev, dev, last_peer_mac);
	if (!last_peer) {
		SLSI_NET_ERR(dev, "Peer not found\n");
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(get_values); i++)
		get_values[i].index[0] = last_peer->aid;

	ndev_vif->ap.last_disconnected_sta.rx_retry_packets = SLSI_DEFAULT_UNIFI_PEER_RX_RETRY_PACKETS;
	ndev_vif->ap.last_disconnected_sta.rx_bc_mc_packets = SLSI_DEFAULT_UNIFI_PEER_RX_BC_MC_PACKETS;
	ndev_vif->ap.last_disconnected_sta.capabilities = last_peer->capabilities;
	ndev_vif->ap.last_disconnected_sta.bandwidth = SLSI_DEFAULT_UNIFI_PEER_BANDWIDTH;
	ndev_vif->ap.last_disconnected_sta.antenna_mode = SLSI_DEFAULT_UNIFI_PEER_NSS;
	ndev_vif->ap.last_disconnected_sta.rssi = SLSI_DEFAULT_UNIFI_PEER_RSSI;
	ndev_vif->ap.last_disconnected_sta.tx_data_rate = SLSI_DEFAULT_UNIFI_PEER_TX_DATA_RATE;

	mibrsp.dataLength = 15 * ARRAY_SIZE(get_values);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes for interface MIBs\n", mibrsp.dataLength);
		return -ENOMEM;
	}

	values = slsi_read_mibs(sdev, dev, get_values, ARRAY_SIZE(get_values), &mibrsp);

	if (!values) {
		SLSI_NET_DBG1(dev, SLSI_MLME, "mib decode list failed\n");
		kfree(values);
		kfree(mibrsp.data);
		return -EINVAL;
	}
	if (values[0].type != SLSI_MIB_TYPE_NONE) {   /* LAST_PEER_BANDWIDTH */
		SLSI_CHECK_TYPE(sdev, values[0].type, SLSI_MIB_TYPE_INT);
		ndev_vif->ap.last_disconnected_sta.bandwidth = values[0].u.intValue;
	}

	if (values[1].type != SLSI_MIB_TYPE_NONE) {     /*LAST_PEER_NSS*/
		SLSI_CHECK_TYPE(sdev, values[1].type, SLSI_MIB_TYPE_INT);
		ndev_vif->ap.last_disconnected_sta.antenna_mode = values[1].u.intValue;
	}

	if (values[2].type != SLSI_MIB_TYPE_NONE) {    /* LAST_PEER_RSSI*/
		SLSI_CHECK_TYPE(sdev, values[2].type, SLSI_MIB_TYPE_INT);
		ndev_vif->ap.last_disconnected_sta.rssi = values[2].u.intValue;
	}

	if (values[3].type != SLSI_MIB_TYPE_NONE) {    /* LAST_PEER_TX_DATA_RATE */
		SLSI_CHECK_TYPE(sdev, values[3].type, SLSI_MIB_TYPE_UINT);
		fw_tx_rate = values[3].u.uintValue;
		slsi_decode_fw_rate((u32)fw_tx_rate, NULL,
				    (unsigned long *)&ndev_vif->ap.last_disconnected_sta.tx_data_rate);
	}

	kfree(values);
	kfree(mibrsp.data);

	return 0;
}

int slsi_handle_disconnect(struct slsi_dev *sdev, struct net_device *dev, u8 *peer_address, u16 reason,
			   u8 *disassoc_rsp_ie, u32 disassoc_rsp_ie_len)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (WARN_ON(!dev))
		goto exit;

	SLSI_NET_DBG3(dev, SLSI_MLME, "slsi_handle_disconnect(vif:%d)\n", ndev_vif->ifnum);

	/* MUST only be called from somewhere that has acquired the lock */
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (!ndev_vif->activated) {
		SLSI_NET_DBG1(dev, SLSI_MLME, "VIF not activated\n");
		goto exit;
	}

	switch (ndev_vif->vif_type) {
	case FAPI_VIFTYPE_STATION:
	{
		netif_carrier_off(dev);

		/* MLME-DISCONNECT-IND could indicate the completion of a MLME-DISCONNECT-REQ or
		 * the connection with the AP has been lost
		 */
		if (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTING) {
			if (!peer_address)
				SLSI_NET_WARN(dev, "Connection failure\n");
		} else if (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED) {
			if (reason == FAPI_REASONCODE_SYNCHRONISATION_LOSS)
				reason = 0; /*reason code to recognise beacon loss */
			else if (reason == FAPI_REASONCODE_KEEP_ALIVE_FAILURE)
				reason = WLAN_REASON_DEAUTH_LEAVING;/* Change to a standard reason code */
			else if (reason >= 0x8200 && reason <= 0x82FF)
				reason = reason & 0x00FF;

			if (ndev_vif->sta.is_wps) /* Ignore sending deauth or disassoc event to cfg80211 during WPS session */
				SLSI_NET_INFO(dev, "Ignoring Deauth notification to cfg80211 from the peer during WPS procedure\n");
			else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
					cfg80211_disconnected(dev, reason, disassoc_rsp_ie, disassoc_rsp_ie_len,
							      false, GFP_KERNEL);
#else
					cfg80211_disconnected(dev, reason, disassoc_rsp_ie, disassoc_rsp_ie_len,
							      GFP_KERNEL);
#endif
					SLSI_NET_DBG3(dev, SLSI_MLME, "Received disconnect from AP, reason = %d\n", reason);
			}
		} else if (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_DISCONNECTING) {
			/* Change keep alive and sync_loss reason code while sending to supplicant to a standard reason code */
			if (reason == FAPI_REASONCODE_KEEP_ALIVE_FAILURE ||
			    reason == FAPI_REASONCODE_SYNCHRONISATION_LOSS)
				reason = WLAN_REASON_DEAUTH_LEAVING;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
			cfg80211_disconnected(dev, reason, disassoc_rsp_ie, disassoc_rsp_ie_len, true, GFP_KERNEL);
#else
			cfg80211_disconnected(dev, reason, disassoc_rsp_ie, disassoc_rsp_ie_len, GFP_KERNEL);
#endif
			SLSI_NET_DBG3(dev, SLSI_MLME, "Completion of disconnect from AP\n");
		} else {
			/* Vif status is in erronus state.*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
			cfg80211_disconnected(dev, reason, disassoc_rsp_ie, disassoc_rsp_ie_len, false, GFP_KERNEL);
#else
			cfg80211_disconnected(dev, reason, disassoc_rsp_ie, disassoc_rsp_ie_len, GFP_KERNEL);
#endif
			SLSI_NET_WARN(dev, "disconnect in wrong state vif_status(%d)\n", ndev_vif->sta.vif_status);
		}

		ndev_vif->sta.is_wps = false;

		/* Populate bss records on incase of disconnection.
		 * For connection failure its not required.
		 */
		if (!(ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTING ||
		      ndev_vif->sta.vif_status == SLSI_VIF_STATUS_UNSPECIFIED))
			slsi_populate_bss_record(dev);

		kfree(ndev_vif->sta.assoc_req_add_info_elem);
		ndev_vif->sta.assoc_req_add_info_elem = NULL;
		ndev_vif->sta.assoc_req_add_info_elem_len = 0;

		memset(ndev_vif->sta.ssid, 0, ndev_vif->sta.ssid_len);
		memset(ndev_vif->sta.bssid, 0, ETH_ALEN);
		ndev_vif->sta.ssid_len = 0;
#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
		memset(&ndev_vif->enhanced_arp_stats, 0, sizeof(ndev_vif->enhanced_arp_stats));
		memset(ndev_vif->enhanced_arp_host_tag, 0, sizeof(ndev_vif->enhanced_arp_host_tag));
		ndev_vif->enhanced_arp_detect_enabled = false;
#endif

		SLSI_MUTEX_LOCK(sdev->device_config_mutex);
		if (sdev->device_config.legacy_roam_scan_list.n) {
			memset(&sdev->device_config.legacy_roam_scan_list, 0,
			       sizeof(struct slsi_roam_scan_channels));
		}
#ifdef CONFIG_SCSC_WLAN_WES_NCHO
		sdev->device_config.ncho_mode = 0;
		sdev->device_config.roam_scan_mode = 0;
		sdev->device_config.dfs_scan_mode = 0;
		sdev->device_config.dfs_scan_mode = 0;
		if (sdev->device_config.wes_roam_scan_list.n) {
			memset(&sdev->device_config.wes_roam_scan_list, 0, sizeof(struct slsi_roam_scan_channels));
		}
#endif
		SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
		slsi_roam_channel_cache_prune(dev, 0, ndev_vif->sta.last_connected_bss.ssid);

		if (slsi_mlme_del_vif(sdev, dev) != 0)
			SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
		slsi_vif_deactivated(sdev, dev);
		break;
	}
	case FAPI_VIFTYPE_AP:
	{
		struct slsi_peer *peer = NULL;

		peer = slsi_get_peer_from_mac(sdev, dev, peer_address);
		if (!peer) {
			SLSI_NET_DBG1(dev, SLSI_MLME, "peer NOT found by MAC address\n");
			goto exit;
		}

		SLSI_NET_DBG3(dev, SLSI_MLME, "MAC:%pM is_wps:%d Peer State = %d\n", peer_address,  peer->is_wps, peer->connected_state);
		slsi_fill_last_disconnected_sta_info(sdev, dev, peer_address, reason);
		slsi_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
		if (((peer->connected_state == SLSI_STA_CONN_STATE_CONNECTED) ||
				(peer->connected_state == SLSI_STA_CONN_STATE_DOING_KEY_CONFIG)) &&
				!(peer->is_wps))
			cfg80211_del_sta(dev, peer->address, GFP_KERNEL);

		slsi_spinlock_lock(&ndev_vif->peer_lock);
		slsi_peer_remove(sdev, dev, peer);
		slsi_spinlock_unlock(&ndev_vif->peer_lock);

		/* If last client disconnects (after WPA2 handshake) then take wakelock till group is removed
		 * to avoid possibility of delay in group removal if platform suspends at this point.
		 */
		if (ndev_vif->ap.p2p_gc_keys_set && (ndev_vif->peer_sta_records == 0)) {
			SLSI_NET_DBG2(dev, SLSI_MLME, "P2PGO - Acquire wakelock after last client disconnection\n");
			slsi_wake_lock(&sdev->wlan_wl);
		}
		break;
	}
	default:
		SLSI_NET_WARN(dev, "mlme_disconnect_ind(vif:%d, unexpected vif type:%d)\n", ndev_vif->ifnum, ndev_vif->vif_type);
		break;
	}
exit:
	return 0;
}

int slsi_ps_port_control(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer, enum slsi_sta_conn_state s)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_UNUSED_PARAMETER(sdev);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	switch (s) {
	case SLSI_STA_CONN_STATE_DISCONNECTED:
		SLSI_NET_DBG1(dev, SLSI_TX, "STA disconnected, SET : FCQ - Disabled\n");
		peer->authorized = false;
		if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && !ndev_vif->peer_sta_records)
			(void)scsc_wifi_fcq_8021x_port_state(dev, &ndev_vif->ap.group_data_qs, SCSC_WIFI_FCQ_8021x_STATE_BLOCKED);
		return scsc_wifi_fcq_8021x_port_state(dev, &peer->data_qs, SCSC_WIFI_FCQ_8021x_STATE_BLOCKED);

	case SLSI_STA_CONN_STATE_DOING_KEY_CONFIG:
		SLSI_NET_DBG1(dev, SLSI_TX, "STA doing KEY config, SET : FCQ - Disabled\n");
		peer->authorized = false;
		if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && !ndev_vif->peer_sta_records)
			(void)scsc_wifi_fcq_8021x_port_state(dev, &ndev_vif->ap.group_data_qs, SCSC_WIFI_FCQ_8021x_STATE_BLOCKED);
		return scsc_wifi_fcq_8021x_port_state(dev, &peer->data_qs, SCSC_WIFI_FCQ_8021x_STATE_BLOCKED);

	case SLSI_STA_CONN_STATE_CONNECTED:
		SLSI_NET_DBG1(dev, SLSI_TX, "STA connected, SET : FCQ - Enabled\n");
		peer->authorized = true;
		if (ndev_vif->vif_type == FAPI_VIFTYPE_AP)
			(void)scsc_wifi_fcq_8021x_port_state(dev, &ndev_vif->ap.group_data_qs, SCSC_WIFI_FCQ_8021x_STATE_OPEN);
		return scsc_wifi_fcq_8021x_port_state(dev, &peer->data_qs, SCSC_WIFI_FCQ_8021x_STATE_OPEN);

	default:
		SLSI_NET_DBG1(dev, SLSI_TX, "SET : FCQ - Disabled\n");
		peer->authorized = false;
		if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && !ndev_vif->peer_sta_records)
			(void)scsc_wifi_fcq_8021x_port_state(dev, &ndev_vif->ap.group_data_qs, SCSC_WIFI_FCQ_8021x_STATE_BLOCKED);
		return scsc_wifi_fcq_8021x_port_state(dev, &peer->data_qs, SCSC_WIFI_FCQ_8021x_STATE_BLOCKED);
	}

	return 0;
}

int slsi_set_uint_mib(struct slsi_dev *sdev, struct net_device *dev, u16 psid, int value)
{
	struct slsi_mib_data mib_data = { 0, NULL };
	int r = 0;

	SLSI_DBG2(sdev, SLSI_MLME, "UINT MIB Set Request (PSID = 0x%04X, Value = %d)\n", psid, value);

	r = slsi_mib_encode_uint(&mib_data, psid, value, 0);
	if (r == SLSI_MIB_STATUS_SUCCESS) {
		if (mib_data.dataLength) {
			r = slsi_mlme_set(sdev, dev, mib_data.data, mib_data.dataLength);
			if (r != 0)
				SLSI_ERR(sdev, "MIB (PSID = 0x%04X) set error = %d\n", psid, r);
			kfree(mib_data.data);
		}
	}
	return r;
}

int slsi_send_max_transmit_msdu_lifetime(struct slsi_dev *dev, struct net_device *ndev, u32 msdu_lifetime)
{
#ifdef CCX_MSDU_LIFETIME_MIB_NA
	struct slsi_mib_data mib_data = { 0, NULL };
	int error = 0;

	if (slsi_mib_encode_uint(&mib_data, SLSI_PSID_DOT11_MAX_TRANSMIT_MSDU_LIFETIME, msdu_lifetime, 0) == SLSI_MIB_STATUS_SUCCESS)
		if (mib_data.dataLength) {
			error = slsi_mlme_set(dev, ndev, mib_data.data, mib_data.dataLength);
			if (error)
				SLSI_ERR(dev, "Err Sending max msdu lifetime failed. error = %d\n", error);
			kfree(mib_data.data);
		}
	return error;
#endif
	/* TODO: current firmware do not have this MIB yet */
	return 0;
}

int slsi_read_max_transmit_msdu_lifetime(struct slsi_dev *dev, struct net_device *ndev, u32 *msdu_lifetime)
{
#ifdef CCX_MSDU_LIFETIME_MIB_NA
	struct slsi_mib_data mib_data = { 0, NULL };
	struct slsi_mib_data mib_res = { 0, NULL };
	struct slsi_mib_entry mib_val;
	int error = 0;
	int mib_rx_len = 0;
	size_t len;

	SLSI_UNUSED_PARAMETER(ndev);

	mib_res.dataLength = 10; /* PSID header(5) + dot11MaxReceiveLifetime 4 bytes + status(1) */
	mib_res.data = kmalloc(mib_res.dataLength, GFP_KERNEL);

	if (!mib_res.data)
		return -ENOMEM;

	slsi_mib_encode_get(&mib_data, SLSI_PSID_DOT11_MAX_TRANSMIT_MSDU_LIFETIME, 0);
	error = slsi_mlme_get(dev, NULL, mib_data.data, mib_data.dataLength,
			      mib_res.data, mib_res.dataLength, &mib_rx_len);
	kfree(mib_data.data);

	if (error) {
		SLSI_ERR(dev, "Err Reading max msdu lifetime failed. error = %d\n", error);
		kfree(mib_res.data);
		return error;
	}

	len = slsi_mib_decode(&mib_res, &mib_val);

	if (len != 8) {
		kfree(mib_res.data);
		return -EINVAL;
	}
	*msdu_lifetime = mib_val.value.u.uintValue;

	kfree(mib_res.data);

	return error;
#endif
	/* TODO: current firmware do not have this MIB yet */
	return 0;
}

void slsi_band_cfg_update(struct slsi_dev *sdev, int band)
{
	/* TODO: lock scan_mutex*/
	switch (band) {
	case SLSI_FREQ_BAND_AUTO:
		sdev->wiphy->bands[0] = sdev->device_config.band_2G;
		sdev->wiphy->bands[1] = sdev->device_config.band_5G;
		break;
	case SLSI_FREQ_BAND_5GHZ:
		sdev->wiphy->bands[0] = NULL;
		sdev->wiphy->bands[1] = sdev->device_config.band_5G;
		break;
	case SLSI_FREQ_BAND_2GHZ:
		sdev->wiphy->bands[0] = sdev->device_config.band_2G;
		sdev->wiphy->bands[1] = NULL;
		break;
	default:
		break;
	}
	wiphy_apply_custom_regulatory(sdev->wiphy, sdev->device_config.domain_info.regdomain);
	slsi_update_supported_channels_regd_flags(sdev);
}

int slsi_band_update(struct slsi_dev *sdev, int band)
{
	int i;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);

	SLSI_DBG3(sdev, SLSI_CFG80211, "supported_band:%d\n", band);

	if (band == sdev->device_config.supported_band) {
		SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
		return 0;
	}

	sdev->device_config.supported_band = band;

	slsi_band_cfg_update(sdev, band);

	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);

	/* If new band is auto(2.4GHz + 5GHz, no need to check for station connection.*/
	if (band == 0)
		return 0;

	/* If station is connected on any rejected band, disconnect the station. */
	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	for (i = 1; i < (CONFIG_SCSC_WLAN_MAX_INTERFACES + 1); i++) {
		dev = slsi_get_netdev_locked(sdev, i);
		if (!dev)
			break;
		ndev_vif = netdev_priv(dev);
		cancel_work_sync(&ndev_vif->set_multicast_filter_work);
		cancel_work_sync(&ndev_vif->update_pkt_filter_work);
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
		/**
		 * 1. vif should be activated and vif type should be station.
		 * 2. Station should be either in connecting or connected state.
		 * 3. if (new band is 5G and connection is on 2.4) or (new band is 2.4 and connection is 5)
		 * when all the above conditions are true drop the connection
		 * Do not wait for disconnect ind.
		 */
		if ((ndev_vif->activated) && (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) &&
		    (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTING || ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED) &&
		    (ndev_vif->chan->hw_value <= 14 ? band == SLSI_FREQ_BAND_5GHZ : band == SLSI_FREQ_BAND_2GHZ)) {
			int r;

			if (!ndev_vif->sta.sta_bss) {
				SLSI_ERR(sdev, "slsi_mlme_disconnect failed, sta_bss is not available\n");
				SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
				SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
				return -EINVAL;
			}

			r = slsi_mlme_disconnect(sdev, dev, ndev_vif->sta.sta_bss->bssid, WLAN_REASON_DEAUTH_LEAVING, true);
			LOG_CONDITIONALLY(r != 0, SLSI_ERR(sdev, "slsi_mlme_disconnect(" MACSTR ") failed with %d\n", MAC2STR(ndev_vif->sta.sta_bss->bssid), r));

			r = slsi_handle_disconnect(sdev, dev, ndev_vif->sta.sta_bss->bssid, 0, NULL, 0);
			LOG_CONDITIONALLY(r != 0, SLSI_ERR(sdev, "slsi_handle_disconnect(" MACSTR ") failed with %d\n", MAC2STR(ndev_vif->sta.sta_bss->bssid), r));
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	}
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);

	return 0;
}

/* This takes care to free the SKB on failure */
int slsi_send_gratuitous_arp(struct slsi_dev *sdev, struct net_device *dev)
{
	int ret = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *arp;
	struct ethhdr *ehdr;
	static const u8 arp_hdr[] = { 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01 };
	int arp_size = sizeof(arp_hdr) + ETH_ALEN + sizeof(ndev_vif->ipaddress) + ETH_ALEN + sizeof(ndev_vif->ipaddress);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "\n");

	if (!ndev_vif->ipaddress)
		return 0;

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (WARN_ON(!ndev_vif->activated))
		return -EINVAL;
	if (WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION))
		return -EINVAL;
	if (WARN_ON(ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED))
		return -EINVAL;

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "IP:%pI4\n", &ndev_vif->ipaddress);

	arp = alloc_skb(SLSI_NETIF_SKB_HEADROOM + SLSI_NETIF_SKB_TAILROOM + sizeof(struct ethhdr) + arp_size, GFP_KERNEL);
	if (!arp) {
		SLSI_WARN_NODEV("error allocating skb (len: %d)\n", SLSI_NETIF_SKB_HEADROOM + SLSI_NETIF_SKB_TAILROOM + sizeof(struct ethhdr) + arp_size);
		return -ENOMEM;
	}

	skb_reserve(arp, SLSI_NETIF_SKB_HEADROOM - SLSI_SKB_GET_ALIGNMENT_OFFSET(arp));
	/* The Ethernet header is accessed in the stack. */
	skb_reset_mac_header(arp);

	/* Ethernet Header */
	ehdr = (struct ethhdr *)skb_put(arp, sizeof(struct ethhdr));
	memset(ehdr->h_dest, 0xFF, ETH_ALEN);
	SLSI_ETHER_COPY(ehdr->h_source, dev->dev_addr);
	ehdr->h_proto = cpu_to_be16(ETH_P_ARP);

	/* Arp Data */
	memcpy(skb_put(arp, sizeof(arp_hdr)), arp_hdr, sizeof(arp_hdr));
	SLSI_ETHER_COPY(skb_put(arp, ETH_ALEN), dev->dev_addr);
	memcpy(skb_put(arp, sizeof(ndev_vif->ipaddress)), &ndev_vif->ipaddress, sizeof(ndev_vif->ipaddress));
	memset(skb_put(arp, ETH_ALEN), 0xFF, ETH_ALEN);
	memcpy(skb_put(arp, sizeof(ndev_vif->ipaddress)), &ndev_vif->ipaddress, sizeof(ndev_vif->ipaddress));

	arp->dev = dev;
	arp->protocol = ETH_P_ARP;
	arp->ip_summed = CHECKSUM_UNNECESSARY;
	arp->queue_mapping = slsi_netif_get_peer_queue(0, 0); /* Queueset 0 AC 0 */

	ret = slsi_tx_data(sdev, dev, arp);
	if (ret)
		kfree_skb(arp);

	return ret;
}

static const u8 addr_mask[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static const u8 solicited_node_addr_mask[6] = { 0x33, 0x33, 0xff, 0x00, 0x00, 0x01 };

static void slsi_create_packet_filter_element(u8                               filterid,
					      u8                               pkt_filter_mode,
					      u8                               num_pattern_desc,
					      struct slsi_mlme_pattern_desc    *pattern_desc,
					      struct slsi_mlme_pkt_filter_elem *pkt_filter_elem,
					      u8                               *pkt_filters_len)
{
	u8 pkt_filter_hdr[SLSI_PKT_FILTER_ELEM_HDR_LEN] = { 0xdd,             /* vendor ie*/
							    0x00,             /*Length to be filled*/
							    0x00, 0x16, 0x32, /*oui*/
							    0x02,
							    filterid,         /*filter id to be filled*/
							    pkt_filter_mode   /* pkt filter mode to be filled */
	};
	u8 i, pattern_desc_len = 0;

	WARN_ON(num_pattern_desc > SLSI_MAX_PATTERN_DESC);

	memcpy(pkt_filter_elem->header, pkt_filter_hdr, SLSI_PKT_FILTER_ELEM_HDR_LEN);
	pkt_filter_elem->num_pattern_desc = num_pattern_desc;

	for (i = 0; i < num_pattern_desc; i++) {
		memcpy(&pkt_filter_elem->pattern_desc[i], &pattern_desc[i], sizeof(struct slsi_mlme_pattern_desc));
		pattern_desc_len += SLSI_PKT_DESC_FIXED_LEN + (2 * pattern_desc[i].mask_length);
	}

	/*Update the length in the header*/
	pkt_filter_elem->header[1] =  SLSI_PKT_FILTER_ELEM_FIXED_LEN + pattern_desc_len;
	*pkt_filters_len += (SLSI_PKT_FILTER_ELEM_HDR_LEN + pattern_desc_len);

	SLSI_DBG3_NODEV(SLSI_MLME, "filterid=0x%x,pkt_filter_mode=0x%x,num_pattern_desc=0x%x\n",
			filterid, pkt_filter_mode, num_pattern_desc);
}

#define SLSI_SCREEN_OFF_FILTERS_COUNT 1

static int slsi_set_common_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mlme_pattern_desc pattern_desc;
	struct slsi_mlme_pkt_filter_elem pkt_filter_elem[1];
	u8 pkt_filters_len = 0, num_filters = 0;

	/*Opt out all broadcast and multicast packets (filter on I/G bit)*/
	pattern_desc.offset = 0;
	pattern_desc.mask_length = 1;
	pattern_desc.mask[0] = 0x01;
	pattern_desc.pattern[0] = 0x01;

	slsi_create_packet_filter_element(SLSI_ALL_BC_MC_FILTER_ID,
					  FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP | FAPI_PACKETFILTERMODE_OPT_OUT,
					  1, &pattern_desc, &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;
	return slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, num_filters, pkt_filter_elem);
}

int  slsi_set_arp_packet_filter(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mlme_pattern_desc pattern_desc[SLSI_MAX_PATTERN_DESC];
	int num_pattern_desc = 0;
	u8 pkt_filters_len = 0, num_filters = 0;
	struct slsi_mlme_pkt_filter_elem pkt_filter_elem[2];
	int ret;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = slsi_get_peer_from_qs(sdev, dev, SLSI_STA_PEER_QUEUESET);

	if (WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION))
		return -EINVAL;

	if (WARN_ON(!peer))
		return -EINVAL;

	if (slsi_is_proxy_arp_supported_on_ap(peer->assoc_resp_ie))
		return 0;

	/*Set the IP address while suspending as this will be used by firmware for ARP/NDP offloading*/
	slsi_mlme_set_ip_address(sdev, dev);
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	slsi_mlme_set_ipv6_address(sdev, dev);
#endif

	SLSI_NET_DBG2(dev, SLSI_MLME, "Set ARP filter\n");

	/* Opt out all ARP requests*/
	num_pattern_desc = 0;
	SET_ETHERTYPE_PATTERN_DESC(pattern_desc[num_pattern_desc], ETH_P_ARP);
	num_pattern_desc++;

	/* ARP - Request */
	pattern_desc[num_pattern_desc].offset = 0x14; /*sizeof(struct ethhdr) + offsetof(ar_op)*/
	pattern_desc[num_pattern_desc].mask_length = 2;
	pattern_desc[num_pattern_desc].mask[0] = 0xff;
	pattern_desc[num_pattern_desc].mask[1] = 0xff;
	pattern_desc[num_pattern_desc].pattern[0] = 0x00;
	pattern_desc[num_pattern_desc].pattern[1] = 0x01;
	num_pattern_desc++;

	slsi_create_packet_filter_element(SLSI_ALL_ARP_FILTER_ID,
					  FAPI_PACKETFILTERMODE_OPT_OUT | FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP,
					  num_pattern_desc, pattern_desc,
					  &pkt_filter_elem[num_filters],
					  &pkt_filters_len);
	num_filters++;

	/*Opt-in arp packet for device IP address*/
	num_pattern_desc = 0;
	SET_ETHERTYPE_PATTERN_DESC(pattern_desc[num_pattern_desc], ETH_P_ARP);
	num_pattern_desc++;

	pattern_desc[num_pattern_desc].offset = 0x26; /*filtering on Target IP Address*/
	pattern_desc[num_pattern_desc].mask_length = 4;
	memcpy(pattern_desc[num_pattern_desc].mask, addr_mask, pattern_desc[num_pattern_desc].mask_length);
	memcpy(pattern_desc[num_pattern_desc].pattern, &ndev_vif->ipaddress, pattern_desc[num_pattern_desc].mask_length);
	num_pattern_desc++;

	slsi_create_packet_filter_element(SLSI_LOCAL_ARP_FILTER_ID,
					  FAPI_PACKETFILTERMODE_OPT_IN | FAPI_PACKETFILTERMODE_OPT_IN_SLEEP,
					  num_pattern_desc, pattern_desc,
					  &pkt_filter_elem[num_filters],
					  &pkt_filters_len);
	num_filters++;

	ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, num_filters, pkt_filter_elem);
	if (ret)
		return ret;

#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	pkt_filters_len = 0;
	num_filters = 0;
	/*Opt in the multicast NS packets for Local IP address in active mode*/
	num_pattern_desc = 0;
	pattern_desc[num_pattern_desc].offset = 0; /*filtering on MAC destination Address*/
	pattern_desc[num_pattern_desc].mask_length = ETH_ALEN;
	SLSI_ETHER_COPY(pattern_desc[num_pattern_desc].mask, addr_mask);
	memcpy(pattern_desc[num_pattern_desc].pattern, solicited_node_addr_mask, 3);
	memcpy(&pattern_desc[num_pattern_desc].pattern[3], &ndev_vif->ipv6address.s6_addr[13], 3); /* last 3 bytes of IPv6 address*/
	num_pattern_desc++;

	/*filter on ethertype ARP*/
	SET_ETHERTYPE_PATTERN_DESC(pattern_desc[num_pattern_desc], 0x86DD);
	num_pattern_desc++;

	pattern_desc[num_pattern_desc].offset = 0x14; /*filtering on next header*/
	pattern_desc[num_pattern_desc].mask_length = 1;
	pattern_desc[num_pattern_desc].mask[0] = 0xff;
	pattern_desc[num_pattern_desc].pattern[0] = 0x3a;
	num_pattern_desc++;

	pattern_desc[num_pattern_desc].offset = 0x36; /*filtering on ICMP6 packet type*/
	pattern_desc[num_pattern_desc].mask_length = 1;
	pattern_desc[num_pattern_desc].mask[0] = 0xff;
	pattern_desc[num_pattern_desc].pattern[0] = 0x87; /* Neighbor Solicitation type in ICMPv6 */
	num_pattern_desc++;

	slsi_create_packet_filter_element(SLSI_LOCAL_NS_FILTER_ID, FAPI_PACKETFILTERMODE_OPT_IN,
					  num_pattern_desc, pattern_desc, &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;

	ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, num_filters, pkt_filter_elem);
	if (ret)
		return ret;
#endif

	return ret;
}

#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
int slsi_set_enhanced_pkt_filter(struct net_device *dev, char *command, int buf_len)
{
	struct netdev_vif *netdev_vif = netdev_priv(dev);
	struct slsi_dev   *sdev = netdev_vif->sdev;
	struct slsi_ioctl_args *ioctl_args = NULL;
	int ret = 0;
	int is_suspend = 0;
	int pkt_filter_enable;

	ioctl_args = slsi_get_private_command_args(command, buf_len, 1);
	SLSI_VERIFY_IOCTL_ARGS(sdev, ioctl_args);

	if (!slsi_str_to_int(ioctl_args->args[0], &pkt_filter_enable)) {
		SLSI_ERR(sdev, "Invalid string: '%s'\n", ioctl_args->args[0]);
		kfree(ioctl_args);
		return -EINVAL;
	}

	if (pkt_filter_enable != 0 && pkt_filter_enable != 1) {
		SLSI_ERR(sdev, "Invalid pkt_filter_enable value: '%s'\n", ioctl_args->args[0]);
		kfree(ioctl_args);
		return -EINVAL;
	}

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	is_suspend = sdev->device_config.user_suspend_mode;
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);

	if (is_suspend) {
		SLSI_ERR(sdev, "Host is in early suspend state.\n");
		kfree(ioctl_args);
		return -EPERM; /* set_enhanced_pkt_filter should not be called after suspend */
	}

	sdev->enhanced_pkt_filter_enabled = pkt_filter_enable;
	SLSI_INFO(sdev, "Enhanced packet filter is %s", (pkt_filter_enable ? "enabled" : "disabled"));
	kfree(ioctl_args);
	return ret;
}

static int slsi_set_opt_out_unicast_packet_filter(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mlme_pattern_desc pattern_desc;
	u8 pkt_filters_len = 0;
	int ret = 0;
	struct slsi_mlme_pkt_filter_elem pkt_filter_elem;

	/* IPv4 packet */
	pattern_desc.offset = 0; /* destination mac address*/
	pattern_desc.mask_length = ETH_ALEN;
	memset(pattern_desc.mask, 0xff, ETH_ALEN);
	memcpy(pattern_desc.pattern, dev->dev_addr, ETH_ALEN);

	slsi_create_packet_filter_element(SLSI_OPT_OUT_ALL_FILTER_ID,
					  FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP,
					  1, &pattern_desc,
					  &pkt_filter_elem, &pkt_filters_len);

	ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, 1, &pkt_filter_elem);

	return ret;
}

static int  slsi_set_opt_in_tcp4_packet_filter(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mlme_pattern_desc pattern_desc[2];
	u8 pkt_filters_len = 0;
	int ret = 0;
	struct slsi_mlme_pkt_filter_elem pkt_filter_elem;

	/* IPv4 packet */
	pattern_desc[0].offset = ETH_ALEN + ETH_ALEN; /* ethhdr->h_proto */
	pattern_desc[0].mask_length = 2;
	pattern_desc[0].mask[0] = 0xff; /* Big endian 0xffff */
	pattern_desc[0].mask[1] = 0xff;
	pattern_desc[0].pattern[0] = 0x08; /* Big endian 0x0800 */
	pattern_desc[0].pattern[1] = 0x00;

	/* dest.addr(6) + src.addr(6) + Protocol(2) = sizeof(struct ethhdr) = 14 */
	/* VER(1) + Svc(1) + TotalLen(2) + ID(2) + Flag&Fragmentation(2) + TTL(1) = 9 */
	pattern_desc[1].offset = 23; /* iphdr->protocol */
	pattern_desc[1].mask_length = 1;
	pattern_desc[1].mask[0] = 0xff;
	pattern_desc[1].pattern[0] = IPPROTO_TCP; /* 0x11 */
	slsi_create_packet_filter_element(SLSI_OPT_IN_TCP4_FILTER_ID,
					  FAPI_PACKETFILTERMODE_OPT_IN_SLEEP,
					  2,
					  pattern_desc,
					  &pkt_filter_elem,
					  &pkt_filters_len);

	ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, 1, &pkt_filter_elem);

	return ret;
}

static int  slsi_set_opt_in_tcp6_packet_filter(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mlme_pattern_desc pattern_desc[2];
	u8 pkt_filters_len = 0;
	int ret = 0;
	struct slsi_mlme_pkt_filter_elem pkt_filter_elem;

	/* IPv6 packet */
	pattern_desc[0].offset = ETH_ALEN + ETH_ALEN; /* ethhdr->h_proto */
	pattern_desc[0].mask_length = 2;
	pattern_desc[0].mask[0] = 0xff; /* Big endian 0xffff */
	pattern_desc[0].mask[1] = 0xff;
	pattern_desc[0].pattern[0] = 0x86; /* Big endian 0x86DD */
	pattern_desc[0].pattern[1] = 0xdd;

	pattern_desc[1].offset = sizeof(struct ethhdr) + 6; /*filtering on ipv6->next header*/
	pattern_desc[1].mask_length = 1;
	pattern_desc[1].mask[0] = 0xff;
	pattern_desc[1].pattern[0] = IPPROTO_TCP;

	slsi_create_packet_filter_element(SLSI_OPT_IN_TCP6_FILTER_ID,
					  FAPI_PACKETFILTERMODE_OPT_IN_SLEEP,
					  2,
					  pattern_desc,
					  &pkt_filter_elem,
					  &pkt_filters_len);

	ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, 1, &pkt_filter_elem);

	return ret;
}
#endif

int  slsi_set_multicast_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mlme_pattern_desc pattern_desc[3];
	u8 pkt_filters_len = 0, i, num_filters = 0;
	u8 num_pattern_desc = 0;
	int ret = 0;
	struct slsi_mlme_pkt_filter_elem *pkt_filter_elem = NULL;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 mc_filter_id, mc_filter_count;

	/* Multicast packets for registered multicast addresses to be opted in on screen off*/
	SLSI_NET_DBG2(dev, SLSI_MLME, "Set mc filters ,regd mc addr count =%d\n", ndev_vif->sta.regd_mc_addr_count);

	mc_filter_count = ndev_vif->sta.regd_mc_addr_count;
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	pkt_filter_elem = kmalloc(((SLSI_MC_ADDR_ENTRY_MAX + 2) * sizeof(struct slsi_mlme_pkt_filter_elem)), GFP_KERNEL);
#else
	pkt_filter_elem = kmalloc(((SLSI_MC_ADDR_ENTRY_MAX + 1) * sizeof(struct slsi_mlme_pkt_filter_elem)), GFP_KERNEL);
#endif
	if (!pkt_filter_elem) {
		SLSI_NET_ERR(dev, "ERROR Memory allocation failure\n");
		return -ENOMEM;
	}

	/* Multicast to unicast conversion IPv4 filter */
	pattern_desc[num_pattern_desc].offset = 0; /* destination mac address*/
	pattern_desc[num_pattern_desc].mask_length = ETH_ALEN;
	memset(pattern_desc[num_pattern_desc].mask, 0xff, ETH_ALEN);
	memcpy(pattern_desc[num_pattern_desc].pattern, dev->dev_addr, ETH_ALEN);
	num_pattern_desc++;

	pattern_desc[num_pattern_desc].offset = ETH_ALEN + ETH_ALEN; /* ethhdr->h_proto == IPv4 */
	pattern_desc[num_pattern_desc].mask_length = 2;
	pattern_desc[num_pattern_desc].mask[0] = 0xff; /* Big endian 0xffff */
	pattern_desc[num_pattern_desc].mask[1] = 0xff;
	pattern_desc[num_pattern_desc].pattern[0] = 0x08; /* Big endian 0x0800 */
	pattern_desc[num_pattern_desc].pattern[1] = 0x00;
	num_pattern_desc++;

	pattern_desc[num_pattern_desc].offset = sizeof(struct ethhdr) + offsetof(struct iphdr, daddr); /* iphdr->daddr starts with 1110 */
	pattern_desc[num_pattern_desc].mask_length = 1;
	pattern_desc[num_pattern_desc].mask[0] = 0xf0;
	pattern_desc[num_pattern_desc].pattern[0] = 0xe0; /* 224 */
	num_pattern_desc++;

	slsi_create_packet_filter_element(SLSI_MULTI_TO_UNICAST_IPV4_ID,
					  FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP,
					  num_pattern_desc, pattern_desc,
					  &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;

#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	num_pattern_desc = 0;

	/* Multicast to unicast conversion IPv6 filter */
	pattern_desc[num_pattern_desc].offset = 0; /* destination mac address */
	pattern_desc[num_pattern_desc].mask_length = ETH_ALEN;
	SLSI_ETHER_COPY(pattern_desc[num_pattern_desc].mask, addr_mask);
	SLSI_ETHER_COPY(pattern_desc[num_pattern_desc].pattern, dev->dev_addr);
	num_pattern_desc++;

	SET_ETHERTYPE_PATTERN_DESC(pattern_desc[num_pattern_desc], ETH_P_IPV6);
	num_pattern_desc++;

	pattern_desc[num_pattern_desc].offset = sizeof(struct ethhdr) + offsetof(struct ipv6hdr, daddr); /* ipv6hdr->daddr starts with */
	pattern_desc[num_pattern_desc].mask_length = 1;
	pattern_desc[num_pattern_desc].mask[0] = 0xff;
	pattern_desc[num_pattern_desc].pattern[0] = 0xff; /* ffxx:: */
	num_pattern_desc++;

	slsi_create_packet_filter_element(SLSI_MULTI_TO_UNICAST_IPv6_ID,
					  FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP,
					  num_pattern_desc, pattern_desc,
					  &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;
#endif

	/*Regd multicast addresses filter*/
	pattern_desc[0].offset = 0;
	pattern_desc[0].mask_length = ETH_ALEN;
	SLSI_ETHER_COPY(pattern_desc[0].mask, addr_mask);

	for (i = 0; i < mc_filter_count; i++) {
		SLSI_ETHER_COPY(pattern_desc[0].pattern, ndev_vif->sta.regd_mc_addr[i]);
		mc_filter_id = SLSI_REGD_MC_FILTER_ID + i;
#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
		if (sdev->enhanced_pkt_filter_enabled)
			slsi_create_packet_filter_element(mc_filter_id,
							  FAPI_PACKETFILTERMODE_OPT_IN,
							  1, &pattern_desc[0],
							  &pkt_filter_elem[num_filters], &pkt_filters_len);
		else
#endif
			slsi_create_packet_filter_element(mc_filter_id,
							  FAPI_PACKETFILTERMODE_OPT_IN |
							  FAPI_PACKETFILTERMODE_OPT_IN_SLEEP,
							  1, &pattern_desc[0],
							  &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
	}

	for (i = mc_filter_count; i < SLSI_MC_ADDR_ENTRY_MAX; i++) {
		mc_filter_id = SLSI_REGD_MC_FILTER_ID + i;
		slsi_create_packet_filter_element(mc_filter_id, 0, 0, NULL, &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
	}

	ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, num_filters, pkt_filter_elem);
	kfree(pkt_filter_elem);

	return ret;
}

int  slsi_clear_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = slsi_get_peer_from_qs(sdev, dev, SLSI_STA_PEER_QUEUESET);

	u8 i, pkt_filters_len = 0;
	int num_filters = 0;
	int ret = 0;
	struct slsi_mlme_pkt_filter_elem *pkt_filter_elem;
	u8 mc_filter_id;

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION))
		return -EINVAL;

	if (WARN_ON(!peer))
		return -EINVAL;

	SLSI_NET_DBG2(dev, SLSI_MLME, "Clear filters on Screen on");

	/* calculate number of filters (regd_mc_addr_count + multi_to_unicast_ipv4 filter + SLSI_SCREEN_OFF_FILTERS_COUNT) */
	num_filters = ndev_vif->sta.regd_mc_addr_count + 1 + SLSI_SCREEN_OFF_FILTERS_COUNT;
	if ((slsi_is_proxy_arp_supported_on_ap(peer->assoc_resp_ie)) == false) {
		num_filters++;
		num_filters++;
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
		num_filters++;
#endif
	}

#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
	if (sdev->enhanced_pkt_filter_enabled) {
		num_filters++; /*All OPT OUT*/
		num_filters++; /*TCP IPv4 OPT IN*/
		num_filters++; /*TCP IPv6 OPT IN*/
	}
#endif
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	num_filters++; /* MULTI_TO_UNI IPv6 OPT OUT */
#endif
	pkt_filter_elem = kmalloc((num_filters * sizeof(struct slsi_mlme_pkt_filter_elem)), GFP_KERNEL);
	if (!pkt_filter_elem) {
		SLSI_NET_ERR(dev, "ERROR Memory allocation failure");
		return -ENOMEM;
	}

	num_filters = 0;
	for (i = 0; i < ndev_vif->sta.regd_mc_addr_count; i++) {
		mc_filter_id = SLSI_REGD_MC_FILTER_ID + i;
		slsi_create_packet_filter_element(mc_filter_id, 0, 0, NULL, &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
	}
	if ((slsi_is_proxy_arp_supported_on_ap(peer->assoc_resp_ie)) == false) {
		slsi_create_packet_filter_element(SLSI_LOCAL_ARP_FILTER_ID, 0, 0, NULL, &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
		slsi_create_packet_filter_element(SLSI_ALL_ARP_FILTER_ID, 0, 0, NULL, &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
		slsi_create_packet_filter_element(SLSI_LOCAL_NS_FILTER_ID, 0, 0, NULL, &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
#endif
	}

	slsi_create_packet_filter_element(SLSI_ALL_BC_MC_FILTER_ID, 0, 0, NULL, &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;

#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
	if (sdev->enhanced_pkt_filter_enabled) {
		slsi_create_packet_filter_element(SLSI_OPT_OUT_ALL_FILTER_ID, 0, 0, NULL,
						  &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
		slsi_create_packet_filter_element(SLSI_OPT_IN_TCP4_FILTER_ID, 0, 0, NULL,
						  &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
		slsi_create_packet_filter_element(SLSI_OPT_IN_TCP6_FILTER_ID, 0, 0, NULL,
						  &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
	}
#endif
	slsi_create_packet_filter_element(SLSI_MULTI_TO_UNICAST_IPV4_ID, 0, 0, NULL,
					  &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	slsi_create_packet_filter_element(SLSI_MULTI_TO_UNICAST_IPv6_ID, 0, 0, NULL,
					  &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;
#endif
	ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, num_filters, pkt_filter_elem);
	kfree(pkt_filter_elem);
	return ret;
}

int  slsi_update_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	int ret = 0;

	struct netdev_vif *ndev_vif = netdev_priv(dev);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
	WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION);

	ret = slsi_set_multicast_packet_filters(sdev, dev);
	if (ret)
		return ret;

	ret = slsi_set_arp_packet_filter(sdev, dev);
	if (ret)
		return ret;

#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
	if (sdev->enhanced_pkt_filter_enabled) {
		ret = slsi_set_opt_out_unicast_packet_filter(sdev, dev);
		if (ret)
			return ret;
		ret = slsi_set_opt_in_tcp4_packet_filter(sdev, dev);
		if (ret)
			return ret;
		ret = slsi_set_opt_in_tcp6_packet_filter(sdev, dev);
		if (ret)
			return ret;
	}
#endif
	return slsi_set_common_packet_filters(sdev, dev);
}

#define IPV6_PF_PATTERN_MASK 0xf0
#define IPV6_PF_PATTERN 0x60

#ifdef CONFIG_SCSC_WLAN_DISABLE_NAT_KA
#define SLSI_ON_CONNECT_FILTERS_COUNT 2
#else
#define SLSI_ON_CONNECT_FILTERS_COUNT 3
#endif

void slsi_set_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	struct slsi_mlme_pattern_desc pattern_desc[SLSI_MAX_PATTERN_DESC];
	int num_pattern_desc = 0;
	u8 pkt_filters_len = 0;
	int num_filters = 0;

	struct slsi_mlme_pkt_filter_elem pkt_filter_elem[SLSI_ON_CONNECT_FILTERS_COUNT];
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = slsi_get_peer_from_qs(sdev, dev, SLSI_STA_PEER_QUEUESET);
	const u8                 *ie;

	if (WARN_ON(!ndev_vif->activated))
		return;

	if (WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION))
		return;

	if (WARN_ON(!peer))
		return;

	if (WARN_ON(!peer->assoc_resp_ie))
		return;

#ifdef CONFIG_SCSC_WLAN_BLOCK_IPV6

	/*Opt out all IPv6 packets in active and suspended mode (ipv6 filtering)*/
	num_pattern_desc = 0;
	pattern_desc[num_pattern_desc].offset = 0x0E; /*filtering on IP Protocol version*/
	pattern_desc[num_pattern_desc].mask_length = 1;
	pattern_desc[num_pattern_desc].mask[0] = IPV6_PF_PATTERN_MASK;
	pattern_desc[num_pattern_desc].pattern[0] = IPV6_PF_PATTERN;
	num_pattern_desc++;

	slsi_create_packet_filter_element(SLSI_ALL_IPV6_PKTS_FILTER_ID,
					  FAPI_PACKETFILTERMODE_OPT_OUT | FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP,
					  num_pattern_desc, pattern_desc, &pkt_filter_elem[num_filters], &pkt_filters_len);
	num_filters++;

#endif

	ie = cfg80211_find_vendor_ie(WLAN_OUI_WFA, SLSI_WLAN_OUI_TYPE_WFA_HS20_IND,
				     ndev_vif->sta.sta_bss->ies->data, ndev_vif->sta.sta_bss->ies->len);

	if (ie) {
		SLSI_NET_DBG1(dev, SLSI_CFG80211, "Connected to HS2 AP ");

		if (slsi_is_proxy_arp_supported_on_ap(peer->assoc_resp_ie)) {
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "Proxy ARP service supported on HS2 AP ");

			/* Opt out Gratuitous ARP packets (ARP Announcement) in active and suspended mode.
			 * For suspended mode, gratituous ARP is dropped by "opt out all broadcast" that will be
			 * set  in slsi_set_common_packet_filters on screen off
			 */
			num_pattern_desc = 0;
			pattern_desc[num_pattern_desc].offset = 0; /*filtering on MAC destination Address*/
			pattern_desc[num_pattern_desc].mask_length = ETH_ALEN;
			SLSI_ETHER_COPY(pattern_desc[num_pattern_desc].mask, addr_mask);
			SLSI_ETHER_COPY(pattern_desc[num_pattern_desc].pattern, addr_mask);
			num_pattern_desc++;

			SET_ETHERTYPE_PATTERN_DESC(pattern_desc[num_pattern_desc], ETH_P_ARP);
			num_pattern_desc++;

			slsi_create_packet_filter_element(SLSI_PROXY_ARP_FILTER_ID, FAPI_PACKETFILTERMODE_OPT_OUT,
							  num_pattern_desc, pattern_desc, &pkt_filter_elem[num_filters],
							  &pkt_filters_len);
			num_filters++;

#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
			/* Opt out unsolicited Neighbor Advertisement packets .For suspended mode, NA is dropped by
			 * "opt out all IPv6 multicast" already set in slsi_create_common_packet_filters
			 */

			num_pattern_desc = 0;

			pattern_desc[num_pattern_desc].offset = 0; /*filtering on MAC destination Address*/
			pattern_desc[num_pattern_desc].mask_length = ETH_ALEN;
			SLSI_ETHER_COPY(pattern_desc[num_pattern_desc].mask, addr_mask);
			SLSI_ETHER_COPY(pattern_desc[num_pattern_desc].pattern, solicited_node_addr_mask);
			num_pattern_desc++;

			SET_ETHERTYPE_PATTERN_DESC(pattern_desc[num_pattern_desc], 0x86DD);
			num_pattern_desc++;

			pattern_desc[num_pattern_desc].offset = 0x14; /*filtering on next header*/
			pattern_desc[num_pattern_desc].mask_length = 1;
			pattern_desc[num_pattern_desc].mask[0] = 0xff;
			pattern_desc[num_pattern_desc].pattern[0] = 0x3a;
			num_pattern_desc++;

			pattern_desc[num_pattern_desc].offset = 0x36; /*filtering on ICMP6 packet type*/
			pattern_desc[num_pattern_desc].mask_length = 1;
			pattern_desc[num_pattern_desc].mask[0] = 0xff;
			pattern_desc[num_pattern_desc].pattern[0] = 0x88; /* Neighbor Advertisement type in ICMPv6 */
			num_pattern_desc++;

			slsi_create_packet_filter_element(SLSI_PROXY_ARP_NA_FILTER_ID, FAPI_PACKETFILTERMODE_OPT_OUT,
							  num_pattern_desc, pattern_desc, &pkt_filter_elem[num_filters],
							  &pkt_filters_len);
			num_filters++;
#endif
		}
	}

#ifndef CONFIG_SCSC_WLAN_DISABLE_NAT_KA
	{
		const u8 nat_ka_pattern[4] = { 0x11, 0x94, 0x00, 0x09 };
		/*Opt out the NAT T for IPsec*/
		num_pattern_desc = 0;
		pattern_desc[num_pattern_desc].offset = 0x24; /*filtering on destination port number*/
		pattern_desc[num_pattern_desc].mask_length = 4;
		memcpy(pattern_desc[num_pattern_desc].mask, addr_mask, 4);
		memcpy(pattern_desc[num_pattern_desc].pattern, nat_ka_pattern, 4);
		num_pattern_desc++;

		slsi_create_packet_filter_element(SLSI_NAT_IPSEC_FILTER_ID, FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP,
						  num_pattern_desc, pattern_desc, &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
	}
#endif

	if (num_filters)
		slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, num_filters, pkt_filter_elem);
}

int slsi_ip_address_changed(struct slsi_dev *sdev, struct net_device *dev, __be32 ipaddress)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int ret = 0;

	/* Store the IP address outside the check for vif being active
	 * as we get the same notification in case of static IP
	 */
	if (ndev_vif->ipaddress != ipaddress)
		ndev_vif->ipaddress = ipaddress;

	if (ndev_vif->activated && (ndev_vif->vif_type == FAPI_VIFTYPE_AP)) {
#ifdef CONFIG_SCSC_WLAN_BLOCK_IPV6
		struct slsi_mlme_pattern_desc pattern_desc[1];
		u8 num_patterns = 0;
		struct slsi_mlme_pkt_filter_elem pkt_filter_elem[1];
		u8 pkt_filters_len = 0;
		u8 num_filters = 0;
#endif

		ndev_vif->ipaddress = ipaddress;
		ret = slsi_mlme_set_ip_address(sdev, dev);
		if (ret != 0)
			SLSI_NET_ERR(dev, "slsi_mlme_set_ip_address ERROR. ret=%d", ret);

#ifdef CONFIG_SCSC_WLAN_BLOCK_IPV6
		/* Opt out IPv6 packets in platform suspended mode */
		pattern_desc[num_patterns].offset = 0x0E;
		pattern_desc[num_patterns].mask_length = 0x01;
		pattern_desc[num_patterns].mask[0] = IPV6_PF_PATTERN_MASK;
		pattern_desc[num_patterns++].pattern[0] = IPV6_PF_PATTERN;

		slsi_create_packet_filter_element(SLSI_AP_ALL_IPV6_PKTS_FILTER_ID, FAPI_PACKETFILTERMODE_OPT_OUT_SLEEP,
						  num_patterns, pattern_desc, &pkt_filter_elem[num_filters], &pkt_filters_len);
		num_filters++;
		ret = slsi_mlme_set_packet_filter(sdev, dev, pkt_filters_len, num_filters, pkt_filter_elem);
		if (ret != 0)
			SLSI_NET_ERR(dev, "slsi_mlme_set_packet_filter (return :%d) ERROR\n", ret);
#endif
	} else if ((ndev_vif->activated) &&
		   (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) &&
		   (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED)) {
		struct slsi_peer *peer = slsi_get_peer_from_qs(sdev, dev, SLSI_STA_PEER_QUEUESET);

		if (WARN_ON(!peer))
			return -EINVAL;

		if (!(peer->capabilities & WLAN_CAPABILITY_PRIVACY) ||
		    (ndev_vif->sta.group_key_set && peer->pairwise_key_set) ||
		    ndev_vif->sta.wep_key_set)
			slsi_send_gratuitous_arp(sdev, dev);
		else
			ndev_vif->sta.gratuitous_arp_needed = true;

		slsi_mlme_powermgt(sdev, dev, ndev_vif->set_power_mode);
	}

	return ret;
}

#define SLSI_AP_AUTO_CHANLS_LIST_FROM_HOSTAPD_MAX 3

int slsi_auto_chan_select_scan(struct slsi_dev *sdev, int n_channels, struct ieee80211_channel *channels[])
{
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct sk_buff_head unique_scan_results;
	int scan_result_count[SLSI_AP_AUTO_CHANLS_LIST_FROM_HOSTAPD_MAX] = { 0, 0, 0 };
	int i, j;
	int r = 0;
	int selected_index = 0;
	int min_index = 0;
	u32 freqdiff = 0;

	if (slsi_is_test_mode_enabled()) {
		SLSI_WARN(sdev, "not supported in WlanLite mode\n");
		return -EOPNOTSUPP;
	}

	skb_queue_head_init(&unique_scan_results);

	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN); /* use the main VIF */
	if (!dev) {
		r = -EINVAL;
		return r;
	}

	ndev_vif = netdev_priv(dev);
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	if (ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req) {
		r = -EBUSY;
		goto exit_with_vif;
	}
	ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan = true;
	r = slsi_mlme_add_scan(sdev,
			       dev,
			       FAPI_SCANTYPE_AP_AUTO_CHANNEL_SELECTION,
			       FAPI_REPORTMODE_REAL_TIME,
			       0,    /* n_ssids */
			       NULL, /* ssids */
			       n_channels,
			       channels,
			       NULL,
			       NULL,                   /* ie */
			       0,                      /* ie_len */
			       ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan);

	if (r == 0) {
		struct sk_buff *unique_scan;
		struct sk_buff *scan;

		SLSI_MUTEX_LOCK(ndev_vif->scan_result_mutex);
		scan = slsi_dequeue_cached_scan_result(&ndev_vif->scan[SLSI_SCAN_HW_ID], NULL);
		while (scan) {
			struct ieee80211_mgmt *mgmt = fapi_get_mgmt(scan);
			struct ieee80211_channel *channel;

			/* make sure this BSSID has not already been used */
			skb_queue_walk(&unique_scan_results, unique_scan) {
				struct ieee80211_mgmt *unique_mgmt = fapi_get_mgmt(unique_scan);

				if (compare_ether_addr(mgmt->bssid, unique_mgmt->bssid) == 0) {
					kfree_skb(scan);
					goto next_scan;
				}
			}

			skb_queue_head(&unique_scan_results, scan);

			channel = slsi_find_scan_channel(sdev, mgmt, fapi_get_mgmtlen(scan), fapi_get_u16(scan, u.mlme_scan_ind.channel_frequency) / 2);
			if (!channel)
				goto next_scan;

			/* check for interfering channels for 1, 6 and 11 */
			for (i = 0, j = 0; i < SLSI_AP_AUTO_CHANLS_LIST_FROM_HOSTAPD_MAX && channels[j]; i++, j = j + 5) {
				if (channel->center_freq == channels[j]->center_freq) {
					SLSI_NET_DBG3(dev, SLSI_CFG80211, "exact match:%d\n", i);
					scan_result_count[i] += 5;
					goto next_scan;
				}
				freqdiff = abs((int)channel->center_freq - (channels[j]->center_freq));
				if (freqdiff <= 20) {
					SLSI_NET_DBG3(dev, SLSI_CFG80211, "overlapping:%d, freqdiff:%d\n", i, freqdiff);
					scan_result_count[i] += (5 - (freqdiff / 5));
				}
			}

next_scan:
			scan = slsi_dequeue_cached_scan_result(&ndev_vif->scan[SLSI_SCAN_HW_ID], NULL);
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->scan_result_mutex);

		/* Select the channel to use */
		for (i = 0, j = 0; i < SLSI_AP_AUTO_CHANLS_LIST_FROM_HOSTAPD_MAX; i++, j = j + 5) {
			SLSI_NET_DBG3(dev, SLSI_CFG80211, "score[%d]:%d\n", i, scan_result_count[i]);
			if (scan_result_count[i] <= scan_result_count[min_index]) {
				min_index = i;
				selected_index = j;
			}
		}
		SLSI_NET_DBG3(dev, SLSI_CFG80211, "selected:%d with score:%d\n", selected_index, scan_result_count[min_index]);

		SLSI_MUTEX_LOCK(sdev->device_config_mutex);
		sdev->device_config.ap_auto_chan = channels[selected_index]->hw_value & 0xFF;
		SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);

		SLSI_INFO(sdev, "Channel selected = %d", sdev->device_config.ap_auto_chan);
	}
	skb_queue_purge(&unique_scan_results);
	ndev_vif->scan[SLSI_SCAN_HW_ID].is_blocking_scan = false;

exit_with_vif:
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	return r;
}

int slsi_set_boost(struct slsi_dev *sdev, struct net_device *dev)
{
	int error = 0;

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	error = slsi_set_mib_rssi_boost(sdev, dev, SLSI_PSID_UNIFI_ROAM_RSSI_BOOST, 1,
					sdev->device_config.rssi_boost_2g);
	if (error)
		SLSI_ERR(sdev, "Err setting boost value For 2g after adding vif. error = %d\n", error);
	error = slsi_set_mib_rssi_boost(sdev, dev, SLSI_PSID_UNIFI_ROAM_RSSI_BOOST, 2,
					sdev->device_config.rssi_boost_5g);
	if (error)
		SLSI_ERR(sdev, "Err setting boost value for 5g after adding vif . error = %d\n", error);
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
	return error;
}

/**
 * Work to be done when ROC retention duration expires:
 * Send ROC expired event to cfg80211 and queue work to delete unsync vif after retention timeout.
 */
static void slsi_p2p_roc_duration_expiry_work(struct work_struct *work)
{
	struct netdev_vif *ndev_vif = container_of((struct delayed_work *)work, struct netdev_vif, unsync.roc_expiry_work);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	/* There can be a race condition of this work function waiting for ndev_vif->vif_mutex and meanwhile the vif is deleted (due to net_stop).
	 * In such cases ndev_vif->chan would have been cleared.
	 */
	if (ndev_vif->sdev->p2p_state == P2P_IDLE_NO_VIF) {
		SLSI_NET_DBG1(ndev_vif->wdev.netdev, SLSI_CFG80211, "P2P unsync vif is not present\n");
		goto exit;
	}

	SLSI_NET_DBG3(ndev_vif->wdev.netdev, SLSI_CFG80211, "Send ROC expired event\n");

	/* If action frame tx is in progress don't schedule work to delete vif */
	if (ndev_vif->sdev->p2p_state != P2P_ACTION_FRAME_TX_RX) {
		/* After sucessful frame transmission,  we will move to LISTENING or VIF ACTIVE state.
		 * Unset channel should not be sent down during p2p procedure.
		 */
		if (!ndev_vif->drv_in_p2p_procedure) {
			if (delayed_work_pending(&ndev_vif->unsync.unset_channel_expiry_work))
				cancel_delayed_work(&ndev_vif->unsync.unset_channel_expiry_work);
			queue_delayed_work(ndev_vif->sdev->device_wq, &ndev_vif->unsync.unset_channel_expiry_work,
					   msecs_to_jiffies(SLSI_P2P_UNSET_CHANNEL_EXTRA_MSEC));
		}
		slsi_p2p_queue_unsync_vif_del_work(ndev_vif, SLSI_P2P_UNSYNC_VIF_EXTRA_MSEC);
		SLSI_P2P_STATE_CHANGE(ndev_vif->sdev, P2P_IDLE_VIF_ACTIVE);
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	cfg80211_remain_on_channel_expired(&ndev_vif->wdev, ndev_vif->unsync.roc_cookie, ndev_vif->chan, GFP_KERNEL);
#else
	cfg80211_remain_on_channel_expired(ndev_vif->wdev.netdev, ndev_vif->unsync.roc_cookie,
					   ndev_vif->chan, ndev_vif->channel_type, GFP_KERNEL);
#endif

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

/**
 * Work to be done when unsync vif retention duration expires:
 * Delete the unsync vif.
 */
static void slsi_p2p_unsync_vif_delete_work(struct work_struct *work)
{
	struct netdev_vif *ndev_vif = container_of((struct delayed_work *)work, struct netdev_vif, unsync.del_vif_work);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_NET_DBG1(ndev_vif->wdev.netdev, SLSI_CFG80211, "Delete vif duration expired - Deactivate unsync vif\n");
	slsi_p2p_vif_deactivate(ndev_vif->sdev, ndev_vif->wdev.netdev, true);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

/**
 * Work to be done after roc expiry or cancel remain on channel:
 * Unset channel to be sent to Fw.
 */
static void slsi_p2p_unset_channel_expiry_work(struct work_struct *work)
{
	struct netdev_vif *ndev_vif = container_of((struct delayed_work *)work, struct netdev_vif,
						   unsync.unset_channel_expiry_work);
	struct slsi_dev           *sdev = ndev_vif->sdev;
	struct net_device *dev = ndev_vif->wdev.netdev;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (ndev_vif->activated) {
		SLSI_NET_DBG1(ndev_vif->wdev.netdev, SLSI_CFG80211, "Unset channel expiry work-Send Unset Channel\n");
		if (!ndev_vif->drv_in_p2p_procedure) {
			/* Supplicant has stopped FIND/LISTEN. Clear Probe Response IEs in firmware and driver */
			if (slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_PROBE_RESPONSE, NULL, 0) != 0)
				SLSI_NET_ERR(dev, "Clearing Probe Response IEs failed for unsync vif\n");
			slsi_unsync_vif_set_probe_rsp_ie(ndev_vif, NULL, 0);

			/* Send Unset Channel */
			if (ndev_vif->driver_channel != 0) {
				slsi_mlme_unset_channel_req(sdev, dev);
				ndev_vif->driver_channel = 0;
			}
		}
	} else {
		SLSI_NET_ERR(dev, "P2P vif is not activated\n");
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

/* Initializations for P2P - Change vif type to unsync, create workqueue and init work */
int slsi_p2p_init(struct slsi_dev *sdev, struct netdev_vif *ndev_vif)
{
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "Initialize P2P - Init P2P state to P2P_IDLE_NO_VIF\n");
	sdev->p2p_state = P2P_IDLE_NO_VIF;
	sdev->p2p_group_exp_frame = SLSI_PA_INVALID;

	ndev_vif->vif_type = FAPI_VIFTYPE_UNSYNCHRONISED;
	ndev_vif->unsync.slsi_p2p_continuous_fullscan = false;


	INIT_DELAYED_WORK(&ndev_vif->unsync.roc_expiry_work, slsi_p2p_roc_duration_expiry_work);
	INIT_DELAYED_WORK(&ndev_vif->unsync.del_vif_work, slsi_p2p_unsync_vif_delete_work);
	INIT_DELAYED_WORK(&ndev_vif->unsync.unset_channel_expiry_work, slsi_p2p_unset_channel_expiry_work);
	return 0;
}

/* De-initializations for P2P - Reset vif type, cancel work and destroy workqueue */
void slsi_p2p_deinit(struct slsi_dev *sdev, struct netdev_vif *ndev_vif)
{
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "De-initialize P2P\n");

	ndev_vif->vif_type = SLSI_VIFTYPE_UNSPECIFIED;

	/* Work should have been cleaned up by now */
	if (WARN_ON(delayed_work_pending(&ndev_vif->unsync.del_vif_work)))
		cancel_delayed_work(&ndev_vif->unsync.del_vif_work);

	if (WARN_ON(delayed_work_pending(&ndev_vif->unsync.roc_expiry_work)))
		cancel_delayed_work(&ndev_vif->unsync.roc_expiry_work);
}

/**
 * P2P vif activation:
 * Add unsync vif, register for action frames, configure Probe Rsp IEs if required and set channel
 */
int slsi_p2p_vif_activate(struct slsi_dev *sdev, struct net_device *dev, struct ieee80211_channel *chan, u16 duration, bool set_probe_rsp_ies)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u32 af_bmap_active = SLSI_ACTION_FRAME_PUBLIC;
	u32 af_bmap_suspended = SLSI_ACTION_FRAME_PUBLIC;
	int r = 0;

	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "Activate P2P unsync vif\n");

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	/* Interface address and device address are same for P2P unsync vif */
	if (slsi_mlme_add_vif(sdev, dev, dev->dev_addr, dev->dev_addr) != 0) {
		SLSI_NET_ERR(dev, "slsi_mlme_add_vif failed for unsync vif\n");
		goto exit_with_error;
	}

	ndev_vif->activated = true;
	SLSI_P2P_STATE_CHANGE(sdev, P2P_IDLE_VIF_ACTIVE);

	if (slsi_mlme_register_action_frame(sdev, dev, af_bmap_active, af_bmap_suspended) != 0) {
		SLSI_NET_ERR(dev, "Action frame registration failed for unsync vif\n");
		goto exit_with_vif;
	}

	if (set_probe_rsp_ies) {
		u16 purpose = FAPI_PURPOSE_PROBE_RESPONSE;

		if (!ndev_vif->unsync.probe_rsp_ies) {
			SLSI_NET_ERR(dev, "Probe Response IEs not available for ROC\n");
			goto exit_with_vif;
		}

		if (slsi_mlme_add_info_elements(sdev, dev, purpose, ndev_vif->unsync.probe_rsp_ies, ndev_vif->unsync.probe_rsp_ies_len) != 0) {
			SLSI_NET_ERR(dev, "Setting Probe Response IEs for unsync vif failed\n");
			goto exit_with_vif;
		}
		ndev_vif->unsync.ies_changed = false;
	}

	if (slsi_mlme_set_channel(sdev, dev, chan, SLSI_FW_CHANNEL_DURATION_UNSPECIFIED, 0, 0) != 0) {
		SLSI_NET_ERR(dev, "Set channel failed for unsync vif\n");
		goto exit_with_vif;
	} else {
		ndev_vif->chan = chan;
		ndev_vif->driver_channel = chan->hw_value;
	}

	ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
	goto exit;

exit_with_vif:
	slsi_p2p_vif_deactivate(sdev, dev, true);
exit_with_error:
	r = -EINVAL;
exit:
	return r;
}

/* Delete unsync vif - DON'T update the vif type */
void slsi_p2p_vif_deactivate(struct slsi_dev *sdev, struct net_device *dev, bool hw_available)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "De-activate P2P unsync vif\n");

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (sdev->p2p_state == P2P_IDLE_NO_VIF) {
		SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "P2P unsync vif already deactivated\n");
		return;
	}

	/* Indicate failure using cfg80211_mgmt_tx_status() if frame TX is not completed during VIF delete */
	if (ndev_vif->mgmt_tx_data.exp_frame != SLSI_PA_INVALID) {
		ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
		cfg80211_mgmt_tx_status(&ndev_vif->wdev, ndev_vif->mgmt_tx_data.cookie, ndev_vif->mgmt_tx_data.buf, ndev_vif->mgmt_tx_data.buf_len, false, GFP_KERNEL);
#else
		cfg80211_mgmt_tx_status(dev, ndev_vif->mgmt_tx_data.cookie, ndev_vif->mgmt_tx_data.buf, ndev_vif->mgmt_tx_data.buf_len, false, GFP_KERNEL);
#endif
	}

	cancel_delayed_work(&ndev_vif->unsync.del_vif_work);
	if (delayed_work_pending(&ndev_vif->unsync.roc_expiry_work) && sdev->recovery_status) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
		cfg80211_remain_on_channel_expired(&ndev_vif->wdev, ndev_vif->unsync.roc_cookie, ndev_vif->chan,
						   GFP_KERNEL);
#else
		cfg80211_remain_on_channel_expired(ndev_vif->wdev.netdev, ndev_vif->unsync.roc_cookie,
						   ndev_vif->chan, ndev_vif->channel_type, GFP_KERNEL);
#endif
	}
	cancel_delayed_work(&ndev_vif->unsync.roc_expiry_work);

	if (hw_available)
		if (slsi_mlme_del_vif(sdev, dev) != 0)
			SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");

	SLSI_P2P_STATE_CHANGE(sdev, P2P_IDLE_NO_VIF);

	/* slsi_vif_deactivated is not used here after del_vif as it modifies vif type as well */

	ndev_vif->activated = false;
	ndev_vif->chan = NULL;

	if (WARN_ON(ndev_vif->unsync.listen_offload))
		ndev_vif->unsync.listen_offload = false;

	slsi_unsync_vif_set_probe_rsp_ie(ndev_vif, NULL, 0);
	(void)slsi_set_mgmt_tx_data(ndev_vif, 0, 0, NULL, 0);

	SLSI_NET_DBG2(dev, SLSI_INIT_DEINIT, "P2P unsync vif deactivated\n");
}

/**
 * Delete unsync vif when group role is being started.
 * For such cases the net_device during the call would be of the group interface (called from ap_start/connect).
 * Hence get the net_device using P2P Index. Take the mutex lock and call slsi_p2p_vif_deactivate.
 */
void slsi_p2p_group_start_remove_unsync_vif(struct slsi_dev *sdev)
{
	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;

	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "Starting P2P Group - Remove unsync vif\n");

	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2P);
	if (!dev) {
		SLSI_ERR(sdev, "Failed to deactivate p2p vif as dev is not found\n");
		return;
	}

	ndev_vif = netdev_priv(dev);

	if (WARN_ON(!(SLSI_IS_P2P_UNSYNC_VIF(ndev_vif))))
		return;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	slsi_p2p_vif_deactivate(sdev, dev, true);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

/**
 * Called only for P2P Device mode (p2p0 interface) to store the Probe Response IEs
 * which would be used in Listen (ROC) state.
 * If the IEs are received in Listen Offload mode, then configure the IEs in firmware.
 */
int slsi_p2p_dev_probe_rsp_ie(struct slsi_dev *sdev, struct net_device *dev, u8 *probe_rsp_ie, size_t probe_rsp_ie_len)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int ret = 0;

	SLSI_UNUSED_PARAMETER(sdev);

	if (!SLSI_IS_P2P_UNSYNC_VIF(ndev_vif)) {
		SLSI_NET_ERR(dev, "Incorrect vif type - Not unsync vif\n");
		kfree(probe_rsp_ie);
		return -EINVAL;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG2(dev, SLSI_CFG80211, "Received Probe Rsp IE len = %zu, Current IE len = %zu\n", probe_rsp_ie_len, ndev_vif->unsync.probe_rsp_ies_len);

	if (!ndev_vif->unsync.listen_offload) { /* ROC */
		/* Store the IEs. Upon receiving it on subsequent occassions, store only if IEs have changed */
		if (ndev_vif->unsync.probe_rsp_ies_len != probe_rsp_ie_len)                           /* Check if IE length changed */
			ndev_vif->unsync.ies_changed = true;
		else if (memcmp(ndev_vif->unsync.probe_rsp_ies, probe_rsp_ie, probe_rsp_ie_len) != 0) /* Check if IEs changed */
			ndev_vif->unsync.ies_changed = true;
		else {                                                                                    /* No change in IEs */
			kfree(probe_rsp_ie);
			goto exit;
		}

		slsi_unsync_vif_set_probe_rsp_ie(ndev_vif, probe_rsp_ie, probe_rsp_ie_len);
	} else {	/* P2P Listen Offloading */
		if (sdev->p2p_state == P2P_LISTENING) {
			ret = slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_PROBE_RESPONSE, probe_rsp_ie, probe_rsp_ie_len);
			if (ret != 0) {
				SLSI_NET_ERR(dev, "Listen Offloading: Setting Probe Response IEs for unsync vif failed\n");
				ndev_vif->unsync.listen_offload = false;
				slsi_p2p_vif_deactivate(sdev, dev, true);
			}
		}
		kfree(probe_rsp_ie);
	}

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return ret;
}

/**
 * This should be called only for P2P Device mode (p2p0 interface). NULL IEs to clear Probe Response IEs are not updated
 * in driver to avoid configuring the Probe Response IEs to firmware on every ROC.
 * Use this call as a cue to stop any ongoing P2P scan as there is no API from user space for cancelling scan.
 * If ROC was in progress as part of P2P_FIND then Cancel ROC will be received.
 */
int slsi_p2p_dev_null_ies(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct cfg80211_scan_info info = {.aborted = true};

	if (!SLSI_IS_P2P_UNSYNC_VIF(ndev_vif)) {
		SLSI_NET_ERR(dev, "Incorrect vif type - Not unsync vif\n");
		return -EINVAL;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "Probe Rsp NULL IEs\n");

	if (sdev->p2p_state == P2P_SCANNING) {
		struct sk_buff *scan_result;

		SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

		SLSI_NET_DBG1(dev, SLSI_CFG80211, "Stop Find - Abort ongoing P2P scan\n");

		(void)slsi_mlme_del_scan(sdev, dev, ((ndev_vif->ifnum << 8) | SLSI_SCAN_HW_ID), false);

		SLSI_MUTEX_LOCK(ndev_vif->scan_result_mutex);
		scan_result = slsi_dequeue_cached_scan_result(&ndev_vif->scan[SLSI_SCAN_HW_ID], NULL);
		while (scan_result) {
			slsi_rx_scan_pass_to_cfg80211(sdev, dev, scan_result);
			scan_result = slsi_dequeue_cached_scan_result(&ndev_vif->scan[SLSI_SCAN_HW_ID], NULL);
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->scan_result_mutex);

		WARN_ON(!ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req);

		if (ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
			cfg80211_scan_done(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req, &info);
#else
			cfg80211_scan_done(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req, true);
#endif

		ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;

		SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);

		if (ndev_vif->activated) {
			/* Supplicant has stopped FIND. Also clear Probe Response IEs in firmware and driver
			 * as Cancel ROC will not be sent as driver was not in Listen
			 */
			SLSI_NET_DBG1(dev, SLSI_CFG80211, "Stop Find - Clear Probe Response IEs in firmware\n");
			if (slsi_mlme_add_info_elements(sdev, dev, FAPI_PURPOSE_PROBE_RESPONSE, NULL, 0) != 0)
				SLSI_NET_ERR(dev, "Clearing Probe Response IEs failed for unsync vif\n");
			slsi_unsync_vif_set_probe_rsp_ie(ndev_vif, NULL, 0);

			SLSI_P2P_STATE_CHANGE(sdev, P2P_IDLE_VIF_ACTIVE);
		} else {
			SLSI_P2P_STATE_CHANGE(sdev, P2P_IDLE_NO_VIF);
		}
	}

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return 0;
}

/**
 * Returns the public action frame subtype.
 * Returns SLSI_PA_INVALID if it is not a public action frame.
 */
int slsi_get_public_action_subtype(const struct ieee80211_mgmt *mgmt)
{
	int subtype = SLSI_PA_INVALID;
	/* Vendor specific Public Action (0x09), P2P OUI (0x50, 0x6f, 0x9a), P2P Subtype (0x09) */
	u8 p2p_pa_frame[5] = { 0x09, 0x50, 0x6f, 0x9a, 0x09 };
	u8 *action = (u8 *)&mgmt->u.action.u;

	if (memcmp(&action[0], p2p_pa_frame, 5) == 0) {
		subtype = action[5];
	} else {
		/* For service discovery action frames dummy subtype is used */
		switch (action[0]) {
		case SLSI_PA_GAS_INITIAL_REQ:
		case SLSI_PA_GAS_INITIAL_RSP:
		case SLSI_PA_GAS_COMEBACK_REQ:
		case SLSI_PA_GAS_COMEBACK_RSP:
			subtype = (action[0] | SLSI_PA_GAS_DUMMY_SUBTYPE_MASK);
			break;
		}
	}

	return subtype;
}

/**
 * Returns the P2P status code of Status attribute of the GO Neg Rsp frame.
 * Returns -1 if status attribute is NOT found.
 */
int slsi_p2p_get_go_neg_rsp_status(struct net_device *dev, const struct ieee80211_mgmt *mgmt)
{
	int status = -1;
	u8 p2p_oui_type[4] = { 0x50, 0x6f, 0x9a, 0x09 };
	u8 *action = (u8 *)&mgmt->u.action.u;
	u8 *vendor_ie = &action[7];             /* 1 (0x09), 4 (0x50, 0x6f, 0x9a, 0x09), 1 (0x01), 1 (Dialog Token) */
	u8 ie_length, elem_idx;
	u16 attr_length;

	while (vendor_ie && (*vendor_ie == SLSI_WLAN_EID_VENDOR_SPECIFIC)) {
		ie_length = vendor_ie[1];

		if (memcmp(&vendor_ie[2], p2p_oui_type, 4) == 0) {
			elem_idx = 6; /* 1 (Id - 0xdd) + 1 (Length) + 4 (OUI and Type) */

			while (ie_length > elem_idx) {
				attr_length = ((vendor_ie[elem_idx + 1]) | (vendor_ie[elem_idx + 2] << 8));

				if (vendor_ie[elem_idx] == SLSI_P2P_STATUS_ATTR_ID) {
					SLSI_NET_DBG3(dev, SLSI_CFG80211, "Status Attribute Found, attr_length = %d, value (%u %u %u %u)\n",
						      attr_length, vendor_ie[elem_idx], vendor_ie[elem_idx + 1], vendor_ie[elem_idx + 2], vendor_ie[elem_idx + 3]);
					status = vendor_ie[elem_idx + 3];
					break;
				}
				elem_idx += 3 + attr_length;
			}

			break;
		}
		vendor_ie += 2 + ie_length;
	}

	SLSI_UNUSED_PARAMETER(dev);

	return status;
}

/**
 * Returns the next expected public action frame subtype for input subtype.
 * Returns SLSI_PA_INVALID if no frame is expected.
 */
u8 slsi_get_exp_peer_frame_subtype(u8 subtype)
{
	switch (subtype) {
	/* Peer response is expected for following frames */
	case SLSI_P2P_PA_GO_NEG_REQ:
	case SLSI_P2P_PA_GO_NEG_RSP:
	case SLSI_P2P_PA_INV_REQ:
	case SLSI_P2P_PA_DEV_DISC_REQ:
	case SLSI_P2P_PA_PROV_DISC_REQ:
	case SLSI_PA_GAS_INITIAL_REQ_SUBTYPE:
	case SLSI_PA_GAS_COMEBACK_REQ_SUBTYPE:
		return subtype + 1;
	default:
		return SLSI_PA_INVALID;
	}
}

#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
u8 slsi_bss_connect_type_get(struct slsi_dev *sdev, const u8 *ie, size_t ie_len)
{
	const u8 *rsn;
	const u8 *wpa_ie;
	u8 akm_type = 0;

	rsn = cfg80211_find_ie(WLAN_EID_RSN, ie, ie_len);
	wpa_ie = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPA, ie, ie_len);
	if (rsn) {
		int pos = 0;
		int ie_len = rsn[1] + 2;
		int akm_count = 0;
		int i = 0;

		/* Calculate the position of AKM suite in RSNIE
		 * RSNIE TAG(1 byte) + length(1 byte) + version(2 byte) + Group cipher suite(4 bytes)
		 * pairwise suite count(2 byte) + pairwise suite count * 4 + AKM suite count(2 byte)
		 * pos is the array index not length
		 */
		if (ie_len < 9)
			return akm_type;
		pos = 7 + 2 + (rsn[8] * 4);
		akm_count = rsn[pos + 1];
		pos += 2;
		if (ie_len < (pos + 1))
			return akm_type;
		for (i = 0; i < akm_count; i++) {
			if (ie_len < (pos + 1))
				return akm_type;
			if (rsn[pos + 1] == 0x00 && rsn[pos + 2] == 0x0f && rsn[pos + 3] == 0xac) {
				if (rsn[pos + 4] == 0x08 || rsn[pos + 4] == 0x09)
					akm_type |= SLSI_BSS_SECURED_SAE;
				else if (rsn[pos + 4] == 0x02 || rsn[pos + 4] == 0x04 || rsn[pos + 4] == 0x06)
					akm_type |= SLSI_BSS_SECURED_PSK;
				else if (rsn[pos + 4] == 0x01 || rsn[pos + 4] == 0x03)
					akm_type |= SLSI_BSS_SECURED_1x;
			}
			pos += 4;
		}
	} else if (wpa_ie) {
		int pos = 0;
		int ie_len = wpa_ie[1] + 2;
		int akm_count = 0;
		int i = 0;

		/* Calculate the position of AKM suite in WPAIE
		 * ELEMENT ID (dd)(1 byte) + length(1 byte) + ciphersuite(0050f201)(4 bytes)+
		 * version(2 byte) + Group cipher suite(4 bytes)
		 * pairwise suite count(2 byte) + pairwise suite count * 4 + AKM suite count(2 byte)
		 * pos is the array index not length
		 */
		if (ie_len < 13)
			return akm_type;
		pos = 13 + (wpa_ie[12] * 4);
		akm_count = wpa_ie[pos + 1];
		pos += 2;
		if (ie_len < (pos + 1))
			return akm_type;
		for (i = 0; i < akm_count; i++) {
			if (ie_len < (pos + 1))
				return akm_type;
			if (wpa_ie[pos + 1] == 0x00 && wpa_ie[pos + 2] == 0x50 && wpa_ie[pos + 3] == 0xf2) {
				if (wpa_ie[pos + 4] == 0x02)
					akm_type |= SLSI_BSS_SECURED_PSK;
				else if (wpa_ie[pos + 4] == 0x01)
					akm_type |= SLSI_BSS_SECURED_1x;
				else if (wpa_ie[pos + 4] == 0x00)
					akm_type |= SLSI_BSS_SECURED_NO;
			}
			pos += 4;
		}
	} else {
		akm_type = SLSI_BSS_SECURED_NO;
	}

	return akm_type;
}
#endif


void slsi_wlan_dump_public_action_subtype(struct slsi_dev *sdev, struct ieee80211_mgmt *mgmt, bool tx)
{
	u8 action_code = ((u8 *)&mgmt->u.action.u)[0];
	u8 action_category = mgmt->u.action.category;
	char *tx_rx_string = "Received";
	char wnm_action_fields[28][35] = { "Event Request", "Event Report", "Diagnostic Request",
					   "Diagnostic Report", "Location Configuration Request",
					   "Location Configuration Response", "BSS Transition Management Query",
					   "BSS Transition Management Request",
					   "BSS Transition Management Response", "FMS Request", "FMS Response",
					   "Collocated Interference Request", "Collocated Interference Report",
					   "TFS Request", "TFS Response", "TFS Notify", "WNM Sleep Mode Request",
					   "WNM Sleep Mode Response", "TIM Broadcast Request",
					   "TIM Broadcast Response", "QoS Traffic Capability Update",
					   "Channel Usage Request", "Channel Usage Response", "DMS Request",
					   "DMS Response", "Timing Measurement Request",
					   "WNM Notification Request", "WNM Notification Response" };

	if (tx)
		tx_rx_string = "Send";

	switch (action_category) {
	case WLAN_CATEGORY_RADIO_MEASUREMENT:
		switch (action_code) {
		case SLSI_RM_RADIO_MEASUREMENT_REQ:
			SLSI_INFO(sdev, "%s Radio Measurement Frame (Radio Measurement Req)\n", tx_rx_string);
			break;
		case SLSI_RM_RADIO_MEASUREMENT_REP:
			SLSI_INFO(sdev, "%s Radio Measurement Frame (Radio Measurement Rep)\n", tx_rx_string);
			break;
		case SLSI_RM_LINK_MEASUREMENT_REQ:
			SLSI_INFO(sdev, "%s Radio Measurement Frame (Link Measurement Req)\n", tx_rx_string);
			break;
		case SLSI_RM_LINK_MEASUREMENT_REP:
			SLSI_INFO(sdev, "%s Radio Measurement Frame (Link Measurement Rep)\n", tx_rx_string);
			break;
		case SLSI_RM_NEIGH_REP_REQ:
			SLSI_INFO(sdev, "%s Radio Measurement Frame (Neighbor Report Req)\n", tx_rx_string);
			break;
		case SLSI_RM_NEIGH_REP_RSP:
			SLSI_INFO(sdev, "%s Radio Measurement Frame (Neighbor Report Resp)\n", tx_rx_string);
			break;
		default:
			SLSI_INFO(sdev, "%s Radio Measurement Frame (Reserved)\n", tx_rx_string);
		}
		break;
	case WLAN_CATEGORY_PUBLIC:
		switch (action_code) {
		case SLSI_PA_GAS_INITIAL_REQ:
			SLSI_DBG1_NODEV(SLSI_CFG80211, "%s: GAS Initial Request\n", tx ? "TX" : "RX");
			break;
		case SLSI_PA_GAS_INITIAL_RSP:
			SLSI_DBG1_NODEV(SLSI_CFG80211, "%s: GAS Initial Response\n", tx ? "TX" : "RX");
			break;
		case SLSI_PA_GAS_COMEBACK_REQ:
			SLSI_DBG1_NODEV(SLSI_CFG80211, "%s: GAS Comeback Request\n", tx ? "TX" : "RX");
			break;
		case SLSI_PA_GAS_COMEBACK_RSP:
			SLSI_DBG1_NODEV(SLSI_CFG80211, "%s: GAS Comeback Response\n", tx ? "TX" : "RX");
			break;
		default:
			SLSI_DBG1_NODEV(SLSI_CFG80211, "Unknown Public Action Frame : %d\n", action_code);
		}
		break;
	case WLAN_CATEGORY_WNM:
		if (action_code <= SLSI_WNM_ACTION_FIELD_MAX)
			SLSI_INFO(sdev, "%s WNM Frame (%s)\n", tx_rx_string, wnm_action_fields[action_code]);
		else
			SLSI_INFO(sdev, "%s WNM Frame (Reserved)\n", tx_rx_string);
		break;
	}
}

void slsi_abort_sta_scan(struct slsi_dev *sdev)
{
	struct net_device *wlan_net_dev = NULL;
	struct netdev_vif *ndev_vif;
	struct cfg80211_scan_info info = {.aborted = true};

	wlan_net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);

	if (!wlan_net_dev) {
		SLSI_ERR(sdev, "Dev not found\n");
		return;
	}

	ndev_vif = netdev_priv(wlan_net_dev);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	if (ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req) {
		struct sk_buff *scan_result;

		SLSI_DBG2(sdev, SLSI_CFG80211, "Abort ongoing WLAN scan\n");
		(void)slsi_mlme_del_scan(sdev, wlan_net_dev, ((ndev_vif->ifnum << 8) | SLSI_SCAN_HW_ID), false);
		SLSI_MUTEX_LOCK(ndev_vif->scan_result_mutex);
		scan_result = slsi_dequeue_cached_scan_result(&ndev_vif->scan[SLSI_SCAN_HW_ID], NULL);
		while (scan_result) {
			slsi_rx_scan_pass_to_cfg80211(sdev, wlan_net_dev, scan_result);
			scan_result = slsi_dequeue_cached_scan_result(&ndev_vif->scan[SLSI_SCAN_HW_ID], NULL);
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->scan_result_mutex);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
		cfg80211_scan_done(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req, &info);
#else
		cfg80211_scan_done(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req, true);
#endif

		ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;
		ndev_vif->scan[SLSI_SCAN_HW_ID].requeue_timeout_work = false;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

/**
 * Returns a slsi_dhcp_tx enum value after verifying whether the 802.11 packet in skb
 * is a DHCP packet (identified by UDP port numbers)
 */
int slsi_is_dhcp_packet(u8 *data)
{
	u8 *p;
	int ret = SLSI_TX_IS_NOT_DHCP;

	p = data + SLSI_IP_TYPE_OFFSET;

	if (*p == SLSI_IP_TYPE_UDP) {
		u16 source_port, dest_port;

		p = data + SLSI_IP_SOURCE_PORT_OFFSET;
		source_port = p[0] << 8 | p[1];
		p = data + SLSI_IP_DEST_PORT_OFFSET;
		dest_port = p[0] << 8 | p[1];
		if ((source_port == SLSI_DHCP_CLIENT_PORT) && (dest_port == SLSI_DHCP_SERVER_PORT))
			ret = SLSI_TX_IS_DHCP_CLIENT;
		else if ((source_port == SLSI_DHCP_SERVER_PORT) && (dest_port == SLSI_DHCP_CLIENT_PORT))
			ret = SLSI_TX_IS_DHCP_SERVER;
	}

	return ret;
}

#ifdef CONFIG_SCSC_WLAN_PRIORITISE_IMP_FRAMES
int slsi_is_tcp_sync_packet(struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	/* for AP type (AP or P2P Go) check if the packet is local or intra BSS. If intra BSS then
	 * the IP header and TCP header are not set; so return 0
	 */
	if ((ndev_vif->vif_type == FAPI_VIFTYPE_AP) && (compare_ether_addr(eth_hdr(skb)->h_source, dev->dev_addr) != 0))
		return 0;
	if (be16_to_cpu(eth_hdr(skb)->h_proto) != ETH_P_IP)
		return 0;
	if (ip_hdr(skb)->protocol != IPPROTO_TCP)
		return 0;
	if (!skb_transport_header_was_set(skb))
		return 0;
	if (tcp_hdr(skb)->syn)
		return 1;

	return 0;
}

int slsi_is_dns_packet(u8 *data)
{
	u8 *p;

	p = data + SLSI_IP_TYPE_OFFSET;

	if (*p == SLSI_IP_TYPE_UDP) {
		u16 dest_port;

		p = data + SLSI_IP_DEST_PORT_OFFSET;
		dest_port = p[0] << 8 | p[1];
		if (dest_port == SLSI_DNS_DEST_PORT) /* 0x0035 */
			return 1;
	}

	return 0;
}

int slsi_is_mdns_packet(u8 *data)
{
	u8 *p;

	p = data + SLSI_IP_TYPE_OFFSET;

	if (*p == SLSI_IP_TYPE_UDP) {
		u16 dest_port;

		p = data + SLSI_IP_DEST_PORT_OFFSET;
		dest_port = p[0] << 8 | p[1];
		if (dest_port == SLSI_MDNS_DEST_PORT)
			return 1;
	}
	return 0;
}
#endif

int slsi_ap_prepare_add_info_ies(struct netdev_vif *ndev_vif, const u8 *ies, size_t ies_len)
{
	const u8 *wps_p2p_ies = NULL;
	size_t wps_p2p_ie_len = 0;

	/* The ies may contain Extended Capability followed by WPS IE. The Extended capability IE needs to be excluded. */
	wps_p2p_ies = cfg80211_find_ie(SLSI_WLAN_EID_VENDOR_SPECIFIC, ies, ies_len);
	if (wps_p2p_ies) {
		size_t temp_len = wps_p2p_ies - ies;

		wps_p2p_ie_len = ies_len - temp_len;
	}

	SLSI_NET_DBG2(ndev_vif->wdev.netdev, SLSI_MLME, "WPA IE len = %zu, WMM IE len = %zu, IEs len = %zu, WPS_P2P IEs len = %zu\n",
		      ndev_vif->ap.wpa_ie_len, ndev_vif->ap.wmm_ie_len, ies_len, wps_p2p_ie_len);

	ndev_vif->ap.add_info_ies_len = ndev_vif->ap.wpa_ie_len + ndev_vif->ap.wmm_ie_len + wps_p2p_ie_len;
	ndev_vif->ap.add_info_ies = kmalloc(ndev_vif->ap.add_info_ies_len, GFP_KERNEL); /* Caller needs to free this */

	if (!ndev_vif->ap.add_info_ies) {
		SLSI_NET_DBG1(ndev_vif->wdev.netdev, SLSI_MLME, "Failed to allocate memory for IEs\n");
		ndev_vif->ap.add_info_ies_len = 0;
		return -ENOMEM;
	}

	if (ndev_vif->ap.cache_wpa_ie) {
		memcpy(ndev_vif->ap.add_info_ies, ndev_vif->ap.cache_wpa_ie, ndev_vif->ap.wpa_ie_len);
		ndev_vif->ap.add_info_ies += ndev_vif->ap.wpa_ie_len;
	}

	if (ndev_vif->ap.cache_wmm_ie) {
		memcpy(ndev_vif->ap.add_info_ies, ndev_vif->ap.cache_wmm_ie, ndev_vif->ap.wmm_ie_len);
		ndev_vif->ap.add_info_ies += ndev_vif->ap.wmm_ie_len;
	}

	if (wps_p2p_ies) {
		memcpy(ndev_vif->ap.add_info_ies, wps_p2p_ies, wps_p2p_ie_len);
		ndev_vif->ap.add_info_ies += wps_p2p_ie_len;
	}

	ndev_vif->ap.add_info_ies -= ndev_vif->ap.add_info_ies_len;

	return 0;
}
static int slsi_index_for_chan(int channel)
{
	int idx = 0;
	if (channel <= 14)
		idx = channel;
	else if (channel >= 36 && channel <= 64)   /* Uni1 */
		idx = ((channel - 36) / 4);
	else if (channel >= 100 && channel <= 140) /* Uni2 */
		idx = (8 + ((channel - 100) / 4));
	else if (channel >= 149 && channel <= 165) /* Uni3 */
		idx = (19 + ((channel - 149) / 4));
	if (channel <= 14)
		return idx;
	else
		return 15 + idx;
}

/* Set the correct bit in the channel sets */
static void slsi_roam_channel_cache_add_channel(struct slsi_roaming_network_map_entry *network_map, u8 channel)
{
	if (channel <= 14)
		network_map->channels_24_ghz |= (1 << channel);
	else if (channel >= 36 && channel <= 64)   /* Uni1 */
		network_map->channels_5_ghz |= (1 << ((channel - 36) / 4));
	else if (channel >= 100 && channel <= 140) /* Uni2 */
		network_map->channels_5_ghz |= (1 << (8 + ((channel - 100) / 4)));
	else if (channel >= 149 && channel <= 165) /* Uni3 */
		network_map->channels_5_ghz |= (1 << (24 + ((channel - 149) / 4)));
}

void slsi_roam_channel_cache_add_entry(struct slsi_dev *sdev, struct net_device *dev, const u8 *ssid, const u8 *bssid, u8 channel)
{
	struct list_head *pos;
	int found = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int idx;

	list_for_each(pos, &ndev_vif->sta.network_map) {
		struct slsi_roaming_network_map_entry *network_map = list_entry(pos, struct slsi_roaming_network_map_entry, list);

		if (network_map->ssid.ssid_len == ssid[1] &&
		    memcmp(network_map->ssid.ssid, &ssid[2], ssid[1]) == 0) {
			found = 1;
			network_map->last_seen_jiffies = jiffies;
			idx = slsi_index_for_chan(channel);
			network_map->channel_jiffies[idx] = jiffies;
			if (network_map->only_one_ap_seen && memcmp(network_map->initial_bssid, bssid, ETH_ALEN) != 0)
				network_map->only_one_ap_seen = false;
			slsi_roam_channel_cache_add_channel(network_map, channel);
			break;
		}
	}
	if (!found) {
		struct slsi_roaming_network_map_entry *network_map;

		SLSI_NET_DBG3(dev, SLSI_MLME, "New Entry : Channel: %d : %.*s\n", channel, ssid[1], &ssid[2]);
		network_map = kmalloc(sizeof(*network_map), GFP_ATOMIC);
		if (network_map) {
			network_map->ssid.ssid_len = ssid[1];
			memcpy(network_map->ssid.ssid, &ssid[2], ssid[1]);
			network_map->channels_24_ghz = 0;
			network_map->channels_5_ghz = 0;
			network_map->last_seen_jiffies = jiffies;
			idx = slsi_index_for_chan(channel);
			network_map->channel_jiffies[idx] = jiffies;
			SLSI_ETHER_COPY(network_map->initial_bssid, bssid);
			network_map->only_one_ap_seen = true;
			slsi_roam_channel_cache_add_channel(network_map, channel);
			list_add(&network_map->list, &ndev_vif->sta.network_map);
		} else {
			SLSI_ERR(sdev, "New Entry : %.*s kmalloc() failed\n", ssid[1], &ssid[2]);
		}
	}
}

void slsi_roam_channel_cache_add(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt = fapi_get_mgmt(skb);
	size_t mgmt_len = fapi_get_mgmtlen(skb);
	int ielen = mgmt_len - (mgmt->u.beacon.variable - (u8 *)mgmt);
	u32 freq = fapi_get_u16(skb, u.mlme_scan_ind.channel_frequency) / 2;
	const u8 *scan_ds = cfg80211_find_ie(WLAN_EID_DS_PARAMS, mgmt->u.beacon.variable, ielen);
	const u8 *scan_ht = cfg80211_find_ie(WLAN_EID_HT_OPERATION, mgmt->u.beacon.variable, ielen);
	const u8 *scan_ssid = cfg80211_find_ie(WLAN_EID_SSID, mgmt->u.beacon.variable, ielen);
	u8 chan = 0;

	/* Use the DS or HT channel as the Offchannel results mean the RX freq is not reliable */
	if (scan_ds)
		chan = scan_ds[2];
	else if (scan_ht)
		chan = scan_ht[2];
	else
		chan = ieee80211_frequency_to_channel(freq);

	if (chan) {
		enum nl80211_band band = NL80211_BAND_2GHZ;

		if (chan > 14)
			band = NL80211_BAND_5GHZ;

#ifdef CONFIG_SCSC_WLAN_DEBUG
		if (freq != (u32)ieee80211_channel_to_frequency(chan, band)) {
			if (band == NL80211_BAND_5GHZ && freq < 3000)
				SLSI_NET_DBG2(dev, SLSI_MLME, "Off Band Result : mlme_scan_ind(freq:%d) != DS(freq:%d)\n", freq, ieee80211_channel_to_frequency(chan, band));

			if (band == NL80211_BAND_2GHZ && freq > 3000)
				SLSI_NET_DBG2(dev, SLSI_MLME, "Off Band Result : mlme_scan_ind(freq:%d) != DS(freq:%d)\n", freq, ieee80211_channel_to_frequency(chan, band));
		}
#endif
	}

	if (!scan_ssid || !scan_ssid[1] || scan_ssid[1] > 32) {
		SLSI_NET_DBG3(dev, SLSI_MLME, "SSID not defined : Could not find SSID ie or Hidden\n");
		return;
	}

	slsi_roam_channel_cache_add_entry(sdev, dev, scan_ssid, mgmt->bssid, chan);
}

void slsi_roam_channel_cache_prune(struct net_device *dev, int seconds, char *ssid)
{
	struct slsi_roaming_network_map_entry *network_map;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct list_head *pos, *q;
	unsigned long now = jiffies;
	unsigned long age;
	int i = 0;

	if (ssid) {
		list_for_each_safe(pos, q, &ndev_vif->sta.network_map) {
			network_map = list_entry(pos, struct slsi_roaming_network_map_entry, list);
			if (ssid && (memcmp(network_map->ssid.ssid, ssid, network_map->ssid.ssid_len) == 0)) {
				SLSI_ERR_NODEV("Pruning network map entry for ssid %s\n", ssid);
				list_del(pos);
				kfree(network_map);
				break;
			}
		}
	} else {
		list_for_each_safe(pos, q, &ndev_vif->sta.network_map) {
			network_map = list_entry(pos, struct slsi_roaming_network_map_entry, list);
			age = (now - network_map->last_seen_jiffies) / HZ;
			for (i = 1; i <= 38; i++) {
				if (time_after_eq(now, network_map->channel_jiffies[i] + (seconds * HZ))) {
					if (i <= 14)
						network_map->channels_24_ghz &= ~(1 << (i));
					else if (i > 14 && i < 34)
						network_map->channels_5_ghz &= ~(1 << (i - 15));
					else
						network_map->channels_5_ghz &= ~(1 << (i - 10));
				}
			}
			if (!network_map->channels_24_ghz && !network_map->channels_5_ghz) {
					list_del(pos);
					kfree(network_map);
			}
		}
	}
}

int slsi_roam_channel_cache_get_channels_int(struct net_device *dev, struct slsi_roaming_network_map_entry *network_map, u8 *channels)
{
	int index = 0;
	int i;

	SLSI_UNUSED_PARAMETER(dev);

	/* 2.4 Ghz Channels */
	for (i = 1; i <= 14; i++)
		if (network_map->channels_24_ghz & (1 << i)) {
			channels[index] = i;
			index++;
		}

	/* 5 Ghz Uni1 Channels */
	for (i = 36; i <= 64; i += 4)
		if (network_map->channels_5_ghz & (1 << ((i - 36) / 4))) {
			channels[index] = i;
			index++;
		}

	/* 5 Ghz Uni2 Channels */
	for (i = 100; i <= 140; i += 4)
		if (network_map->channels_5_ghz & (1 << (8 + ((i - 100) / 4)))) {
			channels[index] = i;
			index++;
		}

	/* 5 Ghz Uni3 Channels */
	for (i = 149; i <= 165; i += 4)
		if (network_map->channels_5_ghz & (1 << (24 + ((i - 149) / 4)))) {
			channels[index] = i;
			index++;
		}
	return index;
}

struct slsi_roaming_network_map_entry *slsi_roam_channel_cache_get(struct net_device *dev, const u8 *ssid)
{
	struct slsi_roaming_network_map_entry *network_map = NULL;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct list_head *pos;

	if (WARN_ON(!ssid))
		return NULL;

	list_for_each(pos, &ndev_vif->sta.network_map) {
		network_map = list_entry(pos, struct slsi_roaming_network_map_entry, list);
		if (network_map->ssid.ssid_len == ssid[1] &&
		    memcmp(network_map->ssid.ssid, &ssid[2], ssid[1]) == 0)
			break;
	}
	return network_map;
}

u32 slsi_roam_channel_cache_get_channels(struct net_device *dev, const u8 *ssid, u8 *channels)
{
	u32 channels_count = 0;
	struct slsi_roaming_network_map_entry *network_map;

	network_map = slsi_roam_channel_cache_get(dev, ssid);
	if (network_map)
		channels_count = slsi_roam_channel_cache_get_channels_int(dev, network_map, channels);

	return channels_count;
}

int slsi_roaming_scan_configure_channels(struct slsi_dev *sdev, struct net_device *dev, const u8 *ssid, u8 *channels)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u32 cached_channels_count;

	SLSI_UNUSED_PARAMETER(sdev);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
	WARN_ON(!ndev_vif->activated);
	WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION);

	cached_channels_count = slsi_roam_channel_cache_get_channels(dev, ssid, channels);
	SLSI_NET_DBG3(dev, SLSI_MLME, "Roaming Scan Channels. %d cached\n", cached_channels_count);

	return cached_channels_count;
}

int slsi_send_rcl_channel_list_event(struct slsi_dev *sdev, u32 channel_count, u16 *channel_list, u8 *ssid, u8 ssid_len)
{
	struct sk_buff                         *skb = NULL;
	u8                                     err = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, NULL, NLMSG_DEFAULT_SIZE,
					  SLSI_NL80211_VENDOR_RCL_CHANNEL_LIST_EVENT, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, NLMSG_DEFAULT_SIZE,
					  SLSI_NL80211_VENDOR_RCL_CHANNEL_LIST_EVENT, GFP_KERNEL);
#endif
	if (!skb) {
		SLSI_ERR_NODEV("Failed to allocate skb for VENDOR RCL channel list\n");
		return -ENOMEM;
	}

	err |= nla_put(skb, SLSI_WLAN_VENDOR_ATTR_SSID, ssid_len, ssid);
	err |= nla_put_u32(skb, SLSI_WLAN_VENDOR_ATTR_RCL_CHANNEL_COUNT, channel_count);
	err |= nla_put(skb, SLSI_WLAN_VENDOR_ATTR_RCL_CHANNEL_LIST, channel_count * sizeof(u16), channel_list);

	if (err) {
		SLSI_ERR_NODEV("Failed nla_put err=%d\n", err);
		kfree_skb(skb);
		return -EINVAL;
	}
	SLSI_INFO(sdev, "Event: SLSI_NL80211_VENDOR_RCL_CHANNEL_LIST_EVENT(%d)\n",
		  SLSI_NL80211_VENDOR_RCL_CHANNEL_LIST_EVENT);
	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;
}

int slsi_send_acs_event(struct slsi_dev *sdev, struct slsi_acs_selected_channels acs_selected_channels)
{
	struct sk_buff                         *skb = NULL;
	u8 err = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, NULL, NLMSG_DEFAULT_SIZE,
					  SLSI_NL80211_VENDOR_ACS_EVENT, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(sdev->wiphy, NLMSG_DEFAULT_SIZE,
					  SLSI_NL80211_VENDOR_ACS_EVENT, GFP_KERNEL);
#endif
	if (!skb) {
		SLSI_ERR_NODEV("Failed to allocate skb for VENDOR ACS event\n");
		return -ENOMEM;
	}
	err |= nla_put_u8(skb, SLSI_ACS_ATTR_PRIMARY_CHANNEL, acs_selected_channels.pri_channel);
	err |= nla_put_u8(skb, SLSI_ACS_ATTR_SECONDARY_CHANNEL, acs_selected_channels.sec_channel);
	err |= nla_put_u8(skb, SLSI_ACS_ATTR_VHT_SEG0_CENTER_CHANNEL, acs_selected_channels.vht_seg0_center_ch);
	err |= nla_put_u8(skb, SLSI_ACS_ATTR_VHT_SEG1_CENTER_CHANNEL, acs_selected_channels.vht_seg1_center_ch);
	err |= nla_put_u16(skb, SLSI_ACS_ATTR_CHWIDTH, acs_selected_channels.ch_width);
	err |= nla_put_u8(skb, SLSI_ACS_ATTR_HW_MODE, acs_selected_channels.hw_mode);
	SLSI_DBG3(sdev, SLSI_MLME, "pri_channel=%d,sec_channel=%d,vht_seg0_center_ch=%d,"
				"vht_seg1_center_ch=%d, ch_width=%d, hw_mode=%d\n",
				acs_selected_channels.pri_channel, acs_selected_channels.sec_channel,
				acs_selected_channels.vht_seg0_center_ch, acs_selected_channels.vht_seg1_center_ch,
				acs_selected_channels.ch_width, acs_selected_channels.hw_mode);
	if (err) {
		SLSI_ERR_NODEV("Failed nla_put err=%d\n", err);
		kfree_skb(skb);
		return -EINVAL;
	}
	SLSI_INFO(sdev, "Event: SLSI_NL80211_VENDOR_ACS_EVENT(%d)\n", SLSI_NL80211_VENDOR_ACS_EVENT);
	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;
}

#ifdef CONFIG_SCSC_WLAN_WES_NCHO
int slsi_is_wes_action_frame(const struct ieee80211_mgmt *mgmt)
{
	int r = 0;
	/* Vendor specific Action (0x7f), SAMSUNG OUI (0x00, 0x00, 0xf0) */
	u8 wes_vs_action_frame[4] = { 0x7f, 0x00, 0x00, 0xf0 };
	u8 *action = (u8 *)&mgmt->u.action;

	if (memcmp(action, wes_vs_action_frame, 4) == 0)
		r = 1;

	return r;
}
#endif

static u32 slsi_remap_reg_rule_flags(u8 flags)
{
	u32 remapped_flags = 0;

	if (flags & SLSI_REGULATORY_DFS)
		remapped_flags |= NL80211_RRF_DFS;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
	if (flags & SLSI_REGULATORY_NO_OFDM)
		remapped_flags |= NL80211_RRF_NO_OFDM;
#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 13, 0))
	if (flags & SLSI_REGULATORY_NO_IR)
		remapped_flags |= NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS;
#endif
	if (flags & SLSI_REGULATORY_NO_INDOOR)
		remapped_flags |= NL80211_RRF_NO_INDOOR;
	if (flags & SLSI_REGULATORY_NO_OUTDOOR)
		remapped_flags |= NL80211_RRF_NO_OUTDOOR;

	return remapped_flags;
}

static void slsi_reg_mib_to_regd(struct slsi_mib_data *mib, struct slsi_802_11d_reg_domain *domain_info)
{
	int i = 0;
	int num_rules = 0;
	u16 freq;
	u8 byte_val;
	struct ieee80211_reg_rule *reg_rule;

	domain_info->regdomain->alpha2[0] = *(u8 *)(&mib->data[i]);
	i++;

	domain_info->regdomain->alpha2[1] = *(u8 *)(&mib->data[i]);
	i++;

	domain_info->regdomain->dfs_region = *(u8 *)(&mib->data[i]);
	i++;

	while (i < mib->dataLength) {
		reg_rule = &domain_info->regdomain->reg_rules[num_rules];

		/* start freq 2 bytes */
		freq = __le16_to_cpu(*(u16 *)(&mib->data[i]));
		reg_rule->freq_range.start_freq_khz = MHZ_TO_KHZ(freq);

		/* end freq 2 bytes */
		freq = __le16_to_cpu(*(u16 *)(&mib->data[i + 2]));
		reg_rule->freq_range.end_freq_khz = MHZ_TO_KHZ(freq);

		/* Max Bandwidth 1 byte */
		byte_val = *(u8 *)(&mib->data[i + 4]);
		reg_rule->freq_range.max_bandwidth_khz = MHZ_TO_KHZ(byte_val);

		/* max_antenna_gain is obsolute now.*/
		reg_rule->power_rule.max_antenna_gain = 0;

		/* Max Power 1 byte */
		byte_val = *(u8 *)(&mib->data[i + 5]);
		reg_rule->power_rule.max_eirp = DBM_TO_MBM(byte_val);

		/* Flags 1 byte */
		reg_rule->flags = slsi_remap_reg_rule_flags(*(u8 *)(&mib->data[i + 6]));

		i += 7;

		num_rules++; /* Num of reg rules */
	}

	domain_info->regdomain->n_reg_rules = num_rules;
}

void slsi_reset_channel_flags(struct slsi_dev *sdev)
{
	enum nl80211_band band;
	struct ieee80211_channel *chan;
	int i;
	struct wiphy *wiphy = sdev->wiphy;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
#else
	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
#endif
		if (!wiphy->bands[band])
			continue;
		for (i = 0; i < wiphy->bands[band]->n_channels; i++) {
			chan = &wiphy->bands[band]->channels[i];
			chan->flags = 0;
		}
	}
}

int slsi_read_regulatory_rules(struct slsi_dev *sdev, struct slsi_802_11d_reg_domain *domain_info, const char *alpha2)
{
	int i = 0;
	int country_index = 0;
	struct ieee80211_reg_rule *reg_rule = NULL;

	if ((sdev->regdb.regdb_state == SLSI_REG_DB_NOT_SET) || (sdev->regdb.regdb_state == SLSI_REG_DB_ERROR)) {
		SLSI_ERR(sdev, "Regulatory is not set!\n");
		return slsi_read_regulatory_rules_fw(sdev, domain_info, alpha2);
	}

	for (i = 0; i < sdev->regdb.num_countries; i++) {
		if ((sdev->regdb.country[i].alpha2[0] == alpha2[0]) && (sdev->regdb.country[i].alpha2[1] == alpha2[1])) {
			country_index = i;
			break;
		}
	}

	domain_info->regdomain->alpha2[0] = sdev->regdb.country[country_index].alpha2[0];
	domain_info->regdomain->alpha2[1] = sdev->regdb.country[country_index].alpha2[1];
	domain_info->regdomain->dfs_region = sdev->regdb.country[country_index].dfs_region;

	for (i = 0; i < sdev->regdb.country[country_index].collection->reg_rule_num; i++) {
		reg_rule = &domain_info->regdomain->reg_rules[i];

		/* start freq 2 bytes */
		reg_rule->freq_range.start_freq_khz = (sdev->regdb.country[country_index].collection->reg_rule[i]->freq_range->start_freq * 1000);

		/* end freq 2 bytes */
		reg_rule->freq_range.end_freq_khz = (sdev->regdb.country[country_index].collection->reg_rule[i]->freq_range->end_freq * 1000);

		/* Max Bandwidth 1 byte */
		reg_rule->freq_range.max_bandwidth_khz = (sdev->regdb.country[country_index].collection->reg_rule[i]->freq_range->max_bandwidth * 1000);

		/* max_antenna_gain is obsolete now. */
		reg_rule->power_rule.max_antenna_gain = 0;

		/* Max Power 1 byte */
		reg_rule->power_rule.max_eirp = (sdev->regdb.country[country_index].collection->reg_rule[i]->max_eirp * 100);

		/* Flags 1 byte */
		reg_rule->flags = slsi_remap_reg_rule_flags(sdev->regdb.country[country_index].collection->reg_rule[i]->flags);
	}

	domain_info->regdomain->n_reg_rules = sdev->regdb.country[country_index].collection->reg_rule_num;

	return 0;
}

int slsi_read_regulatory_rules_fw(struct slsi_dev *sdev, struct slsi_802_11d_reg_domain *domain_info, const char *alpha2)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_entry mib_val;
	int r                       = 0;
	int rx_len                = 0;
	int len                     = 0;
	int index;

	index = slsi_country_to_index(domain_info, alpha2);

	if (index == -1) {
		SLSI_ERR(sdev, "Unsupported index\n");
		return -EINVAL;
	}

	slsi_mib_encode_get(&mibreq, SLSI_PSID_UNIFI_REGULATORY_PARAMETERS, index);

	/* Max of 6 regulatory constraints.
	 * each constraint start_freq(2 byte), end_freq(2 byte), Band width(1 byte), Max power(1 byte),
	 * rules flag (1 byte)
	 * firmware can have a max of 6 rules for a country.
	 */
	/* PSID header (5 bytes) + ((3 bytes) alpha2 code + dfs) + (max of 50 regulatory rules * 7 bytes each row) + MIB status(1) */
	mibrsp.dataLength = 5 + 3 + (SLSI_MIB_REG_RULES_MAX * 7) + 1;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Failed to alloc for Mib response\n");
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, NULL, mibreq.data, mibreq.dataLength,
			  mibrsp.data, mibrsp.dataLength, &rx_len);
	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = rx_len;

		len = slsi_mib_decode(&mibrsp, &mib_val);

		if (len == 0) {
			kfree(mibrsp.data);
			SLSI_ERR(sdev, "Mib decode error\n");
			return -EINVAL;
		}
		slsi_reg_mib_to_regd(&mib_val.value.u.octetValue, domain_info);
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
	}

	kfree(mibrsp.data);
	return r;
}

static int slsi_country_to_index(struct slsi_802_11d_reg_domain *domain_info, const char *alpha2)
{
	int index = 0;
	bool index_found = false;

	SLSI_DBG3_NODEV(SLSI_MLME, "\n");
	if (domain_info->countrylist) {
		for (index = 0; index < domain_info->country_len; index += 2) {
			if (memcmp(&domain_info->countrylist[index], alpha2, 2) == 0) {
				index_found = true;
				break;
			}
		}

		/* If the set country is not present in the country list, fall back to
		 * world domain i.e. regulatory rules index = 1
		 */
		if (index_found)
			return (index / 2) + 1;
		else
			return 1;
	}

	return -1;
}

/* Set the rssi boost value of a particular band as set in the SETJOINPREFER command*/
int slsi_set_mib_rssi_boost(struct slsi_dev *sdev, struct net_device *dev, u16 psid, int index, int boost)
{
	struct slsi_mib_data mib_data = { 0, NULL };
	int  error = SLSI_MIB_STATUS_FAILURE;

	SLSI_DBG2(sdev, SLSI_MLME, "Set rssi boost: %d\n", boost);
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(sdev->device_config_mutex));
	if (slsi_mib_encode_int(&mib_data, psid, boost, index) == SLSI_MIB_STATUS_SUCCESS)
		if (mib_data.dataLength) {
			error = slsi_mlme_set(sdev, NULL, mib_data.data, mib_data.dataLength);
			if (error)
				SLSI_ERR(sdev, "Err Setting MIB failed. error = %d\n", error);
			kfree(mib_data.data);
		}

	return error;
}

#ifdef CONFIG_SCSC_WLAN_LOW_LATENCY_MODE
int slsi_set_mib_soft_roaming_enabled(struct slsi_dev *sdev, struct net_device *dev, bool enable)
{
	struct slsi_mib_data mib_data = { 0, NULL };
	int error = SLSI_MIB_STATUS_FAILURE;

	if (slsi_mib_encode_bool(&mib_data, SLSI_PSID_UNIFI_ROAM_SOFT_ROAMING_ENABLED,
				 enable, 0) == SLSI_MIB_STATUS_SUCCESS)
		if (mib_data.dataLength) {
			error = slsi_mlme_set(sdev, dev, mib_data.data, mib_data.dataLength);
			if (error)
				SLSI_ERR(sdev, "Err Setting MIB failed. error = %d\n", error);
			kfree(mib_data.data);
		}

	return error;
}
#endif

#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
int slsi_read_enhanced_arp_rx_count_by_lower_mac(struct slsi_dev *sdev, struct net_device *dev, u16 psid)
{
	struct netdev_vif                      *ndev_vif = netdev_priv(dev);
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_entry mib_val;
	int r                       = 0;
	int rx_len                = 0;
	int len                     = 0;

	SLSI_DBG3(sdev, SLSI_MLME, "\n");

	slsi_mib_encode_get(&mibreq, psid, 0);

	mibrsp.dataLength = 10; /* PSID header(5) + uint 4 bytes + status(1) */
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Failed to alloc for Mib response\n");
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, dev, mibreq.data, mibreq.dataLength, mibrsp.data,
			  mibrsp.dataLength, &rx_len);
	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = rx_len;
		len = slsi_mib_decode(&mibrsp, &mib_val);

		if (len == 0) {
			kfree(mibrsp.data);
			SLSI_ERR(sdev, "Mib decode error\n");
			return -EINVAL;
		}
		ndev_vif->enhanced_arp_stats.arp_rsp_rx_count_by_lower_mac = mib_val.value.u.uintValue;
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
	}

	kfree(mibrsp.data);
	return r;
}

void slsi_fill_enhanced_arp_out_of_order_drop_counter(struct netdev_vif *ndev_vif,
						      struct sk_buff *skb)
{
	struct ethhdr *eth_hdr;
	u8 *frame;
	u16 arp_opcode;

#ifdef CONFIG_SCSC_SMAPPER
	/* Check if the payload is in the SMAPPER entry */
	if (fapi_get_u16(skb, u.ma_unitdata_ind.bulk_data_descriptor) == FAPI_BULKDATADESCRIPTOR_SMAPPER) {
		frame = slsi_hip_get_skb_data_from_smapper(ndev_vif->sdev, skb);
		eth_hdr = (struct ethhdr *)frame;
		if (!(eth_hdr)) {
			SLSI_DBG2(ndev_vif->sdev, SLSI_RX, "SKB from SMAPPER is NULL\n");
			return;
		}
		frame = frame + sizeof(struct ethhdr);
	} else {
		frame = fapi_get_data(skb) + sizeof(struct ethhdr);
		eth_hdr = (struct ethhdr *)fapi_get_data(skb);
	}
#else
	frame = fapi_get_data(skb) + sizeof(struct ethhdr);
	eth_hdr = (struct ethhdr *)fapi_get_data(skb);
#endif

	arp_opcode = frame[SLSI_ARP_OPCODE_OFFSET] << 8 | frame[SLSI_ARP_OPCODE_OFFSET + 1];
	/* check if sender ip = gateway ip and it is an ARP response*/
	if ((ntohs(eth_hdr->h_proto) == ETH_P_ARP) &&
	    (arp_opcode == SLSI_ARP_REPLY_OPCODE) &&
	    !SLSI_IS_GRATUITOUS_ARP(frame) &&
	    !memcmp(&frame[SLSI_ARP_SRC_IP_ADDR_OFFSET], &ndev_vif->target_ip_addr, 4))
		ndev_vif->enhanced_arp_stats.arp_rsp_count_out_of_order_drop++;
}
#endif

void slsi_modify_ies_on_channel_switch(struct net_device *dev, struct cfg80211_ap_settings *settings,
				       u8 *ds_params_ie, u8 *ht_operation_ie, struct ieee80211_mgmt  *mgmt,
				       u16 beacon_ie_head_len)
{
	slsi_modify_ies(dev, WLAN_EID_DS_PARAMS, mgmt->u.beacon.variable,
			beacon_ie_head_len, 2, ieee80211_frequency_to_channel(settings->chandef.chan->center_freq));

	slsi_modify_ies(dev, WLAN_EID_HT_OPERATION, (u8 *)settings->beacon.tail,
			settings->beacon.tail_len, 2,
			ieee80211_frequency_to_channel(settings->chandef.chan->center_freq));
}

#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
void slsi_extract_valid_wifi_sharing_channels(struct slsi_dev *sdev)
{
	int i, j;
	int p = 0;
	int k = (SLSI_MAX_CHAN_5G_BAND - 1);
	int flag = 0;

	for (i = 4; i >= 0 ; i--) {
		for (j = 0; j <= 7 ; j++) {
			if ((i == 4) && (j == 0))
				j = 1;
			if (sdev->wifi_sharing_5ghz_channel[i] & (u8)(1 << (7 - j)))
				sdev->valid_5g_chan[p] = slsi_5ghz_all_chans[k];
			else
				sdev->valid_5g_chan[p] = 0;
			p++;
			k--;
			if (p == SLSI_MAX_CHAN_5G_BAND) {
				flag = 1;
				break;
			}
		}
		if (flag == 1)
			break;
	}
}

bool slsi_if_valid_wifi_sharing_channel(struct slsi_dev *sdev, int freq)
{
	int i;

	for (i = 0; i <= (SLSI_MAX_CHAN_5G_BAND - 1) ; i++) {
		if (sdev->valid_5g_chan[i] == ieee80211_get_channel(sdev->wiphy, freq)->hw_value)
			return 1;
	}
	return 0;
}

int slsi_check_if_non_indoor_non_dfs_channel(struct slsi_dev *sdev, int freq)
{
	struct ieee80211_channel  *channel = NULL;
	u32 chan_flags = 0;

	channel =  ieee80211_get_channel(sdev->wiphy, freq);
	if (!channel) {
		SLSI_ERR(sdev, "Invalid frequency %d used to start AP. Channel not found\n", freq);
		return 0;
	}

	chan_flags = (IEEE80211_CHAN_INDOOR_ONLY | IEEE80211_CHAN_RADAR |
			      IEEE80211_CHAN_DISABLED |
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 13)
			      IEEE80211_CHAN_PASSIVE_SCAN
#else
			      IEEE80211_CHAN_NO_IR
#endif
			     );

	if ((channel->flags) & chan_flags)
		return 0;

	return 1;
}

int slsi_get_mhs_ws_chan_vsdb(struct wiphy *wiphy, struct net_device *dev,
			      struct cfg80211_ap_settings *settings,
			      struct slsi_dev *sdev, int *wifi_sharing_channel_switched)
{
	struct net_device *sta_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	struct netdev_vif *ndev_sta_vif  = netdev_priv(sta_dev);
	int sta_frequency = ndev_sta_vif->chan->center_freq;

	if (!slsi_check_if_non_indoor_non_dfs_channel(sdev, sta_frequency))
		return 1; /*AP cannot start on indoor/DFS channel so we will reject request from the host*/

	if ((settings->chandef.chan->center_freq) != (sta_frequency)) {
		*wifi_sharing_channel_switched = 1;
		settings->chandef.chan = ieee80211_get_channel(wiphy, sta_frequency);
		settings->chandef.center_freq1 = sta_frequency;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
		settings->chandef.width = NL80211_CHAN_WIDTH_20;
#endif
	}

	return 0;
}

int slsi_get_mhs_ws_chan_rsdb(struct wiphy *wiphy, struct net_device *dev,
			      struct cfg80211_ap_settings *settings,
			      struct slsi_dev *sdev, int *wifi_sharing_channel_switched)
{
	struct net_device *sta_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	struct netdev_vif *ndev_sta_vif  = netdev_priv(sta_dev);
	int sta_frequency = ndev_sta_vif->chan->center_freq;

	if (((sta_frequency) / 1000) == 2) { /*For 2.4GHz */
		if ((((settings->chandef.chan->center_freq) / 1000) == 5) &&
		    !(slsi_check_if_channel_restricted_already(sdev,
		    ieee80211_frequency_to_channel(settings->chandef.chan->center_freq))) &&
		    slsi_if_valid_wifi_sharing_channel(sdev, settings->chandef.chan->center_freq) &&
		    slsi_check_if_non_indoor_non_dfs_channel(sdev, settings->chandef.chan->center_freq)) {
			settings->chandef.chan = ieee80211_get_channel(wiphy, settings->chandef.chan->center_freq);
			settings->chandef.center_freq1 = settings->chandef.chan->center_freq;
		} else {
			if ((settings->chandef.chan->center_freq) != (sta_frequency)) {
				*wifi_sharing_channel_switched = 1;
				settings->chandef.chan = ieee80211_get_channel(wiphy, sta_frequency);
				settings->chandef.center_freq1 = sta_frequency;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
				settings->chandef.width = NL80211_CHAN_WIDTH_20;
#endif
			}
		}
	} else { /* For 5GHz */
		if (((settings->chandef.chan->center_freq) / 1000) == 5) {
			if (!(slsi_check_if_channel_restricted_already(sdev,
			      ieee80211_frequency_to_channel(sta_frequency))) &&
			    slsi_if_valid_wifi_sharing_channel(sdev, sta_frequency) &&
			    slsi_check_if_non_indoor_non_dfs_channel(sdev, sta_frequency)) {
				if ((settings->chandef.chan->center_freq) != (sta_frequency)) {
					*wifi_sharing_channel_switched = 1;
					settings->chandef.chan = ieee80211_get_channel(wiphy, sta_frequency);
					settings->chandef.center_freq1 = sta_frequency;
					settings->chandef.width = NL80211_CHAN_WIDTH_20;
				}
			} else {
				*wifi_sharing_channel_switched = 1;
				settings->chandef.chan = ieee80211_get_channel(wiphy, SLSI_2G_CHANNEL_ONE);
				settings->chandef.center_freq1 = SLSI_2G_CHANNEL_ONE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
				settings->chandef.width = NL80211_CHAN_WIDTH_20;
#endif
			}
		}
	}

	return 0;
}

int slsi_check_if_channel_restricted_already(struct slsi_dev *sdev, int channel)
{
	int i;

	for (i = 0; i < sdev->num_5g_restricted_channels; i++)
		if (sdev->wifi_sharing_5g_restricted_channels[i] == channel)
			return 1;

	return 0;
}

int slsi_set_wifisharing_permitted_channels(struct net_device *dev, char *buffer, int buf_len)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev   *sdev = ndev_vif->sdev;
	struct slsi_ioctl_args *ioctl_args = NULL;
	int error = 0;
	int i = 0;
	int bit = 0; /* find which bit to set */
	int j = 0;
	int bit_mask = 0;
	int num_channels = 0;
	int indoor_chan_arg = 0;
	u8 *permitted_channels = NULL;

	ioctl_args = slsi_get_private_command_args(buffer, buf_len, 26);
	SLSI_VERIFY_IOCTL_ARGS(sdev, ioctl_args);

	if (!slsi_str_to_int(ioctl_args->args[0], &indoor_chan_arg)) {
		SLSI_ERR(sdev, "Invalid channel_count string: '%s'\n", ioctl_args->args[0]);
		kfree(ioctl_args);
		return -EINVAL;
	}
	permitted_channels = kmalloc(8, GFP_KERNEL);

	if (!permitted_channels) {
		error = -ENOMEM;
		goto exit;
	}

	/* indoor_chan_arg = 0 -> all 5G channels permitted */
	/* indoor_chan_arg = -1 -> no 5G channel permitted */
	/* indoor_chan_arg = n -> set n indoor channels */
	if (indoor_chan_arg == 0) {
		SLSI_DBG2(sdev, SLSI_MLME, "All 5G channels permitted\n");
		sdev->num_5g_restricted_channels = 0;
		for (i = 0; i < 25 ; i++)
			sdev->wifi_sharing_5g_restricted_channels[i] = 0;
		permitted_channels[0] = 0xFF;
		permitted_channels[1] = 0xDF;
		permitted_channels[2] = 0xFF;
		permitted_channels[3] = 0xFF;
		permitted_channels[4] = 0x7F;
		permitted_channels[5] = 0x00;
		permitted_channels[6] = 0x00;
		permitted_channels[7] = 0x00;
	} else if (indoor_chan_arg == -1) {
		SLSI_DBG2(sdev, SLSI_MLME, "No 5G channel permitted\n");
		permitted_channels[0] = 0xFF;
		permitted_channels[1] = 0x1F;
		for (i = 2; i < 8; i++)
			permitted_channels[i] = 0x00;

		for (i = 0; i < 25; i++) {
			sdev->wifi_sharing_5g_restricted_channels[i] = slsi_5ghz_all_chans[i];
		}
		sdev->num_5g_restricted_channels = 25;
	} else if (indoor_chan_arg >= 1 && indoor_chan_arg <= 25) {
		if ((ioctl_args->arg_count - 1) < indoor_chan_arg) {
			SLSI_ERR(sdev, "No. of args actually present are lesser than expected %d\n", indoor_chan_arg);
			error = -EINVAL;
			goto exit;
		}
		sdev->num_5g_restricted_channels = 0;
		for (i = 0; i < 25 ; i++)
			sdev->wifi_sharing_5g_restricted_channels[i] = 0;
		permitted_channels[0] = 0xFF;
		permitted_channels[1] = 0xDF;
		permitted_channels[2] = 0xFF;
		permitted_channels[3] = 0xFF;
		permitted_channels[4] = 0x7F;
		permitted_channels[5] = 0x00;
		permitted_channels[6] = 0x00;
		permitted_channels[7] = 0x00;
		num_channels = indoor_chan_arg;

		for (i = 0; i < num_channels; i++) {
			if (!slsi_str_to_int(ioctl_args->args[i + 1], &indoor_chan_arg)) {
				SLSI_ERR(sdev, "Invalid channel string: '%s'\n", ioctl_args->args[i + 1]);
				error = -EINVAL;
				goto exit;
			}
			if (indoor_chan_arg < 36 || indoor_chan_arg > 165) {
				SLSI_ERR(sdev, "Invalid channel %d\n", indoor_chan_arg);
				error = -EINVAL;
				goto exit;
			}
			sdev->wifi_sharing_5g_restricted_channels[(sdev->num_5g_restricted_channels)++] = indoor_chan_arg;

			for (j = 0; j < 25; j++) {
				if (slsi_5ghz_all_chans[j] == sdev->wifi_sharing_5g_restricted_channels[i]) {
					bit = j + 14;
					break;
				}
			}
			if ((bit < 14) || (bit > 38)) {
				error = -EINVAL;
				SLSI_ERR(sdev, "Incorrect bit position = %d\n", bit);
				goto exit;
			}

			bit_mask  = (bit % 8);
			permitted_channels[bit / 8] &= (u8)(~(1 << (bit_mask)));
		}
	} else {
		SLSI_ERR(sdev, "Invalid channel count %d\n", indoor_chan_arg);
		error = -EINVAL;
		goto exit;
	}

	error = slsi_mlme_wifisharing_permitted_channels(sdev, dev, permitted_channels);

exit:
	if (error)
		SLSI_ERR(sdev, "Error in setting wifi sharing permitted channels. error = %d\n", error);
	kfree(ioctl_args);
	kfree(permitted_channels);
	return error;
}
#endif
#ifdef CONFIG_SCSC_WLAN_ENABLE_MAC_RANDOMISATION
int slsi_set_mac_randomisation_mask(struct slsi_dev *sdev, u8 *mac_address_mask)
{
	int r = 0;
	struct slsi_mib_data mib_data = { 0, NULL };

	SLSI_DBG1(sdev, SLSI_CFG80211, "Mask is :%pM\n", mac_address_mask);
	r = slsi_mib_encode_octet(&mib_data, SLSI_PSID_UNIFI_MAC_ADDRESS_RANDOMISATION_MASK, ETH_ALEN,
				  mac_address_mask, 0);
	if (r != SLSI_MIB_STATUS_SUCCESS) {
		return  -ENOMEM;
	}

	r = slsi_mlme_set(sdev, NULL, mib_data.data, mib_data.dataLength);
	kfree(mib_data.data);

	if (r != SLSI_MIB_STATUS_SUCCESS) {
		SLSI_ERR(sdev, "Err setting unifiMacAddrRandomistaionMask MIB. error = %d\n", r);
		if (sdev->scan_addr_set) {
			struct slsi_mib_data mib_data_randomization_activated = { 0, NULL };

			r = slsi_mib_encode_bool(&mib_data_randomization_activated,
						 SLSI_PSID_UNIFI_MAC_ADDRESS_RANDOMISATION, 1, 0);
			if (r != SLSI_MIB_STATUS_SUCCESS)
				return  -ENOMEM;

			r = slsi_mlme_set(sdev, NULL, mib_data_randomization_activated.data,
					  mib_data_randomization_activated.dataLength);

			kfree(mib_data_randomization_activated.data);

			if (r) {
				SLSI_ERR(sdev, "Err setting unifiMacAddrRandomistaionActivated MIB. error = %d\n", r);
				return r;
			}
		}
	}
	return r;
}
#endif
/* Set the new country code and read the regulatory parameters of updated country. */
int slsi_set_country_update_regd(struct slsi_dev *sdev, const char *alpha2_code, int size)
{
	char alpha2[4];
	int  error = 0;

	SLSI_DBG2(sdev, SLSI_MLME, "Set country code: %c%c\n", alpha2_code[0], alpha2_code[1]);

	if (size == 4) {
		memcpy(alpha2, alpha2_code, 4);
	} else {
		memcpy(alpha2, alpha2_code, 3);
		alpha2[3] = '\0';
	}

	if (memcmp(alpha2, sdev->device_config.domain_info.regdomain->alpha2, 2) == 0) {
		SLSI_DBG3(sdev, SLSI_MLME, "Country is already set to the requested country code\n");
		return 0;
	}

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	error = slsi_mlme_set_country(sdev, alpha2);

	if (error) {
		SLSI_ERR(sdev, "Err setting country error = %d\n", error);
		SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
		return -1;
	}

	/* Read the regulatory params for the country */
	if (slsi_read_regulatory_rules(sdev, &sdev->device_config.domain_info, alpha2) == 0) {
		slsi_reset_channel_flags(sdev);
		wiphy_apply_custom_regulatory(sdev->wiphy, sdev->device_config.domain_info.regdomain);
		slsi_update_supported_channels_regd_flags(sdev);
	}

	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
	return error;
}

/* Read unifiDisconnectTimeOut MIB */
int slsi_read_disconnect_ind_timeout(struct slsi_dev *sdev, u16 psid)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_entry mib_val;
	int r                       = 0;
	int rx_len                = 0;
	int len                     = 0;

	SLSI_DBG3(sdev, SLSI_MLME, "\n");

	slsi_mib_encode_get(&mibreq, psid, 0);

	mibrsp.dataLength = 10; /* PSID header(5) + uint 4 bytes + status(1) */
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Failed to alloc for Mib response\n");
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, NULL, mibreq.data, mibreq.dataLength, mibrsp.data,
			  mibrsp.dataLength, &rx_len);
	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = rx_len;
		len = slsi_mib_decode(&mibrsp, &mib_val);

		if (len == 0) {
			kfree(mibrsp.data);
			SLSI_ERR(sdev, "Mib decode error\n");
			return -EINVAL;
		}
		/* Add additional 1 sec delay */
		sdev->device_config.ap_disconnect_ind_timeout = ((mib_val.value.u.uintValue + 1) * 1000);
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
	}

	kfree(mibrsp.data);
	return r;
}

int slsi_get_beacon_cu(struct slsi_dev *sdev, struct net_device *dev, int *mib_value)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_entry mib_val;
	int r = 0;
	int rx_len = 0;
	int len = 0;

	slsi_mib_encode_get(&mibreq, SLSI_PSID_UNIFI_BEACON_CU, 0);

	mibrsp.dataLength = 6; /* PSID(2) + type(2) + uint16 value(2) */
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Failed to alloc for Mib response\n");
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, dev, mibreq.data, mibreq.dataLength, mibrsp.data,
			  mibrsp.dataLength, &rx_len);
	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = rx_len;
		len = slsi_mib_decode(&mibrsp, &mib_val);

		if (len == 0) {
			kfree(mibrsp.data);
			SLSI_ERR(sdev, "Mib decode error\n");
			return -EINVAL;
		}
		*mib_value = mib_val.value.u.intValue;
		SLSI_DBG2(sdev, SLSI_MLME, "MIB value = %d\n", *mib_value);
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
	}

	kfree(mibrsp.data);
	return r;
}

/* Read unifiDefaultCountry MIB */
int slsi_read_default_country(struct slsi_dev *sdev, u8 *alpha2, u16 index)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_entry mib_val;
	int r                       = 0;
	int rx_len                = 0;
	int len                     = 0;

	slsi_mib_encode_get(&mibreq, SLSI_PSID_UNIFI_DEFAULT_COUNTRY, index);

	mibrsp.dataLength = 11; /* PSID header(5) + index(1) + country code alpha2 3 bytes + status(1) */
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Failed to alloc for Mib response\n");
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, NULL, mibreq.data, mibreq.dataLength, mibrsp.data,
			  mibrsp.dataLength, &rx_len);

	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = rx_len;
		len = slsi_mib_decode(&mibrsp, &mib_val);

		if (len == 0) {
			kfree(mibrsp.data);
			SLSI_ERR(sdev, "Mib decode error\n");
			return -EINVAL;
		}
		memcpy(alpha2, mib_val.value.u.octetValue.data, 2);
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
	}

	kfree(mibrsp.data);
	return r;
}

int slsi_copy_country_table(struct slsi_dev *sdev, struct slsi_mib_data *mib, int len)
{
	SLSI_DBG3(sdev, SLSI_MLME, "\n");

	kfree(sdev->device_config.domain_info.countrylist);
	sdev->device_config.domain_info.countrylist = kmalloc(len, GFP_KERNEL);

	if (!sdev->device_config.domain_info.countrylist) {
		SLSI_ERR(sdev, "kmalloc failed\n");
		return -EINVAL;
	}

	if (!mib || !mib->data) {
		SLSI_ERR(sdev, "Invalid MIB country table\n");
		return -EINVAL;
	}

	memcpy(sdev->device_config.domain_info.countrylist, mib->data, len);
	sdev->device_config.domain_info.country_len = len;

	return 0;
}

/* Read unifi country list */
int slsi_read_unifi_countrylist(struct slsi_dev *sdev, u16 psid)
{
	struct slsi_mib_data mibreq = { 0, NULL };
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_entry mib_val;
	int r                       = 0;
	int rx_len                = 0;
	int len                  = 0;
	int ret;

	slsi_mib_encode_get(&mibreq, psid, 0);

	/* Fixed fields len (5) : 2 bytes(PSID) + 2 bytes (Len) + 1 byte (status)
	 * Data : 148 countries??? for SLSI_PSID_UNIFI_COUNTRY_LIST
	 */
	mibrsp.dataLength = 5 + (NUM_COUNTRY * 2);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);

	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Failed to alloc for Mib response\n");
		kfree(mibreq.data);
		return -ENOMEM;
	}

	r = slsi_mlme_get(sdev, NULL, mibreq.data, mibreq.dataLength, mibrsp.data,
			  mibrsp.dataLength, &rx_len);

	kfree(mibreq.data);

	if (r == 0) {
		mibrsp.dataLength = rx_len;
		len = slsi_mib_decode(&mibrsp, &mib_val);

		if (len == 0) {
			kfree(mibrsp.data);
			return -EINVAL;
		}
		ret = slsi_copy_country_table(sdev, &mib_val.value.u.octetValue, len);
		if (ret < 0) {
			kfree(mibrsp.data);
			return ret;
		}
	} else {
		SLSI_ERR(sdev, "Mib read failed (error: %d)\n", r);
	}

	kfree(mibrsp.data);
	return r;
}

void slsi_clear_offchannel_data(struct slsi_dev *sdev, bool acquire_lock)
{
	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;

	dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	if (WARN_ON(!dev)) {
		SLSI_ERR(sdev, "No Group net dev found\n");
		return;
	}
	ndev_vif = netdev_priv(dev);

	if (acquire_lock)
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	/* Reset dwell time should be sent on group vif */
	(void)slsi_mlme_reset_dwell_time(sdev, dev);

	if (acquire_lock)
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);

	sdev->p2p_group_exp_frame = SLSI_PA_INVALID;
}

static void slsi_hs2_unsync_vif_delete_work(struct work_struct *work)
{
	struct netdev_vif *ndev_vif = container_of((struct delayed_work *)work, struct netdev_vif, unsync.hs2_del_vif_work);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_NET_DBG1(ndev_vif->wdev.netdev, SLSI_CFG80211, "Delete HS vif duration expired  - Deactivate unsync vif\n");
	slsi_wlan_unsync_vif_deactivate(ndev_vif->sdev, ndev_vif->wdev.netdev, true);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

int slsi_wlan_unsync_vif_activate(struct slsi_dev *sdev, struct net_device *dev,
				  struct ieee80211_channel *chan, u16 wait)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int r = 0;
	u8 device_address[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u32                 action_frame_bmap;

	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "Activate wlan unsync vif\n");

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	ndev_vif->vif_type = FAPI_VIFTYPE_UNSYNCHRONISED;

	/* Avoid suspend when wlan unsync VIF is active */
	slsi_wake_lock(&sdev->wlan_wl);

	if (ndev_vif->mgmt_tx_gas_frame) {
		if (slsi_mlme_add_vif(sdev, dev, ndev_vif->gas_frame_mac_addr, device_address) != 0) {
			SLSI_NET_ERR(dev, "add vif failed for wlan unsync vif\n");
			goto exit_with_error;
		}
	} else {
		if (slsi_mlme_add_vif(sdev, dev, dev->dev_addr, device_address) != 0) {
			SLSI_NET_ERR(dev, "add vif failed for wlan unsync vif\n");
			goto exit_with_error;
		}
	}

	if (slsi_vif_activated(sdev, dev) != 0) {
		SLSI_NET_ERR(dev, "vif activate failed for wlan unsync vif\n");
		if (slsi_mlme_del_vif(sdev, dev) != 0)
			SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");
		goto exit_with_error;
	}
	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_ACTIVE;
	INIT_DELAYED_WORK(&ndev_vif->unsync.hs2_del_vif_work, slsi_hs2_unsync_vif_delete_work);
	action_frame_bmap = SLSI_ACTION_FRAME_PUBLIC | SLSI_ACTION_FRAME_RADIO_MEASUREMENT;

	r = slsi_mlme_register_action_frame(sdev, dev, action_frame_bmap, action_frame_bmap);
	if (r != 0) {
		SLSI_NET_ERR(dev, "slsi_mlme_register_action_frame failed: resultcode = %d, action_frame_bmap:%d\n",
			     r, action_frame_bmap);
		goto exit_with_vif;
	}

	if (slsi_mlme_set_channel(sdev, dev, chan, SLSI_FW_CHANNEL_DURATION_UNSPECIFIED, 0, 0) != 0) {
		SLSI_NET_ERR(dev, "Set channel failed for wlan unsync vif\n");
		goto exit_with_vif;
	}
	ndev_vif->chan = chan;
	ndev_vif->driver_channel = chan->hw_value;
	return r;

exit_with_vif:
	slsi_wlan_unsync_vif_deactivate(sdev, dev, true);
exit_with_error:
	slsi_wake_unlock(&sdev->wlan_wl);
	return -EINVAL;
}

#ifdef CONFIG_SCSC_WLAN_BSS_SELECTION
bool slsi_select_ap_for_connection(struct slsi_dev *sdev, struct net_device *dev, const u8 **bssid,
				   struct ieee80211_channel **channel, bool retry)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
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

			if (slsi_is_bssid_in_blacklist(sdev, dev, bssid_info->bssid) ||
			    bssid_info->connect_attempted)
					continue;
			if (retry) {
				if (ndev_vif->sta.sme.ssid) {
					kfree(ndev_vif->sta.sme.ssid);
					ndev_vif->sta.sme.ssid_len = 0;
				}
				ndev_vif->sta.sme.ssid = slsi_mem_dup((u8 *)ssid_info->ssid.ssid,
								      ssid_info->ssid.ssid_len);
				ndev_vif->sta.sme.ssid_len = ssid_info->ssid.ssid_len;

				if (ndev_vif->sta.sme.bssid)
					kfree(ndev_vif->sta.sme.bssid);
				ndev_vif->sta.sme.bssid = slsi_mem_dup((u8 *)bssid_info->bssid,
								       ETH_ALEN);
				ndev_vif->sta.sme.channel =
				(struct ieee80211_channel *)slsi_mem_dup((u8 *)ieee80211_get_channel(sdev->wiphy,
									(bssid_info->freq / 2)),
									 sizeof(struct ieee80211_channel));
				if (ndev_vif->sta.sta_bss) {
					cfg80211_put_bss(sdev->wiphy, ndev_vif->sta.sta_bss);
					ndev_vif->sta.sta_bss = NULL;
				}
			} else {
				*bssid = bssid_info->bssid;
				*channel = ieee80211_get_channel(sdev->wiphy, (bssid_info->freq / 2));
			}
			bssid_info->connect_attempted = true;
			SLSI_INFO(sdev, "Selecting AP %pM for connection\n", bssid_info->bssid);
			return true;
		}
		break;
	}
	return false;
}

void slsi_set_reset_connect_attempted_flag(struct slsi_dev *sdev, struct net_device *dev, const u8 *bssid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct list_head  *pos;

	list_for_each(pos, &ndev_vif->sta.ssid_info) {
		struct slsi_ssid_info *ssid_info = list_entry(pos, struct slsi_ssid_info, list);
		struct list_head    *pos_bssid;

		if (ssid_info->ssid.ssid_len == ndev_vif->sta.ssid_len &&
		    memcmp(ssid_info->ssid.ssid, &ndev_vif->sta.ssid, ndev_vif->sta.ssid_len) == 0  &&
		    (ssid_info->akm_type & ndev_vif->sta.akm_type)) {
			list_for_each(pos_bssid, &ssid_info->bssid_list) {
				struct slsi_bssid_info *bssid_info = list_entry(pos_bssid, struct slsi_bssid_info, list);

				if (bssid && !memcmp(ndev_vif->sta.connected_bssid, bssid_info->bssid, ETH_ALEN)) {
					bssid_info->connect_attempted = true;
					return;
				} else if (!bssid) {
					bssid_info->connect_attempted = false;
				}
			}
		}
	}
}
#endif

bool slsi_is_bssid_in_blacklist(struct slsi_dev *sdev, struct net_device *dev, u8 *bssid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int i, imax;
	struct list_head *blacklist_pos, *blacklist_q;

	/* Check if mac is present , if present then return true */
	list_for_each_safe(blacklist_pos, blacklist_q, &ndev_vif->acl_data_fw_list) {
		struct slsi_bssid_blacklist_info *blacklist_info;

		blacklist_info = list_entry(blacklist_pos, struct slsi_bssid_blacklist_info, list);
		if (blacklist_info && SLSI_ETHER_EQUAL(blacklist_info->bssid, bssid) &&
		    (jiffies_to_msecs(jiffies) < blacklist_info->end_time)) {
			return true;
		}
	}

	if (ndev_vif->acl_data_supplicant)
		imax = ndev_vif->acl_data_supplicant->n_acl_entries;
	else
		imax = 0;
	for (i = 0; i < imax; i++) {
		if (SLSI_ETHER_EQUAL(ndev_vif->acl_data_supplicant->mac_addrs[i].addr, bssid))
			return true;
	}

	return false;
}

/* Delete unsync vif - DON'T update the vif type */
void slsi_wlan_unsync_vif_deactivate(struct slsi_dev *sdev, struct net_device *dev, bool hw_available)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "De-activate wlan unsync vif\n");

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));

	if (sdev->wlan_unsync_vif_state == WLAN_UNSYNC_NO_VIF) {
		SLSI_NET_DBG1(dev, SLSI_INIT_DEINIT, "wlan unsync vif already deactivated\n");
		return;
	}

	cancel_delayed_work(&ndev_vif->unsync.hs2_del_vif_work);
	/*cancel any remain on channel and send roc expiry to cfg80211.*/
	if (delayed_work_pending(&ndev_vif->unsync.roc_expiry_work) && sdev->recovery_status) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
		cfg80211_remain_on_channel_expired(&ndev_vif->wdev, ndev_vif->unsync.roc_cookie, ndev_vif->chan,
						   GFP_KERNEL);
#else
		cfg80211_remain_on_channel_expired(ndev_vif->wdev.netdev, ndev_vif->unsync.roc_cookie,
						   ndev_vif->chan, ndev_vif->channel_type, GFP_KERNEL);
#endif
		cancel_delayed_work(&ndev_vif->unsync.roc_expiry_work);
	}
	/* slsi_vif_deactivated is not used here after slsi_mlme_del_vif
	 *  as it modifies vif type as well
	 */
	if (hw_available)
		if (slsi_mlme_del_vif(sdev, dev) != 0)
			SLSI_NET_ERR(dev, "slsi_mlme_del_vif failed\n");

	slsi_wake_unlock(&sdev->wlan_wl);

	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_NO_VIF;
	ndev_vif->activated = false;
	ndev_vif->chan = NULL;

	(void)slsi_set_mgmt_tx_data(ndev_vif, 0, 0, NULL, 0);
}

void slsi_scan_ind_timeout_handle(struct work_struct *work)
{
	struct netdev_vif *ndev_vif = container_of((struct delayed_work *)work, struct netdev_vif, scan_timeout_work);
	struct net_device *dev = slsi_get_netdev(ndev_vif->sdev, ndev_vif->ifnum);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	if (ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req) {
		if (ndev_vif->scan[SLSI_SCAN_HW_ID].requeue_timeout_work) {
			queue_delayed_work(ndev_vif->sdev->device_wq, &ndev_vif->scan_timeout_work,
					   msecs_to_jiffies(SLSI_FW_SCAN_DONE_TIMEOUT_MSEC));
			ndev_vif->scan[SLSI_SCAN_HW_ID].requeue_timeout_work = false;
		} else {
			SLSI_WARN(ndev_vif->sdev, "Mlme_scan_done_ind not received\n");
			(void)slsi_mlme_del_scan(ndev_vif->sdev, dev, ndev_vif->ifnum << 8 | SLSI_SCAN_HW_ID, true);
			slsi_scan_complete(ndev_vif->sdev, dev, SLSI_SCAN_HW_ID, false, false);
		}
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

void slsi_blacklist_del_work_handle(struct work_struct *work)
{
	struct netdev_vif *ndev_vif = container_of((struct delayed_work *)work, struct netdev_vif, blacklist_del_work);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct list_head  *blacklist_pos, *blacklist_q;
	u32 retention_time = 0;
	int deleted = 0;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	list_for_each_safe(blacklist_pos, blacklist_q, &ndev_vif->acl_data_fw_list) {
		struct slsi_bssid_blacklist_info *blacklist_info;

		blacklist_info = list_entry(blacklist_pos, struct slsi_bssid_blacklist_info, list);
		if (jiffies_to_msecs(jiffies) > blacklist_info->end_time) {
			list_del(blacklist_pos);
			kfree(blacklist_info);
			deleted = 1;
			continue;
		}
		if (!retention_time)
			retention_time = blacklist_info->end_time - jiffies_to_msecs(jiffies);
		if ((blacklist_info->end_time - jiffies_to_msecs(jiffies)) < retention_time)
			retention_time = blacklist_info->end_time - jiffies_to_msecs(jiffies);
	}
	if (deleted)
		slsi_set_acl(sdev, ndev_vif);
	if (retention_time)
		queue_delayed_work(sdev->device_wq, &ndev_vif->blacklist_del_work,
				   msecs_to_jiffies(retention_time));
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

void slsi_update_supported_channels_regd_flags(struct slsi_dev *sdev)
{
	int i = 0;
	struct wiphy *wiphy = sdev->wiphy;
	struct ieee80211_channel *chan;

	/* If all channels are supported by chip no need disable any channel
	 * So return
	 */
	if (sdev->enabled_channel_count == 39)
		return;
	if (wiphy->bands[0]) {
		for (i = 0; i < ARRAY_SIZE(sdev->supported_2g_channels); i++) {
			if (sdev->supported_2g_channels[i] == 0) {
				chan = &wiphy->bands[0]->channels[i];
				chan->flags |= IEEE80211_CHAN_DISABLED;
			}
		}
	}
	if (sdev->band_5g_supported && wiphy->bands[1]) {
		for (i = 0; i <  ARRAY_SIZE(sdev->supported_5g_channels); i++) {
			if (sdev->supported_5g_channels[i] == 0) {
				chan = &wiphy->bands[1]->channels[i];
				chan->flags |= IEEE80211_CHAN_DISABLED;
			}
		}
	}
}

int slsi_find_chan_idx(u16 chan, u8 hw_mode)
{
	int idx = -1, i = 0;
	u16 slsi_5ghz_channels_list[25] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132,
				136, 140, 144, 149, 153, 157, 161, 165};

	if (hw_mode == SLSI_ACS_MODE_IEEE80211B || hw_mode == SLSI_ACS_MODE_IEEE80211G) {
		if (chan <= MAX_24G_CHANNELS)
			idx = chan - 1;
	} else if (hw_mode == SLSI_ACS_MODE_IEEE80211A) {
		for (i = 0; i < MAX_5G_CHANNELS; i++) {
			if (chan == slsi_5ghz_channels_list[i]) {
				idx = i;
				break;
			}
		}
	} else {
		if (chan <= MAX_24G_CHANNELS) {
			idx = chan - 1;
		} else {
			for (i = 0; i < MAX_5G_CHANNELS; i++) {
				if (chan == slsi_5ghz_channels_list[i]) {
					idx = i + MAX_24G_CHANNELS;
					break;
				}
			}
		}
	}
	return idx;
}

int slsi_set_latency_mode(struct net_device *dev, int latency_mode, int cmd_len)
{
	struct netdev_vif    *ndev_vif = netdev_priv(dev);
	struct slsi_dev      *sdev = ndev_vif->sdev;
	int                  ret = 0;
	u8                   host_state;

	SLSI_DBG1(sdev, SLSI_CFG80211, "latency_mode = %d\n", latency_mode);

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	host_state = sdev->device_config.host_state;

	/* latency_mode = 0 (Normal), latency_mode = 1 or 2 (Low) */
	if (latency_mode)
		host_state = host_state | SLSI_HOSTSTATE_LOW_LATENCY_ACTIVE;
	else
		host_state = host_state & ~SLSI_HOSTSTATE_LOW_LATENCY_ACTIVE;

	ret = slsi_mlme_set_host_state(sdev, dev, host_state);
	if (ret != 0)
		SLSI_NET_ERR(dev, "Error in setting the Host State, ret=%d", ret);
	else
		sdev->device_config.host_state = host_state;

	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);

	return ret;
}

#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
void slsi_wlan_recovery_init(struct slsi_dev *sdev)
{
	int i;
	struct netdev_vif *ndev_vif;
	struct net_device *dev;

	slsi_mlme_set_country_for_recovery(sdev);
	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
		if (sdev->netdev[i]) {
			dev = slsi_get_netdev(sdev, i);
			ndev_vif = netdev_priv(dev);
			if (ndev_vif->is_available) {
				netif_tx_start_all_queues(dev);
				if (ndev_vif->vif_type == FAPI_VIFTYPE_AP)
					slsi_start_ap(sdev->wiphy, sdev->netdev[i], &ndev_vif->backup_settings);
			}
		}
	}
}

void slsi_subsystem_reset(struct work_struct *work)
{
	struct slsi_dev *sdev = container_of(work, struct slsi_dev, recovery_work);
	int err = 0, i;
	int level;
	struct netdev_vif *ndev_vif;
#ifndef CONFIG_SCSC_DOWNLOAD_FILE
	const struct firmware *fw[SLSI_WLAN_MAX_MIB_FILE] = { NULL, NULL };
#endif

	level = atomic_read(&sdev->cm_if.reset_level);
	SLSI_INFO_NODEV("Inside subsytem_reset\n");
	if (level < SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC) {
		err = slsi_sm_recovery_service_stop(sdev);
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_STOPPED;
		sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
		slsi_hip_stop(sdev);
		level = atomic_read(&sdev->cm_if.reset_level);
		if (err != 0 || level == SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC)
			return;
		err = slsi_sm_recovery_service_close(sdev);
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_CLOSED;
		level = atomic_read(&sdev->cm_if.reset_level);
		if (err != 0 || level == SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC || sdev->netdev_up_count == 0) {
			if (sdev->netdev_up_count == 0)
				sdev->mlme_blocked = false;
			return;
		}
		SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
		sdev->device_state = SLSI_DEVICE_STATE_STARTING;
		err = slsi_sm_recovery_service_open(sdev);
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_OPENED;
		level = atomic_read(&sdev->cm_if.reset_level);
		if (err != 0 || level == SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC) {
			SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
			return;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
		reinit_completion(&sdev->sig_wait.completion);
#else
		INIT_COMPLETION(sdev->sig_wait.completion);
#endif

#ifndef CONFIG_SCSC_DOWNLOAD_FILE
		/* The "_t" HCF is used in RF Test mode and wlanlite/production test mode */
		if (slsi_is_rf_test_mode_enabled() || slsi_is_test_mode_enabled()) {
			sdev->mib[0].mib_file_name = mib_file_t;
			sdev->mib[1].mib_file_name = mib_file2_t;
		} else {
			sdev->mib[0].mib_file_name = slsi_mib_file;
			sdev->mib[1].mib_file_name = slsi_mib_file2;
		}
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		sdev->collect_mib.num_files = 0;
#endif
		/* Place MIB files in shared memory */
		for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++) {
			err = slsi_mib_open_file(sdev, &sdev->mib[i], &fw[i]);

			/* Only the first file is mandatory */
			if (i == 0 && err) {
				SLSI_ERR(sdev, "mib: Mandatory wlan hcf missing. WLAN will not start (err=%d)\n", err);
				goto err_done;
			}
		}
		err = slsi_sm_recovery_service_start(sdev);
		if (err) {
			SLSI_ERR(sdev, "slsi_sm_wlan_service_start failed: err=%d\n", err);
			for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++)
				slsi_mib_close_file(sdev, fw[i]);
			goto err_done;
		}

		for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++)
			slsi_mib_close_file(sdev, fw[i]);
#else
		/* Download main MIB file via mlme_set */
		err = slsi_sm_recovery_service_start(sdev);
		if (err) {
			SLSI_ERR(sdev, "slsi_sm_wlan_service_start failed: err=%d\n", err);
			goto err_done;
		}
		SLSI_EC_GOTO(slsi_mib_download_file(sdev, &sdev->mib), err, err_hip_started);
#endif
		level = atomic_read(&sdev->cm_if.reset_level);
		if (level == SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC) {
			SLSI_INFO_NODEV("slsi_sm_recovery_service_start failed subsytem error level changed:%d\n", level);
			SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
			return;
		}
		sdev->device_state = SLSI_DEVICE_STATE_STARTED;
		sdev->mlme_blocked = false;
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_STARTED;
		for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
			if (sdev->netdev[i]) {
				ndev_vif = netdev_priv(sdev->netdev[i]);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
				reinit_completion(&ndev_vif->sig_wait.completion);
#else
				INIT_COMPLETION(ndev_vif->sig_wait.completion);
#endif
			}
		}

		/*wlan system recovery actions*/
		SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
		slsi_wlan_recovery_init(sdev);
		return;
	} else {
		return;
	}
#ifdef CONFIG_SCSC_DOWNLOAD_FILE
err_hip_started:
	slsi_sm_recovery_service_stop(sdev);
	slsi_hip_stop(sdev);
	slsi_sm_recovery_service_close(sdev);
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return;
#endif
err_done:
	slsi_sm_recovery_service_close(sdev);
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return;
}
#endif

void slsi_failure_reset(struct work_struct *work)
{
	struct slsi_dev *sdev = container_of(work, struct slsi_dev, recovery_work_on_stop);
	int r = 0;

	if (sdev->cm_if.recovery_state == SLSI_RECOVERY_SERVICE_STARTED) {
		r = slsi_sm_recovery_service_stop(sdev);
		sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
		slsi_hip_stop(sdev);
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_STOPPED;
	}
	if (sdev->cm_if.recovery_state == SLSI_RECOVERY_SERVICE_STOPPED) {
		r = slsi_sm_recovery_service_close(sdev);
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_CLOSED;
	}
	if (sdev->netdev_up_count == 0) {
		sdev->mlme_blocked = false;
#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
		if (work_pending(&sdev->recovery_work_on_start)) {
			SLSI_INFO(sdev, "Cancel work for chip recovery!!\n");
			cancel_work_sync(&sdev->recovery_work_on_start);
		}
#endif
	}
}

#ifdef CONFIG_SCSC_WLAN_FAST_RECOVERY
void slsi_chip_recovery(struct work_struct *work)
{
	struct slsi_dev *sdev = container_of(work, struct slsi_dev, recovery_work_on_start);
	int r = 0, err = 0, i;
	struct netdev_vif *ndev_vif;
#ifndef CONFIG_SCSC_DOWNLOAD_FILE
	const struct firmware *fw[SLSI_WLAN_MAX_MIB_FILE] = { NULL, NULL };
#endif

	slsi_wake_lock(&sdev->wlan_wl);
	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	if (sdev->cm_if.recovery_state == SLSI_RECOVERY_SERVICE_CLOSED && sdev->netdev_up_count > 0) {
		if (sdev->recovery_status) {
			r = wait_for_completion_timeout(&sdev->recovery_completed,
							msecs_to_jiffies(sdev->recovery_timeout));
			if (r == 0)
				SLSI_INFO(sdev, "recovery_completed timeout\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
			reinit_completion(&sdev->recovery_completed);
#else
			INIT_COMPLETION(sdev->recovery_completed);
#endif
		}
		sdev->device_state = SLSI_DEVICE_STATE_STARTING;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
		reinit_completion(&sdev->sig_wait.completion);
#else
		INIT_COMPLETION(sdev->sig_wait.completion);
#endif
		SLSI_EC_GOTO(slsi_sm_wlan_service_open(sdev), err, err_done); //separate function is not required.
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_OPENED;
	} else if (sdev->netdev_up_count == 0) {
		sdev->mlme_blocked = false;
		goto err_done;
	}
	if (sdev->cm_if.recovery_state == SLSI_RECOVERY_SERVICE_OPENED && sdev->netdev_up_count > 0) {
#ifndef CONFIG_SCSC_DOWNLOAD_FILE
		/* The "_t" HCF is used in RF Test mode and wlanlite/production test mode */
		if (slsi_is_rf_test_mode_enabled() || slsi_is_test_mode_enabled()) {
			sdev->mib[0].mib_file_name = mib_file_t;
			sdev->mib[1].mib_file_name = mib_file2_t;
		} else {
			sdev->mib[0].mib_file_name = slsi_mib_file;
			sdev->mib[1].mib_file_name = slsi_mib_file2;
		}
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		sdev->collect_mib.num_files = 0;
#endif
		/* Place MIB files in shared memory */
		for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++) {
			err = slsi_mib_open_file(sdev, &sdev->mib[i], &fw[i]);

			/* Only the first file is mandatory */
			if (i == 0 && err) {
				SLSI_ERR(sdev, "mib: Mandatory wlan hcf missing. WLAN will not start (err=%d)\n", err);
				slsi_sm_recovery_service_close(sdev);
				goto err_done;
			}
		}
		err = slsi_sm_recovery_service_start(sdev);
		if (err) {
			SLSI_ERR(sdev, "slsi_sm_wlan_service_start failed: err=%d\n", err);
			for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++)
				slsi_mib_close_file(sdev, fw[i]);
			slsi_sm_recovery_service_close(sdev);
			goto err_done;
		}

		for (i = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++)
			slsi_mib_close_file(sdev, fw[i]);
#else
		/* Download main MIB file via mlme_set */
		err = slsi_sm_recovery_service_start(sdev);
		if (err) {
			SLSI_ERR(sdev, "slsi_sm_wlan_service_start failed: err=%d\n", err);
			slsi_sm_recovery_service_close(sdev);
			goto err_done;
		}
		SLSI_EC_GOTO(slsi_mib_download_file(sdev, &sdev->mib), err, err_hip_started);
#endif
		sdev->device_state = SLSI_DEVICE_STATE_STARTED;

		/*wlan system recovery actions*/
		sdev->mlme_blocked = false;
		sdev->cm_if.recovery_state = SLSI_RECOVERY_SERVICE_STARTED;
		for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
			if (sdev->netdev[i]) {
				ndev_vif = netdev_priv(sdev->netdev[i]);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
				reinit_completion(&ndev_vif->sig_wait.completion);
#else
				INIT_COMPLETION(ndev_vif->sig_wait.completion);
#endif
			}
		}
		SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
		slsi_wlan_recovery_init(sdev);
		slsi_wake_unlock(&sdev->wlan_wl);
		return;
	} else {
		SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
		slsi_wake_unlock(&sdev->wlan_wl);
		return;
	}
#ifdef CONFIG_SCSC_DOWNLOAD_FILE
err_hip_started:
	slsi_sm_recovery_service_stop(sdev);
	slsi_hip_stop(sdev);
	slsi_sm_recovery_service_close(sdev);
	slsi_wake_unlock(&sdev->wlan_wl);
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return;
#endif
err_done:
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	slsi_wake_unlock(&sdev->wlan_wl);
}
#endif

#ifdef CONFIG_SCSC_WLAN_DYNAMIC_ITO
int slsi_set_ito(struct net_device *dev, char *command, int buf_len)
{
	int ret = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev   *sdev = ndev_vif->sdev;
	struct slsi_ioctl_args *ioctl_args = NULL;
	int ito;

	ioctl_args = slsi_get_private_command_args(command, buf_len, 1);
	SLSI_VERIFY_IOCTL_ARGS(sdev, ioctl_args);

	if (!slsi_str_to_int(ioctl_args->args[0], &ito)) {
		SLSI_ERR(sdev, "Invalid string: '%s'\n", ioctl_args->args[0]);
		kfree(ioctl_args);
		return -EINVAL;
	}

	if (!(ito >= 100)) {
		SLSI_ERR(sdev, "Invalid ITO value: %u\n", ito);
		kfree(ioctl_args);
		return -EINVAL;
	}
	ret = slsi_set_uint_mib(sdev, NULL, SLSI_PSID_UNIFI_FAST_POWER_SAVE_TIMEOUT, ito);
	kfree(ioctl_args);
	return ret;
}
#endif

/* Bubble sort for sorting */
void slsi_sort_array(u8 arr[], int n)
{
	int i, j;
	u8 temp;
	bool swapped;

	for (i = 0; i < n - 1; i++) {
		swapped = false;
		for (j = 0; j < n - i - 1; j++) {
			if (arr[j] > arr[j + 1]) {
				temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
				swapped = true;
			}
		}
		// IF no two elements were swapped by inner loop, then break
		if (swapped == false)
			break;
	}
}

/* Remove duplicates from an array */
int slsi_remove_duplicates(u8 arr[], int n)
{
	int j = 0, i = 0;

	if (n == 0 || n == 1)
		return n;
    /* To store index of next unique element */
	for (i = 0; i < n - 1; i++)
		if (arr[i] != arr[i + 1])
			arr[j++] = arr[i];

	arr[j++] = arr[n - 1];
	return j;
}

/* Merge two list and remove duplicates */
int slsi_merge_lists(u8 ar1[], int len1, u8 ar2[], int len2, u8 result[])
{
	int k = 0, i = 0, new_len = 0;

	for (i = 0; i < len1; i++)
		result[k++] = ar1[i];

	for (i = 0; i < len2; i++)
		result[k++] = ar2[i];

	slsi_sort_array(result, k);
	new_len = slsi_remove_duplicates(result, k);
	if (new_len > SLSI_ROAMING_CHANNELS_MAX)
		new_len = SLSI_ROAMING_CHANNELS_MAX;

	return new_len;
}

void slsi_rx_update_wake_stats(struct slsi_dev *sdev, struct ethhdr *ehdr, int buff_len)
{
	struct slsi_wlan_driver_wake_reason_cnt *wake_reason_count = &sdev->wake_reason_stats;

	slsi_spinlock_lock(&sdev->wake_stats_lock);
	wake_reason_count->total_rx_data_wake++;
	if (buff_len < sizeof(struct ethhdr))
		goto exit;
	if (is_broadcast_ether_addr(ehdr->h_dest)) {
		wake_reason_count->rx_wake_details.rx_broadcast_cnt++;
		SLSI_DBG3(sdev, SLSI_RX, "Wakeup by broadcast rx data packet:%d\n", wake_reason_count->rx_wake_details.rx_broadcast_cnt);
	}  else if (is_multicast_ether_addr(ehdr->h_dest)) {
		wake_reason_count->rx_wake_details.rx_multicast_cnt++;
		SLSI_DBG3(sdev, SLSI_RX, "Wakeup by multicast rx data packet:%d\n", wake_reason_count->rx_wake_details.rx_multicast_cnt);
		if (be16_to_cpu(ehdr->h_proto) == ETH_P_IP)
			wake_reason_count->rx_multicast_wake_pkt_info.ipv4_rx_multicast_addr_cnt++;
		else if (be16_to_cpu(ehdr->h_proto) == ETH_P_IPV6)
			wake_reason_count->rx_multicast_wake_pkt_info.ipv6_rx_multicast_addr_cnt++;
		else
			wake_reason_count->rx_multicast_wake_pkt_info.other_rx_multicast_addr_cnt++;
	} else {
		wake_reason_count->rx_wake_details.rx_unicast_cnt++;
	}
	if (be16_to_cpu(ehdr->h_proto) == ETH_P_IP && buff_len - sizeof(struct ethhdr) >= sizeof(struct iphdr)) {
		struct iphdr *ip = (struct iphdr *)(ehdr + 1);

		SLSI_DBG3(sdev, SLSI_RX, "Wakeup by ip rx data packet protocol :%d\n", ip->protocol);
		if (ip->protocol ==  IPPROTO_ICMP)
			wake_reason_count->rx_wake_pkt_classification_info.icmp_pkt++;
	} else if (be16_to_cpu(ehdr->h_proto) == ETH_P_IPV6 && buff_len - sizeof(struct ethhdr) >= sizeof(struct ipv6hdr)) {
		struct ipv6hdr *ip = (struct ipv6hdr *)(ehdr + 1);

		SLSI_DBG3(sdev, SLSI_RX, "Wakeup by ipv6 rx data packet next hdr:%d\n", ip->nexthdr);
		if (ip->nexthdr == SLSI_ICMP6_PACKET) {
			struct icmp6hdr *icmp6 = (struct icmp6hdr *)(ip + 1);

			SLSI_DBG3(sdev, SLSI_RX, "Wakeup by icmpv6 rx data packet type:%d\n", icmp6->icmp6_type);
			wake_reason_count->rx_wake_pkt_classification_info.icmp6_pkt++;
			if (buff_len - sizeof(struct ethhdr) - sizeof(struct ipv6hdr) < sizeof(struct icmp6hdr))
				goto exit;
			if (icmp6->icmp6_type == SLSI_ICMP6_PACKET_TYPE_RA)
				wake_reason_count->rx_wake_pkt_classification_info.icmp6_ra++;
			else if (icmp6->icmp6_type == SLSI_ICMP6_PACKET_TYPE_NS)
				wake_reason_count->rx_wake_pkt_classification_info.icmp6_ns++;
			else if (icmp6->icmp6_type == SLSI_ICMP6_PACKET_TYPE_NA)
				wake_reason_count->rx_wake_pkt_classification_info.icmp6_na++;
		}
	}
exit:
	slsi_spinlock_unlock(&sdev->wake_stats_lock);
}

