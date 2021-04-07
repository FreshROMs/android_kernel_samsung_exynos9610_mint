/*
 *  sm5713-irq.c - Interrupt controller support for sm5713
 *
 *  Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>

static const u8 sm5713_mask_reg[] = {
	[MUIC_INT1]     = SM5713_MUIC_REG_INTMASK1,
	[MUIC_INT2]     = SM5713_MUIC_REG_INTMASK2,
	[CHG_INT1]      = SM5713_CHG_REG_INTMSK1,
	[CHG_INT2]      = SM5713_CHG_REG_INTMSK2,
    [CHG_INT3]      = SM5713_CHG_REG_INTMSK3,
    [CHG_INT4]      = SM5713_CHG_REG_INTMSK4,
    [CHG_INT5]      = SM5713_CHG_REG_INTMSK5,
    [CHG_INT6]      = SM5713_CHG_REG_INTMSK6,
    [FG_INT]        = SM5713_FG_REG_INTFG_MASK,
};

struct sm5713_irq_data {
	int mask;
	int group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct sm5713_irq_data sm5713_irqs[] = {
    /* MUIC-irqs */
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT1_DPDM_OVP,	        MUIC_INT1,      1 << 5),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT1_VBUS_RID_DETACH,	MUIC_INT1,      1 << 4),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT1_AUTOVBUSCHECK,	    MUIC_INT1,      1 << 3),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT1_RID_DETECT,	    MUIC_INT1,      1 << 2),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT1_CHGTYPE,	        MUIC_INT1,      1 << 1),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT1_DCDTIMEOUT,	    MUIC_INT1,      1 << 0),

    DECLARE_IRQ(SM5713_MUIC_IRQ_INT2_AFC_ERROR,         MUIC_INT2,      1 << 5),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT2_AFC_STA_CHG,       MUIC_INT2,      1 << 4),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT2_MULTI_BYTE,        MUIC_INT2,      1 << 3),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT2_VBUS_UPDATE,	    MUIC_INT2,      1 << 2),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT2_AFC_ACCEPTED,      MUIC_INT2,      1 << 1),
    DECLARE_IRQ(SM5713_MUIC_IRQ_INT2_AFC_TA_ATTACHED,   MUIC_INT2,      1 << 0),

    /* Charger-irqs */
    DECLARE_IRQ(SM5713_CHG_IRQ_INT1_VBUSLIMIT,	        CHG_INT1,       1 << 3),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT1_VBUSOVP,	        CHG_INT1,       1 << 2),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT1_VBUSUVLO,	        CHG_INT1,       1 << 1),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT1_VBUSPOK,	        CHG_INT1,       1 << 0),

    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_WDTMROFF,	        CHG_INT2,       1 << 7),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_DONE,	            CHG_INT2,       1 << 6),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_TOPOFF,	            CHG_INT2,       1 << 5),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_Q4FULLON,	        CHG_INT2,       1 << 4),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_CHGON,	            CHG_INT2,       1 << 3),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_NOBAT,              CHG_INT2,       1 << 2),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_BATOVP,             CHG_INT2,       1 << 1),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT2_AICL,	            CHG_INT2,       1 << 0),

    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_VSYSOVP,	        CHG_INT3,       1 << 7),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_nENQ4,	            CHG_INT3,       1 << 6),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_FASTTMROFF,	        CHG_INT3,       1 << 5),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_PRETMROFF,	        CHG_INT3,       1 << 4),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_DISLIMIT,	        CHG_INT3,       1 << 3),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_OTGFAIL,	        CHG_INT3,       1 << 2),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_THEMSHDN,           CHG_INT3,       1 << 1),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT3_THEMREG,            CHG_INT3,       1 << 0),

    DECLARE_IRQ(SM5713_CHG_IRQ_INT4_CVMODE,	            CHG_INT4,       1 << 7),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT4_VBUS_UPDATE,	    CHG_INT4,       1 << 6),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT4_MRSTB,	            CHG_INT4,       1 << 4),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT4_nVBUSOK,	        CHG_INT4,       1 << 2),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT4_BOOSTPOK,	        CHG_INT4,       1 << 1),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT4_BOOSTPOK_NG,	    CHG_INT4,       1 << 0),

    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_ABSTMR2OFF,	        CHG_INT5,       1 << 7),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_ABSTMR1OFF,	        CHG_INT5,       1 << 6),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_FLED3OPEN,	        CHG_INT5,       1 << 5),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_FLED3SHORT,	        CHG_INT5,       1 << 4),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_FLED2OPEN,	        CHG_INT5,       1 << 3),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_FLED2SHORT,	        CHG_INT5,       1 << 2),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_FLED1OPEN,          CHG_INT5,       1 << 1),
    DECLARE_IRQ(SM5713_CHG_IRQ_INT5_FLED1SHORT,         CHG_INT5,       1 << 0),

    DECLARE_IRQ(SM5713_CHG_IRQ_INT6_VBUSSHORT,          CHG_INT6,       1 << 0),

    /* FuelGauge-irqs */
    DECLARE_IRQ(SM5713_FG_IRQ_INT_LOW_SOC,              FG_INT,         1 << 4),
    DECLARE_IRQ(SM5713_FG_IRQ_INT_HIGH_TEMP,	        FG_INT,         1 << 3),
    DECLARE_IRQ(SM5713_FG_IRQ_INT_LOW_TEMP,	            FG_INT,         1 << 2),
    DECLARE_IRQ(SM5713_FG_IRQ_INT_HIGH_VOLTAGE,	        FG_INT,         1 << 1),
    DECLARE_IRQ(SM5713_FG_IRQ_INT_LOW_VOLTAGE,	        FG_INT,         1 << 0),
};

static struct i2c_client *get_i2c(struct sm5713_dev *sm5713, int src)
{
	switch (src) {
	case MUIC_INT1:
	case MUIC_INT2:
		return sm5713->muic;
	case CHG_INT1:
	case CHG_INT2:
	case CHG_INT3:
	case CHG_INT4:
	case CHG_INT5:
	case CHG_INT6:
		return sm5713->charger;
	case FG_INT:
		return sm5713->fuelgauge;
	}

	return ERR_PTR(-EINVAL);
}

static void sm5713_irq_lock(struct irq_data *data)
{
	struct sm5713_dev *sm5713 = irq_get_chip_data(data->irq);

	mutex_lock(&sm5713->irqlock);
}

static void sm5713_irq_sync_unlock(struct irq_data *data)
{
	struct sm5713_dev *sm5713 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < SM5713_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c = get_i2c(sm5713, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;

		sm5713->irq_masks_cache[i] = sm5713->irq_masks_cur[i];

		if (i == FG_INT) {
			sm5713_write_word(i2c, sm5713_mask_reg[i], sm5713->irq_masks_cur[i]);
		} else {
			sm5713_write_reg(i2c, sm5713_mask_reg[i], sm5713->irq_masks_cur[i]);
		}

	}

	mutex_unlock(&sm5713->irqlock);
}

static const inline struct sm5713_irq_data *
irq_to_sm5713_irq(struct sm5713_dev *sm5713, int irq)
{
	return &sm5713_irqs[irq - sm5713->irq_base];
}

static void sm5713_irq_mask(struct irq_data *data)
{
	struct sm5713_dev *sm5713 = irq_get_chip_data(data->irq);
	const struct sm5713_irq_data *irq_data = irq_to_sm5713_irq(sm5713, data->irq);

	if (irq_data->group >= SM5713_IRQ_GROUP_NR)
		return;

    sm5713->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void sm5713_irq_unmask(struct irq_data *data)
{
	struct sm5713_dev *sm5713 = irq_get_chip_data(data->irq);
    const struct sm5713_irq_data *irq_data = irq_to_sm5713_irq(sm5713, data->irq);

	if (irq_data->group >= SM5713_IRQ_GROUP_NR)
		return;

    sm5713->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip sm5713_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= sm5713_irq_lock,
	.irq_bus_sync_unlock	= sm5713_irq_sync_unlock,
	.irq_mask		= sm5713_irq_mask,
	.irq_unmask		= sm5713_irq_unmask,
	.irq_disable		= sm5713_irq_mask,
	.irq_enable		= sm5713_irq_unmask,
};

static irqreturn_t sm5713_irq_thread(int irq, void *data)
{
	struct sm5713_dev *sm5713 = data;
	u8 irq_reg[SM5713_IRQ_GROUP_NR] = {0};
	u8 irq_src;
	int i, ret;

	pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__, gpio_get_value(sm5713->irq_gpio));

	ret = sm5713_read_reg(sm5713->charger, SM5713_CHG_REG_INT_SOURCE, &irq_src);
	pr_info("%s\n irq_src = %x", __func__, irq_src);
	if (ret) {
		pr_err("%s:%s fail to read interrupt source: %d\n", MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}
	pr_debug("%s: INT_SOURCE=0x%02x)\n", __func__, irq_src);

	/* Irregular case: check IC status */
	if (irq_src == 0) {
		if (sm5713->check_muic_reset)
			sm5713->check_muic_reset(sm5713->muic, sm5713->muic_data);
		if (sm5713->check_chg_reset)
			sm5713->check_chg_reset(sm5713->charger, sm5713->chg_data);
		if (sm5713->check_fg_reset)
			sm5713->check_fg_reset(sm5713->fuelgauge, sm5713->fg_data);
	}

	/* Charger INT 1 ~ 6 */
	if (irq_src & SM5713_IRQSRC_CHG) {
		ret = sm5713_bulk_read(sm5713->charger, SM5713_CHG_REG_INT1, SM5713_NUM_IRQ_CHG_REGS, &irq_reg[CHG_INT1]);
		if (ret) {
			pr_err("%s:%s fail to read CHG_INT source: %d\n", MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		for (i = CHG_INT1; i <= CHG_INT6; i++) {
			pr_debug("%s:%s CHG_INT%d = 0x%x\n", MFD_DEV_NAME, __func__, (i - 1), irq_reg[i]);
		}
	}

	/* MUIC INT 1 ~ 2 */
	if (irq_src & SM5713_IRQSRC_MUIC) {
		ret = sm5713_bulk_read(sm5713->muic, SM5713_MUIC_REG_INT1, SM5713_NUM_IRQ_MUIC_REGS, &irq_reg[MUIC_INT1]);
		pr_info ("%s:%s MUIC_INT1 = 0x%x, MUIC_INT2 = 0x%x\n", MFD_DEV_NAME, __func__, irq_reg[MUIC_INT1], irq_reg[MUIC_INT2]);
		if (ret) {
			pr_err("%s:%s fail to read MUIC_INT source: %d\n", MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		for (i = MUIC_INT1; i <= MUIC_INT2; i++) {
			pr_debug("%s:%s MUIC_INT%d = 0x%x\n", MFD_DEV_NAME, __func__, (i + 1), irq_reg[i]);
		}
	}

	/* Fuel Gauge INT */
	if (irq_src & SM5713_IRQSRC_FG) {
		/* FG_INT Lock */
		sm5713->irq_masks_cur[FG_INT] |= (1 << 7);
		sm5713_write_word(sm5713->fuelgauge, sm5713_mask_reg[FG_INT], sm5713->irq_masks_cur[FG_INT]);

		irq_reg[FG_INT] = (u8)(sm5713_read_word(sm5713->fuelgauge, SM5713_FG_REG_INTFG) & 0x00FF);

		/* FG_INT Un-Lock */
		sm5713->irq_masks_cur[FG_INT] &= ~(1 << 7);
		sm5713_write_word(sm5713->fuelgauge, sm5713_mask_reg[FG_INT], sm5713->irq_masks_cur[FG_INT]);

		pr_info("%s:%s FG_INT = 0x%x\n", MFD_DEV_NAME, __func__, irq_reg[FG_INT]);
	}

	/* Apply masking */
	for (i = 0; i < SM5713_IRQ_GROUP_NR; i++) {
		irq_reg[i] &= ~sm5713->irq_masks_cur[i];
	}

	/* Report */
	for (i = 0; i < SM5713_IRQ_NR; i++) {
		if (irq_reg[sm5713_irqs[i].group] & sm5713_irqs[i].mask) {
			handle_nested_irq(sm5713->irq_base + i);
		}
	}

	return IRQ_HANDLED;
}

int sm5713_irq_init(struct sm5713_dev *sm5713)
{
	struct i2c_client *i2c;
	int i;
	int ret;

	if (!sm5713->irq_gpio) {
		dev_warn(sm5713->dev, "No interrupt specified.\n");
		sm5713->irq_base = 0;
		return 0;
	}

	if (!sm5713->irq_base) {
		dev_err(sm5713->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&sm5713->irqlock);

	sm5713->irq = gpio_to_irq(sm5713->irq_gpio);
	pr_err("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
			sm5713->irq, sm5713->irq_gpio);

	ret = gpio_request(sm5713->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(sm5713->dev, "%s: failed requesting gpio %d\n",
			__func__, sm5713->irq_gpio);
		return ret;
	}
	gpio_direction_input(sm5713->irq_gpio);
	gpio_free(sm5713->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < SM5713_IRQ_GROUP_NR; i++) {
		i2c = get_i2c(sm5713, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;

		if (i == FG_INT) {
			sm5713->irq_masks_cur[i] = 0x000f;
			sm5713->irq_masks_cache[i] = 0x000f;
			sm5713_write_word(i2c, sm5713_mask_reg[i], sm5713->irq_masks_cur[i]);
		} else {
			sm5713->irq_masks_cur[i] = 0xff;
			sm5713->irq_masks_cache[i] = 0xff;
			sm5713_write_reg(i2c, sm5713_mask_reg[i], sm5713->irq_masks_cur[i]);
		}
	}

	/* Register with genirq */
	for (i = 0; i < SM5713_IRQ_NR; i++) {
		int cur_irq;
		cur_irq = i + sm5713->irq_base;
		irq_set_chip_data(cur_irq, sm5713);
		irq_set_chip_and_handler(cur_irq, &sm5713_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(sm5713->irq, NULL, sm5713_irq_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"sm5713-irq", sm5713);
	if (ret) {
		dev_err(sm5713->dev, "Fail to request IRQ %d: %d\n", sm5713->irq, ret);
		return ret;
	}

	return 0;
}

void sm5713_irq_exit(struct sm5713_dev *sm5713)
{
	if (sm5713->irq)
		free_irq(sm5713->irq, sm5713);
}




