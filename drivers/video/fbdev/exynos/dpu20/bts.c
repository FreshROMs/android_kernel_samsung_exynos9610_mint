 /*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * BTS file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "decon.h"
#include "dpp.h"

#include <soc/samsung/bts.h>
#include <media/v4l2-subdev.h>
#if defined(CONFIG_CAL_IF)
#include <soc/samsung/cal-if.h>
#endif
#if defined(CONFIG_SOC_EXYNOS9610)
#include <dt-bindings/clock/exynos9610.h>
#endif

#define DISP_FACTOR		100UL
#define LCD_REFRESH_RATE	63UL
#define MULTI_FACTOR 		(1UL << 10)

u64 dpu_bts_calc_aclk_disp(struct decon_device *decon,
		struct decon_win_config *config, u64 resol_clock)
{
	u64 s_ratio_h, s_ratio_v;
	u64 aclk_disp;
	u64 ppc;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;

	s_ratio_h = (src->w <= dst->w) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src->w / (u64)dst->w;
	s_ratio_v = (src->h <= dst->h) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src->h / (u64)dst->h;

	/* case for using dsc encoder 1ea at decon0 or decon1 */
	if ((decon->id != 2) && (decon->lcd_info->dsc_cnt == 1))
		ppc = ((decon->bts.ppc / 2UL) >= 1UL) ? (decon->bts.ppc / 2UL) : 1UL;
	else
		ppc = decon->bts.ppc;

	aclk_disp = resol_clock * s_ratio_h * s_ratio_v * DISP_FACTOR  / 100UL
		/ ppc * (MULTI_FACTOR * (u64)dst->w / (u64)decon->lcd_info->xres)
		/ (MULTI_FACTOR * MULTI_FACTOR * MULTI_FACTOR);

	if (aclk_disp < (resol_clock / ppc))
		aclk_disp = resol_clock / ppc;

	return aclk_disp;
}

static void dpu_bts_sum_all_decon_bw(struct decon_device *decon, u32 ch_bw[])
{
	int i, j;

	if (decon->id < 0 || decon->id >= decon->dt.decon_cnt) {
		decon_warn("[%s] undefined decon id(%d)!\n", __func__, decon->id);
		return;
	}

	for (i = 0; i < BTS_DPU_MAX; ++i)
		decon->bts.ch_bw[decon->id][i] = ch_bw[i];

	for (i = 0; i < decon->dt.decon_cnt; ++i) {
		if (decon->id == i)
			continue;

		for (j = 0; j < BTS_DPU_MAX; ++j)
			ch_bw[j] += decon->bts.ch_bw[i][j];
	}
}

/* bus utilization 75% */
#define BUS_UTIL	75

static void dpu_bts_find_max_disp_freq(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i, j, idx;
	u32 disp_ch_bw[BTS_DPU_MAX];
	u32 max_disp_ch_bw;
	u32 disp_op_freq = 0, freq = 0;
	u64 resol_clock;
	u64 op_fps = LCD_REFRESH_RATE;
	struct decon_win_config *config = regs->dpp_config;

	memset(disp_ch_bw, 0, sizeof(disp_ch_bw));

	for (i = 0; i < BTS_DPP_MAX; ++i)
		for (j = 0; j < BTS_DPU_MAX; ++j)
			if (decon->bts.bw[i].ch_num == j)
				disp_ch_bw[j] += decon->bts.bw[i].val;

	/* must be considered other decon's bw */
	dpu_bts_sum_all_decon_bw(decon, disp_ch_bw);

	for (i = 0; i < BTS_DPU_MAX; ++i)
		if (disp_ch_bw[i])
			DPU_DEBUG_BTS("\tCH%d = %d\n", i, disp_ch_bw[i]);

	max_disp_ch_bw = disp_ch_bw[0];
	for (i = 1; i < BTS_DPU_MAX; ++i)
		if (max_disp_ch_bw < disp_ch_bw[i])
			max_disp_ch_bw = disp_ch_bw[i];

	decon->bts.peak = max_disp_ch_bw;
	decon->bts.max_disp_freq = max_disp_ch_bw * 100 / (16 * BUS_UTIL) + 1;

	if (decon->dt.out_type == DECON_OUT_DP)
		op_fps = decon->lcd_info->fps;

	/* 1.1: 10% margin, 1000: for KHZ, 1: for raising to a unit */
	resol_clock = decon->lcd_info->xres * decon->lcd_info->yres *
		op_fps * 11 / 10 / 1000 + 1;
	decon->bts.resol_clk = resol_clock;

	DPU_DEBUG_BTS("\tDECON%d : resol clock = %d Khz\n",
		decon->id, decon->bts.resol_clk);

	for (i = 0; i < decon->dt.max_win; ++i) {
		idx = config[i].idma_type;
		if ((config[i].state != DECON_WIN_STATE_BUFFER) &&
				(config[i].state != DECON_WIN_STATE_COLOR))
			continue;

		freq = dpu_bts_calc_aclk_disp(decon, &config[i], resol_clock);
		if (disp_op_freq < freq)
			disp_op_freq = freq;
	}

	DPU_DEBUG_BTS("\tDISP bus freq(%d), operating freq(%d)\n",
			decon->bts.max_disp_freq, disp_op_freq);

	if (decon->bts.max_disp_freq < disp_op_freq)
		decon->bts.max_disp_freq = disp_op_freq;

	DPU_DEBUG_BTS("\tMAX DISP CH FREQ = %d\n", decon->bts.max_disp_freq);
}

static void dpu_bts_share_bw_info(int id)
{
	int i, j;
	struct decon_device *decon[3];
	int decon_cnt;

	decon_cnt = get_decon_drvdata(0)->dt.decon_cnt;

	for (i = 0; i < MAX_DECON_CNT; i++)
		decon[i] = NULL;

	for (i = 0; i < decon_cnt; i++)
		decon[i] = get_decon_drvdata(i);

	for (i = 0; i < decon_cnt; ++i) {
		if (id == i || decon[i] == NULL)
			continue;

		for (j = 0; j < BTS_DPU_MAX; ++j)
			decon[i]->bts.ch_bw[id][j] = decon[id]->bts.ch_bw[id][j];
	}
}

void dpu_bts_calc_bw(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct decon_win_config *config = regs->dpp_config;
	struct bts_decon_info bts_info;
	enum dpp_rotate rot;
	int idx, i;

	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS("\n");
	DPU_DEBUG_BTS("%s + : DECON%d\n", __func__, decon->id);

	memset(&bts_info, 0, sizeof(struct bts_decon_info));
	for (i = 0; i < decon->dt.max_win; ++i) {
		if (config[i].state == DECON_WIN_STATE_BUFFER) {
			idx = config[i].idma_type;
			bts_info.dpp[idx].used = true;
		} else {
			continue;
		}

		bts_info.dpp[idx].bpp = dpu_get_bpp(config[i].format);
		bts_info.dpp[idx].src_w = config[i].src.w;
		bts_info.dpp[idx].src_h = config[i].src.h;
		bts_info.dpp[idx].dst.x1 = config[i].dst.x;
		bts_info.dpp[idx].dst.x2 = config[i].dst.x + config[i].dst.w;
		bts_info.dpp[idx].dst.y1 = config[i].dst.y;
		bts_info.dpp[idx].dst.y2 = config[i].dst.y + config[i].dst.h;
		rot = config[i].dpp_parm.rot;
		bts_info.dpp[idx].rotation = (rot > DPP_ROT_180) ? true : false;

		DPU_DEBUG_BTS("\tDPP%d : bpp(%d) src w(%d) h(%d) rot(%d)\n",
				idx, bts_info.dpp[idx].bpp,
				bts_info.dpp[idx].src_w, bts_info.dpp[idx].src_h,
				bts_info.dpp[idx].rotation);
		DPU_DEBUG_BTS("\t\t\t\tdst x(%d) right(%d) y(%d) bottom(%d)\n",
				bts_info.dpp[idx].dst.x1,
				bts_info.dpp[idx].dst.x2,
				bts_info.dpp[idx].dst.y1,
				bts_info.dpp[idx].dst.y2);
	}

	bts_info.vclk = decon->bts.resol_clk;
	bts_info.lcd_w = decon->lcd_info->xres;
	bts_info.lcd_h = decon->lcd_info->yres;
	decon->bts.total_bw = bts_calc_bw(decon->bts.type, &bts_info);
	memcpy(&decon->bts.bts_info, &bts_info, sizeof(struct bts_decon_info));

	for (i = 0; i < BTS_DPP_MAX; ++i) {
		decon->bts.bw[i].val = bts_info.dpp[i].bw;
		if (decon->bts.bw[i].val)
			DPU_DEBUG_BTS("\tDPP%d bandwidth = %d\n",
					i, decon->bts.bw[i].val);
	}

	DPU_DEBUG_BTS("\tDECON%d total bandwidth = %d\n", decon->id,
			decon->bts.total_bw);

	dpu_bts_find_max_disp_freq(decon, regs);

	/* update bw for other decons */
	dpu_bts_share_bw_info(decon->id);

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_update_bw(struct decon_device *decon, struct decon_reg_data *regs,
		u32 is_after)
{
	struct bts_bw bw = { 0, };
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	struct displayport_device *displayport = get_displayport_drvdata();
	videoformat cur = displayport->cur_video;
	__u64 pixelclock = supported_videos[cur].dv_timings.bt.pixelclock;
#endif

	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	/* update peak & read bandwidth per DPU port */
	bw.peak = decon->bts.peak;
	bw.read = decon->bts.total_bw;
	DPU_DEBUG_BTS("\tpeak = %d, read = %d\n", bw.peak, bw.read);

	if (bw.read == 0)
		bw.peak = 0;

	if (is_after) { /* after DECON h/w configuration */
		if (decon->bts.total_bw <= decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.type, bw);

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		if ((displayport->state == DISPLAYPORT_STATE_ON)
			&& (pixelclock >= 533000000)) /* 4K DP case */
			return;
#endif

		if (decon->bts.max_disp_freq <= decon->bts.prev_max_disp_freq)
			pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);

		decon->bts.prev_total_bw = decon->bts.total_bw;
		decon->bts.prev_max_disp_freq = decon->bts.max_disp_freq;
	} else {
		if (decon->bts.total_bw > decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.type, bw);

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		if ((displayport->state == DISPLAYPORT_STATE_ON)
			&& (pixelclock >= 533000000)) /* 4K DP case */
			return;
#endif

		if (decon->bts.max_disp_freq > decon->bts.prev_max_disp_freq)
			pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);
	}

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_acquire_bw(struct decon_device *decon)
{
#if defined(CONFIG_DECON_BTS_LEGACY) && defined(CONFIG_EXYNOS_DISPLAYPORT)
	struct displayport_device *displayport = get_displayport_drvdata();
	videoformat cur = displayport->cur_video;
	__u64 pixelclock = supported_videos[cur].dv_timings.bt.pixelclock;
#endif
	struct decon_win_config config;
	u64 resol_clock;
	u32 aclk_freq = 0;

	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	if (decon->dt.out_type == DECON_OUT_DSI) {
		memset(&config, 0, sizeof(struct decon_win_config));
		config.src.w = config.dst.w = decon->lcd_info->xres;
		config.src.h = config.dst.h = decon->lcd_info->yres;
		resol_clock = decon->lcd_info->xres * decon->lcd_info->yres *
			LCD_REFRESH_RATE * 11 / 10 / 1000 + 1;
		aclk_freq = dpu_bts_calc_aclk_disp(decon, &config, resol_clock);
		DPU_DEBUG_BTS("Initial calculated disp freq(%lu)\n", aclk_freq);
		/*
		 * If current disp freq is higher than calculated freq,
		 * it must not be set. if not, underrun can occur.
		 */
		if (cal_dfs_get_rate(ACPM_DVFS_DISP) < aclk_freq)
			pm_qos_update_request(&decon->bts.disp_qos, aclk_freq);

		DPU_DEBUG_BTS("Get initial disp freq(%lu)\n",
				cal_dfs_get_rate(ACPM_DVFS_DISP));

		return;
	}

#if defined(CONFIG_DECON_BTS_LEGACY) && defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (decon->dt.out_type != DECON_OUT_DP)
		return;

	if (pixelclock >= 533000000) {
		if (pm_qos_request_active(&decon->bts.mif_qos))
			pm_qos_update_request(&decon->bts.mif_qos, 1794 * 1000);
		else
			DPU_ERR_BTS("%s mif qos setting error\n", __func__);

		if (pm_qos_request_active(&decon->bts.int_qos))
			pm_qos_update_request(&decon->bts.int_qos, 534 * 1000);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		if (pm_qos_request_active(&decon->bts.disp_qos))
			pm_qos_update_request(&decon->bts.disp_qos, 400 * 1000);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		if (!decon->bts.scen_updated) {
			decon->bts.scen_updated = 1;
			bts_update_scen(BS_DP_DEFAULT, 1);
		}
	} else if (pixelclock > 148500000) { /* pixelclock < 533000000 ? */
		if (pm_qos_request_active(&decon->bts.mif_qos))
			pm_qos_update_request(&decon->bts.mif_qos, 1352 * 1000);
		else
			DPU_ERR_BTS("%s mif qos setting error\n", __func__);
	} /* pixelclock <= 148500000 ? */

	DPU_DEBUG_BTS("%s: decon%d, pixelclock(%u)\n", __func__, decon->id,
			pixelclock);
#endif
}

void dpu_bts_release_bw(struct decon_device *decon)
{
	struct bts_bw bw = { 0, };
	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	if (decon->dt.out_type == DECON_OUT_DSI) {
		bts_update_bw(decon->bts.type, bw);
		decon->bts.prev_total_bw = 0;
		pm_qos_update_request(&decon->bts.disp_qos, 0);
		decon->bts.prev_max_disp_freq = 0;
	} else if (decon->dt.out_type == DECON_OUT_DP) {
#if defined(CONFIG_DECON_BTS_LEGACY) && defined(CONFIG_EXYNOS_DISPLAYPORT)
		if (pm_qos_request_active(&decon->bts.mif_qos))
			pm_qos_update_request(&decon->bts.mif_qos, 0);
		else
			DPU_ERR_BTS("%s mif qos setting error\n", __func__);

		if (pm_qos_request_active(&decon->bts.int_qos))
			pm_qos_update_request(&decon->bts.int_qos, 0);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		if (pm_qos_request_active(&decon->bts.disp_qos))
			pm_qos_update_request(&decon->bts.disp_qos, 0);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		if (decon->bts.scen_updated) {
			decon->bts.scen_updated = 0;
			bts_update_scen(BS_DP_DEFAULT, 0);
		}
#endif
	}

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_init(struct decon_device *decon)
{
	int comp_ratio;
	int i;
	struct v4l2_subdev *sd = NULL;

	DPU_DEBUG_BTS("%s +\n", __func__);

	decon->bts.enabled = false;

	if (!IS_ENABLED(CONFIG_EXYNOS_BTS)) {
		DPU_ERR_BTS("decon%d bts feature is disabled\n", decon->id);
		return;
	}

	if (decon->id == 1)
		decon->bts.type = BTS_BW_DECON1;
	else if (decon->id == 2)
		decon->bts.type = BTS_BW_DECON2;
	else
		decon->bts.type = BTS_BW_DECON0;

	for (i = 0; i < BTS_DPU_MAX; i++)
		decon->bts.ch_bw[decon->id][i] = 0;

	DPU_DEBUG_BTS("BTS_BW_TYPE(%d) -\n", decon->bts.type);

	if (decon->lcd_info->dsc_enabled)
		comp_ratio = 3;
	else
		comp_ratio = 1;

	if (decon->dt.out_type == DECON_OUT_DP) {
		/*
		* Decon2-DP : various resolutions are available
		* therefore, set max resolution clock at init phase to avoid underrun
		*/
		decon->bts.resol_clk = (u32)((u64)4096 * 2160 * 60 * 11
				/ 10 / 1000 + 1);
	} else {
		/*
		 * Resol clock(KHZ) = lcd width x lcd height x 63(refresh rate) x
		 *               1.1(10% margin) x comp_ratio(1/3 DSC) / 2(2PPC) /
		 *		1000(for KHZ) + 1(for raising to a unit)
		 */
		decon->bts.resol_clk = (u32)((u64)decon->lcd_info->xres *
				(u64)decon->lcd_info->yres *
				LCD_REFRESH_RATE * 11 / 10 / 1000 + 1);
	}
	DPU_DEBUG_BTS("[Init: D%d] resol clock = %d Khz\n",
		decon->id, decon->bts.resol_clk);

	pm_qos_add_request(&decon->bts.mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&decon->bts.int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
	pm_qos_add_request(&decon->bts.disp_qos, PM_QOS_DISPLAY_THROUGHPUT, 0);
	decon->bts.scen_updated = 0;

	for (i = 0; i < BTS_DPP_MAX; ++i) {
		sd = decon->dpp_sd[DPU_DMA2CH(i)];
		v4l2_subdev_call(sd, core, ioctl, DPP_GET_PORT_NUM,
				&decon->bts.bw[i].ch_num);
		DPU_INFO_BTS("IDMA_TYPE(%d) CH(%d) Port(%d)\n", i,
				DPU_DMA2CH(i), decon->bts.bw[i].ch_num);
	}

	decon->bts.enabled = true;

	DPU_INFO_BTS("decon%d bts feature is enabled\n", decon->id);
}

void dpu_bts_deinit(struct decon_device *decon)
{
	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS("%s +\n", __func__);
	pm_qos_remove_request(&decon->bts.disp_qos);
	pm_qos_remove_request(&decon->bts.int_qos);
	pm_qos_remove_request(&decon->bts.mif_qos);
	DPU_DEBUG_BTS("%s -\n", __func__);
}

struct decon_bts_ops decon_bts_control = {
	.bts_init		= dpu_bts_init,
	.bts_calc_bw		= dpu_bts_calc_bw,
	.bts_update_bw		= dpu_bts_update_bw,
	.bts_acquire_bw		= dpu_bts_acquire_bw,
	.bts_release_bw		= dpu_bts_release_bw,
	.bts_deinit		= dpu_bts_deinit,
};
