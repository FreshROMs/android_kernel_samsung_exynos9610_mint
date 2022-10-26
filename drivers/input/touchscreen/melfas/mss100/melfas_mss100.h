
/*
 * MELFAS MMS400 Touchscreen Driver
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 */

#ifndef __MELFAS_MMS400_H
#define __MELFAS_MMS400_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>
#include <linux/wakelock.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/kernel.h>

#include "melfas_mss100_reg.h"

#include <linux/sec_class.h>
#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/sec_batt.h>
#endif

#ifdef CONFIG_VBUS_NOTIFIER
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/vbus_notifier.h>
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <linux/sec_debug.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
#include <linux/sec_debug.h>
extern struct tsp_dump_callbacks dump_callbacks;
static struct delayed_work *p_ghost_check;
#endif

#include <linux/input/sec_cmd.h>

#ifdef CONFIG_SAMSUNG_TUI
#include "stui_inf.h"
#endif

#ifdef CONFIG_OF
#define MMS_USE_DEVICETREE		1
#else
#define MMS_USE_DEVICETREE		0
#endif

#define GLOVE_MODE
#define COVER_MODE

#define MMS_DEVICE_NAME	"mms_ts"
#define MMS_CONFIG_DATE		{0x00, 0x00, 0x00, 0x00}
#define CHIP_NAME "MSS100"

//Config driver
#define MMS_USE_INPUT_OPEN_CLOSE	1
#define I2C_RETRY_COUNT			3
#define RESET_ON_EVENT_ERROR		0
#define ESD_COUNT_FOR_DISABLE		1
#define MMS_USE_TOUCHKEY		0

#define POWER_ON_DELAY        150 /* ms */
#define POWER_ON_DELAY_ISC    20 /* ms */
#define POWER_OFF_DELAY       10 /* ms */
#define USE_STARTUP_WAITING   0 /* 0 (default) or 1 */
#define STARTUP_TIMEOUT       200 /* ms */

#define TO_TOUCH_MODE		0
#define TO_LOWPOWER_MODE		1

//Features
#define MMS_USE_NAP_MODE		0

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
#define MMS_USE_DEV_MODE		1
#else
#define MMS_USE_DEV_MODE		0
#endif

#define MMS_USE_CMD_MODE		1

//Input value
#define MAX_FINGER_NUM			10
#define INPUT_AREA_MIN			0
#define INPUT_AREA_MAX			255
#define INPUT_PRESSURE_MIN		0
#define INPUT_PRESSURE_MAX		255
#define INPUT_TOUCH_MAJOR_MIN		0
#define INPUT_TOUCH_MAJOR_MAX		255
#define INPUT_TOUCH_MINOR_MIN		0
#define INPUT_TOUCH_MINOR_MAX		255
#define INPUT_ANGLE_MIN			0
#define INPUT_ANGLE_MAX			255
#define INPUT_HOVER_MIN			0
#define INPUT_HOVER_MAX			255
#define INPUT_PALM_MIN			0
#define INPUT_PALM_MAX			1

/* Firmware update */
#define EXTERNAL_FW_PATH		"/sdcard/Firmware/TSP/melfas.bin"
#define FFU_FW_PATH	"ffu_tsp.bin"
#define MMS_USE_AUTO_FW_UPDATE		1
#define MMS_FW_MAX_SECT_NUM		4
#define MMS_FW_UPDATE_DEBUG		0
#define MMS_FW_UPDATE_SECTION		1
#define MMS_EXT_FW_FORCE_UPDATE		1

/* Command mode */
#define CMD_LEN				32
#define CMD_RESULT_LEN			512
#define CMD_RESULT_STR_LEN		(4096 - 1)
#define CMD_RESULT_WORD_LEN			10

/* event format */
#define EVENT_FORMAT_BASIC 			0
#define EVENT_FORMAT_WITH_RECT 			1
#define EVENT_FORMAT_WITH_PRESSURE		3
#define EVENT_FORMAT_KEY_ONLY 		4
#define EVENT_FORMAT_WITH_PRESSURE_2BYTE	8

/* vector id */
#define VECTOR_ID_SCREEN_RX				1
#define VECTOR_ID_SCREEN_TX				2
#define VECTOR_ID_KEY_RX				3
#define VECTOR_ID_KEY_TX				4
#define VECTOR_ID_PRESSURE				5
#define VECTOR_ID_OPEN_RESULT			7
#define VECTOR_ID_OPEN_RX				8
#define VECTOR_ID_OPEN_TX				9
#define VECTOR_ID_SHORT_RESULT			10
#define VECTOR_ID_SHORT_RX				11
#define VECTOR_ID_SHORT_TX				12

#define OPEN_SHORT_TEST		1
#define CHECK_ONLY_OPEN_TEST	1
#define CHECK_ONLY_SHORT_TEST	2

/**
 * LPM status bitmask
 */
#define MMS_LPM_FLAG_SPAY		(1 << 0)
#define MMS_LPM_FLAG_AOD		(1 << 1)

#define MMS_MODE_SPONGE_SWIPE		(1 << 1)
#define MMS_MODE_SPONGE_AOD			(1 << 2)
#define MMS_MODE_SPONGE_SINGLE_TAP		(1 << 3)
#define MMS_MODE_SPONGE_PRESS		(1 << 4)
#define MMS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP	(1 << 5)

#define MMS_GESTURE_CODE_SWIPE		0x00
#define MMS_GESTURE_CODE_DOUBLE_TAP		0x01
#define MMS_GESTURE_CODE_PRESS		0x03
#define MMS_GESTURE_CODE_SINGLE_TAP		0x04

/* MMS_GESTURE_ID */
#define MMS_GESTURE_ID_AOD			0x00
#define MMS_GESTURE_ID_DOUBLETAP_TO_WAKEUP	0x01

#define MMS_GESTURE_ID_FOD_LONG			0x00
#define MMS_GESTURE_ID_FOD_NORMAL			0x01
#define MMS_GESTURE_ID_FOD_RELEASE	0x02
#define MMS_GESTURE_ID_FOD_OUT	0x03

typedef enum {
	SPONGE_EVENT_TYPE_SPAY			= 0x04,
	SPONGE_EVENT_TYPE_SINGLE_TAP		= 0x08,
	SPONGE_EVENT_TYPE_AOD_PRESS		= 0x09,
	SPONGE_EVENT_TYPE_AOD_LONGPRESS		= 0x0A,
	SPONGE_EVENT_TYPE_AOD_DOUBLETAB		= 0x0B,
	SPONGE_EVENT_TYPE_FOD		= 0x0F,
	SPONGE_EVENT_TYPE_FOD_RELEASE		= 0x10,
	SPONGE_EVENT_TYPE_FOD_OUT = 0x11
} SPONGE_EVENT_TYPE;

#define SPONGE_AOD_ENABLE_OFFSET 0x00
#define SPONGE_TOUCHBOX_W_OFFSET 0x02
#define SPONGE_TOUCHBOX_H_OFFSET 0x04
#define SPONGE_TOUCHBOX_X_OFFSET 0x06
#define SPONGE_TOUCHBOX_Y_OFFSET 0x08
#define SPONGE_UTC_OFFSET        0x10
#define SPONGE_FOD_PROPERTY      0x14
#define SPONGE_FOD_INFO			0x15
#define SPONGE_FOD_POSITION			0x19
#define SPONGE_LP_DUMP_REG_ADDR            0xF0
#define SPONGE_DUMP_FORMAT_REG_OFFSET    (D_BASE_LP_DUMP_REG_ADDR + 0x00)
#define SPONGE_DUMP_NUM_REG_OFFSET       (D_BASE_LP_DUMP_REG_ADDR + 0x01)
#define SPONGE_DUMP_CUR_INDEX_REG_OFFSET (D_BASE_LP_DUMP_REG_ADDR + 0x02)
#define SPONGE_DUMP_START                (D_BASE_LP_DUMP_REG_ADDR + 0x04)

#define MMS_TS_COORDINATE_ACTION_RELEASE		0
#define MMS_TS_COORDINATE_ACTION_PRESS_MOVE		1

/*
 * support_feature
 * bit value should be made a promise with InputFramework.
 */
#define INPUT_FEATURE_ENABLE_SETTINGS_AOT	(1 << 0) /* Double tap wakeup settings */
#define INPUT_FEATURE_ENABLE_PRESSURE		(1 << 1) /* homekey pressure */
#define INPUT_FEATURE_ENABLE_SYNC_RR120		(1 << 2) /* sync reportrate 120hz */

#define TWO_LEVEL_GRIP_CONCEPT
#ifdef TWO_LEVEL_GRIP_CONCEPT
#define SEC_TS_CMD_EDGE_HANDLER 	0xAA
#define SEC_TS_CMD_EDGE_AREA		0xAB
#define SEC_TS_CMD_DEAD_ZONE		0xAC
#define SEC_TS_CMD_LANDSCAPE_MODE	0xAD

#define DIFF_SCALER 1000

enum grip_write_mode {
	G_NONE				= 0,
	G_SET_EDGE_HANDLER		= 1,
	G_SET_EDGE_ZONE			= 2,
	G_SET_NORMAL_MODE		= 4,
	G_SET_LANDSCAPE_MODE	= 8,
	G_CLR_LANDSCAPE_MODE	= 16,
};
enum grip_set_data {
	ONLY_EDGE_HANDLER		= 0,
	GRIP_ALL_DATA			= 1,
};
#endif

struct mms_ts_coordinate {
	u8 id;
	u8 action;
	u16 x;
	u16 y;
	u16 p_x;
	u16 p_y;
	u16 z;
	u8 major;
	u8 minor;
	bool palm;
	u16 mcount;
	int palm_count;
	u8 left_event;
	u8 type;
	u8 pre_type;
};


/**
 * Device info structure
 */
struct mms_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *input_dev_pad;
	struct input_dev *input_dev_proximity;
	char phys[32];
	struct mms_devicetree_data *dtdata;
	struct pinctrl *pinctrl;
	struct completion resume_done;

	dev_t mms_dev;
	struct class *class;

	struct mutex lock;
	struct mutex lock_test;
	struct mutex lock_cmd;
	struct mutex lock_dev;
	struct mutex sponge_mutex;

	struct sec_cmd_data sec;

	struct mms_ts_coordinate coord[MAX_FINGER_NUM];

	int irq;
	bool	 enabled;
	bool init;
	char *fw_name;

	int test_min;
	int test_max;
	int test_diff_max;

	u8 product_name[16];
	int max_x;
	int max_y;
	u8 node_x;
	u8 node_y;
	u8 node_key;
	u16 fw_ver_ic;
	u16 fw_ver_bin[8];
	u16 fw_model_ver_ic;
	u8 event_size;
	int event_format;
	u16 fw_year;
	u8 fw_month;
	u8 fw_date;
	u16 pre_chksum;
	u16 rt_chksum;
	unsigned char finger_state[MAX_FINGER_NUM];
	int touch_count;

	u8 fod_tx;
	u8 fod_rx;
	u8 fod_vi_size;
	u8 fod_lp_mode;

	u8 grip_landscape_mode;
	u16 grip_landscape_top_deadzone;
	u16 grip_landscape_bottom_deadzone;

	bool tkey_enable;

	u8 nap_mode;
	u8 glove_mode;
	u8 charger_mode;
	u8 cover_mode;

	u8 esd_cnt;
	bool disable_esd;

	unsigned int sram_addr_num;
	u32 sram_addr[8];

	u8 *print_buf;
	int *image_buf;

	bool test_busy;
	bool cmd_busy;
	bool dev_busy;

#if MMS_USE_DEV_MODE
	struct cdev cdev;
	u8 *dev_fs_buf;
#endif

#ifdef CONFIG_VBUS_NOTIFIER
	struct notifier_block vbus_nb;
	bool ta_stsatus;
#endif

#ifdef USE_TSP_TA_CALLBACKS
	void (*register_cb)(struct tsp_callbacks *);
	struct tsp_callbacks callbacks;
#endif
	struct delayed_work ghost_check;
	u8 tsp_dump_lock;

	struct mutex modechange;
	struct delayed_work work_read_info;
	bool info_work_done;

	struct delayed_work work_print_info;
	int noise_mode;
	int wet_mode;
	int print_info_cnt_open;
	int print_info_cnt_release;

	bool lowpower_mode;
	unsigned char lowpower_flag;

	long prox_power_off;
	u8 ed_enable;

	int ic_status;
	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

	u8 check_multi;
	unsigned int multi_count;
	unsigned int comm_err_count;

	int open_short_type;
	int open_short_result;

	u32 defect_probability;
	u8 item_cmdata;
};

enum IC_STATUS {
	PWR_ON = 0,
	PWR_OFF = 1,
	LP_MODE = 2,
	LP_ENTER = 3,
};

/**
 * Platform Data
 */
struct mms_devicetree_data {
	int gpio_intr;
	const char *gpio_vdd_en;
	const char *gpio_io_en;
	int gpio_sda;
	int gpio_scl;
	int bringup;
	struct regulator *vdd_io;
	const char *fw_name;
	const char *fw_name_old;
	const char *model_name;
	bool support_lpm;
	bool support_ear_detect;
	bool support_fod;
	bool enable_settings_aot;
	bool sync_reportrate_120;
	bool no_vsync;

	int max_x;
	int max_y;
	int node_x;
	int node_y;
	int node_key;
	int event_format;
	int event_size;
	int fod_tx;
	int fod_rx;
	int fod_vi_size;

	u32	area_indicator;
	u32	area_navigation;
	u32	area_edge;
};

/**
 * Firmware binary header info
 */
struct mms_bin_hdr {
	char	tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;

	u32	extention_offset;
	u32	reserved1;
} __attribute__ ((packed));

/**
 * Firmware image info
 */
struct mms_fw_img {
	u16	type;
	u16	version;

	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;
} __attribute__ ((packed));

/*
 * Firmware update error code
 */
enum fw_update_errno {
	FW_ERR_FILE_READ = -4,
	FW_ERR_FILE_OPEN = -3,
	FW_ERR_FILE_TYPE = -2,
	FW_ERR_DOWNLOAD = -1,
	FW_ERR_NONE = 0,
	FW_ERR_UPTODATE = 1,
};

/*
 * Firmware file location
 */
enum fw_bin_source {
	FW_BIN_SOURCE_KERNEL = 1,
	FW_BIN_SOURCE_EXTERNAL = 2,
};

/*
 * Flash failure type
 */
enum flash_fail_type {
	FLASH_FAIL_NONE = 0,
	FLASH_FAIL_CRITICAL = 1,
	FLASH_FAIL_SECTION = 2,
};

/**
 * Declarations
 */
//main
void mms_reboot(struct mms_ts_info *info);
int mms_i2c_read(struct mms_ts_info *info, char *write_buf, unsigned int write_len,
			char *read_buf, unsigned int read_len);
int mms_i2c_read_next(struct mms_ts_info *info, char *read_buf, int start_idx,
			unsigned int read_len);
int mms_i2c_write(struct mms_ts_info *info, char *write_buf, unsigned int write_len);
int mms_enable(struct mms_ts_info *info);
int mms_disable(struct mms_ts_info *info);
int mms_get_ready_status(struct mms_ts_info *info);
int mms_get_fw_version(struct mms_ts_info *info, u8 *ver_buf);
int mms_get_fw_version_u16(struct mms_ts_info *info, u16 *ver_buf_u16);
int mms_disable_esd_alert(struct mms_ts_info *info);
int mms_fw_update_from_kernel(struct mms_ts_info *info, bool force);
int mms_fw_update_from_storage(struct mms_ts_info *info, bool force);
int mms_fw_update_from_ffu(struct mms_ts_info *info, bool force);

//mod

#ifdef USE_TSP_TA_CALLBACKS
static bool ta_connected;
#endif

/**
 * Control power supply
 */
static inline int mms_power_control(struct mms_ts_info *info, int enable)
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
static inline void mms_clear_input(struct mms_ts_info *info)
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

void mms_report_input_event(struct mms_ts_info *info, u8 sz, u8 *buf);

/************************************************************
*  720  * 1480 : <48 96 60> indicator: 24dp navigator:48dp edge:60px dpi=320
* 1080  * 2220 :  4096 * 4096 : <133 266 341>  (approximately value)
************************************************************/
static inline void mms_ts_location_detect(struct mms_ts_info *info, char *loc, int x, int y)
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
static inline void mms_input_event_handler(struct mms_ts_info *info, u8 sz, u8 *buf)
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

			switch (id) {
			case 1:
				key_code = KEY_MENU;
				break;
			case 2:
				key_code = KEY_BACK;
				break;
			}

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
static inline int mms_custom_event_handler(struct mms_ts_info *info, u8 *rbuf, u8 size)
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
#ifndef CONFIG_MINT_SESL
				input_report_key(info->input_dev, KEY_WAKEUP, 1);
#else
				input_report_key(info->input_dev, KEY_HOMEPAGE, 1);
#endif
				input_sync(info->input_dev);
#ifndef CONFIG_MINT_SESL
				input_report_key(info->input_dev, KEY_WAKEUP, 0);
#else
				input_report_key(info->input_dev, KEY_HOMEPAGE, 0);
#endif
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

static inline int sponge_read(struct mms_ts_info *info, u16 addr, u8 *buf, u8 len)
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

static inline int sponge_write(struct mms_ts_info *info, u16 addr, u8 *buf, u8 len)
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

static inline int mms_set_custom_library(struct mms_ts_info *info, u16 addr, u8 *buf, u8 len)
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

/**
 * Read image data
 */
static inline int mms_get_image(struct mms_ts_info *info, u8 image_type)
{
	int busy_cnt = 500;
	int wait_cnt = 200;
	u8 wbuf[8];
	u8 rbuf[512];
	u8 row_num;
	u8 col_num;
	u8 buffer_col_num;
	u8 rotate;
	u8 key_num;
	u8 data_type;
	u8 data_type_size;
	u8 data_type_sign;
	u8 vector_num = 0;
	u16 vector_id[16];
	u16 vector_elem_num[16];
	u8 buf_addr_h;
	u8 buf_addr_l;
	u16 buf_addr = 0;
	int i;
	int table_size;
	int ret = 0;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);
	input_dbg(true, &info->client->dev, "%s - image_type[%d]\n", __func__, image_type);

	while (busy_cnt--) {
		if (info->test_busy == false)
			break;

		msleep(10);
	}

	memset(info->print_buf, 0, PAGE_SIZE);

	/* disable touch event */
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_CTRL_TRIGGER_NONE;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Disable event\n", __func__);
		return 1;
	}

	mutex_lock(&info->lock);
	info->test_busy = true;
	disable_irq(info->irq);
	mutex_unlock(&info->lock);

	//check image type
	switch (image_type) {
	case MIP_IMG_TYPE_INTENSITY:
		input_dbg(true, &info->client->dev, "=== Intensity Image ===\n");
		break;
	case MIP_IMG_TYPE_RAWDATA:
		input_dbg(true, &info->client->dev, "=== Rawdata Image ===\n");
		break;
	case MIP_IMG_TYPE_HSELF_RAWDATA:
		input_dbg(true, &info->client->dev, "=== self Rawdata Image ===\n");
		break;
	case MIP_IMG_TYPE_HSELF_INTENSITY:
		input_dbg(true, &info->client->dev, "=== self intensity Image ===\n");
		break;
	case MIP_IMG_TYPE_PROX_INTENSITY:
		input_dbg(true, &info->client->dev, "=== PROX intensity Image ===\n");
		break;
	case MIP_IMG_TYPE_5POINT_INTENSITY:
		input_dbg(true, &info->client->dev, "=== sensitivity Image ===\n");
		break;		
	default:
		input_err(true, &info->client->dev, "%s [ERROR] Unknown image type\n", __func__);
		goto ERROR;
	}

	//set image type
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_TYPE;
	wbuf[2] = image_type;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Write image type\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - set image type\n", __func__);

	//wait ready status
	wait_cnt = 200;
	while (wait_cnt--) {
		if (mms_get_ready_status(info) == MIP_CTRL_STATUS_READY)
			break;

		msleep(10);

		input_dbg(true, &info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt);
	}

	if (wait_cnt <= 0) {
		input_err(true, &info->client->dev, "%s [ERROR] Wait timeout\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s - ready\n", __func__);

	//data format
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_DATA_FORMAT;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 7)) {
		input_err(true, &info->client->dev, "%s [ERROR] Read data format\n", __func__);
		goto ERROR;
	}

	row_num = rbuf[0];
	col_num = rbuf[1];
	buffer_col_num = rbuf[2];
	rotate = rbuf[3];
	key_num = rbuf[4];
	data_type = rbuf[5];
	data_type_sign = (data_type & 0x80) >> 7;
	data_type_size = data_type & 0x7F;
	vector_num = rbuf[6];

	input_dbg(true, &info->client->dev,
		"%s - row_num[%d] col_num[%d] buffer_col_num[%d] rotate[%d] key_num[%d]\n",
		__func__, row_num, col_num, buffer_col_num, rotate, key_num);
	input_dbg(true, &info->client->dev,
		"%s - data_type[0x%02X] data_sign[%d] data_size[%d]\n",
		__func__, data_type, data_type_sign, data_type_size);

	if (vector_num > 0) {
		wbuf[0] = MIP_R0_IMAGE;
		wbuf[1] = MIP_R1_IMAGE_VECTOR_INFO;
		if (mms_i2c_read(info, wbuf, 2, rbuf, (vector_num * 4))) {
			input_err(true, &info->client->dev, "%s [ERROR] Read vector info\n", __func__);
			goto ERROR;
		}
		for (i = 0; i < vector_num; i++) {
			vector_id[i] = rbuf[i * 4 + 0] | (rbuf[i * 4 + 1] << 8);
			vector_elem_num[i] = rbuf[i * 4 + 2] | (rbuf[i * 4 + 3] << 8);
			input_dbg(true, &info->client->dev, "%s - vector[%d] : id[%d] elem_num[%d]\n", __func__, i, vector_id[i], vector_elem_num[i]);
		}
	}

	//get buf addr
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_BUF_ADDR;
	if (mms_i2c_read(info, wbuf, 2, rbuf, 2)) {
		input_err(true, &info->client->dev, "%s [ERROR] Read buf addr\n", __func__);
		goto ERROR;
	}

	buf_addr_l = rbuf[0];
	buf_addr_h = rbuf[1];
	input_dbg(true, &info->client->dev, "%s - buf_addr[0x%02X 0x%02X]\n",
		__func__, buf_addr_h, buf_addr_l);

	if ((key_num > 0) || (vector_num > 0)) {
		if (table_size > 0)
			buf_addr += (row_num * buffer_col_num * data_type_size);

		buf_addr_l = buf_addr & 0xFF;
		buf_addr_h = (buf_addr >> 8) & 0xFF;
		input_dbg(true, &info->client->dev, "%s - vector buf_addr[0x%02X 0x%02X][0x%04X]\n", __func__, buf_addr_h, buf_addr_l, buf_addr);

	}
	goto EXIT;

ERROR:
	ret = 1;
EXIT:
	/* clear image type */
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_TYPE;
	wbuf[2] = MIP_IMG_TYPE_NONE;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Clear image type\n", __func__);
		ret = 1;
	}

	/* enable touch event */
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_CTRL_TRIGGER_INTR;
	if (mms_i2c_write(info, wbuf, 3)) {
		input_err(true, &info->client->dev, "%s [ERROR] Enable event\n", __func__);
		ret = 1;
	}

	if (ret)
		mms_reboot(info);

	//exit
	mutex_lock(&info->lock);
	info->test_busy = false;
	enable_irq(info->irq);
	mutex_unlock(&info->lock);

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

#ifdef CONFIG_VBUS_NOTIFIER
int mms_charger_attached(struct mms_ts_info *info, bool status);
#endif

#if MMS_USE_DEVICETREE
/**
 * Parse device tree
 */
static inline int mms_parse_devicetree(struct device *dev, struct mms_ts_info *info)
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
static inline void mms_config_input(struct mms_ts_info *info)
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
#ifndef CONFIG_MINT_SESL
	set_bit(KEY_WAKEUP, input_dev->keybit);
#else
	set_bit(KEY_HOMEPAGE, input_dev->keybit);
#endif
	set_bit(KEY_BLACK_UI_GESTURE, input_dev->keybit);
	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);
}

int mms_lowpower_mode(struct mms_ts_info *info, u8 on);

//fw_update
int mip4_ts_flash_fw(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size,
			bool force, bool section, bool on_probe);
int mip4_ts_bin_fw_version(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf);

//test
#if MMS_USE_DEV_MODE
int mms_dev_create(struct mms_ts_info *info);
int mms_get_log(struct mms_ts_info *info);
#endif


//cmd
#if MMS_USE_CMD_MODE
int mms_sysfs_cmd_create(struct mms_ts_info *info);
void mms_sysfs_cmd_remove(struct mms_ts_info *info);
#endif

/**
 * Callback - get charger status
 */
#ifdef USE_TSP_TA_CALLBACKS
static inline void mms_charger_status_cb(struct tsp_callbacks *cb, int status)
{
	pr_info("%s: TA %s\n",
		__func__, status ? "connected" : "disconnected");

	if (status)
		ta_connected = true;
	else
		ta_connected = false;

	/* not yet defined functions */
}

static inline void mms_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_info("%s\n", __func__);
}
#endif

#ifdef CONFIG_VBUS_NOTIFIER
static inline int mms_vbus_notification(struct notifier_block *nb,
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

void minority_report_calculate_cmdata(struct mms_ts_info *info);
void minority_report_sync_latest_value(struct mms_ts_info *info);

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

#endif /* __MELFAS_MMS400_H */
