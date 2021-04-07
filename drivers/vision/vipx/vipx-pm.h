/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIPX_PM_H__
#define __VIPX_PM_H__

#include <linux/pm_qos.h>

struct vipx_system;

struct vipx_pm {
	struct vipx_system	*system;
	struct mutex		lock;
#if defined(CONFIG_PM_DEVFREQ)
	struct pm_qos_request	cam_qos_req;
	int			qos_count;
	unsigned int		*qos_table;
	int			default_qos;
	int			resume_qos;
	int			current_qos;
#endif
};

int vipx_pm_qos_active(struct vipx_pm *pm);
int vipx_pm_qos_set_default(struct vipx_pm *pm, int default_qos);
int vipx_pm_qos_update(struct vipx_pm *pm, int request_qos);
void vipx_pm_qos_suspend(struct vipx_pm *pm);
void vipx_pm_qos_resume(struct vipx_pm *pm);

int vipx_pm_open(struct vipx_pm *pm);
void vipx_pm_close(struct vipx_pm *pm);
int vipx_pm_probe(struct vipx_system *sys);
void vipx_pm_remove(struct vipx_pm *pm);

#endif
