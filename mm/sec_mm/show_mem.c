// SPDX-License-Identifier: GPL-2.0
/*
 * sec_mm/
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/memcontrol.h>
#include <linux/swap.h>
#include <linux/oom.h>
#include "sec_mm.h"

void mm_debug_show_free_areas(void)
{
	printk("mm_debug(KB) totalram:%lu"
		" active_anon:%lu inactive_anon:%lu isolated_anon:%lu"
		" active_file:%lu inactive_file:%lu isolated_file:%lu"
		" unevictable:%lu dirty:%lu writeback:%lu unstable:%lu"
		" slab_reclaimable:%lu slab_unreclaimable:%lu"
		" mapped:%lu shmem:%lu pagetables:%lu bounce:%lu"
		" free:%lu free_cma:%lu"
#if IS_ENABLED(CONFIG_ZSMALLOC)
		" zspages:%lu"
#endif
		" swapfree:%lu\n",
		K(totalram_pages),
		K(global_node_page_state(NR_ACTIVE_ANON)),
		K(global_node_page_state(NR_INACTIVE_ANON)),
		K(global_node_page_state(NR_ISOLATED_ANON)),
		K(global_node_page_state(NR_ACTIVE_FILE)),
		K(global_node_page_state(NR_INACTIVE_FILE)),
		K(global_node_page_state(NR_ISOLATED_FILE)),
		K(global_node_page_state(NR_UNEVICTABLE)),
		K(global_node_page_state(NR_FILE_DIRTY)),
		K(global_node_page_state(NR_WRITEBACK)),
		K(global_node_page_state(NR_UNSTABLE_NFS)),
		K(global_node_page_state(NR_SLAB_RECLAIMABLE)),
		K(global_node_page_state(NR_SLAB_UNRECLAIMABLE)),
		K(global_node_page_state(NR_FILE_MAPPED)),
		K(global_node_page_state(NR_SHMEM)),
		K(global_zone_page_state(NR_PAGETABLE)),
		K(global_zone_page_state(NR_BOUNCE)),
		K(global_zone_page_state(NR_FREE_PAGES)),
		K(global_zone_page_state(NR_FREE_CMA_PAGES)),
#if IS_ENABLED(CONFIG_ZSMALLOC)
		K(global_zone_page_state(NR_ZSPAGES)),
#endif
		K(get_nr_swap_pages()));
}

/* return true if the task is not adequate as candidate victim task. */
static bool oom_unkillable_task(struct task_struct *p)
{
	if (is_global_init(p))
		return true;
	if (p->flags & PF_KTHREAD)
		return true;
	return false;
}

/*
 * The process p may have detached its own ->mm while exiting or through
 * use_mm(), but one or more of its subthreads may still have a valid
 * pointer.  Return p, or any of its subthreads with a valid ->mm, with
 * task_lock() held.
 */
static struct task_struct *mm_debug_find_lock_task_mm(struct task_struct *p)
{
	struct task_struct *t;

	rcu_read_lock();

	for_each_thread(p, t) {
		task_lock(t);
		if (likely(t->mm))
			goto found;
		task_unlock(t);
	}
	t = NULL;
found:
	rcu_read_unlock();

	return t;
}

void mm_debug_dump_tasks(void)
{
	struct task_struct *p;
	struct task_struct *task;
	unsigned long cur_rss_sum;
	unsigned long heaviest_rss_sum = 0;
	char heaviest_comm[TASK_COMM_LEN];
	pid_t heaviest_pid;

	pr_info("mm_debug dump tasks\n");
	pr_info("[  pid  ]   uid  tgid total_vm      rss pgtables_bytes swapents oom_score_adj name\n");
	rcu_read_lock();
	for_each_process(p) {
		if (oom_unkillable_task(p))
			continue;

		task = mm_debug_find_lock_task_mm(p);
		if (!task) {
			/*
			 * This is a kthread or all of p's threads have already
			 * detached their mm's.  There's no need to report
			 * them; they can't be oom killed anyway.
			 */
			continue;
		}

		pr_info("[%7d] %5d %5d %8lu %8lu %8ld %8lu         %5hd %s\n",
			task->pid, from_kuid(&init_user_ns, task_uid(task)),
			task->tgid, task->mm->total_vm, get_mm_rss(task->mm),
			mm_pgtables_bytes(task->mm),
			get_mm_counter(task->mm, MM_SWAPENTS),
			task->signal->oom_score_adj, task->comm);
		cur_rss_sum = get_mm_rss(task->mm) +
					get_mm_counter(task->mm, MM_SWAPENTS);
		if (cur_rss_sum > heaviest_rss_sum) {
			heaviest_rss_sum = cur_rss_sum;
			strncpy(heaviest_comm, task->comm, TASK_COMM_LEN);
			heaviest_pid = task->pid;
		}
		task_unlock(task);
	}
	rcu_read_unlock();
	if (heaviest_rss_sum)
		pr_info("heaviest_task_rss:%s(%d) size:%luKB, totalram_pages:%luKB\n",
			heaviest_comm, heaviest_pid, K(heaviest_rss_sum),
			K(totalram_pages));
}

MODULE_LICENSE("GPL");
