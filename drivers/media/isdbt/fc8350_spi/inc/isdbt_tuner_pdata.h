/*
 * Copyright (C) (2011, Samsung Electronics)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ISDBT_TUNER_PDATA_H_
#define _ISDBT_TUNER_PDATA_H_

struct isdbt_platform_data {
	int	irq;
	int gpio_en;
	int gpio_power_en;
	int gpio_rst;
	int gpio_lna_en;
	int gpio_int;
	int gpio_i2c_sda;
	int gpio_i2c_scl;
	int gpio_cp_dt;
	int gpio_ant_ctrl;
	int gpio_b28_ctrl;
	int gpio_ant_ctrl1;
	int gpio_ant_ctrl2;
	int gpio_dtv_check;
	u32 gpio_ant_ctrl_sel[6];
	struct clk *isdbt_clk;
#ifdef CONFIG_ISDBT_F_TYPE_ANTENNA
	int gpio_tmm_sw;
#endif
	int gpio_spi_do;
	int gpio_spi_di;
	int gpio_spi_cs;
	int gpio_spi_clk;
	const char *ldo_vdd_1p8;
	u8 regulator_is_enable;
	int BER[2];
	int type;
#if defined(CONFIG_SEC_GPIO_SETTINGS)
	struct device  *isdbt_device;
#endif
};
#endif
