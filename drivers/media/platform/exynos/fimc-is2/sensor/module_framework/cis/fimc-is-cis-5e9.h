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

#ifndef FIMC_IS_CIS_5E9_H
#define FIMC_IS_CIS_5E9_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_5E9_MAX_WIDTH		(2592 + 0)
#define SENSOR_5E9_MAX_HEIGHT		(1944 + 0)

/* TODO: Check below values are valid */
#define SENSOR_5E9_FINE_INTEGRATION_TIME_MIN                0x64
#define SENSOR_5E9_FINE_INTEGRATION_TIME_MAX                0x64
#define SENSOR_5E9_COARSE_INTEGRATION_TIME_MIN              0x2
#define SENSOR_5E9_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x5

#define USE_GROUP_PARAM_HOLD	(0)
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
#define USE_OTP_AWB_CAL_DATA	(1)
#else
#define USE_OTP_AWB_CAL_DATA	(0)
#endif

#endif

