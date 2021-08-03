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

#ifndef __SSP_IIO_H__
#define __SSP_IIO_H__

#include "ssp.h"

#define CAMERA_LUX_ENABLE		-1
#define CAMERA_LUX_DISABLE		-2

struct sensor_value;

void report_sensor_data(struct ssp_data *, int, struct sensor_value *);
void report_meta_data(struct ssp_data *, struct sensor_value *);
int initialize_indio_dev(struct device *dev, struct ssp_data *data);
void remove_indio_dev(struct ssp_data *data);
void report_scontext_notice_data(struct ssp_data *data, char notice);
void report_scontext_data(struct ssp_data *data, char *data_buf, u32 length);
void report_camera_lux_data(struct ssp_data *data, int lux);
void report_sensorhub_data(struct ssp_data *data, char* buf);
#endif
