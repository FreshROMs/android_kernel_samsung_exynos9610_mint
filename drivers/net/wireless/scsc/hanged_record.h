/****************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __HANGED_RECORD_H__
#define __HANGED_RECORD_H__

#define HANGED_FW_VERSION_SIZE		(70)
#define HANGED_HOST_VERSION_SIZE	(64)
#define HANGED_VERSION_SIZE		(4)
#define HANGED_FW_PANIC_SIZE		(4)
#define HANGED_OFFSET_SIZE		(4)
#define HANGED_RESERVED			(3)
#define HANGED_PANIC_RECORD_COUNT       (145)
#define HANGED_PANIC_RECORD_SIZE        ((HANGED_PANIC_RECORD_COUNT) * sizeof(scsc_fw_record_t))

/* Hexadecimal value in string format 0x011c */
#define HANGED_OFFSET_DATA		"011C"

#define HANGED_PANIC_VERSION		"0000"

typedef unsigned int scsc_fw_record_t;

/* HANGED RECORD v 0 */
struct scsc_hanged_record {
	char	hang_type[5];
	char	ver[8];
	char	cook[16];
	char	hg01[8];
	char	hg02[8];
	char	hg03[8];
	char	hg04[8];
	char	hg05[8];
	char	hg06[8];
	char	version[HANGED_VERSION_SIZE];
	char	fw_version[HANGED_FW_VERSION_SIZE];
	char	host_version[HANGED_HOST_VERSION_SIZE];
	/* unsigned int fw_panic; */
	char	fw_panic[HANGED_FW_PANIC_SIZE];
	/* unsigned int offset_data; */
	char	offset_data[HANGED_OFFSET_SIZE];
	char	reserved[HANGED_RESERVED];
	char	panic_record[HANGED_PANIC_RECORD_SIZE];
} __packed;

/* we should be using a typedef for record_type
* but it hasn't been definet yet, so use the current type: u32 */

#define HANGED_PANIC_RECORD_HEX_SZ	((sizeof(scsc_fw_record_t) * 2))
#define HANGED_VERSION_FORMATTING	"%04X"
#define HANGED_FW_PANIC_FORMATTING	"%04X"
#define HANGED_PANIC_REC_FORMATTING	"%08X"
#endif
