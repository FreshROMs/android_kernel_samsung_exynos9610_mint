/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SEC_UTIL_H
#define FIMC_IS_SEC_UTIL_H

#include <linux/i2c.h>

#define I2C_WRITE 3
#define I2C_BYTE  2
#define I2C_DATA  1
#define I2C_ADDR  0

enum i2c_write {
	I2C_WRITE_ADDR8_DATA8 = 0x0,
	I2C_WRITE_ADDR16_DATA8,
	I2C_WRITE_ADDR16_DATA16
};

int fimc_is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size);
int fimc_is_i2c_write(struct i2c_client *client, u16 addr, u8 data);
int fimc_is_i2c_config(struct i2c_client *client, bool onoff);
#if defined(SENSOR_OTP_HYNIX)
int fimc_is_i2c_read_burst(struct i2c_client *client, void *buf, u32 addr, size_t size);
#endif

int fimc_is_sec_set_registers(struct i2c_client *client, const u32 *regs, const u32 size);
bool fimc_is_sec_file_exist(char *name);

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t write_data_to_file_append(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos);
	
#endif //FIMC_IS_SEC_UTIL_H
