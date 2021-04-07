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

#ifndef FIMC_IS_CIS_IMX586_H
#define FIMC_IS_CIS_IMX586_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX586_MAX_WIDTH          (8000 + 0)
#define SENSOR_IMX586_MAX_HEIGHT         (6000 + 0)

#define USE_GROUP_PARAM_HOLD                      (1)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (8)

#define SENSOR_IMX586_FINE_INTEGRATION_TIME                    (1588)    //FINE_INTEG_TIME is a fixed value (0x0200: 16bit - read value is 0x0634)
#define SENSOR_IMX586_COARSE_INTEGRATION_TIME_MIN              (16)
#define SENSOR_IMX586_COARSE_INTEGRATION_TIME_MIN_FOR_QBCHDR   (11)
#define SENSOR_IMX586_COARSE_INTEGRATION_TIME_MAX_MARGIN       (48)

#define SENSOR_IMX586_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_IMX586_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_IMX586_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_IMX586_OTP_CHIP_REVISION_ADDR      (0x0018)

#define SENSOR_IMX586_FRAME_LENGTH_LINE_ADDR      (0x0340)
#define SENSOR_IMX586_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_IMX586_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_IMX586_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_IMX586_DIG_GAIN_ADDR               (0x020E)
#define SENSOR_IMX586_SOHT_DIG_GAIN_ADDR          (0x0218)

#define SENSOR_IMX586_MIN_ANALOG_GAIN_SET_VALUE   (112)
#define SENSOR_IMX586_MAX_ANALOG_GAIN_SET_VALUE   (1008)
#define SENSOR_IMX586_MIN_DIGITAL_GAIN_SET_VALUE  (0x0100)
#define SENSOR_IMX586_MAX_DIGITAL_GAIN_SET_VALUE  (0x0FFF)

#define SENSOR_IMX586_ABS_GAIN_GR_SET_ADDR        (0x0B8E)
#define SENSOR_IMX586_ABS_GAIN_R_SET_ADDR         (0x0B90)
#define SENSOR_IMX586_ABS_GAIN_B_SET_ADDR         (0x0B92)
#define SENSOR_IMX586_ABS_GAIN_GB_SET_ADDR        (0x0B94)

#define SENSOR_IMX586_LRC_CAL_BASE_REAR           (0x1560)
#define SENSOR_IMX586_LRC_CAL_BASE_FRONT          (0x4810)
#define SENSOR_IMX586_LRC_CAL_SIZE                (216)
#define SENSOR_IMX586_LRC_REG_ADDR                (0x7520)

#define SENSOR_IMX586_QUAD_SENS_CAL_BASE_REAR     (0x1120)
#define SENSOR_IMX586_QUAD_SENS_CAL_BASE_FRONT    (0x3810)
#define SENSOR_IMX586_QUAD_SENS_CAL_SIZE          (1056)
#define SENSOR_IMX586_QUAD_SENS_REG_ADDR          (0x8000)

#define SENSOR_IMX586_DPC_CAL_BASE_REAR           (0x0900)
#define SENSOR_IMX586_DPC_CAL_BASE_FRONT          (0x3000)
#define SENSOR_IMX586_DPC_CAL_SIZE                (2048)

#define SENSOR_IMX586_DPC_DUMP_NAME               "/data/vendor/camera/IMX586_DPC_DUMP.bin"
#define SENSOR_IMX586_QSC_DUMP_NAME               "/data/vendor/camera/IMX586_QSC_DUMP.bin"
#define SENSOR_IMX586_LRC_DUMP_NAME               "/data/vendor/camera/IMX586_LRC_DUMP.bin"

#define WRITE_PDAF_CAL                            (0)
#define WRITE_SENSOR_CAL_FOR_REMOSAIC             (0)
#define SENSOR_IMX586_CAL_DEBUG                   (0)
#define DEBUG_IMX586_PLL                          (0)


/*
 * [Mode Information]
 *
 * Reference File : IMX586_SAM-DPHY-26MHz_RegisterSetting_ver3.00-10.00_MP_b3_190222.xlsx
 * Update Data    : 2019-03-07
 * Author         : takkyoum.kim
 *
 * - Global Setting -
 *
 * - 2x2 BIN For Single Still Preview / Capture -
 *    [  0 ] REG_C2 : 2x2 Binning Full 4000x3000 30fps    : Single Still Preview (4:3)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  1 ] REG_G1 : 2x2 Binning Crop 4000x2268 30fps    : Single Still Preview (16:9)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  2 ] REG_I1 : 2x2 Binning Crop 4000x1968 30fps    : Single Still Preview (18.5:9) ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  3 ] REG_J1 : 2x2 Binning Crop 4000x1860 30fps    : Single Still Preview (19.5:9) ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  4 ] REG_K1 : 2x2 Binning Crop 4000x1816 30fps    : Single Still Preview (20:9)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  5 ] REG_L1 : 2x2 Binning Crop 3000x3000 30fps    : Single Still Preview (1:1)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 * 
 * - 2x2 BIN For Bokeh Preview / Capture -
 *    [  6 ] REG_C4 : 2x2 Binning Full 4000x3000 24fps    : Bokeh Preview (4:3)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  7 ] REG_G2 : 2x2 Binning Crop 4000x2268 24fps    : Bokeh Preview (16:9)          ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  8 ] REG_I2 : 2x2 Binning Crop 4000x1968 24fps    : Bokeh Preview (18.5:9)        ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [  9 ] REG_J2 : 2x2 Binning Crop 4000x1860 24fps    : Bokeh Preview (19.5:9)        ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [ 10 ] REG_K2 : 2x2 Binning Crop 4000x1816 24fps    : Bokeh Preview (20:9)          ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *    [ 11 ] REG_L2 : 2x2 Binning Crop 3000x3000 24fps    : Bokeh Preview (1:1)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *
 * - FULL Remosaic For Single Still Remosaic Capture -
 *    [ 12 ] REG_A  : Full Remosaic 8000x6000 18fps       : Single Still Remosaic Capture (4:3) , MIPI lane: 4, MIPI data rate(Mbps/lane): 2496
 *
 */

enum sensor_imx586_mode_enum {
	/* 2x2 Binnig 30Fps */
	SENSOR_IMX586_2X2BIN_FULL_4000X3000_30FPS = 0,
	SENSOR_IMX586_2X2BIN_CROP_4000x2268_30FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000x1968_30FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000x1860_30FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000x1816_30FPS,
	SENSOR_IMX586_2X2BIN_CROP_3000x3000_30FPS,
	/* 2X2 Binning 24Fps */
	SENSOR_IMX586_2X2BIN_FULL_4000X3000_24FPS = 6,
	SENSOR_IMX586_2X2BIN_CROP_4000x2268_24FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000x1968_24FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000x1860_24FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000x1816_24FPS,
	SENSOR_IMX586_2X2BIN_CROP_3000x3000_24FPS,
	/* FULL Remosaic */
	SENSOR_IMX586_FULL_REMOSAIC_8000x6000 = 12,
};

#endif


