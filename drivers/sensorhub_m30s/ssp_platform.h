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

#ifndef __SSP_PLATFORM_H__
#define __SSP_PLATFORM_H__


#include <linux/kernel.h>

struct ssp_data;

#define SSP_BOOTLOADER_FILE     "sensorhub/shub_bl.fw"

void *ssp_device_probe(struct device *);
void ssp_device_remove(void *ssp_data);
void ssp_device_suspend(void *ssp_data);
void ssp_device_resume(void *ssp_data);

void ssp_platform_init(void *ssp_data, void *);
void ssp_handle_recv_packet(void *ssp_data, char *, int);
void ssp_platform_start_refrsh_task(void *ssp_data);

int sensorhub_reset(void *ssp_data);
int sensorhub_shutdown(void *ssp_data);
int sensorhub_firmware_download(void *ssp_data);

int sensorhub_comms_read(void *ssp_data, u8 *buf, int length, int timeout);
int sensorhub_comms_write(void *ssp_data, u8 *buf, int length, int timeout);

void save_ram_dump(void *ssp_data);
int get_sensorhub_dump_size(void);
void *get_sensorhub_dump_address(void);
void ssp_dump_write_file(void *ssp_data, void *dump_data, int dump_size, int err_type);

bool is_sensorhub_working(void *ssp_data);

int ssp_download_firmware(void *ssp_data, struct device *dev, void *addr);
void ssp_set_firmware_name(void *ssp_data, const char *fw_name);

#endif /* __SSP_PLATFORM_H__ */
