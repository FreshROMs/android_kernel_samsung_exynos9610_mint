/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <net/genetlink.h>
#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>
#include <scsc/scsc_log_collector.h>

/* module parameter value to indicate control of recovery via .memdump.info file */
#define MEMDUMP_FILE_FOR_RECOVERY 2
/* content of .memdump.info file indicating to panic kernel */
#define MEMDUMP_FILE_KERNEL_PANIC 3

/**
 * Attributes are fields of data your messages will contain.
 * The designers of Netlink really want you to use these instead of just dumping
 * data to the packet payload.
 */
enum attributes {
	 /* The first one has to be a throwaway empty attribute */
	ATTR_UNSPEC,

	ATTR_STR,
	ATTR_INT,
	ATTR_PATH,
	ATTR_CONTENT,
	ATTR_INT8,

	/* This must be last! */
	__ATTR_MAX,
};

/**
 * Message type codes.
 */
enum events {
	/* must be first */
	EVENT_UNSPEC,

	EVENT_SCSC,
	EVENT_SYSTEM_PROPERTY,
	EVENT_WRITE_FILE,
	EVENT_SABLE,
#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 12
	EVENT_CHIPSET_LOGGING,
#endif
	/* This must be last! */
	__EVENT_MAX,
};

enum scsc_wlbtd_response_codes {
	/* NOTE: keep the enum in sync with userspace wlbtd */
	/* parse failed */
	SCSC_WLBTD_ERR_PARSE_FAILED,

	/* fw_panic trigger */
	SCSC_WLBTD_FW_PANIC_TAR_GENERATED,
	SCSC_WLBTD_FW_PANIC_ERR_SCRIPT_FILE_NOT_FOUND,
	SCSC_WLBTD_FW_PANIC_ERR_NO_DEV,
	SCSC_WLBTD_FW_PANIC_ERR_MMAP,
	SCSC_WLBTD_FW_PANIC_ERR_SABLE_FILE,
	SCSC_WLBTD_FW_PANIC_ERR_TAR,

	/* other triggers */
	SCSC_WLBTD_OTHER_SBL_GENERATED,
	SCSC_WLBTD_OTHER_TAR_GENERATED,
	SCSC_WLBTD_OTHER_ERR_SCRIPT_FILE_NOT_FOUND,
	SCSC_WLBTD_OTHER_ERR_NO_DEV,
	SCSC_WLBTD_OTHER_ERR_MMAP,
	SCSC_WLBTD_OTHER_ERR_SABLE_FILE,
	SCSC_WLBTD_OTHER_ERR_TAR,
	SCSC_WLBTD_OTHER_IGNORE_TRIGGER,

	/* Keep this as last entry */
	SCSC_WLBTD_LAST_RESPONSE_CODE,
};

static const struct genl_multicast_group scsc_mcgrp[] = {
	{ .name = "scsc_mdp_grp", },
};

int scsc_wlbtd_init(void);
int scsc_wlbtd_deinit(void);
int call_wlbtd(const char *script_path);
int wlbtd_write_file(const char *path, const char *content);
#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 12
int wlbtd_chipset_logging(const char *content, size_t bytes, bool over_mmap);
#endif
int call_wlbtd_sable(u8 trigger_code, u16 reason_code);
void scsc_wlbtd_wait_for_sable_logging(void);
int scsc_wlbtd_get_and_print_build_type(void);
