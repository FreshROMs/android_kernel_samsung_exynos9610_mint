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
#include <linux/kernel.h>
#include <linux/slab.h>
#include "../ssp.h"
#include "../sensors_core.h"
#include "ssp_factory.h"
#include "../ssp_comm.h"
#include "../ssp_cmd_define.h"
#include "../ssp_data.h"

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/
#define SELFTEST_REVISED 1

static ssize_t gyro_name_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->gyro_ops == NULL || data->gyro_ops->get_gyro_name == NULL)
		return -EINVAL;
	return data->gyro_ops->get_gyro_name(buf);
}


static ssize_t gyro_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->gyro_ops == NULL || data->gyro_ops->get_gyro_vendor == NULL)
		return -EINVAL;

	return data->gyro_ops->get_gyro_vendor(buf);
}

static ssize_t gyro_power_off(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->gyro_ops == NULL || data->gyro_ops->get_gyro_power_off == NULL)
		return -EINVAL;

	return data->gyro_ops->get_gyro_power_off(buf);
}

static ssize_t gyro_power_on(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->gyro_ops == NULL || data->gyro_ops->get_gyro_power_on == NULL)
		return -EINVAL;

	return data->gyro_ops->get_gyro_power_on(buf);
}


static ssize_t gyro_temp_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->gyro_ops == NULL || data->gyro_ops->get_gyro_temperature == NULL)
		return -EINVAL;

	return data->gyro_ops->get_gyro_temperature(data, buf);
}


static ssize_t gyro_selftest_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->gyro_ops == NULL || data->gyro_ops->get_gyro_selftest == NULL)
		return -EINVAL;

	return data->gyro_ops->get_gyro_selftest(data, buf);
}

static ssize_t gyro_selftest_dps_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->buf[SENSOR_TYPE_GYROSCOPE].gyro_dps);
}

static ssize_t gyro_selftest_dps_store(struct device *dev,
				       struct device_attribute *attr, const char *buf, size_t size)
{
	ssp_info("Do not support gyro dps selftest");
	return size;
}

static ssize_t gyro_selftest_revised_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", SELFTEST_REVISED);
}

static DEVICE_ATTR(name, S_IRUGO, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, gyro_vendor_show, NULL);
static DEVICE_ATTR(power_off, S_IRUGO, gyro_power_off, NULL);
static DEVICE_ATTR(power_on, S_IRUGO, gyro_power_on, NULL);
static DEVICE_ATTR(temperature, S_IRUGO, gyro_temp_show, NULL);
static DEVICE_ATTR(selftest, S_IRUGO, gyro_selftest_show, NULL);
static DEVICE_ATTR(selftest_dps, S_IRUGO | S_IWUSR | S_IWGRP,
		   gyro_selftest_dps_show, gyro_selftest_dps_store);
static DEVICE_ATTR(selftest_revised, S_IRUGO, gyro_selftest_revised_show, NULL);

static struct device_attribute *gyro_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_power_on,
	&dev_attr_power_off,
	&dev_attr_temperature,
	&dev_attr_selftest_dps,
	&dev_attr_selftest_revised,
	NULL,
};

void select_gyro_ops(struct ssp_data *data, char *name)
{
	struct  gyroscope_sensor_operations **gyro_ops_ary;
	int count = 0, i;
	char temp_buffer[SENSORNAME_MAX_LEN] = {0,};

	ssp_infof("");

#if defined(CONFIG_SENSORS_SSP_GYROSCOPE_ICM42605M)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_GYROSCOPE_LSM6DSL)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_GYROSCOPE_K6DS3TR)
	count++;
#endif

	if (count == 0) {
		ssp_infof("count is 0");
		return;
	}

	gyro_ops_ary = (struct gyroscope_sensor_operations **)kzalloc(count * sizeof(struct gyroscope_sensor_operations *), GFP_KERNEL);

	i = 0;
#if defined(CONFIG_SENSORS_SSP_GYROSCOPE_ICM42605M)
	gyro_ops_ary[i++] = get_gyroscope_icm42605m_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_GYROSCOPE_LSM6DSL)
	gyro_ops_ary[i++] = get_gyroscope_lsm6dsl_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_GYROSCOPE_K6DS3TR)
	gyro_ops_ary[i++] = get_gyroscope_k6ds3tr_function_pointer(data);
#endif

	if (count > 1) {
		for (i = 0; i < count ; i++) {
			int size = gyro_ops_ary[i]->get_gyro_name(temp_buffer);

			temp_buffer[size - 1] = '\0';
			ssp_infof("%d gyro name : %s", i, temp_buffer);

			if (strcmp(temp_buffer, name) == 0)
				break;
		}

		if (i == count)
			i = 0;
	} else
		i = 0;

	data->gyro_ops = gyro_ops_ary[i];
	kfree(gyro_ops_ary);
}

void initialize_gyro_factorytest(struct ssp_data *data)
{
	sensors_register(data->devices[SENSOR_TYPE_GYROSCOPE], data, gyro_attrs,
			 data->info[SENSOR_TYPE_GYROSCOPE].name);
}

void remove_gyro_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->devices[SENSOR_TYPE_GYROSCOPE], gyro_attrs);
}
