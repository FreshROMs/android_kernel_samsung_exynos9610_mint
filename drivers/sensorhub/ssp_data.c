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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "ssp_data.h"
#include "ssp_debug.h"
#include "ssp_iio.h"
#include "ssp_type_define.h"
#include "ssp_cmd_define.h"
#include "ssp_comm.h"
#include "ssp_sysfs.h"

#define SSP2AP_BYPASS_DATA	 0x37
#define SSP2AP_LIBRARY_DATA	0x01
#define SSP2AP_DEBUG_DATA	  0x03
#define SSP2AP_BIG_DATA	    0x04
#define SSP2AP_META_DATA	   0x05
#define SSP2AP_TIME_SYNC	   0x06
#define SSP2AP_NOTI_RESET	  0x07
#define SSP2AP_GYRO_CAL	    0x08
#define SSP2AP_PROX_THRESH	 0x09
#define SSP2AP_REQ_RESET	       0x0A
#define SSP2AP_MAG_CAL	     0x0B
#define SSP2AP_DUMP_DATA	   0xDD
#define SSP2AP_CALLSTACK	   0x0F
#define SSP2AP_SYSTEM_INFO	 0x31
#define SSP2AP_SENSOR_SPEC	 0x41

typedef enum _hub_req_reset_type {
	HUB_RESET_REQ_NO_EVENT = 0x1a,
} hub_req_reset_type;


#define U64_US2NS 1000ULL

#define SSP_TIMESTAMP_SYNC_TIMER_SEC     (30 * HZ)

/* fw */
#define SSP_INVALID_REVISION	    99999
#define SSP_INVALID_REVISION2	   0xFFFFFF

u64 get_current_timestamp(void)
{
	u64 timestamp;
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());
	timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

	return timestamp;
}

void get_timestamp(struct ssp_data *data, char *dataframe,
		   int *ptr_data, struct sensor_value *event, int type)
{
	u64 timestamp_ns = 0;
	u64 current_timestamp = get_current_timestamp();
	memset(&timestamp_ns, 0, 8);
	memcpy(&timestamp_ns, dataframe + *ptr_data, 8);

	if (timestamp_ns > current_timestamp) {
		//ssp_infof("future timestamp(%d) : last = %lld, cur = %lld", type, data->latest_timestamp[type], current_timestamp);
		timestamp_ns = current_timestamp;
	}
	event->timestamp = timestamp_ns;
	data->buf[type].timestamp = event->timestamp;
	data->latest_timestamp[type] = current_timestamp;

	*ptr_data += 8;
}

void get_sensordata(struct ssp_data *data, char *dataframe,
		    int *ptr_data, int type, struct sensor_value *event)
{
	memcpy(event, dataframe + *ptr_data, data->info[type].get_data_len);
	*ptr_data += data->info[type].get_data_len;
	memcpy(&data->buf[type], (char *)event, data->info[type].get_data_len);
}

void show_system_info(char *dataframe, int *idx)
{
	typedef struct _sensor_debug_info {
		uint8_t uid;
		uint8_t total_count;
		uint8_t ext_client;
		int32_t ext_sampling;
		int32_t ext_report;
		int32_t fastest_sampling;
	} sensor_debug_info;

	typedef struct _base_timestamp {
		uint64_t kernel_base;
		uint64_t hub_base;
	} base_timestamp;

	typedef struct _utc_time {
		int8_t nHour;
		int8_t nMinute;
		int8_t nSecond;
		int16_t nMilliSecond;
	} utc_time;

	typedef struct _system_debug_info {
		int32_t version;
		int32_t rate;
		int8_t ap_state;
		utc_time time;
		base_timestamp timestamp;
	} system_debug_info;
	//===================================================//
	sensor_debug_info *info = 0;
	system_debug_info *s_info = 0;
	int i;
	int count = *dataframe;

	++dataframe;
	*idx += (1 + sizeof(sensor_debug_info) * count + sizeof(system_debug_info));

	ssp_info("==system info ===");
	for (i = 0; i < count; ++i) {
		info = (sensor_debug_info *)dataframe;
		ssp_info("id(%d), total(%d), external(%d), e_sampling(%d), e_report(%d), fastest(%d)",
			 info->uid, info->total_count, info->ext_client, info->ext_sampling, info->ext_report, info->fastest_sampling);
		dataframe += sizeof(sensor_debug_info);
	}

	s_info = (system_debug_info *)dataframe;
	ssp_info("version(%d), rate(%d), ap_state(%s), time(%d:%d:%d.%d), base_ts_k(%lld), base_ts_hub(%lld)",
		 s_info->version, s_info->rate, s_info->ap_state == 0 ? "run" : "suspend", s_info->time.nHour, s_info->time.nMinute, s_info->time.nSecond, s_info->time.nMilliSecond, s_info->timestamp.kernel_base, s_info->timestamp.hub_base);
}

void handle_sensor_spec(struct ssp_data *data, char *dataframe, int *idx)
{
	struct sensor_spec_t *spec = 0;
	int i = 0, prev_size = data->sensor_spec_size;
	int count = *dataframe;
	int size = (sizeof(struct sensor_spec_t) * count);
	char *prev_sensor_spec = data->sensor_spec;
	++dataframe;
	*idx += (size + 1);
	if (size == 0)
		return;

	data->sensor_spec = kzalloc(size + prev_size, GFP_KERNEL);
	if (prev_sensor_spec != NULL) { // prev_size != 0
		memcpy(data->sensor_spec, prev_sensor_spec, prev_size);
		kfree(prev_sensor_spec);
	}
	memcpy(data->sensor_spec + prev_size, dataframe, size);
	data->sensor_spec_size = size + prev_size;

	for (i = 0; i < count; ++i) {
		spec = (struct sensor_spec_t *)dataframe;
		dataframe += sizeof(struct sensor_spec_t);

		ssp_info("id(%d), name(%s), vendor(%d)", spec->uid, spec->name, spec->vendor);
		if (spec->uid == SENSOR_TYPE_ACCELEROMETER) {
#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
			select_accel_ops(data, spec->name);
#endif
		}
#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
		else if (spec->uid == SENSOR_TYPE_GYROSCOPE)
			select_gyro_ops(data, spec->name);
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC
		else if (spec->uid == SENSOR_TYPE_GEOMAGNETIC_FIELD)
			select_magnetic_ops(data, spec->name);
#endif
#ifdef CONFIG_SENSORS_SSP_BAROMETER
		else if (spec->uid == SENSOR_TYPE_PRESSURE)
			select_barometer_ops(data, spec->name);
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT
		else if (spec->uid == SENSOR_TYPE_LIGHT)
			select_light_ops(data, spec->name);
#endif
#ifdef CONFIG_SENSORS_SSP_PROXIMITY
		else if (spec->uid == SENSOR_TYPE_PROXIMITY)
			select_prox_ops(data, spec->name);
#endif
	}
}

int parse_dataframe(struct ssp_data *data, char *dataframe, int frame_len)
{
	int type, index;
	struct sensor_value event;
	u16 batch_event_count;
	bool parsing_error = false;
	u16 length = 0;

	if (!is_sensorhub_working(data)) {
		ssp_infof("ssp shutdown, do not parse");
		return SUCCESS;
	}

	//print_dataframe(data, dataframe, frame_len);

	memset(&event, 0, sizeof(event));

	for (index = 0; index < frame_len && !parsing_error;) {
		switch (dataframe[index++]) {
		case SSP2AP_DEBUG_DATA:
			type = print_mcu_debug(dataframe, &index, frame_len);
			if (type) {
				ssp_errf("Mcu debug dataframe err %d", type);
				parsing_error = true;
			}
			break;
		case SSP2AP_BYPASS_DATA:
			type = dataframe[index++];
			if ((type < 0) || (type >= SENSOR_TYPE_MAX)) {
				ssp_errf("Mcu bypass dataframe err %d", type);
				parsing_error = true;
				break;
			}

			memcpy(&length, dataframe + index, 2);
			index += 2;
			batch_event_count = length;

			do {
				get_sensordata(data, dataframe, &index, type, &event);
				get_timestamp(data, dataframe, &index, &event, type);
				report_sensor_data(data, type, &event);

				batch_event_count--;
			} while ((batch_event_count > 0) && (index < frame_len));

			if (batch_event_count > 0)
				ssp_errf("batch count error (%d)", batch_event_count);
			break;
		case SSP2AP_LIBRARY_DATA:
			memcpy(&length, dataframe + index, 2);
			index += 2;
			report_scontext_data(data, dataframe + index, length);
			index += length;
			break;

		case SSP2AP_META_DATA:
			event.meta_data.what = dataframe[index++];
			event.meta_data.sensor = dataframe[index++];
			if ((event.meta_data.sensor < 0) || (event.meta_data.sensor >= SENSOR_TYPE_MAX)) {
				ssp_errf("mcu meta data sensor dataframe err %d", event.meta_data.sensor);
				parsing_error = true;
				break;
			}

			report_meta_data(data, &event);
			break;

		case SSP2AP_NOTI_RESET:
			ssp_infof("Reset MSG received from MCU");
			if (data->is_probe_done == true) {
				//data->sensor_probe_state = 0;
				//queue_refresh_task(data, 0);
			} else
				ssp_infof("skip reset msg");
			break;
		case SSP2AP_REQ_RESET: {
			int reset_type = dataframe[index++];
			//if (reset_type == HUB_RESET_REQ_NO_EVENT) {
			ssp_infof("Hub request reset[0x%x] No Event type %d", reset_type, dataframe[index++]);
			reset_mcu(data, RESET_TYPE_HUB_NO_EVENT);
			//}
		}
		break;
#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
		case SSP2AP_GYRO_CAL: {
			s16 caldata[3] = {0, };

			ssp_infof("Gyro caldata received from MCU\n");
			memcpy(caldata, dataframe + index, sizeof(caldata));

			wake_lock(&data->ssp_wake_lock);
			save_gyro_cal_data(data, caldata);
			wake_unlock(&data->ssp_wake_lock);
			index += sizeof(caldata);
		}
		break;
#endif // CONFIG_SENSORS_SSP_GYROSCOPE
#ifdef CONFIG_SENSORS_SSP_MAGNETIC
		case SSP2AP_MAG_CAL: {
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_MMC5603
			u8 caldata[16] = {0,};
#else
			u8 caldata[13] = {0,};
#endif

			ssp_infof("Mag caldata received from MCU(%d)\n", sizeof(caldata));
			memcpy(caldata, dataframe + index, sizeof(caldata));

			wake_lock(&data->ssp_wake_lock);
			save_mag_cal_data(data, caldata);
			wake_unlock(&data->ssp_wake_lock);
			index += sizeof(caldata);
		}
		break;
#endif
#ifdef CONFIG_SENSORS_SSP_PROXIMITY
#ifdef CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL
		case SSP2AP_PROX_THRESH: {
			u16 thresh[2] = {0, };
			memcpy(thresh, dataframe + index, sizeof(thresh));
			data->prox_thresh[0] = thresh[0];
			data->prox_thresh[1] = thresh[1];
			index += sizeof(thresh);
			ssp_infof("prox thresh received %u %u", data->prox_thresh[0], data->prox_thresh[1]);
		}
		break;
#endif // CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL
#endif // CONFIG_SENSORS_SSP_PROXIMITY
		case SSP2AP_SYSTEM_INFO:
			show_system_info(dataframe + index, &index);
			break;
		case SSP2AP_SENSOR_SPEC:
			handle_sensor_spec(data, dataframe + index, &index);
			break;
		default :
			ssp_errf("0x%x cmd doesn't support", dataframe[index - 1]);
			parsing_error = true;
			break;
		}
	}

	if (parsing_error) {
		print_dataframe(data, dataframe, frame_len);
		return ERROR;
	}

	return 0;
}

int get_sensor_scanning_info(struct ssp_data *data)
{
	int ret = 0, z = 0;
	uint64_t result[3] = {0,};
	char *buffer = NULL;
	unsigned int buffer_length;

	char sensor_scanning_state[SENSOR_TYPE_MAX + 1];

	ret = ssp_send_command(data, CMD_GETVALUE, TYPE_MCU, SENSOR_SCAN_RESULT, 1000,
			       NULL, 0, &buffer, &buffer_length);
	if (ret < 0)
		ssp_errf("MSG2SSP_AP_SENSOR_SCANNING fail %d", ret);
	else
		memcpy(&result[0], buffer, buffer_length > sizeof(result) ? sizeof(result) : buffer_length);

	data->sensor_probe_state = result[0];
	memcpy(&data->ss_sensor_probe_state, &result[1], sizeof(data->ss_sensor_probe_state));

	sensor_scanning_state[SENSOR_TYPE_MAX] = '\0';
	for (z = 0; z < SENSOR_TYPE_MAX; z++) {
		if (!(result[0] & (1ULL << z)) && z != SENSOR_TYPE_SCONTEXT && z != SENSOR_TYPE_SENSORHUB)
			data->info[z].enable = false;
		sensor_scanning_state[SENSOR_TYPE_MAX - 1 - z] = (result[0] & (1ULL << z)) ? '1' : '0';
	}

	ssp_info("state(0x%llx): %s", data->sensor_probe_state, sensor_scanning_state);
	ssp_info("probe state 0x%llx, 0x%llx, 0x%llx", result[0], result[1], result[2]);

	if (buffer != NULL)
		kfree(buffer);

	return ret;
}

int get_firmware_rev(struct ssp_data *data)
{
	int ret;
	u32 result = SSP_INVALID_REVISION;
	char *buffer = NULL;
	int buffer_length;

	ret = ssp_send_command(data, CMD_GETVALUE, TYPE_MCU, VERSION_INFO, 1000, NULL,
			       0, &buffer, &buffer_length);

	if (ret != SUCCESS)
		ssp_errf("transfer fail %d", ret);
	else if (buffer_length != sizeof(result))
		ssp_errf("VERSION_INFO length is wrong");
	else
		memcpy(&result, buffer, buffer_length);

	if (buffer != NULL)
		kfree(buffer);

	data->curr_fw_rev = result;
	ssp_info("MCU Firm Rev : New = %8u", data->curr_fw_rev);

	return ret;
}

int set_sensor_position(struct ssp_data *data)
{
	int ret[3] = {0,};
	ssp_infof();

#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
	ret[0] = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_ACCELEROMETER,
				  SENSOR_AXIS, 0, (char *)&(data->accel_position), sizeof(data->accel_position),
				  NULL, NULL);
	ssp_infof("A : %u", data->accel_position);
#endif

#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
	ret[1] = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_GYROSCOPE,
				  SENSOR_AXIS, 0, (char *)&(data->gyro_position), sizeof(data->gyro_position),
				  NULL, NULL);

	ssp_infof("G : %u", data->gyro_position);
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC
	ret[2] = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD,
				  SENSOR_AXIS, 0, (char *)&(data->mag_position), sizeof(data->mag_position),
				  NULL, NULL);

	ssp_infof("M : %u", data->mag_position);
#endif

	if ((ret[0] & ret[1] & ret[2]) != 0) {
		ssp_errf("fail to set_sensor_position %d %d %d", ret[0], ret[1], ret[2]);
		return ERROR;
	}

	return 0;
}

#ifdef CONFIG_SENSORS_SSP_PROXIMITY
#ifdef CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL
void set_proximity_threshold_addval(struct ssp_data *data)
{
	int ret = 0;
	u16 prox_th_addval[PROX_THRESH_SIZE + 1] = {0, };

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_PROXIMITY))) {
		ssp_infof("Skip this function!, proximity sensor is not connected(0x%llx)",
			  data->sensor_probe_state);
		return;
	}

	memcpy(prox_th_addval, data->prox_thresh_addval, sizeof(prox_th_addval));

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_PROXIMITY,
			       PROXIMITY_ADDVAL, 0, (char *)prox_th_addval, sizeof(prox_th_addval), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_err("SENSOR_PROXTHRESHOLD CMD fail %d", ret);
		return;
	}

	ssp_info("Proximity Threshold Additional Value(%d) - %u, %u", sizeof(prox_th_addval), data->prox_thresh_addval[PROX_THRESH_HIGH],
		 data->prox_thresh_addval[PROX_THRESH_LOW]);

}
#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
#define PROX_SETTINGS_FILE_PATH	 "/efs/FactoryApp/prox_settings"
int save_proximity_setting_mode(struct ssp_data *data)
{
	struct file *filp = NULL;
	mm_segment_t old_fs;
	int ret = -1;
	char buf[3] = "";
	int buf_len = 0;

	buf_len = snprintf(buf, PAGE_SIZE, "%d", data->prox_setting_mode);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(PROX_SETTINGS_FILE_PATH,
			 O_CREAT | O_TRUNC | O_RDWR | O_SYNC, 0666);

	if (filp == NULL) {
		ssp_infof("filp is NULL");
		return ret;
	}

	if (IS_ERR(filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(filp);
		ssp_errf("Can't open prox settings file (%d)", ret);
		return ret;
	}

	ret = vfs_write(filp, buf, buf_len, &filp->f_pos);
	if (ret != buf_len) {
		ssp_errf("Can't write the prox settings data to file, ret=%d", ret);
		ret = -EIO;
	}

	filp_close(filp, current->files);
	set_fs(old_fs);

	msleep(150);

	return ret;
}

int open_proximity_setting_mode(struct ssp_data *data)
{
	struct file *filp = NULL;
	mm_segment_t old_fs;
	int ret = -1;
	char buf[3] = "";

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(PROX_SETTINGS_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(filp);
		ssp_errf("Can't open prox settings file (%d)", ret);
		return ret;
	}

	ret = vfs_read(filp, buf, sizeof(buf), &filp->f_pos);
	ssp_infof("buf=%s", buf);

	if (ret <= 0) {
		ssp_errf("Can't read the prox settings data from file, bytes=%d", ret);
		ret = -EIO;
	} else {
		sscanf(buf, "%d", &data->prox_setting_mode);
		ssp_infof("prox_settings %d", data->prox_setting_mode);
		if (data->prox_setting_mode != 1 && data->prox_setting_mode != 2) {
			data->prox_setting_mode = 1;
			ssp_errf("leg_reg_val is wrong. set defulat setting");
		}
	}

	filp_close(filp, current->files);
	set_fs(old_fs);

	if (data->prox_setting_mode != 1)
		memcpy(data->prox_thresh, data->prox_mode_thresh, sizeof(data->prox_thresh));

	return ret;
}

void set_proximity_setting_mode(struct ssp_data *data)
{
	int ret = 0;
	u8 mode = data->prox_setting_mode;

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_PROXIMITY))) {
		ssp_infof("Skip this function!, proximity sensor is not connected(0x%llx)",
			  data->sensor_probe_state);
		return;
	}

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_PROXIMITY,
			       PROXIMITY_SETTING_MODE, 0, (char *)&mode, sizeof(mode), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_err("PROXIMITY_SETTING_MODE CMD fail %d", ret);
		return;
	}

	ssp_infof("%d", mode);

}
#endif

void proximity_calibration_off(struct ssp_data *data)
{
	ssp_infof("");

	disable_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_CALIBRATION);
}

void do_proximity_calibration(struct ssp_data *data)
{
	ssp_infof("");

	set_delay_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_CALIBRATION, 10, 0);
	enable_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_CALIBRATION);
}

void set_proximity_threshold(struct ssp_data *data)
{
	int ret = 0;
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
	char prox_th[PROX_THRESH_SIZE] = {0, };
#else
	u16 prox_th[PROX_THRESH_SIZE] = {0, };
#endif

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_PROXIMITY))) {
		ssp_infof("Skip this function!, proximity sensor is not connected(0x%llx)",
			  data->sensor_probe_state);
		return;
	}

	memcpy(prox_th, data->prox_thresh, sizeof(prox_th));

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_PROXIMITY,
			       PROXIMITY_THRESHOLD, 0, (char *)prox_th, sizeof(prox_th), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_err("SENSOR_PROXTHRESHOLD CMD fail %d", ret);
		return;
	}
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
	ssp_info("Proximity Threshold - %u, %u, %u, %u", data->prox_thresh[PROX_THRESH_HIGH],
		 data->prox_thresh[PROX_THRESH_LOW],
		 data->prox_thresh[PROX_THRESH_DETECT_HIGH], data->prox_thresh[PROX_THRESH_DETECT_LOW]);
#else
	ssp_info("Proximity Threshold - %u, %u", data->prox_thresh[PROX_THRESH_HIGH],
		 data->prox_thresh[PROX_THRESH_LOW]);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_LIGHT
void set_light_coef(struct ssp_data *data)
{
	int ret = 0;

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_LIGHT))) {
		pr_info("[SSP]: %s - Skip this function!!!,"\
			"light sensor is not connected(0x%llx)\n",
			__func__, data->sensor_probe_state);
		return;
	}

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_COEF, 0,
			       (char *)data->light_coef, sizeof(data->light_coef), NULL, NULL);

	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - MSG2SSP_AP_SET_LIGHT_COEF CMD fail %d\n",
		       __func__, ret);
		return;
	}

	pr_info("[SSP]: %s - %d %d %d %d %d %d %d\n", __func__,
		data->light_coef[0], data->light_coef[1], data->light_coef[2],
		data->light_coef[3], data->light_coef[4], data->light_coef[5],
		data->light_coef[6]);
}

void set_light_brightness(struct ssp_data *data)
{
	int ret = 0;
	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_LIGHT))) {
		ssp_infof("Skip this function!!!,"\
			  "light sensor is not connected(0x%llx)\n",
			  data->sensor_probe_state);
		return;
	}

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_BRIGHTNESS, 0,
			       (char *)&data->brightness, sizeof(data->brightness), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_errf("CMD fail %d\n", ret);
		return;
	}

	ssp_infof("%d \n", data->brightness);
}

void set_light_ab_camera_hysteresis(struct ssp_data *data)
{
	int ret = 0;
	int buf[4];
	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS))) {
		ssp_infof("Skip this function!!!,"\
			  "light sensor is not connected(0x%llx)\n",
			  data->sensor_probe_state);
		return;
	}

	memcpy(buf, data->camera_lux_hysteresis, sizeof(data->camera_lux_hysteresis));
	memcpy(&buf[2], data->camera_br_hysteresis, sizeof(data->camera_br_hysteresis));

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS, LIGHT_AB_HYSTERESIS, 0,
			       (char *)buf, sizeof(buf), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_errf("CMD fail %d\n", ret);
		return;
	}

	ssp_infof("%d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);
}

#endif

#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
#define GYRO_CALIBRATION_FILE_PATH	   "/efs/FactoryApp/gyro_cal_data"

int gyro_open_calibration(struct ssp_data *data)
{
	int ret = 0;
#ifdef CONFIG_SENSORS_SSP_GYROFILE_FOR_MAG
	ssp_infof("gyrofile for mag");
#else
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(GYRO_CALIBRATION_FILE_PATH,
			     O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);

		data->gyrocal.x = 0;
		data->gyrocal.y = 0;
		data->gyrocal.z = 0;

		pr_err("[SSP]: %s - Can't open calibration file %d\n", __func__, ret);
		return ret;
	}

	ret = vfs_read(cal_filp, (char *)&data->gyrocal, sizeof(data->gyrocal), &cal_filp->f_pos);
	if (ret != sizeof(data->gyrocal))
		ret = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	ssp_info("open gyro calibration %d, %d, %d",
		 data->gyrocal.x, data->gyrocal.y, data->gyrocal.z);
#endif
	return ret;
}

int save_gyro_cal_data(struct ssp_data *data, s16 *cal_data)
{
	int ret = 0;
#ifdef CONFIG_SENSORS_SSP_GYROFILE_FOR_MAG
	ssp_infof("gyrofile for mag");
#else
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	data->gyrocal.x = cal_data[0];
	data->gyrocal.y = cal_data[1];
	data->gyrocal.z = cal_data[2];

	ssp_info("do gyro calibrate %d, %d, %d",
		 data->gyrocal.x, data->gyrocal.y, data->gyrocal.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(GYRO_CALIBRATION_FILE_PATH,
			     O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW |
			     O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open calibration file\n", __func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return -EIO;
	}

	ret = vfs_write(cal_filp, (char *)&data->gyrocal, sizeof(data->gyrocal), &cal_filp->f_pos);
	if (ret != sizeof(data->gyrocal)) {
		pr_err("[SSP]: %s - Can't write gyro cal to file\n", __func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);
#endif

	return ret;
}

int set_gyro_cal(struct ssp_data *data)
{
	int ret = 0;
#ifdef CONFIG_SENSORS_SSP_GYROFILE_FOR_MAG
	ssp_infof("gyrofile for mag");
#else
	s16 gyro_cal[3] = {0, };

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_GYROSCOPE))) {
		pr_info("[SSP]: %s - Skip this function!!!"\
			", gyro sensor is not connected(0x%llx)\n",
			__func__, data->sensor_probe_state);
		return ret;
	}

	gyro_cal[0] = data->gyrocal.x;
	gyro_cal[1] = data->gyrocal.y;
	gyro_cal[2] = data->gyrocal.z;

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_GYROSCOPE, CAL_DATA, 0,
			       (char *)gyro_cal, 6 * sizeof(char), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		goto exit;
	}

	pr_info("[SSP] Set temp gyro cal data %d, %d, %d\n",
		gyro_cal[0], gyro_cal[1], gyro_cal[2]);

	pr_info("[SSP] Set gyro cal data %d, %d, %d\n",
		data->gyrocal.x, data->gyrocal.y, data->gyrocal.z);
exit:
#endif
	return ret;
}
#endif

#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
#define ACCEL_CALIBRATION_FILE_PATH   "/efs/FactoryApp/calibration_data"
int accel_open_calibration(struct ssp_data *data)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(ACCEL_CALIBRATION_FILE_PATH, O_RDONLY, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);

		data->accelcal.x = 0;
		data->accelcal.y = 0;
		data->accelcal.z = 0;

		return ret;
	}

	ret = vfs_read(cal_filp, (char *)&data->accelcal, 3 * sizeof(int), &cal_filp->f_pos);
	if (ret != 3 * sizeof(int))
		ret = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	ssp_infof("open accel calibration %d, %d, %d\n",
		  data->accelcal.x, data->accelcal.y, data->accelcal.z);

	if ((data->accelcal.x == 0) && (data->accelcal.y == 0)
	    && (data->accelcal.z == 0))
		return ERROR;

	return ret;
}

int set_accel_cal(struct ssp_data *data)
{
	int ret = 0;
	s16 accel_cal[3] = {0, };

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_ACCELEROMETER))) {
		pr_info("[SSP]: %s - Skip this function!!!"\
			", accel sensor is not connected(0x%llx)\n",
			__func__, data->sensor_probe_state);
		return ret;
	}

	accel_cal[0] = data->accelcal.x;
	accel_cal[1] = data->accelcal.y;
	accel_cal[2] = data->accelcal.z;

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_ACCELEROMETER, CAL_DATA,
			       0, (char *)accel_cal, 6 * sizeof(char), NULL, NULL);

	if (ret != SUCCESS)
		ssp_errf("ssp_send_command Fail %d", ret);
	pr_info("[SSP] Set accel cal data %d, %d, %d\n",
		data->accelcal.x, data->accelcal.y, data->accelcal.z);

	return ret;
}


#define ORIENTATION_CMD_MODE	128
int set_device_orientation_mode(struct ssp_data *data)
{
	int ret = 0;

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_DEVICE_ORIENTATION))) {
		ssp_infof("sensor is not connected(0x%llx)", data->sensor_probe_state);
		return ret;
	}

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_DEVICE_ORIENTATION, ORIENTATION_CMD_MODE, 0,
				&data->orientation_mode, sizeof(data->orientation_mode), NULL, NULL);
	if (ret < 0) {
		ssp_errf("CMD fail %d", ret);
		return ret;
	}

	ssp_infof("%d", data->orientation_mode);

	return ret;
}
#endif

#ifdef CONFIG_SENSORS_SSP_BAROMETER
#define BARO_CALIBRATION_FILE_PATH	   "/efs/FactoryApp/baro_delta"

int pressure_open_calibration(struct ssp_data *data)
{
	char chBuf[10] = {0,};
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(BARO_CALIBRATION_FILE_PATH, O_RDONLY, 0660);
	if (IS_ERR(cal_filp)) {
		ret = PTR_ERR(cal_filp);
		if (ret != -ENOENT)
			pr_err("[SSP]: %s - Can't open calibration file(%d)\n",
			       __func__, ret);
		set_fs(old_fs);
		return ret;
	}
	ret = vfs_read(cal_filp, chBuf, 10 * sizeof(char), &cal_filp->f_pos);
	if (ret < 0) {
		pr_err("[SSP]: %s - Can't read the cal data from file (%d)\n",
		       __func__, ret);
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
		return ret;
	}
	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	ret = kstrtoint(chBuf, 10, &data->buf[SENSOR_TYPE_PRESSURE].pressure_cal);
	if (ret < 0) {
		pr_err("[SSP]: %s - kstrtoint failed. %d", __func__, ret);
		return ret;
	}

	ssp_info("open barometer calibration %d",
		 data->buf[SENSOR_TYPE_PRESSURE].pressure_cal);

	return ret;
}
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC
#ifdef CONFIG_SENSORS_SSP_GYROFILE_FOR_MAG
#define MAG_CALIBRATION_FILE_PATH	   "/efs/FactoryApp/gyro_cal_data"
#else
#define MAG_CALIBRATION_FILE_PATH	   "/efs/FactoryApp/mag_cal_data"
#endif
int set_pdc_matrix(struct ssp_data *data)
{
	int ret = 0;

	if (!(data->sensor_probe_state & 1ULL << SENSOR_TYPE_GEOMAGNETIC_FIELD)) {
		pr_info("[SSP] %s - Skip this function!!!"\
			", magnetic sensor is not connected(0x%llx)\n",
			__func__, data->sensor_probe_state);
		return ret;
	}

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD,
			       MAGNETIC_STATIC_MATRIX, 0, &data->pdc_matrix[0], sizeof(data->pdc_matrix), NULL,
			       NULL);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		goto exit;
	}

	pr_info("[SSP] %s: finished\n", __func__);

exit:
	return ret;
}

#ifdef CONFIG_SENSORS_SSP_MAGNETIC_MMC5603
int mag_open_calibration(struct ssp_data *data)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	char buffer[16] = {0,};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(MAG_CALIBRATION_FILE_PATH,
			     O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		memset(&data->magcal, 0, sizeof(data->magcal));

		pr_err("[SSP]: %s - Can't open calibration file %d\n", __func__, ret);
		return ret;
	}

	ret = vfs_read(cal_filp, buffer, sizeof(buffer), &cal_filp->f_pos);
	if ((ret != sizeof(buffer))) {
		ret = -EIO;
		memset(&data->magcal, 0, sizeof(data->magcal));
	}

	memcpy(&data->magcal, buffer, sizeof(buffer));

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	ssp_infof("%d, %d, %d, %d",
		  data->magcal.offset_x, data->magcal.offset_y, data->magcal.offset_z,
		  data->magcal.radius);
	return ret;
}

int save_mag_cal_data(struct ssp_data *data, u8 *cal_data)
{
	int ret = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	char buffer[16] = {0,};


	memcpy(&data->magcal, cal_data, sizeof(buffer));

	ssp_infof("%d, %d, %d, %d",
		  data->magcal.offset_x, data->magcal.offset_y, data->magcal.offset_z,
		  data->magcal.radius);

	memcpy(buffer, cal_data, sizeof(buffer));

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(MAG_CALIBRATION_FILE_PATH,
			     O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW |
			     O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open calibration file\n", __func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return -EIO;
	}

	ret = vfs_write(cal_filp, buffer, sizeof(buffer), &cal_filp->f_pos);
	if (ret != sizeof(buffer)) {
		pr_err("[SSP]: %s - Can't write mag cal to file\n", __func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int set_mag_cal(struct ssp_data *data)
{
	int ret = 0;
	char buffer[16];

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_GEOMAGNETIC_FIELD))) {
		pr_info("[SSP]: %s - Skip this function!!!"\
			", mag sensor is not connected(0x%llx)\n",
			__func__, data->sensor_probe_state);
		return ret;
	}

	memcpy(buffer, &data->magcal, sizeof(buffer));

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, CAL_DATA, 0,
			       (char *)&buffer, sizeof(buffer), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		goto exit;
	}

	ssp_infof("%d, %d, %d, %d",
		  data->magcal.offset_x, data->magcal.offset_y, data->magcal.offset_z,
		  data->magcal.radius);

exit:
	return ret;
}
#else

int mag_open_calibration(struct ssp_data *data)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	char buffer[14] = {0,};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(MAG_CALIBRATION_FILE_PATH,
			     O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		memset(&data->magcal, 0, sizeof(data->magcal));

		pr_err("[SSP]: %s - Can't open calibration file %d\n", __func__, ret);
		return ret;
	}

	ret = vfs_read(cal_filp, buffer, sizeof(buffer), &cal_filp->f_pos);
	if ((ret != sizeof(buffer))) {
		ret = -EIO;
		memset(&data->magcal, 0, sizeof(data->magcal));
	}

	if (buffer[0] == 'M') {
		data->magcal.accuracy = buffer[1];
		memcpy(&data->magcal.offset_x, (s16 *)&buffer[2], sizeof(s16));
		memcpy(&data->magcal.offset_y, (s16 *)&buffer[4], sizeof(s16));
		memcpy(&data->magcal.offset_z, (s16 *)&buffer[6], sizeof(s16));
		memcpy(&data->magcal.flucv_x, (s16 *)&buffer[8], sizeof(s16));
		memcpy(&data->magcal.flucv_y, (s16 *)&buffer[10], sizeof(s16));
		memcpy(&data->magcal.flucv_z, (s16 *)&buffer[12], sizeof(s16));
	} else
		memset(&data->magcal, 0, sizeof(data->magcal));

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	ssp_infof("%u, %d, %d, %d, %d, %d, %d",
		  data->magcal.accuracy, data->magcal.offset_x, data->magcal.offset_y, data->magcal.offset_z,
		  data->magcal.flucv_x, data->magcal.flucv_y, data->magcal.flucv_z);
	return ret;
}

int save_mag_cal_data(struct ssp_data *data, u8 *cal_data)
{
	int ret = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	char buffer[14] = {0,};

	data->magcal.accuracy = cal_data[0];
	memcpy(&data->magcal.offset_x, (s16 *)&cal_data[1], sizeof(s16));
	memcpy(&data->magcal.offset_y, (s16 *)&cal_data[3], sizeof(s16));
	memcpy(&data->magcal.offset_z, (s16 *)&cal_data[5], sizeof(s16));
	memcpy(&data->magcal.flucv_x, (s16 *)&cal_data[7], sizeof(s16));
	memcpy(&data->magcal.flucv_y, (s16 *)&cal_data[9], sizeof(s16));
	memcpy(&data->magcal.flucv_z, (s16 *)&cal_data[11], sizeof(s16));

	ssp_infof("%u, %d, %d, %d, %d, %d, %d",
		  data->magcal.accuracy, data->magcal.offset_x, data->magcal.offset_y, data->magcal.offset_z,
		  data->magcal.flucv_x, data->magcal.flucv_y, data->magcal.flucv_z);

	buffer[0] = 'M';
	memcpy(&buffer[1], cal_data, 13);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(MAG_CALIBRATION_FILE_PATH,
			     O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW |
			     O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open calibration file\n", __func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return -EIO;
	}

	ret = vfs_write(cal_filp, buffer, sizeof(buffer), &cal_filp->f_pos);
	if (ret != sizeof(buffer)) {
		pr_err("[SSP]: %s - Can't write mag cal to file\n", __func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int set_mag_cal(struct ssp_data *data)
{
	int ret = 0;
	char buffer[13];

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_GEOMAGNETIC_FIELD))) {
		pr_info("[SSP]: %s - Skip this function!!!"\
			", mag sensor is not connected(0x%llx)\n",
			__func__, data->sensor_probe_state);
		return ret;
	}

	buffer[0] = data->magcal.accuracy;
	memcpy(&buffer[1], &data->magcal.offset_x, sizeof(s16));
	memcpy(&buffer[3], &data->magcal.offset_y, sizeof(s16));
	memcpy(&buffer[5], &data->magcal.offset_z, sizeof(s16));
	memcpy(&buffer[7], &data->magcal.flucv_x, sizeof(s16));
	memcpy(&buffer[9], &data->magcal.flucv_y, sizeof(s16));
	memcpy(&buffer[11], &data->magcal.flucv_z, sizeof(s16));

	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, CAL_DATA, 0,
			       (char *)&buffer, sizeof(buffer), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		goto exit;
	}

	ssp_infof("%u, %d, %d, %d, %d, %d, %d",
		  data->magcal.accuracy, data->magcal.offset_x, data->magcal.offset_y, data->magcal.offset_z,
		  data->magcal.flucv_x, data->magcal.flucv_y, data->magcal.flucv_z);
exit:
	return ret;
}
#endif
#endif

int get_sensorname(struct ssp_data *data, int sensor_type, char *name, int size)
{
	char *buffer = NULL;
	int buffer_length = 0;
	int ret;

	ret = ssp_send_command(data, CMD_GETVALUE, sensor_type,
			       SENSOR_NAME, 1000, NULL, 0, &buffer, &buffer_length);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		if (buffer != NULL)
			kfree(buffer);
		return FAIL;
	}

	if (buffer == NULL) {
		ssp_errf("buffer is null");
		return FAIL;
	}

	memcpy(name, buffer, (size > buffer_length) ? buffer_length : size);
	kfree(buffer);

	ssp_infof("type %d name %s", sensor_type, name);

	return SUCCESS;
}

static void timestamp_sync_work_func(struct work_struct *work)
{
	struct ssp_data *data = container_of(work, struct ssp_data, work_ts_sync);
	int ret;

	ret = ssp_send_command(data, CMD_SETVALUE, TYPE_MCU, RTC_TIME, 0,
			       NULL, 0, NULL, NULL);
	if (ret != SUCCESS) {
		pr_err("[SSP]: %s - TIMESTAMP_SYNC CMD fail %d\n", __func__, ret);
		return;
	}
}

static void timestamp_sync_timer_func(unsigned long ptr)
{
	struct ssp_data *data = (struct ssp_data *)ptr;

	queue_work(data->ts_sync_wq, &data->work_ts_sync);
	mod_timer(&data->ts_sync_timer,
		  round_jiffies_up(jiffies + SSP_TIMESTAMP_SYNC_TIMER_SEC));
}

void enable_timestamp_sync_timer(struct ssp_data *data)
{
	mod_timer(&data->ts_sync_timer,
		  round_jiffies_up(jiffies + SSP_TIMESTAMP_SYNC_TIMER_SEC));
}

void disable_timestamp_sync_timer(struct ssp_data *data)
{
	del_timer_sync(&data->ts_sync_timer);
	cancel_work_sync(&data->work_ts_sync);
}

int initialize_timestamp_sync_timer(struct ssp_data *data)
{
	setup_timer(&data->ts_sync_timer, timestamp_sync_timer_func, (unsigned long)data);

	data->ts_sync_wq = create_singlethread_workqueue("ssp_ts_sync_wq");
	if (!data->ts_sync_wq)
		return -ENOMEM;

	INIT_WORK(&data->work_ts_sync, timestamp_sync_work_func);
	return 0;
}


