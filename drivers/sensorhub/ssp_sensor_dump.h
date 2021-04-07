/*
 *  Copyright (C) 2016, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SSP_DUMP_H__
#define __SSP_DUMP_H__

#include "ssp.h"

//#define SENSOR_DUMP_FILE_STORE

#ifdef SENSOR_DUMP_FILE_STORE
#define SENSOR_DUMP_PATH                "/data/log/sensor_dump_"
#define SENSOR_DUMP_FILE_LENGTH         100
#endif

#define DUMPREGISTER_MAX_SIZE                   130
#define NUM_LINE_ITEM                                   16
#define LENGTH_1BYTE_HEXA_WITH_BLANK    3       /* example : 'ff '*/
#define LENGTH_EXTRA_STRING                             1       /* '\0' */
#define LENGTH_SENSOR_NAME_MAX                  30
#define LENGTH_SENSOR_TYPE_MAX                  3       /*      SENSOR_MAX digits = 3 */

#define sensor_dump_length(x)  (x*LENGTH_1BYTE_HEXA_WITH_BLANK+LENGTH_EXTRA_STRING)

#define SENSOR_DUMP_SENSOR_LIST         {SENSOR_TYPE_ACCELEROMETER, SENSOR_TYPE_GYROSCOPE, \
                                                                        SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_TYPE_PRESSURE, \
                                                                        SENSOR_TYPE_PROXIMITY, SENSOR_TYPE_LIGHT}

int send_sensor_dump_command(struct ssp_data *data, u8 sensor_type);
int send_all_sensor_dump_command(struct ssp_data *data);
#endif
