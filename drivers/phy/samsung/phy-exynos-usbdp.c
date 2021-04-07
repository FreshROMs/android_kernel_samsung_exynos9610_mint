/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Sung-Hyun Na <sunghyun.na@samsung.com>
 *
 * Chip Abstraction Layer for USB PHY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include "phy-samsung-usb-cal.h"
#include "phy-exynos-usbdp-reg.h"

void phy_exynos_usbdp_tune_each(struct exynos_usbphy_info *info, char *name,
	int val)
{
	void __iomem *regs_base = info->regs_base;
	u32 reg;

	if (!name)
		return;

	if (!strcmp(name, "sstx_deemph")) {
		reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R0C);
		reg &= ~USBDP_TRSV0C_MAN_TX_DE_EMP_LVL_MASK;
		reg |= USBDP_TRSV0C_MAN_TX_DE_EMP_LVL_SET(val);
		writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R0C);
	} else if (!strcmp(name, "sstx_amp")) {
		reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R0C);
		reg &= ~USBDP_TRSV0C_MAN_TX_DRVR_LVL_MASK;
		reg |= USBDP_TRSV0C_MAN_TX_DRVR_LVL_SET(val);
		writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R0C);
	} else if (!strcmp(name, "ssrx_los")) {
		reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R0A);
		reg &= ~USBDP_TRSV0A_APB_CAL_OFFSET_DIFP_MASK;
		reg |= USBDP_TRSV0A_APB_CAL_OFFSET_DIFP_SET(val);
		writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R0A);

		reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R0B);
		reg &= ~USBDP_TRSV0B_APB_CAL_OFFSET_DIFN_MASK;
		reg |= USBDP_TRSV0B_APB_CAL_OFFSET_DIFN_SET(val);
		writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R0B);
	} else if (!strcmp(name, "ssrx_ctle")) {
		reg = USBDP_TRSV02_RXAFE_LEQ_CSEL_GEN2_SET(0x2);
		reg |= USBDP_TRSV02_RXAFE_LEQ_CSEL_GEN1_SET(0x7);
		writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R02);

		reg = USBDP_TRSV03_RXAFE_TERM_MODE;
		reg &= ~USBDP_TRSV03_RXAFE_LEQ_RSEL_GEN2_MASK;
		reg |= USBDP_TRSV03_RXAFE_LEQ_RSEL_GEN2_SET(0x7);
		reg &= ~USBDP_TRSV03_RXAFE_LEQ_RSEL_GEN1_MASK;
		writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R03);

		reg = USBDP_TRSV04_RXAFE_TUNE_SET(val);
		reg |= USBDP_TRSV04_RXAFE_SQ_VFFSET_CTRL_SET(val);
		writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R04);
	}
}

void phy_exynos_usbdp_tune(struct exynos_usbphy_info *info)
{
	u32 cnt = 0;

	for (; info->tune_param[cnt].value != EXYNOS_USB_TUNE_LAST; cnt++) {
		char *para_name;
		int val;

		val = info->tune_param[cnt].value;
		if (val == -1)
			continue;
		para_name = info->tune_param[cnt].name;
		if (!para_name)
			break;
		phy_exynos_usbdp_tune_each(info, para_name, val);
	}
}

void phy_exynos_usbdp_ilbk(struct exynos_usbphy_info *info)
{

}

void phy_exynos_usbdp_enable(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->regs_base;
	u32 reg, reg_2c, reg_2d, reg_dp_b3;

	/* Recevier Detection Off */
	reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R24);
	reg |= USBDP_TRSV24_MAN_TX_RCV_DET_EN;
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R24);

	/* Set Proper Value for PCS to avoid abnormal RX Detect */
	reg = readl(info->regs_base_2nd + USBDP_PCSREG_FRONT_END_MODE_VEC);
	reg |= USBDP_PCSREG_EN_REALIGN;
	writel(reg, info->regs_base_2nd + USBDP_PCSREG_FRONT_END_MODE_VEC);

	reg = USBDP_PCSREG_COMP_EN_ASSERT_SET(0x3f);
	writel(reg, info->regs_base_2nd + USBDP_PCSREG_DET_COMP_EN_SET);

	reg = 0;
	reg |= USBDP_CMN0E_PLL_AFC_VCO_CNT_RUN_NO_SET(0x4);
	reg |= USBDP_CMN0E_PLL_AFC_ANA_CPI_CTRL_SET(0x2);
	writel(reg, regs_base + EXYNOS_USBDP_COM_CMN_R0E);

	reg = 0;
	reg |= USBDP_CMN0F_PLL_ANA_EN_PI;
	reg |= USBDP_CMN0F_PLL_ANA_DCC_EN_SET(0xf);
	reg |= USBDP_CMN0F_PLL_ANA_CPP_CTRL_SET(0x7);
	writel(reg, regs_base + EXYNOS_USBDP_COM_CMN_R0F);

	reg = 0;
	reg |= USBDP_CMN10_PLL_ANA_VCI_SEL_SET(0x6);
	reg |= USBDP_CMN10_PLL_ANA_LPF_RSEL_SET(0x8);
	writel(reg, regs_base + EXYNOS_USBDP_COM_CMN_R10);

	reg = 0;
	reg |= USBDP_CMN25_PLL_AGMC_TG_CODE_SET(0x30);
	writel(reg, regs_base + EXYNOS_USBDP_COM_CMN_R25);

	reg = 0;
	reg |= USBDP_CMN26_PLL_AGMC_COMP_EN;
	reg |= USBDP_CMN26_PLL_AGMC_FROM_MAX_GM;
	reg |= USBDP_CMN26_PLL_AFC_FROM_PRE_CODE;
	reg |= USBDP_CMN26_PLL_AFC_MAN_BSEL_L_SET(0x3);
	writel(reg, regs_base + EXYNOS_USBDP_COM_CMN_R26);

	reg = 0;
	reg |= USBDP_CMN27_PLL_ANA_LC_VREF_BYPASS_EN;
	reg |= USBDP_CMN27_PLL_ANA_LC_VCDO_CAP_OFFSET_SEL_SET(0x5);
	reg |= USBDP_CMN27_PLL_ANA_LC_VCO_BUFF_EN;
	reg |= USBDP_CMN27_PLL_ANA_LC_GM_COMP_VCI_SEL_SET(0x4);
	writel(reg, regs_base + EXYNOS_USBDP_COM_CMN_R27);

	reg = 0;
	reg |= USBDP_TRSV23_DATA_CLEAR_BY_SIGVAL;
	reg |= USBDP_TRSV23_FBB_H_BW_DIFF_SET(0x5);
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R23);

	/* SSC Setting */
	reg = readl(regs_base + EXYNOS_USBDP_COM_CMN_R24);
	reg |= USBDP_CMN24_SSC_EN;
	writel(reg, regs_base + EXYNOS_USBDP_COM_CMN_R24);

	reg_2c = readl(regs_base + EXYNOS_USBDP_COM_CMN_R2C);
	reg_2d = readl(regs_base + EXYNOS_USBDP_COM_CMN_R2D);
	reg_dp_b3 = readl(regs_base + EXYNOS_USBDP_COM_DP_RB3);
	if (info->used_phy_port == 0) {
		reg_2c |= USBDP_CMN2C_MAN_USBDP_MODE_SET(0x3);
		reg_2d |= USBDP_CMN2D_USB_TX1_SEL;
		reg_2d &= ~USBDP_CMN2D_USB_TX3_SEL;
		reg_dp_b3 &= ~USBDP_DPB3_CMN_DUMMY_CTRL_7;
		reg_dp_b3 |= USBDP_DPB3_CMN_DUMMY_CTRL_6;
		reg_dp_b3 |= USBDP_DPB3_CMN_DUMMY_CTRL_1;
		reg_dp_b3 |= USBDP_DPB3_CMN_DUMMY_CTRL_0;
	} else {
		reg_2c &= ~USBDP_CMN2C_MAN_USBDP_MODE_MASK;
		reg_2d &= ~USBDP_CMN2D_USB_TX1_SEL;
		reg_2d |= USBDP_CMN2D_USB_TX3_SEL;
		reg_dp_b3 |= USBDP_DPB3_CMN_DUMMY_CTRL_7;
		reg_dp_b3 &= ~USBDP_DPB3_CMN_DUMMY_CTRL_6;
		reg_dp_b3 &= ~USBDP_DPB3_CMN_DUMMY_CTRL_1;
		reg_dp_b3 &= ~USBDP_DPB3_CMN_DUMMY_CTRL_0;
	}
	reg_2c |= USBDP_CMN2C_MAN_USBDP_MODE_EN;
	reg_2d &= ~USBDP_CMN2D_LCPLL_SSCDIV_MASK;
	reg_2d |= USBDP_CMN2D_LCPLL_SSCDIV_SET(0x1);
	writel(reg_2c, regs_base + EXYNOS_USBDP_COM_CMN_R2C);
	writel(reg_2d, regs_base + EXYNOS_USBDP_COM_CMN_R2D);
	writel(reg_dp_b3,  regs_base + EXYNOS_USBDP_COM_DP_RB3);

	reg = 0;
	reg |= USBDP_TRSV38_SFR_RX_LFPS_LPF_CTRL_SET(0x2);
	reg |= USBDP_TRSV38_SFR_RX_LFPS_TH_CTRL_SET(0x2);
	reg |= USBDP_TRSV38_RX_LFPS_DET_EN;
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R38);

	/* RX EQ tuning */
	reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R01);
	reg &= ~USBDP_TRSV01_RXAFE_CTLE_SEL_MASK;
	reg |= USBDP_TRSV01_RXAFE_CTLE_SEL_SET(0x3);
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R01);

	reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R03);
	reg &= ~USBDP_TRSV03_RXAFE_LEQ_RSEL_GEN1_MASK;
	reg &= ~USBDP_TRSV03_RXAFE_LEQ_RSEL_GEN2_MASK;
	reg |= USBDP_TRSV03_RXAFE_LEQ_RSEL_GEN1_SET(0x0);
	reg |= USBDP_TRSV03_RXAFE_LEQ_RSEL_GEN2_SET(0x5);
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R03);

	reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R0D);
	reg |= USBDP_TRSV0D_MAN_DRVR_DE_EMP_LVL_MAN_EN;
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R0D);

	/* Sys Valid Debouncd Digital Filter */
	reg = 0;
	reg |= USBDP_TRSV34_INT_SIGVAL_FILT_SEL;
	reg |= USBDP_TRSV34_OUT_SIGVAL_FILT_SEL;
	reg |= USBDP_TRSV34_SIGVAL_FILT_DLY_CODE_SET(0x3);
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R34);

	reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R27);
	reg |= USBDP_TRSV27_MAN_RSTN_EN;
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R27);

	/* Recevier Detection On */
	reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R24);
	reg &= ~USBDP_TRSV24_MAN_TX_RCV_DET_EN;
	writel(reg, regs_base + EXYNOS_USBDP_COM_TRSV_R24);

	/* Set Tune Value */
	phy_exynos_usbdp_tune(info);
}

int phy_exynos_usbdp_check_pll_lock(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->regs_base;
	u32 reg;

	reg = readl(regs_base + EXYNOS_USBDP_COM_CMN_R2F);
	printk("CMN_2F(0x%p) : 0x%x\n",
		regs_base + EXYNOS_USBDP_COM_CMN_R2F,
		reg);
	if (!(reg & USBDP_CMN2F_PLL_LOCK_DONE))
		return -1;

	reg = readl(regs_base + EXYNOS_USBDP_COM_TRSV_R4B);
	printk("TRSV_4B(0x%p) : 0x%x\n",
		regs_base + EXYNOS_USBDP_COM_TRSV_R4B,
		reg);
	if (!(reg & USBDP_TRSV4B_CDR_FLD_PLL_MODE_DONE))
		return -1;

	return 0;
}

void phy_exynos_usbdp_disable(struct exynos_usbphy_info *info)
{

}
