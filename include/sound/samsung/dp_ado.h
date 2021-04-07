/*
 * Samsung DP Audio driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DPADO_H
#define __DPADO_H

#ifdef CONFIG_SOC_EXYNOS9810
void dp_ado_switch_set_state(int state);
#else
#define dp_ado_set_state(state) do { } while (0)
#endif

#endif /* __DPADO_H */
