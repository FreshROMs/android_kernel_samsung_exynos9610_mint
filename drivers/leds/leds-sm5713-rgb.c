/*
 * leds-sm5713-rgb.c - Service-LED device driver for SM5713
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
#include <linux/leds.h>
#include <linux/of_gpio.h>
#include <linux/sec_class.h>
#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>

#define SM5713_LED_CH_MAX       3

extern unsigned int lcdtype;

enum {
	PATTERN_OFF,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
};

enum {
    CLED_MODE_ALWAYS    = 0,
    CLED_MODE_DIMM      = 1,
};

struct sm5713_rgb_platform_data {
    u8 normal_powermode_current;
    u8 low_powermode_current;
    u8 br_ratio_r;
    u8 br_ratio_g;
    u8 br_ratio_b;
    u8 br_ratio_low_r;
    u8 br_ratio_low_g;
    u8 br_ratio_low_b;
    int gpio_vdd;

    u8 index_r;
    u8 index_g;
    u8 index_b;
};

struct sm5713_rgb_data {
    struct device *dev;
    struct i2c_client *i2c;

    struct sm5713_rgb_platform_data pdata;

    struct led_classdev led[SM5713_LED_CH_MAX];
    struct device *led_dev;

	unsigned int delay_on_times_ms;
	unsigned int delay_off_times_ms;

    unsigned char en_lowpower_mode;
    u8 ratio_r;
    u8 ratio_g;
    u8 ratio_b;
	u8 brightness;
};

static u8 __get_on_time_offset(u32 ms)
{
	u8 offset;

	if (ms <= 200) {
		offset = 0x0;
	} else if (ms <= 300) {
		offset = 0x1;
	} else if (ms <= 400) {
		offset = 0x2;
	} else if (ms <= 500) {
		offset = 0x3;
	} else if (ms <= 600) {
		offset = 0x4;
	} else if (ms <= 700) {
		offset = 0x5;
	} else if (ms <= 800) {
		offset = 0x6;
	} else if (ms <= 900) {
		offset = 0x7;
	} else if (ms <= 1000) {
		offset = 0x8;
	} else if (ms <= 1300) {
		offset = 0x9;
	} else if (ms <= 1600) {
		offset = 0xA;
	} else if (ms <= 1800) {
		offset = 0xB;
	} else if (ms <= 2000) {
		offset = 0xC;
	} else if (ms <= 2500) {
		offset = 0xD;
	} else if (ms <= 3000) {
		offset = 0xE;
	} else {
		offset = 0xF;
	}

	return offset;
}

static u8 __get_off_time_offset(u32 ms)
{
	u8 offset;

	if (ms <= 200) {
		offset = 0x0;
	} else if (ms <= 400) {
		offset = 0x1;
	} else if (ms <= 500) {
		offset = 0x2;
	} else if (ms <= 700) {
		offset = 0x3;
	} else if (ms <= 900) {
		offset = 0x4;
	} else if (ms <= 1000) {
		offset = 0x5;
	} else if (ms <= 1400) {
		offset = 0x6;
	} else if (ms <= 1800) {
		offset = 0x7;
	} else if (ms <= 2200) {
		offset = 0x8;
	} else if (ms <= 2600) {
		offset = 0x9;
	} else if (ms <= 3000) {
		offset = 0xA;
	} else if (ms <= 4000) {
		offset = 0xB;
	} else if (ms <= 5000) {
		offset = 0xC;
	} else if (ms <= 6000) {
		offset = 0xD;
	} else if (ms <= 8000) {
		offset = 0xE;
	} else {
		offset = 0xF;
	}

	return offset;
}

static void color_led_set_mode(struct sm5713_rgb_data *rgb, u8 index, u8 mode)
{
    u8 offset = 4 + index;

    sm5713_update_reg(rgb->i2c, SM5713_CHG_REG_LED123MODE, (mode << offset), (0x1 << offset));
}

static void color_led_set_enable(struct sm5713_rgb_data *rgb, u8 index, bool enable)
{
    u8 offset = 0 + index;

    sm5713_update_reg(rgb->i2c, SM5713_CHG_REG_LED123MODE, (enable << offset), (enable << offset));
}

static void color_led_do_reset(struct sm5713_rgb_data *rgb)
{
	sm5713_write_reg(rgb->i2c, SM5713_CHG_REG_LED123MODE, 0x0);
}

static void color_led_set_brightness(struct sm5713_rgb_data *rgb, u8 index, u8 brightness)
{
    sm5713_write_reg(rgb->i2c, SM5713_CHG_REG_LED1CNTL1 + (index * 3), brightness);
}

static void color_led_set_dimm_ctrl(struct sm5713_rgb_data *rgb, u8 index, u8 ramp_up, u8 ramp_down, u8 on_time, u8 off_time)
{
	u8 reg;

	reg = ((ramp_up & 0xf) << 4) | (ramp_down & 0xf);
	sm5713_write_reg(rgb->i2c, SM5713_CHG_REG_LED1CNTL2 + (index * 3), reg);
	reg = ((on_time & 0xf) << 4) | (off_time & 0xf);
	sm5713_write_reg(rgb->i2c, SM5713_CHG_REG_LED1CNTL2 + (index * 3), reg);
}

#define PRINT_RGB_REG_NUM   11
static void color_led_print_reg(struct sm5713_rgb_data *rgb)
{
	u8 regs[PRINT_RGB_REG_NUM] = {0x0, };
	int i;

	sm5713_bulk_read(rgb->i2c, SM5713_CHG_REG_LED123MODE, PRINT_RGB_REG_NUM, regs);

	pr_info("sm5713-rgb: print regmap\n");
	for (i = 0; i < PRINT_RGB_REG_NUM; ++i) {
		pr_info("0x%02x:0x%02x ", SM5713_CHG_REG_LED123MODE + i, regs[i]);
		if (i % 6 == 0)
			pr_info("\n");
	}
}

/**
 * sysfs:sec_class service_led attribute control support
 */

static ssize_t store_led_r(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);
	u32 brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: failed get brightness.\n", __func__);
		return ret;
	}

	color_led_set_brightness(rgb, rgb->pdata.index_r, brightness);
	color_led_set_mode(rgb, rgb->pdata.index_r, CLED_MODE_ALWAYS);
	color_led_set_enable(rgb, rgb->pdata.index_r, (brightness > 0 ? 1 : 0));

	dev_dbg(dev, "%s: curr=0x%x, mode=always LED-%s\n", __func__, brightness, (brightness) ? "ON" : "OFF");

	return count;
}

static ssize_t store_led_g(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);
	u32 brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: failed get brightness.\n", __func__);
		return ret;
	}

	color_led_set_brightness(rgb, rgb->pdata.index_g, brightness);
	color_led_set_mode(rgb, rgb->pdata.index_g, CLED_MODE_ALWAYS);
	color_led_set_enable(rgb, rgb->pdata.index_g, (brightness > 0 ? 1 : 0));

	dev_dbg(dev, "%s: curr=0x%x, mode=always LED-%s\n", __func__, brightness, (brightness) ? "ON" : "OFF");

	return count;
}

static ssize_t store_led_b(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);
	u32 brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: failed get brightness.\n", __func__);
		return ret;
	}

	color_led_set_brightness(rgb, rgb->pdata.index_b, brightness);
	color_led_set_mode(rgb, rgb->pdata.index_b, CLED_MODE_ALWAYS);
	color_led_set_enable(rgb, rgb->pdata.index_b, (brightness > 0 ? 1 : 0));

	dev_dbg(dev, "%s: curr=0x%x, mode=always LED-%s\n", __func__, brightness, (brightness) ? "ON" : "OFF");

	return count;

}

static ssize_t show_led_brightness(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", rgb->brightness);
}

static ssize_t store_led_brightness(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);
	u8 brightness;
	int ret;

	ret = kstrtou8(buf, 0, &brightness);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: failed get brightness.\n", __func__);
		return ret;
	}

	rgb->en_lowpower_mode = 0;
	rgb->brightness = brightness;
	dev_info(dev, "%s: store brightness = 0x%x\n", __func__, brightness);

	return count;
}

static ssize_t show_led_lowpower(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", rgb->en_lowpower_mode);
}

static ssize_t store_led_lowpower(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);
	u8 temp;
	int ret;

	ret = kstrtou8(buf, 0, &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s : failed get led_lowpower_mode.\n", __func__);
		return ret;
	}

	rgb->en_lowpower_mode = temp;
	dev_info(dev, "%s: led_lowpower mode = %d\n", __func__, rgb->en_lowpower_mode);

	rgb->brightness = !temp ? rgb->pdata.normal_powermode_current : rgb->pdata.low_powermode_current;

	if (temp) {	/* low power mode */
		rgb->ratio_r = rgb->pdata.br_ratio_low_r;
		rgb->ratio_g = rgb->pdata.br_ratio_low_g;
		rgb->ratio_b = rgb->pdata.br_ratio_low_b;	
	} else {	/* normal power mode */
		rgb->ratio_r = rgb->pdata.br_ratio_r;
		rgb->ratio_g = rgb->pdata.br_ratio_g;
		rgb->ratio_b = rgb->pdata.br_ratio_b;			
	}

	return count;
}

static u8 calc_led_br(struct sm5713_rgb_data *rgb, u8 brightness, u8 ratio)
{
	u8 br_curr;

	br_curr = rgb->brightness * brightness / 0xff;
	br_curr = br_curr * ratio / 100;

	br_curr = (br_curr > 0) ? br_curr : 1;

	return br_curr;
}

static ssize_t store_led_blink(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);
	u32 brightness, on_time, off_time;
	u8 led_r, led_g, led_b, br_curr, t_on, t_off;
	int ret;

	ret = sscanf(buf, "0x%8x %5d %5d", &brightness, &on_time, &off_time);
	if (!ret) {
		dev_err(dev, "%s: failed get led_blink value.\n", __func__);
		return ret;
	}
	led_r = ((brightness & 0xFF0000) >> 16);
	led_g = ((brightness & 0x00FF00) >> 8);
	led_b = ((brightness & 0x0000FF) >> 0);
	t_on = __get_on_time_offset(on_time);
	t_off = __get_off_time_offset(off_time);


	dev_info(dev, "%s: RGB=0x%02x:0x%02x:0x%02x, on_t=%d(0x%x), off_t=%d(0x%x)\n",
				__func__, led_r, led_g, led_b, on_time, t_on, off_time, t_off);

	color_led_do_reset(rgb);

	if (led_r) {
		br_curr = calc_led_br(rgb, led_r, rgb->ratio_r);
		color_led_set_brightness(rgb, rgb->pdata.index_r, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_r, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_r, 0x2, 0x2, t_on, t_off);
		color_led_set_enable(rgb, rgb->pdata.index_r, 1);
	}

	if (led_g) {
		br_curr = calc_led_br(rgb, led_g, rgb->ratio_g);
		color_led_set_brightness(rgb, rgb->pdata.index_g, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_g, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_g, 0x2, 0x2, t_on, t_off);
		color_led_set_enable(rgb, rgb->pdata.index_g, 1);
	}

	if (led_b) {
		br_curr = calc_led_br(rgb, led_b, rgb->ratio_b);
		color_led_set_brightness(rgb, rgb->pdata.index_b, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_b, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_b, 0x2, 0x2, t_on, t_off);
		color_led_set_enable(rgb, rgb->pdata.index_b, 1);
	}

	color_led_print_reg(rgb);

	return count;
}

static ssize_t store_led_pattern(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5713_rgb_data *rgb = dev_get_drvdata(dev);
	int ret, mode;
	u8 br_curr;

	dev_err(dev, "%s: dev = %p.\n", __func__, dev);
	
	ret = sscanf(buf, "%1d", &mode);
	if (!ret) {
		dev_err(dev, "%s: failed get led_pattern mode.\n", __func__);
		return ret;
	}
	rgb->brightness = rgb->en_lowpower_mode ? rgb->pdata.low_powermode_current : rgb->pdata.normal_powermode_current;

	dev_info(dev, "%s: pattern=%d\n", __func__, mode);

	color_led_do_reset(rgb);

	switch (mode) {
	case CHARGING:
		/* LED_R constant mode ON */
		br_curr = rgb->brightness * rgb->ratio_r / 100;
		color_led_set_brightness(rgb, rgb->pdata.index_r, br_curr);
		color_led_set_enable(rgb, rgb->pdata.index_r, 1);
		break;
	case CHARGING_ERR:
		/* LED_R slope mode ON (500ms to 500ms) */
		br_curr = rgb->brightness * rgb->ratio_r / 100;
		color_led_set_brightness(rgb, rgb->pdata.index_r, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_r, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_r, 0x2, 0x2, 0x3, 0x2);
		color_led_set_enable(rgb, rgb->pdata.index_r, 1);
		break;
	case MISSED_NOTI:
		/* LED_B slope mode ON (500ms to 5000ms) */
		br_curr = rgb->brightness * rgb->ratio_b / 100;
		color_led_set_brightness(rgb, rgb->pdata.index_b, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_b, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_b, 0x2, 0x2, 0x3, 0xc);
		color_led_set_enable(rgb, rgb->pdata.index_b, 1);
		break;
	case LOW_BATTERY:
		/* LED_R slope mode ON (500ms to 5000ms) */
		br_curr = rgb->brightness * rgb->ratio_r / 100;
		color_led_set_brightness(rgb, rgb->pdata.index_r, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_r, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_r, 0x2, 0x2, 0x3, 0xc);
		color_led_set_enable(rgb, rgb->pdata.index_r, 1);
		break;
	case FULLY_CHARGED:
		/* LED_G constant mode ON */
		br_curr = rgb->brightness * rgb->ratio_g / 100;
		color_led_set_brightness(rgb, rgb->pdata.index_g, br_curr);
		color_led_set_enable(rgb, rgb->pdata.index_g, 1);
		break;
	case POWERING:
		/* LED_G & LED_B slope mode ON (1000ms to 1000ms) */
		br_curr = rgb->brightness * rgb->ratio_g / 100;
		color_led_set_brightness(rgb, rgb->pdata.index_g, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_g, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_g, 0xf, 0xf, 0x8, 0x5);   /* Ramp up/down time = 1024ms */
		br_curr = rgb->brightness * rgb->ratio_b / 100;
		color_led_set_brightness(rgb, rgb->pdata.index_b, br_curr);
		color_led_set_mode(rgb, rgb->pdata.index_b, CLED_MODE_DIMM);
		color_led_set_dimm_ctrl(rgb, rgb->pdata.index_b, 0x2, 0x2, 0x8, 0x5);   /* Ramp up/down time = 4ms */
		color_led_set_enable(rgb, rgb->pdata.index_g, 1);
		color_led_set_enable(rgb, rgb->pdata.index_b, 1);
		break;
	case PATTERN_OFF:
		break;
	default:
		break;
	}

	color_led_print_reg(rgb);

	return count;
}

/* SAMSUNG specific attribute nodes */
static DEVICE_ATTR(led_r, 0660, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0660, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0660, NULL, store_led_b);
static DEVICE_ATTR(led_pattern, 0660, NULL, store_led_pattern);
static DEVICE_ATTR(led_blink, 0660, NULL,  store_led_blink);
static DEVICE_ATTR(led_brightness, 0660, show_led_brightness, store_led_brightness);
static DEVICE_ATTR(led_lowpower, 0660, show_led_lowpower,  store_led_lowpower);

static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};

#ifdef CONFIG_OF
#if 0
static inline void _decide_octa(char *octa, unsigned char octa_color)
{
	switch (octa_color) {
	case 1:
		strcpy(octa, "_bk");    break;
	case 2:
		strcpy(octa, "_wt");    break;
	default:
		break;
	}
}

static void make_property_string(char *dst, const char *src, const char *octa)
{
    strcpy(dst, src);
    strcat(dst, octa);
}
#endif 

static int sm5713_rgb_parse_dt(struct sm5713_rgb_data *rgb, unsigned char octa_color, char **leds_name)
{
	struct device_node *nproot = rgb->dev->parent->of_node;
	struct device_node *np;
	int i, ret, temp;

	np = of_find_node_by_name(nproot, "sm5713_rgb");
	if (unlikely(np == NULL)) {
		dev_err(rgb->dev, "failed find rgb node\n");
		return -ENOENT;
	}

	for (i = 0; i < SM5713_LED_CH_MAX; ++i) {
		ret = of_property_read_string_index(np, "rgb-name", i, (const char **)&leds_name[i]);
		if (!(strcmp(leds_name[i], "red")) || !(strcmp(leds_name[i], "led_r"))) {
			rgb->pdata.index_r = i;
		}
		if (!(strcmp(leds_name[i], "green")) || !(strcmp(leds_name[i], "led_g"))) {
			rgb->pdata.index_g = i;
		}
		if (!(strcmp(leds_name[i], "blue")) || !(strcmp(leds_name[i], "led_b"))) {
			rgb->pdata.index_b = i;
		}

		dev_info(rgb->dev, "rgb-name[%d] string: ""%s""\n", i, leds_name[i]);


		if (IS_ERR_VALUE(ret)) {
			return -ENOENT;
		}
	}

	rgb->pdata.gpio_vdd = of_get_named_gpio(np, "rgb,vdd-gpio", 0);
	if (rgb->pdata.gpio_vdd < 0) {
		dev_info(rgb->dev, "don't support gpio to lift up power supply!\n");
	}

/*	_decide_octa(octa, octa_color); */

	/* parsing dt:rgb-normal_powermode_current */
	ret = of_property_read_u32(np, "normal_powermode_current", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "normal_powermode_current");
	} else {
		rgb->pdata.normal_powermode_current = temp & 0xff;
	}

	/* parsing dt:rgb-low_powermode_current */
	ret = of_property_read_u32(np, "low_powermode_current", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "low_powermode_current");
	} else {
		rgb->pdata.low_powermode_current = temp & 0xff;
	}

	/* parsing dt:rgb-br_ratio_r */
	ret = of_property_read_u32(np, "br_ratio_r", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "br_ratio_r");
	} else {
		rgb->pdata.br_ratio_r = temp & 0xff;
	}

	/* parsing dt:rgb-br_ratio_g */
	ret = of_property_read_u32(np, "br_ratio_g", &temp);	
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "br_ratio_g");
	} else {
		rgb->pdata.br_ratio_g = temp & 0xff;
	}

	/* parsing dt:rgb-br_ratio_b */
	ret = of_property_read_u32(np, "br_ratio_b", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "br_ratio_b");
	} else {
		rgb->pdata.br_ratio_b = temp & 0xff;
	}

	dev_info(rgb->dev, "n_pwr=0x%x, l_pwr=0x%x, rto_r=%d, rto_g=%d, rto_b=%d gpio=%d rgb_index=(%d:%d:%d) \n",
			rgb->pdata.normal_powermode_current, rgb->pdata.low_powermode_current,
			rgb->pdata.br_ratio_r, rgb->pdata.br_ratio_g, rgb->pdata.br_ratio_b,
			rgb->pdata.gpio_vdd, rgb->pdata.index_r, rgb->pdata.index_g, rgb->pdata.index_b);


	ret = of_property_read_u32(np, "br_ratio_low_r", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "br_ratio_low_r");
	} else {
		rgb->pdata.br_ratio_low_r = temp & 0xff;
	}

	ret = of_property_read_u32(np, "br_ratio_low_g", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "br_ratio_low_g");
	} else {
		rgb->pdata.br_ratio_low_g = temp & 0xff;
	}

	ret = of_property_read_u32(np, "br_ratio_low_b", &temp);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "can't parsing [%s] in RGB dt\n", "br_ratio_low_b");
	} else {
		rgb->pdata.br_ratio_low_b = temp & 0xff;
	}	

	dev_info(rgb->dev, "rto_low_r=%d, rto_low_g=%d, rto_low_b=%d \n",
			rgb->pdata.br_ratio_low_r, rgb->pdata.br_ratio_low_g, rgb->pdata.br_ratio_low_b );
	
	return 0;
}

#endif

static int sm5713_rgb_probe(struct platform_device *pdev)
{
	struct sm5713_dev *sm5713 = dev_get_drvdata(pdev->dev.parent);
	struct sm5713_rgb_data *rgb;
	unsigned char octa_color = (lcdtype & 0x0F0000) >> 16;
	char *leds_name[SM5713_LED_CH_MAX];
	int ret;

	rgb = devm_kzalloc(&pdev->dev, sizeof(struct sm5713_rgb_data), GFP_KERNEL);
	if (unlikely(!rgb)) {
		dev_err(&pdev->dev, "%s: failed alloc_devmem\n", __func__);
		return -ENOMEM;
	}
	rgb->i2c = sm5713->charger;
	rgb->dev = &pdev->dev;

	dev_info(rgb->dev, " %s: lcdtype=0x%x, octa_color=0x%x\n", __func__, lcdtype, octa_color);

#ifdef CONFIG_OF
	ret = sm5713_rgb_parse_dt(rgb, octa_color, leds_name);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "failed parse dt (ret=%d)\n", ret);
		goto free_devm;
	}
#else

	rgb->pdata.normal_powermode_current = 0x28;
	rgb->pdata.low_powermode_current = 0x5;
	rgb->pdata.br_ratio_r = 100;
	rgb->pdata.br_ratio_g = 100;
	rgb->pdata.br_ratio_b = 100;
	rgb->pdata.br_ratio_low_r = 10;
	rgb->pdata.br_ratio_low_g = 10;
	rgb->pdata.br_ratio_low_b = 10;	
	rgb->pdata.gpio_vdd = -1;
	rgb->pdata.index_r = 0;
	rgb->pdata.index_g = 1;
	rgb->pdata.index_b = 2;
#endif

	rgb->ratio_r = rgb->pdata.br_ratio_r;
	rgb->ratio_g = rgb->pdata.br_ratio_g;
	rgb->ratio_b = rgb->pdata.br_ratio_b;
	rgb->brightness= rgb->pdata.normal_powermode_current;
	
	rgb->led_dev = sec_device_create(rgb, "led");
	if (unlikely(!rgb->led_dev)) {
		dev_err(rgb->dev, "failed create sec_class led-dev\n");
		goto free_devm;
	}

	ret = sysfs_create_group(&rgb->led_dev->kobj, &sec_led_attr_group);
	if (IS_ERR_VALUE(ret)) {
		dev_err(rgb->dev, "failed create sysfs:sec_led_attr\n");
		goto free_device;
	}
	platform_set_drvdata(pdev, rgb);

	if (rgb->pdata.gpio_vdd > 0) {
		ret = gpio_request(rgb->pdata.gpio_vdd, "sm5713-rgb_vdd_supply");
		if (ret < 0) {
			dev_err(rgb->dev, "failed request_gpio(%d) used vdd_supply\n", rgb->pdata.gpio_vdd);
		}
	}

#if defined(CONFIG_LEDS_USE_ED28) && defined(CONFIG_SEC_FACTORY)
	if (lcdtype == 0 && jig_status == false) {
		/* LED_R constant mode ON */
		color_led_set_brightness(rgb, rgb->pdata.index_r, rgb->pdata.normal_powermode_current);
		color_led_set_mode(rgb, rgb->pdata.index_r, CLED_MODE_ALWAYS);
	}
#endif

	dev_info(rgb->dev, "%s: probe done\n (rev=%d)", __func__, sm5713->pmic_rev);

	return 0;


free_device:
	sec_device_destroy(rgb->led_dev->devt);
free_devm:
	devm_kfree(&pdev->dev, rgb);

	return ret;
}

static int sm5713_rgb_remove(struct platform_device *pdev)
{
	struct sm5713_rgb_data *rgb = platform_get_drvdata(pdev);

	color_led_do_reset(rgb);

	sysfs_remove_group(&rgb->led_dev->kobj, &sec_led_attr_group);

	devm_kfree(&pdev->dev, rgb);

	return 0;
}

static void sm5713_rgb_shutdown(struct platform_device *pdev)
{
	struct sm5713_rgb_data *rgb = platform_get_drvdata(pdev);

	color_led_do_reset(rgb);
}

static struct platform_driver sm5713_rgbled_driver = {
	.driver		= {
		.name	= "sm5713-rgb",
		.owner	= THIS_MODULE,
	},
	.probe		= sm5713_rgb_probe,
	.remove		= sm5713_rgb_remove,
	.shutdown	= sm5713_rgb_shutdown,
};

static int __init sm5713_rgb_init(void)
{
	return platform_driver_register(&sm5713_rgbled_driver);
}
module_init(sm5713_rgb_init);

static void __exit sm5713_rgb_exit(void)
{
	platform_driver_unregister(&sm5713_rgbled_driver);
}
module_exit(sm5713_rgb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Flash-LED device driver for SM5713");

