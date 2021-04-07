/* linux/drivers/video/decon_display/s6e3fa0_param.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __S6E3FA0_PARAM_H__
#define __S6E3FA0_PARAM_H__

/* MIPI commands list */
static const unsigned char SEQ_TEST_KEY_ON_C0[] = {
	0xc0,
	0x63, 0x02, 0x03, 0x32, 0xFF, 0x44, 0x44, 0xC0, 0x00, 0x40
};

static const unsigned char SEQ_TEST_KEY_ON_EB[] = {
	0xeb,
	0x01, 0x00
};

static const unsigned char SEQ_TEST_KEY_ON_F2[] = {
	0xf2,
	0x02,
};

static const unsigned char SEQ_MIC_ENABLE[] = {
	0xf9,
	0x2b
};

static const unsigned char SEQ_TEST_KEY_ON_E7[] = {
	0xE7,
	0xED, 0xC7, 0x23, 0x57, 0xA5
};

static const unsigned char SEQ_TEST_KEY_ON_F6[] = {
	0xf6,
	0x08
};

static const unsigned char SEQ_TEST_KEY_ON_FD[] = {
	0xfd,
	0x16, 0x80
};

static const unsigned char SEQ_TEST_KEY_ON_ED[] = {
	0xed,
	0x01, 0x00
};

static const unsigned char SEQ_READ_ID[] = {
	0x04,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SEQ_GAMMA_CONTROL_SET_300CD[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x06, 0x06, 0x06, 0x48, 0x18, 0x3F, 0xFF,
	0xFF,
};

static const unsigned char SEQ_ELVSS_CONDITION_SET[] = {
	0xB6,
	0x88, 0x0A,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03, 0x00
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_ACL_CONTROL[] = {
	0xB5,
	0x03, 0x98, 0x26, 0x36, 0x45,
};

static const unsigned char SEQ_ETC_PENTILE_SETTING[] = {
	0xC0,
	0x00, 0x02, 0x03, 0x32, 0xD8, 0x44, 0x44, 0xC0, 0x00, 0x48,
	0x20, 0xD8,
};

static const unsigned char SEQ_GLOBAL_PARAM_SOURCE_AMP[] = {
	0xB0,
	0x24,
};

static const unsigned char SEQ_ETC_SOURCE_AMP[] = {
	0xD7,
	0xA5,
};

static const unsigned char SEQ_GLOBAL_PARAM_BIAS_CURRENT[] = {
	0xB0,
	0x1F,
};

static const unsigned char SEQ_ETC_BIAS_CURRENT[] = {
	0xD7,
	0x0A,
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00,  0x00
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char SEQ_TOUCHKEY_OFF[] = {
	0xFF,
	0x00,
};

static const unsigned char SEQ_TOUCHKEY_ON[] = {
	0xFF,
	0x01,
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55, 0x00,
	0x00
};

static const unsigned char SEQ_ACL_40[] = {
	0x55, 0x02,
	0x00
};

static const unsigned char SEQ_ACL_40_RE_LOW[] = {
	0x55, 0x02,
	0x00
};

static const unsigned char SEQ_DISPCTL[] = {
	0xF2,
	0x02, 0x03, 0xC, 0xA0, 0x01, 0x48
};

#endif /* __S6E3FA0_PARAM_H__ */
