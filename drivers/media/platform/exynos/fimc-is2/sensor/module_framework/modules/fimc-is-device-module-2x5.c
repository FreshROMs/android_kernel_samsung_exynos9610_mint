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
#include <linux/of_gpio.h>
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

#define SENSOR_2X5_MAX_WIDTH		(5760 + 0)
#define SENSOR_2X5_MAX_HEIGHT		(4312 + 0)
#define MAX_2X5_SETPIN_CNT 2

enum sensor_module_2x5_actuator {
	SENSOR_MODULE_2X5_WITHOUT_ACTUATOR = 0,
	SENSOR_MODULE_2X5_WITH_ACTUATOR = 1,
};

enum sensor_module_2x5_position {
	SENSOR_MODULE_2X5_REAR = 0,
	SENSOR_MODULE_2X5_FRONT = 1,
};

/** Sensor CFG
0: 2880x2156@30fps
1: 2880x1620@30fps
2: 2880x1332@30fps
3: 2156x2156@30fps
4: 2880x2156@120fps
5: 5760x4312@30fps
6: 5760x3240@30fps
7: 5760x2664@30fps
8: 4312x4312@30fps
9: 2880x2156@30fps_3dhdr
*/
enum {
	MODE_2880x2156_30 = 0,		/* 0, 4:3 */
	MODE_2880x1620_30, 		/* 1, 16:9 */
	MODE_2880x1332_30,		/* 2, 19.5 : 9 */
	MODE_2156x2156_30,		/* 3, 1:1*/
	MODE_2352x1764_30,		/* 4, crop */
	MODE_2352x1324_30,		/* 5, crop */
	MODE_2352x1088_30,		/* 6, crop */
	MODE_1760x1760_30,		/* 7, crop */
	MODE_2880x2156_100,		/* 8 */
	MODE_5760x4312_30, 		/* 9, Remosaic */
	MODE_5760x3240_30,		/* 10, 16:9 */
	MODE_5760x2664_30,		/* 11, 19.5 : 9 */
	MODE_4312x4312_30,		/* 12, 1:1 */
	MODE_4688x3516_30,		/* 13, crop */
	MODE_2880x2156_30_3DHDR,	/* 14 */
	MODE_2880x1620_30_3DHDR,	/* 15 */
	MODE_2880x1332_30_3DHDR,	/* 16 */
	MODE_2156x2156_30_3DHDR,	/* 17 */
	MODE_END
};

static struct fimc_is_sensor_cfg config_module_2x5[] = {
	/* width, height, fps, settle, mode, lane, speed, interleave, pd_mode */
	/* 0: 2880x2156@30fps */
	FIMC_IS_SENSOR_CFG(2880, 2156, 30, 0, MODE_2880x2156_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2880, 2156), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 1: 2880x1620@30fps */
	FIMC_IS_SENSOR_CFG(2880, 1620, 30, 0, MODE_2880x1620_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2880, 1620), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 2: 2880x1332@30fps */
	FIMC_IS_SENSOR_CFG(2880, 1332, 30, 0, MODE_2880x1332_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2880, 1332), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 3: 2156x2156@30fps */
	FIMC_IS_SENSOR_CFG(2156, 2156, 30, 0, MODE_2156x2156_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10,  2156, 2156), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 4: 2352x1764@30fps_crop */
	FIMC_IS_SENSOR_CFG(2352, 1764, 30, 0, MODE_2352x1764_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10,  2352, 1764), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 5: 2352x1324@30fps_crop */
	FIMC_IS_SENSOR_CFG(2352, 1324, 30, 0, MODE_2352x1324_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10,  2352, 1324), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 6: 2352x1088@30fps_crop */
	FIMC_IS_SENSOR_CFG(2352, 1088, 30, 0, MODE_2352x1088_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10,  2352, 1088), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 7: 1760x1760@30fps_crop */
	FIMC_IS_SENSOR_CFG(1760, 1760, 30, 0, MODE_1760x1760_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 1760, 1760), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 8: 2880x2156@100fps FAST AE*/
	FIMC_IS_SENSOR_CFG(2880, 2156, 100, 0, MODE_2880x2156_100, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2880, 2156), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 9: 5760x4312@30fps */
	FIMC_IS_SENSOR_CFG(5760, 4312, 30, 0, MODE_5760x4312_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 5760, 4312), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 10: 5760x3240@30fps */
	FIMC_IS_SENSOR_CFG(5760,  3240, 30, 0, MODE_5760x3240_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 5760, 3240), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 11: 5760x2664@30fps */
	FIMC_IS_SENSOR_CFG(5760, 2664, 30, 0, MODE_5760x2664_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 5760, 2664), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 12: 4312x4312@30fps */
	FIMC_IS_SENSOR_CFG(4312, 4312, 30, 0, MODE_4312x4312_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4312, 4312), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 13: 4688x3516@30fps_crop */
	FIMC_IS_SENSOR_CFG(4688, 3516, 30, 0, MODE_4688x3516_30, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4688, 3516), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 9: 2880x2156@30fps_3dhdr */
	/* FIMC_IS_SENSOR_CFG_EX(2880, 2156, 30, 0, MODE_2880x2156_30_3DHDR, CSI_DATA_LANES_4, 2093, CSI_MODE_DT_ONLY, PD_NONE, EX_3DHDR,
		VC_IN(0, HW_FORMAT_RAW10, 2880, 2156), VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_EMBEDDED_8BIT, 0, 0), VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(0, HW_FORMAT_USER, 2880, 28), VC_OUT(HW_FORMAT_USER, VC_EMBEDDED, 2880, 28)), */
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

#if 0
static int sensor_module_2x5_power_setpin_with_af(struct device *dev,
	struct exynos_platform_fimc_is_module *pdata)
{
	struct device_node *dnode;
	int gpio_reset = 0;
	int gpio_mclk = 0;
	int gpio_none = 0;
	int gpio_core_en = 0;
	int gpio_cam_avdd_en = 0;
	int gpio_cam_af_en = 0;
	int gpio_cam_io_en = 0;

#if defined (CONFIG_OIS_USE)
	int gpio_ois_reset = 0;
	int gpio_ois_core_en = 0;
	int gpio_ois_io_en = 0;
#endif

	BUG_ON(!dev);

	dnode = dev->of_node;

	dev_info(dev, "%s E v4\n", __func__);

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	gpio_core_en = of_get_named_gpio(dnode, "gpio_core_en", 0);
	if (!gpio_is_valid(gpio_core_en)) {
		dev_err(dev, "failed to get gpio_core_en\n");
	} else {
		gpio_request_one(gpio_core_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_core_en);
	}

	gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
	if (!gpio_is_valid(gpio_mclk)) {
		dev_err(dev, "failed to get gpio_mclk\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW");
		gpio_free(gpio_mclk);
	}

	gpio_cam_avdd_en = of_get_named_gpio(dnode, "gpio_cam_avdd_en", 0);
	if (!gpio_is_valid(gpio_cam_avdd_en)) {
		dev_err(dev, "failed to get gpio_cam_avdd_en\n");
	} else {
		gpio_request_one(gpio_cam_avdd_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_cam_avdd_en);
	}

	gpio_cam_af_en = of_get_named_gpio(dnode, "gpio_cam_af_en", 0);
	if (!gpio_is_valid(gpio_cam_af_en)) {
		dev_err(dev,"failed to get gpio_cam_af_en\n");
	} else {
		gpio_request_one(gpio_cam_af_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_cam_af_en);
	}

	gpio_cam_io_en = of_get_named_gpio(dnode, "gpio_cam_io_en", 0);
	if (!gpio_is_valid(gpio_cam_io_en)) {
		dev_err(dev, "failed to get gpio_cam_io_en\n");
	} else {
		gpio_request_one(gpio_cam_io_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_cam_io_en);
	}

#if defined (CONFIG_OIS_USE)
	gpio_ois_reset = of_get_named_gpio(dnode, "gpio_ois_reset", 0);
	if (!gpio_is_valid(gpio_ois_reset)) {
		dev_err(dev, "failed to get gpio_ois_reset\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_ois_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_ois_reset);
	}

	gpio_ois_core_en = of_get_named_gpio(dnode, "gpio_ois_core_en", 0);
	if (!gpio_is_valid(gpio_ois_core_en)) {
		dev_err(dev, "failed to get gpio_ois_core_en\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_ois_core_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_ois_core_en);
	}

	gpio_ois_io_en = of_get_named_gpio(dnode, "gpio_ois_io_en", 0);
	if (!gpio_is_valid(gpio_ois_io_en)) {
		dev_err(dev, "failed to get gpio_ois_io_en\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_ois_io_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_ois_io_en);
	}
#endif

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);

#if defined (CONFIG_OIS_USE)
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF);
#endif

	/* BACK CAMERA - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "pdaf sen_rst low", PIN_OUTPUT, 0, 0);

	if (gpio_is_valid(gpio_cam_avdd_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_cam_avdd_en, "pdaf gpio_cam_avdd_en", PIN_OUTPUT, 1, 200);
	} else {
#if !defined (CONFIG_SOC_EXYNOS7885)
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDA_2.8V_CAM", PIN_REGULATOR, 1, 200);
#endif
	}

#if defined (CONFIG_OIS_USE)
	if (gpio_is_valid(gpio_ois_core_en)) {  /* VDD_OIS_2P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_ois_core_en, "pdaf gpio_ois_core_en", PIN_OUTPUT, 1, 0);
	}
	if (gpio_is_valid(gpio_ois_io_en)) {  /* VDD_OIS_IO_1P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_ois_io_en, "pdaf gpio_ois_io_en", PIN_OUTPUT, 1, 2000);
	}
#endif

	if (gpio_is_valid(gpio_core_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_core_en, "pdaf gpio_core_en", PIN_OUTPUT, 1, 200);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 1, 200);
	}

	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 1, 7000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 1, 7000);
	}

	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 1, 3000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 3000);
	}

#if defined (CONFIG_OIS_USE)
	if (gpio_is_valid(gpio_ois_reset)) {  /* OIS_RESET */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_ois_reset, "pdaf gpio_ois_reset", PIN_OUTPUT, 1, 0);
	}
#endif

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "pdaf sen_rst high", PIN_OUTPUT, 1, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 2, 3000);

	/* BACK CAEMRA - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin_none", PIN_NONE, 1, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, "pdaf sen_rst", PIN_OUTPUT, 0, 2000);

	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 0, 200);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 0, 200);
	}

#if defined (CONFIG_OIS_USE)
	if (gpio_is_valid(gpio_ois_reset)) {  /* OIS_RESET */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_ois_reset, "pdaf gpio_ois_reset", PIN_OUTPUT, 0, 0);
	}
#endif

	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 0, 100);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 100);
	}

#if defined (CONFIG_OIS_USE)
	if (gpio_is_valid(gpio_ois_core_en)) {  /* VDD_OIS_2P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_ois_core_en, "pdaf gpio_ois_core_en", PIN_OUTPUT, 0, 0);
	}
	if (gpio_is_valid(gpio_ois_io_en)) {  /* VDD_OIS_IO_1P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_ois_io_en, "pdaf gpio_ois_io_en", PIN_OUTPUT, 0, 0);
	}
#endif

	if (gpio_is_valid(gpio_core_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_core_en, "pdaf gpio_core_en", PIN_OUTPUT, 0, 100);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 0, 100);
	}

	if (gpio_is_valid(gpio_cam_avdd_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_cam_avdd_en, "pdaf gpio_cam_avdd_en", PIN_OUTPUT, 0, 100);
	} else {
#if !defined (CONFIG_SOC_EXYNOS7885)
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDA_2.8V_CAM", PIN_REGULATOR, 0, 100);
#endif
	}

	/* READ_ROM - POWER ON */
#if defined(USE_AF_PWR_READ_EEPROM)
	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 1, 2000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 1, 2000);
	}
#endif
	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 1, 2000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 2000);
	}

#if defined (CONFIG_OIS_USE)
	if (gpio_is_valid(gpio_ois_core_en)) {  /* VDD_OIS_2P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_ois_core_en, "pdaf gpio_ois_core_en", PIN_OUTPUT, 1, 0);
	}
	if (gpio_is_valid(gpio_ois_io_en)) {  /* VDD_OIS_IO_1P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_ois_io_en, "pdaf gpio_ois_io_en", PIN_OUTPUT, 1, 2000);
	}
	if (gpio_is_valid(gpio_ois_reset)) {  /* OIS_RESET */
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_ois_reset, "pdaf gpio_ois_reset", PIN_OUTPUT, 1, 2000);
	}
#endif

	/* READ_ROM - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, "pin_none", PIN_NONE, 1, 500);

#if defined(USE_AF_PWR_READ_EEPROM)
	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 0, 10);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 0, 10);
	}
#endif
	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 0, 0);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 0);
	}

#if defined (CONFIG_OIS_USE)
	if (gpio_is_valid(gpio_ois_reset)) {  /* OIS_RESET */
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_ois_reset, "pdaf gpio_ois_reset", PIN_OUTPUT, 0, 10);
	}
	if (gpio_is_valid(gpio_ois_core_en)) {  /* VDD_OIS_2P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_ois_core_en, "pdaf gpio_ois_core_en", PIN_OUTPUT, 0, 0);
	}
	if (gpio_is_valid(gpio_ois_io_en)) {  /* VDD_OIS_IO_1P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_ois_io_en, "pdaf gpio_ois_io_en", PIN_OUTPUT, 0, 10);
	}
#endif

#if defined(CONFIG_OIS_USE)
	/* BACK CAMERA - OIS FACTORY ON */
	if (gpio_is_valid(gpio_ois_core_en)) {  /* VDD_OIS_2P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, gpio_ois_core_en, "pdaf gpio_ois_core_en", PIN_OUTPUT, 1, 0);
	}

	if (gpio_is_valid(gpio_ois_io_en)) {  /* VDD_OIS_IO_1P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, gpio_ois_io_en, "pdaf gpio_ois_io_en", PIN_OUTPUT, 1, 2000);
	}

	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 1, 2000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 1, 2000);
	}

	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 1, 2000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 2000);
	}

	if (gpio_is_valid(gpio_ois_reset)) {  /* OIS_RESET */
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, gpio_ois_reset, "pdaf gpio_ois_reset", PIN_OUTPUT, 1, 2000);
	}

	/* BACK CAMERA - OIS FACTORY OFF */
	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 0, 10);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 0, 10);
	}

	if (gpio_is_valid(gpio_ois_reset)) {  /* OIS_RESET */
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, gpio_ois_reset, "pdaf gpio_ois_reset", PIN_OUTPUT, 0, 10);
	}

	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 0, 2000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 2000);
	}

	if (gpio_is_valid(gpio_ois_core_en)) {  /* VDD_OIS_2P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, gpio_ois_core_en, "pdaf gpio_ois_core_en", PIN_OUTPUT, 0, 0);
	}
	if (gpio_is_valid(gpio_ois_io_en)) {  /* VDD_OIS_IO_1P8 */
		SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, gpio_ois_io_en, "pdaf gpio_ois_io_en", PIN_OUTPUT, 0, 10);
	}
#endif

#if defined(CONFIG_CAMERA_JACKPOT)|| defined(CONFIG_CAMERA_JACKPOT_JPN)
	/* VISION - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_reset, "pdaf sen_rst low", PIN_OUTPUT, 0, 0);

	if (gpio_is_valid(gpio_cam_avdd_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_cam_avdd_en, "pdaf gpio_cam_avdd_en", PIN_OUTPUT, 1, 200);
	} else {
#if !defined (CONFIG_SOC_EXYNOS7885)
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "VDDA_2.8V_CAM", PIN_REGULATOR, 1, 200);
#endif
	}

	if (gpio_is_valid(gpio_core_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_core_en, "pdaf gpio_core_en", PIN_OUTPUT, 1, 200);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 1, 200);
	}

	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 1, 7000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 1, 7000);
	}

	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 1, 3000);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 3000);
	}

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_reset, "pdaf sen_rst high", PIN_OUTPUT, 1, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 2, 3000);

	/* VISION - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "pin_none", PIN_NONE, 1, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_reset, "pdaf sen_rst", PIN_OUTPUT, 0, 2000);

	if (gpio_is_valid(gpio_cam_af_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_cam_af_en, "pdaf gpio_cam_af_en", PIN_OUTPUT, 0, 200);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "VDD_CAM_AF_2P8", PIN_REGULATOR, 0, 200);
	}

	if (gpio_is_valid(gpio_cam_io_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_cam_io_en, "pdaf gpio_cam_io_en", PIN_OUTPUT, 0, 100);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 100);
	}

	if (gpio_is_valid(gpio_core_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_core_en, "pdaf gpio_core_en", PIN_OUTPUT, 0, 100);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 0, 100);
	}

	if (gpio_is_valid(gpio_cam_avdd_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_cam_avdd_en, "pdaf gpio_cam_avdd_en", PIN_OUTPUT, 0, 100);
	} else {
#if !defined (CONFIG_SOC_EXYNOS7885)
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "VDDA_2.8V_CAM", PIN_REGULATOR, 0, 100);
#endif
	}
#endif

	dev_info(dev, "%s X v4\n", __func__);

	return 0;
}
#endif

static int sensor_module_2x5_power_setpin(struct device *dev,
	struct exynos_platform_fimc_is_module *pdata)
{
	struct device_node *dnode;
	int gpio_reset = 0;
	int gpio_mclk = 0;
	int gpio_none = 0;

	BUG_ON(!dev);

	dnode = dev->of_node;

	dev_info(dev, "%s E v0\n", __func__);

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
	if (!gpio_is_valid(gpio_mclk)) {
		dev_err(dev, "failed to get gpio_mclk\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW");
		gpio_free(gpio_mclk);
	}

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);

	/* Normal on */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "sen_rst low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "FCAM_AVDD_2P8", PIN_REGULATOR, 1, 0); /* FCAM_AVDD_2P8 */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "FCAM_DVDD_1P05", PIN_REGULATOR, 1, 0); /* FCAM_DVDD_1P05 */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "CAM_VDDIO_1P8", PIN_REGULATOR, 1, 0); /* CAM_VDDIO_1P8 */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "sen_rst high", PIN_OUTPUT, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 2, 7100);

	/* Normal off */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, "sen_rst", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "FCAM_AVDD_2P8", PIN_REGULATOR, 0, 0); /* FCAM_AVDD_2P8 */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "FCAM_DVDD_1P05", PIN_REGULATOR, 0, 0); /* RCAM1_DVDD_ 1P05 */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "CAM_VDDIO_1P8", PIN_REGULATOR, 0, 0); /* CAM_VDDIO_1P8 */

	/* READ_ROM - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON, gpio_none, "CAM_VDDIO_1P8", PIN_REGULATOR, 1, 5000); /* CAM_VDDIO_1P8 */
	/* READ_ROM - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, "CAM_VDDIO_1P8", PIN_REGULATOR, 0, 0); /* CAM_VDDIO_1P8 */

	dev_info(dev, "%s X v0\n", __func__);

	return 0;
}

/*
static int (* module_2x5_power_setpin[MAX_2X5_SETPIN_CNT])(struct device *pdev,
	struct exynos_platform_fimc_is_module *pdata) = {
	sensor_module_2x5_power_setpin,
	sensor_module_2x5_power_setpin_with_af
}; */

int sensor_module_2x5_probe(struct platform_device *pdev)
{
	int ret = 0;
	bool use_pdaf = false;
	u8 exist_actuator = 0;
	int ch, t;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	struct exynos_platform_fimc_is_module *pdata;
	struct device *dev;
	struct device_node *af_np;
	struct pinctrl_state *s;

	BUG_ON(!fimc_is_dev);

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

	af_np = of_find_node_by_name(dev->of_node, "af");
	if (!af_np)
		exist_actuator = SENSOR_MODULE_2X5_WITHOUT_ACTUATOR;
	else
		exist_actuator = SENSOR_MODULE_2X5_WITH_ACTUATOR;

	probe_info("%s exist_actuator(%d)\n", __func__, exist_actuator);

	fimc_is_module_parse_dt(dev, sensor_module_2x5_power_setpin);

	pdata = dev_get_platdata(dev);
	device = &core->sensor[pdata->id];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		probe_err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	probe_info("%s pdta->id(%d), module_enum id = %d \n", __func__, pdata->id, atomic_read(&device->module_count));
	module = &device->module_enum[atomic_read(&device->module_count)];
	atomic_inc(&device->module_count);
	clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->dev = dev;
	module->sensor_id = SENSOR_NAME_S5K2X5;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = SENSOR_2X5_MAX_WIDTH;
	module->active_height = SENSOR_2X5_MAX_HEIGHT;
	module->margin_left = 0;
	module->margin_right = 0;
	module->margin_top = 0;
	module->margin_bottom = 0;
	module->pixel_width = module->active_width;
	module->pixel_height = module->active_height;
	module->max_framerate = 120;
	module->position = pdata->position;
	module->bitwidth = 10;
	module->sensor_maker = "SLSI";
	if (use_pdaf == true) {
		module->sensor_name = "S5K2X5SX"; /* pdaf sensor */
	} else {
		module->sensor_name = "S5K2X5"; /* default */
	}
#if 0
	if (pdata->position == SENSOR_MODULE_2X5_REAR) {
		module->setfile_name = "setfile_2x5.bin";
	} else if (pdata->position == SENSOR_MODULE_2X5_FRONT) {
		module->setfile_name = "setfile_2x5_front.bin";
	}
#else
	module->setfile_name = "setfile_2x5.bin";
#endif
	module->cfgs = ARRAY_SIZE(config_module_2x5);
	module->cfg = config_module_2x5;
	module->ops = NULL;

	for (ch = 1; ch < CSI_VIRTUAL_CH_MAX; ch++)
		module->vc_buffer_offset[ch] = pdata->vc_buffer_offset[ch];

	for (t = VC_BUF_DATA_TYPE_SENSOR_STAT1; t < VC_BUF_DATA_TYPE_MAX; t++) {
		module->vc_extra_info[t].stat_type = VC_STAT_TYPE_INVALID;
		module->vc_extra_info[t].sensor_mode = VC_SENSOR_MODE_INVALID;
		module->vc_extra_info[t].max_width = 0;
		module->vc_extra_info[t].max_height = 0;
		module->vc_extra_info[t].max_element = 0;

		switch (t) {
		case VC_BUF_DATA_TYPE_SENSOR_STAT2:
			module->vc_extra_info[t].stat_type
				= VC_STAT_TYPE_TAIL_FOR_3HDR_LSI;

			module->vc_extra_info[t].sensor_mode = VC_SENSOR_MODE_3HDR_LSI;
			module->vc_extra_info[t].max_width = 2880;
			module->vc_extra_info[t].max_height = 28;
			module->vc_extra_info[t].max_element = 1;
			break;
		}
	}

	/* Sensor peri */
	module->private_data = kzalloc(sizeof(struct fimc_is_device_sensor_peri), GFP_KERNEL);
	if (!module->private_data) {
		probe_err("fimc_is_device_sensor_peri is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	fimc_is_sensor_peri_probe((struct fimc_is_device_sensor_peri*)module->private_data);
	PERI_SET_MODULE(module);

	ext = &module->ext;

	ext->sensor_con.product_name = module->sensor_id;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
	ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

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

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

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
		ext->preprocessor_con.peri_info2.peri_setting.dma.channel = FLITE_ID_D;
	} else {
		ext->preprocessor_con.product_name = pdata->preprocessor_product_name;
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

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static int sensor_module_2x5_remove(struct platform_device *pdev)
{
        int ret = 0;

        info("%s\n", __func__);

        return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_module_2x5_match[] = {
	{
		.compatible = "samsung,sensor-module-2x5",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_module_2x5_match);

static struct platform_driver sensor_module_2x5_driver = {
	.probe  = sensor_module_2x5_probe,
	.remove = sensor_module_2x5_remove,
	.driver = {
		.name   = "FIMC-IS-SENSOR-MODULE-2X5",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_module_2x5_match,
	}
};

module_platform_driver(sensor_module_2x5_driver);
