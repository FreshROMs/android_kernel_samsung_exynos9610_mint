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
#include <asm/byteorder.h>

#include <exynos-fimc-is-sensor.h>
#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-cis-2x5.h"
#include "fimc-is-cis-2x5-setA.h"
#include "fimc-is-cis-2x5-setB.h"

#include "fimc-is-helper-i2c.h"
#if defined(SUPPORT_2X5_SENSOR_VARIATION) || defined(SENSOR_2X5_CAL_UPLOAD)
#ifdef CONFIG_VENDER_MCD_V2
#include "fimc-is-sec-define.h"
#endif
static bool last_version = false;
#endif
static bool cal_done = false;

#if SENSOR_2X5_CAL_BURST_WRITE
static u32 burst_cal_buf[BURST_BUF_SIZE];
#ifdef DEBUG_BURSTBUF_MEM
static u32 burst_index;
static u32 *end_addr;
#endif /* DEBUG_BURSTBUF_MEM */
#endif /* SENSOR_2X5_CAL_BURST_WRITE */

#define SENSOR_NAME "S5K2X5"
/* #define DEBUG_2X5_PLL */

static const struct v4l2_subdev_ops subdev_ops;

static const u32 *sensor_2x5_global;
static u32 sensor_2x5_global_size;
static const u32 **sensor_2x5_setfiles;
static const u32 *sensor_2x5_setfile_sizes;
static u32 sensor_2x5_max_setfile_num;
static const u32 *sensor_2x5_precal;
static u32 sensor_2x5_precal_size;
static const u32 *sensor_2x5_postcal;
static u32 sensor_2x5_postcal_size;
static const u32 **sensor_2x5_tetra_isp;
static const u32 *sensor_2x5_tetra_isp_sizes;
static const struct sensor_pll_info_compact **sensor_2x5_pllinfos;

/* variables of pdaf setting */
#if 0
static const u32 *sensor_2x5_pdaf_global;
static u32 sensor_2x5_pdaf_global_size;
static const u32 **sensor_2x5_pdaf_setfiles;
static const u32 *sensor_2x5_pdaf_setfile_sizes;
static u32 sensor_2x5_pdaf_max_setfile_num;
static const struct sensor_pll_info_compact **sensor_2x5_pdaf_pllinfos;
#endif

static void sensor_2x5_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact, cis_shared_data *cis_data)
{
	u32 vt_pix_clk_hz = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	BUG_ON(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%d), pclk(%d)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck
					/ (vt_pix_clk_hz / (1000 * 1000)));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;

	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%d) / "
		KERN_CONT "(pll_info_compact->frame_length_lines(%d) * pll_info_compact->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, pll_info_compact->frame_length_lines, pll_info_compact->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info_compact->frame_length_lines;
	cis_data->line_length_pck = pll_info_compact->line_length_pck;
	cis_data->line_readOut_time = sensor_cis_do_div64((u64)cis_data->line_length_pck * (u64)(1000 * 1000 * 1000), cis_data->pclk);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = sensor_cis_do_div64((u64)cis_data->cur_height * (u64)cis_data->line_length_pck * (u64)(1000 * 1000), cis_data->pclk);
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n",
					cis_data->cur_width, cis_data->cur_height);
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
	cis_data->min_fine_integration_time = SENSOR_2X5_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_2X5_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_2X5_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_2X5_COARSE_INTEGRATION_TIME_MAX_MARGIN;
}

#if 0
static int sensor_2x5_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	BUG_ON(!cis_data);

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
#endif

#ifdef SENSOR_2X5_CAL_UPLOAD
#if SENSOR_2X5_CAL_BURST_WRITE
void sensor_2x5_cis_print_burst_calbuf(u32 *burst_buf, size_t sz)
{
	u32 nr = sz / 3;
	int i, line;
	
	info("%s: buf-size %d, line %d\n", __func__, sz, nr);

	for (i = 0, line = 0; i < sz; line++) {
		switch(burst_buf[i]) {
		case I2C_MODE_BURST_ADDR:
			info("[cal] %03d: ADDR, 0x%04X, 0x%04X\n", line, burst_buf[i + 1], burst_buf[i + 2]);
			break;
		case I2C_MODE_BURST_DATA:
			info("[cal] %03d: DATA, 0x%04X, 0x%04X\n", line, burst_buf[i + 1], burst_buf[i + 2]);
			break;
		default:
			info("[cal] %03d: 0x%04X, 0x%04X, 0x%04X\n", line, burst_buf[i], burst_buf[i + 1], burst_buf[i + 2]);
			break;
		}

		i += 3;
	}
}

/**
* sensor_2x5_cis_copy_to_burstbuf:
*
* - nr: the number of raw (or line)
**/
void sensor_2x5_cis_copy_burstdata(const u16 *src, u32 *dest, u32 *index, size_t nr)
{
	u32 (*burst_buf)[3] = (u32 (*)[3])&dest[*index];
	int i;

	for (i = 0; i < nr; i++) {
		burst_buf[i][I2C_ADDR] = I2C_MODE_BURST_DATA;
		burst_buf[i][I2C_DATA] = cpu_to_be16(src[i]);
		burst_buf[i][I2C_BYTE] = 0x02;
		*index += 3;
#ifdef DEBUG_BURSTBUF_MEM
		burst_index++;
		end_addr += 3;
#endif
	}
}

void sensor_2x5_cis_copy_ctrldata(const u32 *src, u32 *dest, u32 *index, size_t nr)
{
	u32 (*burst_buf)[3] = (u32 (*)[3])&dest[*index];
	u32 (*ctrl_data)[3] = (u32 (*)[3])src;
	int i;

	for (i = 0; i < nr; i++) {
		burst_buf[i][I2C_ADDR] = ctrl_data[i][I2C_ADDR];
		burst_buf[i][I2C_DATA] = ctrl_data[i][I2C_DATA];
		burst_buf[i][I2C_BYTE] = ctrl_data[i][I2C_BYTE];
		*index += 3;
#ifdef DEBUG_BURSTBUF_MEM
		burst_index++;
		end_addr += 3;
#endif
	}
}

void sensor_2x5_cis_copy_to_burstbuf(const u16 *src, u32 *dest, u32 *index, size_t nr, bool is_cal)
{
	u32 (*burst_buf)[3] = (u32 (*)[3])&dest[*index];
	u16 (*ctrl_data)[3] = (u16 (*)[3])src;
	int i;

	if (is_cal) {
		for (i = 0; i < nr; i++) {
			burst_buf[i][I2C_ADDR] = I2C_MODE_BURST_DATA;
			burst_buf[i][I2C_DATA] = cpu_to_be16(src[i]);
			burst_buf[i][I2C_BYTE] = 0x02;
			*index += 3;
#ifdef DEBUG_BURSTBUF_MEM
			burst_index++;
			end_addr += 3;
#endif
		}
	} else {
		for (i = 0; i < nr; i++) {
			burst_buf[i][I2C_ADDR] = ctrl_data[i][I2C_ADDR];
			burst_buf[i][I2C_DATA] = ctrl_data[i][I2C_DATA];
			burst_buf[i][I2C_BYTE] = ctrl_data[i][I2C_BYTE];
			*index += 3;
#ifdef DEBUG_BURSTBUF_MEM
			burst_index++;
			end_addr += 3;
#endif
		}
	}

}

int sensor_2x5_cis_create_burst_cal(u32 *burst_buf)
{
	const int position = SENSOR_POSITION_FRONT;
	const u16 *src_xtalk = NULL, *src_lsc = NULL;
	const int sz_lsc = SENSOR_2X5_CAL_LSC_SIZE / 2;
	const int sz_xtalk = SENSOR_2X5_CAL_XTALK_SIZE /2;
	const u32 src_head[][3] = {
		{0xFCFC, 0x4000, 0x02},
		{0x6004, 0x0001, 0x02},
		{0x6028, 0x2001, 0x02},
		{0x602A, 0x14D8, 0x02},
		{I2C_MODE_BURST_ADDR, 0x6F12, 0x02}
	};
	const u32 src_xtalkaddr[][3] = {
		{0x602A, 0x2188, 0x02},
		{I2C_MODE_BURST_ADDR, 0x6F12, 0x02}
	};
	const u32 src_tail[][3] = {
		{0xFCFC, 0x4000, 0x02},
		{0x6004, 0x0000, 0x02}
	};
	char *cal_buf = NULL;
	u32 index = 0;
#ifdef CONFIG_VENDER_MCD_V2
	static struct fimc_is_rom_info *finfo = NULL;
#else
	err("write burst cal not implemented!");
	return -ENOSYS;
#endif

#ifdef DEBUG_BURSTBUF_MEM
	burst_index = 0;
	end_addr = burst_buf;
#endif

#ifdef CONFIG_VENDER_MCD_V2
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &cal_buf);
	dbg_sensor(1, "%s: sensor cal start addr1 0x%X, end addr1 0x%X, addr2 0x%X, end addr2 0x%X\n",
		__func__, finfo->sensor_cal_data_start_addr, finfo->sensor_cal_data_end_addr,
		*((u32 *)&cal_buf[0x18]), *((u32 *)&cal_buf[0x1C]));
#endif
	src_xtalk = (u16 *)&cal_buf[finfo->sensor_cal_data_start_addr];
	src_lsc = (u16 *)&cal_buf[finfo->sensor_cal_data_start_addr + SENSOR_2X5_CAL_XTALK_SIZE];

	index = 0;
	sensor_2x5_cis_copy_ctrldata(src_head[0], burst_buf, &index, ARRAY_SIZE(src_head));
	dbg_sensor(2, "%s: copy head (nr %d). index %d\n", __func__, ARRAY_SIZE(src_head), index);

	/* LSC */
	sensor_2x5_cis_copy_burstdata(src_lsc, burst_buf, &index, sz_lsc);
	dbg_sensor(2, "%s: copy lsc (nr %d). index %d\n", __func__, sz_lsc, index);

	sensor_2x5_cis_copy_ctrldata(src_xtalkaddr[0], burst_buf, &index, ARRAY_SIZE(src_xtalkaddr));
	dbg_sensor(2, "%s: copy xtalk addr (nr %d). index %d\n", __func__, ARRAY_SIZE(src_xtalkaddr), index);

	/* XTALK */
	sensor_2x5_cis_copy_burstdata(src_xtalk, burst_buf, &index, sz_xtalk);
	dbg_sensor(2, "%s: copy xtalk (nr %d). index %d\n", __func__, sz_xtalk, index);

	sensor_2x5_cis_copy_ctrldata(src_tail[0], burst_buf, &index, ARRAY_SIZE(src_tail));
	dbg_sensor(2, "%s: copy tail (nr %d). index %d\n", __func__, ARRAY_SIZE(src_tail), index);

#ifdef DEBUG_BURSTBUF_MEM
	info("%s: Last index %d, Addr 0x%p\n", __func__, burst_index, end_addr);
	if (index != (burst_index * 3) || burst_index != SENSOR_2X5_BURST_CAL_NR_RAW 
	  || &burst_cal_buf[BURST_BUF_SIZE] != end_addr) {
		err("check Burst Cal. Buf-Sz=%d, index=%d, burst_index=%d end addr 0x%p, 0x%p",
			BURST_BUF_SIZE, index, burst_index, &burst_cal_buf[BURST_BUF_SIZE], end_addr);
	}

	/*sensor_2x5_cis_print_burst_calbuf(burst_cal_buf, BURST_BUF_SIZE);*/
#endif
	info("%s: Complete!\n", __func__);

	return 0;
}

int sensor_2x5_cis_write_cal_burst(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	/* Create cal buffer for burst */
	sensor_2x5_cis_create_burst_cal(burst_cal_buf);

	ret = sensor_cis_set_registers(subdev, sensor_2x5_precal, sensor_2x5_precal_size);
	CHECK_ERR_GOTO(ret < 0, p_err, "set_registers(precal) fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_precal\n");

	/* Write burst cal */
	ret = sensor_cis_set_registers(subdev, burst_cal_buf, BURST_BUF_SIZE);
	CHECK_ERR_GOTO(ret < 0, p_err, "set_registers(burstcal) fail!");

	ret = sensor_cis_set_registers(subdev, sensor_2x5_postcal, sensor_2x5_postcal_size);
	CHECK_ERR_GOTO(ret < 0, p_err, "set_registers(postcal) fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_postcal\n");

	cal_done = true;

	info("[2x5] sensor cal done (burst)!\n");
	return 0;

p_err:
	return ret;
}
#endif /* SENSOR_2X5_CAL_BURST_WRITE*/

int sensor_2x5_cis_write_cal(struct v4l2_subdev *subdev)
{
	int i, ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct i2c_client *client;
	const int position = SENSOR_POSITION_FRONT;
	u16 cal_page_xtalk, cal_page_lsc, cal_offset_xtalk, cal_offset_lsc;
	u16 *xtalk_buf = NULL, *lsc_buf = NULL;
	char *cal_buf = NULL;
#ifdef CONFIG_VENDER_MCD_V2
	static struct fimc_is_rom_info *finfo = NULL;
#else
	err("write cal not implemented!");
	return -ENOSYS;
#endif

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

#ifdef CONFIG_VENDER_MCD_V2
	fimc_is_sec_get_sysfs_finfo_by_position(position, &finfo);
	fimc_is_sec_get_cal_buf(position, &cal_buf);
	dbg_sensor(1, "%s: sensor cal start addr1 0x%X, end addr1 0x%X, addr2 0x%X, end addr2 0x%X\n",
		__func__, finfo->sensor_cal_data_start_addr, finfo->sensor_cal_data_end_addr,
		*((u32 *)&cal_buf[0x18]), *((u32 *)&cal_buf[0x1C]));
#endif
	xtalk_buf = (u16 *)&cal_buf[finfo->sensor_cal_data_start_addr];
	lsc_buf = (u16 *)&cal_buf[finfo->sensor_cal_data_start_addr + SENSOR_2X5_CAL_XTALK_SIZE];

	ret = sensor_cis_set_registers(subdev, sensor_2x5_precal, sensor_2x5_precal_size);
	CHECK_ERR_GOTO(ret < 0, p_err, "set_registers(precal) fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_precal\n");

#ifdef SUPPORT_2X5_SENSOR_VARIATION
	if (last_version == false) {
		cal_page_xtalk = SENSOR_2X5_EVT0_XTALK_ADDR_PAGE;
		cal_offset_xtalk = SENSOR_2X5_EVT0_XTALK_ADDR_OFFSET;
		cal_page_lsc = SENSOR_2X5_EVT0_LSC_ADDR_PAGE;
		cal_offset_lsc = SENSOR_2X5_EVT0_LSC_ADDR_OFFSET;
	} else
#endif
	{
		cal_page_xtalk = SENSOR_2X5_XTALK_ADDR_PAGE;
		cal_offset_xtalk = SENSOR_2X5_XTALK_ADDR_OFFSET;
		cal_page_lsc = SENSOR_2X5_LSC_ADDR_PAGE;
		cal_offset_lsc = SENSOR_2X5_LSC_ADDR_OFFSET;
	}

	/* Write LSC */
	dbg_sensor(1, "%s: Write Cal LSC\n", __func__);
	ret = fimc_is_sensor_write16(client, SENSOR_2X5_W_DIR_PAGE, cal_page_lsc);
	/*dbg_sensor(1, "[   lsc] 0x%X, 0x%X\n", SENSOR_2X5_W_DIR_PAGE, cal_page_lsc); */
	CHECK_ERR_GOTO(ret < 0, p_err, "[lsc] i2c fail addr(%x, %x) ret = %d", SENSOR_2X5_W_DIR_PAGE, cal_page_lsc, ret);

	for (i = 0; i < SENSOR_2X5_CAL_LSC_SIZE / 2; i++) {
		ret = fimc_is_sensor_write16(client, cal_offset_lsc + (i * 2), cpu_to_be16(lsc_buf[i]));
		/*dbg_sensor(2, "[   lsc] %4d: 0x%X, 0x%04X\n", i, cal_offset_lsc + (i * 2), cpu_to_be16(lsc_buf[i])); */
		CHECK_ERR_GOTO(ret < 0, p_err, "[lsc] i2c fail addr(%x), val(%04x), ret = %d",
				cal_offset_lsc + (i * 2), cpu_to_be16(lsc_buf[i]), ret);
	}

	/* Write X-talk */
	dbg_sensor(1, "%s: Write Cal Xtalk\n", __func__);
	for (i = 0; i < SENSOR_2X5_CAL_XTALK_SIZE / 2; i++) {
		ret = fimc_is_sensor_write16(client, cal_offset_xtalk + (i * 2), cpu_to_be16(xtalk_buf[i]));
		/*dbg_sensor(2, "[xtalk] %4d: 0x%X, 0x%04X\n", i, cal_offset_xtalk + (i * 2), cpu_to_be16(xtalk_buf[i])); */
		CHECK_ERR_GOTO(ret < 0, p_err, "[xtalk] i2c fail addr(%x), val(%04x), ret = %d",
				cal_offset_xtalk + (i * 2), cpu_to_be16(xtalk_buf[i]), ret);
	}

	ret = sensor_cis_set_registers(subdev, sensor_2x5_postcal, sensor_2x5_postcal_size);
	CHECK_ERR_GOTO(ret < 0, p_err, "set_registers(postcal) fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_postcal\n");

	cal_done = true;

	info("[2x5] sensor cal done!\n");
	return 0;

p_err:
	return ret;
}
#endif /* SENSOR_2X5_CAL_UPLOAD */

int sensor_2x5_cis_set_i2c_datamode(struct v4l2_subdev *subdev, int mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	 if (mode >= DATAMODE_END) {
		err("Invalid data mode %d", mode);
		return -EINVAL;
	}

	if (mode == DATAMODE_1BYTE) {
		/* 1 byte */
		ret = fimc_is_sensor_write16(client, SENSOR_2X5_W_DIR_PAGE, 0x4000);
		CHECK_ERR_GOTO(ret < 0, p_err, "i2c fail addr(%x 0x4000) ret = %d", SENSOR_2X5_W_DIR_PAGE, ret);

		ret = fimc_is_sensor_write16(client, 0x6000, 0x0085);
		CHECK_ERR_GOTO(ret < 0, p_err, "i2c fail addr(0x6000 0x0085) ret = %d", ret);
	} else {
		/* 2 byte */
		ret = fimc_is_sensor_write16(client, SENSOR_2X5_W_DIR_PAGE, 0x4000);
		CHECK_ERR_GOTO(ret < 0, p_err, "i2c fail addr(%x 0x4000) ,ret = %d", SENSOR_2X5_W_DIR_PAGE, ret);

		ret = fimc_is_sensor_write16(client, 0x6000, 0x0005);
		CHECK_ERR_GOTO(ret < 0, p_err, "i2c fail addr(0x6000 0x0005) ret = %d", ret);
	}

p_err:
	return ret;
}

/* CIS OPS */
int sensor_2x5_cis_init(struct v4l2_subdev *subdev)
{
#ifdef SUPPORT_2X5_SENSOR_VARIATION
#ifdef CONFIG_VENDER_MCD_V2
	static struct fimc_is_rom_info *finfo = NULL;
#endif
	u8 version_id = 0;
#endif
	struct fimc_is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo = {NULL, 0};
	int ret = 0;
#ifdef USE_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
#endif

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	BUG_ON(!cis->cis_data);
	memset(cis->cis_data, 0, sizeof(cis_shared_data));
	cis->rev_flag = false;

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
		sensor_peri = container_of(cis, struct fimc_is_device_sensor_peri, cis);
		if (sensor_peri)
			fimc_is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
#endif
		warn("sensor_2x5_check_rev is fail when cis init");
		cis->rev_flag = true;
		ret = 0;
	}

	cis->cis_data->cur_width = SENSOR_2X5_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_2X5_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cal_done = false;

#ifdef SUPPORT_2X5_SENSOR_VARIATION
#ifdef CONFIG_VENDER_MCD_V2
	fimc_is_sec_get_sysfs_finfo_by_position(SENSOR_POSITION_FRONT, &finfo);
	version_id = finfo->rom_sensor_id[8];
#endif
	if (version_id >= SENSOR_2X5_VER_SP13 && version_id <= SENSOR_2X5_VER_SP13_END) {
		last_version = true;
	} else if (version_id == SENSOR_2X5_VER_SP03) {
		last_version = false;
		err(" ==== EVT0 module not suppoted ==== ");
		return -ENODEV;
	} else
#endif /* SUPPORT_2X5_SENSOR_VARIATION */
	{
		/* We assume last version if we couldn't get sersor version info */
		last_version = true;
#ifdef SUPPORT_2X5_SENSOR_VARIATION
		err("invalid version ID 0x%02X. Check module!!\n", version_id);
#endif
	}

	sensor_2x5_cis_data_calculation(sensor_2x5_pllinfos[setfile_index], cis->cis_data);

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

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2x5_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;

	BUG_ON(!subdev);

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

	pr_err("[SEN:DUMP] *******************************\n");
	ret = fimc_is_sensor_read16(client, 0x0000, &data16);
	if (unlikely(!ret)) printk("[SEN:DUMP] model_id(%x)\n", data16);
	ret = fimc_is_sensor_read8(client, 0x0002, &data8);
	if (unlikely(!ret)) printk("[SEN:DUMP] revision_number(%x)\n", data8);
	ret = fimc_is_sensor_read8(client, 0x0005, &data8);
	if (unlikely(!ret)) printk("[SEN:DUMP] frame_count(%x)\n", data8);
	ret = fimc_is_sensor_read8(client, 0x0100, &data8);
	if (unlikely(!ret)) printk("[SEN:DUMP] mode_select(%x)\n", data8);

	sensor_cis_dump_registers(subdev, sensor_2x5_setfiles[0], sensor_2x5_setfile_sizes[0]);
	pr_err("[SEN:DUMP] *******************************\n");

p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_2x5_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

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
static inline int sensor_2x5_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_2x5_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	ret = sensor_2x5_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	return ret;
}

int sensor_2x5_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* setfile global setting is at camera entrance */
	ret = sensor_cis_set_registers(subdev, sensor_2x5_global, sensor_2x5_global_size);
	CHECK_ERR_GOTO(ret < 0, p_err, "sensor_2x5_set_registers(global) fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_global\n");

	dbg_sensor(1, "[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_mode_change_evt0(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	int isp_mode = TETRA_ISP_24MP; /* default 24MP, for SP03 version */
	struct fimc_is_cis *cis = NULL;

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	if (mode > sensor_2x5_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	} else if (mode < MODE_2X5_REMOSAIC_START) {
		isp_mode = TETRA_ISP_6MP;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_2x5_check_rev is fail!");
			return ret;
		}
	}

	/* DSLIM: Check below */
	sensor_2x5_cis_data_calculation(sensor_2x5_pllinfos[mode], cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_cis_set_registers(subdev, sensor_2x5_setfiles[mode], sensor_2x5_setfile_sizes[mode]);
	CHECK_ERR_GOTO(ret < 0, p_err, "set_registers(mode) fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_setfiles mode %d\n", mode);

	/* Setting Tetra ISP in old revision */
	info("%s: Tetra ISP mode %d (EVT0)\n", __func__, isp_mode);
	ret = sensor_cis_set_registers(subdev, sensor_2x5_tetra_isp[isp_mode], sensor_2x5_tetra_isp_sizes[isp_mode]);
	CHECK_ERR_GOTO(ret < 0, p_err, "sensor_2x5_tetra_isp fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_tetra_isp %d\n", isp_mode);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s: mode changed(%d) EVT0\n", __func__, mode);

	return 0;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct fimc_is_cis *cis = NULL;
	struct i2c_client *client;
	bool cal = cal_done;

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (mode > sensor_2x5_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_cis_check_rev(cis);
		CHECK_ERR_RET(ret < 0, ret, "sensor_2x5_check_rev is fail!");
	}

	/* DSLIM: Check below */
	sensor_2x5_cis_data_calculation(sensor_2x5_pllinfos[mode], cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef SENSOR_2X5_CAL_UPLOAD
	if (cal_done == true) {
		ret = sensor_2x5_cis_set_i2c_datamode(subdev, DATAMODE_2BYTE);
		CHECK_ERR_GOTO(ret < 0, p_err_i2c, "set datamode2 fail!");
	}
#endif

	ret = sensor_cis_set_registers(subdev, sensor_2x5_setfiles[mode], sensor_2x5_setfile_sizes[mode]);
	CHECK_ERR_GOTO(ret < 0, p_err_i2c, "set_registers(mode) fail!");
	dbg_sensor(1, "[set_setfile]: sensor_2x5_setfiles mode %d\n", mode);

#ifdef SENSOR_2X5_CAL_UPLOAD
	if (cal_done == true) {
		ret = sensor_2x5_cis_set_i2c_datamode(subdev, DATAMODE_1BYTE);
		CHECK_ERR_GOTO(ret < 0, p_err_i2c, "set datamode1 fail!");
	} else {
		/* Write Sensor cal */
#if SENSOR_2X5_CAL_BURST_WRITE
		ret = sensor_2x5_cis_write_cal_burst(subdev);
#else
		ret = sensor_2x5_cis_write_cal(subdev);
#endif
		CHECK_ERR_GOTO(ret < 0, p_err_i2c, "set_registers(cal) fail!");
	}
#endif /* SENSOR_2X5_CAL_UPLOAD */

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s: mode changed(%d)%s\n", __func__, mode, (cal != cal_done) ? " with CAL": "");
	/*dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);*/

	return 0;

p_err_i2c:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_2x5_cis_mode_change_wrap(struct v4l2_subdev *subdev, u32 mode)
{
	if (last_version)
		return sensor_2x5_cis_mode_change(subdev, mode);
	else
		return sensor_2x5_cis_mode_change_evt0(subdev, mode); /* for EVT0 */
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_2x5_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
#if 1
	err("not implemented!!");
#else

	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x= 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct fimc_is_cis *cis = NULL;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif
	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);

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
	ret = sensor_2x5_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_2X5_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_2X5_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_2X5_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_2X5_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* 1. page_select */
	ret = fimc_is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		goto p_err;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_2X5_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_2X5_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
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
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed)) */
	/* down scale factor = down_scale_m / down_scale_n */
	ret = fimc_is_sensor_write16(client, 0x0404, 0x0010);
	if (ret < 0)
		goto p_err;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);


#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
#endif
	return ret;
}

int sensor_2x5_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	info("[2x5] start\n");
	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("[%s] sensor_2x5_cis_group_param_hold_func fail\n", __func__);

#ifdef DEBUG_2X5_PLL
	{
	u16 pll;
	ret = fimc_is_sensor_read16(client, 0x0300, &pll);
	dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x0302, &pll);
	dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x0304, &pll);
	dbg_sensor(1, "______ pre_pll_clk_div(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x0306, &pll);
	dbg_sensor(1, "______ pll_multiplier(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x0308, &pll);
	dbg_sensor(1, "______ op_pix_clk_div(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x030a, &pll);
	dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);

	ret = fimc_is_sensor_read16(client, 0x030c, &pll);
	dbg_sensor(1, "______ secnd_pre_pll_clk_div(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x030e, &pll);
	dbg_sensor(1, "______ secnd_pll_multiplier(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x0340, &pll);
	dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
	ret = fimc_is_sensor_read16(client, 0x0342, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

	/* Sensor stream on */
	ret = fimc_is_sensor_write16(client, 0x0100, 0x0100);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x0100, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2x5_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	/*dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);*/
	info("[2x5] stop\n");

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("[%s] sensor_2x5_cis_group_param_hold_func fail\n", __func__);

	/* Sensor stream off */
	ret |= fimc_is_sensor_write16(client, 0x0100, 0x0000);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x00, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	cis_data->stream_on = false;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2x5_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u16 middle_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!target_exposure);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

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

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d), middle(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count,
			target_exposure->long_val, target_exposure->short_val, target_exposure->middle_val);

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
	short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
	middle_coarse_int = ((target_exposure->middle_val * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;

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

	if (middle_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), middle coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, middle_coarse_int, cis_data->max_coarse_integration_time);
		middle_coarse_int = cis_data->max_coarse_integration_time;
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

	if (middle_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), middle coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, middle_coarse_int, cis_data->min_coarse_integration_time);
		middle_coarse_int = cis_data->min_coarse_integration_time;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	/* Short exposure */
	ret = fimc_is_sensor_write16(client, 0x0202, short_coarse_int);
	if (ret < 0)
		goto p_err;

	/* Long exposure */
	if (IS_3DHDR_MODE(cis, NULL) && cis_data->is_data.wdr_mode != CAMERA_WDR_OFF) {
		ret = fimc_is_sensor_write16(client, 0x0226, long_coarse_int);
		if (ret < 0)
			goto p_err;

		ret = fimc_is_sensor_write16(client, 0x022C, middle_coarse_int);
		if (ret < 0)
			goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_mhz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n", cis->id, __func__,
		cis_data->sen_vsync_count, vt_pic_clk_freq_mhz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "long_coarse_int %#x, short_coarse_int %#x, middle_coarse_int %#x\n", cis->id, __func__,
		cis_data->sen_vsync_count, cis_data->frame_length_lines,
		long_coarse_int, short_coarse_int, middle_coarse_int);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
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

	BUG_ON(!subdev);
	BUG_ON(!min_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

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

int sensor_2x5_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
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

	BUG_ON(!subdev);
	BUG_ON(!max_expo);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

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

int sensor_2x5_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
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

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!target_duration);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = ((vt_pic_clk_freq_mhz * input_exposure_time) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (frame_length_lines * line_length_pck) / vt_pic_clk_freq_mhz;

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_2x5_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_mhz * frame_duration) / line_length_pck);

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_mhz(%#x) frame_duration = %d us,"
		KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pic_clk_freq_mhz, frame_duration, line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_write16(client, 0x0340, frame_length_lines);
	if (ret < 0)
		goto p_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

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

	ret = sensor_2x5_cis_set_frame_duration(subdev, frame_duration);
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

int sensor_2x5_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
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

	BUG_ON(!subdev);
	BUG_ON(!target_permile);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0])
		again_code = cis_data->max_analog_gain[0];
	else if (again_code < cis_data->min_analog_gain[0])
		again_code = cis_data->min_analog_gain[0];

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_2x5_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
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

	BUG_ON(!subdev);
	BUG_ON(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0])
		analog_gain = cis->cis_data->min_analog_gain[0];

	if (analog_gain > cis->cis_data->max_analog_gain[0])
		analog_gain = cis->cis_data->max_analog_gain[0];

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->long_val, analog_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_write16(client, 0x0204, analog_gain);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
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

	BUG_ON(!subdev);
	BUG_ON(!again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_read16(client, 0x0204, &analog_gain);
	if (ret < 0)
		goto p_err;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 read_value = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!min_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = fimc_is_sensor_read16(client, 0x0084, &read_value);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0084, read_value, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->min_analog_gain[0] = read_value;
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(read_value);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2x5_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 read_value = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!max_again);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = fimc_is_sensor_read16(client, 0x0086, &read_value);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0086, read_value, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->max_analog_gain[0] = read_value;
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(read_value);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2x5_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;
	u16 middle_gain = 0;
	u16 dgains[4] = {0};

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);
	middle_gain = (u16)sensor_cis_calc_dgain_code(dgain->middle_val);

	if (long_gain < cis->cis_data->min_digital_gain[0])
		long_gain = cis->cis_data->min_digital_gain[0];

	if (long_gain > cis->cis_data->max_digital_gain[0])
		long_gain = cis->cis_data->max_digital_gain[0];

	if (short_gain < cis->cis_data->min_digital_gain[0])
		short_gain = cis->cis_data->min_digital_gain[0];

	if (short_gain > cis->cis_data->max_digital_gain[0])
		short_gain = cis->cis_data->max_digital_gain[0];

	if (middle_gain < cis->cis_data->min_digital_gain[0])
		middle_gain = cis->cis_data->min_digital_gain[0];

	if (middle_gain > cis->cis_data->max_digital_gain[0])
		middle_gain = cis->cis_data->max_digital_gain[0];

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d/%d us,"
		KERN_CONT "long_gain(%#x), short_gain(%#x), middle_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count,
			dgain->long_val, dgain->short_val, dgain->middle_val,
			long_gain, short_gain, middle_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	/* Short digital gain */
	dgains[0] = dgains[1] = dgains[2] = dgains[3] = short_gain;
	ret = fimc_is_sensor_write16(client, 0x020E, short_gain);
	if (ret < 0)
		goto p_err;

	/* Long digital gain */
	if (IS_3DHDR_MODE(cis, NULL) && cis_data->is_data.wdr_mode != CAMERA_WDR_OFF) {
		/* long digital gain */
		ret = fimc_is_sensor_write16(client, 0x0230, long_gain);
		if (ret < 0)
			goto p_err;

		/* middle digital gain */
		ret = fimc_is_sensor_write16(client, 0x0240, middle_gain);
		if (ret < 0)
			goto p_err;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
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

	BUG_ON(!subdev);
	BUG_ON(!dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_read16(client, 0x020E, &digital_gain);
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
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!min_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	/* 2X5 cannot read min/max digital gain */
	cis_data->min_digital_gain[0] = 0x0100;

	cis_data->min_digital_gain[1] = 1000;

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_2x5_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct fimc_is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);
	BUG_ON(!max_dgain);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	/* 2X5 cannot read min/max digital gain */
	cis_data->max_digital_gain[0] = 0x1000;

	cis_data->max_digital_gain[1] = 16000;

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_2x5_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	const u16 addr_page = SENSOR_2X5_ABS_GAIN_PAGE;
	const u16 addr_offset = SENSOR_2X5_ABS_GAIN_OFFSET;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	int i, ret = 0;
	int hold = 0;
	int mode = 0;
	u16 abs_gains[3] = {0, }; /* R, G, B */
	u32 avg_g = 0, div = 0;

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

	if (!cis->use_wb_gain || !last_version)
		return 0;

	mode = cis->cis_data->sens_config_index_cur;

	if (mode < MODE_2X5_REMOSAIC_START)
		return 0;

	if (wb_gains.gr != wb_gains.gb) {
		err("gr, gb not euqal"); /* check DDK layer */
		return -EINVAL;
	}

	if (wb_gains.gr == 1024)
		div = 4;
	else if (wb_gains.gr == 2048)
		div = 8;
	else {
		err("invalid gr,gb %d", wb_gains.gr); /* check DDK layer */
		return -EINVAL;
	}

	dbg_sensor(1, "[SEN:%d]%s: DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	avg_g = (wb_gains.gr + wb_gains.gb) / 2;
	abs_gains[0] = (u16)((wb_gains.r / div) & 0xFFFF);
	abs_gains[1] = (u16)((avg_g / div) & 0xFFFF);
	abs_gains[2] = (u16)((wb_gains.b / div) & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s: abs_gain_r(0x%4X), abs_gain_g(0x%4X), abs_gain_b(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2]);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	ret = fimc_is_sensor_write16(client, SENSOR_2X5_W_DIR_PAGE, addr_page);
	CHECK_ERR_GOTO(ret < 0, p_err, "i2c fail addr(%x, %x) ,ret = %d", SENSOR_2X5_W_DIR_PAGE, addr_page, ret);

	for (i = 0; i < ARRAY_SIZE(abs_gains); i++) {
		ret = fimc_is_sensor_write16(client, addr_offset + (i * 2), abs_gains[i]);
		dbg_sensor(2, "[wbgain] %d: 0x%04X, 0x%04X\n", i, addr_offset + (i * 2), abs_gains[i]);
		CHECK_ERR_GOTO(ret < 0, p_err, "i2c fail addr(%x), val(%04x), ret = %d\n",
				addr_offset + (i * 2), abs_gains[i], ret);
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

#ifdef SUPPORT_SENSOR_3DHDR
int sensor_2x5_cis_set_3hdr_roi(struct v4l2_subdev *subdev, struct roi_setting_t roi_control)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	u16 roi_val[4];
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	BUG_ON(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	if (!IS_3DHDR_MODE(cis, NULL)) {
		err("can not set roi in none-3dhdr");
		return 0;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(1, "%s: [MOD:%d] roi_control (start_x:%d, start_y:%d, end_x:%d, end_y:%d)\n",
		__func__, cis->id,
		roi_control.roi_start_x, roi_control.roi_start_y,
		roi_control.roi_end_x, roi_control.roi_end_y);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	/* 0x2000_XXXX */
	ret = fimc_is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		goto p_err;

	/* t_isp_rgby_hist_mem_cfg_active_window_percent_14bit */
	roi_val[0] = roi_control.roi_start_x; /* 0x20004AC4: top_left_x */
	roi_val[1] = roi_control.roi_start_y; /* 0x20004AC6: top_left_y */
	roi_val[2] = roi_control.roi_end_x; /* 0x20004AC8: bot_right_x */
	roi_val[3] = roi_control.roi_end_y; /* 0x20004ACA: bot_right_y */

	ret = fimc_is_sensor_write16_array(client, 0x4AC4, roi_val, 4);
	if (ret < 0)
		goto p_err;

	/* t_isp_rgby_hist_grid_grid & thstat_grid_area */
	roi_val[0] = roi_control.roi_end_x - roi_control.roi_start_x; /* 0x20004B8C & 0x20004DE0: width */
	roi_val[1] = roi_control.roi_end_y - roi_control.roi_start_y; /* 0x20004B8E & 0x20004DE2: height */
	roi_val[2] = (roi_control.roi_start_x + roi_control.roi_end_x) / 2; /* center_x */
	roi_val[3] = (roi_control.roi_start_y + roi_control.roi_end_y) / 2; /* center_y */

	ret = fimc_is_sensor_write16_array(client, 0x4B8C, roi_val, 4);
	if (ret < 0)
		goto p_err;

	ret = fimc_is_sensor_write16_array(client, 0x4DE0, roi_val, 2);
	if (ret < 0)
		goto p_err;

	/* t_isp_rgby_hist_mem_cfg_b_win_enable */
	ret = fimc_is_sensor_write16(client, 0x4AC2, 0x0000);
	if (ret < 0)
		goto p_err;

	/* restore 0x4000_XXXX */
	ret = fimc_is_sensor_write16(client, 0x6028, 0x4000);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2x5_cis_set_3hdr_stat(struct v4l2_subdev *subdev, bool streaming, void *data)
{
	int ret = 0;
	int hold = 0;
	struct fimc_is_cis *cis;
	struct i2c_client *client;
	u16 weight[3];
	u16 low_gate_thr, high_gate_thr;
	struct roi_setting_t y_sum_roi;
	struct sensor_lsi_3hdr_stat_control_mode_change mode_change_stat;
	struct sensor_lsi_3hdr_stat_control_per_frame per_frame_stat;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!data);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	if (!IS_3DHDR_MODE(cis, NULL)) {
		err("can not set stat in none-3dhdr");
		return 0;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	if (streaming) {
		per_frame_stat = *(struct sensor_lsi_3hdr_stat_control_per_frame *)data;

		weight[0] = per_frame_stat.r_weight;
		weight[1] = per_frame_stat.g_weight;
		weight[2] = per_frame_stat.b_weight;
	} else {
		mode_change_stat = *(struct sensor_lsi_3hdr_stat_control_mode_change *)data;

		weight[0] = mode_change_stat.r_weight;
		weight[1] = mode_change_stat.g_weight;
		weight[2] = mode_change_stat.b_weight;

		low_gate_thr = mode_change_stat.low_gate_thr;
		high_gate_thr = mode_change_stat.high_gate_thr;

		y_sum_roi = mode_change_stat.y_sum_roi;
	}

	ret = fimc_is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		goto p_err;

	/* t_isp_rgby_hist_short_exp_weight */
	ret = fimc_is_sensor_write16_array(client, 0x4B5A, weight, 3);
	if (ret < 0)
		goto p_err;

	/* t_isp_rgby_hist_long_exp_weight */
	ret = fimc_is_sensor_write16_array(client, 0x4B68, weight, 3);
	if (ret < 0)
		goto p_err;

	/* t_isp_rgby_hist_medium_exp_weight */
	ret = fimc_is_sensor_write16_array(client, 0x4B76, weight, 3);
	if (ret < 0)
		goto p_err;

	/* t_isp_rgby_hist_mixed_exp_weight */
	ret = fimc_is_sensor_write16_array(client, 0x4B84, weight, 3);
	if (ret < 0)
		goto p_err;

	/* t_isp_drc_thstat_rgb_weights */
	ret = fimc_is_sensor_write16_array(client, 0x4DCA, weight, 3);
	if (ret < 0)
		goto p_err;

	if (!streaming) {
		/* t_isp_drc_thstat_u_low_tresh_red */
		ret = fimc_is_sensor_write16(client, 0x4DB8, low_gate_thr);
		if (ret < 0)
			goto p_err;
		/* t_isp_drc_thstat_u_high_tresh_red */
		ret = fimc_is_sensor_write16(client, 0x4DBA, high_gate_thr);
		if (ret < 0)
			goto p_err;

		/* t_isp_drc_thstat_u_low_tresh_green */
		ret = fimc_is_sensor_write16(client, 0x4DBC, low_gate_thr);
		if (ret < 0)
			goto p_err;

		/* t_isp_drc_thstat_u_high_tresh_green */
		ret = fimc_is_sensor_write16(client, 0x4DBE, high_gate_thr);
		if (ret < 0)
			goto p_err;

		/* t_isp_drc_thstat_u_low_tresh_blue */
		ret = fimc_is_sensor_write16(client, 0x4DC0, low_gate_thr);
		if (ret < 0)
			goto p_err;

		/* t_isp_drc_thstat_u_high_tresh_blue */
		ret = fimc_is_sensor_write16(client, 0x4DC2, high_gate_thr);
		if (ret < 0)
			goto p_err;

		/* t_isp_y_sum_top_left_x */
		ret = fimc_is_sensor_write16(client, 0x4DA4, y_sum_roi.roi_start_x);
		if (ret < 0)
			goto p_err;
		ret = fimc_is_sensor_write16(client, 0x4DA6, y_sum_roi.roi_start_y);
		if (ret < 0)
			goto p_err;
	}

	/* restore 0x4000_XXXX */
	ret = fimc_is_sensor_write16(client, 0x6028, 0x4000);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	if (hold > 0) {
		hold = sensor_2x5_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

void sensor_2x5_cis_check_wdr_mode(struct v4l2_subdev *subdev, u32 mode_idx)
{
	struct fimc_is_cis *cis;

	FIMC_BUG_VOID(!subdev);

	cis = (struct fimc_is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG_VOID(!cis);
	FIMC_BUG_VOID(!cis->cis_data);

	/* check wdr mode */
	if (IS_3DHDR_MODE(cis, &mode_idx))
		cis->cis_data->is_data.wdr_enable = true;
	else
		cis->cis_data->is_data.wdr_enable = false;

	dbg_sensor(1, "[%s] wdr_enable: %d\n", __func__,
				cis->cis_data->is_data.wdr_enable);
}
#endif /* SUPPORT_SENSOR_3DHDR */

static struct fimc_is_cis_ops cis_ops = {
	.cis_init = sensor_2x5_cis_init,
	.cis_log_status = sensor_2x5_cis_log_status,
	.cis_group_param_hold = sensor_2x5_cis_group_param_hold,
	.cis_set_global_setting = sensor_2x5_cis_set_global_setting,
	.cis_mode_change = sensor_2x5_cis_mode_change_wrap,
	.cis_set_size = sensor_2x5_cis_set_size,
	.cis_stream_on = sensor_2x5_cis_stream_on,
	.cis_stream_off = sensor_2x5_cis_stream_off,
	.cis_set_exposure_time = sensor_2x5_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_2x5_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_2x5_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_2x5_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_2x5_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_2x5_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_2x5_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_2x5_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_2x5_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_2x5_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_2x5_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_2x5_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_2x5_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_2x5_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_2x5_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_set_wb_gains = sensor_2x5_cis_set_wb_gain,
#ifdef SUPPORT_SENSOR_3DHDR
	.cis_set_roi_stat = sensor_2x5_cis_set_3hdr_roi,
	.cis_set_3hdr_stat = sensor_2x5_cis_set_3hdr_stat,
	.cis_check_wdr_mode = sensor_2x5_cis_check_wdr_mode,
#endif
};

int cis_2x5_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	bool use_pdaf = false;

	struct fimc_is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct fimc_is_cis *cis = NULL;
	struct fimc_is_device_sensor *device = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id = 0;
	char const *setfile;
	struct device *dev;
	struct device_node *dnode;

	BUG_ON(!client);
	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	if (of_property_read_bool(dnode, "use_pdaf"))
		use_pdaf = true;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("sensor id read is fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s sensor id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];

	sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_S5K2X5);
	if (!sensor_peri) {
		probe_info("sensor peri is not yet probed");
		return -EPROBE_DEFER;
	}

	cis = &sensor_peri->cis;
	if (!cis) {
		err("cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_cis) {
		probe_err("subdev_cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_cis = subdev_cis;

	cis->id = SENSOR_NAME_S5K2X5;
	cis->subdev = subdev_cis;
	cis->device = 0;
	cis->client = client;
	sensor_peri->module->client = cis->client;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;

	cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
	if (!cis->cis_data) {
		err("cis_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	cis->cis_ops = &cis_ops;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	if (of_property_read_bool(dnode, "sensor_f_number")) {
		ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
		if (ret) {
			warn("f-number read is fail(%d)",ret);
		}
	} else {
		cis->aperture_num = F1_9;
	}

	probe_info("%s f-number %d\n", __func__, cis->aperture_num);

	cis->use_dgain = true;
	cis->hdr_ctrl_by_again = false;
	cis->use_wb_gain = true;
	cis->use_3hdr = false;
	cis->use_pdaf = false; /* cis->use_pdaf =  use_pdaf */

	cis->use_initial_ae = of_property_read_bool(dnode, "use_initial_ae");
	probe_info("%s use initial_ae(%d)\n", __func__, cis->use_initial_ae);

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		info("%s : EVT1 setfile_A\n", __func__);
		sensor_2x5_global = sensor_2x5_setfile_A_Global;
		sensor_2x5_global_size = ARRAY_SIZE(sensor_2x5_setfile_A_Global);
		sensor_2x5_setfiles = sensor_2x5_setfiles_A;
		sensor_2x5_setfile_sizes = sensor_2x5_setfile_A_sizes;
		sensor_2x5_max_setfile_num = ARRAY_SIZE(sensor_2x5_setfiles_A);
		sensor_2x5_precal = sensor_2x5_setfile_A_pre_cal;
		sensor_2x5_precal_size = ARRAY_SIZE(sensor_2x5_setfile_A_pre_cal);
		sensor_2x5_postcal = sensor_2x5_setfile_A_post_cal;
		sensor_2x5_postcal_size = ARRAY_SIZE(sensor_2x5_setfile_A_post_cal);
		sensor_2x5_pllinfos = sensor_2x5_pllinfos_A;
	} else if (strcmp(setfile, "setB") == 0) {
		info("%s : EVT1 setfile_B for 3DHDR\n", __func__);
#ifdef SUPPORT_SENSOR_3DHDR
		cis->use_3hdr = of_property_read_bool(dnode, "use_3dhdr");
#endif
#if SENSOR_2X5_BURST_WRITE
		sensor_2x5_global = sensor_2x5_setfile_B_GlobalBurst;
		sensor_2x5_global_size = ARRAY_SIZE(sensor_2x5_setfile_B_GlobalBurst);
#else
		sensor_2x5_global = sensor_2x5_setfile_B_Global;
		sensor_2x5_global_size = ARRAY_SIZE(sensor_2x5_setfile_B_Global);
#endif
		sensor_2x5_setfiles = sensor_2x5_setfiles_B;
		sensor_2x5_setfile_sizes = sensor_2x5_setfile_B_sizes;
		sensor_2x5_max_setfile_num = ARRAY_SIZE(sensor_2x5_setfiles_B);
		sensor_2x5_precal = sensor_2x5_setfile_B_pre_cal;
		sensor_2x5_precal_size = ARRAY_SIZE(sensor_2x5_setfile_B_pre_cal);
		sensor_2x5_postcal = sensor_2x5_setfile_B_post_cal;
		sensor_2x5_postcal_size = ARRAY_SIZE(sensor_2x5_setfile_B_post_cal);
		sensor_2x5_pllinfos = sensor_2x5_pllinfos_B;
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_2x5_global = sensor_2x5_setfile_A_Global;
		sensor_2x5_global_size = ARRAY_SIZE(sensor_2x5_setfile_A_Global);
		sensor_2x5_setfiles = sensor_2x5_setfiles_A;
		sensor_2x5_setfile_sizes = sensor_2x5_setfile_A_sizes;
		sensor_2x5_max_setfile_num = ARRAY_SIZE(sensor_2x5_setfiles_A);
		sensor_2x5_precal = sensor_2x5_setfile_A_pre_cal;
		sensor_2x5_precal_size = ARRAY_SIZE(sensor_2x5_setfile_A_pre_cal);
		sensor_2x5_postcal = sensor_2x5_setfile_A_post_cal;
		sensor_2x5_postcal_size = ARRAY_SIZE(sensor_2x5_setfile_A_post_cal);
		sensor_2x5_pllinfos = sensor_2x5_pllinfos_A;
	}

	v4l2_i2c_subdev_init(subdev_cis, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_cis, cis);
	v4l2_set_subdev_hostdata(subdev_cis, device);
	snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static int cis_2x5_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

static const struct of_device_id exynos_fimc_is_cis_2x5_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-cis-2x5",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_cis_2x5_match);

static const struct i2c_device_id cis_2x5_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver cis_2x5_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = exynos_fimc_is_cis_2x5_match
	},
	.probe	= cis_2x5_probe,
	.remove	= cis_2x5_remove,
	.id_table = cis_2x5_idt
};
module_i2c_driver(cis_2x5_driver);
