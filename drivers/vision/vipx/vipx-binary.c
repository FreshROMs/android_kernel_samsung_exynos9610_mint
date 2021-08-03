/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/firmware.h>

#include "vipx-log.h"
#include "vipx-system.h"
#include "vipx-binary.h"

int vipx_binary_firmware_load(struct vipx_binary *bin,
		const char *name, void *target, size_t size)
{
	int ret;
	const struct firmware *fw_blob;

	vipx_enter();
	if (!target) {
		ret = -EINVAL;
		vipx_err("binary(%s) memory is NULL\n", name);
		goto p_err_target;
	}

	ret = request_firmware(&fw_blob, name, bin->dev);
	if (ret) {
		vipx_err("request_firmware(%s) is fail (%d)\n", name, ret);
		goto p_err_req;
	}

	if (fw_blob->size > size) {
		ret = -EIO;
		vipx_err("binary(%s) size is over (%ld > %ld)\n",
				name, fw_blob->size, size);
		goto p_err_size;
	}

	memcpy(target, fw_blob->data, fw_blob->size);
	release_firmware(fw_blob);

	vipx_leave();
	return 0;
p_err_size:
	release_firmware(fw_blob);
p_err_req:
p_err_target:
	return ret;
}

int vipx_binary_init(struct vipx_system *sys)
{
	struct vipx_binary *bin;

	vipx_enter();
	bin = &sys->binary;
	bin->dev = sys->dev;
	vipx_leave();
	return 0;
}

void vipx_binary_deinit(struct vipx_binary *bin)
{
	vipx_enter();
	vipx_leave();
}
