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

#ifndef FIMC_IS_CIS_4HA_H
#define FIMC_IS_CIS_4HA_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_4HA_MAX_WIDTH		(3264 + 0)
#define SENSOR_4HA_MAX_HEIGHT		(2448 + 0)

/* TODO: Check below values are valid */
#define SENSOR_4HA_FINE_INTEGRATION_TIME_MIN                0xDD8
#define SENSOR_4HA_FINE_INTEGRATION_TIME_MAX                0xDD8
#define SENSOR_4HA_COARSE_INTEGRATION_TIME_MIN              0x2
#define SENSOR_4HA_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x4

#define SENSOR_4HA_COARSE_INTEG_TIME_ADDR                   (0x0202)
#define SENSOR_4HA_FRAME_LENGTH_LINES_ADDR                  (0x0340)
#define SENSOR_4HA_GLOBAL_AGAIN_CODE_ADDR                   (0x0204)
#define SENSOR_4HA_DGAIN_CODE_ADDR                          (0x020E)

#define USE_GROUP_PARAM_HOLD	(0)

#endif

