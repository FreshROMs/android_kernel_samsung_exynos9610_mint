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

#ifndef FIMC_IS_CIS_2P6_H
#define FIMC_IS_CIS_2P6_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_2P6_MAX_WIDTH		(4608 + 0)
#define SENSOR_2P6_MAX_HEIGHT		(3456 + 0)

/* TODO: Check below values are valid */
#define SENSOR_2P6_FINE_INTEGRATION_TIME_MIN                0x0618
#define SENSOR_2P6_FINE_INTEGRATION_TIME_MAX                0x0618
#define SENSOR_2P6_COARSE_INTEGRATION_TIME_MIN              0x07
#define SENSOR_2P6_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x08

#define USE_GROUP_PARAM_HOLD	(0)

typedef enum
{
    SENSOR_2P6_MODE_4608X3456_30 = 0,
    SENSOR_2P6_MODE_4608X2624_30 = 1,
    SENSOR_2P6_MODE_4608X2240_30 = 2,
    SENSOR_2P6_MODE_2304X1728_30 = 3,
    SENSOR_2P6_MODE_2304X1728_15 = 4,
    SENSOR_2P6_MODE_2304X1312_30 = 5,
    SENSOR_2P6_MODE_2304X1120_30 = 6,
    SENSOR_2P6_MODE_1152X864_120 = 7,
    SENSOR_2P6_MODE_1152X656_120 = 8,
    SENSOR_2P6_MODE_END
}SENSOR_2P6_MODE_ENUM;

#endif

