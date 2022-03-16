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
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "ssp.h"
#include "ssp_platform.h"
#include "ssp_dump.h"
#include "ssp_iio.h"

#define SENSORHUB_DUMP_NOTI_EVENT               0xFC

char* sensorhub_dump;
int sensorhub_dump_size;

void write_ssp_dump_file(struct ssp_data *data, char *dump, int dumpsize, int type, int count)
{
	char buffer[3];

	if (dump == NULL) {
		ssp_errf("dump is NULL");
		return;
	} else if (PTR_ERR_OR_ZERO(sensorhub_dump)) {
		ssp_errf("dump ptr error");
		return;
	} else if (dumpsize != sensorhub_dump_size) {
		ssp_errf("dump size is wrong %d(%d)", dumpsize, sensorhub_dump_size);
		return;
	}
	memcpy_fromio(sensorhub_dump, dump, sensorhub_dump_size);

	buffer[0] = SENSORHUB_DUMP_NOTI_EVENT;
	buffer[1] = type;
	buffer[2] = count;
	report_sensorhub_data(data, buffer);
}

static ssize_t shub_dump_read(struct file *file, struct kobject *kobj,
				  struct bin_attribute *battr, char *buf,
				  loff_t off, size_t size)
{
	memcpy_fromio(buf, battr->private + off, size);
	return size;
}

BIN_ATTR_RO(shub_dump, 0);

struct bin_attribute *ssp_dump_bin_attrs[] = {
	&bin_attr_shub_dump,
};

void initialize_ssp_dump(struct ssp_data *data)
{
	int i, ret;
	ssp_infof();

	sensorhub_dump_size = get_sensorhub_dump_size();
	if(sensorhub_dump_size == 0)
		return;

	sensorhub_dump = (char*)kvzalloc(sensorhub_dump_size, GFP_KERNEL);
	if(PTR_ERR_OR_ZERO(sensorhub_dump)) {
		ssp_infof("memory alloc failed");
		return;
	}

	ssp_infof("dump size %d", sensorhub_dump_size);

	bin_attr_shub_dump.size = sensorhub_dump_size;
	bin_attr_shub_dump.private = sensorhub_dump;

	for (i = 0; i < ARRAY_SIZE(ssp_dump_bin_attrs); i++) {
		struct bin_attribute *battr = ssp_dump_bin_attrs[i];

		ret = device_create_bin_file(data->mcu_device, battr);
		if (ret < 0) {
			ssp_errf("Failed to create file: %s %d", battr->attr.name, ret);
			break;
		}
	}

	if(ret < 0) {
		kvfree(sensorhub_dump);
		sensorhub_dump_size = 0;
	}
}

void remove_ssp_dump(struct ssp_data *data)
{
	int i;
	if(!PTR_ERR_OR_ZERO(sensorhub_dump)) {
		kvfree(sensorhub_dump);

		for (i = 0; ARRAY_SIZE(ssp_dump_bin_attrs); i++) {
			device_remove_bin_file(data->mcu_device, ssp_dump_bin_attrs[i]);
		}
	}

}
