/*
 * thread group band
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ems.h>
#include <linux/sched/signal.h>
#include <trace/events/ems.h>

#include "ems.h"
#include "../sched.h"

static struct task_band *lookup_band(struct task_struct *p)
{
	struct task_band *band;

	rcu_read_lock();
	band = rcu_dereference(p->band);
	rcu_read_unlock();

	if (!band)
		return NULL;

	return band;
}

int band_play_cpu(struct task_struct *p)
{
	struct task_band *band;
	int cpu, min_cpu = -1;
	unsigned long min_util = ULONG_MAX;

	band = lookup_band(p);
	if (!band)
		return -1;

	for_each_cpu(cpu, &band->playable_cpus) {
		if (!cpu_rq(cpu)->nr_running)
			return cpu;

		if (cpu_util(cpu) < min_util) {
			min_cpu = cpu;
			min_util = cpu_util(cpu);
		}
	}

	return min_cpu;
}

static void pick_playable_cpus(struct task_band *band)
{
	cpumask_clear(&band->playable_cpus);

	/* pick condition should be fixed */
	if (band->util < 442) // LIT up-threshold * 2
		cpumask_and(&band->playable_cpus, cpu_online_mask, cpu_coregroup_mask(0));
	else if (band->util < 1260) // MED up-threshold * 2
		cpumask_and(&band->playable_cpus, cpu_online_mask, cpu_coregroup_mask(4));
	else
		cpumask_and(&band->playable_cpus, cpu_online_mask, cpu_coregroup_mask(6));
}

static unsigned long out_of_time = 100000000;	/* 100ms */

/* This function should be called protected with band->lock */
static void __update_band(struct task_band *band, unsigned long now)
{
	struct task_struct *task;
	unsigned long util_sum = 0;

	list_for_each_entry(task, &band->members, band_members) {
		if (now - task->se.avg.last_update_time > out_of_time)
			continue;
		util_sum += task_util(task);
	}

	band->util = util_sum;
	band->last_update_time = now;

	pick_playable_cpus(band);

	task = list_first_entry(&band->members, struct task_struct, band_members);
	trace_ems_update_band(band->id, band->util, band->member_count,
		*(unsigned int *)cpumask_bits(&band->playable_cpus));
}

static int update_interval = 20000000;	/* 20ms */

void update_band(struct task_struct *p, long old_util)
{
	struct task_band *band;
	unsigned long now = cpu_rq(0)->clock_task;

	band = lookup_band(p);
	if (!band)
		return;

	/*
	 * Updates the utilization of the band only when it has been enough time
	 * to update the utilization of the band, or when the utilization of the
	 * task changes abruptly.
	 */
	if (now - band->last_update_time >= update_interval ||
	    (old_util >= 0 && abs(old_util - task_util(p)) > (SCHED_CAPACITY_SCALE >> 4))) {
		raw_spin_lock(&band->lock);
		__update_band(band, now);
		raw_spin_unlock(&band->lock);
	}
}

#define MAX_NUM_BAND_ID		20
static struct task_band *bands[MAX_NUM_BAND_ID];

DEFINE_RWLOCK(band_rwlock);

#define band_playing(band)	(band->tgid >= 0)
static void join_band(struct task_struct *p)
{
	struct task_band *band;
	int pos, empty = -1;
	char event[30] = "join band";

	if (lookup_band(p))
		return;

	write_lock(&band_rwlock);

	/*
	 * Find the band assigned to the tasks's thread group in the
	 * band pool. If there is no band assigend to thread group, it
	 * indicates that the task is the first one in the thread group
	 * to join the band. In this case, assign the first empty band
	 * in the band pool to the thread group.
	 */
	for (pos = 0; pos < MAX_NUM_BAND_ID; pos++) {
		band = bands[pos];

		if (!band_playing(band)) {
			if (empty < 0)
				empty = pos;
			continue;
		}

		if (p->tgid == band->tgid)
			break;
	}

	/* failed to find band, organize the new band */
	if (pos == MAX_NUM_BAND_ID)
		band = bands[empty];

	raw_spin_lock(&band->lock);
	if (!band_playing(band))
		band->tgid = p->tgid;
	list_add(&p->band_members, &band->members);
	rcu_assign_pointer(p->band, band);
	band->member_count++;
	trace_ems_manage_band(p, band->id, event);

	__update_band(band, cpu_rq(0)->clock_task);
	raw_spin_unlock(&band->lock);

	write_unlock(&band_rwlock);
}

static void leave_band(struct task_struct *p)
{
	struct task_band *band;
	char event[30] = "leave band";

	if (!lookup_band(p))
		return;

	write_lock(&band_rwlock);
	band = p->band;

	raw_spin_lock(&band->lock);
	list_del_init(&p->band_members);
	rcu_assign_pointer(p->band, NULL);
	band->member_count--;
	trace_ems_manage_band(p, band->id, event);

	/* last member of band, band split up */
	if (list_empty(&band->members)) {
		band->tgid = -1;
		cpumask_clear(&band->playable_cpus);
	}

	__update_band(band, cpu_rq(0)->clock_task);
	raw_spin_unlock(&band->lock);

	write_unlock(&band_rwlock);
}

void sync_band(struct task_struct *p, bool join)
{
	if (join)
		join_band(p);
	else
		leave_band(p);
}

void newbie_join_band(struct task_struct *newbie)
{
	unsigned long flags;
	struct task_band *band;
	struct task_struct *leader = newbie->group_leader;
	char event[30] = "newbie join band";

	if (thread_group_leader(newbie))
		return;

	write_lock_irqsave(&band_rwlock, flags);

	band = lookup_band(leader);
	if (!band || newbie->band) {
		write_unlock_irqrestore(&band_rwlock, flags);
		return;
	}

	raw_spin_lock(&band->lock);
	list_add(&newbie->band_members, &band->members);
	rcu_assign_pointer(newbie->band, band);
	band->member_count++;
	trace_ems_manage_band(newbie, band->id, event);
	raw_spin_unlock(&band->lock);

	write_unlock_irqrestore(&band_rwlock, flags);
}

int alloc_bands(void)
{
	struct task_band *band;
	int pos, ret, i;

	for (pos = 0; pos < MAX_NUM_BAND_ID; pos++) {
		band = kzalloc(sizeof(*band), GFP_KERNEL);
		if (!band) {
			ret = -ENOMEM;
			goto fail;
		}

		band->id = pos;
		band->tgid = -1;
		raw_spin_lock_init(&band->lock);
		INIT_LIST_HEAD(&band->members);
		band->member_count = 0;
		cpumask_clear(&band->playable_cpus);

		bands[pos] = band;
	}

	return 0;

fail:
	for (i = pos - 1; i >= 0; i--) {
		kfree(bands[i]);
		bands[i] = NULL;
	}

	return ret;
}
