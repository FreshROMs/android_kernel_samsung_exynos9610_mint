/*****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd and its Licensors.
 * All rights reserved.
 *
 ****************************************************************************/
#include <linux/jiffies.h>
#include <linux/module.h>
#include "dev.h"
#include "debug.h"
#include "wakelock.h"
#include "utils.h"

void slsi_wakelock(struct slsi_wake_lock *lock)
{
	unsigned long flags;

	if (lock == NULL) {
		SLSI_ERR_NODEV("Failed to take wakelock, lock is NULL");
		return;
	}
	spin_lock_irqsave(&lock->wl_spinlock, flags);
	if (!lock->counter)
		wake_lock(&lock->wl);

	lock->counter++;
	spin_unlock_irqrestore(&lock->wl_spinlock, flags);
}

void slsi_wakeunlock(struct slsi_wake_lock *lock)
{
	unsigned long flags;

	if (lock == NULL) {
		SLSI_ERR_NODEV("Failed to unlock the wakelock, lock is NULL");
		return;
	}
	spin_lock_irqsave(&lock->wl_spinlock, flags);

	if (lock->counter) {
		lock->counter--;
		if (!lock->counter)
			wake_unlock(&lock->wl);
	} else {
		SLSI_ERR_NODEV("Wakelock has already released!");
	}
	spin_unlock_irqrestore(&lock->wl_spinlock, flags);
}

void slsi_wakelock_timeout(struct slsi_wake_lock *lock, int timeout)
{
	if (lock == NULL) {
		SLSI_ERR_NODEV("Failed to take wakelock timeout, lock is NULL");
		return;
	}
	lock->counter = 1;
	wake_lock_timeout(&lock->wl, msecs_to_jiffies(timeout));
}

int slsi_is_wakelock_active(struct slsi_wake_lock *lock)
{
	if (lock == NULL) {
		SLSI_ERR_NODEV("Failed to check wakelock status, lock is NULL");
		return 0;
	}

	if (wake_lock_active(&lock->wl))
		return 1;
	return 0;
}

void slsi_wakelock_exit(struct slsi_wake_lock *lock)
{
	if (lock == NULL) {
		SLSI_ERR_NODEV("Failed to destroy the wakelock, lock is NULL");
		return;
	}

	wake_lock_destroy(&lock->wl);
}

void slsi_wakelock_init(struct slsi_wake_lock *lock, char *name)
{
	if (lock == NULL) {
		SLSI_ERR_NODEV("Failed to init the wakelock, lock is NULL");
		return;
	}
	lock->counter = 0;
	wake_lock_init(&lock->wl, WAKE_LOCK_SUSPEND, name);
	spin_lock_init(&lock->wl_spinlock);
}
