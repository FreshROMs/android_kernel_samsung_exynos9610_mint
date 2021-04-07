/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_VENDER_SPECIFIC_H
#define FIMC_IS_VENDER_SPECIFIC_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>

#ifdef CONFIG_COMPANION_DCDC_USE
#include "fimc-is-pmic.h"
#endif

typedef enum FRomPowersource{
    FROM_POWER_SOURCE_REAR	= 0,  /*wide*/
    FROM_POWER_SOURCE_REAR_SECOND /*tele*/
} FRomPowersource;

/* #define USE_ION_ALLOC */
#define FIMC_IS_COMPANION_CRC_SIZE	4
#define I2C_RETRY_COUNT			5

#ifdef CONFIG_PREPROCESSOR_STANDBY_USE
#define SENSOR_STATE_OFF		0x0
#define SENSOR_STATE_ON			0x1
#define SENSOR_STATE_STANDBY		0x2
#define SENSOR_STATE_UNKNOWN		0x3
#define SENSOR_MASK			0x3
#define SENSOR_STATE_SENSOR		0
#define SENSOR_STATE_COMPANION		2

#define SET_SENSOR_STATE(state, sensor, value) \
	(state = (state & ~(SENSOR_MASK << sensor)) | value << sensor)
#define GET_SENSOR_STATE(state, sensor) \
	(((state & (SENSOR_MASK << sensor)) >> sensor) & SENSOR_MASK)
#endif

struct fimc_is_companion_retention {
	int firmware_size;
	char firmware_crc32[FIMC_IS_COMPANION_CRC_SIZE];
};

struct fimc_is_vender_specific {
#ifdef USE_ION_ALLOC
	struct ion_client	*fimc_ion_client;
#endif
	struct mutex		spi_lock;
#ifdef CONFIG_COMPANION_DCDC_USE
	struct dcdc_power	companion_dcdc;
#endif
#ifdef CONFIG_OIS_USE
	bool			ois_ver_read;
#endif /* CONFIG_OIS_USE */

	struct i2c_client	*eeprom_client[SENSOR_POSITION_END];

#if defined(CONFIG_USE_DIRECT_IS_CONTROL) && defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	struct i2c_client	*front_cis_client;
#endif
#if defined(CONFIG_USE_DIRECT_IS_CONTROL) && defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
	struct i2c_client	*rear_cis_client;
#endif

	bool			running_rear_camera;
	bool			running_front_camera;
	bool			running_rear_second_camera;
	bool			running_front_second_camera;

	char			*comp_int_pin; /* Companion PAF INT */
	char			*comp_int_pinctrl;
	u8			standby_state;
	struct fimc_is_companion_retention	retention_data;
#ifdef CONFIG_SENSOR_RETENTION_USE
	bool			need_retention_init;
#endif

	/* dt */
	u32			rear_sensor_id;
	u32			front_sensor_id;
	u32			rear2_sensor_id;
	u32			rear3_sensor_id;
	u32			front2_sensor_id;
#ifdef CONFIG_SECURE_CAMERA_USE
	u32			secure_sensor_id;
#endif
	bool			check_sensor_vendor;
	bool			skip_cal_loading;
	bool			use_ois_hsi2c;
	bool			use_ois;
	bool			use_module_check;
	bool			need_i2c_config;

	bool			suspend_resume_disable;
	bool			need_cold_reset;
	bool			zoom_running;
	FRomPowersource		f_rom_power;
};

#endif
