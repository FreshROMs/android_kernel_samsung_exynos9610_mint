#ifndef _FTT_RWSEM_H_
#define _FTT_RWSEM_H_
extern void rwsem_list_add(struct task_struct *tsk, struct list_head *entry, struct list_head *head);
extern void rwsem_dynamic_ftt_enqueue(struct task_struct *tsk, struct task_struct *waiter_task, struct task_struct *owner, struct rw_semaphore *sem);
extern void rwsem_dynamic_ftt_dequeue(struct rw_semaphore *sem, struct task_struct *tsk);
#endif
