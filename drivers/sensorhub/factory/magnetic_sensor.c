/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/slab.h>
#include "../ssp.h"
#include "../sensors_core.h"
#include "../ssp_data.h"
#include "ssp_factory.h"

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

static ssize_t magnetic_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_name == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_name(buf);
}

static ssize_t magnetic_vendor_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_vendor == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_vendor(buf);
}

#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09916C) || defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09911)
static ssize_t magnetic_get_asa(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_asa == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_asa(data, buf);
}
#endif

static ssize_t magnetic_get_status(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_status == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_status(data, buf);
}

static ssize_t magnetic_check_dac(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_dac == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_dac(data, buf);
}

static ssize_t magnetic_logging_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_logging_data == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_logging_data(data, buf);
}

static ssize_t magnetic_hw_offset_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", data->magcal.offset_x, data->magcal.offset_y, data->magcal.offset_z);
}

static ssize_t magnetic_matrix_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_matrix == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_matrix(data, buf);
}

static ssize_t magnetic_matrix_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->magnetic_ops == NULL || data->magnetic_ops->set_magnetic_matrix == NULL)
		return -EINVAL;
	ret = data->magnetic_ops->set_magnetic_matrix(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);
	return size;
}

static ssize_t magnetic_raw_data_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_raw_data == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_raw_data(data, buf);
}

static ssize_t magnetic_raw_data_store(struct device *dev,
				       struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->magnetic_ops == NULL || data->magnetic_ops->set_magnetic_raw_data == NULL)
		return -EINVAL;
	ret = data->magnetic_ops->set_magnetic_raw_data(data, buf);
	if (ret < 0)
		ssp_errf("- failed = %d", ret);

	return size;
}

static ssize_t magnetic_adc_data_read(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_adc == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_adc(data, buf);
}

static ssize_t magnetic_get_selftest(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->magnetic_ops == NULL || data->magnetic_ops->get_magnetic_selftest == NULL)
		return -EINVAL;
	return data->magnetic_ops->get_magnetic_selftest(data, buf);
}

static DEVICE_ATTR(name, S_IRUGO, magnetic_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, magnetic_vendor_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO | S_IWUSR | S_IWGRP,
		   magnetic_raw_data_show, magnetic_raw_data_store);
static DEVICE_ATTR(adc, S_IRUGO, magnetic_adc_data_read, NULL);
static DEVICE_ATTR(dac, S_IRUGO, magnetic_check_dac, NULL);
static DEVICE_ATTR(selftest, S_IRUGO, magnetic_get_selftest, NULL);

static DEVICE_ATTR(status, S_IRUGO,  magnetic_get_status, NULL);
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09916C) || defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09911)
static DEVICE_ATTR(ak09911_asa, S_IRUGO, magnetic_get_asa, NULL);
#endif
static DEVICE_ATTR(logging_data, S_IRUGO, magnetic_logging_show, NULL);

static DEVICE_ATTR(hw_offset, S_IRUGO, magnetic_hw_offset_show, NULL);
static DEVICE_ATTR(matrix, S_IRUGO | S_IWUSR | S_IWGRP, magnetic_matrix_show,
		   magnetic_matrix_store);

static struct device_attribute *mag_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_adc,
	&dev_attr_dac,
	&dev_attr_raw_data,
	&dev_attr_selftest,
	&dev_attr_status,
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09916C) || defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09911)
	&dev_attr_ak09911_asa,
#endif
	&dev_attr_logging_data,
	&dev_attr_hw_offset,
	&dev_attr_matrix,
	NULL,
};

void select_magnetic_ops(struct ssp_data *data, char *name)
{
	struct magnetic_sensor_operations **mag_ops_ary;
	int count = 0, i;
	char temp_buffer[SENSORNAME_MAX_LEN] = {0,};

	ssp_infof("");

#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09918C)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_MMC5603)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09916C)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09911)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_LSM303AH)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_YAS539)
	count++;
#endif
	if (count == 0) {
		ssp_infof("count is 0");
		return;
	}

	mag_ops_ary = (struct magnetic_sensor_operations **)kzalloc(count * sizeof(struct magnetic_sensor_operations *), GFP_KERNEL);

	i = 0;
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09918C)
	mag_ops_ary[i++] = get_magnetic_ak09918c_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_MMC5603)
	mag_ops_ary[i++] = get_magnetic_mmc5603_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09916C)
	mag_ops_ary[i++] = get_magnetic_ak09916c_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_AK09911)
	mag_ops_ary[i++] = get_magnetic_ak09911_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_LSM303AH)
	mag_ops_ary[i++] = get_magnetic_lsm303ah_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_MAGNETIC_YAS539)
	mag_ops_ary[i++] = get_magnetic_yas539_function_pointer(data);
#endif

	if (count > 1) {
		for (i = 0; i < count ; i++) {
			int size = mag_ops_ary[i]->get_magnetic_name(temp_buffer);

			temp_buffer[size - 1] = '\0';
			ssp_infof("%d name : %s", i, temp_buffer);

			if (strcmp(temp_buffer, name) == 0)
				break;
		}

		if (i == count)
			i = 0;
	} else
		i = 0;

	data->magnetic_ops = mag_ops_ary[i];
	kfree(mag_ops_ary);
}

void initialize_magnetic_factorytest(struct ssp_data *data)
{
	sensors_register(data->devices[SENSOR_TYPE_GEOMAGNETIC_FIELD], data, mag_attrs,
			 "magnetic_sensor");
}

void remove_magnetic_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->devices[SENSOR_TYPE_GEOMAGNETIC_FIELD], mag_attrs);
}
