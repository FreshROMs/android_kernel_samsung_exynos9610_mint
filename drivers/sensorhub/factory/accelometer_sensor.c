/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "../ssp.h"
#include "../sensors_core.h"
#include "../ssp_sysfs.h"
#include "../ssp_data.h"
#include "ssp_factory.h"
/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

static ssize_t accel_name_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->accel_ops == NULL || data->accel_ops->get_accel_name == NULL)
		return -EINVAL;
	return data->accel_ops->get_accel_name(buf);
}

static ssize_t accel_vendor_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->accel_ops == NULL || data->accel_ops->get_accel_vendor == NULL)
		return -EINVAL;
	return data->accel_ops->get_accel_vendor(buf);
}

static ssize_t accel_calibration_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->accel_ops == NULL || data->accel_ops->get_accel_calibration == NULL)
		return -EINVAL;
	return data->accel_ops->get_accel_calibration(data, buf);
}

static ssize_t accel_calibration_store(struct device *dev,
				       struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->accel_ops == NULL || data->accel_ops->set_accel_calibration == NULL)
		return -EINVAL;

	ret = data->accel_ops->set_accel_calibration(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);
	return size;
}

static ssize_t raw_data_read(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->accel_ops == NULL || data->accel_ops->get_accel_raw_data == NULL)
		return -EINVAL;
	return data->accel_ops->get_accel_raw_data(data, buf);
}

static ssize_t accel_reactive_alert_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->accel_ops == NULL || data->accel_ops->get_accel_reactive_alert == NULL)
		return -EINVAL;
	return data->accel_ops->get_accel_reactive_alert(data, buf);
}

static ssize_t accel_reactive_alert_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->accel_ops == NULL || data->accel_ops->set_accel_reactive_alert == NULL)
		return -EINVAL;

	ret = data->accel_ops->set_accel_reactive_alert(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);

	return size;
}

static ssize_t accel_selftest_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->accel_ops == NULL || data->accel_ops->get_accel_selftest == NULL)
		return -EINVAL;
	return data->accel_ops->get_accel_selftest(data, buf);
}


static ssize_t accel_lowpassfilter_store(struct device *dev,
					 struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->accel_ops == NULL || data->accel_ops->set_accel_lowpassfilter == NULL)
		return -EINVAL;

	ret = data->accel_ops->set_accel_lowpassfilter(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);

	return size;
}

void select_accel_ops(struct ssp_data *data, char *name)
{
	struct  accelometer_sensor_operations **accel_ops_ary;
	int count = 0, i;
	char temp_buffer[SENSORNAME_MAX_LEN] = {0,};

	ssp_infof("");

#if defined(CONFIG_SENSORS_SSP_ACCELOMETER_ICM42605M)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_ACCELOMETER_LSM6DSL)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_ACCELOMETER_K6DS3TR)
	count++;
#endif

	if (count == 0) {
		ssp_infof("count is 0");
		return;
	}

	accel_ops_ary = (struct accelometer_sensor_operations **)kzalloc(count * sizeof(struct accelometer_sensor_operations *), GFP_KERNEL);

	i = 0;
#if defined(CONFIG_SENSORS_SSP_ACCELOMETER_ICM42605M)
	accel_ops_ary[i++] = get_accelometer_icm42605m_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_ACCELOMETER_LSM6DSL)
	accel_ops_ary[i++] = get_accelometer_lsm6dsl_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_ACCELOMETER_K6DS3TR)
	accel_ops_ary[i++] = get_accelometer_k6ds3tr_function_pointer(data);
#endif

	if (count > 1) {
		for (i = 0; i < count ; i++) {
			int size = accel_ops_ary[i]->get_accel_name(temp_buffer);

			temp_buffer[size - 1] = '\0';
			ssp_infof("%d accel name : %s", i, temp_buffer);

			if (strcmp(temp_buffer, name) == 0)
				break;
		}

		if (i == count)
			i = 0;
	} else
		i = 0;

	data->accel_ops = accel_ops_ary[i];
	kfree(accel_ops_ary);
}

static DEVICE_ATTR(name, S_IRUGO, accel_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, accel_vendor_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		   accel_calibration_show, accel_calibration_store);
static DEVICE_ATTR(raw_data, S_IRUGO, raw_data_read, NULL);
static DEVICE_ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
		   accel_reactive_alert_show, accel_reactive_alert_store);
static DEVICE_ATTR(selftest, S_IRUGO, accel_selftest_show, NULL);
static DEVICE_ATTR(lowpassfilter, S_IWUSR | S_IWGRP,
		   NULL, accel_lowpassfilter_store);

static struct device_attribute *acc_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_calibration,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	&dev_attr_selftest,
	&dev_attr_lowpassfilter,
	NULL,
};

void initialize_accel_factorytest(struct ssp_data *data)
{
	sensors_register(data->devices[SENSOR_TYPE_ACCELEROMETER], data, acc_attrs,
			 data->info[SENSOR_TYPE_ACCELEROMETER].name);
}

void remove_accel_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->devices[SENSOR_TYPE_ACCELEROMETER], acc_attrs);
}
