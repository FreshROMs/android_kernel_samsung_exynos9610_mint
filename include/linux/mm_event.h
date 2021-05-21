/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MM_EVENT_H
#define _LINUX_MM_EVENT_H

#include <linux/types.h>
#include <linux/ktime.h>

enum mm_event_type {
	MM_MIN_FAULT = 0,
	MM_MAJ_FAULT,
	MM_READ_IO,
	MM_COMPACTION,
	MM_RECLAIM,
	MM_SWP_FAULT,
	MM_KERN_ALLOC,
	MM_TYPE_NUM,
};

struct mm_event_task {
	unsigned int count;
	unsigned int max_lat;
	u64 accm_lat;
} __attribute__ ((packed));

struct task_struct;

#ifdef CONFIG_MM_EVENT_STAT
void mm_event_task_init(struct task_struct *tsk);
void mm_event_start(ktime_t *time);
void mm_event_end(enum mm_event_type event, ktime_t start);
void mm_event_count(enum mm_event_type event, int count);
#else
static inline void mm_event_task_init(struct task_struct *tsk) {}
static inline void mm_event_start(ktime_t *time) {}
static inline void mm_event_end(enum mm_event_type event, ktime_t start) {}
static inline void mm_event_count(enum mm_event_type event, int count) {}
#endif /* _LINUX_MM_EVENT_H */
#endif
