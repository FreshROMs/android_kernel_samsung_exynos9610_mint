/*
 * sm5713_fuelgauge.h
 * Samsung sm5713 Fuel Gauge Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
 * This software is sm5713 under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __sm5713_FUELGAUGE_H
#define __sm5713_FUELGAUGE_H __FILE__

#include <linux/power_supply.h>
#include "../sec_charging_common.h"

#include <linux/mfd/core.h>
#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>
#include <linux/regulator/machine.h>

#if defined(CONFIG_BATTERY_AGE_FORECAST)
#define ENABLE_BATT_LONG_LIFE 1
#endif

#define ENABLE_INTER_Q4_SENSE 1
/* #define ENABLE_sm5713_MQ_FUNCTION 1 */
#define ENABLE_FULL_OFFSET 1


/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define PRINT_COUNT	10

#define ALERT_EN 0x04
#define CAPACITY_SCALE_DEFAULT_CURRENT 1000
#define CAPACITY_SCALE_HV_CURRENT 600

#define SW_RESET_CODE			0x00A6
#define SW_RESET_OTP_CODE		0x01A6
#define RS_MAN_CNTL				0x0800

#define FG_INIT_MARK			0xA000
#define FG_PARAM_UNLOCK_CODE	0x3700
#define FG_PARAM_LOCK_CODE	    0x0000
#define FG_TABLE_LEN			0x17 /* real table length -1 */
#define FG_ADD_TABLE_LEN		0xF /* real table length -1 */
#define FG_INIT_B_LEN		    0x7 /* real table length -1 */


/* control register value */
#define ENABLE_MIX_MODE         0x8200
#define ENABLE_TEMP_MEASURE     0x4000
#define ENABLE_TOPOFF_SOC       0x2000
#define ENABLE_RS_MAN_MODE      0x0800
#define ENABLE_MANUAL_OCV       0x0400
#define ENABLE_MODE_nENQ4       0x0200

#define ENABLE_SOC_ALARM        0x0008
#define ENABLE_T_H_ALARM        0x0004
#define ENABLE_T_L_ALARM        0x0002
#define ENABLE_V_ALARM          0x0001

#define START_MQ                0x8000
#define DUMP_MQ                 0x4000

#define CNTL_REG_DEFAULT_VALUE  0x2008
#define INIT_CHECK_MASK         0x0010
#define DISABLE_RE_INIT         0x0010
#define JIG_CONNECTED	0x0001
#define I2C_ERROR_REMAIN		0x0004
#define I2C_ERROR_CHECK	0x0008
#define DATA_VERSION	0x00F0

enum max77854_vempty_mode {
	VEMPTY_MODE_HW = 0,
	VEMPTY_MODE_SW,
	VEMPTY_MODE_SW_VALERT,
	VEMPTY_MODE_SW_RECOVERY,
};

struct sm5713_fg_info {
	/* test print count */
	int pr_cnt;
	/* full charge comp */
	struct delayed_work	full_comp_work;
	u32 previous_fullcap;
	u32 previous_vffullcap;

    /* Device_id */
    int device_id;
    /* State Of Connect */
    int online;
    /* battery SOC (capacity) */
    int batt_soc;
    /* battery voltage */
    int batt_voltage;
    /* battery AvgVoltage */
    int batt_avgvoltage;
    /* battery OCV */
    int batt_ocv;
    /* Current */
    int batt_current;
    /* battery Avg Current */
    int batt_avgcurrent;
    /* battery SOC cycle */
    int batt_soc_cycle;
    /* battery measure point*/
    int is_read_vpack;

    struct battery_data_t *comp_pdata;

    struct mutex param_lock;
    /* copy from platform data /
     * DTS or update by shell script */

    struct mutex io_lock;
    struct device *dev;
    int32_t temperature;; /* 0.1 deg C*/
    int32_t temp_fg;; /* 0.1 deg C*/
    /* register programming */
    int reg_addr;
    u8 reg_data[2];

    int battery_typ;        /*SDI_BATTERY_TYPE or ATL_BATTERY_TYPE*/
    int batt_id_adc_check;
    int battery_table[3][24];
#ifdef ENABLE_BATT_LONG_LIFE
    int v_max_table[5];
    int q_max_table[5];
    int v_max_now;
    int q_max_now;
#endif
#ifdef ENABLE_SM5713_MQ_FUNCTION
    int full_mq_dump;
#endif
    int rce_value[3];
    int dtcd_value;
    int rs_value[5]; /*rs p_mix_factor n_mix_factor max min*/
    int vit_period;
    int mix_value[2]; /*mix_rate init_blank*/
    int batt_v_max;
    int misc;
    int min_cap;
    int cap;
    int maxcap;
    int arsm[4];
    int soh;

    int enable_topoff_soc;
    int topoff_soc;
    int top_off;

    int cycle_high_limit;
    int cycle_low_limit;
    int cycle_limit_cntl;

    int enable_v_offset_cancel_p;
    int enable_v_offset_cancel_n;
    int v_offset_cancel_level;
    int v_offset_cancel_mohm;

    int volt_cal[2];
    int en_auto_i_offset;
    int ecv_i_off;
    int csp_i_off;
    int csn_i_off;
    int dp_ecv_i_off;
    int dp_csp_i_off;
    int dp_csn_i_off;
    int ecv_i_slo;
    int csp_i_slo;
    int csn_i_slo;
    int dp_ecv_i_slo;
    int dp_csp_i_slo;
    int dp_csn_i_slo;

    int cntl_value;
#ifdef ENABLE_FULL_OFFSET
    int full_offset_enable;
    int full_offset_margin;
    int full_extra_offset;
    int aux_stat_base;
    int aux_stat_check;
#endif

    int temp_std;
    int en_fg_temp_volcal;
    int fg_temp_volcal_denom;
    int fg_temp_volcal_fact;
    int en_high_fg_temp_offset;
    int high_fg_temp_offset_denom;
    int high_fg_temp_offset_fact;
    int en_low_fg_temp_offset;
    int low_fg_temp_offset_denom;
    int low_fg_temp_offset_fact;
    int en_high_fg_temp_cal;
    int high_fg_temp_p_cal_denom;
    int high_fg_temp_p_cal_fact;
    int high_fg_temp_n_cal_denom;
    int high_fg_temp_n_cal_fact;
    int en_low_fg_temp_cal;
    int low_fg_temp_p_cal_denom;
    int low_fg_temp_p_cal_fact;
    int low_fg_temp_n_cal_denom;
    int low_fg_temp_n_cal_fact;
    int en_high_temp_cal;
    int high_temp_p_cal_denom;
    int high_temp_p_cal_fact;
    int high_temp_n_cal_denom;
    int high_temp_n_cal_fact;
    int en_low_temp_cal;
    int low_temp_p_cal_denom;
    int low_temp_p_cal_fact;
    int low_temp_n_cal_denom;
    int low_temp_n_cal_fact;
    int coeff;

    int data_ver;
    int age_cntl;
    int tcal_ioff[2];

    uint32_t soc_alert_flag : 1;  /* 0 : nu-occur, 1: occur */
    uint32_t volt_alert_flag : 1; /* 0 : nu-occur, 1: occur */
    uint32_t flag_full_charge : 1; /* 0 : no , 1 : yes*/
    uint32_t flag_chg_status : 1; /* 0 : discharging, 1: charging*/
    uint32_t flag_charge_health : 1; /* 0 : no , 1 : good*/

    int32_t irq_ctrl;
    int value_v_alarm;
    int value_v_alarm_hys;

    uint32_t is_FG_initialised;
    int iocv_error_count;

    int n_tem_poff;
    int n_tem_poff_offset;
    int l_tem_poff;
    int l_tem_poff_offset;

    /* previous battery voltage current*/
    int p_batt_voltage;
    int p_batt_current;

	unsigned long fullcap_check_interval;
	int full_check_flag;
	bool is_first_check;
};

#define CURRENT_RANGE_MAX_NUM	5

struct battery_data_t {
	const int battery_type; /* 4200 or 4350 or 4400*/
	const int battery_table[3][24];
	const int rce_value[3];
	const int dtcd_value;
	const int rs_value[4];
	const int vit_period;
	const int mix_value[2];
	const int topoff_soc[2];
	const int volt_cal;
	const int curr_cal;
	const int temp_std;
	const int temp_offset;
	const int temp_offset_cal;
    int Capacity;
    u32 V_empty;
    u32 V_empty_origin;
	u32 sw_v_empty_vol;
	u32 sw_v_empty_vol_cisd;
	u32 sw_v_empty_recover_vol;
	u32 vbat_ovp;
};

/* FullCap learning setting */
#define VFFULLCAP_CHECK_INTERVAL	300 /* sec */
/* soc should be 0.1% unit */
#define VFSOC_FOR_FULLCAP_LEARNING	950
#define LOW_CURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_CURRENT_FOR_FULLCAP_LEARNING	120
#define LOW_AVGCURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_AVGCURRENT_FOR_FULLCAP_LEARNING	100

/* power off margin */
/* soc should be 0.1% unit */
#define POWER_OFF_SOC_HIGH_MARGIN	20
#define POWER_OFF_VOLTAGE_HIGH_MARGIN	3500
#define POWER_OFF_VOLTAGE_LOW_MARGIN	3400

struct cv_slope{
	int fg_current;
	int soc;
	int time;
};

struct sm5713_fuelgauge_data {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct i2c_client       *pmic;
	struct mutex            fuelgauge_mutex;
	struct sm5713_platform_data *sm5713_pdata;
	sec_fuelgauge_platform_data_t *pdata;
	struct power_supply		*psy_fg;
	struct delayed_work isr_work;

	u8 pmic_rev;
	u8 vender_id;

	int cable_type;
	bool is_charging;
	bool ta_exist;

	/* HW-dedicated fuel guage info structure
	 * used in individual fuel gauge file only
	 * (ex. dummy_fuelgauge.c)
	 */
	struct sm5713_fg_info	info;
	struct battery_data_t        *battery_data;

	bool is_fuel_alerted;
	struct wake_lock fuel_alert_wake_lock;

	unsigned int capacity_old;	/* only for atomic calculation */
	unsigned int capacity_max;	/* only for dynamic calculation */
	unsigned int standard_capacity;

	bool initial_update_of_soc;
	struct mutex fg_lock;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	unsigned int pre_soc;
	int fg_irq;

	int raw_capacity;
	int current_now;
	int current_avg;
	unsigned int ttf_capacity;
	struct cv_slope *cv_data;
	int cv_data_lenth;

	bool using_temp_compensation;
	bool using_hw_vempty;
	unsigned int vempty_mode;

	unsigned int low_temp_limit;

	bool auto_discharge_en;
	u32 discharge_temp_threshold;
	u32 discharge_volt_threshold;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	unsigned int chg_full_soc; /* BATTERY_AGE_FORECAST */
#endif

	u32 fg_resistor;
	bool isjigmoderealvbat;
};

#endif /* __SM5713_FUELGAUGE_H */
