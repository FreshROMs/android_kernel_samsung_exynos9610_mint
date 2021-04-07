/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_IMX616_H
#define FIMC_IS_CIS_IMX616_H

#include "fimc-is-cis.h"
#define DEBUG	1 /* Don't forget to turn me off later */

/* #define TEMP_3HDR */

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX616_MAX_WIDTH          (6528 + 0)
#define SENSOR_IMX616_MAX_HEIGHT         (4896 + 0)

#define USE_GROUP_PARAM_HOLD                      (1)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (4)

/* INTEGRATION TIME Value:
 * INE_INTEG_TIME is a fixed value (0x0200: 16bit - read value is 0x134c)
 */
#define SENSOR_IMX616_FINE_INTEG_TIME_VALUE			(4940)
#define SENSOR_IMX616_COARSE_INTEG_TIME_MIN_VALUE		(16)
#define SENSOR_IMX616_COARSE_INTEG_TIME_MAX_MARGIN_VALUE	(48)

/* For short name
 * : difficult to comply with kernel coding rule because of too long name
 */
#define REG(name)	SENSOR_IMX616_##name##_ADDR
#define VAL(name)	SENSOR_IMX616_##name##_VALUE

/****
 **  Register Address
 **  : address name format: SENSOR_IMX616_XX...XX_ADDR
 ****/
#define SENSOR_IMX616_OTP_PAGE_SETUP_ADDR		(0x0A02)
#define SENSOR_IMX616_OTP_READ_TRANSFER_MODE_ADDR	(0x0A00)
#define SENSOR_IMX616_OTP_STATUS_REGISTER_ADDR		(0x0A01)
#define SENSOR_IMX616_OTP_CHIP_REVISION_ADDR		(0x0018)

#define SENSOR_IMX616_FRAME_LENGTH_LINE_ADDR		(0x0340)
#define SENSOR_IMX616_LINE_LENGTH_PCK_ADDR		(0x0342)
#define SENSOR_IMX616_FINE_INTEG_TIME_ADDR		(0x0200)
#define SENSOR_IMX616_COARSE_INTEG_TIME_ADDR		(0x0202)
#define SENSOR_IMX616_AGAIN_ADDR			(0x0204)
#define SENSOR_IMX616_DGAIN_ADDR			(0x020E)

#define SENSOR_IMX616_ABS_GAIN_GR_SET_ADDR		(0x0B8E)
#define SENSOR_IMX616_ABS_GAIN_R_SET_ADDR		(0x0B90)
#define SENSOR_IMX616_ABS_GAIN_B_SET_ADDR		(0x0B92)
#define SENSOR_IMX616_ABS_GAIN_GB_SET_ADDR		(0x0B94)

/**
 * Extra register for 3hdr
 **/
#define SENSOR_IMX616_MID_COARSE_INTEG_TIME_ADDR	(0x3FE0)
#define SENSOR_IMX616_ST_COARSE_INTEG_TIME_ADDR		(0x0224)
#define SENSOR_IMX616_MID_AGAIN_ADDR			(0x3FE2)
#define SENSOR_IMX616_ST_AGAIN_ADDR			(0x0216)
#define SENSOR_IMX616_MID_DGAIN_ADDR			(0x3FE4)
#define SENSOR_IMX616_ST_DGAIN_ADDR			(0x0218)

#define SENSOR_IMX616_SHD_CORR_EN_ADDR			(0x0B00)
#define SENSOR_IMX616_LSC_APP_RATE_ADDR			(0x380C)
#define SENSOR_IMX616_LSC_APP_RATE_Y_ADDR		(0x380D)
#define SENSOR_IMX616_CALC_MODE_ADDR			(0x3804)
#define SENSOR_IMX616_TABLE_SEL_ADDR			(0x3805)
#define SENSOR_IMX616_KNOT_TAB_GR_ADDR			(0x9C04)
#define SENSOR_IMX616_KNOT_TAB_GB_ADDR			(0x9D08)

#define SENSOR_IMX616_AEHIST1_ADDR			(0x37E0)
#define SENSOR_IMX616_AEHIST2_ADDR			(0x3DA0)
#define SENSOR_IMX616_FLK_AREA_ADDR			(0x37F0)

/* Blend for TC */
#define SENSOR_IMX616_BLD1_GMTC2_EN_ADDR		(0x3DB6)
#define SENSOR_IMX616_BLD1_GMTC2_RATIO_ADDR		(0x3DB0)
#define SENSOR_IMX616_BLD1_GMTC_NR_TRANSIT_FRM_ADDR	(0xF602)
#define SENSOR_IMX616_BLD2_TC_RATIO_1_ADDR		(0xF4B6)
#define SENSOR_IMX616_BLD2_TC_RATIO_2_ADDR		(0xF4B9)
#define SENSOR_IMX616_BLD2_TC_RATIO_3_ADDR		(0xF4BC)
#define SENSOR_IMX616_BLD2_TC_RATIO_4_ADDR		(0xF4BF)
#define SENSOR_IMX616_BLD2_TC_RATIO_5_ADDR		(0xF4C2)
#define SENSOR_IMX616_BLD3_LTC_RATIO_1_ADDR		(0x3C0C)
#define SENSOR_IMX616_BLD3_LTC_RATIO_6_ADDR		(0x3DAE) /* RATIO 6 */
#define SENSOR_IMX616_BLD3_LTC_RATIO_START_ADDR		SENSOR_IMX616_BLD3_LTC_RATIO_1_ADDR /* RATIO 1 ~ 5 */
#define SENSOR_IMX616_BLD4_HDR_TC_RATIO1_UP_ADDR	(0x3C15)
#define SENSOR_IMX616_BLD4_HDR_TC_RATIO1_LO_ADDR	(0x3C16)
#define SENSOR_IMX616_BLD4_HDR_TC_RATIO_START_ADDR	SENSOR_IMX616_BLD4_HDR_TC_RATIO1_UP_ADDR

#define SENSOR_IMX616_EVC_PGAIN_ADDR			(0x3DB2)
#define SENSOR_IMX616_EVC_NGAIN_ADDR			(0x3DB3)

/* AEHIST Thresh */
#define SENSOR_IMX616_AEHIST_LN_AUTO_THRETH_ADDR	(0x3C00)
#define SENSOR_IMX616_AEHIST_LN_LOWER_TH_MSB_ADDR	(0x3C01)
#define SENSOR_IMX616_AEHIST_LN_THRETH_START_ADDR	SENSOR_IMX616_AEHIST_LN_LOWER_TH_MSB_ADDR
#define SENSOR_IMX616_AEHIST_LOG_AUTO_THRETH_ADDR	(0x3C05)
#define SENSOR_IMX616_AEHIST_LOG_LOWER_TH_MSB_ADDR	(0x3C06)
#define SENSOR_IMX616_AEHIST_LOG_THRETH_START_ADDR	SENSOR_IMX616_AEHIST_LOG_LOWER_TH_MSB_ADDR


/****
 **  Constant Value
 **  : value name format: SENSOR_IMX616_XX...XX_VALUE
 ***/
/* Register Value */
#define SENSOR_IMX616_MIN_AGAIN_VALUE			(112)
#define SENSOR_IMX616_MAX_AGAIN_VALUE			(1008)
#define SENSOR_IMX616_MIN_DGAIN_VALUE			(0x0100)
#define SENSOR_IMX616_MAX_DGAIN_VALUE			(0x0FFF)

/**
 * Extra Value for 3hdr
 **/
#define SENSOR_IMX616_QBCHDR_MIN_AGAIN_VALUE		(0)
#define SENSOR_IMX616_QBCHDR_MAX_AGAIN_VALUE		(448)
#define SENSOR_IMX616_QBCHDR_MIN_DGAIN_VALUE		(0x0100)
#define SENSOR_IMX616_QBCHDR_MAX_DGAIN_VALUE		(0x0FFF)
#define SENSOR_IMX616_LSC_MAX_GAIN_VALUE		(0x03FF)
#define SENSOR_IMX616_LSC_APP_RATE_VALUE		(0x00)
#define SENSOR_IMX616_LSC_APP_RATE_Y_VALUE		(0x80)
#define SENSOR_IMX616_CALC_MODE_MANUAL_VALUE		(0x02)
#define SENSOR_IMX616_TABLE_SEL_1_VALUE			(0x00)

/* AEHIST Threth: ((lower_16bit << 16) | upper_16bit) */
#define SENSOR_IMX616_AEHIST_LINEAR_THRETH_VALUE	(0x000003FF)
#define SENSOR_IMX616_AEHIST_LOG_THRETH_VALUE		(0x0000FFFF)
#define SENSOR_IMX616_AEHIST_LINEAR_LO_THRETH_VALUE	(0x0000)
#define SENSOR_IMX616_AEHIST_LINEAR_UP_THRETH_VALUE	(0x03FF)
#define SENSOR_IMX616_AEHIST_LOG_LO_THRETH_VALUE	(0x0000)
#define SENSOR_IMX616_AEHIST_LOG_UP_THRETH_VALUE	(0xFFFF)


/****
 **  Others
 ***/
/* Related EEPROM CAL */
#define SENSOR_IMX616_QUAD_SENS_CAL_BASE_FRONT		(0x1700)
#define SENSOR_IMX616_QUAD_SENS_CAL_SIZE		(1560)
#define SENSOR_IMX616_QUAD_SENS_REG_ADDR		(0xC500)

#define SENSOR_IMX616_QSC_DUMP_NAME               "/data/vendor/camera/IMX616_QSC_DUMP.bin"

/* Related Function Option */
#define SENSOR_IMX616_SENSOR_CAL_FOR_REMOSAIC		(1)
#define SENSOR_IMX616_CAL_DEBUG				(0)
#define SENSOR_IMX616_DEBUG_INFO			(0)

/* 3HDR */
#define NR_FLKER_BLK_W		(4)
#define NR_FLKER_BLK_H		(96)
#define RAMTABLE_H		13
#define RAMTABLE_V		10
#define RAMTABLE_LEN		(RAMTABLE_H * RAMTABLE_V)

#define NR_ROI_AREAS		(2)

 /*
  * [Mode Information]
  *
  * Reference File : IMX616-AAJH5_SAM-DPHY_26MHz_RegisterSetting_ver6.00-8.20_b2_190610.xlsx
  *
  * -. Global Setting -
  *
  * -. 2x2 BIN For Single Still Preview / Capture -
  *    [ 0 ] REG_H : 2x2 Binning 3264x2448 30fps	: Single Still Preview (4:3)	,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *    [ 1 ] REG_I : 2x2 Binning 3264x1836 30fps	: Single Still Preview (16:9)	 ,	MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *    [ 2 ] REG_J : 2x2 Binning 3264x1504 30fps	: Single Still Preview (19.5:9)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *    [ 3 ] REG_N : 2x2 Binning 2448x2448 30fps	: Single Still Preview (1:1)	,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *
  * -. 68¨¬2x2 BIN For Single Still Preview / Capture -
  *    [ 4 ] REG_K : 2x2 Binning 2640x1980 30fps	: Single Still Preview (4:3)	,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *    [ 5 ] REG_L : 2x2 Binning 2640x1448 30fps	: Single Still Preview (16:9)	,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *    [ 6 ] REG_M : 2x2 Binning 2640x1216 30fps	: Single Still Preview (19.5:9)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *    [ 7 ] REG_O_2 : 2x2 Binning 1968x1968 30fps	: Single Still Preview (1:1)	,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *
  * -. 2x2 BIN H2V2 For FastAE
  *    [ 8 ] REG_R : 2x2 Binning 1632x1224 120fps	  : FAST AE (4:3)	 ,	MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
  *
  * -. FULL Remosaic For Single Still Remosaic Capture -
  *    [ 9 ] REG_F	: Full Remosaic 6528x4896 24fps 	  : Single Still Remosaic Capture (4:3) , MIPI lane: 4, MIPI data rate(Mbps/lane): 2218
  *
  * -. 68¨¬FULL Remosaic For Single Still Remosaic Capture -
  *    [ 10 ] REG_G	: Full Remosaic 5264x3948 24fps 	  : Single Still Remosaic Capture (4:3) , MIPI lane: 4, MIPI data rate(Mbps/lane): 2218
  *
  * -. QBC HDR
  *    [ 11 ] REG_S : QBC HDR 3264x2448 24fps		: Single Still Preview (4:3)	,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2218
  */


enum sensor_imx616_mode_enum {
	IMX616_MODE_2X2BIN_3264x2448_30FPS = 0,		/* 0 */
	IMX616_MODE_2X2BIN_3264x1836_30FPS,
	IMX616_MODE_2X2BIN_3264x1504_30FPS,
	IMX616_MODE_2X2BIN_2448x2448_30FPS,
	IMX616_MODE_2X2BIN_2640x1980_30FPS,
	IMX616_MODE_2X2BIN_2640x1488_30FPS,
	IMX616_MODE_2X2BIN_2640x1216_30FPS,
	IMX616_MODE_2X2BIN_1968x1968_30FPS,
	IMX616_MODE_H2V2_1632x1224_120FPS,		/* 8 */
	IMX616_MODE_2X2BIN_3264x1836_120FPS,
	/* FULL Remosaic */
	IMX616_MODE_REMOSAIC_START,
	IMX616_MODE_REMOSAIC_6528x4896_24FPS = IMX616_MODE_REMOSAIC_START,	/* 10 */
	IMX616_MODE_REMOSAIC_5264x3948_24FPS ,
	IMX616_MODE_REMOSAIC_END = IMX616_MODE_REMOSAIC_5264x3948_24FPS,
	/* QBC 3HDR */
	IMX616_MODE_QBCHDR_START ,
	IMX616_MODE_QBCHDR_3264x2448_24FPS = IMX616_MODE_QBCHDR_START,	/* 12 */
	IMX616_MODE_QBCHDR_3264x1836_30FPS,
	IMX616_MODE_QBCHDR_END = IMX616_MODE_QBCHDR_3264x1836_30FPS,
	IMX616_MODE_END
};

/* IS_REMOSAIC(u32 mode): check if mode is remosaic */
#define IS_REMOSAIC(mode) ({						\
	typecheck(u32, mode) && (m >= IMX616_MODE_REMOSAIC_START) &&	\
	(m <= IMX616_MODE_REMOSAIC_END);				\
})

/* IS_REMOSAIC_MODE(struct fimc_is_cis *cis) */
#define IS_REMOSAIC_MODE(cis) ({					\
	u32 m;								\
	typecheck(struct fimc_is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m >= IMX616_MODE_REMOSAIC_START) && (m <= IMX616_MODE_REMOSAIC_END); \
})

/* IS_3DHDR(struct fimc_is_cis *cis, u32 mode): check if mode is 3dhdr */
#define IS_3DHDR(cis, mode) ({						\
	typecheck(u32, mode);						\
	typecheck(struct fimc_is_cis *, cis);				\
	(cis->use_3hdr) && (mode >= IMX616_MODE_QBCHDR_START) &&	\
	(mode <= IMX616_MODE_QBCHDR_END);				\
})

/* IS_3DHDR_MODE(struct fimc_is_cis *cis) */
#define IS_3DHDR_MODE(cis) ({						\
	u32 m;								\
	typecheck(struct fimc_is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(cis->use_3hdr) && (m >= IMX616_MODE_QBCHDR_START) &&		\
	(m <= IMX616_MODE_QBCHDR_END);					\
})

#if defined(DEBUG) && (DEBUG >= 2)
#define ASSERT(x, fmt, args...)						\
	do {								\
		if (!(x)) {						\
			err("[ASSERT FAILURE] "fmt, ##args);		\
			panic("fimc-is2:imx616:%d\n" "[ASSERT] " fmt,	\
				__LINE__, ##args);			\
		}							\
	} while (0) 
#elif defined(DEBUG) && (DEBUG >= 1)
#define ASSERT(x, fmt, args...)						\
	do {								\
		if (!(x)) {						\
			err("[ASSERT FAILURE] "fmt, ##args);		\
		}							\
	} while (0) 
#else
#define ASSERT(x, format...) do { } while (0)
#endif

#define CHECK_GOTO(conditon, label)		\
	do {					\
		if (unlikely(conditon))		\
			goto label;		\
	} while (0)

#define CHECK_RET(conditon, ret)		\
	do {					\
		if (unlikely(conditon))		\
			return ret;		\
	} while (0)

#define CHECK_ERR_GOTO(conditon, label, fmt, args...)	\
	do {						\
		if (unlikely(conditon)) {		\
			err(fmt, ##args);		\
			goto label;			\
		}					\
	} while (0)

#define CHECK_ERR_RET(conditon, ret,  fmt, args...)	\
	do {						\
		if (unlikely(conditon)) {		\
			err(fmt, ##args);		\
			return ret;			\
		}					\
	} while (0)


#endif

