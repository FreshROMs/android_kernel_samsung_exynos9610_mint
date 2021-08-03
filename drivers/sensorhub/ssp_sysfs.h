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

#ifndef __SSP_SYSFS_H__
#define __SSP_SYSFS_H__

#include "ssp.h"


int initialize_sysfs(struct ssp_data *);
void remove_sysfs(struct ssp_data *);

int enable_legacy_sensor(struct ssp_data *data, unsigned int type);
int disable_legacy_sensor(struct ssp_data *data, unsigned int type);
int set_delay_legacy_sensor(struct ssp_data *data, unsigned int type, int sampling_period, int max_report_latency);

#endif

