/*
 * sm5713-charger.c - SM5713 Charger operation mode control module.
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>
#include <linux/usb/typec/pdic_notifier.h>
#include "include/sec_charging_common.h"

enum {
    OP_MODE_SUSPEND     = 0x0,
    OP_MODE_CHG_ON_VBUS = 0x5,
    OP_MODE_USB_OTG     = 0x7,
    OP_MODE_FLASH_BOOST = 0x8,
};

enum {
    BSTOUT_4400mV   = 0x0,
    BSTOUT_4500mV   = 0x1,
    BSTOUT_4600mV   = 0x2,
    BSTOUT_4700mV   = 0x3,
    BSTOUT_4800mV   = 0x4,
    BSTOUT_5000mV   = 0x5,
    BSTOUT_5100mV   = 0x6,
    BSTOUT_5200mV   = 0x7,
};

enum {
	OTG_CURRENT_500mA   = 0x0,
	OTG_CURRENT_900mA   = 0x1,
	OTG_CURRENT_1200mA  = 0x2,
	OTG_CURRENT_1500mA  = 0x3,
};

#define make_OP_STATUS(vbus, otg, pwr_shar, flash, torch, suspend)  (((vbus & 0x1)      << SM5713_CHARGER_OP_EVENT_VBUSIN)      | \
												((otg & 0x1)       << SM5713_CHARGER_OP_EVENT_USB_OTG)     | \
												((pwr_shar & 0x1)  << SM5713_CHARGER_OP_EVENT_PWR_SHAR)    | \
												((flash & 0x1)     << SM5713_CHARGER_OP_EVENT_FLASH)       | \
												((torch & 0x1)     << SM5713_CHARGER_OP_EVENT_TORCH)       | \
												((suspend & 0x1)   << SM5713_CHARGER_OP_EVENT_SUSPEND))

struct sm5713_charger_oper_table_info {
	unsigned short status;
	unsigned char oper_mode;
	unsigned char BST_OUT;
	unsigned char OTG_CURRENT;
};

struct sm5713_charger_oper_info {
	struct i2c_client *i2c;
	struct mutex op_mutex;
	int max_table_num;
	struct sm5713_charger_oper_table_info current_table;

	/* for check vbus voltage */
	bool vbus_updated;
	int irq_vbus_update;

	/* for Factory mode control */
	unsigned char factory_RID;
};
static struct sm5713_charger_oper_info *oper_info;

/**
 *  (VBUS in/out) (USB-OTG in/out) (PWR-SHAR in/out)
 *  (Flash on/off) (Torch on/off) (Suspend mode on/off)
 **/
static struct sm5713_charger_oper_table_info sm5713_charger_op_mode_table[] = {
	/* Charger=ON Mode in a valid Input */
	{ make_OP_STATUS(0, 0, 0, 0, 0, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 0, 0, 0, 0, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 1, 0, 0, 0, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},      /* Prevent : VBUS + OTG timing sync */
	{ make_OP_STATUS(1, 0, 0, 0, 1, 0), OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	/* Flash Boost Mode */
	{ make_OP_STATUS(0, 0, 0, 1, 0, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 0, 0, 1, 0, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(0, 0, 1, 1, 0, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 0, 1, 1, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 0, 0, 1, 0), OP_MODE_FLASH_BOOST, BSTOUT_4500mV, OTG_CURRENT_900mA},
	/* USB OTG Mode */
	{ make_OP_STATUS(0, 1, 0, 0, 0, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 1, 0, 0, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 1, 0, 1, 0, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 1, 0, 0, 1, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 1, 0, 1, 0), OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	/* Suspend Mode */
	{ make_OP_STATUS(0, 0, 0, 0, 0, 1), OP_MODE_SUSPEND,     BSTOUT_5100mV, OTG_CURRENT_900mA},      /* Reserved position of SUSPEND mode table */
};

static int set_OP_MODE(struct i2c_client *i2c, u8 mode)
{
	return sm5713_update_reg(i2c, SM5713_CHG_REG_CNTL2, (mode << 0), (0xF << 0));
}

static int set_BSTOUT(struct i2c_client *i2c, u8 bstout)
{
	return sm5713_update_reg(i2c, SM5713_CHG_REG_BSTCNTL1, (bstout << 0), (0xF << 0));
}

static int set_OTG_CURRENT(struct i2c_client *i2c, u8 otg_curr)
{
	return sm5713_update_reg(i2c, SM5713_CHG_REG_BSTCNTL1, (otg_curr << 6), (0x3 << 6));
}

static inline int change_op_table(unsigned char new_status)
{
	int i = 0;

	/* Check actvated Suspend Mode */
	if (new_status & (0x1 << SM5713_CHARGER_OP_EVENT_SUSPEND)) {
		i = oper_info->max_table_num - 1;    /* Reserved SUSPEND Mode Table index */
	} else {
		/* Search matched Table */
		for (i = 0; i < oper_info->max_table_num; ++i) {
			if (new_status == sm5713_charger_op_mode_table[i].status) {
				break;
			}
		}
	}
	if (i == oper_info->max_table_num) {
		pr_err("sm5713-charger: %s: can't find matched charger op_mode table (status = 0x%x)\n", __func__, new_status);
		return -EINVAL;
	}

    /* Update current table info */
	if (sm5713_charger_op_mode_table[i].BST_OUT != oper_info->current_table.BST_OUT) {
		set_BSTOUT(oper_info->i2c, sm5713_charger_op_mode_table[i].BST_OUT);
		oper_info->current_table.BST_OUT = sm5713_charger_op_mode_table[i].BST_OUT;
	}
	if (sm5713_charger_op_mode_table[i].OTG_CURRENT != oper_info->current_table.OTG_CURRENT) {
		set_OTG_CURRENT(oper_info->i2c, sm5713_charger_op_mode_table[i].OTG_CURRENT);
		oper_info->current_table.OTG_CURRENT = sm5713_charger_op_mode_table[i].OTG_CURRENT;
	}
	if (sm5713_charger_op_mode_table[i].oper_mode != oper_info->current_table.oper_mode) {
        /* Factory 523K-JIG Test : Torch Light - Prevent VBUS input source */
		if ((oper_info->factory_RID == RID_523K) && (sm5713_charger_op_mode_table[i].status == make_OP_STATUS(0, 0, 0, 0, 1, 0))) {
			pr_info("sm5713-charger: %s: skip Flash Boost mode for Factory JIG fled:torch test\n", __func__);
        /* Factory 523K-JIG Test : Flash Light - Prevent VBUS input source */
		} else if ((oper_info->factory_RID == RID_523K) && (sm5713_charger_op_mode_table[i].status == make_OP_STATUS(0, 0, 0, 1, 0, 0))) {
			pr_info("sm5713-charger: %s: skip Flash Boost mode for Factory JIG fled:flash test\n", __func__);
        } else {
            set_OP_MODE(oper_info->i2c, sm5713_charger_op_mode_table[i].oper_mode);
            oper_info->current_table.oper_mode = sm5713_charger_op_mode_table[i].oper_mode;
        }
	}
	oper_info->current_table.status = new_status;

	pr_info("sm5713-charger: %s: New table[%d] info (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x\n",
			__func__, i, oper_info->current_table.status, oper_info->current_table.oper_mode,
			oper_info->current_table.BST_OUT, oper_info->current_table.OTG_CURRENT);

	return 0;
}

static inline unsigned char update_status(int event_type, bool enable)
{
	if (event_type > SM5713_CHARGER_OP_EVENT_VBUSIN) {
		pr_debug("sm5713-charger: %s: invalid event type (type=0x%x)\n", __func__, event_type);
		return oper_info->current_table.status;
	}

	if (enable) {
		return (oper_info->current_table.status | (1 << event_type));
	} else {
		return (oper_info->current_table.status & ~(1 << event_type));
	}
}

int sm5713_charger_oper_push_event(int event_type, bool enable)
{
	unsigned char new_status;
    int ret = 0;

	if (oper_info == NULL) {
		pr_err("sm5713-charger: %s: required init op_mode table\n", __func__);
		return -ENOENT;
	}
	pr_info("sm5713-charger: %s: event_type=%d, enable=%d\n", __func__, event_type, enable);

	mutex_lock(&oper_info->op_mutex);

	new_status = update_status(event_type, enable);
    if (new_status == oper_info->current_table.status) {
		goto out;
	}
    ret = change_op_table(new_status);

out:
	mutex_unlock(&oper_info->op_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(sm5713_charger_oper_push_event);

static inline int detect_initial_table_index(struct i2c_client *i2c)
{
    return 0;
}

static irqreturn_t vbus_update_isr(int irq, void *data)
{
	pr_info("sm5713-charger: %s: irq=%d\n", __func__, irq);

	oper_info->vbus_updated = 1;
	sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL9, (0x0 << 0), (0x1 << 0));

	return IRQ_HANDLED;
}

int sm5713_charger_oper_table_init(struct sm5713_dev *sm5713)
{
	struct i2c_client *i2c = sm5713->charger;
	int ret, index;

	if (oper_info) {
		pr_info("sm5713-charger: %s: already initialized\n", __func__);
		return 0;
	}

	if (i2c == NULL) {
		pr_err("sm5713-charger: %s: invalid i2c client handler=n", __func__);
		return -EINVAL;
	}

	oper_info = kmalloc(sizeof(struct sm5713_charger_oper_info), GFP_KERNEL);
	if (oper_info == NULL) {
		pr_err("sm5713-charger: %s: failed to alloctae memory\n", __func__);
		return -ENOMEM;
	}
	oper_info->i2c = i2c;

	mutex_init(&oper_info->op_mutex);

	/* set default operation mode condition */
	oper_info->max_table_num = ARRAY_SIZE(sm5713_charger_op_mode_table);
	index = detect_initial_table_index(oper_info->i2c);
	oper_info->current_table.status = sm5713_charger_op_mode_table[index].status;
	oper_info->current_table.oper_mode = sm5713_charger_op_mode_table[index].oper_mode;
	oper_info->current_table.BST_OUT = sm5713_charger_op_mode_table[index].BST_OUT;
	oper_info->current_table.OTG_CURRENT = sm5713_charger_op_mode_table[index].OTG_CURRENT;

	set_OP_MODE(oper_info->i2c, oper_info->current_table.oper_mode);
	set_BSTOUT(oper_info->i2c, oper_info->current_table.BST_OUT);
	set_OTG_CURRENT(oper_info->i2c, oper_info->current_table.OTG_CURRENT);

	oper_info->irq_vbus_update = sm5713->irq_base + SM5713_CHG_IRQ_INT4_VBUS_UPDATE;
	ret = request_threaded_irq(oper_info->irq_vbus_update, NULL, vbus_update_isr, 0 , "vbusupdate-irq", NULL);
	if (ret < 0) {
		pr_err("sm5713-charger: %s: failed request irq:%d (ret=%d)\n", __func__, oper_info->irq_vbus_update, ret);
		return ret;
	}
	oper_info->factory_RID = 0;

	pr_info("sm5713-charger: %s: current table info (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x)\n", \
			__func__, oper_info->current_table.status, oper_info->current_table.oper_mode, oper_info->current_table.BST_OUT, \
			oper_info->current_table.OTG_CURRENT);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5713_charger_oper_table_init);

int sm5713_charger_oper_get_current_status(void)
{
	if (oper_info == NULL) {
		return -EINVAL;
	}
	return oper_info->current_table.status;
}
EXPORT_SYMBOL_GPL(sm5713_charger_oper_get_current_status);

int sm5713_charger_oper_get_current_op_mode(void)
{
	if (oper_info == NULL) {
		return -EINVAL;
	}
	return oper_info->current_table.oper_mode;
}
EXPORT_SYMBOL_GPL(sm5713_charger_oper_get_current_op_mode);

int sm5713_charger_oper_get_vbus_voltage(void)
{
	/* This function need to polling time (max=150ms) */
	int i;
	u8 reg;
	int vbus_voltage;

	if (oper_info == NULL) {
		return -EINVAL;
	}

	sm5713_read_reg(oper_info->i2c, SM5713_CHG_REG_STATUS1, &reg);
	if ((reg & 0x1) == 0) {
		pr_info("sm5713-charger: %s: vbus=uvlo\n", __func__);
		return 0;
	}

	oper_info->vbus_updated = 0;
	sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL9, (0x1 << 0), (0x1 << 0));

	for (i = 0; i < 15; ++i) {
		if (oper_info->vbus_updated) {
			break;
		}
		msleep(10);
	}

	if (i == 15) {
		pr_err("sm5713-charger: %s: time out\n", __func__);
		return -EINVAL;
	}

	sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL9, (0x0 << 0), (0x1 << 0));
	sm5713_read_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL10, &reg);

	vbus_voltage = ((reg & 0x3) * 1000) / 4;
	vbus_voltage += (reg >> 2) * 1000;

	pr_info("sm5713-charger: %s: vbus voltage=%dmV\n", __func__, vbus_voltage);

	return vbus_voltage;
}
EXPORT_SYMBOL_GPL(sm5713_charger_oper_get_vbus_voltage);

int sm5713_charger_oper_en_factory_mode(int dev_type, int rid, bool enable)
{
	u8 reg;
	bool nenq4_status;
	union power_supply_propval val = {0, };

	if (oper_info == NULL) {
		return -EINVAL;
	}

	sm5713_read_reg(oper_info->i2c, SM5713_CHG_REG_STATUS3, &reg);
	nenq4_status = (reg & 0x40);
	pr_info("sm5713-charger: %s device type = %d, enable = %d, nENQ4_ST = %d \n", __func__, dev_type, enable, nenq4_status);

	if (enable) {
		switch(rid) {
		case RID_523K:
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CNTL1, (0x0 << 6), (0x1 << 6));			/* AICLEN_VBUS = 0 */
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_FACTORY1, (0x1 << 0), (0x1 << 0));		/* NOZX = 1 */
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL6, (0x0 << 0), (0x3 << 0));		/* Q2LIM = 0b00 */
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_VBUSCNTL, (0x7F << 0), (0x7F << 0));		/* VBUS_LIMIT=MAX(3275mA) */
		break;
		case RID_301K:
		case RID_619K:
			sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CNTL1, (0x0 << 6), (0x1 << 6));			/* AICLEN_VBUS = 0 */
			sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_FACTORY1, (0x0 << 0), (0x1 << 0));		/* NOZX = 0 */
			sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL6, (0x0 << 0), (0x3 << 0));		/* Q2LIM = 0b00 */
			sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_VBUSCNTL, (0x44 << 0), (0x7F << 0));		/* VBUS_LIMIT=1800mA */
		break;
		}

		psy_do_property("sm5713-fuelgauge", set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);

		oper_info->factory_RID = rid;
		pr_info("sm5713-charger: %s enable factroy mode configuration\n", __func__);
	} else {
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL11, (0 << 0) , (0x1 << 0));		/*q4 forced vsys disable */
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CNTL1, (0x1 << 6), (0x1 << 6));			/* AICLEN_VBUS = 1 */
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_FACTORY1, (0x0 << 0), (0x1 << 0));		/* NOZX = 0 */
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_CHGCNTL6, (0x0 << 0), (0x3 << 0));		/* Q2LIM = 0b00 */
		sm5713_update_reg(oper_info->i2c, SM5713_CHG_REG_VBUSCNTL, (0x10 << 0), (0x7F << 0));		/* VBUS_LIMIT=500mA*/

		oper_info->factory_RID = 0;
		pr_info("sm5713-charger: %s disable factroy mode configuration\n", __func__);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5713_charger_oper_en_factory_mode);