#ifdef CONFIG_FAST_TRACK
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <cpu/ftt/ftt.h>

enum rwsem_waiter_type {
	RWSEM_WAITING_FOR_WRITE,
	RWSEM_WAITING_FOR_READ
};

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	enum rwsem_waiter_type type;
};

#define RWSEM_READER_OWNED	((struct task_struct *)1UL)

static inline bool rwsem_owner_is_writer(struct task_struct *owner)
{
	return owner && owner != RWSEM_READER_OWNED;
}

static void rwsem_list_add_ftt(struct list_head *entry, struct list_head *head)
{
	struct list_head *pos = NULL;
	struct rwsem_waiter *waiter = NULL;

	list_for_each(pos, head) {
		waiter = list_entry(pos, struct rwsem_waiter, list);
		if (!is_ftt(&waiter->task->se)) {
			list_add(entry, waiter->list.prev);
			return;
		}
	}
	if (pos == head) {
		list_add_tail(entry, head);
	}
}

void rwsem_list_add(struct task_struct *tsk, struct list_head *entry, struct list_head *head)
{
	if (unlikely(tsk == NULL))
		return;

	if (is_ftt(&tsk->se)) {
		rwsem_list_add_ftt(entry, head);
	} else {
		list_add_tail(entry, head);
	}
}

void rwsem_dynamic_ftt_enqueue(struct task_struct *tsk, struct task_struct *waiter_task, struct task_struct *owner, struct rw_semaphore *sem)
{
	if (unlikely(tsk == NULL) || unlikely(waiter_task == NULL || owner == NULL))
		return;
	if (is_ftt(&tsk->se)) {
		if (rwsem_owner_is_writer(owner) && !is_ftt(&owner->se) && sem && !sem->ftt_dep_task) {
			dynamic_ftt_enqueue(owner, DYNAMIC_FTT_RWSEM);
			sem->ftt_dep_task = owner;
		}
	}
}

void rwsem_dynamic_ftt_dequeue(struct rw_semaphore *sem, struct task_struct *tsk)
{
	if (tsk && sem && sem->ftt_dep_task == tsk) {
		dynamic_ftt_dequeue(tsk, DYNAMIC_FTT_RWSEM);
		sem->ftt_dep_task = NULL;
	}
}

#endif

