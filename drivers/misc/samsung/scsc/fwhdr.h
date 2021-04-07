/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef FWHDR_H
#define FWHDR_H

#define FW_BUILD_ID_SZ	128
#define FW_TTID_SZ	32

struct fwhdr {
	u16 hdr_major;
	u16 hdr_minor;

	u16 fwapi_major;
	u16 fwapi_minor;

	u32 firmware_entry_point;
	u32 fw_runtime_length;

	u32 fw_crc32;
	u32 const_crc32;
	u32 header_crc32;

	u32 const_fw_length;
	u32 hdr_length;
	u32 r4_panic_record_offset;
	u32 m4_panic_record_offset;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 m4_1_panic_record_offset;
#endif
};

bool fwhdr_parse(char *fw, struct fwhdr *fwhdr);
char *fwhdr_get_build_id(char *fw, struct fwhdr *fwhdr);
char *fwhdr_get_ttid(char *fw, struct fwhdr *fwhdr);

#endif /* FWHDR_H */
