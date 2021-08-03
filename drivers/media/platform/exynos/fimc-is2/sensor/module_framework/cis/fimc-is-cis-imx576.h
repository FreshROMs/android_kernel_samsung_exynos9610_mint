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

#ifndef FIMC_IS_CIS_IMX576_H
#define FIMC_IS_CIS_IMX576_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX576_MAX_WIDTH          (5760 + 0)
#define SENSOR_IMX576_MAX_HEIGHT         (4312 + 0)

#define SENSOR_IMX576_FINE_INTEGRATION_TIME_MIN                (0x510)
#define SENSOR_IMX576_FINE_INTEGRATION_TIME_MAX                (0x510)
#define SENSOR_IMX576_COARSE_INTEGRATION_TIME_MIN              (0x04)
#define SENSOR_IMX576_COARSE_INTEGRATION_TIME_MAX_MARGIN       (0x3D)
#define SENSOR_IMX576_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL    (65535)
#define SENSOR_IMX576_MAX_CIT_LSHIFT_VALUE        (0x7)
#define SENSOR_IMX576_CIT_LSHIFT_ADDR             (0x3100)
#define SENSOR_IMX576_FRAME_LENGTH_LINE_ADDR      (0x0340)

#define USE_GROUP_PARAM_HOLD                      (1)

#define SENSOR_IMX576_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_IMX576_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_IMX576_DIG_GAIN_ADDR               (0x020E)
#define SENSOR_IMX576_SOHT_DIG_GAIN_ADDR          (0x0218)

#define SENSOR_IMX576_MIN_ANALOG_GAIN_SET_VALUE   (0)
#define SENSOR_IMX576_MAX_ANALOG_GAIN_SET_VALUE   (960)
#define SENSOR_IMX576_MIN_DIGITAL_GAIN_SET_VALUE  (0x0100)
#define SENSOR_IMX576_MAX_DIGITAL_GAIN_SET_VALUE  (0x0FFF)

#define SENSOR_IMX576_ABS_GAIN_GR_SET_ADDR        (0x0B8E)
#define SENSOR_IMX576_ABS_GAIN_R_SET_ADDR         (0x0B90)
#define SENSOR_IMX576_ABS_GAIN_B_SET_ADDR         (0x0B92)
#define SENSOR_IMX576_ABS_GAIN_GB_SET_ADDR        (0x0B94)

#define SENSOR_IMX576_LRC_CAL_BASE_REAR           (0x1560)
#define SENSOR_IMX576_LRC_CAL_BASE_FRONT          (0x4810)
#define SENSOR_IMX576_LRC_CAL_SIZE                (216)
#define SENSOR_IMX576_LRC_REG_ADDR                (0x7520)

#define SENSOR_IMX576_QUAD_SENS_CAL_BASE_REAR     (0x1120)
#define SENSOR_IMX576_QUAD_SENS_CAL_BASE_FRONT    (0x3810)
#define SENSOR_IMX576_QUAD_SENS_CAL_SIZE          (1056)
#define SENSOR_IMX576_QUAD_SENS_REG_ADDR          (0x8000)

#define SENSOR_IMX576_DPC_CAL_BASE_REAR           (0x0900)
#define SENSOR_IMX576_DPC_CAL_BASE_FRONT          (0x3000)
#define SENSOR_IMX576_DPC_CAL_SIZE                (2048)

#define SENSOR_IMX576_DPC_DUMP_NAME               "/data/vendor/camera/IMX576_DPC_DUMP.bin"
#define SENSOR_IMX576_QSC_DUMP_NAME               "/data/vendor/camera/IMX576_QSC_DUMP.bin"
#define SENSOR_IMX576_LRC_DUMP_NAME               "/data/vendor/camera/IMX576_LRC_DUMP.bin"
#define SENSOR_IMX576_CAL_DEBUG                   (0)

#define IMX576J_WRONG_VERSION_CHECK
#ifdef IMX576J_WRONG_VERSION_CHECK
#define IMX576J_PDAF_WRONG_CHIP_VERSION           (0x11)
#define IMX576J_PDAF_SUPPORT_CALMAP_VER           (50) //'2'
#define IMX576J_PROJECT_NAME_FOR_MODULE_CHECK     "A5019LR1"
#endif

/*
 * [Mode Information]
 *	- Global Setting -
 *
 *	- 2X2 BINNING -
 *	[0] REG_Z   : Single Still Preview (4:3) : 2880x2156@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[1] REG_AA : Single Still Preview (16:9) : 2880x1620@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[2] REG_AI  : Single Still Preview (19.5:9) : 2880x1332@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[3] REG_AC : Single Still Preview (1:1) : 2156x2156@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *
 *	- QBC_HDR -
 *	[4] REG_AK : Single Still 3HDR (4:3) : 2880x2156@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[5] REG_AL : Single Still 3HDR (16:9) : 2880x1620@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[6] REG_AN : Single Still 3HDR (19.5:9) : 2880x1332@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[7] REG_AO : Single Still 3HDR (1:1) : 2156x2156@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *
 *	- QBC_REMOSAIC -
 *	[8] REG_Q   : Single Still Capture (4:3) : 5760x4312@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[9] REG_AD : Single Still Capture (16:9) : 5760x3240@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[10]REG_AJ : Single Still Capture (19.5:9) : 5760x2664@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[11]REG_AF : Single Still Capture (1:1) : 4312x4312@30,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *
 *	- Super Slow Motion (SSM) -
 *	[12]REG_O : Super Slow Motion (16:9) : 1280x720 @240,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *	[13]REG_ : Super Slow Motion (16:9) : 1280x720 @120,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 *
 *	- FAST AE -
 *	[14]REG_AG : Single Preview Fast(4:3) : 2880x2156@114,  MIPI lane: 4, MIPI data rate(Mbps/lane) Sensor: 2054
 */

enum sensor_imx576_mode_enum {
	/* 2x2 Binnig */
	SENSOR_IMX576_2880X2156_2X2BIN_30FPS = 0,
	SENSOR_IMX576_2880X1620_2X2BIN_30FPS,
	SENSOR_IMX576_2880X1332_2X2BIN_30FPS,
	SENSOR_IMX576_2156X2156_2X2BIN_30FPS,

	/* QBC-HDR */
	SENSOR_IMX576_2880X2156_QBCHDR_30FPS = 4,
	SENSOR_IMX576_2880X1620_QBCHDR_30FPS,
	SENSOR_IMX576_2880X1332_QBCHDR_30FPS,
	SENSOR_IMX576_2156X2156_QBCHDR_30FPS,

	/* QBC-REMOSAIC */
	SENSOR_IMX576_5760X4312_QBCREMOSAIC_30FPS = 8,
	SENSOR_IMX576_5760X3240_QBCREMOSAIC_30FPS,
	SENSOR_IMX576_5760X2664_QBCREMOSAIC_30FPS,
	SENSOR_IMX576_4312X4312_QBCREMOSAIC_30FPS,

	/* Super Slow Motion */
	SENSOR_IMX576_1872X1052_SSM_240FPS,
	SENSOR_IMX576_2880X1620_SSM_120FPS,

	/* FAST AE */
	SENSOR_IMX576_2880X2156_120FPS = 14,
};

#endif


