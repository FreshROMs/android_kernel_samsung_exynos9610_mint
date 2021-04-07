/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_2X5_H
#define FIMC_IS_CIS_2X5_H

#include "fimc-is-cis.h"

#define SUPPORT_2X5_SENSOR_VARIATION
#define SENSOR_2X5_CAL_UPLOAD

#define SENSOR_2X5_BURST_WRITE			(1)
#define SENSOR_2X5_CAL_BURST_WRITE		(1)

#define SENSOR_2X5_CAL_XTALK_SIZE		(1040)
#define SENSOR_2X5_CAL_LSC_SIZE			(464)
#define SENSOR_2X5_XTALK_ADDR_PAGE		(0x2001)
#define SENSOR_2X5_XTALK_ADDR_OFFSET		(0x2188)
#define SENSOR_2X5_LSC_ADDR_PAGE		(0x2001)
#define SENSOR_2X5_LSC_ADDR_OFFSET		(0x14D8)
#define SENSOR_2X5_EVT0_XTALK_ADDR_PAGE		(0x2004)
#define SENSOR_2X5_EVT0_XTALK_ADDR_OFFSET	(0x56E8)
#define SENSOR_2X5_EVT0_LSC_ADDR_PAGE		(0x2004)
#define SENSOR_2X5_EVT0_LSC_ADDR_OFFSET		(0x4A38)

#if SENSOR_2X5_CAL_BURST_WRITE
#define SENSOR_2X5_BURST_CAL_NR_RAW		((SENSOR_2X5_CAL_XTALK_SIZE + SENSOR_2X5_CAL_LSC_SIZE) / 2 + 9)
#define BURST_BUF_SIZE				(SENSOR_2X5_BURST_CAL_NR_RAW * 3)
#endif

#define SENSOR_2X5_ABS_GAIN_PAGE		(0x4000)
#define SENSOR_2X5_ABS_GAIN_OFFSET		(0x0D82)
#define SENSOR_2X5_EVT0_ABS_GAIN_PAGE		(0x4000)
#define SENSOR_2X5_EVT0_ABS_GAIN_OFFSET		(0x0D12)

#define SENSOR_2X5_W_DIR_PAGE		0xFCFC
#define SENSOR_2X5_W_INDIR_PAGE		0x6028
#define SENSOR_2X5_W_INDIR_OFFSET	0x602A
#define SENSOR_2X5_W_INDIR_DATA		0x6F12

#define EXT_CLK_Mhz (26)

#define SENSOR_2X5_MAX_WIDTH		(5760 + 0)
#define SENSOR_2X5_MAX_HEIGHT		(4312 + 0)

/* TODO: Check below values are valid */
#define SENSOR_2X5_FINE_INTEGRATION_TIME_MIN                0x0100
#define SENSOR_2X5_FINE_INTEGRATION_TIME_MAX                0x0100
#define SENSOR_2X5_COARSE_INTEGRATION_TIME_MIN              0x04
#define SENSOR_2X5_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x04

#define USE_GROUP_PARAM_HOLD	(0)

typedef enum
{
	MODE_2X5_2880x2156_30 = 0,	/* 0, 4:3 */
	MODE_2X5_2880x1620_30, 		/* 1, 16:9 */
	MODE_2X5_2880x1332_30,		/* 2, 19.5 : 9 */
	MODE_2X5_2156x2156_30,		/* 3, 1:1*/
	MODE_2X5_2352x1764_30,		/* 4, crop */
	MODE_2X5_2352x1324_30,		/* 5, crop */
	MODE_2X5_2352x1088_30,		/* 6, crop */
	MODE_2X5_1760x1760_30,		/* 7, crop */
	MODE_2X5_2880x2156_120,		/* 8 */
	MODE_2X5_REMOSAIC_START,	
	MODE_2X5_5760x4312_30 = MODE_2X5_REMOSAIC_START, /* 9, Remosaic */
	MODE_2X5_5760x3240_30,		/* 10, 16:9 */
	MODE_2X5_5760x2664_30,		/* 11, 19.5 : 9 */
	MODE_2X5_4312x4312_30,		/* 12, 1:1 */
	MODE_2X5_4688x3516_30,		/* 13, crop */
	MODE_2X5_REMOSAIC_END = MODE_2X5_4688x3516_30,
	MODE_2X5_3DHDR_START,	
	MODE_2X5_2880x2156_30_3DHDR = MODE_2X5_3DHDR_START, /* 14 */
	MODE_2X5_2880x1620_30_3DHDR,	/* 15 */
	MODE_2X5_2880x1332_30_3DHDR,	/* 16 */
	MODE_2X5_2156x2156_30_3DHDR,	/* 17 */
	MODE_2X5_3DHDR_END = MODE_2X5_2156x2156_30_3DHDR,
	MODE_2X5_END
} MODE_2X5_ENUM;

enum {
	TETRA_ISP_6MP = 0,
	TETRA_ISP_24MP,
};

enum {
	DATAMODE_1BYTE = 0,
	DATAMODE_2BYTE,
	DATAMODE_END
};
		
enum {
	SENSOR_2X5_VER_SP03		= 0xA0, /* for test */
	SENSOR_2X5_VER_SP13		= 0xA1,
	SENSOR_2X5_VER_SP13_A1		= SENSOR_2X5_VER_SP13,
	SENSOR_2X5_VER_SP13_A2		= 0xA2,
	SENSOR_2X5_VER_SP13_END		= SENSOR_2X5_VER_SP13_A2
};

/* IS_3DHDR_MODE(struct fimc_is_cis *cis, u32 *mode) */
#define IS_3DHDR_MODE(cis, mode) ({					\
	u32 m, *_mode = (u32 *)(mode);					\
	typecheck(struct fimc_is_cis *, cis);				\
									\
	m = _mode ? *_mode : cis->cis_data->sens_config_index_cur;	\
	(cis->use_3hdr) && (m >= MODE_2X5_3DHDR_START) && (m <= MODE_2X5_3DHDR_END); \
})

#define CHECK_ERR_GOTO(conditon, label, fmt, args...)		\
	do {							\
		if (unlikely(conditon)) {			\
			err(fmt, ##args);			\
			goto label;				\
		}						\
	} while (0)

#define CHECK_ERR_RET(conditon, ret,  fmt, args...)		\
	do {							\
		if (unlikely(conditon)) {			\
			err(fmt, ##args);			\
			return ret;				\
		}						\
	} while (0)

#endif

