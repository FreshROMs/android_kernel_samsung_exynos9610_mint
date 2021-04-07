#ifndef _FTT_H_
#define _FTT_H_

#define FTT_MAX_SCHED 5
enum DYNAMIC_FTT_TYPE
{
        DYNAMIC_FTT_BINDER = 0,
        DYNAMIC_FTT_RWSEM,
        DYNAMIC_FTT_MUTEX,
        DYNAMIC_FTT_MAX,
};

struct rq;
struct cfs_rq;
struct sched_entity;
struct task_struct;

struct ftt_stat {
	int ftt_cnt;
	int pick_ftt;
	int wrong;
	int dyn_cnt;
};
extern struct ftt_stat fttstat;
extern int is_ftt(struct sched_entity *se);
extern u64 ftt_vruntime(struct cfs_rq *cfs_rq);
extern int is_ftt_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void __ftt_init_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void ftt_init_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void __ftt_normalize_vruntime(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void init_task_ftt_info(struct task_struct *p);
extern void ftt_enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void ftt_dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void ftt_mark(struct task_struct *task);
extern void ftt_unmark(struct task_struct *task);
extern int is_dyn_ftt(struct sched_entity *se, int type);
extern int dynamic_ftt_dequeue(struct task_struct *task, int type);
extern int dynamic_ftt_enqueue(struct task_struct *task, int type);
extern void ftt_set_vruntime(struct task_struct *task, int set);
#endif
