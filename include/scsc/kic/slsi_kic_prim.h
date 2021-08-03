/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_KIC_PRIM_H
#define __SLSI_KIC_PRIM_H

#ifdef __KERNEL__
#include <net/netlink.h>
#else
#include <netlink/attr.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define SLSI_KIC_INTERFACE_VERSION_MAJOR 1
#define SLSI_KIC_INTERFACE_VERSION_MINOR 0

/**
 * Common
 */
enum slsi_kic_technology_type {
	slsi_kic_technology_type_curator,
	slsi_kic_technology_type_bt,
	slsi_kic_technology_type_wifi,
	slsi_kic_technology_type_audio,
	slsi_kic_technology_type_gnss,
	slsi_kic_technology_type_nfc,
	slsi_kic_technology_type_janitor,
	slsi_kic_technology_type_common,
	slsi_kic_technology_type_ant,

	/* keep last */
	slsi_kic_technology_type__after_last,
	slsi_kic_technology_type_max_category = slsi_kic_technology_type__after_last - 1
};

static const char *const slsi_kic_technology_type_text[slsi_kic_technology_type_max_category + 1] = {
	"curator",
	"bt",
	"wifi",
	"audio",
	"gnss",
	"nfc",
	"janitor",
	"ant"
};

/**
 * System events
 */
enum slsi_kic_system_event_category {
	slsi_kic_system_event_category_initialisation,
	slsi_kic_system_event_category_deinitialisation,
	slsi_kic_system_event_category_error,
	slsi_kic_system_event_category_recovery,

	/* keep last */
	slsi_kic_system_category__after_last,
	slsi_kic_system_category_max_category = slsi_kic_system_category__after_last - 1
};

static const char *const slsi_kic_system_category_text[slsi_kic_system_category_max_category + 1] = {
	"Initialisation",
	"Deinitialisation",
	"Error",
	"Recovery"
};

enum slsi_kic_system_events {
	slsi_kic_system_events_wifi_on,
	slsi_kic_system_events_wifi_off,
	slsi_kic_system_events_wifi_suspend,
	slsi_kic_system_events_wifi_resume,
	slsi_kic_system_events_wifi_service_driver_attached,
	slsi_kic_system_events_wifi_service_driver_detached,
	slsi_kic_system_events_wifi_firmware_patch_downloaded,
	slsi_kic_system_events_wifi_service_driver_started,
	slsi_kic_system_events_wifi_service_driver_stopped,
	slsi_kic_system_events_bt_on,
	slsi_kic_system_events_bt_off,
	slsi_kic_system_events_bt_service_driver_attached,
	slsi_kic_system_events_bt_service_driver_detached,
	slsi_kic_system_events_bt_firmware_patch_downloaded,
	slsi_kic_system_events_curator_firmware_patch_downloaded,
	slsi_kic_system_events_sdio_powered,
	slsi_kic_system_events_sdio_inserted,
	slsi_kic_system_events_sdio_removed,
	slsi_kic_system_events_sdio_powered_off,
	slsi_kic_system_events_sdio_error,
	slsi_kic_system_events_uart_powered,
	slsi_kic_system_events_uart_powered_off,
	slsi_kic_system_events_uart_error,
	slsi_kic_system_events_coredump_in_progress,
	slsi_kic_system_events_coredump_done,
	slsi_kic_system_events_subsystem_crashed,
	slsi_kic_system_events_subsystem_recovered,
	slsi_kic_system_events_host_ready_ind,
	slsi_kic_system_events_ant_on,
	slsi_kic_system_events_ant_off,

	/* keep last */
	slsi_kic_system_events__after_last,
	slsi_kic_system_events_max_event = slsi_kic_system_events__after_last - 1
};

static const char *const slsi_kic_system_event_text[slsi_kic_system_events_max_event + 1] = {
	"Wi-Fi on",
	"Wi-Fi off",
	"Wi-Fi suspend",
	"Wi-Fi resume",
	"Wi-Fi service driver attached",
	"Wi-Fi service driver detached",
	"Wi-Fi firmware patch downloaded",
	"Wi-Fi service driver started",
	"Wi-Fi service driver stopped",
	"BT on",
	"BT off",
	"BT service driver attached",
	"BT service driver detached",
	"BT firmware patch downloaded",
	"Curator firmware patch downloaded",
	"SDIO powered",
	"SDIO inserted",
	"SDIO removed",
	"SDIO powered off",
	"SDIO error",
	"UART powered",
	"UART powered off",
	"UART error",
	"Coredump in progress",
	"Coredump done",
	"Subsystem has crashed",
	"Subsystem has been recovered",
	"CCP Host ready Ind sent",
	"ANT on",
	"ANT off"
};

/**
 * Time stamp
 */
struct slsi_kic_timestamp {
	uint64_t tv_sec;  /* seconds */
	uint64_t tv_nsec; /* nanoseconds */
};

/* Policy */
enum slsi_kic_attr_timestamp_attributes {
	__SLSI_KIC_ATTR_TIMESTAMP_INVALID,
	SLSI_KIC_ATTR_TIMESTAMP_TV_SEC,
	SLSI_KIC_ATTR_TIMESTAMP_TV_NSEC,

	/* keep last */
	__SLSI_KIC_ATTR_TIMESTAMP_AFTER_LAST,
	SLSI_KIC_ATTR_TIMESTAMP_MAX = __SLSI_KIC_ATTR_TIMESTAMP_AFTER_LAST - 1
};


/**
 * Firmware event
 */
enum slsi_kic_firmware_event_type {
	slsi_kic_firmware_event_type_panic,
	slsi_kic_firmware_event_type_fault,

	/* keep last */
	slsi_kic_firmware_events__after_last,
	slsi_kic_firmware_events_max_event = slsi_kic_firmware_events__after_last - 1
};

static const char *const slsi_kic_firmware_event_type_text[slsi_kic_firmware_events_max_event + 1] = {
	"Firmware panic",
	"Firmware fault"
};

enum slsi_kic_firmware_container_type {
	slsi_kic_firmware_container_type_ccp_host,
};


/*
 * Firmware event: CCP host container
 */
#define SLSI_KIC_FIRMWARE_EVENT_CCP_HOST_ARG_LENGTH 16
struct slsi_kic_firmware_event_ccp_host {
	uint32_t id;
	uint32_t level;
	char     *level_string;
	uint32_t timestamp;
	uint32_t cpu;
	uint32_t occurences;
	uint32_t arg_length;
	uint8_t  *arg;
};

/**
 * Trigger recovery
 */
enum slsi_kic_test_recovery_type {
	slsi_kic_test_recovery_type_subsystem_panic,
	slsi_kic_test_recovery_type_emulate_firmware_no_response,
	slsi_kic_test_recovery_type_watch_dog,
	slsi_kic_test_recovery_type_chip_crash,
	slsi_kic_test_recovery_type_service_start_panic,
	slsi_kic_test_recovery_type_service_stop_panic,
};

enum slsi_kic_test_recovery_status {
	slsi_kic_test_recovery_status_ok,
	slsi_kic_test_recovery_status_error_invald_param,
	slsi_kic_test_recovery_status_error_send_msg,
};


/* Policy */
enum slsi_kic_attr_firmware_event_ccp_host_attributes {
	__SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_INVALID,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_ID,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_LEVEL,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_LEVEL_STRING,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_TIMESTAMP,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_CPU,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_OCCURENCES,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_ARG,

	/* keep last */
	__SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_AFTER_LAST,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_MAX = __SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_AFTER_LAST - 1
};


/**
 * System information
 */
struct slsi_kic_service_info {
	char     ver_str[128];
	uint16_t fw_api_major;
	uint16_t fw_api_minor;
	uint16_t release_product;
	uint16_t host_release_iteration;
	uint16_t host_release_candidate;
};

/* Policy */
enum slsi_kic_attr_service_info_attributes {
	__SLSI_KIC_ATTR_SERVICE_INFO_INVALID,
	SLSI_KIC_ATTR_SERVICE_INFO_VER_STR,
	SLSI_KIC_ATTR_SERVICE_INFO_FW_API_MAJOR,
	SLSI_KIC_ATTR_SERVICE_INFO_FW_API_MINOR,
	SLSI_KIC_ATTR_SERVICE_INFO_RELEASE_PRODUCT,
	SLSI_KIC_ATTR_SERVICE_INFO_HOST_RELEASE_ITERATION,
	SLSI_KIC_ATTR_SERVICE_INFO_HOST_RELEASE_CANDIDATE,

	/* keep last */
	__SLSI_KIC_ATTR_SERVICE_INFO_AFTER_LAST,
	SLSI_KIC_ATTR_SERVICE_INFO_MAX = __SLSI_KIC_ATTR_SERVICE_INFO_AFTER_LAST - 1
};



/**
 * enum slsi_kic_commands - supported Samsung KIC commands
 *
 * @SLSI_KIC_CMD_UNSPEC: unspecified command to catch errors
 * @SLSI_KIC_CMD_KIC_INTERFACE_VERSION_NUMBER_REQ: Requests the KIC interface
 *    version numbers to be send back.
 * @SLSI_KIC_CMD_SYSTEM_EVENT_IND: Indicate a system event to user space.
 * @SLSI_KIC_CMD_SERVICE_INFORMATION_REQ: Requests information for all
 *    already enabled subsystems.
 * @SLSI_KIC_CMD_SERVICE_INFORMATION_IND: Indicate that a new subsystem has
 *    been started and the information for this subsystem.
 * @SLSI_KIC_CMD_TEST_TRIGGER_RECOVERY_REQ: Triggers a recovery (crash) for a
 *    subsystem specified in the primitive.
 * @SLSI_KIC_CMD_FIRMWARE_EVENT_IND: Indicates a firmware event to user space.
 * @SLSI_KIC_CMD_ECHO_REQ: Request an echo (test primitive, which will be
 *  removed later).
 *
 * @SLSI_KIC_CMD_MAX: highest used command number
 * @__SLSI_KIC_CMD_AFTER_LAST: internal use
 */
enum slsi_kic_commands {
/* Do not change the order or add anything between, this is ABI! */
	SLSI_KIC_CMD_UNSPEC,

	SLSI_KIC_CMD_KIC_INTERFACE_VERSION_NUMBER_REQ,
	SLSI_KIC_CMD_SYSTEM_EVENT_IND,
	SLSI_KIC_CMD_SERVICE_INFORMATION_REQ,
	SLSI_KIC_CMD_SERVICE_INFORMATION_IND,
	SLSI_KIC_CMD_TEST_TRIGGER_RECOVERY_REQ,
	SLSI_KIC_CMD_FIRMWARE_EVENT_IND,
	SLSI_KIC_CMD_ECHO_REQ,

	/* add new commands above here */

	/* used to define SLSI_KIC_CMD_MAX below */
	__SLSI_KIC_CMD_AFTER_LAST,
	SLSI_KIC_CMD_MAX = __SLSI_KIC_CMD_AFTER_LAST - 1
};


/**
 * enum slsi_kic_attrs - Samsung KIC netlink attributes
 *
 * @SLSI_KIC_ATTR_UNSPEC: unspecified attribute to catch errors
 * @SLSI_KIC_ATTR_KIC_VERSION_MAJOR: KIC version number - increments when the
 *    interface is updated with backward incompatible changes.
 * @SLSI_KIC_ATTR_KIC_VERSION_MINOR: KIC version number - increments when the
 *    interface is updated with backward compatible changes.
 * @SLSI_KIC_ATTR_TECHNOLOGY_TYPE: Technology type
 * @SLSI_KIC_ATTR_SYSTEM_EVENT_CATEGORY: System event category
 * @SLSI_KIC_ATTR_SYSTEM_EVENT: System event
 * @SLSI_KIC_ATTR_SERVICE_INFO: Pass service info to user space.
 * @SLSI_KIC_ATTR_NUMBER_OF_ENCODED_SERVICES: The attribute is used to determine
 *    the number of encoded services in the payload
 * @SLSI_KIC_ATTR_TEST_RECOVERY_TYPE: Specifies the recovery type.
 * @SLSI_KIC_ATTR_TIMESTAMP: A timestamp - ideally nano second resolution and
 *    precision, but it's platform dependent.
 * @SLSI_KIC_ATTR_FIRMWARE_EVENT_TYPE: A firmware even type - panic or fault
 * @SLSI_KIC_ATTR_FIRMWARE_CONTAINER_TYPE: Indicates container type carried in
 *    payload.
 * @SLSI_KIC_ATTR_FIRMWARE_EVENT_CONTAINER_CCP_HOST: The firmware event data.
 * @SLSI_KIC_ATTR_TRIGGER_RECOVERY_STATUS: Indicates if the recovery has been
 *    successfully triggered. The recovery signalling will happen afterwards as
 *    normal system events.
 * @SLSI_KIC_ATTR_ECHO: An echo test primitive, which will be removed later.
 *
 * @SLSI_KIC_ATTR_MAX: highest attribute number currently defined
 * @__SLSI_KIC_ATTR_AFTER_LAST: internal use
 */
enum slsi_kic_attrs {
/* Do not change the order or add anything between, this is ABI! */
	SLSI_KIC_ATTR_UNSPEC,

	SLSI_KIC_ATTR_KIC_VERSION_MAJOR,
	SLSI_KIC_ATTR_KIC_VERSION_MINOR,
	SLSI_KIC_ATTR_TECHNOLOGY_TYPE,
	SLSI_KIC_ATTR_SYSTEM_EVENT_CATEGORY,
	SLSI_KIC_ATTR_SYSTEM_EVENT,
	SLSI_KIC_ATTR_SERVICE_INFO,
	SLSI_KIC_ATTR_NUMBER_OF_ENCODED_SERVICES,
	SLSI_KIC_ATTR_TEST_RECOVERY_TYPE,
	SLSI_KIC_ATTR_TIMESTAMP,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_TYPE,
	SLSI_KIC_ATTR_FIRMWARE_CONTAINER_TYPE,
	SLSI_KIC_ATTR_FIRMWARE_EVENT_CONTAINER_CCP_HOST,
	SLSI_KIC_ATTR_TRIGGER_RECOVERY_STATUS,
	SLSI_KIC_ATTR_ECHO,

	/* Add attributes here, update the policy below */
	__SLSI_KIC_ATTR_AFTER_LAST,
	SLSI_KIC_ATTR_MAX = __SLSI_KIC_ATTR_AFTER_LAST - 1
};


/* Policy for the attributes */
static const struct nla_policy slsi_kic_attr_policy[SLSI_KIC_ATTR_MAX + 1] = {
	[SLSI_KIC_ATTR_KIC_VERSION_MAJOR] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_KIC_VERSION_MINOR] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_TECHNOLOGY_TYPE] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_SYSTEM_EVENT_CATEGORY] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_SYSTEM_EVENT] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_SERVICE_INFO] = { .type = NLA_NESTED, },
	[SLSI_KIC_ATTR_NUMBER_OF_ENCODED_SERVICES] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_TEST_RECOVERY_TYPE] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_TIMESTAMP] = { .type = NLA_NESTED },
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_TYPE] = { .type = NLA_U16 },
	[SLSI_KIC_ATTR_FIRMWARE_CONTAINER_TYPE] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CONTAINER_CCP_HOST] = { .type = NLA_NESTED },
	[SLSI_KIC_ATTR_TRIGGER_RECOVERY_STATUS] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_ECHO] = { .type = NLA_U32 },
};

/* Policy for the slsi_kic_firmware_event_ccp_host attribute */
static const struct nla_policy slsi_kic_attr_firmware_event_ccp_host_policy[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_MAX + 1] = {
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_ID] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_LEVEL] = { .type = NLA_U32 },
#ifdef __KERNEL__
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_LEVEL_STRING] = { .type = NLA_STRING, .len = 128 },
#else
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_LEVEL_STRING] = { .type = NLA_STRING, .maxlen = 128 },
#endif
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_TIMESTAMP] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_CPU] = { .type = NLA_U32 },
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_OCCURENCES] = { .type = NLA_U32 },
#ifdef __KERNEL__
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_ARG] = { .type = NLA_UNSPEC,
							.len  = SLSI_KIC_FIRMWARE_EVENT_CCP_HOST_ARG_LENGTH },
#else
	[SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_ARG] = { .type = NLA_UNSPEC,
							.maxlen = SLSI_KIC_FIRMWARE_EVENT_CCP_HOST_ARG_LENGTH },
#endif
};


/* Policy for the slsi_kic_service_info attribute */
static const struct nla_policy slsi_kic_attr_service_info_policy[SLSI_KIC_ATTR_SERVICE_INFO_MAX + 1] = {
#ifdef __KERNEL__
	[SLSI_KIC_ATTR_SERVICE_INFO_VER_STR] = { .type = NLA_STRING, .len = 128 },
#else
	[SLSI_KIC_ATTR_SERVICE_INFO_VER_STR] = { .type = NLA_STRING, .maxlen = 128 },
#endif
	[SLSI_KIC_ATTR_SERVICE_INFO_FW_API_MAJOR] = { .type = NLA_U16 },
	[SLSI_KIC_ATTR_SERVICE_INFO_FW_API_MINOR] = { .type = NLA_U16 },
	[SLSI_KIC_ATTR_SERVICE_INFO_RELEASE_PRODUCT] = { .type = NLA_U16 },
	[SLSI_KIC_ATTR_SERVICE_INFO_HOST_RELEASE_ITERATION] = { .type = NLA_U16 },
	[SLSI_KIC_ATTR_SERVICE_INFO_HOST_RELEASE_CANDIDATE] = { .type = NLA_U16 },
};

/* Policy for the slsi_kic_timestamp attribute */
static const struct nla_policy slsi_kic_attr_timestamp_policy[SLSI_KIC_ATTR_TIMESTAMP_MAX + 1] = {
	[SLSI_KIC_ATTR_TIMESTAMP_TV_SEC] = { .type = NLA_U64 },
	[SLSI_KIC_ATTR_TIMESTAMP_TV_NSEC] = { .type = NLA_U64 },
};


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* #ifndef __SLSI_KIC_PRIM_H */
