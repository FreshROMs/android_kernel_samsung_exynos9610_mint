/*
 * MELFAS MMS400 Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 *
 * Command Functions (Optional)
 *
 */

#include "melfas_mss100.h"
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/trustedui.h>
#endif

#if MMS_USE_CMD_MODE

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	char buff[256] = { 0 };

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
	input_info(true, &info->client->dev,
			"%s: scrub_id: %d\n", __func__, info->scrub_id);
#else
	input_info(true, &info->client->dev,
			"%s: scrub_id: %d, X:%d, Y:%d\n", __func__,
			info->scrub_id, info->scrub_x, info->scrub_y);
#endif

	snprintf(buff, sizeof(buff), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_id = 0;
	info->scrub_x = 0;
	info->scrub_y = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%ld", info->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	long data;
	int ret;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &info->client->dev, "%s: %ld\n", __func__, data);

	info->prox_power_off = data;

	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	u32 feature = 0;

	if (info->dtdata->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (info->dtdata->sync_reportrate_120)
		feature |= INPUT_FEATURE_ENABLE_SYNC_RR120;

	input_info(true, &info->client->dev, "%s: %d%s%s%s\n",
			__func__, feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
			feature & INPUT_FEATURE_ENABLE_PRESSURE ? " pressure" : "",
			feature & INPUT_FEATURE_ENABLE_SYNC_RR120 ? " RR120hz" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t ear_detect_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->ed_enable);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d\n", info->ed_enable);
}

static ssize_t ear_detect_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	long data;
	int ret;
	u8 wbuf[3];

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &info->client->dev, "%s: %ld\n", __func__, data);

	info->ed_enable = data;

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_PROXIMITY;
	wbuf[2] = info->ed_enable;

	if (mms_i2c_write(info, wbuf, 3))
		input_err(true, &info->client->dev, "%s: failed to set ed_enable\n", __func__);

	return count;
}

static ssize_t fod_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	u8 data[255] = { 0 };
	char buff[3] = { 0 };
	int i, ret;

	if (!info->dtdata->support_fod) {
		input_err(true, &info->client->dev, "%s: fod is not supported\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	if (!info->fod_vi_size) {
		input_err(true, &info->client->dev, "%s: not read fod_info yet\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	ret = sponge_read(info, SPONGE_FOD_POSITION, data, info->fod_vi_size);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to read\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	for (i = 0; i < info->fod_vi_size; i++) {
		snprintf(buff, 3, "%02X", data[i]);
		strlcat(buf, buff, SEC_CMD_BUF_SIZE);
	}

	return strlen(buf);
}

static ssize_t fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	if (!info->dtdata->support_fod) {
		input_err(true, &info->client->dev, "%s: fod is not supported\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	input_info(true, &info->client->dev, "%s: tx:%d, rx:%d, size:%d\n",
			__func__, info->fod_tx, info->fod_rx, info->fod_vi_size);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d", info->fod_tx, info->fod_rx, info->fod_vi_size);
}

/**
 * Command : Update firmware
 */
static void cmd_fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int fw_location = 0;

	sec_cmd_set_default_result(sec);
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	if (sec->cmd_param[0] == 1) {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		input_info(true, &info->client->dev, "%s: user_ship, success\n", __func__);
		return;
	}
#endif

	if (info->ic_status == PWR_OFF) {
		input_err(true, &info->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	fw_location = sec->cmd_param[0];

	/* Factory cmd for firmware update
	 * argument represent what is source of firmware like below.
	 *
	 * 0 : [BUILT_IN] Getting firmware which is for user.
	 * 1 : [UMS] Getting firmware from sd card.
	 * 2 : none
	 * 3 : [FFU] Getting firmware from air.
	 */

	switch (fw_location) {
	case 0:
		if (mms_fw_update_from_kernel(info, true))
			goto ERROR;
		break;
	case 1:
		if (mms_fw_update_from_storage(info, true))
			goto ERROR;
		break;
	case 3:
		if (mms_fw_update_from_ffu(info, true))
			goto ERROR;
		break;
	default:
		goto ERROR;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;	
	goto EXIT;

ERROR:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	goto EXIT;

EXIT:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buff, sec->cmd_state);
}

/**
 * Command : Get firmware version from MFSB file
 */
static void cmd_get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff),"ME%02X%02X%02X%02X",
					info->fw_ver_bin[4], info->fw_ver_bin[5],
					info->fw_ver_bin[6], info->fw_ver_bin[7]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

/**
 * Command : Get firmware version from IC
 */
static void cmd_get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[16] = { 0 };
	u8 rbuf[16];
	char model[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->ic_status == PWR_OFF) {
		input_err(true, &info->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (mms_get_fw_version(info, rbuf)) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buff, sizeof(buff),"ME%02X%02X%02X%02X",
					rbuf[4], rbuf[5], rbuf[6], rbuf[7]);

	snprintf(model, sizeof(model), "ME%02X%02X",rbuf[4], rbuf[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buff, sec->cmd_state);
}

/**
 * Command : Get chip vendor
 */
static void cmd_get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "MELFAS");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "IC_VENDOR");
	
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);

	return;
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_PANEL_CONN))
		goto EXIT;

	input_info(true, &info->client->dev, "%s: connection check(%d)\n", __func__, info->image_buf[0]);

	if (!info->image_buf[0])
		goto EXIT;
	
	sprintf(buf, "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);

	return;

EXIT:
	sprintf(buf, "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Get chip name
 */
static void cmd_get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	//u8 rbuf[64];

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), CHIP_NAME);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "IC_NAME");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);

	return;
}

static void cmd_get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buf, sizeof(buf), "%s_ME_%02d%02d",
		info->product_name, info->fw_month, info->fw_date);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CHECKSUM_REALTIME;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_err(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void cmd_get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 wbuf[5];
	u8 precal[5];
	u8 realtime[5];
	u8 rbuf[8];

	sec_cmd_set_default_result(sec);

	if (mms_get_fw_version(info, rbuf)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	if (info->fw_ver_ic == 0xFFFF) {
		input_info(true, &info->client->dev, "%s: fw version fail\n", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CHECKSUM_PRECALC;
	if (mms_i2c_read(info, wbuf, 2, precal, 1)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CHECKSUM_REALTIME;
	if (mms_i2c_read(info, wbuf, 2, realtime, 1)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	input_info(true, &info->client->dev, "%s: checksum1:%02X, checksum2:%02X\n",
		__func__, precal[0], realtime[0]);

	if (precal[0] == realtime[0]) {
		snprintf(buf, sizeof(buf), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

EXIT:
	input_err(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Get X ch num
 */
static void cmd_get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_X;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Get Y ch num
 */
static void cmd_get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_Y;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Get X resolution
 */
static void cmd_get_max_x(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 2)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[0]) | (rbuf[1] << 8);

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Get Y resolution
 */
static void cmd_get_max_y(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_Y;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 2)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[0]) | (rbuf[1] << 8);

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Power off
 */
static void cmd_module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	mms_power_control(info, 0);

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Power on
 */
static void cmd_module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	mms_power_control(info, 1);

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_dbg(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

/**
 * Command : Run cm delta test
 */
static void cmd_run_test_cm(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CM)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", info->test_min, info->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM");
}

static void cmd_run_test_cm_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CM)) {
		input_err(true, &info->client->dev, "%s: failed to cm read\n", __func__);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_run_test_cm_h_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CM_DIFF_HOR)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", info->test_min, info->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM_H_GAP");
}

static void cmd_run_test_cm_h_gap_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = mms_run_test(info, MIP_TEST_TYPE_CM_DIFF_HOR);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_run_test_cm_v_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CM_DIFF_VER)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", info->test_min, info->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM_V_GAP");
}
static void cmd_run_test_cm_v_gap_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = mms_run_test(info, MIP_TEST_TYPE_CM_DIFF_VER);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_run_test_cm_jitter(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CM_JITTER)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", info->test_min, info->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM_JITTER");
}
static void cmd_run_test_cm_jitter_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = mms_run_test(info, MIP_TEST_TYPE_CM_JITTER);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_run_test_cp(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	int i;
	int tx_min, rx_min;
	int tx_max, rx_max;

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CP)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	for (i = 0; i < info->node_y; i++) {
		if (i == 0)
			rx_min = rx_max = info->image_buf[i];

		rx_min = min(rx_min, info->image_buf[i]);
		rx_max = max(rx_max, info->image_buf[i]);
	}

	for (i = info->node_y; i < (info->node_x + info->node_y); i++) {
		if (i == info->node_y)
			tx_min = tx_max = info->image_buf[i];

		tx_min = min(tx_min, info->image_buf[i]);
		tx_max = max(tx_max, info->image_buf[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", info->test_min, info->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buf, sizeof(buf), "%d,%d", tx_min, tx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_TX");
		snprintf(buf, sizeof(buf), "%d,%d", rx_min, rx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_RX");
	}

	return;

ERROR:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_TX");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_RX");
	}
}
static void cmd_run_test_cp_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = mms_run_test(info, MIP_TEST_TYPE_CP);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_run_test_cp_short(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	int i;
	int tx_min, rx_min;
	int tx_max, rx_max;

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CP_SHORT)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	for (i = 0; i < info->node_y; i++) {
		if (i == 0)
			rx_min = rx_max = info->image_buf[i];

		rx_min = min(rx_min, info->image_buf[i]);
		rx_max = max(rx_max, info->image_buf[i]);
	}

	for (i = info->node_y; i < (info->node_x + info->node_y); i++) {
		if (i == info->node_y)
			tx_min = tx_max = info->image_buf[i];

		tx_min = min(tx_min, info->image_buf[i]);
		tx_max = max(tx_max, info->image_buf[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", info->test_min, info->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buf, sizeof(buf), "%d,%d", tx_min, tx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_TX");
		snprintf(buf, sizeof(buf), "%d,%d", rx_min, rx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_RX");
	}

	return;

ERROR:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_TX");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_RX");
	}
}
static void cmd_run_test_cp_short_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = mms_run_test(info, MIP_TEST_TYPE_CP_SHORT);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_run_test_cp_lpm(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	int i;
	int tx_min, rx_min;
	int tx_max, rx_max;

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_CP_LPM)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	for (i = 0; i < info->node_y; i++) {
		if (i == 0)
			rx_min = rx_max = info->image_buf[i];

		rx_min = min(rx_min, info->image_buf[i]);
		rx_max = max(rx_max, info->image_buf[i]);
	}

	for (i = info->node_y; i < (info->node_x + info->node_y); i++) {
		if (i == info->node_y)
			tx_min = tx_max = info->image_buf[i];

		tx_min = min(tx_min, info->image_buf[i]);
		tx_max = max(tx_max, info->image_buf[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", info->test_min, info->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buf, sizeof(buf), "%d,%d", tx_min, tx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_TX");
		snprintf(buf, sizeof(buf), "%d,%d", rx_min, rx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_RX");
	}

	return;

ERROR:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_TX");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_RX");
	}
}

static void cmd_run_test_cp_lpm_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = mms_run_test(info, MIP_TEST_TYPE_CP_LPM);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	u8 wbuf[4] = {0, };
	u8 rbuf[4] = {0, };
	char data[SEC_CMD_STR_LEN] = {0};

	sec_cmd_set_default_result(sec);

	if (mms_get_image(info, MIP_IMG_TYPE_PROX_INTENSITY)) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__);
		sprintf(data, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PROXIMITY_THD;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		sprintf(data, "%s", "NG");
		goto out;
	}

	snprintf(data, sizeof(data), "SUM_X:%d THD_X:%d", info->image_buf[3], rbuf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, data, strlen(data));
}

static void run_cs_delta_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	sec_cmd_set_default_result(sec);

	if (mms_get_image(info, MIP_IMG_TYPE_INTENSITY)) {
		input_err(true, &info->client->dev, "%s: failed to read, %d\n", __func__);
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void cmd_check_gpio(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	int value_low;
	int value_high;

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_GPIO_LOW)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto error;
	}
	value_low = info->image_buf[0];

	if (mms_run_test(info, MIP_TEST_TYPE_GPIO_HIGH)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto error;
	}

	value_high = info->image_buf[0];

	if ((value_high == 1) && (value_low == 0)) {
		snprintf(buf, sizeof(buf), "%d", value_low);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

error:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "GPIO_CHECK");
}

static void cmd_check_wet_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 wbuf[4] = {0, };
	u8 rbuf[4] = {0, };

	sec_cmd_set_default_result(sec);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_WET_MODE;	
	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d", rbuf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:	
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "WET_MODE");
}

static void cmd_get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };
	u8 rbuf[4];
	u8 wbuf[4];

	sec_cmd_set_default_result(sec);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CONTACT_THD_SCR;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		sprintf(info->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	sprintf(buf, "%d", rbuf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_err(true, &info->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void cmd_run_test_vsync(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (mms_run_test(info, MIP_TEST_TYPE_VSYNC)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d", info->image_buf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "VSYNC");
}

static void mms_gap_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	char buff[16] = { 0 };
	int ii;
	int16_t node_gap_h = 0;
	int16_t node_gap_v = 0;
	int16_t h_max = 0;
	int16_t v_max = 0;
	int16_t h_min = 999;
	int16_t v_min = 999;
	
	int numerator_v;
	int denominator_v;

	int numerator_h;
	int denominator_h;

	for (ii = 0; ii < (info->node_x * info->node_y); ii++) {
		if ((ii + 1) % (info->node_y) != 0) {
			numerator_h = (int)(info->image_buf[ii + 1] - info->image_buf[ii]);
			denominator_h = (int)((info->image_buf[ii + 1] + info->image_buf[ii]) >> 1);

			if (denominator_h > 1)
				node_gap_h = (numerator_h * DIFF_SCALER) / denominator_h;
			else
				node_gap_h = (numerator_h * DIFF_SCALER) / 1;

			h_max = max(h_max, node_gap_h);
			h_min = min(h_min, node_gap_h);
		}

		if (ii < (info->node_x - 1) * info->node_y) {
			numerator_v = (int)(info->image_buf[ii + info->node_y] - info->image_buf[ii]) ;
			denominator_v = (int)((info->image_buf[ii + info->node_y] + info->image_buf[ii]) >> 1);

			if (denominator_v > 1)
				node_gap_v = (numerator_v * DIFF_SCALER) / denominator_v;
			else
				node_gap_v = (numerator_v * DIFF_SCALER) / 1;

			v_max = max(v_max, node_gap_v);
			v_min = min(v_min, node_gap_v);
		}
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", h_min, h_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "CM_H_GAP");
		snprintf(buff, sizeof(buff), "%d,%d", v_min, v_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "CM_V_GAP");
	}
}

static void cmd_mms_gap_data_h_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char *buff = NULL;
	int ii;
	int16_t node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	int numerator;
	int denominator;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(info->node_x * info->node_y * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, sizeof(temp), "%s", "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (ii = 0; ii < (info->node_x * info->node_y); ii++) {
		if ((ii + 1) % (info->node_y) != 0) {
			numerator = (int)(info->image_buf[ii + 1] - info->image_buf[ii]);
			denominator = (int)((info->image_buf[ii + 1] + info->image_buf[ii]) >> 1);

			if (denominator > 1)
				node_gap = (numerator * DIFF_SCALER) / denominator;
			else
				node_gap = (numerator * DIFF_SCALER) / 1;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, info->node_x * info->node_y * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, info->node_x * info->node_y * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void cmd_mms_gap_data_v_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char *buff = NULL;
	int ii;
	int16_t node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	int numerator;
	int denominator;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(info->node_x * info->node_y * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, sizeof(temp), "%s", "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (ii = 0; ii < (info->node_x * info->node_y); ii++) {
		if (ii < (info->node_x - 1) * info->node_y) {
			numerator = (int)(info->image_buf[ii + info->node_y] - info->image_buf[ii]) ;
			denominator = (int)((info->image_buf[ii + info->node_y] + info->image_buf[ii]) >> 1);
			
			if (denominator > 1)
				node_gap = (numerator * DIFF_SCALER) / denominator;
			else
				node_gap = (numerator * DIFF_SCALER) / 1;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, info->node_x * info->node_y * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, info->node_x * info->node_y * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char test[32];
	char result[32];
	u8 wbuf[4];

	sec_cmd_set_default_result(sec);

	memset(test, 0x00, sizeof(test));
	memset(result, 0x00, sizeof(result));
	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	if (sec->cmd_param[0] == OPEN_SHORT_TEST && sec->cmd_param[1] == 0) {
		input_err(true, &info->client->dev,
				"%s: seperate cm1 test open / short test result\n", __func__);

		snprintf(info->print_buf, sizeof(info->print_buf), "%s", "CONT");
		sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
		return;
	}

	if (sec->cmd_param[0] == OPEN_SHORT_TEST &&
		sec->cmd_param[1] == CHECK_ONLY_OPEN_TEST) {
		info->open_short_type = CHECK_ONLY_OPEN_TEST;

	} else if (sec->cmd_param[0] == OPEN_SHORT_TEST &&
		sec->cmd_param[1] == CHECK_ONLY_SHORT_TEST) {
		info->open_short_type = CHECK_ONLY_SHORT_TEST;
	} else {
		input_err(true, &info->client->dev, "%s: not support\n", __func__);
		snprintf(info->print_buf, PAGE_SIZE, "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
		sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
		return;
	}

	if (!info->dtdata->no_vsync) {
		/* vsync off */
		wbuf[0] = MIP_R0_CTRL;
		wbuf[1] = MIP_R1_CTRL_ASYNC;
		wbuf[2] = 0;

		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s [ERROR] failed to write async cmd\n", __func__);
			snprintf(info->print_buf, PAGE_SIZE, "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "RESULT=FAIL");
			sec_cmd_send_event_to_user(sec, test, result);
			sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
			return;
		} else {
			input_info(true, &info->client->dev, "%s success to write async cmd\n", __func__);
		}
	}

	if (mms_run_test(info, MIP_TEST_TYPE_OPEN_SHORT)) {
		input_err(true, &info->client->dev, "%s: failed to read open short\n", __func__);
		snprintf(info->print_buf, PAGE_SIZE, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
		sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
		return;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (info->open_short_result == 1) {
		snprintf(result, sizeof(result), "RESULT=PASS");
		sec_cmd_send_event_to_user(sec, test, result);
	} else {
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
	}

	sec_cmd_set_cmd_result(sec, info->print_buf, strlen(info->print_buf));
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int enable = sec->cmd_param[0];
	u8 wbuf[4];
	int status;

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s %d\n", __func__, enable);

	if (enable)
		status = 0;
	else
		status = 2;

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_DISABLE_EDGE_EXPAND;
	wbuf[2] = status;

	if ((enable == 0) || (enable == 1)) {
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
			goto out;
		} else
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, wbuf[2]);
	} else {
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value[%d]\n", __func__, status);
		goto out;
	}
	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);
}

#ifdef GLOVE_MODE
static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int enable = sec->cmd_param[0];
	u8 wbuf[4];

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s %d\n", __func__, enable);

	info->glove_mode = enable;

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_GLOVE_MODE;
	wbuf[2] = enable;

	if ((enable == 0) || (enable == 1)) {
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
			goto out;
		} else
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, wbuf[2]);
	} else {
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value[%d]\n", __func__, enable);
		goto out;
	}
	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);
}
#endif

#ifdef COVER_MODE
static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int enable = sec->cmd_param[0];
	u8 wbuf[4];

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s %d\n", __func__, enable);

	if (!info->enabled) {
		input_err(true, &info->client->dev,
			"%s : tsp disabled\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		goto out;
	}

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_WINDOW_MODE;
	wbuf[2] = enable;

	if ((enable >= 0) || (enable <= 3)) {
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
			goto out;
		} else{
			input_info(true, &info->client->dev, "%s - value[%d]\n", __func__, wbuf[2]);
		}
	} else {
		input_err(true, &info->client->dev, "%s [ERROR] Unknown value[%d]\n", __func__, enable);
		goto out;
	}

	if (enable > 0)
		info->cover_mode = true;
	else
		info->cover_mode = false;

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
static void tui_mode_cmd(struct mms_ts_info *info)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = "TUImode:FAIL";

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	input_err(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
		  (int)strnlen(buff, sizeof(buff)));
}
#endif

void set_grip_data_to_ic(struct mms_ts_info *info)
{
	u8 data[7] = { 0 };

	data[0] = MIP_R0_CUSTOM;
	data[1] = MIP_R1_SEC_GRIP_EDGE_HANDLER_TOP_BOTTOM;
	data[2] = info->grip_landscape_mode;
	if (info->grip_landscape_mode == 1) {
		data[3] = info->grip_landscape_top_deadzone & 0xFF;
		data[4] = (info->grip_landscape_top_deadzone >> 8) & 0xFF;
		data[5] = info->grip_landscape_bottom_deadzone & 0xFF;
		data[6] = (info->grip_landscape_bottom_deadzone >> 8) & 0xFF;
	}
	
	mms_i2c_write(info, data, 7);
	input_info(true, &info->client->dev, "%s: top bottom %02X,%02X,%02X,%02X\n",
			__func__, data[3], data[4], data[5], data[6]);
}

/*
 * only support tom bottom for letter box
 */
static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	memset(buff, 0, sizeof(buff));

	mutex_lock(&info->lock);

	if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			info->grip_landscape_mode = 0;
		} else if (sec->cmd_param[1] == 1) {
			info->grip_landscape_mode = 1;
			info->grip_landscape_top_deadzone = sec->cmd_param[4];
			info->grip_landscape_bottom_deadzone = sec->cmd_param[5];
		} else {
			input_err(true, &info->client->dev, "%s: cmd1 is abnormal, %d\n",
					__func__, sec->cmd_param[1]);
			goto err_grip_data;
		}
		set_grip_data_to_ic(info);
	} else {
		input_err(true, &info->client->dev, "%s: cmd0 is abnormal, %d", __func__, sec->cmd_param[0]);
		goto err_grip_data;
	}

	mutex_unlock(&info->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:
	mutex_unlock(&info->lock);

	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	info->fod_lp_mode = sec->cmd_param[0];

	input_info(true, &info->client->dev, "%s: fod_lp_mode %d\n", __func__, info->fod_lp_mode);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
}

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int ret;
	u8 fod_property;

	sec_cmd_set_default_result(sec);

	if (!info->dtdata->support_lpm || !info->dtdata->support_fod) {
		input_err(true, &info->client->dev, "%s not supported\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0]) {
		info->lowpower_mode = true;
		info->lowpower_flag = info->lowpower_flag | MMS_MODE_SPONGE_PRESS;
	} else {
		info->lowpower_flag = info->lowpower_flag & ~(MMS_MODE_SPONGE_PRESS);
		if (!info->lowpower_flag)
			info->lowpower_mode = false;
	}

	fod_property = !!sec->cmd_param[1];

	input_info(true, &info->client->dev, "%s: fast: %d, %x\n",
			__func__, fod_property, info->lowpower_flag);

	ret = mms_set_custom_library(info, SPONGE_AOD_ENABLE_OFFSET, &(info->lowpower_flag), 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail set custom lib \n", __func__);
		goto out;
	}

	ret = mms_set_custom_library(info, SPONGE_FOD_PROPERTY, &fod_property, 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail set property \n", __func__);
		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!info->dtdata->support_lpm) {
		input_err(true, &info->client->dev, "%s not supported\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0]) {
		info->lowpower_mode = true;
		info->lowpower_flag = info->lowpower_flag | MMS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
	} else {
		info->lowpower_flag = info->lowpower_flag & ~(MMS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP);
		if (!info->lowpower_flag)
			info->lowpower_mode = false;
	}

	input_info(true, &info->client->dev, "%s: %s mode, %x\n",
			__func__, info->lowpower_mode ? "LPM" : "normal",
			info->lowpower_flag);

	ret = mms_set_custom_library(info, SPONGE_AOD_ENABLE_OFFSET, &(info->lowpower_flag), 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail set custom lib \n", __func__);
		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!info->dtdata->support_lpm) {
		input_err(true, &info->client->dev, "%s not supported\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0]) {
		info->lowpower_mode = true;
		info->lowpower_flag = info->lowpower_flag | MMS_MODE_SPONGE_SWIPE;
	} else {
		info->lowpower_flag = info->lowpower_flag & ~(MMS_MODE_SPONGE_SWIPE);
		if (!info->lowpower_flag)
			info->lowpower_mode = false;
	}

	input_info(true, &info->client->dev, "%s: %s mode, %x\n",
			__func__, info->lowpower_mode ? "LPM" : "normal",
			info->lowpower_flag);

	ret = mms_set_custom_library(info, SPONGE_AOD_ENABLE_OFFSET, &(info->lowpower_flag), 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail set custom lib \n", __func__);
		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!info->dtdata->support_lpm) {
		input_err(true, &info->client->dev, "%s not supported\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0]) {
		info->lowpower_mode = true;
		info->lowpower_flag = info->lowpower_flag | MMS_MODE_SPONGE_SINGLE_TAP;
	} else {
		info->lowpower_flag = info->lowpower_flag & ~(MMS_MODE_SPONGE_SINGLE_TAP);
		if (!info->lowpower_flag)
			info->lowpower_mode = false;
	}

	input_info(true, &info->client->dev, "%s: %s mode, %x\n",
			__func__, info->lowpower_mode ? "LPM" : "normal",
			info->lowpower_flag);

	ret = mms_set_custom_library(info, SPONGE_AOD_ENABLE_OFFSET, &(info->lowpower_flag), 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail set custom lib \n", __func__);
		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!info->dtdata->support_lpm) {
		input_err(true, &info->client->dev, "%s not supported\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0]) {
		info->lowpower_mode = true;
		info->lowpower_flag = info->lowpower_flag | MMS_MODE_SPONGE_AOD;
	} else {
		info->lowpower_flag = info->lowpower_flag & ~(MMS_MODE_SPONGE_AOD);
		if (!info->lowpower_flag)
			info->lowpower_mode = false;
	}
	input_info(true, &info->client->dev, "%s: %s mode, %x\n",
			__func__, info->lowpower_mode ? "LPM" : "normal",
			info->lowpower_flag);

	ret = mms_set_custom_library(info, SPONGE_AOD_ENABLE_OFFSET, &(info->lowpower_flag), 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail set custom lib \n", __func__);
		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	u8 data[11] = {0};
	int i;
	int ret;

	sec_cmd_set_default_result(sec);

	disable_irq(info->client->irq);

	if (!info->enabled) {
		input_err(true, &info->client->dev,
			  "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		goto out;
	}

	input_info(true, &info->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	for (i = 0; i < 4; i++) {
		data[i * 2] = sec->cmd_param[i] & 0xFF;
		data[i * 2 + 1] = (sec->cmd_param[i] >> 8) & 0xFF;
	}

	ret = mms_set_custom_library(info, SPONGE_TOUCHBOX_W_OFFSET, data, 8);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail set custom lib \n", __func__);
		goto out;
	}
	enable_irq(info->client->irq);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	enable_irq(info->client->irq);

	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);	
}


static void get_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[64] = { 0 };
	u8 wbuf[16];
	u8 rbuf[16];
	u16 rect_data[4] = {0, };
	int i;

	sec_cmd_set_default_result(sec);

	disable_irq(info->client->irq);

	wbuf[0] = MIP_R0_AOT;
	wbuf[1] = MIP_R0_AOT_BOX_W;

	if (mms_i2c_read(info, wbuf, 2, rbuf, 8)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		goto out;
	}

	enable_irq(info->client->irq);

	for (i = 0; i < 4; i++)
		rect_data[i] = (rbuf[i * 2 + 1] & 0xFF) << 8 | (rbuf[i * 2] & 0xFF);

	input_info(true, &info->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, rect_data[0], rect_data[1], rect_data[2], rect_data[3]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;
out:
	enable_irq(info->client->irq);
	
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);	
}

/**
 * Command : Unknown cmd
 */
static void cmd_unknown_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: \"%s\"\n", __func__, buff);
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (!info->enabled) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	cmd_get_chip_vendor(sec);
	cmd_get_chip_name(sec);
	cmd_get_fw_ver_bin(sec);
	cmd_get_fw_ver_ic(sec);

	cmd_run_test_cm(sec);
	if (info->dtdata->no_vsync) {
		cmd_run_test_cm_h_gap(sec);
		cmd_run_test_cm_v_gap(sec);
	} else {
		mms_gap_data(sec);
	}

	cmd_run_test_cm_jitter(sec);
	cmd_run_test_cp(sec);
	cmd_run_test_cp_short(sec);
	cmd_run_test_cp_lpm(sec);
	cmd_check_gpio(sec);
	cmd_check_wet_mode(sec);

	if (!info->dtdata->no_vsync)
		cmd_run_test_vsync(sec);

	mms_reboot(info);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &info->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
static void tui_mode_cmd(struct mms_ts_info *info);
#endif

/**
 * List of command functions
 */
static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", cmd_fw_update),},
	{SEC_CMD("get_fw_ver_bin", cmd_get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", cmd_get_fw_ver_ic),},
	{SEC_CMD("get_chip_vendor", cmd_get_chip_vendor),},
	{SEC_CMD("get_chip_name", cmd_get_chip_name),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("get_crc_check", cmd_get_crc_check),},
	{SEC_CMD("get_x_num", cmd_get_x_num),},
	{SEC_CMD("get_y_num", cmd_get_y_num),},
	{SEC_CMD("get_max_x", cmd_get_max_x),},
	{SEC_CMD("get_max_y", cmd_get_max_y),},
	{SEC_CMD("module_off_master", cmd_module_off_master),},
	{SEC_CMD("module_on_master", cmd_module_on_master),},
	{SEC_CMD("run_cm_read", cmd_run_test_cm),},
	{SEC_CMD("run_cm_read_all", cmd_run_test_cm_all),},
	{SEC_CMD("run_cm_h_gap_read", cmd_run_test_cm_h_gap),},
	{SEC_CMD("run_cm_h_gap_read_all", cmd_run_test_cm_h_gap_all),},
	{SEC_CMD("run_cm_v_gap_read", cmd_run_test_cm_v_gap),},
	{SEC_CMD("run_cm_v_gap_read_all", cmd_run_test_cm_v_gap_all),},
	{SEC_CMD("run_cm_jitter_read", cmd_run_test_cm_jitter),},
	{SEC_CMD("run_cm_jitter_read_all", cmd_run_test_cm_jitter_all),},
	{SEC_CMD("run_cp_read", cmd_run_test_cp),},
	{SEC_CMD("run_cp_read_all", cmd_run_test_cp_all),},
	{SEC_CMD("run_cp_short_read", cmd_run_test_cp_short),},
	{SEC_CMD("run_cp_short_read_all", cmd_run_test_cp_short_all),},
	{SEC_CMD("run_cp_lpm_read", cmd_run_test_cp_lpm),},
	{SEC_CMD("run_cp_lpm_read_all", cmd_run_test_cp_lpm_all),},
	{SEC_CMD("gap_data_h_all", cmd_mms_gap_data_h_all),},
	{SEC_CMD("gap_data_v_all", cmd_mms_gap_data_v_all),},
	{SEC_CMD("run_prox_intensity_read_all", cmd_run_prox_intensity_read_all),},
	{SEC_CMD("run_vsync_read", cmd_run_test_vsync),},
	{SEC_CMD("run_cs_delta_read_all", run_cs_delta_read_all),},
	{SEC_CMD("get_config_ver", cmd_get_config_ver),},
	{SEC_CMD("get_threshold", cmd_get_threshold),},
	{SEC_CMD("check_gpio", cmd_check_gpio),},
	{SEC_CMD("wet_mode", cmd_check_wet_mode),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
#ifdef GLOVE_MODE
	{SEC_CMD_H("glove_mode", glove_mode),},
#endif
#ifdef COVER_MODE
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
#endif
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD("fod_enable", fod_enable),},
	{SEC_CMD_H("fod_lp_mode", fod_lp_mode),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_aod_rect", get_aod_rect),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},	
	{SEC_CMD("not_support_cmd", cmd_unknown_cmd),},
};

static ssize_t read_multi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__, info->multi_count);

	return snprintf(buf, PAGE_SIZE, "%d", info->multi_count);
}

static ssize_t clear_multi_count_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	info->multi_count = 0;
	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_comm_err_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__, info->comm_err_count);

	return snprintf(buf, PAGE_SIZE, "%d", info->comm_err_count);
}

static ssize_t clear_comm_err_count_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	info->comm_err_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_module_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	return snprintf(buf, PAGE_SIZE, "ME%04x%04x",
		info->fw_model_ver_ic, info->fw_ver_ic);
}

static ssize_t read_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "MELFAS");
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	
	if (mms_get_image(info, MIP_IMG_TYPE_5POINT_INTENSITY)) {
		input_err(true, &info->client->dev, "%s: mms_get_image fail!\n", __func__);
		return -1;
	}

	input_info(true, &info->client->dev, "%s: %s\n", __func__, info->print_buf);
		
	return snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{

	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);

	u8 wbuf[64];
	int ret;
	unsigned long value = 0;

	wbuf[0] = MIP_R0_CTRL;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	input_err(true, &info->client->dev, "%s: enable:%d\n", __func__, value);
	
	if (value == 1) {
		wbuf[1] = MIP_R1_CTRL_NP_ACTIVE_MODE;
		wbuf[2] = 1;
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s: send sensitivity mode on fail!\n", __func__);
			return ret;
		}

		wbuf[1] = MIP_R1_CTRL_5POINT_TEST_MODE;
		wbuf[2] = 1;
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s: send sensitivity mode on fail!\n", __func__);
			return ret;
		}
		input_info(true, &info->client->dev, "%s: enable end\n", __func__);
	} else {
		wbuf[1] = MIP_R1_CTRL_NP_ACTIVE_MODE;
		wbuf[2] = 0;
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s: send sensitivity mode off fail!\n", __func__);
			return ret;
		}

		wbuf[1] = MIP_R1_CTRL_5POINT_TEST_MODE;
		wbuf[2] = 0;
		if (mms_i2c_write(info, wbuf, 3)) {
			input_err(true, &info->client->dev, "%s: send sensitivity mode off fail!\n", __func__);
			return ret;
		}
		input_info(true, &info->client->dev, "%s: disable end\n", __func__);
	}
	input_info(true, &info->client->dev, "%s: done\n", __func__);

	return count;
}

static ssize_t get_lp_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct mms_ts_info *info = container_of(sec, struct mms_ts_info, sec);
	u8 string_data[10] = {0, };
	u16 current_index;
	u8 dump_format, dump_num;
	u16 dump_start, dump_end;
	int i, ret;

	if (info->ic_status == PWR_OFF) {
		input_err(true, &info->client->dev, "%s: Touch is stopped!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	input_info(true, &info->client->dev, "%s : start\n", __func__);

	disable_irq(info->client->irq);

	ret = sponge_read(info, SPONGE_LP_DUMP_REG_ADDR, string_data, 4);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to read rect\n", __func__);
		snprintf(buf, SEC_CMD_STR_LEN, "NG, Failed to read rect");
		goto exit;
	}
	dump_format = string_data[0];
	dump_num = string_data[1];
	dump_start = SPONGE_LP_DUMP_REG_ADDR + 4;
	dump_end = dump_start + (dump_format * (dump_num - 1));

	current_index = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &info->client->dev, "Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_STR_LEN,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto exit;
	}

	input_info(true, &info->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
				__func__, dump_format, dump_num, dump_start, dump_end, current_index);

	for (i = dump_num - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 string_addr;

		if (current_index < (dump_format * i))
			string_addr = (dump_format * dump_num) + current_index - (dump_format * i);
		else
			string_addr = current_index - (dump_format * i);

		if (string_addr < dump_start)
			string_addr += (dump_format * dump_num);

		ret = sponge_read(info, string_addr, string_data, 10);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: Failed to read rect\n", __func__);
			snprintf(buf, SEC_CMD_STR_LEN,
					"NG, Failed to read rect, addr=%d",
					string_addr);
			goto exit;
		}

		data0 = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
		data1 = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
		data2 = (string_data[5] & 0xFF) << 8 | (string_data[4] & 0xFF);
		data3 = (string_data[7] & 0xFF) << 8 | (string_data[6] & 0xFF);
		data4 = (string_data[9] & 0xFF) << 8 | (string_data[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (dump_format == 10) {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3, data4);
			} else {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3);
			}
			strlcat(buf, buff, PAGE_SIZE);
		}
	}

exit:
	enable_irq(info->client->irq);

	input_info(true, &info->client->dev, "%s : end\n", __func__);

	return strlen(buf);
}

static DEVICE_ATTR(get_lp_dump, S_IRUGO, get_lp_dump_show, NULL);
static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_position_show, NULL);
static DEVICE_ATTR(multi_count, S_IRUGO | S_IWUSR | S_IWGRP, read_multi_count_show, clear_multi_count_store);
static DEVICE_ATTR(comm_err_count, S_IRUGO | S_IWUSR | S_IWGRP, read_comm_err_count_show, clear_comm_err_count_store);
static DEVICE_ATTR(module_id, S_IRUGO, read_module_id_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, read_vendor_show, NULL);
static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(ear_detect_enable, 0664, ear_detect_enable_show, ear_detect_enable_store);
static DEVICE_ATTR(fod_pos, 0444, fod_position_show, NULL);
static DEVICE_ATTR(fod_info, 0444, fod_info_show, NULL);

/**
 * Sysfs - cmd attr info
 */
static struct attribute *mms_cmd_attr[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_multi_count.attr,
	&dev_attr_comm_err_count.attr,
	&dev_attr_module_id.attr,
	&dev_attr_vendor.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_ear_detect_enable.attr,
	&dev_attr_fod_pos.attr,
	&dev_attr_fod_info.attr,
	NULL,
};

/**
 * Sysfs - cmd attr group info
 */
static const struct attribute_group cmd_attr_group = {
	.attrs = mms_cmd_attr,
};

/**
 * Create sysfs command functions
 */
int mms_sysfs_cmd_create(struct mms_ts_info *info)
{
	int retval;

	info->print_buf = kzalloc(sizeof(u8) * 4096, GFP_KERNEL);	
	info->image_buf =
		kzalloc(sizeof(int) * ((info->node_x * info->node_y) + info->node_key),
			GFP_KERNEL);

	retval = sec_cmd_init(&info->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&info->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		goto exit;
	}

	retval = sysfs_create_link(&info->sec.fac_dev->kobj,
			&info->input_dev->dev.kobj, "input");
	if (retval < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to create input symbolic link\n",
				__func__);
		goto exit;
	}

	return 0;
exit:
	return retval;	
}

/**
 * Remove sysfs command functions
 */
void mms_sysfs_cmd_remove(struct mms_ts_info *info)
{
	input_err(true, &info->client->dev, "%s\n", __func__);

	sysfs_delete_link(&info->sec.fac_dev->kobj, &info->input_dev->dev.kobj, "input");

	sysfs_remove_group(&info->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);

	kfree(info->print_buf);
	kfree(info->image_buf);
}

#endif
