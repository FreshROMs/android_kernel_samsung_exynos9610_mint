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

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-fimc-is-sensor.h>
#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-module-base.h"


#define SENSOR_HI1336_NAME		SENSOR_NAME_HI1336

#ifndef USE_VENDOR_PWR_PIN_NAME
#define HI1336_IOVDD    "CAM_VLDO3"         /* CAM_VDDIO_1P8  */
#define HI1336_AVDD     "gpio_cam_a2p8_en"  /* RCAM3_AVDD_2P8: XGPIO35: GPM22[0] */
#define HI1336_DVDD     "CAM_VLDO4"         /* RCAM3_DVDD_1P05 */
#endif

struct pin_info {
	int gpio; /* gpio_none or gpio name */
	int type; /* PIN_OUTPUT, PIN_REGULATOR */
};

/*
 * [Mode Information]
 *
 * Reference File : Hi-1336_setting_v4.0.2_SEC12_20190613.xlsx
 * Update Data    : 2019-06-18
 * Author         : bluesay.kim
 *
 *  - Global setting
 *
 *  - Mode setting
 *  [ 0 ] 4208 x 3120(13M full) @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 1 ] 4032 x 3024(4:3)      @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 2 ] 4032 x 2268(16:9)     @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 3 ] 4032 x 1860(19.5:9)   @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 4 ] 4032 x 1816(20:9)     @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 5 ] 3024 x 3024(1:1)      @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 6 ] 2016 x 1512(4:3)      @30,  MIPI lane: 4, MIPI data rate(Mbps/lane): 747.5, Mclk(Mhz): 26
 *  [ 7 ] 2016 x 1134(16:9)     @30,  MIPI lane: 4, MIPI data rate(Mbps/lane): 747.5, Mclk(Mhz): 26
 *  [ 8 ] 1008 x 756(4:3)       @120, MIPI lane: 4, MIPI data rate(Mbps/lane):   364, Mclk(Mhz): 26
 *
 *  - Default Mode setting For HI1336B
 *  [ 9 ]  4128 x 3096(4:3)      @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 10 ] 4128 x 2324(16:9)     @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 11 ] 4128 x 1908(19.5:9)   @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 12 ] 3088 x 3088(1:1)      @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  - Sensor Crop Mode (68') setting For HI1336B
 *  [ 13 ] 3408 x 2556(4:3)      @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 14 ] 3408 x 1916(16:9)     @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 15 ] 3408 x 1576(19.5:9)   @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  [ 16 ] 2560 x 2560(1:1)      @30,  MIPI lane: 4, MIPI data rate(Mbps/lane):  1456, Mclk(Mhz): 26
 *  - FAST AE (4 binning) setting For HI1336B
 *  [ 17 ] 1024 x 768(4:3)       @120, MIPI lane: 4, MIPI data rate(Mbps/lane):   364, Mclk(Mhz): 26
 *
 *  - Fsync setting
 *  [ 0 ] Normal
 *  [ 1 ] Slave
 *  [ 2 ] Master full
 *  [ 3 ] Master 2-binnig
 *  [ 4 ] Master 4-binning
 *
 */
 
static struct fimc_is_sensor_cfg config_hi1336[] = {
	/* Full (4208x3120) */
	FIMC_IS_SENSOR_CFG_EX(4208, 3120,  30, 0, 0, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4280, 3120),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(4032x3024) 4:3 */
	FIMC_IS_SENSOR_CFG_EX(4032, 3024,  30, 0, 1, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4032, 3024),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(4032x2268) 16:9 */
	FIMC_IS_SENSOR_CFG_EX(4032, 2268,  30, 0, 2, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4032, 2268),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(4032x1860) 19.5:9 */
	FIMC_IS_SENSOR_CFG_EX(4032, 1860,  30, 0, 3, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4032, 1860),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(4032x1816) 20:9 */
	FIMC_IS_SENSOR_CFG_EX(4032, 1816,  30, 0, 4, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4032, 1816),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(3024x3024) 1:1 */
	FIMC_IS_SENSOR_CFG_EX(3024, 3024,  30, 0, 5, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3024, 3024),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 2-binning Crop(2016x1512) 4:3 */
	FIMC_IS_SENSOR_CFG_EX(2016, 1512,  30, 0, 6, CSI_DATA_LANES_4,  748, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2016, 1512),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 2-binning Crop(2016x1134) 16:9 */
	FIMC_IS_SENSOR_CFG_EX(2016, 1134,  30, 0, 7, CSI_DATA_LANES_4,  748, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2016, 1134),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 4-binning Crop(1008x756) 4:3 */
	FIMC_IS_SENSOR_CFG_EX(1008,  756, 120, 0, 8, CSI_DATA_LANES_4,  364, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 1008, 756),   VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(4128x3096) 4:3 */
	FIMC_IS_SENSOR_CFG_EX(4128, 3096,  30, 0, 9, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4128, 3096),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(4128x2324) 16:9 */
	FIMC_IS_SENSOR_CFG_EX(4128, 2324,  30, 0, 10, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4128, 2324),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(4128x1908) 19.5:9 */
	FIMC_IS_SENSOR_CFG_EX(4128, 1908,  30, 0, 11, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4128, 1908),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(3088x3088) 1:1 */
	FIMC_IS_SENSOR_CFG_EX(3088, 3088,  30, 0, 12, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3088, 3088),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(3408x2556) 4:3 */
	FIMC_IS_SENSOR_CFG_EX(3408, 2556,  30, 0, 13, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3408, 2556),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(3408x1916) 16:9 */
	FIMC_IS_SENSOR_CFG_EX(3408, 1916,  30, 0, 14, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3408, 1916),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(3408x1576) 19.5:9 */
	FIMC_IS_SENSOR_CFG_EX(3408, 1576,  30, 0, 15, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3408, 1576),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* Full Crop(2560x2560) 1:1 */
	FIMC_IS_SENSOR_CFG_EX(2560, 2560,  30, 0, 16, CSI_DATA_LANES_4, 1456, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2560, 2560),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 4-binning FastAE */
	FIMC_IS_SENSOR_CFG_EX(1024, 768,  120, 0, 17, CSI_DATA_LANES_4, 364, CSI_MODE_CH0_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 1024, 768),  VC_OUT(HW_FORMAT_RAW10,   VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),      VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
};

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_module_init,
	.g_ctrl = sensor_module_g_ctrl,
	.s_ctrl = sensor_module_s_ctrl,
	.g_ext_ctrls = sensor_module_g_ext_ctrls,
	.s_ext_ctrls = sensor_module_s_ext_ctrls,
	.ioctl = sensor_module_ioctl,
	.log_status = sensor_module_log_status,
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_routing = sensor_module_s_routing,
	.s_stream = sensor_module_s_stream,
	.s_parm = sensor_module_s_param
};

static const struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt = sensor_module_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad = &pad_ops
};

static int sensor_hi1336_power_setpin(struct device *dev,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct fimc_is_core *core;
	struct device_node *dnode = dev->of_node;
	int gpio_mclk = 0;
	int gpio_reset = 0;
	int gpio_cam_a2p8_en = 0;
	int gpio_none = 0;

	struct pin_info pin_a2p8;

	FIMC_BUG(!dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

	dev_info(dev, "%s E v4\n", __func__);

	gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
	if (!gpio_is_valid(gpio_mclk)) {
		dev_err(dev, "failed to get gpio_mclk\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW");
		gpio_free(gpio_mclk);
	}

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	}else{
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	gpio_cam_a2p8_en = of_get_named_gpio(dnode, HI1336_AVDD, 0);
	if (!gpio_is_valid(gpio_cam_a2p8_en)) {
		dev_err(dev, "%s is PIN_REGULATOR\n", HI1336_AVDD);
		pin_a2p8.gpio	= gpio_none;
		pin_a2p8.type	= PIN_REGULATOR;
	} else {
		gpio_request_one(gpio_cam_a2p8_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_cam_a2p8_en);
		pin_a2p8.gpio	= gpio_cam_a2p8_en;
		pin_a2p8.type	= PIN_OUTPUT;
	}

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL,   GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL,   GPIO_SCENARIO_OFF);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);

	/* Normal on */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_reset, SENSOR_RESET_LOW,  PIN_OUTPUT,    0, 500);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  HI1336_IOVDD,      PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  pin_a2p8.gpio, HI1336_AVDD,    pin_a2p8.type, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  HI1336_DVDD,       PIN_REGULATOR, 1, 0);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  SENSOR_MCLK_PIN,   PIN_FUNCTION,  1, 1000);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_reset, SENSOR_RESET_HIGH, PIN_OUTPUT,    1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  SENSOR_SET_DELAY,  PIN_NONE,      0, 5000);

	/* Normal off */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  SENSOR_SET_DELAY,  PIN_NONE,      0, 5000);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, SENSOR_RESET_LOW,  PIN_OUTPUT,    0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  SENSOR_MCLK_PIN,   PIN_FUNCTION,  0, 1000);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  HI1336_DVDD,       PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, pin_a2p8.gpio, HI1336_AVDD,    pin_a2p8.type, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  HI1336_IOVDD,      PIN_REGULATOR, 0, 0);

	/* READ_ROM - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON,  gpio_none, HI1336_IOVDD,     PIN_REGULATOR, 1, 0);
	
	/* READ_ROM - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, HI1336_IOVDD,     PIN_REGULATOR, 0, 0);

	dev_info(dev, "%s X v4\n", __func__);

	return 0;
}

static int __init sensor_module_hi1336_probe(struct platform_device *pdev)
{
	int ret = 0;
	bool use_pdaf = false;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	struct exynos_platform_fimc_is_module *pdata;
	struct device *dev;
	struct pinctrl_state *s;

	FIMC_BUG(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &pdev->dev;

	if (of_property_read_bool(dev->of_node, "use_pdaf")) {
		use_pdaf = true;
	}

	probe_info("%s use_pdaf(%d)\n", __func__, use_pdaf);

	fimc_is_module_parse_dt(dev, sensor_hi1336_power_setpin);

	pdata = dev_get_platdata(dev);
	device = &core->sensor[pdata->id];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		ret = -ENOMEM;
		goto p_err;
	}

	module = &device->module_enum[atomic_read(&device->module_count)];
	atomic_inc(&device->module_count);
	clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->dev = dev;
	module->sensor_id = SENSOR_NAME_HI1336;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = 4208;
	module->active_height = 3120;

	module->margin_left = 0;
	module->margin_right = 0;
	module->margin_top = 0;
	module->margin_bottom = 0;
	module->pixel_width = module->active_width + 0;
	module->pixel_height = module->active_height + 0;
	//module->max_framerate = 247;
	module->max_framerate = 120;
	module->position = pdata->position;
	module->bitwidth = 10;
	module->sensor_maker = "HYNIX";
	module->sensor_name = "HI1336";
	module->setfile_name = "setfile_hi1336.bin";
	module->cfgs = ARRAY_SIZE(config_hi1336);
	module->cfg = config_hi1336;
	module->ops = NULL;

	/* Sensor peri */
	module->private_data = kzalloc(sizeof(struct fimc_is_device_sensor_peri), GFP_KERNEL);
	if (!module->private_data) {
		ret = -ENOMEM;
		goto p_err;
	}
	fimc_is_sensor_peri_probe((struct fimc_is_device_sensor_peri *)module->private_data);
	PERI_SET_MODULE(module);

	ext = &module->ext;
	ext->sensor_con.product_name = module->sensor_id /*SENSOR_NAME_HI1336*/;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
	ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->actuator_con.product_name = ACTUATOR_NAME_NOTHING;
	ext->flash_con.product_name = FLADRV_NAME_NOTHING;
	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->preprocessor_con.product_name = PREPROCESSOR_NAME_NOTHING;
	ext->ois_con.product_name = OIS_NAME_NOTHING;

	if (pdata->af_product_name !=  ACTUATOR_NAME_NOTHING) {
		ext->actuator_con.product_name = pdata->af_product_name;
		ext->actuator_con.peri_type = SE_I2C;
		ext->actuator_con.peri_setting.i2c.channel = pdata->af_i2c_ch;
		ext->actuator_con.peri_setting.i2c.slave_address = pdata->af_i2c_addr;
		ext->actuator_con.peri_setting.i2c.speed = 400000;
	}

	if (pdata->flash_product_name != FLADRV_NAME_NOTHING) {
		ext->flash_con.product_name = pdata->flash_product_name;
		ext->flash_con.peri_type = SE_GPIO;
		ext->flash_con.peri_setting.gpio.first_gpio_port_no = pdata->flash_first_gpio;
		ext->flash_con.peri_setting.gpio.second_gpio_port_no = pdata->flash_second_gpio;
	}

	if (pdata->preprocessor_product_name != PREPROCESSOR_NAME_NOTHING) {
		ext->preprocessor_con.product_name = pdata->preprocessor_product_name;
		ext->preprocessor_con.peri_info0.valid = true;
		ext->preprocessor_con.peri_info0.peri_type = SE_SPI;
		ext->preprocessor_con.peri_info0.peri_setting.spi.channel = pdata->preprocessor_spi_channel;
		ext->preprocessor_con.peri_info1.valid = true;
		ext->preprocessor_con.peri_info1.peri_type = SE_I2C;
		ext->preprocessor_con.peri_info1.peri_setting.i2c.channel = pdata->preprocessor_i2c_ch;
		ext->preprocessor_con.peri_info1.peri_setting.i2c.slave_address = pdata->preprocessor_i2c_addr;
		ext->preprocessor_con.peri_info1.peri_setting.i2c.speed = 400000;
		ext->preprocessor_con.peri_info2.valid = true;
		ext->preprocessor_con.peri_info2.peri_type = SE_DMA;
		if (pdata->preprocessor_dma_channel == DMA_CH_NOT_DEFINED)
			ext->preprocessor_con.peri_info2.peri_setting.dma.channel = FLITE_ID_D;
		else
			ext->preprocessor_con.peri_info2.peri_setting.dma.channel = pdata->preprocessor_dma_channel;
	}

	if (pdata->ois_product_name != OIS_NAME_NOTHING) {
		ext->ois_con.product_name = pdata->ois_product_name;
		ext->ois_con.peri_type = SE_I2C;
		ext->ois_con.peri_setting.i2c.channel = pdata->ois_i2c_ch;
		ext->ois_con.peri_setting.i2c.slave_address = pdata->ois_i2c_addr;
		ext->ois_con.peri_setting.i2c.speed = 400000;
	} else {
		ext->ois_con.product_name = pdata->ois_product_name;
		ext->ois_con.peri_type = SE_NULL;
	}

	v4l2_subdev_init(subdev_module, &subdev_ops);

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

	s = pinctrl_lookup_state(pdata->pinctrl, "release");

	if (pinctrl_select_state(pdata->pinctrl, s) < 0) {
		probe_err("pinctrl_select_state is fail\n");
		goto p_err;
	}
p_err:
	probe_info("%s(%d)\n", __func__, ret);
	return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_module_hi1336_match[] = {
	{
		.compatible = "samsung,sensor-module-hi1336",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_module_hi1336_match);

static struct platform_driver sensor_module_hi1336_driver = {
	.driver = {
		.name   = "FIMC-IS-SENSOR-MODULE-HI1336",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_module_hi1336_match,
	}
};

static int __init fimc_is_sensor_module_hi1336_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_module_hi1336_driver,
				sensor_module_hi1336_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_module_hi1336_driver.driver.name, ret);

	return ret;
}
late_initcall(fimc_is_sensor_module_hi1336_init);

