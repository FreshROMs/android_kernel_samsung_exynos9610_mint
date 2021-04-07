/*
 * Samsung EXYNOS SoC series MIPI CSIS/DSIM DPHY driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * Author: Sewoon Park <seuni.park@samsung.com>
 * Author: Wooki Min <wooki.min@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <soc/samsung/exynos-pmu.h>

#define EXYNOS_MIPI_PHY_ISO_BYPASS  (1 << 0)
#define EXYNOS_MIPI_PHYS_NUM 4

#define MIPI_PHY_MxSx_UNIQUE 	(0 << 1)
#define MIPI_PHY_MxSx_SHARED 	(1 << 1)
#define MIPI_PHY_MxSx_INIT_DONE (2 << 1)
#define MIPI_PHY_MxSxSx_SHARED 	(3 << 1)

void __iomem *global_addr[2];

enum exynos_mipi_phy_type {
	EXYNOS_MIPI_PHY_M4S4_TOP,
	EXYNOS_MIPI_PHY_M4S4_MOD,
	EXYNOS_MIPI_PHY_M1S2S2,
};

enum exynos_mipi_phy_owner {
	EXYNOS_MIPI_PHY_OWNER_DSIM = 0,
	EXYNOS_MIPI_PHY_OWNER_CSIS = 1,
};

struct mipi_phy_data {
	enum exynos_mipi_phy_type type;
	u8 flags;
	int phy_count;
};

struct exynos_mipi_phy {
	struct device *dev;
	spinlock_t slock;
	void __iomem *regs;
	struct regmap *reg_pmu;
	u32 owner;
	struct mipi_phy_desc {
		struct phy *phy;
		struct mipi_phy_data *data;
		int owner;
		unsigned int index;
		unsigned int iso_offset;
		unsigned int rst_bit;
		unsigned int init_bit;
	} phys[EXYNOS_MIPI_PHYS_NUM];
};

/* 1: Isolation bypass, 0: Isolation enable */
static int __set_phy_isolation(struct regmap *reg_pmu,
		unsigned int offset, unsigned int on)
{
	unsigned int val;
	int ret;

	val = on ? EXYNOS_MIPI_PHY_ISO_BYPASS : 0;

	ret = exynos_pmu_update(offset, EXYNOS_MIPI_PHY_ISO_BYPASS, val);

	pr_debug("%s off=0x%x, val=0x%x\n", __func__, offset, val);
	return ret;
}

static int __set_phy_init_ctrl(struct exynos_mipi_phy *state,
		unsigned int bit)
{
	void __iomem *addr = state->regs;
	unsigned int cfg;

	if (!addr)
		return 0;

	if (IS_ERR(addr)) {
		dev_err(state->dev, "%s Invalid address\n", __func__);
		return -EINVAL;
	}

	cfg = readl(addr);
	cfg &= ~(1 << bit);
	cfg |= (1 << bit);
	writel(cfg, addr);

	pr_debug("%s bit=%d, val=0x%x\n", __func__, bit, cfg);
	return 0;

}

/* 1: Enable reset -> release reset, 0: Enable reset */
static int __set_phy_reset(struct exynos_mipi_phy *state,
		unsigned int bit, unsigned int on)
{
	void __iomem *addr = state->regs;
	unsigned int cfg;

	if (!addr)
		return 0;

	if (IS_ERR(addr)) {
		dev_err(state->dev, "%s Invalid address\n", __func__);
		return -EINVAL;
	}

	cfg = readl(addr);
	cfg &= ~(1 << bit);
	writel(cfg, addr);

	/* release a reset before using a PHY */
	if (on) {
		cfg |= (1 << bit);
		writel(cfg, addr);
	}

	pr_debug("%s bit=%d, val=0x%x\n", __func__, bit, cfg);
	return 0;
}

static int __set_phy_init(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned int cfg;

	ret = exynos_pmu_read(phy_desc->iso_offset, &cfg);
	if (ret) {
		dev_err(state->dev, "%s Can't read 0x%x\n",
				__func__, phy_desc->iso_offset);
		ret = -EINVAL;
		goto phy_exit;
	}

	/* Add INIT_DONE flag when ISO is already bypass(LCD_ON_UBOOT) */
	if (cfg && EXYNOS_MIPI_PHY_ISO_BYPASS)
		phy_desc->data->flags |= MIPI_PHY_MxSx_INIT_DONE;

	if (phy_desc->init_bit != UINT_MAX)
		__set_phy_init_ctrl(state, phy_desc->init_bit);

phy_exit:
	return ret;
}

static int __set_phy_alone(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&state->slock, flags);

	if (on) {
		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset, on);

		__set_phy_reset(state, phy_desc->rst_bit, on);

	} else {
		__set_phy_reset(state, phy_desc->rst_bit, on);

		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset, on);

	}
	pr_debug("%s: isolation 0x%x, reset 0x%x\n", __func__,
			phy_desc->iso_offset, phy_desc->rst_bit);
	spin_unlock_irqrestore(&state->slock, flags);

	return ret;
}

static DEFINE_SPINLOCK(lock_share);
static int __set_phy_share(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&lock_share, flags);

	on ? ++(phy_desc->data->phy_count) : --(phy_desc->data->phy_count);

	/* If phy is already initialization(power_on) */
	if (state->owner == EXYNOS_MIPI_PHY_OWNER_DSIM &&
			phy_desc->data->flags & MIPI_PHY_MxSx_INIT_DONE) {
		phy_desc->data->flags &= (~MIPI_PHY_MxSx_INIT_DONE);
		spin_unlock_irqrestore(&lock_share, flags);
		return ret;
	}

	if (on) {
		/* Isolation bypass when reference count is 1 */
		if (phy_desc->data->phy_count)
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset, on);

		__set_phy_reset(state, phy_desc->rst_bit, on);

	} else {
		__set_phy_reset(state, phy_desc->rst_bit, on);

		/* Isolation enabled when reference count is zero */
		if (phy_desc->data->phy_count == 0)
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset, on);
	}

	pr_debug("%s: isolation 0x%x, reset 0x%x\n", __func__,
			phy_desc->iso_offset, phy_desc->rst_bit);
	spin_unlock_irqrestore(&lock_share, flags);

	return ret;
}

static int __set_phy_state(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;

	if (phy_desc->data->flags & MIPI_PHY_MxSx_SHARED)
		ret = __set_phy_share(state, phy_desc, on);
	else
		ret = __set_phy_alone(state, phy_desc, on);

	return ret;
}

static struct mipi_phy_data mipi_phy_m4s4_top = {
	.type = EXYNOS_MIPI_PHY_M4S4_TOP,
	.flags =  MIPI_PHY_MxSx_SHARED,
	.phy_count = 0,
};

static struct mipi_phy_data mipi_phy_m4s4_mod = {
	.type = EXYNOS_MIPI_PHY_M4S4_MOD,
	.flags =  MIPI_PHY_MxSx_SHARED,
	.phy_count = 0,
};

static struct mipi_phy_data mipi_phy_m1s2s2 = {
	.type = EXYNOS_MIPI_PHY_M1S2S2,
	.flags =  MIPI_PHY_MxSxSx_SHARED,
	.phy_count = 0,
};

static const struct of_device_id exynos_mipi_phy_of_table[] = {
	{
		.compatible = "samsung,mipi-phy-m4s4-top",
		.data = &mipi_phy_m4s4_top,
	},
	{
		.compatible = "samsung,mipi-phy-m4s4-mod",
		.data = &mipi_phy_m4s4_mod,
	},
	{
		.compatible = "samsung,mipi-phy-m1s2s2",
		.data = &mipi_phy_m1s2s2,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_mipi_phy_of_table);

#define to_mipi_video_phy(desc) \
	container_of((desc), struct exynos_mipi_phy, phys[(desc)->index])

static int exynos_mipi_phy_init(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_init(state, phy_desc, 1);
}


static int exynos_mipi_phy_power_on(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_state(state, phy_desc, 1);
}

static int exynos_mipi_phy_power_off(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_state(state, phy_desc, 0);
}

static struct phy *exynos_mipi_phy_of_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_mipi_phy *state = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] >= EXYNOS_MIPI_PHYS_NUM))
		return ERR_PTR(-ENODEV);

	return state->phys[args->args[0]].phy;
}

static struct phy_ops exynos_mipi_phy_ops = {
	.init		= exynos_mipi_phy_init,
	.power_on	= exynos_mipi_phy_power_on,
	.power_off	= exynos_mipi_phy_power_off,
	.owner		= THIS_MODULE,
};

static int exynos_mipi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	struct exynos_mipi_phy *state;
	struct phy_provider *phy_provider;
	struct mipi_phy_data *phy_data;
	const struct of_device_id *of_id;
	unsigned int iso[EXYNOS_MIPI_PHYS_NUM];
	unsigned int rst[EXYNOS_MIPI_PHYS_NUM];
	unsigned int init[EXYNOS_MIPI_PHYS_NUM];
	unsigned int i;
	int ret = 0, elements = 0;

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->dev  = &pdev->dev;

	of_id = of_match_device(of_match_ptr(exynos_mipi_phy_of_table), dev);
	if (!of_id)
		return -EINVAL;

	phy_data = (struct mipi_phy_data *)of_id->data;

	dev_set_drvdata(dev, state);
	spin_lock_init(&state->slock);

	elements = of_property_count_u32_elems(node, "isolation");
	if ((elements < 0) || (elements > EXYNOS_MIPI_PHYS_NUM)) {
		return -EINVAL;
	}
	ret = of_property_read_u32_array(node, "isolation", iso,
					elements);
	if (ret) {
		dev_err(dev, "cannot get mipi-phy isolation!!!\n");
		return ret;
	}

	/* reset control */
	for (i = 0; i < EXYNOS_MIPI_PHYS_NUM; ++i) {
		rst[i] = UINT_MAX;
		init[i] = UINT_MAX;
	}

	of_property_read_u32(node, "owner", &state->owner);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		state->regs = devm_ioremap_resource(dev, res);
		global_addr[state->owner] = state->regs;
		if (IS_ERR(state->regs))
			return PTR_ERR(state->regs);

	} else {
		state->regs = global_addr[state->owner];
	}

	if (of_property_read_u32_array(node, "reset", rst,
			elements))
		dev_info(dev, "doesn't control mipi-phy reset by sysreg!!!\n");

	/* it's optional */
	if (of_property_read_u32_array(node, "init", init,
				elements))
		dev_info(dev, "doesn't use mipi-phy init control!!!\n");

	for (i = 0; i < elements; i++) {
		state->phys[i].iso_offset = iso[i];
		state->phys[i].rst_bit	  = rst[i];
		state->phys[i].init_bit	  = init[i];
		dev_info(dev, "%s: iso 0x%x, reset %d (%d)\n", __func__,
				state->phys[i].iso_offset, state->phys[i].rst_bit,
				state->phys[i].init_bit);
	}

	for (i = 0; i < elements; i++) {
		struct phy *generic_phy = devm_phy_create(dev, NULL,
				&exynos_mipi_phy_ops);
		if (IS_ERR(generic_phy)) {
			dev_err(dev, "failed to create PHY\n");
			return PTR_ERR(generic_phy);
		}

		state->phys[i].index	= i;
		state->phys[i].phy	= generic_phy;
		state->phys[i].data	= phy_data;
		phy_set_drvdata(generic_phy, &state->phys[i]);
	}

	phy_provider = devm_of_phy_provider_register(dev,
			exynos_mipi_phy_of_xlate);

	if (IS_ERR(phy_provider))
		dev_err(dev, "failed to create exynos mipi-phy\n");
	else
		dev_err(dev, "Creating exynos-mipi-phy\n");

	return PTR_ERR_OR_ZERO(phy_provider);
}

static int exynos_mipi_phy_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s, successfully removed\n", __func__);
	return 0;
}

static struct platform_driver exynos_mipi_phy_driver = {
	.probe	= exynos_mipi_phy_probe,
	.remove	= exynos_mipi_phy_remove,
	.driver = {
		.name  = "exynos-mipi-phy",
		.of_match_table = of_match_ptr(exynos_mipi_phy_of_table),
		.suppress_bind_attrs = true,
	}
};
module_platform_driver(exynos_mipi_phy_driver);

MODULE_DESCRIPTION("Samsung EXYNOS SoC MIPI CSI/DSI PHY driver");
MODULE_LICENSE("GPL v2");
