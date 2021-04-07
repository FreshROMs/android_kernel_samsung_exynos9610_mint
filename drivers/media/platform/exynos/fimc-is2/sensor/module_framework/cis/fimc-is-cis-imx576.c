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
#include <linux/of_gpio.h>
#include <linux/syscalls.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-fimc-is-sensor.h>
#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-cis-imx576.h"
#include "fimc-is-cis-imx576-setA.h"
#include "fimc-is-cis-imx576-setB.h"

#include "fimc-is-helper-i2c.h"
#include "fimc-is-sec-define.h"

#include "interface/fimc-is-interface-library.h"

#define SENSOR_NAME "IMX576"
/* #define DEBUG_IMX576_PLL */

static const u32 *sensor_imx576_global;
static u32 sensor_imx576_global_size;
static const u32 **sensor_imx576_setfiles;
static const u32 *sensor_imx576_setfile_sizes;
static u32 sensor_imx576_max_setfile_num;
static const struct sensor_pll_info_compact **sensor_imx576_pllinfos;

static bool sensor_imx576_cal_write_flag;
static bool sensor_imx576J_PDAF_version;

extern struct fimc_is_lib_support gPtr_lib_support;

static void sensor_imx576_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	switch (mode) {
	case SENSOR_IMX576_2880X2156_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X1620_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X1332_2X2BIN_30FPS:
	case SENSOR_IMX576_2156X2156_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X2156_QBCHDR_30FPS:
	case SENSOR_IMX576_2880X1620_QBCHDR_30FPS:
	case SENSOR_IMX576_2880X1332_QBCHDR_30FPS:
	case SENSOR_IMX576_2156X2156_QBCHDR_30FPS:
	case SENSOR_IMX576_5760X4312_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_5760X3240_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_5760X2664_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_4312X4312_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_1872X1052_SSM_240FPS:
	case SENSOR_IMX576_2880X1620_SSM_120FPS:
	case SENSOR_IMX576_2880X2156_120FPS:
		cis_data->max_margin_coarse_integration_time = SENSOR_IMX576_COARSE_INTEGRATION_TIME_MAX_MARGIN;
		dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
			cis_data->max_margin_coarse_integration_time);
		break;
	default:
		err("[%s] Unsupport imx576 sensor mode\n", __func__);
		cis_data->max_margin_coarse_integration_time = SENSOR_IMX576_COARSE_INTEGRATION_TIME_MAX_MARGIN;
		dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
			cis_data->max_margin_coarse_integration_time);
		break;
	}
}

static void sensor_imx576_set_integration_min(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	switch (mode) {
	case SENSOR_IMX576_2880X2156_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X1620_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X1332_2X2BIN_30FPS:
	case SENSOR_IMX576_2156X2156_2X2BIN_30FPS:
		cis_data->min_coarse_integration_time = SENSOR_IMX576_COARSE_INTEGRATION_TIME_MIN;
		dbg_sensor(1, "min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	case SENSOR_IMX576_2880X2156_QBCHDR_30FPS:
	case SENSOR_IMX576_2880X1620_QBCHDR_30FPS:
	case SENSOR_IMX576_2880X1332_QBCHDR_30FPS:
	case SENSOR_IMX576_2156X2156_QBCHDR_30FPS:
		cis_data->min_coarse_integration_time = 0x06;
		dbg_sensor(1, "min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	case SENSOR_IMX576_5760X4312_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_5760X3240_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_5760X2664_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_4312X4312_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_1872X1052_SSM_240FPS:
	case SENSOR_IMX576_2880X1620_SSM_120FPS:
	case SENSOR_IMX576_2880X2156_120FPS:
		cis_data->min_coarse_integration_time = SENSOR_IMX576_COARSE_INTEGRATION_TIME_MIN;
		dbg_sensor(1, "min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	default:
		err("[%s] Unsupport imx576 sensor mode\n", __func__);
		cis_data->min_coarse_integration_time = SENSOR_IMX576_COARSE_INTEGRATION_TIME_MIN;
		dbg_sensor(1, "min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	}
}

static void sensor_imx576_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
{
	u32 vt_pix_clk_hz = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	FIMC_BUG_VOID(!pll_info);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info->pclk;

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = ((pll_info->frame_length_lines * pll_info->line_length_pck)
								/ (vt_pix_clk_hz / (1000 * 1000)));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;
#ifdef REAR_SUB_CAMERA
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif
	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info->frame_length_lines * pll_info->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%d) / "
		KERN_CONT "(pll_info->frame_length_lines(%d) * pll_info->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, pll_info->frame_length_lines, pll_info->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info->frame_length_lines * pll_info->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info->frame_length_lines;
	cis_data->line_length_pck = pll_info->line_length_pck;
	cis_data->line_readOut_time = sensor_cis_do_div64((u64)cis_data->line_length_pck * (u64)(1000 * 1000 * 1000), cis_data->pclk);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;
	cis_data->max_coarse_integration_time =
		SENSOR_IMX576_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL - SENSOR_IMX576_COARSE_INTEGRATION_TIME_MAX_MARGIN;

	/* Frame valid time calcuration */
	frame_valid_us = sensor_cis_do_div64((u64)cis_data->cur_height * (u64)cis_data->line_length_pck * (u64)(1000 * 1000), cis_data->pclk);
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n", cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(us): %d\n", frame_valid_us);
	dbg_sensor(1, "rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(Mbps): %d\n", cis_data->pclk / 1000000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
	cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_IMX576_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_IMX576_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_IMX576_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_IMX576_COARSE_INTEGRATION_TIME_MAX_MARGIN;
	info("%s: done", __func__);
}

void sensor_imx576_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG_VOID(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG_VOID(!cis);
	FIMC_BUG_VOID(!cis->cis_data);

	if (mode > sensor_imx576_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_imx576_check_rev is fail: ret(%d)", ret);
			return;
		}
	}

	sensor_imx576_cis_data_calculation(sensor_imx576_pllinfos[mode], cis->cis_data);
}

static int sensor_imx576_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	FIMC_BUG(!cis_data);

#define STREAM_OFF_WAIT_TIME 250
	while (timeout < STREAM_OFF_WAIT_TIME) {
		if (cis_data->is_active_area == false &&
				cis_data->stream_on == false) {
			pr_debug("actual stream off\n");
			break;
		}
		timeout++;
	}

	if (timeout == STREAM_OFF_WAIT_TIME) {
		pr_err("actual stream off wait timeout\n");
		ret = -1;
	}

	return ret;
}

#ifdef IMX576J_WRONG_VERSION_CHECK
int sensor_imx576_cis_check_module_info(struct fimc_is_cis *cis)
{
	char imx576_module_cal_map_ver;
	char imx576_module_project_name[FIMC_IS_PROJECT_NAME_SIZE + 1];
	struct fimc_is_rom_info *sysfs_finfo;

	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
	if (sysfs_finfo == NULL){
		return -EINVAL;
	}

	imx576_module_cal_map_ver = sysfs_finfo->cal_map_ver[3];
	strcpy(imx576_module_project_name, sysfs_finfo->project_name);

	info("[%s] Module Info : cal_map_ver=%c, Project name=%s\n", __func__,
		imx576_module_cal_map_ver, imx576_module_project_name);

	if ( *imx576_module_project_name != 0 &&
		strcmp(IMX576J_PROJECT_NAME_FOR_MODULE_CHECK, imx576_module_project_name)) {
		info("[%s]This is not %s module\n", __func__, IMX576J_PROJECT_NAME_FOR_MODULE_CHECK);
		return 0;
	}

	/***** A50 IMX576 Module *****/
	warn("THIS IS IMX576 MODULE FOR A50 PROJECT. PDAF CHIP_REV CHECK!!!");
	if ((imx576_module_cal_map_ver < IMX576J_PDAF_SUPPORT_CALMAP_VER)) {
		warn("NO SUPPORTED MODULE");
		return -EINVAL;
	} else {
		if ((cis->cis_data->cis_rev) == IMX576J_PDAF_WRONG_CHIP_VERSION) {
			warn("WRONG CHIP_REV OF PDAF SENSOR");
			sensor_imx576J_PDAF_version = true;
		}
	}

	return 0;
}
#endif

int sensor_imx576_cis_check_rev(struct fimc_is_cis *cis)
{
	int ret = 0;
	u8 rev = 0, status = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* Specify OTP Page Address for READ - Page63(dec) */
	fimc_is_sensor_write8(client, 0x0A02,  0x3F);

	/* Turn ON OTP Read MODE */
	fimc_is_sensor_write8(client, 0x0A00,  0x01);

	/* Check status - 0x01 : read ready*/
	fimc_is_sensor_read8(client, 0x0A01,  &status);
	if ((status & 0x1) == false)
		err("status fail, (%d)", status);

	/* Readout data
	 * addr = 0x0018
	 * value = 0x10 ---> IMX576K MP0 (frist sample)
	 * value = 0x11 ---> IMX576K MP  (for MP)
	 * value = 0x20 ---> IMX576J PDAF Cut1.0 (PDAF 1st sample)
	 * value = 0x21 ---> IMX576J PDAF MP1    (PDAF MP1)
	 */
	ret = fimc_is_sensor_read8(client, 0x0018, &rev);
	if (ret < 0) {
		err("fimc_is_sensor_read8 fail (ret %d)", ret);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		return ret;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis->cis_data->cis_rev = rev;

	if (rev == 0x10 || rev == 0x11) {
		sensor_imx576J_PDAF_version = false;
	}
	probe_info("imx576 rev:%x", rev);

	return 0;
}

#if SENSOR_IMX576_CAL_DEBUG
int sensor_imx576_cis_cal_dump(char* name, char *buf, size_t size)
{
	int ret = 0;

	struct file *fp;
	ssize_t tx = -ENOENT;
	int fd, old_mask;
	loff_t pos = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	sys_rmdir(name);
	fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	if (fd < 0) {
		err("open file error(%d): %s", fd, name);
		sys_umask(old_mask);
		set_fs(old_fs);
		ret = -EINVAL;
		goto p_err;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, size, &pos);
		if (tx != size) {
			err("fail to write %s. ret %zd", name, tx);
			ret = -ENOENT;
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

p_err:
	return ret;
}
#endif

int sensor_imx576_cis_LRC_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	int position;
	u16 start_addr;
	u16 data_size;

	ulong cal_addr;
	u8 cal_data[SENSOR_IMX576_LRC_CAL_SIZE] = {0, };

#ifdef CONFIG_VENDER_MCD_V2
	char *rom_cal_buf = NULL;
#else
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	position = sensor_peri->module->position;

#ifdef CONFIG_VENDER_MCD_V2
	ret = fimc_is_sec_get_cal_buf(position, &rom_cal_buf);

	if (ret < 0) {
		goto p_err;
	}

	cal_addr = (ulong)rom_cal_buf;
	if (position == SENSOR_POSITION_REAR) {
		cal_addr += SENSOR_IMX576_LRC_CAL_BASE_REAR;
	} else if (position == SENSOR_POSITION_FRONT) {
		cal_addr += SENSOR_IMX576_LRC_CAL_BASE_FRONT;
	} else {
		err("cis_imx576 position(%d) is invalid!\n", position);
		goto p_err;
	}
#else
	if (position == SENSOR_POSITION_REAR){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX576_LRC_CAL_BASE_REAR;
	}else if (position == SENSOR_POSITION_FRONT){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX576_LRC_CAL_BASE_FRONT;
	}else {
		err("cis_imx576 position(%d) is invalid!\n", position);
		goto p_err;
	}
#endif

	memcpy(cal_data, (u16 *)cal_addr, SENSOR_IMX576_LRC_CAL_SIZE);

#if SENSOR_IMX576_CAL_DEBUG
	ret = sensor_imx576_cis_cal_dump(SENSOR_IMX576_LRC_DUMP_NAME, (char *)cal_data, (size_t)SENSOR_IMX576_LRC_CAL_SIZE);
	if (ret < 0) {
		err("cis_imx576 LRC Cal dump fail(%d)!\n", ret);
		goto p_err;
	}
#endif

	start_addr = SENSOR_IMX576_LRC_REG_ADDR;
	data_size = SENSOR_IMX576_LRC_CAL_SIZE;
	ret = fimc_is_sensor_write8_sequential(client, start_addr, cal_data, data_size);
	if (ret < 0) {
		err("cis_imx576 LRC write Error(%d)\n", ret);
	}

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_imx576_cis_QuadSensCal_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	int position;
	u16 start_addr;
	u16 data_size;

	ulong cal_addr;
	u8 cal_data[SENSOR_IMX576_QUAD_SENS_CAL_SIZE] = {0, };

#ifdef CONFIG_VENDER_MCD_V2
	char *rom_cal_buf = NULL;
#else
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	position = sensor_peri->module->position;

#ifdef CONFIG_VENDER_MCD_V2
	ret = fimc_is_sec_get_cal_buf(position, &rom_cal_buf);

	if (ret < 0) {
		goto p_err;
	}

	cal_addr = (ulong)rom_cal_buf;
	if (position == SENSOR_POSITION_REAR) {
		cal_addr += SENSOR_IMX576_QUAD_SENS_CAL_BASE_REAR;
	} else if (position == SENSOR_POSITION_FRONT) {
		cal_addr += SENSOR_IMX576_QUAD_SENS_CAL_BASE_FRONT;
	} else {
		err("cis_imx576 position(%d) is invalid!\n", position);
		goto p_err;
	}
#else
	if (position == SENSOR_POSITION_REAR){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX576_QUAD_SENS_CAL_BASE_REAR;
	}else if (position == SENSOR_POSITION_FRONT){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX576_QUAD_SENS_CAL_BASE_FRONT;
	}else {
		err("cis_imx576 position(%d) is invalid!\n", position);
		goto p_err;
	}
#endif

	memcpy(cal_data, (u16 *)cal_addr, SENSOR_IMX576_QUAD_SENS_CAL_SIZE);

#if SENSOR_IMX576_CAL_DEBUG
	ret = sensor_imx576_cis_cal_dump(SENSOR_IMX576_QSC_DUMP_NAME, (char *)cal_data, (size_t)SENSOR_IMX576_QUAD_SENS_CAL_SIZE);
	if (ret < 0) {
		err("cis_imx576 QSC Cal dump fail(%d)!\n", ret);
		goto p_err;
	}
#endif

	start_addr = SENSOR_IMX576_QUAD_SENS_REG_ADDR;
	data_size = SENSOR_IMX576_QUAD_SENS_CAL_SIZE;
	ret = fimc_is_sensor_write8_sequential(client, start_addr, cal_data, data_size);
	if (ret < 0) {
		err("cis_imx576 QSC write Error(%d)\n", ret);
	}

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_imx576_cis_DPC_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

	int position;
	u16 start_val[2] = {0, };
	u16 count_val[2] = {0, };
	u16 start_addr = 0;
	u16 data_size = 0;

	ulong cal_addr;
	u8 *cal_data = NULL;

#ifdef CONFIG_VENDER_MCD_V2
	char *rom_cal_buf = NULL;
#else
	struct fimc_is_lib_support *lib = &gPtr_lib_support;
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	position = sensor_peri->module->position;

#ifdef CONFIG_VENDER_MCD_V2
	fimc_is_sec_get_cal_buf(position, &rom_cal_buf);

	if (ret < 0) {
		goto p_err;
	}

	cal_addr = (ulong)rom_cal_buf;
	if (position == SENSOR_POSITION_REAR) {
		cal_addr += SENSOR_IMX576_DPC_CAL_BASE_REAR;
	} else if (position == SENSOR_POSITION_FRONT) {
		cal_addr += SENSOR_IMX576_DPC_CAL_BASE_FRONT;
	} else {
		err("cis_imx576 position(%d) is invalid!\n", position);
		goto p_err;
	}
#else
	if (position == SENSOR_POSITION_REAR){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX576_DPC_CAL_BASE_REAR;
	}else if (position == SENSOR_POSITION_FRONT){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX576_DPC_CAL_BASE_FRONT;
	}else {
		err("cis_imx576 position(%d) is invalid!\n", position);
		goto p_err;
	}
#endif

	cal_data = kzalloc(SENSOR_IMX576_DPC_CAL_SIZE, GFP_KERNEL);
	if (!cal_data) {
		err("cis_imx576 cal_data alloc fail");
		ret = -ENOMEM;
		goto p_err;
	}

	memcpy(cal_data, (u16 *)cal_addr, SENSOR_IMX576_DPC_CAL_SIZE);

#if SENSOR_IMX576_CAL_DEBUG
	ret = sensor_imx576_cis_cal_dump(SENSOR_IMX576_DPC_DUMP_NAME, (char *)cal_data, (size_t)SENSOR_IMX576_DPC_CAL_SIZE);
	if (ret < 0) {
		err("cis_imx576 QSC Cal dump fail(%d)!\n", ret);
		goto p_err;
	}
#endif

	start_val[0] = cal_data[SENSOR_IMX576_DPC_CAL_SIZE - 4];
	start_val[1] = cal_data[SENSOR_IMX576_DPC_CAL_SIZE - 3];
	count_val[0] = cal_data[SENSOR_IMX576_DPC_CAL_SIZE - 2];
	count_val[1] = cal_data[SENSOR_IMX576_DPC_CAL_SIZE - 1];
	start_addr = ((((start_val[0] & 0x00FF) << 8) & 0xFF00) | (start_val[1] & 0x00FF));
	data_size = ((((count_val[0] & 0x00FF) << 8) & 0xFF00) | (count_val[1] & 0x00FF));

	if (start_addr < 0xFFFF && data_size > 0x0) {
		ret = fimc_is_sensor_write8_sequential(client, start_addr, cal_data, data_size);
		if (ret < 0) {
			err("cis_imx576 DPC write Error(%d).\n", ret);
		}
	} else {
		warn("[%s]DPC write skip. start_addr=0x%#x, data_size = %d", __func__, start_addr, data_size);
	}

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if(cal_data)
		kfree(cal_data);

	return ret;
}

/* CIS OPS */
int sensor_imx576_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
#ifdef USE_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
#endif

	setinfo.param = NULL;
	setinfo.return_value = 0;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!cis->cis_data);
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	probe_info("%s imx576 init\n", __func__);
	cis->rev_flag = false;

/***********************************************************************
***** Check that QSC and DPC Cal is written for Remosaic Capture.
***** false : Not yet write the QSC and DPC
***** true  : Written the QSC and DPC Or Skip
***********************************************************************/
	sensor_imx576_cal_write_flag = false;

/***********************************************************************
***** Check to apply the PDAF Version
***** false : (IMX576K version - NON PDAF)
***** true  : (IMX576J version - PDAF)
***********************************************************************/
	sensor_imx576J_PDAF_version = true;

	ret = sensor_imx576_cis_check_rev(cis);
	if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
		sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
		if (sensor_peri)
			fimc_is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
#endif
		warn("sensor_imx576_check_rev is fail when cis init");
		cis->rev_flag = true;
		ret = 0;
	}
#ifdef IMX576J_WRONG_VERSION_CHECK
	ret = sensor_imx576_cis_check_module_info(cis);
	if (ret < 0) {
		goto p_err;
	}
#endif

	//It is temporary code for prevent camera fail with old module(without PDAF) for A50 Model
	if(sensor_imx576J_PDAF_version == false) {
		sensor_imx576_cal_write_flag = true;
	}

	info("[%s] cis_rev=%#x, PDAF support is %d\n", __func__,
		cis->cis_data->cis_rev, sensor_imx576J_PDAF_version);

	if (sensor_imx576J_PDAF_version) {
		probe_info("%s chip_rev(%d) setfile_B for PDAF sensor\n", __func__, cis->cis_data->cis_rev);
		sensor_imx576_global = sensor_imx576_setfile_B_Global;
		sensor_imx576_global_size = sizeof(sensor_imx576_setfile_B_Global) / sizeof(sensor_imx576_setfile_B_Global[0]);
		sensor_imx576_setfiles = sensor_imx576_setfiles_B;
		sensor_imx576_setfile_sizes = sensor_imx576_setfile_B_sizes;
		sensor_imx576_pllinfos = sensor_imx576_pllinfos_B;
		sensor_imx576_max_setfile_num = sizeof(sensor_imx576_setfiles_B) / sizeof(sensor_imx576_setfiles_B[0]);
	} else if (!sensor_imx576J_PDAF_version) {
		probe_info("%s chip_rev(%d) setfile_A for NON PDAF sensor\n", __func__, cis->cis_data->cis_rev);
		sensor_imx576_global = sensor_imx576_setfile_A_Global;
		sensor_imx576_global_size = sizeof(sensor_imx576_setfile_A_Global) / sizeof(sensor_imx576_setfile_A_Global[0]);
		sensor_imx576_setfiles = sensor_imx576_setfiles_A;
		sensor_imx576_setfile_sizes = sensor_imx576_setfile_A_sizes;
		sensor_imx576_pllinfos = sensor_imx576_pllinfos_A;
		sensor_imx576_max_setfile_num = sizeof(sensor_imx576_setfiles_A) / sizeof(sensor_imx576_setfiles_A[0]);
	} else {
		probe_info("%s chip_rev(%d) is wrong! default PDAF setfile \n", __func__, cis->cis_data->cis_rev);
		sensor_imx576_global = sensor_imx576_setfile_B_Global;
		sensor_imx576_global_size = sizeof(sensor_imx576_setfile_B_Global) / sizeof(sensor_imx576_setfile_B_Global[0]);
		sensor_imx576_setfiles = sensor_imx576_setfiles_B;
		sensor_imx576_setfile_sizes = sensor_imx576_setfile_B_sizes;
		sensor_imx576_pllinfos = sensor_imx576_pllinfos_B;
		sensor_imx576_max_setfile_num = sizeof(sensor_imx576_setfiles_B) / sizeof(sensor_imx576_setfiles_B[0]);
	}

	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_IMX576_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_IMX576_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	sensor_imx576_cis_data_calculation(sensor_imx576_pllinfos[setfile_index], cis->cis_data);
	sensor_imx576_set_integration_max_margin(setfile_index, cis->cis_data);
	sensor_imx576_set_integration_min(setfile_index, cis->cis_data);

	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max dgain : %d\n", __func__, setinfo.return_value);

	if(sensor_imx576J_PDAF_version == true) {

		info("[%s] PDAF sensor! Write LRC data.\n", __func__);

		ret = sensor_imx576_cis_LRC_write(subdev);
		if (ret < 0) {
			err("sensor_imx576_LRC_Cal_write fail!! (%d)", ret);
			goto p_err;
		}
	}

	if (sensor_imx576_cal_write_flag == false) {
		sensor_imx576_cal_write_flag = true;

		info("[%s] mode is QBC Remosaic Mode! Write QSC and DPC data.\n", __func__);

		ret = sensor_imx576_cis_QuadSensCal_write(subdev);
		if (ret < 0) {
			err("sensor_imx576_Quad_Sens_Cal_write fail!! (%d)", ret);
			goto p_err;
		}
		ret = sensor_imx576_cis_DPC_write(subdev);
		if (ret < 0) {
			err("sensor_imx576_DPC_write fail!! (%d)", ret);
			goto p_err;
		}
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx576_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	pr_err("[SEN:DUMP] *******************************\n");
	fimc_is_sensor_read16(client, 0x0000, &data16);
	pr_err("[SEN:DUMP] model_id(%x)\n", data16);
	fimc_is_sensor_read8(client, 0x0002, &data8);
	pr_err("[SEN:DUMP] revision_number(%x)\n", data8);
	fimc_is_sensor_read8(client, 0x0005, &data8);
	pr_err("[SEN:DUMP] frame_count(%x)\n", data8);
	fimc_is_sensor_read8(client, 0x0100, &data8);
	pr_err("[SEN:DUMP] mode_select(%x)\n", data8);

	sensor_cis_dump_registers(subdev, sensor_imx576_setfiles[0], sensor_imx576_setfile_sizes[0]);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	pr_err("[SEN:DUMP] *******************************\n");

p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_imx576_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = fimc_is_sensor_write8(client, 0x0104, hold);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;
p_err:
	return ret;
}
#else
static inline int sensor_imx576_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_imx576_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_imx576_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

static void sensor_imx576_cis_set_paf_stat_enable(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
	case SENSOR_IMX576_2880X2156_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X1620_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X1332_2X2BIN_30FPS:
	case SENSOR_IMX576_2156X2156_2X2BIN_30FPS:
	case SENSOR_IMX576_2880X2156_QBCHDR_30FPS:
	case SENSOR_IMX576_2880X1620_QBCHDR_30FPS:
	case SENSOR_IMX576_2880X1332_QBCHDR_30FPS:
	case SENSOR_IMX576_2156X2156_QBCHDR_30FPS:
	case SENSOR_IMX576_5760X4312_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_5760X3240_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_5760X2664_QBCREMOSAIC_30FPS:
	case SENSOR_IMX576_4312X4312_QBCREMOSAIC_30FPS:
		cis_data->is_data.paf_stat_enable = true;
		break;
	default:
		cis_data->is_data.paf_stat_enable = false;
		break;
	}
}

int sensor_imx576_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* setfile global setting is at camera entrance */
	info("[%s] global setting start\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_imx576_global, sensor_imx576_global_size);
	if (ret < 0) {
		err("sensor_imx576_set_registers fail!!");
		goto p_err;
	}

	dbg_sensor(1, "[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	// Check that QSC and DPC Cal is written for Remosaic Capture.
	// false : Not yet write the QSC and DPC
	// true  : Written the QSC and DPC
	sensor_imx576_cal_write_flag = false;
	return ret;
}

int sensor_imx576_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (mode > sensor_imx576_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	/* If check_rev(Sensor ID in OTP) of IMX576 fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_imx576_check_rev is fail");
			goto p_err;
		}
		info("[%s] cis_rev=%#x\n", __func__, cis->cis_data->cis_rev);
	}

#if 0 /* cis_data_calculation is called in module_s_format */
	sensor_imx576_cis_data_calculation(sensor_imx576_pllinfos[mode], cis->cis_data);
#endif
	sensor_imx576_set_integration_max_margin(mode, cis->cis_data);

#ifdef USE_AP_PDAF
	sensor_imx576_cis_set_paf_stat_enable(mode, cis->cis_data);
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] mode=%d, mode change setting start\n", __func__, mode);
	ret = sensor_cis_set_registers(subdev, sensor_imx576_setfiles[mode], sensor_imx576_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_imx576_set_registers fail!!");
		goto p_err;
	}
	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

#if 0
	if (mode >= SENSOR_IMX576_5760X4312_QBCREMOSAIC_30FPS
		&& mode <= SENSOR_IMX576_4312X4312_QBCREMOSAIC_30FPS
		&& sensor_imx576_cal_write_flag == false) {
		sensor_imx576_cal_write_flag = true;

		info("[%s] %d mode is QBC Remosaic Mode! Write QSC and DPC data.\n", __func__, mode);

		ret = sensor_imx576_cis_QuadSensCal_write(subdev);
		if (ret < 0) {
			err("sensor_imx576_Quad_Sens_Cal_write fail!! (%d)", ret);
			goto p_err;
		}
		ret = sensor_imx576_cis_DPC_write(subdev);
		if (ret < 0) {
			err("sensor_imx576_DPC_write fail!! (%d)", ret);
			goto p_err;
		}
	}
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_imx576_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x = 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct fimc_is_cis *cis = NULL;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif
	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (unlikely(!cis_data)) {
		err("cis data is NULL");
		if (unlikely(!cis->cis_data)) {
			ret = -EINVAL;
			goto p_err;
		} else {
			cis_data = cis->cis_data;
		}
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Wait actual stream off */
	ret = sensor_imx576_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_IMX576_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_IMX576_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_IMX576_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_IMX576_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 1. page_select */
	ret = fimc_is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		goto p_err;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_IMX576_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_IMX576_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_write16(client, 0x0344, start_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0346, start_y);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0348, end_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x034A, end_y);
	if (ret < 0)
		goto p_err;

	/* 3. output address setting */
	ret = fimc_is_sensor_write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		goto p_err;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = fimc_is_sensor_write16(client, 0x0380, even_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0382, odd_x);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0384, even_y);
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write16(client, 0x0386, odd_y);
	if (ret < 0)
		goto p_err;

	/* 5. binnig setting */
	ret = fimc_is_sensor_write8(client, 0x0900, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err;
	ret = fimc_is_sensor_write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err;

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full) */
	ret = fimc_is_sensor_write16(client, 0x0400, 0x0000);
	if (ret < 0)
		goto p_err;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed))
	down scale factor = down_scale_m / down_scale_n */
	ret = fimc_is_sensor_write16(client, 0x0404, 0x0010);
	if (ret < 0)
		goto p_err;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__, cis->cis_data->frame_time,
			cis->cis_data->rolling_shutter_skew);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_imx576_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	sensor_imx576_cis_group_param_hold_func(subdev, 0x01);

#ifdef DEBUG_IMX576_PLL
	{
	u16 pll;
	fimc_is_sensor_read16(client, 0x0300, &pll);
	dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0302, &pll);
	dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0304, &pll);
	dbg_sensor(1, "______ pre_pll_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0306, &pll);
	dbg_sensor(1, "______ pll_multiplier(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x030a, &pll);
	dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x030c, &pll);
	dbg_sensor(1, "______ op_prepllck_div(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x030e, &pll);
	dbg_sensor(1, "______ op_pll_multiplier(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0310, &pll);
	dbg_sensor(1, "______ pll_mult_driv(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0340, &pll);
	dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
	fimc_is_sensor_read16(client, 0x0342, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

	info("[%s] start\n", __func__);
	/* here Add for Master mode in dual */
	fimc_is_sensor_write8(client, 0x3040, 0x01);
	fimc_is_sensor_write8(client, 0x3F71, 0x01);
	/* Sensor stream on */
	fimc_is_sensor_write8(client, 0x0100, 0x01);

	sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx576_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	sensor_imx576_cis_group_param_hold_func(subdev, 0x00);

	fimc_is_sensor_write8(client, 0x0100, 0x00);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx576_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct fimc_is_long_term_expo_mode *lte_mode;

	u32 vt_pic_clk_freq_mhz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u8 arrayBuf[4];
	u32 target_exp = 0;
	u32 target_frame_duration = 0;
	u16 frame_length_lines = 0;

	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	target_exp = target_exposure->val;
	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night Hyper Laps: min exp. is 1.5sec)
	 * If frame duration over than 1sec, then sequence is same as below
	 * 1. set CIT_LSHFIT
	 * 2. set COARSE_INTEGRATION_TIME
	 * 3. set FRM_LENGTH_LINES
	 */
	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		target_frame_duration = cis_data->cur_frame_us_time;
		dbg_sensor(1, "[MOD:D:%d] %s, input frame duration(%d) for CIT SHIFT \n",
			cis->id, __func__, target_frame_duration);

		if (target_frame_duration > 100000) {
			cit_lshift_val = (unsigned char)(target_frame_duration / 100000);
			while(cit_lshift_val > 1) {
				cit_lshift_val /= 2;
				target_frame_duration /= 2;
				target_exp /= 2;
				cit_lshift_count ++;
			}

			if (cit_lshift_count > SENSOR_IMX576_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_IMX576_MAX_CIT_LSHIFT_VALUE;
		}

		frame_length_lines = (u16)((vt_pic_clk_freq_mhz * target_frame_duration) / line_length_pck);

		cis_data->frame_length_lines = frame_length_lines;
		cis_data->frame_length_lines_shifter = cit_lshift_count;
		cis_data->max_coarse_integration_time =
			frame_length_lines - cis_data->max_margin_coarse_integration_time;

		dbg_sensor(1, "[MOD:D:%d] %s, target_frame_duration(%d), frame_length_line(%d), cit_lshift_count(%d)\n",
			cis->id, __func__, target_frame_duration, frame_length_lines, cit_lshift_count);
	}

	long_coarse_int = ((target_exp * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
	short_coarse_int = ((target_exp * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;

	if (long_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->max_coarse_integration_time);
		long_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
		long_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	cis_data->cur_long_exposure_coarse = long_coarse_int;
	cis_data->cur_short_exposure_coarse = short_coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		if (cit_lshift_count > 0) {
			ret = fimc_is_sensor_write8(client, SENSOR_IMX576_CIT_LSHIFT_ADDR, cit_lshift_count);
			if (ret < 0)
				goto p_err;
		}
	}

	//Long exposure
	arrayBuf[0] = (cis_data->cur_long_exposure_coarse & 0xFF00) >> 8;
	arrayBuf[1] = cis_data->cur_long_exposure_coarse & 0xFF;
	ret = fimc_is_sensor_write8_array(client, SENSOR_IMX576_COARSE_INTEG_TIME_ADDR, arrayBuf, 2);
		if (ret < 0)
			goto p_err;

	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		ret = fimc_is_sensor_write16(client, SENSOR_IMX576_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
		if (ret < 0)
			goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_mhz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n", cis->id, __func__,
		cis_data->sen_vsync_count, vt_pic_clk_freq_mhz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "long_coarse_int %#x, short_coarse_int %#x\n", cis->id, __func__,
		cis_data->sen_vsync_count, cis_data->frame_length_lines, long_coarse_int, short_coarse_int);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_imx576_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	if (vt_pic_clk_freq_mhz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_mhz(%d)\n", cis->id, __func__, vt_pic_clk_freq_mhz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = ((line_length_pck * min_coarse) + min_fine) / vt_pic_clk_freq_mhz;
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx576_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	if (vt_pic_clk_freq_mhz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_mhz(%d)\n", cis->id, __func__, vt_pic_clk_freq_mhz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = ((line_length_pck * max_coarse) + max_fine) / vt_pic_clk_freq_mhz;

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time, cis_data->max_margin_fine_integration_time, cis_data->max_coarse_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx576_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	u32 max_frame_us_time = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = ((vt_pic_clk_freq_mhz * input_exposure_time) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (frame_length_lines * line_length_pck) / vt_pic_clk_freq_mhz;
	max_frame_us_time = 1000000/cis->min_fps;

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d), max_frame_us_time(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time, max_frame_us_time);

	dbg_sensor(1, "[%s] min_fps(%d), max_fps(%d)\n", __func__, cis->min_fps, cis->max_fps);
	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	if((cis_data->min_frame_us_time <= 100000) && (cis->min_fps == cis->max_fps)) {
		*target_duration = MIN(frame_duration, max_frame_us_time);
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx576_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct fimc_is_long_term_expo_mode *lte_mode;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night HyperLapse)
	 * If frame duration over than 1sec, then it has to be applied CIT shift.
	 * In this case, frame_duration is setted in set_exposure_time with CIT shift.
	 */
	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > 1000000) {
		cis_data->cur_frame_us_time = frame_duration;
		dbg_sensor(1, "[MOD:D:%d][%s] Skip set frame duration(%d) for CIT SHIFT.\n",
			cis->id, __func__, frame_duration);
		return ret;
	}

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_mhz * frame_duration) / line_length_pck);

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_mhz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
			cis->id, __func__, vt_pic_clk_freq_mhz, frame_duration,
			line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->frame_length_lines_shifter > 0) {
		cis_data->frame_length_lines_shifter = 0;
		ret = fimc_is_sensor_write8(client, SENSOR_IMX576_CIT_LSHIFT_ADDR, 0);
	}

	ret = fimc_is_sensor_write16(client, SENSOR_IMX576_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
	if (ret < 0)
		goto p_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = SENSOR_IMX576_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL - cis_data->max_margin_coarse_integration_time;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_imx576_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n",
			cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_imx576_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

	cis_data->min_frame_us_time = frame_duration;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:

	return ret;
}

u32 sensor_imx576_cis_calc_again_code(u32 permille)
{
	return 1024 - (1024000 / permille);
}

u32 sensor_imx576_cis_calc_again_permile(u32 code)
{
	return 1024000 / (1024 - code);
}

int sensor_imx576_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_permile);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_imx576_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_imx576_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;
	u8 arrayBuf[2];

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	analog_gain = (u16)sensor_imx576_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	// Analog gain
	arrayBuf[0] = (analog_gain & 0xFF00) >> 8;
	arrayBuf[1] = analog_gain & 0xFF;
	ret = fimc_is_sensor_write8_array(client, SENSOR_IMX576_ANALOG_GAIN_ADDR, arrayBuf, 2);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_imx576_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_read16(client, SENSOR_IMX576_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto p_err;

	*again = sensor_imx576_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_imx576_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u16 read_value = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	read_value = SENSOR_IMX576_MIN_ANALOG_GAIN_SET_VALUE;

	cis_data->min_analog_gain[0] = read_value;
	cis_data->min_analog_gain[1] = sensor_imx576_cis_calc_again_permile(cis_data->min_analog_gain[0]);
	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_analog_gain[0],
		cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx576_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;
	u16 read_value = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	read_value = SENSOR_IMX576_MAX_ANALOG_GAIN_SET_VALUE;

	cis_data->max_analog_gain[0] = read_value;
	cis_data->max_analog_gain[1] = sensor_imx576_cis_calc_again_permile(cis_data->max_analog_gain[0]);
	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_analog_gain[0],
		cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx576_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;
	u8 dgains[2] = {0};

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis->cis_data->min_digital_gain[0]) {
		long_gain = cis->cis_data->min_digital_gain[0];
	}
	if (long_gain > cis->cis_data->max_digital_gain[0]) {
		long_gain = cis->cis_data->max_digital_gain[0];
	}

	if (short_gain < cis->cis_data->min_digital_gain[0]) {
		short_gain = cis->cis_data->min_digital_gain[0];
	}
	if (short_gain > cis->cis_data->max_digital_gain[0]) {
		short_gain = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us,"
			KERN_CONT "long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count,
			dgain->long_val, dgain->short_val, long_gain, short_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	// Set current short & long digitial gain
	// 0x0218 ~ 0x0219 : ST_DIG_GAIN_GLOBAL
	// 0x020E ~ 0x020F : DIG_GAIN_GLOBAL
	if (fimc_is_vender_wdr_mode_on(cis_data)) {
		dgains[0] = (short_gain & 0xFF00) >> 8;
		dgains[1] = short_gain & 0xFF;
		ret = fimc_is_sensor_write8_array(client, SENSOR_IMX576_SOHT_DIG_GAIN_ADDR, dgains, 2);
		if (ret < 0) {
			goto p_err;
	}
	}
	dgains[0] = (long_gain & 0xFF00) >> 8;
	dgains[1] = long_gain & 0xFF;
	ret = fimc_is_sensor_write8_array(client, SENSOR_IMX576_DIG_GAIN_ADDR, dgains, 2);
	if (ret < 0) {
		goto p_err;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_imx576_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_read16(client, SENSOR_IMX576_DIG_GAIN_ADDR, &digital_gain);
	if (ret < 0)
		goto p_err;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_imx576_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_digital_gain[0] = SENSOR_IMX576_MIN_DIGITAL_GAIN_SET_VALUE;
	cis_data->min_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->min_digital_gain[0]);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_digital_gain[0],
		cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx576_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_digital_gain[0] = SENSOR_IMX576_MAX_DIGITAL_GAIN_SET_VALUE;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->max_digital_gain[0]);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_digital_gain[0],
		cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx576_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct fimc_is_long_term_expo_mode *lte_mode;
	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;
	u32 target_exp = 0;
	int hold = 0;

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	lte_mode = &cis->long_term_mode;

	hold = sensor_imx576_cis_group_param_hold(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable) {
		target_exp = lte_mode->expo[0];
		if (target_exp >= 125000 ) {
			cit_lshift_val = (unsigned char)(target_exp / 125000);
			while(cit_lshift_val > 1)
			{
				cit_lshift_val /= 2;
				target_exp /= 2;
				cit_lshift_count ++;
			}

			lte_mode->expo[0] = target_exp;

			if (cit_lshift_count > SENSOR_IMX576_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_IMX576_MAX_CIT_LSHIFT_VALUE;

			ret = fimc_is_sensor_write8(cis->client, SENSOR_IMX576_CIT_LSHIFT_ADDR, cit_lshift_count);
		}
	} else {
		cit_lshift_count = 0;
		ret = fimc_is_sensor_write8(cis->client, SENSOR_IMX576_CIT_LSHIFT_ADDR, cit_lshift_count);
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	info("[%s] sen_strm_enable(%d), cit_lshift_count (%d), target_exp(%d)", __func__,
		lte_mode->sen_strm_off_on_enable, cit_lshift_count, lte_mode->expo[0]);

	if (ret < 0)
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);

	return ret;
}

int sensor_imx576_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	int hold = 0;
	int mode = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	u16 abs_gains[4] = {0, };	//[0]=gr, [1]=r, [2]=b, [3]=gb

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (!cis->use_wb_gain)
		return ret;

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	mode = cis->cis_data->sens_config_index_cur;

	if ( mode < SENSOR_IMX576_5760X4312_QBCREMOSAIC_30FPS || mode > SENSOR_IMX576_4312X4312_QBCREMOSAIC_30FPS)
		return 0;

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	abs_gains[0] = (u16)((wb_gains.gr / 4) & 0xFFFF);
	abs_gains[1] = (u16)((wb_gains.r / 4) & 0xFFFF);
	abs_gains[2] = (u16)((wb_gains.b / 4) & 0xFFFF);
	abs_gains[3] = (u16)((wb_gains.gb / 4) & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s, abs_gain_gr(0x%4X), abs_gain_r(0x%4X), abs_gain_b(0x%4X), abs_gain_gb(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2], abs_gains[3]);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_write16_array(client, SENSOR_IMX576_ABS_GAIN_GR_SET_ADDR, abs_gains, 4);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_imx576_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

static struct fimc_is_cis_ops cis_ops_imx576 = {
	.cis_init = sensor_imx576_cis_init,
	.cis_log_status = sensor_imx576_cis_log_status,
	.cis_group_param_hold = sensor_imx576_cis_group_param_hold,
	.cis_set_global_setting = sensor_imx576_cis_set_global_setting,
	.cis_mode_change = sensor_imx576_cis_mode_change,
	.cis_set_size = sensor_imx576_cis_set_size,
	.cis_stream_on = sensor_imx576_cis_stream_on,
	.cis_stream_off = sensor_imx576_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_set_exposure_time = sensor_imx576_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_imx576_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_imx576_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_imx576_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_imx576_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_imx576_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_imx576_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_imx576_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_imx576_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_imx576_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_imx576_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_imx576_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_imx576_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_imx576_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_imx576_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
#if 0
	.cis_compensate_gain_for_extremely_br = sensor_imx576_cis_compensate_gain_under_ext_br,
#endif
	.cis_data_calculation = sensor_imx576_cis_data_calc,
	.cis_set_long_term_exposure = sensor_imx576_cis_long_term_exposure,
	.cis_set_wb_gains = sensor_imx576_cis_set_wb_gain,
};

int cis_imx576_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	bool use_pdaf = false;
	struct fimc_is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct fimc_is_cis *cis = NULL;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id[FIMC_IS_STREAM_COUNT] = {0, };
	u32 sensor_id_len;
	const u32 *sensor_id_spec;
	char const *setfile;
	struct device *dev;
	struct device_node *dnode;
	int i;

	FIMC_BUG(!client);
	FIMC_BUG(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	if (of_property_read_bool(dnode, "use_pdaf")) {
		use_pdaf = true;
	}

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= sizeof(*sensor_id_spec);

	probe_info("%s sensor_id_spec %d, sensor_id_len %d\n", __func__,
			*sensor_id_spec, sensor_id_len);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);
		device = &core->sensor[sensor_id[i]];

		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_IMX576);
		if (!sensor_peri) {
			probe_info("sensor peri is not yet probed");
			return -EPROBE_DEFER;
		}
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_IMX576);

		cis = &sensor_peri->cis;
		subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
		if (!subdev_cis) {
			probe_err("subdev_cis is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		sensor_peri->subdev_cis = subdev_cis;

		cis->id = SENSOR_NAME_IMX576;
		cis->subdev = subdev_cis;
		cis->device = sensor_id[i];
		cis->client = client;
		sensor_peri->module->client = cis->client;
		cis->i2c_lock = NULL;
		cis->ctrl_delay = N_PLUS_TWO_FRAME;

		cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
		if (!cis->cis_data) {
			err("cis_data is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		cis->cis_ops = &cis_ops_imx576;

		/* belows are depend on sensor cis. MUST check sensor spec */
		cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;

		if (of_property_read_bool(dnode, "sensor_f_number")) {
			ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
			if (ret) {
				warn("f-number read is fail(%d)", ret);
			}
		} else {
			cis->aperture_num = F2_2;
		}

		probe_info("%s f-number %d\n", __func__, cis->aperture_num);

		cis->use_dgain = true;
		cis->hdr_ctrl_by_again = false;
		cis->use_wb_gain = true;

		if (use_pdaf == true) {
			cis->use_pdaf = true;
		} else {
			cis->use_pdaf = false;
		}

		v4l2_set_subdevdata(subdev_cis, cis);
		v4l2_set_subdev_hostdata(subdev_cis, device);
		snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);
	}

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_imx576_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-cis-imx576",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx576_match);

static const struct i2c_device_id sensor_cis_imx576_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx576_driver = {
	.probe	= cis_imx576_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx576_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx576_idt
};

static int __init sensor_cis_imx576_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx576_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx576_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx576_init);
