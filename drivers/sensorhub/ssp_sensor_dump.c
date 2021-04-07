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
#include <linux/slab.h>
 
#include "ssp_sensor_dump.h"
#include "ssp_cmd_define.h"
#include "ssp_comm.h"

static bool is_support_registerdump(int sensor_type)
{
	int types[] = SENSOR_DUMP_SENSOR_LIST;
	int list_len = (int)(sizeof(types) / sizeof(types[0]));
	int i, ret = false;
	for (i = 0; i < list_len ; i++) {
		if (types[i] == sensor_type) {
			ret = true;
			break;
		}
	}

	return ret;
}

static int store_sensor_dump(struct ssp_data *data, int sensor_type, u16 length, u8 *buf)
{
#ifdef SENSOR_DUMP_FILE_STORE
	mm_segment_t old_fs;
	struct file *dump_filp = NULL;
	char file_name[SENSOR_DUMP_FILE_LENGTH] = {0,};
#endif

	char *contents;
	int ret = SUCCESS;
	int i = 0;
	int dump_len = sensor_dump_length(length);
	char tmp_ch;

	pr_info("[SSP] %s - type %d, length %d\n", __func__, sensor_type, length);

	/*make file contents*/
	contents = (char *)kzalloc(dump_len, GFP_KERNEL);

	for (i = 0; i < length; i++) {
		tmp_ch = ((i % NUM_LINE_ITEM == NUM_LINE_ITEM - 1) || (i - 1 == length)) ? '\n' : ' ';
		snprintf(contents + i * LENGTH_1BYTE_HEXA_WITH_BLANK, dump_len - i * LENGTH_1BYTE_HEXA_WITH_BLANK,
		         "%02x%c", buf[i], tmp_ch);
	}

	if (data->sensor_dump[sensor_type] != NULL) {
		kfree(data->sensor_dump[sensor_type]);
		data->sensor_dump[sensor_type] = NULL;
	}

	data->sensor_dump[sensor_type] =  contents;

#ifdef SENSOR_DUMP_FILE_STORE
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/*make file name*/
	snprintf(file_name, SENSOR_DUMP_FILE_LENGTH, "%s%d.txt", SENSOR_DUMP_PATH, sensor_type);

	dump_filp = filp_open(file_name,
	                      O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0640);

	if (IS_ERR(dump_filp)) {
		pr_err("[SSP] %s - Can't open dump file %d \n", __func__, sensor_type);
		set_fs(old_fs);
		ret = PTR_ERR(dump_filp);
		return ret;
	}

	ret = vfs_write(dump_filp, data->sensor_dump[sensor_type], dump_len, &dump_filp->f_pos);
	if (ret < 0) {
		pr_err("[SSP] %s - Can't write the dump data to file\n", __func__);
		ret = -EIO;
	}

	filp_close(dump_filp, current->files);
	set_fs(old_fs);
#endif

	return ret;
}


int send_sensor_dump_command(struct ssp_data *data, u8 sensor_type)
{
	int ret = SUCCESS;
	char *buffer = NULL;
	int buffer_length = 0;

	if (sensor_type >= SENSOR_TYPE_MAX) {
		ssp_errf("invalid sensor type %d\n", sensor_type);
		return -EINVAL;
	} else if (!(data->sensor_probe_state & (1ULL << sensor_type))) {
		ssp_errf("%u is not connected(0x%llx)\n", sensor_type, data->sensor_probe_state);
		return -EINVAL;
	}

	if (!is_support_registerdump(sensor_type)) {
		ssp_errf("unsupported sensor type %u\n", sensor_type);
		return -EINVAL;
	}

	ret = ssp_send_command(data, CMD_GETVALUE, sensor_type, SENSOR_REGISTER_DUMP, 1000, NULL, 0,
	                       &buffer, &buffer_length);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		if (buffer != NULL) {
			kfree(buffer);
		}
		return ret;
	}

	if (buffer == NULL) {
		ssp_errf("buffer is null");
		return -EINVAL;
	}

	ssp_infof("(%u)\n", sensor_type);

	ret = store_sensor_dump(data, sensor_type, buffer_length, buffer);

	kfree(buffer);

	return ret;
}

int send_all_sensor_dump_command(struct ssp_data *data)
{
	int types[] = SENSOR_DUMP_SENSOR_LIST;
	int i, ret = SUCCESS;
	for (i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		int temp;
		if ((temp = send_sensor_dump_command(data, types[i])) != SUCCESS) {
			ret = temp;
		}
	}

	return ret;
}
