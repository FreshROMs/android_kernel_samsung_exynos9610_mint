/*
 * s2mpu09-irq.c - Interrupt controller support for S2MPU09
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/samsung/s2mpu09.h>
#include <linux/mfd/samsung/s2mpu09-regulator.h>

static const u8 s2mpu09_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[PMIC_INT1] = 	S2MPU09_PMIC_REG_INT1M,
	[PMIC_INT2] =	S2MPU09_PMIC_REG_INT2M,
	[PMIC_INT3] = 	S2MPU09_PMIC_REG_INT3M,
	[PMIC_INT4] =	S2MPU09_PMIC_REG_INT4M,
	[PMIC_INT5] = 	S2MPU09_PMIC_REG_INT5M,
};

static struct i2c_client *get_i2c(struct s2mpu09_dev *s2mpu09,
				enum s2mpu09_irq_source src)
{
	switch (src) {
	case PMIC_INT1 ... PMIC_INT5:
		return s2mpu09->pmic;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mpu09_irq_data {
	int mask;
	enum s2mpu09_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct s2mpu09_irq_data s2mpu09_irqs[] = {
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_PWRONR_INT1,	PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_PWRONF_INT1,	PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_JIGONBF_INT1,	PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_JIGONBR_INT1,	PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_ACOKF_INT1,	PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_ACOKR_INT1,	PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_PWRON1S_INT1,	PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_MRB_INT1,		PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPU09_PMIC_IRQ_RTC60S_INT2,	PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_RTCA1_INT2,	PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_RTCA0_INT2,	PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_SMPL_INT2,		PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_RTC1S_INT2,	PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_WTSR_INT2,		PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_WTSRB_INT2,	PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPU09_PMIC_IRQ_120C_INT3,		PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_140C_INT3,		PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_TSD_INT3,		PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB1_INT3,	PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB2_INT3,	PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB3_INT4,	PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB4_INT4,	PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB5_INT4,	PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB6_INT4,	PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB7_INT4,	PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB8_INT4,	PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB9_INT4,	PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_OCPB10_INT4,	PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPU09_PMIC_IRQ_SCLDO2_INT5,	PMIC_INT5, 1 << 0),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_SCLDO18_INT5,	PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_SCLDO19_INT5,	PMIC_INT5, 1 << 2),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_SCLDO33_INT5,	PMIC_INT5, 1 << 3),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_SCLDO34_INT5,	PMIC_INT5, 1 << 4),
	DECLARE_IRQ(S2MPU09_PMIC_IRQ_SCLDO35_INT5,	PMIC_INT5, 1 << 5),
};

static void s2mpu09_irq_lock(struct irq_data *data)
{
	struct s2mpu09_dev *s2mpu09 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpu09->irqlock);
}

static void s2mpu09_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpu09_dev *s2mpu09 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPU09_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpu09_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpu09, i);

		if (mask_reg == S2MPU09_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mpu09->irq_masks_cache[i] = s2mpu09->irq_masks_cur[i];

		s2mpu09_write_reg(i2c, s2mpu09_mask_reg[i],
				s2mpu09->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpu09->irqlock);
}

static const inline struct s2mpu09_irq_data *
irq_to_s2mpu09_irq(struct s2mpu09_dev *s2mpu09, int irq)
{
	return &s2mpu09_irqs[irq - s2mpu09->irq_base];
}

static void s2mpu09_irq_mask(struct irq_data *data)
{
	struct s2mpu09_dev *s2mpu09 = irq_get_chip_data(data->irq);
	const struct s2mpu09_irq_data *irq_data =
	    irq_to_s2mpu09_irq(s2mpu09, data->irq);

	if (irq_data->group >= S2MPU09_IRQ_GROUP_NR)
		return;

	s2mpu09->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpu09_irq_unmask(struct irq_data *data)
{
	struct s2mpu09_dev *s2mpu09 = irq_get_chip_data(data->irq);
	const struct s2mpu09_irq_data *irq_data =
	    irq_to_s2mpu09_irq(s2mpu09, data->irq);

	if (irq_data->group >= S2MPU09_IRQ_GROUP_NR)
		return;

	s2mpu09->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpu09_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpu09_irq_lock,
	.irq_bus_sync_unlock	= s2mpu09_irq_sync_unlock,
	.irq_mask		= s2mpu09_irq_mask,
	.irq_unmask		= s2mpu09_irq_unmask,
};

static irqreturn_t s2mpu09_irq_thread(int irq, void *data)
{
	struct s2mpu09_dev *s2mpu09 = data;
	u8 irq_reg[S2MPU09_IRQ_GROUP_NR] = {0};
	u8 irq_src;
	int i, ret;

	//pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__,
	pr_err("%s: irq gpio pre-state(0x%02x)\n", __func__,
				gpio_get_value(s2mpu09->irq_gpio));

	ret = s2mpu09_read_reg(s2mpu09->i2c,
					S2MPU09_PMIC_REG_INTSRC, &irq_src);
	pr_err("%s: interrupt source(0x%02x)\n", __func__, irq_src);
	if (ret) {
		pr_err("%s:%s Failed to read interrupt source: %d\n",
			MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}

	pr_info("%s: interrupt source(0x%02x)\n", __func__, irq_src);

	if (irq_src & S2MPU09_IRQSRC_PMIC) {
		/* PMIC_INT */
		ret = s2mpu09_bulk_read(s2mpu09->pmic, S2MPU09_PMIC_REG_INT1,
				S2MPU09_NUM_IRQ_PMIC_REGS, &irq_reg[PMIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}

		pr_info("%s: pmic interrupt(0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
			 __func__, irq_reg[PMIC_INT1], irq_reg[PMIC_INT2], irq_reg[PMIC_INT3],
			 irq_reg[PMIC_INT4], irq_reg[PMIC_INT5]);
	}

	/* Apply masking */
	for (i = 0; i < S2MPU09_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mpu09->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPU09_IRQ_NR; i++) {
		if (irq_reg[s2mpu09_irqs[i].group] & s2mpu09_irqs[i].mask)
			handle_nested_irq(s2mpu09->irq_base + i);
	}

	return IRQ_HANDLED;
}

int s2mpu09_irq_init(struct s2mpu09_dev *s2mpu09)
{
	int i;
	int ret;
	u8 i2c_data;
	int cur_irq;

	if (!s2mpu09->irq_gpio) {
		dev_warn(s2mpu09->dev, "No interrupt specified.\n");
		s2mpu09->irq_base = 0;
		return 0;
	}

	if (!s2mpu09->irq_base) {
		dev_err(s2mpu09->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mpu09->irqlock);

	s2mpu09->irq = gpio_to_irq(s2mpu09->irq_gpio);
	pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
			s2mpu09->irq, s2mpu09->irq_gpio);

	ret = gpio_request(s2mpu09->irq_gpio, "pmic_irq");
	if (ret) {
		dev_err(s2mpu09->dev, "%s: failed requesting gpio %d\n",
			__func__, s2mpu09->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mpu09->irq_gpio);
	gpio_free(s2mpu09->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPU09_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mpu09->irq_masks_cur[i] = 0xff;
		s2mpu09->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mpu09, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mpu09_mask_reg[i] == S2MPU09_REG_INVALID)
			continue;

		s2mpu09_write_reg(i2c, s2mpu09_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPU09_IRQ_NR; i++) {
		cur_irq = i + s2mpu09->irq_base;
		irq_set_chip_data(cur_irq, s2mpu09);
		irq_set_chip_and_handler(cur_irq, &s2mpu09_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mpu09_write_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_INTSRC_MASK, 0xff);
	/* Unmask s2mpu09 interrupt */
	ret = s2mpu09_read_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read intsrc mask reg\n",
					 MFD_DEV_NAME, __func__);
		return ret;
	}

	i2c_data &= ~(S2MPU09_IRQSRC_PMIC);	/* Unmask pmic interrupt */
	s2mpu09_write_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_INTSRC_MASK,
			   i2c_data);

	pr_info("%s:%s s2mpu09_PMIC_REG_INTSRC_MASK=0x%02x\n",
			MFD_DEV_NAME, __func__, i2c_data);

	ret = request_threaded_irq(s2mpu09->irq, NULL, s2mpu09_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "s2mpu09-irq", s2mpu09);

	if (ret) {
		dev_err(s2mpu09->dev, "Failed to request IRQ %d: %d\n",
			s2mpu09->irq, ret);
		return ret;
	}

	return 0;
}

void s2mpu09_irq_exit(struct s2mpu09_dev *s2mpu09)
{
	if (s2mpu09->irq)
		free_irq(s2mpu09->irq, s2mpu09);
}
