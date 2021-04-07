/*
 * Copyright 2014,215 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or later
 * as published by the Free Software Foundation.
 */

#include "tfa_dsp_fw.h"
#include "tfa_service.h"
#include "tfa_internal.h"

#include "tfa98xx_tfafieldnames.h"

static enum tfa98xx_error tfa9895_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int result;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	/* all i2C registers are already set to default */

	result = TFA_SET_BF(handle, AMPE, 1);
	if (result < 0)
		return -result;

	/* some other registers must be set for optimal amplifier behaviour */
	reg_write(handle, 0x05, 0x13AB);
	reg_write(handle, 0x06, 0x001F);
	/* peak voltage protection is always on, but may be written */
	reg_write(handle, 0x08, 0x3C4E);
	/*TFA98XX_SYSCTRL_DCA=0*/
	reg_write(handle, 0x09, 0x024D);
	reg_write(handle, 0x41, 0x0308);
	error = reg_write(handle, 0x49, 0x0E82);

	return error;
}

static enum tfa98xx_error tfa9890_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short regRead = 0;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	/* all i2C registers are already set to default for N1C2 */

	/* some PLL registers must be set optimal for amplifier behaviour
	 */
	error = reg_write(handle, 0x40, 0x5a6b);
	if (error)
		return error;
	reg_read(handle, 0x59, &regRead);
	regRead |= 0x3;
	reg_write(handle, 0x59, regRead);
	error = reg_write(handle, 0x40, 0x0000);

	error = reg_write(handle, 0x47, 0x7BE1);

	return error;
}

static enum tfa98xx_error tfa9891_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	/* ----- generated code start ----- */
	/* -----  version 18.0 ----- */
	reg_write(handle, 0x09, 0x025d); /* POR=0x024d */
	reg_write(handle, 0x10, 0x0018); /* POR=0x0024 */
	reg_write(handle, 0x22, 0x0003); /* POR=0x0023 */
	reg_write(handle, 0x25, 0x0001); /* POR=0x0000 */
	reg_write(handle, 0x46, 0x0000); /* POR=0x4000 */
	reg_write(handle, 0x55, 0x3ffb); /* POR=0x7fff */
	/* ----- generated code end   ----- */

	return error;
}

static enum tfa98xx_error tfa9896_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short check_value;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	/* all i2C registers must already set to default POR value */

	/* $48:[3] - 1 ==> 0; iddqtestbst - default value changed.
	 * When Iddqtestbst is set to "0", the slewrate is reduced.
	 * This will lower overshoot on IN-B to avoid NMOS damage of booster.
	 */
	/* ----- generated code start ----- */
	/* v17 */
	reg_write(handle, 0x06, 0x000b); /* POR=0x0001 */
	reg_write(handle, 0x07, 0x3e7f); /* POR=0x1e7f */
	reg_write(handle, 0x0a, 0x0d8a); /* POR=0x0592 */
	reg_write(handle, 0x48, 0x0300); /* POR=0x0308 */
	reg_write(handle, 0x88, 0x0100); /* POR=0x0000 */
	/* ----- generated code end   ----- */
	/* $49:[0] - 1 ==> 0; CLIP - default value changed. 0: CLIPPER on */
	error = reg_read(handle, 0x49, &check_value);
	check_value &= ~0x1;
	error = reg_write(handle, 0x49, check_value);

	return error;
}

static enum tfa98xx_error tfa9897_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short check_value;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	/* all i2C registers must already set to default POR value */

	/* $48:[3] - 1 ==> 0; iddqtestbst - default value changed.
	 * When Iddqtestbst is set to "0", the slewrate is reduced.
	 * This will lower overshoot on IN-B to avoid NMOS damage of booster.
	 */
	error = reg_write(handle, 0x48, 0x0300); /* POR value = 0x308 */

	/* $49:[0] - 1 ==> 0; CLIP - default value changed. 0: CLIPPER on */
	error = reg_read(handle, 0x49, &check_value);
	check_value &= ~0x1;
	error = reg_write(handle, 0x49, check_value);

	return error;
}

static enum tfa98xx_error tfa9888_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short value, xor;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	if ((handles_local[handle].rev & 0xff) != 0x88) {
		pr_err("This code is not for this device type: %x\n",
		       handles_local[handle].rev);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	/* Unlock keys to write settings */
	error = reg_write(handle, 0x0F, 0x5A6B);
	error = reg_read(handle, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(handle, 0xA0, xor);

	/* The optimal settings are different for 1c, 2c, 3b and 2b/1b */
	if (handles_local[handle].rev == 0x2c88) {
		/* ----- generated code start ----- */
		/* --------- Version v1 ---------- */
		reg_write(handle, 0x00, 0x164d); /* POR=0x064d */
		reg_write(handle, 0x01, 0x828b); /* POR=0x92cb */
		reg_write(handle, 0x02, 0x1dc8); /* POR=0x1828 */
		reg_write(handle, 0x0e, 0x0080); /* POR=0x0000 */
		reg_write(handle, 0x20, 0x089e); /* POR=0x0890 */
		reg_write(handle, 0x22, 0x543c); /* POR=0x545c */
		reg_write(handle, 0x23, 0x0006); /* POR=0x0000 */
		reg_write(handle, 0x24, 0x0014); /* POR=0x0000 */
		reg_write(handle, 0x25, 0x000a); /* POR=0x0000 */
		reg_write(handle, 0x26, 0x0100); /* POR=0x0000 */
		reg_write(handle, 0x28, 0x1000); /* POR=0x0000 */
		reg_write(handle, 0x51, 0x0000); /* POR=0x00c0 */
		reg_write(handle, 0x52, 0xfafe); /* POR=0xbaf6 */
		reg_write(handle, 0x70, 0x3ee4); /* POR=0x3ee6 */
		reg_write(handle, 0x71, 0x1074); /* POR=0x3074 */
		reg_write(handle, 0x83, 0x0014); /* POR=0x0013 */
		/* ----- generated code end   ----- */
	} else if (handles_local[handle].rev == 0x1c88) {
		/* ----- generated code start ----- */
		/* --------- Version v6 ---------- */
		reg_write(handle, 0x00, 0x164d); /* POR=0x064d */
		reg_write(handle, 0x01, 0x828b); /* POR=0x92cb */
		reg_write(handle, 0x02, 0x1dc8); /* POR=0x1828 */
		reg_write(handle, 0x0e, 0x0080); /* POR=0x0000 */
		reg_write(handle, 0x20, 0x089e); /* POR=0x0890 */
		reg_write(handle, 0x22, 0x543c); /* POR=0x545c */
		reg_write(handle, 0x23, 0x0006); /* POR=0x0000 */
		reg_write(handle, 0x24, 0x0014); /* POR=0x0000 */
		reg_write(handle, 0x25, 0x000a); /* POR=0x0000 */
		reg_write(handle, 0x26, 0x0100); /* POR=0x0000 */
		reg_write(handle, 0x28, 0x1000); /* POR=0x0000 */
		reg_write(handle, 0x51, 0x0000); /* POR=0x00c0 */
		reg_write(handle, 0x52, 0xfafe); /* POR=0xbaf6 */
		reg_write(handle, 0x70, 0x3ee4); /* POR=0x3ee6 */
		reg_write(handle, 0x71, 0x1074); /* POR=0x3074 */
		reg_write(handle, 0x83, 0x0014); /* POR=0x0013 */
		/* ----- generated code end   ----- */
	} else if (handles_local[handle].rev == 0x3b88) {
		/* ----- generated code start ----- */
		/* --------- Version v20 ---------- */
		reg_write(handle, 0x01, 0x828b); /* POR=0x92cb */
		reg_write(handle, 0x02, 0x1dc8); /* POR=0x1828 */
		reg_write(handle, 0x20, 0x089e); /* POR=0x0890 */
		reg_write(handle, 0x22, 0x543c); /* POR=0x545c */
		reg_write(handle, 0x23, 0x0c06); /* POR=0x0000 */
		reg_write(handle, 0x24, 0x0014); /* POR=0x0000 */
		reg_write(handle, 0x25, 0x000a); /* POR=0x0000 */
		reg_write(handle, 0x26, 0x0100); /* POR=0x0000 */
		reg_write(handle, 0x28, 0x1000); /* POR=0x0000 */
		reg_write(handle, 0x51, 0x0000); /* POR=0x00c0 */
		reg_write(handle, 0x52, 0xfafe); /* POR=0xbaf6 */
		reg_write(handle, 0x58, 0x1e1c); /* POR=0x161c */
		reg_write(handle, 0x70, 0x3ee4); /* POR=0x3ee6 */
		reg_write(handle, 0x71, 0x1074); /* POR=0x3074 */
		reg_write(handle, 0x83, 0x0014); /* POR=0x0013 */
		/* ----- generated code end   -----  */
	} else {
		/* If not 1c or 3b assume older version */
		/* ----- generated code start ----- */
		/* --------- Version v19 ---------- */
		reg_write(handle, 0x00, 0x1e5d); /* POR=0x064d */
		reg_write(handle, 0x01, 0x828b); /* POR=0x92cb */
		reg_write(handle, 0x20, 0x089e); /* POR=0x0890 */
		reg_write(handle, 0x23, 0x0c06); /* POR=0x0000 */
		reg_write(handle, 0x24, 0x0014); /* POR=0x0000 */
		reg_write(handle, 0x25, 0x000a); /* POR=0x0000 */
		reg_write(handle, 0x26, 0x0100); /* POR=0x0000 */
		reg_write(handle, 0x28, 0x1000); /* POR=0x0000 */
		reg_write(handle, 0x51, 0x0000); /* POR=0x00c0 */
		reg_write(handle, 0x52, 0x9ae2); /* POR=0xbaf6 */
		reg_write(handle, 0x58, 0x1e1c); /* POR=0x161c */
		reg_write(handle, 0x70, 0x3ce6); /* POR=0x3ee6 */
		reg_write(handle, 0x71, 0x1074); /* POR=0x3074 */
		reg_write(handle, 0x83, 0x0014); /* POR=0x0013 */
		/* ----- generated code end   -----  */
	}

	return error;
}

static enum tfa98xx_error tfa9872_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t MANAOOSC = 0x0140; /* version 17 */
	unsigned short value, xor;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	/* Unlock key 1 and 2 */
	error = reg_write(handle, 0x0F, 0x5A6B);
	error = reg_read(handle, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(handle, 0xA0, xor);
	tfa98xx_key2(handle, 0);

	switch (handles_local[handle].rev) {
	case 0x1a72:
	case 0x2a72:
		/* ----- generated code start ----- */
		/* -----  version 26 ----- */
		reg_write(handle, 0x00, 0x1801); /* POR=0x0001 */
		reg_write(handle, 0x02, 0x2dc8); /* POR=0x2028 */
		reg_write(handle, 0x20, 0x0890); /* POR=0x2890 */
		reg_write(handle, 0x22, 0x043c); /* POR=0x045c */
		reg_write(handle, 0x51, 0x0000); /* POR=0x0080 */
		reg_write(handle, 0x52, 0x1a1c); /* POR=0x7ae8 */
		reg_write(handle, 0x58, 0x161c); /* POR=0x101c */
		reg_write(handle, 0x61, 0x0198); /* POR=0x0000 */
		reg_write(handle, 0x65, 0x0a8b); /* POR=0x0a9a */
		reg_write(handle, 0x70, 0x07f5); /* POR=0x06e6 */
		reg_write(handle, 0x74, 0xcc84); /* POR=0xd823 */
		reg_write(handle, 0x82, 0x01ed); /* POR=0x000d */
		reg_write(handle, 0x83, 0x0014); /* POR=0x0013 */
		reg_write(handle, 0x84, 0x0021); /* POR=0x0020 */
		reg_write(handle, 0x85, 0x0001); /* POR=0x0003 */
		/* ----- generated code end   -----  */
		break;
	case 0x1b72:
	case 0x2b72:
	case 0x3b72:
		/* ----- generated code start ----- */
		/* ----- TFA9872 Probus Registers map N1B2
		 * - Version 21 (10/19/2016) -----
		 */
		reg_write(handle, 0x02, 0x2dc8); /* POR=0x2828 */
		reg_write(handle, 0x20, 0x0890); /* POR=0x2890 */
		reg_write(handle, 0x22, 0x043c); /* POR=0x045c */
		reg_write(handle, 0x23, 0x0001); /* POR=0x0003 */
		reg_write(handle, 0x51, 0x0000); /* POR=0x0080 */
		reg_write(handle, 0x52, 0x5a1c); /* POR=0x7a08 */
		reg_write(handle, 0x61, 0x0198); /* POR=0x0000 */
		reg_write(handle, 0x63, 0x0a9a); /* POR=0x0a93 */
		reg_write(handle, 0x65, 0x0a82); /* POR=0x0a8d */
		reg_write(handle, 0x6f, 0x01e3); /* POR=0x02e4 */
		reg_write(handle, 0x70, 0x06fd); /* POR=0x06e6 */
		reg_write(handle, 0x71, 0x307e); /* POR=0x207e */
		reg_write(handle, 0x74, 0xcc84); /* POR=0xd913 */
		reg_write(handle, 0x75, 0x1132); /* POR=0x118a */
		reg_write(handle, 0x82, 0x01ed); /* POR=0x000d */
		reg_write(handle, 0x83, 0x001a); /* POR=0x0013 */
		/* ----- generated code end   ----- */
		break;
	default:
		pr_info("\nWarning: Optimal settings not found for device with revid = 0x%x\n",
			handles_local[handle].rev);
		break;
	}

	/* Turn off the osc1m to save power: PLMA4928 */
	error = tfa_set_bf(handle, MANAOOSC, 1);

	/* Bypass OVP by setting bit 3 from register 0xB0 (bypass_ovp=1): */
	/* PLMA5258 */
	error = reg_read(handle, 0xB0, &value);
	value |= 1 << 3;
	error = reg_write(handle, 0xB0, value);

	return error;
}

static enum tfa98xx_error tfa9912_specific(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short value, xor;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	if ((handles_local[handle].rev & 0xff) != 0x13) {
		pr_err("This code is not for this device type: %x\n",
		       handles_local[handle].rev);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	/* Unlock keys to write settings */
	error = reg_write(handle, 0x0F, 0x5A6B);
	error = reg_read(handle, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(handle, 0xA0, xor);

	/* The optimal settings */
	if (handles_local[handle].rev == 0x1a13) {
		/* ----- generated code start ----- */
		/* -----  version 1.34 ----- */
		reg_write(handle, 0x00, 0x0255); /* POR=0x0245 */
		reg_write(handle, 0x01, 0x838a); /* POR=0x83ca */
		reg_write(handle, 0x02, 0x2dc8); /* POR=0x2828 */
		reg_write(handle, 0x05, 0x762a); /* POR=0x766a */
		reg_write(handle, 0x22, 0x543c); /* POR=0x545c */
		reg_write(handle, 0x26, 0x0100); /* POR=0x0010 */
		reg_write(handle, 0x51, 0x0000); /* POR=0x0080 */
		reg_write(handle, 0x52, 0x551c); /* POR=0x1afc */
		reg_write(handle, 0x61, 0x000c); /* POR=0x0018 */
		reg_write(handle, 0x63, 0x0a96); /* POR=0x0a9a */
		reg_write(handle, 0x65, 0x0a82); /* POR=0x0a8b */
		reg_write(handle, 0x6c, 0x01d5); /* POR=0x02d5 */
		reg_write(handle, 0x70, 0x26f8); /* POR=0x06e0 */
		reg_write(handle, 0x71, 0x3074); /* POR=0x2074 */
		reg_write(handle, 0x75, 0x4484); /* POR=0x4585 */
		reg_write(handle, 0x76, 0x72ea); /* POR=0x54a2 */
		reg_write(handle, 0x82, 0x024d); /* POR=0x000d */
		reg_write(handle, 0x89, 0x0013); /* POR=0x0014 */
		/* ----- generated code end   ----- */
	}

	return error;
}

/*
 * Tfa9890_DspSystemStable will compensate for the wrong behavior of CLKS
 * to determine if the DSP subsystem is ready for patch and config loading.
 *
 * A MTP calibration register is checked for non-zero.
 *
 * Note: This only works after i2c reset as this will clear the MTP contents.
 * When we are configured then the DSP communication will synchronize access.
 *
 */
static enum tfa98xx_error
tfa9890_dsp_system_stable(tfa98xx_handle_t handle, int *ready)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short status, mtp0;
	int result, tries;

	/* check the contents of the STATUS register */
	result = TFA_READ_REG(handle, AREFS);
	if (result < 0) {
		error = -result;
		goto errorExit;
	}
	status = (unsigned short)result;

	/* if AMPS is set then we were already configured and running
	 *   no need to check further
	 */
	*ready = (TFA_GET_BF_VALUE(handle, AMPS, status) == 1);
	if (*ready)		/* if  ready go back */
		return error;	/* will be TFA98XX_ERROR_OK */

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(handle, AREFS, status) == 0)
		   || (TFA_GET_BF_VALUE(handle, CLKS, status) == 0));
	if (!*ready)		/* if not ready go back */
		return error;	/* will be TFA98XX_ERROR_OK */

	/* check MTPB
	 *   mtpbusy will be active when the subsys copies MTP to I2C
	 *   2 times retry avoids catching this short mtpbusy active period
	 */
	for (tries = 2; tries > 0; tries--) {
		result = TFA_GET_BF(handle, MTPB);
		if (result < 0) {
			error = -result;
			goto errorExit;
		}
		status = (unsigned short)result;

		/* check the contents of the STATUS register */
		*ready = (result == 0);
		if (*ready)	/* if ready go on */
			break;
	}
	if (tries == 0)		/* ready will be 0 if retries exausted */
		return TFA98XX_ERROR_OK;

	/* check the contents of  MTP register for non-zero,
	 *  this indicates that the subsys is ready
	 */

	error = reg_read(handle, 0x84, &mtp0);
	if (error)
		goto errorExit;

	*ready = (mtp0 != 0);	/* The MTP register written? */

	return error;

errorExit:
	*ready = 0;
	return error;
}

/*
 * The CurrentSense4 register is not in the datasheet, define local
 */
#define TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF (1<<2)
#define TFA98XX_CURRENTSENSE4 0x49
/*
 * Disable clock gating
 */
static enum tfa98xx_error tfa9890_clockgating(tfa98xx_handle_t handle, int on)
{
	enum tfa98xx_error error;
	unsigned short value;

	/* TFA9890 temporarily disable clock gating when dsp reset is used */
	error = reg_read(handle, TFA98XX_CURRENTSENSE4, &value);
	if (error)
		return error;

	if (error == TFA98XX_ERROR_OK) {
		if (on)  /* clock gating on - clear the bit */
			value &= ~TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;
		else  /* clock gating off - set the bit */
			value |= TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;

		error = reg_write(handle, TFA98XX_CURRENTSENSE4, value);
	}

	return error;
}

/*
 * Tfa9890_DspReset will deal with clock gating control in order
 * to reset the DSP for warm state restart
 */
static enum tfa98xx_error tfa9890_dsp_reset(tfa98xx_handle_t handle, int state)
{
	enum tfa98xx_error error;

	/* for TFA9890 temporarily disable clock gating
	 *  when dsp reset is used
	 */
	tfa9890_clockgating(handle, 0);

	TFA_SET_BF(handle, RST, (uint16_t)state);

	/* clock gating restore */
	error = tfa9890_clockgating(handle, 1);

	return error;
}

/*
 * the int24 values for the vsfw delay table
 */
static unsigned char vsfwdelay_table[] = {
	0, 0, 2, /*Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0, /*Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0, /*Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 2, /*Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 2, /*Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 2, /*Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 2, /*Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2, /*Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 3  /*Index 8 - Current/Volt Fractional Delay for 48KHz */
};

/*
 * TODO make this tfa98xx
 *  Note that the former products write this table via the patch
 *  so moving this to the tfa98xx API requires also updating all patches
 */
static enum tfa98xx_error
tfa9896_dsp_write_vsfwdelay_table(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error;

	error = tfa_dsp_cmd_id_write(handle, MODULE_FRAMEWORK,
		TFA1_FW_PAR_ID_SET_CURRENT_DELAY, sizeof(vsfwdelay_table),
		vsfwdelay_table);

	return error;
}

/*
 * The int24 values for the fracdelay table
 * For now applicable only for 8 and 48 kHz
 */
static unsigned char cvfracdelay_table[] = {
	0, 0, 51, /*Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0,  0, /*Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0,  0, /*Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 38, /*Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 34, /*Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 33, /*Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 11, /*Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0,  2, /*Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 62  /*Index 8 - Current/Volt Fractional Delay for 48KHz */
};

enum tfa98xx_error tfa9896_dsp_write_cvfracdelay_table(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error;

	error = tfa_dsp_cmd_id_write(handle, MODULE_FRAMEWORK,
		TFA1_FW_PAR_ID_SET_CURFRAC_DELAY, sizeof(cvfracdelay_table),
		cvfracdelay_table);

	return error;
}

static enum tfa98xx_error
tfa9896_tfa_dsp_write_tables(tfa98xx_handle_t dev_idx, int sample_rate)
{
	enum tfa98xx_error error;

	error = tfa9896_dsp_write_vsfwdelay_table(dev_idx);
	if (error == TFA98XX_ERROR_OK)
		error = tfa9896_dsp_write_cvfracdelay_table(dev_idx);

	tfa98xx_dsp_reset(dev_idx, 1);
	tfa98xx_dsp_reset(dev_idx, 0);

	return error;
}

#if defined(TFA9896_SET_TRIP_LEVEL) /* TODO: remove or use me */
static enum tfa98xx_error
tfa9896_tfa_set_boost_trip_level(tfa98xx_handle_t handle, int Re25C)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int trip_value;

	if (Re25C == 0) {
		pr_debug("\nWarning: No calibration values found: Boost trip level not adjusted!\n");
		return error;
	}

	/* Read trip level: The trip level is already set
	 * (if defined in cnt file) so we can just read the bitfield
	 */
	trip_value = tfa_get_bf(handle, TFA9896_BF_DCTRIP);
	pr_debug("\nCurrent calibration value is %d mOhm and the boost_trip_lvl is %d\n",
		Re25C, trip_value);

	if (Re25C > 0 && Re25C < 4000)
		trip_value = 0xa;
	else if (Re25C >= 4000 && Re25C < 6000)
		trip_value = 0x9;
	else if (Re25C >= 6000 && Re25C < 8000)
		trip_value = 0x8;
	else if (Re25C >= 8000)
		trip_value = 0x7;

	/* Set the boost trip level according to the new value */
	error = tfa_set_bf(handle, TFA9896_BF_DCTRIP, (uint16_t)trip_value);
	pr_debug("New boost_trip_lvl is set to %d\n", trip_value);

	return error;
}
#endif

/*
 * the int24 values for the vsfw delay table
 */
static unsigned char tfa9897_vsfwdelay_table[] = {
	0, 0, 2, /*Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0, /*Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0, /*Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 2, /*Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 2, /*Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 2, /*Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 2, /*Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2, /*Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 3  /*Index 8 - Current/Volt Fractional Delay for 48KHz */
};

/*
 * TODO make this tfa98xx
 *  Note that the former products write this table via the patch
 *  so moving this to tfa98xx API requires also updating all patches
 */
static enum tfa98xx_error
tfa9897_dsp_write_vsfwdelay_table(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error;

	error = tfa_dsp_cmd_id_write(handle, MODULE_FRAMEWORK,
		TFA1_FW_PAR_ID_SET_CURRENT_DELAY,
		sizeof(tfa9897_vsfwdelay_table),
		tfa9897_vsfwdelay_table);

	return error;
}

/*
 * The int24 values for the fracdelay table
 * For now applicable only for 8 and 48 kHz
 */
static unsigned char tfa9897_cvfracdelay_table[] = {
	0, 0, 51, /*Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0,  0, /*Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0,  0, /*Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 38, /*Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 34, /*Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 33, /*Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 11, /*Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0,  2, /*Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 62  /*Index 8 - Current/Volt Fractional Delay for 48KHz */
};

enum tfa98xx_error
tfa9897_dsp_write_cvfracdelay_table(tfa98xx_handle_t handle)
{
	enum tfa98xx_error error;

	error = tfa_dsp_cmd_id_write(handle, MODULE_FRAMEWORK,
		TFA1_FW_PAR_ID_SET_CURFRAC_DELAY,
		sizeof(tfa9897_cvfracdelay_table),
		tfa9897_cvfracdelay_table);

	return error;
}

static enum tfa98xx_error
tfa9897_tfa_dsp_write_tables(tfa98xx_handle_t dev_idx, int sample_rate)
{
	enum tfa98xx_error error;

	error = tfa9897_dsp_write_vsfwdelay_table(dev_idx);
	if (error == TFA98XX_ERROR_OK)
		error = tfa9897_dsp_write_cvfracdelay_table(dev_idx);

	tfa98xx_dsp_reset(dev_idx, 1);
	tfa98xx_dsp_reset(dev_idx, 0);

	return error;
}

static enum tfa98xx_error
tfa9888_tfa_dsp_write_tables(tfa98xx_handle_t handle, int sample_rate)
{
	unsigned char buffer[15] = {0};
	int size = 15 * sizeof(char);

	/* Write the fractional delay in hardware register 'cs_frac_delay' */
	switch (sample_rate) {
	case 0:	/* 8kHz */
		TFA_SET_BF(handle, FRACTDEL, 40);
		break;
	case 1:	/* 11.025KHz */
		TFA_SET_BF(handle, FRACTDEL, 38);
		break;
	case 2:	/* 12kHz */
		TFA_SET_BF(handle, FRACTDEL, 37);
		break;
	case 3:	/* 16kHz */
		TFA_SET_BF(handle, FRACTDEL, 59);
				break;
	case 4:	/* 22.05KHz */
		TFA_SET_BF(handle, FRACTDEL, 56);
		break;
	case 5:	/* 24kHz */
		TFA_SET_BF(handle, FRACTDEL, 56);
		break;
	case 6:	/* 32kHz */
		TFA_SET_BF(handle, FRACTDEL, 52);
		break;
	case 7:	/* 44.1kHz */
		TFA_SET_BF(handle, FRACTDEL, 48);
		break;
	case 8:
	default:/* 48kHz */
		TFA_SET_BF(handle, FRACTDEL, 46);
		break;
	}

	/* First copy the msg_id to the buffer */
	buffer[0] = (uint8_t) 0;
	buffer[1] = (uint8_t) MODULE_FRAMEWORK + 128;
	buffer[2] = (uint8_t) FW_PAR_ID_SET_SENSES_DELAY;

	/* Required for all FS exept 8kHz (8kHz is all zero) */
	if (sample_rate != 0) {
		buffer[5] = 1;	/* Vdelay_P */
		buffer[8] = 0;	/* Idelay_P */
		buffer[11] = 1; /* Vdelay_S */
		buffer[14] = 0; /* Idelay_S */
	}

	/* send SetSensesDelay msg */
	return dsp_msg(handle, size, (char *)buffer);
}

static enum tfa98xx_error
tfa9912_tfa_dsp_write_tables(tfa98xx_handle_t handle, int sample_rate)
{
	unsigned char buffer[15] = {0};
	int size = 15 * sizeof(char);

	/* Write the fractional delay in hardware register 'cs_frac_delay' */
	switch (sample_rate) {
	case 0:	/* 8kHz */
		TFA_SET_BF(handle, FRACTDEL, 40);
		break;
	case 1:	/* 11.025KHz */
		TFA_SET_BF(handle, FRACTDEL, 38);
		break;
	case 2:	/* 12kHz */
		TFA_SET_BF(handle, FRACTDEL, 37);
		break;
	case 3:	/* 16kHz */
		TFA_SET_BF(handle, FRACTDEL, 59);
		break;
	case 4:	/* 22.05KHz */
		TFA_SET_BF(handle, FRACTDEL, 56);
		break;
	case 5:	/* 24kHz */
		TFA_SET_BF(handle, FRACTDEL, 56);
		break;
	case 6:	/* 32kHz */
		TFA_SET_BF(handle, FRACTDEL, 52);
		break;
	case 7:	/* 44.1kHz */
		TFA_SET_BF(handle, FRACTDEL, 48);
		break;
	case 8:
	default:/* 48kHz */
		TFA_SET_BF(handle, FRACTDEL, 46);
		break;
	}

	/* First copy the msg_id to the buffer */
	buffer[0] = (uint8_t) 0;
	buffer[1] = (uint8_t) MODULE_FRAMEWORK + 128;
	buffer[2] = (uint8_t) FW_PAR_ID_SET_SENSES_DELAY;

	/* Required for all FS exept 8kHz (8kHz is all zero) */
	if (sample_rate != 0) {
		buffer[5] = 1;	/* Vdelay_P */
		buffer[8] = 0;	/* Idelay_P */
		buffer[11] = 1; /* Vdelay_S */
		buffer[14] = 0; /* Idelay_S */
	}

	/* send SetSensesDelay msg */
	return dsp_msg(handle, size, (char *)buffer);
}

/*
 * register device specifics functions
 */
void tfa9895_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9895_specific;
}

/*
 * register device specifics functions
 */
void tfa9890_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9890_specific;
	ops->tfa_dsp_reset = tfa9890_dsp_reset;
	ops->tfa_dsp_system_stable = tfa9890_dsp_system_stable;
}

/*
 * register device specifics functions
 */
void tfa9891_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9891_specific;
}

/*
 * register device specifics functions
 */
void tfa9896_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9896_specific;
	ops->tfa_dsp_write_tables = tfa9896_tfa_dsp_write_tables;
}

/*
 * register device specifics functions
 */
void tfa9897_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9897_specific;
	ops->tfa_dsp_write_tables = tfa9897_tfa_dsp_write_tables;
}

/*
 * register device specifics functions
 */
void tfa9888_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9888_specific;
	ops->tfa_dsp_write_tables = tfa9888_tfa_dsp_write_tables;
}

/*
 * register device specifics functions
 */
void tfa9872_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9872_specific;
}

/*
 * register device specifics functions
 */
void tfa9912_ops(struct tfa_device_ops *ops)
{
	ops->tfa_init = tfa9912_specific;
	ops->tfa_dsp_write_tables = tfa9912_tfa_dsp_write_tables;
}
