/*
 * drivers/misc/samsung/scsc/mxman_sysevent.c
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
#include <linux/platform_device.h>
#include <soc/samsung/sysevent_notif.h>

#include "mxman_sysevent.h"

int wlbt_sysevent_powerup(const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Subsystem Power Up related
	 */
	 pr_info("%s: call-back function\n", __func__);
	return 0;
}

int wlbt_sysevent_shutdown(const struct sysevent_desc *sysevent, bool force_stop)
{
	/*
	 * This function is called in syseventtem_put / restart
	 * TODO: Subsystem Shutdown related
	 */
	pr_info("%s: call-back function - force_stop:%d\n", __func__, force_stop);
	return 0;
}

int wlbt_sysevent_ramdump(int enable, const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in syseventtem_put / restart
	 * TODO: Ramdump related
	 */
	pr_info("%s: call-back function - enable(%d)\n", __func__, enable);
	return 0;

}

void wlbt_sysevent_crash_shutdown(const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in panic handler
	 * TODO: Subsystem Crash Shutdown related
	 */
	pr_info("%s: call-back function\n", __func__);
}

int wlbt_sysevent_notifier_cb(struct notifier_block *nb,
						unsigned long code, void *nb_data)
{
	struct notif_data *notifdata = NULL;

	notifdata = (struct notif_data *) nb_data;
	switch (code) {
	case SYSTEM_EVENT_BEFORE_SHUTDOWN:
		pr_info("%s: %s: %s\n", __func__, notifdata->pdev->name,
			__stringify(SYSTEM_EVENT_BEFORE_SHUTDOWN));
		break;
	case SYSTEM_EVENT_AFTER_SHUTDOWN:
		pr_info("%s: %s: %s\n", __func__, notifdata->pdev->name,
			__stringify(SYSTEM_EVENT_AFTER_SHUTDOWN));
		break;
	case SYSTEM_EVENT_RAMDUMP_NOTIFICATION:
		pr_info("%s: %s: %s\n", __func__, notifdata->pdev->name,
			__stringify(SYSTEM_EVENT_RAMDUMP_NOTIFICATION));
		break;
	case SYSTEM_EVENT_BEFORE_POWERUP:
		if (nb_data) {
			notifdata = (struct notif_data *) nb_data;
			pr_info("%s: %s: %s, crash_status:%d, enable_ramdump:%d\n",
				__func__, notifdata->pdev->name,
				__stringify(SYSTEM_EVENT_BEFORE_POWERUP),
				notifdata->crashed, notifdata->enable_ramdump);
		} else {
			pr_info("%s: %s: %s\n", __func__,
				notifdata->pdev->name,
				__stringify(SYSTEM_EVENT_BEFORE_POWERUP));
		}
		break;
	case SYSTEM_EVENT_AFTER_POWERUP:
		pr_info("%s: %s: %s\n", __func__, notifdata->pdev->name,
		__stringify(SYSTEM_EVENT_AFTER_POWERUP));
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

#endif
