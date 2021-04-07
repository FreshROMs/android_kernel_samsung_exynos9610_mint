/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_runtime.h>

#include "vipx-log.h"
#include "vipx-system.h"
#include "vipx-pm.h"

#if defined(CONFIG_PM_DEVFREQ)

#if defined(CONFIG_EXYNOS_VIPX_EXYNOS9610_A)
static unsigned int cam_qos_table[] = {
	700000,
	690000,
	680000,
	670000,
	660000,
	650000,
	640000
};
#endif

#if defined(CONFIG_EXYNOS_VIPX_EXYNOS9610_R)
static unsigned int cam_qos_table[] = {
	690000,
	680000,
	670000,
	660000,
	650000,
	640000
};
#endif

static int __vipx_pm_qos_check_valid(struct vipx_pm *pm, int qos)
{
	vipx_check();
	if ((qos >= pm->qos_count) || (qos < 0)) {
		vipx_err("pm_qos level(%d) is invalid (L0 ~ L%d)\n",
				qos, pm->qos_count - 1);
		return -EINVAL;
	} else {
		return 0;
	}
}

int vipx_pm_qos_active(struct vipx_pm *pm)
{
	vipx_check();
	return pm_qos_request_active(&pm->cam_qos_req);
}

static void __vipx_pm_qos_set_default(struct vipx_pm *pm, int default_qos)
{
	vipx_enter();
	pm->default_qos = default_qos;
	vipx_info("default pm_qos level is set as L%d\n", pm->default_qos);
	vipx_leave();
}

int vipx_pm_qos_set_default(struct vipx_pm *pm, int default_qos)
{
	int ret;

	vipx_enter();
	ret = __vipx_pm_qos_check_valid(pm, default_qos);
	if (ret)
		goto p_err;

	mutex_lock(&pm->lock);
	__vipx_pm_qos_set_default(pm, default_qos);
	mutex_unlock(&pm->lock);
	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_pm_qos_update(struct vipx_pm *pm, int request_qos)
{
	int ret;

	vipx_enter();
	ret = __vipx_pm_qos_check_valid(pm, request_qos);
	if (ret)
		goto p_err;

	mutex_lock(&pm->lock);
	if (vipx_pm_qos_active(pm) && (pm->current_qos != request_qos)) {
		pm_qos_update_request(&pm->cam_qos_req,
				pm->qos_table[request_qos]);
		vipx_info("pm_qos level is changed from L%d to L%d\n",
				pm->current_qos, request_qos);
		pm->current_qos = request_qos;
	}
	mutex_unlock(&pm->lock);
	vipx_leave();
	return 0;
p_err:
	return ret;
}

void vipx_pm_qos_suspend(struct vipx_pm *pm)
{
	vipx_enter();
	pm->resume_qos = pm->current_qos;
	vipx_pm_qos_update(pm, pm->qos_count - 1);
	vipx_leave();
}

void vipx_pm_qos_resume(struct vipx_pm *pm)
{
	vipx_enter();
	vipx_pm_qos_update(pm, pm->resume_qos);
	pm->resume_qos = -1;
	vipx_leave();
}

static int __vipx_pm_qos_add(struct vipx_pm *pm)
{
	vipx_enter();
	mutex_lock(&pm->lock);
	if (pm->default_qos < 0)
		__vipx_pm_qos_set_default(pm, 0);

	if (!vipx_pm_qos_active(pm)) {
		pm_qos_add_request(&pm->cam_qos_req, PM_QOS_CAM_THROUGHPUT,
				pm->qos_table[pm->default_qos]);
		pm->current_qos = pm->default_qos;
		vipx_info("The power of device is on(L%d)\n",
				pm->current_qos);
	}
	mutex_unlock(&pm->lock);
	vipx_leave();
	return 0;
}

static void __vipx_pm_qos_remove(struct vipx_pm *pm)
{
	vipx_enter();
	mutex_lock(&pm->lock);
	if (vipx_pm_qos_active(pm)) {
		pm_qos_remove_request(&pm->cam_qos_req);
		pm->current_qos = -1;
		vipx_info("The power of device is off\n");
	}
	mutex_unlock(&pm->lock);
	vipx_leave();
}

static int __vipx_pm_qos_init(struct vipx_pm *pm)
{
	vipx_enter();
	pm->qos_count = sizeof(cam_qos_table) / sizeof(unsigned int);
	pm->qos_table = cam_qos_table;
	pm->default_qos = -1;
	pm->resume_qos = -1;
	pm->current_qos = -1;

	vipx_leave();
	return 0;
}
#else
int vipx_pm_qos_active(struct vipx_pm *pm)
{
	return 1;
}

int vipx_pm_qos_set_default(struct vipx_pm *pm, int default_qos)
{
	return 0;
}

int vipx_pm_qos_update(struct vipx_pm *pm, int request_qos)
{
	return 0;
}

void vipx_pm_qos_suspend(struct vipx_pm *pm)
{
}

void vipx_pm_qos_resume(struct vipx_pm *pm)
{
}

static int __vipx_pm_qos_add(struct vipx_pm *pm)
{
	return 0;
}

static void __vipx_pm_qos_remove(struct vipx_pm *pm)
{
}

static int __vipx_pm_qos_init(struct vipx_pm *pm)
{
	return 0;
}
#endif

int vipx_pm_open(struct vipx_pm *pm)
{
	int ret;

	vipx_enter();
	ret = __vipx_pm_qos_add(pm);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

void vipx_pm_close(struct vipx_pm *pm)
{
	vipx_enter();
	__vipx_pm_qos_remove(pm);
	vipx_leave();
}

int vipx_pm_probe(struct vipx_system *sys)
{
	struct vipx_pm *pm;

	vipx_enter();
	pm = &sys->pm;
	pm->system = sys;

	__vipx_pm_qos_init(pm);
#if defined(CONFIG_PM)
	pm_runtime_enable(sys->dev);
#endif
	mutex_init(&pm->lock);

	vipx_leave();
	return 0;
}

void vipx_pm_remove(struct vipx_pm *pm)
{
	vipx_enter();
	mutex_destroy(&pm->lock);

#if defined(CONFIG_PM)
	pm_runtime_disable(pm->system->dev);
#endif
	vipx_leave();
}
