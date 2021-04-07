/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/bts.h>
#include "cal_bts8890.h"
#include "regs-bts.h"

#define VPP_MAX			9
#define MIF_BLK_NUM		4
#define TREX_SCI_NUM		2

#define BTS_DISP		(BTS_TREX_DISP0_0 | BTS_TREX_DISP0_1 | \
				BTS_TREX_DISP1_0 | BTS_TREX_DISP1_1)
#define BTS_MFC		(BTS_TREX_MFC0 | BTS_TREX_MFC1)
#define BTS_G3D		(BTS_TREX_G3D0 | BTS_TREX_G3D1)
#define BTS_RT			(BTS_TREX_DISP0_0 | BTS_TREX_DISP0_1 | \
				BTS_TREX_DISP1_0 | BTS_TREX_DISP1_1 | \
				BTS_TREX_ISP0 | BTS_TREX_CAM0 | BTS_TREX_CP)

#define update_rot_scen(a)	(pr_state.rot_scen = a)
#define update_g3d_scen(a)	(pr_state.g3d_scen = a)
#define update_urgent_scen(a)	(pr_state.urg_scen = a)

#ifdef BTS_DBGGEN
#define BTS_DBG(x...) 		pr_err(x)
#else
#define BTS_DBG(x...) 		do {} while (0)
#endif

enum bts_index {
	BTS_IDX_TREX_DISP0_0,
	BTS_IDX_TREX_DISP0_1,
	BTS_IDX_TREX_DISP1_0,
	BTS_IDX_TREX_DISP1_1,
	BTS_IDX_TREX_ISP0,
	BTS_IDX_TREX_CAM0,
	BTS_IDX_TREX_CAM1,
	BTS_IDX_TREX_CP,
	BTS_IDX_TREX_MFC0,
	BTS_IDX_TREX_MFC1,
	BTS_IDX_TREX_G3D0,
	BTS_IDX_TREX_G3D1,
	BTS_IDX_TREX_FSYS0,
	BTS_IDX_TREX_FSYS1,
	BTS_IDX_TREX_MSCL0,
	BTS_IDX_TREX_MSCL1,
	BTS_MAX,
};

#define	BTS_TREX_DISP0_0	((u64)1 << (u64)BTS_IDX_TREX_DISP0_0)
#define	BTS_TREX_DISP0_1	((u64)1 << (u64)BTS_IDX_TREX_DISP0_1)
#define	BTS_TREX_DISP1_0	((u64)1 << (u64)BTS_IDX_TREX_DISP1_0)
#define	BTS_TREX_DISP1_1	((u64)1 << (u64)BTS_IDX_TREX_DISP1_1)
#define	BTS_TREX_ISP0		((u64)1 << (u64)BTS_IDX_TREX_ISP0)
#define	BTS_TREX_CAM0		((u64)1 << (u64)BTS_IDX_TREX_CAM0)
#define	BTS_TREX_CAM1		((u64)1 << (u64)BTS_IDX_TREX_CAM1)
#define	BTS_TREX_CP		((u64)1 << (u64)BTS_IDX_TREX_CP)
#define	BTS_TREX_MFC0		((u64)1 << (u64)BTS_IDX_TREX_MFC0)
#define	BTS_TREX_MFC1		((u64)1 << (u64)BTS_IDX_TREX_MFC1)
#define	BTS_TREX_G3D0		((u64)1 << (u64)BTS_IDX_TREX_G3D0)
#define	BTS_TREX_G3D1		((u64)1 << (u64)BTS_IDX_TREX_G3D1)
#define	BTS_TREX_FSYS0		((u64)1 << (u64)BTS_IDX_TREX_FSYS0)
#define	BTS_TREX_FSYS1		((u64)1 << (u64)BTS_IDX_TREX_FSYS1)
#define	BTS_TREX_MSCL0		((u64)1 << (u64)BTS_IDX_TREX_MSCL0)
#define	BTS_TREX_MSCL1		((u64)1 << (u64)BTS_IDX_TREX_MSCL1)

enum exynos_bts_scenario {
	BS_DISABLE,
	BS_DEFAULT,
	BS_ROTATION,
	BS_HIGHPERF,
	BS_G3DFREQ,
	BS_URGENTOFF,
	BS_DEBUG,
	BS_MAX,
};

enum exynos_bts_function {
	BF_SETQOS,
	BF_SETQOS_BW,
	BF_SETQOS_MO,
	BF_SETQOS_FBMBW,
	BF_DISABLE,
	BF_SETTREXQOS,
	BF_SETTREXQOS_MO,
	BF_SETTREXQOS_MO_RT,
	BF_SETTREXQOS_MO_CP,
	BF_SETTREXQOS_MO_CHANGE,
	BF_SETTREXQOS_URGENT_OFF,
	BF_SETTREXQOS_BW,
	BF_SETTREXQOS_FBMBW,
	BF_SETTREXDISABLE,
	BF_NOP,
};

enum vpp_state {
	VPP_BW,
	VPP_ROT_BW,
	VPP_STAT,
};

struct bts_table {
	enum exynos_bts_function fn;
	unsigned int priority;
	unsigned int window;
	unsigned int token;
	unsigned int mo;
	unsigned int fbm;
	unsigned int mask;
	unsigned int timeout;
	unsigned int bypass_en;
	unsigned int decval;
	struct bts_info *next_bts;
	int prev_scen;
	int next_scen;
};

struct bts_info {
	u64 id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table[BS_MAX];
	const char *pd_name;
	bool on;
	struct list_head list;
	bool enable;
	struct clk_info *ct_ptr;
	enum exynos_bts_scenario cur_scen;
	enum exynos_bts_scenario top_scen;
};

struct bts_scen_status {
	bool rot_scen;
	bool g3d_scen;
	bool urg_scen;
};

struct bts_scenario {
	const char *name;
	u64 ip;
	enum exynos_bts_scenario id;
	struct bts_info *head;
};

struct bts_scen_status pr_state = {
	.rot_scen = false,
	.g3d_scen = false,
	.urg_scen = false,
};

struct clk_info {
	const char *clk_name;
	struct clk *clk;
	enum bts_index index;
};

static void __iomem *base_trex[TREX_SCI_NUM];

static DEFINE_MUTEX(media_mutex);

#ifdef CONFIG_EXYNOS8890_BTS_OPTIMIZATION
static unsigned int prev_mo;
static unsigned int vpp_rot[VPP_MAX];
#else
static unsigned int vpp_bw[VPP_STAT][VPP_MAX];
static unsigned int cam_bw, sum_rot_bw, total_bw;
static enum vpp_bw_type vpp_status[VPP_MAX];
static unsigned int mif_freq, int_freq;
#endif
static struct pm_qos_request exynos8_mif_bts_qos;
static struct pm_qos_request exynos8_int_bts_qos;
static struct pm_qos_request exynos8_gpu_mif_bts_qos;
static struct pm_qos_request exynos8_winlayer_mif_bts_qos;
static struct srcu_notifier_head exynos_media_notifier;
static struct clk_info clk_table[0];

static int bts_trex_qosoff(void __iomem *base);
static int bts_trex_qoson(void __iomem *base);

static struct bts_info exynos8_bts[] = {
	[BTS_IDX_TREX_DISP0_0] = {
		.id = BTS_TREX_DISP0_0,
		.name = "disp0_0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_DISP0_0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_DEFAULT].priority = 0x00000008,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].timeout = 0x40,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_ROTATION].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_ROTATION].priority = 0x00000008,
		.table[BS_ROTATION].mo = 0x20,
		.table[BS_ROTATION].timeout = 0x40,
		.table[BS_ROTATION].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_DISP0_1] = {
		.id = BTS_TREX_DISP0_1,
		.name = "disp0_1",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_DISP0_1,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_DEFAULT].priority = 0x00000008,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].timeout = 0x40,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_ROTATION].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_ROTATION].priority = 0x00000008,
		.table[BS_ROTATION].mo = 0x20,
		.table[BS_ROTATION].timeout = 0x40,
		.table[BS_ROTATION].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_DISP1_0] = {
		.id = BTS_TREX_DISP1_0,
		.name = "disp1_0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_DISP1_0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_DEFAULT].priority = 0x00000008,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].timeout = 0x40,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_ROTATION].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_ROTATION].priority = 0x00000008,
		.table[BS_ROTATION].mo = 0x20,
		.table[BS_ROTATION].timeout = 0x40,
		.table[BS_ROTATION].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_DISP1_1] = {
		.id = BTS_TREX_DISP1_1,
		.name = "disp1_1",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_DISP1_1,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_DEFAULT].priority = 0x00000008,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].timeout = 0x40,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_ROTATION].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_ROTATION].priority = 0x00000008,
		.table[BS_ROTATION].mo = 0x20,
		.table[BS_ROTATION].timeout = 0x40,
		.table[BS_ROTATION].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_ISP0] = {
		.id = BTS_TREX_ISP0,
		.name = "isp0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_ISP0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_DEFAULT].priority = 0x0000000A,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].timeout = 0x10,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_CAM0] = {
		.id = BTS_TREX_CAM0,
		.name = "cam0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_CAM0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO_RT,
		.table[BS_DEFAULT].priority = 0x0000000A,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].timeout = 0x10,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_CAM1] = {
		.id = BTS_TREX_CAM1,
		.name = "cam1",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_CAM1,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x0000000A,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_CP] = {
		.id = BTS_TREX_CP,
		.name = "cp",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_CP,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO_CP,
		.table[BS_DEFAULT].priority = 0x0000000C,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].timeout = 0x10,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_URGENTOFF].fn = BF_SETTREXQOS_URGENT_OFF,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_MFC0] = {
		.id = BTS_TREX_MFC0,
		.name = "mfc0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_MFC0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x8,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_HIGHPERF].fn = BF_SETTREXQOS_MO_CHANGE,
		.table[BS_HIGHPERF].mo = 0x400,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_MFC1] = {
		.id = BTS_TREX_MFC1,
		.name = "mfc1",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_MFC1,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x8,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_HIGHPERF].fn = BF_SETTREXQOS_MO_CHANGE,
		.table[BS_HIGHPERF].mo = 0x400,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_G3D0] = {
		.id = BTS_TREX_G3D0,
		.name = "g3d0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_G3D0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_G3DFREQ].fn = BF_SETTREXQOS_MO_CHANGE,
		.table[BS_G3DFREQ].mo = 0x20,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_G3D1] = {
		.id = BTS_TREX_G3D1,
		.name = "g3d1",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_G3D1,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x10,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_G3DFREQ].fn = BF_SETTREXQOS_MO_CHANGE,
		.table[BS_G3DFREQ].mo = 0x20,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FSYS0] = {
		.id = BTS_TREX_FSYS0,
		.name = "fsys0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_FSYS0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x4,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FSYS1] = {
		.id = BTS_TREX_FSYS1,
		.name = "fsys1",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_FSYS1,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x4,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_MSCL0] = {
		.id = BTS_TREX_MSCL0,
		.name = "mscl0",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_MSCL0,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x4,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_MSCL1] = {
		.id = BTS_TREX_MSCL1,
		.name = "mscl1",
		.pd_name = "trex",
		.pa_base = EXYNOS8890_PA_BTS_TREX_MSCL1,
		.table[BS_DEFAULT].fn = BF_SETTREXQOS_MO,
		.table[BS_DEFAULT].priority = 0x00000004,
		.table[BS_DEFAULT].mo = 0x4,
		.table[BS_DEFAULT].bypass_en = 0,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
};

static struct bts_scenario bts_scen[] = {
	[BS_DISABLE] = {
		.name = "bts_disable",
		.id = BS_DISABLE,
	},
	[BS_DEFAULT] = {
		.name = "bts_default",
		.id = BS_DEFAULT,
	},
	[BS_ROTATION] = {
		.name = "bts_rotation",
		.ip = BTS_DISP,
		.id = BS_ROTATION,
	},
	[BS_HIGHPERF] = {
		.name = "bts_mfchighperf",
		.ip = BTS_MFC,
		.id = BS_HIGHPERF,
	},
	[BS_G3DFREQ] = {
		.name = "bts_g3dfreq",
		.ip = BTS_G3D,
		.id = BS_G3DFREQ,
	},
	[BS_URGENTOFF] = {
		.name = "bts_urgentoff",
		.ip = BTS_TREX_CP,
		.id = BS_URGENTOFF,
	},
	[BS_DEBUG] = {
		.name = "bts_dubugging_ip",
		.id = BS_DEBUG,
	},
	[BS_MAX] = {
		.name = "undefined"
	}
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);

static void is_bts_clk_enabled(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;

	if (ptr) {
		btstable_index = ptr->index;
		do {
			if(!__clk_is_enabled(ptr->clk))
				pr_err("[BTS] CLK is not enabled : %s in %s\n",
						ptr->clk_name,
						bts->name);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_clk_on(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;

	if (ptr) {
		btstable_index = ptr->index;
		do {
			clk_enable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_clk_off(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;
	if (ptr) {
		btstable_index = ptr->index;
		do {
			clk_disable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_set_ip_table(enum exynos_bts_scenario scen,
		struct bts_info *bts)
{
	enum exynos_bts_function fn = bts->table[scen].fn;
	int i;

	is_bts_clk_enabled(bts);
	BTS_DBG("[BTS] %s on:%d bts scen: [%s]->[%s]\n", bts->name, bts->on,
			bts_scen[bts->cur_scen].name, bts_scen[scen].name);

	switch (fn) {
	case BF_SETTREXQOS:
		bts_settrexqos(bts->va_base, bts->table[scen].priority, 0, 0);
		break;
	case BF_SETTREXQOS_MO_RT:
		bts_settrexqos_mo_rt(bts->va_base, bts->table[scen].priority, bts->table[scen].mo,
				0, 0, bts->table[scen].timeout, bts->table[scen].bypass_en);
		break;
	case BF_SETTREXQOS_MO_CP:
		bts_settrexqos_mo_cp(bts->va_base, bts->table[scen].priority, bts->table[scen].mo,
				0, 0, bts->table[scen].timeout, bts->table[scen].bypass_en);
		bts_settrexqos_urgent_on(bts->va_base);
		for (i = 0; i < (unsigned int)ARRAY_SIZE(base_trex); i++)
			bts_trex_qoson(base_trex[i]);
		break;
	case BF_SETTREXQOS_MO:
		bts_settrexqos_mo(bts->va_base, bts->table[scen].priority, bts->table[scen].mo, 0, 0);
		break;
	case BF_SETTREXQOS_MO_CHANGE:
		bts_settrexqos_mo_change(bts->va_base, bts->table[scen].mo);
		break;
	case BF_SETTREXQOS_URGENT_OFF:
		bts_settrexqos_urgent_off(bts->va_base);
		for (i = 0; i < (unsigned int)ARRAY_SIZE(base_trex); i++)
			bts_trex_qosoff(base_trex[i]);
		break;
	case BF_SETTREXQOS_BW:
		bts_settrexqos_bw(bts->va_base, bts->table[scen].priority,
				bts->table[scen].decval, 0, 0);
		break;
	case BF_SETTREXQOS_FBMBW:
		bts_settrexqos_fbmbw(bts->va_base, bts->table[scen].priority, 0, 0);
		break;
	case BF_SETTREXDISABLE:
		bts_trexdisable(bts->va_base, 0, 0);
		break;
	case BF_SETQOS:
		bts_setqos(bts->va_base, bts->table[scen].priority, 0);
		break;
	case BF_SETQOS_BW:
		bts_setqos_bw(bts->va_base, bts->table[scen].priority,
				bts->table[scen].window, bts->table[scen].token, 0);
		break;
	case BF_SETQOS_MO:
		bts_setqos_mo(bts->va_base, bts->table[scen].priority, bts->table[scen].mo, 0);
		break;
	case BF_SETQOS_FBMBW:
		bts_setqos_fbmbw(bts->va_base, bts->table[scen].priority, bts->table[scen].window,
				bts->table[scen].token, bts->table[scen].fbm, 0);
		break;
	case BF_DISABLE:
		bts_disable(bts->va_base, 0);
		break;
	case BF_NOP:
		break;
	}

	bts->cur_scen = scen;
}

static enum exynos_bts_scenario bts_get_scen(struct bts_info *bts)
{
	enum exynos_bts_scenario scen;

	scen = BS_DEFAULT;

	return scen;
}


static void bts_add_scen(enum exynos_bts_scenario scen, struct bts_info *bts)
{
	struct bts_info *first = bts;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[bts %s] scen %s off\n",
			bts->name, bts_scen[scen].name);

	do {
		if (bts->enable) {
			if (bts->table[scen].next_scen == 0) {
				if (scen >= bts->top_scen) {
					bts->table[scen].prev_scen = bts->top_scen;
					bts->table[bts->top_scen].next_scen = scen;
					bts->top_scen = scen;
					bts->table[scen].next_scen = -1;

					if(bts->on)
						bts_set_ip_table(bts->top_scen, bts);

				} else {
					for (prev = bts->top_scen; prev > scen; prev = bts->table[prev].prev_scen)
						next = prev;

					bts->table[scen].prev_scen = bts->table[next].prev_scen;
					bts->table[scen].next_scen = bts->table[prev].next_scen;
					bts->table[next].prev_scen = scen;
					bts->table[prev].next_scen = scen;
				}
			}
		}

		bts = bts->table[scen].next_bts;

	} while (bts && bts != first);
}

static void bts_del_scen(enum exynos_bts_scenario scen, struct bts_info *bts)
{
	struct bts_info *first = bts;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[bts %s] scen %s off\n",
			bts->name, bts_scen[scen].name);

	do {
		if (bts->enable) {
			if (bts->table[scen].next_scen != 0) {
				if (scen == bts->top_scen) {
					prev = bts->table[scen].prev_scen;
					bts->top_scen = prev;
					bts->table[prev].next_scen = -1;
					bts->table[scen].next_scen = 0;
					bts->table[scen].prev_scen = 0;

					if (bts->on)
						bts_set_ip_table(prev, bts);
				} else if (scen < bts->top_scen) {
					prev = bts->table[scen].prev_scen;
					next = bts->table[scen].next_scen;

					bts->table[next].prev_scen = bts->table[scen].prev_scen;
					bts->table[prev].next_scen = bts->table[scen].next_scen;

					bts->table[scen].prev_scen = 0;
					bts->table[scen].next_scen = 0;

				} else {
					BTS_DBG("%s scenario couldn't exist above top_scen\n", bts_scen[scen].name);
				}
			}

		}

		bts = bts->table[scen].next_bts;

	} while (bts && bts != first);
}

void bts_scen_update(enum bts_scen_type type, unsigned int val)
{
	enum exynos_bts_scenario scen = BS_DEFAULT;
	struct bts_info *bts = NULL;
	bool on;
	spin_lock(&bts_lock);

	switch (type) {
	case TYPE_ROTATION:
		on = val ? true : false;
		scen = BS_ROTATION;
		bts = &exynos8_bts[BTS_IDX_TREX_DISP0_0];
		BTS_DBG("[BTS] ROTATION: %s\n", bts_scen[scen].name);
		update_rot_scen(val);
		break;
	case TYPE_HIGHPERF:
		on = val ? true : false;
		scen = BS_HIGHPERF;
		bts = &exynos8_bts[BTS_IDX_TREX_MFC0];
		BTS_DBG("[BTS] MFC PERF: %s\n", bts_scen[scen].name);
		break;
	case TYPE_G3D_FREQ:
		on = val ? true : false;
		scen = BS_G3DFREQ;
		bts = &exynos8_bts[BTS_IDX_TREX_G3D0];
		BTS_DBG("[BTS] G3D FREQ: %s\n", bts_scen[scen].name);
		update_g3d_scen(val);
		break;
	case TYPE_URGENT_OFF:
		on = val ? true : false;
		scen = BS_URGENTOFF;
		bts = &exynos8_bts[BTS_IDX_TREX_CP];
		BTS_DBG("[BTS] URGENT: %s\n", bts_scen[scen].name);
		update_urgent_scen(val);
		break;
	default:
		spin_unlock(&bts_lock);
		return;
		break;
	}

	if (on)
		bts_add_scen(scen, bts);
	else
		bts_del_scen(scen, bts);

	spin_unlock(&bts_lock);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	enum exynos_bts_scenario scen = BS_DISABLE;

	spin_lock(&bts_lock);

	list_for_each_entry(bts, &bts_list, list) {
		BTS_DBG("[BTS] %s on/off:%d->%d\n", bts->name, bts->on, on);

		if (!bts->enable) {
			bts->on = on;
			continue;
		}

		scen = bts_get_scen(bts);
		if (on) {
			bts_add_scen(scen, bts);
			if (!bts->on) {
				bts->on = true;
				bts_clk_on(bts);
			}
			bts_set_ip_table(bts->top_scen, bts);
		} else {
			if (bts->on) {
				bts->on = false;
				bts_clk_off(bts);
			}
			bts_del_scen(scen, bts);
		}
	}

	spin_unlock(&bts_lock);
}

static void scen_chaining(enum exynos_bts_scenario scen)
{
	struct bts_info *prev = NULL;
	struct bts_info *first = NULL;
	struct bts_info *bts;

	if (bts_scen[scen].ip) {
		list_for_each_entry(bts, &bts_list, list) {
			if (bts_scen[scen].ip & bts->id) {
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
	}
}

static void bts_trex_init(void __iomem *base)
{
	__raw_writel(0x0B070000, base + SCI_CTRL);
	__raw_writel(0x00200000, base + READ_QURGENT);
	__raw_writel(0x00200000, base + WRITE_QURGENT);
	__raw_writel(0x2A55A954, base + VC_NUM0);
	__raw_writel(0x00000CA0, base + VC_NUM1);
	__raw_writel(0x04040404, base + TH_IMM0);
	__raw_writel(0x04040404, base + TH_IMM1);
	__raw_writel(0x04040404, base + TH_IMM2);
	__raw_writel(0x04040404, base + TH_IMM3);
	__raw_writel(0x04040404, base + TH_IMM4);
	__raw_writel(0x04040004, base + TH_IMM5);
	__raw_writel(0x00040404, base + TH_IMM6);
	__raw_writel(0x02020202, base + TH_HIGH0);
	__raw_writel(0x02020202, base + TH_HIGH1);
	__raw_writel(0x02020202, base + TH_HIGH2);
	__raw_writel(0x02020202, base + TH_HIGH3);
	__raw_writel(0x02020202, base + TH_HIGH4);
	__raw_writel(0x02020002, base + TH_HIGH5);
	__raw_writel(0x00020202, base + TH_HIGH6);

	return;
}

static int bts_trex_qoson(void __iomem *base)
{
	__raw_writel(0x00200000, base + READ_QURGENT);
	__raw_writel(0x00200000, base + WRITE_QURGENT);
	__raw_writel(0x04040004, base + TH_IMM5);
	__raw_writel(0x02020002, base + TH_HIGH5);

	return 0;
}

static int bts_trex_qosoff(void __iomem *base)
{
	__raw_writel(0x00000000, base + READ_QURGENT);
	__raw_writel(0x00000000, base + WRITE_QURGENT);
	__raw_writel(0x04040404, base + TH_IMM5);
	__raw_writel(0x02020202, base + TH_HIGH5);

	return 0;
}

static int exynos_bts_notifier_event(struct notifier_block *this,
		unsigned long event,
		void *ptr)
{
	unsigned long i;

	switch ((unsigned int)event) {
	case PM_POST_SUSPEND:
		for (i = 0; i < (unsigned int)ARRAY_SIZE(base_trex); i++)
			bts_trex_init(base_trex[i]);
		bts_initialize("trex", true);
		return NOTIFY_OK;
		break;
	case PM_SUSPEND_PREPARE:
		return NOTIFY_OK;
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

int bts_update_gpu_mif(unsigned int freq)
{
	int ret = 0;

	if (pm_qos_request_active(&exynos8_gpu_mif_bts_qos))
		pm_qos_update_request(&exynos8_gpu_mif_bts_qos, freq);

	return ret;
}

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
unsigned int ip_sum_bw[IP_NUM];
unsigned int ip_peak_bw[IP_NUM];

void exynos_update_bw(enum bts_media_type ip_type,
		unsigned int sum_bw, unsigned int peak_bw)
{
	unsigned int ip_sum, ip_peak;
	unsigned int int_freq;
	unsigned int mif_freq;
	int i;

	mutex_lock(&media_mutex);

	switch (ip_type) {
	case TYPE_VPP0:
	case TYPE_VPP1:
	case TYPE_VPP2:
	case TYPE_VPP3:
	case TYPE_VPP4:
	case TYPE_VPP5:
	case TYPE_VPP6:
	case TYPE_VPP7:
	case TYPE_VPP8:
		ip_sum_bw[IP_VPP] = sum_bw;
		ip_peak_bw[IP_VPP] = peak_bw;
		break;
	case TYPE_CAM:
		ip_sum_bw[IP_CAM] = sum_bw;
		ip_peak_bw[IP_CAM] = peak_bw;
		break;
	case TYPE_MFC:
		ip_sum_bw[IP_MFC] = sum_bw;
		ip_peak_bw[IP_MFC] = peak_bw;
		break;
	default:
		pr_err("BTS : unsupported ip type - %u", ip_type);
		break;
	}

	ip_sum = 0;
	ip_peak = ip_peak_bw[0];
	for (i = 0; i < IP_NUM; i++) {
		ip_sum += ip_sum_bw[i];
		if (ip_peak < ip_peak_bw[i])
			ip_peak = ip_peak_bw[i];
	}
	BTS_DBG("[BTS BW]: TOTAL SUM: %u, PEAK: %u\n", ip_sum, ip_peak);

	mif_freq = (ip_sum * 100000) / (MIF_UTIL * BUS_WIDTH);
	int_freq = (ip_peak * 100000) / (INT_UTIL * BUS_WIDTH);

	if (mif_freq < 4 * int_freq)
		mif_freq = 4 * int_freq;
	else
		int_freq = mif_freq / 4;
	BTS_DBG("[BTS FREQ]: MIF: %uMHz, INT: %uMHz\n", mif_freq, int_freq);

	if (pm_qos_request_active(&exynos8_mif_bts_qos))
		pm_qos_update_request(&exynos8_mif_bts_qos, mif_freq);
	if (pm_qos_request_active(&exynos8_int_bts_qos))
		pm_qos_update_request(&exynos8_int_bts_qos, int_freq);

	mutex_unlock(&media_mutex);
}

struct bts_vpp_info *exynos_bw_calc(enum bts_media_type ip_type, struct bts_hw *hw)
{
	u64 s_ratio_h, s_ratio_v = 0;
	u64 s_ratio = 0;
	u8 df = 0;
	u8 bpl = 0;
	struct bts_vpp_info *vpp = to_bts_vpp(hw);

	switch (ip_type) {
	case TYPE_VPP0:
	case TYPE_VPP1:
	case TYPE_VPP2:
	case TYPE_VPP3:
	case TYPE_VPP4:
	case TYPE_VPP5:
	case TYPE_VPP6:
	case TYPE_VPP7:
	case TYPE_VPP8:
		s_ratio_h = MULTI_FACTOR * vpp->src_w / vpp->dst_w;
		s_ratio_v = MULTI_FACTOR * vpp->src_h / vpp->dst_h;

		s_ratio = PIXEL_BUFFER * s_ratio_h * s_ratio_v
			/ (MULTI_FACTOR * MULTI_FACTOR);

		bpl = vpp->bpp * 10 / 8;

		if (vpp->bpp == 32)
			df = 20;
		else if (vpp->bpp == 12)
			df = 36;
		else if (vpp->bpp == 16)
			df = 28;

		vpp->cur_bw = vpp->src_h * vpp->src_w * bpl * s_ratio_h * s_ratio_v
			* 72 / (MULTI_FACTOR * MULTI_FACTOR * 10)
			/ (MULTI_FACTOR * MULTI_FACTOR);

		if (vpp->is_rotation)
			vpp->peak_bw = (s_ratio + df * vpp->src_h) * bpl * PEAK_FACTOR
				* (FPS * vpp->dst_h) / VBI_FACTOR
				/ 100 / MULTI_FACTOR;
		else
			vpp->peak_bw = (s_ratio + 4 * vpp->src_w + 1000) * bpl * PEAK_FACTOR
				* (FPS * vpp->dst_h) / VBI_FACTOR
				/ 100 / MULTI_FACTOR;

		BTS_DBG("%s[%i] cur_bw: %llu peak_bw: %llu\n", __func__, (int)ip_type, vpp->cur_bw, vpp->peak_bw);

		break;
	case TYPE_CAM:
	case TYPE_MFC:
	default:
		pr_err("%s: BW calculation unsupported - %u", __func__, ip_type);
		break;
	}

	return vpp;
}

void bts_ext_scenario_set(enum bts_media_type ip_type,
				enum bts_scen_type scen_type, bool on)
{
	int i = 0;
	unsigned int cur_mo;

	switch (ip_type) {
	case TYPE_VPP0:
	case TYPE_VPP1:
	case TYPE_VPP2:
	case TYPE_VPP3:
	case TYPE_VPP4:
	case TYPE_VPP5:
	case TYPE_VPP6:
	case TYPE_VPP7:
	case TYPE_VPP8:
		if (scen_type == TYPE_ROTATION) {
			if (on)
				vpp_rot[ip_type - TYPE_VPP0] = 16;
			else
				vpp_rot[ip_type - TYPE_VPP0] = 8;

			cur_mo = 8;
			for (i = 0; i < VPP_MAX; i++) {
				if (cur_mo < vpp_rot[i])
						cur_mo = vpp_rot[i];
			}

			if (cur_mo == 16 && cur_mo != prev_mo) {
				bts_scen_update(scen_type, 1);
				prev_mo = 16;
			} else if (cur_mo == prev_mo) {
				return;
			} else {
				bts_scen_update(scen_type, 0);
				prev_mo = 8;
			}
		}
		break;

	case TYPE_CAM:
		if (scen_type == TYPE_URGENT_OFF) {
			if (on)
				bts_scen_update(scen_type, 1);
			else
				bts_scen_update(scen_type, 0);
		}
		break;
	case TYPE_MFC:
		if (scen_type == TYPE_HIGHPERF) {
			if (on)
				bts_scen_update(scen_type, 1);
			else
				bts_scen_update(scen_type, 0);
		}
		break;
	case TYPE_G3D:
		if (scen_type == TYPE_G3D_FREQ) {
			if (on)
				bts_scen_update(scen_type, 1);
			else
				bts_scen_update(scen_type, 0);
		}
		break;
	default:
		pr_err("BTS : unsupported rotation ip - %u", ip_type);
		break;
	}

	return;
}

int bts_update_winlayer(unsigned int layers)
{
	unsigned int freq = 0;

	switch (layers) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
		freq = 0;
		break;
	case 5:
		freq = 676000;
		break;
	case 6:
		freq = 845000;
		break;
	case 7:
	case 8:
		freq = 1014000;
		break;
	default:
		break;
	}

	if (pm_qos_request_active(&exynos8_winlayer_mif_bts_qos))
		pm_qos_update_request(&exynos8_winlayer_mif_bts_qos, freq);

	return 0;
}

#else /* BTS_OPTIMIZATION */

void exynos_update_media_scenario(enum bts_media_type media_type,
		unsigned int bw, int bw_type)
{
	unsigned int vpp_total_bw = 0;
	unsigned int vpp_bus_bw[4];
	unsigned int vpp_bus_max_bw;
	int utilization;
	int i;
	int max_status = 0;
	static unsigned int is_yuv;
	static unsigned int prev_sum_rot_bw;
	static unsigned int mif_ud_encoding, mif_ud_decoding;

	mutex_lock(&media_mutex);

	switch (media_type) {
	case TYPE_VPP0:
	case TYPE_VPP1:
	case TYPE_VPP2:
	case TYPE_VPP3:
	case TYPE_VPP4:
	case TYPE_VPP5:
	case TYPE_VPP6:
	case TYPE_VPP7:
	case TYPE_VPP8:
		vpp_bw[VPP_BW][media_type - TYPE_VPP0] = bw;
		vpp_status[media_type - TYPE_VPP0] = bw_type;
		if (bw_type)
			vpp_bw[VPP_ROT_BW][media_type - TYPE_VPP0] = bw;
		else
			vpp_bw[VPP_ROT_BW][media_type - TYPE_VPP0] = 0;

		/*
		 *  VPP0(G0), VPP1(G1), VPP2(VG0), VPP3(VG1),
		 *  VPP4(G2), VPP5(G3), VPP6(VGR0), VPP7(VGR1), VPP8(WB)
		 *  vpp_bus_bw[0] = DISP0_0_BW = G0 + VG0;
		 *  vpp_bus_bw[1] = DISP0_1_BW = G1 + VG1;
		 *  vpp_bus_bw[2] = DISP1_0_BW = G2 + VGR0;
		 *  vpp_bus_bw[3] = DISP1_1_BW = G3 + VGR1 + WB;
		 */
		vpp_bus_bw[0] = vpp_bw[VPP_BW][0] + vpp_bw[VPP_BW][2];
		vpp_bus_bw[1] = vpp_bw[VPP_BW][1] + vpp_bw[VPP_BW][3];
		vpp_bus_bw[2] = vpp_bw[VPP_BW][4] + vpp_bw[VPP_BW][6];
		vpp_bus_bw[3] = vpp_bw[VPP_BW][5] + vpp_bw[VPP_BW][7] + vpp_bw[VPP_BW][8];

		/* sum VPP rotation BW for RT BW */
		sum_rot_bw = 0;
		for (i = 0; i < VPP_MAX; i++)
			sum_rot_bw += vpp_bw[VPP_ROT_BW][i];

		/* Such VPP max BW for INT PM QoS */
		vpp_bus_max_bw = vpp_bus_bw[0];
		for (i = 0; i < 4; i++) {
			if (vpp_bus_max_bw < vpp_bus_bw[i])
				vpp_bus_max_bw = vpp_bus_bw[i];
		}

		prev_sum_rot_bw = sum_rot_bw;
		break;
	case TYPE_CAM:
		cam_bw = bw;
		break;
	case TYPE_YUV:
		is_yuv = bw;
		break;
	case TYPE_UD_ENC:
		mif_ud_encoding = bw;
		break;
	case TYPE_UD_DEC:
		mif_ud_decoding = bw;
		break;
	default:
		pr_err("BTS : unsupported media type - %u", media_type);
		break;
	}

	for (i = 0; i < VPP_MAX; i++)
		vpp_total_bw += vpp_bw[VPP_BW][i];

	total_bw = vpp_total_bw + cam_bw;

	int_freq = (total_bw * 100) / (INT_UTIL * BUS_WIDTH);
	/* INT_L0 shared between camera and normal scenarioes */
	if (int_freq > 468000)
		int_freq = 690000;

	if (pm_qos_request_active(&exynos8_int_bts_qos))
		pm_qos_update_request(&exynos8_int_bts_qos, int_freq);

	for (i = 0; i < VPP_MAX; i++) {
		if (max_status < vpp_status[i])
			max_status = vpp_status[i];
	}

	switch (max_status) {
	case BW_FULLHD_ROT:
		SIZE_FACTOR(total_bw);
	case BW_ROT:
		if (total_bw < 200000)
			utilization = 7;
		else if (total_bw < 500000)
			utilization = 10;
		else if (total_bw < 1600000)
			utilization = 29;
		else if (total_bw < 3000000)
			utilization = 48;
		else if (total_bw < 3500000)
			utilization = 55;
		else if (total_bw < 4000000)
			utilization = 60;
		else if (total_bw < 5000000)
			utilization = 70;
		else if (total_bw < 7000000)
			utilization = 76;
		else
			utilization = 83;
		break;
	case BW_DEFAULT:
		if (total_bw < (63000 * 2) * 8 * 4)
			utilization = MIF_UTIL;
		else
			utilization = MIF_UTIL2;
		break;
	default:
		pr_err("BTS : unsupported bandwidth type - %u", media_type);
		break;
	}

	mif_freq = (total_bw * 100) / (utilization * BUS_WIDTH);

	/* for MFC UHD en/decoding (TBD) */
	if (mif_ud_encoding && mif_freq < MIF_ENCODING)
		mif_freq = MIF_ENCODING;
	if (mif_ud_decoding && mif_freq < MIF_DECODING)
		mif_freq = MIF_DECODING;

	BTS_DBG("[BTS BW] total: %u, vpp: %u, vpp_rot: %u, cam: %u, yuv: %u, ud_en: %u, ud_de:%u\n",
			total_bw, vpp_total_bw, sum_rot_bw, cam_bw, is_yuv,
			mif_ud_encoding, mif_ud_decoding);
		/*
		 *  VPP0(G0), VPP1(G1), VPP2(VG0), VPP3(VG1),
		 *  VPP4(G2), VPP5(G3), VPP6(VGR0), VPP7(VGR1), VPP8(WB)
		 *  vpp_bus_bw[0] = DISP0_0_BW = G0 + VG0;
		 *  vpp_bus_bw[1] = DISP0_1_BW = G1 + VG1;
		 *  vpp_bus_bw[2] = DISP1_0_BW = G2 + VGR0;
		 *  vpp_bus_bw[3] = DISP1_1_BW = G3 + VGR1 + WB;
		 */
	BTS_DBG("[BTS VPP] vpp0(G0) vpp1(G1) vpp2(VG0) vpp3(VG1) vpp4(G2) vpp5(G3) "
				"vpp6(VGR0) vpp7(VGR1) vpp8(WB)\n");
	BTS_DBG("[BTS VPP]");
	for (i = 0; i < VPP_MAX; i++)
		BTS_DBG(" vpp%d %u,", i, vpp_bw[VPP_BW][i]);
	BTS_DBG("\n");
	BTS_DBG("[BTS FREQ] MIF freq: %u, util: %u, INT freq: %u\n",
			mif_freq, utilization, int_freq);

	if (pm_qos_request_active(&exynos8_mif_bts_qos))
		pm_qos_update_request(&exynos8_mif_bts_qos, mif_freq);

	mutex_unlock(&media_mutex);
}
#endif /* BTS_OPT */

static void __iomem *sci_base;
void exynos_bts_scitoken_setting(bool on)
{
	if (on)
		__raw_writel(0x10101117, sci_base + CMDTOKEN);
	else
		__raw_writel(0x10101127, sci_base + CMDTOKEN);
}

#ifdef CONFIG_CPU_IDLE
static int exynos8_bts_lpa_event(struct notifier_block *nb,
				unsigned long event, void *data)
{
	unsigned long i;

	switch (event) {
	case LPA_EXIT:
		for (i = 0; i < (unsigned int)ARRAY_SIZE(base_trex); i++)
			bts_trex_init(base_trex[i]);
		bts_initialize("trex", true);
		break;
	case LPA_ENTER:
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block bts_lpa_nb = {
	.notifier_call = exynos8_bts_lpa_event,
};
#endif

static int __init exynos8_bts_init(void)
{
	signed long i;
	int ret;
	enum bts_index btstable_index = BTS_MAX - 1;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(clk_table); i++) {
		if (btstable_index != clk_table[i].index) {
			btstable_index = clk_table[i].index;
			exynos8_bts[btstable_index].ct_ptr = clk_table + i;
		}
		clk_table[i].clk = clk_get(NULL, clk_table[i].clk_name);

		if (IS_ERR(clk_table[i].clk)){
			BTS_DBG("failed to get bts clk %s\n",
					clk_table[i].clk_name);
			exynos8_bts[btstable_index].ct_ptr = NULL;
		}
		else {
			ret = clk_prepare(clk_table[i].clk);
			if (ret) {
				pr_err("[BTS] failed to prepare bts clk %s\n",
						clk_table[i].clk_name);
				for (; i >= 0; i--)
					clk_put(clk_table[i].clk);
				return ret;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(exynos8_bts); i++) {
		exynos8_bts[i].va_base = ioremap(exynos8_bts[i].pa_base, SZ_16K);

		list_add(&exynos8_bts[i].list, &bts_list);
	}

	base_trex[0] = ioremap(EXYNOS8_PA_TREX_CCORE0, SZ_4K);
	base_trex[1] = ioremap(EXYNOS8_PA_TREX_CCORE1, SZ_4K);

	for (i = BS_DEFAULT + 1; i < BS_MAX; i++)
		scen_chaining(i);

	for (i = 0; i < ARRAY_SIZE(base_trex); i++)
		bts_trex_init(base_trex[i]);

	bts_initialize("trex", true);

	/* SCI Related settings */
	sci_base = ioremap(EXYNOS8_PA_SCI, SZ_4K);
	exynos_bts_scitoken_setting(false);

	pm_qos_add_request(&exynos8_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&exynos8_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
	pm_qos_add_request(&exynos8_gpu_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&exynos8_winlayer_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, 0);

	register_pm_notifier(&exynos_bts_notifier);

	//bts_debugfs();

	srcu_init_notifier_head(&exynos_media_notifier);
#ifdef CONFIG_CPU_IDLE
	exynos_pm_register_notifier(&bts_lpa_nb);
#endif

	return 0;
}
arch_initcall(exynos8_bts_init);
