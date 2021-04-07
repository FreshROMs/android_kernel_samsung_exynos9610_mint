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

#ifndef __IST40XX_CMCS_H__
#define __IST40XX_CMCS_H__

#define CM_MIN_SPEC		500
#define CM_MAX_SPEC		2500
#define SPEC_GAP		50

#define CMCS_FLAG_CM		(1 << 0)
#define CMCS_FLAG_CS		(1 << 1)
#define CMCS_FLAG_CMJIT		(1 << 2)
#define CMCS_FLAG_SLOPE		(1 << 3)
#define CMCS_FLAG_ALL		(CMCS_FLAG_CM | CMCS_FLAG_SLOPE | \
					CMCS_FLAG_CS | CMCS_FLAG_CMJIT)

#define CMCS_CMJIT		"CMJIT"
#define CMCS_CM			"CM"
#define CMCS_CS			"CS"

#define CMCS_MSG(n)		(n & 0xFFFFFF00)
#define CMCS_RESULT(n)		(n & 0x000000FF)

#define CM_MSG_VALID		(0x5E7FC300)
#define CS_MSG_VALID		(0x5E7FC500)
#define CMJIT_MSG_VALID		(0x5E7F7100)
#define SLOPE_MSG_VALID		(0x5E7F5700)

#define IST40XX_CMCS_NAME	"ist40xx.cms"
#define IST40XX_CMCS_MAGIC	"CMCS5TAG"

struct CMCS_SPEC_NODE {
	u32 node_cnt;
	u16 *buf_min;
	u16 *buf_max;
};

struct CMCS_SPEC_TOTAL {
	s16 screen_min;
	s16 screen_max;
	s16 key_min;
	s16 key_max;
};

struct CMCS_ITEM_INFO {
	char name[8];
	u32 addr;
	u32 size;
	char data_type[2];
	char spec_type[2];
};

typedef struct _CMCS_ITEM {
	u32 cnt;
	struct CMCS_ITEM_INFO *item;
} CMCS_ITEM;

struct CMCS_CMD_INFO {
	u32 addr;
	u32 value;
};

typedef struct _CMCS_CMD {
	u32 cnt;
	struct CMCS_CMD_INFO *cmd;
} CMCS_CMD;

union CMCS_SPEC_ITEM {
	struct CMCS_SPEC_NODE spec_node;
	struct CMCS_SPEC_TOTAL spec_total;
};

struct CMCS_SPEC_SLOPE {
	char name[8];
	s16 x_min;
	s16 x_max;
	s16 y_min;
	s16 y_max;
	s16 key_min;
	s16 key_max;
};

struct CMCS_REG_INFO {
	char name[8];
	u32 addr;
	u32 size;
};

typedef struct _CMCS_PARAM {
	u32 cmcs_size_addr;
	u32 cmcs_size;
	u32 enable_addr;
	u32 checksum_addr;
	u32 end_notify_addr;
	u32 sensor1_addr;
	u32 sensor2_addr;
	u32 sensor3_addr;
	u32 cm_sensor1_size;
	u32 cm_sensor2_size;
	u32 cm_sensor3_size;
	u32 cs_sensor1_size;
	u32 cs_sensor2_size;
	u32 cs_sensor3_size;
	u32 jit_sensor1_size;
	u32 jit_sensor2_size;
	u32 jit_sensor3_size;
	u32 cmcs_chksum;
	u32 cm_sensor_chksum;
	u32 cs_sensor_chksum;
	u32 jit_sensor_chksum;
	u32 cs_tx_result_addr;
	u32 cs_rx_result_addr;
} CMCS_PARAM;

typedef struct _CMCS_BIN_INFO {
	char magic1[8];
	CMCS_ITEM items;
	CMCS_CMD cmds;
	struct CMCS_SPEC_SLOPE spec_slope;
	struct CMCS_SPEC_TOTAL spec_cr;
	struct CMCS_SPEC_TOTAL spec_cm_hfreq;
	struct CMCS_SPEC_TOTAL spec_cm_lfreq;
	CMCS_PARAM param;
	union CMCS_SPEC_ITEM *spec_item;
	u8 *buf_cmcs;
	u32 *buf_cm_sensor;
	u32 *buf_cs_sensor;
	u32 *buf_jit_sensor;
	u32 version;
	char magic2[8];
} CMCS_BIN_INFO;

typedef struct _CMCS_BUF {
	s16 cm[IST40XX_MAX_NODE_NUM];
	s16 cs[IST40XX_MAX_NODE_NUM];
	s16 cm_jit[IST40XX_MAX_NODE_NUM];
	s16 slope[IST40XX_MAX_NODE_NUM];
	s16 slope0[IST40XX_MAX_NODE_NUM];
	s16 slope1[IST40XX_MAX_NODE_NUM];
	u32 cm_tx_result[2];
	u32 cm_rx_result[2];
	u32 cs_tx_result[2];
	u32 cs_rx_result[2];
	u32 slope_tx_result[2];
	u32 slope_rx_result[2];
	u32 cm_slope_result[2];
	int cm_result;
	int cs_result;
	int cm_jit_result;
	int slope_result;
	int cm_min;
	int cm_max;
	int cs_min;
	int cs_max;
	int cm_jit_min;
	int cm_jit_max;
	int slope_max;
} CMCS_BUF;

int check_tsp_type(struct ist40xx_data *data, int tx, int rx);
int ist40xx_get_cmcs_info(const u8 *buf, const u32 size);
int ist40xx_cmcs_test(struct ist40xx_data *data, u32 flag);
int ist40xx_init_cmcs_sysfs(struct ist40xx_data *data);

#endif // __IST40XX_CMCS_H__
