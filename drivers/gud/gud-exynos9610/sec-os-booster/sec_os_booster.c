/* drivers/gud/sec-os-ctrl/secos_booster.c
 *
 * Secure OS booster driver for Samsung Exynos
 *
 * Copyright (c) 2014 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/of_gpio.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/suspend.h>

#include "secos_booster.h"
#include <linux/cpufreq.h>

#include "platform.h"

#define BOOST_POLICY_OFFSET	0
#define BOOST_TIME_OFFSET	16

#define NS_DIV_MS (1000ull * 1000ull)
#define WAIT_TIME (10ull * NS_DIV_MS)

int nq_switch_core(uint32_t cpu);
void mc_set_schedule_policy(int core);
uint32_t mc_active_core(void);

int mc_boost_usage_count;
struct mutex boost_lock;

unsigned int current_core;

unsigned int is_suspend_prepared;

struct timer_work {
	struct kthread_work work;
};

static struct pm_qos_request secos_booster_cluster1_qos;
static struct hrtimer timer;
static int max_cpu_freq;

static struct task_struct *mc_timer_thread;	/* Timer Thread task structure */
static DEFINE_KTHREAD_WORKER(mc_timer_worker);
static struct hrtimer mc_hrtimer;

static enum hrtimer_restart mc_hrtimer_func(struct hrtimer *timer)
{
#ifdef CONFIG_SECURE_OS_SUPPORT_MCT_DISABLE
	struct irq_desc *desc = irq_to_desc(MC_INTR_LOCAL_TIMER);

	if (desc->depth != 0)
		enable_irq(MC_INTR_LOCAL_TIMER);
#endif

	return HRTIMER_NORESTART;
}

static void mc_timer_work_func(struct kthread_work *work)
{
	hrtimer_start(&mc_hrtimer, ns_to_ktime((u64)LOCAL_TIMER_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
}

int secos_booster_request_pm_qos(struct pm_qos_request *req, s32 freq)
{
	static ktime_t recent_qos_req_time;
	ktime_t current_time;
	unsigned long long ns;

	current_time = ktime_get();

	ns = ktime_to_ns(ktime_sub(current_time, recent_qos_req_time));

	if (ns > 0 && WAIT_TIME > ns) {
		pr_info("%s: recalling time is too short. wait %lldms\n", __func__, (WAIT_TIME - ns) / NS_DIV_MS + 1);
		msleep((WAIT_TIME - ns) / NS_DIV_MS + 1);
	}

	pm_qos_update_request(req, freq);

	recent_qos_req_time = ktime_get();

	return 0;
}

int mc_timer(void)
{
	struct timer_work t_work = {
		KTHREAD_WORK_INIT(t_work.work, mc_timer_work_func),
	};

	if (!kthread_queue_work(&mc_timer_worker, &t_work.work))
		return false;

	kthread_flush_work(&t_work.work);
	return true;
}

static int mc_timer_init(void)
{
	cpumask_t cpu;

	mc_timer_thread = kthread_create(kthread_worker_fn, &mc_timer_worker, "mc_timer");
	if (IS_ERR(mc_timer_thread)) {
		mc_timer_thread = NULL;
		pr_err("%s: timer thread creation failed!", __func__);
		return -EFAULT;
	}

	wake_up_process(mc_timer_thread);

	cpumask_setall(&cpu);
	cpumask_clear_cpu(MIGRATE_TARGET_CORE, &cpu);
	/* ExySp */
	set_cpus_allowed_ptr(mc_timer_thread, &cpu);

	hrtimer_init(&mc_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mc_hrtimer.function = mc_hrtimer_func;

	return 0;
}

static void stop_wq(struct work_struct *work)
{
	int ret;

	ret = secos_booster_stop();
	if (ret)
		pr_err("%s: secos_booster_stop failed. err:%d\n", __func__, ret);

	return;
}

static DECLARE_WORK(stopwq, stop_wq);

static enum hrtimer_restart secos_booster_hrtimer_fn(struct hrtimer *timer)
{
	schedule_work_on(0, &stopwq);

	return HRTIMER_NORESTART;
}

int secos_booster_start(enum secos_boost_policy policy)
{
	int ret = 0;
	int freq;
	uint32_t boost_time;	/* milli second */
	enum secos_boost_policy boost_policy;

	mutex_lock(&boost_lock);
	mc_boost_usage_count++;

	if (mc_boost_usage_count > 1) {
		goto out;
	} else if (mc_boost_usage_count <= 0) {
		pr_err("boost usage count sync error. count : %d\n", mc_boost_usage_count);
		mc_boost_usage_count = 0;
		ret = -EINVAL;
		goto error;
	}

	current_core = mc_active_core();

	boost_time = (((uint32_t)policy) >> BOOST_TIME_OFFSET) & 0xFFFF;
	boost_policy = (((uint32_t)policy) >> BOOST_POLICY_OFFSET) & 0xFFFF;

	/* migrate to big Core */
	if (boost_policy >= PERFORMANCE_MAX_CNT || boost_policy < 0) {
		pr_err("%s: wrong secos boost policy:%d\n", __func__, boost_policy);
		ret = -EINVAL;
		goto error;
	}

	/* cpufreq configuration */
	if (boost_policy == MAX_PERFORMANCE)
		freq = max_cpu_freq;
	else if (boost_policy == MID_PERFORMANCE)
		freq = max_cpu_freq;
	else if (boost_policy == STB_PERFORMANCE)
		freq = max_cpu_freq;
	else
		freq = 0;


	if (!cpu_online(MIGRATE_TARGET_CORE)) {
		pr_debug("%s: %d core is offline\n", __func__, MIGRATE_TARGET_CORE);
		udelay(100);
		if (!cpu_online(MIGRATE_TARGET_CORE)) {
			pr_debug("%s: %d core is offline\n", __func__, MIGRATE_TARGET_CORE);
			ret = -EPERM;
			goto error;
		}
		pr_debug("%s: %d core is online\n", __func__, MIGRATE_TARGET_CORE);
	}

	if (secos_booster_request_pm_qos(&secos_booster_cluster1_qos, freq)) { /* KHz */
		ret = -EPERM;
		goto error;
	}

	ret = nq_switch_core(MIGRATE_TARGET_CORE);
	if (ret) {
		pr_err("%s: mc switch failed : err:%d\n", __func__, ret);
		secos_booster_request_pm_qos(&secos_booster_cluster1_qos, 0);
		ret = -EPERM;
		goto error;
	}

	if (boost_policy == STB_PERFORMANCE) {
		/* Restore origin performance policy after spend default boost time */
		if (boost_time == 0)
			boost_time = DEFAULT_SECOS_BOOST_TIME;

		hrtimer_cancel(&timer);
		hrtimer_start(&timer, ns_to_ktime((u64)boost_time * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
	} else {
		/* Change schedule policy */
		mc_set_schedule_policy(MIGRATE_TARGET_CORE);
	}

out:
	mutex_unlock(&boost_lock);
	return ret;

error:
	mc_boost_usage_count--;
	mutex_unlock(&boost_lock);
	return ret;
}

int secos_booster_stop(void)
{
	int ret = 0;

	mutex_lock(&boost_lock);
	mc_boost_usage_count--;
	mc_set_schedule_policy(DEFAULT_LITTLE_CORE);

	if (mc_boost_usage_count > 0) {
		goto out;
	} else if(mc_boost_usage_count == 0) {
		hrtimer_cancel(&timer);
		pr_debug("%s: mc switch to little core \n", __func__);
		ret = nq_switch_core(current_core);
		if (ret)
			pr_err("%s: mc switch core failed. err:%d\n", __func__, ret);

		secos_booster_request_pm_qos(&secos_booster_cluster1_qos, 0);
	} else {
		/* mismatched usage count */
		pr_warn("boost usage count sync mismatched. count : %d\n", mc_boost_usage_count);
		mc_boost_usage_count = 0;
	}

out:
	mutex_unlock(&boost_lock);
	return ret;
}

static int secos_booster_pm_notifier(struct notifier_block *notifier,
		unsigned long pm_event, void *dummy)
{
	mutex_lock(&boost_lock);
	switch (pm_event) {
		case PM_SUSPEND_PREPARE:
			is_suspend_prepared = true;
			break;
		case PM_POST_SUSPEND:
			is_suspend_prepared = false;
			break;
	}
	mutex_unlock(&boost_lock);

	return NOTIFY_OK;
}

static struct notifier_block secos_booster_pm_notifier_block = {
	.notifier_call = secos_booster_pm_notifier,
};

static int __init secos_booster_init(void)
{
	int ret;

	mutex_init(&boost_lock);

	ret = mc_timer_init();
	if (ret) {
		pr_err("%s: mc timer init error :%d\n", __func__, ret);
		return ret;
	}

	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = secos_booster_hrtimer_fn;

	max_cpu_freq = cpufreq_quick_get_max(MIGRATE_TARGET_CORE);

	pm_qos_add_request(&secos_booster_cluster1_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);

	register_pm_notifier(&secos_booster_pm_notifier_block);

	return ret;
}
late_initcall(secos_booster_init);
