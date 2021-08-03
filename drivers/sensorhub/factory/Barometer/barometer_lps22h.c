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
#include "../../ssp.h"
#include "../ssp_factory.h"
#include "../../ssp_cmd_define.h"
#include "../../ssp_comm.h"
#include "../../ssp_data.h"

#define CALIBRATION_FILE_PATH           "/efs/FactoryApp/baro_delta"

#define PR_ABS_MAX      8388607         /* 24 bit 2'compl */
#define PR_ABS_MIN      -8388608

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

ssize_t get_barometer_lps22h_name(char *buf)
{
	return sprintf(buf, "%s\n", "LPS22H");
}


ssize_t get_barometer_lps22h_vendor(char *buf)
{
	return sprintf(buf, "%s\n", "STM");
}

ssize_t set_barometer_lps22h_sea_level_pressure(struct ssp_data *data,
                                                const char *buf)
{
	int ret = 0;

	ret = sscanf(buf, "%9d", &data->buf[SENSOR_TYPE_PRESSURE].pressure_sealevel);

	if (data->buf[SENSOR_TYPE_PRESSURE].pressure_sealevel == 0) {
		pr_info("%s, our->temperature = 0\n", __func__);
		data->buf[SENSOR_TYPE_PRESSURE].pressure_sealevel = -1;
	}

	pr_info("[SSP] %s sea_level_pressure = %d\n",
	        __func__, data->buf[SENSOR_TYPE_PRESSURE].pressure_sealevel);
	return ret;
}

ssize_t get_barometer_lps22h_calibration(struct ssp_data *data, char *buf)
{
	pressure_open_calibration(data);

	return sprintf(buf, "%d\n", data->buf[SENSOR_TYPE_PRESSURE].pressure_cal);
}


ssize_t set_barometer_lps22h_calibration(struct ssp_data *data, const char *buf)
{
	int pressure_cal = 0;
	int ret = 0;;

	ret = kstrtoint(buf, 10, &pressure_cal);
	if (ret < 0) {
		pr_err("[SSP]: %s - kstrtoint failed.(%d)", __func__, ret);
		return ret;
	}

	if (pressure_cal < PR_ABS_MIN || pressure_cal > PR_ABS_MAX) {
		return -EINVAL;
	}

	data->buf[SENSOR_TYPE_PRESSURE].pressure_cal = (s32)pressure_cal;

	return ret;
}

ssize_t get_barometer_lps22h_eeprom_check(struct ssp_data *data, char *buf)
{
	char *buffer = NULL;
	int buffer_length = 0;
	int ret = 0;

	ret = ssp_send_command(data, CMD_GETVALUE, SENSOR_TYPE_PRESSURE, SENSOR_FACTORY,
	                       3000, NULL, 0, &buffer, &buffer_length);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		goto exit;
	}

	if (buffer == NULL) {
		ssp_errf("buffer is null");
		ret = ERROR;
		goto exit;
	}

	ssp_infof("%u", *buffer);
	ret = snprintf(buf, PAGE_SIZE, "%d", *buffer);

exit:
	if (buffer != NULL) {
		kfree(buffer);
	}

	return ret;
}

ssize_t get_barometer_lps22h_temperature(struct ssp_data *data, char *buf)
{
	s32 temperature = 0;
	s32 float_temperature = 0;

	temperature = (s32)(data->buf[SENSOR_TYPE_PRESSURE].temperature);
	float_temperature = ((temperature % 100) > 0 ? (temperature % 100) : -(temperature % 100));

	return sprintf(buf, "%d.%02d\n", (temperature / 100), float_temperature);
}

struct barometer_sensor_operations barometer_lps22h_ops = {
	.get_barometer_name = get_barometer_lps22h_name,
	.get_barometer_vendor = get_barometer_lps22h_vendor,
	.get_barometer_eeprom_check = get_barometer_lps22h_eeprom_check,
	.get_barometer_calibration = get_barometer_lps22h_calibration,
	.set_barometer_calibration = set_barometer_lps22h_calibration,
	.set_barometer_sea_level_pressure = set_barometer_lps22h_sea_level_pressure,
	.get_barometer_temperature = get_barometer_lps22h_temperature,
};

struct barometer_sensor_operations* get_barometer_lps22h_function_pointer(struct ssp_data *data)
{
	return &barometer_lps22h_ops;
}
