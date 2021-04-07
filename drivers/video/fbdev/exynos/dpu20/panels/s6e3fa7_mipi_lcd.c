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
#include <linux/fb.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/notifier.h>

#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "../dsim.h"

#define MAX_VOLTAGE_PULSE	0x04
#define MAX_BRIGHTNESS		255
/* set the minimum brightness value to see the screen */
#define MIN_BRIGHTNESS		0
#define DEFAULT_BRIGHTNESS	98

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED	1
#define PANEL_STATE_SUSPENDING	2

struct panel_private {
	unsigned int lcd_connected;
	void *par;
};

struct lcd_info {
	unsigned int	connected;
	unsigned int	state;
	struct dsim_device	*dsim;
	struct mutex	lock;
	struct notifier_block	fb_notifier;
};

struct panel_private panel_priv;

static BLOCKING_NOTIFIER_HEAD(decon_notifier_list);

int decon_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&decon_notifier_list, nb);
}

int decon_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&decon_notifier_list, nb);
}

int decon_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&decon_notifier_list, val, v);
}
static int decon_notifier_event(struct notifier_block *this,
	unsigned long val, void *v)
{
	if (decon_notifier_list.head == NULL)
		return NOTIFY_DONE;

	switch (val) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	/*pr_info("lcd panel: %s: %02lx\n", __func__, val);*/

	decon_notifier_call_chain(val, v);

	return NOTIFY_DONE;
}

static int s6e3fa7_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int update_brightness(struct dsim_device *dsim, int brightness)
{
	#if 0
	int vol_pulse;
	int id = dsim->id;
	unsigned char gamma_update[3];
	unsigned char aor_control[12];

	if (dsim->state != DSIM_STATE_HSCLKEN) {
		return 0;
	}

	memcpy(gamma_update, SEQ_GAMMA_UPDATE, 3);
	memcpy(aor_control, SEQ_AOR_CONTROL, 12);

	/*
	 * In order to change brightness to be set to one of values in the
	 * gamma_control parameter. Brightness value(real_br) from 0 to 255.
	 * This value is controlled by the control bar.
	 */
	if (brightness > 100)
		vol_pulse = MAX_VOLTAGE_PULSE + ((255 - brightness) * 10);
	else
		vol_pulse = MAX_VOLTAGE_PULSE + 0x604 + ((101 - brightness) * 4);

	aor_control[2] = aor_control[4] = 0xFF & vol_pulse;
	aor_control[1] = aor_control[3] = ((0xF00 & vol_pulse) >> 8);

	/* It updates the changed brightness value to ddi */
	gamma_update[1] = 0x00;

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)aor_control,
				ARRAY_SIZE(aor_control)) < 0)
		dsim_err("fail to send aor_control command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_GAMMA_UPDATE,
				ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("fail to send SEQ_GAMMA_UPDATE command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)gamma_update,
				ARRAY_SIZE(gamma_update)) < 0)
		dsim_err("fail to send gamma_update command.\n");
	#endif

	return 0;
}

static int s6e3fa7_set_brightness(struct backlight_device *bd)
{
	struct dsim_device *dsim;
	struct v4l2_subdev *sd;
	int brightness = bd->props.brightness;

	sd = dev_get_drvdata(bd->dev.parent);
	dsim = container_of(sd, struct dsim_device, sd);

	if (brightness == 146 || brightness == 226)
		return 1;
	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(dsim, brightness);

	return 1;
}

static const struct backlight_ops s6e3fa7_backlight_ops = {
	.get_brightness = s6e3fa7_get_brightness,
	.update_status = s6e3fa7_set_brightness,
};

static int s6e3fa7_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd = panel_priv.par;
	struct panel_private *priv = &panel_priv;

	priv->lcd_connected = lcd->connected = 1;

	lcd->dsim = dsim;
	lcd->state = PANEL_STATE_SUSPENED;

	dsim->bd = backlight_device_register("panel", dsim->dev,
		NULL, &s6e3fa7_backlight_ops, NULL);
	if (IS_ERR(dsim->bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	dsim->bd->props.max_brightness = MAX_BRIGHTNESS;
	dsim->bd->props.brightness = DEFAULT_BRIGHTNESS;

	dsim_info("lcd panel: %s: done\n", __func__);

	return ret;
}

static int s6e3fa7_init(struct lcd_info *lcd)
{
	int ret = 0;
	struct dsim_device *dsim = lcd->dsim;

	dsim_info("lcd panel: %s\n", __func__);

	lcd_init(dsim->id, &dsim->lcd_info);

	return ret;
}

static int s6e3fa7_displayon(struct lcd_info *lcd)
{
	int ret = 0;
	struct dsim_device *dsim = lcd->dsim;

	dsim_info("lcd panel: %s\n", __func__);

	lcd_enable(dsim->id);

	return ret;
}

static int s6e3fa7_suspend(struct lcd_info *lcd)
{
	return 0;
}

static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd_info *lcd = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, fb_notifier);

	fb_blank = *(int *)evdata->data;

	dsim_info("lcd panel: %s: %02lx, %d\n", __func__, event, fb_blank);

	if (evdata->info->node != 0)
		return NOTIFY_DONE;

	if (event == FB_EVENT_BLANK && fb_blank == FB_BLANK_UNBLANK)
		s6e3fa7_displayon(lcd);

	return NOTIFY_DONE;
}

static int s6e3fa7_register_notifier(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->fb_notifier.notifier_call = fb_notifier_callback;
	decon_register_notifier(&lcd->fb_notifier);

	return ret;
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;

	panel_priv.par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s: failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto probe_err;
	}

	mutex_init(&lcd->lock);

	ret = s6e3fa7_probe(dsim);
	if (ret < 0) {
		pr_err("%s: failed to probe panel\n", __func__);
		goto probe_err;
	}

	s6e3fa7_register_notifier(lcd);

	dsim_info("lcd panel: %s: %s: done\n", kbasename(__FILE__), __func__);
probe_err:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	struct lcd_info *lcd = panel_priv.par;
	int ret = 0;

	dsim_info("lcd panel: +%s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ret = s6e3fa7_init(lcd);
		if (ret) {
			dsim_info("%s: failed to panel init, %d\n", __func__, ret);
			goto displayon_err;
		}
	}

displayon_err:
	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_RESUMED;
	mutex_unlock(&lcd->lock);

	dsim_info("lcd panel: -%s: %d\n", __func__, lcd->connected);

	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	struct lcd_info *lcd = panel_priv.par;
	int ret = 0;

	dsim_info("lcd panel: +%s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENDING;
	mutex_unlock(&lcd->lock);

	ret = s6e3fa7_suspend(lcd);
	if (ret) {
		dsim_info("lcd panel: %s: failed to panel exit, %d\n", __func__, ret);
		goto suspend_err;
	}

suspend_err:
	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENED;
	mutex_unlock(&lcd->lock);

	dsim_info("lcd panel: -%s: %d\n", __func__, lcd->state);

	return ret;
}

static int dsim_panel_dump(struct dsim_device *dsim)
{
	return 0;
}

struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver = {
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_dump,
};

static struct notifier_block decon_fb_notifier = {
	.notifier_call = decon_notifier_event,
};

static void __exit decon_notifier_exit(void)
{
	fb_unregister_client(&decon_fb_notifier);
}

static int __init decon_notifier_init(void)
{
	fb_register_client(&decon_fb_notifier);

	return 0;
}

late_initcall(decon_notifier_init);
module_exit(decon_notifier_exit);

