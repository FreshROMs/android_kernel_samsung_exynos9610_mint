
/*
 * s2mpu09-core.c - mfd core driver for the s2mpu09
 *
 * Copyright (C) 2016 Samsung Electronics
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/samsung/s2mpu09.h>
#include <linux/mfd/samsung/s2mpu09-regulator.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#include <soc/samsung/acpm_mfd.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_TOP	0x00
#define I2C_ADDR_PMIC	0x01
#define I2C_ADDR_RTC	0x02

struct device_node *acpm_mfd_node;

static struct mfd_cell s2mpu09_devs[] = {
	{ .name = "s2mpu09-regulator", },
	{ .name = "s2mpu09-rtc", },
};

#if defined(CONFIG_EXYNOS_ACPM)
int s2mpu09_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	int ret;

	ret = exynos_acpm_read_reg(i2c->addr, reg, dest);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu09_read_reg);

int s2mpu09_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret;

	ret = exynos_acpm_bulk_read(i2c->addr, reg, count, buf);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu09_bulk_read);

int s2mpu09_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret;

	ret = exynos_acpm_write_reg(i2c->addr, reg, value);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu09_write_reg);

int s2mpu09_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret;

	ret = exynos_acpm_bulk_write(i2c->addr, reg, count, buf);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu09_bulk_write);

int s2mpu09_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	int ret;

	ret = exynos_acpm_update_reg(i2c->addr, reg, val, mask);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu09_update_reg);
#else
int s2mpu09_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu09->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&s2mpu09->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
			 MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu09_read_reg);

int s2mpu09_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu09->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mpu09->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu09_bulk_read);

int s2mpu09_read_word(struct i2c_client *i2c, u8 reg)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu09->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&s2mpu09->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu09_read_word);

int s2mpu09_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu09->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&s2mpu09->i2c_lock);
	if (ret < 0)
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu09_write_reg);

int s2mpu09_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu09->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mpu09->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu09_bulk_write);

int s2mpu09_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu09->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&s2mpu09->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu09_write_word);

int s2mpu09_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);
	int ret;
	u8 old_val, new_val;

	mutex_lock(&s2mpu09->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&s2mpu09->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu09_update_reg);

#endif

#if defined(CONFIG_OF)
static int of_s2mpu09_dt(struct device *dev,
			 struct s2mpu09_platform_data *pdata,
			 struct s2mpu09_dev *s2mpu09)
{
	struct device_node *np = dev->of_node;
	int ret, strlen;
	const char *status;
	u32 val;

	if (!np)
		return -EINVAL;

	acpm_mfd_node = np;

	pdata->irq_gpio = of_get_named_gpio(np, "s2mpu09,irq-gpio", 0);

	status = of_get_property(np, "s2mpu09,wakeup", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wakeup = true;
		else
			pdata->wakeup = false;
	}

	if (of_get_property(np, "i2c-speedy-address", NULL))
		pdata->use_i2c_speedy = true;

	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	/* WTSR, SMPL */
	pdata->wtsr_smpl = devm_kzalloc(dev, sizeof(*pdata->wtsr_smpl),
			GFP_KERNEL);
	if (!pdata->wtsr_smpl)
		return -ENOMEM;

	status = of_get_property(np, "wtsr_en", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wtsr_smpl->wtsr_en = true;
		else
			pdata->wtsr_smpl->wtsr_en = false;
	}

	status = of_get_property(np, "smpl_en", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wtsr_smpl->smpl_en = true;
		else
			pdata->wtsr_smpl->smpl_en = false;
	}

	ret = of_property_read_u32(np, "wtsr_timer_val",
			&pdata->wtsr_smpl->wtsr_timer_val);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "smpl_timer_val",
			&pdata->wtsr_smpl->smpl_timer_val);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "check_jigon", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_smpl->check_jigon = !!val;

	/* init time */
	pdata->init_time = devm_kzalloc(dev, sizeof(*pdata->init_time),
			GFP_KERNEL);
	if (!pdata->init_time)
		return -ENOMEM;

	ret = of_property_read_u32(np, "init_time,sec",
			&pdata->init_time->tm_sec);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,min",
			&pdata->init_time->tm_min);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,hour",
			&pdata->init_time->tm_hour);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mday",
			&pdata->init_time->tm_mday);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mon",
			&pdata->init_time->tm_mon);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,year",
			&pdata->init_time->tm_year);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,wday",
			&pdata->init_time->tm_wday);
	if (ret)
		return -EINVAL;

	/* rtc optimize */
	ret = of_property_read_u32(np, "osc-bias-up", &val);
	if (!ret)
		pdata->osc_bias_up = val;
	else
		pdata->osc_bias_up = -1;

	ret = of_property_read_u32(np, "rtc_cap_sel", &val);
	if (!ret)
		pdata->cap_sel = val;
	else
		pdata->cap_sel = -1;

	ret = of_property_read_u32(np, "rtc_osc_xin", &val);
	if (!ret)
		pdata->osc_xin = val;
	else
		pdata->osc_xin = -1;

	ret = of_property_read_u32(np, "rtc_osc_xout", &val);
	if (!ret)
		pdata->osc_xout = val;
	else
		pdata->osc_xout = -1;

	return 0;
}
#else
static int of_s2mpu09_dt(struct device *dev,
			 struct s2mpu09_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpu09_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct s2mpu09_dev *s2mpu09;
	struct s2mpu09_platform_data *pdata = i2c->dev.platform_data;

	u8 reg_data;
	int ret = 0;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	s2mpu09 = kzalloc(sizeof(struct s2mpu09_dev), GFP_KERNEL);
	if (!s2mpu09) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for s2mpu09\n",
								 __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
			 sizeof(struct s2mpu09_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_s2mpu09_dt(&i2c->dev, pdata, s2mpu09);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;


	s2mpu09->dev = &i2c->dev;
	s2mpu09->i2c = i2c;
	s2mpu09->irq = i2c->irq;
	s2mpu09->device_type = S2MPU09X;

	if (pdata) {
		s2mpu09->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, S2MPU09_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			s2mpu09->irq_base = pdata->irq_base;

		s2mpu09->irq_gpio = pdata->irq_gpio;
		s2mpu09->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&s2mpu09->i2c_lock);

	i2c_set_clientdata(i2c, s2mpu09);

	/* TODO */
	if (s2mpu09_read_reg(i2c, S2MPU09_PMIC_REG_PMICID, &reg_data) < 0) {
		dev_err(s2mpu09->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else {
		/* print rev */
		s2mpu09->pmic_rev = reg_data;
	}

	s2mpu09->pmic = i2c_new_dummy(i2c->adapter, I2C_ADDR_PMIC);
	s2mpu09->rtc = i2c_new_dummy(i2c->adapter, I2C_ADDR_RTC);

	if (pdata->use_i2c_speedy) {
		dev_err(s2mpu09->dev, "use_i2c_speedy was true\n");
		s2mpu09->pmic->flags |= I2C_CLIENT_SPEEDY;
		s2mpu09->rtc->flags |= I2C_CLIENT_SPEEDY;
	}

	i2c_set_clientdata(s2mpu09->pmic, s2mpu09);
	i2c_set_clientdata(s2mpu09->rtc, s2mpu09);

	pr_info("%s device found: rev.0x%2x\n", __func__, s2mpu09->pmic_rev);

	ret = s2mpu09_irq_init(s2mpu09);
	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(s2mpu09->dev, -1, s2mpu09_devs,
			ARRAY_SIZE(s2mpu09_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(s2mpu09->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(s2mpu09->dev);
err_irq_init:
	i2c_unregister_device(s2mpu09->i2c);
err_w_lock:
	mutex_destroy(&s2mpu09->i2c_lock);
err:
	kfree(s2mpu09);
	return ret;
}

static int s2mpu09_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);

	mfd_remove_devices(s2mpu09->dev);
	i2c_unregister_device(s2mpu09->i2c);
	kfree(s2mpu09);

	return 0;
}

static const struct i2c_device_id s2mpu09_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPU09 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpu09_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id s2mpu09_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpu09mfd" },
	{ },
};
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int s2mpu09_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(s2mpu09->irq);

	disable_irq(s2mpu09->irq);

	return 0;
}

static int s2mpu09_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mpu09_dev *s2mpu09 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(s2mpu09->irq);

	enable_irq(s2mpu09->irq);

	return 0;
}
#else
#define s2mpu09_suspend	NULL
#define s2mpu09_resume NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mpu09_pm = {
	.suspend_late = s2mpu09_suspend,
	.resume_early = s2mpu09_resume,
};

static struct i2c_driver s2mpu09_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &s2mpu09_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= s2mpu09_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= s2mpu09_i2c_probe,
	.remove		= s2mpu09_i2c_remove,
	.id_table	= s2mpu09_i2c_id,
};

static int __init s2mpu09_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mpu09_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mpu09_i2c_init);

static void __exit s2mpu09_i2c_exit(void)
{
	i2c_del_driver(&s2mpu09_i2c_driver);
}
module_exit(s2mpu09_i2c_exit);

MODULE_DESCRIPTION("s2mpu09 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
