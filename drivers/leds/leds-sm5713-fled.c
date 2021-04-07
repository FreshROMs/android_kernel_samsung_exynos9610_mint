/*
 * leds-sm5713-fled.c - SM5713 Flash-LEDs device driver
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

static struct sm5713_fled_data *g_sm5713_fled;
extern struct class *camera_class; /*sys/class/camera*/

/* for MUIC HV-VBUS control */
extern int muic_request_disable_afc_state(void);
extern void sm5713_request_default_power_src(void);
extern int muic_check_fled_state(bool enable, u8 mode);   /* mode:1 = FLED_MODE_TORCH, mode:2 = FLED_MODE_FLASH */
extern int sm5713_usbpd_check_fled_state(bool enable, u8 mode);

static void fled_set_mode(struct sm5713_fled_data *fled, u8 fled_index, u8 mode)
{
	switch (fled_index) {
	case SM5713_FLED_INDEX_1:
	case SM5713_FLED_INDEX_2:
		sm5713_update_reg(fled->i2c, SM5713_CHG_REG_FLED1CNTL1 + (fled_index * 2), (mode << 0), (0x3 << 0));
		break;
	case SM5713_FLED_INDEX_3:
		sm5713_update_reg(fled->i2c, SM5713_CHG_REG_FLED3CNTL, (mode << 4), (0x1 << 4));
		break;
	}
}

static void fled_set_fled_current(struct sm5713_fled_data *fled, u8 fled_index, u8 offset)
{
	switch (fled_index) {
	case SM5713_FLED_INDEX_1:
	case SM5713_FLED_INDEX_2:
		sm5713_update_reg(fled->i2c, SM5713_CHG_REG_FLED1CNTL2 + (fled_index * 2), (offset << 0), (0xf << 0));
		break;
	case SM5713_FLED_INDEX_3:
		dev_err(fled->dev, "%s: can't used fled2\n", __func__);
		break;
	}
}

static void fled_set_mled_current(struct sm5713_fled_data *fled, u8 fled_index, u8 offset)
{
	switch (fled_index) {
	case SM5713_FLED_INDEX_1:
	case SM5713_FLED_INDEX_2:
		sm5713_update_reg(fled->i2c, SM5713_CHG_REG_FLED1CNTL2 + (fled_index * 2), (offset << 4), (0x7 << 4));
		break;
	case SM5713_FLED_INDEX_3:
		sm5713_update_reg(fled->i2c, SM5713_CHG_REG_FLED3CNTL, (offset << 0), (0xf << 0));
		break;
	}
}

static int sm5713_fled_check_vbus(struct sm5713_fled_data *fled)
{
	fled->vbus_voltage = sm5713_charger_oper_get_vbus_voltage();
	if (fled->vbus_voltage > 5500) {
		muic_request_disable_afc_state();
		sm5713_request_default_power_src();
	}	
	return 0;
}

static int sm5713_fled_control(u8 fled_index, u8 fled_mode)
{
	struct sm5713_fled_data *fled = g_sm5713_fled;
	int ret = 0;

	if (g_sm5713_fled == NULL) {
		pr_err("sm5713-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	if (fled_mode == FLED_MODE_FLASH) {
		ret = gpio_request(fled->pdata->led[fled_index].fen_pin, "sm5713_fled");
		if (ret < 0) {
			dev_err(fled->dev, "%s: failed request fen-gpio(%d)", __func__, ret);

			fled_set_mode(fled, fled_index, fled_mode);
			fled->pdata->led[fled_index].used_gpio_ctrl = false;
		} else {
			gpio_direction_output(fled->pdata->led[fled_index].fen_pin, 0);
			msleep(10); //it need to delay change to boosting mode in sm5713(about 7ms)
			fled_set_mode(fled, fled_index, FLED_MODE_EXTERNAL);
			gpio_set_value(fled->pdata->led[fled_index].fen_pin, 1);
			gpio_set_value(fled->pdata->led[fled_index].men_pin, 0);
			fled->pdata->led[fled_index].used_gpio_ctrl = true;
		}
		pr_info("sm5713-fled: %s: Flash mode & used gpio = %d.\n", __func__, fled->pdata->led[fled_index].used_gpio_ctrl);
	} else if (fled_mode == FLED_MODE_TORCH) {
		ret = gpio_request(fled->pdata->led[fled_index].men_pin, "sm5713_fled");
		if (ret < 0) {
			dev_err(fled->dev, "%s: failed request men-gpio(%d)", __func__, ret);
			fled_set_mode(fled, fled_index, fled_mode);
			fled->pdata->led[fled_index].used_gpio_ctrl = false;
		} else {
			gpio_direction_output(fled->pdata->led[fled_index].men_pin, 0);

			fled_set_mode(fled, fled_index, FLED_MODE_EXTERNAL);
			gpio_set_value(fled->pdata->led[fled_index].men_pin, 1);
			gpio_set_value(fled->pdata->led[fled_index].fen_pin, 0);
			fled->pdata->led[fled_index].used_gpio_ctrl = true;
		}
		pr_info("sm5713-fled: %s: Torch mode & used gpio = %d.\n", __func__, fled->pdata->led[fled_index].used_gpio_ctrl);
	} else if (fled_mode == FLED_MODE_OFF) {
		pr_info("sm5713-fled: %s: off mode & used gpio = %d.\n", __func__, fled->pdata->led[fled_index].used_gpio_ctrl);

		fled_set_mode(fled, fled_index, fled_mode);

		if (fled->pdata->led[fled_index].used_gpio_ctrl == true) {
			gpio_set_value(fled->pdata->led[fled_index].fen_pin, 0);
			gpio_set_value(fled->pdata->led[fled_index].men_pin, 0);
			gpio_free(fled->pdata->led[fled_index].men_pin);
			gpio_free(fled->pdata->led[fled_index].fen_pin);
			fled->pdata->led[fled_index].used_gpio_ctrl = false;
		}

		if (fled->pdata->led[fled_index].en_mled == true) {
			fled->torch_on_cnt--;
			if (fled->torch_on_cnt == 0) {
				sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_TORCH, 0);
				if (fled->flash_prepare_cnt == 0) {
					muic_check_fled_state(0, FLED_MODE_TORCH);
					sm5713_usbpd_check_fled_state(0, FLED_MODE_TORCH);
				}
			}
			fled->pdata->led[fled_index].en_mled = false;
		}

		if (fled->pdata->led[fled_index].en_fled == true) {
			fled->flash_on_cnt--;
			if (fled->flash_on_cnt == 0) {
				sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_FLASH, 0);
			}
			fled->pdata->led[fled_index].en_fled = false;
		}
	} else {
		pr_err("sm5713-fled: %s: fen_pin : %d, men_pin : %d, FLED_MODE = %d\n", __func__,  fled->pdata->led[fled_index].fen_pin, fled->pdata->led[fled_index].men_pin, fled_mode);
		return -EINVAL;
	}

	return 0;
}

static int sm5713_fled_torch_on(u8 fled_index, u8 brightness)
{
	struct sm5713_fled_data *fled = g_sm5713_fled;

	pr_info("sm5713-fled: %s: start.\n", __func__);

	if (g_sm5713_fled == NULL) {
		pr_err("sm5713-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	mutex_lock(&fled->fled_mutex);

	if (brightness) {
		fled_set_mled_current(fled, fled_index, brightness);
	} else {
		fled_set_mled_current(fled, fled_index, fled->pdata->led[fled_index].torch_brightness);
	}

	if (fled->pdata->led[fled_index].en_mled == false) {
		if (fled->torch_on_cnt == 0) {
			sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_TORCH, 1);
		}
		fled->pdata->led[fled_index].en_mled = true;
		fled->torch_on_cnt++;
	}

	sm5713_fled_control(fled_index, FLED_MODE_TORCH);

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5713-fled: %s: done.\n", __func__);

	return 0;
}

static int sm5713_fled_flash_on(u8 fled_index, u8 brightness)
{
	struct sm5713_fled_data *fled = g_sm5713_fled;

	pr_info("sm5713-fled: %s: start.\n", __func__);

	if (g_sm5713_fled == NULL) {
		pr_err("sm5713-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	mutex_lock(&fled->fled_mutex);

	if (brightness) {
		fled_set_fled_current(fled, fled_index, brightness);
	} else {
		fled_set_fled_current(fled, fled_index, fled->pdata->led[fled_index].flash_brightness);
	}

	if (fled->pdata->led[fled_index].en_fled == false) {
		if (fled->flash_on_cnt == 0) {
			sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_FLASH, 1);
		}
		fled->pdata->led[fled_index].en_fled = true;
		fled->flash_on_cnt++;
	}
	sm5713_fled_control(fled_index, FLED_MODE_FLASH);

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5713-fled: %s: done.\n", __func__);

	return 0;
}

static int sm5713_fled_prepare_flash(u8 fled_index)
{
	struct sm5713_fled_data *fled = g_sm5713_fled;

	pr_info("sm5713-fled: %s: start.\n", __func__);

	if (fled == NULL) {
		pr_err("sm5713-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	if (fled_index > SM5713_FLED_INDEX_2 || fled->pdata->led[fled_index].flash_brightness == 0) {
		pr_err("sm5713-fled: %s: don't support flash mode (fled_index=%d)\n", __func__, fled_index);
		return -EPERM;
	}

	if (fled->pdata->led[fled_index].pre_fled == true) {
		pr_info("sm5713-fled: %s: already prepared\n", __func__);
		return 0;
	}

	mutex_lock(&fled->fled_mutex);

	if (fled->flash_prepare_cnt == 0) {
		sm5713_fled_check_vbus(fled);
		muic_check_fled_state(1, FLED_MODE_FLASH);
		sm5713_usbpd_check_fled_state(1, FLED_MODE_FLASH);
	}
	fled_set_fled_current(fled, fled_index, fled->pdata->led[fled_index].flash_brightness);
	fled_set_mled_current(fled, fled_index, fled->pdata->led[fled_index].torch_brightness);

	fled->flash_prepare_cnt++;
	fled->pdata->led[fled_index].pre_fled = true;

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5713-fled: %s: done.\n", __func__);

	return 0;
}

static int sm5713_fled_close_flash(u8 fled_index)
{
	struct sm5713_fled_data *fled = g_sm5713_fled;

	pr_info("sm5713-fled: %s: start.\n", __func__);

	if (fled == NULL) {
		pr_err("sm5713-fled: %s: not probe fled yet\n", __func__);
		return -ENXIO;
	}

	if (fled_index > SM5713_FLED_INDEX_2 || fled->pdata->led[fled_index].flash_brightness == 0) {
		pr_err("sm5713-fled: %s: don't support flash mode (fled_index=%d)\n", __func__, fled_index);
		return -EPERM;
	}

	if (fled->pdata->led[fled_index].pre_fled == false) {
		pr_info("sm5713-fled: %s: already closed\n", __func__);
		return 0;
	}

	mutex_lock(&fled->fled_mutex);

	fled_set_mode(fled, fled_index, FLED_MODE_OFF);
	fled->flash_prepare_cnt--;

	if (fled->flash_prepare_cnt == 0) {
		sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_TORCH, 0);
		sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_FLASH, 0);
		muic_check_fled_state(0, FLED_MODE_FLASH);
		sm5713_usbpd_check_fled_state(0, FLED_MODE_FLASH);
	}
	fled->pdata->led[fled_index].pre_fled = false;

	mutex_unlock(&fled->fled_mutex);

	pr_info("sm5713-fled: %s: done.\n", __func__);

	return 0;
}

/**
 * For export Camera flash control support
 *
 * Caution - MUST be called "sm5713_fled_prepare_flash" before
 * using to FLED. also if finished using FLED, MUST be called
 * "sm5713_fled_close_flash".
 */

int32_t sm5713_fled_mode_ctrl(u8 fled_index, int state)
{
	struct sm5713_fled_data *fled = g_sm5713_fled;
	int ret = 0;

	pr_info("sm5713-fled: %s: sm5713_fled_mode_ctrl start\n", __func__);
	
	if (g_sm5713_fled == NULL) 
	{
		pr_err("sm5713_fled: %s: g_sm5713_fled is not initialized.\n", __func__);
		return -EFAULT;	
	}

	switch (state) {

	case SM5713_FLED_MODE_OFF:
		/* FlashLight Mode OFF */
		ret = sm5713_fled_control(fled_index, FLED_MODE_OFF);
		if (ret < 0)
			pr_err("sm5713-fled: %s: SM5713_FLED_MODE_OFF(%d) failed\n", __func__, state);
		else
			pr_info("sm5713-fled: %s: SM5713_FLED_MODE_OFF(%d) done\n", __func__, state);
		break;

	case SM5713_FLED_MODE_MAIN_FLASH:
		/* FlashLight Mode Flash */
		ret = sm5713_fled_flash_on(fled_index, fled->pdata->led[fled_index].flash_brightness);
		if (ret < 0)
			pr_err("sm5713-fled: %s: SM5713_FLED_MODE_MAIN_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5713-fled: %s: SM5713_FLED_MODE_MAIN_FLASH(%d) done\n", __func__, state);
		break;

	case SM5713_FLED_MODE_TORCH_FLASH: /* TORCH FLASH */
		/* FlashLight Mode TORCH */
		ret = sm5713_fled_torch_on(fled_index, fled->pdata->led[fled_index].torch_brightness);
		if (ret < 0)
			pr_err("sm5713-fled: %s: SM5713_FLED_MODE_TORCH_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5713-fled: %s: SM5713_FLED_MODE_TORCH_FLASH(%d) done\n", __func__, state);
		break;

	case SM5713_FLED_MODE_PRE_FLASH: /* TORCH FLASH */
		/* FlashLight Mode TORCH */
		ret = sm5713_fled_torch_on(fled_index, fled->pdata->led[fled_index].preflash_brightness);
		if (ret < 0)
			pr_err("sm5713-fled: %s: SM5713_FLED_MODE_PRE_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5713-fled: %s: SM5713_FLED_MODE_PRE_FLASH(%d) done\n", __func__, state);
		break;

	case SM5713_FLED_MODE_PREPARE_FLASH:
		/* 9V -> 5V VBUS change */
		ret = sm5713_fled_prepare_flash(fled_index);
		if (ret < 0)
			pr_err("sm5713-fled: %s: SM5713_FLED_MODE_PREPARE_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5713-fled: %s: SM5713_FLED_MODE_PREPARE_FLASH(%d) done\n", __func__, state);
		break;

	case SM5713_FLED_MODE_CLOSE_FLASH:
		/* 5V -> 9V VBUS change */
		ret = sm5713_fled_close_flash(fled_index);
		if (ret < 0)
			pr_err("sm5713-fled: %s: SM5713_FLED_MODE_CLOSE_FLASH(%d) failed\n", __func__, state);
		else
			pr_info("sm5713-fled: %s: SM5713_FLED_MODE_CLOSE_FLASH(%d) done\n", __func__, state);
		break;

	default:
		/* FlashLight Mode OFF */
		ret = sm5713_fled_control(fled_index, FLED_MODE_OFF);
		if (ret < 0)
			pr_err("sm5713-fled: %s: FLED_MODE_OFF(%d) failed\n", __func__, state);
		else
			pr_info("sm5713-fled: %s: FLED_MODE_OFF(%d) done\n", __func__, state);
		break;
	}

	return ret;
}

EXPORT_SYMBOL_GPL(sm5713_fled_mode_ctrl);

/**
 *  For camera_class device file control (Torch-LED)
 */

static int get_fled_index(struct device_attribute *attr)
{
	int fled_index = -EINVAL;

	if (strcmp(attr->attr.name, "rear_flash") == 0 || strcmp(attr->attr.name, "rear_torch_flash") == 0) {
		fled_index = SM5713_FLED_INDEX_1;
	} else {
		pr_err("sm5713-fled: flash index not matched (attr=%s) \n", attr->attr.name);
	}

	return fled_index;
}

static ssize_t sm5713_rear_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int fled_index = get_fled_index(attr);
	u32 store_value;
	int ret;
	struct sm5713_fled_data *fled = g_sm5713_fled;

	if (g_sm5713_fled == NULL) {
		pr_err("sm5713-fled: %s: g_sm5713_fled NULL \n", __func__);
		return -ENODEV;
	}

	if (IS_ERR_VALUE((unsigned long)fled_index) || (buf == NULL) || kstrtouint(buf, 10, &store_value)) {
		return -ENXIO;
	}

	fled->pdata->led[fled_index].sysfs_input_data = store_value;

	dev_info(fled->dev, "%s: value=%d\n", __func__, store_value);

	mutex_lock(&fled->fled_mutex);

	if (store_value == 0) { /* 0: Torch OFF */
		if (fled->pdata->led[fled_index].en_mled == false && fled->pdata->led[fled_index].en_fled == false) {
			goto out_skip;
		}
		sm5713_fled_control(fled_index, FLED_MODE_OFF);
	} else if (store_value == 200) { /* 200 : Flash ON */
		fled_set_fled_current(fled, fled_index, 0x0); /* Set fled = 600mA */

		if (fled->pdata->led[fled_index].en_fled == false) {
			if (fled->flash_on_cnt == 0) {
				sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_FLASH, 1);
			}
			fled->pdata->led[fled_index].en_fled = true;
			fled->flash_on_cnt++;
		}
		sm5713_fled_control(fled_index, FLED_MODE_FLASH);
	} else { /* 1, 100, 1001~1010, : Torch ON */
		/* Main Torch on */
		if (store_value == 1) {
			fled_set_mled_current(fled, fled_index, 0x1); /* Set mled = 75mA(0x1) */
		/* Factory Torch on */
		} else if (store_value == 100) {
			fled_set_mled_current(fled, fled_index, 0x7);    /* Set mled=225mA */
		} else if (store_value >= 1001 && store_value <= 1010) {
			/* Torch on (Normal) */
			if (store_value-1001 > 7)
				fled_set_mled_current(fled, fled_index, 0x07); /* Max 225mA(0x7)  */
			else
				fled_set_mled_current(fled, fled_index, (store_value-1001)); /* 50mA(0x0) ~ 225mA(0x7) at 25mA step */
		} else {
			dev_err(fled->dev, "%s: failed store cmd\n", __func__);
			ret = -EINVAL;
			goto out_p;
		}
		dev_info(fled->dev, "%s: en_mled=%d, torch_on_cnt = %d \n", __func__, fled->pdata->led[fled_index].en_mled, fled->torch_on_cnt);

		if (fled->pdata->led[fled_index].en_mled == true) {
			goto out_skip;
		}

		if (fled->torch_on_cnt == 0) {
			sm5713_fled_check_vbus(fled);
			muic_check_fled_state(1, FLED_MODE_TORCH);
			sm5713_usbpd_check_fled_state(1, FLED_MODE_TORCH);
			sm5713_charger_oper_push_event(SM5713_CHARGER_OP_EVENT_TORCH, 1);
		}
		sm5713_fled_control(fled_index, FLED_MODE_TORCH);

		fled->pdata->led[fled_index].en_mled = true;
		fled->torch_on_cnt++;
	}

out_skip:
	ret = count;

out_p:
	mutex_unlock(&fled->fled_mutex);

	return count;
}

static ssize_t sm5713_rear_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int fled_index = get_fled_index(attr);
	struct sm5713_fled_data *fled = g_sm5713_fled;

	if (g_sm5713_fled == NULL) {
		pr_err("sm5713-fled: %s: g_sm5713_fled is NULL\n", __func__);
		return -ENODEV;
	}

	if (IS_ERR_VALUE((unsigned long)fled_index)) {
		return -ENOENT;
	}

	return sprintf(buf, "%d\n", fled->pdata->led[fled_index].sysfs_input_data);
}

static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, sm5713_rear_flash_show, sm5713_rear_flash_store); 

static int sm5713_fled_parse_dt(struct device *dev, struct sm5713_fled_platform_data *pdata)
{
	struct device_node *np, *c_np;
	int fled_index, ret = 0;
	u32 temp;

	np = of_find_node_by_name(NULL, "sm5713-fled");
	if (!np) {
		dev_err(dev, "%s: can't find sm5713-fled np\n", __func__);
		return -EINVAL;
	}

	for_each_child_of_node(np, c_np) {
		ret = of_property_read_u32(c_np, "id", &fled_index);
		if (ret) {
			pr_err("sm5713-fled: %s: fail to get a id\n", __func__);
			return ret;
		}

		of_property_read_u32(c_np, "flash-brightness", &temp);
		pdata->led[fled_index].flash_brightness = (temp & 0xff);
		of_property_read_u32(c_np, "preflash-brightness", &temp);
		pdata->led[fled_index].preflash_brightness = (temp & 0xff);
		of_property_read_u32(c_np, "torch-brightness", &temp);
		pdata->led[fled_index].torch_brightness = (temp & 0xff);
		of_property_read_u32(c_np, "timeout", &temp);
		pdata->led[fled_index].timeout = (temp & 0xff);

		pdata->led[fled_index].fen_pin = of_get_named_gpio(c_np, "flash-en-gpio", 0);
		pdata->led[fled_index].men_pin = of_get_named_gpio(c_np, "torch-en-gpio", 0);

		dev_info(dev, "%s: f_cur=0x%x, pre_cur=0x%x, t_cur=0x%x, tout=%d, gpio=%d:%d\n",
				__func__, pdata->led[fled_index].flash_brightness, pdata->led[fled_index].preflash_brightness,
				pdata->led[fled_index].torch_brightness, pdata->led[fled_index].timeout,
				pdata->led[fled_index].fen_pin, pdata->led[fled_index].men_pin);
	}

	dev_info(dev, "%s: parse dt done.\n", __func__);
	return 0;
}

static void sm5713_fled_init(struct sm5713_fled_data *fled)
{
	int i;

	for (i = 0; i < SM5713_FLED_MAX_NUM; ++i) {

		fled_set_mode(fled, i, FLED_MODE_OFF);
		fled->pdata->led[i].en_mled = 0;
		fled->pdata->led[i].en_fled = 0;
		fled->pdata->led[i].pre_fled = 0;
		fled->pdata->led[i].used_gpio_ctrl = 0;
	}

	fled->torch_on_cnt = 0;
	fled->flash_on_cnt = 0;
	fled->flash_prepare_cnt = 0;

	mutex_init(&fled->fled_mutex);
}

static int sm5713_fled_probe(struct platform_device *pdev)
{
	struct sm5713_dev *sm5713 = dev_get_drvdata(pdev->dev.parent);
	struct sm5713_fled_data *fled;
	int ret = 0;

	dev_info(&pdev->dev, "sm5713 fled probe start (rev=%d)\n", sm5713->pmic_rev);

	fled = devm_kzalloc(&pdev->dev, sizeof(struct sm5713_fled_data), GFP_KERNEL);
	if (unlikely(!fled)) {
		dev_err(&pdev->dev, "%s: fail to alloc_devm\n", __func__);
		return -ENOMEM;
	}
	fled->dev = &pdev->dev;
	fled->i2c = sm5713->charger;

	fled->pdata = devm_kzalloc(&pdev->dev, sizeof(struct sm5713_fled_platform_data), GFP_KERNEL);
	if (unlikely(!fled->pdata)) {
		dev_err(fled->dev, "%s: fail to alloc_pdata\n", __func__);
		ret = -ENOMEM;
		goto free_dev;
	}
	ret = sm5713_fled_parse_dt(fled->dev, fled->pdata);
	if (ret < 0) {
		goto free_pdata;
	}

	sm5713_fled_init(fled);
	g_sm5713_fled = fled;

	if (IS_ERR_OR_NULL(camera_class)) {
		dev_err(fled->dev, "%s: can't find camera_class sysfs object, didn't used rear_flash attribute\n", __func__);
		goto free_pdata;
	}

	fled->rear_fled_dev = device_create(camera_class, NULL, 0, NULL, "flash");
	if (IS_ERR(fled->rear_fled_dev)) {
		dev_err(fled->dev, "%s failed create device for rear_flash\n", __func__);
		goto free_pdata;
	}
	fled->rear_fled_dev->parent = fled->dev;

	ret = device_create_file(fled->rear_fled_dev, &dev_attr_rear_flash);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		dev_err(fled->dev, "%s failed create device file for rear_flash\n", __func__);
		goto free_device;
	} 

	dev_info(&pdev->dev, "sm5713 fled probe done.\n");

	return 0;

free_device:
	device_destroy(camera_class, fled->rear_fled_dev->devt);
free_pdata:
	devm_kfree(&pdev->dev, fled->pdata);
free_dev:
	devm_kfree(&pdev->dev, fled);

	return ret;
}

static int sm5713_fled_remove(struct platform_device *pdev)
{
	struct sm5713_fled_data *fled = platform_get_drvdata(pdev);
	int i = 0;

	device_remove_file(fled->rear_fled_dev, &dev_attr_rear_flash); 

	device_destroy(camera_class, fled->rear_fled_dev->devt);

	for (i = 0; i < SM5713_FLED_MAX_NUM ; i++)
		fled_set_mode(fled, i, FLED_MODE_OFF);

	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, fled->pdata);
	devm_kfree(&pdev->dev, fled);

	return 0;
}

static struct of_device_id sm5713_fled_match_table[] = {
	{ .compatible = "siliconmitus,sm5713-fled",},
	{},
};

static const struct platform_device_id sm5713_fled_id[] = {
	{"sm5713-fled", 0},
	{},
};

static struct platform_driver sm5713_led_driver = {
	.driver = {
		.name  = "sm5713-fled",
		.owner = THIS_MODULE,
		.of_match_table = sm5713_fled_match_table,
		},
	.probe  = sm5713_fled_probe,
	.remove = sm5713_fled_remove,
	.id_table = sm5713_fled_id,
};

static int __init sm5713_led_driver_init(void)
{
	return platform_driver_register(&sm5713_led_driver);
}
module_init(sm5713_led_driver_init);

static void __exit sm5713_led_driver_exit(void)
{
	platform_driver_unregister(&sm5713_led_driver);
}
module_exit(sm5713_led_driver_exit);


MODULE_DESCRIPTION("SM5713 LED driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
