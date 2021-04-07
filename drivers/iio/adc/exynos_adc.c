/*
 *  exynos_adc.c - Support for ADC in EXYNOS SoCs
 *
 *  8 ~ 10 channel, 10/12-bit ADC
 *
 *  Copyright (C) 2013 Naveen Krishna Chatradhi <ch.naveen@samsung.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/input.h>

#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>


/* Semaphore for peterson algorithm  */
#define AP_TURN 0
#define CHUB_TURN 1

#define AP_FLAG_OFFSET		(0x3100)
#define CHUB_FLAG_OFFSET	(0x3104)
#define INIT_TURN_OFFSET	(0x3108)

/* S3C/EXYNOS4412/5250 ADC_V1 registers definitions */
#define ADC_V1_CON(x)		((x) + 0x00)
#define ADC_V1_TSC(x)		((x) + 0x04)
#define ADC_V1_DLY(x)		((x) + 0x08)
#define ADC_V1_DATX(x)		((x) + 0x0C)
#define ADC_V1_DATY(x)		((x) + 0x10)
#define ADC_V1_UPDN(x)		((x) + 0x14)
#define ADC_V1_INTCLR(x)	((x) + 0x18)
#define ADC_V1_MUX(x)		((x) + 0x1c)
#define ADC_V1_CLRINTPNDNUP(x)	((x) + 0x20)

/* S3C2410 ADC registers definitions */
#define ADC_S3C2410_MUX(x)	((x) + 0x18)

/* Future ADC_V2 registers definitions */
#define ADC_V2_CON1(x)		((x) + 0x00)
#define ADC_V2_CON2(x)		((x) + 0x04)
#define ADC_V2_STAT(x)		((x) + 0x08)
#define ADC_V2_INT_EN(x)	((x) + 0x10)
#define ADC_V2_INT_ST(x)	((x) + 0x14)
#define ADC_V2_VER(x)		((x) + 0x20)

/* Sharing ADC_V3 registers definitions */
#define ADC_V3_DAT(x)		((x) + 0x08)
#define ADC_V3_DAT_SUM(x)	((x) + 0x0C)
#define ADC_V3_DBG_DATA(x)	((x) + 0x1C)

/* Bit definitions for ADC_V1 */
#define ADC_V1_CON_RES		(1u << 16)
#define ADC_V1_CON_PRSCEN	(1u << 14)
#define ADC_V1_CON_PRSCLV(x)	(((x) & 0xFF) << 6)
#define ADC_V1_CON_STANDBY	(1u << 2)

/* Bit definitions for S3C2410 ADC */
#define ADC_S3C2410_CON_SELMUX(x) (((x) & 7) << 3)
#define ADC_S3C2410_DATX_MASK	0x3FF
#define ADC_S3C2416_CON_RES_SEL	(1u << 3)

/* touch screen always uses channel 0 */
#define ADC_S3C2410_MUX_TS	0

/* ADCTSC Register Bits */
#define ADC_S3C2443_TSC_UD_SEN		(1u << 8)
#define ADC_S3C2410_TSC_YM_SEN		(1u << 7)
#define ADC_S3C2410_TSC_YP_SEN		(1u << 6)
#define ADC_S3C2410_TSC_XM_SEN		(1u << 5)
#define ADC_S3C2410_TSC_XP_SEN		(1u << 4)
#define ADC_S3C2410_TSC_PULL_UP_DISABLE	(1u << 3)
#define ADC_S3C2410_TSC_AUTO_PST	(1u << 2)
#define ADC_S3C2410_TSC_XY_PST(x)	(((x) & 0x3) << 0)

#define ADC_TSC_WAIT4INT (ADC_S3C2410_TSC_YM_SEN | \
			 ADC_S3C2410_TSC_YP_SEN | \
			 ADC_S3C2410_TSC_XP_SEN | \
			 ADC_S3C2410_TSC_XY_PST(3))

#define ADC_TSC_AUTOPST	(ADC_S3C2410_TSC_YM_SEN | \
			 ADC_S3C2410_TSC_YP_SEN | \
			 ADC_S3C2410_TSC_XP_SEN | \
			 ADC_S3C2410_TSC_AUTO_PST | \
			 ADC_S3C2410_TSC_XY_PST(0))

/* Bit definitions for ADC_V2 */
#define ADC_V2_CON1_SOFT_RESET	(1u << 2)
#define ADC_V2_CON1_SOFT_NON_RESET	(1u << 1)

#define ADC_V2_CON2_OSEL	(1u << 10)
#define ADC_V2_CON2_ESEL	(1u << 9)
#define ADC_V2_CON2_HIGHF	(1u << 8)
#define ADC_V2_CON2_C_TIME(x)	(((x) & 7) << 4)
#define ADC_V2_CON2_ACH_SEL(x)	(((x) & 0xF) << 0)
#define ADC_V2_CON2_ACH_MASK	0xF

/* Bit definitions for ADC_V3 */
#define ADC_V3_DAT_FLAG		(1u << 31)

#define MAX_ADC_V3_CHANNELS		12
#define MAX_ADC_V2_CHANNELS		10
#define MAX_ADC_V1_CHANNELS		8
#define MAX_EXYNOS3250_ADC_CHANNELS	2

/* Bit definitions common for ADC_V1, ADC_V2, ADC_V3 */
#define ADC_CON_EN_START	(1u << 0)
#define ADC_DATX_MASK		0xFFF
#define ADC_DATY_MASK		0xFFF

#define EXYNOS_ADC_TIMEOUT	(msecs_to_jiffies(100))

#define EXYNOS_ADCV1_PHY_OFFSET	0x0718
#define EXYNOS_ADCV2_PHY_OFFSET	0x0720

struct exynos_adc {
	struct exynos_adc_data	*data;
	struct device		*dev;
	struct input_dev	*input;
	void __iomem		*regs;
	void __iomem		*cmgp_sysreg;
	struct regmap		*pmu_map;
	struct clk		*clk;
	struct clk		*sclk;
	unsigned int		irq;
	unsigned int		tsirq;
	unsigned int		delay;
	struct regulator	*vdd;
	bool 			needs_adc_phy;

	struct completion	completion;

	u32			value;
	unsigned int            version;
	int			idle_ip_index;
};

struct exynos_adc_data {
	int num_channels;
	bool needs_sclk;
	int phy_offset;
	u32 mask;

	void (*init_hw)(struct exynos_adc *info);
	void (*exit_hw)(struct exynos_adc *info);
	void (*clear_irq)(struct exynos_adc *info);
	void (*start_conv)(struct exynos_adc *info, unsigned long addr);
	irqreturn_t (*adc_isr)(int irq, void *dev_id);
};

static void exynos_adc_unprepare_clk(struct exynos_adc *info)
{
	if (info->data->needs_sclk)
		clk_unprepare(info->sclk);
	clk_unprepare(info->clk);
}

static int exynos_adc_prepare_clk(struct exynos_adc *info)
{
	int ret;

	ret = clk_prepare(info->clk);
	if (ret) {
		dev_err(info->dev, "failed preparing adc clock: %d\n", ret);
		return ret;
	}

	if (info->data->needs_sclk) {
		ret = clk_prepare(info->sclk);
		if (ret) {
			clk_unprepare(info->clk);
			dev_err(info->dev,
				"failed preparing sclk_adc clock: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static void exynos_adc_disable_clk(struct exynos_adc *info)
{
	if (info->data->needs_sclk)
		clk_disable(info->sclk);
	clk_disable(info->clk);
}

static int exynos_adc_enable_clk(struct exynos_adc *info)
{
	int ret;

	ret = clk_enable(info->clk);
	if (ret) {
		dev_err(info->dev, "failed enabling adc clock: %d\n", ret);
		return ret;
	}

	if (info->data->needs_sclk) {
		ret = clk_enable(info->sclk);
		if (ret) {
			clk_disable(info->clk);
			dev_err(info->dev,
				"failed enabling sclk_adc clock: %d\n", ret);
			return ret;
		}
	}

	return 0;
}
static void exynos_adc_update_ip_idle_status(struct exynos_adc *info, int idle)
{
#ifdef CONFIG_ARCH_EXYNOS_PM
	exynos_update_ip_idle_status(info->idle_ip_index, idle);
#endif
}
static int exynos_adc_enable_access(struct exynos_adc *info)
{
	int ret;

	exynos_adc_update_ip_idle_status(info, 0);
	if (info->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 1);

	if (info->vdd) {
		ret = regulator_enable(info->vdd);
		if (ret)
			goto err;
	}

	ret = exynos_adc_prepare_clk(info);
	if (ret)
		goto err_disable_reg;

	ret = exynos_adc_enable_clk(info);
	if (ret)
		goto err_unprepare_clk;;

	return 0;

err_unprepare_clk:
	exynos_adc_unprepare_clk(info);
err_disable_reg:
	if (info->vdd)
		regulator_disable(info->vdd);
err:
	if (info->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 0);

	exynos_adc_update_ip_idle_status(info, 1);
	return ret;
}

static void exynos_adc_disable_access(struct exynos_adc *info)
{
	exynos_adc_disable_clk(info);
	exynos_adc_unprepare_clk(info);
	if (info->vdd)
		regulator_disable(info->vdd);

	if (info->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 0);
	exynos_adc_update_ip_idle_status(info, 1);
}

static void exynos_adc_v1_init_hw(struct exynos_adc *info)
{
	u32 con1;

	/* set default prescaler values and Enable prescaler */
	con1 =  ADC_V1_CON_PRSCLV(49) | ADC_V1_CON_PRSCEN;

	/* Enable 12-bit ADC resolution */
	con1 |= ADC_V1_CON_RES;
	writel(con1, ADC_V1_CON(info->regs));

	/* set touchscreen delay */
	writel(info->delay, ADC_V1_DLY(info->regs));
}

static void exynos_adc_v1_exit_hw(struct exynos_adc *info)
{
	u32 con;

	con = readl(ADC_V1_CON(info->regs));
	con |= ADC_V1_CON_STANDBY;
	writel(con, ADC_V1_CON(info->regs));
}

static void exynos_adc_v1_clear_irq(struct exynos_adc *info)
{
	writel(1, ADC_V1_INTCLR(info->regs));
}

static void exynos_adc_v1_start_conv(struct exynos_adc *info,
				     unsigned long addr)
{
	u32 con1;

	writel(addr, ADC_V1_MUX(info->regs));

	con1 = readl(ADC_V1_CON(info->regs));
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

static irqreturn_t exynos_adc_v1_isr(int irq, void *dev_id)
{
	struct exynos_adc *info = (struct exynos_adc *)dev_id;
	u32 mask = info->data->mask;

	/* Read value */
	info->value = readl(ADC_V1_DATX(info->regs)) & mask;

	/* clear irq */
	if (info->data->clear_irq)
		info->data->clear_irq(info);

	complete(&info->completion);

	return IRQ_HANDLED;
}

static const struct exynos_adc_data exynos_adc_v1_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */
	.phy_offset	= EXYNOS_ADCV1_PHY_OFFSET,

	.init_hw	= exynos_adc_v1_init_hw,
	.exit_hw	= exynos_adc_v1_exit_hw,
	.clear_irq	= exynos_adc_v1_clear_irq,
	.start_conv	= exynos_adc_v1_start_conv,
	.adc_isr	= exynos_adc_v1_isr,
};

static void exynos_adc_s3c2416_start_conv(struct exynos_adc *info,
					  unsigned long addr)
{
	u32 con1;

	/* Enable 12 bit ADC resolution */
	con1 = readl(ADC_V1_CON(info->regs));
	con1 |= ADC_S3C2416_CON_RES_SEL;
	writel(con1, ADC_V1_CON(info->regs));

	/* Select channel for S3C2416 */
	writel(addr, ADC_S3C2410_MUX(info->regs));

	con1 = readl(ADC_V1_CON(info->regs));
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

static struct exynos_adc_data const exynos_adc_s3c2416_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,
	.exit_hw	= exynos_adc_v1_exit_hw,
	.start_conv	= exynos_adc_s3c2416_start_conv,
	.adc_isr	= exynos_adc_v1_isr,
};

static void exynos_adc_s3c2443_start_conv(struct exynos_adc *info,
					  unsigned long addr)
{
	u32 con1;

	/* Select channel for S3C2433 */
	writel(addr, ADC_S3C2410_MUX(info->regs));

	con1 = readl(ADC_V1_CON(info->regs));
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

static struct exynos_adc_data const exynos_adc_s3c2443_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_S3C2410_DATX_MASK, /* 10 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,
	.exit_hw	= exynos_adc_v1_exit_hw,
	.start_conv	= exynos_adc_s3c2443_start_conv,
	.adc_isr	= exynos_adc_v1_isr,
};

static void exynos_adc_s3c64xx_start_conv(struct exynos_adc *info,
					  unsigned long addr)
{
	u32 con1;

	con1 = readl(ADC_V1_CON(info->regs));
	con1 &= ~ADC_S3C2410_CON_SELMUX(0x7);
	con1 |= ADC_S3C2410_CON_SELMUX(addr);
	writel(con1 | ADC_CON_EN_START, ADC_V1_CON(info->regs));
}

static struct exynos_adc_data const exynos_adc_s3c24xx_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_S3C2410_DATX_MASK, /* 10 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,
	.exit_hw	= exynos_adc_v1_exit_hw,
	.start_conv	= exynos_adc_s3c64xx_start_conv,
	.adc_isr	= exynos_adc_v1_isr,
};

static struct exynos_adc_data const exynos_adc_s3c64xx_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK,	/* 12 bit ADC resolution */

	.init_hw	= exynos_adc_v1_init_hw,
	.exit_hw	= exynos_adc_v1_exit_hw,
	.clear_irq	= exynos_adc_v1_clear_irq,
	.start_conv	= exynos_adc_s3c64xx_start_conv,
	.adc_isr	= exynos_adc_v1_isr,
};

static void exynos_adc_v2_init_hw(struct exynos_adc *info)
{
	u32 con1, con2;

	con1 = ADC_V2_CON1_SOFT_RESET;
	writel(con1, ADC_V2_CON1(info->regs));

	con2 = ADC_V2_CON2_OSEL | ADC_V2_CON2_ESEL |
		ADC_V2_CON2_HIGHF | ADC_V2_CON2_C_TIME(6);
	writel(con2, ADC_V2_CON2(info->regs));

	/* Enable interrupts */
	writel(1, ADC_V2_INT_EN(info->regs));
}

static void exynos_adc_v2_exit_hw(struct exynos_adc *info)
{
	u32 con2;

	con2 = readl(ADC_V2_CON2(info->regs));
	con2 &= ~(ADC_V2_CON2_OSEL | ADC_V2_CON2_ESEL |
		ADC_V2_CON2_HIGHF | ADC_V2_CON2_C_TIME(7));
	writel(con2, ADC_V2_CON2(info->regs));

	/* Disable interrupts */
	writel(0, ADC_V2_INT_EN(info->regs));
}

static void exynos_adc_v2_clear_irq(struct exynos_adc *info)
{
	writel(1, ADC_V2_INT_ST(info->regs));
}

static void exynos_adc_v2_start_conv(struct exynos_adc *info,
				     unsigned long addr)
{
	u32 con1, con2;

	con2 = readl(ADC_V2_CON2(info->regs));
	con2 &= ~ADC_V2_CON2_ACH_MASK;
	con2 |= ADC_V2_CON2_ACH_SEL(addr);
	writel(con2, ADC_V2_CON2(info->regs));

	con1 = readl(ADC_V2_CON1(info->regs));
	writel(con1 | ADC_CON_EN_START, ADC_V2_CON1(info->regs));
}

static const struct exynos_adc_data exynos_adc_v2_data = {
	.num_channels	= MAX_ADC_V2_CHANNELS,
	.mask		= ADC_DATX_MASK, /* 12 bit ADC resolution */
	.phy_offset	= EXYNOS_ADCV2_PHY_OFFSET,

	.init_hw	= exynos_adc_v2_init_hw,
	.exit_hw	= exynos_adc_v2_exit_hw,
	.clear_irq	= exynos_adc_v2_clear_irq,
	.start_conv	= exynos_adc_v2_start_conv,
	.adc_isr	= exynos_adc_v1_isr,
};

static const struct exynos_adc_data exynos3250_adc_data = {
	.num_channels	= MAX_EXYNOS3250_ADC_CHANNELS,
	.mask		= ADC_DATX_MASK, /* 12 bit ADC resolution */
	.needs_sclk	= true,
	.phy_offset	= EXYNOS_ADCV1_PHY_OFFSET,

	.init_hw	= exynos_adc_v2_init_hw,
	.exit_hw	= exynos_adc_v2_exit_hw,
	.clear_irq	= exynos_adc_v2_clear_irq,
	.start_conv	= exynos_adc_v2_start_conv,
	.adc_isr	= exynos_adc_v1_isr,
};

static void exynos_adc_v3_init_hw(struct exynos_adc *info)
{
	u32 con1, con2;

	con1 = ADC_V2_CON1_SOFT_RESET;
	writel(con1, ADC_V2_CON1(info->regs));

	con1 = ADC_V2_CON1_SOFT_NON_RESET;
	writel(con1, ADC_V2_CON1(info->regs));

	con2 = ADC_V2_CON2_C_TIME(6);
	writel(con2, ADC_V2_CON2(info->regs));

	/* Enable interrupts */
	writel(1, ADC_V2_INT_EN(info->regs));
}

static void exynos_adc_v3_exit_hw(struct exynos_adc *info)
{
	u32 con2;

	con2 = readl(ADC_V2_CON2(info->regs));
	con2 &= ~ADC_V2_CON2_C_TIME(7);
	writel(con2, ADC_V2_CON2(info->regs));

	/* Disable interrupts */
	writel(0, ADC_V2_INT_EN(info->regs));
}

static irqreturn_t exynos_adc_v3_isr(int irq, void *dev_id)
{
	struct exynos_adc *info = (struct exynos_adc *)dev_id;
	u32 mask = info->data->mask;

	/* Read value */
	info->value = readl(ADC_V3_DAT(info->regs)) & mask;

	/* clear irq */
	if (info->data->clear_irq)
		info->data->clear_irq(info);

	complete(&info->completion);

	return IRQ_HANDLED;
}

static const struct exynos_adc_data exynos_adc_v3_data = {
	.num_channels	= MAX_ADC_V3_CHANNELS,
	.mask		= ADC_DATX_MASK, /* 12 bit ADC resolution */

	.init_hw	= exynos_adc_v3_init_hw,
	.exit_hw	= exynos_adc_v3_exit_hw,
	.clear_irq	= exynos_adc_v2_clear_irq,
	.start_conv	= exynos_adc_v2_start_conv,
	.adc_isr	= exynos_adc_v3_isr,
};

static void exynos_adc_exynos7_init_hw(struct exynos_adc *info)
{
	u32 con1, con2;

	if (info->needs_adc_phy)
		regmap_write(info->pmu_map, info->data->phy_offset, 1);

	con1 = ADC_V2_CON1_SOFT_RESET;
	writel(con1, ADC_V2_CON1(info->regs));

	con2 = readl(ADC_V2_CON2(info->regs));
	con2 &= ~ADC_V2_CON2_C_TIME(7);
	con2 |= ADC_V2_CON2_C_TIME(0);
	writel(con2, ADC_V2_CON2(info->regs));

	/* Enable interrupts */
	writel(1, ADC_V2_INT_EN(info->regs));
}

static const struct exynos_adc_data exynos7_adc_data = {
	.num_channels	= MAX_ADC_V1_CHANNELS,
	.mask		= ADC_DATX_MASK, /* 12 bit ADC resolution */

	.init_hw	= exynos_adc_exynos7_init_hw,
	.exit_hw	= exynos_adc_v2_exit_hw,
	.clear_irq	= exynos_adc_v2_clear_irq,
	.start_conv	= exynos_adc_v2_start_conv,
};

static const struct of_device_id exynos_adc_match[] = {
	{
		.compatible = "samsung,s3c2410-adc",
		.data = &exynos_adc_s3c24xx_data,
	}, {
		.compatible = "samsung,s3c2416-adc",
		.data = &exynos_adc_s3c2416_data,
	}, {
		.compatible = "samsung,s3c2440-adc",
		.data = &exynos_adc_s3c24xx_data,
	}, {
		.compatible = "samsung,s3c2443-adc",
		.data = &exynos_adc_s3c2443_data,
	}, {
		.compatible = "samsung,s3c6410-adc",
		.data = &exynos_adc_s3c64xx_data,
	}, {
		.compatible = "samsung,exynos-adc-v1",
		.data = &exynos_adc_v1_data,
	}, {
		.compatible = "samsung,exynos-adc-v2",
		.data = &exynos_adc_v2_data,
	}, {
		.compatible = "samsung,exynos-adc-v3",
		.data = &exynos_adc_v3_data,
	}, {
		.compatible = "samsung,exynos3250-adc",
		.data = &exynos3250_adc_data,
	}, {
		.compatible = "samsung,exynos7-adc",
		.data = &exynos7_adc_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_adc_match);

static struct exynos_adc_data *exynos_adc_get_data(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_node(exynos_adc_match, pdev->dev.of_node);
	return (struct exynos_adc_data *)match->data;
}

static int exynos_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val,
				int *val2,
				long mask)
{
	struct exynos_adc *info = iio_priv(indio_dev);
	unsigned long timeout, sysreg_timeout = 0;
	int ret;

	if (info->cmgp_sysreg) {
		__raw_writel(true, info->cmgp_sysreg + AP_FLAG_OFFSET);
		__raw_writel(CHUB_TURN,	info->cmgp_sysreg + INIT_TURN_OFFSET);
		while ((__raw_readl(info->cmgp_sysreg + CHUB_FLAG_OFFSET) == true)
				&& (__raw_readl(info->cmgp_sysreg + INIT_TURN_OFFSET) == CHUB_TURN)
				&& (sysreg_timeout < 100)) {
			/* If CHUB does not use ADC or timeout value is 100, this loop end. */
			mdelay(1);
			sysreg_timeout++;
		}
		if (sysreg_timeout >= 100) {
			dev_err(&indio_dev->dev, "Time out! Adc is currently being used by CHUB.\n");
			return -ETIMEDOUT;
		}
	}

	/* critical section start */
	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	reinit_completion(&info->completion);

	ret = exynos_adc_enable_access(info);
	if (ret)
		goto err_unlock;

	enable_irq(info->irq);
	if (info->data->init_hw)
		info->data->init_hw(info);

	/* Select the channel to be used and Trigger conversion */
	if (info->data->start_conv)
		info->data->start_conv(info, chan->address);

	timeout = wait_for_completion_timeout(&info->completion,
					      EXYNOS_ADC_TIMEOUT);
	if (timeout == 0) {
		dev_warn(&indio_dev->dev, "Conversion timed out! Resetting\n");
		ret = -ETIMEDOUT;
	} else {
		*val = info->value;
		*val2 = 0;
		ret = IIO_VAL_INT;
	}

	if (info->data->exit_hw)
		info->data->exit_hw(info);

	disable_irq(info->irq);
	exynos_adc_disable_access(info);
err_unlock:
	mutex_unlock(&indio_dev->mlock);


	/* critical section end */
	if (info->cmgp_sysreg)
		__raw_writel(false, info->cmgp_sysreg + AP_FLAG_OFFSET);

	return ret;
}

#ifdef ADC_TS
static int exynos_read_s3c64xx_ts(struct iio_dev *indio_dev, int *x, int *y)
{
	struct exynos_adc *info = iio_priv(indio_dev);
	unsigned long timeout;
	int ret;

	mutex_lock(&indio_dev->mlock);
	info->read_ts = true;

	reinit_completion(&info->completion);

	writel(ADC_S3C2410_TSC_PULL_UP_DISABLE | ADC_TSC_AUTOPST,
	       ADC_V1_TSC(info->regs));

	/* Select the ts channel to be used and Trigger conversion */
	info->data->start_conv(info, ADC_S3C2410_MUX_TS);

	timeout = wait_for_completion_timeout(&info->completion,
					      EXYNOS_ADC_TIMEOUT);
	if (timeout == 0) {
		dev_warn(&indio_dev->dev, "Conversion timed out! Resetting\n");
		if (info->data->init_hw)
			info->data->init_hw(info);
		ret = -ETIMEDOUT;
	} else {
		*x = info->ts_x;
		*y = info->ts_y;
		ret = 0;
	}

	info->read_ts = false;
	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static irqreturn_t exynos_adc_isr(int irq, void *dev_id)
{
	struct exynos_adc *info = dev_id;
	u32 mask = info->data->mask;

	/* Read value */
	if (info->read_ts) {
		info->ts_x = readl(ADC_V1_DATX(info->regs));
		info->ts_y = readl(ADC_V1_DATY(info->regs));
		writel(ADC_TSC_WAIT4INT | ADC_S3C2443_TSC_UD_SEN, ADC_V1_TSC(info->regs));
	} else {
		info->value = readl(ADC_V1_DATX(info->regs)) & mask;
	}

	/* clear irq */
	if (info->data->clear_irq)
		info->data->clear_irq(info);

	complete(&info->completion);

	return IRQ_HANDLED;
}

/*
 * Here we (ab)use a threaded interrupt handler to stay running
 * for as long as the touchscreen remains pressed, we report
 * a new event with the latest data and then sleep until the
 * next timer tick. This mirrors the behavior of the old
 * driver, with much less code.
 */
static irqreturn_t exynos_ts_isr(int irq, void *dev_id)
{
	struct exynos_adc *info = dev_id;
	struct iio_dev *dev = dev_get_drvdata(info->dev);
	u32 x, y;
	bool pressed;
	int ret;

	while (info->input->users) {
		ret = exynos_read_s3c64xx_ts(dev, &x, &y);
		if (ret == -ETIMEDOUT)
			break;

		pressed = x & y & ADC_DATX_PRESSED;
		if (!pressed) {
			input_report_key(info->input, BTN_TOUCH, 0);
			input_sync(info->input);
			break;
		}

		input_report_abs(info->input, ABS_X, x & ADC_DATX_MASK);
		input_report_abs(info->input, ABS_Y, y & ADC_DATY_MASK);
		input_report_key(info->input, BTN_TOUCH, 1);
		input_sync(info->input);

		usleep_range(1000, 1100);
	};

	writel(0, ADC_V1_CLRINTPNDNUP(info->regs));

	return IRQ_HANDLED;
}
#endif

static int exynos_adc_reg_access(struct iio_dev *indio_dev,
			      unsigned reg, unsigned writeval,
			      unsigned *readval)
{
	struct exynos_adc *info = iio_priv(indio_dev);
	int ret;

	if (readval == NULL)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	ret = exynos_adc_enable_access(info);
	if (ret)
		goto err_unlock;

	*readval = readl(info->regs + reg);

	exynos_adc_disable_access(info);
err_unlock:
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static const struct iio_info exynos_adc_iio_info = {
	.read_raw = &exynos_read_raw,
	.debugfs_reg_access = &exynos_adc_reg_access,
	.driver_module = THIS_MODULE,
};

#define ADC_CHANNEL(_index, _id) {			\
	.type = IIO_VOLTAGE,				\
	.indexed = 1,					\
	.channel = _index,				\
	.address = _index,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
	.datasheet_name = _id,				\
}

static const struct iio_chan_spec exynos_adc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),
	ADC_CHANNEL(1, "adc1"),
	ADC_CHANNEL(2, "adc2"),
	ADC_CHANNEL(3, "adc3"),
	ADC_CHANNEL(4, "adc4"),
	ADC_CHANNEL(5, "adc5"),
	ADC_CHANNEL(6, "adc6"),
	ADC_CHANNEL(7, "adc7"),
	ADC_CHANNEL(8, "adc8"),
	ADC_CHANNEL(9, "adc9"),
	ADC_CHANNEL(10, "adc10"),
	ADC_CHANNEL(11, "adc11"),
};

static int exynos_adc_remove_devices(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

#ifdef ADC_TS
static int exynos_adc_ts_open(struct input_dev *dev)
{
	struct exynos_adc *info = input_get_drvdata(dev);

	enable_irq(info->tsirq);

	return 0;
}

static void exynos_adc_ts_close(struct input_dev *dev)
{
	struct exynos_adc *info = input_get_drvdata(dev);

	disable_irq(info->tsirq);
}

static int exynos_adc_ts_init(struct exynos_adc *info)
{
	int ret;

	if (info->tsirq <= 0)
		return -ENODEV;

	info->input = input_allocate_device();
	if (!info->input)
		return -ENOMEM;

	info->input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	info->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(info->input, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(info->input, ABS_Y, 0, 0x3FF, 0, 0);

	info->input->name = "S3C24xx TouchScreen";
	info->input->id.bustype = BUS_HOST;
	info->input->open = exynos_adc_ts_open;
	info->input->close = exynos_adc_ts_close;

	input_set_drvdata(info->input, info);

	ret = input_register_device(info->input);
	if (ret) {
		input_free_device(info->input);
		return ret;
	}

	disable_irq(info->tsirq);
	ret = request_threaded_irq(info->tsirq, NULL, exynos_ts_isr,
				   IRQF_ONESHOT, "touchscreen", info);
	if (ret)
		input_unregister_device(info->input);

	return ret;
}
#endif

static int exynos_adc_probe(struct platform_device *pdev)
{
	struct exynos_adc *info = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct iio_dev *indio_dev = NULL;
	struct resource	*mem;
	int ret = -ENODEV;
	int irq;
	unsigned int sysreg;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct exynos_adc));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);

	info->data = exynos_adc_get_data(pdev);
	if (!info->data) {
		dev_err(&pdev->dev, "failed getting exynos_adc_data\n");
		return -EINVAL;
	}

	if (of_find_property(np, "samsung,adc-phy-control", NULL))
		info->needs_adc_phy = true;
	else
		info->needs_adc_phy = false;

	if (of_property_read_u32(np, "sysreg", &sysreg)) {
		dev_info(&pdev->dev, "Do not use Peterson algorithm\n");
		info->cmgp_sysreg = NULL;
	} else {
		info->cmgp_sysreg = devm_ioremap(&pdev->dev, sysreg, SZ_64K);
		if (IS_ERR(info->cmgp_sysreg)) {
			dev_err(&pdev->dev, "Failed devm_ioremap\n");
			return PTR_ERR(info->cmgp_sysreg);
		}
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(info->regs))
		return PTR_ERR(info->regs);


	if (info->needs_adc_phy) {
		info->pmu_map = syscon_regmap_lookup_by_phandle(
					pdev->dev.of_node,
					"samsung,syscon-phandle");
		if (IS_ERR(info->pmu_map)) {
			dev_err(&pdev->dev, "syscon regmap lookup failed.\n");
			return PTR_ERR(info->pmu_map);
		}
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return irq;
	}
	info->irq = irq;

	irq = platform_get_irq(pdev, 1);
	if (irq == -EPROBE_DEFER)
		return irq;

	info->tsirq = irq;

	info->dev = &pdev->dev;
#ifdef CONFIG_ARCH_EXYNOS_PM
	info->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev));
#endif
	init_completion(&info->completion);

	info->clk = devm_clk_get(&pdev->dev, "gate_adcif");
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed getting clock, err = %ld\n",
							PTR_ERR(info->clk));
		return PTR_ERR(info->clk);
	}

	if (info->data->needs_sclk) {
		info->sclk = devm_clk_get(&pdev->dev, "sclk");
		if (IS_ERR(info->sclk)) {
			dev_err(&pdev->dev,
				"failed getting sclk clock, err = %ld\n",
				PTR_ERR(info->sclk));
			return PTR_ERR(info->sclk);
		}
	}

	info->vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(info->vdd)) {
		dev_err(&pdev->dev, "failed getting regulator, err = %ld\n",
							PTR_ERR(info->vdd));
		info->vdd = NULL;
	}

	ret = exynos_adc_enable_access(info);
	if (ret)
		goto err_access;

	platform_set_drvdata(pdev, indio_dev);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->info = &exynos_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = exynos_adc_iio_channels;
	indio_dev->num_channels = info->data->num_channels;

	ret = devm_request_irq(&pdev->dev, info->irq, info->data->adc_isr,
					0, dev_name(&pdev->dev), info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed requesting irq, irq = %d\n",
							info->irq);
		goto err_disable_clk;
	}

	disable_irq(info->irq);
	exynos_adc_disable_access(info);
	ret = iio_device_register(indio_dev);
	if (ret)
		goto err_irq;

	ret = of_platform_populate(np, exynos_adc_match, NULL, &indio_dev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed adding child nodes\n");
		goto err_of_populate;
	}

	dev_info(&pdev->dev, "Probed successfully driver.\n");

	return 0;

err_of_populate:
	device_for_each_child(&indio_dev->dev, NULL,
				exynos_adc_remove_devices);

#ifdef ADC_TS
err_iio:
	iio_device_unregister(indio_dev);
#endif
err_irq:
	free_irq(info->irq, info);
err_disable_clk:
	if (info->data->exit_hw)
		info->data->exit_hw(info);

	exynos_adc_disable_access(info);
err_access:
	return ret;
}

static int exynos_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct exynos_adc *info = iio_priv(indio_dev);
	int ret;

	if (IS_REACHABLE(CONFIG_INPUT) && info->input) {
		free_irq(info->tsirq, info);
		input_unregister_device(info->input);
	}
	device_for_each_child(&indio_dev->dev, NULL,
				exynos_adc_remove_devices);
	iio_device_unregister(indio_dev);
	free_irq(info->irq, info);

	ret = exynos_adc_enable_access(info);
	if (ret)
		return ret;

	if (info->data->exit_hw)
		info->data->exit_hw(info);

	exynos_adc_disable_access(info);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_adc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct exynos_adc *info = iio_priv(indio_dev);
	int ret;

	ret = exynos_adc_enable_access(info);
	if (ret)
		return ret;

	if (info->data->exit_hw)
		info->data->exit_hw(info);

	exynos_adc_disable_access(info);

	return 0;
}

static int exynos_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct exynos_adc *info = iio_priv(indio_dev);
	int ret;

	ret = exynos_adc_enable_access(info);
	if (ret)
		return ret;

	if (info->data->init_hw)
		info->data->init_hw(info);

	exynos_adc_disable_access(info);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(exynos_adc_pm_ops,
			exynos_adc_suspend,
			exynos_adc_resume);

static struct platform_driver exynos_adc_driver = {
	.probe		= exynos_adc_probe,
	.remove		= exynos_adc_remove,
	.driver		= {
		.name	= "exynos-adc",
		.of_match_table = exynos_adc_match,
	},
};

module_platform_driver(exynos_adc_driver);

MODULE_AUTHOR("Naveen Krishna Chatradhi <ch.naveen@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS5 ADC driver");
MODULE_LICENSE("GPL v2");
