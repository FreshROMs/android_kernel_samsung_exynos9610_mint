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

#include "ssp_scontext.h"
#include "ssp_cmd_define.h"
#include "ssp_comm.h"
#include "ssp_sysfs.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>

void ssp_scontext_log(const char *func_name,
                              const char *data, int length)
{
	char buf[6];
	char *log_str;
	int log_size;
	int i;

	if (likely(length <= BIG_DATA_SIZE)) {
		log_size = length;
	} else {
		log_size = PRINT_TRUNCATE * 2 + 1;
	}

	log_size = sizeof(buf) * log_size + 1;
	log_str = kzalloc(log_size, GFP_ATOMIC);
	if (unlikely(!log_str)) {
		ssp_errf("allocate memory for data log err");
		return;
	}

	for (i = 0; i < length; i++) {
		if (length < BIG_DATA_SIZE ||
		    i < PRINT_TRUNCATE || i >= length - PRINT_TRUNCATE) {
			snprintf(buf, sizeof(buf), "0x%x", (unsigned char)data[i]);
			strlcat(log_str, buf, log_size);

			if (i < length - 1) {
				strlcat(log_str, ", ", log_size);
			}
		}
		if (length > BIG_DATA_SIZE && i == PRINT_TRUNCATE) {
			strlcat(log_str, "..., ", log_size);
		}
	}

	ssp_info("%s(%d): %s", func_name, length, log_str);
	kfree(log_str);
}

static int ssp_scontext_send_cmd(struct ssp_data *data,
                                  const char *buf, int count)
{
	int ret = 0;

	if (buf[2] < SCONTEXT_AP_STATUS_WAKEUP ||
	    buf[2] > SCONTEXT_AP_STATUS_CALL_ACTIVE) {
		ssp_errf("INST_LIB_NOTI err(%d)", buf[2]);
		return -EINVAL;
	}

	ret = ssp_send_status(data, buf[2]);

	if (buf[2] == SCONTEXT_AP_STATUS_WAKEUP ||
	    buf[2] == SCONTEXT_AP_STATUS_SLEEP) {
		data->last_ap_status = buf[2];
	}

	if (buf[2] == SCONTEXT_AP_STATUS_SUSPEND ||
	    buf[2] == SCONTEXT_AP_STATUS_RESUME) {
		data->last_resume_status = buf[2];
	}

	return ret;
}


#define SCONTEXT_VALUE_CURRENTSYSTEMTIME                      0x0E
#define SCONTEXT_VALUE_PEDOMETER_USERHEIGHT           0x12
#define SCONTEXT_VALUE_PEDOMETER_USERWEIGHT           0x13
#define SCONTEXT_VALUE_PEDOMETER_USERGENDER           0x14
#define SCONTEXT_VALUE_PEDOMETER_INFOUPDATETIME       0x15

int convert_scontext_putvalue_subcmd(int subcmd)
{
	int ret = -1;
	switch (subcmd) {
	case SCONTEXT_VALUE_CURRENTSYSTEMTIME :
		ret = CURRENT_SYSTEM_TIME;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERHEIGHT :
		ret = PEDOMETER_USERHEIGHT;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERWEIGHT:
		ret = PEDOMETER_USERWEIGHT;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERGENDER:
		ret = PEDOMETER_USERGENDER;
		break;
	case SCONTEXT_VALUE_PEDOMETER_INFOUPDATETIME:
		ret = PEDOMETER_INFOUPDATETIME;
		break;
	default:
		ret = subcmd;
	}

	return ret;
}

int convert_scontext_getvalue_subcmd(int subcmd)
{
	int ret = -1;
	switch (subcmd) {
	case SCONTEXT_VALUE_CURRENTSTATUS :
		ret = LIBRARY_CURRENTSTATUS;
		break;
	case SCONTEXT_VALUE_CURRENTSTATUS_BATCH :
		ret = LIBRARY_CURRENTSTATUS_BATCH;
		break;
	case SCONTEXT_VALUE_VERSIONINFO:
		ret = LIBRARY_VERSIONINFO;
		break;
	default:
		ret = subcmd;
	}

	return ret;
}

void get_ss_sensor_name(struct ssp_data *data, int type, char *buf, int buf_size)
{
        memset(buf, 0, buf_size);
        switch (type) {
                case SS_SENSOR_TYPE_PEDOMETER:
                        strncpy(buf, "pedometer", buf_size);
                        break;
                case SS_SENSOR_TYPE_STEP_COUNT_ALERT:
                        strncpy(buf, "step count alert", buf_size);
                        break;
                case SS_SENSOR_TYPE_AUTO_ROTATION:
                        strncpy(buf, "auto rotation", buf_size);
                        break;
                case SS_SENSOR_TYPE_SLOCATION:
                        strncpy(buf, "slocation", buf_size);
                        break;
                case SS_SENSOR_TYPE_MOVEMENT:
                        strncpy(buf, "smart alert", buf_size);
                        break;
                case SS_SENSOR_TYPE_ACTIVITY_TRACKER:
                        strncpy(buf, "activity tracker", buf_size);
                        break;
                case SS_SENSOR_TYPE_DPCM:
                        strncpy(buf, "dpcm", buf_size);
                        break;
                case SS_SENSOR_TYPE_SENSOR_STATUS_CHECK:
                        strncpy(buf, "sensor status check", buf_size);
                        break;
                case SS_SENSOR_TYPE_ACTIVITY_CALIBRATION:
                        strncpy(buf, "activity calibration", buf_size);
                        break;
                case SS_SENSOR_TYPE_DEVICE_POSITION:
                        strncpy(buf, "device position", buf_size);
                        break;
                case SS_SENSOR_TYPE_CHANGE_LOCATION_TRIGGER:
                        strncpy(buf, "change location trigger", buf_size);
                        break;
        }

        return;
}


static int ssp_scontext_send_instruction(struct ssp_data *data,
                                          const char *buf, int count)
{
	char command, type, sub_cmd = 0;
	char *buffer = (char *)(buf + 2);
	int length = count - 2;
	char name[SENSOR_NAME_MAX_LEN] = "";

	if (buf[0] == SCONTEXT_INST_LIBRARY_REMOVE) {
		command = CMD_REMOVE;
		type = buf[1] + SS_SENSOR_TYPE_BASE;
		if(type < SS_SENSOR_TYPE_MAX)
		{
			get_ss_sensor_name(data, type, name, sizeof(name));
			ssp_infof("REMOVE LIB %s, type %d", name, type);

			return disable_sensor(data, type, buffer, length);
		}
		else
			return -EINVAL;
	} else if (buf[0] == SCONTEXT_INST_LIBRARY_ADD) {
		command = CMD_ADD;
		type = buf[1] + SS_SENSOR_TYPE_BASE;
		if(type < SS_SENSOR_TYPE_MAX)
		{
			get_ss_sensor_name(data, type, name, sizeof(name));
			ssp_infof("ADD LIB, type %d", type);

			return enable_sensor(data, type, buffer, length);
		}
		else
			return ERROR;
	} else if (buf[0] == SCONTEXT_INST_LIB_SET_DATA) {
		command = CMD_SETVALUE;
		if (buf[1] != SCONTEXT_VALUE_LIBRARY_DATA) {
			type = TYPE_MCU;
			sub_cmd = convert_scontext_putvalue_subcmd(buf[1]);
		} else {
			type = buf[2] + SS_SENSOR_TYPE_BASE;
			sub_cmd = LIBRARY_DATA;
			length = count - 3;
			if (length > 0) {
				buffer = (char *)(buf + 3);
			} else {
				buffer = NULL;
			}
		}
	} else if (buf[0] == SCONTEXT_INST_LIB_GET_DATA) {
		command = CMD_GETVALUE;
		type = buf[1] + SS_SENSOR_TYPE_BASE;
		sub_cmd =  convert_scontext_getvalue_subcmd(buf[2]);
		length = count - 3;
		if (length > 0) {
			buffer = (char *)(buf + 3);
		} else {
			buffer = NULL;
		}
	} else {
		ssp_errf("0x%x is not supported", buf[0]);
		return ERROR;
	}

	return ssp_send_command(data, command, type, sub_cmd, 0,
	                        buffer, length, NULL, NULL);
}

static ssize_t ssp_scontext_write(struct file *file, const char __user *buf,
                                   size_t count, loff_t *pos)
{
	struct ssp_data *data = container_of(file->private_data, struct ssp_data, scontext_device);
	int ret = 0;
	char *buffer;

	if (!is_sensorhub_working(data)) {
		ssp_errf("stop sending library data(is not working)");
		return -EIO;
	}

	if (unlikely(count < 2)) {
		ssp_errf("library data length err(%d)", (int)count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		ssp_errf("memcpy for kernel buffer err");
		ret = -EFAULT;
		goto exit;
	}

	ssp_scontext_log(__func__, buffer, count);

	if (buffer[0] == SCONTEXT_INST_LIB_NOTI) {
		ret = ssp_scontext_send_cmd(data, buffer, count);
	} else {
		ret = ssp_scontext_send_instruction(data, buffer, count);
	}

	if (unlikely(ret < 0)) {
		ssp_errf("send library data err(%d)", ret);
		if (ret == ERROR) {
			ret = -EIO;
		}
		
		else if (ret == FAIL) {
			ret = -EAGAIN;
		}

		goto exit;
	}

	ret = count;

exit:
	kfree(buffer);
	return ret;
}

static struct file_operations ssp_scontext_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = ssp_scontext_write,
};

int ssp_scontext_initialize(struct ssp_data *data)
{
	int ret;
	ssp_dbgf("----------");
	
	/* register scontext misc device */
	data->scontext_device.minor = MISC_DYNAMIC_MINOR;
	data->scontext_device.name = "ssp_sensorhub";
	data->scontext_device.fops = &ssp_scontext_fops;

	ret = misc_register(&data->scontext_device);
	if (ret < 0) {
		ssp_errf("register scontext misc device err(%d)", ret);
	}

	return ret;
}

void ssp_scontext_remove(struct ssp_data *data)
{
	ssp_scontext_fops.write = NULL;
	misc_deregister(&data->scontext_device);
}

MODULE_DESCRIPTION("Seamless Sensor Platform(SSP) sensorhub driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
