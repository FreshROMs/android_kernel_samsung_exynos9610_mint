/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/exynos_iovmm.h>

#include "vipx-log.h"
#include "vipx-device.h"
#include "vipx-system.h"

int vipx_system_fw_bootup(struct vipx_system *sys)
{
	int ret;
	struct vipx_memory *mem;
	struct vipx_binary *bin;

	struct {
		unsigned int mbox_addr;
		unsigned int mbox_size;
		unsigned int heap_addr;
		unsigned int heap_size;
		unsigned int log_addr;
		unsigned int log_size;
	} *shared_mem;

	vipx_enter();
	mem = &sys->memory;
	bin = &sys->binary;

	sys->ctrl_ops->reset(sys);

	ret = vipx_binary_firmware_load(bin, VIPX_FW_DRAM_NAME, mem->fw.kvaddr,
			mem->fw.size);
	if (ret)
		goto p_err;

	ret = vipx_binary_firmware_load(bin, VIPX_FW_DTCM_NAME, sys->dtcm,
			sys->dtcm_size);
	if (ret)
		goto p_err;

	shared_mem = sys->dtcm + 0x3FD0;
	shared_mem->mbox_addr = mem->mbox.dvaddr;
	shared_mem->mbox_size = mem->mbox.size;
	shared_mem->heap_addr = mem->heap.dvaddr;
	shared_mem->heap_size = mem->heap.size;
	shared_mem->log_addr = mem->log.dvaddr;
	shared_mem->log_size = mem->log.size;

	ret = vipx_binary_firmware_load(bin, VIPX_FW_ITCM_NAME, sys->itcm,
			sys->itcm_size);
	if (ret)
		goto p_err;

	sys->ctrl_ops->start(sys);

	ret = vipx_hw_wait_bootup(&sys->interface);
	if (ret)
		goto p_err;

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_system_start(struct vipx_system *sys)
{
	int ret;

	vipx_enter();
	ret = vipx_interface_start(&sys->interface);
	if (ret)
		goto p_err;

	vipx_leave();
p_err:
	return ret;
}

int vipx_system_stop(struct vipx_system *sys)
{
	int ret;

	vipx_enter();
	ret = vipx_interface_stop(&sys->interface);
	if (ret)
		goto p_err;

	vipx_leave();
p_err:
	return ret;
}

int vipx_system_resume(struct vipx_system *sys)
{
	int ret;
	struct vipx_pm *pm;

	vipx_enter();
	pm = &sys->pm;

	if (vipx_pm_qos_active(pm)) {
		vipx_pm_qos_resume(pm);
		ret = vipx_system_fw_bootup(sys);
		if (ret)
			goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

int vipx_system_suspend(struct vipx_system *sys)
{
	struct vipx_pm *pm;

	vipx_enter();
	pm = &sys->pm;

	if (vipx_pm_qos_active(pm)) {
		sys->ctrl_ops->reset(sys);
		vipx_pm_qos_suspend(&sys->pm);
	}

	vipx_leave();
	return 0;
}

int vipx_system_runtime_resume(struct vipx_system *sys)
{
	int ret;

	vipx_enter();
	vipx_pm_open(&sys->pm);

	ret = sys->clk_ops->on(sys);
	if (ret)
		goto p_err_clk_on;

	sys->clk_ops->dump(sys);

	ret = iovmm_activate(sys->dev);
	if (ret) {
		vipx_err("Failed to activate iommu (%d)\n", ret);
		goto p_err_iovmm;
	}

	vipx_leave();
	return 0;
p_err_iovmm:
	sys->clk_ops->off(sys);
p_err_clk_on:
	vipx_pm_close(&sys->pm);
	return ret;
}

int vipx_system_runtime_suspend(struct vipx_system *sys)
{
	vipx_enter();
	sys->ctrl_ops->reset(sys);
	iovmm_deactivate(sys->dev);
	sys->clk_ops->off(sys);
	vipx_pm_close(&sys->pm);

	vipx_leave();
	return 0;
}

int vipx_system_open(struct vipx_system *sys)
{
	int ret;

	vipx_enter();
	ret = vipx_memory_open(&sys->memory);
	if (ret)
		goto p_err_memory;

	ret = vipx_interface_open(&sys->interface,
			sys->memory.mbox.kvaddr);
	if (ret)
		goto p_err_interface;

	ret = vipx_graphmgr_open(&sys->graphmgr);
	if (ret)
		goto p_err_graphmgr;

	vipx_leave();
	return 0;
p_err_graphmgr:
	vipx_interface_close(&sys->interface);
p_err_interface:
	vipx_memory_close(&sys->memory);
p_err_memory:
	return ret;
}

int vipx_system_close(struct vipx_system *sys)
{
	vipx_enter();
	vipx_graphmgr_close(&sys->graphmgr);
	vipx_interface_close(&sys->interface);
	vipx_memory_close(&sys->memory);
	vipx_leave();
	return 0;
}

int vipx_system_probe(struct vipx_device *device)
{
	int ret;
	struct vipx_system *sys;
	struct platform_device *pdev;
	struct device *dev;
	struct resource *res;
	void __iomem *iomem;

	vipx_enter();
	sys = &device->system;
	sys->device = device;

	pdev = to_platform_device(device->dev);
	dev = device->dev;
	sys->dev = dev;

	/* VIPX_CPU_SS1 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		vipx_err("platform_get_resource(0) is fail");
		goto p_err_res_ss1;
	}

	iomem = devm_ioremap_resource(dev, res);
	if (IS_ERR(iomem)) {
		ret = PTR_ERR(iomem);
		vipx_err("devm_ioremap_resource(0) is fail (%d)", ret);
		goto p_err_remap_ss1;
	}

	sys->reg_ss[REG_SS1] = iomem;
	sys->reg_ss_size[REG_SS1] = resource_size(res);

	/* VIPX_CPU_SS2 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		ret = -EINVAL;
		vipx_err("platform_get_resource(1) is fail");
		goto p_err_res_ss2;
	}

	iomem = devm_ioremap_resource(dev, res);
	if (IS_ERR(iomem)) {
		ret = PTR_ERR(iomem);
		vipx_err("devm_ioremap_resource(1) is fail (%d)", ret);
		goto p_err_remap_ss2;
	}

	sys->reg_ss[REG_SS2] = iomem;
	sys->reg_ss_size[REG_SS2] = resource_size(res);

	/* VIPX ITCM */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		ret = -EINVAL;
		vipx_err("platform_get_resource(2) is fail");
		goto p_err_res_itcm;
	}

	iomem = devm_ioremap_resource(dev, res);
	if (IS_ERR(iomem)) {
		ret = PTR_ERR(iomem);
		vipx_err("devm_ioremap_resource(2) is fail (%d)", ret);
		goto p_err_remap_itcm;
	}

	sys->itcm = iomem;
	sys->itcm_size = resource_size(res);

	/* VIPX DTCM */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		ret = -EINVAL;
		vipx_err("platform_get_resource(3) is fail");
		goto p_err_res_dtcm;
	}

	iomem = devm_ioremap_resource(dev, res);
	if (IS_ERR(iomem)) {
		ret = PTR_ERR(iomem);
		vipx_err("devm_ioremap_resource(3) is fail (%d)", ret);
		goto p_err_remap_dtcm;
	}

	sys->dtcm = iomem;
	sys->dtcm_size = resource_size(res);

	sys->clk_ops = &vipx_clk_ops;
	sys->ctrl_ops = &vipx_ctrl_ops;
	sys->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(sys->pinctrl)) {
		ret = PTR_ERR(sys->pinctrl);
		vipx_err("Failed to get devm pinctrl (%d)\n", ret);
		goto p_err_pinctrl;
	}

	ret = vipx_pm_probe(sys);
	if (ret)
		goto p_err_pm;

	ret = sys->clk_ops->init(sys);
	if (ret)
		goto p_err_clk;

	ret = vipx_memory_probe(sys);
	if (ret)
		goto p_err_memory;

	ret = vipx_interface_probe(sys);
	if (ret)
		goto p_err_interface;

	ret = vipx_binary_init(sys);
	if (ret)
		goto p_err_binary;

	ret = vipx_graphmgr_probe(sys);
	if (ret)
		goto p_err_graphmgr;

	vipx_leave();
	return 0;
p_err_graphmgr:
	vipx_binary_deinit(&sys->binary);
p_err_binary:
	vipx_interface_remove(&sys->interface);
p_err_interface:
	vipx_memory_remove(&sys->memory);
p_err_memory:
	sys->clk_ops->deinit(sys);
p_err_clk:
	vipx_pm_remove(&sys->pm);
p_err_pm:
	devm_pinctrl_put(sys->pinctrl);
p_err_pinctrl:
	devm_iounmap(dev, sys->dtcm);
p_err_remap_dtcm:
p_err_res_dtcm:
	devm_iounmap(dev, sys->itcm);
p_err_remap_itcm:
p_err_res_itcm:
	devm_iounmap(dev, sys->reg_ss[REG_SS2]);
p_err_remap_ss2:
p_err_res_ss2:
	devm_iounmap(dev, sys->reg_ss[REG_SS1]);
p_err_remap_ss1:
p_err_res_ss1:
	return ret;
}

void vipx_system_remove(struct vipx_system *sys)
{
	vipx_enter();
	vipx_graphmgr_remove(&sys->graphmgr);
	vipx_binary_deinit(&sys->binary);
	vipx_interface_remove(&sys->interface);
	vipx_memory_remove(&sys->memory);
	sys->clk_ops->deinit(sys);
	vipx_pm_remove(&sys->pm);
	devm_pinctrl_put(sys->pinctrl);
	devm_iounmap(sys->dev, sys->dtcm);
	devm_iounmap(sys->dev, sys->itcm);
	devm_iounmap(sys->dev, sys->reg_ss[REG_SS2]);
	devm_iounmap(sys->dev, sys->reg_ss[REG_SS1]);
	vipx_leave();
}
