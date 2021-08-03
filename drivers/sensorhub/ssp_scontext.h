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

#ifndef __SSP_SCONTEXT_H__
#define __SSP_SCONTEXT_H__

#include "ssp.h"

#define BIG_DATA_SIZE                   256
#define PRINT_TRUNCATE                  6

#define RESET_REASON_KERNEL_RESET            0x01
#define RESET_REASON_MCU_CRASHED             0x02
#define RESET_REASON_SYSFS_REQUEST           0x03

void ssp_scontext_log(const char *func_name,
                              const char *data, int length);
int ssp_scontext_initialize(struct ssp_data *ssp_data);
void ssp_scontext_remove(struct ssp_data *ssp_data);


void get_ss_sensor_name(struct ssp_data *data, int type, char *buf, int buf_size);
#endif /*__SSP_SCONTEXT_H__*/
