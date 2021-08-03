/*
 * drivers/misc/samsung/scsc/mxman_sysevent.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <soc/samsung/sysevent.h>
#include <soc/samsung/sysevent_notif.h>

int wlbt_sysevent_powerup(const struct sysevent_desc *desc);
int wlbt_sysevent_shutdown(const struct sysevent_desc *desc, bool force_stop);
int wlbt_sysevent_ramdump(int enable, const struct sysevent_desc *sysevent);
void wlbt_sysevent_crash_shutdown(const struct sysevent_desc *desc);
int wlbt_sysevent_notifier_cb(struct notifier_block *nb,
				unsigned long code, void *nb_data);

#endif
