/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include "../decon_notify.h"

#include "mdnie.h"
#include "dd.h"

#ifdef CONFIG_DISPLAY_USE_INFO
#include "dpui.h"
#endif

#define IS_DMB(idx)					(idx == DMB_NORMAL_MODE)
#define IS_SCENARIO(idx)			(idx < SCENARIO_MAX && !(idx > VIDEO_NORMAL_MODE && idx < CAMERA_MODE))
#define IS_ACCESSIBILITY(idx)		(idx && idx < ACCESSIBILITY_MAX)
#define IS_COLOR_LENS(idx)			(idx && idx < COLOR_LENS_MAX)
#define IS_HBM(idx)					(idx && idx < HBM_MAX)
#define IS_HMT(idx)					(idx && idx < HMT_MDNIE_MAX)
#define IS_NIGHT_MODE(idx)			(idx && idx < NIGHT_MODE_MAX)
#define IS_LIGHT_NOTIFICATION(idx)	(idx && idx < LIGHT_NOTIFICATION_MAX)

#define SCENARIO_IS_VALID(idx)	(IS_DMB(idx) || IS_SCENARIO(idx))
#define WRGB_IS_VALID(_x)		((_x <= 0) && (_x >= -30))

/* Split 16 bit as 8bit x 2 */
#define GET_MSB_8BIT(x)		((x >> 8) & (BIT(8) - 1))
#define GET_LSB_8BIT(x)		((x >> 0) & (BIT(8) - 1))

static struct class *mdnie_class;

/* Do not call mdnie write directly */
static int mdnie_write(struct mdnie_info *mdnie, struct mdnie_table *table, unsigned int num)
{
	int ret = 0;

	if (mdnie->enable)
		ret = mdnie->ops.write(mdnie->data, table->seq, num);

	return ret;
}

static int mdnie_write_table(struct mdnie_info *mdnie, struct mdnie_table *table)
{
	int i, ret = 0;
	struct mdnie_table *buf = NULL;

	for (i = 0; table->seq[i].len; i++) {
		if (IS_ERR_OR_NULL(table->seq[i].cmd)) {
			dev_info(mdnie->dev, "mdnie sequence %s %dth is null\n", table->name, i);
			return -EPERM;
		}
	}

	mutex_lock(&mdnie->dev_lock);

	buf = table;

	ret = mdnie_write(mdnie, buf, i);

	mutex_unlock(&mdnie->dev_lock);

	return ret;
}

static struct mdnie_table *mdnie_find_table(struct mdnie_info *mdnie)
{
	struct mdnie_table *table = NULL;
	struct mdnie_trans_info *trans_info = mdnie->tune->trans_info;

	mutex_lock(&mdnie->lock);

	if (IS_LIGHT_NOTIFICATION(mdnie->light_notification)) {
		table = mdnie->tune->light_notification_table ? &mdnie->tune->light_notification_table[mdnie->light_notification] : NULL;
		goto exit;
	} else if (IS_ACCESSIBILITY(mdnie->accessibility)) {
		table = mdnie->tune->accessibility_table ? &mdnie->tune->accessibility_table[mdnie->accessibility] : NULL;
		goto exit;
	} else if (IS_COLOR_LENS(mdnie->color_lens)) {
		table = mdnie->tune->lens_table ? &mdnie->tune->lens_table[mdnie->color_lens] : NULL;
		goto exit;
	} else if (IS_HMT(mdnie->hmt_mode)) {
		table = mdnie->tune->hmt_table ? &mdnie->tune->hmt_table[mdnie->hmt_mode] : NULL;
		goto exit;
	} else if (IS_NIGHT_MODE(mdnie->night_mode)) {
		table = mdnie->tune->night_table ? &mdnie->tune->night_table[mdnie->night_mode] : NULL;
		goto exit;
	} else if (IS_HBM(mdnie->hbm)) {
		table = mdnie->tune->hbm_table ? &mdnie->tune->hbm_table[mdnie->hbm] : NULL;
		goto exit;
	} else if (IS_DMB(mdnie->scenario)) {
		table = mdnie->tune->dmb_table ? &mdnie->tune->dmb_table[mdnie->mode] : NULL;
		goto exit;
	} else if (IS_SCENARIO(mdnie->scenario)) {
		table = mdnie->tune->main_table ? &mdnie->tune->main_table[mdnie->scenario][mdnie->mode] : NULL;
		goto exit;
	}

exit:
	if (trans_info->enable && mdnie->disable_trans_dimming && (table != NULL)) {
		dev_info(mdnie->dev, "%s: disable_trans_dimming=%d\n", __func__, mdnie->disable_trans_dimming);
		memcpy(&(mdnie->table_buffer), table, sizeof(struct mdnie_table));
		memcpy(mdnie->sequence_buffer, table->seq[trans_info->index].cmd, table->seq[trans_info->index].len);
		mdnie->table_buffer.seq[trans_info->index].cmd = mdnie->sequence_buffer;
		mdnie->table_buffer.seq[trans_info->index].cmd[trans_info->offset] = 0x0;
		mutex_unlock(&mdnie->lock);
		return &(mdnie->table_buffer);
	}

	mutex_unlock(&mdnie->lock);

	return table;
}

static void mdnie_update_sequence(struct mdnie_info *mdnie, struct mdnie_table *table)
{
	mdnie_renew_table(mdnie, table);
	mdnie_write_table(mdnie, table);
}

void mdnie_update(struct mdnie_info *mdnie)
{
	struct mdnie_table *table = NULL;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	if (!mdnie->enable) {
		dev_err(mdnie->dev, "mdnie state is off\n");
		return;
	}

	table = mdnie_find_table(mdnie);
	if (!IS_ERR_OR_NULL(table) && !IS_ERR_OR_NULL(table->name)) {
		mdnie_update_sequence(mdnie, table);
		dev_info(mdnie->dev, "%s\n", table->name);

		mdnie->wrgb_current.r = table->seq[scr_info->index].cmd[scr_info->wr];
		mdnie->wrgb_current.g = table->seq[scr_info->index].cmd[scr_info->wg];
		mdnie->wrgb_current.b = table->seq[scr_info->index].cmd[scr_info->wb];
	}
}

static void update_color_position(struct mdnie_info *mdnie, unsigned int idx)
{
	u8 mode, scenario;
	mdnie_t *wbuf;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	dev_info(mdnie->dev, "%s: %d\n", __func__, idx);

	mutex_lock(&mdnie->lock);

	for (mode = 0; mode < MODE_MAX; mode++) {
		for (scenario = 0; scenario <= EMAIL_MODE; scenario++) {
			wbuf = mdnie->tune->main_table[scenario][mode].seq[scr_info->index].cmd;
			if (IS_ERR_OR_NULL(wbuf))
				continue;
			if (scenario != EBOOK_MODE && mode != EBOOK) {
				wbuf[scr_info->wr] = mdnie->tune->coordinate_table[mode][idx * 3 + 0];
				wbuf[scr_info->wg] = mdnie->tune->coordinate_table[mode][idx * 3 + 1];
				wbuf[scr_info->wb] = mdnie->tune->coordinate_table[mode][idx * 3 + 2];
#ifdef CONFIG_SEC_FACTORY
				if (mode == AUTO) {
					wbuf[scr_info->wr] = mdnie->tune->coordinate_table[mode][idx * 3 + 0] + mdnie->wrgb_balance.r;
					wbuf[scr_info->wg] = mdnie->tune->coordinate_table[mode][idx * 3 + 1] + mdnie->wrgb_balance.g;
					wbuf[scr_info->wb] = mdnie->tune->coordinate_table[mode][idx * 3 + 2] + mdnie->wrgb_balance.b;
				}
#endif
			}
			if (mode == AUTO && scenario == UI_MODE) {
				mdnie->wrgb_default.r = mdnie->tune->coordinate_table[mode][idx * 3 + 0];
				mdnie->wrgb_default.g = mdnie->tune->coordinate_table[mode][idx * 3 + 1];
				mdnie->wrgb_default.b = mdnie->tune->coordinate_table[mode][idx * 3 + 2];
				dev_info(mdnie->dev, "%s: %d, %d, %d\n",
				__func__, mdnie->wrgb_default.r, mdnie->wrgb_default.g, mdnie->wrgb_default.b);
			}
		}
	}

	mutex_unlock(&mdnie->lock);
}

static int mdnie_calibration(int *r)
{
	int ret = 0;

	if (r[1] > 0) {
		if (r[3] > 0)
			ret = 3;
		else
			ret = (r[4] < 0) ? 1 : 2;
	} else {
		if (r[2] < 0) {
			if (r[3] > 0)
				ret = 9;
			else
				ret = (r[4] < 0) ? 7 : 8;
		} else {
			if (r[3] > 0)
				ret = 6;
			else
				ret = (r[4] < 0) ? 4 : 5;
		}
	}

	pr_info("%d, %d, %d, %d, tune%d\n", r[1], r[2], r[3], r[4], ret);

	return ret;
}

static int get_panel_coordinate(struct mdnie_info *mdnie, int *result)
{
	int ret = 0;
	unsigned short x, y;

	x = mdnie->coordinate[0];
	y = mdnie->coordinate[1];

	if (!(x || y)) {
		dev_info(mdnie->dev, "%s: %d, %d\n", __func__, x, y);
		ret = -EINVAL;
		goto skip_color_correction;
	}

	result[COLOR_OFFSET_FUNC_F1] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F1](x, y);
	result[COLOR_OFFSET_FUNC_F2] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F2](x, y);
	result[COLOR_OFFSET_FUNC_F3] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F3](x, y);
	result[COLOR_OFFSET_FUNC_F4] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F4](x, y);

	ret = mdnie_calibration(result);
	dev_info(mdnie->dev, "%s: %d, %d, %d\n", __func__, x, y, ret);

skip_color_correction:
	mdnie->color_correction = 1;

	return ret;
}

static ssize_t mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->mode);
}

static ssize_t mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret, idx, result[COLOR_OFFSET_FUNC_MAX] = {0,};

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, value);

	if (value >= MODE_MAX)
		return -EINVAL;

	mutex_lock(&mdnie->lock);
	mdnie->mode = value;
	mutex_unlock(&mdnie->lock);

	if (!mdnie->color_correction) {
		idx = get_panel_coordinate(mdnie, result);
		if (idx > 0)
			update_color_position(mdnie, idx);
	}

	mdnie_update(mdnie);

	return count;
}


static ssize_t scenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->scenario);
}

static ssize_t scenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, value);

	if (!SCENARIO_IS_VALID(value))
		value = UI_MODE;

	mutex_lock(&mdnie->lock);
	mdnie->scenario = value;
	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

static ssize_t accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->accessibility);
}

static ssize_t accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0, s[12] = {0, }, i = 0;
	int ret;
	mdnie_t *wbuf;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = sscanf(buf, "%8d %8x %8x %8x %8x %8x %8x %8x %8x %8x %8x %8x %8x",
		&value, &s[0], &s[1], &s[2], &s[3],
		&s[4], &s[5], &s[6], &s[7], &s[8], &s[9], &s[10], &s[11]);

	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d, %d\n", __func__, value, ret);

	if (value >= ACCESSIBILITY_MAX)
		return -EINVAL;

	mutex_lock(&mdnie->lock);
	mdnie->accessibility = value;
	if (value == COLOR_BLIND) {
		if (ret > ARRAY_SIZE(s) + 1) {
			mutex_unlock(&mdnie->lock);
			return -EINVAL;
		}
		wbuf = &mdnie->tune->accessibility_table[value].seq[scr_info->index].cmd[scr_info->cr];
		while (i < ret - 1) {
			wbuf[i * 2 + 0] = GET_LSB_8BIT(s[i]);
			wbuf[i * 2 + 1] = GET_MSB_8BIT(s[i]);
			i++;
		}

		dev_info(dev, "%s: %s\n", __func__, buf);
	}
	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

static ssize_t color_correct_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *pos = buf;
	int i, idx, result[COLOR_OFFSET_FUNC_MAX] = {0,};

	if (!mdnie->color_correction)
		return -EINVAL;

	idx = get_panel_coordinate(mdnie, result);

	for (i = COLOR_OFFSET_FUNC_F1; i < COLOR_OFFSET_FUNC_MAX; i++)
		pos += sprintf(pos, "f%d: %d, ", i, result[i]);
	pos += sprintf(pos, "tune%d\n", idx);

	return pos - buf;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	sprintf(buf, "%d, %d\n", mdnie->coordinate[0], mdnie->coordinate[1]);

	return strlen(buf);
}

static ssize_t color_coordinate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int ret, idx, result[COLOR_OFFSET_FUNC_MAX] = {0,};

	ret = sscanf(buf, "%8d %8d", &mdnie->coordinate[0], &mdnie->coordinate[1]);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d, %d\n", __func__, mdnie->coordinate[0], mdnie->coordinate[1]);

	idx = get_panel_coordinate(mdnie, result);
	if (idx > 0)
		update_color_position(mdnie, idx);

	mdnie_update(mdnie);

	return count;
}

static ssize_t bypass_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->bypass);
}

static ssize_t bypass_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	struct mdnie_table *table = NULL;
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, value);

	if (value >= BYPASS_MAX)
		return -EINVAL;

	value = (value) ? BYPASS_ON : BYPASS_OFF;

	mutex_lock(&mdnie->lock);
	mdnie->bypass = value;
	mutex_unlock(&mdnie->lock);

	table = &mdnie->tune->bypass_table[value];
	if (!IS_ERR_OR_NULL(table)) {
		mdnie_write_table(mdnie, table);
		dev_info(mdnie->dev, "%s\n", table->name);
	}

	return count;
}

static ssize_t lux_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->hbm);
}

static ssize_t lux_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int hbm = 0, update = 0;
	int ret, value = 0;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (!mdnie->tune->get_hbm_index)
		return count;

	mutex_lock(&mdnie->lock);
	hbm = mdnie->tune->get_hbm_index(value);
	update = (mdnie->hbm != hbm) ? 1 : 0;
	mdnie->hbm = update ? hbm : mdnie->hbm;
	mutex_unlock(&mdnie->lock);

	if (update) {
		dev_info(dev, "%s: %d\n", __func__, value);
		mdnie_update(mdnie);
	}

	return count;
}

static ssize_t sensorRGB_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d %d\n", mdnie->wrgb_current.r, mdnie->wrgb_current.g, mdnie->wrgb_current.b);
}

static ssize_t sensorRGB_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	struct mdnie_table *table = NULL;
	unsigned int white_r = 0, white_g = 0, white_b = 0;
	int ret;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = sscanf(buf, "%8d %8d %8d", &white_r, &white_g, &white_b);
	if (ret < 0)
		return ret;

	if (mdnie->enable
		&& mdnie->accessibility == ACCESSIBILITY_OFF
		&& !mdnie->ldu
		&& mdnie->mode == AUTO
		&& (mdnie->scenario == BROWSER_MODE || mdnie->scenario == EBOOK_MODE)) {
		dev_info(dev, "%s: %d, %d, %d\n", __func__, white_r, white_g, white_b);

		table = mdnie_find_table(mdnie);

		memcpy(&mdnie->table_buffer, table, sizeof(struct mdnie_table));
		memcpy(&mdnie->sequence_buffer, table->seq[scr_info->index].cmd, table->seq[scr_info->index].len);
		mdnie->table_buffer.seq[scr_info->index].cmd = mdnie->sequence_buffer;

		mdnie->table_buffer.seq[scr_info->index].cmd[scr_info->wr] = mdnie->wrgb_current.r = (unsigned char)white_r;
		mdnie->table_buffer.seq[scr_info->index].cmd[scr_info->wg] = mdnie->wrgb_current.g = (unsigned char)white_g;
		mdnie->table_buffer.seq[scr_info->index].cmd[scr_info->wb] = mdnie->wrgb_current.b = (unsigned char)white_b;

		mdnie_update_sequence(mdnie, &mdnie->table_buffer);
	}

	return count;
}

static ssize_t whiteRGB_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d %d\n", mdnie->wrgb_balance.r, mdnie->wrgb_balance.g, mdnie->wrgb_balance.b);
}

static ssize_t whiteRGB_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	mdnie_t *wbuf;
	u8 scenario;
	int white_r = 0, white_g = 0, white_b = 0;
	int ret;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = sscanf(buf, "%8d %8d %8d", &white_r, &white_g, &white_b);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d, %d, %d\n", __func__, white_r, white_g, white_b);

	if (!WRGB_IS_VALID(white_r) || !WRGB_IS_VALID(white_g) || !WRGB_IS_VALID(white_b))
		return count;

	if (mdnie->mode != AUTO)
		return count;

	mutex_lock(&mdnie->lock);
	if (!mdnie->ldu) {
		mdnie->wrgb_ldu.r = mdnie->wrgb_default.r;
		mdnie->wrgb_ldu.g = mdnie->wrgb_default.g;
		mdnie->wrgb_ldu.b = mdnie->wrgb_default.b;
	}

	for (scenario = 0; scenario < SCENARIO_MAX; scenario++) {
		wbuf = mdnie->tune->main_table[scenario][mdnie->mode].seq[scr_info->index].cmd;
		if (IS_ERR_OR_NULL(wbuf))
			continue;
		if (scenario != EBOOK_MODE) {
			wbuf[scr_info->wr] = (unsigned char)(mdnie->wrgb_ldu.r + white_r);
			wbuf[scr_info->wg] = (unsigned char)(mdnie->wrgb_ldu.g + white_g);
			wbuf[scr_info->wb] = (unsigned char)(mdnie->wrgb_ldu.b + white_b);
			mdnie->wrgb_balance.r = white_r;
			mdnie->wrgb_balance.g = white_g;
			mdnie->wrgb_balance.b = white_b;
		}
	}

	if (!IS_ERR_OR_NULL(mdnie->tune->dmb_table)) {
		wbuf = mdnie->tune->dmb_table[mdnie->mode].seq[scr_info->index].cmd;
		if (!IS_ERR_OR_NULL(wbuf)) {
			wbuf[scr_info->wr] = (unsigned char)(mdnie->wrgb_ldu.r + white_r);
			wbuf[scr_info->wg] = (unsigned char)(mdnie->wrgb_ldu.g + white_g);
			wbuf[scr_info->wb] = (unsigned char)(mdnie->wrgb_ldu.b + white_b);
			mdnie->wrgb_balance.r = white_r;
			mdnie->wrgb_balance.g = white_g;
			mdnie->wrgb_balance.b = white_b;
		}
	}
	mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t night_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d\n", mdnie->night_mode, mdnie->night_mode_level);
}

static ssize_t night_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int enable = 0, level = 0, base_index;
	int i;
	int ret;
	mdnie_t *wbuf;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = sscanf(buf, "%8d %8d", &enable, &level);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d, %d\n", __func__, enable, level);

	if (IS_ERR_OR_NULL(mdnie->tune->night_table) || IS_ERR_OR_NULL(mdnie->tune->night_info))
		return count;

	if (!mdnie->tune->night_info->max_w || !mdnie->tune->night_info->max_h)
		return count;

	if (enable >= NIGHT_MODE_MAX)
		return -EINVAL;

	if (level >= mdnie->tune->night_info->max_h)
		return -EINVAL;

	mutex_lock(&mdnie->lock);

	if (enable) {
		wbuf = &mdnie->tune->night_table[enable].seq[scr_info->index].cmd[scr_info->cr];
		base_index = mdnie->tune->night_info->max_w * level;
		for (i = 0; i < mdnie->tune->night_info->max_w; i++)
			wbuf[i] = mdnie->tune->night_mode_table[base_index + i];
	}

	mdnie->night_mode = enable;
	mdnie->night_mode_level = level;

	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

static ssize_t mdnie_ldu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d %d\n", mdnie->wrgb_current.r, mdnie->wrgb_current.g, mdnie->wrgb_current.b);
}

static ssize_t mdnie_ldu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	mdnie_t *wbuf;
	u8 mode, scenario;
	unsigned int idx = 0;
	int ret;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = kstrtouint(buf, 0, &idx);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, idx);

	if (idx >= MODE_MAX)
		return -EINVAL;

	if (IS_ERR_OR_NULL(mdnie->tune->adjust_ldu_table))
		return count;

	mutex_lock(&mdnie->lock);
	mdnie->ldu = idx;
	for (mode = 0; mode < MODE_MAX; mode++) {
		for (scenario = 0; scenario <= EMAIL_MODE; scenario++) {
			wbuf = mdnie->tune->main_table[scenario][mode].seq[scr_info->index].cmd;
			if (IS_ERR_OR_NULL(wbuf))
				continue;
			if (scenario != EBOOK_MODE && mode != EBOOK) {
				if (mode == AUTO) {
					wbuf[scr_info->wr] = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 0] + mdnie->wrgb_balance.r;
					wbuf[scr_info->wg] = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 1] + mdnie->wrgb_balance.g;
					wbuf[scr_info->wb] = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 2] + mdnie->wrgb_balance.b;
					mdnie->wrgb_ldu.r = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 0];
					mdnie->wrgb_ldu.g = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 1];
					mdnie->wrgb_ldu.b = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 2];
				} else {
					wbuf[scr_info->wr] = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 0];
					wbuf[scr_info->wg] = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 1];
					wbuf[scr_info->wb] = mdnie->tune->adjust_ldu_table[mode][idx * 3 + 2];
				}
			}
		}
	}
	mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t light_notification_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->light_notification);
}

static ssize_t light_notification_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, value);

	if (value >= LIGHT_NOTIFICATION_MAX)
		return -EINVAL;

	value = (value) ? LIGHT_NOTIFICATION_ON : LIGHT_NOTIFICATION_OFF;

	mutex_lock(&mdnie->lock);
	mdnie->light_notification = value;
	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

static ssize_t color_lens_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d %d\n", mdnie->color_lens, mdnie->color_lens_color, mdnie->color_lens_level);
}

static ssize_t color_lens_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int enable = 0, color = 0, level = 0, base_index;
	int i;
	int ret;
	mdnie_t *wbuf;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = sscanf(buf, "%8d %8d %8d", &enable, &color, &level);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d, %d, %d\n", __func__, enable, color, level);

	if (IS_ERR_OR_NULL(mdnie->tune->color_lens_table) || IS_ERR_OR_NULL(mdnie->tune->color_lens_info))
		return count;

	if (!mdnie->tune->color_lens_info->max_color || !mdnie->tune->color_lens_info->max_level || !mdnie->tune->color_lens_info->max_w)
		return count;

	if (enable >= COLOR_LENS_MAX)
		return -EINVAL;

	if ((color >= mdnie->tune->color_lens_info->max_color) || (level >= mdnie->tune->color_lens_info->max_level))
		return -EINVAL;

	mutex_lock(&mdnie->lock);

	if (enable) {
		wbuf = &mdnie->tune->lens_table[enable].seq[scr_info->index].cmd[scr_info->cr];
		base_index = (mdnie->tune->color_lens_info->max_level * mdnie->tune->color_lens_info->max_w * color) + (mdnie->tune->color_lens_info->max_w * level);
		for (i = 0; i < mdnie->tune->color_lens_info->max_w; i++)
			wbuf[i] = mdnie->tune->color_lens_table[base_index + i];
	}

	mdnie->color_lens = enable;
	mdnie->color_lens_color = color;
	mdnie->color_lens_level = level;

	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

#ifdef CONFIG_LCD_HMT
static ssize_t hmtColorTemp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->hmt_mode);
}

static ssize_t hmtColorTemp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value != mdnie->hmt_mode && value < HMT_MDNIE_MAX) {
		mutex_lock(&mdnie->lock);
		mdnie->hmt_mode = value;
		mutex_unlock(&mdnie->lock);
		mdnie_update(mdnie);
	}

	return count;
}
#endif

static DEVICE_ATTR(mode, 0664, mode_show, mode_store);
static DEVICE_ATTR(scenario, 0664, scenario_show, scenario_store);
static DEVICE_ATTR(accessibility, 0664, accessibility_show, accessibility_store);
static DEVICE_ATTR(color_correct, 0444, color_correct_show, NULL);
static DEVICE_ATTR(color_coordinate, 0000, color_coordinate_show, color_coordinate_store);
static DEVICE_ATTR(bypass, 0664, bypass_show, bypass_store);
static DEVICE_ATTR(lux, 0664, lux_show, lux_store);
static DEVICE_ATTR(sensorRGB, 0664, sensorRGB_show, sensorRGB_store);
static DEVICE_ATTR(whiteRGB, 0664, whiteRGB_show, whiteRGB_store);
static DEVICE_ATTR(night_mode, 0664, night_mode_show, night_mode_store);
static DEVICE_ATTR(mdnie_ldu, 0664, mdnie_ldu_show, mdnie_ldu_store);
static DEVICE_ATTR(light_notification, 0664, light_notification_show, light_notification_store);
static DEVICE_ATTR(color_lens, 0664, color_lens_show, color_lens_store);
#ifdef CONFIG_LCD_HMT
static DEVICE_ATTR(hmt_color_temperature, 0664, hmtColorTemp_show, hmtColorTemp_store);
#endif


static struct attribute *mdnie_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_scenario.attr,
	&dev_attr_color_correct.attr,
	&dev_attr_color_coordinate.attr,
	&dev_attr_bypass.attr,
	&dev_attr_lux.attr,
	&dev_attr_light_notification.attr,
#ifdef CONFIG_LCD_HMT
	&dev_attr_hmt_color_temperature.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(mdnie);

static const struct attribute *mdnie_scr_attrs[] = {
	&dev_attr_accessibility.attr,
	&dev_attr_sensorRGB.attr,
	&dev_attr_whiteRGB.attr,
	&dev_attr_night_mode.attr,
	&dev_attr_mdnie_ldu.attr,
	&dev_attr_color_lens.attr,
	NULL,
};

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct mdnie_info *mdnie;
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case DECON_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	mdnie = container_of(self, struct mdnie_info, fb_notif);

	fb_blank = *(int *)evdata->data;

	dev_info(mdnie->dev, "%s: event: %lu, blank: %d\n", __func__, event, fb_blank);

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (event == DECON_EVENT_DOZE) {
		mutex_lock(&mdnie->lock);
		mdnie->lpm = 1;
		mutex_unlock(&mdnie->lock);
	} else if (event == FB_EVENT_BLANK) {
		mutex_lock(&mdnie->lock);
		mdnie->lpm = 0;
		mutex_unlock(&mdnie->lock);
	}

	if (fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&mdnie->lock);
		mdnie->light_notification = LIGHT_NOTIFICATION_OFF;
		mdnie->enable = 1;
		mutex_unlock(&mdnie->lock);

		mdnie_update(mdnie);
		if (mdnie->tune->trans_info->enable)
			mdnie->disable_trans_dimming = 0;
	} else if (fb_blank == FB_BLANK_POWERDOWN) {
		mutex_lock(&mdnie->lock);
		mdnie->enable = 0;
		if (mdnie->tune->trans_info->enable)
			mdnie->disable_trans_dimming = 1;
		mutex_unlock(&mdnie->lock);
	}

	return NOTIFY_DONE;
}

static int mdnie_register_fb(struct mdnie_info *mdnie)
{
	memset(&mdnie->fb_notif, 0, sizeof(mdnie->fb_notif));
	mdnie->fb_notif.notifier_call = fb_notifier_callback;
	return decon_register_notifier(&mdnie->fb_notif);
}

#ifdef CONFIG_DISPLAY_USE_INFO
static int dpui_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct mdnie_info *mdnie;
	char tbuf[MAX_DPUI_VAL_LEN];
	int size;

	mdnie = container_of(self, struct mdnie_info, dpui_notif);

	mutex_lock(&mdnie->lock);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", mdnie->coordinate[0]);
	set_dpui_field(DPUI_KEY_WCRD_X, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", mdnie->coordinate[1]);
	set_dpui_field(DPUI_KEY_WCRD_Y, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", mdnie->wrgb_balance.r);
	set_dpui_field(DPUI_KEY_WOFS_R, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", mdnie->wrgb_balance.g);
	set_dpui_field(DPUI_KEY_WOFS_G, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", mdnie->wrgb_balance.b);
	set_dpui_field(DPUI_KEY_WOFS_B, tbuf, size);

	mutex_unlock(&mdnie->lock);

	return NOTIFY_DONE;
}

static int mdnie_register_dpui(struct mdnie_info *mdnie)
{
	memset(&mdnie->dpui_notif, 0, sizeof(mdnie->dpui_notif));
	mdnie->dpui_notif.notifier_call = dpui_notifier_callback;
	return dpui_logging_register(&mdnie->dpui_notif, DPUI_TYPE_PANEL);
}
#endif /* CONFIG_DISPLAY_USE_INFO */

static struct mdnie_scr_info default_scr_info;
static struct mdnie_night_info default_night_info;
static struct mdnie_trans_info default_trans_info;
static int mdnie_check_info(struct mdnie_info *mdnie)
{
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;
	struct mdnie_night_info *night_info = mdnie->tune->night_info;
	struct mdnie_trans_info *trans_info = mdnie->tune->trans_info;
	struct mdnie_table *table = NULL;
	unsigned int index = 0, limit = 0;
	int ret = 0;

	table = mdnie->tune->main_table ? &mdnie->tune->main_table[mdnie->scenario][mdnie->mode] : NULL;

	if (!table) {
		pr_err("%s: failed to get initial mdnie table\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (scr_info && (scr_info->cr || scr_info->wr || scr_info->wg || scr_info->wb)) {
		index = scr_info->index;
		limit = max(scr_info->cr, max3(scr_info->wr, scr_info->wg, scr_info->wb));

		if (index >= MDNIE_IDX_MAX) {
			pr_err("%s: invalid scr_info index. %d\n", __func__, index);
			ret = -EINVAL;
			goto exit;
		}

		if (limit >= table->seq[index].len) {
			pr_err("%s: invalid scr_info limit. %d, %d\n", __func__, limit, table->seq[index].len);
			ret = -EINVAL;
			goto exit;
		}
	}

	if (scr_info && night_info && night_info->max_w) {
		index = scr_info->index;
		limit = scr_info->cr + night_info->max_w - 1;

		if (index >= MDNIE_IDX_MAX) {
			pr_err("%s: invalid night_info index. %d\n", __func__, index);
			ret = -EINVAL;
			goto exit;
		}

		if (limit >= table->seq[index].len) {
			pr_err("%s: invalid night_info offset. %d, %d\n", __func__, limit, table->seq[index].len);
			ret = -EINVAL;
			goto exit;
		}
	}

	if (trans_info && trans_info->enable) {
		index = trans_info->index;
		limit = trans_info->offset;

		if (index >= MDNIE_IDX_MAX) {
			pr_err("%s: invalid trans_info index. %d\n", __func__, index);
			ret = -EINVAL;
			goto exit;
		}

		if (limit >= table->seq[index].len) {
			pr_err("%s: invalid trans_info offset. %d, %d\n", __func__, limit, table->seq[index].len);
			ret = -EINVAL;
			goto exit;
		}
	}

exit:
	if (ret < 0)
		pr_info("%s: skip to use mdnie\n", __func__);
	else {
		if (!scr_info) {
			pr_info("%s: mdnie tune scr info as default\n", __func__);
			mdnie->tune->scr_info = &default_scr_info;
		}

		if (!night_info) {
			pr_info("%s: mdnie tune night info as default\n", __func__);
			mdnie->tune->night_info = &default_night_info;
		}

		if (!trans_info) {
			pr_info("%s: mdnie tune trans info as default\n", __func__);
			mdnie->tune->trans_info = &default_trans_info;
		}
	}

	return ret;
}

int mdnie_register(struct device *p, void *data, mdnie_w w, mdnie_r r,
		unsigned int *coordinate, struct mdnie_tune *tune)
{
	int ret = 0;
	struct mdnie_info *mdnie;
	static unsigned int mdnie_no;

	if (!tune) {
		pr_err("failed to get mdnie tune\n");
		goto exit0;
	}

	mdnie = kzalloc(sizeof(struct mdnie_info), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie\n");
		ret = -ENOMEM;
		goto exit0;
	}

	mdnie->scenario = UI_MODE;
	mdnie->mode = AUTO;
	mdnie->accessibility = ACCESSIBILITY_OFF;
	mdnie->bypass = BYPASS_OFF;
	mdnie->night_mode = NIGHT_MODE_OFF;
	mdnie->light_notification = LIGHT_NOTIFICATION_OFF;
	mdnie->color_lens = COLOR_LENS_OFF;

	mdnie->wrgb_default.r = mdnie->wrgb_ldu.r = 255;
	mdnie->wrgb_default.g = mdnie->wrgb_ldu.r = 255;
	mdnie->wrgb_default.b = mdnie->wrgb_ldu.r = 255;

	mdnie->data = data;
	mdnie->ops.write = w;
	mdnie->ops.read = r;

	mdnie->coordinate[0] = coordinate ? coordinate[0] : 0;
	mdnie->coordinate[1] = coordinate ? coordinate[1] : 0;
	mdnie->tune = tune;

	ret = mdnie_check_info(mdnie);
	if (ret < 0)
		goto exit1;

	if (IS_ERR_OR_NULL(mdnie_class)) {
		mdnie_class = class_create(THIS_MODULE, "mdnie");
		if (IS_ERR_OR_NULL(mdnie_class)) {
			pr_err("failed to create mdnie class\n");
			ret = -EINVAL;
			goto exit1;
		}

		mdnie_class->dev_groups = mdnie_groups;
	}

	mdnie->dev = device_create(mdnie_class, p, 0, &mdnie, !mdnie_no ? "mdnie" : "mdnie%d", mdnie_no);
	if (IS_ERR_OR_NULL(mdnie->dev)) {
		pr_err("failed to create mdnie device\n");
		ret = -EINVAL;
		goto exit2;
	}

	if (tune->scr_info->cr && tune->scr_info->wr && tune->scr_info->wg && tune->scr_info->wb) {
		ret = sysfs_create_files(&mdnie->dev->kobj, mdnie_scr_attrs);
		if (ret < 0) {
			pr_err("failed to create mdnie scr attributes\n");
			goto exit3;
		}
	}

	mutex_init(&mdnie->lock);
	mutex_init(&mdnie->dev_lock);

	dev_set_drvdata(mdnie->dev, mdnie);

	mdnie_register_fb(mdnie);
#ifdef CONFIG_DISPLAY_USE_INFO
	mdnie_register_dpui(mdnie);
#endif
	mdnie->enable = 1;

	init_debugfs_mdnie(mdnie, mdnie_no);

	mdnie_update(mdnie);

	dev_info(mdnie->dev, "registered successfully\n");

	mdnie_no++;

	return 0;

exit3:
	device_unregister(mdnie->dev);
exit2:
	class_destroy(mdnie_class);
exit1:
	kfree(mdnie);
exit0:
	return ret;
}


static int attr_find_and_store(struct device *dev,
	const char *name, const char *buf, size_t size)
{
	struct device_attribute *dev_attr;
	struct kernfs_node *kn;
	struct attribute *attr;

	kn = kernfs_find_and_get(dev->kobj.sd, name);
	if (!kn) {
		dev_info(dev, "%s: not found: %s\n", __func__, name);
		return 0;
	}

	attr = kn->priv;
	dev_attr = container_of(attr, struct device_attribute, attr);

	if (dev_attr && dev_attr->store)
		dev_attr->store(dev, dev_attr, buf, size);

	kernfs_put(kn);

	return 0;
}

ssize_t attr_store_for_each(struct class *cls,
	const char *name, const char *buf, size_t size)
{
	struct class_dev_iter iter;
	struct device *dev;
	int error = 0;
	struct class *class = cls;

	if (!class)
		return -EINVAL;
	if (!class->p) {
		WARN(1, "%s called for class '%s' before it was initialized",
		     __func__, class->name);
		return -EINVAL;
	}

	class_dev_iter_init(&iter, class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		error = attr_find_and_store(dev, name, buf, size);
		if (error)
			break;
	}
	class_dev_iter_exit(&iter);

	return error;
}

struct class *get_mdnie_class(void)
{
	return mdnie_class;
}

