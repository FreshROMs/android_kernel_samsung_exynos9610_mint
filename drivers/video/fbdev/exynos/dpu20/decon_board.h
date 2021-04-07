/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DECON_BOARD_H__
#define __DECON_BOARD_H__

#include <linux/device.h>

#if defined(CONFIG_EXYNOS_DPU20)
extern unsigned int lcdtype;
#elif defined(CONFIG_EXYNOS_DPU30)
extern int boot_panel_id;
#endif

extern void run_list(struct device *dev, const char *name);
extern void run_action(struct device *dev, const char *name, const char *type, const char *subinfo);
extern void run_action_list(struct device *dev, const char *name, const char **type_list);
extern void run_timer_from(struct device *dev, const char *name, const char *timer_name, unsigned int ms);
extern void run_timer_to(struct device *dev, const char *name, const char *timer_name);
extern int of_gpio_get_active(const char *gpioname);
extern int of_gpio_get_value(const char *gpioname);
extern int of_gpio_set_value(const char *gpioname, int value);
extern int of_get_gpio_with_name(const char *gpioname);

extern struct platform_device *of_find_dsim_platform_device(void);
extern struct platform_device *of_find_decon_platform_device(void);
extern struct platform_device *of_find_device_by_path(const char *name);
extern struct device_node *of_find_decon_board(struct device *dev);
extern struct device_node *of_find_recommend_lcd_info(struct device *dev);

extern int of_update_phandle_property(struct device_node *from, const char *phandle_name, const char *node_name);
extern int of_update_phandle_property_list(struct device_node *from, const char *phandle_name, const char **node_names);
extern int of_update_recommend(struct device_node *np);

extern struct regulator_bulk_data *get_regulator_with_name(const char *name);
extern int get_regulator_use_count(struct regulator_bulk_data *bulk, const char *name);
#endif

