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

#ifndef FIMC_IS_CIS_3L6_H
#define FIMC_IS_CIS_3L6_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_3L6_MAX_WIDTH		(4128)
#define SENSOR_3L6_MAX_HEIGHT		(3096)

/* TODO: Check below values are valid */
#define SENSOR_3L6_FINE_INTEGRATION_TIME_MIN                0x1C5
#define SENSOR_3L6_FINE_INTEGRATION_TIME_MAX                0x1C5
#define SENSOR_3L6_COARSE_INTEGRATION_TIME_MIN              0x2
#define SENSOR_3L6_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x4

#define USE_GROUP_PARAM_HOLD	(0)

/*Apply the same order as in fimc-is-cis-3l6-setX.h file*/
enum sensor_mode_enum {
	SENSOR_3L6_MODE_4128x3096_30FPS,
	SENSOR_3L6_MODE_4128x2324_30FPS,
	SENSOR_3L6_MODE_4128x1908_30FPS,
	SENSOR_3L6_MODE_3088x3088_30FPS,
	SENSOR_3L6_MODE_1280x720_120FPS,
	SENSOR_3L6_MODE_1024x768_120FPS,
};

#endif

