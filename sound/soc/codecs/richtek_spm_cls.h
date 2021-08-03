/*
 * Richtek SPM Class Header
 *
 * Copyright (C) 2019, Richtek Technology Corp.
 * Author: CY Hunag <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_RICHTEK_SPM_CLASS_H__
#define __LINUX_RICHTEK_SPM_CLASS_H__

#include <linux/completion.h>
#include <linux/mutex.h>

#define RICHTEK_SPM_MAX_PARAM_ITEMS	(30)
#define RICHTEK_SPM_MAGIC	(5526789)
#define RICHTEK_SPM_RSPK_PATH	"/efs/richtek/rt_amp"

struct richtek_spm_classdev;

struct richtek_spm_device_ops {
	int (*suspend)(struct richtek_spm_classdev *ptc);
	int (*resume)(struct richtek_spm_classdev *ptc);
};

struct richtek_spm_ipc_msg {
	u32 cmd;
	u32 spkid;
	u32 index;
	s32 param_items;
	s32 params[RICHTEK_SPM_MAX_PARAM_ITEMS];
};

struct richtek_spm_classdev {
	const char *name;
	struct device *dev;
	const struct richtek_spm_device_ops *ops;
	const struct attribute_group **groups;
	struct task_struct *trig_task;
	struct completion trig_complete;
	struct mutex var_lock;
	struct richtek_spm_ipc_msg ipc_msg;
	/* internal use for algorithm */
	u32 spkidx;
	u32 t0;
	u32 rspk;
	u32 monitor_on;
	s32 t;
	s32 tmax;
	s32 tmaxcnt;
	s32 xpeak;
	s32 xmax;
	s32 xmaxcnt;
	s32 boot_on_xmax;
	s32 boot_on_tmax;
	s32 calib_enable;
	s32 calib_status;
	s32 vali_enable;
	s32 vali_status;
	s32 vali_real_power;
};

/* API List */
extern int devm_richtek_spm_classdev_register(struct device *parent,
					      struct richtek_spm_classdev *rdc);
extern int richtek_spm_classdev_register(struct device *parent,
					 struct richtek_spm_classdev *rdc);
extern void richtek_spm_classdev_unregister(struct richtek_spm_classdev *rdc);

extern int richtek_spm_classdev_trigger_ampon(struct richtek_spm_classdev *rdc);
extern int richtek_spm_classdev_trigger_ampoff(struct richtek_spm_classdev *rdc);

#endif /* __LINUX_RICHTEK_SPM_CLASS_H__ */
