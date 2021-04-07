/*
 *  sm5713_fuelgauge.c
 *  Samsung sm5713 Fuel Gauge Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define BATTERY_LOG_MESSAGE */
#if defined(CONFIG_BATTERY_SAMSUNG_V2)
#include "include/sec_battery.h"
#else
#include <linux/battery/sec_battery.h>
#endif
#include <linux/mfd/sm5713-private.h>
#include "include/fuelgauge/sm5713_fuelgauge.h"
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>


static enum power_supply_property sm5713_fuelgauge_props[] = {
};

static char *sm5713_fg_supplied_to[] = {
	"sm5713-fuelgauge",
};

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MAXVAL(a, b) ((a > b) ? a : b)

#define LIMIT_N_CURR_MIXFACTOR -2000
#define TABLE_READ_COUNT 2
#define FG_ABNORMAL_RESET -1
#define IGNORE_N_I_OFFSET 1

#define SM5713_FG_FULL_DEBUG 1
#define I2C_ERROR_COUNT_MAX 5

#ifdef ENABLE_SM5713_MQ_FUNCTION
static int sm5713_get_full_chg_mq (struct sm5713_fuelgauge_data *fuelgauge);
static void sm5713_set_full_chg_mq (struct sm5713_fuelgauge_data *fuelgauge, int mq);
static void sm5713_meas_mq_suspend (struct sm5713_fuelgauge_data *fuelgauge);
static void sm5713_meas_mq_resume (struct sm5713_fuelgauge_data *fuelgauge);
static void sm5713_meas_mq_start (struct sm5713_fuelgauge_data *fuelgauge);
static int sm5713_meas_eq_dump (struct sm5713_fuelgauge_data *fuelgauge);
static int sm5713_meas_mq_dump (struct sm5713_fuelgauge_data *fuelgauge);
static void sm5713_meas_mq_off (struct sm5713_fuelgauge_data *fuelgauge);
#endif

void sm5713_adabt_full_offset(struct sm5713_fuelgauge_data *fuelgauge);
static bool sm5713_fg_init(struct sm5713_fuelgauge_data *fuelgauge, bool is_surge);



static int sm5713_device_id = -1;

/* static unsigned int lpcharge = 0; */

enum sm5713_battery_table_type {
	DISCHARGE_TABLE = 0,
	SOC_TABLE,
	TABLE_MAX,
};

bool sm5713_fg_fuelalert_init(struct sm5713_fuelgauge_data *fuelgauge,
			int soc);

#if !defined(CONFIG_SEC_FACTORY)
static void sm5713_fg_periodic_read(struct sm5713_fuelgauge_data *fuelgauge)
{
	static struct timespec old_ts = {0, };
	struct timespec c_ts = {0, };
	u8 reg;
	int i;
	int data[0x10];
	char *str = NULL;

	c_ts = ktime_to_timespec(ktime_get_boottime());
	if ((unsigned long)(c_ts.tv_sec - old_ts.tv_sec) <= 180 && old_ts.tv_sec != 0) { /*3 min*/
		pr_info("%s: skip old(%ld) current(%ld)\n", __func__, old_ts.tv_sec, c_ts.tv_sec);
		return;
	}
	old_ts = c_ts;

	str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
	if (!str)
		return;

	for (i = 0; i <= 0xB; i++) {
		if (i == 7)
			i = 8;
		for (reg = 0; reg < 0x10; reg++) {
			if ((i == 0) && ((reg == 0) || (reg == 2)))	{
				data[reg] = 0;
				reg++;
			}
			data[reg] = sm5713_read_word(fuelgauge->i2c, reg + (i * 0x10));
			if (data[reg] < 0) {
				kfree(str);
				return;
			}
		}
		sprintf(str+strlen(str),
			"%02x:%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",
			i, data[0x00], data[0x01], data[0x02], data[0x03],
			data[0x04], data[0x05], data[0x06], data[0x07]);
		sprintf(str+strlen(str),
			"%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",
			data[0x08], data[0x09], data[0x0a], data[0x0b],
			data[0x0c], data[0x0d], data[0x0e], data[0x0f]);
		if (!fuelgauge->initial_update_of_soc) {
			usleep_range(1000, 2000);
		}
	}

	pr_info("[FG_ALL] %s", str);
	pr_info("\n");

	kfree(str);
}
#endif

static bool sm5713_fg_check_reg_init_need(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_OP_STATUS);

	if ((ret & INIT_CHECK_MASK) == DISABLE_RE_INIT) {
		pr_info("%s: SM5713_REG_FG_OP_STATUS : 0x%x , return FALSE NO init need\n", __func__, ret);
		return 0;
	} else {
		pr_info("%s: SM5713_REG_FG_OP_STATUS : 0x%x , return TRUE init need!!!!\n", __func__, ret);
		return 1;
	}
}

void sm5713_cal_avg_vbat(struct sm5713_fuelgauge_data *fuelgauge)
{
	if (fuelgauge->info.batt_avgvoltage == 0)
		fuelgauge->info.batt_avgvoltage = fuelgauge->info.batt_voltage;

	else if (fuelgauge->info.batt_voltage == 0 && fuelgauge->info.p_batt_voltage == 0)
		fuelgauge->info.batt_avgvoltage = 3400;

	else if (fuelgauge->info.batt_voltage == 0)
		fuelgauge->info.batt_avgvoltage =
				((fuelgauge->info.batt_avgvoltage) + (fuelgauge->info.p_batt_voltage))/2;

	else if (fuelgauge->info.p_batt_voltage == 0)
		fuelgauge->info.batt_avgvoltage =
				((fuelgauge->info.batt_avgvoltage) + (fuelgauge->info.batt_voltage))/2;

	else
		fuelgauge->info.batt_avgvoltage =
				((fuelgauge->info.batt_avgvoltage*2) +
				 (fuelgauge->info.p_batt_voltage+fuelgauge->info.batt_voltage))/4;

#ifdef SM5713_FG_FULL_DEBUG
	pr_info("%s: batt_avgvoltage = %d\n", __func__, fuelgauge->info.batt_avgvoltage);
#endif

	return;
}

void sm5713_voffset_cancel(struct sm5713_fuelgauge_data *fuelgauge)
{
	int volt_slope, mohm_volt_cal;
	int fg_temp_gap = 0, volt_cal = 0, fg_delta_volcal = 0, pn_volt_slope = 0, volt_offset = 0;

	if ((sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_STAT) & 0x02) ||
		factory_mode) {
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_VOLT_CAL, fuelgauge->info.volt_cal[1]);
	} else {
		/*set vbat offset cancel start */
		volt_slope = fuelgauge->info.volt_cal[0] & 0xFF00;
		mohm_volt_cal = fuelgauge->info.volt_cal[0] & 0x00FF;
		if (fuelgauge->info.enable_v_offset_cancel_p) {
			if (fuelgauge->is_charging && (fuelgauge->info.batt_current > fuelgauge->info.v_offset_cancel_level)) {
				if (mohm_volt_cal & 0x0080) {
					mohm_volt_cal = -(mohm_volt_cal & 0x007F);
				}
				mohm_volt_cal = mohm_volt_cal - (fuelgauge->info.batt_current/(fuelgauge->info.v_offset_cancel_mohm * 13)); /* ((curr*0.001)*0.006)*2048 -> 6mohm */
				if (mohm_volt_cal < 0) {
					mohm_volt_cal = -mohm_volt_cal;
					mohm_volt_cal = mohm_volt_cal|0x0080;
				}
			}
		}
		if (fuelgauge->info.enable_v_offset_cancel_n) {
			if (!(fuelgauge->is_charging) && (fuelgauge->info.batt_current < -(fuelgauge->info.v_offset_cancel_level))) {
				if (fuelgauge->info.volt_cal[0] & 0x0080) {
					mohm_volt_cal = -(mohm_volt_cal & 0x007F);
				}
				mohm_volt_cal = mohm_volt_cal - (fuelgauge->info.batt_current/(fuelgauge->info.v_offset_cancel_mohm * 13)); /* ((curr*0.001)*0.006)*2048 -> 6mohm */
				if (mohm_volt_cal < 0) {
					mohm_volt_cal = -mohm_volt_cal;
					mohm_volt_cal = mohm_volt_cal|0x0080;
				}
			}
		}
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_VOLT_CAL, ((mohm_volt_cal & 0x00FF) | (volt_slope & 0xFF00)));
		pr_info("%s: <%d %d %d %d> volt_cal = 0x%x, volt_slope = 0x%x, mohm_volt_cal = 0x%x\n",
			__func__, fuelgauge->info.enable_v_offset_cancel_p, fuelgauge->info.enable_v_offset_cancel_n
			, fuelgauge->info.v_offset_cancel_level, fuelgauge->info.v_offset_cancel_mohm
			, fuelgauge->info.volt_cal[0], volt_slope, mohm_volt_cal);
		/* set vbat offset cancel end */

		fg_temp_gap = (fuelgauge->info.temp_fg/10) - fuelgauge->info.temp_std;

		volt_cal = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_VOLT_CAL);
		volt_offset = volt_cal & 0x00FF;
		pn_volt_slope = fuelgauge->info.volt_cal[0] & 0xFF00;

		if (fuelgauge->info.en_fg_temp_volcal) {
			fg_delta_volcal = (fg_temp_gap / fuelgauge->info.fg_temp_volcal_denom)*fuelgauge->info.fg_temp_volcal_fact;
			pn_volt_slope = pn_volt_slope + (fg_delta_volcal<<8);
			volt_cal = pn_volt_slope | volt_offset;
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_VOLT_CAL, volt_cal);
		}
	}

	return;
}

static unsigned int sm5713_get_vbat(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret = 0;
	unsigned int vbat = 0; /* = 3500; 3500 means 3500mV*/

	sm5713_voffset_cancel(fuelgauge);

	if (fuelgauge->info.is_read_vpack)
		ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_VOLTAGE_CHGOUT);
	else
		ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_VOLTAGE_VBAT);

	if ((sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_STAT) & 0x0002) ||
		factory_mode) {
		if (fuelgauge->isjigmoderealvbat)
			pr_info("%s : nENQ4 high JIG_ON, BUT need real VBAT \n", __func__, ret);
		else {
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_BAT_PTT1, 0x0109);
			ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_VOLTAGE_VSYS);
			pr_info("%s : nENQ4 high JIG_ON, vsys register read result 0x%x\n", __func__, ret);
		}
	}
	else {
		if(sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_BAT_PTT1) != 0x0100) {
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_BAT_PTT1, 0x0100);
		}
	}

	if (ret < 0) {
		pr_err("%s: read vbat reg fail", __func__);
		vbat = 4000;
	} else {
		vbat = ((ret&0x3fff)*1000)>>11;
	}
	fuelgauge->info.batt_voltage = vbat;

	sm5713_cal_avg_vbat(fuelgauge);

	if ((fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT) &&
		(vbat >= fuelgauge->battery_data->sw_v_empty_recover_vol)) {
		fuelgauge->vempty_mode = VEMPTY_MODE_SW_RECOVERY;

		sm5713_fg_fuelalert_init(fuelgauge,
			fuelgauge->pdata->fuel_alert_soc);
		pr_info("%s : Recoverd from SW V EMPTY Activation\n", __func__);

	}

	return vbat;
}

static unsigned int sm5713_get_ocv(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;
	unsigned int ocv; /* = 3500; *//*3500 means 3500mV*/

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_OCV);
	if (ret < 0) {
		pr_err("%s: read ocv reg fail\n", __func__);
		ocv = 4000;
	} else {
		ocv = ((ret&0x7fff)*1000)>>11;
	}

	fuelgauge->info.batt_ocv = ocv;

	return ocv;
}

void sm5713_cal_avg_current(struct sm5713_fuelgauge_data *fuelgauge)
{
	if (fuelgauge->info.batt_avgcurrent == 0)
		fuelgauge->info.batt_avgcurrent = fuelgauge->info.batt_current;

	else if (fuelgauge->info.batt_avgcurrent == 0 && fuelgauge->info.p_batt_current == 0)
		fuelgauge->info.batt_avgcurrent = fuelgauge->info.batt_current;

	else if (fuelgauge->info.batt_current == 0)
		fuelgauge->info.batt_avgcurrent =
				((fuelgauge->info.batt_avgcurrent) + (fuelgauge->info.p_batt_current))/2;

	else if (fuelgauge->info.p_batt_current == 0)
		fuelgauge->info.batt_avgcurrent =
				((fuelgauge->info.batt_avgcurrent) + (fuelgauge->info.batt_current))/2;

	else
		fuelgauge->info.batt_avgcurrent =
				((fuelgauge->info.batt_avgcurrent*2) +
				(fuelgauge->info.p_batt_current+fuelgauge->info.batt_current))/4;

	return;
}

static int sm5713_get_curr(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;
	int curr = 0; /* = 1000; 1000 means 1000mA*/

#ifdef ENABLE_FULL_OFFSET
	sm5713_adabt_full_offset(fuelgauge);
#endif

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_CURRENT);
	if (ret < 0) {
		pr_err("%s: read curr reg fail", __func__);
		curr = 0;
	} else {
		curr = ((ret&0x3fff)*1000)>>11;
		if (ret&0x8000) {
			curr *= -1;
		}
	}

	fuelgauge->info.batt_current = curr;

	sm5713_cal_avg_current(fuelgauge);

	return curr;
}

static int sm5713_get_temperature(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;
	int temp; /* = 250; 250 means 25.0oC*/

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_TEMPERATURE);
	if (ret < 0) {
		pr_err("%s: read temp reg fail", __func__);
		temp = 0;
	} else {
		temp = ((ret&0x7FF0)*10)>>8;
		if (ret&0x8000) {
			temp *= -1;
		}
	}
	fuelgauge->info.temp_fg = temp;

	return temp;
}

static int sm5713_get_cycle(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;
	int cycle;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_SOC_CYCLE);
	if (ret < 0) {
		pr_err("%s: read cycle reg fail", __func__);
		cycle = 0;
	} else {
		cycle = ret&0x03FF;
	}
	fuelgauge->info.batt_soc_cycle = cycle;

	return cycle;
}

static int sm5713_get_asoc(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ctrl, info, soh, pre_soh, h_flag, c_flag, delta_t, temp;

	ctrl = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AGING_CTRL);
	info = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AGING_INFO);
	pre_soh = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_2);
	c_flag = (sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_SOC_CYCLE) >> 4 ) % 2;

	h_flag = (pre_soh & 0x80)>>7;
	pre_soh = pre_soh & 0x7F;
	pr_info("%s : asoc = %d, ctrl = 0x%x, info = 0x%x, pre = 0x%x, t = %d \n", __func__, fuelgauge->info.soh, ctrl, info, pre_soh, fuelgauge->info.temperature);
	ctrl = ctrl & 0x0200;
	if (ctrl != 0x0200) {
		soh = pre_soh;
        sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AGING_CTRL, fuelgauge->info.age_cntl);
	} else {
		info = info & 0x007F;
		soh = ((info * 100) / 128) + 2;
	}

	if ((soh > 100) || (soh <= 10))
		soh = 100;

	if ((pre_soh > 100) || (pre_soh <= 10))
		pre_soh = 100;

	delta_t = fuelgauge->info.temperature/10 - fuelgauge->info.temp_std;
	if (delta_t >= 0) {
		if (soh < pre_soh) {
			if (c_flag != h_flag) {
				pre_soh = pre_soh-1;
				temp = (c_flag<<7) | pre_soh;
				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_2, temp);
				pr_info("%s : pre_soh update to %d, write 0x%x\n", __func__, pre_soh, temp);
			}
		}
	}
	soh = pre_soh;

	fuelgauge->info.soh = soh;

	return fuelgauge->info.soh;
}


static void sm5713_vbatocv_check(struct sm5713_fuelgauge_data *fuelgauge)
{
	if ((abs(fuelgauge->info.batt_current) < 50) ||
	   ((fuelgauge->is_charging) && (fuelgauge->info.batt_current < (fuelgauge->info.top_off)) &&
	   (fuelgauge->info.batt_current > (fuelgauge->info.top_off/3)) && (fuelgauge->info.batt_soc >= 900))) {
		if (abs(fuelgauge->info.batt_ocv-fuelgauge->info.batt_voltage) > 30) { /* 30mV over */
			fuelgauge->info.iocv_error_count++;
		}

		pr_info("%s: sm5713 FG iocv_error_count (%d)\n", __func__, fuelgauge->info.iocv_error_count);

		if (fuelgauge->info.iocv_error_count > 5) /* prevent to overflow */
			fuelgauge->info.iocv_error_count = 6;
	} else {
		fuelgauge->info.iocv_error_count = 0;
	}

	if (fuelgauge->info.iocv_error_count > 5) {
		pr_info("%s: p_v - v = (%d)\n", __func__, fuelgauge->info.p_batt_voltage - fuelgauge->info.batt_voltage);
		if (abs(fuelgauge->info.p_batt_voltage - fuelgauge->info.batt_voltage) > 15) { /* 15mV over */
			fuelgauge->info.iocv_error_count = 0;
		} else {
			/* mode change to mix RS manual mode */
			pr_info("%s: mode change to mix RS manual mode\n", __func__);
			/* RS manual value write */
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, fuelgauge->info.rs_value[0]+5);
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, fuelgauge->info.rs_value[0]-5);
		}
	} else {
		/* voltage mode with 3.4V */
		if (fuelgauge->vempty_mode != VEMPTY_MODE_HW) {
			if ((fuelgauge->info.p_batt_voltage < fuelgauge->info.n_tem_poff) &&
				(fuelgauge->info.batt_voltage < fuelgauge->info.n_tem_poff) && (!fuelgauge->is_charging)) {
				pr_info("%s: mode change to normal tem mix RS manual mode\n", __func__);
				/* mode change to mix RS manual mode */
				/* RS manual value write */
				if ((fuelgauge->info.p_batt_voltage <
					(fuelgauge->info.n_tem_poff - fuelgauge->info.n_tem_poff_offset)) &&
					(fuelgauge->info.batt_voltage <
					(fuelgauge->info.n_tem_poff - fuelgauge->info.n_tem_poff_offset))) {
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, (fuelgauge->info.rs_value[0]>>1)+5);
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, (fuelgauge->info.rs_value[0]>>1)-5);
				} else {
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, fuelgauge->info.rs_value[0]+5);
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, fuelgauge->info.rs_value[0]-5);
				}
			} else {
				pr_info("%s: mode change to mix RS auto mode\n", __func__);
				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, fuelgauge->info.rs_value[3]);
				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, fuelgauge->info.rs_value[4]);
			}
		} else { /* voltage mode with 3.25V, VEMPTY_MODE_HW mode */
			if ((fuelgauge->info.p_batt_voltage < fuelgauge->info.l_tem_poff) &&
				(fuelgauge->info.batt_voltage < fuelgauge->info.l_tem_poff) && (!fuelgauge->is_charging)) {
				pr_info("%s: mode change to normal tem mix RS manual mode\n", __func__);
				/* mode change to mix RS manual mode */
				/* RS manual value write */
				if ((fuelgauge->info.p_batt_voltage <
					(fuelgauge->info.l_tem_poff - fuelgauge->info.l_tem_poff_offset)) &&
					(fuelgauge->info.batt_voltage <
					(fuelgauge->info.l_tem_poff - fuelgauge->info.l_tem_poff_offset))) {
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, (fuelgauge->info.rs_value[0]>>1)+5);
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, (fuelgauge->info.rs_value[0]>>1)-5);
				} else {
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, fuelgauge->info.rs_value[0]+5);
					sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, fuelgauge->info.rs_value[0]-5);
				}
			} else {
				pr_info("%s: mode change to mix RS auto mode\n", __func__);
				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, fuelgauge->info.rs_value[3]);
				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, fuelgauge->info.rs_value[4]);
			}
		}
	}
	fuelgauge->info.p_batt_voltage = fuelgauge->info.batt_voltage;
	fuelgauge->info.p_batt_current = fuelgauge->info.batt_current;
	/* iocv error case cover end */
}

#ifdef ENABLE_FULL_OFFSET
void sm5713_adabt_full_offset(struct sm5713_fuelgauge_data *fuelgauge)
{
	int fg_temp_gap;
	int full_offset, i_offset, sign_offset, curr;
	int curr_off, sign_origin, i_origin;
	int sign_curr, i_curr;
	int aux_stat;

	curr_off = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSP_I_OFF);
	aux_stat = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_STAT);
	pr_info("%s: curr_off=%x, aux_stat=%x, flag_charge_health=%d, flag_chg_status=%d, flag_full_charge=%d\n", __func__,
		curr_off, aux_stat, fuelgauge->info.flag_charge_health, fuelgauge->info.flag_chg_status, fuelgauge->info.flag_full_charge);

	if (fuelgauge->info.full_offset_enable > 0) {
		if (((aux_stat & fuelgauge->info.aux_stat_base) == fuelgauge->info.aux_stat_check)
			&& (fuelgauge->info.batt_avgcurrent < fuelgauge->info.full_offset_margin)
			&& (fuelgauge->info.flag_chg_status == 0)) {
			fg_temp_gap = (fuelgauge->info.temp_fg/10) - fuelgauge->info.temp_std;
			if (abs(fg_temp_gap) < 10) {
				curr = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_CURRENT);
				sign_curr = curr & 0x8000;
				i_curr = (curr & 0x7FFF)>>1;
				if (sign_curr) {
					i_curr = -i_curr;
				}

				curr_off = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSP_I_OFF);
				sign_origin = curr_off & 0x0080;
				i_origin = curr_off & 0x007F;
				if (sign_origin) {
					i_origin = -i_origin;
				}

				full_offset = i_origin - i_curr + fuelgauge->info.full_extra_offset;
				if (full_offset < 0) {
					i_offset = -full_offset;
					sign_offset = 1;
				} else {
					i_offset = full_offset;
					sign_offset = 0;
				}

				pr_info("%s: curr=%x, curr_off=%x, i_offset=%x, sign_offset=%d, full_offset_margin=%x, full_extra_offset=%x\n",
					__func__, curr, curr_off, i_offset, sign_offset, fuelgauge->info.full_offset_margin, fuelgauge->info.full_extra_offset);

				if (sign_offset == 1) {
					full_offset = i_offset|0x0080;
				}

				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_EV_I_OFF, full_offset);
				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSP_I_OFF, full_offset);
				sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSN_I_OFF, full_offset);
				pr_info("%s: LAST i_offset=%x, sign_offset=%x, full_offset=%x\n", __func__, i_offset, sign_offset, full_offset);        
			}
		} else {
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_EV_I_OFF, fuelgauge->info.dp_ecv_i_off);
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSP_I_OFF, fuelgauge->info.dp_csp_i_off);
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSN_I_OFF, fuelgauge->info.dp_csn_i_off);
		}
	}

	return;
}
#endif

#ifdef ENABLE_SM5713_MQ_FUNCTION
static int sm5713_get_full_chg_mq (struct sm5713_fuelgauge_data *fuelgauge)
{
	int mq_raw, mq;

	mq_raw = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1);

	mq = ((mq_raw&0x7FFF) * 1000) >> 11;

	pr_info("%s: raw = 0x%x, mq = %d\n", __func__, mq_raw, mq);

	return mq;
}

static void sm5713_set_full_chg_mq (struct sm5713_fuelgauge_data *fuelgauge, int mq)
{
	int mq_abs;

	mq_abs = mq;
	mq_abs = (mq_abs << 11)/1000;
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1, mq_abs);

	pr_info("%s: mq = %d, abs = 0x%x\n", __func__, mq, mq_abs);
}

static void sm5713_meas_mq_suspend (struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret, suspend_mq;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2);

	if ((ret & START_MQ) == START_MQ) {
		suspend_mq = sm5713_meas_mq_dump(fuelgauge);

		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_START_MQ, ((suspend_mq<<11)/1000));
		pr_info("%s: suspend mode mq is <0x%x>, AUX_2 : <0x%x> \n", __func__, suspend_mq, ret);

		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2, 0);
	} else {
		pr_info("%s: AUX_2 : <0x%x> \n", __func__, ret);
	}
}

static void sm5713_meas_mq_resume (struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;
	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2);
	pr_info("%s: SM5713_FG_REG_AUX_2 : <0x%x> \n", __func__, ret);

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2, START_MQ);
}

static void sm5713_meas_mq_start (struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret, mq;
	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2);
	pr_info("%s: SM5713_FG_REG_AUX_2 : <0x%x> \n", __func__, ret);

	if ((ret & START_MQ) != START_MQ) {
		ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_Q_MEAS_INIT);
		if (ret == 0) {
			mq = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_Q_EST);
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_Q_MEAS_INIT, mq);
			pr_info("%s: starting mq is <0x%x> \n", __func__, mq);

			msleep(50);
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2, START_MQ);

			/* full mq set */
			sm5713_set_full_chg_mq(fuelgauge, ((fuelgauge->info.cap * 1000) >> 11));
		} else {
			pr_info("%s: read start mq is <0x%x> and resumed \n", __func__, ret);
			sm5713_meas_mq_resume(fuelgauge);
		}
	} else {
		pr_info("%s: sm5713_meas_mq_start already started : mq is <%d> \n", __func__, sm5713_meas_mq_dump(fuelgauge));
	}
}

static int sm5713_meas_eq_dump (struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret, eq;
	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_Q_EST);
	pr_info("%s: SM5713_FG_REG_Q_EST : <0x%x> \n", __func__, ret);

	eq = ((ret&0x7FFF) * 1000) >> 11;
	if (ret&0x8000) {
		eq *= -1;
	}

	if (eq == 0) {
		eq = (fuelgauge->info.cap * 1000) >> 11;
		pr_info("%s: eq is 0, It's abnormal, return cap : %d\n", __func__, eq);
	}

	return eq;
}

static int sm5713_meas_mq_dump (struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret, mq = 0, count = 0;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2);

	if ((ret & START_MQ) == START_MQ) {
		pr_info("%s: AUX_2 : <0x%x> \n", __func__, ret);

		ret = START_MQ | DUMP_MQ;
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2, ret);
		msleep(50);
		ret = START_MQ;
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2, ret);

		for (count = 0; count < 5; count++) {
			ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_Q_DUMP);
			if (ret == 0) {
				pr_info("%s: SM5713_FG_REG_MQ : <0x%x> retry %d\n", __func__, ret, count);
				msleep(50);
			} else {
				pr_info("%s: SM5713_FG_REG_MQ : <0x%x> OK count = %d\n", __func__, ret, count);
				break;
			}
		}

		if (ret < 0) {
			mq = ((fuelgauge->info.cap * 1000) >> 11);
			pr_err("%s: sm5713_meas_mq_dump read fail!!!! return battery default value : %d\n", __func__, mq);
		} else {
			mq = ((ret&0x07FF) * 1000) >> 7;
			if (ret&0x8000) {
				mq *= -1;
			}
		}

		if (mq == 0) {
			mq = sm5713_meas_eq_dump(fuelgauge);
			pr_info("%s: mq is 0, It's abnormal, return eq : %d\n", __func__, mq);
		}
	} else {
		mq = ((fuelgauge->info.cap * 1000) >> 11);
		pr_info("%s: sm5713_meas_mq not started : return battery default value : %d, AUX_2 : <0x%x>\n", __func__, mq, ret);
	}

	return mq;
}

static void sm5713_meas_mq_off (struct sm5713_fuelgauge_data *fuelgauge)
{
	int last_mq;
	last_mq = sm5713_meas_mq_dump(fuelgauge);

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_Q_MEAS_INIT, 0);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUX_2, 0);

	pr_info("%s: mq off completed, last_mq : %d\n", __func__, last_mq);
}

#endif

static void sm5713_dp_setup (struct sm5713_fuelgauge_data *fuelgauge)
{
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_EV_I_OFF, fuelgauge->info.dp_ecv_i_off);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSP_I_OFF, fuelgauge->info.dp_csp_i_off);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSN_I_OFF, fuelgauge->info.dp_csn_i_off);

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_EV_I_SLO, fuelgauge->info.dp_ecv_i_slo);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSP_I_SLO, fuelgauge->info.dp_csp_i_slo);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_CSN_I_SLO, fuelgauge->info.dp_csn_i_slo);

	pr_info("%s: dp_off : <0x%x 0x%x 0x%x> dp_slo : <0x%x 0x%x 0x%x>\n",
		__func__,
	fuelgauge->info.dp_ecv_i_off, fuelgauge->info.dp_csp_i_off, fuelgauge->info.dp_csn_i_off,
	fuelgauge->info.dp_ecv_i_slo, fuelgauge->info.dp_csp_i_slo, fuelgauge->info.dp_csn_i_slo);
}

static void sm5713_alg_setup (struct sm5713_fuelgauge_data *fuelgauge)
{
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_EV_I_OFF, fuelgauge->info.ecv_i_off);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSP_I_OFF, fuelgauge->info.csp_i_off);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSN_I_OFF, fuelgauge->info.csn_i_off);

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_EV_I_SLO, fuelgauge->info.ecv_i_slo);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSP_I_SLO, fuelgauge->info.csp_i_slo);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSN_I_SLO, fuelgauge->info.csn_i_slo);

	pr_info("%s: alg_off : <0x%x 0x%x 0x%x> alg_slo : <0x%x 0x%x 0x%x>\n",
		__func__,
	fuelgauge->info.ecv_i_off, fuelgauge->info.csp_i_off, fuelgauge->info.csn_i_off,
	fuelgauge->info.ecv_i_slo, fuelgauge->info.csp_i_slo, fuelgauge->info.csn_i_slo);
}

static void sm5713_coeff_setup (struct sm5713_fuelgauge_data *fuelgauge)
{
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_VSBC_VOLT_TEMP_CAL, fuelgauge->info.coeff);
}

static void sm5713_cal_carc (struct sm5713_fuelgauge_data *fuelgauge)
{
	int curr_cal = 0, p_curr_cal = 0, n_curr_cal = 0, p_delta_cal = 0, n_delta_cal = 0, p_fg_delta_cal = 0, n_fg_delta_cal = 0, temp_curr_offset = 0;
	int temp_gap, fg_temp_gap, mix_factor = 0;

	sm5713_vbatocv_check(fuelgauge);

	if (fuelgauge->is_charging || (fuelgauge->info.batt_current < LIMIT_N_CURR_MIXFACTOR)) {
		mix_factor = fuelgauge->info.rs_value[1];
	} else {
		mix_factor = fuelgauge->info.rs_value[2];
	}
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIX_FACTOR, mix_factor);

	fg_temp_gap = (fuelgauge->info.temp_fg/10) - fuelgauge->info.temp_std;


	temp_curr_offset = fuelgauge->info.ecv_i_off;
	if (fuelgauge->info.en_high_fg_temp_offset && (fg_temp_gap > 0)) {
		if (temp_curr_offset & 0x0080) {
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + (fg_temp_gap / fuelgauge->info.high_fg_temp_offset_denom)*fuelgauge->info.high_fg_temp_offset_fact;
		if (temp_curr_offset < 0) {
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	} else if (fuelgauge->info.en_low_fg_temp_offset && (fg_temp_gap < 0)) {
		if (temp_curr_offset & 0x0080) {
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + ((-fg_temp_gap) / fuelgauge->info.low_fg_temp_offset_denom)*fuelgauge->info.low_fg_temp_offset_fact;
		if (temp_curr_offset < 0) {
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}
	temp_curr_offset = temp_curr_offset | (temp_curr_offset<<8);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_EV_I_OFF, temp_curr_offset);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSP_I_OFF, temp_curr_offset);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSN_I_OFF, temp_curr_offset);

	n_curr_cal = (fuelgauge->info.ecv_i_slo & 0xFF00)>>8;
	p_curr_cal = (fuelgauge->info.ecv_i_slo & 0x00FF);

	if (fuelgauge->info.en_high_fg_temp_cal && (fg_temp_gap > 0)) {
		p_fg_delta_cal = (fg_temp_gap / fuelgauge->info.high_fg_temp_p_cal_denom)*fuelgauge->info.high_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / fuelgauge->info.high_fg_temp_n_cal_denom)*fuelgauge->info.high_fg_temp_n_cal_fact;
	} else if (fuelgauge->info.en_low_fg_temp_cal && (fg_temp_gap < 0)) {
		fg_temp_gap = -fg_temp_gap;
		p_fg_delta_cal = (fg_temp_gap / fuelgauge->info.low_fg_temp_p_cal_denom)*fuelgauge->info.low_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / fuelgauge->info.low_fg_temp_n_cal_denom)*fuelgauge->info.low_fg_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_fg_delta_cal);
	n_curr_cal = n_curr_cal + (n_fg_delta_cal);

	pr_info("%s: <%d %d %d %d %d %d %d %d %d %d>, temp_fg = %d ,p_curr_cal = 0x%x, n_curr_cal = 0x%x, "
		"batt_temp = %d\n",
		__func__,
		fuelgauge->info.en_high_fg_temp_cal,
		fuelgauge->info.high_fg_temp_p_cal_denom, fuelgauge->info.high_fg_temp_p_cal_fact,
		fuelgauge->info.high_fg_temp_n_cal_denom, fuelgauge->info.high_fg_temp_n_cal_fact,
		fuelgauge->info.en_low_fg_temp_cal,
		fuelgauge->info.low_fg_temp_p_cal_denom, fuelgauge->info.low_fg_temp_p_cal_fact,
		fuelgauge->info.low_fg_temp_n_cal_denom, fuelgauge->info.low_fg_temp_n_cal_fact,
		fuelgauge->info.temp_fg, p_curr_cal, n_curr_cal, fuelgauge->info.temperature);

	temp_gap = (fuelgauge->info.temperature/10) - fuelgauge->info.temp_std;
	if (fuelgauge->info.en_high_temp_cal && (temp_gap > 0)) {
		p_delta_cal = (temp_gap / fuelgauge->info.high_temp_p_cal_denom)*fuelgauge->info.high_temp_p_cal_fact;
		n_delta_cal = (temp_gap / fuelgauge->info.high_temp_n_cal_denom)*fuelgauge->info.high_temp_n_cal_fact;
	} else if (fuelgauge->info.en_low_temp_cal && (temp_gap < 0)) {
		temp_gap = -temp_gap;
		p_delta_cal = (temp_gap / fuelgauge->info.low_temp_p_cal_denom)*fuelgauge->info.low_temp_p_cal_fact;
		n_delta_cal = (temp_gap / fuelgauge->info.low_temp_n_cal_denom)*fuelgauge->info.low_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_delta_cal);
	n_curr_cal = n_curr_cal + (n_delta_cal);

	curr_cal = (n_curr_cal << 8) | p_curr_cal;

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_EV_I_SLO, curr_cal);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSP_I_SLO, curr_cal);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CSN_I_SLO, curr_cal);

	pr_info("%s: <%d %d %d %d %d %d %d %d %d %d>, "
		"p_curr_cal = 0x%x, n_curr_cal = 0x%x, mix_factor=0x%x ,curr_cal = 0x%x\n",
		__func__,
		fuelgauge->info.en_high_temp_cal,
		fuelgauge->info.high_temp_p_cal_denom, fuelgauge->info.high_temp_p_cal_fact,
		fuelgauge->info.high_temp_n_cal_denom, fuelgauge->info.high_temp_n_cal_fact,
		fuelgauge->info.en_low_temp_cal,
		fuelgauge->info.low_temp_p_cal_denom, fuelgauge->info.low_temp_p_cal_fact,
		fuelgauge->info.low_temp_n_cal_denom, fuelgauge->info.low_temp_n_cal_fact,
		p_curr_cal, n_curr_cal, mix_factor, curr_cal);

	return;
}

static int sm5713_fg_verified_write_word(struct i2c_client *client,
		u8 reg_addr, u16 data)
{
	int ret;

	ret = sm5713_write_word(client, reg_addr, data);
	if (ret < 0) {
		msleep(50);
		pr_info("1st fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
				__func__, ret, reg_addr, data);
		ret = sm5713_write_word(client, reg_addr, data);
		if (ret < 0) {
			msleep(50);
			pr_info("2nd fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
					__func__, ret, reg_addr, data);
			ret = sm5713_write_word(client, reg_addr, data);
			if (ret < 0) {
				pr_info("3rd fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
						__func__, ret, reg_addr, data);
			}
		}
	}

	return ret;
}

static int sm5713_fg_fs_read_word_table(struct i2c_client *client,
		u8 reg_addr, u8 count)
{
	int ret, i;

	for (i = 0; i < count; i++) {
		ret = sm5713_read_word(client, reg_addr);
		if (ret < 0) {
			pr_err("%s: 1st fail i2c write ret = %d, addr = 0x%x\n, count = %d",
				__func__, ret, reg_addr, count);
		} else {
			if (ret == 0xffff)
				ret = sm5713_read_word(client, reg_addr);
			else
				break;
		}
	}
	return ret;
}

int sm5713_fg_calculate_iocv(struct sm5713_fuelgauge_data *fuelgauge, bool is_vsys)
{
	bool only_lb = false, sign_i_offset = 0; /*valid_cb=false, */
	int roop_start = 0, roop_max = 0, i = 0, cb_last_index = 0, cb_pre_last_index = 0;
	int lb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int lb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int i_offset_margin = 0x14, i_vset_margin = 0x67;
	int v_max = 0, v_min = 0, v_sum = 0, lb_v_avg = 0, cb_v_avg = 0, lb_v_set = 0, lb_i_set = 0, i_offset = 0; /* lb_v_minmax_offset=0, */
	int i_max = 0, i_min = 0, i_sum = 0, lb_i_avg = 0, cb_i_avg = 0, cb_v_set = 0, cb_i_set = 0; /* lb_i_minmax_offset=0, */
	int lb_i_p_v_min = 0, lb_i_n_v_max = 0, cb_i_p_v_min = 0, cb_i_n_v_max = 0;

	int v_ret = 0, i_ret = 0, ret = 0;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_END_V_IDX);
	pr_info("%s: iocv_status_read = addr : 0x%x , data : 0x%x\n", __func__, SM5713_FG_REG_END_V_IDX, ret);

	/* init start */
	if ((ret & 0x0010) == 0x0000) {
		only_lb = true;
	}

/*
	if ((ret & 0x0300) == 0x0300) {
		valid_cb = true;
	}
*/
/* init end */

	/* lb get start */
	roop_max = (ret & 0x000F);
	if (roop_max > FG_INIT_B_LEN)
		roop_max = FG_INIT_B_LEN;

	roop_start = SM5713_FG_REG_START_LB_V;
	for (i = roop_start; i < roop_start + roop_max; i++) {
		if (is_vsys)
			v_ret = sm5713_read_word(fuelgauge->i2c, i+0x10);
		else
			v_ret = sm5713_read_word(fuelgauge->i2c, i);
		i_ret = sm5713_read_word(fuelgauge->i2c, i+0x20);

		if ((i_ret&0x4000) == 0x4000) {
			i_ret = -(i_ret&0x3FFF);
		}

		lb_v_buffer[i-roop_start] = v_ret;
		lb_i_buffer[i-roop_start] = i_ret;

		if (i == roop_start) {
			v_max = v_ret;
			v_min = v_ret;
			v_sum = v_ret;
			i_max = i_ret;
			i_min = i_ret;
			i_sum = i_ret;
		} else {
			if (v_ret > v_max)
				v_max = v_ret;
			else if (v_ret < v_min)
				v_min = v_ret;
			v_sum = v_sum + v_ret;

			if (i_ret > i_max)
				i_max = i_ret;
			else if (i_ret < i_min)
				i_min = i_ret;
			i_sum = i_sum + i_ret;
		}

		if (abs(i_ret) > i_vset_margin) {
			if (i_ret > 0) {
				if (lb_i_p_v_min == 0) {
					lb_i_p_v_min = v_ret;
				} else {
					if (v_ret < lb_i_p_v_min)
						lb_i_p_v_min = v_ret;
				}
			} else {
				if (lb_i_n_v_max == 0) {
					lb_i_n_v_max = v_ret;
				} else {
					if (v_ret > lb_i_n_v_max)
						lb_i_n_v_max = v_ret;
				}
			}
		}
	}
	v_sum = v_sum - v_max - v_min;
	i_sum = i_sum - i_max - i_min;

    /*
	lb_v_minmax_offset = v_max - v_min;
	lb_i_minmax_offset = i_max - i_min;
	*/

	lb_v_avg = v_sum / (roop_max-2);
	lb_i_avg = i_sum / (roop_max-2);
	/* lb get end */

	/* lb_vset start */
	if (abs(lb_i_buffer[roop_max-1]) < i_vset_margin) {
		if (abs(lb_i_buffer[roop_max-2]) < i_vset_margin) {
			lb_v_set = MAXVAL(lb_v_buffer[roop_max-2], lb_v_buffer[roop_max-1]);
			if (abs(lb_i_buffer[roop_max-3]) < i_vset_margin) {
				lb_v_set = MAXVAL(lb_v_buffer[roop_max-3], lb_v_set);
			}
		} else {
			lb_v_set = lb_v_buffer[roop_max-1];
		}
	} else {
		lb_v_set = lb_v_avg;
	}

	if (lb_i_n_v_max > 0) {
		lb_v_set = MAXVAL(lb_i_n_v_max, lb_v_set);
	}
/*
	else if (lb_i_p_v_min > 0) {
		lb_v_set = MINVAL(lb_i_p_v_min, lb_v_set);
	}
	lb_vset end

	lb offset make start
*/
	if (roop_max > 3) {
		lb_i_set = (lb_i_buffer[2] + lb_i_buffer[3]) / 2;
	}

	if ((abs(lb_i_buffer[roop_max-1]) < i_offset_margin) && (abs(lb_i_set) < i_offset_margin)) {
		lb_i_set = MAXVAL(lb_i_buffer[roop_max-1], lb_i_set);
	} else if (abs(lb_i_buffer[roop_max-1]) < i_offset_margin) {
		lb_i_set = lb_i_buffer[roop_max-1];
	} else if (abs(lb_i_set) < i_offset_margin) {
		i_offset = lb_i_set;
	} else {
		lb_i_set = 0;
	}

	i_offset = lb_i_set;

	i_offset = i_offset + 4;	/* add extra offset */

	if (i_offset <= 0) {
		sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
		i_offset = 0;
#else
		i_offset = -i_offset;
#endif
	}

	i_offset = i_offset>>1;

	if (sign_i_offset == 0) {
		i_offset = i_offset|0x0080;
	}
	i_offset = i_offset | i_offset<<8;
/*
	do not write in kernel point.
	sm5713_write_word(client, SM5713_FG_REG_DP_ECV_I_OFF, i_offset);
	lb offset make end
*/
	pr_info("%s: iocv_l_max=0x%x, iocv_l_min=0x%x, iocv_l_avg=0x%x, lb_v_set=0x%x, roop_max=%d \n",
			__func__, v_max, v_min, lb_v_avg, lb_v_set, roop_max);
	pr_info("%s: ioci_l_max=0x%x, ioci_l_min=0x%x, ioci_l_avg=0x%x, lb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
			__func__, i_max, i_min, lb_i_avg, lb_i_set, i_offset, sign_i_offset);

	if (!only_lb) {
		/* cb get start */
		roop_start = SM5713_FG_REG_START_CB_V;
		roop_max = 6;
		for (i = roop_start; i < roop_start + roop_max; i++) {

			if (is_vsys)
				v_ret = sm5713_read_word(fuelgauge->i2c, i+0x10);
			else
				v_ret = sm5713_read_word(fuelgauge->i2c, i);
			i_ret = sm5713_read_word(fuelgauge->i2c, i+0x20);

			if ((i_ret&0x4000) == 0x4000) {
				i_ret = -(i_ret&0x3FFF);
			}

			cb_v_buffer[i-roop_start] = v_ret;
			cb_i_buffer[i-roop_start] = i_ret;

			if (i == roop_start) {
				v_max = v_ret;
				v_min = v_ret;
				v_sum = v_ret;
				i_max = i_ret;
				i_min = i_ret;
				i_sum = i_ret;
			} else {
				if (v_ret > v_max)
					v_max = v_ret;
				else if (v_ret < v_min)
					v_min = v_ret;
				v_sum = v_sum + v_ret;

				if (i_ret > i_max)
					i_max = i_ret;
				else if (i_ret < i_min)
					i_min = i_ret;
				i_sum = i_sum + i_ret;
			}

			if (abs(i_ret) > i_vset_margin) {
				if (i_ret > 0) {
					if (cb_i_p_v_min == 0) {
						cb_i_p_v_min = v_ret;
					} else {
						if (v_ret < cb_i_p_v_min)
							cb_i_p_v_min = v_ret;
					}
				} else {
					if (cb_i_n_v_max == 0) {
						cb_i_n_v_max = v_ret;
					} else {
						if (v_ret > cb_i_n_v_max)
							cb_i_n_v_max = v_ret;
					}
				}
			}
		}
		v_sum = v_sum - v_max - v_min;
		i_sum = i_sum - i_max - i_min;

		cb_v_avg = v_sum / (roop_max-2);
		cb_i_avg = i_sum / (roop_max-2);
		/* cb get end */

		/* cb_vset start */
		cb_last_index = (ret & 0x000F)-7; /*-6-1 */
		if (cb_last_index < 0) {
			cb_last_index = 5;
		}

		for (i = roop_max; i > 0; i--) {
			if (abs(cb_i_buffer[cb_last_index]) < i_vset_margin) {
				cb_v_set = cb_v_buffer[cb_last_index];
				if (abs(cb_i_buffer[cb_last_index]) < i_offset_margin) {
					cb_i_set = cb_i_buffer[cb_last_index];
				}

				cb_pre_last_index = cb_last_index - 1;
				if (cb_pre_last_index < 0) {
					cb_pre_last_index = 5;
				}

				if (abs(cb_i_buffer[cb_pre_last_index]) < i_vset_margin) {
					cb_v_set = MAXVAL(cb_v_buffer[cb_pre_last_index], cb_v_set);
					if (abs(cb_i_buffer[cb_pre_last_index]) < i_offset_margin) {
						cb_i_set = MAXVAL(cb_i_buffer[cb_pre_last_index], cb_i_set);
					}
				}
			} else {
				cb_last_index--;
				if (cb_last_index < 0) {
					cb_last_index = 5;
				}
			}
		}

		if (cb_v_set == 0) {
			cb_v_set = cb_v_avg;
			if (cb_i_set == 0) {
				cb_i_set = cb_i_avg;
			}
		}

		if (cb_i_n_v_max > 0) {
			cb_v_set = MAXVAL(cb_i_n_v_max, cb_v_set);
		}
/*
		else if(cb_i_p_v_min > 0) {
			cb_v_set = MINVAL(cb_i_p_v_min, cb_v_set);
		}
		cb_vset end

		cb offset make start
*/
		if (abs(cb_i_set) < i_offset_margin) {
			if (cb_i_set > lb_i_set) {
				i_offset = cb_i_set;
				i_offset = i_offset + 4;	/* add extra offset */

				if (i_offset <= 0) {
					sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
					i_offset = 0;
#else
					i_offset = -i_offset;
#endif
				}

				i_offset = i_offset>>1;

				if (sign_i_offset == 0) {
					i_offset = i_offset|0x0080;
				}
				i_offset = i_offset | i_offset<<8;

				/* do not write in kernel point. */
				/* sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DP_ECV_I_OFF, i_offset); */
			}
		}
		/* cb offset make end */

		pr_info("%s: iocv_c_max=0x%x, iocv_c_min=0x%x, iocv_c_avg=0x%x, cb_v_set=0x%x, cb_last_index=%d, is_vsys=%d \n",
				__func__, v_max, v_min, cb_v_avg, cb_v_set, cb_last_index, is_vsys);
		pr_info("%s: ioci_c_max=0x%x, ioci_c_min=0x%x, ioci_c_avg=0x%x, cb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
				__func__, i_max, i_min, cb_i_avg, cb_i_set, i_offset, sign_i_offset);

	}

	/* final set */
	if ((abs(cb_i_set) > i_vset_margin) || only_lb) {
		ret = MAXVAL(lb_v_set, cb_i_n_v_max);
	} else {
		ret = cb_v_set;
	}

	if (ret > fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1]) {
		pr_info("%s: iocv ret change 0x%x -> 0x%x \n", __func__, ret, fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1]);
		ret = fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1];
	} else if (ret < fuelgauge->info.battery_table[DISCHARGE_TABLE][0]) {
		pr_info("%s: iocv ret change 0x%x -> 0x%x \n", __func__, ret, (fuelgauge->info.battery_table[DISCHARGE_TABLE][0] + 0x10));
		ret = fuelgauge->info.battery_table[DISCHARGE_TABLE][0] + 0x10;
	}

	return ret;
}

void sm5713_set_cycle_cfg(struct sm5713_fuelgauge_data *fuelgauge)
{
	int value;

	value = fuelgauge->info.cycle_limit_cntl|(fuelgauge->info.cycle_high_limit<<12)|(fuelgauge->info.cycle_low_limit<<8);

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_SOC_CYCLE_CFG, value);

	pr_info("%s: cycle cfg value = 0x%x\n", __func__, value);

}

void sm5713_set_arsm_cfg(struct sm5713_fuelgauge_data *fuelgauge)
{
	int value;

	value = fuelgauge->info.arsm[0]<<15 | fuelgauge->info.arsm[1]<<6 | fuelgauge->info.arsm[2]<<4 | fuelgauge->info.arsm[3];

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUTO_RS_MAN, value);

	pr_info("%s: arsm cfg value = 0x%x\n", __func__, value);
}

#ifdef ENABLE_BATT_LONG_LIFE
int get_v_max_index_by_cycle(struct sm5713_fuelgauge_data *fuelgauge)

{
	int cycle_index = 0, len;

	for (len = fuelgauge->pdata->num_age_step-1; len >= 0; --len) {
		if (fuelgauge->chg_full_soc == fuelgauge->pdata->age_data[len].full_condition_soc) {
			cycle_index = len;
			break;
		}
	}
	pr_info("%s: chg_full_soc = %d, index = %d \n", __func__, fuelgauge->chg_full_soc, cycle_index);

	return cycle_index;
}
#endif


static bool sm5713_fg_reg_init(struct sm5713_fuelgauge_data *fuelgauge, bool is_surge)
{
	int i, j, k, value, ret = 0;
	uint8_t table_reg;
	int write_table[TABLE_MAX][FG_TABLE_LEN+1];
	int error_remain = 0, error_check = 0;

	pr_info("%s: sm5713_fg_reg_init START!!\n", __func__);

	/* init mark */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RESET, FG_INIT_MARK);

	/* start first param_ctrl unlock */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_PARAM_CTRL, FG_PARAM_UNLOCK_CODE);

	/* RCE write */
	for (i = 0; i < 3; i++)	{
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RCE0+i, fuelgauge->info.rce_value[i]);
		pr_info("%s: RCE write RCE%d = 0x%x : 0x%x\n",
				__func__,  i, SM5713_FG_REG_RCE0+i, fuelgauge->info.rce_value[i]);
	}

	/* DTCD write */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_DTCD, fuelgauge->info.dtcd_value);
	pr_info("%s: DTCD write DTCD = 0x%x : 0x%x\n",
			__func__, SM5713_FG_REG_DTCD, fuelgauge->info.dtcd_value);

	/* RS write */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAN, fuelgauge->info.rs_value[0]);
	pr_info("%s: RS write RS = 0x%x : 0x%x\n",
			__func__, SM5713_FG_REG_AUTO_RS_MAN, fuelgauge->info.rs_value[0]);

	/* VIT_PERIOD write */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_VIT_PERIOD, fuelgauge->info.vit_period);
	pr_info("%s: VIT_PERIOD write VIT_PERIOD = 0x%x : 0x%x\n",
			__func__, SM5713_FG_REG_VIT_PERIOD, fuelgauge->info.vit_period);

	/* TABLE_LEN write & pram unlock */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_PARAM_CTRL,
					FG_PARAM_UNLOCK_CODE | FG_TABLE_LEN);
	/* CAP write */
#ifdef ENABLE_BATT_LONG_LIFE
	i = get_v_max_index_by_cycle(fuelgauge);
	pr_info("%s: v_max_now is change %x -> %x \n", __func__, fuelgauge->info.v_max_now, fuelgauge->info.v_max_table[i]);
	pr_info("%s: q_max_now is change %x -> %x \n", __func__, fuelgauge->info.q_max_now, fuelgauge->info.q_max_table[i]);
	fuelgauge->info.v_max_now = fuelgauge->info.v_max_table[i];
	fuelgauge->info.q_max_now = fuelgauge->info.q_max_table[i];
	fuelgauge->info.cap = fuelgauge->info.q_max_now;
#endif

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_BAT_CAP, fuelgauge->info.cap);
	pr_info("%s: SM5713_REG_CAP 0x%x, 0x%x\n",
		__func__, fuelgauge->info.min_cap, fuelgauge->info.cap);

	for (i = 0; i < TABLE_MAX; i++) {
		for (j = 0; j <= FG_TABLE_LEN; j++) {
#ifdef ENABLE_BATT_LONG_LIFE
			if (i == SOC_TABLE) {
					write_table[i][j] = (fuelgauge->info.battery_table[i][j] * 2) - (fuelgauge->info.battery_table[i][j] * fuelgauge->info.q_max_now / fuelgauge->info.maxcap);
				if (j == FG_TABLE_LEN) {
					write_table[i][FG_TABLE_LEN-1] = 100*256;
					write_table[i][FG_TABLE_LEN] = 100*256+26;
				}
			} else {
				write_table[i][j] = fuelgauge->info.battery_table[i][j];
				if (j == FG_TABLE_LEN-1){
					write_table[i][FG_TABLE_LEN-1] = fuelgauge->info.v_max_now;
					if (write_table[i][FG_TABLE_LEN-1] < write_table[i][FG_TABLE_LEN-2]) {
						write_table[i][FG_TABLE_LEN-2] = write_table[i][FG_TABLE_LEN-1] - 0x18; // ~11.7mV
						write_table[SOC_TABLE][FG_TABLE_LEN-2] = (write_table[SOC_TABLE][FG_TABLE_LEN-1]*99)/100;
					}
				}
			}
#else
			write_table[i][j] = fuelgauge->info.battery_table[i][j];
#endif
		}
	}

	for (i = 0; i < TABLE_MAX; i++)	{
		table_reg = SM5713_FG_REG_TABLE_0_START + (i*(FG_TABLE_LEN+1));
		for (j = 0; j <= FG_TABLE_LEN; j++) {
			sm5713_write_word(fuelgauge->i2c, (table_reg + j), write_table[i][j]);
			msleep(10);
			value = sm5713_fg_fs_read_word_table(fuelgauge->i2c,
				(table_reg + j), TABLE_READ_COUNT);
			if (write_table[i][j] == value) {
				pr_info("%s: TABLE write and verify OK [%d][%d] = 0x%x : 0x%x\n",
					__func__, i, j, (table_reg + j), write_table[i][j]);
			} else {
				error_check = 1;

				for (k = 1; k <= I2C_ERROR_COUNT_MAX; k++) {
					pr_info("%s: TABLE write data ERROR!!!! rewrite [%d][%d] = 0x%x : 0x%x, count=%d\n",
						__func__, i, j, (table_reg + j), write_table[i][j], k);
					sm5713_write_word(fuelgauge->i2c, (table_reg + j), write_table[i][j]);
					msleep(30);
					value = sm5713_fg_fs_read_word_table(fuelgauge->i2c,
						(table_reg + j), TABLE_READ_COUNT);
					if (write_table[i][j] == value) {
						pr_info("%s: TABLE rewrite OK [%d][%d] = 0x%x : 0x%x, count=%d\n",
						__func__, i, j, (table_reg + j), write_table[i][j], k);
						break;
					}

					if (k == I2C_ERROR_COUNT_MAX)
						error_remain = 1;
				}
			}
		}
	}

	table_reg = SM5713_FG_REG_TABLE_2_START;
	for (j = 0; j <= FG_ADD_TABLE_LEN; j++) {
		sm5713_write_word(fuelgauge->i2c, (table_reg + j), fuelgauge->info.battery_table[i][j]);
		pr_info("%s: TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
				__func__, i, j, (table_reg + j), fuelgauge->info.battery_table[i][j]);
	}

	/* MIX_MODE write */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIX_FACTOR, fuelgauge->info.rs_value[2]);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MAX, fuelgauge->info.rs_value[3]);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_RS_MIN, fuelgauge->info.rs_value[4]);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_MIX_RATE, fuelgauge->info.mix_value[0]);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_MIX_INIT_BLANK, fuelgauge->info.mix_value[1]);

	pr_info("%s: RS_MIX_FACTOR = 0x%x, RS_MAX = 0x%x, RS_MIN = 0x%x, MIX_RATE = 0x%x, MIX_INIT_BLANK = 0x%x\n",
		__func__,
		fuelgauge->info.rs_value[2], fuelgauge->info.rs_value[3], fuelgauge->info.rs_value[4],
		fuelgauge->info.mix_value[0], fuelgauge->info.mix_value[1]);

	/* v_cal write */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_VOLT_CAL, fuelgauge->info.volt_cal[0]);
	/* need writing value print for debug */

	/* MISC write */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_MISC, fuelgauge->info.misc);
	pr_info("%s: SM5713_REG_MISC 0x%x : 0x%x\n",
		__func__, SM5713_FG_REG_MISC, fuelgauge->info.misc);

	/* TOPOFF SOC */
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_TOPOFF_SOC, fuelgauge->info.topoff_soc);
	pr_info("%s: SM5713_REG_TOPOFF_SOC 0x%x : 0x%x\n", __func__,
		SM5713_FG_REG_TOPOFF_SOC, fuelgauge->info.topoff_soc);

	/* INIT_last -  control register set */
	value = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_CNTL);
	if (value == CNTL_REG_DEFAULT_VALUE) {
		value = fuelgauge->info.cntl_value;
	}
	value = ENABLE_MIX_MODE | ENABLE_TEMP_MEASURE | ENABLE_MANUAL_OCV | (fuelgauge->info.enable_topoff_soc << 13);
	pr_info("%s: SM5713_REG_CNTL reg : 0x%x\n", __func__, value);

	ret = sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CNTL, value);
	if (ret < 0)
		pr_info("%s: fail control register set(%d)\n", __func__, ret);

	pr_info("%s: LAST SM5713_REG_CNTL = 0x%x : 0x%x\n", __func__, SM5713_FG_REG_CNTL, value);

	/* LOCK */
	value = FG_PARAM_LOCK_CODE | FG_TABLE_LEN;
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_PARAM_CTRL, value);
	pr_info("%s: LAST PARAM CTRL VALUE = 0x%x : 0x%x\n", __func__, SM5713_FG_REG_PARAM_CTRL, value);

	/* surge reset defence */
	if (is_surge) {
		value = ((fuelgauge->info.batt_ocv<<8)/125);
	} else {
		if ((sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_STAT) & 0x0002) ||
			factory_mode) {
			value = sm5713_fg_calculate_iocv(fuelgauge, true);
			ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1);
			ret &= ~JIG_CONNECTED;
			ret |= JIG_CONNECTED;
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1, ret);
		} else
			value = sm5713_fg_calculate_iocv(fuelgauge, false);

		if ((fuelgauge->info.volt_cal[0] & 0x0080) == 0x0080) {
			value = value - (fuelgauge->info.volt_cal[0] & 0x007F);
		} else {
			value = value + (fuelgauge->info.volt_cal[0] & 0x007F);
		}
	}

	msleep(10);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_IOCV_MAN, value);
	pr_info("%s: IOCV_MAN_WRITE = %d : 0x%x\n", __func__, SM5713_FG_REG_IOCV_MAN, value);

	/* init delay */
	msleep(20);

	/* write cycle cfg */
	sm5713_set_cycle_cfg(fuelgauge);
	/* write auto_rs_man cfg */
	sm5713_set_arsm_cfg(fuelgauge);

	/* write batt data version */
	value = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1);
	value &= ~DATA_VERSION;
	value |= (fuelgauge->info.data_ver << 4) & DATA_VERSION;
	if (error_remain)
		value |= I2C_ERROR_REMAIN;
	if (error_check)
		value |= I2C_ERROR_CHECK;
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1, value);
	pr_info("%s: RESERVED = %d : 0x%x\n", __func__, SM5713_FG_REG_USER_RESERV_1, value);

	return 1;
}

static int sm5713_abnormal_reset_check(struct sm5713_fuelgauge_data *fuelgauge)
{
	int cntl_read, reset_read;

	reset_read = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_RESET) & 0xF000;
	/* abnormal case process */
	if (sm5713_fg_check_reg_init_need(fuelgauge) || (reset_read == 0)) {
		cntl_read = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_CNTL);
		pr_info("%s: SM5713 FG abnormal case!!!! SM5713_REG_CNTL : 0x%x, is_FG_initialised : %d, reset_read : 0x%x\n", __func__, cntl_read, fuelgauge->info.is_FG_initialised, reset_read);

		if (fuelgauge->info.is_FG_initialised == 1) {
			/* SW reset code */
			fuelgauge->info.is_FG_initialised = 0;
			if (sm5713_fg_verified_write_word(fuelgauge->i2c, SM5713_FG_REG_RESET, SW_RESET_OTP_CODE) < 0) {
				pr_info("%s: Warning!!!! SM5713 FG abnormal case.... SW reset FAIL \n", __func__);
			} else {
				pr_info("%s: SM5713 FG abnormal case.... SW reset OK\n", __func__);
			}
			/* delay 100ms */
			msleep(100);
#ifdef ENABLE_SM5713_MQ_FUNCTION
			sm5713_meas_mq_off(fuelgauge);
#endif
			/* init code */
			sm5713_fg_init(fuelgauge, true);
		}
		return FG_ABNORMAL_RESET;
	}
	return 0;
}

static unsigned int sm5713_get_device_id(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;
	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_DEVICE_ID);
	sm5713_device_id = ret;
	pr_info("%s: SM5713 device_id = 0x%x\n", __func__, ret);

	return ret;
}

int sm5713_call_fg_device_id(void)
{
	pr_info("%s: extern call SM5713 fg_device_id = 0x%x\n", __func__, sm5713_device_id);

	return sm5713_device_id;
}

unsigned int sm5713_get_soc(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;
	unsigned int soc;



	pr_info("%s: \n", __func__);

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_SOC);
	if (ret < 0) {
		pr_err("%s: Warning!!!! read soc reg fail\n", __func__);
		soc = 500;
	} else {
		soc = ((ret&0x7f00)>>8) * 10; /* integer bit; */
		soc = soc + (((ret&0x00ff)*10)/256); /* integer + fractional bit */
	}

	if (ret&0x8000)	{
		soc = 0;
	}
/*	soc = 500; */

	if (sm5713_abnormal_reset_check(fuelgauge) < 0)	{
		pr_info("%s: FG init ERROR!! pre_SOC returned!!, read_SOC = %d, pre_SOC = %d\n", __func__, soc, fuelgauge->info.batt_soc);
		return fuelgauge->info.batt_soc;
	}

#ifdef SM5713_FG_FULL_DEBUG
	pr_info("%s: read = 0x%x, soc = %d\n", __func__, ret, soc);
#endif

	/* for low temp power off test */
	if (fuelgauge->info.volt_alert_flag && (fuelgauge->info.temperature < -100)) {
		pr_info("%s: volt_alert_flag is TRUE!!!! SOC make force ZERO!!!!\n", __func__);
		fuelgauge->info.batt_soc = 0;
		return 0;
	} else {
		fuelgauge->info.batt_soc = soc;
		pr_info("%s: batt_soc = %d, soc = %d\n", __func__, fuelgauge->info.batt_soc, soc);
	}

	return soc;
}

static void sm5713_fg_test_read(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7, ret8, ret9, t_addr;

	t_addr = SM5713_FG_REG_TABLE_0_START + (FG_TABLE_LEN+1);

	ret0 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_TABLE_0_START);
	ret1 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_TABLE_0_START+FG_TABLE_LEN-3);
	ret2 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_TABLE_0_START+FG_TABLE_LEN-2);
	ret3 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_TABLE_0_START+FG_TABLE_LEN-1);
	ret4 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_TABLE_0_START+FG_TABLE_LEN);
	ret5 = sm5713_read_word(fuelgauge->i2c, 0x28);
	ret6 = sm5713_read_word(fuelgauge->i2c, 0x2F);
	ret7 = sm5713_read_word(fuelgauge->i2c, 0x01);
	pr_info("%s: 0xA0=0x%04x, 0xAC=0x%04x, 0xAD=0x%04x, 0xAE=0x%04x, 0xAF=0x%04x, 0x28=0x%04x, 0x2F=0x%04x, 0x01=0x%04x, SM5713_ID=0x%04x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7, sm5713_device_id);

	ret0 = sm5713_read_word(fuelgauge->i2c, t_addr);
	ret1 = sm5713_read_word(fuelgauge->i2c, t_addr+FG_TABLE_LEN-3);
	ret2 = sm5713_read_word(fuelgauge->i2c, t_addr+FG_TABLE_LEN-2);
	ret3 = sm5713_read_word(fuelgauge->i2c, t_addr+FG_TABLE_LEN-1);
	ret4 = sm5713_read_word(fuelgauge->i2c, t_addr+FG_TABLE_LEN);
	ret5 = sm5713_read_word(fuelgauge->i2c, 0x85);
	ret6 = sm5713_read_word(fuelgauge->i2c, 0x86);
	ret7 = sm5713_read_word(fuelgauge->i2c, 0x87);
	ret8 = sm5713_read_word(fuelgauge->i2c, 0x1F);
	ret9 = sm5713_read_word(fuelgauge->i2c, 0x94);
	pr_info("%s: 0xB0=0x%04x, 0xBC=0x%04x, 0xBD=0x%04x, 0xBE=0x%04x, 0xBF=0x%04x, 0x85=0x%04x, 0x86=0x%04x, 0x87=0x%04x, 0x1F=0x%04x, 0x94=0x%04x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7, ret8, ret9);

	return;
}

static void sm5713_update_all_value(struct sm5713_fuelgauge_data *fuelgauge)
{

	union power_supply_propval value;
	int temp;

	fuelgauge->is_charging = (fuelgauge->info.flag_charge_health |
		fuelgauge->ta_exist) && (fuelgauge->info.batt_current >= 30);

	/* check charger status */

	psy_do_property("sm5713-charger", get,
			POWER_SUPPLY_PROP_STATUS, value);
	fuelgauge->info.flag_full_charge =
		(value.intval == POWER_SUPPLY_STATUS_FULL) ? 1 : 0;
	fuelgauge->info.flag_chg_status =
		(value.intval == POWER_SUPPLY_STATUS_CHARGING) ? 1 : 0;

	/* vbat */
	sm5713_get_vbat(fuelgauge);

	/* current */
	sm5713_get_curr(fuelgauge);

	/* ocv */
	sm5713_get_ocv(fuelgauge);

	/* temperature */
	sm5713_get_temperature(fuelgauge);

	/* cycle */
	sm5713_get_cycle(fuelgauge);

	/* carc */
	sm5713_cal_carc(fuelgauge);

	/* soc */
	sm5713_get_soc(fuelgauge);


	sm5713_fg_test_read(fuelgauge);

	pr_info("%s: chg_h=%d, chg_f=%d, chg_s=%d, is_chg=%d, ta_exist=%d, "
		"v=%d, v_avg=%d, i=%d, i_avg=%d, ocv=%d, fg_t=%d, b_t=%d, cycle=%d, soc=%d, state=0x%x\n",
		__func__, fuelgauge->info.flag_charge_health, fuelgauge->info.flag_full_charge,
		fuelgauge->info.flag_chg_status, fuelgauge->is_charging, fuelgauge->ta_exist,
		fuelgauge->info.batt_voltage, fuelgauge->info.batt_avgvoltage,
		fuelgauge->info.batt_current, fuelgauge->info.batt_avgcurrent, fuelgauge->info.batt_ocv,
		fuelgauge->info.temp_fg, fuelgauge->info.temperature, fuelgauge->info.batt_soc_cycle,
		fuelgauge->info.batt_soc, sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_OCV_STATE));

#ifdef ENABLE_SM5713_MQ_FUNCTION
	fuelgauge->info.full_mq_dump = sm5713_meas_mq_dump(fuelgauge);
#endif

	// for abnormal case asoc update stop, MUST USE AFTER PASSS_5
	if (fuelgauge->pmic_rev > 5)
	{
		if (fuelgauge->info.temperature/10 < 18)
		{
			temp = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_MISC);
			temp = temp & !0x0040;
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_MISC, temp);
		}
		else if ((fuelgauge->info.temperature/10 > fuelgauge->info.temp_std) && (fuelgauge->info.batt_soc > 950))
		{
			temp = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_MISC);
			temp = temp | 0x0040;
			sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_MISC, temp);
		}
		pr_info("%s : temp = 0x%x, t = %d \n", __func__, temp, fuelgauge->info.temperature);
	}
	// for abnormal case asoc update stop, MUST USE AFTER PASSS_5
}

static int sm5713_fg_set_jig_mode_real_vbat(struct sm5713_fuelgauge_data *fuelgauge, int meas_mode)
{
	int stat;

	fuelgauge->isjigmoderealvbat = false;

	if (sm5713_fg_check_reg_init_need(fuelgauge)) {
		pr_info("%s: FG init fail!! \n", __func__);
		return -1;
	}

	/** meas_mode = 0 is inbat mode with jig **/
	if (meas_mode == 0)	{
		stat = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AUX_STAT);
		if (stat & 0x0002) {
			fuelgauge->isjigmoderealvbat = true;
			pr_info("%s: FG check jig_ON!! and after read real VBAT!! \n", __func__);
		} else
			pr_info("%s: isjigmoderealvbat = 1 but FG check nENQ4 LOW!! check jig!! \n", __func__);
	} else
		pr_info("%s: meas_mode = 1, isjigmoderealvbat = false!! \n", __func__);

	return 0;
}

static int sm5713_fg_check_battery_present(struct sm5713_fuelgauge_data *fuelgauge)
{
	/* SM5713 is not suport batt present */
	pr_info("%s: sm5713_fg_get_batt_present\n", __func__);

	return true;
}

static bool sm5713_check_jig_status(struct sm5713_fuelgauge_data *fuelgauge)
{
	bool ret = false;

	if (fuelgauge->pdata->jig_gpio) {
		if (fuelgauge->pdata->jig_low_active)
			ret = !gpio_get_value(fuelgauge->pdata->jig_gpio);
		else
			ret = gpio_get_value(fuelgauge->pdata->jig_gpio);
	}
	pr_info("%s: jig_gpio(%d), ret(%d)\n",
		__func__, fuelgauge->pdata->jig_gpio, ret);
	return ret;
}

void sm5713_fg_reset_capacity_by_jig_connection(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1);
	ret &= ~JIG_CONNECTED;
	ret |= JIG_CONNECTED;
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1, ret);

	pr_info("%s: set JIG_CONNECTED(0x%04x) (Jig Connection or bypass)\n", __func__, ret);
}

int sm5713_fg_alert_init(struct sm5713_fuelgauge_data *fuelgauge, int soc)
{
	int ret;
	int value_soc_alarm;

	fuelgauge->is_fuel_alerted = false;

	/* remove interrupt */
	/* ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_INTFG); */

	/* check status */
	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_STATUS);
	if (ret < 0) {
		pr_err("%s: Failed to read SM5713_FG_REG_STATUS\n", __func__);
		return -1;
	}

	/* remove all mask */
	/* sm5713_write_word(fuelgauge->i2c,SM5713_FG_REG_INTFG_MASK, 0x0000); */

	/* enable volt alert only, other alert mask */
/*
	ret = MASK_L_SOC_INT|MASK_H_TEM_INT|MASK_L_TEM_INT;
	sm5713_write_word(fuelgauge->i2c,SM5713_FG_REG_INTFG_MASK,ret);
	fuelgauge->info.irq_ctrl = ~(ret);
*/
	value_soc_alarm = (soc<<8); /* 0x0100 = 1.00% */
	if (sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_SOC_ALARM, value_soc_alarm) < 0) {
		pr_err("%s: Failed to write SM5713_FG_REG_SOC_ALARM\n", __func__);
		return -1;
	}

	/* enabel volt alert control, other alert disable */
	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_CNTL);
	if (ret < 0) {
		pr_err("%s: Failed to read SM5713_FG_REG_CNTL\n", __func__);
		return -1;
	}
	ret = ret | ENABLE_V_ALARM;
	ret = ret & (~ENABLE_SOC_ALARM & ~ENABLE_T_H_ALARM & ~ENABLE_T_L_ALARM);
	if (sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_CNTL, ret) < 0) {
		pr_err("%s: Failed to write SM5713_REG_CNTL\n", __func__);
		return -1;
	}

	pr_info("%s: fg_irq= 0x%x, REG_CNTL=0x%x, SOC_ALARM=0x%x \n",
		__func__, fuelgauge->fg_irq, ret, value_soc_alarm);

	return 1;
}

static int sm5713_set_tcal_ioff(struct sm5713_fuelgauge_data *fuelgauge)
{
	int tcal, ioff;

	tcal = fuelgauge->info.tcal_ioff[0];
	ioff = fuelgauge->info.tcal_ioff[1];

	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_VOLT_TEMP_CAL, tcal);
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AUX_3, ioff);
	pr_info("%s : set tcal & ioff!! tcal = 0x%x, ioff = 0x%x\n", __func__, tcal, ioff);

	return 0;
}

static int sm5713_asoc_init(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret, temp;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_AGING_CTRL);
	pr_info("%s 0x%x : 0x%x\n", __func__, SM5713_FG_REG_AGING_CTRL, ret);
	if(ret != fuelgauge->info.age_cntl){
		if (sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_AGING_CTRL, fuelgauge->info.age_cntl) < 0) {
			pr_err("%s: Failed to write SM5713_FG_REG_AGING_CTRL\n", __func__);
		}
	}

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_2);
	temp = ret;
	ret = ret & 0x7F;
	if (ret == 0) {
		fuelgauge->info.soh = 100;
		temp = temp & 0x80;
		temp = temp | fuelgauge->info.soh;
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_2, temp);
		pr_info("%s : soh reset %d, write 0x%x\n", __func__, fuelgauge->info.soh, temp);
	} else {
		fuelgauge->info.soh = ret;
		pr_info("%s asoc restore : %d\n", __func__, fuelgauge->info.soh);
	}

	return 0;
}

static irqreturn_t sm5713_jig_irq_thread(int irq, void *irq_data)
{
	struct sm5713_fuelgauge_data *fuelgauge = irq_data;

	if (sm5713_check_jig_status(fuelgauge))
		sm5713_fg_reset_capacity_by_jig_connection(fuelgauge);
	else
		pr_info("%s: jig removed\n", __func__);
	return IRQ_HANDLED;
}

static void sm5713_fg_buffer_read(struct sm5713_fuelgauge_data *fuelgauge)
{
	int ret0, ret1, ret2, ret3, ret4, ret5;

	ret0 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_V);
	ret1 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_V+1);
	ret2 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_V+2);
	ret3 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_V+3);
	ret4 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_V+4);
	ret5 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_V+5);
	pr_info("%s: sm5713 FG buffer 0x30_0x35 lb_V = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);

	ret0 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_V);
	ret1 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_V+1);
	ret2 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_V+2);
	ret3 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_V+3);
	ret4 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_V+4);
	ret5 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_V+5);
	pr_info("%s: sm5713 FG buffer 0x36_0x3B cb_V = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);


	ret0 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_I);
	ret1 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_I+1);
	ret2 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_I+2);
	ret3 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_I+3);
	ret4 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_I+4);
	ret5 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_LB_I+5);
	pr_info("%s: sm5713 FG buffer 0x40_0x45 lb_I = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);


	ret0 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_I);
	ret1 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_I+1);
	ret2 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_I+2);
	ret3 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_I+3);
	ret4 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_I+4);
	ret5 = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_START_CB_I+5);
	pr_info("%s: sm5713 FG buffer 0x46_0x4B cb_I = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);

	return;
}

static bool sm5713_fg_init(struct sm5713_fuelgauge_data *fuelgauge, bool is_surge)
{
	int error_remain, ret;

	fuelgauge->info.is_FG_initialised = 0;

	if (sm5713_get_device_id(fuelgauge) < 0) {
		return false;
	}
	sm5713_fg_check_battery_present(fuelgauge);

	if (fuelgauge->pdata->jig_gpio) {
		int ret;
		/* if (fuelgauge->pdata->jig_low_active) { */
		if (0) {
			ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
				NULL, sm5713_jig_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"jig-irq", fuelgauge);
		} else {
			ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
				NULL, sm5713_jig_irq_thread,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"jig-irq", fuelgauge);
		}
		if (ret) {
			pr_info("%s: Failed to Request IRQ\n",
				__func__);
		}
		pr_info("%s: jig_result : %d\n", __func__, sm5713_check_jig_status(fuelgauge));

		/* initial check for the JIG */
		if (sm5713_check_jig_status(fuelgauge))
			sm5713_fg_reset_capacity_by_jig_connection(fuelgauge);
	}

#ifdef ENABLE_BATT_LONG_LIFE
	fuelgauge->info.q_max_now = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_BAT_CAP);
	pr_info("%s: q_max_now = 0x%x\n", __func__, fuelgauge->info.q_max_now);
#endif

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_USER_RESERV_1);
	error_remain = (ret & I2C_ERROR_REMAIN) ? 1 : 0;
	pr_info("%s: reserv_1 = 0x%x\n", __func__, ret);

	if (sm5713_fg_check_reg_init_need(fuelgauge) || error_remain) {
		if (sm5713_fg_reg_init(fuelgauge, is_surge))
			pr_info("%s: boot time kernel init DONE!\n", __func__);
		else
			pr_info("%s: ERROR!! boot time kernel init ERROR!!\n", __func__);
	} else {
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_MIX_RATE, fuelgauge->info.mix_value[0]);
	}

	sm5713_dp_setup(fuelgauge);
	sm5713_alg_setup(fuelgauge);

 	sm5713_coeff_setup(fuelgauge);

	/* for debug */
	sm5713_fg_buffer_read(fuelgauge);

#ifdef ENABLE_SM5713_MQ_FUNCTION
	/* for start mq */
	if (is_surge) {
		sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_Q_MEAS_INIT, ((fuelgauge->info.full_mq_dump<<11)/1000));
	}
	sm5713_meas_mq_start(fuelgauge);
#endif

	sm5713_set_tcal_ioff(fuelgauge);
	sm5713_asoc_init(fuelgauge);

	fuelgauge->info.is_FG_initialised = 1;

	return true;
}

bool sm5713_fg_fuelalert_init(struct sm5713_fuelgauge_data *fuelgauge,
				int soc)
{
	/* 1. Set sm5713 alert configuration. */
	if (sm5713_fg_alert_init(fuelgauge, soc) > 0)
		return true;
	else
		return false;
}

void sm5713_fg_fuelalert_set(struct sm5713_fuelgauge_data *fuelgauge,
				int enable)
{
	u16 ret;

	ret = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_STATUS);
	pr_info("%s: SM5713_FG_REG_STATUS(0x%x)\n",
		__func__, ret);

	/* not use SOC alarm
	if (ret & fuelgauge->info.irq_ctrl & ENABLE_SOC_ALARM) {
		fuelgauge->info.soc_alert_flag = true;
*/		/* todo more action */
/*	}
	*/

	if (ret & ENABLE_V_ALARM && !lpcharge && !fuelgauge->is_charging) {
		pr_info("%s : Battery Voltage is Very Low!! SW V EMPTY ENABLE\n", __func__);

		if (fuelgauge->vempty_mode == VEMPTY_MODE_SW ||
					fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT) {
			fuelgauge->vempty_mode = VEMPTY_MODE_SW_VALERT;
		}
#if defined(CONFIG_BATTERY_CISD)
		else {
			union power_supply_propval value;
			value.intval = fuelgauge->vempty_mode;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_VOLTAGE_MIN, value);
		}
#endif
	}
}

bool sm5713_fg_fuelalert_process(void *irq_data)
{
	struct sm5713_fuelgauge_data *fuelgauge =
		(struct sm5713_fuelgauge_data *)irq_data;

	sm5713_fg_fuelalert_set(fuelgauge, 0);

	return true;
}

bool sm5713_fg_reset(struct sm5713_fuelgauge_data *fuelgauge, bool is_quickstart)
{
	if (fuelgauge->info.is_FG_initialised == 0) {
		pr_info("%s: Not work reset! prev init working! return! \n", __func__);
		return true;
	}

	pr_info("%s: Start fg reset\n", __func__);
	/* SW reset code */
	fuelgauge->info.is_FG_initialised = 0;
	sm5713_fg_verified_write_word(fuelgauge->i2c, SM5713_FG_REG_RESET, SW_RESET_CODE);
	/* delay 1000ms */
	msleep(1000);
#ifdef ENABLE_SM5713_MQ_FUNCTION
	sm5713_meas_mq_off(fuelgauge);
#endif

	if (is_quickstart) {
		if (sm5713_fg_init(fuelgauge, false)) {
			pr_info("%s: Quick Start - mq STOP!!\n", __func__);
#ifdef ENABLE_SM5713_MQ_FUNCTION
			sm5713_meas_mq_off(fuelgauge);
#endif
		} else {
			pr_info("%s: sm5713_fg_init ERROR!!!!\n", __func__);
			return false;
		}
	}
#ifdef ENABLE_BATT_LONG_LIFE
	else {
		if (sm5713_fg_init(fuelgauge, true)) {
			pr_info("%s: BATT_LONG_LIFE reset - mq CONTINUE!!\n", __func__);
		} else {
			pr_info("%s: sm5713_fg_init ERROR!!!!\n", __func__);
			return false;
		}
	}
#endif

	pr_info("%s: End fg reset\n", __func__);

	return true;
}

static int sm5713_fg_check_capacity_max(
	struct sm5713_fuelgauge_data *fuelgauge, int capacity_max)
{
	int cap_max, cap_min;

	cap_max = fuelgauge->pdata->capacity_max;
	cap_min = (fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin);

	return (capacity_max < cap_min) ? cap_min :
		((capacity_max >= cap_max) ? cap_max : capacity_max);
}

static void sm5713_fg_get_scaled_capacity(
	struct sm5713_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_info("%s: scaled capacity (%d.%d)\n",
		__func__, val->intval/10, val->intval%10);
}


/* capacity is integer */
static void sm5713_fg_get_atomic_capacity(
	struct sm5713_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{

	pr_debug("%s : NOW(%d), OLD(%d)\n",
		__func__, val->intval, fuelgauge->capacity_old);

	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
		if (fuelgauge->capacity_old < val->intval)
			val->intval = fuelgauge->capacity_old + 1;
		else if (fuelgauge->capacity_old > val->intval)
			val->intval = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
			fuelgauge->capacity_old < val->intval) {
			pr_err("%s: capacity (old %d : new %d)\n",
			__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int sm5713_fg_calculate_dynamic_scale(
	struct sm5713_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = sm5713_get_soc(fuelgauge);

	if (raw_soc_val.intval <
		fuelgauge->pdata->capacity_max -
		fuelgauge->pdata->capacity_max_margin) {
		pr_info("%s: raw soc(%d) is very low, skip routine\n",
			__func__, raw_soc_val.intval);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval * 100 / (capacity + 1));
		fuelgauge->capacity_old = capacity;

		fuelgauge->capacity_max =
			sm5713_fg_check_capacity_max(fuelgauge,
			fuelgauge->capacity_max);

		pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
			__func__, fuelgauge->capacity_max, capacity);
	}

	return fuelgauge->capacity_max;
}

#if defined(CONFIG_EN_OOPS)
static void sm5713_set_full_value(struct sm5713_fuelgauge_data *fuelgauge,
					int cable_type)
{
	pr_info("%s : sm5713 todo\n",
		__func__);
}
#endif

static int calc_ttf(struct sm5713_fuelgauge_data *fuelgauge, union power_supply_propval *val)
{
	int i;
	int cc_time = 0, cv_time = 0;

	int soc = fuelgauge->raw_capacity;
	int charge_current = val->intval;
	struct cv_slope *cv_data = fuelgauge->cv_data;
	int design_cap = fuelgauge->ttf_capacity;

	if (!cv_data || (val->intval <= 0)) {
		pr_info("%s: no cv_data or val: %d\n", __func__, val->intval);
		return -1;
	}
	for (i = 0; i < fuelgauge->cv_data_lenth; i++) {
		if (charge_current >= cv_data[i].fg_current)
			break;
	}
	i = i >= fuelgauge->cv_data_lenth ? fuelgauge->cv_data_lenth - 1 : i;
	if (cv_data[i].soc < soc) {
		for (i = 0; i < fuelgauge->cv_data_lenth; i++) {
			if (soc <= cv_data[i].soc)
				break;
		}
		cv_time = ((cv_data[i-1].time - cv_data[i].time) * (cv_data[i].soc - soc)\
				/ (cv_data[i].soc - cv_data[i-1].soc)) + cv_data[i].time;
	} else { /* CC mode || NONE */
		cv_time = cv_data[i].time;
		cc_time = design_cap * (cv_data[i].soc - soc)\
				/ val->intval * 3600 / 1000;
		pr_debug("%s: cc_time: %d\n", __func__, cc_time);
		if (cc_time < 0) {
			cc_time = 0;
		}
	}

	pr_info("%s: cap: %d, soc: %4d, T: %6d, avg: %4d, cv soc: %4d, i: %4d, val: %d\n",
		__func__, design_cap, soc, cv_time + cc_time, fuelgauge->info.batt_avgcurrent, cv_data[i].soc, i, val->intval);

	if (cv_time + cc_time >= 0)
		return cv_time + cc_time + 60;
	else
		return 60; /* minimum 1minutes */
}

static void sm5713_fg_set_vempty(struct sm5713_fuelgauge_data *fuelgauge, int vempty_mode)
{
	u16 data = 0;
	u32 value_v_alarm = 0;

	if (!fuelgauge->using_temp_compensation) {
		pr_info("%s: does not use temp compensation, default hw vempty\n", __func__);
		vempty_mode = VEMPTY_MODE_HW;
	}

	fuelgauge->vempty_mode = vempty_mode;

	switch (vempty_mode) {
	case VEMPTY_MODE_SW:
		/* HW Vempty Disable */

		/* set volt alert threshold */
		value_v_alarm = (fuelgauge->battery_data->sw_v_empty_vol << 8)/1000;

		if (sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_V_L_ALARM, value_v_alarm) < 0) {
			pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
			return;
		}
		data = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_V_L_ALARM);
		pr_info("%s: HW V EMPTY Disable, SW V EMPTY Enable with %d mV (%d) \n",
			__func__, fuelgauge->battery_data->sw_v_empty_vol, data);
		break;
	default:
		/* HW Vempty works together with CISD v_alarm */
		value_v_alarm = (fuelgauge->battery_data->sw_v_empty_vol_cisd << 8)/1000;

		if (sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_V_L_ALARM, value_v_alarm) < 0) {
			pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
			return;
		}
		data = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_V_L_ALARM);
		pr_info("%s: HW V EMPTY Enable, SW V EMPTY Disable %d mV (%d) \n",
			__func__, fuelgauge->battery_data->sw_v_empty_vol_cisd, data);
		break;
	}

	/* for v_alarm Hysteresis */
	value_v_alarm = ((fuelgauge->info.value_v_alarm_hys << 4) / 1000)<<4;
	if (sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_V_ALARM_HYS, value_v_alarm) < 0) {
		pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return;
	}
	data = sm5713_read_word(fuelgauge->i2c, SM5713_FG_REG_V_ALARM_HYS);
	pr_info("%s: VALRT_THRESHOLD hysteresis set %d mV (0x%x) \n",
		__func__, fuelgauge->info.value_v_alarm_hys, data);
}

static int sm5713_fg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sm5713_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
/*											  
	static int abnormal_current_cnt = 0;
	union power_supply_propval value;
*/

	pr_info("%s: psp = 0x%x\n", __func__, psp);
	switch (psp) {
	/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		sm5713_get_vbat(fuelgauge);
		val->intval = fuelgauge->info.batt_voltage;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_OCV:
			sm5713_get_ocv(fuelgauge);
			val->intval = fuelgauge->info.batt_ocv;
			break;
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = fuelgauge->info.batt_avgvoltage;
			break;
		}
		break;
	/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		sm5713_get_curr(fuelgauge);
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = fuelgauge->info.batt_current * 1000;
		else
			val->intval = fuelgauge->info.batt_current;
		break;
	/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = fuelgauge->info.batt_avgcurrent * 1000;
		else
			val->intval = fuelgauge->info.batt_avgcurrent;
		break;
	/* Full Capacity */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_DESIGNED:
			break;
		case SEC_BATTERY_CAPACITY_ABSOLUTE:
			break;
		case SEC_BATTERY_CAPACITY_TEMPERARY:
			break;
		case SEC_BATTERY_CAPACITY_CURRENT:
			break;
		case SEC_BATTERY_CAPACITY_AGEDCELL:
			break;
		case SEC_BATTERY_CAPACITY_CYCLE:
			sm5713_get_cycle(fuelgauge);
			val->intval = fuelgauge->info.batt_soc_cycle;
			break;
/*		case SEC_BATTERY_CAPACITY_FULL: */
#ifdef ENABLE_SM5713_MQ_FUNCTION
			if (sm5713_meas_mq_dump(fuelgauge) > sm5713_get_full_chg_mq(fuelgauge))
				val->intval = sm5713_meas_mq_dump(fuelgauge);
			else
				val->intval = sm5713_get_full_chg_mq(fuelgauge);
#endif
/*		break; */
		}
		break;
	/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		sm5713_update_all_value(fuelgauge);

		/* SM5713 F/G unit is 0.1%, raw ==> convert the unit to 0.01% */
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = fuelgauge->info.batt_soc * 10;
			break;
		} else
			val->intval = fuelgauge->info.batt_soc;

		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
			SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {
			sm5713_fg_get_scaled_capacity(fuelgauge, val);

			if (val->intval > 1010) {
				pr_info("%s : scaled capacity (%d)\n", __func__, val->intval);
				sm5713_fg_calculate_dynamic_scale(fuelgauge, 100);
			}
		}

		/* capacity should be between 0% and 100%
		* (0.1% degree)
		*/
		if (val->intval > 1000)
			val->intval = 1000;
		if (val->intval < 0)
			val->intval = 0;

		fuelgauge->raw_capacity = val->intval;

		/* get only integer part */
		val->intval /= 10;

		/* SW/HW V Empty setting */
		if (fuelgauge->using_hw_vempty) {
			if (fuelgauge->info.temperature <= (int)fuelgauge->low_temp_limit) {
				if (fuelgauge->raw_capacity <= 50) {
					if (fuelgauge->vempty_mode != VEMPTY_MODE_HW) {
						sm5713_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
					}
				} else if (fuelgauge->vempty_mode == VEMPTY_MODE_HW) {
					sm5713_fg_set_vempty(fuelgauge, VEMPTY_MODE_SW);
				}
			} else if (fuelgauge->vempty_mode != VEMPTY_MODE_HW) {
				sm5713_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
			}
		}

		if (!fuelgauge->is_charging &&
		    fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT && !lpcharge) {
			pr_info("%s : SW V EMPTY. Decrease SOC\n", __func__);
			val->intval = 0;
		} else if ((fuelgauge->vempty_mode == VEMPTY_MODE_SW_RECOVERY) &&
			   (val->intval == fuelgauge->capacity_old)) {
			fuelgauge->vempty_mode = VEMPTY_MODE_SW;
		}


		/* check whether doing the wake_unlock */
		if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
			fuelgauge->is_fuel_alerted) {
			sm5713_fg_fuelalert_init(fuelgauge,
				fuelgauge->pdata->fuel_alert_soc);
		}

		/* (Only for atomic capacity)
		* In initial time, capacity_old is 0.
		* and in resume from sleep,
		* capacity_old is too different from actual soc.
		* should update capacity_old
		* by val->intval in booting or resume.
		*/
		if ((fuelgauge->initial_update_of_soc) &&
			(fuelgauge->vempty_mode != VEMPTY_MODE_SW_VALERT)) {
			/* updated old capacity */
			fuelgauge->capacity_old = val->intval;
			fuelgauge->initial_update_of_soc = false;
			break;
		}

		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
			SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL)){
			sm5713_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		sm5713_get_temperature(fuelgauge);
		val->intval = fuelgauge->info.temp_fg;
		break;
#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		return -ENODATA;
#endif
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = sm5713_get_asoc(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = calc_ttf(fuelgauge, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval =
			fuelgauge->battery_data->Capacity * fuelgauge->raw_capacity; //uAh
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		return -ENODATA;
#endif
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_JIG_GPIO:
			if (fuelgauge->pdata->jig_gpio)
				val->intval = gpio_get_value(fuelgauge->pdata->jig_gpio);
			else
				val->intval = -1;
			pr_info("%s: jig gpio = %d \n", __func__, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			/* not supported */
			val->intval = 0;
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
#if !defined(CONFIG_SEC_FACTORY)
			sm5713_fg_periodic_read(fuelgauge);
#endif
			break;
		default:
			return -EINVAL;			
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int sm5713_fuelgauge_parse_dt(struct sm5713_fuelgauge_data *fuelgauge);
#endif
static int sm5713_fg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sm5713_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	/*u8 data[2] = {0, 0}; */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
#ifdef ENABLE_BATT_LONG_LIFE
		if (val->intval == POWER_SUPPLY_STATUS_FULL) {			
			pr_info("%s: POWER_SUPPLY_STATUS_FULL : q_max_now = 0x%x \n", __func__, fuelgauge->info.q_max_now);
			if (fuelgauge->info.q_max_now !=
				fuelgauge->info.q_max_table[get_v_max_index_by_cycle(fuelgauge)]) {
				if (!sm5713_fg_reset(fuelgauge, false))
					return -EINVAL;
			}
		}
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
			sm5713_fg_calculate_dynamic_scale(fuelgauge, val->intval);

#ifdef ENABLE_SM5713_MQ_FUNCTION
		if (sm5713_meas_mq_dump(fuelgauge) > sm5713_get_full_chg_mq(fuelgauge))
			sm5713_set_full_chg_mq(fuelgauge, sm5713_meas_mq_dump(fuelgauge));
#endif
		break;
#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		sm5713_set_full_value(fuelgauge, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (is_nocharge_type(fuelgauge->cable_type)) {
			fuelgauge->ta_exist = false;
			fuelgauge->is_charging = false;
		} else {
			fuelgauge->ta_exist = true;
			fuelgauge->is_charging = true;

			/* enable alert */
			if (fuelgauge->vempty_mode >= VEMPTY_MODE_SW_VALERT) {
				sm5713_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
				fuelgauge->initial_update_of_soc = true;
				sm5713_fg_fuelalert_init(fuelgauge,
							fuelgauge->pdata->fuel_alert_soc);
			}
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			if (!sm5713_fg_reset(fuelgauge, true))
				return -EINVAL;
			else
				fuelgauge->initial_update_of_soc = true;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		fuelgauge->info.temperature = val->intval;
		if (val->intval < 0) {
				pr_info("%s: set the low temp reset! temp : %d\n",
						__func__, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		fuelgauge->info.flag_charge_health =
		(val->intval == POWER_SUPPLY_HEALTH_GOOD) ? 1 : 0;
		pr_info("%s: charge health from charger = 0x%x\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		sm5713_fg_reset_capacity_by_jig_connection(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		pr_info("%s: capacity_max changed, %d -> %d\n",
			__func__, fuelgauge->capacity_max, val->intval);
		fuelgauge->capacity_max = sm5713_fg_check_capacity_max(fuelgauge, val->intval);
		fuelgauge->initial_update_of_soc = true;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		pr_info("%s: full condition soc changed, %d -> %d\n",
			__func__, fuelgauge->chg_full_soc, val->intval);
		fuelgauge->chg_full_soc = val->intval;
		break;
#endif

#if defined(CONFIG_UPDATE_BATTERY_DATA)
	case POWER_SUPPLY_PROP_POWER_DESIGN:
		sm5713_fuelgauge_parse_dt(fuelgauge);
		break;
#endif
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_INBAT_VOLTAGE_FGSRC_SWITCHING:
			sm5713_fg_set_jig_mode_real_vbat(fuelgauge, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY:
			if (val->intval) {
				pr_info("%s: bypass mode is enabled\n", __func__);
				sm5713_fg_reset_capacity_by_jig_connection(fuelgauge);
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void sm5713_fg_isr_work(struct work_struct *work)
{
	struct sm5713_fuelgauge_data *fuelgauge =
		container_of(work, struct sm5713_fuelgauge_data, isr_work.work);

	/* process for fuel gauge chip */
	sm5713_fg_fuelalert_process(fuelgauge);

	wake_unlock(&fuelgauge->fuel_alert_wake_lock);
}

static irqreturn_t sm5713_fg_irq_thread(int irq, void *irq_data)
{
	struct sm5713_fuelgauge_data *fuelgauge = irq_data;

	pr_info("%s\n", __func__);

	if (fuelgauge->is_fuel_alerted) {
		return IRQ_HANDLED;
	} else {
		wake_lock(&fuelgauge->fuel_alert_wake_lock);
		fuelgauge->is_fuel_alerted = true;
		schedule_delayed_work(&fuelgauge->isr_work, 0);
	}

	return IRQ_HANDLED;
}

static int sm5713_fuelgauge_debugfs_show(struct seq_file *s, void *data)
{
	struct sm5713_fuelgauge_data *fuelgauge = s->private;
	int i;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "SM5713 FUELGAUGE IC :\n");
	seq_printf(s, "===================\n");
	for (i = 0; i < 16; i++) {
		if (i == 12)
			continue;
		for (reg = 0; reg < 0x10; reg++) {
			reg_data = sm5713_read_word(fuelgauge->i2c, reg + i * 0x10);
			seq_printf(s, "0x%02x:\t0x%04x\n", reg + i * 0x10, reg_data);
		}
		if (i == 4)
			i = 10;
	}
	seq_printf(s, "\n");
	return 0;
}

static int sm5713_fuelgauge_debugfs_open(struct inode *inode, struct file *file)
{
    return single_open(file, sm5713_fuelgauge_debugfs_show, inode->i_private);
}

static const struct file_operations sm5713_fuelgauge_debugfs_fops = {
    .open           = sm5713_fuelgauge_debugfs_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

#ifdef CONFIG_OF
#define PROPERTY_NAME_SIZE 128

#define PINFO(format, args...) \
	printk(KERN_INFO "%s() line-%d: " format, \
		__func__, __LINE__, ## args)

#if defined(ENABLE_BATT_LONG_LIFE)
static int temp_parse_dt(struct sm5713_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "battery");
	int len = 0, ret;
	const u32 *p;

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		p = of_get_property(np, "battery,age_data", &len);
		if (p) {
			fuelgauge->pdata->num_age_step = len / sizeof(sec_age_data_t);
			fuelgauge->pdata->age_data = kzalloc(len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,age_data",
					 (u32 *)fuelgauge->pdata->age_data, len/sizeof(u32));
			if (ret) {
				pr_err("%s failed to read battery->pdata->age_data: %d\n",
						__func__, ret);
				kfree(fuelgauge->pdata->age_data);
				fuelgauge->pdata->age_data = NULL;
				fuelgauge->pdata->num_age_step = 0;
			}
			pr_info("%s num_age_step : %d\n", __func__, fuelgauge->pdata->num_age_step);
			for (len = 0; len < fuelgauge->pdata->num_age_step; ++len) {
				pr_info("[%d/%d]cycle:%d, float:%d, full_v:%d, recharge_v:%d, soc:%d\n",
					len, fuelgauge->pdata->num_age_step-1,
					fuelgauge->pdata->age_data[len].cycle,
					fuelgauge->pdata->age_data[len].float_voltage,
					fuelgauge->pdata->age_data[len].full_condition_vcell,
					fuelgauge->pdata->age_data[len].recharge_condition_vcell,
					fuelgauge->pdata->age_data[len].full_condition_soc);
			}
		} else {
			fuelgauge->pdata->num_age_step = 0;
			pr_err("%s there is not age_data\n", __func__);
		}
	}
	return 0;
}
#endif

static int sm5713_fuelgauge_parse_dt(struct sm5713_fuelgauge_data *fuelgauge)
{
	char prop_name[PROPERTY_NAME_SIZE];
	int battery_id = -1;
	int table[24];
#ifdef ENABLE_BATT_LONG_LIFE
	int v_max_table[5];
	int q_max_table[5];
#endif
	int rce_value[3];
	int rs_value[5];
	int mix_value[2];
	int battery_type[3];
	int v_alarm[2];
	int topoff_soc[3];
	int cycle_cfg[3];
	int v_offset_cancel[4];
	int temp_volcal[3];
	int temp_offset[6];
	int temp_cal[10];
	int volt_cal[2];
	int ext_temp_cal[10];
	int set_temp_poff[4];
	int curr_offset[7];
	int curr_cal[6];
	int arsm[4];
	int tcal_ioff[2];
#ifdef ENABLE_FULL_OFFSET
	int full_offset[5];
#endif

	int ret;
	int i, j;
	const u32 *p;
	int len;

	struct device_node *np = of_find_node_by_name(NULL, "sm5713-fuelgauge");

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0) {
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);
			fuelgauge->pdata->capacity_max_margin = 300;
		}

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		pr_info("%s: capacity_max: %d, capacity_max_margin: %d, capacity_min: %d\n",
			__func__, fuelgauge->pdata->capacity_max,
			fuelgauge->pdata->capacity_max_margin, fuelgauge->pdata->capacity_min);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);
		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		fuelgauge->pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgaguge,repeated_fuelalert");

		pr_info("%s: "
				"calculation_type: 0x%x, fuel_alert_soc: %d,\n"
				"repeated_fuelalert: %d\n", __func__,
				fuelgauge->pdata->capacity_calculation_type,
				fuelgauge->pdata->fuel_alert_soc, fuelgauge->pdata->repeated_fuelalert);


		fuelgauge->using_temp_compensation = of_property_read_bool(np,
							"fuelgauge,using_temp_compensation");
		if (fuelgauge->using_temp_compensation) {
			ret = of_property_read_u32(np, "fuelgauge,low_temp_limit",
						&fuelgauge->low_temp_limit);
			if (ret < 0)
				pr_err("%s error reading low temp limit %d\n", __func__, ret);

			pr_info("%s : LOW TEMP LIMIT(%d)\n",
				__func__, fuelgauge->low_temp_limit);
		}

		fuelgauge->using_hw_vempty = of_property_read_bool(np,
									"fuelgauge,using_hw_vempty");
		if (fuelgauge->using_hw_vempty) {
			ret = of_property_read_u32(np, "fuelgauge,v_empty",
						&fuelgauge->battery_data->V_empty);
			if (ret < 0)
				pr_err("%s error reading v_empty %d\n",
						__func__, ret);

			ret = of_property_read_u32(np, "fuelgauge,v_empty_origin",
						&fuelgauge->battery_data->V_empty_origin);
			if (ret < 0)
				pr_err("%s error reading v_empty_origin %d\n",
						__func__, ret);

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_voltage_cisd",
					&fuelgauge->battery_data->sw_v_empty_vol_cisd);
			if (ret < 0) {
				pr_err("%s error reading sw_v_empty_default_vol_cise %d\n",
						__func__, ret);
				fuelgauge->battery_data->sw_v_empty_vol_cisd = 3100;
			}

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_voltage",
						   &fuelgauge->battery_data->sw_v_empty_vol);
			if (ret < 0) {
				pr_err("%s error reading sw_v_empty_default_vol %d\n",
					   __func__, ret);
				fuelgauge->battery_data->sw_v_empty_vol	= 3200;
			}

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_recover_voltage",
						   &fuelgauge->battery_data->sw_v_empty_recover_vol);
			if (ret < 0) {
				pr_err("%s error reading sw_v_empty_recover_vol %d\n",
					   __func__, ret);
				fuelgauge->battery_data->sw_v_empty_recover_vol = 3480;
			}

			ret = of_property_read_u32(np, "fuelgauge,vbat_ovp",
						   &fuelgauge->battery_data->vbat_ovp);
			if (ret < 0) {
				pr_err("%s error reading vbat_ovp %d\n",
					   __func__, ret);
				fuelgauge->battery_data->vbat_ovp = 4400;
			}

			pr_info("%s : SW V Empty (%d)mV,  SW V Empty recover (%d)mV, CISD V_Alarm (%d)mV, Vbat OVP (%d)mV \n",
				__func__,
				fuelgauge->battery_data->sw_v_empty_vol,
				fuelgauge->battery_data->sw_v_empty_recover_vol,
				fuelgauge->battery_data->sw_v_empty_vol_cisd,
				fuelgauge->battery_data->vbat_ovp);
		}

		fuelgauge->pdata->jig_gpio = of_get_named_gpio(np, "fuelgauge,jig_gpio", 0);
		if (fuelgauge->pdata->jig_gpio < 0) {
			pr_err("%s error reading jig_gpio = %d\n",
					__func__, fuelgauge->pdata->jig_gpio);
			fuelgauge->pdata->jig_gpio = 0;
		} else {
			fuelgauge->pdata->jig_irq = gpio_to_irq(fuelgauge->pdata->jig_gpio);
		}
/*
		if (fuelgauge->pdata->jig_gpio) {
			ret = of_property_read_u32(np, "fuelgauge,jig_low_active",
						&fuelgauge->pdata->jig_low_active);

			if (ret < 0) {
				pr_err("%s error reading jig_low_active %d\n", __func__, ret);
				fuelgauge->pdata->jig_low_active = 0;
			}
		}
*/
		ret = of_property_read_u32(np, "fuelgauge,fg_resistor",
				&fuelgauge->fg_resistor);
		if (ret < 0) {
			pr_err("%s error reading fg_resistor %d\n",
					__func__, ret);
			fuelgauge->fg_resistor = 1;
		}


#if defined(CONFIG_EN_OOPS)
		/* todo cap redesign */
#endif

		ret = of_property_read_u32(np, "fuelgauge,capacity",
						&fuelgauge->battery_data->Capacity);
		if (ret < 0)
			pr_err("%s error reading Capacity %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,ttf_capacity",
						&fuelgauge->ttf_capacity);
		if (ret < 0)
			pr_err("%s error reading ttf Capacity %d\n",
					__func__, ret);

		p = of_get_property(np, "fuelgauge,cv_data", &len);
		if (p) {
			fuelgauge->cv_data = kzalloc(len, GFP_KERNEL);
			fuelgauge->cv_data_lenth = len / sizeof(struct cv_slope);
			pr_err("%s len: %ld, lenth: %d, %d\n",
						__func__, sizeof(int) * len, len, fuelgauge->cv_data_lenth);
			ret = of_property_read_u32_array(np, "fuelgauge,cv_data",
						(u32 *)fuelgauge->cv_data, len/sizeof(u32));

			if (ret) {
				pr_err("%s failed to read fuelgauge->cv_data: %d\n",
						__func__, ret);
				kfree(fuelgauge->cv_data);
				fuelgauge->cv_data = NULL;
			}
		} else {
			pr_err("%s there is not cv_data\n", __func__);
		}


#if defined(CONFIG_BATTERY_AGE_FORECAST)
		ret = of_property_read_u32(np, "battery,full_condition_soc",
			&fuelgauge->pdata->full_condition_soc);
		if (ret) {
			fuelgauge->pdata->full_condition_soc = 93;
			pr_info("%s : Full condition soc is Empty\n", __func__);
		}
#endif
	}

	/* get battery_params node for reg init */
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		PINFO("Cannot find child node \"battery_params\"\n");
		return -EINVAL;
	}

	/* get battery_id */
	if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
		PINFO("not battery,id property\n");
	PINFO("battery id = %d\n", battery_id);

	/* vbat measure point */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "is_read_vpack");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.is_read_vpack, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <0x%x>\n", prop_name, fuelgauge->info.is_read_vpack);

	/* get battery_table */
	for (i = DISCHARGE_TABLE; i < TABLE_MAX; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE,
			"battery%d,%s%d", battery_id, "battery_table", i);

		ret = of_property_read_u32_array(np, prop_name, table, 24);
		if (ret < 0) {
			PINFO("Can get prop %s (%d)\n", prop_name, ret);
		}
		for (j = 0; j <= FG_TABLE_LEN; j++) {
			fuelgauge->info.battery_table[i][j] = table[j];
			PINFO("%s = <table[%d][%d] 0x%x>\n", prop_name, i, j, table[j]);
		}
	}

	snprintf(prop_name, PROPERTY_NAME_SIZE,
			"battery%d,%s%d", battery_id, "battery_table", i);

	ret = of_property_read_u32_array(np, prop_name, table, 16);
	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	}
	for (j = 0; j <= FG_ADD_TABLE_LEN; j++) {
		fuelgauge->info.battery_table[i][j] = table[j];
		PINFO("%s = <table[%d][%d] 0x%x>\n", prop_name, i, j, table[j]);
	}

	/* get rce */
	for (i = 0; i < 3; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "rce_value");
		ret = of_property_read_u32_array(np, prop_name, rce_value, 3);
		if (ret < 0) {
			PINFO("Can get prop %s (%d)\n", prop_name, ret);
		}
		fuelgauge->info.rce_value[i] = rce_value[i];
	}
	PINFO("%s = <0x%x 0x%x 0x%x>\n", prop_name, rce_value[0], rce_value[1], rce_value[2]);

	/* get dtcd_value */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "dtcd_value");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.dtcd_value, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <0x%x>\n", prop_name, fuelgauge->info.dtcd_value);

	/* get rs_value */
	for (i = 0; i < 5; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "rs_value");
		ret = of_property_read_u32_array(np, prop_name, rs_value, 5);
		if (ret < 0) {
			PINFO("Can get prop %s (%d)\n", prop_name, ret);
		}
		fuelgauge->info.rs_value[i] = rs_value[i];
	}
	PINFO("%s = <0x%x 0x%x 0x%x 0x%x 0x%x>\n", prop_name, rs_value[0], rs_value[1], rs_value[2], rs_value[3], rs_value[4]);

	/* get vit_period */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "vit_period");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.vit_period, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <0x%x>\n", prop_name, fuelgauge->info.vit_period);

	/* get mix_value */
	for (i = 0; i < 2; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "mix_value");
		ret = of_property_read_u32_array(np, prop_name, mix_value, 2);
		if (ret < 0) {
			PINFO("Can get prop %s (%d)\n", prop_name, ret);
		}
		fuelgauge->info.mix_value[i] = mix_value[i];
	}
	PINFO("%s = <0x%x 0x%x>\n", prop_name, mix_value[0], mix_value[1]);

	/* battery_type */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_type");
	ret = of_property_read_u32_array(np, prop_name, battery_type, 3);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.batt_v_max = battery_type[0];
	fuelgauge->info.min_cap = battery_type[1];
	fuelgauge->info.cap = battery_type[2];
	fuelgauge->info.maxcap = battery_type[2];
	if (fuelgauge->battery_data->Capacity == 0)
		fuelgauge->battery_data->Capacity = fuelgauge->info.maxcap / 2;

	PINFO("%s = <%d %d 0x%x>\n", prop_name,
		fuelgauge->info.batt_v_max, fuelgauge->info.min_cap, fuelgauge->info.cap);

#ifdef ENABLE_BATT_LONG_LIFE
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_max_table");
	ret = of_property_read_u32_array(np, prop_name, v_max_table, fuelgauge->pdata->num_age_step);

	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);

		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.v_max_table[i] = fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1];
			PINFO("%s = <v_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.v_max_table[i]);
		}
	} else {
		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.v_max_table[i] = v_max_table[i];
			PINFO("%s = <v_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.v_max_table[i]);
		}
	}

	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "q_max_table");
	ret = of_property_read_u32_array(np, prop_name, q_max_table, fuelgauge->pdata->num_age_step);

	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);

		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.q_max_table[i] = fuelgauge->info.cap;
			PINFO("%s = <q_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.q_max_table[i]);
		}
	} else {
		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.q_max_table[i] = q_max_table[i];
			PINFO("%s = <q_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.q_max_table[i]);
		}
	}
	fuelgauge->chg_full_soc = fuelgauge->pdata->age_data[0].full_condition_soc;
	fuelgauge->info.v_max_now = fuelgauge->info.v_max_table[0];
	fuelgauge->info.q_max_now = fuelgauge->info.q_max_table[0];
	PINFO("%s = <v_max_now = 0x%x>, <q_max_now = 0x%x>, <chg_full_soc = %d>\n", prop_name, fuelgauge->info.v_max_now, fuelgauge->info.q_max_now, fuelgauge->chg_full_soc);
#endif

	/* MISC */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "misc");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.misc, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <0x%x>\n", prop_name, fuelgauge->info.misc);

	/* V_ALARM */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_alarm");
	ret = of_property_read_u32_array(np, prop_name, v_alarm, 2);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.value_v_alarm = v_alarm[0];
	fuelgauge->info.value_v_alarm_hys = v_alarm[1];
	PINFO("%s = <%d %d>\n", prop_name, fuelgauge->info.value_v_alarm, fuelgauge->info.value_v_alarm_hys);

    /* TOP OFF SOC */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "topoff_soc");
	ret = of_property_read_u32_array(np, prop_name, topoff_soc, 3);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.enable_topoff_soc = topoff_soc[0];
	fuelgauge->info.topoff_soc = topoff_soc[1];
	fuelgauge->info.top_off = topoff_soc[2];

	PINFO("%s = <%d %d %d>\n", prop_name,
		fuelgauge->info.enable_topoff_soc, fuelgauge->info.topoff_soc, fuelgauge->info.top_off);

	/* SOC cycle cfg */
	snprintf (prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "cycle_cfg");
	ret = of_property_read_u32_array(np, prop_name, cycle_cfg, 3);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.cycle_high_limit = cycle_cfg[0];
	fuelgauge->info.cycle_low_limit = cycle_cfg[1];
	fuelgauge->info.cycle_limit_cntl = cycle_cfg[2];

	PINFO("%s = <%d %d %d>\n", prop_name,
		fuelgauge->info.cycle_high_limit, fuelgauge->info.cycle_low_limit, fuelgauge->info.cycle_limit_cntl);

	/* v_offset_cancel */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_offset_cancel");
	ret = of_property_read_u32_array(np, prop_name, v_offset_cancel, 4);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.enable_v_offset_cancel_p = v_offset_cancel[0];
	fuelgauge->info.enable_v_offset_cancel_n = v_offset_cancel[1];
	fuelgauge->info.v_offset_cancel_level = v_offset_cancel[2];
	fuelgauge->info.v_offset_cancel_mohm = v_offset_cancel[3];

	PINFO("%s = <%d %d %d %d>\n", prop_name,
		fuelgauge->info.enable_v_offset_cancel_p, fuelgauge->info.enable_v_offset_cancel_n, fuelgauge->info.v_offset_cancel_level, fuelgauge->info.v_offset_cancel_mohm);

	/* VOL & CURR CAL */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "volt_cal");
	ret = of_property_read_u32_array(np, prop_name, volt_cal, 2);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.volt_cal[0] = volt_cal[0];
	fuelgauge->info.volt_cal[1] = volt_cal[1];
	PINFO("%s = <0x%x 0x%x>\n", prop_name, fuelgauge->info.volt_cal[0], fuelgauge->info.volt_cal[1]);

	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_offset");
	ret = of_property_read_u32_array(np, prop_name, curr_offset, 7);
	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	} else {
		fuelgauge->info.en_auto_i_offset = curr_offset[0];
		fuelgauge->info.ecv_i_off = curr_offset[1];
		fuelgauge->info.csp_i_off = curr_offset[2];
		fuelgauge->info.csn_i_off = curr_offset[3];
		fuelgauge->info.dp_ecv_i_off = curr_offset[4];
		fuelgauge->info.dp_csp_i_off = curr_offset[5];
		fuelgauge->info.dp_csn_i_off = curr_offset[6];
	}
	PINFO("%s = <%d arg : 0x%x 0x%x 0x%x, dp : 0x%x 0x%x 0x%x>\n", prop_name,
		fuelgauge->info.en_auto_i_offset, fuelgauge->info.ecv_i_off, fuelgauge->info.csp_i_off, fuelgauge->info.csn_i_off,
		fuelgauge->info.dp_ecv_i_off, fuelgauge->info.dp_csp_i_off, fuelgauge->info.dp_csn_i_off);

	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_cal");
	ret = of_property_read_u32_array(np, prop_name, curr_cal, 6);
	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	} else {
		fuelgauge->info.ecv_i_slo = curr_cal[0];
		fuelgauge->info.csp_i_slo = curr_cal[1];
		fuelgauge->info.csn_i_slo = curr_cal[2];
		fuelgauge->info.dp_ecv_i_slo = curr_cal[3];
		fuelgauge->info.dp_csp_i_slo = curr_cal[4];
		fuelgauge->info.dp_csn_i_slo = curr_cal[5];
	}
	PINFO("%s = <arg : 0x%x 0x%x 0x%x, dp : 0x%x 0x%x 0x%x>\n", prop_name,
		fuelgauge->info.ecv_i_slo, fuelgauge->info.csp_i_slo, fuelgauge->info.csn_i_slo,
		fuelgauge->info.dp_ecv_i_slo, fuelgauge->info.dp_csp_i_slo, fuelgauge->info.dp_csn_i_slo);

#ifdef ENABLE_FULL_OFFSET
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "full_offset");
	ret = of_property_read_u32_array(np, prop_name, full_offset, 5);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.full_offset_enable = full_offset[0];
	fuelgauge->info.full_offset_margin = full_offset[1];
	fuelgauge->info.full_extra_offset = full_offset[2];
	fuelgauge->info.aux_stat_base = full_offset[3];
	fuelgauge->info.aux_stat_check = full_offset[4];

	PINFO("%s = <%d %d %d 0x%x 0x%x>\n", prop_name,
        fuelgauge->info.full_offset_enable,
        fuelgauge->info.full_offset_margin, fuelgauge->info.full_extra_offset,
        fuelgauge->info.aux_stat_base, fuelgauge->info.aux_stat_check);
#endif

	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "coeff");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.coeff, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <%x>\n", prop_name, fuelgauge->info.coeff);

	/* temp_std */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_std");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.temp_std, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <%d>\n", prop_name, fuelgauge->info.temp_std);

	/* temp_volcal */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_volcal");
	ret = of_property_read_u32_array(np, prop_name, temp_volcal, 3);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.en_fg_temp_volcal = temp_volcal[0];
	fuelgauge->info.fg_temp_volcal_denom = temp_volcal[1];
	fuelgauge->info.fg_temp_volcal_fact = temp_volcal[2];
	PINFO("%s = <%d, %d, %d>\n", prop_name,
		fuelgauge->info.en_fg_temp_volcal, fuelgauge->info.fg_temp_volcal_denom, fuelgauge->info.fg_temp_volcal_fact);

	/* temp_offset */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_offset");
	ret = of_property_read_u32_array(np, prop_name, temp_offset, 6);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.en_high_fg_temp_offset = temp_offset[0];
	fuelgauge->info.high_fg_temp_offset_denom = temp_offset[1];
	fuelgauge->info.high_fg_temp_offset_fact = temp_offset[2];
	fuelgauge->info.en_low_fg_temp_offset = temp_offset[3];
	fuelgauge->info.low_fg_temp_offset_denom = temp_offset[4];
	fuelgauge->info.low_fg_temp_offset_fact = temp_offset[5];
	PINFO("%s = <%d, %d, %d, %d, %d, %d>\n", prop_name,
		fuelgauge->info.en_high_fg_temp_offset,
		fuelgauge->info.high_fg_temp_offset_denom, fuelgauge->info.high_fg_temp_offset_fact,
		fuelgauge->info.en_low_fg_temp_offset,
		fuelgauge->info.low_fg_temp_offset_denom, fuelgauge->info.low_fg_temp_offset_fact);

	/* temp_calc */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_cal");
	ret = of_property_read_u32_array(np, prop_name, temp_cal, 10);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.en_high_fg_temp_cal = temp_cal[0];
	fuelgauge->info.high_fg_temp_p_cal_denom = temp_cal[1];
	fuelgauge->info.high_fg_temp_p_cal_fact = temp_cal[2];
	fuelgauge->info.high_fg_temp_n_cal_denom = temp_cal[3];
	fuelgauge->info.high_fg_temp_n_cal_fact = temp_cal[4];
	fuelgauge->info.en_low_fg_temp_cal = temp_cal[5];
	fuelgauge->info.low_fg_temp_p_cal_denom = temp_cal[6];
	fuelgauge->info.low_fg_temp_p_cal_fact = temp_cal[7];
	fuelgauge->info.low_fg_temp_n_cal_denom = temp_cal[8];
	fuelgauge->info.low_fg_temp_n_cal_fact = temp_cal[9];
	PINFO("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
		fuelgauge->info.en_high_fg_temp_cal,
		fuelgauge->info.high_fg_temp_p_cal_denom, fuelgauge->info.high_fg_temp_p_cal_fact,
		fuelgauge->info.high_fg_temp_n_cal_denom, fuelgauge->info.high_fg_temp_n_cal_fact,
		fuelgauge->info.en_low_fg_temp_cal,
		fuelgauge->info.low_fg_temp_p_cal_denom, fuelgauge->info.low_fg_temp_p_cal_fact,
		fuelgauge->info.low_fg_temp_n_cal_denom, fuelgauge->info.low_fg_temp_n_cal_fact);

	/* ext_temp_calc */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "ext_temp_cal");
	ret = of_property_read_u32_array(np, prop_name, ext_temp_cal, 10);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.en_high_temp_cal = ext_temp_cal[0];
	fuelgauge->info.high_temp_p_cal_denom = ext_temp_cal[1];
	fuelgauge->info.high_temp_p_cal_fact = ext_temp_cal[2];
	fuelgauge->info.high_temp_n_cal_denom = ext_temp_cal[3];
	fuelgauge->info.high_temp_n_cal_fact = ext_temp_cal[4];
	fuelgauge->info.en_low_temp_cal = ext_temp_cal[5];
	fuelgauge->info.low_temp_p_cal_denom = ext_temp_cal[6];
	fuelgauge->info.low_temp_p_cal_fact = ext_temp_cal[7];
	fuelgauge->info.low_temp_n_cal_denom = ext_temp_cal[8];
	fuelgauge->info.low_temp_n_cal_fact = ext_temp_cal[9];
	PINFO("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
		fuelgauge->info.en_high_temp_cal,
		fuelgauge->info.high_temp_p_cal_denom, fuelgauge->info.high_temp_p_cal_fact,
		fuelgauge->info.high_temp_n_cal_denom, fuelgauge->info.high_temp_n_cal_fact,
		fuelgauge->info.en_low_temp_cal,
		fuelgauge->info.low_temp_p_cal_denom, fuelgauge->info.low_temp_p_cal_fact,
		fuelgauge->info.low_temp_n_cal_denom, fuelgauge->info.low_temp_n_cal_fact);

	/* tem poff level */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "tem_poff");
	ret = of_property_read_u32_array(np, prop_name, set_temp_poff, 4);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.n_tem_poff = set_temp_poff[0];
	fuelgauge->info.n_tem_poff_offset = set_temp_poff[1];
	fuelgauge->info.l_tem_poff = set_temp_poff[2];
	fuelgauge->info.l_tem_poff_offset = set_temp_poff[3];

	PINFO("%s = <%d, %d, %d, %d>\n",
		prop_name,
		fuelgauge->info.n_tem_poff, fuelgauge->info.n_tem_poff_offset,
		fuelgauge->info.l_tem_poff, fuelgauge->info.l_tem_poff_offset);

	/* arsm setting */
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "arsm");
	ret = of_property_read_u32_array(np, prop_name, arsm, 4);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.arsm[0] = arsm[0];
	fuelgauge->info.arsm[1] = arsm[1];
	fuelgauge->info.arsm[2] = arsm[2];
	fuelgauge->info.arsm[3] = arsm[3];

	PINFO("%s = <%d, %d, %d, %d>\n",
		prop_name,
		fuelgauge->info.arsm[0], fuelgauge->info.arsm[1],
		fuelgauge->info.arsm[2], fuelgauge->info.arsm[3]);

	/* age cntl value */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "age_cntl");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.age_cntl, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <0x%x>\n", prop_name, fuelgauge->info.age_cntl);

	/* tcal_ioff setting */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "tcal_ioff");
	ret = of_property_read_u32_array(np, prop_name, tcal_ioff, 2);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.tcal_ioff[0] = tcal_ioff[0];
	fuelgauge->info.tcal_ioff[1] = tcal_ioff[1];

	PINFO("%s = <0x%x, 0x%x>\n",
		prop_name,
		fuelgauge->info.tcal_ioff[0], fuelgauge->info.tcal_ioff[1]);

	/* batt data version */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "data_ver");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.data_ver, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <%d>\n", prop_name, fuelgauge->info.data_ver);




	return 0;
}
#endif

static const struct power_supply_desc sm5713_fuelgauge_power_supply_desc = {
	.name = "sm5713-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sm5713_fuelgauge_props,
	.num_properties = ARRAY_SIZE(sm5713_fuelgauge_props),
	.get_property = sm5713_fg_get_property,
	.set_property = sm5713_fg_set_property,
};

static int sm5713_fuelgauge_probe(struct platform_device *pdev)
{
	struct sm5713_dev *sm5713 = dev_get_drvdata(pdev->dev.parent);
	struct sm5713_platform_data *pdata = dev_get_platdata(sm5713->dev);
	struct sm5713_fuelgauge_data *fuelgauge;
	sec_fuelgauge_platform_data_t *fuelgauge_data;
	/* struct power_supply_config fuelgauge_cfg = {}; */
	struct power_supply_config psy_fg = {};
	int ret = 0;
	union power_supply_propval raw_soc_val;

	pr_info("%s: SM5713 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	fuelgauge_data = kzalloc(sizeof(sec_fuelgauge_platform_data_t), GFP_KERNEL);
	if (!fuelgauge_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->pdata = fuelgauge_data;
	fuelgauge->i2c = sm5713->fuelgauge;
	/* fuelgauge->pmic = sm5713->i2c; */
	fuelgauge->sm5713_pdata = pdata;

	fuelgauge->pmic_rev = sm5713->pmic_rev;
	fuelgauge->vender_id = sm5713->vender_id;

#if defined(ENABLE_BATT_LONG_LIFE)
	temp_parse_dt(fuelgauge);
#endif

#if defined(CONFIG_OF)
	fuelgauge->battery_data = kzalloc(sizeof(struct battery_data_t),
					GFP_KERNEL);
	if (!fuelgauge->battery_data) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_pdata_free;
	}
	ret = sm5713_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0) {
		pr_err("%s not found charger dt! ret[%d]\n",
				__func__, ret);
	}
#endif

	/* initialize value */
	fuelgauge->isjigmoderealvbat = false;

	platform_set_drvdata(pdev, fuelgauge);

	(void) debugfs_create_file("sm5713-fuelgauge-regs",
		S_IRUGO, NULL, (void *)fuelgauge, &sm5713_fuelgauge_debugfs_fops);

	if (!sm5713_fg_init(fuelgauge, false)) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
		goto err_data_free;
	}

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	raw_soc_val.intval = sm5713_get_soc(fuelgauge);

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		sm5713_fg_calculate_dynamic_scale(fuelgauge, 100);

	/* SW/HW init code. SW/HW V Empty mode must be opposite ! */
	fuelgauge->info.temperature = 300; /* default value */
	pr_info("%s: SW/HW V empty init \n", __func__);
	sm5713_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);

	psy_fg.drv_data = fuelgauge;
	psy_fg.supplied_to = sm5713_fg_supplied_to;
	psy_fg.num_supplicants = ARRAY_SIZE(sm5713_fg_supplied_to),

	fuelgauge->psy_fg = power_supply_register(&pdev->dev, &sm5713_fuelgauge_power_supply_desc, &psy_fg);
	if (!fuelgauge->psy_fg) {
		dev_err(&pdev->dev, "%s: failed to power supply fg register", __func__);
		goto err_data_free;
	}
/*	
	fuelgauge->psy_fg.desc->name			= "sm5713-fuelgauge";
	fuelgauge->psy_fg.desc->type			= POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.desc->get_property	= sm5713_fg_get_property;
	fuelgauge->psy_fg.desc->set_property	= sm5713_fg_set_property;
	fuelgauge->psy_fg.desc->properties		= sm5713_fuelgauge_props;
	fuelgauge->psy_fg.desc->num_properties	= ARRAY_SIZE(sm5713_fuelgauge_props);

	ret = power_supply_register(&pdev->dev, &fuelgauge->psy_fg);

	if (ret) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		goto err_data_free;
	}
*/
	fuelgauge->fg_irq = pdata->irq_base + SM5713_FG_IRQ_INT_LOW_VOLTAGE;
	pr_info("[%s]IRQ_BASE(%d) FG_IRQ(%d)\n",
		__func__, pdata->irq_base, fuelgauge->fg_irq);

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		if (sm5713_fg_fuelalert_init(fuelgauge,
					fuelgauge->pdata->fuel_alert_soc)) {
			wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
					WAKE_LOCK_SUSPEND, "fuel_alerted");
			if (fuelgauge->fg_irq) {
				INIT_DELAYED_WORK(&fuelgauge->isr_work, sm5713_fg_isr_work);

				ret = request_threaded_irq(fuelgauge->fg_irq,
						NULL, sm5713_fg_irq_thread,
						0,
						"fuelgauge-irq", fuelgauge);
				if (ret) {
					pr_err("%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}
			}
		} else {
			pr_err("%s: Failed to Initialize Fuel-alert\n",
					__func__);
			goto err_supply_unreg;
		}
	}

	fuelgauge->initial_update_of_soc = true;

	pr_info("%s: SM5713 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_supply_unreg:
	power_supply_unregister(fuelgauge->psy_fg);
err_data_free:
#if defined(CONFIG_OF)
	kfree(fuelgauge->battery_data);
#endif
err_pdata_free:
	kfree(fuelgauge_data);
	mutex_destroy(&fuelgauge->fg_lock);
err_free:
	kfree(fuelgauge);

	return ret;
}

static int sm5713_fuelgauge_remove(struct platform_device *pdev)
{
	struct sm5713_fuelgauge_data *fuelgauge =
		platform_get_drvdata(pdev);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

static int sm5713_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int sm5713_fuelgauge_resume(struct device *dev)
{
	struct sm5713_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->initial_update_of_soc = true;

	return 0;
}

static void sm5713_fuelgauge_shutdown(struct platform_device *pdev)
{
	struct sm5713_fuelgauge_data *fuelgauge = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	if (fuelgauge->using_hw_vempty)
		sm5713_fg_set_vempty(fuelgauge, false);
#ifdef ENABLE_SM5713_MQ_FUNCTION
	sm5713_meas_mq_suspend(fuelgauge);
#endif
	sm5713_write_word(fuelgauge->i2c, SM5713_FG_REG_MIX_RATE, 0x0F03);

	pr_info("%s: --\n", __func__);
}

static SIMPLE_DEV_PM_OPS(sm5713_fuelgauge_pm_ops, sm5713_fuelgauge_suspend,
			sm5713_fuelgauge_resume);

static struct platform_driver sm5713_fuelgauge_driver = {
	.driver = {
			.name = "sm5713-fuelgauge",
			.owner = THIS_MODULE,
#ifdef CONFIG_PM
			.pm = &sm5713_fuelgauge_pm_ops,
#endif
	},
	.probe  = sm5713_fuelgauge_probe,
	.remove = sm5713_fuelgauge_remove,
	.shutdown = sm5713_fuelgauge_shutdown,
};

static int __init sm5713_fuelgauge_init(void)
{
	pr_info("%s: \n", __func__);
	return platform_driver_register(&sm5713_fuelgauge_driver);
}

static void __exit sm5713_fuelgauge_exit(void)
{
	platform_driver_unregister(&sm5713_fuelgauge_driver);
}
module_init(sm5713_fuelgauge_init);
module_exit(sm5713_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung SM5713 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
