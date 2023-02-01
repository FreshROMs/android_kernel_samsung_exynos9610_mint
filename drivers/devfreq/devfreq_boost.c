// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018-2021 Sultan Alsawaf <sultan@kerneltoast.com>.
 */

#define pr_fmt(fmt) "devfreq_boost: " fmt

#include <linux/devfreq_boost.h>
#ifdef CONFIG_SCHED_EMS
#include <linux/ems.h>
#endif
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <uapi/linux/sched/types.h>

static unsigned long devfreq_boost_freq =
	CONFIG_DEVFREQ_EXYNOS_MIF_BOOST_FREQ;
static unsigned short devfreq_boost_dur =
	CONFIG_DEVFREQ_MIF_BOOST_DURATION_MS;

module_param(devfreq_boost_freq, long, 0644);
module_param(devfreq_boost_dur, short, 0644);

enum {
	SCREEN_OFF,
	MIF_BOOST,
	MAX_BOOST
};

struct boost_dev {
	struct devfreq *df;
	struct delayed_work device_unboost;
	struct delayed_work max_unboost;
	wait_queue_head_t boost_waitq;
	atomic_long_t max_boost_expires;
	unsigned long boost_freq;
	unsigned long state;
};

struct df_boost_drv {
	struct boost_dev devices[DEVFREQ_MAX];
	struct notifier_block fb_notif;
};

static void devfreq_device_unboost(struct work_struct *work);
static void devfreq_max_unboost(struct work_struct *work);

#define BOOST_DEV_INIT(b, dev, freq) .devices[dev] = {				\
	.device_unboost =							\
		__DELAYED_WORK_INITIALIZER((b).devices[dev].device_unboost,	\
					   devfreq_device_unboost, 0),		\
	.max_unboost =								\
		__DELAYED_WORK_INITIALIZER((b).devices[dev].max_unboost,	\
					   devfreq_max_unboost, 0),		\
	.boost_waitq =								\
		__WAIT_QUEUE_HEAD_INITIALIZER((b).devices[dev].boost_waitq),	\
	.boost_freq = freq							\
}

static struct df_boost_drv df_boost_drv_g __read_mostly = {
	BOOST_DEV_INIT(df_boost_drv_g, DEVFREQ_EXYNOS_MIF,
		       CONFIG_DEVFREQ_EXYNOS_MIF_BOOST_FREQ)
};
static int disable_boost = 0;

void devfreq_boost_disable(int disable)
{
	disable_boost = disable;
}

static void __devfreq_boost_kick(struct boost_dev *b)
{
	if (!READ_ONCE(b->df) || test_bit(SCREEN_OFF, &b->state))
		return;

	set_bit(MIF_BOOST, &b->state);
	if (!mod_delayed_work(system_unbound_wq, &b->device_unboost,
		msecs_to_jiffies(devfreq_boost_dur))) {
		/* Set the bit again in case we raced with the unboost worker */
		set_bit(MIF_BOOST, &b->state);
		wake_up(&b->boost_waitq);
	}
}

void devfreq_boost_kick(enum df_device device)
{
	struct df_boost_drv *d = &df_boost_drv_g;

	__devfreq_boost_kick(&d->devices[device]);
}

static void __devfreq_boost_kick_max(struct boost_dev *b,
				     unsigned long boost_jiffies)
{
	unsigned long curr_expires, new_expires;

	if (!READ_ONCE(b->df) || test_bit(SCREEN_OFF, &b->state))
		return;

	do {
		curr_expires = atomic_long_read(&b->max_boost_expires);
		new_expires = jiffies + boost_jiffies;

		/* Skip this boost if there's a longer boost in effect */
		if (time_after(curr_expires, new_expires))
			return;
	} while (atomic_long_cmpxchg(&b->max_boost_expires, curr_expires,
				     new_expires) != curr_expires);

	set_bit(MAX_BOOST, &b->state);
	if (!mod_delayed_work(system_unbound_wq, &b->max_unboost,
			      boost_jiffies)) {
		/* Set the bit again in case we raced with the unboost worker */
		set_bit(MAX_BOOST, &b->state);
		wake_up(&b->boost_waitq);
	}
}

void devfreq_boost_kick_max(enum df_device device, unsigned int duration_ms)
{
	struct df_boost_drv *d = &df_boost_drv_g;

	__devfreq_boost_kick_max(&d->devices[device], msecs_to_jiffies(duration_ms));
}

void devfreq_boost_frame_kick(enum df_device device)
{
	struct df_boost_drv *d = &df_boost_drv_g;

	if (unlikely(disable_boost))
		return;

	__devfreq_boost_kick_max(&d->devices[device], msecs_to_jiffies(CONFIG_DEVFREQ_DECON_BOOST_DURATION_MS));
}

void devfreq_register_boost_device(enum df_device device, struct devfreq *df)
{
	struct df_boost_drv *d = &df_boost_drv_g;
	struct boost_dev *b;

	df->is_boost_device = true;
	b = &d->devices[device];
	WRITE_ONCE(b->df, df);
}

static void devfreq_device_unboost(struct work_struct *work)
{
	struct boost_dev *b = container_of(to_delayed_work(work),
					   typeof(*b), device_unboost);

	clear_bit(MIF_BOOST, &b->state);
	wake_up(&b->boost_waitq);
}

static void devfreq_max_unboost(struct work_struct *work)
{
	struct boost_dev *b = container_of(to_delayed_work(work), typeof(*b),
					   max_unboost);

	clear_bit(MAX_BOOST, &b->state);
	wake_up(&b->boost_waitq);
}

static void devfreq_update_boosts(struct boost_dev *b, unsigned long state)
{
	struct devfreq *df = b->df;
	int first_freq_idx = df->profile->max_state - 1;

	mutex_lock(&df->lock);
	if (state & BIT(SCREEN_OFF)) {
		df->min_freq = df->profile->freq_table[first_freq_idx];
		df->max_boost = false;
	} else {
		df->min_freq = state & BIT(MIF_BOOST) ?
			       min(devfreq_boost_freq, df->max_freq) :
			       df->profile->freq_table[first_freq_idx];
		df->max_boost = state & BIT(MAX_BOOST);
	}
	update_devfreq(df);
	mutex_unlock(&df->lock);
}

static int devfreq_boost_thread(void *data)
{
	static const struct sched_param sched_max_rt_prio = {
		.sched_priority = MAX_RT_PRIO - 1
	};
	struct boost_dev *b = data;
	unsigned long old_state = 0;

	sched_setscheduler_nocheck(current, SCHED_FIFO, &sched_max_rt_prio);

	while (1) {
		bool should_stop = false;
		unsigned long curr_state;

		wait_event_interruptible(b->boost_waitq,
			(curr_state = READ_ONCE(b->state)) != old_state ||
			(should_stop = kthread_should_stop()));

		if (should_stop)
			break;

		if (old_state != curr_state) {
			devfreq_update_boosts(b, curr_state);
			old_state = curr_state;
		}
	}

	return 0;
}

static int fb_notifier_cb(struct notifier_block *nb, unsigned long action,
			  void *data)
{
	struct df_boost_drv *d = container_of(nb, typeof(*d), fb_notif);
	int i, *blank = ((struct fb_event *)data)->data;

	/* Parse framebuffer blank events as soon as they occur */
	if (action != FB_EARLY_EVENT_BLANK)
		return NOTIFY_OK;

	/* Boost when the screen turns on and unboost when it turns off */
	for (i = 0; i < DEVFREQ_MAX; i++) {
		struct boost_dev *b = d->devices + i;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
			clear_bit(SCREEN_OFF, &b->state);
			__devfreq_boost_kick_max(b,
				msecs_to_jiffies(CONFIG_DEVFREQ_WAKE_BOOST_DURATION_MS));
			break;
		case FB_BLANK_POWERDOWN:
			set_bit(SCREEN_OFF, &b->state);
			wake_up(&b->boost_waitq);
			break;
		}
	}

	return NOTIFY_OK;
}

#ifdef CONFIG_SCHED_EMS
/****************************************************************/
/*		  sysbusy state change notifier			*/
/****************************************************************/
static int devfreq_boost_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;
	struct df_boost_drv *d = &df_boost_drv_g;
	int mode;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	mode = !!(state > SYSBUSY_STATE0);

	if (mode)
		__devfreq_boost_kick_max(&d->devices[DEVFREQ_EXYNOS_MIF], sysbusy_params[state].release_duration);

	return NOTIFY_OK;
}

static struct notifier_block devfreq_boost_sysbusy_notifier = {
	.notifier_call = devfreq_boost_sysbusy_notifier_call,
};
#endif

static int __init devfreq_boost_init(void)
{
	struct df_boost_drv *d = &df_boost_drv_g;
	struct task_struct *thread[DEVFREQ_MAX];
	int i, ret;

	for (i = 0; i < DEVFREQ_MAX; i++) {
		struct boost_dev *b = d->devices + i;

		thread[i] = kthread_run_perf_critical(cpu_perf_mask,
						      devfreq_boost_thread, b,
						      "devfreq_boostd/%d", i);
		if (IS_ERR(thread[i])) {
			ret = PTR_ERR(thread[i]);
			pr_err("Failed to create kthread, err: %d\n", ret);
			goto stop_kthreads;
		}
	}

	d->fb_notif.notifier_call = fb_notifier_cb;
	d->fb_notif.priority = INT_MAX;
	ret = fb_register_client(&d->fb_notif);
	if (ret) {
		pr_err("Failed to register fb notifier, err: %d\n", ret);
		goto stop_kthreads;
	}

#ifdef CONFIG_SCHED_EMS
	ret = sysbusy_register_notifier(&devfreq_boost_sysbusy_notifier);
	if (ret)
		pr_warn("Failed to register sysbusy notifier, err: %d\n", ret);
#endif

	return 0;

stop_kthreads:
	while (i--)
		kthread_stop(thread[i]);
	return ret;
}
late_initcall(devfreq_boost_init);
