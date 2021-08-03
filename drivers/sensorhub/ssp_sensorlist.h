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

#ifndef __SSP_SENSORLIST_H__
#define __SSP_SENSORLIST_H__

/* Sensors's reporting mode */
#define SENSOR_INFO_UNKNOWN			     {"", false, 0, 0}
#define SENSOR_INFO_ACCELEROMETER		       {"accelerometer_sensor", true, 6, 6}
#define SENSOR_INFO_GEOMAGNETIC_FIELD		   {"geomagnetic_sensor", true, 7, 7}
#define SENSOR_INFO_GYRO				{"gyro_sensor", true, 6, 6}
#define SENSOR_INFO_PRESSURE			    {"pressure_sensor", true, 6, 14}
#define SENSOR_INFO_PROXIMITY			   {"proximity_sensor", true, 3, 1}
#define SENSOR_INFO_ROTATION_VECTOR		     {"rotation_vector_sensor", true, 17, 17}
#define SENSOR_INFO_MAGNETIC_FIELD_UNCALIBRATED	 {"uncal_geomagnetic_sensor", true, 12, 12}
#define SENSOR_INFO_GYRO_UNCALIBRATED		   {"uncal_gyro_sensor", true, 12, 12}
#define SENSOR_INFO_SIGNIFICANT_MOTION		  {"sig_motion_sensor", true, 1, 1}
#define SENSOR_INFO_STEP_DETECTOR		       {"step_det_sensor", true, 1, 1}
#define SENSOR_INFO_STEP_COUNTER			{"step_cnt_sensor", true, 4, 12}
#define SENSOR_INFO_GEOMAGNETIC_ROTATION_VECTOR	 {"geomagnetic_rotation_vector_sensor", true, 17, 17}
#define SENSOR_INFO_TILT_DETECTOR		       {"tilt_detector", true, 1, 1}
#define SENSOR_INFO_PICK_UP_GESTURE		     {"pickup_gesture", true, 1, 1}
#define SENSOR_INFO_PROXIMITY_RAW		       {"proximity_raw", true, 2, 0}
#define SENSOR_INFO_GEOMAGNETIC_POWER		   {"geomagnetic_power", true, 6, 6}
#define SENSOR_INFO_INTERRUPT_GYRO		      {"interrupt_gyro_sensor", true, 6, 6}
#define SENSOR_INFO_SCONTEXT			    {"scontext_iio", true, 0, 64}
#define SENSOR_INFO_CALL_GESTURE			{"call_gesture", true, 1, 1}
#define SENSOR_INFO_WAKE_UP_MOTION		      {"wake_up_motion", true, 1, 1}
#define SENSOR_INFO_LIGHT			       {"light_sensor", true, 28, 4}
#define SENSOR_INFO_LIGHT_CCT			   {"light_cct_sensor", true, 24, 12}
#define SENSOR_INFO_AUTO_BIRGHTNESS		     {"auto_brightness", true, 9, 5}
#define SENSOR_INFO_VDIS_GYRO			   {"vdis_gyro_sensor", true, 6, 6}
#define SENSOR_INFO_POCKET_MODE			 {"pocket_mode_sensor", true, 5, 5}
#define SENSOR_INFO_PROXIMITY_CALIBRATION	       {"proximity_calibration", true, 4, 0}
#define SENSOR_INFO_SENSORHUB			   {"sensorhub_sensor", true, 0, 2}
#define SENSOR_INFO_PROTOS_MOTION		       {"protos_motion", true, 1, 1}
#endif

