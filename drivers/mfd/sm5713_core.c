/*
 *  sm5713_core.c - mfd core driver for the SM5713.
 *
 *  Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>
#include <linux/regulator/machine.h>
#include <linux/usb_notify.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#endif /* CONFIG_OF */

static struct mfd_cell sm5713_devs[] = {
#if defined(CONFIG_MUIC_SM5713)
	{ .name = "sm5713-muic", },
#endif  /* CONFIG_MUIC_SM5713 */
#if defined(CONFIG_FUELGAUGE_SM5713)
	{ .name = "sm5713-fuelgauge", },
#endif  /* CONFIG_FUELGAUGE_SM5713 */
#if defined(CONFIG_CHARGER_SM5713)
	{ .name = "sm5713-charger", },
#endif  /* CONFIG_CHARGER_SM5713 */
#if defined(CONFIG_LEDS_SM5713_RGB)
	{ .name = "sm5713-rgb", },
#endif
#if defined(CONFIG_LEDS_SM5713)
	{ .name = "sm5713-fled", },
#endif /* CONFIG_LEDS_SM5713 */
};

int sm5713_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5713->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5713->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}
	*dest = (ret & 0xff);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5713_read_reg);

int sm5713_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5713->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5713->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5713_bulk_read);

int sm5713_read_word(struct i2c_client *i2c, u8 reg)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5713->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_word_data(i2c, reg);
		if (ret >= 0)
			break;
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5713->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sm5713_read_word);

int sm5713_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5713->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_write_byte_data(i2c, reg, value);
		if (ret >= 0)
			break;
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5713->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sm5713_write_reg);

int sm5713_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5713->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5713->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5713_bulk_write);

int sm5713_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5713->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_write_word_data(i2c, reg, value);
		if (ret >= 0)
			break;
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5713->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5713_write_word);

int sm5713_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5713->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		for (i = 0; i < retry; i++) {
			ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
			if (ret >= 0)
				break;
			pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
		}
	}
	else {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}
	mutex_unlock(&sm5713->i2c_lock);

    return ret;
}
EXPORT_SYMBOL_GPL(sm5713_update_reg);

#if defined(CONFIG_OF)
static int of_sm5713_dt(struct device *dev, struct sm5713_platform_data *pdata)
{
	struct device_node *np_sm5713 = dev->of_node;

	if (!np_sm5713)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_sm5713, "sm5713,irq-gpio", 0);
	pdata->wakeup = of_property_read_bool(np_sm5713, "sm5713,wakeup");

	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	return 0;
}
#else
static int of_sm5713_dt(struct device *dev, struct sm5713_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int sm5713_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct sm5713_dev *sm5713;
	struct sm5713_platform_data *pdata = i2c->dev.platform_data;

	int ret = 0;
	u8 temp = 0;

	dev_info(&i2c->dev, "%s start\n", __func__);

	sm5713 = kzalloc(sizeof(struct sm5713_dev), GFP_KERNEL);
	if (!sm5713) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for sm5713\n",
								__func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5713_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err;

		}

		ret = of_sm5713_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else {
		pdata = i2c->dev.platform_data;
	}

	sm5713->dev = &i2c->dev;
	sm5713->charger = i2c;
	sm5713->irq = i2c->irq;

	if (pdata) {
		sm5713->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, SM5713_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else {
			sm5713->irq_base = pdata->irq_base;
		}

		sm5713->irq_gpio = pdata->irq_gpio;
		sm5713->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}

	mutex_init(&sm5713->i2c_lock);

	i2c_set_clientdata(i2c, sm5713);

    /* Get Device ID & Check Charger I2C transmission */
    ret = sm5713_read_reg(i2c, SM5713_CHG_REG_DEVICEID, &temp);
	if (ret < 0 || (temp & 0x7) != 0x1) {
		pr_err("%s:%s device not found on this channel (reg_data=0x%x)\n", MFD_DEV_NAME, __func__, temp);
		ret = -ENODEV;
		goto err_w_lock;
	}
    sm5713->vender_id = (temp & 0x7);
    sm5713->pmic_rev = ((temp >> 3) & 0xF);

    pr_info("%s:%s v_id=0x%x, rev=0x%x\n",
		MFD_DEV_NAME, __func__, sm5713->vender_id, sm5713->pmic_rev);

	sm5713->fuelgauge = i2c_new_dummy(i2c->adapter, SM5713_I2C_SADR_FG);
	i2c_set_clientdata(sm5713->fuelgauge, sm5713);
    /* Check FG I2C transmission */
    ret = sm5713_read_word(sm5713->fuelgauge, SM5713_FG_REG_DEVICE_ID);
    if ((unsigned int)ret > 0xFF) {
		pr_err("%s:%s fail to setup FG I2C transmission (ret=0x%x)\n", MFD_DEV_NAME, __func__, ret);
		ret = -ENODEV;
		goto err_w_lock;
    }

	sm5713->muic = i2c_new_dummy(i2c->adapter, SM5713_I2C_SADR_MUIC);
	i2c_set_clientdata(sm5713->muic, sm5713);
    /* Check MUIC I2C transmission */
    ret = sm5713_read_reg(sm5713->muic, SM5713_MUIC_REG_DeviceID, &temp);
	if (ret < 0 || temp != 0x1) {
		pr_err("%s:%s fail to setup MUIC I2C transmission (reg_data=0x%x)\n", MFD_DEV_NAME, __func__, temp);
		ret = -ENODEV;
		goto err_w_lock;
	}

	ret = sm5713_irq_init(sm5713);
	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(sm5713->dev, -1, sm5713_devs, ARRAY_SIZE(sm5713_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(sm5713->dev, pdata->wakeup);

    pr_info("%s: %s done\n", MFD_DEV_NAME, __func__);

	return ret;

err_mfd:
	mfd_remove_devices(sm5713->dev);
    sm5713_irq_exit(sm5713);
err_irq_init:
	i2c_unregister_device(sm5713->fuelgauge);
    i2c_unregister_device(sm5713->muic);
err_w_lock:
	mutex_destroy(&sm5713->i2c_lock);
err:
	kfree(sm5713);

    return ret;
}

static int sm5713_i2c_remove(struct i2c_client *i2c)
{
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);

	mfd_remove_devices(sm5713->dev);
    sm5713_irq_exit(sm5713);

	i2c_unregister_device(sm5713->muic);
    i2c_unregister_device(sm5713->fuelgauge);

    mutex_destroy(&sm5713->i2c_lock);

    kfree(sm5713);

	return 0;
}

static const struct i2c_device_id sm5713_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_SM5713 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5713_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id sm5713_i2c_dt_ids[] = {
	{ .compatible = "siliconmitus,sm5713mfd" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5713_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int sm5713_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5713->irq);

	disable_irq(sm5713->irq);

	return 0;
}

static int sm5713_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5713->irq);

	enable_irq(sm5713->irq);

	return 0;
}
#else
#define sm5713_suspend	    NULL
#define sm5713_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION

static u8 sm5713_dumpaddr_charger[] = {
    SM5713_CHG_REG_CNTL1,
    SM5713_CHG_REG_CNTL2,
    SM5713_CHG_REG_VBUSCNTL,
    SM5713_CHG_REG_CHGCNTL1,
    SM5713_CHG_REG_CHGCNTL2,
    SM5713_CHG_REG_CHGCNTL4,
    SM5713_CHG_REG_CHGCNTL5,
    SM5713_CHG_REG_CHGCNTL6,
    SM5713_CHG_REG_CHGCNTL7,
    SM5713_CHG_REG_CHGCNTL8,
    SM5713_CHG_REG_CHGCNTL9,
    SM5713_CHG_REG_CHGCNTL10,

    SM5713_CHG_REG_LED123MODE,
    SM5713_CHG_REG_LEDCNTL,
    SM5713_CHG_REG_LED1CNTL1,
    SM5713_CHG_REG_LED1CNTL2,
    SM5713_CHG_REG_LED1CNTL3,
    SM5713_CHG_REG_LED2CNTL1,
    SM5713_CHG_REG_LED2CNTL2,
    SM5713_CHG_REG_LED2CNTL3,
    SM5713_CHG_REG_LED3CNTL1,
    SM5713_CHG_REG_LED3CNTL2,
    SM5713_CHG_REG_LED3CNTL3,

    SM5713_CHG_REG_FLED1CNTL1,
    SM5713_CHG_REG_FLED1CNTL2,
    SM5713_CHG_REG_FLED2CNTL1,
    SM5713_CHG_REG_FLED2CNTL2,
    SM5713_CHG_REG_FLED3CNTL,
};

static u8 sm5713_dumpaddr_muic[] = {

};

static u8 sm5713_dumpaddr_fuelgauge[] = {

};

static int sm5713_freeze(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < ARRAY_SIZE(sm5713_dumpaddr_charger); i++)
		sm5713_read_reg(sm5713->charger, sm5713_dumpaddr_charger[i],
				&sm5713->reg_chg_dump[i]);

	for (i = 0; i < ARRAY_SIZE(sm5713_dumpaddr_muic); i++)
		sm5713_read_reg(sm5713->muic, sm5713_dumpaddr_muic[i],
				&sm5713->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(sm5713_dumpaddr_fuelgauge); i++)
		sm5713_read_word(sm5713->fuelgauge, sm5713_dumpaddr_fuelgauge[i],
				&sm5713->reg_fg_dump[i]);

	disable_irq(sm5713->irq);

	return 0;
}

static int sm5713_restore(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5713_dev *sm5713 = i2c_get_clientdata(i2c);
	int i;

	enable_irq(sm5713->irq);

	for (i = 0; i < ARRAY_SIZE(sm5713_dumpaddr_charger); i++)
		sm5713_write_reg(sm5713->charger, sm5713_dumpaddr_charger[i],
				sm5713->reg_chg_dump[i]);

	for (i = 0; i < ARRAY_SIZE(sm5713_dumpaddr_muic); i++)
		sm5713_write_reg(sm5713->muic, sm5713_dumpaddr_muic[i],
				sm5713->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(sm5713_dumpaddr_fuelgauge); i++)
		sm5713_write_word(sm5713->fuelgauge, sm5713_dumpaddr_fuelgauge[i],
				sm5713->reg_fg_dump[i]);

	return 0;
}
#endif

const struct dev_pm_ops sm5713_pm = {
	.suspend    = sm5713_suspend,
	.resume     = sm5713_resume,
#ifdef CONFIG_HIBERNATION
	.freeze     = sm5713_freeze,
	.thaw       = sm5713_restore,
	.restore    = sm5713_restore,
#endif
};

static struct i2c_driver sm5713_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	    = &sm5713_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= sm5713_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= sm5713_i2c_probe,
	.remove		= sm5713_i2c_remove,
	.id_table	= sm5713_i2c_id,
};

static int __init sm5713_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&sm5713_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(sm5713_i2c_init);

static void __exit sm5713_i2c_exit(void)
{
	i2c_del_driver(&sm5713_i2c_driver);
}
module_exit(sm5713_i2c_exit);

MODULE_DESCRIPTION("SM5713 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
