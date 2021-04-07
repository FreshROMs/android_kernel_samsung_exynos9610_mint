/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_GC5035_H
#define FIMC_IS_CIS_GC5035_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GC5035_MAX_WIDTH          (2576)
#define SENSOR_GC5035_MAX_HEIGHT         (1932)

#define SENSOR_GC5035_FINE_INTEGRATION_TIME_MIN                (0x00)
#define SENSOR_GC5035_FINE_INTEGRATION_TIME_MAX                (0x00)
#define SENSOR_GC5035_COARSE_INTEGRATION_TIME_MIN              (0x04)
#define SENSOR_GC5035_COARSE_INTEGRATION_TIME_MAX_MARGIN       (0x10)

#define USE_GROUP_PARAM_HOLD            (0)
#define USE_OTP_AWB_CAL_DATA            (0)

#define SENSOR_GC5035_MIN_ANALOG_GAIN_SET_VALUE   (0x00)
#define SENSOR_GC5035_MAX_ANALOG_GAIN_SET_VALUE   (0x14)

#define SENSOR_GC5035_MIN_DIGITAL_GAIN_SET_VALUE   (0x0100)
#define SENSOR_GC5035_MAX_DIGITAL_GAIN_SET_VALUE   (0x09FF)

#define MAX_GAIN_INDEX    17
#define CODE_GAIN_INDEX    0
#define PERMILE_GAIN_INDEX 1

const u32 sensor_gc5035_analog_gain[][MAX_GAIN_INDEX] = {
	{0, 1000},
	{1, 1180},
	{2, 1400},
	{3, 1660},
	{8, 1960},
	{9, 2340},
	{10, 2800},
	{11, 3300},
	{12, 3900},
	{13, 4700},
	{14, 5600},
	{15, 6680},
	{16, 7800},
	{17, 9200},
	{18, 11000},
	{19, 12960},
	{20, 15600},
	{21, 16000},
};

#endif //FIMC_IS_CIS_GC5035_H
