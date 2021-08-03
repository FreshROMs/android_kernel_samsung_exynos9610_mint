/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#ifndef __SSP_DATA_H__
#define __SSP_DATA_H__
#include "ssp.h"

u64 get_current_timestamp(void);

int parse_dataframe(struct ssp_data *, char *, int);

int get_sensor_scanning_info(struct ssp_data *data);
int get_firmware_rev(struct ssp_data *data);
int set_sensor_position(struct ssp_data *data);
u64 get_current_timestamp(void);

void enable_timestamp_sync_timer(struct ssp_data *);
void disable_timestamp_sync_timer(struct ssp_data *);
int initialize_timestamp_sync_timer(struct ssp_data *);

void get_sensordata(struct ssp_data *, char *, int *, int, struct sensor_value *);
void get_timestamp(struct ssp_data *, char *, int *, struct sensor_value *, int);

int get_sensorname(struct ssp_data *data, int sensor_type, char* name, int size);

#ifdef CONFIG_SENSORS_SSP_PROXIMITY
void set_proximity_threshold(struct ssp_data *data);
#ifdef CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL
void set_proximity_threshold_addval(struct ssp_data *data);
#endif
#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
void set_proximity_setting_mode(struct ssp_data *data);
int save_proximity_setting_mode(struct ssp_data *data);
int open_proximity_setting_mode(struct ssp_data *data);
#endif
void do_proximity_calibration(struct ssp_data *data);
void proximity_calibration_off(struct ssp_data *data);
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT
void set_light_coef(struct ssp_data *data);
void set_light_brightness(struct ssp_data *data);
void set_light_ab_camera_hysteresis(struct ssp_data *data);
#endif
#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
int gyro_open_calibration(struct ssp_data *data);
int set_gyro_cal(struct ssp_data *data);
int save_gyro_cal_data(struct ssp_data *data, s16 *cal_data);
#endif
#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
int accel_open_calibration(struct ssp_data *);
int set_accel_cal(struct ssp_data *);
#endif
#ifdef CONFIG_SENSORS_SSP_BAROMETER
int pressure_open_calibration(struct ssp_data *);
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC
int set_pdc_matrix(struct ssp_data *data);
int mag_open_calibration(struct ssp_data *data);
int set_mag_cal(struct ssp_data *data);
int save_mag_cal_data(struct ssp_data *data, u8 *cal_data);
#endif
#endif /* __SSP_DATA_H__ */
