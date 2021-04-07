/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350_tun.c
 *
 *	Description : API source of ISDB-T baseband module
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	History :
 *	----------------------------------------------------------------------
 *******************************************************************************/
#include "fci_types.h"
#include "fci_oal.h"
#include "fci_tun.h"
#include "fci_hal.h"
#include "fc8350_regs.h"
#include "fc8350_tun_table.h"

#define NOSAW
#define DRIVER_VERSION 54

#define PD_L 1
#define PD_H 14

#define FC8350_XTAL_FREQ      BBM_XTAL_FREQ
#define FC8350_BAND_WIDTH     BBM_BAND_WIDTH
#define FC8350_BAND_WIDTH_DVB BBM_BAND_WIDTH_DVB

static enum BROADCAST_TYPE broadcast_type = ISDBT_13SEG;

static s32 fc8350_write(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
{
	s32 res;

	res = bbm_write(handle, devid, (0xc000 | addr), data);

	return res;
}

static s32 fc8350_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
{
	s32 res;

	res = bbm_read(handle, devid, (0xc000 | addr), data);

	return res;
}
/*
 *static s32 fc8350_bb_write(HANDLE handle, DEVICEID devid, u16 addr, u8 data)
 *{
 *	s32 res;
 *
 *	res = bbm_write(handle, devid, addr, data);
 *
 *	return res;
 *}
 *
 *static s32 fc8350_bb_read(HANDLE handle, DEVICEID devid, u16 addr, u8 *data)
 *{
 *	s32 res;
 *
 *	res = bbm_read(handle, devid, addr, data);
 *
 *	return res;
 *}
 */

static void fc8350_pd_man(HANDLE handle, DEVICEID devid)
{
	u8 rf_fast = 0;
	u8 mix_fast = 0;
	u8 state = 0;
	u8 i = 0;
	u8 cal[5] = {50, 30, 0};

	for (; i < 2; i++) {
		msWait(50);

		fc8350_read(handle, devid, 0xe9, &rf_fast);
		fc8350_read(handle, devid, 0xeb, &mix_fast);

		if ((rf_fast < PD_L) || (rf_fast > PD_H))
			state = state + 1;
		if ((mix_fast < PD_L) || (mix_fast > PD_H))
			state = state + 2;
		if (i == 4)
			break;
		if (state == 1) {
			fc8350_write(handle, devid, 0x9b, cal[i]);
			fc8350_write(handle, devid, 0x66, 0x06);
			msWait(1);
			fc8350_write(handle, devid, 0x66, 0x04);
			state = 0;
		} else if (state == 2) {
			fc8350_write(handle, devid, 0x9c, cal[i]);
			fc8350_write(handle, devid, 0x66, 0x06);
			msWait(1);
			fc8350_write(handle, devid, 0x66, 0x04);
			state = 0;
		} else if (state == 3) {
			fc8350_write(handle, devid, 0x9b, cal[i]);
			fc8350_write(handle, devid, 0x9c, cal[i]);
			fc8350_write(handle, devid, 0x66, 0x06);
			msWait(1);
			fc8350_write(handle, devid, 0x66, 0x04);
			state = 0;
		} else {
			return;
		}

		fc8350_write(handle, devid, 0xFD, i);
	}

	if (state == 1) {
		fc8350_read(handle, devid, 0xd7, &rf_fast);
		fc8350_write(handle, devid, 0xa5, rf_fast);
	} else if (state == 2) {
		fc8350_read(handle, devid, 0xd9, &mix_fast);
		fc8350_write(handle, devid, 0xa6, mix_fast);
	} else if (state == 3) {
		fc8350_read(handle, devid, 0xd7, &rf_fast);
		fc8350_write(handle, devid, 0xa5, rf_fast);
		fc8350_read(handle, devid, 0xd9, &mix_fast);
		fc8350_write(handle, devid, 0xa6, mix_fast);
	}
	fc8350_write(handle, devid, 0xca, 0x03);
}

static void fc8350_set_filter(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	u32 div_cal = 0;
	u16 div = 0;

	div_cal = (875000 / filter_bw) * FC8350_XTAL_FREQ / 10000;

	if ((div_cal % 10) >= 5)
		div = (u16) (div_cal / 10) + 1;
	else
		div = (u16) (div_cal / 10);

	fc8350_write(handle, devid, 0x6a, ((div >> 2) & 0x80));
	fc8350_write(handle, devid, 0x6b, ((div >> 1) & 0xff));
	fc8350_write(handle, devid, 0x6c, (((div & 0x01) << 7) + 0x37));

	msWait(5);

	fc8350_write(handle, devid, 0x66, 0x04);

	fc8350_pd_man(handle, devid);
}

static void fc8350_1seg_init(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	fc8350_write(handle, devid, 0x02, 0x01);

#ifdef NOSAW
	fc8350_write(handle, devid, 0x09, 0x02);
#else
	fc8350_write(handle, devid, 0x09, 0x08);
#endif

	fc8350_write(handle, devid, 0x12, 0x00);
	fc8350_write(handle, devid, 0x14, 0x04);
	fc8350_write(handle, devid, 0x1b, 0x04);
	fc8350_write(handle, devid, 0x1d, 0x04);
	fc8350_write(handle, devid, 0x1f, 0x04);

	fc8350_write(handle, devid, 0x2a, 0x11);
	fc8350_write(handle, devid, 0x2b, 0x21);
	fc8350_write(handle, devid, 0x6e, 0x66);
	fc8350_write(handle, devid, 0x73, 0x66);
	fc8350_write(handle, devid, 0x7a, 0x55);
	fc8350_write(handle, devid, 0xc4, 0x19);

	fc8350_write(handle, devid, 0xbc, 0x04);
	fc8350_write(handle, devid, 0xc5, 0x00);

	fc8350_set_filter(handle, devid, filter_bw);
}

static void fc8350_tsb_1seg_init(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	fc8350_write(handle, devid, 0x02, 0x01);
	fc8350_write(handle, devid, 0x09, 0x02);

	fc8350_write(handle, devid, 0x12, 0x00);
	fc8350_write(handle, devid, 0x14, 0x04);
	fc8350_write(handle, devid, 0x1b, 0x04);
	fc8350_write(handle, devid, 0x1d, 0x04);
	fc8350_write(handle, devid, 0x1f, 0x04);

	fc8350_write(handle, devid, 0x2a, 0x11);
	fc8350_write(handle, devid, 0x2b, 0x21);
	fc8350_write(handle, devid, 0x6e, 0x66);
	fc8350_write(handle, devid, 0x73, 0x66);
	fc8350_write(handle, devid, 0x7a, 0x55);
	fc8350_write(handle, devid, 0xc4, 0x1a);

	fc8350_write(handle, devid, 0xbc, 0x04);
	fc8350_write(handle, devid, 0xc5, 0x00);

	fc8350_set_filter(handle, devid, filter_bw);
}

static void fc8350_tsb_3seg_init(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	fc8350_write(handle, devid, 0x02, 0x01);
	fc8350_write(handle, devid, 0x09, 0x02);

	fc8350_write(handle, devid, 0x12, 0x00);
	fc8350_write(handle, devid, 0x14, 0x04);
	fc8350_write(handle, devid, 0x1b, 0x04);
	fc8350_write(handle, devid, 0x1d, 0x04);
	fc8350_write(handle, devid, 0x1f, 0x04);

	fc8350_write(handle, devid, 0x2a, 0x11);
	fc8350_write(handle, devid, 0x2b, 0x21);
	fc8350_write(handle, devid, 0x6e, 0x66);
	fc8350_write(handle, devid, 0x73, 0x77);
	fc8350_write(handle, devid, 0x7a, 0x55);
	fc8350_write(handle, devid, 0xc4, 0x1a);

	fc8350_write(handle, devid, 0xbc, 0x04);
	fc8350_write(handle, devid, 0xc5, 0x00);

	fc8350_set_filter(handle, devid, filter_bw);
}

static void fc8350_13seg_init(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	fc8350_write(handle, devid, 0x02, 0x01);

#ifdef NOSAW
	fc8350_write(handle, devid, 0x09, 0x02);
#else
	fc8350_write(handle, devid, 0x09, 0x08);
#endif

	fc8350_write(handle, devid, 0x12, 0x00);
	fc8350_write(handle, devid, 0x14, 0x04);
	fc8350_write(handle, devid, 0x1b, 0x04);
	fc8350_write(handle, devid, 0x1d, 0x04);
	fc8350_write(handle, devid, 0x1f, 0x04);

	fc8350_write(handle, devid, 0x2a, 0x11);
	fc8350_write(handle, devid, 0x2b, 0x21);
	fc8350_write(handle, devid, 0x6e, 0x66);
	fc8350_write(handle, devid, 0x73, 0x77);
	fc8350_write(handle, devid, 0x7a, 0x55);
	fc8350_write(handle, devid, 0xc4, 0x19);

	fc8350_write(handle, devid, 0xbc, 0x04);
	fc8350_write(handle, devid, 0xc5, 0x00);

	fc8350_set_filter(handle, devid, filter_bw);
}

static void fc8350_catv_init(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	fc8350_write(handle, devid, 0x02, 0x05);
	fc8350_write(handle, devid, 0x09, 0x02);

	fc8350_write(handle, devid, 0x12, 0x04);
	fc8350_write(handle, devid, 0x14, 0x0f);
	fc8350_write(handle, devid, 0x1b, 0x0f);
	fc8350_write(handle, devid, 0x1d, 0x0f);
	fc8350_write(handle, devid, 0x1f, 0x0f);

	fc8350_write(handle, devid, 0x2a, 0x33);
	fc8350_write(handle, devid, 0x2b, 0x33);
	fc8350_write(handle, devid, 0x6E, 0x66);
	fc8350_write(handle, devid, 0xc4, 0x16);
	fc8350_write(handle, devid, 0x73, 0x88);
	fc8350_write(handle, devid, 0x7a, 0xaa);

	fc8350_write(handle, devid, 0xbc, 0x04);
	fc8350_write(handle, devid, 0xc5, 0x04);

	fc8350_set_filter(handle, devid, filter_bw);
}

static void fc8350_catv_vhf_init(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	fc8350_write(handle, devid, 0x02, 0x05);
	fc8350_write(handle, devid, 0x09, 0x02);

	fc8350_write(handle, devid, 0x12, 0x04);
	fc8350_write(handle, devid, 0x14, 0x0f);
	fc8350_write(handle, devid, 0x1b, 0x0f);
	fc8350_write(handle, devid, 0x1d, 0x0f);
	fc8350_write(handle, devid, 0x1f, 0x0f);

	fc8350_write(handle, devid, 0x2a, 0x33);
	fc8350_write(handle, devid, 0x2b, 0x33);
	fc8350_write(handle, devid, 0x6E, 0x66);
	fc8350_write(handle, devid, 0x73, 0x88);
	fc8350_write(handle, devid, 0x7a, 0xaa);

	fc8350_write(handle, devid, 0xc4, 0x16);

	fc8350_write(handle, devid, 0xbc, 0x04);
	fc8350_write(handle, devid, 0xc5, 0x04);

	fc8350_set_filter(handle, devid, filter_bw);
}

static void fc8350_dtv_init(HANDLE handle, DEVICEID devid, u16 filter_bw)
{
	fc8350_write(handle, devid, 0x02, 0x01);
	fc8350_write(handle, devid, 0x09, 0x02);

	fc8350_write(handle, devid, 0x12, 0x00);
	fc8350_write(handle, devid, 0x1b, 0x0f);
	fc8350_write(handle, devid, 0x1d, 0x0f);
	fc8350_write(handle, devid, 0x1f, 0x0f);

	fc8350_write(handle, devid, 0x2a, 0x33);
	fc8350_write(handle, devid, 0x2b, 0x33);
	fc8350_write(handle, devid, 0x6E, 0x66);
	fc8350_write(handle, devid, 0x73, 0x88);
	fc8350_write(handle, devid, 0xc4, 0x18);

	fc8350_write(handle, devid, 0xbc, 0x04);
	fc8350_write(handle, devid, 0xc5, 0x00);

	fc8350_set_filter(handle, devid, filter_bw);
}

static s32 fc8350_tuner_set_pll(HANDLE handle, DEVICEID devid, u32 freq)
{
	s16 div_index;
	u8 lo_bps;
	u32 f_diff, f_diff_shifted, n_val, k_val, f_vco;
	u8 data_0x31, data_0x32;
	u8 lo_sel;
	u8 lna_cap;

	u8 f = 0;

	u32 bank_table[128] = {
		6440000, 6400000, 6320000, 6240000, 6160000, 6080000, 6000000,
		5920000, 5840000, 5760000, 5720000, 5680000, 5600000, 5520000,
		5480000, 5440000, 5400000, 5360000, 5280000, 5240000, 5200000,
		5120000, 5080000, 5040000, 5000000, 4960000, 4920000, 4880000,
		4840000, 4800000, 4760000, 4740000, 4720000, 4680000, 4660000,
		4640000, 4600000, 4560000, 4520000, 4480000, 4460000, 4440000,
		4400000, 4360000, 4340000, 4340000, 4320000, 4280000, 4260000,
		4240000, 4220000, 4200000, 4160000, 4120000, 4100000, 4080000,
		4060000, 4040000, 4020000, 4020000, 4020000, 4020000, 4020000,
		4020000, 4000000, 3980000, 3960000, 3920000, 3900000, 3880000,
		3860000, 3840000, 3820000, 3800000, 3784000, 3400000, 3400000,
		3400000, 3400000, 3400000, 3400000, 3400000, 3400000, 3400000,
		3400000, 3400000, 3400000, 3400000, 3400000, 3400000, 3400000,
		3400000, 3400000, 3400000, 3400000, 3400000, 3400000, 3400000,
		3400000, 3400000, 3400000, 3400000, 3400000, 3400000, 3400000,
		3400000, 3400000, 3400000, 3400000, 3400000, 3400000, 3400000,
		3400000, 3400000, 3400000, 3400000, 3400000, 3400000, 3400000,
		3400000, 3400000, 3400000, 3400000, 3400000, 3400000, 3400000,
		3400000, 3400000
	};

	/* PLL Parameter Setting */
	u8 ref_div = 1;
	u8 send_dc = 0;
	u8 mixlo_div2 = 1;
	u8 mixlo_ret = 0;

	u8 pre_shift_bits = 4;
	u32 f_vco_max = 6000000;

	u8 lo_divide_ratio[8] = {8, 12, 16, 24, 32, 48, 64, 96};

	u8 lo_sel_con[2][8] = {
		{0, 1, 2, 3, 4, 5, 6, 7},
		{0, 1, 0, 1, 2, 3, 4, 5}
	};

	u8 i;

#ifdef NOSAW
	for (i = 1; i < 115; i++) {
		if (ch_mode_nosaw[i + 1][0] >= freq)
			break;
	}

	lna_cap = ch_mode_nosaw[i][2];
	fc8350_write(handle, devid, ch_mode_nosaw[0][3], ch_mode_nosaw[i][3]);
	fc8350_write(handle, devid, ch_mode_nosaw[0][4], ch_mode_nosaw[i][4]);
	fc8350_write(handle, devid, ch_mode_nosaw[0][5], ch_mode_nosaw[i][5]);
	fc8350_write(handle, devid, ch_mode_nosaw[0][6], ch_mode_nosaw[i][6]);
	fc8350_write(handle, devid, ch_mode_nosaw[0][7], ch_mode_nosaw[i][7]);
	fc8350_write(handle, devid, ch_mode_nosaw[0][8], ch_mode_nosaw[i][8]);
	fc8350_write(handle, devid, ch_mode_nosaw[0][9], ch_mode_nosaw[i][9]);
	fc8350_write(handle, devid, ch_mode_nosaw[0][10], ch_mode_nosaw[i][10]);
#else
	for (i = 1; i < 115; i++) {
		if (ch_mode_saw[i + 1][0] >= freq)
			break;
	}

	lna_cap = ch_mode_saw[i][2];
	fc8350_write(handle, devid, ch_mode_saw[0][3], ch_mode_saw[i][3]);
	fc8350_write(handle, devid, ch_mode_saw[0][4], ch_mode_saw[i][4]);
	fc8350_write(handle, devid, ch_mode_saw[0][5], ch_mode_saw[i][5]);
	fc8350_write(handle, devid, ch_mode_saw[0][6], ch_mode_saw[i][6]);
	fc8350_write(handle, devid, ch_mode_saw[0][7], ch_mode_saw[i][7]);
	fc8350_write(handle, devid, ch_mode_saw[0][8], ch_mode_saw[i][8]);
	fc8350_write(handle, devid, ch_mode_saw[0][9], ch_mode_saw[i][9]);
	fc8350_write(handle, devid, ch_mode_saw[0][10], ch_mode_saw[i][10]);
#endif

	if (freq > 226000)
		lna_cap = lna_cap + 4;

	if ((freq < 226000) && (broadcast_type == ISDBT_CATV_VHF_13SEG)) {
		fc8350_write(handle, devid, 0x02, 0x03);
		fc8350_write(handle, devid, 0x12, lna_cap);
		fc8350_write(handle, devid, 0x1b, 0x04);
		fc8350_write(handle, devid, 0x1d, 0x04);
		fc8350_write(handle, devid, 0x1f, 0x04);
		fc8350_write(handle, devid, 0xc4, 0x1a);
	} else if ((broadcast_type == ISDBT_CATV_VHF_13SEG) ||
				(broadcast_type == ISDBT_CATV_13SEG)) {
		fc8350_write(handle, devid, 0x02, 0x05);
		fc8350_write(handle, devid, 0x12, lna_cap);
		fc8350_write(handle, devid, 0x1b, 0x0f);
		fc8350_write(handle, devid, 0x1d, 0x0f);
		fc8350_write(handle, devid, 0x1f, 0x0f);
		fc8350_write(handle, devid, 0xc4, 0x16);
	}

	lo_sel = mixlo_div2;

	if (freq >= 470000) {
		f_vco = freq * lo_divide_ratio[0];
		div_index = 0;
	} else {
		for (div_index = 7; div_index >= 0; div_index = div_index - 1) {
			f_vco = freq * lo_divide_ratio[div_index];

			if (f_vco_max > f_vco) {
				break;
			} else if (div_index > 128) {
				div_index = 0;
				f_vco = freq * lo_divide_ratio[div_index];
				break;
			}
		}
	}

	for (f = 0; f < 128; f++) {
		if (bank_table[f] < f_vco)
			break;
	}

	fc8350_write(handle, devid, 0x89, f);

	if (div_index == 0)
		lo_bps = mixlo_div2;
	else
		lo_bps = 0;

	if (div_index == 1) {
		mixlo_div2 = 0;
		mixlo_ret = 0;
	}

	n_val =	f_vco / FC8350_XTAL_FREQ / ref_div;
	f_diff = f_vco - FC8350_XTAL_FREQ * n_val;

	f_diff_shifted = f_diff << (20 - pre_shift_bits);

	k_val = (f_diff_shifted / (FC8350_XTAL_FREQ >>
				(pre_shift_bits - 1))) << 1;
	k_val = k_val | 1;

	data_0x31 = ((lo_bps << 6) | (mixlo_div2 << 5)
		| (mixlo_ret << 4)) + send_dc;
	data_0x32 = (lo_sel_con[lo_sel][div_index] << 5) + ((n_val >> 4) & 0x10)
		+ ((k_val >> 16) & 0x0f);

	fc8350_write(handle, devid, 0x31, data_0x31);
	fc8350_write(handle, devid, 0x32, data_0x32);
	fc8350_write(handle, devid, 0x33, (u8) ((k_val >> 8) & 0xff));
	fc8350_write(handle, devid, 0x34, (u8) (k_val & 0xff));
	fc8350_write(handle, devid, 0x35, (u8) (n_val & 0xff));

	if (freq < 200000)
		fc8350_write(handle, devid, 0x25, 0x03);
	else
		fc8350_write(handle, devid, 0x25, 0x13);

	return BBM_OK;
}

s32 fc8350_tuner_init(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast)
{
	u16 filter_bw = 0;

	broadcast_type = broadcast;

	fc8350_write(handle, devid, 0x00, 0x00);
	fc8350_write(handle, devid, 0x03, 0x04);

	fc8350_write(handle, devid, 0x0D, 0x81);
	fc8350_write(handle, devid, 0x18, 0x44);

	fc8350_write(handle, devid, 0x25, 0x13);
	fc8350_write(handle, devid, 0x31, 0x60);

	/*AGC Control*/
	fc8350_write(handle, devid, 0x38, 0x64);
	fc8350_write(handle, devid, 0x39, 0x3C);
	fc8350_write(handle, devid, 0x3a, 0xf2);
	fc8350_write(handle, devid, 0x3b, 0x64);
	fc8350_write(handle, devid, 0x3c, 0x46);
	fc8350_write(handle, devid, 0x3d, 0xf2);

	fc8350_write(handle, devid, 0x42, 0x11);
	fc8350_write(handle, devid, 0x43, 0x11);
	fc8350_write(handle, devid, 0x44, 0x11);
	fc8350_write(handle, devid, 0x45, 0x11);
	fc8350_write(handle, devid, 0x46, 0xc8);
	fc8350_write(handle, devid, 0x47, 0x14);
	fc8350_write(handle, devid, 0x48, 0x50);
	fc8350_write(handle, devid, 0x49, 0x1e);
	fc8350_write(handle, devid, 0x4a, 0xc8);
	fc8350_write(handle, devid, 0x4b, 0x14);
	fc8350_write(handle, devid, 0x4c, 0x50);
	fc8350_write(handle, devid, 0x4d, 0x28);
	fc8350_write(handle, devid, 0x4e, 0x78);
	fc8350_write(handle, devid, 0x4f, 0x14);
	fc8350_write(handle, devid, 0x50, 0x46);
	fc8350_write(handle, devid, 0x51, 0x1e);
	fc8350_write(handle, devid, 0x52, 0x64);
	fc8350_write(handle, devid, 0x53, 0x14);
	fc8350_write(handle, devid, 0x54, 0x3c);
	fc8350_write(handle, devid, 0x55, 0x28);
	fc8350_write(handle, devid, 0x56, 0xc8);
	fc8350_write(handle, devid, 0x57, 0x14);
	fc8350_write(handle, devid, 0x58, 0x50);
	fc8350_write(handle, devid, 0x59, 0x14);
	fc8350_write(handle, devid, 0x5a, 0xc8);
	fc8350_write(handle, devid, 0x5b, 0x0f);
	fc8350_write(handle, devid, 0x5c, 0x50);
	fc8350_write(handle, devid, 0x5d, 0x14);
	fc8350_write(handle, devid, 0x5e, 0x64);
	fc8350_write(handle, devid, 0x5f, 0x0f);
	fc8350_write(handle, devid, 0x60, 0x3c);
	fc8350_write(handle, devid, 0x61, 0x14);
	fc8350_write(handle, devid, 0x62, 0x64);
	fc8350_write(handle, devid, 0x63, 0x0f);
	fc8350_write(handle, devid, 0x64, 0x3c);
	fc8350_write(handle, devid, 0x65, 0x14);

	fc8350_write(handle, devid, 0x66, 0x00);

	fc8350_write(handle, devid, 0x66, 0x07);

	fc8350_write(handle, devid, 0x142, 0x1f);
	fc8350_write(handle, devid, 0x143, 0x1f);
	fc8350_write(handle, devid, 0x144, 0x03);
	fc8350_write(handle, devid, 0x145, 0x03);
	fc8350_write(handle, devid, 0x146, 0x1f);
	fc8350_write(handle, devid, 0x147, 0x1f);
	fc8350_write(handle, devid, 0x148, 0x03);
	fc8350_write(handle, devid, 0x149, 0x03);
	fc8350_write(handle, devid, 0x150, 0x80);

	fc8350_write(handle, devid, 0x71, 0x30);
	fc8350_write(handle, devid, 0x72, 0x6b);
	fc8350_write(handle, devid, 0x73, 0x88);
	fc8350_write(handle, devid, 0x74, 0x6e);
	fc8350_write(handle, devid, 0x75, 0x77);
	fc8350_write(handle, devid, 0x76, 0x00);
	fc8350_write(handle, devid, 0x77, 0x00);
	fc8350_write(handle, devid, 0x78, 0xff);
	fc8350_write(handle, devid, 0x79, 0xff);

	fc8350_write(handle, devid, 0x7b, 0x15);
	fc8350_write(handle, devid, 0x7c, 0x13);

	fc8350_write(handle, devid, 0x7e, 0x1f);
	fc8350_write(handle, devid, 0x7d, 0x3f);

	fc8350_write(handle, devid, 0x7f, 0xf1);
	fc8350_write(handle, devid, 0x80, 0x67);
	fc8350_write(handle, devid, 0x82, 0x17);

	if (FC8350_XTAL_FREQ >= 32000)
		fc8350_write(handle, devid, 0x84, 0x1f);
	else
		fc8350_write(handle, devid, 0x84, (FC8350_XTAL_FREQ / 1000));

	fc8350_write(handle, devid, 0x86, 0x84);
	fc8350_write(handle, devid, 0x87, 0x0c);
	fc8350_write(handle, devid, 0x81, 0x00);
	fc8350_write(handle, devid, 0x81, 0x80);

	fc8350_write(handle, devid, 0x8a, 0xb0);
	fc8350_write(handle, devid, 0x8b, 0x00);
	fc8350_write(handle, devid, 0x8c, 0x00);
	fc8350_write(handle, devid, 0x8e, 0x39);

	fc8350_write(handle, devid, 0x91, 0x78);
	fc8350_write(handle, devid, 0x92, 0xff);

	fc8350_write(handle, devid, 0x94, 0x30);
	fc8350_write(handle, devid, 0x95, 0x30);

	/* W/D Factor */
	fc8350_write(handle, devid, 0x96, 0x05);
	fc8350_write(handle, devid, 0x98, 0xa1);
	fc8350_write(handle, devid, 0x9a, 0x17);
	fc8350_write(handle, devid, 0x9b, 0x46);
	fc8350_write(handle, devid, 0x9c, 0x46);
	fc8350_write(handle, devid, 0x9d, 0x3f);
	fc8350_write(handle, devid, 0x9e, 0x3f);

	fc8350_write(handle, devid, 0xa3, 0x00);

	fc8350_write(handle, devid, 0xac, 0x0b);
	fc8350_write(handle, devid, 0xad, 0x1b);
	fc8350_write(handle, devid, 0xae, 0x5b);
	fc8350_write(handle, devid, 0xaf, 0x9b);
	fc8350_write(handle, devid, 0xb0, 0xdb);
	fc8350_write(handle, devid, 0xb1, 0xeb);
	fc8350_write(handle, devid, 0xb2, 0xfb);
	fc8350_write(handle, devid, 0xb5, 0x8f);
	fc8350_write(handle, devid, 0xb6, 0xb6);
	fc8350_write(handle, devid, 0xb7, 0xb0);
	fc8350_write(handle, devid, 0xb8, 0x02);
	fc8350_write(handle, devid, 0xb9, 0x00);
	fc8350_write(handle, devid, 0xba, 0xb5);
	fc8350_write(handle, devid, 0xbb, 0xb0);

	fc8350_write(handle, devid, 0xbd, 0x80);
	fc8350_write(handle, devid, 0xbe, 0x07);
	fc8350_write(handle, devid, 0xbf, 0x12);
	fc8350_write(handle, devid, 0xc0, 0x04);
	fc8350_write(handle, devid, 0xc1, 0xc3);
	fc8350_write(handle, devid, 0xc2, 0xe1);

	fc8350_write(handle, devid, 0xc5, 0x00);

	fc8350_write(handle, devid, 0xca, 0x00);

	fc8350_write(handle, devid, 0xcb, 0x17);
	fc8350_write(handle, devid, 0xcc, 0x97);

	fc8350_write(handle, devid, 0x90, 0x12);

	fc8350_write(handle, devid, 0xce, 0xf0);

	fc8350_write(handle, devid, 0xf2, 0x00);
	fc8350_write(handle, devid, 0xf3, 0x0f);
	fc8350_write(handle, devid, 0xf5, 0x84);

	fc8350_write(handle, devid, 0x119, 0x16);
	fc8350_write(handle, devid, 0x11a, 0x0A);
	fc8350_write(handle, devid, 0x11b, 0x00);
	fc8350_write(handle, devid, 0x11c, 0x03);
	fc8350_write(handle, devid, 0x133, 0x29);

	switch (broadcast_type) {
	case ISDBT_1SEG:
		filter_bw = 780;
		fc8350_1seg_init(handle, devid, filter_bw);
		break;
	case ISDBTSB_1SEG:
		filter_bw = 780;
		fc8350_tsb_1seg_init(handle, devid, filter_bw);
		break;
	case ISDBTSB_3SEG:
		filter_bw = 1600;
		fc8350_tsb_3seg_init(handle, devid, filter_bw);
		break;
	case ISDBT_13SEG:
		if (FC8350_BAND_WIDTH == 6)
			filter_bw = 2800;
		else if (FC8350_BAND_WIDTH == 7)
			filter_bw = 3200;
		else if (FC8350_BAND_WIDTH == 8)
			filter_bw = 3800;
		else
			filter_bw = 2800;

		fc8350_13seg_init(handle, devid, filter_bw);
		break;
	case ISDBT_CATV_13SEG:
		if (FC8350_BAND_WIDTH == 6)
			filter_bw = 2800;
		else if (FC8350_BAND_WIDTH == 7)
			filter_bw = 3200;
		else if (FC8350_BAND_WIDTH == 8)
			filter_bw = 3800;
		else
			filter_bw = 2800;

		fc8350_catv_init(handle, devid, filter_bw);
		break;
	case ISDBT_CATV_VHF_13SEG:
		if (FC8350_BAND_WIDTH == 6)
			filter_bw = 2800;
		else if (FC8350_BAND_WIDTH == 7)
			filter_bw = 3200;
		else if (FC8350_BAND_WIDTH == 8)
			filter_bw = 3800;
		else
			filter_bw = 2800;

		fc8350_catv_vhf_init(handle, devid, filter_bw);
		break;
	case DVB_T:
		if (FC8350_BAND_WIDTH_DVB == 6)
			filter_bw = 2800;
		else if (FC8350_BAND_WIDTH_DVB == 7)
			filter_bw = 3200;
		else if (FC8350_BAND_WIDTH_DVB == 8)
			filter_bw = 3800;
		else
			filter_bw = 2800;

		fc8350_dtv_init(handle, devid, filter_bw);
		break;
	default:
		break;
	}

	/* Driver Version */
	fc8350_write(handle, devid, 0xfe, DRIVER_VERSION);

	return BBM_OK;
}

s32 fc8350_set_freq(HANDLE handle, DEVICEID devid, u32 freq)
{
	fc8350_tuner_set_pll(handle, devid, freq);

	fc8350_write(handle, devid, 0x7d, 0x2f);
	fc8350_write(handle, devid, 0x7e, 0x15);
	fc8350_write(handle, devid, 0x7e, 0x1f);
	fc8350_write(handle, devid, 0x7d, 0x3f);

	return BBM_OK;
}

s32 fc8350_get_rssi(HANDLE handle, DEVICEID devid, s32 *rssi)
{
	s8 tmp = 0;

	fc8350_read(handle, devid, 0xf8, (u8 *) &tmp);
	*rssi = tmp;

	if (tmp < 0)
		*rssi += 1;

	return BBM_OK;
}

s32 fc8350_tuner_deinit(HANDLE handle, DEVICEID devid)
{
	return BBM_OK;
}

