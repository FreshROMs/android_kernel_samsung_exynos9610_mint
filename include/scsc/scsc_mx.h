/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _SCSC_CORE_H
#define _SCSC_CORE_H

#include <linux/types.h>
#include <linux/notifier.h>
#include "scsc_mifram.h"

#define SCSC_PANIC_CODE_FW 0
#define SCSC_PANIC_CODE_HOST 1

#define	SCSC_FW_EVENT_FAILURE			0
#define	SCSC_FW_EVENT_MOREDUMP_COMPLETE		1

/** The following flags define the pools that can used for memory allocation.
 * To be used with scsc_mx_service_mifram_alloc_extended **/
/* Standard memory allocation */
#define MIFRAMMAN_MEM_POOL_GENERIC 1
/* Used for buffers containing logs that will not be dumped by moredump */
#define MIFRAMMAN_MEM_POOL_LOGGING 2

#ifdef ANDROID_VERSION
#ifndef SCSC_SEP_VERSION
#define SCSC_SEP_VERSION ANDROID_VERSION
#endif
#endif

struct device;
struct firmware;
struct scsc_mx;

enum scsc_service_id {
	SCSC_SERVICE_ID_NULL = 0,
	SCSC_SERVICE_ID_WLAN = 1,
	SCSC_SERVICE_ID_BT = 2,
	SCSC_SERVICE_ID_ANT = 3,
	SCSC_SERVICE_ID_R4DBG = 4,
	SCSC_SERVICE_ID_ECHO = 5,
	SCSC_SERVICE_ID_DBG_SAMPLER = 6,
	SCSC_SERVICE_ID_CLK20MHZ = 7,
	SCSC_SERVICE_ID_FM = 8,
	SCSC_SERVICE_ID_INVALID = 0xff,
};

#ifdef CONFIG_SCSC_QOS
#define SCSC_SERVICE_TOTAL	9
#endif

enum scsc_module_client_reason {
	SCSC_MODULE_CLIENT_REASON_HW_PROBE = 0,
	SCSC_MODULE_CLIENT_REASON_HW_REMOVE = 1,
	SCSC_MODULE_CLIENT_REASON_RECOVERY = 2,
	SCSC_MODULE_CLIENT_REASON_INVALID = 0xff,
};

#ifdef CONFIG_SCSC_QOS
enum scsc_qos_config {
	SCSC_QOS_DISABLED = 0,
	SCSC_QOS_MIN = 1,
	SCSC_QOS_MED = 2,
	SCSC_QOS_MAX = 3,
};
#endif

/* SYSTEM ERROR SUB-SYSTEMS */
#define SYSERR_SUBSYS_COMMON		(0)
#define SYSERR_SUBSYS_BT		(1)
#define SYSERR_SUBSYS_WLAN		(2)
#define SYSERR_SUBSYS_HOST		(8)


/* SYSTEM ERROR levels */

/* System Error level 1
 * Minor warning from firmware
 * Should not be escalted by driver
 */
#define MX_SYSERR_LEVEL_1		(1)

/* System Error level 2
 * More severe warning from firmware
 * May be escalated by driver
 */
#define MX_SYSERR_LEVEL_2		(2)

/* System Error level 2
 * Minor error handled in firmware
 * may be escalated by driver
 */
#define MX_SYSERR_LEVEL_3		(3)

/* System Error level 3
 * More severe error handled in firmware
 * May be escalated by driver
 */
#define MX_SYSERR_LEVEL_4		(4)

/* System Error level 5
 * Firmware requested service restart
 * May be escalated by driver
 */
#define MX_SYSERR_LEVEL_5		(5)

/* System Error level 5
 * Firmware requested service restart (firmware may have restarted some hardware)
 * May be escalated by driver
 */
#define MX_SYSERR_LEVEL_6		(6)

/* System Error level 7
 * Firmware halt and full restart required
 */
#define MX_SYSERR_LEVEL_7		(7)

/* System Error level 7
 * Firmware halt and full restart (not just drivers) required
 */
#define MX_SYSERR_LEVEL_8		(8)

/* Null error code */
#define MX_NULL_SYSERR			(0xFFFF)

/* Details for decoding */
#define SYSERR_SUB_CODE_POSN			(0)
#define SYSERR_SUB_CODE_MASK			(0xFFFF)
#define SYSERR_SUB_SYSTEM_POSN			(12)
#define SYSERR_SUB_SYSTEM_MASK			(0xF)
#define SYSERR_LEVEL_POSN			(24)
#define SYSERR_LEVEL_MASK			(0xF)
#define SYSERR_TYPE_POSN			(28)
#define SYSERR_TYPE_MASK			(0xFF)
#define SYSERR_SUB_SYSTEM_HOST			(8)


/* Core Driver Module registration */
struct scsc_mx_module_client {
	char *name;
	void (*probe)(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason);
	void (*remove)(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason);
};

/* Service Client interface */

/* Decoded syserr_code */
struct mx_syserr_decode {
	u8      subsys;
	u8      level;
	u16     type;
	u16     subcode;
};

struct scsc_service_client;

struct scsc_service_client {
	/** Called on Maxwell System Error. The Client should use its internal state information
	 * and the information provided by this call to return an appropriate recovery level
	 * which must be greater than or equal to that passed in as parameter within the
	 * mx_syserr_decode structure.
	 */
	u8 (*failure_notification)(struct scsc_service_client *client, struct mx_syserr_decode *err);
	/** Called on Maxwell failure requiring a service restart or full chip restart.
	 * The level within the mx_syserr_decode structure indicates the recover level taking place.
	 * The Client should Stop all SDRAM & MIF Mailbox access as fast as possible
	 * and inform the Manager by calling client_stopped(). The boolean return value
	 * indicates that this failure should trigger an slsi_send_hanged_vendor_event
	 * if WLAN is active (common code will ensure this happens by passing scsc_syserr_code
	 * parameter with a value other than MX_NULL_SYSERR when failure_reset is called subsequently)
	 */
	bool (*stop_on_failure_v2)(struct scsc_service_client *client, struct mx_syserr_decode *err);
	/* Old version to be depricated */
	void (*stop_on_failure)(struct scsc_service_client *client);
	/** Called when Maxwell failure has been handled and the Maxwell has been
	 * reset if the level has demanded it. The Client should assume that any Maxwell
	 * resources it held are invalid. If a scsc_syserr_code other than MX_NULL_SYSERR is provided,
	 * then this may be propogated by the WLAN driver as a slsi_send_hanged_vendor_event
	 * to notify the host of the failure and its cause
	 */
	void (*failure_reset_v2)(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code);
	/* Old version to be depricated */
	void (*failure_reset)(struct scsc_service_client *client, u16 scsc_panic_code);
	/* called when AP processor is going into suspend. */
	int (*suspend)(struct scsc_service_client *client);
	/* called when AP processor has resumed */
	int (*resume)(struct scsc_service_client *client);
	/* called when log collection has been triggered */
	void (*log)(struct scsc_service_client *client, u16 reason);
};

/*
 * This must be used by FM Radio Service only. Other services must not use it.
 * FM Radio client must allocate memory for this structure using scsc_mx_service_mifram_alloc()
 * and pass this structure as a ref parameter to scsc_mx_service_start().
 * The version of fm_ldo_conf (the LDO configuration structure) must be written
 * to the version field by the FM Radio Service and confirmed to match the define by the firmware.
 * Increment the version (FM_LDO_CONFIG_VERSION) when changing the layout of the structure.
 */
#define FM_LDO_CONFIG_VERSION 0

struct fm_ldo_conf {
	uint32_t version;      /* FM_LDO_CONFIG_VERSION */
	uint32_t ldo_on;
};

/* Parameters to pass from FM radio client driver to WLBT drivers */
struct wlbt_fm_params {
	u32 freq;		/* Frequency (Hz) in use by FM radio */
};

/* Shared Data BT-ABOX */
struct scsc_btabox_data {
       unsigned long btaboxmem_start;
       size_t        btaboxmem_size;
};

#define PANIC_RECORD_SIZE			64
#define PANIC_STACK_RECORD_SIZE			256
#define PANIC_RECORD_DUMP_BUFFER_SZ		4096

/* WARNING: THIS IS INTERRUPT CONTEXT!
 * here: some serious warnings about not blocking or doing anything lengthy at all
 */
typedef void (*scsc_mifintrbit_handler)(int which_bit, void *data);

/*
 * Core Module Inteface
 */
int scsc_mx_module_register_client_module(struct scsc_mx_module_client *module_client);
void scsc_mx_module_unregister_client_module(struct scsc_mx_module_client *module_client);
int scsc_mx_module_reset(void);

/*
 *  Core Instance interface
 */
/** 1st thing to do is call open and return service managment interface*/
struct scsc_service *scsc_mx_service_open(struct scsc_mx *mx, enum scsc_service_id id, struct scsc_service_client *client, int *status);

/*
 * Service interface
 */
/** pass a portable dram reference and returns kernel pointer (basically is dealing with the pointers) */
void *scsc_mx_service_mif_addr_to_ptr(struct scsc_service *service, scsc_mifram_ref ref);
void *scsc_mx_service_mif_addr_to_phys(struct scsc_service *service, scsc_mifram_ref ref);
int scsc_mx_service_mif_ptr_to_addr(struct scsc_service *service, void *mem_ptr, scsc_mifram_ref *ref);

int scsc_mx_service_start(struct scsc_service *service, scsc_mifram_ref ref);
int scsc_mx_service_stop(struct scsc_service *service);
int scsc_mx_service_close(struct scsc_service *service);
int scsc_mx_service_mif_dump_registers(struct scsc_service *service);

/** Signal a failure detected by the Client. This will trigger the systemwide
 * MX_SYSERR_LEVEL_7 failure handling procedure: _All_ Clients will be called back via
 * their stop_on_failure() handler as a side-effect. */
void scsc_mx_service_service_failed(struct scsc_service *service, const char *reason);

/* MEMORY Interface*/
/** Allocate a contiguous block of SDRAM accessible to Client Driver. The memory will be allocated
 * from generic pool (MIFRAMMAN_MEM_POOL_GENERIC) */
int scsc_mx_service_mifram_alloc(struct scsc_service *service, size_t nbytes, scsc_mifram_ref *ref, u32 align);
/* Same as scsc_mx_service_mifram_alloc but allows to specify flags (MIFRAMMAN_MEM_POOL_XX).
 * So, for example, to allocate memory from the logging pool use MIFRAMMAN_MEM_POOL_LOGGING. */
int scsc_mx_service_mifram_alloc_extended(struct scsc_service *service, size_t nbytes, scsc_mifram_ref *ref, u32 align, uint32_t flags);
struct scsc_bt_audio_abox *scsc_mx_service_get_bt_audio_abox(struct scsc_service *service);
struct mifabox *scsc_mx_service_get_aboxram(struct scsc_service *service);
/** Free a contiguous block of SDRAM */
void scsc_mx_service_mifram_free(struct scsc_service *service, scsc_mifram_ref ref);
void scsc_mx_service_mifram_free_extended(struct scsc_service *service, scsc_mifram_ref ref, uint32_t flags);

/* MBOX Interface */
/** Allocate n contiguous mailboxes. Outputs index of first mbox, returns FALSE if can’t allocate n contiguous mailboxes. */
bool scsc_mx_service_alloc_mboxes(struct scsc_service *service, int n, int *first_mbox_index);
/** Free n contiguous mailboxes. */
void scsc_service_free_mboxes(struct scsc_service *service, int n, int first_mbox_index);

/** Get kernel-space pointer to a mailbox.
 * The pointer can be cached as it is guaranteed not to change between service start & stop.
 **/
u32 *scsc_mx_service_get_mbox_ptr(struct scsc_service *service, int mbox_index);

/* IRQ Interface */
/* Getters/Setters */

/* From R4/M4 */
int scsc_service_mifintrbit_bit_mask_status_get(struct scsc_service *service);
int scsc_service_mifintrbit_get(struct scsc_service *service);
void scsc_service_mifintrbit_bit_clear(struct scsc_service *service, int which_bit);
void scsc_service_mifintrbit_bit_mask(struct scsc_service *service, int which_bit);
void scsc_service_mifintrbit_bit_unmask(struct scsc_service *service, int which_bit);

/* To R4/M4 */
enum scsc_mifintr_target {
	SCSC_MIFINTR_TARGET_R4 = 0,
	SCSC_MIFINTR_TARGET_M4 = 1
};

void scsc_service_mifintrbit_bit_set(struct scsc_service *service, int which_bit, enum scsc_mifintr_target dir);

/* Register an interrupt handler -TOHOST direction.
 * Function returns the IRQ associated , -EIO if all interrupts have been assigned */
int scsc_service_mifintrbit_register_tohost(struct scsc_service *service, scsc_mifintrbit_handler handler, void *data);
/* Unregister an interrupt handler associated with a bit -TOHOST direction */
int scsc_service_mifintrbit_unregister_tohost(struct scsc_service *service, int which_bit);

/* Get an interrupt bit associated with the target (R4/M4) -FROMHOST direction
 * Function returns the IRQ bit associated , -EIO if error */
int scsc_service_mifintrbit_alloc_fromhost(struct scsc_service *service, enum scsc_mifintr_target dir);
/* Free an interrupt bit associated with the target (R4/M4) -FROMHOST direction
 * Function returns the 0 if succedes , -EIO if error */
int scsc_service_mifintrbit_free_fromhost(struct scsc_service *service, int which_bit, enum scsc_mifintr_target dir);
/*
 * Return a kernel device associated 1:1 with the Maxwell instance.
 * This is published only for the purpose of associating service drivers
 * with a Maxwell instance for logging purposes. Clients should not make
 * any assumptions about the device type. In some configurations this may
 * be the associated host-interface device (AXI/PCIe),
 * but this may change in future.
 */
struct device *scsc_service_get_device(struct scsc_service *service);
struct device *scsc_service_get_device_by_mx(struct scsc_mx *mx);

int scsc_service_force_panic(struct scsc_service *service);

/*
 * API to share /sys/wifi kobject between core and wifi driver modules.
 * Depending upon the order of loading respective drivers, a kobject is
 * created and shared with the other driver. This convoluted implementation
 * is required as we need the common kobject associated with "/sys/wifi" directory
 * when creating a file underneth. core driver (mxman.c) need to create "memdump"
 * and wifi driver (dev.c,mgt.c) needs to create "mac_addr" files respectively.
 */
struct kobject *mxman_wifi_kobject_ref_get(void);
void mxman_wifi_kobject_ref_put(void);

#ifdef CONFIG_SCSC_SMAPPER
/* SMAPPER Interface */
/* Configure smapper. Function should configure smapper FW memory map, range, and granularity */
void scsc_service_mifsmapper_configure(struct scsc_service *service, u32 granularity);
/* Allocate large/small entries bank. Outputs index of bank, returns -EIO if can’t allocate any banks. */
/* Function also returns by the numbers of entries that could be used in the bank as the number of entries
 * is HW dependent (entries/granurality/memory window in FW)
 */
int scsc_service_mifsmapper_alloc_bank(struct scsc_service *service, bool large_bank, u32 entry_size, u16 *entries);
/* Free large/small entries bank */
int scsc_service_mifsmapper_free_bank(struct scsc_service *service, u8 bank);
/* Get number entries, returns error if entries have not been allocated */
int scsc_service_mifsmapper_get_entries(struct scsc_service *service, u8 bank, u8 num_entries, u8 *entries);
/* Free number entries, returns error if entries have not been allocated */
int scsc_service_mifsmapper_free_entries(struct scsc_service *service, u8 bank, u8 num_entries, u8 *entries);
/* Program SRAM entry */
int scsc_service_mifsmapper_write_sram(struct scsc_service *service, u8 bank, u8 num_entries, u8 first_entry, dma_addr_t *addr);
u32 scsc_service_mifsmapper_get_bank_base_address(struct scsc_service *service, u8 bank);
/* Get SMAPPER aligment */
u16 scsc_service_get_alignment(struct scsc_service *service);
#endif

#ifdef CONFIG_SCSC_QOS
int scsc_service_pm_qos_add_request(struct scsc_service *service, enum scsc_qos_config config);
int scsc_service_pm_qos_update_request(struct scsc_service *service, enum scsc_qos_config config);
int scsc_service_pm_qos_remove_request(struct scsc_service *service);
int scsc_service_set_affinity_cpu(struct scsc_service *service, u8 cpu);
#endif

/* Return the panic record */
int scsc_service_get_panic_record(struct scsc_service *service, u8 *dst, u16 max_size);

/* MXLOGGER API */
/* If there is no service/mxman associated, register the observer as global (will affect all the mx instanes)*/
/* Users of these functions should ensure that the registers/unregister functions are balanced (i.e. if observer is registed as global,
 * it _has_ to unregister as global) */
int scsc_service_register_observer(struct scsc_service *service, char *name);
/* Unregister an observer */
int scsc_service_unregister_observer(struct scsc_service *service, char *name);

/* Reads a configuration file into memory.
 *
 * Path is relative to the currently selected firmware configuration
 * subdirectory.
 * Returns pointer to data or NULL if file not found.
 * Call mx140_file_release_conf()to release the memory.
 */
int mx140_file_request_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_path, const char *filename);

/* Reads a debug configuration file into memory.
 *
 * Path is relative to the currently selected firmware configuration
 * subdirectory.
 * Returns pointer to data or NULL if file not found.
 * Call mx140_file_release_conf()to release the memory.
 */
int mx140_file_request_debug_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_path);

/* Read device configuration file into memory.
 *
 * Path is relative to the device configuration directory.
 * Returns pointer to data or NULL if file not found.
 * Call mx140_file_release_conf() to release the memory.
 * This call is only used for configuration files that are
 * device instance specific (e.g. mac addresses)
 */
int mx140_file_request_device_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_path);

/* Release configuration file memory
 *
 * If conf is NULL, has no effect.
 */
void mx140_file_release_conf(struct scsc_mx *mx, const struct firmware *conf);

/* Read device configuration file into memory.
 *
 * Path is absolute.
 * Returns pointer to data or NULL if file not found.
 * Call mx140_release_file() to release the memory.
 */
int mx140_request_file(struct scsc_mx *mx, char *path, const struct firmware **firmp);

/* Release configuration file memory allocated with mx140_request_file()
 *
 * If firmp is NULL, has no effect.
 */
int mx140_release_file(struct scsc_mx *mx, const struct firmware *firmp);

/* 20 MHz clock API.
 * The mx140 device uses a clock that is also required by the USB driver.
 * This API allows the USB/clock driver to inform the mx140 driver that the
 * clock is required and that it must boot and/or keep the clock running.
 */

enum mx140_clk20mhz_status {
	MX140_CLK_SUCCESS = 0,	/* Returned successfully */
	MX140_CLK_STARTED,	/* mx140 has started the clock */
	MX140_CLK_STOPPED,	/* mx140 has stopped the clock */
	MX140_CLK_NOT_STARTED,	/* failed to start the clock */
	MX140_CLK_NOT_STOPPED,	/* failed to stop the clock */
	MX140_CLK_ASYNC_FAIL,	/* mx140 failure, async call */
};

/* Register for 20 MHz clock API callbacks
 *
 * Parameters:
 * client_cb:
 *  If client provides non-NULL client_cb, the request is asynchronous and
 *  the client will be called back when the clock service is started.
 *  If client_cb is NULL, the request is blocking.
 * data:
 *  opaque context for the client, and will be passed back in any callback
 *
 * Note it is possible that the callback may be made in the context of the
 * calling request/release function.
 *
 * Returns 0 on success
 */
int mx140_clk20mhz_register(void (*client_cb)(void *data, enum mx140_clk20mhz_status event), void *data);

/* Unregister for 20 MHz clock API callbacks.
 * After this call is made, the mx140 driver will no longer call back.
 */
void mx140_clk20mhz_unregister(void);

/* Client request that the clock be available.
 *
 * If a callback was installed via mx140_clk20mhz_register(), the mx140 driver
 * will call back when the clock is available. If no callback was installed,
 * the request is blocking and will return when the clock is running.
 *
 * Returns:
 *  mx140_clk20mhz_status if a blocking attempt was made to start the clock,
 *  MX140_CLK_SUCCESS if the request will happen asynchronously, or,
 *  -ve error code on other error.
 *
 */
int mx140_clk20mhz_request(void);

/* Client informs that the clock is no longer needed
 *
 * Returns:
 *  mx140_clk20mhz_status if a blocking attempt was made to stop the clock,
 *  MX140_CLK_SUCCESS if the request will happen asynchronously, or,
 *  -ve error code on other error.
 */
int mx140_clk20mhz_release(void);


/* Client requests that FM LDO be available.
 *
 * Returns:
 *  0 on success or -ve error code on error.
 *
 */
int mx250_fm_request(void);


/* Client informs that the LDO is no longer needed
 *
 * Returns:
 *  0 on success or -ve error code on error.
 */
int mx250_fm_release(void);


/* FM client informs of parameter change.
 *
 * mx250_fm_request() must have been called first.
 *
 * Returns:
 *  None
 */
void mx250_fm_set_params(struct wlbt_fm_params *info);

/*
 * for set test mode.
 *
 */
bool slsi_is_rf_test_mode_enabled(void);

int mx140_log_dump(void);

void mxman_get_fw_version(char *version, size_t ver_sz);
void mxman_get_driver_version(char *version, size_t ver_sz);

int mxman_register_firmware_notifier(struct notifier_block *nb);
int mxman_unregister_firmware_notifier(struct notifier_block *nb);

/* Status of WLBT autorecovery on the platform
 *
 * Returns:
 *  false - enabled, true disabled
 */
bool mxman_recovery_disabled(void);

#endif
