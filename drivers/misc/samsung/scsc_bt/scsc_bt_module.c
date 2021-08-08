/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 * BT driver entry point
 *
 ****************************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/termios.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/ctype.h>

#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#include <linux/soc/samsung/exynos-soc.h>
#endif

#include <scsc/scsc_logring.h>
#include <scsc/kic/slsi_kic_lib.h>
#include <scsc/kic/slsi_kic_bt.h>
#include <scsc/kic/slsi_kic_ant.h>

#include "scsc_bt_priv.h"
#include "../scsc/scsc_mx_impl.h"

#ifdef CONFIG_SCSC_LOG_COLLECTION
#include <scsc/scsc_log_collector.h>
#endif

#define SCSC_MODDESC "SCSC MX BT Driver"
#define SCSC_MODAUTH "Samsung Electronics Co., Ltd"
#define SCSC_MODVERSION "-devel"

#define SLSI_BT_SERVICE_CLOSE_RETRY 60
#define SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT 20000
#define SLSI_BT_SERVICE_STOP_RECOVERY_DISABLED_TIMEOUT 2000
#define SLSI_BT_SERVICE_RELEASE_RECOVERY_TIMEOUT (2*HZ)

/* btlog string
 *
 * The string must be null-terminated, and may also include a single
 * newline before its terminating null. The string shall be given
 * as a hexadecimal number, but the first character may also be a
 * plus sign. The maximum number of Hexadecimal characters is 32
 * (128bits)
 */
#define SCSC_BTLOG_MAX_STRING_LEN       (37)
#define SCSC_BTLOG_BUF_LEN              (19)
#define SCSC_BTLOG_BUF_MAX_CHAR_TO_COPY (16)
#define SCSC_BTLOG_BUF_PREFIX_LEN        (2)

#define SCSC_ANT_MAX_TIMEOUT (20*HZ)

static u16 bt_module_irq_mask;

#ifdef CONFIG_SCSC_ANT
static DECLARE_WAIT_QUEUE_HEAD(ant_recovery_complete_queue);
#endif

static DEFINE_MUTEX(bt_start_mutex);
static DEFINE_MUTEX(bt_audio_mutex);
#ifdef CONFIG_SCSC_ANT
static DEFINE_MUTEX(ant_start_mutex);
#endif

static int recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT;

struct scsc_common_service common_service;
struct scsc_bt_service bt_service;
#ifdef CONFIG_SCSC_ANT
struct scsc_ant_service ant_service;
#endif

static int service_start_count;
#ifdef CONFIG_SCSC_ANT
static int ant_service_start_count;
#endif

static u64 bluetooth_address;
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
static char bluetooth_address_fallback[] = "00:00:00:00:00:00";
#endif
static u32 bt_info_trigger;
static u32 bt_info_interrupt;
static u32 firmware_control;
static bool firmware_control_reset = true;
static u32 firmware_btlog_enables0_low;
static u32 firmware_btlog_enables0_high;
static u32 firmware_btlog_enables1_low;
static u32 firmware_btlog_enables1_high;
static bool disable_service;

/* Audio */
#ifndef CONFIG_SOC_EXYNOS7885
static struct device *audio_device;
static bool audio_device_probed;
#else
static struct scsc_bt_audio_driver *audio_driver;
static bool audio_driver_probed;
#endif
static struct scsc_bt_audio bt_audio;

module_param(bluetooth_address, ullong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bluetooth_address,
		 "Bluetooth address");

#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
module_param_string(bluetooth_address_fallback, bluetooth_address_fallback,
		    sizeof(bluetooth_address_fallback), 0444);
MODULE_PARM_DESC(bluetooth_address_fallback,
		 "Bluetooth address as proposed by the driver");
#endif

module_param(service_start_count, int, S_IRUGO);
MODULE_PARM_DESC(service_start_count,
		"Track how many times the BT service has been started");
#ifdef CONFIG_SCSC_ANT
module_param(ant_service_start_count, int, 0444);
MODULE_PARM_DESC(ant_service_start_count,
		"Track how many times the ANT service has been started");
#endif

module_param(firmware_control, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_control, "Control how the firmware behaves");

module_param(firmware_control_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_control_reset,
		 "Controls the resetting of the firmware_control variable");

module_param(disable_service, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_service,
		 "Disables service startup");

/*
 * Service event callbacks called from mx-core when things go wrong
 */
static u8 bt_failure_notification(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	UNUSED(client);
	SCSC_TAG_INFO(BT_COMMON, "Error level %d\n", err->level);

	return err->level;
}

static bool bt_stop_on_failure(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	UNUSED(client);

	SCSC_TAG_ERR(BT_COMMON, "Error level %d\n", err->level);

	reinit_completion(&bt_service.recovery_probe_complete);
	bt_service.recovery_level = err->level;

	atomic_inc(&bt_service.error_count);

	/* Zero the shared memory on error. The A-Box does not stop using this
	 * memory immediately as designed. To prevent noise during recovery we zero the
	 * shared memory before freeing it
	 */
	mutex_lock(&bt_audio_mutex);

	if (bt_service.abox_ref != 0 && bt_audio.abox_virtual) {
		memset(bt_audio.abox_virtual->abox_to_bt_streaming_if_data, 0, SCSC_BT_AUDIO_ABOX_DATA_SIZE);
		memset(bt_audio.abox_virtual->bt_to_abox_streaming_if_data, 0, SCSC_BT_AUDIO_ABOX_DATA_SIZE);
	}

	mutex_unlock(&bt_audio_mutex);

	return false;
}

static void bt_failure_reset(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code)
{
	UNUSED(client);
	UNUSED(level);
	UNUSED(scsc_syserr_code);

	SCSC_TAG_ERR(BT_COMMON, "\n");

	wake_up(&bt_service.read_wait);
}

static int bt_ap_resumed(struct scsc_service_client *client)
{
	UNUSED(client);
	if (bt_service.interrupt_count != bt_service.last_suspend_interrupt_count)
		SCSC_TAG_INFO(BT_COMMON, "Possible Bluetooth firmware wake up detected\n");
	return 0;
}

static int bt_ap_suspended(struct scsc_service_client *client)
{
	UNUSED(client);
	bt_service.last_suspend_interrupt_count = bt_service.interrupt_count;
	return 0;
}

#ifdef CONFIG_SCSC_ANT
static u8 ant_failure_notification(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	UNUSED(client);
	SCSC_TAG_INFO(BT_COMMON, "Error level %d\n", err->level);

	return err->level;
}

static bool ant_stop_on_failure(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	UNUSED(client);

	SCSC_TAG_ERR(BT_COMMON, "\n");

	reinit_completion(&ant_service.recovery_probe_complete);
	ant_service.recovery_level = err->level;

	atomic_inc(&ant_service.error_count);

	/* Let the ANT stack call poll() to be notified about the reset asap */
	wake_up(&ant_service.read_wait);

	return false;
}

static void ant_failure_reset(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code)
{
	UNUSED(client);
	UNUSED(level);
	UNUSED(scsc_syserr_code);

	SCSC_TAG_ERR(BT_COMMON, "\n");

	wake_up(&ant_service.read_wait);
}
#endif

static void scsc_bt_shm_irq_handler(int irqbit, void *data)
{
	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(bt_service.service, irqbit);

	bt_info_interrupt++;

	wake_up(&bt_service.info_wait);
}

static struct scsc_service_client mx_bt_client = {
	.failure_notification =    bt_failure_notification,
	.stop_on_failure_v2 =      bt_stop_on_failure,
	.failure_reset_v2 =        bt_failure_reset,
	.suspend =                 bt_ap_suspended,
	.resume =                  bt_ap_resumed,
};

#ifdef CONFIG_SCSC_ANT
static struct scsc_service_client mx_ant_client = {
	.failure_notification =    ant_failure_notification,
	.stop_on_failure_v2 =      ant_stop_on_failure,
	.failure_reset_v2 =        ant_failure_reset,
};
#endif

static void slsi_sm_bt_service_cleanup_interrupts(void)
{
	u16 irq_num = 0;

	SCSC_TAG_DEBUG(BT_COMMON,
		       "unregister firmware information interrupts\n");

	if (bt_module_irq_mask & 1 << irq_num++)
		scsc_service_mifintrbit_unregister_tohost(bt_service.service,
            bt_service.bsmhcp_protocol->header.info_bg_to_ap_int_src);
	if (bt_module_irq_mask & 1 << irq_num++)
		scsc_service_mifintrbit_free_fromhost(bt_service.service,
		    bt_service.bsmhcp_protocol->header.info_ap_to_bg_int_src,
		    SCSC_MIFINTR_TARGET_R4);
}

static int slsi_sm_bt_service_init_interrupts(void) {
	int irq_ret;
	u16 irq_num = 0;

	irq_ret = scsc_service_mifintrbit_register_tohost(bt_service.service,
	    scsc_bt_shm_irq_handler, NULL);
	if (irq_ret < 0)
		return irq_ret;

	bt_service.bsmhcp_protocol->header.info_bg_to_ap_int_src = irq_ret;
	bt_module_irq_mask |= 1 << irq_num++;

	irq_ret = scsc_service_mifintrbit_alloc_fromhost(bt_service.service,
	    SCSC_MIFINTR_TARGET_R4);
	if (irq_ret < 0)
		return irq_ret;

	bt_service.bsmhcp_protocol->header.info_ap_to_bg_int_src = irq_ret;
	bt_module_irq_mask |= 1 << irq_num++;

	return 0;
}

static int slsi_sm_bt_service_cleanup_stop_service(void)
{
	int ret;

	/* Stop service first, then it's safe to release shared memory
	   resources */
	ret = scsc_mx_service_stop(bt_service.service);

	if (ret < 0 && ret != -EPERM) {
		SCSC_TAG_ERR(BT_COMMON,
			     "scsc_mx_service_stop failed err: %d\n", ret);

		/* Only trigger recovery if the service_stop did not fail because recovery is already in progress */
		if (atomic_read(&bt_service.error_count) == 0 && ret != -EILSEQ) {
			scsc_mx_service_service_failed(bt_service.service, "BT service stop failed");
			SCSC_TAG_DEBUG(BT_COMMON,
				       "force service fail complete\n");

			return ret;
		}
	}

	return 0;
}

#ifndef CONFIG_SOC_EXYNOS7885
static int slsi_bt_audio_probe(void)
{
	phys_addr_t paddr;
	size_t size;

	if (audio_device == NULL || bt_audio.dev_iommu_map == NULL) {
		SCSC_TAG_ERR(BT_COMMON, "failed audio_device %p bt_audio.dev_iommu_map %p\n",
			audio_device, bt_audio.dev_iommu_map);
		return -EFAULT;
	}

	paddr = (phys_addr_t)bt_audio.abox_physical;
	size = PAGE_ALIGN(sizeof(*bt_audio.abox_physical));

	SCSC_TAG_DEBUG(BT_COMMON, "paddr %p size %zu\n", paddr, size);

	return bt_audio.dev_iommu_map(audio_device, paddr, size);
}
#endif

#ifndef CONFIG_SOC_EXYNOS7885
/* Note A-Box memory should only be unmapped when A-Box driver is finished with it */
static void slsi_bt_audio_remove(void)
{
	size_t size;

	if (audio_device == NULL || bt_audio.dev_iommu_unmap == NULL || bt_audio.abox_physical == NULL)
		return;

	size = PAGE_ALIGN(sizeof(*bt_audio.abox_physical));
	bt_audio.dev_iommu_unmap(audio_device, size);
}
#endif

#ifdef CONFIG_SCSC_LOG_COLLECTION
static int bt_hcf_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	struct scsc_bt_hcf_collection *hcf_collect = (struct scsc_bt_hcf_collection *) collect_client->prv;
	int ret = 0;

	if (hcf_collect == NULL)
		return ret;

	SCSC_TAG_DEBUG(BT_COMMON, "Collecting BT config file\n");
	ret = scsc_log_collector_write(hcf_collect->hcf, hcf_collect->hcf_size, 1);

	return ret;
}

struct scsc_log_collector_client bt_collect_hcf_client = {
	.name = "bt_hcf",
	.type = SCSC_LOG_CHUNK_BT_HCF,
	.collect_init = NULL,
	.collect = bt_hcf_collect,
	.collect_end = NULL,
	.prv = NULL,
};
#endif

static bool scsc_recovery_in_progress()
{
#ifdef CONFIG_SCSC_ANT
	return bt_service.recovery_level != 0 || ant_service.recovery_level != 0;
#else
	return bt_service.recovery_level != 0;
#endif
}

static int slsi_sm_bt_service_cleanup()
{
	int ret = 0;

	SCSC_TAG_DEBUG(BT_COMMON, "enter\n");

	if (NULL != bt_service.service) {
		SCSC_TAG_DEBUG(BT_COMMON, "stopping debugging thread (service=%p)\n", bt_service.service);

		/* If slsi_sm_bt_service_cleanup_stop_service fails, then let
		   recovery do the rest of the deinit later. */
		if (bt_service.service_started) {
			ret = slsi_sm_bt_service_cleanup_stop_service();
			bt_service.service_started = false;

			if (ret < 0) {
				SCSC_TAG_DEBUG(BT_COMMON, "service stop failed. Recovery has been triggered\n");
				goto done_error;
			}
		}

		/* Service is stopped - ensure polling function is existed */
		SCSC_TAG_DEBUG(BT_COMMON, "wake reader/poller thread\n");
		wake_up_interruptible(&bt_service.read_wait);

		/* Unregister firmware information interrupts */
		if (bt_service.bsmhcp_protocol) {
			slsi_sm_bt_service_cleanup_interrupts();
			bt_module_irq_mask = 0;
		}

		/* Shut down the shared memory interface */
		SCSC_TAG_DEBUG(BT_COMMON,
			"cleanup protocol structure and main interrupts\n");
		scsc_bt_shm_exit();

		/* Cleanup AVDTP detections */
		SCSC_TAG_DEBUG(BT_COMMON,
			"cleanup ongoing avdtp detections\n");
		scsc_avdtp_detect_exit();

		/* Report quality of service statistics */
		scsc_bt_qos_service_stop();

		mutex_lock(&bt_audio_mutex);
#ifndef CONFIG_SOC_EXYNOS7885
		if (audio_device) {
			bt_audio.dev			= NULL;
			bt_audio.abox_virtual		= NULL;
			bt_audio.abox_physical		= NULL;
		}
#else
		if (audio_driver) {
			if (audio_driver_probed)
				audio_driver->remove(&bt_audio);

			bt_audio.dev           = NULL;
			bt_audio.abox_virtual  = NULL;
			bt_audio.abox_physical = NULL;
			audio_driver_probed    = false;
		}
#endif
		mutex_unlock(&bt_audio_mutex);

#ifdef CONFIG_SCSC_LOG_COLLECTION
		/* Deinit HCF log collection */
		scsc_log_collector_unregister_client(&bt_collect_hcf_client);
		bt_collect_hcf_client.prv = NULL;

		if (bt_service.hcf_collection.hcf) {
			/* Reset HCF pointer - memory will be freed later */
			bt_service.hcf_collection.hcf_size = 0;
			bt_service.hcf_collection.hcf = NULL;
		}
#endif

		/* Release the shared memory */
		SCSC_TAG_DEBUG(BT_COMMON,
			"free memory allocated in the shared DRAM pool\n");
		if (bt_service.config_ref != 0) {
			scsc_mx_service_mifram_free(bt_service.service,
					bt_service.config_ref);
			bt_service.config_ref = 0;
		}
		if (bt_service.bsmhcp_ref != 0) {
			scsc_mx_service_mifram_free(bt_service.service,
					bt_service.bsmhcp_ref);
			bt_service.bsmhcp_ref = 0;
		}
		if (bt_service.bhcs_ref != 0) {
			scsc_mx_service_mifram_free(bt_service.service,
					bt_service.bhcs_ref);
			bt_service.bhcs_ref = 0;
		}

		SCSC_TAG_DEBUG(BT_COMMON, "closing service...\n");

		ret = scsc_mx_service_close(bt_service.service);

		if (ret < 0 && ret != -EPERM) {
			int retry_counter;

			SCSC_TAG_DEBUG(BT_COMMON,
				"scsc_mx_service_close failed\n");

			/**
			 * Error handling in progress - try and close again
			 * later. The service close call shall remain blocked
			 * until close service is successful. Will try up to
			 * 30 seconds.
			 */
			for (retry_counter = 0; retry_counter < SLSI_BT_SERVICE_CLOSE_RETRY; retry_counter++) {
				msleep(500);

				ret = scsc_mx_service_close(bt_service.service);

				if (ret == 0) {
					SCSC_TAG_DEBUG(BT_COMMON,
						"scsc_mx_service_close closed after %d attempts\n",
						retry_counter + 1);
					break;
				}
			}

			if (retry_counter + 1 == SLSI_BT_SERVICE_CLOSE_RETRY)
					SCSC_TAG_ERR(BT_COMMON, "scsc_mx_service_close failed %d times\n",
						 SLSI_BT_SERVICE_CLOSE_RETRY);
		}
		bt_service.service = NULL;

		SCSC_TAG_DEBUG(BT_COMMON,
			"notify the KIC subsystem of the shutdown\n");
		slsi_kic_system_event(
			slsi_kic_system_event_category_deinitialisation,
			slsi_kic_system_events_bt_off,
			0);
	}

	atomic_set(&bt_service.error_count, 0);

	/* Release write wake lock if held */
	if (wake_lock_active(&bt_service.write_wake_lock)) {
		bt_service.write_wake_unlock_count++;
		wake_unlock(&bt_service.write_wake_lock);
	}

	SCSC_TAG_DEBUG(BT_COMMON, "complete\n");
	return 0;

done_error:
	return -EIO;
}

#ifdef CONFIG_SCSC_ANT
static int slsi_sm_ant_service_cleanup_stop_service(void)
{
	int ret;

	/* Stop service first, then it's safe to release shared memory
	 * resources
	 */
	ret = scsc_mx_service_stop(ant_service.service);
	if (ret < 0 && ret != -EPERM) {
		SCSC_TAG_ERR(BT_COMMON,
			     "scsc_mx_service_stop failed err: %d\n", ret);
		if (atomic_read(&ant_service.error_count) == 0 && ret != -EILSEQ) {
			scsc_mx_service_service_failed(ant_service.service, "ANT service stop failed");
			SCSC_TAG_DEBUG(BT_COMMON, "force service fail complete\n");
			return ret;
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_SCSC_ANT
static int slsi_sm_ant_service_cleanup()
{
	int ret = 0;

	SCSC_TAG_DEBUG(BT_COMMON, "enter\n");

	if (ant_service.service != NULL) {
		SCSC_TAG_DEBUG(BT_COMMON, "stopping debugging thread\n");

		/* If slsi_sm_ant_service_cleanup_stop_service fails, then let
		 * recovery do the rest of the deinit later.
		 **/
		if (ant_service.service_started) {
			ret = slsi_sm_ant_service_cleanup_stop_service();
			ant_service.service_started = false;

			if (ret < 0) {
				SCSC_TAG_DEBUG(BT_COMMON, "service stop failed. Recovery has been triggered\n");
				goto done_error;
			}
		}

		/* Service is stopped - ensure polling function is existed */
		SCSC_TAG_DEBUG(BT_COMMON, "wake reader/poller thread\n");
		wake_up_interruptible(&ant_service.read_wait);

		/* Shut down the shared memory interface */
		SCSC_TAG_DEBUG(BT_COMMON,
			"cleanup protocol structure and main interrupts\n");
		scsc_ant_shm_exit();

		/* Release the shared memory */
		SCSC_TAG_DEBUG(BT_COMMON,
			"free memory allocated in the shared DRAM pool\n");
		if (ant_service.config_ref != 0) {
			scsc_mx_service_mifram_free(ant_service.service,
					ant_service.config_ref);
			ant_service.config_ref = 0;
		}
		if (ant_service.asmhcp_ref != 0) {
			scsc_mx_service_mifram_free(ant_service.service,
					ant_service.asmhcp_ref);
			ant_service.asmhcp_ref = 0;
		}
		if (ant_service.bhcs_ref != 0) {
			scsc_mx_service_mifram_free(ant_service.service,
					ant_service.bhcs_ref);
			ant_service.bhcs_ref = 0;
		}

		SCSC_TAG_DEBUG(BT_COMMON, "closing ant service...\n");

		ret = scsc_mx_service_close(ant_service.service);

		if (ret < 0 && ret != -EPERM) {
			int retry_counter;

			SCSC_TAG_DEBUG(BT_COMMON,
				"scsc_mx_service_close failed\n");

			/**
			 * Error handling in progress - try and close again
			 * later. The service close call shall remain blocked
			 * until close service is successful. Will try up to
			 * 30 seconds.
			 */
			for (retry_counter = 0;
			     retry_counter < SLSI_BT_SERVICE_CLOSE_RETRY;
			     retry_counter++) {
				msleep(500);
				ret = scsc_mx_service_close(ant_service.service);
				if (ret == 0) {
					SCSC_TAG_DEBUG(BT_COMMON,
						"scsc_mx_service_close closed after %d attempts\n",
						retry_counter + 1);
					break;
				}
			}

			if (retry_counter + 1 == SLSI_BT_SERVICE_CLOSE_RETRY)
					SCSC_TAG_ERR(BT_COMMON,
						     "scsc_mx_service_close failed %d times\n",
						     SLSI_BT_SERVICE_CLOSE_RETRY);
		}
		ant_service.service = NULL;

		SCSC_TAG_DEBUG(BT_COMMON,
			"notify the KIC subsystem of the shutdown\n");
		slsi_kic_system_event(
			slsi_kic_system_event_category_deinitialisation,
			slsi_kic_system_events_ant_off,
			0);
	}

	atomic_set(&ant_service.error_count, 0);

	SCSC_TAG_DEBUG(BT_COMMON, "complete\n");
	return 0;

done_error:
	return -EIO;
}
#endif

static int setup_bhcs(struct scsc_service *service,
		      struct BHCS *bhcs,
		      uint32_t protocol_ref,
		      uint32_t protocol_length,
		      scsc_mifram_ref *config_ref,
		      scsc_mifram_ref *bhcs_ref)
{
	int err = 0;
	unsigned char *conf_ptr;
	const struct firmware *firm = NULL;
	/* Fill the configuration information */
	bhcs->version = BHCS_VERSION;
	bhcs->bsmhcp_protocol_offset = protocol_ref;
	bhcs->bsmhcp_protocol_length = protocol_length;
	bhcs->configuration_offset = 0;
	bhcs->configuration_length = 0;
	bhcs->bluetooth_address_lap = 0;
	bhcs->bluetooth_address_uap = 0;
	bhcs->bluetooth_address_nap = 0;

	/* Request the configuration file */
	SCSC_TAG_DEBUG(BT_COMMON,
		"loading configuration: " SCSC_BT_CONF "\n");
	err = mx140_file_request_conf(common_service.maxwell_core,
				      &firm, "bluetooth", SCSC_BT_CONF);
	if (err) {
		/* Not found - just silently ignore this */
		SCSC_TAG_DEBUG(BT_COMMON, "configuration not found\n");
		*config_ref = 0;
	} else if (firm && firm->size) {
		SCSC_TAG_DEBUG(BT_COMMON,
			       "configuration size = %zu\n", firm->size);

		/* Allocate a region for the data */
		err = scsc_mx_service_mifram_alloc(service,
						   firm->size,
						   config_ref,
						   BSMHCP_ALIGNMENT);
		if (err) {
			SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
			mx140_file_release_conf(common_service.maxwell_core, firm);
			return -EINVAL;
		}

		/* Map the region to a memory pointer */
		conf_ptr = scsc_mx_service_mif_addr_to_ptr(service,
						*config_ref);
		if (conf_ptr == NULL) {
			SCSC_TAG_ERR(BT_COMMON,
				     "couldn't map kmem to bhcs_ref 0x%08x\n",
				     (u32)*bhcs_ref);
			mx140_file_release_conf(common_service.maxwell_core, firm);
			return -EINVAL;
		}

		/* Copy the configuration data to the shared memory area */
		memcpy(conf_ptr, firm->data, firm->size);
		bhcs->configuration_offset = *config_ref;
		bhcs->configuration_length = firm->size;

		/* Relase the configuration information */
		mx140_file_release_conf(common_service.maxwell_core, firm);
		firm = NULL;
	} else {
		/* Empty configuration - just silently ignore this */
		SCSC_TAG_DEBUG(BT_COMMON, "empty configuration\n");
		*config_ref = 0;

		/* Relase the configuration information */
		mx140_file_release_conf(common_service.maxwell_core, firm);
		firm = NULL;
	}

#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
	bhcs->bluetooth_address_nap =
		(exynos_soc_info.unique_id & 0x000000FFFF00) >> 8;
	bhcs->bluetooth_address_uap =
		(exynos_soc_info.unique_id & 0x0000000000FF);
	bhcs->bluetooth_address_lap =
		(exynos_soc_info.unique_id & 0xFFFFFF000000) >> 24;
#endif

	if (bluetooth_address) {
		SCSC_TAG_INFO(BT_COMMON,
			      "using stack supplied Bluetooth address\n");
		bhcs->bluetooth_address_nap =
			(bluetooth_address & 0xFFFF00000000) >> 32;
		bhcs->bluetooth_address_uap =
			(bluetooth_address & 0x0000FF000000) >> 24;
		bhcs->bluetooth_address_lap =
			(bluetooth_address & 0x000000FFFFFF);
	}

#ifdef SCSC_BT_ADDR
	/* Request the Bluetooth address file */
	SCSC_TAG_DEBUG(BT_COMMON,
		"loading Bluetooth address configuration file: "
		SCSC_BT_ADDR "\n");
	err = mx140_request_file(common_service.maxwell_core, SCSC_BT_ADDR, &firm);
	if (err) {
		/* Not found - just silently ignore this */
		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth address not found\n");
	} else if (firm && firm->size) {
		u32 u[SCSC_BT_ADDR_LEN];

#ifdef CONFIG_SCSC_BT_BLUEZ
		/* Convert the data into a native format */
		if (sscanf(firm->data, "%04x %02X %06x",
			   &u[0], &u[1], &u[2])
		    == SCSC_BT_ADDR_LEN) {
			bhcs->bluetooth_address_lap = u[2];
			bhcs->bluetooth_address_uap = u[1];
			bhcs->bluetooth_address_nap = u[0];
		} else
			SCSC_TAG_WARNING(BT_COMMON,
				"data size incorrect = %zu\n", firm->size);
#else
		/* Convert the data into a native format */
		if (sscanf(firm->data, "%02X:%02X:%02X:%02X:%02X:%02X",
			   &u[0], &u[1], &u[2], &u[3], &u[4], &u[5])
		    == SCSC_BT_ADDR_LEN) {
			bhcs->bluetooth_address_lap =
				(u[3] << 16) | (u[4] << 8) | u[5];
			bhcs->bluetooth_address_uap = u[2];
			bhcs->bluetooth_address_nap = (u[0] << 8) | u[1];
		} else
			SCSC_TAG_WARNING(BT_COMMON,
				"data size incorrect = %zu\n", firm->size);
#endif
		/* Relase the configuration information */
		mx140_release_file(common_service.maxwell_core, firm);
		firm = NULL;
	} else {
		SCSC_TAG_DEBUG(BT_COMMON, "empty Bluetooth address\n");
		mx140_release_file(common_service.maxwell_core, firm);
		firm = NULL;
	}
#endif

#ifdef CONFIG_SCSC_DEBUG
	SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth address: %04X:%02X:%06X\n",
		       bhcs->bluetooth_address_nap,
		       bhcs->bluetooth_address_uap,
		       bhcs->bluetooth_address_lap);

	/* Always print Bluetooth Address in Kernel log */
	printk(KERN_INFO "Bluetooth address: %04X:%02X:%06X\n",
		bhcs->bluetooth_address_nap,
		bhcs->bluetooth_address_uap,
		bhcs->bluetooth_address_lap);
#endif /* CONFIG_SCSC_DEBUG */

	return err;
}

/* Start the BT service */
int slsi_sm_bt_service_start(void)
{
	int                   err = 0;
	struct BHCS           *bhcs;

	++service_start_count;

	/* Lock the start/stop procedures to handle multiple application
	 * starting the sercice
	 */
	mutex_lock(&bt_start_mutex);

	if (disable_service) {
		SCSC_TAG_WARNING(BT_COMMON, "service disabled\n");
		mutex_unlock(&bt_start_mutex);
		return -EBUSY;
	}

	/* is BT/ANT recovery in progress? */
	if (scsc_recovery_in_progress()) {
		SCSC_TAG_WARNING(BT_COMMON, "recovery in progress\n");
		mutex_unlock(&bt_start_mutex);
		return -EFAULT;
	}

	/* Has probe been called */
	if (common_service.maxwell_core == NULL) {
		SCSC_TAG_WARNING(BT_COMMON, "service probe not arrived\n");
		mutex_unlock(&bt_start_mutex);
		return -EFAULT;
	}

	/* Is this the first service to enter */
	if (atomic_inc_return(&bt_service.service_users) > 1) {
		SCSC_TAG_WARNING(BT_COMMON, "service already opened\n");

		if (!bt_service.service_started) {
			SCSC_TAG_DEBUG(BT_COMMON, "service not started, returning error\n");
			err = -EFAULT;
			atomic_dec(&bt_service.service_users);
		}

		mutex_unlock(&bt_start_mutex);
		return err;
	}

	/* Open service - will download FW - will set MBOX0 with Starting
	 * address
	 */
	SCSC_TAG_DEBUG(BT_COMMON,
		       "open Bluetooth service id %d opened %d times\n",
		       SCSC_SERVICE_ID_BT, service_start_count);
	wake_lock(&bt_service.service_wake_lock);
	bt_service.service = scsc_mx_service_open(common_service.maxwell_core,
						  SCSC_SERVICE_ID_BT,
						  &mx_bt_client,
						  &err);
	if (!bt_service.service) {
		SCSC_TAG_WARNING(BT_COMMON, "service open failed %d\n", err);
		atomic_dec(&bt_service.service_users);
		wake_unlock(&bt_service.service_wake_lock);
		mutex_unlock(&bt_start_mutex);
		return -EINVAL;
	}

	/* Shorter completion timeout if autorecovery is disabled, as it will
	 * never be signalled.
	 */
	if (mxman_recovery_disabled())
		recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_DISABLED_TIMEOUT;
	else
		recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT;

	/* Get shared memory region for the configuration structure from
	 * the MIF
	 */
	SCSC_TAG_DEBUG(BT_COMMON, "allocate mifram regions\n");
	err = scsc_mx_service_mifram_alloc(bt_service.service,
					   sizeof(struct BHCS),
					   &bt_service.bhcs_ref,
					   BSMHCP_ALIGNMENT);
	if (err) {
		SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
		err = -EINVAL;
		goto exit;
	}

	/* Get shared memory region for the protocol structure from the MIF */
	err = scsc_mx_service_mifram_alloc(bt_service.service,
					   sizeof(struct BSMHCP_PROTOCOL),
					   &bt_service.bsmhcp_ref,
					   BSMHCP_ALIGNMENT);
	if (err) {
		SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
		err = -EINVAL;
		goto exit;
	}

	/* The A-Box driver must have registered before reaching this point
	 * otherwise there is no audio routing
	 */
#ifndef CONFIG_SOC_EXYNOS7885
	if (audio_device != NULL) {
#else
	if (audio_driver != NULL) {
#endif
		/* Get shared memory region for the A-Box structure from the MIF.
		 * The allocated memory is aligned to 4kB, but this is going to work
		 * only if the physical start address of the 4MB region is aligned
		 * to 4kB (which maybe will be always the case).
		 */

		/* On 9610, do not unmap previously mapped memory from IOMMU.
		 * It may still be used by A-Box.
		 */

		err = scsc_mx_service_mif_ptr_to_addr(bt_service.service,
				scsc_mx_service_get_bt_audio_abox(bt_service.service),
				&bt_service.abox_ref);
		if (err) {
			SCSC_TAG_WARNING(BT_COMMON, "scsc_mx_service_mif_ptr_to_addr failed\n");
			err = -EINVAL;
			goto exit;
		}
		/* irrespective of the technical definition of probe - wrt to memory allocation it has been */

		bt_audio.abox_virtual = (struct scsc_bt_audio_abox *)
						scsc_mx_service_mif_addr_to_ptr(
							bt_service.service,
							bt_service.abox_ref);

		memset(bt_audio.abox_virtual, 0, sizeof(struct scsc_bt_audio_abox));

		bt_audio.abox_virtual->magic_value                      = SCSC_BT_AUDIO_ABOX_MAGIC_VALUE;
		bt_audio.abox_virtual->version_major                    = SCSC_BT_AUDIO_ABOX_VERSION_MAJOR;
		bt_audio.abox_virtual->version_minor                    = SCSC_BT_AUDIO_ABOX_VERSION_MINOR;
		bt_audio.abox_virtual->abox_to_bt_streaming_if_0_size   = SCSC_BT_AUDIO_ABOX_IF_0_SIZE;
		bt_audio.abox_virtual->bt_to_abox_streaming_if_0_size   = SCSC_BT_AUDIO_ABOX_IF_0_SIZE;
		bt_audio.abox_virtual->abox_to_bt_streaming_if_1_size   = SCSC_BT_AUDIO_ABOX_IF_1_SIZE;
		bt_audio.abox_virtual->abox_to_bt_streaming_if_1_offset = SCSC_BT_AUDIO_ABOX_IF_0_SIZE;
		bt_audio.abox_virtual->bt_to_abox_streaming_if_1_size   = SCSC_BT_AUDIO_ABOX_IF_1_SIZE;
		bt_audio.abox_virtual->bt_to_abox_streaming_if_1_offset = SCSC_BT_AUDIO_ABOX_IF_0_SIZE;

		/* Resolve the physical address of the structure */
		bt_audio.abox_physical = (struct scsc_bt_audio_abox *)scsc_mx_service_mif_addr_to_phys(
									bt_service.service,
									bt_service.abox_ref);


		bt_audio.dev = bt_service.dev;
	}

	/* Map the configuration pointer */
	bhcs = (struct BHCS *) scsc_mx_service_mif_addr_to_ptr(
					bt_service.service,
					bt_service.bhcs_ref);
	if (bhcs == NULL) {
		SCSC_TAG_ERR(BT_COMMON,
			     "couldn't map kmem to bhcs_ref 0x%08x\n",
			     (u32)bt_service.bhcs_ref);
		err = -ENOMEM;
		goto exit;
	}

	SCSC_TAG_INFO(BT_COMMON,
	    "regions (bhcs_ref=0x%08x, bsmhcp_ref=0x%08x, config_ref=0x%08x, abox_ref=0x%08x)\n",
				 bt_service.bhcs_ref,
				 bt_service.bsmhcp_ref,
				 bt_service.config_ref,
				 bt_service.abox_ref);
	SCSC_TAG_INFO(BT_COMMON, "version=%u\n", BHCS_VERSION);

	err = setup_bhcs(bt_service.service,
			 bhcs,
			 bt_service.bsmhcp_ref,
			 sizeof(struct BSMHCP_PROTOCOL),
			 &bt_service.config_ref,
			 &bt_service.bhcs_ref);

#ifdef CONFIG_SCSC_LOG_COLLECTION
	/* Save the binary BT config ref and register for
	 * log collector to collect the hcf file
	 */
	if (bhcs->configuration_length > 0) {
		bt_service.hcf_collection.hcf =
				scsc_mx_service_mif_addr_to_ptr(bt_service.service,
								bt_service.config_ref);
		bt_service.hcf_collection.hcf_size = bhcs->configuration_length;
		bt_collect_hcf_client.prv = &bt_service.hcf_collection;
		scsc_log_collector_register_client(&bt_collect_hcf_client);
	}
#endif

	if (err == -EINVAL)
		goto exit;

	/* Initialise the shared-memory interface */
	err = scsc_bt_shm_init();
	if (err) {
		SCSC_TAG_ERR(BT_COMMON, "scsc_bt_shm_init err %d\n", err);
		err = -EINVAL;
		goto exit;
	}

	err = slsi_sm_bt_service_init_interrupts();
	if (err < 0)
		goto exit;

	bt_service.bsmhcp_protocol->header.btlog_enables0_low = firmware_btlog_enables0_low;
	bt_service.bsmhcp_protocol->header.btlog_enables0_high = firmware_btlog_enables0_high;
	bt_service.bsmhcp_protocol->header.btlog_enables1_low = firmware_btlog_enables1_low;
	bt_service.bsmhcp_protocol->header.btlog_enables1_high = firmware_btlog_enables1_high;
	bt_service.bsmhcp_protocol->header.firmware_control = firmware_control;
	bt_service.bsmhcp_protocol->header.abox_offset = bt_service.abox_ref;
	bt_service.bsmhcp_protocol->header.abox_length = sizeof(struct scsc_bt_audio_abox);

	SCSC_TAG_DEBUG(BT_COMMON,
		       "firmware_control=0x%08x, firmware_control_reset=%u\n",
		       firmware_control, firmware_control_reset);

	if (firmware_control_reset)
		firmware_control = 0;

	/* Start service last - after setting up shared memory resources */
	SCSC_TAG_DEBUG(BT_COMMON, "starting Bluetooth service\n");
	err = scsc_mx_service_start(bt_service.service, bt_service.bhcs_ref);
	if (err < 0) {
		SCSC_TAG_ERR(BT_COMMON, "scsc_mx_service_start err %d\n", err);
		err = -EINVAL;
	} else {
		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth service running\n");
		bt_service.service_started = true;
		slsi_kic_system_event(
			slsi_kic_system_event_category_initialisation,
			slsi_kic_system_events_bt_on, 0);

		mutex_lock(&bt_audio_mutex);
#ifndef CONFIG_SOC_EXYNOS7885
		if (audio_device && !audio_device_probed) {
			err = slsi_bt_audio_probe();

			audio_device_probed = true;
		}
#else
		if (audio_driver && !audio_driver_probed) {
			audio_driver->probe(audio_driver, &bt_audio);

			audio_driver_probed = true;
		}
#endif
		mutex_unlock(&bt_audio_mutex);
	}

	if (bt_service.bsmhcp_protocol->header.firmware_features &
	    BSMHCP_FEATURE_M4_INTERRUPTS)
		SCSC_TAG_DEBUG(BT_COMMON, "features enabled: M4_INTERRUPTS\n");

exit:
	if (err < 0) {
		if (slsi_sm_bt_service_cleanup() == 0)
			atomic_dec(&bt_service.service_users);
	}

	wake_unlock(&bt_service.service_wake_lock);
	mutex_unlock(&bt_start_mutex);
	return err;
}

#ifdef CONFIG_SCSC_ANT
/* Start the ANT service */
int slsi_sm_ant_service_start(void)
{
	int                   err = 0;
	struct BHCS           *bhcs;

	++ant_service_start_count;

	/* Lock the start/stop procedures to handle multiple application
	 * starting the sercice
	 */
	mutex_lock(&ant_start_mutex);

	if (disable_service) {
		SCSC_TAG_WARNING(BT_COMMON, "service disabled\n");
		mutex_unlock(&ant_start_mutex);
		return -EBUSY;
	}

	/* Has probe been called */
	if (common_service.maxwell_core == NULL) {
		SCSC_TAG_WARNING(BT_COMMON, "service probe not arrived\n");
		mutex_unlock(&ant_start_mutex);
		return -EFAULT;
	}

	/* Is this the first service to enter */
	if (atomic_inc_return(&ant_service.service_users) > 1) {
		SCSC_TAG_WARNING(BT_COMMON, "service already opened\n");

		if (!ant_service.service_started) {
			SCSC_TAG_DEBUG(BT_COMMON, "service not started, returning error\n");
			err = -EFAULT;
			atomic_dec(&bt_service.service_users);
		}

		mutex_unlock(&ant_start_mutex);
		return err;
	}

	/* Open service - will download FW - will set MBOX0 with Starting
	 * address
	 */
	SCSC_TAG_DEBUG(BT_COMMON,
		       "open ANT service id %d opened %d times\n",
		       SCSC_SERVICE_ID_ANT, ant_service_start_count);

	wake_lock(&ant_service.service_wake_lock);
	ant_service.service = scsc_mx_service_open(common_service.maxwell_core,
						  SCSC_SERVICE_ID_ANT,
						  &mx_ant_client,
						  &err);
	if (!ant_service.service) {
		SCSC_TAG_WARNING(BT_COMMON, "ant service open failed %d\n", err);
		if (err < 0) {
			atomic_dec(&ant_service.service_users);
			wake_unlock(&ant_service.service_wake_lock);
			mutex_unlock(&ant_start_mutex);
			return -EINVAL;
		}
	}

	/* Shorter completion timeout if autorecovery is disabled, as it will
	 * never be signalled.
	 */
	if (mxman_recovery_disabled())
		recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_DISABLED_TIMEOUT;
	else
		recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT;


	/* Get shared memory region for the configuration structure from
	 * the MIF
	 */
	SCSC_TAG_DEBUG(BT_COMMON, "allocate mifram regions\n");
	err = scsc_mx_service_mifram_alloc(ant_service.service,
					   sizeof(struct BHCS),
					   &ant_service.bhcs_ref,
					   BSMHCP_ALIGNMENT);
	if (err) {
		SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
		err = -EINVAL;
		goto exit;
	}

	/* Get shared memory region for the protocol structure from the MIF */
	err = scsc_mx_service_mifram_alloc(ant_service.service,
					   sizeof(struct ASMHCP_PROTOCOL),
					   &ant_service.asmhcp_ref,
					   BSMHCP_ALIGNMENT);
	if (err) {
		SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
		err = -EINVAL;
		goto exit;
	}

	/* Map the configuration pointer */
	bhcs = (struct BHCS *) scsc_mx_service_mif_addr_to_ptr(
					ant_service.service,
					ant_service.bhcs_ref);
	if (bhcs == NULL) {
		SCSC_TAG_ERR(BT_COMMON,
			     "couldn't map kmem to bhcs_ref 0x%08x\n",
			     (u32)ant_service.bhcs_ref);
		err = -ENOMEM;
		goto exit;
	}

	SCSC_TAG_INFO(BT_COMMON,
	    "regions (bhcs_ref=0x%08x, bsmhcp_ref=0x%08x, config_ref=0x%08x)\n",
				 ant_service.bhcs_ref,
				 ant_service.asmhcp_ref,
				 ant_service.config_ref);
	SCSC_TAG_INFO(BT_COMMON, "version=%u\n", BHCS_VERSION);

	err = setup_bhcs(ant_service.service,
			 bhcs,
			 ant_service.asmhcp_ref,
			 sizeof(struct ASMHCP_PROTOCOL),
			 &ant_service.config_ref,
			 &ant_service.bhcs_ref);

	if (err == -EINVAL)
		goto exit;

	/* Initialise the shared-memory interface */
	err = scsc_ant_shm_init();
	if (err) {
		SCSC_TAG_ERR(BT_COMMON, "scsc_ant_shm_init err %d\n", err);
		err = -EINVAL;
		goto exit;
	}

	ant_service.asmhcp_protocol->header.btlog_enables0_low = firmware_btlog_enables0_low;
	ant_service.asmhcp_protocol->header.btlog_enables0_high = firmware_btlog_enables0_high;
	ant_service.asmhcp_protocol->header.btlog_enables1_low = firmware_btlog_enables1_low;
	ant_service.asmhcp_protocol->header.btlog_enables1_high = firmware_btlog_enables1_high;
	ant_service.asmhcp_protocol->header.firmware_control = firmware_control;

	SCSC_TAG_DEBUG(BT_COMMON,
		       "firmware_control=0x%08x, firmware_control_reset=%u\n",
		       firmware_control, firmware_control_reset);

	if (firmware_control_reset)
		firmware_control = 0;

	/* Start service last - after setting up shared memory resources */
	SCSC_TAG_DEBUG(BT_COMMON, "starting ANT service\n");
	err = scsc_mx_service_start(ant_service.service, ant_service.bhcs_ref);
	if (err < 0) {
		SCSC_TAG_ERR(BT_COMMON, "scsc_mx_service_start err %d\n", err);
		err = -EINVAL;
	} else {
		SCSC_TAG_DEBUG(BT_COMMON, "Ant service running\n");
		ant_service.service_started = true;
		slsi_kic_system_event(
			slsi_kic_system_event_category_initialisation,
			slsi_kic_system_events_ant_on, 0);
	}

exit:
	if (err < 0) {
		if (slsi_sm_ant_service_cleanup() == 0)
			atomic_dec(&ant_service.service_users);
	}

	wake_unlock(&ant_service.service_wake_lock);
	mutex_unlock(&ant_start_mutex);
	return err;
}
#endif

/* Stop the BT service */
static int slsi_sm_bt_service_stop()
{
	SCSC_TAG_INFO(BT_COMMON, "bt service users %u\n", atomic_read(&bt_service.service_users));

	if (1 < atomic_read(&bt_service.service_users)) {
		atomic_dec(&bt_service.service_users);
	} else if (1 == atomic_read(&bt_service.service_users)) {
		if (slsi_sm_bt_service_cleanup() == 0)
			atomic_dec(&bt_service.service_users);
		else
			return -EIO;
	}

	return 0;
}

#ifdef CONFIG_SCSC_ANT
/* Stop the ANT service */
static int slsi_sm_ant_service_stop()
{
	SCSC_TAG_INFO(BT_COMMON, "ant service users %u\n", atomic_read(&ant_service.service_users));

	if (atomic_read(&ant_service.service_users) > 1) {
		atomic_dec(&ant_service.service_users);
	} else if (atomic_read(&ant_service.service_users) == 1) {
		if (slsi_sm_ant_service_cleanup() == 0)
			atomic_dec(&ant_service.service_users);
		else
			return -EIO;
	}

	return 0;
}
#endif

static int scsc_bt_h4_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	SCSC_TAG_INFO(BT_COMMON, "(h4_users=%u)\n", bt_service.h4_users ? 1 : 0);

	if (!bt_service.h4_users) {
		ret = slsi_sm_bt_service_start();
		if (0 == ret)
			bt_service.h4_users = true;
	} else {
		ret = -EBUSY;
	}

	return ret;
}

static int scsc_bt_h4_release(struct inode *inode, struct file *file)
{
	SCSC_TAG_INFO(BT_COMMON, "\n");

	mutex_lock(&bt_start_mutex);
	wake_lock(&bt_service.service_wake_lock);
	/* service_started will only be false in case we timed out during
	 * recovery waiting for the release from the user.
	 */
	if (bt_service.recovery_level < MX_SYSERR_LEVEL_7 && bt_service.service_started) {
		if (slsi_sm_bt_service_stop() == -EIO)
			goto recovery;

		/* Clear all control structures */
		bt_service.read_offset = 0;
		bt_service.read_operation = 0;
		bt_service.read_index = 0;
		bt_service.h4_write_offset = 0;

		/* The recovery flag can be set in case of crossing release and
		 * recovery signaling. It's safe to check the flag here since
		 * the bt_start_mutex guarantees that the remove/probe callbacks
		 * will be called after the mutex is released. Jump to the
		 * normal recovery path.
		 */
		if (bt_service.recovery_level >= MX_SYSERR_LEVEL_7)
			goto recovery;
#ifdef CONFIG_SCSC_ANT
		else if (bt_service.recovery_level >= MX_SYSERR_LEVEL_5) {
			/* Try to lock ant_start mutex and synchronize release with ANT.
			 * If this does not succeed, it means that ANT is either opening
			 * or closing and in any case not waiting for BT to synchronize shutdown.
			 */
			if (mutex_trylock(&ant_start_mutex)) {
				if (ant_service.recovery_level >= MX_SYSERR_LEVEL_5 &&
				    ant_service.recovery_level < MX_SYSERR_LEVEL_7) {
					int timeout_res;

					mutex_unlock(&bt_start_mutex);
					mutex_unlock(&ant_start_mutex);

					timeout_res = wait_for_completion_timeout(
					    &ant_service.release_complete,
					    msecs_to_jiffies(SLSI_BT_SERVICE_RELEASE_RECOVERY_TIMEOUT));
					mutex_lock(&bt_start_mutex);

					if (timeout_res == 0)
						SCSC_TAG_INFO(BT_COMMON, "timeout waiting for ant release\n");

					/* Ant service will reset recovery_level if not syncing with BT service */
					if (timeout_res > 0) {
						mutex_lock(&ant_start_mutex);
						ant_service.recovery_level = 0;
						mutex_unlock(&ant_start_mutex);
					}

					/* ant_start_mutex not needed here, since this variable is
					 * guarded with bt_start_mutex in ant release path.
					 */
					reinit_completion(&ant_service.release_complete);

					bt_service.recovery_level = 0;
					wake_up_interruptible(&ant_recovery_complete_queue);
				} else {
					mutex_unlock(&ant_start_mutex);
				}
			}
		}
#endif

		bt_service.recovery_level = 0;

		wake_unlock(&bt_service.service_wake_lock);
		bt_service.h4_users = false;
		mutex_unlock(&bt_start_mutex);
	} else {
		int ret;
recovery:
		/* recovery_release_complete will already have been re-inited if
		 * the BT service was closed before the release call, so don't complete
		 * it in that case.
		 */
		if (bt_service.service_started)
			complete_all(&bt_service.recovery_release_complete);
		wake_unlock(&bt_service.service_wake_lock);
		mutex_unlock(&bt_start_mutex);

		ret = wait_for_completion_timeout(&bt_service.recovery_probe_complete,
		       msecs_to_jiffies(recovery_timeout));
		if (ret == 0)
			SCSC_TAG_INFO(BT_COMMON, "recovery_probe_complete timeout\n");

		bt_service.h4_users = false;
	}

	return 0;
}

#ifdef CONFIG_SCSC_ANT
static int scsc_ant_release(struct inode *inode, struct file *file)
{
	SCSC_TAG_INFO(BT_COMMON, "\n");

	mutex_lock(&ant_start_mutex);
	wake_lock(&ant_service.service_wake_lock);
	/* service_started will only be false in case we timed out during
	 * recovery waiting for the release from the user.
	 */
	if (ant_service.recovery_level < MX_SYSERR_LEVEL_7 && ant_service.service_started) {
		bool reset_recovery_level = true;
		if (slsi_sm_ant_service_stop() == -EIO)
			goto recovery;

		/* Clear all control structures */
		ant_service.read_offset = 0;
		ant_service.read_operation = 0;
		ant_service.read_index = 0;
		ant_service.ant_write_offset = 0;

		/* The recovery flag can be set in case of crossing release and
		 * recovery signaling. It's safe to check the flag here since
		 * the bt_start_mutex guarantees that the remove/probe callbacks
		 * will be called after the mutex is released. Jump to the
		 * normal recovery path.
		 */
		if (ant_service.recovery_level >= MX_SYSERR_LEVEL_7)
			goto recovery;
		else if (ant_service.recovery_level >= MX_SYSERR_LEVEL_5) {
			/* Try to lock bt_start mutex and synchronize release with BT.
			 * If this does not succeed, it means that BT is either opening
			 * or closing and in any case not waiting for ANT to synchronize shutdown.
			 */
			if (mutex_trylock(&bt_start_mutex)) {
				if (bt_service.h4_users &&
				    bt_service.recovery_level >= MX_SYSERR_LEVEL_5 &&
				    bt_service.recovery_level < MX_SYSERR_LEVEL_7) {
					complete_all(&ant_service.release_complete);
					/* BT will handle resetting in case we need to sync */
					reset_recovery_level = false;
				}
				mutex_unlock(&bt_start_mutex);
			}
		}

		if (reset_recovery_level)
			ant_service.recovery_level = 0;
		ant_service.ant_users = false;

		wake_unlock(&ant_service.service_wake_lock);
		mutex_unlock(&ant_start_mutex);
	} else {
		int ret;
recovery:
		/* recovery_release_complete will already have been re-inited if
		 * the ANT service was closed before the release call, so don't complete
		 * it in that case.
		 */
		if (ant_service.service_started)
			complete_all(&ant_service.recovery_release_complete);
		wake_unlock(&ant_service.service_wake_lock);
		mutex_unlock(&ant_start_mutex);

		ret = wait_for_completion_timeout(&ant_service.recovery_probe_complete,
		       msecs_to_jiffies(recovery_timeout));
		if (ret == 0)
			SCSC_TAG_INFO(BT_COMMON, "recovery_probe_complete timeout\n");

		ant_service.ant_users = false;
	}

	return 0;
}
#endif

#ifdef CONFIG_SCSC_ANT
static int scsc_ant_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	SCSC_TAG_INFO(BT_COMMON, "(ant_users=%u)\n", ant_service.ant_users ? 1 : 0);

	/* is BT/ANT recovery in progress? */
	if (scsc_recovery_in_progress()) {
		SCSC_TAG_WARNING(BT_COMMON, "recovery in progress\n");
		wait_event_interruptible_timeout(ant_recovery_complete_queue,
						 !scsc_recovery_in_progress(),
						 SCSC_ANT_MAX_TIMEOUT);

		if (scsc_recovery_in_progress()) {
			SCSC_TAG_WARNING(BT_COMMON, "recovery timeout, aborting\n");
			return -EFAULT;
		}
	}
	if (!ant_service.ant_users) {
		ret = slsi_sm_ant_service_start();
		if (ret == 0)
			ant_service.ant_users = true;
	} else {
		ret = -EBUSY;
	}

	return ret;
}
#endif

static long scsc_default_ioctl(struct file *file,
			       unsigned int cmd,
			       unsigned long arg)
{
	UNUSED(file);
	UNUSED(cmd);
	UNUSED(arg);

	switch (cmd) {
	case TCGETS:
		SCSC_TAG_DEBUG(BT_COMMON, "TCGETS (arg=%lu)\n", arg);
		break;
	case TCSETS:
		SCSC_TAG_DEBUG(BT_COMMON, "TCSETS (arg=%lu)\n", arg);
		break;
	default:
		SCSC_TAG_DEBUG(BT_COMMON,
			"trapped ioctl in virtual tty device, cmd %d arg %lu\n",
			cmd, arg);
		break;
	}

	return 0;
}

static int scsc_bt_trigger_recovery(void *priv,
				    enum slsi_kic_test_recovery_type type)
{
	int err = 0;

	SCSC_TAG_INFO(BT_COMMON, "forcing panic\n");

	mutex_lock(&bt_start_mutex);

	if (0 < atomic_read(&bt_service.service_users) &&
	    bt_service.bsmhcp_protocol) {
		SCSC_TAG_INFO(BT_COMMON, "trashing magic value\n");

		if (slsi_kic_test_recovery_type_service_stop_panic == type)
			bt_service.bsmhcp_protocol->header.firmware_control =
				BSMHCP_CONTROL_STOP_PANIC;
		else if (slsi_kic_test_recovery_type_service_start_panic ==
			 type)
			firmware_control = BSMHCP_CONTROL_START_PANIC;
		else
			bt_service.bsmhcp_protocol->header.magic_value = 0;

		scsc_service_mifintrbit_bit_set(bt_service.service,
			bt_service.bsmhcp_protocol->header.ap_to_bg_int_src,
			SCSC_MIFINTR_TARGET_R4);
	} else {
		if (slsi_kic_test_recovery_type_service_stop_panic == type)
			firmware_control = BSMHCP_CONTROL_STOP_PANIC;
		else if (slsi_kic_test_recovery_type_service_start_panic ==
			 type)
			firmware_control = BSMHCP_CONTROL_START_PANIC;
		else
			err = -EFAULT;
	}

	mutex_unlock(&bt_start_mutex);

	return err;
}

#ifdef CONFIG_SCSC_ANT
static int scsc_ant_trigger_recovery(void *priv,
				    enum slsi_kic_test_recovery_type type)
{
	int err = 0;

	SCSC_TAG_INFO(BT_COMMON, "forcing panic\n");

	mutex_lock(&ant_start_mutex);

	if (atomic_read(&ant_service.service_users) > 0 &&
	    ant_service.asmhcp_protocol) {
		SCSC_TAG_INFO(BT_COMMON, "trashing magic value\n");

		if (slsi_kic_test_recovery_type_service_stop_panic == type)
			ant_service.asmhcp_protocol->header.firmware_control =
				BSMHCP_CONTROL_STOP_PANIC;
		else if (slsi_kic_test_recovery_type_service_start_panic ==
			 type)
			firmware_control = BSMHCP_CONTROL_START_PANIC;
		else
			ant_service.asmhcp_protocol->header.magic_value = 0;

		scsc_service_mifintrbit_bit_set(ant_service.service,
			ant_service.asmhcp_protocol->header.ap_to_bg_int_src,
			SCSC_MIFINTR_TARGET_R4);
	} else {
		if (slsi_kic_test_recovery_type_service_stop_panic == type)
			firmware_control = BSMHCP_CONTROL_STOP_PANIC;
		else if (slsi_kic_test_recovery_type_service_start_panic ==
			 type)
			firmware_control = BSMHCP_CONTROL_START_PANIC;
		else
			err = -EFAULT;
	}

	mutex_unlock(&ant_start_mutex);

	return err;
}
#endif

static const struct file_operations scsc_bt_shm_fops = {
	.owner            = THIS_MODULE,
	.open             = scsc_bt_h4_open,
	.release          = scsc_bt_h4_release,
	.read             = scsc_bt_shm_h4_read,
	.write            = scsc_bt_shm_h4_write,
	.poll             = scsc_bt_shm_h4_poll,
	.unlocked_ioctl   = scsc_default_ioctl,
};

#ifdef CONFIG_SCSC_ANT
static const struct file_operations scsc_ant_shm_fops = {
	.owner            = THIS_MODULE,
	.open             = scsc_ant_open,
	.release          = scsc_ant_release,
	.read             = scsc_shm_ant_read,
	.write            = scsc_shm_ant_write,
	.poll             = scsc_shm_ant_poll,
};
#endif

static struct slsi_kic_bt_ops scsc_bt_kic_ops = {
	.trigger_recovery = scsc_bt_trigger_recovery
};

#ifdef CONFIG_SCSC_ANT
static struct slsi_kic_ant_ops scsc_ant_kic_ops = {
	.trigger_recovery = scsc_ant_trigger_recovery
};
#endif

/* A new MX instance is available */
void slsi_bt_service_probe(struct scsc_mx_module_client *module_client,
			   struct scsc_mx *mx,
			   enum scsc_module_client_reason reason)
{
	/* Note: mx identifies the instance */
	SCSC_TAG_INFO(BT_COMMON,
		      "BT service probe (%s %p)\n", module_client->name, mx);

	mutex_lock(&bt_start_mutex);
	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY &&
	    bt_service.recovery_level == 0) {
		SCSC_TAG_INFO(BT_COMMON,
			      "BT service probe recovery, but no recovery in progress\n");
		goto done;
	}

	bt_service.dev = scsc_mx_get_device(mx);
	common_service.maxwell_core = mx;

	get_device(bt_service.dev);

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY &&
	    bt_service.recovery_level != 0) {
		complete_all(&bt_service.recovery_probe_complete);
		bt_service.recovery_level = 0;
	}

	slsi_bt_notify_probe(bt_service.dev,
			     &scsc_bt_shm_fops,
			     &bt_service.error_count,
			     &bt_service.read_wait);

done:
	mutex_unlock(&bt_start_mutex);
}

/* The MX instance is now unavailable */
static void slsi_bt_service_remove(struct scsc_mx_module_client *module_client,
				   struct scsc_mx *mx,
				   enum scsc_module_client_reason reason)
{
	SCSC_TAG_INFO(BT_COMMON,
		      "BT service remove (%s %p)\n", module_client->name, mx);

	mutex_lock(&bt_start_mutex);
	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY &&
	    bt_service.recovery_level == 0) {
		SCSC_TAG_INFO(BT_COMMON,
			      "BT service remove recovery, but no recovery in progress\n");
		goto done;
	}

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY &&
	    bt_service.recovery_level != 0) {
		bool service_active = bt_service.service_started;

		mutex_unlock(&bt_start_mutex);

		SCSC_TAG_INFO(BT_COMMON, "wait for recovery_release_complete\n");
		/* Don't wait for the recovery_release_complete if service is not active */
		if (service_active) {
			int ret = wait_for_completion_timeout(&bt_service.recovery_release_complete,
			       msecs_to_jiffies(SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT));
			if (ret == 0)
				SCSC_TAG_INFO(BT_COMMON, "recovery_release_complete timeout\n");
		}

		mutex_lock(&bt_start_mutex);

		if (service_active)
			reinit_completion(&bt_service.recovery_release_complete);

		if (slsi_sm_bt_service_stop() == -EIO)
			SCSC_TAG_INFO(BT_COMMON, "Service stop or close failed during recovery.\n");

		/* Clear all control structures */
		bt_service.read_offset = 0;
		bt_service.read_operation = 0;
		bt_service.read_index = 0;
		bt_service.h4_write_offset = 0;
	}

	slsi_bt_notify_remove();
	put_device(bt_service.dev);
	common_service.maxwell_core = NULL;

done:
	mutex_unlock(&bt_start_mutex);

	SCSC_TAG_INFO(BT_COMMON,
	      "BT service remove complete (%s %p)\n", module_client->name, mx);
}

/* BT service driver registration interface */
static struct scsc_mx_module_client bt_driver = {
	.name = "BT driver",
	.probe = slsi_bt_service_probe,
	.remove = slsi_bt_service_remove,
};

#ifdef CONFIG_SCSC_ANT
/* A new MX instance is available */
void slsi_ant_service_probe(struct scsc_mx_module_client *module_client,
			   struct scsc_mx *mx,
			   enum scsc_module_client_reason reason)
{
	/* Note: mx identifies the instance */
	SCSC_TAG_INFO(BT_COMMON,
		      "ANT service probe (%s %p)\n", module_client->name, mx);

	mutex_lock(&ant_start_mutex);
	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && ant_service.recovery_level == 0) {
		SCSC_TAG_INFO(BT_COMMON,
			      "ANT service probe recovery, but no recovery in progress\n");
		goto done;
	}

	ant_service.dev = scsc_mx_get_device(mx);
	common_service.maxwell_core = mx;

	get_device(ant_service.dev);

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && ant_service.recovery_level != 0) {
		complete_all(&ant_service.recovery_probe_complete);
		ant_service.recovery_level = 0;
	}

done:
	mutex_unlock(&ant_start_mutex);
	wake_up_interruptible(&ant_recovery_complete_queue);
}
#endif

#ifdef CONFIG_SCSC_ANT
/* The MX instance is now unavailable */
static void slsi_ant_service_remove(struct scsc_mx_module_client *module_client,
				    struct scsc_mx *mx,
				    enum scsc_module_client_reason reason)
{
	SCSC_TAG_INFO(BT_COMMON,
		      "ANT service remove (%s %p)\n", module_client->name, mx);

	mutex_lock(&ant_start_mutex);
	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && ant_service.recovery_level == 0) {
		SCSC_TAG_INFO(BT_COMMON,
			      "ANT service remove recovery, but no recovery in progress\n");
		goto done;
	}

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && ant_service.recovery_level != 0) {
		int ret;
		bool service_active = ant_service.service_started;

		mutex_unlock(&ant_start_mutex);

		/* Don't wait for recovery_release_complete if service is not active */
		if (service_active) {
			ret = wait_for_completion_timeout(&ant_service.recovery_release_complete,
			       msecs_to_jiffies(SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT));
			if (ret == 0)
				SCSC_TAG_INFO(BT_COMMON, "recovery_release_complete timeout\n");
		}

		mutex_lock(&ant_start_mutex);

		if (service_active)
			reinit_completion(&ant_service.recovery_release_complete);

		if (slsi_sm_ant_service_stop() == -EIO)
			SCSC_TAG_INFO(BT_COMMON, "Service stop or close failed during recovery.\n");

		ant_service.ant_users = false;

		/* Clear all control structures */
		ant_service.read_offset = 0;
		ant_service.read_operation = 0;
		ant_service.read_index = 0;
		ant_service.ant_write_offset = 0;
	}

	put_device(ant_service.dev);
	common_service.maxwell_core = NULL;

done:
	mutex_unlock(&ant_start_mutex);

	SCSC_TAG_INFO(BT_COMMON,
		      "ANT service remove complete (%s %p)\n", module_client->name, mx);
}
#endif

#ifdef CONFIG_SCSC_ANT
/* ANT service driver registration interface */
static struct scsc_mx_module_client ant_driver = {
	.name = "ANT driver",
	.probe = slsi_ant_service_probe,
	.remove = slsi_ant_service_remove,
};
#endif

static void slsi_bt_service_proc_show_firmware(struct seq_file *m)
{
	struct BSMHCP_FW_INFO *info =
		&bt_service.bsmhcp_protocol->information;
	int res;
	u32 index;
	u32 user_defined_count = info->user_defined_count;

	bt_info_trigger++;

	scsc_service_mifintrbit_bit_set(bt_service.service,
		bt_service.bsmhcp_protocol->header.info_ap_to_bg_int_src,
		SCSC_MIFINTR_TARGET_R4);

	res = wait_event_interruptible_timeout(bt_service.info_wait,
			bt_info_trigger == bt_info_interrupt,
			2*HZ);

	seq_printf(m, "  r4_from_ap_interrupt_count    = %u\n",
			info->r4_from_ap_interrupt_count);
	seq_printf(m, "  m4_from_ap_interrupt_count    = %u\n",
			info->m4_from_ap_interrupt_count);
	seq_printf(m, "  r4_to_ap_interrupt_count      = %u\n",
			info->r4_to_ap_interrupt_count);
	seq_printf(m, "  m4_to_ap_interrupt_count      = %u\n\n",
			info->m4_to_ap_interrupt_count);
	seq_printf(m, "  bt_deep_sleep_time_total      = %u\n",
			info->bt_deep_sleep_time_total);
	seq_printf(m, "  bt_deep_sleep_wakeup_duration = %u\n\n",
			info->bt_deep_sleep_wakeup_duration);
	seq_printf(m, "  sched_n_messages              = %u\n\n",
			info->sched_n_messages);
	seq_printf(m, "  user_defined_count            = %u\n\n",
			info->user_defined_count);

	if (user_defined_count > BSMHCP_FW_INFO_USER_DEFINED_COUNT)
		user_defined_count = BSMHCP_FW_INFO_USER_DEFINED_COUNT;

	for (index = 0; index < user_defined_count; index++)
		seq_printf(m, "  user%02u                        = 0x%08x (%u)\n",
			index, info->user_defined[index], info->user_defined[index]);

	if (user_defined_count)
		seq_puts(m, "\n");

	seq_printf(m, "  bt_info_trigger               = %u\n",
			bt_info_trigger);
	seq_printf(m, "  bt_info_interrupt             = %u\n\n",
			bt_info_interrupt);
	seq_printf(m, "  result                        = %d\n", res);
}

static int slsi_bt_service_proc_show(struct seq_file *m, void *v)
{
	char    allocated_text[BSMHCP_DATA_BUFFER_TX_ACL_SIZE + 1];
	char    processed_text[BSMHCP_TRANSFER_RING_EVT_SIZE + 1];
	size_t  index;
	struct scsc_bt_avdtp_detect_hci_connection *cur = bt_service.avdtp_detect.connections;

	seq_puts(m, "Driver statistics:\n");
	seq_printf(m, "  write_wake_lock_count         = %zu\n",
		bt_service.write_wake_lock_count);
	seq_printf(m, "  write_wake_unlock_count       = %zu\n\n",
		bt_service.write_wake_unlock_count);

	seq_printf(m, "  mailbox_hci_evt_read          = %u\n",
		bt_service.mailbox_hci_evt_read);
	seq_printf(m, "  mailbox_hci_evt_write         = %u\n",
		bt_service.mailbox_hci_evt_write);
	seq_printf(m, "  mailbox_acl_rx_read           = %u\n",
		bt_service.mailbox_acl_rx_read);
	seq_printf(m, "  mailbox_acl_rx_write          = %u\n",
		bt_service.mailbox_acl_rx_write);
	seq_printf(m, "  mailbox_acl_free_read         = %u\n",
		bt_service.mailbox_acl_free_read);
	seq_printf(m, "  mailbox_acl_free_read_scan    = %u\n",
		bt_service.mailbox_acl_free_read_scan);
	seq_printf(m, "  mailbox_acl_free_write        = %u\n",
		bt_service.mailbox_acl_free_write);

	seq_printf(m, "  hci_event_paused              = %u\n",
		bt_service.hci_event_paused);
	seq_printf(m, "  acldata_paused                = %u\n\n",
		bt_service.acldata_paused);

	seq_printf(m, "  interrupt_count               = %zu\n",
		bt_service.interrupt_count);
	seq_printf(m, "  interrupt_read_count          = %zu\n",
		bt_service.interrupt_read_count);
	seq_printf(m, "  interrupt_write_count         = %zu\n",
		bt_service.interrupt_write_count);

	for (index = 0; index < BSMHCP_DATA_BUFFER_TX_ACL_SIZE; index++)
		allocated_text[index] = bt_service.allocated[index] ? '1' : '0';
	allocated_text[BSMHCP_DATA_BUFFER_TX_ACL_SIZE] = 0;

	for (index = 0; index < BSMHCP_TRANSFER_RING_EVT_SIZE; index++)
		processed_text[index] = bt_service.processed[index] ? '1' : '0';
	processed_text[BSMHCP_DATA_BUFFER_TX_ACL_SIZE] = 0;

	seq_printf(m, "  allocated_count               = %u\n",
		bt_service.allocated_count);
	seq_printf(m, "  freed_count                   = %u\n",
		bt_service.freed_count);
	seq_printf(m, "  allocated                     = %s\n",
		allocated_text);
	seq_printf(m, "  processed                     = %s\n\n",
		processed_text);

	while (cur) {
		seq_printf(m, "	 avdtp_hci_connection_handle   = %u\n\n",
				   cur->hci_connection_handle);
		seq_printf(m, "	 avdtp_signaling_src_cid	   = %u\n",
				   cur->signal.src_cid);
		seq_printf(m, "	 avdtp_signaling_dst_cid	   = %u\n",
				   cur->signal.dst_cid);
		seq_printf(m, "	 avdtp_streaming_src_cid	   = %u\n",
				   cur->stream.src_cid);
		seq_printf(m, "	 avdtp_streaming_dst_cid	   = %u\n",
				   cur->stream.dst_cid);
		cur = cur->next;
	}
	seq_puts(m, "Firmware statistics:\n");

	mutex_lock(&bt_start_mutex);

	if (NULL != bt_service.service) {
		if (bt_service.bsmhcp_protocol->header.firmware_features &
		    BSMHCP_FEATURE_FW_INFORMATION) {
			slsi_bt_service_proc_show_firmware(m);
		} else
			seq_puts(m,
			    "  Firmware does not provide this information\n");
	} else
		seq_puts(m,
			"  Error: bluetooth service is currently disabled\n");

	mutex_unlock(&bt_start_mutex);

	return 0;
}

static int slsi_bt_service_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, slsi_bt_service_proc_show, NULL);
}

static const struct file_operations scsc_bt_procfs_fops = {
	.owner   = THIS_MODULE,
	.open    = slsi_bt_service_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void scsc_update_btlog_params(void)
{
	mutex_lock(&bt_start_mutex);
	if (bt_service.service) {
		bt_service.bsmhcp_protocol->header.btlog_enables0_low = firmware_btlog_enables0_low;
		bt_service.bsmhcp_protocol->header.btlog_enables0_high = firmware_btlog_enables0_high;
		bt_service.bsmhcp_protocol->header.btlog_enables1_low = firmware_btlog_enables1_low;
		bt_service.bsmhcp_protocol->header.btlog_enables1_high = firmware_btlog_enables1_high;

		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(bt_service.service,
				bt_service.bsmhcp_protocol->header.ap_to_bg_int_src,
				SCSC_MIFINTR_TARGET_R4);
	}
	mutex_unlock(&bt_start_mutex);
}

static int scsc_mxlog_filter_set_param_cb(const char *buffer,
					  const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(buffer, 0, &value);
	if (!ret) {
		firmware_btlog_enables0_low = value;
		scsc_update_btlog_params();
	}

	return ret;
}

/* Validate, by conventional semantics, that the base of the
 * string is 16. I.e if the string begins with 0x the number
 * can be parsed as a hexadecimal (case insensitive)
 *
 * Returns true if the string can be parsed as hexadecimal
 */
static bool scsc_string_is_hexadecimal(const char *s)
{
	if (s[0] == '0' && tolower(s[1]) == 'x' && isxdigit(s[2]))
		return true;

	return false;
}

/* Updates btlog level by converting the string to four u32 integers.
 *
 * Note the string given to kstrtou64 must be null-terminated, and may also
 * include a single newline before its terminating null. The first character
 * may also be a plus sign, but not a minus sign.
 *
 * If the string cannot be parsed as a hexadecimal number it is considered
 * as a parsing error
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 */
static int scsc_btlog_enables_set_param_cb(const char *buffer,
				           const struct kernel_param *kp)
{
	int ret;
	size_t buffer_len;
	u16 newline_len = 0;
	u64 btlog_enables_low = 0;
	u64 btlog_enables_high = 0;

	if (buffer == NULL)
		return -EINVAL;

	buffer_len = strnlen(buffer, SCSC_BTLOG_MAX_STRING_LEN);

	if (buffer_len >= SCSC_BTLOG_MAX_STRING_LEN)
		return -ERANGE;

	if (buffer_len <= SCSC_BTLOG_BUF_PREFIX_LEN)
		return -EINVAL;

	/* If the first character is a plus sign sign, ignore it */
	if (buffer[0] == '+')
	{
		buffer++;
		buffer_len--;

		if (buffer_len <= SCSC_BTLOG_BUF_PREFIX_LEN)
			return -EINVAL;
	}

	/* Only accept the string if it can be parsed as a hexadecimal number */
	if (!scsc_string_is_hexadecimal(buffer))
		return -EINVAL;

	/* Is a newline included before the terminating null */
	if (buffer[buffer_len - 1] == '\n')
		newline_len = 1;

	if (buffer_len < SCSC_BTLOG_BUF_LEN + newline_len)
		ret = kstrtou64(buffer, 0, &btlog_enables_low);
	else {
		/* Need to split the string into two parts.
		 *
		 * First, the least significant integer(u64) is found by
		 * copying the prefix ('0' 'x') plus the last 17
		 * (18 if a newline is included) character including
		 * the null terminator, to a temporary buffer.
		 *
		 * Second, the most significant integer(u64) is found
		 * by copying the remaining part of the string plus a null
		 * terminator, to a temporary buffer.
		 */
		char btlog_enables_buf[SCSC_BTLOG_BUF_LEN + 1];

		u32 start_index = buffer_len - SCSC_BTLOG_BUF_MAX_CHAR_TO_COPY - newline_len;

		memcpy(btlog_enables_buf, buffer, SCSC_BTLOG_BUF_PREFIX_LEN);
		strcpy(&btlog_enables_buf[SCSC_BTLOG_BUF_PREFIX_LEN], &buffer[start_index]);
		ret = kstrtou64(btlog_enables_buf, 0, &btlog_enables_low);

		if (!ret) {
			u32 char_to_copy = start_index;

			memcpy(btlog_enables_buf, buffer, char_to_copy);
			btlog_enables_buf[char_to_copy] = '\0';
			ret = kstrtou64(btlog_enables_buf, 0, &btlog_enables_high);
		}
	}

	if (!ret) {
		firmware_btlog_enables0_low  = (u32)(btlog_enables_low & 0x00000000FFFFFFFF);
		firmware_btlog_enables0_high = (u32)((btlog_enables_low & 0xFFFFFFFF00000000) >> 32);
		firmware_btlog_enables1_low  = (u32)(btlog_enables_high & 0x00000000FFFFFFFF);
		firmware_btlog_enables1_high = (u32)((btlog_enables_high & 0xFFFFFFFF00000000) >> 32);
		scsc_update_btlog_params();
	}

	return ret;
}

static int scsc_mxlog_filter_get_param_cb(char *buffer,
					  const struct kernel_param *kp)
{
	return sprintf(buffer, "mxlog_filter=0x%08x\n", firmware_btlog_enables0_low);
}

static int scsc_btlog_enables_get_param_cb(char *buffer,
					    const struct kernel_param *kp)
{
	return sprintf(buffer, "btlog_enables = 0x%08x%08x%08x%08x\n",
			firmware_btlog_enables1_high ,firmware_btlog_enables1_low,
			firmware_btlog_enables0_high, firmware_btlog_enables0_low);
}

static struct kernel_param_ops scsc_mxlog_filter_ops = {
	.set = scsc_mxlog_filter_set_param_cb,
	.get = scsc_mxlog_filter_get_param_cb,
};

static struct kernel_param_ops scsc_btlog_enables_ops = {
	.set = scsc_btlog_enables_set_param_cb,
	.get = scsc_btlog_enables_get_param_cb,
};

module_param_cb(mxlog_filter, &scsc_mxlog_filter_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlog_filter,
		 	 	 "Set the enables for btlog sources in Bluetooth firmware (31..0)");

module_param_cb(btlog_enables, &scsc_btlog_enables_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(btlog_enables,
				"Set the enables for btlog sources in Bluetooth firmware (127..0)");

static int scsc_force_crash_set_param_cb(const char *buffer,
					 const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(buffer, 0, &value);
	if (!ret && value == 0xDEADDEAD) {
		mutex_lock(&bt_start_mutex);
		if (bt_service.service) {
			atomic_inc(&bt_service.error_count);
			wake_up(&bt_service.read_wait);
		}
		mutex_unlock(&bt_start_mutex);
	}

	return ret;
}

static struct kernel_param_ops scsc_force_crash_ops = {
	.set = scsc_force_crash_set_param_cb,
	.get = NULL,
};

module_param_cb(force_crash, &scsc_force_crash_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_crash,
		 "Forces a crash of the Bluetooth driver");


#ifndef CONFIG_SOC_EXYNOS7885
phys_addr_t scsc_bt_audio_get_paddr_buf(bool tx)
{
	if (bt_audio.abox_physical) {
		struct scsc_bt_audio_abox *abox_physical;
		void *ptr;

		abox_physical = bt_audio.abox_physical;
		if (tx)
			ptr = abox_physical->bt_to_abox_streaming_if_data;
		else
			ptr = abox_physical->abox_to_bt_streaming_if_data;

		return (phys_addr_t)ptr;
	} else
		return 0;
}
EXPORT_SYMBOL(scsc_bt_audio_get_paddr_buf);
#endif

#ifndef CONFIG_SOC_EXYNOS7885
unsigned int scsc_bt_audio_get_rate(int id)
{
	if (!bt_audio.abox_virtual)
		return 0;

	switch (id) {
	case 0:
		return bt_audio.abox_virtual->streaming_if_0_sample_rate;
	case 1:
		return bt_audio.abox_virtual->streaming_if_1_sample_rate;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(scsc_bt_audio_get_rate);
#endif

#ifndef CONFIG_SOC_EXYNOS7885
int scsc_bt_audio_register(struct device *dev,
		int (*dev_iommu_map)(struct device *, phys_addr_t, size_t),
		void (*dev_iommu_unmap)(struct device *, size_t))
#else
int scsc_bt_audio_register(struct scsc_bt_audio_driver *driver)
#endif
{
	int ret = 0;

	mutex_lock(&bt_audio_mutex);

#ifndef CONFIG_SOC_EXYNOS7885
	if (audio_device != NULL || dev == NULL ||
	    dev_iommu_map == NULL || dev_iommu_unmap == NULL) {
		SCSC_TAG_ERR(BT_COMMON,
			"failed audio_device %p dev %p dev_iommu_map %p dev_iommu_unmap %p\n",
				audio_device, dev, dev_iommu_map, dev_iommu_unmap);
		ret = -EINVAL;
	} else {
		audio_device = dev;
		bt_audio.dev_iommu_map = dev_iommu_map;
		bt_audio.dev_iommu_unmap = dev_iommu_unmap;
	}
#else
	if (NULL == driver || NULL == driver->name ||
	    NULL == driver->probe || NULL == driver->remove)
		ret = -EINVAL;
	else {
		audio_driver = driver;

		if (bt_service.h4_users && bt_audio.dev != NULL) {
			audio_driver->probe(audio_driver, &bt_audio);

			audio_driver_probed = true;
		}

	}
#endif

	mutex_unlock(&bt_audio_mutex);

	return ret;
}
EXPORT_SYMBOL(scsc_bt_audio_register);

#ifndef CONFIG_SOC_EXYNOS7885
int scsc_bt_audio_unregister(struct device *dev)
#else
int scsc_bt_audio_unregister(struct scsc_bt_audio_driver *driver)
#endif
{
	int ret = 0;

	mutex_lock(&bt_audio_mutex);

#ifndef CONFIG_SOC_EXYNOS7885
	if (audio_device != NULL && dev == audio_device) {

		/* Unmap ringbuffer IOMMU now that A-Box is finished with it,
		 * but for safety don't allow this if BT is running.
		 *
		 * In practice, A-Box driver only unregisters if platform
		 * driver unloads at shutdown, so it would be safe to leave the
		 * memmory mapped.
		 */
		if (atomic_read(&bt_service.service_users) == 0 && audio_device_probed)
			slsi_bt_audio_remove();

		bt_audio.dev			= NULL;
		bt_audio.abox_virtual		= NULL;
		bt_audio.abox_physical		= NULL;
		bt_audio.dev_iommu_map		= NULL;
		bt_audio.dev_iommu_unmap	= NULL;
		audio_device			= NULL;
		audio_device_probed		= false;
	} else
		ret = -EINVAL;
#else
	if (audio_driver != NULL && driver == audio_driver) {
		if (audio_driver_probed)
			audio_driver->remove(&bt_audio);

		bt_audio.dev           = NULL;
		bt_audio.abox_virtual  = NULL;
		bt_audio.abox_physical = NULL;
		audio_driver           = NULL;
		audio_driver_probed    = false;
	} else
		ret = -EINVAL;
#endif

	mutex_unlock(&bt_audio_mutex);

	return ret;
}
EXPORT_SYMBOL(scsc_bt_audio_unregister);

/******* Module entry/exit point ********/
static int __init scsc_bt_module_init(void)
{
	int ret;
	struct proc_dir_entry *procfs_dir;

	SCSC_TAG_INFO(BT_COMMON, "%s %s (C) %s\n",
		      SCSC_MODDESC, SCSC_MODVERSION, SCSC_MODAUTH);
	bt_module_irq_mask = 0;
	bt_service.recovery_level = 0;
#ifdef CONFIG_SCSC_ANT
	ant_service.recovery_level = 0;
#endif

	memset(&bt_service, 0, sizeof(bt_service));
#ifdef CONFIG_SCSC_ANT
	memset(&ant_service, 0, sizeof(ant_service));
#endif

	init_waitqueue_head(&bt_service.read_wait);
	init_waitqueue_head(&bt_service.info_wait);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	wake_lock_init(&bt_service.read_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "bt_read_wake_lock");
	wake_lock_init(&bt_service.write_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "bt_write_wake_lock");
	wake_lock_init(&bt_service.service_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "bt_service_wake_lock");

#ifdef CONFIG_SCSC_ANT
	init_waitqueue_head(&ant_service.read_wait);

	wake_lock_init(&ant_service.read_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "ant_read_wake_lock");
	wake_lock_init(&ant_service.write_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "ant_write_wake_lock");
	wake_lock_init(&ant_service.service_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "ant_service_wake_lock");
#endif
#else
        wake_lock_init(NULL, &bt_service.read_wake_lock.ws,
                       "bt_read_wake_lock");
        wake_lock_init(NULL, &bt_service.write_wake_lock.ws,
                       "bt_write_wake_lock");
        wake_lock_init(NULL, &bt_service.service_wake_lock.ws,
                       "bt_service_wake_lock");

#ifdef CONFIG_SCSC_ANT
        init_waitqueue_head(&ant_service.read_wait);

        wake_lock_init(NULL, &ant_service.read_wake_lock.ws,
                       "ant_read_wake_lock");
        wake_lock_init(NULL, &ant_service.write_wake_lock.ws,
                       "ant_write_wake_lock");
        wake_lock_init(NULL, &ant_service.service_wake_lock.ws,
                       "ant_service_wake_lock");
#endif
#endif
	procfs_dir = proc_mkdir("driver/scsc_bt", NULL);
	if (NULL != procfs_dir) {
		proc_create_data("stats", S_IRUSR | S_IRGRP,
				 procfs_dir, &scsc_bt_procfs_fops, NULL);
	}

	ret = alloc_chrdev_region(&bt_service.device, 0,
				  SCSC_TTY_MINORS, "scsc_char");
	if (ret) {
		SCSC_TAG_ERR(BT_COMMON, "error alloc_chrdev_region %d\n", ret);
		return ret;
	}

	common_service.class = class_create(THIS_MODULE, "scsc_char");
	if (IS_ERR(common_service.class)) {
		ret = PTR_ERR(common_service.class);
		goto error;
	}

	cdev_init(&bt_service.h4_cdev, &scsc_bt_shm_fops);
	ret = cdev_add(&bt_service.h4_cdev,
		       MKDEV(MAJOR(bt_service.device), MINOR(0)), 1);
	if (ret) {
		SCSC_TAG_ERR(BT_COMMON,
			     "cdev_add failed for device %s\n",
			     SCSC_H4_DEVICE_NAME);
		bt_service.h4_cdev.dev = 0;
		goto error;
	}

	bt_service.h4_device = device_create(common_service.class,
					     NULL,
					     bt_service.h4_cdev.dev,
					     NULL,
					     SCSC_H4_DEVICE_NAME);
	if (bt_service.h4_device == NULL) {
		cdev_del(&bt_service.h4_cdev);
		ret = -EFAULT;
		goto error;
	}

	init_completion(&bt_service.recovery_probe_complete);
	init_completion(&bt_service.recovery_release_complete);

#ifdef CONFIG_SCSC_ANT
	ret = alloc_chrdev_region(&ant_service.device, 0,
				  SCSC_TTY_MINORS, "scsc_ant_char");
	if (ret) {
		SCSC_TAG_ERR(BT_COMMON, "error alloc_chrdev_region %d\n", ret);
		return ret;
	}

	cdev_init(&ant_service.ant_cdev, &scsc_ant_shm_fops);
	ret = cdev_add(&ant_service.ant_cdev,
		       MKDEV(MAJOR(ant_service.device), MINOR(0)), 1);
	if (ret) {
		SCSC_TAG_ERR(BT_COMMON,
			     "cdev_add failed for device %s\n",
			     SCSC_ANT_DEVICE_NAME);
		ant_service.ant_cdev.dev = 0;
		goto error;
	}

	ant_service.ant_device = device_create(common_service.class,
					     NULL,
					     ant_service.ant_cdev.dev,
					     NULL,
					     SCSC_ANT_DEVICE_NAME);
	if (ant_service.ant_device == NULL) {
		cdev_del(&ant_service.ant_cdev);
		ret = -EFAULT;
		goto error;
	}

	init_completion(&ant_service.recovery_probe_complete);
	init_completion(&ant_service.recovery_release_complete);
	init_completion(&ant_service.release_complete);
#endif

	/* Register KIC interface */
	slsi_kic_bt_ops_register(NULL, &scsc_bt_kic_ops);

	/* Register with MX manager */
	scsc_mx_module_register_client_module(&bt_driver);

#ifdef CONFIG_SCSC_ANT
/* Register KIC interface */
	slsi_kic_ant_ops_register(NULL, &scsc_ant_kic_ops);
	SCSC_TAG_DEBUG(BT_COMMON, "Register the KIC interface, %p\n",
			   &scsc_ant_kic_ops);

	/* Register with MX manager */
	scsc_mx_module_register_client_module(&ant_driver);
#endif

	SCSC_TAG_DEBUG(BT_COMMON, "dev=%u class=%p\n",
			   bt_service.device, common_service.class);

	spin_lock_init(&bt_service.avdtp_detect.lock);
	spin_lock_init(&bt_service.avdtp_detect.fw_write_lock);

#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
	sprintf(bluetooth_address_fallback, "%02X:%02X:%02X:%02X:%02X:%02X",
	       (exynos_soc_info.unique_id & 0x000000FF0000) >> 16,
	       (exynos_soc_info.unique_id & 0x00000000FF00) >> 8,
	       (exynos_soc_info.unique_id & 0x0000000000FF) >> 0,
	       (exynos_soc_info.unique_id & 0xFF0000000000) >> 40,
	       (exynos_soc_info.unique_id & 0x00FF00000000) >> 32,
	       (exynos_soc_info.unique_id & 0x0000FF000000) >> 24);
#endif

#ifdef CONFIG_SCSC_ANT
	SCSC_TAG_DEBUG(BT_COMMON, "dev=%u class=%p\n",
			   ant_service.device, common_service.class);
#endif
	scsc_bt_qos_init();

	return 0;

error:
	SCSC_TAG_ERR(BT_COMMON, "error class_create bt device\n");
	unregister_chrdev_region(bt_service.device, SCSC_TTY_MINORS);

#ifdef CONFIG_SCSC_ANT
	SCSC_TAG_ERR(BT_COMMON, "error class_create ant device\n");
	unregister_chrdev_region(ant_service.device, SCSC_TTY_MINORS);
#endif

	return ret;
}


static void __exit scsc_bt_module_exit(void)
{
	SCSC_TAG_INFO(BT_COMMON, "\n");

	wake_lock_destroy(&bt_service.write_wake_lock);
	wake_lock_destroy(&bt_service.read_wake_lock);
	wake_lock_destroy(&bt_service.service_wake_lock);
	complete_all(&bt_service.recovery_probe_complete);
	complete_all(&bt_service.recovery_release_complete);

#ifdef CONFIG_SCSC_ANT
	wake_lock_destroy(&ant_service.write_wake_lock);
	wake_lock_destroy(&ant_service.read_wake_lock);
	wake_lock_destroy(&ant_service.service_wake_lock);
	complete_all(&ant_service.recovery_probe_complete);
	complete_all(&ant_service.recovery_release_complete);
	complete_all(&ant_service.release_complete);
#endif

	slsi_kic_bt_ops_unregister(&scsc_bt_kic_ops);

	/* Register with MX manager */
	scsc_mx_module_unregister_client_module(&bt_driver);

	if (bt_service.h4_device) {
		device_destroy(common_service.class, bt_service.h4_cdev.dev);
		bt_service.h4_device = NULL;
	}

	cdev_del(&bt_service.h4_cdev);

	unregister_chrdev_region(bt_service.device, SCSC_TTY_MINORS);

#ifdef CONFIG_SCSC_ANT
	slsi_kic_ant_ops_unregister(&scsc_ant_kic_ops);

	/* Register with MX manager */
	scsc_mx_module_unregister_client_module(&ant_driver);

	if (ant_service.ant_device) {
		device_destroy(common_service.class, ant_service.ant_cdev.dev);
		ant_service.ant_device = NULL;
	}

	cdev_del(&ant_service.ant_cdev);

	unregister_chrdev_region(ant_service.device, SCSC_TTY_MINORS);
#endif

	scsc_bt_qos_deinit();

	SCSC_TAG_INFO(BT_COMMON, "exit, module unloaded\n");
}

module_init(scsc_bt_module_init);
module_exit(scsc_bt_module_exit);

MODULE_DESCRIPTION(SCSC_MODDESC);
MODULE_AUTHOR(SCSC_MODAUTH);
MODULE_LICENSE("GPL");
MODULE_VERSION(SCSC_MODVERSION);
