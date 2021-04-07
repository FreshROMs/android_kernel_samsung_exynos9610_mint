/* drivers/video/exynos/panels/ea8076_lcd_ctrl.c
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


/*
 * EA8076 lcd init sequence
 */

static unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};
/*
static unsigned char SEQ_SLEEP_IN[] = {
	0x10
};
*/
static unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};
/*
static unsigned char SEQ_DISPLAY_OFF[] = {
	0x28
};
*/
static unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x20
};

static unsigned char SEQ_BRIGHTNESS[] = {
	0x51,
	0x01, 0xBD
};

static unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00, 0x00
};

static unsigned char SEQ_PAGE_ADDR_SETTING[] = {
	0x2B,
	0x00, 0x00, 0x09, 0x23
};


static unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static unsigned char SEQ_FFC_SET[] = {
	0xE9,
	0x11, 0x55, 0x98, 0x96, 0x80, 0xB2, 0x41, 0xC3, 0x00, 0x1A,
	0xB8  /* MIPI Speed 1.2Gbps */
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

static int ea8076_read_id(u32 id)
{
	return 1;
}

void lcd_init(int id, struct decon_lcd *lcd)
{
	dsim_dbg("%s +\n", __func__);

	/* 6. Sleep Out(11h) */
	dsi_write(id, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 7. Wait 10ms */
	udelay(10000);

	/* ID READ */
	ea8076_read_id(id);

	/* 8. Common Setting */
	/* Test Key Enable */
	dsi_write(id, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	/* 4.1.1 TE(Vsync) ON/OFF */
	dsi_write(id, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	/* 4.1.2 PAGE ADDRESS SET */
	dsi_write(id, SEQ_PAGE_ADDR_SETTING, ARRAY_SIZE(SEQ_PAGE_ADDR_SETTING));


	/* 8.3 FFC SET */
	dsi_write(id, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	dsi_write(id, SEQ_FFC_SET, ARRAY_SIZE(SEQ_FFC_SET));
	dsi_write(id, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	/* 10. Brightness Setting */
	dsi_write(id, SEQ_HBM_OFF, ARRAY_SIZE(SEQ_HBM_OFF));
	dsi_write(id, SEQ_BRIGHTNESS, ARRAY_SIZE(SEQ_BRIGHTNESS));
	dsi_write(id, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	/* Test Key Disable */
	dsi_write(id, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	/* 11. Wait 110ms */
	msleep(110);

	dsim_dbg("%s -\n", __func__);
}

void lcd_enable(int id)
{
	dsim_dbg("%s +\n", __func__);

	 /* 12. Display On(29h) */
	 dsi_write(id, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	 msleep(20);

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
