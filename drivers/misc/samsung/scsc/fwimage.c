/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crc32.h>
#include <scsc/scsc_logring.h>
#include "fwimage.h"

int  fwimage_check_fw_header_crc(char *fw, u32 hdr_length, u32 header_crc32)
{
	u32 header_crc32_calculated;

	/*
	 * The last 4-bytes are header CRC
	 */
	header_crc32_calculated = ether_crc(hdr_length - sizeof(u32), fw);
	if (header_crc32_calculated != header_crc32) {
		SCSC_TAG_ERR(FW_LOAD, "CRC32 doesn't match: header_crc32_calculated=%d header_crc32=%d\n",
			     header_crc32_calculated, header_crc32);
		return -EINVAL;
	}
	SCSC_TAG_DEBUG(FW_LOAD, "CRC32 OK: header_crc32_calculated=%d header_crc32=%d\n",
		       header_crc32_calculated, header_crc32);
	return 0;
}

int fwimage_check_fw_const_section_crc(char *fw, u32 const_crc32, u32 const_fw_length, u32 hdr_length)
{
	u32 const_crc32_calculated;

	const_crc32_calculated = ether_crc(const_fw_length - hdr_length, fw + hdr_length);
	if (const_crc32_calculated != const_crc32) {
		SCSC_TAG_ERR(FW_LOAD, "CRC32 doesn't match: const_crc32_calculated=%d const_crc32=%d\n",
			     const_crc32_calculated, const_crc32);
		return -EINVAL;
	}
	SCSC_TAG_DEBUG(FW_LOAD, "CRC32 OK: const_crc32_calculated=%d const_crc32=%d\n",
		       const_crc32_calculated, const_crc32);
	return 0;
}

int fwimage_check_fw_crc(char *fw, u32 fw_image_length, u32 hdr_length, u32 fw_crc32)
{
	u32 fw_crc32_calculated;

	fw_crc32_calculated = ether_crc(fw_image_length - hdr_length, fw + hdr_length);
	if (fw_crc32_calculated != fw_crc32) {
		SCSC_TAG_ERR(FW_LOAD, "CRC32 doesn't match: fw_crc32_calculated=%d fw_crc32=%d\n",
			     fw_crc32_calculated, fw_crc32);
		return -EINVAL;
	}
	SCSC_TAG_DEBUG(FW_LOAD, "CRC32 OK: fw_crc32_calculated=%d fw_crc32=%d\n",
		       fw_crc32_calculated, fw_crc32);
	return 0;
}
