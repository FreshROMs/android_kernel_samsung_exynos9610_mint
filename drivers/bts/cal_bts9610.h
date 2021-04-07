/* Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - BTS CAL code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BTSCAL_H__
#define __BTSCAL_H__

#include <linux/io.h>
#include <linux/debugfs.h>

#define EXYNOS9610_PA_ABOX		0x12400000
#define EXYNOS9610_PA_COREX		0x12410000
#define EXYNOS9610_PA_CAM		0x12420000
#define EXYNOS9610_PA_DPU		0x12440000
#define EXYNOS9610_PA_DIT		0x12450000
#define EXYNOS9610_PA_FSYS		0x12460000
#define EXYNOS9610_PA_G2D		0x12470000
#define EXYNOS9610_PA_G3D		0x12480000
#define EXYNOS9610_PA_GNSS		0x12490000
#define EXYNOS9610_PA_ISP0		0x124A0000
#define EXYNOS9610_PA_ISP1		0x124B0000
#define EXYNOS9610_PA_MFC0		0x124C0000
#define EXYNOS9610_PA_MFC1		0x124D0000
#define EXYNOS9610_PA_MODEM0		0x124E0000
#define EXYNOS9610_PA_MODEM1		0x124F0000
#define EXYNOS9610_PA_WLBT		0x12500000
#define EXYNOS9610_PA_USB		0x12510000
#define EXYNOS9610_PA_VIPX1		0x12520000
#define EXYNOS9610_PA_VIPX2		0x12530000
#define EXYNOS9610_PA_S_CCI		0x125A0000
#define EXYNOS9610_PA_PERI		0x125B0000
#define EXYNOS9610_PA_SIREX		0x12A00000
#define EXYNOS9610_PA_CPU_DMC0		0x10480000
#define EXYNOS9610_PA_CPU_DMC1		0x10580000
#define EXYNOS9610_PA_DREX0		0x10440000
#define EXYNOS9610_PA_DREX1		0x10540000
#define EXYNOS9610_PA_DREX0_PF		0x10450000
#define EXYNOS9610_PA_DREX1_PF		0x10550000

#define EXYNOS9610_PA_S_NRT0		0x12542000
#define EXYNOS9610_PA_S_NRT1		0x12552000
#define EXYNOS9610_PA_RT_MEM0		0x12562000
#define EXYNOS9610_PA_RT_MEM1		0x12572000
#define EXYNOS9610_PA_CP_MEM0		0x12582000
#define EXYNOS9610_PA_CP_MEM1		0x12592000

/* DREX SFR offset */
#define WRITE_FLUSH_CONFIG0		0x034
#define WRITE_FLUSH_CONFIG1		0x038

#define QOS_TIMEOUT_0			0x300
#define QOS_TIMEOUT_1			0x304
#define QOS_TIMEOUT_2			0x308
#define QOS_TIMEOUT_3			0x30C
#define QOS_TIMEOUT_4			0x310
#define QOS_TIMEOUT_5			0x314
#define QOS_TIMEOUT_6			0x318
#define QOS_TIMEOUT_7			0x31C
#define QOS_TIMEOUT_8			0x320
#define QOS_TIMEOUT_9			0x324
#define QOS_TIMEOUT_A			0x328
#define QOS_TIMEOUT_B			0x32C
#define QOS_TIMEOUT_C			0x330
#define QOS_TIMEOUT_D			0x334
#define QOS_TIMEOUT_E			0x338
#define QOS_TIMEOUT_F			0x33C

#define VC_TIMER_TH_0			0x340
#define VC_TIMER_TH_1			0x344
#define VC_TIMER_TH_2			0x348
#define VC_TIMER_TH_3			0x34C
#define VC_TIMER_TH_4			0x350
#define VC_TIMER_TH_5			0x354
#define VC_TIMER_TH_6			0x358
#define VC_TIMER_TH_7			0x35C

#define CUTOFF_CONTROL			0x370
#define BRB_CUTOFF_CONFIG0		0x374
#define BRB_CUTOFF_CONFIG1		0x378
#define RDBUF_CUTOFF_CONFIG0		0x37C
#define RDBUF_CUTOFF_CONFIG1		0x380

/* DREX_PF SFR offset */
#define PORT_TOKEN_CONTROL		0x020
#define PORT_TOKEN_THRESHOLD0		0x024
#define PORT_TOKEN_THRESHOLD1		0x028

#define PF_RREQ_THROTTLE_CONTROL	0x02C
#define PF_RREQ_THROTTLE_REGION_P2	0x040
#define PF_RREQ_THROTTLE_MO_P2		0x044

#define PF_QOS_TIMER_0			0x070
#define PF_QOS_TIMER_1			0x074
#define PF_QOS_TIMER_2			0x078
#define PF_QOS_TIMER_3			0x07C
#define PF_QOS_TIMER_4			0x080
#define PF_QOS_TIMER_5			0x084
#define PF_QOS_TIMER_6			0x088
#define PF_QOS_TIMER_7			0x08C

#define BTS_MAX_MO			0xffff
#define BTS_QMAX_MAX_THRESHOLD		0xffff
#define BTS_PRIORITY_MAX		0xF
#define BTS_VC_TIMER_TH_NR		8
#define BTS_VC_TIMER_TH_H_SHIFT		16
#define BTS_VC_TIMER_TH_MASK		0x1FF

#define BTS_PF_TIMER_NR			8
#define BTS_PF_TIMER_H_SHIFT		16
#define BTS_PF_TIMER_MASK		0x1FF

struct bts_status {
	bool scen_en;
	unsigned int priority;
	bool disable;
	bool bypass_en;
	bool timeout_en;
	unsigned int rmo;
	unsigned int wmo;
	unsigned int full_rmo;
	unsigned int full_wmo;
	unsigned int busy_rmo;
	unsigned int busy_wmo;
	unsigned int max_rmo;
	unsigned int max_wmo;
	unsigned int timeout_r;
	unsigned int timeout_w;
};

void bts_setqos(void __iomem *base, struct bts_status *stat);
void bts_showqos(void __iomem *base, struct seq_file *buf);
void bts_set_qmax(void __iomem *base, unsigned int r_thsd0,
			unsigned int r_thsd1, unsigned int w_thsd0,
			unsigned int w_thsd1);
void bts_show_qmax(void __iomem *base, struct seq_file *buf);

#endif
