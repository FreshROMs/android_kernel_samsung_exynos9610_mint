/*
 * Copyrights (C) 2018 Samsung Electronics, Inc.
 * Copyrights (C) 2018 Silicon Mitus, Inc.
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
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/sec_batt.h>
#include <linux/muic/muic.h>
#include <linux/usb/typec/sm5713/sm5713_pd.h>
#include <linux/usb/typec/sm5713/sm5713_typec.h>
#include <linux/mfd/sm5713-private.h>
#include <linux/usb/typec/pdic_sysfs.h>
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif /* CONFIG_VBUS_NOTIFIER */
#if defined(CONFIG_BATTERY_SAMSUNG_V2)
#include "../../../battery_v2/include/sec_charging_common.h"
#else
#include <linux/battery/sec_charging_common.h>
#endif
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
#include <linux/battery/battery_notifier.h>
#endif

#define I2C_RETRY_CNT	3

#if defined(CONFIG_CCIC_NOTIFIER)
static enum ccic_sysfs_property sm5713_sysfs_properties[] = {
	CCIC_SYSFS_PROP_CHIP_NAME,
	CCIC_SYSFS_PROP_LPM_MODE,
	CCIC_SYSFS_PROP_STATE,
	CCIC_SYSFS_PROP_RID,
	CCIC_SYSFS_PROP_CTRL_OPTION,
	CCIC_SYSFS_PROP_FW_WATER,
	CCIC_SYSFS_PROP_ACC_DEVICE_VERSION,
	CCIC_SYSFS_PROP_USBPD_IDS,
	CCIC_SYSFS_PROP_USBPD_TYPE,
#if defined(CONFIG_SEC_FACTORY)
	CCIC_SYSFS_PROP_VBUS_ADC,
#endif
};
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
static enum dual_role_property sm5713_fusb_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};
#endif

struct i2c_client *test_i2c;
static usbpd_phy_ops_type sm5713_ops;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
extern struct pdic_notifier_struct pd_noti;
#endif

static int sm5713_usbpd_reg_init(struct sm5713_phydrv_data *_data);

static int sm5713_usbpd_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	ret &= 0xff;
	*dest = ret;
	return 0;
}

static int sm5713_usbpd_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_write_byte_data(i2c, reg, value);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	return ret;
}

static int sm5713_usbpd_multi_read(struct i2c_client *i2c,
		u8 reg, int count, u8 *buf)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	return 0;
}

static int sm5713_usbpd_multi_write(struct i2c_client *i2c,
		u8 reg, int count, u8 *buf)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	return 0;
}

static void sm5713_set_dfp(struct i2c_client *i2c)
{
	u8 data;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL2, &data);
	data |= 0x01;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, data);

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL1, &data);
	data |= 0xF0;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL1, data);
}

static void sm5713_set_ufp(struct i2c_client *i2c)
{
	u8 data;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL2, &data);
	data &= ~0x01;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, data);

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL1, &data);
	data &= 0x0F;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL1, data);
}

static void sm5713_set_src(struct i2c_client *i2c)
{
	u8 data;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL2, &data);
	data |= 0x02;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, data);
}

static void sm5713_set_snk(struct i2c_client *i2c)
{
	u8 data;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL2, &data);
	data &= ~0x02;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, data);
}

static int sm5713_set_attach(struct sm5713_phydrv_data *pdic_data, u8 mode)
{
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	if (mode == TYPE_C_ATTACH_DFP) {
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL1, 0x88);
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, 0x82);
	} else if (mode == TYPE_C_ATTACH_UFP) {
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL1, 0x84);
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, 0x81);
	}

	dev_info(dev, "%s sm5713 force to attach\n", __func__);

	return ret;
}

static int sm5713_set_detach(struct sm5713_phydrv_data *pdic_data, u8 mode)
{
	u8 data;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL3, &data);
	data |= 0x08;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, data);

	dev_info(dev, "%s sm5713 force to detach\n", __func__);

	return ret;
}

static int sm5713_set_vconn_source(void *_data, int val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0, reg_val = 0, attach_type = 0;
	int cable_type = 0;

	if (!pdic_data->vconn_en) {
		pr_err("%s, not support vconn source\n", __func__);
		return -1;
	}

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_STATUS, &reg_val);

	attach_type = (reg_val & SM5713_ATTACH_TYPE);
	cable_type = (reg_val >> SM5713_CABLE_TYPE_SHIFT) ?
			PWR_CABLE : NON_PWR_CABLE;

	if (val == USBPD_VCONN_ON) {
		if ((attach_type == SM5713_ATTACH_SINK) && cable_type) {
			reg_data = (reg_val & 0x20) ? 0x1A : 0x19;
			sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL5, reg_data);
		}
	} else if (val == USBPD_VCONN_OFF) {
		reg_data = 0x08;
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL5, reg_data);
	} else
		return -1;

	pdic_data->vconn_source = val;
	return 0;
}

static int sm5713_set_normal_mode(struct sm5713_phydrv_data *pdic_data)
{
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL4, 0x90);

	pdic_data->lpm_mode = false;

	dev_info(dev, "%s sm5713 exit lpm mode\n", __func__);

	return ret;
}

static int sm5713_set_lpm_mode(struct sm5713_phydrv_data *pdic_data)
{
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	pdic_data->lpm_mode = true;
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL4, 0x97);
#else
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL4, 0x92);
#endif

	dev_info(dev, "%s sm5713 enter lpm mode\n", __func__);

	return ret;
}

static void sm5713_adc_value_read(void *_data, u8 *adc_value)
{
	struct sm5713_phydrv_data *pdic_data = _data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0;
	int retry = 0;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_ADC_CTRL2, &reg_data);
	for (retry = 0; retry < 5; retry++) {
		if (!(reg_data & SM5713_ADC_DONE)) {
			pr_info("%s, ADC_DONE is not yet, retry : %d\n", __func__, retry);
			sm5713_usbpd_read_reg(i2c, SM5713_REG_ADC_CTRL2, &reg_data);
		} else {
			break;
		}
	}
	reg_data &= 0x3F;
	*adc_value = reg_data;
}

static void sm5713_corr_sbu_volt_read(void *_data, u8 *adc_sbu1,
				u8 *adc_sbu2, int mode)
{
	struct sm5713_phydrv_data *pdic_data = _data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 adc_value1 = 0, adc_value2 = 0;

	if (mode) {
		sm5713_usbpd_write_reg(i2c, 0x24, 0x04);
		msleep(5);
	}
	sm5713_usbpd_write_reg(i2c, SM5713_REG_ADC_CTRL1, SM5713_ADC_PATH_SEL_SBU1);
	sm5713_adc_value_read(pdic_data, &adc_value1);
	*adc_sbu1 = adc_value1;

	sm5713_usbpd_write_reg(i2c, SM5713_REG_ADC_CTRL1, SM5713_ADC_PATH_SEL_SBU2);
	sm5713_adc_value_read(pdic_data, &adc_value2);
	*adc_sbu2 = adc_value2;

	if (mode)
		sm5713_usbpd_write_reg(i2c, 0x24, 0x00);
	pr_info("%s, mode : %d, SBU1_VOLT : 0x%x, SBU2_VOLT : 0x%x\n",
			__func__, mode, adc_value1, adc_value2);
}
#if defined(CONFIG_SEC_FACTORY)
static int sm5713_vbus_adc_read(void *_data)
{
	struct sm5713_phydrv_data *pdic_data = _data;
	struct i2c_client *i2c = NULL;
	u8 vbus_adc = 0, status1 = 0;

	if (!pdic_data)
		return -ENXIO;

	i2c = pdic_data->i2c;
	if (!i2c)
		return -ENXIO;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS1, &status1);
	sm5713_usbpd_write_reg(i2c, SM5713_REG_ADC_CTRL1, SM5713_ADC_PATH_SEL_VBUS);
	sm5713_adc_value_read(pdic_data, &vbus_adc);

	pr_info("%s, STATUS1 = 0x%x, VBUS_VOLT : 0x%x\n",
			__func__, status1, vbus_adc);

	return vbus_adc; /* 0 is OK, others are NG */
}
#endif
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
static void sm5713_process_cc_water_det(void *data, int state)
{
	struct sm5713_phydrv_data *pdic_data = data;

	sm5713_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_BATTERY,
			CCIC_NOTIFY_ID_WATER, state/*attach*/,
			USB_STATUS_NOTIFY_DETACH, 0);

	pr_info("%s, water state : %d\n", __func__, state);
}
#endif

void sm5713_short_state_check(void *_data)
{
	struct sm5713_phydrv_data *pdic_data = _data;
	u8 adc_sbu1, adc_sbu2, adc_sbu3, adc_sbu4;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
	int event = 0;
#endif

	if (pdic_data->is_cc_abnormal_state || pdic_data->is_otg_vboost)
		return;

	sm5713_corr_sbu_volt_read(pdic_data, &adc_sbu1, &adc_sbu2,
			SBU_SOURCING_OFF);
	if (adc_sbu1 > 0xA && adc_sbu2 > 0xA) {
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
		pdic_data->is_water_detect = true;
		sm5713_process_cc_water_det(pdic_data, WATER_MODE_ON);
#endif
		pr_info("%s, case 3-2-2\n", __func__);
		return;
	} else if (adc_sbu1 > 0x2C || adc_sbu2 > 0x2C) {
		sm5713_corr_sbu_volt_read(pdic_data, &adc_sbu3,
				&adc_sbu4, SBU_SOURCING_ON);
		if ((adc_sbu1 < 0x4 || adc_sbu2 < 0x4) &&
				(adc_sbu3 > 0x2C || adc_sbu4 > 0x2C)) {
			pdic_data->is_sbu_abnormal_state = true;
			pr_info("%s, case 2-6,13 SBU-VBUS SHORT\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
			if (o_notify)
				inc_hw_param(o_notify,
						USB_CCIC_VBUS_SBU_SHORT_COUNT);
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
			event = NOTIFY_EXTRA_SYSMSG_SBU_VBUS_SHORT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return;
		}
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
		pdic_data->is_water_detect = true;
		sm5713_process_cc_water_det(pdic_data, WATER_MODE_ON);
#endif
		pr_info("%s, case 3-2-2\n", __func__);
		return;
	}

	sm5713_corr_sbu_volt_read(pdic_data, &adc_sbu1, &adc_sbu2,
			SBU_SOURCING_ON);
	if ((adc_sbu1 < 0x04 || adc_sbu2 < 0x04) &&
			(adc_sbu1 > 0x2C || adc_sbu2 > 0x2C)) {
#if !defined(CONFIG_SEC_FACTORY)
		pdic_data->is_sbu_abnormal_state = true;
#endif
		pr_info("%s, case 2-8,14 SBU-GND SHORT\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_GND_SBU_SHORT_COUNT);
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
		event = NOTIFY_EXTRA_SYSMSG_SBU_GND_SHORT;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
	}
}

static void sm5713_check_cc_state(struct sm5713_phydrv_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0, status1 = 0;

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (!pdic_data->try_state_change)
#elif defined(CONFIG_TYPEC)
	if (!pdic_data->typec_try_state_change)
#endif
	{
		sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS1, &status1);
		sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE0, &data);
		if (!(status1 & SM5713_REG_INT_STATUS1_ATTACH)) {
			if ((data & 0x0F) == 0x1) /* Set CC_DISABLE to CC_UNATT_SNK */
				sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, 0x81);
			else /* Set CC_OP_EN */
				sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, 0x80);
		}
	}
	pr_info("%s, cc state : 0x%x\n", __func__, data);
}

static void sm5713_notify_rp_current_level(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 cc_status = 0, rp_currentlvl = 0;
#if defined(CONFIG_TYPEC)
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#endif

	if (pdic_data->is_cc_abnormal_state ||
			pdic_data->is_sbu_abnormal_state)
		return;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_STATUS, &cc_status);

	pr_info("%s : cc_status=0x%x\n", __func__, cc_status);

	/* PDIC = SINK */
	if ((cc_status & SM5713_ATTACH_TYPE) == SM5713_ATTACH_SOURCE) {
		if ((cc_status & SM5713_ADV_CURR) == 0x00) {
			/* 5V/0.5A RP charger is detected by CCIC */
			rp_currentlvl = RP_CURRENT_LEVEL_DEFAULT;
		} else if ((cc_status & SM5713_ADV_CURR) == 0x08) {
			/* 5V/1.5A RP charger is detected by CCIC */
			rp_currentlvl = RP_CURRENT_LEVEL2;
#if defined(CONFIG_TYPEC)
			mode = TYPEC_PWR_MODE_1_5A;
#endif
		} else {
			/* 5V/3A RP charger is detected by CCIC */
			rp_currentlvl = RP_CURRENT_LEVEL3;
#if defined(CONFIG_TYPEC)
			mode = TYPEC_PWR_MODE_3_0A;
#endif
		}
#if defined(CONFIG_TYPEC)
		if (!pdic_data->pd_support) {
			pdic_data->pwr_opmode = mode;
			typec_set_pwr_opmode(pdic_data->port, mode);
		}
#endif
		if (rp_currentlvl != pd_noti.sink_status.rp_currentlvl &&
				rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT) {
			pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
			pd_noti.event = PDIC_NOTIFY_EVENT_CCIC_ATTACH;
			pr_info("%s : rp_currentlvl(%d)\n", __func__,
					pd_noti.sink_status.rp_currentlvl);
			/* TODO: Notify to AP - rp_currentlvl */
			sm5713_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_BATTERY,
				CCIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);

			sm5713_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
					CCIC_NOTIFY_ID_ATTACH,
					CCIC_NOTIFY_ATTACH,
					USB_STATUS_NOTIFY_DETACH,
					rp_currentlvl);
		}
	}
}

static void sm5713_notify_rp_abnormal(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 cc_status = 0, rp_currentlvl = RP_CURRENT_ABNORMAL;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_STATUS, &cc_status);

	/* PDIC = SINK */
	if ((cc_status & SM5713_ATTACH_TYPE) == SM5713_ATTACH_SOURCE) {
		if (rp_currentlvl != pd_noti.sink_status.rp_currentlvl) {
			pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
			pd_noti.event = PDIC_NOTIFY_EVENT_CCIC_ATTACH;
			pr_info("%s : rp_currentlvl(%d)\n", __func__,
					pd_noti.sink_status.rp_currentlvl);

			sm5713_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_BATTERY,
				CCIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);

			sm5713_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
					CCIC_NOTIFY_ID_ATTACH,
					CCIC_NOTIFY_ATTACH,
					USB_STATUS_NOTIFY_DETACH,
					rp_currentlvl);
		}
	}
}

static void sm5713_send_role_swap_message(
	struct sm5713_phydrv_data *usbpd_data, u8 mode)
{
	struct sm5713_usbpd_data *pd_data;

	pd_data = dev_get_drvdata(usbpd_data->dev);
	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return;
	}

	pr_info("%s : send %s\n", __func__,
		mode == POWER_ROLE_SWAP ? "pr_swap" : "dr_swap");
	if (mode == POWER_ROLE_SWAP)
		sm5713_usbpd_inform_event(pd_data, MANAGER_PR_SWAP_REQUEST);
	else
		sm5713_usbpd_inform_event(pd_data, MANAGER_DR_SWAP_REQUEST);
}

void sm5713_rprd_mode_change(struct sm5713_phydrv_data *usbpd_data, u8 mode)
{
	struct i2c_client *i2c = usbpd_data->i2c;

	pr_info("%s : mode=0x%x\n", __func__, mode);

	switch (mode) {
	case TYPE_C_ATTACH_DFP: /* SRC */
		sm5713_set_detach(usbpd_data, mode);
		msleep(1000);
		sm5713_set_attach(usbpd_data, mode);
		break;
	case TYPE_C_ATTACH_UFP: /* SNK */
		sm5713_set_detach(usbpd_data, mode);
		msleep(1000);
		sm5713_set_attach(usbpd_data, mode);
		break;
	case TYPE_C_ATTACH_DRP: /* DRP */
		sm5713_usbpd_write_reg(i2c,
			SM5713_REG_CC_CNTL1, SM5713_DEAD_RD_ENABLE);
		break;
	};
}

void sm5713_power_role_change(struct sm5713_phydrv_data *usbpd_data,
	int power_role)
{
	pr_info("%s : power_role is %s\n", __func__,
		power_role == TYPE_C_ATTACH_SRC ? "src" : "snk");

	switch (power_role) {
	case TYPE_C_ATTACH_SRC:
	case TYPE_C_ATTACH_SNK:
		sm5713_send_role_swap_message(usbpd_data, POWER_ROLE_SWAP);
		break;
	};
}

void sm5713_data_role_change(struct sm5713_phydrv_data *usbpd_data,
	int data_role)
{
	pr_info("%s : data_role is %s\n", __func__, 
		data_role == TYPE_C_ATTACH_DFP ? "dfp" : "ufp");

	switch (data_role) {
	case TYPE_C_ATTACH_DFP:
	case TYPE_C_ATTACH_UFP:
		sm5713_send_role_swap_message(usbpd_data, DATA_ROLE_SWAP);
		break;
	};
}

static void sm5713_role_swap_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct sm5713_phydrv_data *usbpd_data =
		container_of(delay_work,
			struct sm5713_phydrv_data, role_swap_work);

	pr_info("%s: role swap check again.\n", __func__);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)	
	usbpd_data->try_state_change = 0;
#elif defined(CONFIG_TYPEC)
	usbpd_data->typec_try_state_change = 0;
#endif
	if (!usbpd_data->is_attached) {
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		sm5713_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
	}
}

static void sm5713_vbus_dischg_work(struct work_struct *work)
{
	struct sm5713_phydrv_data *pdic_data =
		container_of(work, struct sm5713_phydrv_data,
				vbus_dischg_work.work);

	if (gpio_is_valid(pdic_data->vbus_dischg_gpio)) {
		gpio_set_value(pdic_data->vbus_dischg_gpio, 0);
		pr_info("%s vbus_discharging(%d)\n", __func__,
				gpio_get_value(pdic_data->vbus_dischg_gpio));
	}
}

void sm5713_usbpd_set_vbus_dischg_gpio(struct sm5713_phydrv_data
		*pdic_data, int vbus_dischg)
{
	if (!gpio_is_valid(pdic_data->vbus_dischg_gpio))
		return;

	cancel_delayed_work_sync(&pdic_data->vbus_dischg_work);
	gpio_set_value(pdic_data->vbus_dischg_gpio, vbus_dischg);

	if (vbus_dischg > 0)
		schedule_delayed_work(&pdic_data->vbus_dischg_work,
				msecs_to_jiffies(600));

	pr_info("%s vbus_discharging(%d)\n", __func__,
			gpio_get_value(pdic_data->vbus_dischg_gpio));
}

#if defined(CONFIG_VBUS_NOTIFIER)
static void sm5713_usbpd_handle_vbus(struct work_struct *work)
{
	struct sm5713_phydrv_data *pdic_data =
		container_of(work, struct sm5713_phydrv_data,
				vbus_noti_work.work);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 status1 = 0;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS1, &status1);

	if (status1 & SM5713_REG_INT_STATUS1_VBUSPOK)
		vbus_notifier_handle(STATUS_VBUS_HIGH);
	else if (status1 & SM5713_REG_INT_STATUS1_VBUSUVLO)
		vbus_notifier_handle(STATUS_VBUS_LOW);
}
#endif /* CONFIG_VBUS_NOTIFIER */

#if defined(CONFIG_SEC_FACTORY)
static void sm5713_factory_check_abnormal_state(struct work_struct *work)
{
	struct sm5713_phydrv_data *pdic_data =
		container_of(work, struct sm5713_phydrv_data,
				factory_state_work.work);
	int state_cnt = pdic_data->factory_mode.FAC_Abnormal_Repeat_State;

	if (state_cnt >= FAC_ABNORMAL_REPEAT_STATE) {
		pr_info("%s: Notify the abnormal state [STATE] [ %d]",
				__func__, state_cnt);
		/* Notify to AP - ERROR STATE 1 */
		sm5713_ccic_event_work(pdic_data,	CCIC_NOTIFY_DEV_CCIC,
			CCIC_NOTIFY_ID_FAC, 1, 0, 0);
	} else
		pr_info("%s: [STATE] cnt :  [%d]", __func__, state_cnt);
	pdic_data->factory_mode.FAC_Abnormal_Repeat_State = 0;
}

static void sm5713_factory_check_normal_rid(struct work_struct *work)
{
	struct sm5713_phydrv_data *pdic_data =
		container_of(work, struct sm5713_phydrv_data,
				factory_rid_work.work);
	int rid_cnt = pdic_data->factory_mode.FAC_Abnormal_Repeat_RID;

	if (rid_cnt >= FAC_ABNORMAL_REPEAT_RID) {
		pr_info("%s: Notify the abnormal state [RID] [ %d]",
				__func__, rid_cnt);
		/* Notify to AP - ERROR STATE 2 */
		sm5713_ccic_event_work(pdic_data,	CCIC_NOTIFY_DEV_CCIC,
			CCIC_NOTIFY_ID_FAC, 1 << 1, 0, 0);
	} else
		pr_info("%s: [RID] cnt :  [%d]", __func__, rid_cnt);

	pdic_data->factory_mode.FAC_Abnormal_Repeat_RID = 0;
}

static void sm5713_factory_execute_monitor(
		struct sm5713_phydrv_data *usbpd_data, int type)
{
	uint32_t state_cnt = usbpd_data->factory_mode.FAC_Abnormal_Repeat_State;
	uint32_t rid_cnt = usbpd_data->factory_mode.FAC_Abnormal_Repeat_RID;
	uint32_t rid0_cnt = usbpd_data->factory_mode.FAC_Abnormal_RID0;

	pr_info("%s: state_cnt = %d, rid_cnt = %d, rid0_cnt = %d\n",
			__func__, state_cnt, rid_cnt, rid0_cnt);

	switch (type) {
	case FAC_ABNORMAL_REPEAT_RID0:
		if (!rid0_cnt) {
			pr_info("%s: Notify the abnormal state [RID0] [%d]!!",
					__func__, rid0_cnt);
			usbpd_data->factory_mode.FAC_Abnormal_RID0++;
			/* Notify to AP - ERROR STATE 4 */
			sm5713_ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_CCIC,
				CCIC_NOTIFY_ID_FAC, 1 << 2, 0, 0);
		} else {
			usbpd_data->factory_mode.FAC_Abnormal_RID0 = 0;
		}
	break;
	case FAC_ABNORMAL_REPEAT_RID:
		if (!rid_cnt) {
			schedule_delayed_work(&usbpd_data->factory_rid_work,
					msecs_to_jiffies(1000));
			pr_info("%s: start the factory_rid_work", __func__);
		}
		usbpd_data->factory_mode.FAC_Abnormal_Repeat_RID++;
	break;
	case FAC_ABNORMAL_REPEAT_STATE:
		if (!state_cnt) {
			schedule_delayed_work(&usbpd_data->factory_state_work,
					msecs_to_jiffies(1000));
			pr_info("%s: start the factory_state_work", __func__);
		}
		usbpd_data->factory_mode.FAC_Abnormal_Repeat_State++;
	break;
	default:
		pr_info("%s: Never Calling [%d]", __func__, type);
	break;
	}
}
#endif

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
static int sm5713_ccic_set_dual_role(struct dual_role_phy_instance *dual_role,
				enum dual_role_property prop,
				const unsigned int *val)
{
	struct sm5713_phydrv_data *usbpd_data = dual_role_get_drvdata(dual_role);
	struct i2c_client *i2c;
	USB_STATUS attached_state;
	int mode;
	int timeout = 0;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null\n", __func__);
		return -EINVAL;
	}

	i2c = usbpd_data->i2c;

	/* Get Current Role */
	attached_state = usbpd_data->data_role_dual;
	pr_info("%s : request prop = %d , attached_state = %d\n",
			__func__, prop, attached_state);

	if (attached_state != USB_STATUS_NOTIFY_ATTACH_DFP
			&& attached_state != USB_STATUS_NOTIFY_ATTACH_UFP) {
		pr_err("%s : current mode : %d - just return\n",
				__func__, attached_state);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP
			&& *val == DUAL_ROLE_PROP_MODE_DFP) {
		pr_err("%s : current mode : %d - request mode : %d just return\n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP
			&& *val == DUAL_ROLE_PROP_MODE_UFP) {
		pr_err("%s : current mode : %d - request mode : %d just return\n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		/* Current mode DFP and Source  */
		pr_info("%s: try reversing, from Source to Sink\n", __func__);
		/* turns off VBUS first */
		sm5713_vbus_turn_on_ctrl(usbpd_data, 0);
#if defined(CONFIG_CCIC_NOTIFIER)
		/* muic */
		sm5713_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
			CCIC_NOTIFY_DETACH/*attach*/,
			USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
#endif
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_UFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_UFP;
		sm5713_rprd_mode_change(usbpd_data, mode);
	} else {
		/* Current mode UFP and Sink  */
		pr_info("%s: try reversing, from Sink to Source\n", __func__);
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_DFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_DFP;
		sm5713_rprd_mode_change(usbpd_data, mode);
	}

	reinit_completion(&usbpd_data->reverse_completion);
	timeout =
		wait_for_completion_timeout(&usbpd_data->reverse_completion,
					msecs_to_jiffies
					(DUAL_ROLE_SET_MODE_WAIT_MS));

	if (!timeout) {
		usbpd_data->try_state_change = 0;
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		/* exit from Disabled state and set mode to DRP */
		mode =  TYPE_C_ATTACH_DRP;
		sm5713_rprd_mode_change(usbpd_data, mode);
		ret = -EIO;
	} else {
		pr_err("%s: reverse success, one more check\n", __func__);
		schedule_delayed_work(&usbpd_data->role_swap_work,
				msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
	}

	dev_info(&i2c->dev, "%s -> data role : %d\n", __func__, *val);
	return ret;
}

/* Decides whether userspace can change a specific property */
static int sm5713_dual_role_is_writeable(struct dual_role_phy_instance *drp,
				enum dual_role_property prop)
{
	if (prop == DUAL_ROLE_PROP_MODE)
		return 1;
	else
		return 0;
}

static int sm5713_dual_role_get_prop(
		struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop,
		unsigned int *val)
{
	struct sm5713_phydrv_data *usbpd_data = dual_role_get_drvdata(dual_role);

	USB_STATUS attached_state;
	int power_role_dual;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null : request prop = %d\n",
				__func__, prop);
		return -EINVAL;
	}
	attached_state = usbpd_data->data_role_dual;
	power_role_dual = usbpd_data->power_role_dual;

	pr_info("%s : prop = %d , attached_state = %d, power_role_dual = %d\n",
		__func__, prop, attached_state, power_role_dual);

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			return -EINVAL;
	} else if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_UFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_DEVICE;
		else
			return -EINVAL;
	} else {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
			return -EINVAL;
	}

	return 0;
}

static int sm5713_dual_role_set_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop,
			const unsigned int *val)
{
	pr_info("%s : request prop = %d , *val = %d\n", __func__, prop, *val);
	if (prop == DUAL_ROLE_PROP_MODE)
		return sm5713_ccic_set_dual_role(dual_role, prop, val);
	else
		return -EINVAL;
}
#elif defined(CONFIG_TYPEC)
static int sm5713_dr_set(const struct typec_capability *cap,
	enum typec_data_role role)
{
	struct sm5713_phydrv_data *usbpd_data =
		container_of(cap, struct sm5713_phydrv_data, typec_cap);

	if (!usbpd_data)
		return -EINVAL;

	pr_info("%s : typec_power_role=%d, typec_data_role=%d, role=%d\n",
		__func__, usbpd_data->typec_power_role,
		usbpd_data->typec_data_role, role);
	
	if (usbpd_data->typec_data_role != TYPEC_DEVICE
		&& usbpd_data->typec_data_role != TYPEC_HOST)
		return -EPERM;
	else if (usbpd_data->typec_data_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_DEVICE) {
		pr_info("%s : try reversing, from DFP to UFP\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		sm5713_data_role_change(usbpd_data, TYPE_C_ATTACH_UFP);
	} else if (role == TYPEC_HOST) {
		pr_info("%s : try reversing, from UFP to DFP\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		sm5713_data_role_change(usbpd_data, TYPE_C_ATTACH_DFP);
	} else {
		pr_info("%s : invalid typec_role\n", __func__);
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		return -ETIMEDOUT;
	}

	return 0;
}

static int sm5713_pr_set(const struct typec_capability *cap,
	enum typec_role role)
{
	struct sm5713_phydrv_data *usbpd_data =
		container_of(cap, struct sm5713_phydrv_data, typec_cap);

	if (!usbpd_data)
		return -EINVAL;

	pr_info("%s : typec_power_role=%d, typec_data_role=%d, role=%d\n",
		__func__, usbpd_data->typec_power_role,
		usbpd_data->typec_data_role, role);

	if (usbpd_data->typec_power_role != TYPEC_SINK
	    && usbpd_data->typec_power_role != TYPEC_SOURCE)
		return -EPERM;
	else if (usbpd_data->typec_power_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_SINK) {
		pr_info("%s : try reversing, from Source to Sink\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		sm5713_power_role_change(usbpd_data, TYPE_C_ATTACH_SNK);
	} else if (role == TYPEC_SOURCE) {
		pr_info("%s : try reversing, from Sink to Source\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		sm5713_power_role_change(usbpd_data, TYPE_C_ATTACH_SRC);
	} else {
		pr_info("%s : invalid typec_role\n", __func__);
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		if (usbpd_data->typec_power_role != role)
			return -ETIMEDOUT;
	}

	return 0;
}

static int sm5713_port_type_set(const struct typec_capability *cap,
	enum typec_port_type port_type)
{
	struct sm5713_phydrv_data *usbpd_data =
		container_of(cap, struct sm5713_phydrv_data, typec_cap);

	if (!usbpd_data)
		return -EINVAL;

	pr_info("%s : typec_power_role=%d, typec_data_role=%d, port_type=%d\n",
		__func__, usbpd_data->typec_power_role,
		usbpd_data->typec_data_role, port_type);

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (port_type == TYPEC_PORT_DFP) {
		pr_info("%s : try reversing, from UFP(Sink) to DFP(Source)\n",
			__func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		sm5713_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DFP);
	} else if (port_type == TYPEC_PORT_UFP) {
		pr_info("%s : try reversing, from DFP(Source) to UFP(Sink)\n",
			__func__);
#if defined(CONFIG_CCIC_NOTIFIER)
		sm5713_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
			0/*attach*/, 0/*rprd*/, 0);
#endif
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		sm5713_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_UFP);
	} else if (port_type == TYPEC_PORT_DRP) {
		pr_info("%s : set to DRP (No action)\n", __func__);
		return 0;
	} else {
		pr_info("%s : invalid typec_role\n", __func__);
		return -EIO;
	}

	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		/* exit from Disabled state and set mode to DRP */
		sm5713_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
		return -ETIMEDOUT;
	} else {
		pr_err("%s: reverse success, one more check\n", __func__);
		schedule_delayed_work(&usbpd_data->role_swap_work,
				msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
	}

	return 0;
}

int sm5713_get_pd_support(struct sm5713_phydrv_data *usbpd_data)
{
	bool support_pd_role_swap = false;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, "sm5713-usbpd,i2c");

	if (np)
		support_pd_role_swap =
			of_property_read_bool(np, "support_pd_role_swap");
	else
		pr_info("%s : np is null\n");

	pr_info("%s : support_pd_role_swap is %d, pd_support : %d\n",
		__func__, support_pd_role_swap, usbpd_data->pd_support);

	if (support_pd_role_swap && usbpd_data->pd_support)
		return TYPEC_PWR_MODE_PD;

	return usbpd_data->pwr_opmode;
}
#endif

#if defined(CONFIG_CCIC_NOTIFIER)
static void sm5713_ccic_event_notifier(struct work_struct *data)
{
	struct ccic_state_work *event_work =
		container_of(data, struct ccic_state_work, ccic_work);
	CC_NOTI_TYPEDEF ccic_noti;

	switch (event_work->dest) {
	case CCIC_NOTIFY_DEV_USB:
		pr_info("%s, dest=%s, id=%s, attach=%s, drp=%s, event_work=%p\n",
			__func__,
			CCIC_NOTI_DEST_Print[event_work->dest],
			CCIC_NOTI_ID_Print[event_work->id],
			event_work->attach ? "Attached" : "Detached",
			CCIC_NOTI_USB_STATUS_Print[event_work->event],
			event_work);
		break;
	default:
		pr_info("%s, dest=%s, id=%s, attach=%d, event=%d, event_work=%p\n",
			__func__,
			CCIC_NOTI_DEST_Print[event_work->dest],
			CCIC_NOTI_ID_Print[event_work->id],
			event_work->attach,
			event_work->event,
			event_work);
		break;
	}

	ccic_noti.src = CCIC_NOTIFY_DEV_CCIC;
	ccic_noti.dest = event_work->dest;
	ccic_noti.id = event_work->id;
	ccic_noti.sub1 = event_work->attach;
	ccic_noti.sub2 = event_work->event;
	ccic_noti.sub3 = event_work->sub;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	ccic_noti.pd = &pd_noti;
#endif
	ccic_notifier_notify((CC_NOTI_TYPEDEF *)&ccic_noti, NULL, 0);

	kfree(event_work);
}

void sm5713_ccic_event_work(void *data, int dest,
		int id, int attach, int event, int sub)
{
	struct sm5713_phydrv_data *usbpd_data = data;
	struct ccic_state_work *event_work;
#if defined(CONFIG_TYPEC)
	struct typec_partner_desc desc;
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#endif

	pr_info("%s : usb: DIAES %d-%d-%d-%d-%d\n",
		__func__, dest, id, attach, event, sub);
	event_work = kmalloc(sizeof(struct ccic_state_work), GFP_ATOMIC);
	if (!event_work) {
		pr_err("%s: failed to allocate event_work\n", __func__);
		return;
	}
	INIT_WORK(&event_work->ccic_work, sm5713_ccic_event_notifier);

	event_work->dest = dest;
	event_work->id = id;
	event_work->attach = attach;
	event_work->event = event;
	event_work->sub = sub;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (id == CCIC_NOTIFY_ID_USB) {
		pr_info("usb: %s, dest=%d, event=%d, try_state_change=%d\n",
			__func__, dest, event, usbpd_data->try_state_change);

		usbpd_data->data_role_dual = event;

		if (usbpd_data->dual_role != NULL)
			dual_role_instance_changed(usbpd_data->dual_role);

		if (usbpd_data->try_state_change &&
				(usbpd_data->data_role_dual !=
				USB_STATUS_NOTIFY_DETACH)) {
			pr_info("usb: %s, reverse_completion\n", __func__);
			complete(&usbpd_data->reverse_completion);
		}
	}
#elif defined(CONFIG_TYPEC)
	if (id == CCIC_NOTIFY_ID_USB) {
		pr_info("%s : typec_power_role=%d typec_data_role=%d, event=%d\n",
			__func__, usbpd_data->typec_power_role,
			usbpd_data->typec_data_role, event);
		if (usbpd_data->partner == NULL) {
			if (event == USB_STATUS_NOTIFY_ATTACH_UFP) {
				mode = sm5713_get_pd_support(usbpd_data);
				typec_set_pwr_opmode(usbpd_data->port, mode);
				desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
				desc.accessory = TYPEC_ACCESSORY_NONE;
				desc.identity = NULL;
				usbpd_data->typec_data_role = TYPEC_DEVICE;
				typec_set_pwr_role(usbpd_data->port,
					usbpd_data->typec_power_role);
				typec_set_data_role(usbpd_data->port,
					usbpd_data->typec_data_role);
				usbpd_data->partner = typec_register_partner(usbpd_data->port,
					&desc);
			} else if (event == USB_STATUS_NOTIFY_ATTACH_DFP) {
				mode = sm5713_get_pd_support(usbpd_data);
				typec_set_pwr_opmode(usbpd_data->port, mode);
				desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
				desc.accessory = TYPEC_ACCESSORY_NONE;
				desc.identity = NULL;
				usbpd_data->typec_data_role = TYPEC_HOST;
				typec_set_pwr_role(usbpd_data->port,
					usbpd_data->typec_power_role);
				typec_set_data_role(usbpd_data->port,
					usbpd_data->typec_data_role);
				usbpd_data->partner = typec_register_partner(usbpd_data->port,
					&desc);
			} else
				pr_info("%s : detach case\n", __func__);
			if (usbpd_data->typec_try_state_change &&
					(event != USB_STATUS_NOTIFY_DETACH)) {
				pr_info("usb: %s, typec_reverse_completion\n", __func__);
				complete(&usbpd_data->typec_reverse_completion);
			}
		} else {
			if (event == USB_STATUS_NOTIFY_ATTACH_UFP) {
				usbpd_data->typec_data_role = TYPEC_DEVICE;
				typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
			} else if (event == USB_STATUS_NOTIFY_ATTACH_DFP) {
				usbpd_data->typec_data_role = TYPEC_HOST;
				typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
			} else
				pr_info("%s : detach case\n", __func__);
		}
	}
#endif
	if (!queue_work(usbpd_data->ccic_wq, &event_work->ccic_work)) {
		pr_info("usb: %s, event_work(%p) is dropped\n",
			__func__, event_work);
		kfree(event_work);
	}
}

static void sm5713_process_dr_swap(struct sm5713_phydrv_data *usbpd_data, int val)
{
	if (val == USBPD_UFP) {
		sm5713_ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				CCIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		sm5713_ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				CCIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
	} else if (val == USBPD_DFP) {
		/* muic */
		sm5713_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
			CCIC_NOTIFY_ATTACH/*attach*/,
			USB_STATUS_NOTIFY_ATTACH_DFP/*rprd*/, 0);
		sm5713_ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				CCIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		sm5713_ccic_event_work(usbpd_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				CCIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/, 0);
	} else
		pr_err("%s : invalid val\n", __func__);
}

static void sm5713_control_option_command(
		struct sm5713_phydrv_data *pdic_data, int cmd)
{
	struct sm5713_usbpd_data *_data = dev_get_drvdata(pdic_data->dev);
	int pd_cmd = cmd & 0x0f;

	switch (pd_cmd) {
	/* 0x1 : Vconn control option command ON */
	case 1:
		sm5713_set_vconn_source(_data, USBPD_VCONN_ON);
		break;
	/* 0x2 : Vconn control option command OFF */
	case 2:
		sm5713_set_vconn_source(_data, USBPD_VCONN_OFF);
		break;
	default:
		break;
	}
}

static int sm5713_sysfs_get_prop(struct _ccic_data_t *pccic_data,
					enum ccic_sysfs_property prop,
					char *buf)
{
	int retval = -ENODEV;
	struct sm5713_phydrv_data *usbpd_data =
			(struct sm5713_phydrv_data *)pccic_data->drv_data;
	struct sm5713_usbpd_data *pd_data;
	struct sm5713_usbpd_manager_data *manager;

	if (!usbpd_data) {
		pr_info("%s : usbpd_data is null\n", __func__);
		return retval;
	}
	pd_data = dev_get_drvdata(usbpd_data->dev);
	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return retval;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s : manager is null\n", __func__);
		return retval;
	}

	switch (prop) {
	case CCIC_SYSFS_PROP_LPM_MODE:
		retval = sprintf(buf, "%d\n", usbpd_data->lpm_mode);
		pr_info("%s : CCIC_SYSFS_PROP_LPM_MODE : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_RID:
		retval = sprintf(buf, "%d\n", usbpd_data->rid == REG_RID_MAX ?
				REG_RID_OPEN : usbpd_data->rid);
		pr_info("%s : CCIC_SYSFS_PROP_RID : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_FW_WATER:
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
		retval = sprintf(buf, "%d\n", usbpd_data->is_water_detect);
#else
		retval = sprintf(buf, "0\n");
#endif
		pr_info("%s : CCIC_SYSFS_PROP_FW_WATER : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_STATE:
		retval = sprintf(buf, "%d\n", (int)pd_data->policy.plug_valid);
		pr_info("%s : CCIC_SYSFS_PROP_STATE : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_ACC_DEVICE_VERSION:
		retval = sprintf(buf, "%04x\n", manager->Device_Version);
		pr_info("%s : CCIC_SYSFS_PROP_ACC_DEVICE_VERSION : %s",
				__func__, buf);
		break;
	case CCIC_SYSFS_PROP_USBPD_IDS:
		retval = sprintf(buf, "%04x:%04x\n",
				le16_to_cpu(manager->Vendor_ID),
				le16_to_cpu(manager->Product_ID));
		pr_info("%s : CCIC_SYSFS_PROP_USBPD_IDS : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_USBPD_TYPE:
		retval = sprintf(buf, "%d\n", manager->acc_type);
		pr_info("%s : CCIC_SYSFS_PROP_USBPD_TYPE : %s", __func__, buf);
		break;
#if defined(CONFIG_SEC_FACTORY)
	case CCIC_SYSFS_PROP_VBUS_ADC:
		manager->vbus_adc = sm5713_vbus_adc_read(usbpd_data);
		retval = sprintf(buf, "%d\n", manager->vbus_adc);
		pr_info("%s : CCIC_SYSFS_PROP_VBUS_ADC : %s", __func__, buf);
		break;
#endif
	default:
		pr_info("%s : prop read not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		break;
	}
	return retval;
}

static ssize_t sm5713_sysfs_set_prop(struct _ccic_data_t *pccic_data,
				enum ccic_sysfs_property prop,
				const char *buf, size_t size)
{
	ssize_t retval = size;
	struct sm5713_phydrv_data *usbpd_data =
			(struct sm5713_phydrv_data *)pccic_data->drv_data;
	int rv;
	int mode = 0;

	if (!usbpd_data) {
		pr_info("%s : usbpd_data is null : request prop = %d\n",
				__func__, prop);
		return -ENODEV;
	}

	switch (prop) {
	case CCIC_SYSFS_PROP_LPM_MODE:
		rv = sscanf(buf, "%d", &mode);
		pr_info("%s : CCIC_SYSFS_PROP_LPM_MODE mode=%d\n",
				__func__, mode);
		mutex_lock(&usbpd_data->lpm_mutex);
		if (mode == 1 || mode == 2)
			sm5713_set_lpm_mode(usbpd_data);
		else
			sm5713_set_normal_mode(usbpd_data);
		mutex_unlock(&usbpd_data->lpm_mutex);
		break;
	case CCIC_SYSFS_PROP_CTRL_OPTION:
		rv = sscanf(buf, "%d", &mode);
		pr_info("%s : CCIC_SYSFS_PROP_CTRL_OPTION mode=%d\n",
				__func__, mode);
		sm5713_control_option_command(usbpd_data, mode);
		break;
	default:
		pr_info("%s : prop write not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		return retval;
	}
	return size;
}

static int sm5713_sysfs_is_writeable(struct _ccic_data_t *pccic_data,
				enum ccic_sysfs_property prop)
{
	switch (prop) {
	case CCIC_SYSFS_PROP_LPM_MODE:
	case CCIC_SYSFS_PROP_CTRL_OPTION:
		return 1;
	default:
		return 0;
	}
}
#endif

#ifndef CONFIG_SEC_FACTORY
void sm5713_usbpd_set_rp_scr_sel(struct sm5713_usbpd_data *_data, int scr_sel)
{
	struct sm5713_phydrv_data *pdic_data = _data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	pr_info("%s: scr_sel : (%d)\n", __func__, scr_sel);

	switch (scr_sel) {
	case PLUG_CTRL_RP80:
		sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL1, &data);
		data &= 0xCF;
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL1, data);
		break;
	case PLUG_CTRL_RP180:
		sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL1, &data);
		data |= 0x10;
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL1, data);
		break;
	case PLUG_CTRL_RP330:
		sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL1, &data);
		data |= 0x20;
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL1, data);
		break;
	default:
		break;
	}
}
#endif

#if defined(CONFIG_IF_CB_MANAGER)
static int sm5713_usbpd_sbu_test_read(void *data)
{
	struct sm5713_phydrv_data *usbpd_data = data;
	struct i2c_client *test_i2c;
	u8 adc_sbu1, adc_sbu2;

	if (usbpd_data == NULL)
		return -ENXIO;

	test_i2c = usbpd_data->i2c;

	sm5713_usbpd_write_reg(test_i2c, SM5713_REG_ADC_CTRL1,
			SM5713_ADC_PATH_SEL_SBU1);
	sm5713_adc_value_read(usbpd_data, &adc_sbu1);

	sm5713_usbpd_write_reg(test_i2c, SM5713_REG_ADC_CTRL1,
			SM5713_ADC_PATH_SEL_SBU2);
	sm5713_adc_value_read(usbpd_data, &adc_sbu2);

	pr_info("%s, SBU1_VOLT : 0x%x, SBU2_VOLT : 0x%x\n",
			__func__, adc_sbu1, adc_sbu2);
	if (adc_sbu1 == 0 || adc_sbu2 == 0)
		return 0;
	else
		return 1;
}
#endif

static int sm5713_write_msg_header(struct i2c_client *i2c, u8 *buf)
{
	int ret;
	ret = sm5713_usbpd_multi_write(i2c, SM5713_REG_TX_HEADER_00, 2, buf);
	return ret;
}

static int sm5713_write_msg_obj(struct i2c_client *i2c,
		int count, data_obj_type *obj)
{
	int ret = 0;
	int i = 0, j = 0;
	u8 reg[USBPD_MAX_COUNT_RX_PAYLOAD] = {0, };
	struct device *dev = &i2c->dev;

	if (count > SM5713_MAX_NUM_MSG_OBJ)
		dev_err(dev, "%s, not invalid obj count number\n", __func__);
	else {
		for (i = 0; i < (count * 4); i++) {
			if ((i != 0) && (i % 4 == 0))
				j++;
			reg[i] = obj[j].byte[i % 4];
		}
			ret = sm5713_usbpd_multi_write(i2c,
			SM5713_REG_TX_PAYLOAD_00, (count * 4), reg);
		}
	return ret;
}

static int sm5713_send_msg(void *_data, struct i2c_client *i2c)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_policy_data *policy = &data->policy;
	int ret;
	u8 val;

	if (policy->origin_message == 0x00)
		val = SM5713_REG_MSG_SEND_TX_SOP_REQ;
	else if (policy->origin_message == 0x01)
		val = SM5713_REG_MSG_SEND_TX_SOPP_REQ;
	else
		val = SM5713_REG_MSG_SEND_TX_SOPPP_REQ;

	pr_info("%s, TX_REQ : %x\n", __func__, val);
	ret = sm5713_usbpd_write_reg(i2c, SM5713_REG_TX_REQ, val);

	return ret;
}

static int sm5713_read_msg_header(struct i2c_client *i2c,
		msg_header_type *header)
{
	int ret;

	ret = sm5713_usbpd_multi_read(i2c,
			SM5713_REG_RX_HEADER_00, 2, header->byte);

	return ret;
}

static int sm5713_read_msg_obj(struct i2c_client *i2c,
		int count, data_obj_type *obj)
{
	int ret = 0;
	int i = 0, j = 0;
	u8 reg[USBPD_MAX_COUNT_RX_PAYLOAD] = {0, };
	struct device *dev = &i2c->dev;

	if (count > SM5713_MAX_NUM_MSG_OBJ) {
		dev_err(dev, "%s, not invalid obj count number\n", __func__);
		ret = -EINVAL;
	} else {
		ret = sm5713_usbpd_multi_read(i2c,
			SM5713_REG_RX_PAYLOAD_00, (count * 4), reg);
		for (i = 0; i < (count * 4); i++) {
			if ((i != 0) && (i % 4 == 0))
				j++;
			obj[j].byte[i % 4] = reg[i];
		}
	}

	return ret;
}

static void sm5713_set_irq_enable(struct sm5713_phydrv_data *_data,
		u8 int0, u8 int1, u8 int2, u8 int3, u8 int4)
{
	u8 int_mask[5]
		= {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	int ret = 0;
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;

	int_mask[0] &= ~int0;
	int_mask[1] &= ~int1;
	int_mask[2] &= ~int2;
	int_mask[3] &= ~int3;
	int_mask[4] &= ~int4;

	ret = sm5713_usbpd_multi_write(i2c, SM5713_REG_INT_MASK1,
			5, int_mask);

	if (ret < 0)
		dev_err(dev, "err write interrupt mask\n");
}

static void sm5713_driver_reset(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	int i;

	pdic_data->status_reg = 0;
	data->wait_for_msg_arrived = 0;
	pdic_data->header.word = 0;
	for (i = 0; i < SM5713_MAX_NUM_MSG_OBJ; i++)
		pdic_data->obj[i].object = 0;

	sm5713_set_irq_enable(pdic_data, ENABLED_INT_1, ENABLED_INT_2,
			ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
}

void sm5713_protocol_layer_reset(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 status2, status5;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS2, &status2);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS5, &status5);

	if ((status2 & SM5713_REG_INT_STATUS2_RID_INT) ||
			(status5 & SM5713_REG_INT_STATUS5_JIG_CASE_ON)) {
		pr_info("%s: Do not protocol reset.\n", __func__);
		return;
	}

	/* Reset Protocol Layer */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL4,
			SM5713_REG_CNTL_PROTOCOL_RESET_MESSAGE);

	pr_info("%s\n", __func__);
}

void sm5713_cc_state_hold_on_off(void *_data, int onoff)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL3, &val);
	if (onoff == 1)
		val |= 0x10;
	else
		val &= 0xEF;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, val);
	pr_info("%s: CC State Hold [%d], val = %x\n", __func__, onoff, val);
}

bool sm5713_get_rx_buf_st(void *_data)
{
	struct sm5713_usbpd_data *pd_data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_RX_BUF_ST, &val);
	pr_info("%s: RX_BUF_ST [0x%02X]\n", __func__, val);

	if (val & 0x04) /* Rx Buffer Empty */
		return false;
	else
		return true;
}

void sm5713_src_transition_to_default(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL2, &val);
	val &= 0xEF;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, val); /* BIST Off */

	sm5713_set_vconn_source(data, USBPD_VCONN_OFF);
	sm5713_vbus_turn_on_ctrl(pdic_data, 0);
	sm5713_set_dfp(i2c);
	pdic_data->data_role = USBPD_DFP;
	pdic_data->pd_support = 0;

	if (!sm5713_check_vbus_state(data))
		sm5713_usbpd_kick_policy_work(pdic_data->dev);
	dev_info(pdic_data->dev, "%s\n", __func__);
}

void sm5713_src_transition_to_pwr_on(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;

	sm5713_set_vconn_source(data, USBPD_VCONN_ON);
	sm5713_vbus_turn_on_ctrl(pdic_data, 1);

	/* Hard Reset Done Notify to PRL */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL4,
			SM5713_ATTACH_SOURCE);
	dev_info(pdic_data->dev, "%s\n", __func__);
}

void sm5713_snk_transition_to_default(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;

	sm5713_cc_state_hold_on_off(data, 1); /* Enable CC State Hold */
	sm5713_set_vconn_source(data, USBPD_VCONN_OFF);
	sm5713_set_ufp(i2c);
	pdic_data->data_role = USBPD_UFP;
	pdic_data->pd_support = 0;

	/* Hard Reset Done Notify to PRL */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL4,
			SM5713_ATTACH_SOURCE);
	dev_info(pdic_data->dev, "%s\n", __func__);
}

bool sm5713_check_vbus_state(void *_data)
{
	struct sm5713_usbpd_data *pd_data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PROBE0, &val);
	if (val & 0x40) /* VBUS OK */
		return true;
	else
		return false;
}

static void sm5713_usbpd_abnormal_reset_check(struct sm5713_phydrv_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	u8 reg_data = 0;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL1, &reg_data);
	pr_info("%s, CC_CNTL1 : 0x%x\n", __func__, reg_data);

	if (reg_data == 0x84) { /* surge reset */
		sm5713_driver_reset(pd_data);
		sm5713_usbpd_reg_init(pdic_data);
	}
}

static void sm5713_assert_rd(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL7, &val);

	val ^= 0x01;
	/* Apply CC State PR_Swap (Att.Src -> Att.Snk) */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL7, val);
}

static void sm5713_assert_rp(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL7, &val);

	val ^= 0x01;
	/* Apply CC State PR_Swap (Att.Snk -> Att.Src) */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL7, val);
}

static unsigned int sm5713_get_status(void *_data, unsigned int flag)
{
	unsigned int ret;
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;

	if (pdic_data->status_reg & flag) {
		ret = pdic_data->status_reg & flag;
		dev_info(pdic_data->dev, "%s: status_reg = (%x)\n",
				__func__, ret);
		pdic_data->status_reg &= ~flag; /* clear the flag */
		return ret;
	} else {
		return 0;
	}
}

static bool sm5713_poll_status(void *_data, int irq)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[5] = {0};
	u8 status[5] = {0};
	int ret = 0;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
	int event = 0;
#endif

	ret = sm5713_usbpd_multi_read(i2c, SM5713_REG_INT1, 5, intr);
	ret = sm5713_usbpd_multi_read(i2c, SM5713_REG_STATUS1, 5, status);

	if (irq == (-1)) {
		intr[0] = (status[0] & ENABLED_INT_1);
		intr[1] = (status[1] & ENABLED_INT_2);
		intr[2] = (status[2] & ENABLED_INT_3);
		intr[3] = (status[3] & ENABLED_INT_4);
		intr[4] = (status[4] & ENABLED_INT_5);
	}

	dev_info(dev, "%s: INT[0x%x 0x%x 0x%x 0x%x 0x%x], STATUS[0x%x 0x%x 0x%x 0x%x 0x%x]\n",
		__func__, intr[0], intr[1], intr[2], intr[3], intr[4],
		status[0], status[1], status[2], status[3], status[4]);

	if ((intr[0] | intr[1] | intr[2] | intr[3] | intr[4]) == 0) {
		sm5713_usbpd_abnormal_reset_check(pdic_data);
		pdic_data->status_reg |= MSG_NONE;
		goto out;
	}

	if (intr[0] & SM5713_REG_INT_STATUS1_DET_RELEASE) {
		if (pdic_data->is_sbu_abnormal_state)
			pdic_data->is_sbu_abnormal_state = false;
		if (pdic_data->is_cc_abnormal_state)
			pdic_data->is_cc_abnormal_state = false;
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
		if (status[2] & SM5713_REG_INT_STATUS3_WATER) {
			/* Water Detection CC & SBU Threshold 1Mohm */
			sm5713_usbpd_write_reg(i2c, 0x94, 0x0C);
			/* Water Detection DET Threshold 1Mohm */
			sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL2, 0x1F);		
		}
#endif
	}

#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	if ((intr[0] & SM5713_REG_INT_STATUS1_DET_DETECT) &&
			(status[2] & SM5713_REG_INT_STATUS3_WATER_RLS)) {
		/* Water Detection CC & SBU Threshold 1Mohm */
		sm5713_usbpd_write_reg(i2c, 0x94, 0x0C);
		/* Water Detection DET Threshold 1Mohm */
		sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL2, 0x1F);		
	}
#endif

	if ((intr[4] & SM5713_REG_INT_STATUS5_CC_ABNORMAL_ST) &&
			(status[4] & SM5713_REG_INT_STATUS5_CC_ABNORMAL_ST)) {
		pdic_data->is_cc_abnormal_state = true;
		if ((status[0] & SM5713_REG_INT_STATUS1_ATTACH) &&
				pdic_data->is_attached) {
			/* rp abnormal */
			sm5713_notify_rp_abnormal(_data);
		}
		pr_info("%s, CC-VBUS SHORT\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_VBUS_CC_SHORT_COUNT);
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
		event = NOTIFY_EXTRA_SYSMSG_CC_SHORT;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
	}

#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	if ((intr[2] & SM5713_REG_INT_STATUS3_WATER) &&
			(status[2] & SM5713_REG_INT_STATUS3_WATER)) {
		pdic_data->is_water_detect = true;
		sm5713_process_cc_water_det(pdic_data, WATER_MODE_ON);

		if (status[0] & SM5713_REG_INT_STATUS1_VBUSUVLO)
			schedule_delayed_work(&pdic_data->wat_pd_ta_work,
					msecs_to_jiffies(3000));
	}
#endif
	if ((intr[1] & SM5713_REG_INT_STATUS2_SRC_ADV_CHG) &&
			!(intr[3] & SM5713_REG_INT_STATUS4_RX_DONE))
		sm5713_notify_rp_current_level(data);

	if (intr[0] & SM5713_REG_INT_STATUS1_VBUSUVLO ||
			intr[1] & SM5713_REG_INT_STATUS2_VBUS_0V) {
		if (pdic_data->is_sbu_abnormal_state) {
			sm5713_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
				CCIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
		}

#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
		if (status[2] & SM5713_REG_INT_STATUS3_WATER) {
			sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL5, 0x00);
			schedule_delayed_work(&pdic_data->wat_pd_ta_work,
					msecs_to_jiffies(3000));
		}
#endif
	}

	if ((intr[0] & SM5713_REG_INT_STATUS1_VBUSPOK) &&
			(status[0] & SM5713_REG_INT_STATUS1_VBUSPOK))
		sm5713_check_cc_state(pdic_data);

#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	if (intr[2] & SM5713_REG_INT_STATUS3_WATER_RLS) {
		if ((intr[2] & SM5713_REG_INT_STATUS3_WATER) == 0 &&
				pdic_data->is_water_detect && !lpcharge) {
			pdic_data->is_water_detect = false;
			sm5713_process_cc_water_det(pdic_data, WATER_MODE_OFF);
			cancel_delayed_work(&pdic_data->wat_pd_ta_work);
			/* Water Detection CC & SBU Threshold 500kohm */
			sm5713_usbpd_write_reg(i2c, 0x94, 0x06);
			/* Water Detection DET Threshold 500kohm */
			sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL2, 0x0F);
		}
	}
#endif
	if ((intr[0] & SM5713_REG_INT_STATUS1_DETACH) &&
			(status[0] & SM5713_REG_INT_STATUS1_DETACH)) {
		pdic_data->status_reg |= PLUG_DETACH;
		sm5713_set_vconn_source(data, USBPD_VCONN_OFF);
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
		if (status[2] & SM5713_REG_INT_STATUS3_WATER_RLS &&
				pdic_data->is_water_detect && !lpcharge) {
			pdic_data->is_water_detect = false;
			sm5713_process_cc_water_det(pdic_data, WATER_MODE_OFF);
		}
#endif
		if (irq != (-1))
			sm5713_usbpd_set_vbus_dischg_gpio(pdic_data, 1);
	}

	mutex_lock(&pdic_data->lpm_mutex);
	if ((intr[0] & SM5713_REG_INT_STATUS1_ATTACH) &&
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
			(!pdic_data->is_water_detect) &&
#endif
			(status[0] & SM5713_REG_INT_STATUS1_ATTACH)) {
		pdic_data->status_reg |= PLUG_ATTACH;
		if (irq != (-1))
			sm5713_usbpd_set_vbus_dischg_gpio(pdic_data, 0);
	}
	mutex_unlock(&pdic_data->lpm_mutex);

	if (intr[3] & SM5713_REG_INT_STATUS4_HRST_RCVED) {
		pdic_data->status_reg |= MSG_HARDRESET;
		goto out;
	}

	if ((intr[1] & SM5713_REG_INT_STATUS2_RID_INT)) {
		pdic_data->status_reg |= MSG_RID;
	}

	/* JIG Case On */
	if (status[4] & SM5713_REG_INT_STATUS5_JIG_CASE_ON) {
		pdic_data->is_jig_case_on = true;
		goto out;
	}

	if (intr[2] & SM5713_REG_INT_STATUS3_VCONN_OCP)
		sm5713_set_vconn_source(data, USBPD_VCONN_OFF);

	if (intr[3] & SM5713_REG_INT_STATUS4_RX_DONE) {
		sm5713_usbpd_protocol_rx(data);
		if (sm5713_get_rx_buf_st(data))
			schedule_delayed_work(&pdic_data->rx_buf_work,
					msecs_to_jiffies(10));
	}

	if (intr[3] & SM5713_REG_INT_STATUS4_TX_DONE) {
		data->protocol_tx.status = MESSAGE_SENT;
		pdic_data->status_reg |= MSG_GOODCRC;
	}

	if (intr[3] & SM5713_REG_INT_STATUS4_TX_DISCARD) {
		data->protocol_tx.status = TRANSMISSION_ERROR;
		pdic_data->status_reg |= MSG_PASS;
		sm5713_usbpd_tx_request_discard(data);
	}

	if (intr[3] & SM5713_REG_INT_STATUS4_TX_SOP_ERR ||
			intr[3] & SM5713_REG_INT_STATUS4_TX_NSOP_ERR)
		data->protocol_tx.status = TRANSMISSION_ERROR;

	if ((intr[3] & SM5713_REG_INT_STATUS4_PRL_RST_DONE) ||
			(intr[3] & SM5713_REG_INT_STATUS4_HCRST_DONE))
		pdic_data->reset_done = 1;

out:
	if (pdic_data->status_reg & data->wait_for_msg_arrived) {
		dev_info(pdic_data->dev, "%s: wait_for_msg_arrived = (%d)\n",
				__func__, data->wait_for_msg_arrived);
		data->wait_for_msg_arrived = 0;
		complete(&data->msg_arrived);
	}

	return 0;
}

static int sm5713_hard_reset(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret;
	u8 val;

	if (pdic_data->rid != REG_RID_UNDF &&
			pdic_data->rid != REG_RID_OPEN && pdic_data->rid != REG_RID_MAX)
		return 0;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL4, &val);
	val |= SM5713_REG_CNTL_HARD_RESET_MESSAGE;
	ret = sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL4, val);
	if (ret < 0)
		goto fail;

	pdic_data->status_reg = 0;

	return 0;

fail:
	return -EIO;
}

static int sm5713_receive_message(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct sm5713_policy_data *policy = &data->policy;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	int obj_num = 0;
	int ret = 0;
	u8 val;

	ret = sm5713_read_msg_header(i2c, &pdic_data->header);
	if (ret < 0)
		dev_err(dev, "%s read msg header error\n", __func__);

	obj_num = pdic_data->header.num_data_objs;

	if (obj_num > 0) {
		ret = sm5713_read_msg_obj(i2c,
			obj_num, &pdic_data->obj[0]);
	}

	sm5713_usbpd_read_reg(i2c, SM5713_REG_RX_SRC, &val);
	/* 0: SOP, 1: SOP', 2: SOP", 3: SOP' Debug, 4: SOP" Debug */
	policy->origin_message = val & 0x0F;
	dev_info(pdic_data->dev, "%s: Origin of Message = (%x)\n",
			__func__, val);

	return ret;
}

static int sm5713_tx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret = 0;
	int count = 0;

	mutex_lock(&pdic_data->_mutex);

	/* if there is no attach, skip tx msg */
	if (pdic_data->detach_valid)
		goto done;

	ret = sm5713_write_msg_header(i2c, header->byte);
	if (ret < 0)
		goto done;

	count = header->num_data_objs;

	if (count > 0) {
		ret = sm5713_write_msg_obj(i2c, count, obj);
		if (ret < 0)
			goto done;
	}

	ret = sm5713_send_msg(data, i2c);
	if (ret < 0)
		goto done;

	pdic_data->status_reg = 0;
	data->wait_for_msg_arrived = 0;

done:
	mutex_unlock(&pdic_data->_mutex);
	return ret;
}

static int sm5713_rx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	int i;
	int count = 0;

	if (!sm5713_receive_message(data)) {
		header->word = pdic_data->header.word;
		count = pdic_data->header.num_data_objs;
		if (count > 0) {
			for (i = 0; i < count; i++)
				obj[i].object = pdic_data->obj[i].object;
		}
		pdic_data->header.word = 0; /* To clear for duplicated call */
		return 0;
	} else {
		return -EINVAL;
	}
}

static void sm5713_get_short_state(void *_data, bool *val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;

	*val = (pdic_data->is_cc_abnormal_state ||
			pdic_data->is_sbu_abnormal_state);
}

static int sm5713_get_vconn_source(void *_data, int *val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;

	*val = pdic_data->vconn_source;
	return 0;
}

/* val : sink(0) or source(1) */
static int sm5713_set_power_role(void *_data, int val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	pr_info("%s: pr_swap received to %s\n",	__func__, val==1 ? "SRC" : "SNK");

	if (val == USBPD_SINK) {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SNK;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
#endif
		sm5713_assert_rd(data);
		sm5713_set_snk(pdic_data->i2c);
	} else {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SOURCE;
#endif
		sm5713_assert_rp(data);
		sm5713_set_src(pdic_data->i2c);
	}

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (pdic_data->dual_role != NULL)
		dual_role_instance_changed(pdic_data->dual_role);
#elif defined(CONFIG_TYPEC)
	typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, val);
#endif
	pdic_data->power_role = val;
	return 0;
}

static int sm5713_get_power_role(void *_data, int *val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->power_role;
	return 0;
}

static int sm5713_set_data_role(void *_data, int val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct sm5713_usbpd_manager_data *manager;

	manager = &data->manager;
	if (!manager) {
		pr_err("%s : manager is null\n", __func__);
		return -ENODEV;
	}
	pr_info("%s: dr_swap received to %s\n", __func__, val==1 ? "DFP" : "UFP");

	/* DATA_ROLE
	 * 0 : UFP
	 * 1 : DFP
	 */
	if (val == USBPD_UFP) {
		sm5713_set_ufp(i2c);
	} else {
		sm5713_set_dfp(i2c);
	}

	pdic_data->data_role = val;

#if defined(CONFIG_CCIC_NOTIFIER)
	/* exception code for 0x45 friends firmware */
	if (manager->dr_swap_cnt < INT_MAX)
		manager->dr_swap_cnt++;
	if (manager->Vendor_ID == SAMSUNG_VENDOR_ID &&
		manager->Product_ID == FRIENDS_PRODUCT_ID &&
		manager->dr_swap_cnt > 2) {
		pr_err("%s : skip %dth dr_swap message in samsung friends", __func__, manager->dr_swap_cnt);
		return -EPERM;
	}

	sm5713_process_dr_swap(pdic_data, val);
#endif
	return 0;
}

static int sm5713_get_data_role(void *_data, int *val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->data_role;
	return 0;
}

static int sm5713_set_check_msg_pass(void *_data, int val)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;

	dev_info(pdic_data->dev, "%s: check_msg_pass val(%d)\n", __func__, val);

	pdic_data->check_msg_pass = val;

	return 0;
}

void sm5713_set_bist_carrier_m2(void *_data)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL2, &val);
	val |= 0x10;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, val);

	msleep(30);

	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL2, &val);
	val &= 0xEF;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, val);
	pr_info("%s\n", __func__);
}

void sm5713_error_recovery_mode(void *_data)
{
	struct sm5713_usbpd_data *pd_data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int power_role = 0;
	u8 data;

	sm5713_get_power_role(pd_data, &power_role);

	if (pdic_data->is_attached) {
		/* SRC to SNK pr_swap fail case when counter part is MPSM mode */
		if (power_role == USBPD_SINK) {
			sm5713_set_attach(pdic_data, TYPE_C_ATTACH_DFP);
		} else {
			/* SRC to SRC when hard reset occured 2times */
			if (pd_data->counter.hard_reset_counter > USBPD_nHardResetCount)
				sm5713_set_attach(pdic_data, TYPE_C_ATTACH_DFP);
			/* SNK to SRC pr_swap fail case */
			else {
				/* SNK_Only */
				sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL1, 0x84);
				sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL3, &data);
				data |= 0x04; /* go to ErrorRecovery State */
				sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, data);
			}
		}
	}
	pr_info("%s: power_role = %s\n", __func__, power_role ? "SRC" : "SNK");
}

void sm5713_mpsm_enter_mode_change(struct sm5713_phydrv_data *usbpd_data)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(usbpd_data->dev);
	int power_role = 0;
	sm5713_get_power_role(pd_data, &power_role);
	switch (power_role) {
		case PDIC_SINK: /* SNK */
			pr_info("%s : do nothing for SNK\n", __func__);
		break;
		case PDIC_SOURCE: /* SRC */
			sm5713_usbpd_kick_policy_work(usbpd_data->dev);
		break;
	};
}

void sm5713_mpsm_exit_mode_change(struct sm5713_phydrv_data *usbpd_data)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(usbpd_data->dev);
	int power_role = 0;
	sm5713_get_power_role(pd_data, &power_role);
	switch (power_role) {
		case PDIC_SINK: /* SNK */
			pr_info("%s : do nothing for SNK\n", __func__);
		break;
		case PDIC_SOURCE: /* SRC */
			pr_info("%s : reattach to SRC\n", __func__);
			usbpd_data->is_mpsm_exit = 1;
			reinit_completion(&usbpd_data->exit_mpsm_completion);
			sm5713_set_detach(usbpd_data, TYPE_C_ATTACH_DFP);
			msleep(400); /* debounce time */
			sm5713_set_attach(usbpd_data, TYPE_C_ATTACH_DFP);
			if (!wait_for_completion_timeout(
					&usbpd_data->exit_mpsm_completion,
					msecs_to_jiffies(400))) {
				pr_err("%s: SRC reattach failed\n", __func__);
				usbpd_data->is_mpsm_exit = 0;
				sm5713_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
			} else {
				pr_err("%s: SRC reattach success\n", __func__);
			}
		break;
	};
}

void sm5713_set_enable_pd_function(void *_data, int enable)
{
	struct sm5713_usbpd_data *data = (struct sm5713_usbpd_data *) _data;
	struct sm5713_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int power_role = 0;

	if (enable && pdic_data->is_jig_case_on) {
		pr_info("%s: Do not enable pd function.\n", __func__);
		return;
	}

	sm5713_get_power_role(data, &power_role);

	pr_info("%s: enable : (%d), power_role : (%s)\n", __func__,
		enable,	power_role ? "SOURCE" : "SINK");

	if (enable == PD_ENABLE) {
		if (power_role == PDIC_SINK)
			sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL1, 0x03);
		else
			sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL1, 0xF3);
	} else {
		sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL1, 0x00);
	}
}

static void sm5713_notify_pdic_rid(struct sm5713_phydrv_data *pdic_data,
	int rid)
{
	u8 data;

	pdic_data->is_factory_mode = false;

#if defined(CONFIG_SM5713_FACTORY_MODE)
	if (rid == RID_301K || rid == RID_523K || rid == RID_619K) {
		if (rid == RID_523K)
			pdic_data->is_factory_mode = true;
		sm5713_charger_oper_en_factory_mode(DEV_TYPE_SM5713_CCIC,
				rid, 1);
	} else if (rid == RID_OPEN) {
		sm5713_charger_oper_en_factory_mode(DEV_TYPE_SM5713_CCIC,
				rid, 0);
	}
#else
	if (rid == RID_523K)
		pdic_data->is_factory_mode = true;
#endif
	/* rid */
	sm5713_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
		rid/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);

	if (rid == REG_RID_523K || rid == REG_RID_619K || rid == REG_RID_OPEN)
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
#endif
		sm5713_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
			CCIC_NOTIFY_DETACH/*attach*/,
			USB_STATUS_NOTIFY_DETACH, 0);

	msleep(300);
	sm5713_usbpd_read_reg(pdic_data->i2c, SM5713_REG_SYS_CNTL, &data);
	data &= 0xF7;
	sm5713_usbpd_write_reg(pdic_data->i2c, SM5713_REG_SYS_CNTL, data);
}

static void sm5713_usbpd_check_rid(struct sm5713_phydrv_data *pdic_data)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 rid;
	int prev_rid = pdic_data->rid;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_FACTORY, &rid);

	dev_info(pdic_data->dev, "%s : attached rid state(%d)", __func__, rid);

	sm5713_set_enable_pd_function(pd_data, PD_DISABLE);
#if defined(CONFIG_SEC_FACTORY)
	sm5713_factory_execute_monitor(pdic_data, FAC_ABNORMAL_REPEAT_RID);
#endif

	if (rid) {
		if (prev_rid != rid) {
			pdic_data->rid = rid;
			if (prev_rid >= REG_RID_OPEN && rid >= REG_RID_OPEN)
				dev_err(pdic_data->dev,
				"%s : rid is not changed, skip notify(%d)",
				__func__, rid);
			else
				sm5713_notify_pdic_rid(pdic_data, rid);
		}

		if (rid >= REG_RID_MAX) {
			dev_err(pdic_data->dev, "%s : overflow rid value",
					__func__);
			return;
		}
	}
}

static void sm5713_execute_rx_buffer(struct work_struct *work)
{
	struct sm5713_phydrv_data *pdic_data =
		container_of(work, struct sm5713_phydrv_data, rx_buf_work.work);
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);

	if (sm5713_get_rx_buf_st(pd_data)) {
		sm5713_usbpd_protocol_rx(pd_data);

		if (pdic_data->status_reg & pd_data->wait_for_msg_arrived) {
			pr_info("%s: wait_for_msg_arrived = (%d)\n",
			__func__, pd_data->wait_for_msg_arrived);
			pd_data->wait_for_msg_arrived = 0;
			complete(&pd_data->msg_arrived);
		} else {
			sm5713_usbpd_kick_policy_work(pd_data->dev);
		}
	} else {
		pr_info("%s, Rx Buffer Empty...\n", __func__);
		return;
	}

	pr_info("%s\n", __func__);
}

#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
static void sm5713_check_water_pd_ta(struct work_struct *work)
{
	struct sm5713_phydrv_data *pdic_data = container_of(work,
			struct sm5713_phydrv_data, wat_pd_ta_work.work);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0, vbus_state = 0;

	sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS1, &vbus_state);

	pr_info("%s: vbus_state=0x%d ++\n", __func__, vbus_state);

	if (vbus_state & SM5713_REG_INT_STATUS1_VBUSPOK) {
		return;
	} else {
		sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE4, &reg_data);
		if ((reg_data >> 4) == 0x9) { /* Rp charger is detected */
			sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL5, 0x02);
			msleep(200);

			sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS1, &vbus_state);
			if (vbus_state & SM5713_REG_INT_STATUS1_VBUSPOK)
				return;
			else
				sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL5, 0x00);
		}
		schedule_delayed_work(&pdic_data->wat_pd_ta_work,
				msecs_to_jiffies(3000));
	}

	pr_info("%s: --\n", __func__);
}
#endif

#if defined(CONFIG_IF_CB_MANAGER)
struct usbpd_ops ops_usbpd = {
	.usbpd_sbu_test_read = sm5713_usbpd_sbu_test_read,
};
#endif

static int check_usb_killer(struct sm5713_phydrv_data *pdic_data)
{
#if defined(CONFIG_IF_CB_MANAGER)
	return muic_check_usb_killer(pdic_data->man);
#else
	return 0;
#endif
}

void sm5713_vbus_turn_on_ctrl(struct sm5713_phydrv_data *usbpd_data,
	bool enable)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(usbpd_data->dev);
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct power_supply *psy_otg;
	union power_supply_propval val;
	int on = !!enable;
	int ret = 0;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
	bool must_block_host = is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif
	pr_info("%s : enable=%d, must_block_host=%d\n",
		__func__, enable, must_block_host);
	if (must_block_host) {
		pr_info("%s : turn off vbus because of blocked host\n",	__func__);
		return;
	}
	if (enable && (policy->state != PE_PRS_SNK_SRC_Source_on) &&
			(policy->state != PE_SRC_Transition_to_default) &&
			check_usb_killer(usbpd_data)) {
		pr_info("%s : do not turn on VBUS because of USB Killer.\n",
			__func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_USBKILLER;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_USB_KILLER_COUNT);
#endif
		return;
	}
#endif
	pr_info("%s : enable=%d\n", __func__, enable);
	psy_otg = get_power_supply_by_name("otg");

	if (psy_otg) {
		val.intval = enable;
		usbpd_data->is_otg_vboost = enable;
		ret = psy_otg->desc->set_property(psy_otg,
				POWER_SUPPLY_PROP_ONLINE, &val);
	} else {
		pr_err("%s: Fail to get psy battery\n", __func__);
	}
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	} else {
		pr_info("otg accessory power = %d\n", on);
	}
}

static int sm5713_usbpd_notify_attach(void *data)
{
	struct sm5713_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	u8 reg_data;
	int ret = 0;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	int prev_power_role = pdic_data->power_role_dual;
#elif defined(CONFIG_TYPEC)
	int prev_power_role = pdic_data->typec_power_role;
#endif

	ret = sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_STATUS, &reg_data);
	if (ret < 0)
		dev_err(dev, "%s, i2c read CC_STATUS error\n", __func__);
	pdic_data->is_attached = 1;
	/* cc_SINK */
	if ((reg_data & SM5713_ATTACH_TYPE) == SM5713_ATTACH_SOURCE) {
		dev_info(dev, "ccstat : cc_SINK\n");
		manager->pn_flag = false;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		if (prev_power_role == DUAL_ROLE_PROP_PR_SRC)
#elif defined(CONFIG_TYPEC)
		if (prev_power_role == TYPEC_SOURCE)
#endif
			sm5713_vbus_turn_on_ctrl(pdic_data, 0);
#if defined(CONFIG_SEC_FACTORY)
		sm5713_factory_execute_monitor(pdic_data,
				FAC_ABNORMAL_REPEAT_STATE);
#endif
		sm5713_short_state_check(pdic_data);

#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
		if (pdic_data->is_water_detect)
			return -1;
#endif

		pdic_data->power_role = PDIC_SINK;
		pdic_data->data_role = USBPD_UFP;
		sm5713_set_snk(i2c);
		sm5713_set_ufp(i2c);

		if (pdic_data->is_factory_mode == true)
#if defined(CONFIG_CCIC_NOTIFIER)
		{
			/* muic */
			sm5713_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
			CCIC_NOTIFY_ATTACH/*attach*/,
			USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
			return true;
		}
#else
		return true;
#endif
		sm5713_usbpd_policy_reset(pd_data, PLUG_EVENT);
#if defined(CONFIG_CCIC_NOTIFIER)
		/* muic, battery */
		if (pdic_data->is_cc_abnormal_state ||
				pdic_data->is_sbu_abnormal_state) {
			/* rp abnormal */
			sm5713_notify_rp_abnormal(pd_data);
		} else {
			/* rp current */
			sm5713_notify_rp_current_level(pd_data);
		}
		if (!(pdic_data->rid == REG_RID_523K ||
				pdic_data->rid == REG_RID_619K)) {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
			pdic_data->power_role_dual =
					DUAL_ROLE_PROP_PR_SNK;
			if (pdic_data->dual_role != NULL &&
				pdic_data->data_role != USB_STATUS_NOTIFY_DETACH)
				dual_role_instance_changed(pdic_data->dual_role);
#elif defined(CONFIG_TYPEC)
			pdic_data->typec_power_role = TYPEC_SINK;
			typec_set_pwr_role(pdic_data->port, TYPEC_SINK);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
			if (o_notify)
				send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
			sm5713_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				CCIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
		}
#endif
		sm5713_set_vconn_source(pd_data, USBPD_VCONN_OFF);
		if (pdic_data->reset_done == 0)
			sm5713_set_enable_pd_function(pd_data, PD_ENABLE);
	/* cc_SOURCE */
	} else if (((reg_data & SM5713_ATTACH_TYPE) == SM5713_ATTACH_SINK) &&
			check_usb_killer(pdic_data) == 0) {
		dev_info(dev, "ccstat : cc_SOURCE\n");
		if (pdic_data->is_mpsm_exit) {
			complete(&pdic_data->exit_mpsm_completion);
			pdic_data->is_mpsm_exit = 0;
			dev_info(dev, "exit mpsm completion\n");
		}
		manager->pn_flag = false;
		/* add to turn on external 5V */
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		if (prev_power_role == DUAL_ROLE_PROP_PR_NONE)
#elif defined(CONFIG_TYPEC)
		if (prev_power_role == TYPEC_SINK)
#endif
		sm5713_vbus_turn_on_ctrl(pdic_data, 1);
		pdic_data->power_role = PDIC_SOURCE;
		pdic_data->data_role = USBPD_DFP;

		sm5713_set_enable_pd_function(pd_data, PD_ENABLE); /* PD Enable */
		sm5713_usbpd_policy_reset(pd_data, PLUG_EVENT);
#if defined(CONFIG_CCIC_NOTIFIER)
		/* muic */
		sm5713_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
			CCIC_NOTIFY_ATTACH/*attach*/,
			USB_STATUS_NOTIFY_ATTACH_DFP/*rprd*/, 0);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
		if (pdic_data->dual_role != NULL &&
			pdic_data->data_role != USB_STATUS_NOTIFY_DETACH)
			dual_role_instance_changed(pdic_data->dual_role);
#elif defined(CONFIG_TYPEC)
		if (!pdic_data->detach_valid &&
			pdic_data->typec_data_role == TYPEC_DEVICE) {
			sm5713_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				CCIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
			dev_info(dev, "directly called from UFP to DFP\n");
		}
		pdic_data->typec_power_role = TYPEC_SOURCE;
		typec_set_pwr_role(pdic_data->port, TYPEC_SOURCE);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		if (o_notify)
			send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 1);
#endif
		/* USB */
		sm5713_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				CCIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/, 0);
#endif /* CONFIG_CCIC_NOTIFIER */
		sm5713_set_vconn_source(pd_data, USBPD_VCONN_ON);
		sm5713_set_dfp(i2c);
		sm5713_set_src(i2c);
		msleep(180); /* don't over 310~620ms(tTypeCSinkWaitCap) */
		/* cc_AUDIO */
	} else if (((reg_data & SM5713_ATTACH_TYPE) == SM5713_ATTACH_AUDIO) ||
			((reg_data & SM5713_ATTACH_TYPE) == SM5713_ATTACH_AUDIO_CHARGE)) {
		dev_info(dev, "ccstat : cc_AUDIO\n");
		manager->acc_type = CCIC_DOCK_UNSUPPORTED_AUDIO;
		sm5713_usbpd_check_accessory(manager);
	} else if ((reg_data & SM5713_ATTACH_TYPE) == SM5713_ATTACH_DEBUG) {
		dev_info(dev, "ccstat : cc_DEBUG\n");
	} else {
		dev_err(dev, "%s, PLUG Error\n", __func__);
		return -1;
	}

	pdic_data->detach_valid = false;

	return ret;
}

static void sm5713_usbpd_notify_detach(void *data)
{
	struct sm5713_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	u8 reg_data;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	dev_info(dev, "ccstat : cc_No_Connection\n");
	sm5713_vbus_turn_on_ctrl(pdic_data, 0);
	pdic_data->is_attached = 0;
	pdic_data->status_reg = 0;
	sm5713_usbpd_reinit(dev);

	sm5713_usbpd_read_reg(i2c, SM5713_REG_SYS_CNTL, &reg_data);
	reg_data |= 0x08;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_SYS_CNTL, reg_data);

#if defined(CONFIG_SM5713_FACTORY_MODE)
	if ((pdic_data->rid == REG_RID_301K) ||
			(pdic_data->rid == REG_RID_523K) ||
			(pdic_data->rid == REG_RID_619K))
		sm5713_charger_oper_en_factory_mode(DEV_TYPE_SM5713_CCIC,
				REG_RID_OPEN, 0);
#endif
	pdic_data->rid = REG_RID_MAX;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	pd_noti.sink_status.current_pdo_num = 0;
	pd_noti.sink_status.selected_pdo_num = 0;
	pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
#endif

	sm5713_set_vconn_source(pd_data, USBPD_VCONN_OFF);
	pdic_data->detach_valid = true;
	pdic_data->is_factory_mode = false;
	pdic_data->is_cc_abnormal_state = false;
	pdic_data->is_sbu_abnormal_state = false;
	pdic_data->is_jig_case_on = false;
	pdic_data->reset_done = 0;
	pdic_data->pd_support = 0;
	sm5713_usbpd_policy_reset(pd_data, PLUG_DETACHED);
#if defined(CONFIG_TYPEC)
	if (pdic_data->partner) {
		pr_info("%s : typec_unregister_partner - pd_support : %d\n",
			__func__, pdic_data->pd_support);
		if (!IS_ERR(pdic_data->partner))
			typec_unregister_partner(pdic_data->partner);
		pdic_data->partner = NULL;
		pdic_data->typec_power_role = TYPEC_SINK;
		pdic_data->typec_data_role = TYPEC_DEVICE;
		pdic_data->pwr_opmode = TYPEC_PWR_MODE_USB;
	}
	if (pdic_data->typec_try_state_change == TRY_ROLE_SWAP_PR ||
		pdic_data->typec_try_state_change == TRY_ROLE_SWAP_DR) {
		/* Role change try and new mode detected */
		pr_info("%s : typec_reverse_completion, detached while pd/dr_swap",
			__func__);
		pdic_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		complete(&pdic_data->typec_reverse_completion);
	}
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
	/* MUIC */
	sm5713_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
		CCIC_NOTIFY_DETACH/*attach*/,
		USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
	sm5713_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
		REG_RID_OPEN/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
		/* usb or otg */
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
	pdic_data->typec_power_role = TYPEC_SINK;
#endif
	/* USB */
	sm5713_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
		CCIC_NOTIFY_DETACH/*attach*/,
		USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (!pdic_data->try_state_change)
#elif defined(CONFIG_TYPEC)
	if (!pdic_data->typec_try_state_change)
#endif
		sm5713_rprd_mode_change(pdic_data, TYPE_C_ATTACH_DRP);
#endif /* end of CONFIG_CCIC_NOTIFIER */

	/* release SBU sourcing for water detection */
	sm5713_usbpd_write_reg(i2c, 0x24, 0x00);

	/* Set Sink / UFP */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL2, 0x00);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE3, &reg_data);
	if (reg_data & 0x06) /* Reset Done */
		sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL4, 0x01);
	else /* Protocol Layer reset */
		sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL4, 0x00);
	/* Disable PD Function */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_PD_CNTL1, 0x00);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL7, &reg_data);
	reg_data &= 0xFB;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL7, reg_data);
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL3, 0x80);
	sm5713_usbpd_acc_detach(dev);
	if (manager->dp_is_connect == 1)
		sm5713_usbpd_dp_detach(dev);
	manager->dr_swap_cnt = 0;
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE5, &reg_data);
	if (reg_data != 0x0) { /* Jig Detection State = Idle(0) */
		/* Recovery for jig detection state */
		sm5713_usbpd_write_reg(i2c, 0x90, 0x02);
		sm5713_usbpd_write_reg(i2c, 0x90, 0x00);
	}
}

/* check RID again for attached cable case */
bool sm5713_second_check_rid(struct sm5713_phydrv_data *data)
{
	struct sm5713_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);

	if (sm5713_get_status(pd_data, PLUG_ATTACH)) {
		if (sm5713_usbpd_notify_attach(data) < 0)
			return false;
	}

	if (sm5713_get_status(pd_data, MSG_RID))
		sm5713_usbpd_check_rid(pdic_data);
	return true;
}

static irqreturn_t sm5713_ccic_irq_thread(int irq, void *data)
{
	struct sm5713_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int rid_status = 0;
#if defined(CONFIG_SEC_FACTORY)
	u8 rid;
#endif /* CONFIG_SEC_FACTORY */

	dev_info(dev, "%s, irq = %d\n", __func__, irq);

	mutex_lock(&pdic_data->_mutex);

	sm5713_poll_status(pd_data, irq);

	if (sm5713_get_status(pd_data, MSG_NONE))
		goto out;

#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	if (pdic_data->is_water_detect)
		goto out;
#endif

	if (sm5713_get_status(pd_data, MSG_HARDRESET)) {
		sm5713_usbpd_rx_hard_reset(dev);
		sm5713_usbpd_kick_policy_work(dev);
		goto out;
	}

	if (sm5713_get_status(pd_data, MSG_SOFTRESET)) {
		sm5713_usbpd_rx_soft_reset(pd_data);
		sm5713_usbpd_kick_policy_work(dev);
		goto out;
	}

	if (sm5713_get_status(pd_data, PLUG_ATTACH)) {
		pr_info("%s PLUG_ATTACHED +++\n", __func__);
		rid_status = sm5713_get_status(pd_data, MSG_RID);
		ret = sm5713_usbpd_notify_attach(pdic_data);
		if (ret >= 0) {
			if (rid_status)
				sm5713_usbpd_check_rid(pdic_data);
			goto hard_reset;
		}
	}

	if (sm5713_get_status(pd_data, PLUG_DETACH)) {
		pr_info("%s PLUG_DETACHED ---\n", __func__);
#if defined(CONFIG_SEC_FACTORY)
			sm5713_factory_execute_monitor(pdic_data,
					FAC_ABNORMAL_REPEAT_STATE);
			if (pdic_data->rid == REG_RID_619K) {
				msleep(250);
				sm5713_usbpd_read_reg(i2c, SM5713_REG_FACTORY, &rid);
				pr_info("%s : Detached, check if still 619K? => 0x%X\n",
						__func__, rid);
				if (rid == REG_RID_619K) {
					if (!sm5713_second_check_rid(pdic_data))
						goto out;
					else
						goto hard_reset;
				}
			}
#else
			if (pdic_data->is_otg_vboost) {
				dev_info(&i2c->dev, "%s : Detached, go back to 80uA\n",
					__func__);
				sm5713_usbpd_set_rp_scr_sel(pd_data, PLUG_CTRL_RP80);
			}
#endif /* CONFIG_SEC_FACTORY */
		sm5713_usbpd_notify_detach(pdic_data);
		goto out;
	}

	if (!sm5713_second_check_rid(pdic_data))
		goto out;

hard_reset:
	mutex_lock(&pdic_data->lpm_mutex);
	sm5713_usbpd_kick_policy_work(dev);
	mutex_unlock(&pdic_data->lpm_mutex);
out:

#if defined(CONFIG_VBUS_NOTIFIER)
	schedule_delayed_work(&pdic_data->vbus_noti_work, 0);
#endif /* CONFIG_VBUS_NOTIFIER */

	mutex_unlock(&pdic_data->_mutex);

	return IRQ_HANDLED;
}

static int sm5713_usbpd_reg_init(struct sm5713_phydrv_data *_data)
{
	struct i2c_client *i2c = _data->i2c;

	pr_info("%s", __func__);
	/* Release SNK Only */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL1, 0x80);
	/* DRP_PERIOD = 70ms, DUTY_DRP = 50% */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CC_CNTL2, 0x12);
	sm5713_check_cc_state(_data);
	/* Release SBU Sourcing */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL5, 0x00);
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL4, 0x93);
	/* Water Detection CC & SBU Threshold 500kohm */
	sm5713_usbpd_write_reg(i2c, 0x94, 0x06);
	/* Water Detection DET Threshold 500kohm */
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL2, 0x0F);	
	/* Water Release CC & SBU Threshold 600kohm */
	sm5713_usbpd_write_reg(i2c, 0x96, 0x04);
	/* Water Release condition set enable */
	sm5713_usbpd_write_reg(i2c, 0x9E, 0x43);
	/* Water Release condition [CC or SBU] */
	sm5713_usbpd_write_reg(i2c, 0xA4, 0x06);
	/* Water Release DET Threshold 600kohm */
	sm5713_usbpd_write_reg(i2c, 0xA6, 0x04);
	/* Water Release condition set disble */
	sm5713_usbpd_write_reg(i2c, 0x9E, 0x42);
#else
	sm5713_usbpd_write_reg(i2c, SM5713_REG_CORR_CNTL4, 0x92);
#endif
	/* BMC Receiver Threshold Level for Source = 0.49V, 0.77V */
	/* Debug Accessory Sink Recognition Enable */
	sm5713_usbpd_write_reg(i2c, 0xEE, 0x28);
	sm5713_usbpd_write_reg(i2c, 0x3D, 0x77);
	/* BMC Receiver Threshold Level for Sink = 0.25V, 0.49V */
	sm5713_usbpd_write_reg(i2c, 0x3E, 0x01);
	/* Wake-up when cable is attached */
	sm5713_usbpd_write_reg(i2c, 0x9A, 0x11);

	/* DET 100uA Sourcing */
	sm5713_usbpd_write_reg(i2c, 0x93, 0x0E);
	/* Water Release Period = 10s */
	sm5713_usbpd_write_reg(i2c, 0x99, 0x7D);
	/* LPM Toggle Start &&
	   Enable to Wake up when stay in gender state */
	sm5713_usbpd_write_reg(i2c, 0x9C, 0x28);
	return 0;
}

static int sm5713_usbpd_irq_init(struct sm5713_phydrv_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;
	int ret = 0;

	pr_info("%s", __func__);
	if (!_data->irq_gpio) {
		dev_err(dev, "%s No interrupt specified\n", __func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(_data->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
			sm5713_ccic_irq_thread,
			(IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND | IRQF_ONESHOT),
			"sm5713-usbpd", _data);
		if (ret < 0) {
			dev_err(dev, "%s failed to request irq(%d)\n",
					__func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			dev_err(dev, "%s failed to enable wakeup src\n",
					__func__);
	}

	if (_data->lpm_mode)
		sm5713_set_irq_enable(_data, 0, 0, 0, 0, 0);
	else
		sm5713_set_irq_enable(_data, ENABLED_INT_1, ENABLED_INT_2,
				ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	return ret;
}

#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
static void sm5713_power_off_water_check(struct sm5713_phydrv_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	u8 adc_sbu1, adc_sbu2, adc_sbu3, adc_sbu4, status3;

	pr_info("%s\n", __func__);

	sm5713_usbpd_read_reg(i2c, SM5713_REG_STATUS3, &status3);

	if (status3 & SM5713_REG_INT_STATUS3_WATER)
		return;

	sm5713_corr_sbu_volt_read(_data, &adc_sbu1, &adc_sbu2, SBU_SOURCING_OFF);
	if (adc_sbu1 > 0xA && adc_sbu2 > 0xA) {
		_data->is_water_detect = true;
		sm5713_process_cc_water_det(_data, WATER_MODE_ON);
		pr_info("%s, TA with water.\n", __func__);
		return;
	} else if (adc_sbu1 > 0x3E || adc_sbu2 > 0x3E) {
		sm5713_corr_sbu_volt_read(_data, &adc_sbu3, &adc_sbu4, SBU_SOURCING_ON);
		if ((adc_sbu1 < 0x2 || adc_sbu2 < 0x2) &&
			(adc_sbu3 > 0x3E || adc_sbu4 > 0x3E)) {
			return;
		}
		_data->is_water_detect = true;
		sm5713_process_cc_water_det(_data, WATER_MODE_ON);
		pr_info("%s, TA with water.\n", __func__);
		return;
	}
}
#endif

static int of_sm5713_ccic_dt(struct device *dev,
			struct sm5713_phydrv_data *_data)
{
	struct device_node *np_usbpd = dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);
	if (np_usbpd == NULL) {
		dev_err(dev, "%s np NULL\n", __func__);
		ret = -EINVAL;
	} else {
		_data->irq_gpio = of_get_named_gpio(np_usbpd,
							"usbpd,usbpd_int", 0);
		if (_data->irq_gpio < 0) {
			dev_err(dev, "error reading usbpd irq = %d\n",
						_data->irq_gpio);
			_data->irq_gpio = 0;
		}
		pr_info("%s irq_gpio = %d", __func__, _data->irq_gpio);

		_data->vbus_dischg_gpio = of_get_named_gpio(np_usbpd,
						"usbpd,vbus_discharging", 0);
		if (gpio_is_valid(_data->vbus_dischg_gpio))
			pr_info("%s vbus_discharging = %d, value = %d\n",
					__func__, _data->vbus_dischg_gpio,
					gpio_get_value(_data->vbus_dischg_gpio));

		if (of_find_property(np_usbpd, "vconn-en", NULL))
			_data->vconn_en = true;
		else
			_data->vconn_en = false;
	}

	return ret;
}

void sm5713_manual_JIGON(struct sm5713_phydrv_data *usbpd_data, int mode)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	u8 data;

	pr_info("usb: mode=%s", mode ? "High" : "Low");
	sm5713_usbpd_read_reg(i2c, SM5713_REG_SYS_CNTL, &data);
	data |= 0x08;
	sm5713_usbpd_write_reg(i2c, SM5713_REG_SYS_CNTL, data);
}

static int sm5713_handle_usb_external_notifier_notification(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct sm5713_phydrv_data *usbpd_data = container_of(nb,
		struct sm5713_phydrv_data, usb_external_notifier_nb);
	int ret = 0;
	int enable = *(int *)data;

	pr_info("%s : action=%lu , enable=%d\n", __func__, action, enable);
	switch (action) {
	case EXTERNAL_NOTIFY_HOSTBLOCK_PRE:
		if (enable) {
			pr_info("%s : EXTERNAL_NOTIFY_HOSTBLOCK_PRE\n", __func__);
			/* sm5713_set_enable_alternate_mode(ALTERNATE_MODE_STOP); */
			sm5713_mpsm_enter_mode_change(usbpd_data);
		} else {
		}
		break;
	case EXTERNAL_NOTIFY_HOSTBLOCK_POST:
		if (enable) {
		} else {
			pr_info("%s : EXTERNAL_NOTIFY_HOSTBLOCK_POST\n", __func__);
			/* sm5713_set_enable_alternate_mode(ALTERNATE_MODE_START); */
			sm5713_mpsm_exit_mode_change(usbpd_data);
		}
		break;
	}

	return ret;
}

static void sm5713_delayed_external_notifier_init(struct work_struct *work)
{
	int ret = 0;
	static int retry_count = 1;
	int max_retry_count = 5;
	struct delayed_work *delay_work =
		container_of(work, struct delayed_work, work);
	struct sm5713_phydrv_data *usbpd_data =
		container_of(delay_work,
			struct sm5713_phydrv_data, usb_external_notifier_register_work);

	pr_info("%s : %d = times!\n", __func__, retry_count);

	/* Register ccic handler to ccic notifier block list */
	ret = usb_external_notify_register(&usbpd_data->usb_external_notifier_nb,
		sm5713_handle_usb_external_notifier_notification,
		EXTERNAL_NOTIFY_DEV_PDIC);
	if (ret < 0) {
		pr_err("Manager notifier init time is %d.\n", retry_count);
		if (retry_count++ != max_retry_count)
			schedule_delayed_work(
			&usbpd_data->usb_external_notifier_register_work,
			msecs_to_jiffies(2000));
		else
			pr_err("fail to init external notifier\n");
	} else
		pr_info("%s : external notifier register done!\n", __func__);
}

static void sm5713_usbpd_debug_reg_log(struct work_struct *work)
{
	struct sm5713_phydrv_data *pdic_data =
		container_of(work, struct sm5713_phydrv_data,
				debug_work.work);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data[14] = {0, };

	sm5713_usbpd_read_reg(i2c, SM5713_REG_SYS_CNTL, &data[0]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_CORR_CNTL4, &data[1]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_CORR_CNTL5, &data[2]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_STATUS, &data[3]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL1, &data[4]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL3, &data[5]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_CC_CNTL7, &data[6]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL1, &data[7]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_CNTL4, &data[8]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_RX_BUF_ST, &data[9]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE0, &data[10]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE3, &data[11]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE4, &data[12]);
	sm5713_usbpd_read_reg(i2c, SM5713_REG_PD_STATE5, &data[13]);

	pr_info("%s SYS_CT:0x%02x CR_CT[4:0x%02x 5:0x%02x] CC_ST:0x%02x CC_CT[1:0x%02x 3:0x%02x 7:0x%02x] PD_CT[1:0x%02x 4:0x%02x] RX_BUF_ST:0x%02x PD_ST[0:0x%02x 3:0x%02x 4:0x%02x 5:0x%02x]\n",
			__func__, data[0], data[1], data[2], data[3], data[4],
			data[5], data[6], data[7], data[8], data[9], data[10],
			data[11], data[12], data[13]);

	if (!pdic_data->suspended)
		schedule_delayed_work(&pdic_data->debug_work,
				msecs_to_jiffies(60000));
}

static int sm5713_usbpd_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct sm5713_phydrv_data *pdic_data;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_IF_CB_MANAGER)
	struct usbpd_dev *usbpd_d;
#endif
	int ret = 0;
	u8 rid = 0;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
	pccic_data_t pccic_data;
	pccic_sysfs_property_t pccic_sysfs_prop;
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	u8 data;

	pr_info("%s start\n", __func__);
	test_i2c = i2c;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	pdic_data = kzalloc(sizeof(struct sm5713_phydrv_data), GFP_KERNEL);
	if (!pdic_data) {
		dev_err(dev, "%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	pdic_data->dev = &i2c->dev;
	pdic_data->i2c = i2c;

	ret = of_sm5713_ccic_dt(&i2c->dev, pdic_data);
	if (ret < 0)
		dev_err(dev, "%s: not found dt!\n", __func__);

	sm5713_usbpd_read_reg(i2c, SM5713_REG_FACTORY, &rid);

	pdic_data->rid = rid;
	pdic_data->lpm_mode = false;
	pdic_data->is_factory_mode = false;

	if (factory_mode) {
		if (rid != REG_RID_523K) {
			dev_err(dev, "%s : In factory mode, but RID is not 523K, RID : %x\n",
					__func__, rid);
		} else {
			dev_err(dev, "%s : In factory mode, but RID is 523K OK\n",
					__func__);
			pdic_data->is_factory_mode = true;
		}
	} else {
		sm5713_usbpd_read_reg(i2c, SM5713_REG_SYS_CNTL, &data);
		data |= 0x08;
		sm5713_usbpd_write_reg(i2c, SM5713_REG_SYS_CNTL, data);
	}

	pdic_data->check_msg_pass = false;
	pdic_data->vconn_source = USBPD_VCONN_OFF;
	pdic_data->rid = REG_RID_MAX;
	pdic_data->is_attached = 0;
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	pdic_data->is_water_detect = false;
#endif
	pdic_data->detach_valid = true;
	pdic_data->is_otg_vboost = false;
	pdic_data->is_jig_case_on = false;
	pdic_data->reset_done = 0;
	pdic_data->is_cc_abnormal_state = false;
	pdic_data->is_sbu_abnormal_state = false;
	pdic_data->pd_support = 0;
	ret = sm5713_usbpd_init(dev, pdic_data);
	if (ret < 0) {
		dev_err(dev, "failed on usbpd_init\n");
		goto err_return;
	}

	sm5713_usbpd_set_ops(dev, &sm5713_ops);

	mutex_init(&pdic_data->_mutex);
	mutex_init(&pdic_data->lpm_mutex);

	sm5713_usbpd_reg_init(pdic_data);
#if defined(CONFIG_SEC_FACTORY)
	INIT_DELAYED_WORK(&pdic_data->factory_state_work,
		sm5713_factory_check_abnormal_state);
	INIT_DELAYED_WORK(&pdic_data->factory_rid_work,
		sm5713_factory_check_normal_rid);
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
	INIT_DELAYED_WORK(&pdic_data->vbus_noti_work, sm5713_usbpd_handle_vbus);
#endif
#if defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	INIT_DELAYED_WORK(&pdic_data->wat_pd_ta_work, sm5713_check_water_pd_ta);
#endif
	INIT_DELAYED_WORK(&pdic_data->rx_buf_work, sm5713_execute_rx_buffer);
	INIT_DELAYED_WORK(&pdic_data->vbus_dischg_work,
			sm5713_vbus_dischg_work);
	INIT_DELAYED_WORK(&pdic_data->debug_work, sm5713_usbpd_debug_reg_log);
	schedule_delayed_work(&pdic_data->debug_work, msecs_to_jiffies(10000));

#if defined(CONFIG_CCIC_NOTIFIER)
	ccic_register_switch_device(1);
	/* Create a work queue for the ccic irq thread */
	pdic_data->ccic_wq
		= create_singlethread_workqueue("ccic_irq_event");
	if (!pdic_data->ccic_wq) {
		pr_err("%s failed to create work queue for ccic notifier\n",
			__func__);
		goto err_return;
	}
	if (pdic_data->rid == REG_RID_UNDF)
		pdic_data->rid = REG_RID_MAX;
	pccic_data = kzalloc(sizeof(ccic_data_t), GFP_KERNEL);
	pccic_sysfs_prop = kzalloc(sizeof(ccic_sysfs_property_t), GFP_KERNEL);
	pccic_sysfs_prop->get_property = sm5713_sysfs_get_prop;
	pccic_sysfs_prop->set_property = sm5713_sysfs_set_prop;
	pccic_sysfs_prop->property_is_writeable = sm5713_sysfs_is_writeable;
	pccic_sysfs_prop->properties = sm5713_sysfs_properties;
	pccic_sysfs_prop->num_properties = ARRAY_SIZE(sm5713_sysfs_properties);
	pccic_data->ccic_syfs_prop = pccic_sysfs_prop;
	pccic_data->drv_data = pdic_data;
	pccic_data->name = "sm5713";
	ccic_core_register_chip(pccic_data);
	ccic_misc_init(pccic_data);
	pccic_data->misc_dev->uvdm_read = sm5713_usbpd_uvdm_in_request_message;
	pccic_data->misc_dev->uvdm_write = sm5713_usbpd_uvdm_out_request_message;
	pccic_data->misc_dev->uvdm_ready = sm5713_usbpd_uvdm_ready;
	pccic_data->misc_dev->uvdm_close = sm5713_usbpd_uvdm_close;
#endif

#if defined(CONFIG_IF_CB_MANAGER)
	usbpd_d = kzalloc(sizeof(struct usbpd_dev), GFP_KERNEL);
	if (!usbpd_d) {
		dev_err(dev, "%s: failed to allocate usbpd data\n", __func__);
		ret = -ENOMEM;
		goto err_kfree1;
	}

	usbpd_d->ops = &ops_usbpd;
	usbpd_d->data = (void *)pdic_data;
	pdic_data->man = register_usbpd(usbpd_d);
#endif

#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SM5713_WATER_DETECTION_ENABLE)
	if (lpcharge)
		sm5713_power_off_water_check(pdic_data);
#endif

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	pdic_data->data_role_dual = 0;
	pdic_data->power_role_dual = 0;
	desc =
		devm_kzalloc(&i2c->dev,
				sizeof(struct dual_role_phy_desc), GFP_KERNEL);
	if (!desc) {
		pr_err("unable to allocate dual role descriptor\n");
		goto fail_init_irq;
	}

	desc->name = "otg_default";
	desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
	desc->get_property = sm5713_dual_role_get_prop;
	desc->set_property = sm5713_dual_role_set_prop;
	desc->properties = sm5713_fusb_drp_properties;
	desc->num_properties = ARRAY_SIZE(sm5713_fusb_drp_properties);
	desc->property_is_writeable = sm5713_dual_role_is_writeable;
	dual_role =
		devm_dual_role_instance_register(&i2c->dev, desc);
	dual_role->drv_data = pdic_data;
	pdic_data->dual_role = dual_role;
	pdic_data->desc = desc;
	init_completion(&pdic_data->reverse_completion);
#elif defined(CONFIG_TYPEC)
	pdic_data->typec_cap.revision = USB_TYPEC_REV_1_2;
	pdic_data->typec_cap.pd_revision = 0x300;
	pdic_data->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	pdic_data->typec_cap.pr_set = sm5713_pr_set;
	pdic_data->typec_cap.dr_set = sm5713_dr_set;
	pdic_data->typec_cap.port_type_set = sm5713_port_type_set;
	pdic_data->typec_cap.type = TYPEC_PORT_DRP;
	pdic_data->typec_power_role = TYPEC_SINK;
	pdic_data->typec_data_role = TYPEC_DEVICE;
	pdic_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
	pdic_data->port = typec_register_port(pdic_data->dev,
		&pdic_data->typec_cap);
	if (IS_ERR(pdic_data->port))
		pr_err("%s : unable to register typec_register_port\n");
	else
		pr_info("%s : success typec_register_port port=%p\n",
		pdic_data->port);
	init_completion(&pdic_data->typec_reverse_completion);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
	INIT_DELAYED_WORK(&pdic_data->role_swap_work, sm5713_role_swap_check);
	ret = sm5713_usbpd_irq_init(pdic_data);
	if (ret) {
		dev_err(dev, "%s: failed to init irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	/* initial cable detection */
	sm5713_ccic_irq_thread(-1, pdic_data);
	INIT_DELAYED_WORK(&pdic_data->usb_external_notifier_register_work,
				  sm5713_delayed_external_notifier_init);
	/* Register ccic handler to ccic notifier block list */
	ret = usb_external_notify_register(&pdic_data->usb_external_notifier_nb,
			sm5713_handle_usb_external_notifier_notification,
			EXTERNAL_NOTIFY_DEV_PDIC);
	if (ret < 0)
		schedule_delayed_work(&pdic_data->usb_external_notifier_register_work,
			msecs_to_jiffies(2000));
	else
		pr_info("%s : external notifier register done!\n", __func__);
	init_completion(&pdic_data->exit_mpsm_completion);
	pr_info("%s : sm5713 usbpd driver uploaded!\n", __func__);
	return 0;
fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, pdic_data);
#if defined(CONFIG_IF_CB_MANAGER)
	kfree(usbpd_d);
err_kfree1:
#endif
	kfree(pdic_data);
err_return:
	return ret;
}

#if defined CONFIG_PM
static int sm5713_usbpd_suspend(struct device *dev)
{
	struct sm5713_usbpd_data *_data = dev_get_drvdata(dev);
	struct sm5713_phydrv_data *pdic_data = _data->phy_driver_data;

	pdic_data->suspended = true;

	if (device_may_wakeup(dev))
		enable_irq_wake(pdic_data->i2c->irq);

	disable_irq(pdic_data->i2c->irq);
	cancel_delayed_work_sync(&pdic_data->debug_work);

	return 0;
}

static int sm5713_usbpd_resume(struct device *dev)
{
	struct sm5713_usbpd_data *_data = dev_get_drvdata(dev);
	struct sm5713_phydrv_data *pdic_data = _data->phy_driver_data;

	pdic_data->suspended = false;

	if (device_may_wakeup(dev))
		disable_irq_wake(pdic_data->i2c->irq);

	enable_irq(pdic_data->i2c->irq);
	schedule_delayed_work(&pdic_data->debug_work, msecs_to_jiffies(1500));

	return 0;
}
#endif

static int sm5713_usbpd_remove(struct i2c_client *i2c)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(&i2c->dev);
	struct sm5713_phydrv_data *_data = pd_data->phy_driver_data;

	if (_data) {
		cancel_delayed_work_sync(&_data->debug_work);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		devm_dual_role_instance_unregister(_data->dev,
		_data->dual_role);
		devm_kfree(_data->dev, _data->desc);
#elif defined(CONFIG_TYPEC)
		typec_unregister_port(_data->port);
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
		ccic_register_switch_device(0);
		ccic_misc_exit();
#endif
		disable_irq_wake(_data->i2c->irq);
		free_irq(_data->i2c->irq, _data);
		mutex_destroy(&_data->_mutex);
		sm5713_usbpd_set_vbus_dischg_gpio(_data, 0);
#if defined(CONFIG_IF_CB_MANAGER)
		kfree(_data->man->usbpd_d);
#endif
		kfree(_data);
	}
	return 0;
}

static const struct i2c_device_id sm5713_usbpd_i2c_id[] = {
	{ USBPD_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, sm5713_usbpd_i2c_id);

static const struct of_device_id sec_usbpd_i2c_dt_ids[] = {
	{ .compatible = "sm5713-usbpd,i2c" },
	{ }
};

static void sm5713_usbpd_shutdown(struct i2c_client *i2c)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(&i2c->dev);
	struct sm5713_phydrv_data *_data = pd_data->phy_driver_data;

	if (!_data->i2c)
		return;

	cancel_delayed_work_sync(&_data->debug_work);
	sm5713_usbpd_write_reg(i2c, SM5713_REG_SYS_CNTL, 0x15);
	sm5713_set_enable_pd_function(pd_data, PD_DISABLE);
	sm5713_usbpd_set_vbus_dischg_gpio(_data, 0);
}

static usbpd_phy_ops_type sm5713_ops = {
	.tx_msg			= sm5713_tx_msg,
	.rx_msg			= sm5713_rx_msg,
	.hard_reset		= sm5713_hard_reset,
	.set_power_role		= sm5713_set_power_role,
	.get_power_role		= sm5713_get_power_role,
	.set_data_role		= sm5713_set_data_role,
	.get_data_role		= sm5713_get_data_role,
	.set_vconn_source	= sm5713_set_vconn_source,
	.get_vconn_source	= sm5713_get_vconn_source,
	.set_check_msg_pass	= sm5713_set_check_msg_pass,
	.get_status		= sm5713_get_status,
	.poll_status		= sm5713_poll_status,
	.driver_reset		= sm5713_driver_reset,
	.get_short_state	= sm5713_get_short_state,
};

#if defined CONFIG_PM
const struct dev_pm_ops sm5713_usbpd_pm = {
	.suspend = sm5713_usbpd_suspend,
	.resume = sm5713_usbpd_resume,
};
#endif

static struct i2c_driver sm5713_usbpd_driver = {
	.driver		= {
		.name	= USBPD_DEV_NAME,
		.of_match_table	= sec_usbpd_i2c_dt_ids,
#if defined CONFIG_PM
		.pm	= &sm5713_usbpd_pm,
#endif /* CONFIG_PM */
	},
	.probe		= sm5713_usbpd_probe,
	.remove		= sm5713_usbpd_remove,
	.shutdown	= sm5713_usbpd_shutdown,
	.id_table	= sm5713_usbpd_i2c_id,
};

static int __init sm5713_usbpd_typec_init(void)
{
	return i2c_add_driver(&sm5713_usbpd_driver);
}
late_initcall(sm5713_usbpd_typec_init);

static void __exit sm5713_usbpd_typec_exit(void)
{
	i2c_del_driver(&sm5713_usbpd_driver);
}
module_exit(sm5713_usbpd_typec_exit);

MODULE_DESCRIPTION("SM5713 USB TYPE-C driver");
MODULE_LICENSE("GPL");
