/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <video/mipi_display.h>

#include "../decon.h"
#include "../decon_notify.h"
#include "../dsim.h"
#include "dsim_panel.h"

#include "s6e3fa7_vogue_param.h"

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
#include "mdnie.h"
#include "s6e3fa7_vogue_mdnie.h"
#endif

#if defined(CONFIG_DISPLAY_USE_INFO)
#include "dpui.h"

#define	DPUI_VENDOR_NAME	"SDC"
#define DPUI_MODEL_NAME		"AMS604NL01"
#endif

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED	1

#define DSIM_FIFO_SIZE		512

#define DSI_WRITE(cmd, size)		do {				\
	if (size >= DSIM_FIFO_SIZE)	\
		ret = mipi_write_side_ram(lcd, cmd, size);	\
	else	\
		ret = dsim_write_hl_data(lcd, cmd, size);			\
	if (ret < 0)							\
		dev_info(&lcd->ld->dev, "%s: failed to write %s\n", __func__, #cmd);	\
} while (0)

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

union aor_info {
	u32 value;
	struct {
		u8 aor_1;
		u8 aor_2;
		u16 reserved;
	};
};

union elvss_info {
	u32 value;
	struct {
		u8 tset;
		u8 mpscon;
		u8 dim_offset;
		u8 cal_offset;
	};
};

union acl_info {
	u32 value;
	struct {
		u8 enable;
		u8 frame_avg_hbm;
		u8 percent;
		u8 frame_avg;
	};
};

struct hbm_interpolation_t {
	int		*hbm;
	const int	*gamma_default;

	const int	*ibr_tbl;
	int		idx_ref;
	int		idx_hbm;
};

union lpm_info {
	u32 value;
	struct {
		u8 state;
		u8 mode;	/* comes from sysfs. 0(off), 1(alpm 2nit), 2(hlpm 2nit), 3(alpm 60nit), 4(hlpm 60nit) or 1(alpm), 2(hlpm) */
		u8 ver;		/* comes from sysfs. 0(old), 1(new) */
		u16 reserved;
	};
};

struct lcd_info {
	unsigned int			connected;
	unsigned int			bl;
	unsigned int			brightness;
	unsigned int			current_bl;
	union elvss_info		current_elvss;
	union aor_info			current_aor;
	union acl_info			current_acl;
	unsigned int			current_vint;
	unsigned int			state;

	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct device			svc_dev;
	struct dynamic_aid_param_t	daid;

	unsigned char			elvss_table[IBRIGHTNESS_HBM_MAX][TEMP_MAX][ELVSS_CMD_CNT];
	unsigned char			gamma_table[IBRIGHTNESS_HBM_MAX][GAMMA_CMD_CNT];

	unsigned char			(*aor_table)[AID_CMD_CNT];
	unsigned char			(*irc_table)[IRC_CMD_CNT];
	unsigned char			**acl_table;
	unsigned char			**opr_table;
	unsigned char			(*vint_table)[VINT_CMD_CNT];

	int				temperature;
	unsigned int			temperature_index;

	union {
		struct {
			u8		reserved;
			u8		id[LDI_LEN_ID];
		};
		u32			value;
	} id_info;
	unsigned char			mtp[LDI_LEN_MTP];
	unsigned char			hbm[LDI_LEN_HBM];
	unsigned char			code[LDI_LEN_CHIP_ID];
	unsigned char			date[LDI_LEN_DATE];
	unsigned int			coordinate[2];
	unsigned char			coordinates[20];
	unsigned char			manufacture_info[LDI_LEN_MANUFACTURE_INFO];
	unsigned char			rddpm;
	unsigned char			rddsm;

	unsigned int			adaptive_control;
	int				lux;
	struct class			*mdnie_class;

	struct dsim_device		*dsim;
	struct mutex			lock;

	struct hbm_interpolation_t	hitp;

	struct notifier_block		fb_notifier;

#if defined(CONFIG_DISPLAY_USE_INFO)
	struct notifier_block		dpui_notif;
#endif

#if defined(CONFIG_EXYNOS_DOZE)
	union lpm_info			alpm;
	union lpm_info			current_alpm;

#if defined(CONFIG_SEC_FACTORY)
	unsigned int			prev_brightness;
	union lpm_info			prev_alpm;
#endif
#endif

#if defined(CONFIG_LCD_HMT)
	struct dynamic_aid_param_t	hmt_daid;
	unsigned int			hmt_on;
	unsigned int			hmt_brightness;
	unsigned int			hmt_bl;
	unsigned int			hmt_current_bl;
	unsigned char			hmt_gamma_table[IBRIGHTNESS_HMT_MAX][GAMMA_CMD_CNT];
#endif
	unsigned char			poc_eb[LDI_LEN_POC_EB];
	unsigned char			poc_ec[LDI_LEN_POC_EC];
};

#if defined(CONFIG_LCD_HMT)
static int s6e3fa7_hmt_update(struct lcd_info *lcd, u8 forced);
static int hmt_init(struct lcd_info *lcd);
static void show_hmt_aid_log(struct lcd_info *lcd);
#endif

static int dsim_write_hl_data(struct lcd_info *lcd, const u8 *cmd, u32 cmdsize)
{
	int ret = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_write:
	if (cmdsize == 1)
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_SHORT_WRITE, cmd[0], 0);
	else if (cmdsize == 2)
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd[0], cmd[1]);
	else
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd, cmdsize);

	if (ret < 0) {
		if (--retry)
			goto try_write;
		else
			dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, cmd[0], ret);
	}

	return ret;
}

static int mipi_write_side_ram(struct lcd_info *lcd, const u8 *cmd, int size)
{
	int ret;
	u8 cmd_buf[DSIM_FIFO_SIZE];
	u32 t_tx_size, tx_size, remind_size;
	u32 fifo_size, tmp;

	dev_info(&lcd->ld->dev, "%s: %d\n", __func__, size);

	fifo_size = DSIM_FIFO_SIZE;

	/* for SIDE_RAM_ALIGN_CNT byte align */
	tmp = (fifo_size - 1) / SIDE_RAM_ALIGN_CNT;
	fifo_size = (tmp * SIDE_RAM_ALIGN_CNT) + 1;

	dev_info(&lcd->ld->dev, "%s: fifo_size: %d\n", __func__, fifo_size);

	t_tx_size = 0;
	remind_size = size;

	while (remind_size) {
		if (remind_size == size)
			cmd_buf[0] = MIPI_DSI_OEM1_WR_SIDE_RAM;
		else
			cmd_buf[0] = MIPI_DSI_OEM1_WR_SIDE_RAM2;

		tx_size = min(remind_size, fifo_size - 1);

		memcpy((u8 *)cmd_buf + 1, (u8 *)cmd + t_tx_size, tx_size);

		ret = dsim_write_hl_data(lcd, cmd_buf, tx_size + 1);
		if (ret < 0) {
			dev_info(&lcd->ld->dev, "%s: failed to write command: %d\n", __func__, ret);
			goto err_write_side_ram;
		}

		t_tx_size += tx_size;
		remind_size -= tx_size;
	}

err_write_side_ram:
	return ret;
}

static int dsim_read_hl_data(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf)
{
	int ret = 0, rx_size = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_read:
	rx_size = dsim_read_data(lcd->dsim, MIPI_DSI_DCS_READ, (u32)addr, size, buf);
	dev_info(&lcd->ld->dev, "%s: %2d(%2d), %02x, %*ph%s\n", __func__, size, rx_size, addr,
		min_t(u32, min_t(u32, size, rx_size), 5), buf, (rx_size > 5) ? "..." : "");
	if (rx_size != size) {
		if (--retry)
			goto try_read;
		else {
			dev_info(&lcd->ld->dev, "%s: fail. %02x, %d(%d)\n", __func__, addr, size, rx_size);
			ret = -EPERM;
		}
	}

	return ret;
}

static int dsim_read_info(struct lcd_info *lcd, u8 reg, u32 len, u8 *buf)
{
	int ret = 0, i;

	ret = dsim_read_hl_data(lcd, reg, len, buf);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, reg, ret);
		goto exit;
	}

	smtd_dbg("%s: %02xh\n", __func__, reg);
	for (i = 0; i < len; i++)
		smtd_dbg("%02dth value is %02x, %3d\n", i + 1, buf[i], buf[i]);

exit:
	return ret;
}

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
static int dsim_write_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = dsim_write_hl_data(lcd, seq[i].cmd, seq[i].len);
			if (ret < 0) {
				dev_info(&lcd->ld->dev, "%s: %dth fail\n", __func__, i);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000, seq[i].sleep * 1100);
	}
	return ret;
}
#endif

static int dsim_panel_gamma_ctrl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	if (force)
		goto update;
	else if (lcd->current_bl != lcd->bl)
		goto update;
	else
		goto exit;

update:
	DSI_WRITE(lcd->gamma_table[lcd->bl], GAMMA_CMD_CNT);

exit:
	return ret;
}

static int dsim_panel_aid_ctrl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	DSI_WRITE(lcd->aor_table[lcd->brightness], AID_CMD_CNT);

	return ret;
}

static int dsim_panel_set_elvss(struct lcd_info *lcd, u8 force)
{
	u8 *elvss = NULL;
	int ret = 0;
	union elvss_info elvss_value;
	unsigned char tset;

	elvss = lcd->elvss_table[lcd->bl][lcd->temperature_index];

	tset = ((lcd->temperature < 0) ? BIT(7) : 0) | abs(lcd->temperature);
	elvss_value.tset = elvss[LDI_OFFSET_ELVSS_1] = tset;
	elvss_value.mpscon = elvss[LDI_OFFSET_ELVSS_2];
	elvss_value.dim_offset = elvss[LDI_OFFSET_ELVSS_3];
	elvss_value.cal_offset = elvss[LDI_OFFSET_ELVSS_4];

	if (force)
		goto update;
	else if (lcd->current_elvss.value != elvss_value.value)
		goto update;
	else
		goto exit;

update:
	DSI_WRITE(elvss, ELVSS_CMD_CNT);
	lcd->current_elvss.value = elvss_value.value;
	dev_info(&lcd->ld->dev, "elvss: %x\n", lcd->current_elvss.value);

exit:
	return ret;
}

static int dsim_panel_set_acl(struct lcd_info *lcd, int force)
{
	int ret = 0, opr_status = OPR_STATUS_15P, acl_status = ACL_STATUS_ON;
	union acl_info acl_value;

	opr_status = brightness_opr_table[!!lcd->adaptive_control][lcd->brightness];
	acl_status = !!opr_status;

	acl_value.enable = lcd->acl_table[acl_status][LDI_OFFSET_ACL];
	acl_value.frame_avg_hbm = lcd->opr_table[opr_status][LDI_OFFSET_OPR_1];
	acl_value.percent = lcd->opr_table[opr_status][LDI_OFFSET_OPR_2];
	acl_value.frame_avg = lcd->opr_table[opr_status][LDI_OFFSET_OPR_3];

	if (force)
		goto update;
	else if (lcd->current_acl.value != acl_value.value)
		goto update;
	else
		goto exit;

update:
	DSI_WRITE(lcd->opr_table[opr_status], OPR_CMD_CNT);
	DSI_WRITE(lcd->acl_table[acl_status], ACL_CMD_CNT);
	lcd->current_acl.value = acl_value.value;
	dev_info(&lcd->ld->dev, "acl: %x, brightness: %d, adaptive_control: %d\n", lcd->current_acl.value, lcd->brightness, lcd->adaptive_control);

exit:
	return ret;
}

static int dsim_panel_irc_ctrl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	DSI_WRITE(lcd->irc_table[lcd->brightness], IRC_CMD_CNT);

	return ret;
}

static int dsim_panel_vint_ctrl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	unsigned char vint = lcd->vint_table[lcd->bl][LDI_OFFSET_VINT];

	if (force)
		goto update;
	else if (lcd->current_vint != vint)
		goto update;
	else
		goto exit;

update:
	DSI_WRITE(lcd->vint_table[lcd->bl], VINT_CMD_CNT);
	lcd->current_vint = vint;
	dev_info(&lcd->ld->dev, "vint: %x\n", lcd->current_vint);

exit:
	return ret;
}

static int low_level_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	dsim_panel_gamma_ctrl(lcd, force);

	dsim_panel_aid_ctrl(lcd, force);

	dsim_panel_set_elvss(lcd, force);

	dsim_panel_set_acl(lcd, force);

	dsim_panel_irc_ctrl(lcd, force);

	dsim_panel_vint_ctrl(lcd, force);

	DSI_WRITE(SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	return 0;
}

static int get_backlight_level_from_brightness(int brightness)
{
	return brightness_table[brightness];
}

static int dsim_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;

	mutex_lock(&lcd->lock);

#if defined(CONFIG_EXYNOS_DOZE)
	if (lcd->current_alpm.state) {
		dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
			lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);
		goto exit;
	}
#endif

#if defined(CONFIG_LCD_HMT)
	if (lcd->hmt_on) {
		dev_info(&lcd->ld->dev, "%s: brightness: %d, hmt_state: %d\n", __func__, lcd->bd->props.brightness, lcd->hmt_on);
		goto exit;
	}
#endif
	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(lcd->brightness);

	if (!force && lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: brightness: %d, panel_state: %d\n", __func__, lcd->brightness, lcd->state);
		goto exit;
	}

	ret = low_level_set_brightness(lcd, force);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to set brightness : %d\n", __func__, index_brightness_table[lcd->bl]);

	lcd->current_bl = lcd->bl;

	dev_info(&lcd->ld->dev, "brightness: %d, bl: %d, nit: %d, lx: %d\n", lcd->brightness, lcd->bl, index_brightness_table[lcd->bl], lcd->lux);
exit:
	mutex_unlock(&lcd->lock);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return index_brightness_table[lcd->bl];
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct lcd_info *lcd = bl_get_data(bd);

	if (lcd->state == PANEL_STATE_RESUMED) {
		ret = dsim_panel_set_brightness(lcd, 0);
		if (ret < 0)
			dev_info(&lcd->ld->dev, "%s: failed to set brightness\n", __func__);
	}

	return ret;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};

static void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = lcd->daid.mtp ? lcd->daid.mtp : kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;

	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;
	lcd->daid.iv_ref = index_voltage_reference;
	lcd->daid.m_gray = m_gray;
}

/* V255(msb is separated) ~ VT -> VT ~ V255(msb is not separated) and signed bit */
static void reorder_reg2mtp(u8 *reg, int *mtp)
{
	int j, c, v;

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (reg[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = reg[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = reg[j];
	}

	for (v = IV_MAX - 2; v >= 0; v--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (reg[j] & 0x80)
				mtp[CI_MAX*v+c] = (reg[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*v+c] = reg[j];
		}
	}
}

/* V255(msb is separated) ~ VT -> VT ~ V255(msb is not separated) */
static void reorder_reg2gamma(u8 *reg, int *gamma)
{
	int j, c, v;

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (reg[j++] & 0x01)
			gamma[(IV_MAX-1)*CI_MAX+c] = reg[j] | BIT(8);
		else
			gamma[(IV_MAX-1)*CI_MAX+c] = reg[j];
	}

	for (v = IV_MAX - 2; v >= 0; v--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (reg[j] & 0x80)
				gamma[CI_MAX*v+c] = (reg[j] & 0x7F) | BIT(7);
			else
				gamma[CI_MAX*v+c] = reg[j];
		}
	}
}

/* VT ~ V255(msb is not separated) -> V255(msb is separated) ~ VT */
/* array idx zero (reg[0]) is reserved for gamma command address (0xCA) */
static void reorder_gamma2reg(int *gamma, u8 *reg)
{
	int j, c, v;
	int *pgamma;

	/* V255_R_1: 1st para D[2] */
	/* V255_R_2: 3th para */
	/* V255_G_1: 1st para D[1] */
	/* V255_G_2: 4rd para */
	/* V255_B_1: 1st para D[0] */
	/* V255_B_2: 6th para */
	v = IV_MAX - 1;
	pgamma = &gamma[v * CI_MAX];
	reg[1] = get_bit(pgamma[CI_RED], 8, 1) << 2 | get_bit(pgamma[CI_GREEN], 8, 1) << 1 | get_bit(pgamma[CI_BLUE], 8, 1) << 0;
	reg[3] = pgamma[CI_RED] & 0xff;
	reg[4] = pgamma[CI_GREEN] & 0xff;
	reg[5] = 0;
	reg[6] = pgamma[CI_BLUE] & 0xff;

	for (v = IV_MAX - 2, j = 7; v > IV_VT; v--) {
		pgamma = &gamma[v * CI_MAX];
		for (c = 0; c < CI_MAX; c++, pgamma++)
			reg[j++] = *pgamma;
	}

	/* VT_R: 1st para D[7:4] */
	/* VT_G: 2nd para D[7:4] */
	/* VT_B: 2nd para D[3:0] */
	v = IV_VT;
	pgamma = &gamma[v * CI_MAX];
	reg[1] = (pgamma[CI_RED] & 0xf) << 4 | (reg[1] & 0x7);
	reg[2] = (pgamma[CI_GREEN] & 0xf) << 4 | (pgamma[CI_BLUE] & 0xf);
}

static void init_mtp_data(struct lcd_info *lcd, u8 *data)
{
	int i, c;
	int *mtp = lcd->daid.mtp;
	u8 tmp[IV_MAX * CI_MAX + CI_MAX] = {0, };
	u8 v255[CI_MAX][2] = {{0,}, };

	memcpy(&tmp[6], &data[5], (IV_203 - IV_1 + 1) * CI_MAX);	/* V203 ~ V1 */

	/* V255_R_1: C8h 1st para D[2] */
	/* V255_R_2: C8h 3th para */
	/* V255_G_1: C8h 1st para D[1] */
	/* V255_G_2: C8h 4rd para */
	/* V255_B_1: C8h 1st para D[0] */
	/* V255_B_2: C8h 5th para */
	v255[CI_RED][0] = get_bit(data[0], 2, 1);
	v255[CI_RED][1] = data[2];

	v255[CI_GREEN][0] = get_bit(data[0], 1, 1);
	v255[CI_GREEN][1] = data[3];

	v255[CI_BLUE][0] = get_bit(data[0], 0, 1);
	v255[CI_BLUE][1] = data[4];

	tmp[0] = v255[CI_RED][0];
	tmp[1] = v255[CI_RED][1];
	tmp[2] = v255[CI_GREEN][0];
	tmp[3] = v255[CI_GREEN][1];
	tmp[4] = v255[CI_BLUE][0];
	tmp[5] = v255[CI_BLUE][1];

	/* VT_R: C8h 1st para D[7:4] */
	/* VT_G: C8h 2nd para D[7:4] */
	/* VT_B: C8h 2nd para D[3:0] */
	tmp[33] = get_bit(data[0], 4, 4);
	tmp[34] = get_bit(data[1], 4, 4);
	tmp[35] = get_bit(data[1], 0, 4);

	reorder_reg2mtp(tmp, mtp);

	smtd_dbg("MTP_Offset_Value\n");
	for (i = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++)
			smtd_dbg("%4d ", mtp[i*CI_MAX+c]);
		smtd_dbg("\n");
	}
}

static int init_gamma(struct lcd_info *lcd)
{
	int i, j;
	int ret = 0;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kcalloc(IBRIGHTNESS_MAX, sizeof(int *), GFP_KERNEL);
	if (!gamma) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kcalloc(IV_MAX*CI_MAX, sizeof(int), GFP_KERNEL);
		if (!gamma[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* pre-allocate memory for gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		memcpy(&lcd->gamma_table[i], SEQ_GAMMA_CONDITION_SET, GAMMA_CMD_CNT);

	/* calculate gamma table */
	init_mtp_data(lcd, lcd->mtp);
	dynamic_aid(lcd->daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		reorder_gamma2reg(gamma[i], lcd->gamma_table[i]);

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("Gamma [%3d] = ", lcd->daid.ibr_tbl[i]);
		for (j = 0; j < GAMMA_CMD_CNT; j++)
			smtd_dbg("%4d ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}

static int s6e3fa7_read_id(struct lcd_info *lcd)
{
	struct panel_private *priv = &lcd->dsim->priv;
	int ret = 0;
	struct decon_device *decon = get_decon_drvdata(0);
	static char *LDI_BIT_DESC_ID[BITS_PER_BYTE * LDI_LEN_ID] = {
		[0 ... 23] = "ID Read Fail",
	};

	lcd->id_info.value = 0;
	priv->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	ret = dsim_read_info(lcd, LDI_REG_ID, LDI_LEN_ID, lcd->id_info.id);
	if (ret < 0 || !lcd->id_info.value) {
		priv->lcdconnected = lcd->connected = 0;
		dev_info(&lcd->ld->dev, "%s: connected lcd is invalid\n", __func__);

		if (lcdtype && decon)
			decon_abd_save_bit(&decon->abd, BITS_PER_BYTE * LDI_LEN_ID, cpu_to_be32(lcd->id_info.value), LDI_BIT_DESC_ID);
	}

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	return ret;
}

static int s6e3fa7_read_mtp(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_GPARA_DATE + LDI_LEN_DATE] = {0, };

	ret = dsim_read_info(lcd, LDI_REG_MTP, ARRAY_SIZE(buf), buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->mtp, buf, LDI_LEN_MTP);
	memcpy(lcd->date, &buf[LDI_GPARA_DATE], LDI_LEN_DATE);

	return ret;
}

static int s6e3fa7_read_coordinate(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_COORDINATE] = {0, };

	ret = dsim_read_info(lcd, LDI_REG_COORDINATE, LDI_LEN_COORDINATE, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */

	scnprintf(lcd->coordinates, sizeof(lcd->coordinates), "%d %d\n", lcd->coordinate[0], lcd->coordinate[1]);

	return ret;
}

static int s6e3fa7_read_manufacture_info(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_GPARA_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO] = {0, };

	ret = dsim_read_info(lcd, LDI_REG_MANUFACTURE_INFO, ARRAY_SIZE(buf), buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->manufacture_info, &buf[LDI_GPARA_MANUFACTURE_INFO], LDI_LEN_MANUFACTURE_INFO);

	return ret;
}

static int s6e3fa7_read_chip_id(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_CHIP_ID] = {0, };

	ret = dsim_read_info(lcd, LDI_REG_CHIP_ID, LDI_LEN_CHIP_ID, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->code, buf, LDI_LEN_CHIP_ID);

	return ret;
}

static int s6e3fa7_read_elvss(struct lcd_info *lcd, unsigned char *buf)
{
	int ret = 0;

	ret = dsim_read_info(lcd, LDI_REG_ELVSS, LDI_LEN_ELVSS, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int s6e3fa7_read_irc(struct lcd_info *lcd, unsigned char *buf)
{
	int ret = 0;

	ret = dsim_read_info(lcd, LDI_REG_IRC, LDI_LEN_IRC, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int s6e3fa7_read_hbm(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_HBM] = {0, };

	ret = dsim_read_info(lcd, LDI_REG_HBM, LDI_LEN_HBM, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->hbm, buf, LDI_LEN_HBM);

	return ret;
}

static int s6e3fa7_read_poc_info(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char eb_buf[LDI_LEN_POC_EB] = {0, };
	unsigned char ec_buf[LDI_LEN_POC_EC] = {0, };

	ret = dsim_read_info(lcd, LDI_REG_POC_EB, LDI_LEN_POC_EB, eb_buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->poc_eb, eb_buf, LDI_LEN_POC_EB);

	ret = dsim_read_info(lcd, LDI_REG_POC_EC, LDI_LEN_POC_EC, ec_buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->poc_ec, ec_buf, LDI_LEN_POC_EC);

	return ret;
}

static int panel_read_bit_info(struct lcd_info *lcd, u32 index, u8 *rxbuf)
{
	int ret = 0;
	u8 buf[5] = {0, };
	struct bit_info *bit_info_list = ldi_bit_info_list;
	unsigned int reg, len, mask, expect, offset, invert, print_tag, bit;
	char **print_org = NULL;
	char *print_new[sizeof(u8) * BITS_PER_BYTE] = {0, };
	struct decon_device *decon = get_decon_drvdata(0);

	if (!lcd->connected)
		return ret;

	if (index >= LDI_BIT_ENUM_MAX) {
		dev_info(&lcd->ld->dev, "%s: invalid index(%d)\n", __func__, index);
		ret = -EINVAL;
		return ret;
	}

	reg = bit_info_list[index].reg;
	len = bit_info_list[index].len;
	print_org = bit_info_list[index].print;
	expect = bit_info_list[index].expect;
	offset = bit_info_list[index].offset;
	invert = bit_info_list[index].invert;
	mask = bit_info_list[index].mask;
	if (!mask) {
		for (bit = 0; bit < sizeof(u8) * BITS_PER_BYTE; bit++) {
			if (print_org[bit])
				mask |= BIT(bit);
		}
		bit_info_list[index].mask = mask;
	}

	if (offset + len > ARRAY_SIZE(buf)) {
		dev_info(&lcd->ld->dev, "%s: invalid length(%d)\n", __func__, len);
		ret = -EINVAL;
		return ret;
	}

	ret = dsim_read_info(lcd, reg, offset + len, buf);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
		return ret;
	}

	print_tag = buf[offset] & mask;
	print_tag = print_tag ^ invert;

	memcpy(&bit_info_list[index].result, &buf[offset], len);

	if (rxbuf)
		memcpy(rxbuf, &buf[offset], len);

	if (print_tag) {
		for_each_set_bit(bit, (unsigned long *)&print_tag, sizeof(u8) * BITS_PER_BYTE) {
			if (print_org[bit])
				print_new[bit] = print_org[bit];
		}

		if (likely(decon)) {
			dev_info(&lcd->ld->dev, "==================================================\n");
			decon_abd_save_bit(&decon->abd, len * BITS_PER_BYTE, buf[offset], print_new);
		}
		dev_info(&lcd->ld->dev, "==================================================\n");
		dev_info(&lcd->ld->dev, "%s: 0x%02X is invalid. 0x%02X(expect %02X)\n", __func__, reg, buf[offset], expect);
		for (bit = 0; bit < sizeof(u8) * BITS_PER_BYTE; bit++) {
			if (print_new[bit]) {
				if (!bit || !print_new[bit - 1] || strcmp(print_new[bit - 1], print_new[bit]))
					dev_info(&lcd->ld->dev, "* %s (NG)\n", print_new[bit]);
			}
		}
		dev_info(&lcd->ld->dev, "==================================================\n");

	}

	return ret;
}

#if defined(CONFIG_DISPLAY_USE_INFO)
static int panel_inc_dpui_u32_field(struct lcd_info *lcd, enum dpui_key key, u32 value)
{
	if (lcd->connected) {
		inc_dpui_u32_field(key, value);
		if (value)
			dev_info(&lcd->ld->dev, "%s: key(%d) invalid\n", __func__, key);
	}

	return 0;
}

static int s6e3fa7_read_rdnumpe(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_RDNUMPE] = {0, };

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDNUMPE, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
	else
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNDSIE, (buf[0] & LDI_PNDSIE_MASK));

	return ret;
}

static int s6e3fa7_read_esderr(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_ESDERR] = {0, };

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_ESDERR, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
	else {
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNELVDE, !!(buf[0] & LDI_PNELVDE_MASK));
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNVLI1E, !!(buf[0] & LDI_PNVLI1E_MASK));
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNVLO3E, !!(buf[0] & LDI_PNVLO3E_MASK));
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNESDE, !!(buf[0] & LDI_PNESDE_MASK));
	}

	return ret;
}

static int s6e3fa7_read_rddsdr(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_RDDSDR] = {0, };

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDSDR, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
	else
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNSDRE, !(buf[0] & LDI_PNSDRE_MASK));

	return ret;
}
#endif

static int s6e3fa7_read_rddpm(struct lcd_info *lcd)
{
	int ret = 0;

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDPM, &lcd->rddpm);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int s6e3fa7_read_rddsm(struct lcd_info *lcd)
{
	int ret = 0;

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDSM, &lcd->rddsm);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int s6e3fa7_init_elvss(struct lcd_info *lcd, u8 *data)
{
	int i, temp, ret = 0;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			/* Duplicate with reading value from DDI */
			memcpy(&lcd->elvss_table[i][temp][1], data, LDI_LEN_ELVSS);

			lcd->elvss_table[i][temp][0] = LDI_REG_ELVSS;
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_1] = NORMAL_TEMPERATURE;		/* B5h 1st Para: TSET */
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_2] = elvss_mpscon_data[i];		/* B5h 2nd Para: MPS_CON */
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_3] = elvss_offset_data[i][temp];	/* B5h 3rd Para: ELVSS_Dim_offset */
		}
	}

	return ret;
}

static int s6e3fa7_init_hbm_elvss(struct lcd_info *lcd, u8 *data)
{
	int i, temp, ret = 0;

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			/* Duplicate with reading value from DDI */
			memcpy(&lcd->elvss_table[i][temp][1], data, LDI_LEN_ELVSS);

			lcd->elvss_table[i][temp][0] = LDI_REG_ELVSS;
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_1] = NORMAL_TEMPERATURE;		/* B5h 1st Para: TSET */
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_2] = elvss_mpscon_data[i];		/* B5h 2nd Para: MPS_CON */
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_3] = elvss_offset_data[i][temp];	/* B5h 3rd Para: ELVSS_Dim_offset */
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_4] = data[LDI_GPARA_HBM_ELVSS];	/* B5h 23th Para: ELVSS_offset */
		}
	}

	return ret;
}

static int s6e3fa7_init_irc(struct lcd_info *lcd, u8 *data)
{
	int i, j, ret = 0;

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++) {
		/* Duplicate with reading value from DDI */
		for (j = 1; j < IRC_CMD_CNT; j++)
			lcd->irc_table[i][j] = irc_otp_flag[j] ? data[j - 1] : lcd->irc_table[i][j];
	}

#if defined(CONFIG_LCD_HMT)
	/* Duplicate with reading value from DDI */
	for (j = 1; j < IRC_OFF_CNT; j++)
		SEQ_IRC_OFF[j] = irc_otp_flag[j] ? data[j - 1] : SEQ_IRC_OFF[j];
#endif

	return ret;
}

static void init_hbm_interpolation(struct lcd_info *lcd)
{
	lcd->hitp.hbm = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->hitp.gamma_default = gamma_default;

	lcd->hitp.ibr_tbl = index_brightness_table;
	lcd->hitp.idx_ref = IBRIGHTNESS_MAX - 1;
	lcd->hitp.idx_hbm = IBRIGHTNESS_HBM_MAX - 1;
}

static void init_hbm_data(struct lcd_info *lcd, u8 *data)
{
	int i, c;
	int *hbm = lcd->hitp.hbm;
	u8 tmp[IV_MAX * CI_MAX + CI_MAX] = {0, };
	u8 v255[CI_MAX][2] = {{0,}, };

	memcpy(&tmp[6], &data[7], (IV_203 - IV_1 + 1) * CI_MAX);	/* V203 ~ V1 */

	/* V255_R_1: B3h 3rd para D[2] */
	/* V255_R_2: B3h 5th para */
	/* V255_G_1: B3h 3rd para D[1] */
	/* V255_G_2: B3h 6rd para */
	/* V255_B_1: B3h 3rd para D[0] */
	/* V255_B_2: B3h 7th para */
	v255[CI_RED][0] = get_bit(data[2], 2, 1);
	v255[CI_RED][1] = data[4];

	v255[CI_GREEN][0] = get_bit(data[2], 1, 1);
	v255[CI_GREEN][1] = data[5];

	v255[CI_BLUE][0] = get_bit(data[2], 0, 1);
	v255[CI_BLUE][1] = data[6];

	tmp[0] = v255[CI_RED][0];
	tmp[1] = v255[CI_RED][1];
	tmp[2] = v255[CI_GREEN][0];
	tmp[3] = v255[CI_GREEN][1];
	tmp[4] = v255[CI_BLUE][0];
	tmp[5] = v255[CI_BLUE][1];

	/* VT_R: B3h 3rd para D[7:4] */
	/* VT_G: B3h 4th para D[7:4] */
	/* VT_B: B3h 4th para D[3:0] */
	tmp[33] = get_bit(data[2], 4, 4);
	tmp[34] = get_bit(data[3], 4, 4);
	tmp[35] = get_bit(data[3], 0, 4);

	reorder_reg2gamma(tmp, hbm);

	smtd_dbg("HBM_Gamma_Value\n");
	for (i = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++)
			smtd_dbg("%4d ", hbm[i*CI_MAX+c]);
		smtd_dbg("\n");
	}
}

static int init_hbm_gamma(struct lcd_info *lcd)
{
	int i, v, c, ret = 0;
	int *pgamma_def, *pgamma_hbm, *pgamma;
	s64 t1, t2, ratio;
	int gamma[IV_MAX * CI_MAX] = {0, };
	struct hbm_interpolation_t *hitp = &lcd->hitp;

	init_hbm_data(lcd, lcd->hbm);

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++)
		memcpy(&lcd->gamma_table[i], SEQ_GAMMA_CONDITION_SET, GAMMA_CMD_CNT);

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		t1 = hitp->ibr_tbl[i] - hitp->ibr_tbl[hitp->idx_ref];
		t2 = hitp->ibr_tbl[hitp->idx_hbm] - hitp->ibr_tbl[hitp->idx_ref];

		ratio = (t1 << 10) / t2;

		for (v = 0; v < IV_MAX; v++) {
			pgamma_def = (int *)&hitp->gamma_default[v*CI_MAX];
			pgamma_hbm = &hitp->hbm[v*CI_MAX];
			pgamma = &gamma[v*CI_MAX];

			for (c = 0; c < CI_MAX; c++) {
				t1 = pgamma_def[c];
				t1 = t1 << 10;
				t2 = pgamma_hbm[c] - pgamma_def[c];
				pgamma[c] = (t1 + (t2 * ratio)) >> 10;
			}
		}

		reorder_gamma2reg(gamma, lcd->gamma_table[i]);
	}

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		smtd_dbg("Gamma [%3d] = ", lcd->hitp.ibr_tbl[i]);
		for (v = 0; v < GAMMA_CMD_CNT; v++)
			smtd_dbg("%4d ", lcd->gamma_table[i][v]);
		smtd_dbg("\n");
	}

	return ret;
}

static int s6e3fa7_read_init_info(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char elvss_data[LDI_LEN_ELVSS] = {0, };
	unsigned char irc_data[LDI_LEN_IRC] = {0, };

	s6e3fa7_read_id(lcd);

	init_dynamic_aid(lcd);
	init_hbm_interpolation(lcd);

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	s6e3fa7_read_mtp(lcd);
	s6e3fa7_read_coordinate(lcd);
	s6e3fa7_read_chip_id(lcd);
	s6e3fa7_read_elvss(lcd, elvss_data);
	s6e3fa7_read_manufacture_info(lcd);
	s6e3fa7_read_hbm(lcd);
	s6e3fa7_read_irc(lcd, irc_data);
	s6e3fa7_read_poc_info(lcd);
	s6e3fa7_init_elvss(lcd, elvss_data);
	s6e3fa7_init_hbm_elvss(lcd, elvss_data);
	s6e3fa7_init_irc(lcd, irc_data);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	init_gamma(lcd);
	init_hbm_gamma(lcd);

	return ret;
}

static int s6e3fa7_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	s6e3fa7_read_rddpm(lcd);
	s6e3fa7_read_rddsm(lcd);

#if defined(CONFIG_DISPLAY_USE_INFO)
	s6e3fa7_read_rdnumpe(lcd);

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa7_read_esderr(lcd);
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
#endif

	/* 2. Display Off (28h) */
	DSI_WRITE(SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	DSI_WRITE(SEQ_SELF_MASK_OFF, ARRAY_SIZE(SEQ_SELF_MASK_OFF));

	/* 3. Sleep In (10h) */
	DSI_WRITE(SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* 4. Wait 120ms */
	msleep(120);

#if defined(CONFIG_EXYNOS_DOZE)
	mutex_lock(&lcd->lock);
	lcd->current_alpm.value = 0;
	mutex_unlock(&lcd->lock);
#endif

	return ret;
}

static int s6e3fa7_displayon(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 14. Display On(29h) */
	DSI_WRITE(SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int s6e3fa7_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 7. Sleep Out(11h) */
	DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 8. Wait 20ms */
	msleep(20);

#if defined(CONFIG_SEC_FACTORY)
	s6e3fa7_read_init_info(lcd);
#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	attr_store_for_each(lcd->mdnie_class, "color_coordinate", lcd->coordinates, strlen(lcd->coordinates));
#endif
#else
	s6e3fa7_read_id(lcd);
#endif

	/* Test Key Enable */
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

#if defined(CONFIG_DISPLAY_USE_INFO)
	s6e3fa7_read_rddsdr(lcd);
#endif

	/* Parial update setting */
	DSI_WRITE(SEQ_PARTIAL_SETTING, ARRAY_SIZE(SEQ_PARTIAL_SETTING));

	/* 10. Common Setting */
	/* 4.1.2 PCD Setting */
	DSI_WRITE(SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW));
	/* 4.1.3 ERR_FG Setting */
	DSI_WRITE(SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING));
	/* 4.1.4 TSP SYNC Setting */
	DSI_WRITE(SEQ_TSP_SYNC_SETTING, ARRAY_SIZE(SEQ_TSP_SYNC_SETTING));
	/* 4.1.5 FFC Setting */
	DSI_WRITE(SEQ_FFC_SETTING, ARRAY_SIZE(SEQ_FFC_SETTING));

	DSI_WRITE(SEQ_SELF_MASK_SEL, ARRAY_SIZE(SEQ_SELF_MASK_SEL));
	DSI_WRITE(SEQ_SELF_MASK_IMAGE, ARRAY_SIZE(SEQ_SELF_MASK_IMAGE));
	DSI_WRITE(SEQ_SELF_MASK_ON, ARRAY_SIZE(SEQ_SELF_MASK_ON));

	/* Test Key Disable */
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	/* 11. Brightness control */
	dsim_panel_set_brightness(lcd, 1);

	/* 4.1.1 TE(Vsync) ON/OFF */
	DSI_WRITE(SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));

	msleep(20);

#if defined(CONFIG_LCD_HMT)
	if (lcd->hmt_on == 1)
		s6e3fa7_hmt_update(lcd, 1);
#endif

	return ret;
}

#if defined(CONFIG_DISPLAY_USE_INFO)
static int panel_dpui_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct lcd_info *lcd = NULL;
	struct dpui_info *dpui = data;
	char tbuf[MAX_DPUI_VAL_LEN];
	int size;
	unsigned int site, rework, poc, i, invalid = 0;
	unsigned char *m_info;

	struct seq_file m = {
		.buf = tbuf,
		.size = sizeof(tbuf) - 1,
	};

	if (dpui == NULL) {
		pr_err("%s: dpui is null\n", __func__);
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, dpui_notif);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%04d%02d%02d %02d%02d%02d",
			((lcd->date[0] & 0xF0) >> 4) + 2011, lcd->date[0] & 0xF, lcd->date[1] & 0x1F,
			lcd->date[2] & 0x1F, lcd->date[3] & 0x3F, lcd->date[4] & 0x3F);
	set_dpui_field(DPUI_KEY_MAID_DATE, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id_info.id[0]);
	set_dpui_field(DPUI_KEY_LCDID1, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id_info.id[1]);
	set_dpui_field(DPUI_KEY_LCDID2, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id_info.id[2]);
	set_dpui_field(DPUI_KEY_LCDID3, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%s_%s", DPUI_VENDOR_NAME, DPUI_MODEL_NAME);
	set_dpui_field(DPUI_KEY_DISP_MODEL, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "0x%02X%02X%02X%02X%02X",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4]);
	set_dpui_field(DPUI_KEY_CHIPID, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		lcd->date[0], lcd->date[1], lcd->date[2], lcd->date[3], lcd->date[4],
		lcd->date[5], lcd->date[6], (lcd->coordinate[0] & 0xFF00) >> 8, lcd->coordinate[0] & 0x00FF,
		(lcd->coordinate[1] & 0xFF00) >> 8, lcd->coordinate[1] & 0x00FF);
	set_dpui_field(DPUI_KEY_CELLID, tbuf, size);

	m_info = lcd->manufacture_info;
	site = get_bit(m_info[0], 4, 4);
	rework = get_bit(m_info[0], 0, 4);
	poc = get_bit(m_info[1], 0, 4);
	seq_printf(&m, "%d%d%d%02x%02x", site, rework, poc, m_info[2], m_info[3]);

	for (i = 4; i < LDI_LEN_MANUFACTURE_INFO; i++) {
		if (!isdigit(m_info[i]) && !isupper(m_info[i])) {
			invalid = 1;
			break;
		}
	}
	for (i = 4; !invalid && i < LDI_LEN_MANUFACTURE_INFO; i++)
		seq_printf(&m, "%c", m_info[i]);

	set_dpui_field(DPUI_KEY_OCTAID, tbuf, m.count);

	return NOTIFY_DONE;
}
#endif /* CONFIG_DISPLAY_USE_INFO */

static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd_info *lcd = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case DECON_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, fb_notifier);

	fb_blank = *(int *)evdata->data;

	dev_info(&lcd->ld->dev, "%s: %02lx, %d\n", __func__, event, fb_blank);

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&lcd->lock);
		s6e3fa7_displayon(lcd);
		mutex_unlock(&lcd->lock);
	}

	return NOTIFY_DONE;
}

static int s6e3fa7_register_notifier(struct lcd_info *lcd)
{
	lcd->fb_notifier.notifier_call = fb_notifier_callback;
	decon_register_notifier(&lcd->fb_notifier);

#if defined(CONFIG_DISPLAY_USE_INFO)
	lcd->dpui_notif.notifier_call = panel_dpui_notifier_callback;
	if (lcd->connected)
		dpui_logging_register(&lcd->dpui_notif, DPUI_TYPE_PANEL);
#endif

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	return 0;
}

static int s6e3fa7_probe(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	lcd->state = PANEL_STATE_RESUMED;

	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->adaptive_control = ACL_STATUS_ON;
	lcd->lux = -1;

	lcd->acl_table = ACL_TABLE;
	lcd->opr_table = OPR_TABLE;
	lcd->aor_table = AOR_TABLE;
	lcd->irc_table = IRC_TABLE;
	lcd->vint_table = VINT_TABLE;

	ret = s6e3fa7_read_init_info(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to init information\n", __func__);

#if defined(CONFIG_LCD_HMT)
	hmt_init(lcd);
#endif

	dsim_panel_set_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return 0;
}

#if defined(CONFIG_EXYNOS_DOZE)
static int s6e3fa7_setalpm(struct lcd_info *lcd, int mode)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	switch (mode) {
	case HLPM_ON_LOW:
	case ALPM_ON_LOW:
		DSI_WRITE(SEQ_HLPM_CONTROL_02, ARRAY_SIZE(SEQ_HLPM_CONTROL_02));
		DSI_WRITE(SEQ_HLPM_ON_02, ARRAY_SIZE(SEQ_HLPM_ON_02));
		DSI_WRITE(SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		dev_info(&lcd->ld->dev, "%s: HLPM_ON_02, %d\n", __func__, mode);
		break;
	case HLPM_ON_HIGH:
	case ALPM_ON_HIGH:
		DSI_WRITE(SEQ_HLPM_CONTROL_60, ARRAY_SIZE(SEQ_HLPM_CONTROL_60));
		DSI_WRITE(SEQ_HLPM_ON_60, ARRAY_SIZE(SEQ_HLPM_ON_60));
		DSI_WRITE(SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		dev_info(&lcd->ld->dev, "%s: HLPM_ON_60, %d\n", __func__, mode);
		break;
	default:
		dev_info(&lcd->ld->dev, "%s: input is out of range: %d\n", __func__, mode);
		break;
	}

	return ret;
}

static int s6e3fa7_enteralpm(struct lcd_info *lcd)
{
	int ret = 0;
	union lpm_info lpm = {0, };

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	mutex_lock(&lcd->lock);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		goto exit;
	}

	lpm.value = lcd->alpm.value;
	lpm.state = (lpm.ver && lpm.mode) ? lpm_brightness_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	if (lcd->current_alpm.value == lpm.value)
		goto exit;

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	DSI_WRITE(SEQ_SELF_MASK_OFF, ARRAY_SIZE(SEQ_SELF_MASK_OFF));

	/* 2. AOR Setting for HLPM On */
	/* 5.2.1 AOR Setting for HLPM On */
	DSI_WRITE(SEQ_AOR_CONTROL_HLPM_ON, ARRAY_SIZE(SEQ_AOR_CONTROL_HLPM_ON));
	DSI_WRITE(SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

	/* 3. Image Write for HLPM Mode */
	/* 4. Wait 16.7ms */
	msleep(20);

	ret = s6e3fa7_setalpm(lcd, lpm.state);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to set alpm\n", __func__);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	lcd->current_alpm.value = lpm.value;
exit:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int s6e3fa7_exitalpm(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	mutex_lock(&lcd->lock);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		goto exit;
	}

	/* 5.1.2 HLPM Off Sequence (HLPM -> Normal) */
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	/* 2. AOR Setting for HLPM Off */
	/* 5.2.2 AOR Setting for HLPM Off */
	DSI_WRITE(SEQ_HLPM_CONTROL_OFF, ARRAY_SIZE(SEQ_HLPM_CONTROL_OFF));

	/* 3. Wait 33.4ms */
	msleep(34);

	/* 3 Image Write for Normal Mode */
	/* 4. HLPM Off setting */
	/* 5.2.4 HLPM Off Setting */
	DSI_WRITE(SEQ_HLPM_OFF, ARRAY_SIZE(SEQ_HLPM_OFF));

	dev_info(&lcd->ld->dev, "%s: HLPM_OFF\n", __func__);

	DSI_WRITE(SEQ_SELF_MASK_ON, ARRAY_SIZE(SEQ_SELF_MASK_ON));

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	lcd->current_alpm.value = 0;
exit:
	mutex_unlock(&lcd->lock);
	return ret;
}
#endif

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02x %02x %02x\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, bl;
	char *pos = buf;

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++) {
		bl = get_backlight_level_from_brightness(i);
		pos += sprintf(pos, "%3d %3d\n", i, index_brightness_table[bl]);
	}

	return pos - buf;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-15, -14, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value, rc, temperature_index = 0;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	switch (value) {
	case 1:
		temperature_index = TEMP_ABOVE_MINUS_00_DEGREE;
		break;
	case 0:
	case -14:
		temperature_index = TEMP_ABOVE_MINUS_15_DEGREE;
		break;
	case -15:
		temperature_index = TEMP_BELOW_MINUS_15_DEGREE;
		break;
	default:
		dev_info(&lcd->ld->dev, "%s: %d is invalid\n", __func__, value);
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	lcd->temperature = value;
	lcd->temperature_index = temperature_index;
	mutex_unlock(&lcd->lock);

	if (lcd->state == PANEL_STATE_RESUMED)
		dsim_panel_set_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "%s: %d, %d, %d\n", __func__, value, lcd->temperature, lcd->temperature_index);

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", lcd->coordinate[0], lcd->coordinate[1]);

	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month, day, hour, min, sec;
	u16 ms;

	year = ((lcd->date[0] & 0xF0) >> 4) + 2011;
	month = lcd->date[0] & 0xF;
	day = lcd->date[1] & 0x1F;
	hour = lcd->date[2] & 0x1F;
	min = lcd->date[3] & 0x3F;
	sec = lcd->date[4];
	ms = (lcd->date[5] << 8) | lcd->date[6];

	sprintf(buf, "%04d, %02d, %02d, %02d:%02d:%02d.%04d\n", year, month, day, hour, min, sec, ms);

	return strlen(buf);
}

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4]);

	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		lcd->date[0], lcd->date[1], lcd->date[2], lcd->date[3], lcd->date[4],
		lcd->date[5], lcd->date[6], (lcd->coordinate[0] & 0xFF00) >> 8, lcd->coordinate[0] & 0x00FF,
		(lcd->coordinate[1] & 0xFF00) >> 8, lcd->coordinate[1] & 0x00FF);

	return strlen(buf);
}

static void show_aid_log(struct lcd_info *lcd)
{
	u8 temp[256];
	int i, j, k;
	int *mtp;

	mtp = lcd->daid.mtp;
	for (i = 0, j = 0; i < IV_MAX; i++, j += CI_MAX) {
		if (i == 0)
			dev_info(&lcd->ld->dev, "MTP Offset VT   : %4d %4d %4d\n",
				mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
		else
			dev_info(&lcd->ld->dev, "MTP Offset V%3d : %4d %4d %4d\n",
				lcd->daid.iv_tbl[i], mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < GAMMA_CMD_CNT; j++) {
			if (j == 3) {
				k = get_bit(lcd->gamma_table[i][1], 2, 1) * 256;
				j++;
			} else if (j == 4) {
				k = get_bit(lcd->gamma_table[i][1], 1, 1) * 256;
				j++;
			} else if (j == 5) {
				j++;
				continue;
			} else if (j == 6) {
				k = get_bit(lcd->gamma_table[i][1], 0, 1) * 256;
				j++;
			} else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %3d", lcd->gamma_table[i][j] + k);
		}

		dev_info(&lcd->ld->dev, "nit : %3d  %s\n", lcd->daid.ibr_tbl[i], temp);
	}

	mtp = lcd->hitp.hbm;
	for (i = 0, j = 0; i < IV_MAX; i++, j += CI_MAX) {
		if (i == 0)
			dev_info(&lcd->ld->dev, "HBM Gamma VT   : %4d %4d %4d\n",
				mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
		else
			dev_info(&lcd->ld->dev, "HBM Gamma V%3d : %4d %4d %4d\n",
				lcd->daid.iv_tbl[i], mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
	}

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < GAMMA_CMD_CNT; j++) {
			if (j == 3) {
				k = get_bit(lcd->gamma_table[i][1], 2, 1) * 256;
				j++;
			} else if (j == 4) {
				k = get_bit(lcd->gamma_table[i][1], 1, 1) * 256;
				j++;
			} else if (j == 5) {
				j++;
				continue;
			} else if (j == 6) {
				k = get_bit(lcd->gamma_table[i][1], 0, 1) * 256;
				j++;
			} else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %3d", lcd->gamma_table[i][j] + k);
		}

		dev_info(&lcd->ld->dev, "nit : %3d  %s\n", lcd->daid.ibr_tbl[i], temp);
	}

	dev_info(&lcd->ld->dev, "%s\n", __func__);
}

static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	show_aid_log(lcd);
#if defined(CONFIG_LCD_HMT)
	show_hmt_aid_log(lcd);
#endif
	return strlen(buf);
}

static ssize_t adaptive_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->adaptive_control);

	return strlen(buf);
}

static ssize_t adaptive_control_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int rc;
	unsigned int value;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->adaptive_control != value) {
		dev_info(&lcd->ld->dev, "%s: %d, %d\n", __func__, lcd->adaptive_control, value);
		mutex_lock(&lcd->lock);
		lcd->adaptive_control = value;
		mutex_unlock(&lcd->lock);
		if (lcd->state == PANEL_STATE_RESUMED)
			dsim_panel_set_brightness(lcd, 1);
	}

	return size;
}

static ssize_t lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->lux);

	return strlen(buf);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->lux != value) {
		mutex_lock(&lcd->lock);
		lcd->lux = value;
		mutex_unlock(&lcd->lock);

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
		attr_store_for_each(lcd->mdnie_class, attr->attr.name, buf, size);
#endif
	}

	return size;
}

static ssize_t octa_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int site, rework, poc, i, invalid = 0;
	unsigned char *m_info;

	struct seq_file m = {
		.buf = buf,
		.size = PAGE_SIZE - 1,
	};

	m_info = lcd->manufacture_info;
	site = get_bit(m_info[0], 4, 4);
	rework = get_bit(m_info[0], 0, 4);
	poc = get_bit(m_info[1], 0, 4);
	seq_printf(&m, "%d%d%d%02x%02x", site, rework, poc, m_info[2], m_info[3]);

	for (i = 4; i < LDI_LEN_MANUFACTURE_INFO; i++) {
		if (!isdigit(m_info[i]) && !isupper(m_info[i])) {
			invalid = 1;
			break;
		}
	}
	for (i = 4; !invalid && i < LDI_LEN_MANUFACTURE_INFO; i++)
		seq_printf(&m, "%c", m_info[i]);

	seq_puts(&m, "\n");

	return strlen(buf);
}

#if defined(CONFIG_SEC_FACTORY)
static ssize_t poc_enabled_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int ret = 0;

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa7_read_poc_info(lcd);
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	dev_info(&lcd->ld->dev, "%s EC[5]: %x EC[6]: %x EC[8]: %x EB[7]: %x\n",
				__func__, lcd->poc_ec[4], lcd->poc_ec[5], lcd->poc_ec[7], lcd->poc_eb[6]);

	sprintf(buf, "%02x %02x %02x %02x\n", lcd->poc_ec[4], lcd->poc_ec[5], lcd->poc_ec[7], lcd->poc_eb[6]);

	return strlen(buf);
}
#endif

#if defined(CONFIG_LCD_HMT)
static void init_dynamic_aid_for_hmt(struct lcd_info *lcd)
{
	lcd->hmt_daid = lcd->daid;
	lcd->hmt_daid.ibr_tbl = index_brightness_table_hmt;
	lcd->hmt_daid.ibr_max = IBRIGHTNESS_HMT_MAX;

	lcd->hmt_daid.offset_color = (const struct rgb_t(*)[])offset_color_hmt;
	lcd->hmt_daid.m_gray = m_gray_hmt;
}

static int init_hmt_gamma(struct lcd_info *lcd)
{
	int i, j;
	int ret = 0;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kcalloc(IBRIGHTNESS_HMT_MAX, sizeof(int *), GFP_KERNEL);
	if (!gamma) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_HMT_MAX; i++) {
		gamma[i] = kcalloc(IV_MAX*CI_MAX, sizeof(int), GFP_KERNEL);
		if (!gamma[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* pre-allocate memory for gamma table */
	for (i = 0; i < IBRIGHTNESS_HMT_MAX; i++)
		memcpy(&lcd->hmt_gamma_table[i], SEQ_GAMMA_CONDITION_SET, GAMMA_CMD_CNT);

	/* calculate gamma table */
	dynamic_aid(lcd->hmt_daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_HMT_MAX; i++)
		reorder_gamma2reg(gamma[i], lcd->hmt_gamma_table[i]);

	for (i = 0; i < IBRIGHTNESS_HMT_MAX; i++) {
		smtd_dbg("Gamma [%3d] = ", lcd->hmt_daid.ibr_tbl[i]);
		for (j = 0; j < GAMMA_CMD_CNT; j++)
			smtd_dbg("%4d ", lcd->hmt_gamma_table[i][j]);
		smtd_dbg("\n");
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_HMT_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}

static void show_hmt_aid_log(struct lcd_info *lcd)
{
	u8 temp[256];
	int i, j, k;

	for (i = 0; i < IBRIGHTNESS_HMT_MAX; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < GAMMA_CMD_CNT; j++) {
			if (j == 3) {
				k = get_bit(lcd->hmt_gamma_table[i][1], 2, 1) * 256;
				j++;
			} else if (j == 4) {
				k = get_bit(lcd->hmt_gamma_table[i][1], 1, 1) * 256;
				j++;
			} else if (j == 5) {
				j++;
				continue;
			} else if (j == 6) {
				k = get_bit(lcd->hmt_gamma_table[i][1], 0, 1) * 256;
				j++;
			} else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %3d", lcd->hmt_gamma_table[i][j] + k);
		}

		dev_info(&lcd->ld->dev, "nit : %3d  %s\n", lcd->hmt_daid.ibr_tbl[i], temp);
	}
	dev_info(&lcd->ld->dev, "%s\n", __func__);
}

static int hmt_init(struct lcd_info *lcd)
{
	init_dynamic_aid_for_hmt(lcd);
	init_hmt_gamma(lcd);

	mutex_lock(&lcd->lock);
	lcd->hmt_on = lcd->dsim->hmt_on = 0;
	lcd->hmt_bl = lcd->hmt_current_bl = 0;
	lcd->hmt_brightness = DEFAULT_HMT_BRIGHTNESS;
	mutex_unlock(&lcd->lock);

	return 0;
}

static int get_backlight_level_from_hmt_brightness(int brightness)
{
	return hmt_brightness_table[brightness];
}

static int low_level_set_brightness_for_hmt(struct lcd_info *lcd, int force)
{
	int ret = 0;
	unsigned char hmt_elvss[] = {0xB5, NORMAL_TEMPERATURE, 0xCC, 0x04};

	dev_info(&lcd->ld->dev, "%s++\n", __func__);
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	DSI_WRITE(lcd->hmt_gamma_table[lcd->hmt_bl], GAMMA_CMD_CNT);			/* gamma */

	if (lcd->hmt_bl >= IBRIGHTNESS_HMT_077NIT)					/* aor */
		DSI_WRITE(SEQ_AID_FOR_HMT[AID_HMT_UPPER], AID_CMD_CNT);
	else
		DSI_WRITE(SEQ_AID_FOR_HMT[AID_HMT_LOWER], AID_CMD_CNT);

	DSI_WRITE(hmt_elvss, ARRAY_SIZE(hmt_elvss));					/* elvss */
	DSI_WRITE(SEQ_ACL_OFF, ACL_CMD_CNT);						/* acl off */
	DSI_WRITE(SEQ_IRC_OFF, IRC_OFF_CNT);						/* irc off */

	DSI_WRITE(SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	dev_info(&lcd->ld->dev, "%s--\n", __func__);
	return ret;
}


static int hmt_set_mode(struct lcd_info *lcd, u8 forced)
{
	int ret = 0;

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (lcd->hmt_on) {
		DSI_WRITE(SEQ_HMT_ON, ARRAY_SIZE(SEQ_HMT_ON));
		DSI_WRITE(SEQ_AID_REVERSE, ARRAY_SIZE(SEQ_AID_REVERSE));
		dev_info(&lcd->ld->dev, "%s: hmt on %d %d\n", __func__, lcd->hmt_on, lcd->dsim->hmt_on);
	} else {
		DSI_WRITE(SEQ_HMT_OFF, ARRAY_SIZE(SEQ_HMT_OFF));
		DSI_WRITE(SEQ_AID_FORWARD, ARRAY_SIZE(SEQ_AID_FORWARD));
		dev_info(&lcd->ld->dev, "%s: hmt off %d %d\n", __func__, lcd->hmt_on, lcd->dsim->hmt_on);
	}
	DSI_WRITE(SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));


	return ret;
}

static int dsim_panel_set_hmt_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;

	mutex_lock(&lcd->lock);
	lcd->hmt_bl = get_backlight_level_from_hmt_brightness(lcd->hmt_brightness);

	if (!force && lcd->state != PANEL_STATE_RESUMED) {
		lcd->hmt_current_bl = lcd->hmt_bl;
		dev_info(&lcd->ld->dev, "%s: hmt brightness: %d, panel_state: %d\n", __func__, lcd->hmt_brightness, lcd->state);
		goto exit;
	}

	ret = low_level_set_brightness_for_hmt(lcd, force);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to set hmt brightness : %d\n", __func__, index_brightness_table_hmt[lcd->hmt_bl]);

	lcd->hmt_current_bl = lcd->hmt_bl;

	dev_info(&lcd->ld->dev, "hmt brightness: %d, hmt bl: %d, hmt nit: %d\n",
	lcd->hmt_brightness, lcd->hmt_bl, index_brightness_table_hmt[lcd->hmt_bl]);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}


static int s6e3fa7_hmt_update(struct lcd_info *lcd, u8 forced)
{
/*
 *	1. set hmt comaand
 *	2. set hmt brightness
 */

	hmt_set_mode(lcd, forced);
	if (lcd->hmt_on)
		dsim_panel_set_hmt_brightness(lcd, forced);
	else
		dsim_panel_set_brightness(lcd, forced);

	return 0;
}

static ssize_t hmt_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "index : %d, brightness : %d\n", lcd->hmt_current_bl, lcd->hmt_brightness);

	return strlen(buf);
}

static ssize_t hmt_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "++ %s: %d\n", __func__, value);
	if (!lcd->hmt_on) {
		dev_info(&lcd->ld->dev, "%s: hmt is not on\n", __func__);
		return -EINVAL;
	}

	if (lcd->hmt_brightness != value) {
		mutex_lock(&lcd->lock);
		lcd->hmt_brightness = value;
		mutex_unlock(&lcd->lock);
		dsim_panel_set_hmt_brightness(lcd, 0);
	}
	dev_info(&lcd->ld->dev, "-- %s: %d\n", __func__, value);

	return size;
}

static ssize_t hmt_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->hmt_on);

	return strlen(buf);
}

static ssize_t hmt_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->hmt_on != value) {
		dev_info(&lcd->ld->dev, "%s: %d\n", __func__, lcd->hmt_on);
		mutex_lock(&lcd->lock);
		lcd->hmt_on = lcd->dsim->hmt_on = value;
		mutex_unlock(&lcd->lock);
		s6e3fa7_hmt_update(lcd, 1);
		dev_info(&lcd->ld->dev, "%s: finish %d\n", __func__, lcd->hmt_on);
	} else
		dev_info(&lcd->ld->dev, "%s: hmt already %s\n", __func__, value ? "on" : "off");

	return size;
}

static DEVICE_ATTR(hmt_bright, 0664, hmt_brightness_show, hmt_brightness_store);
static DEVICE_ATTR(hmt_on, 0664, hmt_on_show, hmt_on_store);
#endif

#if defined(CONFIG_DISPLAY_USE_INFO)
/*
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);

	return size;
}

/*
 * [DEV ONLY]
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_dbg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_dbg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);

	return size;
}

static DEVICE_ATTR(dpui, 0660, dpui_show, dpui_store);
static DEVICE_ATTR(dpui_dbg, 0660, dpui_dbg_show, dpui_dbg_store);
#endif

#if defined(CONFIG_EXYNOS_DOZE)
#if defined(CONFIG_SEC_FACTORY)
static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct decon_device *decon = get_decon_drvdata(0);
	struct fb_info *fbinfo = decon->win[decon->dt.dft_win]->fbinfo;
	union lpm_info lpm = {0, };
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %06x, lpm: %06x(%06x)\n", __func__,
		value, lcd->current_alpm.value, lcd->alpm.value);

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		return -EINVAL;
	}

	if (!lock_fb_info(fbinfo)) {
		dev_info(&lcd->ld->dev, "%s: fblock is failed\n", __func__);
		return -EINVAL;
	}

	lpm.ver = get_bit(value, 16, 8);
	lpm.mode = get_bit(value, 0, 8);

	if (!lpm.ver && lpm.mode >= ALPM_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		unlock_fb_info(fbinfo);
		return -EINVAL;
	}

	if (lpm.ver && lpm.mode >= AOD_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		unlock_fb_info(fbinfo);
		return -EINVAL;
	}

	lpm.state = (lpm.ver && lpm.mode) ? lpm_brightness_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	mutex_lock(&lcd->lock);
	lcd->prev_alpm = lcd->alpm;
	lcd->alpm = lpm;
	mutex_unlock(&lcd->lock);

	switch (lpm.mode) {
	case ALPM_OFF:
		if (lcd->prev_brightness) {
			mutex_lock(&lcd->lock);
			lcd->bd->props.brightness = lcd->prev_brightness;
			lcd->prev_brightness = 0;
			mutex_unlock(&lcd->lock);
		}
		mutex_lock(&decon->lock);
		call_panel_ops(dsim, displayon, dsim);	/* for exitalpm */
		mutex_unlock(&decon->lock);
		usleep_range(17000, 18000);
		mutex_lock(&lcd->lock);
		s6e3fa7_displayon(lcd);
		mutex_unlock(&lcd->lock);
		break;
	case ALPM_ON_LOW:
	case HLPM_ON_LOW:
	case ALPM_ON_HIGH:
	case HLPM_ON_HIGH:
		if (lcd->prev_alpm.mode == ALPM_OFF) {
			mutex_lock(&lcd->lock);
			lcd->prev_brightness = lcd->bd->props.brightness;
			mutex_unlock(&lcd->lock);
		}
		mutex_lock(&decon->lock);
		lcd->bd->props.brightness = (value <= HLPM_ON_LOW) ? 0 : UI_MAX_BRIGHTNESS;
		call_panel_ops(dsim, doze, dsim);
		mutex_unlock(&decon->lock);
		usleep_range(17000, 18000);
		mutex_lock(&lcd->lock);
		s6e3fa7_displayon(lcd);
		mutex_unlock(&lcd->lock);
		break;
	}

	unlock_fb_info(fbinfo);

	return size;
}
#else
static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct decon_device *decon = get_decon_drvdata(0);
	union lpm_info lpm = {0, };
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %06x, lpm: %06x(%06x)\n", __func__,
		value, lcd->current_alpm.value, lcd->alpm.value);

	lpm.ver = get_bit(value, 16, 8);
	lpm.mode = get_bit(value, 0, 8);

	if (!lpm.ver && lpm.mode >= ALPM_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		return -EINVAL;
	}

	if (lpm.ver && lpm.mode >= AOD_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		return -EINVAL;
	}

	lpm.state = (lpm.ver && lpm.mode) ? lpm_brightness_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	if (lcd->alpm.value == lpm.value && lcd->current_alpm.value == lpm.value) {
		dev_info(&lcd->ld->dev, "%s: unchanged lpm value: %x\n", __func__, lpm.value);
		return size;
	}

	mutex_lock(&decon->lock);
	mutex_lock(&lcd->lock);
	lcd->alpm = lpm;
	mutex_unlock(&lcd->lock);

	if (dsim->state == DSIM_STATE_DOZE)
		call_panel_ops(dsim, doze, dsim);
	mutex_unlock(&decon->lock);

	return size;
}
#endif

static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%06x\n", lcd->current_alpm.value);

	return strlen(buf);
}

static DEVICE_ATTR(alpm, 0664, alpm_show, alpm_store);
#endif

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(cell_id, 0444, cell_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
static DEVICE_ATTR(adaptive_control, 0664, adaptive_control_show, adaptive_control_store);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);
static DEVICE_ATTR(octa_id, 0444, octa_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA, 0444, cell_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA_CHIPID, 0444, octa_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA_DDI_CHIPID, 0444, manufacture_code_show, NULL);
#if defined(CONFIG_SEC_FACTORY)
static DEVICE_ATTR(poc_enabled, 0444, poc_enabled_show, NULL);
#endif

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_temperature.attr,
	&dev_attr_color_coordinate.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_aid_log.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_adaptive_control.attr,
	&dev_attr_lux.attr,
	&dev_attr_octa_id.attr,
#if defined(CONFIG_EXYNOS_DOZE)
	&dev_attr_alpm.attr,
#endif
#if defined(CONFIG_DISPLAY_USE_INFO)
	&dev_attr_dpui.attr,
	&dev_attr_dpui_dbg.attr,
#endif
#if defined(CONFIG_LCD_HMT)
	&dev_attr_hmt_on.attr,
	&dev_attr_hmt_bright.attr,
#endif
#if defined(CONFIG_SEC_FACTORY)
	&dev_attr_poc_enabled.attr,
#endif
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static void lcd_init_svc(struct lcd_info *lcd)
{
	struct device *dev = &lcd->svc_dev;
	struct kobject *top_kobj = &lcd->ld->dev.kobj.kset->kobj;
	struct kernfs_node *kn = kernfs_find_and_get(top_kobj->sd, "svc");
	struct kobject *svc_kobj = NULL;
	char *buf = NULL;
	int ret = 0;

	svc_kobj = kn ? kn->priv : kobject_create_and_add("svc", top_kobj);
	if (!svc_kobj)
		return;

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (buf) {
		kernfs_path(svc_kobj->sd, buf, PATH_MAX);
		dev_info(&lcd->ld->dev, "%s: %s %s\n", __func__, buf, !kn ? "create" : "");
		kfree(buf);
	}

	dev->kobj.parent = svc_kobj;
	dev_set_name(dev, "OCTA");
	dev_set_drvdata(dev, lcd);
	ret = device_register(dev);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: device_register fail\n", __func__);
		return;
	}

	device_create_file(dev, &dev_attr_SVC_OCTA);
	device_create_file(dev, &dev_attr_SVC_OCTA_CHIPID);
	device_create_file(dev, &dev_attr_SVC_OCTA_DDI_CHIPID);

	if (kn)
		kernfs_put(kn);
}

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "failed to add lcd sysfs\n");

	lcd_init_svc(lcd);
}

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
static int mdnie_send_seq(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		ret = -EIO;
		goto exit;
	}

	ret = dsim_write_set(lcd, seq, num);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}

static int mdnie_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		ret = -EIO;
		goto exit;
	}

	ret = dsim_read_hl_data(lcd, addr, size, buf);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}
#endif

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;

	dsim->priv.par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s: failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		pr_err("%s: failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto exit;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s: failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto exit;
	}

	mutex_init(&lcd->lock);

	lcd->dsim = dsim;
	ret = s6e3fa7_probe(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to probe panel\n", __func__);

	lcd_init_sysfs(lcd);

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)mdnie_send_seq, (mdnie_r)mdnie_read, lcd->coordinate, &tune_info);
	lcd->mdnie_class = get_mdnie_class();
#endif

	s6e3fa7_register_notifier(lcd);

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);

exit:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		s6e3fa7_init(lcd);
#if defined(CONFIG_EXYNOS_DOZE)
		if (lcd->current_alpm.state) /* not sure this is useful */
			msleep(104);
#endif
		mutex_lock(&lcd->lock);
		lcd->state = PANEL_STATE_RESUMED;
		mutex_unlock(&lcd->lock);
	}

#if defined(CONFIG_EXYNOS_DOZE)
	if (lcd->current_alpm.state) {
		s6e3fa7_exitalpm(lcd);

		dsim_panel_set_brightness(lcd, 1);
	}
#endif
	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto exit;

	s6e3fa7_exit(lcd);

	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENED;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

exit:
	return 0;
}

#if defined(CONFIG_EXYNOS_DOZE)
static int dsim_panel_enteralpm(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		s6e3fa7_init(lcd);

		msleep(104);
		mutex_lock(&lcd->lock);
		lcd->state = PANEL_STATE_RESUMED;
		mutex_unlock(&lcd->lock);
	}

	s6e3fa7_enteralpm(lcd);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}
#endif

#if defined(CONFIG_LOGGING_BIGDATA_BUG)
static unsigned int dsim_get_panel_bigdata(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	unsigned int val = 0;

	lcd->rddpm = 0xff;
	lcd->rddsm = 0xff;

	s6e3fa7_read_rddpm(lcd);
	s6e3fa7_read_rddsm(lcd);

	val = (lcd->rddpm  << 8) | lcd->rddsm;

	return val;
}
#endif

struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver = {
	.name		= "s6e3fa7",
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
#if defined(CONFIG_EXYNOS_DOZE)
	.doze		= dsim_panel_enteralpm,
#endif
#if defined(CONFIG_LOGGING_BIGDATA_BUG)
	.get_buginfo	= dsim_get_panel_bigdata,
#endif
};
__XX_ADD_LCD_DRIVER(s6e3fa7_mipi_lcd_driver);

