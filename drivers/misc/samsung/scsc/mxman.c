/****************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/notifier.h>
#ifdef CONFIG_ARCH_EXYNOS
#include <linux/soc/samsung/exynos-soc.h>
#endif
#include "scsc_mx_impl.h"
#include "miframman.h"
#include "mifmboxman.h"
#include "mxman.h"
#include "srvman.h"
#include "mxmgmt_transport.h"
#include "gdb_transport.h"
#include "mxconf.h"
#include "fwimage.h"
#include "fwhdr.h"
#include "mxlog.h"
#include "mxlogger.h"
#include "fw_panic_record.h"
#include "panicmon.h"
#include "mxproc.h"
#include "mxlog_transport.h"
#include "mxsyserr.h"
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include "mxman_sysevent.h"
#endif
#ifdef CONFIG_SCSC_SMAPPER
#include "mifsmapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include "mifqos.h"
#endif
#include "mxfwconfig.h"
#include <scsc/kic/slsi_kic_lib.h>
#include <scsc/scsc_release.h>
#include <scsc/scsc_mx.h>
#include <linux/fs.h>
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

#include <scsc/scsc_logring.h>
#ifdef CONFIG_SCSC_WLBTD
#include "scsc_wlbtd.h"
#define SCSC_SCRIPT_MOREDUMP	"moredump"
#define SCSC_SCRIPT_LOGGER_DUMP	"mx_logger_dump.sh"
static struct work_struct	wlbtd_work;
#else
#define MEMDUMP_FILE_FOR_RECOVERY 2
#endif

#include "scsc_lerna.h"
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
#include "scsc_log_in_dram.h"
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/debug-snapshot.h>
#else
#include <linux/debug-snapshot.h>
#endif
#endif

#include <asm/page.h>
#include <scsc/api/bt_audio.h>

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#include <soc/samsung/memlogger.h>
#endif

#define STRING_BUFFER_MAX_LENGTH 512
#define NUMBER_OF_STRING_ARGS	5
#define MX_DRAM_SIZE (4 * 1024 * 1024)
#define MX_DRAM_SIZE_SECTION_1 (8 * 1024 * 1024)

#if defined(CONFIG_SOC_EXYNOS3830)
#define MX_DRAM_SIZE_SECTION_2 (4 * 1024 * 1024)
#else
#define MX_DRAM_SIZE_SECTION_2 (8 * 1024 * 1024)
#endif

#define MX_DRAM_OFFSET_SECTION_2 MX_DRAM_SIZE_SECTION_1

#define MX_FW_RUNTIME_LENGTH (1024 * 1024)
#define WAIT_FOR_FW_TO_START_DELAY_MS 1000
#define MBOX2_MAGIC_NUMBER 0xbcdeedcb
#define MBOX_INDEX_0 0
#define MBOX_INDEX_1 1
#define MBOX_INDEX_2 2
#define MBOX_INDEX_3 3
#ifdef CONFIG_SOC_EXYNOS7570
#define MBOX_INDEX_4 4
#define MBOX_INDEX_5 5
#define MBOX_INDEX_6 6
#define MBOX_INDEX_7 7
#endif

#define SCSC_PANIC_ORIGIN_FW   (0x0 << 15)
#define SCSC_PANIC_ORIGIN_HOST (0x1 << 15)

#define SCSC_PANIC_TECH_WLAN   (0x0 << 13)
#define SCSC_PANIC_TECH_CORE   (0x1 << 13)
#define SCSC_PANIC_TECH_BT     (0x2 << 13)
#define SCSC_PANIC_TECH_UNSP   (0x3 << 13)

#define SCSC_PANIC_CODE_MASK 0xFFFF
#define SCSC_PANIC_ORIGIN_MASK  0x8000
#define SCSC_PANIC_TECH_MASK    0x6000
#define SCSC_PANIC_SUBCODE_MASK_LEGACY 0x0FFF
#define SCSC_PANIC_SUBCODE_MASK 0x7FFF

#define SCSC_R4_V2_MINOR_52 52
#define SCSC_R4_V2_MINOR_53 53
#define SCSC_R4_V2_MINOR_54 54

#define MM_HALT_RSP_TIMEOUT_MS 100

/* If limits below are exceeded, a service level reset will be raised to level 7 */
#define SYSERR_LEVEL7_HISTORY_SIZE      (4)
/* Minimum time between system error service resets (ms) */
#define SYSERR_LEVEL7_MIN_INTERVAL      (300000)
/* No more then SYSERR_RESET_HISTORY_SIZE system error service resets in this period (ms)*/
#define SYSERR_LEVEL7_MONITOR_PERIOD    (3600000)

static char panic_record_dump[PANIC_RECORD_DUMP_BUFFER_SZ];
static BLOCKING_NOTIFIER_HEAD(firmware_chain);

/**
 * This will be returned as fw version ONLY if Maxwell
 * was never found or was unloaded.
 */
static char saved_fw_build_id[FW_BUILD_ID_SZ] = "Maxwell WLBT unavailable";

static bool allow_unidentified_firmware;
module_param(allow_unidentified_firmware, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(allow_unidentified_firmware, "Allow unidentified firmware");

static bool skip_header;
module_param(skip_header, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(skip_header, "Skip header, assuming unidentified firmware");

static bool crc_check_allow_none = true;
module_param(crc_check_allow_none, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(crc_check_allow_none, "Allow skipping firmware CRC checks if CRC is not present");

static int crc_check_period_ms = 30000;
module_param(crc_check_period_ms, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(crc_check_period_ms, "Time period for checking the firmware CRCs");

static ulong mm_completion_timeout_ms = 2000;
module_param(mm_completion_timeout_ms, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mm_completion_timeout_ms, "Timeout wait_for_mm_msg_start_ind (ms) - default 1000. 0 = infinite");

static bool skip_mbox0_check;
module_param(skip_mbox0_check, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(skip_mbox0_check, "Allow skipping firmware mbox0 signature check");

static uint firmware_startup_flags;
module_param(firmware_startup_flags, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_startup_flags, "0 = Proceed as normal (default); Bit 0 = 1 - spin at start of CRT0; Other bits reserved = 0");

static uint trigger_moredump_level = MX_SYSERR_LEVEL_8;
module_param(trigger_moredump_level, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(trigger_moredump_level, "System error level that triggers moredump - may be 7 or 8 only");

#ifdef CONFIG_SCSC_CHV_SUPPORT
/* First arg controls chv function */
int chv_run;
module_param(chv_run, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_run, "Run chv f/w: 0 = feature disabled, 1 = for continuous checking, 2 = 1 shot, anything else, undefined");

/* Optional array of args for firmware to interpret when chv_run = 1 */
static unsigned int chv_argv[32];
static int chv_argc;

module_param_array(chv_argv, uint, &chv_argc, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_argv, "Array of up to 32 x u32 args for the CHV firmware when chv_run = 1");
#endif

static bool disable_auto_coredump;
module_param(disable_auto_coredump, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_auto_coredump, "Disable driver automatic coredump");

static bool disable_error_handling;
module_param(disable_error_handling, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_error_handling, "Disable error handling");

#define DISABLE_RECOVERY_HANDLING_SCANDUMP 3 /* Halt kernel and scandump on FW failure */

#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION >= 10)
static int disable_recovery_handling = 2; /* MEMDUMP_FILE_FOR_RECOVERY : for /sys/wifi/memdump */
#else
/* AOSP */
static int disable_recovery_handling = 1; /* Recovery disabled, enable in init.rc, not here. */
#endif

module_param(disable_recovery_handling, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_recovery_handling, "Disable recovery handling");
static bool disable_recovery_from_memdump_file = true;
static int memdump = -1;
static bool disable_recovery_until_reboot;

static uint scandump_trigger_fw_panic = 0;
module_param(scandump_trigger_fw_panic, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scandump_trigger_fw_panic, "Specify fw panic ID");

static uint panic_record_delay = 1;
module_param(panic_record_delay, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(panic_record_delay, "Delay in ms before accessing the panic record");

static bool disable_logger = true;
module_param(disable_logger, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_logger, "Disable launch of user space logger");

static uint syserr_level7_min_interval = SYSERR_LEVEL7_MIN_INTERVAL;
module_param(syserr_level7_min_interval, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_level7_min_interval, "Minimum time between system error level 7 resets (ms)");

static uint syserr_level7_monitor_period = SYSERR_LEVEL7_MONITOR_PERIOD;
module_param(syserr_level7_monitor_period, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_level7_monitor_period, "No more then 4 system error level 7 resets in this period (ms)");

/*
 * shared between this module and mgt.c as this is the kobject referring to
 * /sys/wifi directory. Core driver is called 1st we create the directory
 * here and share the kobject, so in mgt.c wifi driver can create
 * /sys/wif/mac_addr using sysfs_create_file api using the kobject
 *
 * If both modules tried to create the dir we were getting kernel panic
 * failure due to kobject associated with dir already existed
 */
static struct kobject *wifi_kobj_ref;
static int refcount;
static ssize_t sysfs_show_memdump(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf);
static ssize_t sysfs_store_memdump(struct kobject *kobj, struct kobj_attribute *attr,
				   const char *buf, size_t count);
static struct kobj_attribute memdump_attr =
		__ATTR(memdump, 0660, sysfs_show_memdump, sysfs_store_memdump);

/* Time stamps of last level7 resets in jiffies */
static unsigned long syserr_level7_history[SYSERR_LEVEL7_HISTORY_SIZE] = {0};
static int syserr_level7_history_index;

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
static int mxman_logring_register_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxlogger_register_global_observer(name);
}

static int mxman_logring_unregister_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxlogger_unregister_global_observer(name);
}

/* callbacks to mxman */
struct scsc_logring_mx_cb mx_logring = {
	.scsc_logring_register_observer = mxman_logring_register_observer,
	.scsc_logring_unregister_observer = mxman_logring_unregister_observer,
};

#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int mxman_minimoredump_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	int ret = 0;
	struct mxman *mxman = (struct mxman *) collect_client->prv;

	if (!mxman || !mxman->start_dram)
		return ret;

	SCSC_TAG_INFO(MXMAN, "Collecting Minimoredump runtime_length %d fw_image_size %d\n",
		mxman->fwhdr.fw_runtime_length, mxman->fw_image_size);
	/* collect RAM sections of FW */
	ret = scsc_log_collector_write(mxman->start_dram + mxman->fw_image_size,
		mxman->fwhdr.fw_runtime_length - mxman->fw_image_size, 1);

	return ret;
}

struct scsc_log_collector_client mini_moredump_client = {
	.name = "minimoredump",
	.type = SCSC_LOG_MINIMOREDUMP,
	.collect_init = NULL,
	.collect = mxman_minimoredump_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static void mxman_get_fw_version_cb(struct scsc_log_collector_mx_cb *mx_cb, char *version, size_t ver_sz)
{
	mxman_get_fw_version(version, ver_sz);
}

static void mxman_get_drv_version_cb(struct scsc_log_collector_mx_cb *mx_cb, char *version, size_t ver_sz)
{
	mxman_get_driver_version(version, ver_sz);
}

static void call_wlbtd_sable_cb(struct scsc_log_collector_mx_cb *mx_cb, u8 trigger_code, u16 reason_code)
{
	call_wlbtd_sable(trigger_code, reason_code);
}

/* Register callbacks from scsc_collect to mx */
struct scsc_log_collector_mx_cb mx_cb = {
	.get_fw_version = mxman_get_fw_version_cb,
	.get_drv_version = mxman_get_drv_version_cb,
	.call_wlbtd_sable = call_wlbtd_sable_cb,
};

#endif

/* Retrieve memdump in sysfs global */
static ssize_t sysfs_show_memdump(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "%d\n", memdump);
}

/* Update memdump in sysfs global */
static ssize_t sysfs_store_memdump(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf,
				   size_t count)
{
	int r;

	r = kstrtoint(buf, 10, &memdump);
	if (r < 0)
		memdump = -1;

	switch (memdump) {
	case 0:
	case 2:
		disable_recovery_from_memdump_file = false;
		break;
	case 3:
	default:
		disable_recovery_from_memdump_file = true;
		break;
	}

	SCSC_TAG_INFO(MXMAN, "memdump: %d\n", memdump);

	return (r == 0) ? count : 0;
}

struct kobject *mxman_wifi_kobject_ref_get(void)
{
	if (refcount++ == 0) {
		/* Create sysfs directory /sys/wifi */
		wifi_kobj_ref = kobject_create_and_add("wifi", NULL);
		kobject_get(wifi_kobj_ref);
		kobject_uevent(wifi_kobj_ref, KOBJ_ADD);
		SCSC_TAG_INFO(MXMAN, "wifi_kobj_ref: 0x%p\n", wifi_kobj_ref);
		WARN_ON(refcount == 0);
	}
	return wifi_kobj_ref;
}
EXPORT_SYMBOL(mxman_wifi_kobject_ref_get);

void mxman_wifi_kobject_ref_put(void)
{
	if (--refcount == 0) {
		kobject_put(wifi_kobj_ref);
		kobject_uevent(wifi_kobj_ref, KOBJ_REMOVE);
		wifi_kobj_ref = NULL;
		WARN_ON(refcount < 0);
	}
}
EXPORT_SYMBOL(mxman_wifi_kobject_ref_put);

/* Register memdump override */
void mxman_create_sysfs_memdump(void)
{
	int r;
	struct kobject *kobj_ref = mxman_wifi_kobject_ref_get();

	SCSC_TAG_INFO(MXMAN, "kobj_ref: 0x%p\n", kobj_ref);

	if (kobj_ref) {
		/* Create sysfs file /sys/wifi/memdump */
		r = sysfs_create_file(kobj_ref, &memdump_attr.attr);
		if (r) {
			/* Failed, so clean up dir */
			SCSC_TAG_ERR(MXMAN, "Can't create /sys/wifi/memdump\n");
			mxman_wifi_kobject_ref_put();
			return;
		}
	} else {
		SCSC_TAG_ERR(MXMAN, "failed to create /sys/wifi directory");
	}
}

/* Unregister memdump override */
void mxman_destroy_sysfs_memdump(void)
{
	if (!wifi_kobj_ref)
		return;

	/* Destroy /sys/wifi/memdump file */
	sysfs_remove_file(wifi_kobj_ref, &memdump_attr.attr);

	/* Destroy /sys/wifi virtual dir */
	mxman_wifi_kobject_ref_put();
}

/* Track when WLBT reset fails to allow debug */
static u64 reset_failed_time;

/* Status of FM driver request, which persists beyond the lifecyle
 * of the scsx_mx driver.
 */
#ifdef CONFIG_SCSC_FM
static u32 is_fm_on;
#endif

static int firmware_runtime_flags;
static int syserr_command;
/**
 * This mxman reference is initialized/nullified via mxman_init/deinit
 * called by scsc_mx_create/destroy on module probe/remove.
 */
static struct mxman *active_mxman;
static bool send_fw_config_to_active_mxman(uint32_t fw_runtime_flags);
static bool send_syserr_cmd_to_active_mxman(u32 syserr_cmd);
static void mxman_fail_level8(struct mxman *mxman, u16 scsc_panic_code, const char *reason);


static bool reset_failed;
static bool mxman_check_reset_failed(struct scsc_mif_abs *mif)
{
	return reset_failed; // || mif->mif_reset_failure(mif);
}

static void mxman_set_reset_failed(void)
{
	reset_failed = true;
}

static int fw_runtime_flags_setter(const char *val, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	uint32_t fw_runtime_flags = 0;

	if (!val)
		return ret;
	ret = kstrtouint(val, 10, &fw_runtime_flags);
	if (!ret) {
		if (send_fw_config_to_active_mxman(fw_runtime_flags))
			firmware_runtime_flags = fw_runtime_flags;
		else
			ret = -EINVAL;
	}
	return ret;
}

/**
 * We don't bother to keep an updated copy of the runtime flags effectively
 * currently set into FW...we should add a new message answer handling both in
 * Kenrel and FW side to be sure and this is just to easy debug at the end.
 */
static struct kernel_param_ops fw_runtime_kops = {
	.set = fw_runtime_flags_setter,
	.get = NULL
};

module_param_cb(firmware_runtime_flags, &fw_runtime_kops, NULL, 0200);
MODULE_PARM_DESC(firmware_runtime_flags,
		 "0 = Proceed as normal (default); nnn = Provides FW runtime flags bitmask: unknown bits will be ignored.");

static int syserr_setter(const char *val, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	u32 syserr_cmd = 0;

	if (!val)
		return ret;
	ret = kstrtouint(val, 10, &syserr_cmd);
	if (!ret) {
		u8 sub_system = (u8)(syserr_cmd / 10);
		u8 level = (u8)(syserr_cmd % 10);

		if (((sub_system > 2) && (sub_system < 8)) || (sub_system > 8) || (level > MX_SYSERR_LEVEL_8))
			ret = -EINVAL;
		else if (level == MX_SYSERR_LEVEL_8) {
			if (active_mxman)
				mxman_fail_level8(active_mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
		} else if (send_syserr_cmd_to_active_mxman(syserr_cmd))
			syserr_command = syserr_cmd;
		else
			ret = -EINVAL;
	}
	return ret;
}

static struct kernel_param_ops syserr_kops = {
	.set = syserr_setter,
	.get = NULL
};

module_param_cb(syserr_command, &syserr_kops, NULL, 0200);
MODULE_PARM_DESC(syserr_command,
		 "Decimal XY - Trigger Type X(0,1,2,8), Level Y(1-8). Some combinations not supported");

/**
 * Maxwell Agent Management Messages.
 *
 * TODO: common defn with firmware, generated.
 *
 * The numbers here *must* match the firmware!
 */
enum {
	MM_START_IND = 0,
	MM_HALT_REQ = 1,
	MM_FORCE_PANIC = 2,
	MM_HOST_SUSPEND = 3,
	MM_HOST_RESUME = 4,
	MM_FW_CONFIG = 5,
	MM_HALT_RSP = 6,
	MM_FM_RADIO_CONFIG = 7,
	MM_LERNA_CONFIG = 8,
	MM_SYSERR_IND = 9,
	MM_SYSERR_CMD = 10
} ma_msg;

/**
 * Format of the Maxwell agent messages
 * on the Maxwell management transport stream.
 */
struct ma_msg_packet {

	uint8_t ma_msg; /* Message from ma_msg enum */
	uint32_t arg;	/* Optional arg set by f/w in some to-host messages */
} __packed;

/**
 * Special case Maxwell management, carrying FM radio configuration structure
 */
struct ma_msg_packet_fm_radio_config {

	uint8_t ma_msg;				/* Message from ma_msg enum */
	struct wlbt_fm_params fm_params;	/* FM Radio parameters */
} __packed;

static bool send_fw_config_to_active_mxman(uint32_t fw_runtime_flags)
{
	bool ret = false;
	struct srvman *srvman = NULL;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send running FW config.\n");
		return ret;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman_in_error(srvman)) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return ret;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED) {
		struct ma_msg_packet message = { .ma_msg = MM_FW_CONFIG,
			.arg = fw_runtime_flags };

		SCSC_TAG_INFO(MXMAN, "MM_FW_CONFIG -  firmware_runtime_flags:%d\n", message.arg);
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message,
				sizeof(message));
		ret = true;
	} else {
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_FW_CONFIG msg.\n");
	}
	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}

static bool send_syserr_cmd_to_active_mxman(u32 syserr_cmd)
{
	bool ret = false;
	struct srvman *srvman = NULL;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send running FW config.\n");
		return ret;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman_in_error(srvman)) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return ret;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED) {
		struct ma_msg_packet message = { .ma_msg = MM_SYSERR_CMD,
			.arg = syserr_cmd};

		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_CMD - Args %02d\n", message.arg);
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message,
				sizeof(message));
		ret = true;
	} else {
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_SYSERR_CMD msg.\n");
	}
	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}

#ifdef CONFIG_SCSC_FM
static bool send_fm_params_to_active_mxman(struct wlbt_fm_params *params)
{
	bool ret = false;
	struct srvman *srvman = NULL;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send FM params\n");
		return false;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman_in_error(srvman)) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return false;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED) {
		struct ma_msg_packet_fm_radio_config message = { .ma_msg = MM_FM_RADIO_CONFIG,
								 .fm_params = *params };

		SCSC_TAG_INFO(MXMAN, "MM_FM_RADIO_CONFIG\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message,
				sizeof(message));

		ret = true;	/* Success */
	} else
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_FM_RADIO_CONFIG msg.\n");

	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}
#endif

static void mxman_stop(struct mxman *mxman);
static void print_mailboxes(struct mxman *mxman);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#ifdef CONFIG_SCSC_WLBTD
static int _mx_exec(char *prog, int wait_exec) __attribute__((unused));
#else
static int _mx_exec(char *prog, int wait_exec);
#endif
#endif
static int wait_for_mm_msg(struct mxman *mxman, struct completion *mm_msg_completion, ulong timeout_ms)
{
	int r;

	(void)mxman; /* unused */

	if (timeout_ms == 0) {
		/* Zero implies infinite wait */
		r = wait_for_completion_interruptible(mm_msg_completion);
		/* r = -ERESTARTSYS if interrupted, 0 if completed */
		return r;
	}
	r = wait_for_completion_timeout(mm_msg_completion, msecs_to_jiffies(timeout_ms));
	if (r == 0) {
		SCSC_TAG_ERR(MXMAN, "timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int wait_for_mm_msg_start_ind(struct mxman *mxman)
{
	return wait_for_mm_msg(mxman, &mxman->mm_msg_start_ind_completion, mm_completion_timeout_ms);
}

static int wait_for_mm_msg_halt_rsp(struct mxman *mxman)
{
	int r;
	(void)mxman; /* unused */

	if (MM_HALT_RSP_TIMEOUT_MS == 0) {
		/* Zero implies infinite wait */
		r = wait_for_completion_interruptible(&mxman->mm_msg_halt_rsp_completion);
		/* r = -ERESTARTSYS if interrupted, 0 if completed */
		return r;
	}

	r = wait_for_completion_timeout(&mxman->mm_msg_halt_rsp_completion, msecs_to_jiffies(MM_HALT_RSP_TIMEOUT_MS));
	if (r)
		SCSC_TAG_INFO(MXMAN, "Received MM_HALT_RSP from firmware");

	return r;
}

#ifndef CONFIG_SCSC_WLBTD
static int coredump_helper(void)
{
	int r;
	int i;
	static char mdbin[128];

	/* Determine path to moredump helper script */
	r = mx140_exe_path(NULL, mdbin, sizeof(mdbin), "moredump");
	if (r) {
		SCSC_TAG_ERR(MXMAN, "moredump path error\n");
		return r;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	for (i = 0; i < 20; i++) {
		r = _mx_exec(mdbin, UMH_WAIT_PROC);
		if (r != -EBUSY)
			break;
		/* If the usermode helper fails with -EBUSY, the userspace is
		 * likely still frozen from suspend. Back off and retry.
		 */
		SCSC_TAG_INFO(MXMAN, "waiting for userspace to thaw...\n");
		msleep(1000);
	}

	/* Application return codes are in the MSB */
	if (r > 0xffL)
		SCSC_TAG_INFO(MXMAN, "moredump.bin exit(%ld), check syslog\n", (r & 0xff00L) >> 8);

	return r;
#else
	SCSC_TAG_INFO(MXMAN, "coredump_helper is not used in GKI\n");
	return -EINVAL;
#endif
}
#endif
static int send_mm_msg_stop_blocking(struct mxman *mxman)
{
	int r;
#ifdef CONFIG_SCSC_FM
	struct ma_msg_packet message = { .ma_msg = MM_HALT_REQ,
			.arg = mxman->on_halt_ldos_on };
#else
	struct ma_msg_packet message = { .ma_msg = MM_HALT_REQ };
#endif
	mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));

	r = wait_for_mm_msg_halt_rsp(mxman);
	if (r) {
		/*
		 * MM_MSG_HALT_RSP is not implemented in all versions of firmware, so don't treat it's non-arrival
		 * as an error
		 */
		SCSC_TAG_INFO(MXMAN, "wait_for_MM_HALT_RSP completed");
	}

	return 0;
}

static char *chip_version(u32 rf_hw_ver)
{
	switch (rf_hw_ver & 0x00ff) {
	default:
		break;
	case 0x00b0:
		if ((rf_hw_ver & 0xff00) > 0x1000)
			return "S610/S611";
		else
			return "S610";
	case 0x00b1:
		return "S612";
	case 0x00b2:
		return "S620";
	case 0x0000:
#if !defined CONFIG_SOC_EXYNOS9610 && !defined CONFIG_SOC_EXYNOS9630
		return "Error: check if RF chip is present";
#else
		return "Unknown";
#endif
	}
	return "Unknown";
}

/*
 * This function is used in this file and in mxproc.c to generate consistent
 * RF CHIP VERSION string for logging on console and for storing the same
 * in proc/drivers/mxman_info/rf_chip_version file.
 */
int mxman_print_rf_hw_version(struct mxman *mxman, char *buf, const size_t bufsz)
{
	int r;

	r = snprintf(buf, bufsz, "RF_CHIP_VERSION: 0x%04x: %s (0x%02x), EVT%x.%x\n",
				  mxman->rf_hw_ver,
				  chip_version(mxman->rf_hw_ver), (mxman->rf_hw_ver & 0x00ff),
				  ((mxman->rf_hw_ver >> 12) & 0xfU), ((mxman->rf_hw_ver >> 8) & 0xfU));

	return r;
}

static void mxman_print_versions(struct mxman *mxman)
{
	char buf[80];

	memset(buf, '\0', sizeof(buf));

	(void)mxman_print_rf_hw_version(mxman, buf, sizeof(buf));

	SCSC_TAG_INFO(MXMAN, "%s", buf);
	SCSC_TAG_INFO(MXMAN, "WLBT FW: %s\n", mxman->fw_build_id);
	SCSC_TAG_INFO(MXMAN, "WLBT Driver: %d.%d.%d.%d.%d\n",
		SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION, SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_get_and_print_build_type();
#endif
}

/** Receive handler for messages from the FW along the maxwell management transport */
static void mxman_message_handler(const void *message, void *data)
{
	struct mxman        *mxman = (struct mxman *)data;

	/* Forward the message to the applicable service to deal with */
	const struct ma_msg_packet *msg = message;

	switch (msg->ma_msg) {
	case MM_START_IND:
		/* The arg can be used to determine the WLBT/S610 hardware revision */
		SCSC_TAG_INFO(MXMAN, "Received MM_START_IND message from the firmware, arg=0x%04x\n", msg->arg);
		mxman->rf_hw_ver = msg->arg;
		mxman_print_versions(mxman);
		atomic_inc(&mxman->boot_count);
		complete(&mxman->mm_msg_start_ind_completion);
		break;
	case MM_HALT_RSP:
		complete(&mxman->mm_msg_halt_rsp_completion);
		SCSC_TAG_INFO(MXMAN, "Received MM_HALT_RSP message from the firmware\n");
		break;
	case MM_LERNA_CONFIG:
		/* Message response to a firmware configuration query. */
		SCSC_TAG_INFO(MXMAN, "Received MM_LERNA_CONFIG message from firmware\n");
		scsc_lerna_response(message);
		break;
	case MM_SYSERR_IND:
		/* System Error report from firmware */
		SCSC_TAG_INFO(MXMAN, "Received MM_SYSERR_IND message from firmware\n");
		mx_syserr_handler(mxman, message);
		break;
	default:
		/* HERE: Unknown message, raise fault */
		SCSC_TAG_WARNING(MXMAN, "Received unknown message from the firmware: msg->ma_msg=%d\n", msg->ma_msg);
		break;
	}
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int mxman_is_memlog_valid(void)
{
	const char *desc_name = "WB_LOG";
	const char *obj_name = "drm-mem";
	struct memlog *desc = memlog_get_desc(desc_name);
	struct memlog_obj *obj;

	if (!desc)
		return 1;
		// treat this as fw is not loaded yet
	else
		obj = memlog_get_obj_by_name(desc, obj_name);

	if(!obj)
		return 0;
	else
		return 1;
}
#endif

/*
 * This function calulates and checks two or three (depending on crc32_over_binary flag)
 * crc32 values in the firmware header. The function will check crc32 over the firmware binary
 * (i.e. everything in the file following the header) only if the crc32_over_binary is set to 'true'.
 * This includes initialised data regions so it can be used to check when loading but will not be
 * meaningful once execution starts.
 */
static int do_fw_crc32_checks(char *fw, u32 fw_image_size, struct fwhdr *fwhdr, bool crc32_over_binary)
{
	int r;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	if (!mxman_is_memlog_valid()) {
		SCSC_TAG_ERR(MXMAN, "fw_crc_work_func failed by memlog API fail\n");
		return -ENOMEM;
	}
#endif

	if ((fwhdr->fw_crc32 == 0 || fwhdr->header_crc32 == 0 || fwhdr->const_crc32 == 0) && crc_check_allow_none == 0) {
		SCSC_TAG_ERR(MXMAN, "error: CRC is missing fw_crc32=%d header_crc32=%d crc_check_allow_none=%d\n",
			     fwhdr->fw_crc32, fwhdr->header_crc32, crc_check_allow_none);
		return -EINVAL;
	}

	if (fwhdr->header_crc32 == 0 && crc_check_allow_none == 1) {
		SCSC_TAG_INFO(MXMAN, "Skipping CRC check header_crc32=%d crc_check_allow_none=%d\n",
			      fwhdr->header_crc32, crc_check_allow_none);
	} else {
		/*
		 * CRC-32-IEEE of all preceding header fields (including other CRCs).
		 * Always the last word in the header.
		 */
		r = fwimage_check_fw_header_crc(fw, fwhdr->hdr_length, fwhdr->header_crc32);
		if (r) {
			SCSC_TAG_ERR(MXMAN, "fwimage_check_fw_header_crc() failed\n");
			return r;
		}
	}

	if (fwhdr->const_crc32 == 0 && crc_check_allow_none == 1) {
		SCSC_TAG_INFO(MXMAN, "Skipping CRC check const_crc32=%d crc_check_allow_none=%d\n",
			      fwhdr->const_crc32, crc_check_allow_none);
	} else {
		/*
		 * CRC-32-IEEE over the constant sections grouped together at start of firmware binary.
		 * This CRC should remain valid during execution. It can be used by run-time checker on
		 * host to detect firmware corruption (not all memory masters are subject to MPUs).
		 */
		r = fwimage_check_fw_const_section_crc(fw, fwhdr->const_crc32, fwhdr->const_fw_length, fwhdr->hdr_length);
		if (r) {
			SCSC_TAG_ERR(MXMAN, "fwimage_check_fw_const_section_crc() failed\n");
			return r;
		}
	}

	if (crc32_over_binary) {
		if (fwhdr->fw_crc32 == 0 && crc_check_allow_none == 1)
			SCSC_TAG_INFO(MXMAN, "Skipping CRC check fw_crc32=%d crc_check_allow_none=%d\n",
				      fwhdr->fw_crc32, crc_check_allow_none);
		else {
			/*
			 * CRC-32-IEEE over the firmware binary (i.e. everything
			 * in the file following this header).
			 * This includes initialised data regions so it can be used to
			 * check when loading but will not be meaningful once execution starts.
			 */
			r = fwimage_check_fw_crc(fw, fw_image_size, fwhdr->hdr_length, fwhdr->fw_crc32);
			if (r) {
				SCSC_TAG_ERR(MXMAN, "fwimage_check_fw_crc() failed\n");
				return r;
			}
		}
	}

	return 0;
}

static void fw_crc_wq_start(struct mxman *mxman)
{
	if (mxman->check_crc && crc_check_period_ms)
		queue_delayed_work(mxman->fw_crc_wq, &mxman->fw_crc_work, msecs_to_jiffies(crc_check_period_ms));
}

static void fw_crc_work_func(struct work_struct *work)
{
	int          r;
	struct mxman *mxman = container_of((struct delayed_work *)work, struct mxman, fw_crc_work);

	r = do_fw_crc32_checks(mxman->fw, mxman->fw_image_size, &mxman->fwhdr, false);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "do_fw_crc32_checks() failed r=%d\n", r);
		mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
		return;
	}
	fw_crc_wq_start(mxman);
}

static void fw_crc_wq_init(struct mxman *mxman)
{
	mxman->fw_crc_wq = create_singlethread_workqueue("fw_crc_wq");
	INIT_DELAYED_WORK(&mxman->fw_crc_work, fw_crc_work_func);
}

static void fw_crc_wq_stop(struct mxman *mxman)
{
	mxman->check_crc = false;
	cancel_delayed_work(&mxman->fw_crc_work);
	flush_workqueue(mxman->fw_crc_wq);
}

static void fw_crc_wq_deinit(struct mxman *mxman)
{
	fw_crc_wq_stop(mxman);
	destroy_workqueue(mxman->fw_crc_wq);
}

static int transports_init(struct mxman *mxman)
{
	struct mxconf  *mxconf;
	int            r;
	struct scsc_mx *mx = mxman->mx;

	/* Initialise mx management stack */
	r = mxmgmt_transport_init(scsc_mx_get_mxmgmt_transport(mx), mx);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxmgmt_transport_init() failed %d\n", r);
		return r;
	}

	/* Initialise gdb transport for cortex-R4 */
	r = gdb_transport_init(scsc_mx_get_gdb_transport_r4(mx), mx, GDB_TRANSPORT_R4);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "gdb_transport_init() failed %d\n", r);
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}

	/* Initialise gdb transport for cortex-M4 */
	r = gdb_transport_init(scsc_mx_get_gdb_transport_m4(mx), mx, GDB_TRANSPORT_M4);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "gdb_transport_init() failed %d\n", r);
		gdb_transport_release(scsc_mx_get_gdb_transport_r4(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	/* Initialise gdb transport for cortex-M4 */
	r = gdb_transport_init(scsc_mx_get_gdb_transport_m4_1(mx), mx, GDB_TRANSPORT_M4_1);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "gdb_transport_init() failed %d\n", r);
		gdb_transport_release(scsc_mx_get_gdb_transport_r4(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}
#endif

	/* Initialise mxlog transport */
	r = mxlog_transport_init(scsc_mx_get_mxlog_transport(mx), mx);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxlog_transport_init() failed %d\n", r);
		gdb_transport_release(scsc_mx_get_gdb_transport_m4(mx));
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		gdb_transport_release(scsc_mx_get_gdb_transport_m4_1(mx));
#endif
		gdb_transport_release(scsc_mx_get_gdb_transport_r4(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}

	/*
	 * Allocate & Initialise Infrastructre Config Structure
	 * including the mx management stack config information.
	 */
	mxconf = miframman_alloc(scsc_mx_get_ramman(mx), sizeof(struct mxconf), 4, MIFRAMMAN_OWNER_COMMON);
	if (!mxconf) {
		SCSC_TAG_ERR(MXMAN, "miframman_alloc() failed\n");
		gdb_transport_release(scsc_mx_get_gdb_transport_m4(mx));
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		gdb_transport_release(scsc_mx_get_gdb_transport_m4_1(mx));
#endif
		gdb_transport_release(scsc_mx_get_gdb_transport_r4(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		mxlog_transport_release(scsc_mx_get_mxlog_transport(mx));
		return -ENOMEM;
	}
	mxman->mxconf = mxconf;
	mxconf->magic = MXCONF_MAGIC;
	mxconf->version.major = MXCONF_VERSION_MAJOR;

#ifdef CONFIG_SOC_EXYNOS7885
	SCSC_TAG_DEBUG(MXMAN, "exynos_soc_info.revision=%d\n", exynos_soc_info.revision);
	mxconf->soc_revision = exynos_soc_info.revision;
#endif
	mxconf->version.minor = MXCONF_VERSION_MINOR;
	/* Pass pre-existing FM status to FW */
	mxconf->flags = 0;
#ifdef CONFIG_SCSC_FM
	mxconf->flags |= is_fm_on ? MXCONF_FLAGS_FM_ON : 0;
#endif
	SCSC_TAG_INFO(MXMAN, "mxconf flags 0x%08x\n", mxconf->flags);

	/* serialise mxmgmt transport */
	mxmgmt_transport_config_serialise(scsc_mx_get_mxmgmt_transport(mx), &mxconf->mx_trans_conf);
	/* serialise Cortex-R4 gdb transport */
	gdb_transport_config_serialise(scsc_mx_get_gdb_transport_r4(mx), &mxconf->mx_trans_conf_gdb_r4);
	/* serialise Cortex-M4 gdb transport */
	gdb_transport_config_serialise(scsc_mx_get_gdb_transport_m4(mx), &mxconf->mx_trans_conf_gdb_m4);

	/* Default to Fleximac M4_1 monitor channel not in use.
	 * Allows CONFIG_SCSC_MX450_GDB_SUPPORT to be turned off in Kconfig even though mxconf
	 * struct v5 defines M4_1 channel
	 */
	mxconf->mx_trans_conf_gdb_m4_1.from_ap_stream_conf.buf_conf.buffer_loc = 0;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	/* serialise Cortex-M4 gdb transport */
	gdb_transport_config_serialise(scsc_mx_get_gdb_transport_m4_1(mx), &mxconf->mx_trans_conf_gdb_m4_1);
#endif
	/* serialise mxlog transport */
	mxlog_transport_config_serialise(scsc_mx_get_mxlog_transport(mx), &mxconf->mxlogconf);
	SCSC_TAG_DEBUG(MXMAN, "read_bit_idx=%d write_bit_idx=%d buffer=%p num_packets=%d packet_size=%d read_index=%d write_index=%d\n",
		       scsc_mx_get_mxlog_transport(mx)->mif_stream.read_bit_idx,
		       scsc_mx_get_mxlog_transport(mx)->mif_stream.write_bit_idx,
		       scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.buffer,
		       scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.num_packets,
		       scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.packet_size,
		       *scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.read_index,
		       *scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.write_index
		      );

	/* Need to initialise fwconfig or else random data can make firmware data abort. */
	mxconf->fwconfig.offset = 0;
	mxconf->fwconfig.size = 0;
#ifdef CONFIG_SCSC_COMMON_HCF
	/* Load Common Config HCF */
	mxfwconfig_load(mxman->mx, &mxconf->fwconfig);
#endif
	return 0;
}

static void transports_release(struct mxman *mxman)
{
	mxlog_transport_release(scsc_mx_get_mxlog_transport(mxman->mx));
	mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mxman->mx));
	gdb_transport_release(scsc_mx_get_gdb_transport_r4(mxman->mx));
	gdb_transport_release(scsc_mx_get_gdb_transport_m4(mxman->mx));
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	gdb_transport_release(scsc_mx_get_gdb_transport_m4_1(mxman->mx));
#endif
	miframman_free(scsc_mx_get_ramman(mxman->mx), mxman->mxconf);
}

static void mbox_init(struct mxman *mxman, u32 firmware_entry_point)
{
	u32                 *mbox0;
	u32                 *mbox1;
	u32                 *mbox2;
	u32                 *mbox3;
	scsc_mifram_ref     mifram_ref;
	struct scsc_mx      *mx = mxman->mx;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);

	/* Place firmware entry address in MIF MBOX 0 so R4 ROM knows where to jump to! */
	mbox0 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_0);
	mbox1 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_1);

	/* Write (and flush) entry point to MailBox 0, config address to MBOX 1 */
	*mbox0 = firmware_entry_point;
	mif->get_mifram_ref(mif, mxman->mxconf, &mifram_ref);
	*mbox1 = mifram_ref; /* must be R4-relative address here */

	/*
	 * write the magic number "0xbcdeedcb" to MIF Mailbox #2 &
	 * copy the firmware_startup_flags to MIF Mailbox #3 before starting (reset = 0) the R4
	 */
	mbox2 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_2);
	*mbox2 = MBOX2_MAGIC_NUMBER;
	mbox3 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_3);
	*mbox3 = firmware_startup_flags;

	/* CPU memory barrier */
	smp_wmb();
}

static int fwhdr_init(char *fw, struct fwhdr *fwhdr, bool *fwhdr_parsed_ok, bool *check_crc)
{
	/*
	 * Validate the fw image including checking the firmware header, majic #, version, checksum  so on
	 * then do CRC on the entire image
	 *
	 * Derive some values from header -
	 *
	 * PORT: assumes little endian
	 */
	if (skip_header)
		*fwhdr_parsed_ok = false; /* Allows the forced start address to be used */
	else
		*fwhdr_parsed_ok = fwhdr_parse(fw, fwhdr);
	*check_crc = false;
	if (*fwhdr_parsed_ok) {
		SCSC_TAG_INFO(MXMAN, "FW HEADER version: hdr_major: %d hdr_minor: %d\n", fwhdr->hdr_major, fwhdr->hdr_minor);
		switch (fwhdr->hdr_major) {
		case 0:
			switch (fwhdr->hdr_minor) {
			case 2:
				*check_crc = true;
				break;
			default:
				SCSC_TAG_ERR(MXMAN, "Unsupported FW HEADER version: hdr_major: %d hdr_minor: %d\n",
					fwhdr->hdr_major, fwhdr->hdr_minor);
				return -EINVAL;
			}
			break;
		case 1:
			*check_crc = true;
			break;
		default:
			SCSC_TAG_ERR(MXMAN, "Unsupported FW HEADER version: hdr_major: %d hdr_minor: %d\n",
				fwhdr->hdr_major, fwhdr->hdr_minor);
			return -EINVAL;
		}
		switch (fwhdr->fwapi_major) {
		case 0:
			switch (fwhdr->fwapi_minor) {
			case 2:
				SCSC_TAG_INFO(MXMAN, "FWAPI version: fwapi_major: %d fwapi_minor: %d\n",
					fwhdr->fwapi_major, fwhdr->fwapi_minor);
				break;
			default:
				SCSC_TAG_ERR(MXMAN, "Unsupported FWAPI version: fwapi_major: %d fwapi_minor: %d\n",
					fwhdr->fwapi_major, fwhdr->fwapi_minor);
				return -EINVAL;
			}
			break;
		default:
			SCSC_TAG_ERR(MXMAN, "Unsupported FWAPI version: fwapi_major: %d fwapi_minor: %d\n",
				fwhdr->fwapi_major, fwhdr->fwapi_minor);
			return -EINVAL;
		}
	} else {
		/* This is unidetified pre-header firmware - assume it is built to run at 0xb8000000 == 0 for bootrom */
		if (allow_unidentified_firmware) {
			SCSC_TAG_INFO(MXMAN, "Unidentified firmware override\n");
			fwhdr->firmware_entry_point = 0;
			fwhdr->fw_runtime_length = MX_FW_RUNTIME_LENGTH;
		} else {
			SCSC_TAG_ERR(MXMAN, "Unidentified firmware is not allowed\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int fw_init(struct mxman *mxman, void *start_dram, size_t size_dram, bool *fwhdr_parsed_ok)
{
	int                 r;
	char                *build_id;
	char                *ttid;
	u32                 fw_image_size;
	struct fwhdr        *fwhdr = &mxman->fwhdr;
	char                *fw = start_dram;

	r = mx140_file_download_fw(mxman->mx, start_dram, size_dram, &fw_image_size);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mx140_file_download_fw() failed (%d)\n", r);
		return r;
	}

	r = fwhdr_init(fw, fwhdr, fwhdr_parsed_ok, &mxman->check_crc);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "fwhdr_init() failed\n");
		return r;
	}
	mxman->fw = fw;
	mxman->fw_image_size = fw_image_size;
	if (mxman->check_crc) {
		/* do CRC on the entire image */
		r = do_fw_crc32_checks(fw, fw_image_size, &mxman->fwhdr, true);
		if (r) {
			SCSC_TAG_ERR(MXMAN, "do_fw_crc32_checks() failed\n");
			return r;
		}
		fw_crc_wq_start(mxman);
	}

	if (*fwhdr_parsed_ok) {
		build_id = fwhdr_get_build_id(fw, fwhdr);
		if (build_id) {
			struct slsi_kic_service_info kic_info;

			(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), "%s", build_id);
			SCSC_TAG_INFO(MXMAN, "Firmware BUILD_ID: %s\n", mxman->fw_build_id);
			memcpy(saved_fw_build_id, mxman->fw_build_id,
			       sizeof(saved_fw_build_id));

			(void) snprintf(kic_info.ver_str,
					min(sizeof(mxman->fw_build_id), sizeof(kic_info.ver_str)),
					"%s", mxman->fw_build_id);
			kic_info.fw_api_major = fwhdr->fwapi_major;
			kic_info.fw_api_minor = fwhdr->fwapi_minor;
			kic_info.release_product = SCSC_RELEASE_PRODUCT;
			kic_info.host_release_iteration = SCSC_RELEASE_ITERATION;
			kic_info.host_release_candidate = SCSC_RELEASE_CANDIDATE;

			slsi_kic_service_information(slsi_kic_technology_type_common, &kic_info);
		} else
			SCSC_TAG_ERR(MXMAN, "Failed to get Firmware BUILD_ID\n");

		ttid = fwhdr_get_ttid(fw, fwhdr);
		if (ttid) {
			(void)snprintf(mxman->fw_ttid, sizeof(mxman->fw_ttid), "%s", ttid);
			SCSC_TAG_INFO(MXMAN, "Firmware ttid: %s\n", mxman->fw_ttid);
		}
	}

	SCSC_TAG_DEBUG(MXMAN, "firmware_entry_point=0x%x fw_runtime_length=%d\n", fwhdr->firmware_entry_point, fwhdr->fw_runtime_length);

	return 0;

}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
struct memlog_obj *mxman_get_memlog_obj(struct scsc_mif_abs *mif, const char *desc_name)
{
	struct device *dev = mif->get_mif_device(mif);
	struct memlog *desc = memlog_get_desc(desc_name);
	struct memlog_obj *obj = NULL;
	const char *obj_name = "drm-mem";

	if (!desc) {
		int val = memlog_register(desc_name, dev, &desc);

		if (!val) {
			/* callback can be registered in each driver (optional) */
			// desc->ops.file_ops_completed = file_ops_completed;
			// desc->ops.log_status_notify = log_status_notify;
			// desc->ops.log_level_notify = log_level_notify;
			// desc->ops.log_enable_notify = log_enable_notify;
		} else {
			/* error handling */
		}

		/* MX_DRAM_SIZE_SECTION_1 = 8MB */
		obj = memlog_alloc_direct(desc, MX_DRAM_SIZE_SECTION_2, NULL, obj_name);
		if (!obj) {
			/* Alloc fail */
			SCSC_TAG_INFO(MXMAN, "obj alloc failed!!\n");
		}
	} else {
		obj = memlog_get_obj_by_name(desc, obj_name);
	}
	return obj;
}

static void mxman_set_memlog_version(struct scsc_mif_abs *mif)
{
	struct memlog *desc = memlog_get_desc("WB_LOG");
	struct memlog_obj *scsc_memlog_version_info_obj;

	struct scsc_memlog_version_info {
		char fw_version[128];
		char host_version[64];
		char fapi_version[64];
	} *memlog_version_info;


	if (desc) {
		const char *fapi_version = "ma:14.1, mlme:14.6, debug:13.3, test:14.0";

		scsc_memlog_version_info_obj = memlog_get_obj_by_name(desc, "str-mem");
		if (!scsc_memlog_version_info_obj) {
			scsc_memlog_version_info_obj = memlog_alloc_array(
				desc, 1, sizeof(struct scsc_memlog_version_info),
				NULL, "str-mem", "scsc_memlog_version_info", 0);

			if (!scsc_memlog_version_info_obj) {
				/* Alloc fail */
				return;
			}
		}

		memlog_version_info = (struct scsc_memlog_version_info *)scsc_memlog_version_info_obj->vaddr;
		mxman_get_fw_version(memlog_version_info->fw_version, SCSC_LOG_FW_VERSION_SIZE);
		mxman_get_driver_version(memlog_version_info->host_version, SCSC_LOG_HOST_VERSION_SIZE);
		memcpy(memlog_version_info->fapi_version, fapi_version, SCSC_LOG_FAPI_VERSION_SIZE);
	}
}
#endif

static int mxman_start(struct mxman *mxman)
{
	void                *start_dram;
	void                *start_dram_section2;
	size_t              size_dram = MX_DRAM_SIZE;
	struct scsc_mif_abs *mif;
	struct fwhdr        *fwhdr = &mxman->fwhdr;
	bool                fwhdr_parsed_ok;
	void                *start_mifram_heap;
	u32                 length_mifram_heap;
	u32                 length_mifram_heap2;
	int                 r;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	const char          *desc_name = "WB_LOG";
	struct memlog_obj   *obj;
	struct device       *dev;
#endif

	mif = scsc_mx_get_mif_abs(mxman->mx);
	if (mxman_check_reset_failed(mif)) {
		struct timeval tval = ns_to_timeval(reset_failed_time);

		SCSC_TAG_ERR(MXMAN, "previous reset failed at [%6lu.%06ld], ignoring\n", tval.tv_sec, tval.tv_usec);
		return -EIO;
	}

	(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), "unknown");

	/* If the option is set to skip header, we must allow unidentified f/w */
	if (skip_header) {
		SCSC_TAG_INFO(MXMAN, "Ignoring firmware header block\n");
		allow_unidentified_firmware = true;
	}

	mif = scsc_mx_get_mif_abs(mxman->mx);
	start_dram = mif->map(mif, &size_dram);

	if (!start_dram) {
		SCSC_TAG_ERR(MXMAN, "Error allocating dram\n");
		return -ENOMEM;
	}

	SCSC_TAG_INFO(MXMAN, "Allocated %zu bytes\n", size_dram);

#ifdef CONFIG_SCSC_CHV_SUPPORT
	if (chv_run)
		allow_unidentified_firmware = true;
	/* Set up chv arguments. */

#endif

	mxman->start_dram = start_dram;

	r = fw_init(mxman, start_dram, size_dram, &fwhdr_parsed_ok);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "fw_init() failed\n");
		mif->unmap(mif, mxman->start_dram);
		return r;
	}

	/* ABox reserved at end so adjust length - round to multiple of PAGE_SIZE */
	length_mifram_heap2 = MX_DRAM_SIZE_SECTION_2
		- ((sizeof(struct scsc_bt_audio_abox) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	dev = mif->get_mif_device(mif);
	obj = mxman_get_memlog_obj(mif, desc_name);

	/* WLBT fw update is needed only use 8MB if mxlogger is disabled */
	/* After that, we can remove below IF block */
	if (!obj) {
		SCSC_TAG_ERR(MXMAN, "memlog erro\n");
		fw_crc_wq_stop(mxman);
		mif->unmap(mif, mxman->start_dram);
		return -ENOMEM;
	}

	if (obj) {
		mxman_set_memlog_version(mif);

		/* do the new BAAW mapings */
		r = mif->set_mem_region2(mif, obj->vaddr, MXL_POOL_SZ);

		start_dram_section2 = (char *)obj->vaddr;
		miframman_init(scsc_mx_get_ramman2(mxman->mx),
			start_dram_section2,
			length_mifram_heap2,
			start_dram_section2);
		miframabox_init(scsc_mx_get_aboxram(mxman->mx), start_dram_section2 + length_mifram_heap2);
	}
#else
	start_dram_section2 = (char *)start_dram + MX_DRAM_SIZE_SECTION_1;
	miframman_init(scsc_mx_get_ramman2(mxman->mx),
		start_dram_section2,
		length_mifram_heap2,
		start_dram_section2);
	miframabox_init(scsc_mx_get_aboxram(mxman->mx), start_dram_section2 + length_mifram_heap2);
#endif
	/* set up memory protection (read only) from start_dram to start_dram+fw_length
	 * rounding up the size if required
	 */
	start_mifram_heap = (char *)start_dram + fwhdr->fw_runtime_length;
	length_mifram_heap = MX_DRAM_SIZE_SECTION_1 - fwhdr->fw_runtime_length;

	miframman_init(scsc_mx_get_ramman(mxman->mx), start_mifram_heap, length_mifram_heap, start_dram);
	mifmboxman_init(scsc_mx_get_mboxman(mxman->mx));
	mifintrbit_init(scsc_mx_get_intrbit(mxman->mx), mif);
	mxfwconfig_init(mxman->mx);

	/* Initialise transports */
	r = transports_init(mxman);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "transports_init() failed\n");
		fw_crc_wq_stop(mxman);
		mifintrbit_deinit(scsc_mx_get_intrbit(mxman->mx));
		miframman_deinit(scsc_mx_get_ramman(mxman->mx));
		miframman_deinit(scsc_mx_get_ramman2(mxman->mx));
		miframabox_deinit(scsc_mx_get_aboxram(mxman->mx));
		mifmboxman_deinit(scsc_mx_get_mboxman(mxman->mx));
		/* Release the MIF memory resources */
		mif->unmap(mif, mxman->start_dram);
		return r;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	mif->recovery_disabled_reg(mif, mxman_recovery_disabled);
#endif
	mbox_init(mxman, fwhdr->firmware_entry_point);
	init_completion(&mxman->mm_msg_start_ind_completion);
	init_completion(&mxman->mm_msg_halt_rsp_completion);
	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxman->mx),
						MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
						&mxman_message_handler, mxman);

	mxlog_init(scsc_mx_get_mxlog(mxman->mx), mxman->mx, mxman->fw_build_id);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	mxlogger_init(mxman->mx, scsc_mx_get_mxlogger(mxman->mx), MXL_POOL_SZ);
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Register minimoredump  client */
	mini_moredump_client.prv = mxman;
	scsc_log_collector_register_client(&mini_moredump_client);
#endif
#ifdef CONFIG_SCSC_SMAPPER
	/* Initialize SMAPPER */
	mifsmapper_init(scsc_mx_get_smapper(mxman->mx), mif);
#endif
#ifdef CONFIG_SCSC_QOS
	mifqos_init(scsc_mx_get_qos(mxman->mx), mif);
#endif
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
	scsc_log_in_dram_mmap_create();
#endif
#ifdef CONFIG_SCSC_CHV_SUPPORT
	if (chv_run) {
		int i;

		u32 *p = (u32 *)((u8 *)start_dram + SCSC_CHV_ARGV_ADDR_OFFSET);

		if (chv_argc == 0) {
			/*
			 * Setup the chv f/w arguments.
			 * Argument of 0 means run once (driver never set this).
			 * Argument of 1 means run forever.
			 */
			SCSC_TAG_INFO(MXMAN, "Setting up CHV arguments: start_dram=%p arg=%p, chv_run=%d\n", start_dram, p, chv_run);
			*p++ = 1;                    /* argc */
			*p++ = chv_run == 1 ? 0 : 1; /* arg */
		} else {
			/* Pass separate args */
			*p++ = chv_argc;  /* argc */
			SCSC_TAG_INFO(MXMAN, "Setting up additional CHV args: chv_argc = %d\n", chv_argc);

			for (i = 0; i < chv_argc; i++) {
				SCSC_TAG_INFO(MXMAN, "Setting up additional CHV args: chv_argv[%d]: *(%p) = 0x%x\n", i, p, (u32)chv_argv[i]);
				*p++ = (u32)chv_argv[i]; /* arg */
			}
		}
	}
#endif
	mxproc_create_ctrl_proc_dir(&mxman->mxproc, mxman);
	panicmon_init(scsc_mx_get_panicmon(mxman->mx), mxman->mx);

	/* Change state to STARTING to allow coredump as we come out of reset */
	mxman->mxman_state = MXMAN_STATE_STARTING;

	/* release Maxwell from reset */

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	mif->set_memlog_paddr(mif, obj->paddr);
#endif
	r = mif->reset(mif, 0);
	if (r) {
		mxman_set_reset_failed();
		SCSC_TAG_INFO(MXMAN, "HW reset deassertion failed\n");

		/* Save log at point of failure */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_schedule_collection(SCSC_LOG_HOST_COMMON, SCSC_LOG_HOST_COMMON_REASON_START);
#else
		mx140_log_dump();
#endif
	}
	if (fwhdr_parsed_ok) {
		r = wait_for_mm_msg_start_ind(mxman);
		if (r) {
			SCSC_TAG_ERR(MXMAN, "wait_for_MM_START_IND() failed: r=%d\n", r);
			print_mailboxes(mxman);
			if (skip_mbox0_check) {
				SCSC_TAG_ERR(MXMAN, "timeout ignored in skip_mbox0_check mode\n");
				return 0;
			}
			mxman_stop(mxman);
			return r;
		}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_start(scsc_mx_get_mxlogger(mxman->mx));
#endif
	} else {
		msleep(WAIT_FOR_FW_TO_START_DELAY_MS);
	}

	return 0;
}

static bool is_bug_on_enabled(struct scsc_mx *mx)
{
	bool bug_on_enabled;
	const struct firmware *firm;
	int r;

	if ((memdump == 3) && (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY))
		bug_on_enabled = true;
	else
		bug_on_enabled = false;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	(void)firm; /* unused */
	(void)r; /* unused */
	goto out;
#else
	/* non SABLE platforms should also follow /sys/wifi/memdump if enabled */
	if (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY)
		goto out;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	/* for legacy platforms (including Andorid P) using .memdump.info */
#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION >= 9)
	#define MX140_MEMDUMP_INFO_FILE	"/data/vendor/conn/.memdump.info"
#else
	#define MX140_MEMDUMP_INFO_FILE	"/data/misc/conn/.memdump.info"
#endif

	SCSC_TAG_INFO(MX_FILE, "Loading %s file\n", MX140_MEMDUMP_INFO_FILE);
	r = mx140_request_file(mx, MX140_MEMDUMP_INFO_FILE, &firm);
	if (r) {
		SCSC_TAG_WARNING(MX_FILE, "Error Loading %s file %d\n", MX140_MEMDUMP_INFO_FILE, r);
		return bug_on_enabled;
	}
	if (firm->size < sizeof(char))
		SCSC_TAG_WARNING(MX_FILE, "file is too small\n");
	else if (*firm->data == '3')
		bug_on_enabled = true;
	mx140_release_file(mx, firm);
#endif //(LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#endif //CONFIG_SCSC_LOG_COLLECTION
out:
	SCSC_TAG_INFO(MX_FILE, "bug_on_enabled %d\n", bug_on_enabled);
	return bug_on_enabled;
}

static void print_panic_code_legacy(u16 code)
{
	u16 tech = code & SCSC_PANIC_TECH_MASK;
	u16 origin = code & SCSC_PANIC_ORIGIN_MASK;

	SCSC_TAG_INFO(MXMAN, "Decoding panic code=0x%x:\n", code);
	switch (origin) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic origin\n");
		break;
	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_ORIGIN_FW\n");
		break;
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_ORIGIN_HOST\n");
		break;
	}

	switch (tech) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic technology\n");
		break;
	case SCSC_PANIC_TECH_WLAN:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_WLAN\n");
		break;
	case SCSC_PANIC_TECH_CORE:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_CORE\n");
		break;
	case SCSC_PANIC_TECH_BT:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_BT\n");
		break;
	case SCSC_PANIC_TECH_UNSP:
		SCSC_TAG_INFO(MXMAN, "PANIC_TECH_UNSP\n");
		break;
	}
	SCSC_TAG_INFO(MXMAN, "panic subcode=0x%x\n", code & SCSC_PANIC_SUBCODE_MASK_LEGACY);
}

static void print_panic_code(u16 code)
{
	u16 origin = code & SCSC_PANIC_ORIGIN_MASK;	/* Panic origin (host/fw) */
	u16 subcode = code & SCSC_PANIC_SUBCODE_MASK;	/* The panic code */

	SCSC_TAG_INFO(MXMAN, "Decoding panic code=0x%x:\n", code);
	SCSC_TAG_INFO(MXMAN, "panic subcode=0x%x\n", code & SCSC_PANIC_SUBCODE_MASK);

	switch (origin) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic origin\n");
		break;
	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "WLBT FW PANIC: 0x%02x\n", subcode);
		break;
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "WLBT HOST detected FW failure, service:\n");
		switch (subcode >> SCSC_SYSERR_HOST_SERVICE_SHIFT) {
		case SCSC_SERVICE_ID_WLAN:
			SCSC_TAG_INFO(MXMAN, " WLAN\n");
			break;
		case SCSC_SERVICE_ID_BT:
			SCSC_TAG_INFO(MXMAN, " BT\n");
			break;
		case SCSC_SERVICE_ID_ANT:
			SCSC_TAG_INFO(MXMAN, " ANT\n");
			break;
		case SCSC_SERVICE_ID_CLK20MHZ:
			SCSC_TAG_INFO(MXMAN, " CLK20MHZ\n");
			break;
		default:
			SCSC_TAG_INFO(MXMAN, " Service 0x%x\n", subcode);
			break;
		}
		break;
	}
}

/**
 * Print the last panic record collected to aid in post mortem.
 *
 * Helps when all we have is kernel log showing WLBT failed some time ago
 *
 * Only prints the R4 record
 */
void mxman_show_last_panic(struct mxman *mxman)
{
	u32 r4_panic_record_length = 0;	/* in u32s */
	u32 r4_panic_stack_record_length = 0;	/* in u32s */

	/* Any valid panic? */
	if (mxman->scsc_panic_code == 0)
		return;

	SCSC_TAG_INFO(MXMAN, "\n\n--- DETAILS OF LAST WLBT FAILURE ---\n\n");

	switch (mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK) {
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "Last panic was host induced:\n");
		break;

	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "Last panic was FW:\n");
		fw_parse_r4_panic_record(mxman->last_panic_rec_r, &r4_panic_record_length, NULL, true);
		fw_parse_r4_panic_stack_record(mxman->last_panic_stack_rec_r, &r4_panic_stack_record_length, true);
		break;

	default:
		SCSC_TAG_INFO(MXMAN, "Last panic unknown origin %d\n", mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK);
		break;
	}

	print_panic_code(mxman->scsc_panic_code);

	SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", disable_recovery_handling ? "off" : "on");

	if (mxman_recovery_disabled()) {
		/* Labour the point that a reboot is needed when autorecovery is disabled */
		SCSC_TAG_INFO(MXMAN, "\n\n*** HANDSET REBOOT NEEDED TO RESTART WLAN AND BT ***\n\n");
	}

	SCSC_TAG_INFO(MXMAN, "\n\n--- END DETAILS OF LAST WLBT FAILURE ---\n\n");
}

static void process_panic_record(struct mxman *mxman, bool dump)
{
	u32 *r4_panic_record = NULL;
	u32 *r4_panic_stack_record = NULL;
	u32 *m4_panic_record = NULL;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 *m4_1_panic_record = NULL;
#endif
	u32 r4_panic_record_length = 0;	/* in u32s */
	u32 r4_panic_stack_record_offset = 0; /* in bytes */
	u32 r4_panic_stack_record_length = 0;	/* in u32s */
	u32 m4_panic_record_length = 0; /* in u32s */
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 m4_1_panic_record_length = 0; /* in u32s */
#endif
	u32 full_panic_code = 0;
	bool r4_panic_record_ok = false;
	bool r4_panic_stack_record_ok = false;
	bool m4_panic_record_ok = false;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	bool m4_1_panic_record_ok = false;
#endif
	bool r4_sympathetic_panic_flag = false;
	bool m4_sympathetic_panic_flag = false;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	bool m4_1_sympathetic_panic_flag = false;
#endif

	/* some configurable delay before accessing the panic record */
	msleep(panic_record_delay);

	/*
	* Check if the panic was trigered by MX and set the subcode if so.
	*/
	if ((mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK) == SCSC_PANIC_ORIGIN_FW) {
		if (mxman->fwhdr.r4_panic_record_offset) {
			r4_panic_record = (u32 *)(mxman->fw + mxman->fwhdr.r4_panic_record_offset);
			r4_panic_record_ok = fw_parse_r4_panic_record(r4_panic_record, &r4_panic_record_length,
								      &r4_panic_stack_record_offset, dump);
		} else {
			SCSC_TAG_INFO(MXMAN, "R4 panic record doesn't exist in the firmware header\n");
		}
		if (mxman->fwhdr.m4_panic_record_offset) {
			m4_panic_record = (u32 *)(mxman->fw + mxman->fwhdr.m4_panic_record_offset);
			m4_panic_record_ok = fw_parse_m4_panic_record(m4_panic_record, &m4_panic_record_length, dump);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		} else if (mxman->fwhdr.m4_1_panic_record_offset) {
			m4_1_panic_record = (u32 *)(mxman->fw + mxman->fwhdr.m4_1_panic_record_offset);
			m4_1_panic_record_ok = fw_parse_m4_panic_record(m4_1_panic_record, &m4_1_panic_record_length, dump);
#endif
		} else {
			SCSC_TAG_INFO(MXMAN, "M4 panic record doesn't exist in the firmware header\n");
		}

		/* Extract and print the panic code */
		switch (r4_panic_record_length) {
		default:
			SCSC_TAG_WARNING(MXMAN, "Bad panic record length/subversion\n");
			break;
		case SCSC_R4_V2_MINOR_52:
			if (r4_panic_record_ok) {
				full_panic_code = r4_panic_record[2];
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & full_panic_code;
			} else if (m4_panic_record_ok)
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_panic_record[2];
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
			else if (m4_1_panic_record_ok)
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_1_panic_record[2];
#endif
			/* Set unspecified technology for now */
			mxman->scsc_panic_code |= SCSC_PANIC_TECH_UNSP;
			print_panic_code_legacy(mxman->scsc_panic_code);
			break;
		case SCSC_R4_V2_MINOR_54:
		case SCSC_R4_V2_MINOR_53:
			if (r4_panic_record_ok) {
				/* Save the last R4 panic record for future display */
				BUG_ON(sizeof(mxman->last_panic_rec_r) < r4_panic_record_length * sizeof(u32));
				memcpy((u8 *)mxman->last_panic_rec_r, (u8 *)r4_panic_record, r4_panic_record_length * sizeof(u32));
				mxman->last_panic_rec_sz = r4_panic_record_length;

				r4_sympathetic_panic_flag = fw_parse_get_r4_sympathetic_panic_flag(r4_panic_record);
				if (dump)
					SCSC_TAG_INFO(MXMAN, "r4_panic_record_ok=%d r4_sympathetic_panic_flag=%d\n",
							r4_panic_record_ok,
							r4_sympathetic_panic_flag);
				/* Check panic stack if present */
				if (r4_panic_record_length >= SCSC_R4_V2_MINOR_54) {
					r4_panic_stack_record = (u32 *)(mxman->fw + r4_panic_stack_record_offset);
					r4_panic_stack_record_ok = fw_parse_r4_panic_stack_record(r4_panic_stack_record, &r4_panic_stack_record_length, dump);
				} else {
					r4_panic_stack_record_ok = false;
					r4_panic_stack_record_length = 0;
				}
				if (r4_sympathetic_panic_flag == false) {
					/* process R4 record */
					full_panic_code = r4_panic_record[3];
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & full_panic_code;
					if (dump)
						print_panic_code(mxman->scsc_panic_code);
					break;
				}
			}
			if (m4_panic_record_ok) {
				m4_sympathetic_panic_flag = fw_parse_get_m4_sympathetic_panic_flag(m4_panic_record);
				if (dump)
					SCSC_TAG_INFO(MXMAN, "m4_panic_record_ok=%d m4_sympathetic_panic_flag=%d\n",
							m4_panic_record_ok,
							m4_sympathetic_panic_flag);
				if (m4_sympathetic_panic_flag == false) {
					/* process M4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_panic_record[3];
				} else if (r4_panic_record_ok) {
					/* process R4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & r4_panic_record[3];
				}
				if (dump)
					print_panic_code(mxman->scsc_panic_code);
			}
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT /* this is wrong but not sure what is "right" */
/* "sympathetic panics" are not really a thing on the Neus architecture unless */
/* generated by the host                                                       */
			if (m4_1_panic_record_ok) {
				m4_1_sympathetic_panic_flag = fw_parse_get_m4_sympathetic_panic_flag(m4_1_panic_record);
				if (dump) {
					SCSC_TAG_DEBUG(MXMAN, "m4_1_panic_record_ok=%d m4_1_sympathetic_panic_flag=%d\n",
							m4_1_panic_record_ok,
							m4_1_sympathetic_panic_flag);
				}
				if (m4_1_sympathetic_panic_flag == false) {
					/* process M4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_SUBCODE_MASK & m4_1_panic_record[3];
				} else if (r4_panic_record_ok) {
					/* process R4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_SUBCODE_MASK & r4_panic_record[3];
				}
				if (dump)
					print_panic_code(mxman->scsc_panic_code);
			}
#endif
			break;
		}
	}
	if (r4_panic_record_ok) {
		/* Populate syserr info with panic equivalent, but don't modify level  */
		mxman->last_syserr.subsys = (u8) ((full_panic_code >> SYSERR_SUB_SYSTEM_POSN) & SYSERR_SUB_SYSTEM_MASK);
		mxman->last_syserr.type = (u8) ((full_panic_code >> SYSERR_TYPE_POSN) & SYSERR_TYPE_MASK);
		mxman->last_syserr.subcode = (u16) ((full_panic_code >> SYSERR_SUB_CODE_POSN) & SYSERR_SUB_CODE_MASK);
	}
}

/* Check whether syserr should be promoted based on frequency or service driver override */
static void mxman_check_promote_syserr(struct mxman *mxman)
{
	int i;
	int entry = -1;
	unsigned long now = jiffies;

	/* We use 0 as a NULL timestamp so avoid this */
	now = (now) ? now : 1;

	/* Promote all L7 to L8 to maintain existing moredump scheme,
	 * unless code is found in the filter list
	 */
	if (mxman->last_syserr.level == MX_SYSERR_LEVEL_7) {
		u8 new_level = MX_SYSERR_LEVEL_7;
		for (i = 0; i < ARRAY_SIZE(mxfwconfig_syserr_no_promote); i++) {
			/* End of list reached without match, promote to L8 by default */
			if (mxfwconfig_syserr_no_promote[i] == 0) {
				new_level = MX_SYSERR_LEVEL_8;
				entry = i;
				break;
			}

			/* If 0xFFFFFFFF in list: only if host induced, promote to L8 */
			if (mxfwconfig_syserr_no_promote[i] == 0xFFFFFFFF) {
				if ((mxman->last_syserr.subsys == SYSERR_SUB_SYSTEM_HOST || mxman->last_syserr.subcode == 0xF0)) {
					/* Host induced so promote */
					new_level = MX_SYSERR_LEVEL_8;
				}
				entry = i;
				break;
			}

			/* If code is in list, don't promote. Note that subsequent loop
			 * detection checks may promote later, though.
			 */
			if (mxfwconfig_syserr_no_promote[i] == mxman->last_syserr.subcode) {
				entry = i;
				break;
			}
		}

		SCSC_TAG_INFO(MXMAN, "entry %d = 0x%x: syserr in %d, subcode 0x%0x: L%d -> L%d\n",
			      entry,
			      (entry != -1) ? mxfwconfig_syserr_no_promote[entry] : 0,
			      mxman->last_syserr.subsys,
			      mxman->last_syserr.subcode,
			      mxman->last_syserr.level,
			      new_level);

		mxman->last_syserr.level = new_level;
	}

	/* last_syserr_level7_recovery_time is always zero-ed before we restart the chip */
	if (mxman->last_syserr_level7_recovery_time) {
		/* Have we had a too recent system error level 7 reset
		 * Chance of false positive here is low enough to be acceptable
		 */
		if ((syserr_level7_min_interval) && (time_in_range(now, mxman->last_syserr_level7_recovery_time,
				mxman->last_syserr_level7_recovery_time + msecs_to_jiffies(syserr_level7_min_interval)))) {

			SCSC_TAG_INFO(MXMAN, "Level 7 failure raised to level 8 (less than %dms after last)\n",
				syserr_level7_min_interval);
			mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
		} else if (syserr_level7_monitor_period) {
			/* Have we had too many system error level 7 resets in one period? */
			/* This will be the case if all our stored history was in this period */
			bool out_of_danger_period_found = false;

			for (i = 0; (i < SYSERR_LEVEL7_HISTORY_SIZE) && (!out_of_danger_period_found); i++)
				out_of_danger_period_found = ((!syserr_level7_history[i]) ||
						      (!time_in_range(now, syserr_level7_history[i],
							syserr_level7_history[i] + msecs_to_jiffies(syserr_level7_monitor_period))));

			if (!out_of_danger_period_found) {
				SCSC_TAG_INFO(MXMAN, "Level 7 failure raised to level 8 (too many within %dms)\n",
					syserr_level7_monitor_period);
				mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
			}
		}
	} else
		/* First syserr level 7 reset since chip was (re)started - zap history */
		for (i = 0; i < SYSERR_LEVEL7_HISTORY_SIZE; i++)
			syserr_level7_history[i] = 0;

	if ((mxman->last_syserr.level != MX_SYSERR_LEVEL_8) && (trigger_moredump_level > MX_SYSERR_LEVEL_7)) {
		/* Allow services to raise to level 8 */
		mxman->last_syserr.level = srvman_notify_services(scsc_mx_get_srvman(mxman->mx), &mxman->last_syserr);
	}

	if (mxman->last_syserr.level != MX_SYSERR_LEVEL_8) {
		/* Log this in our history */
		syserr_level7_history[syserr_level7_history_index++ % SYSERR_LEVEL7_HISTORY_SIZE] = now;
		mxman->last_syserr_level7_recovery_time = now;
	}
}

#define MAX_UHELP_TMO_MS	20000
/*
 * workqueue thread
 */
static void mxman_failure_work(struct work_struct *work)
{
	struct mxman  *mxman = container_of(work, struct mxman, failure_work);
	struct srvman *srvman;
	struct scsc_mx *mx = mxman->mx;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	int used = 0, r = 0;

#ifdef CONFIG_ANDROID
	wake_lock(&mxman->failure_recovery_wake_lock);
#endif
	/* Take mutex shared with syserr recovery */
	mutex_lock(&mxman->mxman_recovery_mutex);

	/* Check panic code for error promotion early on.
	 * Attempt to parse the panic record, to get the panic ID. This will
	 * only succeed for FW induced panics. Later we'll try again and dump.
	 */
	process_panic_record(mxman, false); /* check but don't dump */
	mxman_check_promote_syserr(mxman);

	SCSC_TAG_INFO(MXMAN, "This syserr level %d. Triggering moredump at level %d\n",
		mxman->last_syserr.level, trigger_moredump_level);

	if (mxman->last_syserr.level >= trigger_moredump_level) {
		slsi_kic_system_event(slsi_kic_system_event_category_error,
			      slsi_kic_system_events_subsystem_crashed, GFP_KERNEL);

		/* Mark as level 8 as services neeed to know this has happened */
		if (mxman->last_syserr.level < MX_SYSERR_LEVEL_8) {
			mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
			SCSC_TAG_INFO(MXMAN, "Syserr level raised to 8\n");
		}
	}

	blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_FAILURE, NULL);

	SCSC_TAG_INFO(MXMAN, "Complete mm_msg_start_ind_completion\n");
	complete(&mxman->mm_msg_start_ind_completion);
	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);

	if (mxman->mxman_state != MXMAN_STATE_STARTED && mxman->mxman_state != MXMAN_STATE_STARTING) {
		SCSC_TAG_WARNING(MXMAN, "Not in started state: mxman->mxman_state=%d\n", mxman->mxman_state);
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	/**
	 * Set error on mxlog and unregister mxlog msg-handlers.
	 * mxlog ISR and kthread will ignore further messages
	 * but mxlog_thread is NOT stopped here.
	 */
	mxlog_transport_set_error(scsc_mx_get_mxlog_transport(mx));
	mxlog_release(scsc_mx_get_mxlog(mx));
	/* unregister channel handler */
	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
						  NULL, NULL);
	mxmgmt_transport_set_error(scsc_mx_get_mxmgmt_transport(mx));
	srvman_set_error_complete(srvman, NOT_ALLOWED_START_STOP);
	fw_crc_wq_stop(mxman);

	mxman->mxman_state = mxman->mxman_next_state;

	/* Mark any single service recovery as no longer in progress */
	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;

	if (mxman->mxman_state != MXMAN_STATE_FAILED
	    && mxman->mxman_state != MXMAN_STATE_FROZEN) {
		WARN_ON(mxman->mxman_state != MXMAN_STATE_FAILED
			&& mxman->mxman_state != MXMAN_STATE_FROZEN);
		SCSC_TAG_ERR(MXMAN, "Bad state=%d\n", mxman->mxman_state);
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}
	/* Signal panic to r4 and m4 processors */
	SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_R4\n");
	mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_R4, SCSC_MIF_ABS_TARGET_R4); /* SCSC_MIFINTR_TARGET_R4 */
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_M4\n");
	mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_M4, SCSC_MIF_ABS_TARGET_M4); /* SCSC_MIFINTR_TARGET_M4 */
	SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_M4_1\n");
	mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_M4_1, SCSC_MIF_ABS_TARGET_M4_1); /* SCSC_MIFINTR_TARGET_M4 */
#else
	SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_M4\n");
	mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_M4, SCSC_MIF_ABS_TARGET_M4); /* SCSC_MIFINTR_TARGET_M4 */
#endif
	srvman_freeze_services(srvman, &mxman->last_syserr);
	if (mxman->mxman_state == MXMAN_STATE_FAILED) {
		mxman->last_panic_time = local_clock();

		/* Process and dump panic record, which should be valid now even for host induced panic */
		process_panic_record(mxman, true);

		SCSC_TAG_INFO(MXMAN, "Trying to schedule coredump\n");
		SCSC_TAG_INFO(MXMAN, "scsc_release %d.%d.%d.%d.%d\n",
			SCSC_RELEASE_PRODUCT,
			SCSC_RELEASE_ITERATION,
			SCSC_RELEASE_CANDIDATE,
			SCSC_RELEASE_POINT,
			SCSC_RELEASE_CUSTOMER);
		SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
#ifdef CONFIG_SCSC_WLBTD
		scsc_wlbtd_get_and_print_build_type();
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && defined(GO_S2D_ID)
		/* Scandump if requested on this panic. Must be tried after process_panic_record() */
		if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP) {
			if (scandump_trigger_fw_panic == mxman->scsc_panic_code) {
				SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
						 scandump_trigger_fw_panic);
				dbg_snapshot_do_dpm_policy(GO_S2D_ID);
			}
		}
#endif

		if (mxman->last_syserr.level != MX_SYSERR_LEVEL_8) {
			/* schedule system error and wait for it to finish */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
			scsc_log_collector_schedule_collection(SCSC_LOG_SYS_ERR, mxman->scsc_panic_code);
#endif
		} else {
			/* Reset level 7 loop protection */
			mxman->last_syserr_level7_recovery_time = 0;

			if (disable_auto_coredump) {
				SCSC_TAG_INFO(MXMAN, "Driver automatic coredump disabled, not launching coredump helper\n");
			} else {
#ifndef CONFIG_SCSC_WLBTD
				/* schedule coredump and wait for it to finish
				 *
				 * Releasing mxman_mutex here gives way to any
				 * eventually running resume process while waiting for
				 * the usermode helper subsystem to be resurrected,
				 * since this last will be re-enabled right at the end
				 * of the resume process itself.
				 */
				mutex_unlock(&mxman->mxman_mutex);
				SCSC_TAG_INFO(MXMAN,
					      "waiting up to %dms for usermode_helper subsystem.\n",
					      MAX_UHELP_TMO_MS);
				/* Waits for the usermode_helper subsytem to be re-enabled. */
				if (usermodehelper_read_lock_wait(msecs_to_jiffies(MAX_UHELP_TMO_MS))) {
					/**
					 * Release immediately the rwsem on usermode_helper
					 * enabled since we anyway already hold a wakelock here
					 */
					usermodehelper_read_unlock();
					/**
					 * We claim back the mxman_mutex immediately to avoid anyone
					 * shutting down the chip while we are dumping the coredump.
					 */
					mutex_lock(&mxman->mxman_mutex);
					SCSC_TAG_INFO(MXMAN, "Invoking coredump helper\n");
					slsi_kic_system_event(slsi_kic_system_event_category_recovery,
						slsi_kic_system_events_coredump_in_progress,
						GFP_KERNEL);

					r = coredump_helper();
#else
					/* we can safely call call_wlbtd as we are
					 * in workqueue context
					 */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
					/* Collect mxlogger logs */
					scsc_log_collector_schedule_collection(SCSC_LOG_FW_PANIC, mxman->scsc_panic_code);
#else
					r = call_wlbtd(SCSC_SCRIPT_MOREDUMP);
#endif
#endif
					if (r >= 0) {
						slsi_kic_system_event(slsi_kic_system_event_category_recovery,
							slsi_kic_system_events_coredump_done, GFP_KERNEL);
					}

					used = snprintf(panic_record_dump,
							PANIC_RECORD_DUMP_BUFFER_SZ,
							"RF HW Ver: 0x%X\n", mxman->rf_hw_ver);
					used += snprintf(panic_record_dump + used,
							 PANIC_RECORD_DUMP_BUFFER_SZ - used,
							 "SCSC Panic Code:: 0x%X\n", mxman->scsc_panic_code);
					used += snprintf(panic_record_dump + used,
							 PANIC_RECORD_DUMP_BUFFER_SZ - used,
							 "SCSC Last Panic Time:: %lld\n", mxman->last_panic_time);
					panic_record_dump_buffer("r4", mxman->last_panic_rec_r,
								 mxman->last_panic_rec_sz,
								 panic_record_dump + used,
								 PANIC_RECORD_DUMP_BUFFER_SZ - used);

					/* Print the host code/reason again so it's near the FW panic
					 * record in the kernel log
					 */
					print_panic_code(mxman->scsc_panic_code);
					SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");

					blocking_notifier_call_chain(&firmware_chain,
								     SCSC_FW_EVENT_MOREDUMP_COMPLETE,
								     &panic_record_dump);
#ifndef CONFIG_SCSC_WLBTD
				} else {
					SCSC_TAG_INFO(MXMAN,
						      "timed out waiting for usermode_helper. Skipping coredump.\n");
					mutex_lock(&mxman->mxman_mutex);
				}
#endif
			}
		}

		if (is_bug_on_enabled(mx)) {
			SCSC_TAG_ERR(MX_FILE, "Deliberately panic the kernel due to WLBT firmware failure!\n");
			SCSC_TAG_ERR(MX_FILE, "calling BUG_ON(1)\n");
			BUG_ON(1);
		}
		/* Clean up the MIF following error handling */
		if (mif->mif_cleanup && mxman_recovery_disabled())
			mif->mif_cleanup(mif);
	}

	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n",
		mxman_recovery_disabled() ? "off" : "on");

	if (!mxman_recovery_disabled())
		srvman_set_error(srvman, NOT_ALLOWED_START);
	mutex_unlock(&mxman->mxman_mutex);
	if (!mxman_recovery_disabled()) {
		SCSC_TAG_INFO(MXMAN, "Calling srvman_unfreeze_services\n");
		srvman_unfreeze_services(srvman, &mxman->last_syserr);
		if (scsc_mx_module_reset() < 0)
			SCSC_TAG_INFO(MXMAN, "failed to call scsc_mx_module_reset\n");
		srvman_set_error(srvman, ALLOWED_START_STOP);
		atomic_inc(&mxman->recovery_count);
	}

	/**
	 * If recovery is disabled and an scsc_mx_service_open has been hold up,
	 * release it, rather than wait for the recovery_completion to timeout.
	 */
	if (mxman_recovery_disabled())
		complete(&mxman->recovery_completion);

	/* Safe to allow syserr recovery thread to run */
	mutex_unlock(&mxman->mxman_recovery_mutex);

#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
}

static void failure_wq_init(struct mxman *mxman)
{
	mxman->failure_wq = create_singlethread_workqueue("failure_wq");
	INIT_WORK(&mxman->failure_work, mxman_failure_work);
}

static void failure_wq_stop(struct mxman *mxman)
{
	cancel_work_sync(&mxman->failure_work);
	flush_workqueue(mxman->failure_wq);
}

static void failure_wq_deinit(struct mxman *mxman)
{
	failure_wq_stop(mxman);
	destroy_workqueue(mxman->failure_wq);
}

static void failure_wq_start(struct mxman *mxman)
{
	if (disable_error_handling)
		SCSC_TAG_INFO(MXMAN, "error handling disabled\n");
	else
		queue_work(mxman->failure_wq, &mxman->failure_work);
}

/*
 * workqueue thread
 */
static void mxman_syserr_recovery_work(struct work_struct *work)
{
	struct mxman  *mxman = container_of(work, struct mxman, syserr_recovery_work);
	struct srvman *srvman;

#ifdef CONFIG_ANDROID
	wake_lock(&mxman->syserr_recovery_wake_lock);
#endif
	if (!mutex_trylock(&mxman->mxman_recovery_mutex)) {
		SCSC_TAG_WARNING(MXMAN, "Syserr during full reset - ignored\n");
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
		return;
	}

	mutex_lock(&mxman->mxman_mutex);

	if (mxman->mxman_state != MXMAN_STATE_STARTED && mxman->mxman_state != MXMAN_STATE_STARTING) {
		SCSC_TAG_WARNING(MXMAN, "Syserr reset ignored: mxman->mxman_state=%d\n", mxman->mxman_state);
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	srvman = scsc_mx_get_srvman(mxman->mx);

	srvman_freeze_sub_system(srvman, &mxman->last_syserr);

#ifdef CONFIG_SCSC_WLBTD
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Wait for log generation if not finished */
	SCSC_TAG_INFO(MXMAN, "Wait for syserr sable logging\n");
	scsc_wlbtd_wait_for_sable_logging();
	SCSC_TAG_INFO(MXMAN, "Syserr sable logging complete\n");
#endif
#endif

	srvman_unfreeze_sub_system(srvman, &mxman->last_syserr);

#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
	mutex_unlock(&mxman->mxman_recovery_mutex);
	mutex_unlock(&mxman->mxman_mutex);
}

static void syserr_recovery_wq_init(struct mxman *mxman)
{
	mxman->syserr_recovery_wq = create_singlethread_workqueue("syserr_recovery_wq");
	INIT_WORK(&mxman->syserr_recovery_work, mxman_syserr_recovery_work);
}

static void syserr_recovery_wq_stop(struct mxman *mxman)
{
	cancel_work_sync(&mxman->syserr_recovery_work);
	flush_workqueue(mxman->syserr_recovery_wq);
}

static void syserr_recovery_wq_deinit(struct mxman *mxman)
{
	syserr_recovery_wq_stop(mxman);
	destroy_workqueue(mxman->syserr_recovery_wq);
}

static void syserr_recovery_wq_start(struct mxman *mxman)
{
	queue_work(mxman->syserr_recovery_wq, &mxman->syserr_recovery_work);
}

static void print_mailboxes(struct mxman *mxman)
{
	struct scsc_mif_abs *mif;
	struct mifmboxman   *mboxman;
	int                 i;

	mif = scsc_mx_get_mif_abs(mxman->mx);
	mboxman = scsc_mx_get_mboxman(mxman->mx);

	SCSC_TAG_INFO(MXMAN, "Printing mailbox values:\n");
	for (i = 0; i < MIFMBOX_NUM; i++)
		SCSC_TAG_INFO(MXMAN, "MBOX_%d: 0x%x\n", i, *mifmboxman_get_mbox_ptr(mboxman, mif, i));
}
#ifdef CONFIG_SCSC_WLBTD
static void wlbtd_work_func(struct work_struct *work)
{
	/* require sleep-able workqueue to run successfully */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Collect mxlogger logs */
	/* Extend to scsc_log_collector_collect() if required */
#else
	call_wlbtd(SCSC_SCRIPT_LOGGER_DUMP);
#endif
}

static void wlbtd_wq_init(struct mxman *mx)
{
	INIT_WORK(&wlbtd_work, wlbtd_work_func);
}

static void wlbtd_wq_deinit(struct mxman *mx)
{
	/* flush and block until work is complete */
	flush_work(&wlbtd_work);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
int mxman_sysevent_desc_init(struct mxman *mxman)
{
	int ret = 0;
	struct device *dev;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);
	dev = mif->get_mif_device(mif);

	mxman->sysevent_dev = NULL;
	mxman->sysevent_desc.name = "wlbt";
	mxman->sysevent_desc.owner = THIS_MODULE;
	mxman->sysevent_desc.powerup = wlbt_sysevent_powerup;
	mxman->sysevent_desc.shutdown = wlbt_sysevent_shutdown;
	mxman->sysevent_desc.ramdump = wlbt_sysevent_ramdump;
	mxman->sysevent_desc.crash_shutdown = wlbt_sysevent_crash_shutdown;
	mxman->sysevent_desc.dev = dev;
	mxman->sysevent_dev = sysevent_register(&mxman->sysevent_desc);
	if (IS_ERR(mxman->sysevent_dev)) {
		ret = PTR_ERR(mxman->sysevent_dev);
		SCSC_TAG_WARNING(MXMAN,	"sysevent_register failed :%d\n", ret);
	} else
		SCSC_TAG_INFO(MXMAN, "sysevent_register success\n");

	return ret;
}
#endif

/*
 * Check for matching f/w and h/w
 *
 * Returns	0:  f/w and h/w match
 *		1:  f/w and h/w mismatch, try the next config
 *		-ve fatal error
 */
static int mxman_hw_ver_check(struct mxman *mxman)
{
	if (mx140_file_supported_hw(mxman->mx, mxman->rf_hw_ver))
		return 0;
	else
		return 1;
}

/*
 * Select the f/w version to load next
 */
static int mxman_select_next_fw(struct mxman *mxman)
{
	return mx140_file_select_fw(mxman->mx, mxman->rf_hw_ver);
}

/* Boot MX140 with given f/w */
static int __mxman_open(struct mxman *mxman)
{
	int r;
	struct srvman *srvman;

	mx140_basedir_file(mxman->mx);

	mutex_lock(&mxman->mxman_mutex);
	if (mxman->scsc_panic_code) {
		SCSC_TAG_INFO(MXMAN, "Previously recorded crash panic code: scsc_panic_code=0x%x\n", mxman->scsc_panic_code);
		SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
		print_panic_code(mxman->scsc_panic_code);
	}
	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && srvman_in_error(srvman)) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return -EINVAL;
	}

	/* Reset the state after a previous crash during f/w boot */
	if (mxman->mxman_state == MXMAN_STATE_STARTING)
		mxman->mxman_state = MXMAN_STATE_STOPPED;

	if (mxman->mxman_state == MXMAN_STATE_STARTED) {
		/* if in the STARTED state there MUST already be some users */
		if (WARN_ON(!mxman->users)) {
			SCSC_TAG_ERR(MXMAN, "ERROR mxman->mxman_state=%d users=%d\n", mxman->mxman_state, mxman->users);
			mutex_unlock(&mxman->mxman_mutex);
			return -EINVAL;
		}
		mxman->users++;
		SCSC_TAG_INFO(MXMAN, "Already opened: users=%d\n", mxman->users);
		mxman_print_versions(mxman);
		mutex_unlock(&mxman->mxman_mutex);
		return 0;
	} else if (mxman->mxman_state == MXMAN_STATE_STOPPED) {
		r = mxman_start(mxman);
		if (r) {
			SCSC_TAG_ERR(MXMAN, "maxwell_manager_start() failed r=%d users=%d\n", r, mxman->users);
			mutex_unlock(&mxman->mxman_mutex);
			return r;
		}
		mxman->users++;
		mxman->mxman_state = MXMAN_STATE_STARTED;
		mutex_unlock(&mxman->mxman_mutex);
		/* Start mxlogger */
		if (!disable_logger) {
			static char mxlbin[128];

			r = mx140_exe_path(NULL, mxlbin, sizeof(mxlbin), "mx_logger.sh");
			if (r) {
				/* Not found */
				SCSC_TAG_ERR(MXMAN, "mx_logger.sh path error\n");
			}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
			else {
				/* Launch it */
				_mx_exec(mxlbin, UMH_WAIT_EXEC);
			}
#endif
		}
		return 0;
	}
	WARN_ON(mxman->mxman_state != MXMAN_STATE_STARTED && mxman->mxman_state != MXMAN_STATE_STOPPED);
	SCSC_TAG_ERR(MXMAN, "Bad state: mxman->mxman_state=%d\n", mxman->mxman_state);
	mutex_unlock(&mxman->mxman_mutex);
	return -EIO;
}

int mxman_open(struct mxman *mxman)
{
	int r;
	int try = 0;

	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);

	for (try = 0; try < 2; try++) {
		/* Boot WLBT. This will determine the h/w version */
		r = __mxman_open(mxman);
		if (r)
			return r;

		/* On retries, restore USBPLL owner as WLBT */
		if (try > 0 && mif->mif_restart)
			mif->mif_restart(mif);

		/* Check the h/w and f/w versions are compatible */
		r = mxman_hw_ver_check(mxman);
		if (r > 0) {
			/* Not compatible, so try next f/w */
			SCSC_TAG_INFO(MXMAN, "Incompatible h/w 0x%04x vs f/w, close and try next\n", mxman->rf_hw_ver);

			/* Temporarily return USBPLL owner to AP to keep USB alive */
			if (mif->mif_cleanup)
				mif->mif_cleanup(mif);

			/* Stop WLBT */
			mxman_close(mxman);

			/* Select the new f/w for this hw ver */
			mxman_select_next_fw(mxman);
		} else
			break; /* Running or given up */
	}

#ifdef CONFIG_SCSC_FM
	/* If we have stored FM radio parameters, deliver them to FW now */
	if (r == 0 && mxman->fm_params_pending) {
		SCSC_TAG_INFO(MXMAN, "Send pending FM params\n");
		mxman_fm_set_params(&mxman->fm_params);
	}
#endif

	return r;
}

static void mxman_stop(struct mxman *mxman)
{
	int r;
	struct scsc_mif_abs *mif;

	SCSC_TAG_INFO(MXMAN, "\n");

	mif = scsc_mx_get_mif_abs(mxman->mx);
	/* If reset is failed, prevent new resets */
	if (mxman_check_reset_failed(mif)) {
		struct timeval tval = ns_to_timeval(reset_failed_time);

		SCSC_TAG_ERR(MXMAN, "previous reset failed at [%6lu.%06ld], ignoring\n", tval.tv_sec, tval.tv_usec);
		return;
	}

	(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), "unknown");

	mxproc_remove_ctrl_proc_dir(&mxman->mxproc);

	/* Shutdown the hardware */
	mif = scsc_mx_get_mif_abs(mxman->mx);
	r = mif->reset(mif, 1);
	if (r) {
		reset_failed_time = local_clock();
		SCSC_TAG_INFO(MXMAN, "HW reset failed\n");
		mxman_set_reset_failed();

		/* Save log at point of failure */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_schedule_collection(SCSC_LOG_HOST_COMMON, SCSC_LOG_HOST_COMMON_REASON_STOP);
#else
		mx140_log_dump();
#endif
	}

	panicmon_deinit(scsc_mx_get_panicmon(mxman->mx));
	transports_release(mxman);
	mxfwconfig_unload(mxman->mx);

	mxlog_release(scsc_mx_get_mxlog(mxman->mx));
	/* unregister channel handler */
	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
						  NULL, NULL);
	fw_crc_wq_stop(mxman);

	/* Unitialise components (they may perform some checks - e.g. all memory freed) */
	mxfwconfig_deinit(mxman->mx);
	mifintrbit_deinit(scsc_mx_get_intrbit(mxman->mx));
	miframman_deinit(scsc_mx_get_ramman(mxman->mx));
	miframman_deinit(scsc_mx_get_ramman2(mxman->mx));
	miframabox_deinit(scsc_mx_get_aboxram(mxman->mx));
	mifmboxman_deinit(scsc_mx_get_mboxman(mxman->mx));
#ifdef CONFIG_SCSC_SMAPPER
	mifsmapper_deinit(scsc_mx_get_smapper(mxman->mx));
#endif
#ifdef CONFIG_SCSC_QOS
	mifqos_deinit(scsc_mx_get_qos(mxman->mx));
#endif
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
	scsc_log_in_dram_mmap_destroy();
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	mif->recovery_disabled_unreg(mif);
#endif
	/* Release the MIF memory resources */
	mif->unmap(mif, mxman->start_dram);
}

void mxman_close(struct mxman *mxman)
{
	int r;
	struct srvman *srvman;

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && !srvman_allow_close(srvman)) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error(%d) - ignore\n", srvman->error);
		return;
	}

	SCSC_TAG_INFO(MXMAN, "\n");

	if (mxman->mxman_state == MXMAN_STATE_STARTED) {
		if (WARN_ON(!mxman->users)) {
			SCSC_TAG_ERR(MXMAN, "ERROR users=%d\n", mxman->users);
			mutex_unlock(&mxman->mxman_mutex);
			return;
		}
		mxman->users--;
		if (mxman->users) {
			SCSC_TAG_INFO(MXMAN, "Current number of users=%d\n", mxman->users);
			mutex_unlock(&mxman->mxman_mutex);
			return;
		}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		/* Unregister minimoredump client */
		scsc_log_collector_unregister_client(&mini_moredump_client);
#endif
		/**
		 * Deinit mxlogger on last service stop...BUT before asking for HALT
		 */
		mxlogger_deinit(mxman->mx, scsc_mx_get_mxlogger(mxman->mx));
#endif
		/*
		 * Ask the subsystem to stop (MM_STOP_REQ), and wait
		 * for response (MM_STOP_RSP).
		 */
		r = send_mm_msg_stop_blocking(mxman);
		if (r)
			SCSC_TAG_ERR(MXMAN, "send_mm_msg_stop_blocking failed: r=%d\n", r);

		mxman_stop(mxman);
		mxman->mxman_state = MXMAN_STATE_STOPPED;
		mutex_unlock(&mxman->mxman_mutex);
	} else if (mxman->mxman_state == MXMAN_STATE_FAILED) {
		if (WARN_ON(!mxman->users))
			SCSC_TAG_ERR(MXMAN, "ERROR users=%d\n", mxman->users);

		mxman->users--;
		if (mxman->users) {
			SCSC_TAG_INFO(MXMAN, "Current number of users=%d\n", mxman->users);
			mutex_unlock(&mxman->mxman_mutex);
			return;
		}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		/* Unregister minimoredump client */
		scsc_log_collector_unregister_client(&mini_moredump_client);
#endif
		/**
		 * Deinit mxlogger on last service stop...BUT before asking for HALT
		 */
		mxlogger_deinit(mxman->mx, scsc_mx_get_mxlogger(mxman->mx));
#endif

		mxman_stop(mxman);
		mxman->mxman_state = MXMAN_STATE_STOPPED;
		mutex_unlock(&mxman->mxman_mutex);
		complete(&mxman->recovery_completion);
	} else {
		WARN_ON(mxman->mxman_state != MXMAN_STATE_STARTED);
		SCSC_TAG_ERR(MXMAN, "Bad state: mxman->mxman_state=%d\n", mxman->mxman_state);
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}
}

void mxman_syserr(struct mxman *mxman, struct mx_syserr_decode *syserr)
{
	mxman->syserr_recovery_in_progress = true;

	mxman->last_syserr.subsys = syserr->subsys;
	mxman->last_syserr.level = syserr->level;
	mxman->last_syserr.type = syserr->type;
	mxman->last_syserr.subcode = syserr->subcode;

	syserr_recovery_wq_start(mxman);
}

void mxman_fail(struct mxman *mxman, u16 failure_source, const char *reason)
{
	SCSC_TAG_WARNING(MXMAN, "WLBT FW failure 0x%x\n", failure_source);

	/* For FW failure, scsc_panic_code is not set up fully until process_panic_record() checks it */
	if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && defined(GO_S2D_ID)
		if (scandump_trigger_fw_panic == 0) {
			SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n", scandump_trigger_fw_panic);
			dbg_snapshot_do_dpm_policy(GO_S2D_ID);
		}
#else
		/* Support not present, fallback to vanilla moredump and stop WLBT */
		disable_recovery_handling = 1;
		SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - scandump requested but not supported in kernel\n");
#endif
	}

	/* The STARTING state allows a crash during firmware boot to be handled */
	if (mxman->mxman_state == MXMAN_STATE_STARTED || mxman->mxman_state == MXMAN_STATE_STARTING) {
		mxman->mxman_next_state = MXMAN_STATE_FAILED;
		mxman->scsc_panic_code = failure_source;
		strlcpy(mxman->failure_reason, reason, sizeof(mxman->failure_reason));
		/* If recovery is disabled, don't let it be
		 * re-enabled from now on. Device must reboot
		 */
		if (mxman_recovery_disabled())
			disable_recovery_until_reboot  = true;

		/* Populate syserr info with panic equivalent or best we can */
		mxman->last_syserr.subsys = failure_source >> SYSERR_SUB_SYSTEM_POSN;
		mxman->last_syserr.level = MX_SYSERR_LEVEL_7;
		mxman->last_syserr.type = failure_source;
		mxman->last_syserr.subcode = failure_source;
		atomic_inc(&mxman->cancel_resume);
		failure_wq_start(mxman);
	} else {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
	}
}

void mxman_fail_level8(struct mxman *mxman, u16 failure_source, const char *reason)
{
	SCSC_TAG_WARNING(MXMAN, "WLBT FW level 8 failure 0x%0x\n", failure_source);

	/* The STARTING state allows a crash during firmware boot to be handled */
	if (mxman->mxman_state == MXMAN_STATE_STARTED || mxman->mxman_state == MXMAN_STATE_STARTING) {
		mxman->mxman_next_state = MXMAN_STATE_FAILED;
		mxman->scsc_panic_code = failure_source;
		strlcpy(mxman->failure_reason, reason, sizeof(mxman->failure_reason));
		/* If recovery is disabled, don't let it be
		 * re-enabled from now on. Device must reboot
		 */
		if (mxman_recovery_disabled())
			disable_recovery_until_reboot  = true;

		/* Populate syserr info with panic equivalent or best we can */
		mxman->last_syserr.subsys = failure_source >> SYSERR_SUB_SYSTEM_POSN;
		mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
		mxman->last_syserr.type = failure_source;
		mxman->last_syserr.subcode = failure_source;

		failure_wq_start(mxman);
	} else {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
	}
}

void mxman_freeze(struct mxman *mxman)
{
	SCSC_TAG_WARNING(MXMAN, "WLBT FW frozen\n");

	if (mxman->mxman_state == MXMAN_STATE_STARTED) {
		mxman->mxman_next_state = MXMAN_STATE_FROZEN;
		failure_wq_start(mxman);
	} else {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
	}
}

void mxman_init(struct mxman *mxman, struct scsc_mx *mx)
{
	mxman->mx = mx;
	mxman->suspended = 0;
#ifdef CONFIG_SCSC_FM
	mxman->on_halt_ldos_on = 0;
	mxman->fm_params_pending = 0;
#endif
	fw_crc_wq_init(mxman);
	failure_wq_init(mxman);
	syserr_recovery_wq_init(mxman);
#ifdef CONFIG_SCSC_WLBTD
	wlbtd_wq_init(mxman);
#endif
	mutex_init(&mxman->mxman_mutex);
	mutex_init(&mxman->mxman_recovery_mutex);
	init_completion(&mxman->recovery_completion);
#ifdef CONFIG_ANDROID
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	wake_lock_init(&mxman->failure_recovery_wake_lock, WAKE_LOCK_SUSPEND, "mxman_recovery");
	wake_lock_init(&mxman->syserr_recovery_wake_lock, WAKE_LOCK_SUSPEND, "mxman_syserr_recovery");
#else
	wake_lock_init(NULL, &mxman->failure_recovery_wake_lock.ws, "mxman_recovery");
        wake_lock_init(NULL, &mxman->syserr_recovery_wake_lock.ws, "mxman_syserr_recovery");
#endif
#endif
	mxman->last_syserr_level7_recovery_time = 0;

	atomic_set(&mxman->cancel_resume, 0);

	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;

	/* set the initial state */
	mxman->mxman_state = MXMAN_STATE_STOPPED;
	(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), "unknown");
	memcpy(saved_fw_build_id, mxman->fw_build_id,
	       sizeof(saved_fw_build_id));
	(void)snprintf(mxman->fw_ttid, sizeof(mxman->fw_ttid), "unknown");
	mxproc_create_info_proc_dir(&mxman->mxproc, mxman);
	active_mxman = mxman;

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (!mxman_sysevent_desc_init(mxman)) {
		mxman->sysevent_nb.notifier_call = wlbt_sysevent_notifier_cb;
		sysevent_notif_register_notifier(mxman->sysevent_desc.name,
							&mxman->sysevent_nb);
	}
#endif

#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 9
	mxman_create_sysfs_memdump();
#endif
	scsc_lerna_init();

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	scsc_logring_register_mx_cb(&mx_logring);
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_register_mx_cb(&mx_cb);
#endif
}

void mxman_deinit(struct mxman *mxman)
{
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	scsc_logring_unregister_mx_cb(&mx_logring);
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_mx_cb(&mx_cb);
#endif
	scsc_lerna_deinit();
#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 9
	mxman_destroy_sysfs_memdump();
#endif
	active_mxman = NULL;
	mxproc_remove_info_proc_dir(&mxman->mxproc);
	fw_crc_wq_deinit(mxman);
	failure_wq_deinit(mxman);
	syserr_recovery_wq_deinit(mxman);
#ifdef CONFIG_SCSC_WLBTD
	wlbtd_wq_deinit(mxman);
#endif
#ifdef CONFIG_ANDROID
	wake_lock_destroy(&mxman->failure_recovery_wake_lock);
	wake_lock_destroy(&mxman->syserr_recovery_wake_lock);
#endif
	mutex_destroy(&mxman->mxman_recovery_mutex);
	mutex_destroy(&mxman->mxman_mutex);
}

int mxman_force_panic(struct mxman *mxman)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_FORCE_PANIC };

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && srvman_in_error(srvman)) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return -EINVAL;
	}

	if (mxman->mxman_state == MXMAN_STATE_STARTED) {
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		mutex_unlock(&mxman->mxman_mutex);
		return 0;
	}
	mutex_unlock(&mxman->mxman_mutex);
	return -EINVAL;
}

int mxman_suspend(struct mxman *mxman)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_HOST_SUSPEND };
	int ret;

	SCSC_TAG_INFO(MXMAN, "\n");

	atomic_set(&mxman->cancel_resume, 0);
	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);

	if (srvman && srvman_in_error(srvman)) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return 0;
	}

	/* Call Service suspend callbacks */
	ret = srvman_suspend_services(srvman);
	if (ret) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Service Suspend canceled - ignore %d\n", ret);
		return ret;
	}

	if (mxman->mxman_state == MXMAN_STATE_STARTED) {
		SCSC_TAG_INFO(MXMAN, "MM_HOST_SUSPEND\n");
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_generate_sync_record(scsc_mx_get_mxlogger(mxman->mx), MXLOGGER_SYN_SUSPEND);
#endif
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		mxman->suspended = 1;
		atomic_inc(&mxman->suspend_count);
	}
	mutex_unlock(&mxman->mxman_mutex);
	return 0;
}

#ifdef CONFIG_SCSC_FM
void mxman_fm_on_halt_ldos_on(void)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return;
	}

	active_mxman->on_halt_ldos_on = 1;

	/* FM status to pass into FW at next FW init,
	 * by which time driver context is lost.
	 * This is required, because now WLBT gates
	 * LDOs with TCXO instead of leaving them
	 * always on, to save power in deep sleep.
	 * FM, however, needs them always on. So
	 * we need to know when to leave the LDOs
	 * alone at WLBT boot.
	 */
	is_fm_on = 1;
}

void mxman_fm_on_halt_ldos_off(void)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return;
	}

	/* Newer FW no longer need set shared LDOs
	 * always-off at WLBT halt, as TCXO gating
	 * has the same effect. But pass the "off"
	 * request for backwards compatibility
	 * with old FW.
	 */
	active_mxman->on_halt_ldos_on = 0;
	is_fm_on = 0;
}

/* Update parameters passed to WLBT FM */
int mxman_fm_set_params(struct wlbt_fm_params *params)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return -EINVAL;
	}

	/* Params are no longer valid (FM stopped) */
	if (!params) {
		active_mxman->fm_params_pending = 0;
		SCSC_TAG_INFO(MXMAN, "FM params cleared\n");
		return 0;
	}

	/* Once set the value needs to be remembered for each time WLBT starts */
	active_mxman->fm_params = *params;
	active_mxman->fm_params_pending = 1;

	if (send_fm_params_to_active_mxman(params)) {
		SCSC_TAG_INFO(MXMAN, "FM params sent to FW\n");
		return 0;
	}

	/* Stored for next time FW is up */
	SCSC_TAG_INFO(MXMAN, "FM params stored\n");

	return -EAGAIN;
}
#endif

void mxman_resume(struct mxman *mxman)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_HOST_RESUME };
	int ret;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (atomic_read(&mxman->cancel_resume)) {
		SCSC_TAG_INFO(MXMAN, "Recovery in progress ... ignoring");
		return;
	}

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && srvman_in_error(srvman)) {
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	if (mxman->mxman_state == MXMAN_STATE_STARTED) {
		SCSC_TAG_INFO(MXMAN, "MM_HOST_RESUME\n");
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_generate_sync_record(scsc_mx_get_mxlogger(mxman->mx), MXLOGGER_SYN_RESUME);
#endif
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		mxman->suspended = 0;
	}

	/* Call Service Resume callbacks */
	ret = srvman_resume_services(srvman);
	if (ret)
		SCSC_TAG_INFO(MXMAN, "Service Resume error %d\n", ret);

	mutex_unlock(&mxman->mxman_mutex);
}

static void _mx_exec_cleanup(struct subprocess_info *sp_info)
{
	if (!sp_info) {
		SCSC_TAG_ERR(MXMAN, "sp_info is null\n");
		return;
	}
	if (!sp_info->argv) {
		SCSC_TAG_ERR(MXMAN, "argv is null\n");
		return;
	}

	SCSC_TAG_INFO(MXMAN, "0x%p\n", sp_info->argv);
	argv_free(sp_info->argv);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
/* prog - full path to programme
 * wait_exec - one of UMH_WAIT_EXEC, UMH_WAIT_PROC, UMH_KILLABLE, UMH_NO_WAIT
 */
static int _mx_exec(char *prog, int wait_exec)
{
	/**
	 * ENV vars ANDROID_ROOT and ANDROID_DATA are needed to have
	 * the UMH spawned process working properly (as an example finding
	 * Timezones files)
	 */
	static char const      *envp[] = { "HOME=/", "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin:/vendor/xbin",
					   "ANDROID_ROOT=/system", "ANDROID_DATA=/data", NULL };
	char                   **argv;
	char                   argv_str[STRING_BUFFER_MAX_LENGTH];
	int                    argc, result, len;
	struct subprocess_info *sp_info;

	len = snprintf(argv_str, STRING_BUFFER_MAX_LENGTH, "%s", prog);
	if (len >= STRING_BUFFER_MAX_LENGTH) {
		/* snprintf() returns a value of buffer size of greater if it had to truncate the format string. */
		SCSC_TAG_ERR(MXMAN,
			     "exec string buffer insufficient (buffer size=%d, actual string=%d)\n",
			     STRING_BUFFER_MAX_LENGTH, len);
		return -E2BIG;
	}

	/* Kernel library function argv_split() will allocate memory for argv. */
	argc = 0;
	argv = argv_split(GFP_KERNEL, argv_str, &argc);
	if (!argv) {
		SCSC_TAG_ERR(MXMAN, "failed to allocate argv for userspace helper\n");
		return -ENOMEM;
	}

	/* Check the argument count just to avoid future abuse */
	if (argc > NUMBER_OF_STRING_ARGS) {
		SCSC_TAG_ERR(MXMAN,
			     "exec string has the wrong number of arguments (has %d, should be %d)\n",
			     argc, NUMBER_OF_STRING_ARGS);
		argv_free(argv);
		return -E2BIG;
	}

	/* Allocate sp_info and initialise pointers to argv and envp. */
	sp_info = call_usermodehelper_setup(argv[0], argv, (char **)envp,
						GFP_KERNEL, NULL, _mx_exec_cleanup,
						NULL);

	if (!sp_info) {
		SCSC_TAG_ERR(MXMAN, "call_usermodehelper_setup() failed\n");
		argv_free(argv);
		return -EIO;
	}

	/* Put sp_info into work queue for processing by khelper. */
	SCSC_TAG_INFO(MXMAN, "Launch %s\n", prog);

	result = call_usermodehelper_exec(sp_info, wait_exec);

	if (result != 0) {
		/*
		 * call_usermodehelper_exec() will free sp_info and call any cleanup function
		 * whether it succeeds or fails, so do not free argv.
		 */
		if (result == -ENOENT)
			SCSC_TAG_ERR(MXMAN, "call_usermodehelper() failed with %d, Executable not found %s'\n",
				     result, prog);
		else
			SCSC_TAG_ERR(MXMAN, "call_usermodehelper_exec() failed with %d\n", result);
	}
	return result;
}
#endif

#if defined(CONFIG_SCSC_PRINTK) && !defined(CONFIG_SCSC_WLBTD)
static int __stat(const char *file)
{
	struct kstat stat;
	mm_segment_t fs;
	int r;

	fs = get_fs();
	set_fs(KERNEL_DS);
	r = vfs_stat(file, &stat);
	set_fs(fs);

	return r;
}
#endif

int mx140_log_dump(void)
{
#ifdef CONFIG_SCSC_PRINTK
	int r;
# ifdef CONFIG_SCSC_WLBTD
	r = schedule_work(&wlbtd_work);
# else
	char mxlbin[128];

	r = mx140_exe_path(NULL, mxlbin, sizeof(mxlbin), "mx_logger_dump.sh");
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mx_logger_dump.sh path error\n");
	} else {
		/*
		 * Test presence of script before invoking, to suppress
		 * unnecessary error message if not installed.
		 */
		r = __stat(mxlbin);
		if (r) {
			SCSC_TAG_DEBUG(MXMAN, "%s not installed\n", mxlbin);
			return r;
		}
		SCSC_TAG_INFO(MXMAN, "Invoking mx_logger_dump.sh UHM\n");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		r = _mx_exec(mxlbin, UMH_WAIT_EXEC);
		if (r)
			SCSC_TAG_ERR(MXMAN, "mx_logger_dump.sh err:%d\n", r);
#endif
	}
# endif /* CONFIG_SCSC_WLBTD */
	return r;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mx140_log_dump);

bool mxman_recovery_disabled(void)
{
#ifdef CONFIG_SCSC_WLBT_AUTORECOVERY_PERMANENT_DISABLE
	/* Add option to kill autorecovery, ignoring module parameter
	 * to work around platform that enables it against our wishes
	 */
	SCSC_TAG_ERR(MXMAN, "CONFIG_SCSC_WLBT_AUTORECOVERY_PERMANENT_DISABLE is set\n");
	return true;
#endif
	/* If FW has panicked when recovery was disabled, don't allow it to
	 * be enabled. The horse has bolted.
	 */
	if (disable_recovery_until_reboot)
		return true;

	if (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY)
		return disable_recovery_from_memdump_file;
	else
		return disable_recovery_handling ? true : false;
}
EXPORT_SYMBOL(mxman_recovery_disabled);

/**
 * This returns the last known loaded FW build_id
 * even when the fw is NOT running at the time of the request.
 *
 * It could be used anytime by Android Enhanced Logging
 * to query for fw version.
 */
void mxman_get_fw_version(char *version, size_t ver_sz)
{
	/* unavailable only if chip not probed ! */
	snprintf(version, ver_sz, "%s", saved_fw_build_id);
}
EXPORT_SYMBOL(mxman_get_fw_version);

void mxman_get_driver_version(char *version, size_t ver_sz)
{
	/* IMPORTANT - Do not change the formatting as User space tooling is parsing the string
	* to read SAP fapi versions. */
	snprintf(version, ver_sz, "drv_ver: %u.%u.%u.%u.%u",
		 SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION, SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_get_and_print_build_type();
#endif
}
EXPORT_SYMBOL(mxman_get_driver_version);

int mxman_register_firmware_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&firmware_chain, nb);
}
EXPORT_SYMBOL(mxman_register_firmware_notifier);

int mxman_unregister_firmware_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&firmware_chain, nb);
}
EXPORT_SYMBOL(mxman_unregister_firmware_notifier);


int mxman_lerna_send(struct mxman *mxman, void *message, u32 message_size)
{
	struct srvman *srvman = NULL;

	/* May be called when WLBT is off, so find the context in this case */
	if (!mxman)
		mxman = active_mxman;

	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return -EINVAL;
	}

	if (!message || (message_size == 0)) {
		SCSC_TAG_INFO(MXMAN, "No lerna request provided.\n");
		return 0;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman_in_error(srvman)) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Lerna configuration called during error - ignore\n");
		return 0;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED) {
		SCSC_TAG_INFO(MXMAN, "MM_LERNA_CONFIG\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, message,
				message_size);
		mutex_unlock(&active_mxman->mxman_mutex);
		return 0;
	}

	SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_LERNA_CONFIG msg.\n");
	mutex_unlock(&active_mxman->mxman_mutex);
	return -EAGAIN;
}
