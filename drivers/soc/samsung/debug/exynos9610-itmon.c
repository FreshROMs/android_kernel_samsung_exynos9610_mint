/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver for Samsung Exynos9610 SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/bitops.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-itmon.h>
#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif

#define OFFSET_TMOUT_REG		(0x2000)
#define OFFSET_REQ_R			(0x0)
#define OFFSET_REQ_W			(0x20)
#define OFFSET_RESP_R			(0x40)
#define OFFSET_RESP_W			(0x60)
#define OFFSET_ERR_REPT			(0x20)
#define OFFSET_HW_ASSERT		(0x100)
#define OFFSET_NUM			(0x4)

#define REG_INT_MASK			(0x0)
#define REG_INT_CLR			(0x4)
#define REG_INT_INFO			(0x8)
#define REG_EXT_INFO_0			(0x10)
#define REG_EXT_INFO_1			(0x14)
#define REG_EXT_INFO_2			(0x18)

#define REG_DBG_CTL			(0x10)
#define REG_TMOUT_INIT_VAL		(0x14)
#define REG_TMOUT_FRZ_EN		(0x18)
#define REG_TMOUT_BUF_WR_OFFSET 	(0x20)

#define REG_TMOUT_BUF_STATUS		(0x1C)
#define REG_TMOUT_BUF_POINT_ADDR	(0x20)
#define REG_TMOUT_BUF_ID		(0x24)
#define REG_TMOUT_BUF_PAYLOAD		(0x28)
#define REG_TMOUT_BUF_PAYLOAD_SRAM1	(0x30)
#define REG_TMOUT_BUF_PAYLOAD_SRAM2	(0x34)
#define REG_TMOUT_BUF_PAYLOAD_SRAM3	(0x38)

#define REG_HWA_CTL			(0x4)
#define REG_HWA_INT			(0x8)
#define REG_HWA_INT_ID			(0xC)
#define REG_HWA_START_ADDR_LOW		(0x10)
#define REG_HWA_END_ADDR_LOW		(0x14)
#define REG_HWA_START_END_ADDR_UPPER	(0x18)

#define RD_RESP_INT_ENABLE		(1 << 0)
#define WR_RESP_INT_ENABLE		(1 << 1)
#define ARLEN_RLAST_INT_ENABLE		(1 << 2)
#define AWLEN_WLAST_INT_ENABLE		(1 << 3)
#define INTEND_ACCESS_INT_ENABLE	(1 << 4)

#define BIT_HWA_ERR_OCCURRED(x)		(((x) & (0x1 << 0)) >> 0)
#define BIT_HWA_ERR_CODE(x)		(((x) & (0xF << 1)) >> 28)

#define BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_AXID(x)			(((x) & (0xFFFF)))
#define BIT_AXUSER(x)			(((x) & (0xFFFF << 16)) >> 16)
#define BIT_AXBURST(x)			(((x) & (0x3)))
#define BIT_AXPROT(x)			(((x) & (0x3 << 2)) >> 2)
#define BIT_AXLEN(x)			(((x) & (0xF << 16)) >> 16)
#define BIT_AXSIZE(x)			(((x) & (0x7 << 28)) >> 28)

#define M_NODE				(0)
#define T_S_NODE			(1)
#define T_M_NODE			(2)
#define S_NODE				(3)
#define NODE_TYPE			(4)

#define ERRCODE_SLVERR			(0)
#define ERRCODE_DECERR			(1)
#define ERRCODE_UNSUPORTED		(2)
#define ERRCODE_POWER_DOWN		(3)
#define ERRCODE_UNKNOWN_4		(4)
#define ERRCODE_UNKNOWN_5		(5)
#define ERRCODE_TMOUT			(6)

#define BUS_DATA			(0)
#define BUS_PERI			(1)
#define BUS_PATH_TYPE			(2)

#define TRANS_TYPE_WRITE		(0)
#define TRANS_TYPE_READ			(1)
#define TRANS_TYPE_NUM			(2)

#define FROM_PERI			(0)
#define FROM_CPU			(1)
#define FROM_CP				(2)

#define CP_COMMON_STR			"CP_"

#define TMOUT				(0xFFFFF)
#define TMOUT_TEST			(0x1)

#define PANIC_ALLOWED_THRESHOLD		(0x2)
#define INVALID_REMAPPING		(0x08000000)
#define BAAW_RETURN			(0x08000000)

static bool initial_multi_irq_enable = false;
static struct itmon_dev *g_itmon = NULL;

struct itmon_rpathinfo {
	unsigned int id;
	char *port_name;
	char *dest_name;
	unsigned int bits;
	unsigned int shift_bits;
};

struct itmon_masterinfo {
	char *port_name;
	unsigned int user;
	char *master_name;
	unsigned int bits;
};

struct itmon_nodegroup;

struct itmon_traceinfo {
	char *port;
	char *master;
	char *dest;
	unsigned long target_addr;
	unsigned int errcode;
	bool read;
	bool path_dirty;
	bool snode_dirty;
	bool dirty;
	unsigned long from;
	char buf[SZ_32];
};

struct itmon_tracedata {
	unsigned int int_info;
	unsigned int ext_info_0;
	unsigned int ext_info_1;
	unsigned int ext_info_2;
	unsigned int hwa_ctl;
	unsigned int hwa_info;
	unsigned int hwa_int_id;
	unsigned int offset;
	bool logging;
	bool read;
};

struct itmon_nodeinfo {
	unsigned int type;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	unsigned int time_val;
	bool tmout_enabled;
	bool tmout_frz_enabled;
	bool err_enabled;
	bool hw_assert_enabled;
	bool retention;
	struct itmon_tracedata tracedata;
	struct itmon_nodegroup *group;
	struct list_head list;
};

static const char *itmon_pathtype[] = {
	"DATA Path transaction (0x2000_0000 ~ 0xf_ffff_ffff)",
	"PERI(SFR) Path transaction (0x0 ~ 0x1fff_ffff)",
};

/* Error Code Description */
static const char *itmon_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Unsupported transaction",
	"Unsupported transaction",
	"Timeout error - response timeout in timeout value",
	"Invalid errorcode",
};

static const char *itmon_nodestring[] = {
	"M_NODE",
	"TAXI_S_NODE",
	"TAXI_M_NODE",
	"S_NODE",
};

struct itmon_nodegroup {
	int irq;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	struct itmon_nodeinfo *nodeinfo;
	unsigned int nodesize;
	unsigned int bus_type;
};

struct itmon_platdata {
	const struct itmon_rpathinfo *rpathinfo;
	const struct itmon_masterinfo *masterinfo;
	struct itmon_nodegroup *nodegroup;
	struct itmon_traceinfo traceinfo[BUS_PATH_TYPE];
	struct list_head tracelist[BUS_PATH_TYPE];
	unsigned int err_cnt;
	bool panic_allowed;
	bool crash_in_progress;
	unsigned int sysfs_tmout_val;
	bool sysfs_scandump;
	bool probed;
};

static struct itmon_rpathinfo rpathinfo[] = {
	/* Data BUS */
	{0,	"MFC0",		"S_CCI",	GENMASK(3, 0),	0},
	{1,	"MFC1",		"S_CCI",	GENMASK(3, 0),	0},
	{2,	"VIPX2",	"S_CCI",	GENMASK(3, 0),	0},
	{3,	"DIT",		"S_CCI",	GENMASK(3, 0),	0},
	{4,	"G2D",		"S_CCI",	GENMASK(3, 0),	0},
	{5,	"FSYS",		"S_CCI",	GENMASK(3, 0),	0},
	{6,	"USB",		"S_CCI",	GENMASK(3, 0),	0},
	{7,	"ISP0",		"S_CCI",	GENMASK(3, 0),	0},
	{8,	"ISP1",		"S_CCI",	GENMASK(3, 0),	0},
	{9,	"CAM",		"S_CCI",	GENMASK(3, 0),	0},
	{10,	"DPU",		"S_CCI",	GENMASK(3, 0),	0},
	{11,	"VIPX1",	"S_CCI",	GENMASK(3, 0),	0},

	{0,	"CP_1",		"S_NRT",	GENMASK(3, 0),	0},
	{1,	"ISP0",		"S_NRT",	GENMASK(3, 0),	0},
	{2,	"ISP1",		"S_NRT",	GENMASK(3, 0),	0},
	{3,	"VIPX1",	"S_NRT",	GENMASK(3, 0),	0},
	{4,	"VIPX2",	"S_NRT",	GENMASK(3, 0),	0},
	{5,	"MFC0",		"S_NRT",	GENMASK(3, 0),	0},
	{6,	"MFC1",		"S_NRT",	GENMASK(3, 0),	0},
	{7,	"G2D",		"S_NRT",	GENMASK(3, 0),	0},
	{8,	"FSYS",		"S_NRT",	GENMASK(3, 0),	0},
	{9,	"USB",		"S_NRT",	GENMASK(3, 0),	0},
	{10,	"COREX",	"S_NRT",	GENMASK(3, 0),	0},
	{11,	"GNSS",		"S_NRT",	GENMASK(3, 0),	0},
	{12,	"WLBT",		"S_NRT",	GENMASK(3, 0),	0},
	{13,	"DIT",		"S_NRT",	GENMASK(3, 0),	0},
	{14,	"CSSYS",	"S_NRT",	GENMASK(3, 0),	0},

	{0,	"CAM",		"RT_MEM",	GENMASK(3, 0),	0},
	{1,	"DPU",		"RT_MEM",	GENMASK(3, 0),	0},
	{2,	"ABOX",		"RT_MEM",	GENMASK(3, 0),	0},
	{3,	"CP_1",		"RT_MEM",	GENMASK(3, 0),	0},
	{4,	"WLBT",		"RT_MEM",	GENMASK(3, 0),	0},
	{5,	"GNSS",		"RT_MEM",	GENMASK(3, 0),	0},
	{6,	"VIPX1",	"RT_MEM",	GENMASK(3, 0),	0},
	{7,	"VIPX2",	"RT_MEM",	GENMASK(3, 0),	0},
	{8,	"ISP0",		"RT_MEM",	GENMASK(3, 0),	0},
	{9,	"ISP1",		"RT_MEM",	GENMASK(3, 0),	0},

	{0,	"CP_0",		"CP_MEM",	GENMASK(3, 0),	0},
	{1,	"ABOX",		"CP_MEM",	GENMASK(3, 0),	0},
	{2,	"CP_1",		"CP_MEM",	GENMASK(3, 0),	0},
	{3,	"WLBT",		"CP_MEM",	GENMASK(3, 0),	0},
	{4,	"DIT",		"CP_MEM",	GENMASK(3, 0),	0},

	/* Peri BUS */
	{0,	"G3D",		"PERI",		GENMASK(4, 0),	0},
	{1,	"MFC0",		"PERI",		GENMASK(4, 0),	0},
	{2,	"ISP1",		"PERI",		GENMASK(4, 0),	0},
	{3,	"CAM",		"PERI",		GENMASK(4, 0),	0},
	{4,	"DPU",		"PERI",		GENMASK(4, 0),	0},
	{5,	"ABOX",		"PERI",		GENMASK(4, 0),	0},
	{6,	"WLBT",		"PERI",		GENMASK(4, 0),	0},
	{7,	"CP_0",		"PERI",		GENMASK(4, 0),	0},
	{8,	"CP_1",		"PERI",		GENMASK(4, 0),	0},
	{9,	"VIPX1",	"PERI",		GENMASK(4, 0),	0},
	{10,	"VIPX2",	"PERI",		GENMASK(4, 0),	0},
	{11,	"DIT",		"PERI",		GENMASK(4, 0),	0},
	{12,	"MFC1",		"PERI",		GENMASK(4, 0),	0},
	{13,	"G2D",		"PERI",		GENMASK(4, 0),	0},
	{14,	"FSYS",		"PERI",		GENMASK(4, 0),	0},
	{15,	"USB",		"PERI",		GENMASK(4, 0),	0},
	{16,	"COREX",	"PERI",		GENMASK(4, 0),	0},
	{17,	"GNSS",		"PERI",		GENMASK(4, 0),	0},
	{18,	"CSSYS",	"PERI",		GENMASK(4, 0),	0},
	{19,	"ISP0",		"PERI",		GENMASK(4, 0),	0},
};

/* XIU ID Information */
static struct itmon_masterinfo masterinfo[] = {
	/* BLK_CAM */
	{"CAM",		0,				"PAF-STAT",	GENMASK(2, 1)},
	{"CAM",		BIT(1),				"3AA",		GENMASK(2, 1)},

	/* BLK_DISPAUD */
	{"DPU",		0,				"IDMA0",	0},
	{"ABOX",	0,				"SPUS_SPUM",	GENMASK(1, 1)},
	{"ABOX",	BIT(1),				"ABOX_CA7",	GENMASK(1, 1)},

	/* BLK_VIPX1 */
	{"VIPX1",	0,				"SDMA",		GENMASK(1, 1)},
	{"VIPX1",	BIT(1),				"CM7",		GENMASK(1, 1)},

	/* BLK_VIPX2 */
	{"VIPX2",	0,				"SDMA",		0},

	/* BLK_ISP */
	{"ISP0",	0,				"FIMC-ISP",	GENMASK(2, 1)},
	{"ISP0",	BIT(1),				"VRA",		GENMASK(2, 1)},
	{"ISP0",	BIT(2),				"GDC",		GENMASK(2, 1)},
	{"ISP1",	0,				"MC_SCALER",	0},

	/* BLK_FSYS */
	{"FSYS",	0,				"UFS",		GENMASK(1, 0)},
	{"FSYS",	BIT(0),				"MMC_CARD",	GENMASK(1, 0)},
	{"FSYS",	BIT(1),				"SSS",		GENMASK(1, 0)},
	{"FSYS",	BIT(1) | BIT(0),		"RTIC",		GENMASK(1, 0)},

	/* BLK_USB */
	{"USB",		0,				"USB",		0},

	/* BLK_APM */
	{"COREX",	0,				"APM",		GENMASK(1, 0)},

	/* BLK_SHUB */
	{"COREX",	BIT(0),				"CM4_SHUB_CD",	GENMASK(3, 0)},
	{"COREX",	BIT(0) | BIT(2),		"CM4_SHUB_P",	GENMASK(3, 0)},
	{"COREX",	BIT(0) | BIT(3),		"PDMA_SHUB",	GENMASK(3, 0)},

	/* BLK_CORE */
	{"COREX",	BIT(1),				"PDMA",		GENMASK(1, 0)},
	{"COREX",	BIT(0) | BIT(1),		"SPDMA",	GENMASK(1, 0)},
	{"SIREX",	0,				"SIREX",	0},
	{"DIT",		0,				"DIT",		0},

	/* BLK_G3D - Unique ID */
	{"G3D",		0,				"",		0},

	/* BLK_MFC */
	{"MFC0",	0,				"MFC0",		0},
	{"MFC1",	0,				"MFC1",		GENMASK(1, 1)},
	{"MFC1",	BIT(1),				"WFD",		GENMASK(1, 1)},

	/* BLK_G2D */
	{"G2D",		0,				"JPEG",		GENMASK(2, 1)},
	{"G2D",		BIT(1),				"MSCL",		GENMASK(2, 1)},
	{"G2D",		BIT(2),				"G2D",		GENMASK(2, 1)},

	/* BLK_CP */
	{"CP_0",		0,				"UCPUM",	GENMASK(5, 0)},
	{"CP_0",		BIT(0),				"DMA0",		GENMASK(5, 0)},
	{"CP_0",		BIT(0) | BIT(3),		"DMA1",		GENMASK(5, 0)},
	{"CP_0",		BIT(0) | BIT(4),		"DMA2",		GENMASK(5, 0)},
	{"CP_0",		BIT(0) | BIT(2),		"LCPUMtoL2",	GENMASK(5, 0)},
	{"CP_0",		BIT(2),				"LMAC",		GENMASK(5, 0)},
	{"CP_0",		BIT(1),				"CSXAP",	GENMASK(5, 0)},
	{"CP_0",		BIT(0) | BIT(1),		"DATAMOVER",	GENMASK(5, 0)},
	{"CP_0",		BIT(0) | BIT(1) | BIT(4),	"BAYES",	GENMASK(5, 0)},
	{"CP_0",		BIT(0) | BIT(1) | BIT(3),	"LOGGER",	GENMASK(5, 0)},
	{"CP_0",		BIT(1) | BIT(2),		"HARQMOVER",	GENMASK(5, 0)},

	/* BLK_CP */
	{"CP_1",		0,				"UCPUM",	GENMASK(5, 0)},
	{"CP_1",		BIT(0),				"DMA0",		GENMASK(5, 0)},
	{"CP_1",		BIT(0) | BIT(3),		"DMA1",		GENMASK(5, 0)},
	{"CP_1",		BIT(0) | BIT(4),		"DMA2",		GENMASK(5, 0)},
	{"CP_1",		BIT(0) | BIT(2),		"LCPUMtoL2",	GENMASK(5, 0)},
	{"CP_1",		BIT(2),				"LMAC",		GENMASK(5, 0)},
	{"CP_1",		BIT(1),				"CSXAP",	GENMASK(5, 0)},
	{"CP_1",		BIT(0) | BIT(1),		"DATAMOVER",	GENMASK(5, 0)},
	{"CP_1",		BIT(0) | BIT(1) | BIT(4),	"BAYES",	GENMASK(5, 0)},
	{"CP_1",		BIT(0) | BIT(1) | BIT(3),	"LOGGER",	GENMASK(5, 0)},
	{"CP_1",		BIT(1) | BIT(2),		"HARQMOVER",	GENMASK(5, 0)},

	/* BLK_WLBT */
	{"WLBT",	0,				"CR7",		GENMASK(2, 0)},
	{"WLBT",	BIT(0),				"XDMA",		GENMASK(2, 0)},
	{"WLBT",	BIT(1),				"ENC_DMA0",	GENMASK(2, 0)},
	{"WLBT",	BIT(0) | BIT(1),		"ENC_DMA0",	GENMASK(2, 0)},
	{"WLBT",	BIT(2),				"BTLC",		GENMASK(2, 0)},

	/* BLK_GNSS */
	{"GNSS",	0,				"CM7F_S0",	GENMASK(2, 0)},
	{"GNSS",	BIT(0),				"CM7F_S1_AHB",	GENMASK(2, 0)},
	{"GNSS",	BIT(1),				"XDMA0",	GENMASK(2, 0)},
	{"GNSS",	BIT(0) | BIT(1),		"XDMA1",	GENMASK(1, 0)},
};

/* data_path is sorted by INT_VEC_DEBUG_INTERRUPT_VECTOR_TABLE bits */
static struct itmon_nodeinfo data_path[] = {
	{M_NODE, "ABOX",		0x12403000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CAM",			0x12423000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "COREX",		0x12413000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CSSYS",		0x12433000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "DIT",			0x12453000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "DPU",			0x12443000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "FSYS",		0x12463000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "G2D",			0x12473000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "G3D",			0x12483000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "GNSS",		0x12493000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "ISP0",		0x124A3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "ISP1",		0x124B3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "MFC0",		0x124C3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "MFC1",		0x124D3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CP_0",		0x124E3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CP_1",		0x124F3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "USB",			0x12513000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "VIPX1",		0x12523000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "VIPX2",		0x12533000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "WLBT",		0x12503000, NULL, 0,	   false, false,  true, true, false},
	{S_NODE, "CP_MEM0",		0x12583000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "CP_MEM1",		0x12593000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "PERI",		0x125B3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "RT_MEM0",		0x12563000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "RT_MEM1",		0x12573000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "S_CCI",		0x125A3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "S_NRT0",		0x12543000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "S_NRT1",		0x12553000, NULL, TMOUT,   true,  false,  true, true, false},
};

/* peri_path is sorted by INT_VEC_DEBUG_INTERRUPT_VECTOR_TABLE bits */
static struct itmon_nodeinfo trex_d_nrt[] = {
	{M_NODE, "M_NRT0",		0x12A13000, NULL, 0,	   false, false, true, true, false},
	{M_NODE, "M_NRT1",		0x12A23000, NULL, 0,	   false, false, true, true, false},
	{M_NODE, "SIREX",		0x12A03000, NULL, 0,	   false, false, true, true, false},
	{M_NODE, "NRT_MEM0",		0x12A33000, NULL, 0,	   false, false, true, true, false},
	{M_NODE, "NRT_MEM1",		0x12A43000, NULL, 0,	   false, false, true, true, false},
};

/* peri_path is sorted by INT_VEC_DEBUG_INTERRUPT_VECTOR_TABLE bits */
static struct itmon_nodeinfo peri_path[] = {
	{M_NODE, "CPU_TO_SFR",		0x12803000, NULL, 0,	   false, false, true, true, false},
	{M_NODE, "PERI_TO_SFR",		0x12813000, NULL, 0,	   false, false, true, true, false},
	{S_NODE, "APMP",		0x12833000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "CAMP",		0x12843000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "COREP_SFR",		0x12893000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "COREP_TREX",		0x12883000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "CPU_CL0P",		0x12853000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "CPU_CL1P",		0x12863000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "CSSYS",		0x12873000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "DISPAUDP",		0x128A3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "FSYSP",		0x128B3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "G2DP",		0x128C3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "G3DP",		0x128D3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "GICP",		0x128E3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "GNSSP",		0x128F3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "ISPP",		0x12903000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "MFCP",		0x12913000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "MIF0P",		0x12923000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "MIF1P",		0x12933000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "CP_P",		0x12943000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "PERIP",		0x12953000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "SHUBP",		0x12963000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "SIREXP",		0x12973000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "USBP",		0x12983000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "VIPX1P",		0x129A3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "VIPX2P",		0x129B3000, NULL, TMOUT,   true,  false, true, true, false},
	{S_NODE, "WLBTP",		0x12993000, NULL, TMOUT,   true,  false, true, true, false},
};

static struct itmon_nodegroup nodegroup[] = {
	{306, "TREX_D_CORE",	0x127F3000, NULL, data_path, ARRAY_SIZE(data_path), BUS_DATA},
	{320, "TREX_D_NRT",	0x12BF3000, NULL, trex_d_nrt, ARRAY_SIZE(trex_d_nrt), BUS_DATA},
	{307, "TREX_P_CORE",	0x129F3000, NULL, peri_path, ARRAY_SIZE(peri_path), BUS_PERI},
};

struct itmon_dev {
	struct device *dev;
	struct itmon_platdata *pdata;
	struct of_device_id *match;
	int irq;
	int id;
	void __iomem *regs;
	spinlock_t ctrl_lock;
	struct itmon_notifier notifier_info;
};

struct itmon_panic_block {
	struct notifier_block nb_panic_block;
	struct itmon_dev *pdev;
};

/* declare notifier_list */
ATOMIC_NOTIFIER_HEAD(itmon_notifier_list);

static const struct of_device_id itmon_dt_match[] = {
	{.compatible = "samsung,exynos-itmon",
	 .data = NULL,},
	{},
};
MODULE_DEVICE_TABLE(of, itmon_dt_match);

#define EXYNOS_PMU_BURNIN_CTRL		0x0A08
#define BIT_ENABLE_DBGSEL_WDTRESET	BIT(25)
#ifdef CONFIG_S3C2410_WATCHDOG
extern int s3c2410wdt_set_emergency_reset(unsigned int timeout, int index);
#else
#define s3c2410wdt_set_emergency_reset(a, b)	do { } while (0)
#endif
static void itmon_switch_scandump(void)
{
	unsigned int val;
	int ret;

	ret = exynos_pmu_read(EXYNOS_PMU_BURNIN_CTRL, &val);
	ret = exynos_pmu_write(EXYNOS_PMU_BURNIN_CTRL, val | BIT_ENABLE_DBGSEL_WDTRESET);
	s3c2410wdt_set_emergency_reset(5, 0);
}

static struct itmon_rpathinfo *itmon_get_rpathinfo(struct itmon_dev *itmon,
					       unsigned int id,
					       char *dest_name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_rpathinfo *rpath = NULL;
	int i;

	if (!dest_name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(rpathinfo); i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (dest_name && !strncmp(pdata->rpathinfo[i].dest_name,
						  dest_name,
						  strlen(pdata->rpathinfo[i].dest_name))) {
				rpath = (struct itmon_rpathinfo *)&pdata->rpathinfo[i];
				break;
			}
		}
	}
	return rpath;
}

static struct itmon_masterinfo *itmon_get_masterinfo(struct itmon_dev *itmon,
						 char *port_name,
						 unsigned int user)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_masterinfo *master = NULL;
	unsigned int val;
	int i;

	if (!port_name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(masterinfo); i++) {
		if (!strncmp(pdata->masterinfo[i].port_name, port_name, strlen(port_name))) {
			val = user & pdata->masterinfo[i].bits;
			if (val == pdata->masterinfo[i].user) {
				master = (struct itmon_masterinfo *)&pdata->masterinfo[i];
				break;
			}
		}
	}
	return master;
}

static void itmon_init(struct itmon_dev *itmon, bool enabled)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node;
	unsigned int offset;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].type == S_NODE && node[j].tmout_enabled) {
				offset = OFFSET_TMOUT_REG;
				/* Enable Timeout setting */
				__raw_writel(enabled, node[j].regs + offset + REG_DBG_CTL);
				/* set tmout interval value */
				__raw_writel(node[j].time_val,
					     node[j].regs + offset + REG_TMOUT_INIT_VAL);
				pr_debug("Exynos ITMON - %s timeout enabled\n", node[j].name);
				if (node[j].tmout_frz_enabled) {
					/* Enable freezing */
					__raw_writel(enabled,
						     node[j].regs + offset + REG_TMOUT_FRZ_EN);
				}
			}
			if (node[j].err_enabled) {
				/* clear previous interrupt of req_read */
				offset = OFFSET_REQ_R;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of req_write */
				offset = OFFSET_REQ_W;
				if (pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_read */
				offset = OFFSET_RESP_R;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_write */
				offset = OFFSET_RESP_W;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);
				pr_debug("Exynos ITMON - %s error reporting enabled\n", node[j].name);
			}
			if (node[j].hw_assert_enabled) {
				offset = OFFSET_HW_ASSERT;
				__raw_writel(RD_RESP_INT_ENABLE | WR_RESP_INT_ENABLE |
					     ARLEN_RLAST_INT_ENABLE | AWLEN_WLAST_INT_ENABLE,
						node[j].regs + offset + REG_HWA_CTL);
			}
		}
	}
}

void itmon_enable(bool enabled)
{
	if (g_itmon)
		itmon_init(g_itmon, enabled);
}

void itmon_set_errcnt(int cnt)
{
	struct itmon_platdata *pdata;

	if (g_itmon) {
		pdata = g_itmon->pdata;
		pdata->err_cnt = cnt;
	}
}

static void itmon_post_handler_to_notifier(struct itmon_dev *itmon,
					   unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];

	/* After treatment by port */
	if (!traceinfo->port || strlen(traceinfo->port) < 1)
		return;

	itmon->notifier_info.port = traceinfo->port;
	itmon->notifier_info.master = traceinfo->master;
	itmon->notifier_info.dest = traceinfo->dest;
	itmon->notifier_info.read = traceinfo->read;
	itmon->notifier_info.target_addr = traceinfo->target_addr;
	itmon->notifier_info.errcode = traceinfo->errcode;

	/* call notifier_call_chain of itmon */
	atomic_notifier_call_chain(&itmon_notifier_list, 0, &itmon->notifier_info);
}

static void itmon_post_handler_by_master(struct itmon_dev *itmon,
					unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];

	/* After treatment by port */
	if (!traceinfo->port || strlen(traceinfo->port) < 1)
		return;

	if (!strncmp(traceinfo->port, "CPU", strlen("CPU"))) {
		/* if master is CPU, then we expect any exception */
		if (pdata->err_cnt > PANIC_ALLOWED_THRESHOLD) {
			pdata->err_cnt = 0;
			itmon_init(itmon, false);
			pr_info("ITMON is turn-off when CPU transaction is detected repeatly\n");
		} else {
			pr_info("ITMON skips CPU transaction detected\n");
		}
	} else if (!strncmp(traceinfo->port, CP_COMMON_STR, strlen(CP_COMMON_STR))) {
		/* if master is DSP and operation is read, we don't care this */
		if (traceinfo->master && traceinfo->target_addr == INVALID_REMAPPING &&
			!strncmp(traceinfo->master, "CR4MtoL2", strlen(traceinfo->master))) {
			pdata->err_cnt = 0;
			pr_info("ITMON skips CP's DSP(CR4MtoL2) detected\n");
		} else {
			/* Disable busmon all interrupts */
			itmon_init(itmon, false);
			/* TODO: CP Crash operation */
		}
	}
}

void itmon_report_timeout(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	unsigned int info, axid, valid, timeout, payload;
	unsigned long addr;
	char *master_name, *port_name;
	struct itmon_rpathinfo *port;
	struct itmon_masterinfo *master;
	int i, num = (trans_type == TRANS_TYPE_READ ? SZ_128 : SZ_64);
	int fz_offset = (trans_type == TRANS_TYPE_READ ? 0 : REG_TMOUT_BUF_WR_OFFSET);

	pr_info("\n      TIMEOUT_BUFFER Information\n\n");
	pr_info("      > NUM|   BLOCK|  MASTER|   VALID| TIMEOUT|      ID|   ADDRESS|    INFO|\n");

	for (i = 0; i < num; i++) {
		writel(i, node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_POINT_ADDR + fz_offset);
		axid = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_ID + fz_offset);
		payload = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD + fz_offset);
		addr = (((unsigned long)readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_SRAM1 + fz_offset) &
				GENMASK(15, 0)) << 32ULL);
		addr |= (readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_SRAM2 + fz_offset));
		info = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_SRAM3 + fz_offset);

		valid = payload & BIT(0);
		timeout = (payload & GENMASK(19, 16)) >> 16;

		port = (struct itmon_rpathinfo *)
				itmon_get_rpathinfo(itmon, axid, node->name);
		if (port) {
			port_name = port->port_name;
			master = (struct itmon_masterinfo *)
				itmon_get_masterinfo(itmon, port_name,
							axid >> port->shift_bits);
			if (master)
				master_name = master->master_name;
			else
				master_name = "Unknown";
		} else {
			port_name = "Unknown";
			master_name = "Unknown";
		}
		pr_info("      > %03d|%8s|%8s|%8u|%8x|%08x|%010zx|%08x|\n",
				i, port_name, master_name, valid, timeout, axid, addr, info);
	}
	pr_info("--------------------------------------------------------------------------\n");
}

static unsigned int power(unsigned int param, unsigned int num)
{
	if (num == 0)
		return 1;
	return param * (power(param, num - 1));
}

static void itmon_report_traceinfo(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];
	struct itmon_nodegroup *group = NULL;
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	char temp_buf[SZ_128];
#endif

	if (!traceinfo->dirty)
		return;

	pr_auto(ASL3,
		"--------------------------------------------------------------------------\n"
		"      Transaction Information\n\n"
		"      > Master         : %s %s\n"
		"      > Target         : %s\n"
		"      > Target Address : 0x%lX %s\n"
		"      > Type           : %s\n"
		"      > Error code     : %s\n",
		traceinfo->port, traceinfo->master ? traceinfo->master : "",
		traceinfo->dest ? traceinfo->dest : "Unknown",
		traceinfo->target_addr,
		(unsigned int)traceinfo->target_addr == INVALID_REMAPPING ?
		"(BAAW Remapped address)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[traceinfo->errcode]);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	snprintf(temp_buf, SZ_128, "%s %s/ %s/ 0x%zx %s/ %s/ %s",
		traceinfo->port, traceinfo->master ? traceinfo->master : "",
		traceinfo->dest ? traceinfo->dest : "Unknown",
		traceinfo->target_addr,
		traceinfo->target_addr == INVALID_REMAPPING ?
		"(by CP maybe)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[traceinfo->errcode]);
	sec_debug_set_extra_info_busmon(temp_buf);
#endif

	if (node) {
		struct itmon_tracedata *tracedata = &node->tracedata;

		pr_auto(ASL3,
			"      > Size           : %u bytes x %u burst => %u bytes\n"
			"      > Burst Type     : %u (0:FIXED, 1:INCR, 2:WRAP)\n"
			"      > Level          : %s\n"
			"      > Protection     : %s\n",
			power(BIT_AXSIZE(tracedata->ext_info_1), 2), BIT_AXLEN(tracedata->ext_info_1) + 1,
			power(BIT_AXSIZE(tracedata->ext_info_1), 2) * (BIT_AXLEN(tracedata->ext_info_1) + 1),
			BIT_AXBURST(tracedata->ext_info_2),
			(BIT_AXPROT(tracedata->ext_info_2) & 0x1) ? "Privileged access" : "Unprivileged access",
			(BIT_AXPROT(tracedata->ext_info_2) & 0x2) ? "Non-secure access" : "Secure access");

		group = node->group;
		pr_auto(ASL3,
			"      > Path Type      : %s\n"
			"--------------------------------------------------------------------------\n",
			itmon_pathtype[group->bus_type]);

	} else {
		pr_auto(ASL3, "--------------------------------------------------------------------------\n");
	}
}

static void itmon_report_pathinfo(struct itmon_dev *itmon,
				  struct itmon_nodeinfo *node,
				  unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_tracedata *tracedata = &node->tracedata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];

	if (!traceinfo->path_dirty) {
		pr_auto(ASL3,
			"--------------------------------------------------------------------------\n"
			"      ITMON Report (%s)\n"
			"--------------------------------------------------------------------------\n"
			"      PATH Information\n",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE");
		traceinfo->path_dirty = true;
	}
	switch (node->type) {
	case M_NODE:
		pr_auto(ASL3, " > %14s, %8s(0x%08X)\n",
			node->name, "M_NODE", node->phy_regs + tracedata->offset);
		break;
	case T_S_NODE:
		pr_auto(ASL3, " > %14s, %8s(0x%08X)\n",
			node->name, "T_S_NODE", node->phy_regs + tracedata->offset);
		break;
	case T_M_NODE:
		pr_auto(ASL3, " > %14s, %8s(0x%08X)\n",
			node->name, "T_M_NODE", node->phy_regs + tracedata->offset);
		break;
	case S_NODE:
		pr_auto(ASL3, " > %14s, %8s(0x%08X)\n",
			node->name, "S_NODE", node->phy_regs + tracedata->offset);
		break;
	}
}

static void itmon_report_tracedata(struct itmon_dev *itmon,
				   struct itmon_nodeinfo *node,
				   unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_tracedata *tracedata = &node->tracedata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];
	struct itmon_masterinfo *master;
	struct itmon_rpathinfo *port;
	unsigned int errcode, axid;

	errcode = BIT_ERR_CODE(tracedata->int_info);
	axid = BIT_AXID(tracedata->int_info);

	switch (node->type) {
	case M_NODE:
		/* In this case, we can get information from M_NODE
		 * Fill traceinfo->port / target_addr / read / master */
		if (BIT_ERR_VALID(tracedata->int_info) && tracedata->ext_info_2) {
			/* If only detecting M_NODE only(DECERR) */
			traceinfo->port = node->name;
			master = (struct itmon_masterinfo *)
				itmon_get_masterinfo(itmon, node->name, axid);
			if (master)
				traceinfo->master = master->master_name;
			else
				traceinfo->master = NULL;

			traceinfo->target_addr =
				(((unsigned long)node->tracedata.ext_info_1
				& GENMASK(3, 0)) << 32ULL);
			traceinfo->target_addr |= node->tracedata.ext_info_0;
			traceinfo->read = tracedata->read;
			traceinfo->errcode = errcode;
			traceinfo->dirty = true;
		} else {
			traceinfo->master = NULL;
			traceinfo->target_addr = 0;
			traceinfo->read = tracedata->read;
			traceinfo->port = node->name;
			traceinfo->errcode = errcode;
			traceinfo->dirty = true;
		}
		itmon_report_pathinfo(itmon, node, trans_type);
		break;
	case S_NODE:
		/*
		 * In DECERR case, the follow information was already filled in M_NODE.
		 */
		port = (struct itmon_rpathinfo *) itmon_get_rpathinfo(itmon, axid, node->name);

		if (port) {
			struct itmon_nodeinfo *m_node, *next_m_node;
			struct itmon_tracedata *m_tracedata;
			unsigned int m_axid;

			traceinfo->port = port->port_name;
			list_for_each_entry_safe(m_node, next_m_node,
					&pdata->tracelist[trans_type], list) {
				if (m_node && m_node->name && port->port_name && m_node->type == M_NODE &&
					strncmp(m_node->name, port->port_name,
						strlen(port->port_name)) == 0) {
					m_tracedata = &m_node->tracedata;
					m_axid = BIT_AXID(m_tracedata->int_info);
					master = (struct itmon_masterinfo *)
						itmon_get_masterinfo(itmon, traceinfo->port, m_axid);
					if (master) {
						traceinfo->master = master->master_name;
						break;
					}
				}
			}
		}
		if (!traceinfo->port)
			traceinfo->port = "Unknown";
		if (!traceinfo->master)
			traceinfo->master = "Unknown";

		traceinfo->target_addr =
			(((unsigned long)node->tracedata.ext_info_1
			& GENMASK(3, 0)) << 32ULL);
		traceinfo->target_addr |= node->tracedata.ext_info_0;
		traceinfo->errcode = errcode;
		traceinfo->dest = node->name;
		traceinfo->dirty = true;
		traceinfo->snode_dirty = true;
		itmon_report_pathinfo(itmon, node, trans_type);
		itmon_report_traceinfo(itmon, node, trans_type);
		break;
	default:
		pr_info("Unknown Error - offset:%u\n", tracedata->offset);
		break;
	}
}

static void itmon_report_hwa_rawdata(struct itmon_dev *itmon,
				     struct itmon_nodeinfo *node)
{
	unsigned int hwa_ctl, hwa_info, hwa_int_id;

	hwa_ctl = __raw_readl(node->regs +  OFFSET_HW_ASSERT + REG_HWA_CTL);
	hwa_info = __raw_readl(node->regs +  OFFSET_HW_ASSERT + REG_HWA_INT);
	hwa_int_id = __raw_readl(node->regs + OFFSET_HW_ASSERT + REG_HWA_INT_ID);

	/* Output Raw register information */
	pr_info("--------------------------------------------------------------------------\n"
		"      HWA Raw Register Information(ITMON information)\n\n");
	pr_info("      > %s(%s, 0x%08X)\n"
		"      > REG(0x104~0x10C)      : 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_nodestring[node->type],
		node->phy_regs,
		hwa_ctl,
		hwa_info,
		hwa_int_id);
}

static void itmon_report_rawdata(struct itmon_dev *itmon,
				 struct itmon_nodeinfo *node,
				 unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];
	struct itmon_tracedata *tracedata = &node->tracedata;

	/* Output Raw register information */
	pr_info("      > %s(%s, 0x%08X)\n"
		"      > REG(0x08~0x18)        : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n"
		"      > REG(0x104~0x10C)      : 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_nodestring[node->type],
		node->phy_regs + tracedata->offset,
		tracedata->int_info,
		tracedata->ext_info_0,
		tracedata->ext_info_1,
		tracedata->ext_info_2,
		tracedata->hwa_ctl,
		tracedata->hwa_info,
		tracedata->hwa_int_id);

	/* If node is to DREX S_NODE, Outputing timeout freezing result */
	if (node->type == S_NODE && traceinfo->errcode == ERRCODE_TMOUT)
		itmon_report_timeout(itmon, node, trans_type);
}

static void itmon_route_tracedata(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo;
	struct itmon_nodeinfo *node, *next_node;
	unsigned int trans_type;
	int i;

	/* To call function is sorted by declaration */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		for (i = M_NODE; i < NODE_TYPE; i++) {
			list_for_each_entry(node, &pdata->tracelist[trans_type], list) {
				if (i == node->type)
					itmon_report_tracedata(itmon, node, trans_type);
			}
		}
		/* If there is no S_NODE information, check one more */
		traceinfo = &pdata->traceinfo[trans_type];
		if (!traceinfo->snode_dirty)
			itmon_report_traceinfo(itmon, NULL, trans_type);
	}

	if (pdata->traceinfo[TRANS_TYPE_READ].dirty ||
		pdata->traceinfo[TRANS_TYPE_WRITE].dirty)
		pr_auto(ASL3, " Raw Register Information(ITMON Internal Information)\n\n");

	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		for (i = M_NODE; i < NODE_TYPE; i++) {
			list_for_each_entry_safe(node, next_node, &pdata->tracelist[trans_type], list) {
				if (i == node->type) {
					itmon_report_rawdata(itmon, node, trans_type);
					/* clean up */
					list_del(&node->list);
					kfree(node);
				}
			}
		}
	}

	if (pdata->traceinfo[TRANS_TYPE_READ].dirty ||
		pdata->traceinfo[TRANS_TYPE_WRITE].dirty)
		pr_auto(ASL3, "--------------------------------------------------------------------------\n");

	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		itmon_post_handler_to_notifier(itmon, trans_type);
		itmon_post_handler_by_master(itmon, trans_type);
	}
}

static void itmon_trace_data(struct itmon_dev *itmon,
			    struct itmon_nodegroup *group,
			    struct itmon_nodeinfo *node,
			    unsigned int offset)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *new_node = NULL;
	unsigned int int_info, info0, info1, info2;
	unsigned int hwa_ctl, hwa_info, hwa_int_id;
	bool read = TRANS_TYPE_WRITE;
	bool req = false;

	int_info = __raw_readl(node->regs + offset + REG_INT_INFO);
	info0 = __raw_readl(node->regs + offset + REG_EXT_INFO_0);
	info1 = __raw_readl(node->regs + offset + REG_EXT_INFO_1);
	info2 = __raw_readl(node->regs + offset + REG_EXT_INFO_2);

	hwa_ctl = __raw_readl(node->regs +  OFFSET_HW_ASSERT + REG_HWA_CTL);
	hwa_info = __raw_readl(node->regs +  OFFSET_HW_ASSERT + REG_HWA_INT);
	hwa_int_id = __raw_readl(node->regs + OFFSET_HW_ASSERT + REG_HWA_INT_ID);

	switch (offset) {
	case OFFSET_REQ_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_REQ_W:
		req = true;
		/* Only S-Node is able to make log to registers */
		break;
	case OFFSET_RESP_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_RESP_W:
		req = false;
		/* Only NOT S-Node is able to make log to registers */
		break;
	default:
		pr_auto(ASL3, "Unknown Error - node:%s offset:%u\n", node->name, offset);
		break;
	}

	new_node = kmalloc(sizeof(struct itmon_nodeinfo), GFP_ATOMIC);
	if (new_node) {
		/* Fill detected node information to tracedata's list */
		memcpy(new_node, node, sizeof(struct itmon_nodeinfo));
		new_node->tracedata.int_info = int_info;
		new_node->tracedata.ext_info_0 = info0;
		new_node->tracedata.ext_info_1 = info1;
		new_node->tracedata.ext_info_2 = info2;
		new_node->tracedata.hwa_ctl = hwa_ctl;
		new_node->tracedata.hwa_info = hwa_info;
		new_node->tracedata.hwa_int_id = hwa_int_id;

		new_node->tracedata.offset = offset;
		new_node->tracedata.read = read;
		new_node->group = group;
		if (BIT_ERR_VALID(int_info))
			node->tracedata.logging = true;
		else
			node->tracedata.logging = false;

		list_add(&new_node->list, &pdata->tracelist[read]);
	} else {
		pr_auto(ASL3, "failed to kmalloc for %s node %x offset\n",
			node->name, offset);
	}
}

static int itmon_search_node(struct itmon_dev *itmon, struct itmon_nodegroup *group, bool clear)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = NULL;
	unsigned int val, offset;
	unsigned long vec, flags, bit = 0;
	int i, j, ret = 0;

	spin_lock_irqsave(&itmon->ctrl_lock, flags);
	memset(pdata->traceinfo, 0, sizeof(struct itmon_traceinfo) * 2);
	if (group) {
		/* Processing only this group and select detected node */
		vec = (unsigned long)__raw_readl(group->regs);
		node = group->nodeinfo;
		if (!vec)
			goto exit;

		for_each_set_bit(bit, &vec, group->nodesize) {
			/* exist array */
			for (i = 0; i < OFFSET_NUM; i++) {
				offset = i * OFFSET_ERR_REPT;
				/* Check Request information */
				val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
				if (BIT_ERR_OCCURRED(val)) {
					/* This node occurs the error */
					itmon_trace_data(itmon, group, &node[bit], offset);
					if (clear)
						__raw_writel(1, node[bit].regs
								+ offset + REG_INT_CLR);
					ret = true;
				}
			}
			/* Check H/W assertion */
			if (node[bit].hw_assert_enabled) {
				val = __raw_readl(node[bit].regs + OFFSET_HW_ASSERT +
							REG_HWA_INT);
				if (BIT_HWA_ERR_OCCURRED(val)) {
					itmon_report_hwa_rawdata(itmon, &node[bit]);
					/* Go panic now */
					pdata->err_cnt = PANIC_ALLOWED_THRESHOLD + 1;
					ret = true;
				}
			}
		}
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
			group = &nodegroup[i];
			if (group->phy_regs)
				vec = (unsigned long)__raw_readl(group->regs);
			else
				vec = GENMASK(group->nodesize, 0);

			node = group->nodeinfo;
			bit = 0;

			for_each_set_bit(bit, &vec, group->nodesize) {
				for (j = 0; j < OFFSET_NUM; j++) {
					offset = j * OFFSET_ERR_REPT;
					/* Check Request information */
					val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
					if (BIT_ERR_OCCURRED(val)) {
						/* This node occurs the error */
						itmon_trace_data(itmon, group, &node[bit], offset);
						if (clear)
							__raw_writel(1, node[bit].regs
									+ offset + REG_INT_CLR);
						ret = true;
					}
				}
				/* Check H/W assertion */
				if (node[bit].hw_assert_enabled) {
					val = __raw_readl(node[bit].regs + OFFSET_HW_ASSERT +
								REG_HWA_INT);
					if (BIT_HWA_ERR_OCCURRED(val)) {
						itmon_report_hwa_rawdata(itmon, &node[bit]);
						/* Go panic now */
						pdata->err_cnt = PANIC_ALLOWED_THRESHOLD + 1;
						ret = true;
					}
				}
			}
		}
	}
	itmon_route_tracedata(itmon);
 exit:
	spin_unlock_irqrestore(&itmon->ctrl_lock, flags);
	return ret;
}

static irqreturn_t itmon_irq_handler(int irq, void *data)
{
	struct itmon_dev *itmon = (struct itmon_dev *)data;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group = NULL;
	bool ret;
	int i;

	/* Search itmon group */
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		if (irq == nodegroup[i].irq) {
			group = &pdata->nodegroup[i];
			if (group->phy_regs != 0) {
				pr_info("\nITMON Detected: %d irq, %s group, 0x%x vec, err_cnt:%u\n",
					irq, group->name, __raw_readl(group->regs), pdata->err_cnt);
			} else {
				pr_info("\nITMON Detected: %d irq, %s group, err_cnt:%u\n",
					irq, group->name, pdata->err_cnt);
			}
			break;
		}
	}

	ret = itmon_search_node(itmon, NULL, true);
	if (!ret) {
		pr_info("ITMON could not detect any error\n");
	} else {
		if (pdata->sysfs_scandump) {
			itmon_switch_scandump();
			wfi();
		}
		if (pdata->err_cnt++ > PANIC_ALLOWED_THRESHOLD)
			pdata->panic_allowed = true;
	}

	if (pdata->panic_allowed)
		panic("ITMON occurs panic, Transaction is invalid from IPs");

	return IRQ_HANDLED;
}

void itmon_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&itmon_notifier_list, block);
}

static struct bus_type itmon_subsys = {
	.name = "itmon",
	.dev_name = "itmon",
};

static ssize_t itmon_timeout_fix_val_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
	struct itmon_platdata *pdata = g_itmon->pdata;

	n = scnprintf(buf + n, 24, "set timeout val: 0x%x\n", pdata->sysfs_tmout_val);

	return n;
}

static ssize_t itmon_timeout_fix_val_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = simple_strtoul(buf, NULL, 0);
	struct itmon_platdata *pdata = g_itmon->pdata;

	if (val > 0UL && val <= 0xFFFFFUL)
		pdata->sysfs_tmout_val = (unsigned int)val;

	return count;
}

static ssize_t itmon_scandump_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
	struct itmon_platdata *pdata = g_itmon->pdata;

	n = scnprintf(buf + n, 30, "scandump mode is %sable : %d\n",
		pdata->sysfs_scandump == 1 ? "en" : "dis",
		pdata->sysfs_scandump);

	return n;
}

static ssize_t itmon_scandump_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = simple_strtoul(buf, NULL, 0);
	struct itmon_platdata *pdata = g_itmon->pdata;

	if (val > 0UL && val <= 0xFFFFFUL) {
		pdata = g_itmon->pdata;
		pdata->sysfs_scandump = (unsigned int)val;
	}

	return count;
}

static ssize_t itmon_timeout_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned long i, offset;
	ssize_t n = 0;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	/* Processing all group & nodes */
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE) {
				n += scnprintf(buf + n, 60, "%-12s : 0x%08X, timeout : %x\n",
					node[bit].name, node[bit].phy_regs,
					__raw_readl(node[bit].regs + offset + REG_DBG_CTL));
			}
		}
	}
	return n;
}

static ssize_t itmon_timeout_val_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned long i, offset;
	ssize_t n = 0;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	/* Processing all group & nodes */
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE) {
				n += scnprintf(buf + n, 60, "%-12s : 0x%08X, timeout : 0x%x\n",
					node[bit].name, node[bit].phy_regs,
					__raw_readl(node[bit].regs + offset + REG_TMOUT_INIT_VAL));
			}
		}
	}
	return n;
}

static ssize_t itmon_timeout_freeze_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned long i, offset;
	ssize_t n = 0;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	/* Processing all group & nodes */
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE) {
				n += scnprintf(buf + n, 60, "%-12s : 0x%08X, timeout_freeze : %x\n",
					node[bit].name, node[bit].phy_regs,
					__raw_readl(node[bit].regs + offset + REG_TMOUT_FRZ_EN));
			}
		}
	}
	return n;
}

static ssize_t itmon_timeout_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	char *name;
	unsigned int val, offset, i;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	if (!name)
		return count;

	name[count - 1] = '\0';
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE &&
				!strncmp(name, node[bit].name, strlen(name))) {
				val = __raw_readl(node[bit].regs + offset + REG_DBG_CTL);
				if (!val)
					val = 1;
				else
					val = 0;
				__raw_writel(val, node[bit].regs + offset + REG_DBG_CTL);
				node[bit].tmout_enabled = val;
			}
		}
	}
	kfree(name);
	return count;
}

static ssize_t itmon_timeout_val_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	char *name;
	unsigned int offset, i;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;
	struct itmon_platdata *pdata = g_itmon->pdata;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	if (!name)
		return count;

	name[count - 1] = '\0';
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE &&
				!strncmp(name, node[bit].name, strlen(name))) {
				__raw_writel(pdata->sysfs_tmout_val,
						node[bit].regs + offset + REG_TMOUT_INIT_VAL);
				node[bit].time_val = pdata->sysfs_tmout_val;
			}
		}
	}
	kfree(name);
	return count;
}

static ssize_t itmon_timeout_freeze_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	char *name;
	unsigned int val, offset, i;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	if (!name)
		return count;

	name[count - 1] = '\0';
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE &&
				!strncmp(name, node[bit].name, strlen(name))) {
				val = __raw_readl(node[bit].regs + offset + REG_TMOUT_FRZ_EN);
				if (!val)
					val = 1;
				else
					val = 0;
				__raw_writel(val, node[bit].regs + offset + REG_TMOUT_FRZ_EN);
				node[bit].tmout_frz_enabled = val;
			}
		}
	}
	kfree(name);
	return count;
}

static struct kobj_attribute itmon_timeout_attr =
	__ATTR(timeout_en, 0644, itmon_timeout_show, itmon_timeout_store);
static struct kobj_attribute itmon_timeout_fix_attr =
	__ATTR(set_val, 0644, itmon_timeout_fix_val_show, itmon_timeout_fix_val_store);
static struct kobj_attribute itmon_scandump_attr =
	__ATTR(scandump_en, 0644, itmon_scandump_show, itmon_scandump_store);
static struct kobj_attribute itmon_timeout_val_attr =
	__ATTR(timeout_val, 0644, itmon_timeout_val_show, itmon_timeout_val_store);
static struct kobj_attribute itmon_timeout_freeze_attr =
	__ATTR(timeout_freeze, 0644, itmon_timeout_freeze_show, itmon_timeout_freeze_store);

static struct attribute *itmon_sysfs_attrs[] = {
	&itmon_timeout_attr.attr,
	&itmon_timeout_fix_attr.attr,
	&itmon_timeout_val_attr.attr,
	&itmon_timeout_freeze_attr.attr,
	&itmon_scandump_attr.attr,
	NULL,
};

static struct attribute_group itmon_sysfs_group = {
	.attrs = itmon_sysfs_attrs,
};

static const struct attribute_group *itmon_sysfs_groups[] = {
	&itmon_sysfs_group,
	NULL,
};

static int __init itmon_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&itmon_subsys, itmon_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-snapshop subsys\n");

	return ret;
}
late_initcall(itmon_sysfs_init);

static int itmon_logging_panic_handler(struct notifier_block *nb,
				     unsigned long l, void *buf)
{
	struct itmon_panic_block *itmon_panic = (struct itmon_panic_block *)nb;
	struct itmon_dev *itmon = itmon_panic->pdev;
	struct itmon_platdata *pdata = itmon->pdata;
	int ret;

	if (!IS_ERR_OR_NULL(itmon)) {
		/* Check error has been logged */
		ret = itmon_search_node(itmon, NULL, false);
		if (!ret) {
			pr_info("No found error in %s\n", __func__);
		} else {
			pr_info("Found errors in %s\n", __func__);
			if (pdata->sysfs_scandump) {
				itmon_switch_scandump();
				wfi();
			}
		}
	}
	return 0;
}

static int itmon_probe(struct platform_device *pdev)
{
	struct itmon_dev *itmon;
	struct itmon_panic_block *itmon_panic = NULL;
	struct itmon_platdata *pdata;
	struct itmon_nodeinfo *node;
	unsigned int irq_option = 0, irq;
	char *dev_name;
	int ret, i, j;

	itmon = devm_kzalloc(&pdev->dev, sizeof(struct itmon_dev), GFP_KERNEL);
	if (!itmon) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				    "private data\n");
		return -ENOMEM;
	}
	itmon->dev = &pdev->dev;

	spin_lock_init(&itmon->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct itmon_platdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				    "platform data\n");
		return -ENOMEM;
	}
	itmon->pdata = pdata;
	itmon->pdata->masterinfo = masterinfo;
	itmon->pdata->rpathinfo = rpathinfo;
	itmon->pdata->nodegroup = nodegroup;

	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		dev_name = nodegroup[i].name;
		node = nodegroup[i].nodeinfo;

		if (nodegroup[i].phy_regs) {
			nodegroup[i].regs = devm_ioremap_nocache(&pdev->dev,
							 nodegroup[i].phy_regs, SZ_16K);
			if (nodegroup[i].regs == NULL) {
				dev_err(&pdev->dev, "failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
		}

		if (initial_multi_irq_enable)
			irq_option = IRQF_GIC_MULTI_TARGET;

		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		nodegroup[i].irq = irq;

		ret = devm_request_irq(&pdev->dev, irq,
				       itmon_irq_handler, irq_option, dev_name, itmon);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request irq - %s\n", dev_name);
			return -ENOENT;
		} else {
			dev_err(&pdev->dev, "success to register request irq%u - %s\n", irq, dev_name);
		}

		for (j = 0; j < nodegroup[i].nodesize; j++) {
			node[j].regs = devm_ioremap_nocache(&pdev->dev, node[j].phy_regs, SZ_16K);
			if (node[j].regs == NULL) {
				dev_err(&pdev->dev, "failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
		}
	}

	itmon_panic = devm_kzalloc(&pdev->dev, sizeof(struct itmon_panic_block),
				 GFP_KERNEL);

	if (!itmon_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				    "panic handler data\n");
	} else {
		itmon_panic->nb_panic_block.notifier_call = itmon_logging_panic_handler;
		itmon_panic->pdev = itmon;
		atomic_notifier_chain_register(&panic_notifier_list,
					       &itmon_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, itmon);

	INIT_LIST_HEAD(&pdata->tracelist[BUS_DATA]);
	INIT_LIST_HEAD(&pdata->tracelist[BUS_PERI]);

	pdata->crash_in_progress = false;
	itmon_init(itmon, true);

	g_itmon = itmon;
	pdata->probed = true;

	dev_info(&pdev->dev, "success to probe Exynos ITMON driver\n");

	return 0;
}

static int itmon_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int itmon_suspend(struct device *dev)
{
	return 0;
}

static int itmon_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;

	/* re-enable ITMON if cp-crash progress is not starting */
	if (!pdata->crash_in_progress)
		itmon_init(itmon, true);

	return 0;
}

static SIMPLE_DEV_PM_OPS(itmon_pm_ops, itmon_suspend, itmon_resume);
#define ITMON_PM	(itmon_pm_ops)
#else
#define ITM_ONPM	NULL
#endif

static struct platform_driver exynos_itmon_driver = {
	.probe = itmon_probe,
	.remove = itmon_remove,
	.driver = {
		   .name = "exynos-itmon",
		   .of_match_table = itmon_dt_match,
		   .pm = &itmon_pm_ops,
		   },
};

module_platform_driver(exynos_itmon_driver);

MODULE_DESCRIPTION("Samsung Exynos ITMON DRIVER");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-itmon");
