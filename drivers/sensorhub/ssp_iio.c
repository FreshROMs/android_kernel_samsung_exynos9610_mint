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
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/events.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/types.h>
#include <linux/slab.h>

#include "ssp.h"
#include "ssp_iio.h"
#include "ssp_cmd_define.h"
#include "ssp_data.h"
#include "ssp_iio.h"
#include "ssp_scontext.h"

#define IIO_CHANNEL             -1
#define IIO_SCAN_INDEX          3
#define IIO_SIGN                's'
#define IIO_SHIFT               0

#define META_EVENT              0
#define META_TIMESTAMP          0

#define PROX_AVG_READ_NUM       80
enum
{
	PROX_RAW_NUM = 0,
	PROX_RAW_MIN,
	PROX_RAW_SUM,
	PROX_RAW_MAX,
	PROX_RAW_DATA_SIZE,
};

#define SCONTEXT_DATA_LEN       56
#define SCONTEXT_HEADER_LEN     8

#define RESET_REASON_KERNEL_RESET            0x01
#define RESET_REASON_MCU_CRASHED             0x02
#define RESET_REASON_SYSFS_REQUEST           0x03
#define RESET_REASON_HUB_REQUEST             0x04

static int ssp_preenable(struct iio_dev *indio_dev)
{
	return 0;
}

static int ssp_predisable(struct iio_dev *indio_dev)
{
	return 0;
}

static const struct iio_buffer_setup_ops ssp_iio_ring_setup_ops = {
	.preenable = &ssp_preenable,
	.predisable = &ssp_predisable,
};

static int ssp_iio_configure_ring(struct iio_dev *indio_dev)
{
	struct iio_buffer *ring;

	ring = iio_kfifo_allocate();
	if (!ring) {
		return -ENOMEM;
	}

	ring->scan_timestamp = true;
	ring->bytes_per_datum = 8;
	indio_dev->buffer = ring;
	indio_dev->setup_ops = &ssp_iio_ring_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	return 0;
}

static void ssp_iio_push_buffers(struct iio_dev *indio_dev, u64 timestamp,
                                 char *data, int data_len)
{
	char buf[data_len + sizeof(timestamp)];

	if (!indio_dev || !data) {
		return;
	}

	memcpy(buf, data, data_len);
	memcpy(buf + data_len, &timestamp, sizeof(timestamp));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);
}

#ifdef CONFIG_SENSORS_SSP_PROXIMITY
static void report_prox_raw_data(struct ssp_data *data, int type,
                                 struct sensor_value *proxrawdata)
{
	if (data->prox_raw_avg[PROX_RAW_NUM]++ >= PROX_AVG_READ_NUM) {
		data->prox_raw_avg[PROX_RAW_SUM] /= PROX_AVG_READ_NUM;
		data->buf[type].prox_raw[1] = (u16)data->prox_raw_avg[1];
		data->buf[type].prox_raw[2] = (u16)data->prox_raw_avg[2];
		data->buf[type].prox_raw[3] = (u16)data->prox_raw_avg[3];

		data->prox_raw_avg[PROX_RAW_NUM] = 0;
		data->prox_raw_avg[PROX_RAW_MIN] = 0;
		data->prox_raw_avg[PROX_RAW_SUM] = 0;
		data->prox_raw_avg[PROX_RAW_MAX] = 0;
	} else {
		data->prox_raw_avg[PROX_RAW_SUM] += proxrawdata->prox_raw[0];

		if (data->prox_raw_avg[PROX_RAW_NUM] == 1) {
			data->prox_raw_avg[PROX_RAW_MIN] = proxrawdata->prox_raw[0];
		} else if (proxrawdata->prox_raw[0] < data->prox_raw_avg[PROX_RAW_MIN]) {
			data->prox_raw_avg[PROX_RAW_MIN] = proxrawdata->prox_raw[0];
		}

		if (proxrawdata->prox_raw[0] > data->prox_raw_avg[PROX_RAW_MAX]) {
			data->prox_raw_avg[PROX_RAW_MAX] = proxrawdata->prox_raw[0];
		}
	}

	data->buf[type].prox_raw[0] = proxrawdata->prox_raw[0];
}

static void report_prox_cal_data(struct ssp_data *data, int type,
                                 struct sensor_value *p_cal_data)
{
	data->prox_thresh[0] = p_cal_data->prox_cal[0];
	data->prox_thresh[1] = p_cal_data->prox_cal[1];
	ssp_info("prox thresh %u %u", data->prox_thresh[0], data->prox_thresh[1]);
	
	proximity_calibration_off(data);
}
#endif

void report_sensor_data(struct ssp_data *data, int type,
                        struct sensor_value *event)
{	
	if (type == SENSOR_TYPE_PROXIMITY) {
		ssp_info("Proximity Sensor Detect : %u, raw : %u",
		         event->prox, event->prox_ex);
#ifdef CONFIG_SENSORS_SSP_PROXIMITY
	} else if (type == SENSOR_TYPE_PROXIMITY_RAW) {
		report_prox_raw_data(data, type, event);
		return;
	} else if (type == SENSOR_TYPE_PROXIMITY_CALIBRATION) {
		report_prox_cal_data(data, type, event);
		return;
#endif
	} else if (type == SENSOR_TYPE_LIGHT) {
#ifdef CONFIG_SENSORS_SSP_LIGHT
		if (data->light_log_cnt < 3) {
			ssp_info("Light Sensor : lux=%u brightness=%u r=%d g=%d b=%d c=%d atime=%d again=%d",
					 data->buf[SENSOR_TYPE_LIGHT].lux,
					 data->buf[SENSOR_TYPE_LIGHT].brightness,
			         data->buf[SENSOR_TYPE_LIGHT].r, data->buf[SENSOR_TYPE_LIGHT].g,
			         data->buf[SENSOR_TYPE_LIGHT].b,
			         data->buf[SENSOR_TYPE_LIGHT].w, data->buf[SENSOR_TYPE_LIGHT].a_time,
			         data->buf[SENSOR_TYPE_LIGHT].a_gain);
			data->light_log_cnt++;
		}
	} else if (type == SENSOR_TYPE_LIGHT_CCT) {
		if (data->light_cct_log_cnt < 3) {
			ssp_info("Light cct Sensor : lux=%u r=%d g=%d b=%d c=%d atime=%d again=%d",
				     data->buf[SENSOR_TYPE_LIGHT_CCT].lux,
			         data->buf[SENSOR_TYPE_LIGHT_CCT].r, data->buf[SENSOR_TYPE_LIGHT_CCT].g,
			         data->buf[SENSOR_TYPE_LIGHT_CCT].b,
			         data->buf[SENSOR_TYPE_LIGHT_CCT].w, data->buf[SENSOR_TYPE_LIGHT_CCT].a_time,
			         data->buf[SENSOR_TYPE_LIGHT_CCT].a_gain);
			data->light_cct_log_cnt++;
		}
	} else if (type == SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS) {
		if(!data->camera_lux_en && 
			(data->buf[type].ab_lux <= data->camera_lux_hysteresis[0]) && 
			(data->buf[type].ab_brightness > data->camera_br_hysteresis[0]))
		{

			if(data->light_ab_log_cnt == 0)
			{
				ssp_infof("Light AB Sensor : report first lux form light sensor");
				report_camera_lux_data(data, data->buf[type].ab_lux);
			}

			ssp_infof("Light AB Sensor : report cam enable");
			data->camera_lux_en = true;
			data->buf[type].ab_lux = CAMERA_LUX_ENABLE;
		}	
		else if(data->camera_lux_en &&
			((data->buf[type].ab_lux >= data->camera_lux_hysteresis[1]) || 
			(data->buf[type].ab_brightness <= data->camera_br_hysteresis[1])))
		{
		    ssp_infof("Light AB Sensor : report cam disable");
			data->camera_lux_en = false;
			data->buf[type].ab_lux = CAMERA_LUX_DISABLE;
		}
		else if(data->camera_lux_en)
		{
			//ssp_infof("Light AB Sensor : report skip");
			return;
		}

		if (data->light_ab_log_cnt < 3) {
			ssp_info("Light AB Sensor : lux=%u brightness=%u camera_lux_en=%d / %d %d %d %d",
				     data->buf[SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS].ab_lux,
			        data->buf[SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS].ab_brightness,
			        data->camera_lux_en,
			        data->camera_lux_hysteresis[0], data->camera_lux_hysteresis[1],
			        data->camera_br_hysteresis[0], data->camera_br_hysteresis[1]);
			data->light_ab_log_cnt++;
		}
#endif
	} else if (type == SENSOR_TYPE_STEP_COUNTER) {
		data->buf[type].step_total += event->step_diff;
	}

	if (!(atomic64_read(&data->sensor_en_state) & (1ULL << type)))
	{
		ssp_errf("sensor is not enabled(%d)", type);
		return;
	}

	ssp_iio_push_buffers(data->indio_devs[type], event->timestamp,
	                     (char *)&data->buf[type], data->info[type].report_data_len);

	/* wake-up sensor */
	if (type == SENSOR_TYPE_PROXIMITY || type == SENSOR_TYPE_SIGNIFICANT_MOTION
	    || type == SENSOR_TYPE_TILT_DETECTOR || type == SENSOR_TYPE_PICK_UP_GESTURE
	    || type == SENSOR_TYPE_WAKE_UP_MOTION) {
		wake_lock_timeout(&data->ssp_wake_lock, 0.3 * HZ);
	}
}

void report_camera_lux_data(struct ssp_data *data, int lux)
{
	int type = SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS;

	ssp_infof("%d", lux);

	data->buf[type].ab_lux = lux;
	ssp_iio_push_buffers(data->indio_devs[type], get_current_timestamp(),
	                     (char *)&data->buf[type], data->info[type].report_data_len);

}

void report_meta_data(struct ssp_data *data, struct sensor_value *s)
{
	char *meta_event = kzalloc(data->info[s->meta_data.sensor].report_data_len, GFP_KERNEL);

	ssp_infof("what: %d, sensor: %d", s->meta_data.what, s->meta_data.sensor);

	memset(meta_event, META_EVENT,
	       data->info[s->meta_data.sensor].report_data_len);
	ssp_iio_push_buffers(data->indio_devs[s->meta_data.sensor],
	                     META_TIMESTAMP, meta_event,
	                     data->info[s->meta_data.sensor].report_data_len);
	kfree(meta_event);
}

void report_scontext_data(struct ssp_data *data, char *data_buf, u32 length)
{
	char buf[SCONTEXT_HEADER_LEN + SCONTEXT_DATA_LEN] = {0, };
	u16 start, end;
	u64 timestamp;

	ssp_scontext_log(__func__, data_buf, length);

	start = 0;
	memcpy(buf, &length, sizeof(u32));
	timestamp = get_current_timestamp();

	while (start < length) {
		if (start + SCONTEXT_DATA_LEN < length) {
			end = start + SCONTEXT_DATA_LEN - 1;
		} else {
			memset(buf + SCONTEXT_HEADER_LEN, 0, SCONTEXT_DATA_LEN);
			end = length - 1;
		}

		memcpy(buf + sizeof(length), &start, sizeof(u16));
		memcpy(buf + sizeof(length) + sizeof(start), &end, sizeof(u16));
		memcpy(buf + SCONTEXT_HEADER_LEN, data_buf + start, end - start + 1);

/*
        ssp_infof("[%d, %d, %d] 0x%x 0x%x 0x%x 0x%x// 0x%x 0x%x// 0x%x 0x%x",
                length, start, end,
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);


        ssp_infof("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);


        ssp_infof("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x, //0x%llx",
                buf[16], buf[17], buf[18], buf[19], buf[20], buf[21], buf[22], buf[23], timestamp);
*/		
		ssp_iio_push_buffers(data->indio_devs[SENSOR_TYPE_SCONTEXT], timestamp,
		                     buf, data->info[SENSOR_TYPE_SCONTEXT].report_data_len);

		start = end + 1;
	}
}

void report_scontext_notice_data(struct ssp_data *data, char notice)
{
	char notice_buf[4] = {0x02, 0x01, 0x00, 0x00};
	int len = 3;

	notice_buf[2] = notice;
	if (notice == SCONTEXT_AP_STATUS_RESET) {
		len = 4;
		if (data->reset_type == RESET_TYPE_KERNEL_SYSFS) {
			notice_buf[3] = RESET_REASON_SYSFS_REQUEST;
		} else if(data->reset_type == RESET_TYPE_KERNEL_NO_EVENT) {
			notice_buf[3] = RESET_REASON_KERNEL_RESET;
		} else if(data->reset_type == RESET_TYPE_KERNEL_COM_FAIL) {
			notice_buf[3] = RESET_REASON_KERNEL_RESET;
		} else if(data->reset_type == RESET_TYPE_HUB_CRASHED) {
			notice_buf[3] = RESET_REASON_MCU_CRASHED;
		} else if(data->reset_type == RESET_TYPE_HUB_NO_EVENT) {
			notice_buf[3] = RESET_REASON_HUB_REQUEST;
		}
	}

	report_scontext_data(data, notice_buf, len);

	if (notice == SCONTEXT_AP_STATUS_WAKEUP) {
		ssp_infof("wake up");
	} else if (notice == SCONTEXT_AP_STATUS_SLEEP) {
		ssp_infof("sleep");
	} else if (notice == SCONTEXT_AP_STATUS_RESET) {
		ssp_infof("reset");
	} else {
		ssp_errf("invalid notice(0x%x)", notice);
	}
}

void report_sensorhub_data(struct ssp_data *data, char* buf)
{
	ssp_infof();
	ssp_iio_push_buffers(data->indio_devs[SENSOR_TYPE_SENSORHUB], get_current_timestamp(),
							buf, data->info[SENSOR_TYPE_SENSORHUB].report_data_len);
}

static void *init_indio_device(struct device *dev, struct ssp_data *data,
                               const struct iio_info *info,
                               const struct iio_chan_spec *channels,
                               const char *device_name)
{
	struct iio_dev *indio_dev;
	int ret = 0;

	indio_dev = iio_device_alloc(0);
	if (!indio_dev) {
		goto err_alloc;
	}

	indio_dev->name = device_name;
	indio_dev->dev.parent = dev;
	indio_dev->info = info;
	indio_dev->channels = channels;
	indio_dev->num_channels = 1;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	ret = ssp_iio_configure_ring(indio_dev);
	if (ret) {
		goto err_config_ring;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		goto err_register_device;
	}

	return indio_dev;

err_register_device:
	ssp_err("fail to register %s device", device_name);
	iio_kfifo_free(indio_dev->buffer);
err_config_ring:
	ssp_err("failed to configure %s buffer\n", indio_dev->name);
	iio_device_unregister(indio_dev);
err_alloc:
	ssp_err("fail to allocate memory for iio %s device", device_name);
	return NULL;
}

static const struct iio_info indio_info = {
	.driver_module = THIS_MODULE,
};

int initialize_indio_dev(struct device *dev, struct ssp_data *data)
{
	int timestamp_len = 0;
	int type;
	int realbits_size = 0;
	int repeat_size = 0;

	for (type = 0; type < SENSOR_TYPE_MAX; type++) {
		if (!data->info[type].enable || (data->info[type].report_data_len == 0)) {
			continue;
		}

		timestamp_len = sizeof(data->buf[type].timestamp);

		realbits_size = (data->info[type].report_data_len+timestamp_len) * BITS_PER_BYTE;
		repeat_size = 1;

		while ((realbits_size / repeat_size > 255) && (realbits_size % repeat_size == 0))
			repeat_size++;

		realbits_size /= repeat_size;

		data->indio_channels[type].type = IIO_TIMESTAMP;
		data->indio_channels[type].channel = IIO_CHANNEL;
		data->indio_channels[type].scan_index = IIO_SCAN_INDEX;
		data->indio_channels[type].scan_type.sign = IIO_SIGN;
		data->indio_channels[type].scan_type.realbits = realbits_size;
		data->indio_channels[type].scan_type.storagebits = realbits_size;
		data->indio_channels[type].scan_type.shift = IIO_SHIFT;
		data->indio_channels[type].scan_type.repeat = repeat_size;
		
		data->indio_devs[type]
		        = (struct iio_dev *)init_indio_device(dev, data,
		                                              &indio_info, &data->indio_channels[type],
		                                              data->info[type].name);

		if (!data->indio_devs[type]) {
			ssp_err("fail to init %s iio dev", data->info[type].name);
			remove_indio_dev(data);
			return ERROR;
		}
	}

	return SUCCESS;
}

void remove_indio_dev(struct ssp_data *data)
{
	int type;

	for (type = SENSOR_TYPE_MAX - 1; type >= 0; type--) {
		if (data->indio_devs[type]) {
			iio_device_unregister(data->indio_devs[type]);
		}
	}
}

