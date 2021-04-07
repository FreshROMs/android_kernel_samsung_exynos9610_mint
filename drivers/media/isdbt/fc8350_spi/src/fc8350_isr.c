/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350_isr.c
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
#include <linux/kernel.h>

#include "fci_types.h"
#include "fc8350_regs.h"
#include "fci_hal.h"

s32 (*fc8350_ts_callback)(void *userdata, u8 bufid, u8 *data, s32 length);
void *fc8350_ts_user_data;

#ifndef BBM_I2C_TSIF
#ifdef CONFIG_SEC_SPI_BYPASS_DATA
extern u8 *g_isdbt_ts_buffer;
#else
static u8 ts_buffer[TS0_32PKT_LENGTH];
#endif

static void fc8350_data(HANDLE handle, DEVICEID devid, u8 buf_int_status)
{
	if (buf_int_status) {
#ifdef CONFIG_SEC_SPI_BYPASS_DATA
		u8 *ts_buffer = g_isdbt_ts_buffer;
#endif
		bbm_data(handle, devid, BBM_TS0_DATA
			, &ts_buffer[0], g_pkt_length);

#ifdef CONFIG_SEC_SPI_BYPASS_DATA
		if (fc8350_ts_callback)
			(*fc8350_ts_callback)(fc8350_ts_user_data,
					0, &ts_buffer[4], g_pkt_length);
#else
		if (fc8350_ts_callback)
			(*fc8350_ts_callback)(fc8350_ts_user_data,
					0, &ts_buffer[0], g_pkt_length);
#endif
	}
}
#endif

#ifdef BBM_AUX_INT
static void fc8350_aux_int(HANDLE handle, DEVICEID devid, u8 aux_int_status)
{
	if (aux_int_status & AUX_INT_TMCC_INT_SRC)
		;

	if (aux_int_status & AUX_INT_TMCC_INDTPS_SRC)
		;

	if (aux_int_status & AUX_INT_AC_PREFRM_SRC)
		;

	if (aux_int_status & AUX_INT_AC_EWISTAFLAG_SRC)
		;

	if (aux_int_status & AUX_INT_SYNC_RELATED_INT) {
		u8 sync = 0;

		bbm_byte_read(handle, DIV_MASTER, BBM_SYS_MD_INT_CLR, &sync);

		if (sync) {
			bbm_byte_write(handle, DIV_MASTER, BBM_SYS_MD_INT_CLR,
									sync);

			if (sync & SYS_MD_NO_OFDM_DETECT)
				;

			if (sync & SYS_MD_RESYNC_OCCUR)
				;

			if (sync & SYS_MD_TMCC_LOCK)
				;

			if (sync & SYS_MD_A_LAYER_BER_UPDATE)
				;

			if (sync & SYS_MD_B_LAYER_BER_UPDATE)
				;

			if (sync & SYS_MD_C_LAYER_BER_UPDATE)
				;

			if (sync & SYS_MD_BER_UPDATE)
				;
		}
	}

	if (aux_int_status & AUX_INT_GPIO_INT_CLEAR)
		;

	if (aux_int_status & AUX_INT_FEC_RELATED_INT) {
		u8 fec = 0;

		bbm_byte_read(handle, DIV_MASTER, BBM_FEC_INT_CLR, &fec);

		if (fec) {
			bbm_byte_write(handle, DIV_MASTER, BBM_FEC_INT_CLR,
								fec);

			if (fec & FEC_INT_IRQ_A_TS_ERROR)
				;

			if (fec & FEC_INT_IRQ_B_TS_ERROR)
				;

			if (fec & FEC_INT_IRQ_C_TS_ERROR)
				;
		}
	}

	if (aux_int_status & AUX_INT_AUTO_SWITCH) {
		u8 auto_switch = 0;
		bbm_byte_read(handle, DIV_MASTER, BBM_OSS_MNT, &auto_switch);

		if (auto_switch & AUTO_SWITCH_1_SEG) /* 1-SEG */
			;
		else /* 12-SEG */
			;
	}
}
#endif

void fc8350_isr(HANDLE handle)
{
#ifdef BBM_AUX_INT
	u8 aux_int_status = 0;
#endif

#ifndef BBM_I2C_TSIF
	u8 buf_int_status;
	int i;
	for (i = 0; i < 10; i++) {
		buf_int_status = 0;
		bbm_byte_read(handle, DIV_MASTER, BBM_BUF_STATUS_CLEAR,
				&buf_int_status);
		if (buf_int_status) {
			bbm_byte_write(handle, DIV_MASTER,
					BBM_BUF_STATUS_CLEAR, buf_int_status);

			fc8350_data(handle, DIV_MASTER, buf_int_status);
			pr_debug("[FC8350] buf_int_status read count  %d\n"
				, i+1);
		} else {
			break;
		}
	}

	buf_int_status = 0;
	bbm_byte_read(handle, DIV_MASTER, BBM_BUF_STATUS_CLEAR,
					&buf_int_status);
	if (buf_int_status) {
		bbm_byte_write(handle, DIV_MASTER,
				BBM_BUF_STATUS_CLEAR, buf_int_status);

		fc8350_data(handle, DIV_MASTER, buf_int_status);
	}
#endif

#ifdef BBM_AUX_INT
	bbm_byte_read(handle, DIV_MASTER, BBM_AUX_STATUS_CLEAR,
					&aux_int_status);

	if (aux_int_status) {
		bbm_byte_write(handle, DIV_MASTER,
				BBM_AUX_STATUS_CLEAR, aux_int_status);

		fc8350_aux_int(handle, DIV_MASTER, aux_int_status);
	}
#endif
}

