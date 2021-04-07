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

#ifdef CONFIG_ISDBT_GPIO_CLK
#include <linux/pinctrl/consumer.h>
#endif

struct isdbt_platform_data {
	int	irq;
	int gpio_en;
	int gpio_rst;
	int gpio_int;
	int gpio_i2c_sda;
	int gpio_i2c_scl;
	int gpio_cp_dt;
	struct clk *isdbt_clk;
	int gpio_spi_do;
	int gpio_spi_di;
	int gpio_spi_cs;
	int gpio_spi_clk;
	const char *ldo_vdd_1p8;
	u8 regulator_is_enable;
	int BER;
#ifdef CONFIG_ISDBT_GPIO_CLK
	int gpio_clk;
	struct pinctrl *isdbt_pinctrl;
	struct pinctrl_state *isdbt_on;
	struct pinctrl_state *isdbt_off;
#endif
};
#endif
