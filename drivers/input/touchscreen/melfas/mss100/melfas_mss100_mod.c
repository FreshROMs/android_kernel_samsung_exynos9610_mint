/*
 * MELFAS MMS400 Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 *
 * Model dependent functions
 *
 */

#include "melfas_mss100.h"

#ifdef USE_TSP_TA_CALLBACKS
static bool ta_connected;
#endif

/**
 * Control power supply
 */
int mms_power_control(struct mms_ts_info *info, int enable)
{
	int ret = 0;
	struct i2c_client *client = info->client;
	struct regulator *regulator_dvdd = NULL;
	struct regulator *regulator_avdd = NULL;
	struct pinctrl_state *pinctrl_state;
	static bool on;

	input_info(true, &info->client->dev, "%s [START %s]\n",
			__func__, enable ? "on":"off");

	if (on == enable) {
		input_err(true, &client->dev, "%s : TSP power already %s\n",
			__func__, (on) ? "on":"off");
		return ret;
	}

	if (info->dtdata->gpio_io_en) {
		regulator_dvdd = regulator_get(NULL, info->dtdata->gpio_io_en);
		if (IS_ERR_OR_NULL(regulator_dvdd)) {
			input_info(true, &client->dev, "%s: Failed to get %s regulator.\n",
				 __func__, info->dtdata->gpio_io_en);
			ret = PTR_ERR(regulator_dvdd);
			goto out;
		}
	}

	regulator_avdd = regulator_get(NULL, info->dtdata->gpio_vdd_en);
	if (IS_ERR_OR_NULL(regulator_avdd)) {
		input_info(true, &client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, info->dtdata->gpio_vdd_en);
		ret = PTR_ERR(regulator_avdd);
		goto out;
	}

	if (enable) {
		ret = regulator_enable(regulator_avdd);
		if (ret) {
			input_info(true, &client->dev, "%s: Failed to enable avdd: %d\n", __func__, ret);
			goto out;
		}
		if (info->dtdata->gpio_io_en) {
			ret = regulator_enable(regulator_dvdd);
			if (ret) {
				input_info(true, &client->dev, "%s: Failed to enable vdd: %d\n", __func__, ret);
				goto out;
			}
		}
		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "on_state");
	} else {
		if (info->dtdata->gpio_io_en) {
			if (regulator_is_enabled(regulator_dvdd))
				regulator_disable(regulator_dvdd);
		}
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "off_state");
	}

	if (IS_ERR_OR_NULL(pinctrl_state)) {
		input_info(true, &client->dev, "%s: Failed to lookup pinctrl.\n", __func__);
	} else {
		ret = pinctrl_select_state(info->pinctrl, pinctrl_state);
		if (ret)
			input_info(true, &client->dev, "%s: Failed to configure pinctrl.\n", __func__);
	}

	on = enable;
out:
	if (info->dtdata->gpio_io_en && !IS_ERR_OR_NULL(regulator_dvdd))
		regulator_put(regulator_dvdd);
	if (!IS_ERR_OR_NULL(regulator_avdd))
		regulator_put(regulator_avdd);

	if (!enable)
		usleep_range(10 * 1000, 11 * 1000);
	else
		msleep(90);

	input_info(true, &info->client->dev, "%s [DONE %s]\n",
			__func__, enable ? "on":"off");
	return ret;
}

/**
 * Clear touch input events
 */
void mms_clear_input(struct mms_ts_info *info)
{
	int i;

	input_info(true, &info->client->dev, "%s\n", __func__);

	input_report_key(info->input_dev, BTN_TOUCH, 0);
	input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

	for (i = 0; i < MAX_FINGER_NUM; i++) {
		info->finger_state[i] = 0;
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, false);
		info->coord[i].mcount = 0;
	}

	info->touch_count = 0;
	info->check_multi = 0;
	info->print_info_cnt_release = 0;

	input_sync(info->input_dev);
}

int mms_set_custom_library(struct mms_ts_info *info, u16 addr, u8 *buf, u8 len)
{
	int ret = 0;
	u8 wbuf[3];

	mutex_lock(&info->sponge_mutex);

	ret = sponge_write(info, addr, buf, len);
	if (ret < 0)
		goto exit;
	
	wbuf[0] =(u8)((MIP_LIB_ADDR_SYNC >> 8) & 0xFF);
	wbuf[1] =(u8)(MIP_LIB_ADDR_SYNC & 0xFF);
	wbuf[2] = 1;

	if (mms_i2c_write(info, wbuf, 3)) {
	  input_err(true,&info->client->dev, "%s [ERROR] mms_i2c_write\n",__func__);
	  ret = -1;
	  goto exit;
	}

exit:
	mutex_unlock(&info->sponge_mutex);
	return ret;
}

int sponge_read(struct mms_ts_info *info, u16 addr, u8 *buf, u8 len)
{
	int ret = 0;
	u8 rbuf[4] = {0, };
	u16 mip4_addr = 0;

	mutex_lock(&info->sponge_mutex);

	mip4_addr = MIP_LIB_ADDR_START + addr;
	if (mip4_addr > MIP_LIB_ADDR_END) {
		input_err(true, &info->client->dev, "%s [ERROR] sponge addr range\n", __func__);
		ret = -1;
		goto exit;
	}

	rbuf[0] = (u8)((mip4_addr >> 8) & 0xFF);
	rbuf[1] = (u8)(mip4_addr & 0xFF);
	if (mms_i2c_read(info, rbuf, 2, buf, len)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_read\n", __func__);
		ret = -1;
	}

exit:
	mutex_unlock(&info->sponge_mutex);
	return ret;
}

int sponge_write(struct mms_ts_info *info, u16 addr, u8 *buf, u8 len)
{
	int ret = 0;
	u8 *wbuf;
	u16 mip4_addr = 0;

	mip4_addr = MIP_LIB_ADDR_START + addr;
	if (mip4_addr > MIP_LIB_ADDR_END) {
		input_err(true, &info->client->dev, "%s [ERROR] sponge addr range\n", __func__);
		ret = -1;
		goto exit;
	}

	wbuf = kzalloc(sizeof(u8) * (2 + len), GFP_KERNEL);

	wbuf[0] = (u8)((mip4_addr >> 8) & 0xFF);
	wbuf[1] = (u8)(mip4_addr & 0xFF);
	memcpy(&wbuf[2], buf, len);

	if (mms_i2c_write(info, wbuf, 2 + len)) {
		input_err(true, &info->client->dev, "%s [ERROR] mms_i2c_write\n", __func__);
		ret = -1;
	}

	kfree(wbuf);

exit:
	return ret;
}

void mip4_ts_sponge_write_time(struct mms_ts_info *info, u32 val)
{
	int ret = 0;
	u8 data[4];

	input_info(true, &info->client->dev, "%s - time[%u]\n", __func__, val);

	data[0] = (val >> 0) & 0xFF; /* Data */
	data[1] = (val >> 8) & 0xFF; /* Data */
	data[2] = (val >> 16) & 0xFF; /* Data */
	data[3] = (val >> 24) & 0xFF; /* Data */

	ret = mms_set_custom_library(info, SPONGE_UTC_OFFSET, data, 4);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s [ERROR] sponge_write\n", __func__);
		return;
	}
}

/************************************************************
*  720  * 1480 : <48 96 60> indicator: 24dp navigator:48dp edge:60px dpi=320
* 1080  * 2220 :  4096 * 4096 : <133 266 341>  (approximately value)
************************************************************/
void mms_ts_location_detect(struct mms_ts_info *info, char *loc, int x, int y)
{
	int i;
	for (i = 0 ; i < 6 ; ++i){
		loc[i] = 0;
	}

	if (x < info->dtdata->area_edge)
		strcat(loc, "E.");
	else if (x < (info->max_x - info->dtdata->area_edge))
		strcat(loc, "C.");
	else
		strcat(loc, "e.");

	if (y < info->dtdata->area_indicator)
		strcat(loc, "S");
	else if (y < (info->max_y - info->dtdata->area_navigation))
		strcat(loc, "C");
	else
		strcat(loc, "N");
}

static const char finger_mode[10] = { 'N', 'P' };
/**
 * Input event handler - Report touch input event
 */
void mms_input_event_handler(struct mms_ts_info *info, u8 sz, u8 *buf)
{
	struct i2c_client *client = info->client;
	int i;
	int id;
	int hover = 0;
	int palm = 0;
	int state = 0;
	int x, y;
	int z = 0;
	int size = 0;
	int pressure_stage = 0;
	int pressure = 0;
	int touch_major = 0;
	int touch_minor = 0;
	char location[6] = { 0, };
	char pos[5];

	input_dbg(false, &client->dev, "%s [START]\n", __func__);
	input_dbg(false, &client->dev, "%s - sz[%d] buf[0x%02X]\n", __func__, sz, buf[0]);

	for (i = 0; i < sz; i += info->event_size) {
		u8 *packet = &buf[i];
		int type;

		/* Event format & type */
		switch (info->event_format) {
		case EVENT_FORMAT_BASIC:
		case EVENT_FORMAT_WITH_RECT:
			type = (packet[0] & 0x40) >> 6;
			break;
		case EVENT_FORMAT_WITH_PRESSURE:
		case EVENT_FORMAT_WITH_PRESSURE_2BYTE:
			type = (packet[0] & 0xF0) >> 4;
			break;
		case EVENT_FORMAT_KEY_ONLY:
			type = MIP4_EVENT_INPUT_TYPE_KEY;
			break;
		default:
			input_dbg(true, &client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
			goto error;
		}

		input_dbg(false, &client->dev, "%s - Type[%d]\n", __func__, type);

		/* Report input event */
		if (type == MIP4_EVENT_INPUT_TYPE_SCREEN) {
			/* Screen event */
			switch (info->event_format) {
			case EVENT_FORMAT_BASIC:
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[5];
				touch_minor = packet[5];
				break;
			case EVENT_FORMAT_WITH_RECT:
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[6];
				touch_minor = packet[7];
				break;
			case EVENT_FORMAT_WITH_PRESSURE:
				id = (packet[0] & 0x0F) - 1;
				hover = (packet[1] & 0x04) >> 2;
				palm = (packet[1] & 0x02) >> 1;
				state = (packet[1] & 0x01);
				x = ((packet[2] & 0x0F) << 8) | packet[3];
				y = (((packet[2] >> 4) & 0x0F) << 8) | packet[4];
				z = packet[5];
				size = packet[6];
				pressure_stage = (packet[7] & 0xF0) >> 4;
				pressure = ((packet[7] & 0x0F) << 8) | packet[8];
				touch_major = packet[9];
				touch_minor = packet[10];
				break;
			case EVENT_FORMAT_WITH_PRESSURE_2BYTE:
				id = (packet[0] & 0x0F) - 1;
				hover = (packet[1] & 0x04) >> 2;
				palm = (packet[1] & 0x02) >> 1;
				state = (packet[1] & 0x01);
				x = ((packet[2] & 0x0F) << 8) | packet[3];
				y = (((packet[2] >> 4) & 0x0F) << 8) | packet[4];
				z = (packet[6] << 8) | packet[5];
				size = packet[7];
				pressure_stage = (packet[8] & 0xF0) >> 4;
				pressure = ((packet[8] & 0x0F) << 8) | packet[9];
				touch_major = packet[10];
				touch_minor = packet[11];
				break;
			default:
				input_err(true, &client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
				goto error;
			}

			info->coord[id].action = state;
			info->coord[id].x = x;
			info->coord[id].y = y;
			info->coord[id].z = z;
			info->coord[id].major = touch_major;
			info->coord[id].minor = touch_minor;
			info->coord[id].palm = palm;
			info->coord[id].type = palm;

			if (state == MMS_TS_COORDINATE_ACTION_RELEASE) {
				/* Release */
				input_mt_slot(info->input_dev, id);
#ifdef CONFIG_SEC_FACTORY
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, 0);
#endif
				input_mt_report_slot_state(info->input_dev,
								MT_TOOL_FINGER, false);
				if (info->finger_state[id] != 0) {
					info->touch_count--;
					if (!info->touch_count) {
						input_report_key(info->input_dev, BTN_TOUCH, 0);
						input_report_key(info->input_dev,
									BTN_TOOL_FINGER, 0);
						info->check_multi = 0;
						info->print_info_cnt_release = 0;
					}
					info->finger_state[id] = 0;

					mms_ts_location_detect(info, location, info->coord[id].x, info->coord[id].y);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
					input_info(true, &info->client->dev,
							"[R] tID:%d loc:%s dd:%d,%d mc:%d tc:%d | ed:%d\n",
							id, location,
							info->coord[id].x - info->coord[id].p_x,
							info->coord[id].y - info->coord[id].p_y,
							info->coord[id].mcount, info->touch_count,
							info->ed_enable);

#else
					input_info(true, &info->client->dev,
							"[R] tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d | ed:%d\n",
							id, location,
							info->coord[id].x - info->coord[id].p_x,
							info->coord[id].y - info->coord[id].p_y,
							info->coord[id].mcount, info->touch_count,
							info->coord[id].x, info->coord[id].y,
							info->ed_enable);
#endif
					info->coord[id].mcount = 0;
				}

				continue;
			} else if (state == MMS_TS_COORDINATE_ACTION_PRESS_MOVE) {
				/* Press or Move */
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, true);
				input_report_key(info->input_dev, BTN_TOUCH, 1);
				input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
				input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
#ifdef CONFIG_SEC_FACTORY
				if (pressure)
					input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
				else
					input_report_abs(info->input_dev, ABS_MT_PRESSURE, 1);
#endif
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, touch_minor);
				input_report_abs(info->input_dev, ABS_MT_PALM, palm);
				if (info->finger_state[id] == 0) {
					info->finger_state[id] = 1;
					info->touch_count++;

					info->coord[id].p_x = x;
					info->coord[id].p_y = y;

					mms_ts_location_detect(info, location, info->coord[id].x, info->coord[id].y);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
					input_info(true, &info->client->dev,
							"[P] tID:%d.%d z:%d major:%d minor:%d loc:%s tc:%d\n",
							id, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
							info->coord[id].z,
							info->coord[id].major, info->coord[id].minor,
							location, info->touch_count);

#else
					input_info(true, &info->client->dev,
							"[P] tID:%d.%d x:%d y:%d z:%d major:%d minor:%d loc:%s tc:%d\n",
							id, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
							info->coord[id].x, info->coord[id].y, info->coord[id].z,
							info->coord[id].major, info->coord[id].minor,
							location, info->touch_count);
#endif
					if ((info->touch_count > 2) && (info->check_multi == 0)) {
						info->check_multi = 1;
						info->multi_count++;
					}
				}
				info->coord[id].mcount++;
			}

			if (state == MMS_TS_COORDINATE_ACTION_RELEASE)
				snprintf(pos, 5, "R");
			if (state == MMS_TS_COORDINATE_ACTION_PRESS_MOVE) {
				if (info->finger_state[id] == 0)
					snprintf(pos, 5, "P");
				else
					snprintf(pos, 5, "M");
			}

			if (info->coord[id].pre_type != info->coord[id].type)
				input_info(true, &info->client->dev, "%s: tID:%d ttype(%c->%c) : %s\n",
						__func__, id, finger_mode[info->coord[id].pre_type],
						finger_mode[info->coord[id].type], pos);

			info->coord[id].pre_type = info->coord[id].type;

		} else if (type == MIP4_EVENT_INPUT_TYPE_KEY) {
			int key_code;

			switch (info->event_format) {
			case EVENT_FORMAT_BASIC:
			case EVENT_FORMAT_WITH_RECT:
				id = (packet[0] & 0x0F) - 1;
				state = (packet[0] & 0x80) >> 7;
				break;
			case EVENT_FORMAT_WITH_PRESSURE:
			case EVENT_FORMAT_WITH_PRESSURE_2BYTE:
				id = (packet[0] & 0x0F) - 1;
				state = (packet[1] & 0x01);
				break;
			default:
				input_err(true, &client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
				goto error;
			}

			if (id == 1)
				key_code = KEY_MENU;
			else if (id == 2)
				key_code = KEY_BACK;

			if (id == 1 || id ==2) {
				input_report_key(info->input_dev, key_code, state);
				input_dbg(false, &client->dev, "%s - Key : ID[%d] Code[%d] State[%d]\n",
					__func__, id, key_code, state);
			}
		} else if (type == MIP4_EVENT_INPUT_TYPE_PROXIMITY) {
			int hover_id;
			int hover_state = 0;

			for (hover_id = 1; hover_id < 4; hover_id++) {
				if (packet[1] & (0x01 << (hover_id + 1)))
					hover_state = hover_id;
			}

			if (info->dtdata->support_ear_detect && info->ed_enable) {
				if (info->ic_status >= LP_MODE) {
					input_info(true, &client->dev, "%s: LPM : SKIP HOVER DETECT(%d)\n", __func__, hover_state);
				} else {
					input_info(true, &client->dev, "%s: HOVER DETECT(%d)\n", __func__, hover_state);
					input_report_abs(info->input_dev_proximity, ABS_MT_CUSTOM, hover_state);
					input_sync(info->input_dev_proximity);
				}
			}
		}
	}

	input_sync(info->input_dev);
	input_dbg(false, &client->dev, "%s [DONE]\n", __func__);
error:
	return;
}

/*
 * Event handler
 */
int mms_custom_event_handler(struct mms_ts_info *info, u8 *rbuf, u8 size)
{
	int ret = 0;
	u8 s_feature = 0;
	u8 event_id = 0;
	u8 gesture_type = 0;
	u8 gesture_id = 0;
	u8 gesture_data[4] = {0, };
	u8 left_event = 0;

	input_dbg(false, &info->client->dev, "%s [START]\n", __func__);

	s_feature = (rbuf[2] >> 6) & 0x03;
	gesture_type = (rbuf[2] >> 2) & 0x0F;
	event_id = rbuf[2] & 0x03;
	gesture_id = rbuf[3];
	gesture_data[0] = rbuf[4];
	gesture_data[1] = rbuf[5];
	gesture_data[2] = rbuf[6];
	gesture_data[3] = rbuf[7];
	left_event = rbuf[9] & 0x3F;

	input_dbg(false, &info->client->dev, "%s - sf[%u] eid[%u] left[%u]\n", __func__, s_feature, event_id, left_event);
	input_info(true, &info->client->dev, "%s - gesture type[%u] id[%u] data[0x%02X 0x%02X 0x%02X 0x%02X]\n", __func__, gesture_type, gesture_id, gesture_data[0], gesture_data[1], gesture_data[2], gesture_data[3]);

	if (s_feature) {
		/* Samsung */
		if (gesture_type == MMS_GESTURE_CODE_SWIPE) {
			/* Swipe up */
			if (gesture_id == 0) {
				info->scrub_id = SPONGE_EVENT_TYPE_SPAY;
				input_info(true, &info->client->dev, "%s: SPAY: %d\n", __func__, info->scrub_id);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
			}
		} else if (gesture_type == MMS_GESTURE_CODE_DOUBLE_TAP) {
			if (gesture_id == MMS_GESTURE_ID_AOD) {
				info->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;
				info->scrub_x = (gesture_data[0] << 4)|(gesture_data[2] >> 4);
				info->scrub_y = (gesture_data[1] << 4)|(gesture_data[2] & 0x0F);
				input_info(true, &info->client->dev, "%s - AOD: id[%d] x[%d] y[%d]\n",
									__func__, info->scrub_id, info->scrub_x, info->scrub_y);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
			} else if (gesture_id == MMS_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
				input_info(true, &info->client->dev, "%s: AOT\n", __func__);
				input_report_key(info->input_dev, KEY_HOMEPAGE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_HOMEPAGE, 0);
			}
		} else if (gesture_type == MMS_GESTURE_CODE_SINGLE_TAP) {
			info->scrub_id = SPONGE_EVENT_TYPE_SINGLE_TAP;
			info->scrub_x = (gesture_data[0] << 4)|(gesture_data[2] >> 4);
			info->scrub_y = (gesture_data[1] << 4)|(gesture_data[2] & 0x0F);
			input_info(true, &info->client->dev, "%s: SINGLE TAP: %d\n", __func__, info->scrub_id);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
			input_sync(info->input_dev);
		} else if (gesture_type == MMS_GESTURE_CODE_PRESS) {
			if (gesture_id == MMS_GESTURE_ID_FOD_LONG || gesture_id == MMS_GESTURE_ID_FOD_NORMAL) {
				info->scrub_id = SPONGE_EVENT_TYPE_FOD;
				input_info(true, &info->client->dev, "%s: FOD: %s\n", __func__, gesture_id ? "normal" : "long");
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
			} else if (gesture_id == MMS_GESTURE_ID_FOD_RELEASE) {
				info->scrub_id = SPONGE_EVENT_TYPE_FOD_RELEASE;
				input_info(true, &info->client->dev, "%s: FOD release\n", __func__);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
			} else if (gesture_id == MMS_GESTURE_ID_FOD_OUT) {
				info->scrub_id = SPONGE_EVENT_TYPE_FOD_OUT;
				input_info(true, &info->client->dev, "%s: FOD OUT\n", __func__);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
			}
		}
	}

	input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
	input_sync(info->input_dev);

	input_dbg(false, &info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

#if MMS_USE_DEVICETREE
/**
 * Parse device tree
 */
int mms_parse_devicetree(struct device *dev, struct mms_ts_info *info)
{
	struct device_node *np = dev->of_node;
	u32 px_zone[3] = { 0 };
	u32 tmp[3] = { 0 };

	input_info(true, dev, "%s [START]\n", __func__);

	info->dtdata->gpio_intr = of_get_named_gpio(np, "melfas,irq-gpio", 0);
	gpio_request(info->dtdata->gpio_intr, "irq-gpio");
	gpio_direction_input(info->dtdata->gpio_intr);
	info->client->irq = gpio_to_irq(info->dtdata->gpio_intr);

	info->dtdata->gpio_scl = of_get_named_gpio(np, "melfas,scl-gpio", 0);
	gpio_request(info->dtdata->gpio_scl, "melfas_scl_gpio");
	info->dtdata->gpio_sda = of_get_named_gpio(np, "melfas,sda-gpio", 0);
	gpio_request(info->dtdata->gpio_sda, "melfas_sda_gpio");

	if (of_property_read_string(np, "melfas,vdd_en", &info->dtdata->gpio_vdd_en))
		input_err(true, dev,  "Failed to get regulator_dvdd name property\n");

	if (of_property_read_string(np, "melfas,io_en", &info->dtdata->gpio_io_en)) {
		input_err(true, dev, "Failed to get regulator_avdd name property\n");
		info->dtdata->gpio_io_en = NULL;
	}

	if (of_property_read_u32_array(np, "melfas,max_x_y", tmp, 2)){
		input_info(true, dev, "Failed to get max_x_y\n");
	} else {
		info->dtdata->max_x = tmp[0];
		info->dtdata->max_y = tmp[1];
	}

	if (of_property_read_u32_array(np, "melfas,node_info", tmp, 3)){
		input_info(true, dev, "Failed to get node_info\n");
	} else {
		info->dtdata->node_x = tmp[0];
		info->dtdata->node_y = tmp[1];
		info->dtdata->node_key = tmp[2];
	}

	if (of_property_read_u32_array(np, "melfas,event_info", tmp, 2)){
		input_info(true, dev, "Failed to get event_info\n");
	} else {
		info->dtdata->event_format = tmp[0];
		info->dtdata->event_size = tmp[1];
	}
	input_info(true, dev, "%s : max_x:%d, max_y:%d, node_x:%d, node_y:%d, node_key:%d, event_format:%d, event_size:%d\n",
		__func__, info->dtdata->max_x, info->dtdata->max_y, info->dtdata->node_x, info->dtdata->node_y,
		info->dtdata->node_key, info->dtdata->event_format, info->dtdata->event_size);

	if (of_property_read_u32_array(np, "melfas,fod_info", tmp, 3)){
		input_info(true, dev, "Failed to get fod_info\n");
	} else {
		info->dtdata->fod_tx = tmp[0];
		info->dtdata->fod_rx = tmp[1];
		info->dtdata->fod_vi_size= tmp[2];
	}

	input_info(true, dev, "%s : fod_tx:%d, fod_rx:%d, fod_vi_size:%d\n",
		__func__, info->dtdata->fod_tx, info->dtdata->fod_rx, info->dtdata->fod_vi_size);

	if (of_property_read_u32(np, "melfas,bringup", &info->dtdata->bringup) < 0)
		info->dtdata->bringup = 0;

	if (of_property_read_string(np, "melfas,fw_name", &info->dtdata->fw_name))
		input_err(true, dev, "Failed to get fw_name property\n");

	info->dtdata->support_lpm = of_property_read_bool(np, "melfas,support_lpm");
	info->dtdata->support_ear_detect = of_property_read_bool(np, "support_ear_detect_mode");
	info->dtdata->support_fod = of_property_read_bool(np, "support_fod");	
	info->dtdata->enable_settings_aot = of_property_read_bool(np, "enable_settings_aot");
	info->dtdata->sync_reportrate_120 = of_property_read_bool(np, "sync-reportrate-120");
	info->dtdata->no_vsync = of_property_read_bool(np, "melfas,no_vsync");

	if (of_property_read_u32_array(np, "melfas,area-siz", px_zone, 3)){
		input_info(true, dev, "Failed to get zone's size\n");
		info->dtdata->area_indicator = 133;
		info->dtdata->area_navigation = 266;
		info->dtdata->area_edge = 341;
	} else {
		info->dtdata->area_indicator = px_zone[0];
		info->dtdata->area_navigation = px_zone[1];
		info->dtdata->area_edge = px_zone[2];
	}
	input_info(true, dev, "%s : zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, info->dtdata->area_indicator, info->dtdata->area_navigation ,info->dtdata->area_edge);

	input_info(true, dev, "%s: fw_name %s int:%d irq:%d sda:%d scl:%d support_LPM:%d AOT:%d FOD:%d ED:%d\n",
		__func__, info->dtdata->fw_name, info->dtdata->gpio_intr, info->client->irq, info->dtdata->gpio_sda,
		info->dtdata->gpio_scl, info->dtdata->support_lpm, info->dtdata->enable_settings_aot,
		info->dtdata->support_fod, info->dtdata->support_ear_detect);

	return 0;
}
#endif

/**
 * Config input interface
 */
void mms_config_input(struct mms_ts_info *info)
{
	struct input_dev *input_dev = info->input_dev;

	input_dbg(false, &info->client->dev, "%s [START]\n", __func__);

	//Screen
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);

	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	set_bit(KEY_INT_CANCEL, input_dev->keybit);

	input_mt_init_slots(input_dev, MAX_FINGER_NUM, INPUT_MT_DIRECT);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->max_y, 0, 0);
#ifdef CONFIG_SEC_FACTORY
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, INPUT_PRESSURE_MAX, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, INPUT_TOUCH_MAJOR_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, INPUT_TOUCH_MINOR_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PALM, 0, 1, 0, 0);

	//Key
	set_bit(EV_KEY, input_dev->evbit);
#if MMS_USE_TOUCHKEY
	set_bit(KEY_BACK, input_dev->keybit);
	set_bit(KEY_MENU, input_dev->keybit);
#endif
#if MMS_USE_NAP_MODE
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_POWER, input_dev->keybit);
#endif
	set_bit(KEY_HOMEPAGE, input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, input_dev->keybit);
	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
}

#ifdef CONFIG_VBUS_NOTIFIER
int mms_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	struct mms_ts_info *info = container_of(nb, struct mms_ts_info, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *)data;

	input_info(true, &info->client->dev, "%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		input_info(true, &info->client->dev, "%s : attach\n", __func__);
		info->ta_stsatus = true;
		break;
	case STATUS_VBUS_LOW:
		input_info(true, &info->client->dev, "%s : detach\n", __func__);
		info->ta_stsatus = false;
		break;
	default:
		break;
	}

	if (!info->enabled) {
		input_err(true, &info->client->dev, "%s tsp disabled", __func__);
		return 0;
	}

	mms_charger_attached(info, info->ta_stsatus);
	return 0;
}
#endif

/**
 * Callback - get charger status
 */
#ifdef USE_TSP_TA_CALLBACKS
void mms_charger_status_cb(struct tsp_callbacks *cb, int status)
{
	pr_info("%s: TA %s\n",
		__func__, status ? "connected" : "disconnected");

	if (status)
		ta_connected = true;
	else
		ta_connected = false;

	/* not yet defined functions */
}

void mms_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_info("%s\n", __func__);
}
#endif

int mms_set_power_state(struct mms_ts_info *info, u8 mode)
{
	u8 wbuf[3];
	u8 rbuf[1];

	input_dbg(false, &info->client->dev, "%s [START]\n", __func__);
	input_dbg(false, &info->client->dev, "%s - mode[%u]\n", __func__, mode);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_POWER_STATE;
	wbuf[2] = mode;

	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		return -EIO;
	}

	msleep(20);

	if (mms_i2c_read(info, wbuf, 2, rbuf, 1)) {
		input_err(true, &info->client->dev, "%s [ERROR] read %x %x, rbuf %x\n",
				__func__, wbuf[0], wbuf[1], rbuf[0]);
		return -EIO;
	}

	if (rbuf[0] != mode) {
		input_err(true, &info->client->dev, "%s [ERROR] not changed to %s mode, rbuf %x\n",
				__func__, mode ? "LPM" : "normal", rbuf[0]);
		return -EIO;
	}

	input_dbg(false, &info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

static void mms_set_utc_sponge(struct mms_ts_info *info)
{
	struct timeval current_time;
	u32 time_val = 0;

	do_gettimeofday(&current_time);
	time_val = (u32)current_time.tv_sec;
	mip4_ts_sponge_write_time(info, time_val);
}

int mms_lowpower_mode(struct mms_ts_info *info, u8 on)
{
	int ret;
	u8 wbuf[3];

	if (!info->dtdata->support_lpm) {
		input_err(true, &info->client->dev, "%s not supported\n", __func__);
		return -EINVAL;
	}

	if (on == TO_LOWPOWER_MODE)
		info->ic_status = LP_ENTER;

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_PROX_OFF;
	wbuf[2] = info->prox_power_off;

	if (mms_i2c_write(info, wbuf, 3))
		input_err(true, &info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);

	ret = mms_set_power_state(info, on);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s [ERROR] write power mode %s\n",
				__func__, on ? "LP" : "NP");

	if (on == TO_LOWPOWER_MODE) {
		mms_set_custom_library(info, SPONGE_AOD_ENABLE_OFFSET, &(info->lowpower_flag), 1);
		mms_set_utc_sponge(info);
		info->ic_status = LP_MODE;
	} else {
		info->ic_status = PWR_ON;
	}

	input_info(true, &info->client->dev, "%s: %s mode flag %x  prox power %d\n", __func__,
									on ? "LPM" : "normal", info->lowpower_flag, info->prox_power_off);
	return 0;
}
