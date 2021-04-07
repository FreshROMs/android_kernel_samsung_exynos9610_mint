/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_BINARY_H__
#define __VIPX_BINARY_H__

#include <linux/device.h>

#define VIPX_DEBUG_BIN_PATH	"/data"

#define VIPX_FW_DRAM_NAME	"CC_DRAM_CODE_FLASH.bin"
#define VIPX_FW_ITCM_NAME	"CC_ITCM_CODE_FLASH.bin"
#define VIPX_FW_DTCM_NAME	"CC_DTCM_CODE_FLASH.bin"

#define VIPX_FW_NAME_LEN	(100)
#define VIPX_VERSION_SIZE	(42)

struct vipx_system;

struct vipx_binary {
	struct device		*dev;
};

int vipx_binary_firmware_load(struct vipx_binary *bin, const char *name,
		void *target, size_t size);

int vipx_binary_init(struct vipx_system *sys);
void vipx_binary_deinit(struct vipx_binary *bin);

#endif
