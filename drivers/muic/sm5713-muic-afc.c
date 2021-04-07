/*
 * sm5713-muic-afc.c - afc driver for the SiliconMitus sm5713
 *
 *  Copyright (C) 2017 SiliconMitus
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * This driver is based on max77843-muic-afc.c
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/device.h>
#include <linux/battery/sec_battery.h>

#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include <linux/muic/sm5713-muic.h>


#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#define SM5713_MUIC_REG_AFCTASTATUS	0x2D
#define SM5713_MUIC_REG_REVID1		0x3e

#define SM5713_MUIC_AFC_TA		0x00
#define SM5713_MUIC_QC20		0x01

static struct sm5713_muic_data *afc_init_data;

/* To make AFC work properly on boot */
static int is_charger_ready;
static struct work_struct muic_afc_init_work;

static int afc_prepare_afctxd;
static int afc_prepare_qc20;

static int sm5713_muic_voltage_control(int afctxd, int qc20)
{
	struct sm5713_muic_data *muic_data = afc_init_data;

	pr_info("[%s:%s] old_afctxd(0x%x), afctxd(0x%x), qc20(0x%x)\n",
			MUIC_DEV_NAME, __func__, muic_data->old_afctxd,
			afctxd, qc20);

	muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
	muic_notifier_attach_attached_dev(muic_data->attached_dev);

	afc_prepare_afctxd = afctxd;
	afc_prepare_qc20 = qc20;

	cancel_delayed_work(&muic_data->afc_prepare_work);
	schedule_delayed_work(&muic_data->afc_prepare_work,
			msecs_to_jiffies(100)); /* 100ms */
	pr_info("[%s:%s] afc_prepare_work(afctxd=0x%x,qc20=%d) start\n",
			MUIC_DEV_NAME, __func__,
			afc_prepare_afctxd, afc_prepare_qc20);

	return 0;
}

static int muic_disable_afc(int disable)
{
	struct sm5713_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;

	pr_info("[%s:%s] disable: %d\n", MUIC_DEV_NAME, __func__, disable);

	if (disable) { /* AFC disable : 9V(12V) -> 5V */
		switch (muic_data->attached_dev) {
		case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
		case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
			pr_info("[%s:%s] attached_dev(%d)\n", MUIC_DEV_NAME,
					__func__, muic_data->attached_dev);

			if (muic_data->attached_dev ==
					ATTACHED_DEV_QC_CHARGER_9V_MUIC) {
				muic_data->old_afctxd = muic_data->qc20_vbus;
				sm5713_muic_voltage_control(SM5713_ENQC20_5V,
						SM5713_MUIC_QC20);
			} else {
				muic_data->old_afctxd = sm5713_i2c_read_byte(
						i2c, SM5713_MUIC_REG_AFCTXD);
				sm5713_muic_voltage_control(SM5713_MUIC_HV_5V,
						SM5713_MUIC_AFC_TA);
			}
			break;
		default:
			pr_info("[%s:%s] skip: attached_dev(%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
			break;
		}
	} else { /* AFC enable : 5V -> 9V(12V) */
#if defined(CONFIG_MUIC_SUPPORT_CCIC)
		if (muic_data->ccic_afc_state == SM5713_MUIC_AFC_ABNORMAL) {
			pr_info("[%s:%s] ccic abnormal: AFC(QC20) skip\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}
#endif

		switch (muic_data->attached_dev) {
		case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
		case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
		case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
			pr_info("[%s:%s] attached_dev(%d), old_afctxd(0x%x)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev,
					muic_data->old_afctxd);

			if (muic_data->attached_dev ==
					ATTACHED_DEV_QC_CHARGER_5V_MUIC) {
				muic_data->old_afctxd = muic_data->qc20_vbus;
				sm5713_muic_voltage_control(SM5713_ENQC20_9V,
						SM5713_MUIC_QC20);
			} else {
				if (muic_data->old_afctxd == 0x00) {
					sm5713_muic_voltage_control(
							SM5713_MUIC_HV_9V,
							SM5713_MUIC_AFC_TA);
				} else {
					sm5713_muic_voltage_control(
							muic_data->old_afctxd,
							SM5713_MUIC_AFC_TA);
				}
			}
			break;
		default:
			pr_info("[%s:%s] skip: attached_dev(%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
			break;
		}
	}

	return ret;
}

int muic_request_disable_afc_state(void)
{
	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	muic_disable_afc(1); /* 9V(12V) -> 5V */

	return 0;
}
EXPORT_SYMBOL(muic_request_disable_afc_state);

int muic_check_fled_state(bool enable, u8 mode)
{
	struct sm5713_muic_data *muic_data = afc_init_data;

	pr_info("[%s:%s] enable(%d), mode(%d)\n", MUIC_DEV_NAME, __func__,
			enable, mode);

	if (mode == FLED_MODE_TORCH) { /* torch */
		cancel_delayed_work(&muic_data->afc_torch_work);
		pr_info("[%s:%s] afc_torch_work cancel\n",
				MUIC_DEV_NAME, __func__);

		muic_data->fled_torch_enable = enable;
	} else if (mode == FLED_MODE_FLASH) { /* flash */
		muic_data->fled_flash_enable = enable;
	}

	pr_info("[%s:%s] fled_torch_enable(%d), fled_flash_enable(%d)\n",
			MUIC_DEV_NAME, __func__, muic_data->fled_torch_enable,
			muic_data->fled_flash_enable);

	if ((muic_data->fled_torch_enable == false) &&
			(muic_data->fled_flash_enable == false)) {
		if (muic_data->hv_voltage == 5) {
			pr_info("[%s:%s] skip high voltage setting\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}
		if ((mode == FLED_MODE_TORCH) && (enable == false)) {
			cancel_delayed_work(&muic_data->afc_torch_work);
			schedule_delayed_work(&muic_data->afc_torch_work,
					msecs_to_jiffies(5000));
			pr_info("[%s:%s] afc_torch_work start(5sec)\n",
					MUIC_DEV_NAME, __func__);
		} else {
			muic_disable_afc(0);  /* 5V -> 9V(12V) */
		}
	}

	return 0;
}
EXPORT_SYMBOL(muic_check_fled_state);

int sm5713_set_afc_ctrl_reg(struct sm5713_muic_data *muic_data, int shift,
		bool on)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val = 0;
	int ret = 0;

	pr_info("[%s:%s] Register[%d], set [%d]\n", MUIC_DEV_NAME, __func__,
			shift, on);
	ret = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_AFCCNTL);
	if (ret < 0)
		pr_err("[%s:%s](%d)\n", MUIC_DEV_NAME, __func__, ret);
	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		pr_debug("[%s:%s] reg_val(0x%x) != AFC_CTRL reg(0x%x), update reg\n",
			MUIC_DEV_NAME, __func__, reg_val, ret);

		ret = sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_AFCCNTL,
				reg_val);
		if (ret < 0)
			pr_err("[%s:%s] err write AFC_CTRL(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
	} else {
		pr_debug("[%s:%s] (0x%x), just return\n",
			MUIC_DEV_NAME, __func__, ret);
		return 0;
	}

	ret = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_AFCCNTL);
	if (ret < 0)
		pr_err("[%s:%s] err read AFC_CTRL(%d)\n",
			MUIC_DEV_NAME, __func__, ret);
	else
		pr_debug("[%s:%s] AFC_CTRL reg after change(0x%x)\n",
			MUIC_DEV_NAME, __func__, ret);

	return ret;
}

static int muic_get_vbus_value(void)
{
	struct sm5713_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	int vbus_voltage = 0, voltage1 = 0, voltage2 = 0;
	int irqvbus = 0;
	int intmask2 = 0;
	int retry = 0;

	pr_info("[%s:%s] attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	intmask2 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_INTMASK2);
	intmask2 = intmask2 | 0x04;
	sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_INTMASK2, intmask2);

	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_VBUS_READ, 1);

	for (retry = 0; retry < 10 ; retry++) {
		msleep(20);
		irqvbus = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_INT2);
		if (irqvbus & INT2_VBUS_UPDATE_MASK) {
			pr_info("[%s:%s] VBUS update Success(%d), retry: %d)\n",
				MUIC_DEV_NAME, __func__, irqvbus, retry);
			break;
		}
		pr_info("[%s:%s] VBUS update Fail(%d), retry: %d)\n",
			MUIC_DEV_NAME, __func__, irqvbus, retry);
	}
	intmask2 = intmask2 & 0xFB;
	sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_INTMASK2, intmask2);

	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_VBUS_READ, 0);

	if (retry >= 10) {
		pr_info("[%s:%s] VBUS update Failed(%d)\n",
				MUIC_DEV_NAME, __func__, retry);
		return 0;
	}
	voltage1 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_VBUS_VOLTAGE1);
	voltage2 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_VBUS_VOLTAGE2);
	vbus_voltage = voltage1*1000 + (voltage2*3900)/1000;

	pr_info("[%s:%s] voltage1=[0x%02x], voltage2=[0x%02x], vbus_voltage=%d mV\n",
		MUIC_DEV_NAME, __func__, voltage1, voltage2, vbus_voltage);

	return vbus_voltage;
}

static void muic_afc_prepare_work(struct work_struct *work)
{
	struct sm5713_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	union power_supply_propval value;
	int ret = 0, reg_val = 0;
	int retry = 0;
	int intmask2 = 0;
	int irqafc = 0;
	int afcstatus = 0, afc_dpreset = 0;
	int vbus_voltage = 0;
	int vbus_txd_voltage = 0;
	int voltage_min = 0, voltage_max = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	for (retry = 0; retry < 10; retry++) {
		psy_do_property("sm5713-charger", get,
				POWER_SUPPLY_PROP_CURRENT_MAX, value);
		if (value.intval <= 500) {
			pr_info("[%s:%s]PREPARE Success(%d), retry(%d)\n",
					MUIC_DEV_NAME, __func__,
					value.intval, retry);
			break;
		}

		if (value.intval == 1650) {
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
		}

		pr_info("[%s:%s]PREPARE fail(%d), retry(%d)\n",
				MUIC_DEV_NAME, __func__, value.intval, retry);
		msleep(50);
	}

	mutex_lock(&muic_data->afc_mutex);

	/* QC20 */
	if (afc_prepare_qc20 == SM5713_MUIC_QC20) {
		pr_info("[%s:%s]QC20: afctxd(0x%x)\n", MUIC_DEV_NAME, __func__,
				afc_prepare_afctxd);

		ret = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_AFCCNTL);
		reg_val = (ret & 0x3F) | (afc_prepare_afctxd<<6);
		sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_AFCCNTL, reg_val);
		pr_info("[%s:%s] read REG_AFCCNTL=0x%x ,  write REG_AFCCNTL=0x%x , qc20_vbus=%d\n",
				MUIC_DEV_NAME, __func__, ret, reg_val,
				afc_prepare_afctxd);

		muic_data->qc20_vbus = afc_prepare_afctxd;

		if (muic_data->qc20_vbus == SM5713_ENQC20_5V) {
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_5V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
		} else if (muic_data->qc20_vbus == SM5713_ENQC20_9V) {
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
		} else {
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
		}
	} else { /* AFC TA */
		pr_info("[%s:%s] AFC: afctxd(0x%x), old_afctxd(0x%x)\n",
				MUIC_DEV_NAME, __func__, afc_prepare_afctxd,
				muic_data->old_afctxd);

		sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_AFCTXD,
				afc_prepare_afctxd);

		/* AFC(INT2) mask */
		intmask2 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_INTMASK2);
		intmask2 = intmask2 | 0x3F;
		sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_INTMASK2, intmask2);

		/* ENAFC set '1' */
		sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);

		afc_dpreset = 0;

		for (retry = 0; retry < 30 ; retry++) {
			msleep(50);
			irqafc = sm5713_i2c_read_byte(i2c,
					SM5713_MUIC_REG_INT2);
			if (irqafc & INT2_AFC_ACCEPTED_MASK) {
				pr_info("[%s:%s] AFC_ACCEPTED Success(0x%x), retry(%d)\n",
						MUIC_DEV_NAME, __func__,
						irqafc, retry);
				break;
			}

			if ((irqafc & INT2_AFC_TA_ATTACHED_MASK) &&
					(afc_dpreset == 1)) {
				pr_info("[%s:%s] AFC_TA_ATTACHED(0x%x), retry(%d)\n",
						MUIC_DEV_NAME, __func__,
						irqafc, retry);
				break;
			}

			if (irqafc & INT2_AFC_ERROR_MASK) {
				/* ENAFC set '0' */
				sm5713_set_afc_ctrl_reg(muic_data,
						AFCCTRL_ENAFC, 0);
				/* read AFC_STATUS */
				afcstatus = sm5713_i2c_read_byte(i2c,
						SM5713_MUIC_REG_AFCSTATUS);

				pr_info("[%s:%s] AFC_ERROR(0x%x), afcstatus(0x%x), retry(%d)\n",
						MUIC_DEV_NAME, __func__, irqafc,
						afcstatus, retry);

				/* DP_RESET '1' */
				sm5713_set_afc_ctrl_reg(muic_data,
						AFCCTRL_DP_RESET, 1);
				afc_dpreset = 1;
			} else {
				pr_info("[%s:%s] AFC_ACCEPTED Fail(0x%x), retry(%d)\n",
						MUIC_DEV_NAME, __func__,
						irqafc, retry);
			}
		}

		/* ENAFC set '0' */
		sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

		/* AFC(INT2) unmask */
		intmask2 = intmask2 & 0xC0;
		sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_INTMASK2, intmask2);

		vbus_voltage = muic_get_vbus_value();

		vbus_txd_voltage = sm5713_i2c_read_byte(i2c,
				SM5713_MUIC_REG_AFCTXD);
		pr_info("[%s:%s] AFC_TXD [0x%02x]\n", MUIC_DEV_NAME, __func__,
				vbus_txd_voltage);
		vbus_txd_voltage = 5000 + ((vbus_txd_voltage&0xF0)>>4)*1000;

		pr_info("[%s:%s] vbus_voltage:%d mV , AFC_TXD_VOLTAGE:%d mV\n",
				MUIC_DEV_NAME, __func__, vbus_voltage,
				vbus_txd_voltage);

		voltage_min = vbus_txd_voltage - 2000; /* - 2000mV */
		voltage_max = vbus_txd_voltage + 1000; /* + 1000mV */

		if ((voltage_min <= vbus_voltage) &&
				(vbus_voltage <= voltage_max)) { /* AFC DONE */
			if (vbus_txd_voltage == 12000) { /* 12V */
				muic_data->attached_dev =
					ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] AFC 12V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			} else if (vbus_txd_voltage == 9000) { /* 9V */
				muic_data->attached_dev =
					ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] AFC 9V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			} else {  /* 5V */
				muic_data->attached_dev =
					ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] AFC 5V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			}
		} else {
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
			pr_info("[%s:%s] AFC 5V (%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
		}
	}
	afc_prepare_qc20 = 0;

	mutex_unlock(&muic_data->afc_mutex);
}

int sm5713_afc_ta_attach(struct sm5713_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	union power_supply_propval value;
	int ret = 0, afctxd = 0;
	int retry = 0;

	if (!is_charger_ready) {
		pr_info("[%s:%s] charger is not ready, return\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}

	if (muic_data->pdata->afc_disable) {
		pr_info("[%s:%s] AFC is disabled by USER, return\n",
				MUIC_DEV_NAME, __func__);
		muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		return ret;
	}

	pr_info("[%s:%s] AFC_TA_ATTACHED\n", MUIC_DEV_NAME, __func__);

	/* read VBUS VALID */
	ret = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_REVID1);
	if (ret < 0)
		pr_err("[%s:%s] err read VBUS\n", MUIC_DEV_NAME, __func__);
	pr_info("[%s:%s] VBUS[0x%02x]\n", MUIC_DEV_NAME, __func__, ret);
	if ((ret&0x01) == 0x00) {
		pr_info("[%s:%s] VBUS NOT VALID [0x%02x] just return\n",
				MUIC_DEV_NAME, __func__, ret);
		return 0;
	}

	/* read clear : AFC_STATUS */
	ret = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_AFCSTATUS);
	if (ret < 0)
		pr_err("[%s:%s] err read AFC_STATUS\n",
				MUIC_DEV_NAME, __func__);
	pr_info("[%s:%s] AFC_STATUS [0x%02x]\n", MUIC_DEV_NAME, __func__, ret);

	if ((muic_data->fled_torch_enable == 1) ||
			(muic_data->fled_flash_enable == 1)) {
		pr_info("[%s:%s] FLASH or Torch On, Skip AFC\n",
				MUIC_DEV_NAME, __func__);
		muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		return 0;
	}

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	if (muic_data->ccic_afc_state == SM5713_MUIC_AFC_ABNORMAL) {
		muic_data->ccic_afc_state_count = 0;
		cancel_delayed_work(&muic_data->ccic_afc_work);
		cancel_delayed_work(&muic_data->afc_retry_work);
		schedule_delayed_work(&muic_data->ccic_afc_work,
				msecs_to_jiffies(200));
		pr_info("[%s:%s] CCIC Abnormal State, ccic_afc_work(%d) start\n",
				MUIC_DEV_NAME, __func__,
				muic_data->ccic_afc_state_count);

		return 0;
	}
#endif

	muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
	muic_notifier_attach_attached_dev(muic_data->attached_dev);

	for (retry = 0; retry < 20; retry++) {
		msleep(50);
		psy_do_property("sm5713-charger", get,
				POWER_SUPPLY_PROP_CURRENT_MAX, value);
		if (value.intval <= 500) {
			pr_info("[%s:%s]PREPARE Success(%d mA)\n",
					MUIC_DEV_NAME, __func__, value.intval);
			break;
		}
		pr_info("[%s:%s]PREPARE fail(%d mA)\n", MUIC_DEV_NAME, __func__,
				value.intval);
	}

	cancel_delayed_work(&muic_data->afc_retry_work);
	schedule_delayed_work(&muic_data->afc_retry_work,
			msecs_to_jiffies(5000)); /* 5sec */
	pr_info("[%s:%s] afc_retry_work(ATTACH) start\n",
			MUIC_DEV_NAME, __func__);

	/* voltage(9.0V)  + current(1.65A) setting : 0x46 */
	/* voltage(12.0V) + current(2.1A) setting  : 0x79 */
	afctxd = AFC_TXBYTE_9V_1_65A;
	ret = sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_AFCTXD, afctxd);
	if (ret < 0)
		pr_err("[%s:%s] err write AFC_TXD(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
	pr_info("[%s:%s] AFC_TXD [0x%02x]\n", MUIC_DEV_NAME, __func__, afctxd);

	/* ENAFC set '1' */
	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
	pr_info("[%s:%s] AFCCTRL_ENAFC 1\n", MUIC_DEV_NAME, __func__);
	muic_data->afc_retry_count = 0;
	muic_data->afc_vbus_retry_count = 0;
	return 0;
}

int sm5713_afc_ta_accept(struct sm5713_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int dev1 = 0;

	pr_info("[%s:%s] AFC_ACCEPTED\n", MUIC_DEV_NAME, __func__);

	/* ENAFC set '0' */
	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

	cancel_delayed_work(&muic_data->afc_retry_work);
	pr_info("[%s:%s] afc_retry_work(ACCEPTED) cancel\n",
			MUIC_DEV_NAME, __func__);

	if ((muic_data->fled_torch_enable == 1) ||
			(muic_data->fled_flash_enable == 1)) {
		pr_info("[%s:%s] FLASH or Torch On, AFC_ACCEPTED VBUS(9V->5V)\n",
				MUIC_DEV_NAME, __func__);
		muic_disable_afc(1); /* 9V(12V) -> 5V */
		return 0;
	}

	dev1 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_DEVICETYPE1);
	pr_info("[%s:%s] dev1 [0x%02x]\n", MUIC_DEV_NAME, __func__, dev1);
	if (dev1 & DEV_TYPE1_AFC_TA) {
		/* VBUS_READ */
		sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_VBUS_READ, 1);

		pr_info("[%s:%s] VBUS READ start(AFC)\n", MUIC_DEV_NAME,
				__func__);
		if (muic_data->attached_dev !=
				ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) {
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
		}
		muic_data->afc_vbus_retry_count = 0;
	} else {
		muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		pr_info("[%s:%s] attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
				muic_data->attached_dev);
	}

	return 0;
}

int sm5713_afc_vbus_update(struct sm5713_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int vbus_txd_voltage = 0;
	int vbus_voltage = 0, voltage1 = 0, voltage2 = 0;
	int voltage_min = 0, voltage_max = 0;
	int dev1 = 0;
	int ret = 0, reg_val = 0;

	pr_info("[%s:%s] AFC_VBUS_UPDATE\n", MUIC_DEV_NAME, __func__);

	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_VBUS_READ, 0);

	if (muic_data->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("[%s:%s] Device type is None\n",
				MUIC_DEV_NAME, __func__);
		return 0;
	}

	voltage1 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_VBUS_VOLTAGE1);
	voltage2 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_VBUS_VOLTAGE2);
	vbus_voltage = voltage1*1000 + (voltage2*3900)/1000;

	pr_info("[%s:%s] voltage1=[0x%02x], voltage2=[0x%02x], vbus_voltage=%d mV\n",
		MUIC_DEV_NAME, __func__, voltage1, voltage2, vbus_voltage);

	dev1 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_DEVICETYPE1);
	pr_info("[%s:%s] DEVICE_TYPE1 [0x%02x]\n", MUIC_DEV_NAME, __func__,
			dev1);
	if (dev1 & DEV_TYPE1_QC20_TA) { /* QC20_TA vbus update */
		if (muic_data->qc20_vbus == SM5713_ENQC20_12V) {
			voltage_min  = 10000; /* - 10000mV */
			voltage_max  = 13000; /* + 13000mV */
		} else if (muic_data->qc20_vbus == SM5713_ENQC20_9V) {
			voltage_min  = 7000;  /* - 7000mV */
			voltage_max  = 10000; /* + 10000mV */
		}

		pr_info("[%s:%s] QC20 vbus_voltage:%d mV (%d)\n",
				MUIC_DEV_NAME, __func__,
				vbus_voltage, muic_data->qc20_vbus);

		if ((voltage_min <= vbus_voltage) &&
				(vbus_voltage <= voltage_max)) { /* AFC DONE */
			if (muic_data->qc20_vbus == SM5713_ENQC20_12V) {
				muic_data->attached_dev =
					ATTACHED_DEV_QC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] QC20 12V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			} else if (muic_data->qc20_vbus == SM5713_ENQC20_9V) {
				muic_data->attached_dev =
					ATTACHED_DEV_QC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] QC20 9V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			} else if (muic_data->qc20_vbus == SM5713_ENQC20_5V) {
				muic_data->attached_dev =
					ATTACHED_DEV_QC_CHARGER_5V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] QC20 5V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			}
		} else { /* vbus fail */
			if (muic_data->afc_vbus_retry_count < 3) {
				msleep(100);
				muic_data->afc_vbus_retry_count++;

				/* VBUS_READ */
				sm5713_set_afc_ctrl_reg(muic_data,
						AFCCTRL_VBUS_READ, 1);
				pr_info("[%s:%s] [QC20] VBUS READ retry = %d\n",
					MUIC_DEV_NAME, __func__,
					muic_data->afc_vbus_retry_count);
			} else {
				msleep(100);
				muic_data->qc20_vbus = SM5713_ENQC20_9V;
				ret = sm5713_i2c_read_byte(i2c,
						SM5713_MUIC_REG_AFCCNTL);
				/* QC20 9V */
				reg_val = (ret & 0x3F) |
					(muic_data->qc20_vbus<<6);
				sm5713_i2c_write_byte(i2c,
						SM5713_MUIC_REG_AFCCNTL,
						reg_val);
				pr_info("[%s:%s] read REG_AFCCNTL=0x%x, write REG_AFCCNTL=0x%x, qc20_vbus=%d\n",
						MUIC_DEV_NAME, __func__, ret,
						reg_val, muic_data->qc20_vbus);

				if (muic_data->qc20_vbus == SM5713_ENQC20_12V) {
					/* VBUS_READ */
					sm5713_set_afc_ctrl_reg(muic_data,
							AFCCTRL_VBUS_READ, 1);
					pr_info("[%s:%s] VBUS READ start(QC20-9V)\n",
							MUIC_DEV_NAME,
							__func__);
					muic_data->afc_vbus_retry_count = 0;
				} else {
					muic_data->attached_dev =
						ATTACHED_DEV_QC_CHARGER_5V_MUIC;
					muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				}
			}
		}
	} else if (dev1 & DEV_TYPE1_AFC_TA) { /* AFC vbus update */
		vbus_txd_voltage = sm5713_i2c_read_byte(i2c,
				SM5713_MUIC_REG_AFCTXD);
		pr_info("[%s:%s] AFC_TXD [0x%02x]\n", MUIC_DEV_NAME, __func__,
				vbus_txd_voltage);
		vbus_txd_voltage = 5000 + ((vbus_txd_voltage&0xF0)>>4)*1000;

		pr_info("[%s:%s] vbus_voltage:%d mV , AFC_TXD_VOLTAGE:%d mV\n",
				MUIC_DEV_NAME, __func__,
				vbus_voltage, vbus_txd_voltage);

		voltage_min = vbus_txd_voltage - 2000; /* - 2000mV */
		voltage_max = vbus_txd_voltage + 1000; /* + 1000mV */

		if ((voltage_min <= vbus_voltage) &&
				(vbus_voltage <= voltage_max)) { /* AFC DONE */
			muic_data->afc_vbus_retry_count = 0;

			pr_info("[%s:%s] AFC done\n", MUIC_DEV_NAME, __func__);
			if (vbus_txd_voltage == 12000) { /* 12V */
				muic_data->attached_dev =
					ATTACHED_DEV_AFC_CHARGER_12V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] AFC 12V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			} else if (vbus_txd_voltage == 9000) { /* 9V */
				muic_data->attached_dev =
					ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] AFC 9V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			} else {  /* 5V */
				muic_data->attached_dev =
					ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				pr_info("[%s:%s] AFC 5V (%d)\n",
						MUIC_DEV_NAME, __func__,
						muic_data->attached_dev);
			}
		} else {
			/* VBUS_READ */
			if (muic_data->afc_vbus_retry_count < 3) {
				msleep(100);
				muic_data->afc_vbus_retry_count++;

				/* VBUS_READ */
				sm5713_set_afc_ctrl_reg(muic_data,
						AFCCTRL_VBUS_READ, 1);
				pr_info("[%s:%s] VBUS READ retry = %d\n",
					MUIC_DEV_NAME, __func__,
					muic_data->afc_vbus_retry_count);
			} else {
				muic_data->attached_dev =
					ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
				muic_notifier_attach_attached_dev(
						muic_data->attached_dev);
				muic_data->afc_vbus_retry_count = 0;
			}
		}
	} /* if (dev1 & DEV_TYPE1_QC20_TA){ // QC20_TA vbus update */

	return 0;
}

int sm5713_afc_multi_byte(struct sm5713_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int multi_byte[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int i = 0;
	int ret = 0;
	int voltage_find = 0;

	pr_info("[%s:%s] AFC_MULTI_BYTE\n", MUIC_DEV_NAME, __func__);

	/* ENAFC set '0' */
	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

	/* read AFC_RXD1 ~ RXD15 */
	voltage_find = 0;
	for (i = 0 ; i < 15 ; i++) {
		multi_byte[i] = sm5713_i2c_read_byte(i2c,
				SM5713_MUIC_REG_AFC_RXD1 + i);
		if (multi_byte[i] < 0) {
			pr_err("[%s:%s] err read AFC_RXD%d %d\n",
				MUIC_DEV_NAME, __func__, i+1, multi_byte[i]);
		}
		pr_info("[%s:%s] AFC_RXD%d [0x%02x]\n", MUIC_DEV_NAME, __func__,
				i+1, multi_byte[i]);
		if (multi_byte[i] == 0x00)
			break;
		if (i >= 1) /* voltate find */
			if (((multi_byte[i]&0xF0)>>4) >=
					((multi_byte[voltage_find]&0xF0)>>4))
				voltage_find = i;
	}

	pr_info("[%s:%s] AFC_RXD%d multi_byte[%d]=0x%02x\n",
			MUIC_DEV_NAME, __func__,
			voltage_find+1, voltage_find, multi_byte[voltage_find]);

	ret = sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_AFCTXD,
			multi_byte[voltage_find]);
	if (ret < 0)
		pr_err("[%s:%s] err write AFC_TXD(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	/* ENAFC set '1' */
	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
	pr_info("[%s:%s] AFCCTRL_ENAFC 1\n", MUIC_DEV_NAME, __func__);

	return 0;
}

int sm5713_afc_error(struct sm5713_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int value = 0;
	int dev1 = 0;
	int ret = 0, reg_val = 0;

	pr_info("[%s:%s] AFC_ERROR (%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->afc_retry_count);

	/* ENAFC set '0' */
	sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

	/* read AFC_STATUS */
	value = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_AFCSTATUS);
	if (value < 0)
		pr_err("[%s:%s] err read AFC_STATUS %d\n",
				MUIC_DEV_NAME, __func__, value);
	pr_info("[%s:%s] REG_AFCSTATUS [0x%02x]\n", MUIC_DEV_NAME, __func__,
			value);

	dev1 = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_DEVICETYPE1);

	if (muic_data->afc_retry_count < 5) {
		pr_info("[%s:%s] DEVICE_TYPE1 [0x%02x]\n",
				MUIC_DEV_NAME, __func__, dev1);
		if ((dev1 & DEV_TYPE1_QC20_TA) &&
				(muic_data->afc_retry_count >= 2)) {
			/* QC20_9V_TA */
			muic_data->qc20_vbus = SM5713_ENQC20_9V;
			ret = sm5713_i2c_read_byte(i2c,
					SM5713_MUIC_REG_AFCCNTL);
			reg_val = (ret & 0x3F) | (SM5713_ENQC20_9V<<6);
			sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_AFCCNTL,
					reg_val);
			pr_info("[%s:%s] read REG_AFCCNTL=0x%x ,  write REG_AFCCNTL=0x%x , qc20_vbus=%d\n",
				MUIC_DEV_NAME, __func__, ret, reg_val,
				muic_data->qc20_vbus);
			/* VBUS_READ */
			sm5713_set_afc_ctrl_reg(muic_data,
					AFCCTRL_VBUS_READ, 1);
			pr_info("[%s:%s] VBUS READ start(QC20-9V)\n",
					MUIC_DEV_NAME, __func__);
			msleep(50); /* 50ms delay */
		} else {
			msleep(100); /* 100ms delay */
			/* ENAFC set '1' */
			sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
			muic_data->afc_retry_count++;
			pr_info("[%s:%s] re-start AFC (afc_retry_count=%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->afc_retry_count);
		}
	} else {
		pr_info("[%s:%s]  ENAFC end = %d\n", MUIC_DEV_NAME, __func__,
				muic_data->afc_retry_count);
		if (dev1 & DEV_TYPE1_QC20_TA)
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC;
		else
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
	}
	return 0;
}

int sm5713_afc_sta_chg(struct sm5713_muic_data *muic_data)
{
	pr_info("[%s:%s] AFC_STA_CHG (attached_dev: %d)\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);

	return 0;
}

static void hv_muic_change_afc_voltage(int tx_data)
{
	struct sm5713_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	u8 val = 0;
	union power_supply_propval value;
	int retry = 0;


	pr_info("[%s:%s] change afc voltage(%x)\n",
			MUIC_DEV_NAME, __func__, tx_data);

	/* QC20 */
	if ((muic_data->attached_dev == ATTACHED_DEV_QC_CHARGER_9V_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_QC_CHARGER_5V_MUIC)) {
		switch (tx_data) {
		case SM5713_MUIC_HV_5V:
			/* QC20 5V */
			sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_QC20_9V, 0);
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_5V_MUIC;
			break;
		case SM5713_MUIC_HV_9V:
			/* QC20 9V */
			sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_QC20_9V, 1);
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			break;
		default:
			break;
		}
	} else {	/* AFC */
		val = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_AFCTXD);
		if (val == tx_data) {
			pr_info("[%s:%s] same to current voltage 0x%x\n",
					MUIC_DEV_NAME, __func__, val);
			return;
		}

		for (retry = 0; retry < 10; retry++) {
			msleep(50);
			psy_do_property("sm5713-charger", get,
					POWER_SUPPLY_PROP_CURRENT_MAX, value);
			if (value.intval <= 500) {
				pr_info("[%s:%s]PREPARE Success(%d)\n",
						MUIC_DEV_NAME, __func__,
						value.intval);
				break;
			}
			pr_info("[%s:%s]PREPARE fail(%d)\n",
					MUIC_DEV_NAME, __func__, value.intval);
		}

		cancel_delayed_work(&muic_data->afc_retry_work);
		schedule_delayed_work(&muic_data->afc_retry_work,
				msecs_to_jiffies(5000)); /* 5sec */
		pr_info("[%s:%s] afc_retry_work(afc voltage) start\n",
				MUIC_DEV_NAME, __func__);

		sm5713_i2c_write_byte(i2c, SM5713_MUIC_REG_AFCTXD, tx_data);
		/* ENAFC set '1' */
		sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
	}
}

int sm5713_muic_afc_set_voltage(int vol)
{
	int now_vol = 0;
	struct sm5713_muic_data *muic_data = afc_init_data;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		break;
	default:
		pr_info("[%s:%s] not HV charger, returnV\n",
				MUIC_DEV_NAME, __func__);
		return -EINVAL;
	}

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		now_vol = 9;
		break;
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		now_vol = 5;
		break;
	default:
		break;
	}

	if (now_vol == vol) {
		pr_info("[%s:%s] same voltage(%d), return\n",
				MUIC_DEV_NAME, __func__, vol);
		return 0;
	}

	/* do not set high voltage at below conditions */
	if (vol > 5) {
		if ((muic_data->fled_torch_enable == 1) ||
				(muic_data->fled_flash_enable == 1)) {
			pr_info("[%s:%s] FLASH or Torch On, Skip AFC\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
		if (muic_data->ccic_afc_state == SM5713_MUIC_AFC_ABNORMAL) {
			pr_info("[%s:%s] ccic abnormal: AFC(QC20) skip\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}
#endif
	}

	pr_info("[%s:%s] vol = %dV\n", MUIC_DEV_NAME, __func__, vol);
	muic_data->hv_voltage = vol;

	if (vol == 5) {
		hv_muic_change_afc_voltage(SM5713_MUIC_HV_5V);
	} else if (vol == 9) {
		hv_muic_change_afc_voltage(SM5713_MUIC_HV_9V);
	} else if (vol == 12) {
		hv_muic_change_afc_voltage(SM5713_MUIC_HV_12V);
	} else {
		pr_warn("[%s:%s]invalid value\n", MUIC_DEV_NAME, __func__);
		return -EINVAL;
	}

	return 0;
}

static void muic_afc_torch_work(struct work_struct *work)
{
	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	muic_disable_afc(0);  /* 5V -> 9V(12V) */
}


static void muic_afc_retry_work(struct work_struct *work)
{
	struct sm5713_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0, vbus = 0;

	ret = sm5713_i2c_read_byte(i2c, SM5713_MUIC_REG_AFCSTATUS);
	pr_info("[%s:%s]: Read REG_AFCSTATUS = [0x%02x]\n",
			MUIC_DEV_NAME, __func__, ret);

	pr_info("[%s:%s] attached_dev = %d\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	if (muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) {
		vbus = sm5713_i2c_read_byte(i2c, 0x3E);
		if (!(vbus & 0x01)) {
			pr_info("[%s:%s] VBUS is nothing\n",
					MUIC_DEV_NAME, __func__);
			muic_notifier_detach_attached_dev(
					muic_data->attached_dev);
			muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
			return;
		}

		pr_info("[%s:%s] [MUIC] device type is afc prepare, DP_RESET\n",
				MUIC_DEV_NAME, __func__);

		/* DP_RESET '1' */
		sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_DP_RESET, 1);
	}
}

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
static void sm5713_muic_ccic_afc_handler(struct work_struct *work)
{
	struct sm5713_muic_data *muic_data =
		container_of(work, struct sm5713_muic_data, ccic_afc_work.work);

	pr_info("[%s:%s] ccic_afc_state:%d, ccic_afc_state_count:%d\n",
			MUIC_DEV_NAME, __func__, muic_data->ccic_afc_state,
			muic_data->ccic_afc_state_count);

	if (muic_data->ccic_afc_state_count > 4) {
		cancel_delayed_work(&muic_data->ccic_afc_work);
		muic_data->attached_dev = ATTACHED_DEV_TA_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		return;
	}

	if (muic_data->ccic_afc_state == SM5713_MUIC_AFC_NORMAL) {
		pr_info("[%s:%s] SM5713_MUIC_AFC_NORMAL\n",
				MUIC_DEV_NAME, __func__);
		/* ENAFC set '1' */
		sm5713_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
		pr_info("[%s:%s] AFCCTRL_ENAFC 1\n", MUIC_DEV_NAME, __func__);
		muic_data->afc_retry_count = 0;
		muic_data->afc_vbus_retry_count = 0;
	} else {
		muic_data->ccic_afc_state_count++;
		cancel_delayed_work(&muic_data->ccic_afc_work);
		schedule_delayed_work(&muic_data->ccic_afc_work,
				msecs_to_jiffies(200));
		pr_info("[%s:%s] ccic_afc_work(%d) start\n", MUIC_DEV_NAME,
				__func__, muic_data->ccic_afc_state_count);
	}
}
#endif

static void sm5713_hv_muic_init_detect(struct work_struct *work)
{
	struct sm5713_muic_data *muic_data = afc_init_data;
	int afc_ta_attached = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	afc_ta_attached = sm5713_i2c_read_byte(muic_data->i2c,
			SM5713_MUIC_REG_AFCTASTATUS);
	pr_info("[%s:%s] REG_AFCTASTATUS:[0x%02x]\n", MUIC_DEV_NAME, __func__,
			afc_ta_attached);

	/* AFC_TA_ATTACHED */
	if (afc_ta_attached & 0x01)
		sm5713_afc_ta_attach(muic_data);
}

int sm5713_muic_charger_init(void)
{
	int ret = -EINVAL;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	if (!afc_init_data) {
		pr_info("[%s:%s] MUIC AFC is not ready.\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}

	if (is_charger_ready) {
		pr_info("[%s:%s] charger is already ready.\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}

	is_charger_ready = true;

	if (afc_init_data->attached_dev == ATTACHED_DEV_TA_MUIC)
		schedule_work(&muic_afc_init_work);

	return 0;
}

void sm5713_hv_muic_initialize(struct sm5713_muic_data *muic_data)
{
	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	afc_init_data = muic_data;

	is_charger_ready = false;

	/* To make AFC work properly on boot */
	INIT_WORK(&muic_afc_init_work, sm5713_hv_muic_init_detect);

	INIT_DELAYED_WORK(&muic_data->afc_retry_work, muic_afc_retry_work);
	INIT_DELAYED_WORK(&muic_data->afc_torch_work, muic_afc_torch_work);
	INIT_DELAYED_WORK(&muic_data->afc_prepare_work, muic_afc_prepare_work);

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	INIT_DELAYED_WORK(&muic_data->ccic_afc_work,
			sm5713_muic_ccic_afc_handler);
#endif
}

