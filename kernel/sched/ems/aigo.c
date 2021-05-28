/*
 * CPUFreq governor based on scheduler-provided CPU utilization data.
 *
 * Copyright (C) 2016, Intel Corporation
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpu_pm.h>
#include <linux/cpufreq.h>
#include <linux/ems.h>
#include <linux/fb.h>
#include <linux/ffsi.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <uapi/linux/sched/types.h>

#include <trace/events/ems.h>
#include <trace/events/power.h>

#include "../sched.h"
#include "../tune.h"
#include "ems.h"

/**
 * 2nd argument of ffsi_obj_creator() experimentally decided by client itself,
 * which represents how much variant the random variable registered to FFSI
 * instance can behave at most, in terms of referencing d2u_decl_cmtpdf table
 * (maximum index of d2u_decl_cmtpdf table).
 */
#define UTILAVG_FFSI_VARIANCE	16
static struct elasticity elasticity_cpufreq = {
	.gamma_numer 	= 32,
	.gamma_denom 	= 25,
	.theta_numer 	= 23,
	.theta_denom 	= 25,
};

#define AIGOV_KTHREAD_PRIORITY	50

struct aigov_tunables {
	struct gov_attr_set  attr_set;
	unsigned int         up_rate_limit_us;
	unsigned int         down_rate_limit_us;
	bool                 iowait_boost_enable;
};

struct aigov_policy {
	struct cpufreq_policy    *policy;

	struct aigov_tunables    *tunables;
	struct list_head         tunables_hook;

	raw_spinlock_t           update_lock;  /* For shared policies */
	u64                      last_freq_update_time;
	s64                      min_rate_limit_ns;
	s64                      up_rate_delay_ns;
	s64                      down_rate_delay_ns;
	unsigned int             next_freq;
	unsigned int             cached_raw_freq;

	/* The next fields are only needed if fast switch cannot be used. */
	struct irq_work          irq_work;
	struct kthread_work      work;
	struct mutex             work_lock;
	struct kthread_worker    worker;
	struct task_struct       *thread;
	bool                     work_in_progress;

	bool 					 limits_changed;
	bool                     need_freq_update;
	bool 			         be_stochastic;

	/* Framebuffer callbacks */
	struct notifier_block    fb_notifier;
	bool                     fb_panel_blank;
};

struct aigov_cpu {
	struct update_util_data   update_util;
	struct aigov_policy       *ag_policy;
	unsigned int              cpu;

	bool                      iowait_boost_pending;
	unsigned int              iowait_boost;
	unsigned int              iowait_boost_max;
	u64                       last_update;

	/* slack timer */
	unsigned long             slack_min_util;
	int                       slack_enabled;
	bool                      slack_started;
	int                       slack_expired_time_ms;
	struct timer_list         slack_timer;

	/* QoS minimum class */
	int                       qos_min_class;

	/* vessel for FFSI inferrer and learner */
	struct ffsi_class 	      *util_vessel;
	unsigned long 		      cached_util;

	/* The fields below are only needed when sharing a policy. */
	unsigned long             util;
	unsigned long             max;
	unsigned int              flags;

	/* The field below is for single-CPU policies only. */
#ifdef CONFIG_NO_HZ_COMMON
	unsigned long             saved_idle_calls;
#endif
};

static DEFINE_PER_CPU(struct aigov_cpu, aigov_cpu);

#define DEFAULT_EXPIRED_TIME	70
static void aigov_stop_slack(int cpu);
static void aigov_start_slack(int cpu);
static void aigov_update_min(struct cpufreq_policy *policy);

/************************ Governor internals ***********************/
struct aigov_policy_list {
	struct list_head list;
	struct aigov_policy *ag_policy;
	struct cpumask cpus;
};
static LIST_HEAD(aigov_policy_list);

static inline
struct aigov_policy_list *find_ag_pol_list(struct cpufreq_policy *policy)
{
	struct aigov_policy_list *ag_pol_list;

	list_for_each_entry(ag_pol_list, &aigov_policy_list, list)
		if (cpumask_test_cpu(policy->cpu, &ag_pol_list->cpus))
			return ag_pol_list;

	return NULL;
}

static struct aigov_policy
	*aigov_restore_policy(struct cpufreq_policy *policy)
{
	struct aigov_policy_list *ag_pol_list =
			ag_pol_list = find_ag_pol_list(policy);

	if (!ag_pol_list)
		return NULL;

	pr_info("Restore ag_policy(%d) from policy_list\(%x)n",
		policy->cpu,
		*(unsigned int *)cpumask_bits(&ag_pol_list->cpus));

	return ag_pol_list->ag_policy;
}

static int aigov_save_policy(struct aigov_policy *ag_policy)
{
	struct aigov_policy_list *ag_pol_list;
	struct cpufreq_policy *policy = ag_policy->policy;

	if (unlikely(!ag_policy))
		return 0;

	ag_pol_list = find_ag_pol_list(policy);
	if (ag_pol_list) {
		pr_info("Already saved ag_policy(%d) to policy_list\(%x)n",
			policy->cpu,
			*(unsigned int *)cpumask_bits(&ag_pol_list->cpus));
		return 1;
	}

	/* Back up aigov_policy to list */
	ag_pol_list = kzalloc(sizeof(struct aigov_policy_list), GFP_KERNEL);
	if (!ag_pol_list)
		return 0;

	cpumask_copy(&ag_pol_list->cpus, policy->related_cpus);
	ag_pol_list->ag_policy = ag_policy;
	list_add(&ag_pol_list->list, &aigov_policy_list);

	pr_info("Save ag_policy(%d) to policy_list(%x)\n",
		policy->cpu,
		*(unsigned int *)cpumask_bits(&ag_pol_list->cpus));

	return 1;
}

static
bool aigov_should_update_freq(struct aigov_policy *ag_policy, u64 time)
{
	s64 delta_ns;

	/*
	 * Since cpufreq_update_util() is called with rq->lock held for
	 * the @target_cpu, our per-cpu data is fully serialized.
	 *
	 * However, drivers cannot in general deal with cross-cpu
	 * requests, so while get_next_freq() will work, our
	 * aigov_update_commit() call may not for the fast switching platforms.
	 *
	 * Hence stop here for remote requests if they aren't supported
	 * by the hardware, as calculating the frequency is pointless if
	 * we cannot in fact act on it.
	 *
	 * For the slow switching platforms, the kthread is always scheduled on
	 * the right set of CPUs and any CPU can find the next frequency and
	 * schedule the kthread.
	 */
	if (ag_policy->policy->fast_switch_enabled &&
	    !cpufreq_can_do_remote_dvfs(ag_policy->policy))
		return false;

	if (unlikely(ag_policy->limits_changed)) {
		ag_policy->limits_changed = false;
		ag_policy->need_freq_update = true;
		return true;
	}

	/* No need to recalculate next freq for min_rate_limit_us
	 * at least. However we might still decide to further rate
	 * limit once frequency change direction is decided, according
	 * to the separate rate limits.
	 */

	delta_ns = time - ag_policy->last_freq_update_time;
	return delta_ns >= ag_policy->min_rate_limit_ns;
}

static bool aigov_up_down_rate_limit(struct aigov_policy *ag_policy, u64 time,
				     unsigned int next_freq)
{
	s64 delta_ns;

	delta_ns = time - ag_policy->last_freq_update_time;

	if (next_freq > ag_policy->next_freq &&
	    delta_ns < ag_policy->up_rate_delay_ns)
			return true;

	if (next_freq < ag_policy->next_freq &&
	    delta_ns < ag_policy->down_rate_delay_ns)
			return true;

	return false;
}

static inline
void aigov_irq_work_queue(struct irq_work *work)
{
	if (likely(cpu_active(raw_smp_processor_id())))
		irq_work_queue(work);
	else
		irq_work_queue_on(work, cpumask_any(cpu_active_mask));
}

static void aigov_update_commit(struct aigov_policy *ag_policy, u64 time,
				unsigned int next_freq)
{
	struct cpufreq_policy *policy = ag_policy->policy;
	int cpu;

	if (ag_policy->next_freq == next_freq)
		return;

	if (aigov_up_down_rate_limit(ag_policy, time, next_freq)) {
		/* Don't cache a raw freq that didn't become next_freq */
		ag_policy->cached_raw_freq = 0;
		return;
	}

	ag_policy->next_freq = next_freq;
	ag_policy->last_freq_update_time = time;

	if (policy->fast_switch_enabled) {
		next_freq = cpufreq_driver_fast_switch(policy, next_freq);
		if (!next_freq)
			return;

		policy->cur = next_freq;
		trace_cpu_frequency(next_freq, smp_processor_id());
	} else if (!ag_policy->work_in_progress) {
		ag_policy->work_in_progress = true;
		trace_cpu_frequency(next_freq, cpu);
		aigov_irq_work_queue(&ag_policy->irq_work);
	}
}

static int sysbusy_state = SYSBUSY_STATE0;
static
void aigov_inferrer_get_delta(struct rand_var *rv, struct aigov_cpu *ai_cpu) {
	rv->nval = min(ai_cpu->max, ai_cpu->util) - ai_cpu->cached_util;
	rv->ubound = ai_cpu->max - ai_cpu->cached_util;
	rv->lbound = ai_cpu->cached_util;
}

static inline
unsigned int aigov_inferrer_get_freq(struct aigov_cpu *ai_cpu, unsigned int initial_freq) {
	unsigned int inferred_freq = initial_freq;
	struct ffsi_class *vessel;
	int cur_rand = FFSI_DIVERGING;
	struct rand_var rv = {
		.nval		= 0,
		.ubound		= 0,
		.lbound		= 0
	};

	if (sysbusy_state > SYSBUSY_STATE1)
		goto skip_inference;

	vessel = ai_cpu->util_vessel;

	if (unlikely(!vessel))
		goto skip_inference;

	cur_rand = vessel->job_inferer(vessel);
	if (cur_rand == FFSI_DIVERGING)
		goto skip_inference;

	aigov_inferrer_get_delta(&rv, ai_cpu);
	inferred_freq = vessel->cap_bettor(vessel, &rv, initial_freq);

skip_inference:
	trace_aigov_ffsi_freq(ai_cpu->cpu, ai_cpu->util, ai_cpu->max, cur_rand, initial_freq, inferred_freq);
	return inferred_freq;
}

static inline
void aigov_inferrer_adapt_util(struct aigov_cpu *ai_cpu)
{
	struct ffsi_class *vessel = ai_cpu->util_vessel;
	struct rand_var job = {
		.nval		= 0,
		.ubound		= 0,
		.lbound		= 0
	};

	if (unlikely(!vessel))
		return;

	aigov_inferrer_get_delta(&job, ai_cpu);
	vessel->job_learner(vessel, &job);
}

/**
 * get_next_freq - Compute a new frequency for a given cpufreq policy.
 * @ag_policy: adaptive policy object to compute the new frequency for.
 * @util: Current CPU utilization.
 * @max: CPU capacity.
 *
 * If the utilization is frequency-invariant, choose the new frequency to be
 * proportional to it, that is
 *
 * next_freq = C * max_freq * util / max
 *
 * Otherwise, approximate the would-be frequency-invariant utilization by
 * util_raw * (curr_freq / max_freq) which leads to
 *
 * next_freq = C * curr_freq * util_raw / max
 *
 * Take C = 1.25 for the frequency tipping point at (util / max) = 0.8.
 *
 * The lowest driver-supported frequency which is equal or greater than the raw
 * next_freq (as calculated above) is returned, subject to policy min/max and
 * cpufreq driver limitations.
 */
static
unsigned int aigov_get_next_freq(struct aigov_cpu *ai_cpu, struct aigov_policy *ag_policy,
				  unsigned long util, unsigned long max)
{
	struct cpufreq_policy *policy = ag_policy->policy;
	unsigned int freq = arch_scale_freq_invariant() ? policy->max : policy->cur;

	freq = freq * util / max;
	freq = aigov_inferrer_get_freq(ai_cpu, freq);

	if (freq == ag_policy->cached_raw_freq && !ag_policy->need_freq_update)
		return ag_policy->next_freq;

	ag_policy->need_freq_update = false;
	ag_policy->cached_raw_freq = freq;
	freq = cpufreq_driver_resolve_freq(policy, freq);
	trace_cpu_frequency_aigov(util, freq, policy->cpu);

	return freq;
}

static
void aigov_get_target_util(unsigned long *util, unsigned long *max, int cpu)
{
	unsigned long max_cap = arch_scale_cpu_capacity(NULL, cpu);

	struct rq *rq = cpu_rq(cpu);
	struct mlt *mlt = &rq->mlt;
	unsigned long pelt_util, pelt_max;
	int util_ratio, util_boost, active_ratio = 0;

	/* get basic pelt util with uclamp */
   	pelt_util = ml_cpu_util(cpu);
   	pelt_util = uclamp_util_with(rq, pelt_util, NULL);

   	/* boost util with schedtune */
   	pelt_util += schedtune_cpu_margin(pelt_util, cpu);

   	/* get tipping point of util */
	pelt_util = pelt_util + (pelt_util >> 2);
	pelt_util = min(pelt_util, max_cap);
	pelt_max = max_cap;

	/* get pelt active ratio */
	if (unlikely(mlt->period_start == 0))
		goto skip_active_ratio;

	util_ratio = pelt_util * SCHED_CAPACITY_SCALE / pelt_max;
	util_boost = mlt_art_boost(mlt);

	if (mlt_art_last_boost_time(mlt) && util_ratio < util_boost) {
		*util = util_boost;
		*max = SCHED_CAPACITY_SCALE;
		goto out;
	}

	if (util_ratio > mlt_art_boost_limit(mlt))
		goto skip_active_ratio;

	if (mlt_art_high_patten(mlt)) {
		*util = 0;
		*max = SCHED_CAPACITY_SCALE;
		goto out;
	}

	active_ratio = max(mlt->active_ratio_recent, mlt->period[mlt->cur_period]);

	*util = max(active_ratio, mlt->active_ratio_est);
	*util = min_t(unsigned long, *util, (unsigned long) mlt_art_boost_limit(mlt));
	*max = SCHED_CAPACITY_SCALE;

	if (util_ratio <= *util)
		goto out;

skip_active_ratio:
	*util = pelt_util;
	*max = pelt_max;
out:
	trace_ems_cpu_active_ratio_util_stat(cpu, *util, (unsigned long) util_ratio);
}

static void aigov_set_iowait_boost(struct aigov_cpu *ai_cpu, u64 time,
				   unsigned int flags)
{
	struct aigov_policy *ag_policy = ai_cpu->ag_policy;
	int iowait_boost_enabled = ag_policy->tunables->iowait_boost_enable;

	/* temporarily enable iowait_boost if device is busy */
	if (sysbusy_state > SYSBUSY_STATE0)
		iowait_boost_enabled = 1;

	if (!iowait_boost_enabled || ag_policy->fb_panel_blank)
		return;

	if (ai_cpu->iowait_boost) {
		s64 delta_ns = time - ai_cpu->last_update;

		/* Clear iowait_boost if the CPU apprears to have been idle. */
		if (delta_ns > TICK_NSEC) {
			ai_cpu->iowait_boost = 0;
			ai_cpu->iowait_boost_pending = false;
		}
    }

	if (flags & SCHED_CPUFREQ_IOWAIT) {
		if (ai_cpu->iowait_boost_pending)
			return;

		ai_cpu->iowait_boost_pending = true;

		if (ai_cpu->iowait_boost) {
			ai_cpu->iowait_boost <<= 1;
			if (ai_cpu->iowait_boost > ai_cpu->iowait_boost_max)
				ai_cpu->iowait_boost = ai_cpu->iowait_boost_max;
		} else {
			ai_cpu->iowait_boost = ai_cpu->ag_policy->policy->min;
		}
	}
}

static void aigov_iowait_boost(struct aigov_cpu *ai_cpu, unsigned long *util,
			       unsigned long *max)
{
	unsigned int boost_util, boost_max;

	if (!ai_cpu->iowait_boost)
		return;

	if (ai_cpu->iowait_boost_pending) {
		ai_cpu->iowait_boost_pending = false;
	} else {
		ai_cpu->iowait_boost >>= 1;
		if (ai_cpu->iowait_boost < ai_cpu->ag_policy->policy->min) {
			ai_cpu->iowait_boost = 0;
			return;
		}
	}

	boost_util = ai_cpu->iowait_boost;
	boost_max = ai_cpu->iowait_boost_max;

	if (*util * boost_max < *max * boost_util) {
		*util = boost_util;
		*max = boost_max;
	}
}

static unsigned int aigov_next_freq(struct aigov_cpu *ai_cpu, u64 time)
{
	struct aigov_policy *ag_policy = ai_cpu->ag_policy;
	struct cpufreq_policy *policy = ag_policy->policy;
	unsigned long util = 0, max = 1;
	unsigned int j;

	for_each_cpu_and(j, policy->related_cpus, cpu_online_mask) {
		struct aigov_cpu *j_ai_cpu = &per_cpu(aigov_cpu, j);
		unsigned long j_util, j_max;
		s64 delta_ns;

		/*
		 * If the CPU utilization was last updated before the previous
		 * frequency update and the time elapsed between the last update
		 * of the CPU utilization and the last frequency update is long
		 * enough, don't take the CPU into account as it probably is
		 * idle now (and clear iowait_boost for it).
		 */
		delta_ns = time - j_ai_cpu->last_update;
		if (delta_ns > TICK_NSEC) {
			j_ai_cpu->iowait_boost = 0;
			j_ai_cpu->iowait_boost_pending = false;
			continue;
		}
		if (j_ai_cpu->flags & SCHED_CPUFREQ_DL) {
			/* clear cache when it's bypassed */
			ag_policy->cached_raw_freq = 0;
			return policy->cpuinfo.max_freq;
		}

		j_util = j_ai_cpu->util;
		j_max = j_ai_cpu->max;
		if (j_util * max > j_max * util) {
			util = j_util;
			max = j_max;
		}

		aigov_iowait_boost(j_ai_cpu, &util, &max);
	}

	return aigov_get_next_freq(ai_cpu, ag_policy, util, max);
}

static void aigov_update(struct update_util_data *hook, u64 time,
				unsigned int flags)
{
	struct aigov_cpu *ai_cpu = container_of(hook, struct aigov_cpu, update_util);
	struct aigov_policy *ag_policy = ai_cpu->ag_policy;
	unsigned long util, max;
	unsigned int next_f;

	aigov_get_target_util(&util, &max, ai_cpu->cpu);

	raw_spin_lock(&ag_policy->update_lock);

	ai_cpu->cached_util = min(max, ai_cpu->max ? mult_frac(ai_cpu->util, max, ai_cpu->max) : ai_cpu->util);
	ai_cpu->util = util;
	ai_cpu->max = max;
	ai_cpu->flags = flags;

	aigov_inferrer_adapt_util(ai_cpu);
	aigov_set_iowait_boost(ai_cpu, time, flags);
	ai_cpu->last_update = time;

	if (aigov_should_update_freq(ag_policy, time)) {
		if (flags & SCHED_CPUFREQ_DL) {
			/* clear cache when it's bypassed */
			ag_policy->cached_raw_freq = 0;
			next_f = ag_policy->policy->cpuinfo.max_freq;
		} else {
			next_f = aigov_next_freq(ai_cpu, time);
		}

		aigov_update_commit(ag_policy, time, next_f);
	}

	raw_spin_unlock(&ag_policy->update_lock);
}

static void aigov_work(struct kthread_work *work)
{
	struct aigov_policy *ag_policy = container_of(work, struct aigov_policy, work);
	unsigned int freq;
	unsigned long flags;

	/*
	 * Hold ag_policy->update_lock shortly to handle the case where:
	 * incase ag_policy->next_freq is read here, and then updated by
	 * aigov_update just before work_in_progress is set to false
	 * here, we may miss queueing the new update.
	 *
	 * Note: If a work was queued after the update_lock is released,
	 * aigov_work will just be called again by kthread_work code; and the
	 * request will be proceed before the aigov thread sleeps.
	 */
	raw_spin_lock_irqsave(&ag_policy->update_lock, flags);
	freq = ag_policy->next_freq;
	ag_policy->work_in_progress = false;
	raw_spin_unlock_irqrestore(&ag_policy->update_lock, flags);

	down_write(&ag_policy->policy->rwsem);
	mutex_lock(&ag_policy->work_lock);
	__cpufreq_driver_target(ag_policy->policy, freq, CPUFREQ_RELATION_L);
	mutex_unlock(&ag_policy->work_lock);
	up_write(&ag_policy->policy->rwsem);
}

static void aigov_irq_work(struct irq_work *irq_work)
{
	struct aigov_policy *ag_policy;

	ag_policy = container_of(irq_work, struct aigov_policy, irq_work);

	/*
	 * For RT and deadline tasks, the adaptive governor shoots the
	 * frequency to maximum. Special care must be taken to ensure that this
	 * kthread doesn't result in the same behavior.
	 *
	 * This is (mostly) guaranteed by the work_in_progress flag. The flag is
	 * updated only at the end of the aigov_work() function and before that
	 * the adaptive governor rejects all other frequency scaling requests.
	 *
	 * There is a very rare case though, where the RT thread yields right
	 * after the work_in_progress flag is cleared. The effects of that are
	 * neglected for now.
	 */
	kthread_queue_work(&ag_policy->worker, &ag_policy->work);
}

/************************** sysfs interface ************************/

static struct aigov_tunables *global_tunables;
static DEFINE_MUTEX(global_tunables_lock);
static DEFINE_MUTEX(min_rate_lock);

static
inline struct aigov_tunables *to_aigov_tunables(struct gov_attr_set *attr_set)
{
	return container_of(attr_set, struct aigov_tunables, attr_set);
}

static
void update_min_rate_limit_ns(struct aigov_policy *ag_policy)
{
	mutex_lock(&min_rate_lock);
	ag_policy->min_rate_limit_ns = min(ag_policy->up_rate_delay_ns,
					   ag_policy->down_rate_delay_ns);
	mutex_unlock(&min_rate_lock);
}

static
ssize_t up_rate_limit_us_show(struct gov_attr_set *attr_set, char *buf)
{
	struct aigov_tunables *tunables = to_aigov_tunables(attr_set);

	return sprintf(buf, "%u\n", tunables->up_rate_limit_us);
}

static
ssize_t down_rate_limit_us_show(struct gov_attr_set *attr_set, char *buf)
{
	struct aigov_tunables *tunables = to_aigov_tunables(attr_set);

	return sprintf(buf, "%u\n", tunables->down_rate_limit_us);
}

static
ssize_t up_rate_limit_us_store(struct gov_attr_set *attr_set,
				      const char *buf, size_t count)
{
	struct aigov_tunables *tunables = to_aigov_tunables(attr_set);
	struct aigov_policy *ag_policy;
	unsigned int rate_limit_us;

	if (kstrtouint(buf, 10, &rate_limit_us))
		return -EINVAL;

	tunables->up_rate_limit_us = rate_limit_us;

	list_for_each_entry(ag_policy, &attr_set->policy_list, tunables_hook) {
		ag_policy->up_rate_delay_ns = rate_limit_us * NSEC_PER_USEC;
		update_min_rate_limit_ns(ag_policy);
	}

	return count;
}

static
ssize_t down_rate_limit_us_store(struct gov_attr_set *attr_set,
					const char *buf, size_t count)
{
	struct aigov_tunables *tunables = to_aigov_tunables(attr_set);
	struct aigov_policy *ag_policy;
	unsigned int rate_limit_us;

	if (kstrtouint(buf, 10, &rate_limit_us))
		return -EINVAL;

	tunables->down_rate_limit_us = rate_limit_us;

	list_for_each_entry(ag_policy, &attr_set->policy_list, tunables_hook) {
		ag_policy->down_rate_delay_ns = rate_limit_us * NSEC_PER_USEC;
		update_min_rate_limit_ns(ag_policy);
	}

	return count;
}

static
ssize_t iowait_boost_enable_show(struct gov_attr_set *attr_set,
					char *buf)
{
	struct aigov_tunables *tunables = to_aigov_tunables(attr_set);

	return sprintf(buf, "%u\n", tunables->iowait_boost_enable);
}

static
ssize_t iowait_boost_enable_store(struct gov_attr_set *attr_set,
					 const char *buf, size_t count)
{
	struct aigov_tunables *tunables = to_aigov_tunables(attr_set);
	bool enable;

	if (kstrtobool(buf, &enable))
		return -EINVAL;

	tunables->iowait_boost_enable = enable;

	return count;
}

static struct governor_attr up_rate_limit_us = __ATTR_RW(up_rate_limit_us);
static struct governor_attr down_rate_limit_us = __ATTR_RW(down_rate_limit_us);
static struct governor_attr iowait_boost_enable = __ATTR_RW(iowait_boost_enable);

static struct attribute *aigov_attributes[] = {
	&up_rate_limit_us.attr,
	&down_rate_limit_us.attr,
	&iowait_boost_enable.attr,
	NULL
};

static
void aigov_tunables_free(struct kobject *kobj)
{
	struct gov_attr_set *attr_set = container_of(kobj, struct gov_attr_set, kobj);

	kfree(to_aigov_tunables(attr_set));
}

static struct kobj_type aigov_tunables_ktype = {
	.default_attrs = aigov_attributes,
	.sysfs_ops = &governor_sysfs_ops,
	.release = &aigov_tunables_free,
};

/********************** cpufreq governor interface *********************/

static struct cpufreq_governor adaptive_gov;

static
struct aigov_policy *aigov_policy_alloc(struct cpufreq_policy *policy)
{
	struct aigov_policy *ag_policy;

	ag_policy = kzalloc(sizeof(*ag_policy), GFP_KERNEL);
	if (!ag_policy)
		return NULL;

	ag_policy->policy = policy;
	raw_spin_lock_init(&ag_policy->update_lock);
	return ag_policy;
}

static inline
void aigov_policy_free(struct aigov_policy *ag_policy)
{
	kfree(ag_policy);
}

static
int aigov_kthread_create(struct aigov_policy *ag_policy)
{
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = AIGOV_KTHREAD_PRIORITY };
	struct cpufreq_policy *policy = ag_policy->policy;
	int ret;

	/* kthread only required for slow path */
	if (policy->fast_switch_enabled)
		return 0;

	kthread_init_work(&ag_policy->work, aigov_work);
	kthread_init_worker(&ag_policy->worker);
	thread = kthread_create(kthread_worker_fn, &ag_policy->worker,
				"aigov:%d",
				cpumask_first(policy->related_cpus));
	if (IS_ERR(thread)) {
		pr_err("failed to create aigov thread: %ld\n", PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		pr_warn("%s: failed to set SCHED_FIFO\n", __func__);
		return ret;
	}

	ag_policy->thread = thread;

	/* Kthread is bound to all CPUs by default */
	if (!policy->dvfs_possible_from_any_cpu)
		kthread_bind_mask(thread, cpu_coregroup_mask(0));

	init_irq_work(&ag_policy->irq_work, aigov_irq_work);
	mutex_init(&ag_policy->work_lock);

	wake_up_process(thread);

	return 0;
}

static
void aigov_kthread_stop(struct aigov_policy *ag_policy)
{
	/* kthread only required for slow path */
	if (ag_policy->policy->fast_switch_enabled)
		return;

	kthread_flush_worker(&ag_policy->worker);
	kthread_stop(ag_policy->thread);
	mutex_destroy(&ag_policy->work_lock);
}

static
struct aigov_tunables *aigov_tunables_alloc(struct aigov_policy *ag_policy)
{
	struct aigov_tunables *tunables;

	tunables = kzalloc(sizeof(*tunables), GFP_KERNEL);
	if (tunables) {
		gov_attr_set_init(&tunables->attr_set, &ag_policy->tunables_hook);
		if (!have_governor_per_policy())
			global_tunables = tunables;
	}
	return tunables;
}

static
void aigov_clear_global_tunables(void)
{
	if (!have_governor_per_policy())
		global_tunables = NULL;
}

static
int fb_notifier_cb(struct notifier_block *nb, unsigned long action,
			  void *data)
{
	struct aigov_policy *ag_policy = container_of(nb, struct aigov_policy, fb_notifier);
	int *blank = ((struct fb_event *)data)->data;

	if (action != FB_EARLY_EVENT_BLANK)
		return NOTIFY_OK;

	if (*blank == FB_BLANK_UNBLANK)
		ag_policy->fb_panel_blank = false;
	else
		ag_policy->fb_panel_blank = true;

	return NOTIFY_OK;
}

static
int aigov_init(struct cpufreq_policy *policy)
{
	struct aigov_policy *ag_policy;
	struct aigov_tunables *tunables;
	int ret = 0;

	/* State should be equivalent to EXIT */
	if (policy->governor_data)
		return -EBUSY;

	cpufreq_enable_fast_switch(policy);

	/* restore saved ag_policy */
	ag_policy = aigov_restore_policy(policy);
	if (ag_policy)
		goto tunables_init;

	ag_policy = aigov_policy_alloc(policy);
	if (!ag_policy) {
		ret = -ENOMEM;
		goto disable_fast_switch;
	}

	ret = aigov_kthread_create(ag_policy);
	if (ret)
		goto free_ag_policy;

tunables_init:
	mutex_lock(&global_tunables_lock);

	if (global_tunables) {
		if (WARN_ON(have_governor_per_policy())) {
			ret = -EINVAL;
			goto stop_kthread;
		}
		policy->governor_data = ag_policy;
		ag_policy->tunables = global_tunables;

		gov_attr_set_get(&global_tunables->attr_set, &ag_policy->tunables_hook);
		goto out;
	}

	tunables = aigov_tunables_alloc(ag_policy);
	if (!tunables) {
		ret = -ENOMEM;
		goto stop_kthread;
	}

	if (cpumask_test_cpu(policy->cpu, cpu_perf_mask)) {
		tunables->up_rate_limit_us =
					CONFIG_ENERGY_ADAPTIVE_UP_RATE_LIMIT_FAST;
		tunables->down_rate_limit_us =
					CONFIG_ENERGY_ADAPTIVE_DOWN_RATE_LIMIT_FAST;
	} else if (cpumask_test_cpu(policy->cpu, cpu_lp_mask)) {
		tunables->up_rate_limit_us =
					CONFIG_ENERGY_ADAPTIVE_UP_RATE_LIMIT_SLOW;
		tunables->down_rate_limit_us =
					CONFIG_ENERGY_ADAPTIVE_DOWN_RATE_LIMIT_SLOW;
	}

	ag_policy->be_stochastic = false;
	tunables->iowait_boost_enable = policy->iowait_boost_enable;

	policy->governor_data = ag_policy;
	ag_policy->tunables = tunables;
	ag_policy->fb_panel_blank = false;

	ret = kobject_init_and_add(&tunables->attr_set.kobj, &aigov_tunables_ktype,
				   get_governor_parent_kobj(policy), "%s",
				   adaptive_gov.name);
	if (ret)
		goto fail;

out:
	mutex_unlock(&global_tunables_lock);

	ag_policy->fb_notifier.notifier_call = fb_notifier_cb;
	ag_policy->fb_notifier.priority = INT_MAX;
	ret = fb_register_client(&ag_policy->fb_notifier);
	if (ret) {
		pr_err("Failed to register fb notifier, err: %d\n", ret);
		goto fail;
	}

	return 0;

fail:
	kobject_put(&tunables->attr_set.kobj);
	policy->governor_data = NULL;
	aigov_clear_global_tunables();

stop_kthread:
	aigov_kthread_stop(ag_policy);
	mutex_unlock(&global_tunables_lock);

free_ag_policy:
	aigov_policy_free(ag_policy);

disable_fast_switch:
	cpufreq_disable_fast_switch(policy);

	pr_err("initialization failed (error %d)\n", ret);
	return ret;
}

static
void aigov_exit(struct cpufreq_policy *policy)
{
	struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, policy->cpu);
	struct aigov_policy *ag_policy = policy->governor_data;
	struct aigov_tunables *tunables = ag_policy->tunables;
	unsigned int count;

	mutex_lock(&global_tunables_lock);

	count = gov_attr_set_put(&tunables->attr_set, &ag_policy->tunables_hook);
	policy->governor_data = NULL;
	if (!count)
		aigov_clear_global_tunables();

	if (ai_cpu->util_vessel) {
		ai_cpu->util_vessel->finalizer(ai_cpu->util_vessel);
		ffsi_obj_destructor(ai_cpu->util_vessel);
		ai_cpu->util_vessel = NULL;
	}

	ag_policy->be_stochastic = false;

	if (aigov_save_policy(ag_policy))
		goto out;

	fb_unregister_client(&ag_policy->fb_notifier);
	aigov_kthread_stop(ag_policy);
	aigov_policy_free(ag_policy);

out:
	mutex_unlock(&global_tunables_lock);

	cpufreq_disable_fast_switch(policy);
}

static
int aigov_start(struct cpufreq_policy *policy)
{
	struct aigov_policy *ag_policy = policy->governor_data;
	unsigned int cpu;
	char alias[FFSI_ALIAS_LEN];

	ag_policy->up_rate_delay_ns =
		ag_policy->tunables->up_rate_limit_us * NSEC_PER_USEC;
	ag_policy->down_rate_delay_ns =
		ag_policy->tunables->down_rate_limit_us * NSEC_PER_USEC;
	update_min_rate_limit_ns(ag_policy);
	ag_policy->last_freq_update_time = 0;
	ag_policy->next_freq = 0;
	ag_policy->work_in_progress = false;
	ag_policy->limits_changed = false;
	ag_policy->need_freq_update = false;
	ag_policy->cached_raw_freq = 0;

	for_each_cpu(cpu, policy->cpus) {
		struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);

		if (!ag_policy->be_stochastic) {
			sprintf(alias, "govern%d", cpu);
			memset(ai_cpu, 0, sizeof(*ai_cpu));
			ai_cpu->util_vessel =
				ffsi_obj_creator(alias,
						 UTILAVG_FFSI_VARIANCE,
						 policy->cpuinfo.max_freq,
						 policy->cpuinfo.min_freq,
						 &elasticity_cpufreq);
			if (ai_cpu->util_vessel->initializer(ai_cpu->util_vessel) < 0) {
				ai_cpu->util_vessel->finalizer(ai_cpu->util_vessel);
				ffsi_obj_destructor(ai_cpu->util_vessel);
				ai_cpu->util_vessel = NULL;
			}
		} else {
			struct ffsi_class *vptr = ai_cpu->util_vessel;
			memset(ai_cpu, 0, sizeof(*ai_cpu));
			ai_cpu->util_vessel = vptr;
		}

		ai_cpu->cpu = cpu;
		ai_cpu->ag_policy = ag_policy;
		ai_cpu->flags = 0;
		aigov_start_slack(cpu);
		ai_cpu->iowait_boost_max = policy->cpuinfo.max_freq;
	}

	ag_policy->be_stochastic = true;

	for_each_cpu(cpu, policy->cpus) {
		struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);
		cpufreq_add_update_util_hook(cpu, &ai_cpu->update_util, aigov_update);
	}
	return 0;
}

static
void aigov_stop(struct cpufreq_policy *policy)
{
	struct aigov_policy *ag_policy = policy->governor_data;
	unsigned int cpu;

	for_each_cpu(cpu, policy->cpus) {
		aigov_stop_slack(cpu);
		cpufreq_remove_update_util_hook(cpu);
	}

	synchronize_sched();

	for_each_cpu(cpu, policy->cpus) {
		struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);
		if (ai_cpu->util_vessel) {
			ai_cpu->util_vessel->stopper(ai_cpu->util_vessel);
		}
	}

	if (!policy->fast_switch_enabled) {
		irq_work_sync(&ag_policy->irq_work);
	}
}

static void aigov_limits(struct cpufreq_policy *policy)
{
	struct aigov_policy *ag_policy = policy->governor_data;

	mutex_lock(&global_tunables_lock);

	if (!ag_policy) {
		mutex_unlock(&global_tunables_lock);
		return;
	}

	if (!policy->fast_switch_enabled) {
		mutex_lock(&ag_policy->work_lock);
		cpufreq_policy_apply_limits(policy);
		mutex_unlock(&ag_policy->work_lock);
	}

	aigov_update_min(policy);

	ag_policy->limits_changed = true;

	mutex_unlock(&global_tunables_lock);
}

static struct cpufreq_governor adaptive_gov = {
	.name = "energy_adaptive",
	.owner = THIS_MODULE,
	.dynamic_switching = true,
	.init = aigov_init,
	.exit = aigov_exit,
	.start = aigov_start,
	.stop = aigov_stop,
	.limits = aigov_limits,
};

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ENERGY_ADAPTIVE
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return &adaptive_gov;
}
#endif

static
void aigov_update_min(struct cpufreq_policy *policy)
{
	struct aigov_cpu *ai_cpu;
	int cpu, max_cap;
	int min_cap;

	max_cap = arch_scale_cpu_capacity(NULL, policy->cpu);

	/* min_cap is minimum value making higher frequency than policy->min */
	min_cap = max_cap * policy->min / policy->max;
	min_cap = (min_cap * 4 / 5) + 1;

	for_each_cpu(cpu, policy->cpus) {
		ai_cpu = &per_cpu(aigov_cpu, cpu);
		ai_cpu->slack_min_util = min_cap;
	}
}

static
void aigov_nop_timer(unsigned long data)
{
	/*
	 * The purpose of the slack timer is to wake up the CPU from IDLE, in order
	 * to decrease its frequency if it is not set to minimum already.
	 *
	 * This is important for platforms where CPU with higher frequencies
	 * consume higher power even at IDLE.
	 */
	trace_aigov_slack_func(smp_processor_id());
}

static
void aigov_start_slack(int cpu)
{
	struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);

	if (!ai_cpu->slack_enabled)
		return;

	ai_cpu->slack_min_util = ULONG_MAX;
	ai_cpu->slack_started = true;
}

static
void aigov_stop_slack(int cpu)
{
	struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);

	ai_cpu->slack_started = false;
	if (timer_pending(&ai_cpu->slack_timer))
		del_timer_sync(&ai_cpu->slack_timer);
}

static inline
s64 aigov_get_next_event_time(unsigned int cpu)
{
	return ktime_to_ms(ktime_sub(*(get_next_event_cpu(cpu)), ktime_get()));
}

static
int aigov_need_slack_timer(unsigned int cpu)
{
	struct aigov_cpu *ai_gov = &per_cpu(aigov_cpu, cpu);

	if (schedtune_cpu_boost(cpu))
		return 0;

	if (ai_gov->util > ai_gov->slack_min_util &&
		aigov_get_next_event_time(cpu) > ai_gov->slack_expired_time_ms)
		return 1;

	return 0;
}

static int aigov_pm_notifier(struct notifier_block *self,
						unsigned long action, void *v)
{
	unsigned int cpu = raw_smp_processor_id();
	struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);
	struct timer_list *timer = &ai_cpu->slack_timer;

	if (!ai_cpu->slack_started)
		return NOTIFY_OK;

	switch (action) {
	case CPU_PM_ENTER_PREPARE:
		if (timer_pending(timer))
			del_timer_sync(timer);

		if (aigov_need_slack_timer(cpu)) {
			timer->expires = jiffies + msecs_to_jiffies(ai_cpu->slack_expired_time_ms);
			add_timer_on(timer, cpu);
			trace_aigov_slack(cpu, ai_cpu->util, ai_cpu->slack_min_util, action, 1);
		}
		break;

	case CPU_PM_ENTER:
		if (timer_pending(timer) && !aigov_need_slack_timer(cpu)) {
			del_timer_sync(timer);
			trace_aigov_slack(cpu, ai_cpu->util, ai_cpu->slack_min_util, action, -1);
		}
		break;

	case CPU_PM_EXIT_POST:
		if (timer_pending(timer) && (time_after(timer->expires, jiffies))) {
			del_timer_sync(timer);
			trace_aigov_slack(cpu, ai_cpu->util, ai_cpu->slack_min_util, action, -1);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block aigov_pm_notifier_block = {
	.notifier_call = aigov_pm_notifier,
};

static int aigov_find_pm_qos_class(int pm_qos_class)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);

		if ((ai_cpu->qos_min_class == pm_qos_class) &&
				cpumask_test_cpu(cpu, cpu_active_mask))
			return cpu;
	}

	pr_err("cannot find cpu of PM QoS class\n");
	return -EINVAL;
}

static int aigov_pm_qos_callback(struct notifier_block *nb,
					unsigned long val, void *v)
{
	struct aigov_cpu *ai_cpu;
	struct cpufreq_policy *policy;
	int pm_qos_class = *((int *)v);
	unsigned int next_freq;
	int cpu;

	cpu = aigov_find_pm_qos_class(pm_qos_class);
	if (cpu < 0)
		return NOTIFY_BAD;

	ai_cpu = &per_cpu(aigov_cpu, cpu);
	if (!ai_cpu || !ai_cpu->ag_policy || !ai_cpu->ag_policy->policy)
		return NOTIFY_BAD;

	next_freq = ai_cpu->ag_policy->next_freq;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return NOTIFY_BAD;

	if (val >= policy->cur) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

	cpufreq_driver_target(policy, next_freq, CPUFREQ_RELATION_L);

	cpufreq_cpu_put(policy);

	return NOTIFY_OK;
}

static struct notifier_block aigov_min_qos_notifier = {
	.notifier_call = aigov_pm_qos_callback,
	.priority = INT_MIN,
};

/****************************************************************/
/*		  sysbusy state change notifier			*/
/****************************************************************/
static int aigov_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	sysbusy_state = state;

	return NOTIFY_OK;
}

static struct notifier_block aigov_sysbusy_notifier = {
	.notifier_call = aigov_sysbusy_notifier_call,
};

static int __init aigov_parse_dt(struct device_node *dn, int cpu)
{
	struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);

	/* parse slack timer information from DT */
	if (of_property_read_u32(dn, "slack-enabled", &ai_cpu->slack_enabled))
		return -EINVAL;
	if (ai_cpu->slack_enabled)
		if (of_property_read_u32(dn, "slack-expired-time", &ai_cpu->slack_expired_time_ms))
			ai_cpu->slack_expired_time_ms = DEFAULT_EXPIRED_TIME;

	/* parse QoS min class info from DT */
	if (of_property_read_u32(dn, "qos-min-class", &ai_cpu->qos_min_class))
		return -EINVAL;

	return 0;
}

static void __init aigov_cpufreq_init(void)
{
	int cpu, ret;
	struct device_node *dn = NULL;
	const char *buf;

	while ((dn = of_find_node_by_type(dn, "aigov-domain"))) {
		struct cpumask shared_mask;
		/* Get shared cpus */
		ret = of_property_read_string(dn, "shared-cpus", &buf);
		if (ret)
			goto exit;

		cpulist_parse(buf, &shared_mask);
		for_each_cpu(cpu, &shared_mask)
			if (aigov_parse_dt(dn, cpu))
				goto exit;
	}

	for_each_possible_cpu(cpu) {
		struct aigov_cpu *ai_cpu = &per_cpu(aigov_cpu, cpu);

		if (!ai_cpu->slack_enabled)
			continue;

		/* Initialize slack timer */
		init_timer_pinned(&ai_cpu->slack_timer);
		ai_cpu->slack_timer.function = aigov_nop_timer;
	}

	pm_qos_add_notifier(PM_QOS_CLUSTER0_FREQ_MIN, &aigov_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER1_FREQ_MIN, &aigov_min_qos_notifier);
	cpu_pm_register_notifier(&aigov_pm_notifier_block);
	sysbusy_register_notifier(&aigov_sysbusy_notifier);

	return;
exit:
	pr_info("%s: failed to initialized slack_timer, pm_qos handler\n", __func__);
}

static int __init aigov_register(void)
{
	aigov_cpufreq_init();
	
	return cpufreq_register_governor(&adaptive_gov);
}
core_initcall(aigov_register);
