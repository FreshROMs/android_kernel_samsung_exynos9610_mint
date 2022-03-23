/* include/linux/wakelock.h
 *
 * Copyright (C) 2007-2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_WAKELOCK_H
#define _LINUX_WAKELOCK_H

#include <linux/ktime.h>
#include <linux/device.h>

/* A wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the wake_lock
 * prevents a full system suspend.
 */

enum {
	WAKE_LOCK_SUSPEND, /* Prevent suspend */
	WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
	struct wakeup_source ws;
};

static inline void wake_lock_init(struct wake_lock *lock, int type,
				  const char *name)
{
	wakeup_source_init(&lock->ws, name);
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
	wakeup_source_trash(&lock->ws);
}

static inline void wake_lock_stock(struct wake_lock *lock)
{
	__pm_stay_awake(&lock->ws);
}

static inline void wake_lock(struct wake_lock *lock)
{
#ifdef CONFIG_WAKELOCKS_DEFAULT_TIMEOUT
	__pm_wakeup_event(&lock->ws, CONFIG_WAKELOCKS_TIMEOUT);
#else
	__pm_stay_awake(&lock->ws);
#endif
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
#ifdef CONFIG_WAKELOCKS_DEFAULT_TIMEOUT
	__pm_wakeup_event(&lock->ws, min(jiffies_to_msecs(timeout), (unsigned int) CONFIG_WAKELOCKS_TIMEOUT));
#else
	__pm_wakeup_event(&lock->ws, jiffies_to_msecs(timeout));
#endif
}

static inline void wake_unlock(struct wake_lock *lock)
{
	__pm_relax(&lock->ws);
}

static inline int wake_lock_active(struct wake_lock *lock)
{
	return lock->ws.active;
}

#endif
