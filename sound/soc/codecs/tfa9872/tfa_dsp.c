/*
 * Copyright 2015 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or later
 * as published by the Free Software Foundation.
 */

#include "config.h"

#include "tfa98xx_tfafieldnames.h"
#include "tfa_internal.h"
#include "tfa.h"
#include <sound/tfa_ext.h>
#include "tfa_service.h"
#include "tfa_container.h"
#include "tfa_dsp_fw.h"
/* TODO: remove genregs usage? */
#include "tfa98xx_genregs_N1C.h"
#if defined(TFADSP_DSP_MSG_APR_PACKET_STRATEGY)
#include <linux/qdsp6v2/apr_tal.h>
#endif

/* handle macro for bitfield */
#define TFA_MK_BF(reg, pos, len) ((reg << 8)|(pos << 4)|(len - 1))

/* abstract family for register */
#define FAM_TFA98XX_CF_CONTROLS (TFA_FAM(handle, RST) >> 8)
#define FAM_TFA98XX_CF_MEM      (TFA_FAM(handle, MEMA) >> 8)
#define FAM_TFA98XX_MTP0        (TFA_FAM(handle, MTPOTC) >> 8)
#define FAM_TFA98xx_INT_EN      (TFA_FAM(handle, INTENVDDS) >> 8)

#define CF_STATUS_I2C_CMD_ACK 0x01

int tfa98xx_log_start_cnt;
int tfa98xx_log_tfa_family;

#ifndef MIN
#define MIN(A, B) ((A < B) ? A : B)
#endif

#if defined(TFADSP_DSP_MSG_APR_PACKET_STRATEGY)
/* in case of CONFIG_MSM_QDSP6_APRV2_GLINK/APRV3_GLINK, */
/* with smaller APR_MAX_BUF (512) */
#define APR_RESIDUAL_SIZE  60
#define MAX_APR_MSG_SIZE  (APR_MAX_BUF-APR_RESIDUAL_SIZE) /* 452 */
/* #define STANDARD_PACKET_SIZE  (MAX_APR_MSG_SIZE-4) // 448 */
/* (packet_id:2, packet_size:2) */
#endif

/* retry values */
#define CFSTABLE_TRIES   10
#define PWDNWAIT_TRIES   50
#define AMPOFFWAIT_TRIES 50
#define MTPBWAIT_TRIES   50
#define MTPEX_WAIT_NTRIES 25

#define REDUCED_REGISTER_SETTING
#define RAMPING_DOWN_BEFORE_STOP
#define WRITE_CALIBRATION_DATA_TO_MTP
#define CHECK_CALIBRATION_DATA_RANGE
#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
#define MAX_WAIT_COUNT_UNTIL_CALIBRATION_DONE 20
static enum tfa98xx_error
tfa_tfadsp_wait_calibrate_done(tfa98xx_handle_t handle);
#endif
#define SET_CALIBRATION_AT_ALL_DEVICE_READY

static int tfa98xx_runtime_verbose;
static int tfa98xx_trace_level;
static int tfa98xx_dsp_verbose;

/* 4 possible I2C addresses */
TFA_INTERNAL struct tfa98xx_handle_private handles_local[MAX_HANDLES];

/* calibration done executed */
#define TFA_MTPEX_POS           TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS /**/

static enum tfa98xx_error _tfa_stop(tfa98xx_handle_t handle);

/*
 * static variables
 */
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
static int active_handle = -1;
#endif
#if defined(REDUCED_REGISTER_SETTING)
static int first_after_boot[MAX_HANDLES] = {1};
#endif

/*
 * static functions
 */
#if defined(TFADSP_32BITS)
/* Sign extend to 32-bit from 24-bit */
static void tfa_msg24to32(
	int32_t *out32buf, const uint8_t *in24buf, int length)
{
	int i = 0;
	int cmd_index = 0;
	int32_t tmp;

	while (length > 0) {
		tmp = ((uint8_t)in24buf[i] << 16)
			+ ((uint8_t)in24buf[i+1] << 8)
			+ (uint8_t)in24buf[i+2];

		/* Sign extend to 32-bit from 24-bit */
		out32buf[cmd_index] = ((int32_t)tmp << 8) >> 8;

		cmd_index++;
		i = i+3;
		length = length-3;
	}
}

/* 32bits data --> 24 bits: discard the MSB byte */
static void tfa_msg32to24(
	uint8_t *out24buf, const uint8_t *in32buf, int length)
{
	int i, j;
	int len_out;

	len_out = (length / 4) * 3;

	for (i = 0, j = 0; i < len_out; i += 3, j += 4) {
		/* reverse bytes order */
		out24buf[i+2] = in32buf[j];
		out24buf[i+1] = in32buf[j+1];
		out24buf[i] = in32buf[j+2];
	}
}
#endif /* (TFADSP_32BITS) */

TFA_INTERNAL int tfa98xx_handle_is_open(tfa98xx_handle_t h)
{
	int retval = 0;

	if ((h >= 0) && (h < MAX_HANDLES))
		retval = handles_local[h].in_use != 0;

	return retval;
}

int print_calibration(tfa98xx_handle_t handle, char *str, size_t size)
{
	return snprintf(str, size, "[%d:0x%02x] Prim:%d mOhms, Sec:%d mOhms\n",
		handle, handles_local[handle].slave_address / 2,
		handles_local[handle].mohm[0],
		handles_local[handle].mohm[1]);
}

int tfa_get_calibration_info(tfa98xx_handle_t handle, int channel)
{
	return handles_local[handle].mohm[channel];
}

/* return sign extended tap pattern */
int tfa_get_tap_pattern(tfa98xx_handle_t handle)
{
	int value = tfa_get_bf(handle, TFA9912_BF_CFTAPPAT);
	int bitshift;
	uint8_t field_len = 1 + (TFA9912_BF_CFTAPPAT & 0x0f);
	/* length of bitfield */

	bitshift = 8 * sizeof(int) - field_len;
	/* signextend */
	value = (value << bitshift) >> bitshift;

	return value;
}

/*
 * interrupt bit function to clear
 */
int tfa_irq_clear(tfa98xx_handle_t handle, enum tfa9912_irq bit)
{
	unsigned char reg;

	/* make bitfield enum */
	if (bit == tfa9912_irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_INTERRUPT_IN_REG1;
			reg < TFA98XX_INTERRUPT_IN_REG1 + 3; reg++)
			reg_write(handle, reg, 0xffff); /* all bits */
	} else {
		if (bit < tfa9912_irq_max) {
			reg = (unsigned char)
				(TFA98XX_INTERRUPT_IN_REG1 + (bit >> 4));
			reg_write(handle, reg,  1 << (bit & 0x0f));
			/* only this bit */
		} else {
			return TFA_ERROR;
		}
	}

	return 0;
}

/*
 * return state of irq or -1 if illegal bit
 */
int tfa_irq_get(tfa98xx_handle_t handle, enum tfa9912_irq bit)
{
	uint16_t value;
	int reg, mask;

	if (bit < tfa9912_irq_max) {
		/* only this bit */
		reg = TFA98XX_INTERRUPT_OUT_REG1 + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(handle, (unsigned char)reg, &value);
	} else
		return TFA_ERROR;

	return (value & mask) != 0;
}

/*
 * interrupt bit function that operates on the shadow regs in the handle
 */
int tfa_irq_ena(tfa98xx_handle_t handle, enum tfa9912_irq bit, int state)
{
	uint16_t value, new_value;
	int reg = 0, mask;

	/* */
	if (bit == tfa9912_irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_INTERRUPT_ENABLE_REG1;
			reg <= TFA98XX_INTERRUPT_ENABLE_REG1
			+ (tfa9912_irq_max / 16); reg++) {
			reg_write(handle,
				(unsigned char)reg,
				state ? 0xffff : 0); /* all bits */
			handles_local[handle].interrupt_enable
				[reg - TFA98XX_INTERRUPT_ENABLE_REG1] =
				state ? 0xffff : 0; /* all bits */
		}
	} else {
		if (bit < tfa9912_irq_max) {
		 /* only this bit */
			reg = TFA98XX_INTERRUPT_ENABLE_REG1 + (bit >> 4);
			mask = 1 << (bit & 0x0f);
			reg_read(handle, (unsigned char)reg, &value);

			if (state) /* set */
				new_value = (uint16_t)(value | mask);
			else /* clear */
				new_value = value & ~mask;

			if (new_value != value) {
				reg_write(handle,
					(unsigned char)reg,
					new_value); /* only this bit */
				handles_local[handle].interrupt_enable
					[reg-TFA98XX_INTERRUPT_ENABLE_REG1] =
					new_value;
			}
		} else {
			return TFA_ERROR;
		}
	}

	return 0;
}

/*
 * mask interrupts by disabling them
 */
int tfa_irq_mask(tfa98xx_handle_t handle)
{
	int reg;

	/* operate on all bits */
	for (reg = TFA98XX_INTERRUPT_ENABLE_REG1;
		reg <= TFA98XX_INTERRUPT_ENABLE_REG1
		+ (tfa9912_irq_max / 16); reg++)
		reg_write(handle, (unsigned char)reg, 0);

	return 0;
}

/*
 * unmask interrupts by enabling them again
 */
int tfa_irq_unmask(tfa98xx_handle_t handle)
{
	int reg;

	/* operate on all bits */
	for (reg = TFA98XX_INTERRUPT_ENABLE_REG1;
		reg <= TFA98XX_INTERRUPT_ENABLE_REG1
		+ (tfa9912_irq_max / 16); reg++)
		reg_write(handle,
			(unsigned char)reg,
			handles_local[handle].interrupt_enable
			[reg-TFA98XX_INTERRUPT_ENABLE_REG1]);

	return 0;
}

/*
 * interrupt bit function that sets the polarity
 */
int tfa_irq_set_pol(tfa98xx_handle_t handle, enum tfa9912_irq bit, int state)
{
	uint16_t value, new_value;
	int reg = 0, mask;

	if (bit == tfa9912_irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_STATUS_POLARITY_REG1;
			reg <= TFA98XX_STATUS_POLARITY_REG1
			+ (tfa9912_irq_max / 16); reg++) {
			reg_write(handle,
				(unsigned char)reg,
				state ? 0xffff : 0); /* all bits */
		}
	} else if (bit < tfa9912_irq_max) {
		 /* only this bit */
		reg = TFA98XX_STATUS_POLARITY_REG1 + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(handle, (unsigned char)reg, &value);

		if (state) /* Active High */
			new_value = (uint16_t)(value | mask);
		else /* Active Low */
			new_value = value & ~mask;

		if (new_value != value) {
			reg_write(handle, (unsigned char)reg, new_value);
			/* only this bit */
		}
	} else
		return TFA_ERROR;

	return 0;
}

/*
 * update dirty cached regs to i2c registers
 */
void tfa_irq_update(tfa98xx_handle_t handle)
{
	static int maxregs;
	static uint16_t regs_cache_irq_ena[(tfa9912_irq_max / 16) + 1];
	int i, reg;

	if (!maxregs) {
		maxregs = tfa9912_irq_max/16;
		for (i = 0; i < maxregs; i++) {
			reg = TFA98XX_INTERRUPT_ENABLE_REG1 + i;
			reg_read(handle, (unsigned char)reg,
				&regs_cache_irq_ena[i]);
		}
	}
	/* write only regs that changed */
	for (i = 0; i < maxregs; i++) {
		if (regs_cache_irq_ena[i] !=
			handles_local[handle].interrupt_enable[i]) {
			reg = TFA98XX_INTERRUPT_ENABLE_REG1 + i;
			reg_write(handle, (unsigned char)reg,
				regs_cache_irq_ena[i]);
		}
	}
}

/*
 * open
 * set device info and register device ops
 */
static void tfa_set_query_info(int dev_idx)
{
	if (dev_idx >= MAX_HANDLES) {
		_ASSERT(dev_idx >= MAX_HANDLES);
		return;
	}

	/* invalidate device struct cached values */
	handles_local[dev_idx].hw_feature_bits = -1;
	handles_local[dev_idx].sw_feature_bits[0] = -1;
	handles_local[dev_idx].sw_feature_bits[1] = -1;
	handles_local[dev_idx].profile = -1;
	handles_local[dev_idx].vstep[0] = -1;
	handles_local[dev_idx].vstep[1] = -1;
	handles_local[dev_idx].default_boost_trip_level = -1;
	/* defaults */
	handles_local[dev_idx].tfa_family = 1;
	handles_local[dev_idx].daimap = TFA98XX_DAI_I2S; /* all others */
	handles_local[dev_idx].spkr_count = 1;
	handles_local[dev_idx].spkr_select = 0;
	handles_local[dev_idx].spkr_damaged = 0;
	handles_local[dev_idx].support_tcoef = SUPPORT_YES;
	handles_local[dev_idx].support_drc = SUPPORT_NOT_SET;
	handles_local[dev_idx].support_saam = SUPPORT_NOT_SET;
	/* handles_local[dev_idx].ext_dsp = 0; */
	handles_local[dev_idx].is_cold = 1;
	handles_local[dev_idx].is_bypass = 0;
	handles_local[dev_idx].saam_use_case = 0; /* not in use */
	handles_local[dev_idx].stream_state = 0; /* not in use */

	/* TODO use the getfeatures() for retrieving the features [artf103523]
	 * handles_local[dev_idx].support_drc = SUPPORT_NOT_SET;
	 */

	pr_info("%s: device type (0x%04x)\n",
		__func__, handles_local[dev_idx].rev);
	switch (handles_local[dev_idx].rev & 0xff) {
	case 0x72:
		/* tfa9872 */
		handles_local[dev_idx].support_drc = SUPPORT_YES;
		handles_local[dev_idx].tfa_family = 2;
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = TFA98XX_DAI_TDM;
		tfa9872_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;
	case 0x88:
		/* tfa9888 */
		handles_local[dev_idx].tfa_family = 2;
		handles_local[dev_idx].spkr_count = 2;
		handles_local[dev_idx].daimap = TFA98XX_DAI_TDM;
		tfa9888_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;
	case 0x97:
		/* tfa9897 */
		handles_local[dev_idx].support_drc = SUPPORT_NO;
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = TFA98XX_DAI_TDM;
		tfa9897_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;
	case 0x96:
		/* tfa9896 */
		handles_local[dev_idx].support_drc = SUPPORT_NO;
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = TFA98XX_DAI_TDM;
		tfa9896_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;
	case 0x92:
		/* tfa9891 */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap =
			(TFA98XX_DAI_PDM | TFA98XX_DAI_I2S);
		tfa9891_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;
	case 0x91:
		/* tfa9890B */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap =
			(TFA98XX_DAI_PDM | TFA98XX_DAI_I2S);
		break;
	case 0x80:
	case 0x81:
		/* tfa9890 */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = TFA98XX_DAI_I2S;
		handles_local[dev_idx].support_drc = SUPPORT_NO;
		handles_local[dev_idx].support_framework = SUPPORT_NO;
		tfa9890_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;
	case 0x12:
		/* tfa9895 */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = TFA98XX_DAI_I2S;
		tfa9895_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;
	case 0x13:
		/* tfa9912 */
		handles_local[dev_idx].tfa_family = 2;
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = TFA98XX_DAI_TDM;
		tfa9912_ops(&handles_local[dev_idx].dev_ops);
		/* register device operations */
		break;

	default:
		pr_err("unknown device type : 0x%02x\n",
			handles_local[dev_idx].rev);
		_ASSERT(0);
		break;
	}
}

#if defined(TFADSP_DSP_BUFFER_POOL)
enum tfa98xx_error tfa_buffer_pool(int index, int size, int control)
{
	int dev, devcount = tfa98xx_cnt_max_device();

	switch (control) {
	case POOL_ALLOC: /* allocate */
		for (dev = 0; dev < devcount; dev++) {
			handles_local[dev].buf_pool[index].pool =
				kmalloc(size, GFP_KERNEL);
			if (handles_local[dev]
				.buf_pool[index].pool == NULL) {
				handles_local[dev]
					.buf_pool[index].size = 0;
				handles_local[dev]
					.buf_pool[index].in_use = 1;
				pr_err("%s: dev %d - buffer_pool[%d] - kmalloc error %d bytes\n",
					__func__, dev, index, size);
				return TFA98XX_ERROR_FAIL;
			}
			pr_info("%s: dev %d - buffer_pool[%d] - kmalloc allocated %d bytes\n",
				__func__, dev, index, size);
			handles_local[dev].buf_pool[index].size = size;
			handles_local[dev].buf_pool[index].in_use = 0;
		}
		break;

	case POOL_FREE: /* deallocate */
		for (dev = 0; dev < devcount; dev++) {
			if (handles_local[dev]
				.buf_pool[index].pool != NULL)
				kfree(handles_local[dev]
					.buf_pool[index].pool);
			pr_info("%s: dev %d - buffer_pool[%d] - kfree\n",
				__func__, dev, index);
			handles_local[dev].buf_pool[index].pool = NULL;
			handles_local[dev].buf_pool[index].size = 0;
			handles_local[dev].buf_pool[index].in_use = 0;
		}
		break;

	default:
		pr_info("%s: wrong control\n", __func__);
		break;
	}

	return TFA98XX_ERROR_OK;
}

int tfa98xx_buffer_pool_access(tfa98xx_handle_t handle,
	int r_index, size_t g_size, int control)
{
	int index;

	switch (control) {
	case POOL_GET: /* get */
		for (index = 0; index < POOL_MAX_INDEX; index++) {
			if (handles_local[handle]
				.buf_pool[index].in_use)
				continue;
			if (handles_local[handle]
				.buf_pool[index].size < g_size)
				continue;
			handles_local[handle]
				.buf_pool[index].in_use = 1;
			pr_debug("%s: dev %d - get buffer_pool[%d]\n",
				 __func__, handle, index);
			return index;
		}

		pr_info("%s: dev %d - failed to get buffer_pool\n",
			__func__, handle);
		break;

	case POOL_RETURN: /* return */
		if (handles_local[handle]
			.buf_pool[r_index].in_use == 0) {
			pr_info("%s: dev %d - buffer_pool[%d] is not in use\n",
				__func__, handle, r_index);
			break;
		}

		pr_debug("%s: dev %d - return buffer_pool[%d]\n",
			 __func__, handle, r_index);
		memset(handles_local[handle].buf_pool[r_index].pool,
		       0, handles_local[handle].buf_pool[r_index].size);
		handles_local[handle].buf_pool[r_index].in_use = 0;

		return 0;

	default:
		pr_info("%s: wrong control\n", __func__);
		break;
	}

	return TFA_ERROR;
}
#endif /* TFADSP_DSP_BUFFER_POOL */

/*
 * lookup the device type and return the family type
 */
int tfa98xx_dev2family(int dev_type)
{
	/* only look at the die ID part (lsb byte) */
	switch (dev_type & 0xff) {
	case 0x12:
	case 0x80:
	case 0x81:
	case 0x91:
	case 0x92:
	case 0x97:
	case 0x96:
		return 1;
	case 0x88:
	case 0x72:
		return 2;
	case 0x50:
		return 3;
	default:
		return 0;
	}
}

int tfa98xx_dev_family(tfa98xx_handle_t dev_idx)
{
	return handles_local[dev_idx].tfa_family;
}

unsigned short tfa98xx_dev_revision(tfa98xx_handle_t dev_idx)
{
	return handles_local[dev_idx].rev;
}

void tfa98xx_set_spkr_select(tfa98xx_handle_t dev_idx, char *configuration)
{
	 char first_letter;

	/* 4=Left, 2=Right, 1=none, 0=default */
	if (configuration == NULL) {
		handles_local[dev_idx].spkr_select = 0;
	} else {
		first_letter = (char)tolower((unsigned char)configuration[0]);
		switch (first_letter) {
		case 'b': /* SC / both -> apply primary to secondary */
			handles_local[dev_idx].spkr_select = 8;
			handles_local[dev_idx].spkr_count = 2;
			break;
		case 'l':
		case 'p': /* DS / left -> only use primary channel */
			handles_local[dev_idx].spkr_select = 4;
			handles_local[dev_idx].spkr_count = 1;
			break;
		case 'r':
		case 's': /* DP / right -> only use secondary channel */
			handles_local[dev_idx].spkr_select = 2;
			handles_local[dev_idx].spkr_count = 1;
			break;
		case 'd': /* DC / disable -> skip applying to both */
			handles_local[dev_idx].spkr_select = 1;
			handles_local[dev_idx].spkr_count = 2;
			break;
		default:
			handles_local[dev_idx].spkr_select = 0;
			handles_local[dev_idx].spkr_count = 2;
			break;
		}
	}
}

void tfa_mock_probe(int dev_idx, unsigned short revid, int slave_address)
{
	handles_local[dev_idx].slave_address = (unsigned char)slave_address*2;
	handles_local[dev_idx].rev = revid;
	tfa_set_query_info(dev_idx);
}

enum tfa98xx_error
tfa_soft_probe(int dev_idx, int revid)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	error = tfa_cont_get_slave
		(dev_idx, &handles_local[dev_idx].slave_address);
	handles_local[dev_idx].slave_address *= 2;
	if (error)
		return error;

	handles_local[dev_idx].rev = (unsigned short)revid;
	tfa_set_query_info(dev_idx);

	return error;
}

/*
 * TODO The slave/cnt check in tfa98xx_register_dsp()
 * should be done here in tfa_probe()
 */
enum tfa98xx_error
tfa_probe(unsigned char slave_address, tfa98xx_handle_t *p_handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int rev;
	int idx;

	_ASSERT(p_handle != NULL);
	*p_handle = -1;

	/* when available select index used in container file */
	idx = tfa98xx_cnt_slave2idx(slave_address >> 1);
	if (idx < 0)
		idx = 0; /* when no container file, use first instance */

	if (handles_local[idx].in_use == 1)
		return TFA98XX_ERROR_IN_USE;

	handles_local[idx].in_use = 1;

	switch (slave_address) {
	case TFA98XX_GENERIC_SLAVE_ADDRESS: /* same as (0x0E << 1) test adr */
	case 0x68:
	case 0x6A:
	case 0x6C:
	case 0x6E:
	case (0x1a << 1):
	/* TODO implement foreign i2c addressing like uda1355 */
		handles_local[idx].buffer_size =  NXP_I2C_BufferSize();
		handles_local[idx].slave_address = slave_address;
		/* this can be the very first read, so check error here */
		rev = TFA_READ_REG(idx, REV);
		if (rev < 0) /* returns negative if error */
			error = -rev;
		if (error != TFA98XX_ERROR_OK) {
			handles_local[idx].in_use = 0;
			pr_debug("Error: Unable to read revid from slave:0x%02x\n",
				slave_address / 2);
			return error;
		}
		handles_local[idx].rev = (unsigned short) rev;
		*p_handle = idx;
		error = TFA98XX_ERROR_OK;
#ifdef __KERNEL__ /* don't spam userspace with information */
		tfa98xx_trace_printk("slave:0x%02x revid:0x%04x\n",
			slave_address, rev);
		pr_debug("slave:0x%02x revid:0x%04x\n", slave_address, rev);
#endif
		break;
	default:
		pr_info("Unknown slave address!\n");
		/* wrong slave address */
		error = TFA98XX_ERROR_BAD_PARAMETER;
	}

	tfa_set_query_info(idx);
	handles_local[idx].in_use = 0;

	return error;
}

enum tfa98xx_error
tfa98xx_open(tfa98xx_handle_t handle)
{
	if (tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_IN_USE;

	handles_local[handle].in_use = 1;
	return TFA98XX_ERROR_OK;
}

/*
 * close
 */
enum tfa98xx_error tfa98xx_close(tfa98xx_handle_t handle)
{
	if (tfa98xx_handle_is_open(handle)) {
		handles_local[handle].in_use = 0;
		return TFA98XX_ERROR_OK;
	}

	return TFA98XX_ERROR_NOT_OPEN;
}

/*
 * return the target address for the filter on this device
 *
 * filter_index:
 *	[0..9] reserved for EQ (not deployed, calc. is available)
 *	[10..12] anti-alias filter
 *	[13] integrator filter
 */
enum tfa98xx_dmem
tfa98xx_filter_mem(tfa98xx_handle_t dev,
	int filter_index, unsigned short *address, int channel)
{
	enum tfa98xx_dmem dmem = -1;
	int idx;
	unsigned short bq_table[7][4] = {
	/* index: 10, 11, 12, 13 */
			{ 346,  351,  356,  288}, /* 87 BRA_MAX_MRA4-2_7.00 */
			{ 346,  351,  356,  288}, /* 90 BRA_MAX_MRA6_9.02 */
			{ 467,  472,  477,  409}, /* 95 BRA_MAX_MRA7_10.02 */
			{ 406,  411,  416,  348}, /* 97 BRA_MAX_MRA9_12.01 */
			{ 467,  472,  477,  409}, /* 91 BRA_MAX_MRAA_13.02 */
			{8832, 8837, 8842, 8847}, /* 88 part1 */
			{8853, 8858, 8863, 8868}  /* 88 part2 */
			/* Since the 88 is stereo we have 2 parts.
			 * Every index has 5 values except index 13
			 * this one has 6 values
			 */
	};

	if ((filter_index >= 10) && (filter_index <= 13)) {
		dmem = TFA98XX_DMEM_YMEM; /* for all devices */
		idx = filter_index - 10;

		switch (handles_local[dev].rev & 0xff) {
		/* only compare lower byte */
		case 0x12:
			*address = bq_table[2][idx];
			break;
		case 0x97:
			*address = bq_table[3][idx];
			break;
		case 0x96:
			*address = bq_table[3][idx];
			break;
		case 0x80:
		case 0x81: /* for the RAM version */
		case 0x91:
			*address = bq_table[1][idx];
			break;
		case 0x92:
			*address = bq_table[4][idx];
			break;
		case 0x88:
			/* Channel 1 = primary, 2 = secondary */
			if (channel == 1)
				*address = bq_table[5][idx];
			else
				*address = bq_table[6][idx];
			break;
		case 0x72:
		default:
			/* unsupported case, possibly intermediate version */
			return TFA98XX_DMEM_ERR;
			_ASSERT(0);
		}
	}
	return dmem;
}

/************************ query functions *******************************/
/* no device involved */
/**
 * return revision
 */
void tfa98xx_rev(int *major, int *minor, int *revision)
{
	char version_str[] = TFA98XX_API_REV_STR;
	char residual[20] = {'\0'};
	int ret;

	ret = sscanf(version_str, "v%d.%d.%d-%s",
		major, minor, revision, residual);
	if (ret != 4)
		pr_err("%s: failure reading API revision", __func__);
}

/**
 * Return the maximum nr of devices (SC39786)
 * TODO get from cnt: now only called from contOpen
 */
int tfa98xx_max_devices(void)
{
	return MAX_HANDLES;
}

/* return the device revision id
 */
unsigned short tfa98xx_get_device_revision(tfa98xx_handle_t handle)
{
	/* local function. Caller must make sure handle is valid */
	return handles_local[handle].rev;
}

/**
 * return the device digital audio interface (DAI) type bitmap
 */
enum tfa98xx_dai_bitmap tfa98xx_get_device_dai(tfa98xx_handle_t handle)
{
	/* local function. Caller must make sure handle is valid */
	return handles_local[handle].daimap;
}

/**
 * tfa98xx_supported_speakers
 * returns the number of the supported speaker count
 */
enum tfa98xx_error
tfa98xx_supported_speakers(tfa98xx_handle_t handle, int *spkr_count)
{
	*spkr_count = handles_local[handle].spkr_count;

	if ((handles_local[handle].rev & 0xff) == 0x72) {
		if (tfa98xx_cnt_max_device() > 1)
			*spkr_count = 2;
		else
			*spkr_count = 1;
	}

	return TFA98XX_ERROR_OK;
}

/**
 * tfa98xx_supported_dai
 * returns the bitmap of the supported Digital Audio Interfaces
 */
enum tfa98xx_error
tfa98xx_supported_dai(tfa98xx_handle_t handle,
	enum tfa98xx_dai_bitmap *daimap)
{
	if (tfa98xx_handle_is_open(handle))
		*daimap = handles_local[handle].daimap;
	else
		return TFA98XX_ERROR_NOT_OPEN;

	return TFA98XX_ERROR_OK;
}

/*
 * tfa98xx_supported_saam
 * returns the supportedspeaker as microphone feature
 */
enum tfa98xx_error
tfa98xx_supported_saam(tfa98xx_handle_t handle,
	enum tfa98xx_saam *saam)
{
	int features;
	enum tfa98xx_error error;

	if (handles_local[handle].support_saam == SUPPORT_NOT_SET) {
		error = tfa98xx_dsp_get_hw_feature_bits(handle, &features);
		if (error != TFA98XX_ERROR_OK)
			return error;
		handles_local[handle].support_saam =
			(features & 0x8000) ? SUPPORT_YES : SUPPORT_NO;
		/* SAAM is bit15 */
	}
	*saam = handles_local[handle].support_saam == SUPPORT_YES ?
		TFA98XX_SAAM : TFA98XX_SAAM_NONE;

	return TFA98XX_ERROR_OK;
}

/*
 * tfa98xx_set_saam_use_case
 * sets use case of saam: 0: not in use, 1: RaM / SaM only, 2: bidirectional
 */
enum tfa98xx_error tfa98xx_set_saam_use_case(int samstream)
{
	int dev, devcount = tfa98xx_cnt_max_device();

	pr_info("%s: set saam_use_case=%d\n", __func__, samstream);

	if (devcount < 1) {
		pr_err("No or wrong container file loaded\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	for (dev = 0; dev < devcount; dev++)
		handles_local[dev].saam_use_case = samstream;

	return TFA98XX_ERROR_OK;
}

/*
 * tfa98xx_set_stream_state
 * sets the stream: b0: pstream (Rx), b1: cstream (Tx), b2: samstream (SaaM)
 */
enum tfa98xx_error tfa98xx_set_stream_state(int stream_state)
{
	int dev, devcount = tfa98xx_cnt_max_device();

	pr_debug("%s: set stream_state=0x%04x\n", __func__, stream_state);

	if (devcount < 1) {
		pr_err("No or wrong container file loaded\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	for (dev = 0; dev < devcount; dev++)
		handles_local[dev].stream_state = stream_state;

	return TFA98XX_ERROR_OK;
}

/*
 * tfa98xx_compare_features
 * obtains features_from_MTP and features_from_cnt
 */
enum tfa98xx_error
tfa98xx_compare_features(tfa98xx_handle_t handle,
	int features_from_MTP[3], int features_from_cnt[3])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint32_t value;
	uint16_t mtpbf;
	unsigned char bytes[3 * 2];

	/* int sw_feature_bits[2]; // cached feature bits data */
	/* int hw_feature_bits; // cached feature bits data */

	/* Set proper MTP location per device: */
	if (tfa98xx_dev_family(handle) == 1)
		mtpbf = 0x850f; /* MTP5 for tfa1,16 bits */
	else
		mtpbf = 0xf907; /* MTP9 for tfa2, 8 bits */

	/* Read HW features from MTP: */
	value = tfa_read_reg(handle, mtpbf) & 0xffff;
	features_from_MTP[0] = handles_local[handle].hw_feature_bits = value;

	/* Read SW features: */
	error = tfa_dsp_cmd_id_write_read(handle, MODULE_FRAMEWORK,
		FW_PAR_ID_GET_FEATURE_INFO, sizeof(bytes), bytes);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* old ROM code may respond with TFA98XX_ERROR_RPC_PARAM_ID */
	tfa98xx_convert_bytes2data(sizeof(bytes), bytes, &features_from_MTP[1]);

	/* check if feature bits from MTP match feature bits from cnt file: */
	get_hw_features_from_cnt(handle, &features_from_cnt[0]);
	get_sw_features_from_cnt(handle, &features_from_cnt[1]);

	return error;
}

/*************************** device specific ops ***************************/
/* the wrapper for DspReset, in case of full */
enum tfa98xx_error tfa98xx_dsp_reset(tfa98xx_handle_t dev_idx, int state)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (tfa98xx_handle_is_open(dev_idx)) {
		if (handles_local[dev_idx].dev_ops.tfa_dsp_reset)
			error = (*handles_local[dev_idx]
				.dev_ops.tfa_dsp_reset)(dev_idx, state);
		else
			/* generic function */
			TFA_SET_BF_VOLATILE(dev_idx, RST, (uint16_t)state);
	}

	return error;
}

/* tfa98xx_dsp_system_stable
 * return: *ready = 1 when clocks are stable to allow DSP subsystem access
 * This is the clean, default static
 */
static enum tfa98xx_error _tfa98xx_dsp_system_stable(tfa98xx_handle_t handle,
						int *ready)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short status;
	int value;

	/* check the contents of the STATUS register */
	value = TFA_READ_REG(handle, AREFS);
	if (value < 0) {
		error = -value;
		*ready = 0;
		_ASSERT(error);		/* an error here can be fatal */
		return error;
	}
	status = (unsigned short)value;

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(handle, AREFS, status) == 0)
		|| (TFA_GET_BF_VALUE(handle, CLKS, status) == 0));

	return error;
}

/* deferred calibration */
void tfa98xx_apply_deferred_calibration(tfa98xx_handle_t handle)
{
	struct tfa98xx_controls *controls =
		&(handles_local[handle].dev_ops.controls);
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	unsigned short value;

	if (controls->otc.deferrable && controls->otc.triggered) {
		pr_debug("Deferred writing otc = %d\n", controls->otc.wr_value);
		err = tfa98xx_set_mtp(handle,
			(uint16_t)controls->otc.wr_value
			<< TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS,
			1 << TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS);
		if (err != TFA98XX_ERROR_OK) {
			pr_err("Unable to apply deferred MTP OTC write. Error=%d\n",
				err);
		} else {
			controls->otc.triggered = false;
			controls->otc.rd_valid = true;
			err = tfa98xx_get_mtp(handle, &value);
			value = (value
				& TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK)
				>> TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS;
			if (err == TFA98XX_ERROR_OK)
				controls->otc.rd_value = value;
			else
				controls->otc.rd_value = controls->otc.wr_value;
		}
	}

	if (controls->mtpex.deferrable && controls->mtpex.triggered) {
		pr_debug("Deferred writing mtpex = %d\n",
			controls->mtpex.wr_value);
		err = tfa98xx_set_mtp(handle,
			(uint16_t)controls->mtpex.wr_value
			<< TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS,
			1 << TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS);
		if (err != TFA98XX_ERROR_OK) {
			pr_err("Unable to apply deferred MTPEX write. Rrror=%d\n",
									err);
		} else {
			controls->mtpex.triggered = false;
			controls->mtpex.rd_valid = true;
			err = tfa98xx_get_mtp(handle, &value);
			value = (value
				& TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK)
				>> TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS;
			if (err == TFA98XX_ERROR_OK)
				controls->mtpex.rd_value = value;
			else
				controls->mtpex.rd_value =
					controls->mtpex.wr_value;
		}
	}

	if (controls->calib.triggered) {
		err = tfa_calibrate(handle);
		controls->calib.wr_value = true;
		if (err) {
			pr_info("Deferred calibration failed: %d\n", err);
		} else {
			pr_debug("Deferred calibration ok\n");
			controls->calib.triggered = false;
		}
	}
}

/* the ops wrapper for tfa98xx_dsp_SystemStable */
enum tfa98xx_error
tfa98xx_dsp_system_stable(tfa98xx_handle_t dev_idx, int *ready)
{
	enum tfa98xx_error error;

	if (!tfa98xx_handle_is_open(dev_idx))
		return TFA98XX_ERROR_NOT_OPEN;

	if (handles_local[dev_idx].dev_ops.tfa_dsp_system_stable)
		error = (*handles_local[dev_idx]
			.dev_ops.tfa_dsp_system_stable)(dev_idx, ready);
	else
		/* generic function */
		error = _tfa98xx_dsp_system_stable(dev_idx, ready);
	return error;
}

/* the ops wrapper for tfa98xx_dsp_SystemStable */
int tfa98xx_cf_enabled(tfa98xx_handle_t dev_idx)
{
	if (!tfa98xx_handle_is_open(dev_idx))
		return TFA98XX_ERROR_NOT_OPEN;

	return TFA_GET_BF(dev_idx, CFE);
}


/*
 * bring the device into a state similar to reset
 */
enum tfa98xx_error tfa98xx_init(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t value = 0;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	/* reset all i2C registers to default
	 * Write the register directly to avoid read in the bitfield function.
	 * The I2CR bit may overwrite full register because it is reset anyway.
	 * This will save a reg read transaction.
	 */
	TFA_SET_BF_VALUE(handle, I2CR, 1, &value);
	TFA_WRITE_REG(handle, I2CR, value);

	if (tfa98xx_dev_family(handle) == 2) {
		/* restore MANSCONF and MANCOLD to POR state */
		TFA_SET_BF_VOLATILE(handle, MANSCONF, 0);
		TFA_SET_BF_VOLATILE(handle, MANCOLD, 1);
	} else {
		/* Mark TFA1 family chips OTC and MTPEX calibration accesses
		 * as deferrable, since these registers cannot be accesed
		 * while the I2S link is not up and running
		 */
		handles_local[handle].dev_ops.controls.otc.deferrable = true;
		handles_local[handle].dev_ops.controls.mtpex.deferrable = true;
	}
	/* Put DSP in reset */
	tfa98xx_dsp_reset(handle, 1); /* in pair of tfa_run_start_dsp() */

	/* some other registers must be set for optimal amplifier behaviour
	 * This is implemented in a file specific for the type number
	 */

	if (handles_local[handle].dev_ops.tfa_init)
		error = (*handles_local[handle].dev_ops.tfa_init)(handle);
	/* TODO warning here? */

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_tables(tfa98xx_handle_t handle, int sample_rate)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	if (handles_local[handle].dev_ops.tfa_dsp_write_tables)
		error = (*handles_local[handle]
			.dev_ops.tfa_dsp_write_tables)(handle, sample_rate);

	return error;
}

/********************* new tfa2 ***********************************/
/* newly added messaging for tfa2 tfa1? */
enum tfa98xx_error
tfa98xx_dsp_get_memory(tfa98xx_handle_t handle, int memory_type,
	int offset, int length, unsigned char bytes[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	char msg[12];

	msg[0] = 8;
	msg[1] = MODULE_FRAMEWORK + 128;
	msg[2] = FW_PAR_ID_GET_MEMORY;

	msg[3] = 0;
	msg[4] = 0;
	msg[5] = (char)memory_type;

	msg[6] = 0;
	msg[7] = (offset >> 8) & 0xff;
	msg[8] = offset & 0xff;

	msg[9] = 0;
	msg[10] = (length >> 8) & 0xff;
	msg[11] = length & 0xff;

	/* send msg */
	error = dsp_msg(handle, sizeof(msg), (char *)msg);

	if (error != TFA98XX_ERROR_OK)
		return error;

	/* read the data from the device (length * 3 = words) */
	error = dsp_msg_read(handle, length * 3, bytes);

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_set_memory(tfa98xx_handle_t handle, int memory_type,
	int offset, int length, int value)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	char msg[15];

	msg[0] = 8;
	msg[1] = MODULE_FRAMEWORK + 128;
	msg[2] = FW_PAR_ID_SET_MEMORY;

	msg[3] = 0;
	msg[4] = 0;
	msg[5] = (char)memory_type;

	msg[6] = 0;
	msg[7] = (offset >> 8) & 0xff;
	msg[8] = offset & 0xff;

	msg[9] = 0;
	msg[10] = (length >> 8) & 0xff;
	msg[11] = length & 0xff;

	msg[12] = (value >> 16) & 0xff;
	msg[13] = (value >> 8) & 0xff;
	msg[14] = value & 0xff;

	/* send msg */
	error = dsp_msg(handle, sizeof(msg), (char *)msg);

	return error;
}
/****************************** calibration support **************************/
/*
 * get/set the mtp with user controllable values
 * check if the relevant clocks are available
 */
enum tfa98xx_error tfa98xx_get_mtp(tfa98xx_handle_t handle, uint16_t *value)
{
	int result;

	/* not possible if PLL in powerdown */
	if (TFA_GET_BF(handle, PWDN)) {
		pr_debug("PLL in powerdown\n");
		return TFA98XX_ERROR_NO_CLOCK;
	}
	result = TFA_READ_REG(handle, MTP0);
	if (result < 0)
		return -result;
	*value = (uint16_t)result;

	return TFA98XX_ERROR_OK;
}

/*
 * lock or unlock KEY2
 *  lock = 1 will lock
 *  lock = 0 will unlock
 * note that on return all the hidden key will be off
 */
void tfa98xx_key2(tfa98xx_handle_t handle, int lock)
{
	/* unhide lock registers */
	reg_write(handle, (tfa98xx_dev_family(handle) == 1) ?
		0x40 : 0x0F, 0x5A6B);
	/* lock/unlock key2 MTPK */
	TFA_WRITE_REG(handle, MTPKEY2, lock ? 0 : 0x5A);
	/* unhide lock registers */
	reg_write(handle, (tfa98xx_dev_family(handle) == 1) ? 0x40 : 0x0F, 0);
}

enum tfa98xx_error
tfa98xx_set_mtp(tfa98xx_handle_t handle,
	uint16_t value, uint16_t mask)
{
	unsigned short mtp_old, mtp_new;
	int loop;
	enum tfa98xx_error error;

	error = tfa98xx_get_mtp(handle, &mtp_old);

	if (error != TFA98XX_ERROR_OK)
		return error;

	mtp_new = (value & mask) | (mtp_old & ~mask);

	if (mtp_old == mtp_new) /* no change */ {
		if (tfa98xx_runtime_verbose)
			pr_info("%s: no change in MTP. no value written!\n",
				__func__);
		return TFA98XX_ERROR_OK;
	}
	tfa98xx_key2(handle, 0); /* unlock */
	TFA_WRITE_REG(handle, MTP0, mtp_new);	/* write to i2c shadow reg */
	/* CIMTP=1 start copying all the data from i2c regs_mtp to mtp */
	TFA_SET_BF(handle, CIMTP, 1);
	/* no check for MTPBUSY here, i2c delay assumed to be enough */
	tfa98xx_key2(handle, 1); /* lock */

	/* wait until MTP write is done
	 */
	for (loop = 0; loop < 100 /*x10ms*/; loop++) {
		msleep_interruptible(10); /* wait 10ms to avoid busload */
		if (TFA_GET_BF(handle, MTPB) == 0)
			return TFA98XX_ERROR_OK;
	}

	return TFA98XX_ERROR_STATE_TIMED_OUT;
}
/*
 * clear mtpex
 * set ACS
 * start tfa
 */
enum tfa98xx_error tfa_calibrate(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error;

	/* clear mtpex */
	error = tfa98xx_set_mtp(handle, 0,
				TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
	if (error)
		return error;

	/* set RST=1 to trigger recalibration */
	TFA_SET_BF(handle, RST, 1);

	/* set ACS/coldboot state */
	handles_local[handle].is_cold = 1;
	error = tfa_run_coldboot(handle, 1);

	/* start tfa by playing */
	return error;
}

static short twos(short x)
{
	return (x < 0) ? x + 512 : x;
}

void tfa98xx_set_exttemp(tfa98xx_handle_t handle, short ext_temp)
{
	if ((-256 <= ext_temp) && (ext_temp <= 255)) {
		/* make twos complement */
		pr_debug("Using ext temp %d C\n", twos(ext_temp));
		TFA_SET_BF(handle, TROS, 1);
		TFA_SET_BF(handle, EXTTS, twos(ext_temp));
	} else {
		pr_debug("Clearing ext temp settings\n");
		TFA_SET_BF(handle, TROS, 0);
	}
}

short tfa98xx_get_exttemp(tfa98xx_handle_t handle)
{
	short ext_temp = (short)TFA_GET_BF(handle, EXTTS);

	return twos(ext_temp);
}

/************* tfa simple bitfield interfacing ********************/
/* convenience functions */
enum tfa98xx_error
tfa98xx_set_volume_level(tfa98xx_handle_t handle, unsigned short vol)
{
	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	if (vol > 255)	/* restricted to 8 bits */
		vol = 255;

	/* 0x00 -> 0.0 dB
	 * 0x01 -> -0.5 dB
	 * ...
	 * 0xFE -> -127dB
	 * 0xFF -> muted
	 */

	/* volume value is in the top 8 bits of the register */
	return -TFA_SET_BF(handle, VOL, (uint16_t)vol);
}

static enum tfa98xx_error
tfa98xx_set_mute_tfa2(tfa98xx_handle_t handle, enum tfa98xx_mute mute)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	switch (mute) {
	case TFA98XX_MUTE_OFF:
		TFA_SET_BF(handle, INTSMUTE, 0);

		TFA_SET_BF(handle, CFSMR, 0);
		TFA_SET_BF(handle, CFSML, 0);
		break;
	case TFA98XX_MUTE_AMPLIFIER:
	case TFA98XX_MUTE_DIGITAL:
		TFA_SET_BF(handle, INTSMUTE, 1);

		TFA_SET_BF(handle, CFSMR, 1);
		TFA_SET_BF(handle, CFSML, 1);
		break;
	default:
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return error;
}

static enum tfa98xx_error
tfa98xx_set_mute_tfa1(tfa98xx_handle_t handle, enum tfa98xx_mute mute)
{
	enum tfa98xx_error error;
	unsigned short audioctrl_value;
	unsigned short sysctrl_value;
	int value;

	value = TFA_READ_REG(handle, CFSM); /* audio control register */
	if (value < 0)
		return -value;
	audioctrl_value = (unsigned short)value;
	value = TFA_READ_REG(handle, AMPE); /* system control register */
	if (value < 0)
		return -value;
	sysctrl_value = (unsigned short)value;

	switch (mute) {
	case TFA98XX_MUTE_OFF:
		/* previous state can be digital or amplifier mute,
		 * clear the cf_mute and set the enbl_amplifier bits
		 *
		 * To reduce PLOP at power on it is needed to switch the
		 * amplifier on with the DCDC in follower mode
		 * (enbl_boost = 0 ?).
		 * This workaround is also needed when toggling the
		 * powerdown bit!
		 */
		TFA_SET_BF_VALUE(handle, CFSM, 0, &audioctrl_value);
		TFA_SET_BF_VALUE(handle, AMPE, 1, &sysctrl_value);
		TFA_SET_BF_VALUE(handle, DCA, 1, &sysctrl_value);
		break;
	case TFA98XX_MUTE_DIGITAL:
		/* expect the amplifier to run */
		/* set the cf_mute bit */
		TFA_SET_BF_VALUE(handle, CFSM, 1, &audioctrl_value);
		/* set the enbl_amplifier bit */
		TFA_SET_BF_VALUE(handle, AMPE, 1, &sysctrl_value);
		/* clear active mode */
		TFA_SET_BF_VALUE(handle, DCA, 0, &sysctrl_value);
		break;
	case TFA98XX_MUTE_AMPLIFIER:
		/* clear the cf_mute bit */
		TFA_SET_BF_VALUE(handle, CFSM, 0, &audioctrl_value);
		/* clear the enbl_amplifier bit and active mode */
		TFA_SET_BF_VALUE(handle, AMPE, 0, &sysctrl_value);
		TFA_SET_BF_VALUE(handle, DCA, 0, &sysctrl_value);
		break;
	default:
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	error = -TFA_WRITE_REG(handle, CFSM, audioctrl_value);
	if (error)
		return error;
	error = -TFA_WRITE_REG(handle, AMPE, sysctrl_value);
	return error;
}

enum tfa98xx_error
tfa98xx_set_mute(tfa98xx_handle_t handle, enum tfa98xx_mute mute)
{
	if (!tfa98xx_handle_is_open(handle)) {
		pr_err("device with handle %d is not opened\n", (int)handle);
		return TFA98XX_ERROR_NOT_OPEN;
	}

	if (tfa98xx_dev_family(handle) == 1)
		return tfa98xx_set_mute_tfa1(handle, mute);
	else
		return tfa98xx_set_mute_tfa2(handle, mute);
}

/****************** patching **************************/
static enum tfa98xx_error
tfa98xx_process_patch_file(tfa98xx_handle_t handle, int length,
	const unsigned char *bytes)
{
	unsigned short size;
	int index = 0;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	while (index < length) {
		size = bytes[index] + bytes[index + 1] * 256;
		index += 2;
		if ((index + size) > length) {
			/* outside the buffer, error in the input data */
			return TFA98XX_ERROR_BAD_PARAMETER;
		}

		if (size > handles_local[handle].buffer_size) {
			/* too big, must fit buffer */
			return TFA98XX_ERROR_BAD_PARAMETER;
		}

		error = tfa98xx_write_raw(handle, size, &bytes[index]);
		if (error != TFA98XX_ERROR_OK)
			break;
		index += size;
	}

	return error;
}

/* the patch contains a header with the following
 * IC revision register: 1 byte, 0xFF means don't care
 * XMEM address to check: 2 bytes, big endian, 0xFFFF means don't care
 * XMEM value to expect: 3 bytes, big endian
 */
static enum tfa98xx_error
tfa98xx_check_ic_rom_version(tfa98xx_handle_t handle,
	const unsigned char patchheader[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short checkrev, revid;
	unsigned char lsb_revid;
	unsigned short checkaddress;
	int checkvalue;
	int value = 0;

	checkrev = patchheader[0];
	lsb_revid = handles_local[handle].rev & 0xff;
	/* only compare lower byte */

	if ((checkrev != 0xFF) && (checkrev != lsb_revid))
		return TFA98XX_ERROR_NOT_SUPPORTED;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue =
		(patchheader[3] << 16) + (patchheader[4] << 8) + patchheader[5];
	if (checkaddress != 0xFFFF) {
		/* read register to check the correct ROM version */
		if (error == TFA98XX_ERROR_OK) {
			error =
			mem_read(handle, checkaddress, 1, &value);
		}
		if (error == TFA98XX_ERROR_OK) {
			if (value != checkvalue) {
				pr_err("patch file romid type check failed [0x%04x]: expected 0x%02x, actual 0x%02x\n",
					checkaddress, value, checkvalue);
				error = TFA98XX_ERROR_NOT_SUPPORTED;
			}
		}
	} else { /* == 0xffff */
		/* check if the revid subtype is in there */
		if (checkvalue != 0xFFFFFF && checkvalue != 0) {
			revid = patchheader[5] << 8 | patchheader[0];
			/* full revid */
			if (revid != handles_local[handle].rev) {
				pr_err("patch file device type check failed: expected 0x%02x, actual 0x%02x\n",
					handles_local[handle].rev, revid);
				return TFA98XX_ERROR_NOT_SUPPORTED;
			}
		}
	}

	return error;
}

#define PATCH_HEADER_LENGTH 6
enum tfa98xx_error
tfa_dsp_patch(tfa98xx_handle_t handle, int patch_length,
	const unsigned char *patch_bytes)
{
	enum tfa98xx_error error;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;
	if (patch_length < PATCH_HEADER_LENGTH)
		return TFA98XX_ERROR_BAD_PARAMETER;
	error = tfa98xx_check_ic_rom_version(handle, patch_bytes);
	if (error != TFA98XX_ERROR_OK)
		return error;

	error = tfa98xx_process_patch_file
		(handle, patch_length - PATCH_HEADER_LENGTH,
		patch_bytes + PATCH_HEADER_LENGTH);

	return error;
}
/****************** end patching ******************/

TFA_INTERNAL enum tfa98xx_error
tfa98xx_wait_result(tfa98xx_handle_t handle, int wait_retry_count)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int cf_status; /* the contents of the CF_STATUS register */
	int tries = 0;

	do {
		cf_status = TFA_GET_BF(handle, ACK);
		if (cf_status < 0)
			error = -cf_status;
		tries++;
		/* i2c_cmd_ack */
		/* don't wait forever, DSP is pretty quick to respond (< 1ms) */
	} while ((error == TFA98XX_ERROR_OK)
		&& ((cf_status & CF_STATUS_I2C_CMD_ACK) == 0)
		&& (tries < wait_retry_count));
	if (tries >= wait_retry_count) {
		/* something wrong with communication with DSP */
		error = TFA98XX_ERROR_DSP_NOT_RUNNING;
	}
	return error;
}

/*
 * support functions for data conversion
 * convert memory bytes to signed 24 bit integers
 *	input:  bytes contains "num_bytes" byte elements
 *	output: data contains "num_bytes/3" int24 elements
 */
void tfa98xx_convert_bytes2data(int num_bytes,
	const unsigned char bytes[], int data[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	int num_data = num_bytes / 3;

	_ASSERT((num_bytes % 3) == 0);
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}

/**
 * convert signed 24 bit integers to 32bit aligned bytes
 * input:   data contains "num_bytes/3" int24 elements
 * output:  bytes contains "num_bytes" byte elements
 */
void tfa98xx_convert_data2bytes(int num_data, const int data[],
	unsigned char bytes[])
{
	int i; /* index for data */
	int k; /* index for bytes */
	int d;

	/* note: cannot just take the lowest 3 bytes from the 32 bit
	 * integer, because also need to take care of clipping any
	 * value > 2&23
	 */
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		if (data[i] >= 0)
			d = MIN(data[i], (1 << 23) - 1);
		else {
			/* 2's complement */
			d = (1 << 24) - MIN(-data[i], 1 << 23);
		}
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		bytes[k] = (d >> 16) & 0xFF;	/* MSB */
		bytes[k + 1] = (d >> 8) & 0xFF;
		bytes[k + 2] = (d) & 0xFF;	/* LSB */
	}
}

/*
 * DSP RPC message support functions
 * depending on framework to be up and running
 * need base i2c of memaccess (tfa1=0x70/tfa2=0x90)
 */
/* write dsp messages in function tfa_dsp_msg()
 *  note the 'old' write_parameter() was more efficient
 *  because all i2c was in one burst transaction
 */

/* TODO properly handle bitfields: state should be restored! */
/* (now it will change eg dmesg field to xmem) */
enum tfa98xx_error tfa_dsp_msg_write(tfa98xx_handle_t handle,
	int length, const char *buffer)
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(handles_local[handle].buffer_size, 3);
	/* XMEM word size */
	int remaining_bytes = length;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(handle, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(handle, DMEM, (uint16_t)TFA98XX_DMEM_XMEM, &cfctl);
	/* set cf ctl to DMEM  */
	TFA_SET_BF_VALUE(handle, AIF, 0, &cfctl); /* set to autoincrement */
	TFA_WRITE_REG(handle, DMEM, cfctl);

	/* xmem[1] is start of message
	 * direct write to register to save cycles avoiding read-modify-write
	 */
	TFA_WRITE_REG(handle, MADD, 1);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address
	 */
	while ((error == TFA98XX_ERROR_OK) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data
			(handle, FAM_TFA98XX_CF_MEM,
			chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == TFA98XX_ERROR_OK) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(handle, REQCMD, 0x01, &cfctl); /* bit 0 */
		TFA_SET_BF_VALUE(handle, CFINT, 1, &cfctl);
		error = -TFA_WRITE_REG(handle, CFINT, cfctl);
	}

	return error;
}

enum tfa98xx_error
tfa_dsp_msg_write_id(tfa98xx_handle_t handle,
	int length, const char *buffer, uint8_t cmdid[3])
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(handles_local[handle].buffer_size, 3);
	/* XMEM word size */
	int remaining_bytes = length;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(handle, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(handle, DMEM, (uint16_t)TFA98XX_DMEM_XMEM, &cfctl);
	/* set cf ctl to DMEM  */
	TFA_SET_BF_VALUE(handle, AIF, 0, &cfctl); /* set to autoincrement */
	TFA_WRITE_REG(handle, DMEM, cfctl);

	/* xmem[1] is start of message
	 * direct write to register to save cycles avoiding read-modify-write
	 */
	TFA_WRITE_REG(handle, MADD, 1);

	/* write cmd-id */
	error = tfa98xx_write_data
		(handle, FAM_TFA98XX_CF_MEM, 3, (const unsigned char *)cmdid);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address
	 */
	while ((error == TFA98XX_ERROR_OK) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data
			(handle, FAM_TFA98XX_CF_MEM,
			chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == TFA98XX_ERROR_OK) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(handle, REQCMD, 0x01, &cfctl); /* bit 0 */
		TFA_SET_BF_VALUE(handle, CFINT, 1, &cfctl);
		error = -TFA_WRITE_REG(handle, CFINT, cfctl);
	}

	return error;
}

/*
 * status function used by tfa_dsp_msg() to retrieve command/msg status:
 * return a <0 status of the DSP did not ACK.
 */
enum tfa98xx_error
tfa_dsp_msg_status(tfa98xx_handle_t handle, int *p_rpc_status)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	error = tfa98xx_wait_result(handle, 2); /* 2 is only one try */
	if (error == TFA98XX_ERROR_DSP_NOT_RUNNING) {
		*p_rpc_status = -1;
		return TFA98XX_ERROR_OK;
	} else if (error != TFA98XX_ERROR_OK)
		return error;

	error = tfa98xx_check_rpc_status(handle, p_rpc_status);

	return error;
}

const char *tfa98xx_get_i2c_status_id_string(int status)
{
	const char *p_id_str;

	switch (status) {
	case TFA98XX_DSP_NOT_RUNNING:
		p_id_str = "No response from DSP";
		break;
	case TFA98XX_I2C_REQ_DONE:
		p_id_str = "Ok";
		break;
	case TFA98XX_I2C_REQ_BUSY:
		p_id_str = "Request is being processed";
		break;
	case TFA98XX_I2C_REQ_INVALID_M_ID:
		p_id_str =
			"Provided M-ID does not fit in valid range [0..2]";
		break;
	case TFA98XX_I2C_REQ_INVALID_P_ID:
		p_id_str =
			"Provided P-ID is not valid in the given M-ID context";
		break;
	case TFA98XX_I2C_REQ_INVALID_CC:
		p_id_str =
			"Invalid channel config bits (SC|DS|DP|DC) combination";
		break;
	case TFA98XX_I2C_REQ_INVALID_SEQ:
		p_id_str =
			"Invalid cmd sequence: DSP expected a specific order";
		break;
	case TFA98XX_I2C_REQ_INVALID_PARAM:
		p_id_str = "Generic error";
		break;
	case TFA98XX_I2C_REQ_BUFFER_OVERFLOW:
		p_id_str =
			"I2C buffer overflowed: host sent too many parameters";
		break;
	default:
		p_id_str = "Unspecified error";
	}

	return p_id_str;
}

enum tfa98xx_error
tfa_dsp_msg_read(tfa98xx_handle_t handle, int length, unsigned char *bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int burst_size;		/* number of words per burst size */
	int bytes_per_word = 3;
	int num_bytes;
	int offset = 0;
	unsigned short start_offset = 2; /* msg starts @xmem[2], [1]=cmd */

	if (length > TFA2_MAX_PARAM_SIZE)
		return TFA98XX_ERROR_BAD_PARAMETER;

	TFA_SET_BF(handle, DMEM, (uint16_t)TFA98XX_DMEM_XMEM);
	error = -TFA_WRITE_REG(handle, MADD, start_offset);
	if (error != TFA98XX_ERROR_OK)
		return error;

	num_bytes = length; /* input param */
	while (num_bytes > 0) {
		burst_size = ROUND_DOWN(handles_local[handle].buffer_size,
			bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;
		error = tfa98xx_read_data(handle, FAM_TFA98XX_CF_MEM,
			burst_size, bytes + offset);
		if (error != TFA98XX_ERROR_OK)
			return error;

		num_bytes -= burst_size;
		offset += burst_size;
	}

	return error;
}

void tfa_dsp_msg_register(int dev_idx, dsp_msg_t func)
{
	handles_local[dev_idx].dev_ops.dsp_msg = func;
}

void tfa_dsp_msg_read_register(int dev_idx, dsp_msg_read_t func)
{
	handles_local[dev_idx].dev_ops.dsp_msg_read = func;
}

void tfa_dsp_reg_register(int dev_idx,
	reg_read_t read_func, reg_write_t write_func)
{
	handles_local[dev_idx].dev_ops.reg_read = read_func;
	handles_local[dev_idx].dev_ops.reg_write = write_func;
}

void tfa_dsp_mem_register(int dev_idx,
	mem_read_t read_func, mem_write_t write_func)
{
	handles_local[dev_idx].dev_ops.mem_read = read_func;
	handles_local[dev_idx].dev_ops.mem_write = write_func;
}

enum tfa98xx_error dsp_msg(tfa98xx_handle_t handle, int length, const char *buf)
{
	enum tfa98xx_error error = 0;
	static int lastmessage;
	uint8_t *blob = NULL;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int blob_p_index = -1;
#endif
	int i;
	const char *tfadsp_buf;
	int tfadsp_buf_size;
#if defined(TFADSP_DEBUG)
	int n;
#endif
#if defined(TFADSP_32BITS)
	int32_t *buf32 = NULL;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int buf32_p_index = -1;
#endif
	int buf32_len;
#endif

	if (handles_local[handle].saam_use_case == 1
		|| (handles_local[handle].stream_state & BIT_PSTREAM) == 0) {
		pr_debug("%s: skip in SaaM (RaM/SaM only)\n", __func__);
		return error;
	}

	pr_debug("%s: length = %d\n", __func__, length);

	tfadsp_buf = buf;
	tfadsp_buf_size = length;

	if ((handles_local[0].ext_dsp == 1)
		&& (handles_local[handle].ext_dsp != 1)) {
		pr_debug("%s: switch handle to master device (%d to 0)\n",
			__func__, handle);
		handle = 0; /* master device only */
	}

#if defined(TFADSP_32BITS)
	/*need bits conversion: 24 bits --> 32 bits */
	buf32_len = (length/3)*4;
#if defined(TFADSP_DSP_BUFFER_POOL)
	buf32_p_index = tfa98xx_buffer_pool_access
		(handle, -1, buf32_len, POOL_GET);
	if (buf32_p_index != -1) {
		pr_debug("%s: allocated from buffer_pool[%d] for %d bytes\n",
			__func__, buf32_p_index, buf32_len);
		buf32 = (int32_t *)(handles_local[handle]
			.buf_pool[buf32_p_index].pool);
	} else {
		buf32 = kmalloc(buf32_len, GFP_KERNEL);
		if (buf32 == NULL)
			goto dsp_msg_error_exit;
	}
#else
	buf32 = kmalloc(buf32_len, GFP_KERNEL);
	if (buf32 == NULL)
		goto dsp_msg_error_exit;
#endif /* TFADSP_DSP_BUFFER_POOL */

	/*convert 24 bits -> 32 bits */
	tfa_msg24to32(buf32, buf, length);

	tfadsp_buf = (char *) buf32;
	tfadsp_buf_size = buf32_len;
#endif /* (TFADSP_32BITS) */

	if (handles_local[handle].dev_ops.dsp_msg) {
		/* Only create multi-msg when the dsp is cold */
		if (handles_local[handle].ext_dsp == 1) {
			/* Creating the multi-msg */
			lastmessage =
				tfa_tib_dsp_msgblob
				(handle, tfadsp_buf_size, tfadsp_buf);
			if (lastmessage == TFA_ERROR)
				goto dsp_msg_error_exit;

			/* When the lastmessage is done
			 * we can send the multi-msg to the target
			 */
			if (lastmessage == 1) {
				/* Get the full multi-msg data */
#if defined(TFADSP_DSP_BUFFER_POOL)
				blob_p_index =
					tfa98xx_buffer_pool_access
					(handle, -1, 64*1024, POOL_GET);
				if (blob_p_index != -1) {
					pr_debug("%s: allocated from buffer_pool[%d] - blob\n",
						__func__, blob_p_index);
					blob = (uint8_t *)
						(handles_local[handle]
						.buf_pool
						[blob_p_index].pool);
				} else {
					blob = kmalloc(64*1024,
						GFP_KERNEL);
					/* max length is 64k */
					if (blob == NULL)
						goto dsp_msg_error_exit;
				}
				tfadsp_buf_size =
					tfa_tib_dsp_msgblob
					(handle, -1,
					(const char *)blob);
#else
				blob = kmalloc(64*1024, GFP_KERNEL);
				/* max length is 64k */
				if (blob == NULL)
					goto dsp_msg_error_exit;
				tfadsp_buf_size =
					tfa_tib_dsp_msgblob
					(-1, tfadsp_buf_size,
					(const char *)blob);
#endif /* TFADSP_DSP_BUFFER_POOL */

				if (tfa98xx_runtime_verbose)
					pr_debug("Last message for the multi-message received. Multi-message length=%d\n",
						length);

#if defined(TFADSP_DSP_MSG_APR_PACKET_STRATEGY)
				error = dsp_msg_packet(handle,
					blob, tfadsp_buf_size);
#else
				/* Send to the target selected */
				if ((handles_local[handle]
					.stream_state & BIT_PSTREAM) == 1) {
					error = (*handles_local[handle]
						.dev_ops.dsp_msg)
						(handle,
						tfadsp_buf_size,
						(const char *)
						/*tfadsp_buf*/blob);
				} else {
					pr_info("%s: skip when pstream is not active\n",
						__func__);
				}
#endif /* TFADSP_DSP_MSG_APR_PACKET_STRATEGY */

#if defined(TFADSP_DSP_BUFFER_POOL)
				if (blob_p_index != -1) {
					tfa98xx_buffer_pool_access
						(handle, blob_p_index,
						0, POOL_RETURN);
					blob_p_index = -1;
				} else {
					kfree(blob);
					blob = NULL;
				}
#else
				kfree(blob); /* Free the kmalloc blob */
				blob = NULL;
#endif /* TFADSP_DSP_BUFFER_POOL */
				lastmessage = 0;
				/* reset to be able to re-start */
			}
		} else {
			if ((handles_local[handle]
				.stream_state & BIT_PSTREAM) == 1) {
				error = (*handles_local[handle]
					.dev_ops.dsp_msg)
					(handle, tfadsp_buf_size, tfadsp_buf);
			} else {
				pr_info("%s: skip when pstream is not active\n",
					__func__);
			}
		}
	} else {
		if ((handles_local[handle].stream_state & BIT_PSTREAM) == 1) {
			error = tfa_dsp_msg
				(handle, tfadsp_buf_size, tfadsp_buf);
			/* Use default */
		} else {
			pr_info("%s: skip when pstream is not active\n",
				__func__);
		}
	}

#if defined(TFADSP_DEBUG)
#if defined(TFADSP_32BITS)
	/*show buffer: 32 bits*/
	pr_debug("%s: buf32_len = %d\n", __func__, tfadsp_buf_size);
	n = (tfadsp_buf_size/4 >= 3) ? 3 : tfadsp_buf_size/4;
	for (i = 0; i < n; i++)
		pr_debug("%s: buf32[%d] = 0x%08x\n",
			__func__, i, tfadsp_buf[i]);
#else
	/*show buffer: 24 bits*/
	pr_debug("%s: buf24_len = %d\n", __func__, tfadsp_buf_size);
	n = (tfadsp_buf_size >= 9) ? 9 : tfadsp_buf_size;
	for (i = 0; i < n; i++)
		pr_debug("%s: buf24[%d] = 0x%02x\n",
			__func__, i, tfadsp_buf[i]);
#endif
#endif

	if (error == TFA98XX_ERROR_OK)
		pr_debug("%s: OK\n", __func__);
	else {
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
		pr_info("%s: (rpc base %d) error = %d\n",
			__func__, TFA98XX_ERROR_BUFFER_RPC_BASE, error);
	}

#if defined(TFADSP_32BITS)
#if defined(TFADSP_DSP_BUFFER_POOL)
	if (buf32_p_index != -1) {
		tfa98xx_buffer_pool_access
			(handle, buf32_p_index, 0, POOL_RETURN);
		buf32_p_index = -1;
	} else {
		if (buf32 != NULL)
			kfree(buf32);
	}
#else
	if (buf32 != NULL)
		kfree(buf32);
#endif /* TFADSP_DSP_BUFFER_POOL */
#endif /* (TFADSP_32BITS) */
	if (tfa98xx_dsp_verbose) {
		pr_debug("DSP w [%d]: ", length);
		for (i = 0; i < length; i++)
			pr_debug("0x%02x ", (uint8_t)buf[i]);
		pr_debug("\n");
	}

	return error;

dsp_msg_error_exit:
	pr_err("%s: exiting due to error\n", __func__);
	if (lastmessage == TFA_ERROR)
		pr_err("%s: error in merging messages\n", __func__);
	else
		pr_err("%s: can not allocate memory\n", __func__);
#if defined(TFADSP_32BITS)
#if defined(TFADSP_DSP_BUFFER_POOL)
	if (buf32_p_index != -1) {
		tfa98xx_buffer_pool_access
			(handle, buf32_p_index, 0, POOL_RETURN);
	} else {
		kfree(buf32);
	}
#else
	kfree(buf32);
#endif /* TFADSP_DSP_BUFFER_POOL */
#endif /* (TFADSP_32BITS) */

#if defined(TFADSP_DSP_BUFFER_POOL)
	if (blob_p_index != -1) {
		tfa98xx_buffer_pool_access
			(handle, blob_p_index, 0, POOL_RETURN);
	} else {
		kfree(blob);
	}
#else
	kfree(blob); /* Free the kmalloc blob */
#endif /* TFADSP_DSP_BUFFER_POOL */

	return TFA98XX_ERROR_FAIL;
}

#if defined(TFADSP_DSP_MSG_APR_PACKET_STRATEGY)
enum tfa98xx_error dsp_msg_packet(tfa98xx_handle_t handle,
	uint8_t *blob, int tfadsp_buf_size)
{
	enum tfa98xx_error error = 0;
	uint8_t *apr_buff = NULL, *apr_buff_last = NULL;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int apr_buff_p_index = -1;
#endif
	int remaining_blob_size, tfadsp_buf_offset;
	short packet_id, packet_size;

	tfadsp_buf_offset = 0;
	remaining_blob_size = tfadsp_buf_size;
	packet_size = MAX_APR_MSG_SIZE - 4;
#if defined(TFADSP_DSP_BUFFER_POOL)
	apr_buff_p_index = tfa98xx_buffer_pool_access(handle,
		-1, MAX_APR_MSG_SIZE, POOL_GET);
	if (apr_buff_p_index != -1) {
		pr_debug("%s: allocated from buffer_pool[%d] - apr_buff\n",
			__func__, apr_buff_p_index);
		apr_buff = (uint8_t *)
			(handles_local[handle].buf_pool[apr_buff_p_index].pool);
	} else {
		apr_buff = kmalloc(MAX_APR_MSG_SIZE, GFP_KERNEL);
		if (apr_buff == NULL)
			goto dsp_msg_packet_error_exit;
	}
#else
	apr_buff = kmalloc(MAX_APR_MSG_SIZE, GFP_KERNEL);
	if (apr_buff == NULL)
		goto dsp_msg_packet_error_exit;
#endif /* TFADSP_DSP_BUFFER_POOL */

	for (packet_id = 0;
		packet_id < (int)(tfadsp_buf_size / (MAX_APR_MSG_SIZE - 4));
		packet_id++) {
		pr_debug("packet[%d]: size (%d)\n", packet_id, packet_size);
		apr_buff[0] = (uint8_t)(((packet_id + 1) >> 8) & 0xFF);
		apr_buff[1] = (uint8_t)((packet_id + 1) & 0xFF);
		apr_buff[2] = (uint8_t)((packet_size >> 8) & 0xFF);
		apr_buff[3] = (uint8_t) (packet_size & 0xFF);
		memcpy(apr_buff + 4, blob + tfadsp_buf_offset, packet_size);
		if ((handles_local[handle].stream_state & BIT_PSTREAM) == 1) {
			error = (*handles_local[handle].dev_ops.dsp_msg)
				(handle, packet_size + 4,
				(const char *)apr_buff);
		} else {
			pr_info("%s: skip dsp_msg when pstream is not active\n",
				__func__);
		}
		tfadsp_buf_offset += packet_size;
		remaining_blob_size -= packet_size;
	}

#if defined(TFADSP_DSP_BUFFER_POOL)
	if (apr_buff_p_index != -1) {
		tfa98xx_buffer_pool_access(handle,
			apr_buff_p_index, 0, POOL_RETURN);
		apr_buff_p_index = -1;
	} else {
		kfree(apr_buff);
		apr_buff = NULL;
	}
#else
	kfree(apr_buff);
	apr_buff = NULL;
#endif /* TFADSP_DSP_BUFFER_POOL */

	if (remaining_blob_size > 0) {
		packet_size = remaining_blob_size;
#if defined(TFADSP_DSP_BUFFER_POOL)
		apr_buff_p_index = tfa98xx_buffer_pool_access
			(handle, -1, packet_size + 4, POOL_GET);
		if (apr_buff_p_index != -1) {
			pr_debug("%s: allocated from buffer_pool [%d] - apr_buff_last\n",
				__func__, apr_buff_p_index);
			apr_buff_last = (uint8_t *)(handles_local[handle]
				.buf_pool[apr_buff_p_index].pool);
		} else {
			apr_buff_last = kmalloc(packet_size + 4, GFP_KERNEL);
			if (apr_buff_last == NULL)
				goto dsp_msg_packet_error_exit;
		}
#else
		apr_buff_last = kmalloc(packet_size + 4, GFP_KERNEL);
		if (apr_buff_last == NULL)
			goto dsp_msg_packet_error_exit;
#endif /* TFADSP_DSP_BUFFER_POOL */

		pr_debug("packet[%d]: size (%d) - last\n",
			packet_id, remaining_blob_size);
		apr_buff_last[0] = 0xFF;
		apr_buff_last[1] = 0xFF;
		apr_buff_last[2] = (uint8_t)((packet_size >> 8) & 0xFF);
		apr_buff_last[3] = (uint8_t)(packet_size & 0xFF);
		memcpy(apr_buff_last + 4,
			blob + tfadsp_buf_offset, remaining_blob_size);
		if ((handles_local[handle].stream_state & BIT_PSTREAM) == 1) {
			error = (*handles_local[handle].dev_ops.dsp_msg)
				(handle, packet_size + 4,
				(const char *)apr_buff_last);
		} else {
			pr_info("%s: skip dsp_msg when pstream is not active\n",
				__func__);
		}
		tfadsp_buf_offset += packet_size;
		remaining_blob_size = 0;
#if defined(TFADSP_DSP_BUFFER_POOL)
		if (apr_buff_p_index != -1) {
			tfa98xx_buffer_pool_access(handle,
				apr_buff_p_index, 0, POOL_RETURN);
			apr_buff_p_index = -1;
		} else {
			kfree(apr_buff_last);
			apr_buff_last = NULL;
		}
#else
		kfree(apr_buff_last);
		apr_buff_last = NULL;
#endif /* TFADSP_DSP_BUFFER_POOL */
	}

	return error;

dsp_msg_packet_error_exit:
	pr_err("%s: can not allocate memory\n", __func__);
#if defined(TFADSP_DSP_BUFFER_POOL)
	if (apr_buff_p_index != -1) {
		tfa98xx_buffer_pool_access
			(handle, apr_buff_p_index, 0, POOL_RETURN);
	} else {
		kfree(apr_buff);
		kfree(apr_buff_last);
	}
#else
	kfree(apr_buff);
	kfree(apr_buff_last);
#endif /* TFADSP_DSP_BUFFER_POOL */

	return TFA98XX_ERROR_FAIL;
}
#endif /* TFADSP_DSP_MSG_APR_PACKET_STRATEGY */

enum tfa98xx_error
dsp_msg_read(tfa98xx_handle_t handle, int length, unsigned char *bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned char *tfadsp_buf;
	int tfadsp_buf_size;
#if defined(TFADSP_32BITS)
	int32_t *buf32;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int buf32_p_index = -1;
#endif
	int buf32_len;
#endif
	int i;
#if defined(TFADSP_DEBUG)
	int n;
#endif

	if (handles_local[handle].saam_use_case == 1) {
		pr_info("%s: skip in SaaM (RaM / SaM only)\n",
			__func__);
		return error;
	}

	pr_debug("%s: length = %d\n", __func__, length);

	tfadsp_buf = bytes;
	tfadsp_buf_size = length;

	if ((handles_local[0].ext_dsp == 1)
		&& (handles_local[handle].ext_dsp != 1)) {
		pr_debug("%s: switch handle to master device (%d to 0)\n",
			__func__, handle);
		handle = 0; /* master device only */
	}

#if defined(TFADSP_32BITS)
	buf32_len = (length/3)*4;

#if defined(TFADSP_DSP_BUFFER_POOL)
	buf32_p_index = tfa98xx_buffer_pool_access
		(handle, -1, buf32_len, POOL_GET);
	if (buf32_p_index != -1) {
		pr_debug("%s: allocated from buffer_pool[%d] for %d bytes\n",
			__func__, buf32_p_index, buf32_len);
		buf32 = (int32_t *)(handles_local[handle]
			.buf_pool[buf32_p_index].pool);
	} else {
		buf32 = kmalloc(buf32_len, GFP_KERNEL);
		if (buf32 == NULL)
			goto dsp_msg_read_error_exit;
	}
#else
	buf32 = kmalloc(buf32_len, GFP_KERNEL);
	if (buf32 == NULL)
		goto dsp_msg_read_error_exit;
#endif /* TFADSP_DSP_BUFFER_POOL */

	tfadsp_buf = (unsigned char *) buf32;
	tfadsp_buf_size = buf32_len;
#endif /* TFADSP_32BITS */

	if ((handles_local[handle].stream_state & BIT_PSTREAM) == 1) {
		if (handles_local[handle].dev_ops.dsp_msg_read) {
			/*dsp read msg*/
			error = (*handles_local[handle].dev_ops.dsp_msg_read)(
				handle, tfadsp_buf_size, tfadsp_buf);

			if (error == TFA98XX_ERROR_OK)
				pr_debug("%s: OK\n", __func__);
			else {
				/* Get actual error code from softDSP */
				error = (enum tfa98xx_error)
					(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
				pr_info("%s: (rpc base %d) error = %d\n",
					__func__,
					TFA98XX_ERROR_BUFFER_RPC_BASE, error);
			}
		} else {
			error = tfa_dsp_msg_read
				(handle, tfadsp_buf_size, tfadsp_buf);
			/* Use default */
		}
	} else {
		pr_debug("%s: skip when pstream is not active\n",
			__func__);
	}

#if defined(TFADSP_32BITS)
	for (i = 0; i < 6; i++)
		pr_debug("%s: buf32bits[%d] = 0x%08x\n",
			__func__, i, tfadsp_buf[i]);
	/*convert 32 bits -> 24 bits */
	tfa_msg32to24(bytes, (uint8_t *)tfadsp_buf, tfadsp_buf_size);
	for (i = 0; i < 6; i++)
		pr_debug("%s: buf24bits[%d] = 0x%08x\n", __func__, i, bytes[i]);

#if defined(TFADSP_DEBUG)
	pr_debug("%s: buf32_len = %d\n",
		__func__, tfadsp_buf_size);
	n = (tfadsp_buf_size/4 >= 3) ? 3 : tfadsp_buf_size/4;
	for (i = 0; i < n; i++)
		pr_debug("%s: buf32[%d] = 0x%08x\n",
			__func__, i, tfadsp_buf[i]);

	pr_debug("%s: buf24_len = %d\n",
		__func__, length);
	 n = (length >= 9) ? 9 : length;
	for (i = 0; i < n; i++)
		pr_debug("%s: buf24[%d] = 0x%02x\n",
			__func__, i, bytes[i]);
#endif

#if defined(TFADSP_DSP_BUFFER_POOL)
	if (buf32_p_index != -1) {
		tfa98xx_buffer_pool_access
			(handle, buf32_p_index, 0, POOL_RETURN);
	} else {
		kfree(buf32);
	}
#else
	if (buf32 != NULL)
		kfree(buf32);
#endif /* TFADSP_DSP_BUFFER_POOL */
#endif /* (TFADSP_32BITS) */

	if (tfa98xx_dsp_verbose) {
		pr_debug("DSP R [%d]: ", length);
		for (i = 0; i < length; i++)
			pr_debug("0x%02x ", (uint8_t)bytes[i]);
		pr_debug("\n");
	}

	return error;

#if defined(TFADSP_32BITS)
dsp_msg_read_error_exit:
	pr_err("%s: can not allocate memory\n", __func__);
	return TFA98XX_ERROR_FAIL;
#endif /* (TFADSP_32BITS) */
}

enum tfa98xx_error
reg_read(tfa98xx_handle_t handle,
	unsigned char subaddress, unsigned short *value)
{
	enum tfa98xx_error error;

	if (handles_local[handle].dev_ops.reg_read) {
		error = (*handles_local[handle]
			.dev_ops.reg_read)(handle, subaddress, value);
		if (error != TFA98XX_ERROR_OK)
			error = (enum tfa98xx_error)
				(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
			/* Get actual error code from softDSP */
	} else {
		error = tfa98xx_read_register16(handle, subaddress, value);
		/* Use default */
	}

	return error;
}

enum tfa98xx_error
reg_write(tfa98xx_handle_t handle,
	unsigned char subaddress, unsigned short value)
{
	enum tfa98xx_error error;

	if (handles_local[handle].dev_ops.reg_write) {
		error = (*handles_local[handle]
			.dev_ops.reg_write)(handle, subaddress, value);
		if (error != TFA98XX_ERROR_OK)
			error = (enum tfa98xx_error)
				(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
			/* Get actual error code from softDSP */
	} else {
		error = tfa98xx_write_register16(handle, subaddress, value);
		/* Use default */
	}

	return error;
}

enum tfa98xx_error
mem_read(tfa98xx_handle_t handle, unsigned int start_offset,
	int num_words, int *p_values)
{
	enum tfa98xx_error error;

	if (handles_local[handle].dev_ops.mem_read) {
		error = (*handles_local[handle].dev_ops.mem_read)
			(handle, start_offset, num_words, p_values);
		if (error != TFA98XX_ERROR_OK)
			error = (enum tfa98xx_error)
				(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
			/* Get actual error code from softDSP */
	} else {
		error = tfa98xx_dsp_read_mem
			(handle, start_offset, num_words, p_values);
		/* Use default */
	}

	return error;
}

enum tfa98xx_error
mem_write(tfa98xx_handle_t handle,
	unsigned short address, int value, int memtype)
{
	enum tfa98xx_error error;

	if (handles_local[handle].dev_ops.mem_write) {
		error = (*handles_local[handle].dev_ops.mem_write)
			(handle, address, value, memtype);
		if (error != TFA98XX_ERROR_OK)
			error = (enum tfa98xx_error)
				(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
		/* Get actual error code from softDSP */
	} else {
		error = tfa98xx_dsp_write_mem_word
			(handle, address, value, memtype);
		/* Use default */
	}

	return error;
}


/*
 *  write/read raw msg functions :
 *  the buffer is provided in little endian format, each word
 *  occupying 3 bytes, length is in bytes.
 *  functions will return immediately and do not not wait for DSP response.
 */
#define MAX_WORDS (300)
enum tfa98xx_error
tfa_dsp_msg(tfa98xx_handle_t handle, int length, const char *buf)
{
	enum tfa98xx_error error;
	int tries, rpc_status = TFA98XX_I2C_REQ_DONE;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write(handle, length, buf);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* get the result from the DSP (polling) */
	for (tries = TFA98XX_WAITRESULT_NTRIES; tries > 0; tries--) {
		error = tfa_dsp_msg_status(handle, &rpc_status);
		if (error == TFA98XX_ERROR_OK
			&& rpc_status == TFA98XX_I2C_REQ_DONE)
			break;
		/* If the rpc status is a specific error we want to know it.
		 * If it is busy or not running it should retry
		 */
		if (rpc_status != TFA98XX_I2C_REQ_BUSY
			&& rpc_status != TFA98XX_DSP_NOT_RUNNING)
			break;
	}

	/* if (tfa98xx_runtime_verbose)
	 *  pr_debug("Number of tries: %d\n",
	 *   TFA98XX_WAITRESULT_NTRIES-tries);
	 */

	if (rpc_status != TFA98XX_I2C_REQ_DONE) {
		/* DSP RPC call returned an error */
		error = (enum tfa98xx_error)
			(rpc_status + TFA98XX_ERROR_BUFFER_RPC_BASE);
		pr_debug("DSP msg status: %d (%s)\n", rpc_status,
			tfa98xx_get_i2c_status_id_string(rpc_status));
	}
	return error;
}

/**
 * write/read raw msg functions:
 * the buffer is provided in little endian format, each word
 * occupying 3 bytes, length is in bytes.
 * functions will return immediately and do not not wait for DSP ressponse.
 * An ID is added to modify the command-ID
 */
enum tfa98xx_error
tfa_dsp_msg_id(tfa98xx_handle_t handle,
	int length, const char *buf, uint8_t cmdid[3])
{
	enum tfa98xx_error error;
	int tries, rpc_status = TFA98XX_I2C_REQ_DONE;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write_id(handle, length, buf, cmdid);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* get the result from the DSP (polling) */
	for (tries = TFA98XX_WAITRESULT_NTRIES; tries > 0; tries--) {
		error = tfa_dsp_msg_status(handle, &rpc_status);
		if (error == TFA98XX_ERROR_OK
			&& rpc_status == TFA98XX_I2C_REQ_DONE)
			break;
	}

	/* if (tfa98xx_runtime_verbose)
	 *  pr_debug("Number of tries: %d\n",
	 *   TFA98XX_WAITRESULT_NTRIES-tries);
	 */

	if (rpc_status != TFA98XX_I2C_REQ_DONE) {
		/* DSP RPC call returned an error */
		error = (enum tfa98xx_error)
			(rpc_status + TFA98XX_ERROR_BUFFER_RPC_BASE);
		pr_debug("DSP msg status: %d (%s)\n", rpc_status,
			tfa98xx_get_i2c_status_id_string(rpc_status));
	}
	return error;
}

/* read the return code for the RPC call */
TFA_INTERNAL enum tfa98xx_error
tfa98xx_check_rpc_status(tfa98xx_handle_t handle, int *p_rpc_status)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	/* the value to sent to the * CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0
	 */
	unsigned short cf_ctrl = 0x0002;
	/* memory address to be accessed (0: Status, 1: ID, 2: parameters) */
	unsigned short cf_mad = 0x0000;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;
	if (p_rpc_status == 0)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* 1) write DMEM=XMEM to the DSP XMEM */
	{
		/* minimize the number of I2C transactions
		 * by making use of the autoincrement in I2C
		 */
		unsigned char buffer[4];
		/* first the data for CF_CONTROLS */
		buffer[0] = (unsigned char)((cf_ctrl >> 8) & 0xFF);
		buffer[1] = (unsigned char)(cf_ctrl & 0xFF);
		/* write the contents of CF_MAD which is the subaddress
		 * following CF_CONTROLS
		 */
		buffer[2] = (unsigned char)((cf_mad >> 8) & 0xFF);
		buffer[3] = (unsigned char)(cf_mad & 0xFF);
		error = tfa98xx_write_data
			(handle, FAM_TFA98XX_CF_CONTROLS,
			sizeof(buffer), buffer);
	}
	if (error == TFA98XX_ERROR_OK) {
		/* read 1 word (24 bit) from XMEM */
		error = tfa98xx_dsp_read_mem(handle, 0, 1, p_rpc_status);
	}

	return error;
}

/***************************** xmem only **********************************/
enum tfa98xx_error
tfa98xx_dsp_read_mem(tfa98xx_handle_t handle,
	unsigned int start_offset, int num_words, int *p_values)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned char *bytes;
	int burst_size;		/* number of words per burst size */
	const int bytes_per_word = 3;
	int dmem;
	int num_bytes;
	int *p;

	bytes = kmalloc(num_words*bytes_per_word, GFP_KERNEL);
	if (bytes == NULL)
		return TFA98XX_ERROR_FAIL;

	/* If no offset is given, assume XMEM! */
	if (((start_offset >> 16) & 0xf) > 0)
		dmem = (start_offset >> 16) & 0xf;
	else
		dmem = TFA98XX_DMEM_XMEM;

	/* Remove offset from address */
	start_offset = start_offset & 0xffff;
	num_bytes = num_words * bytes_per_word;
	p = p_values;

	TFA_SET_BF(handle, DMEM, (uint16_t)dmem);
	error = -TFA_WRITE_REG(handle, MADD, (unsigned short)start_offset);
	if (error != TFA98XX_ERROR_OK)
		goto tfa98xx_dsp_read_mem_exit;

	for (; num_bytes > 0;) {
		burst_size = ROUND_DOWN(handles_local[handle].buffer_size,
			bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;

		_ASSERT(burst_size <= sizeof(bytes));
		error = tfa98xx_read_data(handle, FAM_TFA98XX_CF_MEM,
			burst_size, bytes);
		if (error != TFA98XX_ERROR_OK)
			goto tfa98xx_dsp_read_mem_exit;

		tfa98xx_convert_bytes2data(burst_size, bytes, p);

		num_bytes -= burst_size;
		p += burst_size / bytes_per_word;
	}

tfa98xx_dsp_read_mem_exit:
	kfree(bytes);

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_mem_word(tfa98xx_handle_t handle,
	unsigned short address, int value, int memtype)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned char bytes[3];

	TFA_SET_BF(handle, DMEM, (uint16_t)memtype);

	error = -TFA_WRITE_REG(handle, MADD, address);
	if (error != TFA98XX_ERROR_OK)
		return error;

	tfa98xx_convert_data2bytes(1, &value, bytes);
	error = tfa98xx_write_data(handle, FAM_TFA98XX_CF_MEM, 3, bytes);

	return error;
}

enum tfa98xx_error
tfa_cont_write_filterbank(int device, struct tfa_filter *filter)
{
	unsigned char biquad_index;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	for (biquad_index = 0; biquad_index < 10; biquad_index++) {
		if (filter[biquad_index].enabled) {
			error = tfa_dsp_cmd_id_write
				(device, MODULE_BIQUADFILTERBANK,
				biquad_index+1, /* start @1 */
				sizeof(filter[biquad_index].biquad.bytes),
				filter[biquad_index].biquad.bytes);
		} else {
			error = tfa98xx_dsp_biquad_disable
				(device, biquad_index+1);
		}
		if (error)
			return error;
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_biquad_disable(tfa98xx_handle_t handle, int biquad_index)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int coeff_buffer[BIQUAD_COEFF_SIZE];
	unsigned char bytes[3 + BIQUAD_COEFF_SIZE * 3];

	if (biquad_index > TFA98XX_BIQUAD_NUM)
		return TFA98XX_ERROR_BAD_PARAMETER;
	if (biquad_index < 1)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* make opcode */

	/* set in correct order and format for the DSP */
	coeff_buffer[0] = (int) -8388608;	/* -1.0f */
	coeff_buffer[1] = 0;
	coeff_buffer[2] = 0;
	coeff_buffer[3] = 0;
	coeff_buffer[4] = 0;
	coeff_buffer[5] = 0;
	/* convert to packed 24 bits data */
	tfa98xx_convert_data2bytes(BIQUAD_COEFF_SIZE, coeff_buffer, &bytes[3]);

	bytes[0] = 0;
	bytes[1] = MODULE_BIQUADFILTERBANK + 128;
	bytes[2] = (unsigned char)biquad_index;

	error = dsp_msg(handle, 3 + BIQUAD_COEFF_SIZE * 3, (char *)bytes);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
enum tfa98xx_error tfa_dsp_cmd_id_write(tfa98xx_handle_t handle,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	const unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char *buffer;
#if defined(TFADSP_DSP_BUFFER_POOL)
	int buffer_p_index = -1;
#endif
	int handle_in_use = 0;

	handle_in_use = tfa98xx_handle_is_open(handle);
	if (!handle_in_use) {
		pr_info("%s: open handle [%d] by force, when not in use\n",
			__func__, handle);
		tfa98xx_open(handle);
	}

#if defined(TFADSP_DSP_BUFFER_POOL)
	buffer_p_index = tfa98xx_buffer_pool_access
		(handle, -1, 3 + num_bytes, POOL_GET);
	if (buffer_p_index != -1) {
		pr_debug("%s: allocated from buffer_pool[%d] for %d bytes\n",
			__func__, buffer_p_index, 3 + num_bytes);
		buffer = (unsigned char *)
			(handles_local[handle].buf_pool[buffer_p_index].pool);
	} else {
		buffer = kmalloc(3 + num_bytes, GFP_KERNEL);
		if (buffer == NULL)
			goto dsp_cmd_id_write_error_exit;
	}
#else
	buffer = kmalloc(3 + num_bytes, GFP_KERNEL);
	if (buffer == NULL)
		goto dsp_cmd_id_write_error_exit;
#endif /* TFADSP_DSP_BUFFER_POOL */

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;

	memcpy(&buffer[3], data, num_bytes);

	error = dsp_msg(handle, 3 + num_bytes, (char *)buffer);

#if defined(TFADSP_DSP_BUFFER_POOL)
	if (buffer_p_index != -1) {
		tfa98xx_buffer_pool_access
			(handle, buffer_p_index, 0, POOL_RETURN);
	} else {
		kfree(buffer);
	}
#else
	kfree(buffer);
#endif /* TFADSP_DSP_BUFFER_POOL */

	if (!handle_in_use) {
		pr_info("%s: close handle [%d] when it's open by force\n",
			__func__, handle);
		tfa98xx_close(handle);
	}

	return error;

dsp_cmd_id_write_error_exit:
	pr_err("%s: can not allocate memory\n", __func__);

	if (!handle_in_use) {
		pr_info("%s: close handle [%d] when it's open by force\n",
			__func__, handle);
		tfa98xx_close(handle);
	}

	return TFA98XX_ERROR_FAIL;
}
EXPORT_SYMBOL(tfa_dsp_cmd_id_write);

/* wrapper for dsp_msg that adds opcode */
/* this is as the former tfa98xx_dsp_get_param() */
enum tfa98xx_error tfa_dsp_cmd_id_write_read(tfa98xx_handle_t handle,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char buffer[3];
	int handle_in_use = 0;

	handle_in_use = tfa98xx_handle_is_open(handle);
	if (!handle_in_use) {
		pr_info("%s: open handle [%d] by force, when not in use\n",
			__func__, handle);
		tfa98xx_open(handle);
	}

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;

	if (((handles_local[handle].rev & 0xff) == 0x72)
		&& (tfa98xx_cnt_max_device() == 1)
		&& (param_id == SB_PARAM_GET_RE25C ||
			param_id == SB_PARAM_GET_LSMODEL ||
			param_id == SB_PARAM_GET_ALGO_PARAMS)) {
		/* Modifying the ID for GetRe25C */
		buffer[0] = 4; /* CC: 4 (DS) for mono */
		pr_debug("%s: CC bit: 4 (DS) for mono\n", __func__);
	} else {
		if (tfa98xx_cnt_max_device() == 2)
			buffer[0] = 0; /* CC: 0 (reset all) for stereo */
		pr_debug("%s: CC bit: %d\n", __func__, buffer[0]);
	}

	pr_debug("%s: dsp_msg[]=0x%x, 0x%x, 0x%x\n",
		__func__, buffer[0], buffer[1], buffer[2]);
	error = dsp_msg(handle, sizeof(unsigned char[3]), (char *)buffer);
	if (error == TFA98XX_ERROR_OK)
		/* read the data from the dsp */
		error = dsp_msg_read(handle, num_bytes, data);

	if (!handle_in_use) {
		pr_info("%s: close handle [%d] when it's open by force\n",
			__func__, handle);
		tfa98xx_close(handle);
	}

	return error;
}
EXPORT_SYMBOL(tfa_dsp_cmd_id_write_read);

/* wrapper for dsp_msg that adds opcode and 3 bytes required for coefs */
enum tfa98xx_error tfa_dsp_cmd_id_coefs(tfa98xx_handle_t handle,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char buffer[6];

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;

	error = dsp_msg(handle, sizeof(unsigned char[6]), (char *)buffer);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(handle, num_bytes, data);

	return error;
}

/* wrapper for dsp_msg that adds opcode; 3 bytes required for MBDrcDynamics */
enum tfa98xx_error tfa_dsp_cmd_id_mbdrc_dynamics(tfa98xx_handle_t handle,
	unsigned char module_id,
	unsigned char param_id, int index_subband,
	int num_bytes, unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char buffer[6];

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = (unsigned char)index_subband;

	error = dsp_msg(handle, sizeof(unsigned char[6]), (char *)buffer);
	if (error != TFA98XX_ERROR_OK)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(handle, num_bytes, data);

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_preset(tfa98xx_handle_t handle, int length,
	const unsigned char *p_preset_bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (p_preset_bytes != 0) {
		/* by design: keep the data opaque and no
		 * interpreting/calculation
		 */
		error = tfa_dsp_cmd_id_write(handle, MODULE_SPEAKERBOOST,
					SB_PARAM_SET_PRESET, length,
					p_preset_bytes);
	} else {
		error = TFA98XX_ERROR_BAD_PARAMETER;
	}
	return error;
}

/*
 * get features from MTP
 */
enum tfa98xx_error
tfa98xx_dsp_get_hw_feature_bits(tfa98xx_handle_t handle, int *features)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint32_t value;
	uint16_t mtpbf;

	/* return the cache data if it's valid */
	if (handles_local[handle].hw_feature_bits != -1) {
		*features = handles_local[handle].hw_feature_bits;
	} else {
		mtpbf = 0xf907; /* MTP9 for tfa2, 8 bits */
		value = tfa_read_reg(handle, mtpbf) & 0xffff;
		*features = handles_local[handle].hw_feature_bits = value;
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_get_sw_feature_bits(tfa98xx_handle_t handle, int features[2])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned char bytes[3 * 2];

	/* return the cache data if it's valid */
	if (handles_local[handle].sw_feature_bits[0] != -1) {
		features[0] = handles_local[handle].sw_feature_bits[0];
		features[1] = handles_local[handle].sw_feature_bits[1];
	} else {
		error = tfa_dsp_cmd_id_write_read(handle, MODULE_FRAMEWORK,
			FW_PAR_ID_GET_FEATURE_INFO,
			sizeof(bytes), bytes);

		if (error != TFA98XX_ERROR_OK) {
			/* old ROM code may respond
			 * with TFA98XX_ERROR_RPC_PARAM_ID
			 */
			return error;
		}
		tfa98xx_convert_bytes2data(sizeof(bytes), bytes, features);
	}
	return error;
}

enum tfa98xx_error
tfa98xx_dsp_get_state_info(tfa98xx_handle_t handle,
	unsigned char bytes[], unsigned int *statesize)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int b_support_framework = 0;
	unsigned int state_size = 9;

	err = tfa98xx_dsp_support_framework(handle, &b_support_framework);
	if (err == TFA98XX_ERROR_OK) {
		if (b_support_framework) {
			err = tfa_dsp_cmd_id_write_read(handle,
				MODULE_FRAMEWORK,
				FW_PARAM_GET_STATE, 3 * state_size, bytes);
		} else {
			/* old ROM code, ask SpeakerBoost and
			 * only do first portion
			 */
			state_size = 8;
			err = tfa_dsp_cmd_id_write_read
				(handle, MODULE_SPEAKERBOOST,
				SB_PARAM_GET_STATE, 3 * state_size, bytes);
		}
	}

	*statesize = state_size;

	return err;
}

enum tfa98xx_error
tfa98xx_dsp_support_drc(tfa98xx_handle_t handle, int *pb_support_drc)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	*pb_support_drc = 0;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;
	if (handles_local[handle].support_drc != SUPPORT_NOT_SET) {
		*pb_support_drc =
			(handles_local[handle].support_drc == SUPPORT_YES);
	} else {
		int feature_bits[2];

		error = tfa98xx_dsp_get_sw_feature_bits(handle, feature_bits);
		if (error == TFA98XX_ERROR_OK) {
			/* easy case: new API available */
			/* bit=0 means DRC enabled */
			*pb_support_drc = (feature_bits[0] & FEATURE1_DRC) == 0;
		} else if (error == TFA98XX_ERROR_RPC_PARAM_ID) {
			/* older ROM code, doesn't support it */
			*pb_support_drc = 0;
			error = TFA98XX_ERROR_OK;
		}
		/* else some other error, return transparently */
		/* pb_support_drc only changed when error == TFA98XX_ERROR_OK */

		if (error == TFA98XX_ERROR_OK) {
			handles_local[handle].support_drc =
				*pb_support_drc ? SUPPORT_YES : SUPPORT_NO;
		}
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_support_framework(tfa98xx_handle_t handle,
	int *pb_support_framework)
{
	int feature_bits[2] = {0, 0};
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	_ASSERT(pb_support_framework != 0);

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	if (handles_local[handle].support_framework != SUPPORT_NOT_SET) {
		if (handles_local[handle].support_framework == SUPPORT_NO)
			*pb_support_framework = 0;
		else
			*pb_support_framework = 1;
	} else {
		error = tfa98xx_dsp_get_sw_feature_bits(handle, feature_bits);
		if (error == TFA98XX_ERROR_OK) {
			*pb_support_framework = 1;
			handles_local[handle].support_framework = SUPPORT_YES;
		} else {
			*pb_support_framework = 0;
			handles_local[handle].support_framework = SUPPORT_NO;
			error = TFA98XX_ERROR_OK;
		}
	}

	/* *pb_support_framework only changed when error == TFA98XX_ERROR_OK */
	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_speaker_parameters(tfa98xx_handle_t handle,
		int length, const unsigned char *p_speaker_bytes)
{
	enum tfa98xx_error error;
	int b_support_drc;

	if (p_speaker_bytes != 0) {
		/* by design: keep the data opaque and no
		 * interpreting/calculation
		 * Use long WaitResult retry count
		 */
		error = tfa_dsp_cmd_id_write(
					handle,
					MODULE_SPEAKERBOOST,
					SB_PARAM_SET_LSMODEL, length,
					p_speaker_bytes);
	} else {
		error = TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (error != TFA98XX_ERROR_OK)
		return error;

	error = tfa98xx_dsp_support_drc(handle, &b_support_drc);
	if (error != TFA98XX_ERROR_OK)
		return error;

	if (b_support_drc) {
		/* Need to set AgcGainInsert back to PRE,
		 * as the SetConfig forces it to POST
		 */
		uint8_t bytes[3] = {0, 0, 0};

		error = tfa_dsp_cmd_id_write(handle,
			MODULE_SPEAKERBOOST,
			SB_PARAM_SET_AGCINS,
			sizeof(bytes),
			bytes);
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_write_config(tfa98xx_handle_t handle, int length,
	const unsigned char *p_config_bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int b_support_drc;

	error = tfa_dsp_cmd_id_write(handle,
		MODULE_SPEAKERBOOST,
		SB_PARAM_SET_CONFIG, length,
		p_config_bytes);
	if (error != TFA98XX_ERROR_OK)
		return error;

	error = tfa98xx_dsp_support_drc(handle, &b_support_drc);
	if (error != TFA98XX_ERROR_OK)
		return error;

	if (b_support_drc) {
		/* Need to set AgcGainInsert back to PRE,
		 * as the SetConfig forces it to POST
		 */
		uint8_t bytes[3] = {0, 0, 0};

		error = tfa_dsp_cmd_id_write(handle,
			MODULE_SPEAKERBOOST,
			SB_PARAM_SET_AGCINS,
			sizeof(bytes),
			bytes);
	}

	return error;
}

/* load all the parameters for the DRC settings from a file */
enum tfa98xx_error tfa98xx_dsp_write_drc(tfa98xx_handle_t handle,
	int length, const unsigned char *p_drc_bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (p_drc_bytes != 0) {
		error = tfa_dsp_cmd_id_write(handle,
			MODULE_SPEAKERBOOST,
			SB_PARAM_SET_DRC, length,
			p_drc_bytes);
	} else {
		error = TFA98XX_ERROR_BAD_PARAMETER;
	}

	return error;
}

enum tfa98xx_error tfa98xx_powerdown(tfa98xx_handle_t handle, int powerdown)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	TFA_SET_BF(handle, PWDN, (uint16_t)powerdown);

	return error;
}

enum tfa98xx_error
tfa98xx_select_mode(tfa98xx_handle_t handle, enum tfa98xx_mode mode)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	if (error == TFA98XX_ERROR_OK) {
		switch (mode) {

		default:
			error = TFA98XX_ERROR_BAD_PARAMETER;
		}
	}

	return error;
}

int tfa_set_bf(tfa98xx_handle_t dev_idx,
	const uint16_t bf, const uint16_t value)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk, oldvalue;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(dev_idx, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	oldvalue = regvalue;
	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= value << pos;

	/* Only write when the current register value is
	 * not the same as the new value
	 */
	if (oldvalue != regvalue) {
		err = reg_write(dev_idx, address, regvalue);
		if (err) {
			pr_err("Error setting bf :%d\n", -err);
			return -err;
		}
	}

	return 0;
}

int tfa_set_bf_volatile(tfa98xx_handle_t dev_idx,
	const uint16_t bf, const uint16_t value)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(dev_idx, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= value << pos;

	err = reg_write(dev_idx, address, regvalue);
	if (err) {
		pr_err("Error setting bf :%d\n", -err);
		return -err;
	}

	return 0;
}

int tfa_get_bf(tfa98xx_handle_t dev_idx, const uint16_t bf)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk;
	uint16_t value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(dev_idx, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= msk;
	value = regvalue >> pos;

	return value;
}

int tfa_set_bf_value(const uint16_t bf,
	const uint16_t bf_value, uint16_t *p_reg_value)
{
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	regvalue = *p_reg_value;

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= bf_value << pos;

	*p_reg_value = regvalue;

	return 0;
}

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value)
{
	uint16_t msk, value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	msk = ((1 << (len + 1)) - 1) << pos;
	value = (reg_value & msk) >> pos;

	return value;
}


int tfa_write_reg(tfa98xx_handle_t dev_idx,
	const uint16_t bf, const uint16_t reg_value)
{
	enum tfa98xx_error err;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_write(dev_idx, address, reg_value);
	if (err)
		return -err;

	return 0;
}

int tfa_read_reg(tfa98xx_handle_t dev_idx, const uint16_t bf)
{
	enum tfa98xx_error err;
	uint16_t regvalue;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(dev_idx, address, &regvalue);
	if (err)
		return -err;

	return regvalue;
}

/*
 * powerup the coolflux subsystem and wait for it
 */
enum tfa98xx_error tfa_cf_powerup(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	/* power on the sub system */
	TFA_SET_BF_VOLATILE(handle, PWDN, 0);
	return err;
}

/*
 * Enable/Disable the I2S output for TFA1 devices
 * without TDM interface
 */
static enum tfa98xx_error
tfa98xx_aec_output(tfa98xx_handle_t handle, int enable)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	if ((tfa98xx_get_device_dai(handle) & TFA98XX_DAI_TDM)
		== TFA98XX_DAI_TDM)
		return err;

	if (tfa98xx_dev_family(handle) == 1)
		err = -tfa_set_bf(handle, TFA1_BF_I2SDOE, (enable != 0));
	else {
		pr_err("I2SDOE on unsupported family\n");
		err = TFA98XX_ERROR_NOT_SUPPORTED;
	}

	return err;
}

/*
 * Print the current state of the hardware manager
 * Device manager status information, man_state from TFA9888_N1B_I2C_regmap_V12
 */
enum tfa98xx_error show_current_state(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int manstate = -1;

	if (tfa98xx_dev_family(handle) == 2) {
		manstate = TFA_GET_BF(handle, MANSTATE);
		if (manstate < 0)
			return -manstate;
	}

	pr_debug("Current HW manager state: dev = %d", handle);

	switch (manstate) {
	case 0:
		pr_debug("power_down_state\n");
		break;
	case 1:
		pr_debug("wait_for_source_settings_state\n");
		break;
	case 2:
		pr_debug("connnect_pll_input_state\n");
		break;
	case 3:
		pr_debug("disconnect_pll_input_state\n");
		break;
	case 4:
		pr_debug("enable_pll_state\n");
		break;
	case 5:
		pr_debug("enable_cgu_state\n");
		break;
	case 6:
		pr_debug("init_cf_state\n");
		break;
	case 7:
		pr_debug("enable_amplifier_state\n");
		break;
	case 8:
		pr_debug("alarm_state\n");
		break;
	case 9:
		pr_debug("operating_state\n");
		break;
	case 10:
		pr_debug("mute_audio_state\n");
		break;
	case 11:
		pr_debug("disable_cgu_pll_state\n");
		break;
	default:
		pr_debug("Unable to find current state\n");
		break;
	}

	return err;
}

/*
 * start the speakerboost algorithm
 * this implies a full system startup when the system was not already started
 */
enum tfa98xx_error
tfa_run_speaker_boost(tfa98xx_handle_t handle, int force, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int value;
	int istap_prof = 0;

	if (force) {
		handles_local[handle].is_cold = 1;
		err = tfa_run_coldstartup(handle, profile);
		if (err)
			return err;
		/* DSP is running now */
	}

	/* Returns 1 when device is "cold" and 0 when device is warm */
	value = tfa_is_cold(handle);
	if (value < 0)
		err = value;
	else
		handles_local[handle].is_cold = value;

	pr_info("%s: %s boot, ext_dsp = %d, profile = %d\n",
		__func__, value ? "cold" : "warm",
		handles_local[handle].ext_dsp, profile);

#ifdef __KERNEL__ /* TODO try to combine this with the pr_debug below */
	tfa98xx_trace_printk("%s %sstart\n",
		tfa_cont_device_name(handle),
		value ? "cold" : "warm");
#endif

	/* Check if next profile is a tap profile */
	istap_prof = tfa_cont_is_tap_profile(handle, profile);

	/* cold start and not tap profile */
	if ((value == 1) && (!istap_prof)) {
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		if (handle == 0
			|| handle == active_handle) /* master device only */
#else
		if (handle == 0) /* master device only */
		/* if (handles_local[handle].ext_dsp) */
#endif
		{
			/* flush message buffer */
			pr_debug("%s: flush buffer in blob, in cold start\n",
				__func__);
			err = tfa_tib_dsp_msgblob(handle, -2, NULL);
		}

		/* Run startup and write all files */
		pr_info("%s: cold start, speaker startup\n", __func__);
		err = tfa_run_speaker_startup(handle, force, profile);
		if (err) {
			pr_info("%s: speaker startup error = %d\n",
				__func__, err);
			return err;
		}

		/* Save the current profile and set the vstep to 0 */
		/* This needs to be overwritten even in CF bypass */
		tfa_set_swprof(handle, (unsigned short)profile);
		tfa_set_swvstep(handle, 0);

		/* Dont run this for softDSP */
#if defined(SET_CALIBRATION_AT_ALL_DEVICE_READY)
		if (!handles_local[0].ext_dsp) { /* check only master device */
#else
		if (!handles_local[handle].ext_dsp) {
#endif
			/* Startup with CF in bypass then return here */
			if (tfa_cf_enabled(handle) == 0) {
				pr_info("%s: tfa_cf_enabled() = 0, err = %d\n",
					__func__, err);
				return err;
			}

#ifdef __KERNEL__ /* TODO check if this can move to the tfa98xx.c */
			/* Necessary here for early calibrate (MTPEX + ACS) */
			tfa98xx_apply_deferred_calibration(handle);
#endif
			/* calibrate */
			pr_info("%s: tfa_run_speaker_calibration\n", __func__);
			err = tfa_run_speaker_calibration(handle, profile);
		} else { /* Only for Tiberius */
			/* always send the SetRe25 message
			 * to indicate all messages are send
			 */
			pr_info("%s: tfa_set_calibration_values\n", __func__);
			err = tfa_set_calibration_values(handle);
			if (err)
				pr_info("%s: set calibration values error = %d\n",
					__func__, err);
		}
	} else if (istap_prof) {
		/* Dont run this for softDSP */
		if (!handles_local[handle].ext_dsp) {
			/* Save the current profile and set the vstep to 0 */
			/* This needs to be overwritten in tap profile */
			tfa_set_swprof(handle, (unsigned short)profile);
			tfa_set_swvstep(handle, 0);
		}
	}

	return err;
}

enum tfa98xx_error
tfa_run_speaker_startup(tfa98xx_handle_t handle, int force, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	pr_debug("coldstart%s :", force ? " (forced)" : "");

	if (!force) { /* in case of force CF already running */
		err = tfa_run_startup(handle, profile);
		PRINT_ASSERT(err);
		if (err) {
			pr_info("%s: tfa_run_startup error = %d\n",
				__func__, err);
			return err;
		}

		/* Startup with CF in bypass then return here */
		if (tfa_cf_enabled(handle) == 0)
			return err;

		/* if (!handles_local[handle].ext_dsp) { */
		/* check only master device */
		if (!handles_local[0].ext_dsp) {
			err = tfa_run_start_dsp(handle);
			if (err)
				return err;
		}
	}

	/* SET auto_copy_mtp_to_iic (bit 5 of A3) to 1.
	 * Workaround for 72 and 88 (see PLMA5290)
	 */
	err = reg_write(handle, 0xA3, 0x20);
	if (err)
		return err;

	/* DSP is running now */
	/* write all the files from the device list */
	handles_local[handle].is_bypass = 1; /* reset before start */
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
	if (handle == active_handle) {
		pr_info("%s: load dev files from master device\n",
			__func__);
		err = tfa_cont_write_files(0); /* loading master device */
	} else if ((active_handle == -1)
		&& (handles_local[0].is_cold == 0)
		&& (handles_local[handle].is_cold != 0)
		&& (handle != 0)) {
		pr_info("%s: load dev files from master device, when master is not cold\n",
			__func__);
		err = tfa_cont_write_files(0); /* loading master device */
	} else {
		err = tfa_cont_write_files(handle);
	}
#else
	err = tfa_cont_write_files(handle);
#endif
	if (err) {
		pr_debug("[%s] tfa_cont_write_files error = %d\n",
			__func__, err);
		return err;
	}

	/* write all the files from the profile list (use volumstep 0) */
	err = tfa_cont_write_files_prof(handle, profile, 0);
	if (err) {
		pr_debug("[%s] tfa_cont_write_files_prof error = %d\n",
			__func__, err);
		return err;
	}

	return err;
}

/*
 * Run calibration
 */
enum tfa98xx_error
tfa_run_speaker_calibration(tfa98xx_handle_t handle, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int calibrate_done, spkr_count = 0;
	(void)(profile); /* Remove unreferenced warning */

#ifdef __KERNEL__
	/* Necessary otherwise we are thrown out of operating mode
	 * in kernel (because of internal clock)
	 */
	if ((strnstr(tfa_cont_profile_name(handle, profile),
		".cal", strlen(tfa_cont_profile_name(handle, profile)))
		== NULL)
		|| (tfa98xx_dev_family(handle) == 1))
#endif
		TFA_SET_BF_VOLATILE(handle, SBSL, 1);

	/* return if there is no audio running */
	if ((tfa98xx_dev_family(handle) == 2) && TFA_GET_BF(handle, NOCLK))
		return TFA98XX_ERROR_NO_CLOCK;

	/* When MTPOTC is set (cal=once) unlock key2 */
	if (TFA_GET_BF(handle, MTPOTC) == 1)
		tfa98xx_key2(handle, 0);

	/* await calibration, this should return ok */
	err = tfa_run_wait_calibration(handle, &calibrate_done);
	if (err == TFA98XX_ERROR_OK) {
		err = tfa_dsp_get_calibration_impedance(handle);
		PRINT_ASSERT(err);
	}

	/* Give reason why calibration failed! */
	if (err != TFA98XX_ERROR_OK) {
		if ((tfa98xx_dev_family(handle) == 2)
			&& (TFA_GET_BF(handle, REFCKSEL) == 1))
			pr_err("Unable to calibrate the device with the internal clock!\n");
	}

	if (err == TFA98XX_ERROR_OK) {
		err = tfa98xx_supported_speakers(handle, &spkr_count);

		if (spkr_count == 1)
			pr_info("P: %d mOhms\n",
				handles_local[handle].mohm[0]);
		else
			pr_info("P: %d mOhms, S:%d mOhms\n",
						handles_local[0].mohm[0],
						handles_local[1].mohm[0]);
	}

	/* When MTPOTC is set (cal=once) re-lock key2 */
	if (TFA_GET_BF(handle, MTPOTC) == 1)
		tfa98xx_key2(handle, 1);

	return err;
}

/*
 * Write calibration values for Tiberius
 */
enum tfa98xx_error tfa_set_calibration_values(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	unsigned char bytes[6] = {0};
	unsigned short value = 0, channel = 0;
	int dsp_cal_value_prim = 0, dsp_cal_value_scnd = 0;
	int i, devcount = tfa98xx_cnt_max_device();
	static int need_cal, is_bypass;

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
	/* reset need_cal for the first device */
	if ((handle == 0)
		|| (handle == active_handle))
#else
	if (handle == 0)
#endif /* TFA_USE_DEVICE_SPECIFIC_CONTROL */
	{
		need_cal = 0;
		is_bypass = 1;
	}
	need_cal |= (TFA_GET_BF(handle, MTPEX) == 0) ? 1 : 0;
	is_bypass &= handles_local[handle].is_bypass;

#if defined(SET_CALIBRATION_AT_ALL_DEVICE_READY)
	/* trigger at the last device */
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
	if ((handle < devcount - 1)
		&& (handle != active_handle))
#else
	if (handle < devcount - 1)
#endif /* TFA_USE_DEVICE_SPECIFIC_CONTROL */
	{
		pr_debug("%s: suspend setting calibration data till all device is enabled\n",
			__func__);
		return err;
	}
#endif /* (SET_CALIBRATION_AT_ALL_DEVICE_READY) */

	if (is_bypass) {
		pr_info("%s: [%d] skip sending calibration data in bypass",
			__func__, handle);
		/* check only master device */
		if (handles_local[0].ext_dsp == 1) {
			pr_info("%s: flush buffer in blob, in bypass\n",
				__func__);
			/* master device only */
			err = tfa_tib_dsp_msgblob(0, -2, NULL);
		}
		return err;
	}

	pr_debug("%s: calibration is %srequired\n",
		__func__, (need_cal) ? "" : "not ");
	pr_info("%s: devcount=%d\n", __func__, devcount);

	/* If calibration is set to once we load from MTP, else send zero's */
	if (need_cal == 0) {
		pr_info("%s: [%d] MTPEX=1\n", __func__, handle);
		if (devcount == 1) { /* mono */
			err = reg_read(handle, 0xF5, &value);
			dsp_cal_value_prim = (value * 65536) / 1000;
			pr_info("%s: calibration data: %d\n",
				__func__, value);
			dsp_cal_value_scnd = dsp_cal_value_prim;
		} else if (devcount == 2) { /* stereo */
			/* For now use quick hack
			 * to assume handle 0 & 1 for stereo
			*/
			for (i = 0; i < devcount; i++) {
				err = reg_read(i, 0xF5, &value);
				channel = tfa98xx_get_profile_tdmspks(i,
					handles_local[i].profile);
				if (channel == 0) {
					dsp_cal_value_prim =
						(value * 65536) / 1000;
					pr_info("%s: dev %d - calibration data[prim]: %d\n",
						__func__, i, value);
				} else if (channel == 1) {
					dsp_cal_value_scnd =
						(value * 65536) / 1000;
					pr_info("%s: dev %d - calibration data[scnd]: %d\n",
						__func__, i, value);
				}
			}
		} else {
			pr_err("%s: more than 2 devices were selected (devcount %d)\n",
				__func__, devcount);
		}

		/* We have to copy it for both channels. Even when MONO! */
		bytes[0] = (uint8_t) ((dsp_cal_value_prim >> 16) & 0xffff);
		bytes[1] = (uint8_t) ((dsp_cal_value_prim >> 8) & 0xff);
		bytes[2] = (uint8_t) (dsp_cal_value_prim & 0xff);

		bytes[3] = (uint8_t) ((dsp_cal_value_scnd >> 16) & 0xffff);
		bytes[4] = (uint8_t) ((dsp_cal_value_scnd >> 8) & 0xff);
		bytes[5] = (uint8_t) (dsp_cal_value_scnd & 0xff);

#if defined(TFA_BLACKBOX_LOGGING)
		if (handles_local[0].log_set_cb)
			handles_local[0].log_set_cb();
#endif
	} else { /* calibration is required */
		pr_info("%s: config ResetRe25C to do calibration\n", __func__);
		bytes[0] = 0;
		bytes[1] = 0;
		bytes[2] = 0;
		bytes[3] = 0;
		bytes[4] = 0;
		bytes[5] = 0;
	}

	err = tfa_dsp_cmd_id_write
		(handle, MODULE_SPEAKERBOOST, SB_PARAM_SET_RE25C,
		sizeof(bytes), bytes);

#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
	if (need_cal == 1)
		err = tfa_tfadsp_wait_calibrate_done(handle);
#endif

	return err;
}

/*
 * Set the debug option
 */
void tfa_verbose(int level)
{
	tfa98xx_trace_level = level;
	tfa98xx_runtime_verbose = (level != 0); /* any non-zero */
}

int tfa_get_verbose(void)
{
	return tfa98xx_trace_level;
}

void tfa_dsp_verbose(int level)
{
	tfa98xx_dsp_verbose = level;
}

enum tfa98xx_error tfa_run_coldboot(tfa98xx_handle_t handle, int state)
{
#define CF_CONTROL 0x8100
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int tries = 10;

	/* repeat set ACS bit until set as requested */
	while (state != TFA_GET_BF(handle, ACS)) {
		/* set colstarted in CF_CONTROL to force ACS */
		err = mem_write(handle, CF_CONTROL, state, TFA98XX_DMEM_IOMEM);
		PRINT_ASSERT(err);

		if (tries-- == 0) {
			pr_debug("coldboot (ACS) did not %s\n", state ?
				"set" : "clear");
			return TFA98XX_ERROR_OTHER;
		}
	}

	return err;
}

/*
 * load the patch if any
 * else tell no loaded
 */
static enum tfa98xx_error tfa_run_load_patch(tfa98xx_handle_t handle)
{
	return tfa_cont_write_patch(handle);
}

/*
 * this will load the patch witch will implicitly start the DSP
 * if no patch is available the DPS is started immediately
 */
enum tfa98xx_error tfa_run_start_dsp(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	err = tfa_run_load_patch(handle);
	if (err) { /* patch load is fatal so return immediately */
		return err;
	}

	/* Clear count_boot, should be reset to 0
	 * before the DSP reset is released
	 */
	err = mem_write(handle, 512, 0, TFA98XX_DMEM_XMEM);
	PRINT_ASSERT(err);

	/* Reset DSP once for sure after initializing */
	if (err == TFA98XX_ERROR_OK) {
		err = tfa98xx_dsp_reset(handle, 0);
		/* in pair of tfa98xx_init() - tfa_run_startup() */
		PRINT_ASSERT(err);
	}

	/* Sample rate is needed to set the correct tables */
	err = tfa98xx_dsp_write_tables(handle, TFA_GET_BF(handle, AUDFS));
	PRINT_ASSERT(err);

	return err;
}

/*
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 */
enum tfa98xx_error tfa_run_startup(tfa98xx_handle_t handle, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device_list *dev = tfa_cont_device(handle);
	int i, noinit = 0;
#if defined(REDUCED_REGISTER_SETTING)
	int is_cold_amp;
#endif

	if (dev == NULL)
		return TFA98XX_ERROR_FAIL;

#if defined(REDUCED_REGISTER_SETTING)
	is_cold_amp = tfa_is_cold_amp(handle);
	if (first_after_boot[handle] || (is_cold_amp == 1)) {
#endif
		/* process the device list
		 * to see if the user implemented the noinit
		 */
		for (i = 0; i < dev->length; i++) {
			if (dev->list[i].type == dsc_no_init) {
				noinit = 1;
				break;
			}
		}

		if (!noinit) {
			/* load the optimal TFA98XX in HW settings */
			err = tfa98xx_init(handle);
			PRINT_ASSERT(err);
		} else {
			pr_debug("Warning: No init keyword found in the cnt file. Init is skipped!\n");
		}

		/* I2S settings to define the audio input properties
		 * these must be set before the subsys is up
		 * this will run the list
		 * until a non-register item is encountered
		 */
		err = tfa_cont_write_regs_dev(handle);
		/* write device register settings */
		PRINT_ASSERT(err);
#if defined(REDUCED_REGISTER_SETTING)
	} else {
		pr_info("%s: skip init and writing registers under dev (%d:%d)\n",
			__func__, first_after_boot[handle], is_cold_amp);
	}
#endif

#if defined(REDUCED_REGISTER_SETTING)
	if ((first_after_boot[handle] || (is_cold_amp == 1))
		|| (profile != tfa_get_swprof(handle))) {
#endif
		/* also write register the settings from the default profile
		 * NOTE we may still have ACS=1
		 * so we can switch sample rate here
		 */
		err =  tfa_cont_write_regs_prof(handle, profile);
		PRINT_ASSERT(err);
#if defined(REDUCED_REGISTER_SETTING)
	} else {
		pr_info("%s: skip writing registers under profile (%d)\n",
			__func__, profile);
	}
#endif

	if ((tfa98xx_dev_revision(handle) & 0xff) == 0x88) {
		/* Factory trimming for the Boost converter */
		tfa_factory_trimmer(handle);
	}

	/* leave power off state */
	err = tfa98xx_powerdown(handle, 0);
	PRINT_ASSERT(err);

	if (tfa98xx_dev_family(handle) == 2) {
	/* signal that the clock settings are done
	 *  - PLL can start
	 */
		TFA_SET_BF_VOLATILE(handle, MANSCONF, 1);
	}
#if defined(REDUCED_REGISTER_SETTING)
	first_after_boot[handle] = 0;
#endif

	if (tfa98xx_runtime_verbose && (tfa98xx_dev_family(handle) == 2))
		err = show_current_state(handle);

	return err;
}

/*
 * run the startup/init sequence and set ACS bit
 */
enum tfa98xx_error tfa_run_coldstartup(tfa98xx_handle_t handle, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	err = tfa_run_startup(handle, profile);
	PRINT_ASSERT(err);
	if (err)
		return err;

	if ((handles_local[handle].rev & 0xFF) != 0x72) {
		/* force cold boot */
		err = tfa_run_coldboot(handle, 1); /* set ACS */
		PRINT_ASSERT(err);
		if (err)
			return err;
	}

	/* start */
	err = tfa_run_start_dsp(handle);
	PRINT_ASSERT(err);

	return err;
}

/*
 *
 */
enum tfa98xx_error tfa_run_mute(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int status;
	int tries = 0;

	/* signal the TFA98XX to mute */
	err = tfa98xx_set_mute(handle, TFA98XX_MUTE_AMPLIFIER);

	if (tfa98xx_dev_family(handle) == 1) {
		if (err == TFA98XX_ERROR_OK) {
			/* now wait for the amplifier to turn off */
			do {
				status = TFA_GET_BF(handle, SWS);
				if (status != 0)
					msleep_interruptible(10);
					/* wait 10ms to avoid busload */
				else
					break;
				tries++;
			} while (tries < AMPOFFWAIT_TRIES);

			/*The amplifier is always switching*/
			if (tries == AMPOFFWAIT_TRIES) {
				pr_err("%s: timeout in stopping amplifier switching\n",
					__func__);
				return TFA98XX_ERROR_OTHER;
			}
		}
	}

	if (tfa98xx_runtime_verbose)
		pr_debug("-------------------- muted --------------------\n");

	return err;
}
/*
 *
 */
enum tfa98xx_error tfa_run_unmute(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	/* signal the TFA98XX to mute */
	err = tfa98xx_set_mute(handle, TFA98XX_MUTE_OFF);

	if (tfa98xx_runtime_verbose)
		pr_debug("-------------------unmuted ------------------\n");

	return err;
}


/*
 * wait for calibrate_done
 */
enum tfa98xx_error
tfa_run_wait_calibration(tfa98xx_handle_t handle, int *calibrate_done)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int tries = 0, mtp_busy = 1, tries_mtp_busy = 0;

	*calibrate_done = 0;

	/* in case of calibrate once wait for MTPEX */
	if (TFA_GET_BF(handle, MTPOTC)) {
		/* Check if MTP_busy is clear! */
		while (tries_mtp_busy < MTPBWAIT_TRIES) {
			mtp_busy = TFA_GET_BF(handle, MTPB);
			if (mtp_busy == 1)
				msleep_interruptible(10);
				/* wait 10ms to avoid busload */
			else
				break;
			tries_mtp_busy++;
		}

		if (tries_mtp_busy < MTPBWAIT_TRIES) {
			/* Because of the msleep
			 * TFA98XX_API_WAITRESULT_NTRIES is way to long!
			 * Setting this to 25 will take
			 * at least 25*50ms = 1.25 sec
			 */
			while ((*calibrate_done == 0)
				&& (tries < MTPEX_WAIT_NTRIES)) {
				*calibrate_done = TFA_GET_BF(handle, MTPEX);
				if (*calibrate_done == 1)
					break;
				msleep_interruptible(50);
				/* wait 50ms to avoid busload */
				tries++;
			}

			if (tries >= MTPEX_WAIT_NTRIES)
				tries = TFA98XX_API_WAITRESULT_NTRIES;
		} else {
			pr_err("MTP busy after %d tries\n", MTPBWAIT_TRIES);
		}
	}

	/* poll xmem for calibrate always
	 * calibrate_done = 0 means "calibrating",
	 * calibrate_done = -1 (or 0xFFFFFF) means "fails"
	 * calibrate_done = 1 means calibration done
	 */
	while ((*calibrate_done != 1)
		&& (tries < TFA98XX_API_WAITRESULT_NTRIES)) {
		err = mem_read(handle, TFA_FW_XMEM_CALIBRATION_DONE,
			1, calibrate_done);
		tries++;
	}

	if (*calibrate_done != 1) {
		pr_err("Calibration failed!\n");
		err = TFA98XX_ERROR_BAD_PARAMETER;
	} else if (tries == TFA98XX_API_WAITRESULT_NTRIES) {
		pr_debug("Calibration has timedout!\n");
		err = TFA98XX_ERROR_STATE_TIMED_OUT;
	} else if (tries_mtp_busy == 1000) {
		pr_err("Calibrate Failed: MTP_busy stays high!\n");
		err = TFA98XX_ERROR_STATE_TIMED_OUT;
	} else {
		pr_info("Calibration succeeded!\n");
	}

	/* Check which speaker calibration failed. Only for 88C */
	if ((err != TFA98XX_ERROR_OK)
		&& ((handles_local[handle].rev & 0x0FFF) == 0xc88)) {
		individual_calibration_results(handle);
	}

#ifdef CONFIG_DEBUG_FS
	tfa98xx_deferred_calibration_status(handle, *calibrate_done);
#endif
	return err;
}

static enum tfa98xx_error _tfa_stop(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	pr_debug("%s: dev %d\n", __func__, handle);

	/* mute */
	tfa_run_mute(handle);
	/* powerdown CF */
	err = tfa98xx_powerdown(handle, 1);
	if (err != TFA98XX_ERROR_OK)
		return err;

	/* Workaround for ticket PLMA5337 */
	if ((handles_local[handle].rev & 0xff) == 0x72)
		TFA_SET_BF_VOLATILE(handle, AMPE, 0);

	/* disable I2S output on TFA1 devices without TDM */
	err = tfa98xx_aec_output(handle, 0);

	return err;
}

enum tfa_error tfa_start(int next_profile, int *vstep)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int dev, devcount = tfa98xx_cnt_max_device();
	int profile = -1, cal_profile = -1, istap_prof = 0, active_profile = -1;
#ifdef __KERNEL__
	uint8_t kernel_sleep_timeout = 0;
#endif

	tfa98xx_log_start_cnt++;
	tfa98xx_log_tfa_family = tfa98xx_dev_family(0); /* device 0 */

	pr_debug("%s: tfa98xx_log_tfa_family=%d, ",
		__func__, tfa98xx_log_tfa_family);
	pr_debug("%s: tfa98xx_log_revision=0x%x, ",
		__func__, tfa98xx_log_revision);
	pr_debug("%s: tfa98xx_log_subrevision=0x%x, ",
		__func__, tfa98xx_log_subrevision);
	pr_debug("%s: tfa98xx_log_i2c_devicenum=/dev/i2c-%d, ",
		__func__, tfa98xx_log_i2c_devicenum);
	pr_debug("%s: tfa98xx_log_i2c_slaveaddress=0x%x, ",
		__func__, tfa98xx_log_i2c_slaveaddress);
	pr_info("%s: tfa98xx_log_start_cnt=%d\n",
		__func__, tfa98xx_log_start_cnt);

	if (devcount < 1) {
		pr_err("No or wrong container file loaded\n");
		return tfa_error_bad_param;
	}

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
	active_handle = -1;
#endif

	for (dev = 0; dev < devcount; dev++) {
		err = tfa_cont_open(dev);
		if (err != TFA98XX_ERROR_OK)
			goto error_exit;

		if (TFA_GET_BF(dev, MTPEX) == 0) {
			int cal_prof_idx = -2;

			cal_prof_idx = tfa_cont_get_cal_profile(dev);

			if (cal_prof_idx >= 0) {
				pr_info("%s: set profile for calibration cal_prof_idx %d\n",
					__func__, cal_prof_idx);
				next_profile = cal_prof_idx;
			}
		}

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
#if defined(TFA_PROFILE_ON_DEVICE)
		if (tfa_cont_is_dev_specific_profile(dev,
			next_profile) != 0)
			active_handle = dev; /* check activeness with profile */
#endif
#endif /* TFA_USE_DEVICE_SPECIFIC_CONTROL */
	}

	for (dev = 0; dev < devcount; dev++) {
		/* We need to remember the next_profile otherwise
		 * .cal profile overwrites this for the next dev
		 */
		profile = next_profile;

		/* Get currentprofile */
		active_profile = tfa_get_swprof(dev);
		if (active_profile == 0xff)
			active_profile = -1;

		/* Check if next profile is a tap profile */
		istap_prof = tfa_cont_is_tap_profile(dev, next_profile);

		/* tfaRun_SpeakerBoost implies un-mute */
		if (tfa98xx_runtime_verbose) {
			pr_debug("active_profile:%s, next_profile:%s\n",
				tfa_cont_profile_name(dev, active_profile),
				tfa_cont_profile_name(dev, profile));
			pr_debug("Starting device [%s]\n",
				tfa_cont_device_name(dev));

			if (tfa98xx_dev_family(dev) == 2)
				err = show_current_state(dev);
		}

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		if (active_handle != -1) {
			if (dev != active_handle) {
				err = _tfa_stop(dev); /* stop inactive handle */
				continue;
			}
		}
#endif

		pr_debug("%s: device[%d] [%s] - tfa_run_speaker_boost profile=%d\n",
			__func__, dev, tfa_cont_device_name(dev), profile);

		/* Check if we need coldstart or ACS is set */
		err = tfa_run_speaker_boost(dev, 0, profile);
		if (err != TFA98XX_ERROR_OK)
			goto error_exit;
		if (handles_local[dev].is_cold)
			/* separate cold flag
			 * to adjust contents in container file
			 */
			pr_info("%s:[cold] device:%s profile:%s\n",
				__func__, tfa_cont_device_name(dev),
				tfa_cont_profile_name(dev, profile));
		else
			pr_info("%s:[warm] device:%s profile:%s\n",
				__func__, tfa_cont_device_name(dev),
				tfa_cont_profile_name(dev, next_profile));
	}

	for (dev = 0; dev < devcount; dev++) {
		active_profile = tfa_get_swprof(dev);

		/* After loading calibration profile
		 * we need to load acoustic shock profile
		 */
		if (cal_profile >= 0) {
			next_profile = 0;
			pr_debug("Loading %s profile!\n",
				tfa_cont_profile_name(dev, next_profile));
		}

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		if (active_handle != -1) {
			if (dev != active_handle) {
				tfa_set_swprof(dev,
					(unsigned short)next_profile);
				tfa_set_swvstep(dev,
					(unsigned short)
					tfa_cont_get_current_vstep(dev));
				continue;
			}
		}
#endif

		/* check if the profile and steps are the one we want */
		/* was it not done already */
		if ((next_profile != active_profile && active_profile >= 0)
			|| (istap_prof == 1)) {
			handles_local[dev].is_bypass = 1; /* reset beforehand */
			err = tfa_cont_write_profile
				(dev, next_profile, vstep[dev]);
			if (err != TFA98XX_ERROR_OK)
				goto error_exit;
			if (handles_local[dev].is_cold == 0) {
				/* if (handles_local[dev].ext_dsp == 1) { */
				/* check only master device */
				if (handles_local[0].ext_dsp == 1) {
					pr_info("%s: flush buffer in blob, in warm start\n",
						__func__);
					/* master device only */
					err = tfa_tib_dsp_msgblob(0, -2, NULL);
				}
			}
		}

		/* If the profile contains the .standby suffix go
		 * to powerdown else we should be in operating state
		 */
		if (strnstr(tfa_cont_profile_name(dev, next_profile),
			".standby", strlen(tfa_cont_profile_name
			(dev, next_profile))) != NULL) {
			err = tfa98xx_powerdown(dev, 1);
		} else if (TFA_GET_BF(dev, PWDN) != 0) {
			err = tfa98xx_powerdown(dev, 0);
		}

		if (err != TFA98XX_ERROR_OK)
			goto error_exit;

		if (tfa98xx_runtime_verbose && tfa98xx_dev_family(dev) == 2)
			err = show_current_state(dev);

#ifdef __KERNEL__
		/* To write something in state 6 we need to be sure
		 * that SBSL is also set in IOMEM!
		 * More information can be found in the document
		 * about the powerswitch
		 */
		if (tfa98xx_dev_family(dev) == 2) {
			/* If we are in state 0 or 1 we can skip
			 * setting SBSL and RST, but also filters
			 * and vstep (since there is no clk)
			 * We can be in state 0 or 1
			 * because of standby profile or switching profile
			 * with internal clock
			 * No need to wait since we can only
			 * get out of state 0 or 1 by setting a bit
			 * (not by waiting)
			 */
			int manstate = TFA_GET_BF(dev, MANSTATE);

			if (manstate > 1) {
				/* Wait for the HW manager
				 * to be in state 6 (initCF)
				 * Absolute worstcase
				 * the HW manager takes 28ms (norm = ~3)
				 */
				while ((manstate < 6)
					&& (kernel_sleep_timeout < 28)) {
					kernel_sleep_timeout++;
					msleep_interruptible(1);
					manstate = TFA_GET_BF(dev, MANSTATE);
				}

				if ((manstate == 6)
					&& (TFA_GET_BF(dev, RST) == 1)
					&& (TFA_GET_BF(dev, SBSL) == 0)) {
					TFA_SET_BF_VOLATILE(dev, SBSL, 0);
					TFA_SET_BF(dev, RST, 0);
				}
			}
		}
#endif

		/* Optimize Intelligent boost trip level
		 * based on Re25 measurements
		 */
		err = tfa98xx_set_boost_trip_level();

		tfa_set_swprof(dev, (unsigned short)next_profile);
		tfa_set_swvstep
			(dev, (unsigned short)tfa_cont_get_current_vstep(dev));
	}

	for (dev = 0; dev < devcount; dev++) {
		if (tfa98xx_runtime_verbose && (tfa98xx_dev_family(dev) == 2))
			show_current_state(dev);

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
		if (active_handle != -1) {
			if (dev != active_handle) {
				tfa_cont_close(dev); /* close all of them */
				continue;
			}
		}
#endif

		tfa_run_unmute(dev);	/* unmute */
		tfa_cont_close(dev); /* close all of them */
	}

	return err;

error_exit:
	if (dev < devcount)
		if (tfa98xx_runtime_verbose && (tfa98xx_dev_family(dev) == 2))
			show_current_state(dev);

	for (dev = 0; dev < devcount; dev++)
		tfa_cont_close(dev); /* close all of them */

	return err;
}

enum tfa_error tfa_stop(void)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int dev, devcount = tfa98xx_cnt_max_device();
#if defined(RAMPING_DOWN_BEFORE_STOP)
	int ampgain = 0;
	int i;
#endif

	if (devcount == 0) {
		pr_err("No or wrong container file loaded\n");
		return	tfa_error_bad_param;
	}

	for (dev = 0; dev < devcount; dev++) {
		err = tfa_cont_open(dev);
		if (err != TFA98XX_ERROR_OK)
			goto error_exit;
		if (tfa98xx_runtime_verbose)
			pr_debug("Stopping device [%s]\n",
				tfa_cont_device_name(dev));

#if defined(RAMPING_DOWN_BEFORE_STOP)
		if (tfa98xx_dev_family(dev) == 2) {
			/* ramp down amplifier gain for 5 msec */
			ampgain = tfa_get_bf(dev, TFA2_BF_AMPGAIN);
			pr_debug("%s: ramp down ampgain (%d)\n",
				__func__, ampgain);
			for (i = 4; i >= 0; i--) {
				tfa_set_bf(dev,
					TFA2_BF_AMPGAIN,
					ampgain * i / 5);
				msleep_interruptible(1);
			}
		}
#endif

		err = _tfa_stop(dev);
		if (err != TFA98XX_ERROR_OK)
			goto error_exit;

		/* check only master device only */
		if (handles_local[0].ext_dsp == 1) {
			/* flush message buffer */
			pr_debug("%s: flush buffer in blob, at stop\n",
				__func__);
			err = tfa_tib_dsp_msgblob(dev, -2, NULL);
		}

#if defined(RAMPING_DOWN_BEFORE_STOP)
		if (tfa98xx_dev_family(dev) == 2) {
			/* restore amplifier gain */
			pr_debug("%s: restore ampgain (%d)\n",
				__func__, ampgain);
			tfa_set_bf(dev,
				TFA2_BF_AMPGAIN, ampgain);
		}
#endif
	}

error_exit:
	for (dev = 0; dev < devcount; dev++)
		tfa_cont_close(dev); /* close all of them */

	return err;
}

/*
 * int registers and coldboot dsp
 */
int tfa98xx_reset(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	err = -TFA_SET_BF_VOLATILE(handle, I2CR, 1);
	if (err)
		return err;

	/* restore MANSCONF to POR state */
	TFA_SET_BF_VOLATILE(handle, MANSCONF, 0);

	if ((handles_local[handle].rev & 0xff) != 0x72) {
		/* restore MANCOLD to POR state */
		TFA_SET_BF_VOLATILE(handle, MANCOLD, 1);
		/* powerup CF to access CF io */
		tfa98xx_powerdown(handle, 0);
		/* for clock */
		err = tfa_cf_powerup(handle);
		PRINT_ASSERT(err);

		/* force cold boot */
		err = tfa_run_coldboot(handle, 1); /* set ACS */
		PRINT_ASSERT(err);

		/* reset all i2C registers to default */
		err = -TFA_SET_BF(handle, I2CR, 1);
		PRINT_ASSERT(err);
	}

	return err;
}

enum tfa_error tfa_reset(void)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int dev, devcount = tfa98xx_cnt_max_device();

	for (dev = 0; dev < devcount; dev++) {
		err = tfa_cont_open(dev);
		if (err != TFA98XX_ERROR_OK)
			break;

		if (tfa98xx_runtime_verbose)
			pr_debug("resetting device [%s]\n",
				tfa_cont_device_name(dev));
		err = tfa98xx_reset(dev);
		if (err != TFA98XX_ERROR_OK)
			break;

		tfa_cont_close(dev);
	}

	return err;
}

/*
 * Write all the bytes specified by num_bytes and data
 */
enum tfa98xx_error
tfa98xx_write_data(tfa98xx_handle_t handle,
	unsigned char subaddress, int num_bytes,
	const unsigned char data[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	/* subaddress followed by data */
	const int bytes2write = num_bytes + 1;
	unsigned char *write_data;

	if (num_bytes > TFA2_MAX_PARAM_SIZE)
		return TFA98XX_ERROR_BAD_PARAMETER;

	write_data = kmalloc(bytes2write, GFP_KERNEL);
	if (write_data == NULL)
		return TFA98XX_ERROR_FAIL;

	write_data[0] = subaddress;
	memcpy(&write_data[1], data, num_bytes);

	error = tfa98xx_write_raw(handle, bytes2write, write_data);

	kfree(write_data);
	return error;
}

/*
 * fill the calibration value as milli ohms in the struct
 * assume that the device has been calibrated
 */
enum tfa98xx_error
tfa_dsp_get_calibration_impedance(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int spkr_count, nr_bytes, i, dev;
	/* unsigned char bytes[6] = {0}; */
	unsigned char bytes[TFACONT_MAXDEVS * 9] = {0}; /* 3 data */
	unsigned int scaled_data[2];
	unsigned int channel;
	/* int data[2]; */
	int data[3]; /* 3 data */

	error = tfa98xx_supported_speakers(handle, &spkr_count);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("error in checking supported speakers\n");
		return error;
	}

	if ((handles_local[handle].rev & 0xff) == 0x72) {
		if (TFA_GET_BF(handle, MTPOTC)
			&& TFA_GET_BF(handle, MTPEX)) {
			pr_debug("Getting calibration values from MTP\n");

			handles_local[handle].mohm[0] =
				tfa_read_reg(handle,
				(uint16_t)TFA_MK_BF((0xF5), 0, 16));

			/* stereo */
			if ((spkr_count > 1)
				&& ((handle + 1) < tfa98xx_cnt_max_device())) {
				dev = handle + 1;
				if (!tfa98xx_handle_is_open(dev)) {
					/* If handle is not
					 * open, try to open
					 */
					error = tfa_cont_open(dev);
					if (error != TFA98XX_ERROR_OK)
						return error;

					handles_local[dev].mohm[0] =
						tfa_read_reg(dev, (uint16_t)
						TFA_MK_BF((0xF5), 0, 16));

					tfa_cont_close(dev);
				} else {
					handles_local[dev].mohm[0] =
						tfa_read_reg(dev, (uint16_t)
						TFA_MK_BF((0xF5), 0, 16));
				}
			}
		} else {
			pr_debug("Getting calibration values from Speakerboost\n");
			/* nr_bytes = spkr_count * 3; */
			nr_bytes = spkr_count * 9; /* 3 data */
			pr_info("%s: read SB_PARAM_GET_RE25C\n",
				__func__);
			error = tfa_dsp_cmd_id_write_read
				(handle,
				MODULE_SPEAKERBOOST,
				SB_PARAM_GET_RE25C,
				nr_bytes, bytes);
			if (error == TFA98XX_ERROR_OK) {
				tfa98xx_convert_bytes2data
					(nr_bytes, bytes, data);

				/* signed data has`
				 * a limit of 30 Ohm
				 */
				/* scaled_data[0] = data[0]; */
				/* scaled_data[1] = data[1]; */
				scaled_data[0] = data[1];
				/* first data */
				scaled_data[1] = data[2];
				/* second data */

				for (i = 0; i < spkr_count; i++) {
					channel = tfa98xx_get_profile_tdmspks(i,
						handles_local[i].profile);
					handles_local[i].mohm[0] =
						(scaled_data[channel] * 1000)
						/ TFA_FW_ReZ_SCALE;
				}
			} else {
				for (i = 0; i < spkr_count; i++)
					handles_local[i].mohm[0] = -1;
			}
		}
	} else if (TFA_GET_BF(handle, MTPOTC)
		&& (((handles_local[handle].rev & 0xff) == 0x88)
		|| (handles_local[handle].rev & 0xff) == 0x13)) {
		if (tfa98xx_runtime_verbose)
			pr_debug("Getting calibration values from MTP\n");
		for (i = 0; i < spkr_count; i++) {
			handles_local[i].mohm[0] =
				tfa_read_reg(handle, (uint16_t)
				TFA_MK_BF((0xF4 + i), 0, 16));
		}
	} else {
		/* Get values from speakerboost */
		if (tfa98xx_runtime_verbose)
			pr_debug("Getting calibration values from Speakerboost\n");
		/* nr_bytes = spkr_count * 3; */
		nr_bytes = spkr_count * 9; /* 3 data */
		error = tfa_dsp_cmd_id_write_read
			(handle,
			MODULE_SPEAKERBOOST,
			SB_PARAM_GET_RE25C,
			nr_bytes, bytes);
		if (error == TFA98XX_ERROR_OK) {
			tfa98xx_convert_bytes2data
				(nr_bytes, bytes, data);
			for (i = 0; i < spkr_count; i++) {
				channel = tfa98xx_get_profile_tdmspks(i,
					handles_local[i].profile);
				handles_local[i].mohm[0] =
					(data[channel] * 1000)
					/ TFA_FW_ReZ_SCALE;
			}
		} else {
			for (i = 0; i < spkr_count; i++)
				handles_local[i].mohm[0] = -1;
		}
	}

	return error;
}

enum tfa98xx_error
tfa_mtp_get_calibration(tfa98xx_handle_t handle, uint16_t *cal_data)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	*cal_data = (uint16_t)tfa_read_reg
		(handle, (uint16_t)TFA_MK_BF(0xF5, 0, 16));

	return err;
}

enum tfa98xx_error
tfa_mtp_set_calibration(tfa98xx_handle_t handle, uint16_t cal_data)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int loop;

	tfa98xx_key2(handle, 0); /* unlock */
	/* Write calibration values to MTP shadow register */
	err = tfa_write_reg(handle, (uint16_t)TFA_MK_BF(0xF5, 0, 16), cal_data);
	if (err != TFA98XX_ERROR_OK)
		return err;

	/* CIMTP=1 start copying all the data from i2c regs_mtp to mtp*/
	err = TFA_SET_BF(handle, CIMTP, 1);
	if (err != TFA98XX_ERROR_OK)
		return err;

	/* no check for MTPBUSY here, i2c delay assumed to be enough */
	tfa98xx_key2(handle, 1); /* lock */

	/* wait until MTP write is done */
	for (loop = 0; loop < 100/*x10ms*/; loop++) {
		msleep_interruptible(10); /* wait 10ms to avoid busload */
		if (TFA_GET_BF(handle, MTPB) == 0)
			return TFA98XX_ERROR_OK;
	}

	return TFA98XX_ERROR_STATE_TIMED_OUT;
}

/* start count from 1, 0 is invalid */
int tfa_get_swprof(tfa98xx_handle_t handle)
{
	unsigned short rev = handles_local[handle].rev & 0xff;
	/* get from register if not set yet */
	if (handles_local[handle].profile < 0) {
		switch (rev) {
		case 0x72:{
			handles_local[handle].profile =
				tfa_get_bf(handle, TFA9872_BF_SWPROFIL) - 1;
			break;
		}
		case 0x88:{
			handles_local[handle].profile =
				TFA_GET_BF(handle, SWPROFIL) - 1;
			break;
		}
		case 0x13:{
			handles_local[handle].profile =
				tfa_get_bf(handle, TFA9912_BF_SWPROFIL) - 1;
			break;
		}
		default:
			break;
		}
	}

	return handles_local[handle].profile;
}

int tfa_set_swprof(tfa98xx_handle_t handle, unsigned short new_value)
{
	int mtpk, active_value = tfa_get_swprof(handle);
	unsigned short rev = handles_local[handle].rev & 0xff;

	handles_local[handle].profile = new_value;

	switch (rev) {
	case 0x72:
		tfa_set_bf_volatile(handle, TFA9872_BF_SWPROFIL, new_value + 1);
		break;
	case 0x88:
		TFA_SET_BF_VOLATILE(handle, SWPROFIL, new_value + 1);
		break;
	case 0x13:
		tfa_set_bf_volatile(handle, TFA9912_BF_SWPROFIL, new_value + 1);
		break;
	default:
		/* for max1 devices */
		/* it's in MTP shadow, so unlock if not done already */
		mtpk = TFA_GET_BF(handle, MTPK); /* get current key */
		TFA_SET_BF_VOLATILE(handle, MTPK, 0x5a);
		TFA_SET_BF_VOLATILE(handle, SWPROFIL, new_value + 1);
		/* set current profile */
		TFA_SET_BF_VOLATILE(handle, MTPK, (uint16_t)mtpk);
		/* restore key */
	}

	return active_value;
}

/* same value for all channels
 * start count from 1, 0 is invalid
 */
int tfa_get_swvstep(tfa98xx_handle_t handle)
{
	int value = 0;
	unsigned short rev = handles_local[handle].rev & 0xff;

	if (handles_local[handle].vstep[0] > 0)
		return handles_local[handle].vstep[0] - 1;

	switch (rev) {
	case 0x72:
		value = tfa_get_bf(handle, TFA9872_BF_SWVSTEP);
		break;
	case 0x88:
		value = TFA_GET_BF(handle, SWVSTEP);
		break;
	case 0x13:
		value = tfa_get_bf(handle, TFA9912_BF_SWVSTEP);
		break;
	}
	handles_local[handle].vstep[0] = value;
	handles_local[handle].vstep[1] = value;

	return value-1; /* invalid if 0 */
}

int tfa_set_swvstep(tfa98xx_handle_t handle, unsigned short new_value)
{
	int mtpk, active_value = tfa_get_swvstep(handle);
	unsigned short rev = handles_local[handle].rev & 0xff;

	handles_local[handle].vstep[0] = new_value;
	handles_local[handle].vstep[1] = new_value;

	switch (rev) {
	case 0x72:
		tfa_set_bf_volatile(handle, TFA9872_BF_SWVSTEP, new_value + 1);
		break;
	case 0x88:
		TFA_SET_BF_VOLATILE(handle, SWVSTEP, new_value + 1);
		break;
	case 0x13:
		tfa_set_bf_volatile(handle, TFA9912_BF_SWVSTEP, new_value + 1);
		break;
	default:
		/* for max1 devices */
		/* it's in MTP shadow, so unlock if not done already */
		mtpk = TFA_GET_BF(handle, MTPK); /* get current key */
		TFA_SET_BF_VOLATILE(handle, MTPK, 0x5a);
		TFA_SET_BF_VOLATILE(handle, SWVSTEP, new_value+1);
		/* set current vstep[0] */
		TFA_SET_BF_VOLATILE(handle, MTPK, (uint16_t)mtpk);
		/* restore key */
	}

	return active_value;
}

int tfa_is_cold(tfa98xx_handle_t handle)
{
	int value;

	/* if ((handles_local[handle].ext_dsp) */
	/* check only master device */
	if ((handles_local[0].ext_dsp)
		|| ((handles_local[handle].rev & 0xff) == 0x72)) {
		/* If device is already in operating state
		 * then it has to be warm! (for profile switching)
		 */
		if (TFA_GET_BF(handle, MANSTATE) == 9)
			value = 0; /* warm */
		else
			value = 1; /* cold */
	} else {
		value = TFA_GET_BF(handle, ACS);
	}

	return value;
}

int tfa_is_cold_amp(tfa98xx_handle_t handle)
{
	int value;

	/* if ((handles_local[handle].ext_dsp) */
	/* check only master device */
	if ((handles_local[0].ext_dsp)
		|| ((handles_local[handle].rev & 0xff) == 0x72))
		/*
		 * for non-dsp device reading MANSCONF
		 * (src_set_configured) is a way
		 * to check for cold boot status
		 */
		value = (TFA_GET_BF(handle, MANSCONF) == 0);
	else
		value = TFA_GET_BF(handle, ACS);

	return value;
}

int tfa_cf_enabled(tfa98xx_handle_t handle)
{
	int value;

	/* For 72 there is no CF */
	if ((handles_local[handle].rev & 0xff) == 0x72)
		/* value = (handles_local[handle].ext_dsp != 0); */
		/* check only master device */
		value = (handles_local[0].ext_dsp != 0);
	else
		value = TFA_GET_BF(handle, CFE);

	return value;
}

enum tfa98xx_error tfa98xx_set_boost_trip_level(void)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int dev, devcount = tfa98xx_cnt_max_device();
	unsigned short Re25C = 0, local_open = 0;

	for (dev = 0; dev < devcount; dev++) {
		if (!tfa98xx_handle_is_open(dev)) {
			/* If handle is not open, try to open */
			error = tfa_cont_open(dev);
			if (error != TFA98XX_ERROR_OK)
				return error;
			local_open = 1;
		}

		/* If we dont have an MMAP (72 stand alone)
		 * to access try reading the value from MTP
		 */
		if (Re25C == 0) {
			unsigned char mtp_reg_addr = 0xF5;
			/* mtp_reg_addr of tfa2 family as default */
			/* set the MTP register address,
			 * depending on the tfa device family
			 */

			if (handles_local[dev].tfa_family == 1)
				mtp_reg_addr = 0x83;
			else {
				if (handles_local[dev].tfa_family == 2)
					mtp_reg_addr = 0xF5;
			}

			Re25C = (unsigned short)tfa_read_reg(dev,
				(uint16_t)TFA_MK_BF
				((mtp_reg_addr), 0, 16));
		}

		if (handles_local[dev].dev_ops.tfa_set_boost_trip_level)
			error = (*handles_local[dev]
				.dev_ops.tfa_set_boost_trip_level)(dev, Re25C);

		/* If locally opened we also need to close! */
		if (local_open == 1)
			tfa_cont_close(dev);
	}

	return error;
}

/*
 * tfa external interfacing and event handling
 */
/**
 * parse the event and call driver in case of tfadsp event
 */
static int
tfa_dsp_handle_event(tfa98xx_handle_t handle,
	enum tfadsp_event_en tfadsp_event)
{
	int retval = handles_local[handle].rev; /* return revid by default */
	enum tfa_error err;

	switch (tfadsp_event) {
	case TFADSP_EXT_PWRUP: /* DSP starting */
		/* pr_info("set cold\n"); */
		handles_local[handle].ext_dsp = 1;
		handles_local[handle].is_cold = 1;
		break;
	case TFADSP_CMD_READY: /* Ready to receive commands */
		/* confirm */
		pr_info("TFADSP_CMD_READY: call tfa_start to send msgs\n");
		pr_info("TFADSP_CMD_READY: set cold\n");
		handles_local[handle].ext_dsp = 1;
		err = tfa_start
			(handles_local[handle].profile,
			handles_local[handle].vstep);
		if (err == tfa_error_ok) {
			handles_local[handle].ext_dsp = 2; /* set warm */
			handles_local[handle].is_cold = 0;
			retval = 0;
		} else
			retval = -1*err;
		break;
	/*
	 * case TFADSP_WARM: // config complete
	 *	enable amp
	 * send Re0
	 *	await pilot tone [audio power detect]
	 *	audio active
	 *	break;
	 */
	case TFADSP_EXT_PWRDOWN: /* DSP stopping */
		pr_info("disable ext dsp\n");
		handles_local[handle].ext_dsp = 0; /* unregister */
		/* amp off */
		break;
	case TFADSP_SOFT_MUTE_READY:
		/* [audio low power detect]: Muting completed */
		break;
	case TFADSP_VOLUME_READY: /* Volume change completed */
		break;
	case TFADSP_DAMAGED_SPEAKER: /* Damaged speaker was detected */
		/* amp off */
		break;
	default:
		pr_err("%s: unknown tfadsp event:0x%0x\n",
			__func__, tfadsp_event);
		retval = -1;
		break;
	}

	handles_local[handle].tfadsp_event = tfadsp_event;
	/* record active event */

	return retval;
}

/* Since this is only valid for Tiberius we can set this for 2 handles,
 * regardless if we are mono or stereo.
 */
/*	int tfa_ext_register(dsp_write_reg_t tfa_write_reg,
 *		dsp_send_message_t tfa_send_message,
 *		dsp_read_message_t tfa_read_message,
 *		tfa_event_handler_t *tfa_event_handler)
 */
int tfa_ext_register(dsp_send_message_t tfa_send_message,
	dsp_read_message_t tfa_read_message,
	tfa_event_handler_t *tfa_event_handler)
{
	/* handles_local[0].rev = 0x1b72; // added for smart studio. */

	/* pr_info("%s: device type (0x%02x)\n",
	 *	__func__, handles_local[0].rev);
	 * if (((handles_local[0].rev & 0xff) == 0x72)
	 *	|| (handles_local[0].ext_dsp != 0))
	 */
	{
	/*
	 *	if (tfa_write_reg != NULL) {
	 *		handles_local[0].dev_ops.reg_write =
	 *			(reg_write_t)tfa_write_reg;
	 *		handles_local[1].dev_ops.reg_write =
	 *			(reg_write_t)tfa_write_reg;
	 *	}
	 */

		/* master device only: external is always device 0,
		 * Don't do this for device 1!
		 */
		handles_local[0].ext_dsp = 1; /* set cold 1st msg */
		handles_local[0].is_cold = 1;

		handles_local[0].profile = 0;
		handles_local[1].profile = 0;

		handles_local[0].dev_ops.dsp_msg = (dsp_msg_t)tfa_send_message;
		handles_local[1].dev_ops.dsp_msg = (dsp_msg_t)tfa_send_message;

		handles_local[0].dev_ops.dsp_msg_read =
			(dsp_msg_read_t)tfa_read_message;
		handles_local[1].dev_ops.dsp_msg_read =
			(dsp_msg_read_t)tfa_read_message;

		if (tfa_event_handler != NULL)
			*tfa_event_handler = tfa_dsp_handle_event;
	}

	return 0;
}
EXPORT_SYMBOL(tfa_ext_register);

/* This is required to set the right status when remote is used */
void tfa_ext_set_ext_dsp(int value)
{
	handles_local[0].ext_dsp = value;
}

#define NR_COEFFS 6
#define NR_BIQUADS 28
#define BQ_SIZE (3 * NR_COEFFS)
#define DSP_MSG_OVERHEAD 27

#pragma pack(push, 1)
struct dsp_msg_all_coeff {
	uint8_t select_eq[3];
	uint8_t biquad[NR_BIQUADS][NR_COEFFS][3];
};
#pragma pack(pop)

/* number of biquads for each equalizer */
static const int eq_biquads[] = {
	10, 10, 2, 2, 2, 2
};

#define NR_EQ (int)(sizeof(eq_biquads) / sizeof(int))

enum tfa98xx_error
dsp_partial_coefficients(tfa98xx_handle_t dev_idx,
	uint8_t *prev, uint8_t *next)
{
	uint8_t bq, eq;
	int eq_offset;
	int new_cost, old_cost;
	uint32_t eq_biquad_mask[NR_EQ];
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct dsp_msg_all_coeff *data1 = (struct dsp_msg_all_coeff *)prev;
	struct dsp_msg_all_coeff *data2 = (struct dsp_msg_all_coeff *)next;

	old_cost = DSP_MSG_OVERHEAD + 3 + sizeof(struct dsp_msg_all_coeff);
	new_cost = 0;

	eq_offset = 0;
	for (eq = 0; eq < NR_EQ; eq++) {
		uint8_t *eq1 = &data1->biquad[eq_offset][0][0];
		uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

		eq_biquad_mask[eq] = 0;

		if (memcmp(eq1, eq2, BQ_SIZE*eq_biquads[eq]) != 0) {
			int nr_bq = 0;
			int bq_sz, eq_sz;

			for (bq = 0; bq < eq_biquads[eq]; bq++) {
				uint8_t *bq1 = &eq1[bq*BQ_SIZE];
				uint8_t *bq2 = &eq2[bq*BQ_SIZE];

				if (memcmp(bq1, bq2, BQ_SIZE) != 0) {
					eq_biquad_mask[eq] |= (1 << bq);
					nr_bq++;
				}
			}

			bq_sz = (2 * 3 + BQ_SIZE) * nr_bq;
			eq_sz = 2 * 3 + BQ_SIZE * eq_biquads[eq];

			/* dsp message i2c transaction overhead */
			bq_sz += DSP_MSG_OVERHEAD * nr_bq;
			eq_sz += DSP_MSG_OVERHEAD;

			if (bq_sz >= eq_sz) {
				eq_biquad_mask[eq] = 0xffffffff;

				new_cost += eq_sz;

			} else {
				new_cost += bq_sz;
			}
		}
		pr_debug("eq_biquad_mask[%d] = 0x%.8x\n",
			eq, eq_biquad_mask[eq]);

		eq_offset += eq_biquads[eq];
	}

	pr_debug("cost for writing all coefficients = %d\n", old_cost);
	pr_debug("cost for writing changed coefficients = %d\n", new_cost);

	if (new_cost >= old_cost) {
		const int buffer_sz = 3 + sizeof(struct dsp_msg_all_coeff);
		uint8_t *buffer;

		buffer = kmalloc(buffer_sz, GFP_KERNEL);
		if (buffer == NULL)
			return TFA98XX_ERROR_FAIL;

		/* cmd id */
		buffer[0] = 0x00;
		buffer[1] = 0x82;
		buffer[2] = 0x00;

		/* parameters */
		memcpy(&buffer[3], data2, sizeof(struct dsp_msg_all_coeff));

		err = dsp_msg(dev_idx, buffer_sz, (const char *)buffer);

		kfree(buffer);

		return err;
	}

	/* (new_cost < old_cost) */
	eq_offset = 0;
	for (eq = 0; eq < NR_EQ; eq++) {
		uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

		if (eq_biquad_mask[eq] == 0xffffffff) {
			const int msg_sz = 6 + BQ_SIZE * eq_biquads[eq];
			uint8_t *msg;

			msg = kmalloc(msg_sz, GFP_KERNEL);
			if (msg == NULL)
				return TFA98XX_ERROR_FAIL;

			/* cmd id */
			msg[0] = 0x00;
			msg[1] = 0x82;
			msg[2] = 0x00;

			/* select eq and bq */
			msg[3] = 0x00;
			msg[4] = eq + 1;
			msg[5] = 0x00; /* all biquads */

			/* biquad parameters */
			memcpy(&msg[6], eq2, BQ_SIZE * eq_biquads[eq]);

			err = dsp_msg(dev_idx, msg_sz, (const char *)msg);

			kfree(msg);
			if (err)
				return err;
		} else if (eq_biquad_mask[eq] != 0) {
			const int msg_sz = 6 + BQ_SIZE;
			uint8_t *msg;

			msg = kmalloc(msg_sz, GFP_KERNEL);
			if (msg == NULL)
				return TFA98XX_ERROR_FAIL;

			for (bq = 0; bq < eq_biquads[eq]; bq++) {
				if (eq_biquad_mask[eq] & (1 << bq)) {
					uint8_t *bq2
						= &eq2[bq * BQ_SIZE];

					/* cmd id */
					msg[0] = 0x00;
					msg[1] = 0x82;
					msg[2] = 0x00;

					/* select eq and bq */
					msg[3] = 0x00;
					msg[4] = eq + 1;
					msg[5] = bq + 1;

					/* biquad parameters */
					memcpy(&msg[6], bq2, BQ_SIZE);

					err = dsp_msg(dev_idx, msg_sz,
						(const char *)msg);

					if (err) {
						kfree(msg);
						return err;
					}
				}
			}

			kfree(msg);
		}
		eq_offset += eq_biquads[eq];
	}

	return err;
}

int get_manager_state(tfa98xx_handle_t handle)
{
	int manstate = -1;

	/* First we need to find the current manager state */
	if (tfa98xx_dev_family(handle) == 2) {
		manstate = TFA_GET_BF(handle, MANSTATE);
	} else {
		/* For max1 we dont have a manager state bitfield
		 * so we need to determine the Manager state ourself
		 */
		if (TFA_GET_BF(handle, PWDN) == 1) {
			/* we are in powerdown state */
			manstate = 0;
		} else if (TFA_GET_BF(handle, SBSL) == 0) {
			/* we are in initCF state */
			manstate = 6;
		} else if (TFA_GET_BF(handle, AMPS) == 1) {
			/* we are in operating state */
			manstate = 9;
		} else {
			/* we are in unknown state? */
			if (tfa98xx_runtime_verbose)
				pr_debug("Error: Unknown state!\n");
			return TFA_ERROR;
		}
	}

	return manstate;
}

int set_manager_state(tfa98xx_handle_t handle, int state_nr)
/* TODO CHANGE INT TO ENUM! */
{
	int manstate, not_possible = 0;

	/* Get the current manager state */
	manstate = get_manager_state(handle);
	if (manstate < 0)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (tfa98xx_runtime_verbose)
		pr_debug("Current HW manager state for device %d is %d\n",
			handle, manstate);

	if (state_nr == manstate) {
		if (tfa98xx_runtime_verbose)
			pr_debug("HW manager state for device %d is already correct\n",
				handle);

		return TFA98XX_ERROR_OK;
	}

	/* It is not possible to switch to every state:
	 * For example: From powerdown to operating is not possible,
	 * we cannot do this with setting bitfield alone
	 */
	if (tfa98xx_dev_family(handle) == 2) {
		if (manstate == 0 && (state_nr != 6 && state_nr != 1))
			not_possible = 1;
		else if (manstate == 1 && (state_nr != 6 && state_nr != 0))
			not_possible = 1;
		else if (manstate == 6 && (state_nr != 9 && state_nr != 0))
			not_possible = 1;
		else if (manstate == 9 && state_nr != 0)
			not_possible = 1;
		else {
			/* [11/14/2016]: Needs more testing
			 * to find the corner cases!
			 */
			switch (state_nr) {
			case 0:
				TFA_SET_BF(handle, PWDN, 1);
				break;
			case 1:
				TFA_SET_BF(handle, SBSL, 0);
				TFA_SET_BF(handle, MANSCONF, 0);
				TFA_SET_BF(handle, PWDN, 0);
				break;
			case 6:
				TFA_SET_BF(handle, SBSL, 0);
				TFA_SET_BF(handle, MANSCONF, 1);
				TFA_SET_BF(handle, PWDN, 0);
				break;
			case 9:
				TFA_SET_BF(handle, SBSL, 1);
				break;
			}
		}
	} else {
		if (manstate == 0 && state_nr != 6)
			not_possible = 1;
		else if (manstate == 6 && (state_nr != 9 && state_nr != 0))
			not_possible = 1;
		else if (manstate == 9 && state_nr != 0)
			not_possible = 1;
		else {
			/* [11/14/2016]: Needs more testing
			 * to find the corner cases!
			 */
			switch (state_nr) {
			case 0:
				TFA_SET_BF(handle, PWDN, 1);
				break;
			case 6:
				TFA_SET_BF(handle, SBSL, 0);
				TFA_SET_BF(handle, PWDN, 0);
				break;
			case 9:
				TFA_SET_BF(handle, SBSL, 1);
				break;
			}
		}
	}

	if (not_possible) {
		pr_debug("Error: It is not possible to switch to state %d from current state %d\n",
			state_nr, manstate);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	msleep_interruptible(100);
	/* wait 100ms to get the correct state */
	manstate = get_manager_state(handle);
	if (manstate < 0)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (tfa98xx_runtime_verbose)
		pr_debug("New HW manager state for device %d is %d\n",
			handle, manstate);

	return TFA98XX_ERROR_OK;
}

/*****************************************************************************/
enum tfa98xx_error
tfa_tfadsp_get_calibration_impedance(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int spkr_count, nr_bytes, i;
	/* unsigned char bytes[6] = {0}; */
	unsigned char bytes[TFACONT_MAXDEVS * 9] = {0}; /* 3 data */
	unsigned int scaled_data[2];
	unsigned int channel;
	int data[3]; /* 3 data */

	if ((handles_local[handle].stream_state & BIT_PSTREAM) != 1) {
		pr_info("%s: no playback; getting calibration fail from tfadsp\n",
			__func__);
		return TFA98XX_ERROR_RPC_CALIB_FAILED;
	}

	error = tfa98xx_supported_speakers(handle, &spkr_count);

	if (error == TFA98XX_ERROR_OK) {
		pr_debug("%s: get calibration values from tfadsp\n",
			__func__);
		nr_bytes = spkr_count * 9; /* 3 data */
		pr_info("%s: read SB_PARAM_GET_RE25C\n", __func__);
		error = tfa_dsp_cmd_id_write_read
			(handle,
			MODULE_SPEAKERBOOST,
			SB_PARAM_GET_RE25C,
			nr_bytes, bytes);
		if (error == TFA98XX_ERROR_OK) {
			tfa98xx_convert_bytes2data(nr_bytes, bytes, data);

			/* signed data has a limit of 30 Ohm */
			scaled_data[0] = data[1]; /* first data */
			scaled_data[1] = data[2]; /* second data */

			for (i = 0; i < spkr_count; i++) {
				channel = tfa98xx_get_profile_tdmspks(i,
					handles_local[i].profile);
				handles_local[i].mohm[0] =
					(scaled_data[channel] * 1000)
					/ TFA_FW_ReZ_SCALE;
			}
		} else {
			pr_err("%s: tfa_dsp_cmd_id_write_read is failed, err=%d\n",
				__func__, error);
			for (i = 0; i < spkr_count; i++)
				handles_local[i].mohm[0] = -1;
		}
	} else {
		pr_err("%s: tfa98xx_supported_speakers is failed, err=%d\n",
			__func__, error);
	}

	return error;
}

enum tfa98xx_error tfa_tfadsp_calibrate(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int cal_prof_idx = 0, cur_prof_idx = 0;
	int i, spkr_count = 1;

	if ((handles_local[handle].stream_state & BIT_PSTREAM) != 1) {
		pr_info("%s: no playback; speaker init calibration fail\n",
			__func__);
		return TFA98XX_ERROR_RPC_CALIB_FAILED;
	}

	pr_info("%s: begin\n", __func__);

	err = tfa98xx_supported_speakers(handle, &spkr_count);

	for (i = 0; i < spkr_count; i++) {
		if (TFA_GET_BF(i, MTPEX) == 1) {
			pr_debug("[%d] speaker is already calibrated; reset MTPEX first\n",
				i);
			/* reset MTPEX */
			err = tfa98xx_set_mtp(i,
				0 << TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS,
				TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
			if (err) {
				pr_err("%s: [%d] setting MPTEX failed, err (%d)\n",
					__func__, i, err);
				return TFA98XX_ERROR_RPC_CALIB_FAILED;
			}
		}
	}

	cur_prof_idx = tfa_get_swprof(handle);
	cal_prof_idx = tfa_cont_get_cal_profile(handle);
	if (cal_prof_idx < 0)
		cal_prof_idx = 0;

	/* set cal profile */
	pr_info("%s: set profile for calibration cal_prof_idx %d (from %d)\n",
		__func__, cal_prof_idx, cur_prof_idx);
	for (i = 0; i < spkr_count; i++) {
		handles_local[i].is_bypass = 0; /* force at calibration */
		err = tfa_cont_write_files(i);
		PRINT_ASSERT(err);
		err = tfa_cont_write_regs_prof(i, cal_prof_idx);
		PRINT_ASSERT(err);
		err = tfa_cont_write_files_prof(i, cal_prof_idx, 0);
		PRINT_ASSERT(err);

		tfa_set_swprof(i, (unsigned short)cal_prof_idx);
	}

	pr_info("%s: set_calibration (cal)\n", __func__);
	/* SetRe25C with {0, 0} */
#if defined(SET_CALIBRATION_AT_ALL_DEVICE_READY)
	/* force to set calibrate all by calling last device */
	err = tfa_set_calibration_values(tfa98xx_cnt_max_device() - 1);
#else
	err = tfa_set_calibration_values(handle);
#endif
	PRINT_ASSERT(err);

	/* run calibration */
#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
	if (TFA_GET_BF(handle, MTPEX) == 0)
		err = tfa_tfadsp_wait_calibrate_done(handle);
#else
	err = tfa_tfadsp_wait_calibrate_done(handle);
#endif

	/* restore profile */
	pr_info("%s: set profile for restoration cur_prof_idx %d\n",
		__func__, cur_prof_idx);
	for (i = 0; i < spkr_count; i++) {
		handles_local[i].is_bypass = 1; /* reset beforehand */
		err = tfa_cont_write_files(i);
		PRINT_ASSERT(err);
		err =  tfa_cont_write_regs_prof(i, cur_prof_idx);
		PRINT_ASSERT(err);
		err = tfa_cont_write_files_prof(i, cur_prof_idx, 0);
		PRINT_ASSERT(err);

		tfa_set_swprof(i, (unsigned short)cur_prof_idx);

		pr_info("%s: set_calibration (post-cal)\n", __func__);
		/* SetRe25C with calibrated values */
		err = tfa_set_calibration_values(i);
		PRINT_ASSERT(err);
	}

	pr_info("%s: end\n", __func__);

	return err;
}

static enum tfa98xx_error
tfa_tfadsp_wait_calibrate_done(tfa98xx_handle_t handle)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa98xx_controls *controls =
		&(handles_local[handle].dev_ops.controls);
	char buffer[12] = {0};
	int calibration_done = 0;
	int otc_val = 1;
	uint16_t readback;
	int count;
	int i, spkr_count = 1;
	unsigned int channel;

	pr_info("%s: begin\n", __func__);

	controls->calib.wr_value = true; /* request was triggered from here */
	controls->calib.rd_valid = false; /* result not available */
	controls->calib.rd_value = false; /* result not valid (default) */

	pr_info("%s: wait until calibration is done\n", __func__);
	for (count = 0;
		count < MAX_WAIT_COUNT_UNTIL_CALIBRATION_DONE; count++) {
		err = tfa_dsp_cmd_id_write_read
			(handle, MODULE_FRAMEWORK, FW_PAR_ID_GET_STATUS_CHANGE,
			9 /* 3 data */, (unsigned char *)buffer);
		msleep_interruptible(100);
		pr_debug("%s: err (%d), buffer (0x%x), count (%d)\n",
			__func__, err, buffer[2], count);
		if (buffer[8] & 0x1)
			break;
	}

	err = tfa98xx_supported_speakers(handle, &spkr_count);

	if (count >= MAX_WAIT_COUNT_UNTIL_CALIBRATION_DONE) {
		pr_err("%s: calibration failed from time out\n", __func__);

		/* inform which speaker is not calibrated */
		if (spkr_count == 1) {
			handles_local[0].spkr_damaged = buffer[8] & 0x1;
		} else if ((spkr_count == 2)
			&& !(buffer[8] & 0x1)) {
			for (i = 0; i < spkr_count; i++) {
				channel = tfa98xx_get_profile_tdmspks(i,
					handles_local[i].profile);

				switch (channel) {
				case 0: /* Primary channel */
					handles_local[i].spkr_damaged =
						((buffer[8] & 0x2) != 0)
						? 1 : 0;
					break;
				case 1: /* Secondary channel */
					handles_local[i].spkr_damaged =
						((buffer[8] & 0x4) != 0)
						? 1 : 0;
					break;
				default:
					break;
				}

				if (handles_local[i].spkr_damaged)
					pr_info("%s: failed to calibrate speaker [%s] (dev %d, channel %d)\n",
						__func__,
						tfa_cont_device_name(i),
						i, channel);
			}
		}

		err = TFA98XX_ERROR_RPC_CALIB_BUSY;
		goto wait_calibrate_done_error_exit;
	}

	err = tfa_tfadsp_get_calibration_impedance(handle);
	if (err) {
		pr_err("%s: getting calibration data failed, err (%d)\n",
			__func__, err);
		err = TFA98XX_ERROR_RPC_CALIB_FAILED;
		goto wait_calibrate_done_error_exit;
	}

	pr_info("%s: calibration done\n", __func__);
	calibration_done = 1;

	if (spkr_count == 1)
		pr_debug("P: %d mOhms\n", handles_local[handle].mohm[0]);
	else
		pr_debug("P: %d mOhms, S: %d mOhms\n",
			handles_local[0].mohm[0], handles_local[1].mohm[0]);

#if defined(CHECK_CALIBRATION_DATA_RANGE)
	for (i = 0; i < spkr_count; i++) {
		channel = tfa98xx_get_profile_tdmspks(i,
			handles_local[i].profile);
		err = tfa_calibration_range_check(i,
			channel, handles_local[i].mohm[0]);
		if (err) {
			calibration_done = 0;
			pr_err("%s: calibration data is out of range: device %d\n",
				__func__, i);
			err = TFA98XX_ERROR_BAD_PARAMETER;
			goto wait_calibrate_done_error_exit;
		}
	}
#endif

#if defined(WRITE_CALIBRATION_DATA_TO_MTP)
	for (i = 0; i < spkr_count; i++) {
		if ((handles_local[i].mohm[0] > 0) && calibration_done) {
			err = tfa_mtp_set_calibration
				(i, handles_local[i].mohm[0]);
			if (err) {
				pr_err("%s: writing calibration data failed to MTP, device %d err (%d)\n",
					__func__, i, err);
				err = TFA98XX_ERROR_RPC_CALIB_FAILED;
				goto wait_calibrate_done_error_exit;
			}

			/* readback after writing onto MTP */
			err = tfa_mtp_get_calibration(i, &readback);
			if (err) {
				pr_err("%s: reading calibration data back failed from MTP, device %d err (%d)\n",
					__func__, i, err);
				err = TFA98XX_ERROR_RPC_CALIB_FAILED;
				goto wait_calibrate_done_error_exit;
			}

			/* write again if readback is out of range */
			pr_info("%s: calibaration data %d, readback from MTP, device %d\n",
				__func__, readback, i);
#if defined(CHECK_CALIBRATION_DATA_RANGE)
			channel = tfa98xx_get_profile_tdmspks(i,
				handles_local[i].profile);
			err = tfa_calibration_range_check(i,
				channel, readback);
#else
			err = (readback == 0) ?
				TFA98XX_ERROR_BAD_PARAMETER : TFA98XX_ERROR_OK;
#endif
			if (err) {
				err = tfa_mtp_set_calibration
					(i, handles_local[i].mohm[0]);
				if (err) {
					pr_err("%s: writing calibration data failed to MTP (2nd), device %d err (%d)\n",
						__func__, i, err);
					err = TFA98XX_ERROR_RPC_CALIB_FAILED;
					goto wait_calibrate_done_error_exit;
				}
			}

			if (controls->otc.deferrable
				&& controls->otc.triggered) {
				otc_val = controls->otc.wr_value;
				pr_debug("Deferred writing otc = %d\n",
					otc_val);
			} else {
				otc_val = 1;
			}

			/* set OTC <0:always 1:once> */
			err = tfa98xx_set_mtp(i, (otc_val & 1)
				<< TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS,
				TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK);
			if (err) {
				pr_info("%s: setting OTC failed, device %d err (%d)\n",
					__func__, i, err);
				err = TFA98XX_ERROR_RPC_CALIB_FAILED;
				goto wait_calibrate_done_error_exit;
			} else {
				controls->otc.triggered = false;
				controls->otc.wr_value = otc_val;
				controls->otc.rd_value = otc_val;
			}

			/* set MTPEX */
			err = tfa98xx_set_mtp(i,
				1 << TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS,
				TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
			if (err) {
				pr_err("%s: setting MPTEX failed, device %d err (%d)\n",
					__func__, i, err);
				err = TFA98XX_ERROR_RPC_CALIB_FAILED;
				goto wait_calibrate_done_error_exit;
			}

			/* readback after writing onto MTP and setting MTPEX */
			err = tfa_mtp_get_calibration(i, &readback);
			if (err) {
				pr_err("%s: reading calibration data back failed from MTP (2nd), device %d err (%d)\n",
					__func__, i, err);
				err = TFA98XX_ERROR_RPC_CALIB_FAILED;
				goto wait_calibrate_done_error_exit;
			}

			/* panic if readback is out of range */
			pr_info("%s: calibaration data %d readback from MTP (2nd), device %d\n",
				__func__, readback, i);
#if defined(CHECK_CALIBRATION_DATA_RANGE)
			channel = tfa98xx_get_profile_tdmspks(i,
				handles_local[i].profile);
			err = tfa_calibration_range_check(i,
				channel, readback);
#else
			err = (readback == 0) ?
				TFA98XX_ERROR_BAD_PARAMETER : TFA98XX_ERROR_OK;
#endif
			if (err) {
				pr_err("%s: calibration data (%d) is out of range, readback from MTP, device %d\n",
					__func__, readback, i);
				panic("[AUDIO_BSP] SPK INIT CALIBRATION Fail.");
			}
		}
	}
#endif /* WRITE_CALIBRATION_DATA_TO_MTP */

	pr_info("%s: end\n", __func__);

wait_calibrate_done_error_exit:
	if (err) {
		controls->calib.wr_value = false;
		/* request was not in triggered state */
		controls->calib.rd_valid = false;
		/* result not available */
		controls->calib.rd_value = false;
		/* result not valid (default) */
	} else {
		controls->calib.wr_value = false;
		/* request was not in triggered state */
		controls->calib.rd_valid = true;
		/* result available */
		controls->calib.rd_value = true;
		/* result valid */
	}

	return err;
}

#if defined(CHECK_CALIBRATION_DATA_RANGE)
#define TFA_CAL_RANGE(max_devcnt, handle, type, revid) \
	((max_devcnt != 2) ? type##_LIMIT_CAL_##revid : \
	((handle == 0) ? \
	type##_LIMIT_CAL_P_##revid : type##_LIMIT_CAL_S_##revid))
#define LOWER_LIMIT_CAL_N3B 0
#define UPPER_LIMIT_CAL_N3B 32000
#define LOWER_LIMIT_CAL_N1A 0
#define UPPER_LIMIT_CAL_N1A 32000
#define LOWER_LIMIT_CAL_P_N3B 0
#define UPPER_LIMIT_CAL_P_N3B 32000
#define LOWER_LIMIT_CAL_P_N1A 0
#define UPPER_LIMIT_CAL_P_N1A 32000
#define LOWER_LIMIT_CAL_S_N3B 0
#define UPPER_LIMIT_CAL_S_N3B 32000
#define LOWER_LIMIT_CAL_S_N1A 0
#define UPPER_LIMIT_CAL_S_N1A 32000

enum tfa98xx_error
tfa_calibration_range_check(tfa98xx_handle_t handle,
	unsigned int channel, int mohm)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int devcount = tfa98xx_cnt_max_device();
	int lower_limit_cal, upper_limit_cal;

	lower_limit_cal = TFA_CAL_RANGE(devcount, channel, LOWER, N3B);
	upper_limit_cal = TFA_CAL_RANGE(devcount, channel, UPPER, N3B);
	if (((handles_local[handle].rev >> 8) & 0xff) == 0x1a) {
		pr_info("%s: Change N1A Calibration range", __func__);
		lower_limit_cal = TFA_CAL_RANGE(devcount, channel, LOWER, N1A);
		upper_limit_cal = TFA_CAL_RANGE(devcount, channel, UPPER, N1A);
	}

	pr_info("%s: channel [%d] - calibration range check [%d, %d] with %d\n",
		__func__, channel, lower_limit_cal, upper_limit_cal, mohm);
	if (mohm < lower_limit_cal || mohm > upper_limit_cal)
		err = TFA98XX_ERROR_BAD_PARAMETER;

	return err;
}
#endif /* CHECK_CALIBRATION_DATA_RANGE */
/**************************************************************************/
