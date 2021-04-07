#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "../../ssp.h"
#include "../ssp_factory.h"
#include "../../ssp_cmd_define.h"
#include "../../ssp_comm.h"
#include "../../ssp_sysfs.h"
#include "../../ssp_data.h"

#define PROX_ADC_BITS_NUM               14

#define PDATA_MIN           0
#define PDATA_MAX           0x0FFF

/*************************************************************************/
/* Functions                                                             */
/*************************************************************************/

u16 get_proximity_stk3x3x_raw_data(struct ssp_data *data)
{
	u16 uRowdata = 0;
	char chTempbuf[8] = { 0, };

	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	if (data->is_proxraw_enabled == false) {
        data->is_proxraw_enabled = true;
		set_delay_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_RAW, 20, 0);
		enable_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_RAW);

		msleep(200);
		uRowdata = data->buf[SENSOR_TYPE_PROXIMITY_RAW].prox_raw[0];
                data->is_proxraw_enabled = false;
		disable_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_RAW);
	} else {
		uRowdata = data->buf[SENSOR_TYPE_PROXIMITY_RAW].prox_raw[0];
	}

	return uRowdata;
}

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

ssize_t get_proximity_stk3x3x_name(char *buf)
{
	return sprintf(buf, "%s\n", "STK3031");
}

ssize_t get_proximity_stk3x3x_vendor(char *buf)
{
	return sprintf(buf, "%s\n", "Sitronix");
}

ssize_t get_proximity_stk3x3x_probe_status(struct ssp_data *data, char *buf)
{
	bool probe_pass_fail = FAIL;

	if (data->sensor_probe_state & (1ULL << SENSOR_TYPE_PROXIMITY)) {
		probe_pass_fail = SUCCESS;
	} else {
		probe_pass_fail = FAIL;
	}

	pr_info("[SSP]: %s - All sensor 0x%llx, prox_sensor %d \n",
	        __func__, data->sensor_probe_state, probe_pass_fail);

	return snprintf(buf, PAGE_SIZE, "%d\n", probe_pass_fail);
}

ssize_t get_stk3x3x_threshold_high(struct ssp_data *data, char *buf)
{
	ssp_dbg("[SSP]: ProxThresh = hi - %u \n", data->prox_thresh[PROX_THRESH_HIGH]);

	return sprintf(buf, "%u\n", data->prox_thresh[PROX_THRESH_HIGH]);
}

ssize_t set_stk3x3x_threshold_high(struct ssp_data *data, const char *buf)
{
	u16 uNewThresh;
	int ret, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;

	while (i < PROX_ADC_BITS_NUM) {
		prox_bits_mask += (1 << i++);
	}

	while (i < 16) {
		prox_non_bits_mask += (1 << i++);
	}

	ret = kstrtou16(buf, 10, &uNewThresh);
	if (ret < 0) {
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, ret);
	} else {
		if (uNewThresh & prox_non_bits_mask) {
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM,
			       uNewThresh);
		} else {
			uNewThresh &= prox_bits_mask;
			data->prox_thresh[PROX_THRESH_HIGH] = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u \n", __func__,
	        data->prox_thresh[PROX_THRESH_HIGH]);

	return ret;
}

ssize_t get_stk3x3x_threshold_low(struct ssp_data *data, char *buf)
{
	ssp_dbg("[SSP]: ProxThresh = lo - %u \n", data->prox_thresh[PROX_THRESH_LOW]);

	return sprintf(buf, "%u\n", data->prox_thresh[PROX_THRESH_LOW]);
}

ssize_t set_stk3x3x_threshold_low(struct ssp_data *data, const char *buf)
{
	u16 uNewThresh;
	int ret, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;

	while (i < PROX_ADC_BITS_NUM) {
		prox_bits_mask += (1 << i++);
	}

	while (i < 16) {
		prox_non_bits_mask += (1 << i++);
	}

	ret = kstrtou16(buf, 10, &uNewThresh);
	if (ret < 0) {
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, ret);
	} else {
		if (uNewThresh & prox_non_bits_mask) {
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM,
			       uNewThresh);
		} else {
			uNewThresh &= prox_bits_mask;
			data->prox_thresh[PROX_THRESH_LOW] = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : lo - %u \n", __func__,
	        data->prox_thresh[PROX_THRESH_LOW]);

	return ret;
}

ssize_t get_proximity_stk3x3x_trim_value(struct ssp_data *data, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;
	int trim = 0;

	if (!(data->sensor_probe_state & (1ULL << SENSOR_TYPE_PROXIMITY))) {
		ssp_infof("Skip this function!, proximity sensor is not connected(0x%llx)",
		          data->sensor_probe_state);
		return FAIL;
	}


	if (data->sensor_probe_state == 0 || !is_sensorhub_working(data)) {
		return FAIL;
	}

	ret = ssp_send_command(data, CMD_GETVALUE, SENSOR_TYPE_PROXIMITY,
	                       PROXIMITY_OFFSET, 1000, NULL, 0, &buffer, &buffer_length);

	if (ret != SUCCESS) {
		ssp_errf("ssp_send_command Fail %d", ret);
		if (buffer != NULL) {
			kfree(buffer);
		}
		return FAIL;
	}

	if (buffer == NULL) {
		ssp_errf("buffer is null");
		return -EINVAL;
	}

	if (buffer_length != 2) {
		ssp_errf("buffer length error %d", buffer_length);
		ret = snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
		if (buffer != NULL) {
			kfree(buffer);
		}
		return -EINVAL;
	}

	if (buffer[1] > 0) {
		data->prox_trim = (buffer[0]) * (-1);
	} else {
		data->prox_trim = buffer[0];
	}
	trim = buffer[0];

	pr_info("[SSP] %s - %d, 0x%x, 0x%x \n", __func__, trim, buffer[1], buffer[0]);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", trim);

	if (buffer != NULL) {
		kfree(buffer);
	}

	return ret;
}

ssize_t get_proximity_stk3x3x_avg_raw_data(struct ssp_data *data, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
	                data->buf[SENSOR_TYPE_PROXIMITY_RAW].prox_raw[1],
	                data->buf[SENSOR_TYPE_PROXIMITY_RAW].prox_raw[2],
	                data->buf[SENSOR_TYPE_PROXIMITY_RAW].prox_raw[3]);
}

/*
        Proximity Raw Sensor Register/Unregister
*/
ssize_t set_proximity_stk3x3x_avg_raw_data(struct ssp_data *data,
                                           const char *buf)
{
	char chTempbuf[8] = { 0, };
	int ret;
	int64_t dEnable;

	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	ret = kstrtoll(buf, 10, &dEnable);
	if (ret < 0) {
		return ret;
	}

	if (dEnable) {
		if(!data->is_proxraw_enabled) {
			data->is_proxraw_enabled = true;
			set_delay_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_RAW, 20, 0);
			enable_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_RAW);
		}
	} else {
		if(data->is_proxraw_enabled) {
			data->is_proxraw_enabled = false;
			disable_legacy_sensor(data, SENSOR_TYPE_PROXIMITY_RAW);
		}
	}

	return ret;
}

ssize_t set_proximity_stk3x3x_calibration(struct ssp_data *data, const char *buf)
{
	int ret;
	u8 temp;

	pr_info("[SSP] %s - %s\n", __func__, buf);

	ret = kstrtou8(buf, 10, &temp);
	if (ret < 0) {
		return ret;
	}

	if (temp == 1) {
		do_proximity_calibration(data);
	}

	return ret;
}

ssize_t get_proximity_stk3x3x_calibration_result(char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

struct proximity_sensor_operations prox_stk3x3x_ops = {
	.get_proximity_name = get_proximity_stk3x3x_name,
	.get_proximity_vendor = get_proximity_stk3x3x_vendor,
	.get_proximity_probe_status = get_proximity_stk3x3x_probe_status,
	.get_threshold_high = get_stk3x3x_threshold_high,
	.set_threshold_high = set_stk3x3x_threshold_high,
	.get_threshold_low = get_stk3x3x_threshold_low,
	.set_threshold_low = set_stk3x3x_threshold_low,
	.get_proximity_avg_raw_data = get_proximity_stk3x3x_avg_raw_data,
	.set_proximity_avg_raw_data = set_proximity_stk3x3x_avg_raw_data,
	.get_proximity_raw_data = get_proximity_stk3x3x_raw_data,
	.get_proximity_trim_value = get_proximity_stk3x3x_trim_value,
	.set_proximity_calibration = set_proximity_stk3x3x_calibration,
	.get_proximity_calibration_result = get_proximity_stk3x3x_calibration_result,
};

struct proximity_sensor_operations* get_proximity_stk3x3x_function_pointer(
        struct ssp_data *data)
{
	return &prox_stk3x3x_ops;
}

