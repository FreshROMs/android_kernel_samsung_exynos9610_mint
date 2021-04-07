/*
 *  sm5713.h - mfd driver for SM5713.
 *
 *  Copyright (C) 2017 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5713_H__
#define __SM5713_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MFD_DEV_NAME "sm5713"

struct sm5713_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct sm5713_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;

	/* USBLDO */
	struct sm5713_regulator_data *regulators;
	int num_regulators;

	struct mfd_cell *sub_devices;
	int num_subdevs;
};

/* For SM5713 Flash LED */
enum sm5713_fled_mode {
	SM5713_FLED_MODE_OFF = 1,
	SM5713_FLED_MODE_MAIN_FLASH,
	SM5713_FLED_MODE_TORCH_FLASH,
	SM5713_FLED_MODE_PREPARE_FLASH,
	SM5713_FLED_MODE_CLOSE_FLASH,
	SM5713_FLED_MODE_PRE_FLASH,
};

enum {
	FLED_MODE_OFF       = 0x0,
	FLED_MODE_TORCH     = 0x1,
	FLED_MODE_FLASH     = 0x2,
	FLED_MODE_EXTERNAL  = 0x3,
};

enum {
	SM5713_FLED_INDEX_1 = 0,
	SM5713_FLED_INDEX_2 = 1,
	SM5713_FLED_INDEX_3 = 2,	
	SM5713_FLED_MAX_NUM,
};

struct sm5713_fled_platform_data {
	struct {
		const char *name;
		u8 flash_brightness;
		u8 preflash_brightness;
		u8 torch_brightness;
		u8 timeout;

		int fen_pin;            /* GPIO-pin for Flash */
		int men_pin;            /* GPIO-pin for Torch */

		bool used_gpio_ctrl;
		int sysfs_input_data; //ys1978

		bool pre_fled;
		bool en_fled;
		bool en_mled;
	} led[SM5713_FLED_MAX_NUM];
};

struct sm5713_fled_data {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex fled_mutex;

	struct sm5713_fled_platform_data *pdata;
	struct device *rear_fled_dev;

	int vbus_voltage;
	u8 torch_on_cnt;
	u8 flash_on_cnt;
	u8 flash_prepare_cnt;
};

extern int32_t sm5713_fled_mode_ctrl(u8 fled_index, int state);

#endif /* __SM5713_H__ */

