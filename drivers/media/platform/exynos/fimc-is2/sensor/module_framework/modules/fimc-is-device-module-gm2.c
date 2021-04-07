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

#define SENSOR_S5KGM2_NAME		SENSOR_NAME_S5KGM2

#ifndef USE_VENDOR_PWR_PIN_NAME
#define RCAM_AF_VDD      "vdd_ldo37"       /* RCAM1_AFVDD_2P8 */
#define S5KGM2_IOVDD     "CAM_VLDO3"       /* CAM_VDDIO_1P8 : CAM_VLDO3 is used for all camera commonly */
#define S5KGM2_AVDD      "CAM_VLDO6"       /* RCAM1_AVDD1_2P9 */
#define S5KGM2_DVDD      "CAM_VLDO1"       /* RCAM1_DVDD_1P05 */
#endif

/*
 * [Mode Information]
 *
 * Reference File : S5KGM2SP_EVT0_Setfile_Ver1.4_190729.xlsx
 * Update Data    : 2019-07-30
 * Author         : kmin.kim
 *
 * - Remosaic Full For Single Still Remosaic Capture -
 *    [  0 ] FULL : Remosaic Full 8000x6000 10fps       : Single Still Remosaic Capture (4:3)
 *
 * - 2x2 BIN For Still Preview / Capture -
 *    [  1 ] 2SUM2BIN : 2 Binning Full 4000x3000 30fps    : Single Still Preview/Capture (4:3)
 *    [  2 ] 2SUM2BIN : 2 Binning Crop 4000X2256 30fps    : Single Still Preview/Capture (16:9)
 *    [  3 ] 2SUM2BIN : 2 Binning Crop 4000X1844 30fps    : Single Still Preview/Capture (19.5:9)
 *    [  4 ] 2SUM2BIN : 2 Binning Crop 3680X3000 30fps    : MMS (11:9)
 *    [  5 ] 2SUM2BIN : 2 Binning Crop 3000X3000 30fps    : Single Still Preview/Capture (1:1)
 *    [  6 ] 4SUM4BIN : 4 Binning Full 2000X1500 94fps    : Fast-AE (2000x1500@94fps)
 *    [  7 ] 4SUM4BIN : 4 Binning Crop 2000X1128 120fps   : SlowMotion (1280x720@120fps)
 *    [  8 ] 4SUM4BIN : 4 Binning Crop 1920X1080 240fps   : SlowMotion (1280x720@240fps)
 *
 */

static struct fimc_is_sensor_cfg config_gm2[] = {
	/* Remosaic Full For Single Still Remosaic Capture */
	FIMC_IS_SENSOR_CFG_EX(8000, 6000,  10, 0, 0, CSI_DATA_LANES_4, 2054, CSI_MODE_VC_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 8000, 6000),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 2SUM2BIN Full For Single Still Preview / Capture */
	FIMC_IS_SENSOR_CFG_EX(4000, 3000,  30, 0, 1, CSI_DATA_LANES_4, 1352, CSI_MODE_VC_ONLY, PD_MSPD_TAIL, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4000, 3000),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_RAW10,  500, 2976),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_MIPISTAT, 500, 2976),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_PRIVATE, 500, 2976)),
	/* 2SUM2BIN Crop For Single Still Preview / Capture */
	FIMC_IS_SENSOR_CFG_EX(4000, 2256,  30, 0, 2, CSI_DATA_LANES_4, 1352, CSI_MODE_VC_ONLY, PD_MSPD_TAIL, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4000, 2256),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_RAW10,  500, 2240),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_MIPISTAT, 500, 2240),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_PRIVATE, 500, 2240)),
	/* 2SUM2BIN Crop For Single Still Preview / Capture */
	FIMC_IS_SENSOR_CFG_EX(4000, 1844,  30, 0, 3, CSI_DATA_LANES_4, 1352, CSI_MODE_VC_ONLY, PD_MSPD_TAIL, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 4000, 1844),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_RAW10,  500, 1824),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_MIPISTAT, 500, 1824),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_PRIVATE, 500, 1824)),
	/* 2SUM2BIN Crop For Single Still Preview / Capture */
	FIMC_IS_SENSOR_CFG_EX(3680, 3000,  30, 0, 4, CSI_DATA_LANES_4, 1352, CSI_MODE_VC_ONLY, PD_MSPD_TAIL, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3680, 3000),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_RAW10,  460, 2976),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_MIPISTAT, 460, 2976),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_PRIVATE, 460, 2976)),
	/* 2SUM2BIN Crop For Single Still Preview / Capture */
	FIMC_IS_SENSOR_CFG_EX(3000, 3000,  30, 0, 5, CSI_DATA_LANES_4, 1352, CSI_MODE_VC_ONLY, PD_MSPD_TAIL, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 3000, 3000),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_RAW10,  376, 2976),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_MIPISTAT, 376, 2976),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_USER, VC_PRIVATE, 376, 2976)),
	/* 4SUM4BIN Full For Fast-AE (2000x1500@94fps) */
	FIMC_IS_SENSOR_CFG_EX(2000, 1500,  94, 0, 6, CSI_DATA_LANES_4, 1352, CSI_MODE_VC_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2000, 1500),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 4SUM4BIN Crop For SlowMotion (1280x720@120fps) */
	FIMC_IS_SENSOR_CFG_EX(2000, 1128,  120, 0, 7, CSI_DATA_LANES_4, 1352, CSI_MODE_VC_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 2000, 1128),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(3, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0)),
	/* 4SUM4BIN Crop For SlowMotion (1280x720@240fps) */
	FIMC_IS_SENSOR_CFG_EX(1920, 1080,  240, 0, 8, CSI_DATA_LANES_4, 2054, CSI_MODE_VC_ONLY, PD_NONE, EX_NONE,
		VC_IN(0, HW_FORMAT_RAW10, 1920, 1080),   VC_OUT(HW_FORMAT_RAW10, VC_NOTHING, 0, 0),
		VC_IN(1, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
		VC_IN(2, HW_FORMAT_UNKNOWN, 0, 0),       VC_OUT(HW_FORMAT_UNKNOWN, VC_NOTHING, 0, 0),
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

static int sensor_gm2_power_setpin(struct device *dev,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct fimc_is_core *core;
	struct device_node *dnode = dev->of_node;
	int gpio_mclk = 0;
	int gpio_reset = 0;
	int gpio_none = 0;
	bool shared_mclk = false;

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

	shared_mclk = of_property_read_bool(dnode, "shared_mclk");

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);

	/* Normal on */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_reset, SENSOR_RESET_LOW,  PIN_OUTPUT,    0, 500);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  S5KGM2_AVDD,       PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  S5KGM2_IOVDD,      PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  S5KGM2_DVDD,       PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  RCAM_AF_VDD,       PIN_REGULATOR, 1, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  SENSOR_MCLK_PIN,   PIN_FUNCTION,  1, 1000);
	if (shared_mclk) {
		SET_PIN_SHARED(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, SRT_ACQUIRE,
				&core->shared_rsc_slock[SHARED_PIN0], &core->shared_rsc_count[SHARED_PIN0], 1);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_reset, SENSOR_RESET_HIGH, PIN_OUTPUT,    1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,  gpio_none,  SENSOR_SET_DELAY,  PIN_NONE,      0, 5000);

	/* Normal off */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  SENSOR_SET_DELAY,  PIN_NONE,      0, 5000);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, SENSOR_RESET_LOW,  PIN_OUTPUT,    0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  SENSOR_MCLK_PIN,   PIN_FUNCTION,  0, 0);
	if(shared_mclk) {
		SET_PIN_SHARED(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, SRT_RELEASE,
				&core->shared_rsc_slock[SHARED_PIN0], &core->shared_rsc_count[SHARED_PIN0], 0);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  SENSOR_MCLK_PIN,   PIN_FUNCTION,  2, 1000);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  RCAM_AF_VDD,       PIN_REGULATOR, 0, 500);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  S5KGM2_AVDD,       PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  S5KGM2_IOVDD,      PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none,  S5KGM2_DVDD,       PIN_REGULATOR, 0, 0);

	/* READ_ROM - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON,  gpio_none, RCAM_AF_VDD,       PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON,  gpio_none, S5KGM2_IOVDD,      PIN_REGULATOR, 1, 5000);
	
	/* READ_ROM - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, RCAM_AF_VDD,       PIN_REGULATOR, 0, 5000);
	SET_PIN(pdata, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF, gpio_none, S5KGM2_IOVDD,      PIN_REGULATOR, 0, 0);

	dev_info(dev, "%s X v4\n", __func__);

	return 0;
}

static int __init sensor_module_gm2_probe(struct platform_device *pdev)
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
	int ch, t;

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

	fimc_is_module_parse_dt(dev, sensor_gm2_power_setpin);

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
	module->sensor_id = SENSOR_NAME_S5KGM2;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = 8000;
	module->active_height = 6000;
	module->margin_left = 0;
	module->margin_right = 0;
	module->margin_top = 0;
	module->margin_bottom = 0;
	module->pixel_width = module->active_width + 0;
	module->pixel_height = module->active_height + 0;
	module->max_framerate = 240;
	module->position = pdata->position;
	module->bitwidth = 10;
	module->sensor_maker = "SLSI";
	module->sensor_name = "GM2";
	module->setfile_name = "setfile_gm2.bin";
	module->cfgs = ARRAY_SIZE(config_gm2);
	module->cfg = config_gm2;
	module->ops = NULL;

	for (ch = 1; ch < CSI_VIRTUAL_CH_MAX; ch++)
		module->vc_buffer_offset[ch] = pdata->vc_buffer_offset[ch];
	
	for (t = VC_BUF_DATA_TYPE_SENSOR_STAT1; t < VC_BUF_DATA_TYPE_MAX; t++) {
		module->vc_extra_info[t].stat_type = VC_STAT_TYPE_INVALID;
		module->vc_extra_info[t].sensor_mode = VC_SENSOR_MODE_INVALID;
		module->vc_extra_info[t].max_width = 0;
		module->vc_extra_info[t].max_height = 0;
		module->vc_extra_info[t].max_element = 0;

		if (IS_ENABLED(CONFIG_CAMERA_PAFSTAT)) {
			switch (t) {
			case VC_BUF_DATA_TYPE_GENERAL_STAT1:
				module->vc_extra_info[t].stat_type
					= VC_STAT_TYPE_PAFSTAT_FLOATING;

				module->vc_extra_info[t].sensor_mode = VC_SENSOR_MODE_SUPER_PD_2_TAIL;
				module->vc_extra_info[t].max_width = 500;
				module->vc_extra_info[t].max_height = 2976;
				module->vc_extra_info[t].max_element = 2;
				break;
			case VC_BUF_DATA_TYPE_GENERAL_STAT2:
				module->vc_extra_info[t].stat_type
					= VC_STAT_TYPE_PAFSTAT_STATIC;

				module->vc_extra_info[t].sensor_mode = VC_SENSOR_MODE_SUPER_PD_2_TAIL;
				module->vc_extra_info[t].max_width = 500;
				module->vc_extra_info[t].max_height = 2976;
				module->vc_extra_info[t].max_element = 2;
				break;
			}
		}
	}

	/* Sensor peri */
	module->private_data = kzalloc(sizeof(struct fimc_is_device_sensor_peri), GFP_KERNEL);
	if (!module->private_data) {
		ret = -ENOMEM;
		goto p_err;
	}
	fimc_is_sensor_peri_probe((struct fimc_is_device_sensor_peri *)module->private_data);
	PERI_SET_MODULE(module);

	ext = &module->ext;
	ext->sensor_con.product_name = module->sensor_id /*SENSOR_NAME_S5KGM2*/;
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

static const struct of_device_id exynos_fimc_is_sensor_module_gm2_match[] = {
	{
		.compatible = "samsung,sensor-module-gm2",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_module_gm2_match);

static struct platform_driver sensor_module_gm2_driver = {
	.driver = {
		.name   = "FIMC-IS-SENSOR-MODULE-GM2",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_module_gm2_match,
	}
};

static int __init fimc_is_sensor_module_gm2_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_module_gm2_driver,
				sensor_module_gm2_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_module_gm2_driver.driver.name, ret);

	return ret;
}
late_initcall(fimc_is_sensor_module_gm2_init);

