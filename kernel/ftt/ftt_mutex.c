#ifdef CONFIG_FAST_TRACK
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <cpu/ftt/ftt.h>

static void mutex_list_add_ftt(struct list_head *entry, struct list_head *head)
{
	struct list_head *pos = NULL;
	struct mutex_waiter *waiter = NULL;

	list_for_each(pos, head) {
		waiter = list_entry(pos, struct mutex_waiter, list);
		if (!is_ftt(&waiter->task->se)) {
			list_add(entry, waiter->list.prev);
			return;
		}
	}
	if (pos == head) {
		list_add_tail(entry, head);
	}
}

void mutex_list_add(struct task_struct *task, struct list_head *entry, struct list_head *head, struct mutex *lock)
{
	if (unlikely(task == NULL))
		return;

	if (is_ftt(&task->se)) {
		mutex_list_add_ftt(entry, head);
	} else {
		list_add_tail(entry, head);
	}
}

void mutex_dynamic_ftt_enqueue(struct mutex *lock, struct task_struct *task)
{
	struct task_struct *owner = NULL;

	if (unlikely(task == NULL))
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	owner = __mutex_owner(lock);
#else
	owner = lock->owner;
#endif
	if (is_ftt(&task->se) && !lock->ftt_dep_task && owner && !is_ftt(&owner->se)) {
		dynamic_ftt_enqueue(owner, DYNAMIC_FTT_MUTEX);
		lock->ftt_dep_task = owner;
	}
}

void mutex_dynamic_ftt_dequeue(struct mutex *lock, struct task_struct *task)
{
	if (lock->ftt_dep_task == task) {
		dynamic_ftt_dequeue(task, DYNAMIC_FTT_MUTEX);
		lock->ftt_dep_task = NULL;
	}
}

#endif
