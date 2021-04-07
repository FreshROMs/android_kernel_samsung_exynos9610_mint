/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350_bb.c
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
#include <linux/delay.h>
#include "fci_types.h"
#include "fci_oal.h"
#include "fci_hal.h"
#include "fci_tun.h"
#include "fc8350_regs.h"

#define SCAN_CHK_PERIOD 1 /* 1 ms */

static enum BROADCAST_TYPE broadcast_type;

static u32 fc8350_get_current_clk(HANDLE handle, DEVICEID devid)
{
	u32 pre_sel, post_sel, multi;
	u16 pll_set = 0;

	bbm_word_read(handle, devid, BBM_PLL1_PRE_POST_SELECTION, &pll_set);

	multi = ((pll_set & 0xff00) >> 8);
	pre_sel = pll_set & 0x000f;
	post_sel = (pll_set & 0x00f0) >> 4;

	return ((BBM_XTAL_FREQ >> pre_sel) * multi) >> post_sel;
}

static u32 fc8350_get_core_16000(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 100000;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 100000;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104000;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112000;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104000;
			break;
		case 551143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112000;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		case 521143:
			clk = 124000;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 132000;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 112000;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 116000;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 132000;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 100000;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 100000;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 112000;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_16384(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 98304;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 98304;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 102400;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 106496;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110592;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 118784;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 102400;
			break;
		case 551143:
		case 683143:
			clk = 106496;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110592;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 118784;
			break;
		case 521143:
			clk = 122880;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 131072;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 110592;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 114688;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 131072;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 98304;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 98304;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 110592;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_18000(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 99000;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 99000;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 103500;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112500;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 121500;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 103500;
			break;
		case 551143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112500;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 121500;
			break;
		case 521143:
			clk = 126000;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 130500;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 112500;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 117000;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 130500;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 99000;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 99000;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 112500;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_19200(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 100800;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 100800;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105600;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 110400;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110400;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105600;
			break;
		case 551143:
		case 683143:
			clk = 110400;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110400;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		case 521143:
			clk = 124800;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 134400;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 110400;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 115200;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 134400;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 100800;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 100800;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 110400;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_24000(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 99000;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 99000;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105000;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111000;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105000;
			break;
		case 551143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111000;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		case 521143:
			clk = 126000;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 132000;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 111000;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 114000;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 132000;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 99000;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 99000;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 111000;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_24576(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 98304;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 98304;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104448;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 107520;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110592;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 119808;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104448;
			break;
		case 551143:
		case 683143:
			clk = 107520;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110592;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 119808;
			break;
		case 521143:
			clk = 122880;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 129024;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 110592;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 116736;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 135168;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 98304;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 98304;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 110592;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_26000(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 100750;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 100750;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104000;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 107250;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 113750;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120250;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104000;
			break;
		case 551143:
		case 683143:
			clk = 107250;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 113750;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120250;
			break;
		case 521143:
			clk = 123500;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 130000;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 113750;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 117000;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 136500;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 100750;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 100750;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 110500;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_27000(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 101250;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 101250;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104625;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111375;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 121500;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104625;
			break;
		case 551143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111375;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 121500;
			break;
		case 521143:
			clk = 124875;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 131625;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 111375;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 114750;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 131625;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 97875;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 97875;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 111375;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_27120(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 98310;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 98310;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105090;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 108480;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111870;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 118650;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105090;
			break;
		case 551143:
		case 683143:
			clk = 108480;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111870;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 118650;
			break;
		case 521143:
			clk = 125430;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 132210;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 111870;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 115260;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 132210;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 98310;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 98310;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 111870;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_32000(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 100000;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 92000;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104000;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112000;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 104000;
			break;
		case 551143:
		case 683143:
			clk = 108000;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112000;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		case 521143:
			clk = 124000;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 132000;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 112000;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 116000;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 132000;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 100000;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 100000;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 112000;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_37200(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 97650;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 97650;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 102300;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 106950;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111600;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120900;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 102300;
			break;
		case 551143:
		case 683143:
			clk = 106950;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 111600;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120900;
			break;
		case 521143:
			clk = 125550;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 130200;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 111600;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 116250;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 130200;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 97650;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 97650;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 111600;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_37400(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 98175;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 98175;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 102850;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 107525;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112200;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 121550;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 102850;
			break;
		case 551143:
		case 683143:
			clk = 107525;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 112200;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 121550;
			break;
		case 521143:
			clk = 126225;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 130900;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 112200;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 116875;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 130900;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 98175;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 98175;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 112200;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_38400(enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

#if (BBM_BAND_WIDTH == 6) /* ISDB-T 6M TARGET: 97.523 */
	clk = 100800;

	switch (broadcast) {
	case ISDBT_1SEG:
		switch (freq) {
		case 485143:
		case 623143:
			clk = 100800;
			break;
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105600;
			break;
		case 551143:
		case 617143:
		case 671143:
		case 683143:
			clk = 110400;
			break;
		case 473143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110400;
			break;
		case 521143:
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		default:
			break;
		}
		break;
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		switch (freq) {
		case 491143:
		case 497143:
		case 503143:
		case 509143:
		case 653143:
			clk = 105600;
			break;
		case 551143:
		case 683143:
			clk = 110400;
			break;
		case 473143:
		case 485143:
		case 533143:
		case 539143:
		case 545143:
		case 581143:
		case 593143:
		case 599143:
		case 641143:
		case 647143:
		case 695143:
		case 701143:
			clk = 110400;
			break;
		case 575143:
		case 629143:
		case 635143:
		case 689143:
		case 707143:
			clk = 120000;
			break;
		case 521143:
			clk = 124800;
			break;
		case 617143:
		case 623143:
		case 671143:
			clk = 134400;
			break;
		default:
			break;
		}
		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (freq) {
		case 99143:
		case 201143:
		case 249143:
		case 297143:
		case 351143:
		case 399143:
		case 417143:
		case 497143:
		case 551143:
		case 599143:
		case 647143:
		case 701143:
		case 749143:
			clk = 110400;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#elif (BBM_BAND_WIDTH == 7) /* ISDB-T 7M TARGET: 113.777 */
	clk = 115200;
#else /* (BBM_BAND_WIDTH == 8) */ /* ISDB-T 8M TARGET: 130.031 */
	clk = 134400;
#endif /* #if (BBM_BAND_WIDTH == 6) */

	if (broadcast == DVB_T) {
#if (BBM_BAND_WIDTH_DVB == 6) /* DVB 6M TARGET: 82.29M */
		clk = 100800;
#elif (BBM_BAND_WIDTH_DVB == 7) /* DVB 7M TARGET: 96M */
		clk = 100800;
#elif (BBM_BAND_WIDTH_DVB == 8) /* DVB 8M TARGET: 109.71M */
		clk = 110400;
#endif /* #if (BBM_BAND_WIDTH_DVB == 6) */
	}

	return clk;
}

static u32 fc8350_get_core_clk(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 clk;

	if (BBM_XTAL_FREQ == 16000)
		clk = fc8350_get_core_16000(broadcast, freq);
	else if (BBM_XTAL_FREQ == 16384)
		clk = fc8350_get_core_16384(broadcast, freq);
	else if (BBM_XTAL_FREQ == 18000)
		clk = fc8350_get_core_18000(broadcast, freq);
	else if (BBM_XTAL_FREQ == 19200)
		clk = fc8350_get_core_19200(broadcast, freq);
	else if (BBM_XTAL_FREQ == 24000)
		clk = fc8350_get_core_24000(broadcast, freq);
	else if (BBM_XTAL_FREQ == 24576)
		clk = fc8350_get_core_24576(broadcast, freq);
	else if (BBM_XTAL_FREQ == 26000)
		clk = fc8350_get_core_26000(broadcast, freq);
	else if (BBM_XTAL_FREQ == 27000)
		clk = fc8350_get_core_27000(broadcast, freq);
	else if (BBM_XTAL_FREQ == 27120)
		clk = fc8350_get_core_27120(broadcast, freq);
	else if (BBM_XTAL_FREQ == 32000)
		clk = fc8350_get_core_32000(broadcast, freq);
	else if (BBM_XTAL_FREQ == 37200)
		clk = fc8350_get_core_37200(broadcast, freq);
	else if (BBM_XTAL_FREQ == 37400)
		clk = fc8350_get_core_37400(broadcast, freq);
	else if (BBM_XTAL_FREQ == 38400)
		clk = fc8350_get_core_38400(broadcast, freq);
	else
		clk = fc8350_get_core_19200(broadcast, freq);

	return clk;
}

static s32 fc8350_set_acif_b31_1seg(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH == 6)
	switch (clk) {
	case 92000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xff000000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x050300fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf5f4fb03);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4e422507);
		break;
	case 96000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xff000000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x040301ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf8f4f901);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4a40260a);
		break;
	case 97650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x030301ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf9f4f9ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x483e260b);
		break;
	case 98175:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03030100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x483e260c);
		break;
	case 98310:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03030200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473e260c);
		break;
	case 99000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473e270d);
		break;
	case 100000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfbf5f8fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x463d270d);
		break;
	case 100750:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf5f7fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x453c270e);
		break;
	case 100800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf5f7fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x453c270e);
		break;
	case 101700:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040201);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x443c270f);
		break;
	case 102000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040201);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x443c270f);
		break;
	case 102300:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040201);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x443c270f);
		break;
	case 102850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b270f);
		break;
	case 104000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b2710);
		break;
	case 105000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x423b2710);
		break;
	case 105090:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x423b2710);
		break;
	case 105600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f6fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x423a2710);
		break;
	case 106950:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 107250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 107520:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 107525:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 108000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 108480:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 110400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x00f7f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x40392712);
		break;
	case 111000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x00f7f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x40392712);
		break;
	case 112000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x01f7f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f392712);
		break;
	case 112200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x01f7f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f392712);
		break;
	case 113750:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xff030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x01f8f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f382713);
		break;
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xff020302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x02f8f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3e382713);
		break;
	case 111600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x00f7f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f392712);
		break;
	case 120000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0100ff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfe020302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x04f9f6f9);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3c362714);
		break;
	case 120250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0100ff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfe010302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x04f9f6f9);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3c362714);
		break;
	case 132000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x02010000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfe0002);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x0afff9f8);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x35312618);
		break;
	default:
		return BBM_NOK;
	}
#elif (BBM_BAND_WIDTH == 7)
	switch (clk) {
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473e260c);
		break;
	case 120000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b270f);
		break;
	case 124800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 129600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x00f7f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x40392712);
		break;
	case 114000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03030100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf9f4f9ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x483e260c);
		break;
	case 117000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfbf5f7fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x453d270e);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02030301);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf5f7fd);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b270f);
		  break;*/
	case 126000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 115260:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473e260c);
		break;
	case 118650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040201);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x443c270f);
		break;
	case 122040:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x423b2710);
		break;
	case 125430:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 116000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfbf5f8fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x463d270d);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02030301);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf5f7fd);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b270f);
		  break;*/
	case 124000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x423a2711);
		break;
	case 128000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x00f7f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x40392712);
		break;
	case 116250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfbf5f8fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x463d270d);
		break;
	case 120900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b2710);
		break;
	case 125550:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x00f7f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f392712);
		break;
	case 116875:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfbf5f8fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x463d270e);
		break;
	default:
		return BBM_NOK;
	}
#else /* BBM_BAND_WIDTH == 8) */
	switch (clk) {
	case 134400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf5f7fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x453c270e);
		break;
	case 139200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b2710);
		break;
	case 144000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 148800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x00f7f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f392712);
		break;
	case 132000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473e270d);
		break;
	case 138000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x433b2710);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030301);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		  break;*/
	case 150000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x01f7f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f392712);
		break;
	case 132210:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfbf4f8fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473d270d);
		break;
	case 135600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040201);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x443c270f);
		break;
	case 142380:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 149160:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030302);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x01f7f6fa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x3f392712);
		break;
		/*case 132000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03040200);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473e270d);
		  break;*/
	case 136000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040201);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfdf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x443c270f);
		break;
	case 140000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x423b2710);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030301);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		  break;*/
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x030301ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf9f4f9ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x483e260b);
		break;
	case 134850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02040200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf5f7fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x453c270e);
		break;
	case 139500:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfef6f7fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x423b2710);
		break;
	case 144150:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00ffff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00030301);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfff6f6fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x413a2711);
		break;
	case 130900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xffff0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03030100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf4f8ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x483e260c);
		break;
	default:
		return BBM_NOK;
	}
#endif /* #if (BBM_BAND_WIDTH == 6) */

	return BBM_OK;
}

static s32 fc8350_set_acif_1seg(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH == 6)
	switch (clk) {
	case 100750:
	case 100800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfeff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120b04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2624201a);
		break;
	case 105600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221f19);
		break;
	case 110400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x22211e19);
		break;
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfeff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fdfcfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130e0804);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x201f1d19);
		break;
	case 99000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2725211a);
		break;
	case 102000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130b0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2524201a);
		break;
	case 105000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24221f19);
		break;
	case 108000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		break;
	case 98310:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2725211a);
		break;
	case 101700:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130b05ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2524201a);
		break;
	case 105090:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24221f19);
		break;
	case 108480:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x22211e19);
		break;
	case 100000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfeff0101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120b04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2625201a);
		break;
	case 104000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24231f1a);
		break;
		/*case 108000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		  break;*/
	case 112000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfeff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffdfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x21201d19);
		break;
	case 97650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2726211a);
		break;
	case 102300:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130b0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2524201a);
		break;
	case 106950:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221e19);
		break;
	case 111600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfe00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x21201d19);
		break;
	case 98175:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2725211a);
		break;
	case 102850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x25231f1a);
		break;
	case 107250:
	case 107525:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221e19);
		break;
	case 112200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfeff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffdfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x21201d19);
		break;
	default:
		return BBM_NOK;
	}
#elif (BBM_BAND_WIDTH == 7)
	switch (clk) {
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2725211a);
		break;
	case 120000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x25231f1a);
		break;
	case 124800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221e19);
		break;
	case 129600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfe00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x22201d19);
		break;
	case 114000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2726211a);
		break;
	case 117000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfeff0101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120b04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2624201a);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfc);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0500);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x25231f1a);
		  break;*/
	case 126000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		break;
	case 115260:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2725211a);
		break;
	case 118650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130b05ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2524201a);
		break;
	case 122040:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24231f19);
		break;
	case 125430:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221e19);
		break;
	case 116000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2625201a);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfc);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0500);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x25231f1a);
		  break;*/
	case 124000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221f19);
		break;
	case 128000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x22211e19);
		break;
	case 116250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfeff0101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120b04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2625201a);
		break;
	case 120900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24231f1a);
		break;
	case 125550:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221e19);
		break;
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfe00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x21201d19);
		break;
	case 116875:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfeff0101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120b04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2624201a);
		break;
	default:
		return BBM_NOK;
	}
#else /* BBM_BAND_WIDTH == 8) */
	switch (clk) {
	case 134400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfeff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120b04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2624201a);
		break;
	case 139200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24231f19);
		break;
	case 144000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		break;
	case 148800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfe00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x21201d19);
		break;
	case 132000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2725211a);
		break;
	case 138000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24231f1a);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		  break;*/
	case 150000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfeff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffdfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130e0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x21201d19);
		break;
	case 132210:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a04fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2625211a);
		break;
	case 135600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130b05ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2524201a);
		break;
	case 142380:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23221e19);
		break;
	case 149160:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfdfeff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffdfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x21201d19);
		break;
		/*case 132000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2725211a);
		  break;*/
	case 136000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130b0500);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2524201a);
		break;
	case 140000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0000);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24221f19);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		  break;*/
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe000101);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfbfafbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120a03fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2726211a);
		break;
	case 134850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdff0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfcfbfbfc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x120b04ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x2524201a);
		break;
	case 139500:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfe0001);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfdfbfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130c0601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x24231f19);
		break;
	case 144150:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		break;
	case 130900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfeff00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefcfbfb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0x130d0702);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x23211e19);
		break;
	default:
		return BBM_NOK;
	}
#endif /* #if (BBM_BAND_WIDTH == 6) */

	return BBM_OK;
}

static s32 fc8350_set_acif_3seg(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH == 6)
	switch (clk) {
	case 100750:
	case 100800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefb0808);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x604b1cf6);
		break;
	case 105600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef80509);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5c491ffa);
		break;
	case 110400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x01020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fffcfe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff50208);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x584721fe);
		break;
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xff010200);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0701fdfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f3fe07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x53452302);
		break;
	case 99000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0807);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1bf5);
		break;
	case 102000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefa0708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5f4a1df7);
		break;
	case 105000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020100ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef80609);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ef9);
		break;
		/*case 108000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		  break;*/
	case 98310:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0907);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x624b1bf5);
		break;
	case 101700:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefa0708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5f4b1df7);
		break;
	case 105090:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef80609);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d491ef9);
		break;
	case 108480:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fefcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60309);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		break;
	case 100000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefb0808);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x604b1cf6);
		break;
	case 104000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef90608);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ef8);
		break;
	case 108000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		break;
	case 112000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0600fcfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f40108);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x564722ff);
		break;
	case 97650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefd0907);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x624c1af4);
		break;
	case 102300:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefa0708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5f4a1df7);
		break;
	case 106950:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5b491ffb);
		break;
	case 111600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fffcfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff40108);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722ff);
		break;
	case 98175:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0907);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x624c1bf5);
		break;
	case 102850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfc01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef90708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1df8);
		break;
	case 107250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 107525:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 112200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0600fcfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f40008);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x564622ff);
		break;
	default:
		return BBM_NOK;
	}
#elif (BBM_BAND_WIDTH == 7)
	switch (clk) {
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0807);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1bf5);
		break;
	case 120000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfc01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef90708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1df8);
		break;
	case 124800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5b491ffb);
		break;
	case 129600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x01020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fffcfe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff50108);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722fe);
		break;
	case 114000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefd0907);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x624c1af4);
		break;
	case 117000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefb0808);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x604b1cf6);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfc01);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef90708);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1df8);
		  break;*/
	case 126000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		break;
	case 115260:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0807);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1bf5);
		break;
	case 118650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefa0708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5f4b1df7);
		break;
	case 122040:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020100ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef80609);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ef9);
		break;
	case 125430:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 116000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfd02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefb0808);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1bf5);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfc01);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef90708);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1df8);
		  break;*/
	case 124000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70509);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5c491ffa);
		break;
	case 128000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x010201ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fefcfe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff50208);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x584821fd);
		break;
	case 116250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefb0808);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1cf6);
		break;
	case 120900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef90608);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1ef8);
		break;
	case 125550:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fffcfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff40108);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722ff);
		break;
	case 116875:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefb0808);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x604b1cf6);
		break;
	default:
		return BBM_NOK;
	}
#else /* BBM_BAND_WIDTH == 8) */
	switch (clk) {
	case 134400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefb0808);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x604b1cf6);
		break;
	case 139200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020100ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef80609);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ef9);
		break;
	case 144000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		break;
	case 148800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fffcfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff40108);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722ff);
		break;
	case 132000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0807);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1bf5);
		break;
	case 138000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef90608);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1ef8);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		  break;*/
	case 150000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0600fcfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f40008);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x56462200);
		break;
	case 132210:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfd02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0807);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1bf5);
		break;
	case 135600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefa0708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5f4b1df7);
		break;
	case 142380:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef70509);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5b491ffb);
		break;
	case 149160:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00020100);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fffcfd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f40108);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x564722ff);
		break;
		/*case 132000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfffbfe02);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefc0807);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x614b1bf5);
		  break;*/
	case 136000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x01fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefa0708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5f4a1df7);
		break;
	case 140000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020100ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef80609);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ef9);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		  break;*/
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0200ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0xfefbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefd0907);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x624c1af4);
		break;
	case 134850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0201ffff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x00fbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeefa0708);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x604b1cf7);
		break;
	case 139500:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020100ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fcfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef80609);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ef9);
		break;
	case 144150:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		break;
	case 130900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020200ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fdfcff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef60409);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4820fc);
		break;
	default:
		return BBM_NOK;
	}
#endif /* #if (BBM_BAND_WIDTH == 6) */

	return BBM_OK;
}

static s32 fc8350_set_acif_13seg(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH == 6)
	switch (clk) {
	case 97650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0503fefc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fafa01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf8080a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1ef8);
		break;
	case 98175:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0503fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf8070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1ef8);
		break;
	case 98310:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0503fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf8070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ef8);
		break;
	case 99000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0504fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf7070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ff9);
		break;
	case 100000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040400fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fbf9ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf7060b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5c4a1ffa);
		break;
	case 100750:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040400fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fbf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf6050b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5b4920fa);
		break;
	case 100800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040400fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fbf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf6050b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5b4920fa);
		break;
	case 101700:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040401fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fcf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5050b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 102000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030501fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fcf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5040b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 102300:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030501fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fcf9fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5040b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4921fc);
		break;
	case 102850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030501fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fdf9fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5040b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x594821fc);
		break;
	case 104000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020502fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fdf9fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef4030a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x584821fd);
		break;
	case 105000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020503ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x07fef9fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef4020a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722fe);
		break;
	case 105090:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020503ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x07fef9fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef3020a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722fe);
		break;
	case 105600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x010503ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x07fff9fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef3010a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722fe);
		break;
	case 106950:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff3000a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x56472300);
		break;
	case 107250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff20009);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55472300);
		break;
	case 107525:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff20009);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462300);
		break;
	case 108000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff2ff09);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462301);
		break;
	case 108480:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xff040401);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0801fafa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff2ff09);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x54462401);
		break;
	case 110400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe030402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0902fbfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f1fe08);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x53452402);
		break;
	case 111000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe030402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f1fd08);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452403);
		break;
	case 111600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfd020402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f1fd07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452503);
		break;
	case 112000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfd020402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f1fd07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452504);
		break;
	case 112200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfd020402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f1fc07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452504);
		break;
	case 113750:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfc010403);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0904fdfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf2f1fc06);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x51442505);
		break;
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfc010403);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0905fefa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf2f0fb06);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x50432606);
		break;
	case 116000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfc000404);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0905fefa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf3f0fa05);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4f432606);
		break;
	case 120000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfbfe0304);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x090700fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf4f0f803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4d422709);
		break;
	case 120250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfbfe0204);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x090700fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf5f0f803);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4c422709);
		break;
	case 123500:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfafd0104);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x080802fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf6f0f701);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4a40270a);
		break;
	case 124000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfafd0104);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x080802fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf6f0f601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4a40270b);
		break;
	case 124800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfafc0003);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x080803fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf7f0f601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4a40270b);
		break;
	case 126000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfafc0003);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x070803fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf7f1f600);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x4940280c);
		break;
	case 130000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfbfbfe02);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x060805ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfaf1f4fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x473e280e);
		break;
	case 132000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfcfbfd01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05080600);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfbf2f4fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x453d280f);
		break;
	case 134400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfdfbfc00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04080601);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xfcf2f4fb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x443c2810);
		break;
	default:
		return BBM_NOK;
	}
#elif (BBM_BAND_WIDTH == 7)
	switch (clk) {
	case 115200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0504fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf8070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ff9);
		break;
	case 120000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030501fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fdf9fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5040b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x594821fc);
		break;
	case 124800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff30009);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x56472300);
		break;
	case 129600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe030402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f1fd08);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452403);
		break;
	case 114000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0503fefc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fafa01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf8080a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1ef8);
		break;
	case 117000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040400fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fbf9ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf6060b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5c4920fa);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030501fe);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fdf9fd);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5040b);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x594821fc);
		  break;*/
	case 126000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff2ff09);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462301);
		break;
	case 115260:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0504fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf7070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ff9);
		break;
	case 118650:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040401fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fcf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5050b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 122040:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020502fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x07fef9fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef4020a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x584822fe);
		break;
	case 125430:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff20009);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462300);
		break;
	case 116000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0504fffd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fafaff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf7060b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5c4a1ff9);
		break;
		/*case 120000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030501fe);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fdf9fd);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5040b);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x594821fc);
		  break;*/
	case 124000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x010403ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x08fffafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef3010a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x564723ff);
		break;
	case 128000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfe030401);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0902fbfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf0f2fe08);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x53452402);
		break;
	case 116250:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0404fffd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fbfaff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf7060b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5c4a1ff9);
		break;
	case 120900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020502fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fdf9fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf4030a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x594821fd);
		break;
	case 125550:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff20009);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462300);
		break;
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfd020402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f1fd07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452503);
		break;
	case 116875:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040400fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fbf9ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf7060b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5c491ffa);
		break;
	default:
		return BBM_NOK;
	}
#else /* BBM_BAND_WIDTH == 8) */
	switch (clk) {
	case 134400:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040400fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fbf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf6050b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5b4920fa);
		break;
	case 139200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020502fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x07fef9fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef4020a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x584822fd);
		break;
	case 144000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff2ff09);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462301);
		break;
	case 148800:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfd020402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f1fd07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452503);
		break;
	case 132000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0504fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf7070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ff9);
		break;
	case 138000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030502fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x06fdf9fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf4030a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x594821fd);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafa);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff2ff09);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462301);
		  break;*/
	case 150000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfd020403);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f1fc07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x51442504);
		break;
	case 132210:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0504fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf7070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ff9);
		break;
	case 135600:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040401fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fcf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5050b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 142380:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040300);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafb);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff3000a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x56472300);
		break;
	case 149160:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xfd020402);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0903fcfa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xf1f1fd07);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x52452503);
		break;
		/*case 132000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0504fffc);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x03fafa00);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf7070b);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5d4a1ff9);
		  break;*/
	case 136000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x030501fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x05fcf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf5040b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5a4920fb);
		break;
	case 140000:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020503ff);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x07fef9fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef4020a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x574722fe);
		break;
		/*case 144000:
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x00040400);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0800fafa);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff2ff09);
		  bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462301);
		  break;*/
	case 130200:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0503fefc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fafa01);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf8080a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1ef8);
		break;
	case 134850:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x040400fd);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x04fbf9fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xedf6050b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5b4920fb);
		break;
	case 139500:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x020502fe);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x07fef9fc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeef4020a);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x584822fe);
		break;
	case 144150:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0xff040400);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x0801fafa);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xeff2ff09);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x55462301);
		break;
	case 130900:
		bbm_long_write(handle, devid, BBM_ACIF_COEF_00, 0x0503fffc);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_04, 0x02fafa00);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_08, 0xecf8070b);
		bbm_long_write(handle, devid, BBM_ACIF_COEF_12, 0x5e4a1ef8);
		break;
	default:
		return BBM_NOK;
	}
#endif
	return BBM_OK;
}

static s32 fc8350_set_acif_dvbt(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH_DVB == 6)
	switch (clk) {
	case 86400:
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_00, 0x03);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_01, 0xff);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_02, 0xfc);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_03, 0xfe);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_04, 0x05);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_05, 0x05);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_06, 0xfc);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_07, 0xf8);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_08, 0x01);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_09, 0x0c);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_10, 0x06);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_11, 0xf0);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_12, 0xec);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_13, 0x12);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_14, 0x4f);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_15, 0x6d);
		break;
	}
#elif (BBM_BAND_WIDTH_DVB == 7)
	switch (clk) {
	case 100800:
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_00, 0x03);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_01, 0xff);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_02, 0xfc);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_03, 0xfe);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_04, 0x05);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_05, 0x05);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_06, 0xfc);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_07, 0xf8);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_08, 0x01);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_09, 0x0c);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_10, 0x06);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_11, 0xf0);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_12, 0xec);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_13, 0x12);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_14, 0x4f);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_15, 0x6d);
		break;
	}
#else /* (BBM_BAND_WIDTH_DVB == 8) */
	switch (clk) {
	case 112000:
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_00, 0x03);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_01, 0x01);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_02, 0xfc);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_03, 0xfd);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_04, 0x03);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_05, 0x06);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_06, 0xfe);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_07, 0xf7);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_08, 0xfe);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_09, 0x0c);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_10, 0x08);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_11, 0xf2);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_12, 0xea);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_13, 0x10);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_14, 0x50);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_15, 0x70);
		break;
	case 134400:
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_00, 0xfc);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_01, 0xff);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_02, 0x03);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_03, 0x05);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_04, 0x00);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_05, 0xfa);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_06, 0xfa);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_07, 0x03);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_08, 0x0b);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_09, 0x07);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_10, 0xf8);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_11, 0xec);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_12, 0xf8);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_13, 0x1f);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_14, 0x4a);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_15, 0x5d);
		break;
	case 110400:
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_00, 0x03);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_01, 0x02);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_02, 0xfd);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_03, 0xfc);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_04, 0x02);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_05, 0x06);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_06, 0xff);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_07, 0xf7);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_08, 0xfd);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_09, 0x0b);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_10, 0x09);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_11, 0xf3);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_12, 0xe9);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_13, 0x0e);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_14, 0x50);
		bbm_byte_write(handle, devid, BBM_ACIF_COEF_15, 0x71);
		break;
	default:
		return BBM_NOK;
	}
#endif

	return BBM_OK;
}

static s32 fc8350_set_cal_front_1seg(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH == 6)
	switch (clk) {
	case 92000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x32c1);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x043d7b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1e300000);
		break;
	case 97650:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2fd1);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fead);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x200a999a);
		break;
	case 97875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2fb5);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fc53);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x201d8000);
		break;
	case 98175:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2f90);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f935);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2036b333);
		break;
	case 98304:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2f80);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7df);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20418937);
		break;
	case 98310:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2f7f);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7d0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20420a3d);
		break;
	case 99000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2f2b);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f0bb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x207c0000);
		break;
	case 100000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2eb2);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3e6a5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20d00000);
		break;
	case 100750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2e59);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3df36);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x210f0000);
		break;
	case 100800:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2e53);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3deb8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21133333);
		break;
	case 101250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2e1e);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3da51);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21390000);
		break;
	case 101376:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2e10);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d917);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21439581);
		break;
	case 101700:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2dea);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d5f3);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x215ecccd);
		break;
	case 102400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2d9a);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3cf3d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2199999a);
		break;
	case 102850:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2d67);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3caf9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21bf6666);
		break;
	case 103125:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2d48);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c862);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21d68000);
		break;
	case 104000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2ce6);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c03c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x22200000);
		break;
	case 105000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2c79);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3b717);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x22740000);
		break;
	case 105600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2c38);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3b1af);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x22a66666);
		break;
	case 107250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2b8a);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3a323);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x23310000);
		break;
	case 107525:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2b6d);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3a0c1);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2348199a);
		break;
	case 108000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2b3c);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x39cac);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x23700000);
		break;
	case 110400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2a4c);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x38892);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2439999a);
		break;
	case 111000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2a11);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x383ae);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x246c0000);
		break;
	case 111600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x29d7);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x37ed8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x249e6666);
		break;
	case 112000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x29b1);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x37ba5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x24c00000);
		break;
	case 112200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x299e);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x37a0f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x24d0cccd);
		break;
	case 113750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x290d);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x36dee);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x25530000);
		break;
	case 120000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x26e9);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x034034);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x27600000);
		break;
	case 120250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x26d5);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x33e79);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x27750000);
		break;
	case 132000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2360);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x02f48c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2b500000);
		break;
	default:
		return BBM_NOK;
	}
#elif (BBM_BAND_WIDTH == 7)
	switch (clk) {
	case 114000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x28f6);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fe01);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20100000);
		break;
	case 114688:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x28b7);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7df);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20418937);
		break;
	case 114750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x28b1);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f753);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20460000);
		break;
	case 115200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2889);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f35c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20666666);
		break;
	case 115260:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2883);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f2d5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x206ab852);
		break;
	case 115625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2862);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3efa4);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20850000);
		break;
	case 116000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2841);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ec62);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20a00000);
		break;
	case 116250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x282b);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ea39);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20b20000);
		break;
	case 116736:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2800);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3e60d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20d4fdf4);
		break;
	case 116875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x27f4);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x35674);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x26598000);
		break;
	case 117000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x27e9);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3e3cc);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20e80000);
		break;
	case 118125:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2788);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3da51);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21390000);
		break;
	case 118650:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x275b);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d5f3);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x215ecccd);
		break;
	case 118750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2752);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d520);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21660000);
		break;
	case 118784:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x274f);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d4d8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x216872b0);
		break;
	case 120000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x26e9);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3cae7);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21c00000);
		break;
	case 120900:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x269f);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c3ad);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2200cccd);
		break;
	default:
		return BBM_NOK;
	}
#else /* (BBM_BAND_WIDTH == 8) */
	switch (clk) {
	case 130200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x23dd);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fead);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x200a999a);
		break;
	case 130500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x23c8);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fc53);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x201d8000);
		break;
	case 130900:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x23ac);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x2fae8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2af3999a);
		break;
	case 131072:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x23a0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7df);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20418937);
		break;
	case 131250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2394);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f67f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x204cc000);
		break;
	case 131625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x237a);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f39b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20646000);
		break;
	case 132000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2360);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f0bb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x207c0000);
		break;
	case 132210:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2351);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ef21);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x20893ae1);
		break;
	case 134400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x22be);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3deb8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21133333);
		break;
	case 134850:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x22a0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3db69);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x212f8ccd);
		break;
	case 135000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2297);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3da51);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21390000);
		break;
	case 135168:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x228c);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d917);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21439581);
		break;
	case 135600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x226f);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d5f3);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x215ecccd);
		break;
	case 136000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2256);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d310);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21780000);
		break;
	case 136500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2235);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3cf7a);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21978000);
		break;
	case 137500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x21f6);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c862);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21d68000);
		break;
	case 138000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x21d6);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c4e0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x21f60000);
		break;
	case 139200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x218b);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3bc8e);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2241999a);
		break;
	case 139264:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2188);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3bc1e);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x2245a1cb);
		break;
	case 139500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x2179);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ba80);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x22548000);
		break;
	case 141312:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x210b);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ae42);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x22c6a7f0);
		break;
	case 141750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x20f1);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ab59);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x22e24000);
		break;
	case 143000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x20a7);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3a323);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x23310000);
		break;
	default:
		return BBM_NOK;
	}
#endif

	return BBM_OK;
}

static s32 fc8350_set_cal_front_3seg(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH == 6)
	switch (clk) {
	case 97650:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7dd6);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fead);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10054ccd);
		break;
	case 97875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7d8c);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fc53);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x100ec000);
		break;
	case 98175:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7d2a);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f935);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x101b599a);
		break;
	case 98304:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7d00);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7df);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1020c49c);
		break;
	case 98310:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7cfe);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7d0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1021051f);
		break;
	case 99000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7c1f);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f0bb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x103e0000);
		break;
	case 100000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7ae1);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3e6a5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10680000);
		break;
	case 100750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x79f7);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3df36);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10878000);
		break;
	case 100800:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x79e8);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3deb8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1089999a);
		break;
	case 101250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x795d);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3da51);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x109c8000);
		break;
	case 101376:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7936);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d917);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10a1cac1);
		break;
	case 101700:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x78d3);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d5f3);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10af6666);
		break;
	case 102000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7878);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d310);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10bc0000);
		break;
	case 102300:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x781e);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d031);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10c8999a);
		break;
	case 102400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7800);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3cf3d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10cccccd);
		break;
	case 102850:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x777a);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3caf9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10dfb333);
		break;
	case 103125:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7728);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c862);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10eb4000);
		break;
	case 103500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x76ba);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c4e0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10fb0000);
		break;
	case 104000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7627);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c03c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11100000);
		break;
	case 105600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x745d);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3b1af);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11533333);
		break;
	case 107250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7293);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3a323);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11988000);
		break;
	case 107525:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x7248);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3a0c1);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11a40ccd);
		break;
	case 108000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x71c7);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x39cac);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11b80000);
		break;
	case 112000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6db7);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x37ba5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12600000);
		break;
	case 112200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6d85);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x37a0f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12686666);
		break;
	default:
		return BBM_NOK;
	}
#elif (BBM_BAND_WIDTH == 7)
	switch (clk) {
	case 114000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6bca);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fe01);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10080000);
		break;
	case 114688:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6b25);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7df);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1020c49c);
		break;
	case 114750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6b16);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f753);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10230000);
		break;
	case 115200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6aab);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f35c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10333333);
		break;
	case 115260:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6a9c);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f2d5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10355c29);
		break;
	case 115625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6a46);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3efa4);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10428000);
		break;
	case 116000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x69ee);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ec62);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10500000);
		break;
	case 116250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x69b4);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ea39);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10590000);
		break;
	case 116736:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6943);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3e60d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x106a7efa);
		break;
	case 116875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6923);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x35674);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x132cc000);
		break;
	case 117000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6907);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3e3cc);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10740000);
		break;
	case 118125:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6807);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3da51);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x109c8000);
		break;
	case 118650:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6791);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d5f3);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10af6666);
		break;
	case 118750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x677a);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d520);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10b30000);
		break;
	case 118784:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6773);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d4d8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10b43958);
		break;
	case 119808:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6690);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3cc76);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10d91687);
		break;
	case 120000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6666);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3cae7);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10e00000);
		break;
	case 120250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6630);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c8e3);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10e90000);
		break;
	case 120900:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x65a3);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3c3ad);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11006666);
		break;
	case 121500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x6523);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3beeb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11160000);
		break;
	default:
		return BBM_NOK;
	}
#else /* BBM_BAND_WIDTH == 8) */
	switch (clk) {
	case 130200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5e61);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fead);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10054ccd);
		break;
	case 130500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5e29);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3fc53);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x100ec000);
		break;
	case 130900:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5de0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x2fae8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1579cccd);
		break;
	case 131072:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5dc0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f7df);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1020c49c);
		break;
	case 131250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5d9f);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f67f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10266000);
		break;
	case 131625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5d5b);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f39b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10323000);
		break;
	case 132000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5d17);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3f0bb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x103e0000);
		break;
	case 132210:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5cf1);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ef21);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10449d71);
		break;
	case 134400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5b6e);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3deb8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1089999a);
		break;
	case 135168:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5ae9);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3d917);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10a1cac1);
		break;
	case 136500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x5a06);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3cf7a);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10cbc000);
		break;
	case 141312:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x56f5);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3ae42);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x116353f8);
		break;
	case 143000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x55ee);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x3a323);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11988000);
		break;
	default:
		return BBM_NOK;
	}
#endif /* #if (BBM_BAND_WIDTH == 6) */

	return BBM_OK;
}

static s32 fc8350_set_cal_front_13seg(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH == 6)
	switch (clk) {
	case 97650:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xffab);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10054ccd);
		break;
	case 97875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xff15);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x100ec000);
		break;
	case 98175:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfe4d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x101b599a);
		break;
	case 98304:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfdf8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1020c49c);
		break;
	case 98310:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfdf4);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1021051f);
		break;
	case 99000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfc2f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x103e0000);
		break;
	case 100000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf9a9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10680000);
		break;
	case 100750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf7cd);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10878000);
		break;
	case 100800:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf7ae);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1089999a);
		break;
	case 101250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf694);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x109c8000);
		break;
	case 101376:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf646);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10a1cac1);
		break;
	case 101700:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf57d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10af6666);
		break;
	case 102000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf4c4);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10bc0000);
		break;
	case 102300:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf40c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10c8999a);
		break;
	case 102400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf3cf);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10cccccd);
		break;
	case 102850:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf2be);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10dfb333);
		break;
	case 103125:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf218);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10eb4000);
		break;
	case 103500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf138);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10fb0000);
		break;
	case 104000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf00f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11100000);
		break;
	case 104448:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xef07);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1122d0e5);
		break;
	case 104625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xeea0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x112a4000);
		break;
	case 105000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xedc6);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x113a0000);
		break;
	case 105090:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xed92);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x113dc7ae);
		break;
	case 105600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xec6c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11533333);
		break;
	case 106250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xeafa);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x116e8000);
		break;
	case 106496:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xea6f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1178d4fe);
		break;
	case 106950:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe970);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x118be666);
		break;
	case 107250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe8c9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11988000);
		break;
	case 107525:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe830);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11a40ccd);
		break;
	case 108000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe72b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11b80000);
		break;
	case 108480:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe625);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11cc28f6);
		break;
	case 109375:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe443);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11f1c000);
		break;
	case 110400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe224);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x121ccccd);
		break;
	case 110500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe1f0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12210000);
		break;
	case 110592:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe1c0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1224dd2f);
		break;
	case 111000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe0eb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12360000);
		break;
	case 111600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdfb6);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x124f3333);
		break;
	case 112000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdee9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12600000);
		break;
	case 112200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xde84);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12686666);
		break;
	case 112500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xddec);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12750000);
		break;
	case 113750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdb7b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12a98000);
		break;
	case 114000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdb00);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12b40000);
		break;
	case 114750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd992);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12d38000);
		break;
	case 115200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd8b8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12e66666);
		break;
	case 116000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd73a);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13080000);
		break;
	case 117000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd563);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13320000);
		break;
	case 118125:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd35a);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13614000);
		break;
	case 118750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd23e);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x137b8000);
		break;
	case 120000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd00d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13b00000);
		break;
	case 120250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xcf9e);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13ba8000);
		break;
	case 123500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xca28);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x14430000);
		break;
	case 124000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc957);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x14580000);
		break;
	case 124800:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc80d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1479999a);
		break;
	case 126000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc625);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x14ac0000);
		break;
	case 130000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc00c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x15540000);
		break;
	case 132000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xbd23);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x15a80000);
		break;
	case 134400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xb9c2);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x160ccccd);
		break;
	default:
		return BBM_NOK;
	}
#elif (BBM_BAND_WIDTH == 7)
	switch (clk) {
	case 114000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xff80);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10080000);
		break;
	case 114688:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfdf8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1020c49c);
		break;
	case 114750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfdd5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10230000);
		break;
	case 115200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfcd7);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10333333);
		break;
	case 115260:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfcb5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10355c29);
		break;
	case 115625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfbe9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10428000);
		break;
	case 116000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfb19);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10500000);
		break;
	case 116250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfa8e);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10590000);
		break;
	case 116736:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf983);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x106a7efa);
		break;
	case 116875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd59d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x132cc000);
		break;
	case 117000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf8f3);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10740000);
		break;
	case 118125:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf694);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x109c8000);
		break;
	case 118650:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf57d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10af6666);
		break;
	case 118750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf548);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10b30000);
		break;
	case 118784:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf536);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10b43958);
		break;
	case 119808:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf31d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10d91687);
		break;
	case 120000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf2ba);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10e00000);
		break;
	case 120250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf239);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10e90000);
		break;
	case 120900:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf0eb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11006666);
		break;
	case 121500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xefbb);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11160000);
		break;
	case 121875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xeefe);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11238000);
		break;
	case 122040:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xeeab);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x112970a4);
		break;
	case 122880:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xed09);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1147ae14);
		break;
	case 123500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xebd9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x115e0000);
		break;
	case 124000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xeae5);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11700000);
		break;
	case 124800:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe964);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x118ccccd);
		break;
	case 124875:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe940);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x118f8000);
		break;
	case 125000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe904);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11940000);
		break;
	case 125430:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe838);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11a37ae1);
		break;
	case 125550:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe7ff);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11a7cccd);
		break;
	case 126000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe72b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11b80000);
		break;
	case 126750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe5cd);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11d30000);
		break;
	case 126976:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe564);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11db22d1);
		break;
	case 128000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe38e);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12000000);
		break;
	case 128250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe31d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12090000);
		break;
	case 128820:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe21b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x121d851f);
		break;
	case 129024:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe1c0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1224dd2f);
		break;
	case 129600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe0bf);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1239999a);
		break;
	case 130000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe00e);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12480000);
		break;
	case 130200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdfb6);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x124f3333);
		break;
	case 130500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdf32);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x125a0000);
		break;
	case 131072:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xde39);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x126e978d);
		break;
	case 131250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xddec);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12750000);
		break;
	case 131625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdd4a);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12828000);
		break;
	case 132000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdca9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12900000);
		break;
	case 132210:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdc4f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12978f5c);
		break;
	case 134400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd8b8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12e66666);
		break;
	case 134850:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd7ff);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12f6999a);
		break;
	case 135000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd7c2);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12fc0000);
		break;
	case 135168:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd77d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13020c4a);
		break;
	case 136000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd62c);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13200000);
		break;
	case 136500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd563);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13320000);
		break;
	case 139200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd13f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13933333);
		break;
	case 139500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd0cc);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x139e0000);
		break;
	case 140000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd00d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13b00000);
		break;
	case 141750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xcd7b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13ef0000);
		break;
	default:
		return BBM_NOK;
	}
#else /* BBM_BAND_WIDTH == 8) */
	switch (clk) {
	case 130200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xffab);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10054ccd);
		break;
	case 130500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xff15);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x100ec000);
		break;
	case 130900:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xbeba);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1579cccd);
		break;
	case 131072:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfdf8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1020c49c);
		break;
	case 131250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfda0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10266000);
		break;
	case 131625:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfce7);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10323000);
		break;
	case 132000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfc2f);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x103e0000);
		break;
	case 132210:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xfbc8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10449d71);
		break;
	case 134400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf7ae);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1089999a);
		break;
	case 134850:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf6da);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1097c666);
		break;
	case 135000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf694);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x109c8000);
		break;
	case 135168:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf646);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10a1cac1);
		break;
	case 135600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf57d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10af6666);
		break;
	case 136000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf4c4);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10bc0000);
		break;
	case 136500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf3de);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10cbc000);
		break;
	case 137500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf218);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10eb4000);
		break;
	case 138000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xf138);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10fb0000);
		break;
	case 139200:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xef24);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1120cccd);
		break;
	case 139264:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xef07);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1122d0e5);
		break;
	case 139500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xeea0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x112a4000);
		break;
	case 140000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xedc6);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x113a0000);
		break;
	case 141312:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xeb91);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x116353f8);
		break;
	case 141750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xead6);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11712000);
		break;
	case 142380:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe9cc);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1184f852);
		break;
	case 143000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe8c9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11988000);
		break;
	case 143360:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe833);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11a3d70a);
		break;
	case 143750:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe792);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11b02000);
		break;
	case 144000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe72b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11b80000);
		break;
	case 144150:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe6ed);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x11bcb99a);
		break;
	case 147456:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe1c0);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1224dd2f);
		break;
	case 148500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xe02a);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1245c000);
		break;
	case 149500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdeaa);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12654000);
		break;
	case 150000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xddec);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12750000);
		break;
	case 152000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xdb00);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12b40000);
		break;
	case 153000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd992);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12d38000);
		break;
	case 153450:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd8ee);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12e1accd);
		break;
	case 153600:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd8b8);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x12e66666);
		break;
	case 156000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd563);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13320000);
		break;
	case 156250:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd50b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1339e000);
		break;
	case 157500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd35a);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13614000);
		break;
	case 158100:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd28d);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13742666);
		break;
	case 158400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd227);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x137d999a);
		break;
	case 159744:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xd062);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13a7ef9e);
		break;
	case 162000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xcd7b);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13ef0000);
		break;
	case 162500:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xccda);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x13fec000);
		break;
	case 165888:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc8ab);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x146978d5);
		break;
	case 169000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc4f9);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x14cb8000);
		break;
	case 172032:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0);
		bbm_long_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc180);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x152b020c);
		break;
	default:
		return BBM_NOK;
	}
#endif /* #if (BBM_BAND_WIDTH == 6) */

	return BBM_OK;
}

static s32 fc8350_set_cal_front_dvbt(HANDLE handle, DEVICEID devid, u32 clk)
{
#if (BBM_BAND_WIDTH_DVB == 6)
	switch (clk) {
	case 86400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0000);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xcf);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL1, 0xf3);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL2, 0x00);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10cccccc);
		break;
	}
#elif (BBM_BAND_WIDTH_DVB == 7)
	switch (clk) {
	case 100800:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0000);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xcf);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL1, 0xf3);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL2, 0x00);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10cccccc);
		break;
	}
#else /* BBM_BAND_WIDTH_DVB == 8) */
	switch (clk) {
	case 112000:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0000);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0xc7);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL1, 0xfa);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL2, 0x00);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x105554c9);
		break;
	case 134400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0000);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x69);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL1, 0xfe);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL2, 0x00);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x1019990F);
		break;
	case 110400:
		bbm_word_write(handle, devid, BBM_LOW_IF_VALUE, 0x0000);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL0, 0x68);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL1, 0xfe);
		bbm_byte_write(handle, devid, BBM_FREQ_COMPEN_VAL2, 0x00);
		bbm_long_write(handle, devid, BBM_NCO_OFFSET, 0x10199999);
		break;
	default:
		return BBM_NOK;
	}
#endif

	return BBM_OK;
}

static s32 fc8350_set_default_core_clk(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast)
{
	u16 pll_set = 0x1921;

	switch (broadcast) {
	case ISDBT_1SEG:
	case ISDBTSB_1SEG:
	case ISDBTSB_3SEG:
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		switch (BBM_XTAL_FREQ) {
#if (BBM_BAND_WIDTH == 6)
		case 16000:
			pll_set = 0x1920;
			break;
		case 16384:
			pll_set = 0x1820;
			break;
		case 18000:
			pll_set = 0x1620;
			break;
		case 19200:
			pll_set = 0x1520;
			break;
		case 24000:
			pll_set = 0x2121;
			break;
		case 24576:
			pll_set = 0x1020;
			break;
		case 26000:
			pll_set = 0x1f21;
			break;
		case 27000:
			pll_set = 0x0f20;
			break;
		case 27120:
			pll_set = 0x1d21;
			break;
		case 32000:
			pll_set = 0x1921;
			break;
		case 37200:
			pll_set = 0x1521;
			break;
		case 37400:
			pll_set = 0x1521;
			break;
		case 38400:
			pll_set = 0x1521;
			break;
#elif (BBM_BAND_WIDTH == 7)
		case 16000:
			pll_set = 0x1d20;
			break;
		case 16384:
			pll_set = 0x1c20;
			break;
		case 18000:
			pll_set = 0x1a20;
			break;
		case 19200:
			pll_set = 0x1820;
			break;
		case 24000:
			pll_set = 0x2721;
			break;
		case 24576:
			pll_set = 0x1320;
			break;
		case 26000:
			pll_set = 0x1220;
			break;
		case 27000:
			pll_set = 0x2321;
			break;
		case 27120:
			pll_set = 0x1120;
			break;
		case 32000:
			pll_set = 0x1d21;
			break;
		case 37200:
			pll_set = 0x1921;
			break;
		case 37400:
			pll_set = 0x1921;
			break;
		case 38400:
			pll_set = 0x1821;
#else /* BBM_BAND_WIDTH == 8 */
		case 16000:
			pll_set = 0x2120;
			break;
		case 16384:
			pll_set = 0x2120;
			break;
		case 18000:
			pll_set = 0x1e20;
			break;
		case 19200:
			pll_set = 0x1c20;
			break;
		case 24000:
			pll_set = 0x1620;
			break;
		case 24576:
			pll_set = 0x1620;
			break;
		case 26000:
			pll_set = 0x1520;
			break;
		case 27000:
			pll_set = 0x1420;
			break;
		case 27120:
			pll_set = 0x1420;
			break;
		case 32000:
			pll_set = 0x1120;
			break;
		case 37200:
			pll_set = 0x1d21;
			break;
		case 37400:
			pll_set = 0x1d21;
			break;
		case 38400:
			pll_set = 0x1c21;
			break;
#endif /* #if (BBM_BAND_WIDTH == 6) */
		default:
			break;
		}
		break;
	case DVB_T:
		switch (BBM_XTAL_FREQ) {
#if (BBM_BAND_WIDTH_DVB == 6)
		case 19200:
			pll_set = 0x1220;
			break;
#elif (BBM_BAND_WIDTH_DVB == 7)
		case 19200:
			pll_set = 0x1520;
			break;
#else /* (BBM_BAND_WIDTH_DVB == 8) */
		case 19200:
			pll_set = 0x1720;
			break;
		case 32000:
			pll_set = 0x1c21;
			break;
#endif
		}
		break;
	default:
		break;
	}

	bbm_byte_write(handle, devid, BBM_PLL_SEL, 0x00);
	bbm_byte_write(handle, devid, BBM_PLL1_RESET, 0x01);
	bbm_word_write(handle, devid, BBM_PLL1_PRE_POST_SELECTION, pll_set);
	bbm_byte_write(handle, devid, BBM_PLL1_RESET, 0x00);
	msWait(1);
	bbm_byte_write(handle, devid, BBM_PLL_SEL, 0x01);

	return fc8350_get_current_clk(handle, devid);
}

#ifdef BBM_I2C_TSIF
static s32 fc8350_set_tsif_clk(HANDLE handle, DEVICEID devid)
{
#if (BBM_TSIF_CLK == 48000)
	bbm_byte_write(handle, devid, BBM_TS_CLK_DIV, 0x01);
#elif (BBM_TSIF_CLK == 32000)
	bbm_byte_write(handle, devid, BBM_TS_CLK_DIV, 0x02);
#else /* (BBM_TSIF_CLK == 26000) */
	bbm_byte_write(handle, devid, BBM_TS_CLK_DIV, 0x02);
#endif

	bbm_byte_write(handle, devid, BBM_PLL2_ENABLE, 0x01);
	bbm_byte_write(handle, devid, BBM_PLL2_PD, 0x00);
#if (BBM_TSIF_CLK == 48000)
	if (BBM_XTAL_FREQ == 16000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2430);
	else if (BBM_XTAL_FREQ == 16384)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2330);
	else if (BBM_XTAL_FREQ == 18000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2030);
	else if (BBM_XTAL_FREQ == 19200)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1e30);
	else if (BBM_XTAL_FREQ == 24000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1830);
	else if (BBM_XTAL_FREQ == 24576)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1730);
	else if (BBM_XTAL_FREQ == 26000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1630);
	else if (BBM_XTAL_FREQ == 27000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1530);
	else if (BBM_XTAL_FREQ == 27120)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1530);
	else if (BBM_XTAL_FREQ == 32000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2431);
	else if (BBM_XTAL_FREQ == 37200)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1e31);
	else if (BBM_XTAL_FREQ == 37400)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1e31);
	else if (BBM_XTAL_FREQ == 38400)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1e31);
#elif (BBM_TSIF_CLK == 32000)
	if (BBM_XTAL_FREQ == 16000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2030);
	else if (BBM_XTAL_FREQ == 16384)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1f30);
	else if (BBM_XTAL_FREQ == 18000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1c30);
	else if (BBM_XTAL_FREQ == 19200)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1a30);
	else if (BBM_XTAL_FREQ == 24000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1530);
	else if (BBM_XTAL_FREQ == 24576)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1430);
	else if (BBM_XTAL_FREQ == 26000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1330);
	else if (BBM_XTAL_FREQ == 27000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1230);
	else if (BBM_XTAL_FREQ == 27120)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1230);
	else if (BBM_XTAL_FREQ == 32000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2031);
	else if (BBM_XTAL_FREQ == 37200)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1b31);
	else if (BBM_XTAL_FREQ == 37400)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1b31);
	else if (BBM_XTAL_FREQ == 38400)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1a31);
#else /* (BBM_TSIF_CLK == 26000) */
	if (BBM_XTAL_FREQ == 16000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1a30);
	else if (BBM_XTAL_FREQ == 16384)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1930);
	else if (BBM_XTAL_FREQ == 18000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1730);
	else if (BBM_XTAL_FREQ == 19200)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1530);
	else if (BBM_XTAL_FREQ == 24000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2231);
	else if (BBM_XTAL_FREQ == 24576)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2131);
	else if (BBM_XTAL_FREQ == 26000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x2031);
	else if (BBM_XTAL_FREQ == 27000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1e31);
	else if (BBM_XTAL_FREQ == 27120)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1e31);
	else if (BBM_XTAL_FREQ == 32000)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1a31);
	else if (BBM_XTAL_FREQ == 37200)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1631);
	else if (BBM_XTAL_FREQ == 37400)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1631);
	else if (BBM_XTAL_FREQ == 38400)
		bbm_word_write(handle, devid
		, BBM_PLL2_PRE_POST_SELECTION, 0x1531);
#endif /* #if (BBM_TSIF_CLK == 48000) */

	bbm_byte_write(handle, devid, BBM_PLL2_RESET, 0x00);

	return BBM_OK;
}
#endif /* #ifdef BBM_I2C_TSIF */

s32 fc8350_reset(HANDLE handle, DEVICEID devid)
{
	bbm_byte_write(handle, DIV_BROADCAST, BBM_SW_RESET, 0x7f);
	msWait(2);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_SW_RESET, 0xcf);
	msWait(2);
	bbm_byte_write(handle, DIV_MASTER, BBM_SW_RESET, 0xff);
	msWait(2);

	return BBM_OK;
}

s32 fc8350_probe(HANDLE handle, DEVICEID devid)
{
	u16 ver;
	bbm_word_read(handle, devid, BBM_CHIP_ID, &ver);
	pr_err("%s ver = %x\n", __func__, ver);
	return (ver == 0x8350) ? BBM_OK : BBM_NOK;
}

s32 fc8350_set_core_clk(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast, u32 freq)
{
	u32 new_pll_clk;
	u32 pre_sel, post_sel = 0, multi = 0;
	u32 current_clk, new_clk, pll_set;
	s32 res = BBM_OK;
	u32 div;
	int pow;

	current_clk = fc8350_get_current_clk(handle, devid);

	new_clk = fc8350_get_core_clk(handle, devid, broadcast, freq);

	if (new_clk == current_clk)
		return BBM_OK;

	div = new_clk * 1000 / BBM_XTAL_FREQ;
	for (pow = 0; pow < 3; pow++) {
		div = div * 2;
		if ((div % 1000) == 0)
			break;
	}

	multi = div;
	multi = multi / 1000;

	while (((BBM_XTAL_FREQ / 1000) * multi) < 390) {
		multi *= 2;
		pow++;
	}

	if (multi < 15) {
		multi *= 2;
		pow++;
	}

	if (pow == 2) {
		pre_sel = 1;
		post_sel = 2;
	} else {
		pre_sel = 0;
		post_sel = pow + 1;
	}

	new_pll_clk = (((BBM_XTAL_FREQ) >> pre_sel) * multi) >> post_sel;

	if (new_clk != new_pll_clk)
		return BBM_NOK;

	bbm_byte_write(handle, devid, BBM_PLL_SEL, 0x00);
	bbm_byte_write(handle, devid, BBM_PLL1_RESET, 0x01);
	pll_set = (multi << 8) | (post_sel << 4) | pre_sel;
	bbm_word_write(handle, devid, BBM_PLL1_PRE_POST_SELECTION, pll_set);
	bbm_byte_write(handle, devid, BBM_PLL1_RESET, 0x00);
	msWait(1);
	bbm_byte_write(handle, devid, BBM_PLL_SEL, 0x01);

	switch (broadcast) {
	case ISDBT_1SEG:
	case ISDBTSB_1SEG:
		res |= fc8350_set_cal_front_1seg(handle, devid, new_clk);
		break;
	case ISDBTSB_3SEG:
		res |= fc8350_set_cal_front_3seg(handle, devid, new_clk);
		break;
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		res |= fc8350_set_cal_front_13seg(handle, devid, new_clk);
		break;
	case DVB_T:
		res |= fc8350_set_cal_front_dvbt(handle, devid, new_clk);
		break;
	default:
		break;
	}

	switch (broadcast) {
	case ISDBT_1SEG:
		res |= fc8350_set_acif_b31_1seg(handle, devid, new_clk);
		break;
	case ISDBTSB_1SEG:
		res |= fc8350_set_acif_1seg(handle, devid, new_clk);
		break;
	case ISDBTSB_3SEG:
		res |= fc8350_set_acif_3seg(handle, devid, new_clk);
		break;
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		res |= fc8350_set_acif_13seg(handle, devid, new_clk);
		break;
	case DVB_T:
		res |= fc8350_set_acif_dvbt(handle, devid, new_clk);
		break;
	default:
		break;
	}

	return res;
}

s32 fc8350_init(HANDLE handle, DEVICEID devid)
{
#ifdef BBM_I2C_SPI
	u8 pol_pha = 0x10;
#endif

#if defined(BBM_I2C_TSIF) || defined(BBM_I2C_SPI) || defined(BBM_I2C_SDIO)
	bbm_byte_write(handle, DIV_MASTER, BBM_MD_INTERFACE, 0x01);
#elif defined(BBM_SPI_IF)
	bbm_byte_write(handle, DIV_MASTER, BBM_MD_INTERFACE, 0x02);
#elif defined(BBM_SDIO_IF)
	bbm_byte_write(handle, DIV_MASTER, BBM_MD_INTERFACE, 0x04);
#endif

#ifdef BBM_I2C_TSIF
#ifdef BBM_TS_204
	bbm_byte_write(handle, DIV_MASTER, BBM_TS_SEL, 0xc0);
#else
	bbm_byte_write(handle, DIV_MASTER, BBM_TS_SEL, 0x80);
#endif /* #ifdef BBM_TS_204 */
#endif /* #ifdef BBM_I2C_TSIF */

	bbm_byte_write(handle, DIV_BROADCAST, BBM_PLL1_ENABLE, 0x01);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_PLL1_PD, 0x00);

	fc8350_set_default_core_clk(handle, DIV_BROADCAST, ISDBT_13SEG);
#ifdef BBM_I2C_TSIF
	fc8350_set_tsif_clk(handle, DIV_MASTER);
#endif

	bbm_byte_write(handle, DIV_BROADCAST, BBM_XTAL_GAIN, 0x07);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_XTAL_LOAD_CAP, 0x05);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_LDO_VCTRL, 0x07);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_BB2XTAL_VCTRL, 0x0f);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_MEMORY_RWM0, 0x77);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_MEMORY_RWM1, 0x77);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_MEMORY_RWM2, 0x77);

	bbm_byte_write(handle, DIV_BROADCAST, BBM_ADC_CTRL, 0x27);

	bbm_byte_write(handle, DIV_BROADCAST, BBM_HOLD_RST_EN, 0x06);
	bbm_word_write(handle, DIV_BROADCAST, BBM_FD_RD_LATENCY_1SEG, 0x0620);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_NO_SIG_DET_EN, 0x00);

	bbm_word_write(handle, DIV_BROADCAST, BBM_REF_AMP, 0x03e0);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_DC_EST_EN, 0x1e);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_HP_EN_DURATION, 0xff);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_IQC_EN, 0x73);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_PGA_GAIN_MAX, 0x0c);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_PGA_GAIN_MIN, 0xe8);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_CSF_GAIN_MIN, 0x02);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_DC_OFFSET, 0x16);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_IQC_EN_AFTER_ACI, 0x70);

	bbm_word_write(handle, DIV_BROADCAST, BBM_CIR_P_POS, 0x0a8e);
	bbm_word_write(handle, DIV_BROADCAST, BBM_CIR_M_POS, 0x1fe2);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_CIR_COPY_MARGIN, 0x1e);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_CIR_SEL_MARGIN_MODE1, 0x14);

	bbm_byte_write(handle, DIV_BROADCAST, BBM_IDOPDETCFG, 0x03);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_IDOPDETCFG_MFDVALTHL, 0x22);

	bbm_byte_write(handle, DIV_BROADCAST, BBM_ADC_PWRDN, 0x00);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_ADC_RST, 0x00);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_ADC_BIAS, 0x06);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_BB2RF_RFEN, 0x01);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_RF_RST, 0x00);
	bbm_byte_write(handle, DIV_BROADCAST, BBM_RF_POWER_SAVE, 0x00);

	bbm_byte_write(handle, DIV_MASTER, BBM_FD_OUT_MODE, 0x02);
	bbm_byte_write(handle, DIV_MASTER, BBM_DIV_START_MODE, 0x16);

	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_TS0_START, TS0_BUF_START);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_TS0_END, TS0_BUF_END);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_TS0_THR, TS0_BUF_THR);

	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_A_START,
			AC_A_BUF_START);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_A_END, AC_A_BUF_END);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_A_THR, AC_A_BUF_THR);

	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_B_START,
			AC_B_BUF_START);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_B_END, AC_B_BUF_END);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_B_THR, AC_B_BUF_THR);

	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_C_START,
			AC_C_BUF_START);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_C_END, AC_C_BUF_END);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_C_THR, AC_C_BUF_THR);

	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_D_START,
			AC_D_BUF_START);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_D_END, AC_D_BUF_END);
	bbm_word_write(handle, DIV_BROADCAST, BBM_BUF_AC_D_THR, AC_D_BUF_THR);

#ifdef BBM_I2C_SPI
#ifdef BBM_I2C_SPI_POL_HI
	pol_pha |= 1;
#endif
#ifdef BBM_I2C_SPI_PHA_HI
	pol_pha |= 2;
#endif
	bbm_byte_write(handle, DIV_MASTER, BBM_BUF_SPIOUT, 0x10);
#endif

#ifdef BBM_I2C_PARALLEL_TSIF
	bbm_byte_write(handle, DIV_MASTER, BBM_TS_CTRL, 0x8e);
#endif

#ifdef BBM_AUX_INT
	bbm_byte_write(handle, DIV_MASTER, BBM_SYS_MD_INT_EN, 0x7f);
	bbm_byte_write(handle, DIV_MASTER, BBM_AUX_INT_EN, 0xff);
	bbm_byte_write(handle, DIV_MASTER, BBM_FEC_INT_EN, 0x07);
	bbm_byte_write(handle, DIV_MASTER, BBM_BER_AUTO_UP, 0xff);
	bbm_byte_write(handle, DIV_MASTER, BBM_OSS_CFG_EN, 0x01);
#endif

#ifdef BBM_NULL_PID_FILTER
	bbm_byte_write(handle, DIV_MASTER, BBM_NULL_PID_FILTERING, 0x01);
#endif

#ifdef BBM_FAIL_FRAME
	bbm_byte_write(handle, DIV_MASTER, BBM_FAIL_FRAME_TX, 0x07);
#endif

#ifdef BBM_TS_204
	bbm_byte_write(handle, DIV_MASTER, BBM_FEC_CTRL_A, 0x03);
	bbm_byte_write(handle, DIV_MASTER, BBM_FEC_CTRL_B, 0x03);
	bbm_byte_write(handle, DIV_MASTER, BBM_FEC_CTRL_C, 0x03);
	/* bbm_byte_write(handle, DIV_MASTER, BBM_FEC_CTRL, 0x03); */
#endif

#ifdef BBM_SPI_30M
	bbm_byte_write(handle, DIV_BROADCAST, BBM_MD_MISO, 0x1f);
#endif

#ifdef BBM_DESCRAMBLER
	bbm_byte_write(handle, DIV_BROADCAST, BBM_BCAS_ENABLE, 0x01);
#else
	bbm_byte_write(handle, DIV_BROADCAST, BBM_BCAS_ENABLE, 0x00);
#endif

	bbm_byte_write(handle, DIV_MASTER, BBM_INT_AUTO_CLEAR, 0x01);
	bbm_byte_write(handle, DIV_MASTER, BBM_BUF_ENABLE, 0x01);
	bbm_byte_write(handle, DIV_MASTER, BBM_BUF_INT_ENABLE, 0x01);

	bbm_byte_write(handle, DIV_MASTER, BBM_INT_MASK, 0x01);
	bbm_byte_write(handle, DIV_MASTER, BBM_INT_STS_EN, 0x01);

	fc8350_reset(handle, DIV_BROADCAST);

	return BBM_OK;
}

s32 fc8350_deinit(HANDLE handle, DEVICEID devid)
{
#ifdef BBM_I2C_TSIF
	bbm_byte_write(handle, DIV_MASTER, BBM_TS_SEL, 0x00);
	msWait(24);
#endif

	bbm_byte_write(handle, devid, BBM_SW_RESET, 0x00);

	return BBM_OK;
}

s32 fc8350_scan_status(HANDLE handle, DEVICEID devid)
{
	u32 ifagc_timeout       = 70;
	u32 ofdm_timeout        = 160;
	u32 ffs_lock_timeout    = 100;
	u32 cfs_timeout         = 120;
	u32 tmcc_timeout        = 1050;
	u32 ts_err_free_timeout = 0;
	u32 data                = 0;
	u8  a;
	u32 i;

	for (i = 0; i < ifagc_timeout; i++) {
		bbm_byte_read(handle, DIV_MASTER, 0x3025, &a);

		if (a & 0x01)
			break;

		msWait(SCAN_CHK_PERIOD);
	}

	if (i == ifagc_timeout) {
		pr_info("ifagc_timeout returning error\n");
		return BBM_NOK;
	}

	for (; i < ofdm_timeout; i++) {
		bbm_byte_read(handle, DIV_MASTER, 0x3025, &a);
		pr_debug("ofdm loop: a=0x%x\n", a);
		if (a & 0x08)
			break;

		msWait(SCAN_CHK_PERIOD);
	}

	if (i == ofdm_timeout) {
		pr_info("ISDBT ofdm_timeout\n");
		return BBM_NOK;
	}

	if (0 == (a & 0x04))
		return BBM_NOK;

	for (; i < ffs_lock_timeout; i++) {
		bbm_byte_read(handle, DIV_MASTER, 0x3026, &a);
		pr_debug("ffs loop: a=0x%x\n", a);
		if ((a & 0x11) == 0x11)
			break;

		msWait(SCAN_CHK_PERIOD);
	}

	if (i == ffs_lock_timeout) {
		pr_info("ISDBT ffs_lock_timeout\n");
		return BBM_NOK;
	}

	for (i = 0; i < cfs_timeout; i++) {
		bbm_byte_read(handle, DIV_MASTER, 0x3025, &a);
		pr_debug("cfs loop: a=0x%x\n", a);
		if (a & 0x40)
			break;

		msWait(SCAN_CHK_PERIOD);
	}

	if (i == cfs_timeout) {
		pr_info("ISDBT cfs_timeout\n");
		return BBM_NOK;
	}

	bbm_byte_read(handle, DIV_MASTER, 0x2023, &a);

	if (a & 0x01) {
		pr_info("ISDBT a & 0x01 = 1 return BBM_NOK\n");
		return BBM_NOK;
	}

	for (i = 0; i < tmcc_timeout; i++) {
		bbm_byte_read(handle, DIV_MASTER, 0x3026, &a);
		pr_debug("tmcc loop: a=0x%x\n", a);
		if (a & 0x02)
			break;

		msWait(SCAN_CHK_PERIOD);
	}

	if (i == tmcc_timeout) {
		pr_info("ISDBT tmcc_timeout\n");
		return BBM_NOK;
	}

	ts_err_free_timeout = 950;

	switch (broadcast_type) {
	case ISDBT_1SEG:
		bbm_word_read(handle, DIV_MASTER, 0x4113, (u16 *) &data);
		pr_info("ISDBT_1SEG\n");

		if ((data & 0x0008) == 0x0000)
			return BBM_NOK;

		if ((data & 0x1c70) == 0x1840)
			ts_err_free_timeout = 700;

		break;
	case ISDBTSB_1SEG:
		break;
	case ISDBTSB_3SEG:
		break;
	case ISDBT_13SEG:
		bbm_long_read(handle, DIV_MASTER, 0x4113, &data);

		if ((data & 0x3f8e1c78) == 0x0d0c1848)
			ts_err_free_timeout = 700;

		break;
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		bbm_long_read(handle, DIV_MASTER, 0x4113, &data);

		if ((data & 0x3f8e1c78) == 0x0d0c1848)
			ts_err_free_timeout = 700;

		break;
	case DVB_T:
		break;
	default:
		break;
	}

	for (i = 0; i < ts_err_free_timeout; i++) {
		bbm_byte_read(handle, DIV_MASTER, 0x50c5, &a);
		pr_debug("ts-err loop: a=0x%x\n", a);

		if (a)
			break;

		msWait(SCAN_CHK_PERIOD);
	}

	if (i == ts_err_free_timeout) {
		pr_info("ISDBT ts_err_free_timeout\n");
		return BBM_NOK;
	}

	return BBM_OK;
}

s32 fc8350_set_broadcast_mode(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast)
{
	s32 res = BBM_OK;
	u32 clk = fc8350_set_default_core_clk(handle, devid, broadcast);

	broadcast_type = broadcast;

	switch (broadcast) {
	case ISDBT_1SEG:
	case ISDBTSB_1SEG:
		res |= fc8350_set_cal_front_1seg(handle, devid, clk);
		break;
	case ISDBTSB_3SEG:
		res |= fc8350_set_cal_front_3seg(handle, devid, clk);
		break;
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		res |= fc8350_set_cal_front_13seg(handle, devid, clk);
		break;
	case DVB_T:
		res |= fc8350_set_cal_front_dvbt(handle, devid, clk);
		break;
	default:
		break;
	}

	switch (broadcast) {
	case ISDBT_1SEG:
		res |= fc8350_set_acif_b31_1seg(handle, devid, clk);
		break;
	case ISDBTSB_1SEG:
		res |= fc8350_set_acif_1seg(handle, devid, clk);
		break;
	case ISDBTSB_3SEG:
		res |= fc8350_set_acif_3seg(handle, devid, clk);
		break;
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		res |= fc8350_set_acif_13seg(handle, devid, clk);
		break;
	case DVB_T:
		res |= fc8350_set_acif_dvbt(handle, devid, clk);
		break;
	default:
		break;
	}

	/* system mode */
	switch (broadcast) {
	case ISDBT_1SEG:
		bbm_byte_write(handle, devid, BBM_CLK_CTRL, 0x05);
		bbm_byte_write(handle, devid, BBM_SYSTEM_MODE, 0x01);
		break;
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		bbm_byte_write(handle, devid, BBM_CLK_CTRL, 0x07);
		bbm_byte_write(handle, devid, BBM_SYSTEM_MODE, 0x00);
		break;
	case ISDBTSB_1SEG:
		bbm_byte_write(handle, devid, BBM_CLK_CTRL, 0x05);
		bbm_byte_write(handle, devid, BBM_SYSTEM_MODE, 0x02);
		break;
	case ISDBTSB_3SEG:
		bbm_byte_write(handle, devid, BBM_CLK_CTRL, 0x06);
		bbm_byte_write(handle, devid, BBM_SYSTEM_MODE, 0x03);
		break;
	case DVB_T:
		bbm_byte_write(handle, devid, BBM_CLK_CTRL, 0x07);
		bbm_byte_write(handle, devid, BBM_SYSTEM_MODE, 0x04);
		break;
	default:
		break;
	}

	/* pre-run */
	switch (broadcast) {
	case ISDBT_1SEG:
		bbm_long_write(handle, devid, BBM_MAN_PARTIAL_EN, 0x000c0101);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_A_MOD_TYPE,
				0x01000301);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_B_CODE_RATE,
				0x02030002);
		bbm_byte_write(handle, devid, BBM_MAN_LAYER_C_TI_LENGTH, 0x00);
		bbm_word_write(handle, devid, BBM_TDI_PRE_A, 0xc213);
		bbm_byte_write(handle, devid, BBM_TDI_PRE_C, 0x03);
		bbm_byte_write(handle, devid, BBM_FEC_MAIN_CTRL, 0x01);
		bbm_byte_write(handle, devid, BBM_FEC_LAYER, 0x01);
		break;
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		bbm_long_write(handle, devid, BBM_MAN_PARTIAL_EN, 0x000C0101);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_A_MOD_TYPE,
				0x01000301);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_B_CODE_RATE,
				0x02030002);
		bbm_byte_write(handle, devid, BBM_MAN_LAYER_C_TI_LENGTH, 0x00);
		bbm_word_write(handle, devid, BBM_TDI_PRE_A, 0xC21B);
		bbm_byte_write(handle, devid, BBM_TDI_PRE_C, 0x03);
		bbm_byte_write(handle, devid, BBM_FEC_MAIN_CTRL, 0x01);
		bbm_byte_write(handle, devid, BBM_FEC_LAYER, 0x02);
		break;
	case ISDBTSB_1SEG:
		bbm_long_write(handle, devid, BBM_MAN_PARTIAL_EN, 0x00020101);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_A_MOD_TYPE,
				0x00000202);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_B_CODE_RATE,
				0x03030000);
		bbm_byte_write(handle, devid, BBM_MAN_LAYER_C_TI_LENGTH, 0x00);
		bbm_word_write(handle, devid, BBM_TDI_PRE_A, 0x2313);
		bbm_byte_write(handle, devid, BBM_TDI_PRE_C, 0x03);
		bbm_byte_write(handle, devid, BBM_FEC_MAIN_CTRL, 0x01);
		bbm_byte_write(handle, devid, BBM_FEC_LAYER, 0x01);
		break;
	case ISDBTSB_3SEG:
		bbm_long_write(handle, devid, BBM_MAN_PARTIAL_EN, 0x00020101);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_A_MOD_TYPE,
				0x00000202);
		bbm_long_write(handle, devid, BBM_MAN_LAYER_B_CODE_RATE,
				0x03030000);
		bbm_byte_write(handle, devid, BBM_MAN_LAYER_C_TI_LENGTH, 0x00);
		bbm_word_write(handle, devid, BBM_TDI_PRE_A, 0x2313);
		bbm_byte_write(handle, devid, BBM_TDI_PRE_C, 0x03);
		bbm_byte_write(handle, devid, BBM_FEC_MAIN_CTRL, 0x01);
		bbm_byte_write(handle, devid, BBM_FEC_LAYER, 0x02);
		break;
	case DVB_T:
		bbm_byte_write(handle, devid, BBM_FEC_TX_BYPASS, 0x01);
		bbm_byte_write(handle, devid, BBM_FEC_RX_CTRL, 0x20);
		bbm_byte_write(handle, devid, BBM_FEC_MAIN_CTRL, 0x21);
		bbm_byte_write(handle, devid, BBM_FEC_LAYER, 0x01);
		break;
	default:
		break;
	}

	if (broadcast == ISDBT_13SEG || broadcast == ISDBT_CATV_13SEG ||
			broadcast == ISDBT_CATV_VHF_13SEG)
		bbm_byte_write(handle, devid,
				BBM_IIFOECFG_EARLYSTOP_THM, 0x18);
	else
		bbm_byte_write(handle, devid,
				BBM_IIFOECFG_EARLYSTOP_THM, 0x0e);

	switch (broadcast) {
	case ISDBT_1SEG:
	case ISDBTSB_1SEG:
		bbm_word_write(handle, devid, BBM_MSNR_FREQ_S_POW_MAN_VALUE3,
				0xb00f);
		break;
	case ISDBTSB_3SEG:
		bbm_word_write(handle, devid, BBM_MSNR_FREQ_S_POW_MAN_VALUE3,
				0x3030);
		break;
	case ISDBT_13SEG:
	case ISDBT_CATV_13SEG:
	case ISDBT_CATV_VHF_13SEG:
		bbm_word_write(handle, devid, BBM_MSNR_FREQ_S_POW_MAN_VALUE3,
				0x00c3);
		break;
	case DVB_T:
		break;
	default:
		break;
	}

	if (broadcast == ISDBT_1SEG || broadcast == ISDBTSB_1SEG)
		bbm_byte_write(handle, devid, BBM_CFG_ADG_INIT, 0xff);
	else
		bbm_byte_write(handle, devid, BBM_CFG_ADG_INIT, 0x40);

	return res;
}

