/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <asm/uaccess.h>

#include <soc/samsung/bts.h>
#include "cal_bts9610.h"

#define BTS_DBG(x...)				\
do {						\
	if (exynos_bts_log)			\
		pr_info(x);			\
} while (0)

#define MIF_UTIL		65

#define DEFAULT_QMAX_R		0x12
#define DEFAULT_QMAX_W		0x3
#define NO_QMAX			0x0

static int exynos_bts_log;
static unsigned int exynos_mif_util = MIF_UTIL;
static unsigned int exynos_qmax_r[2] = {DEFAULT_QMAX_R, NO_QMAX};
static unsigned int exynos_qmax_w[2] = {DEFAULT_QMAX_W, NO_QMAX};

enum bts_index {
	BTS_IDX_ABOX,
	BTS_IDX_COREX,
	BTS_IDX_CAM,
	BTS_IDX_DPU,
	BTS_IDX_DIT,
	BTS_IDX_FSYS,
	BTS_IDX_G2D,
	BTS_IDX_G3D,
	BTS_IDX_GNSS,
	BTS_IDX_ISP0,
	BTS_IDX_ISP1,
	BTS_IDX_MFC0,
	BTS_IDX_MFC1,
	BTS_IDX_MODEM0,
	BTS_IDX_MODEM1,
	BTS_IDX_WLBT,
	BTS_IDX_USB,
	BTS_IDX_VIPX1,
	BTS_IDX_VIPX2,
	BTS_IDX_SIREX,
	BTS_IDX_CPU_DMC0,
	BTS_IDX_CPU_DMC1,
};

enum exynos_bts_type {
	BT_TREX,
};

struct bts_table {
	struct bts_status stat;
	struct bts_info *next_bts;
	int prev_scen;
	int next_scen;
};

struct bts_info {
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	bool enable;
	enum exynos_bts_type type;
	struct bts_table table[BS_MAX];
	enum bts_scen_type top_scen;
};

enum drex_index {
	DREX_IDX_0,
	DREX_IDX_1,
};

struct drex_status {
	bool scen_en;
	unsigned int write_flush_config[2];
	unsigned int drex_timeout[BTS_PRIORITY_MAX + 1];
	unsigned int vc_timer_th[BTS_VC_TIMER_TH_NR];
	/* BRB [0]:CPU, [1]:RT, [2]:NRT, [3]:CP */
	/* RDBUF [4]:CPU, [5]:RT, [6]:NRT, [7]:CP */
	unsigned int cutoff_con;
	/* [7:0]:CPU, [15:8]:RT, [23:16]:NRT, [31:24]:CP */
	unsigned int brb_cutoff_config;
	/* [7:0]:CPU, [15:8]:RT, [23:16]:NRT, [31:24]:CP */
	unsigned int rdbuf_cutoff_config;
};

struct drex_table {
	struct drex_status stat;
	struct drex_info *next_drex;
	int prev_scen;
	int next_scen;
};

struct drex_info {
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	bool enable;
	struct drex_table table[BS_MAX];
	enum bts_scen_type top_scen;
};

enum drex_pf_index {
	DREX_PF_IDX_0,
	DREX_PF_IDX_1,
};

struct drex_pf_status {
	bool scen_en;
	unsigned int pf_rreq_thrt_con;
	unsigned int allow_mo_for_region;
	unsigned int pf_qos_timer[BTS_PF_TIMER_NR];
};

struct drex_pf_table {
	struct drex_pf_status stat;
	struct drex_pf_info *next_drex_pf;
	int prev_scen;
	int next_scen;
};

struct drex_pf_info {
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	bool enable;
	struct drex_pf_table table[BS_MAX];
	enum bts_scen_type top_scen;
};

struct bts_scenario {
	const char *name;
	struct bts_info *head;
	struct drex_info *drex_head;
	struct drex_pf_info *drex_pf_head;
};

struct trex_info {
	unsigned int pa_base;
	void __iomem *va_base;
	unsigned int value;
	unsigned int read;
	unsigned int write;
};

static struct pm_qos_request exynos_mif_bts_qos;
static DEFINE_SPINLOCK(bts_lock);
static DEFINE_MUTEX(media_mutex);

static struct trex_info trex_snode[] = {
	{ .pa_base = EXYNOS9610_PA_S_NRT0, },
	{ .pa_base = EXYNOS9610_PA_S_NRT1, },
	{ .pa_base = EXYNOS9610_PA_RT_MEM0, },
	{ .pa_base = EXYNOS9610_PA_RT_MEM1, },
	{ .pa_base = EXYNOS9610_PA_CP_MEM0, },
	{ .pa_base = EXYNOS9610_PA_CP_MEM1, },
};

static struct bts_info exynos_bts[] = {
	[BTS_IDX_ABOX] = {
		.name = "abox",
		.pa_base = EXYNOS9610_PA_ABOX,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0xC,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0xC,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0xC,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0xA,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0xC,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x4,
	},
	[BTS_IDX_COREX] = {
		.name = "corex",
		.pa_base = EXYNOS9610_PA_COREX,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x4,
		.table[BS_DEFAULT].stat.wmo = 0x4,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
	},
	[BTS_IDX_CAM] = {
		.name = "cam",
		.pa_base = EXYNOS9610_PA_CAM,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0xC,
		.table[BS_DEFAULT].stat.rmo = 0x18,
		.table[BS_DEFAULT].stat.wmo = 0x18,
		.table[BS_DEFAULT].stat.timeout_en = true,
		.table[BS_DEFAULT].stat.timeout_r = 0xc,
		.table[BS_DEFAULT].stat.timeout_w = 0xc,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0xC,
		.table[BS_MFC_UHD].stat.rmo = 0x18,
		.table[BS_MFC_UHD].stat.wmo = 0x18,
		.table[BS_MFC_UHD].stat.timeout_en = true,
		.table[BS_MFC_UHD].stat.timeout_r = 0xc,
		.table[BS_MFC_UHD].stat.timeout_w = 0xc,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0xC,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x18,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x18,
		.table[BS_G3D_PERFORMANCE].stat.timeout_en = true,
		.table[BS_G3D_PERFORMANCE].stat.timeout_r = 0xc,
		.table[BS_G3D_PERFORMANCE].stat.timeout_w = 0xc,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0xC,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x18,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x18,
		.table[BS_CAMERA_DEFAULT].stat.timeout_en = true,
		.table[BS_CAMERA_DEFAULT].stat.timeout_r = 0xc,
		.table[BS_CAMERA_DEFAULT].stat.timeout_w = 0xc,
		.table[BS_CAMERA_REMOSAIC].stat.scen_en = true,
		.table[BS_CAMERA_REMOSAIC].stat.priority = 0xC,
		.table[BS_CAMERA_REMOSAIC].stat.rmo = 0x10,
		.table[BS_CAMERA_REMOSAIC].stat.wmo = 0x10,
		.table[BS_CAMERA_REMOSAIC].stat.timeout_en = true,
		.table[BS_CAMERA_REMOSAIC].stat.timeout_r = 0xc,
		.table[BS_CAMERA_REMOSAIC].stat.timeout_w = 0xc,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0xC,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x16,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x16,
		.table[BS_FHD_480_ENCODING].stat.timeout_en = true,
		.table[BS_FHD_480_ENCODING].stat.timeout_r = 0xc,
		.table[BS_FHD_480_ENCODING].stat.timeout_w = 0xc,
	},
	[BTS_IDX_DPU] = {
		.name = "dpu",
		.pa_base = EXYNOS9610_PA_DPU,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0xA,
		.table[BS_DEFAULT].stat.rmo = 0x20,
		.table[BS_DEFAULT].stat.wmo = 0x20,
		.table[BS_DEFAULT].stat.timeout_en = true,
		.table[BS_DEFAULT].stat.timeout_r = 0x20,
		.table[BS_DEFAULT].stat.timeout_w = 0x20,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0xA,
		.table[BS_MFC_UHD].stat.rmo = 0x20,
		.table[BS_MFC_UHD].stat.wmo = 0x20,
		.table[BS_MFC_UHD].stat.timeout_en = true,
		.table[BS_MFC_UHD].stat.timeout_r = 0x20,
		.table[BS_MFC_UHD].stat.timeout_w = 0x20,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0xA,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x20,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x20,
		.table[BS_G3D_PERFORMANCE].stat.timeout_en = true,
		.table[BS_G3D_PERFORMANCE].stat.timeout_r = 0x20,
		.table[BS_G3D_PERFORMANCE].stat.timeout_w = 0x20,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0xA,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x20,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x20,
		.table[BS_CAMERA_DEFAULT].stat.timeout_en = true,
		.table[BS_CAMERA_DEFAULT].stat.timeout_r = 0x20,
		.table[BS_CAMERA_DEFAULT].stat.timeout_w = 0x20,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0xA,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x18,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x18,
		.table[BS_FHD_480_ENCODING].stat.timeout_en = true,
		.table[BS_FHD_480_ENCODING].stat.timeout_r = 0x20,
		.table[BS_FHD_480_ENCODING].stat.timeout_w = 0x20,
	},
	[BTS_IDX_DIT] = {
		.name = "dit",
		.pa_base = EXYNOS9610_PA_DIT,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x4,
		.table[BS_DEFAULT].stat.wmo = 0x4,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
	},
	[BTS_IDX_FSYS] = {
		.name = "fsys",
		.pa_base = EXYNOS9610_PA_FSYS,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x4,
		.table[BS_DEFAULT].stat.wmo = 0x4,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
	},
	[BTS_IDX_G2D] = {
		.name = "g2d",
		.pa_base = EXYNOS9610_PA_G2D,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x10,
		.table[BS_DEFAULT].stat.wmo = 0x10,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x10,
		.table[BS_MFC_UHD].stat.wmo = 0x10,
		.table[BS_MFC_UHD].stat.max_rmo = 0x1,
		.table[BS_MFC_UHD].stat.max_wmo = 0x1,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.max_rmo = 0x1,
		.table[BS_G3D_PERFORMANCE].stat.max_wmo = 0x1,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x8,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x8,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0x4,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x6,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x6,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x1,
	},
	[BTS_IDX_G3D] = {
		.name = "g3d",
		.pa_base = EXYNOS9610_PA_G3D,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x8,
		.table[BS_DEFAULT].stat.wmo = 0x8,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x8,
		.table[BS_MFC_UHD].stat.wmo = 0x8,
		.table[BS_MFC_UHD].stat.max_rmo = 0x1,
		.table[BS_MFC_UHD].stat.max_wmo = 0x1,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_CAMERA_REMOSAIC].stat.scen_en = true,
		.table[BS_CAMERA_REMOSAIC].stat.priority = 0x4,
		.table[BS_CAMERA_REMOSAIC].stat.rmo = 0x3,
		.table[BS_CAMERA_REMOSAIC].stat.wmo = 0x3,
		.table[BS_CAMERA_REMOSAIC].stat.max_rmo = 0x1,
		.table[BS_CAMERA_REMOSAIC].stat.max_wmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0x4,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x1,
	},
	[BTS_IDX_GNSS] = {
		.name = "gnss",
		.pa_base = EXYNOS9610_PA_GNSS,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x4,
		.table[BS_DEFAULT].stat.wmo = 0x4,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
	},
	[BTS_IDX_ISP0] = {
		.name = "isp0",
		.pa_base = EXYNOS9610_PA_ISP0,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x10,
		.table[BS_DEFAULT].stat.wmo = 0x10,
		.table[BS_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x10,
		.table[BS_MFC_UHD].stat.wmo = 0x10,
		.table[BS_MFC_UHD].stat.max_rmo = 0x4,
		.table[BS_MFC_UHD].stat.max_wmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.max_rmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.max_wmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0xC,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x1,
	},
	[BTS_IDX_ISP1] = {
		.name = "isp1",
		.pa_base = EXYNOS9610_PA_ISP1,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x10,
		.table[BS_DEFAULT].stat.wmo = 0x10,
		.table[BS_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x10,
		.table[BS_MFC_UHD].stat.wmo = 0x10,
		.table[BS_MFC_UHD].stat.max_rmo = 0x4,
		.table[BS_MFC_UHD].stat.max_wmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.max_rmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.max_wmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0xC,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x1,
	},
	[BTS_IDX_MFC0] = {
		.name = "mfc0",
		.pa_base = EXYNOS9610_PA_MFC0,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x8,
		.table[BS_DEFAULT].stat.wmo = 0x8,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x14,
		.table[BS_MFC_UHD].stat.wmo = 0x14,
		.table[BS_MFC_UHD].stat.max_rmo = 0x1,
		.table[BS_MFC_UHD].stat.max_wmo = 0x1,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x8,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x8,
		.table[BS_G3D_PERFORMANCE].stat.max_rmo = 0x1,
		.table[BS_G3D_PERFORMANCE].stat.max_wmo = 0x1,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x8,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x8,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0x4,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x4,
	},
	[BTS_IDX_MFC1] = {
		.name = "mfc1",
		.pa_base = EXYNOS9610_PA_MFC1,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x8,
		.table[BS_DEFAULT].stat.wmo = 0x8,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x14,
		.table[BS_MFC_UHD].stat.wmo = 0x14,
		.table[BS_MFC_UHD].stat.max_rmo = 0x1,
		.table[BS_MFC_UHD].stat.max_wmo = 0x1,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x8,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x8,
		.table[BS_G3D_PERFORMANCE].stat.max_rmo = 0x1,
		.table[BS_G3D_PERFORMANCE].stat.max_wmo = 0x1,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x8,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x8,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0x4,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x10,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x4,
	},
	[BTS_IDX_MODEM0] = {
		.name = "modem0",
		.pa_base = EXYNOS9610_PA_MODEM0,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0xD,
		.table[BS_DEFAULT].stat.timeout_en = true,
		.table[BS_DEFAULT].stat.timeout_r = 0xc,
		.table[BS_DEFAULT].stat.timeout_w = 0xa,
	},
	[BTS_IDX_MODEM1] = {
		.name = "modem1",
		.pa_base = EXYNOS9610_PA_MODEM1,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
	},
	[BTS_IDX_WLBT] = {
		.name = "wlbt",
		.pa_base = EXYNOS9610_PA_WLBT,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x4,
		.table[BS_DEFAULT].stat.wmo = 0x4,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
	},
	[BTS_IDX_USB] = {
		.name = "usb",
		.pa_base = EXYNOS9610_PA_USB,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x4,
		.table[BS_DEFAULT].stat.wmo = 0x4,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
	},
	[BTS_IDX_VIPX1] = {
		.name = "vipx1",
		.pa_base = EXYNOS9610_PA_VIPX1,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x10,
		.table[BS_DEFAULT].stat.wmo = 0x10,
		.table[BS_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x10,
		.table[BS_MFC_UHD].stat.wmo = 0x10,
		.table[BS_MFC_UHD].stat.max_rmo = 0x4,
		.table[BS_MFC_UHD].stat.max_wmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.max_rmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.max_wmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0x8,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x1,
	},
	[BTS_IDX_VIPX2] = {
		.name = "vipx2",
		.pa_base = EXYNOS9610_PA_VIPX2,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x10,
		.table[BS_DEFAULT].stat.wmo = 0x10,
		.table[BS_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_MFC_UHD].stat.scen_en = true,
		.table[BS_MFC_UHD].stat.priority = 0x4,
		.table[BS_MFC_UHD].stat.rmo = 0x10,
		.table[BS_MFC_UHD].stat.wmo = 0x10,
		.table[BS_MFC_UHD].stat.max_rmo = 0x4,
		.table[BS_MFC_UHD].stat.max_wmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.scen_en = true,
		.table[BS_G3D_PERFORMANCE].stat.priority = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.rmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.wmo = 0x10,
		.table[BS_G3D_PERFORMANCE].stat.max_rmo = 0x4,
		.table[BS_G3D_PERFORMANCE].stat.max_wmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.priority = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.rmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.wmo = 0x10,
		.table[BS_CAMERA_DEFAULT].stat.max_rmo = 0x4,
		.table[BS_CAMERA_DEFAULT].stat.max_wmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.priority = 0x8,
		.table[BS_FHD_480_ENCODING].stat.rmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.wmo = 0x4,
		.table[BS_FHD_480_ENCODING].stat.max_rmo = 0x1,
		.table[BS_FHD_480_ENCODING].stat.max_wmo = 0x1,
	},
	[BTS_IDX_SIREX] = {
		.name = "sirex",
		.pa_base = EXYNOS9610_PA_SIREX,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x4,
		.table[BS_DEFAULT].stat.rmo = 0x4,
		.table[BS_DEFAULT].stat.wmo = 0x4,
		.table[BS_DEFAULT].stat.max_rmo = 0x1,
		.table[BS_DEFAULT].stat.max_wmo = 0x1,
	},
	[BTS_IDX_CPU_DMC0] = {
		.name = "cpu_dmc0",
		.pa_base = EXYNOS9610_PA_CPU_DMC0,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x0,
	},
	[BTS_IDX_CPU_DMC1] = {
		.name = "cpu_dmc1",
		.pa_base = EXYNOS9610_PA_CPU_DMC1,
		.type = BT_TREX,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.priority = 0x0,
	},
};

static struct drex_info exynos_drex[] = {
	[DREX_IDX_0] = {
		.name = "drex0",
		.pa_base = EXYNOS9610_PA_DREX0,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.write_flush_config[0] = 0xB4301606,
		.table[BS_DEFAULT].stat.write_flush_config[1] = 0x1810100A,
		.table[BS_DEFAULT].stat.drex_timeout[0x0] = 0x000A0180,
		.table[BS_DEFAULT].stat.drex_timeout[0x1] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x2] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x3] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x4] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x5] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x6] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x7] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x8] = 0x000A0080,
		.table[BS_DEFAULT].stat.drex_timeout[0x9] = 0x000A0060,
		.table[BS_DEFAULT].stat.drex_timeout[0xA] = 0x000A0040,
		.table[BS_DEFAULT].stat.drex_timeout[0xB] = 0x000A0020,
		.table[BS_DEFAULT].stat.drex_timeout[0xC] = 0x000A0010,
		.table[BS_DEFAULT].stat.drex_timeout[0xD] = 0x000A0008,
		.table[BS_DEFAULT].stat.drex_timeout[0xE] = 0x000A0004,
		.table[BS_DEFAULT].stat.drex_timeout[0xF] = 0x000A0001,
		.table[BS_DEFAULT].stat.vc_timer_th[0] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[1] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[2] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[3] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[4] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[5] = 0x000D001A,
		.table[BS_DEFAULT].stat.vc_timer_th[6] = 0x00030007,
		.table[BS_DEFAULT].stat.vc_timer_th[7] = 0x00010001,
		.table[BS_DEFAULT].stat.cutoff_con = 0x00000005,
		.table[BS_DEFAULT].stat.brb_cutoff_config = 0x00020002,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.write_flush_config[0] = 0xB4301606,
		.table[BS_CAMERA_DEFAULT].stat.write_flush_config[1] = 0x1820200A,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x0] = 0x000A0180,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x1] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x2] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x3] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x4] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x5] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x6] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x7] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x8] = 0x000A0080,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x9] = 0x000A0060,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xA] = 0x000A0040,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xB] = 0x000A0020,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xC] = 0x000A0010,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xD] = 0x000A0008,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xE] = 0x000A0004,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xF] = 0x000A0001,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[0] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[1] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[2] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[3] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[4] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[5] = 0x000D001A,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[6] = 0x00030007,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[7] = 0x00010001,
		.table[BS_CAMERA_DEFAULT].stat.cutoff_con = 0x00000005,
		.table[BS_CAMERA_DEFAULT].stat.brb_cutoff_config = 0x00030003,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.write_flush_config[0] = 0xB4301606,
		.table[BS_FHD_480_ENCODING].stat.write_flush_config[1] = 0x1810100A,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x0] = 0x000A0180,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x1] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x2] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x3] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x4] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x5] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x6] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x7] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x8] = 0x000A0011,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x9] = 0x000A0060,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xA] = 0x000A0040,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xB] = 0x000A0020,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xC] = 0x000A0010,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xD] = 0x000A0008,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xE] = 0x000A0004,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xF] = 0x000A0001,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[0] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[1] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[2] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[3] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[4] = 0x00C10009,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[5] = 0x000D0022,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[6] = 0x00030007,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[7] = 0x00010001,
		.table[BS_FHD_480_ENCODING].stat.cutoff_con = 0x00000005,
		.table[BS_FHD_480_ENCODING].stat.brb_cutoff_config = 0x00020002,
	},
	[DREX_IDX_1] = {
		.name = "drex1",
		.pa_base = EXYNOS9610_PA_DREX1,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.write_flush_config[0] = 0xB4301606,
		.table[BS_DEFAULT].stat.write_flush_config[1] = 0x1810100A,
		.table[BS_DEFAULT].stat.drex_timeout[0x0] = 0x000A0180,
		.table[BS_DEFAULT].stat.drex_timeout[0x1] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x2] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x3] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x4] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x5] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x6] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x7] = 0x000A0100,
		.table[BS_DEFAULT].stat.drex_timeout[0x8] = 0x000A0080,
		.table[BS_DEFAULT].stat.drex_timeout[0x9] = 0x000A0060,
		.table[BS_DEFAULT].stat.drex_timeout[0xA] = 0x000A0040,
		.table[BS_DEFAULT].stat.drex_timeout[0xB] = 0x000A0020,
		.table[BS_DEFAULT].stat.drex_timeout[0xC] = 0x000A0010,
		.table[BS_DEFAULT].stat.drex_timeout[0xD] = 0x000A0008,
		.table[BS_DEFAULT].stat.drex_timeout[0xE] = 0x000A0004,
		.table[BS_DEFAULT].stat.drex_timeout[0xF] = 0x000A0001,
		.table[BS_DEFAULT].stat.vc_timer_th[0] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[1] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[2] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[3] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[4] = 0x00C100C1,
		.table[BS_DEFAULT].stat.vc_timer_th[5] = 0x000D001A,
		.table[BS_DEFAULT].stat.vc_timer_th[6] = 0x00030007,
		.table[BS_DEFAULT].stat.vc_timer_th[7] = 0x00010001,
		.table[BS_DEFAULT].stat.cutoff_con = 0x00000005,
		.table[BS_DEFAULT].stat.brb_cutoff_config = 0x00020002,
		.table[BS_CAMERA_DEFAULT].stat.scen_en = true,
		.table[BS_CAMERA_DEFAULT].stat.write_flush_config[0] = 0xB4301606,
		.table[BS_CAMERA_DEFAULT].stat.write_flush_config[1] = 0x1820200A,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x0] = 0x000A0180,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x1] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x2] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x3] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x4] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x5] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x6] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x7] = 0x000A0100,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x8] = 0x000A0080,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0x9] = 0x000A0060,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xA] = 0x000A0040,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xB] = 0x000A0020,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xC] = 0x000A0010,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xD] = 0x000A0008,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xE] = 0x000A0004,
		.table[BS_CAMERA_DEFAULT].stat.drex_timeout[0xF] = 0x000A0001,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[0] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[1] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[2] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[3] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[4] = 0x00C100C1,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[5] = 0x000D001A,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[6] = 0x00030007,
		.table[BS_CAMERA_DEFAULT].stat.vc_timer_th[7] = 0x00010001,
		.table[BS_CAMERA_DEFAULT].stat.cutoff_con = 0x00000005,
		.table[BS_CAMERA_DEFAULT].stat.brb_cutoff_config = 0x00030003,
		.table[BS_FHD_480_ENCODING].stat.scen_en = true,
		.table[BS_FHD_480_ENCODING].stat.write_flush_config[0] = 0xB4301606,
		.table[BS_FHD_480_ENCODING].stat.write_flush_config[1] = 0x1810100A,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x0] = 0x000A0180,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x1] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x2] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x3] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x4] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x5] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x6] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x7] = 0x000A0100,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x8] = 0x000A0011,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0x9] = 0x000A0060,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xA] = 0x000A0040,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xB] = 0x000A0020,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xC] = 0x000A0010,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xD] = 0x000A0008,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xE] = 0x000A0004,
		.table[BS_FHD_480_ENCODING].stat.drex_timeout[0xF] = 0x000A0001,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[0] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[1] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[2] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[3] = 0x00C100C1,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[4] = 0x00C10009,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[5] = 0x000D0022,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[6] = 0x00030007,
		.table[BS_FHD_480_ENCODING].stat.vc_timer_th[7] = 0x00010001,
		.table[BS_FHD_480_ENCODING].stat.cutoff_con = 0x00000005,
		.table[BS_FHD_480_ENCODING].stat.brb_cutoff_config = 0x00020002,
	},
};

static struct drex_pf_info exynos_drex_pf[] = {
	[DREX_PF_IDX_0] = {
		.name = "drex0_pf",
		.pa_base = EXYNOS9610_PA_DREX0_PF,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.pf_rreq_thrt_con = 0x00008000,
		.table[BS_DEFAULT].stat.allow_mo_for_region = 0x00000000,
		.table[BS_DEFAULT].stat.pf_qos_timer[0] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[1] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[2] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[3] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[4] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[5] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[6] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[7] = 0x00050005,
	},
	[DREX_PF_IDX_1] = {
		.name = "drex1_pf",
		.pa_base = EXYNOS9610_PA_DREX1_PF,
		.enable = true,
		.table[BS_DEFAULT].stat.scen_en = true,
		.table[BS_DEFAULT].stat.pf_rreq_thrt_con = 0x00008000,
		.table[BS_DEFAULT].stat.allow_mo_for_region = 0x00000000,
		.table[BS_DEFAULT].stat.pf_qos_timer[0] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[1] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[2] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[3] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[4] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[5] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[6] = 0x00050005,
		.table[BS_DEFAULT].stat.pf_qos_timer[7] = 0x00050005,
	},
};

static struct bts_scenario bts_scen[BS_MAX] = {
	[BS_DEFAULT] = {
		.name = "default",
	},
	[BS_MFC_UHD] = {
		.name = "mfc uhd",
	},
	[BS_G3D_PERFORMANCE] = {
		.name = "g3d per",
	},
	[BS_CAMERA_DEFAULT] = {
		.name = "camscen",
	},
	[BS_CAMERA_REMOSAIC] = {
		.name = "camremo",
	},
	[BS_FHD_480_ENCODING] = {
		.name = "fhd-480",
	},
};

static void bts_set_ip_table(struct bts_info *bts)
{
	enum bts_scen_type scen = bts->top_scen;

	BTS_DBG("[BTS] %s bts scen: [%s]->[%s]\n", bts->name,
			bts_scen[scen].name, bts_scen[scen].name);

	switch (bts->type) {
	case BT_TREX:
		bts_setqos(bts->va_base, &bts->table[scen].stat);
		break;
	default:
		break;
	}
}

static void bts_set_drex_table(struct drex_info *drex)
{
	enum bts_scen_type scen = drex->top_scen;
	int i;

	BTS_DBG("[BTS] %s bts scen: [%s]->[%s]\n", drex->name,
			bts_scen[scen].name, bts_scen[scen].name);

	__raw_writel(drex->table[scen].stat.write_flush_config[0],
			drex->va_base + WRITE_FLUSH_CONFIG0);
	__raw_writel(drex->table[scen].stat.write_flush_config[1],
			drex->va_base + WRITE_FLUSH_CONFIG1);

	for (i = 0; i <= BTS_PRIORITY_MAX; i++)
		__raw_writel(drex->table[scen].stat.drex_timeout[i],
				drex->va_base + QOS_TIMEOUT_0 + (4 * i));

	for (i = 0; i < BTS_VC_TIMER_TH_NR; i++)
		__raw_writel(drex->table[scen].stat.vc_timer_th[i],
				drex->va_base + VC_TIMER_TH_0 + (4 * i));

	__raw_writel(drex->table[scen].stat.cutoff_con,
			drex->va_base + CUTOFF_CONTROL);
	__raw_writel(drex->table[scen].stat.brb_cutoff_config,
			drex->va_base + BRB_CUTOFF_CONFIG0);
	__raw_writel(drex->table[scen].stat.rdbuf_cutoff_config,
			drex->va_base + RDBUF_CUTOFF_CONFIG0);
}

static void bts_set_drex_pf_table(struct drex_pf_info *drex_pf)
{
	enum bts_scen_type scen = drex_pf->top_scen;
	int i;

	BTS_DBG("[BTS] %s bts scen: [%s]->[%s]\n", drex_pf->name,
			bts_scen[scen].name, bts_scen[scen].name);

	__raw_writel(drex_pf->table[scen].stat.pf_rreq_thrt_con,
			drex_pf->va_base + PF_RREQ_THROTTLE_CONTROL);

	__raw_writel(drex_pf->table[scen].stat.allow_mo_for_region,
			drex_pf->va_base + PF_RREQ_THROTTLE_MO_P2);

	for (i = 0; i < BTS_PF_TIMER_NR; i++)
		__raw_writel(drex_pf->table[scen].stat.pf_qos_timer[i],
				drex_pf->va_base + PF_QOS_TIMER_0 + (4 * i));
}

static void bts_drex_add_scen(enum bts_scen_type scen)
{
	struct drex_info *first = bts_scen[scen].drex_head;
	struct drex_info *drex = bts_scen[scen].drex_head;
	int next = 0;
	int prev = 0;

	if (!drex)
		return;

	do {
		if (drex->enable && !drex->table[scen].next_scen) {
			if (scen >= drex->top_scen) {
				/* insert at top priority */
				drex->table[scen].prev_scen = drex->top_scen;
				drex->table[drex->top_scen].next_scen = scen;
				drex->top_scen = scen;
				drex->table[scen].next_scen = -1;

				bts_set_drex_table(drex);

			} else {
				/* insert at middle */
				for (prev = drex->top_scen; prev > scen;
				     prev = drex->table[prev].prev_scen)
					next = prev;

				drex->table[scen].prev_scen =
					drex->table[next].prev_scen;
				drex->table[scen].next_scen =
					drex->table[prev].next_scen;
				drex->table[next].prev_scen = scen;
				drex->table[prev].next_scen = scen;
			}
		}

		drex = drex->table[scen].next_drex;
	/* set all DREX in the current scenario */
	} while (drex && drex != first);
}

static void bts_drex_pf_add_scen(enum bts_scen_type scen)
{
	struct drex_pf_info *first = bts_scen[scen].drex_pf_head;
	struct drex_pf_info *drex_pf = bts_scen[scen].drex_pf_head;
	int next = 0;
	int prev = 0;

	if (!drex_pf)
		return;

	do {
		if (drex_pf->enable && !drex_pf->table[scen].next_scen) {
			if (scen >= drex_pf->top_scen) {
				/* insert at top priority */
				drex_pf->table[scen].prev_scen = drex_pf->top_scen;
				drex_pf->table[drex_pf->top_scen].next_scen = scen;
				drex_pf->top_scen = scen;
				drex_pf->table[scen].next_scen = -1;

				bts_set_drex_pf_table(drex_pf);

			} else {
				/* insert at middle */
				for (prev = drex_pf->top_scen; prev > scen;
				     prev = drex_pf->table[prev].prev_scen)
					next = prev;

				drex_pf->table[scen].prev_scen =
					drex_pf->table[next].prev_scen;
				drex_pf->table[scen].next_scen =
					drex_pf->table[prev].next_scen;
				drex_pf->table[next].prev_scen = scen;
				drex_pf->table[prev].next_scen = scen;
			}
		}

		drex_pf = drex_pf->table[scen].next_drex_pf;
	/* set all DREX_PF in the current scenario */
	} while (drex_pf && drex_pf != first);
}

static void bts_add_scen(enum bts_scen_type scen)
{
	struct bts_info *first = bts_scen[scen].head;
	struct bts_info *bts = bts_scen[scen].head;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[BTS] scen %s on\n", bts_scen[scen].name);

	do {
		if (bts->enable && !bts->table[scen].next_scen) {
			if (scen >= bts->top_scen) {
				/* insert at top priority */
				bts->table[scen].prev_scen = bts->top_scen;
				bts->table[bts->top_scen].next_scen = scen;
				bts->top_scen = scen;
				bts->table[scen].next_scen = -1;

				bts_set_ip_table(bts);

			} else {
				/* insert at middle */
				for (prev = bts->top_scen; prev > scen;
				     prev = bts->table[prev].prev_scen)
					next = prev;

				bts->table[scen].prev_scen =
					bts->table[next].prev_scen;
				bts->table[scen].next_scen =
					bts->table[prev].next_scen;
				bts->table[next].prev_scen = scen;
				bts->table[prev].next_scen = scen;
			}
		}

		bts = bts->table[scen].next_bts;
	/* set all bts ip in the current scenario */
	} while (bts && bts != first);

	bts_drex_add_scen(scen);
	bts_drex_pf_add_scen(scen);
}

static void bts_drex_del_scen(enum bts_scen_type scen)
{
	struct drex_info *first = bts_scen[scen].drex_head;
	struct drex_info *drex = bts_scen[scen].drex_head;
	int next = 0;
	int prev = 0;

	if (!drex)
		return;

	do {
		if (drex->enable && drex->table[scen].next_scen) {
			if (scen == drex->top_scen) {
				/* revert to prev scenario */
				prev = drex->table[scen].prev_scen;
				drex->top_scen = prev;
				drex->table[prev].next_scen = -1;
				drex->table[scen].next_scen = 0;
				drex->table[scen].prev_scen = 0;

				bts_set_drex_table(drex);
			} else if (scen < drex->top_scen) {
				/* delete mid scenario */
				prev = drex->table[scen].prev_scen;
				next = drex->table[scen].next_scen;

				drex->table[next].prev_scen =
					drex->table[scen].prev_scen;
				drex->table[prev].next_scen =
					drex->table[scen].next_scen;

				drex->table[scen].prev_scen = 0;
				drex->table[scen].next_scen = 0;

			} else {
				BTS_DBG("[BTS]%s scenario couldn't exist above top_scen\n",
						bts_scen[scen].name);
			}
		}

		drex = drex->table[scen].next_drex;
	/* revert all DREX to prev in the current scenario */
	} while (drex && drex != first);
}

static void bts_drex_pf_del_scen(enum bts_scen_type scen)
{
	struct drex_pf_info *first = bts_scen[scen].drex_pf_head;
	struct drex_pf_info *drex_pf = bts_scen[scen].drex_pf_head;
	int next = 0;
	int prev = 0;

	if (!drex_pf)
		return;

	do {
		if (drex_pf->enable && drex_pf->table[scen].next_scen) {
			if (scen == drex_pf->top_scen) {
				/* revert to prev scenario */
				prev = drex_pf->table[scen].prev_scen;
				drex_pf->top_scen = prev;
				drex_pf->table[prev].next_scen = -1;
				drex_pf->table[scen].next_scen = 0;
				drex_pf->table[scen].prev_scen = 0;

				bts_set_drex_pf_table(drex_pf);
			} else if (scen < drex_pf->top_scen) {
				/* delete mid scenario */
				prev = drex_pf->table[scen].prev_scen;
				next = drex_pf->table[scen].next_scen;

				drex_pf->table[next].prev_scen =
					drex_pf->table[scen].prev_scen;
				drex_pf->table[prev].next_scen =
					drex_pf->table[scen].next_scen;

				drex_pf->table[scen].prev_scen = 0;
				drex_pf->table[scen].next_scen = 0;

			} else {
				BTS_DBG("[BTS]%s scenario couldn't exist above top_scen\n",
						bts_scen[scen].name);
			}
		}

		drex_pf = drex_pf->table[scen].next_drex_pf;
	/* revert all DREX_PF to prev in the current scenario */
	} while (drex_pf && drex_pf != first);
}

static void bts_del_scen(enum bts_scen_type scen)
{
	struct bts_info *first = bts_scen[scen].head;
	struct bts_info *bts = bts_scen[scen].head;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[BTS] scen %s off\n", bts_scen[scen].name);

	do {
		if (bts->enable && bts->table[scen].next_scen) {
			if (scen == bts->top_scen) {
				/* revert to prev scenario */
				prev = bts->table[scen].prev_scen;
				bts->top_scen = prev;
				bts->table[prev].next_scen = -1;
				bts->table[scen].next_scen = 0;
				bts->table[scen].prev_scen = 0;

				bts_set_ip_table(bts);
			} else if (scen < bts->top_scen) {
				/* delete mid scenario */
				prev = bts->table[scen].prev_scen;
				next = bts->table[scen].next_scen;

				bts->table[next].prev_scen =
					bts->table[scen].prev_scen;
				bts->table[prev].next_scen =
					bts->table[scen].next_scen;

				bts->table[scen].prev_scen = 0;
				bts->table[scen].next_scen = 0;

			} else {
				BTS_DBG("[BTS]%s scenario couldn't exist above top_scen\n",
						bts_scen[scen].name);
			}
		}

		bts = bts->table[scen].next_bts;
	/* revert all bts ip to prev in the current scenario */
	} while (bts && bts != first);

	bts_drex_del_scen(scen);
	bts_drex_pf_del_scen(scen);
}

void bts_update_scen(enum bts_scen_type scen, unsigned int val)
{
	bool on = val ? 1 : 0;

	if (scen <= BS_DEFAULT || scen >= BS_MAX)
		return;

	switch (scen) {
	default:
		spin_lock(&bts_lock);
		if (on)
			bts_add_scen(scen);
		else
			bts_del_scen(scen);
		spin_unlock(&bts_lock);
		break;
	}
}

static void scen_chaining(enum bts_scen_type scen)
{
	struct bts_info *prev = NULL;
	struct bts_info *first = NULL;
	struct bts_info *bts;
	struct drex_info *drex_prev = NULL;
	struct drex_info *drex_first = NULL;
	struct drex_info *drex;
	struct drex_pf_info *drex_pf_prev = NULL;
	struct drex_pf_info *drex_pf_first = NULL;
	struct drex_pf_info *drex_pf;

	for (bts = exynos_bts;
	     bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (bts->table[scen].stat.scen_en) {
			if (!first)
				first = bts;
			if (prev)
				prev->table[scen].next_bts = bts;

			prev = bts;
		}
	}

	if (prev)
		prev->table[scen].next_bts = first;

	bts_scen[scen].head = first;

	for (drex = exynos_drex;
	     drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (drex->table[scen].stat.scen_en) {
			if (!drex_first)
				drex_first = drex;
			if (drex_prev)
				drex_prev->table[scen].next_drex = drex;

			drex_prev = drex;
		}
	}

	if (drex_prev)
		drex_prev->table[scen].next_drex = drex_first;

	bts_scen[scen].drex_head = drex_first;

	for (drex_pf = exynos_drex_pf;
	     drex_pf <= &exynos_drex_pf[ARRAY_SIZE(exynos_drex_pf) - 1]; drex_pf++) {
		if (drex_pf->table[scen].stat.scen_en) {
			if (!drex_pf_first)
				drex_pf_first = drex_pf;
			if (drex_pf_prev)
				drex_pf_prev->table[scen].next_drex_pf = drex_pf;

			drex_pf_prev = drex_pf;
		}
	}

	if (drex_pf_prev)
		drex_pf_prev->table[scen].next_drex_pf = drex_pf_first;

	bts_scen[scen].drex_pf_head = drex_pf_first;
}

#define BIT_PER_BYTE		8

static unsigned int bts_bw_calc(struct bts_decon_info *decon, int idx)
{
	struct bts_dpp_info *dpp = &decon->dpp[idx];
	unsigned int bw;
	unsigned int dst_w, dst_h;

	dst_w = dpp->dst.x2 - dpp->dst.x1;
	dst_h = dpp->dst.y2 - dpp->dst.y1;
	if (!(dst_w && dst_h))
		return 0;
	/* use multifactor for KB/s */
	bw = ((u64)dpp->src_h * dpp->src_w * dpp->bpp * decon->vclk) *
	       (decon->lcd_w*11 + 480) / decon->lcd_w / 10 /
		(BIT_PER_BYTE * dst_h * decon->lcd_w);

	return bw;
}

static unsigned int bts_find_max_bw(struct bts_decon_info *decon,
			 const struct bts_layer_position *input, int idx)
{
	struct bts_layer_position output;
	struct bts_dpp_info *dpp;
	unsigned int max = 0;
	int i;

	for (i = idx; i < BTS_DPP_MAX; i++) {
		dpp = &decon->dpp[i];
		if (!dpp->used)
			continue;
		output.y1 = input->y1 < dpp->dst.y1 ? dpp->dst.y1 : input->y1;
		output.y2 = input->y2 > dpp->dst.y2 ? dpp->dst.y2 : input->y2;
		output.x1 = input->x1 < dpp->dst.x1 ? dpp->dst.x1 : input->x1;
		output.x2 = input->x2 > dpp->dst.x2 ? dpp->dst.x2 : input->x2;
		if (output.y1 < output.y2) {
			unsigned int bw;

			bw = dpp->bw + bts_find_max_bw(decon, &output, i + 1);
			if (bw > max)
				max = bw;
		}
	}
	return max;

}

static unsigned int bts_update_decon_bw(struct bts_decon_info *decon)
{
	unsigned int max = 0;
	struct bts_dpp_info *dpp;
	int i;

	for (i = 0; i < BTS_DPP_MAX; i++) {
		dpp = &decon->dpp[i];
		if (!dpp->used)
			continue;
		dpp->bw = bts_bw_calc(decon, i);
	}
	for (i = 0; i < BTS_DPP_MAX; i++) {
		unsigned int bw;

		dpp = &decon->dpp[i];
		if (!dpp->used)
			continue;
		bw = dpp->bw + bts_find_max_bw(decon, &dpp->dst, i + 1);
		if (bw > max)
			max = bw;
	}

	return max;
}

unsigned int bts_calc_bw(enum bts_bw_type type, void *data)
{
	unsigned int bw;

	switch (type) {
	case BTS_BW_DECON0:
	case BTS_BW_DECON1:
	case BTS_BW_DECON2:
		bw = bts_update_decon_bw(data);
		break;
	default:
		bw = 0;
		break;
	}

	return bw;
}

void bts_update_bw(enum bts_bw_type type, struct bts_bw bw)
{
	static struct bts_bw ip_bw[BTS_BW_MAX];
	unsigned int mif_freq;
	unsigned int total_bw = 0;
	unsigned int bw_r = 0;
	unsigned int bw_w = 0;
	int i;

	if (type >= BTS_BW_MAX)
		return;
	if (ip_bw[type].peak == bw.peak
	    && ip_bw[type].read == bw.read
	    && ip_bw[type].write == bw.write)
		return;
	mutex_lock(&media_mutex);

	ip_bw[type] = bw;
	for (i = 0; i < BTS_BW_MAX; i++) {
		bw_r += ip_bw[i].read;
		bw_w += ip_bw[i].write;
	}
	total_bw = bw_r + bw_w;

	/* MIF minimum frequency calculation as per BTS guide */
	mif_freq = total_bw * 100 / BUS_WIDTH / exynos_mif_util;

	pm_qos_update_request(&exynos_mif_bts_qos, mif_freq);

	BTS_DBG("[BTS] BW(KB/s): type%i bw %up %ur %uw,\n",
				type, bw.peak, bw.read, bw.write);
	BTS_DBG("[BTS] BW(KB/s, calc): total %u, read %u, write %u,\n",
				total_bw, bw_r, bw_w);
	BTS_DBG("[BTS] freq(Khz): mif %u\n", mif_freq);

	mutex_unlock(&media_mutex);
}

static void bts_initialize_domains(void)
{
	struct bts_info *bts;
	struct drex_info *drex;
	struct drex_pf_info *drex_pf;
	int i;

	spin_lock(&bts_lock);

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (!drex->enable)
			continue;
		bts_set_drex_table(drex);
	}

	for (drex_pf = exynos_drex_pf;
		drex_pf <= &exynos_drex_pf[ARRAY_SIZE(exynos_drex_pf) - 1]; drex_pf++) {
		if (!drex_pf->enable)
			continue;
		bts_set_drex_pf_table(drex_pf);
	}

	for (bts = exynos_bts;
		bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable)
			continue;
		bts_set_ip_table(bts);
	}

	for (i = 0; i < ARRAY_SIZE(trex_snode); i++)
		bts_set_qmax(trex_snode[i].va_base, exynos_qmax_r[0],
			exynos_qmax_r[1], exynos_qmax_w[0], exynos_qmax_w[1]);

	spin_unlock(&bts_lock);
}

static int exynos_bts_syscore_suspend(void)
{
	return 0;
}

static void exynos_bts_syscore_resume(void)
{
	bts_initialize_domains();
}

static struct syscore_ops exynos_bts_syscore_ops = {
	.suspend	= exynos_bts_syscore_suspend,
	.resume		= exynos_bts_syscore_resume,
};

#if defined(CONFIG_DEBUG_FS)
static int exynos_qos_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;

	spin_lock(&bts_lock);

	for (bts = exynos_bts;
	     bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable) {
			seq_printf(buf, "%5s(disabled):\n", bts->name);
			continue;
		} else {
			seq_printf(buf, "%5s(%s): ",
					bts->name, bts_scen[bts->top_scen].name);
		}
		switch (bts->type) {
		case BT_TREX:
			bts_showqos(bts->va_base, buf);
			break;
		default:
			seq_puts(buf, "none\n");
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static int exynos_dmc_timeout_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_info *drex;
	int i, j, nr_drex = 0;

	seq_puts(buf, "\tDREX/Scen/qos/timeout\nex)echo 0 0 0 0x100 > dmc_timeout\n");

	spin_lock(&bts_lock);

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (!drex->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex++, drex->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex++, drex->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			for (j = 0; j <= BTS_PRIORITY_MAX; j++)
				seq_printf(buf, "[0x%x]: 0x%08x\n", j,
						drex->table[i].stat.drex_timeout[j]);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_dmc_timeout_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_info *drex;
	char *buf;
	int drex_ip, scen, qos, ret;
	unsigned int timeout;
	int nr_drex = ARRAY_SIZE(exynos_drex) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %x\n", &drex_ip, &scen, &qos, &timeout);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_ip < 0 || drex_ip > nr_drex || scen < 0 ||
			scen > nr_scen || qos < 0 || qos > BTS_PRIORITY_MAX) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex = &exynos_drex[drex_ip];

	spin_lock(&bts_lock);
	drex->table[scen].stat.drex_timeout[qos] = timeout;

	if (!drex->table[scen].stat.scen_en) {
		drex->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex->top_scen)
		bts_set_drex_table(drex);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_mo_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;
	int i, nr_ip = 0;

	seq_puts(buf, "\tIP/Scen/RW/MO\nex)echo 0 0 0 16 > mo\n");
	spin_lock(&bts_lock);

	for (bts = exynos_bts;
			bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable) {
			seq_printf(buf, "[%2d]IP: %s is disabled\n", nr_ip++, bts->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]IP: %s\n", nr_ip++, bts->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s: rmo:0x%x wmo:0x%x\n",
					bts_scen[i].name,
					bts->table[i].stat.rmo,
					bts->table[i].stat.wmo);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_mo_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bts_info *bts = NULL;
	char *buf;
	int ip, scen, rw, mo, ret;
	int nr_ip = ARRAY_SIZE(exynos_bts) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %d\n", &ip, &scen, &rw, &mo);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (ip < 0 || ip > nr_ip || scen < 0 ||
			scen > nr_scen || rw < 0 || mo < 0) {
		pr_info("Invalid variable\n");
		goto out;
	}

	bts = &exynos_bts[ip];

	spin_lock(&bts_lock);
	if (!rw)
		bts->table[scen].stat.rmo = mo;
	else
		bts->table[scen].stat.wmo = mo;

	if (!bts->table[scen].stat.scen_en) {
		bts->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == bts->top_scen)
		bts_setqos(bts->va_base, &bts->table[scen].stat);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_max_mo_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;
	int i, nr_ip = 0;

	seq_puts(buf, "\tIP/Scen/RW/MO\nex)echo 0 0 0 16 > max_mo\n");
	spin_lock(&bts_lock);

	for (bts = exynos_bts;
			bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable) {
			seq_printf(buf, "[%2d]IP: %s is disabled\n", nr_ip++, bts->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]IP: %s\n", nr_ip++, bts->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s: max_rmo:0x%x max_wmo:0x%x\n",
					bts_scen[i].name,
					bts->table[i].stat.max_rmo,
					bts->table[i].stat.max_wmo);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_max_mo_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bts_info *bts = NULL;
	char *buf;
	int ip, scen, rw, mo, ret;
	int nr_ip = ARRAY_SIZE(exynos_bts) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %d\n", &ip, &scen, &rw, &mo);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (ip < 0 || ip > nr_ip || scen < 0 ||
			scen > nr_scen || rw < 0 || mo < 0) {
		pr_info("Invalid variable\n");
		goto out;
	}

	bts = &exynos_bts[ip];

	spin_lock(&bts_lock);
	if (!rw)
		bts->table[scen].stat.max_rmo = mo;
	else
		bts->table[scen].stat.max_wmo = mo;

	if (!bts->table[scen].stat.scen_en) {
		bts->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == bts->top_scen)
		bts_setqos(bts->va_base, &bts->table[scen].stat);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_full_mo_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;
	int i, nr_ip = 0;

	seq_puts(buf, "\tIP/Scen/RW/MO\nex)echo 0 0 0 16 > full_mo\n");
	spin_lock(&bts_lock);

	for (bts = exynos_bts;
			bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable) {
			seq_printf(buf, "[%2d]IP: %s is disabled\n", nr_ip++, bts->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]IP: %s\n", nr_ip++, bts->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s: full_rmo:0x%x full_wmo:0x%x\n",
					bts_scen[i].name,
					bts->table[i].stat.full_rmo,
					bts->table[i].stat.full_wmo);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_full_mo_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bts_info *bts = NULL;
	char *buf;
	int ip, scen, rw, mo, ret;
	int nr_ip = ARRAY_SIZE(exynos_bts) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %d\n", &ip, &scen, &rw, &mo);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (ip < 0 || ip > nr_ip || scen < 0 ||
			scen > nr_scen || rw < 0 || mo < 0) {
		pr_info("Invalid variable\n");
		goto out;
	}

	bts = &exynos_bts[ip];

	spin_lock(&bts_lock);
	if (!rw)
		bts->table[scen].stat.full_rmo = mo;
	else
		bts->table[scen].stat.full_wmo = mo;

	if (!bts->table[scen].stat.scen_en) {
		bts->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == bts->top_scen)
		bts_setqos(bts->va_base, &bts->table[scen].stat);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_prio_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;
	int i, nr_ip = 0;

	seq_puts(buf, "\tqos IP/Scen/Prio\nex)echo 0 0 8 > priority\n");
	spin_lock(&bts_lock);

	for (bts = exynos_bts;
			bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable) {
			seq_printf(buf, "[%2d]IP: %s is disabled\n", nr_ip++, bts->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]IP: %s\n", nr_ip++, bts->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s: %d\n",
					bts_scen[i].name, bts->table[i].stat.priority);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_prio_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bts_info *bts = NULL;
	char *buf;
	int ip, scen, prio, ret;
	int nr_ip = ARRAY_SIZE(exynos_bts) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d\n", &ip, &scen, &prio);
	if (ret != 3) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (ip < 0 || ip > nr_ip || scen < 0 ||
			scen > nr_scen || prio < 0 || prio > 0xf) {
		pr_info("Invalid variable\n");
		goto out;
	}

	bts = &exynos_bts[ip];

	spin_lock(&bts_lock);
	bts->table[scen].stat.priority = prio;

	if (!bts->table[scen].stat.scen_en) {
		bts->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == bts->top_scen)
		bts_setqos(bts->va_base, &bts->table[scen].stat);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_scen_status_open_show(struct seq_file *buf, void *d)
{
	int i;

	seq_puts(buf, "\tqos Scen/On\nex)echo 1 1 > scenario\n");

	for (i = 0; i < BS_MAX; i++) {
		if (!bts_scen[i].name)
			continue;
		seq_printf(buf, "[%2d]%9s\n", i, bts_scen[i].name);
	}
	return 0;
}

static ssize_t exynos_scen_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char *buf;
	int ret;
	u32 scen, on;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%u %u", &scen, &on);
	if (ret != 2) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (scen >= BS_MAX) {
		pr_err("Invalid variable\n");
		goto out;
	}

	bts_update_scen((enum bts_scen_type)scen, on);

out:
	kfree(buf);

	return count;
}

static int exynos_addr_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;

	spin_lock(&bts_lock);

	for (bts = exynos_bts;
			bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable)
			continue;
		seq_printf(buf, "[IP: %9s]:0x%x\n", bts->name, bts->pa_base);
	}

	spin_unlock(&bts_lock);

	return 0;
}

static int exynos_qmax_status_open_show(struct seq_file *buf, void *d)
{
	int i;

	seq_puts(buf, "\tr0_thrd\\r1_thrd\\w0_thrd\\w1_thrd\nex)echo 20 20 12 12 > qmax\n");

	spin_lock(&bts_lock);

	for (i = 0; i < ARRAY_SIZE(trex_snode); i++) {
		seq_printf(buf, "[0x%08x]: ", trex_snode[i].pa_base);
		bts_show_qmax(trex_snode[i].va_base, buf);
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_qmax_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int r0, r1, w0, w1;
	int i, ret;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%u %u %u %u\n", &r0, &r1, &w0, &w1);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (r0 > BTS_QMAX_MAX_THRESHOLD || r1 > BTS_QMAX_MAX_THRESHOLD ||
		w0 > BTS_QMAX_MAX_THRESHOLD || w1 > BTS_QMAX_MAX_THRESHOLD) {
		pr_err("Invalid variable\n");
		goto out;
	}

	spin_lock(&bts_lock);
	exynos_qmax_r[0] = r0;
	exynos_qmax_r[1] = r1;
	exynos_qmax_w[0] = w0;
	exynos_qmax_w[1] = w1;
	for (i = 0; i < ARRAY_SIZE(trex_snode); i++)
		bts_set_qmax(trex_snode[i].va_base, r0, r1, w0, w1);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_timeout_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;
	int i, nr_ip = 0;

	seq_puts(buf, "\tIP/Scen/RW/timeout\nex)echo 0 0 0 16 > timeout\n");
	spin_lock(&bts_lock);

	for (bts = exynos_bts;
			bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable) {
			seq_printf(buf, "[%2d]IP: %s is disabled\n", nr_ip++, bts->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]IP: %s\n", nr_ip++, bts->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s: timeout_r:0x%x timeout_w:0x%x\n",
					bts_scen[i].name,
					bts->table[i].stat.timeout_r,
					bts->table[i].stat.timeout_w);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_timeout_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bts_info *bts = NULL;
	char *buf;
	int ip, scen, rw, timeout, ret;
	int nr_ip = ARRAY_SIZE(exynos_bts) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %d\n", &ip, &scen, &rw, &timeout);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (ip < 0 || ip > nr_ip || scen < 0 ||
			scen > nr_scen || rw < 0 || timeout < 0) {
		pr_info("Invalid variable\n");
		goto out;
	}

	bts = &exynos_bts[ip];

	spin_lock(&bts_lock);
	if (!rw)
		bts->table[scen].stat.timeout_r = timeout;
	else
		bts->table[scen].stat.timeout_w = timeout;

	if (!bts->table[scen].stat.scen_en) {
		bts->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == bts->top_scen)
		bts_setqos(bts->va_base, &bts->table[scen].stat);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_timeout_en_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;
	int i, nr_ip = 0;

	seq_puts(buf, "\tIP/Scen/Enable\nex)echo 0 0 1 > timeout_en\n");
	spin_lock(&bts_lock);

	for (bts = exynos_bts;
			bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (!bts->enable) {
			seq_printf(buf, "[%2d]IP: %s is disabled\n", nr_ip++, bts->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]IP: %s\n", nr_ip++, bts->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s: timeout_en:%d\n",
					bts_scen[i].name,
					bts->table[i].stat.timeout_en);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_timeout_en_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bts_info *bts = NULL;
	char *buf;
	int ip, scen, timeout_en, ret;
	int nr_ip = ARRAY_SIZE(exynos_bts) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d\n", &ip, &scen, &timeout_en);
	if (ret != 3) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (ip < 0 || ip > nr_ip || scen < 0 ||
			scen > nr_scen || timeout_en < 0) {
		pr_info("Invalid variable\n");
		goto out;
	}

	bts = &exynos_bts[ip];

	spin_lock(&bts_lock);
	bts->table[scen].stat.timeout_en = timeout_en;

	if (!bts->table[scen].stat.scen_en) {
		bts->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == bts->top_scen)
		bts_setqos(bts->va_base, &bts->table[scen].stat);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_write_flush_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_info *drex;
	int i, j, nr_drex = 0;

	seq_puts(buf, "\tDREX/Scen/set/config\nex)echo 0 0 0 0xA0201404 > write_flush\n");

	spin_lock(&bts_lock);

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (!drex->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex++, drex->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex++, drex->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			for (j = 0; j < 2; j++)
				seq_printf(buf, "[%d]: 0x%08x\n", j,
						drex->table[i].stat.write_flush_config[j]);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_write_flush_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_info *drex;
	char *buf;
	int drex_ip, scen, set, ret;
	unsigned int config;
	int nr_drex = ARRAY_SIZE(exynos_drex) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %x\n", &drex_ip, &scen, &set, &config);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_ip < 0 || drex_ip > nr_drex || scen < 0 ||
			scen > nr_scen || set < 0 || set >= 2) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex = &exynos_drex[drex_ip];

	spin_lock(&bts_lock);
	drex->table[scen].stat.write_flush_config[set] = config;

	if (!drex->table[scen].stat.scen_en) {
		drex->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex->top_scen)
		bts_set_drex_table(drex);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_vc_timer_th_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_info *drex;
	int i, j, nr_drex = 0;

	seq_puts(buf, "\tDREX/Scen/qos/threshold\nex)echo 0 0 0 0x1D > vc_timer_th\n");

	spin_lock(&bts_lock);

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (!drex->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex++, drex->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex++, drex->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			for (j = 0; j < BTS_VC_TIMER_TH_NR; j++) {
				seq_printf(buf, "[0x%x]: 0x%04x\n", j * 2,
					drex->table[i].stat.vc_timer_th[j] & BTS_VC_TIMER_TH_MASK);
				seq_printf(buf, "[0x%x]: 0x%04x\n", j * 2 + 1,
					(drex->table[i].stat.vc_timer_th[j] >>
						BTS_VC_TIMER_TH_H_SHIFT) & BTS_VC_TIMER_TH_MASK);
			}
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_vc_timer_th_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_info *drex;
	char *buf;
	int drex_ip, scen, qos, set, ret;
	unsigned int threshold;
	int nr_drex = ARRAY_SIZE(exynos_drex) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %x\n", &drex_ip, &scen, &qos, &threshold);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_ip < 0 || drex_ip > nr_drex || scen < 0 ||
			scen > nr_scen || qos < 0 || qos > BTS_PRIORITY_MAX) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex = &exynos_drex[drex_ip];

	spin_lock(&bts_lock);
	set = qos / 2;
	if (qos % 2) {
		drex->table[scen].stat.vc_timer_th[set] &=
			~(BTS_VC_TIMER_TH_MASK << BTS_VC_TIMER_TH_H_SHIFT);
		drex->table[scen].stat.vc_timer_th[set] |=
			(threshold & BTS_VC_TIMER_TH_MASK) << BTS_VC_TIMER_TH_H_SHIFT;
	} else {
		drex->table[scen].stat.vc_timer_th[set] &=
					~(BTS_VC_TIMER_TH_MASK);
		drex->table[scen].stat.vc_timer_th[set] |=
					(threshold & BTS_VC_TIMER_TH_MASK);
	}

	if (!drex->table[scen].stat.scen_en) {
		drex->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex->top_scen)
		bts_set_drex_table(drex);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_cutoff_con_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_info *drex;
	int i, nr_drex = 0;

	seq_puts(buf, "\tDREX/Scen/control\nex)echo 0 0 0x00000005 > cutoff_con\n");

	spin_lock(&bts_lock);

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (!drex->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex++, drex->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex++, drex->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			seq_printf(buf, " : 0x%08x\n", drex->table[i].stat.cutoff_con);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_cutoff_con_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_info *drex;
	char *buf;
	int drex_ip, scen, ret;
	unsigned int control;
	int nr_drex = ARRAY_SIZE(exynos_drex) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %x\n", &drex_ip, &scen, &control);
	if (ret != 3) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_ip < 0 || drex_ip > nr_drex || scen < 0 ||
			scen > nr_scen) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex = &exynos_drex[drex_ip];

	spin_lock(&bts_lock);
	drex->table[scen].stat.cutoff_con = control;

	if (!drex->table[scen].stat.scen_en) {
		drex->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex->top_scen)
		bts_set_drex_table(drex);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_brb_cutoff_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_info *drex;
	int i, nr_drex = 0;

	seq_puts(buf, "\tDREX/Scen/config\nex)echo 0 0 0x00080008 > brb_cutoff\n");

	spin_lock(&bts_lock);

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (!drex->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex++, drex->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex++, drex->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			seq_printf(buf, " : 0x%08x\n", drex->table[i].stat.brb_cutoff_config);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_brb_cutoff_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_info *drex;
	char *buf;
	int drex_ip, scen, ret;
	unsigned int config;
	int nr_drex = ARRAY_SIZE(exynos_drex) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %x\n", &drex_ip, &scen, &config);
	if (ret != 3) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_ip < 0 || drex_ip > nr_drex || scen < 0 ||
			scen > nr_scen) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex = &exynos_drex[drex_ip];

	spin_lock(&bts_lock);
	drex->table[scen].stat.brb_cutoff_config = config;

	if (!drex->table[scen].stat.scen_en) {
		drex->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex->top_scen)
		bts_set_drex_table(drex);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_rdbuf_cutoff_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_info *drex;
	int i, nr_drex = 0;

	seq_puts(buf, "\tDREX/Scen/config\nex)echo 0 0 0x00080008 > rdbuf_cutoff\n");

	spin_lock(&bts_lock);

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (!drex->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex++, drex->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex++, drex->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			seq_printf(buf, " : 0x%08x\n", drex->table[i].stat.rdbuf_cutoff_config);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_rdbuf_cutoff_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_info *drex;
	char *buf;
	int drex_ip, scen, ret;
	unsigned int config;
	int nr_drex = ARRAY_SIZE(exynos_drex) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %x\n", &drex_ip, &scen, &config);
	if (ret != 3) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_ip < 0 || drex_ip > nr_drex || scen < 0 ||
			scen > nr_scen) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex = &exynos_drex[drex_ip];

	spin_lock(&bts_lock);
	drex->table[scen].stat.rdbuf_cutoff_config = config;

	if (!drex->table[scen].stat.scen_en) {
		drex->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex->top_scen)
		bts_set_drex_table(drex);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_rreq_thrt_con_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_pf_info *drex_pf;
	int i, nr_drex_pf = 0;

	seq_puts(buf, "\tDREX/Scen/control\nex)echo 0 0 0x00008000 > rreq_thrt_con\n");

	spin_lock(&bts_lock);

	for (drex_pf = exynos_drex_pf;
		drex_pf <= &exynos_drex_pf[ARRAY_SIZE(exynos_drex_pf) - 1]; drex_pf++) {
		if (!drex_pf->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex_pf++, drex_pf->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex_pf++, drex_pf->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			seq_printf(buf, " : 0x%08x\n", drex_pf->table[i].stat.pf_rreq_thrt_con);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_rreq_thrt_con_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_pf_info *drex_pf;
	char *buf;
	int drex_pf_ip, scen, ret;
	unsigned int control;
	int nr_drex_pf = ARRAY_SIZE(exynos_drex_pf) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %x\n", &drex_pf_ip, &scen, &control);
	if (ret != 3) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_pf_ip < 0 || drex_pf_ip > nr_drex_pf || scen < 0 ||
			scen > nr_scen) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex_pf = &exynos_drex_pf[drex_pf_ip];

	spin_lock(&bts_lock);
	drex_pf->table[scen].stat.pf_rreq_thrt_con = control;

	if (!drex_pf->table[scen].stat.scen_en) {
		drex_pf->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex_pf->top_scen)
		bts_set_drex_pf_table(drex_pf);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_allow_mo_region_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_pf_info *drex_pf;
	int i, nr_drex_pf = 0;

	seq_puts(buf, "\tDREX/Scen/config\nex)echo 0 0 0x02040608 > allow_mo_region\n");

	spin_lock(&bts_lock);

	for (drex_pf = exynos_drex_pf;
		drex_pf <= &exynos_drex_pf[ARRAY_SIZE(exynos_drex_pf) - 1]; drex_pf++) {
		if (!drex_pf->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex_pf++, drex_pf->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex_pf++, drex_pf->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			seq_printf(buf, " : 0x%08x\n", drex_pf->table[i].stat.allow_mo_for_region);
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_allow_mo_region_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_pf_info *drex_pf;
	char *buf;
	int drex_pf_ip, scen, ret;
	unsigned int config;
	int nr_drex_pf = ARRAY_SIZE(exynos_drex_pf) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %x\n", &drex_pf_ip, &scen, &config);
	if (ret != 3) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_pf_ip < 0 || drex_pf_ip > nr_drex_pf || scen < 0 ||
			scen > nr_scen) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex_pf = &exynos_drex_pf[drex_pf_ip];

	spin_lock(&bts_lock);
	drex_pf->table[scen].stat.allow_mo_for_region = config;

	if (!drex_pf->table[scen].stat.scen_en) {
		drex_pf->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex_pf->top_scen)
		bts_set_drex_pf_table(drex_pf);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_pf_qos_timer_status_open_show(struct seq_file *buf, void *d)
{
	struct drex_pf_info *drex_pf;
	int i, j, nr_drex_pf = 0;

	seq_puts(buf, "\tDREX/Scen/qos/timeout\nex)echo 0 0 0 0x5 > pf_qos_timer\n");

	spin_lock(&bts_lock);

	for (drex_pf = exynos_drex_pf;
		drex_pf <= &exynos_drex_pf[ARRAY_SIZE(exynos_drex_pf) - 1]; drex_pf++) {
		if (!drex_pf->enable) {
			seq_printf(buf, "[%2d]DREX: %s is disabled\n", nr_drex_pf++, drex_pf->name);
			continue;
		} else {
			seq_printf(buf, "[%2d]DREX: %s\n", nr_drex_pf++, drex_pf->name);
		}
		for (i = 0; i < BS_MAX; i++) {
			if (!bts_scen[i].name)
				continue;
			seq_printf(buf, "%6s:\n", bts_scen[i].name);
			for (j = 0; j < BTS_PF_TIMER_NR; j++) {
				seq_printf(buf, "[0x%x]: 0x%04x\n", j * 2,
					drex_pf->table[i].stat.pf_qos_timer[j] & BTS_PF_TIMER_MASK);
				seq_printf(buf, "[0x%x]: 0x%04x\n", j * 2 + 1,
					(drex_pf->table[i].stat.pf_qos_timer[j] >>
						BTS_PF_TIMER_H_SHIFT) & BTS_PF_TIMER_MASK);
			}
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static ssize_t exynos_pf_qos_timer_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct drex_pf_info *drex_pf;
	char *buf;
	int drex_pf_ip, scen, qos, set, ret;
	unsigned int timeout;
	int nr_drex_pf = ARRAY_SIZE(exynos_drex_pf) - 1;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d %d %x\n", &drex_pf_ip, &scen, &qos, &timeout);
	if (ret != 4) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (drex_pf_ip < 0 || drex_pf_ip > nr_drex_pf || scen < 0 ||
			scen > nr_scen || qos < 0 || qos > BTS_PRIORITY_MAX) {
		pr_err("Invalid variable\n");
		goto out;
	}

	drex_pf = &exynos_drex_pf[drex_pf_ip];

	spin_lock(&bts_lock);
	set = qos / 2;
	if (qos % 2) {
		drex_pf->table[scen].stat.pf_qos_timer[set] &=
			~(BTS_PF_TIMER_MASK << BTS_PF_TIMER_H_SHIFT);
		drex_pf->table[scen].stat.pf_qos_timer[set] |=
			(timeout & BTS_PF_TIMER_MASK) << BTS_PF_TIMER_H_SHIFT;
	} else {
		drex_pf->table[scen].stat.pf_qos_timer[set] &=
					~(BTS_PF_TIMER_MASK);
		drex_pf->table[scen].stat.pf_qos_timer[set] |=
					(timeout & BTS_PF_TIMER_MASK);
	}

	if (!drex_pf->table[scen].stat.scen_en) {
		drex_pf->table[scen].stat.scen_en = true;
		scen_chaining(scen);
	}

	if (scen == drex_pf->top_scen)
		bts_set_drex_pf_table(drex_pf);
	spin_unlock(&bts_lock);

out:
	kfree(buf);

	return count;
}

static int exynos_bts_scen_test_status_open_show(struct seq_file *buf, void *d)
{
	seq_puts(buf, "\tScen/control\nex)echo 0 1 > bts_scen_test\n");

	return 0;
}

static ssize_t exynos_bts_scen_test_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char *buf;
	int scen, control, ret;
	int nr_scen = ARRAY_SIZE(bts_scen) - 1;
	ssize_t len;

	buf = kmalloc(count, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	len = simple_write_to_buffer(buf, count, ppos, user_buf, count);
	if (len < 0) {
		kfree(buf);
		return len;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d %d\n", &scen, &control);
	if (ret != 2) {
		pr_err("%s, Failed at sscanf function: %d\n", __func__, ret);
		goto out;
	}

	if (scen < 0 ||	scen > nr_scen || control < 0) {
		pr_err("Invalid variable\n");
		goto out;
	}

	bts_update_scen((enum bts_scen_type)scen, control);

out:
	kfree(buf);

	return count;
}

static int exynos_qos_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_qos_status_open_show, inode->i_private);
}

static int exynos_dmc_timeout_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_dmc_timeout_status_open_show, inode->i_private);
}

static int exynos_mo_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_mo_status_open_show, inode->i_private);
}

static int exynos_max_mo_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_max_mo_status_open_show, inode->i_private);
}

static int exynos_full_mo_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_full_mo_status_open_show, inode->i_private);
}

static int exynos_prio_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_prio_status_open_show, inode->i_private);
}

static int exynos_scen_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_scen_status_open_show, inode->i_private);
}

static int exynos_addr_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_addr_status_open_show, inode->i_private);
}

static int exynos_qmax_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_qmax_status_open_show, inode->i_private);
}

static int exynos_timeout_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_timeout_status_open_show, inode->i_private);
}

static int exynos_timeout_en_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_timeout_en_status_open_show, inode->i_private);
}

static int exynos_write_flush_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_write_flush_status_open_show, inode->i_private);
}

static int exynos_vc_timer_th_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_vc_timer_th_status_open_show, inode->i_private);
}

static int exynos_cutoff_con_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_cutoff_con_status_open_show, inode->i_private);
}

static int exynos_brb_cutoff_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_brb_cutoff_status_open_show, inode->i_private);
}

static int exynos_rdbuf_cutoff_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_rdbuf_cutoff_status_open_show, inode->i_private);
}

static int exynos_rreq_thrt_con_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_rreq_thrt_con_status_open_show, inode->i_private);
}

static int exynos_allow_mo_region_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_allow_mo_region_status_open_show, inode->i_private);
}

static int exynos_pf_qos_timer_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_pf_qos_timer_status_open_show, inode->i_private);
}

static int exynos_bts_scen_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_scen_test_status_open_show, inode->i_private);
}

static const struct file_operations debug_qos_status_fops = {
	.open		= exynos_qos_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_dmc_timeout_status_fops = {
	.open		= exynos_dmc_timeout_open,
	.read		= seq_read,
	.write		= exynos_dmc_timeout_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_mo_status_fops = {
	.open		= exynos_mo_open,
	.read		= seq_read,
	.write		= exynos_mo_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_max_mo_status_fops = {
	.open		= exynos_max_mo_open,
	.read		= seq_read,
	.write		= exynos_max_mo_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_full_mo_status_fops = {
	.open		= exynos_full_mo_open,
	.read		= seq_read,
	.write		= exynos_full_mo_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_prio_status_fops = {
	.open		= exynos_prio_open,
	.read		= seq_read,
	.write		= exynos_prio_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_scen_status_fops = {
	.open		= exynos_scen_open,
	.read		= seq_read,
	.write		= exynos_scen_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_addr_status_fops = {
	.open		= exynos_addr_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_qmax_status_fops = {
	.open		= exynos_qmax_open,
	.read		= seq_read,
	.write		= exynos_qmax_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_timeout_status_fops = {
	.open		= exynos_timeout_open,
	.read		= seq_read,
	.write		= exynos_timeout_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_timeout_en_status_fops = {
	.open		= exynos_timeout_en_open,
	.read		= seq_read,
	.write		= exynos_timeout_en_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_write_flush_status_fops = {
	.open		= exynos_write_flush_open,
	.read		= seq_read,
	.write		= exynos_write_flush_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_vc_timer_th_status_fops = {
	.open		= exynos_vc_timer_th_open,
	.read		= seq_read,
	.write		= exynos_vc_timer_th_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_cutoff_con_status_fops = {
	.open		= exynos_cutoff_con_open,
	.read		= seq_read,
	.write		= exynos_cutoff_con_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_brb_cutoff_status_fops = {
	.open		= exynos_brb_cutoff_open,
	.read		= seq_read,
	.write		= exynos_brb_cutoff_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_rdbuf_cutoff_status_fops = {
	.open		= exynos_rdbuf_cutoff_open,
	.read		= seq_read,
	.write		= exynos_rdbuf_cutoff_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_rreq_thrt_con_status_fops = {
	.open		= exynos_rreq_thrt_con_open,
	.read		= seq_read,
	.write		= exynos_rreq_thrt_con_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_allow_mo_region_status_fops = {
	.open		= exynos_allow_mo_region_open,
	.read		= seq_read,
	.write		= exynos_allow_mo_region_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_pf_qos_timer_status_fops = {
	.open		= exynos_pf_qos_timer_open,
	.read		= seq_read,
	.write		= exynos_pf_qos_timer_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_scen_test_status_fops = {
	.open		= exynos_bts_scen_test_open,
	.read		= seq_read,
	.write		= exynos_bts_scen_test_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void bts_debugfs(void)
{
	struct dentry *den;

	den = debugfs_create_dir("bts", NULL);
	if (IS_ERR_OR_NULL(den)) {
		pr_err("%s debugfs create directory failed\n", __func__);
		return;
	}

	debugfs_create_file("qos", 0440, den, NULL, &debug_qos_status_fops);
	debugfs_create_file("mo", 0644, den, NULL, &debug_mo_status_fops);
	debugfs_create_file("max_mo", 0644, den, NULL, &debug_max_mo_status_fops);
	debugfs_create_file("full_mo", 0644, den, NULL, &debug_full_mo_status_fops);
	debugfs_create_file("dmc_timeout", 0644, den, NULL,
					&debug_dmc_timeout_status_fops);
	debugfs_create_file("priority", 0644, den, NULL, &debug_prio_status_fops);
	debugfs_create_file("scenario", 0640, den, NULL, &debug_scen_status_fops);
	debugfs_create_file("address", 0440, den, NULL, &debug_addr_status_fops);
	debugfs_create_file("qmax", 0640, den, NULL, &debug_qmax_status_fops);
	debugfs_create_file("timeout", 0640, den, NULL, &debug_timeout_status_fops);
	debugfs_create_file("timeout_en", 0640, den, NULL, &debug_timeout_en_status_fops);
	debugfs_create_file("write_flush", 0640, den, NULL, &debug_write_flush_status_fops);
	debugfs_create_file("vc_timer_th", 0640, den, NULL, &debug_vc_timer_th_status_fops);
	debugfs_create_file("cutoff_con", 0640, den, NULL, &debug_cutoff_con_status_fops);
	debugfs_create_file("brb_cutoff", 0640, den, NULL, &debug_brb_cutoff_status_fops);
	debugfs_create_file("rdbuf_cutoff", 0640, den, NULL, &debug_rdbuf_cutoff_status_fops);
	debugfs_create_file("rreq_thrt_con", 0640, den, NULL, &debug_rreq_thrt_con_status_fops);
	debugfs_create_file("allow_mo_region", 0640, den, NULL, &debug_allow_mo_region_status_fops);
	debugfs_create_file("pf_qos_timer", 0640, den, NULL, &debug_pf_qos_timer_status_fops);
	debugfs_create_file("bts_scen_test", 0640, den, NULL, &debug_bts_scen_test_status_fops);

	if (!debugfs_create_u32("log", 0644, den, &exynos_bts_log))
		pr_err("[BTS]: could't create debugfs bts log\n");
	if (!debugfs_create_u32("mif_util", 0644, den, &exynos_mif_util))
		pr_err("[BTS]: could't create debugfs mif util\n");
}
#else
static void bts_debugfs(void)
{
	pr_info("%s is disabled, check configuration\n", __func__);
}
#endif

static int __init exynos_bts_init(void)
{
	unsigned int i;
	int ret = 0;
	struct bts_info *bts;
	struct drex_info *drex;
	struct drex_pf_info *drex_pf;

	for (bts = exynos_bts;
		bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		bts->va_base = ioremap(bts->pa_base, SZ_2K);
		if (!bts->va_base) {
			pr_err("failed to map bts physical address\n");
			bts->enable = false;
		}
	}

	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		drex->va_base = ioremap(drex->pa_base, SZ_4K);
		if (!drex->va_base) {
			pr_err("failed to map %s physical address\n", drex->name);
			ret = -ENOMEM;
			goto err_drex;
		}
	}

	for (drex_pf = exynos_drex_pf;
		drex_pf <= &exynos_drex_pf[ARRAY_SIZE(exynos_drex_pf) - 1]; drex_pf++) {
		drex_pf->va_base = ioremap(drex_pf->pa_base, SZ_4K);
		if (!drex_pf->va_base) {
			pr_err("failed to map %s physical address\n", drex_pf->name);
			ret = -ENOMEM;
			goto err_drex_pf;
		}
	}

	for (i = 0; i < ARRAY_SIZE(trex_snode); i++) {
		trex_snode[i].va_base = ioremap(trex_snode[i].pa_base, SZ_1K);
		if (!trex_snode[i].va_base) {
			pr_err("failed to map trex_snode physical address\n");
			ret = -ENOMEM;
			goto err_trex_snode;
		}
	}

	for (i = BS_DEFAULT + 1; i < BS_MAX; i++)
		scen_chaining(i);

	bts_initialize_domains();

	pm_qos_add_request(&exynos_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, 0);
	register_syscore_ops(&exynos_bts_syscore_ops);

	bts_debugfs();
	pr_info("BTS: driver is initialized\n");

	return 0;

err_trex_snode:
	for (i = 0; i < ARRAY_SIZE(trex_snode); i++) {
		if (trex_snode[i].va_base)
			iounmap(trex_snode[i].va_base);
	}

err_drex_pf:
	for (drex_pf = exynos_drex_pf;
		drex_pf <= &exynos_drex_pf[ARRAY_SIZE(exynos_drex_pf) - 1]; drex_pf++) {
		if (drex_pf->va_base)
			iounmap(drex_pf->va_base);
	}

err_drex:
	for (drex = exynos_drex;
		drex <= &exynos_drex[ARRAY_SIZE(exynos_drex) - 1]; drex++) {
		if (drex->va_base)
			iounmap(drex->va_base);
	}

	for (bts = exynos_bts;
		bts <= &exynos_bts[ARRAY_SIZE(exynos_bts) - 1]; bts++) {
		if (bts->enable)
			iounmap(bts->va_base);
	}

	return ret;
}
arch_initcall(exynos_bts_init);
