/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _SYNAPTICS_RMI4_H_
#define _SYNAPTICS_RMI4_H_

#define SYNAPTICS_RMI4_DRIVER_VERSION "DS5 1.0"
#include <linux/device.h>
#include <linux/i2c/synaptics_rmi.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <linux/sec_debug.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

/**************************************/
/* Define related with driver feature */
#define PROXIMITY_MODE
#undef EDGE_SWIPE
#define USE_CUSTOM_REZERO
#define DEBUG_HOVER
#define USE_SHUTDOWN_CB
#ifdef CONFIG_INPUT_BOOSTER
#define TSP_BOOSTER
#endif
#if defined(CONFIG_GLOVE_TOUCH)
#define GLOVE_MODE
#endif
/* #define SIDE_TOUCH */
#define CHECK_PR_NUMBER
#define REPORT_2D_W

/* #define USE_GUEST_THREAD */
#define USE_DETECTION_FLAG_2
#define USE_STYLUS
/* #define SKIP_UPDATE_FW_ON_PROBE */
/* #define REPORT_2D_Z */
/* #define REPORT_ORIENTATION */
/* #define USE_SENSOR_SLEEP */
#define USE_ACTIVE_REPORT_RATE
/**************************************/
/**************************************/

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#define SYNAPTICS_DEVICE_NAME	"SYNAPTICS"
#define DRIVER_NAME "synaptics_rmi4_i2c"

#define SYNAPTICS_HW_RESET_TIME	100
#define SYNAPTICS_REZERO_TIME 100
#define SYNAPTICS_POWER_MARGIN_TIME	150
#define SYNAPTICS_DEEPSLEEP_TIME	20

#define TSP_FACTEST_RESULT_PASS		2
#define TSP_FACTEST_RESULT_FAIL		1
#define TSP_FACTEST_RESULT_NONE		0

#define SYNAPTICS_MAX_FW_PATH	64

#define SYNAPTICS_DEFAULT_UMS_FW "/sdcard/synaptics.fw"

/* Define for Firmware file image format */
#define FIRMWARE_IMG_HEADER_MAJOR_VERSION_OFFSET	(0x07)
#define NEW_IMG_MAJOR_VERSION	(0x10)

/* Previous firmware image format */
#define OLD_IMG_CHECK_PR_BIT_BIN_OFFSET	(0x06)
#define OLD_IMG_DATE_OF_FIRMWARE_BIN_OFFSET	(0x016D00)
#define OLD_IMG_IC_REVISION_BIN_OFFSET	(0x016D02)
#define OLD_IMG_FW_VERSION_BIN_OFFSET	(0x016D03)
#define OLD_IMG_PR_NUMBER_0TH_BYTE_BIN_OFFSET	(0x50)

/* New firmware image format(PR number is loaded defaultly) */
#define DATE_OF_FIRMWARE_BIN_OFFSET	(0x00B0)
#define IC_REVISION_BIN_OFFSET	(0x00B2)
#define FW_VERSION_BIN_OFFSET	(0x00B3)
#define PR_NUMBER_0TH_BYTE_BIN_OFFSET	(0x74)

#define PDT_PROPS (0X00EF)
#define PDT_START (0x00E9)
#define PDT_END (0x000A)
#define PDT_ENTRY_SIZE (0x0006)
#define PAGES_TO_SERVICE (10)
#define PAGE_SELECT_LEN (2)

#define SYNAPTICS_RMI4_F01 (0x01)
#define SYNAPTICS_RMI4_F11 (0x11)
#define SYNAPTICS_RMI4_F12 (0x12)
#define SYNAPTICS_RMI4_F1A (0x1a)
#define SYNAPTICS_RMI4_F34 (0x34)
#define SYNAPTICS_RMI4_F51 (0x51)
#define SYNAPTICS_RMI4_F54 (0x54)
#define SYNAPTICS_RMI4_F55 (0x55)
#define SYNAPTICS_RMI4_F60 (0x60)
#define SYNAPTICS_RMI4_FDB (0xdb)

#define SYNAPTICS_RMI4_PRODUCT_INFO_SIZE 2
#define SYNAPTICS_RMI4_DATE_CODE_SIZE 3
#define SYNAPTICS_RMI4_PRODUCT_ID_SIZE 10
#define SYNAPTICS_RMI4_BUILD_ID_SIZE 3
#define SYNAPTICS_RMI4_PRODUCT_ID_LENGTH 10
#define SYNAPTICS_RMI4_PACKAGE_ID_SIZE 4

#define MAX_NUMBER_OF_BUTTONS 4
#define MAX_INTR_REGISTERS 4
#define F12_FINGERS_TO_SUPPORT 10
#define MAX_NUMBER_OF_FINGERS (F12_FINGERS_TO_SUPPORT)

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define INVALID_X	65535
#define INVALID_Y	65535

/* Define for Object type and status(F12_2D_data(N)/0).
 * Each 3-bit finger status field represents the following:
 * 000 = finger not present
 * 001 = finger present and data accurate
 * 010 = stylus pen (passive pen)
 * 011 = palm touch
 * 100 = not used
 * 101 = hover
 * 110 = glove touch
 */
#define OBJECT_NOT_PRESENT		(0x00)
#define OBJECT_FINGER			(0x01)
#define OBJECT_PASSIVE_STYLUS	(0x02)
#define OBJECT_PALM				(0x03)
#define OBJECT_UNCLASSIFIED		(0x04)
#define OBJECT_HOVER			(0x05)
#define OBJECT_GLOVE			(0x06)

/* Define for Data report enable Mask(F12_2D_CTRL28) */
#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

/* Define for Feature enable(F12_2D_CTRL26)
 * bit[0] : represent enable or disable glove mode(high sensitivity mode)
 * bit[1] : represent enable or disable cover mode.
 *		(cover is on lcd, change sensitivity to prevent unintended touch)
 * bit[2] : represent enable or disable fast glove mode.
 *		(change glove mode entering condition to be faster)
 */
#define GLOVE_DETECTION_EN (1 << 0)
#define CLOSED_COVER_EN (1 << 1)
#define FAST_GLOVE_DECTION_EN (1 << 2)

#define CLEAR_COVER_MODE_EN	(CLOSED_COVER_EN | GLOVE_DETECTION_EN)
#define FLIP_COVER_MODE_EN	(CLOSED_COVER_EN)

#ifdef PROXIMITY_MODE
#define F51_FINGER_TIMEOUT 50 /* ms */
#define HOVER_Z_MAX (255)

#define F51_PROXIMITY_ENABLES_OFFSET (0)
/* Define for proximity enables(F51_CUSTOM_CTRL00) */
#define FINGER_HOVER_EN (1 << 0)
#define AIR_SWIPE_EN (1 << 1)
#define LARGE_OBJ_EN (1 << 2)
#define HOVER_PINCH_EN (1 << 3)
#define LARGE_OBJ_WAKEUP_GESTURE_EN (1 << 4)
/* Reserved 5 */
#define ENABLE_HANDGRIP_RECOG (1 << 6)
#define SLEEP_PROXIMITY (1 << 7)

#define F51_GENERAL_CONTROL_OFFSET (1)
/* Define for General Control(F51_CUSTOM_CTRL01) */
#define JIG_TEST_EN	(1 << 0)
#define JIG_COMMAND_EN	(1 << 1)
#define NO_PROXIMITY_ON_TOUCH (1 << 2)
#define CONTINUOUS_LOAD_REPORT (1 << 3)
#define HOST_REZERO_COMMAND (1 << 4)
#define EDGE_SWIPE_EN (1 << 5)
#define HSYNC_STATUS (1 << 6)
#define HOST_ID (1 << 7)

#define F51_GENERAL_CONTROL_2_OFFSET (2)
/* Define for General Control(F51_CUSTOM_CTRL02) */
#define FACE_DETECTION_EN	(1 << 0)
#define SIDE_BUTTONS_EN	(1 << 1)
#define SIDE_BUTTONS_PRODUCTION_TEST	(1 << 2)
#define SIDE_TOUCH_ONLY_ACTIVE	(1 << 3)
#define SIDE_CHANNEL_DISABLE	(1 << 4)
#define ENTER_SLEEP_MODE		(1 << 5)
/* Reserved 2 ~ 7 */

/* Define for proximity Controls(F51_CUSTOM_QUERY04) */
#define HAS_FINGER_HOVER (1 << 0)
#define HAS_AIR_SWIPE (1 << 1)
#define HAS_LARGE_OBJ (1 << 2)
#define HAS_HOVER_PINCH (1 << 3)
#define HAS_EDGE_SWIPE (1 << 4)
#define HAS_SINGLE_FINGER (1 << 5)
#define HAS_GRIP_SUPPRESSION (1 << 6)
#define HAS_PALM_REJECTION (1 << 7)

/* Define for proximity Controls 2(F51_CUSTOM_QUERY05) */
#define HAS_PROFILE_HANDEDNESS (1 << 0)
#define HAS_LOWG (1 << 1)
#define HAS_FACE_DETECTION (1 << 2)
#define HAS_SIDE_BUTTONS (1 << 3)
#define HAS_CAMERA_GRIP_DETECTION (1 << 4)
/* Reserved 5 ~ 7 */

/* Define for Detection flag 2(F51_CUSTOM_DATA06) */
#define HAS_HAND_EDGE_SWIPE_DATA (1 << 0)
#define SIDE_BUTTON_DETECTED (1 << 1)
/* Reserved 2 ~ 7 */

#define F51_DATA_RESERVED_SIZE	(1)
#define F51_DATA_1_SIZE (4)	/* FINGER_HOVER */
#define F51_DATA_2_SIZE (1)	/* HOVER_PINCH */
#define F51_DATA_3_SIZE (1)	/* AIR_SWIPE | LARGE_OBJ */
#define F51_DATA_4_SIZE (2)	/* SIDE_BUTTON */
#define F51_DATA_5_SIZE	(1)	/* CAMERA_GRIP_DETECTION */
#define F51_DATA_6_SIZE (2)	/* DETECTION_FLAG2 */

#ifdef EDGE_SWIPE
#define EDGE_SWIPE_WIDTH_MAX	255
#define EDGE_SWIPE_SUMSIZE_MAX	255
#define EDGE_SWIPE_PALM_MAX		1

#define EDGE_SWIPE_WITDH_X_OFFSET	5
#define EDGE_SWIPE_AREA_OFFSET	7
#endif

#ifdef SIDE_TOUCH
#define MAX_SIDE_BUTTONS	8
#define NUM_OF_ACTIVE_SIDE_BUTTONS	6
#endif
#endif

#define SYN_I2C_RETRY_TIMES 3
#define MAX_F11_TOUCH_WIDTH 15

#define CHECK_STATUS_TIMEOUT_MS 200
#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12
#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

/* Define for Device Control(F01_RMI_CTRL00) */
#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_ON (1 << 2)
/* Reserved 3 ~ 4 */
#define CHARGER_CONNECTED (1 << 5)
#define REPORT_RATE (1 << 6)
#define CONFIGURED (1 << 7)

#define TSP_NEEDTO_REBOOT	(-ECONNREFUSED)
#define MAX_TSP_REBOOT		3

#define SYNAPTICS_BL_ID_0_OFFSET	0
#define SYNAPTICS_BL_ID_1_OFFSET	1
#define SYNAPTICS_BL_MINOR_REV_OFFSET	2
#define SYNAPTICS_BL_MAJOR_REV_OFFSET	3

#define SYNAPTICS_BOOTLOADER_ID_SIZE	4

/* Below version is represent that it support guest thread functionality. */
#define BL_MAJOR_VER_OF_GUEST_THREAD	0x36	/* '6' */
#define BL_MINOR_VER_OF_GUEST_THREAD	0x34	/* '4' */

#define SYNAPTICS_ACCESS_READ	false
#define SYNAPTICS_ACCESS_WRITE	true

/* Below offsets are defined manually.
 * So please keep in your mind when use this. it can be changed based on
 * firmware version you can check it debug_address sysfs node
 * (sys/class/sec/tsp/cmd).
 * If it is possible to replace that getting address from IC,
 * I recommend the latter than former.
 */
#ifdef PROXIMITY_MODE
#define MANUAL_DEFINED_OFFSET_GRIP_EDGE_EXCLUSION_RX	(32)
#endif
#ifdef SIDE_TOUCH
#define MANUAL_DEFINED_OFFSET_SIDEKEY_THRESHOLD	(47)
#endif
#ifdef USE_STYLUS
#define MANUAL_DEFINED_OFFSET_FORCEFINGER_ON_EDGE	(61)
#endif
/* Enum for each product id */
enum synaptics_product_ids {
	SYNAPTICS_PRODUCT_ID_NONE = 0,
	SYNAPTICS_PRODUCT_ID_S5000,
	SYNAPTICS_PRODUCT_ID_S5050,
	SYNAPTICS_PRODUCT_ID_S5100,
	SYNAPTICS_PRODUCT_ID_S5700,
	SYNAPTICS_PRODUCT_ID_MAX
};

/* Define for Revision of IC */
#define SYNAPTICS_IC_REVISION_A0	0xA0
#define SYNAPTICS_IC_REVISION_A1	0xA1
#define SYNAPTICS_IC_REVISION_A2	0xA2
#define SYNAPTICS_IC_REVISION_A3	0xA3
#define SYNAPTICS_IC_REVISION_B0	0xB0
#define SYNAPTICS_IC_REVISION_B1	0xB1
#define SYNAPTICS_IC_REVISION_B2	0xB2
#define SYNAPTICS_IC_REVISION_AF	0xAF
#define SYNAPTICS_IC_REVISION_BF	0xBF

/* Release event type for manual release.. */
#define RELEASE_TYPE_FINGER	(1 << 0)
#define RELEASE_TYPE_SIDEKEY	(1 << 1)

#define RELEASE_TYPE_ALL (RELEASE_TYPE_FINGER | RELEASE_TYPE_SIDEKEY)

#ifdef USE_ACTIVE_REPORT_RATE
#define	SYNAPTICS_RPT_RATE_30HZ_VAL	(0x50)
#define	SYNAPTICS_RPT_RATE_60HZ_VAL	(0x16)
#define	SYNAPTICS_RPT_RATE_90HZ_VAL	(0x04)

enum synaptics_report_rate {
	SYNAPTICS_RPT_RATE_START = 0,
	SYNAPTICS_RPT_RATE_90HZ = SYNAPTICS_RPT_RATE_START,
	SYNAPTICS_RPT_RATE_60HZ,
	SYNAPTICS_RPT_RATE_30HZ,
	SYNAPTICS_RPT_RATE_END
};
#endif

struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl24_is_present:1;
				unsigned char ctrl25_is_present:1;
				unsigned char ctrl26_is_present:1;
				unsigned char ctrl27_is_present:1;
				unsigned char ctrl28_is_present:1;
				unsigned char ctrl29_is_present:1;
				unsigned char ctrl30_is_present:1;
				unsigned char ctrl31_is_present:1;
			} __packed;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __packed;
			struct {
				unsigned char data8_is_present:1;
				unsigned char data9_is_present:1;
				unsigned char data10_is_present:1;
				unsigned char data11_is_present:1;
				unsigned char data12_is_present:1;
				unsigned char data13_is_present:1;
				unsigned char data14_is_present:1;
				unsigned char data15_is_present:1;
			} __packed;
		};
		unsigned char data[3];
	};
};

struct synaptics_rmi4_f12_query_10 {
	union {
		struct {
			unsigned char f12_query10_b0__4:5;
			unsigned char glove_mode_feature:1;
			unsigned char f12_query10_b6__7:2;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f12_ctrl_8 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char tx_pitch_lsb;
			unsigned char tx_pitch_msb;
			unsigned char low_rx_clip;
			unsigned char high_rx_clip;
			unsigned char low_tx_clip;
			unsigned char high_tx_clip;
			unsigned char num_of_rx;
			unsigned char num_of_tx;
		};
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_ctrl_9 {
	union {
		struct {
			unsigned char touch_threshold;
			unsigned char lift_hysteresis;
			unsigned char small_z_scale_factor_lsb;
			unsigned char small_z_scale_factor_msb;
			unsigned char large_z_scale_factor_lsb;
			unsigned char large_z_scale_factor_msb;
			unsigned char small_large_boundary;
			unsigned char wx_scale;
			unsigned char wx_offset;
			unsigned char wy_scale;
			unsigned char wy_offset;
			unsigned char x_size_lsb;
			unsigned char x_size_msb;
			unsigned char y_size_lsb;
			unsigned char y_size_msb;
			unsigned char gloved_finger;
		};
		unsigned char data[16];
	};
};

struct synaptics_rmi4_f12_ctrl_11 {
	union {
		struct {
			unsigned char small_corner;
			unsigned char large_corner;
			unsigned char jitter_filter_strength;
			unsigned char x_minimum_z;
			unsigned char y_minimum_z;
			unsigned char x_maximum_z;
			unsigned char y_maximum_z;
			unsigned char x_amplitude;
			unsigned char y_amplitude;
			unsigned char gloved_finger_jitter_filter_strength;
		};
		unsigned char data[10];
	};
};

struct synaptics_rmi4_f12_ctrl_23 {
	union {
		struct {
			unsigned char obj_type_enable;
			unsigned char max_reported_objects;
		};
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f12_ctrl_26 {
	union {
		struct {
			unsigned char feature_enable;
		};
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f12_finger_data {
	unsigned char object_type_and_status;
	unsigned char x_lsb;
	unsigned char x_msb;
	unsigned char y_lsb;
	unsigned char y_msb;
#ifdef REPORT_2D_Z
	unsigned char z;
#endif
#ifdef REPORT_2D_W
	unsigned char wx;
	unsigned char wy;
#endif
};

struct synaptics_rmi4_f1a_query {
	union {
		struct {
			unsigned char max_button_count:3;
			unsigned char reserved:5;
			unsigned char has_general_control:1;
			unsigned char has_interrupt_enable:1;
			unsigned char has_multibutton_select:1;
			unsigned char has_tx_rx_map:1;
			unsigned char has_perbutton_threshold:1;
			unsigned char has_release_threshold:1;
			unsigned char has_strongestbtn_hysteresis:1;
			unsigned char has_filter_strength:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f1a_control_0 {
	union {
		struct {
			unsigned char multibutton_report:2;
			unsigned char filter_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control {
	struct synaptics_rmi4_f1a_control_0 general_control;
	unsigned char button_int_enable;
	unsigned char multi_button;
	unsigned char *txrx_map;
	unsigned char *button_threshold;
	unsigned char button_release_threshold;
	unsigned char strongest_button_hysteresis;
	unsigned char filter_strength;
};

struct synaptics_rmi4_f1a_handle {
	int button_bitmask_size;
	unsigned char max_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
};

struct synaptics_rmi4_f34_ctrl_3 {
	union {
		struct {
			unsigned char fw_release_month;
			unsigned char fw_release_date;
			unsigned char fw_release_revision;
			unsigned char fw_release_version;
		};
		unsigned char data[4];
	};
};

#ifdef PROXIMITY_MODE
struct synaptics_rmi4_f51_query {
	union {
		struct {
			unsigned char query_register_count;
			unsigned char data_register_count;
			unsigned char control_register_count;
			unsigned char command_register_count;
			unsigned char proximity_controls;
			unsigned char proximity_controls_2;
		};
		unsigned char data[6];
	};
};

struct synaptics_rmi4_f51_data {
	union {
		struct {
			unsigned char finger_hover_det:1;
			unsigned char air_swipe_det:1;
			unsigned char large_obj_det:1;
			unsigned char hover_pinch_det:1;
			unsigned char lowg_detected:1;
			unsigned char profile_handedness_status:2;
			unsigned char face_detect:1;

			unsigned char hover_finger_x_4__11;
			unsigned char hover_finger_y_4__11;
			unsigned char hover_finger_xy_0__3;
			unsigned char hover_finger_z;
		} __packed;
		unsigned char proximity_data[5];
	};

#ifdef EDGE_SWIPE
	union {
		struct {
			unsigned char edge_swipe_x_lsb;
			unsigned char edge_swipe_x_msb;
			unsigned char edge_swipe_y_lsb;
			unsigned char edge_swipe_y_msb;
			unsigned char edge_swipe_z;
			unsigned char edge_swipe_wx;
			unsigned char edge_swipe_wy;
			unsigned char edge_swipe_mm;
			signed char edge_swipe_dg;
		} __packed;
		unsigned char edge_swipe_data[9];
	};
#endif
#ifdef SIDE_TOUCH
	union {
		struct {
			unsigned char side_button_leading;
			unsigned char side_button_trailing;
		} __packed;
		unsigned char side_button_data[2];
	};
#endif
};

#ifdef EDGE_SWIPE
struct synaptics_rmi4_edge_swipe {
	int sumsize;
	int palm;
	int wx;
	int wy;
};
#endif

struct synaptics_rmi4_f51_handle {
/* CTRL */
	unsigned char proximity_enables;		/* F51_CUSTOM_CTRL00 */
	unsigned short proximity_enables_addr;
	unsigned char general_control;			/* F51_CUSTOM_CTRL01 */
	unsigned short general_control_addr;
	unsigned char general_control_2;		/* F51_CUSTOM_CTRL02 */
	unsigned short general_control_2_addr;
#ifdef PROXIMITY_MODE
	unsigned short grip_edge_exclusion_rx_addr;
#endif
#ifdef SIDE_TOUCH
	unsigned short sidebutton_tapthreshold_addr;
#endif
#ifdef USE_STYLUS
	unsigned short forcefinger_onedge_addr;
#endif
/* QUERY */
	unsigned char proximity_controls;		/* F51_CUSTOM_QUERY04 */
	unsigned char proximity_controls_2;		/* F51_CUSTOM_QUERY05 */
/* DATA */
	unsigned short detection_flag_2_addr;	/* F51_CUSTOM_DATA06 */
	unsigned short edge_swipe_data_addr;	/* F51_CUSTOM_DATA07 */
#ifdef EDGE_SWIPE
	struct synaptics_rmi4_edge_swipe edge_swipe_data;
#endif
	unsigned short side_button_data_addr;	/* F51_CUSTOM_DATA04 */
	bool finger_is_hover;	/* To print hover log */
};
#endif

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	unsigned char query_base_addr;
	unsigned char cmd_base_addr;
	unsigned char ctrl_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count;
	unsigned char fn_number;
};

/*
 * synaptics_rmi4_fn_full_addr - full 16-bit base addresses
 * @query_base: 16-bit base address for query registers
 * @cmd_base: 16-bit base address for data registers
 * @ctrl_base: 16-bit base address for command registers
 * @data_base: 16-bit base address for control registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

struct synaptics_rmi4_f12_extra_data {
	unsigned char data1_offset;
	unsigned char data15_offset;
	unsigned char data15_size;
	unsigned char data15_data[(F12_FINGERS_TO_SUPPORT + 7) / 8];
};

/*
 * struct synaptics_rmi4_fn - function handler data structure
 * @fn_number: function number
 * @num_of_data_sources: number of data sources
 * @num_of_data_points: maximum number of fingers supported
 * @size_of_data_register_block: data register block size
 * @data1_offset: offset to data1 register from data base address
 * @intr_reg_num: index to associated interrupt register
 * @intr_mask: interrupt mask
 * @full_addr: full 16-bit base addresses of function registers
 * @link: linked list for function handlers
 * @data_size: size of private data
 * @data: pointer to private data
 */
struct synaptics_rmi4_fn {
	unsigned char fn_number;
	unsigned char num_of_data_sources;
	unsigned char num_of_data_points;
	unsigned char size_of_data_register_block;
	unsigned char intr_reg_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr full_addr;
	struct list_head link;
	int data_size;
	void *data;
	void *extra;
};

/*
 * struct synaptics_rmi4_device_info - device information
 * @version_major: rmi protocol major version number
 * @version_minor: rmi protocol minor version number
 * @manufacturer_id: manufacturer id
 * @product_props: product properties information
 * @product_info: product info array
 * @date_code: device manufacture date
 * @tester_id: tester id array
 * @serial_number: device serial number
 * @product_id_string: device product id
 * @support_fn_list: linked list for function handlers
 * @exp_fn_list: linked list for expanded function handlers
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	unsigned char date_code[SYNAPTICS_RMI4_DATE_CODE_SIZE];
	unsigned short tester_id;
	unsigned short serial_number;
	unsigned char product_id_string[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char build_id[SYNAPTICS_RMI4_BUILD_ID_SIZE];
	unsigned int package_id;
	unsigned int package_rev;
	unsigned int pr_number;
	struct list_head support_fn_list;
	struct list_head exp_fn_list;
};

/**
 * struct synaptics_finger - Represents fingers.
 * @ state: finger status.
 * @ mcount: moving counter for debug.
 * @ stylus: represent stylus..
 */
struct synaptics_finger {
	unsigned char state;
	unsigned short mcount;
#ifdef USE_STYLUS
	bool stylus;
#endif
};

struct synaptics_rmi4_f12_handle {
/* CTRL */
	unsigned short ctrl11_addr;		/* F12_2D_CTRL11 : for jitter level*/
	unsigned short ctrl15_addr;		/* F12_2D_CTRL15 : for finger amplitude threshold */

	unsigned short ctrl26_addr;		/* F12_2D_CTRL26 : for glove mode */
	unsigned char feature_enable;	/* F12_2D_CTRL26 */
	unsigned short ctrl28_addr;		/* F12_2D_CTRL28 : for report data */
	unsigned char report_enable;	/* F12_2D_CTRL28 */
/* QUERY */
	unsigned char glove_mode_feature;	/* F12_2D_QUERY_10 */
};

/*
 * struct synaptics_rmi4_data - rmi4 device instance data
 * @i2c_client: pointer to associated i2c client
 * @input_dev: pointer to associated input device
 * @board: constant pointer to platform data
 * @rmi4_mod_info: device information
 * @regulator: pointer to associated regulator
 * @rmi4_io_ctrl_mutex: mutex for i2c i/o control
 * @early_suspend: instance to support early suspend power management
 * @current_page: current page in sensor to acess
 * @button_0d_enabled: flag for 0d button support
 * @full_pm_cycle: flag for full power management cycle in early suspend stage
 * @num_of_intr_regs: number of interrupt registers
 * @f01_query_base_addr: query base address for f01
 * @f01_cmd_base_addr: command base address for f01
 * @f01_ctrl_base_addr: control base address for f01
 * @f01_data_base_addr: data base address for f01
 * @irq: attention interrupt
 * @sensor_max_x: sensor maximum x value
 * @sensor_max_y: sensor maximum y value
 * @irq_enabled: flag for indicating interrupt enable status
 * @touch_stopped: flag to stop interrupt thread processing
 * @fingers_on_2d: flag to indicate presence of fingers in 2d area
 * @sensor_sleep: flag to indicate sleep state of sensor
 * @wait: wait queue for touch data polling in interrupt thread
 * @i2c_read: pointer to i2c read function
 * @i2c_write: pointer to i2c write function
 * @irq_enable: pointer to irq enable function
 */
struct synaptics_rmi4_data {
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
	const struct synaptics_rmi4_platform_data *board;
	struct synaptics_rmi4_device_info rmi4_mod_info;
	struct mutex rmi4_reset_mutex;
	struct mutex rmi4_io_ctrl_mutex;
	struct mutex rmi4_reflash_mutex;
	struct timer_list f51_finger_timer;
	struct delayed_work		work_init_irq;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	const char *firmware_name;

	struct completion init_done;
	struct synaptics_finger finger[MAX_NUMBER_OF_FINGERS];

	unsigned char current_page;
	unsigned char button_0d_enabled;
	unsigned char full_pm_cycle;
	unsigned char num_of_rx;
	unsigned char num_of_tx;
	unsigned int num_of_node;
	unsigned char num_of_fingers;
	unsigned char max_touch_width;
	unsigned char intr_mask[MAX_INTR_REGISTERS];
	unsigned short num_of_intr_regs;
	unsigned char *button_txrx_mapping;
	unsigned short f01_query_base_addr;
	unsigned short f01_cmd_base_addr;
	unsigned short f01_ctrl_base_addr;
	unsigned short f01_data_base_addr;
	unsigned short f34_ctrl_base_addr;
	int irq;
	int sensor_max_x;
	int sensor_max_y;
	bool flash_prog_mode;
	bool irq_enabled;
	bool touch_stopped;
	bool fingers_on_2d;
	bool f51_finger;
	bool sensor_sleep;
	bool stay_awake;
	bool staying_awake;
	bool tsp_probe;

	enum synaptics_product_ids product_id;			/* product id of ic */
	int ic_revision_of_ic;		/* revision of reading from IC */
	int fw_version_of_ic;		/* firmware version of IC */
	int ic_revision_of_bin;		/* revision of reading from binary */
	int fw_version_of_bin;		/* firmware version of binary */
	int fw_release_date_of_ic;	/* Config release data from IC */
	int panel_revision;			/* Octa panel revision */
	unsigned char bootloader_id[SYNAPTICS_BOOTLOADER_ID_SIZE];	/* Bootloader ID */
	bool doing_reflash;
	int rebootcount;

#ifdef GLOVE_MODE
	bool fast_glove_state;
	bool touchkey_glove_mode_status;
#endif
	unsigned char ddi_type;

	struct synaptics_rmi4_f12_handle f12;
#ifdef PROXIMITY_MODE
	struct synaptics_rmi4_f51_handle *f51;
#endif
	struct delayed_work rezero_work;

	struct mutex rmi4_device_mutex;
#ifdef SIDE_TOUCH
	unsigned char sidekey_data;
#endif
	bool use_stylus;
#ifdef SYNAPTICS_RMI_INFORM_CHARGER
	int ta_status;
	void (*register_cb)(struct synaptics_rmi_callbacks *);
	struct synaptics_rmi_callbacks callbacks;
#endif
	bool use_deepsleep;
#ifdef USE_GUEST_THREAD
	unsigned char guest_pkt_dbg_level;
#endif
	int (*i2c_read)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*i2c_write)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*stop_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*start_device)(struct synaptics_rmi4_data *rmi4_data);
	void (*sleep_device)(struct synaptics_rmi4_data *rmi4_data);
	void (*wake_device)(struct synaptics_rmi4_data *rmi4_data);
};

enum exp_fn {
	RMI_DEV = 0,
	RMI_F54,
	RMI_FW_UPDATER,
	RMI_DB,
	RMI_GUEST,
	RMI_LAST,
};

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	bool initialized;
	int (*func_init)(struct synaptics_rmi4_data *rmi4_data);
	int (*func_reinit)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
	struct list_head link;
};

struct synaptics_rmi4_exp_fn_ptr {
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
};

int synaptics_rmi4_new_function(enum exp_fn fn_type,
		struct synaptics_rmi4_data *rmi4_data,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		int (*func_reinit)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
				unsigned char intr_mask));

int rmidev_module_register(struct synaptics_rmi4_data *rmi4_data);
int rmi4_f54_module_register(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_f54_set_control(struct synaptics_rmi4_data *rmi4_data);
int rmi4_fw_update_module_register(struct synaptics_rmi4_data *rmi4_data);
int rmidb_module_register(struct synaptics_rmi4_data *rmi4_data);
int rmi_guest_module_register(struct synaptics_rmi4_data *rmi4_data);

int synaptics_fw_updater(unsigned char *fw_data);
extern int synaptics_rmi4_fw_update_on_probe(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_f12_ctrl11_set(struct synaptics_rmi4_data *rmi4_data, unsigned char data);
int synaptics_rmi4_set_tsp_test_result_in_config(int value);
int synaptics_rmi4_read_tsp_test_result(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_access_register(struct synaptics_rmi4_data *rmi4_data,
				bool mode, unsigned short address, int length, unsigned char *value);
void synpatics_rmi4_release_all_event(struct synaptics_rmi4_data *rmi4_data, unsigned char type);
int fwu_do_read_config(void);
#ifdef PROXIMITY_MODE
int synaptics_rmi4_proximity_enables(struct synaptics_rmi4_data *rmi4_data, unsigned char enables);
#endif
#ifdef GLOVE_MODE
int synaptics_rmi4_glove_mode_enables(struct synaptics_rmi4_data *rmi4_data);
#endif
#ifdef SYNAPTICS_RMI_INFORM_CHARGER
extern void synaptics_tsp_register_callback(struct synaptics_rmi_callbacks *cb);
#endif

extern struct class *sec_class;

static inline ssize_t synaptics_rmi4_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_warn(dev, "%s Attempted to read from write-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t synaptics_rmi4_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	dev_warn(dev, "%s Attempted to write to read-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static inline void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}
#endif
