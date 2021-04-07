/*
 * Samsung EXYNOS SoC series USB DRD PHY driver
 *
 * Phy provider for USB 3.0 DRD controller on Exynos SoC series
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 * Author: Vivek Gautam <gautam.vivek@samsung.com>
 *	   Minho Lee <minho55.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/exynos5-pmu.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/samsung_usb.h>
#include <linux/usb/otg.h>
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include "phy-exynos-usbdrd.h"
#include "phy-exynos-debug.h"

extern int sm5713_get_usb_connect(void);

static void exynos_usbdrd_check_connection(struct exynos_usbdrd_phy *phy_drd)
{
#if 0
	int usb_side;

	usb_side = sm5713_get_usb_connect();
	dev_info(phy_drd->dev, "USB is plugged in %d side...\n", usb_side);

	if (usb_side == 1) /* front */
		phy_drd->usbphy_info.used_phy_port = 1;
	else if (usb_side == 0)
		phy_drd->usbphy_info.used_phy_port = 0;
#endif
}

static int exynos_usbdrd_clk_prepare(struct exynos_usbdrd_phy *phy_drd)
{
	int i;
	int ret;

	for (i = 0; phy_drd->clocks[i] != NULL; i++) {
		ret = clk_prepare(phy_drd->clocks[i]);
		if (ret)
			goto err;
	}

	if (phy_drd->use_phy_umux) {
		for (i = 0; phy_drd->phy_clocks[i] != NULL; i++) {
			ret = clk_prepare(phy_drd->phy_clocks[i]);
			if (ret)
				goto err1;
		}
	}
	return 0;

err1:
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(phy_drd->phy_clocks[i]);
err:
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(phy_drd->clocks[i]);
	return ret;
}

static int exynos_usbdrd_clk_enable(struct exynos_usbdrd_phy *phy_drd,
					bool umux)
{
	int i;
	int ret;

#ifdef CONFIG_SOC_EXYNOS7885
	clk_set_rate(phy_drd->ref_clk, 50 * 1000000);
#endif

	if (!phy_drd->use_phy_umux) {
		for (i = 0; phy_drd->clocks[i] != NULL; i++) {
			ret = clk_enable(phy_drd->clocks[i]);
			if (ret)
				goto err;
		}
	} else {
		for (i = 0; phy_drd->phy_clocks[i] != NULL; i++) {
			ret = clk_enable(phy_drd->phy_clocks[i]);
			if (ret)
				goto err1;
			}
	}
	return 0;

err1:
	for (i = i - 1; i >= 0; i--)
		clk_disable(phy_drd->phy_clocks[i]);
	return ret;
err:
	for (i = i - 1; i >= 0; i--)
		clk_disable(phy_drd->clocks[i]);
	return ret;
}

static void exynos_usbdrd_clk_unprepare(struct exynos_usbdrd_phy *phy_drd)
{
	int i;

	for (i = 0; phy_drd->clocks[i] != NULL; i++)
		clk_unprepare(phy_drd->clocks[i]);
	for (i = 0; phy_drd->phy_clocks[i] != NULL; i++)
		clk_unprepare(phy_drd->phy_clocks[i]);
}

static void exynos_usbdrd_clk_disable(struct exynos_usbdrd_phy *phy_drd, bool umux)
{
	int i;

	if (!umux) {
		for (i = 0; phy_drd->clocks[i] != NULL; i++)
			clk_disable(phy_drd->clocks[i]);
	} else {
		for (i = 0; phy_drd->phy_clocks[i] != NULL; i++)
			clk_disable(phy_drd->phy_clocks[i]);
	}
}
static int exynos_usbdrd_phyclk_get(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	const char	**phyclk_ids;
	const char	**clk_ids;
	const char	*refclk_name;
	struct clk	*clk;
	int		phyclk_count;
	int		clk_count;
	bool		is_phyclk = false;
	int		clk_index = 0;
	int		i, j, ret;

	phyclk_count = of_property_count_strings(dev->of_node, "phyclk_mux");
	if (IS_ERR_VALUE((unsigned long)phyclk_count)) {
		dev_err(dev, "invalid phyclk list in %s node\n",
							dev->of_node->name);
		return -EINVAL;
	}

	phyclk_ids = (const char **)devm_kmalloc(dev,
					(phyclk_count+1) * sizeof(const char *),
					GFP_KERNEL);
	for (i = 0; i < phyclk_count; i++) {
		ret = of_property_read_string_index(dev->of_node,
						"phyclk_mux", i, &phyclk_ids[i]);
		if (ret) {
			dev_err(dev, "failed to read phyclk_mux name %d from %s node\n",
					i, dev->of_node->name);
			return ret;
		}
	}
	phyclk_ids[phyclk_count] = NULL;

	if (!strcmp("none", phyclk_ids[0])) {
		dev_info(dev, "don't need user Mux for phyclk\n");
		phy_drd->use_phy_umux = false;
		phyclk_count = 0;

	} else {
		phy_drd->use_phy_umux = true;

		phy_drd->phy_clocks = (struct clk **) devm_kmalloc(dev,
				(phyclk_count+1) * sizeof(struct clk *),
				GFP_KERNEL);
		if (!phy_drd->phy_clocks) {
			dev_err(dev, "failed to alloc : phy clocks\n");
			return -ENOMEM;
		}

		for (i = 0; phyclk_ids[i] != NULL; i++) {
			clk = devm_clk_get(dev, phyclk_ids[i]);
			if (IS_ERR_OR_NULL(clk)) {
				dev_err(dev, "couldn't get %s clock\n", phyclk_ids[i]);
				return -EINVAL;
			}
			phy_drd->phy_clocks[i] = clk;
		}

		phy_drd->phy_clocks[i] = NULL;
	}

	clk_count = of_property_count_strings(dev->of_node, "clock-names");
	if (IS_ERR_VALUE((unsigned long)clk_count)) {
		dev_err(dev, "invalid clk list in %s node", dev->of_node->name);
		return -EINVAL;
	}
	clk_ids = (const char **)devm_kmalloc(dev,
				(clk_count + 1) * sizeof(const char *),
				GFP_KERNEL);
	for (i = 0; i < clk_count; i++) {
		ret = of_property_read_string_index(dev->of_node, "clock-names",
								i, &clk_ids[i]);
		if (ret) {
			dev_err(dev, "failed to read clocks name %d from %s node\n",
					i, dev->of_node->name);
			return ret;
		}
	}
	clk_ids[clk_count] = NULL;

	phy_drd->clocks = (struct clk **) devm_kmalloc(dev,
				(clk_count + 1) * sizeof(struct clk *), GFP_KERNEL);
	if (!phy_drd->clocks) {
		dev_err(dev, "failed to alloc for clocks\n");
		return -ENOMEM;
	}

	for (i = 0; clk_ids[i] != NULL; i++) {
		if (phyclk_count) {
			for (j = 0; phyclk_ids[j] != NULL; j++) {
				if (!strcmp(phyclk_ids[j], clk_ids[i])) {
					is_phyclk = true;
					phyclk_count--;
				}
			}
		}
		if (!is_phyclk) {
			clk = devm_clk_get(dev, clk_ids[i]);
			if (IS_ERR_OR_NULL(clk)) {
				dev_err(dev, "couldn't get %s clock\n", clk_ids[i]);
				return -EINVAL;
			}
			phy_drd->clocks[clk_index] = clk;
			clk_index++;
		}
		is_phyclk = false;
	}
	phy_drd->clocks[clk_index] = NULL;

	ret = of_property_read_string_index(dev->of_node,
						"phy_refclk", 0, &refclk_name);
	if (ret) {
		dev_err(dev, "failed to read ref_clocks name from %s node\n",
				dev->of_node->name);
		return ret;
	}

	if (!strcmp("none", refclk_name)) {
		dev_err(dev, "phy reference clock shouldn't be omitted");
		return -EINVAL;
	}

	for (i = 0; clk_ids[i] != NULL; i++) {
		if (!strcmp(clk_ids[i], refclk_name)) {
			phy_drd->ref_clk = devm_clk_get(dev, refclk_name);
			break;
		}
	}

	if (IS_ERR_OR_NULL(phy_drd->ref_clk)) {
		dev_err(dev, "%s couldn't get ref_clk", __func__);
		return -EINVAL;
	}

	devm_kfree(dev, phyclk_ids);
	devm_kfree(dev, clk_ids);

	return 0;

}

static int exynos_usbdrd_clk_get(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	int		ret;

	ret = exynos_usbdrd_phyclk_get(phy_drd);
	if (ret < 0) {
		dev_err(dev, "failed to get clock for DRD USBPHY");
		return ret;
	}

	return 0;
}

static inline
struct exynos_usbdrd_phy *to_usbdrd_phy(struct phy_usb_instance *inst)
{
	return container_of((inst), struct exynos_usbdrd_phy,
			    phys[(inst)->index]);
}

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
void exynos_usbdrd_phy_get_otp_info(struct exynos_usbdrd_phy *phy_drd)
{
	struct tune_bits *data;
	u16 magic;
	u8 type;
	u8 index_count;
	u8 i, j;

	phy_drd->otp_index[0] = phy_drd->otp_index[1] = 0;

	for (i = 0; i < OTP_SUPPORT_USBPHY_NUMBER; i++) {
		magic = i ? OTP_MAGIC_USB2 : OTP_MAGIC_USB3;

		if (otp_tune_bits_parsed(magic, &type, &index_count, &data)) {
			dev_err(phy_drd->dev, "%s failed to get usb%d otp\n",
				__func__, i ? 2 : 3);
			continue;
		}
		dev_info(phy_drd->dev, "usb[%d] otp index_count: %d\n",
								i, index_count);

		if (!index_count) {
			phy_drd->otp_data[i] = NULL;
			continue;
		}

		phy_drd->otp_data[i] = devm_kzalloc(phy_drd->dev,
			sizeof(*data) * index_count, GFP_KERNEL);
		if (!phy_drd->otp_data[i])
			continue;

		phy_drd->otp_index[i] = index_count;
		phy_drd->otp_type[i] = type ? 4 : 1;
		dev_info(phy_drd->dev, "usb[%d] otp type: %d\n", i, type);

		for (j = 0; j < index_count; j++) {
			phy_drd->otp_data[i][j].index = data[j].index;
			phy_drd->otp_data[i][j].value = data[j].value;
			dev_dbg(phy_drd->dev,
				"usb[%d][%d] otp_data index:%d, value:0x%08x\n",
					i, j, phy_drd->otp_data[i][j].index,
					phy_drd->otp_data[i][j].value);
		}
	}
}
#endif

/*
 * exynos_rate_to_clk() converts the supplied clock rate to the value that
 * can be written to the phy register.
 */
static unsigned int exynos_rate_to_clk(struct exynos_usbdrd_phy *phy_drd)
{
	int ret;

#ifdef CONFIG_SOC_EXYNOS7885
	clk_set_rate(phy_drd->ref_clk, 50 * 1000000);
#endif

	ret = clk_prepare_enable(phy_drd->ref_clk);
	if (ret) {
		dev_err(phy_drd->dev, "%s failed to enable ref_clk", __func__);
		return 0;
	}

	/* EXYNOS_FSEL_MASK */
	switch (clk_get_rate(phy_drd->ref_clk)) {
	case 9600 * KHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_9MHZ6;
		break;
	case 10 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_10MHZ;
		break;
	case 12 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_12MHZ;
		break;
	case 19200 * KHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_19MHZ2;
		break;
	case 20 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_20MHZ;
		break;
	case 24 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_24MHZ;
		break;
	case 26 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_26MHZ;
		break;
	case 50 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_50MHZ;
		break;
	default:
		phy_drd->extrefclk = 0;
		clk_disable_unprepare(phy_drd->ref_clk);
		return -EINVAL;
	}

	clk_disable_unprepare(phy_drd->ref_clk);

	return 0;
}


static void exynos_usbdrd_usb_txco_enable(struct phy_usb_instance *inst, int on)
{
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);
	void __iomem *base;
	u32	reg;

	base = ioremap(0x11860000, 0x100000);
	if(!base) {
		dev_err(phy_drd->dev, "[%s] Unable to map I/O memory\n",
							__func__);
		return;
	}
	reg = readl(base + EXYNOS_USBDEV_PHY_CONTROL);

	dev_info(phy_drd->dev, "[%s] ++USB DEVCTRL reg 0x%x \n",
							__func__, reg);

	if (!on) {
		reg |= ENABLE_TCXO_BUF_MASK;
	} else {
		reg &= ~ENABLE_TCXO_BUF_MASK;
	}
	writel(reg, base + EXYNOS_USBDEV_PHY_CONTROL);

	reg = readl(base + EXYNOS_USBDEV_PHY_CONTROL);
	dev_info(phy_drd->dev, "[%s] --USB DEVCTRL reg 0x%x \n",
							__func__, reg);
	iounmap(base);
}

static void exynos_usbdrd_pipe3_phy_isol(struct phy_usb_instance *inst,
					unsigned int on, unsigned int mask)
{
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);
	unsigned int val;

	if (phy_drd->usb3phy_isolation == 1)
		return;

	if (!inst->reg_pmu)
		return;

	val = on ? 0 : mask;

	dev_info(phy_drd->dev, "[%s] val : 0x%x / mask : 0x%x \n",
							__func__, val, mask);
	regmap_update_bits(inst->reg_pmu, inst->pmu_offset,
			   mask, val);

	/* Enable TCXO_USB */
	val = on ? 0 : ENABLE_TCXO_BUF_MASK;
	regmap_update_bits(inst->reg_pmu, inst->pmu_offset,
			   ENABLE_TCXO_BUF_MASK, val);

	/* exynos_usbdrd_usb_txco_enable(inst, on); */
}

static void exynos_usbdrd_utmi_phy_isol(struct phy_usb_instance *inst,
					unsigned int on, unsigned int mask)
{
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);
	unsigned int val;

	if (!inst->reg_pmu)
		return;

	val = on ? 0 : mask;

	dev_info(phy_drd->dev, "[%s] val : 0x%x / mask : 0x%x \n",
						__func__, val, mask);
	regmap_update_bits(inst->reg_pmu, inst->pmu_offset,
			   mask, val);

	exynos_usbdrd_usb_txco_enable(inst, on);
}

/*
 * Sets the pipe3 phy's clk as EXTREFCLK (XXTI) which is internal clock
 * from clock core. Further sets multiplier values and spread spectrum
 * clock settings for SuperSpeed operations.
 */
static unsigned int
exynos_usbdrd_pipe3_set_refclk(struct phy_usb_instance *inst)
{
	return 0;
}

/*
 * Sets the utmi phy's clk as EXTREFCLK (XXTI) which is internal clock
 * from clock core. Further sets the FSEL values for HighSpeed operations.
 */
static unsigned int
exynos_usbdrd_utmi_set_refclk(struct phy_usb_instance *inst)
{
	static u32 reg;
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);
	/* PHYCLKRST setting isn't required in Combo PHY */
	if (phy_drd->usbphy_info.version >= EXYNOS_USBPHY_VER_02_0_0)
		return -EINVAL;

	/* restore any previous reference clock settings */
	reg = readl(phy_drd->reg_phy + EXYNOS_DRD_PHYCLKRST);

	reg &= ~PHYCLKRST_REFCLKSEL_MASK;
	reg |=	PHYCLKRST_REFCLKSEL_EXT_REFCLK;

	reg &= ~PHYCLKRST_FSEL_UTMI_MASK |
		PHYCLKRST_MPLL_MULTIPLIER_MASK |
		PHYCLKRST_SSC_REFCLKSEL_MASK;
	reg |= PHYCLKRST_FSEL(phy_drd->extrefclk);

	return reg;
}

#ifdef OLD_FASHIONED_PHY_TUNE
/*
 * Sets the default PHY tuning values for high-speed connection.
 */
static int exynos_usbdrd_fill_hstune(struct exynos_usbdrd_phy *phy_drd,
				struct device_node *node)
{
	struct device *dev = phy_drd->dev;
	struct exynos_usbphy_hs_tune *hs_tune = phy_drd->hs_value;
	int ret;
	u32 res[2];
	u32 value;

	ret = of_property_read_u32_array(node, "tx_vref", res, 2);
	if (ret == 0) {
		hs_tune[0].tx_vref = res[0];
		hs_tune[1].tx_vref = res[1];
	} else {
		dev_err(dev, "can't get tx_vref value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_pre_emp", res, 2);
	if (ret == 0) {
		hs_tune[0].tx_pre_emp = res[0];
		hs_tune[1].tx_pre_emp = res[1];
	} else {
		dev_err(dev, "can't get tx_pre_emp value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_pre_emp_puls", res, 2);
	if (ret == 0) {
		hs_tune[0].tx_pre_emp_puls = res[0];
		hs_tune[1].tx_pre_emp_puls = res[1];
	} else {
		dev_err(dev, "can't get tx_pre_emp_puls value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_res", res, 2);
	if (ret == 0) {
		hs_tune[0].tx_res = res[0];
		hs_tune[1].tx_res = res[1];
	} else {
		dev_err(dev, "can't get tx_res value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_rise", res, 2);
	if (ret == 0) {
		hs_tune[0].tx_rise = res[0];
		hs_tune[1].tx_rise = res[1];
	} else {
		dev_err(dev, "can't get tx_rise value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_hsxv", res, 2);
	if (ret == 0) {
		hs_tune[0].tx_hsxv = res[0];
		hs_tune[1].tx_hsxv = res[1];
	} else {
		dev_err(dev, "can't get tx_hsxv value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_fsls", res, 2);
	if (ret == 0) {
		hs_tune[0].tx_fsls = res[0];
		hs_tune[1].tx_fsls = res[1];
	} else {
		dev_err(dev, "can't get tx_fsls value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "rx_sqrx", res, 2);
	if (ret == 0) {
		hs_tune[0].rx_sqrx = res[0];
		hs_tune[1].rx_sqrx = res[1];
	} else {
		dev_err(dev, "can't get tx_sqrx value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "compdis", res, 2);
	if (ret == 0) {
		hs_tune[0].compdis = res[0];
		hs_tune[1].compdis = res[1];
	} else {
		dev_err(dev, "can't get compdis value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "otg", res, 2);
	if (ret == 0) {
		hs_tune[0].otg = res[0];
		hs_tune[1].otg = res[1];
	} else {
		dev_err(dev, "can't get otg_tune value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "enable_user_imp", res, 2);
	if (ret == 0) {
		if (res[0]) {
			hs_tune[0].enable_user_imp = true;
			hs_tune[1].enable_user_imp = true;
			hs_tune[0].user_imp_value = res[1];
			hs_tune[1].user_imp_value = res[1];
		} else {
			hs_tune[0].enable_user_imp = false;
			hs_tune[1].enable_user_imp = false;
		}
	} else {
		dev_err(dev, "can't get enable_user_imp value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "is_phyclock", &value);
	if (ret == 0) {
		if (value == 1) {
			hs_tune[0].utmi_clk = USBPHY_UTMI_PHYCLOCK;
			hs_tune[1].utmi_clk = USBPHY_UTMI_PHYCLOCK;
		} else {
			hs_tune[0].utmi_clk = USBPHY_UTMI_FREECLOCK;
			hs_tune[1].utmi_clk = USBPHY_UTMI_FREECLOCK;
		}
	} else {
		dev_err(dev, "can't get is_phyclock value, error = %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * Sets the default PHY tuning values for super-speed connection.
 */
static int exynos_usbdrd_fill_sstune(struct exynos_usbdrd_phy *phy_drd,
							struct device_node *node)
{
	struct device *dev = phy_drd->dev;
	struct exynos_usbphy_ss_tune *ss_tune = phy_drd->ss_value;
	u32 res[2];
	int ret;

	ret = of_property_read_u32_array(node, "tx_boost_level", res, 2);
	if (ret == 0) {
		ss_tune[0].tx_boost_level = res[0];
		ss_tune[1].tx_boost_level = res[1];
	} else {
		dev_err(dev, "can't get tx_boost_level value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_swing_level", res, 2);
	if (ret == 0) {
		ss_tune[0].tx_swing_level = res[0];
		ss_tune[1].tx_swing_level = res[1];
	} else {
		dev_err(dev, "can't get tx_swing_level value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_swing_full", res, 2);
	if (ret == 0) {
		ss_tune[0].tx_swing_full = res[0];
		ss_tune[1].tx_swing_full = res[1];
	} else {
		dev_err(dev, "can't get tx_swing_full value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_swing_low", res, 2);
	if (ret == 0) {
		ss_tune[0].tx_swing_low = res[0];
		ss_tune[1].tx_swing_low = res[1];
	} else {
		dev_err(dev, "can't get tx_swing_low value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_deemphasis_mode", res, 2);
	if (ret == 0) {
		ss_tune[0].tx_deemphasis_mode = res[0];
		ss_tune[1].tx_deemphasis_mode = res[1];
	} else {
		dev_err(dev, "can't get tx_deemphasis_mode value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_deemphasis_3p5db", res, 2);
	if (ret == 0) {
		ss_tune[0].tx_deemphasis_3p5db = res[0];
		ss_tune[1].tx_deemphasis_3p5db = res[1];
	} else {
		dev_err(dev, "can't get tx_deemphasis_3p5db value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "tx_deemphasis_6db", res, 2);
	if (ret == 0) {
		ss_tune[0].tx_deemphasis_6db = res[0];
		ss_tune[1].tx_deemphasis_6db = res[1];
	} else {
		dev_err(dev, "can't get tx_deemphasis_6db value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "enable_ssc", res, 2);
	if (ret == 0) {
		ss_tune[0].enable_ssc = res[0];
		ss_tune[1].enable_ssc = res[1];
	} else {
		dev_err(dev, "can't get enable_ssc value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "ssc_range", res, 2);
	if (ret == 0) {
		ss_tune[0].ssc_range = res[0];
		ss_tune[1].ssc_range = res[1];
	} else {
		dev_err(dev, "can't get ssc_range value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "los_bias", res, 2);
	if (ret == 0) {
		ss_tune[0].los_bias = res[0];
		ss_tune[1].los_bias = res[1];
	} else {
		dev_err(dev, "can't get los_bias value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "los_mask_val", res, 2);
	if (ret == 0) {
		ss_tune[0].los_mask_val = res[0];
		ss_tune[1].los_mask_val = res[1];
	} else {
		dev_err(dev, "can't get los_mask_val value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "enable_fixed_rxeq_mode", res, 2);
	if (ret == 0) {
		ss_tune[0].enable_fixed_rxeq_mode = res[0];
		ss_tune[1].enable_fixed_rxeq_mode = res[1];
	} else {
		dev_err(dev, "can't get enable_fixed_rxeq_mode value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "fix_rxeq_value", res, 2);
	if (ret == 0) {
		ss_tune[0].fix_rxeq_value = res[0];
		ss_tune[1].fix_rxeq_value = res[1];
	} else {
		dev_err(dev, "can't get fix_rxeq_value value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "set_crport_level_en", res, 2);
	if (ret == 0) {
		ss_tune[0].set_crport_level_en = res[0];
		ss_tune[1].set_crport_level_en = res[1];
	} else {
		dev_err(dev, "can't get set_crport_level_en value, error = %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "set_crport_mpll_charge_pump", res, 2);
	if (ret == 0) {
		ss_tune[0].set_crport_mpll_charge_pump = res[0];
		ss_tune[1].set_crport_mpll_charge_pump = res[1];
	} else {
		dev_err(dev, "can't get set_crport_mpll_charge_pump value, error = %d\n", ret);
		return -EINVAL;
	}

	return 0;
}
#endif

static int exynos_usbdrd_fill_hstune_param(struct exynos_usbdrd_phy *phy_drd,
				struct device_node *node)
{
	struct device *dev = phy_drd->dev;
	struct device_node *child = NULL;
	struct exynos_usb_tune_param *hs_tune_param;
	size_t size = sizeof(struct exynos_usb_tune_param);
	int ret;
	u32 res[2];
	u32 param_index = 0;
	const char *name;

	ret = of_property_read_u32_array(node, "hs_tune_cnt", &res[0], 1);

	dev_info(dev, "%s hs tune cnt = %d\n", __func__, res[0]);

	hs_tune_param = devm_kzalloc(dev, size*(res[0]+1), GFP_KERNEL);
	if (hs_tune_param == NULL)
		return -ENOMEM;

	phy_drd->usbphy_info.tune_param = hs_tune_param;

	for_each_child_of_node(node, child) {
		ret = of_property_read_string(child, "tune_name", &name);
		if (ret == 0) {
			memcpy(hs_tune_param[param_index].name, name, strlen(name));
		} else {
			dev_err(dev, "failed to read hs tune name from %s node\n", child->name);
			return ret;
		}

		ret = of_property_read_u32_array(child, "tune_value", res, 2);
		if (ret == 0) {
			phy_drd->hs_tune_param_value[param_index][0] = res[0];
			phy_drd->hs_tune_param_value[param_index][1] = res[1];
		} else {
			dev_err(dev, "failed to read hs tune value from %s node\n", child->name);
			return -EINVAL;
		}
		param_index++;
	}

	hs_tune_param[param_index].value = EXYNOS_USB_TUNE_LAST;

	return 0;
}

/*
 * Sets the default PHY tuning values for super-speed connection.
 */
static int exynos_usbdrd_fill_sstune_param(struct exynos_usbdrd_phy *phy_drd,
							struct device_node *node)
{
	struct device *dev = phy_drd->dev;
	struct device_node *child = NULL;
	struct exynos_usb_tune_param *ss_tune_param;
	size_t size = sizeof(struct exynos_usb_tune_param);
	int ret;
	u32 res[2];
	u32 param_index = 0;
	const char *name;

	ret = of_property_read_u32_array(node, "ss_tune_cnt", &res[0], 1);

	dev_info(dev, "%s ss tune cnt = %d\n", __func__, res[0]);

	ss_tune_param = devm_kzalloc(dev, size*res[0], GFP_KERNEL);
	if (ss_tune_param == NULL)
		return -ENOMEM;

	phy_drd->usbphy_sub_info.tune_param = ss_tune_param;
	for_each_child_of_node(node, child) {
		ret = of_property_read_string(child, "tune_name", &name);
		if (ret == 0)
			memcpy(ss_tune_param[param_index].name, name, strlen(name));
		else {
			dev_err(dev, "failed to read ss tune name from %s node\n", child->name);
			return ret;
		}

		ret = of_property_read_u32_array(child, "tune_value", res, 2);
		if (ret == 0) {
			ss_tune_param[param_index].value = res[0];
		} else {
			dev_err(dev, "failed to read ss tune value from %s node\n", child->name);
			return -EINVAL;
		}
		param_index++;
	}

	ss_tune_param[param_index].value = EXYNOS_USB_TUNE_LAST;

	return 0;
}

static int exynos_usbdrd_get_phy_refsel(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	struct device_node *node = dev->of_node;
	int value, ret;
	int check_flag = 0;

	ret = of_property_read_u32(node, "phy_refsel_clockcore", &value);
	if (ret < 0) {
		dev_err(dev, "can't get phy_refsel_clockcore, error = %d\n", ret);
		return ret;
	}

	if (value == 1) {
		phy_drd->usbphy_info.refsel = USBPHY_REFSEL_CLKCORE;
		phy_drd->usbphy_sub_info.refsel = USBPHY_REFSEL_CLKCORE;
	} else {
		check_flag++;
	}

	ret = of_property_read_u32(node, "phy_refsel_ext_osc", &value);
	if (ret < 0) {
		dev_err(dev, "can't get phy_refsel_ext_osc, error = %d\n", ret);
		return ret;
	}

	if (value == 1) {
		phy_drd->usbphy_info.refsel = USBPHY_REFSEL_EXT_OSC;
		phy_drd->usbphy_sub_info.refsel = USBPHY_REFSEL_EXT_OSC;
	} else {
		check_flag++;
	}

	ret = of_property_read_u32(node, "phy_refsel_xtal", &value);
	if (ret < 0) {
		dev_err(dev, "can't get phy_refsel_xtal, error = %d\n", ret);
		return ret;
	}

	if (value == 1) {
		phy_drd->usbphy_info.refsel = USBPHY_REFSEL_EXT_XTAL;
		phy_drd->usbphy_sub_info.refsel = USBPHY_REFSEL_EXT_XTAL;
	} else {
		check_flag++;
	}

	ret = of_property_read_u32(node, "phy_refsel_diff_pad", &value);
	if (ret < 0) {
		dev_err(dev, "can't get phy_refsel_diff_pad, error = %d\n", ret);
		return ret;
	}

	if (value == 1) {
		phy_drd->usbphy_info.refsel = USBPHY_REFSEL_DIFF_PAD;
		phy_drd->usbphy_sub_info.refsel = USBPHY_REFSEL_DIFF_PAD;
	} else {
		check_flag++;
	}

	ret = of_property_read_u32(node, "phy_refsel_diff_internal", &value);
	if (ret < 0) {
		dev_err(dev, "can't get phy_refsel_diff_internal, error = %d\n", ret);
		return ret;
	}

	if (value == 1) {
		phy_drd->usbphy_info.refsel = USBPHY_REFSEL_DIFF_INTERNAL;
		phy_drd->usbphy_sub_info.refsel = USBPHY_REFSEL_DIFF_INTERNAL;
	} else {
		check_flag++;
	}

	ret = of_property_read_u32(node, "phy_refsel_diff_single", &value);
	if (ret < 0) {
		dev_err(dev, "can't get phy_refsel_diff_single, error = %d\n", ret);
		return ret;
	}

	if (value == 1) {
		phy_drd->usbphy_info.refsel = USBPHY_REFSEL_DIFF_SINGLE;
		phy_drd->usbphy_sub_info.refsel = USBPHY_REFSEL_DIFF_SINGLE;
	} else {
		check_flag++;
	}

	if (check_flag > 5) {
		dev_err(dev, "USB refsel Must be choosed\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos_usbdrd_get_sub_phyinfo(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	struct device_node *tune_node;
	int ret;
	int value;

	if (!of_property_read_u32(dev->of_node, "sub_phy_version", &value)) {
		phy_drd->usbphy_sub_info.version = value;
	} else {
		dev_err(dev, "can't get sub_phy_version\n");
		return -EINVAL;
	}
	phy_drd->usbphy_sub_info.refclk = phy_drd->extrefclk;
	phy_drd->usbphy_sub_info.regs_base = phy_drd->reg_phy2;
	phy_drd->usbphy_sub_info.regs_base_2nd = phy_drd->reg_phy3;

	/*
	 * use PHY of samsung
	 */
	tune_node = of_parse_phandle(dev->of_node, "ss_tune_param", 0);
	if (tune_node != NULL) {
		ret = exynos_usbdrd_fill_sstune_param(phy_drd, tune_node);
		if (ret < 0) {
			dev_err(dev, "can't fill super speed tuning param\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int exynos_usbdrd_get_phyinfo(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	struct device_node *tune_node;
	int ret;
	int value;

	if (!of_property_read_u32(dev->of_node, "phy_version", &value)) {
		phy_drd->usbphy_info.version = value;
	} else {
		dev_err(dev, "can't get phy_version\n");
		return -EINVAL;
	}

	if (!of_property_read_u32(dev->of_node, "use_io_for_ovc", &value)) {
		phy_drd->usbphy_info.use_io_for_ovc = value ? true : false;
	} else {
		dev_err(dev, "can't get io_for_ovc\n");
		return -EINVAL;
	}

	if (!of_property_read_u32(dev->of_node, "common_block_disable", &value)) {
		phy_drd->usbphy_info.common_block_disable = value ? true : false;
	} else {
		dev_err(dev, "can't get common_block_disable\n");
		return -EINVAL;
	}

	phy_drd->usbphy_info.refclk = phy_drd->extrefclk;
	phy_drd->usbphy_info.regs_base = phy_drd->reg_phy;

	if (!of_property_read_u32(dev->of_node, "is_not_vbus_pad", &value)) {
		phy_drd->usbphy_info.not_used_vbus_pad = value ? true : false;
	} else {
		dev_err(dev, "can't get vbus_pad\n");
		return -EINVAL;
	}
	if (!of_property_read_u32(dev->of_node, "used_phy_port", &value)) {
		phy_drd->usbphy_info.used_phy_port = value ? true : false;
	} else {
		dev_err(dev, "can't get used_phy_port\n");
		return -EINVAL;
	}

	ret = exynos_usbdrd_get_phy_refsel(phy_drd);
	if (ret < 0)
		dev_err(dev, "can't get phy refsel\n");

#ifdef OLD_FASHIONED_PHY_TUNE
	/*
	 * use PHY of synopsys
	 */
	tune_node = of_parse_phandle(dev->of_node, "ss_tune_info", 0);
	if (tune_node == NULL)
		dev_info(dev, "don't need usbphy tuning info for super speed\n");

	if (of_device_is_available(tune_node)) {
		ret = exynos_usbdrd_fill_sstune(phy_drd, tune_node);
		if (ret < 0) {
			dev_err(dev, "can't fill super speed tuning info\n");
			return -EINVAL;
		}
	}

	/*
	 * use PHY of synopsys
	 */
	tune_node = of_parse_phandle(dev->of_node, "hs_tune_info", 0);
	if (tune_node == NULL)
		dev_info(dev, "don't need usbphy tuning info for high speed\n");

	if (of_device_is_available(tune_node)) {
		ret = exynos_usbdrd_fill_hstune(phy_drd, tune_node);
		if (ret < 0) {
			dev_err(dev, "can't fill high speed tuning info\n");
			return -EINVAL;
		}
	}
#endif

	/*
	 * use PHY of synopsys
	 */
	tune_node = of_parse_phandle(dev->of_node, "ss_tune_param", 0);
	if (tune_node != NULL) {
		ret = exynos_usbdrd_fill_sstune_param(phy_drd, tune_node);
		if (ret < 0) {
			dev_err(dev, "can't fill super speed tuning param\n");
			return -EINVAL;
		}
	} else {
		dev_info(dev, "don't need usbphy tuning param for super speed\n");
	}

	/*
	 * use PHY of samsung
	 */
	tune_node = of_parse_phandle(dev->of_node, "hs_tune_param", 0);
	if (tune_node != NULL) {
		ret = exynos_usbdrd_fill_hstune_param(phy_drd, tune_node);
		if (ret < 0) {
			dev_err(dev, "can't fill high speed tuning param\n");
			return -EINVAL;
		}
	} else {
		dev_info(dev, "don't need usbphy tuning param for high speed\n");
	}

	dev_info(phy_drd->dev, "usbphy info: version:0x%x, refclk:0x%x\n",
		phy_drd->usbphy_info.version, phy_drd->usbphy_info.refclk);

	return 0;
}

static int exynos_usbdrd_get_iptype(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	int ret, value;

	ret = of_property_read_u32(dev->of_node, "ip_type", &value);
	if (ret) {
		dev_err(dev, "can't get ip type");
		return ret;
	}

	switch (value) {
	case TYPE_USB3DRD:
		phy_drd->ip_type = TYPE_USB3DRD;
		dev_info(dev, "It is TYPE USB3DRD");
		break;
	case TYPE_USB3HOST:
		phy_drd->ip_type = TYPE_USB3HOST;
		dev_info(dev, "It is TYPE USB3HOST");
		break;
	case TYPE_USB2DRD:
		phy_drd->ip_type = TYPE_USB2DRD;
		dev_info(dev, "It is TYPE USB2DRD");
		break;
	case TYPE_USB2HOST:
		phy_drd->ip_type = TYPE_USB2HOST;
		dev_info(dev, "It is TYPE USB2HOST");
	default:
		break;
	}

	if (!of_property_read_u32(dev->of_node, "usb3phy-isolation", &value)) {
		if (value == 1)
			dev_info(dev, "USB3.0 PHY Isolation is ENABLED!!!\n");
		phy_drd->usb3phy_isolation = value;
	} else {
		phy_drd->usb3phy_isolation = 0;
	}

	return 0;
}

static void exynos_usbdrd_pipe3_init(struct exynos_usbdrd_phy *phy_drd)
{
	if (phy_drd->usb3phy_isolation == 1) {
		dev_info(phy_drd->dev, "USB3.0 PHY is isolated...\n");
		return;
	}

	exynos_usbdrd_check_connection(phy_drd);
	phy_exynos_usb_v3p1_enable(&phy_drd->usbphy_info);
}

static void exynos_usbdrd_utmi_init(struct exynos_usbdrd_phy *phy_drd)
{
	int ret;
	struct phy_usb_instance *inst = &phy_drd->phys[0];
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	struct tune_bits *otp_data;
	u8 otp_type;
	u8 otp_index;
	u8 i;
#endif
	pr_info("%s: +++\n", __func__);

	if (gpio_is_valid(phy_drd->phy_port)) {
		phy_drd->usbphy_info.used_phy_port = !gpio_get_value(phy_drd->phy_port);
		dev_info(phy_drd->dev, "%s: phy port[%d]\n", __func__,
						phy_drd->usbphy_info.used_phy_port);
	}

	inst->phy_cfg->phy_isol(inst, 0, inst->pmu_mask);

	ret = exynos_usbdrd_clk_enable(phy_drd, false);
	if (ret) {
		dev_err(phy_drd->dev, "%s: Failed to enable clk\n", __func__);
		return;
	}

	phy_exynos_usb_v3p1_enable(&phy_drd->usbphy_info);
	/*
	 * The below function is used to block USB3.0 PHY. If you don't want to
	 * use USB3.0 PHY, add this function and comment phy_exynos_usbv3p1_pipe
	 * _ready().
	 *
	 * phy_exynos_usb_v3p1_pipe_ovrd(&phy_drd->usbphy_info);
	 */

	if (phy_drd->usb3phy_isolation == 1)
		phy_exynos_usb_v3p1_pipe_ovrd(&phy_drd->usbphy_info);
	else
		phy_exynos_usb_v3p1_pipe_ready(&phy_drd->usbphy_info);

	if (phy_drd->use_phy_umux) {
		/* USB User MUX enable */
		ret = exynos_usbdrd_clk_enable(phy_drd, true);
		if (ret) {
			dev_err(phy_drd->dev, "%s: Failed to enable clk\n", __func__);
			return;
		}
	}
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	if (phy_drd->ip_type < TYPE_USB2DRD) {
		otp_type = phy_drd->otp_type[OTP_USB3PHY_INDEX];
		otp_index = phy_drd->otp_index[OTP_USB3PHY_INDEX];
		otp_data = phy_drd->otp_data[OTP_USB3PHY_INDEX];
	} else {
		otp_type = phy_drd->otp_type[OTP_USB2PHY_INDEX];
		otp_index = phy_drd->otp_index[OTP_USB2PHY_INDEX];
		otp_data = phy_drd->otp_data[OTP_USB2PHY_INDEX];
	}

	for (i = 0; i < otp_index; i++) {
		samsung_exynos_cal_usb3phy_write_register(
			&phy_drd->usbphy_info,
			otp_data[i].index * otp_type,
			otp_data[i].value);
	}
#endif

	pr_info("%s: ---\n", __func__);
}

static int exynos_usbdrd_phy_init(struct phy *phy)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	/* UTMI or PIPE3 specific init */
	inst->phy_cfg->phy_init(phy_drd);

	return 0;
}

static void exynos_usbdrd_pipe3_exit(struct exynos_usbdrd_phy *phy_drd)
{
	pr_info("%s : Do nothing...\n", __func__);
}

static void exynos_usbdrd_utmi_exit(struct exynos_usbdrd_phy *phy_drd)
{
	struct phy_usb_instance *inst = &phy_drd->phys[0];

	if (phy_drd->use_phy_umux) {
		/*USB User MUX disable */
		exynos_usbdrd_clk_disable(phy_drd, true);
	}
	phy_exynos_usb_v3p1_disable(&phy_drd->usbphy_info);

	exynos_usbdrd_clk_disable(phy_drd, false);

	inst->phy_cfg->phy_isol(inst, 1, inst->pmu_mask);
}

static int exynos_usbdrd_phy_exit(struct phy *phy)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	/* UTMI or PIPE3 specific exit */
	inst->phy_cfg->phy_exit(phy_drd);

	return 0;
}

static void exynos_usbdrd_pipe3_tune(struct exynos_usbdrd_phy *phy_drd,
							int phy_state)
{
	struct exynos_usb_tune_param *ss_tune_param =
					phy_drd->usbphy_info.ss_tune_param;
	int i = 0;

	if (phy_drd->usb3phy_isolation == 1)
		return;

	exynos_usbdrd_check_connection(phy_drd);

	dev_info(phy_drd->dev, "%s %s %d\n", __func__, ss_tune_param[0].name,
		ss_tune_param[0].value);

	for (i = 0; ss_tune_param[i].value != EXYNOS_USB_TUNE_LAST; i++)
		phy_exynos_usb_v3p1_tune_each(&phy_drd->usbphy_info,
				ss_tune_param[i].name, ss_tune_param[i].value);
}

static void exynos_usbdrd_utmi_tune(struct exynos_usbdrd_phy *phy_drd,
							int phy_state)
{
	struct exynos_usb_tune_param *hs_tune_param = phy_drd->usbphy_info.tune_param;
	int i;

	dev_info(phy_drd->dev, "%s: device=%d\n", __func__, (phy_state >= OTG_STATE_A_IDLE)? 0 : 1);

	if (phy_state >= OTG_STATE_A_IDLE) {
		/* for host mode */
		for (i = 0; hs_tune_param[i].value != EXYNOS_USB_TUNE_LAST; i++) {
			if (i == EXYNOS_DRD_MAX_TUNEPARAM_NUM)
				break;
			hs_tune_param[i].value = phy_drd->hs_tune_param_value[i][USBPHY_MODE_HOST];
		}
	} else {
		/* for device mode */
		for (i = 0; hs_tune_param[i].value != EXYNOS_USB_TUNE_LAST; i++)  {
			if (i == EXYNOS_DRD_MAX_TUNEPARAM_NUM)
				break;
			hs_tune_param[i].value = phy_drd->hs_tune_param_value[i][USBPHY_MODE_DEV];
		}
	}

	phy_exynos_usb_v3p1_tune(&phy_drd->usbphy_info);

	/* USB3P1 CAL code doesn't provide late_enable api */
	/* samsung_exynos_cal_usb3phy_late_enable(&phy_drd->usbphy_info); */
}

static int exynos_usbdrd_phy_tune(struct phy *phy, int phy_state)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	inst->phy_cfg->phy_tune(phy_drd, phy_state);

	return 0;
}

static void exynos_usbdrd_pipe3_set(struct exynos_usbdrd_phy *phy_drd,
						int option, void *info)
{
}

static void exynos_usbdrd_utmi_set(struct exynos_usbdrd_phy *phy_drd,
						int option, void *info)
{
	switch (option) {
	case SET_DPPULLUP_ENABLE:
#if 0
		phy_exynos_usb_v3p1_enable_dp_pullup(
					&phy_drd->usbphy_info);
#endif
		break;
	case SET_DPPULLUP_DISABLE:
#if 0
		phy_exynos_usb_v3p1_disable_dp_pullup(
					&phy_drd->usbphy_info);
#endif
		break;
	case SET_DPDM_PULLDOWN:
		phy_exynos_usb_v3p1_config_host_mode(
					&phy_drd->usbphy_info);
	default:
		break;
	}
}

static int exynos_usbdrd_phy_set(struct phy *phy, int option, void *info)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	inst->phy_cfg->phy_set(phy_drd, option, info);

	return 0;
}

static int exynos_usbdrd_phy_power_on(struct phy *phy)
{
	int ret;
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	dev_dbg(phy_drd->dev, "Request to power_on usbdrd_phy phy\n");

	/* Enable VBUS supply */
	if (phy_drd->vbus) {
		ret = regulator_enable(phy_drd->vbus);
		if (ret) {
			dev_err(phy_drd->dev, "Failed to enable VBUS supply\n");
			return ret;
		}
	}

	inst->phy_cfg->phy_isol(inst, 0, inst->pmu_mask);

	return 0;
}

static int exynos_usbdrd_phy_power_off(struct phy *phy)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	dev_dbg(phy_drd->dev, "Request to power_off usbdrd_phy phy\n");

	/* Disable VBUS supply */
	if (phy_drd->vbus)
		regulator_disable(phy_drd->vbus);

	inst->phy_cfg->phy_isol(inst, 1, inst->pmu_mask);

	return 0;
}

static struct phy *exynos_usbdrd_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_usbdrd_phy *phy_drd = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] > EXYNOS_DRDPHYS_NUM))
		return ERR_PTR(-ENODEV);

	return phy_drd->phys[args->args[0]].phy;
}

static struct phy_ops exynos_usbdrd_phy_ops = {
	.init		= exynos_usbdrd_phy_init,
	.exit		= exynos_usbdrd_phy_exit,
	.tune		= exynos_usbdrd_phy_tune,
	.set		= exynos_usbdrd_phy_set,
	.power_on	= exynos_usbdrd_phy_power_on,
	.power_off	= exynos_usbdrd_phy_power_off,
	.owner		= THIS_MODULE,
};

static const struct exynos_usbdrd_phy_config phy_cfg_exynos[] = {
	{
		.id		= EXYNOS_DRDPHY_UTMI,
		.phy_isol	= exynos_usbdrd_utmi_phy_isol,
		.phy_init	= exynos_usbdrd_utmi_init,
		.phy_exit	= exynos_usbdrd_utmi_exit,
		.phy_tune	= exynos_usbdrd_utmi_tune,
		.phy_set	= exynos_usbdrd_utmi_set,
		.set_refclk	= exynos_usbdrd_utmi_set_refclk,
	},
	{
		.id		= EXYNOS_DRDPHY_PIPE3,
		.phy_isol	= exynos_usbdrd_pipe3_phy_isol,
		.phy_init	= exynos_usbdrd_pipe3_init,
		.phy_exit	= exynos_usbdrd_pipe3_exit,
		.phy_tune	= exynos_usbdrd_pipe3_tune,
		.phy_set	= exynos_usbdrd_pipe3_set,
		.set_refclk	= exynos_usbdrd_pipe3_set_refclk,
	},
};

static const struct exynos_usbdrd_phy_drvdata exynos_usbdrd_phy = {
	.phy_cfg		= phy_cfg_exynos,
};

static const struct of_device_id exynos_usbdrd_phy_of_match[] = {
	{
		.compatible = "samsung,exynos-usbdrd-phy",
		.data = &exynos_usbdrd_phy
	},
	{ },
};
MODULE_DEVICE_TABLE(of, exynos5_usbdrd_phy_of_match);

static int exynos_usbdrd_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_usbdrd_phy *phy_drd;
	struct phy_provider *phy_provider;
	struct resource *res;
	const struct of_device_id *match;
	const struct exynos_usbdrd_phy_drvdata *drv_data;
	struct phy_usb_instance *inst;
	struct regmap *reg_pmu;
	u32 pmu_offset, pmu_mask;
	int i, ret;

	pr_info("%s: +++ %s\n", __func__, pdev->name);
	phy_drd = devm_kzalloc(dev, sizeof(*phy_drd), GFP_KERNEL);
	if (!phy_drd)
		return -ENOMEM;

	dev_set_drvdata(dev, phy_drd);
	phy_drd->dev = dev;

	match = of_match_node(exynos_usbdrd_phy_of_match, pdev->dev.of_node);

	drv_data = match->data;
	phy_drd->drv_data = drv_data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_drd->reg_phy = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy_drd->reg_phy))
		return PTR_ERR(phy_drd->reg_phy);

	ret = exynos_usbdrd_get_iptype(phy_drd);
	if (ret) {
		dev_err(dev, "%s: Failed to get ip_type\n", __func__);
		return ret;
	}

	ret = exynos_usbdrd_clk_get(phy_drd);
	if (ret) {
		dev_err(dev, "%s: Failed to get clocks\n", __func__);
		return ret;
	}

	ret = exynos_usbdrd_clk_prepare(phy_drd);
	if (ret) {
		dev_err(dev, "%s: Failed to prepare clocks\n", __func__);
		return ret;
	}

	reg_pmu = syscon_regmap_lookup_by_phandle(dev->of_node,
						   "samsung,pmu-syscon");
	if (IS_ERR(reg_pmu)) {
		dev_err(dev, "Failed to lookup PMU regmap\n");
		goto err;
	}

	ret = of_property_read_u32(dev->of_node, "pmu_offset", &pmu_offset);
	if (ret < 0) {
		dev_err(dev, "couldn't read pmu_offset on %s node, error = %d\n",
						dev->of_node->name, ret);
		goto err;
	}

	ret = of_property_read_u32(dev->of_node, "pmu_mask", &pmu_mask);
	if (ret < 0) {
		dev_err(dev, "couldn't read pmu_mask on %s node, error = %d\n",
						dev->of_node->name, ret);
		goto err;
	}

	dev_vdbg(dev, "Creating usbdrd_phy phy\n");
	phy_drd->phy_port =  of_get_named_gpio(dev->of_node,
					"phy,gpio_phy_port", 0);
	if (gpio_is_valid(phy_drd->phy_port)) {
		dev_err(dev, "PHY CON Selection OK\n");

		ret = gpio_request(phy_drd->phy_port, "PHY_CON");
		if (ret)
			dev_err(dev, "fail to request gpio %s:%d\n", "PHY_CON", ret);
		else
			gpio_direction_input(phy_drd->phy_port);
	} else {
		dev_err(dev, "non-DT: PHY CON Selection\n");
	}

	if (!of_property_read_u32(dev->of_node, "has_combo_phy", &ret)) {
		if (ret) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
			phy_drd->reg_phy2 = devm_ioremap_resource(dev, res);
			if (IS_ERR(phy_drd->reg_phy2))
				return PTR_ERR(phy_drd->reg_phy2);

			res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
			phy_drd->reg_phy3 = devm_ioremap_resource(dev, res);
			if (IS_ERR(phy_drd->reg_phy3))
				return PTR_ERR(phy_drd->reg_phy3);

			exynos_usbdrd_get_sub_phyinfo(phy_drd);
		} else {
			dev_err(dev, "It has not the other phy\n");
		}
	}
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	exynos_usbdrd_phy_get_otp_info(phy_drd);
#endif

	for (i = 0; i < EXYNOS_DRDPHYS_NUM; i++) {
		struct phy *phy = devm_phy_create(dev, NULL,
						  &exynos_usbdrd_phy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "Failed to create usbdrd_phy phy\n");
			goto err;
		}

		phy_drd->phys[i].phy = phy;
		phy_drd->phys[i].index = i;
		phy_drd->phys[i].reg_pmu = reg_pmu;
		phy_drd->phys[i].pmu_offset = pmu_offset;
		phy_drd->phys[i].pmu_mask = pmu_mask;
		phy_drd->phys[i].phy_cfg = &drv_data->phy_cfg[i];
		phy_set_drvdata(phy, &phy_drd->phys[i]);
	}
#if IS_ENABLED(CONFIG_PHY_EXYNOS_DEBUGFS)
	ret = exynos_usbdrd_debugfs_init(phy_drd);
	if (ret) {
		dev_err(dev, "Failed to initialize debugfs\n");
		goto err;
	}
#endif

	inst = &phy_drd->phys[0];
	inst->phy_cfg->phy_isol(inst, 0, inst->pmu_mask);
	ret = exynos_rate_to_clk(phy_drd);
	if (ret) {
		dev_err(phy_drd->dev, "%s: Not supported ref clock\n",
				__func__);
		goto err;
	}
	inst->phy_cfg->phy_isol(inst, 1, inst->pmu_mask);

	ret = exynos_usbdrd_get_phyinfo(phy_drd);
	if (ret)
		goto err;

	/*
	 * Both has_other_phy and has_combo_phy can't be enabled at the same time.
	 * It's alternative.
	 */
	if (!of_property_read_u32(dev->of_node, "has_other_phy", &ret)) {
		if (ret) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
			phy_drd->reg_phy2 = devm_ioremap_resource(dev, res);
			if (IS_ERR(phy_drd->reg_phy2))
				return PTR_ERR(phy_drd->reg_phy2);

			phy_drd->usbphy_info.regs_base_2nd = phy_drd->reg_phy2;
			phy_drd->usbphy_info.ss_tune_param =
					phy_drd->usbphy_sub_info.tune_param;
		} else {
			dev_err(dev, "It has not the other phy\n");
		}
	}

	phy_provider = devm_of_phy_provider_register(dev,
						     exynos_usbdrd_phy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(phy_drd->dev, "Failed to register phy provider\n");
		goto err;
	}

	pr_info("%s: ---\n", __func__);
	return 0;

err:
	exynos_usbdrd_clk_unprepare(phy_drd);

	return ret;
}

#define EXYNOS_USBDRD_PHY_PM_OPS	NULL

static struct platform_driver phy_exynos_usbdrd = {
	.probe	= exynos_usbdrd_phy_probe,
	.driver = {
		.of_match_table	= exynos_usbdrd_phy_of_match,
		.name		= "phy_exynos_usbdrd",
		.pm		= EXYNOS_USBDRD_PHY_PM_OPS,
	}
};

module_platform_driver(phy_exynos_usbdrd);
MODULE_DESCRIPTION("Samsung EXYNOS SoCs USB DRD controller PHY driver");
MODULE_AUTHOR("Vivek Gautam <gautam.vivek@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:phy_exynos_usbdrd");
