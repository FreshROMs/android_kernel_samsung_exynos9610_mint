/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
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

#ifndef __IST40XX_H__
#define __IST40XX_H__

/*
 * Support F/W ver : ~ IST40XX v1.0.0.0
 * Support CmCs ver : v1.0
 * Support IC : IST4070
 * Release : 2016.11.15 by Drake
 */


#include <linux/input/sec_cmd.h>

#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include <linux/pm_runtime.h>
#include <linux/atomic.h>

#define SECURE_TOUCH_ENABLED	1
#define SECURE_TOUCH_DISABLED	0
#endif


#define FIRMWARE_PATH_LENGTH		(64)
#define FIRMWARE_PATH			("tsp_imagis/")

#define IMAGIS_TSP_DD_VERSION		("1.0.0.0")

#define IMAGIS_IST4050			(1) /* 4050*/
#define IMAGIS_IST4070			(2) /* 4070*/

#define IMAGIS_TSP_IC			IMAGIS_IST4050 // Select using IC
#define TSP_CHIP_VENDOR			("IMAGIS")

/* IST40XX FUNCTION SET */
#define USE_OPEN_CLOSE
#define SEC_FACTORY_MODE
#define IST40XX_NOISE_MODE

/* TCLM MODE */
#include <linux/input/sec_tclm_v2.h>
#ifdef CONFIG_INPUT_TOUCHSCREEN_TCLMV2
#define TCLM_CONCEPT
#endif

#define IST40XX_NVM_OFFSET_FAC_RESULT		0
#define IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT	1
#define IST40XX_NVM_OFFSET_CAL_COUNT		2
#define IST40XX_NVM_OFFSET_TUNE_VERSION		3
#define IST40XX_NVM_OFFSET_CAL_POSITION		4
#define IST40XX_NVM_OFFSET_HISTORY_QUEUE_COUNT	5
#define IST40XX_NVM_OFFSET_HISTORY_QUEUE_LASTP	6
#define IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO	7
#define IST40XX_NVM_OFFSET_HISTORY_QUEUE_SIZE	10

#define IST40XX_NVM_OFFSET_LENGTH	(IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO + \
						IST40XX_NVM_OFFSET_HISTORY_QUEUE_SIZE)

/* ----------------------------------------
 * write 0xE4 [ 11 | 10 | 01 | 00 ]
 * MSB <-------------------> LSB
 * read 0xE4
 * mapping sequnce : LSB -> MSB
 * struct sec_ts_test_result {
 * * assy : front + OCTA assay
 * * module : only OCTA
 *   union {
 *       struct {
 *           u8 assy_count:2;   -> 00
 *           u8 assy_result:2;  -> 01
 *           u8 module_count:2; -> 10
 *           u8 module_result:2;-> 11
 *       } __attribute__ ((packed));
 *       unsigned char data[1];
 *   };
 *};
 * ----------------------------------------
 */
struct ts_test_result {
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
#define TEST_OCTA_MODULE		1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE			0
#define TEST_OCTA_FAIL			1
#define TEST_OCTA_PASS			2

/* IST40XX FUNCTION SET */
#define IST40XX_DEFAULT_CHIP_ID		(0x4070)
#if (IMAGIS_TSP_IC == IMAGIS_IST4070)
#define TSP_CHIP_NAME			("IST4070")
#define IST40XX_CHIP_ID			(0x4070)
#define IST40XX_MAX_NODE_NUM		(27 * 27)
#define IST40XX_MAX_SELF_NODE_NUM	(54)
#elif (IMAGIS_TSP_IC == IMAGIS_IST4050)
#define TSP_CHIP_NAME			("IST4050")
#define IST40XX_CHIP_ID			(0x4050)
#define IST40XX_MAX_NODE_NUM		(27 * 27)
#define IST40XX_MAX_SELF_NODE_NUM	(54)
#endif

#define IST40XX_MAX_FINGER_ID		(15)
#define IST40XX_MAX_MAJOR		(0xFF)
#define IST40XX_MAX_MINOR		(0xFF)
#define IST40XX_MAX_Z			(0xFFF)

#define IST40XX_INITIAL_VALUE		(0x8EAD8EAD)
#define IST40XX_REC_VALUE		(0x8CA0D00E)
#define IST40XX_REC_FILENAME_SIZE	(128)
#define IST40XX_EXCEPT_MASK		(0xFFFFFF00)
#define IST40XX_EXCEPT_VALUE		(0xE11CE900)
#define IST40XX_MAX_EXCEPT_SIZE		(2)
#define IST40XX_EXCEPT_INTEGRITY	(0xE11CE901)
#define IST40XX_LPM_VALUE		(0x193030DE)
#define IST40XX_MISCALIB_MASK		(0xFFFF0000)
#define IST40XX_MISCALIB_MSG		(0x3CAB0000)
#define IST40XX_MISCALIB_VAL(n)		(n & 0xFFFF)

#define CALIB_MSG_MASK			(0xF0000FFF)
#define CALIB_MSG_VALID			(0x80000CAB)
#define CALIB_WAIT_TIME			(50)    /* unit : 100msec */
#define CALIB_MAX_I2C_FAIL_CNT		(10)
#define CALIB_TO_GAP(n)			((n >> 16) & 0xFFF)
#define CALIB_TO_STATUS(n)		((n >> 12) & 0xF)
#define CALIB_TO_OS_VALUE(n)		((n >> 12) & 0xFFFF)
#define IST40XX_MAX_CALIB_SIZE		(3)

#define IST40XX_JIG_TOUCH		(0xC0)
#define IST40XX_START_SCAN		(2)
#define IST40XX_ENABLE			(1)
#define IST40XX_DISABLE			(0)
#define IST40XX_SPAY			(1 << 1)
#define IST40XX_AOD			(1 << 2)
#define IST40XX_SINGLETAP		(1 << 3)
#define IST40XX_FOD			(1 << 4)
#define IST40XX_DOUBLETAP_WAKEUP	(1 << 5)

#define EID_GESTURE			(2)

#define G_TYPE_SWIPE			(0)
#define G_TYPE_DOUBLETAP		(1)
#define G_TYPE_PRESSURE			(2)
#define G_TYPE_PRESS			(3)
#define G_TYPE_SINGLETAP		(4)

#define G_ID_SWIPE_UP			(0)
#define G_ID_AOD_DOUBLETAP		(0)
#define G_ID_DOUBLETAP_WAKEUP		(1)
#define G_ID_PRESSURE_PRESS		(0)
#define G_ID_PRESSURE_RELEASE		(1)
#define G_ID_LONG_PRESS_LP		(0)
#define G_ID_PRESS_NP			(1)
#define G_ID_SINGLETAP			(0)
#define G_ID_FOD_LONG			(0)
#define G_ID_FOD_NORMAL		(1)
#define G_ID_FOD_RELEASE	(2)

#define NOISE_MODE_TA			(0)
#define NOISE_MODE_CALL			(1)
#define NOISE_MODE_COVER		(2)
#define NOISE_MODE_GLOVE		(3)
#define NOISE_MODE_EDGE			(4)
#define NOISE_MODE_SENSITIVITY		(5)
#define NOISE_MODE_TOUCHABLE		(6)
#define NOISE_MODE_POWER		(8)
#define NOISE_MODE_REJECTZONE		(9)
#define NOISE_MODE_HALFAOD		(10)

/* retry count */
#define IST40XX_MAX_RETRY_CNT		(3)

/* Log message */
#define DEV_ERR				(1)
#define DEV_WARN			(2)
#define DEV_INFO			(3)
#define DEV_NOTI			(4)
#define DEV_DEBUG			(5)
#define DEV_VERB			(6)

#define IST40XX_LOG_TAG			("[sec_input][ TSP ]")
#define IST40XX_LOG_LEVEL		DEV_NOTI

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <linux/sec_debug.h>

#define tsp_err(fmt, ...)    \
({    \
    if (IST40XX_LOG_LEVEL<DEV_ERR)    \
        tsp_printk(DEV_ERR, fmt, ## __VA_ARGS__);    \
    else{    \
        tsp_printk(DEV_ERR, fmt, ## __VA_ARGS__);    \
        sec_debug_tsp_log(fmt, ## __VA_ARGS__);    \
    }    \
})
#define tsp_warn(fmt, ...)    \
({    \
    if (IST40XX_LOG_LEVEL<DEV_WARN)        \
        tsp_printk(DEV_WARN, fmt, ## __VA_ARGS__);    \
    else{    \
        tsp_printk(DEV_WARN, fmt, ## __VA_ARGS__);    \
        sec_debug_tsp_log(fmt, ## __VA_ARGS__);    \
    }    \
})
#define tsp_info(fmt, ...)    \
({    \
    if (IST40XX_LOG_LEVEL<DEV_INFO)        \
        tsp_printk(DEV_INFO, fmt, ## __VA_ARGS__);    \
    else{    \
        tsp_printk(DEV_INFO, fmt, ## __VA_ARGS__);    \
        sec_debug_tsp_log(fmt, ## __VA_ARGS__);    \
    }    \
})
#define tsp_noti(fmt, ...)    \
({    \
    if (IST40XX_LOG_LEVEL<DEV_NOTI)        \
        tsp_printk(DEV_NOTI, fmt, ## __VA_ARGS__);    \
    else{    \
        tsp_printk(DEV_NOTI, fmt, ## __VA_ARGS__);    \
        sec_debug_tsp_log(fmt, ## __VA_ARGS__);    \
    }    \
})
#define tsp_debug(fmt, ...)    \
({    \
    if (IST40XX_LOG_LEVEL<DEV_DEBUG)        \
        tsp_printk(DEV_DEBUG, fmt, ## __VA_ARGS__);    \
    else{    \
        tsp_printk(DEV_DEBUG, fmt, ## __VA_ARGS__);    \
        sec_debug_tsp_log(fmt, ## __VA_ARGS__);    \
    }    \
})
#define tsp_verb(fmt, ...)    \
({    \
    if (IST40XX_LOG_LEVEL<DEV_VERB)        \
        tsp_printk(DEV_VERB, fmt, ## __VA_ARGS__);    \
    else{    \
        tsp_printk(DEV_VERB, fmt, ## __VA_ARGS__);    \
        sec_debug_tsp_log(fmt, ## __VA_ARGS__);    \
    }    \
})
#else
#define tsp_err(fmt, ...)	tsp_printk(DEV_ERR, fmt, ## __VA_ARGS__)
#define tsp_warn(fmt, ...)	tsp_printk(DEV_WARN, fmt, ## __VA_ARGS__)
#define tsp_info(fmt, ...)	tsp_printk(DEV_INFO, fmt, ## __VA_ARGS__)
#define tsp_noti(fmt, ...)	tsp_printk(DEV_NOTI, fmt, ## __VA_ARGS__)
#define tsp_debug(fmt, ...)	tsp_printk(DEV_DEBUG, fmt, ## __VA_ARGS__)
#define tsp_verb(fmt, ...)	tsp_printk(DEV_VERB, fmt, ## __VA_ARGS__)
#endif

/* timer & err cnt */
#define IST40XX_MAX_ERR_CNT		(100)
#define EVENT_TIMER_INTERVAL		(HZ * data->timer_period_ms / 1000)

/* i2c setting */
/* I2C Device info */
#define IST40XX_DEV_NAME		("IST40XX")
#define IST40XX_DEV_ID			(0xA0 >> 1)

/* I2C transfer msg number */
#define WRITE_CMD_MSG_LEN		(1)
#define READ_CMD_MSG_LEN		(2)

/* I2C address/Data length */
#define IST40XX_ADDR_LEN		(4)    /* bytes */
#define IST40XX_DATA_LEN		(4)    /* bytes */

/* I2C transaction size */
#define I2C_MAX_WRITE_SIZE		(1024)    /* bytes */
#define I2C_MAX_READ_SIZE		(1024)    /* bytes */

/* I2C access mode */
#define IST40XX_DIRECT_ACCESS		(1 << 31)
#define IST40XX_BURST_ACCESS		(1 << 27)
#define IST40XX_HIB_ACCESS		(0x800B << 16)
#define IST40XX_CMD_ACCESS		(0x800A << 16)
#define IST40XX_DA_ADDR(n)		(n | IST40XX_DIRECT_ACCESS)
#define IST40XX_BA_ADDR(n)		(n | IST40XX_BURST_ACCESS)
#define IST40XX_HA_ADDR(n)		(n | IST40XX_HIB_ACCESS)
#define IST40XX_CA_ADDR(n)		(n | IST40XX_CMD_ACCESS)

/* register */
/* Info register */
#define rSYS_CHIPID			IST40XX_DA_ADDR(0x40001000)
#define rSYS_LCLK_CON			IST40XX_DA_ADDR(0x40001054)

/* SW Reset */
#define rCMD_SW_RESET			IST40XX_CA_ADDR(0x40000000)

/* MEMX register */
#define rMEMX_CDC_MTL			IST40XX_DA_ADDR(0x30004000)
#define rMEMX_CPC_MTL			IST40XX_DA_ADDR(0x30004EDC)
#define rMEMX_CDC_SLF			IST40XX_DA_ADDR(0x30006068)
#define rMEMX_CPC_SLF			IST40XX_DA_ADDR(0x30006180)

/* HIB register */
#define IST40XX_HIB_BASE		(0x30000100)
#define IST40XX_HIB_TOUCH_STATUS	IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x00)
#define IST40XX_HIB_INTR_MSG		IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x04)
#define IST40XX_HIB_COORD		IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x08)
#define IST40XX_HIB_GESTURE_REG		IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x0C)
#define IST40XX_HIB_GESTURE_MSG		IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x1C)
#define IST40XX_HIB_SPONGE_RECT		IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x50)
#define IST40XX_HIB_PROX_SENSITI	IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x58)
#define IST40XX_HIB_CMD			IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x5C)

/* Touch status macro */
#define TOUCH_STATUS_MAGIC		(0x00000075)
#define TOUCH_STATUS_MASK		(0x000000FF)
#define FINGER_ENABLE_MASK		(1 << 20)
#define NOISE_MODE_MASK			(1 << 19)
#define WET_MODE_MASK			(1 << 18)
#define SCAN_CNT_MASK			(0xFFE00000)
#define GET_FINGER_ENABLE(n)		(n & FINGER_ENABLE_MASK)
#define GET_NOISE_MODE(n)		(n & NOISE_MODE_MASK)
#define GET_WET_MODE(n)			(n & WET_MODE_MASK)
#define GET_SCAN_CNT(n)			((n & SCAN_CNT_MASK) >> 21)
#define TOUCH_STATUS_NORMAL_MODE	(0 << 0)
#define TOUCH_STATUS_NOISE_MODE		(1 << 0)
#define TOUCH_STATUS_WET_MODE		(1 << 1)

#define IST40XX_SPONGE_REG_R_DATA	IST40XX_HA_ADDR(IST40XX_HIB_BASE | 0x58)
#define IST40XX_SPONGE_REG_BASE		IST40XX_DA_ADDR(0x00004300)
#define IST40XX_SPONGE_CTRL		(0x00)
#define IST40XX_SPONGE_RECT		(0x02)
#define IST40XX_SPONGE_UTC		(0x10)
#define IST40XX_SPONGE_FOD_PROPERTY	(0x14)
#define IST40XX_SPONGE_LP_DUMP		(0xF0)

/* sysfs max buf */
#define MAX_BUF_SIZE			8192

/* interrupt macro */
#define IST40XX_INTR_STATUS		(0x00000800)
#define CHECK_INTR_STATUS(n)		(((n & IST40XX_INTR_STATUS) \
						== IST40XX_INTR_STATUS) ? 1 : 0)
#define IST40XX_TOUCH_FRAME_CNT		(2)
#define PARSE_HOVER_NOTI(n)         ((n >> 2) & 0x1)
#define PARSE_HOVER_VAL(n)          (n & 0x3)
#define PARSE_TOUCH_CNT(n)		((n >> 12) & 0xF)
#define PARSE_PALM_STATUS(n)		((n >> 10) & 0x1)

/* mode */
typedef enum {
    STATE_POWER_OFF = 0,
    STATE_LPM,
    STATE_POWER_ON
} TOUCH_POWER_MODE;

#define IST40XX_MAX_CMD_SIZE		(0x20)
#define IST40XX_CMD_ADDR(n)		(n * 4)
#define IST40XX_CMD_VALUE(n)		(n / 4)
enum ist40xx_read_commands {
	eHCOM_GET_CHIP_ID		= IST40XX_CMD_ADDR(0x00),
	eHCOM_GET_VER_MAIN		= IST40XX_CMD_ADDR(0x01),
	eHCOM_GET_VER_FW		= IST40XX_CMD_ADDR(0x02),
	eHCOM_GET_ALGOINFO		= IST40XX_CMD_ADDR(0x03),
	eHCOM_GET_VER_TEST		= IST40XX_CMD_ADDR(0x04),
	eHCOM_GET_CRC32			= IST40XX_CMD_ADDR(0x05),
	eHCOM_GET_CAL_RESULT_P		= IST40XX_CMD_ADDR(0x06),
	eHCOM_GET_CAL_RESULT_M		= IST40XX_CMD_ADDR(0x07),
	eHCOM_GET_CAL_RESULT_S		= IST40XX_CMD_ADDR(0x08),
	eHCOM_GET_FW_INTEGRITY		= IST40XX_CMD_ADDR(0x09),

	eHCOM_GET_PROXIMITY_TH		= IST40XX_CMD_ADDR(0x0B),
	eHCOM_GET_MAX_DCM		= IST40XX_CMD_ADDR(0x0C),
	eHCOM_GET_REC_INFO_BASE		= IST40XX_CMD_ADDR(0x0D),

	eHCOM_GET_BASELINE_TH		= IST40XX_CMD_ADDR(0x0E),
	eHCOM_GET_LCD_INFO		= IST40XX_CMD_ADDR(0x10),
	eHCOM_GET_TSP_INFO		= IST40XX_CMD_ADDR(0x11),
	eHCOM_GET_SCR_INFO		= IST40XX_CMD_ADDR(0x12),

	eHCOM_GET_GAP_SPEC		= IST40XX_CMD_ADDR(0x13),

	eHCOM_GET_SWAP_INFO		= IST40XX_CMD_ADDR(0x17),

	eHCOM_GET_MISCAL_BASE		= IST40XX_CMD_ADDR(0x18),
	eHCOM_GET_SELFTEST_TX_BASE	= IST40XX_CMD_ADDR(0x19),
	eHCOM_GET_SELFTEST_BASE		= IST40XX_CMD_ADDR(0x1A),
	eHCOM_GET_PROX_CP_BASE		= IST40XX_CMD_ADDR(0x1B),
	eHCOM_GET_PROX_CDC_BASE		= IST40XX_CMD_ADDR(0x1C),
	eHCOM_GET_SEC_INFO_BASE		= IST40XX_CMD_ADDR(0x1D),
	eHCOM_GET_COPY_SEC_INFO_BASE	= IST40XX_CMD_ADDR(0x1E),

	eHCOM_GET_COM_CHECKSUM		= IST40XX_CMD_ADDR(0x1F),
};

enum ist40xx_write_commands {
	eHCOM_FW_START			= 0x01,
	eHCOM_FW_HOLD			= 0x02,

	eHCOM_CP_CORRECT_EN		= 0x10,
	eHCOM_WDT_EN			= 0x11,
	eHCOM_GESTURE_EN		= 0x12,
	eHCOM_SCALE_EN			= 0x13,
	eHCOM_NEW_POSITION_DIS		= 0x14,
	eHCOM_SLEEP_MODE_EN		= 0x15,

	eHCOM_SET_TIME_ACTIVE		= 0x20,
	eHCOM_SET_TIME_IDLE		= 0x21,
	eHCOM_SET_MODE_SPECIAL		= 0x22,
	eHCOM_SET_LOCAL_MODEL		= 0x23,
	eHCOM_SET_FOD_ENABLE		= 0x24,
	eHCOM_SET_FOD_DISABLE		= 0x25,
	eHCOM_SET_AOD_RECT		= 0x26,

	eHCOM_RUN_RAMCODE		= 0x30,
	eHCOM_RUN_CAL_AUTO		= 0x31,
	eHCOM_RUN_CAL_ONLY		= 0x32,
	eHCOM_RUN_CAL_MIS		= 0x33,
	eHCOM_RUN_SELFTEST		= 0x34,

	eHCOM_SET_SEC_INFO_WRITE	= 0x40,

	eHCOM_NOTIRY_G_REGMAP		= 0x50,

	eHCOM_SET_JIG_MODE		= 0x80,
	eHCOM_SET_JIG_SENSITI		= 0x81,

	eHCOM_SET_SP_REG_IDX		= 0xA8,
	eHCOM_SET_SP_REG_W_DATA		= 0xA9,

	eHCOM_SET_REJECTZONE_TOP	= 0xC0,
	eHCOM_SET_REJECTZONE_BOTTOM	= 0xC1,

	eHCOM_SET_REC_MODE		= 0xE0,

	eHCOM_DEFAULT			= 0xFF,
};

#define IST40XX_RINGBUF_NO_ERR		(0)
#define IST40XX_RINGBUF_NOT_ENOUGH	(1)
#define IST40XX_RINGBUF_EMPTY		(2)
#define IST40XX_RINGBUF_FULL		(3)
#define IST40XX_RINGBUF_TIMEOUT		(4)
#define IST40XX_MAX_RINGBUF_SIZE	(4 * 100 * 1024)
typedef struct _IST40XX_RINGBUF {
	u32 RingBufCtr;
	u32 RingBufInIdx;
	u32 RingBufOutIdx;
	u8 LogBuf[IST40XX_MAX_RINGBUF_SIZE];
} IST40XX_RING_BUF;

typedef union {
	struct {
		u32 y:12;
		u32 x:12;
		u32 status:4;
		u32 id:4;
		u32 area:4;
		u32 z:12;
		u32 ma:8;
		u32 mi:8;
	} b;
	u32 full[2];
} finger_info;

// TouchStatus
#define TOUCH_STA_NONE		0
#define TOUCH_STA_PRESS		1
#define TOUCH_STA_RELEASE	2
#define TOUCH_STA_CSV		3

struct ist40xx_status {
	int update;
	int update_result;

	int calib;
	u32 calib_msg[IST40XX_MAX_CALIB_SIZE];

	int miscalib;
	u32 miscalib_msg;
	u16 miscalib_result;

	u32 cmcs;
	u8 cmcs_result;

	bool event_mode;
	bool noise_mode;
	int sys_mode;
};

struct ist40xx_version {
	u32 main_ver;
	u32 fw_ver;
	u32 test_ver;
};

struct ist40xx_fw {
	struct ist40xx_version cur;
	struct ist40xx_version bin;
	u32 index;
	u32 size;
	u32 chksum;
	u32 buf_size;
	u8 *buf;
};

#define IST40XX_TAG_MAGIC	("ISTV2TAG")
struct ist40xx_tags {
	char magic1[8];
	u32 rom_base;
	u32 ram_base;
	u32 reserved0;
	u32 reserved1;

	u32 fw_addr;
	u32 fw_size;
	u32 cfg_addr;
	u32 cfg_size;
	u32 sensor_addr;
	u32 sensor_size;
	u32 cp_addr;
	u32 cp_size;
	u32 flag_addr;
	u32 flag_size;
	u32 reserved2;
	u32 reserved3;

	u32 reserved4;
	u32 reserved5;
	u32 cdc_base;
	u32 filter_base;
	u32 reserved6;
	u32 reserved7;

	u32 chksum;
	u32 chksum_all;
	u32 reserved8;
	u32 reserved9;

	u8 day;
	u8 month;
	u16 year;
	u8 hour;
	u8 min;
	u8 sec;
	u8 reserved10;
	char magic2[8];
};

struct CH_NUM {
	u8 tx;
	u8 rx;
};

struct TSP_NODE_BUF {
	u16 cdc[IST40XX_MAX_NODE_NUM];
	u16 self_cdc[IST40XX_MAX_SELF_NODE_NUM];
	u16 prox_cdc[IST40XX_MAX_SELF_NODE_NUM];
	u16 base[IST40XX_MAX_NODE_NUM];
	u16 self_base[IST40XX_MAX_SELF_NODE_NUM];
	u16 prox_base[IST40XX_MAX_SELF_NODE_NUM];
	s16 lofs[IST40XX_MAX_NODE_NUM];
	u16 memx_cp[IST40XX_MAX_NODE_NUM];
	u16 memx_self_cp[IST40XX_MAX_SELF_NODE_NUM];
	u16 memx_prox_cp[IST40XX_MAX_SELF_NODE_NUM];
	u16 rom_cp[IST40XX_MAX_NODE_NUM];
	u16 rom_self_cp[IST40XX_MAX_SELF_NODE_NUM];
	u16 rom_prox_cp[IST40XX_MAX_SELF_NODE_NUM];
	u16 miscal[IST40XX_MAX_NODE_NUM];
	u16 len;
	u16 self_len;
};

struct TSP_DIRECTION {
	bool swap_xy;
	bool flip_x;
	bool flip_y;
};

typedef struct _TSP_INFO {
	struct CH_NUM ch_num;
	struct CH_NUM screen;
	struct TSP_DIRECTION dir;
	struct TSP_NODE_BUF node;
	int height;
	int width;
	int finger_num;
	u16 threshold;
	u16 baseline;
	u16 prox_tx_threshold;
	u16 prox_rx_threshold;
} TSP_INFO;

typedef union {
	struct {
		u8 eid:2;
		u8 gtype:4;
		u8 sf:2;
		u8 gid;
		u8 gdata[4];
		u8 reserved0;
		u8 left_e:6;
		u8 reserved1:2;
	} b;
	u32 full[4];
} gesture_msg;

#ifdef SEC_FACTORY_MODE
#include "ist40xx_sec.h"
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
struct ist40xx_dt_data {
	int irq_gpio;
	int power_gpio;
	bool is_power_by_gpio;
	const char *regulator_avdd;
	int fw_bin;
	const char *ic_version;
	const char *project_name;
	char fw_path[FIRMWARE_PATH_LENGTH];
	char cmcs_path[FIRMWARE_PATH_LENGTH];
	int octa_hw;
	int bringup;
	int item_version;
	u32 area_indicator;
	u32 area_navigation;
	u32 area_edge;
	u32 cm_min_spec;
	u32 cm_max_spec;
	u32 cm_spec_gap;
	bool enable_settings_aot;
	bool support_fod;
};

struct ist40xx_data {
	struct mutex lock;
	struct mutex i2c_lock;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *input_dev_proximity;
	struct ist40xx_dt_data *dt_data;
	struct sec_tclm_data *tdata;
	TSP_INFO tsp_info;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
#ifdef CONFIG_PM
	struct completion resume_done;
#endif
	struct ist40xx_status status;
	struct ist40xx_fw fw;
	struct ist40xx_tags tags;
#ifdef IST40XX_PINCTRL
	struct pinctrl *pinctrl;
#endif
#ifdef SEC_FACTORY_MODE
	struct sec_cmd_data sec;
#endif
	u32 chip_id;
	u32 t_status;
	finger_info fingers[IST40XX_MAX_FINGER_ID];
	bool tsp_touched[IST40XX_MAX_FINGER_ID];
	u32 move_count[IST40XX_MAX_FINGER_ID];
	u8 hover;
	struct delayed_work work_read_info;
	bool info_work_done;
	u32 disassemble_count;
#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	struct delayed_work ghost_check;
	u8 tsp_dump_lock;
#endif
	u32 gap_spec;
	u32 selftest_addr;
	u32 selftest_tx_addr;
	u32 selftest_rx_addr;
	u32 cdc_addr;
	u32 self_cdc_addr;
	u32 prox_cdc_addr;
	u32 cp_addr;
	u32 self_cp_addr;
	u32 prox_cp_addr;
	u32 miscal_addr;
	u32 rec_addr;
	u32 rec_size;
	u32 algo_addr;
	u32 algo_size;
	u32 sec_info_addr;
	u32 copy_sec_info_addr;
	volatile bool irq_working;
	bool irq_enabled;
	bool initialized;
	bool ignore_delay;
	u32 noise_mode;
	u32 debug_mode;
	int report_rate;
	int idle_rate;
	u8 lpm_mode;
	u16 rect_data[4];
	u16 rejectzone_t;
	u16 rejectzone_b;
	u16 fod_property;
	int scan_count;
	int scan_retry;
	int max_scan_retry;
	int timer_period_ms;
	struct timer_list event_timer;
	struct timespec t_current;
	u32 event_ms;
	u32 timer_ms;
	int irq_err_cnt;
	int max_irq_err_cnt;
	u32 intr_debug1_addr;
	u32 intr_debug1_size;
	u32 intr_debug2_addr;
	u32 intr_debug2_size;
	u32 intr_debug3_addr;
	u32 intr_debug3_size;
	u32 rec_mode;
	int rec_delay;
	char *rec_file_name;
	u32 recording_scancnt;
	struct delayed_work work_reset_check;
#ifdef IST40XX_NOISE_MODE
	struct delayed_work work_noise_protect;
#else
	struct delayed_work work_force_release;
#endif
#ifdef CONFIG_MUIC_NOTIFIER
	struct notifier_block muic_nb;
#endif
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	struct notifier_block ccic_nb;
#endif
#ifdef CONFIG_VBUS_NOTIFIER
	struct notifier_block vbus_nb;
#endif
	struct ts_test_result test_result;

	int touch_pressed_num;
	u8 ito_test[4];
	u8 check_multi;
	unsigned int multi_count;
	unsigned int wet_count;
	unsigned int dive_count;
	unsigned int comm_err_count;
	unsigned int checksum_result;
	unsigned int all_finger_count;
	unsigned int all_spay_count;
	unsigned int all_aod_tsp_count;
	unsigned int all_singletap_count;
	long time_longest;
	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;
	char node_buf[MAX_BUF_SIZE];
	int node_cnt;
#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_interrupt;
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI_QC)
	struct completion st_irq_received;
#endif
#endif
	u16 p_x[IST40XX_MAX_FINGER_ID];
	u16 p_y[IST40XX_MAX_FINGER_ID];
	u16 r_x[IST40XX_MAX_FINGER_ID];
	u16 r_y[IST40XX_MAX_FINGER_ID];

	bool prox_power_off;
};

typedef enum {
	SPONGE_EVENT_TYPE_SPAY = 0x04,
	SPONGE_EVENT_TYPE_SINGLETAP = 0x08,
	SPONGE_EVENT_TYPE_AOD_PRESS = 0x09,
	SPONGE_EVENT_TYPE_AOD_LONGPRESS = 0x0A,
	SPONGE_EVENT_TYPE_AOD_DOUBLETAB = 0x0B,
	SPONGE_EVENT_TYPE_FOD		= 0x0F,
	SPONGE_EVENT_TYPE_FOD_RELEASE	= 0x10
} SPONGE_EVENT_TYPE;

extern int ist40xx_log_level;
extern struct class *ist40xx_class;
void tsp_printk(int level, const char *fmt, ...);
void ist40xx_delay(unsigned int ms);
int ist40xx_intr_wait(struct ist40xx_data *data, long ms);

void ist40xx_enable_irq(struct ist40xx_data *data);
void ist40xx_disable_irq(struct ist40xx_data *data);
void ist40xx_set_ta_mode(bool charging);
void ist40xx_set_edge_mode(int mode);
void ist40xx_set_call_mode(int mode);
void ist40xx_set_halfaod_mode(int mode);
void ist40xx_set_cover_mode(int mode);
void ist40xx_set_sensitivity_mode(int mode);
void ist40xx_set_glove_mode(int mode);
void ist40xx_set_rejectzone_mode(int mode, u16 top, u16 bottom);
void ist40xx_set_sensitivity_mode(int mode);
void ist40xx_set_touchable_mode(int mode);
void ist40xx_start(struct ist40xx_data *data);

int ist40xx_read_reg(struct i2c_client *client, u32 reg, u32 *buf);
int ist40xx_read_cmd(struct ist40xx_data *data, u32 cmd, u32 *buf);
int ist40xx_write_cmd(struct ist40xx_data *data, u32 cmd, u32 val);
int ist40xx_read_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len);
int ist40xx_write_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len);
int ist40xx_burst_read(struct i2c_client *client, u32 addr, u32 *buf32,
		       u16 len, bool bit_en);
int ist40xx_burst_write(struct i2c_client *client, u32 addr, u32 *buf32,
			u16 len);

int ist40xx_write_sponge_reg(struct ist40xx_data *data, u16 idx, u16 *buf16,
			     int len, bool hold);
int ist40xx_read_sponge_reg(struct ist40xx_data *data, u16 idx, u16 *buf16,
			    int len, bool hold);

int ist40xx_cmd_gesture(struct ist40xx_data *data, u16 value);
int ist40xx_cmd_start_scan(struct ist40xx_data *data);
int ist40xx_cmd_calibrate(struct ist40xx_data *data);
int ist40xx_cmd_miscalibrate(struct ist40xx_data *data);
int ist40xx_cmd_check_calib(struct ist40xx_data *data);
int ist40xx_cmd_update(struct ist40xx_data *data, int cmd);
int ist40xx_cmd_hold(struct ist40xx_data *data, int enable);

int ist40xx_power_on(struct ist40xx_data *data, bool download);
int ist40xx_power_off(struct ist40xx_data *data);
int ist40xx_reset(struct ist40xx_data *data, bool download);

int ist40xx_init_system(struct ist40xx_data *data);
void ist40xx_display_booting_dump_log(struct ist40xx_data *data);
void ist40xx_display_key_dump_log(struct ist40xx_data *data);

#ifdef SEC_FACTORY_MODE
extern struct class *sec_class;
extern int sec_touch_sysfs(struct ist40xx_data *data);
extern int sec_fac_cmd_init(struct ist40xx_data *data);
extern void sec_fac_cmd_remove(struct ist40xx_data *data);
extern void sec_touch_sysfs_remove(struct ist40xx_data *data);
#endif

#ifdef TCLM_CONCEPT
int ist40xx_tclm_data_read(struct i2c_client *client, int address);
int ist40xx_tclm_data_write(struct i2c_client *client, int address);
int ist40xx_execute_force_calibration(struct i2c_client *client, int cal_mode);
int ist40xx_write_sec_info(struct ist40xx_data *data, u8 idx, u32 *buf32,
			   int len);
int ist40xx_read_sec_info(struct ist40xx_data *data, u8 idx, u32 *buf32,
			  int len);
#endif
#ifdef IST40XX_USE_CMCS
void run_cmcs_full_test(void *dev_data);
#endif
#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif
#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
irqreturn_t ist40xx_irq_thread(int irq, void *ptr);
irqreturn_t ist40xx_filter_interrupt(struct ist40xx_data *data);
void ist40xx_secure_touch_stop(struct ist40xx_data *data, int blocking);
#endif
#endif  /* __IST40XX_H__ */
