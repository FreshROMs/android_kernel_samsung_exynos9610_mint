/*
 * Samsung Exynos5 SoC series Flash driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "fimc-is-flash.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-core.h"

#ifdef CONFIG_LEDS_SM5713
#include <linux/mfd/sm5713.h>
extern int32_t sm5713_fled_mode_ctrl(u8 fled_index, int state);
#endif

static int flash_sm5713_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_flash *flash;

	FIMC_BUG(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!flash);

	/* TODO: init flash driver */
	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */
	flash->flash_data.flash_fired = false;

	gpio_request_one(flash->flash_gpio, GPIOF_OUT_INIT_LOW, "CAM_FLASH_GPIO_OUTPUT");
	gpio_free(flash->flash_gpio);
	gpio_request_one(flash->torch_gpio, GPIOF_OUT_INIT_LOW, "CAM_TORCH_GPIO_OUTPUT");
	gpio_free(flash->torch_gpio);

	return ret;
}

static int sensor_sm5713_flash_control(struct v4l2_subdev *subdev, enum flash_mode mode, u32 intensity)
{
	int ret = 0;
	struct fimc_is_flash *flash = NULL;

	FIMC_BUG(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	info("%s : mode = %s, intensity = %d\n", __func__,
		mode == CAM2_FLASH_MODE_OFF ? "OFF" :
		mode == CAM2_FLASH_MODE_SINGLE ? "FLASH" : "TORCH",
		intensity);

#ifdef CONFIG_LEDS_SM5713
	if (mode == CAM2_FLASH_MODE_OFF) {
		ret = sm5713_fled_mode_ctrl(SM5713_FLED_INDEX_1, SM5713_FLED_MODE_OFF);
		if (ret)
			err("%s off fail",__func__);
	} else if (mode == CAM2_FLASH_MODE_SINGLE) {
		ret = sm5713_fled_mode_ctrl(SM5713_FLED_INDEX_1, SM5713_FLED_MODE_MAIN_FLASH);
		if (ret)
			err("%s capture flash on fail",__func__);
	} else if (mode == CAM2_FLASH_MODE_TORCH) {
		ret = sm5713_fled_mode_ctrl(SM5713_FLED_INDEX_1, SM5713_FLED_MODE_PRE_FLASH);
		if (ret)
			err("%s torch flash on fail",__func__);
	} else {
		err("Invalid flash mode");
		ret = control_flash_gpio(flash->flash_gpio, 0);
		ret = control_flash_gpio(flash->torch_gpio, 0);
		ret = -EINVAL;
		goto p_err;
	}
#else
	if (mode == CAM2_FLASH_MODE_OFF) {
		ret = control_flash_gpio(flash->flash_gpio, 0);
		if (ret)
			err("capture flash off fail");
		ret = control_flash_gpio(flash->torch_gpio, 0);
		if (ret)
			err("torch flash off fail");
	} else if (mode == CAM2_FLASH_MODE_SINGLE) {
		ret = control_flash_gpio(flash->flash_gpio, intensity);
		if (ret)
			err("capture flash on fail");
	} else if (mode == CAM2_FLASH_MODE_TORCH) {
		ret = control_flash_gpio(flash->torch_gpio, intensity);
		if (ret)
			err("torch flash on fail");
	} else {
		err("Invalid flash mode");
		ret = -EINVAL;
		goto p_err;
	}
#endif

p_err:
	return ret;
}

int flash_sm5713_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_flash *flash = NULL;

	FIMC_BUG(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	switch(ctrl->id) {
	case V4L2_CID_FLASH_SET_INTENSITY:
		/* TODO : Check min/max intensity */
		if (ctrl->value < 0) {
			err("failed to flash set intensity: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.intensity = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRING_TIME:
		/* TODO : Check min/max firing time */
		if (ctrl->value < 0) {
			err("failed to flash set firing time: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.firing_time_us = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRE:
		ret =  sensor_sm5713_flash_control(subdev, flash->flash_data.mode, ctrl->value);
		if (ret) {
			err("sensor_sm5713_flash_control(mode:%d, val:%d) is fail(%d)",
					(int)flash->flash_data.mode, ctrl->value, ret);
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = flash_sm5713_init,
	.s_ctrl = flash_sm5713_s_ctrl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int __init flash_sm5713_probe(struct device *dev, struct i2c_client *client)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_flash;
	struct fimc_is_device_sensor *device;
	struct fimc_is_flash *flash;
	struct device_node *dnode;
	const u32 *sensor_id_spec;
	u32 sensor_id_len;
	u32 sensor_id[FIMC_IS_SENSOR_COUNT];
	int i;

	FIMC_BUG(!fimc_is_dev);
	FIMC_BUG(!dev);

	dnode = dev->of_node;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		if (!device) {
			err("sensor device is NULL");
			ret = -EPROBE_DEFER;
			goto p_err;
		}
	}

	flash = kzalloc(sizeof(struct fimc_is_flash) * sensor_id_len, GFP_KERNEL);
	if (!flash) {
		err("flash is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_flash = kzalloc(sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_flash) {
		err("subdev_flash is NULL");
		ret = -ENOMEM;
		kfree(flash);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);
		flash[i].id = FLADRV_NAME_DRV_FLASH_GPIO;
		flash[i].subdev = &subdev_flash[i];
		flash[i].client = client;

		flash[i].flash_gpio = of_get_named_gpio(dnode, "flash-gpio", 0);
		if (!gpio_is_valid(flash[i].flash_gpio)) {
			dev_err(dev, "failed to get PIN_RESET\n");
			kfree(flash);
			kfree(subdev_flash);
			return -EINVAL;
		} else {
			gpio_request_one(flash[i].flash_gpio, GPIOF_OUT_INIT_LOW, "CAM_FLASH_OUTPUT");
			gpio_free(flash[i].flash_gpio);
		}

		flash[i].torch_gpio = of_get_named_gpio(dnode, "torch-gpio", 0);
		if (!gpio_is_valid(flash[i].torch_gpio)) {
			dev_err(dev, "failed to get PIN_RESET\n");
			kfree(flash);
			kfree(subdev_flash);
			return -EINVAL;
		} else {
			gpio_request_one(flash[i].torch_gpio, GPIOF_OUT_INIT_LOW, "CAM_TORCH_OUTPUT");
			gpio_free(flash[i].torch_gpio);
		}

		flash[i].flash_data.mode = CAM2_FLASH_MODE_OFF;
		flash[i].flash_data.intensity = 255; /* TODO: Need to figure out min/max range */
		flash[i].flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */

		device = &core->sensor[sensor_id[i]];
		device->subdev_flash = &subdev_flash[i];
		device->flash = &flash[i];

		if (client)
			v4l2_i2c_subdev_init(&subdev_flash[i], client, &subdev_ops);
		else
			v4l2_subdev_init(&subdev_flash[i], &subdev_ops);

		v4l2_set_subdevdata(&subdev_flash[i], &flash[i]);
		v4l2_set_subdev_hostdata(&subdev_flash[i], device);
		snprintf(subdev_flash[i].name, V4L2_SUBDEV_NAME_SIZE,
					"flash-subdev.%d", flash[i].id);

		probe_info("%s done\n", __func__);
	}

p_err:
	return ret;
}

static int __init flash_sm5713_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	FIMC_BUG(!pdev);

	dev = &pdev->dev;

	ret = flash_sm5713_probe(dev, NULL);
	if (ret < 0) {
		probe_err("flash sm5713 probe fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_flash_sm5713_match[] = {
	{
		.compatible = "samsung,sensor-flash-sm5713",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_flash_sm5713_match);

/* register platform driver */
static struct platform_driver sensor_flash_sm5713_platform_driver = {
	.driver = {
		.name   = "FIMC-IS-SENSOR-FLASH-SM5713-PLATFORM",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_flash_sm5713_match,
	}
};

static int __init sensor_flash_sm5713_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_flash_sm5713_platform_driver,
				flash_sm5713_platform_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_flash_sm5713_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_flash_sm5713_init);
