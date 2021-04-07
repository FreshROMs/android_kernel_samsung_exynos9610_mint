/*
 * sm5713-charger.h - header file of SM5713 Charger device driver
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __SM5713_CHARGER_H__
#define __SM5713_CHARGER_H__

#include <linux/mfd/sm5713.h>
#include <linux/mfd/sm5713-private.h>

enum {
	CHIP_ID = 0,
	EN_BYPASS_MODE = 1,
};

ssize_t sm5713_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sm5713_chg_store_attrs(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count);

#define SM5713_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = sm5713_chg_show_attrs,			    \
	.store = sm5713_chg_store_attrs,			\
}

enum {
	AICL_TH_V_4_3   = 0x0,
	AICL_TH_V_4_4   = 0x1,
	AICL_TH_V_4_5   = 0x2,
	AICL_TH_V_4_6   = 0x3,
};

enum {
	DISCHG_LIMIT_C_3_5  = 0x0,
	DISCHG_LIMIT_C_4_5  = 0x1,
	DISCHG_LIMIT_C_5_0  = 0x2,
	DISCHG_LIMIT_C_6_0  = 0x3,
};

enum {
	BST_IQ3LIMIT_C_2_0  = 0x0,
	BST_IQ3LIMIT_C_2_8  = 0x1,
	BST_IQ3LIMIT_C_3_5  = 0x2,
	BST_IQ3LIMIT_C_4_0  = 0x3,
};

enum {
	WDT_TIME_S_30   = 0x0,
	WDT_TIME_S_60   = 0x1,
	WDT_TIME_S_90   = 0x2,
	WDT_TIME_S_120  = 0x3,
};

enum {
	TOPOFF_TIME_M_10 = 0x0,
	TOPOFF_TIME_M_20 = 0x1,
	TOPOFF_TIME_M_30 = 0x2,
	TOPOFF_TIME_M_45 = 0x3,
};


struct sm5713_charger_platform_data {
	int chg_float_voltage;
	unsigned int chg_ocp_current;
};

#define REDUCE_CURRENT_STEP						100
#define MINIMUM_INPUT_CURRENT					300
#define SLOW_CHARGING_CURRENT_STANDARD          400

struct sm5713_charger_data {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex charger_mutex;

	struct sm5713_charger_platform_data *pdata;
	struct power_supply	*psy_chg;
	struct power_supply	*psy_otg;

	int status;
	int cable_type;
	int input_current;
	int charging_current;
	int topoff_current;
	int float_voltage;
	int charge_mode;
	int unhealth_cnt;
	bool is_charging;
	bool otg_on;

	/* sm5713 Charger-IRQs */
	int irq_vbuspok;
	int irq_aicl;
	int irq_aicl_enabled;
	int irq_vsysovp;
	int irq_otgfail;
	int irq_vbusshort;
	int irq_batovp;
	int irq_done;
	int irq_vbusuvlo;

	int pmic_rev;

	/* for slow-rate-charging noti */
	bool slow_rate_chg_mode;
	struct workqueue_struct *wqueue;
	struct delayed_work aicl_work;
	struct wake_lock aicl_wake_lock;
};

#endif  /* __SM5713_CHARGER_H__ */
