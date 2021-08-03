/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>

#include "vipx-log.h"
#include "vipx-mailbox.h"
#include "vipx-graph.h"
#include "vipx-device.h"

static int __attribute__((unused)) vipx_fault_handler(
		struct iommu_domain *domain, struct device *dev,
		unsigned long fault_addr, int fault_flag, void *token)
{
	struct vipx_device *vdev;
	struct vipx_mailbox_ctrl *mctrl;

	pr_err("< VIPX FAULT HANDLER >\n");
	pr_err("Device virtual(0x%lX) is invalid access\n", fault_addr);

	vdev = dev_get_drvdata(dev);
	mctrl = vdev->system.interface.mctrl;

	vipx_debug_dump_debug_regs();
	vipx_mailbox_dump(mctrl);
	vipx_debug_log_flush(&vdev->debug);

	return -EINVAL;
}

static int __vipx_device_start(struct vipx_device *vdev)
{
	int ret;

	vipx_enter();
	ret = vipx_system_start(&vdev->system);
	if (ret)
		goto p_err_system;

	ret = vipx_debug_start(&vdev->debug);
	if (ret)
		goto p_err_debug;

	vipx_leave();
	return 0;
p_err_debug:
	vipx_system_stop(&vdev->system);
p_err_system:
	return ret;
}

static int __vipx_device_stop(struct vipx_device *vdev)
{
	vipx_enter();
	vipx_debug_stop(&vdev->debug);
	vipx_system_stop(&vdev->system);
	vipx_leave();
	return 0;
}

#if defined(CONFIG_PM_SLEEP)
static int vipx_device_suspend(struct device *dev)
{
	int ret;
	struct vipx_device *vdev;

	vipx_enter();
	vdev = dev_get_drvdata(dev);

	mutex_lock(&vdev->open_lock);
	if (!vdev->open_count) {
		mutex_unlock(&vdev->open_lock);
		return 0;
	}

	mutex_lock(&vdev->start_lock);
	vdev->suspended = true;
	if (vdev->start_count)
		__vipx_device_stop(vdev);
	mutex_unlock(&vdev->start_lock);

	ret = vipx_system_suspend(&vdev->system);
	if (ret)
		goto p_err;

	vipx_leave();
p_err:
	mutex_unlock(&vdev->open_lock);
	return ret;
}

static int vipx_device_resume(struct device *dev)
{
	int ret;
	struct vipx_device *vdev;

	vipx_enter();
	vdev = dev_get_drvdata(dev);

	mutex_lock(&vdev->open_lock);
	if (!vdev->open_count) {
		mutex_unlock(&vdev->open_lock);
		return 0;
	}

	ret = vipx_system_resume(&vdev->system);
	if (ret)
		goto p_err;

	mutex_lock(&vdev->start_lock);
	vdev->suspended = false;
	if (vdev->start_count)
		__vipx_device_start(vdev);
	mutex_unlock(&vdev->start_lock);

	vipx_leave();
p_err:
	mutex_unlock(&vdev->open_lock);
	return ret;
}
#endif

static int vipx_device_runtime_suspend(struct device *dev)
{
	int ret;
	struct vipx_device *vdev;

	vipx_enter();
	vdev = dev_get_drvdata(dev);

	ret = vipx_system_runtime_suspend(&vdev->system);
	if (ret)
		goto p_err;

	vipx_leave();
p_err:
	return ret;
}

static int vipx_device_runtime_resume(struct device *dev)
{
	int ret;
	struct vipx_device *vdev;

	vipx_enter();
	vdev = dev_get_drvdata(dev);

	ret = vipx_system_runtime_resume(&vdev->system);
	if (ret)
		goto p_err;

	vipx_leave();
p_err:
	return ret;
}

static const struct dev_pm_ops vipx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			vipx_device_suspend,
			vipx_device_resume)
	SET_RUNTIME_PM_OPS(
			vipx_device_runtime_suspend,
			vipx_device_runtime_resume,
			NULL)
};

int vipx_device_start(struct vipx_device *vdev)
{
	int ret;

	vipx_enter();
	mutex_lock(&vdev->start_lock);
	if (vdev->suspended) {
		ret = -ENOSTR;
		vipx_err("Failed to start device as it was suspended(%d)\n",
				ret);
		goto p_err;
	}

	if (vdev->start_count) {
		vdev->start_count++;
		mutex_unlock(&vdev->start_lock);
		return 0;
	}

	ret = __vipx_device_start(vdev);
	if (ret)
		goto p_err;

	vdev->start_count = 1;
	vipx_leave();
p_err:
	mutex_unlock(&vdev->start_lock);
	return ret;
}

int vipx_device_stop(struct vipx_device *vdev)
{
	vipx_enter();
	mutex_lock(&vdev->start_lock);
	if (!(--vdev->start_count) && !vdev->suspended)
		__vipx_device_stop(vdev);
	mutex_unlock(&vdev->start_lock);
	vipx_leave();
	return 0;
}

static int __vipx_device_power_on(struct vipx_device *vdev)
{
	int ret;

	vipx_enter();
#if defined(CONFIG_PM)
	ret = pm_runtime_get_sync(vdev->dev);
	if (ret) {
		vipx_err("Failed to get pm_runtime sync (%d)\n", ret);
		goto p_err;
	}
#else
	ret = vipx_device_runtime_resume(vdev->dev);
	if (ret)
		goto p_err;
#endif

	vipx_leave();
p_err:
	return ret;
}

static int __vipx_device_power_off(struct vipx_device *vdev)
{
	int ret;

	vipx_enter();
#if defined(CONFIG_PM)
	ret = pm_runtime_put_sync(vdev->dev);
	if (ret) {
		vipx_err("Failed to put pm_runtime sync (%d)\n", ret);
		goto p_err;
	}
#else
	ret = vipx_device_runtime_suspend(vdev->dev);
	if (ret)
		goto p_err;
#endif

	vipx_leave();
p_err:
	return ret;
}

int vipx_device_open(struct vipx_device *vdev)
{
	int ret;

	vipx_enter();
	mutex_lock(&vdev->open_lock);
	if (vdev->open_count) {
		vdev->open_count++;
		mutex_unlock(&vdev->open_lock);
		return 0;
	}

	ret = vipx_system_open(&vdev->system);
	if (ret)
		goto p_err_system;

	ret = vipx_debug_open(&vdev->debug);
	if (ret)
		goto p_err_debug;

	ret = __vipx_device_power_on(vdev);
	if (ret)
		goto p_err_power;

	ret = vipx_system_fw_bootup(&vdev->system);
	if (ret)
		goto p_err_boot;

	vdev->open_count = 1;
	mutex_lock(&vdev->start_lock);
	vdev->start_count = 0;
	vdev->suspended = false;
	mutex_unlock(&vdev->start_lock);
	mutex_unlock(&vdev->open_lock);

	vipx_leave();
	return 0;
p_err_boot:
	__vipx_device_power_off(vdev);
p_err_power:
	vipx_debug_close(&vdev->debug);
p_err_debug:
	vipx_system_close(&vdev->system);
p_err_system:
	mutex_unlock(&vdev->open_lock);
	return ret;
}

int vipx_device_close(struct vipx_device *vdev)
{
	vipx_enter();
	mutex_lock(&vdev->open_lock);
	if (!vdev->open_count) {
		vipx_warn("device is already closed\n");
		goto p_end;
	}

	if (!(--vdev->open_count)) {
		mutex_lock(&vdev->start_lock);
		if (vdev->start_count) {
			__vipx_device_stop(vdev);
			vdev->start_count = 0;
		}
		mutex_unlock(&vdev->start_lock);

		__vipx_device_power_off(vdev);
		vipx_debug_close(&vdev->debug);
		vipx_system_close(&vdev->system);
	}

	vipx_leave();
p_end:
	mutex_unlock(&vdev->open_lock);
	return 0;
}

static int vipx_device_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev;
	struct vipx_device *vdev;

	vipx_enter();
	dev = &pdev->dev;

	vdev = devm_kzalloc(dev, sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		ret = -ENOMEM;
		vipx_err("Fail to alloc device structure\n");
		goto p_err_alloc;
	}

	get_device(dev);
	vdev->dev = dev;
	dev_set_drvdata(dev, vdev);

	mutex_init(&vdev->open_lock);
	vdev->open_count = 0;
	mutex_init(&vdev->start_lock);
	vdev->start_count = 0;

	ret = vipx_system_probe(vdev);
	if (ret)
		goto p_err_system;

	ret = vipx_core_probe(vdev);
	if (ret)
		goto p_err_core;

	ret = vipx_debug_probe(vdev);
	if (ret)
		goto p_err_debug;

	iovmm_set_fault_handler(dev, vipx_fault_handler, NULL);

	vipx_leave();
	vipx_info("vipx device is initilized\n");
	return 0;
p_err_debug:
	vipx_core_remove(&vdev->core);
p_err_core:
	vipx_system_remove(&vdev->system);
p_err_system:
	devm_kfree(dev, vdev);
p_err_alloc:
	vipx_err("vipx device is not registered (%d)\n", ret);
	return ret;
}

static int vipx_device_remove(struct platform_device *pdev)
{
	struct vipx_device *vdev;

	vipx_enter();
	vdev = dev_get_drvdata(&pdev->dev);

	vipx_debug_remove(&vdev->debug);
	vipx_core_remove(&vdev->core);
	vipx_system_remove(&vdev->system);
	devm_kfree(vdev->dev, vdev);
	vipx_leave();
	return 0;
}

static void vipx_device_shutdown(struct platform_device *pdev)
{
	struct vipx_device *vdev;

	vipx_enter();
	vdev = dev_get_drvdata(&pdev->dev);
	vipx_leave();
}

#if defined(CONFIG_OF)
static const struct of_device_id exynos_vipx_match[] = {
	{
		.compatible = "samsung,exynos-vipx",
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_vipx_match);
#endif

static struct platform_driver vipx_driver = {
	.probe		= vipx_device_probe,
	.remove		= vipx_device_remove,
	.shutdown	= vipx_device_shutdown,
	.driver = {
		.name	= "exynos-vipx",
		.owner	= THIS_MODULE,
		.pm	= &vipx_pm_ops,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(exynos_vipx_match)
#endif
	}
};

static int __init vipx_device_init(void)
{
	int ret;

	vipx_enter();
	ret = platform_driver_register(&vipx_driver);
	if (ret)
		vipx_err("platform driver for vipx is not registered(%d)\n",
				ret);

	vipx_leave();
	return ret;
}

static void __exit vipx_device_exit(void)
{
	vipx_enter();
	platform_driver_unregister(&vipx_driver);
	vipx_leave();
}

#if defined(MODULE)
module_init(vipx_device_init);
#else
late_initcall(vipx_device_init);
#endif
module_exit(vipx_device_exit);

MODULE_AUTHOR("@samsung.com>");
MODULE_DESCRIPTION("Exynos VIPx driver");
MODULE_LICENSE("GPL");
