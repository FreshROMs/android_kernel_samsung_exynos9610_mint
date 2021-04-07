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


#define SENSOR_IMX616_NAME		SENSOR_NAME_IMX616

#ifndef USE_VENDOR_PWR_PIN_NAME
#define IMX616_AVDD		"CAM_VLDO7"       /* CAM_PMIC_VLDO7 */
#define IMX616_DVDD		"CAM_VLDO2"       /* CAM_PMIC_VLDO2 */
#define IMX616_IOVDD		"CAM_VLDO3"       /* CAM_PMIC_VLDO3  */
#endif

/*
 * [Mode Information]
 *
 * Reference File : IMX616-AAJH5_SAM-DPHY_26MHz_RegisterSetting_ver6.00-8.20_b2_190610.xlsx
 *
 * -. Global Setting -
 *
 * -. 2x2 BIN For Single Still Preview / Capture -
 *    [ 0 ] REG_H : 2x2 Binning 3264x2448 30fps    : Single Still Preview (4:3)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *    [ 1 ] REG_I : 2x2 Binning 3264x1836 30fps    : Single Still Preview (16:9)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *    [ 2 ] REG_J : 2x2 Binning 3264x1504 30fps    : Single Still Preview (19.5:9)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *    [ 3 ] REG_N : 2x2 Binning 2448x2448 30fps    : Single Still Preview (1:1)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *
 * -. 68━2x2 BIN For Single Still Preview / Capture -
 *    [ 4 ] REG_K : 2x2 Binning 2640x1980 30fps    : Single Still Preview (4:3)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *    [ 5 ] REG_L : 2x2 Binning 2640x1448 30fps    : Single Still Preview (16:9)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *    [ 6 ] REG_M : 2x2 Binning 2640x1216 30fps    : Single Still Preview (19.5:9)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *    [ 7 ] REG_O_2 : 2x2 Binning 1968x1968 30fps    : Single Still Preview (1:1)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *
 * -. 2x2 BIN H2V2 For FastAE
 *    [ 8 ] REG_R : 2x2 Binning 1632x1224 120fps	 : FAST AE (4:3)	,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2054
 *
 * -. FULL Remosaic For Single Still Remosaic Capture -
 *    [ 9 ] REG_F  : Full Remosaic 6528x4896 24fps		 : Single Still Remosaic Capture (4:3) , MIPI lane: 4, MIPI data rate(Mbps/lane): 2218
 *
 * -. 68━FULL Remosaic For Single Still Remosaic Capture -
 *    [ 10 ] REG_G : Full Remosaic 5264x3948 24fps		 : Single Still Remosaic Capture (4:3) , MIPI lane: 4, MIPI data rate(Mbps/lane): 2218
 *
 * -. QBC HDR
 *    [ 11 ] REG_S : QBC HDR 3264x2448 24fps	   : Single Still Preview (4:3)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2218
 */


static struct fimc_is_sensor_cfg config_imx616[] = {
	/* 0 : 2X2BIN_3264x2448_30FPS */
	FIMC_IS_SENSOR_CFG_EX(3264, 2448,  30, 0, 0, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3264, 2448),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 1 : 2X2BIN_3264x1836_30FPS */
	FIMC_IS_SENSOR_CFG_EX(3264, 1836,  30, 0, 1, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3264, 1836),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 2 : 2X2BIN_3264x1504_30FPS */
	FIMC_IS_SENSOR_CFG_EX(3264, 1504,  30, 0, 2, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3264, 1504),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 3: 2X2BIN_2448x2448_30FPS */
	FIMC_IS_SENSOR_CFG_EX(2448, 2448,  30, 0, 3, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2448, 2448),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 4 : 68━Crop 2X2BIN_2640x1980_30FPS */
	FIMC_IS_SENSOR_CFG_EX(2640, 1980,  30, 0, 4, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2640, 1980),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 5 : 68━Crop 2X2BIN_2640x1488_30FPS */
	FIMC_IS_SENSOR_CFG_EX(2640, 1488,  30, 0, 5, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2640, 1488),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 6 : 68━Crop 2X2BIN_2640x1216_30FPS */
	FIMC_IS_SENSOR_CFG_EX(2640, 1216,  30, 0, 6, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2640, 1216),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 7 : 68━Crop  2X2BIN_1968x1968_30FPS*/
	FIMC_IS_SENSOR_CFG_EX(1968, 1968,  30, 0, 7, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 1968, 1968),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 8 : H2V2_1632x1224_120FPS */
	FIMC_IS_SENSOR_CFG_EX(1632, 1224,  120, 0, 8, CSI_DATA_LANES_4, 2054, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 1632, 1224),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 9 : 2X2BIN_3264x1836_120FPS */
	FIMC_IS_SENSOR_CFG_EX(3264, 1836, 120, 0, 9, CSI_DATA_LANES_4, 2218, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3264, 1836),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 10 : REMOSAIC_6528x4896_24FPS */
	FIMC_IS_SENSOR_CFG_EX(6528, 4896,  24, 0, 10, CSI_DATA_LANES_4, 2218, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 6528, 4896),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 11 : 68━Crop REMOSAIC_5264x3948_24FPS */
	FIMC_IS_SENSOR_CFG_EX(5264, 3948,  24, 0, 11, CSI_DATA_LANES_4, 2218, CSI_MODE_DT_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 5264, 3948),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 12 : QBCHDR_3264x2448_24FPS */
	FIMC_IS_SENSOR_CFG_EX(3264, 2448,  24, 0, 12, CSI_DATA_LANES_4, 2218, CSI_MODE_DT_ONLY, PD_NONE, EX_3DHDR,
		VC_IN(0, HW_FORMAT_RAW10, 3264, 2448),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 13 : QBCHDR_3264x1836_30FPS */
	FIMC_IS_SENSOR_CFG_EX(3264, 1836,  30, 0, 13, CSI_DATA_LANES_4, 2218, CSI_MODE_DT_ONLY, PD_NONE, EX_3DHDR,
		VC_IN(0, HW_FORMAT_RAW10, 3264, 1836),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
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

static int sensor_imx616_power_setpin(struct device *dev,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct fimc_is_core *core;
	struct device_node *dnode = dev->of_node;
	int gpio_mclk = 0;
	int gpio_reset = 0;
	int gpio_none = 0;

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

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);

	/* Normal on */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_ON, gpio_reset, SENSOR_RESET_LOW,	PIN_OUTPUT,    0, 500);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_ON, gpio_none, IMX616_AVDD,	PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_ON, gpio_none, IMX616_IOVDD,	PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_ON, gpio_none, IMX616_DVDD,	PIN_REGULATOR, 1, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_ON, gpio_none, SENSOR_MCLK_PIN,	PIN_FUNCTION,  2, 1000);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_ON, gpio_reset, SENSOR_RESET_HIGH, PIN_OUTPUT,    1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_ON, gpio_none, SENSOR_SET_DELAY,	PIN_NONE,      0, 15000);

	/* Normal off */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_none, SENSOR_SET_DELAY,	PIN_NONE,      0, 5000);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_reset, SENSOR_RESET_LOW, PIN_OUTPUT,    0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_none, SENSOR_MCLK_PIN,	PIN_FUNCTION,  0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_none, SENSOR_MCLK_PIN,	PIN_FUNCTION,  1, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_none, SENSOR_MCLK_PIN,	PIN_FUNCTION,  0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_none, IMX616_AVDD,	PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_none, IMX616_IOVDD,	PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL,	GPIO_SCENARIO_OFF, gpio_none, IMX616_DVDD,	PIN_REGULATOR, 0, 0);

	/* READ_ROM - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON,  gpio_none, IMX616_IOVDD,      PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON,  gpio_none, SENSOR_SET_DELAY,  PIN_NONE,      0, 3000);
	/* READ_ROM - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, IMX616_IOVDD,      PIN_REGULATOR, 0, 0);

	dev_info(dev, "%s X v4\n", __func__);

	return 0;
}

static int __init sensor_module_imx616_probe(struct platform_device *pdev)
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

	fimc_is_module_parse_dt(dev, sensor_imx616_power_setpin);

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
	module->sensor_id = SENSOR_NAME_IMX616;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = 6528;
	module->active_height = 4896;
	module->margin_left = 0;
	module->margin_right = 0;
	module->margin_top = 0;
	module->margin_bottom = 0;
	module->pixel_width = module->active_width + 0;
	module->pixel_height = module->active_height + 0;
	module->max_framerate = 120;
	module->position = pdata->position;
	module->bitwidth = 10;
	module->sensor_maker = "SONY";
	module->sensor_name = "IMX616";
	module->setfile_name = "setfile_imx616.bin";
	module->cfgs = ARRAY_SIZE(config_imx616);
	module->cfg = config_imx616;
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
	ext->sensor_con.product_name = module->sensor_id /*SENSOR_NAME_IMX616*/;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
	ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
	ext->sensor_con.peri_setting.i2c.speed = 1000000;

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

static const struct of_device_id exynos_fimc_is_sensor_module_imx616_match[] = {
	{
		.compatible = "samsung,sensor-module-imx616",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_module_imx616_match);

static struct platform_driver sensor_module_imx616_driver = {
	.driver = {
		.name   = "FIMC-IS-SENSOR-MODULE-IMX616",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_module_imx616_match,
	}
};

static int __init fimc_is_sensor_module_imx616_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_module_imx616_driver,
				sensor_module_imx616_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_module_imx616_driver.driver.name, ret);

	return ret;
}
late_initcall(fimc_is_sensor_module_imx616_init);

