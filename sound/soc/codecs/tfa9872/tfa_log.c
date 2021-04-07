/*
 * tfa_log.c   tfa98xx logging in sysfs
 *
 * Copyright (c) 2015 NXP Semiconductors
 *
 * Author: Michael Kim <michael.kim@nxp.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sysfs.h>

#include "tfa98xx_tfafieldnames.h"
#include "tfa_internal.h"
#include "tfa_service.h"
#include "tfa_container.h"
#include "tfa98xx_genregs_N1C.h"

#define TFA_CLASS_NAME	"nxp"
#define TFA_LOG_DEV_NAME	"tfa_log"
#define TFA_LOG_MAX_COUNT	4
#define DESC_MAXX_LOG	"maxium of X"
#define DESC_MAXT_LOG	"maximum of T"
#define DESC_OVERXMAX_COUNT	"counter of X > Xmax"
#define DESC_OVERTMAX_COUNT	"counter of T > Tmax"
#define TFA_LOG_IN_SEPARATE_NODES
#if defined(TFA_LOG_IN_SEPARATE_NODES)
#define FILESIZE_LOG	10
#else
#define FILESIZE_LOG	(10 * TFA_LOG_MAX_COUNT)
#endif

/* ---------------------------------------------------------------------- */

#if defined(TFA_LOG_IN_SEPARATE_NODES)
static ssize_t tfa_data_maxx_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_data_maxx_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(data_maxx, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_data_maxx_show, tfa_data_maxx_store);

static ssize_t tfa_data_maxt_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_data_maxt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(data_maxt, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_data_maxt_show, tfa_data_maxt_store);

static ssize_t tfa_count_overxmax_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_count_overxmax_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(count_overxmax, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_count_overxmax_show, tfa_count_overxmax_store);

static ssize_t tfa_count_overtmax_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_count_overtmax_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(count_overtmax, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_count_overtmax_show, tfa_count_overtmax_store);
#else
static ssize_t tfa_data_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(data, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_data_show, tfa_data_store);
#endif

static ssize_t tfa_log_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_log_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_log_enable_show, tfa_log_enable_store);

/*
 * to check the data in debug log, in the middle by force
 * without hurting scheme to reset after reading
 */
static ssize_t tfa_log_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_log_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(log, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_log_show, tfa_log_store);

static struct attribute *tfa_log_attr[] = {
#if defined(TFA_LOG_IN_SEPARATE_NODES)
	&dev_attr_data_maxx.attr,
	&dev_attr_data_maxt.attr,
	&dev_attr_count_overxmax.attr,
	&dev_attr_count_overtmax.attr,
#else
	&dev_attr_data.attr,
#endif
	&dev_attr_enable.attr,
	&dev_attr_log.attr,
	NULL,
};

static struct attribute_group tfa_log_attr_grp = {
	.attrs = tfa_log_attr,
};

struct tfa_log {
	char *desc;
	uint16_t prev_value;
	bool is_max;
	bool is_counter;
	bool is_dirty;
};

/* ---------------------------------------------------------------------- */

extern struct class *g_nxp_class;
struct device *g_tfa_log_dev;
static int cur_status;
static struct tfa_log blackbox[TFA_LOG_MAX_COUNT];
static bool blackbox_enabled;

/* ---------------------------------------------------------------------- */

/* temporarily until API is ready for driver */
static enum tfa98xx_error
tfa_read_log(tfa98xx_handle_t handle,
	uint16_t index, uint16_t *value, bool reset);

static enum tfa98xx_error
tfa_read_log(tfa98xx_handle_t handle,
	uint16_t index, uint16_t *value, bool reset)
{
	pr_info("%s [%d]: %s\n", __func__, index, blackbox[index].desc);

	if (blackbox[index].is_dirty) {
		pr_info("%s: it's read before updated\n", __func__);
		*value = 0; /* no meaningful data to be updated */
		return TFA98XX_ERROR_OK;
	}

	*value = blackbox[index].prev_value;
	if (*value == 0xffff) {
		pr_info("%s: invalid data\n", __func__);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (reset) {
		/* reset the last data */
		blackbox[index].prev_value = 0;
		blackbox[index].is_dirty = 1;
	}

	return TFA98XX_ERROR_OK;
}

static int tfa_configure_log(void)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	enum tfa98xx_error err;
	int is_handle_open = 0;
	uint8_t cmd_buf[3];

	cmd_buf[0] = 0;
	cmd_buf[1] = 0;
	/* 0 - disable logger, 1 - enable logger */
	cmd_buf[2] = (blackbox_enabled) ? 1 : 0;

	for (idx = 0; idx < devcount; idx++) {
		pr_info("%s: set blackbox (%d)\n",
			__func__, blackbox_enabled);
		is_handle_open = tfa98xx_handle_is_open(idx);
		if (!is_handle_open) {
			err = tfa98xx_open(idx);
			if (err) {
				pr_info("cannot open handle (%d)\n", err);
				return err;
			}
		}

		err = tfa_dsp_cmd_id_write
			(idx, MODULE_SPEAKERBOOST,
			 SB_PARAM_SET_DATA_LOGGER, 3, cmd_buf);
		if (err)
			pr_err("%s: error in setting blackbox, err = %d\n",
				__func__, err);

		if (!is_handle_open)
			tfa98xx_close(idx);
	}

	return TFA98XX_ERROR_OK;
}

static int tfa_update_log(void)
{
	int i, idx, devcount = tfa98xx_cnt_max_device();
	enum tfa98xx_error err;
	int is_handle_open = 0;
	uint8_t cmd_buf[(TFA_LOG_MAX_COUNT + 1) * TFACONT_MAXDEVS * 3];
	int data[(TFA_LOG_MAX_COUNT + 1) * TFACONT_MAXDEVS];
	int read_size;

	if (!blackbox_enabled) {
		pr_info("%s: blackbox is inactive\n", __func__);
		return TFA98XX_ERROR_OK;
	}

	read_size = (TFA_LOG_MAX_COUNT + 1) * devcount * 3;

	for (idx = 0; idx < devcount; idx++) {
		pr_info("%s: read from blackbox\n", __func__);
		is_handle_open = tfa98xx_handle_is_open(idx);
		if (!is_handle_open) {
			err = tfa98xx_open(idx);
			if (err) {
				pr_info("cannot open handle (%d)\n", err);
				return err;
			}
		}

		err = tfa_dsp_cmd_id_write_read
			(idx, MODULE_SPEAKERBOOST,
			 SB_PARAM_GET_DATA_LOGGER,
			 read_size, cmd_buf);
		if (err) {
			pr_err("%s: failed to read data from blackbox, err = %d\n",
				__func__, err);
			return err;
		}

		tfa98xx_convert_bytes2data(read_size, cmd_buf, data);

		/* maximum x */
		data[1] = (int)(data[1] / (TFA2_FW_X_DATA_SCALE / 1000));
		/* maximum t */
		data[2] = (int)(data[2] / TFA2_FW_T_DATA_SCALE);
		for (i = 0; i < TFA_LOG_MAX_COUNT; i++)
			pr_info("blackbox: data[%d] = %d\n", i, data[i + 1]);

		if (!is_handle_open)
			tfa98xx_close(idx);
	}

	for (i = 0; i < TFA_LOG_MAX_COUNT; i++) {
		blackbox[i].is_dirty = 0;

		if (blackbox[i].is_max)
			blackbox[i].prev_value =
				(data[i + 1] > blackbox[i].prev_value)
				? data[i + 1] : blackbox[i].prev_value;
		if (blackbox[i].is_counter)
			blackbox[i].prev_value += data[i + 1];
	}

	return TFA98XX_ERROR_OK;
}

/* ---------------------------------------------------------------------- */

#if defined(TFA_LOG_IN_SEPARATE_NODES)
static ssize_t tfa_data_maxx_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value = 0xffff;
	int size;
	int ret = 0;
	char read_string[FILESIZE_LOG] = {0};

	pr_info("%s: begin\n", __func__);

	pr_info("%s: read from driver: %s\n",
		__func__, blackbox[0].desc);

	for (idx = 0; idx < devcount; idx++) {
		/* reset the data in driver after reading */
		ret = tfa_read_log(idx, 0, &value, true);
		if (ret) {
			pr_info("%s: failed to read data from driver\n",
				__func__);
			continue;
		}

		if (idx == 0)
			snprintf(read_string,
				FILESIZE_LOG, "%d", value);
		else
			snprintf(read_string,
				FILESIZE_LOG, "%s %d",
				read_string, value);
	}

	pr_info("%s: %s\n", __func__, read_string);

	if (ret)
		size = snprintf(buf, 10 + 1, "data_error");
	else
		size = snprintf(buf,
			strlen(read_string) + 1,
			"%s", read_string);

	if (size <= 0) {
		pr_err("%s: failed to show in sysfs file", __func__);
		return -EINVAL;
	}

	pr_info("%s: end (%d)\n", __func__, ret);

	return size;
}

static ssize_t tfa_data_maxx_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: not allowed to write log data: %s\n",
		__func__, blackbox[0].desc);

	return size;
}

static ssize_t tfa_data_maxt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value = 0xffff;
	int size;
	int ret = 0;
	char read_string[FILESIZE_LOG] = {0};

	pr_info("%s: begin\n", __func__);

	pr_info("%s: read from driver: %s\n",
		__func__, blackbox[1].desc);

	for (idx = 0; idx < devcount; idx++) {
		/* reset the data in driver after reading */
		ret = tfa_read_log(idx, 1, &value, true);
		if (ret) {
			pr_info("%s: failed to read data from driver\n",
				__func__);
			continue;
		}

		if (idx == 0)
			snprintf(read_string,
				FILESIZE_LOG, "%d", value);
		else
			snprintf(read_string,
				FILESIZE_LOG, "%s %d",
				read_string, value);
	}

	pr_info("%s: %s\n", __func__, read_string);

	if (ret)
		size = snprintf(buf, 10 + 1, "data_error");
	else
		size = snprintf(buf,
			strlen(read_string) + 1,
			"%s", read_string);

	if (size <= 0) {
		pr_err("%s: failed to show in sysfs file", __func__);
		return -EINVAL;
	}

	pr_info("%s: end (%d)\n", __func__, ret);

	return size;
}

static ssize_t tfa_data_maxt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: not allowed to write log data: %s\n",
		__func__, blackbox[1].desc);

	return size;
}

static ssize_t tfa_count_overxmax_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value = 0xffff;
	int size;
	int ret = 0;
	char read_string[FILESIZE_LOG] = {0};

	pr_info("%s: begin\n", __func__);

	pr_info("%s: read from driver: %s\n",
		__func__, blackbox[2].desc);

	for (idx = 0; idx < devcount; idx++) {
		/* reset the data in driver after reading */
		ret = tfa_read_log(idx, 2, &value, true);
		if (ret) {
			pr_info("%s: failed to read data from driver\n",
				__func__);
			continue;
		}

		if (idx == 0)
			snprintf(read_string,
				FILESIZE_LOG, "%d", value);
		else
			snprintf(read_string,
				FILESIZE_LOG, "%s %d",
				read_string, value);
	}

	pr_info("%s: %s\n", __func__, read_string);

	if (ret)
		size = snprintf(buf, 10 + 1, "data_error");
	else
		size = snprintf(buf,
			strlen(read_string) + 1,
			"%s", read_string);

	if (size <= 0) {
		pr_err("%s: failed to show in sysfs file", __func__);
		return -EINVAL;
	}

	pr_info("%s: end (%d)\n", __func__, ret);

	return size;
}

static ssize_t tfa_count_overxmax_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: not allowed to write log data: %s\n",
		__func__, blackbox[2].desc);

	return size;
}

static ssize_t tfa_count_overtmax_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value = 0xffff;
	int size;
	int ret = 0;
	char read_string[FILESIZE_LOG] = {0};

	pr_info("%s: begin\n", __func__);

	pr_info("%s: read from driver: %s\n",
		__func__, blackbox[3].desc);

	for (idx = 0; idx < devcount; idx++) {
		/* reset the data in driver after reading */
		ret = tfa_read_log(idx, 3, &value, true);
		if (ret) {
			pr_info("%s: failed to read data from driver\n",
				__func__);
			continue;
		}

		if (idx == 0)
			snprintf(read_string,
				FILESIZE_LOG, "%d", value);
		else
			snprintf(read_string,
				FILESIZE_LOG, "%s %d",
				read_string, value);
	}

	pr_info("%s: %s\n", __func__, read_string);

	if (ret)
		size = snprintf(buf, 10 + 1, "data_error");
	else
		size = snprintf(buf,
			strlen(read_string) + 1,
			"%s", read_string);

	if (size <= 0) {
		pr_err("%s: failed to show in sysfs file", __func__);
		return -EINVAL;
	}

	pr_info("%s: end (%d)\n", __func__, ret);

	return size;
}

static ssize_t tfa_count_overtmax_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: not allowed to write log data: %s\n",
		__func__, blackbox[3].desc);

	return size;
}
#else /* TFA_LOG_IN_SEPARATE_NODES */
static ssize_t tfa_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value[TFA_LOG_MAX_COUNT] = {0xffff};
	int size;
	int ret = 0;
	char read_string[FILESIZE_LOG] = {0};

	pr_info("%s: begin\n", __func__);

	for (i = 0; i < TFA_LOG_MAX_COUNT; i++) {
		pr_info("%s: read from driver: %s\n",
			__func__, blackbox[i].desc);

		for (idx = 0; idx < devcount; idx++) {
			/* reset the data in driver after reading */
			ret = tfa_read_log(idx, i, &value[i], true);
			if (ret) {
				pr_info("%s: failed to read data from driver\n",
					__func__);
				continue;
			}

			if (idx == 0)
				snprintf(read_string,
					FILESIZE_LOG, "%d", value[i]);
			else
				snprintf(read_string,
					FILESIZE_LOG, "%s %d",
					read_string, value[i]);
		}

		pr_info("%s: %s\n", __func__, read_string);
	}

	if (ret)
		size = snprintf(buf, 10 + 1, "data_error");
	else
		size = snprintf(buf,
			strlen(read_string) + 1,
			"%s", read_string);

	if (size <= 0) {
		pr_err("%s: failed to show in sysfs file", __func__);
		return -EINVAL;
	}

	pr_info("%s: end (%d)\n", __func__, ret);

	return size;
}

static ssize_t tfa_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: not allowed to write log data\n",
		__func__);

	return size;
}
#endif /* TFA_LOG_IN_SEPARATE_NODES */

static ssize_t tfa_log_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size;

	size = snprintf(buf, 25, "%s\n", blackbox_enabled ?
		"blackbox is active" : "blackbox is inactive");

	return size;
}

static ssize_t tfa_log_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int status;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (!sysfs_streq(buf, "1") && !sysfs_streq(buf, "0")) {
		pr_debug("%s: invalid value to write\n",
			__func__);
		return -EINVAL;
	}

	ret = kstrtou32(buf, 10, &status);
	blackbox_enabled = (status) ? true : false;

	tfa_configure_log();

	return size;
}

static ssize_t tfa_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size;

	size = snprintf(buf, 25, "%s\n", cur_status ?
		"sysfs log is active" : "sysfs log is inactive");

	return size;
}

static ssize_t tfa_log_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int i, idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value[TFA_LOG_MAX_COUNT] = {0xffff};
	int ret = 0, ret2 = 0;
	int status;
	char read_string[FILESIZE_LOG] = {0};

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (!sysfs_streq(buf, "1") && !sysfs_streq(buf, "0")) {
		pr_debug("%s: invalid value to write\n",
			__func__);
		return -EINVAL;
	}

	ret = kstrtou32(buf, 10, &status);
	if (!status) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}
	if (cur_status)
		pr_info("%s: prior writing still runs\n", __func__);

	pr_info("%s: begin\n", __func__);

	cur_status = status; /* run - changed to active */
	for (i = 0; i < TFA_LOG_MAX_COUNT; i++) {
		pr_info("%s: read from driver: %s\n",
			__func__, blackbox[i].desc);

		for (idx = 0; idx < devcount; idx++) {
			/* reset the data in driver after reading */
			ret2 = tfa_read_log(idx, i, &value[i], false);
			if (ret2) {
				pr_info("%s: failed to read data from driver\n",
					__func__);
				continue;
			}

			cur_status = 0; /* done - changed to inactive */

			if (idx == 0)
				snprintf(read_string,
					FILESIZE_LOG, "%d",
					blackbox[i].prev_value);
			else
				snprintf(read_string,
					FILESIZE_LOG, "%s %d",
					read_string, blackbox[i].prev_value);
		}

		pr_info("%s: %s\n", __func__, read_string);
	}

	pr_info("%s: end (%d)\n", __func__, ret2);

	return size;
}

static int __init tfa98xx_log_init(void)
{
	int ret = 0;

	if (!g_nxp_class)
		g_nxp_class = class_create(THIS_MODULE, TFA_CLASS_NAME);
	if (g_nxp_class) {
		g_tfa_log_dev = device_create(g_nxp_class,
			NULL, 2, NULL, TFA_LOG_DEV_NAME);
		if (!IS_ERR(g_tfa_log_dev)) {
			ret = sysfs_create_group(&g_tfa_log_dev->kobj,
				&tfa_log_attr_grp);
			if (ret)
				pr_err("%s: failed to create sysfs group. ret (%d)\n",
					__func__, ret);
		} else {
			class_destroy(g_nxp_class);
		}
	}

#if defined(TFA_BLACKBOX_LOGGING)
	blackbox_enabled = true; /* enable by default */
#else
	blackbox_enabled = false; /* control with sysfs node */
#endif

	tfa_log_register(tfa_configure_log, tfa_update_log);

	/* maximum x */
	blackbox[0].desc = DESC_MAXX_LOG;
	blackbox[0].is_max = true;
	blackbox[0].is_counter = false;
	/* maximum t */
	blackbox[1].desc = DESC_MAXT_LOG;
	blackbox[1].is_max = true;
	blackbox[1].is_counter = false;
	/* counter x > x_max */
	blackbox[2].desc = DESC_OVERXMAX_COUNT;
	blackbox[2].is_max = false;
	blackbox[2].is_counter = true;
	/* counter t > t_max */
	blackbox[3].desc = DESC_OVERTMAX_COUNT;
	blackbox[3].is_max = false;
	blackbox[3].is_counter = true;

	pr_info("%s: g_nxp_class=%p\n", __func__, g_nxp_class);
	pr_info("%s: initialized\n", __func__);

	return ret;
}
module_init(tfa98xx_log_init);

static void __exit tfa98xx_log_exit(void)
{
	device_destroy(g_nxp_class, 2);
	class_destroy(g_nxp_class);
	pr_info("exited\n");
}
module_exit(tfa98xx_log_exit);

MODULE_DESCRIPTION("ASoC TFA98XX logging driver");
MODULE_LICENSE("GPL");
