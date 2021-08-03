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
#include <linux/slab.h>
#include "../ssp.h"
#include "../sensors_core.h"
#include "../ssp_data.h"
#include "ssp_factory.h"

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/
static ssize_t light_name_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->light_ops == NULL || data->light_ops->get_light_name == NULL)
		return -EINVAL;
	return data->light_ops->get_light_name(buf);
}

static ssize_t light_vendor_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->light_ops == NULL || data->light_ops->get_light_vendor == NULL)
		return -EINVAL;
	return data->light_ops->get_light_vendor(buf);
}

static ssize_t light_lux_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->light_ops == NULL || data->light_ops->get_lux == NULL)
		return -EINVAL;
	return data->light_ops->get_lux(data, buf);
}

static ssize_t light_data_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	if (data->light_ops == NULL || data->light_ops->get_light_data == NULL)
		return -EINVAL;
	return data->light_ops->get_light_data(data, buf);
}

static ssize_t light_coef_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->light_ops == NULL || data->light_ops->get_light_coefficient == NULL)
		return -EINVAL;
	ret = data->light_ops->get_light_coefficient(data, buf);

	return ret;
}

static ssize_t light_circle_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->light_ops == NULL || data->light_ops->get_light_circle == NULL)
		return -EINVAL;
	ret = data->light_ops->get_light_circle(data, buf);

	return ret;
}

static DEVICE_ATTR(name, S_IRUGO, light_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, light_vendor_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);
static DEVICE_ATTR(coef, S_IRUGO, light_coef_show, NULL);
static DEVICE_ATTR(light_circle, S_IRUGO, light_circle_show, NULL);

static struct device_attribute *light_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_coef,
	&dev_attr_light_circle,
	NULL,
};

void select_light_ops(struct ssp_data *data, char *name)
{
	struct light_sensor_operations **light_ops_ary;
	int count = 0, i;
	char temp_buffer[SENSORNAME_MAX_LEN] = {0,};

	ssp_infof("");

#if defined(CONFIG_SENSORS_SSP_LIGHT_TMD3700)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_LIGHT_TMD3725)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_LIGHT_TCS3701)
	count++;
#endif
#if defined(CONFIG_SENSORS_SSP_LIGHT_VEML3328)
	count++;
#endif

	if (count == 0) {
		ssp_infof("count is 0");
		return;
	}

	light_ops_ary = (struct light_sensor_operations **)kzalloc(count * sizeof(struct light_sensor_operations *), GFP_KERNEL);

	i = 0;
#if defined(CONFIG_SENSORS_SSP_LIGHT_TMD3700)
	light_ops_ary[i++] = get_light_tmd3700_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_LIGHT_TMD3725)
	light_ops_ary[i++] = get_light_tmd3725_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_LIGHT_TCS3701)
	light_ops_ary[i++] = get_light_tcs3701_function_pointer(data);
#endif
#if defined(CONFIG_SENSORS_SSP_LIGHT_VEML3328)
	light_ops_ary[i++] = get_light_veml3328_function_pointer(data);
#endif

	if (count > 1) {
		for (i = 0; i < count ; i++) {
			int size = light_ops_ary[i]->get_light_name(temp_buffer);

			temp_buffer[size - 1] = '\0';
			ssp_infof("%d light name : %s", i, temp_buffer);

			if (strcmp(temp_buffer, name) == 0)
				break;
		}

		if (i == count)
			i = 0;
	} else
		i = 0;

	data->light_ops = light_ops_ary[i];
	kfree(light_ops_ary);
}

void initialize_light_factorytest(struct ssp_data *data)
{
	sensors_register(data->devices[SENSOR_TYPE_LIGHT], data, light_attrs,
			 "light_sensor");
}

void remove_light_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->devices[SENSOR_TYPE_LIGHT], light_attrs);
}
