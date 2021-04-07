/* drivers/video/exynos/panels/s6e3fa7_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include "lcd_ctrl.h"

#include "../dsim.h"
#include <video/mipi_display.h>

#define EXTEND_BRIGHTNESS	365
#define UI_MAX_BRIGHTNESS	255
#define UI_MIN_BRIGHTNESS	0
#define UI_DEFAULT_BRIGHTNESS	128
#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */

#define GAMMA_CMD_CNT				((u16)ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET))
#define ACL_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ACL_OFF))
#define OPR_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ACL_OPR_OFF))
#define ELVSS_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ELVSS_SET))
#define AID_CMD_CNT				((u16)ARRAY_SIZE(SEQ_AID_SETTING))
#define IRC_CMD_CNT				((u16)ARRAY_SIZE(SEQ_IRC_SETTING))

#define LDI_REG_ELVSS				0xB5
#define LDI_REG_COORDINATE			0xA1
#define LDI_REG_DATE				LDI_REG_MTP
#define LDI_REG_ID				0x04
#define LDI_REG_CHIP_ID				0xD6
#define LDI_REG_MTP				0xC8
#define LDI_REG_HBM				0xB3
#define LDI_REG_MANUFACTURE_INFO		0xC9
#define LDI_REG_IRC				0xB8
#define LDI_REG_RDDPM				0x0A
#define LDI_REG_RDDSM				0x0E
#define LDI_REG_ESDERR				0xEE

/* len is read length */
#define LDI_LEN_ELVSS				(ELVSS_CMD_CNT - 1)
#define LDI_LEN_COORDINATE			4
#define LDI_LEN_DATE				7
#define LDI_LEN_ID				3
#define LDI_LEN_CHIP_ID				5
#define LDI_LEN_MTP				32
#define LDI_LEN_HBM				34
#define LDI_LEN_MANUFACTURE_INFO		21
#define LDI_LEN_IRC				(IRC_CMD_CNT - 1)
#define LDI_LEN_RDDPM				1
#define LDI_LEN_RDDSM				1
#define LDI_LEN_ESDERR				1

/* offset is position including addr, not only para */
#define LDI_OFFSET_AOR_1	1
#define LDI_OFFSET_AOR_2	2

#define LDI_OFFSET_ELVSS_1	1	/* B5h 1st Para: TSET */
#define LDI_OFFSET_ELVSS_2	2	/* B5h 2nd Para: MPS_CON */
#define LDI_OFFSET_ELVSS_3	3	/* B5h 3rd Para: ELVSS_Dim_offset */
#define LDI_OFFSET_ELVSS_4	23	/* B5h 23th Para: ELVSS_Cal_Offset */

#define LDI_OFFSET_OPR_1	1	/* B4h 1st Para: 16 Frame Avg at ACL Off (HBM OPR Cal mode) */
#define LDI_OFFSET_OPR_2	5	/* B4h 5th Para: ACL Percent */
#define LDI_OFFSET_OPR_3	13	/* B4h 13th Para: 16 Frame Avg at ACL Off (Normal OPR Cal mode) */

#define LDI_OFFSET_ACL		1

#define LDI_GPARA_DATE		40	/* C8h 41th Para: Manufacture Year, Month */
#define LDI_GPARA_HBM_ELVSS	23	/* B5h 24th Para: ELVSS_Cal_Offset for HBM */

/*
 * 3FA7 lcd init sequence
 *
 * Parameters
 *	- mic : if mic is enabled, MIC_ENABLE command must be sent
 *	- mode : LCD init sequence depends on command or video mode
 */

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C
};

static const unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44
};

static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
};

static const unsigned char SEQ_AID_SETTING[] = {
	0xB1,
	0x00, 0x10
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB5,
	0x19,	/* B5h 1st Para: TSET */
	0xDC,	/* B5h 2nd Para: MPS_CON */
	0x04,	/* B5h 3rd Para: ELVSS_Dim_offset */
	0x01, 0x34, 0x67, 0x9A, 0xCD, 0x01, 0x22,
	0x33, 0x44, 0xC0, 0x00, 0x09, 0x99, 0x33, 0x13, 0x01, 0x11,
	0x10, 0x00,
	0x00,	/* RESERVED: B5h 23th Para: ELVSS_Cal_Offset */
	0x00	/* RESERVED: B5h 24th Para: ELVSS_Cal_Offset for HBM */
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static const unsigned char SEQ_ACL_OPR_OFF[] = {
	0xB4,
	0x40,	/* B4h 1st Para: Para : 0x40 = 16 Frame Avg at ACL Off (HBM OPR Cal mode) */
	0x40, 0xFC, 0x48,
	0x48,	/* B4h 5th Para: 0x48 = ACL 15% */
	0x9C, 0x55, 0x55, 0x55, 0x3F,
	0xB7, 0x12,
	0x40	/* B4h 13th Para: 0x40 = 16 Frame Avg at ACL Off (Normal OPR Cal mode) */
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00	/* 0x00 : ACL OFF */
};

static const unsigned char SEQ_IRC_SETTING[] = {
	0xB8,
	0x40, 0x20, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80,
	0x61, 0x3D, 0x46, 0x6F, 0xE3, 0x33, 0x69, 0x12, 0x7A, 0x8C,
	0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x3C, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};

static int dsi_write(u32 id, const unsigned char *wbuf, int size)
{
	int ret = 0;

	if (size == 1)
		ret = dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, wbuf[0], 0);
	else
		ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)wbuf, size);

	mdelay(12);

	return ret;
}

#if 0
static int dsi_read(u32 id, const u8 addr, u16 len, u8 *buf)
{
	int ret = 0;

	ret = dsim_rd_data(id, MIPI_DSI_DCS_READ, addr, len, buf);

	return ret;
}
#endif
static int s6e3fa7_read_id(u32 id)
{
#if 0
	int i, ret, retry_cnt = 1;
	u8 buf[LDI_LEN_ID];

	for (i = 0; i < LDI_LEN_ID; i++)
		buf[i] = 0;
retry:
	ret = dsi_read(id, LDI_REG_ID, LDI_LEN_ID, (u8 *)buf);
	if (ret <= 0) {
		if (retry_cnt) {
			dsim_err("%s: retry: %d\n", __func__, retry_cnt);
			retry_cnt--;
			goto retry;
		} else
			dsim_err("%s: 0x%02x\n", __func__, LDI_REG_ID);
		return 0;
	}

	for (i = 0; i < LDI_LEN_ID; i++)
		dsim_dbg("%x, ", buf[i]);
	dsim_dbg("\n");

	return ret;
#else
	return 1;
#endif
}

void lcd_init(int id, struct decon_lcd *lcd)
{
	dsim_dbg("%s +\n", __func__);

	/* 7. Sleep Out(11h) */
	if (dsi_write(id, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT)) < 0)
		dsim_err("fail to send SEQ_SLEEP_OUT command.\n");

	/* 8. Wait 20ms */
	msleep(20);

	/* 9. ID READ */
	s6e3fa7_read_id(id);

	/* Test Key Enable */
	if (dsi_write(id, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to send SEQ_TEST_KEY_ON_F0 command.\n");

	/* 10. Common Setting */
	/* 4.1.2 PCD Setting */
	if (dsi_write(id, SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW)) < 0)
		dsim_err("fail to send SEQ_PCD_SET_DET_LOW command.\n");
	/* 4.1.3 ERR_FG Setting */
	if (dsi_write(id, SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING)) < 0)
		dsim_err("fail to send SEQ_ERR_FG_SETTING command.\n");

	/* 11. Brightness control */
	if (dsi_write(id, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET)) < 0)
		dsim_err("fail to send SEQ_GAMMA_CONDITION_SET command.\n");
	if (dsi_write(id, SEQ_AID_SETTING, ARRAY_SIZE(SEQ_AID_SETTING)) < 0)
		dsim_err("fail to send SEQ_AID_SETTING command.\n");
	if (dsi_write(id, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET)) < 0)
		dsim_err("fail to send SEQ_ELVSS_SET command.\n");

	/* 4.2.4 ACL ON/OFF */
	if (dsi_write(id, SEQ_ACL_OPR_OFF, ARRAY_SIZE(SEQ_ACL_OPR_OFF)) < 0)
		dsim_err("fail to send SEQ_ACL_OPR_OFF command.\n");
	if (dsi_write(id, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF)) < 0)
		dsim_err("fail to send SEQ_ACL_OFF command.\n");
	if (dsi_write(id, SEQ_IRC_SETTING, ARRAY_SIZE(SEQ_IRC_SETTING)) < 0)
		dsim_err("fail to send SEQ_IRC_SETTING command.\n");

	if (dsi_write(id, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("fail to send SEQ_GAMMA_UPDATE command.\n");

	/* Test Key Disable */
	if (dsi_write(id, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to send SEQ_TEST_KEY_OFF_F0 command.\n");

	/* 4.1.1 TE(Vsync) ON/OFF */
	if (dsi_write(id, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON)) < 0)
		dsim_err("fail to send SEQ_TE_ON command.\n");

	/* 12. Wait 80ms */
	msleep(80);

	dsim_dbg("%s -\n", __func__);
}

void lcd_enable(int id)
{
	dsim_dbg("%s +\n", __func__);

	/* 16. Display On(29h) */
	if (dsi_write(id, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON)) < 0)
		dsim_err("fail to send SEQ_DISPLAY_ON command.\n");

	dsim_dbg("%s -\n", __func__);
}

void lcd_disable(int id)
{
	/* This function needs to implement */
}

/*
 * Set gamma values
 *
 * Parameter
 *	- backlightlevel : It is from 0 to 26.
 */
int lcd_gamma_ctrl(int id, u32 backlightlevel)
{
	return 0;
}

int lcd_gamma_update(int id)
{
	return 0;
}
