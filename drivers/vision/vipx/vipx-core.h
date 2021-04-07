/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_CORE_H__
#define __VIPX_CORE_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>

#include "vipx-system.h"

/* Information about VIPx device file */
#define VIPX_DEV_NAME_LEN		(16)
#define VIPX_DEV_NAME			"vipx"

struct vipx_device;
struct vipx_core;

struct vipx_miscdev {
	int                             minor;
	char                            name[VIPX_DEV_NAME_LEN];
	struct miscdevice               miscdev;
};

struct vipx_core {
	struct vipx_miscdev		misc_vdev;
	struct mutex			lock;
	const struct vipx_ioctl_ops	*ioc_ops;
	DECLARE_BITMAP(vctx_map, VIPX_MAX_CONTEXT);
	struct list_head		vctx_list;
	unsigned int			vctx_count;

	struct vipx_device		*device;
	struct vipx_system		*system;
};

int vipx_core_probe(struct vipx_device *device);
void vipx_core_remove(struct vipx_core *core);

#endif
