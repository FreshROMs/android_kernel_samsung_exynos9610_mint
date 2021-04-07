/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <exynos-fimc-is-sensor.h>
#include "fimc-is-core.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-interface.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-ois.h"
#include "fimc-is-vender-specific.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "fimc-is-device-af.h"
#endif
#include <linux/pinctrl/pinctrl.h>
#if defined (CONFIG_OIS_USE_RUMBA_S4)
#include "fimc-is-device-ois_s4.h"
#elif defined (CONFIG_OIS_USE_RUMBA_SA)
#include "fimc-is-device-ois_sa.h"
#endif

#define FIMC_IS_OIS_DEV_NAME		"exynos-fimc-is-ois"
#define OIS_I2C_RETRY_COUNT	1

struct fimc_is_ois_info ois_minfo;
struct fimc_is_ois_info ois_pinfo;
struct fimc_is_ois_info ois_uinfo;
struct fimc_is_ois_exif ois_exif_data;

void fimc_is_ois_i2c_config(struct i2c_client *client, bool onoff)
{
	struct pinctrl *pinctrl_i2c = NULL;
	struct device *i2c_dev = NULL;
	struct fimc_is_device_ois *ois_device = NULL;

	if (client == NULL) {
		err("client is NULL.");
		return;
	}

	i2c_dev = client->dev.parent->parent;
	ois_device = i2c_get_clientdata(client);

	if (ois_device->ois_hsi2c_status != onoff) {
		info("%s : ois_hsi2c_stauts(%d),onoff(%d)\n",__func__,
			ois_device->ois_hsi2c_status, onoff);

		if (onoff) {
			pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
			if (IS_ERR_OR_NULL(pinctrl_i2c)) {
				printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
			} else {
				devm_pinctrl_put(pinctrl_i2c);
			}
		} else {
			pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
			if (IS_ERR_OR_NULL(pinctrl_i2c)) {
				printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
			} else {
				devm_pinctrl_put(pinctrl_i2c);
			}
		}
		ois_device->ois_hsi2c_status = onoff;
	}

}

int fimc_is_ois_i2c_read(struct i2c_client *client, u16 addr, u8 *data)
{
	int err;
	u8 txbuf[2], rxbuf[1];
	struct i2c_msg msg[2];

	*data = 0;
	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail err = %d\n", __func__, err);
		return -EIO;
	}

	*data = rxbuf[0];
	return 0;
}

int fimc_is_ois_i2c_write(struct i2c_client *client ,u16 addr, u8 data)
{
        int retries = OIS_I2C_RETRY_COUNT;
        int ret = 0, err = 0;
        u8 buf[3] = {0,};
        struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = 3,
                .buf    = buf,
        };

        buf[0] = (addr & 0xff00) >> 8;
        buf[1] = addr & 0xff;
        buf[2] = data;

#if 0
        info("%s : W(0x%02X%02X %02X)\n",__func__, buf[0], buf[1], buf[2]);
#endif

        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < OIS_I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, data, retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
        }

        return 0;
}

int fimc_is_ois_i2c_write_multi(struct i2c_client *client ,u16 addr, u8 *data, size_t size)
{
	int retries = OIS_I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	ulong i = 0;
	u8 buf[258] = {0,};
	struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = size,
                .buf    = buf,
	};

	buf[0] = (addr & 0xFF00) >> 8;
	buf[1] = addr & 0xFF;

	for (i = 0; i < size - 2; i++) {
	        buf[i + 2] = *(data + i);
	}
#if 0
        info("OISLOG %s : W(0x%02X%02X%02X)\n", __func__, buf[0], buf[1], buf[2]);
#endif
        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < OIS_I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, *data, retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
	}

        return 0;
}

int fimc_is_ois_i2c_read_multi(struct i2c_client *client, u16 addr, u8 *data, size_t size)
{
	int err;
	u8 rxbuf[256], txbuf[2];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail", __func__);
		return -EIO;
	}

	memcpy(data, rxbuf, size);
	return 0;
}

int fimc_is_ois_gpio_on(struct fimc_is_core *core)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int i = 0;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
#ifdef CAMERA_USE_OIS_EXT_CLK
	u32 id = core->preproc.pdata->id;
#endif

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], SENSOR_POSITION_REAR, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

#ifdef CAMERA_USE_OIS_EXT_CLK
	ret = fimc_is_sensor_mclk_on(&core->sensor[id], SENSOR_SCENARIO_OIS_FACTORY, module->pdata->mclk_ch);
	if (ret) {
		err("fimc_is_sensor_mclk_on is fail(%d)", ret);
		goto p_err;
	}
#endif

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ois_gpio_off(struct fimc_is_core *core)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_vender_specific *specific = core->vender.private_data;
	struct fimc_is_module_enum *module = NULL;
	int i = 0;
#ifdef CAMERA_USE_OIS_EXT_CLK
	u32 id = core->preproc.pdata->id;
#endif

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], SENSOR_POSITION_REAR, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

#ifdef CAMERA_USE_OIS_EXT_CLK
	ret = fimc_is_sensor_mclk_off(&core->sensor[id], SENSOR_SCENARIO_OIS_FACTORY, module->pdata->mclk_ch);
	if (ret) {
		err("fimc_is_sensor_mclk_off is fail(%d)", ret);
		goto p_err;
	}
#endif

p_err:
	return ret;
}

void fimc_is_ois_enable(struct fimc_is_core *core)
{
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	CALL_OISOPS(ois_device, ois_enable, core);
}

void fimc_is_ois_offset_test(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	CALL_OISOPS(ois_device, ois_offset_test, core, raw_data_x, raw_data_y);
}

void fimc_is_ois_get_offset_data(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	CALL_OISOPS(ois_device, ois_get_offset_data, core, raw_data_x, raw_data_y);
}

int fimc_is_ois_self_test(struct fimc_is_core *core)
{
	int ret = 0;
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	ret = CALL_OISOPS(ois_device, ois_self_test, core);

	return ret;
}

bool fimc_is_ois_diff_test(struct fimc_is_core *core, int *x_diff, int *y_diff)
{
	bool result = false;
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	result = CALL_OISOPS(ois_device, ois_diff_test, core, x_diff, y_diff);

	return result;
}

bool fimc_is_ois_auto_test(struct fimc_is_core *core,
		            int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y)

{
	bool result = false;
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	result = CALL_OISOPS(ois_device, ois_auto_test, core,
			threshold, x_result, y_result, sin_x, sin_y);

	return result;
}

u16 fimc_is_ois_calc_checksum(u8 *data, int size)
{
	int i = 0;
	u16 result = 0;

	for(i = 0; i < size; i += 2) {
		result = result + (0xFFFF & (((*(data + i + 1)) << 8) | (*(data + i))));
	}

	return result;
}

void fimc_is_ois_gyro_sleep(struct fimc_is_core *core)
{
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	CALL_OISOPS(ois_device, ois_gyro_sleep, core);
}

void fimc_is_ois_exif_data(struct fimc_is_core *core)
{
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	CALL_OISOPS(ois_device, ois_exif_data, core);
}

int fimc_is_ois_get_exif_data(struct fimc_is_ois_exif **exif_info)
{
	*exif_info = &ois_exif_data;
	return 0;
}

int fimc_is_ois_get_module_version(struct fimc_is_ois_info **minfo)
{
	*minfo = &ois_minfo;
	return 0;
}

int fimc_is_ois_get_phone_version(struct fimc_is_ois_info **pinfo)
{
	*pinfo = &ois_pinfo;
	return 0;
}

int fimc_is_ois_get_user_version(struct fimc_is_ois_info **uinfo)
{
	*uinfo = &ois_uinfo;
	return 0;
}

bool fimc_is_ois_version_compare(char *fw_ver1, char *fw_ver2, char *fw_ver3)
{
	if (fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_CORE_VERSION] != fw_ver2[FW_CORE_VERSION]) {
		return false;
	}

	if (fw_ver2[FW_GYRO_SENSOR] != fw_ver3[FW_GYRO_SENSOR]
		|| fw_ver2[FW_DRIVER_IC] != fw_ver3[FW_DRIVER_IC]
		|| fw_ver2[FW_CORE_VERSION] != fw_ver3[FW_CORE_VERSION]) {
		return false;
	}

	return true;
}

bool fimc_is_ois_version_compare_default(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_CORE_VERSION] != fw_ver2[FW_CORE_VERSION]) {
		return false;
	}

	return true;
}

u8 fimc_is_ois_read_status(struct fimc_is_core *core)
{
	u8 status = 0;
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	status = CALL_OISOPS(ois_device, ois_read_status, core);

	return status;
}

u8 fimc_is_ois_read_cal_checksum(struct fimc_is_core *core)
{
	u8 status = 0;
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	status = CALL_OISOPS(ois_device, ois_read_cal_checksum, core);

	return status;
}

void fimc_is_ois_fw_status(struct fimc_is_core *core)
{
	ois_minfo.checksum = fimc_is_ois_read_status(core);
	ois_minfo.caldata = fimc_is_ois_read_cal_checksum(core);

	return;
}

bool fimc_is_ois_crc_check(struct fimc_is_core *core, char *buf)
{
	u8 check_8[4] = {0, };
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_bin;
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);

	if (ois_device->not_crc_bin) {
		err("ois binary does not conatin crc checksum.\n");
		return false;
	}

	if (buf == NULL) {
		err("buf is NULL. CRC check failed.");
		return false;
	}

	buf32 = (u32 *)buf;

	memcpy(check_8, buf + OIS_BIN_LEN, 4);
	checksum_bin = (check_8[3] << 24) | (check_8[2] << 16) | (check_8[1] << 8) | (check_8[0]);

	checksum = (u32)getCRC((u16 *)&buf32[0], OIS_BIN_LEN, NULL, NULL);
	if (checksum != checksum_bin) {
		return false;
	} else {
		return true;
	}
}

bool fimc_is_ois_check_fw(struct fimc_is_core *core)
{
	bool ret = false;
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	ret = CALL_OISOPS(ois_device, ois_check_fw, core);

	return ret;
}

void fimc_is_ois_fw_update(struct fimc_is_core *core)
{
	struct fimc_is_device_ois *ois_device = NULL;

	ois_device = i2c_get_clientdata(core->client1);
	
	fimc_is_ois_gpio_on(core);
	msleep(150);
	CALL_OISOPS(ois_device, ois_fw_update, core);
	fimc_is_ois_gpio_off(core);

	return;
}

MODULE_DESCRIPTION("OIS driver for Rumba");
MODULE_AUTHOR("kyoungho yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
