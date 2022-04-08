// SPDX-License-Identifier: GPL-2.0

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

/* Implements */
#include <gpexwa_interactive_boost.h>

/* Uses */
#include <gpexbe_devicetree.h>
#include <gpex_clock.h>

#define BOOST_MAX_DURATION 5000
#define DELAY_DURATION_MS 100

struct interactive_boost_info {
	ktime_t end_time;
	struct delayed_work unset_work;
	struct work_struct set_work;
	spinlock_t spinlock;
	int clock;
	int duration;
} ib_info;

int gpexwa_interactive_boost_set(int duration)
{
	if (duration > BOOST_MAX_DURATION)
		return -EINVAL;

	ib_info.duration = duration;
	schedule_work(&ib_info.set_work);

	return 0;
}

static void work_interactive_boost_set(struct work_struct *data)
{
	unsigned long flags;

	spin_lock_irqsave(&ib_info.spinlock, flags);

	ib_info.end_time = ktime_add_ms(ktime_get(), ib_info.duration);
	gpex_clock_lock_clock(GPU_CLOCK_MIN_LOCK, INTERACTIVE_LOCK, ib_info.clock);
	schedule_delayed_work(&ib_info.unset_work, msecs_to_jiffies(ib_info.duration));

	spin_unlock_irqrestore(&ib_info.spinlock, flags);
}

static void work_interactive_boost_unset(struct work_struct *data)
{
	unsigned long flags;

	spin_lock_irqsave(&ib_info.spinlock, flags);

	if (ktime_after(ktime_get(), ib_info.end_time))
		gpex_clock_lock_clock(GPU_CLOCK_MIN_UNLOCK, INTERACTIVE_LOCK, 0);
	else
		schedule_delayed_work(&ib_info.unset_work, msecs_to_jiffies(DELAY_DURATION_MS));

	spin_unlock_irqrestore(&ib_info.spinlock, flags);
}

int gpexwa_interactive_boost_init(void)
{
	INIT_WORK(&ib_info.set_work, work_interactive_boost_set);
	INIT_DELAYED_WORK(&ib_info.unset_work, work_interactive_boost_unset);
	ib_info.clock = gpexbe_devicetree_get_int(interactive_info.highspeed_clock);
	spin_lock_init(&ib_info.spinlock);

	return 0;
}

void gpexwa_interactive_boost_term(void)
{
	cancel_delayed_work_sync(&ib_info.unset_work);

	gpex_clock_lock_clock(GPU_CLOCK_MIN_UNLOCK, INTERACTIVE_LOCK, 0);
}
