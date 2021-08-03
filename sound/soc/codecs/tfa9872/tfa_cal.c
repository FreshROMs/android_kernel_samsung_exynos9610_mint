/*
 * tfa_cal.c   tfa98xx calibration in sysfs
 *
 * Copyright (c) 2015 NXP Semiconductors
 *
 *  Author: Michael Kim <michael.kim@nxp.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/file.h>

#include "tfa98xx_tfafieldnames.h"
#include "tfa_internal.h"
#include "tfa_service.h"
#include "tfa_container.h"
#include "tfa98xx_genregs_N1C.h"

#define TFA_CLASS_NAME	"nxp"
#define TFA_CAL_DEV_NAME	"tfa_cal"
#define FILESIZE_RDC_CAL	12
#define FILESIZE_TEMP_CAL	12
#define FILEPATH_RDC_CAL	"/efs/nxp/rdc_cal"
#define FILEPATH_TEMP_CAL	"/efs/nxp/temp_cal"
#if defined(TFA_WRITE_CAL_TO_FILE)
#if defined(FOLDER_DOESNT_EXIST)
#define FOLDERPATH_NXP	"/efs/nxp/"
#endif
#else
static char rdc_string[FILESIZE_RDC_CAL] = {0};
static char temp_string[FILESIZE_TEMP_CAL] = {0};
#endif

/* ---------------------------------------------------------------------- */

static ssize_t tfa_cal_rdc_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_cal_rdc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(rdc, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_cal_rdc_show, tfa_cal_rdc_store);

static ssize_t tfa_cal_temp_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_cal_temp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(temp, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_cal_temp_show, tfa_cal_temp_store);

static ssize_t tfa_cal_status_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t tfa_cal_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR(status, S_IRUGO | S_IWUSR | S_IWGRP,
	tfa_cal_status_show, tfa_cal_status_store);

static struct attribute *tfa_cal_attr[] = {
	&dev_attr_rdc.attr,
	&dev_attr_temp.attr,
	&dev_attr_status.attr,
	NULL,
};

static struct attribute_group tfa_cal_attr_grp = {
	.attrs = tfa_cal_attr,
};

/* ---------------------------------------------------------------------- */

struct class *g_nxp_class;
struct device *g_tfa_dev;
static int cur_status;

/* ---------------------------------------------------------------------- */

static int tfa_cal_read_file(char *filename, char *data, size_t size);
#if defined(TFA_WRITE_CAL_TO_FILE)
static int tfa_cal_write_file(char *filename, char *data, size_t size);
#endif

/* ---------------------------------------------------------------------- */

static ssize_t tfa_cal_rdc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value;
	int size;
#if !defined(TFA_WRITE_CAL_TO_FILE)
	int rstr_len = 0;
#endif
	int ret = -1;
	char cal_done_string[FILESIZE_RDC_CAL] = {0};

#if !defined(TFA_WRITE_CAL_TO_FILE)
	if (rdc_string[0] != 0) {
		rstr_len = strlcpy(cal_done_string,
			rdc_string, FILESIZE_RDC_CAL);
		if (rstr_len > 0)
			ret = 0;
		goto tfa_cal_rdc_show_exit;
	} else {
		ret = tfa_cal_read_file(FILEPATH_RDC_CAL,
			cal_done_string, sizeof(cal_done_string));
		if (ret >= 0) {
			ret = kstrtou16(cal_done_string, 0, &value);
			strlcpy(rdc_string, cal_done_string, FILESIZE_RDC_CAL);
			pr_info("%s: get from file\n", __func__);
			goto tfa_cal_rdc_show_exit;
		}
	}
#else
	ret = tfa_cal_read_file(FILEPATH_RDC_CAL,
		cal_done_string, sizeof(cal_done_string));
	if (ret >= 0) {
		ret = kstrtou16(cal_done_string, 0, &value);
		pr_info("%s: get from file\n", __func__);
		goto tfa_cal_rdc_show_exit;
	}
#endif

	pr_info("%s: failed to read %s and get from amplifier\n",
		__func__, FILEPATH_RDC_CAL);

	for (idx = 0; idx < devcount; idx++) {
		ret = tfa_read_calibrate(idx, &value);
		if (ret) {
			pr_info("%s: failed to read data from amplifier\n",
				__func__);
			continue;
		}

		if (idx == 0)
			snprintf(cal_done_string,
				FILESIZE_RDC_CAL, "%d", value);
		else
			snprintf(cal_done_string,
				FILESIZE_RDC_CAL, "%s %d",
				cal_done_string, value);
	}

#if defined(TFA_WRITE_CAL_TO_FILE)
	ret = tfa_cal_write_file(FILEPATH_RDC_CAL,
		cal_done_string, sizeof(cal_done_string));
	if (ret < 0) {
		pr_err("%s: failed to write %s\n",
			__func__, FILEPATH_RDC_CAL);
		return -EINVAL;
	}
	ret = 0;
#else
	strlcpy(rdc_string, cal_done_string, FILESIZE_RDC_CAL);
#endif

tfa_cal_rdc_show_exit:
	if (ret || cal_done_string[0] == 0)
		size = snprintf(buf,
			7 + 1, "no_data");
	else
		size = snprintf(buf,
			strlen(cal_done_string) + 1,
			"%s", cal_done_string);

	if (size <= 0) {
		pr_err("%s: failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t tfa_cal_rdc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: not allowed to write calibration data\n",
		__func__);

	return size;
}

static ssize_t tfa_cal_temp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value, value_file = 0;
	int size;
#if !defined(TFA_WRITE_CAL_TO_FILE)
	int tstr_len = 0;
#endif
	int ret = 0, ret2 = -1;
	char cal_done_string[FILESIZE_TEMP_CAL] = {0};

#if !defined(TFA_WRITE_CAL_TO_FILE)
	if (temp_string[0] != 0) {
		tstr_len = strlcpy(cal_done_string,
			temp_string, FILESIZE_TEMP_CAL);
		if (tstr_len > 0)
			ret2 = 0;
		goto tfa_cal_temp_show_exit;
	} else {
		ret = tfa_cal_read_file(FILEPATH_TEMP_CAL,
			cal_done_string, sizeof(cal_done_string));
		if (ret >= 0) {
			ret2 = kstrtou16(cal_done_string, 0, &value_file);
			strlcpy(temp_string, cal_done_string,
				FILESIZE_TEMP_CAL);
			pr_info("%s: get from file\n", __func__);
			goto tfa_cal_temp_show_exit;
		}
	}
#else
	ret = tfa_cal_read_file(FILEPATH_TEMP_CAL,
		cal_done_string, sizeof(cal_done_string));
	if (ret >= 0) {
		ret2 = kstrtou16(cal_done_string, 0, &value_file);
		pr_info("%s: get from file\n", __func__);
		goto tfa_cal_temp_show_exit;
	}
#endif

	pr_info("%s: failed to read %s and get from driver\n",
		__func__, FILEPATH_TEMP_CAL);

	for (idx = 0; idx < devcount; idx++) {
		ret = tfa_read_cal_temp(idx, &value);
		if (ret) {
			pr_info("%s: failed to read temp from driver\n",
				__func__);
			continue;
		}

		if (value_file != 0xffff && value == 0xffff) {
			pr_info("%s: driver has no temp, to be updated\n",
				__func__);
			value = value_file;
		}

		if (idx == 0)
			snprintf(cal_done_string,
				FILESIZE_TEMP_CAL, "%d", value);
		else
			snprintf(cal_done_string,
				FILESIZE_TEMP_CAL, "%s %d",
				cal_done_string, value);
	}

	if ((ret == 0) && (value == value_file)) {
		pr_info("%s: driver has the same temp as file\n",
			__func__);
		ret2 = 0;
		goto tfa_cal_temp_show_exit;
	}

	if (value == 0xffff) {
		ret2 = -1;
		pr_info("%s: invalid temp\n", __func__);
		goto tfa_cal_temp_show_exit;
	}

#if defined(TFA_WRITE_CAL_TO_FILE)
	ret = tfa_cal_write_file(FILEPATH_TEMP_CAL,
		cal_done_string, sizeof(cal_done_string));
	if (ret < 0) {
		pr_err("%s: failed to write %s\n",
			__func__, FILEPATH_TEMP_CAL);
		return -EINVAL;
	}
#else
	strlcpy(temp_string, cal_done_string, FILESIZE_TEMP_CAL);
#endif
	ret2 = 0;

tfa_cal_temp_show_exit:
	if (ret2)
		size = snprintf(buf,
			7 + 1, "no_data");
	else
		size = snprintf(buf,
			strlen(cal_done_string) + 1,
			"%s", cal_done_string);

	if (size <= 0) {
		pr_err("%s: failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t tfa_cal_temp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: not allowed to write temperature used in calibration\n",
		__func__);

	return size;
}

static ssize_t tfa_cal_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size;

	size = snprintf(buf, 25, "%s\n", cur_status ?
		"calibration is active" : "calibration is inactive");

	return size;
}

static ssize_t tfa_cal_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx, devcount = tfa98xx_cnt_max_device();
	uint16_t value, value2;
	int ret = 0, status;
	char cal_done_string[FILESIZE_RDC_CAL] = {0};
	char cal_done_string2[FILESIZE_TEMP_CAL] = {0};

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (!sysfs_streq(buf, "1") && !sysfs_streq(buf, "0")) {
		pr_debug("%s: invalid value to start calibration\n",
			__func__);
		return -EINVAL;
	}

	ret = kstrtou32(buf, 10, &status);
	if (!status) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}
	if (cur_status)
		pr_info("%s: prior calibration still runs\n", __func__);

	pr_info("%s: begin\n", __func__);

	cur_status = status; /* run - changed to active */
	for (idx = 0; idx < devcount; idx++) {
		/* run calibration and read data to store */
		ret = tfa_run_calibrate(idx, &value);
		if (ret) {
			pr_info("%s: failed to calibrate speaker\n", __func__);
			continue;
		}

		if (value == 0xffff) {
			pr_info("%s: invalid data\n", __func__);
			return -EINVAL;
		}
		cur_status = 0; /* done - changed to inactive */

		if (idx == 0)
			snprintf(cal_done_string,
				FILESIZE_RDC_CAL, "%d", value);
		else
			snprintf(cal_done_string,
				FILESIZE_RDC_CAL, "%s %d",
				cal_done_string, value);

		/* read temp to store */
		ret = tfa_read_cal_temp(idx, &value2);
		if (ret) {
			pr_info("%s: failed to read temp after calibration\n",
				__func__);
			continue;
		}

		if (value2 == 0xffff) {
			pr_info("%s: invalid data\n", __func__);
			return -EINVAL;
		}

		if (idx == 0)
			snprintf(cal_done_string2,
				FILESIZE_TEMP_CAL, "%d", value2);
		else
			snprintf(cal_done_string2,
				FILESIZE_TEMP_CAL, "%s %d",
				cal_done_string2, value2);
	}

#if defined(TFA_WRITE_CAL_TO_FILE)
	ret = tfa_cal_write_file(FILEPATH_RDC_CAL,
		cal_done_string, sizeof(cal_done_string));
	if (ret < 0) {
		pr_err("%s: failed to write %s\n",
			__func__, FILEPATH_RDC_CAL);
		return -EINVAL;
	}

	ret = tfa_cal_write_file(FILEPATH_TEMP_CAL,
		cal_done_string2, sizeof(cal_done_string2));
	if (ret < 0) {
		pr_err("%s: failed to write %s\n",
			__func__, FILEPATH_TEMP_CAL);
		return -EINVAL;
	}
#else
	strlcpy(rdc_string, cal_done_string, FILESIZE_RDC_CAL);
	strlcpy(temp_string, cal_done_string2, FILESIZE_TEMP_CAL);
#endif

	pr_info("%s: end\n", __func__);

	return size;
}

/* filp_open/close() needs to be used to access file */
static int tfa_cal_read_file(char *filename, char *data, size_t size)
{
	struct file *cal_filp;
	mm_segment_t old_fs = get_fs();
	int ret;

	set_fs(KERNEL_DS);

	cal_filp = filp_open(filename, O_RDONLY, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: file does not exist: %s\n", __func__, filename);
		set_fs(old_fs);
		return -EEXIST;
	}

	ret = vfs_read(cal_filp, data, size, &cal_filp->f_pos);
	if (ret != size) {
		pr_err("%s: failed to read calibration data\n",
				__func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

#if defined(TFA_WRITE_CAL_TO_FILE)
/* filp_open/close() needs to be used to access file */
static int tfa_cal_write_file(char *filename, char *data, size_t size)
{
	struct file *cal_filp;
	mm_segment_t old_fs = get_fs();
	int ret;
#if defined(FOLDER_DOESNT_EXIST)
	static int folder_created;
#endif
	set_fs(KERNEL_DS);

#if defined(FOLDER_DOESNT_EXIST)
	if (!folder_created) {
		cal_filp = filp_open(FOLDERPATH_NXP,
			O_DIRECTORY | O_CREAT, 0660);
		if (IS_ERR(cal_filp)) {
			pr_err("%s: failed to create folder: nxp\n", __func__);
			set_fs(old_fs);
			ret = PTR_ERR(cal_filp);
			return ret;
		}
		folder_created = 1;
	}
#endif

	cal_filp = filp_open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: failed to open file: %s\n", __func__, filename);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = vfs_write(cal_filp, data, size, &cal_filp->f_pos);
	if (ret != size) {
		pr_err("%s: failed to write calibration data\n",
				__func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}
#endif /* TFA_WRITE_CAL_TO_FILE */

static int __init tfa98xx_cal_init(void)
{
	int ret = 0;

	if (!g_nxp_class)
		g_nxp_class = class_create(THIS_MODULE, TFA_CLASS_NAME);
	if (g_nxp_class) {
		g_tfa_dev = device_create(g_nxp_class,
			NULL, 1, NULL, TFA_CAL_DEV_NAME);
		if (!IS_ERR(g_tfa_dev)) {
			ret = sysfs_create_group(&g_tfa_dev->kobj,
				&tfa_cal_attr_grp);
			if (ret)
				pr_err("%s: failed to create sysfs group. ret (%d)\n",
					__func__, ret);
		} else {
			class_destroy(g_nxp_class);
		}
	}

	pr_info("%s: g_nxp_class=%p\n", __func__, g_nxp_class);
	pr_info("%s: initialized\n", __func__);

	return ret;
}
module_init(tfa98xx_cal_init);

static void __exit tfa98xx_cal_exit(void)
{
	device_destroy(g_nxp_class, 1);
	class_destroy(g_nxp_class);
	pr_info("exited\n");
}
module_exit(tfa98xx_cal_exit);

MODULE_DESCRIPTION("ASoC TFA98XX calibration driver");
MODULE_LICENSE("GPL");
