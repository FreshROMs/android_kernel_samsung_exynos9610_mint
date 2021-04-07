/*
 * s2mpu09-regulator.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/s2mpu09.h>
#include <linux/mfd/samsung/s2mpu09-regulator.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/debug-snapshot.h>
#include <linux/debugfs.h>
#ifdef CONFIG_SEC_PM_DEBUG
#include <linux/sec_pm_debug.h>
#endif

static struct s2mpu09_info *static_info;
static struct regulator_desc regulators[S2MPU09_REGULATOR_MAX];

#ifdef CONFIG_DEBUG_FS
static u8 i2caddr = 0;
static u8 i2cdata = 0;
static struct i2c_client *dbgi2c = NULL;
static struct dentry *s2mpu09_root = NULL;
static struct dentry *s2mpu09_i2caddr = NULL;
static struct dentry *s2mpu09_i2cdata = NULL;
#endif

struct s2mpu09_info {
	struct regulator_dev *rdev[S2MPU09_REGULATOR_MAX];
	unsigned int opmode[S2MPU09_REGULATOR_MAX];
	int num_regulators;
	struct s2mpu09_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
};

static unsigned int s2mpu09_of_map_mode(unsigned int val) {
	switch (val) {
	case SEC_OPMODE_SUSPEND:	/* ON in Standby Mode */
		return 0x1;
	case SEC_OPMODE_MIF:		/* ON in PWREN_MIF mode */
		return 0x2;
	case SEC_OPMODE_ON:		/* ON in Normal Mode */
		return 0x3;
	default:
		return 0x3;
	}
}

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPU09_ENABLE_SHIFT;
	s2mpu09->opmode[id] = val;

	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);

	return s2mpu09_update_reg(s2mpu09->i2c, rdev->desc->enable_reg,
				  s2mpu09->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);
	u8 val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpu09_update_reg(s2mpu09->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;


	ret = s2mpu09_read_reg(s2mpu09->i2c,
				rdev->desc->enable_reg, &val);
	if (ret)
		return ret;

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);
	int ramp_shift, ramp_addr, reg_id = rdev_get_id(rdev);
	int ramp_mask = 0x03;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay/1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}

	switch (reg_id) {
	case S2MPU09_BUCK1:
	case S2MPU09_BUCK5:
	case S2MPU09_BUCK6:
		ramp_shift = 6;
		break;
	case S2MPU09_BUCK2:
	case S2MPU09_BUCK7:
		ramp_shift = 4;
		break;
	case S2MPU09_BUCK3:
	case S2MPU09_BUCK8:
		ramp_shift = 2;
		break;
	case S2MPU09_BUCK4:
	case S2MPU09_BUCK9:
		ramp_shift = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (reg_id) {
	case S2MPU09_BUCK1:
	case S2MPU09_BUCK2:
	case S2MPU09_BUCK3:
	case S2MPU09_BUCK4:
		ramp_addr = S2MPU09_PMIC_REG_BUCKRAMP1;
		break;
	case S2MPU09_BUCK5:
	case S2MPU09_BUCK6:
	case S2MPU09_BUCK7:
	case S2MPU09_BUCK8:
	case S2MPU09_BUCK9:
		ramp_addr = S2MPU09_PMIC_REG_BUCKRAMP2;
		break;
	default:
		return -EINVAL;
	}

	return s2mpu09_update_reg(s2mpu09->i2c, ramp_addr,
		ramp_value << ramp_shift, ramp_mask << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mpu09_read_reg(s2mpu09->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	int ret;
	char name[16];
	unsigned int voltage;

	/* voltage information logging to snapshot feature */
	snprintf(name, sizeof(name), "LDO%d", (reg_id - S2MPU09_LDO1) + 1);
	voltage = ((sel & rdev->desc->vsel_mask) * S2MPU09_LDO_STEP2) + S2MPU09_LDO_MIN1;
	ret = s2mpu09_update_reg(s2mpu09->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpu09_update_reg(s2mpu09->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
								unsigned sel)
{
	int ret;
	struct s2mpu09_info *s2mpu09 = rdev_get_drvdata(rdev);

	ret = s2mpu09_write_reg(s2mpu09->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto i2c_out;

	if (rdev->desc->apply_bit)
		ret = s2mpu09_update_reg(s2mpu09->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;

i2c_out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt, new_volt;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return -EINVAL;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);

	return 0;
}

static struct regulator_ops s2mpu09_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};

static struct regulator_ops s2mpu09_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
	.set_ramp_delay		= s2m_set_ramp_delay,
};

#define _BUCK(macro)	S2MPU09_BUCK##macro
#define _buck_ops(num)	s2mpu09_buck_ops##num

#define _LDO(macro)	S2MPU09_LDO##macro
#define _REG(ctrl)	S2MPU09_PMIC_REG##ctrl
#define _ldo_ops(num)	s2mpu09_ldo_ops##num
#define _TIME(macro)	S2MPU09_ENABLE_TIME##macro

#define BUCK_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU09_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU09_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU09_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu09_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, m, s, v, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPU09_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU09_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU09_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu09_of_map_mode			\
}

static struct regulator_desc regulators[S2MPU09_REGULATOR_MAX] = {
	/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L2CTRL1), _REG(_L2CTRL1), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), _LDO(_MIN2),
	_LDO(_STEP1), _REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L5CTRL), _REG(_L5CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), _LDO(_MIN1),
	_LDO(_STEP2), _REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), _LDO(_MIN1),
	_LDO(_STEP2), _REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), _LDO(_MIN1),
	_LDO(_STEP2), _REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), _LDO(_MIN1),
	_LDO(_STEP2), _REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L18CTRL), _REG(_L18CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L21CTRL), _REG(_L21CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L25CTRL), _REG(_L25CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L26CTRL), _REG(_L26CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L27CTRL), _REG(_L27CTRL), _TIME(_LDO)),
	LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L28CTRL), _REG(_L28CTRL), _TIME(_LDO)),
	LDO_DESC("LDO29", _LDO(29), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L29CTRL), _REG(_L29CTRL), _TIME(_LDO)),
	LDO_DESC("LDO30", _LDO(30), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L30CTRL), _REG(_L30CTRL), _TIME(_LDO)),
	LDO_DESC("LDO31", _LDO(31), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L31CTRL), _REG(_L31CTRL), _TIME(_LDO)),
	LDO_DESC("LDO32", _LDO(32), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L32CTRL), _REG(_L32CTRL), _TIME(_LDO)),
*/	LDO_DESC("LDO33", _LDO(33), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L33CTRL), _REG(_L33CTRL), _TIME(_LDO)),
	LDO_DESC("LDO34", _LDO(34), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L34CTRL), _REG(_L34CTRL), _TIME(_LDO)),
	LDO_DESC("LDO35", _LDO(35), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L35CTRL), _REG(_L35CTRL), _TIME(_LDO)),
	LDO_DESC("LDO36", _LDO(36), &_ldo_ops(), _LDO(_MIN1),
	_LDO(_STEP2), _REG(_L36CTRL), _REG(_L36CTRL), _TIME(_LDO)),
	LDO_DESC("LDO37", _LDO(37), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L37CTRL), _REG(_L37CTRL), _TIME(_LDO)),
	LDO_DESC("LDO38", _LDO(38), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L38CTRL), _REG(_L38CTRL), _TIME(_LDO)),
	LDO_DESC("LDO39", _LDO(39), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L39CTRL), _REG(_L39CTRL), _TIME(_LDO)),
	LDO_DESC("LDO40", _LDO(40), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L40CTRL), _REG(_L40CTRL), _TIME(_LDO)),
	LDO_DESC("LDO41", _LDO(41), &_ldo_ops(), _LDO(_MIN4),
	_LDO(_STEP2), _REG(_L41CTRL), _REG(_L41CTRL), _TIME(_LDO)),
	LDO_DESC("LDO42", _LDO(42), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP2), _REG(_L42CTRL), _REG(_L42CTRL), _TIME(_LDO)),
	LDO_DESC("LDO43", _LDO(43), &_ldo_ops(), _LDO(_MIN1),
	_LDO(_STEP2), _REG(_L43CTRL), _REG(_L43CTRL), _TIME(_LDO)),
	LDO_DESC("LDO44", _LDO(44), &_ldo_ops(), _LDO(_MIN3),
	_LDO(_STEP1), _REG(_L44CTRL), _REG(_L44CTRL), _TIME(_LDO)),
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B1OUT), _REG(_B1CTRL), _TIME(_BUCK1)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B2OUT), _REG(_B2CTRL), _TIME(_BUCK2)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B3OUT), _REG(_B3CTRL), _TIME(_BUCK3)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B4OUT), _REG(_B4CTRL), _TIME(_BUCK4)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B5OUT), _REG(_B5CTRL), _TIME(_BUCK5)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B6OUT), _REG(_B6CTRL), _TIME(_BUCK6)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B7OUT), _REG(_B7CTRL), _TIME(_BUCK7)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B8OUT1), _REG(_B8CTRL), _TIME(_BUCK8)),
	BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), _BUCK(_MIN2),
	_BUCK(_STEP2), _REG(_B9OUT1), _REG(_B9CTRL), _TIME(_BUCK9)),
/*	BUCK_DESC("BUCK10", _BUCK(10), &_buck_ops(), _BUCK(_MIN1),
	_BUCK(_STEP1), _REG(_B10OUT), _REG(_B10CTRL), _TIME(_BUCK10)),*/
};
#ifdef CONFIG_OF
static int s2mpu09_pmic_dt_parse_pdata(struct s2mpu09_dev *iodev,
					struct s2mpu09_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpu09_regulator_data *rdata;
	unsigned int i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name,
					regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(
						iodev->dev, reg_np,
						&regulators[i]);
		rdata->reg_node = reg_np;
		rdata++;
	}

	return 0;
}
#else
static int s2mpu09_pmic_dt_parse_pdata(struct s2mpu09_pmic_dev *iodev,
					struct s2mpu09_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_DEBUG_FS
static ssize_t s2mpu09_i2caddr_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t ret;

	ret = snprintf(buf, sizeof(buf), "0x%x\n", i2caddr);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t s2mpu09_i2caddr_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t len;
	u8 val;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	if (!kstrtou8(buf, 0, &val))
		i2caddr = val;

	return len;
}

static ssize_t s2mpu09_i2cdata_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t ret;

	ret = s2mpu09_read_reg(dbgi2c, i2caddr, &i2cdata);
	if (ret)
		return ret;

	ret = snprintf(buf, sizeof(buf), "0x%x\n", i2cdata);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t s2mpu09_i2cdata_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[10];
	ssize_t len, ret;
	u8 val;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	if (!kstrtou8(buf, 0, &val)) {
		ret = s2mpu09_write_reg(dbgi2c, i2caddr, val);
		if (ret < 0)
			return ret;
	}

	return len;
}

static const struct file_operations s2mpu09_i2caddr_fops = {
	.open = simple_open,
	.read = s2mpu09_i2caddr_read,
	.write = s2mpu09_i2caddr_write,
	.llseek = default_llseek,
};
static const struct file_operations s2mpu09_i2cdata_fops = {
	.open = simple_open,
	.read = s2mpu09_i2cdata_read,
	.write = s2mpu09_i2cdata_write,
	.llseek = default_llseek,
};
#endif

static int s2mpu09_pmic_probe(struct platform_device *pdev)
{
	struct s2mpu09_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu09_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu09_info *s2mpu09;
	int i, ret;

	pr_info("%s s2mpu09 pmic driver Loading start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mpu09_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu09 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu09_info),
				GFP_KERNEL);
	if (!s2mpu09)
		return -ENOMEM;

	s2mpu09->iodev = iodev;
	s2mpu09->i2c = iodev->pmic;

	mutex_init(&s2mpu09->lock);
	static_info = s2mpu09;

	platform_set_drvdata(pdev, s2mpu09);

#ifdef CONFIG_SEC_PM_DEBUG
	ret = s2mpu09_read_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_PWRONSRC,
			&pmic_onsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read PWRONSRC\n");

	ret = s2mpu09_read_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_OFFSRC,
			&pmic_offsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read OFFSRC\n");
#endif

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpu09;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu09->opmode[id] =
			regulators[id].enable_mask;

		s2mpu09->rdev[i] = regulator_register(
				&regulators[id], &config);
		if (IS_ERR(s2mpu09->rdev[i])) {
			ret = PTR_ERR(s2mpu09->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mpu09->rdev[i] = NULL;
			goto err;
		}

	}

	s2mpu09->num_regulators = pdata->num_regulators;

#ifdef CONFIG_DEBUG_FS
	dbgi2c = s2mpu09->i2c;
	s2mpu09_root = debugfs_create_dir("s2mpu09-regs", NULL);
	s2mpu09_i2caddr = debugfs_create_file("i2caddr", 0644, s2mpu09_root, NULL, &s2mpu09_i2caddr_fops);
	s2mpu09_i2cdata = debugfs_create_file("i2cdata", 0644, s2mpu09_root, NULL, &s2mpu09_i2cdata_fops);
#endif

#ifdef CONFIG_SOC_EXYNOS9610
	/* SELMIF settings.
	   LDO2/4/5/6/7/8/11/12/13/14/36/43: PWREN_MIF
	   LDO10: PWREN
	 */
	s2mpu09_write_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_SELMIF0, 0xbf);
	s2mpu09_update_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_SELMIF1, 0xC7, 0xC7);
#endif

	pr_info("%s s2mpu09 pmic driver Loading end\n", __func__);
	s2mpu09_update_reg(s2mpu09->i2c, S2MPU09_PMIC_REG_RTCBUF, 0x4, 0x4);

	return 0;
err:
	for (i = 0; i < S2MPU09_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu09->rdev[i]);

	return ret;
}

static int s2mpu09_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu09_info *s2mpu09 = platform_get_drvdata(pdev);
	int i;

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(s2mpu09_i2cdata);
	debugfs_remove_recursive(s2mpu09_i2caddr);
	debugfs_remove_recursive(s2mpu09_root);
#endif

	for (i = 0; i < S2MPU09_REGULATOR_MAX; i++)
		regulator_unregister(s2mpu09->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mpu09_pmic_id[] = {
	{ "s2mpu09-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu09_pmic_id);

static struct platform_driver s2mpu09_pmic_driver = {
	.driver = {
		.name = "s2mpu09-regulator",
		.owner = THIS_MODULE,
	},
	.probe = s2mpu09_pmic_probe,
	.remove = s2mpu09_pmic_remove,
	.id_table = s2mpu09_pmic_id,
};

static int __init s2mpu09_pmic_init(void)
{
	return platform_driver_register(&s2mpu09_pmic_driver);
}
subsys_initcall(s2mpu09_pmic_init);

static void __exit s2mpu09_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu09_pmic_driver);
}
module_exit(s2mpu09_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPU09 Regulator Driver");
MODULE_LICENSE("GPL");
