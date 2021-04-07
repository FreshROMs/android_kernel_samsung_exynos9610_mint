#ifdef CONFIG_FAST_TRACK

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <trace/events/sched.h>
#include <../kernel/sched/sched.h>
#include <cpu/ftt/ftt.h>

#define FTT_VRUNTIME_SPAN 20000000

struct ftt_stat fttstat;

#define entity_is_task(se)	(!se->my_q)

inline int is_ftt(struct sched_entity *se)
{
	return likely(entity_is_task(se)) && (se->ftt_mark || atomic64_read(&se->ftt_dyn_mark));
}

inline u64 ftt_vruntime(struct cfs_rq *cfs_rq)
{
	return cfs_rq->min_vruntime - FTT_VRUNTIME_SPAN;
}

inline int is_ftt_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	return se->vruntime == ftt_vruntime(cfs_rq);
}

inline void __ftt_init_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	u64 vruntime = cfs_rq->min_vruntime - FTT_VRUNTIME_SPAN;

	se->ftt_vrt_delta += se->vruntime - vruntime;
	se->vruntime = vruntime;
}

inline void ftt_init_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (is_ftt(se))
		__ftt_init_vruntime(cfs_rq, se);
}

inline void __ftt_normalize_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	se->vruntime += se->ftt_vrt_delta;
	se->ftt_vrt_delta = 0;
}

inline void ftt_normalize_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (is_ftt(se))
		__ftt_normalize_vruntime(cfs_rq, se);
}

void ftt_enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (is_ftt(se)) {
		__ftt_init_vruntime(cfs_rq, se);
		if (se->ftt_enqueue_time == 0) {
			se->ftt_enqueue_time = 1;
			cfs_rq->ftt_rqcnt++;
		}
	}
}

void ftt_dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (is_ftt(se)) {
		__ftt_normalize_vruntime(cfs_rq, se);
		if (se->ftt_enqueue_time) {
			se->ftt_enqueue_time = 0;
			if (likely(cfs_rq->ftt_rqcnt > 0))
				cfs_rq->ftt_rqcnt--;
		}
	}
}

inline void init_task_ftt_info(struct task_struct *p)
{
	p->se.ftt_mark = 0;
	atomic64_set(&p->se.ftt_dyn_mark, 0);
	p->se.ftt_vrt_delta = 0;
	p->se.ftt_enqueue_time = 0;
}

void ftt_mark(struct task_struct *task)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	unsigned long flags;
#else
	struct rq_flags flags;
#endif
	struct rq *rq = NULL;

	rq = task_rq_lock(task, &flags);
	if (task->se.ftt_mark) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	task->se.ftt_mark = 1;
	ftt_set_vruntime(task, 1);

	task_rq_unlock(rq, task, &flags);
}

void ftt_unmark(struct task_struct *task)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	unsigned long flags;
#else
	struct rq_flags flags;
#endif
	struct rq *rq = NULL;

	rq = task_rq_lock(task, &flags);
	if (task->se.ftt_mark == 0) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	task->se.ftt_mark = 0;
	ftt_set_vruntime(task, 0);

	task_rq_unlock(rq, task, &flags);
}

/* Dynamic ftt Mechanism */
#define DYNAMIC_FTT_SEC_WIDTH   8

#define dynamic_ftt_offset_of(type) (type * DYNAMIC_FTT_SEC_WIDTH)
#define dynamic_ftt_one(type) ((u64)1 << dynamic_ftt_offset_of(type))

inline int is_dyn_ftt(struct sched_entity *se, int type)
{
	return (atomic64_read(&se->ftt_dyn_mark) & dynamic_ftt_one(type)) != 0;
}

static inline void dynamic_ftt_inc(struct task_struct *task, int type)
{
	atomic64_add(dynamic_ftt_one(type), &task->se.ftt_dyn_mark);
}

static inline void dynamic_ftt_dec(struct task_struct *task, int type)
{
	atomic64_sub(dynamic_ftt_one(type), &task->se.ftt_dyn_mark);
}

extern const struct sched_class fair_sched_class;
static int __dynamic_ftt_enqueue(struct task_struct *task, int type)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	unsigned long flags;
#else
	struct rq_flags flags;
#endif
	struct rq *rq = NULL;

	rq = task_rq_lock(task, &flags);
	/* TODO: Should check when set elsewhere */
	if (task->sched_class != &fair_sched_class) {
		task_rq_unlock(rq, task, &flags);
		return -1;
	}

	/* TODO: Should allow when this type of ftt unset */
	if (unlikely(is_ftt(&task->se))) {
		task_rq_unlock(rq, task, &flags);
		return 0;
	}
	dynamic_ftt_inc(task, type);
	ftt_set_vruntime(task, 1);

	task_rq_unlock(rq, task, &flags);
	return 1;
}

int dynamic_ftt_enqueue(struct task_struct *task, int type)
{
	if (likely(task))
		return __dynamic_ftt_enqueue(task, type);
	return -1;
}

static int __dynamic_ftt_dequeue(struct task_struct *task, int type)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	unsigned long flags;
#else
	struct rq_flags flags;
#endif
	struct rq *rq = NULL;
	u64 dynamic_ftt = 0;

	rq = task_rq_lock(task, &flags);
	dynamic_ftt = atomic64_read(&task->se.ftt_dyn_mark);
	if ((dynamic_ftt & dynamic_ftt_one(type)) == 0) {
		task_rq_unlock(rq, task, &flags);
		return -1;
	}
	dynamic_ftt_dec(task, type);
	ftt_set_vruntime(task, 0);

	task_rq_unlock(rq, task, &flags);
	return 1;
}

int dynamic_ftt_dequeue(struct task_struct *task, int type)
{
	if (likely(task))
		return __dynamic_ftt_dequeue(task, type);
	return -1;
}
#endif
