/*
 * sm5713-charger.c - SM5713 Charger device driver
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/sec_batt.h>
#include <linux/muic/muic.h>
#include "include/sec_charging_common.h"
#include "include/charger/sm5713_charger.h"

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

#define HEALTH_DEBOUNCE_CNT     	1
#define ENABLE_SM5713_ENBYPASS_MODE	1

static struct device_attribute sm5713_charger_attrs[] = {
	SM5713_CHARGER_ATTR(chip_id),
};

static char *sm5713_supplied_to[] = {
	"sm5713-charger",
};

static enum power_supply_property sm5713_charger_props[] = {
};

static enum power_supply_property sm5713_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static void sm5713_charger_enable_aicl_irq(struct sm5713_charger_data *charger);

#if defined(ENABLE_SM5713_ENBYPASS_MODE)
static void chg_set_en_bypass(struct sm5713_charger_data *charger, bool enable)
{
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_FACTORY1, (enable << 1), (0x1 << 1));
	pr_info("sm5713-charger: %s: bypass mode - %s \n", __func__, enable ? "Enable" : "Disable");
}

static void chg_set_en_bypass_mode(struct sm5713_charger_data *charger, bool enable)
{
	union power_supply_propval val = {0, };

	if (enable) {
		sm5713_update_reg(charger->i2c, SM5713_CHG_REG_FACTORY1, (0x1 << 4), (0x1 << 4));	/* OFFREVERSE deactivated(1) */

		sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CNTL1, (0x0 << 6), (0x1 << 6));		/* AICLEN_VBUS = disable(0) */
		sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CNTL1, (0x0 << 2), (0x1 << 2));		/* ENUSBLDO = disable(0) */

		chg_set_en_bypass(charger, 1);	/* ENBYPASS = enable(1) */

		psy_do_property("sm5713-fuelgauge", set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);

	} else {
		sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CNTL1, (0x1 << 2), (0x1 << 2));		/* ENUSBLDO = enable(1) */
		sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CNTL1, (0x1 << 6), (0x1 << 6));		/* AICLEN_VBUS = enable(1) */

		sm5713_update_reg(charger->i2c, SM5713_CHG_REG_FACTORY1, (0x0 << 4), (0x1 << 4));	/* OFFREVERSE activated(0) */

		chg_set_en_bypass(charger, 0);	/* ENBYPASS = disable(0) */
	}
	pr_info("sm5713-charger: %s: %s\n", __func__, enable ? "Enable" : "Disable");
}
#endif

static void chg_set_aicl(struct sm5713_charger_data *charger, bool enable, u8 aicl)
{
    sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL6, (aicl << 6), (0x3 << 6));
    sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CNTL1, (enable << 6), (0x1 << 6));
}

static void chg_set_dischg_limit(struct sm5713_charger_data *charger, u8 dischg)
{
    sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL6, (dischg << 2), (0x3 << 2));
}

static void chg_set_ocp_current(struct sm5713_charger_data *charger, u32 ocp_current)
{
	u8 dischg = DISCHG_LIMIT_C_4_5;

	if (ocp_current >= 6000)
		dischg = DISCHG_LIMIT_C_6_0;
	else if (ocp_current >= 5000)
		dischg = DISCHG_LIMIT_C_5_0;
	else if (ocp_current >= 4500)
		dischg = DISCHG_LIMIT_C_4_5;
	else
		dischg = DISCHG_LIMIT_C_3_5;

	chg_set_dischg_limit(charger, dischg);		
}

static void chg_set_batreg(struct sm5713_charger_data *charger, u16 float_voltage)
{
	u8 offset;

	if (float_voltage <= 3700)
		offset = 0x0;
	else if (float_voltage < 3900)
		offset = ((float_voltage - 3700) / 50);    /* BATREG = 3.70 ~ 3.85V in 0.05V steps */
	else if (float_voltage < 4050)
		offset = (((float_voltage - 3900) / 100) + 4);    /* BATREG = 3.90, 4.0V in 0.1V steps */
	else if (float_voltage < 4630)
		offset = (((float_voltage - 4050) / 10) + 6);    /* BATREG = 4.05 ~ 4.62V in 0.01V steps */
	else {
		dev_err(charger->dev, "%s: can't support BATREG at over voltage 4.62V (mV=%d)\n", __func__, float_voltage);
		offset = 0x15;    /* default Offset : 4.2V */
	}

	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL4, ((offset & 0x3F) << 0), (0x3F << 0));
}

#if 0
static void chg_set_dis_q4_ocp(struct sm5713_charger_data *charger, bool disable)
{
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL11, (disable << 1), (0x1 << 1));
}

static void chg_set_q4_forced_vsys(struct sm5713_charger_data *charger, bool enable)
{
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL11, (enable << 0), (0x1 << 0));
}
#endif

static void chg_set_iq3limit(struct sm5713_charger_data *charger, u8 q3limit)
{
    sm5713_update_reg(charger->i2c, SM5713_CHG_REG_BSTCNTL1, (q3limit << 4), (0x3 << 4));
}

static void chg_set_wdt_timer(struct sm5713_charger_data *charger, u8 wdt_timer)
{
    sm5713_update_reg(charger->i2c, SM5713_CHG_REG_WDTCNTL, (wdt_timer << 1), (0x3 << 1));
}

static void chg_set_wdt_tmr_reset(struct sm5713_charger_data *charger)
{
	dev_info(charger->dev, "%s: wdt kick\n", __func__);
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_WDTCNTL, (0x1 << 3), (0x1 << 3));
}

static void chg_set_wdt_enable(struct sm5713_charger_data *charger, bool enable)
{
	dev_info(charger->dev, "%s: wdt enable(%d)\n", __func__, enable);
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_WDTCNTL, (enable << 0), (0x1 << 0));
	if (enable)
		chg_set_wdt_tmr_reset(charger);
}

static void chg_set_wdtcntl_reset(struct sm5713_charger_data *charger)
{
	dev_info(charger->dev, "%s: clear wdt expired\n", __func__);
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_WDTCNTL, (0x1 << 6), (0x1 << 6));
}

static void chg_set_enq4fet(struct sm5713_charger_data *charger, bool enable)
{
    sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CNTL1, (enable << 3), (0x1 << 3));
}

static void chg_set_input_current_limit(struct sm5713_charger_data *charger, int mA)
{
    u8 offset;

	if (factory_mode) {
		pr_info("%s: Factory Mode Skip current limit Control\n", __func__);
		return;
	}

	mutex_lock(&charger->charger_mutex);
	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_FACTORY1, &offset);
	if (offset & 0x1) {
		dev_info(charger->dev, "enabled FACTORY mode, skipped VBUS_LIMIT setting\n");
	} else {
		if (mA < 100) {
			offset = 0x00;
		} else {
			offset = ((mA - 100) / 25) & 0x7F;
		}
		sm5713_update_reg(charger->i2c, SM5713_CHG_REG_VBUSCNTL, (offset << 0), (0x7F << 0));
	}
	mutex_unlock(&charger->charger_mutex);
}

static void chg_set_charging_current(struct sm5713_charger_data *charger, int mA)
{
	u8 offset;

	if (factory_mode) {
		pr_info("%s: Factory Mode Skip charging current Control\n", __func__);
		return;
	}

	if (mA < 100) {
		offset = 0x00;
	} else if (mA > 3500) {
		offset = 0x44;
	} else {
		offset = ((mA - 100) / 50) & 0x7F;
	}
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL2, (offset << 0), (0x7F << 0));
}

static void chg_set_topoff_current(struct sm5713_charger_data *charger, int mA)
{
	u8 offset;

	if (mA < 100) {
		offset = 0x0;               /* Topoff = 100mA */
	} else if (mA < 600) {
		offset = (mA - 100) / 25;   /* Topoff = 125mA ~ 575mA in 25mA steps */
	} else {
		offset = 0x14;              /* Topoff = 600mA */
	}
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL5, (offset << 0), (0x1F << 0));
}

static void chg_set_topoff_timer(struct sm5713_charger_data *charger, u8 tmr_offset)
{
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL7, (tmr_offset << 3), (0x3 << 3));
}

static void chg_set_autostop(struct sm5713_charger_data *charger, bool enable)
{
	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL4, (enable << 6), (0x1 << 6));
}

static int chg_get_input_current_limit(struct sm5713_charger_data *charger)
{
	u8 reg;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_VBUSCNTL, &reg);

	return ((reg & 0x7F) * 25) + 100;
}

static int chg_get_charging_current(struct sm5713_charger_data *charger)
{
	u8 reg;
	int fast_curr;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL2, &reg);

	if ((reg & 0x7F) >= 0x44) {
		fast_curr = 3500;
	} else {
		fast_curr = ((reg & 0x7F) * 50) + 100;
	}

	return fast_curr;
}

static int chg_get_topoff_current(struct sm5713_charger_data *charger)
{
	u8 reg;
	int topoff;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL5, &reg);

	if ((reg & 0x1F) >= 0x14) {
		topoff = 600;
	} else {
		topoff = ((reg & 0x1F) * 25) + 100;
	}

	return topoff;
}

static int chg_get_regulation_voltage(struct sm5713_charger_data *charger)
{
	u8 reg;
	int float_voltage;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_CHGCNTL4, &reg);

	reg = reg & 0x3F;

	if (reg <= 0x03)	/* BATREG = 3.70 ~ 3.85V in 0.05V steps */
		float_voltage = 3700 + (reg * 50);
	else if (reg <= 0x5)	/* BATREG = 3.90, 4.0V in 0.1V steps */
		float_voltage = 3900 + ((reg - 0x4) * 100);
	else if (reg <= 0x3F)	/* BATREG = 4.05 ~ 4.62V in 0.01V steps */
		float_voltage = 4050 + ((reg - 0x6) * 10);
	else
		float_voltage = 4620;

	return float_voltage;
}

#define PRINT_CHG_REG_NUM   32
static void chg_print_regmap(struct sm5713_charger_data *charger)
{
	u8 regs[PRINT_CHG_REG_NUM] = {0x0, };
	char temp_buf[500] = {0,};
	int i;

	sm5713_bulk_read(charger->i2c, SM5713_CHG_REG_INTMSK1, PRINT_CHG_REG_NUM, regs);

	for (i = 0; i < PRINT_CHG_REG_NUM; ++i) {
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", SM5713_CHG_REG_INTMSK1 + i, regs[i]);
		if (((i+1) % 16 == 0) || ((i+1) == PRINT_CHG_REG_NUM)) {
			pr_info("sm5713-charger: regmap: %s\n", temp_buf);
			memset(temp_buf, 0x0, sizeof(temp_buf));
		}
	}
}

static int sm5713_chg_create_attrs(struct device *dev)
{
	unsigned long i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(sm5713_charger_attrs); i++) {
		rc = device_create_file(dev, &sm5713_charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &sm5713_charger_attrs[i]);
	return rc;
}

ssize_t sm5713_chg_show_attrs(struct device *dev, struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - sm5713_charger_attrs;
	int i = 0;

	switch (offset) {
	case CHIP_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", "SM5713");
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t sm5713_chg_store_attrs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5713_charger_data *charger =	power_supply_get_drvdata(psy);
	int ret = 0;
	u32 store_value;

	if (charger == NULL) {
		pr_err("%s: charger is NULL \n", __func__);
		return -ENODEV;
	}

	if (buf == NULL || kstrtouint(buf, 10, &store_value)) {
		return -ENXIO;
	}

	pr_info("sm5713-charger: %s:  store_value = %d \n", __func__, store_value);
	if (charger->i2c == NULL) {
		pr_err("%s: Charger i2c is NULL \n", __func__);
	}
	ret = count;

	switch (store_value) {
	case CHIP_ID:
		break;
#if defined(ENABLE_SM5713_ENBYPASS_MODE)
	case EN_BYPASS_MODE:
		chg_set_en_bypass_mode(charger, 1);
		break;
#endif
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int psy_chg_get_online(struct sm5713_charger_data *charger)
{
	u8 reg;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS1, &reg);

	return (reg & 0x1) ? 1 : 0;
}

static int psy_chg_get_status(struct sm5713_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 reg_st1, reg_st2, reg_st3;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS1, &reg_st1);
	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS2, &reg_st2);
	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS3, &reg_st3);
	dev_info(charger->dev, "%s: STATUS1(0x%02x), STATUS2(0x%02x), STATUS3(0x%02x)\n",
		__func__, reg_st1, reg_st2, reg_st3);

	if (reg_st2 & (0x1 << 5)) { /* check: Top-off */
		status = POWER_SUPPLY_STATUS_FULL;
	} else if (reg_st2 & (0x1 << 3)) {  /* check: Charging ON */
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (reg_st1 & (0x1 << 0)) { /* check: VBUS_POK */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	return status;
}

static int psy_chg_get_health(struct sm5713_charger_data *charger)
{
	u8 reg;
	int health = POWER_SUPPLY_HEALTH_GOOD;

	if (charger->is_charging) {
		chg_set_wdt_tmr_reset(charger);
	}
	chg_print_regmap(charger);  /* please keep this log message */

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS1, &reg);

	if (reg & (0x1 << 0)) {
		charger->unhealth_cnt = 0;
		health = POWER_SUPPLY_HEALTH_GOOD;
	} else {
		if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT) {
			health = POWER_SUPPLY_HEALTH_GOOD;
			charger->unhealth_cnt++;
		} else {
			if (reg & (0x1 << 2)) {
				health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			} else if (reg & (0x1 << 1)) {
				health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
			}
		}
	}

	return health;
}

static int psy_chg_get_charge_type(struct sm5713_charger_data *charger)
{
	int charge_type;
	u8 reg;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS2, &reg);

	if (charger->is_charging) {
		if (charger->slow_rate_chg_mode) {
			dev_info(charger->dev, "%s: slow rate charge mode\n", __func__);
			charge_type = POWER_SUPPLY_CHARGE_TYPE_SLOW;
		} else {
			charge_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
	} else {
		charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return charge_type;
}

static int psy_chg_get_present(struct sm5713_charger_data *charger)
{
	u8 reg;

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS2, &reg);

	return (reg & (0x1 << 2)) ? 0 : 1;
}

static int sm5713_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sm5713_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = psy_chg_get_online(charger);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = psy_chg_get_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = psy_chg_get_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX: /* get input current which was set */
		val->intval = charger->input_current;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG: /* get input current which was read */
		val->intval = chg_get_input_current_limit(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW: /* get charge current which was set */
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT: /* get charge current which was read */
		val->intval = chg_get_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		val->intval = chg_get_topoff_current(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = psy_chg_get_charge_type(charger);
		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chg_get_regulation_voltage(charger);
		break;
#endif
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = psy_chg_get_present(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = charger->charge_mode;
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			chg_print_regmap(charger);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void psy_chg_set_charging_enable(struct sm5713_charger_data *charger, int charge_mode)
{
	int buck_off = false;
	u8 reg;
	bool buck_off_status = (sm5713_charger_oper_get_current_status() & (0x1 << SM5713_CHARGER_OP_EVENT_SUSPEND)) ? 1 : 0;

	dev_info(charger->dev, "charger_mode changed [%d] -> [%d]\n", charger->charge_mode, charge_mode);
	charger->charge_mode = charge_mode;

	if (factory_mode) {
		pr_info("%s: Factory Mode Skip charging enable Control\n", __func__);
		return;
	}

	switch (charger->charge_mode) {
	case SEC_BAT_CHG_MODE_BUCK_OFF:
		buck_off = true;
	case SEC_BAT_CHG_MODE_CHARGING_OFF:
		charger->is_charging = false;
		break;
	case SEC_BAT_CHG_MODE_CHARGING:
		charger->is_charging = true;
		break;
	}

	if (charger->is_charging) {
		sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS2, &reg);
		if (reg & 0x80) { /* reset wdt expired status and re-init wdt */
			chg_set_wdtcntl_reset(charger);
			chg_set_wdt_timer(charger, WDT_TIME_S_90);
		}
	}

	chg_set_wdt_enable(charger, charger->is_charging);
	chg_set_enq4fet(charger, charger->is_charging);
	if (buck_off != buck_off_status) {
		sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_SUSPEND, buck_off);
	}
}

static void psy_chg_set_online(struct sm5713_charger_data *charger, int cable_type)
{
	dev_info(charger->dev, "[start] cable_type(%d->%d), op_mode(%d), op_status(0x%x)",
			charger->cable_type, cable_type, sm5713_charger_oper_get_current_op_mode(),
			sm5713_charger_oper_get_current_status());

	charger->slow_rate_chg_mode = false;
	charger->cable_type = cable_type;

	if (charger->cable_type == SEC_BATTERY_CABLE_NONE ||
			charger->cable_type == SEC_BATTERY_CABLE_UNKNOWN) {
		sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_VBUSIN, 0);
		sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_PWR_SHAR, 0);
		sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_USB_OTG, 0);

		/* set default input current */
		chg_set_input_current_limit(charger, 500);

		if (charger->irq_aicl_enabled == 0) {
			u8 reg_data;
			charger->irq_aicl_enabled = 1;
			enable_irq(charger->irq_aicl);
			sm5713_read_reg(charger->i2c, SM5713_CHG_REG_INTMSK2, &reg_data);
			pr_info("%s: enable aicl : 0x%x\n", __func__, reg_data);
		}
	} else {
		if (charger->cable_type != SEC_BATTERY_CABLE_OTG &&
			charger->cable_type != SEC_BATTERY_CABLE_POWER_SHARING)
			sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_VBUSIN, 1);

		if (is_hv_wire_type(charger->cable_type) ||
			(charger->cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)) {
			if (charger->irq_aicl_enabled == 1) {
				u8 reg_data;

				charger->irq_aicl_enabled = 0;
				disable_irq_nosync(charger->irq_aicl);
				cancel_delayed_work_sync(&charger->aicl_work);
				wake_unlock(&charger->aicl_wake_lock);
				sm5713_read_reg(charger->i2c, SM5713_CHG_REG_INTMSK2, &reg_data);
				pr_info("%s: disable aicl : 0x%x\n", __func__, reg_data);
				charger->slow_rate_chg_mode = false;
			}
		}	
	}

	dev_info(charger->dev, "[end] op_mode(%d), op_status(0x%x)\n",
			sm5713_charger_oper_get_current_op_mode(),
			sm5713_charger_oper_get_current_status());
}

static void psy_chg_set_otg_control(struct sm5713_charger_data *charger, bool enable)
{
	if (enable == charger->otg_on) {
		return;
	}
	sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_USB_OTG, enable);
	charger->otg_on = enable;
	power_supply_changed(charger->psy_otg);
}

static int sm5713_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sm5713_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		psy_chg_set_charging_enable(charger, val->intval);
		chg_print_regmap(charger);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		psy_chg_set_online(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		dev_info(charger->dev, "input limit changed [%dmA] -> [%dmA]\n",
			charger->input_current, val->intval);
		charger->input_current = val->intval;
		chg_set_input_current_limit(charger, charger->input_current);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		dev_info(charger->dev, "charging current changed [%dmA] -> [%dmA]\n",
			charger->charging_current, val->intval);
		charger->charging_current = val->intval;
		chg_set_charging_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		break;
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		chg_set_topoff_current(charger, val->intval);
		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		dev_info(charger->dev, "float voltage changed [%dmV] -> [%dmV]\n",
			charger->pdata->chg_float_voltage, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		chg_set_batreg(charger, charger->pdata->chg_float_voltage);
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		dev_info(charger->dev, "OTG_CONTROL=%s\n", val->intval ? "ON" : "OFF");
		psy_chg_set_otg_control(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* if jig attached, */
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
	{
		u8 reg;
		sm5713_charger_enable_aicl_irq(charger);
		sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS2, &reg);
		if (reg & (0x1 << 0))
			queue_delayed_work(charger->wqueue, &charger->aicl_work, msecs_to_jiffies(50));
	}
		break;
#if defined(CONFIG_AFC_CHARGER_MODE)
	case POWER_SUPPLY_PROP_AFC_CHARGER_MODE:
		muic_hv_charger_init();
		break;
#endif
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		if (val->intval)
			chg_set_en_bypass_mode(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_FACTORY_VOLTAGE_REGULATION:
			pr_info("%s: factory voltage regulation (%d)\n", __func__, val->intval);
			chg_set_batreg(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE:
			chg_set_en_bypass_mode(charger, val->intval);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sm5713_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sm5713_charger_data *charger =
		power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->otg_on;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sm5713_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sm5713_charger_data *charger =
		power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		dev_info(charger->dev, "%s: OTG %s\n", __func__,
			val->intval ? "ON" : "OFF");
		psy_chg_set_otg_control(charger, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline u8 _calc_limit_current_offset_to_mA(unsigned short mA)
{
	unsigned char offset;

	if (mA < 100) {
		offset = 0x00;
	} else {
		offset = ((mA - 100) / 25) & 0x7F;
	}

	return offset;
}

static inline int _reduce_input_limit_current(struct sm5713_charger_data *charger)
{
	int input_limit = chg_get_input_current_limit(charger);
	u8 offset = _calc_limit_current_offset_to_mA(input_limit - REDUCE_CURRENT_STEP);

	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_VBUSCNTL, ((offset & 0x7F) << 0), (0x7F << 0));

	charger->input_current = chg_get_input_current_limit(charger);

	dev_info(charger->dev, "reduce input-limit: [%dmA] to [%dmA]\n",
			input_limit, charger->input_current);

	return charger->input_current;
}

static inline void _check_slow_rate_charging(struct sm5713_charger_data *charger)
{
	union power_supply_propval value;

	if (charger->input_current <= SLOW_CHARGING_CURRENT_STANDARD &&
			charger->cable_type != SEC_BATTERY_CABLE_NONE) {

		dev_info(charger->dev, "slow-rate charging on : input current(%dmA), cable-type(%d)\n",
			charger->input_current, charger->cable_type);

		charger->slow_rate_chg_mode = true;
		value.intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
		psy_do_property("battery", set, POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	}
}


static void aicl_work(struct work_struct *work)
{
	struct sm5713_charger_data *charger = container_of(work, struct sm5713_charger_data, aicl_work.work);
	int input_limit;
	bool aicl_on = false;
	u8 reg, aicl_cnt = 0;

	dev_info(charger->dev, "%s - start\n", __func__);

	mutex_lock(&charger->charger_mutex);
	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS2, &reg);
	while ((reg & (0x1 << 0)) && charger->cable_type != SEC_BATTERY_CABLE_NONE) {
		if (++aicl_cnt >= 2) {
			input_limit = _reduce_input_limit_current(charger);
			aicl_on = true;
			if (input_limit <= MINIMUM_INPUT_CURRENT) {
				break;
			}
			aicl_cnt = 0;
		}
		msleep(50);
		sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS2, &reg);
	}

	if (aicl_on) {
		union power_supply_propval value;
		value.intval = input_limit;
		psy_do_property("battery", set,
			POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);
	}
	_check_slow_rate_charging(charger);

	mutex_unlock(&charger->charger_mutex);
	wake_unlock(&charger->aicl_wake_lock);

	dev_info(charger->dev, "%s - done\n", __func__);
}

static irqreturn_t chg_vbuspok_isr(int irq, void *data)
{
	struct sm5713_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_aicl_isr(int irq, void *data)
{
	struct sm5713_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	wake_lock(&charger->aicl_wake_lock);
	queue_delayed_work(charger->wqueue, &charger->aicl_work, msecs_to_jiffies(50));

	return IRQ_HANDLED;
}

static void sm5713_charger_enable_aicl_irq(struct sm5713_charger_data *charger)
{
	int ret;
	u8 reg_data;

	ret = request_threaded_irq(charger->irq_aicl, NULL,
			chg_aicl_isr, 0, "aicl-irq", charger);
	if (ret < 0) {
		charger->irq_aicl_enabled = -1;
		dev_err(charger->dev, "%s: fail to request aicl-irq:%d (ret=%d)\n",
					__func__, charger->irq_aicl, ret);
	} else {
		charger->irq_aicl_enabled = 1;
		sm5713_read_reg(charger->i2c, SM5713_CHG_REG_INTMSK2, &reg_data);
		pr_info("%s: enable aicl : 0x%x\n", __func__, reg_data);
	}
}

static irqreturn_t chg_done_isr(int irq, void *data)
{
	struct sm5713_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);
	if (factory_mode) {
		pr_info("%s: Factory Mode Skip chg done\n", __func__);
		return IRQ_HANDLED;
	}

	/* Toggle ENQ4FET for Re-cycling charger loop */
	chg_set_enq4fet(charger, 0);
	msleep(10);
	chg_set_enq4fet(charger, 1);

	return IRQ_HANDLED;
}

static irqreturn_t chg_vsysovp_isr(int irq, void *data)
{
	struct sm5713_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_vbusshort_isr(int irq, void *data)
{
	struct sm5713_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_vbusuvlo_isr(int irq, void *data)
{
	struct sm5713_charger_data *charger = data;
	u8 reg;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_FACTORY1, &reg);
	if (reg & 0x02) {
		dev_info(charger->dev, "%s: bypass mode enabled\n",
			__func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t chg_otgfail_isr(int irq, void *data)
{
	struct sm5713_charger_data *charger = data;
	u8 reg;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct otg_notify *o_notify;
	o_notify = get_otg_notify();
#endif

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	sm5713_read_reg(charger->i2c, SM5713_CHG_REG_STATUS3, &reg);
	if (reg & 0x04) {
		dev_info(charger->dev, "%s: otg overcurrent limit\n",
			__func__);
		/* send otg ocp noti */
#ifdef CONFIG_USB_HOST_NOTIFY
		if (o_notify)
			send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
#endif
		psy_chg_set_otg_control(charger, false);
	}

	return IRQ_HANDLED;
}

static inline void sm5713_chg_init(struct sm5713_charger_data *charger)
{
    chg_set_aicl(charger, 1, AICL_TH_V_4_5);
    chg_set_ocp_current(charger, charger->pdata->chg_ocp_current);
    chg_set_batreg(charger, charger->pdata->chg_float_voltage);
    chg_set_iq3limit(charger, BST_IQ3LIMIT_C_4_0);
    chg_set_wdt_timer(charger, WDT_TIME_S_90);
    chg_set_topoff_timer(charger, TOPOFF_TIME_M_45);
    chg_set_autostop(charger, 1);

	sm5713_update_reg(charger->i2c, SM5713_CHG_REG_FACTORY1, (0x0 << 6), (0x3 << 6));

    chg_print_regmap(charger);

	dev_info(charger->dev, "%s: init done.\n", __func__);
}

static int sm5713_charger_parse_dt(struct device *dev,
	struct sm5713_charger_platform_data *pdata)
{
	struct device_node *np;
	int ret = 0;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		dev_err(dev, "%s: can't find battery node\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
					   &pdata->chg_float_voltage);
		if (ret) {
			dev_info(dev, "%s: battery,chg_float_voltage is Empty\n", __func__);
			pdata->chg_float_voltage = 4200;
		}
		pr_info("%s: battery,chg_float_voltage is %d\n",
			__func__, pdata->chg_float_voltage);

		ret = of_property_read_u32(np, "battery,chg_ocp_current",
					   &pdata->chg_ocp_current);
		if (ret) {
			pr_info("%s: battery,chg_ocp_current is Empty\n", __func__);
			pdata->chg_ocp_current = 4500; /* mA */
		}
		pr_info("%s: battery,chg_ocp_current is %d\n", __func__,
			pdata->chg_ocp_current);

	}

	dev_info(dev, "%s: parse dt done.\n", __func__);
	return 0;
}

/* if need to set sm5713 pdata */
static struct of_device_id sm5713_charger_match_table[] = {
	{ .compatible = "samsung,sm5713-charger",},
	{},
};

static const struct power_supply_desc sm5713_charger_power_supply_desc = {
	.name           = "sm5713-charger",
	.type           = POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property   = sm5713_chg_get_property,
	.set_property   = sm5713_chg_set_property,
	.properties     = sm5713_charger_props,
	.num_properties = ARRAY_SIZE(sm5713_charger_props),
};

static const struct power_supply_desc otg_power_supply_desc = {
	.name			= "otg",
	.type			= POWER_SUPPLY_TYPE_OTG,
	.get_property	= sm5713_otg_get_property,
	.set_property	= sm5713_otg_set_property,
	.properties		= sm5713_otg_props,
	.num_properties	= ARRAY_SIZE(sm5713_otg_props),
};

static int sm5713_charger_probe(struct platform_device *pdev)
{
	struct sm5713_dev *sm5713 = dev_get_drvdata(pdev->dev.parent);
	struct sm5713_platform_data *pdata = dev_get_platdata(sm5713->dev);
	struct sm5713_charger_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	dev_info(&pdev->dev, "%s: probe start\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->dev = &pdev->dev;
	charger->i2c = sm5713->charger;
	charger->otg_on = false;
	mutex_init(&charger->charger_mutex);

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		dev_err(&pdev->dev, "%s: failed to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = sm5713_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0) {
		goto err_parse_dt;
	}
	platform_set_drvdata(pdev, charger);

	charger->irq_aicl_enabled = -1;

	sm5713_chg_init(charger);
	sm5713_charger_oper_table_init(sm5713);

	/* Init work_queue, wake_lock for Slow-rate-charging */
	charger->wqueue = create_singlethread_workqueue(dev_name(charger->dev));
	if (!charger->wqueue) {
		dev_err(charger->dev, "%s: fail to create workqueue\n", __func__);
		return -ENOMEM;
	}
	charger->slow_rate_chg_mode = false;
	INIT_DELAYED_WORK(&charger->aicl_work, aicl_work);
	wake_lock_init(&charger->aicl_wake_lock, WAKE_LOCK_SUSPEND, "charger-aicl");

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = sm5713_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sm5713_supplied_to),

	charger->psy_chg = power_supply_register(&pdev->dev, &sm5713_charger_power_supply_desc, &psy_cfg);
	if (!charger->psy_chg) {
		dev_err(&pdev->dev, "%s: failed to power supply charger register", __func__);
		goto err_power_supply_register;
	}

	charger->psy_otg = power_supply_register(&pdev->dev, &otg_power_supply_desc, &psy_cfg);
	if (!charger->psy_otg) {
		dev_err(&pdev->dev, "%s: failed to power supply otg register ", __func__);
		goto err_power_supply_register_otg;
	}

	ret = sm5713_chg_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		dev_err(charger->dev, "%s : Failed to create_attrs\n", __func__);
		goto err_reg_irq;
	}

	/* Request IRQs */
	charger->irq_vbuspok = pdata->irq_base + SM5713_CHG_IRQ_INT1_VBUSPOK;
	ret = request_threaded_irq(charger->irq_vbuspok, NULL,
			chg_vbuspok_isr, 0, "vbuspok-irq", charger);
	if (ret < 0) {
		dev_err(sm5713->dev, "%s: fail to request vbuspok-irq:%d (ret=%d)\n",
					__func__, charger->irq_vbuspok, ret);
		goto err_reg_irq;
	}

	charger->irq_aicl = pdata->irq_base + SM5713_CHG_IRQ_INT2_AICL;

	charger->irq_done = pdata->irq_base + SM5713_CHG_IRQ_INT2_DONE;
	ret = request_threaded_irq(charger->irq_done, NULL,
			chg_done_isr, 0, "done-irq", charger);
	if (ret < 0) {
		dev_err(sm5713->dev, "%s: fail to request done-irq:%d (ret=%d)\n",
			__func__, charger->irq_done, ret);
		goto err_reg_irq;
	}

	charger->irq_vsysovp = pdata->irq_base + SM5713_CHG_IRQ_INT3_VSYSOVP;
	ret = request_threaded_irq(charger->irq_vsysovp, NULL,
			chg_vsysovp_isr, 0, "vsysovp-irq", charger);
	if (ret < 0) {
		dev_err(sm5713->dev, "%s: fail to request vsysovp-irq:%d (ret=%d)\n",
			__func__, charger->irq_vsysovp, ret);
		goto err_reg_irq;
	}

	charger->irq_vbusshort = pdata->irq_base + SM5713_CHG_IRQ_INT6_VBUSSHORT;
	ret = request_threaded_irq(charger->irq_vbusshort, NULL,
			chg_vbusshort_isr, 0, "vbusshort-irq", charger);
	if (ret < 0) {
		dev_err(sm5713->dev, "%s: fail to request vbusshort-irq:%d (ret=%d)\n",
			__func__, charger->irq_vbusshort, ret);
		goto err_reg_irq;
	}

	charger->irq_vbusuvlo = pdata->irq_base + SM5713_CHG_IRQ_INT1_VBUSUVLO;
	ret = request_threaded_irq(charger->irq_vbusuvlo, NULL,
			chg_vbusuvlo_isr, 0, "vbusuvlo-irq", charger);
	if (ret < 0) {
		dev_err(sm5713->dev, "%s: fail to request vbusuvlo-irq:%d (ret=%d)\n",
			__func__, charger->irq_vbusuvlo, ret);
		goto err_reg_irq;
	}

	charger->irq_otgfail = pdata->irq_base + SM5713_CHG_IRQ_INT3_OTGFAIL;
	ret = request_threaded_irq(charger->irq_otgfail, NULL,
			chg_otgfail_isr, 0, "otgfail-irq", charger);
	if (ret < 0) {
		dev_err(sm5713->dev, "%s: fail to request otgfail-irq:%d (ret=%d)\n",
			__func__, charger->irq_otgfail, ret);
		goto err_reg_irq;
	}

	dev_info(&pdev->dev, "%s: probe done.\n", __func__);

	return 0;

err_reg_irq:
	power_supply_unregister(charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(charger->psy_chg);
err_power_supply_register:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->charger_mutex);
	kfree(charger);
	return ret;
}

static int sm5713_charger_remove(struct platform_device *pdev)
{
	struct sm5713_charger_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);

	mutex_destroy(&charger->charger_mutex);

	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int sm5713_charger_suspend(struct device *dev)
{
	return 0;
}

static int sm5713_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define sm5713_charger_suspend NULL
#define sm5713_charger_resume NULL
#endif

static void sm5713_charger_shutdown(struct platform_device *pdev)
{
	struct sm5713_charger_data *charger =
		platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	if (charger->i2c) {
		if (!factory_mode) {
			u8 reg;

			/* disable charger */
			chg_set_enq4fet(charger, false);
			sm5713_update_reg(charger->i2c, SM5713_CHG_REG_CNTL2, 0x05, 0x0F);
			/* set input current 500mA */
			chg_set_input_current_limit(charger, 500);
			/* disable bypass mode */
			sm5713_read_reg(charger->i2c, SM5713_CHG_REG_FACTORY1, &reg);
			if (reg & 0x02) {
				pr_info("%s: bypass mode is enabled\n", __func__);
				chg_set_en_bypass_mode(charger, false);
			}
		}
	} else {
		pr_err("%s: not sm5713 i2c client", __func__);
	}

	pr_info("%s: --\n", __func__);
}

static SIMPLE_DEV_PM_OPS(sm5713_charger_pm_ops, sm5713_charger_suspend,
		sm5713_charger_resume);

static struct platform_driver sm5713_charger_driver = {
	.driver = {
		.name	        = "sm5713-charger",
		.owner	        = THIS_MODULE,
		.of_match_table = sm5713_charger_match_table,
		.pm		        = &sm5713_charger_pm_ops,
	},
	.probe		= sm5713_charger_probe,
	.remove		= sm5713_charger_remove,
	.shutdown	= sm5713_charger_shutdown,
};

static int __init sm5713_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sm5713_charger_driver);

	return ret;
}
module_init(sm5713_charger_init);

static void __exit sm5713_charger_exit(void)
{
	platform_driver_unregister(&sm5713_charger_driver);
}
module_exit(sm5713_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for SM5713");
