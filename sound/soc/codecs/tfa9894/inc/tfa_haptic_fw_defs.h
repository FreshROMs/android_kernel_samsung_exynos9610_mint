/*
 * tfa_haptic_fw_defs.h  - tfa9914 haptic 1.5 defines
 *
 * Copyright (C) 2018 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef TFA_HAPTIC_FW_DEFS_H_
#define TFA_HAPTIC_FW_DEFS_H_

/* base version */
#define FW_VERSION		0x0b0000  /* patch version in hex */
#define FW_XMEM_VERSION	0x11ff    /* TFA9894 xmem version offset */

/* firmware constants */
#define FW_XMEM_NR_OBJECTS1		6		  /* nr of objects in table1 */
#define FW_XMEM_NR_OBJECTS2		5		  /* nr of objects in table2 */
#define FW_XMEM_OBJECTSIZE		8
#define FW_XMEM_OBJECTSTATESIZE	8

#define FW_CMDOBJSEL_TABLE2_OFFFSET 64

#define FW_XMEM_CMDOBJSEL0			4102  /* p_obj_state */
#define FW_XMEM_CMDOBJSEL1			(FW_XMEM_CMDOBJSEL0 + FW_XMEM_OBJECTSTATESIZE)
#define FW_XMEM_SAMPCNT0			(FW_XMEM_CMDOBJSEL0 + 2) /* ->time_cnt */
#define FW_XMEM_SAMPCNT1			(FW_XMEM_CMDOBJSEL1 + 2)
#define FW_XMEM_F0					4052  /* f_res_out */
#define FW_XMEM_R0					4053  /* r_f0_out */
#define FW_XMEM_GENOBJECTS1			4054  /* object_array */
#define FW_XMEM_GENOBJECTS2			3152  /* object_array2 */
#define FW_XMEM_F0_R0_RANGES		4134  /* Zfres_max */
#define FW_XMEM_DISF0TRC			4359  /* dis_f0_trc */
#define FW_XMEM_DELAY_ATTACK_SMP	4361  /* duck_lra_par->delay_attack_smp */
#define FW_XMEM_RECALCSEL			4363  /* recalc_selector */

/* note: obj offsets is a table index, the interface index starts at 1 */
#define FW_HB_CAL_OBJ	(FW_XMEM_NR_OBJECTS1 - 2) /* calibration object */
#define FW_HB_STOP_OBJ	(FW_XMEM_NR_OBJECTS1 - 1) /* silence object */
#define FW_HB_SILENCE_OBJ FW_HB_STOP_OBJ	 /* same as stop */
#define FW_HB_RECALC_OBJ 3  			 /* default recalc object */
#define FW_HB_MAX_OBJ	(FW_XMEM_NR_OBJECTS1 + FW_XMEM_NR_OBJECTS2)
#define FW_HB_SEQ_OBJ 	FW_HB_MAX_OBJ /* sequencer virtual objects base */

#define FW_XMEM_R0_SHIFT 11 /* Q13.11 */
#define FW_XMEM_F0_SHIFT 11 /* Q13.11 */

enum tfa_haptic_object_type {
	OBJECT_WAVE     = 0,
	OBJECT_TONE     = 1,
	OBJECT_SILENCE	= 2
};

/* Tone Generator object definition */
struct haptic_tone_object {
	 int32_t type;
	 int32_t freq;
	 int32_t level;
	 int32_t duration_cnt_max;
	 int32_t boost_brake_on;
	 int32_t tracker_on;
	 int32_t boost_length;
	 int32_t reserved;
};

/* Wave table object definition */
struct haptic_wave_object {
	 int32_t type;
	 int32_t offset;
	 int32_t level;
	 int32_t duration_cnt_max;
	 int32_t up_samp_sel;
	 int32_t reserved[3];
};

#endif /* TFA_HAPTIC_FW_DEFS_H_ */
