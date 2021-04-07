// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * 1. in foo.c
 * struct dsim_lcd_driver foo_mipi_lcd_driver = {
 *      .name           = "foo", <----- driver name = node name
 * };
 * __XX_ADD_LCD_DRIVER(foo_mipi_lcd_driver);
 *
 * 2. in bar.c if it needs
 * struct dsim_lcd_driver bar_mipi_lcd_driver = {
 *      .name           = "bar",
 * };
 * __XX_ADD_LCD_DRIVER(bar_mipi_lcd_driver);
 **************************************************
 * case 0
 *
 * 1. in Makefile
 * obj-$(CONFIG_FOO)    += foo.o
 *
 * 2. in display-lcd_example_common.dtsi
 * / {
 *      fragment@lcd {
 *              target-path = "/";
 *              __overlay__ {
 *                      foo: foo {
 *                              decon_board = <&decon_board_foo>;
 *                      };
 *              };
 *      };
 * };
 *
 * &dsim_0 {
 *      lcd_info = <&foo>;
 * };
 *
 * 3. in exynos0000-example_common.dtsi
 * #include "display-lcd_example_common.dtsi"
 **************************************************
 * case 1
 *
 * 1. in Makefile
 * obj-$(CONFIG_FOO)    += foo.o bar.o
 *
 * 2. in display-lcd_example_common.dtsi
 * / {
 *      fragment@lcd {
 *              target-path = "/";
 *              __overlay__ {
 *                      foo: foo {
 *                              decon_board = <&decon_board_foo>;
 *                      };
 *              };
 *      };
 * };
 *
 * &dsim_0 {
 *      lcd_info = <&foo>;
 * };
 *
 * 3. in exynos0000-example_common.dtsi
 * #include "display-lcd_example_common.dtsi"
 *
 * 4. in display-lcd_example_00.dtsi
 * / {
 *      fragment@lcd {
 *              target-path = "/";
 *              __overlay__ {
 *                      bar: bar {
 *                              decon_board = <&decon_board_bar>;
 *                      };
 *              };
 *      };
 * };
 *
 * &dsim_0 {
 *      lcd_info = <&bar>;
 * };
 *
 * 5. in exynos0000-example_00.dts
 * #include "display-lcd_example_00.dtsi"
 **************************************************
 * case 2. __lcd_driver_update_by_id_match
 *
 * 1. in Makefile
 * obj-$(CONFIG_FOO)    += foo.o bar.o
 *
 * 2. in display-lcd_example_common.dtsi
 * / {
 *      fragment@lcd {
 *              target-path = "/";
 *              __overlay__ {
 *                      foo: foo {
 *                              decon_board = <&decon_board_foo>;
 *                      };
 *                      bar: bar {
 *                              id_match: MASK(HEX) EXPECT(HEX) is used with boot param lcdtype
 *                              id_match = <0x0000C0    0x000000>;
 *                              decon_board = <&decon_board_bar>;
 *                      };
 *              };
 *      };
 * };
 *
 * &dsim_0 {
 *      lcd_info = <&foo>;
 * };
 *
 * 3. in exynos0000-example_common.dtsi
 * #include "display-lcd_example_common.dtsi"
 *
 * 4. or you can include multiple separated dtsi for each foo and bar
 * in exynos0000-example_common.dtsi
 * #include "display-lcd_example_foo.dtsi"
 * #include "display-lcd_example_bar.dtsi"
 **************************************************
 * case 3. __lcd_driver_update_by_function
 *
 * 1. in Makefile
 * obj-$(CONFIG_FOO)    += foo.o bar.o
 *
 * 2. in display-lcd_example_common.dtsi
 * / {
 *      fragment@lcd {
 *              target-path = "/";
 *              __overlay__ {
 *                      foo: foo {
 *                              decon_board = <&decon_board_foo>;
 *                      };
 *                      bar: bar {
 *                              decon_board = <&decon_board_bar>;
 *                      };
 *              };
 *      };
 * };
 *
 * &dsim_0 {
 *      lcd_info = <&foo>;
 * };
 *
 * 2. in exynos0000-example_common.dtsi
 * #include "display-lcd_example_common.dtsi"
 *
 * 3. in bar.c
 * static int bar_match(void *unused)
 * {
 *      if (something done)
 *              return NOTIFY_DONE;
 *      else if (something ok)
 *              return NOTIFY_OK;
 *      else if (something ok and stop)
 *              return NOTIFY_STOP;
 *      else if (something not ok and stop)
 *              return NOTIFY_BAD;
 * }
 *
 * struct dsim_lcd_driver bar_mipi_lcd_driver = {
 *      .name           = "bar",
 *      .match          = bar_match,
 * };
 * __XX_ADD_LCD_DRIVER(bar_mipi_lcd_driver);
 **************************************************
 * case 4. __lcd_driver_dts_update
 *
 * 1. in Makefile
 * obj-$(CONFIG_FOO)    += foo.o
 *
 * 2. in display-lcd_example_common.dtsi
 * / {
 *      fragment@lcd {
 *              target-path = "/";
 *              __overlay__ {
 *                      foo: foo {
 *                              decon_board = <&decon_board_foo>;
 *                      };
 *                      bar: bar {
 *                              id_match: MASK(HEX) EXPECT(HEX) is used with boot param lcdtype
 *                              id_match = <0x0000C0    0x000000>;
 *                              decon_board = <&decon_board_bar>;
 *                      };
 *              };
 *      };
 * };
 *
 * &dsim_0 {
 *      lcd_info = <&foo &bar>;
 * };
 *
 * 3. in exynos0000-example_common.dtsi
 * #include "display-lcd_example_common.dtsi"
 *
 */

#include <linux/lcd.h>
#include "../dsim.h"
#include "../decon_board.h"

#include "dsim_panel.h"

#define BOARD_DTS_NAME	"decon_board"
#define PANEL_DTS_NAME	"lcd_info"

#define dbg_info(fmt, ...)		pr_info(pr_fmt("dsim: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("dsim: "fmt), ##__VA_ARGS__)

unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dbg_info("%s: lcdtype: %6X\n", __func__, lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

struct dsim_lcd_driver *mipi_lcd_driver;

static int __lcd_driver_update_by_lcd_info_name(struct dsim_lcd_driver *drv)
{
	struct device_node *node;
	int count = 0, ret = 0;
	char *dts_name = "lcd_info";

	node = of_find_node_with_property(NULL, dts_name);
	if (!node) {
		dbg_info("%s: of_find_node_with_property fail\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	count = of_count_phandle_with_args(node, dts_name, NULL);
	if (count < 0) {
		dbg_info("%s: of_count_phandle_with_args fail: %d\n", __func__, count);
		ret = -EINVAL;
		goto exit;
	}

	node = of_parse_phandle(node, dts_name, 0);
	if (!node) {
		dbg_info("%s: of_parse_phandle fail\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (IS_ERR_OR_NULL(drv) || IS_ERR_OR_NULL(drv->name)) {
		dbg_info("%s: we need lcd_driver name to compare with dts node name(%s)\n", __func__, node->name);
		ret = -EINVAL;
		goto exit;
	}

	if (!strcmp(node->name, drv->name) && mipi_lcd_driver != drv) {
		mipi_lcd_driver = drv;
		dbg_info("%s: driver(%s) is updated\n", __func__, mipi_lcd_driver->name);
	} else
		dbg_none("%s: driver(%s) is diffferent with %s node(%s)\n", __func__, dts_name, node->name, drv->name);

exit:
	return ret;
}

static void __init __lcd_driver_dts_update(void)
{
	struct device_node *nplcd = NULL, *np = NULL;
	int i = 0, count = 0, ret = -EINVAL;
	unsigned int id_index, mask, expect;
	u32 id_match_info[10] = {0, };

	nplcd = of_find_node_with_property(NULL, PANEL_DTS_NAME);
	if (!nplcd) {
		dbg_info("%s: %s property does not exist\n", __func__, PANEL_DTS_NAME);
		return;
	}

	count = of_count_phandle_with_args(nplcd, PANEL_DTS_NAME, NULL);
	if (count < 2) {
		/* dbg_info("%s: %s property phandle count is %d. so no need to update check\n", __func__, PANEL_DTS_NAME, count); */
		return;
	}

	for (i = 0; i < count; i++) {
		np = of_parse_phandle(nplcd, PANEL_DTS_NAME, i);
		dbg_info("%s: %dth dts is %s\n", __func__, i, (np && np->name) ? np->name : "null");
		if (!np || !of_get_property(np, "id_match", NULL))
			continue;

		count = of_property_count_u32_elems(np, "id_match");
		if (count < 0 || count > ARRAY_SIZE(id_match_info) || count % 2) {
			dbg_info("%s: %dth dts(%s) has invalid id_match count(%d)\n", __func__, i, np->name, count);
			continue;
		}

		memset(id_match_info, 0, sizeof(id_match_info));
		ret = of_property_read_u32_array(np, "id_match", id_match_info, count);
		if (ret < 0) {
			dbg_info("%s: of_property_read_u32_array fail. ret(%d)\n", __func__, ret);
			continue;
		}

		for (id_index = 0; id_index < count; id_index += 2) {
			mask = id_match_info[id_index];
			expect = id_match_info[id_index + 1];

			if ((lcdtype & mask) == expect) {
				dbg_info("%s: %dth dts(%s) id_match. lcdtype(%06X), mask(%06X), expect(%06X)\n",
					__func__, i, np->name, lcdtype, mask, expect);
				if (i > 0)
					ret = of_update_phandle_property(NULL, PANEL_DTS_NAME, np->name);
				return;
			}
		}
	}
}

static void __init __lcd_driver_update_by_id_match(void)
{
	struct dsim_lcd_driver **p_lcd_driver = &__start___lcd_driver;
	struct dsim_lcd_driver *lcd_driver = NULL;
	struct device_node *np = NULL;
	int i = 0, count = 0, ret = -EINVAL, do_match = 0;
	unsigned int id_index, mask, expect;
	u32 id_match_info[10] = {0, };

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (lcd_driver && lcd_driver->name) {
			np = of_find_node_by_name(NULL, lcd_driver->name);
			if (np && of_get_property(np, "id_match", NULL)) {
				dbg_info("%s: %dth lcd_driver(%s) has dts id_match property\n", __func__, i, lcd_driver->name);
				do_match++;
			}
		}
	}

	if (!do_match)
		return;

	if (i != do_match)
		of_update_phandle_property(NULL, PANEL_DTS_NAME, __start___lcd_driver->name);

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (!lcd_driver || !lcd_driver->name) {
			dbg_info("%dth lcd_driver is invalid\n", i);
			continue;
		}

		np = of_find_node_by_name(NULL, lcd_driver->name);
		if (!np || !of_get_property(np, "id_match", NULL)) {
			/* dbg_info("%dth lcd_driver(%s) has no id_match property\n", lcd_driver->name); */
			continue;
		}

		count = of_property_count_u32_elems(np, "id_match");
		if (count < 0 || count > ARRAY_SIZE(id_match_info) || count % 2) {
			dbg_info("%s: %dth lcd_driver(%s) has invalid id_match count(%d)\n", __func__, i, lcd_driver->name, count);
			continue;
		}

		memset(id_match_info, 0, sizeof(id_match_info));
		ret = of_property_read_u32_array(np, "id_match", id_match_info, count);
		if (ret < 0) {
			dbg_info("%s: of_property_read_u32_array fail. ret(%d)\n", __func__, ret);
			continue;
		}

		for (id_index = 0; id_index < count; id_index += 2) {
			mask = id_match_info[id_index];
			expect = id_match_info[id_index + 1];

			if ((lcdtype & mask) == expect) {
				dbg_info("%s: %dth lcd_driver(%s) id_match. lcdtype(%06X), mask(%06X), expect(%06X)\n",
					__func__, i, lcd_driver->name, lcdtype, mask, expect);
				if (mipi_lcd_driver != lcd_driver) {
					mipi_lcd_driver = lcd_driver;
					of_update_phandle_property(NULL, PANEL_DTS_NAME, lcd_driver->name);
				}
				return;
			}
		}
	}
}

static void __init __lcd_driver_update_by_function(void)
{
	struct dsim_lcd_driver **p_lcd_driver = &__start___lcd_driver;
	struct dsim_lcd_driver *lcd_driver = NULL;
	int i = 0, ret = -EINVAL, do_match = 0;

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (lcd_driver && lcd_driver->match) {
			dbg_info("%s: %dth lcd_driver %s and has driver match function\n", __func__, i, lcd_driver->name);
			do_match++;
		}
	}

	if (!do_match)
		return;

	if (i != do_match)
		of_update_phandle_property(NULL, PANEL_DTS_NAME, __start___lcd_driver->name);

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (!lcd_driver || !lcd_driver->match)
			continue;

		ret = lcd_driver->match(NULL);
		if (ret & NOTIFY_OK) {
			dbg_info("%s: %dth lcd_driver(%s) notify ok to register\n", __func__, i, lcd_driver->name);
			if (mipi_lcd_driver != lcd_driver) {
				mipi_lcd_driver = lcd_driver;
				ret = of_update_phandle_property(NULL, PANEL_DTS_NAME, lcd_driver->name);
			}
			return;
		}

		if (ret & NOTIFY_STOP_MASK) {
			dbg_info("%s: %dth lcd_driver(%s) notify stop\n", __func__, i, lcd_driver->name);
			return;
		}
	}
}

static int __init lcd_driver_init(void)
{
	struct dsim_lcd_driver **p_lcd_driver = &__start___lcd_driver;
	struct dsim_lcd_driver *lcd_driver = NULL;
	int i = 0, ret = -EINVAL;

	__lcd_driver_dts_update();

	if (mipi_lcd_driver && mipi_lcd_driver->name) {
		dbg_info("%s: %s driver is registered\n", __func__, mipi_lcd_driver->name);
		return 0;
	}

	mipi_lcd_driver = __start___lcd_driver;

	if (++p_lcd_driver == &__stop___lcd_driver) {
		/* dbg_info("%s: lcd_driver is only one\n", __func__); */
		return 0;
	}

	__lcd_driver_update_by_id_match();
	__lcd_driver_update_by_function();

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (!lcd_driver)
			continue;

		dbg_info("%s: %dth lcd_driver is %s\n", __func__, i, lcd_driver->name);
		ret = __lcd_driver_update_by_lcd_info_name(lcd_driver);
	}

	WARN_ON(!mipi_lcd_driver);
	mipi_lcd_driver = mipi_lcd_driver ? mipi_lcd_driver : __start___lcd_driver;

	dbg_info("%s: %s driver is registered\n", __func__, mipi_lcd_driver->name ? mipi_lcd_driver->name : "null");

	return 0;
}
device_initcall(lcd_driver_init);

