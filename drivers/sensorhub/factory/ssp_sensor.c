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
#include "../ssp_comm.h"
#include "../ssp_cmd_define.h"
#include "../ssp_data.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define MODEL_NAME      "CHUB_EXYNOS9610"

#if defined(CONFIG_SENSORS_SSP_A50S)
#define SSP_FIRMWARE_REVISION       	21020200
#elif defined(CONFIG_SENSORS_SSP_M30S)
#define SSP_FIRMWARE_REVISION       	21020200
#else // CONFIG_SENSOR_SSP_A50
#define SSP_FIRMWARE_REVISION       	21020200
#endif

#define FACTORY_DATA_MAX        100
static char buffer[FACTORY_DATA_MAX];

unsigned int get_module_rev(struct ssp_data *data)
{
	return SSP_FIRMWARE_REVISION;
}

ssize_t mcu_revision_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "SLSI01%u,SLSI01%u\n", data->curr_fw_rev, get_module_rev(data));
}

ssize_t mcu_model_name_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MODEL_NAME);
}

ssize_t mcu_factorytest_store(struct device *dev,
                              struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;
	char *temp_buffer = NULL;
	int temp_buffer_length = 0;

	if (sysfs_streq(buf, "1")) {

		ret = ssp_send_command(data, CMD_GETVALUE, TYPE_MCU, SENSOR_FACTORY, 1000, NULL,
		                       0, &temp_buffer, &temp_buffer_length);

		if (ret != SUCCESS) {
			ssp_errf("fail %d", ret);
			if (temp_buffer != NULL) {
				kfree(temp_buffer);
			}
			return ERROR;
		}

	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);
		
		return -EINVAL;
	}

	ssp_info("MCU Factory Test Start! - %d, length = %d", ret, temp_buffer_length);
	memcpy(buffer, temp_buffer, temp_buffer_length);

	if (temp_buffer != NULL) {
		kfree(temp_buffer);
	}

	return size;
}

ssize_t mcu_factorytest_show(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
	bool bMcuTestSuccessed = false;

	ssp_info("MCU Factory Test Data : %u, %u, %u, %u, %u",
	         buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

	/* system clock, RTC, I2C Master, I2C Slave, externel pin */
	if ((buffer[0] == 1)
	    && (buffer[1] == 1)
	    && (buffer[2] == 1)
	    && (buffer[3] == 1)
	    && (buffer[4] == 1)) {
		bMcuTestSuccessed = true;
	}

	ssp_info("MCU Factory Test Result - %s, %s, %s\n", MODEL_NAME,
	         (bMcuTestSuccessed ? "OK" : "NG"), "OK");

	return sprintf(buf, "%s,%s,%s\n", MODEL_NAME,
	               (bMcuTestSuccessed ? "OK" : "NG"), "OK");
}

ssize_t mcu_sleep_factorytest_store(struct device *dev,
                                    struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int ret = 0;
	char *temp_buffer = NULL;
	int temp_buffer_length = 0;

	if (sysfs_streq(buf, "1")) {

		ret = ssp_send_command(data, CMD_GETVALUE, TYPE_MCU, MCU_SLEEP_TEST, 1000, NULL,
		                       0, &temp_buffer, &temp_buffer_length);

		if (ret != SUCCESS) {
			ssp_errf("fail %d", ret);
			if (temp_buffer != NULL) {
				kfree(temp_buffer);
			}
			return ERROR;
		}

	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);

		return -EINVAL;
	}

	ssp_info("MCU Sleep Factory Test Start! - %d, length = %d", 1,
	         temp_buffer_length);
	buffer[0] = temp_buffer_length;
	buffer[1] = 0;
	memcpy(&buffer[2], temp_buffer, temp_buffer_length);

	if (temp_buffer != NULL) {
		kfree(temp_buffer);
	}

	return size;
}

ssize_t mcu_sleep_factorytest_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	int data_index, sensor_type = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct sensor_value fsb[SENSOR_TYPE_MAX];
	u16 length = 0;

	length = (((u16)(buffer[1])) << 8) + ((u16)(buffer[0]));
	memset(fsb, 0, sizeof(struct sensor_value) * SENSOR_TYPE_MAX);

	ssp_infof("length = %d", length);

	for (data_index = 2; data_index < length;) {
		sensor_type = buffer[data_index++];

		if ((sensor_type < 0) ||
		    (sensor_type > (SENSOR_TYPE_MAX - 1))) {
			pr_err("[SSP]: %s - Mcu data frame error %d\n",
			       __func__, sensor_type);
			goto exit;
		}

		get_sensordata(data, (char *)buffer, &data_index,
		               sensor_type, &(fsb[sensor_type]));

		get_timestamp(data, (char *)buffer, &data_index,
		              &(fsb[sensor_type]), sensor_type);
	}

	fsb[SENSOR_TYPE_PRESSURE].pressure
	-= data->buf[SENSOR_TYPE_PRESSURE].pressure_cal;

exit:
	ssp_infof("Result\n"
	          "[SSP] accel %d,%d,%d\n"
	          "[SSP] gyro %d,%d,%d\n"
	          "[SSP] mag %d,%d,%d\n"
	          "[SSP] baro %d,%d\n"
	          "[SSP] prox %u,%u\n"
	          "[SSP] light %u,%u,%u,%u,%u,%u"
	          "[SSP]: temp %d,%d,%d\n",
	          fsb[SENSOR_TYPE_ACCELEROMETER].x, fsb[SENSOR_TYPE_ACCELEROMETER].y,
	          fsb[SENSOR_TYPE_ACCELEROMETER].z, fsb[SENSOR_TYPE_GYROSCOPE].x,
	          fsb[SENSOR_TYPE_GYROSCOPE].y, fsb[SENSOR_TYPE_GYROSCOPE].z,
	          fsb[SENSOR_TYPE_GEOMAGNETIC_FIELD].cal_x,
	          fsb[SENSOR_TYPE_GEOMAGNETIC_FIELD].cal_y,
	          fsb[SENSOR_TYPE_GEOMAGNETIC_FIELD].cal_z, fsb[SENSOR_TYPE_PRESSURE].pressure,
	          fsb[SENSOR_TYPE_PRESSURE].temperature,
	          fsb[SENSOR_TYPE_PROXIMITY].prox, fsb[SENSOR_TYPE_PROXIMITY].prox_ex,
	          fsb[SENSOR_TYPE_LIGHT].r, fsb[SENSOR_TYPE_LIGHT].g, fsb[SENSOR_TYPE_LIGHT].b,
	          fsb[SENSOR_TYPE_LIGHT].w,
	          fsb[SENSOR_TYPE_LIGHT].a_time, fsb[SENSOR_TYPE_LIGHT].a_gain,
	          fsb[SENSOR_TYPE_TEMPERATURE].x, fsb[SENSOR_TYPE_TEMPERATURE].y,
	          fsb[SENSOR_TYPE_TEMPERATURE].z);

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,"
	               "%u,%u,%u,%u,%u,%u,0,0,0,0,%d,%d\n",
	               fsb[SENSOR_TYPE_ACCELEROMETER].x, fsb[SENSOR_TYPE_ACCELEROMETER].y,
	               fsb[SENSOR_TYPE_ACCELEROMETER].z, fsb[SENSOR_TYPE_GYROSCOPE].x,
	               fsb[SENSOR_TYPE_GYROSCOPE].y, fsb[SENSOR_TYPE_GYROSCOPE].z,
	               fsb[SENSOR_TYPE_GEOMAGNETIC_FIELD].cal_x,
	               fsb[SENSOR_TYPE_GEOMAGNETIC_FIELD].cal_y,
	               fsb[SENSOR_TYPE_GEOMAGNETIC_FIELD].cal_z, fsb[SENSOR_TYPE_PRESSURE].pressure,
	               fsb[SENSOR_TYPE_PRESSURE].temperature, fsb[SENSOR_TYPE_PROXIMITY].prox_ex,
	               fsb[SENSOR_TYPE_LIGHT].r, fsb[SENSOR_TYPE_LIGHT].g, fsb[SENSOR_TYPE_LIGHT].b,
	               fsb[SENSOR_TYPE_LIGHT].w,
	               fsb[SENSOR_TYPE_LIGHT].a_time, fsb[SENSOR_TYPE_LIGHT].a_gain,
	               fsb[SENSOR_TYPE_TEMPERATURE].x, fsb[SENSOR_TYPE_TEMPERATURE].y);
}
