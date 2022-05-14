/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung EXYNOS SoC MIPI-DSI Master driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DSIM_H__
#define __SAMSUNG_DSIM_H__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>

#include "./panels/decon_lcd.h"
#if defined(CONFIG_SOC_EXYNOS9610)
#include "./cal_9610/regs-dsim.h"
#include "./cal_9610/dsim_cal.h"
#endif

#if defined(CONFIG_EXYNOS_DECON_LCD_S6E3HA2K)
#include "./panels/s6e3ha2k_param.h"
#elif defined(CONFIG_EXYNOS_DECON_LCD_S6E3HF4)
#include "./panels/s6e3hf4_param.h"
#elif defined(CONFIG_EXYNOS_DECON_LCD_EMUL_DISP)
#include "./panels/emul_disp_param.h"
#elif defined(CONFIG_EXYNOS_DECON_LCD_S6E3HA6)
#include "./panels/s6e3ha6_param.h"
#elif defined(CONFIG_EXYNOS_DECON_LCD_S6E3AA2)
#include "./panels/s6e3aa2_param.h"
#elif defined(CONFIG_EXYNOS_DECON_LCD_S6E3FA0)
#include "./panels/s6e3fa0_param.h"
#endif

extern int dsim_log_level;

#define DSIM_MODULE_NAME			"exynos-dsim"
#define MAX_DSIM_CNT				2
#define DSIM_DDI_ID_LEN				3

#define DSIM_PIXEL_FORMAT_RGB24			0x3E
#define DSIM_PIXEL_FORMAT_RGB18_PACKED		0x1E
#define DSIM_PIXEL_FORMAT_RGB18			0x2E
#define DSIM_PIXEL_FORMAT_RGB30_PACKED		0x0D
#define DSIM_RX_FIFO_MAX_DEPTH			64
#define MAX_DSIM_DATALANE_CNT			4

#define MIPI_WR_TIMEOUT				msecs_to_jiffies(50)
#define MIPI_RD_TIMEOUT				msecs_to_jiffies(100)

#define dsim_err(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 3) {					\
			pr_err(pr_fmt("dsim: "fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dsim_warn(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 4) {					\
			pr_warn(pr_fmt("dsim: "fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dsim_info(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 6)					\
			pr_info(pr_fmt("dsim: "fmt), ##__VA_ARGS__);			\
	} while (0)

#define dsim_dbg(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 7)					\
			pr_info(pr_fmt("dsim: "fmt), ##__VA_ARGS__);			\
	} while (0)

#define call_panel_ops(q, op, args...)				\
	(((q) && ((q)->panel_ops->op)) ? ((q)->panel_ops->op(args)) : 0)

extern struct dsim_device *dsim_drvdata[MAX_DSIM_CNT];
extern struct dsim_lcd_driver s6e3ha2k_mipi_lcd_driver;
extern struct dsim_lcd_driver emul_disp_mipi_lcd_driver;
extern struct dsim_lcd_driver s6e3hf4_mipi_lcd_driver;
extern struct dsim_lcd_driver s6e3ha6_mipi_lcd_driver;
extern struct dsim_lcd_driver s6e3ha8_mipi_lcd_driver;
extern struct dsim_lcd_driver s6e3aa2_mipi_lcd_driver;
extern struct dsim_lcd_driver s6e3fa0_mipi_lcd_driver;
extern struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver;
extern struct dsim_lcd_driver ea8076_mipi_lcd_driver;

/* define video timer interrupt */
enum {
	DSIM_VBP = 0,
	DSIM_VSYNC,
	DSIM_V_ACTIVE,
	DSIM_VFP,
};

/* define dsi bist pattern */
enum {
	DSIM_COLOR_BAR = 0,
	DSIM_GRAY_GRADATION,
	DSIM_USER_DEFINED,
	DSIM_PRB7_RANDOM,
};

/* define DSI lane types. */
enum {
	DSIM_LANE_CLOCK	= (1 << 0),
	DSIM_LANE_DATA0	= (1 << 1),
	DSIM_LANE_DATA1	= (1 << 2),
	DSIM_LANE_DATA2	= (1 << 3),
	DSIM_LANE_DATA3	= (1 << 4),
};

/* DSI Error report bit definitions */
enum {
	MIPI_DSI_ERR_SOT			= (1 << 0),
	MIPI_DSI_ERR_SOT_SYNC			= (1 << 1),
	MIPI_DSI_ERR_EOT_SYNC			= (1 << 2),
	MIPI_DSI_ERR_ESCAPE_MODE_ENTRY_CMD	= (1 << 3),
	MIPI_DSI_ERR_LOW_POWER_TRANSMIT_SYNC	= (1 << 4),
	MIPI_DSI_ERR_HS_RECEIVE_TIMEOUT		= (1 << 5),
	MIPI_DSI_ERR_FALSE_CONTROL		= (1 << 6),
	/* Bit 7 is reserved */
	MIPI_DSI_ERR_ECC_SINGLE_BIT		= (1 << 8),
	MIPI_DSI_ERR_ECC_MULTI_BIT		= (1 << 9),
	MIPI_DSI_ERR_CHECKSUM			= (1 << 10),
	MIPI_DSI_ERR_DATA_TYPE_NOT_RECOGNIZED	= (1 << 11),
	MIPI_DSI_ERR_VCHANNEL_ID_INVALID	= (1 << 12),
	MIPI_DSI_ERR_INVALID_TRANSMIT_LENGTH	= (1 << 13),
	/* Bit 14 is reserved */
	MIPI_DSI_ERR_PROTOCAL_VIOLATION		= (1 << 15),
	/* DSI_PROTOCAL_VIOLATION[15] is for protocol violation that is caused EoTp
	 * missing So this bit is egnored because of not supportung @S.LSI AP */
	/* FALSE_ERROR_CONTROL[6] is for detect invalid escape or turnaround sequence.
	 * This bit is not supporting @S.LSI AP because of non standard
	 * ULPS enter/exit sequence during power-gating */
	/* Bit [14],[7] is reserved */
	MIPI_DSI_ERR_BIT_MASK			= (0x3f3f), /* Error_Range[13:0] */
};

/* operation state of dsim driver */
enum dsim_state {
	DSIM_STATE_INIT,
	DSIM_STATE_ON,			/* HS clock was enabled. */
	DSIM_STATE_DOZE,		/* HS clock was enabled. */
	DSIM_STATE_ULPS,		/* DSIM was entered ULPS state */
	DSIM_STATE_DOZE_SUSPEND,	/* DSIM is suspend state */
	DSIM_STATE_OFF			/* DSIM is suspend state */
};

enum dphy_charic_value {
	M_PLL_CTRL1,
	M_PLL_CTRL2,
	B_DPHY_CTRL2,
	B_DPHY_CTRL3,
	B_DPHY_CTRL4,
	M_DPHY_CTRL1,
	M_DPHY_CTRL2,
	M_DPHY_CTRL3,
	M_DPHY_CTRL4
};

struct dsim_pll_param {
	u32 p;
	u32 m;
	u32 s;
	u32 k;
	u32 pll_freq; /* in/out parameter: Mhz */
};

struct dphy_timing_value {
	u32 bps;
	u32 clk_prepare;
	u32 clk_zero;
	u32 clk_post;
	u32 clk_trail;
	u32 hs_prepare;
	u32 hs_zero;
	u32 hs_trail;
	u32 lpx;
	u32 hs_exit;
	u32 b_dphyctl;
};

struct dsim_resources {
	struct clk *pclk;
	struct clk *dphy_esc;
	struct clk *dphy_byte;
	struct clk *rgb_vclk0;
	struct clk *pclk_disp;
	struct clk *aclk;
	int lcd_power[3];
	int lcd_reset;
	int irq;
	void __iomem *regs;
	void __iomem *ss_regs;
	void __iomem *phy_regs;
	void __iomem *phy_regs_ex;
	struct regulator *regulator_1p8v;
	struct regulator *regulator_3p3v;
};

struct panel_private {
	unsigned int lcdconnected;
	void *par;
};

struct dsim_device {
	int id;
	enum dsim_state state;
	struct device *dev;
	struct dsim_resources res;

	unsigned int data_lane;
	u32 data_lane_cnt;
	struct phy *phy;
	struct phy *phy_ex;
	spinlock_t slock;

	struct dsim_lcd_driver *panel_ops;
	struct decon_lcd lcd_info;

	struct panel_private priv;

	struct v4l2_subdev sd;
	struct dsim_clks clks;
	struct timer_list cmd_timer;

	struct mutex cmd_lock;

	struct completion ph_wr_comp;
	struct completion rd_comp;

	int total_underrun_cnt;
	struct backlight_device *bd;
	int idle_ip_index;

	/* true  - fb reserved     */
	/* false - fb not reserved */
	bool fb_reservation;
	phys_addr_t phys_addr;
	phys_addr_t phys_size;
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
//#define READ_ESD_SOLUTION_TEST
	int esd_test;
	bool esd_recovering;
#endif
	int continuous_irq_count;
};

struct dsim_lcd_driver {
	char *name;
	int (*probe)(struct dsim_device *dsim);
	int (*suspend)(struct dsim_device *dsim);
	int (*displayon)(struct dsim_device *dsim);
	int (*resume)(struct dsim_device *dsim);
	int (*resume_early)(struct dsim_device *dsim);
	int (*dump)(struct dsim_device *dsim);
	int (*mres)(struct dsim_device *dsim, int mres_idx);
	int (*doze)(struct dsim_device *dsim);
	int (*doze_suspend)(struct dsim_device *dsim);
	int (*match)(void *maybe_unused);
#if defined(CONFIG_LOGGING_BIGDATA_BUG)
	unsigned int (*get_buginfo)(struct dsim_device *dsim);
#endif
#if defined(CONFIG_SUPPORT_MASK_LAYER)
	int (*mask_brightness)(struct dsim_device *dsim);
#endif
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	int (*read_state)(struct dsim_device *dsim);
#endif
};

int dsim_write_data(struct dsim_device *dsim, u32 id, unsigned long d0, u32 d1);
int dsim_read_data(struct dsim_device *dsim, u32 id, u32 addr, u32 cnt, u8 *buf);
int dsim_wait_for_cmd_done(struct dsim_device *dsim);

int dsim_reset_panel(struct dsim_device *dsim);
int dsim_set_panel_power(struct dsim_device *dsim, bool on);
int dsim_set_panel_power_early(struct dsim_device *dsim);

void dsim_to_regs_param(struct dsim_device *dsim, struct dsim_regs *regs);

static inline struct dsim_device *get_dsim_drvdata(u32 id)
{
	return dsim_drvdata[id];
}

static inline int dsim_rd_data(u32 id, u32 cmd_id, u32 addr, u32 size, u8 *buf)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_read_data(dsim, cmd_id, addr, size, buf);
	if (ret)
		return ret;

	return 0;
}

static inline int dsim_wr_data(u32 id, u32 cmd_id, unsigned long d0, u32 d1)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_write_data(dsim, cmd_id, d0, d1);
	if (ret)
		return ret;

	return 0;
}

static inline int dsim_wait_for_cmd_completion(u32 id)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_wait_for_cmd_done(dsim);

	return ret;
}

/* register access subroutines */
static inline u32 dsim_read(u32 id, u32 reg_id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	return readl(dsim->res.regs + reg_id);
}

static inline u32 dsim_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dsim_read(id, reg_id);
	val &= (mask);
	return val;
}

static inline void dsim_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	writel(val, dsim->res.regs + reg_id);
}

static inline void dsim_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	u32 old = dsim_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->res.regs + reg_id);
}

/* DPHY register access subroutines */
static inline u32 dsim_phy_read(u32 id, u32 reg_id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	return readl(dsim->res.phy_regs + reg_id);
}

static inline u32 dsim_phy_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dsim_phy_read(id, reg_id);

	val &= (mask);
	return val;
}
static inline void dsim_phy_extra_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	writel(val, dsim->res.phy_regs_ex + reg_id);
}
static inline void dsim_phy_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	writel(val, dsim->res.phy_regs + reg_id);
}

static inline void dsim_phy_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	u32 old = dsim_phy_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->res.phy_regs + reg_id);
	/* printk("offset : 0x%8x, value : 0x%x\n", reg_id, val); */
}

/* DPHY loop back for test */
#ifdef DPHY_LOOP
void dsim_reg_set_dphy_loop_back_test(u32 id);
#endif

static inline bool IS_DSIM_ON_STATE(struct dsim_device *dsim)
{
#ifdef CONFIG_EXYNOS_DOZE
	return (dsim->state == DSIM_STATE_ON ||
			dsim->state == DSIM_STATE_DOZE);
#else
	return (dsim->state == DSIM_STATE_ON);
#endif
}

static inline bool IS_DSIM_OFF_STATE(struct dsim_device *dsim)
{
	return (dsim->state == DSIM_STATE_ULPS ||
#ifdef CONFIG_EXYNOS_DOZE
			dsim->state == DSIM_STATE_DOZE_SUSPEND ||
#endif
			dsim->state == DSIM_STATE_OFF);
}

#define DSIM_IOC_ENTER_ULPS		_IOW('D', 0, u32)
#define DSIM_IOC_GET_LCD_INFO		_IOW('D', 5, struct decon_lcd *)
#define DSIM_IOC_DUMP			_IOW('D', 8, u32)
#define DSIM_IOC_GET_WCLK		_IOW('D', 9, u32)
#define DSIM_IOC_SET_CONFIG		_IOW('D', 10, u32)
#define DSIM_IOC_FREE_FB_RES		_IOW('D', 11, u32)
#define DSIM_IOC_DOZE			_IOW('D', 20, u32)
#define DSIM_IOC_DOZE_SUSPEND		_IOW('D', 21, u32)

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
#define DSIM_ESD_OK			0
#define DSIM_ESD_ERROR			1
#define DSIM_ESD_CHECK_ERROR		2
#endif

#endif /* __SAMSUNG_DSIM_H__ */
