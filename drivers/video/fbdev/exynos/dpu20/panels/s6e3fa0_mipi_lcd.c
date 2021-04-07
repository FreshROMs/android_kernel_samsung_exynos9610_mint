/* drivers/video/exynos/panels/s6e3fa0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/backlight.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>

#include "s6e3fa0_param.h"
#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "../dsim.h"

#define MAX_BRIGHTNESS		255
/* set the minimum brightness value to see the screen */
#define MIN_BRIGHTNESS		0
#define DEFAULT_BRIGHTNESS	170
#define CMD_SIZE		34

static int s6e3fa0_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int get_backlight_level(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0:
		backlightlevel = 0;
		break;
	case 1 ... 29:
		backlightlevel = 0;
		break;
	case 30 ... 34:
		backlightlevel = 1;
		break;
	case 35 ... 39:
		backlightlevel = 2;
		break;
	case 40 ... 44:
		backlightlevel = 3;
		break;
	case 45 ... 49:
		backlightlevel = 4;
		break;
	case 50 ... 54:
		backlightlevel = 5;
		break;
	case 55 ... 64:
		backlightlevel = 6;
		break;
	case 65 ... 74:
		backlightlevel = 7;
		break;
	case 75 ... 83:
		backlightlevel = 8;
		break;
	case 84 ... 93:
		backlightlevel = 9;
		break;
	case 94 ... 103:
		backlightlevel = 10;
		break;
	case 104 ... 113:
		backlightlevel = 11;
		break;
	case 114 ... 122:
		backlightlevel = 12;
		break;
	case 123 ... 132:
		backlightlevel = 13;
		break;
	case 133 ... 142:
		backlightlevel = 14;
		break;
	case 143 ... 152:
		backlightlevel = 15;
		break;
	case 153 ... 162:
		backlightlevel = 16;
		break;
	case 163 ... 171:
		backlightlevel = 17;
		break;
	case 172 ... 181:
		backlightlevel = 18;
		break;
	case 182 ... 191:
		backlightlevel = 19;
		break;
	case 192 ... 201:
		backlightlevel = 20;
		break;
	case 202 ... 210:
		backlightlevel = 21;
		break;
	case 211 ... 220:
		backlightlevel = 22;
		break;
	case 221 ... 230:
		backlightlevel = 23;
		break;
	case 231 ... 240:
		backlightlevel = 24;
		break;
	case 241 ... 250:
		backlightlevel = 25;
		break;
	case 251 ... 255:
		backlightlevel = 26;
		break;
	default:
		backlightlevel = 12;
		break;
	}

	return backlightlevel;
}

static int update_brightness(struct dsim_device *dsim, int brightness)
{
	int backlightlevel;

	/* unused line */
	backlightlevel = get_backlight_level(brightness);
#if 0
	int real_br = brightness / 2;
	int id = dsim->id;
	unsigned char gamma_control[CMD_SIZE];
	unsigned char gamma_update[3];

	memcpy(gamma_control, SEQ_GAMMA_CONTROL_SET_300CD, CMD_SIZE);
	memcpy(gamma_update, SEQ_GAMMA_UPDATE, 3);

	/*
	 * In order to change brightness to be set to one of values in the
	 * gamma_control parameter. Brightness value(real_br) from 0 to 255.
	 * This value is controlled by the control bar.
	 */

	if (brightness < 70)
		real_br = 35;

	gamma_control[1] = 0;
	gamma_control[3] = 0;
	gamma_control[5] = 0;

	gamma_control[2] = real_br * 2;
	gamma_control[4] = real_br * 2;
	gamma_control[6] = real_br * 2;

	gamma_control[28] = real_br;
	gamma_control[29] = real_br;
	gamma_control[30] = real_br;

	/* It updates the changed brightness value to ddi */
	gamma_update[1] = 0x01;

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)gamma_control,
				ARRAY_SIZE(gamma_control)) < 0)
		dsim_err("fail to send gamma_control command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_GAMMA_UPDATE,
				ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("fail to send SEQ_GAMMA_UPDATE command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)gamma_update,
				ARRAY_SIZE(gamma_update)) < 0)
		dsim_err("fail to send gamma_update command.\n");
#endif
	return 0;
}

static int s6e3fa0_set_brightness(struct backlight_device *bd)
{
	struct dsim_device *dsim;
	int brightness = bd->props.brightness;
#if 0
	struct v4l2_subdev *sd;

	sd = dev_get_drvdata(bd->dev.parent);
	dsim = container_of(sd, struct dsim_device, sd);
#endif

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(dsim, brightness);
	return 1;
}

static const struct backlight_ops s6e3fa0_backlight_ops = {
	.get_brightness = s6e3fa0_get_brightness,
	.update_status = s6e3fa0_set_brightness,
};

static int s6e3fa0_probe(struct dsim_device *dsim)
{
	dsim->bd = backlight_device_register("panel", dsim->dev,
		NULL, &s6e3fa0_backlight_ops, NULL);
	if (IS_ERR(dsim->bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	dsim->bd->props.max_brightness = MAX_BRIGHTNESS;
	dsim->bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

static int s6e3fa0_displayon(struct dsim_device *dsim)
{
	lcd_init(dsim->id, &dsim->lcd_info);
	lcd_enable(dsim->id);
	return 1;
}

static int s6e3fa0_suspend(struct dsim_device *dsim)
{
	return 1;
}

static int s6e3fa0_resume(struct dsim_device *dsim)
{
	return 1;
}

struct dsim_lcd_driver s6e3fa0_mipi_lcd_driver = {
	.probe		= s6e3fa0_probe,
	.displayon	= s6e3fa0_displayon,
	.suspend	= s6e3fa0_suspend,
	.resume		= s6e3fa0_resume,
};
