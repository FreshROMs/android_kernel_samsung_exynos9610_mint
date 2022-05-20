// SPDX-License-Identifier: GPL-2.0
/*
 * sec_mm/
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 */

#include <linux/module.h>
#include <linux/mm.h>
#include "sec_mm.h"

static int sec_mm_panic_handler(struct notifier_block *nb, unsigned long action,
				void *str_buf)
{
	/* not to print duplicate information */
	if (strstr(str_buf, "System is deadlocked on memory"))
		return NOTIFY_DONE;

#ifdef CONFIG_SEC_MM
	show_mem(0, NULL);
#else
	mm_debug_show_free_areas();
#endif
	mm_debug_dump_tasks();

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = sec_mm_panic_handler,
};

void init_panic_handler(void)
{
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
}

void exit_panic_handler(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_block);
}

MODULE_LICENSE("GPL");
