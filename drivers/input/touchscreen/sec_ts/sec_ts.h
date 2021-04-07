/* drivers/input/touchscreen/sec_ts.h
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_TS_H__
#define __SEC_TS_H__

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/input/sec_cmd.h>
#ifdef CONFIG_SECURE_TOUCH
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/atomic.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE	0
#endif

#define SEC_TS_I2C_NAME		"sec_ts"
#define SEC_TS_DEVICE_NAME	"SEC_TS"

#define USE_OPEN_CLOSE
#undef USE_RESET_DURING_POWER_ON
#undef USE_RESET_EXIT_LPM
#undef USE_POR_AFTER_I2C_RETRY
#undef USER_OPEN_DWORK

/* TEMP */
#undef CONFIG_SAMSUNG_LPM_MODE

#define TOUCH_RESET_DWORK_TIME		10

/* #define PAT_CONTROL */

#define CMD_STR_LEN		256


#define MASK_1_BITS			0x0001
#define MASK_2_BITS			0x0003
#define MASK_3_BITS			0x0007
#define MASK_4_BITS			0x000F
#define MASK_5_BITS			0x001F
#define MASK_6_BITS			0x003F
#define MASK_7_BITS			0x007F
#define MASK_8_BITS			0x00FF

/* support feature */
#define SEC_TS_SUPPORT_SPONGELIB	/* support display lab algorithm */

#define TYPE_STATUS_EVENT_CMD_DRIVEN	0
#define TYPE_STATUS_EVENT_ERR		1
#define TYPE_STATUS_EVENT_INFO		2
#define TYPE_STATUS_EVENT_USER_INPUT	3
#define TYPE_STATUS_EVENT_VENDOR_INFO	7
#define TYPE_STATUS_CODE_SAR	0x28

#define BIT_STATUS_EVENT_CMD_DRIVEN	a << TYPE_STATUS_EVENT_CMD_DRIVEN
#define BIT_STATUS_EVENT_ERR(a)		a << TYPE_STATUS_EVENT_ERR
#define BIT_STATUS_EVENT_INFO(a)	a << TYPE_STATUS_EVENT_INFO
#define BIT_STATUS_EVENT_USER_INPUT(a)	a << TYPE_STATUS_EVENT_USER_INPUT
#define BIT_STATUS_EVENT_VENDOR_INFO(a)	a << TYPE_STATUS_EVENT_VENDOR_INFO

#define DO_FW_CHECKSUM			(1 << 0)
#define DO_PARA_CHECKSUM		(1 << 1)
#define MAX_SUPPORT_TOUCH_COUNT		10
#define MAX_SUPPORT_HOVER_COUNT		1

#define SEC_TS_EVENTID_HOVER		10

#define SEC_TS_DEFAULT_FW_NAME		"tsp_sec/sec_hero.fw"
#define SEC_TS_DEFAULT_BL_NAME		"tsp_sec/s6smc41_blupdate_img_REL.bin"
#define SEC_TS_DEFAULT_PARA_NAME	"tsp_sec/s6smc41_para_REL_DGA0_V0106_150114_193317.bin"
#define SEC_TS_DEFAULT_UMS_FW		"/sdcard/Firmware/TSP/lsi.bin"
#define SEC_TS_DEFAULT_FFU_FW		"ffu_tsp.bin"
#define SEC_TS_MAX_FW_PATH		64
#define SEC_TS_FW_BLK_SIZE_MAX		(512)
#define SEC_TS_FW_BLK_SIZE_DEFAULT	(256)
#define SEC_TS_SELFTEST_REPORT_SIZE	80

#define I2C_WRITE_BUFFER_SIZE		10

#define SEC_TS_FW_HEADER_SIGN		0x53494654
#define SEC_TS_FW_CHUNK_SIGN		0x53434654

#define SEC_TS_FW_UPDATE_ON_PROBE

#define AMBIENT_CAL			0
#define OFFSET_CAL_SDC			1
#define OFFSET_CAL_SEC			2

#define SEC_TS_SKIPTSP_DUTY		100

#define SEC_TS_NVM_OFFSET_FAC_RESULT		0
#define SEC_TS_NVM_OFFSET_CAL_COUNT		1
#define SEC_TS_NVM_OFFSET_DISASSEMBLE_COUNT	2
#define SEC_TS_NVM_OFFSET_TUNE_VERSION		3
#define SEC_TS_NVM_OFFSET_TUNE_VERSION_LENGTH	2
#define SEC_TS_NVM_OFFSET_LENGTH		(SEC_TS_NVM_OFFSET_TUNE_VERSION + SEC_TS_NVM_OFFSET_TUNE_VERSION_LENGTH + 1)

/* SEC_TS READ REGISTER ADDRESS */
#define SEC_TS_CMD_SENSE_ON			0x10
#define SEC_TS_CMD_SENSE_OFF			0x11
#define SEC_TS_CMD_SW_RESET	0x42 /* only y661 */
#define SEC_TS_CMD_CALIBRATION_SEC		0x13	// send it to touch ic, but toucu ic works nothing.
#define SEC_TS_CMD_FACTORY_PANELCALIBRATION	0x14

#define SEC_TS_READ_GPIO_STATUS			0x20 	// not support
#define SEC_TS_READ_FIRMWARE_INTEGRITY		0x21
#define SEC_TS_READ_DEVICE_ID			0x22
#define SEC_TS_READ_PANEL_INFO			0x23
#define SEC_TS_READ_CORE_CONFIG_VERSION		0x24

#define SEC_TS_CMD_SET_TOUCHFUNCTION		0x30
#define SEC_TS_CMD_SET_TSC_MODE			0x31
#define SET_TS_CMD_SET_CHARGER_MODE		0x32
#define SET_TS_CMD_SET_NOISE_MODE		0x33
#define SET_TS_CMD_SET_REPORT_RATE		0x34
#define SEC_TS_CMD_TOUCH_MODE_FOR_THRESHOLD	0x35
#define SEC_TS_CMD_TOUCH_THRESHOLD		0x36
#define SET_TS_CMD_KEY_THRESHOLD		0x37
#define SEC_TS_CMD_SET_COVERTYPE		0x38
#define SEC_TS_CMD_WAKEUP_GESTURE_MODE		0x39
#define SEC_TS_WRITE_POSITION_FILTER		0x3A
#define SEC_TS_CMD_WET_MODE			0x3B
#define SEC_TS_CMD_ERASE_FLASH			0x45
#define SEC_TS_READ_ID				0x52
#define SEC_TS_READ_BOOT_STATUS			0x55
#define SEC_TS_CMD_ENTER_FW_MODE		0x57
#define SEC_TS_READ_ONE_EVENT			0x60
#define SEC_TS_READ_ALL_EVENT			0x61
#define SEC_TS_CMD_CLEAR_EVENT_STACK		0x62
#define SEC_TS_CMD_MUTU_RAW_TYPE		0x70
#define SEC_TS_CMD_SELF_RAW_TYPE		0x71
#define SEC_TS_READ_TOUCH_RAWDATA		0x72
#define SEC_TS_READ_TOUCH_SELF_RAWDATA		0x73
#define SEC_TS_READ_SELFTEST_RESULT		0x80
#define SEC_TS_CMD_CALIBRATION_AMBIENT		0x81
#define SEC_TS_CMD_STATEMANAGE_ON		0x82
#define SEC_TS_CMD_NVM				0x85
#define SEC_TS_CMD_CALIBRATION_OFFSET_SDC	0x8F

/* SEC_TS SPONGE OPCODE COMMAND */
#define SEC_TS_CMD_SPONGE_GET_INFO	0x90
#define SEC_TS_CMD_SPONGE_WRITE_PARAM	0x91
#define SEC_TS_CMD_SPONGE_READ_PARAM	0x92
#define SEC_TS_CMD_SPONGE_NOTIFY_PACKET	0x93

#define SEC_TS_CMD_STATUS_EVENT_TYPE	0xA0
#define SEC_TS_READ_FW_INFO		0xA2
#define SEC_TS_READ_FW_VERSION		0xA3
#define SEC_TS_READ_PARA_VERSION	0xA4
#define SEC_TS_READ_IMG_VERSION		0xA5
#define SEC_TS_CMD_GET_CHECKSUM		0xA6
#define SEC_TS_CMD_MIS_CAL_CHECK	0xA7
#define SEC_TS_CMD_MIS_CAL_READ		0xA8
#define SEC_TS_CMD_MIS_CAL_SPEC		0xA9
#define SEC_TS_CMD_DEADZONE_RANGE	0xAA
#define SEC_TS_CMD_LONGPRESSZONE_RANGE	0xAB
#define SEC_TS_CMD_LONGPRESS_DROP_AREA	0xAC
#define SEC_TS_CMD_LONGPRESS_DROP_DIFF	0xAD
#define SEC_TS_READ_TS_STATUS		0xAF
#define SEC_TS_CMD_SELFTEST		0xAE

/* SEC_TS FLASH COMMAND */
#define SEC_TS_CMD_FLASH_READ_ADDR	0xD0
#define SEC_TS_CMD_FLASH_READ_SIZE	0xD1
#define SEC_TS_CMD_FLASH_READ_DATA	0xD2
#define SEC_TS_CMD_CHG_SYSMODE		0xD7
#define SEC_TS_CMD_FLASH_ERASE		0xD8
#define SEC_TS_CMD_FLASH_WRITE		0xD9
#define SEC_TS_CMD_FLASH_PADDING	0xDA
#define SEC_TS_READ_BL_UPDATE_STATUS	0xDB
#define SEC_TS_CMD_SET_POWER_MODE	0xE4
#define SEC_TS_CMD_EDGE_DEADZONE	0xE5
#define SEC_TS_READ_CALIBRATION_REPORT		0xF1
#define SEC_TS_CMD_SET_VENDOR_EVENT_LEVEL	0xF2
#define SEC_TS_CMD_SET_SPENMODE			0xF3

#define SEC_TS_FLASH_SIZE_64		64
#define SEC_TS_FLASH_SIZE_128		128
#define SEC_TS_FLASH_SIZE_256		256

#define SEC_TS_FLASH_SIZE_CMD		1
#define SEC_TS_FLASH_SIZE_ADDR		2
#define SEC_TS_FLASH_SIZE_CHECKSUM	1

#define SEC_TS_STATUS_BOOT_MODE		0x10
#define SEC_TS_STATUS_APP_MODE		0x20

#define SEC_TS_FIRMWARE_PAGE_SIZE_256	256
#define SEC_TS_FIRMWARE_PAGE_SIZE_128	128

/* SEC status event id */
#define SEC_TS_COORDINATE_EVENT		0
#define SEC_TS_STATUS_EVENT		1
#define SEC_TS_GESTURE_EVENT		2
#define SEC_TS_EMPTY_EVENT		3

#define SEC_TS_EVENT_BUFF_SIZE 		8
#define SEC_TS_SID_GESTURE		0x14
#define SEC_TS_GESTURE_CODE_SPAY		0x00
#define SEC_TS_GESTURE_CODE_DOUBLE_TAP		0x01

#define SEC_TS_COORDINATE_ACTION_NONE		0
#define SEC_TS_COORDINATE_ACTION_PRESS		1
#define SEC_TS_COORDINATE_ACTION_MOVE		2
#define SEC_TS_COORDINATE_ACTION_RELEASE	3

#define SEC_TS_TOUCHTYPE_NORMAL		0
#define SEC_TS_TOUCHTYPE_HOVER		1
#define SEC_TS_TOUCHTYPE_FLIPCOVER	2
#define SEC_TS_TOUCHTYPE_GLOVE		3
#define SEC_TS_TOUCHTYPE_STYLUS		4
#define SEC_TS_TOUCHTYPE_PALM		5
#define SEC_TS_TOUCHTYPE_WET		6
#define SEC_TS_TOUCHTYPE_PROXIMITY	7
#define SEC_TS_TOUCHTYPE_JIG		8

/* SEC_TS_INFO : Info acknowledge event */
#define SEC_TS_ACK_BOOT_COMPLETE	0x00
#define SEC_TS_ACK_WET_MODE	0x1

/* SEC_TS_VENDOR_INFO : Vendor acknowledge event */
#define SEC_TS_VENDOR_ACK_OFFSET_CAL_DONE	0x40
#define SEC_TS_VENDOR_ACK_SELF_TEST_DONE	0x41

/* SEC_TS_STATUS_EVENT_USER_INPUT */
#define SEC_TS_EVENT_FORCE_KEY	0x1

/* SEC_TS_ERROR : Error event */
#define SEC_TS_ERR_EVNET_CORE_ERR	0x0
#define SEC_TS_ERR_EVENT_QUEUE_FULL	0x01
#define SEC_TS_ERR_EVENT_ESD		0x2

#define SEC_TS_BIT_SETFUNC_TOUCH		(1 << 0)
#define SEC_TS_BIT_SETFUNC_MUTUAL		(1 << 0)
#define SEC_TS_BIT_SETFUNC_HOVER		(1 << 1)
#define SEC_TS_BIT_SETFUNC_COVER		(1 << 2)
#define SEC_TS_BIT_SETFUNC_GLOVE		(1 << 3)
#define SEC_TS_BIT_SETFUNC_STYLUS		(1 << 4)
#define SEC_TS_BIT_SETFUNC_PALM			(1 << 5)
#define SEC_TS_BIT_SETFUNC_WET			(1 << 6)
#define SEC_TS_BIT_SETFUNC_PROXIMITY		(1 << 7)

#define SEC_TS_DEFAULT_ENABLE_BIT_SETFUNC	(SEC_TS_BIT_SETFUNC_TOUCH | SEC_TS_BIT_SETFUNC_PALM | SEC_TS_BIT_SETFUNC_WET)

#define SEC_TS_BIT_CHARGER_MODE_NO			(0x1 << 0)
#define SEC_TS_BIT_CHARGER_MODE_WIRE_CHARGER		(0x1 << 1)
#define SEC_TS_BIT_CHARGER_MODE_WIRELESS_CHARGER	(0x1 << 2)
#define SEC_TS_BIT_CHARGER_MODE_WIRELESS_BATTERY_PACK	(0x1 << 3)

#ifdef PAT_CONTROL
/*---------------------------------------
	<<< apply to server >>>
	0x00 : no action
	0x01 : clear nv  
	0x02 : pat magic
	0x03 : rfu

	<<< use for temp bin >>>
	0x05 : forced clear nv & f/w update  before pat magic, eventhough same f/w
	0x06 : rfu
  ---------------------------------------*/
#define PAT_CONTROL_NONE  		0x00
#define PAT_CONTROL_CLEAR_NV 		0x01
#define PAT_CONTROL_PAT_MAGIC 		0x02
#define PAT_CONTROL_FORCE_UPDATE	0x05

#define PAT_MAX_LCIA			0x80
#define PAT_MAX_MAGIC			0xF5
#define PAT_MAGIC_NUMBER		0x83
#endif

#define STATE_MANAGE_ON			1
#define STATE_MANAGE_OFF		0

#define SEC_TS_STATUS_NOT_CALIBRATION	0x50
#define SEC_TS_STATUS_CALIBRATION_SDC	0xA1
#define SEC_TS_STATUS_CALIBRATION_SEC	0xA2

#define TWO_LEVEL_GRIP_CONCEPT
#ifdef TWO_LEVEL_GRIP_CONCEPT
#define SEC_TS_CMD_EDGE_HANDLER 	0xAA
#define SEC_TS_CMD_EDGE_AREA		0xAB
#define SEC_TS_CMD_DEAD_ZONE		0xAC
#define SEC_TS_CMD_LANDSCAPE_MODE	0xAD

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

typedef enum {
	SEC_TS_STATE_POWER_OFF = 0,
	SEC_TS_STATE_LPM_SUSPEND,
	SEC_TS_STATE_LPM_RESUME,
	SEC_TS_STATE_POWER_ON
} TOUCH_POWER_MODE;

typedef enum {
	TOUCH_SYSTEM_MODE_BOOT		= 0,
	TOUCH_SYSTEM_MODE_CALIBRATION	= 1,
	TOUCH_SYSTEM_MODE_TOUCH		= 2,
	TOUCH_SYSTEM_MODE_SELFTEST	= 3,
	TOUCH_SYSTEM_MODE_FLASH		= 4,
	TOUCH_SYSTEM_MODE_LOWPOWER	= 5,
	TOUCH_SYSTEM_MODE_LISTEN
} TOUCH_SYSTEM_MODE;

typedef enum {
	TOUCH_MODE_STATE_IDLE		= 0,
	TOUCH_MODE_STATE_HOVER		= 1,
	TOUCH_MODE_STATE_TOUCH		= 2,
	TOUCH_MODE_STATE_NOISY		= 3,
	TOUCH_MODE_STATE_CAL		= 4,
	TOUCH_MODE_STATE_CAL2		= 5,
	TOUCH_MODE_STATE_WAKEUP		= 10
} TOUCH_MODE_STATE;

enum switch_system_mode {
	TO_TOUCH_MODE			= 0,
	TO_LOWPOWER_MODE		= 1,
	TO_SELFTEST_MODE		= 2,
	TO_FLASH_MODE			= 3,
};

typedef enum {
	SPONGE_EVENT_TYPE_SPAY			= 0x04,
	SPONGE_EVENT_TYPE_AOD			= 0x08,
	SPONGE_EVENT_TYPE_AOD_PRESS		= 0x09,
	SPONGE_EVENT_TYPE_AOD_LONGPRESS		= 0x0A,
	SPONGE_EVENT_TYPE_AOD_DOUBLETAB		= 0x0B,
	SPECIAL_EVENT_TYPE_AOD_HOMEKEY 		= 0x0C
} SPONGE_EVENT_TYPE;

#define CMD_RESULT_WORD_LEN		10

#define SEC_TS_I2C_RETRY_CNT		3
#define SEC_TS_WAIT_RETRY_CNT		100

#define SEC_TS_MODE_SPONGE_SPAY			(1 << 1)
#define SEC_TS_MODE_SPONGE_AOD			(1 << 2)
#define SEC_TS_MODE_SPONGE_FORCE_KEY           	(1 << 6)

#define SEC_TS_MODE_LOWPOWER_FLAG       	(SEC_TS_MODE_SPONGE_SPAY | SEC_TS_MODE_SPONGE_AOD \
											| SEC_TS_MODE_SPONGE_FORCE_KEY)

#define SEC_TS_AOD_GESTURE_PRESS		(1 << 7)
#define SEC_TS_AOD_GESTURE_LONGPRESS		(1 << 6)
#define SEC_TS_AOD_GESTURE_DOUBLETAB		(1 << 5)

enum sec_ts_cover_id {
	SEC_TS_FLIP_WALLET = 0,
	SEC_TS_VIEW_COVER,
	SEC_TS_COVER_NOTHING1,
	SEC_TS_VIEW_WIRELESS,
	SEC_TS_COVER_NOTHING2,
	SEC_TS_CHARGER_COVER,
	SEC_TS_VIEW_WALLET,
	SEC_TS_LED_COVER,
	SEC_TS_CLEAR_FLIP_COVER,
	SEC_TS_QWERTY_KEYBOARD_EUR,
	SEC_TS_QWERTY_KEYBOARD_KOR,
	SEC_TS_MONTBLANC_COVER = 100,
};

enum sec_fw_update_status {
	SEC_NOT_UPDATE = 0,
	SEC_NEED_FW_UPDATE,
	SEC_NEED_CALIBRATION_ONLY,
	SEC_NEED_FW_UPDATE_N_CALIBRATION,
};

#define TEST_MODE_MIN_MAX		false
#define TEST_MODE_ALL_NODE		true
#define TEST_MODE_READ_FRAME		false
#define TEST_MODE_READ_CHANNEL		true

/* factory test mode */
struct sec_ts_test_mode {
	u8 type;
	short min;
	short max;
	bool allnode;
	bool frame_channel;
};

struct sec_ts_fw_file {
	u8 *data;
	u32 pos;
	size_t size;
};

/* ----------------------------------------
 * write 0xE4 [ 11 | 10 | 01 | 00 ]
 * MSB <-------------------> LSB
 * read 0xE4
 * mapping sequnce : LSB -> MSB
 * struct sec_ts_test_result {
 * * assy : front + OCTA assay
 * * module : only OCTA
 *	 union {
 *		 struct {
 *			 u8 assy_count:2;		-> 00
 *			 u8 assy_result:2;		-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 unsigned char data[1];
 *	 };
 *};
 * ---------------------------------------- */
struct sec_ts_test_result {
	union {
		struct {
			u8 assy_count:2;
			u8 assy_result:2;
			u8 module_count:2;
			u8 module_result:2;
		} __attribute__ ((packed));
		unsigned char data[1];
	};
};

/* 8 byte */
struct sec_ts_gesture_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 gesture_id;
	u8 gesture_data_1;
	u8 gesture_data_2;
	u8 gesture_data_3;
	u8 gesture_data_4;
	u8 reserved_1;
	u8 left_event_5_0:6;
	u8 reserved_2:2;
} __attribute__ ((packed));

/* 8 byte */
struct sec_ts_event_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 status_id;
	u8 status_data_1;
	u8 status_data_2;
	u8 status_data_3;
	u8 status_data_4;
	u8 status_data_5;
	u8 left_event_5_0:6;
	u8 reserved_2:2;
} __attribute__ ((packed));

/* 8 byte */
struct sec_ts_event_coordinate {
	u8 eid:2;
	u8 tid:4;
	u8 tchsta:2;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 major;
	u8 minor;
	u8 z:6;
	u8 ttype_3_2:2;
	u8 left_event:6;
	u8 ttype_1_0:2;
} __attribute__ ((packed));

/* not fixed */
struct sec_ts_coordinate {
	u8 id;
	u8 ttype;
	u8 action;
	u16 x;
	u16 y;
	u8 z;
	u8 hover_flag;
	u8 glove_flag;
	u8 touch_height;
	u16 mcount;
	u8 major;
	u8 minor;
	bool palm;
	int palm_count;
	u8 left_event;
};


struct sec_ts_data {
	u32 isr_pin;

	u32 crc_addr;
	u32 fw_addr;
	u32 para_addr;
	u32 flash_page_size;
	u8 boot_ver[3];

	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct sec_ts_plat_data *plat_data;
	struct sec_ts_coordinate coord[MAX_SUPPORT_TOUCH_COUNT + MAX_SUPPORT_HOVER_COUNT];

	u8 lowpower_mode;
	u8 lowpower_status;
	volatile bool input_closed;

	int touch_count;
	int tx_count;
	int rx_count;
	int i2c_burstmax;
	int ta_status;
	volatile int power_status;
	int raw_status;
	int touchkey_glove_mode_status;
	u16 touch_functions;
	u8 charger_mode;
	struct sec_ts_event_coordinate touchtype;
	bool touched[11];
	u8 gesture_status[6];
	u8 cal_status;
	struct mutex lock;
	struct mutex device_mutex;
	struct mutex i2c_mutex;
	struct mutex eventlock;

	struct delayed_work work_read_nv;
#ifdef USE_RESET_DURING_POWER_ON
	struct delayed_work reset_work;
#endif
#ifdef CONFIG_SECURE_TOUCH
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
	struct completion st_irq_received;
#endif
	struct clk *core_clk;
	struct clk *iface_clk;
#endif
	struct completion resume_done;
	struct wake_lock wakelock;

	struct sec_cmd_data sec;
	short *pFrame;

	bool reinit_done;
	bool flip_enable;
	int cover_type;
	u8 cover_cmd;

	int tspid_val;
	int tspicid_val;

	bool use_sponge;
	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

#ifdef TWO_LEVEL_GRIP_CONCEPT
	u8 grip_edgehandler_direction;
	int grip_edgehandler_start_y;
	int grip_edgehandler_end_y;
	u8 grip_edge_range;
	u8 grip_deadzone_up_x;
	u8 grip_deadzone_dn_x;
	int grip_deadzone_y;
	u8 grip_landscape_mode;
	int grip_landscape_edge;
	u8 grip_landscape_deadzone;
#endif

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	struct delayed_work ghost_check;
	u8 tsp_dump_lock;
#endif

	int nv;
	int cal_count;
#ifdef PAT_CONTROL
	int tune_fix_ver;
#endif
	int temp;
	int wet_mode;

	int (*sec_ts_i2c_write)(struct sec_ts_data *ts, u8 reg, u8 *data, int len);
	int (*sec_ts_i2c_read)(struct sec_ts_data *ts, u8 reg, u8 *data, int len);
	int (*sec_ts_i2c_write_burst)(struct sec_ts_data *ts, u8 *data, int len);
	int (*sec_ts_i2c_read_bulk)(struct sec_ts_data *ts, u8 *data, int len);
	int (*sec_ts_read_sponge)(struct sec_ts_data *ts, u8 *data);
};

struct sec_ts_plat_data {
	int max_x;
	int max_y;
	int num_tx;
	int num_rx;
	unsigned gpio;
	int irq_type;
	int i2c_burstmax;
	int always_lpmode;
	int bringup;
	int grip_concept;
	int mis_cal_check;
	int grip_area;
#ifdef PAT_CONTROL
	int pat_function;
	int afe_base;
#endif
	const char *firmware_name;
	const char *parameter_name;
	const char *model_name;
	const char *project_name;
	const char *regulator_dvdd;
	const char *regulator_avdd;

	u32 panel_revision;
	u8 core_version_of_ic[4];
	u8 core_version_of_bin[4];
	u8 config_version_of_ic[4];
	u8 config_version_of_bin[4];
	u8 img_version_of_ic[4];
	u8 img_version_of_bin[4];

	struct pinctrl *pinctrl;

	int (*power)(void *data, bool on);
	void (*recovery_mode)(bool on);
	void (*enable_sync)(bool on);
	int tsp_icid;
	int tsp_id;
	int tsp_vsync;
};

int sec_ts_firmware_update_on_probe(struct sec_ts_data *ts, bool force_update);
int sec_ts_firmware_update_on_hidden_menu(struct sec_ts_data *ts, int update_type);
int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode);
int sec_ts_set_cover_type(struct sec_ts_data *ts, bool enable);
int sec_ts_wait_for_ready(struct sec_ts_data *ts, unsigned int ack);
int sec_ts_function(int (*func_init)(void *device_data), void (*func_remove)(void));
int sec_ts_fn_init(struct sec_ts_data *ts);
int sec_ts_read_calibration_report(struct sec_ts_data *ts);
int sec_ts_execute_force_calibration(struct sec_ts_data *ts, int cal_mode);
int sec_ts_fix_tmode(struct sec_ts_data *ts, u8 mode, u8 state);
int sec_ts_release_tmode(struct sec_ts_data *ts);
int get_tsp_nvm_data(struct sec_ts_data *ts, u8 offset);
void set_tsp_nvm_data_clear(struct sec_ts_data *ts, u8 offset);
int sec_ts_set_custom_library(struct sec_ts_data *ts);
#ifdef SEC_TS_SUPPORT_SPONGELIB
int sec_ts_check_custom_library(struct sec_ts_data *ts);
#endif
void sec_ts_unlocked_release_all_finger(struct sec_ts_data *ts);
void sec_ts_locked_release_all_finger(struct sec_ts_data *ts);
void sec_ts_fn_remove(struct sec_ts_data *ts);
void sec_ts_delay(unsigned int ms);
#ifdef PAT_CONTROL
void set_pat_magic_number(struct sec_ts_data *ts);
#endif

extern struct class *sec_class;

#if defined(CONFIG_FB_MSM_MDSS_SAMSUNG)
extern int get_lcd_attached(char *mode);
#endif

#if 0
extern int get_lcd_info(char *arg);
#endif

extern bool tsp_init_done;

extern struct sec_ts_data *ts_dup;

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

#ifdef TWO_LEVEL_GRIP_CONCEPT
extern void set_grip_data_to_ic(struct sec_ts_data *ts, u8 flag);
extern void sec_ts_set_grip_type(struct sec_ts_data *ts, u8 set_type);
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
extern void trustedui_mode_on(void);
extern void trustedui_mode_off(void);
#endif

#endif
