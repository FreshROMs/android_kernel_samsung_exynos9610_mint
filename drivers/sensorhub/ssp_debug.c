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
#include <linux/wait.h>

#include "ssp_debug.h"
#include "ssp_type_define.h"
#include "ssp_platform.h"
#include "ssp_comm.h"
#include "ssp_dump.h"
#include "ssp_data.h"
#include "ssp_scontext.h"
#include "ssp_cmd_define.h"

/* define */
#define SSP_DEBUG_TIMER_SEC     (5 * HZ)
#define LIMIT_TIMEOUT_CNT       1
#define LIMIT_COMFAIL_CNT		3

int ssp_wait_event_timeout(struct ssp_waitevent *lock, int timeout)
{
	int ret;
	ret =  wait_event_interruptible_timeout(lock->waitqueue, atomic_read(&lock->state), msecs_to_jiffies(timeout));

	if(ret == 0)
		return FAIL;
	else
		return SUCCESS;
}

void ssp_lock_wait_event(struct ssp_waitevent *lock)
{
	atomic_set(&lock->state, 0);
}

void ssp_wake_up_wait_event(struct ssp_waitevent *lock)
{
	atomic_set(&lock->state, 1);
	wake_up_interruptible_sync(&lock->waitqueue);
}

void reset_mcu(struct ssp_data *data, int reason)
{
	if(work_busy(&data->work_reset)) {
		ssp_infof("reset work state : pending or running");
		return;
	}

	ssp_infof("- reason(%u) pending(%u)", reason, !list_empty(&data->pending_list));
	data->reset_type = reason;

	data->cnt_ssp_reset[RESET_TYPE_MAX]++;
	if(data->reset_type < RESET_TYPE_MAX)
		data->cnt_ssp_reset[data->reset_type]++;

	ssp_lock_wait_event(&data->reset_lock);
	queue_work(data->debug_wq, &data->work_reset);
	return;
}

void reset_task(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct work_struct *)work,
	                                     struct ssp_data, work_reset);
	int ret;
	ssp_infof("");
	disable_timestamp_sync_timer(data);
	ret = sensorhub_reset(data);
	if(ret < 0)	{
		ssp_errf("reset failed");
		ssp_wake_up_wait_event(&data->reset_lock);
	}
}

static void check_no_event(struct ssp_data *data)
{
	u64 timestamp = get_current_timestamp();
	int check_sensors[] = {SENSOR_TYPE_ACCELEROMETER, SENSOR_TYPE_GEOMAGNETIC_FIELD,
		SENSOR_TYPE_GYROSCOPE, SENSOR_TYPE_PRESSURE, SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
		SENSOR_TYPE_GAME_ROTATION_VECTOR, SENSOR_TYPE_GYROSCOPE_UNCALIBRATED};

	int len = sizeof(check_sensors) / sizeof(check_sensors[0]);
	int i, sensor, ret;
	char buffer[9] = {0,};
	bool check_reset = false;

	if(data->check_noevent_reset_cnt >= 0 && data->check_noevent_reset_cnt == data->cnt_reset)
		check_reset = true;

	data->check_noevent_reset_cnt = -1;

	for (i = 0 ; i < len ; i++) {
		sensor = check_sensors[i];
		/* The sensor is registered
		   And none batching mode
		   And there is no sensor event over 5sec */
		if (data->en_info[sensor].enabled
		    && data->delay[sensor].max_report_latency == 0
		    && data->latest_timestamp[sensor] + 5000000000ULL < timestamp) {

			if(check_reset) {
				data->check_noevent_reset_cnt = -1;
				reset_mcu(data, RESET_TYPE_KERNEL_NO_EVENT);
				break;
			}
			data->check_noevent_reset_cnt = data->cnt_reset;

			ssp_infof("sensor(%d) last = %lld, cur = %lld", sensor, data->latest_timestamp[sensor], timestamp);

			buffer[0] = sensor;
			memcpy(&buffer[1], &(data->delay[sensor]), sizeof(data->delay[sensor]));
			ret = ssp_send_command(data, CMD_SETVALUE, TYPE_MCU, NO_EVENT_CHECK, 0, buffer, sizeof(buffer), NULL, NULL);
			if(ret < 0)
				ssp_errf("type %d no event comm failed ret %d", sensor, ret);
		}
	}
}

static void print_sensordata(struct ssp_data *data, unsigned int sensor_type)
{
	if(sensor_type < SENSOR_TYPE_MAX)
	{
		switch (sensor_type) {
		case SENSOR_TYPE_ACCELEROMETER:
		case SENSOR_TYPE_GYROSCOPE:
		case SENSOR_TYPE_INTERRUPT_GYRO:
			ssp_info("%s(%u) : %d, %d, %d (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].x, data->buf[sensor_type].y,
			         data->buf[sensor_type].z,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_GEOMAGNETIC_FIELD:
			ssp_info("%s(%u) : %d, %d, %d, %d (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].cal_x, data->buf[sensor_type].cal_y,
			         data->buf[sensor_type].cal_z, data->buf[sensor_type].accuracy,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
		case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
			ssp_info("%s(%u) : %d, %d, %d, %d, %d, %d (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].uncal_x,
			         data->buf[sensor_type].uncal_y,
			         data->buf[sensor_type].uncal_z,
			         data->buf[sensor_type].offset_x,
			         data->buf[sensor_type].offset_y,
			         data->buf[sensor_type].offset_z,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_PRESSURE:
			ssp_info("%s(%u) : %d, %d (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].pressure,
			         data->buf[sensor_type].temperature,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_LIGHT:
			ssp_info("%s(%u) : %u, %u (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].lux,
			         data->buf[sensor_type].cct,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_LIGHT_CCT:
			ssp_info("%s(%u) : %u, %u, %u (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].lux,
			         data->buf[sensor_type].cct,
			         data->buf[sensor_type].raw_lux,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_PROXIMITY:
			ssp_info("%s(%u) : %d, %d (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].prox, data->buf[sensor_type].prox_ex,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_STEP_DETECTOR:
			ssp_info("%s(%u) : %u (%lld)  (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].step_det,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_GAME_ROTATION_VECTOR:
		case SENSOR_TYPE_ROTATION_VECTOR:
			ssp_info("%s(%u) : %d, %d, %d, %d, %d (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].quat_a, data->buf[sensor_type].quat_b,
			         data->buf[sensor_type].quat_c, data->buf[sensor_type].quat_d,
			         data->buf[sensor_type].acc_rot,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_SIGNIFICANT_MOTION:
			ssp_info("%s(%u) : %u (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].sig_motion,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_STEP_COUNTER:
			ssp_info("%s(%u) : %u (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].step_diff,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		case SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS:
			ssp_info("%s(%u) : %u, %u, %u (%lld) (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->buf[sensor_type].ab_lux,
			         data->buf[sensor_type].ab_min_flag,
			         data->buf[sensor_type].ab_brightness,
			         data->buf[sensor_type].timestamp,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);
			break;
		default:
			ssp_info("%s(%u) : (%ums, %dms)", data->info[sensor_type].name, sensor_type,
			         data->delay[sensor_type].sampling_period,
			         data->delay[sensor_type].max_report_latency);

			break;
		}

	}
	else
	{
		char name[SENSOR_NAME_MAX_LEN] = "";
		get_ss_sensor_name(data, sensor_type, name, SENSOR_NAME_MAX_LEN);
		ssp_info("%s(%u)", name, sensor_type);
	}
}



static void debug_work_func(struct work_struct *work)
{
	struct ssp_data *data = container_of(work, struct ssp_data, work_debug);
	unsigned int type;

	ssp_infof("FW(%d):%u, Sensor state: 0x%llx, En: 0x%llx, Reset cnt: %d[%d : C %u(%u, %u), N %u, %u]",
		  data->fw_type, data->curr_fw_rev,
		  data->sensor_probe_state, data->sensor_en_state,
		  data->cnt_reset, data->cnt_ssp_reset[RESET_TYPE_MAX],
		  data->cnt_ssp_reset[RESET_TYPE_KERNEL_COM_FAIL], data->cnt_com_fail, data->cnt_timeout,
		  data->cnt_ssp_reset[RESET_TYPE_KERNEL_NO_EVENT], data->cnt_ssp_reset[RESET_TYPE_HUB_NO_EVENT]);

	for (type = 0; type < SS_SENSOR_TYPE_MAX; type++)
		if(data->en_info[type].enabled) {
			print_sensordata(data, type);
		}

	if(is_sensorhub_working(data))
		check_no_event(data);

}

static void debug_timer_func(unsigned long ptr)
{
	struct ssp_data *data = (struct ssp_data *)ptr;

	queue_work(data->debug_wq, &data->work_debug);
	mod_timer(&data->debug_timer,
	          round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void enable_debug_timer(struct ssp_data *data)
{
	mod_timer(&data->debug_timer,
	          round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void disable_debug_timer(struct ssp_data *data)
{
	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
}

int initialize_debug_timer(struct ssp_data *data)
{
	setup_timer(&data->debug_timer, debug_timer_func, (unsigned long)data);

	data->debug_wq = create_singlethread_workqueue("ssp_debug_wq");
	if (!data->debug_wq) {
		return -ENOMEM;
	}

	INIT_WORK(&data->work_debug, debug_work_func);
	return 0;
}

int print_mcu_debug(char *dataframe, int *index, int dataframe_length)
{
	u16 length = 0;
	int cur = *index;

	memcpy(&length, dataframe + *index, 1);
	*index += 1;

	if (length > dataframe_length - *index || length <= 0) {
		ssp_infof("[M] invalid debug length(%u/%d/%d)",
		          length, dataframe_length, cur);
		return length ? length : -1;
	}

	ssp_info("[M] %s", &dataframe[*index]);
	*index += length;
	return 0;
}

#define SSP_LOG_MAX_BYTE	200
void print_dataframe(struct ssp_data *data, char *dataframe, int frame_len)
{
	char raw_data[SSP_LOG_MAX_BYTE*4];
	int i = 0, cur = 0;

	while ((frame_len - cur) > 0) {
		int size = 0;
		int pr_size = ((frame_len - cur) > SSP_LOG_MAX_BYTE) ? SSP_LOG_MAX_BYTE : (frame_len - cur);

		memset(raw_data, 0, sizeof(raw_data));
		for (i = 0; i < pr_size; i++)
			size += snprintf(raw_data + size, PAGE_SIZE, "%d ", *(dataframe + cur++));

		ssp_info("%s", raw_data);
	}
}
